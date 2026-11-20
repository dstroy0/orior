// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_shuffle.c
 * @brief MT19937 and the shuffle of CPython. zeta_shuffle.h states the rules.
 */
#include "zeta_shuffle.h"

/**
 * @brief Fills the state from one word, by the linear recurrence of MT19937.
 *
 * @param[out] twister The generator [BORROWS].
 * @param[in]  word    The word.
 */
static void twister_init(ZetaTwister *twister, uint32_t word)
{
    twister->state[0] = word;
    for (int at = 1; at < ZETA_TWISTER_WORDS; at += 1)
    {
        const uint32_t previous = twister->state[at - 1];
        // The index is below 624, and fits a uint32_t.
        twister->state[at] = (1812433253u * (previous ^ (previous >> 30u))) + (uint32_t)at;
    }
    twister->index = ZETA_TWISTER_WORDS;
}

void twister_seed(ZetaTwister *twister, uint32_t seed)
{
    // CPython seeds from an int by init_by_array over its 32-bit words. A seed of at most 32 bits is one word.
    const uint32_t key[1] = {seed};
    const int key_length = 1;
    twister_init(twister, 19650218u);
    int at = 1;
    int key_at = 0;
    for (int count = (ZETA_TWISTER_WORDS > key_length) ? ZETA_TWISTER_WORDS : key_length; count > 0; count -= 1)
    {
        const uint32_t previous = twister->state[at - 1];
        // The key index is below the key length, and fits a uint32_t.
        twister->state[at] =
            (twister->state[at] ^ ((previous ^ (previous >> 30u)) * 1664525u)) + key[key_at] + (uint32_t)key_at;
        at += 1;
        key_at += 1;
        if (at >= ZETA_TWISTER_WORDS)
        {
            twister->state[0] = twister->state[ZETA_TWISTER_WORDS - 1];
            at = 1;
        }
        if (key_at >= key_length)
        {
            key_at = 0;
        }
    }
    for (int count = ZETA_TWISTER_WORDS - 1; count > 0; count -= 1)
    {
        const uint32_t previous = twister->state[at - 1];
        // The index is below 624, and fits a uint32_t.
        twister->state[at] = (twister->state[at] ^ ((previous ^ (previous >> 30u)) * 1566083941u)) - (uint32_t)at;
        at += 1;
        if (at >= ZETA_TWISTER_WORDS)
        {
            twister->state[0] = twister->state[ZETA_TWISTER_WORDS - 1];
            at = 1;
        }
    }
    twister->state[0] = 0x80000000u;
    twister->index = ZETA_TWISTER_WORDS;
}

/**
 * @brief The next 32-bit word of the generator, with the temper of MT19937.
 *
 * @param[in,out] twister The generator [BORROWS].
 * @return                The word.
 */
static uint32_t twister_next(ZetaTwister *twister)
{
    static const uint32_t matrix[2] = {0u, 0x9908B0DFu};
    if (twister->index >= ZETA_TWISTER_WORDS)
    {
        for (int at = 0; at < ZETA_TWISTER_WORDS; at += 1)
        {
            const uint32_t word =
                (twister->state[at] & 0x80000000u) | (twister->state[(at + 1) % ZETA_TWISTER_WORDS] & 0x7FFFFFFFu);
            twister->state[at] = twister->state[(at + 397) % ZETA_TWISTER_WORDS] ^ (word >> 1u) ^ matrix[word & 1u];
        }
        twister->index = 0;
    }
    uint32_t value = twister->state[twister->index];
    twister->index += 1;
    value ^= value >> 11u;
    value ^= (value << 7u) & 0x9D2C5680u;
    value ^= (value << 15u) & 0xEFC60000u;
    value ^= value >> 18u;
    return value;
}

double twister_random(ZetaTwister *twister)
{
    const uint32_t high = twister_next(twister) >> 5u;
    const uint32_t low = twister_next(twister) >> 6u;
    return (((double)high * 67108864.0) + (double)low) * (1.0 / 9007199254740992.0);
}

uint32_t twister_below(ZetaTwister *twister, uint32_t bound)
{
    uint32_t bits = 0u;
    for (uint32_t rest = bound; rest != 0u; rest >>= 1u)
    {
        bits += 1u;
    }
    for (;;)
    {
        const uint32_t draw = twister_next(twister) >> (32u - bits);
        if (draw < bound)
        {
            return draw;
        }
    }
}

void zeta_permuted(unsigned char *symbols, size_t count, uint32_t seed)
{
    ZetaTwister twister;
    twister_seed(&twister, seed);
    twister_shuffle(&twister, symbols, count);
}

void twister_shuffle(ZetaTwister *twister, unsigned char *symbols, size_t count)
{
    for (size_t at = count - 1u; (count > 1u) && (at > 0u); at -= 1u)
    {
        // The count of symbols here is small, and the index fits a uint32_t.
        const size_t other = twister_below(twister, (uint32_t)(at + 1u));
        const unsigned char held = symbols[at];
        symbols[at] = symbols[other];
        symbols[other] = held;
    }
}

size_t zeta_agreement(const unsigned char *symbols, size_t count, size_t lag)
{
    size_t agreed = 0u;
    for (size_t at = 0u; (at + lag) < count; at += 1u)
    {
        agreed += (symbols[at] == symbols[at + lag]) ? 1u : 0u;
    }
    return agreed;
}
