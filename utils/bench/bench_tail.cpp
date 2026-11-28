// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_tail.cpp
 * @brief The tail as error, and the hypothesis of a periodic disturbance on a co-arm.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The sparse part of the delta distribution is not a sample artifact that more samples would
 *       resolve. The tail is the error term, by definition. The task is to drive it down, and not to
 *       sample it better. The chapter on wants in theory/workbooks/orior states the tail error as a
 *       recursive function of the tail, with a target of 10^-26.
 * @note The claim under test is in theory/thought_experiments/orior, under tail error under a
 *       periodic disturbance. A periodic disturbance on one arm lowers the joint error on an
 *       undisturbed co-arm by several orders of magnitude. It is the most concrete positive
 *       prediction available, and this bench tests it.
 * @note The mechanism, if it works, is decorrelation. Co-arms that estimate one quantity from the
 *       same type of sample carry correlated error. Correlated error does not average out across
 *       arms. A disturbance that is periodic, and not random, decorrelates one arm from the others
 *       in a way that the joint estimator can cancel.
 * @note That is dither. The theory of signals gives it a sound basis. Thus the prediction is not
 *       exotic.
 * @note Three conditions, because two would not separate the mechanism from the disturbance: no
 *       disturbance, periodic disturbance, and random disturbance of the same magnitude. If periodic
 *       beats random, the periodicity does the work. If both beat none equally, any dither does.
 *       If neither beats none, the claim does not hold here.
 */

#include "bench_seed.h"
#include "sha256_core.h"

#include <algorithm>
#include "embed_turnstile.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

/** @brief The frame the sweep selected. */
const unsigned FRAME = 31u;

/** @brief The standard's first round constant. */
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

/**
 * @brief Draws one rotational-XOR delta.
 *
 * @param[in,out] generator  Random source [BORROWS].
 * @param[in]     disturbance Applied to the turned member's message word, zero for none.
 * @return                   The output delta on word zero.
 */
uint32_t draw_delta(std::mt19937 &generator, uint32_t disturbance)
{
    uint32_t plain[8];
    uint32_t turned[8];

    for (unsigned slot = 0u; slot < 8u; slot += 1u)
    {
        const uint32_t value = generator();
        plain[slot] = value;
        turned[slot] = rotate_right(value, FRAME);
    }
    const uint32_t message = generator();

    run_round(plain, message);
    run_round(turned, rotate_right(message, FRAME) ^ disturbance);

    return turned[0] ^ rotate_right(plain[0], FRAME);
}

/**
 * @brief The quantity under estimate: the collision probability of the delta distribution.
 *
 * @param[in] counts How often each delta appeared [BORROWS].
 * @param[in] drawn  How many draws produced them.
 * @return           The unbiased collision probability.
 * @note Unbiased form, because the naive sum of squared frequencies runs high at this sparsity and
 *       would report structure that is not there.
 */
double collision_probability(const std::unordered_map<uint32_t, uint32_t> &counts, uint64_t drawn)
{
    long double colliding = 0.0L;

    for (const auto &entry : counts)
    {
        const long double count = (long double)entry.second;
        colliding += count * (count - 1.0L);
    }
    const long double total = (long double)drawn * ((long double)drawn - 1.0L);
    return (total > 0.0L) ? (double)(colliding / total) : 0.0;
}

/** @brief The type of disturbance on a co-arm. */
enum class Disturbance
{
    None,
    Periodic,
    Random
};

/**
 * @brief Estimates the target from co-arms. One of them may carry a disturbance.
 *
 * @param[in,out] generator Random source [BORROWS].
 * @param[in]     arms      How many co-arms.
 * @param[in]     per_arm   Draws per arm.
 * @param[in]     type      Which disturbance the first arm carries.
 * @param[in]     period    Period of the disturbance where it is periodic.
 * @param[in]     magnitude The disturbance value.
 * @return                  The joint estimate: the mean across arms.
 */
double joint_estimate(std::mt19937 &generator, unsigned arms, unsigned per_arm, Disturbance type, unsigned period,
                      uint32_t magnitude)
{
    double total = 0.0;

    for (unsigned arm = 0u; arm < arms; arm += 1u)
    {
        std::unordered_map<uint32_t, uint32_t> counts;
        counts.reserve(per_arm / 2u);

        for (unsigned draw = 0u; draw < per_arm; draw += 1u)
        {
            uint32_t disturbance = 0u;

            // Only the first arm is ever disturbed. The rest are the undisturbed co-arms whose
            // joint error the claim is about.
            if (arm == 0u)
            {
                if (type == Disturbance::Periodic)
                {
                    disturbance = ((draw % period) == 0u) ? magnitude : 0u;
                }
                else if (type == Disturbance::Random)
                {
                    // Same expected rate as the periodic case. Thus magnitude is not the variable.
                    disturbance = ((generator() % period) == 0u) ? magnitude : 0u;
                }
            }
            counts[draw_delta(generator, disturbance)] += 1u;
        }
        total += collision_probability(counts, per_arm);
    }
    return total / (double)arms;
}

} // namespace

int main()
{
    printf("================================================================\n");
    printf("  The tail as error, and periodic disturbance on a co-arm\n");
    printf("================================================================\n");

    std::mt19937 generator(bench_seed(20260908u));
    const auto started = embed_clock_ns();

    // -------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  1. Head and tail, separated\n");
    printf("================================================================\n");
    printf("\n  The tail is the error term instead of an undersampled part of the signal, so\n");
    printf("  it gets separated and measured instead of apologised for.\n");

    {
        const unsigned drawn = 4000000u;
        std::unordered_map<uint32_t, uint32_t> counts;
        counts.reserve(drawn / 2u);
        for (unsigned draw = 0u; draw < drawn; draw += 1u)
        {
            counts[draw_delta(generator, 0u)] += 1u;
        }

        uint64_t singleton_mass = 0u;
        uint64_t head_mass = 0u;
        size_t singletons = 0u;
        size_t doubletons = 0u;
        size_t head = 0u;

        for (const auto &entry : counts)
        {
            if (entry.second == 1u)
            {
                singletons += 1u;
            }
            if (entry.second == 2u)
            {
                doubletons += 1u;
            }
            if (entry.second <= 2u)
            {
                singleton_mass += entry.second;
            }
            else
            {
                head_mass += entry.second;
                head += 1u;
            }
        }

        printf("\n  %-34s %14s %14s\n", "", "deltas", "mass");
        printf("  %-34s %14s %14s\n", "----------------------------------", "--------------", "--------------");
        printf("  %-34s %14zu %13.4f%%\n", "head, count above two", head,
                    100.0 * (double)head_mass / (double)drawn);
        printf("  %-34s %14zu %13.4f%%\n", "tail, count at or below two", singletons + doubletons,
                    100.0 * (double)singleton_mass / (double)drawn);

        // The code keeps the tail here, and must keep it. The unbiased collision estimator below
        // weights a delta by c(c-1), which is exactly zero for a singleton. Thus that estimator
        // throws the whole tail away by construction. The tail is the most sensitive part of the
        // distribution. Only the singleton and doubleton counts carry information about the part
        // that the sample never saw.
        const double coverage = 1.0 - ((double)singletons / (double)drawn); // Good-Turing coverage.
        const double unseen_mass = (double)singletons / (double)drawn;
        const double chao_total =
            (double)counts.size() +
            ((doubletons > 0u) ? (((double)singletons * (double)singletons) / (2.0 * (double)doubletons)) : 0.0);

        printf("\n  Read through the tail instead of past it:\n");
        printf("    singletons f1              : %zu\n", singletons);
        printf("    doubletons f2              : %zu\n", doubletons);
        printf("    Good-Turing coverage       : %.8f\n", coverage);
        printf("    unseen mass, f1/N          : %.8e\n", unseen_mass);
        printf("    distinct deltas observed   : %zu\n", counts.size());
        printf("    Chao1 estimate of the total: %.0f\n", chao_total);
        printf("    never observed             : %.0f\n", chao_total - (double)counts.size());

        printf("\n  Those last four lines exist only because the tail was kept. f1 and f2 are\n");
        printf("  the sole part of a sample that says anything about what the sample missed,\n");
        printf("  and an estimator that zeroes them can report a distribution it has barely\n");
        printf("  seen as though it had seen all of it.\n");
    }

    // -------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  2. hourly_supposition item 1, tested\n");
    printf("================================================================\n");
    printf("\n  Claim: introducing a periodic disturbance adjusts the joint error toward zero\n");
    printf("  by several orders of magnitude on an undisturbed co-arm.\n");
    printf("\n  Eight co-arms estimate the collision probability of the delta distribution.\n");
    printf("  Only arm zero is ever disturbed. The reference is a single large run, and the\n");
    printf("  error is the distance from it. Three conditions, so periodicity is separable\n");
    printf("  from the mere presence of a disturbance.\n");

    // The reference, from one large undisturbed run.
    double reference = 0.0;
    {
        const unsigned drawn = 8000000u;
        std::unordered_map<uint32_t, uint32_t> counts;
        counts.reserve(drawn / 2u);
        for (unsigned draw = 0u; draw < drawn; draw += 1u)
        {
            counts[draw_delta(generator, 0u)] += 1u;
        }
        reference = collision_probability(counts, drawn);
    }
    printf("\n  reference collision probability : %.8e   (8,000,000 draws)\n", reference);

    const unsigned repeats = 24u;
    const unsigned arms = 8u;
    const unsigned per_arm = 60000u;
    const unsigned period = 64u;
    const uint32_t magnitude = 0x00000001u;

    printf("\n  %-24s %18s %18s\n", "condition", "mean |error|", "vs undisturbed");
    printf("  %-24s %18s %18s\n", "------------------------", "------------------", "------------------");

    double baseline = 0.0;

    for (int condition = 0; condition < 3; condition += 1)
    {
        const Disturbance type = (condition == 0)   ? Disturbance::None
                                 : (condition == 1) ? Disturbance::Periodic
                                                    : Disturbance::Random;
        const char *name = (condition == 0)   ? "no disturbance"
                           : (condition == 1) ? "periodic, period 64"
                                              : "random, same rate";

        double error_total = 0.0;
        for (unsigned repeat = 0u; repeat < repeats; repeat += 1u)
        {
            const double estimate = joint_estimate(generator, arms, per_arm, type, period, magnitude);
            error_total += fabs(estimate - reference);
        }
        const double mean_error = error_total / (double)repeats;
        if (condition == 0)
        {
            baseline = mean_error;
        }

        printf("  %-24s %18.8e %17.4fx\n", name, mean_error, (mean_error > 0.0) ? (baseline / mean_error) : 0.0);
    }

    printf("\n  Reading it. A ratio above one means the disturbance reduced the joint error,\n");
    printf("  which is the claim. Several orders of magnitude would be a ratio in the\n");
    printf("  thousands. A ratio near one means the disturbance did nothing here, and a\n");
    printf("  ratio below one means it hurt.\n");
    printf("\n  If periodic and random come out together, any dither is doing the work and\n");
    printf("  the periodicity is not the mechanism. If periodic wins alone, the period is.\n");

    const auto finished = embed_clock_ns();
    printf("\n================================================================\n");
    printf("  elapsed %.1f s\n", ((double)(finished - started) / 1.0e9));
    printf("================================================================\n");
    return 0;
}
