// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_draw64.h
 * @brief The 64-bit Mersenne twister of the C++ standard, mt19937_64 of [rand.predef], seeded from one value.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The parameters are those of the standard: 312 words, a middle word of 156, and 31 low bits in the split. The
 *       matrix word is 0xb5026f5aa96619e9. The output shifts are 29, 17, 37 and 43 bits with their masks, and the seed
 *       step multiplies by 6364136223846793005. A seed thus gives the draws of the standard's mt19937_64.
 */

#ifndef BENCH_DRAW64_H
#define BENCH_DRAW64_H

#include <stdint.h>

/** @brief The words of the state of the twister. */
#define BENCH_DRAW64_WORDS 312u

/** @brief The state of the twister: its words, and the place of the next. */
struct BenchDraw64
{
    uint64_t word[BENCH_DRAW64_WORDS];
    unsigned at;
};

/**
 * @brief Seeds the twister with one value, as the constructor of mt19937_64 of one value seeds it.
 *
 * @param[out] draw The twister [BORROWS].
 * @param[in]  seed The seed.
 */
static inline void bench_draw64_seeded(BenchDraw64 *draw, uint64_t seed)
{
    draw->word[0] = seed;
    for (unsigned at = 1u; at < BENCH_DRAW64_WORDS; at += 1u)
    {
        const uint64_t before = draw->word[at - 1u];
        draw->word[at] = (6364136223846793005ull * (before ^ (before >> 62u))) + at;
    }
    draw->at = BENCH_DRAW64_WORDS;
}

/**
 * @brief Returns the twister's next 64 bits.
 *
 * @param[in,out] draw The twister [BORROWS].
 * @return             The draw.
 */
static inline uint64_t bench_draw64_next(BenchDraw64 *draw)
{
    if (draw->at >= BENCH_DRAW64_WORDS)
    {
        for (unsigned at = 0u; at < BENCH_DRAW64_WORDS; at += 1u)
        {
            const uint64_t joined = (draw->word[at] & 0xffffffff80000000ull) |
                                    (draw->word[(at + 1u) % BENCH_DRAW64_WORDS] & 0x000000007fffffffull);
            const uint64_t shifted = (joined >> 1u) ^ (((joined & 1u) != 0u) ? 0xb5026f5aa96619e9ull : 0u);
            draw->word[at] = draw->word[(at + 156u) % BENCH_DRAW64_WORDS] ^ shifted;
        }
        draw->at = 0u;
    }
    uint64_t value = draw->word[draw->at];
    draw->at += 1u;
    value ^= (value >> 29u) & 0x5555555555555555ull;
    value ^= (value << 17u) & 0x71d67fffeda60000ull;
    value ^= (value << 37u) & 0xfff7eee000000000ull;
    value ^= value >> 43u;
    return value;
}

#endif
