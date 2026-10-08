// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.cu: the record-program pieces a stored double is read into its forms by
#include "forms.h"

#include "../../../../../src/cu/types/integerfloats/edouble/edouble_record.h"

unsigned long long forms_ten(unsigned int power)
{
    unsigned long long ten = 1ull;
    for (unsigned int each = 0u; each < power; each += 1u)
    {
        ten *= 10ull;
    }
    return ten;
}

void forms_fields(ExactRecordProgram *program, FormsFields *fields)
{
    fields->fraction = exact_record_member_field(program, FORMS_MANTISSA_BITS, 0u);
    fields->exponent = exact_record_member_field(program, FORMS_EXPONENT_BITS, FORMS_MANTISSA_BITS);
}

void forms_double(ExactRecordProgram *program, const FormsFields *fields, unsigned int member, FormsDouble *read)
{
    read->fraction = exact_record_read_unsigned(program, fields->fraction, member);
    read->biased = exact_record_read_unsigned(program, fields->exponent, member);
}

// 1 where an integer is odd: v - 2 floor(v / 2)
static unsigned int forms_odd(ExactRecordProgram *program, unsigned int value)
{
    const unsigned int two = exact_record_constant(program, 2ull);
    return exact_record_difference(program, value,
                                   exact_record_product(program, two, exact_record_quotient(program, value, two)));
}

void forms_preimage(ExactRecordProgram *program, const FormsDouble *read, unsigned long long most_placed,
                    unsigned int shift_bits, FormsPreimage *preimage)
{
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int two = exact_record_constant(program, 2ull);
    const unsigned int four = exact_record_constant(program, 4ull);
    const unsigned int normal = exact_record_above(program, read->biased, zero);
    const unsigned int mantissa = exact_record_sum(
        program, read->fraction,
        exact_record_product(program, normal, exact_record_power_two(program, FORMS_MANTISSA_BITS)));
    const unsigned int placed = exact_record_sum(program, read->biased, exact_record_difference(program, one, normal));
    // the lower gap is half the upper where the fraction is 0 above the least normal exponent
    const unsigned int narrow = exact_record_product(program, exact_record_equal(program, read->fraction, zero),
                                                     exact_record_above(program, read->biased, one));
    preimage->centre = exact_record_product(program, four, mantissa);
    preimage->low = exact_record_difference(program, preimage->centre, exact_record_difference(program, two, narrow));
    preimage->high = exact_record_sum(program, preimage->centre, two);
    // closed where M is even
    preimage->odd_low = forms_odd(program, mantissa);
    preimage->odd_high = preimage->odd_low;
    const unsigned int shift = exact_record_difference(program, exact_record_constant(program, most_placed), placed);
    preimage->divisor = exact_record_two_to(
        program, exact_record_sum(program, shift, exact_record_constant(program, FORMS_LIFT - most_placed)),
        shift_bits);
}

// the integers k with k / divisor inside the preimage scaled by `scale`: the least of them, and 1 where there is one
static void forms_integer(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned int scale,
                          unsigned int *least, unsigned int *holds)
{
    const unsigned int one = exact_record_constant(program, 1ull);
    unsigned int low_floor = 0u;
    unsigned int low_ceiling = 0u;
    unsigned int high_floor = 0u;
    unsigned int high_ceiling = 0u;
    edouble_record_floor_ceiling(program, exact_record_product(program, scale, preimage->low), preimage->divisor,
                                 &low_floor, &low_ceiling);
    edouble_record_floor_ceiling(program, exact_record_product(program, scale, preimage->high), preimage->divisor,
                                 &high_floor, &high_ceiling);
    // closed: ceil(low) to floor(high); open: floor(low) + 1 to ceil(high) - 1
    *least = exact_record_select(program, preimage->odd_low, exact_record_sum(program, low_floor, one), low_ceiling);
    const unsigned int most =
        exact_record_select(program, preimage->odd_high, exact_record_difference(program, high_ceiling, one), high_floor);
    *holds = exact_record_difference(program, one, exact_record_above(program, *least, most));
}

void forms_unit(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned int places, unsigned int *held,
                unsigned int *unit)
{
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int one = exact_record_constant(program, 1ull);
    unsigned int none = one;
    unsigned int picked = zero;
    for (unsigned int place = 0u; place <= places; place += 1u)
    {
        unsigned int k_least = 0u;
        unsigned int holds = 0u;
        forms_integer(program, preimage, exact_record_constant(program, forms_ten(place)), &k_least, &holds);
        const unsigned int first = exact_record_product(program, none, holds);
        // the first p that holds, its k carried onto the unit
        picked = exact_record_sum(
            program, picked,
            exact_record_product(program, first,
                                 exact_record_product(program, k_least,
                                                      exact_record_constant(program, forms_ten(places - place)))));
        none = exact_record_product(program, none, exact_record_difference(program, one, holds));
    }
    *held = exact_record_difference(program, one, none);
    *unit = picked;
}

unsigned int forms_floor(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned int places)
{
    unsigned int floor = 0u;
    unsigned int ceiling = 0u;
    edouble_record_floor_ceiling(program,
                                 exact_record_product(program, exact_record_constant(program, forms_ten(places)),
                                                      preimage->centre),
                                 preimage->divisor, &floor, &ceiling);
    return floor;
}

// The double nearest an end of an interval, on the end's side: the least double at or above `value` where `above` is
// 1, the greatest at or below it where `above` is 0, strictly past it where `open` is 1. `value` is an integer of
// `bits_least` to `bits_least` + 2 bits on some unit, and the double comes out as M 2^s on that unit, M from 2^52 to
// 2^53 - 1
static void forms_double_at(ExactRecordProgram *program, unsigned int value, unsigned int bits_least, int above,
                            unsigned int open, unsigned int *mantissa, unsigned int *shift)
{
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int two = exact_record_constant(program, 2ull);
    const unsigned int least_mantissa = exact_record_power_two(program, FORMS_MANTISSA_BITS);
    const unsigned int width = FORMS_MANTISSA_BITS + 1u;
    // s = bits(value) - 53
    unsigned int s = exact_record_constant(program, (unsigned long long)(bits_least - width));
    for (unsigned int more = 0u; more < 2u; more += 1u)
    {
        const unsigned int edge = exact_record_difference(program, exact_record_power_two(program, bits_least + more), one);
        s = exact_record_sum(program, s, exact_record_above(program, value, edge));
    }
    unsigned int floor = 0u;
    unsigned int ceiling = 0u;
    edouble_record_floor_ceiling(program, value, exact_record_two_to(program, s, FORMS_SHIFT_BITS), &floor, &ceiling);
    if (above)
    {
        // closed: ceil(v / 2^s); open: floor(v / 2^s) + 1; 2^53 carries into the next binade
        const unsigned int raw = exact_record_select(program, open, exact_record_sum(program, floor, one), ceiling);
        const unsigned int over = exact_record_equal(program, raw, exact_record_power_two(program, width));
        *mantissa = exact_record_select(program, over, least_mantissa, raw);
        *shift = exact_record_sum(program, s, over);
    }
    else
    {
        // closed: floor(v / 2^s); open: ceil(v / 2^s) - 1; below 2^52 it falls into the binade under
        const unsigned int raw = exact_record_select(program, open, exact_record_difference(program, ceiling, one), floor);
        const unsigned int under = exact_record_difference(
            program, one, exact_record_above(program, raw, exact_record_difference(program, least_mantissa, one)));
        *mantissa =
            exact_record_select(program, under, exact_record_sum(program, exact_record_product(program, two, raw), one), raw);
        *shift = exact_record_difference(program, s, under);
    }
}

void forms_divided(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned long long base,
                   FormsPreimage *divided, unsigned int *exists)
{
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int two = exact_record_constant(program, 2ull);
    const unsigned int four = exact_record_constant(program, 4ull);
    const unsigned int scale = exact_record_constant(program, base);
    // B times the preimage's ends on u: from B (2^54 - 2) to B (2^55 + 2), bits(B) + 53 to bits(B) + 55 bits
    const unsigned int bits_least = exact_record_bits_of(base) + FORMS_MANTISSA_BITS + 1u;
    unsigned int least_mantissa = 0u;
    unsigned int least_shift = 0u;
    unsigned int most_mantissa = 0u;
    unsigned int most_shift = 0u;
    forms_double_at(program, exact_record_product(program, scale, preimage->low), bits_least, 1, preimage->odd_low,
                    &least_mantissa, &least_shift);
    forms_double_at(program, exact_record_product(program, scale, preimage->high), bits_least, 0, preimage->odd_high,
                    &most_mantissa, &most_shift);
    const unsigned int least_two = exact_record_two_to(program, least_shift, FORMS_SHIFT_BITS);
    const unsigned int most_two = exact_record_two_to(program, most_shift, FORMS_SHIFT_BITS);
    *exists = exact_record_difference(program, one,
                                      exact_record_above(program, exact_record_product(program, least_mantissa, least_two),
                                                         exact_record_product(program, most_mantissa, most_two)));
    const unsigned int narrow =
        exact_record_equal(program, least_mantissa, exact_record_power_two(program, FORMS_MANTISSA_BITS));
    // the reals rounding to D_least through D_most, on u / 4
    divided->centre = preimage->centre;
    divided->low = exact_record_product(
        program,
        exact_record_difference(program, exact_record_product(program, four, least_mantissa),
                                exact_record_difference(program, two, narrow)),
        least_two);
    divided->high = exact_record_product(
        program, exact_record_sum(program, exact_record_product(program, four, most_mantissa), two), most_two);
    divided->odd_low = forms_odd(program, least_mantissa);
    divided->odd_high = forms_odd(program, most_mantissa);
    divided->divisor = exact_record_product(program, four, preimage->divisor);
}

void forms_whole(ExactRecordProgram *program, const FormsDouble *base, unsigned int *value, unsigned int *whole)
{
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int one = exact_record_constant(program, 1ull);
    // B is 1 to 2^53 where its biased exponent is 1023 to 1075, and whole where 2^(1075 - E) divides its mantissa
    const unsigned int base_least = exact_record_constant(program, FORMS_BASE_BIASED_LEAST - 1ull);
    const unsigned int base_most = exact_record_constant(program, FORMS_BASE_BIASED_MOST);
    const unsigned int in_range = exact_record_product(
        program, exact_record_above(program, base->biased, base_least),
        exact_record_difference(program, one, exact_record_above(program, base->biased, base_most)));
    const unsigned int base_mantissa =
        exact_record_sum(program, base->fraction, exact_record_power_two(program, FORMS_MANTISSA_BITS));
    const unsigned int drop =
        exact_record_select(program, in_range, exact_record_difference(program, base_most, base->biased), zero);
    const unsigned int step = exact_record_two_to(program, drop, FORMS_SHIFT_BITS);
    *value = exact_record_quotient(program, base_mantissa, step);
    *whole = exact_record_product(
        program, in_range, exact_record_equal(program, exact_record_product(program, *value, step), base_mantissa));
}

void forms_count(ExactRecordProgram *program, const FormsPreimage *preimage, const FormsDouble *base,
                 unsigned int *held, unsigned int *count)
{
    unsigned int whole_base = 0u;
    unsigned int whole = 0u;
    forms_whole(program, base, &whole_base, &whole);
    unsigned int least = 0u;
    unsigned int found = 0u;
    forms_integer(program, preimage, whole_base, &least, &found);
    *held = exact_record_product(program, whole, found);
    *count = exact_record_product(program, *held, least);
}

unsigned int forms_contains(ExactRecordProgram *program, const FormsPreimage *preimage, unsigned int scale,
                            unsigned int value)
{
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int placed = exact_record_product(program, value, preimage->divisor);
    const unsigned int low = exact_record_product(program, preimage->low, scale);
    const unsigned int high = exact_record_product(program, preimage->high, scale);
    // above the low end, or on it where it is closed; below the high end, or on it where it is closed
    const unsigned int past_low = exact_record_sum(
        program, exact_record_above(program, placed, low),
        exact_record_product(program, exact_record_equal(program, placed, low),
                             exact_record_difference(program, one, preimage->odd_low)));
    const unsigned int short_of_high = exact_record_sum(
        program, exact_record_above(program, high, placed),
        exact_record_product(program, exact_record_equal(program, placed, high),
                             exact_record_difference(program, one, preimage->odd_high)));
    return exact_record_product(program, past_low, short_of_high);
}
