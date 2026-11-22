// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-022
/**
 * @file exact_zeta_gram.c
 * @brief The theta function of Riemann and Siegel, the Gram points, and the sign of Z at each, by verdicts.
 *
 * @note Every verdict is a field, zero false and nonzero true. A pass writes its verdicts for the next pass to
 *       read, as in exact_zeta_zeros.c, whose values this reads.
 * @note Run build_exact_zeta.sh gram [height] [bits], beside this file.
 * @note This reads no corpus. It sits in 0_experimental, and is an entry in the workbook of analytic number
 *       theory, on its rail: it claims nothing about the Riemann hypothesis.
 * @note The phase: theta(t) = Im ln Gamma(1/4 + it/2) - (t/2) ln pi. With z = 1/4 + it/2 and w = z + M,
 *       ln Gamma(z) = ln Gamma(w) - sum over k < M of ln(z + k). The series of Stirling with K Bernoulli terms
 *       gives ln Gamma(w).
 * @note Its imaginary part has three parts. The first is (M - 1/4) arg w + (t/4) ln |w|^2 - t/2. The second is
 *       the imaginary parts of B_2j / (2j (2j - 1)) w^(1 - 2j). The third is minus every arg(z + k).
 * @note Each arg is an arctangent of a rational, by two series that must agree. One is the series of Euler, with
 *       its own pi from the identity of Euler. The other is the Taylor series about 1/2, with its own pi from the
 *       identity of Machin.
 * @note ln comes from the two series of zeta_exact.c, and ln pi from the logarithm of the floor of pi. Two routes
 *       run, at M = K = N and at 2N, and where they disagree, N doubles.
 * @note The Gram index: g_n is the point where theta(g_n) = n pi. At a point, the index i is theta, read at its
 *       places, over pi. The search decides the point where theta reads strictly above i pi and strictly below
 *       (i + 1) pi. Each reading is toward zero at the same places. A reading of equal doubles the places.
 * @note The walk: up from t = 10 in cells. theta increases past t = 6.29. The article on the theta function of
 *       Riemann and Siegel on Wikipedia reports that. A cell holds as many Gram points as the indices of its ends
 * differ by. An empty cell doubles the step, a cell that holds one is a bracket, and a cell that holds more splits.
 * @note Each bracket gets one bit per pass: the index at the midpoint gates which half the next pass reads.
 * @note The sign of Z: zeta(1/2 + it) = e^(-i theta) Z(t), with Z real. Just below g_n, Im zeta has the sign of
 *       Re zeta, and just above, the opposite sign. This phase verdict ties theta, from Stirling, to the phase of
 *       zeta, from Euler-Maclaurin. A bracket is final where three things hold at both ends: the phase verdict,
 *       one nonzero sign of Re zeta, and the step verdict. Else the index at its midpoint cuts it in half.
 * @note On a settled bracket, the sign of Re zeta is the sign of (-1)^n Z(g_n). Gram's law is that sign positive.
 * @note Gram's law as an agreement: the signs of Z(g_n) alternate where Gram's law holds. That is agreement at
 *       lag 2 and none at lag 1. The null is the same signs in drawn orders. It counts the draws that reach the
 *       live agreement at lag 2. zeta_shuffle.h states the draws.
 * @note Gram's law is a reading here, and never a steer. It fails for about a quarter of the Gram intervals in the
 *       long run, as the same article reports. The zero walk of exact_zeta_zeros.c steers only by its own verdicts.
 * @note Positive control: the Gram points g_0 to g_15 against the table of that article. Also the first failure
 *       of Gram's law at n = 126, as the same article reports it. The null: the Bernoulli terms left out of both
 *       routes of theta. Then the routes do not meet at the doublings shown, and the search decides no Gram index.
 */
#include "zeta_gram_walk.h"
#include "../../../src/platform/platform.h"
#include "zeta_search.h"
#include "zeta_theta.h"
#include "zeta_shuffle.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The seed of the drawn orders, as the shuffles of the tree name it. */
#define SHUFFLE_SEED 0x51F7u

/** @brief How many drawn orders. */
#define DRAWS 1000

/**
 * @brief The text of a truth value, as Python prints it.
 *
 * @param[in] value The value.
 * @return          "True" or "False".
 */
static const char *truth(int64_t value)
{
    return value ? "True" : "False";
}

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *gram_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief Cuts each bracket until zeta settles the sign of Re zeta(1/2 + it) on it.
 *
 * @param[in,out] search   The search of the Gram index [BORROWS].
 * @param[in]     brackets The placed brackets [BORROWS].
 * @param[in]     count    How many.
 * @param[in,out] zeta     The search of zeta [BORROWS].
 * @param[out]    signs    The sign at each index, by index [BORROWS]. Large enough for every index.
 * @param[out]    settled  The settled bracket at each index, by index [BORROWS].
 * @return                 How many further cuts.
 */
static size_t gram_settle(GramSearch *search, const GramBracket *brackets, size_t count, Steering *zeta, int64_t *signs,
                          GramBracket *settled)
{
    GramBracket *open = (GramBracket *)gram_memory(
        platform_cell_taken((count + 1u) * sizeof(GramBracket), platform_place_g_program_owner, 0));
    memcpy(open, brackets, count * sizeof(GramBracket));
    size_t open_count = count;
    size_t cuts = 0u;
    DecimalPair line;
    pair_small(5, 1, &line);
    while (open_count > 0u)
    {
        PointList asked = {NULL, 0u, 0u};
        size_t *const ends = (size_t *)gram_memory(
            platform_cell_taken((2u * open_count + 1u) * sizeof(size_t), platform_place_g_program_owner, 0));
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            ZetaPoint point;
            point.sigma = line;
            point.t = open[at].low;
            ends[2u * at] = steering_intern(zeta, &point);
            point.t = open[at].high;
            ends[(2u * at) + 1u] = steering_intern(zeta, &point);
            point_list_push(&asked, ends[2u * at]);
            point_list_push(&asked, ends[(2u * at) + 1u]);
        }
        steering_sweep(zeta, &asked);
        point_list_free(&asked);
        size_t next_count = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            const size_t a = ends[2u * at];
            const size_t c = ends[(2u * at) + 1u];
            const int64_t sign_a = zeta->records[a].value[0].sign;
            const int64_t sign_c = zeta->records[c].value[0].sign;
            const int64_t phase =
                (zeta->records[a].value[1].sign == sign_a) && (zeta->records[c].value[1].sign == -sign_c);
            const int64_t same = sign_a == sign_c;
            const int64_t from_a = steering_step(zeta, a, c, a);
            const int64_t from_c = steering_step(zeta, a, c, c);
            const int64_t decided = phase * same * ((1 + from_a) / 2) * ((1 + from_c) / 2);
            if (decided)
            {
                signs[open[at].index] = sign_a;
                settled[open[at].index] = open[at];
                continue;
            }
            open[next_count] = open[at];
            next_count += 1u;
        }
        platform_cell_released(ends, platform_place_g_program_owner);
        gram_cut(search, open, next_count);
        open_count = next_count;
        cuts += next_count;
    }
    platform_cell_released(open, platform_place_g_program_owner);
    return cuts;
}

/**
 * @brief The two routes of theta at t, with the Bernoulli terms, as the walk reads them.
 *
 * @param[in]  t      The point [BORROWS].
 * @param[in]  places The places.
 * @param[in]  n_sum  The cut N.
 * @param[out] one    The first route [BORROWS].
 * @param[out] two    The second route [BORROWS].
 */
static void theta_phase(const DecimalPair *t, int32_t places, int64_t n_sum, OriorExactInteger *one,
                        OriorExactInteger *two)
{
    theta_routes(t, places, n_sum, 1, one, two);
}

/**
 * @brief The gap of the two routes of theta at a cut.
 *
 * @param[in]  t         The point [BORROWS].
 * @param[in]  places    The places.
 * @param[in]  n_sum     The cut N.
 * @param[in]  bernoulli 1 to keep the Bernoulli terms, 0 to drop them.
 * @param[out] gap       The gap [BORROWS].
 */
static void theta_gap(const DecimalPair *t, int32_t places, int64_t n_sum, int bernoulli, OriorExactInteger *gap)
{
    OriorExactInteger one;
    OriorExactInteger two;
    theta_routes(t, places, n_sum, bernoulli, &one, &two);
    z_sub(&one, &two, gap);
    z_abs(gap, gap);
}

/**
 * @brief Writes a list of integers as Python prints a list.
 *
 * @param[in] values The values [BORROWS].
 * @param[in] count  How many.
 * @param[in] out    The stream [BORROWS].
 */
static void print_list(const OriorExactInteger *values, size_t count, FILE *out)
{
    fputs("[", out);
    for (size_t at = 0u; at < count; at += 1u)
    {
        if (at > 0u)
        {
            fputs(", ", out);
        }
        z_print(&values[at], out);
    }
    fputs("]", out);
}

int program_main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *const out = stdout;
    DecimalPair height;
    pair_units((argument_count > 1) ? arguments[1] : "285", &height);
    const int bits = (argument_count > 2) ? atoi(arguments[2]) : 16;
    fputs("  the Riemann-Siegel theta function and the Gram points by truthy and falsy verdicts, in exact integers\n",
          out);
    fputs(
        "  this reads the sign of Z at the Gram points it reaches and claims nothing about the Riemann hypothesis\n\n",
        out);

    GramSearch search;
    memset(&search, 0, sizeof(search));
    search.phase = theta_phase;
    pair_small(10, 0, &search.start);
    pair_small(1, 0, &search.step);
    GramBracket *found = NULL;
    const size_t found_count = gram_walk(&search, &height, &found);
    if (found_count == 0u)
    {
        zeta_stop("no Gram point below the height");
    }
    const int64_t last_index = found[found_count - 1u].index;
    const int64_t consecutive = ((int64_t)found_count == (last_index + 1)) && (found[0].index == 0);
    fprintf(out, "  %llu Gram points with 10 < t < ", (unsigned long long)found_count);
    pair_print(&height, height.places, out);
    fprintf(out, ", indices 0 to %lld, each alone in its bracket: %s\n", (long long)last_index, truth(consecutive));
    fflush(out);

    GramBracket *const placed = (GramBracket *)gram_memory(
        platform_cell_taken(found_count * sizeof(GramBracket), platform_place_g_program_owner, 0));
    memcpy(placed, found, found_count * sizeof(GramBracket));
    for (int bit = 0; bit < bits; bit += 1)
    {
        gram_cut(&search, placed, found_count);
    }
    char label[64];
    snprintf(label, sizeof(label), "placed by %d bits", bits);
    fprintf(out, "  %-5s %-34s %-15s %s\n", "n", label, "published", "holds");
    int64_t every = 1;
    for (size_t at = 0u; (at < found_count) && (at < 16u); at += 1u)
    {
        const int held = pair_holds(&placed[at].low, &placed[at].high, ZETA_GRAM_PUBLISHED[at]);
        every *= held;
        fprintf(out, "  %-5lld [", (long long)placed[at].index);
        pair_print(&placed[at].low, 10, out);
        fputs(", ", out);
        pair_print(&placed[at].high, 10, out);
        fprintf(out, "]  %-15s %s\n", ZETA_GRAM_PUBLISHED[at], truth(held));
    }
    fprintf(out, "  %llu theta values asked, the deepest at %d places, the widest at N = %lld\n\n",
            (unsigned long long)search.asked, (int)search.deepest, (long long)search.widest);
    fflush(out);

    Steering zeta;
    steering_begin(&zeta);
    // The indices run 0 to last_index, one bracket each, as the consecutive verdict checks.
    int64_t *const signs = (int64_t *)gram_memory(
        platform_cell_cleared((size_t)last_index + 1u, sizeof(int64_t), platform_place_g_program_owner, 0));
    GramBracket *const settled = (GramBracket *)gram_memory(
        platform_cell_cleared((size_t)last_index + 1u, sizeof(GramBracket), platform_place_g_program_owner, 0));
    const size_t cuts = gram_settle(&search, placed, found_count, &zeta, signs, settled);
    int64_t *const ordered =
        (int64_t *)gram_memory(platform_cell_taken(found_count * sizeof(int64_t), platform_place_g_program_owner, 0));
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        ordered[at] = signs[found[at].index];
    }
    fputs("  the sign of (-1)^n Z(g_n), from Re zeta(1/2 + i g_n) on each settled bracket:\n", out);
    for (size_t row = 0u; row < found_count; row += 32u)
    {
        fprintf(out, "    %4lld  ", (long long)found[row].index);
        for (size_t at = row; (at < found_count) && (at < (row + 32u)); at += 1u)
        {
            fputc((ordered[at] > 0) ? '+' : '-', out);
        }
        fputs("\n", out);
    }
    fputs("  Gram's law fails at n = ", out);
    size_t bad_count = 0u;
    int64_t first_bad = -1;
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        if (ordered[at] == -1)
        {
            fprintf(out, "%s%lld", (bad_count > 0u) ? ", " : "", (long long)found[at].index);
            first_bad = (bad_count == 0u) ? found[at].index : first_bad;
            bad_count += 1u;
        }
    }
    fputs((bad_count == 0u) ? "none\n" : "\n", out);
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        if (ordered[at] == -1)
        {
            const GramBracket *const bracket = &settled[found[at].index];
            fprintf(out, "    g_%lld in [", (long long)found[at].index);
            pair_print(&bracket->low, 10, out);
            fputs(", ", out);
            pair_print(&bracket->high, 10, out);
            fputs("]\n", out);
        }
    }
    fprintf(out, "  %llu zeta values asked, %llu further cuts, the deepest at %d places, the widest at N = %lld\n",
            (unsigned long long)zeta.asked, (unsigned long long)cuts, (int)zeta.deepest, (long long)zeta.widest);
    const int64_t reached = last_index >= ZETA_GRAM_FIRST_FAILURE;
    const int64_t first = (reached * (first_bad == ZETA_GRAM_FIRST_FAILURE)) + ((1 - reached) * (bad_count == 0u));
    fprintf(out, "  the first failure against the reported index %d: %s\n\n", ZETA_GRAM_FIRST_FAILURE, truth(first));
    fflush(out);

    unsigned char *const seats =
        (unsigned char *)gram_memory(platform_cell_taken(found_count + 1u, platform_place_g_program_owner, 0));
    size_t plus_count = 0u;
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        const int64_t value = ordered[at] * (((found[at].index % 2) == 0) ? 1 : -1);
        // The value is -1 or 1, and (1 + value) / 2 is 0 or 1.
        seats[at] = (unsigned char)((1 + value) / 2);
        plus_count += seats[at];
    }
    const size_t live_one = zeta_agreement(seats, found_count, 1u);
    const size_t live_two = zeta_agreement(seats, found_count, 2u);
    unsigned char *const drawn =
        (unsigned char *)gram_memory(platform_cell_taken(found_count + 1u, platform_place_g_program_owner, 0));
    size_t reach = 0u;
    size_t most = 0u;
    for (unsigned draw = 0u; draw < DRAWS; draw += 1u)
    {
        memcpy(drawn, seats, found_count);
        zeta_permuted(drawn, found_count, SHUFFLE_SEED + draw);
        const size_t agreement = zeta_agreement(drawn, found_count, 2u);
        reach += (agreement >= live_two) ? 1u : 0u;
        most = (agreement > most) ? agreement : most;
    }
    fprintf(out, "  the signs of Z(g_n), %llu positive and %llu negative, as a sequence\n",
            (unsigned long long)plus_count, (unsigned long long)(found_count - plus_count));
    fprintf(out, "    agreement at lag 1: %llu of %llu, at lag 2: %llu of %llu\n", (unsigned long long)live_one,
            (unsigned long long)(found_count - 1u), (unsigned long long)live_two,
            (unsigned long long)(found_count - 2u));
    fprintf(out, "    %d drawn orders of the same signs, %llu reaching %llu at lag 2, the max %llu\n", DRAWS,
            (unsigned long long)reach, (unsigned long long)live_two, (unsigned long long)most);
    const size_t fewer = (plus_count < (found_count - plus_count)) ? plus_count : (found_count - plus_count);
    const int64_t alternates = (live_two > most) && (fewer >= 32u);
    fprintf(out, "  agreement at lag 2 above every drawn order, with 32 or more of each sign: %s\n\n",
            truth(alternates));
    fflush(out);

    DecimalPair point;
    pair_small(20, 0, &point);
    const int32_t places = 8;
    OriorExactInteger true_gaps[6];
    OriorExactInteger null_gaps[6];
    for (unsigned j = 0u; j < 6u; j += 1u)
    {
        theta_gap(&point, places, (int64_t)1 << j, 1, &true_gaps[j]);
        theta_gap(&point, places, (int64_t)1 << j, 0, &null_gaps[j]);
    }
    fprintf(out, "  drawn null at t = 20, %d places: the gap between the two routes of theta as N doubles from 1\n",
            (int)places);
    fputs("    with the Bernoulli terms:     ", out);
    print_list(true_gaps, 6u, out);
    fputs("\n    without them:                 ", out);
    print_list(null_gaps, 6u, out);
    fputs("\n", out);
    const int64_t apart = (true_gaps[5].sign == 0) && (null_gaps[5].sign > 0);
    fprintf(out, "  without the Bernoulli terms the routes do not meet at the doublings displayed: %s\n", truth(apart));
    fflush(out);
    return (int)(1 - (consecutive * every * first * alternates * apart));
}
