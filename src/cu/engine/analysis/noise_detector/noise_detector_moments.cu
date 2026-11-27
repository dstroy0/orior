// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// noise_detector_moments.cu: moments and cumulants
#include "../../../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "noise_detector_internal.h"

// A sample's quiet static voxels, each one's frames in blocks of NOISE_MOMENT_BLOCK. The code takes a block's values
// less their mean rounded to the nearest. It raises them to the powers 1 to 4, sums them, and bins them by that mean.
// The fixed pattern leaves with the mean. The code leaves out and counts a block with a value past NOISE_MOMENT_RANGE
// of its mean. Every power sum fits a word, and the code reads no frame past the last whole block.
static long noise_moments_sample(const unsigned short *volume, const unsigned long long extent[4],
                                 NoiseMomentCell cells[NOISE_LEVEL_BINS], unsigned long long *ceiling,
                                 unsigned long long *sampled, unsigned long long *left_out, EngineError *error)
{
    const unsigned long long frames = extent[0];
    const unsigned long long voxels = extent[1] * extent[2] * extent[3];
    const unsigned long long blocks = frames / NOISE_MOMENT_BLOCK;
    const unsigned long long square = NOISE_MOMENT_RANGE * NOISE_MOMENT_RANGE;
    const unsigned long long fourth = square * square;
    const unsigned long long spread = ((voxels != 0ULL) && (frames <= (~0ULL / voxels))) ? (frames * voxels) : ~0ULL;
    // A block's |S1| is at max half its frames, and each power at max its frames times the range to that power. S4
    // bounds every product that the cells sum but S2^2, which they sum wide. `sumS4` is at max the sample's
    // voxel-frames times the range to the fourth. The values are at max the voxel-frames times the lane's top. The
    // quiet mask stores to 65536 frames
    const int bounded = (blocks != 0ULL) && (frames <= 65536ULL) && (voxels != 0ULL) && (spread != ~0ULL) &&
                        (spread <= (NOISE_SIGNED_TOP / fourth)) && (spread <= (NOISE_SIGNED_TOP / NOISE_LANE_TOP));
    if (!NOISE_DETECTOR_CHECK(bounded, extent, error, ENGINE_ERROR_REQUEST))
    {
        return NOISE_DETECTOR_ERROR;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = cells, .bytes = (size_t)NOISE_LEVEL_BINS * sizeof(NoiseMomentCell),
               .val = 0);
    *sampled = 0ULL;
    *left_out = 0ULL;
    unsigned char *const mask =
        (unsigned char *)platform_cell_taken((size_t)voxels, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    int ok = NOISE_DETECTOR_CHECK(mask != NULL, &mask, error, ENGINE_ERROR_RESOURCE) &&
             NOISE_DETECTOR_CHECK(noise_static_mask(volume, frames, voxels, 1u, mask, ceiling) != 0, mask, error,
                                  ENGINE_ERROR_RESOURCE);
    unsigned long long members = 0ULL;
    for (unsigned long long voxel = 0ULL; ok && (voxel < voxels); voxel += 1ULL)
    {
        members += mask[voxel];
    }
    // one more than the members. A sample with none still allocates
    unsigned long long *const places = (unsigned long long *)platform_cell_taken(
        (size_t)(members + 1ULL) * sizeof(unsigned long long), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    ok = ok && NOISE_DETECTOR_CHECK(places != NULL, &places, error, ENGINE_ERROR_RESOURCE);
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
    }
    const long long range = (long long)NOISE_MOMENT_RANGE;
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
            *sampled += 1ULL;
            // the center is a lane. It re-signs to unsigned int exactly
            const unsigned int summed_bin = (unsigned int)center >> (NOISE_LEVEL_BIN_SHIFT - 1u);
            NoiseMomentCell *const cell =
                &cells[(summed_bin < NOISE_LEVEL_BINS) ? summed_bin : (NOISE_LEVEL_BINS - 1u)];
            const long long s1 = power[0];
            // S2 and S4 are sums of squares, never negative. They re-sign to unsigned long long exactly
            const unsigned long long s2 = (unsigned long long)power[1];
            const unsigned long long s1_squared = (unsigned long long)(s1 * s1);
            cell->blocks += 1ULL;
            cell->values += total;
            cell->s1 += s1;
            cell->s1_squared += s1_squared;
            cell->s1_cubed += s1 * s1 * s1;
            cell->s1_fourth += s1_squared * s1_squared;
            cell->s2 += s2;
            // S2 is at max a block's frames times the range squared, far below 2^63. It re-signs to long long
            // exactly
            cell->s1_s2 += s1 * (long long)s2;
            cell->s1_squared_s2 += s1_squared * s2;
            cell->s3 += power[2];
            cell->s1_s3 += s1 * power[2];
            cell->s4 += (unsigned long long)power[3];
            noise_wide_add_square(&cell->s2_squared, s2);
        }
    }
    platform_cell_released(mask, platform_place_g_program_owner);
    platform_cell_released(places, platform_place_g_program_owner);
    return (ok != 0) ? 0L : NOISE_DETECTOR_ERROR;
}

static int noise_moments_rows(PlatformFile *table, const char *name, const NoiseMomentCell cells[NOISE_LEVEL_BINS])
{
    int ok = 1;
    for (unsigned int level_bin = 0u; ok && (level_bin < NOISE_LEVEL_BINS); level_bin += 1u)
    {
        const NoiseMomentCell *const cell = &cells[level_bin];
        if (cell->blocks == 0ULL)
        {
            continue;
        }
        unsigned long long formed_mark_1;
        ok =
            (formed_mark_1 = platform_file_mark(table), mmgr_line_put(platform_file_line(table), name),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal(platform_file_line(table), level_bin << (NOISE_LEVEL_BIN_SHIFT - 1u), 1u),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal(platform_file_line(table), NOISE_MOMENT_BLOCK, 1u),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal(platform_file_line(table), cell->blocks, 1u),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal(platform_file_line(table), cell->values, 1u),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal_signed(platform_file_line(table), cell->s1),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal(platform_file_line(table), cell->s1_squared, 1u),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal_signed(platform_file_line(table), cell->s1_cubed),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal(platform_file_line(table), cell->s1_fourth, 1u),
             mmgr_line_put(platform_file_line(table), "\t"), mmgr_line_decimal(platform_file_line(table), cell->s2, 1u),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal_signed(platform_file_line(table), cell->s1_s2),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal(platform_file_line(table), cell->s1_squared_s2, 1u),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal(platform_file_line(table), cell->s2_squared.high, 1u),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal(platform_file_line(table), cell->s2_squared.low, 1u),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal_signed(platform_file_line(table), cell->s3),
             mmgr_line_put(platform_file_line(table), "\t"),
             mmgr_line_decimal_signed(platform_file_line(table), cell->s1_s3),
             mmgr_line_put(platform_file_line(table), "\t"), mmgr_line_decimal(platform_file_line(table), cell->s4, 1u),
             mmgr_line_put(platform_file_line(table), "\n"), platform_file_written_since(table, formed_mark_1)) > 0;
    }
    return ok;
}

extern "C" long noise_moments_set(const NoiseSetRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return NOISE_DETECTOR_ERROR;
    }
    EngineError *const error = request->error;
    char path[ENGINE_PATH_CAPACITY];
    PlatformFile *table = NULL;
    int ok = NOISE_DETECTOR_CHECK((request->set != NULL) && (request->samples != NULL) && (request->load != NULL),
                                  request, error, ENGINE_ERROR_REQUEST) &&
             noise_table_open(request->set, "noise_moments.tsv",
                              "sample\tlowest_mean\tblock_frames\tblocks\tvalues\ts1\ts1_squared\ts1_cubed\ts1_fourth"
                              "\ts2\ts1_s2\ts1_squared_s2\ts2_squared_high\ts2_squared_low\ts3\ts1_s3\ts4\n",
                              path, &table, error);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  moments: every quiet static voxel's frames in blocks of ");
        mmgr_line_decimal(output_line, NOISE_MOMENT_BLOCK, 1u);
        mmgr_line_put(output_line,
                      ", each block's values less its mean raised to the powers 1 to 4 and summed, then binned by the "
                      "mean in 8s; the second, third and fourth k-statistics follow exactly from the table");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    NoiseMomentCell *const cells = (NoiseMomentCell *)platform_cell_taken(
        (size_t)NOISE_LEVEL_BINS * sizeof(NoiseMomentCell), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    ok = ok && NOISE_DETECTOR_CHECK(cells != NULL, &cells, error, ENGINE_ERROR_RESOURCE);
    const unsigned long long began = engine_clock_microseconds();
    for (unsigned int sample = 0u; ok && (sample < request->count); sample += 1u)
    {
        const char *const name = request->samples[sample];
        unsigned long long extent[4] = {0ULL, 0ULL, 0ULL, 0ULL};
        unsigned short *volume = NULL;
        unsigned long long ceiling = 0ULL;
        unsigned long long sampled = 0ULL;
        unsigned long long left_out = 0ULL;
        ok = NOISE_DETECTOR_CHECK(request->load(request->set, name, extent, &volume, error) == 0L, name, error,
                                  ENGINE_ERROR_REQUEST) &&
             (noise_moments_sample(volume, extent, cells, &ceiling, &sampled, &left_out, error) == 0L);
        platform_cell_released(volume, platform_place_g_program_owner);
        ok = ok && NOISE_DETECTOR_IO(noise_moments_rows(table, name, cells) != 0, table, error);
        if (ok != 0)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put_columns(output_line, name, 24u);
                mmgr_line_put(output_line, " static means up to ");
                mmgr_line_decimal(output_line, ceiling, 1u);
                mmgr_line_put(output_line, "; ");
                mmgr_line_decimal(output_line, sampled, 1u);
                mmgr_line_put(output_line, " blocks retained, ");
                mmgr_line_decimal(output_line, left_out, 1u);
                mmgr_line_put(output_line, " left out past ");
                mmgr_line_decimal(output_line, NOISE_MOMENT_RANGE, 1u);
                mmgr_line_put(output_line, " of their mean");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            platform_line_flushed();
        }
    }
    platform_cell_released(cells, platform_place_g_program_owner);
    if (table != NULL)
    {
        ok = NOISE_DETECTOR_IO(platform_file_close(table) == 0, path, error) && ok;
    }
    if (ok != 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  moments: ");
        mmgr_line_decimal(output_line, request->count, 1u);
        mmgr_line_put(output_line, " samples in ");
        mmgr_line_decimal(output_line, (engine_clock_microseconds() - began) / 1000ULL, 1u);
        mmgr_line_put(output_line, " ms; every bin's sums are in ");
        mmgr_line_put(output_line, path);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (ok != 0) ? 0L : NOISE_DETECTOR_ERROR;
}

static void noise_exact_signed(OriorExactInteger *value, long long word)
{
    // the magnitude of a negative word is its two's complement negation, taken unsigned
    const unsigned long long magnitude = (word < 0LL) ? (0ULL - (unsigned long long)word) : (unsigned long long)word;
    noise_exact_word(value, magnitude);
    if (word < 0LL)
    {
        value->sign = -1;
    }
}

// sum gains factor times term, exactly
static int noise_exact_add_scaled(OriorExactInteger *sum, long long factor, const OriorExactInteger *term)
{
    OriorExactInteger product;
    noise_exact_signed(&product, factor);
    return (orior_exact_multiply(&product, term, &product) == ORIOR_EXACT_OK) &&
           (orior_exact_add(sum, &product, sum) == ORIOR_EXACT_OK);
}

// The k-statistic of order 2, 3 or 4 over a cell's blocks (the formulas at NoiseMomentCell), in signed thousandths
// rounded toward zero. 0 where the quotient passes a long long.
static int noise_moments_cumulant(const NoiseMomentCell *cell, unsigned int order, long long *thousandths)
{
    // the block's frames, 5, far below 2^31, as are every factor below
    const long long frames = (long long)NOISE_MOMENT_BLOCK;
    OriorExactInteger numerator;
    OriorExactInteger denominator;
    OriorExactInteger term;
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    orior_exact_zero(&numerator);
    // the denominator: the blocks times n (n - 1) down to n - order + 1
    long long falling = 1LL;
    for (unsigned int step = 0u; step < order; step += 1u)
    {
        falling *= frames - (long long)step;
    }
    noise_exact_word(&denominator, cell->blocks);
    // the descending product of the block's frames is positive. It re-signs to unsigned long long exactly
    noise_exact_word(&term, (unsigned long long)falling);
    int ok = orior_exact_multiply(&denominator, &term, &denominator) == ORIOR_EXACT_OK;
    if (order == 2u)
    {
        noise_exact_word(&term, cell->s2);
        ok = ok && noise_exact_add_scaled(&numerator, frames, &term);
        noise_exact_word(&term, cell->s1_squared);
        ok = ok && noise_exact_add_scaled(&numerator, -1LL, &term);
    }
    else if (order == 3u)
    {
        noise_exact_signed(&term, cell->s1_cubed);
        ok = ok && noise_exact_add_scaled(&numerator, 2LL, &term);
        noise_exact_signed(&term, cell->s1_s2);
        ok = ok && noise_exact_add_scaled(&numerator, -3LL * frames, &term);
        noise_exact_signed(&term, cell->s3);
        ok = ok && noise_exact_add_scaled(&numerator, frames * frames, &term);
    }
    else
    {
        noise_exact_word(&term, cell->s4);
        ok = ok && noise_exact_add_scaled(&numerator, frames * frames * (frames + 1LL), &term);
        noise_exact_signed(&term, cell->s1_s3);
        ok = ok && noise_exact_add_scaled(&numerator, -4LL * frames * (frames + 1LL), &term);
        noise_exact_wide(&term, &cell->s2_squared);
        ok = ok && noise_exact_add_scaled(&numerator, -3LL * frames * (frames - 1LL), &term);
        noise_exact_word(&term, cell->s1_squared_s2);
        ok = ok && noise_exact_add_scaled(&numerator, 12LL * frames, &term);
        noise_exact_word(&term, cell->s1_fourth);
        ok = ok && noise_exact_add_scaled(&numerator, -6LL, &term);
    }
    noise_exact_word(&term, 1000ULL);
    ok = ok && (orior_exact_multiply(&numerator, &term, &numerator) == ORIOR_EXACT_OK) &&
         (orior_exact_divide(&numerator, &denominator, &quotient, &remainder) == ORIOR_EXACT_OK);
    for (unsigned int limb = 2u; ok && (limb < ORIOR_EXACT_LIMBS); limb += 1u)
    {
        ok = quotient.limb[limb] == 0u;
    }
    ok = ok && (quotient.limb[1] < 0x80000000u);
    // the test above holds the magnitude below 2^63, and the code keeps the sign apart
    *thousandths =
        ok ? ((long long)(((unsigned long long)quotient.limb[1] << 32u) | quotient.limb[0]) * (long long)quotient.sign)
           : 0LL;
    return ok;
}

extern "C" long noise_moments_volume(const unsigned short *volume, const unsigned long long extent[4],
                                     long long cumulants[NOISE_MOMENT_CUMULANTS], EngineError *error)
{
    if (error == NULL)
    {
        return NOISE_DETECTOR_ERROR;
    }
    NoiseMomentCell *const cells = (NoiseMomentCell *)platform_cell_taken(
        (size_t)NOISE_LEVEL_BINS * sizeof(NoiseMomentCell), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned long long ceiling = 0ULL;
    unsigned long long sampled = 0ULL;
    unsigned long long left_out = 0ULL;
    int ok = NOISE_DETECTOR_CHECK((volume != NULL) && (extent != NULL) && (cumulants != NULL), volume, error,
                                  ENGINE_ERROR_REQUEST) &&
             NOISE_DETECTOR_CHECK(cells != NULL, &cells, error, ENGINE_ERROR_RESOURCE) &&
             (noise_moments_sample(volume, extent, cells, &ceiling, &sampled, &left_out, error) == 0L);
    // every bin pooled: each pooled sum is a sum over the sample's blocks, which the pass's bound already enforces
    NoiseMomentCell pooled;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &pooled, .bytes = sizeof(pooled), .val = 0);
    for (unsigned int level_bin = 0u; ok && (level_bin < NOISE_LEVEL_BINS); level_bin += 1u)
    {
        const NoiseMomentCell *const cell = &cells[level_bin];
        pooled.blocks += cell->blocks;
        pooled.values += cell->values;
        pooled.s1 += cell->s1;
        pooled.s1_squared += cell->s1_squared;
        pooled.s1_cubed += cell->s1_cubed;
        pooled.s1_fourth += cell->s1_fourth;
        pooled.s2 += cell->s2;
        pooled.s1_s2 += cell->s1_s2;
        pooled.s1_squared_s2 += cell->s1_squared_s2;
        pooled.s3 += cell->s3;
        pooled.s1_s3 += cell->s1_s3;
        pooled.s4 += cell->s4;
        noise_wide_add(&pooled.s2_squared, cell->s2_squared.low, cell->s2_squared.high);
    }
    for (unsigned int order = 0u; ok && (order < NOISE_MOMENT_CUMULANTS); order += 1u)
    {
        cumulants[order] = 0LL;
        if (pooled.blocks != 0ULL)
        {
            ok = NOISE_DETECTOR_CHECK(noise_moments_cumulant(&pooled, order + 2u, &cumulants[order]) != 0, &pooled,
                                      error, ENGINE_ERROR_RESOURCE);
        }
    }
    platform_cell_released(cells, platform_place_g_program_owner);
    return (ok != 0) ? 0L : NOISE_DETECTOR_ERROR;
}

extern "C" void noise_line_sums_zero(NoiseLineSums *sums)
{
    orior_exact_zero(&sums->samples);
    orior_exact_zero(&sums->along);
    orior_exact_zero(&sums->along_square);
    orior_exact_zero(&sums->measured);
    orior_exact_zero(&sums->cross);
}

// sum gains left times right, exactly
int noise_exact_add_product(OriorExactInteger *sum, const OriorExactInteger *left, const OriorExactInteger *right)
{
    OriorExactInteger product;
    return (orior_exact_multiply(left, right, &product) == ORIOR_EXACT_OK) &&
           (orior_exact_add(sum, &product, sum) == ORIOR_EXACT_OK);
}

extern "C" long noise_line_sums_add(NoiseLineSums *sums, unsigned long long count, unsigned long long place,
                                    const OriorExactInteger *total, EngineError *error)
{
    if (error == NULL)
    {
        return NOISE_DETECTOR_ERROR;
    }
    if (!NOISE_DETECTOR_CHECK((sums != NULL) && (total != NULL), sums, error, ENGINE_ERROR_REQUEST))
    {
        return NOISE_DETECTOR_ERROR;
    }
    NoiseLineSums gained = *sums;
    OriorExactInteger number;
    OriorExactInteger level;
    OriorExactInteger weighted;
    noise_exact_word(&number, count);
    noise_exact_word(&level, place);
    const int ok = (orior_exact_add(&gained.samples, &number, &gained.samples) == ORIOR_EXACT_OK) &&
                   (orior_exact_multiply(&number, &level, &weighted) == ORIOR_EXACT_OK) &&
                   (orior_exact_add(&gained.along, &weighted, &gained.along) == ORIOR_EXACT_OK) &&
                   noise_exact_add_product(&gained.along_square, &weighted, &level) &&
                   (orior_exact_add(&gained.measured, total, &gained.measured) == ORIOR_EXACT_OK) &&
                   noise_exact_add_product(&gained.cross, &level, total);
    if (!NOISE_DETECTOR_CHECK(ok, sums, error, ENGINE_ERROR_RESOURCE))
    {
        return NOISE_DETECTOR_ERROR;
    }
    *sums = gained;
    return 0L;
}

extern "C" long noise_line_fit(const NoiseLineSums *sums, unsigned long long level_scale,
                               unsigned long long measurement_scale, NoiseLine *line, EngineError *error)
{
    if (error == NULL)
    {
        return NOISE_DETECTOR_ERROR;
    }
    if (!NOISE_DETECTOR_CHECK((sums != NULL) && (line != NULL) && (level_scale != 0ULL) && (measurement_scale != 0ULL),
                              sums, error, ENGINE_ERROR_REQUEST))
    {
        return NOISE_DETECTOR_ERROR;
    }
    OriorExactInteger left;
    OriorExactInteger right;
    OriorExactInteger scale;
    NoiseLine fitted;
    // N B - A^2 is positive exactly where the places span two levels, by Cauchy-Schwarz over the counts
    const int formed = (orior_exact_multiply(&sums->samples, &sums->along_square, &left) == ORIOR_EXACT_OK) &&
                       (orior_exact_multiply(&sums->along, &sums->along, &right) == ORIOR_EXACT_OK) &&
                       (orior_exact_subtract(&left, &right, &fitted.denominator) == ORIOR_EXACT_OK);
    const int spanned = (formed == 0) || (fitted.denominator.sign > 0);
    noise_exact_word(&scale, level_scale);
    int ok = formed && spanned && (orior_exact_multiply(&sums->samples, &sums->cross, &left) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&sums->along, &sums->measured, &right) == ORIOR_EXACT_OK) &&
             (orior_exact_subtract(&left, &right, &fitted.slope) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&fitted.slope, &scale, &fitted.slope) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&sums->measured, &sums->along_square, &left) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&sums->along, &sums->cross, &right) == ORIOR_EXACT_OK) &&
             (orior_exact_subtract(&left, &right, &fitted.intercept) == ORIOR_EXACT_OK);
    noise_exact_word(&scale, measurement_scale);
    ok = ok && (orior_exact_multiply(&fitted.denominator, &scale, &fitted.denominator) == ORIOR_EXACT_OK);
    if (!NOISE_DETECTOR_CHECK(spanned, sums, error, ENGINE_ERROR_REQUEST) ||
        !NOISE_DETECTOR_CHECK(ok, sums, error, ENGINE_ERROR_RESOURCE))
    {
        return NOISE_DETECTOR_ERROR;
    }
    *line = fitted;
    return 0L;
}

// sum gains left times right: the 64 by 64 bit product as four 32 by 32 bit parts, each below 2^64
static void noise_wide_add_product(NoiseWide *sum, unsigned long long left, unsigned long long right)
{
    const unsigned long long left_upper = left >> 32u;
    const unsigned long long left_lower = left & 0xFFFFFFFFULL;
    const unsigned long long right_upper = right >> 32u;
    const unsigned long long right_lower = right & 0xFFFFFFFFULL;
    const unsigned long long first = left_upper * right_lower;
    const unsigned long long second = left_lower * right_upper;
    noise_wide_add(sum, left_lower * right_lower, left_upper * right_upper);
    noise_wide_add(sum, first << 32u, first >> 32u);
    noise_wide_add(sum, second << 32u, second >> 32u);
}

// sum gains place times a signed reading
void noise_signed_wide_add(NoiseSignedWide *sum, unsigned long long place, long long measurement)
{
    // the magnitude of a negative word is its two's complement negation, taken unsigned
    const unsigned long long magnitude =
        (measurement < 0LL) ? (0ULL - (unsigned long long)measurement) : (unsigned long long)measurement;
    noise_wide_add_product((measurement < 0LL) ? &sum->lowered : &sum->raised, place, magnitude);
}

int noise_exact_signed_wide(OriorExactInteger *value, const NoiseSignedWide *wide)
{
    OriorExactInteger lowered;
    noise_exact_wide(value, &wide->raised);
    noise_exact_wide(&lowered, &wide->lowered);
    return orior_exact_subtract(value, &lowered, value) == ORIOR_EXACT_OK;
}
