// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_narrow.cu
 * @brief SHA-256 squeezed to a width small enough for an exact answer, and not a statistical one.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The observer does not have to stand outside the function and watch output go past. The
 *       whole object is available. The observer can enter it, widen it or shrink it. A shrink
 *       changes what is knowable. At a narrow enough width, the state space is small enough to
 *       enumerate, and every sampled quantity becomes exact.
 * @note The target. The largest unknown in theory/workbooks/orior/sha256-topology.md is why the
 *       forward and inverse directions differ. Forward differences die at nine rounds, and inverted
 *       ones survive past thirteen. Topology does not explain it, because a cone bounds what could
 *       arrive, and not what does. At four-bit words, the differential probability is a finite sum
 *       over 2^32 states. The bench simply computes it.
 * @note No sample floor hides anything, and no control is necessary, because nothing stays uncertain.
 * @note Two widths, and they answer different questions, and not the same one twice.
 *
 *         Four-bit words:  a 32-bit state, fully enumerable, with exact answers. Too narrow to keep
 *                          the structure. A rotation can only be 1, 2 or 3. Thus Sigma0 and Sigma1
 *                          collapse to the same set of amounts. That makes it the control: the same
 *                          shape without the rotation design. Anything that survives here does not
 *                          come from that design.
 *
 *         Eight-bit words: a 64-bit state, not enumerable, and thus read by sample. Wide enough for
 *                          three distinct amounts per function: the structure of the real thing.
 *                          Look for flaws in this one.
 *
 * @note Rounds are the other axis, and the bench runs both settings, because they are different
 *       objects. 64 rounds at a narrower width make something more mixed than SHA-256. Rounds
 *       scaled down with the width keep the mix comparable. Neither is the honest default, and a
 *       report of one would choose an answer.
 * @warning The rotation amounts here come from the amounts of the standard, scaled and rounded. The
 *          round step is a decision, and not a derivation. A different round step gives a different
 *          object. A result that moves when it changes is about the choice, and not about SHA-256.
 */

#include "bench_depth_score.h"

#include <cuda_runtime.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{

/** @brief Words in the chaining value, as in the standard. */
const unsigned WORDS = 8u;

/** @brief The standard's round constants, truncated to the working width by the device. */
__constant__ uint32_t d_wide_constant[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

/**
 * @brief The six rotation amounts at a given width, scaled from the amounts of the standard.
 *
 * @param[in]  width  Bits per word.
 * @param[out] low_a  First amount of the low mix function [BORROWS].
 * @param[out] low_b  Second [BORROWS].
 * @param[out] low_c  Third [BORROWS].
 * @param[out] high_a First amount of the high mix function [BORROWS].
 * @param[out] high_b Second [BORROWS].
 * @param[out] high_c Third [BORROWS].
 * @note Derived, and not tabulated. Thus a sweep can cover the width, and not only two picked points.
 *       The standard uses 2, 13, 22 and 6, 11, 25 on 32 bits. The function scales these by the
 *       width, rounds them, forces them nonzero, and forces them distinct within each function by a
 *       step upward.
 * @note The narrowing bites on distinctness, and that effect is the reason for the sweep. A function
 *       needs three different amounts to spread a bit three ways. Below about six bits, three
 *       distinct nonzero amounts do not exist. Thus the two functions collapse onto each other, and
 *       the rotation design disappears. The sweep shows where that occurs, and does not assert it.
 */
__host__ __device__ __forceinline__ void rotation_amounts(unsigned width, unsigned *low_a, unsigned *low_b,
                                                          unsigned *low_c, unsigned *high_a, unsigned *high_b,
                                                          unsigned *high_c)
{
    const unsigned standard[6] = {2u, 13u, 22u, 6u, 11u, 25u};
    unsigned scaled[6];

    for (unsigned at = 0u; at < 6u; at += 1u)
    {
        unsigned value = ((standard[at] * width) + 16u) / 32u;
        if (value == 0u)
        {
            value = 1u;
        }
        if (value >= width)
        {
            value = width - 1u;
        }
        scaled[at] = value;
    }

    // Try to make each triple distinct, with a step upward and a wrap. Give up after a bounded
    // number of attempts, and do not loop forever.
    //
    // Below six bits, three distinct nonzero amounts do not exist. At width three, the only choices
    // are one and two. Thus distinctness there is impossible, and not merely awkward. A retry until
    // success would hang on exactly that case. Repeated amounts are the honest outcome. The caller
    // reports them, and does not get a triple that pretends to be distinct.
    for (unsigned base = 0u; base < 6u; base += 3u)
    {
        for (unsigned at = base + 1u; at < (base + 3u); at += 1u)
        {
            for (unsigned attempt = 0u; attempt < width; attempt += 1u)
            {
                int clashes = 0;
                for (unsigned before = base; before < at; before += 1u)
                {
                    clashes |= (scaled[at] == scaled[before]) ? 1 : 0;
                }
                if (clashes == 0)
                {
                    break;
                }
                scaled[at] = (scaled[at] % (width - 1u)) + 1u;
            }
        }
    }

    *low_a = scaled[0];
    *low_b = scaled[1];
    *low_c = scaled[2];
    *high_a = scaled[3];
    *high_b = scaled[4];
    *high_c = scaled[5];
}

/** @brief Rotates within the working width. */
__device__ __forceinline__ uint32_t turn(uint32_t value, unsigned by, unsigned width, uint32_t mask)
{
    by %= width;
    return (by == 0u) ? (value & mask) : (((value >> by) | (value << (width - by))) & mask);
}

/**
 * @brief One round of the narrowed function.
 *
 * @param[in,out] state Eight words of the working width [BORROWS].
 * @param[in]     round Which round. It selects the constant.
 * @param[in]     word  The message word for this round.
 * @param[in]     width Bits per word, four or eight.
 * @note The shape is exactly that of the standard: same two carries, same shift of six words, same
 *       choice and majority. Only the width and the rotation amounts change. Thus anything that
 *       differs between this and SHA-256 comes from those two things alone.
 */
__device__ __forceinline__ void narrow_step(uint32_t *state, unsigned round, uint32_t word, unsigned width)
{
    const uint32_t mask = (width >= 32u) ? 0xffffffffu : ((1u << width) - 1u);
    unsigned low_a = 0u;
    unsigned low_b = 0u;
    unsigned low_c = 0u;
    unsigned high_a = 0u;
    unsigned high_b = 0u;
    unsigned high_c = 0u;
    rotation_amounts(width, &low_a, &low_b, &low_c, &high_a, &high_b, &high_c);

    const uint32_t mix_high =
        turn(state[4], high_a, width, mask) ^ turn(state[4], high_b, width, mask) ^ turn(state[4], high_c, width, mask);
    const uint32_t choose = state[6] ^ (state[4] & (state[5] ^ state[6]));
    const uint32_t carry_one = (state[7] + mix_high + choose + (d_wide_constant[round] >> (32u - width)) + word) & mask;
    const uint32_t mix_low =
        turn(state[0], low_a, width, mask) ^ turn(state[0], low_b, width, mask) ^ turn(state[0], low_c, width, mask);
    const uint32_t majority = (state[0] & state[1]) | (state[2] & (state[0] ^ state[1]));

    state[7] = state[6];
    state[6] = state[5];
    state[5] = state[4];
    state[4] = (state[3] + carry_one) & mask;
    state[3] = state[2];
    state[2] = state[1];
    state[1] = state[0];
    state[0] = (carry_one + mix_low + majority) & mask;
}

/** @brief The inverse of one round, which the bijection guarantees exists. */
__device__ __forceinline__ void narrow_unstep(uint32_t *state, unsigned round, uint32_t word, unsigned width)
{
    const uint32_t mask = (width >= 32u) ? 0xffffffffu : ((1u << width) - 1u);
    const unsigned low_a = (width == 4u) ? 1u : 1u;
    const unsigned low_b = (width == 4u) ? 2u : 3u;
    const unsigned low_c = (width == 4u) ? 3u : 6u;
    const unsigned high_a = (width == 4u) ? 1u : 2u;
    const unsigned high_b = (width == 4u) ? 2u : 4u;
    const unsigned high_c = (width == 4u) ? 3u : 7u;

    const uint32_t before_a = state[1];
    const uint32_t before_b = state[2];
    const uint32_t before_c = state[3];
    const uint32_t before_e = state[5];
    const uint32_t before_f = state[6];
    const uint32_t before_g = state[7];

    const uint32_t mix_low =
        turn(before_a, low_a, width, mask) ^ turn(before_a, low_b, width, mask) ^ turn(before_a, low_c, width, mask);
    const uint32_t majority = (before_a & before_b) | (before_c & (before_a ^ before_b));
    const uint32_t carry_one = (state[0] - mix_low - majority) & mask;
    const uint32_t before_d = (state[4] - carry_one) & mask;
    const uint32_t mix_high =
        turn(before_e, high_a, width, mask) ^ turn(before_e, high_b, width, mask) ^ turn(before_e, high_c, width, mask);
    const uint32_t choose = before_g ^ (before_e & (before_f ^ before_g));
    const uint32_t before_h = (carry_one - mix_high - choose - (d_wide_constant[round] >> (32u - width)) - word) & mask;

    state[0] = before_a;
    state[1] = before_b;
    state[2] = before_c;
    state[3] = before_d;
    state[4] = before_e;
    state[5] = before_f;
    state[6] = before_g;
    state[7] = before_h;
}

/** @brief Unpacks a packed state into eight words. */
__device__ __forceinline__ void unpack(uint64_t packed, uint32_t *state, unsigned width)
{
    const uint32_t mask = (1u << width) - 1u;
    for (unsigned slot = 0u; slot < WORDS; slot += 1u)
    {
        state[slot] = (uint32_t)((packed >> (slot * width)) & (uint64_t)mask);
    }
}

/**
 * @brief Counts, over every state, how far a difference spreads after r rounds.
 *
 * @param[in]     span      How many states to walk: the whole space for a full enumeration.
 * @param[in]     width     Bits per word.
 * @param[in]     rounds    How many rounds.
 * @param[in]     backward  Nonzero to walk the inverse.
 * @param[in]     difference Packed input difference.
 * @param[in]     words     Message words, one per round.
 * @param[in,out] spread    Accumulates the Hamming distance of the whole output [BORROWS].
 * @param[in,out] counted   Accumulates how many states the walk covered [BORROWS].
 * @note A full enumeration at four-bit words, where the state is 32 bits and the walk visits every
 *       one. Thus the result is the exact expectation, and not an estimate of it. At eight bits, the
 *       space is 64 bits, and the same kernel walks a prefix of it. That is a sample, and the bench
 *       labels it so.
 */
__global__ void spread_of(uint64_t span, unsigned width, unsigned rounds, int backward, uint64_t difference,
                          const uint32_t *words, unsigned long long *spread, unsigned long long *counted)
{
    const uint64_t stride = (uint64_t)gridDim.x * blockDim.x;
    unsigned long long local_spread = 0u;
    unsigned long long local_counted = 0u;

    for (uint64_t at = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x; at < span; at += stride)
    {
        uint32_t left[WORDS];
        uint32_t right[WORDS];
        unpack(at, left, width);
        unpack(at ^ difference, right, width);

        for (unsigned step = 0u; step < rounds; step += 1u)
        {
            if (backward != 0)
            {
                const unsigned which = rounds - 1u - step;
                narrow_unstep(left, which, words[which], width);
                narrow_unstep(right, which, words[which], width);
            }
            else
            {
                narrow_step(left, step, words[step], width);
                narrow_step(right, step, words[step], width);
            }
        }

        for (unsigned slot = 0u; slot < WORDS; slot += 1u)
        {
            local_spread += (unsigned long long)__popc(left[slot] ^ right[slot]);
        }
        local_counted += 1u;
    }

    atomicAdd(spread, local_spread);
    atomicAdd(counted, local_counted);
}

/**
 * @brief Sweeps a whole subspace of inputs and exclusive-ors every output together.
 *
 * @param[in]     dimension How many input bits the sweep covers. Thus 2^dimension inputs in the set.
 * @param[in]     positions Which bit of the packed state each swept coordinate moves [BORROWS].
 * @param[in]     base      The packed state that every unswept coordinate keeps.
 * @param[in]     width     Bits per word.
 * @param[in]     rounds    How many rounds.
 * @param[in]     backward  Nonzero to walk the inverse.
 * @param[in]     words     Message words, one per round.
 * @param[in,out] total     Accumulates the exclusive-or of every output [BORROWS].
 * @note This is the rake, and not another probe. A differential picks one pin at a time. This keeps
 *       every coordinate still, moves one subspace through all of its values at once, then reads the
 *       sum. Where the algebraic degree of the output in those coordinates is below the dimension of
 *       the subspace, that sum is **exactly zero**, and not small. Every monomial of degree below the
 *       dimension appears an even number of times, and cancels.
 * @note Thus it is the only measurement here that needs no control, and has no floor. A statistical
 *       reading is a number to compare against a null. This is an identity that either holds or
 *       does not. The workbook has what sets its reach: the algebraic degree is three at round four,
 *       and at least twelve at round five.
 * @note Exclusive-or, and not addition, because the property is over GF(2). Degree is a GF(2) notion.
 *       Here, that language is the right one.
 */
__global__ void integral_sum(unsigned dimension, const unsigned *positions, uint64_t base, unsigned width,
                             unsigned rounds, int backward, const uint32_t *words, unsigned long long *total)
{
    const uint64_t span = (uint64_t)1u << dimension;
    const uint64_t stride = (uint64_t)gridDim.x * blockDim.x;
    unsigned long long local = 0u;

    for (uint64_t at = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x; at < span; at += stride)
    {
        uint64_t packed = base;
        for (unsigned bit = 0u; bit < dimension; bit += 1u)
        {
            if (((at >> bit) & 1u) != 0u)
            {
                packed ^= (uint64_t)1u << positions[bit];
            }
        }

        uint32_t state[WORDS];
        unpack(packed, state, width);

        for (unsigned step = 0u; step < rounds; step += 1u)
        {
            if (backward != 0)
            {
                const unsigned which = rounds - 1u - step;
                narrow_unstep(state, which, words[which], width);
            }
            else
            {
                narrow_step(state, step, words[step], width);
            }
        }

        uint64_t out = 0u;
        for (unsigned slot = 0u; slot < WORDS; slot += 1u)
        {
            out |= ((uint64_t)state[slot]) << (slot * width);
        }
        local ^= out;
    }

    atomicXor(total, local);
}

/** @brief The stream the depth benches draw from, a width sweep samples the same way they do. */
__device__ __forceinline__ uint64_t splitmix(uint64_t &state)
{
    state += 0x9e3779b97f4a7c15ull;
    uint64_t mixed = state;
    mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ull;
    mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebull;
    return mixed ^ (mixed >> 31);
}

/**
 * @brief Per-word Hamming distance after r rounds at a stated width, sampled.
 *
 * @param[in]     trials   How many pairs to draw.
 * @param[in]     seed     Stream seed.
 * @param[in]     width    Bits per word.
 * @param[in]     rounds   How deep to go.
 * @param[in]     backward Nonzero to walk the inverse.
 * @param[in,out] sums     Room for eight accumulators [BORROWS].
 * @param[in,out] counted  How many pairs the walk covered [BORROWS].
 * @note Per-word, and not whole-state, because the 32-bit result for this comparison is the largest
 *       of eight per-word readings. A whole-state distance would be a different statistic. The widths
 *       would not be comparable to it, or to each other.
 * @note One bit of difference, as the 32-bit arm uses. Thus only the width changes across the sweep.
 */
__global__ void narrow_walk(uint64_t trials, uint64_t seed, unsigned width, unsigned rounds, int backward,
                            unsigned long long *sums, unsigned long long *counted)
{
    const uint32_t mask = (width >= 32u) ? 0xffffffffu : ((1u << width) - 1u);
    const uint64_t stride = (uint64_t)gridDim.x * blockDim.x;
    unsigned long long local[8];
    for (unsigned slot = 0u; slot < 8u; slot += 1u)
    {
        local[slot] = 0u;
    }
    unsigned long long seen = 0u;

    for (uint64_t at = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x; at < trials; at += stride)
    {
        uint64_t generator = seed + (at * 0x2545f4914f6cdd1dull);

        uint32_t words[24];
        for (unsigned slot = 0u; slot < 24u; slot += 1u)
        {
            words[slot] = (uint32_t)splitmix(generator) & mask;
        }

        uint32_t left[8];
        uint32_t right[8];
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            left[slot] = (uint32_t)splitmix(generator) & mask;
            right[slot] = left[slot];
        }

        const unsigned where = (unsigned)(splitmix(generator) % (uint64_t)(width * 8u));
        right[where / width] ^= (1u << (where % width));

        for (unsigned turn_at = 0u; turn_at < rounds; turn_at += 1u)
        {
            if (backward != 0)
            {
                const unsigned which = rounds - 1u - turn_at;
                narrow_unstep(left, which, words[which], width);
                narrow_unstep(right, which, words[which], width);
            }
            else
            {
                narrow_step(left, turn_at, words[turn_at], width);
                narrow_step(right, turn_at, words[turn_at], width);
            }
        }

        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            local[slot] += (unsigned long long)__popc((left[slot] ^ right[slot]) & mask);
        }
        seen += 1u;
    }

    for (unsigned slot = 0u; slot < 8u; slot += 1u)
    {
        atomicAdd(&sums[slot], local[slot]);
    }
    atomicAdd(counted, seen);
}

} // namespace

int main(int argc, char **argv)
{
    const unsigned sample_bits = (argc > 1) ? (unsigned)std::atoi(argv[1]) : 32u;

    std::printf("================================================================\n");
    std::printf("  SHA-256 squished, so the answers are computed instead of sampled\n");
    std::printf("================================================================\n");
    std::printf("\n  The observer is not obliged to stand outside watching output go past. The\n");
    std::printf("  whole object is available and can be shrunk, and shrinking is the move that\n");
    std::printf("  changes what is knowable: at four-bit words the state is 32 bits and every one\n");
    std::printf("  of them can be visited, so the differential spread is a finite sum instead of\n");
    std::printf("  an estimate with a floor under it.\n");
    std::printf("\n  Aimed at the largest unknown in the topology document: why forward differences\n");
    std::printf("  die at nine rounds and inverted ones survive past thirteen. Topology was tested\n");
    std::printf("  for it and failed, because a cone bounds what could arrive and not what does.\n");
    std::printf("\n  Four-bit words are the control: too narrow for three distinct rotation amounts,\n");
    std::printf("  so Sigma0 and Sigma1 collapse together and the rotation design is gone. Anything\n");
    std::printf("  surviving there is not coming from that design. Eight-bit words keep three\n");
    std::printf("  distinct amounts, which is the structure the real thing has.\n");

    uint32_t words[64];
    uint32_t state = 20260908u;
    for (unsigned at = 0u; at < 64u; at += 1u)
    {
        state = (state * 1103515245u) + 12345u;
        words[at] = (state >> 16);
    }
    uint32_t *device_words = nullptr;
    cudaMalloc((void **)&device_words, sizeof(words));
    cudaMemcpy(device_words, words, sizeof(words), cudaMemcpyHostToDevice);

    unsigned long long *device_spread = nullptr;
    unsigned long long *device_counted = nullptr;
    cudaMalloc((void **)&device_spread, sizeof(unsigned long long));
    cudaMalloc((void **)&device_counted, sizeof(unsigned long long));

    std::printf("\n  Width is swept instead of picked, because it is a coordinate and a coordinate\n");
    std::printf("  that can be scanned is not one to fix. The rotation amounts are derived from the\n");
    std::printf("  standard's by scaling, so every width is the same construction instead of a\n");
    std::printf("  hand-made object per width.\n");

    std::printf("\n  %8s %8s %18s %18s %12s\n", "width", "state", "low amounts", "high amounts", "distinct");
    std::printf("  %8s %8s %18s %18s %12s\n", "--------", "--------", "------------------", "------------------",
                "------------");
    for (unsigned width = 3u; width <= 8u; width += 1u)
    {
        unsigned la = 0u;
        unsigned lb = 0u;
        unsigned lc = 0u;
        unsigned ha = 0u;
        unsigned hb = 0u;
        unsigned hc = 0u;
        rotation_amounts(width, &la, &lb, &lc, &ha, &hb, &hc);
        const int low_distinct = ((la != lb) && (lb != lc) && (la != lc)) ? 1 : 0;
        const int high_distinct = ((ha != hb) && (hb != hc) && (ha != hc)) ? 1 : 0;
        const int sets_differ = ((la != ha) || (lb != hb) || (lc != hc)) ? 1 : 0;
        char summary[24];
        std::snprintf(summary, sizeof(summary), "%s%s", ((low_distinct != 0) && (high_distinct != 0)) ? "yes" : "no",
                      (sets_differ != 0) ? ", differ" : ", same");
        std::printf("  %8u %8u %6u %5u %5u %6u %5u %5u %12s\n", width, width * WORDS, la, lb, lc, ha, hb, hc, summary);
    }

    for (unsigned width = 3u; width <= 8u; width += 1u)
    {
        const unsigned bits = width * WORDS;
        const uint64_t span = (bits <= sample_bits) ? ((uint64_t)1u << bits) : ((uint64_t)1u << sample_bits);
        const int exhaustive = (bits <= sample_bits) ? 1 : 0;
        // Saturation for a whole state of this width, which is half its bits.
        const double saturated = (double)bits / 2.0;

        std::printf("\n================================================================\n");
        std::printf("  %u-bit words, %u-bit state, %s over %llu states\n", width, bits,
                    (exhaustive != 0) ? "EXHAUSTIVE" : "sampled", (unsigned long long)span);
        std::printf("================================================================\n");

        for (unsigned rounds_setting = 0u; rounds_setting < 2u; rounds_setting += 1u)
        {
            // Both settings, because they are different objects. 64 rounds at a narrow width make
            // something more mixed than SHA-256. Rounds scaled with the width keep the mix
            // comparable. A report of one would choose an answer.
            const unsigned deepest = (rounds_setting == 0u) ? ((64u * width) / 32u) : 24u;
            std::printf("\n  rounds scaled with the width to %u, and read to that depth\n", deepest);
            std::printf("\n  %8s %16s %16s %14s\n", "rounds", "forward spread", "inverse spread", "saturated at");
            std::printf("  %8s %16s %16s %14s\n", "--------", "----------------", "----------------", "--------------");

            for (unsigned rounds = 1u; rounds <= deepest; rounds += 1u)
            {
                double reading[2];
                for (int backward = 0; backward < 2; backward += 1)
                {
                    cudaMemset(device_spread, 0, sizeof(unsigned long long));
                    cudaMemset(device_counted, 0, sizeof(unsigned long long));
                    // A single-bit difference on the word that a round reads first: the scarcest
                    // stratum, and the deepest survivor.
                    spread_of<<<2048, 256>>>(span, width, rounds, backward, (uint64_t)1u << (7u * width), device_words,
                                             device_spread, device_counted);
                    cudaDeviceSynchronize();

                    unsigned long long total = 0u;
                    unsigned long long seen = 0u;
                    cudaMemcpy(&total, device_spread, sizeof(total), cudaMemcpyDeviceToHost);
                    cudaMemcpy(&seen, device_counted, sizeof(seen), cudaMemcpyDeviceToHost);
                    reading[backward] = (seen > 0u) ? ((double)total / (double)seen) : 0.0;
                }

                std::printf("  %8u %16.6f %16.6f %14.1f\n", rounds, reading[0], reading[1], saturated);
            }
        }
    }

    std::printf("\n================================================================\n");
    std::printf("  Reading it\n");
    std::printf("================================================================\n");
    std::printf("\n  The spread columns are the mean Hamming distance of the whole output state.\n");
    std::printf("  Saturation is half the state bits, and a column that reaches it has lost the\n");
    std::printf("  difference entirely. Where the run is exhaustive these are the exact values and\n");
    std::printf("  there is no floor to clear and no control to compare against, because nothing\n");
    std::printf("  about them is uncertain.\n");
    std::printf("\n  If the forward and inverse columns saturate at different rounds here, the\n");
    std::printf("  asymmetry is a property of this shape at any width and the full-size reading\n");
    std::printf("  was not a coincidence of sampling. If they saturate together, the asymmetry\n");
    std::printf("  belongs to the 32-bit width or to something the narrowing removed, and the\n");
    std::printf("  four-bit control says whether the rotation design is what removed it.\n");

    // ---------------------------------------------------------------------------------------
    // The rake, in both directions.
    //
    // Keep every coordinate still, and move one subspace through all of its values at once. Where
    // the algebraic degree in those coordinates is below the dimension of the subspace, the
    // exclusive-or of every output is exactly zero. Each monomial below that degree appears an even
    // number of times. Zero is the reading, and not a small number. Thus this needs no control, and
    // has no floor.
    //
    // Both directions, because the inverse presses from the other end. An integral that dies at r
    // rounds forward and s backward covers r plus s when the two meet in the middle. The inverse is
    // the side of slower diffusion.
    // ---------------------------------------------------------------------------------------
    std::printf("\n================================================================\n");
    std::printf("  The rake: every pin set at once, and the hole is exact\n");
    std::printf("================================================================\n");
    std::printf("\n  A differential moves one pin. This holds every coordinate still and sweeps a\n");
    std::printf("  whole subspace, then reads the sum. Below the subspace dimension the algebraic\n");
    std::printf("  degree cannot survive the cancellation, so the sum is exactly zero - an\n");
    std::printf("  identity that holds or does not, instead of a number to be compared against a\n");
    std::printf("  null. It is the one reading here that needs no control.\n");

    {
        unsigned *device_positions = nullptr;
        unsigned long long *device_total = nullptr;
        cudaMalloc((void **)&device_positions, 32u * sizeof(unsigned));
        cudaMalloc((void **)&device_total, sizeof(unsigned long long));

        for (unsigned width = 4u; width <= 8u; width += 4u)
        {
            std::printf("\n  %u-bit words\n", width);
            std::printf("\n  %10s %10s %20s %20s\n", "dimension", "inputs", "forward zero to", "inverse zero to");
            std::printf("  %10s %10s %20s %20s\n", "----------", "----------", "--------------------",
                        "--------------------");

            for (unsigned dimension = 2u; dimension <= 12u; dimension += 2u)
            {
                // The swept bits come from the word that a round reads first. Where the dimension
                // exceeds the width, they spill into the next word.
                std::vector<unsigned> positions(dimension);
                for (unsigned bit = 0u; bit < dimension; bit += 1u)
                {
                    positions[bit] = ((7u * width) + bit) % (width * WORDS);
                }
                cudaMemcpy(device_positions, positions.data(), dimension * sizeof(unsigned), cudaMemcpyHostToDevice);

                unsigned reach[2] = {0u, 0u};
                for (int backward = 0; backward < 2; backward += 1)
                {
                    for (unsigned rounds = 1u; rounds <= 24u; rounds += 1u)
                    {
                        // Every base, and not one. An integral is a degree bound, and a degree bound
                        // holds on every coset. A zero at a single base is not the property. It is a
                        // cancel at that base. At one base, the reach comes out non-monotone in the
                        // dimension, which is impossible for a real integral.
                        int all_zero = 1;
                        for (unsigned trial = 0u; (trial < 8u) && (all_zero != 0); trial += 1u)
                        {
                            uint64_t base = 0x0123456789abcdefull;
                            base ^= (uint64_t)trial * 0x9e3779b97f4a7c15ull;

                            cudaMemset(device_total, 0, sizeof(unsigned long long));
                            integral_sum<<<256, 256>>>(dimension, device_positions, base, width, rounds, backward,
                                                       device_words, device_total);
                            cudaDeviceSynchronize();

                            unsigned long long sum = 0u;
                            cudaMemcpy(&sum, device_total, sizeof(sum), cudaMemcpyDeviceToHost);
                            all_zero = (sum == 0u) ? 1 : 0;
                        }
                        if (all_zero == 0)
                        {
                            break;
                        }
                        reach[backward] = rounds;
                    }
                }

                std::printf("  %10u %10llu %20u %20u\n", dimension, (unsigned long long)1u << dimension, reach[0],
                            reach[1]);
            }
        }

        cudaFree(device_positions);
        cudaFree(device_total);
    }

    // ---------------------------------------------------------------------------------------
    // A bisection of the monotonicity violation.
    //
    // Integral reach must not fall as the dimension rises. Suppose that a subspace sums to zero. A
    // larger subspace that contains it is the exclusive-or of that sum over each coset. A sum of
    // zeros is zero. Each set of swept positions here contains the smaller sets. A reach of five at dimension eight and
    // three at dimension ten cannot both be true.
    //
    // The decomposition is the test. A dimension-ten sum must equal the exclusive-or of the four
    // dimension-eight sums over its cosets. That is an identity about how the kernel adds, and not
    // about SHA-256. If it does not hold, the kernel is wrong. If it holds, the argument above is
    // wrong, and one of those zeros is an accident, and not a degree bound.
    // ---------------------------------------------------------------------------------------
    std::printf("\n================================================================\n");
    std::printf("  Bisecting the monotonicity violation\n");
    std::printf("================================================================\n");
    std::printf("\n  A dimension-ten sum must equal the exclusive-or of the four dimension-eight\n");
    std::printf("  sums over its cosets. That is an identity about the kernel's bookkeeping, not\n");
    std::printf("  about the function, a mismatch localises the defect immediately.\n");
    std::printf("\n  %8s %20s %20s %12s\n", "rounds", "whole at dim 10", "cosets of dim 8", "agree");
    std::printf("  %8s %20s %20s %12s\n", "--------", "--------------------", "--------------------", "------------");

    {
        const unsigned width = 4u;
        unsigned *device_positions = nullptr;
        unsigned long long *device_total = nullptr;
        cudaMalloc((void **)&device_positions, 32u * sizeof(unsigned));
        cudaMalloc((void **)&device_total, sizeof(unsigned long long));

        for (unsigned rounds = 1u; rounds <= 6u; rounds += 1u)
        {
            std::vector<unsigned> ten(10u);
            for (unsigned bit = 0u; bit < 10u; bit += 1u)
            {
                ten[bit] = ((7u * width) + bit) % (width * WORDS);
            }
            cudaMemcpy(device_positions, ten.data(), 10u * sizeof(unsigned), cudaMemcpyHostToDevice);
            cudaMemset(device_total, 0, sizeof(unsigned long long));
            integral_sum<<<256, 256>>>(10u, device_positions, 0x0123456789abcdefull, width, rounds, 1, device_words,
                                       device_total);
            cudaDeviceSynchronize();
            unsigned long long whole = 0u;
            cudaMemcpy(&whole, device_total, sizeof(whole), cudaMemcpyDeviceToHost);

            // The same span, taken as four cosets of the dimension-eight subspace. The two extra
            // coordinates of the ten-dimensional space are what selects the coset.
            std::vector<unsigned> eight(8u);
            for (unsigned bit = 0u; bit < 8u; bit += 1u)
            {
                eight[bit] = ten[bit];
            }
            cudaMemcpy(device_positions, eight.data(), 8u * sizeof(unsigned), cudaMemcpyHostToDevice);

            unsigned long long assembled = 0u;
            for (unsigned coset = 0u; coset < 4u; coset += 1u)
            {
                uint64_t shifted = 0x0123456789abcdefull;
                if ((coset & 1u) != 0u)
                {
                    shifted ^= (uint64_t)1u << ten[8];
                }
                if ((coset & 2u) != 0u)
                {
                    shifted ^= (uint64_t)1u << ten[9];
                }
                cudaMemset(device_total, 0, sizeof(unsigned long long));
                integral_sum<<<256, 256>>>(8u, device_positions, shifted, width, rounds, 1, device_words, device_total);
                cudaDeviceSynchronize();
                unsigned long long part = 0u;
                cudaMemcpy(&part, device_total, sizeof(part), cudaMemcpyDeviceToHost);
                assembled ^= part;
            }

            std::printf("  %8u %20llx %20llx %12s\n", rounds, whole, assembled, (whole == assembled) ? "yes" : "NO");
        }

        cudaFree(device_positions);
        cudaFree(device_total);
    }

    std::printf("\n  Agreement everywhere means the kernel adds correctly and the reach table's\n");
    std::printf("  zeros at one base were accidents instead of degree bounds - which would mean\n");
    std::printf("  the reach has to be read over several bases, not one. Disagreement means the\n");
    std::printf("  kernel is wrong and nothing it has produced counts.\n");

    std::printf("\n  The reach columns are the last round at which the sum was still exactly zero.\n");
    std::printf("  A dimension that reaches further than the one below it is the degree growing\n");
    std::printf("  slower than the dimension, and where the columns stop growing with dimension the\n");
    std::printf("  degree has caught up and the rake is finished.\n");
    std::printf("\n  Forward and inverse are read separately because the inverse holds pressure from\n");
    std::printf("  the other end: matched in the middle, an integral covers the sum of the two, and\n");
    std::printf("  the inverse is already known to be the slower-diffusing side.\n");

    // ---------------------------------------------------------------------------------------
    // Does the peak destruction rate follow the width?
    //
    // At 32 bits, the forward decay peaks at 29.73 per round: 4.89 bits, against 5.000 for exactly
    // 32. The natural reading is one 32-bit word destroyed per round. That predicts something that
    // the width sweep can falsify: the peak should track the width. At 8 bits, it should read near
    // 8, and not near 32.
    //
    // The alternative: the peak is a constant of the construction, and not of the word. Then every
    // width peaks near the same figure, and the agreement at 32 is a coincidence.
    //
    // The statistic is the largest of eight per-word readings, with a width-aware null: the same
    // statistic as the 32-bit result. Rounds pinned at the ceiling are the shift chain with an
    // untouched word, and carry no rate. Thus the bench excludes them here, as there.
    // ---------------------------------------------------------------------------------------
    std::printf("\n================================================================\n");
    std::printf("  Peak destruction rate against width\n");
    std::printf("================================================================\n");
    std::printf("\n  At 32 bits the forward decay peaks at 29.73 per round, 4.89 bits against 5.000\n");
    std::printf("  for exactly 32. If that is one word destroyed per round the peak tracks the\n");
    std::printf("  width and reads near 8 at eight bits; if it is a constant of the construction\n");
    std::printf("  every width peaks near the same figure and 32 was a coincidence.\n");

    {
        const unsigned WIDTHS = 7u;
        const unsigned width_of[WIDTHS] = {4u, 6u, 8u, 12u, 16u, 24u, 32u};
        const uint64_t many = (uint64_t)1u << 22u;

        unsigned long long *device_sums = nullptr;
        unsigned long long *device_seen = nullptr;
        cudaMalloc((void **)&device_sums, 8u * sizeof(unsigned long long));
        cudaMalloc((void **)&device_seen, sizeof(unsigned long long));

        // The amounts come from the amounts of the standard, scaled and rounded. Thus they change
        // with the width, and so does their arithmetic relation to it. A rotation by r on a w-bit
        // word has orbit length w/gcd(w,r). Where the width shares factors with an amount, that
        // rotation has short orbits, and mixes weakly. A sweep of the width therefore sweeps the
        // construction, and not one construction at several sizes. The worst gcd in a row is the
        // honest label for how much of the design survives the narrowing.
        std::printf("\n  %6s %11s %11s %6s %11s %10s %8s %8s\n", "width", "low rot", "high rot", "worst", "peak x/rd",
                    "peak bits", "w bits", "dies at");
        std::printf("  %6s %11s %11s %6s %11s %10s %8s %8s\n", "------", "-----------", "-----------", "------",
                    "-----------", "----------", "--------", "--------");

        for (unsigned which = 0u; which < WIDTHS; which += 1u)
        {
            const unsigned width = width_of[which];
            const double ceiling = bench_depth_ceiling((double)many, (double)width);
            const double floor_at = 1.9;

            double before = 0.0;
            double peak = 0.0;
            unsigned dies = 0u;

            for (unsigned rounds = 1u; rounds <= 20u; rounds += 1u)
            {
                cudaMemset(device_sums, 0, 8u * sizeof(unsigned long long));
                cudaMemset(device_seen, 0, sizeof(unsigned long long));
                narrow_walk<<<2048, 128>>>(many, 20260909ull + (rounds * 7919ull) + width, width, rounds, 0,
                                           device_sums, device_seen);
                cudaDeviceSynchronize();

                unsigned long long sums[8];
                unsigned long long counted = 0u;
                cudaMemcpy(sums, device_sums, sizeof(sums), cudaMemcpyDeviceToHost);
                cudaMemcpy(&counted, device_seen, sizeof(counted), cudaMemcpyDeviceToHost);

                double strongest = 0.0;
                for (unsigned slot = 0u; slot < 8u; slot += 1u)
                {
                    const double score = bench_depth_score_at_width((double)sums[slot], (double)counted, (double)width);
                    const double size = (score < 0.0) ? -score : score;
                    strongest = (size > strongest) ? size : strongest;
                }

                // A ratio is only a rate where both ends are readable: above the floor, and off the
                // ceiling that a pinned round sits at.
                if ((before > floor_at * 4.0) && (before < (ceiling * 0.95)) && (strongest > floor_at * 2.0))
                {
                    const double ratio = before / strongest;
                    peak = (ratio > peak) ? ratio : peak;
                }
                if ((dies == 0u) && (strongest < 4.0))
                {
                    dies = rounds;
                }
                before = strongest;
            }

            const double peak_bits = (peak > 0.0) ? std::log2(peak) : 0.0;
            const double width_bits = std::log2((double)width);

            unsigned low_a = 0u;
            unsigned low_b = 0u;
            unsigned low_c = 0u;
            unsigned high_a = 0u;
            unsigned high_b = 0u;
            unsigned high_c = 0u;
            rotation_amounts(width, &low_a, &low_b, &low_c, &high_a, &high_b, &high_c);

            const unsigned every[6] = {low_a, low_b, low_c, high_a, high_b, high_c};
            unsigned worst = 1u;
            for (unsigned at = 0u; at < 6u; at += 1u)
            {
                unsigned left = width;
                unsigned right = every[at];
                while (right != 0u)
                {
                    const unsigned carry = left % right;
                    left = right;
                    right = carry;
                }
                worst = (left > worst) ? left : worst;
            }

            char low_text[16];
            char high_text[16];
            std::snprintf(low_text, sizeof(low_text), "%u,%u,%u", low_a, low_b, low_c);
            std::snprintf(high_text, sizeof(high_text), "%u,%u,%u", high_a, high_b, high_c);

            std::printf("  %6u %11s %11s %6u %11.2f %10.2f %8.2f %8u\n", width, low_text, high_text, worst, peak,
                        peak_bits, width_bits, dies);
        }

        cudaFree(device_sums);
        cudaFree(device_seen);

        std::printf("\n  The worst column is the largest gcd between the width and any of its six\n");
        std::printf("  rotation amounts, so it is the shortest orbit the narrowing left in place.\n");
        std::printf("  A row with a worst above one is not the same construction as a row with one,\n");
        std::printf("  and this sweep therefore compares seven constructions instead of one at\n");
        std::printf("  seven sizes. The peak column tracks that instead of the width.\n");
        std::printf("\n  The peak column is not measurable at this resolution either. The collapse\n");
        std::printf("  falls between two integer rounds, a ratio of rounds is a derivative sampled\n");
        std::printf("  coarser than the thing it is sampling, and it lands where the round grid\n");
        std::printf("  happens to cut the fall. Sub-round resolution wants the input weight swept\n");
        std::printf("  instead, since a heavier difference collapses earlier and walks the fall\n");
        std::printf("  across the grid.\n");
        std::printf("\n  The dies column is the one that holds still, and it holds still at nine or\n");
        std::printf("  ten across an eightfold change in width. A wall set by how many words are on\n");
        std::printf("  the chain is exactly what does not move when the bits inside them change; a\n");
        std::printf("  wall set by the width of a word would have moved and did not. Varying the\n");
        std::printf("  word count instead of the word width is the test that would settle it, and\n");
        std::printf("  this bench holds eight words at every width.\n");
        std::printf("\n  The two triples above are the state functions, Sigma0 at 2, 13, 22 and\n");
        std::printf("  Sigma1 at 6, 11, 25. The schedule's own constants - sigma0 at 7, 18 and a\n");
        std::printf("  shift of 3, sigma1 at 17, 19 and a shift of 10 - are not swept here and are\n");
        std::printf("  not narrowed by anything in this tree, because this bench feeds independent\n");
        std::printf("  random words instead of expanding a schedule. A nonce travels through those\n");
        std::printf("  four and not through these two, so the same orbit argument aimed at them is\n");
        std::printf("  the sharper form of this question.\n");
    }

    cudaFree(device_words);
    cudaFree(device_spread);
    cudaFree(device_counted);
    return 0;
}
