// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_exact.c
 * @brief The exact arithmetic of the zeta examples. zeta_exact.h states each rule.
 */
#include "zeta_exact.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/** @brief The table of zeta zeros of Odlyzko, zeros1: the first forty ordinates as printed there. */
const char *const ZETA_PUBLISHED_ZEROS[40] = {
    "14.134725142",  "21.022039639",  "25.010857580",  "30.424876126",  "32.935061588",  "37.586178159",
    "40.918719012",  "43.327073281",  "48.005150881",  "49.773832478",  "52.970321478",  "56.446247697",
    "59.347044003",  "60.831778525",  "65.112544048",  "67.079810529",  "69.546401711",  "72.067157674",
    "75.704690699",  "77.144840069",  "79.337375020",  "82.910380854",  "84.735492981",  "87.425274613",
    "88.809111208",  "92.491899271",  "94.651344041",  "95.870634228",  "98.831194218",  "101.317851006",
    "103.725538040", "105.446623052", "107.168611184", "111.029535543", "111.874659177", "114.320220915",
    "116.226680321", "118.790782866", "121.370125002", "122.946829294"};

/* ---------------------------------------------------------------- integers */

void zeta_require(OriorExactStatus status, const char *what)
{
    if (status != ORIOR_EXACT_OK)
    {
        fprintf(stderr, "  the exact %s failed with status %d. The value does not fit the width.\n", what, (int)status);
        exit(2);
    }
}

void zeta_stop(const char *message)
{
    fprintf(stderr, "  %s\n", message);
    exit(2);
}

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *zeta_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

void z_set(OriorExactInteger *value, int64_t number)
{
    orior_exact_zero(value);
    if (number == 0)
    {
        return;
    }
    // The unsigned negation holds the magnitude of every int64_t, INT64_MIN included.
    const uint64_t magnitude = (number < 0) ? (0u - (uint64_t)number) : (uint64_t)number;
    // Each limb holds 32 bits of the magnitude. The mask and the shift keep each part in range.
    value->limb[0] = (uint32_t)(magnitude & 0xFFFFFFFFu);
    value->limb[1] = (uint32_t)(magnitude >> 32u);
    value->sign = (number < 0) ? -1 : 1;
}

int64_t z_small(const OriorExactInteger *value)
{
    for (size_t at = 2u; at < ORIOR_EXACT_LIMBS; at += 1u)
    {
        if (value->limb[at] != 0u)
        {
            zeta_stop("a value does not fit a machine integer");
        }
    }
    const uint64_t magnitude = ((uint64_t)value->limb[1] << 32u) | (uint64_t)value->limb[0];
    if (magnitude > (uint64_t)INT64_MAX)
    {
        zeta_stop("a value does not fit a machine integer");
    }
    // The magnitude is at most INT64_MAX, and the signed value fits.
    return (value->sign < 0) ? -(int64_t)magnitude : (int64_t)magnitude;
}

void z_add(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    zeta_require(orior_exact_add(left, right, result), "add");
}

void z_sub(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    zeta_require(orior_exact_subtract(left, right, result), "subtract");
}

void z_mul(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    zeta_require(orior_exact_multiply(left, right, result), "multiply");
}

void z_mul_small(const OriorExactInteger *value, int64_t factor, OriorExactInteger *result)
{
    OriorExactInteger other;
    z_set(&other, factor);
    z_mul(value, &other, result);
}

void z_floor_div(const OriorExactInteger *numerator, const OriorExactInteger *divisor, OriorExactInteger *quotient)
{
    OriorExactInteger remainder;
    zeta_require(orior_exact_divide(numerator, divisor, quotient, &remainder), "division");
    if ((remainder.sign != 0) && (remainder.sign != divisor->sign))
    {
        OriorExactInteger one;
        z_set(&one, 1);
        z_sub(quotient, &one, quotient);
    }
}

void z_mod(const OriorExactInteger *numerator, const OriorExactInteger *divisor, OriorExactInteger *remainder)
{
    OriorExactInteger quotient;
    zeta_require(orior_exact_divide(numerator, divisor, &quotient, remainder), "division");
    if ((remainder->sign != 0) && (remainder->sign != divisor->sign))
    {
        z_add(remainder, divisor, remainder);
    }
}

int z_cmp(const OriorExactInteger *left, const OriorExactInteger *right)
{
    return orior_exact_compare(left, right);
}

void z_abs(const OriorExactInteger *value, OriorExactInteger *result)
{
    *result = *value;
    if (result->sign < 0)
    {
        result->sign = 1;
    }
}

void z_neg(const OriorExactInteger *value, OriorExactInteger *result)
{
    *result = *value;
    result->sign = -result->sign;
}

void z_ten_to(int32_t power, OriorExactInteger *result)
{
    if (power < 0)
    {
        zeta_stop("a negative power of ten");
    }
    z_set(result, 1);
    // The power is at least zero, and fits a uint32_t.
    zeta_require(orior_exact_scale_by_ten(result, (uint32_t)power), "scale");
}

void z_two_to(int64_t power, OriorExactInteger *result)
{
    if ((power < 0) || ((uint64_t)power >= (uint64_t)ORIOR_EXACT_BITS))
    {
        zeta_stop("a power of two does not fit the width");
    }
    orior_exact_zero(result);
    // The power is in range, and the limb index and the bit within the limb follow from it.
    result->limb[(size_t)power / 32u] = (uint32_t)1u << ((uint32_t)power % 32u);
    result->sign = 1;
}

int64_t z_bit_length(const OriorExactInteger *value)
{
    for (size_t at = ORIOR_EXACT_LIMBS; at > 0u; at -= 1u)
    {
        uint32_t word = value->limb[at - 1u];
        if (word != 0u)
        {
            int64_t bits = 0;
            while (word != 0u)
            {
                bits += 1;
                word >>= 1u;
            }
            // The limb count is far below INT64_MAX / 32.
            return ((int64_t)(at - 1u) * 32) + bits;
        }
    }
    return 0;
}

void z_gcd(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    zeta_require(orior_exact_gcd(left, right, result), "gcd");
}

void z_div_exact(const OriorExactInteger *numerator, const OriorExactInteger *divisor, OriorExactInteger *quotient)
{
    zeta_require(orior_exact_divide_exact(numerator, divisor, quotient), "exact division");
}

void z_isqrt(const OriorExactInteger *value, OriorExactInteger *result)
{
    if (value->sign < 0)
    {
        zeta_stop("integer square root of a negative number");
    }
    OriorExactInteger two;
    z_set(&two, 2);
    if (z_cmp(value, &two) < 0)
    {
        *result = *value;
        return;
    }
    OriorExactInteger guess;
    z_two_to((z_bit_length(value) + 1) / 2, &guess);
    for (;;)
    {
        OriorExactInteger share;
        z_floor_div(value, &guess, &share);
        OriorExactInteger stepped;
        z_add(&guess, &share, &stepped);
        z_floor_div(&stepped, &two, &stepped);
        if (z_cmp(&stepped, &guess) >= 0)
        {
            *result = guess;
            return;
        }
        guess = stepped;
    }
}

void z_text(const OriorExactInteger *value, char *text, size_t capacity)
{
    if (value->sign == 0)
    {
        snprintf(text, capacity, "0");
        return;
    }
    enum
    {
        CHUNKS = 4096
    };
    static uint32_t chunk[CHUNKS];
    size_t chunks = 0u;
    OriorExactInteger rest;
    z_abs(value, &rest);
    OriorExactInteger billion;
    z_set(&billion, 1000000000);
    while (rest.sign != 0)
    {
        OriorExactInteger quotient;
        OriorExactInteger remainder;
        zeta_require(orior_exact_divide(&rest, &billion, &quotient, &remainder), "division");
        if (chunks >= CHUNKS)
        {
            zeta_stop("a number is too long to print");
        }
        chunk[chunks] = remainder.limb[0];
        chunks += 1u;
        rest = quotient;
    }
    size_t used = 0u;
    if (value->sign < 0)
    {
        used += (size_t)snprintf(text + used, capacity - used, "-");
    }
    used += (size_t)snprintf(text + used, capacity - used, "%u", (unsigned)chunk[chunks - 1u]);
    for (size_t at = chunks - 1u; (at > 0u) && (used < capacity); at -= 1u)
    {
        used += (size_t)snprintf(text + used, capacity - used, "%09u", (unsigned)chunk[at - 1u]);
    }
}

void z_print(const OriorExactInteger *value, FILE *out)
{
    static char text[40000];
    z_text(value, text, sizeof(text));
    fputs(text, out);
}

/* ---------------------------------------------------------------- decimal pairs */

void pair_make(const OriorExactInteger *numerator, int32_t places, DecimalPair *pair)
{
    if (numerator->sign == 0)
    {
        orior_exact_zero(&pair->numerator);
        pair->places = 0;
        return;
    }
    OriorExactInteger value = *numerator;
    OriorExactInteger ten;
    z_set(&ten, 10);
    int32_t kept = places;
    while (kept > 0)
    {
        OriorExactInteger quotient;
        OriorExactInteger remainder;
        zeta_require(orior_exact_divide(&value, &ten, &quotient, &remainder), "division");
        if (remainder.sign != 0)
        {
            break;
        }
        value = quotient;
        kept -= 1;
    }
    pair->numerator = value;
    pair->places = kept;
}

void pair_small(int64_t numerator, int32_t places, DecimalPair *pair)
{
    OriorExactInteger value;
    z_set(&value, numerator);
    pair_make(&value, places, pair);
}

int32_t pair_common(const DecimalPair *left, const DecimalPair *right, OriorExactInteger *left_value,
                    OriorExactInteger *right_value)
{
    const int32_t places = (left->places > right->places) ? left->places : right->places;
    *left_value = left->numerator;
    *right_value = right->numerator;
    // Each difference is at least zero, and fits a uint32_t.
    zeta_require(orior_exact_scale_by_ten(left_value, (uint32_t)(places - left->places)), "scale");
    zeta_require(orior_exact_scale_by_ten(right_value, (uint32_t)(places - right->places)), "scale");
    return places;
}

void pair_plus(const DecimalPair *left, const DecimalPair *right, DecimalPair *result)
{
    OriorExactInteger first;
    OriorExactInteger second;
    const int32_t places = pair_common(left, right, &first, &second);
    z_add(&first, &second, &first);
    pair_make(&first, places, result);
}

void pair_minus(const DecimalPair *left, const DecimalPair *right, DecimalPair *result)
{
    OriorExactInteger first;
    OriorExactInteger second;
    const int32_t places = pair_common(left, right, &first, &second);
    z_sub(&first, &second, &first);
    pair_make(&first, places, result);
}

void pair_half(const DecimalPair *value, DecimalPair *result)
{
    OriorExactInteger numerator;
    z_mul_small(&value->numerator, 5, &numerator);
    pair_make(&numerator, value->places + 1, result);
}

void pair_twice(const DecimalPair *value, DecimalPair *result)
{
    OriorExactInteger numerator;
    z_mul_small(&value->numerator, 2, &numerator);
    pair_make(&numerator, value->places, result);
}

void pair_middle(const DecimalPair *left, const DecimalPair *right, DecimalPair *result)
{
    DecimalPair sum;
    pair_plus(left, right, &sum);
    pair_half(&sum, result);
}

int pair_below(const DecimalPair *left, const DecimalPair *right)
{
    OriorExactInteger first;
    OriorExactInteger second;
    (void)pair_common(left, right, &first, &second);
    return z_cmp(&first, &second) < 0;
}

int pair_equal(const DecimalPair *left, const DecimalPair *right)
{
    return (left->places == right->places) && orior_exact_equal(&left->numerator, &right->numerator);
}

void pair_units(const char *text, DecimalPair *pair)
{
    OriorExactInteger value;
    orior_exact_zero(&value);
    OriorExactInteger ten;
    z_set(&ten, 10);
    int negative = 0;
    int32_t places = 0;
    int after = 0;
    int digits = 0;
    const char *at = text;
    while ((*at == ' ') || (*at == '\t'))
    {
        at += 1;
    }
    if ((*at == '+') || (*at == '-'))
    {
        negative = *at == '-';
        at += 1;
    }
    for (; *at != '\0'; at += 1)
    {
        if (*at == '.')
        {
            if (after)
            {
                zeta_stop("decimal text with two points");
            }
            after = 1;
            continue;
        }
        if ((*at < '0') || (*at > '9'))
        {
            zeta_stop("text that is not plain decimal");
        }
        OriorExactInteger digit;
        z_set(&digit, *at - '0');
        z_mul(&value, &ten, &value);
        z_add(&value, &digit, &value);
        digits += 1;
        places += after ? 1 : 0;
    }
    if (digits == 0)
    {
        zeta_stop("decimal text with no digit");
    }
    if (negative)
    {
        value.sign = -value.sign;
    }
    pair_make(&value, places, pair);
}

void pair_text(const DecimalPair *value, int32_t places, char *text, size_t capacity)
{
    OriorExactInteger scaled = value->numerator;
    zeta_require(orior_exact_scale_by_ten(&scaled, (uint32_t)places), "scale");
    OriorExactInteger under;
    z_ten_to(value->places, &under);
    z_floor_div(&scaled, &under, &scaled);
    OriorExactInteger unit;
    z_ten_to(places, &unit);
    OriorExactInteger whole;
    OriorExactInteger part;
    z_floor_div(&scaled, &unit, &whole);
    z_mod(&scaled, &unit, &part);
    static char whole_text[4096];
    static char part_text[4096];
    z_text(&whole, whole_text, sizeof(whole_text));
    z_text(&part, part_text, sizeof(part_text));
    const size_t length = strlen(part_text);
    // The places are at least zero here, and the pad count fits an int.
    const int pad = (length < (size_t)places) ? (int)((size_t)places - length) : 0;
    if (pad == 0)
    {
        snprintf(text, capacity, "%s.%s", whole_text, part_text);
    }
    else
    {
        size_t used = (size_t)snprintf(text, capacity, "%s.", whole_text);
        for (int count = 0; (count < pad) && (used + 1u < capacity); count += 1)
        {
            text[used] = '0';
            used += 1u;
        }
        snprintf(text + used, capacity - used, "%s", part_text);
    }
}

void pair_print(const DecimalPair *value, int32_t places, FILE *out)
{
    static char text[8192];
    pair_text(value, places, text, sizeof(text));
    fputs(text, out);
}

int pair_holds(const DecimalPair *low, const DecimalPair *high, const char *published)
{
    DecimalPair value;
    pair_units(published, &value);
    DecimalPair unit;
    pair_small(5, value.places + 1, &unit);
    DecimalPair top;
    DecimalPair bottom;
    pair_plus(&value, &unit, &top);
    pair_minus(&value, &unit, &bottom);
    return !pair_below(&top, low) && !pair_below(high, &bottom);
}

/* ---------------------------------------------------------------- the natural constants */

/**
 * @brief floor(scale * arctan(1 / denominator)), by the series of terms with alternate signs, in integers.
 *
 * @param[in]  denominator The reciprocal of the argument.
 * @param[in]  scale       The scale [BORROWS].
 * @param[out] result      The value [BORROWS].
 * @note Each term is an exact integer division of the scale. The walk stops when the running power floors to zero.
 */
void zeta_arctan_inverse(int64_t denominator, const OriorExactInteger *scale, OriorExactInteger *result)
{
    orior_exact_zero(result);
    OriorExactInteger under;
    z_set(&under, denominator);
    OriorExactInteger power;
    z_floor_div(scale, &under, &power);
    OriorExactInteger squared;
    z_set(&squared, denominator * denominator);
    for (int64_t step = 0; power.sign != 0; step += 1)
    {
        OriorExactInteger odd;
        z_set(&odd, (2 * step) + 1);
        OriorExactInteger term;
        z_floor_div(&power, &odd, &term);
        if ((step % 2) == 0)
        {
            z_add(result, &term, result);
        }
        else
        {
            z_sub(result, &term, result);
        }
        z_floor_div(&power, &squared, &power);
    }
}

/**
 * @brief pi by the identity of Machin, pi = 16 arctan(1/5) - 4 arctan(1/239), times the scale.
 *
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The value [BORROWS].
 */
void zeta_pi_machin(const OriorExactInteger *scale, OriorExactInteger *result)
{
    OriorExactInteger fifth;
    OriorExactInteger part;
    zeta_arctan_inverse(5, scale, &fifth);
    zeta_arctan_inverse(239, scale, &part);
    z_mul_small(&fifth, 16, &fifth);
    z_mul_small(&part, 4, &part);
    z_sub(&fifth, &part, result);
}

/**
 * @brief pi by the identity of Euler, pi = 4 (arctan(1/2) + arctan(1/3)), times the scale.
 *
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The value [BORROWS].
 */
void zeta_pi_euler(const OriorExactInteger *scale, OriorExactInteger *result)
{
    OriorExactInteger half;
    OriorExactInteger third;
    zeta_arctan_inverse(2, scale, &half);
    zeta_arctan_inverse(3, scale, &third);
    z_add(&half, &third, &half);
    z_mul_small(&half, 4, result);
}

/**
 * @brief ln 2 = 2 artanh(1/3) = 2 sum over k of 1 / ((2k + 1) 3^(2k + 1)), times the scale.
 *
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The value [BORROWS].
 */
static void ln_two_artanh(const OriorExactInteger *scale, OriorExactInteger *result)
{
    OriorExactInteger total;
    orior_exact_zero(&total);
    OriorExactInteger three;
    z_set(&three, 3);
    OriorExactInteger nine;
    z_set(&nine, 9);
    OriorExactInteger power;
    z_floor_div(scale, &three, &power);
    for (int64_t step = 0; power.sign != 0; step += 1)
    {
        OriorExactInteger odd;
        z_set(&odd, (2 * step) + 1);
        OriorExactInteger term;
        z_floor_div(&power, &odd, &term);
        z_add(&total, &term, &total);
        z_floor_div(&power, &nine, &power);
    }
    z_mul_small(&total, 2, result);
}

/**
 * @brief The floor sum of (n/d)^(2k+1) / (2k+1) at the scale, for 0 <= n/d < 1.
 *
 * @param[in]  numerator   The numerator n [BORROWS].
 * @param[in]  denominator The denominator d [BORROWS].
 * @param[in]  scale       The scale [BORROWS].
 * @param[out] result      The value [BORROWS].
 */
static void artanh_ratio(const OriorExactInteger *numerator, const OriorExactInteger *denominator,
                         const OriorExactInteger *scale, OriorExactInteger *result)
{
    orior_exact_zero(result);
    OriorExactInteger power;
    z_mul(scale, numerator, &power);
    z_floor_div(&power, denominator, &power);
    OriorExactInteger top_square;
    OriorExactInteger under_square;
    z_mul(numerator, numerator, &top_square);
    z_mul(denominator, denominator, &under_square);
    for (int64_t step = 0; power.sign != 0; step += 1)
    {
        OriorExactInteger odd;
        z_set(&odd, (2 * step) + 1);
        OriorExactInteger term;
        z_floor_div(&power, &odd, &term);
        z_add(result, &term, result);
        z_mul(&power, &top_square, &power);
        z_floor_div(&power, &under_square, &power);
    }
}

/**
 * @brief One route of a constant: its value at a scale, for an argument.
 *
 * @param[in]  argument The argument, or NULL where the constant takes none [BORROWS].
 * @param[in]  scale    The scale [BORROWS].
 * @param[out] result   The value [BORROWS].
 */
typedef void (*ConstantRoute)(const OriorExactInteger *argument, const OriorExactInteger *scale,
                              OriorExactInteger *result);

/**
 * @brief The two routes of a constant at places + ZETA_GUARD, the guard dropped. They must agree.
 *
 * @param[in]  first    The first route [BORROWS].
 * @param[in]  second   The second route [BORROWS].
 * @param[in]  argument The argument of the routes, or NULL [BORROWS].
 * @param[in]  places   The places.
 * @param[in]  name     The name of the constant, for the message [BORROWS].
 * @param[out] result   The agreed value [BORROWS].
 */
static void agree(ConstantRoute first, ConstantRoute second, const OriorExactInteger *argument, int32_t places,
                  const char *name, OriorExactInteger *result)
{
    if (places < 1)
    {
        zeta_stop("places must be at least 1");
    }
    OriorExactInteger scale;
    z_ten_to(places + ZETA_GUARD, &scale);
    OriorExactInteger drop;
    z_ten_to(ZETA_GUARD, &drop);
    OriorExactInteger one;
    OriorExactInteger two;
    first(argument, &scale, &one);
    second(argument, &scale, &two);
    z_floor_div(&one, &drop, &one);
    z_floor_div(&two, &drop, &two);
    if (!orior_exact_equal(&one, &two))
    {
        fprintf(stderr, "  %s: the two routes disagree at %d places\n", name, (int)places);
        exit(2);
    }
    *result = one;
}

/**
 * @brief The route of Machin, in the form that agree() calls.
 *
 * @param[in]  argument Unused [BORROWS].
 * @param[in]  scale    The scale [BORROWS].
 * @param[out] result   The value [BORROWS].
 */
static void route_machin(const OriorExactInteger *argument, const OriorExactInteger *scale, OriorExactInteger *result)
{
    (void)argument;
    zeta_pi_machin(scale, result);
}

/**
 * @brief The route of Euler, in the form that agree() calls.
 *
 * @param[in]  argument Unused [BORROWS].
 * @param[in]  scale    The scale [BORROWS].
 * @param[out] result   The value [BORROWS].
 */
static void route_euler(const OriorExactInteger *argument, const OriorExactInteger *scale, OriorExactInteger *result)
{
    (void)argument;
    zeta_pi_euler(scale, result);
}

/**
 * @brief ln n = k ln 2 + 2 artanh((n - 2^k) / (n + 2^k)), with 2^k the power of two at or below n.
 *
 * @param[in]  argument The argument n, at least one [BORROWS].
 * @param[in]  scale    The scale [BORROWS].
 * @param[out] result   The value [BORROWS].
 */
static void ln_by_two(const OriorExactInteger *argument, const OriorExactInteger *scale, OriorExactInteger *result)
{
    const int64_t k = z_bit_length(argument) - 1;
    OriorExactInteger power;
    z_two_to(k, &power);
    OriorExactInteger two;
    ln_two_artanh(scale, &two);
    z_mul_small(&two, k, &two);
    OriorExactInteger below;
    OriorExactInteger above;
    z_sub(argument, &power, &below);
    z_add(argument, &power, &above);
    OriorExactInteger rest;
    artanh_ratio(&below, &above, scale, &rest);
    z_mul_small(&rest, 2, &rest);
    z_add(&two, &rest, result);
}

/**
 * @brief ln n = j ln 3 + 2 artanh((n - 3^j) / (n + 3^j)), with ln 3 = 2 artanh(1/2).
 *
 * @param[in]  argument The argument n, at least one [BORROWS].
 * @param[in]  scale    The scale [BORROWS].
 * @param[out] result   The value [BORROWS].
 */
static void ln_by_three(const OriorExactInteger *argument, const OriorExactInteger *scale, OriorExactInteger *result)
{
    int64_t j = 0;
    OriorExactInteger power;
    z_set(&power, 1);
    for (;;)
    {
        OriorExactInteger next;
        z_mul_small(&power, 3, &next);
        if (z_cmp(&next, argument) > 0)
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
    OriorExactInteger three;
    artanh_ratio(&one, &two, scale, &three);
    z_mul_small(&three, 2 * j, &three);
    OriorExactInteger below;
    OriorExactInteger above;
    z_sub(argument, &power, &below);
    z_add(argument, &power, &above);
    OriorExactInteger rest;
    artanh_ratio(&below, &above, scale, &rest);
    z_mul_small(&rest, 2, &rest);
    z_add(&three, &rest, result);
}

/** @brief One kept value of a constant, at a key. */
typedef struct
{
    int64_t key_number;      /**< The argument, where it fits a machine integer. */
    char *key_text;          /**< The argument as text, or NULL [OWNS]. */
    int32_t key_places;      /**< The places. */
    int used;                /**< Nonzero where the entry holds a value. */
    OriorExactInteger value; /**< The value. */
} ConstantEntry;

/** @brief A table of kept constant values, by argument and places. A full bucket passes a key to the next one. */
typedef struct
{
    ConstantEntry *entries; /**< The entries [OWNS]. */
    size_t capacity;        /**< How many entries, a power of two. */
    size_t count;           /**< How many hold a value. */
} ConstantTable;

/**
 * @brief The hash of a key: a machine integer, or text where it is not NULL, and the places.
 *
 * @param[in] number The argument.
 * @param[in] text   The argument as text, or NULL [BORROWS].
 * @param[in] places The places.
 * @return           The hash.
 */
static uint64_t constant_hash(int64_t number, const char *text, int32_t places)
{
    uint64_t hash = ((uint64_t)number * 1000003u) ^ (uint64_t)(uint32_t)places;
    for (const char *at = text; (at != NULL) && (*at != '\0'); at += 1)
    {
        hash = (hash ^ (uint64_t)(unsigned char)*at) * 1099511628211u;
    }
    return hash;
}

/**
 * @brief Whether an entry holds a key.
 *
 * @param[in] entry  The entry [BORROWS].
 * @param[in] number The argument.
 * @param[in] text   The argument as text, or NULL [BORROWS].
 * @param[in] places The places.
 * @return           1 where the entry holds the key.
 */
static int constant_matches(const ConstantEntry *entry, int64_t number, const char *text, int32_t places)
{
    if ((entry->key_number != number) || (entry->key_places != places))
    {
        return 0;
    }
    if ((text == NULL) || (entry->key_text == NULL))
    {
        return (text == NULL) && (entry->key_text == NULL);
    }
    return strcmp(text, entry->key_text) == 0;
}

/**
 * @brief The entry of a key in a table, empty where the key is new.
 *
 * @param[in,out] table  The table [BORROWS].
 * @param[in]     number The argument.
 * @param[in]     text   The argument as text, or NULL [BORROWS].
 * @param[in]     places The places.
 * @return               The entry [BORROWS].
 */
static ConstantEntry *constant_slot(ConstantTable *table, int64_t number, const char *text, int32_t places)
{
    if ((table->count * 2u) >= table->capacity)
    {
        const size_t grown = (table->capacity == 0u) ? 256u : (table->capacity * 2u);
        ConstantEntry *const entries = (ConstantEntry *)zeta_memory(calloc(grown, sizeof(ConstantEntry)));
        for (size_t at = 0u; at < table->capacity; at += 1u)
        {
            if (table->entries[at].used)
            {
                const ConstantEntry *const old = &table->entries[at];
                size_t place = (size_t)(constant_hash(old->key_number, old->key_text, old->key_places) & (grown - 1u));
                while (entries[place].used)
                {
                    place = (place + 1u) & (grown - 1u);
                }
                entries[place] = *old;
            }
        }
        free(table->entries);
        table->entries = entries;
        table->capacity = grown;
    }
    size_t place = (size_t)(constant_hash(number, text, places) & (table->capacity - 1u));
    while (table->entries[place].used && !constant_matches(&table->entries[place], number, text, places))
    {
        place = (place + 1u) & (table->capacity - 1u);
    }
    return &table->entries[place];
}

/**
 * @brief Fills a new entry of a table with its key, and counts it.
 *
 * @param[in,out] table  The table [BORROWS].
 * @param[in,out] entry  The entry [BORROWS].
 * @param[in]     number The argument.
 * @param[in]     text   The argument as text, or NULL [BORROWS].
 * @param[in]     places The places.
 */
static void constant_fill(ConstantTable *table, ConstantEntry *entry, int64_t number, const char *text, int32_t places)
{
    entry->key_number = number;
    entry->key_text = NULL;
    if (text != NULL)
    {
        entry->key_text = (char *)zeta_memory(malloc(strlen(text) + 1u));
        memcpy(entry->key_text, text, strlen(text) + 1u);
    }
    entry->key_places = places;
    entry->used = 1;
    table->count += 1u;
}

void zeta_pi(int32_t places, OriorExactInteger *result)
{
    static ConstantTable table = {NULL, 0u, 0u};
    ConstantEntry *const entry = constant_slot(&table, 0, NULL, places);
    if (!entry->used)
    {
        agree(route_machin, route_euler, NULL, places, "pi", &entry->value);
        constant_fill(&table, entry, 0, NULL, places);
    }
    *result = entry->value;
}

void zeta_ln(int64_t n, int32_t places, OriorExactInteger *result)
{
    static ConstantTable table = {NULL, 0u, 0u};
    ConstantEntry *const entry = constant_slot(&table, n, NULL, places);
    if (!entry->used)
    {
        char name[32];
        snprintf(name, sizeof(name), "ln %lld", (long long)n);
        OriorExactInteger argument;
        z_set(&argument, n);
        agree(ln_by_two, ln_by_three, &argument, places, name, &entry->value);
        constant_fill(&table, entry, n, NULL, places);
    }
    *result = entry->value;
}

void zeta_ln_big(const OriorExactInteger *n, int32_t places, OriorExactInteger *result)
{
    if (z_bit_length(n) <= 62)
    {
        zeta_ln(z_small(n), places, result);
        return;
    }
    static ConstantTable table = {NULL, 0u, 0u};
    static char text[40000];
    z_text(n, text, sizeof(text));
    ConstantEntry *const entry = constant_slot(&table, 0, text, places);
    if (!entry->used)
    {
        agree(ln_by_two, ln_by_three, n, places, "ln of a large integer", &entry->value);
        constant_fill(&table, entry, 0, text, places);
    }
    *result = entry->value;
}

/* ---------------------------------------------------------------- n^-s at a point */

/**
 * @brief exp(x) at the places: x = r - k ln 2 with 0 < r <= ln 2, then a Taylor series in r.
 *
 * @param[in]  x      The argument times 10^digits [BORROWS].
 * @param[in]  digits The places.
 * @param[out] result The value [BORROWS].
 */
static void exp_of(const OriorExactInteger *x, int32_t digits, OriorExactInteger *result)
{
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger ln_two;
    zeta_ln(2, digits, &ln_two);
    OriorExactInteger negative;
    z_neg(x, &negative);
    OriorExactInteger count;
    z_floor_div(&negative, &ln_two, &count);
    const int64_t k = z_small(&count) + 1;
    OriorExactInteger r;
    z_mul_small(&ln_two, k, &r);
    z_add(x, &r, &r);
    OriorExactInteger total = scale;
    OriorExactInteger term = scale;
    for (int64_t step = 1; term.sign != 0; step += 1)
    {
        z_mul(&term, &r, &term);
        OriorExactInteger under;
        z_mul_small(&scale, step, &under);
        z_floor_div(&term, &under, &term);
        z_add(&total, &term, &total);
    }
    OriorExactInteger up;
    OriorExactInteger down;
    z_two_to((k < 0) ? -k : 0, &up);
    z_two_to((k > 0) ? k : 0, &down);
    z_mul(&total, &up, &total);
    z_floor_div(&total, &down, result);
}

void zeta_cos_sin(const OriorExactInteger *y, int32_t digits, OriorExactInteger *cosine, OriorExactInteger *sine)
{
    static const int cos_sign[4] = {1, 0, -1, 0};
    static const int sin_sign[4] = {0, 1, 0, -1};
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger half_turn;
    zeta_pi(digits, &half_turn);
    OriorExactInteger turn;
    z_mul_small(&half_turn, 2, &turn);
    OriorExactInteger angle;
    z_mod(y, &turn, &angle);
    if (z_cmp(&angle, &half_turn) >= 0)
    {
        z_sub(&angle, &turn, &angle);
    }
    orior_exact_zero(cosine);
    orior_exact_zero(sine);
    OriorExactInteger term = scale;
    for (int64_t step = 0; term.sign != 0;)
    {
        if (cos_sign[step % 4] > 0)
        {
            z_add(cosine, &term, cosine);
        }
        else if (cos_sign[step % 4] < 0)
        {
            z_sub(cosine, &term, cosine);
        }
        if (sin_sign[step % 4] > 0)
        {
            z_add(sine, &term, sine);
        }
        else if (sin_sign[step % 4] < 0)
        {
            z_sub(sine, &term, sine);
        }
        step += 1;
        z_mul(&term, &angle, &term);
        OriorExactInteger under;
        z_mul_small(&scale, step, &under);
        z_floor_div(&term, &under, &term);
    }
}

/**
 * @brief n^-s = exp(-sigma ln n) (cos(t ln n) - i sin(t ln n)) at the places.
 *
 * @note The values include the derivative of n^-s in s, -ln n n^-s.
 *
 * @param[in]  n      The base.
 * @param[in]  point  The point s [BORROWS].
 * @param[in]  digits The places.
 * @param[out] out    Re, Im, Re', Im' [BORROWS].
 */
static void power_minus_s(int64_t n, const ZetaPoint *point, int32_t digits, OriorExactInteger out[4])
{
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger log;
    zeta_ln(n, digits, &log);
    OriorExactInteger product;
    z_mul(&point->sigma.numerator, &log, &product);
    z_neg(&product, &product);
    OriorExactInteger under;
    z_ten_to(point->sigma.places, &under);
    z_floor_div(&product, &under, &product);
    OriorExactInteger size;
    exp_of(&product, digits, &size);
    z_mul(&point->t.numerator, &log, &product);
    z_ten_to(point->t.places, &under);
    z_floor_div(&product, &under, &product);
    OriorExactInteger cosine;
    OriorExactInteger sine;
    zeta_cos_sin(&product, digits, &cosine, &sine);
    z_mul(&size, &cosine, &out[0]);
    z_floor_div(&out[0], &scale, &out[0]);
    z_mul(&size, &sine, &out[1]);
    z_floor_div(&out[1], &scale, &out[1]);
    z_neg(&out[1], &out[1]);
    z_mul(&log, &out[0], &out[2]);
    z_neg(&out[2], &out[2]);
    z_floor_div(&out[2], &scale, &out[2]);
    z_mul(&log, &out[1], &out[3]);
    z_neg(&out[3], &out[3]);
    z_floor_div(&out[3], &scale, &out[3]);
}

/* ---------------------------------------------------------------- Euler-Maclaurin, every coefficient an exact rational
 */

/** @brief The Bernoulli numbers found so far, B_0 first. */
static ZetaRational *bernoulli_values = NULL;

/** @brief How many Bernoulli numbers the table holds. */
static size_t bernoulli_count = 0u;

/**
 * @brief C(row, column), exactly.
 *
 * @param[in]  row    The row.
 * @param[in]  column The column.
 * @param[out] result The binomial coefficient [BORROWS].
 */
static void binomial_exact(int64_t row, int64_t column, OriorExactInteger *result)
{
    z_set(result, 1);
    for (int64_t index = 0; index < column; index += 1)
    {
        z_mul_small(result, row - index, result);
        OriorExactInteger under;
        z_set(&under, index + 1);
        z_div_exact(result, &under, result);
    }
}

void zeta_bernoulli(int64_t k, ZetaRational *result)
{
    if (bernoulli_values == NULL)
    {
        bernoulli_values = (ZetaRational *)zeta_memory(malloc(sizeof(ZetaRational)));
        z_set(&bernoulli_values[0].top, 1);
        z_set(&bernoulli_values[0].under, 1);
        bernoulli_count = 1u;
    }
    while (bernoulli_count <= (size_t)(2 * k))
    {
        const int64_t n = (int64_t)bernoulli_count;
        OriorExactInteger top;
        OriorExactInteger under;
        z_set(&top, 0);
        z_set(&under, 1);
        // C(n + 1, j), advanced one step per j: C(n + 1, j + 1) = C(n + 1, j) (n + 1 - j) / (j + 1).
        OriorExactInteger choose;
        binomial_exact(n + 1, 0, &choose);
        for (int64_t j = 0; j < n; j += 1)
        {
            const ZetaRational *const term = &bernoulli_values[j];
            OriorExactInteger first;
            OriorExactInteger second;
            z_mul(&top, &term->under, &first);
            z_mul(&choose, &term->top, &second);
            z_mul(&second, &under, &second);
            z_add(&first, &second, &top);
            z_mul(&under, &term->under, &under);
            OriorExactInteger divisor;
            z_gcd(&top, &under, &divisor);
            z_div_exact(&top, &divisor, &top);
            z_div_exact(&under, &divisor, &under);
            z_mul_small(&choose, (n + 1) - j, &choose);
            OriorExactInteger step;
            z_set(&step, j + 1);
            z_div_exact(&choose, &step, &choose);
        }
        OriorExactInteger wide;
        z_mul_small(&under, n + 1, &wide);
        OriorExactInteger divisor;
        z_gcd(&top, &wide, &divisor);
        bernoulli_values =
            (ZetaRational *)zeta_memory(realloc(bernoulli_values, (bernoulli_count + 1u) * sizeof(ZetaRational)));
        z_neg(&top, &top);
        z_floor_div(&top, &divisor, &bernoulli_values[bernoulli_count].top);
        z_floor_div(&wide, &divisor, &bernoulli_values[bernoulli_count].under);
        bernoulli_count += 1u;
    }
    const ZetaRational *const value = &bernoulli_values[2 * k];
    OriorExactInteger factorial;
    z_set(&factorial, 1);
    for (int64_t j = 2; j <= (2 * k); j += 1)
    {
        z_mul_small(&factorial, j, &factorial);
    }
    OriorExactInteger wide;
    z_mul(&value->under, &factorial, &wide);
    OriorExactInteger divisor;
    z_gcd(&value->top, &wide, &divisor);
    z_floor_div(&value->top, &divisor, &result->top);
    z_floor_div(&wide, &divisor, &result->under);
}

/**
 * @brief The sum of two exact complex rationals (re, im, den), reduced by the gcd of all three.
 *
 * @param[in]  left   First addend [BORROWS].
 * @param[in]  right  Second addend [BORROWS].
 * @param[out] result The sum [BORROWS].
 */
static void complex_add(const OriorExactInteger left[3], const OriorExactInteger right[3], OriorExactInteger result[3])
{
    OriorExactInteger re;
    OriorExactInteger im;
    OriorExactInteger den;
    OriorExactInteger product;
    z_mul(&left[0], &right[2], &re);
    z_mul(&right[0], &left[2], &product);
    z_add(&re, &product, &re);
    z_mul(&left[1], &right[2], &im);
    z_mul(&right[1], &left[2], &product);
    z_add(&im, &product, &im);
    z_mul(&left[2], &right[2], &den);
    OriorExactInteger divisor;
    z_gcd(&re, &im, &divisor);
    z_gcd(&divisor, &den, &divisor);
    z_floor_div(&re, &divisor, &result[0]);
    z_floor_div(&im, &divisor, &result[1]);
    z_floor_div(&den, &divisor, &result[2]);
}

/**
 * @brief C and its derivative C' in s, each an exact (re, im, den), where C N^-s is the tail.
 *
 * @param[in]  point    The point [BORROWS].
 * @param[in]  n_sum    The cut N.
 * @param[in]  integral 1 to keep N/(s-1), 0 to drop it.
 * @param[out] total    C [BORROWS].
 * @param[out] slope    C' [BORROWS].
 * @note C is N/(s-1), gated by the integral, plus 1/2, plus the sum of B_2k/(2k)! s(s+1)...(s+2k-2) N^(1-2k) for
 *       k = 1 .. N. The product s(s+1)...(s+2k-2) and its derivative advance together.
 */
static void tail_coefficient(const ZetaPoint *point, int64_t n_sum, int integral, OriorExactInteger total[3],
                             OriorExactInteger slope[3])
{
    OriorExactInteger xs;
    OriorExactInteger xt;
    const int32_t places = pair_common(&point->sigma, &point->t, &xs, &xt);
    OriorExactInteger unit;
    z_ten_to(places, &unit);
    OriorExactInteger shifted;
    z_sub(&xs, &unit, &shifted);
    OriorExactInteger size;
    OriorExactInteger product;
    z_mul(&shifted, &shifted, &size);
    z_mul(&xt, &xt, &product);
    z_add(&size, &product, &size);

    OriorExactInteger gated;
    z_mul_small(&unit, (int64_t)integral * n_sum, &gated);
    OriorExactInteger half[3];
    z_set(&half[0], 1);
    z_set(&half[1], 0);
    z_set(&half[2], 2);
    OriorExactInteger integral_term[3];
    z_mul(&gated, &shifted, &integral_term[0]);
    z_mul(&gated, &xt, &integral_term[1]);
    z_neg(&integral_term[1], &integral_term[1]);
    integral_term[2] = size;
    complex_add(half, integral_term, total);

    OriorExactInteger gated_square;
    z_mul(&gated, &unit, &gated_square);
    OriorExactInteger difference;
    z_mul(&shifted, &shifted, &difference);
    z_mul(&xt, &xt, &product);
    z_sub(&difference, &product, &difference);
    z_mul(&gated_square, &difference, &slope[0]);
    z_neg(&slope[0], &slope[0]);
    z_mul(&gated_square, &shifted, &slope[1]);
    z_mul(&slope[1], &xt, &slope[1]);
    z_mul_small(&slope[1], 2, &slope[1]);
    z_mul(&size, &size, &slope[2]);

    OriorExactInteger rising_re = xs;
    OriorExactInteger rising_im = xt;
    OriorExactInteger rising_den = unit;
    OriorExactInteger turning_re = unit;
    OriorExactInteger turning_im;
    z_set(&turning_im, 0);
    OriorExactInteger power_of_n;
    z_set(&power_of_n, n_sum);
    OriorExactInteger n_value;
    z_set(&n_value, n_sum);
    OriorExactInteger n_square;
    z_set(&n_square, n_sum * n_sum);
    for (int64_t k = 1; k <= n_sum; k += 1)
    {
        ZetaRational bernoulli;
        zeta_bernoulli(k, &bernoulli);
        OriorExactInteger den;
        z_mul(&bernoulli.under, &rising_den, &den);
        z_mul(&den, &power_of_n, &den);
        OriorExactInteger term[3];
        z_mul(&bernoulli.top, &rising_re, &term[0]);
        z_mul(&bernoulli.top, &rising_im, &term[1]);
        term[2] = den;
        OriorExactInteger sum[3];
        complex_add(total, term, sum);
        memcpy(total, sum, sizeof(sum));
        z_mul(&bernoulli.top, &turning_re, &term[0]);
        z_mul(&bernoulli.top, &turning_im, &term[1]);
        complex_add(slope, term, sum);
        memcpy(slope, sum, sizeof(sum));
        for (int64_t j = (2 * k) - 1; j <= (2 * k); j += 1)
        {
            OriorExactInteger re;
            z_mul_small(&unit, j, &re);
            z_add(&xs, &re, &re);
            OriorExactInteger next_re;
            OriorExactInteger next_im;
            OriorExactInteger part;
            z_mul(&turning_re, &re, &next_re);
            z_mul(&turning_im, &xt, &part);
            z_sub(&next_re, &part, &next_re);
            z_mul(&rising_re, &unit, &part);
            z_add(&next_re, &part, &next_re);
            z_mul(&turning_re, &xt, &next_im);
            z_mul(&turning_im, &re, &part);
            z_add(&next_im, &part, &next_im);
            z_mul(&rising_im, &unit, &part);
            z_add(&next_im, &part, &next_im);
            turning_re = next_re;
            turning_im = next_im;
            z_mul(&rising_re, &re, &next_re);
            z_mul(&rising_im, &xt, &part);
            z_sub(&next_re, &part, &next_re);
            z_mul(&rising_re, &xt, &next_im);
            z_mul(&rising_im, &re, &part);
            z_add(&next_im, &part, &next_im);
            rising_re = next_re;
            rising_im = next_im;
            z_mul(&rising_den, &unit, &rising_den);
        }
        z_mul(&power_of_n, &n_square, &power_of_n);
    }
    (void)n_value;
}

/**
 * @brief (a + i b) times the exact complex rational c = (re, im, den), each part cut toward minus infinity.
 *
 * @param[in]  a      The real part [BORROWS].
 * @param[in]  b      The imaginary part [BORROWS].
 * @param[in]  c      The rational [BORROWS].
 * @param[out] result The real and imaginary parts [BORROWS].
 */
static void times_rational(const OriorExactInteger *a, const OriorExactInteger *b, const OriorExactInteger c[3],
                           OriorExactInteger result[2])
{
    OriorExactInteger first;
    OriorExactInteger second;
    z_mul(a, &c[0], &first);
    z_mul(b, &c[1], &second);
    z_sub(&first, &second, &first);
    z_floor_div(&first, &c[2], &result[0]);
    z_mul(a, &c[1], &first);
    z_mul(b, &c[0], &second);
    z_add(&first, &second, &first);
    z_floor_div(&first, &c[2], &result[1]);
}

/**
 * @brief zeta(s) and zeta'(s) by Euler-Maclaurin cut at N. The entry powers[n - 1] holds n^-s and its derivative.
 *
 * @param[in]  point    The point [BORROWS].
 * @param[in]  n_sum    The cut N.
 * @param[in]  integral 1 to keep the integral of the rest, 0 to drop it.
 * @param[in]  powers   The powers, four values each [BORROWS].
 * @param[out] result   Re, Im, Re', Im' [BORROWS].
 */
static void zeta_at(const ZetaPoint *point, int64_t n_sum, int integral, const OriorExactInteger (*powers)[4],
                    OriorExactInteger result[4])
{
    for (unsigned part = 0u; part < 4u; part += 1u)
    {
        orior_exact_zero(&result[part]);
        for (int64_t n = 0; n < (n_sum - 1); n += 1)
        {
            z_add(&result[part], &powers[n][part], &result[part]);
        }
    }
    const OriorExactInteger *const last = powers[n_sum - 1];
    OriorExactInteger total[3];
    OriorExactInteger slope[3];
    tail_coefficient(point, n_sum, integral, total, slope);
    OriorExactInteger value[2];
    OriorExactInteger first_slope[2];
    OriorExactInteger second_slope[2];
    times_rational(&last[0], &last[1], total, value);
    times_rational(&last[0], &last[1], slope, first_slope);
    times_rational(&last[2], &last[3], total, second_slope);
    z_add(&result[0], &value[0], &result[0]);
    z_add(&result[1], &value[1], &result[1]);
    z_add(&result[2], &first_slope[0], &result[2]);
    z_add(&result[2], &second_slope[0], &result[2]);
    z_add(&result[3], &first_slope[1], &result[3]);
    z_add(&result[3], &second_slope[1], &result[3]);
}

void zeta_routes(const ZetaPoint *point, int32_t places, int64_t n_sum, int integral, OriorExactInteger values[8])
{
    const int32_t digits = places + ZETA_GUARD;
    // The count of powers is 2N, which fits size_t.
    const size_t count = (size_t)(2 * n_sum);
    OriorExactInteger(*const powers)[4] =
        (OriorExactInteger(*)[4])zeta_memory(malloc(count * sizeof(OriorExactInteger[4])));
    for (int64_t n = 1; n <= (2 * n_sum); n += 1)
    {
        power_minus_s(n, point, digits, powers[n - 1]);
    }
    zeta_at(point, n_sum, integral, (const OriorExactInteger(*)[4])powers, &values[0]);
    zeta_at(point, 2 * n_sum, integral, (const OriorExactInteger(*)[4])powers, &values[4]);
    free(powers);
    OriorExactInteger drop;
    z_ten_to(ZETA_GUARD, &drop);
    for (unsigned at = 0u; at < 8u; at += 1u)
    {
        const int32_t sign = values[at].sign;
        OriorExactInteger size;
        z_abs(&values[at], &size);
        z_floor_div(&size, &drop, &values[at]);
        if (sign < 0)
        {
            values[at].sign = -values[at].sign;
        }
    }
}

void zeta_gap(const ZetaPoint *point, int32_t places, int64_t n_sum, int integral, OriorExactInteger *gap,
              OriorExactInteger value[2])
{
    OriorExactInteger values[8];
    zeta_routes(point, places, n_sum, integral, values);
    OriorExactInteger part;
    z_sub(&values[0], &values[4], &part);
    z_abs(&part, gap);
    z_sub(&values[1], &values[5], &part);
    z_abs(&part, &part);
    z_add(gap, &part, gap);
    value[0] = values[0];
    value[1] = values[1];
}

int64_t zeta_agreed_value(const ZetaPoint *point, int32_t places, OriorExactInteger value[2],
                          OriorExactInteger gaps[64], size_t *count)
{
    int64_t n_sum = 1;
    *count = 0u;
    for (;;)
    {
        if (*count >= 64u)
        {
            zeta_stop("the two routes do not meet by N = 2^63");
        }
        zeta_gap(point, places, n_sum, 1, &gaps[*count], value);
        *count += 1u;
        if (gaps[*count - 1u].sign == 0)
        {
            return n_sum;
        }
        n_sum *= 2;
    }
}

/* ---------------------------------------------------------------- exact rationals */

void rational_make(const OriorExactInteger *top, const OriorExactInteger *under, ZetaRational *result)
{
    if (under->sign == 0)
    {
        zeta_stop("a rational with a zero denominator");
    }
    OriorExactInteger numerator = *top;
    OriorExactInteger denominator = *under;
    if (denominator.sign < 0)
    {
        numerator.sign = -numerator.sign;
        denominator.sign = 1;
    }
    OriorExactInteger divisor;
    z_gcd(&numerator, &denominator, &divisor);
    if (divisor.sign == 0)
    {
        z_set(&divisor, 1);
    }
    z_div_exact(&numerator, &divisor, &result->top);
    z_div_exact(&denominator, &divisor, &result->under);
}

void rational_small(int64_t top, int64_t under, ZetaRational *result)
{
    OriorExactInteger numerator;
    OriorExactInteger denominator;
    z_set(&numerator, top);
    z_set(&denominator, under);
    rational_make(&numerator, &denominator, result);
}

void rational_add(const ZetaRational *left, const ZetaRational *right, ZetaRational *result)
{
    OriorExactInteger first;
    OriorExactInteger second;
    OriorExactInteger under;
    z_mul(&left->top, &right->under, &first);
    z_mul(&right->top, &left->under, &second);
    z_add(&first, &second, &first);
    z_mul(&left->under, &right->under, &under);
    rational_make(&first, &under, result);
}

void rational_sub(const ZetaRational *left, const ZetaRational *right, ZetaRational *result)
{
    ZetaRational negative = *right;
    negative.top.sign = -negative.top.sign;
    rational_add(left, &negative, result);
}

void rational_mul(const ZetaRational *left, const ZetaRational *right, ZetaRational *result)
{
    OriorExactInteger top;
    OriorExactInteger under;
    z_mul(&left->top, &right->top, &top);
    z_mul(&left->under, &right->under, &under);
    rational_make(&top, &under, result);
}

void rational_div(const ZetaRational *left, const ZetaRational *right, ZetaRational *result)
{
    OriorExactInteger top;
    OriorExactInteger under;
    z_mul(&left->top, &right->under, &top);
    z_mul(&left->under, &right->top, &under);
    rational_make(&top, &under, result);
}

int rational_cmp(const ZetaRational *left, const ZetaRational *right)
{
    OriorExactInteger first;
    OriorExactInteger second;
    z_mul(&left->top, &right->under, &first);
    z_mul(&right->top, &left->under, &second);
    return z_cmp(&first, &second);
}

double rational_to_double(const ZetaRational *value)
{
    if (value->top.sign == 0)
    {
        return 0.0;
    }
    OriorExactInteger size;
    z_abs(&value->top, &size);
    // A quotient of 55 bits: 54 for the double and its rounding bit, and one more. The rest decides a tie.
    const int64_t shift = 55 - (z_bit_length(&size) - z_bit_length(&value->under));
    OriorExactInteger top = size;
    OriorExactInteger under = value->under;
    if (shift >= 0)
    {
        OriorExactInteger power;
        z_two_to(shift, &power);
        z_mul(&top, &power, &top);
    }
    else
    {
        OriorExactInteger power;
        z_two_to(-shift, &power);
        z_mul(&under, &power, &under);
    }
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    zeta_require(orior_exact_divide(&top, &under, &quotient, &remainder), "division");
    int64_t mantissa = z_small(&quotient);
    int64_t exponent = -shift;
    // The quotient has 55 or 56 bits. Bring it to 54: 53 bits and the rounding bit.
    int sticky = remainder.sign != 0;
    while (mantissa >= ((int64_t)1 << 54))
    {
        sticky = sticky || ((mantissa & 1) != 0);
        mantissa >>= 1;
        exponent += 1;
    }
    const int round_bit = (int)(mantissa & 1);
    mantissa >>= 1;
    exponent += 1;
    if (round_bit && (sticky || ((mantissa & 1) != 0)))
    {
        mantissa += 1;
    }
    // The exponent of a value in this tree fits an int.
    const double magnitude = ldexp((double)mantissa, (int)exponent);
    return (value->top.sign < 0) ? -magnitude : magnitude;
}

void rational_text(const ZetaRational *value, char *text, size_t capacity)
{
    static char top_text[8192];
    static char under_text[8192];
    z_text(&value->top, top_text, sizeof(top_text));
    OriorExactInteger one;
    z_set(&one, 1);
    if (orior_exact_equal(&value->under, &one))
    {
        snprintf(text, capacity, "%s", top_text);
        return;
    }
    z_text(&value->under, under_text, sizeof(under_text));
    snprintf(text, capacity, "%s/%s", top_text, under_text);
}
