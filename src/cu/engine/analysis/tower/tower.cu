// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tower.cu: the tower's kernels and plan
#include "memoria_operor/memoria_operor.h"
#include "tower_internal.h"

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

__device__ void tower_forward_span(const int *from, int *to, TowerStep step, unsigned int *overflow,
                                   unsigned long long first, unsigned long long jump)
{
    for (unsigned long long index = first; index < step.count; index += jump)
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

__global__ void tower_forward_kernel(const int *from, int *to, TowerStep step, unsigned int *overflow)
{
    tower_forward_span(from, to, step, overflow, ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x,
                       (unsigned long long)gridDim.x * blockDim.x);
}

__device__ static long long tower_even(const int *from, unsigned long long line, unsigned long long stride,
                                       unsigned long long lows, unsigned long long highs, unsigned long long i)
{
    const long long before = (long long)from[line + ((lows + ((i > 0ULL) ? (i - 1ULL) : 0ULL)) * stride)];
    const long long after = (i < highs) ? (long long)from[line + ((lows + i) * stride)] : before;
    return (long long)from[line + (i * stride)] - tower_floor_shift(before + after + 2LL, 2u);
}

__device__ void tower_inverse_span(const int *from, int *to, TowerStep step, unsigned long long first,
                                   unsigned long long jump)
{
    for (unsigned long long index = first; index < step.count; index += jump)
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

__global__ void tower_inverse_kernel(const int *from, int *to, TowerStep step)
{
    tower_inverse_span(from, to, step, ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x,
                       (unsigned long long)gridDim.x * blockDim.x);
}

__device__ void tower_copy_span(const int *from, int *to, TowerStep step, unsigned long long first,
                                unsigned long long jump)
{
    for (unsigned long long index = first; index < step.count; index += jump)
    {
        unsigned long long along = 0ULL;
        const unsigned long long lane = tower_line(step, index, &along) + (along * step.stride[step.axis]);
        to[lane] = from[lane];
    }
}

__global__ void tower_copy_kernel(const int *from, int *to, TowerStep step)
{
    tower_copy_span(from, to, step, ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x,
                    (unsigned long long)gridDim.x * blockDim.x);
}

__device__ void tower_edge_span(int *state, TowerStep region, const unsigned int *table, unsigned int index_bits,
                                unsigned long long first, unsigned long long jump)
{
    const unsigned int mask = (1u << index_bits) - 1u;
    for (unsigned long long index = first; index < region.count; index += jump)
    {
        unsigned long long offset = 0ULL;
        unsigned long long rest = index;
        for (unsigned int axis = 4u; axis > 0u; axis -= 1u)
        {
            const unsigned long long digit = rest % region.extent[axis - 1u];
            rest /= region.extent[axis - 1u];
            offset += digit * region.stride[axis - 1u];
        }
        // The table permutes the low index_bits, and the high bits pass through. The map is a bijection on the word
        const unsigned int word = (unsigned int)state[offset];
        state[offset] = (int)((word & ~mask) | table[word & mask]);
    }
}

__global__ void tower_edge_kernel(int *state, TowerStep region, const unsigned int *table, unsigned int index_bits)
{
    tower_edge_span(state, region, table, index_bits, ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x,
                    (unsigned long long)gridDim.x * blockDim.x);
}

__device__ void tower_widen_span(const unsigned short *lanes, unsigned long long count, int *to,
                                 unsigned long long first, unsigned long long jump)
{
    for (unsigned long long index = first; index < count; index += jump)
    {
        // a 16-bit lane widens to int exactly
        to[index] = (int)lanes[index];
    }
}

__global__ void tower_widen_kernel(const unsigned short *lanes, unsigned long long count, int *to)
{
    tower_widen_span(lanes, count, to, ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x,
                     (unsigned long long)gridDim.x * blockDim.x);
}

__device__ void tower_take_span(const int *values, unsigned long long count, int *to, unsigned int *overflow,
                                unsigned long long first, unsigned long long jump)
{
    for (unsigned long long index = first; index < count; index += jump)
    {
        const int value = values[index];
        if ((value >= TOWER_LIMIT) || (value <= -TOWER_LIMIT))
        {
            atomicOr(overflow, 1u);
        }
        to[index] = value;
    }
}

__global__ void tower_take_kernel(const int *values, unsigned long long count, int *to, unsigned int *overflow)
{
    tower_take_span(values, count, to, overflow, ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x,
                    (unsigned long long)gridDim.x * blockDim.x);
}

__device__ void tower_differ_span(const int *rebuilt, const unsigned short *lanes, unsigned long long count,
                                  unsigned long long *mismatches, unsigned long long first, unsigned long long jump)
{
    unsigned long long differ = 0ULL;
    for (unsigned long long index = first; index < count; index += jump)
    {
        differ += (rebuilt[index] != (int)lanes[index]) ? 1ULL : 0ULL;
    }
    if (differ != 0ULL)
    {
        atomicAdd(mismatches, differ);
    }
}

__global__ void tower_differ_kernel(const int *rebuilt, const unsigned short *lanes, unsigned long long count,
                                    unsigned long long *mismatches)
{
    tower_differ_span(rebuilt, lanes, count, mismatches, ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x,
                      (unsigned long long)gridDim.x * blockDim.x);
}

__device__ void tower_narrow_span(const int *rebuilt, unsigned long long count, unsigned short *to,
                                  unsigned int *outside, unsigned long long first, unsigned long long jump)
{
    unsigned int wide = 0u;
    for (unsigned long long index = first; index < count; index += jump)
    {
        const int value = rebuilt[index];
        wide |= ((value & ~0xFFFF) != 0) ? 1u : 0u;
        // The mask keeps the low 16 bits of the value, which an unsigned short represents exactly
        to[index] = (unsigned short)((unsigned int)value & 0xFFFFu);
    }
    if (wide != 0u)
    {
        atomicOr(outside, 1u);
    }
}

__global__ void tower_narrow_kernel(const int *rebuilt, unsigned long long count, unsigned short *to,
                                    unsigned int *outside)
{
    tower_narrow_span(rebuilt, count, to, outside, ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x,
                      (unsigned long long)gridDim.x * blockDim.x);
}

unsigned int tower_blocks(unsigned long long count)
{
    const unsigned long long needed = (count + TOWER_THREADS - 1ULL) / TOWER_THREADS;
    return (unsigned int)((needed < 65536ULL) ? ((needed == 0ULL) ? 1ULL : needed) : 65536ULL);
}

void tower_floors(const unsigned long long *extent, TowerFloors *floors)
{
    floors->count = 0u;
    TowerStep step;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &step, .bytes = sizeof(step), .val = 0);
    step.stride[3] = 1ULL;
    step.stride[2] = extent[3];
    step.stride[1] = extent[2] * extent[3];
    step.stride[0] = extent[1] * extent[2] * extent[3];
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        step.extent[axis] = extent[axis];
    }
    // each floor halves every extent, and so the floors stop inside TOWER_FLOORS_MAX
    while (((step.extent[0] > 1ULL) || (step.extent[1] > 1ULL) || (step.extent[2] > 1ULL) || (step.extent[3] > 1ULL)) &&
           (floors->count < TOWER_FLOORS_MAX))
    {
        step.count = step.extent[0] * step.extent[1] * step.extent[2] * step.extent[3];
        floors->step[floors->count] = step;
        floors->count += 1u;
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            step.extent[axis] = (step.extent[axis] + 1ULL) / 2ULL;
        }
    }
}

TowerResident g_tower_resident;

void tower_cells(TowerResident *resident, void **cells[TOWER_SLICES])
{
    cells[0] = (void **)&resident->coefficients;
    cells[1] = (void **)&resident->scratch;
    cells[2] = (void **)&resident->flag;
    cells[3] = (void **)&resident->mismatches;
}

extern "C" void tower_resident_release(void)
{
    TowerResident *const resident = &g_tower_resident;
    tessera_job_given(resident->edge_table, TOWER_PLACE);
    void **cells[TOWER_SLICES];
    tower_cells(resident, cells);
    for (unsigned int at = TOWER_SLICES; at > 0u; at -= 1u)
    {
        tessera_job_given(*cells[at - 1u], TOWER_PLACE);
        *cells[at - 1u] = NULL;
    }
    resident->lanes = 0u;
    resident->edge_table = NULL;
    resident->edge_entries = 0u;
}

// the edge table, a persistent cell of the pool of the device that grows to the most entries that a call puts
int tower_edge_reserve(unsigned int entries, EngineError *error)
{
    TowerResident *const resident = &g_tower_resident;
    if ((size_t)entries > resident->edge_entries)
    {
        tessera_job_given(resident->edge_table, TOWER_PLACE);
        resident->edge_table = NULL;
        resident->edge_entries = 0u;
        void *cell = NULL;
        if (TOWER_CHECK(
                tessera_job_persistent_taken((unsigned long long)entries * sizeof(unsigned int), TOWER_PLACE, &cell),
                &resident->edge_table, error, ENGINE_ERROR_RESOURCE) == 0)
        {
            return 0;
        }
        // Explicit cast reads the cell as the entries that it holds
        resident->edge_table = (unsigned int *)cell;
        resident->edge_entries = entries;
    }
    return 1;
}

void tower_slices(size_t lanes, unsigned long long bytes[TOWER_SLICES])
{
    bytes[0] = (unsigned long long)lanes * sizeof(int);
    bytes[1] = (unsigned long long)lanes * sizeof(int);
    bytes[2] = sizeof(unsigned int);
    bytes[3] = sizeof(unsigned long long);
}

__host__ __device__ void tower_edge_region(const TowerFloors *floors, const unsigned long long *extent,
                                           unsigned int floor_at, TowerStep *region)
{
    // the strides are the strides of the whole extent, which stay as they are across the levels that a lift works in
    // place
    region->stride[3] = 1ULL;
    region->stride[2] = extent[3];
    region->stride[1] = extent[2] * extent[3];
    region->stride[0] = extent[1] * extent[2] * extent[3];
    region->axis = 0u;
    region->count = 1ULL;
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        // The whole extent where the tower has no floor. Else the extent of the floor that the edges act on, or above
        // the last floor the half of it that the last level leaves
        unsigned long long active = extent[axis];
        if ((floors->count != 0u) && (floor_at < floors->count))
        {
            active = floors->step[floor_at].extent[axis];
        }
        else if (floors->count != 0u)
        {
            active = (floors->step[floors->count - 1u].extent[axis] + 1ULL) / 2ULL;
        }
        region->extent[axis] = active;
        region->count *= active;
    }
}
