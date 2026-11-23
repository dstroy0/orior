// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// table_draw.h: the draws of the table's tools: the 32-bit Mersenne twister of the C++ standard ([rand.predef],
// mt19937, seeded as its constructor of one value or of a seed sequence seeds it), and a shuffle that takes its
// draws from it as MSVC's
// std::shuffle takes them. That shuffle is the one of the toolset that builds the tools. A seed then puts the same
// order that the tools put with that toolset's library.
//
// The shuffle takes each place `t` from 1 up, and draws a place from 0 to `t`. The elements at `t` and at the
// drawn place change places, and nothing moves where the draw is `t`. A draw below `n` takes 32 bits of the
// twister into a 64-bit word. It gives the word modulo `n`. It draws again where that leaves a bias: where the
// word divided by `n` is not below 2^32 - 1 divided by `n`, and 2^32 - 1 modulo `n` is not `n` - 1
#ifndef TABLE_DRAW_H
#define TABLE_DRAW_H

#include <stddef.h>

// the state of the twister: its 624 words and the place of the next
#define TABLE_DRAW_WORDS 624u

struct TableDraw
{
    unsigned int word[TABLE_DRAW_WORDS];
    unsigned int at;
};

// the twister seeded with `seed`, as mt19937's constructor of one value seeds it
static inline void table_draw_seeded(TableDraw *draw, unsigned int seed)
{
    draw->word[0] = seed;
    for (unsigned int at = 1u; at < TABLE_DRAW_WORDS; at += 1u)
    {
        const unsigned int before = draw->word[at - 1u];
        draw->word[at] = (1812433253u * (before ^ (before >> 30u))) + at;
    }
    draw->at = TABLE_DRAW_WORDS;
}

// a word of seed_seq's mix: `value` with its high bits folded into its low
static inline unsigned int table_draw_folded(unsigned int value)
{
    return value ^ (value >> 27u);
}

// The twister seeded from the `count` words `seeds`, as mt19937's constructor of a std::seed_seq of those words seeds
// it. The sequence's generate fills the 624 words, and a state whose words are 0 but for the top bit of the first
// takes that bit set
static inline void table_draw_sequenced(TableDraw *draw, const unsigned int *seeds, unsigned int count)
{
    unsigned int *const words = draw->word;
    const unsigned int n = TABLE_DRAW_WORDS;
    for (unsigned int at = 0u; at < n; at += 1u)
    {
        words[at] = 0x8b8b8b8bu;
    }
    const unsigned int s = count;
    const unsigned int m = ((s + 1u) > n) ? (s + 1u) : n;
    // the standard's lag for a state of 623 words or more
    const unsigned int t = 11u;
    const unsigned int p = (n - t) / 2u;
    const unsigned int q = p + t;
    for (unsigned int k = 0u; k < m; k += 1u)
    {
        const unsigned int r1 =
            1664525u * table_draw_folded(words[k % n] ^ words[(k + p) % n] ^ words[(k + n - 1u) % n]);
        const unsigned int r2 = (k == 0u) ? (r1 + s) : ((k <= s) ? (r1 + (k % n) + seeds[k - 1u]) : (r1 + (k % n)));
        words[(k + p) % n] += r1;
        words[(k + q) % n] += r2;
        words[k % n] = r2;
    }
    for (unsigned int k = m; k < (m + n); k += 1u)
    {
        const unsigned int r3 =
            1566083941u * table_draw_folded(words[k % n] + words[(k + p) % n] + words[(k + n - 1u) % n]);
        const unsigned int r4 = r3 - (k % n);
        words[(k + p) % n] ^= r3;
        words[(k + q) % n] ^= r4;
        words[k % n] = r4;
    }
    int zero = (words[0] & 0x80000000u) == 0u;
    for (unsigned int at = 1u; zero && (at < n); at += 1u)
    {
        zero = words[at] == 0u;
    }
    if (zero)
    {
        words[0] = 0x80000000u;
    }
    draw->at = TABLE_DRAW_WORDS;
}

// the twister's next 32 bits
static inline unsigned int table_draw_next(TableDraw *draw)
{
    if (draw->at >= TABLE_DRAW_WORDS)
    {
        for (unsigned int at = 0u; at < TABLE_DRAW_WORDS; at += 1u)
        {
            const unsigned int joined =
                (draw->word[at] & 0x80000000u) | (draw->word[(at + 1u) % TABLE_DRAW_WORDS] & 0x7fffffffu);
            const unsigned int shifted = (joined >> 1u) ^ (((joined & 1u) != 0u) ? 0x9908b0dfu : 0u);
            draw->word[at] = draw->word[(at + 397u) % TABLE_DRAW_WORDS] ^ shifted;
        }
        draw->at = 0u;
    }
    unsigned int value = draw->word[draw->at];
    draw->at += 1u;
    value ^= value >> 11u;
    value ^= (value << 7u) & 0x9d2c5680u;
    value ^= (value << 15u) & 0xefc60000u;
    value ^= value >> 18u;
    return value;
}

// a draw from 0 to `count` - 1, as MSVC's _Rng_from_urng draws one for a range of `count` from the twister
static inline unsigned long long table_draw_below(TableDraw *draw, unsigned long long count)
{
    for (;;)
    {
        unsigned long long value = 0ULL;
        unsigned long long mask = 0ULL;
        while (mask < (count - 1ULL))
        {
            value = (value << 32u) | table_draw_next(draw);
            mask = (mask << 32u) | 0xffffffffULL;
        }
        if (((value / count) < (mask / count)) || ((mask % count) == (count - 1ULL)))
        {
            return value % count;
        }
    }
}

// the `count` elements at `elements` shuffled as MSVC's std::shuffle shuffles them, the draws from `draw`
template <typename T> static inline void table_shuffled(T *elements, unsigned long long count, TableDraw *draw)
{
    for (unsigned long long target = 1ULL; target < count; target += 1ULL)
    {
        const unsigned long long drawn = table_draw_below(draw, target + 1ULL);
        if (drawn != target)
        {
            const T kept = elements[target];
            elements[target] = elements[drawn];
            elements[drawn] = kept;
        }
    }
}

#endif
