// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_exact.h
 * @brief The exact arithmetic of the zeta examples: integers, decimal pairs, the natural constants, and zeta.
 *
 * @note Every integer is an OriorExactInteger of includes/exact_types. The division here is floor division, and
 *       the remainder takes the sign of the divisor, as the integer arithmetic of Python defines them. The
 *       examples state their rules in that arithmetic, and their reports depend on it to the last digit.
 * @note An operation whose value does not fit the width stops the program with a message. The program prints no
 *       wrong value.
 * @note A decimal pair is a numerator at a count of decimal places, with the zeros at the end of the numerator
 *       dropped. Thus one value has one pair.
 * @note Each natural constant comes from two routes at places + ZETA_GUARD, with the guard dropped. The two
 *       routes must agree, or the program stops.
 */
#ifndef ZETA_EXACT_H
#define ZETA_EXACT_H

#include "../../../includes/exact_types/integers/exact_integer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/** @brief The guard places of each natural constant, dropped after the two routes agree. */
#define ZETA_GUARD 20

/** @brief The most decimal places that a value may carry. A request past it stops the program. */
#define ZETA_SCALE_DIGITS 1024

/** @brief A decimal value: numerator / 10^places, with the zeros at the end of the numerator dropped. */
typedef struct
{
    OriorExactInteger numerator; /**< The digits. */
    int32_t places;              /**< The decimal places. */
} DecimalPair;

/** @brief A point of the complex plane, sigma + i t, each coordinate a decimal pair. */
typedef struct
{
    DecimalPair sigma; /**< The real part. */
    DecimalPair t;     /**< The imaginary part. */
} ZetaPoint;

/** @brief The table of zeta zeros of Odlyzko, zeros1: the first forty ordinates as printed there. */
extern const char *const ZETA_PUBLISHED_ZEROS[40];

/* ---------------------------------------------------------------- integers */

/**
 * @brief Stops the program where an exact operation fails.
 *
 * @param[in] status The status.
 * @param[in] what   The name of the operation [BORROWS].
 */
void zeta_require(OriorExactStatus status, const char *what);

/**
 * @brief Stops the program with a message.
 *
 * @param[in] message The message [BORROWS].
 */
void zeta_stop(const char *message);

/**
 * @brief Sets an integer to the value of a machine integer.
 *
 * @param[out] value  The integer [BORROWS].
 * @param[in]  number The value.
 */
void z_set(OriorExactInteger *value, int64_t number);

/**
 * @brief The value of an integer that fits a machine integer. A value that does not fit stops the program.
 *
 * @param[in] value The integer [BORROWS].
 * @return          The value.
 */
int64_t z_small(const OriorExactInteger *value);

/**
 * @brief The sum of two integers.
 *
 * @param[in]  left   First addend [BORROWS].
 * @param[in]  right  Second addend [BORROWS].
 * @param[out] result The sum [BORROWS].
 */
void z_add(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result);

/**
 * @brief The difference of two integers.
 *
 * @param[in]  left   The minuend [BORROWS].
 * @param[in]  right  The subtrahend [BORROWS].
 * @param[out] result The difference [BORROWS].
 */
void z_sub(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result);

/**
 * @brief The product of two integers.
 *
 * @param[in]  left   First factor [BORROWS].
 * @param[in]  right  Second factor [BORROWS].
 * @param[out] result The product [BORROWS].
 */
void z_mul(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result);

/**
 * @brief The product of an integer and a machine integer.
 *
 * @param[in]  value  The integer [BORROWS].
 * @param[in]  factor The machine integer.
 * @param[out] result The product [BORROWS].
 */
void z_mul_small(const OriorExactInteger *value, int64_t factor, OriorExactInteger *result);

/**
 * @brief Floor division: the quotient rounded toward minus infinity.
 *
 * @param[in]  numerator The dividend [BORROWS].
 * @param[in]  divisor   The divisor, not zero [BORROWS].
 * @param[out] quotient  The quotient [BORROWS].
 */
void z_floor_div(const OriorExactInteger *numerator, const OriorExactInteger *divisor, OriorExactInteger *quotient);

/**
 * @brief The remainder of floor division, with the sign of the divisor.
 *
 * @param[in]  numerator The dividend [BORROWS].
 * @param[in]  divisor   The divisor, not zero [BORROWS].
 * @param[out] remainder The remainder [BORROWS].
 */
void z_mod(const OriorExactInteger *numerator, const OriorExactInteger *divisor, OriorExactInteger *remainder);

/**
 * @brief Orders two integers.
 *
 * @param[in] left  First integer [BORROWS].
 * @param[in] right Second integer [BORROWS].
 * @return          -1, 0 or 1.
 */
int z_cmp(const OriorExactInteger *left, const OriorExactInteger *right);

/**
 * @brief The magnitude of an integer.
 *
 * @param[in]  value  The integer [BORROWS].
 * @param[out] result The magnitude [BORROWS].
 */
void z_abs(const OriorExactInteger *value, OriorExactInteger *result);

/**
 * @brief The negative of an integer.
 *
 * @param[in]  value  The integer [BORROWS].
 * @param[out] result The negative [BORROWS].
 */
void z_neg(const OriorExactInteger *value, OriorExactInteger *result);

/**
 * @brief Ten raised to a power.
 *
 * @param[in]  power  The power, at least zero.
 * @param[out] result The value [BORROWS].
 */
void z_ten_to(int32_t power, OriorExactInteger *result);

/**
 * @brief Two raised to a power.
 *
 * @param[in]  power  The power, at least zero.
 * @param[out] result The value [BORROWS].
 */
void z_two_to(int64_t power, OriorExactInteger *result);

/**
 * @brief The count of bits of the magnitude of an integer. Zero has none.
 *
 * @param[in] value The integer [BORROWS].
 * @return          The count.
 */
int64_t z_bit_length(const OriorExactInteger *value);

/**
 * @brief The greatest common divisor of the magnitudes of two integers.
 *
 * @param[in]  left   First integer [BORROWS].
 * @param[in]  right  Second integer [BORROWS].
 * @param[out] result The divisor [BORROWS].
 */
void z_gcd(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result);

/**
 * @brief The quotient of a division with no remainder.
 *
 * @param[in]  numerator The dividend [BORROWS].
 * @param[in]  divisor   The divisor [BORROWS].
 * @param[out] quotient  The quotient [BORROWS].
 */
void z_div_exact(const OriorExactInteger *numerator, const OriorExactInteger *divisor, OriorExactInteger *quotient);

/**
 * @brief floor(sqrt(value)) for an integer of at least zero, by integer Newton steps.
 *
 * @param[in]  value  The integer [BORROWS].
 * @param[out] result The root [BORROWS].
 */
void z_isqrt(const OriorExactInteger *value, OriorExactInteger *result);

/**
 * @brief Writes the decimal text of an integer to a stream.
 *
 * @param[in] value The integer [BORROWS].
 * @param[in] out   The stream [BORROWS].
 */
void z_print(const OriorExactInteger *value, FILE *out);

/**
 * @brief Writes the decimal text of an integer into a buffer.
 *
 * @param[in]  value    The integer [BORROWS].
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
void z_text(const OriorExactInteger *value, char *text, size_t capacity);

/* ---------------------------------------------------------------- decimal pairs */

/**
 * @brief The pair of a numerator at places, with the zeros at the end dropped.
 *
 * @param[in]  numerator The numerator [BORROWS].
 * @param[in]  places    The places.
 * @param[out] pair      The pair [BORROWS].
 */
void pair_make(const OriorExactInteger *numerator, int32_t places, DecimalPair *pair);

/**
 * @brief The pair of a machine integer at places.
 *
 * @param[in]  numerator The numerator.
 * @param[in]  places    The places.
 * @param[out] pair      The pair [BORROWS].
 */
void pair_small(int64_t numerator, int32_t places, DecimalPair *pair);

/**
 * @brief Two pairs brought to one count of places.
 *
 * @param[in]  left        First pair [BORROWS].
 * @param[in]  right       Second pair [BORROWS].
 * @param[out] left_value  The numerator of the first at the common places [BORROWS].
 * @param[out] right_value The numerator of the second at the common places [BORROWS].
 * @return                 The common places.
 */
int32_t pair_common(const DecimalPair *left, const DecimalPair *right, OriorExactInteger *left_value,
                    OriorExactInteger *right_value);

/**
 * @brief The sum of two pairs.
 *
 * @param[in]  left   First addend [BORROWS].
 * @param[in]  right  Second addend [BORROWS].
 * @param[out] result The sum [BORROWS].
 */
void pair_plus(const DecimalPair *left, const DecimalPair *right, DecimalPair *result);

/**
 * @brief The difference of two pairs.
 *
 * @param[in]  left   The minuend [BORROWS].
 * @param[in]  right  The subtrahend [BORROWS].
 * @param[out] result The difference [BORROWS].
 */
void pair_minus(const DecimalPair *left, const DecimalPair *right, DecimalPair *result);

/**
 * @brief Half a pair.
 *
 * @param[in]  value  The pair [BORROWS].
 * @param[out] result The half [BORROWS].
 */
void pair_half(const DecimalPair *value, DecimalPair *result);

/**
 * @brief Twice a pair.
 *
 * @param[in]  value  The pair [BORROWS].
 * @param[out] result Twice it [BORROWS].
 */
void pair_twice(const DecimalPair *value, DecimalPair *result);

/**
 * @brief The midpoint of two pairs.
 *
 * @param[in]  left   First pair [BORROWS].
 * @param[in]  right  Second pair [BORROWS].
 * @param[out] result The midpoint [BORROWS].
 */
void pair_middle(const DecimalPair *left, const DecimalPair *right, DecimalPair *result);

/**
 * @brief Whether the first pair is below the second.
 *
 * @param[in] left  First pair [BORROWS].
 * @param[in] right Second pair [BORROWS].
 * @return          1 where left < right, else 0.
 */
int pair_below(const DecimalPair *left, const DecimalPair *right);

/**
 * @brief Whether two pairs are equal.
 *
 * @param[in] left  First pair [BORROWS].
 * @param[in] right Second pair [BORROWS].
 * @return          1 where they are equal.
 */
int pair_equal(const DecimalPair *left, const DecimalPair *right);

/**
 * @brief Reads plain decimal text into a pair, with the zeros at the end dropped.
 *
 * @param[in]  text The text [BORROWS].
 * @param[out] pair The pair [BORROWS].
 */
void pair_units(const char *text, DecimalPair *pair);

/**
 * @brief Writes a pair at a count of places, cut toward minus infinity, as whole.part, into a buffer.
 *
 * @param[in]  value    The pair [BORROWS].
 * @param[in]  places   The places to show.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
void pair_text(const DecimalPair *value, int32_t places, char *text, size_t capacity);

/**
 * @brief Writes a pair at a count of places, cut toward minus infinity, as whole.part.
 *
 * @param[in] value  The pair [BORROWS].
 * @param[in] places The places to show.
 * @param[in] out    The stream [BORROWS].
 */
void pair_print(const DecimalPair *value, int32_t places, FILE *out);

/* ---------------------------------------------------------------- the natural constants */

/**
 * @brief floor(pi * 10^places), by the identities of Machin and Euler, which must agree.
 *
 * @param[in]  places The places.
 * @param[out] result The value [BORROWS].
 */
void zeta_pi(int32_t places, OriorExactInteger *result);

/**
 * @brief ln n * 10^places, by two series that must agree: k ln 2 plus an artanh, and j ln 3 plus another.
 *
 * @param[in]  n      The argument, at least one.
 * @param[in]  places The places.
 * @param[out] result The value [BORROWS].
 */
void zeta_ln(int64_t n, int32_t places, OriorExactInteger *result);

/**
 * @brief The cosine and the sine of y, each times 10^digits, with y reduced to [-pi, pi) by whole turns.
 *
 * @param[in]  y      The angle times 10^digits [BORROWS].
 * @param[in]  digits The places.
 * @param[out] cosine The cosine [BORROWS].
 * @param[out] sine   The sine [BORROWS].
 */
void zeta_cos_sin(const OriorExactInteger *y, int32_t digits, OriorExactInteger *cosine, OriorExactInteger *sine);

/** @brief A reduced rational number. */
typedef struct
{
    OriorExactInteger top;   /**< The numerator. */
    OriorExactInteger under; /**< The denominator. */
} ZetaRational;

/**
 * @brief B_2k / (2k)! as a reduced pair, the Bernoulli numbers by the standard recurrence.
 *
 * @param[in]  k      The half index.
 * @param[out] result The value [BORROWS].
 */
void zeta_bernoulli(int64_t k, ZetaRational *result);

/**
 * @brief floor(scale * arctan(1 / denominator)), by the series of terms with alternate signs, in integers.
 *
 * @param[in]  denominator The reciprocal of the argument.
 * @param[in]  scale       The scale [BORROWS].
 * @param[out] result      The value [BORROWS].
 */
void zeta_arctan_inverse(int64_t denominator, const OriorExactInteger *scale, OriorExactInteger *result);

/**
 * @brief pi by the identity of Machin, pi = 16 arctan(1/5) - 4 arctan(1/239), times the scale.
 *
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The value [BORROWS].
 */
void zeta_pi_machin(const OriorExactInteger *scale, OriorExactInteger *result);

/**
 * @brief pi by the identity of Euler, pi = 4 (arctan(1/2) + arctan(1/3)), times the scale.
 *
 * @param[in]  scale  The scale [BORROWS].
 * @param[out] result The value [BORROWS].
 */
void zeta_pi_euler(const OriorExactInteger *scale, OriorExactInteger *result);

/**
 * @brief ln n * 10^places for an exact integer n of at least one, by the same two series as zeta_ln.
 *
 * @param[in]  n      The argument [BORROWS].
 * @param[in]  places The places.
 * @param[out] result The value [BORROWS].
 */
void zeta_ln_big(const OriorExactInteger *n, int32_t places, OriorExactInteger *result);

/**
 * @brief Whether [low, high] meets the bracket of a published value, half a unit in its last place.
 *
 * @param[in] low       The lower end [BORROWS].
 * @param[in] high      The upper end [BORROWS].
 * @param[in] published The published text [BORROWS].
 * @return              1 where they meet.
 */
int pair_holds(const DecimalPair *low, const DecimalPair *high, const char *published);

/* ---------------------------------------------------------------- zeta */

/**
 * @brief The two routes at a point: N and 2N, each at places + ZETA_GUARD, with the guard dropped.
 *
 * @param[in]  point    The point [BORROWS].
 * @param[in]  places   The places.
 * @param[in]  n_sum    The cut N of the first route.
 * @param[in]  integral 1 to keep the integral of the rest, 0 to drop it.
 * @param[out] values   Eight values: Re, Im, Re', Im' of the first route, then of the second [BORROWS].
 * @note This reads each value toward zero, as its sign times the floor of its size.
 */
void zeta_routes(const ZetaPoint *point, int32_t places, int64_t n_sum, int integral, OriorExactInteger values[8]);

/**
 * @brief How far apart the two routes land at a cut, in units of the last place.
 *
 * @param[in]  point    The point [BORROWS].
 * @param[in]  places   The places.
 * @param[in]  n_sum    The cut.
 * @param[in]  integral 1 to keep the integral of the rest, 0 to drop it.
 * @param[out] gap      The gap [BORROWS].
 * @param[out] value    Re and Im of the first route [BORROWS].
 */
void zeta_gap(const ZetaPoint *point, int32_t places, int64_t n_sum, int integral, OriorExactInteger *gap,
              OriorExactInteger value[2]);

/**
 * @brief Zeta where the two routes agree at the places. N doubles while they do not.
 *
 * @param[in]  point  The point [BORROWS].
 * @param[in]  places The places.
 * @param[out] value  Re and Im [BORROWS].
 * @param[out] gaps   The gap at each N, from N = 1 [BORROWS]. 64 entries.
 * @param[out] count  How many gaps [BORROWS].
 * @return            The cut N at which the routes agree.
 */
int64_t zeta_agreed_value(const ZetaPoint *point, int32_t places, OriorExactInteger value[2],
                          OriorExactInteger gaps[64], size_t *count);

#endif
