// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-021
/**
 * @file exact_zeta_zeros.c
 * @brief The zeros of the Riemann zeta function, counted and placed by verdicts that are true or false.
 *
 * @note Run build_exact_zeta.sh zeros [height] [bits], beside this file.
 * @note This reads no corpus. It sits in 0_experimental, and is an entry in the workbook of analytic number
 *       theory, on its rail: it claims nothing about the Riemann hypothesis. It counts the zeros below the height
 *       that it walks to, and stops there.
 * @note The values: zeta_exact.h states them. Zeta is a sum by Euler-Maclaurin. The sum holds the terms below N,
 *       the integral of the rest N^(1-s)/(s-1), half the last term, and N corrections of Bernoulli. Each coefficient is
 * an exact rational, and zeta' comes from the same sum, term by term. Two routes run, one at N and one at 2N.
 * @note The verdicts at a point: whether the two routes agree, and the signs of Re zeta, Im zeta and
 *       |Re zeta| - |Im zeta|. Where the routes disagree, N doubles. Where they agree and a sign is zero, the
 *       places double. The search decides a point where every verdict is not zero. Its three signs give the eighth of a
 *       turn that zeta sits in.
 * @note The count: the eighth turns along a closed path of points sum to eight times the zeros inside. That is
 *       the argument principle. zeta_search.h states the verdicts on each edge.
 * @note The line, by symmetry: every box is symmetric about Re(s) = 1/2. zeta_zero_symmetry.py checks the group
 *       behind this. The map s -> 1 - conj(s) takes a zero to a zero, and its fixed set is the critical line. A zero
 * off the line brings its mirror into the same box. Thus a symmetric box that counts one holds a zero on the line.
 * @note The walk: the boxes climb the strip from t = 1, with Re(s) from 0 to 1. A box that counts zero doubles the
 *       next step. A box that counts one is a zero. A box that counts two or more splits into halves, and halves the
 *       step. Each zero gets one bit per pass, in squares centred on the line. The lower half counts one or zero,
 *       and that verdict gates which half the next pass reads.
 * @note Positive control: zeta(2) against pi^2/6, and the zeros against the first ordinates of the published table
 *       of Odlyzko. The null: the integral of the rest left out of both routes. Then the two routes move apart as
 *       N doubles, and no point is ever decided.
 */
#include "zeta_search.h"
#include "../../../src/platform/platform.h"

#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

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

// The pool of the program. Its cells of exact integers grow with the count of the run, and they pass the 64 MB default
const unsigned long long g_program_pool_bytes = 1ULL << 30u;

int program_main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *const out = stdout;
    DecimalPair height;
    pair_units((argument_count > 1) ? arguments[1] : "50", &height);
    const int bits = (argument_count > 2) ? atoi(arguments[2]) : 16;
    fputs("  zeros of zeta counted and placed by truthy and falsy verdicts, in exact integers\n", out);
    fputs("  this counts the zeros it reaches and claims nothing about the Riemann hypothesis\n\n", out);

    const int32_t places = 30;
    ZetaPoint two;
    pair_small(2, 0, &two.sigma);
    pair_small(0, 0, &two.t);
    OriorExactInteger value[2];
    OriorExactInteger gaps[64];
    size_t gap_count = 0u;
    const int64_t n_sum = zeta_agreed_value(&two, places, value, gaps, &gap_count);
    const int32_t digits = places + ZETA_GUARD;
    OriorExactInteger pi_value;
    zeta_pi(digits, &pi_value);
    OriorExactInteger sixth;
    z_mul(&pi_value, &pi_value, &sixth);
    OriorExactInteger under;
    z_ten_to(digits, &under);
    z_mul_small(&under, 6, &under);
    z_floor_div(&sixth, &under, &sixth);
    z_ten_to(ZETA_GUARD, &under);
    z_floor_div(&sixth, &under, &sixth);
    const int64_t zeta_two = orior_exact_equal(&value[0], &sixth) && (value[1].sign == 0);
    fprintf(out, "  zeta(2) at %d places, routes agreeing at N = %lld, equals pi^2/6 from Machin and Euler: %s\n\n",
            (int)places, (long long)n_sum, truth(zeta_two));
    fflush(out);

    Steering steering;
    steering_begin(&steering);
    BoxList found;
    steering_walk(&steering, &height, &found);
    fprintf(out, "  %llu zeros with 1 < t < ", (unsigned long long)found.count);
    pair_print(&height, height.places, out);
    fputs(", each alone in a box symmetric about Re(s) = 1/2\n", out);

    DecimalPair *const lows =
        (DecimalPair *)platform_cell_taken((found.count + 1u) * sizeof(DecimalPair), platform_place_g_program_owner, 0);
    DecimalPair *const highs =
        (DecimalPair *)platform_cell_taken((found.count + 1u) * sizeof(DecimalPair), platform_place_g_program_owner, 0);
    char **const paths =
        (char **)platform_cell_taken((found.count + 1u) * sizeof(char *), platform_place_g_program_owner, 0);
    if ((lows == NULL) || (highs == NULL) || (paths == NULL))
    {
        zeta_stop("out of memory");
    }
    steering_place(&steering, &found, bits, lows, highs, paths);
    char label[64];
    snprintf(label, sizeof(label), "placed by %d bits", bits);
    fprintf(out, "  %-36s %-14s %s\n", label, "published", "holds");
    int64_t every = found.count >= 1u;
    for (size_t at = 0u; at < found.count; at += 1u)
    {
        fputs("  [", out);
        pair_print(&lows[at], 8, out);
        fputs(", ", out);
        pair_print(&highs[at], 8, out);
        if (at < 40u)
        {
            const int held = pair_holds(&lows[at], &highs[at], ZETA_PUBLISHED_ZEROS[at]);
            every *= held;
            fprintf(out, "]  %-14s %s   %s\n", ZETA_PUBLISHED_ZEROS[at], truth(held), paths[at]);
        }
        else
        {
            fprintf(out, "]  %-14s %s   %s\n", "-", "-", paths[at]);
        }
    }
    fprintf(out, "  %llu values asked, the deepest point at %d places, the widest at N = %lld\n\n",
            (unsigned long long)steering.asked, (int)steering.deepest, (long long)steering.widest);

    ZetaPoint point;
    pair_small(5, 1, &point.sigma);
    pair_small(20, 0, &point.t);
    size_t true_count = 0u;
    const int64_t n_true = zeta_agreed_value(&point, 4, value, gaps, &true_count);
    OriorExactInteger null_gaps[68];
    const size_t null_count = true_count + 4u;
    for (size_t j = 0u; j < null_count; j += 1u)
    {
        OriorExactInteger ignored[2];
        zeta_gap(&point, 4, (int64_t)1 << j, 0, &null_gaps[j], ignored);
    }
    fputs("  drawn null at s = 1/2 + 20i, 4 places: the gap between the two routes as N doubles from 1\n", out);
    fputs("    with N^(1-s)/(s-1):    ", out);
    print_list(gaps, true_count, out);
    fprintf(out, ", agreeing at N = %lld\n", (long long)n_true);
    fputs("    without it:            ", out);
    print_list(null_gaps, null_count, out);
    fputs("\n", out);
    const int64_t apart =
        (z_cmp(&null_gaps[null_count - 1u], &null_gaps[null_count - 2u]) > 0) && (null_gaps[null_count - 2u].sign > 0);
    fprintf(out, "  without the integral of the rest the routes move apart and nothing is decided: %s\n", truth(apart));
    fflush(out);

    size_t published_below = 0u;
    for (size_t at = 0u; at < 40u; at += 1u)
    {
        DecimalPair ordinate;
        pair_units(ZETA_PUBLISHED_ZEROS[at], &ordinate);
        published_below += pair_below(&ordinate, &height) ? 1u : 0u;
    }
    DecimalPair last_ordinate;
    pair_units(ZETA_PUBLISHED_ZEROS[39], &last_ordinate);
    const int64_t covered = pair_below(&height, &last_ordinate);
    const int64_t counted = (covered * (found.count == published_below)) + ((1 - covered) * (found.count >= 40u));
    fputs("  the count against the published table below t = ", out);
    pair_print(&height, height.places, out);
    fprintf(out, ": %s\n", truth(counted));
    fflush(out);
    return (int)(1 - (zeta_two * every * apart * counted));
}
