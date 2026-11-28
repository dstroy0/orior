// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_canary.cpp
 * @brief The vector walk through delta space, steered by tail error instead of by the peak.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note H13 tests a walk through nonce space steered by fitness, and finds no gradient to climb.
 *       That is the wrong space. This walk is through delta space. The signal is not the fitness of
 *       a candidate, but the tail error of the distribution that it produces.
 * @note This matters because the chain of H30 fails in a way that a different objective might fix.
 *       That chain is greedy on probability. It takes the most likely output delta at each round,
 *       and collapses by a factor of 300 immediately after round zero. The peak is not the only
 *       guide, and nothing shows it to be the right one.
 * @note The canary claim: tail error moves first. If it does, it is a leading indicator. A delta
 *       whose distribution has low tail error should chain further than one chosen for its peak.
 *       The tail error should also show the collapse before the probability does.
 * @note The bench runs two strategies over identical candidate sets. Thus the objective is the only
 *       difference. A report of both at every step also tests the canary claim directly, because a
 *       leading indicator must lead.
 */

#include "../../src/cu/transpiler/lstar/protocol/table/table_draw.h"
#include "bench_bound.h"
#include "bench_seed.h"
#include "bench_tally.h"
#include "sha256_core.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

namespace
{

/** @brief The frame the sweep selected. */
const unsigned FRAME = 31u;

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

/**
 * @brief Rotates a word right.
 *
 * @param[in] value    Word to rotate.
 * @param[in] distance How far.
 * @return             The rotated word.
 */
uint32_t rotate_right(uint32_t value, unsigned distance)
{
    distance &= 31u;
    return (distance == 0u) ? value : ((value >> distance) | (value << (32u - distance)));
}

/** @brief The high mix function of the round. */
uint32_t mix_high(uint32_t value)
{
    return rotate_right(value, 6u) ^ rotate_right(value, 11u) ^ rotate_right(value, 25u);
}

/** @brief The low mix function of the round. */
uint32_t mix_low(uint32_t value)
{
    return rotate_right(value, 2u) ^ rotate_right(value, 13u) ^ rotate_right(value, 22u);
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
 * @brief Runs one real round.
 *
 * @param[in,out] state         Eight working variables [BORROWS].
 * @param[in]     round         Which round. It selects the constant.
 * @param[in]     schedule_word The message word.
 */
void run_round(uint32_t *state, unsigned round, uint32_t schedule_word)
{
    const uint32_t carry_one =
        state[7] + mix_high(state[4]) + choose(state[4], state[5], state[6]) + ROUND_CONSTANT[round] + schedule_word;
    const uint32_t carry_two = mix_low(state[0]) + majority(state[0], state[1], state[2]);

    state[7] = state[6];
    state[6] = state[5];
    state[5] = state[4];
    state[4] = state[3] + carry_one;
    state[3] = state[2];
    state[2] = state[1];
    state[1] = state[0];
    state[0] = carry_one + carry_two;
}

/** @brief The pairs of one step at max. */
const size_t DRAWS_MAX = 400000u;

/** @brief The output deltas of a step, and the room that their sort takes. One step fills them at a time. */
uint32_t g_deltas[DRAWS_MAX];
uint32_t g_deltas_scratch[DRAWS_MAX];

/** @brief The distinct deltas of a step with their counts, and the room that their sort takes. */
BenchTallyRun g_ranked[DRAWS_MAX];
BenchTallyRun g_ranked_scratch[DRAWS_MAX];

/**
 * @brief Returns the order of two runs by count, highest first, and on a tie by value, highest first.
 *
 * @param[in] left  One run [BORROWS].
 * @param[in] right The other run [BORROWS].
 * @return          -1 where the first comes before the second, 1 where it comes after, and 0 where they are equal.
 */
int rank_order(const void *left, const void *right)
{
    const BenchTallyRun *const one = (const BenchTallyRun *)left;
    const BenchTallyRun *const other = (const BenchTallyRun *)right;
    if (one->count != other->count)
    {
        return (one->count > other->count) ? -1 : 1;
    }
    return (one->value > other->value) ? -1 : ((one->value < other->value) ? 1 : 0);
}

/** @brief What one step through delta space reports back. */
struct Step
{
    double peak;        /**< Probability of the most common output delta. */
    double tail_error;  /**< Good-Turing unseen mass, f1 over N. The canary. */
    uint32_t best_peak; /**< The delta the peak objective would follow. */
    uint32_t best_tail; /**< The delta the tail objective would follow. */
    size_t tied;        /**< How many deltas share the maximum count. */
};

/**
 * @brief Takes one step: apply an input delta, run a round, and read the output distribution.
 *
 * @param[in,out] generator   Random source [BORROWS].
 * @param[in]     input_delta The delta carried into this round.
 * @param[in]     round       Which round.
 * @param[in]     draws       How many pairs to sample.
 * @return                    Peak, tail error, and the delta each objective would follow.
 * @note The tail objective picks among the top candidates by peak. It then follows the candidate
 *       that leaves the *next* distribution with the least unseen mass. That look-ahead makes it a
 *       different walk, and not a new label on the same one.
 */
Step take_step(TableDraw &generator, uint32_t input_delta, unsigned round, unsigned draws)
{
    for (unsigned draw = 0u; draw < draws; draw += 1u)
    {
        uint32_t plain[8];
        uint32_t turned[8];
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            const uint32_t value = table_draw_next(&generator);
            plain[slot] = value;
            turned[slot] = rotate_right(value, FRAME) ^ input_delta;
        }
        const uint32_t message = table_draw_next(&generator);

        run_round(plain, round, message);
        run_round(turned, round, rotate_right(message, FRAME));

        g_deltas[draw] = turned[0] ^ rotate_right(plain[0], FRAME);
    }
    bench_tally_sorted(g_deltas, g_deltas_scratch, draws);

    Step step = {0.0, 0.0, 0u, 0u, 0u};
    size_t singletons = 0u;
    size_t ranked_count = 0u;

    for (size_t at = 0u; at < draws;)
    {
        const BenchTallyRun entry = bench_tally_run(g_deltas, draws, &at);
        if (entry.count == 1u)
        {
            singletons += 1u;
        }
        g_ranked[ranked_count] = entry;
        ranked_count += 1u;
    }
    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = g_ranked, .bytes = sizeof(BenchTallyRun),
                     .scratch = g_ranked_scratch, .count = ranked_count, .order = rank_order);
    const BenchTallyRun *const ranked = g_ranked;

    // How many deltas share the top count. At this sparsity, the maximum is two or three
    // occurrences, and a great many deltas reach it. Thus the data do not identify "the most
    // common output delta". The tiebreak of the sort then chooses the delta to follow. That
    // tiebreak is the numeric value of the delta, and measures nothing. A walk that follows an
    // argmax over a tied set follows its tiebreak.
    for (size_t rank = 0u; rank < ranked_count; rank += 1u)
    {
        if (ranked[rank].count != ranked[0].count)
        {
            break;
        }
        step.tied += 1u;
    }

    step.peak = (double)ranked[0].count / (double)draws;
    step.best_peak = ranked[0].value;
    // Good-Turing unseen mass. The code keeps it as the sensitive part, and does not discard it as noise.
    step.tail_error = (double)singletons / (double)draws;
    step.best_tail = ranked[0].value;

    // Look ahead over the top candidates and let the tail objective choose differently.
    const size_t considered = bench_min<size_t>(6u, ranked_count);
    double least_tail = 1.0;

    for (size_t candidate = 0u; candidate < considered; candidate += 1u)
    {
        // the deltas of the probe take the table of the step's own, which the ranked runs no longer read
        const unsigned probe = draws / 8u;

        for (unsigned draw = 0u; draw < probe; draw += 1u)
        {
            uint32_t plain[8];
            uint32_t turned[8];
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                const uint32_t value = table_draw_next(&generator);
                plain[slot] = value;
                turned[slot] = rotate_right(value, FRAME) ^ ranked[candidate].value;
            }
            const uint32_t message = table_draw_next(&generator);

            run_round(plain, (round + 1u) & 63u, message);
            run_round(turned, (round + 1u) & 63u, rotate_right(message, FRAME));

            g_deltas[draw] = turned[0] ^ rotate_right(plain[0], FRAME);
        }
        bench_tally_sorted(g_deltas, g_deltas_scratch, probe);

        size_t ahead_singletons = 0u;
        for (size_t at = 0u; at < probe;)
        {
            if (bench_tally_run(g_deltas, probe, &at).count == 1u)
            {
                ahead_singletons += 1u;
            }
        }
        const double ahead_tail = (double)ahead_singletons / (double)probe;

        if (ahead_tail < least_tail)
        {
            least_tail = ahead_tail;
            step.best_tail = ranked[candidate].value;
        }
    }
    return step;
}

} // namespace

int main()
{
    printf("================================================================\n");
    printf("  The vector walk through delta space, tail error as canary\n");
    printf("================================================================\n");
    printf("\n  H13 walked nonce space by fitness and found it flat. This walks delta space,\n");
    printf("  and the signal is the tail error of the distribution a step produces rather\n");
    printf("  than the height of its peak.\n");
    printf("\n  H30's chain was greedy on the peak and collapsed 300-fold right after round\n");
    printf("  zero. If the tail error leads, steering by it should chain further, and the\n");
    printf("  tail should show the collapse arriving before the probability does.\n");

    const unsigned draws = 400000u;
    const unsigned depth = 7u;

    for (int strategy = 0; strategy < 2; strategy += 1)
    {
        const bool by_tail = (strategy == 1);
        printf("\n================================================================\n");
        printf("  Walk steered by %s\n", by_tail ? "tail error" : "the peak");
        printf("================================================================\n");
        printf("\n  %6s %16s %18s %18s %14s\n", "round", "peak", "tail error", "compound", "tied at peak");
        printf("  %6s %16s %18s %18s %14s\n", "------", "----------------", "------------------", "------------------",
               "--------------");

        TableDraw generator;
        table_draw_seeded(&generator, bench_seed(20260908u));
        uint32_t carried = 0u;
        double compound = 1.0;

        for (unsigned round = 0u; round < depth; round += 1u)
        {
            const Step step = take_step(generator, carried, round, draws);
            compound *= step.peak;

            printf("  %6u %16.8f %18.8f %18.6e %14zu\n", round, step.peak, step.tail_error, compound, step.tied);

            carried = by_tail ? step.best_tail : step.best_peak;
        }

        printf("\n  compound over %u rounds : %.6e\n", depth, compound);
        printf("  geometric mean per round: %.8f  (2^%.2f)\n", pow(compound, 1.0 / (double)depth),
               log2(pow(compound, 1.0 / (double)depth)));
    }

    printf("\n================================================================\n");
    printf("  Reading it\n");
    printf("================================================================\n");
    printf("\n  Two things are being tested and they are separable.\n");
    printf("\n  Is the tail a canary? It leads if it moves before the peak does. Compare the\n");
    printf("  round where tail error jumps against the round where the peak falls, in the\n");
    printf("  peak-steered table. A canary that moves at the same time is a thermometer.\n");
    printf("\n  Is it a better objective? The two compound figures answer that directly, and\n");
    printf("  they were produced over identical candidate sets with the same seed, so the\n");
    printf("  only difference between the runs is which delta got followed.\n");
    printf("\n  Read the tied column before either of those. It counts how many deltas share\n");
    printf("  the top count. Where it is above one there is no single most common delta, and\n");
    printf("  whichever one gets followed is picked by the sort's tiebreak on numeric value\n");
    printf("  instead of by anything measured. A walk steered by an argmax over a tied set\n");
    printf("  is steered by its tiebreak, and comparing two such walks compares two\n");
    printf("  tiebreaks. The peak figure itself is unaffected, since a tie does not change\n");
    printf("  the height of the maximum, only which delta carries it.\n");
    return 0;
}
