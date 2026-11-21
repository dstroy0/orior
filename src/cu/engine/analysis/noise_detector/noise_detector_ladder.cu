// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// noise_detector_ladder.cu: the ladder
#include "noise_detector_internal.h"
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"

// A sample's quiet static voxels, their frames in blocks as the moment pass takes them, each retained block added to
// the ladder's sums at its voxel's level over the other blocks
static long noise_ladder_sample(const unsigned short *volume, const unsigned long long extent[4], NoiseLadderSums *sums,
                                unsigned long long *left_out, EngineError *error)
{
    const unsigned long long frames = extent[0];
    const unsigned long long voxels = extent[1] * extent[2] * extent[3];
    const unsigned long long blocks = frames / NOISE_MOMENT_BLOCK;
    const unsigned long long square = NOISE_MOMENT_RANGE * NOISE_MOMENT_RANGE;
    const unsigned long long fourth = square * square;
    const unsigned long long spread = ((voxels != 0ULL) && (frames <= (~0ULL / voxels))) ? (frames * voxels) : ~0ULL;
    // the moment pass's bounds, and two whole blocks so every block has another to set its level
    const int bounded = (blocks >= 2ULL) && (frames <= 65536ULL) && (voxels != 0ULL) && (spread != ~0ULL) &&
                        (spread <= (NOISE_SIGNED_TOP / fourth)) && (spread <= (NOISE_SIGNED_TOP / NOISE_LANE_TOP));
    if (!NOISE_DETECTOR_CHECK(bounded, extent, error, ENGINE_ERROR_REQUEST))
    {
        return NOISE_DETECTOR_ERROR;
    }
    mmgr_bytes_set(sums, 0, sizeof(*sums));
    *left_out = 0ULL;
    unsigned long long ceiling = 0ULL;
    unsigned char *const mask = (unsigned char *)platform_cell_taken((unsigned long long)((size_t)voxels),
                                                                     (unsigned int)platform_place_g_engine_owner, 0);
    int ok = NOISE_DETECTOR_CHECK(mask != NULL, &mask, error, ENGINE_ERROR_RESOURCE) &&
             NOISE_DETECTOR_CHECK(noise_static_mask(volume, frames, voxels, 1u, mask, &ceiling) != 0, mask, error,
                                  ENGINE_ERROR_RESOURCE);
    unsigned long long members = 0ULL;
    for (unsigned long long voxel = 0ULL; ok && (voxel < voxels); voxel += 1ULL)
    {
        members += mask[voxel];
    }
    // one more than the members. A sample with none still allocates
    unsigned long long *const places = (unsigned long long *)platform_cell_taken(
        (unsigned long long)((size_t)(members + 1ULL) * sizeof(unsigned long long)),
        (unsigned int)platform_place_g_engine_owner, 0);
    unsigned long long *const totals = (unsigned long long *)platform_cell_cleared(
        (unsigned long long)((size_t)(members + 1ULL)), (unsigned long long)(sizeof(unsigned long long)),
        (unsigned int)platform_place_g_engine_owner, 0);
    ok = ok && NOISE_DETECTOR_CHECK((places != NULL) && (totals != NULL), &places, error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        unsigned long long member = 0ULL;
        for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
        {
            if (mask[voxel] != 0u)
            {
                places[member] = voxel;
                member += 1ULL;
            }
        }
        // each voxel's frames over its whole blocks: at max 65536 lanes, below 2^32
        for (unsigned long long frame = 0ULL; frame < (blocks * NOISE_MOMENT_BLOCK); frame += 1ULL)
        {
            const unsigned short *const lanes = &volume[frame * voxels];
            for (unsigned long long each = 0ULL; each < members; each += 1ULL)
            {
                totals[each] += lanes[places[each]];
            }
        }
    }
    const long long range = (long long)NOISE_MOMENT_RANGE;
    const long long n = (long long)NOISE_MOMENT_BLOCK;
    for (unsigned long long block = 0ULL; ok && (block < blocks); block += 1ULL)
    {
        const unsigned short *const first = &volume[block * NOISE_MOMENT_BLOCK * voxels];
        for (unsigned long long each = 0ULL; each < members; each += 1ULL)
        {
            const unsigned short *const lane = &first[places[each]];
            unsigned long long total = 0ULL;
            for (unsigned long long frame = 0ULL; frame < NOISE_MOMENT_BLOCK; frame += 1ULL)
            {
                total += lane[frame * voxels];
            }
            // a mean of lanes rounded to the nearest is itself a lane
            const long long center = (long long)((total + (NOISE_MOMENT_BLOCK / 2ULL)) / NOISE_MOMENT_BLOCK);
            long long power[NOISE_MOMENT_POWERS] = {0LL, 0LL, 0LL, 0LL};
            int within = 1;
            for (unsigned long long frame = 0ULL; within && (frame < NOISE_MOMENT_BLOCK); frame += 1ULL)
            {
                const long long apart = (long long)lane[frame * voxels] - center;
                within = (apart <= range) && (apart >= -range);
                if (within != 0)
                {
                    const long long apart_squared = apart * apart;
                    power[0] += apart;
                    power[1] += apart_squared;
                    power[2] += apart_squared * apart;
                    power[3] += apart_squared * apart_squared;
                }
            }
            if (within == 0)
            {
                *left_out += 1ULL;
                continue;
            }
            // the block's own frames are part of its voxel's total. The rest is never negative
            const unsigned long long place = totals[each] - total;
            const long long s1 = power[0];
            const long long s2 = power[1];
            const long long s3 = power[2];
            const long long s4 = power[3];
            // |S1| is at max n / 2, S2 at max n reach^2, |S3| at max n reach^3 and S4 at max n reach^4. Each
            // term below is under 2^44 and each numerator under 2^46
            const long long readings[NOISE_LADDER_ORDERS] = {
                (n * s2) - (s1 * s1), (2LL * s1 * s1 * s1) - (3LL * n * s1 * s2) + (n * n * s3),
                (n * n * (n + 1LL) * s4) - (4LL * n * (n + 1LL) * s1 * s3) - (3LL * n * (n - 1LL) * s2 * s2) +
                    (12LL * n * s1 * s1 * s2) - (6LL * s1 * s1 * s1 * s1)};
            sums->blocks += 1ULL;
            noise_wide_add(&sums->along, place, 0ULL);
            noise_wide_add_square(&sums->along_square, place);
            for (unsigned int order = 0u; order < NOISE_LADDER_ORDERS; order += 1u)
            {
                noise_signed_wide_add(&sums->measured[order], 1ULL, readings[order]);
                noise_signed_wide_add(&sums->cross[order], place, readings[order]);
            }
        }
    }
    platform_cell_released(mask, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(places, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(totals, (unsigned int)platform_place_g_engine_owner);
    return (ok != 0) ? 0L : NOISE_DETECTOR_ERROR;
}

extern "C" long noise_ladder_volume(const unsigned short *volume, const unsigned long long extent[4],
                                    NoiseLadderMeasurement *measurement, EngineError *error)
{
    if (error == NULL)
    {
        return NOISE_DETECTOR_ERROR;
    }
    if (!NOISE_DETECTOR_CHECK((volume != NULL) && (extent != NULL) && (measurement != NULL), volume, error,
                              ENGINE_ERROR_REQUEST))
    {
        return NOISE_DETECTOR_ERROR;
    }
    NoiseLadderSums *const sums = (NoiseLadderSums *)platform_cell_taken(
        (unsigned long long)(sizeof(NoiseLadderSums)), (unsigned int)platform_place_g_engine_owner, 0);
    unsigned long long left_out = 0ULL;
    int ok = NOISE_DETECTOR_CHECK(sums != NULL, &sums, error, ENGINE_ERROR_RESOURCE) &&
             (noise_ladder_sample(volume, extent, sums, &left_out, error) == 0L);
    // a block's level is its voxel's frames over the other blocks. The level scale is those frames' count
    const unsigned long long other_frames = ((extent[0] / NOISE_MOMENT_BLOCK) - 1ULL) * NOISE_MOMENT_BLOCK;
    NoiseLadderMeasurement read;
    mmgr_bytes_set(&read, 0, sizeof(read));
    unsigned long long falling = NOISE_MOMENT_BLOCK;
    for (unsigned int order = 0u; ok && (order < NOISE_LADDER_ORDERS); order += 1u)
    {
        // k of order j + 2 is its numerator over n (n - 1) down to n - j - 1
        falling *= NOISE_MOMENT_BLOCK - (unsigned long long)(order + 1u);
        NoiseLineSums line;
        noise_line_sums_zero(&line);
        noise_exact_word(&line.samples, sums->blocks);
        noise_exact_wide(&line.along, &sums->along);
        noise_exact_wide(&line.along_square, &sums->along_square);
        ok = NOISE_DETECTOR_CHECK(noise_exact_signed_wide(&line.measured, &sums->measured[order]) &&
                                      noise_exact_signed_wide(&line.cross, &sums->cross[order]),
                                  sums, error, ENGINE_ERROR_RESOURCE) &&
             (noise_line_fit(&line, other_frames, falling, &read.cumulant[order], error) == 0L);
    }
    read.blocks = (sums != NULL) ? sums->blocks : 0ULL;
    read.left_out = left_out;
    platform_cell_released(sums, (unsigned int)platform_place_g_engine_owner);
    const NoiseLine *const second = &read.cumulant[0];
    const NoiseLine *const third = &read.cumulant[1];
    const NoiseLine *const fourth = &read.cumulant[2];
    if (ok == 0)
    {
        return NOISE_DETECTOR_ERROR;
    }
    OriorExactInteger left;
    OriorExactInteger right;
    OriorExactInteger term;
    // s3 >= s2^2: S3 D2^2 >= S2^2 D3, every denominator positive
    int formed = (orior_exact_multiply(&second->denominator, &second->denominator, &term) == ORIOR_EXACT_OK) &&
                 (orior_exact_multiply(&third->slope, &term, &left) == ORIOR_EXACT_OK) &&
                 (orior_exact_multiply(&second->slope, &second->slope, &term) == ORIOR_EXACT_OK) &&
                 (orior_exact_multiply(&term, &third->denominator, &right) == ORIOR_EXACT_OK);
    read.rising = formed && (orior_exact_compare(&left, &right) >= 0);
    // s2 s4 >= s3^2: S2 S4 D3^2 >= S3^2 D2 D4
    formed = formed && (orior_exact_multiply(&second->slope, &fourth->slope, &term) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&term, &third->denominator, &term) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&term, &third->denominator, &left) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&third->slope, &third->slope, &term) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&term, &second->denominator, &term) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&term, &fourth->denominator, &right) == ORIOR_EXACT_OK);
    read.convex = formed && (orior_exact_compare(&left, &right) >= 0);
    // C19: O = -C3 / S3 and R^2 = (C2 S3 - C3 S2) / (D2 S3), each turned to a positive denominator
    read.tail = formed && (third->slope.sign != 0);
    if (read.tail != 0)
    {
        orior_exact_zero(&term);
        formed = (orior_exact_subtract(&term, &third->intercept, &read.offset) == ORIOR_EXACT_OK) &&
                 (orior_exact_multiply(&second->intercept, &third->slope, &left) == ORIOR_EXACT_OK) &&
                 (orior_exact_multiply(&third->intercept, &second->slope, &right) == ORIOR_EXACT_OK) &&
                 (orior_exact_subtract(&left, &right, &read.read_square) == ORIOR_EXACT_OK) &&
                 (orior_exact_multiply(&second->denominator, &third->slope, &read.read_square_denominator) ==
                  ORIOR_EXACT_OK);
        read.offset_denominator = third->slope;
        if (third->slope.sign < 0)
        {
            read.offset.sign = -read.offset.sign;
            read.offset_denominator.sign = -read.offset_denominator.sign;
            read.read_square.sign = -read.read_square.sign;
            read.read_square_denominator.sign = -read.read_square_denominator.sign;
        }
    }
    if (!NOISE_DETECTOR_CHECK(formed, &read, error, ENGINE_ERROR_RESOURCE))
    {
        return NOISE_DETECTOR_ERROR;
    }
    *measurement = read;
    return 0L;
}
