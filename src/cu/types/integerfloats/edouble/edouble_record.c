// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "edouble_record.h"

EdoubleRecord edouble_record_of(unsigned int mantissa, unsigned int exponent)
{
    EdoubleRecord value = {mantissa, exponent};
    return value;
}

EdoubleRecord edouble_record_constant(ExactRecordProgram *program, long long mantissa, long long exponent)
{
    return edouble_record_of(exact_record_signed(program, mantissa), exact_record_signed(program, exponent));
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

// the lift of the larger exponent's side, 2^|e_a - e_b|, and 1 where a's exponent is the larger or the two are equal;
// where the program knows both exponents, the lift is a known power and `side` is 1 or 0 alike in every lane
static void edouble_record_line_up(ExactRecordProgram *program, unsigned int a_exponent, unsigned int b_exponent,
                                   unsigned int spread_bits, unsigned int *lift, unsigned int *a_larger, int *known_side)
{
    long long a_known = 0ll;
    long long b_known = 0ll;
    if (exact_record_known(program, a_exponent, &a_known) && exact_record_known(program, b_exponent, &b_known))
    {
        const long long spread = a_known - b_known;
        *lift = exact_record_power_two(program, (unsigned int)((spread < 0ll) ? -spread : spread));
        *a_larger = exact_record_constant(program, (spread >= 0ll) ? 1ull : 0ull);
        *known_side = (spread >= 0ll) ? 1 : 0;
        return;
    }
    const unsigned int spread = exact_record_difference(program, a_exponent, b_exponent);
    *a_larger = exact_record_difference(program, exact_record_constant(program, 1ull),
                                        exact_record_above(program, b_exponent, a_exponent));
    *lift = exact_record_two_to(program, exact_record_absolute(program, spread), spread_bits);
    *known_side = -1;
}

// a + b's mantissa at the smaller exponent, one side lifted: only the side the program knows where it knows it
static unsigned int edouble_record_lined(ExactRecordProgram *program, unsigned int a, unsigned int b, unsigned int lift,
                                         unsigned int a_larger, int known_side)
{
    if (known_side == 1)
    {
        return exact_record_sum(program, exact_record_product(program, a, lift), b);
    }
    if (known_side == 0)
    {
        return exact_record_sum(program, a, exact_record_product(program, b, lift));
    }
    const unsigned int a_lifted = exact_record_sum(program, exact_record_product(program, a, lift), b);
    const unsigned int b_lifted = exact_record_sum(program, a, exact_record_product(program, b, lift));
    return exact_record_select(program, a_larger, a_lifted, b_lifted);
}

EdoubleRecord edouble_record_sum(ExactRecordProgram *program, EdoubleRecord a, EdoubleRecord b, unsigned int spread_bits)
{
    unsigned int lift = 0u;
    unsigned int a_larger = 0u;
    int known_side = -1;
    edouble_record_line_up(program, a.exponent, b.exponent, spread_bits, &lift, &a_larger, &known_side);
    return edouble_record_of(edouble_record_lined(program, a.mantissa, b.mantissa, lift, a_larger, known_side),
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

// k, the largest below 2^range_bits with magnitude >= 2^(width - 1 + k), or 0, found from its top bit down, and 2^k:
// the magnitude is then below 2^(width + k)
static void edouble_record_shift(ExactRecordProgram *program, unsigned int magnitude, unsigned int width,
                                 unsigned int range_bits, unsigned int *shift, unsigned int *power)
{
    const unsigned int half = exact_record_power_two(program, width - 1u);
    const unsigned int one = exact_record_constant(program, 1ull);
    *shift = exact_record_constant(program, 0ull);
    *power = one;
    for (unsigned int bit = range_bits; bit > 0u; bit -= 1u)
    {
        const unsigned int step = 1u << (bit - 1u);
        const unsigned int tried = exact_record_product(program, *power, exact_record_power_two(program, step));
        const unsigned int reached = exact_record_above(
            program, magnitude, exact_record_difference(program, exact_record_product(program, half, tried), one));
        *shift = exact_record_sum(program, *shift, exact_record_product(program, reached, exact_record_constant(program, step)));
        *power = exact_record_select(program, reached, tried, *power);
    }
}

EdoubleRecordHeld edouble_record_cut(ExactRecordProgram *program, EdoubleRecord value, unsigned int width,
                                     unsigned int range_bits)
{
    return edouble_record_held_cut(program, edouble_record_held_of(value), width, range_bits);
}

EdoubleRecordHeld edouble_record_held_of(EdoubleRecord value)
{
    EdoubleRecordHeld held = {value.mantissa, value.mantissa, value.exponent};
    return held;
}

EdoubleRecordHeld edouble_record_held_negate(ExactRecordProgram *program, EdoubleRecordHeld value)
{
    EdoubleRecordHeld held = {exact_record_negate(program, value.up), exact_record_negate(program, value.down),
                              value.exponent};
    return held;
}

EdoubleRecordHeld edouble_record_held_sum(ExactRecordProgram *program, EdoubleRecordHeld a, EdoubleRecordHeld b,
                                          unsigned int spread_bits)
{
    unsigned int lift = 0u;
    unsigned int a_larger = 0u;
    int known_side = -1;
    edouble_record_line_up(program, a.exponent, b.exponent, spread_bits, &lift, &a_larger, &known_side);
    EdoubleRecordHeld held = {edouble_record_lined(program, a.down, b.down, lift, a_larger, known_side),
                              edouble_record_lined(program, a.up, b.up, lift, a_larger, known_side),
                              exact_record_select(program, a_larger, b.exponent, a.exponent)};
    return held;
}

EdoubleRecordHeld edouble_record_held_difference(ExactRecordProgram *program, EdoubleRecordHeld a,
                                                 EdoubleRecordHeld b, unsigned int spread_bits)
{
    return edouble_record_held_sum(program, a, edouble_record_held_negate(program, b), spread_bits);
}

// the least and the most of the four corner products, whatever the signs
EdoubleRecordHeld edouble_record_held_product(ExactRecordProgram *program, EdoubleRecordHeld a, EdoubleRecordHeld b)
{
    const unsigned int corner[4] = {
        exact_record_product(program, a.down, b.down), exact_record_product(program, a.down, b.up),
        exact_record_product(program, a.up, b.down), exact_record_product(program, a.up, b.up)};
    EdoubleRecordHeld held;
    held.down = exact_record_min(program, exact_record_min(program, corner[0], corner[1]),
                                 exact_record_min(program, corner[2], corner[3]));
    held.up = exact_record_max(program, exact_record_max(program, corner[0], corner[1]),
                               exact_record_max(program, corner[2], corner[3]));
    held.exponent = exact_record_sum(program, a.exponent, b.exponent);
    return held;
}

EdoubleRecordHeld edouble_record_held_cut(ExactRecordProgram *program, EdoubleRecordHeld value, unsigned int width,
                                          unsigned int range_bits)
{
    const unsigned int magnitude = exact_record_max(program, exact_record_absolute(program, value.down),
                                                    exact_record_absolute(program, value.up));
    unsigned int shift = 0u;
    unsigned int power = 0u;
    edouble_record_shift(program, magnitude, width, range_bits, &shift, &power);
    // both ends below 2^(width + k) in size, the floor of the lower and the ceiling of the upper over 2^k within
    // [-2^width, 2^width]
    unsigned int down = 0u;
    unsigned int up = 0u;
    unsigned int unused = 0u;
    edouble_record_floor_ceiling(program, value.down, power, &down, &unused);
    edouble_record_floor_ceiling(program, value.up, power, &unused, &up);
    EdoubleRecordHeld held;
    held.down = exact_record_wrap(program, down, width + 2u);
    held.up = exact_record_wrap(program, up, width + 2u);
    held.exponent = exact_record_sum(program, value.exponent, shift);
    return held;
}

// over a divisor that holds no 0 the quotient is monotone in each operand, and its least and most lie at the corners
EdoubleRecordHeld edouble_record_held_quotient(ExactRecordProgram *program, EdoubleRecordHeld a, EdoubleRecordHeld b,
                                               unsigned int width, unsigned int divisor_bits)
{
    const unsigned int lift = width + divisor_bits;
    const unsigned int scale = exact_record_power_two(program, lift);
    const unsigned int numerator[2] = {exact_record_product(program, a.down, scale),
                                       exact_record_product(program, a.up, scale)};
    const unsigned int divisor[2] = {b.down, b.up};
    unsigned int floor[4];
    unsigned int ceiling[4];
    for (unsigned int corner = 0u; corner < 4u; corner += 1u)
    {
        edouble_record_floor_ceiling(program, numerator[corner / 2u], divisor[corner % 2u], &floor[corner],
                                     &ceiling[corner]);
    }
    EdoubleRecordHeld held;
    held.down = exact_record_min(program, exact_record_min(program, floor[0], floor[1]),
                                 exact_record_min(program, floor[2], floor[3]));
    held.up = exact_record_max(program, exact_record_max(program, ceiling[0], ceiling[1]),
                               exact_record_max(program, ceiling[2], ceiling[3]));
    held.exponent = exact_record_difference(program, exact_record_difference(program, a.exponent, b.exponent),
                                            exact_record_constant(program, (unsigned long long)lift));
    return held;
}

unsigned int edouble_record_held_above(ExactRecordProgram *program, EdoubleRecordHeld a, EdoubleRecordHeld b,
                                       unsigned int spread_bits)
{
    return edouble_record_above(program, edouble_record_of(a.down, a.exponent), edouble_record_of(b.up, b.exponent),
                                spread_bits);
}

EdoubleRecordHeld edouble_record_held_times_two_to(ExactRecordProgram *program, EdoubleRecordHeld value, long long power)
{
    EdoubleRecordHeld held = {value.down, value.up,
                              exact_record_sum(program, value.exponent, exact_record_signed(program, power))};
    return held;
}

EdoubleRecordHeld edouble_record_held_at(ExactRecordProgram *program, EdoubleRecordHeld value, long long exponent,
                                         unsigned int bits, unsigned int range_bits)
{
    const unsigned int target = exact_record_signed(program, exponent);
    unsigned int down = 0u;
    unsigned int up = 0u;
    unsigned int unused = 0u;
    long long known = 0ll;
    if (exact_record_known(program, value.exponent, &known))
    {
        const long long spread = known - exponent;
        const unsigned int power = exact_record_power_two(program, (unsigned int)((spread < 0ll) ? -spread : spread));
        if (spread >= 0ll)
        {
            down = exact_record_product(program, value.down, power);
            up = exact_record_product(program, value.up, power);
        }
        else
        {
            edouble_record_floor_ceiling(program, value.down, power, &down, &unused);
            edouble_record_floor_ceiling(program, value.up, power, &unused, &up);
        }
    }
    else
    {
        // the lane's own spread: the ends lifted where it is 0 or more, and their floor and ceiling below it
        const unsigned int spread = exact_record_difference(program, value.exponent, target);
        const unsigned int lifting = exact_record_difference(program, exact_record_constant(program, 1ull),
                                                             exact_record_above(program, target, value.exponent));
        const unsigned int power = exact_record_two_to(program, exact_record_absolute(program, spread), range_bits);
        unsigned int floor = 0u;
        unsigned int ceiling = 0u;
        edouble_record_floor_ceiling(program, value.down, power, &floor, &unused);
        edouble_record_floor_ceiling(program, value.up, power, &unused, &ceiling);
        down = exact_record_select(program, lifting, exact_record_product(program, value.down, power), floor);
        up = exact_record_select(program, lifting, exact_record_product(program, value.up, power), ceiling);
    }
    EdoubleRecordHeld held = {exact_record_wrap(program, down, bits), exact_record_wrap(program, up, bits), target};
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

// ---- balls ----

EdoubleRecordBall edouble_record_ball_of(EdoubleRecord center, EdoubleRecord radius)
{
    EdoubleRecordBall ball = {center, radius};
    return ball;
}

EdoubleRecordBall edouble_record_ball_exact(ExactRecordProgram *program, EdoubleRecord value)
{
    return edouble_record_ball_of(value, edouble_record_of(exact_record_constant(program, 0ull), value.exponent));
}

EdoubleRecordBall edouble_record_ball_negate(ExactRecordProgram *program, EdoubleRecordBall value)
{
    return edouble_record_ball_of(edouble_record_negate(program, value.center), value.radius);
}

EdoubleRecordBall edouble_record_ball_sum(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                          unsigned int spread_bits)
{
    return edouble_record_ball_of(edouble_record_sum(program, a.center, b.center, spread_bits),
                                  edouble_record_sum(program, a.radius, b.radius, spread_bits));
}

EdoubleRecordBall edouble_record_ball_difference(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                                 unsigned int spread_bits)
{
    return edouble_record_ball_sum(program, a, edouble_record_ball_negate(program, b), spread_bits);
}

static EdoubleRecord edouble_record_size(ExactRecordProgram *program, EdoubleRecord value)
{
    return edouble_record_of(exact_record_absolute(program, value.mantissa), value.exponent);
}

// x y - c_a c_b = c_a (y - c_b) + c_b (x - c_a) + (x - c_a)(y - c_b), each part within its bound
EdoubleRecordBall edouble_record_ball_product(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                              unsigned int spread_bits)
{
    const EdoubleRecord first = edouble_record_product(program, edouble_record_size(program, a.center), b.radius);
    const EdoubleRecord second = edouble_record_product(program, edouble_record_size(program, b.center), a.radius);
    const EdoubleRecord third = edouble_record_product(program, a.radius, b.radius);
    const EdoubleRecord radius =
        edouble_record_sum(program, edouble_record_sum(program, first, second, spread_bits), third, spread_bits);
    return edouble_record_ball_of(edouble_record_product(program, a.center, b.center), radius);
}

EdoubleRecordBall edouble_record_ball_times_two_to(ExactRecordProgram *program, EdoubleRecordBall value, long long power)
{
    const unsigned int moved = exact_record_signed(program, power);
    return edouble_record_ball_of(
        edouble_record_of(value.center.mantissa, exact_record_sum(program, value.center.exponent, moved)),
        edouble_record_of(value.radius.mantissa, exact_record_sum(program, value.radius.exponent, moved)));
}

// a mantissa at exponent e read at the constant exponent `exponent`: lifted exact where e is the larger, else its
// quotient toward zero, or its ceiling where `ceiling` is set; `cut` 1 where it was divided, a register
static unsigned int edouble_record_read_at(ExactRecordProgram *program, EdoubleRecord value, long long exponent,
                                           unsigned int range_bits, int ceiling, unsigned int *cut)
{
    long long known = 0ll;
    unsigned int unused = 0u;
    unsigned int up = 0u;
    if (exact_record_known(program, value.exponent, &known))
    {
        const long long spread = known - exponent;
        const unsigned int power = exact_record_power_two(program, (unsigned int)((spread < 0ll) ? -spread : spread));
        *cut = exact_record_constant(program, (spread < 0ll) ? 1ull : 0ull);
        if (spread >= 0ll)
        {
            return exact_record_product(program, value.mantissa, power);
        }
        if (!ceiling)
        {
            return exact_record_quotient(program, value.mantissa, power);
        }
        edouble_record_floor_ceiling(program, value.mantissa, power, &unused, &up);
        return up;
    }
    const unsigned int target = exact_record_signed(program, exponent);
    const unsigned int lifting = exact_record_difference(program, exact_record_constant(program, 1ull),
                                                         exact_record_above(program, target, value.exponent));
    const unsigned int power = exact_record_two_to(
        program, exact_record_absolute(program, exact_record_difference(program, value.exponent, target)), range_bits);
    *cut = exact_record_difference(program, exact_record_constant(program, 1ull), lifting);
    unsigned int divided = 0u;
    if (!ceiling)
    {
        divided = exact_record_quotient(program, value.mantissa, power);
    }
    else
    {
        edouble_record_floor_ceiling(program, value.mantissa, power, &unused, &divided);
    }
    return exact_record_select(program, lifting, exact_record_product(program, value.mantissa, power), divided);
}

// the center's unit of `exponent` at `radius_exponent`, where the center was cut, added to the radius; the two checks;
// the center wrapped and the radius narrowed
static EdoubleRecordBall edouble_record_ball_close(ExactRecordProgram *program, unsigned int center, unsigned int radius,
                                                   unsigned int cut, long long exponent, unsigned int bits,
                                                   long long radius_exponent, unsigned int radius_bits,
                                                   unsigned int *fits)
{
    const unsigned int unit = (exponent >= radius_exponent)
                                  ? exact_record_power_two(program, (unsigned int)(exponent - radius_exponent))
                                  : exact_record_constant(program, 1ull);
    const unsigned int widened = exact_record_sum(program, radius, exact_record_product(program, cut, unit));
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int center_fits = exact_record_difference(
        program, one,
        exact_record_above(program, exact_record_absolute(program, center), exact_record_mask(program, bits - 1u)));
    const unsigned int radius_fits = exact_record_difference(
        program, one, exact_record_above(program, widened, exact_record_mask(program, radius_bits)));
    *fits = exact_record_product(program, center_fits, radius_fits);
    return edouble_record_ball_of(
        edouble_record_of(exact_record_wrap(program, center, bits), exact_record_signed(program, exponent)),
        edouble_record_of(exact_record_narrow(program, widened, radius_bits), exact_record_signed(program, radius_exponent)));
}

EdoubleRecordBall edouble_record_ball_at(ExactRecordProgram *program, EdoubleRecordBall value, long long exponent,
                                         unsigned int bits, long long radius_exponent, unsigned int radius_bits,
                                         unsigned int range_bits, unsigned int *fits)
{
    unsigned int cut = 0u;
    unsigned int unused = 0u;
    const unsigned int center = edouble_record_read_at(program, value.center, exponent, range_bits, 0, &cut);
    const unsigned int radius = edouble_record_read_at(program, value.radius, radius_exponent, range_bits, 1, &unused);
    return edouble_record_ball_close(program, center, radius, cut, exponent, bits, radius_exponent, radius_bits, fits);
}

// the quotient n 2^(e_n - e_d - exponent) / d of known exponents, toward zero or its ceiling
static unsigned int edouble_record_ratio_at(ExactRecordProgram *program, EdoubleRecord n, EdoubleRecord d,
                                            long long exponent, int ceiling)
{
    long long e_n = 0ll;
    long long e_d = 0ll;
    if (!exact_record_known(program, n.exponent, &e_n) || !exact_record_known(program, d.exponent, &e_d))
    {
        program->failed = 1;
        return 0u;
    }
    const long long shift = e_n - e_d - exponent;
    const unsigned int power = exact_record_power_two(program, (unsigned int)((shift < 0ll) ? -shift : shift));
    const unsigned int top = (shift >= 0ll) ? exact_record_product(program, n.mantissa, power) : n.mantissa;
    const unsigned int bottom = (shift >= 0ll) ? d.mantissa : exact_record_product(program, d.mantissa, power);
    if (!ceiling)
    {
        return exact_record_quotient(program, top, bottom);
    }
    unsigned int unused = 0u;
    unsigned int up = 0u;
    edouble_record_floor_ceiling(program, top, bottom, &unused, &up);
    return up;
}

// |x / y - c_a / c_b| = |x c_b - c_a y| / |y c_b| <= (|c_b| r_a + |c_a| r_b) / (|c_b| (|c_b| - r_b)) for |c_b| > r_b
EdoubleRecordBall edouble_record_ball_quotient(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                               long long exponent, unsigned int bits, long long radius_exponent,
                                               unsigned int radius_bits, unsigned int *fits)
{
    const EdoubleRecord size_a = edouble_record_size(program, a.center);
    const EdoubleRecord size_b = edouble_record_size(program, b.center);
    const EdoubleRecord margin = edouble_record_difference(program, size_b, b.radius, 1u);
    // b's ball holds no 0 where |c_b| - r_b is above 0; where it holds 0 every divisor is 1 and the flag is 0
    const unsigned int clear = exact_record_above(program, margin.mantissa, exact_record_constant(program, 0ull));
    const unsigned int one = exact_record_constant(program, 1ull);
    const EdoubleRecord divisor = edouble_record_of(exact_record_select(program, clear, b.center.mantissa, one), b.center.exponent);
    const EdoubleRecord numerator = edouble_record_sum(program, edouble_record_product(program, size_b, a.radius),
                                                       edouble_record_product(program, size_a, b.radius), 1u);
    const EdoubleRecord product = edouble_record_product(program, size_b, margin);
    const EdoubleRecord denominator = edouble_record_of(exact_record_select(program, clear, product.mantissa, one), product.exponent);
    const unsigned int center = edouble_record_ratio_at(program, a.center, divisor, exponent, 0);
    const unsigned int radius = edouble_record_ratio_at(program, numerator, denominator, radius_exponent, 1);
    unsigned int checked = 0u;
    const EdoubleRecordBall ball = edouble_record_ball_close(program, center, radius, one, exponent, bits, radius_exponent,
                                                             radius_bits, &checked);
    *fits = exact_record_product(program, checked, clear);
    return ball;
}

EdoubleRecord edouble_record_ball_down(ExactRecordProgram *program, EdoubleRecordBall value, unsigned int spread_bits)
{
    return edouble_record_difference(program, value.center, value.radius, spread_bits);
}

EdoubleRecord edouble_record_ball_up(ExactRecordProgram *program, EdoubleRecordBall value, unsigned int spread_bits)
{
    return edouble_record_sum(program, value.center, value.radius, spread_bits);
}

unsigned int edouble_record_ball_above(ExactRecordProgram *program, EdoubleRecordBall a, EdoubleRecordBall b,
                                       unsigned int spread_bits)
{
    return edouble_record_above(program, edouble_record_ball_down(program, a, spread_bits),
                                edouble_record_ball_up(program, b, spread_bits), spread_bits);
}
