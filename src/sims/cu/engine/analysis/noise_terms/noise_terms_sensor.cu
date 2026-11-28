// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// noise_terms_sensor.cu: static columns, ladder, crosstalk, charge and blur
#include "../../../../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "noise_terms_internal.h"

// The static voxels that the ladder reads, counted by column along x, where the scene of the ladder sets the level.
// This is the rule of the detector (noise_detector_lines.cu:155-234), run again on the host. A voxel is eligible where
// it changes, never touches either end of the lane, and passes the quiet test. The call keeps the dimmest quarter of
// those by their mean over the frames, rounded down. That is every voxel at or below the highest mean that the quarter
// reaches. The ceiling is that mean.
static int terms_static_columns(const unsigned short *lanes, unsigned long long columns[TERMS_WIDTH],
                                unsigned long long *ceiling)
{
    const unsigned long long voxels = TERMS_DEPTH * TERMS_HEIGHT * TERMS_WIDTH;
    unsigned long long *const totals = (unsigned long long *)platform_cell_cleared(
        (unsigned long long)((size_t)voxels), (unsigned long long)(sizeof(unsigned long long)),
        (unsigned int)platform_place_g_program_owner, 0);
    unsigned long long *const squares = (unsigned long long *)platform_cell_cleared(
        (unsigned long long)((size_t)voxels), (unsigned long long)(sizeof(unsigned long long)),
        (unsigned int)platform_place_g_program_owner, 0);
    unsigned short *const least = (unsigned short *)platform_cell_taken(
        (unsigned long long)((size_t)voxels * sizeof(unsigned short)), (unsigned int)platform_place_g_program_owner, 0);
    unsigned short *const max = (unsigned short *)platform_cell_cleared(
        (unsigned long long)((size_t)voxels), (unsigned long long)(sizeof(unsigned short)),
        (unsigned int)platform_place_g_program_owner, 0);
    unsigned char *const mask = (unsigned char *)platform_cell_taken((unsigned long long)((size_t)voxels),
                                                                     (unsigned int)platform_place_g_program_owner, 0);
    unsigned long long *const counted = (unsigned long long *)platform_cell_cleared(
        (unsigned long long)((size_t)TERMS_LANE_VALUES), (unsigned long long)(sizeof(unsigned long long)),
        (unsigned int)platform_place_g_program_owner, 0);
    const int ok = (totals != NULL) && (squares != NULL) && (least != NULL) && (max != NULL) && (mask != NULL) &&
                   (counted != NULL);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = columns, .bytes = (size_t)TERMS_WIDTH * sizeof(unsigned long long),
               .val = 0);
    *ceiling = 0ULL;
    if (ok != 0)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = least, .bytes = (size_t)voxels * sizeof(unsigned short), .val = 0xFF);
        for (unsigned long long frame = 0ULL; frame < TERMS_FRAMES; frame += 1ULL)
        {
            const unsigned short *const lane = &lanes[frame * voxels];
            for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
            {
                totals[voxel] += lane[voxel];
                least[voxel] = (lane[voxel] < least[voxel]) ? lane[voxel] : least[voxel];
                max[voxel] = (lane[voxel] > max[voxel]) ? lane[voxel] : max[voxel];
                if (frame != 0ULL)
                {
                    const unsigned short before = lanes[((frame - 1ULL) * voxels) + voxel];
                    const long long moved = (long long)lane[voxel] - (long long)before;
                    // a square is never negative. It re-signs to unsigned long long exactly
                    squares[voxel] += (unsigned long long)(moved * moved);
                }
            }
        }
        unsigned long long eligible = 0ULL;
        for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
        {
            const unsigned long long bound = (TERMS_QUIET_SLOPE * totals[voxel]) + (TERMS_QUIET_FLOOR * TERMS_FRAMES);
            const int still = (squares[voxel] * TERMS_FRAMES) <= ((TERMS_FRAMES - 1ULL) * bound);
            mask[voxel] =
                ((least[voxel] != 0u) && (max[voxel] != TERMS_LANE_TOP) && (least[voxel] != max[voxel]) && still) ? 1u
                                                                                                                  : 0u;
            if (mask[voxel] != 0u)
            {
                // a mean of values that are each below 2^16 is below 2^16
                counted[totals[voxel] / TERMS_FRAMES] += 1ULL;
                eligible += 1ULL;
            }
        }
        const unsigned long long wanted = (eligible + TERMS_STATIC_SHARE - 1ULL) / TERMS_STATIC_SHARE;
        unsigned long long running = 0ULL;
        for (unsigned long long mean = 0ULL; (mean < TERMS_LANE_VALUES) && (running < wanted); mean += 1ULL)
        {
            running += counted[mean];
            *ceiling = mean;
        }
        for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
        {
            if ((mask[voxel] != 0u) && ((totals[voxel] / TERMS_FRAMES) <= *ceiling))
            {
                columns[voxel % TERMS_WIDTH] += 1ULL;
            }
        }
    }
    platform_cell_released(totals, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(squares, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(least, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(max, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(mask, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(counted, (unsigned int)platform_place_g_program_owner);
    return ok;
}

// C15 and C19 read back (build plan item 38). The model is a Poisson shot at gain g over a ramp of levels, with offset
// O, read variance `r^2` and no fixed pattern. At a level `L = O + g S` the cumulants are `k2 = g (L - O) + r^2`,
// `k3 = g^2 (L - O)` and `k4 = g^3 (L - O) - r^2 / 2`.
//
// The slopes of the ladder are g, `g^2` and `g^3`. The k3 line crosses 0 at O, and the k2 line reads `r^2` there. A
// Poisson count satisfies both checks with equality. The sim prints them and checks neither.
//
// Each reach is the smallest fraction of the type that the check uses. It is fiftieths for s2, quarters for s3, tenths
// for `R^2`, and whole units for s4 and O. Each is at or above 5 standard errors of the measurement over the static
// blocks here. Those are the quiet static quarter of the view, 19 blocks of 5 frames to a voxel, over levels 22 to 84.
// The mask keeps 22 to 82.
//
// The standard errors, Derived, first order:
//
// - The shot cumulants 2 to 8 of a frame are `g^2 S`, `g^3 S`, `g^4 S`, 0, `-9 g^6 S`, `-49 g^7 S` and `-104 g^8 S`.
//   There are S electrons, and each adds 0, 1, 2 or 4 at chances 9, 8, 6 and 1 in 24.
// - The cumulants of the read are `r^2`, 0, `-r^2 / 2`, 0, `r^2`, 0 and `-17 r^2 / 4`.
// - A block of n spreads its k-statistics by `var k2 = k4 / n + 2 k2^2 / (n - 1)` and
//   `cov(k2, k3) = k5 / n + 6 k2 k3 / (n - 1)`.
// - The third is `var k3 = k6 / n + 9 k2 k4 / (n - 1) + 9 k3^2 / (n - 1) + 6 n k2^3 / ((n - 1)(n - 2))`.
// - The fourth is `var k4 = k8 / n + (16 k2 k6 + 48 k3 k5 + 34 k4^2) / (n - 1)`
//   `+ (72 k2^2 k4 + 144 k2 k3^2) n / ((n - 1)(n - 2))` `+ 24 n (n + 1) k2^4 / ((n - 1)(n - 2)(n - 3))`.
// - The sources are `R. A. Fisher`, "Moments and product moments of sampling distributions", Proceedings of the
//   London Mathematical Society 30, 1929, and `M. G. Kendall` and `A. Stuart`, The Advanced Theory of Statistics,
//   volume 1.
//
// The static mask keeps the dimmest quarter of the eligible voxels by their mean over the frames. The quiet test
// leaves out part of the bright columns, and the quarter ends at a mean of 77, inside column 28 (level 78). The sim
// prints `n_c`, the voxels kept in column c, for c = 0 to 30, 116456 in all. It checks that their 19 blocks each are
// the blocks of the ladder. Column c holds `N_c = 19 n_c` blocks at `L_c = 22 + 2 c`, with `S = c + 1`. Then
// `N = sum N_c`, and the mean level of the blocks is `Lbar = 49.46`.
//
// The variance of a slope is `sum N_c (L_c - Lbar)^2 var k_j(S_c) / (sum N_c (L_c - Lbar)^2)^2`. The least-squares
// weight at O on a block of column c is `w_c = 1 / N + (O - Lbar)(L_c - Lbar) / sum N_c (L_c - Lbar)^2`. The error of
// O is the error of the k3 line at O over `g^2`, and `var O = sum N_c w_c^2 var k3(S_c) / g^4`. Then `R^2 - r^2` is
// about `e2(O) - e3(O) / g`, the errors of the two lines at O. The share of a block is
// `v(S) = var k2 - 2 cov(k2, k3) / g + var k3 / g^2`. That is `40 (S + 1)^3 + 32 S^2 - 26.1 S + 3.3` at g = 2 and
// `r^2 = 4`.
//
// Over the printed counts, the standard errors and the tolerances are:
//
// - `SE(s2) = 0.00225`, and the tolerance, 1 / 50, is 8.90 of them
// - `SE(s3) = 0.0526`, and 2 / 4 is 9.51
// - `SE(s4) = 1.93`, and 10 is 5.18
// - `SE(O) = 0.248`, and 2 is 8.05
// - `SE(R^2) = 0.487`, and 25 / 10 is 5.13
//
// 0 sits `4 / 0.487`, about 8.2 of them, from `r^2 = 4`. A measurement of 0 falls 3.08 of them past the edge of the
// tolerance. The check shows the read noise present at that strength, short of 5. No tolerance gives 5 on both sides
// with 8.2 between them.
//
// s2 reads 6.6 of its SE below g. The noise of x, `var k2 / 90`, attenuates the slopes by 0.9974, Derived, 2.3 SE of
// it. The other 4.3 SE has the sign of the cut of the quiet test, and the tolerance of 1/50 covers it.
//
// The derivation neglects these: x as fixed, and the place noise that the blocks of one voxel share. It also neglects
// the quiet test and the cut by mean in columns 15 to 30, as unrelated to the blocks that they keep.
void terms_ladder(SimResults *results, const unsigned short *lanes)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    NoiseLadderMeasurement measurement;
    const int ok = noise_ladder_volume(lanes, TERMS_EXTENT, &measurement, &error) == 0L;
    sim_check(results, ok, "the ladder read the volume");
    if (ok == 0)
    {
        return;
    }
    VerbaLinea *const line = &results->line;
    mmgr_line_put(line, "    ");
    mmgr_line_decimal(line, measurement.blocks, 1u);
    mmgr_line_put(line, " blocks retained, ");
    mmgr_line_decimal(line, measurement.left_out, 1u);
    mmgr_line_put(line, " left out; s3 >= s2^2 ");
    mmgr_line_put(line, (measurement.rising != 0) ? "passes" : "fails");
    mmgr_line_put(line, ", s2 s4 >= s3^2 ");
    mmgr_line_put(line, (measurement.convex != 0) ? "passes" : "fails");
    mmgr_line_ch(line, '\n');
    unsigned long long columns[TERMS_WIDTH];
    unsigned long long ceiling = 0ULL;
    const int copied = terms_static_columns(lanes, columns, &ceiling);
    sim_check(results, copied, "the static mask's copy");
    unsigned long long members = 0ULL;
    unsigned long long last = 0ULL;
    for (unsigned long long x = 0ULL; copied && (x < TERMS_WIDTH); x += 1ULL)
    {
        members += columns[x];
        last = (columns[x] != 0ULL) ? x : last;
    }
    mmgr_line_put(line, "    the static voxels by column along x from column 0, every mean no more than ");
    mmgr_line_decimal(line, ceiling, 1u);
    mmgr_line_ch(line, ':');
    for (unsigned long long x = 0ULL; copied && (x <= last); x += 1ULL)
    {
        mmgr_line_ch(line, ' ');
        mmgr_line_decimal(line, columns[x], 1u);
    }
    mmgr_line_ch(line, '\n');
    sim_check(results,
              copied &&
                  ((members * (TERMS_FRAMES / TERMS_MOMENT_BLOCK)) == (measurement.blocks + measurement.left_out)),
              "the copied mask's voxels contain the ladder's blocks, retained and left out");
    const long long gain = TERMS_LADDER_GAIN;
    terms_ratio_within(results, "s2, the k2 line's slope", &measurement.cumulant[0].slope,
                       &measurement.cumulant[0].denominator, gain, 1ULL, 1ULL, 50ULL);
    terms_ratio_within(results, "s3, the k3 line's slope", &measurement.cumulant[1].slope,
                       &measurement.cumulant[1].denominator, gain * gain, 1ULL, 2ULL, 4ULL);
    terms_ratio_within(results, "s4, the k4 line's slope", &measurement.cumulant[2].slope,
                       &measurement.cumulant[2].denominator, gain * gain * gain, 1ULL, 10ULL, 1ULL);
    sim_check(results, measurement.tail != 0, "the k3 line has a slope, so the tail reads");
    if (measurement.tail == 0)
    {
        return;
    }
    // the offset and the read variance are each far below 2^31
    terms_ratio_within(results, "O, where the k3 line crosses 0", &measurement.offset, &measurement.offset_denominator,
                       (long long)TERMS_OFFSET, 1ULL, 2ULL, 1ULL);
    terms_ratio_within(results, "R^2, the k2 line at O", &measurement.read_square, &measurement.read_square_denominator,
                       (long long)TERMS_READ_SQUARE, 1ULL, 25ULL, 10ULL);
}

// crosstalk along x. Every voxel takes `alpha = TERMS_CROSSTALK_EIGHTHS / 8` of the value of each x neighbor after
// the draw, rounded to the nearest. The call reads from a copy, so that each voxel mixes values that no mix changed
// yet. The first and last columns take their one neighbor
void terms_crosstalk_mix(unsigned short *lanes, const unsigned short *drawn)
{
    const unsigned long long rows = TERMS_FRAMES * TERMS_DEPTH * TERMS_HEIGHT;
    for (unsigned long long row = 0ULL; row < rows; row += 1ULL)
    {
        const unsigned short *const from = &drawn[row * TERMS_WIDTH];
        unsigned short *const into = &lanes[row * TERMS_WIDTH];
        for (unsigned long long x = 0ULL; x < TERMS_WIDTH; x += 1ULL)
        {
            const unsigned long long left = (x != 0ULL) ? from[x - 1ULL] : 0ULL;
            const unsigned long long right = ((x + 1ULL) < TERMS_WIDTH) ? from[x + 1ULL] : 0ULL;
            const unsigned long long mixed = from[x] + (((TERMS_CROSSTALK_EIGHTHS * (left + right)) + 4ULL) / 8ULL);
            // the scene's lanes are near 120. A lane plus a quarter of two more stays far below 65536
            into[x] = (unsigned short)mixed;
        }
    }
}

// C20 read back (build plan item 38). Along x, a draw that alpha shares with each neighbor gives
// `alpha = C1 / (2 V)`, `alpha^2 = C2 / V` and `C3 / V = 0`. Along y, the draw shares nothing. All three read 0.
//
// The products are M products of frame differences, and each difference shares a frame with the next. Over them, the
// `C1 / (2 V)` of the null has a standard error of `sqrt(3/2) / (2 sqrt(M))`, and `C2 / V` and `C3 / V` twice that.
// At `M = 95 x 32 x 128 x 125`, that is about 1/11000 and 1/5700. Each tolerance is about 11 of them, room for the
// larger ones of the mix.
void terms_crosstalk(SimResults *results, const unsigned short *lanes, unsigned long long along_x_eighths)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    NoiseCrosstalkMeasurement measurement;
    const int ok = noise_crosstalk_volume(lanes, TERMS_EXTENT, &measurement, &error) == 0L;
    sim_check(results, ok, "the crosstalk pass read the volume");
    if (ok == 0)
    {
        return;
    }
    const char *const axis_names[NOISE_CROSSTALK_AXES] = {"along y", "along x"};
    for (unsigned int axis = 0u; axis < NOISE_CROSSTALK_AXES; axis += 1u)
    {
        // the planted share is a few eighths, far below 2^31
        const long long eighths = (axis == 1u) ? (long long)along_x_eighths : 0LL;
        mmgr_line_put(&results->line, "    ");
        mmgr_line_put(&results->line, axis_names[axis]);
        mmgr_line_put(&results->line, ", over ");
        mmgr_line_decimal(&results->line, measurement.pairs[axis], 1u);
        mmgr_line_put(&results->line, " frame differences\n");
        OriorExactInteger twice;
        const int doubled = sim_exact_scaled(&measurement.spread[axis], 2ULL, &twice);
        sim_check(results, doubled && (measurement.spread[axis].sign > 0), "V is positive");
        if ((doubled == 0) || (measurement.spread[axis].sign <= 0))
        {
            continue;
        }
        // no share reads as 0, not 0/8
        const unsigned long long eighth = (eighths != 0LL) ? 8ULL : 1ULL;
        terms_ratio_within(results, "  alpha, C1 / (2 V)", &measurement.steps[axis][0], &twice, eighths, eighth, 1ULL,
                           1000ULL);
        terms_ratio_within(results, "  alpha^2, C2 / V", &measurement.steps[axis][1], &measurement.spread[axis],
                           eighths * eighths, eighth * eighth, 1ULL, 500ULL);
        terms_ratio_within(results, "  C3 / V", &measurement.steps[axis][2], &measurement.spread[axis], 0LL, 1ULL, 1ULL,
                           500ULL);
    }
}

// C14 read back (build plan item 38). A bias series and a dark series, with no light:
//
// - the bias mean is O
// - the bias `E[d^2] / 2` is `R^2 + kTC`
// - the dark mean less the bias mean is `g D dt`
// - the dark `E[d^2] / 2` less that of the bias is `g^2 D dt`
// - their ratio is g
//
// The run has 96 frames of 32 x 128 x 128, with a Poisson dark shot. At 16 dark electrons, the standard errors are
// about 1/2500 (O), 1/500 (`R^2 + kTC`) and 1/800 (`g D dt`). They are 1/55 (`g^2 D dt`) and 1/1700 (g). At 32 dark
// electrons they are about 1/600, 1/30 and 1/1900 for the last three. Each tolerance is at least 5 of them.
void terms_charge(SimResults *results, const unsigned short *bias, const unsigned short *dark, const SimCamera *camera)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    NoiseChargeMeasurement measurement;
    const int ok = noise_charge_series(bias, dark, TERMS_EXTENT, &measurement, &error) == 0L;
    sim_check(results, ok, "the charge pass read the series");
    if (ok == 0)
    {
        return;
    }
    // the offset, variances, gain and dark electrons are each far below 2^31
    const long long gain = (long long)camera->gain;
    const long long dark_electrons = (long long)camera->dark;
    terms_ratio_within(results, "O, the bias mean", &measurement.offset, &measurement.offset_denominator,
                       (long long)camera->offset, 1ULL, 1ULL, 500ULL);
    terms_ratio_within(results, "R^2 + kTC, the bias E[d^2] / 2", &measurement.level_free,
                       &measurement.level_free_denominator, (long long)(camera->read_square + camera->reset_square),
                       1ULL, 1ULL, 100ULL);
    terms_ratio_within(results, "g D dt, the dark mean less the bias's", &measurement.dark_level,
                       &measurement.dark_level_denominator, gain * dark_electrons, 1ULL, 1ULL, 100ULL);
    terms_ratio_within(results, "g^2 D dt, the dark E[d^2] / 2 less the bias's", &measurement.dark_square,
                       &measurement.dark_square_denominator, gain * gain * dark_electrons, 1ULL, 1ULL, 5ULL);
    sim_check(results, measurement.gain_read != 0, "the dark mean differs from the bias's, so g reads");
    if (measurement.gain_read != 0)
    {
        terms_ratio_within(results, "g, their ratio", &measurement.gain, &measurement.gain_denominator, gain, 1ULL,
                           1ULL, 300ULL);
    }
}

// C16 read back (build plan item 38). A lit series against the bias, as C14 reads a dark one. The lit mean less the
// bias mean is `g q S`, and the lit `E[d^2] / 2` less that of the bias is `g^2 f' q S`. Their ratio is `g f'`. A count
// of Fano factor f thinned by q satisfies `f' - 1 = q (f - 1)`. Where the excess follows the thinning, `f' = F^2 f` at
// every q.
//
// The standard error of the ratio is under 1/1000 in every run here. A tolerance of 1/200 is at least 5 of them.
void terms_thinned(SimResults *results, const unsigned short *bias, const unsigned short *lit, long long expected,
                   unsigned long long expected_denominator)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    NoiseChargeMeasurement measurement;
    const int ok = noise_charge_series(bias, lit, TERMS_EXTENT, &measurement, &error) == 0L;
    sim_check(results, ok && (measurement.gain_read != 0), "the charge pass read the lit series against the bias");
    if ((ok == 0) || (measurement.gain_read == 0))
    {
        return;
    }
    terms_ratio_within(results, "  g f', the lit ratio", &measurement.gain, &measurement.gain_denominator, expected,
                       expected_denominator, 1ULL, 200ULL);
}

// Row 16 read back. The sim blurs the light before the draw. The mean of each column over the frames, z and y is the
// offset plus the pattern column mean plus g times the blurred light. It is within 1/8 of a lane unit, 6 to 8 standard
// errors over its 393,216 voxel-frames at levels 120 to 184. The kernel moves the planted light by more than 1 at some
// place.
void terms_blur(SimResults *results, const unsigned short *lanes, const SimScene *scene, const SimCamera *camera)
{
    const unsigned long long plane = TERMS_HEIGHT * TERMS_WIDTH;
    const unsigned long long voxels = TERMS_DEPTH * plane;
    const unsigned long long samples = TERMS_FRAMES * TERMS_DEPTH * TERMS_HEIGHT;
    unsigned long long worst = 0ULL;
    unsigned long long moved = 0ULL;
    for (unsigned long long x = 0ULL; x < TERMS_WIDTH; x += 1ULL)
    {
        // a column index is far below 2^63
        const long long place[SIM_AXES] = {0LL, 0LL, (long long)x};
        const unsigned long long light = sim_light(scene, camera, 0ULL, place);
        const unsigned long long planted = sim_signal(scene, 0ULL, place);
        const unsigned long long shift = (light > planted) ? (light - planted) : (planted - light);
        moved = (shift > moved) ? shift : moved;
        unsigned long long pattern = 0ULL;
        unsigned long long total = 0ULL;
        for (unsigned long long z = 0ULL; z < TERMS_DEPTH; z += 1ULL)
        {
            for (unsigned long long y = 0ULL; y < TERMS_HEIGHT; y += 1ULL)
            {
                const unsigned long long voxel = (((z * TERMS_HEIGHT) + y) * TERMS_WIDTH) + x;
                pattern += sim_pattern(camera, voxel);
                for (unsigned long long frame = 0ULL; frame < TERMS_FRAMES; frame += 1ULL)
                {
                    total += lanes[(frame * voxels) + voxel];
                }
            }
        }
        const unsigned long long expected =
            (samples * (camera->offset + (camera->gain * light))) + (TERMS_FRAMES * pattern);
        const unsigned long long apart = (total > expected) ? (total - expected) : (expected - total);
        worst = (apart > worst) ? apart : worst;
    }
    mmgr_line_put(&results->line, "    the worst column's mean less the blurred light's: ");
    sim_fraction_print(&results->line, worst, samples, 4u);
    mmgr_line_put(&results->line, " lane units; the kernel moves the planted light by up to ");
    mmgr_line_decimal(&results->line, moved, 1u);
    mmgr_line_put(&results->line, " electrons\n");
    sim_check(results, (8ULL * worst) <= samples, "every column's mean is the blurred light's within 1/8");
    sim_check(results, moved > 1ULL, "the kernel moves the planted light by more than 1 somewhere");
}
