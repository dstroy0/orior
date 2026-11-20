// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef EDOUBLE_RECORD_H
#define EDOUBLE_RECORD_H

#include "../../integers/exact_record/exact_record.h"

// The exact double on the device, as steps of a record program. It has a mantissa register m and an exponent register
// e, and the value m 2^e. Each lane carries its own exponent.
//
// A product and a sum are exact. The sum lines the two mantissas up by the own power of two of the lane. The mantissa
// of the larger exponent multiplies up to the smaller exponent.
//
// Only a cut and a quotient drop bits. Each gives the pair below and above: the floor and ceiling of the mantissa at
// the exponent that it gives. The two are equal where the operation dropped nothing.
//
// Each call that reads two exponents or cuts a mantissa takes the bound that it rests on. That is the bits that the
// spread of the exponents or the shift lies within. The widths that keymath imprints follow from them.
//
// Where the program knows both exponents, as it knows each exponent that it sets, the sum lifts the one side by a known
// power. The exponents then cost no step.

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        unsigned int mantissa;
        unsigned int exponent;
    } EdoubleRecord;

    // the pair below and above a value, at one exponent
    typedef struct
    {
        unsigned int down;
        unsigned int up;
        unsigned int exponent;
    } EdoubleRecordInterval;

    EdoubleRecord edouble_record_of(unsigned int mantissa, unsigned int exponent);

    // mantissa 2^exponent, both constants
    EdoubleRecord edouble_record_constant(ExactRecordProgram *program, long long mantissa, long long exponent);

    EdoubleRecord edouble_record_negate(ExactRecordProgram *program, EdoubleRecord value);

    // a b, exact: the mantissas' product at the exponents' sum
    EdoubleRecord edouble_record_product(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b);

    // a + b, exact, for exponents known to differ by less than 2^spread_bits: the mantissa of the larger exponent
    // times 2^(the difference), at the smaller exponent
    EdoubleRecord edouble_record_sum(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b,
                                     unsigned int spread_bits);

    EdoubleRecord edouble_record_difference(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b,
                                            unsigned int spread_bits);

    // [a > b], 1 or 0, exact, for exponents known to differ by less than 2^spread_bits
    unsigned int edouble_record_above(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b,
                                      unsigned int spread_bits);

    // the mantissa, cut to below 2^width. The cut shifts it down by k, the least k with |m| / 2^k below 2^width. It
    // gives the floor and ceiling at e + k, for |m| known below 2^(width + 2^range_bits - 1). A mantissa past
    // 2^(width - 1) lands in [2^(width - 1), 2^width]. The register of each end gets width + 2 bits
    EdoubleRecordInterval edouble_record_cut(ExactRecordProgram *program, EdoubleRecord value, unsigned int width,
                                             unsigned int range_bits);

    // a / b to `width` bits past the mantissa of b, for a mantissa of b that is nonzero and known below 2^divisor_bits.
    // The result is the floor and ceiling of m_a 2^s / m_b, s = width + divisor_bits, at e_a - e_b - s
    EdoubleRecordInterval edouble_record_quotient(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b,
                                                  unsigned int width, unsigned int divisor_bits);

    // the floor and ceiling of n / d, d nonzero. They are the quotient toward zero, less 1 where the remainder is below
    // zero, and plus 1 where it is above
    void edouble_record_floor_ceiling(ExactRecordProgram *program, unsigned int n, unsigned int d, unsigned int *floor,
                                      unsigned int *ceiling);

    // ---- bounded values: every value between down 2^e and up 2^e, down <= up ----

    // an exact value, both ends its mantissa
    EdoubleRecordInterval edouble_record_interval_of(EdoubleRecord value);

    EdoubleRecordInterval edouble_record_interval_negate(ExactRecordProgram *program, EdoubleRecordInterval value);

    // the ends' sums, exact, for exponents known to differ by less than 2^spread_bits
    EdoubleRecordInterval edouble_record_interval_sum(ExactRecordProgram *program, EdoubleRecordInterval a,
                                                      EdoubleRecordInterval b, unsigned int spread_bits);

    EdoubleRecordInterval edouble_record_interval_difference(ExactRecordProgram *program, EdoubleRecordInterval a,
                                                             EdoubleRecordInterval b, unsigned int spread_bits);

    // the least and the max of the four corner products, exact, whatever the signs
    EdoubleRecordInterval edouble_record_interval_product(ExactRecordProgram *program, EdoubleRecordInterval a,
                                                          EdoubleRecordInterval b);

    // both ends, cut by one shift, the shift of the larger end. The result is the floor of the lower end and the ceiling
    // of the upper end, below 2^width in size. This holds for both ends known below 2^(width + 2^range_bits - 1). The
    // register of each end gets width + 2 bits
    EdoubleRecordInterval edouble_record_interval_cut(ExactRecordProgram *program, EdoubleRecordInterval value,
                                                      unsigned int width, unsigned int range_bits);

    // a / b, where b holds no 0 and both ends of b are below 2^divisor_bits. The result is the least floor and the max
    // ceiling of the four corners m_a 2^s / m_b, s = width + divisor_bits, at e_a - e_b - s
    EdoubleRecordInterval edouble_record_interval_quotient(ExactRecordProgram *program, EdoubleRecordInterval a,
                                                           EdoubleRecordInterval b, unsigned int width,
                                                           unsigned int divisor_bits);

    // 1 where every value a contains lies above every value b contains: a's lower end above b's upper, exact
    unsigned int edouble_record_interval_above(ExactRecordProgram *program, EdoubleRecordInterval a,
                                               EdoubleRecordInterval b, unsigned int spread_bits);

    // value 2^power for a constant power, exact: the exponent moved
    EdoubleRecordInterval edouble_record_interval_times_two_to(ExactRecordProgram *program, EdoubleRecordInterval value,
                                                               long long power);

    // the value at the constant exponent `exponent`, outward. The result is the floor of the lower end and the ceiling
    // of the upper end, of each end times 2^(e - exponent). It is exact where e is the larger.
    // The register of each end wraps to `bits`, for ends that the caller bounds within [-2^(bits - 1), 2^(bits - 1)).
    // Where the program does not know e, |e - exponent| lies below 2^range_bits
    EdoubleRecordInterval edouble_record_interval_at(ExactRecordProgram *program, EdoubleRecordInterval value,
                                                     long long exponent, unsigned int bits, unsigned int range_bits);

    // ---- balls: every value within the radius of the center, the radius never below zero ----
    //
    // The product of a ball costs one product for its center and three for its radius. The product of a bounded pair
    // takes the four corners and their order. Only a read at an exponent drops bits, and it widens the radius by what it
    // drops.
    //
    // Each read wraps the center and narrows the radius to the widths that it gets. It gives the flag `fits`, 1 where
    // both sit inside them. A run that has each flag at 1 dropped nothing to a wrap. The quotient takes exponents that
    // the program knows.

    typedef struct
    {
        EdoubleRecord center;
        EdoubleRecord radius;
    } EdoubleRecordBall;

    EdoubleRecordBall edouble_record_ball_of(EdoubleRecord center, EdoubleRecord radius);

    // an exact value, its radius 0
    EdoubleRecordBall edouble_record_ball_exact(ExactRecordProgram *program, EdoubleRecord value);

    EdoubleRecordBall edouble_record_ball_negate(ExactRecordProgram *program, EdoubleRecordBall value);

    // the centers' sum and the radii's, exact, each pair of exponents known to differ by less than 2^spread_bits
    EdoubleRecordBall edouble_record_ball_sum(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                              unsigned int spread_bits);

    EdoubleRecordBall edouble_record_ball_difference(ExactRecordProgram *program, EdoubleRecordBall a,
                                                     EdoubleRecordBall b, unsigned int spread_bits);

    // the centers' product, and the radius |c_a| r_b + |c_b| r_a + r_a r_b, exact
    EdoubleRecordBall edouble_record_ball_product(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                                  unsigned int spread_bits);

    // the ball times 2^power for a constant power, exact
    EdoubleRecordBall edouble_record_ball_times_two_to(ExactRecordProgram *program, EdoubleRecordBall value,
                                                       long long power);

    // the ball, read at the constant exponent `exponent`, with its radius at `radius_exponent`:
    //   - the center, cut toward zero there and wrapped to `bits`.
    //   - the ceiling of the radius there, narrowed to `radius_bits`. It is one unit of `exponent` more where the cut
    //     dropped bits of the center.
    //   - `fits`, 1 where |center| < 2^(bits - 1) and the radius is below 2^radius_bits.
    //
    // Where the program does not know an exponent, it lies within 2^range_bits of the read exponent
    EdoubleRecordBall edouble_record_ball_at(ExactRecordProgram *program, EdoubleRecordBall value, long long exponent,
                                             unsigned int bits, long long radius_exponent, unsigned int radius_bits,
                                             unsigned int range_bits, unsigned int *fits);

    // a / b at the constant exponent `exponent`, with its radius at `radius_exponent`, for known exponents:
    //   - the center is the quotient of the centers, cut toward zero.
    //   - the radius is the ceiling of (|c_b| r_a + |c_a| r_b) / (|c_b| (|c_b| - r_b)), and one unit more.
    //   - `fits` is as the read gives it, and 0 where the ball of b holds 0.
    EdoubleRecordBall edouble_record_ball_quotient(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                                   long long exponent, unsigned int bits, long long radius_exponent,
                                                   unsigned int radius_bits, unsigned int *fits);

    // the ball's lower and upper ends, exact
    EdoubleRecord edouble_record_ball_down(ExactRecordProgram *program, EdoubleRecordBall value, unsigned int spread_bits);

    EdoubleRecord edouble_record_ball_up(ExactRecordProgram *program, EdoubleRecordBall value, unsigned int spread_bits);

    // 1 where every value a contains lies above every value b contains, exact
    unsigned int edouble_record_ball_above(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                           unsigned int spread_bits);

#ifdef __cplusplus
}
#endif

#endif
