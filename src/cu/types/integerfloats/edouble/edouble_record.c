// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "edouble_record.h"

// a signed constant: its magnitude, negated below zero
static unsigned int edouble_record_signed(ExactRecordProgram *program, long long value)
{
    const unsigned long long magnitude = (value < 0) ? (0ull - (unsigned long long)value) : (unsigned long long)value;
    const unsigned int held = exact_record_constant(program, magnitude);
    return (value < 0) ? exact_record_negate(program, held) : held;
}

EdoubleRecord edouble_record_of(unsigned int mantissa, unsigned int exponent)
{
    EdoubleRecord value = {mantissa, exponent};
    return value;
}

EdoubleRecord edouble_record_constant(ExactRecordProgram *program, long long mantissa, long long exponent)
{
    return edouble_record_of(edouble_record_signed(program, mantissa), edouble_record_signed(program, exponent));
}

EdoubleRecord edouble_record_negate(ExactRecordProgram *program, EdoubleRecord value)
{
    return edouble_record_of(exact_record_negate(program, value.mantissa), value.exponent);
}

EdoubleRecord edouble_record_product(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b)
{
    return edouble_record_of(exact_record_product(program, a.mantissa, b.mantissa),
                             exact_record_sum(program, a.exponent, b.exponent));
}

EdoubleRecord edouble_record_sum(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b, unsigned int spread_bits)
{
    const unsigned int spread = exact_record_difference(program, a.exponent, b.exponent);
    // 1 where a's exponent is the larger or the two are equal
    const unsigned int a_larger = exact_record_difference(program, exact_record_constant(program, 1ull),
                                                          exact_record_above(program, b.exponent, a.exponent));
    const unsigned int lift = exact_record_two_to(program, exact_record_absolute(program, spread), spread_bits);
    const unsigned int a_lifted = exact_record_sum(program, exact_record_product(program, a.mantissa, lift), b.mantissa);
    const unsigned int b_lifted = exact_record_sum(program, a.mantissa, exact_record_product(program, b.mantissa, lift));
    return edouble_record_of(exact_record_select(program, a_larger, a_lifted, b_lifted),
                             exact_record_select(program, a_larger, b.exponent, a.exponent));
}

EdoubleRecord edouble_record_difference(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b,
                                        unsigned int spread_bits)
{
    return edouble_record_sum(program, a, edouble_record_negate(program, b), spread_bits);
}

unsigned int edouble_record_above(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b, unsigned int spread_bits)
{
    const EdoubleRecord apart = edouble_record_difference(program, a, b, spread_bits);
    return exact_record_above(program, apart.mantissa, exact_record_constant(program, 0ull));
}

void edouble_record_floor_ceiling(ExactRecordProgram *program, unsigned int n, unsigned int d, unsigned int *floor,
                                  unsigned int *ceiling)
{
    const unsigned int quotient = exact_record_quotient(program, n, d);
    const unsigned int remainder = exact_record_remainder(program, n, d);
    // the sign of the remainder over d, -1, 0 or 1: the part the quotient dropped toward zero
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int dropped =
        exact_record_product(program, exact_record_step(program, ENGINE_RECORD_COMPARE, remainder, zero, 0u),
                             exact_record_step(program, ENGINE_RECORD_COMPARE, d, zero, 0u));
    *floor = exact_record_difference(program, quotient, exact_record_above(program, zero, dropped));
    *ceiling = exact_record_sum(program, quotient, exact_record_above(program, dropped, zero));
}

EdoubleRecordHeld edouble_record_cut(ExactRecordProgram *program, EdoubleRecord value, unsigned int width,
                                     unsigned int range_bits)
{
    const unsigned int magnitude = exact_record_absolute(program, value.mantissa);
    const unsigned int half = exact_record_power_two(program, width - 1u);
    const unsigned int one = exact_record_constant(program, 1ull);
    // k, the largest below 2^range_bits with |m| >= 2^(width - 1 + k), or 0, found from its top bit down, and 2^k
    unsigned int shift = exact_record_constant(program, 0ull);
    unsigned int power = one;
    for (unsigned int bit = range_bits; bit > 0u; bit -= 1u)
    {
        const unsigned int step = 1u << (bit - 1u);
        const unsigned int tried = exact_record_product(program, power, exact_record_power_two(program, step));
        const unsigned int reached = exact_record_above(
            program, magnitude, exact_record_difference(program, exact_record_product(program, half, tried), one));
        shift = exact_record_sum(program, shift, exact_record_product(program, reached, exact_record_constant(program, step)));
        power = exact_record_select(program, reached, tried, power);
    }
    // |m| < 2^(width + k), and its floor and ceiling over 2^k lie in [-2^width, 2^width]
    EdoubleRecordHeld held;
    unsigned int down = 0u;
    unsigned int up = 0u;
    edouble_record_floor_ceiling(program, value.mantissa, power, &down, &up);
    held.down = exact_record_wrap(program, down, width + 2u);
    held.up = exact_record_wrap(program, up, width + 2u);
    held.exponent = exact_record_sum(program, value.exponent, shift);
    return held;
}

EdoubleRecordHeld edouble_record_quotient(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b,
                                          unsigned int width, unsigned int divisor_bits)
{
    const unsigned int lift = width + divisor_bits;
    const unsigned int lifted = exact_record_product(program, a.mantissa, exact_record_power_two(program, lift));
    EdoubleRecordHeld held;
    edouble_record_floor_ceiling(program, lifted, b.mantissa, &held.down, &held.up);
    held.exponent = exact_record_difference(program, exact_record_difference(program, a.exponent, b.exponent),
                                            exact_record_constant(program, (unsigned long long)lift));
    return held;
}
