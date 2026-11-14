// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// arith.c: the arithmetic of arith.h on a double's bits, each function one body the whole program shares
#include "arith.h"

// a double's bits: the sign, the exponent's field and width, the mantissa's bits and the bias
#define ARITH_SIGN (1ULL << 63u)
#define ARITH_EXPONENT_MASK 0x7FFULL
#define ARITH_MANTISSA_BITS 52u
#define ARITH_BIAS 1023LL

// the value and its bits over one storage
typedef union {
    double value;
    embed_u64 bits;
} CliArithBits;

embed_u64 arith_bits(double value)
{
    CliArithBits both;
    both.value = value;
    return both.bits;
}

double arith_value(embed_u64 bits)
{
    CliArithBits both;
    both.bits = bits;
    return both.value;
}

embed_i64 arith_exponent(double value)
{
    // Explicit cast reads the eleven bits as the signed exponent they are a biased form of
    return (embed_i64)((arith_bits(value) >> ARITH_MANTISSA_BITS) & ARITH_EXPONENT_MASK) - ARITH_BIAS;
}

int arith_finite(double value)
{
    return ((arith_bits(value) >> ARITH_MANTISSA_BITS) & ARITH_EXPONENT_MASK) != ARITH_EXPONENT_MASK;
}

double arith_abs(double value)
{
    return arith_value(arith_bits(value) & ~ARITH_SIGN);
}

double arith_whole(double value)
{
    const embed_i64 exponent = arith_exponent(value);
    if (exponent >= (embed_i64)ARITH_MANTISSA_BITS)
    {
        return value;
    }
    if (exponent < 0LL)
    {
        return arith_value(arith_bits(value) & ARITH_SIGN);
    }
    // Explicit cast widens the exponent, 0 through 51 here, to the shift's unsigned count
    const embed_u64 fraction = (1ULL << (ARITH_MANTISSA_BITS - (embed_u64)exponent)) - 1ULL;
    return arith_value(arith_bits(value) & ~fraction);
}

double arith_floor(double value)
{
    const double whole = arith_whole(value);
    return (whole > value) ? whole - 1.0 : whole;
}

double arith_ceil(double value)
{
    const double whole = arith_whole(value);
    return (whole < value) ? whole + 1.0 : whole;
}

double arith_round(double value)
{
    const double whole = arith_whole(value);
    const double left = value - whole;
    if (left >= 0.5)
    {
        return whole + 1.0;
    }
    if (left <= -0.5)
    {
        return whole - 1.0;
    }
    return whole;
}

long long arith_round_whole(double value)
{
    // Explicit cast reads the rounded value, whole already, as the integer it is
    return (long long)arith_round(value);
}

double arith_min(double one, double other)
{
    return (other < one) ? other : one;
}

double arith_max(double one, double other)
{
    return (other > one) ? other : one;
}

double arith_remainder(double value, double by)
{
    return value - (arith_floor(value / by) * by);
}

// the root: a guess from the bits halved, refined by four Newton steps
double arith_root(double value)
{
    if (value <= 0.0)
    {
        return 0.0;
    }
    double root = arith_value((arith_bits(value) >> 1u) + 0x1FF7A3BEA91D9B1BULL);
    root = 0.5 * (root + (value / root));
    root = 0.5 * (root + (value / root));
    root = 0.5 * (root + (value / root));
    root = 0.5 * (root + (value / root));
    return root;
}

// pi over two in two parts, so the reduction loses little, and two over pi
#define ARITH_HALF_PI_HIGH 1.5707963267948966
#define ARITH_HALF_PI_LOW 6.123233995736766e-17
#define ARITH_TWO_OVER_PI 0.6366197723675814
// the angle past which the quadrant count would overflow: such an angle answers 0
#define ARITH_ANGLE_MAX 1.0e15

// the sine of `small`, which is within a quarter turn of 0, by its odd polynomial
EMBED_INLINE double arith_sin_small(double small)
{
    const double square = small * small;
    double sum = 1.0 / 6227020800.0;
    sum = (sum * square) - (1.0 / 39916800.0);
    sum = (sum * square) + (1.0 / 362880.0);
    sum = (sum * square) - (1.0 / 5040.0);
    sum = (sum * square) + (1.0 / 120.0);
    sum = (sum * square) - (1.0 / 6.0);
    return small + (small * square * sum);
}

// the cosine of `small`, which is within a quarter turn of 0, by its even polynomial
EMBED_INLINE double arith_cos_small(double small)
{
    const double square = small * small;
    double sum = -1.0 / 87178291200.0;
    sum = (sum * square) + (1.0 / 479001600.0);
    sum = (sum * square) - (1.0 / 3628800.0);
    sum = (sum * square) + (1.0 / 40320.0);
    sum = (sum * square) - (1.0 / 720.0);
    sum = (sum * square) + (1.0 / 24.0);
    sum = (sum * square) - 0.5;
    return 1.0 + (square * sum);
}

// The angle reduced to a quadrant and what is left within an eighth of a turn of its axis
typedef struct
{
    double left;
    unsigned int quadrant;
} CliArithQuadrant;

// `angle` as its quadrant count and what is left
EMBED_INLINE CliArithQuadrant arith_quadrant(double angle)
{
    const double quarters = arith_round(angle * ARITH_TWO_OVER_PI);
    const double left = (angle - (quarters * ARITH_HALF_PI_HIGH)) - (quarters * ARITH_HALF_PI_LOW);
    // Explicit cast reads the quarters, whole and within the angle's bound, as the integer whose low bits name the
    // quadrant
    const long long count = (long long)quarters;
    // Explicit cast retains the two low bits of the count, the same for a negative count in two's complement
    return (CliArithQuadrant){left, (unsigned int)((unsigned long long)count & 3ULL)};
}

double arith_sin(double angle)
{
    if (arith_abs(angle) > ARITH_ANGLE_MAX)
    {
        return 0.0;
    }
    const CliArithQuadrant quadrant = arith_quadrant(angle);
    const double sine = arith_sin_small(quadrant.left);
    const double cosine = arith_cos_small(quadrant.left);
    const double by_quadrant[4] = {sine, cosine, -sine, -cosine};
    return by_quadrant[quadrant.quadrant];
}

double arith_cos(double angle)
{
    if (arith_abs(angle) > ARITH_ANGLE_MAX)
    {
        return 0.0;
    }
    const CliArithQuadrant quadrant = arith_quadrant(angle);
    const double sine = arith_sin_small(quadrant.left);
    const double cosine = arith_cos_small(quadrant.left);
    const double by_quadrant[4] = {cosine, -sine, -cosine, sine};
    return by_quadrant[quadrant.quadrant];
}

// the natural logarithm of two in two parts, and of e in base two; the power past which e to it overflows, the power
// below which it is 0
#define ARITH_LN2_HIGH 0.6931471803691238
#define ARITH_LN2_LOW 1.9082149292705877e-10
#define ARITH_LOG2_E 1.4426950408889634
#define ARITH_EXP_MAX 709.0
#define ARITH_EXP_MIN -745.0

// two to the whole power `power`, which is between -1022 and 1023
EMBED_INLINE double arith_two_to(long long power)
{
    // Explicit cast reads the biased power, 1 through 2046 here, as the exponent field
    return arith_value((embed_u64)(power + ARITH_BIAS) << ARITH_MANTISSA_BITS);
}

// the power split into a whole count of ln 2 and what is left, e to what is left by its polynomial, the count added to
// the exponent field
double arith_exp(double power)
{
    if (power > ARITH_EXP_MAX)
    {
        return ARITH_F64_MAX;
    }
    if (power < ARITH_EXP_MIN)
    {
        return 0.0;
    }
    const double count = arith_round(power * ARITH_LOG2_E);
    const double left = (power - (count * ARITH_LN2_HIGH)) - (count * ARITH_LN2_LOW);
    double sum = 1.0 / 479001600.0;
    sum = (sum * left) + (1.0 / 39916800.0);
    sum = (sum * left) + (1.0 / 3628800.0);
    sum = (sum * left) + (1.0 / 362880.0);
    sum = (sum * left) + (1.0 / 40320.0);
    sum = (sum * left) + (1.0 / 5040.0);
    sum = (sum * left) + (1.0 / 720.0);
    sum = (sum * left) + (1.0 / 120.0);
    sum = (sum * left) + (1.0 / 24.0);
    sum = (sum * left) + (1.0 / 6.0);
    sum = (sum * left) + 0.5;
    sum = (sum * left) + 1.0;
    sum = (sum * left) + 1.0;
    // Explicit cast reads the count, whole and between -1075 and 1023 here, as the integer power of two
    const long long whole = (long long)count;
    // a count below the exponent field's reach is applied in two halves, each within it
    const long long first = whole / 2LL;
    return (sum * arith_two_to(first)) * arith_two_to(whole - first);
}

// the square root of two, the bound the mantissa is folded about so its logarithm's series stays short
#define ARITH_ROOT_TWO 1.4142135623730951

// the exponent times ln 2 and the logarithm of the mantissa, folded into the range from the root of a half to the
// root of two, by the series in (m - 1) / (m + 1)
double arith_log(double value)
{
    if (value <= 0.0)
    {
        return -ARITH_F64_MAX;
    }
    embed_i64 exponent = arith_exponent(value);
    const embed_u64 mantissa_bits = (arith_bits(value) & ((1ULL << ARITH_MANTISSA_BITS) - 1ULL)) |
                                    ((embed_u64)ARITH_BIAS << ARITH_MANTISSA_BITS);
    double mantissa = arith_value(mantissa_bits);
    if (mantissa > ARITH_ROOT_TWO)
    {
        mantissa *= 0.5;
        exponent += 1LL;
    }
    const double ratio = (mantissa - 1.0) / (mantissa + 1.0);
    const double square = ratio * ratio;
    double sum = 1.0 / 17.0;
    sum = (sum * square) + (1.0 / 15.0);
    sum = (sum * square) + (1.0 / 13.0);
    sum = (sum * square) + (1.0 / 11.0);
    sum = (sum * square) + (1.0 / 9.0);
    sum = (sum * square) + (1.0 / 7.0);
    sum = (sum * square) + (1.0 / 5.0);
    sum = (sum * square) + (1.0 / 3.0);
    sum = (sum * square) + 1.0;
    // Explicit cast widens the exponent to the double it scales
    return ((double)exponent * (ARITH_LN2_HIGH + ARITH_LN2_LOW)) + (2.0 * ratio * sum);
}

double arith_pow(double base, double power)
{
    if (base <= 0.0)
    {
        return 0.0;
    }
    return arith_exp(power * arith_log(base));
}

double arith_hypot(double across, double down)
{
    return arith_root((across * across) + (down * down));
}

// the tangent of an eighth of a turn, the bound the arc tangent's argument is folded about
#define ARITH_TAN_EIGHTH 0.41421356237309503

// the arc tangent of `ratio`, between 0 and 1: folded about an eighth of a turn, then its odd series
EMBED_INLINE double arith_atan_unit(double ratio)
{
    const int folded = ratio > ARITH_TAN_EIGHTH;
    const double small = folded ? (ratio - 1.0) / (ratio + 1.0) : ratio;
    const double square = small * small;
    double sum = -1.0 / 27.0;
    sum = (sum * square) + (1.0 / 25.0);
    sum = (sum * square) - (1.0 / 23.0);
    sum = (sum * square) + (1.0 / 21.0);
    sum = (sum * square) - (1.0 / 19.0);
    sum = (sum * square) + (1.0 / 17.0);
    sum = (sum * square) - (1.0 / 15.0);
    sum = (sum * square) + (1.0 / 13.0);
    sum = (sum * square) - (1.0 / 11.0);
    sum = (sum * square) + (1.0 / 9.0);
    sum = (sum * square) - (1.0 / 7.0);
    sum = (sum * square) + (1.0 / 5.0);
    sum = (sum * square) - (1.0 / 3.0);
    sum = (sum * square) + 1.0;
    const double arc = small * sum;
    return folded ? (ARITH_PI / 4.0) + arc : arc;
}

double arith_atan(double ratio)
{
    const double size = arith_abs(ratio);
    const double arc = (size > 1.0) ? (ARITH_PI / 2.0) - arith_atan_unit(1.0 / size) : arith_atan_unit(size);
    return (ratio < 0.0) ? -arc : arc;
}

double arith_acos(double cosine)
{
    const double bounded = arith_min(arith_max(cosine, -1.0), 1.0);
    if (bounded <= -1.0)
    {
        return ARITH_PI;
    }
    return 2.0 * arith_atan(arith_root((1.0 - bounded) / (1.0 + bounded)));
}

float arith_floor_f32(float value)
{
    // Explicit cast narrows the whole value, within the float's range, to the float
    return (float)arith_floor((double)value);
}

float arith_ceil_f32(float value)
{
    // Explicit cast narrows the whole value, within the float's range, to the float
    return (float)arith_ceil((double)value);
}

float arith_round_f32(float value)
{
    // Explicit cast narrows the whole value, within the float's range, to the float
    return (float)arith_round((double)value);
}

float arith_abs_f32(float value)
{
    // Explicit cast narrows a value that came from this float
    return (float)arith_abs((double)value);
}

float arith_min_f32(float one, float other)
{
    return (other < one) ? other : one;
}

float arith_max_f32(float one, float other)
{
    return (other > one) ? other : one;
}

float arith_root_f32(float value)
{
    // Explicit cast narrows the root, within the float's range, to the float
    return (float)arith_root((double)value);
}

float arith_sin_f32(float angle)
{
    // Explicit cast narrows a value between -1 and 1 to the float
    return (float)arith_sin((double)angle);
}

float arith_cos_f32(float angle)
{
    // Explicit cast narrows a value between -1 and 1 to the float
    return (float)arith_cos((double)angle);
}

float arith_pow_f32(float base, float power)
{
    // Explicit cast narrows the power to the float; a power past the float's range narrows to infinity
    return (float)arith_pow((double)base, (double)power);
}

float arith_hypot_f32(float across, float down)
{
    // Explicit cast narrows the length, within the float's range, to the float
    return (float)arith_hypot((double)across, (double)down);
}

float arith_acos_f32(float cosine)
{
    // Explicit cast narrows a value between 0 and pi to the float
    return (float)arith_acos((double)cosine);
}

long long arith_round_whole_f32(float value)
{
    return arith_round_whole((double)value);
}
