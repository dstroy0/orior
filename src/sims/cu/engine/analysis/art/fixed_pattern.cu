// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "periodic_energy.h"

#define PATTERN_HEIGHT 8ULL

#define PATTERN_WIDTH 8ULL

#define PATTERN_FRAME (PATTERN_HEIGHT * PATTERN_WIDTH)

#define PATTERN_FRAMES 48ULL

#define PATTERN_LENGTH (PATTERN_FRAME * PATTERN_FRAMES)

#define PATTERN_SWING 20LL

#define PATTERN_AMPLITUDE 40LL

#define PATTERN_DRAWS 8ULL

#define PATTERN_KEY 0xF17AULL

#define PATTERN_DEPTHS 4u

typedef struct
{
    long long scene[PATTERN_LENGTH];
    long long pattern[PATTERN_FRAME];
    long long stack[PATTERN_LENGTH];
    long long arm[PATTERN_LENGTH];
    long long shuffled[PATTERN_LENGTH];
    long long target[PATTERN_LENGTH];
    long long sums[(PATTERN_FRAME * (PATTERN_FRAME + 1ULL)) / 2ULL];
    long long welford_numerator[PATTERN_FRAME];
    unsigned long long welford_denominator[PATTERN_FRAME];
} PatternWork;

static long long pattern_floor_divide(long long numerator, long long denominator)
{
    const long long quotient = numerator / denominator;
    return ((numerator % denominator) != 0LL) && ((numerator < 0LL) != (denominator < 0LL)) ? (quotient - 1LL)
                                                                                            : quotient;
}

static void pattern_moving_scene(long long *scene, unsigned long long key)
{
    long long members[PATTERN_FRAMES];
    for (unsigned long long pixel = 0ULL; pixel < PATTERN_FRAME; pixel += 1ULL)
    {
        const unsigned long long half = PATTERN_FRAMES / 2ULL;
        for (unsigned long long step = 0ULL; step < half; step += 1ULL)
        {
            // a draw below the swing is a small non-negative integer
            members[step] = 1LL + (long long)sim_draw_below(key, (pixel * PATTERN_FRAMES) + step, PATTERN_SWING);
            members[step + half] = -members[step];
        }
        for (unsigned long long at = PATTERN_FRAMES - 1ULL; at > 0ULL; at -= 1ULL)
        {
            const unsigned long long other =
                sim_draw_below(key ^ 0x5348554FULL, (pixel * PATTERN_FRAMES) + at, at + 1ULL);
            const long long temporary = members[at];
            members[at] = members[other];
            members[other] = temporary;
        }
        for (unsigned long long step = 0ULL; step < PATTERN_FRAMES; step += 1ULL)
        {
            scene[pixel + (step * PATTERN_FRAME)] = members[step];
        }
    }
}

static void pattern_fixed(long long *pattern, unsigned long long key)
{
    long long total = 0LL;
    for (unsigned long long pixel = 0ULL; pixel < PATTERN_FRAME; pixel += 1ULL)
    {
        // the draw is below 2 A + 1, a small non-negative integer
        pattern[pixel] =
            (long long)sim_draw_below(key ^ 0x5A5AULL, pixel, (2ULL * PATTERN_AMPLITUDE) + 1ULL) - PATTERN_AMPLITUDE;
        total += pattern[pixel];
    }
    // the frame is 64, far below 2^63
    const long long shift = pattern_floor_divide(total, (long long)PATTERN_FRAME);
    for (unsigned long long pixel = 0ULL; pixel < PATTERN_FRAME; pixel += 1ULL)
    {
        pattern[pixel] -= shift;
    }
}

static void pattern_impulses(long long *values, unsigned long long count, unsigned long long key)
{
    for (unsigned long long impulse = 0ULL; impulse < count; impulse += 1ULL)
    {
        const unsigned long long at = sim_draw_below(key, 2ULL * impulse, PATTERN_LENGTH);
        // the draw is below 4 swing + 1, a small non-negative integer
        values[at] = (long long)sim_draw_below(key, (2ULL * impulse) + 1ULL, (4ULL * PATTERN_SWING) + 1ULL) -
                     (2LL * PATTERN_SWING);
    }
}

static void pattern_print_share(VerbaLinea *line, const OriorExactInteger *numerator,
                                const OriorExactInteger *denominator)
{
    OriorExactInteger percent;
    if (sim_exact_scaled(numerator, 100ULL, &percent) != 0)
    {
        sim_ratio_print(line, &percent, denominator, 4u);
        mmgr_line_ch(line, '%');
    }
}

static int pattern_remove(SimResults *results, PatternWork *work, const long long *noisy, const long long *target,
                          unsigned long long period, OriorExactInteger *numerator, OriorExactInteger *denominator)
{
    EnergyMeasurement unused;
    if (energy_recover(results, noisy, PATTERN_LENGTH, period, work->sums, &unused) == 0)
    {
        return 0;
    }
    return energy_reduction(noisy, target, PATTERN_LENGTH, period, &work->sums[energy_phase_base(period)], numerator,
                            denominator);
}

int main(void)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    PatternWork *const work = (PatternWork *)calloc(1u, sizeof(PatternWork));
    sim_check(&results, work != NULL, "work buffers");
    if (work == NULL)
    {
        return sim_close(&results, "fixed pattern");
    }
    if (!sim_job_submit(&results, "fixed_pattern", 0, NULL, sizeof(PatternWork)))
    {
        free(work);
        return sim_close(&results, "fixed pattern");
    }
    VerbaLinea *const line = &results.line;
    pattern_moving_scene(work->scene, PATTERN_KEY);
    pattern_fixed(work->pattern, PATTERN_KEY);
    for (unsigned long long at = 0ULL; at < PATTERN_LENGTH; at += 1ULL)
    {
        work->stack[at] = work->scene[at] + work->pattern[at % PATTERN_FRAME];
    }
    mmgr_line_put(line, "  fixed-pattern video noise removed to the bit (ART-4-005, ported)\n  declared inputs: "
                         "frame=8x8 frames=48 swing=20 pattern=40 draws=8 key=0x");
    mmgr_line_hex(line, PATTERN_KEY, 1u);
    mmgr_line_put(line, "\n\n");

    EnergyMeasurement live;
    int ok = energy_recover(&results, work->stack, PATTERN_LENGTH, PATTERN_FRAME, work->sums, &live);
    energy_shuffle(work->stack, work->shuffled, PATTERN_LENGTH, sim_draw(PATTERN_KEY, 0x4445414FULL));
    OriorExactInteger dead_numerator;
    OriorExactInteger dead_denominator;
    int dead = 0;
    if (ok && live.found)
    {
        EnergyMeasurement ignored;
        ok = energy_recover(&results, work->shuffled, PATTERN_LENGTH, live.period, work->sums, &ignored);
        dead = ok && energy_ratio(work->shuffled, PATTERN_LENGTH, work->sums, live.period, &dead_numerator,
                                  &dead_denominator);
    }
    mmgr_line_put(line, "  identify: detector period ");
    mmgr_line_decimal(line, live.period, 1u);
    mmgr_line_put(line, " (frame size 64)\n  identify: dispersion ratio live ");
    energy_print(line, &live);
    mmgr_line_put(line, " against its own shuffle ");
    if (dead)
    {
        sim_ratio_print(line, &dead_numerator, &dead_denominator, 3u);
    }
    else
    {
        mmgr_line_put(line, "none");
    }
    mmgr_line_ch(line, '\n');
    sim_check(&results, ok && live.found && (live.period == PATTERN_FRAME), "the detector reads the frame period");

    ok = ok && (energy_recover(&results, work->stack, PATTERN_LENGTH, PATTERN_FRAME, work->sums, &live) != 0);
    ok = ok && energy_welford(&results, work->stack, PATTERN_LENGTH, PATTERN_FRAME, work->welford_numerator,
                              work->welford_denominator);
    const long long *const batch = &work->sums[energy_phase_base(PATTERN_FRAME)];
    unsigned long long agree = 0ULL;
    unsigned long long broken_from_batch = 0ULL;
    unsigned long long broken_from_welford = 0ULL;
    unsigned long long exact = 0ULL;
    for (unsigned long long phase = 0ULL; ok && (phase < PATTERN_FRAME); phase += 1ULL)
    {
        // a phase's member count and the Welford denominator are both below 2^16
        const long long members = (long long)energy_members(PATTERN_LENGTH, PATTERN_FRAME, phase);
        const long long denominator = (long long)work->welford_denominator[phase];
        agree += ((batch[phase] * denominator) == (work->welford_numerator[phase] * members)) ? 1ULL : 0ULL;
        broken_from_batch += ((batch[phase] * members) != (batch[phase] * (members + 1LL))) ? 1ULL : 0ULL;
        broken_from_welford +=
            ((batch[phase] * denominator) != (work->welford_numerator[phase] * (members + 1LL))) ? 1ULL : 0ULL;
    }
    for (unsigned long long at = 0ULL; ok && (at < PATTERN_LENGTH); at += 1ULL)
    {
        // the member count is below 2^16
        const long long members = (long long)energy_members(PATTERN_LENGTH, PATTERN_FRAME, at % PATTERN_FRAME);
        exact +=
            (((members * work->stack[at]) - batch[at % PATTERN_FRAME]) == (members * work->scene[at])) ? 1ULL : 0ULL;
    }
    OriorExactInteger share_numerator;
    OriorExactInteger share_denominator;
    ok = ok &&
         pattern_remove(&results, work, work->stack, work->scene, PATTERN_FRAME, &share_numerator, &share_denominator);
    mmgr_line_put(line, "  reject: batch and incremental (Welford) routes agree on ");
    mmgr_line_decimal(line, agree, 1u);
    mmgr_line_put(line, " of 64 phases\n  reject: the broken route splits from both on ");
    mmgr_line_decimal(line, broken_from_batch, 1u);
    mmgr_line_put(line, " and ");
    mmgr_line_decimal(line, broken_from_welford, 1u);
    mmgr_line_put(line, " phases\n  reject: residual equals the moving scene on ");
    mmgr_line_decimal(line, exact, 1u);
    mmgr_line_put(line, " of ");
    mmgr_line_decimal(line, PATTERN_LENGTH, 1u);
    mmgr_line_put(line, " values\n  reject: noise reduction ");
    pattern_print_share(line, &share_numerator, &share_denominator);
    mmgr_line_put(line, "\n\n");
    sim_check(&results, agree == PATTERN_FRAME, "batch and incremental routes agree bit-exact");
    sim_check(&results, (broken_from_batch != 0ULL) && (broken_from_welford != 0ULL),
              "the broken route splits from both");
    sim_check(&results, exact == PATTERN_LENGTH, "the residual equals the moving scene as integers");
    sim_check(&results, ok && (orior_exact_compare(&share_numerator, &share_denominator) == 0),
              "the noise reduction is exactly 1");

    EnergyMeasurement null_measurement;
    ok = ok && energy_recover(&results, work->shuffled, PATTERN_LENGTH, PATTERN_FRAME, work->sums, &null_measurement);
    OriorExactInteger wrong_numerator;
    OriorExactInteger wrong_denominator;
    ok = ok && pattern_remove(&results, work, work->stack, work->scene, PATTERN_FRAME - 1ULL, &wrong_numerator,
                              &wrong_denominator);
    mmgr_line_put(line, "  null: the shuffled stack reads period ");
    mmgr_line_decimal(line, null_measurement.period, 1u);
    mmgr_line_put(line, " at ");
    energy_print(line, &null_measurement);
    mmgr_line_put(line, "\n  null: rejecting at the wrong period 63 reduces the noise by ");
    pattern_print_share(line, &wrong_numerator, &wrong_denominator);
    mmgr_line_put(line, "\n\n");
    sim_flush(&results);

    EnergyMeasurement top;
    unsigned long long reached = 0ULL;
    ok = ok && energy_band_top(&results, work->stack, PATTERN_LENGTH, PATTERN_FRAME, PATTERN_DRAWS,
                               PATTERN_KEY ^ 0x42414E44ULL, work->shuffled, work->sums, &top, &reached);
    mmgr_line_put(line, "  negative controls: the 100% is licensed by the band declining the wrong noise\n  null band "
                         "over 8 shuffles (");
    mmgr_line_decimal(line, reached, 1u);
    mmgr_line_put(line, " reached a period), top ");
    energy_print(line, &top);
    mmgr_line_put(line, "\n  case                      live ratio   above band  outcome\n");
    const char *const case_name[3] = {"matched pattern         ", "no pattern              ",
                                      "wrong type (impulses)   "};
    for (unsigned int arm = 0u; ok && (arm < 3u); arm += 1u)
    {
        for (unsigned long long at = 0ULL; at < PATTERN_LENGTH; at += 1ULL)
        {
            work->arm[at] = (arm == 0u) ? work->stack[at] : work->scene[at];
        }
        if (arm == 2u)
        {
            pattern_impulses(work->arm, PATTERN_LENGTH / 12ULL, PATTERN_KEY ^ 0x494D50ULL);
        }
        EnergyMeasurement seen;
        ok = energy_recover(&results, work->arm, PATTERN_LENGTH, PATTERN_FRAME, work->sums, &seen);
        const int present = ok && energy_above(&seen, &top);
        mmgr_line_put(line, "  ");
        mmgr_line_put(line, case_name[arm]);
        energy_print(line, &seen);
        mmgr_line_put(line, present ? "   yes         " : "   no          ");
        if (present)
        {
            OriorExactInteger got_numerator;
            OriorExactInteger got_denominator;
            ok = pattern_remove(&results, work, work->arm, work->scene, seen.period, &got_numerator, &got_denominator);
            mmgr_line_put(line, "remove, reduction ");
            pattern_print_share(line, &got_numerator, &got_denominator);
        }
        else if (arm == 1u)
        {
            mmgr_line_put(line, "decline, returned untouched");
        }
        else
        {
            mmgr_line_put(line, "decline, the noise left intact (nothing removed)");
        }
        mmgr_line_ch(line, '\n');
        sim_check(&results, present == (arm == 0u), "only the matched pattern clears the band");
    }
    mmgr_line_ch(line, '\n');
    sim_flush(&results);

    const long long depth[PATTERN_DEPTHS] = {0LL, 5LL, 15LL, 30LL};
    mmgr_line_put(
        line, "  floor: a static scene feature cannot be distinguished from a fixed pattern\n  depth   reduction\n");
    int falling = 1;
    OriorExactInteger last_numerator;
    OriorExactInteger last_denominator;
    for (unsigned int level = 0u; ok && (level < PATTERN_DEPTHS); level += 1u)
    {
        for (unsigned long long at = 0ULL; at < PATTERN_LENGTH; at += 1ULL)
        {
            work->target[at] = work->scene[at] + (((at % PATTERN_FRAME) == 0ULL) ? depth[level] : 0LL);
            work->arm[at] = work->target[at] + work->pattern[at % PATTERN_FRAME];
        }
        OriorExactInteger floor_numerator;
        OriorExactInteger floor_denominator;
        ok = pattern_remove(&results, work, work->arm, work->target, PATTERN_FRAME, &floor_numerator,
                            &floor_denominator);
        mmgr_line_put(line, "  ");
        // every depth is a small non-negative integer
        mmgr_line_decimal_columns(line, (unsigned long long)depth[level], 5u);
        mmgr_line_put(line, "   ");
        pattern_print_share(line, &floor_numerator, &floor_denominator);
        mmgr_line_put(line, (level == 0u) ? "  full rejection\n" : "  a static feature, removed with the pattern\n");
        if (level > 0u)
        {
            int order = 0;
            falling =
                falling &&
                sim_ratio_compare(&floor_numerator, &floor_denominator, &last_numerator, &last_denominator, &order) &&
                (order < 0);
        }
        last_numerator = floor_numerator;
        last_denominator = floor_denominator;
    }
    sim_check(&results, ok && falling, "the reduction falls with every deeper static feature");
    free(work);
    return sim_close(&results, "fixed pattern");
}
