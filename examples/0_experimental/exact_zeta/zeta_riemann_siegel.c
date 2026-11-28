// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_riemann_siegel.c
 * @brief Z(t) by the formula of Riemann and Siegel, and every C_n of Gabcke, as zeta_riemann_siegel.h states them.
 */
#include "zeta_riemann_siegel.h"
#include "../../../includes/platform/platform.h"
#include "zeta_theta.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdlib.h>

/** @brief The degree of the series of Psi. */
#define DEGREE 15

/** @brief The power of pi that the terms of R carry. */
#define PI_POWER 10

/** @brief cos and sin of k pi/2, by k mod 4. */
static const int COS_SIGN[4] = {1, 0, -1, 0};
static const int SIN_SIGN[4] = {0, 1, 0, -1};

const ZetaMathWorldTerm ZETA_MATHWORLD_TERMS[6][4] = {
    {{1, 1, 0, 0}},
    {{-1, 96, 3, 1}},
    {{1, 64, 2, 1}, {1, 18432, 6, 2}},
    {{-1, 64, 1, 1}, {-1, 3840, 5, 2}, {-1, 5308416, 9, 3}},
    {{1, 128, 0, 1}, {19, 24576, 4, 2}, {11, 5898240, 8, 3}, {1, 2038431744, 12, 4}},
    {{-5, 3072, 3, 2}, {-901, 82575360, 7, 3}, {-7, 849346560, 11, 4}, {-1, 978447237120LL, 15, 5}},
};

const int ZETA_MATHWORLD_COUNTS[6] = {1, 1, 2, 3, 4, 4};

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *rs_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

void row_push(ZetaRow *row, const OriorExactInteger *value)
{
    if (row->count >= row->capacity)
    {
        row->capacity = (row->capacity == 0u) ? 16u : (row->capacity * 2u);
        row->values = (OriorExactInteger *)rs_memory(
            platform_cell_grown(row->values, row->count * sizeof(OriorExactInteger),
                                row->capacity * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    }
    row->values[row->count] = *value;
    row->count += 1u;
}

void row_free(ZetaRow *row)
{
    platform_cell_released(row->values, platform_place_g_program_owner);
    row->values = NULL;
    row->count = 0u;
    row->capacity = 0u;
}

/**
 * @brief An integer over 2^bits, rounded toward minus infinity, as >> of Python.
 *
 * @param[in]  value  The integer [BORROWS].
 * @param[in]  bits   The shift.
 * @param[out] result The quotient [BORROWS].
 */
static void floor_shift(const OriorExactInteger *value, int64_t bits, OriorExactInteger *result)
{
    OriorExactInteger power;
    z_two_to(bits, &power);
    z_floor_div(value, &power, result);
}

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
 * @brief C(row, column), exactly.
 *
 * @param[in]  row    The row.
 * @param[in]  column The column.
 * @param[out] result The value [BORROWS].
 */
static void binomial(int64_t row, int64_t column, OriorExactInteger *result)
{
    z_set(result, 1);
    const int64_t least = (column < (row - column)) ? column : (row - column);
    for (int64_t at = 0; at < least; at += 1)
    {
        z_mul_small(result, row - at, result);
        OriorExactInteger under;
        z_set(&under, at + 1);
        z_div_exact(result, &under, result);
    }
}

/**
 * @brief n! for a small n.
 *
 * @param[in]  n      The argument.
 * @param[out] result The value [BORROWS].
 */
static void factorial_of(int64_t n, OriorExactInteger *result)
{
    z_set(result, 1);
    for (int64_t at = 2; at <= n; at += 1)
    {
        z_mul_small(result, at, result);
    }
}

void rs_toward_zero(const OriorExactInteger *value, OriorExactInteger *result)
{
    rs_read(value, ZETA_GUARD, 0, result);
}

void rs_read(const OriorExactInteger *value, int32_t digits, int32_t places, OriorExactInteger *result)
{
    const int32_t sign = value->sign;
    OriorExactInteger size;
    z_abs(value, &size);
    OriorExactInteger drop;
    z_ten_to(digits - places, &drop);
    z_floor_div(&size, &drop, result);
    if (sign < 0)
    {
        result->sign = -result->sign;
    }
}

int64_t rs_theta_at(const DecimalPair *t, int32_t digits, OriorExactInteger *theta)
{
    int64_t n_sum = 1;
    OriorExactInteger one;
    theta_route(t, digits, 1, 1, &one);
    theta_route(t, digits, 2, 2, theta);
    for (;;)
    {
        OriorExactInteger first;
        OriorExactInteger second;
        rs_toward_zero(&one, &first);
        rs_toward_zero(theta, &second);
        if (orior_exact_equal(&first, &second))
        {
            return n_sum;
        }
        n_sum *= 2;
        theta_route(t, digits, n_sum, n_sum, &one);
        theta_route(t, digits, 2 * n_sum, 2 * n_sum, theta);
    }
}

void rs_time_at(int64_t nu, const DecimalPair *p, int32_t digits, DecimalPair *t)
{
    OriorExactInteger unit;
    z_ten_to(p->places, &unit);
    OriorExactInteger big_x;
    z_mul_small(&unit, nu, &big_x);
    z_add(&big_x, &p->numerator, &big_x);
    OriorExactInteger value;
    z_mul(&big_x, &big_x, &value);
    z_mul_small(&value, 2, &value);
    OriorExactInteger big_pi;
    zeta_pi(digits, &big_pi);
    z_mul(&value, &big_pi, &value);
    OriorExactInteger under;
    z_mul(&unit, &unit, &under);
    z_floor_div(&value, &under, &t->numerator);
    t->places = digits;
}

void rs_main_sum(int64_t nu, const DecimalPair *t, const OriorExactInteger *theta, int32_t digits,
                 OriorExactInteger *result)
{
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger cosine;
    OriorExactInteger sine;
    zeta_cos_sin(theta, digits, &cosine, &sine);
    ZetaPoint point;
    pair_small(5, 1, &point.sigma);
    point.t = *t;
    OriorExactInteger re;
    OriorExactInteger im;
    orior_exact_zero(&re);
    orior_exact_zero(&im);
    for (int64_t n = 1; n <= nu; n += 1)
    {
        OriorExactInteger wave[4];
        zeta_power_minus_s(n, &point, digits, wave);
        z_add(&re, &wave[0], &re);
        z_add(&im, &wave[1], &im);
    }
    OriorExactInteger first;
    OriorExactInteger second;
    z_mul(&cosine, &re, &first);
    z_mul(&sine, &im, &second);
    z_sub(&first, &second, &first);
    z_mul_small(&first, 2, &first);
    z_floor_div(&first, &scale, result);
}

/**
 * @brief The product of two power series in h, cut to the length of the first, each coefficient over the scale.
 *
 * @param[in]  left   First series [BORROWS].
 * @param[in]  right  Second series [BORROWS].
 * @param[in]  length The length of both.
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The product [BORROWS]. It must not be either factor.
 */
static void series_times(const OriorExactInteger *left, const OriorExactInteger *right, int length,
                         const OriorExactInteger *scale, OriorExactInteger *result)
{
    for (int at = 0; at < length; at += 1)
    {
        orior_exact_zero(&result[at]);
    }
    for (int i = 0; i < length; i += 1)
    {
        for (int j = 0; j < (length - i); j += 1)
        {
            OriorExactInteger product;
            z_mul(&left[i], &right[j], &product);
            z_add(&result[i + j], &product, &result[i + j]);
        }
    }
    for (int at = 0; at < length; at += 1)
    {
        z_floor_div(&result[at], scale, &result[at]);
    }
}

/**
 * @brief Psi^(j)(p) d^16 for j = 0 .. 15, and d^16, each at the digits.
 *
 * @param[in]  p       The delta [BORROWS].
 * @param[in]  digits  The places.
 * @param[out] carried The carried derivatives [BORROWS].
 * @param[out] d_power d^16 [BORROWS].
 * @note The numerator is cos(phi + alpha h + beta h^2), and the denominator is cos(2pi p + 2pi h). Each is a power
 *       series in h. With Psi^(j) / j! = r_j / d^(j+1), r_k = n_k d^k - sum over j < k of r_j d_(k-j) d^(k-1-j).
 *       That takes products alone. Where d reads zero, both series start one coefficient later.
 */
static void psi_carried(const DecimalPair *p, int32_t digits, OriorExactInteger carried[DEGREE + 1],
                        OriorExactInteger *d_power)
{
    enum
    {
        LENGTH = DEGREE + 2
    };
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger big_pi;
    zeta_pi(digits, &big_pi);
    const OriorExactInteger *const x = &p->numerator;
    OriorExactInteger unit;
    z_ten_to(p->places, &unit);
    OriorExactInteger two_pi;
    z_mul_small(&big_pi, 2, &two_pi);

    // phi = 2pi (16 x^2 - 16 x unit - unit^2) / (16 unit^2).
    OriorExactInteger first;
    OriorExactInteger second;
    z_mul(x, x, &first);
    z_mul_small(&first, 16, &first);
    z_mul(x, &unit, &second);
    z_mul_small(&second, 16, &second);
    z_sub(&first, &second, &first);
    z_mul(&unit, &unit, &second);
    z_sub(&first, &second, &first);
    z_mul(&two_pi, &first, &first);
    OriorExactInteger under;
    z_mul(&unit, &unit, &under);
    z_mul_small(&under, 16, &under);
    OriorExactInteger phi;
    z_floor_div(&first, &under, &phi);
    // alpha = 2pi (2x - unit) / unit.
    z_mul_small(x, 2, &first);
    z_sub(&first, &unit, &first);
    z_mul(&two_pi, &first, &first);
    OriorExactInteger alpha;
    z_floor_div(&first, &unit, &alpha);
    OriorExactInteger cos_phi;
    OriorExactInteger sin_phi;
    zeta_cos_sin(&phi, digits, &cos_phi, &sin_phi);
    z_mul(&two_pi, x, &first);
    z_floor_div(&first, &unit, &first);
    OriorExactInteger cos_d;
    OriorExactInteger sin_d;
    zeta_cos_sin(&first, digits, &cos_d, &sin_d);

    static OriorExactInteger delta[LENGTH];
    static OriorExactInteger power[LENGTH];
    static OriorExactInteger next[LENGTH];
    static OriorExactInteger cos_part[LENGTH];
    static OriorExactInteger sin_part[LENGTH];
    for (int at = 0; at < LENGTH; at += 1)
    {
        orior_exact_zero(&delta[at]);
        orior_exact_zero(&power[at]);
        orior_exact_zero(&sin_part[at]);
    }
    delta[1] = alpha;
    delta[2] = two_pi;
    power[0] = scale;
    for (int at = 0; at < LENGTH; at += 1)
    {
        cos_part[at] = power[at];
    }
    OriorExactInteger factorial;
    z_set(&factorial, 1);
    for (int m = 1; m < LENGTH; m += 1)
    {
        series_times(power, delta, LENGTH, &scale, next);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = power, .src = next, .bytes = sizeof(power));
        z_mul_small(&factorial, m, &factorial);
        for (int at = 0; at < LENGTH; at += 1)
        {
            OriorExactInteger term;
            z_mul_small(&power[at], COS_SIGN[m % 4], &term);
            z_floor_div(&term, &factorial, &term);
            z_add(&cos_part[at], &term, &cos_part[at]);
            z_mul_small(&power[at], SIN_SIGN[m % 4], &term);
            z_floor_div(&term, &factorial, &term);
            z_add(&sin_part[at], &term, &sin_part[at]);
        }
    }
    static OriorExactInteger numerator[LENGTH];
    static OriorExactInteger denominator[LENGTH];
    OriorExactInteger grown;
    grown = scale;
    for (int k = 0; k < LENGTH; k += 1)
    {
        z_mul(&cos_phi, &cos_part[k], &first);
        z_mul(&sin_phi, &sin_part[k], &second);
        z_sub(&first, &second, &first);
        z_floor_div(&first, &scale, &numerator[k]);
        if (k > 0)
        {
            z_mul(&grown, &two_pi, &first);
            z_mul_small(&scale, k, &second);
            z_floor_div(&first, &second, &grown);
        }
        z_mul_small(&cos_d, COS_SIGN[k % 4], &first);
        z_mul_small(&sin_d, SIN_SIGN[k % 4], &second);
        z_sub(&first, &second, &first);
        z_mul(&first, &grown, &first);
        z_floor_div(&first, &scale, &denominator[k]);
    }
    OriorExactInteger read;
    rs_toward_zero(&denominator[0], &read);
    const int gone = read.sign == 0;
    for (int k = 0; k <= DEGREE; k += 1)
    {
        numerator[k] = numerator[k + gone];
        denominator[k] = denominator[k + gone];
    }
    const OriorExactInteger d = denominator[0];
    static OriorExactInteger powers[DEGREE + 2];
    powers[0] = scale;
    for (int at = 0; at <= DEGREE; at += 1)
    {
        times_over(&powers[at], &d, &scale, &powers[at + 1]);
    }
    static OriorExactInteger r[DEGREE + 1];
    for (int k = 0; k <= DEGREE; k += 1)
    {
        times_over(&numerator[k], &powers[k], &scale, &r[k]);
        for (int j = 0; j < k; j += 1)
        {
            OriorExactInteger part;
            times_over(&r[j], &denominator[k - j], &scale, &part);
            times_over(&part, &powers[k - 1 - j], &scale, &part);
            z_sub(&r[k], &part, &r[k]);
        }
    }
    z_set(&factorial, 1);
    for (int j = 0; j <= DEGREE; j += 1)
    {
        z_mul_small(&factorial, (j > 1) ? j : 1, &factorial);
        z_mul(&factorial, &r[j], &first);
        times_over(&first, &powers[DEGREE - j], &scale, &carried[j]);
    }
    *d_power = powers[DEGREE + 1];
}

/**
 * @brief The sign (-1)^(nu-1).
 *
 * @param[in] nu The cell.
 * @return       1 or -1.
 */
static int cell_sign(int64_t nu)
{
    return ((((nu - 1) % 2) + 2) % 2 == 0) ? 1 : -1;
}

int64_t rs_parts(int64_t nu, const DecimalPair *p, const OriorExactInteger *inv_root, int32_t digits,
                 OriorExactInteger *main_sum, OriorExactInteger tail[6], OriorExactInteger *multiplier)
{
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger unit;
    z_ten_to(p->places, &unit);
    OriorExactInteger big_x;
    z_mul_small(&unit, nu, &big_x);
    z_add(&big_x, &p->numerator, &big_x);
    DecimalPair t;
    rs_time_at(nu, p, digits, &t);
    OriorExactInteger theta;
    const int64_t theta_n = rs_theta_at(&t, digits, &theta);
    rs_main_sum(nu, &t, &theta, digits, main_sum);
    OriorExactInteger carried[DEGREE + 1];
    OriorExactInteger d_power;
    psi_carried(p, digits, carried, &d_power);
    OriorExactInteger big_pi;
    zeta_pi(digits, &big_pi);
    OriorExactInteger pi_powers[PI_POWER + 1];
    pi_powers[0] = scale;
    for (int at = 0; at < PI_POWER; at += 1)
    {
        times_over(&pi_powers[at], &big_pi, &scale, &pi_powers[at + 1]);
    }
    times_over(&d_power, &pi_powers[PI_POWER], &scale, multiplier);
    const int sign = cell_sign(nu);
    OriorExactInteger unit_power;
    OriorExactInteger x_power;
    z_set(&unit_power, 1);
    z_set(&x_power, 1);
    for (int k = 0; k < 6; k += 1)
    {
        OriorExactInteger c;
        orior_exact_zero(&c);
        for (int at = 0; at < ZETA_MATHWORLD_COUNTS[k]; at += 1)
        {
            const ZetaMathWorldTerm *const term = &ZETA_MATHWORLD_TERMS[k][at];
            OriorExactInteger value;
            z_mul_small(&carried[term->derivative], term->numerator, &value);
            z_mul(&value, &pi_powers[PI_POWER - (2 * term->pi_half)], &value);
            OriorExactInteger under;
            z_mul_small(&scale, term->denominator, &under);
            z_floor_div(&value, &under, &value);
            z_add(&c, &value, &c);
        }
        OriorExactInteger value;
        z_mul_small(&c, sign, &value);
        z_mul(&value, inv_root, &value);
        z_mul(&value, &unit_power, &value);
        OriorExactInteger under;
        z_mul(&scale, &x_power, &under);
        z_floor_div(&value, &under, &tail[k]);
        z_mul(&unit_power, &unit, &unit_power);
        z_mul(&x_power, &big_x, &x_power);
    }
    return theta_n;
}

/**
 * @brief The count of decimal digits of an integer, as len(str()) of Python counts them, the sign included.
 *
 * @param[in] value The integer [BORROWS].
 * @return          The count.
 */
static int64_t text_length(const OriorExactInteger *value)
{
    static char text[16384];
    z_text(value, text, sizeof(text));
    return (int64_t)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
}

void rs_at(int64_t nu, const DecimalPair *p, int32_t digits, OriorExactInteger *main_sum, OriorExactInteger *remainder)
{
    OriorExactInteger unit;
    z_ten_to(p->places, &unit);
    OriorExactInteger big_x;
    z_mul_small(&unit, nu, &big_x);
    z_add(&big_x, &p->numerator, &big_x);
    int32_t wanted = digits + ZETA_GUARD;
    int32_t work = wanted;
    OriorExactInteger scale;
    OriorExactInteger sum;
    OriorExactInteger tail[6];
    OriorExactInteger multiplier;
    int short_of = 1;
    while (short_of)
    {
        work = wanted;
        z_ten_to(work, &scale);
        OriorExactInteger square;
        z_mul(&scale, &scale, &square);
        z_mul(&square, &unit, &square);
        z_floor_div(&square, &big_x, &square);
        OriorExactInteger inv_root;
        z_isqrt(&square, &inv_root);
        (void)rs_parts(nu, p, &inv_root, work, &sum, tail, &multiplier);
        const int64_t lost = text_length(&scale) - text_length(&multiplier);
        // The loss is a count of digits, and fits the width of the places.
        wanted = digits + ZETA_GUARD + (int32_t)((lost > 0) ? lost : 0);
        short_of = work < wanted;
    }
    OriorExactInteger total;
    orior_exact_zero(&total);
    for (int k = 0; k < 6; k += 1)
    {
        z_add(&total, &tail[k], &total);
    }
    OriorExactInteger value;
    times_over(&total, &scale, &multiplier, &value);
    rs_read(&sum, work, digits, main_sum);
    rs_read(&value, work, digits, remainder);
}

void rs_em_at(int64_t nu, const DecimalPair *p, int32_t digits, OriorExactInteger *result)
{
    const int32_t deep = digits + ZETA_GUARD;
    DecimalPair t;
    rs_time_at(nu, p, deep, &t);
    OriorExactInteger theta;
    (void)rs_theta_at(&t, deep, &theta);
    OriorExactInteger cosine;
    OriorExactInteger sine;
    zeta_cos_sin(&theta, deep, &cosine, &sine);
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
    z_floor_div(&first, &scale, result);
}

/* ---------------------------------------------------------------- every C_n of Gabcke */

/** @brief The Euler numbers E_0, E_2, ... found so far, with their signs. */
static ZetaRow euler_values = {NULL, 0u, 0u};

/** @brief The lambda_l found so far. */
static ZetaRow lambda_values = {NULL, 0u, 0u};

/** @brief The d rows found so far. */
static ZetaRow *d_rows = NULL;

/** @brief How many d rows. */
static size_t d_row_count = 0u;

void rs_euler_abs(int64_t k, OriorExactInteger *result)
{
    if (euler_values.count == 0u)
    {
        OriorExactInteger one;
        z_set(&one, 1);
        row_push(&euler_values, &one);
    }
    while ((int64_t)euler_values.count <= k)
    {
        const int64_t n = (int64_t)euler_values.count;
        OriorExactInteger total;
        orior_exact_zero(&total);
        OriorExactInteger choose;
        z_set(&choose, 1);
        for (int64_t i = 0; i <= (2 * n); i += 1)
        {
            if (((i % 2) == 0) && ((i / 2) < n))
            {
                OriorExactInteger term;
                z_mul(&choose, &euler_values.values[i / 2], &term);
                z_add(&total, &term, &total);
            }
            z_mul_small(&choose, (2 * n) - i, &choose);
            OriorExactInteger under;
            z_set(&under, i + 1);
            z_div_exact(&choose, &under, &choose);
        }
        z_neg(&total, &total);
        row_push(&euler_values, &total);
    }
    z_abs(&euler_values.values[k], result);
}

void rs_lambda(int64_t l, OriorExactInteger *result)
{
    if (lambda_values.count == 0u)
    {
        OriorExactInteger one;
        z_set(&one, 1);
        row_push(&lambda_values, &one);
    }
    while ((int64_t)lambda_values.count <= l)
    {
        const int64_t n = (int64_t)lambda_values.count - 1;
        OriorExactInteger total;
        orior_exact_zero(&total);
        for (int64_t k = 0; k <= n; k += 1)
        {
            OriorExactInteger term;
            rs_euler_abs(k + 1, &term);
            OriorExactInteger power;
            z_two_to((4 * k) + 1, &power);
            z_mul(&term, &power, &term);
            z_mul(&term, &lambda_values.values[n - k], &term);
            z_add(&total, &term, &total);
        }
        OriorExactInteger under;
        z_set(&under, n + 1);
        z_floor_div(&total, &under, &total);
        row_push(&lambda_values, &total);
    }
    *result = lambda_values.values[l];
}

const ZetaRow *rs_d_row(int64_t n)
{
    if (d_row_count == 0u)
    {
        d_rows = (ZetaRow *)rs_memory(platform_cell_cleared(1u, sizeof(ZetaRow), platform_place_g_program_owner, 0));
        OriorExactInteger one;
        z_set(&one, 1);
        row_push(&d_rows[0], &one);
        d_row_count = 1u;
    }
    while ((int64_t)d_row_count <= n)
    {
        const int64_t m = (int64_t)d_row_count - 1;
        d_rows = (ZetaRow *)rs_memory(platform_cell_grown(d_rows, d_row_count * sizeof(ZetaRow),
                                                          (d_row_count + 1u) * sizeof(ZetaRow),
                                                          platform_place_g_program_owner, 0));
        ZetaRow *const row = &d_rows[d_row_count];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = row, .bytes = sizeof(*row), .val = 0);
        const ZetaRow *const before = &d_rows[d_row_count - 1u];
        for (int64_t k = 0; k <= ((3 * (m + 1)) / 4); k += 1)
        {
            OriorExactInteger value;
            if ((3 * (m + 1)) == (4 * k))
            {
                rs_lambda((m + 1) / 4, &value);
            }
            else
            {
                // prev is the row before, with one zero after its end.
                OriorExactInteger at_k;
                orior_exact_zero(&at_k);
                if ((size_t)k < before->count)
                {
                    at_k = before->values[k];
                }
                z_mul_small(&at_k, ((3 * m) + 1 - (4 * k)) * ((3 * m) + 2 - (4 * k)), &value);
                if ((k > 0) && ((size_t)(k - 1) < before->count))
                {
                    z_add(&value, &before->values[k - 1], &value);
                }
            }
            row_push(row, &value);
        }
        d_row_count += 1u;
    }
    return &d_rows[n];
}

void rs_curve_begin(ZetaCurve *curve, int32_t digits)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = curve, .bytes = sizeof(*curve), .val = 0);
    curve->digits = digits;
    z_ten_to(digits, &curve->scale);
    zeta_pi(digits, &curve->big_pi);
    OriorExactInteger square;
    z_mul(&curve->scale, &curve->scale, &square);
    z_mul_small(&square, 2, &square);
    OriorExactInteger root_two;
    z_isqrt(&square, &root_two);
    OriorExactInteger twice;
    z_mul_small(&curve->scale, 2, &twice);
    OriorExactInteger value;
    z_sub(&twice, &root_two, &value);
    z_mul(&value, &curve->scale, &value);
    z_isqrt(&value, &value);
    OriorExactInteger two;
    z_set(&two, 2);
    z_floor_div(&value, &two, &curve->root[0]);
    z_add(&twice, &root_two, &value);
    z_mul(&value, &curve->scale, &value);
    z_isqrt(&value, &value);
    z_floor_div(&value, &two, &curve->root[1]);
    row_push(&curve->up, &curve->scale);
    row_push(&curve->down, &curve->scale);
}

void rs_curve_free(ZetaCurve *curve)
{
    row_free(&curve->up);
    row_free(&curve->down);
    row_free(&curve->phi);
}

/** @brief The curves kept, one per count of digits. */
static ZetaCurve **curves = NULL;

/** @brief How many curves. */
static size_t curve_count = 0u;

ZetaCurve *rs_curve(int32_t digits)
{
    for (size_t at = 0u; at < curve_count; at += 1u)
    {
        if (curves[at]->digits == digits)
        {
            return curves[at];
        }
    }
    curves = (ZetaCurve **)rs_memory(platform_cell_grown(curves, curve_count * sizeof(ZetaCurve *),
                                                         (curve_count + 1u) * sizeof(ZetaCurve *),
                                                         platform_place_g_program_owner, 0));
    curves[curve_count] =
        (ZetaCurve *)rs_memory(platform_cell_taken(sizeof(ZetaCurve), platform_place_g_program_owner, 0));
    rs_curve_begin(curves[curve_count], digits);
    curve_count += 1u;
    return curves[curve_count - 1u];
}

/**
 * @brief pi^e at the scale of the curve, for a signed e.
 *
 * @param[in,out] curve The curve [BORROWS].
 * @param[in]     e     The power.
 * @return              The value [BORROWS].
 */
static const OriorExactInteger *pi_power(ZetaCurve *curve, int64_t e)
{
    const int64_t size = (e < 0) ? -e : e;
    while ((int64_t)curve->up.count <= size)
    {
        OriorExactInteger value;
        times_over(&curve->up.values[curve->up.count - 1u], &curve->big_pi, &curve->scale, &value);
        row_push(&curve->up, &value);
        times_over(&curve->down.values[curve->down.count - 1u], &curve->scale, &curve->big_pi, &value);
        row_push(&curve->down, &value);
    }
    return (e < 0) ? &curve->down.values[size] : &curve->up.values[size];
}

/**
 * @brief The coefficient phi_j of z^(2j) in F, at the scale of the curve.
 *
 * @param[in,out] curve The curve [BORROWS].
 * @param[in]     j     The index.
 * @return              The value [BORROWS].
 * @note F(z) = cos(pi z^2 / 2 + 3pi/8) sec(pi z). The product of the two series puts
 *       sign |E_2(j-m)| / (2^m m! (2(j - m))!) pi^(2j - m) at each m. That is times sin(pi/8) where m is even, and
 *       cos(pi/8) where m is odd. The sign runs +, -, -, + in m mod 4.
 */
static const OriorExactInteger *phi_at(ZetaCurve *curve, int64_t j)
{
    static const int SIGNS[4] = {1, -1, -1, 1};
    while ((int64_t)curve->phi.count <= j)
    {
        const int64_t index = (int64_t)curve->phi.count;
        OriorExactInteger total;
        orior_exact_zero(&total);
        for (int64_t m = 0; m <= index; m += 1)
        {
            OriorExactInteger num;
            rs_euler_abs(index - m, &num);
            OriorExactInteger den;
            factorial_of(m, &den);
            OriorExactInteger part;
            factorial_of(2 * (index - m), &part);
            z_mul(&den, &part, &den);
            z_two_to(m, &part);
            z_mul(&den, &part, &den);
            OriorExactInteger divisor;
            z_gcd(&num, &den, &divisor);
            z_div_exact(&num, &divisor, &num);
            z_div_exact(&den, &divisor, &den);
            z_mul_small(&num, SIGNS[m % 4], &num);
            OriorExactInteger value;
            z_mul(&num, pi_power(curve, (2 * index) - m), &value);
            z_mul(&value, &curve->root[m % 2], &value);
            z_mul(&den, &curve->scale, &den);
            z_floor_div(&value, &den, &value);
            z_add(&total, &value, &total);
        }
        row_push(&curve->phi, &total);
    }
    return &curve->phi.values[j];
}

void rs_curve_gamma(ZetaCurve *curve, int64_t n, int64_t count, ZetaRow *out)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = out, .bytes = sizeof(*out), .val = 0);
    const ZetaRow *const row = rs_d_row(n);
    for (int64_t i = 0; i < count; i += 1)
    {
        OriorExactInteger total;
        orior_exact_zero(&total);
        for (size_t k = 0u; k < row->count; k += 1u)
        {
            // k is at most 3n / 4, and fits a machine integer.
            const int64_t index = (int64_t)k;
            const int64_t m = (3 * n) - (4 * index);
            if (((i + m) % 2) != 0)
            {
                continue;
            }
            OriorExactInteger value;
            binomial(i + m, m, &value);
            z_mul(&value, &row->values[k], &value);
            z_mul(&value, pi_power(curve, (2 * index) - (2 * n)), &value);
            z_mul(&value, phi_at(curve, (i + m) / 2), &value);
            z_floor_div(&value, &curve->scale, &value);
            z_add(&total, &value, &total);
        }
        OriorExactInteger value;
        floor_shift(&total, 2 * n, &value);
        row_push(out, &value);
    }
}

/** @brief A list of Taylor coefficients, kept by its curve, count and digits. */
typedef struct
{
    int64_t n;      /**< The index of the curve. */
    int64_t count;  /**< How many coefficients. */
    int32_t digits; /**< The places. */
    ZetaRow row;    /**< The coefficients. */
} GammaEntry;

/** @brief The lists kept. */
static GammaEntry *gammas = NULL;

/** @brief How many lists. */
static size_t gamma_count = 0u;

/** @brief How many lists the array holds. */
static size_t gamma_capacity = 0u;

const ZetaRow *rs_gamma(int64_t n, int64_t count, int32_t digits)
{
    for (size_t at = 0u; at < gamma_count; at += 1u)
    {
        if ((gammas[at].n == n) && (gammas[at].count == count) && (gammas[at].digits == digits))
        {
            return &gammas[at].row;
        }
    }
    if (gamma_count >= gamma_capacity)
    {
        gamma_capacity = (gamma_capacity == 0u) ? 64u : (gamma_capacity * 2u);
        gammas = (GammaEntry *)rs_memory(platform_cell_grown(gammas, gamma_count * sizeof(GammaEntry),
                                                             gamma_capacity * sizeof(GammaEntry),
                                                             platform_place_g_program_owner, 0));
    }
    GammaEntry *const entry = &gammas[gamma_count];
    entry->n = n;
    entry->count = count;
    entry->digits = digits;
    rs_curve_gamma(rs_curve(digits), n, count, &entry->row);
    gamma_count += 1u;
    return &entry->row;
}

void rs_c_value(int64_t n, int64_t count, int32_t digits, const OriorExactInteger *a, int64_t bits,
                OriorExactInteger *result)
{
    const ZetaRow *const row = rs_gamma(n, count, digits);
    OriorExactInteger power;
    z_two_to(bits, &power);
    orior_exact_zero(result);
    for (size_t at = row->count; at > 0u; at -= 1u)
    {
        times_over(result, a, &power, result);
        z_add(result, &row->values[at - 1u], result);
    }
}

void rs_c_at(int64_t n, const OriorExactInteger *num, const OriorExactInteger *den, int32_t digits,
             OriorExactInteger *result)
{
    const int sign = (((n % 2) != 0) && (num->sign < 0)) ? -1 : 1;
    OriorExactInteger size;
    z_abs(num, &size);
    int64_t count = 64;
    OriorExactInteger reads[2];
    for (;;)
    {
        for (int which = 0; which < 2; which += 1)
        {
            const int64_t terms = count << which;
            // The terms and n are small, and the sum fits the width of the places.
            const int32_t work = digits + ZETA_GUARD + (int32_t)terms + (int32_t)(3 * n);
            const ZetaRow *const row = rs_gamma(n, terms, work);
            OriorExactInteger acc;
            orior_exact_zero(&acc);
            for (size_t at = row->count; at > 0u; at -= 1u)
            {
                times_over(&acc, &size, den, &acc);
                z_add(&acc, &row->values[at - 1u], &acc);
            }
            rs_read(&acc, work, digits + ZETA_GUARD, &reads[which]);
        }
        OriorExactInteger first;
        OriorExactInteger second;
        rs_toward_zero(&reads[0], &first);
        rs_toward_zero(&reads[1], &second);
        if (orior_exact_equal(&first, &second))
        {
            break;
        }
        count *= 2;
    }
    z_mul_small(&reads[1], sign, result);
}

void rs_c_verdict(int64_t n, const OriorExactInteger *a, int64_t bits, int32_t *places, OriorExactInteger *result)
{
    int64_t count = 64;
    for (;;)
    {
        // The count and n are small, and the extra digits fit the width of the places.
        const int32_t extra = (int32_t)(count + (3 * n));
        OriorExactInteger value;
        OriorExactInteger one;
        rs_c_value(n, count, *places + ZETA_GUARD + extra, a, bits, &value);
        rs_read(&value, *places + ZETA_GUARD + extra, *places, &one);
        rs_c_value(n, 2 * count, *places + ZETA_GUARD + (2 * extra), a, bits, &value);
        rs_read(&value, *places + ZETA_GUARD + (2 * extra), *places, result);
        const int agree = orior_exact_equal(&one, result);
        const int read = result->sign != 0;
        if (agree && read)
        {
            return;
        }
        if (!agree)
        {
            count *= 2;
        }
        else
        {
            *places *= 2;
        }
    }
}

void rs_remainder_at(int64_t nu, const DecimalPair *p, int64_t cut, int32_t digits, OriorExactInteger *result)
{
    const int32_t work = digits + ZETA_GUARD;
    OriorExactInteger scale;
    z_ten_to(work, &scale);
    OriorExactInteger unit;
    z_ten_to(p->places, &unit);
    OriorExactInteger big_x;
    z_mul_small(&unit, nu, &big_x);
    z_add(&big_x, &p->numerator, &big_x);
    OriorExactInteger square;
    z_mul(&scale, &scale, &square);
    z_mul(&square, &unit, &square);
    z_floor_div(&square, &big_x, &square);
    OriorExactInteger inv_root;
    z_isqrt(&square, &inv_root);
    OriorExactInteger z_num;
    z_mul_small(&p->numerator, 2, &z_num);
    z_sub(&unit, &z_num, &z_num);
    OriorExactInteger total;
    orior_exact_zero(&total);
    OriorExactInteger unit_power;
    OriorExactInteger x_power;
    z_set(&unit_power, 1);
    z_set(&x_power, 1);
    for (int64_t n = 0; n <= cut; n += 1)
    {
        OriorExactInteger value;
        rs_c_at(n, &z_num, &unit, digits, &value);
        z_mul(&value, &inv_root, &value);
        z_mul(&value, &unit_power, &value);
        OriorExactInteger under;
        z_mul(&scale, &x_power, &under);
        z_floor_div(&value, &under, &value);
        z_add(&total, &value, &total);
        z_mul(&unit_power, &unit, &unit_power);
        z_mul(&x_power, &big_x, &x_power);
    }
    z_mul_small(&total, cell_sign(nu), result);
}

void rs_cut_at(int64_t nu, const DecimalPair *p, int32_t digits, int64_t cut, OriorExactInteger *main_sum,
               OriorExactInteger *remainder)
{
    const int32_t work = digits + ZETA_GUARD;
    DecimalPair t;
    rs_time_at(nu, p, work, &t);
    OriorExactInteger theta;
    (void)rs_theta_at(&t, work, &theta);
    OriorExactInteger sum;
    rs_main_sum(nu, &t, &theta, work, &sum);
    rs_toward_zero(&sum, main_sum);
    OriorExactInteger value;
    rs_remainder_at(nu, p, cut, digits, &value);
    rs_toward_zero(&value, remainder);
}
