// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_tower_billiard.cu: word arithmetic and the billiard
#include "pi_tower_internal.h"

// words += increment over PI_TOWER_WORDS. The answer is the carry out of the top word
static unsigned long long pi_tower_words_add(unsigned long long *words, const unsigned long long *increment)
{
    unsigned long long carry = 0ULL;
    for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
    {
        const unsigned long long added = words[word] + increment[word];
        const unsigned long long total = added + carry;
        carry = ((added < words[word]) || (total < added)) ? 1ULL : 0ULL;
        words[word] = total;
    }
    return carry;
}

// -1, 0 or 1 as left is below, equal to or above right
static int pi_tower_words_compare(const unsigned long long *left, const unsigned long long *right)
{
    for (unsigned int word = PI_TOWER_WORDS; word > 0u; word -= 1u)
    {
        if (left[word - 1u] != right[word - 1u])
        {
            return (left[word - 1u] < right[word - 1u]) ? -1 : 1;
        }
    }
    return 0;
}

static int pi_tower_words_zero(const unsigned long long *words)
{
    for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
    {
        if (words[word] != 0ULL)
        {
            return 0;
        }
    }
    return 1;
}

// 2^(64 PI_TOWER_WORDS) - words, the distance up to the next whole
static void pi_tower_words_complement(const unsigned long long *words, unsigned long long *complement)
{
    unsigned long long carry = 1ULL;
    for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
    {
        const unsigned long long flipped = ~words[word];
        complement[word] = flipped + carry;
        carry = ((carry != 0ULL) && (complement[word] == 0ULL)) ? 1ULL : 0ULL;
    }
}

static PiWide pi_tower_words_wide(const unsigned long long *words)
{
    PiWide value;
    orior_exact_zero(&value);
    for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
    {
        // the low and high halves of a 64-bit word each fit one 32-bit limb
        value.limb[2u * word] = (uint32_t)(words[word] & 0xFFFFFFFFULL);
        // the high half, shifted down, is below 2^32
        value.limb[(2u * word) + 1u] = (uint32_t)(words[word] >> 32u);
    }
    value.sign = (pi_tower_words_zero(words) != 0) ? 0 : 1;
    return value;
}

// 10. The billiard from the corner at slope pi, walked through its unfolded lattice. Column i contains the cells
// j = floor(pi i) .. floor(pi (i + 1)), in the order that the ball runs them. Each cell is one segment between walls.
// Each floor is 3 n + floor(alpha n), read from the carries of the integer turn.
void pi_tower_billiard(SimResults *results, const PiWide &alpha, const PiTowerFloors *floors)
{
    unsigned long long increment[PI_TOWER_WORDS];
    unsigned long long twice[PI_TOWER_WORDS];
    unsigned long long single[PI_TOWER_WORDS];
    unsigned long long odd[PI_TOWER_WORDS];
    unsigned long long nearest[PI_TOWER_WORDS];
    unsigned long long closest[PI_TOWER_WORDS];
    for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
    {
        increment[word] =
            ((unsigned long long)alpha.limb[(2u * word) + 1u] << 32u) | (unsigned long long)alpha.limb[2u * word];
        single[word] = 0ULL;
        odd[word] = increment[word];
        nearest[word] = ~0ULL;
        closest[word] = ~0ULL;
        twice[word] = increment[word];
    }
    const unsigned long long twice_carry = pi_tower_words_add(twice, increment);
    const unsigned long long columns = 1ULL << PI_TOWER_BILLIARD_BITS;
    // floor(alpha i) and floor(alpha (2i + 1)); A is below 2^P. Both start at 0
    unsigned long long single_integer = 0ULL;
    unsigned long long odd_integer = 0ULL;
    unsigned long long segments = 0ULL;
    unsigned long long vertical = 0ULL;
    unsigned long long horizontal = 0ULL;
    unsigned long long corners = 0ULL;
    unsigned long long uncertain = 0ULL;
    unsigned long long zero = 0ULL;
    unsigned long long changes = 0ULL;
    unsigned long long run = 0ULL;
    unsigned long long longest = 0ULL;
    unsigned long long ahead = 0ULL;
    unsigned long long nearest_column = 0ULL;
    unsigned long long nearest_row = 0ULL;
    int sign_before = 0;
    // The record near-corners: each is nearer a corner than every wall hit before it. Their columns are the
    // convergents' denominators to 2^PI_TOWER_BILLIARD_BITS, which grow at least as Fibonacci's numbers do
    unsigned long long near_corners[PI_TOWER_NEAR_CORNERS_MAX];
    unsigned int near_count = 0u;
    for (unsigned long long column = 0ULL; column < columns; column += 1ULL)
    {
        unsigned long long next[PI_TOWER_WORDS];
        for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
        {
            next[word] = single[word];
        }
        const unsigned long long next_integer = single_integer + pi_tower_words_add(next, increment);
        // The real n alpha 2^P lies in (n A, n A + n). Where the top word is short of all ones, n < 2^320 cannot carry
        // it past a whole. The floor read from the carries is then the real floor
        uncertain += ((next[PI_TOWER_WORDS - 1u] == ~0ULL) || (odd[PI_TOWER_WORDS - 1u] == ~0ULL)) ? 1ULL : 0ULL;
        corners += (pi_tower_words_zero(next) != 0) ? 1ULL : 0ULL;
        zero += (pi_tower_words_zero(odd) != 0) ? 1ULL : 0ULL;
        const unsigned long long bottom = (3ULL * column) + single_integer;
        const unsigned long long top = (3ULL * (column + 1ULL)) + next_integer;
        for (unsigned long long row = bottom; row <= top; row += 1ULL)
        {
            // L = (-1)^(i + j) (d - alpha (2i + 1)) / 2, d = (2j + 1) - 3 (2i + 1) whole; the rows and 3 (2i + 1)
            // stay below 2^27 at 2^24 columns. Each fits a signed word
            const long long range = (long long)((2ULL * row) + 1ULL) - (long long)(3ULL * ((2ULL * column) + 1ULL));
            // alpha (2i + 1) lies strictly between odd_integer and odd_integer + 1. D is above it exactly when
            // d > odd_integer, which fits a signed word for the same reason
            const int above = range > (long long)odd_integer;
            // the parity of i + j, one bit, fits an int
            const int positive = above ^ (int)((column + row) & 1ULL);
            if ((segments != 0ULL) && (positive != sign_before))
            {
                changes += 1ULL;
                longest = (run > longest) ? run : longest;
                run = 0ULL;
            }
            run += 1ULL;
            ahead += (positive != 0) ? 1ULL : 0ULL;
            sign_before = positive;
            segments += 1ULL;
            // the nearest tie: |d - alpha (2i + 1)| below 1 when d is odd_integer or odd_integer + 1
            unsigned long long distance[PI_TOWER_WORDS];
            int near = 0;
            if (range == (long long)odd_integer)
            {
                for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
                {
                    distance[word] = odd[word];
                }
                near = 1;
            }
            else if (range == ((long long)odd_integer + 1LL))
            {
                pi_tower_words_complement(odd, distance);
                near = 1;
            }
            if ((near != 0) && (pi_tower_words_compare(distance, nearest) < 0))
            {
                for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
                {
                    nearest[word] = distance[word];
                }
                nearest_column = column;
                nearest_row = row;
            }
        }
        horizontal += top - bottom;
        vertical += 1ULL;
        // how near the wall hit at x = i + 1 comes to a corner: alpha (i + 1)'s distance to a whole
        unsigned long long mirror[PI_TOWER_WORDS];
        pi_tower_words_complement(next, mirror);
        const unsigned long long *const miss = (pi_tower_words_compare(next, mirror) < 0) ? next : mirror;
        if (pi_tower_words_compare(miss, closest) < 0)
        {
            for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
            {
                closest[word] = miss[word];
            }
            if (near_count == PI_TOWER_NEAR_CORNERS_MAX)
            {
                pi_tower_refused("the billiard's record near-corners pass their table");
            }
            near_corners[near_count] = column + 1ULL;
            near_count += 1u;
        }
        for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
        {
            single[word] = next[word];
        }
        single_integer = next_integer;
        odd_integer += pi_tower_words_add(odd, twice) + twice_carry;
    }
    longest = (run > longest) ? run : longest;
    size_t floors_in_walk = 0u;
    while ((floors_in_walk < floors->count) &&
           (pi_tower_compare(floors->denominator[floors_in_walk], pi_tower_unsigned(columns)) <= 0))
    {
        floors_in_walk += 1u;
    }
    int matched = near_count == floors_in_walk;
    for (size_t at = 0u; (at < near_count) && (at < floors_in_walk); at += 1u)
    {
        matched = matched && (near_corners[at] == pi_tower_word(floors->denominator[at]));
    }
    mmgr_line_put(&results->line, "  the billiard from the corner at slope pi, 2^");
    mmgr_line_decimal(&results->line, PI_TOWER_BILLIARD_BITS, 1u);
    mmgr_line_put(&results->line, " columns: ");
    mmgr_line_decimal(&results->line, segments, 1u);
    mmgr_line_put(&results->line, " segments between ");
    mmgr_line_decimal(&results->line, vertical, 1u);
    mmgr_line_put(&results->line, " side-wall and ");
    mmgr_line_decimal(&results->line, horizontal, 1u);
    mmgr_line_put(&results->line, " floor-wall hits, ");
    mmgr_line_decimal(&results->line, corners, 1u);
    mmgr_line_put(&results->line, " corners\n    the lead in L about the center changed passes ");
    mmgr_line_decimal(&results->line, changes, 1u);
    mmgr_line_put(&results->line, " times, the longest lead ");
    mmgr_line_decimal(&results->line, longest, 1u);
    mmgr_line_put(&results->line, " segments, one side ahead on ");
    mmgr_line_decimal(&results->line, ahead, 1u);
    mmgr_line_put(&results->line, " of ");
    mmgr_line_decimal(&results->line, segments, 1u);
    mmgr_line_put(&results->line, "\n    the nearest tie: |L| = ");
    pi_tower_print_exponent(&results->line, pi_tower_words_wide(nearest), pi_tower_power_two(PI_TOWER_BITS + 1u));
    mmgr_line_put(&results->line, " (p = (1, pi)) in cell (");
    mmgr_line_decimal(&results->line, nearest_column, 1u);
    mmgr_line_put(&results->line, ", ");
    mmgr_line_decimal(&results->line, nearest_row, 1u);
    mmgr_line_put(&results->line, "), where pi is nearest ");
    mmgr_line_decimal(&results->line, (2ULL * nearest_row) + 1ULL, 1u);
    mmgr_line_ch(&results->line, '/');
    mmgr_line_decimal(&results->line, (2ULL * nearest_column) + 1ULL, 1u);
    mmgr_line_put(&results->line, "\n    the record near-corners at the side walls: ");
    for (size_t at = 0u; at < near_count; at += 1u)
    {
        mmgr_line_decimal(&results->line, near_corners[at], 1u);
        mmgr_line_put(&results->line, (at + 1u < near_count) ? ", " : "\n");
    }
    // pi is irrational. No corner and no zero L can occur on the real walk. What these two checks measure is that
    // the integer turn's carries decide every floor (uncertain is 0). The walk read is the real one
    sim_check(results, (corners == 0ULL) && (uncertain == 0ULL),
              "no wall hit on the walk is a corner, and every floor on it is the real floor: one wall always wins");
    sim_check(results, (zero == 0ULL) && (uncertain == 0ULL),
              "no segment's angular momentum about the center is zero: one sense always leads");
    sim_check(results, matched && (floors_in_walk > 10u), "the walk's record near-corners are exactly the floors' q_j");
    sim_flush(results);
}

// 11. The residue. On floor j, q_j alpha = p_j + delta_j. Put p_j / q_j for alpha. The turn is then the permutation
// n -> n p_j mod q_j of q_j cells. It is whole again after q_j steps: the identity.
//
// Where q_j |delta_j| < 1, the whole parts of pi for n < q_j are those of the permutation: floor(n alpha) =
// floor(n p_j / q_j). Each point goes n delta_j / q_j off the mark of the permutation. It goes past the mark where
// delta_j > 0, and short of it where delta_j < 0, into the right-closed cell. At n = q_j pi is at P + delta_j, where
// the permutation is whole: that is the residue.
//
// The sim walks the 384-bit turn and reads the whole parts from its carries. It walks every floor with q_j at max
// 2^PI_TOWER_RESIDUE_BITS.
void pi_tower_residues(SimResults *results, const PiWide &walk_alpha, const PiTowerFloors *floors)
{
    const PiWide modulus = pi_tower_power_two(PI_TOWER_BITS);
    const PiWide limit = pi_tower_power_two(PI_TOWER_RESIDUE_BITS);
    unsigned long long increment[PI_TOWER_WORDS];
    for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
    {
        increment[word] = ((unsigned long long)walk_alpha.limb[(2u * word) + 1u] << 32u) |
                          (unsigned long long)walk_alpha.limb[2u * word];
    }
    int ok = 1;
    int certain = 1;
    unsigned int walked = 0u;
    // p_(j-2) and p_(j-1), from p_(-1) = 1 and p_0 = 0
    unsigned long long numerator_before = 1ULL;
    unsigned long long numerator = 0ULL;
    mmgr_line_put(&results->line, "  the residue: floor j, pi's identity P/q, the residue q pi - P, and q steps "
                                  "walked as the permutation n -> n p mod q\n");
    for (size_t at = 0u; (at < floors->count) && (pi_tower_compare(floors->denominator[at], limit) <= 0); at += 1u)
    {
        if (at > 0u)
        {
            // a_j p_(j-1) + p_(j-2): each p_j is below q_j, below 2^21. The words store them
            const unsigned long long next = (pi_tower_word(floors->quotient[at]) * numerator) + numerator_before;
            numerator_before = numerator;
            numerator = next;
        }
        const unsigned long long steps = pi_tower_word(floors->denominator[at]);
        const PiWide residue = pi_tower_difference(pi_tower_product(floors->denominator[at], walk_alpha),
                                                   pi_tower_product(pi_tower_unsigned(numerator), modulus));
        const int negative = residue.sign < 0;
        PiWide size = residue;
        size.sign = (residue.sign == 0) ? 0 : 1;
        // The real residue lies in (qA - pM, qA - pM + q). The integer whole parts are the real ones, and each point
        // stays in its cell, under one condition. Where delta > 0, it is q (size + q) < M. Where delta < 0, it is
        // q size < M with size >= q
        const PiWide q = floors->denominator[at];
        certain =
            certain && ((negative == 0) ? (pi_tower_compare(pi_tower_product(q, pi_tower_sum(size, q)), modulus) < 0)
                                        : ((pi_tower_compare(pi_tower_product(q, size), modulus) < 0) &&
                                           (pi_tower_compare(size, q) >= 0)));
        unsigned long long place[PI_TOWER_WORDS];
        for (unsigned int word = 0u; word < PI_TOWER_WORDS; word += 1u)
        {
            place[word] = 0ULL;
        }
        unsigned long long integer_sum = 0ULL;
        int floor_ok = 1;
        for (unsigned long long step = 0ULL; step < steps; step += 1ULL)
        {
            // n p < 2^42. The permutation's whole part is a word's quotient
            floor_ok = floor_ok && (integer_sum == ((step * numerator) / steps));
            integer_sum += pi_tower_words_add(place, increment);
        }
        // At n = q the walk is at the residue. That is whole P past 3 q and delta past it, or whole P - 1 and |delta|
        // short of the next
        const PiWide located = pi_tower_words_wide(place);
        floor_ok = floor_ok && (integer_sum == ((negative != 0) ? (numerator - 1ULL) : numerator)) &&
                   (pi_tower_compare(located, (negative != 0) ? pi_tower_difference(modulus, size) : size) == 0);
        ok = ok && floor_ok;
        walked += 1u;
        mmgr_line_put(&results->line, "    ");
        mmgr_line_decimal(&results->line, at, 2u);
        mmgr_line_put(&results->line, "  ");
        mmgr_line_decimal(&results->line, (3ULL * steps) + numerator, 1u);
        mmgr_line_ch(&results->line, '/');
        mmgr_line_decimal(&results->line, steps, 1u);
        mmgr_line_put(&results->line, "  residue ");
        mmgr_line_ch(&results->line, (negative != 0) ? '-' : '+');
        pi_tower_print_exponent(&results->line, size, modulus);
        mmgr_line_put(&results->line, (floor_ok != 0) ? "  the permutation's whole parts, and the residue at step q\n"
                                                      : "  NOT the permutation\n");
        sim_flush(results);
    }
    sim_check(results, ok && certain && (walked >= 12u),
              "on every floor with q_j to 2^21, pi's first q_j steps have the whole parts of the permutation n -> n "
              "p_j mod q_j, and at step q_j it is its residue off the whole");
}
