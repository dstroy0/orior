// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "unit_sweep.h"
#include "../../runtime/device_pool/device_pool.h"

#include <cuda_runtime.h>

#include <string.h>

#define UNIT_SWEEP_THREADS 256u

#define UNIT_SWEEP_SHARED_BYTES_MAX 49152u

#define UNIT_SWEEP_INPUT_BITS 16u

static_assert((sizeof(unsigned short) * 8u) == UNIT_SWEEP_INPUT_BITS,
              "unit_sweep: UNIT_SWEEP_INPUT_BITS must equal the bits of an unsigned short input voxel");

static_assert(cudaSuccess == 0, "the engine reads a CUDA status of 0 as success");

// cudaError_t enumerates non-negative codes below INT_MAX: the status converts to int exactly
#define UNIT_SWEEP_STATUS_CHECK(call_, evacaddr_, error_)                                                              \
    engine_status_check((int)(call_), ENGINE_MODULE_UNIT_SWEEP, (unsigned int)__LINE__, (const void *)(evacaddr_),     \
                        (error_))

#define UNIT_SWEEP_CHECK(condition_, evacaddr_, error_, kind_)                                                         \
    engine_error_check((condition_), (kind_), ENGINE_MODULE_UNIT_SWEEP, (unsigned int)__LINE__,                        \
                       (const void *)(evacaddr_), (error_))

struct UnitSweepExtent
{
    unsigned int extent[ENGINE_AXES];
    unsigned long long voxels;
};

struct UnitSweepResident
{
    unsigned int *narrow_planes;
    unsigned int *wide_planes;
    unsigned long long *disagreements;
    unsigned int *over_width;
    size_t narrow_words;
    size_t wide_words;
};

static UnitSweepResident s_unit_sweep_resident;

__global__ static void unit_sweep_load_kernel(const unsigned short *volume, unsigned long long voxels,
                                              unsigned int limbs, unsigned int *planes)
{
    const unsigned long long voxel = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (voxel >= voxels)
    {
        return;
    }
    planes[voxel] = (unsigned int)volume[voxel];
    for (unsigned int limb = 1u; limb < limbs; limb += 1u)
    {
        planes[((unsigned long long)limb * voxels) + voxel] = 0u;
    }
}

__global__ static void unit_sweep_planes_load_kernel(const unsigned int *input, unsigned int input_limbs,
                                                     unsigned int top_mask, unsigned long long voxels,
                                                     unsigned int limbs, unsigned int *planes, unsigned int *over_width)
{
    const unsigned long long voxel = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (voxel >= voxels)
    {
        return;
    }
    for (unsigned int limb = 0u; limb < limbs; limb += 1u)
    {
        planes[((unsigned long long)limb * voxels) + voxel] =
            (limb < input_limbs) ? input[((unsigned long long)limb * voxels) + voxel] : 0u;
    }
    if ((input[((unsigned long long)(input_limbs - 1u) * voxels) + voxel] & top_mask) != 0u)
    {
        atomicOr(over_width, 1u);
    }
}

__global__ static void unit_sweep_widen_kernel(const unsigned int *narrow_planes, unsigned int narrow_limbs,
                                               unsigned long long voxels, unsigned int wide_limbs,
                                               unsigned int *wide_planes)
{
    const unsigned long long voxel = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (voxel >= voxels)
    {
        return;
    }
    for (unsigned int limb = 0u; limb < wide_limbs; limb += 1u)
    {
        wide_planes[((unsigned long long)limb * voxels) + voxel] =
            (limb < narrow_limbs) ? narrow_planes[((unsigned long long)limb * voxels) + voxel] : 0u;
    }
}

// `steps` unit pairs [1, 2, 1] along the axis, each reading the voxel's neighbors with the edge voxel standing in past
// either end, then with `half` set the one unit step [1, 1] an odd order leaves, reading the voxel and the one before
// it with the first voxel standing in before the start. That is the key's window reflected at the edges: a pair keeps
// the line's reflection about the half voxel before its start, and the step after all pairs starts where the key's
// window of the whole order starts, floor((order + 1) / 2) before the voxel.
__global__ static void unit_sweep_axis_kernel(unsigned int *planes, UnitSweepExtent extent, unsigned int axis,
                                              unsigned int limbs, unsigned int steps, unsigned int half,
                                              unsigned int bits)
{
    extern __shared__ unsigned int shared_line[];
    const unsigned int length = extent.extent[axis];
    const unsigned long long line = blockIdx.x;
    const unsigned long long plane = (unsigned long long)extent.extent[1] * extent.extent[2];
    const unsigned long long stride =
        (axis == 0u) ? plane : ((axis == 1u) ? (unsigned long long)extent.extent[2] : 1ull);
    const unsigned long long base =
        (axis == 0u) ? line
                     : ((axis == 1u) ? (((line / extent.extent[2]) * plane) + (line % extent.extent[2]))
                                     : (line * extent.extent[2]));
    unsigned int *front = shared_line;
    unsigned int *back = &shared_line[(size_t)length * limbs];
    const unsigned int input_limbs = (bits + 31u) / 32u;
    for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
    {
        for (unsigned int limb = 0u; limb < limbs; limb += 1u)
        {
            front[(limb * length) + place] =
                (limb < input_limbs) ? planes[((unsigned long long)limb * extent.voxels) + base + (place * stride)]
                                     : 0u;
            back[(limb * length) + place] = 0u;
        }
    }
    __syncthreads();
    for (unsigned int step = 0u; step < steps; step += 1u)
    {
        const unsigned int grown_limbs = (bits + (2u * (step + 1u)) + 31u) / 32u;
        const unsigned int live_limbs = (grown_limbs < limbs) ? grown_limbs : limbs;
        for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
        {
            const unsigned int left = (place == 0u) ? place : (place - 1u);
            const unsigned int right = ((place + 1u) == length) ? place : (place + 1u);
            unsigned long long carry = 0ull;
            for (unsigned int limb = 0u; limb < live_limbs; limb += 1u)
            {
                const unsigned int *const row = &front[limb * length];
                const unsigned long long total = (unsigned long long)row[left] +
                                                 (2ull * (unsigned long long)row[place]) +
                                                 (unsigned long long)row[right] + carry;
                back[(limb * length) + place] = (unsigned int)(total & 0xFFFFFFFFull);
                carry = total >> 32u;
            }
        }
        __syncthreads();
        unsigned int *const former_front = front;
        front = back;
        back = former_front;
    }
    if (half != 0u)
    {
        const unsigned int grown_limbs = (bits + (2u * steps) + 1u + 31u) / 32u;
        const unsigned int live_limbs = (grown_limbs < limbs) ? grown_limbs : limbs;
        for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
        {
            const unsigned int before = (place == 0u) ? place : (place - 1u);
            unsigned long long carry = 0ull;
            for (unsigned int limb = 0u; limb < live_limbs; limb += 1u)
            {
                const unsigned int *const row = &front[limb * length];
                const unsigned long long total =
                    (unsigned long long)row[before] + (unsigned long long)row[place] + carry;
                back[(limb * length) + place] = (unsigned int)(total & 0xFFFFFFFFull);
                carry = total >> 32u;
            }
        }
        __syncthreads();
        unsigned int *const former_front = front;
        front = back;
        back = former_front;
    }
    const unsigned int final_limbs = (bits + (2u * steps) + half + 31u) / 32u;
    const unsigned int stored_limbs = (final_limbs < limbs) ? final_limbs : limbs;
    for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
    {
        for (unsigned int limb = 0u; limb < stored_limbs; limb += 1u)
        {
            planes[((unsigned long long)limb * extent.voxels) + base + (place * stride)] =
                front[(limb * length) + place];
        }
    }
}

// a position along a line of `length`, reflected at its edges about the half voxel past each end, as the key's fold
// table reflects it
__device__ static unsigned int unit_sweep_reflect(long long position, long long length)
{
    const long long period = 2ll * length;
    long long folded = position % period;
    folded += (folded < 0ll) ? period : 0ll;
    folded = (folded >= length) ? (period - 1ll - folded) : folded;
    // folded lies in [0, length), and length is an extent of 32 bits
    return (unsigned int)folded;
}

// the comb's centered part along the axis: `taps` ones, `spacing` voxels apart, centered on the voxel, each reading the
// line reflected at its edges. An odd comb of n is n ones a voxel apart; an even comb of n is n / 2 ones two voxels
// apart, and the one unit step [1, 1] it leaves runs with the halves. `growth` is the bits the sum adds.
__global__ static void unit_sweep_comb_kernel(unsigned int *planes, UnitSweepExtent extent, unsigned int axis,
                                              unsigned int limbs, unsigned int taps, unsigned int spacing,
                                              unsigned int bits, unsigned int growth)
{
    extern __shared__ unsigned int shared_line[];
    const unsigned int length = extent.extent[axis];
    const unsigned long long line = blockIdx.x;
    const unsigned long long plane = (unsigned long long)extent.extent[1] * extent.extent[2];
    const unsigned long long stride =
        (axis == 0u) ? plane : ((axis == 1u) ? (unsigned long long)extent.extent[2] : 1ull);
    const unsigned long long base =
        (axis == 0u) ? line
                     : ((axis == 1u) ? (((line / extent.extent[2]) * plane) + (line % extent.extent[2]))
                                     : (line * extent.extent[2]));
    unsigned int *const front = shared_line;
    unsigned int *const back = &shared_line[(size_t)length * limbs];
    const unsigned int input_limbs = (bits + 31u) / 32u;
    for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
    {
        for (unsigned int limb = 0u; limb < limbs; limb += 1u)
        {
            front[(limb * length) + place] =
                (limb < input_limbs) ? planes[((unsigned long long)limb * extent.voxels) + base + (place * stride)]
                                     : 0u;
        }
    }
    __syncthreads();
    const unsigned int grown_limbs = (bits + growth + 31u) / 32u;
    const unsigned int live_limbs = (grown_limbs < limbs) ? grown_limbs : limbs;
    // the first tap sits (taps - 1) * spacing / 2 before the voxel, which is whole: an even comb's taps are spaced 2
    const long long first = -(long long)(((taps - 1u) * spacing) / 2u);
    for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
    {
        unsigned long long carry = 0ull;
        for (unsigned int limb = 0u; limb < live_limbs; limb += 1u)
        {
            const unsigned int *const row = &front[limb * length];
            unsigned long long total = carry;
            for (unsigned int tap = 0u; tap < taps; tap += 1u)
            {
                const long long position = (long long)place + first + ((long long)tap * (long long)spacing);
                total += (unsigned long long)row[unit_sweep_reflect(position, (long long)length)];
            }
            back[(limb * length) + place] = (unsigned int)(total & 0xFFFFFFFFull);
            carry = total >> 32u;
        }
        for (unsigned int limb = live_limbs; limb < limbs; limb += 1u)
        {
            back[(limb * length) + place] = 0u;
        }
    }
    __syncthreads();
    for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
    {
        for (unsigned int limb = 0u; limb < live_limbs; limb += 1u)
        {
            planes[((unsigned long long)limb * extent.voxels) + base + (place * stride)] = back[(limb * length) + place];
        }
    }
}

// `pairs` pairs [1, 2, 1] along the axis whose taps are `spacing` voxels apart, each reading the line reflected at its
// edges. A pair is symmetric about the voxel and keeps the line's reflection about the half voxel past each end,
// and the pairs, the unit pairs and the comb's centered part may run in any order and still give the key's window.
__global__ static void unit_sweep_spaced_kernel(unsigned int *planes, UnitSweepExtent extent, unsigned int axis,
                                                unsigned int limbs, unsigned int pairs, unsigned int spacing,
                                                unsigned int bits)
{
    extern __shared__ unsigned int shared_line[];
    const unsigned int length = extent.extent[axis];
    const unsigned long long line = blockIdx.x;
    const unsigned long long plane = (unsigned long long)extent.extent[1] * extent.extent[2];
    const unsigned long long stride =
        (axis == 0u) ? plane : ((axis == 1u) ? (unsigned long long)extent.extent[2] : 1ull);
    const unsigned long long base =
        (axis == 0u) ? line
                     : ((axis == 1u) ? (((line / extent.extent[2]) * plane) + (line % extent.extent[2]))
                                     : (line * extent.extent[2]));
    unsigned int *front = shared_line;
    unsigned int *back = &shared_line[(size_t)length * limbs];
    const unsigned int input_limbs = (bits + 31u) / 32u;
    for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
    {
        for (unsigned int limb = 0u; limb < limbs; limb += 1u)
        {
            front[(limb * length) + place] =
                (limb < input_limbs) ? planes[((unsigned long long)limb * extent.voxels) + base + (place * stride)]
                                     : 0u;
            back[(limb * length) + place] = 0u;
        }
    }
    __syncthreads();
    for (unsigned int pair = 0u; pair < pairs; pair += 1u)
    {
        const unsigned int grown_limbs = (bits + (2u * (pair + 1u)) + 31u) / 32u;
        const unsigned int live_limbs = (grown_limbs < limbs) ? grown_limbs : limbs;
        for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
        {
            const unsigned int left = unit_sweep_reflect((long long)place - (long long)spacing, (long long)length);
            const unsigned int right = unit_sweep_reflect((long long)place + (long long)spacing, (long long)length);
            unsigned long long carry = 0ull;
            for (unsigned int limb = 0u; limb < live_limbs; limb += 1u)
            {
                const unsigned int *const row = &front[limb * length];
                const unsigned long long total = (unsigned long long)row[left] +
                                                 (2ull * (unsigned long long)row[place]) +
                                                 (unsigned long long)row[right] + carry;
                back[(limb * length) + place] = (unsigned int)(total & 0xFFFFFFFFull);
                carry = total >> 32u;
            }
        }
        __syncthreads();
        unsigned int *const former_front = front;
        front = back;
        back = former_front;
    }
    const unsigned int final_limbs = (bits + (2u * pairs) + 31u) / 32u;
    const unsigned int stored_limbs = (final_limbs < limbs) ? final_limbs : limbs;
    for (unsigned int place = threadIdx.x; place < length; place += blockDim.x)
    {
        for (unsigned int limb = 0u; limb < stored_limbs; limb += 1u)
        {
            planes[((unsigned long long)limb * extent.voxels) + base + (place * stride)] =
                front[(limb * length) + place];
        }
    }
}

__global__ static void unit_sweep_residual_kernel(const unsigned int *narrow_planes, unsigned int narrow_limbs,
                                                  const unsigned int *wide_planes, unsigned int wide_limbs,
                                                  unsigned long long voxels, unsigned int gain, unsigned int limbs,
                                                  unsigned int *residual_lanes)
{
    const unsigned long long voxel = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (voxel >= voxels)
    {
        return;
    }
    const unsigned int gain_limbs = gain / 32u;
    const unsigned int gain_bits = gain % 32u;
    unsigned long long borrow = 0ull;
    for (unsigned int limb = 0u; limb < limbs; limb += 1u)
    {
        unsigned int shifted_limb = 0u;
        if (limb >= gain_limbs)
        {
            const unsigned int source_limb = limb - gain_limbs;
            shifted_limb = (source_limb < narrow_limbs)
                               ? (narrow_planes[((unsigned long long)source_limb * voxels) + voxel] << gain_bits)
                               : 0u;
            if ((gain_bits != 0u) && (source_limb > 0u) && ((source_limb - 1u) < narrow_limbs))
            {
                shifted_limb |=
                    narrow_planes[((unsigned long long)(source_limb - 1u) * voxels) + voxel] >> (32u - gain_bits);
            }
        }
        const unsigned int wide_limb =
            (limb < wide_limbs) ? wide_planes[((unsigned long long)limb * voxels) + voxel] : 0u;
        const unsigned long long difference =
            (1ull << 32u) + (unsigned long long)shifted_limb - (unsigned long long)wide_limb - borrow;
        residual_lanes[(voxel * limbs) + limb] = (unsigned int)(difference & 0xFFFFFFFFull);
        borrow = (difference < (1ull << 32u)) ? 1ull : 0ull;
    }
}

__global__ static void unit_sweep_compare_kernel(const unsigned int *left, const unsigned int *right,
                                                 unsigned long long lanes, unsigned int limbs,
                                                 unsigned long long *disagreements)
{
    const unsigned long long lane = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (lane >= lanes)
    {
        return;
    }
    unsigned int difference_bits = 0u;
    for (unsigned int limb = 0u; limb < limbs; limb += 1u)
    {
        difference_bits |= left[(lane * limbs) + limb] ^ right[(lane * limbs) + limb];
    }
    if (difference_bits != 0u)
    {
        atomicAdd(disagreements, 1ull);
    }
}

static int unit_sweep_grow(unsigned int **planes, size_t *capacity, size_t words, EngineError *error)
{
    if (words <= *capacity)
    {
        return 1;
    }
    cudaFree(*planes);
    *planes = NULL;
    *capacity = 0u;
    const int ok = UNIT_SWEEP_STATUS_CHECK(cudaMalloc((void **)planes, words * sizeof(unsigned int)), planes, error);
    *capacity = (ok != 0) ? words : 0u;
    return ok;
}

// a bit length: the bits that hold `value`
static unsigned int unit_sweep_bit_length(unsigned int value)
{
    unsigned int bits = 0u;
    while (value != 0u)
    {
        bits += 1u;
        value >>= 1u;
    }
    return bits;
}

// the bits a comb of `length` adds along its axis: the sum of n ones is n times the widest value, bit_length(n - 1)
// bits wider. 0 and 1 add none.
static unsigned int unit_sweep_comb_growth(unsigned int length)
{
    return (length < 2u) ? 0u : unit_sweep_bit_length(length - 1u);
}

// each axis's comb, its centered part: n ones a voxel apart for an odd n, n / 2 ones two voxels apart for an even n,
// whose one unit step [1, 1] runs with the halves. `bits` bounds the planes' width going in.
static int unit_sweep_combs(unsigned int *planes, const UnitSweepExtent *extent, unsigned int limbs,
                            const unsigned int comb[ENGINE_AXES], unsigned int bits, EngineError *error)
{
    int ok = 1;
    for (unsigned int axis = 0u; (ok != 0) && (axis < ENGINE_AXES); axis += 1u)
    {
        const unsigned int length = comb[axis];
        const unsigned int odd = length & 1u;
        const unsigned int taps = (length < 2u) ? 1u : ((odd != 0u) ? length : (length / 2u));
        const unsigned int spacing = (odd != 0u) ? 1u : 2u;
        const unsigned int growth = unit_sweep_bit_length(taps - 1u);
        const unsigned int input_bits = bits;
        bits += growth;
        if (taps < 2u)
        {
            continue;
        }
        const unsigned long long lines = extent->voxels / extent->extent[axis];
        const size_t shared_bytes = 2u * (size_t)extent->extent[axis] * limbs * sizeof(unsigned int);
        ok = UNIT_SWEEP_CHECK(lines <= 0x7FFFFFFFull, &extent->extent[axis], error, ENGINE_ERROR_REQUEST) &&
             UNIT_SWEEP_CHECK(shared_bytes <= UNIT_SWEEP_SHARED_BYTES_MAX, &extent->extent[axis], error,
                              ENGINE_ERROR_REQUEST);
        if (ok != 0)
        {
            // lines was held at or below 2^31 - 1 above: it narrows to the unsigned int grid size exactly
            unit_sweep_comb_kernel<<<(unsigned int)lines, UNIT_SWEEP_THREADS, shared_bytes>>>(
                planes, *extent, axis, limbs, taps, spacing, input_bits, growth);
            ok = UNIT_SWEEP_STATUS_CHECK(cudaGetLastError(), planes, error);
        }
    }
    return ok;
}

// each axis's unit pairs [1, 2, 1], steps[axis] of them; or with `halves` set, only the one unit step [1, 1] where
// halves[axis] is 1, which runs after every pair either term takes so that the pairs see the line's reflection.
// `bits` bounds the planes' width going in: the limbs a step reads and writes follow from it, and a bound above the
// width only carries limbs of zeros.
static int unit_sweep_axes(unsigned int *planes, const UnitSweepExtent *extent, unsigned int limbs,
                           const unsigned int pairs[ENGINE_AXES], unsigned int bits, int halves, EngineError *error)
{
    int ok = 1;
    for (unsigned int axis = 0u; (ok != 0) && (axis < ENGINE_AXES); axis += 1u)
    {
        const unsigned int steps = (halves != 0) ? 0u : pairs[axis];
        const unsigned int half = (halves != 0) ? pairs[axis] : 0u;
        const unsigned int input_bits = bits;
        bits += (2u * steps) + half;
        if ((steps == 0u) && (half == 0u))
        {
            continue;
        }
        const unsigned long long lines = extent->voxels / extent->extent[axis];
        const size_t shared_bytes = 2u * (size_t)extent->extent[axis] * limbs * sizeof(unsigned int);
        ok = UNIT_SWEEP_CHECK(lines <= 0x7FFFFFFFull, &extent->extent[axis], error, ENGINE_ERROR_REQUEST) &&
             UNIT_SWEEP_CHECK(shared_bytes <= UNIT_SWEEP_SHARED_BYTES_MAX, &extent->extent[axis], error,
                              ENGINE_ERROR_REQUEST);
        if (ok != 0)
        {
            // lines was held at or below 2^31 - 1 above: it narrows to the unsigned int grid size exactly
            unit_sweep_axis_kernel<<<(unsigned int)lines, UNIT_SWEEP_THREADS, shared_bytes>>>(
                planes, *extent, axis, limbs, steps, half, input_bits);
            ok = UNIT_SWEEP_STATUS_CHECK(cudaGetLastError(), planes, error);
        }
    }
    return ok;
}

// each axis's spaced pairs, spaced[axis][j] of them with taps 2^j apart, the spacings in ascending order. `bits`
// bounds the planes' width going in.
static int unit_sweep_spaced(unsigned int *planes, const UnitSweepExtent *extent, unsigned int limbs,
                             const unsigned int spaced[ENGINE_AXES][ENGINE_SPACINGS], unsigned int bits,
                             EngineError *error)
{
    int ok = 1;
    for (unsigned int axis = 0u; (ok != 0) && (axis < ENGINE_AXES); axis += 1u)
    {
        for (unsigned int spacing = 0u; (ok != 0) && (spacing < ENGINE_SPACINGS); spacing += 1u)
        {
            const unsigned int pairs = spaced[axis][spacing];
            const unsigned int input_bits = bits;
            bits += 2u * pairs;
            if (pairs == 0u)
            {
                continue;
            }
            const unsigned long long lines = extent->voxels / extent->extent[axis];
            const size_t shared_bytes = 2u * (size_t)extent->extent[axis] * limbs * sizeof(unsigned int);
            ok = UNIT_SWEEP_CHECK(lines <= 0x7FFFFFFFull, &extent->extent[axis], error, ENGINE_ERROR_REQUEST) &&
                 UNIT_SWEEP_CHECK(shared_bytes <= UNIT_SWEEP_SHARED_BYTES_MAX, &extent->extent[axis], error,
                                  ENGINE_ERROR_REQUEST);
            if (ok != 0)
            {
                // lines was held at or below 2^31 - 1 above: it narrows to the unsigned int grid size exactly
                unit_sweep_spaced_kernel<<<(unsigned int)lines, UNIT_SWEEP_THREADS, shared_bytes>>>(
                    planes, *extent, axis, limbs, pairs, 1u << spacing, input_bits);
                ok = UNIT_SWEEP_STATUS_CHECK(cudaGetLastError(), planes, error);
            }
        }
    }
    return ok;
}

static int unit_sweep_planes_load(const UnitSweepRequest *request, const UnitSweepExtent *extent,
                                  unsigned int narrow_limbs, unsigned int blocks, UnitSweepResident *buffers,
                                  EngineError *error)
{
    const unsigned int input_limbs = (request->input_bits + 31u) / 32u;
    const unsigned int top_bits = request->input_bits % 32u;
    const unsigned int top_mask = (top_bits == 0u) ? 0u : ~((1u << top_bits) - 1u);
    unsigned int over_width = 0u;
    int ok = (buffers->over_width != NULL) ||
             UNIT_SWEEP_STATUS_CHECK(cudaMalloc((void **)&buffers->over_width, sizeof(unsigned int)),
                                     &buffers->over_width, error);
    ok = ok &&
         UNIT_SWEEP_STATUS_CHECK(cudaMemset(buffers->over_width, 0, sizeof(unsigned int)), buffers->over_width, error);
    if (ok != 0)
    {
        unit_sweep_planes_load_kernel<<<blocks, UNIT_SWEEP_THREADS>>>(request->device_planes, input_limbs, top_mask,
                                                                      extent->voxels, narrow_limbs,
                                                                      buffers->narrow_planes, buffers->over_width);
        ok = UNIT_SWEEP_STATUS_CHECK(cudaGetLastError(), buffers->narrow_planes, error);
    }
    ok = ok && UNIT_SWEEP_STATUS_CHECK(
                   cudaMemcpy(&over_width, buffers->over_width, sizeof(unsigned int), cudaMemcpyDeviceToHost),
                   buffers->over_width, error);
    return ok && UNIT_SWEEP_CHECK(over_width == 0u, &request->input_bits, error, ENGINE_ERROR_REQUEST);
}

extern "C" unsigned long long unit_sweep_bits(const UnitSweepRequest *request)
{
    unsigned long long bits = ((request->device_planes != NULL) ? request->input_bits : UNIT_SWEEP_INPUT_BITS) + 1ull;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        bits += (unsigned long long)request->smooth_orders[axis] + request->background_orders[axis] +
                unit_sweep_comb_growth(request->comb[axis]);
        for (unsigned int spacing = 0u; spacing < ENGINE_SPACINGS; spacing += 1u)
        {
            bits += 2ull * ((unsigned long long)request->smooth_spaced[axis][spacing] +
                            request->background_spaced[axis][spacing]);
        }
    }
    return bits;
}

extern "C" unsigned long long unit_sweep_bytes(const UnitSweepRequest *request)
{
    const unsigned long long voxels = (unsigned long long)request->depth * request->height * request->width;
    unsigned long long narrow_bits = (request->device_planes != NULL) ? request->input_bits : UNIT_SWEEP_INPUT_BITS;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        narrow_bits += (unsigned long long)request->smooth_orders[axis] + unit_sweep_comb_growth(request->comb[axis]);
        for (unsigned int spacing = 0u; spacing < ENGINE_SPACINGS; spacing += 1u)
        {
            narrow_bits += 2ull * request->smooth_spaced[axis][spacing];
        }
    }
    const unsigned long long wide_bits = unit_sweep_bits(request) - 1ull;
    const unsigned long long narrow = voxels * ((narrow_bits + 31ull) / 32ull) * sizeof(unsigned int);
    const unsigned long long wide = voxels * ((wide_bits + 31ull) / 32ull) * sizeof(unsigned int);
    // each allocation is mapped in whole pages: the two planes, and a page for each counter
    const unsigned long long page = DEVICE_POOL_PAGE_BYTES - 1ull;
    return ((narrow + page) & ~page) + ((wide + page) & ~page) + (2ull * DEVICE_POOL_PAGE_BYTES);
}

extern "C" long unit_sweep_residual(const UnitSweepRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return UNIT_SWEEP_ERROR;
    }
    EngineError *const error = request->error;
    const int planes_given = (request->device_planes != NULL) ? 1 : 0;
    const int asked =
        UNIT_SWEEP_CHECK(((request->device_volume != NULL) != (planes_given != 0)) && (request->device_out != NULL),
                         request, error, ENGINE_ERROR_REQUEST) &&
        UNIT_SWEEP_CHECK((planes_given == 0) ||
                             ((request->input_bits != 0u) && (request->input_bits < (32u * request->limbs))),
                         &request->input_bits, error, ENGINE_ERROR_REQUEST) &&
        UNIT_SWEEP_CHECK((request->depth != 0u) && (request->height != 0u) && (request->width != 0u), &request->depth,
                         error, ENGINE_ERROR_REQUEST);
    if (asked == 0)
    {
        return UNIT_SWEEP_ERROR;
    }
    const unsigned int input_bits = (planes_given != 0) ? request->input_bits : UNIT_SWEEP_INPUT_BITS;
    // the spaced pairs' bits, 2 a pair: both terms take the smooth's, and the background's term its own as well
    unsigned long long smooth_spaced_bits = 0ull;
    unsigned long long background_spaced_bits = 0ull;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        for (unsigned int spacing = 0u; spacing < ENGINE_SPACINGS; spacing += 1u)
        {
            smooth_spaced_bits += 2ull * request->smooth_spaced[axis][spacing];
            background_spaced_bits += 2ull * request->background_spaced[axis][spacing];
        }
    }
    if (!UNIT_SWEEP_CHECK((smooth_spaced_bits + background_spaced_bits) <= (32ull * request->limbs),
                          request->smooth_spaced, error, ENGINE_ERROR_REQUEST))
    {
        return UNIT_SWEEP_ERROR;
    }
    // both are held at or below 32 limbs' bits above, and narrow to unsigned int exactly
    unsigned int narrow_bits = input_bits + (unsigned int)smooth_spaced_bits;
    unsigned int gain = (unsigned int)background_spaced_bits;
    // the narrow term's pairs: the smooth order's, and one more where an odd smooth order and an even comb each leave
    // a unit step [1, 1], which together are one pair [1, 2, 1]; the background's pairs; and the one unit step left
    // where the smooth order and the comb's n - 1 sum to an odd number
    unsigned int narrow_pairs[ENGINE_AXES];
    unsigned int background_pairs[ENGINE_AXES];
    unsigned int halves[ENGINE_AXES];
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        // an odd background order would subtract the terms half a voxel apart; an odd smooth order moves both alike
        if (!UNIT_SWEEP_CHECK((request->background_orders[axis] % 2u) == 0u, &request->background_orders[axis], error,
                              ENGINE_ERROR_REQUEST))
        {
            return UNIT_SWEEP_ERROR;
        }
        const unsigned int smooth_half = request->smooth_orders[axis] % 2u;
        const unsigned int comb_half = ((request->comb[axis] >= 2u) && ((request->comb[axis] % 2u) == 0u)) ? 1u : 0u;
        narrow_pairs[axis] = (request->smooth_orders[axis] / 2u) + (smooth_half & comb_half);
        background_pairs[axis] = request->background_orders[axis] / 2u;
        halves[axis] = smooth_half ^ comb_half;
        narrow_bits += request->smooth_orders[axis] + unit_sweep_comb_growth(request->comb[axis]);
        gain += request->background_orders[axis];
    }
    const unsigned int narrow_limbs = (narrow_bits + 31u) / 32u;
    const unsigned int wide_limbs = (narrow_bits + gain + 31u) / 32u;
    if (!UNIT_SWEEP_CHECK((narrow_bits + gain + 1u) <= (32u * request->limbs), &request->limbs, error,
                          ENGINE_ERROR_REQUEST))
    {
        return UNIT_SWEEP_ERROR;
    }
    const UnitSweepExtent extent = {{request->depth, request->height, request->width},
                                    (unsigned long long)request->depth * request->height * request->width};
    UnitSweepResident *const buffers = &s_unit_sweep_resident;
    // the caller holds the voxel count below 2^32: the block count fits unsigned int
    const unsigned int blocks = (unsigned int)((extent.voxels + UNIT_SWEEP_THREADS - 1u) / UNIT_SWEEP_THREADS);
    int ok =
        unit_sweep_grow(&buffers->narrow_planes, &buffers->narrow_words, (size_t)extent.voxels * narrow_limbs, error) &&
        unit_sweep_grow(&buffers->wide_planes, &buffers->wide_words, (size_t)extent.voxels * wide_limbs, error);
    if ((ok != 0) && (planes_given == 0))
    {
        unit_sweep_load_kernel<<<blocks, UNIT_SWEEP_THREADS>>>(request->device_volume, extent.voxels, narrow_limbs,
                                                               buffers->narrow_planes);
        ok = UNIT_SWEEP_STATUS_CHECK(cudaGetLastError(), buffers->narrow_planes, error);
    }
    ok = ok && ((planes_given == 0) || unit_sweep_planes_load(request, &extent, narrow_limbs, blocks, buffers, error));
    ok = ok && unit_sweep_axes(buffers->narrow_planes, &extent, narrow_limbs, narrow_pairs, input_bits, 0, error);
    unsigned int paired_bits = input_bits;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        paired_bits += 2u * narrow_pairs[axis];
    }
    ok = ok && unit_sweep_spaced(buffers->narrow_planes, &extent, narrow_limbs, request->smooth_spaced, paired_bits,
                                 error);
    // the smooth's spaced bits are held at or below 32 limbs' bits above, and narrow to unsigned int exactly
    paired_bits += (unsigned int)smooth_spaced_bits;
    ok = ok && unit_sweep_combs(buffers->narrow_planes, &extent, narrow_limbs, request->comb, paired_bits, error);
    if (ok != 0)
    {
        unit_sweep_widen_kernel<<<blocks, UNIT_SWEEP_THREADS>>>(buffers->narrow_planes, narrow_limbs, extent.voxels,
                                                                wide_limbs, buffers->wide_planes);
        ok = UNIT_SWEEP_STATUS_CHECK(cudaGetLastError(), buffers->wide_planes, error);
    }
    ok = ok && unit_sweep_axes(buffers->wide_planes, &extent, wide_limbs, background_pairs, narrow_bits, 0, error);
    unsigned int background_paired_bits = narrow_bits;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        background_paired_bits += 2u * background_pairs[axis];
    }
    ok = ok && unit_sweep_spaced(buffers->wide_planes, &extent, wide_limbs, request->background_spaced,
                                 background_paired_bits, error);
    // the odd unit steps last, on both terms alike, once every pair and comb has run
    ok = ok && unit_sweep_axes(buffers->narrow_planes, &extent, narrow_limbs, halves, narrow_bits, 1, error) &&
         unit_sweep_axes(buffers->wide_planes, &extent, wide_limbs, halves, narrow_bits + gain, 1, error);
    if (ok != 0)
    {
        unit_sweep_residual_kernel<<<blocks, UNIT_SWEEP_THREADS>>>(buffers->narrow_planes, narrow_limbs,
                                                                   buffers->wide_planes, wide_limbs, extent.voxels,
                                                                   gain, request->limbs, request->device_out);
        ok = UNIT_SWEEP_STATUS_CHECK(cudaGetLastError(), request->device_out, error);
    }
    return (ok != 0) ? 0L : UNIT_SWEEP_ERROR;
}

extern "C" long unit_sweep_lanes_compare(const UnitSweepComparison *comparison)
{
    if ((comparison == NULL) || (comparison->error == NULL))
    {
        return UNIT_SWEEP_ERROR;
    }
    EngineError *const error = comparison->error;
    if (!UNIT_SWEEP_CHECK((comparison->device_left != NULL) && (comparison->device_right != NULL) &&
                              (comparison->disagreements != NULL),
                          comparison, error, ENGINE_ERROR_REQUEST))
    {
        return UNIT_SWEEP_ERROR;
    }
    UnitSweepResident *const buffers = &s_unit_sweep_resident;
    int ok = (buffers->disagreements != NULL) ||
             UNIT_SWEEP_STATUS_CHECK(cudaMalloc((void **)&buffers->disagreements, sizeof(unsigned long long)),
                                     &buffers->disagreements, error);
    ok = ok && UNIT_SWEEP_STATUS_CHECK(cudaMemset(buffers->disagreements, 0, sizeof(unsigned long long)),
                                       buffers->disagreements, error);
    if ((ok != 0) && (comparison->lanes != 0ull))
    {
        // the lanes are voxels, held below 2^32 by the caller: the block count fits unsigned int
        unit_sweep_compare_kernel<<<(unsigned int)((comparison->lanes + UNIT_SWEEP_THREADS - 1u) / UNIT_SWEEP_THREADS),
                                    UNIT_SWEEP_THREADS>>>(comparison->device_left, comparison->device_right,
                                                          comparison->lanes, comparison->limbs, buffers->disagreements);
        ok = UNIT_SWEEP_STATUS_CHECK(cudaGetLastError(), buffers->disagreements, error);
    }
    ok = ok && UNIT_SWEEP_STATUS_CHECK(cudaMemcpy(comparison->disagreements, buffers->disagreements,
                                                  sizeof(unsigned long long), cudaMemcpyDeviceToHost),
                                       comparison->disagreements, error);
    return (ok != 0) ? 0L : UNIT_SWEEP_ERROR;
}

extern "C" void unit_sweep_release(void)
{
    UnitSweepResident *const buffers = &s_unit_sweep_resident;
    cudaFree(buffers->narrow_planes);
    cudaFree(buffers->wide_planes);
    cudaFree(buffers->disagreements);
    cudaFree(buffers->over_width);
    memset(buffers, 0, sizeof(*buffers));
}
