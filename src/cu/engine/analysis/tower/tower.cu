// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tower.cu: the tower's kernels and plan
#include "tower_internal.h"
#include "memoria_operor/memoria_linea.h"

__device__ static long long tower_floor_shift(long long value, unsigned int shift)
{
    return (value >= 0LL) ? (value >> shift) : -(((-value) + (1LL << shift) - 1LL) >> shift);
}

__device__ static unsigned long long tower_line(const TowerStep &step, unsigned long long index,
                                                unsigned long long *along)
{
    unsigned long long offset = 0ULL;
    unsigned long long rest = index;
    for (unsigned int axis = 4u; axis > 0u; axis -= 1u)
    {
        const unsigned long long digit = rest % step.extent[axis - 1u];
        rest /= step.extent[axis - 1u];
        offset += digit * step.stride[axis - 1u];
        if ((axis - 1u) == step.axis)
        {
            *along = digit;
        }
    }
    return offset - (*along * step.stride[step.axis]);
}

__device__ static long long tower_high(const int *from, unsigned long long line, unsigned long long stride,
                                       unsigned long long length, unsigned long long j)
{
    const long long left = from[line + (2ULL * j * stride)];
    const long long right = ((2ULL * j + 2ULL) < length) ? (long long)from[line + ((2ULL * j + 2ULL) * stride)] : left;
    return (long long)from[line + ((2ULL * j + 1ULL) * stride)] - tower_floor_shift(left + right, 1u);
}

__global__ void tower_forward_kernel(const int *from, int *to, TowerStep step, unsigned int *overflow)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; index < step.count;
         index += jump)
    {
        unsigned long long along = 0ULL;
        const unsigned long long line = tower_line(step, index, &along);
        const unsigned long long stride = step.stride[step.axis];
        const unsigned long long length = step.extent[step.axis];
        const unsigned long long lows = (length + 1ULL) / 2ULL;
        const unsigned long long highs = length / 2ULL;
        long long value = 0LL;
        if (along < lows)
        {
            const long long before = (along > 0ULL) ? tower_high(from, line, stride, length, along - 1ULL)
                                                    : tower_high(from, line, stride, length, 0ULL);
            const long long after = (along < highs) ? tower_high(from, line, stride, length, along) : before;
            value = (long long)from[line + (2ULL * along * stride)] + tower_floor_shift(before + after + 2LL, 2u);
        }
        else
        {
            value = tower_high(from, line, stride, length, along - lows);
        }
        if ((value >= TOWER_LIMIT) || (value <= -TOWER_LIMIT))
        {
            atomicOr(overflow, 1u);
        }
        to[line + (along * stride)] = (int)value;
    }
}

__device__ static long long tower_even(const int *from, unsigned long long line, unsigned long long stride,
                                       unsigned long long lows, unsigned long long highs, unsigned long long i)
{
    const long long before = (long long)from[line + ((lows + ((i > 0ULL) ? (i - 1ULL) : 0ULL)) * stride)];
    const long long after = (i < highs) ? (long long)from[line + ((lows + i) * stride)] : before;
    return (long long)from[line + (i * stride)] - tower_floor_shift(before + after + 2LL, 2u);
}

__global__ void tower_inverse_kernel(const int *from, int *to, TowerStep step)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; index < step.count;
         index += jump)
    {
        unsigned long long along = 0ULL;
        const unsigned long long line = tower_line(step, index, &along);
        const unsigned long long stride = step.stride[step.axis];
        const unsigned long long length = step.extent[step.axis];
        const unsigned long long lows = (length + 1ULL) / 2ULL;
        const unsigned long long highs = length / 2ULL;
        long long value = 0LL;
        if ((along % 2ULL) == 0ULL)
        {
            value = tower_even(from, line, stride, lows, highs, along / 2ULL);
        }
        else
        {
            const unsigned long long j = along / 2ULL;
            const long long left = tower_even(from, line, stride, lows, highs, j);
            const long long right =
                ((2ULL * j + 2ULL) < length) ? tower_even(from, line, stride, lows, highs, j + 1ULL) : left;
            value = (long long)from[line + ((lows + j) * stride)] + tower_floor_shift(left + right, 1u);
        }
        to[line + (along * stride)] = (int)value;
    }
}

__global__ void tower_copy_kernel(const int *from, int *to, TowerStep step)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; index < step.count;
         index += jump)
    {
        unsigned long long along = 0ULL;
        const unsigned long long lane = tower_line(step, index, &along) + (along * step.stride[step.axis]);
        to[lane] = from[lane];
    }
}

__global__ void tower_edge_kernel(int *state, TowerStep region, const unsigned int *table, unsigned int index_bits)
{
    const unsigned int mask = (1u << index_bits) - 1u;
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; index < region.count;
         index += jump)
    {
        unsigned long long offset = 0ULL;
        unsigned long long rest = index;
        for (unsigned int axis = 4u; axis > 0u; axis -= 1u)
        {
            const unsigned long long digit = rest % region.extent[axis - 1u];
            rest /= region.extent[axis - 1u];
            offset += digit * region.stride[axis - 1u];
        }
        // the low index_bits are permuted, the high bits pass through. The map is a bijection on the word
        const unsigned int word = (unsigned int)state[offset];
        state[offset] = (int)((word & ~mask) | table[word & mask]);
    }
}

__global__ void tower_widen_kernel(const unsigned short *lanes, unsigned long long count, int *to)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; index < count;
         index += jump)
    {
        // a 16-bit lane widens to int exactly
        to[index] = (int)lanes[index];
    }
}

__global__ void tower_take_kernel(const int *values, unsigned long long count, int *to, unsigned int *overflow)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; index < count;
         index += jump)
    {
        const int value = values[index];
        if ((value >= TOWER_LIMIT) || (value <= -TOWER_LIMIT))
        {
            atomicOr(overflow, 1u);
        }
        to[index] = value;
    }
}

__global__ void tower_differ_kernel(const int *rebuilt, const unsigned short *lanes, unsigned long long count,
                                    unsigned long long *mismatches)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    unsigned long long differ = 0ULL;
    for (unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; index < count;
         index += jump)
    {
        differ += (rebuilt[index] != (int)lanes[index]) ? 1ULL : 0ULL;
    }
    if (differ != 0ULL)
    {
        atomicAdd(mismatches, differ);
    }
}

__global__ void tower_narrow_kernel(const int *rebuilt, unsigned long long count, unsigned short *to,
                                    unsigned int *outside)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    unsigned int wide = 0u;
    for (unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; index < count;
         index += jump)
    {
        const int value = rebuilt[index];
        wide |= ((value & ~0xFFFF) != 0) ? 1u : 0u;
        // the value is masked to its low 16 bits, which an unsigned short represents exactly
        to[index] = (unsigned short)((unsigned int)value & 0xFFFFu);
    }
    if (wide != 0u)
    {
        atomicOr(outside, 1u);
    }
}

unsigned int tower_blocks(unsigned long long count)
{
    const unsigned long long needed = (count + TOWER_THREADS - 1ULL) / TOWER_THREADS;
    return (unsigned int)((needed < 65536ULL) ? ((needed == 0ULL) ? 1ULL : needed) : 65536ULL);
}

std::vector<TowerStep> tower_floors(const unsigned long long *extent)
{
    std::vector<TowerStep> floors;
    TowerStep step;
    mmgr_bytes_set(&step, 0, sizeof(step));
    step.stride[3] = 1ULL;
    step.stride[2] = extent[3];
    step.stride[1] = extent[2] * extent[3];
    step.stride[0] = extent[1] * extent[2] * extent[3];
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        step.extent[axis] = extent[axis];
    }
    while ((step.extent[0] > 1ULL) || (step.extent[1] > 1ULL) || (step.extent[2] > 1ULL) || (step.extent[3] > 1ULL))
    {
        step.count = step.extent[0] * step.extent[1] * step.extent[2] * step.extent[3];
        floors.push_back(step);
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            step.extent[axis] = (step.extent[axis] + 1ULL) / 2ULL;
        }
    }
    return floors;
}

TowerResident g_tower_resident;

extern "C" void tower_resident_release(void)
{
    TowerResident *const resident = &g_tower_resident;
    device_pool_release(&resident->pool);
    cudaFree(resident->edge_table);
    resident->coefficients = NULL;
    resident->scratch = NULL;
    resident->flag = NULL;
    resident->mismatches = NULL;
    resident->lanes = 0u;
    resident->edge_table = NULL;
    resident->edge_entries = 0u;
}

int tower_edge_reserve(unsigned int entries, EngineError *error)
{
    TowerResident *const resident = &g_tower_resident;
    if ((size_t)entries > resident->edge_entries)
    {
        cudaFree(resident->edge_table);
        resident->edge_table = NULL;
        resident->edge_entries = 0u;
        if (TOWER_STATUS_CHECK(cudaMalloc((void **)&resident->edge_table, (size_t)entries * sizeof(unsigned int)),
                               &resident->edge_table, error) == 0)
        {
            return 0;
        }
        resident->edge_entries = entries;
    }
    return 1;
}

// the pool's slices for `lanes`, in the order they are arranged and taken: the coefficients, the scratch, the flag and
// the mismatch count. The plan is arranged from them, and a pool reserved from it takes them
DevicePoolPlan tower_plan(size_t lanes, EngineError *error, DevicePoolTakeRequest takes[TOWER_SLICES])
{
    TowerResident *const resident = &g_tower_resident;
    const DevicePoolTakeRequest requests[TOWER_SLICES] = {
        {&resident->pool, lanes * sizeof(int), (void **)&resident->coefficients, error},
        {&resident->pool, lanes * sizeof(int), (void **)&resident->scratch, error},
        {&resident->pool, sizeof(unsigned int), (void **)&resident->flag, error},
        {&resident->pool, sizeof(unsigned long long), (void **)&resident->mismatches, error}};
    DevicePoolPlan plan = {0ULL, 0ULL, 0};
    for (unsigned int at = 0u; at < TOWER_SLICES; at += 1u)
    {
        takes[at] = requests[at];
        device_pool_plan_slice(&plan, requests[at].bytes);
    }
    return plan;
}
