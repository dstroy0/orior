// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file torus_fields.c
 * @brief The exact ring of fields on the unit torus: polynomials in pi, ratios, fields and the recurrences.
 *
 * @note torus_fields.h states the ring and its invariants. This file keeps each invariant at each write. A
 *       polynomial holds no zero term, and a field holds no zero mode. Terms and modes are in ascending order.
 *       A denominator is positive, and shares no divisor with the integers of the value.
 */
#include "torus_fields.h"
#include "../../../src/platform/platform.h"

#include "memoria_operor/memoria_operor.h"
#include <stdlib.h>

/* ---------------------------------------------------------------- exact integers */

void ring_require(OriorExactStatus status, const char *what)
{
    if (status != ORIOR_EXACT_OK)
    {
        fprintf(stderr, "  the exact %s failed with status %d. The value does not fit the width.\n", what, (int)status);
        exit(2);
    }
}

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block from malloc or realloc [OWNS].
 * @return          The same block [OWNS].
 */
static void *ring_memory(void *block)
{
    if (block == NULL)
    {
        fprintf(stderr, "  out of memory\n");
        exit(2);
    }
    return block;
}

void big_set(OriorExactInteger *value, int64_t number)
{
    orior_exact_zero(value);
    if (number == 0)
    {
        return;
    }
    // The magnitude of INT64_MIN does not fit int64_t. The unsigned negation holds every magnitude.
    const uint64_t magnitude = (number < 0) ? (0u - (uint64_t)number) : (uint64_t)number;
    // Each limb holds 32 bits of the magnitude. The mask and the shift keep each part in range.
    value->limb[0] = (uint32_t)(magnitude & 0xFFFFFFFFu);
    if (ORIOR_EXACT_LIMBS > 1u)
    {
        value->limb[1] = (uint32_t)(magnitude >> 32u);
    }
    value->sign = (number < 0) ? -1 : 1;
}

/**
 * @brief Whether an exact integer is zero.
 *
 * @param[in] value The integer [BORROWS].
 * @return          1 where it is zero, else 0.
 */
static int big_is_zero(const OriorExactInteger *value)
{
    return value->sign == 0;
}

/**
 * @brief Whether an exact integer is one.
 *
 * @param[in] value The integer [BORROWS].
 * @return          1 where it is one, else 0.
 */
static int big_is_one(const OriorExactInteger *value)
{
    OriorExactInteger one;
    big_set(&one, 1);
    return orior_exact_equal(value, &one);
}

/**
 * @brief The sum of two exact integers.
 *
 * @param[in]  left   First addend [BORROWS].
 * @param[in]  right  Second addend [BORROWS].
 * @param[out] result The sum [BORROWS].
 */
static void big_add(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    ring_require(orior_exact_add(left, right, result), "add");
}

/**
 * @brief The difference of two exact integers.
 *
 * @param[in]  left   The minuend [BORROWS].
 * @param[in]  right  The subtrahend [BORROWS].
 * @param[out] result The difference [BORROWS].
 */
static void big_sub(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    ring_require(orior_exact_subtract(left, right, result), "subtract");
}

/**
 * @brief The product of two exact integers.
 *
 * @param[in]  left   First factor [BORROWS].
 * @param[in]  right  Second factor [BORROWS].
 * @param[out] result The product [BORROWS].
 */
static void big_mul(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    ring_require(orior_exact_multiply(left, right, result), "multiply");
}

/**
 * @brief The quotient of a division that leaves no remainder.
 *
 * @param[in]  numerator The dividend [BORROWS].
 * @param[in]  divisor   The divisor [BORROWS].
 * @param[out] quotient  The quotient [BORROWS].
 */
static void big_div_exact(const OriorExactInteger *numerator, const OriorExactInteger *divisor,
                          OriorExactInteger *quotient)
{
    ring_require(orior_exact_divide_exact(numerator, divisor, quotient), "exact division");
}

/**
 * @brief The greatest common divisor of the magnitudes of two exact integers.
 *
 * @param[in]  left   First integer [BORROWS].
 * @param[in]  right  Second integer [BORROWS].
 * @param[out] result The divisor [BORROWS].
 */
static void big_gcd(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    ring_require(orior_exact_gcd(left, right, result), "gcd");
}

/**
 * @brief The least common multiple of two positive exact integers.
 *
 * @param[in]  left   First integer [BORROWS].
 * @param[in]  right  Second integer [BORROWS].
 * @param[out] result The multiple [BORROWS].
 */
static void big_lcm(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    OriorExactInteger divisor;
    big_gcd(left, right, &divisor);
    OriorExactInteger product;
    big_mul(left, right, &product);
    big_div_exact(&product, &divisor, result);
}

/**
 * @brief The product of an exact integer and a machine integer.
 *
 * @param[in]  value  The exact integer [BORROWS].
 * @param[in]  factor The machine integer.
 * @param[out] result The product [BORROWS].
 */
static void big_mul_small(const OriorExactInteger *value, int64_t factor, OriorExactInteger *result)
{
    OriorExactInteger other;
    big_set(&other, factor);
    big_mul(value, &other, result);
}

size_t big_text(const OriorExactInteger *value, char *text, size_t capacity)
{
    if (big_is_zero(value))
    {
        snprintf(text, capacity, "0");
        return 1u;
    }
    // Chunks of nine decimal digits, least significant first. A width of 32 limbs holds 309 digits, thus
    // 35 chunks. The bound below is generous for every width that this ring uses.
    enum
    {
        RING_TEXT_CHUNKS = 2048
    };
    static uint32_t chunk[RING_TEXT_CHUNKS];
    size_t chunks = 0u;
    OriorExactInteger rest = *value;
    rest.sign = 1;
    OriorExactInteger billion;
    big_set(&billion, 1000000000);
    while (!big_is_zero(&rest))
    {
        OriorExactInteger quotient;
        OriorExactInteger remainder;
        ring_require(orior_exact_divide(&rest, &billion, &quotient, &remainder), "divide");
        if (chunks >= RING_TEXT_CHUNKS)
        {
            fprintf(stderr, "  a number is too long to print\n");
            exit(2);
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
    for (size_t at = chunks - 1u; at > 0u; at -= 1u)
    {
        used += (size_t)snprintf(text + used, capacity - used, "%09u", (unsigned)chunk[at - 1u]);
    }
    return used;
}

/**
 * @brief Writes an exact integer to a stream.
 *
 * @param[in] value The integer [BORROWS].
 * @param[in] out   The stream [BORROWS].
 */
static void big_print(const OriorExactInteger *value, FILE *out)
{
    static char text[20000];
    big_text(value, text, sizeof(text));
    fputs(text, out);
}

/* ---------------------------------------------------------------- polynomials */

/**
 * @brief Makes room for at least a count of terms.
 *
 * @param[in,out] poly  The polynomial [BORROWS].
 * @param[in]     count The count.
 */
static void poly_reserve(PiPoly *poly, size_t count)
{
    if (count <= poly->capacity)
    {
        return;
    }
    const size_t grown = (count > (poly->capacity * 2u)) ? count : (poly->capacity * 2u);
    poly->terms = (PiTerm *)ring_memory(platform_cell_grown(poly->terms, poly->capacity * sizeof(PiTerm),
                                                            grown * sizeof(PiTerm), platform_place_g_program_owner, 0));
    poly->capacity = grown;
}

/**
 * @brief Appends a term above every term present. This stores no zero term.
 *
 * @param[in,out] poly      The polynomial [BORROWS].
 * @param[in]     power     The power of pi, above every power present.
 * @param[in]     real      The real part [BORROWS].
 * @param[in]     imaginary The imaginary part [BORROWS].
 */
static void poly_push(PiPoly *poly, int32_t power, const OriorExactInteger *real, const OriorExactInteger *imaginary)
{
    if (big_is_zero(real) && big_is_zero(imaginary))
    {
        return;
    }
    poly_reserve(poly, poly->count + 1u);
    poly->terms[poly->count].power = power;
    poly->terms[poly->count].real = *real;
    poly->terms[poly->count].imaginary = *imaginary;
    poly->count += 1u;
}

/**
 * @brief The empty polynomial, which is zero.
 *
 * @param[out] poly The polynomial [BORROWS].
 */
static void poly_empty(PiPoly *poly)
{
    poly->terms = NULL;
    poly->count = 0u;
    poly->capacity = 0u;
}

void poly_free(PiPoly *poly)
{
    platform_cell_released(poly->terms, platform_place_g_program_owner);
    poly_empty(poly);
}

void poly_term(PiPoly *poly, int32_t power, int64_t real, int64_t imaginary)
{
    poly_empty(poly);
    OriorExactInteger real_value;
    OriorExactInteger imaginary_value;
    big_set(&real_value, real);
    big_set(&imaginary_value, imaginary);
    poly_push(poly, power, &real_value, &imaginary_value);
}

/**
 * @brief A copy of a polynomial.
 *
 * @param[in]  source The polynomial [BORROWS].
 * @param[out] copy   The copy [BORROWS].
 */
static void poly_copy(const PiPoly *source, PiPoly *copy)
{
    poly_empty(copy);
    poly_reserve(copy, source->count);
    if (source->count > 0u)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = copy->terms, .src = source->terms,
                   .bytes = source->count * sizeof(PiTerm));
    }
    copy->count = source->count;
}

/**
 * @brief The sum of two polynomials.
 *
 * @param[in]  left  First addend [BORROWS].
 * @param[in]  right Second addend [BORROWS].
 * @param[out] sum   The sum [BORROWS].
 */
static void poly_add(const PiPoly *left, const PiPoly *right, PiPoly *sum)
{
    poly_empty(sum);
    poly_reserve(sum, left->count + right->count);
    size_t left_at = 0u;
    size_t right_at = 0u;
    while ((left_at < left->count) || (right_at < right->count))
    {
        if ((right_at >= right->count) ||
            ((left_at < left->count) && (left->terms[left_at].power < right->terms[right_at].power)))
        {
            poly_push(sum, left->terms[left_at].power, &left->terms[left_at].real, &left->terms[left_at].imaginary);
            left_at += 1u;
        }
        else if ((left_at >= left->count) || (right->terms[right_at].power < left->terms[left_at].power))
        {
            poly_push(sum, right->terms[right_at].power, &right->terms[right_at].real,
                      &right->terms[right_at].imaginary);
            right_at += 1u;
        }
        else
        {
            OriorExactInteger real;
            OriorExactInteger imaginary;
            big_add(&left->terms[left_at].real, &right->terms[right_at].real, &real);
            big_add(&left->terms[left_at].imaginary, &right->terms[right_at].imaginary, &imaginary);
            poly_push(sum, left->terms[left_at].power, &real, &imaginary);
            left_at += 1u;
            right_at += 1u;
        }
    }
}

void poly_mul(const PiPoly *left, const PiPoly *right, PiPoly *product)
{
    poly_empty(product);
    if ((left->count == 0u) || (right->count == 0u))
    {
        return;
    }
    const int32_t lowest = left->terms[0].power + right->terms[0].power;
    const int32_t highest = left->terms[left->count - 1u].power + right->terms[right->count - 1u].power;
    // The span is at least one, because highest is never below lowest.
    const size_t span = (size_t)(highest - lowest) + 1u;
    OriorExactInteger *const real = (OriorExactInteger *)ring_memory(
        platform_cell_cleared(span, sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    OriorExactInteger *const imaginary = (OriorExactInteger *)ring_memory(
        platform_cell_cleared(span, sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    for (size_t left_at = 0u; left_at < left->count; left_at += 1u)
    {
        const PiTerm *const one = &left->terms[left_at];
        for (size_t right_at = 0u; right_at < right->count; right_at += 1u)
        {
            const PiTerm *const other = &right->terms[right_at];
            // The offset is in range, because each power lies between the lowest and the highest sum.
            const size_t at = (size_t)((one->power + other->power) - lowest);
            OriorExactInteger first;
            OriorExactInteger second;
            big_mul(&one->real, &other->real, &first);
            big_mul(&one->imaginary, &other->imaginary, &second);
            big_sub(&first, &second, &first);
            big_add(&real[at], &first, &real[at]);
            big_mul(&one->real, &other->imaginary, &first);
            big_mul(&one->imaginary, &other->real, &second);
            big_add(&first, &second, &first);
            big_add(&imaginary[at], &first, &imaginary[at]);
        }
    }
    for (size_t at = 0u; at < span; at += 1u)
    {
        // The offset is below the span, which fits int32_t, because the powers do.
        poly_push(product, lowest + (int32_t)at, &real[at], &imaginary[at]);
    }
    platform_cell_released(real, platform_place_g_program_owner);
    platform_cell_released(imaginary, platform_place_g_program_owner);
}

/**
 * @brief A polynomial times an exact integer.
 *
 * @param[in]  poly   The polynomial [BORROWS].
 * @param[in]  factor The factor [BORROWS].
 * @param[out] result The product [BORROWS].
 */
static void poly_scale_big(const PiPoly *poly, const OriorExactInteger *factor, PiPoly *result)
{
    poly_empty(result);
    if (big_is_zero(factor))
    {
        return;
    }
    poly_reserve(result, poly->count);
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        OriorExactInteger real;
        OriorExactInteger imaginary;
        big_mul(&poly->terms[at].real, factor, &real);
        big_mul(&poly->terms[at].imaginary, factor, &imaginary);
        poly_push(result, poly->terms[at].power, &real, &imaginary);
    }
}

/**
 * @brief A polynomial times a machine integer.
 *
 * @param[in]  poly   The polynomial [BORROWS].
 * @param[in]  factor The factor.
 * @param[out] result The product [BORROWS].
 */
static void poly_scale(const PiPoly *poly, int64_t factor, PiPoly *result)
{
    OriorExactInteger big_factor;
    big_set(&big_factor, factor);
    poly_scale_big(poly, &big_factor, result);
}

/**
 * @brief Negates a polynomial in place.
 *
 * @param[in,out] poly The polynomial [BORROWS].
 */
static void poly_negate(PiPoly *poly)
{
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        poly->terms[at].real.sign = -poly->terms[at].real.sign;
        poly->terms[at].imaginary.sign = -poly->terms[at].imaginary.sign;
    }
}

/**
 * @brief The conjugate of a polynomial: each imaginary part negated.
 *
 * @param[in]  poly   The polynomial [BORROWS].
 * @param[out] result The conjugate [BORROWS].
 */
static void poly_conj(const PiPoly *poly, PiPoly *result)
{
    poly_copy(poly, result);
    for (size_t at = 0u; at < result->count; at += 1u)
    {
        result->terms[at].imaginary.sign = -result->terms[at].imaginary.sign;
    }
}

/**
 * @brief The real part of a polynomial.
 *
 * @param[in]  poly   The polynomial [BORROWS].
 * @param[out] result The real part [BORROWS].
 */
static void poly_real(const PiPoly *poly, PiPoly *result)
{
    poly_empty(result);
    OriorExactInteger zero;
    orior_exact_zero(&zero);
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        poly_push(result, poly->terms[at].power, &poly->terms[at].real, &zero);
    }
}

/**
 * @brief Brings a running divisor down to its gcd with every integer of a polynomial.
 *
 * @param[in]     poly    The polynomial [BORROWS].
 * @param[in,out] current The running divisor [BORROWS].
 */
static void poly_gcd(const PiPoly *poly, OriorExactInteger *current)
{
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        big_gcd(current, &poly->terms[at].real, current);
        big_gcd(current, &poly->terms[at].imaginary, current);
        if (big_is_one(current))
        {
            return;
        }
    }
}

/**
 * @brief Divides every integer of a polynomial by a divisor of all of them, in place.
 *
 * @param[in,out] poly    The polynomial [BORROWS].
 * @param[in]     divisor The divisor [BORROWS].
 */
static void poly_divide(PiPoly *poly, const OriorExactInteger *divisor)
{
    if (big_is_one(divisor))
    {
        return;
    }
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        big_div_exact(&poly->terms[at].real, divisor, &poly->terms[at].real);
        big_div_exact(&poly->terms[at].imaginary, divisor, &poly->terms[at].imaginary);
    }
}

int poly_equal(const PiPoly *left, const PiPoly *right)
{
    if (left->count != right->count)
    {
        return 0;
    }
    for (size_t at = 0u; at < left->count; at += 1u)
    {
        if ((left->terms[at].power != right->terms[at].power) ||
            !orior_exact_equal(&left->terms[at].real, &right->terms[at].real) ||
            !orior_exact_equal(&left->terms[at].imaginary, &right->terms[at].imaginary))
        {
            return 0;
        }
    }
    return 1;
}

void poly_print(const PiPoly *poly, FILE *out)
{
    if (poly->count == 0u)
    {
        fputs("0", out);
        return;
    }
    for (size_t at = 0u; at < poly->count; at += 1u)
    {
        const PiTerm *const term = &poly->terms[at];
        if (at > 0u)
        {
            fputs(" + ", out);
        }
        if (big_is_zero(&term->imaginary))
        {
            big_print(&term->real, out);
        }
        else if (big_is_zero(&term->real))
        {
            big_print(&term->imaginary, out);
            fputs(" i", out);
        }
        else
        {
            fputs("(", out);
            big_print(&term->real, out);
            fputs(" + ", out);
            big_print(&term->imaginary, out);
            fputs(" i)", out);
        }
        if (term->power == 1)
        {
            fputs(" pi", out);
        }
        else if (term->power != 0)
        {
            fprintf(out, " pi^%d", (int)term->power);
        }
    }
}

/* ---------------------------------------------------------------- ratios */

void ratio_free(RingRatio *ratio)
{
    poly_free(&ratio->poly);
}

void ratio_make(RingRatio *ratio, PiPoly *poly, const OriorExactInteger *denominator)
{
    ratio->poly = *poly;
    poly_empty(poly);
    ratio->denominator = *denominator;
    if (ratio->denominator.sign < 0)
    {
        poly_negate(&ratio->poly);
        ratio->denominator.sign = 1;
    }
    OriorExactInteger divisor = ratio->denominator;
    if (ratio->poly.count > 0u)
    {
        poly_gcd(&ratio->poly, &divisor);
    }
    poly_divide(&ratio->poly, &divisor);
    big_div_exact(&ratio->denominator, &divisor, &ratio->denominator);
}

int ratio_is_zero(const RingRatio *ratio)
{
    return ratio->poly.count == 0u;
}

int ratio_equal(const RingRatio *left, const RingRatio *right)
{
    return poly_equal(&left->poly, &right->poly) && orior_exact_equal(&left->denominator, &right->denominator);
}

void ratio_add(const RingRatio *left, const RingRatio *right, RingRatio *sum)
{
    OriorExactInteger common;
    big_lcm(&left->denominator, &right->denominator, &common);
    OriorExactInteger left_factor;
    OriorExactInteger right_factor;
    big_div_exact(&common, &left->denominator, &left_factor);
    big_div_exact(&common, &right->denominator, &right_factor);
    PiPoly left_scaled;
    PiPoly right_scaled;
    poly_scale_big(&left->poly, &left_factor, &left_scaled);
    poly_scale_big(&right->poly, &right_factor, &right_scaled);
    PiPoly total;
    poly_add(&left_scaled, &right_scaled, &total);
    poly_free(&left_scaled);
    poly_free(&right_scaled);
    ratio_make(sum, &total, &common);
}

void ratio_mul(const RingRatio *left, const RingRatio *right, RingRatio *product)
{
    PiPoly poly;
    poly_mul(&left->poly, &right->poly, &poly);
    OriorExactInteger denominator;
    big_mul(&left->denominator, &right->denominator, &denominator);
    ratio_make(product, &poly, &denominator);
}

void ratio_scale(const RingRatio *ratio, int64_t numerator, int64_t denominator, RingRatio *result)
{
    PiPoly poly;
    poly_scale(&ratio->poly, numerator, &poly);
    OriorExactInteger scaled;
    big_mul_small(&ratio->denominator, denominator, &scaled);
    ratio_make(result, &poly, &scaled);
}

/**
 * @brief The real part of a ratio.
 *
 * @param[in]  ratio  The ratio [BORROWS].
 * @param[out] result The real part [BORROWS].
 */
static void ratio_real_part(const RingRatio *ratio, RingRatio *result)
{
    PiPoly poly;
    poly_real(&ratio->poly, &poly);
    ratio_make(result, &poly, &ratio->denominator);
}

void ratio_power(const RingRatio *ratio, uint32_t exponent, RingRatio *result)
{
    PiPoly poly;
    poly_term(&poly, 0, 1, 0);
    OriorExactInteger denominator;
    big_set(&denominator, 1);
    for (uint32_t step = 0u; step < exponent; step += 1u)
    {
        PiPoly next;
        poly_mul(&poly, &ratio->poly, &next);
        poly_free(&poly);
        poly = next;
        big_mul(&denominator, &ratio->denominator, &denominator);
    }
    ratio_make(result, &poly, &denominator);
}

int32_t ratio_pi_degree(const RingRatio *ratio)
{
    return ratio->poly.terms[ratio->poly.count - 1u].power;
}

void ratio_print(const RingRatio *ratio, FILE *out)
{
    if (big_is_one(&ratio->denominator))
    {
        poly_print(&ratio->poly, out);
        return;
    }
    fputs("(", out);
    poly_print(&ratio->poly, out);
    fputs(") / ", out);
    big_print(&ratio->denominator, out);
}

void decay_rate(Viscosity viscosity, int64_t factor, RingRatio *rate)
{
    PiPoly poly;
    poly_term(&poly, 2, factor * viscosity.numerator, 0);
    OriorExactInteger denominator;
    big_set(&denominator, 1);
    ring_require(orior_exact_scale_by_ten(&denominator, viscosity.places), "scale");
    ratio_make(rate, &poly, &denominator);
}

/* ---------------------------------------------------------------- modes */

/**
 * @brief Orders two modes, axis 0 first.
 *
 * @param[in] left  First mode [BORROWS].
 * @param[in] right Second mode [BORROWS].
 * @return          -1, 0 or 1.
 */
static int mode_compare(const int32_t left[TORUS_AXES], const int32_t right[TORUS_AXES])
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        if (left[axis] != right[axis])
        {
            return (left[axis] < right[axis]) ? -1 : 1;
        }
    }
    return 0;
}

/**
 * @brief The order of two mode entries, for qsort.
 *
 * @param[in] left  First entry [BORROWS].
 * @param[in] right Second entry [BORROWS].
 * @return          -1, 0 or 1.
 */
static int entry_order(const void *left, const void *right)
{
    return mode_compare(((const ModeEntry *)left)->mode, ((const ModeEntry *)right)->mode);
}

int64_t norm_of(const int32_t mode[TORUS_AXES])
{
    return ((int64_t)mode[0] * mode[0]) + ((int64_t)mode[1] * mode[1]) + ((int64_t)mode[2] * mode[2]);
}

/* ---------------------------------------------------------------- scalar fields */

/**
 * @brief Makes room for at least a count of modes.
 *
 * @param[in,out] scalar The field [BORROWS].
 * @param[in]     count  The count.
 */
static void scalar_reserve(TorusScalar *scalar, size_t count)
{
    if (count <= scalar->capacity)
    {
        return;
    }
    const size_t grown = (count > (scalar->capacity * 2u)) ? count : (scalar->capacity * 2u);
    scalar->entries =
        (ModeEntry *)ring_memory(platform_cell_grown(scalar->entries, scalar->capacity * sizeof(ModeEntry),
                                                     grown * sizeof(ModeEntry), platform_place_g_program_owner, 0));
    scalar->capacity = grown;
}

/**
 * @brief Appends a mode and takes its polynomial. This frees a zero polynomial, and does not store it.
 *
 * @param[in,out] scalar The field under assembly [BORROWS].
 * @param[in]     mode   The mode [BORROWS].
 * @param[in,out] poly   The polynomial. The field owns it after the call [OWNS].
 */
static void scalar_push(TorusScalar *scalar, const int32_t mode[TORUS_AXES], PiPoly *poly)
{
    if (poly->count == 0u)
    {
        poly_free(poly);
        return;
    }
    scalar_reserve(scalar, scalar->count + 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = scalar->entries[scalar->count].mode, .src = mode,
               .bytes = sizeof(int32_t) * TORUS_AXES);
    scalar->entries[scalar->count].poly = *poly;
    poly_empty(poly);
    scalar->count += 1u;
}

/**
 * @brief An empty field under assembly, with no modes.
 *
 * @param[out] scalar The field [BORROWS].
 */
static void scalar_begin(TorusScalar *scalar)
{
    scalar->entries = NULL;
    scalar->count = 0u;
    scalar->capacity = 0u;
    big_set(&scalar->denominator, 1);
}

/**
 * @brief Ends the assembly of a field: sorts the modes and normalizes against a denominator.
 *
 * @param[in,out] scalar      The field, with distinct modes in any order [BORROWS].
 * @param[in]     denominator The denominator, not zero [BORROWS].
 */
static void scalar_finish(TorusScalar *scalar, const OriorExactInteger *denominator)
{
    if (scalar->count > 1u)
    {
        qsort(scalar->entries, scalar->count, sizeof(ModeEntry), entry_order);
    }
    scalar->denominator = *denominator;
    if (scalar->denominator.sign < 0)
    {
        for (size_t at = 0u; at < scalar->count; at += 1u)
        {
            poly_negate(&scalar->entries[at].poly);
        }
        scalar->denominator.sign = 1;
    }
    OriorExactInteger divisor = scalar->denominator;
    for (size_t at = 0u; (at < scalar->count) && !big_is_one(&divisor); at += 1u)
    {
        poly_gcd(&scalar->entries[at].poly, &divisor);
    }
    for (size_t at = 0u; at < scalar->count; at += 1u)
    {
        poly_divide(&scalar->entries[at].poly, &divisor);
    }
    big_div_exact(&scalar->denominator, &divisor, &scalar->denominator);
}

/**
 * @brief The entry of a field at a mode, or NULL where the field does not carry it.
 *
 * @param[in] scalar The field, with modes in ascending order [BORROWS].
 * @param[in] mode   The mode [BORROWS].
 * @return           The entry, or NULL.
 */
static const ModeEntry *scalar_find(const TorusScalar *scalar, const int32_t mode[TORUS_AXES])
{
    size_t low = 0u;
    size_t high = scalar->count;
    while (low < high)
    {
        const size_t middle = low + ((high - low) / 2u);
        const int order = mode_compare(scalar->entries[middle].mode, mode);
        if (order == 0)
        {
            return &scalar->entries[middle];
        }
        if (order < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return NULL;
}

void scalar_free(TorusScalar *scalar)
{
    for (size_t at = 0u; at < scalar->count; at += 1u)
    {
        poly_free(&scalar->entries[at].poly);
    }
    platform_cell_released(scalar->entries, platform_place_g_program_owner);
    scalar_begin(scalar);
}

void scalar_zero(TorusScalar *scalar)
{
    scalar_begin(scalar);
}

void scalar_copy(const TorusScalar *source, TorusScalar *copy)
{
    scalar_begin(copy);
    scalar_reserve(copy, source->count);
    for (size_t at = 0u; at < source->count; at += 1u)
    {
        PiPoly poly;
        poly_copy(&source->entries[at].poly, &poly);
        scalar_push(copy, source->entries[at].mode, &poly);
    }
    copy->denominator = source->denominator;
}

int scalar_is_zero(const TorusScalar *scalar)
{
    return scalar->count == 0u;
}

int scalar_equal(const TorusScalar *left, const TorusScalar *right)
{
    if ((left->count != right->count) || !orior_exact_equal(&left->denominator, &right->denominator))
    {
        return 0;
    }
    for (size_t at = 0u; at < left->count; at += 1u)
    {
        if ((mode_compare(left->entries[at].mode, right->entries[at].mode) != 0) ||
            !poly_equal(&left->entries[at].poly, &right->entries[at].poly))
        {
            return 0;
        }
    }
    return 1;
}

int scalar_has_mode(const TorusScalar *scalar, const int32_t mode[TORUS_AXES])
{
    return scalar_find(scalar, mode) != NULL;
}

void scalar_add(const TorusScalar *left, const TorusScalar *right, TorusScalar *sum)
{
    OriorExactInteger common;
    big_lcm(&left->denominator, &right->denominator, &common);
    OriorExactInteger left_factor;
    OriorExactInteger right_factor;
    big_div_exact(&common, &left->denominator, &left_factor);
    big_div_exact(&common, &right->denominator, &right_factor);
    scalar_begin(sum);
    scalar_reserve(sum, left->count + right->count);
    size_t left_at = 0u;
    size_t right_at = 0u;
    while ((left_at < left->count) || (right_at < right->count))
    {
        const int order = (right_at >= right->count) ? -1
                          : (left_at >= left->count)
                              ? 1
                              : mode_compare(left->entries[left_at].mode, right->entries[right_at].mode);
        PiPoly poly;
        if (order < 0)
        {
            poly_scale_big(&left->entries[left_at].poly, &left_factor, &poly);
            scalar_push(sum, left->entries[left_at].mode, &poly);
            left_at += 1u;
        }
        else if (order > 0)
        {
            poly_scale_big(&right->entries[right_at].poly, &right_factor, &poly);
            scalar_push(sum, right->entries[right_at].mode, &poly);
            right_at += 1u;
        }
        else
        {
            PiPoly left_scaled;
            PiPoly right_scaled;
            poly_scale_big(&left->entries[left_at].poly, &left_factor, &left_scaled);
            poly_scale_big(&right->entries[right_at].poly, &right_factor, &right_scaled);
            poly_add(&left_scaled, &right_scaled, &poly);
            poly_free(&left_scaled);
            poly_free(&right_scaled);
            scalar_push(sum, left->entries[left_at].mode, &poly);
            left_at += 1u;
            right_at += 1u;
        }
    }
    scalar_finish(sum, &common);
}

void scalar_neg(const TorusScalar *scalar, TorusScalar *result)
{
    scalar_copy(scalar, result);
    for (size_t at = 0u; at < result->count; at += 1u)
    {
        poly_negate(&result->entries[at].poly);
    }
}

/**
 * @brief The difference of two scalar fields.
 *
 * @param[in]  left       The minuend [BORROWS].
 * @param[in]  right      The subtrahend [BORROWS].
 * @param[out] difference The difference [BORROWS].
 */
static void scalar_sub(const TorusScalar *left, const TorusScalar *right, TorusScalar *difference)
{
    TorusScalar negative;
    scalar_neg(right, &negative);
    scalar_add(left, &negative, difference);
    scalar_free(&negative);
}

void scalar_scale(const TorusScalar *scalar, int64_t numerator, int64_t denominator, TorusScalar *result)
{
    scalar_begin(result);
    scalar_reserve(result, scalar->count);
    for (size_t at = 0u; at < scalar->count; at += 1u)
    {
        PiPoly poly;
        poly_scale(&scalar->entries[at].poly, numerator, &poly);
        scalar_push(result, scalar->entries[at].mode, &poly);
    }
    OriorExactInteger scaled;
    big_mul_small(&scalar->denominator, denominator, &scaled);
    scalar_finish(result, &scaled);
}

void scalar_times(const TorusScalar *scalar, const RingRatio *ratio, TorusScalar *result)
{
    scalar_begin(result);
    scalar_reserve(result, scalar->count);
    for (size_t at = 0u; at < scalar->count; at += 1u)
    {
        PiPoly poly;
        poly_mul(&scalar->entries[at].poly, &ratio->poly, &poly);
        scalar_push(result, scalar->entries[at].mode, &poly);
    }
    OriorExactInteger scaled;
    big_mul(&scalar->denominator, &ratio->denominator, &scaled);
    scalar_finish(result, &scaled);
}

/**
 * @brief The product of two scalar fields: a linear convolution of their modes, exact and unaliased.
 *
 * @param[in]  left    First factor [BORROWS].
 * @param[in]  right   Second factor [BORROWS].
 * @param[out] product The product [BORROWS].
 */
static void scalar_mul(const TorusScalar *left, const TorusScalar *right, TorusScalar *product)
{
    const size_t pairs = left->count * right->count;
    ModeEntry *const terms = (pairs > 0u) ? (ModeEntry *)ring_memory(platform_cell_taken(
                                                pairs * sizeof(ModeEntry), platform_place_g_program_owner, 0))
                                          : NULL;
    size_t made = 0u;
    for (size_t left_at = 0u; left_at < left->count; left_at += 1u)
    {
        for (size_t right_at = 0u; right_at < right->count; right_at += 1u)
        {
            for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
            {
                terms[made].mode[axis] = left->entries[left_at].mode[axis] + right->entries[right_at].mode[axis];
            }
            poly_mul(&left->entries[left_at].poly, &right->entries[right_at].poly, &terms[made].poly);
            made += 1u;
        }
    }
    if (made > 1u)
    {
        qsort(terms, made, sizeof(ModeEntry), entry_order);
    }
    scalar_begin(product);
    size_t at = 0u;
    while (at < made)
    {
        PiPoly total = terms[at].poly;
        size_t next = at + 1u;
        while ((next < made) && (mode_compare(terms[next].mode, terms[at].mode) == 0))
        {
            PiPoly sum;
            poly_add(&total, &terms[next].poly, &sum);
            poly_free(&total);
            poly_free(&terms[next].poly);
            total = sum;
            next += 1u;
        }
        scalar_push(product, terms[at].mode, &total);
        at = next;
    }
    platform_cell_released(terms, platform_place_g_program_owner);
    OriorExactInteger denominator;
    big_mul(&left->denominator, &right->denominator, &denominator);
    scalar_finish(product, &denominator);
}

void scalar_derivative(const TorusScalar *scalar, unsigned axis, TorusScalar *result)
{
    scalar_begin(result);
    scalar_reserve(result, scalar->count);
    for (size_t at = 0u; at < scalar->count; at += 1u)
    {
        PiPoly factor;
        poly_term(&factor, 1, 0, 2 * (int64_t)scalar->entries[at].mode[axis]);
        PiPoly poly;
        poly_mul(&scalar->entries[at].poly, &factor, &poly);
        poly_free(&factor);
        scalar_push(result, scalar->entries[at].mode, &poly);
    }
    scalar_finish(result, &scalar->denominator);
}

/**
 * @brief The Laplacian of a scalar field: each mode times -4 pi^2 |k|^2.
 *
 * @param[in]  scalar The field [BORROWS].
 * @param[out] result The Laplacian [BORROWS].
 */
static void scalar_laplacian(const TorusScalar *scalar, TorusScalar *result)
{
    scalar_begin(result);
    scalar_reserve(result, scalar->count);
    for (size_t at = 0u; at < scalar->count; at += 1u)
    {
        PiPoly factor;
        poly_term(&factor, 2, -4 * norm_of(scalar->entries[at].mode), 0);
        PiPoly poly;
        poly_mul(&scalar->entries[at].poly, &factor, &poly);
        poly_free(&factor);
        scalar_push(result, scalar->entries[at].mode, &poly);
    }
    scalar_finish(result, &scalar->denominator);
}

/**
 * @brief A copy of the modes of a field that a test keeps, over the same denominator.
 *
 * @param[in]  scalar The field [BORROWS].
 * @param[in]  keep   Whether to keep a mode [BORROWS].
 * @param[in]  shell  A value that the test reads.
 * @param[in]  other  A mode that the test reads, or NULL [BORROWS].
 * @param[out] result The field of kept modes [BORROWS].
 */
static void scalar_filter(const TorusScalar *scalar,
                          int (*keep)(const int32_t mode[TORUS_AXES], int64_t shell, const int32_t *other),
                          int64_t shell, const int32_t *other, TorusScalar *result)
{
    scalar_begin(result);
    for (size_t at = 0u; at < scalar->count; at += 1u)
    {
        if (keep(scalar->entries[at].mode, shell, other))
        {
            PiPoly poly;
            poly_copy(&scalar->entries[at].poly, &poly);
            scalar_push(result, scalar->entries[at].mode, &poly);
        }
    }
    scalar_finish(result, &scalar->denominator);
}

/**
 * @brief Keeps every mode but the zero mode.
 *
 * @param[in] mode  The mode [BORROWS].
 * @param[in] shell Unused.
 * @param[in] other Unused [BORROWS].
 * @return          1 where the mode is not zero.
 */
static int keep_nonzero(const int32_t mode[TORUS_AXES], int64_t shell, const int32_t *other)
{
    (void)shell;
    (void)other;
    return norm_of(mode) != 0;
}

/**
 * @brief Keeps the modes with |k|^2 equal to the shell.
 *
 * @param[in] mode  The mode [BORROWS].
 * @param[in] shell The shell.
 * @param[in] other Unused [BORROWS].
 * @return          1 where the mode lies on the shell.
 */
static int keep_shell(const int32_t mode[TORUS_AXES], int64_t shell, const int32_t *other)
{
    (void)other;
    return norm_of(mode) == shell;
}

/**
 * @brief Keeps every mode but one.
 *
 * @param[in] mode  The mode [BORROWS].
 * @param[in] shell Unused.
 * @param[in] other The mode to drop [BORROWS].
 * @return          1 where the mode is not that one.
 */
static int keep_other(const int32_t mode[TORUS_AXES], int64_t shell, const int32_t *other)
{
    (void)shell;
    return mode_compare(mode, other) != 0;
}

void scalar_without_mean(const TorusScalar *scalar, TorusScalar *result)
{
    scalar_filter(scalar, keep_nonzero, 0, NULL, result);
}

void scalar_in_shell(const TorusScalar *scalar, int64_t shell, TorusScalar *result)
{
    scalar_filter(scalar, keep_shell, shell, NULL, result);
}

void scalar_without_mode(const TorusScalar *scalar, const int32_t mode[TORUS_AXES], TorusScalar *result)
{
    scalar_filter(scalar, keep_other, 0, mode, result);
}

/* ---------------------------------------------------------------- vector fields */

void vec_free(TorusVector *vector)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_free(&vector->axis[axis]);
    }
}

void vec_zero(TorusVector *vector)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_zero(&vector->axis[axis]);
    }
}

void vec_copy(const TorusVector *source, TorusVector *copy)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_copy(&source->axis[axis], &copy->axis[axis]);
    }
}

void vec_add(const TorusVector *left, const TorusVector *right, TorusVector *sum)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_add(&left->axis[axis], &right->axis[axis], &sum->axis[axis]);
    }
}

void vec_sub(const TorusVector *left, const TorusVector *right, TorusVector *difference)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_sub(&left->axis[axis], &right->axis[axis], &difference->axis[axis]);
    }
}

void vec_scale(const TorusVector *vector, int64_t numerator, int64_t denominator, TorusVector *result)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_scale(&vector->axis[axis], numerator, denominator, &result->axis[axis]);
    }
}

void vec_times(const TorusVector *vector, const RingRatio *ratio, TorusVector *result)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_times(&vector->axis[axis], ratio, &result->axis[axis]);
    }
}

int vec_is_zero(const TorusVector *vector)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        if (!scalar_is_zero(&vector->axis[axis]))
        {
            return 0;
        }
    }
    return 1;
}

int vec_equal(const TorusVector *left, const TorusVector *right)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        if (!scalar_equal(&left->axis[axis], &right->axis[axis]))
        {
            return 0;
        }
    }
    return 1;
}

void vec_laplacian(const TorusVector *vector, TorusVector *result)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_laplacian(&vector->axis[axis], &result->axis[axis]);
    }
}

/**
 * @brief Adds a field into a running sum, and frees the field.
 *
 * @param[in,out] total The running sum [BORROWS].
 * @param[in,out] term  The field to add. This frees it [OWNS].
 */
static void scalar_accumulate(TorusScalar *total, TorusScalar *term)
{
    TorusScalar sum;
    scalar_add(total, term, &sum);
    scalar_free(total);
    scalar_free(term);
    *total = sum;
}

/**
 * @brief Adds a vector field into a running sum, and frees the field.
 *
 * @param[in,out] total The running sum [BORROWS].
 * @param[in,out] term  The field to add. This frees it [OWNS].
 */
static void vec_accumulate(TorusVector *total, TorusVector *term)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_accumulate(&total->axis[axis], &term->axis[axis]);
    }
}

void divergence(const TorusVector *vector, TorusScalar *result)
{
    scalar_zero(result);
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        TorusScalar term;
        scalar_derivative(&vector->axis[axis], axis, &term);
        scalar_accumulate(result, &term);
    }
}

void gradient(const TorusScalar *scalar, TorusVector *result)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_derivative(scalar, axis, &result->axis[axis]);
    }
}

void curl(const TorusVector *vector, TorusVector *result)
{
    // Component c is d_first v_second - d_second v_first, with (c, first, second) a cyclic order of the axes.
    static const unsigned first_of[TORUS_AXES] = {1u, 2u, 0u};
    static const unsigned second_of[TORUS_AXES] = {2u, 0u, 1u};
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        TorusScalar plus;
        TorusScalar minus;
        scalar_derivative(&vector->axis[second_of[axis]], first_of[axis], &plus);
        scalar_derivative(&vector->axis[first_of[axis]], second_of[axis], &minus);
        scalar_sub(&plus, &minus, &result->axis[axis]);
        scalar_free(&plus);
        scalar_free(&minus);
    }
}

void advect(const TorusVector *carrier, const TorusVector *carried, TorusVector *result)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_zero(&result->axis[axis]);
        for (unsigned along = 0u; along < TORUS_AXES; along += 1u)
        {
            TorusScalar slope;
            scalar_derivative(&carried->axis[axis], along, &slope);
            TorusScalar term;
            scalar_mul(&carrier->axis[along], &slope, &term);
            scalar_free(&slope);
            scalar_accumulate(&result->axis[axis], &term);
        }
    }
}

void dot(const TorusVector *left, const TorusVector *right, TorusScalar *result)
{
    scalar_zero(result);
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        TorusScalar term;
        scalar_mul(&left->axis[axis], &right->axis[axis], &term);
        scalar_accumulate(result, &term);
    }
}

/**
 * @brief The order of two modes in a list, for qsort.
 *
 * @param[in] left  First mode [BORROWS].
 * @param[in] right Second mode [BORROWS].
 * @return          -1, 0 or 1.
 */
static int list_order(const void *left, const void *right)
{
    return mode_compare((const int32_t *)left, (const int32_t *)right);
}

void modes_of(const TorusVector *vector, ModeList *list)
{
    size_t total = 0u;
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        total += vector->axis[axis].count;
    }
    list->modes = (total > 0u) ? (int32_t (*)[TORUS_AXES])ring_memory(platform_cell_taken(
                                     total * sizeof(*list->modes), platform_place_g_program_owner, 0))
                               : NULL;
    size_t made = 0u;
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        for (size_t at = 0u; at < vector->axis[axis].count; at += 1u)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = list->modes[made], .src = vector->axis[axis].entries[at].mode,
                       .bytes = sizeof(int32_t) * TORUS_AXES);
            made += 1u;
        }
    }
    if (made > 1u)
    {
        qsort(list->modes, made, sizeof(*list->modes), list_order);
    }
    size_t kept = 0u;
    for (size_t at = 0u; at < made; at += 1u)
    {
        if ((kept == 0u) || (mode_compare(list->modes[kept - 1u], list->modes[at]) != 0))
        {
            EMBED_CALL(memor.move_down, MemoriaCfg, .dst = list->modes[kept], .src = list->modes[at],
                       .bytes = sizeof(int32_t) * TORUS_AXES);
            kept += 1u;
        }
    }
    list->count = kept;
}

/** @brief The three components of a field over one common denominator, before any normal form. */
typedef struct
{
    TorusScalar axis[TORUS_AXES]; /**< The numerators, each over the common denominator. */
    OriorExactInteger common;     /**< The common denominator. */
} CommonNumerators;

/**
 * @brief The three components brought to one denominator: their polynomials scaled, and that denominator.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[out] result The numerators and the common denominator [BORROWS].
 */
static void common_numerators(const TorusVector *vector, CommonNumerators *result)
{
    big_lcm(&vector->axis[0].denominator, &vector->axis[1].denominator, &result->common);
    big_lcm(&result->common, &vector->axis[2].denominator, &result->common);
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        OriorExactInteger factor;
        big_div_exact(&result->common, &vector->axis[axis].denominator, &factor);
        scalar_begin(&result->axis[axis]);
        scalar_reserve(&result->axis[axis], vector->axis[axis].count);
        for (size_t at = 0u; at < vector->axis[axis].count; at += 1u)
        {
            PiPoly poly;
            poly_scale_big(&vector->axis[axis].entries[at].poly, &factor, &poly);
            scalar_push(&result->axis[axis], vector->axis[axis].entries[at].mode, &poly);
        }
    }
}

/**
 * @brief Frees the numerators of common_numerators.
 *
 * @param[in,out] numerators The numerators [BORROWS].
 */
static void common_free(CommonNumerators *numerators)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_free(&numerators->axis[axis]);
    }
}

/**
 * @brief The polynomial of a numerator at a mode, or the empty polynomial where it has none.
 *
 * @param[in] numerators The numerators [BORROWS].
 * @param[in] axis       The axis.
 * @param[in] mode       The mode [BORROWS].
 * @return               The polynomial [BORROWS].
 */
static const PiPoly *numerator_at(const CommonNumerators *numerators, unsigned axis, const int32_t mode[TORUS_AXES])
{
    static const PiPoly nothing = {NULL, 0u, 0u};
    const ModeEntry *const entry = scalar_find(&numerators->axis[axis], mode);
    return (entry != NULL) ? &entry->poly : &nothing;
}

/**
 * @brief The least common multiple of |k|^2 over the nonzero modes: the one integer that each projection divides by.
 *
 * @param[in]  list     The modes [BORROWS].
 * @param[out] multiple The multiple [BORROWS].
 */
static void norm_multiple(const ModeList *list, OriorExactInteger *multiple)
{
    big_set(multiple, 1);
    for (size_t at = 0u; at < list->count; at += 1u)
    {
        const int64_t norm = norm_of(list->modes[at]);
        if (norm != 0)
        {
            OriorExactInteger value;
            big_set(&value, norm);
            big_lcm(multiple, &value, multiple);
        }
    }
}

/**
 * @brief The sum over the axes of k_axis times a polynomial of each axis.
 *
 * @param[in]  parts The three polynomials [BORROWS].
 * @param[in]  mode  The mode k [BORROWS].
 * @param[out] along The sum [BORROWS].
 */
static void along_mode(const PiPoly *const parts[TORUS_AXES], const int32_t mode[TORUS_AXES], PiPoly *along)
{
    poly_empty(along);
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        PiPoly scaled;
        poly_scale(parts[axis], mode[axis], &scaled);
        PiPoly sum;
        poly_add(along, &scaled, &sum);
        poly_free(along);
        poly_free(&scaled);
        *along = sum;
    }
}

void leray(const TorusVector *vector, TorusVector *result)
{
    CommonNumerators numerators;
    common_numerators(vector, &numerators);
    ModeList list;
    modes_of(vector, &list);
    OriorExactInteger multiple;
    norm_multiple(&list, &multiple);
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_begin(&result->axis[axis]);
    }
    for (size_t at = 0u; at < list.count; at += 1u)
    {
        const int32_t *const mode = list.modes[at];
        const int64_t norm = norm_of(mode);
        const PiPoly *const parts[TORUS_AXES] = {numerator_at(&numerators, 0u, mode),
                                                 numerator_at(&numerators, 1u, mode),
                                                 numerator_at(&numerators, 2u, mode)};
        if (norm == 0)
        {
            for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
            {
                PiPoly poly;
                poly_scale_big(parts[axis], &multiple, &poly);
                scalar_push(&result->axis[axis], mode, &poly);
            }
            continue;
        }
        PiPoly along;
        along_mode(parts, mode, &along);
        OriorExactInteger norm_value;
        big_set(&norm_value, norm);
        OriorExactInteger share;
        big_div_exact(&multiple, &norm_value, &share);
        for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
        {
            PiPoly kept;
            poly_scale_big(parts[axis], &multiple, &kept);
            OriorExactInteger factor;
            big_mul_small(&share, mode[axis], &factor);
            PiPoly removed;
            poly_scale_big(&along, &factor, &removed);
            poly_negate(&removed);
            PiPoly poly;
            poly_add(&kept, &removed, &poly);
            poly_free(&kept);
            poly_free(&removed);
            scalar_push(&result->axis[axis], mode, &poly);
        }
        poly_free(&along);
    }
    OriorExactInteger denominator;
    big_mul(&numerators.common, &multiple, &denominator);
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_finish(&result->axis[axis], &denominator);
    }
    platform_cell_released(list.modes, platform_place_g_program_owner);
    common_free(&numerators);
}

void pressure_of(const TorusVector *nonlinear, TorusScalar *result)
{
    CommonNumerators numerators;
    common_numerators(nonlinear, &numerators);
    ModeList list;
    modes_of(nonlinear, &list);
    OriorExactInteger multiple;
    norm_multiple(&list, &multiple);
    PiPoly inverse_pi;
    poly_term(&inverse_pi, -1, 0, 1);
    scalar_begin(result);
    for (size_t at = 0u; at < list.count; at += 1u)
    {
        const int32_t *const mode = list.modes[at];
        const int64_t norm = norm_of(mode);
        if (norm == 0)
        {
            continue;
        }
        const PiPoly *const parts[TORUS_AXES] = {numerator_at(&numerators, 0u, mode),
                                                 numerator_at(&numerators, 1u, mode),
                                                 numerator_at(&numerators, 2u, mode)};
        PiPoly along;
        along_mode(parts, mode, &along);
        OriorExactInteger norm_value;
        big_set(&norm_value, norm);
        OriorExactInteger share;
        big_div_exact(&multiple, &norm_value, &share);
        PiPoly scaled;
        poly_scale_big(&along, &share, &scaled);
        PiPoly poly;
        poly_mul(&scaled, &inverse_pi, &poly);
        poly_free(&along);
        poly_free(&scaled);
        scalar_push(result, mode, &poly);
    }
    OriorExactInteger denominator;
    big_mul_small(&numerators.common, 2, &denominator);
    big_mul(&denominator, &multiple, &denominator);
    scalar_finish(result, &denominator);
    poly_free(&inverse_pi);
    platform_cell_released(list.modes, platform_place_g_program_owner);
    common_free(&numerators);
}

void biot_savart(const TorusVector *vorticity, TorusVector *result)
{
    CommonNumerators numerators;
    common_numerators(vorticity, &numerators);
    ModeList list;
    modes_of(vorticity, &list);
    OriorExactInteger multiple;
    norm_multiple(&list, &multiple);
    PiPoly inverse_pi;
    poly_term(&inverse_pi, -1, 0, 1);
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_begin(&result->axis[axis]);
    }
    // Component c of k x w is k_first w_second - k_second w_first, with (c, first, second) a cyclic order.
    static const unsigned first_of[TORUS_AXES] = {1u, 2u, 0u};
    static const unsigned second_of[TORUS_AXES] = {2u, 0u, 1u};
    for (size_t at = 0u; at < list.count; at += 1u)
    {
        const int32_t *const mode = list.modes[at];
        const int64_t norm = norm_of(mode);
        if (norm == 0)
        {
            continue;
        }
        OriorExactInteger norm_value;
        big_set(&norm_value, norm);
        OriorExactInteger share;
        big_div_exact(&multiple, &norm_value, &share);
        for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
        {
            const unsigned first = first_of[axis];
            const unsigned second = second_of[axis];
            PiPoly plus;
            PiPoly minus;
            poly_scale(numerator_at(&numerators, second, mode), mode[first], &plus);
            poly_scale(numerator_at(&numerators, first, mode), mode[second], &minus);
            poly_negate(&minus);
            PiPoly cross;
            poly_add(&plus, &minus, &cross);
            poly_free(&plus);
            poly_free(&minus);
            PiPoly scaled;
            poly_scale_big(&cross, &share, &scaled);
            PiPoly poly;
            poly_mul(&scaled, &inverse_pi, &poly);
            poly_free(&cross);
            poly_free(&scaled);
            scalar_push(&result->axis[axis], mode, &poly);
        }
    }
    OriorExactInteger denominator;
    big_mul_small(&numerators.common, 2, &denominator);
    big_mul(&denominator, &multiple, &denominator);
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_finish(&result->axis[axis], &denominator);
    }
    poly_free(&inverse_pi);
    platform_cell_released(list.modes, platform_place_g_program_owner);
    common_free(&numerators);
}

void parseval(const TorusVector *left, const TorusVector *right, RingRatio *result)
{
    RingRatio total;
    PiPoly nothing;
    poly_empty(&nothing);
    OriorExactInteger one;
    big_set(&one, 1);
    ratio_make(&total, &nothing, &one);
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        PiPoly piece;
        poly_empty(&piece);
        for (size_t at = 0u; at < left->axis[axis].count; at += 1u)
        {
            const ModeEntry *const other = scalar_find(&right->axis[axis], left->axis[axis].entries[at].mode);
            if (other == NULL)
            {
                continue;
            }
            PiPoly conjugate;
            poly_conj(&other->poly, &conjugate);
            PiPoly product;
            poly_mul(&left->axis[axis].entries[at].poly, &conjugate, &product);
            PiPoly sum;
            poly_add(&piece, &product, &sum);
            poly_free(&conjugate);
            poly_free(&product);
            poly_free(&piece);
            piece = sum;
        }
        OriorExactInteger denominator;
        big_mul(&left->axis[axis].denominator, &right->axis[axis].denominator, &denominator);
        RingRatio part;
        ratio_make(&part, &piece, &denominator);
        RingRatio sum;
        ratio_add(&total, &part, &sum);
        ratio_free(&total);
        ratio_free(&part);
        total = sum;
    }
    ratio_real_part(&total, result);
    ratio_free(&total);
}

int is_real_field(const TorusVector *vector)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        const TorusScalar *const scalar = &vector->axis[axis];
        for (size_t at = 0u; at < scalar->count; at += 1u)
        {
            int32_t mirror[TORUS_AXES];
            for (unsigned index = 0u; index < TORUS_AXES; index += 1u)
            {
                mirror[index] = -scalar->entries[at].mode[index];
            }
            const ModeEntry *const other = scalar_find(scalar, mirror);
            if (other == NULL)
            {
                return 0;
            }
            PiPoly conjugate;
            poly_conj(&scalar->entries[at].poly, &conjugate);
            const int same = poly_equal(&other->poly, &conjugate);
            poly_free(&conjugate);
            if (!same)
            {
                return 0;
            }
        }
    }
    return 1;
}

/* ---------------------------------------------------------------- the recurrences */

int64_t binomial(int64_t row, int64_t column)
{
    int64_t result = 1;
    for (int64_t index = 0; index < column; index += 1)
    {
        result = (result * (row - index)) / (index + 1);
    }
    return result;
}

/**
 * @brief nu times a field, nu as a numerator at decimal places.
 *
 * @param[in]  vector    The field [BORROWS].
 * @param[in]  viscosity The viscosity.
 * @param[out] result    The product [BORROWS].
 */
static void viscous(const TorusVector *vector, Viscosity viscosity, TorusVector *result)
{
    int64_t scale = 1;
    for (uint32_t place = 0u; place < viscosity.places; place += 1u)
    {
        scale *= 10;
    }
    vec_scale(vector, viscosity.numerator, scale, result);
}

/**
 * @brief The sum over the splits of C(step, split) times the advection of two coefficient lists.
 *
 * @param[in]  first  The coefficients of the carrier [BORROWS].
 * @param[in]  second The coefficients that the carrier moves [BORROWS].
 * @param[in]  step   The order.
 * @param[out] result The sum of C(step, split) advect(first[split], second[step - split]) [BORROWS].
 */
static void nonlinear_sum(const TorusVector *first, const TorusVector *second, unsigned step, TorusVector *result)
{
    vec_zero(result);
    for (unsigned split = 0u; split <= step; split += 1u)
    {
        TorusVector term;
        advect(&first[split], &second[step - split], &term);
        TorusVector scaled;
        vec_scale(&term, binomial(step, split), 1, &scaled);
        vec_free(&term);
        vec_accumulate(result, &scaled);
    }
}

void taylor_velocity(const TorusVector *datum, Viscosity viscosity, unsigned order, TorusVector *velocities,
                     TorusScalar *pressures)
{
    vec_copy(datum, &velocities[0]);
    for (unsigned step = 0u; step < order; step += 1u)
    {
        TorusVector nonlinear;
        nonlinear_sum(velocities, velocities, step, &nonlinear);
        if (pressures != NULL)
        {
            pressure_of(&nonlinear, &pressures[step]);
        }
        TorusVector laplacian;
        vec_laplacian(&velocities[step], &laplacian);
        TorusVector diffusion;
        viscous(&laplacian, viscosity, &diffusion);
        TorusVector projected;
        leray(&nonlinear, &projected);
        vec_sub(&diffusion, &projected, &velocities[step + 1u]);
        vec_free(&nonlinear);
        vec_free(&laplacian);
        vec_free(&diffusion);
        vec_free(&projected);
    }
}

void taylor_vorticity(const TorusVector *datum_vorticity, Viscosity viscosity, unsigned order, int stretch_term,
                      TorusVector *vorticities)
{
    vec_copy(datum_vorticity, &vorticities[0]);
    for (unsigned step = 0u; step < order; step += 1u)
    {
        TorusVector transport;
        vec_zero(&transport);
        for (unsigned split = 0u; split <= step; split += 1u)
        {
            TorusVector carrier;
            TorusVector carried;
            biot_savart(&vorticities[split], &carrier);
            biot_savart(&vorticities[step - split], &carried);
            TorusVector term;
            advect(&carrier, &vorticities[step - split], &term);
            if (stretch_term)
            {
                TorusVector stretched;
                advect(&vorticities[split], &carried, &stretched);
                TorusVector difference;
                vec_sub(&term, &stretched, &difference);
                vec_free(&term);
                vec_free(&stretched);
                term = difference;
            }
            TorusVector scaled;
            vec_scale(&term, binomial(step, split), 1, &scaled);
            vec_free(&term);
            vec_free(&carrier);
            vec_free(&carried);
            vec_accumulate(&transport, &scaled);
        }
        TorusVector laplacian;
        vec_laplacian(&vorticities[step], &laplacian);
        TorusVector diffusion;
        viscous(&laplacian, viscosity, &diffusion);
        vec_sub(&diffusion, &transport, &vorticities[step + 1u]);
        vec_free(&transport);
        vec_free(&laplacian);
        vec_free(&diffusion);
    }
}

void residual(const TorusVector *velocities, const TorusScalar *pressures, Viscosity viscosity, unsigned step,
              TorusVector *result)
{
    TorusVector nonlinear;
    nonlinear_sum(velocities, velocities, step, &nonlinear);
    TorusVector total;
    vec_add(&velocities[step + 1u], &nonlinear, &total);
    TorusVector laplacian;
    vec_laplacian(&velocities[step], &laplacian);
    TorusVector diffusion;
    viscous(&laplacian, viscosity, &diffusion);
    TorusVector less;
    vec_sub(&total, &diffusion, &less);
    TorusVector slope;
    gradient(&pressures[step], &slope);
    vec_add(&less, &slope, result);
    vec_free(&nonlinear);
    vec_free(&total);
    vec_free(&laplacian);
    vec_free(&diffusion);
    vec_free(&less);
    vec_free(&slope);
}

/* ---------------------------------------------------------------- data */

/**
 * @brief The field of two modes, k and -k, each with one constant coefficient.
 *
 * @param[in]  mode        The mode k [BORROWS].
 * @param[in]  plus_real   The real part at k.
 * @param[in]  plus_imag   The imaginary part at k.
 * @param[in]  minus_real  The real part at -k.
 * @param[in]  minus_imag  The imaginary part at -k.
 * @param[in]  denominator The denominator.
 * @param[out] scalar      The field [BORROWS].
 */
static void mode_pair(const int32_t mode[TORUS_AXES], int64_t plus_real, int64_t plus_imag, int64_t minus_real,
                      int64_t minus_imag, int64_t denominator, TorusScalar *scalar)
{
    int32_t mirror[TORUS_AXES];
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        mirror[axis] = -mode[axis];
    }
    scalar_begin(scalar);
    PiPoly plus;
    PiPoly minus;
    poly_term(&plus, 0, plus_real, plus_imag);
    poly_term(&minus, 0, minus_real, minus_imag);
    scalar_push(scalar, mode, &plus);
    scalar_push(scalar, mirror, &minus);
    OriorExactInteger value;
    big_set(&value, denominator);
    scalar_finish(scalar, &value);
}

/**
 * @brief The unit mode along an axis.
 *
 * @param[in]  axis The axis.
 * @param[out] mode The mode [BORROWS].
 */
static void unit_mode(unsigned axis, int32_t mode[TORUS_AXES])
{
    for (unsigned index = 0u; index < TORUS_AXES; index += 1u)
    {
        mode[index] = (index == axis) ? 1 : 0;
    }
}

void sine(unsigned axis, int64_t numerator, int64_t denominator, TorusScalar *scalar)
{
    int32_t mode[TORUS_AXES];
    unit_mode(axis, mode);
    mode_pair(mode, 0, -numerator, 0, numerator, 2 * denominator, scalar);
}

void harmonic(const int32_t mode[TORUS_AXES], int64_t numerator, int64_t denominator, TorusScalar *scalar)
{
    mode_pair(mode, numerator, 0, numerator, 0, 2 * denominator, scalar);
}

void cosine(unsigned axis, int64_t numerator, int64_t denominator, TorusScalar *scalar)
{
    int32_t mode[TORUS_AXES];
    unit_mode(axis, mode);
    harmonic(mode, numerator, denominator, scalar);
}

/**
 * @brief The sum of a sine and a cosine, into one component.
 *
 * @param[in]  sine_axis   The axis of the sine.
 * @param[in]  sine_top    The numerator of the amplitude of the sine.
 * @param[in]  sine_under  The denominator of the amplitude of the sine.
 * @param[in]  cosine_axis The axis of the cosine.
 * @param[in]  cosine_top  The numerator of the amplitude of the cosine.
 * @param[in]  cosine_under The denominator of the amplitude of the cosine.
 * @param[out] scalar      The sum [BORROWS].
 */
static void sine_plus_cosine(unsigned sine_axis, int64_t sine_top, int64_t sine_under, unsigned cosine_axis,
                             int64_t cosine_top, int64_t cosine_under, TorusScalar *scalar)
{
    TorusScalar first;
    TorusScalar second;
    sine(sine_axis, sine_top, sine_under, &first);
    cosine(cosine_axis, cosine_top, cosine_under, &second);
    scalar_add(&first, &second, scalar);
    scalar_free(&first);
    scalar_free(&second);
}

void abc_field_fraction(int64_t top_a, int64_t under_a, int64_t amplitude_b, int64_t amplitude_c, TorusVector *vector)
{
    sine_plus_cosine(2u, top_a, under_a, 1u, amplitude_c, 1, &vector->axis[0]);
    sine_plus_cosine(0u, amplitude_b, 1, 2u, top_a, under_a, &vector->axis[1]);
    sine_plus_cosine(1u, amplitude_c, 1, 0u, amplitude_b, 1, &vector->axis[2]);
}

void abc_field(int64_t amplitude_a, int64_t amplitude_b, int64_t amplitude_c, TorusVector *vector)
{
    abc_field_fraction(amplitude_a, 1, amplitude_b, amplitude_c, vector);
}

void generic_field(TorusVector *vector)
{
    generic_field_fraction(1, 1, vector);
}

void generic_field_fraction(int64_t top, int64_t under, TorusVector *vector)
{
    TorusScalar first;
    TorusScalar second;
    sine(1u, top, under, &first);
    sine(2u, 1, 1, &second);
    scalar_add(&first, &second, &vector->axis[0]);
    scalar_free(&first);
    scalar_free(&second);
    sine(2u, 1, 1, &vector->axis[1]);
    sine(0u, 1, 1, &vector->axis[2]);
}

void vec_stretched(const TorusVector *vector, int32_t factor, TorusVector *result)
{
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        const TorusScalar *const scalar = &vector->axis[axis];
        scalar_begin(&result->axis[axis]);
        scalar_reserve(&result->axis[axis], scalar->count);
        for (size_t at = 0u; at < scalar->count; at += 1u)
        {
            int32_t mode[TORUS_AXES];
            for (unsigned index = 0u; index < TORUS_AXES; index += 1u)
            {
                mode[index] = factor * scalar->entries[at].mode[index];
            }
            PiPoly poly;
            poly_scale(&scalar->entries[at].poly, factor, &poly);
            scalar_push(&result->axis[axis], mode, &poly);
        }
        scalar_finish(&result->axis[axis], &scalar->denominator);
    }
}

/* ---------------------------------------------------------------- the box horizon */

/**
 * @brief The largest |k_axis| of a mode, its |k|_inf.
 *
 * @param[in] mode The mode [BORROWS].
 * @return         The value.
 */
static int64_t box_size(const int32_t mode[TORUS_AXES])
{
    int64_t largest = 0;
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        const int64_t size = (mode[axis] < 0) ? -(int64_t)mode[axis] : (int64_t)mode[axis];
        largest = (size > largest) ? size : largest;
    }
    return largest;
}

/**
 * @brief Keeps the modes with |k|_inf at most the radius.
 *
 * @param[in] mode   The mode [BORROWS].
 * @param[in] radius The radius.
 * @param[in] other  Unused [BORROWS].
 * @return           1 where the mode lies in the box.
 */
static int keep_box(const int32_t mode[TORUS_AXES], int64_t radius, const int32_t *other)
{
    (void)other;
    return box_size(mode) <= radius;
}

void scalar_within_box(const TorusScalar *scalar, int64_t radius, TorusScalar *result)
{
    scalar_filter(scalar, keep_box, radius, NULL, result);
}

int64_t vec_mode_radius(const TorusVector *vector)
{
    ModeList list;
    modes_of(vector, &list);
    int64_t largest = 0;
    for (size_t at = 0u; at < list.count; at += 1u)
    {
        const int64_t size = box_size(list.modes[at]);
        largest = (size > largest) ? size : largest;
    }
    platform_cell_released(list.modes, platform_place_g_program_owner);
    return largest;
}

size_t vec_modes_outside(const TorusVector *vector, int64_t radius)
{
    ModeList list;
    modes_of(vector, &list);
    size_t count = 0u;
    for (size_t at = 0u; at < list.count; at += 1u)
    {
        count += (box_size(list.modes[at]) > radius) ? 1u : 0u;
    }
    platform_cell_released(list.modes, platform_place_g_program_owner);
    return count;
}
