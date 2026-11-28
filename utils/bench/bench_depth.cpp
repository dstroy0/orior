// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_depth.cpp
 * @brief The price of the closeness premise by depth, and a joint read of two co-arms.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The premise of this project: everything close to a known point is true of it. The bench
 *       bench_closeness measures that on the whole function, and finds nothing.
 * @note Input pairs one bit apart and sixty-four bits apart land the same distance apart in the
 *       digest. That does not show that the premise is false. The premise has a **depth**. The correlation runs 0.798
 * at four rounds, 0.639 at seven, 0.114 at eight, and is noise from ten. It does not fail. It expires.
 * @note Thus the question is not whether to keep it, but where to spend it. Every round costs the
 *       same to compute, and returns a different amount of discrimination. Thus at one depth, the
 *       bits bought per round of work are largest. That depth is a number, and not a preference.
 * @note The bench reads the bits through the mutual information of a bivariate normal,
 *       `-log2(1 - c^2)/2`. That converts a correlation into its worth for discrimination. Division
 *       by the rounds spent gives bits per round. The maximum of that column is the efficient depth.
 * @note Two arms, coupled, and not independent. A round writes `state[0]` and `state[4]`, and shifts
 *       the rest along. H34 establishes that by write mask.
 * @note Thus a round puts something only into those two words. The same input difference on the same pair drives both.
 * Thus they are co-arms, and not two samples. Independent arms are structurally certain to give a null.
 * @note The joint reading is the multiple correlation of the input distance on both arms at once,
 *
 *         R^2 = (rA^2 + rB^2 - 2 rA rB rAB) / (1 - rAB^2)
 *
 *       and the co-arm gain is `R^2` less the better single arm. Where the two arms carry the same
 *       information, the gain is zero, and a joint read buys nothing. Where they carry different
 *       parts of it, the joint beats either. That is the claim about co-arms, in a form that can come
 *       out zero.
 * @note The control is a full-strength one. The bench computes the same statistic against a
 *       splitmix64 chain in place of the round function, where the true correlation is zero at
 *       every depth. Thus the column shows what this estimator returns for nothing at this sample
 *       size. A depth whose reading does not exceed that column is a sample, and not a measurement.
 */

#include "../../src/cu/transpiler/lstar/protocol/table/table_draw.h"
#include "bench_bound.h"
#include "bench_load_limit.h"
#include "bench_seed.h"
#include "sha256_core.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

namespace
{

/** @brief The deepest round this walks to. */
const unsigned DEEPEST = 20u;

/**
 * @brief Flips exactly the requested number of distinct bits of a chaining value.
 *
 * @param[in,out] state     Chaining value to mutate [BORROWS].
 * @param[in,out] generator Random source [BORROWS].
 * @param[in]     wanted    How many bits to flip, at most 256.
 * @note Distinct is essential. Independent draws of positions let two flips land on the same bit
 *       and cancel. A request for sixteen bits then delivers fourteen or twelve, and the pair lands
 *       in the wrong stratum. That gives a wrong label to every row of a table stratified by weight.
 *       Positions that are not distinct can also produce identical pairs, and a zero-bit output
 *       distance that reads as a collision.
 * @note Rejection, and not a shuffle. The counts here are at most sixteen out of 256. Thus a repeat
 *       is rare, and the loop is cheaper than a permutation of the whole index set.
 */
void flip_distinct_bits(Sha256State *state, TableDraw *generator, unsigned wanted)
{
    unsigned chosen[256];
    unsigned placed = 0u;

    while (placed < wanted)
    {
        const unsigned at = table_draw_next(generator) % 256u;
        int already = 0;
        for (unsigned seen = 0u; seen < placed; seen += 1u)
        {
            if (chosen[seen] == at)
            {
                already = 1;
                break;
            }
        }
        if (already == 0)
        {
            chosen[placed] = at;
            placed += 1u;
        }
    }

    for (unsigned at = 0u; at < wanted; at += 1u)
    {
        state->word[chosen[at] / 32u] ^= (1u << (chosen[at] % 32u));
    }
}

/** @brief The standard's round constants, needed to step a round backward. */
const uint32_t ROUND_CONSTANT[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

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
 * @brief Steps one round backward, and recovers the state before it.
 *
 * @param[in,out] state         Eight working variables, moved back one round [BORROWS].
 * @param[in]     round         The round to undo. It selects the constant.
 * @param[in]     schedule_word The message word that the round consumed.
 * @note The round is a bijection, and this is its inverse. For that reason, a backward delta is a
 *       computable object, and not a formal one. Six of the eight words are shifts, and come back by
 *       a shift the other way. The inverse of the two additions, in their order, recovers the two
 *       words that the round wrote.
 * @warning This does not supply a solution. To undo round t needs W[t]. In the mining problem, the
 *          schedule comes from the unknown. Thus a backward walk produces a characteristic, and not
 *          a differential. Do not confuse the two. For that reason alone, a backward reach is not an
 *          attack.
 */
void unstep_round(uint32_t *state, unsigned round, uint32_t schedule_word)
{
    const uint32_t before_a = state[1];
    const uint32_t before_b = state[2];
    const uint32_t before_c = state[3];
    const uint32_t before_e = state[5];
    const uint32_t before_f = state[6];
    const uint32_t before_g = state[7];

    const uint32_t carry_two = mix_low(before_a) + majority(before_a, before_b, before_c);
    const uint32_t carry_one = state[0] - carry_two;
    const uint32_t before_d = state[4] - carry_one;
    const uint32_t before_h =
        carry_one - mix_high(before_e) - choose(before_e, before_f, before_g) - ROUND_CONSTANT[round] - schedule_word;

    state[0] = before_a;
    state[1] = before_b;
    state[2] = before_c;
    state[3] = before_d;
    state[4] = before_e;
    state[5] = before_f;
    state[6] = before_g;
    state[7] = before_h;
}

/**
 * @brief Steps one round forward, matching the standard exactly.
 *
 * @param[in,out] state         Eight working variables [BORROWS].
 * @param[in]     round         Which round.
 * @param[in]     schedule_word The message word.
 */
void step_round(uint32_t *state, unsigned round, uint32_t schedule_word)
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

/** @brief Counts set bits in a word. */
unsigned population(uint32_t value)
{
    unsigned total = 0u;
    while (value != 0u)
    {
        total += (value & 1u);
        value >>= 1;
    }
    return total;
}

/** @brief Running sums for one correlation. */
struct Correlation
{
    double count;
    double sum_left;
    double sum_right;
    double sum_left_squared;
    double sum_right_squared;
    double sum_product;
};

/**
 * @brief Adds one observation to a correlation accumulator.
 *
 * @param[in,out] into  Accumulator [BORROWS].
 * @param[in]     left  One value.
 * @param[in]     right The other.
 */
void observe(Correlation *into, double left, double right)
{
    into->count += 1.0;
    into->sum_left += left;
    into->sum_right += right;
    into->sum_left_squared += left * left;
    into->sum_right_squared += right * right;
    into->sum_product += left * right;
}

/**
 * @brief Reads the Pearson correlation out of an accumulator.
 *
 * @param[in] from Accumulator [BORROWS].
 * @return         The correlation, or zero where either side has no spread.
 */
double correlation_of(const Correlation &from)
{
    const double count = from.count;
    const double top = (count * from.sum_product) - (from.sum_left * from.sum_right);
    const double left_spread = (count * from.sum_left_squared) - (from.sum_left * from.sum_left);
    const double right_spread = (count * from.sum_right_squared) - (from.sum_right * from.sum_right);
    const double bottom = sqrt(left_spread * right_spread);
    return (bottom > 0.0) ? (top / bottom) : 0.0;
}

/**
 * @brief Converts a correlation into the bits of discrimination it carries.
 *
 * @param[in] value The correlation.
 * @return          Mutual information of a bivariate normal with that correlation, in bits.
 * @note This makes correlations at different depths comparable. Thus a bench can divide them by the
 *       rounds spent. A correlation is not additive, and a bit is.
 */
double bits_of(double value)
{
    const double squared = value * value;
    return (squared >= 1.0) ? 64.0 : (-0.5 * log2(1.0 - squared));
}

} // namespace

int main(int argc, char **argv)
{
    bench_lower_priority();

    const unsigned pairs = (argc > 1) ? (unsigned)atoi(argv[1]) : 400000u;

    printf("================================================================\n");
    printf("  Where the closeness premise is worth spending, by depth\n");
    printf("================================================================\n");
    printf("\n  bench_closeness measured the premise on the whole function and found nothing.\n");
    printf("  Reading that as the premise being false was too strong. It has a depth: the\n");
    printf("  correlation runs 0.798 at four rounds and is noise from ten. It expires rather\n");
    printf("  than fails, and where it expires is a number.\n");
    printf("\n  Every round costs the same and returns a different amount, so there is a depth\n");
    printf("  where bits bought per round of work is largest. Correlations are turned into\n");
    printf("  bits through the mutual information of a bivariate normal, because a\n");
    printf("  correlation is not additive and a bit is.\n");
    printf("\n  Two co-arms, coupled by construction: a round writes state[0] and state[4] and\n");
    printf("  shifts the rest along, so those are the only two places it puts anything, and\n");
    printf("  both are driven by the same input difference on the same pair. An earlier\n");
    printf("  attempt here used independent arms and was guaranteed to null.\n");
    printf("\n  Pairs per depth: %u.\n", pairs);

    printf("\n  %6s %10s %10s %10s %12s %12s %10s %10s\n", "rounds", "arm A", "arm B", "A with B", "joint R", "bits",
           "per round", "control");
    printf("  %6s %10s %10s %10s %12s %12s %10s %10s\n", "------", "----------", "----------", "----------",
           "------------", "------------", "----------", "----------");

    double best_efficiency = 0.0;
    unsigned best_depth = 0u;

    for (unsigned rounds = 1u; rounds <= DEEPEST; rounds += 1u)
    {
        Correlation with_a = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        Correlation with_b = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        Correlation a_with_b = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        Correlation control = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

        TableDraw generator;
        table_draw_seeded(&generator, bench_seed(20260908u) + rounds);

        for (unsigned trial = 0u; trial < pairs; trial += 1u)
        {
            uint32_t message[SHA256_BLOCK_WORDS];
            Sha256State left;
            Sha256State right;

            for (unsigned slot = 0u; slot < SHA256_BLOCK_WORDS; slot += 1u)
            {
                message[slot] = table_draw_next(&generator);
            }
            for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
            {
                left.word[slot] = table_draw_next(&generator);
                right.word[slot] = left.word[slot];
            }

            // The code puts the difference on the chaining value, and not on the message. The message
            // word with the nonce does not reach the state until four rounds run. A difference there
            // would measure absence at every shallower depth. H34 exists to prevent that defect, and
            // the code avoids it here.
            const unsigned flips = 1u + (table_draw_next(&generator) % 16u);
            flip_distinct_bits(&right, &generator, flips);

            unsigned input_distance = 0u;
            for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
            {
                input_distance += population(left.word[slot] ^ right.word[slot]);
            }
            if (input_distance != flips)
            {
                // Distinct positions make this exact, a mismatch is a defect in the mutation
                // instead of a pair to skip. Said out loud instead of silently dropped.
                printf("  [!] asked for %u differing bits and got %u, the mutation is wrong\n", flips, input_distance);
                return 1;
            }

            Sha256State left_out = left;
            Sha256State right_out = right;
            sha256_block_compress_partial(&left_out, message, rounds);
            sha256_block_compress_partial(&right_out, message, rounds);

            const double arm_a = (double)population(left_out.word[0] ^ right_out.word[0]);
            const double arm_b = (double)population(left_out.word[4] ^ right_out.word[4]);

            observe(&with_a, (double)input_distance, arm_a);
            observe(&with_b, (double)input_distance, arm_b);
            observe(&a_with_b, arm_a, arm_b);

            uint64_t state = ((uint64_t)input_distance << 32) ^ (uint64_t)table_draw_next(&generator);
            state += 0x9e3779b97f4a7c15ull;
            uint64_t mixed = state;
            mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ull;
            mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebull;
            observe(&control, (double)input_distance, (double)population((uint32_t)mixed));
        }

        const double reading_a = correlation_of(with_a);
        const double reading_b = correlation_of(with_b);
        const double between = correlation_of(a_with_b);
        const double reading_control = correlation_of(control);

        // Multiple correlation of the input distance on both arms at once.
        const double denominator = 1.0 - (between * between);
        double joint_squared =
            (denominator > 1e-12)
                ? (((reading_a * reading_a) + (reading_b * reading_b) - (2.0 * reading_a * reading_b * between)) /
                   denominator)
                : (reading_a * reading_a);
        joint_squared = (joint_squared < 0.0) ? 0.0 : ((joint_squared > 1.0) ? 1.0 : joint_squared);

        const double joint = sqrt(joint_squared);
        const double bits = bits_of(joint);
        const double efficiency = bits / (double)rounds;

        if (efficiency > best_efficiency)
        {
            best_efficiency = efficiency;
            best_depth = rounds;
        }

        printf("  %6u %10.4f %10.4f %10.4f %12.4f %12.6f %10.6f %10.4f\n", rounds, reading_a, reading_b, between, joint,
               bits, efficiency, reading_control);
    }

    printf("\n  most efficient depth : %u rounds, at %.6f bits per round\n", best_depth, best_efficiency);

    // ---------------------------------------------------------------------------------------
    // The same curve, without the average.
    //
    // Everything above is a mean over difference weights one through sixteen. A mean is the wrong
    // readout if the efficiency is a distribution, and not a number. A one-bit difference has one
    // active bit to diffuse, and a sixteen-bit difference has sixteen. Nothing says that they expire
    // at the same depth, and a pool of them reports neither.
    //
    // If any weight survives deeper than the pooled curve says, the pooled curve hides it. A search
    // would want to steer toward exactly those deep survivors.
    // ---------------------------------------------------------------------------------------
    const unsigned STRATA = 5u;
    // Hoisted so the inverted table below reads the same strata as the forward one.

    const unsigned stratum_low[STRATA] = {1u, 2u, 3u, 5u, 9u};
    const unsigned stratum_high[STRATA] = {1u, 2u, 4u, 8u, 16u};

    printf("\n================================================================\n");
    printf("  The same curve stratified by how many bits differ\n");
    printf("================================================================\n");
    printf("\n  A one-bit difference has one active bit to diffuse and a sixteen-bit one has\n");
    printf("  sixteen. Pooling them reports neither, and if any weight survives deeper than\n");
    printf("  the pooled curve says, the pooled curve was hiding it.\n");
    printf("\n  %10s %8s %8s %8s %8s %8s %8s %10s\n", "bits differ", "r=1", "r=2", "r=3", "r=4", "r=5", "r=6",
           "dies at");
    printf("  %10s %8s %8s %8s %8s %8s %8s %10s\n", "-----------", "--------", "--------", "--------", "--------",
           "--------", "--------", "----------");

    for (unsigned band = 0u; band < STRATA; band += 1u)
    {
        // Deeper than the printed columns. The shallowest stratum outlives six rounds, and "past 6"
        // is not a measurement of where it ends.
        const unsigned STRATUM_DEEPEST = 12u;
        double reading[STRATUM_DEEPEST + 1u];
        unsigned dies_at = 0u;

        for (unsigned rounds = 1u; rounds <= STRATUM_DEEPEST; rounds += 1u)
        {
            Correlation joint_arm = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
            TableDraw generator;
            table_draw_seeded(&generator, bench_seed(20260908u) + (rounds * 97u) + band);

            for (unsigned trial = 0u; trial < pairs; trial += 1u)
            {
                uint32_t message[SHA256_BLOCK_WORDS];
                Sha256State left;
                Sha256State right;

                for (unsigned slot = 0u; slot < SHA256_BLOCK_WORDS; slot += 1u)
                {
                    message[slot] = table_draw_next(&generator);
                }
                for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
                {
                    left.word[slot] = table_draw_next(&generator);
                    right.word[slot] = left.word[slot];
                }

                const unsigned span = stratum_high[band] - stratum_low[band] + 1u;
                const unsigned flips = stratum_low[band] + (table_draw_next(&generator) % span);
                flip_distinct_bits(&right, &generator, flips);

                unsigned input_distance = 0u;
                for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
                {
                    input_distance += population(left.word[slot] ^ right.word[slot]);
                }
                if (input_distance != flips)
                {
                    // A stratum whose members are not the weight that it claims has a wrong label. That
                    // is worse than an absent row, because it still prints a number.
                    printf("  [!] stratum asked for %u bits and got %u\n", flips, input_distance);
                    return 1;
                }

                Sha256State left_out = left;
                Sha256State right_out = right;
                sha256_block_compress_partial(&left_out, message, rounds);
                sha256_block_compress_partial(&right_out, message, rounds);

                // The stronger arm: state[4], by about ten to one in the pooled table. A correlation
                // does not apply here. Within a stratum of fixed weight, the input distance is a
                // constant. Thus its variance is zero, and a correlation against it has no value.
                // It would print 0.0000 for the weight-one and weight-two rows, which reads as "dies
                // immediately" and means "not computed".
                //
                // The mean output distance works at fixed weight, and everywhere else. After a
                // difference diffuses, the output word differs in a Binomial(32, 1/2) number of
                // bits. Thus the mean sits at 16, with a standard deviation of sqrt(32)/2. The
                // distance of the measured mean from 16, in standard errors, has a value for every
                // stratum, and an exact null that nothing here fitted.
                observe(&joint_arm, (double)input_distance, (double)population(left_out.word[4] ^ right_out.word[4]));
            }

            const double seen = joint_arm.count;
            const double mean_out = (seen > 0.0) ? (joint_arm.sum_right / seen) : 16.0;
            const double standard_error = (sqrt(32.0) / 2.0) / sqrt(seen);
            reading[rounds] = (standard_error > 0.0) ? ((mean_out - 16.0) / standard_error) : 0.0;

            // Four standard errors: the floor that this estimator can see past. Below it, a reading is
            // a draw, and not a measurement.
            if ((dies_at == 0u) && (fabs(reading[rounds]) < 4.0))
            {
                dies_at = rounds;
            }
        }

        char band_label[16];
        if (stratum_low[band] == stratum_high[band])
        {
            snprintf(band_label, sizeof(band_label), "%u", stratum_low[band]);
        }
        else
        {
            snprintf(band_label, sizeof(band_label), "%u to %u", stratum_low[band], stratum_high[band]);
        }

        char dies_label[12];
        if (dies_at == 0u)
        {
            snprintf(dies_label, sizeof(dies_label), "past %u", STRATUM_DEEPEST);
        }
        else
        {
            snprintf(dies_label, sizeof(dies_label), "%u", dies_at);
        }

        printf("  %10s %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f %10s\n", band_label, reading[1], reading[2], reading[3],
               reading[4], reading[5], reading[6], dies_label);
    }

    // ---------------------------------------------------------------------------------------
    // The same operation seen from the other end.
    //
    // This is not a second attack surface, and not a reverse delta worth a chase. The round is a
    // bijection. Thus a backward run is the identical map, read from the far side. To undo round t
    // requires W[t], and in the mining problem that comes from the unknown. A backward walk
    // therefore produces a characteristic, and not a differential.
    //
    // It is a control. If the round really is a bijection, the depth law must match in both
    // directions. The asymmetry between the two written words must also mirror.
    // Forward, state[4] is one addition, and state[0] carries both carries plus mix_low and
    // majority. Backward, the shifted word is the cheap one, and the heavily mixed word is the
    // other. A backward reading that did not mirror would show an error in the read of the forward one.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  The same operation from the other end, as a control\n");
    printf("================================================================\n");

    {
        TableDraw generator;
        table_draw_seeded(&generator, bench_seed(20260908u) + 5000u);
        unsigned round_trips = 0u;
        unsigned round_trip_failures = 0u;

        for (unsigned trial = 0u; trial < 20000u; trial += 1u)
        {
            uint32_t state[8];
            uint32_t kept[8];
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                state[slot] = table_draw_next(&generator);
                kept[slot] = state[slot];
            }
            const uint32_t word = table_draw_next(&generator);
            const unsigned round = table_draw_next(&generator) % 64u;

            step_round(state, round, word);
            unstep_round(state, round, word);

            round_trips += 1u;
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                if (state[slot] != kept[slot])
                {
                    round_trip_failures += 1u;
                    break;
                }
            }
        }

        printf("\n  %-46s %10u\n", "round trips forward then back", round_trips);
        printf("  %-46s %10u\n", "of those that did not return", round_trip_failures);
        if (round_trip_failures != 0u)
        {
            printf("\n  [!] the inverse is not the inverse, so nothing below it means anything.\n");
            return 1;
        }
    }

    printf("\n  %10s %10s %10s %10s %10s\n", "rounds", "forward", "backward", "fwd cheap", "back cheap");
    printf("  %10s %10s %10s %10s %10s\n", "----------", "----------", "----------", "----------", "----------");

    for (unsigned rounds = 1u; rounds <= 6u; rounds += 1u)
    {
        Correlation forward_arm = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        Correlation backward_arm = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        Correlation forward_cheap = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        Correlation backward_cheap = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        TableDraw generator;
        table_draw_seeded(&generator, bench_seed(20260908u) + (rounds * 31u) + 900u);

        for (unsigned trial = 0u; trial < pairs; trial += 1u)
        {
            uint32_t words[64];
            for (unsigned slot = 0u; slot < 64u; slot += 1u)
            {
                words[slot] = table_draw_next(&generator);
            }

            uint32_t left[8];
            uint32_t right[8];
            uint32_t back_left[8];
            uint32_t back_right[8];
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                left[slot] = table_draw_next(&generator);
                right[slot] = left[slot];
                back_left[slot] = left[slot];
                back_right[slot] = left[slot];
            }

            const unsigned flips = 1u + (table_draw_next(&generator) % 4u);
            unsigned chosen[4];
            unsigned placed = 0u;
            while (placed < flips)
            {
                const unsigned at = table_draw_next(&generator) % 256u;
                int already = 0;
                for (unsigned seen = 0u; seen < placed; seen += 1u)
                {
                    already |= (chosen[seen] == at) ? 1 : 0;
                }
                if (already == 0)
                {
                    chosen[placed] = at;
                    placed += 1u;
                }
            }
            for (unsigned at = 0u; at < flips; at += 1u)
            {
                right[chosen[at] / 32u] ^= (1u << (chosen[at] % 32u));
                back_right[chosen[at] / 32u] ^= (1u << (chosen[at] % 32u));
            }

            for (unsigned step = 0u; step < rounds; step += 1u)
            {
                step_round(left, step, words[step]);
                step_round(right, step, words[step]);
                unstep_round(back_left, rounds - 1u - step, words[rounds - 1u - step]);
                unstep_round(back_right, rounds - 1u - step, words[rounds - 1u - step]);
            }

            // Forward writes state[0] and state[4]; state[4] is the single addition. Backward
            // writes state[3] and state[7]; state[3] is the single subtraction. Those are the two
            // that should mirror.
            observe(&forward_arm, (double)flips, (double)population(left[0] ^ right[0]));
            observe(&forward_cheap, (double)flips, (double)population(left[4] ^ right[4]));
            observe(&backward_arm, (double)flips, (double)population(back_left[7] ^ back_right[7]));
            observe(&backward_cheap, (double)flips, (double)population(back_left[3] ^ back_right[3]));
        }

        const double forward_mean = forward_arm.sum_right / forward_arm.count;
        const double backward_mean = backward_arm.sum_right / backward_arm.count;
        const double forward_cheap_mean = forward_cheap.sum_right / forward_cheap.count;
        const double backward_cheap_mean = backward_cheap.sum_right / backward_cheap.count;
        const double standard_error = (sqrt(32.0) / 2.0) / sqrt(forward_arm.count);

        printf("  %10u %10.1f %10.1f %10.1f %10.1f\n", rounds, (forward_mean - 16.0) / standard_error,
               (backward_mean - 16.0) / standard_error, (forward_cheap_mean - 16.0) / standard_error,
               (backward_cheap_mean - 16.0) / standard_error);
    }

    printf("\n  Columns are the distance of the mean output distance from 16, in standard\n");
    printf("  errors, for the heavily mixed word and the cheap word in each direction. The\n");
    printf("  two directions should track each other and the two cheap columns should track\n");
    printf("  each other, because it is one bijection read from two ends. Where they do, the\n");
    printf("  forward reading is understood; where they do not, it is not.\n");

    // ---------------------------------------------------------------------------------------
    // The same strata, inverted.
    //
    // Forward, the scarce strata survive deepest. Weight one lasts to round seven, and weight nine
    // to sixteen dies by three. The population of a stratum grows about eight bits per unit of
    // weight. If that order also holds backward, then scarcity is optimal over the whole set, and
    // not in one direction. Test that claim, and do not assume it. Backward diffusion measures
    // about twice as slow, and a different rate can carry a different order.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  The same strata inverted, to see whether scarcity still wins\n");
    printf("================================================================\n");
    printf("\n  %10s %8s %8s %8s %8s %8s %8s %10s\n", "bits differ", "r=1", "r=2", "r=3", "r=4", "r=6", "r=8",
           "dies at");
    printf("  %10s %8s %8s %8s %8s %8s %8s %10s\n", "----------", "--------", "--------", "--------", "--------",
           "--------", "--------", "----------");

    for (unsigned band = 0u; band < STRATA; band += 1u)
    {
        const unsigned BACK_DEEPEST = 14u;
        double reading[BACK_DEEPEST + 1u];
        unsigned dies_at = 0u;

        for (unsigned rounds = 1u; rounds <= BACK_DEEPEST; rounds += 1u)
        {
            Correlation arm = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
            TableDraw generator;
            table_draw_seeded(&generator, bench_seed(20260908u) + (rounds * 53u) + band + 300u);

            for (unsigned trial = 0u; trial < pairs; trial += 1u)
            {
                uint32_t words[16];
                for (unsigned slot = 0u; slot < 16u; slot += 1u)
                {
                    words[slot] = table_draw_next(&generator);
                }

                Sha256State left;
                Sha256State right;
                for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
                {
                    left.word[slot] = table_draw_next(&generator);
                    right.word[slot] = left.word[slot];
                }

                const unsigned span = stratum_high[band] - stratum_low[band] + 1u;
                const unsigned flips = stratum_low[band] + (table_draw_next(&generator) % span);
                flip_distinct_bits(&right, &generator, flips);

                for (unsigned step = 0u; step < rounds; step += 1u)
                {
                    const unsigned which = rounds - 1u - step;
                    unstep_round(left.word, which, words[which]);
                    unstep_round(right.word, which, words[which]);
                }

                // Backward the round writes state[3] and state[7]; state[7] is the heavily mixed
                // one, matching state[0] forward.
                observe(&arm, (double)flips, (double)population(left.word[7] ^ right.word[7]));
            }

            const double seen = arm.count;
            const double mean_out = (seen > 0.0) ? (arm.sum_right / seen) : 16.0;
            const double standard_error = (sqrt(32.0) / 2.0) / sqrt(seen);
            reading[rounds] = (mean_out - 16.0) / standard_error;

            if ((dies_at == 0u) && (fabs(reading[rounds]) < 4.0))
            {
                dies_at = rounds;
            }
        }

        char band_label[16];
        if (stratum_low[band] == stratum_high[band])
        {
            snprintf(band_label, sizeof(band_label), "%u", stratum_low[band]);
        }
        else
        {
            snprintf(band_label, sizeof(band_label), "%u to %u", stratum_low[band], stratum_high[band]);
        }

        char dies_label[12];
        if (dies_at == 0u)
        {
            snprintf(dies_label, sizeof(dies_label), "past %u", BACK_DEEPEST);
        }
        else
        {
            snprintf(dies_label, sizeof(dies_label), "%u", dies_at);
        }

        printf("  %10s %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f %10s\n", band_label, reading[1], reading[2], reading[3],
               reading[4], reading[6], reading[8], dies_label);
    }

    printf("\n  If the ordering here matches the forward one, scarcity is optimal over the\n");
    printf("  whole set and not in one direction only. If it inverts, there is a region where\n");
    printf("  the abundant strata are the better place to be and the rule has an exception.\n");

    // ---------------------------------------------------------------------------------------
    // Is the backward gain real, or did the choice of which word to read create it?
    //
    // The two tables above read state[0] forward and state[7] backward. An argument about which
    // word each direction mixes hardest picks them. That is a choice, and a choice about the
    // readout can drive a result. A read of every word, with the word that survives longest, removes
    // it. If the backward advantage holds when neither direction has a favorite, it comes from the
    // function. If it collapses, it came from the choice.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  The same asymmetry with no word chosen, weight one only\n");
    printf("================================================================\n");
    printf("\n  %10s %10s %10s %12s %12s\n", "rounds", "forward", "backward", "fwd word", "back word");
    printf("  %10s %10s %10s %12s %12s\n", "----------", "----------", "----------", "------------", "------------");

    for (unsigned rounds = 4u; rounds <= 12u; rounds += 1u)
    {
        double best_forward = 0.0;
        double best_backward = 0.0;
        unsigned forward_word = 0u;
        unsigned backward_word = 0u;

        for (unsigned word = 0u; word < 8u; word += 1u)
        {
            Correlation ahead = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
            Correlation behind = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
            TableDraw generator;
            table_draw_seeded(&generator, bench_seed(20260908u) + (rounds * 17u) + word + 7000u);

            for (unsigned trial = 0u; trial < pairs; trial += 1u)
            {
                uint32_t words[16];
                for (unsigned slot = 0u; slot < 16u; slot += 1u)
                {
                    words[slot] = table_draw_next(&generator);
                }

                Sha256State base;
                for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
                {
                    base.word[slot] = table_draw_next(&generator);
                }
                Sha256State altered = base;
                flip_distinct_bits(&altered, &generator, 1u);

                Sha256State left_ahead = base;
                Sha256State right_ahead = altered;
                Sha256State left_behind = base;
                Sha256State right_behind = altered;

                for (unsigned step = 0u; step < rounds; step += 1u)
                {
                    sha256_block_compress_partial(&left_ahead, words, 1u);
                    sha256_block_compress_partial(&right_ahead, words, 1u);
                    const unsigned which = rounds - 1u - step;
                    unstep_round(left_behind.word, which, words[which]);
                    unstep_round(right_behind.word, which, words[which]);
                }

                observe(&ahead, 1.0, (double)population(left_ahead.word[word] ^ right_ahead.word[word]));
                observe(&behind, 1.0, (double)population(left_behind.word[word] ^ right_behind.word[word]));
            }

            const double error = (sqrt(32.0) / 2.0) / sqrt(ahead.count);
            const double ahead_score = fabs(((ahead.sum_right / ahead.count) - 16.0) / error);
            const double behind_score = fabs(((behind.sum_right / behind.count) - 16.0) / error);

            if (ahead_score > best_forward)
            {
                best_forward = ahead_score;
                forward_word = word;
            }
            if (behind_score > best_backward)
            {
                best_backward = behind_score;
                backward_word = word;
            }
        }

        printf("  %10u %10.1f %10.1f %12u %12u\n", rounds, best_forward, best_backward, forward_word, backward_word);
    }

    // ---------------------------------------------------------------------------------------
    // The strata again, with no word chosen, in both directions.
    //
    // The two stratified tables above each read one fixed word. The all-words scan shows that the
    // choice is wrong in both directions. The word that survives is the least overwritten end of the
    // shift chain, and not the most mixed. The order in those tables is safe, because the bench
    // reads every stratum the same way. But the depths are lower bounds. These are the depths
    // themselves.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  Every stratum, both directions, best of all eight words\n");
    printf("================================================================\n");
    printf("\n  %12s %14s %16s %16s\n", "bits differ", "differences", "forward dies", "inverted dies");
    printf("  %12s %14s %16s %16s\n", "------------", "--------------", "----------------", "----------------");

    for (unsigned band = 0u; band < STRATA; band += 1u)
    {
        unsigned forward_dies = 0u;
        unsigned backward_dies = 0u;

        for (unsigned rounds = 1u; rounds <= 16u; rounds += 1u)
        {
            double best_forward = 0.0;
            double best_backward = 0.0;

            for (unsigned word = 0u; word < 8u; word += 1u)
            {
                Correlation ahead = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
                Correlation behind = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
                TableDraw generator;
                table_draw_seeded(&generator, bench_seed(20260908u) + (rounds * 11u) + (word * 3u) + band + 40000u);

                for (unsigned trial = 0u; trial < (pairs / 4u); trial += 1u)
                {
                    uint32_t words[20];
                    for (unsigned slot = 0u; slot < 20u; slot += 1u)
                    {
                        words[slot] = table_draw_next(&generator);
                    }

                    Sha256State base;
                    for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
                    {
                        base.word[slot] = table_draw_next(&generator);
                    }
                    const unsigned span = stratum_high[band] - stratum_low[band] + 1u;
                    const unsigned flips = stratum_low[band] + (table_draw_next(&generator) % span);
                    Sha256State altered = base;
                    flip_distinct_bits(&altered, &generator, flips);

                    Sha256State left_ahead = base;
                    Sha256State right_ahead = altered;
                    Sha256State left_behind = base;
                    Sha256State right_behind = altered;

                    for (unsigned step = 0u; step < rounds; step += 1u)
                    {
                        sha256_block_compress_partial(&left_ahead, words, 1u);
                        sha256_block_compress_partial(&right_ahead, words, 1u);
                        const unsigned which = rounds - 1u - step;
                        unstep_round(left_behind.word, which, words[which]);
                        unstep_round(right_behind.word, which, words[which]);
                    }

                    observe(&ahead, 1.0, (double)population(left_ahead.word[word] ^ right_ahead.word[word]));
                    observe(&behind, 1.0, (double)population(left_behind.word[word] ^ right_behind.word[word]));
                }

                const double error = (sqrt(32.0) / 2.0) / sqrt(ahead.count);
                best_forward = bench_max(best_forward, fabs(((ahead.sum_right / ahead.count) - 16.0) / error));
                best_backward = bench_max(best_backward, fabs(((behind.sum_right / behind.count) - 16.0) / error));
            }

            if ((forward_dies == 0u) && (best_forward < 4.0))
            {
                forward_dies = rounds;
            }
            if ((backward_dies == 0u) && (best_backward < 4.0))
            {
                backward_dies = rounds;
            }
        }

        char band_label[16];
        if (stratum_low[band] == stratum_high[band])
        {
            snprintf(band_label, sizeof(band_label), "%u", stratum_low[band]);
        }
        else
        {
            snprintf(band_label, sizeof(band_label), "%u to %u", stratum_low[band], stratum_high[band]);
        }

        // How many differences of this weight exist: the scarcity that pays for the depth.
        long double population_here = 0.0L;
        for (unsigned weight = stratum_low[band]; weight <= stratum_high[band]; weight += 1u)
        {
            long double count = 1.0L;
            for (unsigned step = 0u; step < weight; step += 1u)
            {
                count = count * (long double)(256u - step) / (long double)(step + 1u);
            }
            population_here += count;
        }

        char forward_label[12];
        char backward_label[12];
        snprintf(forward_label, sizeof(forward_label), "%s%u", (forward_dies == 0u) ? "past " : "",
                 (forward_dies == 0u) ? 16u : forward_dies);
        snprintf(backward_label, sizeof(backward_label), "%s%u", (backward_dies == 0u) ? "past " : "",
                 (backward_dies == 0u) ? 16u : backward_dies);

        printf("  %12s %14.3g %16s %16s\n", band_label, (double)population_here, forward_label, backward_label);
    }

    printf("\n  Scarcity is not a trade here. The scarce stratum has fewer members to search\n");
    printf("  and stays visible longer, so both axes improve together, which is what makes\n");
    printf("  the gain read as exponential instead of as a bargain struck.\n");

    printf("\n  Both directions now get the best of all eight words, so neither is favoured\n");
    printf("  by a choice made in advance. A backward advantage that survives this is the\n");
    printf("  function being genuinely asymmetric under inversion, which a bijection is\n");
    printf("  entitled to be: invertible does not mean symmetric, and a map is not its own\n");
    printf("  inverse. A backward advantage that vanishes here was the readout all along.\n");

    printf("\n  A row that dies later than the pooled curve is a region a search should steer\n");
    printf("  toward, and it would be invisible to the mean. A table where every row dies at\n");
    printf("  the same depth says the pooled reading was not hiding anything and the\n");
    printf("  efficiency really is one number instead of a distribution.\n");

    printf("\n================================================================\n");
    printf("  Reading it\n");
    printf("================================================================\n");
    printf("\n  The control column is what this estimator returns when the true correlation\n");
    printf("  is zero, at this sample size. A depth whose arms do not exceed it has not been\n");
    printf("  measured, it has been sampled, and its bits are sampling noise dressed as\n");
    printf("  discrimination.\n");
    printf("\n  The joint column against the better single arm is the co-arm claim, in a form\n");
    printf("  that can come out zero. Two arms carrying the same information give a joint\n");
    printf("  equal to the better of them and reading them together buys nothing. Only where\n");
    printf("  they carry different parts does the joint exceed both.\n");
    printf("\n  The efficient depth is where bits per round peaks. It is not the depth with\n");
    printf("  the most signal, which is always the shallowest, and it is not the depth where\n");
    printf("  the signal dies. It is where the trade between them turns, and steering there\n");
    printf("  is what keeps the premise instead of rejecting it.\n");
    return 0;
}
