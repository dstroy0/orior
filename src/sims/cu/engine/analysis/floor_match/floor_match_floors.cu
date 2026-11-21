// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// floor_match_floors.cu: floors, the engine and the kernels
#include "floor_match_internal.h"
#include "memoria_operor/memoria_linea.h"

// the tower's own floor shift, toward minus infinity, as tower_*.cu takes it
static long long match_floor_shift(long long value, unsigned int shift)
{
    return (value >= 0LL) ? (value >> shift) : -(((-value) + (1LL << shift) - 1LL) >> shift);
}

// the high d_j of one line, as tower_*.cu's forward kernel reads it: the edge repeats its left neighbor
static long long match_high(const long long *from, unsigned long long line, unsigned long long stride,
                            unsigned long long length, unsigned long long j)
{
    const long long left = from[line + (2ULL * j * stride)];
    const long long right = ((2ULL * j) + 2ULL < length) ? from[line + (((2ULL * j) + 2ULL) * stride)] : left;
    return from[line + (((2ULL * j) + 1ULL) * stride)] - match_floor_shift(left + right, 1u);
}

// one coefficient of one line after the lifting along it: the lows first, then the highs
static long long match_lifted(const long long *from, unsigned long long line, unsigned long long stride,
                              unsigned long long length, unsigned long long along)
{
    const unsigned long long lows = (length + 1ULL) / 2ULL;
    const unsigned long long highs = length / 2ULL;
    if (along >= lows)
    {
        return match_high(from, line, stride, length, along - lows);
    }
    const long long before = (along > 0ULL) ? match_high(from, line, stride, length, along - 1ULL)
                                            : match_high(from, line, stride, length, 0ULL);
    const long long after = (along < highs) ? match_high(from, line, stride, length, along) : before;
    return from[line + (2ULL * along * stride)] + match_floor_shift(before + after + 2LL, 2u);
}

// the host's floor 2: two levels of the 5/3 lifting, each along z, then y, then x over the active corner, every
// axis pass reading the coefficients as they was before it, as the engine's kernels do
void match_floor_host(const unsigned short *lanes, long long *work, long long *before, long long *floor_values)
{
    const unsigned long long stride[SIM_AXES] = {MATCH_SIDE * MATCH_SIDE, MATCH_SIDE, 1ULL};
    for (unsigned long long sample = 0ULL; sample < MATCH_SAMPLES; sample += 1ULL)
    {
        work[sample] = (long long)lanes[sample];
    }
    for (unsigned int level = 0u; level < MATCH_FLOOR; level += 1u)
    {
        const unsigned long long active = MATCH_SIDE >> level;
        for (unsigned int axis = 0u; axis < SIM_AXES; axis += 1u)
        {
            mmgr_bytes_copy(before, work, (size_t)MATCH_SAMPLES * sizeof(long long));
            for (unsigned long long z = 0ULL; z < active; z += 1ULL)
            {
                for (unsigned long long y = 0ULL; y < active; y += 1ULL)
                {
                    for (unsigned long long x = 0ULL; x < active; x += 1ULL)
                    {
                        const unsigned long long place[SIM_AXES] = {z, y, x};
                        const unsigned long long along = place[axis];
                        const unsigned long long line = (z * stride[0]) + (y * stride[1]) + x - (along * stride[axis]);
                        work[line + (along * stride[axis])] = match_lifted(before, line, stride[axis], active, along);
                    }
                }
            }
        }
    }
    for (unsigned long long z = 0ULL; z < MATCH_FLOOR_SIDE; z += 1ULL)
    {
        for (unsigned long long y = 0ULL; y < MATCH_FLOOR_SIDE; y += 1ULL)
        {
            for (unsigned long long x = 0ULL; x < MATCH_FLOOR_SIDE; x += 1ULL)
            {
                floor_values[(((z * MATCH_FLOOR_SIDE) + y) * MATCH_FLOOR_SIDE) + x] =
                    work[(z * stride[0]) + (y * stride[1]) + x];
            }
        }
    }
}

// the engine's floor 2: the tower lifts the whole frame, and the levels past floor 2 lift only its corner. The
// corner of the crystal lowered as a tower of its own side is floor 2. The lowering confines every value to the lane.
int match_floor_engine(SimResults *results, const unsigned short *device_lanes, int *crystal, int *corner,
                       unsigned short *floor_values)
{
    EngineError error;
    mmgr_bytes_set(&error, 0, sizeof(error));
    const int *coefficients = NULL;
    unsigned int *scratch = NULL;
    unsigned int floors = 0u;
    TowerLiftRequest lift;
    mmgr_bytes_set(&lift, 0, sizeof(lift));
    lift.device_lanes = device_lanes;
    lift.extent[0] = 1ULL;
    lift.extent[1] = MATCH_SIDE;
    lift.extent[2] = MATCH_SIDE;
    lift.extent[3] = MATCH_SIDE;
    lift.coefficients = &coefficients;
    lift.scratch = &scratch;
    lift.floors = &floors;
    lift.error = &error;
    int ok = (tower_lift(&lift) == 0L) && (error.type == ENGINE_ERROR_NONE);
    sim_check(results, ok, "the engine's tower lifts the frame");
    ok = ok &&
         sim_status_check(
             results, cudaMemcpy(crystal, coefficients, (size_t)MATCH_SAMPLES * sizeof(int), cudaMemcpyDeviceToHost),
             "crystal read");
    for (unsigned long long z = 0ULL; ok && (z < MATCH_FLOOR_SIDE); z += 1ULL)
    {
        for (unsigned long long y = 0ULL; y < MATCH_FLOOR_SIDE; y += 1ULL)
        {
            for (unsigned long long x = 0ULL; x < MATCH_FLOOR_SIDE; x += 1ULL)
            {
                corner[(((z * MATCH_FLOOR_SIDE) + y) * MATCH_FLOOR_SIDE) + x] =
                    crystal[(((z * MATCH_SIDE) + y) * MATCH_SIDE) + x];
            }
        }
    }
    const unsigned long long floor_extent[4] = {1ULL, MATCH_FLOOR_SIDE, MATCH_FLOOR_SIDE, MATCH_FLOOR_SIDE};
    int *device_buffer = NULL;
    ok = ok && (tower_capacity(floor_extent, &device_buffer, &error) == 0L) &&
         sim_status_check(
             results,
             cudaMemcpy(device_buffer, corner, (size_t)MATCH_FLOOR_VALUES * sizeof(int), cudaMemcpyHostToDevice),
             "corner write");
    unsigned long long mismatches = 0ULL;
    const unsigned short *device_rebuilt = NULL;
    TowerLowerRequest lower;
    mmgr_bytes_set(&lower, 0, sizeof(lower));
    mmgr_bytes_copy(lower.extent, floor_extent, sizeof(lower.extent));
    lower.mismatches = &mismatches;
    lower.device_rebuilt = &device_rebuilt;
    lower.rebuilt = floor_values;
    lower.error = &error;
    ok = ok && (tower_lower(&lower) == 0L) && (error.type == ENGINE_ERROR_NONE);
    sim_check(results, ok, "the crystal's 16^3 corner lowers as its own tower, every value inside the 16-bit lane");
    return ok;
}

// the planes: bit j of floor 2's value at position i is bit i % 32 of plane j's word i / 32, one warp a word
__global__ void match_planes_kernel(const unsigned short *floor_values, unsigned int *planes)
{
    const unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    const unsigned int value = floor_values[index];
    for (unsigned int bit = 0u; bit < MATCH_BITS; bit += 1u)
    {
        const unsigned int word = __ballot_sync(0xFFFFFFFFu, ((value >> bit) & 1u) != 0u);
        if ((threadIdx.x % MATCH_WORD_BITS) == 0u)
        {
            planes[((unsigned long long)bit * MATCH_WORDS) + (index / MATCH_WORD_BITS)] = word;
        }
    }
}

// one query's one word: the and over the planes, from the lowest bit up, of each plane where x's bit is 1 and its
// complement where it is 0. The word stops once its mask is empty, and its reads are the plane words it took.
__global__ void match_query_kernel(const unsigned int *planes, const unsigned int *queries, unsigned int *matches,
                                   unsigned char *reads)
{
    const unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (index >= ((unsigned long long)MATCH_QUERIES * MATCH_WORDS))
    {
        return;
    }
    const unsigned int target = queries[index / MATCH_WORDS];
    const unsigned long long word = index % MATCH_WORDS;
    unsigned int mask = 0xFFFFFFFFu;
    unsigned int taken = 0u;
    for (unsigned int bit = 0u; (bit < MATCH_BITS) && (mask != 0u); bit += 1u)
    {
        const unsigned int plane = planes[((unsigned long long)bit * MATCH_WORDS) + word];
        taken += 1u;
        mask &= (((target >> bit) & 1u) != 0u) ? plane : ~plane;
    }
    matches[index] = mask;
    // at max MATCH_BITS reads, far below a byte's range
    reads[index] = (unsigned char)taken;
}

void match_print_side(VerbaLinea *line, const char *name, const MatchSide *side)
{
    mmgr_line_put(line, "    ");
    mmgr_line_put(line, name);
    mmgr_line_put(line, ": ");
    mmgr_line_decimal(line, side->matched, 1u);
    mmgr_line_put(line, " of ");
    mmgr_line_decimal(line, MATCH_QUERIES_EACH, 1u);
    mmgr_line_put(line, " match sets equal the host's scan; positions found a query ");
    sim_fraction_print(line, side->found, MATCH_QUERIES_EACH, 2u);
    mmgr_line_put(line, " on average, ");
    mmgr_line_decimal(line, side->found_max, 1u);
    mmgr_line_put(line, " at max; plane words read a query ");
    sim_fraction_print(line, side->total, MATCH_QUERIES_EACH, 2u);
    mmgr_line_put(line, " on average, ");
    mmgr_line_decimal(line, side->max, 1u);
    mmgr_line_put(line, " at max\n");
}
