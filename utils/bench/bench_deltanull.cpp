// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_deltanull.cpp
 * @brief The Sigma-delta null, applied to the headline result of this tree.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The Sigma-delta null of the anchor sift. Let Pi_Sigma be the family of permutations that
 *       keep the observed carrier counts and the stated partition. For a statistic M, the residual is
 *
 *           Delta_Sigma[M] = M(x) - E_{pi ~ Pi_Sigma}[ M(pi(x)) ]
 *
 *       The delta null measures what remains after the carrier projects its structure and examines
 *       itself. It does not claim to reconstruct a hidden generator, and it does not invoke
 *       Laplace's demon.
 * @note A common error has one shape: a measurement against an analytical null derived by hand,
 *       where the null is wrong. A hand-derived null can look like a finding. The nibble-geometric
 *       overshoot model, the K xor rotr(K,r) offset and the expected-worst-of-3840 estimate are
 *       examples. A permutation null cannot be wrong in that way. It keeps the counts of the carrier
 *       by construction, and not by hand arithmetic.
 * @note The statistic under test is the strongest positive result of this tree: the concentration
 *       of the rotational-XOR delta, measured at 775,000 times uniform. The delta null asks where
 *       that concentration belongs. In the *pair* between the two members, it would be real
 *       structure. In the marginal distribution of the outputs, which a permutation keeps, it would
 *       be an artifact.
 */

#include "../../src/cu/transpiler/lstar/protocol/table/table_draw.h"
#include "bench_bound.h"
#include "bench_seed.h"
#include "bench_tally.h"
#include "sha256_core.h"

#include "embed_turnstile.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>

namespace
{

/** @brief The frame the earlier sweep selected. */
const unsigned FRAME = 31u;

/** @brief The standard's first round constant, the one the RX measurement used. */
const uint32_t ROUND_CONSTANT_ZERO = 0x428a2f98u;

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
 * @brief Runs one real round, constant included.
 *
 * @param[in,out] state         Eight working variables [BORROWS].
 * @param[in]     schedule_word The message word.
 */
void run_round(uint32_t *state, uint32_t schedule_word)
{
    const uint32_t carry_one =
        state[7] + mix_high(state[4]) + choose(state[4], state[5], state[6]) + ROUND_CONSTANT_ZERO + schedule_word;
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

/** @brief The pairs of the bench. */
const size_t PAIRS = 2000000u;

/**
 * @brief The word-0 outputs of the first member and of the second, one of each a pair. Then which right element pairs
 *        with each left element.
 */
uint32_t g_left[PAIRS];
uint32_t g_right[PAIRS];
uint32_t g_order[PAIRS];

/** @brief The deltas of one order of the pairs, and the room that their sort takes. One order fills them at a time. */
uint32_t g_deltas[PAIRS];
uint32_t g_deltas_scratch[PAIRS];

/** @brief The count of each distinct delta, and the room that their sort takes. */
uint32_t g_masses[PAIRS];
uint32_t g_masses_scratch[PAIRS];

/**
 * @brief Returns the order of two counts, highest first, for memor.sort.
 *
 * @param[in] left  One count [BORROWS].
 * @param[in] right The other count [BORROWS].
 * @return          -1 where the first is above the second, 1 where it is below, and 0 where they are equal.
 */
int mass_order(const void *left, const void *right)
{
    const uint32_t one = *(const uint32_t *)left;
    const uint32_t other = *(const uint32_t *)right;
    return (one > other) ? -1 : ((one < other) ? 1 : 0);
}

/**
 * @brief Puts the delta of each pair under the pair order of g_order in g_deltas, sorted.
 *
 * @param[in] count The pairs.
 * @note The delta is right[order[i]] xor rotr(left[i], FRAME): the rotational-XOR difference. The
 *       identity as order gives the real measurement. A permutation gives one draw from the null.
 */
void deltas_tallied(size_t count)
{
    for (size_t at = 0u; at < count; at += 1u)
    {
        g_deltas[at] = g_right[g_order[at]] ^ rotate_right(g_left[at], FRAME);
    }
    bench_tally_sorted(g_deltas, g_deltas_scratch, count);
}

/**
 * @brief The statistic under test: how concentrated the most common delta is.
 *
 * @param[in] count The pairs.
 * @return          Frequency of the most common delta under the pair order of g_order.
 */
double concentration(size_t count)
{
    deltas_tallied(count);
    uint32_t best = 0u;
    for (size_t at = 0u; at < count;)
    {
        best = bench_max(best, bench_tally_run(g_deltas, count, &at).count);
    }
    return (double)best / (double)count;
}

} // namespace

int main()
{
    printf("================================================================\n");
    printf("  The Sigma-delta null, applied to this tree's own result\n");
    printf("================================================================\n");
    printf("\n  Delta_Sigma[M] = M(x) - E_pi[ M(pi(x)) ], with Pi_Sigma the permutations that\n");
    printf("  preserve the observed carrier counts. Here M is the concentration of the most\n");
    printf("  common rotational-XOR delta, and pi re-pairs the two members while leaving both\n");
    printf("  output multisets exactly as measured.\n");
    printf("\n  If the concentration lives in the pairing, permuting destroys it and the\n");
    printf("  residual is large. If it lives in the marginals, the permutation preserves it\n");
    printf("  and the residual is near zero, which would make the 775,000-fold figure a\n");
    printf("  property of the output distribution instead of of the rotational relation.\n");

    const unsigned pairs = PAIRS;
    TableDraw generator;
    table_draw_seeded(&generator, bench_seed(20260908u));

    for (unsigned trial = 0u; trial < pairs; trial += 1u)
    {
        uint32_t plain[8];
        uint32_t turned[8];
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            const uint32_t value = table_draw_next(&generator);
            plain[slot] = value;
            turned[slot] = rotate_right(value, FRAME);
        }
        const uint32_t message = table_draw_next(&generator);

        run_round(plain, message);
        run_round(turned, rotate_right(message, FRAME));

        g_left[trial] = plain[0];
        g_right[trial] = turned[0];
    }

    for (unsigned at = 0u; at < pairs; at += 1u)
    {
        g_order[at] = at;
    }

    const auto started = embed_clock_ns();
    const double measured = concentration(pairs);

    printf("\n  M(x), the real pairing : %.8f%%\n", measured * 100.0);

    // Sample the null. Each draw re-pairs the members and leaves both multisets untouched.
    const unsigned draws = 12u;
    double null_total = 0.0;
    double null_low = 1.0;
    double null_high = 0.0;

    printf("\n  %8s %18s\n", "draw", "M(pi(x))");
    printf("  %8s %18s\n", "--------", "------------------");

    for (unsigned draw = 0u; draw < draws; draw += 1u)
    {
        table_shuffled(g_order, pairs, &generator);
        const double value = concentration(pairs);

        null_total += value;
        null_low = bench_min(null_low, value);
        null_high = bench_max(null_high, value);
        printf("  %8u %17.8f%%\n", draw, value * 100.0);
    }

    const double null_mean = null_total / (double)draws;
    const double residual = measured - null_mean;
    const auto finished = embed_clock_ns();

    printf("\n================================================================\n");
    printf("  The residual\n");
    printf("================================================================\n");
    printf("\n  M(x)                  : %.8f%%\n", measured * 100.0);
    printf("  E_pi[M(pi(x))]        : %.8f%%   over %u draws, range %.8f%% to %.8f%%\n", null_mean * 100.0, draws,
           null_low * 100.0, null_high * 100.0);
    printf("  Delta_Sigma[M]        : %+.8f%%\n", residual * 100.0);
    printf("  ratio, measured / null: %.2f\n", (null_mean > 0.0) ? (measured / null_mean) : 0.0);

    printf("\n  uniform over 2^32     : %.3e%%\n", 100.0 / 4294967296.0);
    printf("  measured over uniform : %.3e\n", measured * 4294967296.0);
    printf("  null over uniform     : %.3e\n", null_mean * 4294967296.0);

    printf("\n  Reading it. The concentration over uniform was reported earlier as the\n");
    printf("  headline, and the uniform reference is a hand-derived analytical null. The\n");
    printf("  permutation null is the one that cannot be derived wrongly, and the residual\n");
    printf("  above is what survives it. A residual near zero would mean the earlier figure\n");
    printf("  measured the shape of the output distribution instead of the rotational\n");
    printf("  pairing, and the headline would have to be withdrawn to that extent.\n");

    // -------------------------------------------------------------------------------------
    // The peak is one statistic. The eddy is the whole distribution. The anchor sift has the right
    // currency for it: its cost model prices an anchor at q = 2^-H2. Thus the collision entropy of
    // the delta distribution is the cost of an anchor on it.
    // -------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  The eddy priced in anchor-sift's own currency\n");
    printf("================================================================\n");
    printf("\n  A peak is one number. The eddy is the distribution's shape, and section 2.5\n");
    printf("  prices an anchor at q = 2^-H2 where H2 is collision entropy. So H2 of the delta\n");
    printf("  distribution is exactly what an anchor placed on the delta would cost, and the\n");
    printf("  shortfall from 32 is how many bits of coherence the eddy actually carries.\n");

    for (int pass = 0; pass < 2; pass += 1)
    {
        const bool real_pairing = (pass == 0);
        if (real_pairing)
        {
            for (unsigned at = 0u; at < pairs; at += 1u)
            {
                g_order[at] = at;
            }
        }
        else
        {
            table_shuffled(g_order, pairs, &generator);
        }

        deltas_tallied(pairs);

        // Unbiased estimator of the collision probability. The naive sum of squared frequencies has
        // an upward bias at this sparsity, and would invent structure that is not there.
        long double collision_pairs = 0.0L;
        size_t distinct = 0u;
        for (size_t at = 0u; at < pairs;)
        {
            const long double count = (long double)bench_tally_run(g_deltas, pairs, &at).count;
            collision_pairs += count * (count - 1.0L);
            distinct += 1u;
        }
        const long double total_pairs = (long double)pairs * ((long double)pairs - 1.0L);
        const long double collision_probability = collision_pairs / total_pairs;
        const double entropy = (collision_probability > 0.0L) ? -log2((double)collision_probability) : 32.0;

        printf("\n  %s\n", real_pairing ? "real rotational pairing:" : "permuted pairing:");
        printf("    distinct deltas seen : %zu\n", distinct);
        printf("    collision probability: %.6e\n", (double)collision_probability);
        printf("    H2                   : %.6f bits\n", entropy);
        printf("    shortfall from 32    : %.6f bits\n", 32.0 - entropy);
        printf("    anchor cost q = 2^-H2: %.6e\n", pow(2.0, -entropy));

        // Order two answers the collision question. Arikan's bound says that order one half governs
        // search. That is a different number. Do not quote the first as though it answers the
        // second.
        //
        // No estimate of order one half is possible from a sample this sparse. The bench prints the
        // reason, and does not hide it. The plug-in estimator is 2 log2(sum sqrt(p)). Where every
        // observed delta is a singleton, that sum is exactly sqrt(n). Thus the estimate reads
        // log2(n), whatever the true value.
        //
        // The ceiling below states that artifact in advance. Thus no one can take the estimate beside
        // it for a measurement.
        long double root_mass = 0.0L;
        uint32_t largest = 0u;
        size_t singletons = 0u;
        for (size_t at = 0u; at < pairs;)
        {
            const BenchTallyRun entry = bench_tally_run(g_deltas, pairs, &at);
            root_mass += sqrt((long double)entry.count / (long double)pairs);
            largest = (entry.count > largest) ? entry.count : largest;
            singletons += (entry.count == 1u) ? 1u : 0u;
        }

        const double plug_in_half = 2.0 * log2((double)root_mass);
        const double ceiling = log2((double)pairs);
        const double min_entropy = -log2((double)largest / (double)pairs);

        printf("    H_1/2 plug-in        : %.6f bits, against a ceiling of %.6f set by the\n", plug_in_half, ceiling);
        printf("                           sample size alone. Not a measurement.\n");
        printf("    H_inf                : %.6f bits\n", min_entropy);
        printf("    unseen mass, f1 / n  : %.6f\n", (double)singletons / (double)pairs);
        printf("    H_1/2 bounded below  : %.6f bits, since H_alpha decreases in alpha and\n", entropy);
        printf("                           H_2 is estimable here while H_1/2 is not.\n");
    }

    printf("\n  Reading it. A delta distribution at H2 = 32 is flat and an anchor on it costs\n");
    printf("  2^-32, which is what the digest itself already costs, so it buys nothing. Every\n");
    printf("  bit of shortfall is a bit the eddy carries and a bit the anchor is cheaper by.\n");
    printf("\n  The permuted row is the control. Its shortfall is whatever a finite sample of\n");
    printf("  this size produces against a flat distribution, and only the difference between\n");
    printf("  the two rows is coherence instead of sampling.\n");

    // -------------------------------------------------------------------------------------
    // Treat the mode as a center of gravity, and map outward from it. Where the mass
    // concentrates, a walk of deltas in descending probability covers the distribution fastest.
    // The profile below is exactly how many candidates a search must carry.
    // -------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  Mapping outward from the center of gravity\n");
    printf("================================================================\n");
    printf("\n  Order the deltas by mass and walk out from the densest. How many does it take\n");
    printf("  to cover a given share of the distribution? That count is what a characteristic\n");
    printf("  search must carry, and it is the operational form of the entropy above.\n");

    {
        for (unsigned at = 0u; at < pairs; at += 1u)
        {
            g_order[at] = at;
        }
        deltas_tallied(pairs);

        // the count of each distinct delta, highest first
        size_t mass_count = 0u;
        for (size_t at = 0u; at < pairs;)
        {
            g_masses[mass_count] = bench_tally_run(g_deltas, pairs, &at).count;
            mass_count += 1u;
        }
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = g_masses, .bytes = sizeof(uint32_t),
                         .scratch = g_masses_scratch, .count = mass_count, .order = mass_order);
        const uint32_t *const masses = g_masses;

        printf("\n  %10s %16s %18s\n", "coverage", "deltas needed", "share of 2^32");
        printf("  %10s %16s %18s\n", "----------", "----------------", "------------------");

        const double wanted[] = {0.10, 0.25, 0.50, 0.75, 0.90, 0.99};
        for (double share : wanted)
        {
            uint64_t running = 0u;
            size_t needed = 0u;
            while ((needed < mass_count) && ((double)running < (share * (double)pairs)))
            {
                running += masses[needed];
                needed += 1u;
            }
            printf("  %9.0f%% %16zu %18.3e\n", share * 100.0, needed, (double)needed / 4294967296.0);
        }

        printf("\n  A flat distribution would need %.3e deltas to cover half its mass, which\n", 0.5 * 4294967296.0);
        printf("  is 2^31. The real one needs the count in the 50%% row. The ratio between\n");
        printf("  them is the same 15.5 bits the entropy reported, seen as a candidate count\n");
        printf("  instead of as a rate, and it is the number a search actually pays.\n");

        printf("\n  Which rows to trust. %zu distinct deltas came out of %u samples, so the\n", mass_count, pairs);
        printf("  mean count per delta is %.1f and the tail is singletons. The 10%% and 25%%\n",
               (double)pairs / (double)mass_count);
        printf("  rows sit in the well-sampled head and are measurements. The 90%% and 99%%\n");
        printf("  rows approach the distinct count itself, which means they are counting how\n");
        printf("  many deltas happened to be seen once, not how many the distribution holds.\n");
        printf("  Those two rows are sampling artifacts and should not be read as coverage.\n");
    }

    printf("\n  elapsed %.1f s\n", ((double)(finished - started) / 1.0e9));
    printf("================================================================\n");
    return 0;
}
