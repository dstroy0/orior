// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// floor_track_floors.cu: floors, the engine and the planes kernel
#include "floor_track_internal.h"
#include "memoria_operor/memoria_operor.h"

// the tower's own floor shift, toward minus infinity, as tower_*.cu takes it
static long long track_floor_shift(long long value, unsigned int shift)
{
    return (value >= 0LL) ? (value >> shift) : -(((-value) + (1LL << shift) - 1LL) >> shift);
}

// the high d_j of one line, as tower_*.cu's forward kernel reads it: the edge repeats its left neighbor
static long long track_high(const long long *from, unsigned long long line, unsigned long long stride,
                            unsigned long long length, unsigned long long j)
{
    const long long left = from[line + (2ULL * j * stride)];
    const long long right = ((2ULL * j) + 2ULL < length) ? from[line + (((2ULL * j) + 2ULL) * stride)] : left;
    return from[line + (((2ULL * j) + 1ULL) * stride)] - track_floor_shift(left + right, 1u);
}

// one coefficient of one line after the lifting along it: the lows first, then the highs
static long long track_lifted(const long long *from, unsigned long long line, unsigned long long stride,
                              unsigned long long length, unsigned long long along)
{
    const unsigned long long lows = (length + 1ULL) / 2ULL;
    const unsigned long long highs = length / 2ULL;
    if (along >= lows)
    {
        return track_high(from, line, stride, length, along - lows);
    }
    const long long before = (along > 0ULL) ? track_high(from, line, stride, length, along - 1ULL)
                                            : track_high(from, line, stride, length, 0ULL);
    const long long after = (along < highs) ? track_high(from, line, stride, length, along) : before;
    return from[line + (2ULL * along * stride)] + track_floor_shift(before + after + 2LL, 2u);
}

// the host's floor 2 of one frame: two levels of the 5/3 lifting, each along z, then y, then x over the active
// corner, every axis pass reading the coefficients as they was before it, as the engine's kernels do
void track_floor_host(const unsigned short *lanes, long long *work, long long *before, long long *floor_values)
{
    const unsigned long long stride[SIM_AXES] = {TRACK_SIDE * TRACK_SIDE, TRACK_SIDE, 1ULL};
    for (unsigned long long sample = 0ULL; sample < TRACK_SAMPLES; sample += 1ULL)
    {
        work[sample] = (long long)lanes[sample];
    }
    for (unsigned int level = 0u; level < TRACK_FLOOR; level += 1u)
    {
        const unsigned long long active = TRACK_SIDE >> level;
        for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = before, .src = work,
                       .bytes = (size_t)TRACK_SAMPLES * sizeof(long long));
            for (unsigned long long z = 0ULL; z < active; z += 1ULL)
            {
                for (unsigned long long y = 0ULL; y < active; y += 1ULL)
                {
                    for (unsigned long long x = 0ULL; x < active; x += 1ULL)
                    {
                        const unsigned long long place[SIM_AXES] = {z, y, x};
                        const unsigned long long along = place[axis];
                        const unsigned long long line = (z * stride[0]) + (y * stride[1]) + x - (along * stride[axis]);
                        work[line + (along * stride[axis])] = track_lifted(before, line, stride[axis], active, along);
                    }
                }
            }
        }
    }
    for (unsigned long long z = 0ULL; z < TRACK_FLOOR_SIDE; z += 1ULL)
    {
        for (unsigned long long y = 0ULL; y < TRACK_FLOOR_SIDE; y += 1ULL)
        {
            for (unsigned long long x = 0ULL; x < TRACK_FLOOR_SIDE; x += 1ULL)
            {
                floor_values[(((z * TRACK_FLOOR_SIDE) + y) * TRACK_FLOOR_SIDE) + x] =
                    work[(z * stride[0]) + (y * stride[1]) + x];
            }
        }
    }
}

// the engine's floor 2 of one frame: the tower lifts the frame, and the levels past floor 2 lift only its corner.
// The crystal's corner lowered as a tower of its own side is floor 2, every value confined to the lane
int track_floor_engine(const unsigned short *device_lanes, int *crystal, int *corner, unsigned short *floor_values)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const int *coefficients = NULL;
    unsigned int *scratch = NULL;
    unsigned int floors = 0u;
    TowerLiftRequest lift;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &lift, .bytes = sizeof(lift), .val = 0);
    lift.device_lanes = device_lanes;
    lift.extent[0] = 1ULL;
    lift.extent[1] = TRACK_SIDE;
    lift.extent[2] = TRACK_SIDE;
    lift.extent[3] = TRACK_SIDE;
    lift.coefficients = &coefficients;
    lift.scratch = &scratch;
    lift.floors = &floors;
    lift.error = &error;
    int ok =
        (tower_lift(&lift) == 0L) && (error.type == ENGINE_ERROR_NONE) &&
        (cudaMemcpy(crystal, coefficients, (size_t)TRACK_SAMPLES * sizeof(int), cudaMemcpyDeviceToHost) == cudaSuccess);
    for (unsigned long long z = 0ULL; ok && (z < TRACK_FLOOR_SIDE); z += 1ULL)
    {
        for (unsigned long long y = 0ULL; y < TRACK_FLOOR_SIDE; y += 1ULL)
        {
            for (unsigned long long x = 0ULL; x < TRACK_FLOOR_SIDE; x += 1ULL)
            {
                corner[(((z * TRACK_FLOOR_SIDE) + y) * TRACK_FLOOR_SIDE) + x] =
                    crystal[(((z * TRACK_SIDE) + y) * TRACK_SIDE) + x];
            }
        }
    }
    const unsigned long long floor_extent[4] = {1ULL, TRACK_FLOOR_SIDE, TRACK_FLOOR_SIDE, TRACK_FLOOR_SIDE};
    int *device_buffer = NULL;
    ok = ok && (tower_capacity(floor_extent, &device_buffer, &error) == 0L) &&
         (cudaMemcpy(device_buffer, corner, (size_t)TRACK_FLOOR_VALUES * sizeof(int), cudaMemcpyHostToDevice) ==
          cudaSuccess);
    unsigned long long mismatches = 0ULL;
    const unsigned short *device_rebuilt = NULL;
    TowerLowerRequest lower;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &lower, .bytes = sizeof(lower), .val = 0);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = lower.extent, .src = floor_extent, .bytes = sizeof(lower.extent));
    lower.mismatches = &mismatches;
    lower.device_rebuilt = &device_rebuilt;
    lower.rebuilt = floor_values;
    lower.error = &error;
    return ok && (tower_lower(&lower) == 0L) && (error.type == ENGINE_ERROR_NONE);
}

// the planes: bit j of floor 2's value at position i is bit i % 32 of plane j's word i / 32, one warp a word
__global__ void track_planes_kernel(const unsigned short *floor_values, unsigned int *planes)
{
    const unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    const unsigned int value = floor_values[index];
    for (unsigned int bit = 0u; bit < TRACK_BITS; bit += 1u)
    {
        const unsigned int word = __ballot_sync(0xFFFFFFFFu, ((value >> bit) & 1u) != 0u);
        if ((threadIdx.x % TRACK_WORD_BITS) == 0u)
        {
            planes[((unsigned long long)bit * TRACK_WORDS) + (index / TRACK_WORD_BITS)] = word;
        }
    }
}
