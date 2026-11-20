// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_riemann_siegel.h
 * @brief Z(t) by the formula of Riemann and Siegel at t = 2pi x^2, and every C_n of Gabcke, on exact integers.
 *
 * @note x = nu + p, with the cell nu and the delta p, a decimal pair between 0 and 1. The main sum S runs to n = nu,
 *       and the remainder R is the sum of the correction curves.
 * @note R comes by two routes. One is the c_0 to c_5 of MathWorld through Psi and its derivatives. The other is the
 *       exact C_n of Gabcke, through C_cut.
 * @note exact_zeta_riemann_siegel.c states the formula, the curves and their checks. exact_zeta_turing.c and
 *       exact_zeta_lobes.c read the curves and the routes from here.
 */
#ifndef ZETA_RIEMANN_SIEGEL_H
#define ZETA_RIEMANN_SIEGEL_H

#include "zeta_exact.h"

/** @brief One term of a c_k of MathWorld: numerator / denominator Psi^(j)(p) / pi^(2m). */
typedef struct
{
    int64_t numerator;   /**< The signed numerator. */
    int64_t denominator; /**< The denominator. */
    int derivative;      /**< The order j of the derivative. */
    int pi_half;         /**< The half power m of pi. */
} ZetaMathWorldTerm;

/** @brief MathWorld, Riemann-Siegel Formula, equations (8) to (13): each c_k as its terms. */
extern const ZetaMathWorldTerm ZETA_MATHWORLD_TERMS[6][4];

/** @brief How many terms each c_k of MathWorld has. */
extern const int ZETA_MATHWORLD_COUNTS[6];

/** @brief A list of exact integers that grows. */
typedef struct
{
    OriorExactInteger *values; /**< The values [OWNS]. */
    size_t count;              /**< How many. */
    size_t capacity;           /**< How many the array holds. */
} ZetaRow;

/**
 * @brief Adds a value at the end of a list.
 *
 * @param[in,out] row   The list [BORROWS].
 * @param[in]     value The value [BORROWS].
 */
void row_push(ZetaRow *row, const OriorExactInteger *value);

/**
 * @brief Frees a list.
 *
 * @param[in,out] row The list [BORROWS].
 */
void row_free(ZetaRow *row);

/**
 * @brief A value read toward zero with the guard dropped: sign(v) floor(|v| / 10^ZETA_GUARD).
 *
 * @param[in]  value  The value [BORROWS].
 * @param[out] result The reading [BORROWS].
 */
void rs_toward_zero(const OriorExactInteger *value, OriorExactInteger *result);

/**
 * @brief A value read at fewer places: sign(v) floor(|v| / 10^(digits - places)).
 *
 * @param[in]  value  The value [BORROWS].
 * @param[in]  digits The places of the value.
 * @param[in]  places The places of the reading.
 * @param[out] result The reading [BORROWS].
 */
void rs_read(const OriorExactInteger *value, int32_t digits, int32_t places, OriorExactInteger *result);

/**
 * @brief theta at the digits by the routes at N and 2N. N doubles until they agree with the guard dropped.
 *
 * @param[in]  t      The point, its numerator at the digits [BORROWS].
 * @param[in]  digits The places.
 * @param[out] theta  The second route [BORROWS].
 * @return            The N where they agree.
 */
int64_t rs_theta_at(const DecimalPair *t, int32_t digits, OriorExactInteger *theta);

/**
 * @brief t = 2pi x^2 at the digits, with x = nu + p: 2 X^2 floor(pi 10^digits) / 10^(2q).
 *
 * @param[in]  nu     The cell.
 * @param[in]  p      The delta [BORROWS].
 * @param[in]  digits The places.
 * @param[out] t      The point, its numerator at the digits [BORROWS].
 */
void rs_time_at(int64_t nu, const DecimalPair *p, int32_t digits, DecimalPair *t);

/**
 * @brief The main sum S = 2 Re e^(i theta) sum over n <= nu of n^(-1/2 - it), at the digits.
 *
 * @param[in]  nu     The cell.
 * @param[in]  t      The point [BORROWS].
 * @param[in]  theta  theta at t, at the digits [BORROWS].
 * @param[in]  digits The places.
 * @param[out] result The sum [BORROWS].
 */
void rs_main_sum(int64_t nu, const DecimalPair *t, const OriorExactInteger *theta, int32_t digits,
                 OriorExactInteger *result);

/**
 * @brief The main sum S, the terms of R each times d^16 pi^10, that multiplier, and the N of theta, at x = nu + p.
 *
 * @param[in]  nu         The cell.
 * @param[in]  p          The delta [BORROWS].
 * @param[in]  inv_root   x^(-1/2) at the digits [BORROWS].
 * @param[in]  digits     The places.
 * @param[out] main_sum   The main sum [BORROWS].
 * @param[out] tail       The terms c_0 u^-1 to c_5 u^-11 [BORROWS].
 * @param[out] multiplier d^16 pi^10 [BORROWS].
 * @return                The N of theta.
 * @note t = 2pi x^2, and u^-(2k+1) = inv_root x^-k. At p = 1 the own nu of the cell holds, the limit from inside it.
 */
int64_t rs_parts(int64_t nu, const DecimalPair *p, const OriorExactInteger *inv_root, int32_t digits,
                 OriorExactInteger *main_sum, OriorExactInteger tail[6], OriorExactInteger *multiplier);

/**
 * @brief S and R at x = nu + p, each at the digits, with R through the c_5 of MathWorld.
 *
 * @param[in]  nu        The cell.
 * @param[in]  p         The delta [BORROWS].
 * @param[in]  digits    The places.
 * @param[out] main_sum  S [BORROWS].
 * @param[out] remainder R [BORROWS].
 * @note R is the carried tail over d^16 pi^10. Near p = 1/4 and 3/4, d is small, and that multiplier carries fewer
 *       digits than the work. The work rises by the digits that the multiplier lacks, and the code asks again.
 */
void rs_at(int64_t nu, const DecimalPair *p, int32_t digits, OriorExactInteger *main_sum, OriorExactInteger *remainder);

/**
 * @brief Re e^(i theta) zeta(1/2 + it) at t = 2pi x^2, x = nu + p, by the two routes of Euler-Maclaurin.
 *
 * @param[in]  nu     The cell.
 * @param[in]  p      The delta [BORROWS].
 * @param[in]  digits The places.
 * @param[out] result The value at the digits [BORROWS].
 */
void rs_em_at(int64_t nu, const DecimalPair *p, int32_t digits, OriorExactInteger *result);

/* ---------------------------------------------------------------- every C_n of Gabcke */

/**
 * @brief |E_2k|, the Euler numbers by the sum over j of C(2n, 2j) E_2j = 0.
 *
 * @param[in]  k      The half index.
 * @param[out] result The value [BORROWS].
 */
void rs_euler_abs(int64_t k, OriorExactInteger *result);

/**
 * @brief lambda_l: (l + 1) lambda_(l+1) = sum over k of 2^(4k+1) |E_(2k+2)| lambda_(l-k), with lambda_0 = 1.
 *
 * @param[in]  l      The index.
 * @param[out] result The value [BORROWS].
 */
void rs_lambda(int64_t l, OriorExactInteger *result);

/**
 * @brief d_k^(n) for k = 0 .. floor(3n / 4), from the recurrence of Gabcke.
 *
 * @param[in] n The row.
 * @return      The row [BORROWS].
 * @note d_k^(n+1) = (3n + 1 - 4k)(3n + 2 - 4k) d_k^(n) + d_(k-1)^(n). Where 3(n + 1) = 4k, the entry is
 *       lambda_(k/3).
 */
const ZetaRow *rs_d_row(int64_t n);

/** @brief The Taylor coefficients of every C_n about z = 0, read at the digits. */
typedef struct
{
    int32_t digits;            /**< The places. */
    OriorExactInteger scale;   /**< 10^digits. */
    OriorExactInteger big_pi;  /**< Pi at the digits. */
    OriorExactInteger root[2]; /**< sin(pi/8) and cos(pi/8), sqrt(2 -+ sqrt 2) / 2. */
    ZetaRow up;                /**< The powers pi^e. */
    ZetaRow down;              /**< The powers pi^-e. */
    ZetaRow phi;               /**< The coefficients of F found so far. */
} ZetaCurve;

/**
 * @brief The curve at the digits, made once and kept.
 *
 * @param[in] digits The places.
 * @return           The curve [BORROWS].
 */
ZetaCurve *rs_curve(int32_t digits);

/**
 * @brief A new curve at the digits, for the caller alone. The caller frees it with rs_curve_free.
 *
 * @param[out] curve  The curve [BORROWS].
 * @param[in]  digits The places.
 */
void rs_curve_begin(ZetaCurve *curve, int32_t digits);

/**
 * @brief Frees a curve that rs_curve_begin made.
 *
 * @param[in,out] curve The curve [BORROWS].
 */
void rs_curve_free(ZetaCurve *curve);

/**
 * @brief The first count Taylor coefficients of C_n about z = 0.
 *
 * @param[in,out] curve The curve [BORROWS].
 * @param[in]     n     The index of the curve.
 * @param[in]     count How many coefficients.
 * @param[out]    out   The coefficients [BORROWS]. The caller frees them.
 * @note Let m = 3n - 4k. The coefficient at z^(i + m) is 2^(-2n) sum over k of d_k C(i + m, m) pi^(2k - 2n) phi.
 *       It is zero where i + m is odd.
 */
void rs_curve_gamma(ZetaCurve *curve, int64_t n, int64_t count, ZetaRow *out);

/**
 * @brief The first count Taylor coefficients of C_n at the digits, found once and kept.
 *
 * @param[in] n      The index of the curve.
 * @param[in] count  How many coefficients.
 * @param[in] digits The places.
 * @return           The coefficients [BORROWS].
 */
const ZetaRow *rs_gamma(int64_t n, int64_t count, int32_t digits);

/**
 * @brief C_n(a / 2^bits) at the digits by its first count Taylor terms, the rule of Horner on integers.
 *
 * @param[in]  n      The index of the curve.
 * @param[in]  count  How many terms.
 * @param[in]  digits The places.
 * @param[in]  a      The numerator [BORROWS].
 * @param[in]  bits   The bits of the denominator.
 * @param[out] result The value [BORROWS].
 */
void rs_c_value(int64_t n, int64_t count, int32_t digits, const OriorExactInteger *a, int64_t bits,
                OriorExactInteger *result);

/**
 * @brief C_n(num / den), |num / den| <= 1, at digits + ZETA_GUARD.
 *
 * @param[in]  n      The index of the curve.
 * @param[in]  num    The numerator [BORROWS].
 * @param[in]  den    The denominator, above zero [BORROWS].
 * @param[in]  digits The places.
 * @param[out] result The value [BORROWS].
 * @note The code takes count and 2 count Taylor terms, the second at twice the extra digits. They must agree with the
 *       guard dropped, and disagreement doubles count. An odd C_n turns sign with z, and the code reads it at |z|.
 */
void rs_c_at(int64_t n, const OriorExactInteger *num, const OriorExactInteger *den, int32_t digits,
             OriorExactInteger *result);

/**
 * @brief C_n(a / 2^bits), read at the places by count and 2 count terms, the second at twice the extra digits.
 *
 * @param[in]     n      The index of the curve.
 * @param[in]     a      The numerator [BORROWS].
 * @param[in]     bits   The bits of the denominator.
 * @param[in,out] places The places. A reading of zero doubles them [BORROWS].
 * @param[out]    result The reading [BORROWS].
 * @note The two must agree. Disagreement doubles count.
 */
void rs_c_verdict(int64_t n, const OriorExactInteger *a, int64_t bits, int32_t *places, OriorExactInteger *result);

/**
 * @brief R through C_cut at x = nu + p, at digits + ZETA_GUARD.
 *
 * @param[in]  nu     The cell.
 * @param[in]  p      The delta [BORROWS].
 * @param[in]  cut    The last curve.
 * @param[in]  digits The places.
 * @param[out] result The value [BORROWS].
 * @note R = (-1)^(nu-1) sum over n <= cut of C_n(1 - 2p) u^-(2n+1), with u^-(2n+1) = x^(-1/2) x^-n.
 */
void rs_remainder_at(int64_t nu, const DecimalPair *p, int64_t cut, int32_t digits, OriorExactInteger *result);

/**
 * @brief S and R at x = nu + p, each at the digits, with R through C_cut from the exact curves.
 *
 * @param[in]  nu        The cell.
 * @param[in]  p         The delta [BORROWS].
 * @param[in]  digits    The places.
 * @param[in]  cut       The last curve.
 * @param[out] main_sum  S [BORROWS].
 * @param[out] remainder R [BORROWS].
 */
void rs_cut_at(int64_t nu, const DecimalPair *p, int32_t digits, int64_t cut, OriorExactInteger *main_sum,
               OriorExactInteger *remainder);

#endif
