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
// or the shift lies within; the widths keymath imprints follow from them.

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

#ifdef __cplusplus
}
#endif

#endif
