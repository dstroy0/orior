// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-023
/**
 * @file exact_zeta_riemann_siegel.c
 * @brief Z(t) by the formula of Riemann and Siegel, by truthy and falsy verdicts on exact integers.
 *
 * @note Time is a real, and the delta between 0 and 1 stays free.
 * @note Run build_exact_zeta.sh riemann_siegel [height] [bits], beside this file. The modes are triangle, set,
 *       motion, sphere and omitted, each with its cells first and last after its name.
 * @note This reads no corpus. It sits in 0_experimental, and is an entry in the workbook of analytic number theory,
 *       on its rail. It claims nothing about the Riemann hypothesis.
 * @note The formula, as MathWorld prints it: Z(t) = 2 sum over n <= N of n^(-1/2) cos(theta(t) - t ln n) + R(t).
 *       N is the floor of sqrt(t / 2pi), and p = sqrt(t / 2pi) - N.
 * @note R(t) = (-1)^(N-1) (t / 2pi)^(-1/4) times the sum of c_k(p) (t / 2pi)^(-k/2). Each c_k is a sum of the
 *       derivatives of Psi(p) = cos 2pi(p^2 - p - 1/16) / cos 2pi p over powers of pi. MathWorld prints c_0 to c_5.
 * @note Time as a real: the steer is u, and t = 2pi u^4. The code takes no square root, and divides nothing by 2pi.
 *       N is the floor of u^2, and p = u^2 - N exactly. (t / 2pi)^(-1/4 - k/2) = u^-(2k+1), an exact rational.
 * @note The code holds t as its real and its operator: the floor of pi at the places asked, and pi asked again for
 *       more. The delta p is between 0 and 1, and is never fixed. A deeper reading asks it for more places.
 * @note Two magnitudes for one phase: each term of the sum is the pair n^(-1/2) e^(-it ln n). The phase theta turns
 *       the summed pair by e^(i theta). The code holds Psi as the pair of its numerator and its denominator, each a
 *       power series about p, and never divides them.
 * @note With d the first coefficient of the denominator, Psi^(j)(p) / j! = r_j / d^(j+1). r_j comes from the two
 *       series by products alone. The code carries each c_k times d^16 pi^10, a multiplier that is never negative,
 *       and Z with it. The sign of Z is the sign of the carried product.
 * @note Where d reads zero at the work places, both series lose their first coefficient, and the next one comes in.
 *       The numerator vanishes where the denominator does, at p = 1/4 and p = 3/4.
 * @note The verdicts at a point: theta comes by the two routes of zeta_theta.c, with N doubled where they disagree.
 *       The carried Z reads toward zero at the places asked, and a reading of zero doubles the places.
 * @note The last printed term, c_5 u^-11, reads against Z at the same places. The reading tells whether Z reads past
 *       the own last term of the series there. That is a reading, recorded per point, and the series sets its own
 *       reach at that t.
 * @note The walk: the walk and the placement of zeta_gram_walk.c, with steps in u and theta read at t = 2pi u^4. A
 *       bracket settles where Z has one nonzero sign at both ends. Else the Gram index at its midpoint cuts it.
 * @note The boundary between the two pairs: Riemann-Siegel and Euler-Maclaurin both give Z. Euler-Maclaurin works
 *       over the whole strip, and its two routes meet as N doubles. Riemann-Siegel works on the line, as a series in
 *       u that stops at c_5.
 * @note Inside Riemann-Siegel, the main sum S and the remainder R split Z between them at nu = floor(x). Here
 *       x = u^2 = sqrt(t / 2pi) and t = 2pi x^2. The domain is x, cut into cells [nu, nu + 1]. On each cell,
 *       p = x - nu runs over [0, 1]. Psi is symmetric, Psi(1 - p) = Psi(p), and c_k(1) = (-1)^k c_k(0).
 * @note Where x crosses nu + 1, S gains the term 2 (nu + 1)^(-1/2) cos(theta - t ln(nu + 1)). To its first order,
 *       that is 2 (-1)^(nu+1) cos(pi/8) / u.
 * @note R turns from (-1)^(nu-1) sum c_k(1) u^-(2k+1) to (-1)^nu sum c_k(0) u^-(2k+1). That is a jump of
 *       2 (-1)^nu sum over even k of c_k(0) u^-(2k+1). The two jumps meet, and Z is continuous. The rest of them is
 *       the jump of the terms that R leaves out, the seam that seam() reads.
 * @note Each C_n, kept as its real and its operator: with z = 1 - 2p and F(z) = Psi(p), the generator of Gabcke
 *       gives each coefficient. That is Table III and Satz 2.1.4 of his thesis.
 * @note C_n(z) = 2^(-2n) times the sum over k of d_k^(n) F^(3n-4k)(z) / ((3n - 4k)! pi^(2n-2k)). The rows follow
 *       d_k^(n+1) = (3n + 1 - 4k)(3n + 2 - 4k) d_k^(n) + d_(k-1)^(n), except d_3l^(4l) = lambda_l.
 * @note (l + 1) lambda_(l+1) = sum over k of 2^(4k+1) |E_(2k+2)| lambda_(l-k), on the Euler numbers. Each d is an
 *       integer. F is entire and even, F(z) = sum phi_j z^(2j). Each phi_j is the product of the series of
 *       cos(pi z^2 / 2 + 3pi/8) and of sec(pi z).
 * @note Those are rationals times pi^(2j - m) times sin(pi/8) or cos(pi/8). The two are sqrt(2 -+ sqrt 2) / 2. The
 *       code thus computes each C_n exactly. Only a reading at a point asks for digits.
 * @note A reading of C_n at a dyadic z sums its Taylor series on count and 2 count terms, the second at twice the
 *       extra digits. The two must agree at the places asked. Disagreement doubles count, and a reading of zero
 *       doubles the places. The terms of the sec series grow as 4^j, and cancel to those of F.
 * @note The zeros of C_n on [0, 1] are the sign changes read on 2^m and 2^(m+1) parts. The count m grows until the
 *       two counts agree, and the code then halves each zero to the bits asked.
 * @note An odd C_n is zero at 0 by parity, and its sign just past 0 is the sign of its z coefficient. Each zero z
 *       gives p = (1 - z) / 2 and (1 + z) / 2. The term C_n u^-(2n+1) vanishes at t = 2pi (N + p)^2 in each cell N.
 * @note Positive control: the Gram points g_0 to g_15 against the article on the theta function on Wikipedia, each
 *       compared with 2pi u^4 by multiplication. Also the first failure of the law of Gram at n = 126, as that
 *       article reports it. Also Z against the Re e^(i theta) zeta of Euler-Maclaurin at the same real t, the gap in
 *       units of the last place.
 * @note For each C_n: C_0 to C_5 against the c_0 to c_5 of MathWorld, rational for rational. d_k^(8) against Table
 *       II of Gabcke. C_0(1) to C_10(1) against the sum row of his Table IV at 50 places, within the spread between
 *       his Table IV and Table V. One simple zero each in C_4, C_8 and C_10 on 0 < z < 1, as he reports on p. 59.
 * @note Drawn null: R left out, and the Gram signs and the gap read again.
 */
#include "zeta_gram_walk.h"
#include "zeta_riemann_siegel.h"
#include "zeta_shuffle.h"
#include "zeta_theta.h"

#include <dirent.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The points u of the table of gaps, each a decimal pair numerator over 10. */
static const int64_t POINTS[6] = {12, 15, 18, 21, 24, 26};

/** @brief Gabcke, Table II: d_k^(8) as printed, in primes. */
static const int64_t TABLE_II_ROW_8[7] = {32768LL * 125 * 49 * 121 * 13 * 17 * 19 * 23,
                                          16384LL * 25 * 49 * 11 * 13 * 17 * 19 * 23,
                                          2048LL * 5 * 7 * 11 * 13 * 19 * 587,
                                          512LL * 11 * 88651,
                                          16LL * 25 * 5281,
                                          8LL * 3 * 7 * 61,
                                          2LL * 41};

/** @brief Gabcke, Table IV, the sum row under each C_n: C_n(1) at 50 places, as printed. */
static const char *const TABLE_IV_AT_ONE[11] = {
    "0.92387953251128675612818318939678828682241662586366",  "-0.03059730649970626546068192245966280080837903996856",
    "0.00126887416458910500666051884940376436169297025221",  "-0.00019868520940530243222929405399643439118283446391",
    "-0.00000507858983165002468419342114936835424875429909", "-0.00007396543141241629733408848615411272753191792498",
    "0.00000187256420886912251262001682842629296424868174",  "-0.00001001778245912224981642747644926784077639093882",
    "-0.00000001500360185507754523780375237281525837598888", "-0.00000248337616645216969898765435537919977772834558",
    "0.00000001501251599634484404172738666926505568514695"};

/** @brief The sum rows of Table IV and Table V are two routes to the same C_n(1). They differ by up to 5 units of the
 *         50th place, and that is the bar. */
#define TABLE_SPREAD 5

/** @brief The last curve whose zeros the report reads. */
#define TOP_C 24

/** @brief The bits of each zero of a curve. */
#define ZERO_BITS 128

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
static void *siegel_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief The milliseconds of the wall clock, for the times that the report prints.
 *
 * @return The milliseconds.
 */
static int64_t now_ms(void)
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return ((int64_t)now.tv_sec * 1000) + ((int64_t)now.tv_nsec / 1000000);
}

/**
 * @brief The sign of an integer: -1, 0 or 1.
 *
 * @param[in] value The integer [BORROWS].
 * @return          The sign.
 */
static int sign_of(const OriorExactInteger *value)
{
    return value->sign;
}

/**
 * @brief Writes a list of integers as Python prints a list, into a buffer.
 *
 * @param[in]  values   The values [BORROWS].
 * @param[in]  count    How many.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void list_text(const OriorExactInteger *values, size_t count, char *text, size_t capacity)
{
    size_t used = (size_t)snprintf(text, capacity, "[");
    for (size_t at = 0u; at < count; at += 1u)
    {
        char number[8192];
        z_text(&values[at], number, sizeof(number));
        used += (size_t)snprintf(text + used, capacity - used, "%s%s", (at > 0u) ? ", " : "", number);
    }
    snprintf(text + used, capacity - used, "]");
}

/* ---------------------------------------------------------------- time as a real */

/**
 * @brief t = 2pi u^4 at the digits: the floor of pi at those places times 2 u^4.
 *
 * @param[in]  u      The steer [BORROWS].
 * @param[in]  digits The places.
 * @param[out] t      The point [BORROWS].
 */
static void time_of(const DecimalPair *u, int32_t digits, DecimalPair *t)
{
    OriorExactInteger value;
    z_mul(&u->numerator, &u->numerator, &value);
    z_mul(&value, &value, &value);
    z_mul_small(&value, 2, &value);
    OriorExactInteger big_pi;
    zeta_pi(digits, &big_pi);
    z_mul(&value, &big_pi, &value);
    OriorExactInteger under;
    z_ten_to(4 * u->places, &under);
    z_floor_div(&value, &under, &t->numerator);
    t->places = digits;
}

/**
 * @brief The two routes of theta at t = 2pi u^4, for the walk.
 *
 * @param[in]  u      The steer [BORROWS].
 * @param[in]  places The places.
 * @param[in]  n_sum  The cut N.
 * @param[out] one    The first route [BORROWS].
 * @param[out] two    The second route [BORROWS].
 */
static void u_phase(const DecimalPair *u, int32_t places, int64_t n_sum, OriorExactInteger *one, OriorExactInteger *two)
{
    DecimalPair t;
    time_of(u, places + ZETA_GUARD, &t);
    theta_routes(&t, places, n_sum, 1, one, two);
}

/* ---------------------------------------------------------------- Z at a point */

/** @brief Z at a point u, carried, with the last term, the multiplier, and the widths of the work. */
typedef struct
{
    OriorExactInteger total;      /**< Z d^16 pi^10. */
    OriorExactInteger last;       /**< c_5 u^-11 times the same. */
    OriorExactInteger multiplier; /**< d^16 pi^10. */
    int64_t n_top;                /**< The terms of the main sum. */
} CarriedZ;

/**
 * @brief Z d^16 pi^10, the last term c_5 u^-11 times the same, and that multiplier, at places + ZETA_GUARD.
 *
 * @param[in]  u         The steer [BORROWS].
 * @param[in]  places    The places.
 * @param[in]  remainder 1 to keep R, 0 to leave it out.
 * @param[out] carried   The values [BORROWS].
 * @note The work runs at places + 2 ZETA_GUARD, and the code drops one guard toward zero.
 */
static void z_carried(const DecimalPair *u, int32_t places, int remainder, CarriedZ *carried)
{
    const int32_t digits = places + (2 * ZETA_GUARD);
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger unit;
    z_ten_to(u->places, &unit);
    OriorExactInteger square;
    z_mul(&u->numerator, &u->numerator, &square);
    OriorExactInteger unit_square;
    z_mul(&unit, &unit, &unit_square);
    OriorExactInteger top;
    z_floor_div(&square, &unit_square, &top);
    const int64_t n_top = z_small(&top);
    OriorExactInteger delta;
    z_mul(&top, &unit_square, &delta);
    z_sub(&square, &delta, &delta);
    DecimalPair p;
    pair_make(&delta, 2 * u->places, &p);
    OriorExactInteger inv_root;
    z_mul(&scale, &unit, &inv_root);
    z_floor_div(&inv_root, &u->numerator, &inv_root);
    OriorExactInteger main_sum;
    OriorExactInteger tail[6];
    OriorExactInteger multiplier;
    (void)rs_parts(n_top, &p, &inv_root, digits, &main_sum, tail, &multiplier);
    OriorExactInteger total;
    z_mul(&main_sum, &multiplier, &total);
    z_floor_div(&total, &scale, &total);
    if (remainder)
    {
        for (int k = 0; k < 6; k += 1)
        {
            z_add(&total, &tail[k], &total);
        }
    }
    rs_toward_zero(&total, &carried->total);
    if (remainder)
    {
        rs_toward_zero(&tail[5], &carried->last);
    }
    else
    {
        orior_exact_zero(&carried->last);
    }
    rs_toward_zero(&multiplier, &carried->multiplier);
    carried->n_top = n_top;
}

/** @brief The sign of Z at each point u, the reach of its last term there, and the cost of those verdicts. */
typedef struct
{
    int remainder;     /**< 1 to keep R, 0 to leave it out. */
    GramSearch points; /**< The points, interned. */
    int *sign;         /**< The sign at each point, 0 where none [OWNS]. */
    int *reach;        /**< The reach at each point [OWNS]. */
    size_t capacity;   /**< How many points the arrays hold. */
    uint64_t asked;    /**< How many values of Z the sweeps asked for. */
    int32_t deepest;   /**< The most places at a settled point. */
    int64_t widest;    /**< The most terms of the main sum at a settled point. */
} Riemann;

/**
 * @brief The index of a point, with room in the arrays for it.
 *
 * @param[in,out] riemann The verdicts [BORROWS].
 * @param[in]     u       The point [BORROWS].
 * @return                The index.
 */
static size_t riemann_point(Riemann *riemann, const DecimalPair *u)
{
    const size_t index = gram_intern(&riemann->points, u);
    if (index >= riemann->capacity)
    {
        const size_t grown = (riemann->capacity == 0u) ? 256u : (riemann->capacity * 2u);
        riemann->sign = (int *)siegel_memory(realloc(riemann->sign, grown * sizeof(int)));
        riemann->reach = (int *)siegel_memory(realloc(riemann->reach, grown * sizeof(int)));
        memset(riemann->sign + riemann->capacity, 0, (grown - riemann->capacity) * sizeof(int));
        memset(riemann->reach + riemann->capacity, 0, (grown - riemann->capacity) * sizeof(int));
        riemann->capacity = grown;
    }
    return index;
}

/**
 * @brief Decides the sign of Z at each point, pass by pass. A reading of zero doubles the places.
 *
 * @param[in,out] riemann The verdicts [BORROWS].
 * @param[in]     indices The points [BORROWS].
 * @param[in]     count   How many.
 */
static void riemann_sweep(Riemann *riemann, const size_t *indices, size_t count)
{
    size_t *const open = (size_t *)siegel_memory(malloc((count + 1u) * sizeof(size_t)));
    int32_t *const places = (int32_t *)siegel_memory(malloc((count + 1u) * sizeof(int32_t)));
    unsigned char *const seen = (unsigned char *)siegel_memory(calloc(riemann->points.count + 1u, 1u));
    size_t open_count = 0u;
    for (size_t at = 0u; at < count; at += 1u)
    {
        if (!seen[indices[at]] && (riemann->sign[indices[at]] == 0))
        {
            open[open_count] = indices[at];
            places[open_count] = 1;
            open_count += 1u;
        }
        seen[indices[at]] = 1u;
    }
    free(seen);
    while (open_count > 0u)
    {
        size_t next = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            riemann->asked += 1u;
            CarriedZ carried;
            z_carried(&riemann->points.points[open[at]].t, places[at], riemann->remainder, &carried);
            OriorExactInteger read;
            OriorExactInteger read_last;
            rs_toward_zero(&carried.total, &read);
            rs_toward_zero(&carried.last, &read_last);
            if (read.sign != 0)
            {
                OriorExactInteger first;
                OriorExactInteger second;
                z_abs(&read, &first);
                z_abs(&read_last, &second);
                riemann->sign[open[at]] = read.sign;
                riemann->reach[open[at]] = z_cmp(&first, &second);
                riemann->deepest = (places[at] > riemann->deepest) ? places[at] : riemann->deepest;
                riemann->widest = (carried.n_top > riemann->widest) ? carried.n_top : riemann->widest;
                continue;
            }
            open[next] = open[at];
            places[next] = 2 * places[at];
            next += 1u;
        }
        open_count = next;
    }
    free(open);
    free(places);
}

/**
 * @brief Cuts each bracket by the Gram index until Z has one nonzero sign at both ends.
 *
 * @param[in,out] riemann  The verdicts [BORROWS].
 * @param[in]     brackets The placed brackets [BORROWS].
 * @param[in]     count    How many.
 * @param[in,out] walk     The search of the Gram index [BORROWS].
 * @param[out]    signs    The sign of (-1)^n Z at each index [BORROWS].
 * @param[out]    settled  The settled bracket at each index [BORROWS].
 * @return                 How many further cuts.
 */
static size_t riemann_settle(Riemann *riemann, const GramBracket *brackets, size_t count, GramSearch *walk, int *signs,
                             GramBracket *settled)
{
    GramBracket *const open = (GramBracket *)siegel_memory(malloc((count + 1u) * sizeof(GramBracket)));
    memcpy(open, brackets, count * sizeof(GramBracket));
    size_t open_count = count;
    size_t cuts = 0u;
    while (open_count > 0u)
    {
        size_t *const ends = (size_t *)siegel_memory(malloc((2u * open_count + 1u) * sizeof(size_t)));
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            ends[2u * at] = riemann_point(riemann, &open[at].low);
            ends[(2u * at) + 1u] = riemann_point(riemann, &open[at].high);
        }
        riemann_sweep(riemann, ends, 2u * open_count);
        size_t next = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            const int low = riemann->sign[ends[2u * at]];
            const int high = riemann->sign[ends[(2u * at) + 1u]];
            if (low == high)
            {
                const int64_t n = open[at].index;
                signs[n] = low * (((n % 2) == 0) ? 1 : -1);
                settled[n] = open[at];
                continue;
            }
            open[next] = open[at];
            next += 1u;
        }
        free(ends);
        gram_cut(walk, open, next);
        open_count = next;
        cuts += next;
    }
    free(open);
    return cuts;
}

/* ---------------------------------------------------------------- comparisons by multiplication */

/**
 * @brief The verdict that 2pi u^4 >= value, or 2pi u^4 <= value, by products against the floor of pi at 40 places.
 *
 * @param[in] u     The steer [BORROWS].
 * @param[in] value The value [BORROWS].
 * @param[in] above 1 for 2pi u^4 >= value with the floor of pi, 0 for 2pi u^4 <= value with the floor plus one.
 * @return          The verdict.
 */
static int compare_time(const DecimalPair *u, const DecimalPair *value, int above)
{
    const int32_t digits = 40;
    OriorExactInteger left;
    z_mul(&u->numerator, &u->numerator, &left);
    z_mul(&left, &left, &left);
    z_mul_small(&left, 2, &left);
    OriorExactInteger big_pi;
    zeta_pi(digits, &big_pi);
    if (!above)
    {
        OriorExactInteger one;
        z_set(&one, 1);
        z_add(&big_pi, &one, &big_pi);
    }
    z_mul(&left, &big_pi, &left);
    OriorExactInteger power;
    z_ten_to(value->places, &power);
    z_mul(&left, &power, &left);
    OriorExactInteger right;
    z_ten_to((4 * u->places) + digits, &power);
    z_mul(&value->numerator, &power, &right);
    const int order = z_cmp(&left, &right);
    return above ? (order >= 0) : (order <= 0);
}

/**
 * @brief Whether [2pi lo^4, 2pi h^4] meets the own bracket of the published value, half a unit in its last place.
 *
 * @param[in] low       The lower end [BORROWS].
 * @param[in] high      The upper end [BORROWS].
 * @param[in] published The value [BORROWS].
 * @return              The verdict.
 */
static int holds(const DecimalPair *low, const DecimalPair *high, const char *published)
{
    DecimalPair value;
    pair_units(published, &value);
    DecimalPair unit;
    pair_small(5, value.places + 1, &unit);
    DecimalPair plus;
    DecimalPair minus;
    pair_plus(&value, &unit, &plus);
    pair_minus(&value, &unit, &minus);
    return compare_time(low, &plus, 0) && compare_time(high, &minus, 1);
}

/**
 * @brief The least u at 4 places with 2pi u^4 >= height, one bit a pass, by products.
 *
 * @param[in]  height The height [BORROWS].
 * @param[out] u      The steer [BORROWS].
 */
static void u_of(const DecimalPair *height, DecimalPair *u)
{
    const int32_t places = 4;
    int64_t low = 0;
    int64_t high = 10000;
    DecimalPair probe;
    pair_small(high, places, &probe);
    probe.places = places;
    z_set(&probe.numerator, high);
    while (!compare_time(&probe, height, 1))
    {
        high *= 2;
        z_set(&probe.numerator, high);
    }
    while ((high - low) > 1)
    {
        const int64_t mid = (low + high) / 2;
        z_set(&probe.numerator, mid);
        if (compare_time(&probe, height, 1))
        {
            high = mid;
        }
        else
        {
            low = mid;
        }
    }
    pair_small(high, places, u);
}

/**
 * @brief Re e^(i theta) zeta(1/2 + it) at t = 2pi u^4, by the two routes of Euler-Maclaurin, read at the places.
 *
 * @param[in]  u      The steer [BORROWS].
 * @param[in]  places The places.
 * @param[out] result The reading [BORROWS].
 */
static void euler_maclaurin(const DecimalPair *u, int32_t places, OriorExactInteger *result)
{
    const int32_t digits = places + ZETA_GUARD;
    DecimalPair t;
    time_of(u, digits + ZETA_GUARD, &t);
    OriorExactInteger theta;
    (void)rs_theta_at(&t, digits + ZETA_GUARD, &theta);
    OriorExactInteger cosine;
    OriorExactInteger sine;
    zeta_cos_sin(&theta, digits + ZETA_GUARD, &cosine, &sine);
    OriorExactInteger guard;
    z_ten_to(ZETA_GUARD, &guard);
    z_floor_div(&cosine, &guard, &cosine);
    z_floor_div(&sine, &guard, &sine);
    ZetaPoint point;
    pair_small(5, 1, &point.sigma);
    point.t = t;
    OriorExactInteger value[2];
    static OriorExactInteger gaps[64];
    size_t count = 0u;
    (void)zeta_agreed_value(&point, digits, value, gaps, &count);
    OriorExactInteger first;
    OriorExactInteger second;
    z_mul(&cosine, &value[0], &first);
    z_mul(&sine, &value[1], &second);
    z_sub(&first, &second, &first);
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    z_floor_div(&first, &scale, &first);
    rs_toward_zero(&first, result);
}

/**
 * @brief Z by Riemann-Siegel less Z by Euler-Maclaurin at t = 2pi u^4, in units of the last place.
 *
 * @param[in]  u         The steer [BORROWS].
 * @param[in]  places    The places.
 * @param[in]  remainder 1 to keep R, 0 to leave it out.
 * @param[in]  em        Z by Euler-Maclaurin at the places [BORROWS].
 * @param[out] result    The gap [BORROWS].
 */
static void gap_at(const DecimalPair *u, int32_t places, int remainder, const OriorExactInteger *em,
                   OriorExactInteger *result)
{
    CarriedZ carried;
    z_carried(u, places, remainder, &carried);
    OriorExactInteger value = carried.total;
    zeta_require(orior_exact_scale_by_ten(&value, (uint32_t)(places + ZETA_GUARD)), "scale");
    z_floor_div(&value, &carried.multiplier, &value);
    rs_toward_zero(&value, &value);
    z_sub(&value, em, result);
}

/* ---------------------------------------------------------------- where each C_n vanishes */

/** @brief A sign kept by its text key. */
typedef struct
{
    char *key; /**< The key [OWNS]. */
    int sign;  /**< The sign. */
} SignEntry;

/** @brief Where each C_n changes sign on [0, 1], by verdicts at dyadic z. */
typedef struct
{
    SignEntry *entries;  /**< The open table [OWNS]. */
    size_t bucket_count; /**< How many buckets, a power of two. */
    size_t count;        /**< How many entries. */
} CurveSigns;

/**
 * @brief The hash of a text, by FNV-1a.
 *
 * @param[in] text The text [BORROWS].
 * @return         The hash.
 */
static uint64_t key_hash(const char *text)
{
    uint64_t hash = 1469598103934665603u;
    for (const char *at = text; *at != '\0'; at += 1)
    {
        hash = (hash ^ (uint64_t)(unsigned char)*at) * 1099511628211u;
    }
    return hash;
}

/**
 * @brief The entry of a key, the table grown where it fills.
 *
 * @param[in,out] signs The table [BORROWS].
 * @param[in]     key   The key [BORROWS].
 * @return              The entry [BORROWS].
 */
static SignEntry *curve_sign_entry(CurveSigns *signs, const char *key)
{
    if ((signs->count * 2u) >= signs->bucket_count)
    {
        const size_t grown = (signs->bucket_count == 0u) ? 4096u : (signs->bucket_count * 2u);
        SignEntry *const entries = (SignEntry *)siegel_memory(calloc(grown, sizeof(SignEntry)));
        for (size_t at = 0u; at < signs->bucket_count; at += 1u)
        {
            if (signs->entries[at].key == NULL)
            {
                continue;
            }
            size_t place = (size_t)(key_hash(signs->entries[at].key) & (grown - 1u));
            while (entries[place].key != NULL)
            {
                place = (place + 1u) & (grown - 1u);
            }
            entries[place] = signs->entries[at];
        }
        free(signs->entries);
        signs->entries = entries;
        signs->bucket_count = grown;
    }
    size_t place = (size_t)(key_hash(key) & (signs->bucket_count - 1u));
    while ((signs->entries[place].key != NULL) && (strcmp(signs->entries[place].key, key) != 0))
    {
        place = (place + 1u) & (signs->bucket_count - 1u);
    }
    return &signs->entries[place];
}

/**
 * @brief The sign of C_n at a / 2^bits, with a reduced while it is even.
 *
 * @param[in,out] signs The table [BORROWS].
 * @param[in]     n     The index of the curve.
 * @param[in]     a     The numerator [BORROWS].
 * @param[in]     bits  The bits of the denominator.
 * @return              The sign.
 */
static int sign_at(CurveSigns *signs, int64_t n, const OriorExactInteger *a, int64_t bits)
{
    OriorExactInteger value = *a;
    OriorExactInteger two;
    z_set(&two, 2);
    while ((bits > 0) && ((value.limb[0] & 1u) == 0u))
    {
        z_floor_div(&value, &two, &value);
        bits -= 1;
    }
    static char digits[8192];
    z_text(&value, digits, sizeof(digits));
    char key[8300];
    snprintf(key, sizeof(key), "%lld/%s/%lld", (long long)n, digits, (long long)bits);
    SignEntry *const entry = curve_sign_entry(signs, key);
    if ((entry->key != NULL) && (entry->sign != 0))
    {
        return entry->sign;
    }
    int32_t places = 24;
    OriorExactInteger read;
    rs_c_verdict(n, &value, bits, &places, &read);
    if (entry->key == NULL)
    {
        entry->key = (char *)siegel_memory(malloc(strlen(key) + 1u));
        memcpy(entry->key, key, strlen(key) + 1u);
        signs->count += 1u;
    }
    entry->sign = sign_of(&read);
    return entry->sign;
}

/**
 * @brief The sign of an odd C_n just past 0: the sign of its z coefficient, read at digits and twice them.
 *
 * @param[in] n The index of the curve.
 * @return      The sign.
 * @note The two readings must agree and read nonzero.
 */
static int past_zero(int64_t n)
{
    int32_t digits = 40;
    for (;;)
    {
        OriorExactInteger readings[2];
        for (int which = 0; which < 2; which += 1)
        {
            ZetaCurve curve;
            rs_curve_begin(&curve, digits << which);
            ZetaRow row;
            rs_curve_gamma(&curve, n, 2, &row);
            rs_read(&row.values[1], digits << which, digits / 2, &readings[which]);
            row_free(&row);
            rs_curve_free(&curve);
        }
        digits *= 2;
        if (orior_exact_equal(&readings[0], &readings[1]) && (readings[1].sign != 0))
        {
            return readings[1].sign;
        }
    }
}

/**
 * @brief The signs of C_n at j / 2^m for j = 0 to 2^m.
 *
 * @param[in,out] signs The table [BORROWS].
 * @param[in]     n     The index of the curve.
 * @param[in]     m     The bits of the parts.
 * @param[out]    row   The signs [BORROWS]. 2^m + 1 of them.
 */
static void sign_row(CurveSigns *signs, int64_t n, int64_t m, int *row)
{
    OriorExactInteger a;
    orior_exact_zero(&a);
    row[0] = ((n % 2) != 0) ? past_zero(n) : sign_at(signs, n, &a, m);
    for (int64_t j = 1; j <= ((int64_t)1 << m); j += 1)
    {
        z_set(&a, j);
        row[j] = sign_at(signs, n, &a, m);
    }
}

/**
 * @brief The places where a row of signs changes: j where row[j] differs from row[j + 1].
 *
 * @param[in]  row     The signs [BORROWS].
 * @param[in]  count   How many signs.
 * @param[out] changes The places [BORROWS], or NULL for the count alone.
 * @return             How many changes.
 */
static size_t sign_changes(const int *row, size_t count, int64_t *changes)
{
    size_t found = 0u;
    for (size_t j = 0u; (j + 1u) < count; j += 1u)
    {
        if (row[j] != row[j + 1u])
        {
            if (changes != NULL)
            {
                // The place is below the count of the row, and fits a machine integer.
                changes[found] = (int64_t)j;
            }
            found += 1u;
        }
    }
    return found;
}

/**
 * @brief The zeros of C_n on [0, 1]: the sign changes on 2^m and 2^(m+1) parts, each halved to the bits.
 *
 * @param[in,out] signs The table [BORROWS].
 * @param[in]     n     The index of the curve.
 * @param[in]     bits  The bits of each zero.
 * @param[out]    found The left ends, each a over 2^bits [BORROWS]. At most 64.
 * @param[out]    parts The m of the finer row, m + 1 [BORROWS].
 * @return              How many zeros.
 * @note The count m grows from 6 until the two counts agree.
 */
static size_t curve_zeros(CurveSigns *signs, int64_t n, int64_t bits, OriorExactInteger *found, int64_t *parts)
{
    int64_t m = 6;
    int *row = (int *)siegel_memory(malloc((((size_t)1 << m) + 1u) * sizeof(int)));
    int *finer = (int *)siegel_memory(malloc((((size_t)1 << (m + 1)) + 1u) * sizeof(int)));
    sign_row(signs, n, m, row);
    sign_row(signs, n, m + 1, finer);
    while (sign_changes(row, ((size_t)1 << m) + 1u, NULL) != sign_changes(finer, ((size_t)1 << (m + 1)) + 1u, NULL))
    {
        m += 1;
        free(row);
        row = finer;
        finer = (int *)siegel_memory(malloc((((size_t)1 << (m + 1)) + 1u) * sizeof(int)));
        sign_row(signs, n, m + 1, finer);
    }
    const size_t length = ((size_t)1 << (m + 1)) + 1u;
    int64_t *const changes = (int64_t *)siegel_memory(malloc(length * sizeof(int64_t)));
    const size_t count = sign_changes(finer, length, changes);
    if (count > 64u)
    {
        zeta_stop("more than 64 zeros of one curve");
    }
    for (size_t at = 0u; at < count; at += 1u)
    {
        OriorExactInteger low;
        z_set(&low, changes[at]);
        int64_t depth = m + 1;
        const int left = sign_at(signs, n, &low, depth);
        while (depth < bits)
        {
            z_mul_small(&low, 2, &low);
            depth += 1;
            OriorExactInteger probe;
            OriorExactInteger one;
            z_set(&one, 1);
            z_add(&low, &one, &probe);
            if (sign_at(signs, n, &probe, depth) == left)
            {
                low = probe;
            }
        }
        found[at] = low;
    }
    free(changes);
    free(row);
    free(finer);
    *parts = m + 1;
    return count;
}

/**
 * @brief The count of sign changes on [1 - 2^-6, 1] at 2^parts and 2^(parts+1) parts of [0, 1].
 *
 * @param[in,out] signs  The table [BORROWS].
 * @param[in]     n      The index of the curve.
 * @param[out]    counts The two counts [BORROWS].
 * @note The zeros of the even C_n crowd toward z = 1. Two zeros between adjacent points of the coarse grid do not
 *       show there.
 */
static void curve_edge(CurveSigns *signs, int64_t n, int64_t counts[2])
{
    for (int64_t m = 14; m <= 15; m += 1)
    {
        const int64_t base = ((int64_t)1 << m) - ((int64_t)1 << (m - 6));
        const size_t length = ((size_t)1 << (m - 6)) + 1u;
        int *const row = (int *)siegel_memory(malloc(length * sizeof(int)));
        for (size_t j = 0u; j < length; j += 1u)
        {
            OriorExactInteger a;
            // j is below 2^9, and fits a machine integer.
            z_set(&a, base + (int64_t)j);
            row[j] = sign_at(signs, n, &a, m);
        }
        // The count of changes is below the length of the row.
        counts[m - 14] = (int64_t)sign_changes(row, length, NULL);
        free(row);
    }
}

/**
 * @brief a / 2^bits at the places, as "0." and the digits, zero-padded.
 *
 * @param[in]  a        The numerator [BORROWS].
 * @param[in]  bits     The bits of the denominator.
 * @param[in]  places   The places.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void dyadic(const OriorExactInteger *a, int64_t bits, int32_t places, char *text, size_t capacity)
{
    OriorExactInteger value = *a;
    zeta_require(orior_exact_scale_by_ten(&value, (uint32_t)places), "scale");
    OriorExactInteger power;
    z_two_to(bits, &power);
    z_floor_div(&value, &power, &value);
    char digits[4096];
    z_text(&value, digits, sizeof(digits));
    const size_t length = strlen(digits);
    // The places are small, and the pad fits an int.
    const int pad = (length < (size_t)places) ? (int)((size_t)places - length) : 0;
    if (pad == 0)
    {
        snprintf(text, capacity, "0.%.200s", digits);
    }
    else
    {
        size_t used = (size_t)snprintf(text, capacity, "0.");
        for (int at = 0; (at < pad) && ((used + 1u) < capacity); at += 1)
        {
            text[used] = '0';
            used += 1u;
        }
        snprintf(text + used, capacity - used, "%.200s", digits);
    }
}

/**
 * @brief The jumps of S, of R and of Z_RS where x crosses nu + 1, each at the digits.
 *
 * @param[in]  nu     The cell.
 * @param[in]  digits The places.
 * @param[out] jump   The jump of Z_RS [BORROWS].
 * @note From inside the cell, S runs to nu with p = 1. From the next cell, it runs to nu + 1 with p = 0. Z is
 *       continuous there. The jump of Z_RS is the jump of the terms that R leaves out.
 */
static void seam(int64_t nu, int32_t digits, OriorExactInteger *jump)
{
    DecimalPair one;
    DecimalPair zero;
    pair_small(1, 0, &one);
    one.places = 0;
    z_set(&one.numerator, 1);
    zero.places = 0;
    orior_exact_zero(&zero.numerator);
    OriorExactInteger s_in;
    OriorExactInteger r_in;
    OriorExactInteger s_out;
    OriorExactInteger r_out;
    rs_at(nu, &one, digits, &s_in, &r_in);
    rs_at(nu + 1, &zero, digits, &s_out, &r_out);
    z_sub(&s_out, &s_in, jump);
    z_add(jump, &r_out, jump);
    z_sub(jump, &r_in, jump);
}

/**
 * @brief The seam of Z_RS at x = nu + 1, and the jump of the terms that R leaves out, from the exact C_k(1).
 *
 * @param[in]  nu        The cell.
 * @param[out] jump      The seam [BORROWS].
 * @param[out] predicted The jump from the curves [BORROWS].
 * @note The jump is -2 (-1)^nu sum over even k from 6 to 16 of C_k(1) u^-(2k+1), with u = sqrt(nu + 1), at 20
 *       places.
 */
static void seam_against_c(int64_t nu, OriorExactInteger *jump, OriorExactInteger *predicted)
{
    const int32_t digits = 20;
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger square;
    z_mul(&scale, &scale, &square);
    z_mul_small(&square, nu + 1, &square);
    OriorExactInteger root;
    z_isqrt(&square, &root);
    orior_exact_zero(predicted);
    OriorExactInteger one;
    z_set(&one, 1);
    for (int64_t k = 6; k <= 16; k += 2)
    {
        OriorExactInteger inv = scale;
        for (int64_t at = 0; at < ((2 * k) + 1); at += 1)
        {
            z_mul(&inv, &scale, &inv);
            z_floor_div(&inv, &root, &inv);
        }
        int32_t places = digits;
        OriorExactInteger read;
        rs_c_verdict(k, &one, 0, &places, &read);
        OriorExactInteger value;
        z_mul_small(&read, -2 * (((nu % 2) == 0) ? 1 : -1), &value);
        z_mul(&value, &inv, &value);
        z_floor_div(&value, &scale, &value);
        z_add(predicted, &value, predicted);
    }
    seam(nu, digits, jump);
}

/* ---------------------------------------------------------------- the default report */

/**
 * @brief The Gram points, the signs of Z, the gaps against Euler-Maclaurin, the null, and every C_n.
 *
 * @param[in] height_text The height [BORROWS].
 * @param[in] bits        The bits of the placement.
 * @return                The exit code: 0 where every check lands.
 */
static int main_report(const char *height_text, int bits)
{
    FILE *const out = stdout;
    DecimalPair height;
    pair_units(height_text, &height);
    fputs("  Z(t) by the Riemann-Siegel formula with t = 2pi u^4 represented as a real, by truthy and falsy verdicts\n",
          out);
    fputs(
        "  this reads the sign of Z at the Gram points it reaches and claims nothing about the Riemann hypothesis\n\n",
        out);

    int64_t begun = now_ms();
    GramSearch walk;
    memset(&walk, 0, sizeof(walk));
    walk.phase = u_phase;
    pair_small(12, 1, &walk.start);
    pair_small(1, 0, &walk.step);
    DecimalPair top;
    u_of(&height, &top);
    GramBracket *found = NULL;
    const size_t found_count = gram_walk(&walk, &top, &found);
    if (found_count == 0u)
    {
        zeta_stop("no Gram point below the height");
    }
    const int64_t last_index = found[found_count - 1u].index;
    const int64_t consecutive = ((int64_t)found_count == (last_index + 1)) && (found[0].index == 0);
    GramBracket *const placed = (GramBracket *)siegel_memory(malloc(found_count * sizeof(GramBracket)));
    memcpy(placed, found, found_count * sizeof(GramBracket));
    for (int bit = 0; bit < bits; bit += 1)
    {
        gram_cut(&walk, placed, found_count);
    }
    fprintf(out, "  %llu Gram points with 1.2 < u < ", (unsigned long long)found_count);
    pair_print(&top, 4, out);
    fputs(", t < ", out);
    pair_print(&height, height.places, out);
    fprintf(out, ", indices 0 to %lld, each alone in its bracket: %s\n", (long long)last_index, truth(consecutive));
    char label[64];
    snprintf(label, sizeof(label), "u placed by %d bits", bits);
    fprintf(out, "  %-5s %-28s %-15s %s\n", "n", label, "published t", "2pi u^4 holds");
    int64_t every = 1;
    for (size_t at = 0u; (at < found_count) && (at < 16u); at += 1u)
    {
        const int held = holds(&placed[at].low, &placed[at].high, ZETA_GRAM_PUBLISHED[at]);
        every *= held;
        fprintf(out, "  %-5lld [", (long long)placed[at].index);
        pair_print(&placed[at].low, 10, out);
        fputs(", ", out);
        pair_print(&placed[at].high, 10, out);
        fprintf(out, "]  %-15s %s\n", ZETA_GRAM_PUBLISHED[at], truth(held));
    }
    fprintf(out, "  %llu theta values asked, the deepest at %d places, the widest at N = %lld\n\n",
            (unsigned long long)walk.asked, (int)walk.deepest, (long long)walk.widest);
    fflush(out);

    Riemann riemann;
    memset(&riemann, 0, sizeof(riemann));
    riemann.remainder = 1;
    // The indices run 0 to last_index, one bracket each, as the consecutive verdict checks.
    int *const signs = (int *)siegel_memory(calloc((size_t)last_index + 1u, sizeof(int)));
    GramBracket *const settled = (GramBracket *)siegel_memory(calloc((size_t)last_index + 1u, sizeof(GramBracket)));
    const size_t cuts = riemann_settle(&riemann, placed, found_count, &walk, signs, settled);
    const int64_t spent = now_ms() - begun;
    fputs("  the sign of (-1)^n Z(g_n) by Riemann-Siegel:\n", out);
    for (size_t row = 0u; row < found_count; row += 32u)
    {
        fprintf(out, "    %4lld  ", (long long)found[row].index);
        for (size_t at = row; (at < found_count) && (at < (row + 32u)); at += 1u)
        {
            fputc((signs[found[at].index] > 0) ? '+' : '-', out);
        }
        fputs("\n", out);
    }
    fputs("  Gram's law fails at n = ", out);
    size_t bad_count = 0u;
    int64_t first_bad = -1;
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        if (signs[found[at].index] == -1)
        {
            fprintf(out, "%s%lld", (bad_count > 0u) ? ", " : "", (long long)found[at].index);
            first_bad = (bad_count == 0u) ? found[at].index : first_bad;
            bad_count += 1u;
        }
    }
    fputs((bad_count == 0u) ? "none\n" : "\n", out);
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        if (signs[found[at].index] == -1)
        {
            const GramBracket *const bracket = &settled[found[at].index];
            fprintf(out, "    g_%lld in u [", (long long)found[at].index);
            pair_print(&bracket->low, 10, out);
            fputs(", ", out);
            pair_print(&bracket->high, 10, out);
            fputs("]\n", out);
        }
    }
    size_t short_count = 0u;
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        const GramBracket *const bracket = &settled[found[at].index];
        short_count += riemann.reach[riemann_point(&riemann, &bracket->low)] == 1;
        short_count += riemann.reach[riemann_point(&riemann, &bracket->high)] == 1;
    }
    fprintf(
        out, "  %llu values of Z asked, %llu further cuts, the deepest at %d places, the main sum at max %lld terms\n",
        (unsigned long long)riemann.asked, (unsigned long long)cuts, (int)riemann.deepest, (long long)riemann.widest);
    fprintf(out, "  at %llu of the %llu settled ends Z reads past the last term c_5 u^-11\n",
            (unsigned long long)short_count, (unsigned long long)(2u * found_count));
    fprintf(out, "  the walk, the placing and the signs in %lld ms\n", (long long)spent);
    const int64_t reached = last_index >= ZETA_GRAM_FIRST_FAILURE;
    const int64_t first = (reached * (first_bad == ZETA_GRAM_FIRST_FAILURE)) + ((1 - reached) * (bad_count == 0u));
    fprintf(out, "  the first failure against the reported index %d: %s\n\n", ZETA_GRAM_FIRST_FAILURE, truth(first));
    fflush(out);

    fputs("  Z by Riemann-Siegel less Re e^(i theta) zeta by Euler-Maclaurin, in units of the last place\n", out);
    fprintf(out, "  %-6s %-8s %-30s %s\n", "u", "t", "with R, at 2, 4 and 8 places", "R left out, at 2, 4 and 8");
    static const int32_t GAP_PLACES[3] = {2, 4, 8};
    int narrower = 1;
    for (unsigned at = 0u; at < 6u; at += 1u)
    {
        DecimalPair u;
        pair_small(POINTS[at], 1, &u);
        u.places = 1;
        z_set(&u.numerator, POINTS[at]);
        OriorExactInteger with_r[3];
        OriorExactInteger without[3];
        for (unsigned which = 0u; which < 3u; which += 1u)
        {
            OriorExactInteger em;
            euler_maclaurin(&u, GAP_PLACES[which], &em);
            gap_at(&u, GAP_PLACES[which], 1, &em, &with_r[which]);
            gap_at(&u, GAP_PLACES[which], 0, &em, &without[which]);
        }
        OriorExactInteger first_size;
        OriorExactInteger second_size;
        z_abs(&with_r[1], &first_size);
        z_abs(&without[1], &second_size);
        narrower = narrower && (z_cmp(&first_size, &second_size) < 0);
        char u_text[64];
        pair_text(&u, 1, u_text, sizeof(u_text));
        DecimalPair t;
        time_of(&u, 2, &t);
        char t_text[64];
        pair_text(&t, 2, t_text, sizeof(t_text));
        char with_text[256];
        char without_text[256];
        list_text(with_r, 3u, with_text, sizeof(with_text));
        list_text(without, 3u, without_text, sizeof(without_text));
        fprintf(out, "  %-6s %-8s %-30s %s\n", u_text, t_text, with_text, without_text);
    }
    fprintf(out, "  at 4 places the gap with R is narrower than without it at every u displayed: %s\n\n",
            truth(narrower));
    fflush(out);

    Riemann null;
    memset(&null, 0, sizeof(null));
    null.remainder = 0;
    int *const null_signs = (int *)siegel_memory(calloc((size_t)last_index + 1u, sizeof(int)));
    GramBracket *const null_settled =
        (GramBracket *)siegel_memory(calloc((size_t)last_index + 1u, sizeof(GramBracket)));
    (void)riemann_settle(&null, placed, found_count, &walk, null_signs, null_settled);
    fputs("  drawn null, R left out: the sign of (-1)^n Z(g_n) differs at n = ", out);
    size_t flipped = 0u;
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        const int64_t n = found[at].index;
        if (null_signs[n] != signs[n])
        {
            fprintf(out, "%s%lld", (flipped > 0u) ? ", " : "", (long long)n);
            flipped += 1u;
        }
    }
    fputs((flipped == 0u) ? "none\n" : "\n", out);
    fflush(out);

    begun = now_ms();
    fputs("\n  every C_n by Gabcke's generator, each an exact sum of rationals times powers of pi and sqrt(2 -+ sqrt "
          "2) / 2\n",
          out);
    int mathworld = 1;
    for (int64_t n = 0; n < 6; n += 1)
    {
        const ZetaRow *const row = rs_d_row(n);
        int matched = (int)row->count == ZETA_MATHWORLD_COUNTS[n];
        for (size_t k = 0u; k < row->count; k += 1u)
        {
            // k is at most 3n / 4, and fits a machine integer.
            const int64_t m = (3 * n) - (4 * (int64_t)k);
            OriorExactInteger num;
            z_mul_small(&row->values[k], ((m % 2) == 0) ? 1 : -1, &num);
            OriorExactInteger den;
            z_two_to((2 * n) + m, &den);
            OriorExactInteger part;
            z_set(&part, 1);
            for (int64_t at = 2; at <= m; at += 1)
            {
                z_mul_small(&part, at, &part);
            }
            z_mul(&den, &part, &den);
            ZetaRational mine;
            rational_make(&num, &den, &mine);
            int seen = 0;
            for (int at = 0; at < ZETA_MATHWORLD_COUNTS[n]; at += 1)
            {
                const ZetaMathWorldTerm *const term = &ZETA_MATHWORLD_TERMS[n][at];
                if ((term->derivative == m) && (term->pi_half == (n - (int64_t)k)))
                {
                    ZetaRational theirs;
                    rational_small(term->numerator, term->denominator, &theirs);
                    seen = rational_cmp(&mine, &theirs) == 0;
                }
            }
            matched = matched && seen;
        }
        mathworld = mathworld && matched;
    }
    const ZetaRow *const row_eight = rs_d_row(8);
    int table_two = row_eight->count == 7u;
    for (size_t k = 0u; table_two && (k < 7u); k += 1u)
    {
        OriorExactInteger printed;
        z_set(&printed, TABLE_II_ROW_8[k]);
        table_two = orior_exact_equal(&row_eight->values[k], &printed);
    }
    fprintf(out, "  C_0 to C_5 against MathWorld's c_0 to c_5, rational for rational: %s\n", truth(mathworld));
    OriorExactInteger lambdas[5];
    for (int64_t l = 0; l < 5; l += 1)
    {
        rs_lambda(l, &lambdas[l]);
    }
    char lambda_text[512];
    list_text(lambdas, 5u, lambda_text, sizeof(lambda_text));
    fprintf(out, "  d_k^(8) against Gabcke's Table II: %s, and lambda_0 to lambda_4 = %s\n", truth(table_two),
            lambda_text);
    OriorExactInteger apart[11];
    int table_four = 1;
    OriorExactInteger one;
    z_set(&one, 1);
    OriorExactInteger spread;
    z_set(&spread, TABLE_SPREAD);
    for (int64_t n = 0; n < 11; n += 1)
    {
        int32_t places = 50;
        OriorExactInteger read;
        rs_c_verdict(n, &one, 0, &places, &read);
        const char *const printed = TABLE_IV_AT_ONE[n];
        const int negative = printed[0] == '-';
        const char *const digits = printed + (negative ? 3 : 2);
        OriorExactInteger value;
        zeta_require(orior_exact_from_decimal(digits, strlen(digits), 0u, &value), "decimal");
        if (negative)
        {
            value.sign = -value.sign;
        }
        z_sub(&read, &value, &apart[n]);
        OriorExactInteger size;
        z_abs(&apart[n], &size);
        table_four = table_four && (z_cmp(&size, &spread) <= 0);
    }
    char apart_text[512];
    list_text(apart, 11u, apart_text, sizeof(apart_text));
    fprintf(out, "  C_0(1) to C_10(1) less Gabcke's Table IV, in units of the 50th place: %s, within %d: %s\n",
            apart_text, TABLE_SPREAD, truth(table_four));
    CurveSigns curve_signs;
    memset(&curve_signs, 0, sizeof(curve_signs));
    fprintf(out,
            "  the zeros of C_n on (0, 1), z = 1 - 2p, each halved to 2^-%d, and the p in a cell where C_n vanishes\n",
            ZERO_BITS);
    fflush(out);
    int64_t counts[TOP_C + 1];
    OriorExactInteger full;
    z_two_to(ZERO_BITS, &full);
    for (int64_t n = 0; n <= TOP_C; n += 1)
    {
        OriorExactInteger zeros[64];
        int64_t parts = 0;
        const size_t count = curve_zeros(&curve_signs, n, ZERO_BITS, zeros, &parts);
        // The count is at most 64.
        counts[n] = (int64_t)count;
        fprintf(out, "    C_%-3lld %llu  parts 2^%lld  ", (long long)n, (unsigned long long)count, (long long)parts);
        for (size_t at = 0u; at < count; at += 1u)
        {
            char z_text_buffer[256];
            char p_low[256];
            char p_high[256];
            dyadic(&zeros[at], ZERO_BITS, 30, z_text_buffer, sizeof(z_text_buffer));
            OriorExactInteger value;
            z_sub(&full, &zeros[at], &value);
            z_sub(&value, &one, &value);
            dyadic(&value, ZERO_BITS + 1, 12, p_low, sizeof(p_low));
            z_add(&full, &zeros[at], &value);
            dyadic(&value, ZERO_BITS + 1, 12, p_high, sizeof(p_high));
            fprintf(out, "%sz %s p %s, %s", (at > 0u) ? "  " : "", z_text_buffer, p_low, p_high);
        }
        fputs("\n", out);
        fflush(out);
    }
    fprintf(out, "  sign changes on [63/64, 1] at 2^14 and 2^15 parts, C_0 to C_%d: [", TOP_C);
    for (int64_t n = 0; n <= TOP_C; n += 1)
    {
        int64_t edge[2];
        curve_edge(&curve_signs, n, edge);
        fprintf(out, "%s[%lld, %lld]", (n > 0) ? ", " : "", (long long)edge[0], (long long)edge[1]);
    }
    fputs("]\n", out);
    fputs("  the seam of Z_RS at x = nu + 1 against -2 (-1)^nu sum over even k from 6 to 16 of C_k(1) u^-(2k+1), at 20 "
          "places:\n",
          out);
    for (int64_t nu = 1; nu <= 8; nu += 1)
    {
        OriorExactInteger jump;
        OriorExactInteger predicted;
        seam_against_c(nu, &jump, &predicted);
        OriorExactInteger ratio = jump;
        zeta_require(orior_exact_scale_by_ten(&ratio, 8u), "scale");
        z_floor_div(&ratio, &predicted, &ratio);
        DecimalPair ratio_pair;
        pair_make(&ratio, 8, &ratio_pair);
        fprintf(out, "    x = %lld  seam ", (long long)(nu + 1));
        z_print(&jump, out);
        fputs("  from the C_k ", out);
        z_print(&predicted, out);
        fputs("  ratio ", out);
        pair_print(&ratio_pair, 8, out);
        fputs("\n", out);
        fflush(out);
    }
    const int gabcke = (counts[4] == 1) && (counts[8] == 1) && (counts[10] == 1);
    fprintf(out, "  one simple zero each in C_4, C_8 and C_10 on 0 < z < 1, as Gabcke reports (p. 59): %s\n",
            truth(gabcke));
    fprintf(out, "  every C_n in %lld ms\n", (long long)(now_ms() - begun));
    fflush(out);
    return (consecutive && every && first && narrower && mathworld && table_two && table_four && gabcke) ? 0 : 1;
}

/* ---------------------------------------------------------------- shared by the modes */

/**
 * @brief The quotient of a product and a divisor, rounded toward minus infinity.
 *
 * @param[in]  left    First factor [BORROWS].
 * @param[in]  right   Second factor [BORROWS].
 * @param[in]  divisor The divisor [BORROWS].
 * @param[out] result  The quotient [BORROWS].
 */
static void times_over(const OriorExactInteger *left, const OriorExactInteger *right, const OriorExactInteger *divisor,
                       OriorExactInteger *result)
{
    OriorExactInteger product;
    z_mul(left, right, &product);
    z_floor_div(&product, divisor, result);
}

/**
 * @brief An integer over 2^bits, rounded toward minus infinity, as >> of Python.
 *
 * @param[in]  value  The integer [BORROWS].
 * @param[in]  bits   The shift.
 * @param[out] result The quotient [BORROWS].
 */
static void floor_bits(const OriorExactInteger *value, int64_t bits, OriorExactInteger *result)
{
    OriorExactInteger power;
    z_two_to(bits, &power);
    z_floor_div(value, &power, result);
}

/**
 * @brief The sign (-1)^(nu-1).
 *
 * @param[in] nu The cell.
 * @return       1 or -1.
 */
static int cell_sign_of(int64_t nu)
{
    return ((((nu - 1) % 2) + 2) % 2 == 0) ? 1 : -1;
}

/**
 * @brief v at the places, its sign written apart, as the floor of |v| / 10^places.
 *
 * @param[in]  value    The value [BORROWS].
 * @param[in]  places   The places.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void signed_text(const OriorExactInteger *value, int32_t places, char *text, size_t capacity)
{
    DecimalPair pair;
    z_abs(value, &pair.numerator);
    pair.places = places;
    text[0] = '-';
    const size_t skip = (value->sign < 0) ? 1u : 0u;
    pair_text(&pair, places, text + skip, capacity - skip);
}

/**
 * @brief What the trapezoid counts too much of |f| on one part where f goes from x to y and the sign changes.
 *
 * @param[in]  x      The value at the left end [BORROWS].
 * @param[in]  y      The value at the right end [BORROWS].
 * @param[out] result 2 |x y| / (|x| + |y|), or zero where the sign holds [BORROWS].
 * @note It counts twice over as the sides grow.
 */
static void corner(const OriorExactInteger *x, const OriorExactInteger *y, OriorExactInteger *result)
{
    if ((x->sign > 0) == (y->sign > 0))
    {
        orior_exact_zero(result);
        return;
    }
    OriorExactInteger left;
    OriorExactInteger right;
    z_abs(x, &left);
    z_abs(y, &right);
    OriorExactInteger total;
    z_add(&left, &right, &total);
    if (total.sign == 0)
    {
        z_set(&total, 1);
    }
    z_mul(&left, &right, &left);
    z_mul_small(&left, 2, &left);
    z_floor_div(&left, &total, result);
}

/**
 * @brief One correction curve at a point: C_k(num / den) inv_root unit^k / (scale X^k), floored.
 *
 * @param[in]  k        The index of the curve.
 * @param[in]  num      The numerator of z [BORROWS].
 * @param[in]  den      The denominator of z [BORROWS].
 * @param[in]  digits   The places of the curve.
 * @param[in]  inv_root x^(-1/2) at the scale [BORROWS].
 * @param[in]  unit     The unit of x [BORROWS].
 * @param[in]  big_x    x times the unit [BORROWS].
 * @param[in]  scale    The scale [BORROWS].
 * @param[in]  sign     The sign that the term takes before the floor.
 * @param[out] result   The term [BORROWS].
 */
static void curve_term(int64_t k, const OriorExactInteger *num, const OriorExactInteger *den, int32_t digits,
                       const OriorExactInteger *inv_root, const OriorExactInteger *unit, const OriorExactInteger *big_x,
                       const OriorExactInteger *scale, int sign, OriorExactInteger *result)
{
    OriorExactInteger value;
    rs_c_at(k, num, den, digits, &value);
    z_mul_small(&value, sign, &value);
    z_mul(&value, inv_root, &value);
    OriorExactInteger under = *scale;
    for (int64_t at = 0; at < k; at += 1)
    {
        z_mul(&value, unit, &value);
        z_mul(&under, big_x, &under);
    }
    z_floor_div(&value, &under, result);
}

/**
 * @brief x^(-1/2) at the scale, as isqrt(scale^2 unit / X).
 *
 * @param[in]  scale  The scale [BORROWS].
 * @param[in]  unit   The unit of x [BORROWS].
 * @param[in]  big_x  x times the unit [BORROWS].
 * @param[out] result The value [BORROWS].
 */
static void inverse_root(const OriorExactInteger *scale, const OriorExactInteger *unit, const OriorExactInteger *big_x,
                         OriorExactInteger *result)
{
    OriorExactInteger square;
    z_mul(scale, scale, &square);
    z_mul(&square, unit, &square);
    z_floor_div(&square, big_x, &square);
    z_isqrt(&square, result);
}

/**
 * @brief The point x = nu + p as X and its unit.
 *
 * @param[in]  nu    The cell.
 * @param[in]  p     The delta [BORROWS].
 * @param[out] unit  10^q [BORROWS].
 * @param[out] big_x nu 10^q + P [BORROWS].
 */
static void point_of(int64_t nu, const DecimalPair *p, OriorExactInteger *unit, OriorExactInteger *big_x)
{
    z_ten_to(p->places, unit);
    z_mul_small(unit, nu, big_x);
    z_add(big_x, &p->numerator, big_x);
}

/**
 * @brief The prime factors of n and their exponents, by trial division, in ascending order.
 *
 * @param[in]  n      The number.
 * @param[out] primes The primes [BORROWS]. At most 64.
 * @param[out] powers Their exponents [BORROWS].
 * @return            How many primes.
 */
static size_t factored(int64_t n, int64_t primes[64], int64_t powers[64])
{
    size_t count = 0u;
    for (int64_t d = 2; (d * d) <= n; d += 1)
    {
        if ((n % d) != 0)
        {
            continue;
        }
        primes[count] = d;
        powers[count] = 0;
        while ((n % d) == 0)
        {
            powers[count] += 1;
            n /= d;
        }
        count += 1u;
    }
    if (n > 1)
    {
        primes[count] = n;
        powers[count] = 1;
        count += 1u;
    }
    return count;
}

/**
 * @brief Whether n is a prime.
 *
 * @param[in] n The number.
 * @return      1 where it is.
 */
static int is_prime(int64_t n)
{
    int64_t primes[64];
    int64_t powers[64];
    return (n > 1) && (factored(n, primes, powers) == 1u) && (powers[0] == 1);
}

/**
 * @brief The product of two complex numbers kept as pairs at the scale.
 *
 * @param[in]  left   First factor [BORROWS].
 * @param[in]  right  Second factor [BORROWS].
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The product [BORROWS]. It may be either factor.
 */
static void turned(const OriorExactInteger left[2], const OriorExactInteger right[2], const OriorExactInteger *scale,
                   OriorExactInteger result[2])
{
    OriorExactInteger first;
    OriorExactInteger second;
    OriorExactInteger re;
    OriorExactInteger im;
    z_mul(&left[0], &right[0], &first);
    z_mul(&left[1], &right[1], &second);
    z_sub(&first, &second, &re);
    z_floor_div(&re, scale, &re);
    z_mul(&left[0], &right[1], &first);
    z_mul(&left[1], &right[0], &second);
    z_add(&first, &second, &im);
    z_floor_div(&im, scale, &im);
    result[0] = re;
    result[1] = im;
}

/**
 * @brief theta at x = nu + j / 2^depth, t = 2pi x^2, at the work.
 *
 * @param[in]  nu     The cell.
 * @param[in]  j      The numerator.
 * @param[in]  depth  The bits of the denominator.
 * @param[in]  work   The places.
 * @param[out] result theta [BORROWS].
 */
static void theta_x(int64_t nu, const OriorExactInteger *j, int64_t depth, int32_t work, OriorExactInteger *result)
{
    DecimalPair p;
    OriorExactInteger five;
    z_set(&five, 1);
    for (int64_t at = 0; at < depth; at += 1)
    {
        z_mul_small(&five, 5, &five);
    }
    z_mul(j, &five, &p.numerator);
    // The depth of a bisection is small, and fits the places.
    p.places = (int32_t)depth;
    DecimalPair t;
    rs_time_at(nu, &p, work, &t);
    (void)rs_theta_at(&t, work, result);
}

/**
 * @brief The dyadic point j / 2^depth as a normalized decimal pair, as zz.pair(j 5^depth, depth).
 *
 * @param[in]  j     The numerator.
 * @param[in]  depth The bits of the denominator.
 * @param[out] p     The pair [BORROWS].
 */
static void dyadic_pair(int64_t j, int32_t depth, DecimalPair *p)
{
    OriorExactInteger value;
    z_set(&value, j);
    for (int32_t at = 0; at < depth; at += 1)
    {
        z_mul_small(&value, 5, &value);
    }
    pair_make(&value, depth, p);
}

/**
 * @brief What the second route of theta leaves out, whole units at the work.
 *
 * @param[in]  t      The point [BORROWS].
 * @param[in]  n_sum  The cut N of theta.
 * @param[in]  work   The places.
 * @param[out] result The bound [BORROWS].
 * @note The series of Stirling at w = 1/4 + it/2 + 2N with 2N terms stops short. With K = 2N, the shortfall is at
 *       most |B_(2K+2)| / ((2K+2)(2K+1) |w|^(2K+1)) sec^(2K+2)(arg w / 2). The bound arg w < pi/2 makes the power of
 *       the secant at most 2^(K+1), and |w| >= t / 2. Its roundings add 20K + 100.
 */
static void theta_floor(const DecimalPair *t, int64_t n_sum, int32_t work, OriorExactInteger *result)
{
    const int64_t k = 2 * n_sum;
    ZetaRational bernoulli;
    zeta_bernoulli(k + 1, &bernoulli);
    OriorExactInteger top;
    z_abs(&bernoulli.top, &top);
    OriorExactInteger factor;
    z_set(&factor, 1);
    for (int64_t at = 2; at <= ((2 * k) + 2); at += 1)
    {
        z_mul_small(&factor, at, &factor);
    }
    z_mul(&top, &factor, &top);
    z_two_to(k + 1, &factor);
    z_mul(&top, &factor, &top);
    z_ten_to(t->places, &factor);
    z_mul_small(&factor, 2, &factor);
    OriorExactInteger power;
    z_set(&power, 1);
    OriorExactInteger bottom_power;
    z_set(&bottom_power, 1);
    for (int64_t at = 0; at < ((2 * k) + 1); at += 1)
    {
        z_mul(&power, &factor, &power);
        z_mul(&bottom_power, &t->numerator, &bottom_power);
    }
    z_mul(&top, &power, &top);
    zeta_require(orior_exact_scale_by_ten(&top, (uint32_t)work), "scale");
    OriorExactInteger bottom;
    z_mul_small(&bernoulli.under, ((2 * k) + 2) * ((2 * k) + 1), &bottom);
    z_mul(&bottom, &bottom_power, &bottom);
    z_neg(&top, &top);
    z_floor_div(&top, &bottom, result);
    z_neg(result, result);
    OriorExactInteger extra;
    z_set(&extra, (20 * k) + 100);
    z_add(result, &extra, result);
}

/**
 * @brief A rational raised to a whole number of units at the scale.
 *
 * @param[in]  value  The rational [BORROWS].
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The ceiling [BORROWS].
 */
static void ceiling(const ZetaRational *value, const OriorExactInteger *scale, OriorExactInteger *result)
{
    OriorExactInteger top;
    z_mul(&value->top, scale, &top);
    z_neg(&top, &top);
    z_floor_div(&top, &value->under, result);
    z_neg(result, result);
}

/* ---------------------------------------------------------------- the omitted curves */

/**
 * @brief a over [nu, nu + 1] from the omitted curves alone, by the trapezoid on 2^m parts with each corner fixed.
 *
 * @param[in]  nu     The cell.
 * @param[in]  cut    The last curve that R keeps.
 * @param[in]  terms  How many curves past it.
 * @param[in]  m      The bits of the parts.
 * @param[in]  digits The places.
 * @param[out] result The side, times 2^(m+1), at 10^(digits + ZETA_GUARD) [BORROWS].
 * @note These are the terms that R through C_cut leaves out, C_k(1 - 2p) x^(-k - 1/2) for k = cut + 1 to
 *       cut + terms. The code reads only ratios between cells.
 */
static void omitted_side(int64_t nu, int64_t cut, int64_t terms, int64_t m, int32_t digits, OriorExactInteger *result)
{
    const int64_t steps = (int64_t)1 << m;
    OriorExactInteger scale;
    z_ten_to(digits + ZETA_GUARD, &scale);
    OriorExactInteger unit;
    z_set(&unit, steps);
    OriorExactInteger *const f = (OriorExactInteger *)siegel_memory(malloc(((size_t)steps + 1u) * sizeof(*f)));
    orior_exact_zero(result);
    for (int64_t j = 0; j <= steps; j += 1)
    {
        OriorExactInteger big_x;
        z_set(&big_x, (nu * steps) + j);
        OriorExactInteger inv_root;
        inverse_root(&scale, &unit, &big_x, &inv_root);
        OriorExactInteger num;
        z_set(&num, steps - (2 * j));
        orior_exact_zero(&f[j]);
        for (int64_t k = cut + 1; k < (cut + 1 + terms); k += 1)
        {
            OriorExactInteger term;
            curve_term(k, &num, &unit, digits, &inv_root, &unit, &big_x, &scale, 1, &term);
            z_add(&f[j], &term, &f[j]);
        }
        OriorExactInteger size;
        z_abs(&f[j], &size);
        z_mul_small(&size, ((j == 0) || (j == steps)) ? 1 : 2, &size);
        z_add(result, &size, result);
    }
    for (int64_t j = 0; j < steps; j += 1)
    {
        OriorExactInteger fixed;
        corner(&f[j], &f[j + 1], &fixed);
        z_sub(result, &fixed, result);
    }
    free(f);
}

/** @brief The omitted curves over one cell as one power series in z = 1 - 2p, and its antiderivative. */
typedef struct
{
    int64_t nu;              /**< The cell. */
    OriorExactInteger scale; /**< The scale of the work. */
    ZetaRow series;          /**< The coefficients in z. */
    ZetaRow lifted;          /**< The coefficients of the antiderivative, over n + 1. */
} Omitted;

/**
 * @brief The series of the omitted curves over the cell, at count terms and the work.
 *
 * @param[out] omitted The series [BORROWS].
 * @param[in]  nu      The cell.
 * @param[in]  cut     The last curve that R keeps.
 * @param[in]  terms   How many curves past it.
 * @param[in]  count   How many coefficients.
 * @param[in]  work    The places.
 * @note x = nu + p = X - z / 2, with X = nu + 1/2. Then
 *       C_k(z) x^(-k - 1/2) = X^(-1/2) (2 / (2 nu + 1))^k C_k(z) (1 - z / (2X))^(-k - 1/2). Each factor but X^(-1/2) is
 *       a series with rational coefficients times the own coefficients of the curve.
 * @note The weight (1 - z / (2X))^(-k - 1/2) has at z^j the product over i < j of (2k + 1 + 2i), over
 *       j! 2^j (2 nu + 1)^j.
 */
static void omitted_begin(Omitted *omitted, int64_t nu, int64_t cut, int64_t terms, int64_t count, int32_t work)
{
    memset(omitted, 0, sizeof(*omitted));
    omitted->nu = nu;
    z_ten_to(work, &omitted->scale);
    const OriorExactInteger *const scale = &omitted->scale;
    OriorExactInteger *const series = (OriorExactInteger *)siegel_memory(calloc((size_t)count, sizeof(*series)));
    OriorExactInteger *const weight = (OriorExactInteger *)siegel_memory(calloc((size_t)count, sizeof(*weight)));
    for (int64_t k = cut + 1; k < (cut + 1 + terms); k += 1)
    {
        const ZetaRow *const g = rs_gamma(k, count, work);
        OriorExactInteger num;
        OriorExactInteger den;
        z_set(&num, 1);
        z_set(&den, 1);
        for (int64_t j = 0; j < count; j += 1)
        {
            times_over(scale, &num, &den, &weight[j]);
            z_mul_small(&num, (2 * k) + 1 + (2 * j), &num);
            z_mul_small(&den, (j + 1) * 2 * ((2 * nu) + 1), &den);
        }
        OriorExactInteger power;
        z_two_to(k, &power);
        OriorExactInteger under;
        z_set(&under, 1);
        for (int64_t at = 0; at < k; at += 1)
        {
            z_mul_small(&under, (2 * nu) + 1, &under);
        }
        z_mul(&under, scale, &under);
        for (int64_t n = 0; n < count; n += 1)
        {
            OriorExactInteger total;
            orior_exact_zero(&total);
            for (int64_t i = 0; i <= n; i += 1)
            {
                OriorExactInteger product;
                z_mul(&g->values[i], &weight[n - i], &product);
                z_add(&total, &product, &total);
            }
            z_mul(&total, &power, &total);
            z_floor_div(&total, &under, &total);
            z_add(&series[n], &total, &series[n]);
        }
    }
    for (int64_t n = 0; n < count; n += 1)
    {
        row_push(&omitted->series, &series[n]);
        OriorExactInteger lifted;
        OriorExactInteger under;
        z_set(&under, n + 1);
        z_floor_div(&series[n], &under, &lifted);
        row_push(&omitted->lifted, &lifted);
    }
    free(series);
    free(weight);
}

/**
 * @brief Frees the series of the omitted curves.
 *
 * @param[in,out] omitted The series [BORROWS].
 */
static void omitted_free(Omitted *omitted)
{
    row_free(&omitted->series);
    row_free(&omitted->lifted);
}

/**
 * @brief A row at a / 2^bits by the rule of Horner, each step floored.
 *
 * @param[in]  row    The coefficients [BORROWS].
 * @param[in]  a      The numerator [BORROWS].
 * @param[in]  bits   The bits of the denominator.
 * @param[out] result The value [BORROWS].
 */
static void horner(const ZetaRow *row, const OriorExactInteger *a, int64_t bits, OriorExactInteger *result)
{
    OriorExactInteger power;
    z_two_to(bits, &power);
    orior_exact_zero(result);
    for (size_t at = row->count; at > 0u; at -= 1u)
    {
        z_mul(result, a, result);
        z_floor_div(result, &power, result);
        z_add(result, &row->values[at - 1u], result);
    }
}

/**
 * @brief Whether the series reads above zero at z = a / 2^bits.
 *
 * @param[in] omitted The series [BORROWS].
 * @param[in] a       The numerator [BORROWS].
 * @param[in] bits    The bits of the denominator.
 * @return            1 where it does.
 */
static int omitted_above(const Omitted *omitted, const OriorExactInteger *a, int64_t bits)
{
    OriorExactInteger value;
    horner(&omitted->series, a, bits, &value);
    return value.sign > 0;
}

/**
 * @brief The antiderivative of the series at z = a / 2^bits.
 *
 * @param[in]  omitted The series [BORROWS].
 * @param[in]  a       The numerator [BORROWS].
 * @param[in]  bits    The bits of the denominator.
 * @param[out] result  The value [BORROWS].
 */
static void omitted_antiderivative(const Omitted *omitted, const OriorExactInteger *a, int64_t bits,
                                   OriorExactInteger *result)
{
    OriorExactInteger value;
    horner(&omitted->lifted, a, bits, &value);
    z_mul(&value, a, &value);
    floor_bits(&value, bits, result);
}

/**
 * @brief The signs of the series at a / 2^depth for a from -2^depth to 2^depth.
 *
 * @param[in]  omitted The series [BORROWS].
 * @param[in]  depth   The bits.
 * @param[out] row     The signs [BORROWS]. 2^(depth+1) + 1 of them.
 */
static void omitted_row(const Omitted *omitted, int64_t depth, int *row)
{
    const int64_t reach = (int64_t)1 << depth;
    for (int64_t a = -reach; a <= reach; a += 1)
    {
        OriorExactInteger value;
        z_set(&value, a);
        row[a + reach] = omitted_above(omitted, &value, depth);
    }
}

/**
 * @brief The sign changes of the series on [-1, 1], each halved to 2^-bits.
 *
 * @param[in]  omitted The series [BORROWS].
 * @param[in]  bits    The bits of each zero.
 * @param[out] found   The zeros, each a over 2^bits [BORROWS]. At most 64.
 * @return             How many.
 * @note The changes come at 2^(m+1) and 2^(m+2) parts. The count m grows from 6 until the two counts agree.
 */
static size_t omitted_zeros(const Omitted *omitted, int64_t bits, OriorExactInteger *found)
{
    int64_t m = 6;
    int *row = (int *)siegel_memory(malloc((((size_t)2 << m) + 1u) * sizeof(int)));
    int *finer = (int *)siegel_memory(malloc((((size_t)4 << m) + 1u) * sizeof(int)));
    omitted_row(omitted, m, row);
    omitted_row(omitted, m + 1, finer);
    while (sign_changes(row, ((size_t)2 << m) + 1u, NULL) != sign_changes(finer, ((size_t)4 << m) + 1u, NULL))
    {
        m += 1;
        free(row);
        row = finer;
        finer = (int *)siegel_memory(malloc((((size_t)4 << m) + 1u) * sizeof(int)));
        omitted_row(omitted, m + 1, finer);
    }
    const size_t length = ((size_t)4 << m) + 1u;
    int64_t *const changes = (int64_t *)siegel_memory(malloc(length * sizeof(int64_t)));
    const size_t count = sign_changes(finer, length, changes);
    if (count > 64u)
    {
        zeta_stop("more than 64 zeros of the omitted curves");
    }
    for (size_t at = 0u; at < count; at += 1u)
    {
        OriorExactInteger low;
        z_set(&low, changes[at] - ((int64_t)2 << m));
        int64_t depth = m + 1;
        const int left = finer[changes[at]];
        while (depth < bits)
        {
            z_mul_small(&low, 2, &low);
            depth += 1;
            OriorExactInteger probe;
            OriorExactInteger one;
            z_set(&one, 1);
            z_add(&low, &one, &probe);
            if (omitted_above(omitted, &probe, depth) == left)
            {
                low = probe;
            }
        }
        found[at] = low;
    }
    free(changes);
    free(row);
    free(finer);
    return count;
}

/**
 * @brief a = X^(-1/2) / 2 times the sum over the pieces between zeros of |G(right) - G(left)|, at the work scale.
 *
 * @param[in]  omitted The series [BORROWS].
 * @param[in]  bits    The bits of each zero.
 * @param[out] result  The side [BORROWS].
 * @return             How many zeros.
 */
static size_t omitted_area(const Omitted *omitted, int64_t bits, OriorExactInteger *result)
{
    OriorExactInteger ends[66];
    z_two_to(bits, &ends[0]);
    z_neg(&ends[0], &ends[0]);
    const size_t count = omitted_zeros(omitted, bits, &ends[1]);
    z_two_to(bits, &ends[count + 1u]);
    OriorExactInteger total;
    orior_exact_zero(&total);
    for (size_t at = 0u; at <= count; at += 1u)
    {
        OriorExactInteger left;
        OriorExactInteger right;
        omitted_antiderivative(omitted, &ends[at], bits, &left);
        omitted_antiderivative(omitted, &ends[at + 1u], bits, &right);
        z_sub(&right, &left, &right);
        z_abs(&right, &right);
        z_add(&total, &right, &total);
    }
    OriorExactInteger square;
    z_mul(&omitted->scale, &omitted->scale, &square);
    z_mul_small(&square, 2, &square);
    OriorExactInteger under;
    z_set(&under, (2 * omitted->nu) + 1);
    z_floor_div(&square, &under, &square);
    OriorExactInteger root;
    z_isqrt(&square, &root);
    z_mul(&total, &root, &total);
    z_mul_small(&omitted->scale, 2, &under);
    z_floor_div(&total, &under, result);
    return count;
}

/**
 * @brief The main of the omitted mode: a over cells first to last, in closed form, and its exponent between cells.
 *
 * @param[in] first  The first cell.
 * @param[in] last   The last cell.
 * @param[in] cut    The last curve that R keeps.
 * @param[in] terms  How many curves past it.
 * @param[in] digits The places.
 * @param[in] m      The bits of the parts of the second route.
 * @return           The exit code.
 * @note The series runs at count and 2 count terms. They must agree with the guard dropped, and disagreement doubles
 *       count. The code halves the zeros to the bits where a misplaced end moves a by less than the last place. The
 *       second route is the trapezoid on 2^m parts, with corners fixed.
 */
static int omitted_main(int64_t first, int64_t last, int64_t cut, int64_t terms, int32_t digits, int64_t m)
{
    FILE *const out = stdout;
    const int64_t bits = 4 * (digits + ZETA_GUARD);
    OriorExactInteger *const sides =
        (OriorExactInteger *)siegel_memory(calloc((size_t)(last - first) + 2u, sizeof(OriorExactInteger)));
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        const int64_t begun = now_ms();
        int64_t count = 32;
        OriorExactInteger reads[2];
        size_t found = 0u;
        for (;;)
        {
            for (int which = 0; which < 2; which += 1)
            {
                const int64_t terms_n = count << which;
                // The counts are small, and the sum fits the places.
                const int32_t work = digits + ZETA_GUARD + (int32_t)terms_n + (int32_t)(3 * (cut + terms));
                Omitted omitted;
                omitted_begin(&omitted, nu, cut, terms, terms_n, work);
                OriorExactInteger area;
                found = omitted_area(&omitted, bits, &area);
                omitted_free(&omitted);
                OriorExactInteger drop;
                z_ten_to(work - digits - ZETA_GUARD, &drop);
                z_floor_div(&area, &drop, &reads[which]);
            }
            OriorExactInteger one;
            OriorExactInteger two;
            rs_toward_zero(&reads[0], &one);
            rs_toward_zero(&reads[1], &two);
            if (orior_exact_equal(&one, &two))
            {
                break;
            }
            count *= 2;
        }
        const OriorExactInteger *const a = &reads[1];
        OriorExactInteger grid;
        omitted_side(nu, cut, terms, m, digits, &grid);
        OriorExactInteger under;
        z_two_to(m + 1, &under);
        z_floor_div(&grid, &under, &grid);
        sides[nu - first] = *a;
        OriorExactInteger apart;
        z_sub(&grid, a, &apart);
        z_abs(&apart, &apart);
        ZetaRational ratio;
        rational_make(&apart, a, &ratio);
        OriorExactInteger read;
        rs_toward_zero(a, &read);
        char a_text[256];
        signed_text(&read, digits, a_text, sizeof(a_text));
        fprintf(out,
                "  cut %lld  cell %lld  a %s  zeros %llu  series terms %lld  the trapezoid on 2^%lld parts within "
                "%.1e of it  %lld ms\n",
                (long long)cut, (long long)nu, a_text, (unsigned long long)found, (long long)(2 * count), (long long)m,
                rational_to_double(&ratio), (long long)(now_ms() - begun));
        fflush(out);
    }
    for (int64_t nu = first; nu < last; nu += 1)
    {
        ZetaRational ratio;
        rational_make(&sides[nu - first], &sides[nu + 1 - first], &ratio);
        const double exponent = log(rational_to_double(&ratio)) / log(((double)nu + 1.5) / ((double)nu + 0.5));
        fprintf(out, "  cut %lld  cells %lld-%lld  exponent of a from the %lld omitted curves %.3f\n", (long long)cut,
                (long long)nu, (long long)(nu + 1), (long long)terms, exponent);
    }
    free(sides);
    return 0;
}

/* ---------------------------------------------------------------- the curve as a set */

/** @brief A member of the set read at the places: its floor and the budget that the floor carries. */
typedef struct
{
    OriorExactInteger value;  /**< The floor at 10^-places. */
    OriorExactInteger budget; /**< The budget in units of 10^-places. */
} Reading;

/**
 * @brief The frame of the set at x = nu + p: t, theta, its cosine and sine, and the floor of theta.
 *
 * @param[in]  nu    The cell.
 * @param[in]  p     The delta [BORROWS].
 * @param[in]  work  The places.
 * @param[out] t     The point [BORROWS].
 * @param[out] theta theta [BORROWS].
 * @param[out] turn  The cosine and the sine of theta [BORROWS].
 * @param[out] drift The floor of theta [BORROWS].
 */
static void frame_at(int64_t nu, const DecimalPair *p, int32_t work, DecimalPair *t, OriorExactInteger *theta,
                     OriorExactInteger turn[2], OriorExactInteger *drift)
{
    rs_time_at(nu, p, work, t);
    const int64_t n_sum = rs_theta_at(t, work, theta);
    zeta_cos_sin(theta, work, &turn[0], &turn[1]);
    theta_floor(t, n_sum, work, drift);
}

/**
 * @brief The members of the set at x = nu + p, summed, with their budget, and the bound of Gabcke on the rest.
 *
 * @param[in]  nu     The cell.
 * @param[in]  p      The delta [BORROWS].
 * @param[in]  cut    The last curve.
 * @param[in]  places The places.
 * @param[out] summed The sum of the members [BORROWS].
 * @param[out] budget The sum of their budgets [BORROWS].
 * @param[out] rest   The bound on the rest [BORROWS].
 * @return            1 where the bound of Gabcke holds the rest, 0 where the rest is open.
 * @note The members are the waves, the correction curves, and the rest r_K, what the cut leaves out. Each wave is
 *       w_n = 2 n^(-1/2) cos(theta - t ln n). Each correction curve is c_k = (-1)^(nu-1) C_k(z) x^(-k-1/2).
 * @note Each member holds its real and its operator: its floor at the places asked, with the budget that the floor
 *       carries. The bound of Gabcke holds the rest, |r_1| <= 0.053 t^(-5/4) for t >= 200.
 */
static int members_at(int64_t nu, const DecimalPair *p, int64_t cut, int32_t places, OriorExactInteger *summed,
                      OriorExactInteger *budget, OriorExactInteger *rest)
{
    const int32_t work = places + ZETA_GUARD;
    OriorExactInteger scale;
    z_ten_to(work, &scale);
    OriorExactInteger guard;
    z_ten_to(ZETA_GUARD, &guard);
    DecimalPair t;
    OriorExactInteger theta;
    OriorExactInteger turn[2];
    OriorExactInteger drift;
    frame_at(nu, p, work, &t, &theta, turn, &drift);
    orior_exact_zero(summed);
    orior_exact_zero(budget);
    ZetaPoint point;
    pair_small(5, 1, &point.sigma);
    point.t = t;
    OriorExactInteger whole;
    z_floor_div(&t.numerator, &scale, &whole);
    OriorExactInteger one;
    z_set(&one, 1);
    z_add(&whole, &one, &whole);
    OriorExactInteger wave_budget;
    z_mul_small(&whole, 20, &wave_budget);
    OriorExactInteger extra;
    z_set(&extra, 1000);
    z_add(&wave_budget, &extra, &wave_budget);
    z_mul_small(&wave_budget, 2, &wave_budget);
    OriorExactInteger twice_drift;
    z_mul_small(&drift, 2, &twice_drift);
    z_add(&wave_budget, &twice_drift, &wave_budget);
    z_floor_div(&wave_budget, &guard, &wave_budget);
    z_set(&extra, 2);
    z_add(&wave_budget, &extra, &wave_budget);
    for (int64_t n = 1; n <= nu; n += 1)
    {
        OriorExactInteger wave[4];
        zeta_power_minus_s(n, &point, work, wave);
        OriorExactInteger first;
        OriorExactInteger second;
        z_mul(&turn[0], &wave[0], &first);
        z_mul(&turn[1], &wave[1], &second);
        z_sub(&first, &second, &first);
        z_floor_div(&first, &scale, &first);
        z_mul_small(&first, 2, &first);
        z_floor_div(&first, &guard, &first);
        z_add(summed, &first, summed);
        z_add(budget, &wave_budget, budget);
    }
    OriorExactInteger unit;
    OriorExactInteger big_x;
    point_of(nu, p, &unit, &big_x);
    OriorExactInteger inv_root;
    inverse_root(&scale, &unit, &big_x, &inv_root);
    OriorExactInteger num;
    z_mul_small(&p->numerator, 2, &num);
    z_sub(&unit, &num, &num);
    for (int64_t k = 0; k <= cut; k += 1)
    {
        OriorExactInteger value;
        rs_c_at(k, &num, &unit, places, &value);
        z_mul_small(&value, cell_sign_of(nu), &value);
        z_mul(&value, &inv_root, &value);
        OriorExactInteger under = scale;
        for (int64_t at = 0; at < k; at += 1)
        {
            z_mul(&value, &unit, &value);
            z_mul(&under, &big_x, &under);
        }
        z_floor_div(&value, &under, &value);
        z_floor_div(&value, &guard, &value);
        z_add(summed, &value, summed);
        z_set(&extra, 6);
        z_add(budget, &extra, budget);
    }
    // The rest: t_low = 2 X^2 314159 / (100000 unit^2).
    OriorExactInteger t_low;
    z_mul(&big_x, &big_x, &t_low);
    z_mul_small(&t_low, 2 * 314159, &t_low);
    OriorExactInteger under;
    z_mul(&unit, &unit, &under);
    z_mul_small(&under, 100000, &under);
    z_floor_div(&t_low, &under, &t_low);
    const int64_t low = z_small(&t_low);
    const int bounded = (cut == 1) && (low >= 200);
    if (!bounded)
    {
        orior_exact_zero(rest);
        return 0;
    }
    OriorExactInteger fourth;
    OriorExactInteger root_low;
    z_isqrt(&t_low, &root_low);
    z_isqrt(&root_low, &fourth);
    const int64_t quarter = (z_small(&fourth) > 1) ? z_small(&fourth) : 1;
    ZetaRational bound;
    OriorExactInteger top;
    OriorExactInteger bottom;
    z_set(&top, 53);
    z_set(&bottom, 1000);
    z_mul_small(&bottom, (low > 1) ? low : 1, &bottom);
    z_mul_small(&bottom, quarter, &bottom);
    rational_make(&top, &bottom, &bound);
    OriorExactInteger place_scale;
    z_ten_to(places, &place_scale);
    ceiling(&bound, &place_scale, rest);
    return 1;
}

/**
 * @brief Z = Phi(theta, {t ln p}, z, x) from the generators alone, at the places, with its budget.
 *
 * @param[in]  nu     The cell.
 * @param[in]  p      The delta [BORROWS].
 * @param[in]  cut    The last curve.
 * @param[in]  places The places.
 * @param[out] value  Phi [BORROWS].
 * @param[out] budget Its budget [BORROWS].
 * @note Once the relations of the set cancel, theta remains. The unit wave e^(-i t ln p) of each prime also
 *       remains, with its budget. It is also the Taylor coefficients of F through what C_cut asks, with z and x^(-1/2).
 * @note Each wave is the product of the unit waves of its primes times n^(-1/2), turned by theta and doubled for
 *       2 Re. Each c_k comes from the Taylor coefficients of F through the d rows of Gabcke.
 * @note The budget sums magnitudes: the floors of each prime wave, once for each prime factor counted with its
 *       power. Then 2 units for each product, times n^(-1/2) <= 1. It adds the rotation of the frame, and the series
 *       of F read at count and 2 count terms.
 */
static void functional(int64_t nu, const DecimalPair *p, int64_t cut, int32_t places, OriorExactInteger *value,
                       OriorExactInteger *budget)
{
    const int32_t work = places + ZETA_GUARD;
    OriorExactInteger scale;
    z_ten_to(work, &scale);
    OriorExactInteger guard;
    z_ten_to(ZETA_GUARD, &guard);
    DecimalPair t;
    rs_time_at(nu, p, work, &t);
    OriorExactInteger theta;
    const int64_t n_sum = rs_theta_at(&t, work, &theta);
    OriorExactInteger drift;
    theta_floor(&t, n_sum, work, &drift);
    // The unit wave of each prime, indexed by the prime.
    OriorExactInteger(*const waves)[2] =
        (OriorExactInteger(*)[2])siegel_memory(calloc((size_t)nu + 1u, sizeof(OriorExactInteger[2])));
    for (int64_t prime = 2; prime <= nu; prime += 1)
    {
        if (!is_prime(prime))
        {
            continue;
        }
        OriorExactInteger log;
        zeta_ln(prime, work, &log);
        OriorExactInteger angle;
        z_mul(&t.numerator, &log, &angle);
        z_floor_div(&angle, &scale, &angle);
        zeta_cos_sin(&angle, work, &waves[prime][0], &waves[prime][1]);
        z_neg(&waves[prime][1], &waves[prime][1]);
    }
    OriorExactInteger whole;
    z_floor_div(&t.numerator, &scale, &whole);
    OriorExactInteger one;
    z_set(&one, 1);
    z_add(&whole, &one, &whole);
    OriorExactInteger wave_budget;
    z_mul_small(&whole, 20, &wave_budget);
    OriorExactInteger extra;
    z_set(&extra, 1000);
    z_add(&wave_budget, &extra, &wave_budget);
    OriorExactInteger turn[2];
    zeta_cos_sin(&theta, work, &turn[0], &turn[1]);
    OriorExactInteger total;
    OriorExactInteger error;
    orior_exact_zero(&total);
    orior_exact_zero(&error);
    for (int64_t n = 1; n <= nu; n += 1)
    {
        OriorExactInteger unit_wave[2];
        unit_wave[0] = scale;
        orior_exact_zero(&unit_wave[1]);
        int64_t primes[64];
        int64_t powers[64];
        const size_t count = factored(n, primes, powers);
        int64_t factors = 0;
        for (size_t at = 0u; at < count; at += 1u)
        {
            for (int64_t repeat = 0; repeat < powers[at]; repeat += 1)
            {
                turned(unit_wave, waves[primes[at]], &scale, unit_wave);
            }
            factors += powers[at];
        }
        OriorExactInteger square;
        z_mul(&scale, &scale, &square);
        OriorExactInteger under;
        z_set(&under, n);
        z_floor_div(&square, &under, &square);
        OriorExactInteger size;
        z_isqrt(&square, &size);
        OriorExactInteger re;
        OriorExactInteger im;
        times_over(&unit_wave[0], &size, &scale, &re);
        times_over(&unit_wave[1], &size, &scale, &im);
        OriorExactInteger first;
        OriorExactInteger second;
        z_mul(&turn[0], &re, &first);
        z_mul(&turn[1], &im, &second);
        z_sub(&first, &second, &first);
        z_floor_div(&first, &scale, &first);
        z_mul_small(&first, 2, &first);
        z_add(&total, &first, &total);
        OriorExactInteger part;
        z_set(&extra, 2);
        z_add(&wave_budget, &extra, &part);
        z_mul_small(&part, factors, &part);
        z_add(&part, &drift, &part);
        z_set(&extra, 4);
        z_add(&part, &extra, &part);
        z_mul_small(&part, 2, &part);
        z_add(&error, &part, &error);
    }
    free(waves);
    OriorExactInteger unit;
    OriorExactInteger big_x;
    point_of(nu, p, &unit, &big_x);
    OriorExactInteger inv_root;
    inverse_root(&scale, &unit, &big_x, &inv_root);
    OriorExactInteger num;
    z_mul_small(&p->numerator, 2, &num);
    z_sub(&unit, &num, &num);
    for (int64_t k = 0; k <= cut; k += 1)
    {
        OriorExactInteger term;
        curve_term(k, &num, &unit, places, &inv_root, &unit, &big_x, &scale, cell_sign_of(nu), &term);
        z_add(&total, &term, &total);
        z_mul_small(&guard, 6, &extra);
        z_add(&error, &extra, &error);
    }
    z_floor_div(&total, &guard, value);
    z_floor_div(&error, &guard, budget);
    z_set(&extra, 2 * (nu + cut + 2));
    z_add(budget, &extra, budget);
}

/**
 * @brief The main of the set mode: the set over cells first to last, at points in each cell.
 *
 * @param[in] first  The first cell.
 * @param[in] last   The last cell.
 * @param[in] cut    The last curve.
 * @param[in] places The places.
 * @param[in] points The points of each cell, p = (2j + 1) / 2^d with 2^d at least twice the points.
 * @return           The exit code: 0 where every route meets within its budget.
 * @note Three routes to Z must meet within their budgets: the members summed, the functional from the
 *       generators, and rs_cut_at.
 */
static int set_main(int64_t first, int64_t last, int64_t cut, int32_t places, int64_t points)
{
    FILE *const out = stdout;
    int every = 1;
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        const int64_t begun = now_ms();
        int64_t prime_count = 0;
        for (int64_t n = 2; n <= nu; n += 1)
        {
            prime_count += is_prime(n);
        }
        int32_t depth = 0;
        while ((((int64_t)1) << depth) <= ((2 * points) - 1))
        {
            depth += 1;
        }
        OriorExactInteger gaps[3];
        OriorExactInteger allowed[3];
        OriorExactInteger first_rest;
        int first_bounded = 0;
        for (int64_t j = 0; j < points; j += 1)
        {
            DecimalPair p;
            dyadic_pair((2 * j) + 1, depth, &p);
            OriorExactInteger summed;
            OriorExactInteger budget;
            OriorExactInteger rest;
            const int bounded = members_at(nu, &p, cut, places, &summed, &budget, &rest);
            OriorExactInteger phi;
            OriorExactInteger phi_budget;
            functional(nu, &p, cut, places, &phi, &phi_budget);
            OriorExactInteger s;
            OriorExactInteger r;
            rs_cut_at(nu, &p, places, cut, &s, &r);
            OriorExactInteger z_value;
            z_add(&s, &r, &z_value);
            OriorExactInteger these[3];
            OriorExactInteger bars[3];
            z_sub(&summed, &phi, &these[0]);
            z_sub(&summed, &z_value, &these[1]);
            z_sub(&phi, &z_value, &these[2]);
            OriorExactInteger two;
            z_set(&two, 2);
            z_add(&budget, &phi_budget, &bars[0]);
            z_add(&budget, &two, &bars[1]);
            z_add(&phi_budget, &two, &bars[2]);
            for (int at = 0; at < 3; at += 1)
            {
                z_abs(&these[at], &these[at]);
                every = every && (z_cmp(&these[at], &bars[at]) <= 0);
                if ((j == 0) || (z_cmp(&these[at], &gaps[at]) > 0))
                {
                    gaps[at] = these[at];
                }
                if ((j == 0) || (z_cmp(&bars[at], &allowed[at]) < 0))
                {
                    allowed[at] = bars[at];
                }
            }
            if (j == 0)
            {
                first_rest = rest;
                first_bounded = bounded;
            }
        }
        char rest_text[256];
        if (first_bounded)
        {
            char bound_text[200];
            signed_text(&first_rest, places, bound_text, sizeof(bound_text));
            snprintf(rest_text, sizeof(rest_text), "Gabcke's bound %s", bound_text);
        }
        else
        {
            snprintf(rest_text, sizeof(rest_text), "open");
        }
        char numbers[6][128];
        for (int at = 0; at < 3; at += 1)
        {
            z_text(&gaps[at], numbers[2 * at], sizeof(numbers[0]));
            z_text(&allowed[at], numbers[(2 * at) + 1], sizeof(numbers[0]));
        }
        fprintf(
            out,
            "  cell %lld  members %lld (%lld waves, %lld correction curves, the rest)  what remains %lld (theta, "
            "%lld primes, F, z, x, the rest)  members against Phi %s of %s allowed, against rs_cut_at %s of %s, Phi "
            "against rs_cut_at %s of %s, units of 10^-%d  the rest, every member not yet defined, represented as "
            "%s  %lld ms\n",
            (long long)nu, (long long)(nu + cut + 2), (long long)nu, (long long)(cut + 1), (long long)(prime_count + 5),
            (long long)prime_count, numbers[0], numbers[1], numbers[2], numbers[3], numbers[4], numbers[5], (int)places,
            rest_text, (long long)(now_ms() - begun));
        fflush(out);
    }
    return every ? 0 : 1;
}

/* ---------------------------------------------------------------- the ball */

/**
 * @brief theta'(t) = Re psi(1/4 + it/2) / 2 - ln(pi) / 2 at the digits, by one route.
 *
 * @param[in]  t      The point [BORROWS].
 * @param[in]  digits The places.
 * @param[in]  shift  The shift.
 * @param[in]  terms  The Bernoulli terms.
 * @param[out] result The value [BORROWS].
 * @note psi comes from the series of Stirling at w = 1/4 + it/2 + shift, less 1 / (1/4 + it/2 + k) for each k below
 *       the shift. With w = (a + ib) / d, Re w^(-2j) = Re (a - ib)^(2j) d^(2j) / (a^2 + b^2)^(2j).
 */
static void theta_prime_route(const DecimalPair *t, int32_t digits, int64_t shift, int64_t terms,
                              OriorExactInteger *result)
{
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger unit;
    z_ten_to(t->places, &unit);
    OriorExactInteger a;
    OriorExactInteger b;
    OriorExactInteger d;
    z_mul_small(&unit, 1 + (4 * shift), &a);
    z_mul_small(&t->numerator, 2, &b);
    z_mul_small(&unit, 4, &d);
    OriorExactInteger size;
    OriorExactInteger product;
    z_mul(&a, &a, &size);
    z_mul(&b, &b, &product);
    z_add(&size, &product, &size);
    OriorExactInteger psi;
    OriorExactInteger log_d;
    zeta_ln_big(&size, digits, &psi);
    zeta_ln_big(&d, digits, &log_d);
    z_mul_small(&log_d, 2, &log_d);
    z_sub(&psi, &log_d, &psi);
    OriorExactInteger two;
    z_set(&two, 2);
    z_floor_div(&psi, &two, &psi);
    z_mul(&scale, &d, &product);
    z_mul(&product, &a, &product);
    OriorExactInteger under;
    z_mul_small(&size, 2, &under);
    z_floor_div(&product, &under, &product);
    z_sub(&psi, &product, &psi);
    OriorExactInteger square_re;
    OriorExactInteger square_im;
    z_mul(&a, &a, &square_re);
    z_mul(&b, &b, &product);
    z_sub(&square_re, &product, &square_re);
    z_mul(&a, &b, &square_im);
    z_mul_small(&square_im, -2, &square_im);
    OriorExactInteger re = square_re;
    OriorExactInteger im = square_im;
    OriorExactInteger d_square;
    OriorExactInteger size_square;
    z_mul(&d, &d, &d_square);
    z_mul(&size, &size, &size_square);
    OriorExactInteger d_power = d_square;
    OriorExactInteger size_power = size_square;
    OriorExactInteger factorial;
    z_set(&factorial, 1);
    for (int64_t j = 1; j <= terms; j += 1)
    {
        ZetaRational bernoulli;
        zeta_bernoulli(j, &bernoulli);
        OriorExactInteger term;
        z_mul(&scale, &bernoulli.top, &term);
        z_mul(&term, &factorial, &term);
        z_mul(&term, &re, &term);
        z_mul(&term, &d_power, &term);
        z_mul(&bernoulli.under, &size_power, &under);
        z_floor_div(&term, &under, &term);
        z_sub(&psi, &term, &psi);
        OriorExactInteger next_re;
        OriorExactInteger next_im;
        z_mul(&re, &square_re, &next_re);
        z_mul(&im, &square_im, &product);
        z_sub(&next_re, &product, &next_re);
        z_mul(&re, &square_im, &next_im);
        z_mul(&im, &square_re, &product);
        z_add(&next_im, &product, &next_im);
        re = next_re;
        im = next_im;
        z_mul(&d_power, &d_square, &d_power);
        z_mul(&size_power, &size_square, &size_power);
        z_mul_small(&factorial, (2 * j) * ((2 * j) + 1), &factorial);
    }
    OriorExactInteger t_square;
    z_mul(&t->numerator, &t->numerator, &t_square);
    z_mul_small(&t_square, 4, &t_square);
    for (int64_t k = 0; k < shift; k += 1)
    {
        OriorExactInteger c;
        z_mul_small(&unit, 1 + (4 * k), &c);
        OriorExactInteger top;
        z_mul(&scale, &c, &top);
        z_mul_small(&top, 4, &top);
        z_mul(&top, &unit, &top);
        z_mul(&c, &c, &under);
        z_add(&under, &t_square, &under);
        z_floor_div(&top, &under, &top);
        z_sub(&psi, &top, &psi);
    }
    OriorExactInteger pi_log;
    theta_ln_pi(digits, &pi_log);
    z_sub(&psi, &pi_log, &psi);
    z_floor_div(&psi, &two, result);
}

/**
 * @brief theta'(t) at the digits by the routes at N and 2N. N doubles until they agree with the guard dropped.
 *
 * @param[in]  t      The point [BORROWS].
 * @param[in]  digits The places.
 * @param[out] result The second route [BORROWS].
 */
static void theta_prime_at(const DecimalPair *t, int32_t digits, OriorExactInteger *result)
{
    int64_t n_sum = 1;
    OriorExactInteger one;
    theta_prime_route(t, digits, 1, 1, &one);
    theta_prime_route(t, digits, 2, 2, result);
    for (;;)
    {
        OriorExactInteger first;
        OriorExactInteger second;
        rs_toward_zero(&one, &first);
        rs_toward_zero(result, &second);
        if (orior_exact_equal(&first, &second))
        {
            return;
        }
        n_sum *= 2;
        theta_prime_route(t, digits, n_sum, n_sum, &one);
        theta_prime_route(t, digits, 2 * n_sum, 2 * n_sum, result);
    }
}

/** @brief The ball at one point x = nu + p. */
typedef struct
{
    OriorExactInteger radius;    /**< |z|^2. */
    OriorExactInteger d;         /**< D, the curves that R through C_cut leaves out. */
    OriorExactInteger e;         /**< E = S - R. */
    OriorExactInteger z;         /**< Z = S + R. */
    OriorExactInteger momentum;  /**< L = sum (theta' - ln n) / n. */
    OriorExactInteger energy;    /**< sum (theta' - ln n)^2 / 2n. */
    OriorExactInteger arrival;   /**< The spin of the last wave. */
    OriorExactInteger shadow[2]; /**< The sum of the waves. */
} Ball;

/**
 * @brief The ball at x = nu + p, at the digits + ZETA_GUARD.
 *
 * @param[in]  nu     The cell.
 * @param[in]  p      The delta [BORROWS].
 * @param[in]  cut    The last curve.
 * @param[in]  digits The places.
 * @param[out] ball   The ball [BORROWS].
 * @note D is the curves C_k(1 - 2p) x^(-k - 1/2) for k to cut + 3, with the sign that D = S + R - Z takes. E = S - R
 *       and Z = S + R come from Riemann-Siegel.
 */
static void ball_at(int64_t nu, const DecimalPair *p, int64_t cut, int32_t digits, Ball *ball)
{
    const int32_t work = digits + ZETA_GUARD;
    OriorExactInteger scale;
    z_ten_to(work, &scale);
    DecimalPair t;
    rs_time_at(nu, p, work, &t);
    OriorExactInteger theta;
    (void)rs_theta_at(&t, work, &theta);
    OriorExactInteger cosine;
    OriorExactInteger sine;
    zeta_cos_sin(&theta, work, &cosine, &sine);
    ZetaPoint point;
    pair_small(5, 1, &point.sigma);
    point.t = t;
    orior_exact_zero(&ball->radius);
    orior_exact_zero(&ball->shadow[0]);
    orior_exact_zero(&ball->shadow[1]);
    for (int64_t n = 1; n <= nu; n += 1)
    {
        OriorExactInteger wave[4];
        zeta_power_minus_s(n, &point, work, wave);
        OriorExactInteger first;
        OriorExactInteger second;
        OriorExactInteger re;
        OriorExactInteger im;
        z_mul(&cosine, &wave[0], &first);
        z_mul(&sine, &wave[1], &second);
        z_sub(&first, &second, &re);
        z_floor_div(&re, &scale, &re);
        z_mul(&sine, &wave[0], &first);
        z_mul(&cosine, &wave[1], &second);
        z_add(&first, &second, &im);
        z_floor_div(&im, &scale, &im);
        z_mul(&re, &re, &first);
        z_mul(&im, &im, &second);
        z_add(&ball->radius, &first, &ball->radius);
        z_add(&ball->radius, &second, &ball->radius);
        z_add(&ball->shadow[0], &re, &ball->shadow[0]);
        z_add(&ball->shadow[1], &im, &ball->shadow[1]);
    }
    z_floor_div(&ball->radius, &scale, &ball->radius);
    OriorExactInteger s;
    OriorExactInteger r;
    rs_cut_at(nu, p, digits, cut, &s, &r);
    OriorExactInteger unit;
    OriorExactInteger big_x;
    point_of(nu, p, &unit, &big_x);
    OriorExactInteger inv_root;
    inverse_root(&scale, &unit, &big_x, &inv_root);
    OriorExactInteger num;
    z_mul_small(&p->numerator, 2, &num);
    z_sub(&unit, &num, &num);
    orior_exact_zero(&ball->d);
    for (int64_t k = cut + 1; k < (cut + 4); k += 1)
    {
        OriorExactInteger term;
        curve_term(k, &num, &unit, digits, &inv_root, &unit, &big_x, &scale, 1, &term);
        z_add(&ball->d, &term, &ball->d);
    }
    z_mul_small(&ball->d, -cell_sign_of(nu), &ball->d);
    OriorExactInteger turn;
    theta_prime_at(&t, work, &turn);
    orior_exact_zero(&ball->momentum);
    orior_exact_zero(&ball->energy);
    for (int64_t n = 1; n <= nu; n += 1)
    {
        OriorExactInteger log;
        zeta_ln(n, work, &log);
        OriorExactInteger spin;
        z_sub(&turn, &log, &spin);
        OriorExactInteger under;
        z_set(&under, n);
        OriorExactInteger part;
        z_floor_div(&spin, &under, &part);
        z_add(&ball->momentum, &part, &ball->momentum);
        z_mul(&spin, &spin, &part);
        z_mul_small(&scale, 2 * n, &under);
        z_floor_div(&part, &under, &part);
        z_add(&ball->energy, &part, &ball->energy);
        ball->arrival = spin;
    }
    OriorExactInteger guard;
    z_ten_to(ZETA_GUARD, &guard);
    z_sub(&s, &r, &ball->e);
    z_mul(&ball->e, &guard, &ball->e);
    z_add(&s, &r, &ball->z);
    z_mul(&ball->z, &guard, &ball->z);
}

/**
 * @brief Makes a folder, where it does not exist.
 *
 * @param[in] path The path [BORROWS].
 */
static void make_folder(const char *path)
{
#ifdef _WIN32
    (void)_mkdir(path);
#else
    (void)mkdir(path, 0777);
#endif
}

/**
 * @brief The main of the sphere mode: the ball over cells first to last on 2^m parts, its trace in build/sphere.
 *
 * @param[in] first  The first cell.
 * @param[in] last   The last cell.
 * @param[in] cut    The last curve.
 * @param[in] m      The bits of the parts.
 * @param[in] digits The places.
 * @return           The exit code.
 * @note The main sum's waves are one point on a sphere. Each wave z_n = e^(i theta) n^-s on the line is one complex
 *       coordinate of radius n^(-1/2), and turns at theta'(t) - ln n.
 * @note Over a cell the point (z_1, ..., z_nu) keeps |z|^2 = H_nu, the harmonic number, and moves on a torus inside
 *       that sphere. Z = 2 Re sum z_n + R is its shadow on the diagonal.
 */
static int sphere_main(int64_t first, int64_t last, int64_t cut, int64_t m, int32_t digits)
{
    FILE *const out = stdout;
    const int32_t work = digits + ZETA_GUARD;
    OriorExactInteger scale;
    z_ten_to(work, &scale);
    const int64_t steps = (int64_t)1 << m;
    make_folder("build");
    make_folder("build/sphere");
    char path[256];
    snprintf(path, sizeof(path), "build/sphere/ball_%lld_%lld_cut_%lld.tsv", (long long)first, (long long)last,
             (long long)cut);
    FILE *const trace = fopen(path, "w");
    if (trace == NULL)
    {
        zeta_stop("cannot write the trace");
    }
    fputs("x\tradius_squared_less_h\tD\tE\tZ\tL\tenergy\tW_re\tW_im\n", trace);
    Ball *const rows = (Ball *)siegel_memory(malloc(((size_t)steps + 1u) * sizeof(Ball)));
    int *const flags = (int *)siegel_memory(malloc((((size_t)steps * 2u) + 2u) * sizeof(int)));
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        const int64_t begun = now_ms();
        OriorExactInteger harmonic;
        orior_exact_zero(&harmonic);
        for (int64_t n = 1; n <= nu; n += 1)
        {
            OriorExactInteger under;
            OriorExactInteger part;
            z_set(&under, n);
            z_floor_div(&scale, &under, &part);
            z_add(&harmonic, &part, &harmonic);
        }
        OriorExactInteger gap;
        orior_exact_zero(&gap);
        for (int64_t j = 0; j <= steps; j += 1)
        {
            DecimalPair p;
            // m is small, and fits the places.
            dyadic_pair(j, (int32_t)m, &p);
            ball_at(nu, &p, cut, digits, &rows[j]);
            DecimalPair x;
            dyadic_pair((nu * steps) + j, (int32_t)m, &x);
            char text[512];
            pair_text(&x, (int32_t)m, text, sizeof(text));
            fputs(text, trace);
            OriorExactInteger less;
            z_sub(&rows[j].radius, &harmonic, &less);
            const OriorExactInteger *const values[8] = {
                &less,           &rows[j].d,         &rows[j].e,        &rows[j].z, &rows[j].momentum,
                &rows[j].energy, &rows[j].shadow[0], &rows[j].shadow[1]};
            for (int at = 0; at < 8; at += 1)
            {
                signed_text(values[at], work, text, sizeof(text));
                fprintf(trace, "\t%s", text);
            }
            fputs("\n", trace);
            OriorExactInteger size;
            z_abs(&less, &size);
            gap = (z_cmp(&size, &gap) > 0) ? size : gap;
        }
        size_t flips[3];
        for (int which = 0; which < 3; which += 1)
        {
            for (int64_t j = 0; j <= steps; j += 1)
            {
                const OriorExactInteger *const value =
                    (which == 0) ? &rows[j].d : ((which == 1) ? &rows[j].e : &rows[j].z);
                flags[j] = value->sign > 0;
            }
            flips[which] = sign_changes(flags, (size_t)steps + 1u, NULL);
        }
        for (int64_t j = 0; j <= (2 * steps); j += 1)
        {
            DecimalPair p;
            dyadic_pair(j, (int32_t)(m + 1), &p);
            OriorExactInteger s;
            OriorExactInteger r;
            rs_cut_at(nu, &p, digits, cut, &s, &r);
            OriorExactInteger z_value;
            z_add(&s, &r, &z_value);
            flags[j] = z_value.sign > 0;
        }
        const size_t z_finer = sign_changes(flags, ((size_t)steps * 2u) + 1u, NULL);
        OriorExactInteger drop;
        z_ten_to(work - 12, &drop);
        OriorExactInteger reads[4];
        z_floor_div(&rows[0].momentum, &drop, &reads[0]);
        z_floor_div(&rows[steps].momentum, &drop, &reads[1]);
        z_floor_div(&rows[0].energy, &drop, &reads[2]);
        z_floor_div(&rows[steps].energy, &drop, &reads[3]);
        char texts[5][256];
        for (int at = 0; at < 4; at += 1)
        {
            signed_text(&reads[at], 12, texts[at], sizeof(texts[at]));
        }
        signed_text(&rows[0].arrival, work, texts[4], sizeof(texts[4]));
        char gap_text[256];
        z_text(&gap, gap_text, sizeof(gap_text));
        fprintf(out,
                "  cell %lld  |z|^2 - H within %s units of 10^-%d  sign changes of D %llu, E %llu, Z %llu (Z on 2^%lld "
                "parts %llu)  L %s to %s  energy %s to %s  wave %lld arrives turning at %s  %lld ms\n",
                (long long)nu, gap_text, (int)work, (unsigned long long)flips[0], (unsigned long long)flips[1],
                (unsigned long long)flips[2], (long long)(m + 1), (unsigned long long)z_finer, texts[0], texts[1],
                texts[2], texts[3], (long long)nu, texts[4], (long long)(now_ms() - begun));
        fflush(out);
    }
    fclose(trace);
    free(rows);
    free(flags);
    fprintf(out, "  the trace: %s\n", path);
    return 0;
}

/* ---------------------------------------------------------------- the motion of the ball against its permutations */

/** @brief The seed of the drawn orders, as the shuffles of the tree name it. */
#define SHUFFLE_SEED 0x51F7u

/** @brief Each ball at one point of a cell. */
typedef struct
{
    OriorExactInteger t;        /**< t at the work. */
    OriorExactInteger theta;    /**< theta at the work. */
    OriorExactInteger e;        /**< The budget on M. */
    OriorExactInteger e_d;      /**< The budget on M'. */
    OriorExactInteger rate_sum; /**< The sum over n of (theta' - ln n)^2 / n. */
    OriorExactInteger *m;       /**< M of each ball [OWNS]. */
    OriorExactInteger *d;       /**< The part of M' of the waves of each ball [OWNS]. */
} MotionPoint;

/** @brief What the walk over one cell shares. */
typedef struct
{
    int64_t nu;                     /**< The cell. */
    int32_t digits;                 /**< The places. */
    size_t ball_count;              /**< How many balls. */
    OriorExactInteger (**balls)[2]; /**< The constant turn of each wave, per ball [BORROWS]. */
    const OriorExactInteger *logs;  /**< ln n at the work [BORROWS]. */
    OriorExactInteger bounds[3];    /**< B1, B2 and R1. */
    OriorExactInteger rho;          /**< The bound of Gabcke. */
    OriorExactInteger scale;        /**< The scale of the work. */
    int64_t bits;                   /**< The bits of the grid. */
    int64_t deepest;                /**< How many more bits a walk may halve. */
    uint64_t asked;                 /**< How many points the walk asked for. */
    int64_t *zeros;                 /**< The zeros of M of each ball [OWNS]. */
    int64_t *open;                  /**< The intervals left open, per ball [OWNS]. */
    int64_t truth[2];               /**< The zeros of Z itself, and the intervals that rho leaves open. */
} MotionCell;

/**
 * @brief The constant turn of each wave under one null, as unit pairs at the scale, with wave 1 unturned.
 *
 * @param[in]  heads  The direction of each wave at the start of the cell [BORROWS].
 * @param[in]  nu     How many waves.
 * @param[in]  locked 1 to permute the primes and carry them by factors, 0 to permute 2 to nu freely.
 * @param[in]  seed   The seed of the draw.
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] turns  The turns [BORROWS]. nu of them.
 * @note A wave sent to the direction of another turns by that direction times the conjugate of its own. A draw that
 *       moves no seat is the identity, and the code draws again at the seed 2^16 on.
 */
static void null_turns(const OriorExactInteger (*heads)[2], int64_t nu, int locked, uint32_t seed,
                       const OriorExactInteger *scale, OriorExactInteger (*turns)[2])
{
    int64_t *const seats = (int64_t *)siegel_memory(malloc(((size_t)nu + 1u) * sizeof(int64_t)));
    size_t count = 0u;
    for (int64_t n = 2; n <= nu; n += 1)
    {
        if (!locked || is_prime(n))
        {
            seats[count] = n;
            count += 1u;
        }
    }
    unsigned char *const order = (unsigned char *)siegel_memory(malloc(count + 1u));
    for (;;)
    {
        for (size_t at = 0u; at < count; at += 1u)
        {
            // The seats number fewer than 256 for the cells of this report.
            order[at] = (unsigned char)at;
        }
        zeta_permuted(order, count, seed);
        int identity = 1;
        for (size_t at = 0u; at < count; at += 1u)
        {
            identity = identity && (order[at] == at);
        }
        if ((count <= 1u) || !identity)
        {
            break;
        }
        seed += 1u << 16;
    }
    OriorExactInteger(*const sent)[2] =
        (OriorExactInteger(*)[2])siegel_memory(calloc((size_t)nu + 1u, sizeof(OriorExactInteger[2])));
    for (size_t at = 0u; at < count; at += 1u)
    {
        const int64_t n = seats[at];
        const int64_t m = seats[order[at]];
        OriorExactInteger conjugate[2];
        conjugate[0] = heads[n - 1][0];
        z_neg(&heads[n - 1][1], &conjugate[1]);
        turned(heads[m - 1], conjugate, scale, sent[n]);
    }
    turns[0][0] = *scale;
    orior_exact_zero(&turns[0][1]);
    for (int64_t n = 2; n <= nu; n += 1)
    {
        OriorExactInteger turn[2];
        turn[0] = *scale;
        orior_exact_zero(&turn[1]);
        int64_t primes[64];
        int64_t powers[64];
        size_t factor_count = 1u;
        primes[0] = n;
        powers[0] = 1;
        if (locked)
        {
            factor_count = factored(n, primes, powers);
        }
        for (size_t at = 0u; at < factor_count; at += 1u)
        {
            for (int64_t repeat = 0; repeat < powers[at]; repeat += 1)
            {
                turned(turn, sent[primes[at]], scale, turn);
            }
        }
        turns[n - 1][0] = turn[0];
        turns[n - 1][1] = turn[1];
    }
    free(sent);
    free(order);
    free(seats);
}

/**
 * @brief C(row, column) for a small column, zero where the column passes the row.
 *
 * @param[in]  row    The row.
 * @param[in]  column The column.
 * @param[out] result The value [BORROWS].
 */
static void binomial_small(int64_t row, int64_t column, OriorExactInteger *result)
{
    if (column > row)
    {
        orior_exact_zero(result);
        return;
    }
    z_set(result, 1);
    for (int64_t at = 0; at < column; at += 1)
    {
        z_mul_small(result, row - at, result);
        OriorExactInteger under;
        z_set(&under, at + 1);
        z_div_exact(result, &under, result);
    }
}

/**
 * @brief Upper bounds on |C_k|, |C_k'| and |C_k''| over -1 <= z <= 1.
 *
 * @param[in]  k      The index of the curve.
 * @param[in]  work   The places.
 * @param[out] bounds The three bounds [BORROWS].
 * @note Each is the sum of the sizes of the Taylor coefficients times 1, i and i (i - 1), each read at the work.
 *       The code raises it by the count of the terms in units.
 */
static void magnitudes(int64_t k, int32_t work, ZetaRational bounds[3])
{
    const int64_t count = 96;
    OriorExactInteger scale;
    z_ten_to(work, &scale);
    const ZetaRow *const g = rs_gamma(k, count, work);
    for (int64_t m = 0; m < 3; m += 1)
    {
        OriorExactInteger total;
        orior_exact_zero(&total);
        for (int64_t i = 0; i < count; i += 1)
        {
            OriorExactInteger value;
            z_abs(&g->values[i], &value);
            OriorExactInteger choose;
            binomial_small(i, m, &choose);
            z_mul(&value, &choose, &value);
            z_mul_small(&value, (m == 2) ? 2 : 1, &value);
            z_add(&total, &value, &total);
        }
        OriorExactInteger extra;
        z_set(&extra, count + 1);
        z_add(&total, &extra, &total);
        rational_make(&total, &scale, &bounds[m]);
    }
}

/**
 * @brief The bounds of the cell, whole units at the work, shared by each ball.
 *
 * @param[in]  nu     The cell.
 * @param[in]  work   The places.
 * @param[out] bounds B1 on |M'|, B2 on |M''|, and R1 on the derivative of the two terms of R [BORROWS].
 * @param[out] rho    The bound of Gabcke on what R through C_1 leaves out [BORROWS].
 * @note M = 2 Re sum z_n + R, and R = (-1)^(nu-1) sum over k <= 1 of C_k(z) x^(-k-1/2). Here z = 1 - 2(x - nu) and
 *       x = sqrt(t / 2pi). The bound rho is |R| <= 0.053 t^(-5/4) for t >= 200 (Gabcke; Hiary, (1.4)), at the least
 *       t of the cell.
 * @note Each wave is a vector of length n^(-1/2) that turns at theta' - ln n. It is at most ln((nu + 1) / n) +
 *       1 / t^2 in size over the cell, and theta'' is at most 1 / t. |dx/dt| = 1 / (4pi x), and
 *       |d^2x/dt^2| = 1 / (16pi^2 x^3). Each bound is a sum of magnitudes, with each factor raised where read.
 */
static void budgets(int64_t nu, int32_t work, OriorExactInteger bounds[3], OriorExactInteger *rho)
{
    OriorExactInteger scale;
    z_ten_to(work, &scale);
    ZetaRational pi_low;
    rational_small(314159, 100000, &pi_low);
    ZetaRational t_low;
    ZetaRational value;
    rational_small(2 * nu * nu, 1, &value);
    rational_mul(&value, &pi_low, &t_low);
    ZetaRational one;
    rational_small(1, 1, &one);
    ZetaRational t_square;
    rational_mul(&t_low, &t_low, &t_square);
    ZetaRational inverse_square;
    rational_div(&one, &t_square, &inverse_square);
    ZetaRational inverse_t;
    rational_div(&one, &t_low, &inverse_t);
    OriorExactInteger root_nu;
    OriorExactInteger nu_value;
    z_set(&nu_value, nu);
    z_isqrt(&nu_value, &root_nu);
    ZetaRational root;
    OriorExactInteger unit_one;
    z_set(&unit_one, 1);
    rational_make(&unit_one, &root_nu, &root);
    OriorExactInteger log_top;
    zeta_ln(nu + 1, work, &log_top);
    ZetaRational spin_sum;
    ZetaRational square_sum;
    rational_small(0, 1, &spin_sum);
    rational_small(0, 1, &square_sum);
    for (int64_t n = 1; n <= nu; n += 1)
    {
        OriorExactInteger log;
        zeta_ln(n, work, &log);
        OriorExactInteger top;
        z_sub(&log_top, &log, &top);
        OriorExactInteger two;
        z_set(&two, 2);
        z_add(&top, &two, &top);
        ZetaRational spin;
        rational_make(&top, &scale, &spin);
        rational_add(&spin, &inverse_square, &spin);
        OriorExactInteger square;
        z_mul(&scale, &scale, &square);
        OriorExactInteger under;
        z_set(&under, n);
        z_floor_div(&square, &under, &square);
        z_isqrt(&square, &top);
        z_add(&top, &two, &top);
        ZetaRational size;
        rational_make(&top, &scale, &size);
        ZetaRational part;
        rational_mul(&size, &spin, &part);
        rational_add(&spin_sum, &part, &spin_sum);
        rational_mul(&spin, &spin, &part);
        rational_add(&part, &inverse_t, &part);
        rational_mul(&size, &part, &part);
        rational_add(&square_sum, &part, &square_sum);
    }
    ZetaRational speed;
    ZetaRational pull;
    ZetaRational four;
    rational_small(4 * nu, 1, &four);
    rational_mul(&four, &pi_low, &speed);
    rational_div(&one, &speed, &speed);
    ZetaRational sixteen;
    rational_small(16 * nu * nu * nu, 1, &sixteen);
    rational_mul(&sixteen, &pi_low, &pull);
    rational_mul(&pull, &pi_low, &pull);
    rational_div(&one, &pull, &pull);
    ZetaRational first;
    ZetaRational second;
    rational_small(0, 1, &first);
    rational_small(0, 1, &second);
    ZetaRational nu_rational;
    rational_small(nu, 1, &nu_rational);
    for (int64_t k = 0; k < 2; k += 1)
    {
        ZetaRational level[3];
        magnitudes(k, work, level);
        ZetaRational fall = root;
        for (int64_t at = 0; at < k; at += 1)
        {
            rational_div(&fall, &nu_rational, &fall);
        }
        ZetaRational half;
        rational_small((2 * k) + 1, 2, &half);
        ZetaRational part;
        ZetaRational term;
        rational_small(2, 1, &term);
        rational_mul(&term, &level[1], &term);
        rational_mul(&half, &level[0], &part);
        rational_div(&part, &nu_rational, &part);
        rational_add(&term, &part, &term);
        rational_mul(&term, &fall, &term);
        rational_add(&first, &term, &first);
        rational_small(4, 1, &term);
        rational_mul(&term, &level[2], &term);
        rational_small(4, 1, &part);
        rational_mul(&part, &half, &part);
        rational_mul(&part, &level[1], &part);
        rational_div(&part, &nu_rational, &part);
        rational_add(&term, &part, &term);
        ZetaRational half_next;
        rational_add(&half, &one, &half_next);
        rational_mul(&half, &half_next, &part);
        rational_mul(&part, &level[0], &part);
        rational_div(&part, &nu_rational, &part);
        rational_div(&part, &nu_rational, &part);
        rational_add(&term, &part, &term);
        rational_mul(&term, &fall, &term);
        rational_add(&second, &term, &second);
    }
    ZetaRational r_one;
    rational_mul(&first, &speed, &r_one);
    ZetaRational r_two;
    rational_mul(&second, &speed, &r_two);
    rational_mul(&r_two, &speed, &r_two);
    ZetaRational part;
    rational_mul(&first, &pull, &part);
    rational_add(&r_two, &part, &r_two);
    ZetaRational b_one;
    ZetaRational two;
    rational_small(2, 1, &two);
    rational_mul(&two, &spin_sum, &b_one);
    rational_add(&b_one, &r_one, &b_one);
    ZetaRational b_two;
    rational_mul(&two, &square_sum, &b_two);
    rational_add(&b_two, &r_two, &b_two);
    OriorExactInteger whole;
    z_floor_div(&t_low.top, &t_low.under, &whole);
    OriorExactInteger quarter;
    z_isqrt(&whole, &quarter);
    z_isqrt(&quarter, &quarter);
    ZetaRational bound;
    OriorExactInteger top;
    z_set(&top, 53);
    OriorExactInteger bottom;
    z_mul(&whole, &quarter, &bottom);
    z_mul_small(&bottom, 1000, &bottom);
    rational_make(&top, &bottom, &bound);
    ceiling(&b_one, &scale, &bounds[0]);
    ceiling(&b_two, &scale, &bounds[1]);
    ceiling(&r_one, &scale, &bounds[2]);
    ceiling(&bound, &scale, rho);
}

/**
 * @brief Each ball at x = nu + j / 2^depth, at the digits + ZETA_GUARD.
 *
 * @param[in,out] cell  The cell [BORROWS].
 * @param[in]     j     The numerator.
 * @param[in]     depth The bits of the denominator.
 * @param[out]    point The balls [BORROWS]. The caller frees them with motion_point_free.
 * @note Each ball holds t, theta, and M = 2 Re W + R through C_1. It holds the part of M' of the waves,
 *       -2 Im sum (theta' - ln n) z_n. It holds the budgets of the point: E on M and E' on M'.
 * @note E sums magnitudes. The floors of each wave give at most 20 t + 1000 units a wave, twice over for 2 Re. Those
 *       are the floors of its logarithm, power, cosine and sine, and the turn. The rotation of the frame gives at most
 *       the floor of theta times |W| <= nu, twice. The two terms of R read at the digits give 4 10^ZETA_GUARD units.
 */
static void motion_point(MotionCell *cell, const OriorExactInteger *j, int64_t depth, MotionPoint *point)
{
    cell->asked += 1u;
    const int64_t nu = cell->nu;
    const int32_t work = cell->digits + ZETA_GUARD;
    const OriorExactInteger *const scale = &cell->scale;
    DecimalPair p;
    OriorExactInteger five;
    z_set(&five, 1);
    for (int64_t at = 0; at < depth; at += 1)
    {
        z_mul_small(&five, 5, &five);
    }
    OriorExactInteger value;
    z_mul(j, &five, &value);
    // The depth of a bisection is small, and fits the places.
    pair_make(&value, (int32_t)depth, &p);
    DecimalPair t;
    rs_time_at(nu, &p, work, &t);
    const int64_t n_sum = rs_theta_at(&t, work, &point->theta);
    point->t = t.numerator;
    OriorExactInteger cosine;
    OriorExactInteger sine;
    zeta_cos_sin(&point->theta, work, &cosine, &sine);
    ZetaPoint at_point;
    pair_small(5, 1, &at_point.sigma);
    at_point.t = t;
    OriorExactInteger(*const waves)[2] =
        (OriorExactInteger(*)[2])siegel_memory(malloc((size_t)nu * sizeof(OriorExactInteger[2])));
    for (int64_t n = 1; n <= nu; n += 1)
    {
        OriorExactInteger wave[4];
        zeta_power_minus_s(n, &at_point, work, wave);
        OriorExactInteger first;
        OriorExactInteger second;
        z_mul(&cosine, &wave[0], &first);
        z_mul(&sine, &wave[1], &second);
        z_sub(&first, &second, &waves[n - 1][0]);
        z_floor_div(&waves[n - 1][0], scale, &waves[n - 1][0]);
        z_mul(&sine, &wave[0], &first);
        z_mul(&cosine, &wave[1], &second);
        z_add(&first, &second, &waves[n - 1][1]);
        z_floor_div(&waves[n - 1][1], scale, &waves[n - 1][1]);
    }
    OriorExactInteger turn;
    theta_prime_at(&t, work, &turn);
    OriorExactInteger *const rates = (OriorExactInteger *)siegel_memory(malloc((size_t)nu * sizeof(*rates)));
    orior_exact_zero(&point->rate_sum);
    for (int64_t n = 1; n <= nu; n += 1)
    {
        z_sub(&turn, &cell->logs[n - 1], &rates[n - 1]);
        OriorExactInteger square;
        z_mul(&rates[n - 1], &rates[n - 1], &square);
        OriorExactInteger under;
        z_set(&under, n);
        z_floor_div(&square, &under, &square);
        z_add(&point->rate_sum, &square, &point->rate_sum);
    }
    OriorExactInteger r;
    rs_remainder_at(nu, &p, 1, cell->digits, &r);
    point->m = (OriorExactInteger *)siegel_memory(malloc(cell->ball_count * sizeof(OriorExactInteger)));
    point->d = (OriorExactInteger *)siegel_memory(malloc(cell->ball_count * sizeof(OriorExactInteger)));
    for (size_t ball = 0u; ball < cell->ball_count; ball += 1u)
    {
        OriorExactInteger re_sum;
        OriorExactInteger slope;
        orior_exact_zero(&re_sum);
        orior_exact_zero(&slope);
        for (int64_t n = 0; n < nu; n += 1)
        {
            OriorExactInteger seen[2];
            turned(cell->balls[ball][n], waves[n], scale, seen);
            z_add(&re_sum, &seen[0], &re_sum);
            OriorExactInteger product;
            z_mul(&rates[n], &seen[1], &product);
            z_add(&slope, &product, &slope);
        }
        z_mul_small(&re_sum, 2, &point->m[ball]);
        z_add(&point->m[ball], &r, &point->m[ball]);
        z_mul_small(&slope, -2, &slope);
        z_floor_div(&slope, scale, &point->d[ball]);
    }
    OriorExactInteger drift;
    theta_floor(&t, n_sum, work, &drift);
    OriorExactInteger whole;
    z_floor_div(&t.numerator, scale, &whole);
    OriorExactInteger extra;
    z_set(&extra, 1);
    z_add(&whole, &extra, &whole);
    z_mul_small(&whole, 20, &point->e);
    z_set(&extra, 1000);
    z_add(&point->e, &extra, &point->e);
    z_mul_small(&point->e, 2 * nu, &point->e);
    OriorExactInteger part;
    z_mul_small(&drift, 2 * nu, &part);
    z_add(&point->e, &part, &point->e);
    z_ten_to(ZETA_GUARD, &part);
    z_mul_small(&part, 4, &part);
    z_add(&point->e, &part, &point->e);
    z_set(&extra, 1);
    z_add(&point->e, &extra, &point->e);
    int64_t length = 0;
    for (int64_t rest = nu; rest > 0; rest >>= 1)
    {
        length += 1;
    }
    z_mul_small(&point->e, length + 2, &point->e_d);
    z_mul_small(&drift, 2 * nu, &part);
    z_add(&point->e_d, &part, &point->e_d);
    free(rates);
    free(waves);
}

/**
 * @brief Frees the balls of a point.
 *
 * @param[in,out] point The balls [BORROWS].
 */
static void motion_point_free(MotionPoint *point)
{
    free(point->m);
    free(point->d);
}

/** @brief The verdicts of settle: none of the budgets decides, no zero, or one zero. */
enum
{
    SETTLE_OPEN = -1,
    SETTLE_NONE = 0,
    SETTLE_ONE = 1
};

/**
 * @brief Whether ball k's M has no zero between two points, exactly one, or neither, by the budgets.
 *
 * @param[in] left  The left point [BORROWS].
 * @param[in] right The right point [BORROWS].
 * @param[in] k     The ball.
 * @param[in] cell  The cell [BORROWS].
 * @param[in] rho   The bound on |Z - M|, or zero for M itself [BORROWS].
 * @return          SETTLE_NONE, SETTLE_ONE or SETTLE_OPEN.
 * @note With rho, the same for Z itself, and SETTLE_ONE then means at least one.
 */
static int motion_settle(const MotionPoint *left, const MotionPoint *right, size_t k, const MotionCell *cell,
                         const OriorExactInteger *rho)
{
    const OriorExactInteger *const a = &left->m[k];
    const OriorExactInteger *const b = &right->m[k];
    OriorExactInteger e = (z_cmp(&left->e, &right->e) >= 0) ? left->e : right->e;
    z_add(&e, rho, &e);
    OriorExactInteger reach;
    z_sub(&right->t, &left->t, &reach);
    OriorExactInteger size_a;
    OriorExactInteger size_b;
    z_abs(a, &size_a);
    z_abs(b, &size_b);
    if ((z_cmp(&size_a, &e) <= 0) || (z_cmp(&size_b, &e) <= 0))
    {
        return SETTLE_OPEN;
    }
    OriorExactInteger twice;
    z_mul_small(&e, 2, &twice);
    if ((a->sign > 0) == (b->sign > 0))
    {
        OriorExactInteger room;
        z_add(&size_a, &size_b, &room);
        OriorExactInteger travel;
        times_over(&cell->bounds[0], &reach, &cell->scale, &travel);
        z_sub(&room, &travel, &room);
        return (z_cmp(&room, &twice) > 0) ? SETTLE_NONE : SETTLE_OPEN;
    }
    if (rho->sign != 0)
    {
        return SETTLE_ONE;
    }
    const OriorExactInteger *const da = &left->d[k];
    const OriorExactInteger *const db = &right->d[k];
    OriorExactInteger ed = (z_cmp(&left->e_d, &right->e_d) >= 0) ? left->e_d : right->e_d;
    z_add(&ed, &cell->bounds[2], &ed);
    OriorExactInteger size_da;
    OriorExactInteger size_db;
    z_abs(da, &size_da);
    z_abs(db, &size_db);
    OriorExactInteger room;
    z_add(&size_da, &size_db, &room);
    OriorExactInteger travel;
    times_over(&cell->bounds[1], &reach, &cell->scale, &travel);
    z_sub(&room, &travel, &room);
    OriorExactInteger twice_ed;
    z_mul_small(&ed, 2, &twice_ed);
    const int certain = ((da->sign > 0) == (db->sign > 0)) && (z_cmp(&size_da, &ed) > 0) &&
                        (z_cmp(&size_db, &ed) > 0) && (z_cmp(&room, &twice_ed) > 0);
    return certain ? SETTLE_ONE : SETTLE_OPEN;
}

/**
 * @brief Settles each pending ball on [j, j + 1] / 2^depth, or halves the interval until its budgets decide.
 *
 * @param[in,out] cell    The cell [BORROWS].
 * @param[in]     j       The numerator of the left end.
 * @param[in]     depth   The bits.
 * @param[in]     left    The left point [BORROWS].
 * @param[in]     right   The right point [BORROWS].
 * @param[in]     pending The balls still open [BORROWS].
 * @param[in]     count   How many.
 */
static void motion_walk(MotionCell *cell, const OriorExactInteger *j, int64_t depth, const MotionPoint *left,
                        const MotionPoint *right, const size_t *pending, size_t count)
{
    size_t *const unsure = (size_t *)siegel_memory(malloc((count + 1u) * sizeof(size_t)));
    size_t unsure_count = 0u;
    OriorExactInteger zero;
    orior_exact_zero(&zero);
    for (size_t at = 0u; at < count; at += 1u)
    {
        const size_t k = pending[at];
        const int found = motion_settle(left, right, k, cell, &zero);
        cell->zeros[k] += (found == SETTLE_ONE) ? 1 : 0;
        if (found == SETTLE_OPEN)
        {
            unsure[unsure_count] = k;
            unsure_count += 1u;
        }
        if ((k == 0u) && (found != SETTLE_OPEN))
        {
            const int sure = motion_settle(left, right, 0u, cell, &cell->rho);
            cell->truth[0] += (sure == SETTLE_ONE) ? 1 : 0;
            cell->truth[1] += (sure == SETTLE_OPEN) ? 1 : 0;
        }
    }
    if ((unsure_count > 0u) && (depth < (cell->bits + cell->deepest)))
    {
        OriorExactInteger twice;
        z_mul_small(j, 2, &twice);
        OriorExactInteger odd;
        OriorExactInteger one;
        z_set(&one, 1);
        z_add(&twice, &one, &odd);
        MotionPoint middle_point;
        motion_point(cell, &odd, depth + 1, &middle_point);
        motion_walk(cell, &twice, depth + 1, left, &middle_point, unsure, unsure_count);
        motion_walk(cell, &odd, depth + 1, &middle_point, right, unsure, unsure_count);
        motion_point_free(&middle_point);
    }
    if (depth >= (cell->bits + cell->deepest))
    {
        for (size_t at = 0u; at < unsure_count; at += 1u)
        {
            cell->open[unsure[at]] += 1;
        }
    }
    free(unsure);
}

/** @brief The certified sign of M at each Gram index, per ball: -1 absent, 0 negative, 1 positive. */
typedef struct
{
    int *sign;       /**< The signs by index [OWNS]. */
    size_t capacity; /**< How many indices the array holds. */
} GramSigns;

/**
 * @brief Records the sign at a Gram index.
 *
 * @param[in,out] signs The signs [BORROWS].
 * @param[in]     index The index.
 * @param[in]     value 0 or 1.
 */
static void gram_signs_set(GramSigns *signs, int64_t index, int value)
{
    // The indices of the Gram points in these cells are positive and small.
    const size_t at = (size_t)index;
    if (at >= signs->capacity)
    {
        size_t grown = (signs->capacity == 0u) ? 256u : signs->capacity;
        while (grown <= at)
        {
            grown *= 2u;
        }
        signs->sign = (int *)siegel_memory(realloc(signs->sign, grown * sizeof(int)));
        for (size_t fill = signs->capacity; fill < grown; fill += 1u)
        {
            signs->sign[fill] = -1;
        }
        signs->capacity = grown;
    }
    signs->sign[at] = value;
}

/**
 * @brief The middle of a list, as the value at half its length in ascending order.
 *
 * @param[in] values The values [BORROWS].
 * @param[in] count  How many.
 * @return           The middle.
 */
static int64_t middle_of(const int64_t *values, size_t count)
{
    int64_t *const sorted = (int64_t *)siegel_memory(malloc((count + 1u) * sizeof(int64_t)));
    memcpy(sorted, values, count * sizeof(int64_t));
    for (size_t at = 1u; at < count; at += 1u)
    {
        const int64_t moving = sorted[at];
        size_t place = at;
        while ((place > 0u) && (moving < sorted[place - 1u]))
        {
            sorted[place] = sorted[place - 1u];
            place -= 1u;
        }
        sorted[place] = moving;
    }
    const int64_t result = sorted[count / 2u];
    free(sorted);
    return result;
}

/**
 * @brief The first value of largest size in a list, as max with key abs of Python.
 *
 * @param[in] values The values [BORROWS].
 * @param[in] count  How many.
 * @return           The value.
 */
static int64_t largest_of(const int64_t *values, size_t count)
{
    int64_t best = values[0];
    for (size_t at = 1u; at < count; at += 1u)
    {
        const int64_t size = (values[at] < 0) ? -values[at] : values[at];
        const int64_t best_size = (best < 0) ? -best : best;
        best = (size > best_size) ? values[at] : best;
    }
    return best;
}

/**
 * @brief The main of the motion mode: the ball over cells first to last against its locked and unlocked nulls.
 *
 * @param[in] first  The first cell. Cells start at 6, where t >= 200 and the bound of Gabcke holds.
 * @param[in] last   The last cell.
 * @param[in] bits   The bits of the grid of each cell.
 * @param[in] draws  The draws of each null.
 * @param[in] digits The places.
 * @return           The exit code.
 * @note The identity is the ball as it is. Two nulls keep each mass 1/n, each spin theta' - ln n, the sphere H_nu,
 *       the frame theta and R. They differ only in the directions where the waves start the cell.
 * @note The locked null permutes the directions of the primes among the primes, and carries them to each n by its
 *       factors. The unlocked null permutes the directions of 2 to nu among themselves, with no regard to factors.
 * @note Per cell, the report gives the flips of the frame and the zeros of M of the identity, with their excess over
 *       the flips. It gives the zeros of Z itself that the budgets certify, the excess of the nulls, and the
 *       intervals left open.
 * @note At the end, the excess summed over the cells. Then the share of Gram points where the certified sign of M
 *       agrees with the sign two Gram points on. That is the law of Gram.
 */
static int motion_main(int64_t first, int64_t last, int64_t bits, int64_t draws, int32_t digits)
{
    FILE *const out = stdout;
    const int32_t work = digits + ZETA_GUARD;
    first = (first > 6) ? first : 6;
    const size_t ball_count = 1u + (2u * (size_t)draws);
    int64_t *const totals = (int64_t *)siegel_memory(calloc(ball_count, sizeof(int64_t)));
    GramSigns *const gram = (GramSigns *)siegel_memory(calloc(ball_count, sizeof(GramSigns)));
    OriorExactInteger big_pi;
    zeta_pi(work, &big_pi);
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        const int64_t begun = now_ms();
        MotionCell cell;
        memset(&cell, 0, sizeof(cell));
        cell.nu = nu;
        cell.digits = digits;
        cell.ball_count = ball_count;
        cell.bits = bits;
        cell.deepest = 12;
        z_ten_to(work, &cell.scale);
        OriorExactInteger *const logs = (OriorExactInteger *)siegel_memory(malloc((size_t)nu * sizeof(*logs)));
        for (int64_t n = 1; n <= nu; n += 1)
        {
            zeta_ln(n, work, &logs[n - 1]);
        }
        cell.logs = logs;
        // The heads: each wave at x = nu, times n^(1/2).
        DecimalPair zero;
        orior_exact_zero(&zero.numerator);
        zero.places = 0;
        DecimalPair t;
        rs_time_at(nu, &zero, work, &t);
        OriorExactInteger theta;
        (void)rs_theta_at(&t, work, &theta);
        OriorExactInteger cosine;
        OriorExactInteger sine;
        zeta_cos_sin(&theta, work, &cosine, &sine);
        ZetaPoint at_point;
        pair_small(5, 1, &at_point.sigma);
        at_point.t = t;
        OriorExactInteger(*const heads)[2] =
            (OriorExactInteger(*)[2])siegel_memory(malloc((size_t)nu * sizeof(OriorExactInteger[2])));
        for (int64_t n = 1; n <= nu; n += 1)
        {
            OriorExactInteger wave[4];
            zeta_power_minus_s(n, &at_point, work, wave);
            OriorExactInteger first_part;
            OriorExactInteger second_part;
            OriorExactInteger re;
            OriorExactInteger im;
            z_mul(&cosine, &wave[0], &first_part);
            z_mul(&sine, &wave[1], &second_part);
            z_sub(&first_part, &second_part, &re);
            z_floor_div(&re, &cell.scale, &re);
            z_mul(&sine, &wave[0], &first_part);
            z_mul(&cosine, &wave[1], &second_part);
            z_add(&first_part, &second_part, &im);
            z_floor_div(&im, &cell.scale, &im);
            OriorExactInteger square;
            z_mul(&cell.scale, &cell.scale, &square);
            z_mul_small(&square, n, &square);
            OriorExactInteger root;
            z_isqrt(&square, &root);
            times_over(&re, &root, &cell.scale, &heads[n - 1][0]);
            times_over(&im, &root, &cell.scale, &heads[n - 1][1]);
        }
        OriorExactInteger(**const balls)[2] =
            (OriorExactInteger(**)[2])siegel_memory(malloc(ball_count * sizeof(OriorExactInteger(*)[2])));
        for (size_t ball = 0u; ball < ball_count; ball += 1u)
        {
            balls[ball] = (OriorExactInteger(*)[2])siegel_memory(malloc((size_t)nu * sizeof(OriorExactInteger[2])));
        }
        for (int64_t n = 0; n < nu; n += 1)
        {
            balls[0][n][0] = cell.scale;
            orior_exact_zero(&balls[0][n][1]);
        }
        for (int locked_pass = 0; locked_pass < 2; locked_pass += 1)
        {
            for (int64_t k = 0; k < draws; k += 1)
            {
                const size_t ball = 1u + ((size_t)locked_pass * (size_t)draws) + (size_t)k;
                // The seed of each draw is a small offset from the seed of the shuffles.
                null_turns(heads, nu, locked_pass == 0, SHUFFLE_SEED + (uint32_t)k, &cell.scale, balls[ball]);
            }
        }
        cell.balls = balls;
        budgets(nu, work, cell.bounds, &cell.rho);
        cell.zeros = (int64_t *)siegel_memory(calloc(ball_count, sizeof(int64_t)));
        cell.open = (int64_t *)siegel_memory(calloc(ball_count, sizeof(int64_t)));
        const size_t grid_count = ((size_t)1 << bits) + 1u;
        MotionPoint *const grid = (MotionPoint *)siegel_memory(malloc(grid_count * sizeof(MotionPoint)));
        for (size_t j = 0u; j < grid_count; j += 1u)
        {
            OriorExactInteger numerator;
            // The grid has 2^bits + 1 points, and j fits a machine integer.
            z_set(&numerator, (int64_t)j);
            motion_point(&cell, &numerator, bits, &grid[j]);
        }
        size_t *const all = (size_t *)siegel_memory(malloc(ball_count * sizeof(size_t)));
        for (size_t ball = 0u; ball < ball_count; ball += 1u)
        {
            all[ball] = ball;
        }
        for (size_t j = 0u; (j + 1u) < grid_count; j += 1u)
        {
            OriorExactInteger numerator;
            // j is below the grid count.
            z_set(&numerator, (int64_t)j);
            motion_walk(&cell, &numerator, bits, &grid[j], &grid[j + 1u], all, ball_count);
        }
        int64_t *const index = (int64_t *)siegel_memory(malloc(grid_count * sizeof(int64_t)));
        for (size_t j = 0u; j < grid_count; j += 1u)
        {
            OriorExactInteger ratio;
            z_floor_div(&grid[j].theta, &big_pi, &ratio);
            index[j] = z_small(&ratio);
        }
        const int64_t flips = index[grid_count - 1u] - index[0];
        for (size_t j = 0u; (j + 1u) < grid_count; j += 1u)
        {
            for (int64_t k = index[j] + 1; k <= index[j + 1u]; k += 1)
            {
                OriorExactInteger low;
                // j is below the grid count.
                z_set(&low, (int64_t)j);
                int64_t depth = bits;
                OriorExactInteger bound;
                z_mul_small(&big_pi, k, &bound);
                while (depth < (bits + 16))
                {
                    z_mul_small(&low, 2, &low);
                    depth += 1;
                    OriorExactInteger probe;
                    OriorExactInteger one;
                    z_set(&one, 1);
                    z_add(&low, &one, &probe);
                    OriorExactInteger angle;
                    theta_x(nu, &probe, depth, work, &angle);
                    if (z_cmp(&angle, &bound) < 0)
                    {
                        low = probe;
                    }
                }
                MotionPoint here;
                motion_point(&cell, &low, depth, &here);
                for (size_t ball = 0u; ball < ball_count; ball += 1u)
                {
                    OriorExactInteger size;
                    z_abs(&here.m[ball], &size);
                    if (z_cmp(&size, &here.e) > 0)
                    {
                        gram_signs_set(&gram[ball], k, here.m[ball].sign > 0);
                    }
                }
                motion_point_free(&here);
            }
        }
        OriorExactInteger harmonic;
        orior_exact_zero(&harmonic);
        for (int64_t n = 1; n <= nu; n += 1)
        {
            OriorExactInteger under;
            OriorExactInteger part;
            z_set(&under, n);
            z_floor_div(&cell.scale, &under, &part);
            z_add(&harmonic, &part, &harmonic);
        }
        OriorExactInteger expected;
        orior_exact_zero(&expected);
        for (size_t j = 0u; j < grid_count; j += 1u)
        {
            OriorExactInteger value;
            z_mul(&grid[j].rate_sum, &cell.scale, &value);
            z_floor_div(&value, &harmonic, &value);
            z_isqrt(&value, &value);
            // j is below the grid count.
            z_mul_small(&value, 4 * ((nu << bits) + (int64_t)j), &value);
            floor_bits(&value, bits, &value);
            z_mul_small(&value, ((j == 0u) || ((j + 1u) == grid_count)) ? 1 : 2, &value);
            z_add(&expected, &value, &expected);
        }
        floor_bits(&expected, bits + 1, &expected);
        int64_t *const excess = (int64_t *)siegel_memory(malloc(ball_count * sizeof(int64_t)));
        int64_t open_nulls = 0;
        for (size_t ball = 0u; ball < ball_count; ball += 1u)
        {
            excess[ball] = cell.zeros[ball] - flips;
            totals[ball] += excess[ball];
            open_nulls += (ball > 0u) ? cell.open[ball] : 0;
        }
        const size_t draw_count = (size_t)draws;
        char texts[4][256];
        OriorExactInteger read;
        OriorExactInteger drop;
        z_ten_to(work - 1, &drop);
        z_floor_div(&expected, &drop, &read);
        signed_text(&read, 1, texts[0], sizeof(texts[0]));
        z_ten_to(work - 3, &drop);
        z_floor_div(&cell.bounds[0], &drop, &read);
        signed_text(&read, 3, texts[1], sizeof(texts[1]));
        z_floor_div(&cell.bounds[1], &drop, &read);
        signed_text(&read, 3, texts[2], sizeof(texts[2]));
        z_ten_to(work - 9, &drop);
        z_floor_div(&cell.rho, &drop, &read);
        signed_text(&read, 9, texts[3], sizeof(texts[3]));
        fprintf(
            out,
            "  cell %lld  flips %lld  identity zeros of M %lld, excess %lld, Z itself at least %lld (%lld left open "
            "by rho)  locked excess median %lld largest %lld  unlocked excess median %lld largest %lld  unsettled "
            "intervals: identity %lld, nulls %lld  Rice %s  B1 %s B2 %s rho %s  points %llu  %lld ms\n",
            (long long)nu, (long long)flips, (long long)cell.zeros[0], (long long)excess[0], (long long)cell.truth[0],
            (long long)cell.truth[1], (long long)middle_of(excess + 1, draw_count),
            (long long)largest_of(excess + 1, draw_count), (long long)middle_of(excess + 1 + draw_count, draw_count),
            (long long)largest_of(excess + 1 + draw_count, draw_count), (long long)cell.open[0], (long long)open_nulls,
            texts[0], texts[1], texts[2], texts[3], (unsigned long long)cell.asked, (long long)(now_ms() - begun));
        fflush(out);
        free(excess);
        free(index);
        for (size_t j = 0u; j < grid_count; j += 1u)
        {
            motion_point_free(&grid[j]);
        }
        free(grid);
        free(all);
        for (size_t ball = 0u; ball < ball_count; ball += 1u)
        {
            free(balls[ball]);
        }
        free(balls);
        free(heads);
        free(logs);
        free(cell.zeros);
        free(cell.open);
    }
    int64_t *const agree = (int64_t *)siegel_memory(calloc(ball_count, sizeof(int64_t)));
    int64_t *const pairs = (int64_t *)siegel_memory(calloc(ball_count, sizeof(int64_t)));
    for (size_t ball = 0u; ball < ball_count; ball += 1u)
    {
        const GramSigns *const signs = &gram[ball];
        for (size_t at = 0u; (at + 2u) < signs->capacity; at += 1u)
        {
            if ((signs->sign[at] >= 0) && (signs->sign[at + 2u] >= 0))
            {
                pairs[ball] += 1;
                agree[ball] += signs->sign[at] == signs->sign[at + 2u];
            }
        }
    }
    const size_t draw_count = (size_t)draws;
    fprintf(out,
            "  over cells %lld to %lld: excess of zeros over flips, identity %lld, locked median %lld largest %lld, "
            "unlocked median %lld largest %lld\n",
            (long long)first, (long long)last, (long long)totals[0], (long long)middle_of(totals + 1, draw_count),
            (long long)largest_of(totals + 1, draw_count), (long long)middle_of(totals + 1 + draw_count, draw_count),
            (long long)largest_of(totals + 1 + draw_count, draw_count));
    fprintf(out,
            "  Gram's law as agreement at lag 2 over the certified signs: identity %lld of %lld, locked median %lld of "
            "%lld, unlocked median %lld of %lld\n",
            (long long)agree[0], (long long)pairs[0], (long long)middle_of(agree + 1, draw_count),
            (long long)middle_of(pairs + 1, draw_count), (long long)middle_of(agree + 1 + draw_count, draw_count),
            (long long)middle_of(pairs + 1 + draw_count, draw_count));
    for (size_t ball = 0u; ball < ball_count; ball += 1u)
    {
        free(gram[ball].sign);
    }
    free(gram);
    free(agree);
    free(pairs);
    free(totals);
    return 0;
}

/* ---------------------------------------------------------------- the triangle between the two pairs */

/** @brief The device program of the tail coefficient, and its two sources. */
#define TAIL_BUILD "examples/0_experimental/exact_zeta_tail.sh"
#define TAIL_SOURCE "examples/0_experimental/exact_zeta_tail.cu"

/** @brief The bits of the place of a crossing. */
#define SPIKE_BITS 64

/** @brief The C of Euler-Maclaurin at many points at once, one lane a point, on the device. */
typedef struct
{
    char binary[512]; /**< The device program. */
    uint64_t swept;   /**< How many points the device swept. */
} DeviceTail;

/**
 * @brief The path of the newest device program newer than its sources, else the one that exact_zeta_tail.sh builds.
 *
 * @param[out] binary The path [BORROWS]. 512 bytes.
 */
static void tail_build(char binary[512])
{
    struct stat facts;
    double newest_source = 0.0;
    if (stat(TAIL_BUILD, &facts) == 0)
    {
        newest_source = (double)facts.st_mtime;
    }
    if ((stat(TAIL_SOURCE, &facts) == 0) && ((double)facts.st_mtime > newest_source))
    {
        newest_source = (double)facts.st_mtime;
    }
    binary[0] = '\0';
    double newest = 0.0;
    DIR *const folder = opendir("build");
    if (folder != NULL)
    {
        const char *const tail = "_exact_zeta_tail";
        struct dirent *entry;
        while ((entry = readdir(folder)) != NULL)
        {
            const size_t length = strlen(entry->d_name);
            if ((length < strlen(tail)) || (strcmp(entry->d_name + length - strlen(tail), tail) != 0))
            {
                continue;
            }
            static const char *const names[2] = {"exact_zeta_tail.exe", "exact_zeta_tail"};
            for (unsigned which = 0u; which < 2u; which += 1u)
            {
                char path[512];
                snprintf(path, sizeof(path), "build/%.200s/%s", entry->d_name, names[which]);
                if ((stat(path, &facts) == 0) && ((double)facts.st_mtime > newest))
                {
                    newest = (double)facts.st_mtime;
                    snprintf(binary, 512u, "%s", path);
                }
            }
        }
        closedir(folder);
    }
    if ((binary[0] != '\0') && (newest > newest_source))
    {
        return;
    }
    binary[0] = '\0';
    FILE *const made = popen("bash " TAIL_BUILD, "r");
    if (made == NULL)
    {
        zeta_stop("cannot run the build of the device program");
    }
    char line[512];
    while (fgets(line, sizeof(line), made) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0')
        {
            snprintf(binary, 512u, "%s", line);
        }
    }
    if ((pclose(made) != 0) || (binary[0] == '\0'))
    {
        zeta_stop("the build of the device program failed");
    }
}

/**
 * @brief The bits of a signed value: the bits of its size, and one for the sign.
 *
 * @param[in] value The value [BORROWS].
 * @return          The bits.
 */
static int64_t signed_bits(const OriorExactInteger *value)
{
    return z_bit_length(value) + 1;
}

/**
 * @brief Writes the words of a value modulo 2^bits as hex limbs, least significant first.
 *
 * @param[in] handle The file [BORROWS].
 * @param[in] value  The value [BORROWS].
 * @param[in] bits   The width.
 * @param[in] limbs  How many limbs.
 */
static void put_words(FILE *handle, const OriorExactInteger *value, int64_t bits, int64_t limbs)
{
    OriorExactInteger modulus;
    z_two_to(bits, &modulus);
    OriorExactInteger word;
    z_mod(value, &modulus, &word);
    for (int64_t at = 0; at < limbs; at += 1)
    {
        fprintf(handle, "%s%x", (at > 0) ? " " : "", (unsigned)word.limb[at]);
    }
    fputs("\n", handle);
}

/**
 * @brief C at each t, each real and imaginary part at the digits, from the device.
 *
 * @param[in,out] tail   The device [BORROWS].
 * @param[in]     ts     The points [BORROWS].
 * @param[in]     count  How many.
 * @param[in]     n_sum  The cut N.
 * @param[in]     digits The places.
 * @param[out]    values The coefficients, two each [BORROWS].
 * @return               1 where the records of the device equal those of the host.
 * @note The code arranges the records here, and the device builds the program from N and the wrap width W. W is the
 *       bits of a bound on each tau and on the two turns between them. The bound is at the scale S:
 *       |tau_k| <= |B_2k / (2k)!| (|t| + 1)(|t| + 2) ... (|t| + 2k - 1) / N^(2k - 1), with |t| taken up to the power
 *       of two above it. The widths then hold across passes.
 */
static int tail_coefficients(DeviceTail *tail, const DecimalPair *ts, size_t count, int64_t n_sum, int32_t digits,
                             OriorExactInteger (*values)[2])
{
    if (tail->binary[0] == '\0')
    {
        tail_build(tail->binary);
    }
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger *const big_ts = (OriorExactInteger *)siegel_memory(malloc((count + 1u) * sizeof(*big_ts)));
    OriorExactInteger widest_t;
    orior_exact_zero(&widest_t);
    for (size_t at = 0u; at < count; at += 1u)
    {
        OriorExactInteger under;
        z_ten_to(ts[at].places, &under);
        times_over(&ts[at].numerator, &scale, &under, &big_ts[at]);
        OriorExactInteger size;
        z_abs(&big_ts[at], &size);
        widest_t = (z_cmp(&size, &widest_t) > 0) ? size : widest_t;
    }
    OriorExactInteger top;
    z_floor_div(&widest_t, &scale, &top);
    OriorExactInteger one;
    z_set(&one, 1);
    z_add(&top, &one, &top);
    z_two_to(z_bit_length(&top), &top);
    const int64_t top_small = z_small(&top);
    OriorExactInteger *const rho =
        (OriorExactInteger *)siegel_memory(calloc((size_t)n_sum + 1u, sizeof(OriorExactInteger)));
    OriorExactInteger n_square;
    z_set(&n_square, n_sum * n_sum);
    for (int64_t k = 2; k <= n_sum; k += 1)
    {
        ZetaRational this_one;
        ZetaRational before;
        zeta_bernoulli(k, &this_one);
        zeta_bernoulli(k - 1, &before);
        OriorExactInteger num;
        OriorExactInteger den;
        z_mul(&this_one.top, &before.under, &num);
        z_mul(&this_one.under, &before.top, &den);
        z_mul(&den, &n_square, &den);
        const int sign = num.sign * den.sign;
        z_abs(&num, &num);
        z_abs(&den, &den);
        times_over(&num, &scale, &den, &rho[k - 2]);
        z_mul_small(&rho[k - 2], sign, &rho[k - 2]);
    }
    OriorExactInteger widest;
    orior_exact_zero(&widest);
    for (int64_t k = 1; k <= n_sum; k += 1)
    {
        ZetaRational bernoulli;
        zeta_bernoulli(k, &bernoulli);
        OriorExactInteger grown;
        z_set(&grown, 1);
        for (int64_t j = 0; j < ((2 * k) - 1); j += 1)
        {
            z_mul_small(&grown, top_small + j + 1, &grown);
        }
        OriorExactInteger bound;
        z_abs(&bernoulli.top, &bound);
        z_mul(&bound, &grown, &bound);
        z_mul(&bound, &scale, &bound);
        OriorExactInteger under = bernoulli.under;
        for (int64_t j = 0; j < ((2 * k) - 1); j += 1)
        {
            z_mul_small(&under, n_sum, &under);
        }
        z_floor_div(&bound, &under, &bound);
        z_add(&bound, &one, &bound);
        OriorExactInteger reach;
        z_set(&reach, top_small + (2 * k) + 2);
        z_mul(&reach, &reach, &reach);
        z_mul(&bound, &reach, &bound);
        widest = (z_cmp(&bound, &widest) > 0) ? bound : widest;
    }
    const int64_t width = signed_bits(&widest) + 2;
    const size_t field_count = 3u + (size_t)(n_sum - 1);
    int64_t *const bits = (int64_t *)siegel_memory(malloc(field_count * sizeof(int64_t)));
    int64_t *const offsets = (int64_t *)siegel_memory(malloc(field_count * sizeof(int64_t)));
    OriorExactInteger value;
    z_mul(&top, &scale, &value);
    bits[0] = signed_bits(&value);
    bits[1] = z_bit_length(&scale) + 1;
    bits[2] = z_bit_length(&scale) + 1;
    int64_t rho_bits = signed_bits(&one);
    for (int64_t k = 0; k < (n_sum - 1); k += 1)
    {
        const int64_t these = signed_bits(&rho[k]);
        rho_bits = (these > rho_bits) ? these : rho_bits;
    }
    for (size_t at = 3u; at < field_count; at += 1u)
    {
        bits[at] = rho_bits;
    }
    offsets[0] = 0;
    offsets[1] = 0;
    offsets[2] = bits[1];
    for (size_t at = 3u; at < field_count; at += 1u)
    {
        offsets[at] = offsets[at - 1u] + bits[at - 1u];
    }
    const int64_t point_limbs = (bits[0] + 31) / 32;
    const int64_t shared_bits = offsets[field_count - 1u] + bits[field_count - 1u];
    const int64_t shared_limbs = (shared_bits + 31) / 32;
    OriorExactInteger shared = scale;
    OriorExactInteger half;
    OriorExactInteger two;
    z_set(&two, 2);
    z_floor_div(&scale, &two, &half);
    OriorExactInteger power;
    z_two_to(offsets[2], &power);
    z_mul(&half, &power, &half);
    z_add(&shared, &half, &shared);
    for (int64_t k = 0; k < (n_sum - 1); k += 1)
    {
        OriorExactInteger modulus;
        z_two_to(bits[3 + k], &modulus);
        OriorExactInteger part;
        z_mod(&rho[k], &modulus, &part);
        z_two_to(offsets[3 + k], &power);
        z_mul(&part, &power, &part);
        z_add(&shared, &part, &shared);
    }
    char given[256];
    char taken[256];
    snprintf(given, sizeof(given), "build/tail_in_%lld_%d.txt", (long long)n_sum, (int)digits);
    snprintf(taken, sizeof(taken), "build/tail_out_%lld_%d.txt", (long long)n_sum, (int)digits);
    FILE *const input = fopen(given, "wb");
    if (input == NULL)
    {
        zeta_stop("cannot write the input of the device");
    }
    fprintf(input, "%lld %lld\nfields %llu\n", (long long)n_sum, (long long)width, (unsigned long long)field_count);
    for (size_t at = 0u; at < field_count; at += 1u)
    {
        fprintf(input, "%lld %lld\n", (long long)bits[at], (long long)offsets[at]);
    }
    fprintf(input, "in_limbs %lld %lld\npoints %llu\n", (long long)point_limbs, (long long)shared_limbs,
            (unsigned long long)count);
    for (size_t at = 0u; at < count; at += 1u)
    {
        put_words(input, &big_ts[at], bits[0], point_limbs);
    }
    put_words(input, &shared, shared_bits, shared_limbs);
    fclose(input);
    char command[2048];
#ifdef _WIN32
    snprintf(command, sizeof(command), "\"\"%s\" \"%s\" \"%s\" >NUL 2>&1\"", tail->binary, given, taken);
#else
    snprintf(command, sizeof(command), "\"%s\" \"%s\" \"%s\" >/dev/null 2>&1", tail->binary, given, taken);
#endif
    const int status = system(command);
    FILE *const output = fopen(taken, "r");
    if (output == NULL)
    {
        zeta_stop("the device wrote no output");
    }
    static char line[262144];
    long long out_limbs = 0;
    long long re_offset = 0;
    long long re_bits = 0;
    long long im_offset = 0;
    long long im_bits = 0;
    if ((fgets(line, sizeof(line), output) == NULL) || (sscanf(line, "%*s %lld", &out_limbs) != 1) ||
        (fgets(line, sizeof(line), output) == NULL) ||
        (sscanf(line, "%*s %lld %lld %lld %lld", &re_offset, &re_bits, &im_offset, &im_bits) != 4))
    {
        zeta_stop("the device output has no layout");
    }
    for (size_t at = 0u; at < count; at += 1u)
    {
        if (fgets(line, sizeof(line), output) == NULL)
        {
            zeta_stop("the device output is short");
        }
        OriorExactInteger word;
        orior_exact_zero(&word);
        char *cursor = line;
        for (long long limb = 0; (limb < out_limbs) && (limb < (long long)ORIOR_EXACT_LIMBS); limb += 1)
        {
            word.limb[limb] = (uint32_t)strtoul(cursor, &cursor, 16);
        }
        word.sign = 0;
        for (long long limb = 0; limb < out_limbs; limb += 1)
        {
            if (word.limb[limb] != 0u)
            {
                word.sign = 1;
                break;
            }
        }
        const long long places[2][2] = {{re_offset, re_bits}, {im_offset, im_bits}};
        for (int part = 0; part < 2; part += 1)
        {
            OriorExactInteger shifted;
            floor_bits(&word, places[part][0], &shifted);
            OriorExactInteger modulus;
            z_two_to(places[part][1], &modulus);
            z_mod(&shifted, &modulus, &values[at][part]);
            OriorExactInteger high;
            z_two_to(places[part][1] - 1, &high);
            if (z_cmp(&values[at][part], &high) >= 0)
            {
                z_sub(&values[at][part], &modulus, &values[at][part]);
            }
        }
    }
    int same = 0;
    if (fgets(line, sizeof(line), output) != NULL)
    {
        same = atoi(strchr(line, ' ') ? strchr(line, ' ') + 1 : line);
    }
    fclose(output);
    tail->swept += count;
    free(big_ts);
    free(rho);
    free(bits);
    free(offsets);
    return same && (status == 0);
}

/** @brief A point of a cell, x = nu + p, at the digits. */
typedef struct
{
    int64_t nu;     /**< The cell. */
    DecimalPair p;  /**< The delta. */
    int32_t digits; /**< The places. */
} CellKey;

/** @brief S and R, or the value of Euler-Maclaurin, at a key. */
typedef struct
{
    char *key;                /**< The text of the key [OWNS]. */
    OriorExactInteger first;  /**< S, or the value of Euler-Maclaurin. */
    OriorExactInteger second; /**< R. */
} KeptPair;

/** @brief A table of values by key. */
typedef struct
{
    KeptPair *entries;   /**< The open table [OWNS]. */
    size_t bucket_count; /**< How many buckets, a power of two. */
    size_t count;        /**< How many entries. */
} KeptTable;

/**
 * @brief The text of a key: nu, the pair, and the digits.
 *
 * @param[in]  key      The key [BORROWS].
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void key_text(const CellKey *key, char *text, size_t capacity)
{
    static char digits[8192];
    z_text(&key->p.numerator, digits, sizeof(digits));
    snprintf(text, capacity, "%lld/%s/%d/%d", (long long)key->nu, digits, (int)key->p.places, (int)key->digits);
}

/**
 * @brief The entry of a key, or NULL where the table has none.
 *
 * @param[in,out] table  The table [BORROWS].
 * @param[in]     key    The key [BORROWS].
 * @param[in]     create 1 to add an entry where none exists.
 * @return               The entry [BORROWS].
 */
static KeptPair *kept_at(KeptTable *table, const CellKey *key, int create)
{
    if (create && ((table->count * 2u) >= table->bucket_count))
    {
        const size_t grown = (table->bucket_count == 0u) ? 1024u : (table->bucket_count * 2u);
        KeptPair *const entries = (KeptPair *)siegel_memory(calloc(grown, sizeof(KeptPair)));
        for (size_t at = 0u; at < table->bucket_count; at += 1u)
        {
            if (table->entries[at].key == NULL)
            {
                continue;
            }
            size_t place = (size_t)(key_hash(table->entries[at].key) & (grown - 1u));
            while (entries[place].key != NULL)
            {
                place = (place + 1u) & (grown - 1u);
            }
            entries[place] = table->entries[at];
        }
        free(table->entries);
        table->entries = entries;
        table->bucket_count = grown;
    }
    if (table->bucket_count == 0u)
    {
        return NULL;
    }
    char text[8300];
    key_text(key, text, sizeof(text));
    size_t place = (size_t)(key_hash(text) & (table->bucket_count - 1u));
    while (table->entries[place].key != NULL)
    {
        if (strcmp(table->entries[place].key, text) == 0)
        {
            return &table->entries[place];
        }
        place = (place + 1u) & (table->bucket_count - 1u);
    }
    if (!create)
    {
        return NULL;
    }
    table->entries[place].key = (char *)siegel_memory(malloc(strlen(text) + 1u));
    memcpy(table->entries[place].key, text, strlen(text) + 1u);
    table->count += 1u;
    return &table->entries[place];
}

/**
 * @brief Re e^(i theta) zeta(1/2 + it) at each key of one pass, each at its digits, with C from the device.
 *
 * @param[in,out] tail  The device [BORROWS].
 * @param[in]     keys  The keys, all at one count of digits [BORROWS].
 * @param[in]     count How many.
 * @param[in,out] em    The table of values [BORROWS].
 * @return              1 where the records of the device equaled those of the host at every sweep.
 * @note The routes at N and 2N are each a head of powers on the host and C N^-s. The device sweeps C for each open
 *       point at once. A point whose two routes read alike with the guard dropped ends its work, and the rest double
 *       N.
 */
static int em_on_device(DeviceTail *tail, const CellKey *keys, size_t count, KeptTable *em)
{
    const int32_t digits = keys[0].digits;
    const int32_t work = digits + ZETA_GUARD;
    OriorExactInteger scale;
    z_ten_to(work, &scale);
    DecimalPair *const ts = (DecimalPair *)siegel_memory(malloc((count + 1u) * sizeof(DecimalPair)));
    OriorExactInteger(*const turns)[2] =
        (OriorExactInteger(*)[2])siegel_memory(malloc((count + 1u) * sizeof(OriorExactInteger[2])));
    for (size_t at = 0u; at < count; at += 1u)
    {
        rs_time_at(keys[at].nu, &keys[at].p, work, &ts[at]);
        OriorExactInteger theta;
        (void)rs_theta_at(&ts[at], work, &theta);
        zeta_cos_sin(&theta, work, &turns[at][0], &turns[at][1]);
    }
    size_t *open = (size_t *)siegel_memory(malloc((count + 1u) * sizeof(size_t)));
    for (size_t at = 0u; at < count; at += 1u)
    {
        open[at] = at;
    }
    size_t open_count = count;
    int64_t n_sum = 1;
    int same = 1;
    DecimalPair *const asked = (DecimalPair *)siegel_memory(malloc((count + 1u) * sizeof(DecimalPair)));
    OriorExactInteger(*const routes[2])[2] = {
        (OriorExactInteger(*)[2])siegel_memory(malloc((count + 1u) * sizeof(OriorExactInteger[2]))),
        (OriorExactInteger(*)[2])siegel_memory(malloc((count + 1u) * sizeof(OriorExactInteger[2])))};
    while (open_count > 0u)
    {
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            asked[at] = ts[open[at]];
        }
        for (int which = 0; which < 2; which += 1)
        {
            same = tail_coefficients(tail, asked, open_count, n_sum << which, work, routes[which]) && same;
        }
        size_t next = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            const size_t i = open[at];
            OriorExactInteger reads[2][2];
            for (int which = 0; which < 2; which += 1)
            {
                const int64_t n = n_sum << which;
                ZetaPoint point;
                pair_small(5, 1, &point.sigma);
                point.t = ts[i];
                OriorExactInteger re;
                OriorExactInteger im;
                orior_exact_zero(&re);
                orior_exact_zero(&im);
                OriorExactInteger last[4];
                for (int64_t m = 1; m <= n; m += 1)
                {
                    zeta_power_minus_s(m, &point, work, last);
                    if (m < n)
                    {
                        z_add(&re, &last[0], &re);
                        z_add(&im, &last[1], &im);
                    }
                }
                const OriorExactInteger *const c = routes[which][at];
                OriorExactInteger first;
                OriorExactInteger second;
                z_mul(&last[0], &c[0], &first);
                z_mul(&last[1], &c[1], &second);
                z_sub(&first, &second, &first);
                z_floor_div(&first, &scale, &first);
                z_add(&re, &first, &reads[which][0]);
                z_mul(&last[0], &c[1], &first);
                z_mul(&last[1], &c[0], &second);
                z_add(&first, &second, &first);
                z_floor_div(&first, &scale, &first);
                z_add(&im, &first, &reads[which][1]);
            }
            OriorExactInteger checks[4];
            rs_toward_zero(&reads[0][0], &checks[0]);
            rs_toward_zero(&reads[1][0], &checks[1]);
            rs_toward_zero(&reads[0][1], &checks[2]);
            rs_toward_zero(&reads[1][1], &checks[3]);
            if (orior_exact_equal(&checks[0], &checks[1]) && orior_exact_equal(&checks[2], &checks[3]))
            {
                OriorExactInteger first;
                OriorExactInteger second;
                z_mul(&turns[i][0], &reads[1][0], &first);
                z_mul(&turns[i][1], &reads[1][1], &second);
                z_sub(&first, &second, &first);
                z_floor_div(&first, &scale, &first);
                KeptPair *const entry = kept_at(em, &keys[i], 1);
                rs_toward_zero(&first, &entry->first);
                continue;
            }
            open[next] = i;
            next += 1u;
        }
        open_count = next;
        n_sum *= 2;
    }
    free(routes[0]);
    free(routes[1]);
    free(asked);
    free(open);
    free(turns);
    free(ts);
    return same;
}

/**
 * @brief ln(v / scale) for v >= scale, at the scale, by two routes: by 2 and by 3.
 *
 * @param[in]  v      The value [BORROWS].
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The route by 3 [BORROWS].
 * @note One route is k ln 2 + 2 artanh((v - 2^k) / (v + 2^k)). The other is j ln 3 + 2 artanh((v - 3^j) / (v + 3^j)).
 */
static void ln_at(const OriorExactInteger *v, const OriorExactInteger *scale, OriorExactInteger *result)
{
    OriorExactInteger power;
    z_set(&power, 1);
    int64_t j = 0;
    for (;;)
    {
        OriorExactInteger next;
        z_mul_small(&power, 3, &next);
        OriorExactInteger reach;
        z_mul(&next, scale, &reach);
        if (z_cmp(&reach, v) > 0)
        {
            break;
        }
        power = next;
        j += 1;
    }
    OriorExactInteger one;
    OriorExactInteger two;
    z_set(&one, 1);
    z_set(&two, 2);
    OriorExactInteger third;
    zeta_artanh(&one, &two, scale, &third);
    z_mul_small(&third, 2 * j, &third);
    OriorExactInteger base;
    z_mul(&power, scale, &base);
    OriorExactInteger top;
    OriorExactInteger bottom;
    z_sub(v, &base, &top);
    z_add(v, &base, &bottom);
    OriorExactInteger rest;
    zeta_artanh(&top, &bottom, scale, &rest);
    z_mul_small(&rest, 2, &rest);
    z_add(&third, &rest, result);
}

/**
 * @brief g(u) = D0^2 / (sqrt(D0^2 + k^2 u^2) + k |u|) at the scale.
 *
 * @param[in]  d0     D at the crossing [BORROWS].
 * @param[in]  k      |E'| at the crossing [BORROWS].
 * @param[in]  u      The distance from the crossing [BORROWS].
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The value [BORROWS].
 */
static void spike_at(const OriorExactInteger *d0, const OriorExactInteger *k, const OriorExactInteger *u,
                     const OriorExactInteger *scale, OriorExactInteger *result)
{
    OriorExactInteger size;
    z_abs(u, &size);
    OriorExactInteger ku;
    times_over(k, &size, scale, &ku);
    OriorExactInteger square;
    OriorExactInteger part;
    z_mul(d0, d0, &square);
    z_mul(&ku, &ku, &part);
    OriorExactInteger under;
    z_add(&square, &part, &under);
    z_isqrt(&under, &under);
    z_add(&under, &ku, &under);
    if ((d0->sign == 0) && (ku.sign == 0))
    {
        z_set(&part, 1);
        z_add(&under, &part, &under);
    }
    z_floor_div(&square, &under, result);
}

/**
 * @brief The integral of g over u from 0 to L, exactly, written so that nothing cancels.
 *
 * @param[in]  d0     D at the crossing [BORROWS].
 * @param[in]  k      |E'| at the crossing [BORROWS].
 * @param[in]  ell    L [BORROWS].
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The value [BORROWS].
 * @note The integral is (L D0^2 / (sqrt(D0^2 + k^2 L^2) + kL) + (D0^2 / k) asinh(kL / |D0|)) / 2, and
 *       asinh(y) = ln(y + sqrt(y^2 + 1)).
 */
static void spike_integral(const OriorExactInteger *d0, const OriorExactInteger *k, const OriorExactInteger *ell,
                           const OriorExactInteger *scale, OriorExactInteger *result)
{
    OriorExactInteger kl;
    times_over(k, ell, scale, &kl);
    OriorExactInteger square;
    OriorExactInteger part;
    z_mul(d0, d0, &square);
    z_mul(&kl, &kl, &part);
    OriorExactInteger under;
    z_add(&square, &part, &under);
    z_isqrt(&under, &under);
    z_add(&under, &kl, &under);
    if ((d0->sign == 0) && (kl.sign == 0))
    {
        z_set(&part, 1);
        z_add(&under, &part, &under);
    }
    z_mul(&under, scale, &under);
    OriorExactInteger first;
    z_mul(ell, &square, &first);
    z_floor_div(&first, &under, &first);
    OriorExactInteger size;
    z_abs(d0, &size);
    if (size.sign == 0)
    {
        z_set(&size, 1);
    }
    OriorExactInteger y;
    times_over(&kl, scale, &size, &y);
    OriorExactInteger root;
    z_mul(&y, &y, &root);
    z_mul(scale, scale, &part);
    z_add(&root, &part, &root);
    z_isqrt(&root, &root);
    z_add(&y, &root, &y);
    OriorExactInteger log;
    ln_at(&y, scale, &log);
    OriorExactInteger second;
    z_mul(&square, &log, &second);
    OriorExactInteger divisor = *k;
    if (divisor.sign == 0)
    {
        z_set(&divisor, 1);
    }
    z_mul(&divisor, scale, &divisor);
    z_floor_div(&second, &divisor, &second);
    z_add(&first, &second, &first);
    OriorExactInteger two;
    z_set(&two, 2);
    z_floor_div(&first, &two, result);
}

/** @brief One crossing of S and R in a cell. */
typedef struct
{
    OriorExactInteger low;   /**< The place over 2^SPIKE_BITS. */
    OriorExactInteger d0;    /**< D there. */
    OriorExactInteger slope; /**< |E'| there. */
} Crossing;

/** @brief The three sides over a cell, from values each asked once. */
typedef struct
{
    DeviceTail *tail;       /**< The device [BORROWS]. */
    int64_t cut;            /**< The cut, or -1 for the c_5 of MathWorld. */
    KeptTable *em;          /**< Euler-Maclaurin, shared by the cuts [BORROWS]. */
    KeptTable rs;           /**< S and R. */
    int same;               /**< Whether the device equaled the host at every sweep. */
    int64_t crossed_nu;     /**< The cell of the crossings kept. */
    int32_t crossed_digits; /**< Their digits. */
    Crossing *crossings;    /**< The crossings [OWNS]. */
    size_t crossing_count;  /**< How many. */
} Triangle;

/**
 * @brief S and R at a key, asked once and kept.
 *
 * @param[in,out] triangle The triangle [BORROWS].
 * @param[in]     key      The key [BORROWS].
 * @return                 The entry [BORROWS].
 */
static const KeptPair *triangle_rs(Triangle *triangle, const CellKey *key)
{
    KeptPair *const entry = kept_at(&triangle->rs, key, 1);
    if ((entry->first.sign == 0) && (entry->second.sign == 0))
    {
        if (triangle->cut < 0)
        {
            rs_at(key->nu, &key->p, key->digits, &entry->first, &entry->second);
        }
        else
        {
            rs_cut_at(key->nu, &key->p, key->digits, triangle->cut, &entry->first, &entry->second);
        }
    }
    return entry;
}

/**
 * @brief Fills Euler-Maclaurin at the keys it lacks, from the device, and S and R at each key.
 *
 * @param[in,out] triangle The triangle [BORROWS].
 * @param[in]     keys     The keys [BORROWS].
 * @param[in]     count    How many.
 */
static void triangle_fill(Triangle *triangle, const CellKey *keys, size_t count)
{
    CellKey *const fresh = (CellKey *)siegel_memory(malloc((count + 1u) * sizeof(CellKey)));
    size_t fresh_count = 0u;
    KeptTable seen;
    memset(&seen, 0, sizeof(seen));
    for (size_t at = 0u; at < count; at += 1u)
    {
        if (kept_at(&seen, &keys[at], 0) != NULL)
        {
            continue;
        }
        (void)kept_at(&seen, &keys[at], 1);
        if (kept_at(triangle->em, &keys[at], 0) == NULL)
        {
            fresh[fresh_count] = keys[at];
            fresh_count += 1u;
        }
    }
    for (size_t at = 0u; at < seen.bucket_count; at += 1u)
    {
        free(seen.entries[at].key);
    }
    free(seen.entries);
    if (fresh_count > 0u)
    {
        triangle->same = em_on_device(triangle->tail, fresh, fresh_count, triangle->em) && triangle->same;
    }
    for (size_t at = 0u; at < count; at += 1u)
    {
        (void)triangle_rs(triangle, &keys[at]);
    }
    free(fresh);
}

/**
 * @brief The key of the point x = nu + j / 2^bits, at the digits.
 *
 * @param[in]  nu     The cell.
 * @param[in]  j      The numerator [BORROWS].
 * @param[in]  bits   The bits of the denominator.
 * @param[in]  digits The places.
 * @param[out] key    The key [BORROWS].
 */
static void dyadic_key(int64_t nu, const OriorExactInteger *j, int64_t bits, int32_t digits, CellKey *key)
{
    OriorExactInteger value = *j;
    for (int64_t at = 0; at < bits; at += 1)
    {
        z_mul_small(&value, 5, &value);
    }
    key->nu = nu;
    // The bits of a place are small, and fit the places.
    pair_make(&value, (int32_t)bits, &key->p);
    key->digits = digits;
}

/**
 * @brief E = S - R at p = j / 2^bits, from Riemann-Siegel alone.
 *
 * @param[in,out] triangle The triangle [BORROWS].
 * @param[in]     nu       The cell.
 * @param[in]     j        The numerator [BORROWS].
 * @param[in]     bits     The bits of the denominator.
 * @param[in]     digits   The places.
 * @param[out]    result   E [BORROWS].
 */
static void triangle_e(Triangle *triangle, int64_t nu, const OriorExactInteger *j, int64_t bits, int32_t digits,
                       OriorExactInteger *result)
{
    CellKey key;
    dyadic_key(nu, j, bits, digits, &key);
    const KeptPair *const entry = triangle_rs(triangle, &key);
    z_sub(&entry->first, &entry->second, result);
}

/**
 * @brief The signs of E at j / 2^m for j = 0 to 2^m.
 *
 * @param[in,out] triangle The triangle [BORROWS].
 * @param[in]     nu       The cell.
 * @param[in]     m        The bits.
 * @param[in]     digits   The places.
 * @param[out]    row      The signs [BORROWS].
 */
static void triangle_row(Triangle *triangle, int64_t nu, int64_t m, int32_t digits, int *row)
{
    for (int64_t j = 0; j <= ((int64_t)1 << m); j += 1)
    {
        OriorExactInteger numerator;
        z_set(&numerator, j);
        OriorExactInteger e;
        triangle_e(triangle, nu, &numerator, m, digits, &e);
        row[j] = e.sign > 0;
    }
}

/**
 * @brief Where S and R cross in the cell, each as its place over 2^SPIKE_BITS, D there, and |E'| there.
 *
 * @param[in,out] triangle The triangle [BORROWS].
 * @param[in]     nu       The cell.
 * @param[in]     digits   The places.
 * @note The code reads the sign changes of E on 2^m and 2^(m+1) parts. The count m grows from 8 until the two counts
 *       agree, and the code halves each to 2^-SPIKE_BITS. It reads |E'| from the last bracket, and D at the crossing
 *       from both pairs.
 */
static void triangle_crossings(Triangle *triangle, int64_t nu, int32_t digits)
{
    if ((triangle->crossings != NULL) && (triangle->crossed_nu == nu) && (triangle->crossed_digits == digits))
    {
        return;
    }
    int64_t m = 8;
    int *row = (int *)siegel_memory(malloc((((size_t)1 << m) + 1u) * sizeof(int)));
    int *finer = (int *)siegel_memory(malloc((((size_t)2 << m) + 1u) * sizeof(int)));
    triangle_row(triangle, nu, m, digits, row);
    triangle_row(triangle, nu, m + 1, digits, finer);
    while (sign_changes(row, ((size_t)1 << m) + 1u, NULL) != sign_changes(finer, ((size_t)2 << m) + 1u, NULL))
    {
        m += 1;
        free(row);
        row = finer;
        finer = (int *)siegel_memory(malloc((((size_t)2 << m) + 1u) * sizeof(int)));
        triangle_row(triangle, nu, m + 1, digits, finer);
    }
    const size_t length = ((size_t)2 << m) + 1u;
    int64_t *const changes = (int64_t *)siegel_memory(malloc(length * sizeof(int64_t)));
    const size_t count = sign_changes(finer, length, changes);
    free(triangle->crossings);
    triangle->crossings = (Crossing *)siegel_memory(calloc(count + 1u, sizeof(Crossing)));
    CellKey *const keys = (CellKey *)siegel_memory(malloc((count + 1u) * sizeof(CellKey)));
    for (size_t at = 0u; at < count; at += 1u)
    {
        OriorExactInteger low;
        z_set(&low, changes[at]);
        int64_t depth = m + 1;
        const int left = finer[changes[at]];
        OriorExactInteger one;
        z_set(&one, 1);
        while (depth < SPIKE_BITS)
        {
            z_mul_small(&low, 2, &low);
            depth += 1;
            OriorExactInteger probe;
            z_add(&low, &one, &probe);
            OriorExactInteger e;
            triangle_e(triangle, nu, &probe, depth, digits, &e);
            if ((e.sign > 0) == left)
            {
                low = probe;
            }
        }
        OriorExactInteger probe;
        z_add(&low, &one, &probe);
        OriorExactInteger e_high;
        OriorExactInteger e_low;
        triangle_e(triangle, nu, &probe, SPIKE_BITS, digits, &e_high);
        triangle_e(triangle, nu, &low, SPIKE_BITS, digits, &e_low);
        z_sub(&e_high, &e_low, &e_high);
        z_abs(&e_high, &e_high);
        OriorExactInteger power;
        z_two_to(SPIKE_BITS, &power);
        z_mul(&e_high, &power, &triangle->crossings[at].slope);
        triangle->crossings[at].low = low;
        dyadic_key(nu, &low, SPIKE_BITS, digits, &keys[at]);
    }
    triangle_fill(triangle, keys, count);
    for (size_t at = 0u; at < count; at += 1u)
    {
        const KeptPair *const pair = triangle_rs(triangle, &keys[at]);
        const KeptPair *const em = kept_at(triangle->em, &keys[at], 0);
        z_add(&pair->first, &pair->second, &triangle->crossings[at].d0);
        z_sub(&triangle->crossings[at].d0, &em->first, &triangle->crossings[at].d0);
    }
    triangle->crossing_count = count;
    triangle->crossed_nu = nu;
    triangle->crossed_digits = digits;
    free(keys);
    free(changes);
    free(row);
    free(finer);
}

/**
 * @brief a, b and delta over p in [0, 1], by the trapezoid on 2^m equal parts, at the digits.
 *
 * @param[in,out] triangle The triangle [BORROWS].
 * @param[in]     nu       The cell.
 * @param[in]     digits   The places.
 * @param[in]     m        The bits of the parts.
 * @param[out]    sides    a, b and delta [BORROWS].
 * @note D = Z_RS - Z_EM and E = S - R at each point. a = int |D| and b = int |E|. The third side is
 *       c = int |(D, E)| = b + delta, with delta = int D^2 / (|(D, E)| + |E|). c - b never cancels.
 * @note Where S and R cross, E is zero. The integrand of delta is then a spike of height |D| and width |D / E'|, far
 *       below any grid. The code holds each spike by its relation, and its integral over the cell is exact. The side
 *       delta is the sum of those integrals and the trapezoid of the rest, f less each g.
 * @note Where D or E changes sign between two points x and y, |D| or |E| has a corner that the trapezoid cuts. Each
 *       such part gives back |x y| / (|x| + |y|). a and b settle as the grid halves.
 */
static void triangle_sides(Triangle *triangle, int64_t nu, int32_t digits, int64_t m, OriorExactInteger sides[3])
{
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    triangle_crossings(triangle, nu, digits);
    const int64_t steps = (int64_t)1 << m;
    CellKey *const keys = (CellKey *)siegel_memory(malloc(((size_t)steps + 1u) * sizeof(CellKey)));
    for (int64_t j = 0; j <= steps; j += 1)
    {
        OriorExactInteger numerator;
        z_set(&numerator, j);
        dyadic_key(nu, &numerator, m, digits, &keys[j]);
    }
    triangle_fill(triangle, keys, (size_t)steps + 1u);
    OriorExactInteger a;
    OriorExactInteger b;
    OriorExactInteger delta;
    orior_exact_zero(&a);
    orior_exact_zero(&b);
    orior_exact_zero(&delta);
    OriorExactInteger *const ds = (OriorExactInteger *)siegel_memory(malloc(((size_t)steps + 1u) * sizeof(*ds)));
    OriorExactInteger *const es = (OriorExactInteger *)siegel_memory(malloc(((size_t)steps + 1u) * sizeof(*es)));
    OriorExactInteger spike_unit;
    z_two_to(SPIKE_BITS, &spike_unit);
    for (int64_t j = 0; j <= steps; j += 1)
    {
        const KeptPair *const pair = triangle_rs(triangle, &keys[j]);
        const KeptPair *const em = kept_at(triangle->em, &keys[j], 0);
        z_add(&pair->first, &pair->second, &ds[j]);
        z_sub(&ds[j], &em->first, &ds[j]);
        z_sub(&pair->first, &pair->second, &es[j]);
        OriorExactInteger square;
        OriorExactInteger part;
        z_mul(&ds[j], &ds[j], &square);
        z_mul(&es[j], &es[j], &part);
        z_add(&square, &part, &part);
        OriorExactInteger root;
        z_isqrt(&part, &root);
        const int64_t weight = ((j == 0) || (j == steps)) ? 1 : 2;
        OriorExactInteger size;
        z_abs(&ds[j], &size);
        z_mul_small(&size, weight, &size);
        z_add(&a, &size, &a);
        z_abs(&es[j], &size);
        OriorExactInteger e_size = size;
        z_mul_small(&size, weight, &size);
        z_add(&b, &size, &b);
        OriorExactInteger spike_sum;
        orior_exact_zero(&spike_sum);
        for (size_t at = 0u; at < triangle->crossing_count; at += 1u)
        {
            const Crossing *const crossing = &triangle->crossings[at];
            OriorExactInteger u;
            z_two_to(SPIKE_BITS - m, &u);
            z_mul_small(&u, j, &u);
            z_sub(&u, &crossing->low, &u);
            z_mul(&u, &scale, &u);
            floor_bits(&u, SPIKE_BITS, &u);
            OriorExactInteger value;
            spike_at(&crossing->d0, &crossing->slope, &u, &scale, &value);
            z_add(&spike_sum, &value, &spike_sum);
        }
        OriorExactInteger under;
        z_add(&root, &e_size, &under);
        if (under.sign == 0)
        {
            z_set(&under, 1);
        }
        OriorExactInteger value;
        z_floor_div(&square, &under, &value);
        z_sub(&value, &spike_sum, &value);
        z_mul_small(&value, weight, &value);
        z_add(&delta, &value, &delta);
    }
    for (int64_t j = 0; j < steps; j += 1)
    {
        OriorExactInteger fixed;
        corner(&ds[j], &ds[j + 1], &fixed);
        z_sub(&a, &fixed, &a);
        corner(&es[j], &es[j + 1], &fixed);
        z_sub(&b, &fixed, &b);
    }
    OriorExactInteger exact;
    orior_exact_zero(&exact);
    for (size_t at = 0u; at < triangle->crossing_count; at += 1u)
    {
        const Crossing *const crossing = &triangle->crossings[at];
        OriorExactInteger ell;
        z_mul(&crossing->low, &scale, &ell);
        floor_bits(&ell, SPIKE_BITS, &ell);
        OriorExactInteger value;
        spike_integral(&crossing->d0, &crossing->slope, &ell, &scale, &value);
        z_add(&exact, &value, &exact);
        z_sub(&spike_unit, &crossing->low, &ell);
        z_mul(&ell, &scale, &ell);
        floor_bits(&ell, SPIKE_BITS, &ell);
        spike_integral(&crossing->d0, &crossing->slope, &ell, &scale, &value);
        z_add(&exact, &value, &exact);
    }
    OriorExactInteger under;
    z_set(&under, 2 * steps);
    z_floor_div(&a, &under, &sides[0]);
    z_floor_div(&b, &under, &sides[1]);
    z_floor_div(&delta, &under, &sides[2]);
    z_add(&sides[2], &exact, &sides[2]);
    free(ds);
    free(es);
    free(keys);
}

/**
 * @brief kappa = (2b delta + delta^2 - a^2) / a^2, read toward zero at the places.
 *
 * @param[in]  sides  a, b and delta [BORROWS].
 * @param[in]  places The places.
 * @param[out] result kappa [BORROWS].
 * @note cos gamma = -kappa a / 2b. The excess kappa is the shape with the scale a / b taken out. It is zero where |D|
 *       follows |E| across the cell, and positive otherwise.
 */
static void triangle_excess(const OriorExactInteger sides[3], int32_t places, OriorExactInteger *result)
{
    OriorExactInteger top;
    OriorExactInteger part;
    z_mul(&sides[1], &sides[2], &top);
    z_mul_small(&top, 2, &top);
    z_mul(&sides[2], &sides[2], &part);
    z_add(&top, &part, &top);
    z_mul(&sides[0], &sides[0], &part);
    z_sub(&top, &part, &top);
    zeta_require(orior_exact_scale_by_ten(&top, (uint32_t)(places + ZETA_GUARD)), "scale");
    if (part.sign == 0)
    {
        z_set(&part, 1);
    }
    z_floor_div(&top, &part, &top);
    rs_toward_zero(&top, result);
}

/**
 * @brief cos gamma = (a^2 - 2b delta - delta^2) / 2ab, read toward zero at the places where it first reads nonzero.
 *
 * @param[in]  sides a, b and delta [BORROWS].
 * @param[out] read  The reading [BORROWS].
 * @return           Its places. A reading of zero doubles them.
 */
static int32_t triangle_angle(const OriorExactInteger sides[3], OriorExactInteger *read)
{
    OriorExactInteger numerator;
    OriorExactInteger part;
    z_mul(&sides[0], &sides[0], &numerator);
    z_mul(&sides[1], &sides[2], &part);
    z_mul_small(&part, 2, &part);
    z_sub(&numerator, &part, &numerator);
    z_mul(&sides[2], &sides[2], &part);
    z_sub(&numerator, &part, &numerator);
    OriorExactInteger denominator;
    z_mul(&sides[0], &sides[1], &denominator);
    if (denominator.sign == 0)
    {
        z_set(&denominator, 1);
    }
    else
    {
        z_mul_small(&denominator, 2, &denominator);
    }
    int32_t places = 1;
    for (;;)
    {
        OriorExactInteger value = numerator;
        zeta_require(orior_exact_scale_by_ten(&value, (uint32_t)(places + ZETA_GUARD)), "scale");
        z_floor_div(&value, &denominator, &value);
        rs_toward_zero(&value, read);
        if (read->sign != 0)
        {
            return places;
        }
        places *= 2;
    }
}

/**
 * @brief The triangle over [nu, nu + 1].
 *
 * @param[in,out] triangle The triangle [BORROWS].
 * @param[in]     nu       The cell.
 * @param[out]    sides    a, b and delta [BORROWS].
 * @param[out]    kappa    kappa [BORROWS].
 * @param[out]    places   The places of kappa [BORROWS].
 * @param[out]    digits   The digits of the sides [BORROWS].
 * @return                 The bits of the parts, m + 1.
 * @note The verdict holds where a and delta each carry places + ZETA_GUARD digits, and kappa reads nonzero at the
 *       places. The routes at 2^m and 2^(m+1) parts must also agree on it. Each verdict that fails doubles its own
 *       count: the depth, the places, or the parts.
 */
static int64_t triangle_cell(Triangle *triangle, int64_t nu, OriorExactInteger sides[3], OriorExactInteger *kappa,
                             int32_t *places, int32_t *digits)
{
    *places = 1;
    int32_t depth = ZETA_GUARD;
    int64_t m = 1;
    for (;;)
    {
        *digits = *places + ZETA_GUARD + depth;
        OriorExactInteger one[3];
        triangle_sides(triangle, nu, *digits, m, one);
        triangle_sides(triangle, nu, *digits, m + 1, sides);
        OriorExactInteger floor_value;
        z_ten_to(*places + ZETA_GUARD, &floor_value);
        OriorExactInteger a_read;
        OriorExactInteger delta_read;
        z_floor_div(&sides[0], &floor_value, &a_read);
        z_floor_div(&sides[2], &floor_value, &delta_read);
        const int deep = (a_read.sign != 0) && (delta_read.sign != 0);
        OriorExactInteger kappa_one;
        triangle_excess(one, *places, &kappa_one);
        triangle_excess(sides, *places, kappa);
        const int agree = orior_exact_equal(&kappa_one, kappa);
        const int read = kappa->sign != 0;
        if (deep && agree && read)
        {
            return m + 1;
        }
        if (!deep)
        {
            depth *= 2;
        }
        else if (!agree)
        {
            m += 1;
        }
        else
        {
            *places *= 2;
        }
    }
}

/**
 * @brief The main of the triangle mode: the triangle over cells first to last, one line a cell for each cut.
 *
 * @param[in] first The first cell.
 * @param[in] last  The last cell.
 * @param[in] cuts  The cuts [BORROWS]. A cut of -1 is the c_0 to c_5 of MathWorld.
 * @param[in] count How many cuts.
 * @return          The exit code: 0 where the device equaled the host at every sweep.
 * @note Any cut other than -1 is R through C_cut from the exact curves. The cuts share Euler-Maclaurin.
 */
static int triangle_main(int64_t first, int64_t last, const int64_t *cuts, size_t count)
{
    FILE *const out = stdout;
    DeviceTail tail;
    memset(&tail, 0, sizeof(tail));
    tail_build(tail.binary);
    fprintf(out, "  the triangle between the two pairs, C on the device from %s\n", tail.binary);
    fflush(out);
    KeptTable em;
    memset(&em, 0, sizeof(em));
    int same = 1;
    for (size_t which = 0u; which < count; which += 1u)
    {
        Triangle triangle;
        memset(&triangle, 0, sizeof(triangle));
        triangle.tail = &tail;
        triangle.cut = (cuts[which] < 0) ? -1 : cuts[which];
        triangle.em = &em;
        triangle.same = 1;
        for (int64_t nu = first; nu <= last; nu += 1)
        {
            const int64_t begun = now_ms();
            OriorExactInteger sides[3];
            OriorExactInteger kappa;
            int32_t places = 0;
            int32_t digits = 0;
            const int64_t parts = triangle_cell(&triangle, nu, sides, &kappa, &places, &digits);
            OriorExactInteger read;
            const int32_t at = triangle_angle(sides, &read);
            char texts[6][512];
            signed_text(&sides[0], digits, texts[0], sizeof(texts[0]));
            signed_text(&sides[1], digits, texts[1], sizeof(texts[1]));
            signed_text(&sides[2], digits, texts[2], sizeof(texts[2]));
            signed_text(&kappa, places, texts[3], sizeof(texts[3]));
            OriorExactInteger ratio = sides[0];
            zeta_require(orior_exact_scale_by_ten(&ratio, 12u), "scale");
            z_floor_div(&ratio, &sides[1], &ratio);
            signed_text(&ratio, 12, texts[4], sizeof(texts[4]));
            signed_text(&read, at, texts[5], sizeof(texts[5]));
            const size_t crossings =
                ((triangle.crossed_nu == nu) && (triangle.crossed_digits == digits)) ? triangle.crossing_count : 0u;
            fprintf(out,
                    "  cut %lld  cell %lld  a %s  b %s  c - b %s  kappa %s  a/b %s  cos gamma %s  parts 2^%lld  points "
                    "%llu  crossings %llu  device equals host %s  %lld ms\n",
                    (long long)cuts[which], (long long)nu, texts[0], texts[1], texts[2], texts[3], texts[4], texts[5],
                    (long long)parts, (unsigned long long)em.count, (unsigned long long)crossings, truth(triangle.same),
                    (long long)(now_ms() - begun));
            fflush(out);
        }
        same = same && triangle.same;
    }
    return same ? 0 : 1;
}

/**
 * @brief An argument as a machine integer, or a default where it is absent.
 *
 * @param[in] argument_count How many arguments.
 * @param[in] arguments      The arguments [BORROWS].
 * @param[in] at             The place of the argument.
 * @param[in] fallback       The default.
 * @return                   The value.
 */
static int64_t argument_or(int argument_count, char **arguments, int at, int64_t fallback)
{
    return (at < argument_count) ? atoll(arguments[at]) : fallback;
}

int main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    const char *const mode = (argument_count > 1) ? arguments[1] : "";
    if (strcmp(mode, "omitted") == 0)
    {
        // The places are small, and fit an int.
        return omitted_main(argument_or(argument_count, arguments, 2, 1), argument_or(argument_count, arguments, 3, 1),
                            argument_or(argument_count, arguments, 4, 1), argument_or(argument_count, arguments, 5, 3),
                            (int32_t)argument_or(argument_count, arguments, 6, 24),
                            argument_or(argument_count, arguments, 7, 9));
    }
    if (strcmp(mode, "set") == 0)
    {
        // The places are small, and fit an int.
        return set_main(argument_or(argument_count, arguments, 2, 1), argument_or(argument_count, arguments, 3, 1),
                        argument_or(argument_count, arguments, 4, 1),
                        (int32_t)argument_or(argument_count, arguments, 5, 24),
                        argument_or(argument_count, arguments, 6, 4));
    }
    if (strcmp(mode, "triangle") == 0)
    {
        int64_t cuts[64];
        size_t count = 0u;
        for (int at = 4; (at < argument_count) && (count < 64u); at += 1)
        {
            cuts[count] = atoll(arguments[at]);
            count += 1u;
        }
        if (count == 0u)
        {
            cuts[0] = -1;
            count = 1u;
        }
        return triangle_main(argument_or(argument_count, arguments, 2, 1), argument_or(argument_count, arguments, 3, 1),
                             cuts, count);
    }
    if (strcmp(mode, "motion") == 0)
    {
        // The places are small, and fit an int.
        return motion_main(argument_or(argument_count, arguments, 2, 6), argument_or(argument_count, arguments, 3, 6),
                           argument_or(argument_count, arguments, 4, 9), argument_or(argument_count, arguments, 5, 16),
                           (int32_t)argument_or(argument_count, arguments, 6, 20));
    }
    if (strcmp(mode, "sphere") == 0)
    {
        // The places are small, and fit an int.
        return sphere_main(argument_or(argument_count, arguments, 2, 1), argument_or(argument_count, arguments, 3, 1),
                           argument_or(argument_count, arguments, 4, 10), argument_or(argument_count, arguments, 5, 8),
                           (int32_t)argument_or(argument_count, arguments, 6, 24));
    }
    return main_report((argument_count > 1) ? arguments[1] : "285", (argument_count > 2) ? atoi(arguments[2]) : 16);
}
