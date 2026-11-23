// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_tower_records.cu: cells, partial quotients, records and floors
#include "cellularum_laboro/cellularum_laboro.h"
#include "pi_tower_internal.h"

// the run ended with `why`, as a table passes its bound or the pool gives no cell
void pi_tower_refused(const char *why)
{
    static const char s_head[] = "pi_tower: ";
    platform_terminal_write(1, s_head, (unsigned long long)(sizeof(s_head) - 1u));
    platform_terminal_write(
        1, why,
        (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = why, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    platform_terminal_write(1, "\n", 1ULL);
    platform_process_ended(2);
}

// A cell of `count` elements of `size` bytes, on the temporary tier where `ephemeral` is 1 and on the persistent tier
// where it is 0. The run ends where the pool gives no cell [RETURNS OWNERSHIP]
void *pi_tower_taken(unsigned long long count, unsigned long long size, int ephemeral)
{
    void *const cell = platform_cell_taken(count * size, (unsigned int)platform_place_g_program_owner, ephemeral);
    if (cell == NULL)
    {
        pi_tower_refused("the pool gave no cell");
    }
    return cell;
}

// a cell that pi_tower_taken gave, given back [TAKES OWNERSHIP]
void pi_tower_given(void *cell)
{
    platform_cell_released(cell, (unsigned int)platform_place_g_program_owner);
}

// The steps of Euclid's algorithm on a pair below 2^bits, at max. Lame's bound puts them below 1.45 bits + 2, and
// 3 bits / 2 + 2 is above that
unsigned int pi_tower_euclid_bound(unsigned int bits)
{
    return ((3u * bits) / 2u) + 2u;
}

// Pi's continued fraction's partial quotients after the 3, from numerator / denominator below 1, into `quotients`,
// which holds pi_tower_euclid_bound of the denominator's bits. The answer is their count
static unsigned int pi_tower_partial_quotients(const PiWide &numerator, const PiWide &denominator, PiWide *quotients)
{
    unsigned int count = 0u;
    PiWide top = denominator;
    PiWide bottom = numerator;
    while (bottom.sign != 0)
    {
        quotients[count] = pi_tower_quotient(top, bottom);
        count += 1u;
        const PiWide rest = pi_tower_remainder(top, bottom);
        top = bottom;
        bottom = rest;
    }
    return count;
}

// The record after the record at `place`: the first step at which the turn is lower (or higher) than at `place`. The
// answer is 0 where no step reaches it or the step passes 2^turn.range
static int pi_tower_record_next(const PiTowerTurn &turn, int lowest, const PiWide &place, PiTowerRecord *record)
{
    const PiWide one = pi_tower_unsigned(1ULL);
    PiWide step;
    int found = 0;
    if (lowest != 0)
    {
        found = (pi_tower_compare(place, one) > 0) &&
                pi_tower_first_hit(turn.multiplier, turn.modulus, one, pi_tower_difference(place, one), &step);
    }
    else
    {
        const PiWide top = pi_tower_difference(turn.modulus, one);
        found = (pi_tower_compare(place, top) < 0) &&
                pi_tower_first_hit(turn.multiplier, turn.modulus, pi_tower_sum(place, one), top, &step);
    }
    if ((found == 0) || (pi_tower_compare(step, pi_tower_power_two(turn.range)) > 0))
    {
        return 0;
    }
    record->step = step;
    record->place = pi_tower_remainder(pi_tower_product(turn.multiplier, step), turn.modulus);
    return 1;
}

// The steps at which the turn is lower (or higher) than at every step since 1, to 2^turn.range. They go in one cell of
// the persistent tier that `records` names, as the turn holds them for the whole run. The call counts the records
// first, then takes the cell at that count and fills it. The answer is the count [RETURNS OWNERSHIP]
unsigned int pi_tower_records(const PiTowerTurn &turn, int lowest, PiTowerRecord **records)
{
    PiTowerRecord record;
    record.step = pi_tower_unsigned(1ULL);
    record.place = pi_tower_remainder(turn.multiplier, turn.modulus);
    const PiTowerRecord first = record;
    unsigned int count = 1u;
    while (pi_tower_record_next(turn, lowest, record.place, &record) != 0)
    {
        count += 1u;
    }
    PiTowerRecord *const filled = (PiTowerRecord *)pi_tower_taken(count, sizeof(PiTowerRecord), 0);
    filled[0] = first;
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        (void)pi_tower_record_next(turn, lowest, filled[at - 1u].place, &filled[at]);
    }
    *records = filled;
    return count;
}

// The record at the last record step below count, or the first record where none is below it. The steps rise through
// the records, and a binary search finds it
static const PiTowerRecord &pi_tower_record_before(const PiTowerRecord *records, unsigned int records_count,
                                                   const PiWide &count)
{
    unsigned int low = 0u;
    unsigned int high = records_count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (pi_tower_compare(records[middle].step, count) < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return records[(low > 0u) ? (low - 1u) : 0u];
}

// Whether the steps 0 .. count - 1 touch every cell of width cell. The count points cut the circle into count gaps of
// at max three lengths (Sos). Let u and v be the lowest and highest located steps below count. The point after point
// i is i + u where i < count - u, i - v where i >= v, and i + u - v between. A gap from p contains a whole empty cell
// exactly where (p mod cell) + gap >= 2 cell. The turn by A mod cell on cell gives p mod cell.
int pi_tower_covered(const PiTowerTurn &turn, const PiWide &cell, const PiWide &count)
{
    if (pi_tower_compare(count, pi_tower_power_two(turn.range)) > 0)
    {
        // a record past the range can be below count, and the sim does not know the gaps
        g_pi_tower_records_short = 1;
    }
    const PiTowerRecord &lowest = pi_tower_record_before(turn.lowest, turn.lowest_count, count);
    const PiTowerRecord &highest = pi_tower_record_before(turn.highest, turn.highest_count, count);
    const PiWide zero = pi_tower_unsigned(0ULL);
    const PiWide one = pi_tower_unsigned(1ULL);
    const PiWide two_cells = pi_tower_sum(cell, cell);
    const PiWide multiplier = pi_tower_remainder(turn.multiplier, cell);
    const PiWide rising = lowest.place;
    const PiWide falling = pi_tower_difference(turn.modulus, highest.place);
    const PiWide split = pi_tower_difference(count, lowest.step);
    const PiWide from[3] = {zero, highest.step, split};
    const PiWide to[3] = {split, count, highest.step};
    const PiWide gaps[3] = {rising, falling, pi_tower_sum(rising, falling)};
    for (unsigned int type = 0u; type < 3u; type += 1u)
    {
        if ((pi_tower_compare(from[type], to[type]) >= 0) || (pi_tower_compare(gaps[type], cell) <= 0))
        {
            continue;
        }
        if (pi_tower_compare(gaps[type], two_cells) >= 0)
        {
            return 0;
        }
        if (pi_tower_hit_between(multiplier, cell, from[type], to[type], pi_tower_difference(two_cells, gaps[type]),
                                 pi_tower_difference(cell, one)))
        {
            return 0;
        }
    }
    return 1;
}

// The parts of 18 decimal digits of a value at the exact width, at max. Each part takes more than 59 bits of the value
#define PI_TOWER_DECIMAL_PARTS ((unsigned int)(ORIOR_EXACT_BITS / 59ULL) + 2u)

void pi_tower_print_decimal(VerbaLinea *line, PiWide value)
{
    static unsigned long long s_parts[PI_TOWER_DECIMAL_PARTS];
    const PiWide chunk = pi_tower_unsigned(1000000000000000000ULL);
    unsigned int count = 0u;
    do
    {
        s_parts[count] = pi_tower_word(pi_tower_remainder(value, chunk));
        count += 1u;
        value = pi_tower_quotient(value, chunk);
    } while (value.sign != 0);
    mmgr_line_decimal(line, s_parts[count - 1u], 1u);
    for (unsigned int at = count - 1u; at > 0u; at -= 1u)
    {
        mmgr_line_decimal(line, s_parts[at - 1u], 18u);
    }
}

// numerator / denominator in exponent notation, four significant digits
void pi_tower_print_exponent(VerbaLinea *line, PiWide numerator, PiWide denominator)
{
    if (numerator.sign == 0)
    {
        mmgr_line_ch(line, '0');
        return;
    }
    const PiWide ten = pi_tower_unsigned(10ULL);
    long long exponent = 0LL;
    while (pi_tower_compare(numerator, pi_tower_product(denominator, ten)) >= 0)
    {
        denominator = pi_tower_product(denominator, ten);
        exponent += 1LL;
    }
    while (pi_tower_compare(numerator, denominator) < 0)
    {
        numerator = pi_tower_product(numerator, ten);
        exponent -= 1LL;
    }
    const unsigned long long digits =
        pi_tower_word(pi_tower_quotient(pi_tower_product(numerator, pi_tower_unsigned(1000ULL)), denominator));
    mmgr_line_decimal(line, digits / 1000ULL, 1u);
    mmgr_line_ch(line, '.');
    mmgr_line_decimal(line, digits % 1000ULL, 3u);
    mmgr_line_ch(line, 'e');
    mmgr_line_decimal_signed(line, exponent);
}

// pi 2^(precision + PI_TOWER_GUARD) strictly between low and high by Machin's formula. Also A = floor(alpha
// 2^precision) where both ends agree on it; 0 where they do not
static int pi_tower_machin(unsigned int precision, PiWide *alpha, PiWide *low, PiWide *high, unsigned long long *terms)
{
    const unsigned int bits = precision + PI_TOWER_GUARD;
    unsigned long long fifth_terms = 0ULL;
    unsigned long long far_terms = 0ULL;
    const PiWide fifth = pi_tower_arctan(5ULL, bits, &fifth_terms);
    const PiWide far = pi_tower_arctan(239ULL, bits, &far_terms);
    const PiWide middle = pi_tower_difference(pi_tower_product(fifth, pi_tower_unsigned(16ULL)),
                                              pi_tower_product(far, pi_tower_unsigned(4ULL)));
    const PiWide spread = pi_tower_unsigned((16ULL * (fifth_terms + 1ULL)) + (4ULL * (far_terms + 1ULL)));
    *low = pi_tower_difference(middle, spread);
    *high = pi_tower_sum(middle, spread);
    *terms = fifth_terms + far_terms;
    const PiWide guard = pi_tower_power_two(PI_TOWER_GUARD);
    const PiWide below = pi_tower_quotient(*low, guard);
    const PiWide above = pi_tower_quotient(*high, guard);
    *alpha = pi_tower_difference(below, pi_tower_product(pi_tower_unsigned(3ULL), pi_tower_power_two(precision)));
    return pi_tower_compare(below, above) == 0;
}

// 1. pi bracketed: A = floor(alpha 2^precision), and pi 2^(precision + PI_TOWER_GUARD) strictly between low and high
int pi_tower_bracket(SimResults *results, unsigned int precision, PiWide *alpha, PiWide *low, PiWide *high)
{
    const unsigned int bits = precision + PI_TOWER_GUARD;
    unsigned long long terms = 0ULL;
    const int agree = pi_tower_machin(precision, alpha, low, high, &terms);
    const unsigned long long leading = pi_tower_word(pi_tower_quotient(*alpha, pi_tower_power_two(precision - 64u)));
    mmgr_line_put(&results->line, "  pi by Machin's formula at ");
    mmgr_line_decimal(&results->line, bits, 1u);
    mmgr_line_put(&results->line, " bits, ");
    mmgr_line_decimal(&results->line, terms, 1u);
    mmgr_line_put(&results->line, " terms; after the point it begins 0x");
    for (unsigned int nibble = 16u; nibble > 0u; nibble -= 1u)
    {
        mmgr_line_ch(&results->line, "0123456789ABCDEF"[(leading >> (4u * (nibble - 1u))) & 15ULL]);
    }
    mmgr_line_ch(&results->line, '\n');
    sim_check(results, agree,
              "floor(pi 2^P) is one integer at both ends of Machin's bracket, at the turn's precision P");
    sim_check(results, leading == PI_TOWER_PUBLISHED_BITS, "pi's first 64 bits after the point are 0x243F6A8885A308D3");
    return agree;
}

// The closest returns of the turn to 2^PI_TOWER_RECORD_BITS steps, at max. The step of each return is at least the sum
// of the two steps before it. The steps thus grow at least as Fibonacci's numbers do
#define PI_TOWER_RETURNS_MAX ((4u * PI_TOWER_RECORD_BITS) + 4u)

// 2 and 3. The floors: pi's partial quotients and convergents, and the turn's closest returns. The floors go in two
// cells of the persistent tier, taken at the count that the bracket certifies: the run reads them to its end
void pi_tower_floors(SimResults *results, const PiTowerTurn &turn, PiTowerFloors *floors)
{
    const unsigned long long published[PI_TOWER_PUBLISHED_FLOORS] = {7ULL, 15ULL, 1ULL, 292ULL, 1ULL,  1ULL, 1ULL,
                                                                     2ULL, 1ULL,  3ULL, 1ULL,   14ULL, 2ULL, 1ULL,
                                                                     1ULL, 2ULL,  2ULL, 2ULL,   2ULL,  1ULL, 84ULL};
    const PiWide one = pi_tower_unsigned(1ULL);
    const unsigned int bound_quotients = pi_tower_euclid_bound(pi_tower_bit_length(turn.modulus));
    PiWide *const lower = (PiWide *)pi_tower_taken(bound_quotients, sizeof(PiWide), 1);
    PiWide *const upper = (PiWide *)pi_tower_taken(bound_quotients, sizeof(PiWide), 1);
    const unsigned int lower_count = pi_tower_partial_quotients(turn.multiplier, turn.modulus, lower);
    const unsigned int upper_count =
        pi_tower_partial_quotients(pi_tower_sum(turn.multiplier, one), turn.modulus, upper);
    unsigned int common = 0u;
    while ((common < lower_count) && (common < upper_count) && (pi_tower_compare(lower[common], upper[common]) == 0))
    {
        common += 1u;
    }
    // the last shared quotient can differ between the two expansions of a rational endpoint. The bracket does not
    // certify it
    const unsigned int certified = (common > 0u) ? (common - 1u) : 0u;
    int agree = certified >= PI_TOWER_PUBLISHED_FLOORS;
    for (unsigned int at = 0u; (at < PI_TOWER_PUBLISHED_FLOORS) && (at < certified); at += 1u)
    {
        agree = agree && (pi_tower_compare(lower[at], pi_tower_unsigned(published[at])) == 0);
    }
    floors->count = certified + 1u;
    floors->denominator = (PiWide *)pi_tower_taken(floors->count, sizeof(PiWide), 0);
    floors->quotient = (PiWide *)pi_tower_taken(floors->count, sizeof(PiWide), 0);
    PiWide before = pi_tower_unsigned(0ULL);
    PiWide denominator = one;
    floors->denominator[0] = denominator;
    floors->quotient[0] = pi_tower_unsigned(0ULL);
    for (unsigned int at = 0u; at < certified; at += 1u)
    {
        const PiWide next = pi_tower_sum(pi_tower_product(lower[at], denominator), before);
        before = denominator;
        denominator = next;
        floors->denominator[at + 1u] = denominator;
        floors->quotient[at + 1u] = lower[at];
    }
    mmgr_line_put(&results->line, "  ");
    mmgr_line_decimal(&results->line, certified, 1u);
    mmgr_line_put(&results->line, " floors certified by the bracket: pi = [3; ");
    for (unsigned int at = 0u; (at < certified) && (at < 40u); at += 1u)
    {
        mmgr_line_decimal(&results->line, pi_tower_word(lower[at]), 1u);
        mmgr_line_put(&results->line, (at + 1u < certified) ? ", " : "]\n");
    }
    if (certified > 40u)
    {
        mmgr_line_put(&results->line, "...]\n");
    }
    pi_tower_given(upper);
    pi_tower_given(lower);
    sim_check(results, agree,
              "the certified floors begin 7, 15, 1, 292, 1, 1, 1, 2, 1, 3, 1, 14, 2, 1, 1, 2, 2, 2, 2, 1, 84");
    sim_flush(results);

    // the closest returns: records of the distance to the start, by first hits from both sides
    const PiWide bound = pi_tower_power_two(PI_TOWER_RECORD_BITS);
    static PiWide s_returns[PI_TOWER_RETURNS_MAX];
    unsigned int returns = 0u;
    PiWide step = one;
    for (;;)
    {
        if (returns == PI_TOWER_RETURNS_MAX)
        {
            pi_tower_refused("the turn's closest returns pass their table");
        }
        s_returns[returns] = step;
        returns += 1u;
        const PiWide place = pi_tower_remainder(pi_tower_product(turn.multiplier, step), turn.modulus);
        const PiWide mirror = pi_tower_difference(turn.modulus, place);
        const PiWide distance = (pi_tower_compare(place, mirror) < 0) ? place : mirror;
        if (pi_tower_compare(distance, one) <= 0)
        {
            break;
        }
        PiWide from_below;
        PiWide from_above;
        const int below =
            pi_tower_first_hit(turn.multiplier, turn.modulus, one, pi_tower_difference(distance, one), &from_below);
        const int above = pi_tower_first_hit(turn.multiplier, turn.modulus,
                                             pi_tower_sum(pi_tower_difference(turn.modulus, distance), one),
                                             pi_tower_difference(turn.modulus, one), &from_above);
        if ((below == 0) && (above == 0))
        {
            break;
        }
        step = (below == 0)
                   ? from_above
                   : ((above == 0) ? from_below
                                   : ((pi_tower_compare(from_below, from_above) < 0) ? from_below : from_above));
        if (pi_tower_compare(step, bound) > 0)
        {
            break;
        }
    }
    unsigned int matched = 0u;
    unsigned int floors_in_bound = 0u;
    while ((floors_in_bound < floors->count) && (pi_tower_compare(floors->denominator[floors_in_bound], bound) <= 0))
    {
        floors_in_bound += 1u;
    }
    while ((matched < returns) && (matched < floors_in_bound) &&
           (pi_tower_compare(s_returns[matched], floors->denominator[matched]) == 0))
    {
        matched += 1u;
    }
    mmgr_line_put(&results->line, "  floor j, turns on it a_j, the step q_j of its closest return, and how close\n");
    for (unsigned int at = 0u; at < floors_in_bound; at += 1u)
    {
        const PiWide place =
            pi_tower_remainder(pi_tower_product(turn.multiplier, floors->denominator[at]), turn.modulus);
        const PiWide mirror = pi_tower_difference(turn.modulus, place);
        mmgr_line_put(&results->line, "    ");
        mmgr_line_decimal(&results->line, at, 2u);
        mmgr_line_put(&results->line, "  a ");
        pi_tower_print_decimal(&results->line, floors->quotient[at]);
        mmgr_line_put(&results->line, "  q ");
        pi_tower_print_decimal(&results->line, floors->denominator[at]);
        mmgr_line_put(&results->line, "  ||q pi|| ");
        pi_tower_print_exponent(&results->line, (pi_tower_compare(place, mirror) < 0) ? place : mirror, turn.modulus);
        mmgr_line_ch(&results->line, '\n');
        sim_flush(results);
    }
    mmgr_line_put(&results->line, "  the turn's closest returns to 2^112 steps: ");
    mmgr_line_decimal(&results->line, returns, 1u);
    mmgr_line_put(&results->line, "; the floors' q_j to 2^112: ");
    mmgr_line_decimal(&results->line, floors_in_bound, 1u);
    mmgr_line_put(&results->line, "; equal in order: ");
    mmgr_line_decimal(&results->line, matched, 1u);
    mmgr_line_ch(&results->line, '\n');
    sim_check(results, (matched == returns) && (matched == floors_in_bound) && (matched > 40u),
              "the turn's closest returns are exactly the floors' convergent denominators, to 2^112 steps");
    sim_flush(results);
}
