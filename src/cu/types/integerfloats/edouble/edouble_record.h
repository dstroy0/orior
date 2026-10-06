// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef EDOUBLE_RECORD_H
#define EDOUBLE_RECORD_H

#include "../../integers/exact_record/exact_record.h"

// The exact double on the device, as steps of a record program: a mantissa register m and an exponent register e,
// the value m 2^e, each lane holding its own exponent. A product and a sum are exact: the sum lines the two mantissas
// up by the lane's own power of two, the larger exponent's mantissa multiplied up to the smaller exponent. Only a cut
// and a quotient drop bits, and each gives the pair below and above, the mantissa's floor and ceiling at the exponent
// it gives, equal where nothing was dropped.
//
// Each call that reads two exponents or cuts a mantissa takes the bound it rests on, the bits the exponents' spread
// or the shift lies within; the widths keymath imprints follow from them. Where the program knows both exponents, as
// it knows every exponent it sets, the sum lifts the one side by a known power and costs no step for the exponents.

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
    } EdoubleRecordHeld;

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

    // the mantissa cut to below 2^width: shifted down by k, the least k with |m| / 2^k below 2^width, and the floor and
    // ceiling at e + k; for |m| known below 2^(width + 2^range_bits - 1). A mantissa past 2^(width - 1) lands in
    // [2^(width - 1), 2^width], and each end's register is given width + 2 bits
    EdoubleRecordHeld edouble_record_cut(ExactRecordProgram *program, EdoubleRecord value, unsigned int width,
                                         unsigned int range_bits);

    // a / b to `width` bits past b's mantissa, for b's mantissa nonzero and known below 2^divisor_bits: the floor and
    // ceiling of m_a 2^s / m_b, s = width + divisor_bits, at e_a - e_b - s
    EdoubleRecordHeld edouble_record_quotient(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b,
                                              unsigned int width, unsigned int divisor_bits);

    // the floor and ceiling of n / d, d nonzero: the quotient toward zero, less 1 where the remainder is below zero and
    // more 1 where it is above
    void edouble_record_floor_ceiling(ExactRecordProgram *program, unsigned int n, unsigned int d, unsigned int *floor,
                                      unsigned int *ceiling);

    // ---- held values: every value between down 2^e and up 2^e, down <= up ----

    // an exact value, both ends its mantissa
    EdoubleRecordHeld edouble_record_held_of(EdoubleRecord value);

    EdoubleRecordHeld edouble_record_held_negate(ExactRecordProgram *program, EdoubleRecordHeld value);

    // the ends' sums, exact, for exponents known to differ by less than 2^spread_bits
    EdoubleRecordHeld edouble_record_held_sum(ExactRecordProgram *program, EdoubleRecordHeld a, EdoubleRecordHeld b,
                                              unsigned int spread_bits);

    EdoubleRecordHeld edouble_record_held_difference(ExactRecordProgram *program, EdoubleRecordHeld a,
                                                     EdoubleRecordHeld b, unsigned int spread_bits);

    // the least and the most of the four corner products, exact, whatever the signs
    EdoubleRecordHeld edouble_record_held_product(ExactRecordProgram *program, EdoubleRecordHeld a, EdoubleRecordHeld b);

    // both ends cut by one shift, the larger end's: the lower end's floor and the upper end's ceiling below 2^width in
    // size, for both ends known below 2^(width + 2^range_bits - 1); each end's register given width + 2 bits
    EdoubleRecordHeld edouble_record_held_cut(ExactRecordProgram *program, EdoubleRecordHeld value, unsigned int width,
                                              unsigned int range_bits);

    // a / b for b holding no 0 and both of b's ends below 2^divisor_bits: the least floor and the most ceiling of the
    // four corners m_a 2^s / m_b, s = width + divisor_bits, at e_a - e_b - s
    EdoubleRecordHeld edouble_record_held_quotient(ExactRecordProgram *program, EdoubleRecordHeld a, EdoubleRecordHeld b,
                                                   unsigned int width, unsigned int divisor_bits);

    // 1 where every value a holds lies above every value b holds: a's lower end above b's upper, exact
    unsigned int edouble_record_held_above(ExactRecordProgram *program, EdoubleRecordHeld a, EdoubleRecordHeld b,
                                           unsigned int spread_bits);

    // value 2^power for a constant power, exact: the exponent moved
    EdoubleRecordHeld edouble_record_held_times_two_to(ExactRecordProgram *program, EdoubleRecordHeld value,
                                                       long long power);

    // the value at the constant exponent `exponent`, outward: the lower end's floor and the upper end's ceiling of
    // each end times 2^(e - exponent), exact where e is the larger, each end's register wrapped to `bits` for ends the
    // caller holds within [-2^(bits - 1), 2^(bits - 1)). Where the program does not know e, |e - exponent| lies below
    // 2^range_bits
    EdoubleRecordHeld edouble_record_held_at(ExactRecordProgram *program, EdoubleRecordHeld value, long long exponent,
                                             unsigned int bits, unsigned int range_bits);

    // ---- balls: every value within the radius of the center, the radius never below zero ----
    //
    // A ball's product costs one product for its center and three for its radius, against the four corners and their
    // order a held pair's takes. Only a read at an exponent drops bits, and it widens the radius by what it drops. Each
    // read wraps the center and narrows the radius to the widths it is given, and gives the flag `fits`, 1 where both
    // lay inside them: a run that holds every flag at 1 dropped nothing to a wrap. The quotient takes exponents the
    // program knows.

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

    // the ball read at the constant exponent `exponent`, its radius at `radius_exponent`: the center cut toward zero
    // there, the radius's ceiling there and one unit of `exponent` more where the center was cut, the center wrapped
    // to `bits` and the radius narrowed to `radius_bits`; `fits` 1 where |center| < 2^(bits - 1) and the radius is
    // below 2^radius_bits. Where the program does not know an exponent, it lies within 2^range_bits of the one read
    EdoubleRecordBall edouble_record_ball_at(ExactRecordProgram *program, EdoubleRecordBall value, long long exponent,
                                             unsigned int bits, long long radius_exponent, unsigned int radius_bits,
                                             unsigned int range_bits, unsigned int *fits);

    // a / b at the constant exponent `exponent`, its radius at `radius_exponent`, for every exponent known: the
    // center the quotient of the centers cut toward zero, and the radius the ceiling of
    // (|c_b| r_a + |c_a| r_b) / (|c_b| (|c_b| - r_b)) and one unit more; `fits` as the read gives it, and 0 where b's
    // ball holds 0
    EdoubleRecordBall edouble_record_ball_quotient(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                                   long long exponent, unsigned int bits, long long radius_exponent,
                                                   unsigned int radius_bits, unsigned int *fits);

    // the ball's lower and upper ends, exact
    EdoubleRecord edouble_record_ball_down(ExactRecordProgram *program, EdoubleRecordBall value, unsigned int spread_bits);

    EdoubleRecord edouble_record_ball_up(ExactRecordProgram *program, EdoubleRecordBall value, unsigned int spread_bits);

    // 1 where every value a holds lies above every value b holds, exact
    unsigned int edouble_record_ball_above(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                           unsigned int spread_bits);

#ifdef __cplusplus
}
#endif

#endif
