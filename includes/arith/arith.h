// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// arith.h: the arithmetic of the window and the printer on a double's bits: the whole part cut by a mask over the
// mantissa, the sign a bit, the root a bit guess refined, the sine and cosine a quadrant and a short polynomial, the
// exponential and the logarithm the exponent field and a short polynomial. Each float entry narrows its double
#ifndef ARITH_H
#define ARITH_H

#include "embed_compiler_directives.h"
#include "embed_types.h"

// the largest float, the largest double, and pi
#define ARITH_F32_MAX 3.402823466e+38F
#define ARITH_F64_MAX 1.7976931348623157e308
#define ARITH_PI 3.141592653589793

// the bits of `value`, and the value of `bits`
embed_u64 arith_bits(double value);
double arith_value(embed_u64 bits);

// the unbiased exponent of `value`, and whether `value` is a number with a finite value
embed_i64 arith_exponent(double value);
int arith_finite(double value);

// `value` with its sign bit clear
double arith_abs(double value);

// `value` with its fraction bits cleared, toward zero: `value` itself where it is whole, infinite or not a number
double arith_whole(double value);

// the largest whole value at or below `value`, and the smallest at or above it
double arith_floor(double value);
double arith_ceil(double value);

// the whole value nearest `value`, a half going away from zero, and the same as an integer
double arith_round(double value);
long long arith_round_whole(double value);

// the smaller of two values, and the larger
double arith_min(double one, double other);
double arith_max(double one, double other);

// what is left of `value` after the whole multiples of `by` below it: between 0 and `by` for a positive `by`
double arith_remainder(double value, double by);

// the square root of `value`: 0 for a value at or below 0
double arith_root(double value);

// the sine and the cosine of `angle` in radians: 0 for an angle past 1.0e15 either way
double arith_sin(double angle);
double arith_cos(double angle);

// e to the power `power`: ARITH_F64_MAX past 709, 0 below -745
double arith_exp(double power);

// the natural logarithm of `value`: -ARITH_F64_MAX for a value at or below 0
double arith_log(double value);

// `base` to the power `power`, for a base at or above 0: 0 for a base of 0
double arith_pow(double base, double power);

// the length of the line from the origin to the point `across`, `down`
double arith_hypot(double across, double down);

// the arc tangent of `ratio`, in radians between minus and plus a quarter turn
double arith_atan(double ratio);

// the arc cosine of `cosine`, in radians from 0 to pi, a value past 1 or -1 read as that bound
double arith_acos(double cosine);

// the float entries, each its double narrowed
float arith_floor_f32(float value);
float arith_ceil_f32(float value);
float arith_round_f32(float value);
float arith_abs_f32(float value);
float arith_min_f32(float one, float other);
float arith_max_f32(float one, float other);
float arith_root_f32(float value);
float arith_sin_f32(float angle);
float arith_cos_f32(float angle);
float arith_pow_f32(float base, float power);
float arith_hypot_f32(float across, float down);
float arith_acos_f32(float cosine);
long long arith_round_whole_f32(float value);

#endif
