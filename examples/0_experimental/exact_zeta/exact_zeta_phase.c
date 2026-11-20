// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-026
/**
 * @file exact_zeta_phase.c
 * @brief The phase of the Riemann zeta function, read finer than its reader, by evenly spaced jitter.
 *
 * @note Three sign verdicts give an eighth of a turn. The same three verdicts under evenly spaced jitter give the
 *       phase to any fraction of it.
 * @note Run build_exact_zeta.sh phase [jitters], beside this file.
 * @note This reads no corpus. It sits in 0_experimental, and is an entry in the workbook of analytic number
 *       theory, on its rail: it claims nothing about the Riemann hypothesis. It reads the phase at the points that
 *       it asks for, and stops there.
 * @note The reader: exact_zeta_zeros.c reads zeta at a point through three sign verdicts. They are the sign of the
 *       real part, the sign of the imaginary part, and the sign of |Re| - |Im|. Together they name the eighth of a
 *       turn that zeta sits in. The reader never forms an angle, and never divides.
 * @note The jitter: rotate the value by j / J of an eighth turn for j = 0 .. J - 1, and read each rotation with
 *       the same three verdicts. With the phase u counted in eighths, reading j is floor(u + j / J). The identity
 *       of Hermite sums them exactly: the sum over j < J of floor(u + j / J) is floor(J u).
 * @note The mean of J coarse readings is the phase to 1 / J of an eighth, an equality and not an estimate. Each
 *       jitter adds its verdict, and the sum drops none. The jitter must be evenly spaced across one whole eighth.
 *       The same offset read J times returns the coarse reading J times. Offsets spread across half an eighth bias
 *       the mean toward the step that they never reach.
 * @note The turn between jitters: the factor e^(ia) of Euler turns the value one way at a known rate. Across all J
 *       jitters it turns less than an eighth. Thus the lifted reading is 0 up to one crossing, and 1 after it. Two
 *       jitters that read alike read alike at every jitter between them.
 * @note A bisection finds the crossing in log2 J + 1 readings in place of J, and the sum is J k0 + J - j*. Both
 *       routes run in the control, and must agree.
 * @note The depth: the value comes at a count of places from the two routes of exact_zeta_zeros.c. N doubles
 *       until they agree. The program reads the jittered index at those places, and again at twice them. The
 *       places double until the two indices agree. The reader cannot draw more out of a value than the value holds.
 *       That agreement decides the depth that each point needs, and nothing assigns it in advance.
 * @note The line: on Re(s) = 1/2, zeta = e^(-i theta) Z, with Z real. Thus the phase of zeta is -theta up to a
 *       half turn. theta comes by a second route, the series of Stirling in zeta_theta.h. That route shares nothing
 *       with the sign reader. Where Z changes sign, the phase jumps a half turn. Each published zero falls inside a
 *       step where the jittered phase jumps.
 * @note Positive control: e^(i pi / 3), whose phase is 4/3 of an eighth, read as floor(4J / 3) / J for every J,
 *       and the line against -theta. The nulls: the repeated offset and the half spread. Each one is blind to the
 *       third of an eighth above the step.
 */
#include "zeta_theta.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The answer of the reader where any verdict is zero. */
#define NO_READING (-1)

/**
 * @brief The text of a truth value, as Python prints it.
 *
 * @param[in] value The value.
 * @return          "True" or "False".
 */
static const char *truth(int value)
{
    return value ? "True" : "False";
}

/**
 * @brief The reader: three sign verdicts name the eighth of a turn.
 *
 * @param[in] re The real part [BORROWS].
 * @param[in] im The imaginary part [BORROWS].
 * @return       The eighth, 0 to 7, or NO_READING where any verdict is zero.
 */
static int octant(const OriorExactInteger *re, const OriorExactInteger *im)
{
    OriorExactInteger size_re;
    OriorExactInteger size_im;
    z_abs(re, &size_re);
    z_abs(im, &size_im);
    const int sign_re = re->sign;
    const int sign_im = im->sign;
    const int sign_size = z_cmp(&size_re, &size_im);
    if ((sign_re == 0) || (sign_im == 0) || (sign_size == 0))
    {
        return NO_READING;
    }
    const int quadrant = (1 - sign_im) + ((1 - (sign_re * sign_im)) / 2);
    return (2 * quadrant) + ((((1 - sign_size) / 2) + quadrant) % 2);
}

/** @brief A value at places, read by the three verdicts after a rotation by j / J of a fraction of an eighth. */
typedef struct
{
    int32_t digits;         /**< The places, with the guard. */
    OriorExactInteger re;   /**< The real part, with the guard. */
    OriorExactInteger im;   /**< The imaginary part, with the guard. */
    OriorExactInteger step; /**< The angle of one jitter, at the digits. */
    int64_t jitters;        /**< The count J of jitters. */
    int64_t readings;       /**< How many readings it takes. */
} Rotated;

/**
 * @brief re + i im at the places, with a rotation of numerator / denominator of an eighth over J jitters.
 *
 * @param[out] rotated     The value [BORROWS].
 * @param[in]  re          The real part [BORROWS].
 * @param[in]  im          The imaginary part [BORROWS].
 * @param[in]  places      The places.
 * @param[in]  jitters     The count J.
 * @param[in]  numerator   The numerator of the fraction of an eighth.
 * @param[in]  denominator The denominator of the fraction of an eighth.
 */
static void rotated_make(Rotated *rotated, const OriorExactInteger *re, const OriorExactInteger *im, int32_t places,
                         int64_t jitters, int64_t numerator, int64_t denominator)
{
    rotated->digits = places + ZETA_GUARD;
    rotated->re = *re;
    rotated->im = *im;
    zeta_require(orior_exact_scale_by_ten(&rotated->re, ZETA_GUARD), "scale");
    zeta_require(orior_exact_scale_by_ten(&rotated->im, ZETA_GUARD), "scale");
    OriorExactInteger pi_value;
    zeta_pi(rotated->digits, &pi_value);
    z_mul_small(&pi_value, numerator, &rotated->step);
    OriorExactInteger under;
    z_set(&under, 4 * jitters * denominator);
    z_floor_div(&rotated->step, &under, &rotated->step);
    rotated->jitters = jitters;
    rotated->readings = 0;
}

/**
 * @brief The reading at jitter j, lifted onto the first: 0 or 1.
 *
 * @param[in,out] rotated The value [BORROWS].
 * @param[in]     j       The jitter.
 * @param[in]     first   The first reading, or NO_READING to take the plain reading.
 * @return                The reading, or NO_READING where a verdict is zero or the lift fails.
 */
static int rotated_lift(Rotated *rotated, int64_t j, int first)
{
    rotated->readings += 1;
    OriorExactInteger scale;
    z_ten_to(rotated->digits, &scale);
    OriorExactInteger angle;
    z_mul_small(&rotated->step, j, &angle);
    OriorExactInteger cosine;
    OriorExactInteger sine;
    zeta_cos_sin(&angle, rotated->digits, &cosine, &sine);
    OriorExactInteger re;
    OriorExactInteger im;
    OriorExactInteger product;
    z_mul(&rotated->re, &cosine, &re);
    z_mul(&rotated->im, &sine, &product);
    z_sub(&re, &product, &re);
    z_floor_div(&re, &scale, &re);
    z_mul(&rotated->re, &sine, &im);
    z_mul(&rotated->im, &cosine, &product);
    z_add(&im, &product, &im);
    z_floor_div(&im, &scale, &im);
    const int reading = octant(&re, &im);
    if (reading == NO_READING)
    {
        return NO_READING;
    }
    if (first == NO_READING)
    {
        return reading;
    }
    const int lifted = (((reading - first) % 8) + 8) % 8;
    return (lifted > 1) ? NO_READING : lifted;
}

/** @brief The answer of a sum where a verdict is zero. */
#define NO_SUM INT64_MIN

/**
 * @brief The sum of Hermite by a reading at every jitter, j = 0 .. J - 1.
 *
 * @param[in,out] rotated The value [BORROWS].
 * @return                The sum, or NO_SUM where a verdict is zero.
 */
static int64_t jittered_every(Rotated *rotated)
{
    const int first = rotated_lift(rotated, 0, NO_READING);
    if (first == NO_READING)
    {
        return NO_SUM;
    }
    int64_t total = rotated->jitters * first;
    for (int64_t j = 1; j < rotated->jitters; j += 1)
    {
        const int lift = rotated_lift(rotated, j, first);
        if (lift == NO_READING)
        {
            return NO_SUM;
        }
        total += lift;
    }
    return total;
}

/**
 * @brief The sum of Hermite by bisection.
 *
 * @param[in,out] rotated The value [BORROWS].
 * @return                The sum, or NO_SUM where a verdict is zero.
 * @note The rotation e^(ia) turns the phase one way, and by less than an eighth over all J jitters. The lifted
 *       reading is 0 up to one crossing, and 1 after it. The bisection finds the crossing j* in log2 J readings,
 *       and the sum is J k0 + J - j*.
 */
static int64_t jittered(Rotated *rotated)
{
    const int first = rotated_lift(rotated, 0, NO_READING);
    if (first == NO_READING)
    {
        return NO_SUM;
    }
    int64_t low = 0;
    int64_t high = rotated->jitters;
    while ((high - low) > 1)
    {
        const int64_t middle = (low + high) / 2;
        const int lift = rotated_lift(rotated, middle, first);
        if (lift == NO_READING)
        {
            return NO_SUM;
        }
        if (lift == 0)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return (rotated->jitters * first) + rotated->jitters - high;
}

/**
 * @brief The jittered index of zeta at sigma + it, at the fewest places where it agrees with the index at twice.
 *
 * @param[in] point   The point [BORROWS].
 * @param[in] jitters The count J.
 * @return            The index, in 1 / J of an eighth.
 */
static int64_t phase_at(const ZetaPoint *point, int64_t jitters)
{
    int32_t places = 8;
    for (;;)
    {
        OriorExactInteger one[2];
        OriorExactInteger two[2];
        OriorExactInteger gaps[64];
        size_t count = 0u;
        (void)zeta_agreed_value(point, places, one, gaps, &count);
        (void)zeta_agreed_value(point, 2 * places, two, gaps, &count);
        Rotated first;
        rotated_make(&first, &one[0], &one[1], places, jitters, 1, 1);
        const int64_t index = jittered(&first);
        if (index != NO_SUM)
        {
            Rotated second;
            rotated_make(&second, &two[0], &two[1], 2 * places, jitters, 1, 1);
            if (index == jittered(&second))
            {
                return index;
            }
        }
        places *= 2;
    }
}

/**
 * @brief theta at the places, by the two routes of Stirling. N doubles until they agree.
 *
 * @param[in]  t      The point [BORROWS].
 * @param[in]  places The places.
 * @param[out] result The value [BORROWS].
 */
static void theta_at(const DecimalPair *t, int32_t places, OriorExactInteger *result)
{
    int64_t n_sum = 1;
    OriorExactInteger one;
    OriorExactInteger two;
    theta_routes(t, places, n_sum, 1, &one, &two);
    while (!orior_exact_equal(&one, &two))
    {
        n_sum *= 2;
        theta_routes(t, places, n_sum, 1, &one, &two);
    }
    *result = two;
}

/**
 * @brief Writes an index in 1 / J of an eighth as turns, at six places.
 *
 * @param[in] index   The index.
 * @param[in] jitters The count J.
 * @param[in] out     The stream [BORROWS].
 * @param[in] width   The width to pad to on the left.
 */
static void print_turns(int64_t index, int64_t jitters, FILE *out, int width)
{
    OriorExactInteger value;
    z_set(&value, index);
    zeta_require(orior_exact_scale_by_ten(&value, 6u), "scale");
    OriorExactInteger under;
    z_set(&under, 8 * jitters);
    z_floor_div(&value, &under, &value);
    DecimalPair pair;
    pair_make(&value, 6, &pair);
    char text[64];
    pair_text(&pair, 6, text, sizeof(text));
    fprintf(out, "%-*s", width, text);
}

/**
 * @brief Section one: e^(i pi / 3) read at every J against floor(4J / 3), and the two nulls.
 *
 * @param[in] out The stream [BORROWS].
 * @param[in] top The largest J.
 * @return        1 where every check lands.
 */
static int report_control(FILE *out, int64_t top)
{
    const int32_t places = 30;
    const int32_t digits = places + ZETA_GUARD;
    OriorExactInteger third;
    zeta_pi(digits, &third);
    OriorExactInteger three;
    z_set(&three, 3);
    z_floor_div(&third, &three, &third);
    OriorExactInteger cosine;
    OriorExactInteger sine;
    zeta_cos_sin(&third, digits, &cosine, &sine);
    OriorExactInteger drop;
    z_ten_to(ZETA_GUARD, &drop);
    OriorExactInteger re;
    OriorExactInteger im;
    z_floor_div(&cosine, &drop, &re);
    z_floor_div(&sine, &drop, &im);
    fputs("  control: e^(i pi / 3), its phase 4/3 of an eighth, in eighths\n", out);
    fprintf(out, "  %-6s %-12s %-12s %-16s %-16s %-14s %s\n", "J", "bisected", "floor(4J/3)", "every jitter",
            "readings", "same offset", "half spread");
    int exact = 1;
    int blind = 1;
    for (int64_t jitters = 1; jitters <= top; jitters *= 4)
    {
        Rotated bisect;
        Rotated every;
        rotated_make(&bisect, &re, &im, places, jitters, 1, 1);
        rotated_make(&every, &re, &im, places, jitters, 1, 1);
        const int64_t index = jittered(&bisect);
        const int64_t swept = jittered_every(&every);
        Rotated single;
        rotated_make(&single, &re, &im, places, 1, 1, 1);
        const int64_t same = jitters * jittered(&single);
        Rotated half;
        rotated_make(&half, &re, &im, places, jitters, 1, 2);
        const int64_t narrow = jittered(&half);
        exact = exact && (index == swept) && (swept == ((4 * jitters) / 3));
        blind = blind && (same == narrow) && (narrow == jitters);
        char cells[6][64];
        snprintf(cells[0], sizeof(cells[0]), "%lld/%lld", (long long)index, (long long)jitters);
        snprintf(cells[1], sizeof(cells[1]), "%lld/%lld", (long long)((4 * jitters) / 3), (long long)jitters);
        snprintf(cells[2], sizeof(cells[2]), "%lld/%lld", (long long)swept, (long long)jitters);
        snprintf(cells[3], sizeof(cells[3]), "%lld against %lld", (long long)bisect.readings,
                 (long long)every.readings);
        snprintf(cells[4], sizeof(cells[4]), "%lld/%lld", (long long)same, (long long)jitters);
        snprintf(cells[5], sizeof(cells[5]), "%lld/%lld", (long long)narrow, (long long)jitters);
        fprintf(out, "  %-6lld %-12s %-12s %-16s %-16s %-14s %s\n", (long long)jitters, cells[0], cells[1], cells[2],
                cells[3], cells[4], cells[5]);
    }
    fprintf(out, "\n  bisection and the full sweep both equal floor(4J/3) at every J: %s. the same offset and\n",
            truth(exact));
    fprintf(out, "  the half spread stay on the coarse reading at every J: %s.\n\n", truth(blind));
    return exact && blind;
}

/**
 * @brief Section two: the phase on the line through the first zero, at every J, against -theta.
 *
 * @param[in] out The stream [BORROWS].
 * @param[in] top The largest J.
 * @return        1 where every point agrees.
 */
static int report_line(FILE *out, int64_t top)
{
    fputs("  the line Re(s) = 1/2 from t = 14 to t = 15, the phase of zeta in turns\n", out);
    int64_t columns[8];
    size_t column_count = 0u;
    for (int64_t power = 1, k = 0; k < 8; k += 1, power *= 4)
    {
        if (power <= top)
        {
            columns[column_count] = power;
            column_count += 1u;
        }
    }
    fprintf(out, "  %-6s ", "t");
    for (size_t at = 0u; at < column_count; at += 1u)
    {
        char label[32];
        snprintf(label, sizeof(label), "J=%lld", (long long)columns[at]);
        fprintf(out, "%s%-10s", (at > 0u) ? " " : "", label);
    }
    fputs("  -theta\n", out);
    int agree = 1;
    const int64_t widest = columns[column_count - 1u];
    for (int64_t step = 0; step < 11; step += 1)
    {
        ZetaPoint point;
        pair_small(5, 1, &point.sigma);
        pair_small(140 + step, 1, &point.t);
        int64_t readings[8];
        for (size_t at = 0u; at < column_count; at += 1u)
        {
            readings[at] = phase_at(&point, columns[at]);
        }
        OriorExactInteger theta;
        theta_at(&point.t, 40, &theta);
        OriorExactInteger half_turn;
        zeta_pi(40, &half_turn);
        OriorExactInteger negative;
        z_neg(&theta, &negative);
        OriorExactInteger wrapped;
        z_mod(&negative, &half_turn, &wrapped);
        z_mul_small(&wrapped, 4 * widest, &wrapped);
        z_floor_div(&wrapped, &half_turn, &wrapped);
        const int64_t index_theta = z_small(&wrapped);
        const int64_t period = 4 * widest;
        const int held = ((((readings[column_count - 1u] % period) + period) % period) == index_theta);
        agree = agree && held;
        fputs("  ", out);
        char t_text[32];
        pair_text(&point.t, 1, t_text, sizeof(t_text));
        fprintf(out, "%-6s ", t_text);
        for (size_t at = 0u; at < column_count; at += 1u)
        {
            if (at > 0u)
            {
                fputs(" ", out);
            }
            print_turns(readings[at], columns[at], out, 10);
        }
        fprintf(out, "  %s\n", held ? "agrees" : "FAIL");
    }
    fprintf(out, "\n  every point agrees with -theta up to a half turn at J = %lld: %s\n\n", (long long)widest,
            truth(agree));
    return agree;
}

/**
 * @brief Section three: the half-turn jumps from t = 14 to t = 26, each with one published zero.
 *
 * @param[in] out     The stream [BORROWS].
 * @param[in] jitters The count J.
 * @return            1 where each flip holds one published zero, and every such zero has a flip.
 */
static int report_flips(FILE *out, int64_t jitters)
{
    fputs("  the flips: steps of a quarter from t = 14 to t = 26 where the phase jumps a half turn\n", out);
    const int64_t quarter = (8 * jitters) / 4;
    DecimalPair lows[64];
    DecimalPair highs[64];
    size_t bracket_count = 0u;
    int has_last = 0;
    DecimalPair last_t;
    int64_t last_index = 0;
    DecimalPair end;
    pair_small(26, 0, &end);
    ZetaPoint point;
    pair_small(5, 1, &point.sigma);
    pair_small(14, 0, &point.t);
    while (pair_below(&point.t, &end) || pair_equal(&point.t, &end))
    {
        const int64_t index = phase_at(&point, jitters);
        if (has_last)
        {
            const int64_t period = 8 * jitters;
            const int64_t jump = (((index - last_index) % period) + period) % period;
            if ((quarter < jump) && (jump < (3 * quarter)))
            {
                if (bracket_count >= 64u)
                {
                    zeta_stop("too many flips");
                }
                lows[bracket_count] = last_t;
                highs[bracket_count] = point.t;
                bracket_count += 1u;
            }
        }
        has_last = 1;
        last_t = point.t;
        last_index = index;
        OriorExactInteger next = point.t.numerator;
        zeta_require(orior_exact_scale_by_ten(&next, (uint32_t)(2 - point.t.places)), "scale");
        OriorExactInteger step;
        z_set(&step, 25);
        z_add(&next, &step, &next);
        pair_make(&next, 2, &point.t);
    }
    int every = 1;
    for (size_t at = 0u; at < bracket_count; at += 1u)
    {
        size_t inside = 0u;
        fputs("  [", out);
        pair_print(&lows[at], 2, out);
        fputs(", ", out);
        pair_print(&highs[at], 2, out);
        fputs("]  holds ", out);
        for (size_t which = 0u; which < 40u; which += 1u)
        {
            DecimalPair ordinate;
            pair_units(ZETA_PUBLISHED_ZEROS[which], &ordinate);
            if (pair_below(&lows[at], &ordinate) && pair_below(&ordinate, &highs[at]))
            {
                fprintf(out, "%s%s", (inside > 0u) ? ", " : "", ZETA_PUBLISHED_ZEROS[which]);
                inside += 1u;
            }
        }
        fputs((inside == 0u) ? "none\n" : "\n", out);
        every = every && (inside == 1u);
    }
    DecimalPair start;
    pair_small(14, 0, &start);
    size_t published = 0u;
    for (size_t which = 0u; which < 40u; which += 1u)
    {
        DecimalPair ordinate;
        pair_units(ZETA_PUBLISHED_ZEROS[which], &ordinate);
        published += (pair_below(&start, &ordinate) && pair_below(&ordinate, &end)) ? 1u : 0u;
    }
    every = every && (bracket_count == published);
    fprintf(out, "\n  %llu flips, each holding one of the %llu published zeros in the span: %s\n",
            (unsigned long long)bracket_count, (unsigned long long)published, truth(every));
    return every;
}

int main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *const out = stdout;
    const int64_t top = (argument_count > 1) ? atoll(arguments[1]) : 64;
    fputs("  the phase of zeta read finer than its reader, by evenly spaced jitter and Hermite's identity\n", out);
    fputs("  it claims nothing about the Riemann hypothesis\n\n", out);
    const int control = report_control(out, top);
    const int line = report_line(out, top);
    const int flips = report_flips(out, 16);
    fflush(out);
    return (control && line && flips) ? 0 : 1;
}
