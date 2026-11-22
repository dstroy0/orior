// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-031
/**
 * @file exact_zeta_tension.c
 * @brief The tension of the field, read over the zeros that the machine of Turing certifies.
 *
 * @note zeta_tension.c places the zeros. Run build_exact_zeta.sh tension <binary> <first> <last> [extra]
 *       [N at the F of first] [N at the F of last], beside this file.
 * @note On theta / pi, u = theta / pi + 1, each zero at its u. S between zeros is n - u at the midpoint of the n-th
 *       and the next. The report gives its mean, and its variance against the leading term of Selberg,
 *       (1 / (2 pi^2)) ln ln(t / 2 pi).
 * @note It gives the pull of S back to 0: the slope of each step of S on S.
 * @note It gives the correlation of S over k zeros, k to 60, against the waves of S. Those waves are
 *       S = -(1 / pi) times the sum over prime powers p^r of sin(r t ln p) / (r p^(r / 2)).
 * @note Take the phases as independent. Then the correlation of the waves at a lag tau in t is a ratio of two
 *       sums. The top is the sum of cos(r tau ln p) / (r^2 p^r), and the bottom is the sum of 1 / (r^2 p^r).
 * @note It gives the number variance of the zeros over windows of L. That is against the variance of the GUE,
 *       (1 / pi^2)(ln 2 pi L + gamma + 1 - pi^2 / 8), and that of Poisson, L. It is also against the same spacings
 *       shuffled, which keep every gap and drop how the gaps hold to one another.
 * @note The statistics are those of the host, in floating point, over the Z of the device. It sits in
 *       0_experimental, and is an entry in the workbook of analytic number theory, on its rail. It proves nothing.
 *       It reads how the zeros hold to one another.
 */
#include "zeta_shuffle.h"
#include "../../../src/platform/platform.h"
#include "zeta_tension.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The constant of Euler. */
#define EULER 0.5772156649015329

/** @brief The lags of the correlation of S. */
#define LAGS 60

/** @brief The seed of the shuffle and of the windows. */
#define TENSION_SEED 20261004u

/** @brief Pi as a double, as math.pi holds it. */
#define PI_DOUBLE 3.141592653589793

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *analysis_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief The correlation of a series with itself at a lag, over the variance.
 *
 * @param[in] series The series [BORROWS].
 * @param[in] count  How many.
 * @param[in] lag    The lag.
 * @return           The correlation.
 */
static double autocorrelation(const double *series, size_t count, size_t lag)
{
    double mean = 0.0;
    for (size_t at = 0u; at < count; at += 1u)
    {
        mean += series[at];
    }
    mean /= (double)count;
    double variance = 0.0;
    for (size_t at = 0u; at < count; at += 1u)
    {
        variance += (series[at] - mean) * (series[at] - mean);
    }
    variance /= (double)count;
    double total = 0.0;
    for (size_t at = 0u; (at + lag) < count; at += 1u)
    {
        total += (series[at] - mean) * (series[at + lag] - mean);
    }
    // The span past the lag may be zero or less, as in Python.
    const double span = (double)count - (double)lag;
    return total / (span * variance);
}

/**
 * @brief The slope of the next step of S on S.
 *
 * @note The slope is 0 for a walk with no pull back, and -1 for a pull all the way to 0 each step.
 *
 * @param[in] series The series [BORROWS].
 * @param[in] count  How many.
 * @return           The slope.
 */
static double pull(const double *series, size_t count)
{
    double mean = 0.0;
    for (size_t at = 0u; at < count; at += 1u)
    {
        mean += series[at];
    }
    mean /= (double)count;
    double top = 0.0;
    double bottom = 0.0;
    for (size_t at = 0u; (at + 1u) < count; at += 1u)
    {
        const double x = series[at] - mean;
        const double y = series[at + 1u] - series[at];
        top += x * y;
        bottom += x * x;
    }
    return top / bottom;
}

/**
 * @brief The place of the first value past x in an ascending list, as bisect_right finds it.
 *
 * @param[in] values The list [BORROWS].
 * @param[in] count  How many.
 * @param[in] x      The value.
 * @return           The place.
 */
static size_t bisect_right(const double *values, size_t count, double x)
{
    size_t low = 0u;
    size_t high = count;
    while (low < high)
    {
        const size_t middle = (low + high) / 2u;
        if (x < values[middle])
        {
            high = middle;
        }
        else
        {
            low = middle + 1u;
        }
    }
    return low;
}

/**
 * @brief The number variance of the positions over windows of a length, at random windows.
 *
 * @param[in,out] twister   The generator [BORROWS].
 * @param[in]     positions The positions, ascending [BORROWS].
 * @param[in]     count     How many.
 * @param[in]     length    The length.
 * @param[in]     samples   The windows.
 * @return                  The variance.
 */
static double number_variance(ZetaTwister *twister, const double *positions, size_t count, double length,
                              size_t samples)
{
    const double low = positions[0];
    const double high = positions[count - 1u] - length;
    int64_t *const counts =
        (int64_t *)analysis_memory(platform_cell_taken(samples * sizeof(int64_t), platform_place_g_program_owner, 0));
    int64_t total = 0;
    for (size_t at = 0u; at < samples; at += 1u)
    {
        const double x = low + ((high - low) * twister_random(twister));
        // The counts of zeros fit a machine integer.
        counts[at] = (int64_t)bisect_right(positions, count, x + length) - (int64_t)bisect_right(positions, count, x);
        total += counts[at];
    }
    const double mean = (double)total / (double)samples;
    double variance = 0.0;
    for (size_t at = 0u; at < samples; at += 1u)
    {
        const double gap = (double)counts[at] - mean;
        variance += gap * gap;
    }
    platform_cell_released(counts, platform_place_g_program_owner);
    return variance / (double)samples;
}

/**
 * @brief The readings over the zeros placed.
 *
 * @param[in] zeros The zeros [BORROWS].
 * @return          The exit code.
 */
static int analyse(const TensionZeros *zeros)
{
    const size_t count = zeros->count;
    const double *const s_values = zeros->zeros;
    // The count of zeros fits a machine integer.
    const int64_t n_end = zeros->n_start + (int64_t)count;
    double *const u =
        (double *)analysis_memory(platform_cell_taken(count * sizeof(double), platform_place_g_program_owner, 0));
    for (size_t at = 0u; at < count; at += 1u)
    {
        u[at] = tension_theta_pi(s_values[at]) + 1.0;
    }
    const double s_low = s_values[0];
    const double s_high = s_values[count - 1u];
    const double spacing = (2.0 * PI_DOUBLE * (s_high - s_low)) / (double)(count - 1u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  from cell ");
        mmgr_line_decimal_signed(output_line, (long long)zeros->start_nu);
        mmgr_line_put(output_line, "'s F, N = ");
        mmgr_line_decimal_signed(output_line, (long long)zeros->n_start);
        mmgr_line_put(output_line, ", to cell ");
        mmgr_line_decimal_signed(output_line, (long long)zeros->end_nu);
        mmgr_line_put(output_line, "'s, N = ");
        mmgr_line_decimal_signed(output_line, (long long)n_end);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, (unsigned long long)count, 1u);
        mmgr_line_put(output_line, " zeros, t from ");
        mmgr_line_fixed(output_line, (double)(2.0 * PI_DOUBLE * s_low), 1u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " to ");
        mmgr_line_fixed(output_line, (double)(2.0 * PI_DOUBLE * s_high), 1u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", ln(t / 2 pi) ");
        mmgr_line_fixed(output_line, (double)(log(0.5 * (s_low + s_high))), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", mean spacing in t ");
        mmgr_line_fixed(output_line, (double)(spacing), 4u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const size_t s_count = count - 1u;
    double *const s_series =
        (double *)analysis_memory(platform_cell_taken(s_count * sizeof(double), platform_place_g_program_owner, 0));
    double mean = 0.0;
    double least = 0.0;
    double most = 0.0;
    for (size_t k = 0u; k < s_count; k += 1u)
    {
        // k fits a machine integer.
        s_series[k] = (double)(zeros->n_start + (int64_t)k + 1) - (0.5 * (u[k] + u[k + 1u]));
        mean += s_series[k];
        least = ((k == 0u) || (s_series[k] < least)) ? s_series[k] : least;
        most = ((k == 0u) || (s_series[k] > most)) ? s_series[k] : most;
    }
    mean /= (double)s_count;
    double variance = 0.0;
    for (size_t k = 0u; k < s_count; k += 1u)
    {
        variance += (s_series[k] - mean) * (s_series[k] - mean);
    }
    variance /= (double)s_count;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  S between zeros: mean ");
        mmgr_line_fixed(output_line, (double)(mean), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", variance ");
        mmgr_line_fixed(output_line, (double)(variance), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", least ");
        mmgr_line_fixed(output_line, (double)(least), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", max ");
        mmgr_line_fixed(output_line, (double)(most), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, "; Selberg's leading term ");
        mmgr_line_fixed(output_line, (double)(log(log(0.5 * (s_low + s_high))) / (2.0 * (PI_DOUBLE * PI_DOUBLE))), 4u,
                        0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    ZetaTwister twister;
    twister_seed(&twister, TENSION_SEED);
    double *const gaps =
        (double *)analysis_memory(platform_cell_taken(s_count * sizeof(double), platform_place_g_program_owner, 0));
    double *const shuffled =
        (double *)analysis_memory(platform_cell_taken(s_count * sizeof(double), platform_place_g_program_owner, 0));
    for (size_t k = 0u; k < s_count; k += 1u)
    {
        gaps[k] = u[k + 1u] - u[k];
        shuffled[k] = gaps[k];
    }
    for (size_t at = s_count - 1u; (s_count > 1u) && (at > 0u); at -= 1u)
    {
        // The count of gaps is small, and the index fits a uint32_t.
        const size_t other = twister_below(&twister, (uint32_t)(at + 1u));
        const double held = shuffled[at];
        shuffled[at] = shuffled[other];
        shuffled[other] = held;
    }
    double *const u_shuffled =
        (double *)analysis_memory(platform_cell_taken(count * sizeof(double), platform_place_g_program_owner, 0));
    u_shuffled[0] = u[0];
    for (size_t k = 0u; k < s_count; k += 1u)
    {
        u_shuffled[k + 1u] = u_shuffled[k] + shuffled[k];
    }
    double *const s_shuffled =
        (double *)analysis_memory(platform_cell_taken(s_count * sizeof(double), platform_place_g_program_owner, 0));
    for (size_t k = 0u; k < s_count; k += 1u)
    {
        // k fits a machine integer.
        s_shuffled[k] = (double)(zeros->n_start + (int64_t)k + 1) - (0.5 * (u_shuffled[k] + u_shuffled[k + 1u]));
    }
    double gap_variance = 0.0;
    double gap_sum = 0.0;
    for (size_t k = 0u; k < s_count; k += 1u)
    {
        gap_variance += (gaps[k] - 1.0) * (gaps[k] - 1.0);
        gap_sum += gaps[k];
    }
    gap_variance /= (double)s_count;
    double neighbors = 0.0;
    for (size_t k = 0u; (k + 1u) < s_count; k += 1u)
    {
        neighbors += (gaps[k] - 1.0) * (gaps[k + 1u] - 1.0);
    }
    neighbors = (neighbors / (double)(s_count - 1u)) / gap_variance;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  spacings on theta / pi: mean ");
        mmgr_line_fixed(output_line, (double)(gap_sum / (double)s_count), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", variance ");
        mmgr_line_fixed(output_line, (double)(gap_variance), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", neighbors' correlation ");
        mmgr_line_fixed(output_line, (double)(neighbors), 4u, 0u, EMBED_TRUE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  S's pull back each zero ");
        mmgr_line_fixed(output_line, (double)(pull(s_series, s_count)), 4u, 0u, EMBED_TRUE);
        mmgr_line_put(output_line, "; with the spacings shuffled ");
        mmgr_line_fixed(output_line, (double)(pull(s_shuffled, s_count)), 4u, 0u, EMBED_TRUE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    double correlations[LAGS + 1];
    for (size_t lag = 0u; lag <= LAGS; lag += 1u)
    {
        correlations[lag] = autocorrelation(s_series, s_count, lag);
    }
    size_t deepest = 1u;
    for (size_t lag = 2u; lag <= LAGS; lag += 1u)
    {
        deepest = (correlations[lag] < correlations[deepest]) ? lag : deepest;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  S's correlation over k zeros, k 1 to ");
        mmgr_line_decimal_signed(output_line, LAGS);
        mmgr_line_put(output_line, "; its least at k = ");
        mmgr_line_decimal(output_line, (unsigned long long)deepest, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_fixed(output_line, (double)(correlations[deepest]), 3u, 0u, EMBED_TRUE);
        mmgr_line_put(output_line, ", a lag of ");
        mmgr_line_fixed(output_line, (double)((double)deepest * spacing), 2u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " in t:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (size_t row = 0u; row < LAGS; row += 10u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "   ");
            platform_line_out_ended();
        }
        for (size_t at = row + 1u; at <= (row + 10u); at += 1u)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(correlations[at]), 3u, 0u, EMBED_TRUE);
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    static const int64_t LIMITS[2] = {100, 10000};
    for (int which = 0; which < 2; which += 1)
    {
        int64_t (*powers)[2] = NULL;
        const size_t power_count = tension_prime_powers(LIMITS[which], &powers);
        double weight = 0.0;
        double *const sizes = (double *)analysis_memory(
            platform_cell_taken(power_count * sizeof(double), platform_place_g_program_owner, 0));
        for (size_t at = 0u; at < power_count; at += 1u)
        {
            int64_t value = 1;
            for (int64_t r = 0; r < powers[at][1]; r += 1)
            {
                value *= powers[at][0];
            }
            sizes[at] = (double)(powers[at][1] * powers[at][1] * value);
            weight += 1.0 / sizes[at];
        }
        double model[LAGS + 1];
        for (size_t lag = 0u; lag <= LAGS; lag += 1u)
        {
            double total = 0.0;
            for (size_t at = 0u; at < power_count; at += 1u)
            {
                // The lag and the power are small.
                const double angle = ((double)(powers[at][1] * (int64_t)lag) * spacing) * log((double)powers[at][0]);
                total += cos(angle) / sizes[at];
            }
            model[lag] = total / weight;
        }
        double top = 0.0;
        double bottom = 0.0;
        for (size_t lag = 1u; lag <= LAGS; lag += 1u)
        {
            top += correlations[lag] * model[lag];
            bottom += model[lag] * model[lag];
        }
        const double scale = top / bottom;
        double residual = 0.0;
        int agree = 0;
        for (size_t lag = 1u; lag <= LAGS; lag += 1u)
        {
            const double gap = correlations[lag] - (scale * model[lag]);
            residual += gap * gap;
            agree += (correlations[lag] > 0.0) == (model[lag] > 0.0);
        }
        residual = sqrt(residual / LAGS);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  the waves of the prime powers to ");
            mmgr_line_decimal_signed(output_line, (long long)LIMITS[which]);
            mmgr_line_put(output_line, ", scaled by ");
            mmgr_line_fixed(output_line, (double)(scale), 3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal_signed(output_line, agree);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal_signed(output_line, LAGS);
            mmgr_line_put(output_line, " signs the same, rms ");
            mmgr_line_fixed(output_line, (double)(residual), 3u, 0u, EMBED_FALSE);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_cell_released(sizes, platform_place_g_program_owner);
        platform_cell_released(powers, platform_place_g_program_owner);
    }
    static const size_t FAR[4] = {100u, 200u, 500u, 1000u};
    for (int at = 0; at < 4; at += 1)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  S's correlation over ");
        mmgr_line_decimal(output_line, (unsigned long long)FAR[at], 1u);
        mmgr_line_put(output_line, " zeros ");
        mmgr_line_fixed(output_line, (double)(autocorrelation(s_series, s_count, FAR[at])), 4u, 0u, EMBED_TRUE);
        mmgr_line_put(output_line, "; the shuffled spacings' ");
        mmgr_line_fixed(output_line, (double)(autocorrelation(s_shuffled, s_count, FAR[at])), 4u, 0u, EMBED_TRUE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  number variance over L on theta / pi: the zeros, the shuffled spacings, GUE's, Poisson's");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    static const double LENGTHS[21] = {0.5, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 17, 20, 25, 40, 60, 100, 160, 250, 400};
    for (int at = 0; at < 21; at += 1)
    {
        const double length = LENGTHS[at];
        if ((length * 8.0) > (double)count)
        {
            continue;
        }
        const double v = number_variance(&twister, u, count, length, 80000u);
        const double v_shuffled = number_variance(&twister, u_shuffled, count, length, 80000u);
        const double gue =
            (log(2.0 * PI_DOUBLE * length) + EULER + 1.0 - ((PI_DOUBLE * PI_DOUBLE) / 8.0)) / (PI_DOUBLE * PI_DOUBLE);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    L ");
            mmgr_line_fixed(output_line, (double)(length), 1u, 6u, EMBED_FALSE);
            mmgr_line_put(output_line, ": ");
            mmgr_line_fixed(output_line, (double)(v), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, "  ");
            mmgr_line_fixed(output_line, (double)(v_shuffled), 4u, 8u, EMBED_FALSE);
            mmgr_line_put(output_line, "  ");
            mmgr_line_fixed(output_line, (double)(gue), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, "  ");
            mmgr_line_fixed(output_line, (double)(length), 1u, 0u, EMBED_FALSE);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    platform_cell_released(u, platform_place_g_program_owner);
    platform_cell_released(s_series, platform_place_g_program_owner);
    platform_cell_released(gaps, platform_place_g_program_owner);
    platform_cell_released(shuffled, platform_place_g_program_owner);
    platform_cell_released(u_shuffled, platform_place_g_program_owner);
    platform_cell_released(s_shuffled, platform_place_g_program_owner);
    return 0;
}

int program_main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argument_count < 4)
    {
        fprintf(stderr, "usage: exact_zeta_tension <binary> <first> <last> [extra] [N at first's F N at last's F]\n");
        return 2;
    }
    const int64_t extra = (argument_count > 4) ? atoll(arguments[4]) : 2;
    int64_t given[2] = {0, 0};
    const int has_given = argument_count > 6;
    if (has_given)
    {
        given[0] = atoll(arguments[5]);
        given[1] = atoll(arguments[6]);
    }
    TensionZeros zeros;
    tension_certified_zeros(arguments[1], atoll(arguments[2]), atoll(arguments[3]), extra, has_given ? given : NULL,
                            &zeros);
    const int code = analyse(&zeros);
    platform_cell_released(zeros.zeros, platform_place_g_program_owner);
    return code;
}
