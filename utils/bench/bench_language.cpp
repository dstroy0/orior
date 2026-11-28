// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_language.cpp
 * @brief The two linear languages SHA-256 alternates between, measured against each other.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note SHA-256 rests on two module structures that are not bases for each other. Exclusive-or and
 *       rotation are linear over GF(2)^32. Addition is linear over the integers modulo 2^32. The
 *       round alternates between them deliberately, and that alternation is the trick.
 * @note The workbook searches each language separately and in full, and finds nothing in either.
 *       H35 walks all 4,294,967,295 Walsh masks: every GF(2)-linear direction. H37 sweeps the
 *       Fourier spectrum over 32 units and 12 scales: the integer-linear view. Two complete
 *       dictionaries, for two languages that the function does not use alone.
 * @note This bench measures the two against each other. Each language has a native notion of
 *       difference: exclusive-or for one, subtraction modulo 2^32 for the other. The rate at which
 *       the function destroys each is the rate at which that language loses grip on it. The ratio
 *       of those two rates is the translation cost, in rounds.
 * @note The two languages touch only at the carry. An addition is exactly an exclusive-or plus a
 *       carry. The workbook has the exact carry law, `p_{i+1} = p_i / 2 + 1/4`. Thus the translation
 *       per addition has a closed form. How it composes across rounds is unknown, and this bench
 *       measures it.
 * @note Two controls, one for each language, and they must fail in opposite directions. A function
 *       of exclusive-or and rotation alone carries an exclusive-or difference through perfectly, and
 *       scatters an additive one. A function of addition alone does the reverse. An instrument that
 *       reported the same thing for both would measure neither.
 */

#include "bench_load_limit.h"
#include "bench_seed.h"
#include "sha256_core.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <random>
#include <vector>

namespace
{

/** @brief Bins in the window through which the bench reads the difference. */
const unsigned BINS = 65536u;

/** @brief The deepest round this walks to. */
const unsigned DEEPEST = 10u;

/** @brief The standard's round constants. */
const uint32_t ROUND_CONSTANT[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

/** @brief Which function the bench measures. */
enum Machine
{
    MACHINE_SHA,      /**< The real round. */
    MACHINE_XOR_ONLY, /**< Exclusive-or and rotation only, linear over GF(2). */
    MACHINE_ADD_ONLY  /**< Addition and rotation only, linear over the integers. */
};

/** @brief Which notion of difference the bench tracks, by language. */
enum Language
{
    LANGUAGE_XOR, /**< Difference is exclusive-or, native to GF(2). */
    LANGUAGE_ADD  /**< Difference is subtraction modulo 2^32, native to the integers. */
};

/** @brief Rotates a word right. */
uint32_t turn_right(uint32_t value, unsigned distance)
{
    distance &= 31u;
    return (distance == 0u) ? value : ((value >> distance) | (value << (32u - distance)));
}

/** @brief The high mix function of the round. */
uint32_t mix_high(uint32_t value)
{
    return turn_right(value, 6u) ^ turn_right(value, 11u) ^ turn_right(value, 25u);
}

/** @brief The low mix function of the round. */
uint32_t mix_low(uint32_t value)
{
    return turn_right(value, 2u) ^ turn_right(value, 13u) ^ turn_right(value, 22u);
}

/** @brief The round's choice function. */
uint32_t choose(uint32_t control, uint32_t when_set, uint32_t when_clear)
{
    return when_clear ^ (control & (when_set ^ when_clear));
}

/** @brief The round's majority function. */
uint32_t majority(uint32_t left, uint32_t middle, uint32_t right)
{
    return (left & middle) | (right & (left ^ middle));
}

/**
 * @brief Runs one round of the chosen machine.
 *
 * @param[in,out] state         Eight working variables [BORROWS].
 * @param[in]     machine       Which function.
 * @param[in]     round         Which round. It selects the constant.
 * @param[in]     schedule_word The message word.
 * @note The two controls keep the shape of the real round: same rotations, same shift pattern, same
 *       constants. They change only which operation joins the terms. Thus they are controls on the
 *       language, and not on the shape. A control that also changed the structure would not say
 *       which of the two differences it answers.
 */
void step(uint32_t *state, Machine machine, unsigned round, uint32_t schedule_word)
{
    uint32_t carry_one = 0u;
    uint32_t carry_two = 0u;

    if (machine == MACHINE_XOR_ONLY)
    {
        // Exclusive-or joins everything. Thus the whole map is GF(2)-linear.
        carry_one = state[7] ^ mix_high(state[4]) ^ state[5] ^ ROUND_CONSTANT[round] ^ schedule_word;
        carry_two = mix_low(state[0]) ^ state[1];
        state[7] = state[6];
        state[6] = state[5];
        state[5] = state[4];
        state[4] = state[3] ^ carry_one;
        state[3] = state[2];
        state[2] = state[1];
        state[1] = state[0];
        state[0] = carry_one ^ carry_two;
        return;
    }

    if (machine == MACHINE_ADD_ONLY)
    {
        // Addition joins everything, and nothing replaces the rotations. Thus the map is linear
        // over the integers modulo 2^32. A rotation is not integer-linear. With it, this control
        // would be neither language.
        carry_one = state[7] + state[4] + state[5] + ROUND_CONSTANT[round] + schedule_word;
        carry_two = state[0] + state[1];
        state[7] = state[6];
        state[6] = state[5];
        state[5] = state[4];
        state[4] = state[3] + carry_one;
        state[3] = state[2];
        state[2] = state[1];
        state[1] = state[0];
        state[0] = carry_one + carry_two;
        return;
    }

    carry_one =
        state[7] + mix_high(state[4]) + choose(state[4], state[5], state[6]) + ROUND_CONSTANT[round] + schedule_word;
    carry_two = mix_low(state[0]) + majority(state[0], state[1], state[2]);
    state[7] = state[6];
    state[6] = state[5];
    state[5] = state[4];
    state[4] = state[3] + carry_one;
    state[3] = state[2];
    state[2] = state[1];
    state[1] = state[0];
    state[0] = carry_one + carry_two;
}

/** @brief Names a machine, for a printed report. */
const char *machine_name(Machine machine)
{
    return (machine == MACHINE_SHA)
               ? "SHA-256 round"
               : ((machine == MACHINE_XOR_ONLY) ? "xor only, GF(2)-linear" : "add only, integer-linear");
}

/**
 * @brief How many bits of grip a language has on a machine after a given number of rounds.
 *
 * @param[in]     machine  Which function.
 * @param[in]     language Which notion of difference to track.
 * @param[in]     rounds   How many rounds to run.
 * @param[in]     draws    How many pairs to sample.
 * @param[in,out] generator Random source [BORROWS].
 * @return                 Bits by which the output difference distribution falls short of flat.
 * @note The input difference stays the same across every pair. Thus this measures the language, and not
 *       the difference. A language keeps grip when one input difference produces a concentrated
 *       output difference. It loses grip when the output difference spreads over the whole window,
 *       whatever the input.
 * @note Collision entropy is the right reading here, and not a peak. A peak is one bin, and this is
 *       a question about the whole distribution. The function uses the unbiased estimator. The naive
 *       sum of squared frequencies has an upward bias at this sparsity, and would report grip that
 *       is not there.
 */
double grip_bits(Machine machine, Language language, unsigned rounds, unsigned draws, std::mt19937 *generator)
{
    std::vector<uint32_t> counts(BINS, 0u);

    // One fixed input difference for the whole run, placed on the word a round reads first.
    const uint32_t difference = (*generator)() | 1u;

    for (unsigned draw = 0u; draw < draws; draw += 1u)
    {
        uint32_t left[8];
        uint32_t right[8];
        uint32_t words[DEEPEST];

        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            left[slot] = (*generator)();
            right[slot] = left[slot];
        }
        for (unsigned slot = 0u; slot < DEEPEST; slot += 1u)
        {
            words[slot] = (*generator)();
        }

        if (language == LANGUAGE_XOR)
        {
            right[7] = left[7] ^ difference;
        }
        else
        {
            right[7] = left[7] + difference;
        }

        for (unsigned at = 0u; at < rounds; at += 1u)
        {
            step(left, machine, at, words[at]);
            step(right, machine, at, words[at]);
        }

        const uint32_t seen = (language == LANGUAGE_XOR) ? (right[0] ^ left[0]) : (right[0] - left[0]);
        counts[seen & (BINS - 1u)] += 1u;
    }

    long double collisions = 0.0L;
    for (unsigned bin = 0u; bin < BINS; bin += 1u)
    {
        const long double here = (long double)counts[bin];
        collisions += here * (here - 1.0L);
    }
    const long double possible = (long double)draws * ((long double)draws - 1.0L);
    const long double rate = collisions / possible;

    // Sixteen bits is flat. Anything above zero is grip the language still has.
    const double flat = 16.0;
    return (rate > 0.0L) ? (flat + log2((double)rate)) : 0.0;
}

} // namespace

int main(int argc, char **argv)
{
    bench_lower_priority();

    const unsigned draws = (argc > 1) ? (unsigned)atoi(argv[1]) : 300000u;

    printf("================================================================\n");
    printf("  The two languages, measured against each other\n");
    printf("================================================================\n");
    printf("\n  SHA-256 is built on two module structures that are not bases for each other.\n");
    printf("  Exclusive-or and rotation are linear over GF(2)^32. Addition is linear over the\n");
    printf("  integers modulo 2^32. The round alternates between them, and that is the trick.\n");
    printf("\n  This workbook has searched each language separately and exhaustively. H35\n");
    printf("  walked all 4,294,967,295 Walsh masks, every GF(2)-linear direction there is.\n");
    printf("  H37 swept the Fourier view over 32 units and 12 scales. Two complete\n");
    printf("  dictionaries, for two languages the function is not written in.\n");
    printf("\n  Each language has a native difference: exclusive-or for one, subtraction for\n");
    printf("  the other. How fast the function destroys each is how fast that language loses\n");
    printf("  grip, and the gap between the two rates is the translation cost in rounds.\n");
    printf("\n  Grip is bits by which the output difference falls short of flat over a\n");
    printf("  sixteen-bit window. Zero is no grip at all. Pairs per cell: %u.\n", draws);

    const Machine machines[3] = {MACHINE_SHA, MACHINE_XOR_ONLY, MACHINE_ADD_ONLY};

    for (unsigned which = 0u; which < 3u; which += 1u)
    {
        printf("\n  %s\n", machine_name(machines[which]));
        printf("\n  %8s %16s %16s %14s\n", "rounds", "xor grip", "add grip", "which leads");
        printf("  %8s %16s %16s %14s\n", "--------", "----------------", "----------------", "--------------");

        for (unsigned rounds = 1u; rounds <= DEEPEST; rounds += 1u)
        {
            std::mt19937 xor_generator(bench_seed(20260908u) + rounds);
            std::mt19937 add_generator(bench_seed(20260908u) + rounds);

            const double by_xor = grip_bits(machines[which], LANGUAGE_XOR, rounds, draws, &xor_generator);
            const double by_add = grip_bits(machines[which], LANGUAGE_ADD, rounds, draws, &add_generator);

            const char *leader = "neither";
            if ((by_xor > 0.05) || (by_add > 0.05))
            {
                leader = (by_xor > (by_add + 0.05)) ? "xor" : ((by_add > (by_xor + 0.05)) ? "add" : "level");
            }

            printf("  %8u %16.4f %16.4f %14s\n", rounds, by_xor, by_add, leader);
        }
    }

    printf("\n================================================================\n");
    printf("  Reading it\n");
    printf("================================================================\n");
    printf("\n  The two controls have to fail in opposite directions, and that is the whole\n");
    printf("  reason they are here. A machine built only from exclusive-or carries an\n");
    printf("  exclusive-or difference through every round untouched, so its xor column must\n");
    printf("  stay at sixteen bits forever while its add column collapses. A machine built\n");
    printf("  only from addition must do exactly the reverse. If both columns behave the\n");
    printf("  same way on both controls, this instrument is measuring neither language and\n");
    printf("  the SHA rows mean nothing.\n");
    printf("\n  On the real round, the gap between the two columns at a given depth is what\n");
    printf("  the alternation costs. Where one language holds on longer than the other, that\n");
    printf("  is the language the function is closer to being written in, and the round it\n");
    printf("  stops holding is where that closeness runs out.\n");
    return 0;
}
