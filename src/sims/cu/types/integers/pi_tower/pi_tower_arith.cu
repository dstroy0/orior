// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_tower_arith.cu: exact arithmetic and first hits
#include "pi_tower_internal.h"

int g_pi_tower_error = 0;

// set where a search requests a record past the range of the records
int g_pi_tower_records_short = 0;

static void pi_tower_took(OriorExactStatus status)
{
    if (status != ORIOR_EXACT_OK)
    {
        g_pi_tower_error = 1;
    }
}

PiWide pi_tower_unsigned(unsigned long long number)
{
    PiWide value;
    sim_exact_unsigned(&value, number);
    return value;
}

PiWide pi_tower_power_two(unsigned int bits)
{
    PiWide value;
    orior_exact_zero(&value);
    value.limb[bits / 32u] = 1u << (bits % 32u);
    value.sign = 1;
    return value;
}

PiWide pi_tower_sum(const PiWide &left, const PiWide &right)
{
    PiWide value;
    orior_exact_zero(&value);
    pi_tower_took(orior_exact_add(&left, &right, &value));
    return value;
}

PiWide pi_tower_difference(const PiWide &left, const PiWide &right)
{
    PiWide value;
    orior_exact_zero(&value);
    pi_tower_took(orior_exact_subtract(&left, &right, &value));
    return value;
}

PiWide pi_tower_product(const PiWide &left, const PiWide &right)
{
    PiWide value;
    orior_exact_zero(&value);
    pi_tower_took(orior_exact_multiply(&left, &right, &value));
    return value;
}

// the floor of a quotient of two non-negative integers
PiWide pi_tower_quotient(const PiWide &numerator, const PiWide &divisor)
{
    PiWide quotient;
    PiWide remainder;
    orior_exact_zero(&quotient);
    orior_exact_zero(&remainder);
    pi_tower_took(orior_exact_divide(&numerator, &divisor, &quotient, &remainder));
    return quotient;
}

PiWide pi_tower_remainder(const PiWide &numerator, const PiWide &divisor)
{
    PiWide quotient;
    PiWide remainder;
    orior_exact_zero(&quotient);
    orior_exact_zero(&remainder);
    pi_tower_took(orior_exact_divide(&numerator, &divisor, &quotient, &remainder));
    return remainder;
}

int pi_tower_compare(const PiWide &left, const PiWide &right)
{
    return orior_exact_compare(&left, &right);
}

// the low 64 bits
unsigned long long pi_tower_word(const PiWide &value)
{
    return ((unsigned long long)value.limb[1] << 32u) | (unsigned long long)value.limb[0];
}

// the least x >= 0 with low <= (multiplier . x mod modulus) <= high, for low <= high < modulus; 0 where there is
// none. Where no multiple of the multiplier lands in the window, x wraps y times. Then y is the least hit of the window
// reflected into the rotation one floor down, modulus mod multiplier on multiplier: Euclid's descent.
int pi_tower_first_hit(PiWide multiplier, PiWide modulus, PiWide low, PiWide high, PiWide *step)
{
    const PiWide one = pi_tower_unsigned(1ULL);
    // Each level is a step of Euclid's algorithm on the multiplier and the modulus, in a cell of the temporary tier. A
    // multiplier above the modulus adds one step
    const unsigned int multiplier_bits = pi_tower_bit_length(multiplier);
    const unsigned int modulus_bits = pi_tower_bit_length(modulus);
    const unsigned int bound =
        pi_tower_euclid_bound((multiplier_bits > modulus_bits) ? multiplier_bits : modulus_bits) + 1u;
    PiTowerLevel *const levels = (PiTowerLevel *)pi_tower_taken(bound, sizeof(PiTowerLevel), 1);
    unsigned int depth = 0u;
    PiWide found;
    for (;;)
    {
        if (low.sign == 0)
        {
            found = pi_tower_unsigned(0ULL);
            break;
        }
        if (multiplier.sign == 0)
        {
            pi_tower_given(levels);
            return 0;
        }
        const PiWide least = pi_tower_quotient(pi_tower_sum(low, pi_tower_difference(multiplier, one)), multiplier);
        if (pi_tower_compare(pi_tower_product(multiplier, least), high) <= 0)
        {
            found = least;
            break;
        }
        if (depth == bound)
        {
            pi_tower_refused("the descent passes Lame's bound on its levels");
        }
        levels[depth].multiplier = multiplier;
        levels[depth].modulus = modulus;
        levels[depth].low = low;
        depth += 1u;
        const PiWide next_multiplier = pi_tower_remainder(modulus, multiplier);
        const PiWide next_low = pi_tower_difference(multiplier, pi_tower_remainder(high, multiplier));
        const PiWide next_high = pi_tower_difference(multiplier, pi_tower_remainder(low, multiplier));
        modulus = multiplier;
        multiplier = next_multiplier;
        low = next_low;
        high = next_high;
    }
    for (unsigned int at = depth; at > 0u; at -= 1u)
    {
        const PiTowerLevel &level = levels[at - 1u];
        const PiWide range = pi_tower_sum(level.low, pi_tower_product(level.modulus, found));
        found = pi_tower_quotient(pi_tower_sum(range, pi_tower_difference(level.multiplier, one)), level.multiplier);
    }
    pi_tower_given(levels);
    *step = found;
    return 1;
}

// the least x >= from with low <= (multiplier . x mod modulus) <= high; 0 where there is none
int pi_tower_first_hit_from(const PiWide &multiplier, const PiWide &modulus, const PiWide &from, const PiWide &low,
                            const PiWide &high, PiWide *step)
{
    const PiWide start = pi_tower_remainder(pi_tower_product(multiplier, from), modulus);
    const PiWide shifted_low = pi_tower_remainder(pi_tower_difference(pi_tower_sum(low, modulus), start), modulus);
    const PiWide shifted_high = pi_tower_remainder(pi_tower_difference(pi_tower_sum(high, modulus), start), modulus);
    if (pi_tower_compare(shifted_low, shifted_high) > 0)
    {
        // the window wraps past 0, and x = from is in it
        *step = from;
        return 1;
    }
    PiWide later;
    if (pi_tower_first_hit(multiplier, modulus, shifted_low, shifted_high, &later) == 0)
    {
        return 0;
    }
    *step = pi_tower_sum(from, later);
    return 1;
}

// whether some x in [from, to) has low <= (multiplier . x mod modulus) <= high
int pi_tower_hit_between(const PiWide &multiplier, const PiWide &modulus, const PiWide &from, const PiWide &to,
                         const PiWide &low, const PiWide &high)
{
    if (pi_tower_compare(from, to) >= 0)
    {
        return 0;
    }
    const PiWide start = pi_tower_remainder(pi_tower_product(multiplier, from), modulus);
    const PiWide shifted_low = pi_tower_remainder(pi_tower_difference(pi_tower_sum(low, modulus), start), modulus);
    const PiWide shifted_high = pi_tower_remainder(pi_tower_difference(pi_tower_sum(high, modulus), start), modulus);
    if (pi_tower_compare(shifted_low, shifted_high) > 0)
    {
        // the window wraps past 0, and x = from is in it
        return 1;
    }
    PiWide step;
    if (pi_tower_first_hit(multiplier, modulus, shifted_low, shifted_high, &step) == 0)
    {
        return 0;
    }
    return pi_tower_compare(pi_tower_sum(from, step), to) < 0;
}

// 2^bits . arctan(1 / x) within terms + 1: the series with signs in turn, each term floored
PiWide pi_tower_arctan(unsigned long long x, unsigned int bits, unsigned long long *terms)
{
    const PiWide square = pi_tower_unsigned(x * x);
    PiWide power = pi_tower_quotient(pi_tower_power_two(bits), pi_tower_unsigned(x));
    PiWide total = pi_tower_unsigned(0ULL);
    unsigned long long index = 0ULL;
    *terms = 0ULL;
    while (power.sign != 0)
    {
        const PiWide term = pi_tower_quotient(power, pi_tower_unsigned((2ULL * index) + 1ULL));
        total = ((index & 1ULL) == 0ULL) ? pi_tower_sum(total, term) : pi_tower_difference(total, term);
        *terms += 1ULL;
        power = pi_tower_quotient(power, square);
        index += 1ULL;
    }
    return total;
}
