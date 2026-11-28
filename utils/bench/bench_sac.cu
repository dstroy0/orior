// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_sac.cu
 * @brief The strict avalanche criterion per round, at a sample deep enough to settle a thin margin.
 *
 * @note Vaughn and Borowczak (Cryptography 2026, 10(3), 32) report that SHA-256 satisfies the strict avalanche
 *       criterion from round 23 through 53, *fails it from 54 through 57*, and satisfies it again from 58. They call
 *       the late failure unexpected, and give a damping of diffusion between sub-functions as its cause. A published,
 *       measured non-monotonicity in the diffusion of a function this studied deserves a reproduction before anyone
 *       believes it.
 * @note Their margin is thin. The reported SAC value across rounds 54 to 57 is 0.49728, against a Bonferroni threshold
 *       of 0.49734: six parts in a hundred thousand. Their sample is k = 1,000,000 trials, where one standard deviation
 *       is 0.0005. The departure of 0.49728 from one half is 5.44 standard deviations at their sample, and would be 54
 *       at a hundred times the trials. A harder sample can therefore answer the question, and a device exists for that.
 * @note The statistic is theirs, and not a near relative of it. It is a 512 by 256 matrix that counts how often output
 *       bit j flips when input bit i of the message block flips. The SAC value is whichever entry sits furthest from
 *       one half. Every other bench in this tree reads per-word Hamming means instead. That is a different question,
 *       and would not reproduce their number whatever SHA-256 does.
 * @note The feedforward is irrelevant here, and the bench does not compute it. Both members of a pair add the same
 *       chaining value at the end, and it cancels in the exclusive-or exactly.
 */

#include "../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include <cuda_runtime.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

namespace
{

/** @brief Bits in the message block, the input side of the SAC matrix. */
const unsigned INPUT_BITS = 512u;

/** @brief Bits in the chaining value, the output side. */
const unsigned OUTPUT_BITS = 256u;

__constant__ uint32_t d_constant[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

__constant__ uint32_t d_start[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

// The same table on the host. The miner arm runs its comparison in ordinary C++, because it is a few thousand hashes
// and not a few billion. Host code cannot reach __constant__ memory.
static const uint32_t host_constant[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

__device__ __forceinline__ uint32_t turn(uint32_t value, uint32_t by)
{
    return __funnelshift_r(value, value, by);
}

__device__ __forceinline__ uint64_t splitmix(uint64_t &state)
{
    state += 0x9e3779b97f4a7c15ull;
    uint64_t mixed = state;
    mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ull;
    mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebull;
    return mixed ^ (mixed >> 31);
}

/**
 * @brief Runs the compression rounds and leaves the state, without the feedforward.
 *
 * @param[in]  message Sixteen message words [BORROWS].
 * @param[in]  rounds  How many rounds to run.
 * @param[out] state   Eight state words [BORROWS].
 * @note No feedforward, because both members of a SAC pair add the same chaining value and it
 *       cancels in the exclusive-or exactly. Computing it would change nothing and cost eight adds.
 */
/** @brief Which sub-functions a variant keeps, as the published work varies them. */
#define KEEP_SCHEDULE 1u
#define KEEP_MAJORITY 2u
#define KEEP_CHOOSE 4u
#define KEEP_BIG_ZERO 8u
#define KEEP_BIG_ONE 16u
#define KEEP_ADDITION 32u
#define KEEP_CONSTANT 64u
#define KEEP_EVERYTHING 127u

__device__ __forceinline__ void compress_to(const uint32_t *message, unsigned rounds, uint32_t *state, unsigned keep)
{
    uint32_t schedule[64];
    for (unsigned slot = 0u; slot < 16u; slot += 1u)
    {
        schedule[slot] = message[slot];
    }

    // Without the scheduler, the block simply repeats. That is the substitution that the published work uses. A drop of
    // the expansion would leave nothing to feed the later rounds.
    if ((keep & KEEP_SCHEDULE) != 0u)
    {
        for (unsigned slot = 16u; slot < 64u; slot += 1u)
        {
            const uint32_t fifteen = schedule[slot - 15u];
            const uint32_t two = schedule[slot - 2u];
            const uint32_t low = turn(fifteen, 7u) ^ turn(fifteen, 18u) ^ (fifteen >> 3u);
            const uint32_t high = turn(two, 17u) ^ turn(two, 19u) ^ (two >> 10u);
            schedule[slot] = ((keep & KEEP_ADDITION) != 0u) ? (high + schedule[slot - 7u] + low + schedule[slot - 16u])
                                                            : (high ^ schedule[slot - 7u] ^ low ^ schedule[slot - 16u]);
        }
    }
    else
    {
        for (unsigned slot = 16u; slot < 64u; slot += 1u)
        {
            schedule[slot] = schedule[slot - 16u];
        }
    }

    uint32_t a = d_start[0], b = d_start[1], c = d_start[2], d = d_start[3];
    uint32_t e = d_start[4], f = d_start[5], g = d_start[6], h = d_start[7];

    for (unsigned round = 0u; round < rounds; round += 1u)
    {
        // Integer addition carries and exclusive-or does not. That carry is the only difference between them here.
        // Every other sub-function is simply present or absent.
        const uint32_t big_one = ((keep & KEEP_BIG_ONE) != 0u) ? (turn(e, 6u) ^ turn(e, 11u) ^ turn(e, 25u)) : 0u;
        const uint32_t choose = ((keep & KEEP_CHOOSE) != 0u) ? (g ^ (e & (f ^ g))) : 0u;
        const uint32_t big_zero = ((keep & KEEP_BIG_ZERO) != 0u) ? (turn(a, 2u) ^ turn(a, 13u) ^ turn(a, 22u)) : 0u;
        const uint32_t many = ((keep & KEEP_MAJORITY) != 0u) ? ((a & b) | (c & (a ^ b))) : 0u;
        const uint32_t rounder = ((keep & KEEP_CONSTANT) != 0u) ? d_constant[round] : 0u;

        uint32_t first;
        uint32_t next_e;
        uint32_t next_a;
        if ((keep & KEEP_ADDITION) != 0u)
        {
            first = h + big_one + choose + rounder + schedule[round];
            next_e = d + first;
            next_a = first + big_zero + many;
        }
        else
        {
            first = h ^ big_one ^ choose ^ rounder ^ schedule[round];
            next_e = d ^ first;
            next_a = first ^ big_zero ^ many;
        }

        h = g;
        g = f;
        f = e;
        e = next_e;
        d = c;
        c = b;
        b = a;
        a = next_a;
    }

    state[0] = a;
    state[1] = b;
    state[2] = c;
    state[3] = d;
    state[4] = e;
    state[5] = f;
    state[6] = g;
    state[7] = h;
}

/**
 * @brief Fills one row of the SAC matrix: how often each output bit flips for one input bit.
 *
 * @param[in]     trials How many plaintexts to draw.
 * @param[in]     seed   Stream seed.
 * @param[in]     rounds How many rounds to run.
 * @param[in,out] matrix 512 by 256 counters [BORROWS].
 * @note One block per input bit. The working set of the block is then the 256 counters of its own row, and they live in
 *       shared memory. A thread per (trial, bit) pair over the whole matrix would need half a megabyte of counters, and
 *       would make every increment a global atomic.
 */
__global__ void fill_sac_row(uint64_t trials, uint64_t seed, unsigned rounds, unsigned long long *matrix, unsigned keep)
{
    __shared__ unsigned long long tally[OUTPUT_BITS];
    for (unsigned at = threadIdx.x; at < OUTPUT_BITS; at += blockDim.x)
    {
        tally[at] = 0u;
    }
    __syncthreads();

    const unsigned input_bit = blockIdx.x;

    for (uint64_t trial = threadIdx.x; trial < trials; trial += blockDim.x)
    {
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull);

        uint32_t message[16];
        uint32_t flipped[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            message[slot] = (uint32_t)splitmix(generator);
            flipped[slot] = message[slot];
        }
        flipped[input_bit / 32u] ^= (1u << (input_bit % 32u));

        uint32_t plain[8];
        uint32_t other[8];
        compress_to(message, rounds, plain, keep);
        compress_to(flipped, rounds, other, keep);

        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            uint32_t apart = plain[slot] ^ other[slot];
            while (apart != 0u)
            {
                const unsigned bit = (unsigned)__ffs((int)apart) - 1u;
                atomicAdd(&tally[(slot * 32u) + bit], 1ull);
                apart &= apart - 1u;
            }
        }
    }
    __syncthreads();

    unsigned long long *const row = &matrix[(size_t)input_bit * OUTPUT_BITS];
    for (unsigned at = threadIdx.x; at < OUTPUT_BITS; at += blockDim.x)
    {
        atomicAdd(&row[at], tally[at]);
    }
}

/**
 * @brief Fills the matrix from randomness instead of from SHA-256, as the chi-square's own null.
 *
 * @param[in]     trials How many draws, matching the real arm.
 * @param[in]     seed   Stream seed.
 * @param[in,out] matrix 512 by 256 counters [BORROWS].
 * @note Same loop, same shared tally, same atomics and the same denominator as fill_sac_row, with eight random words in
 *       place of the compression. Every cell is then binomial by construction. This therefore reads the sum of squared
 *       z that the instrument produces when the answer is zero by construction. A deficit here is the sampler's, and
 *       not the function's.
 */
__global__ void fill_null_row(uint64_t trials, uint64_t seed, unsigned long long *matrix, unsigned shape)
{
    __shared__ unsigned long long tally[OUTPUT_BITS];
    for (unsigned at = threadIdx.x; at < OUTPUT_BITS; at += blockDim.x)
    {
        tally[at] = 0u;
    }
    __syncthreads();

    const unsigned input_bit = blockIdx.x;

    for (uint64_t trial = threadIdx.x; trial < trials; trial += blockDim.x)
    {
        // Not 0x9e3779b97f4a7c15 for the row offset: that is splitmix's own increment. With it, position j+1 of row b
        // would be the identical draw to position j of row b+1. Adjacent rows would then share seven words of eight,
        // and the measured spread of this control would rise to 6.93 times the analytic variance. The control would
        // then be useless as a null.
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull) + ((uint64_t)input_bit * 0xd1b54a32d192ed03ull);

        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            // Shape 1 gives every word its own stream, and does not take eight in a row from one. If the sequential
            // draw causes the chi-square deficit of the harness, the deficit goes away here. If it survives both
            // shapes, the access pattern is not the cause.
            uint64_t apart_from = generator;
            if (shape == 1u)
            {
                apart_from = seed + (trial * 0x2545f4914f6cdd1dull) + ((uint64_t)input_bit * 0xd1b54a32d192ed03ull) +
                             ((uint64_t)slot * 0xa24baed4963ee407ull);
            }
            uint32_t apart = (uint32_t)splitmix(apart_from);
            if (shape == 0u)
            {
                generator = apart_from;
            }
            while (apart != 0u)
            {
                const unsigned bit = (unsigned)__ffs((int)apart) - 1u;
                atomicAdd(&tally[(slot * 32u) + bit], 1ull);
                apart &= apart - 1u;
            }
        }
    }
    __syncthreads();

    unsigned long long *const row = &matrix[(size_t)input_bit * OUTPUT_BITS];
    for (unsigned at = threadIdx.x; at < OUTPUT_BITS; at += blockDim.x)
    {
        atomicAdd(&row[at], tally[at]);
    }
}

/**
 * @brief Runs the rounds with Sigma1's three rotation amounts supplied instead of fixed.
 *
 * @param[in]  message Sixteen message words [BORROWS].
 * @param[in]  rounds  How many rounds to run.
 * @param[in]  turns   The three amounts to use in place of 6, 11 and 25 [BORROWS].
 * @param[out] state   Eight words [BORROWS].
 * @note Every certain cell that is not on the diagonal sits at residue 25, and none at 6 or 11, at every depth
 *       measured. Two readings of that are available, and they differ in what they predict of a function that nobody
 *       built. One is that the largest amount always wins. The other is that 25 wins because it is 25. A change of the
 *       amounts separates them.
 * @note The rest of the function stays as it is. Only Sigma1 takes parameters, and a difference in the result therefore
 *       belongs to the rotation amounts alone.
 */
__global__ void fill_rotated_row(uint64_t trials, uint64_t seed, unsigned rounds, unsigned turn_a, unsigned turn_b,
                                 unsigned turn_c, unsigned long long *matrix)
{
    const unsigned input_bit = blockIdx.x;

    for (uint64_t trial = threadIdx.x; trial < trials; trial += blockDim.x)
    {
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull);

        uint32_t message[16];
        uint32_t flipped[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            message[slot] = (uint32_t)splitmix(generator);
            flipped[slot] = message[slot];
        }
        flipped[input_bit / 32u] ^= (1u << (input_bit % 32u));

        uint32_t plain[8];
        uint32_t other[8];

        for (unsigned side = 0u; side < 2u; side += 1u)
        {
            const uint32_t *source = (side == 0u) ? message : flipped;
            uint32_t schedule[64];
            for (unsigned slot = 0u; slot < 16u; slot += 1u)
            {
                schedule[slot] = source[slot];
            }
            for (unsigned slot = 16u; slot < 64u; slot += 1u)
            {
                const uint32_t two = schedule[slot - 2u];
                const uint32_t fifteen = schedule[slot - 15u];
                const uint32_t low = turn(fifteen, 7u) ^ turn(fifteen, 18u) ^ (fifteen >> 3u);
                const uint32_t high = turn(two, 17u) ^ turn(two, 19u) ^ (two >> 10u);
                schedule[slot] = schedule[slot - 16u] + low + schedule[slot - 7u] + high;
            }

            uint32_t a = d_start[0], b = d_start[1], c = d_start[2], d = d_start[3];
            uint32_t e = d_start[4], f = d_start[5], g = d_start[6], h = d_start[7];

            for (unsigned round = 0u; round < rounds; round += 1u)
            {
                const uint32_t big_one = turn(e, turn_a) ^ turn(e, turn_b) ^ turn(e, turn_c);
                const uint32_t choose = g ^ (e & (f ^ g));
                const uint32_t big_zero = turn(a, 2u) ^ turn(a, 13u) ^ turn(a, 22u);
                const uint32_t many = (a & b) | (c & (a ^ b));
                const uint32_t one = h + big_one + choose + d_constant[round] + schedule[round];
                const uint32_t two = big_zero + many;
                h = g;
                g = f;
                f = e;
                e = d + one;
                d = c;
                c = b;
                b = a;
                a = one + two;
            }

            uint32_t *const into = (side == 0u) ? plain : other;
            into[0] = a;
            into[1] = b;
            into[2] = c;
            into[3] = d;
            into[4] = e;
            into[5] = f;
            into[6] = g;
            into[7] = h;
        }

        unsigned long long *const row = &matrix[(size_t)input_bit * OUTPUT_BITS];
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            uint32_t apart = plain[slot] ^ other[slot];
            while (apart != 0u)
            {
                const unsigned bit = (unsigned)__ffs((int)apart) - 1u;
                atomicAdd(&row[(slot * 32u) + bit], 1ull);
                apart &= apart - 1u;
            }
        }
    }
}

/** @brief What the coherent reference keeps: only the parts that are linear over GF(2). */
#define REFERENCE_KEEP (KEEP_SCHEDULE | KEEP_BIG_ZERO | KEEP_BIG_ONE)

/**
 * @brief Propagates one input difference through the linear round function, exactly.
 *
 * @param[in]  rounds How many rounds to run.
 * @param[in]  where  Which input bit carries the difference.
 * @param[out] out    Eight words of difference [BORROWS].
 * @note With addition, Choose and Majority removed, the map is linear over GF(2). The difference that it sends forward
 *       then depends on the input difference alone, and not on the message. That makes this a reference instead of
 *       another measurement: it has no sampling error at all.
 * @note One thread. It runs once per depth. A computation on the device avoids a second copy of the round function on
 *       the host, where the rotation intrinsic is unavailable anyway.
 */
__global__ void make_reference(unsigned rounds, unsigned where, uint32_t *out)
{
    uint32_t clear[16];
    uint32_t marked[16];
    for (unsigned slot = 0u; slot < 16u; slot += 1u)
    {
        clear[slot] = 0u;
        marked[slot] = 0u;
    }
    marked[where / 32u] ^= (1u << (where % 32u));

    uint32_t base_state[8];
    uint32_t marked_state[8];
    compress_to(clear, rounds, base_state, REFERENCE_KEEP);
    compress_to(marked, rounds, marked_state, REFERENCE_KEEP);

    for (unsigned slot = 0u; slot < 8u; slot += 1u)
    {
        out[slot] = base_state[slot] ^ marked_state[slot];
    }
}

/**
 * @brief Builds two residue folds, split by the value of one state bit at a chosen round.
 *
 * @param[in]     trials    How many messages to draw.
 * @param[in]     seed      Stream seed.
 * @param[in]     rounds    How many rounds to run for the measurement.
 * @param[in]     gate_at   Which round to read the selector at.
 * @param[in]     gate_bit  Which bit of e to use as the selector.
 * @param[in,out] matrix    Two 512 by 256 matrices laid end to end [BORROWS].
 * @note Every reading in this file averages over the message, and that makes them all blind in the same way. They see
 *       where bits go, and never what values sent them there. The nonlinearity of SHA-256 is entirely value-gated.
 *       Choose is a selector driven by e, Majority by the a chain, and carries by everything. An average over messages
 *       therefore mixes two transport regimes, and reports their mean. A structure present in both halves with opposite
 *       sign vanishes completely from that average.
 * @note The bench reads the gate from the *base* message only, and both members of a pair then land in the same half. A
 *       gate on each member separately would split pairs across the two matrices, and measure nothing.
 */
__global__ void fill_conditional_row(uint64_t trials, uint64_t seed, unsigned rounds, unsigned gate_at,
                                     unsigned gate_bit, unsigned long long *matrix)
{
    const unsigned input_bit = blockIdx.x;
    const size_t plane = (size_t)INPUT_BITS * OUTPUT_BITS;

    for (uint64_t trial = threadIdx.x; trial < trials; trial += blockDim.x)
    {
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull);

        uint32_t message[16];
        uint32_t flipped[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            message[slot] = (uint32_t)splitmix(generator);
            flipped[slot] = message[slot];
        }
        flipped[input_bit / 32u] ^= (1u << (input_bit % 32u));

        // The selector: bit gate_bit of e, the word that Choose reads, at the gate round.
        uint32_t gate_state[8];
        compress_to(message, gate_at, gate_state, KEEP_EVERYTHING);
        const unsigned which = (unsigned)((gate_state[4] >> gate_bit) & 1u);

        uint32_t plain[8];
        uint32_t other[8];
        compress_to(message, rounds, plain, KEEP_EVERYTHING);
        compress_to(flipped, rounds, other, KEEP_EVERYTHING);

        unsigned long long *const row = &matrix[(which * plane) + ((size_t)input_bit * OUTPUT_BITS)];
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            uint32_t apart = plain[slot] ^ other[slot];
            while (apart != 0u)
            {
                const unsigned bit = (unsigned)__ffs((int)apart) - 1u;
                atomicAdd(&row[(slot * 32u) + bit], 1ull);
                apart &= apart - 1u;
            }
        }
    }
}

/**
 * @brief Counts how many messages fell each side of the gate, for the normalization of the halves.
 */
__global__ void count_gate(uint64_t trials, uint64_t seed, unsigned gate_at, unsigned gate_bit,
                           unsigned long long *counts)
{
    unsigned long long mine = 0ull;
    for (uint64_t trial = ((uint64_t)blockIdx.x * blockDim.x) + threadIdx.x; trial < trials;
         trial += (uint64_t)gridDim.x * blockDim.x)
    {
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull);
        uint32_t message[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            message[slot] = (uint32_t)splitmix(generator);
        }
        uint32_t gate_state[8];
        compress_to(message, gate_at, gate_state, KEEP_EVERYTHING);
        mine += (unsigned long long)((gate_state[4] >> gate_bit) & 1u);
    }
    atomicAdd(&counts[0], mine);
}

/**
 * @brief Sweeps message differences and records the sparsest one the linear model carries to depth.
 *
 * @param[in]     rounds  How many rounds to run.
 * @param[in]     pairs   Zero to sweep single bits, one to sweep pairs of bits.
 * @param[in]     total   How many candidates the index space holds.
 * @param[in,out] best    Packed best: weight in the high half, candidate index in the low [BORROWS].
 * @note This is a search, and not a measurement. The linearized function is deterministic. The kernel evaluates every
 *       candidate exactly once, and exactly. The answer is a true minimum over the swept set, and not the smallest
 *       thing that a sample happened to find.
 * @note A swap of addition for exclusive-or is the standard first step of a differential attack on SHA-2. An attack
 *       finds a characteristic in the linear model, and then bounds its real probability against it. The schedule takes
 *       the same linearization as the state, because the expansion is additive too.
 * @note The pack keeps weight and index in one 64-bit word. A single atomicMin then orders by weight and carries the
 *       winner with it, with no second pass to find which candidate won.
 */
__global__ void sweep_linear_weight(unsigned rounds, unsigned pairs, unsigned long long total, unsigned long long *best)
{
    const unsigned long long stride = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long at = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; at < total; at += stride)
    {
        uint32_t clear[16];
        uint32_t marked[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            clear[slot] = 0u;
            marked[slot] = 0u;
        }

        if (pairs == 0u)
        {
            const unsigned where = (unsigned)at;
            marked[where / 32u] ^= (1u << (where % 32u));
        }
        else
        {
            // Unrank the index into an unordered pair without a division, by a walk of the triangle.
            unsigned long long left = at;
            unsigned first = 0u;
            while (left >= (unsigned long long)(INPUT_BITS - 1u - first))
            {
                left -= (unsigned long long)(INPUT_BITS - 1u - first);
                first += 1u;
            }
            const unsigned second = first + 1u + (unsigned)left;
            marked[first / 32u] ^= (1u << (first % 32u));
            marked[second / 32u] ^= (1u << (second % 32u));
        }

        uint32_t base_state[8];
        uint32_t marked_state[8];
        compress_to(clear, rounds, base_state, REFERENCE_KEEP);
        compress_to(marked, rounds, marked_state, REFERENCE_KEEP);

        unsigned weight = 0u;
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            weight += (unsigned)__popc(base_state[slot] ^ marked_state[slot]);
        }

        const unsigned long long packed = ((unsigned long long)weight << 40) | at;
        atomicMin(best, packed);
    }
}

/**
 * @brief Mixes the true difference against every rotation of the reference and tallies agreement.
 *
 * @param[in]     trials    How many messages to draw.
 * @param[in]     seed      Stream seed.
 * @param[in]     rounds    How many rounds to run.
 * @param[in]     where     Which input bit carries the difference.
 * @param[in]     reference Eight words from make_reference [BORROWS].
 * @param[in,out] tally     Thirty-two counters, one per rotation [BORROWS].
 * @note The tally counts agreement over the set bits of the reference only. A bit that the reference does not reach
 *       carries no phase, and its inclusion would pull every rotation toward one half.
 * @note Rotation preserves population count. All thirty-two rotations therefore share one denominator, and compare
 *       directly with no second normalization.
 */
__global__ void lock_against_reference(uint64_t trials, uint64_t seed, unsigned rounds, unsigned where,
                                       const uint32_t *reference, unsigned long long *tally)
{
    __shared__ unsigned long long agree[32];
    for (unsigned at = threadIdx.x; at < 32u; at += blockDim.x)
    {
        agree[at] = 0ull;
    }
    __syncthreads();

    uint32_t held[8];
    for (unsigned slot = 0u; slot < 8u; slot += 1u)
    {
        held[slot] = reference[slot];
    }

    const uint64_t stride = (uint64_t)gridDim.x * (uint64_t)blockDim.x;
    for (uint64_t trial = ((uint64_t)blockIdx.x * blockDim.x) + threadIdx.x; trial < trials; trial += stride)
    {
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull);

        uint32_t message[16];
        uint32_t flipped[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            message[slot] = (uint32_t)splitmix(generator);
            flipped[slot] = message[slot];
        }
        flipped[where / 32u] ^= (1u << (where % 32u));

        uint32_t plain[8];
        uint32_t other[8];
        compress_to(message, rounds, plain, KEEP_EVERYTHING);
        compress_to(flipped, rounds, other, KEEP_EVERYTHING);

        uint32_t truth[8];
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            truth[slot] = plain[slot] ^ other[slot];
        }

        for (unsigned by = 0u; by < 32u; by += 1u)
        {
            unsigned matched = 0u;
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                matched += (unsigned)__popc(turn(held[slot], by) & truth[slot]);
            }
            atomicAdd(&agree[by], (unsigned long long)matched);
        }
    }
    __syncthreads();

    for (unsigned at = threadIdx.x; at < 32u; at += blockDim.x)
    {
        atomicAdd(&tally[at], agree[at]);
    }
}

/**
 * @brief Fills the matrix from randomness with a known plane wave in it, to calibrate the readout.
 *
 * @param[in]     trials    How many draws.
 * @param[in]     seed      Stream seed.
 * @param[in,out] matrix    512 by 256 counters [BORROWS].
 * @param[in]     frequency Cycles of the wave across the thirty-two residues.
 * @param[in]     strength  Bias amplitude is 2^-(strength+1). A larger number is a fainter wave.
 * @note The kernel drives the flip rate of a cell to 0.5 +/- 2^-(strength+1), by the sign of the wave at its residue.
 *       The bias needs no arithmetic on probabilities. A base bit at one half takes an or with a rare bit at
 *       2^-strength to push the rate up. It takes an and with the complement of that bit to push it down. Both land on
 *       0.5 +/- 2^-(strength+1) exactly.
 * @note The rare bit is the and of `strength` draws. The loop leaves early once all eight words reach zero. Without
 *       that, a faint wave would cost the most, which is backwards. At large strength, the words are almost always zero
 *       after four or five ands.
 */
__global__ void fill_wave_row(uint64_t trials, uint64_t seed, unsigned long long *matrix, unsigned frequency,
                              unsigned strength, unsigned shift)
{
    __shared__ unsigned long long tally[OUTPUT_BITS];
    for (unsigned at = threadIdx.x; at < OUTPUT_BITS; at += blockDim.x)
    {
        tally[at] = 0u;
    }
    __syncthreads();

    const unsigned input_bit = blockIdx.x;

    // Which output bits the wave pushes up, as eight words of thirty-two, built once per block.
    uint32_t upward[8];
    for (unsigned slot = 0u; slot < 8u; slot += 1u)
    {
        uint32_t word = 0u;
        for (unsigned bit = 0u; bit < 32u; bit += 1u)
        {
            const unsigned out_bit = (slot * 32u) + bit;
            const unsigned residue = (((input_bit % 32u) + 32u - (out_bit % 32u)) + 32u - (shift % 32u)) % 32u;
            if (((residue * frequency) % 32u) < 16u)
            {
                word |= (1u << bit);
            }
        }
        upward[slot] = word;
    }

    for (uint64_t trial = threadIdx.x; trial < trials; trial += blockDim.x)
    {
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull) + ((uint64_t)input_bit * 0xd1b54a32d192ed03ull);

        uint32_t base[8];
        uint32_t rare[8];
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            base[slot] = (uint32_t)splitmix(generator);
            rare[slot] = (uint32_t)splitmix(generator);
        }

        for (unsigned step = 1u; step < strength; step += 1u)
        {
            uint32_t standing = 0u;
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                rare[slot] &= (uint32_t)splitmix(generator);
                standing |= rare[slot];
            }
            if (standing == 0u)
            {
                break;
            }
        }

        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            const uint32_t pushed_up = base[slot] | rare[slot];
            const uint32_t pushed_down = base[slot] & ~rare[slot];
            uint32_t apart = (pushed_up & upward[slot]) | (pushed_down & ~upward[slot]);
            while (apart != 0u)
            {
                const unsigned bit = (unsigned)__ffs((int)apart) - 1u;
                atomicAdd(&tally[(slot * 32u) + bit], 1ull);
                apart &= apart - 1u;
            }
        }
    }
    __syncthreads();

    unsigned long long *const row = &matrix[(size_t)input_bit * OUTPUT_BITS];
    for (unsigned at = threadIdx.x; at < OUTPUT_BITS; at += blockDim.x)
    {
        atomicAdd(&row[at], tally[at]);
    }
}

/**
 * @brief Exclusive-ors the output over every corner of a d-dimensional input cube.
 *
 * @param[in]     base      Sixteen message words to build the cube around [BORROWS].
 * @param[in]     positions Which d message bits the cube varies [BORROWS].
 * @param[in]     dimension How many bits. The cube has 2^dimension corners.
 * @param[in]     rounds    How many rounds to run.
 * @param[in]     keep      Which sub-functions the variant keeps.
 * @param[in,out] folded    Eight words that take the exclusive-or of the corners [BORROWS].
 * @note A Boolean function of algebraic degree below d has every d-th order derivative identically zero, and that
 *       derivative is exactly this sum. A fold that comes out zero therefore proves the degree is under d. One that
 *       does not proves it is at least d. This is a proof instead of a statistic: no sample, no threshold, and a single
 *       nonzero fold settles it.
 * @note One thread per corner, combined by exclusive-or with atomics, and not one thread that walks the whole cube. At
 *       dimension twenty, that is a million corners, and no thread should walk them alone.
 */
__global__ void fold_cube(const uint32_t *base, const unsigned *positions, unsigned dimension, unsigned rounds,
                          unsigned keep, uint32_t *folded)
{
    const uint64_t corners = (uint64_t)1u << dimension;
    const uint64_t stride = (uint64_t)gridDim.x * blockDim.x;

    for (uint64_t corner = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x; corner < corners; corner += stride)
    {
        uint32_t message[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            message[slot] = base[slot];
        }
        for (unsigned bit = 0u; bit < dimension; bit += 1u)
        {
            if (((corner >> bit) & 1ull) != 0ull)
            {
                const unsigned where = positions[bit];
                message[where / 32u] ^= (1u << (where % 32u));
            }
        }

        uint32_t state[8];
        compress_to(message, rounds, state, keep);
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            atomicXor(&folded[slot], state[slot]);
        }
    }
}

/**
 * @brief Counts how often an output bit agrees with an input bit, for one input bit per block.
 *
 * @param[in]     trials How many messages to draw.
 * @param[in]     seed   Stream seed.
 * @param[in]     rounds How many rounds to run.
 * @param[in]     keep   Which sub-functions the variant keeps.
 * @param[in,out] agree  256 counters for this block's input bit [BORROWS].
 * @note A different axis from everything above. The SAC matrix is differential: it asks what a *change* in the input
 *       does to the output. This is linear: it asks whether the output value *equals* the input value more often than
 *       chance. The two resist different attacks, and a function can be strong in one and weak in the other. A bucket
 *       labeled "nonlinear" that only a differential probe examined is therefore half unexamined.
 * @note Every block hashes the same messages again, which is 512 times redundant and simplest. The hash is cheap next
 *       to the alternative, which is 131072 global counters touched per trial.
 */
__global__ void fill_linear_row(uint64_t trials, uint64_t seed, unsigned rounds, unsigned keep,
                                unsigned long long *agree)
{
    __shared__ unsigned long long tally[OUTPUT_BITS];
    for (unsigned at = threadIdx.x; at < OUTPUT_BITS; at += blockDim.x)
    {
        tally[at] = 0u;
    }
    __syncthreads();

    const unsigned input_bit = blockIdx.x;

    for (uint64_t trial = threadIdx.x; trial < trials; trial += blockDim.x)
    {
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull);
        uint32_t message[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            message[slot] = (uint32_t)splitmix(generator);
        }

        uint32_t state[8];
        compress_to(message, rounds, state, keep);

        const unsigned carried = (message[input_bit / 32u] >> (input_bit % 32u)) & 1u;
        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            uint32_t word = state[slot];
            for (unsigned bit = 0u; bit < 32u; bit += 1u)
            {
                if (((word >> bit) & 1u) == carried)
                {
                    atomicAdd(&tally[(slot * 32u) + bit], 1ull);
                }
            }
        }
    }
    __syncthreads();

    unsigned long long *const row = &agree[(size_t)input_bit * OUTPUT_BITS];
    for (unsigned at = threadIdx.x; at < OUTPUT_BITS; at += blockDim.x)
    {
        atomicAdd(&row[at], tally[at]);
    }
}

/**
 * @brief The same rounds, run backwards, so that the bench can compare a direction against its own reverse.
 *
 * @param[in]     start  Eight state words to unwind [BORROWS].
 * @param[in]     words  One message word per round [BORROWS].
 * @param[in]     rounds How many rounds to undo.
 * @param[in]     keep   Which sub-functions the variant keeps.
 * @param[out]    state  Eight state words [BORROWS].
 * @note The round is a bijection, and its undo is arithmetic, and needs no search. The shift runs the other way. Choice
 *       and majority come again from words that the shift already restored. The two additions become subtractions, or
 *       stay exclusive-ors in the carry-free variant, because an exclusive-or is its own inverse.
 * @note One statistic measures the forward and backward describability. The two directions differ in depth and in
 *       shape. The question is whether they differ in how long a polynomial describes them.
 */
__device__ __forceinline__ void decompress_to(const uint32_t *start, const uint32_t *message, unsigned rounds,
                                              unsigned keep, uint32_t *state)
{
    // The schedule expands here, and does not come from the caller. The sixteen-word message alone serves rounds up to
    // fifteen, where the schedule and the message coincide. From round sixteen on, a read of it would run past the
    // array. The round trip check catches that: at round twenty-two, every trip fails.
    uint32_t schedule[64];
    for (unsigned slot = 0u; slot < 16u; slot += 1u)
    {
        schedule[slot] = message[slot];
    }
    if ((keep & KEEP_SCHEDULE) != 0u)
    {
        for (unsigned slot = 16u; slot < 64u; slot += 1u)
        {
            const uint32_t fifteen = schedule[slot - 15u];
            const uint32_t two = schedule[slot - 2u];
            const uint32_t low = turn(fifteen, 7u) ^ turn(fifteen, 18u) ^ (fifteen >> 3u);
            const uint32_t high = turn(two, 17u) ^ turn(two, 19u) ^ (two >> 10u);
            schedule[slot] = ((keep & KEEP_ADDITION) != 0u) ? (high + schedule[slot - 7u] + low + schedule[slot - 16u])
                                                            : (high ^ schedule[slot - 7u] ^ low ^ schedule[slot - 16u]);
        }
    }
    else
    {
        for (unsigned slot = 16u; slot < 64u; slot += 1u)
        {
            schedule[slot] = schedule[slot - 16u];
        }
    }

    uint32_t a = start[0], b = start[1], c = start[2], d = start[3];
    uint32_t e = start[4], f = start[5], g = start[6], h = start[7];

    for (int round = (int)rounds - 1; round >= 0; round -= 1)
    {
        // The shift runs backwards first. The words that choice and majority read are then the ones that they came from
        // on the way forward.
        const uint32_t was_a = b;
        const uint32_t was_b = c;
        const uint32_t was_c = d;
        const uint32_t was_e = f;
        const uint32_t was_f = g;
        const uint32_t was_g = h;

        const uint32_t big_zero =
            ((keep & KEEP_BIG_ZERO) != 0u) ? (turn(was_a, 2u) ^ turn(was_a, 13u) ^ turn(was_a, 22u)) : 0u;
        const uint32_t many = ((keep & KEEP_MAJORITY) != 0u) ? ((was_a & was_b) | (was_c & (was_a ^ was_b))) : 0u;
        const uint32_t big_one =
            ((keep & KEEP_BIG_ONE) != 0u) ? (turn(was_e, 6u) ^ turn(was_e, 11u) ^ turn(was_e, 25u)) : 0u;
        const uint32_t choose = ((keep & KEEP_CHOOSE) != 0u) ? (was_g ^ (was_e & (was_f ^ was_g))) : 0u;
        const uint32_t rounder = ((keep & KEEP_CONSTANT) != 0u) ? d_constant[round] : 0u;

        uint32_t first;
        uint32_t was_d;
        uint32_t was_h;
        if ((keep & KEEP_ADDITION) != 0u)
        {
            first = a - big_zero - many;
            was_d = e - first;
            was_h = first - big_one - choose - rounder - schedule[round];
        }
        else
        {
            // Exclusive-or is its own inverse, and the carry-free arm therefore undoes itself exactly.
            first = a ^ big_zero ^ many;
            was_d = e ^ first;
            was_h = first ^ big_one ^ choose ^ rounder ^ schedule[round];
        }

        a = was_a;
        b = was_b;
        c = was_c;
        d = was_d;
        e = was_e;
        f = was_f;
        g = was_g;
        h = was_h;
    }

    state[0] = a;
    state[1] = b;
    state[2] = c;
    state[3] = d;
    state[4] = e;
    state[5] = f;
    state[6] = g;
    state[7] = h;
}

/**
 * @brief Checks that the inverse actually inverts, for both variants.
 *
 * @param[in]     trials  How many states to round-trip.
 * @param[in]     seed    Stream seed.
 * @param[in]     rounds  How many rounds to go forward and back.
 * @param[in,out] failed  Counts round trips that did not return the input [BORROWS].
 * @note The direction results rest entirely on decompress_to as the inverse of compress_to. This check measures that,
 *       and does not assume it. An inverse that is subtly wrong produces a clean anti-correlation, and reads as a
 *       discovery. Every error in the ledger of this work has that shape.
 * @note The check covers both variants. Exclusive-or is its own inverse, and the carry-free arm should therefore be
 *       exact trivially. The full arm should be exact because the round is a bijection.
 */
__global__ void round_trip_check(uint64_t trials, uint64_t seed, unsigned rounds, unsigned long long *failed)
{
    const uint64_t stride = (uint64_t)gridDim.x * blockDim.x;

    for (uint64_t trial = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x; trial < trials; trial += stride)
    {
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull);

        uint32_t message[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            message[slot] = (uint32_t)splitmix(generator);
        }

        for (unsigned arm = 0u; arm < 2u; arm += 1u)
        {
            const unsigned keep = (arm == 0u) ? KEEP_EVERYTHING : (KEEP_EVERYTHING & ~KEEP_ADDITION);

            uint32_t went[8];
            uint32_t came[8];
            compress_to(message, rounds, went, keep);
            decompress_to(went, message, rounds, keep, came);

            // The function compress_to starts from the initial value of the standard, and the trip back must produce
            // that value.
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                if (came[slot] != d_start[slot])
                {
                    atomicAdd(failed, 1ull);
                    break;
                }
            }
        }
    }
}

/**
 * @brief How far the carry-free prediction still describes the real difference.
 *
 * @param[in]     trials  How many messages to draw.
 * @param[in]     seed    Stream seed.
 * @param[in]     rounds  How many rounds to run.
 * @param[in,out] agreed  Accumulates matching bits [BORROWS].
 * @param[in,out] counted Accumulates compared bits [BORROWS].
 * @note The polynomial part of SHA-256, the sigmas, the schedule and the constants, is exact and permanent. It is a
 *       property of the function, and cannot decay. What decays is whether a prediction built from it still describes
 *       the real thing. The gap between those two statements decides whether the function destroys structure, or only
 *       buries it.
 * @note The prediction runs the same rounds with exclusive-or in place of addition. That is the standard linearization,
 *       and it is exact whenever no carry fires. Agreement starts at one and falls to one half, which is chance for a
 *       bit. Where it reaches one half, the polynomial description stops working, and the polynomial is still true.
 */
__global__ void carry_free_agreement(uint64_t trials, uint64_t seed, unsigned rounds, unsigned long long *agreed,
                                     unsigned long long *counted, int backward, unsigned long long *by_position)
{
    const uint64_t stride = (uint64_t)gridDim.x * blockDim.x;
    unsigned long long matched = 0u;
    unsigned long long compared = 0u;

    for (uint64_t trial = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x; trial < trials; trial += stride)
    {
        uint64_t generator = seed + (trial * 0x2545f4914f6cdd1dull);

        uint32_t plain[16];
        uint32_t flipped[16];
        for (unsigned slot = 0u; slot < 16u; slot += 1u)
        {
            plain[slot] = (uint32_t)splitmix(generator);
            flipped[slot] = plain[slot];
        }
        const unsigned where = (unsigned)(splitmix(generator) % 512ull);
        flipped[where / 32u] ^= (1u << (where % 32u));

        // The real function, and the same pair through the carry-free one.
        uint32_t real_a[8];
        uint32_t real_b[8];
        uint32_t free_a[8];
        uint32_t free_b[8];

        if (backward == 0)
        {
            compress_to(plain, rounds, real_a, KEEP_EVERYTHING);
            compress_to(flipped, rounds, real_b, KEEP_EVERYTHING);
            compress_to(plain, rounds, free_a, KEEP_EVERYTHING & ~KEEP_ADDITION);
            compress_to(flipped, rounds, free_b, KEEP_EVERYTHING & ~KEEP_ADDITION);
        }
        else
        {
            // Backwards, the difference goes in the state instead of the message, because the state is the input of the
            // inverse. Both members share the message words, exactly as they do forwards.
            uint32_t state_a[8];
            uint32_t state_b[8];
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                state_a[slot] = plain[slot];
                state_b[slot] = plain[slot];
            }
            state_b[(where / 32u) % 8u] ^= (1u << (where % 32u));

            decompress_to(state_a, plain, rounds, KEEP_EVERYTHING, real_a);
            decompress_to(state_b, plain, rounds, KEEP_EVERYTHING, real_b);
            decompress_to(state_a, plain, rounds, KEEP_EVERYTHING & ~KEEP_ADDITION, free_a);
            decompress_to(state_b, plain, rounds, KEEP_EVERYTHING & ~KEEP_ADDITION, free_b);
        }

        for (unsigned slot = 0u; slot < 8u; slot += 1u)
        {
            const uint32_t real_difference = real_a[slot] ^ real_b[slot];
            const uint32_t free_difference = free_a[slot] ^ free_b[slot];

            // Bits where the two differences agree. Complement of the exclusive-or, counted.
            const uint32_t same = ~(real_difference ^ free_difference);
            matched += (unsigned long long)__popc(same);
            compared += 32u;

            // Kept per bit position as well, because a carry moves upward from bit zero and dies at bit thirty-one. Bit
            // zero has no carry-in, and the prediction must describe it exactly. Bit thirty-one carries the most
            // accumulated uncertainty, and wraps into nothing. If the wrap makes the two directions differ, the profile
            // across positions differs between them. If not, both directions wear the same profile.
            for (unsigned bit = 0u; bit < 32u; bit += 1u)
            {
                if (((same >> bit) & 1u) != 0u)
                {
                    atomicAdd(&by_position[bit], 1ull);
                }
            }
        }
    }

    atomicAdd(agreed, matched);
    atomicAdd(counted, compared);
}

/**
 * @brief Turns the counter matrix into the field of per-cell deviations in standard errors.
 *
 * @param[in]  matrix 512 by 256 counters [BORROWS].
 * @param[in]  trials Draws behind each counter.
 * @param[out] field  One z per cell, same order [BORROWS].
 * @note Under the null, each z is standard normal, and the field is white noise. Everything below asks whether it is,
 *       and a hash has no business with anything else.
 */
static void fill_deviation_field(const std::vector<unsigned long long> &matrix, uint64_t trials,
                                 std::vector<double> &field)
{
    const double per_cell = 0.5 / sqrt((double)trials);
    for (size_t at = 0u; at < matrix.size(); at += 1u)
    {
        const double rate = (double)matrix[at] / (double)trials;
        field[at] = (rate - 0.5) / per_cell;
    }
}

/**
 * @brief Correlation of the field with itself shifted along the output axis.
 *
 * @param[in] field One z per cell [BORROWS].
 * @param[in] lag   How many output bits to shift, wrapping within the row.
 * @return Correlation coefficient, zero under a white null.
 * @note A wrap instead of a truncation keeps every lag on the same 131072 pairs. The lags then compare to each other,
 *       and to the analytic error of one over the square root of that.
 */
static double correlate_along_output(const std::vector<double> &field, unsigned lag)
{
    double product = 0.0;
    double power = 0.0;
    for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
    {
        const double *const here = &field[(size_t)row * OUTPUT_BITS];
        for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
        {
            product += here[column] * here[(column + lag) % OUTPUT_BITS];
            power += here[column] * here[column];
        }
    }
    return (power > 0.0) ? (product / power) : 0.0;
}

/**
 * @brief Correlation of the field with itself shifted along the input axis.
 *
 * @param[in] field One z per cell [BORROWS].
 * @param[in] lag   How many input bits to shift, wrapping within the column.
 * @return Correlation coefficient, zero under a white null.
 */
static double correlate_along_input(const std::vector<double> &field, unsigned lag)
{
    double product = 0.0;
    double power = 0.0;
    for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
    {
        const double *const here = &field[(size_t)row * OUTPUT_BITS];
        const double *const there = &field[(size_t)((row + lag) % INPUT_BITS) * OUTPUT_BITS];
        for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
        {
            product += here[column] * there[column];
            power += here[column] * here[column];
        }
    }
    return (power > 0.0) ? (product / power) : 0.0;
}

/**
 * @brief Folds the field onto the thirty-two residue classes and reports the amplitude of each.
 *
 * @param[in]  field  One z per cell [BORROWS].
 * @param[out] folded Thirty-two sums, each divided by the square root of its count. A class then reads
 *                    in standard errors, as a cell does [BORROWS].
 */
static void fold_onto_residue(const std::vector<double> &field, double *folded)
{
    double counted[32];
    for (unsigned at = 0u; at < 32u; at += 1u)
    {
        folded[at] = 0.0;
        counted[at] = 0.0;
    }
    for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
    {
        for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
        {
            const unsigned residue = ((row % 32u) + 32u - (column % 32u)) % 32u;
            folded[residue] += field[((size_t)row * OUTPUT_BITS) + column];
            counted[residue] += 1.0;
        }
    }
    for (unsigned at = 0u; at < 32u; at += 1u)
    {
        if (counted[at] > 0.0)
        {
            folded[at] /= sqrt(counted[at]);
        }
    }
}

/**
 * @brief One complex line of the residue spectrum, normalized so that each part is standard normal.
 *
 * @param[in]  folded    Thirty-two residue amplitudes [BORROWS].
 * @param[in]  frequency Which line.
 * @param[out] real_part Cosine component [BORROWS].
 * @param[out] imaginary Sine component [BORROWS].
 * @note Each folded amplitude is already one standard error. The raw sum then has variance sixteen, and the divide by
 *       four makes both parts unit normal under the null. The phase stays, and the kernel does not discard it, because
 *       phase carries motion.
 */
static void residue_spectrum_line(const double *folded, unsigned frequency, double *real_part, double *imaginary)
{
    double summed_real = 0.0;
    double summed_imaginary = 0.0;
    for (unsigned at = 0u; at < 32u; at += 1u)
    {
        const double angle = 6.283185307179586 * (double)(frequency * at) / 32.0;
        summed_real += folded[at] * cos(angle);
        summed_imaginary -= folded[at] * sin(angle);
    }
    *real_part = summed_real / 4.0;
    *imaginary = summed_imaginary / 4.0;
}

/**
 * @brief Largest discrete-transform magnitude of the residue profile, and where it sits.
 *
 * @param[in]  folded  Thirty-two residue amplitudes [BORROWS].
 * @param[out] at_what Which frequency carried it [BORROWS].
 * @return The magnitude, normalized so that a white profile gives about one.
 */
static double loudest_residue_tone(const double *folded, unsigned *at_what)
{
    double loudest = 0.0;
    *at_what = 0u;
    for (unsigned frequency = 1u; frequency < 16u; frequency += 1u)
    {
        double real_part = 0.0;
        double imaginary = 0.0;
        for (unsigned at = 0u; at < 32u; at += 1u)
        {
            const double angle = 6.283185307179586 * (double)(frequency * at) / 32.0;
            real_part += folded[at] * cos(angle);
            imaginary -= folded[at] * sin(angle);
        }
        const double magnitude = sqrt(((real_part * real_part) + (imaginary * imaginary)) / 16.0);
        if (magnitude > loudest)
        {
            loudest = magnitude;
            *at_what = frequency;
        }
    }
    return loudest;
}

} // namespace

int main(int argc, char **argv)
{
    const unsigned trial_bits = (argc > 1) ? (unsigned)atoi(argv[1]) : 20u;
    const unsigned first_round = (argc > 2) ? (unsigned)atoi(argv[2]) : 45u;
    const unsigned last_round = (argc > 3) ? (unsigned)atoi(argv[3]) : 64u;
    const uint64_t trials = (uint64_t)1u << trial_bits;

    // The arm "waves" runs the spatial arm alone. Every other arm here has a settled result. A run of all of them to
    // reach the last one costs ten minutes a look. An instrument with that cost gets guessed at, and one without it
    // gets iterated on.
    const bool waves_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "waves", .ci = EMBED_FALSE));
    const bool dispersion_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "dispersion", .ci = EMBED_FALSE));
    const bool shadow_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "shadow", .ci = EMBED_FALSE));
    const bool assay_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "assay", .ci = EMBED_FALSE));
    const bool coherent_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "coherent", .ci = EMBED_FALSE));
    const bool construct_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "construct", .ci = EMBED_FALSE));
    const bool conditional_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "conditional", .ci = EMBED_FALSE));
    const bool sources_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "sources", .ci = EMBED_FALSE));
    const bool signed_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "signed", .ci = EMBED_FALSE));
    const bool cells_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "cells", .ci = EMBED_FALSE));
    const bool miner_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "miner", .ci = EMBED_FALSE));
    const bool rotations_only =
        (argc > 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "rotations", .ci = EMBED_FALSE));

    printf("================================================================\n");
    printf("  The strict avalanche criterion, sampled hard\n");
    printf("================================================================\n");
    printf("\n  Vaughn and Borowczak report SHA-256 satisfying the SAC from round 23 to 53,\n");
    printf("  failing it from 54 to 57, and satisfying it again from 58. A measured\n");
    printf("  non-monotonicity in the diffusion of a function this studied is worth\n");
    printf("  reproducing before it is believed.\n");
    printf("\n  Their margin is thin: 0.49728 against a threshold of 0.49734, six parts in a\n");
    printf("  hundred thousand, at a million trials where one standard deviation is 0.0005.\n");
    printf("  That is 5.44 standard deviations at their sample and would be far more at ours,\n");
    printf("  so the question is answerable by sampling harder.\n");

    const double deviation = 0.5 / sqrt((double)trials);
    printf("\n  Trials per cell: 2^%u = %llu. One standard deviation is %.8f, against their\n", trial_bits,
                (unsigned long long)trials, deviation);
    printf("  0.00050000. Their reported dip of 0.00272 below one half would read %.1f\n", 0.00272 / deviation);
    printf("  standard deviations here if it is real.\n");

    unsigned long long *device_matrix = nullptr;
    cudaMalloc((void **)&device_matrix, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
    std::vector<unsigned long long> matrix((size_t)INPUT_BITS * OUTPUT_BITS);

    if (conditional_only)
    {
        // ---------------------------------------------------------------------------------------
        // The blindness that every other arm here shares.
        //
        // Each reading in this file averages over the message. That makes them all blind the same way: they see where
        // bits go, and never what values sent them there. It is the same failure as a read of a word by its token and
        // never its letters. The index is visible, and the content is not.
        //
        // The nonlinearity of SHA-256 is entirely value-gated. Choose is a selector: bit k of e decides whether bit k
        // of the result comes from f or from g. There are therefore two transport regimes, and a value decides which
        // one runs. The a chain gates Majority, and everything gates the carries. An average over messages mixes those
        // regimes and reports their mean. A structure present in both halves with opposite sign cancels out of that
        // mean entirely.
        //
        // The arm therefore splits the messages by the selector, and builds the two folds separately. If they agree,
        // the average cost nothing, and every earlier reading stands. If they differ, the unconditional matrix reports
        // the mean of two different objects.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  Conditioned on the selector, not averaged over it\n");
        printf("================================================================\n");

        const unsigned GATE_AT = 8u;
        const unsigned GATE_BIT = 16u;

        printf("\n  Gate: bit %u of e at round %u, which is the bit Choose reads to decide\n", GATE_BIT, GATE_AT);
        printf("  whether that position passes f or g. Both members of a pair are gated on\n");
        printf("  the base message, so a pair never splits across the two halves.\n");

        const size_t plane = (size_t)INPUT_BITS * OUTPUT_BITS;
        unsigned long long *device_pair = nullptr;
        cudaMalloc((void **)&device_pair, 2u * plane * sizeof(unsigned long long));
        std::vector<unsigned long long> both(2u * plane);

        unsigned long long *device_counts = nullptr;
        cudaMalloc((void **)&device_counts, sizeof(unsigned long long));

        printf("\n  %6s %12s %12s %12s %12s %10s\n", "round", "fold |0", "fold |1", "difference", "error",
                    "sigmas");
        printf("  %6s %12s %12s %12s %12s %10s\n", "------", "------------", "------------", "------------",
                    "------------", "----------");

        for (unsigned at_round = 10u; at_round <= 30u; at_round += 2u)
        {
            const uint64_t seed = 353535001ull + ((uint64_t)at_round * 1000003ull);

            unsigned long long ones = 0ull;
            cudaMemset(device_counts, 0, sizeof(unsigned long long));
            count_gate<<<256, 256>>>(trials, seed, GATE_AT, GATE_BIT, device_counts);
            cudaDeviceSynchronize();
            cudaMemcpy(&ones, device_counts, sizeof(unsigned long long), cudaMemcpyDeviceToHost);
            const double high = (double)ones;
            const double low = (double)trials - high;

            cudaMemset(device_pair, 0, 2u * plane * sizeof(unsigned long long));
            fill_conditional_row<<<INPUT_BITS, 256>>>(trials, seed, at_round, GATE_AT, GATE_BIT, device_pair);
            cudaDeviceSynchronize();
            cudaMemcpy(both.data(), device_pair, 2u * plane * sizeof(unsigned long long), cudaMemcpyDeviceToHost);

            // Each half folded onto the residues, then the halves subtracted. Anything that the two regimes share
            // cancels, and that is the intent: the common mode goes with it.
            double folded_low[32];
            double folded_high[32];
            double counted[32];
            for (unsigned at = 0u; at < 32u; at += 1u)
            {
                folded_low[at] = 0.0;
                folded_high[at] = 0.0;
                counted[at] = 0.0;
            }
            for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
            {
                for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
                {
                    const unsigned residue = ((row % 32u) + 32u - (column % 32u)) % 32u;
                    const size_t at = ((size_t)row * OUTPUT_BITS) + column;
                    if (low > 0.0)
                    {
                        folded_low[residue] += ((double)both[at] / low) - 0.5;
                    }
                    if (high > 0.0)
                    {
                        folded_high[residue] += ((double)both[plane + at] / high) - 0.5;
                    }
                    counted[residue] += 1.0;
                }
            }

            double loudest = 0.0;
            for (unsigned at = 0u; at < 32u; at += 1u)
            {
                if (counted[at] <= 0.0)
                {
                    continue;
                }
                const double gap = (folded_low[at] - folded_high[at]) / sqrt(counted[at]);
                if (fabs(gap) > fabs(loudest))
                {
                    loudest = gap;
                }
            }

            // A difference of two independent rates has the variance of both. The error is therefore the per-cell error
            // of each half, added in quadrature.
            const double error = 0.5 * sqrt((1.0 / (low > 0.0 ? low : 1.0)) + (1.0 / (high > 0.0 ? high : 1.0)));
            const double sigmas = loudest / error;

            printf("  %6u %12.0f %12.0f %12.6f %12.6f %10.2f\n", at_round, low, high, loudest, error, sigmas);
        }

        cudaFree(device_pair);
        cudaFree(device_counts);
        cudaFree(device_matrix);

        printf("\n  The largest of 32 residue classes peaks near %.2f sigmas under a null,\n",
                    sqrt(2.0 * log(32.0)));
        printf("  and 11 depths were read, so the union peaks near %.2f.\n",
                    sqrt(2.0 * log(32.0 * 11.0)));
        printf("\n  A difference standing above that says the unconditional matrix has been\n");
        printf("  reporting the mean of two different objects, and every reading built on it\n");
        printf("  is an average across a regime boundary. A difference at zero says the\n");
        printf("  averaging cost nothing and the earlier readings stand as they are.\n");
        return 0;
    }

    if (construct_only)
    {
        // ---------------------------------------------------------------------------------------
        // Construction instead of a sample.
        //
        // Every instrument before this one asks what deviates from chance over random messages, and all of them go flat
        // by round 23. The published attacks reach 39 steps, and the gap is not sensitivity. A 39-step characteristic
        // has a probability near 2^-100, and no sample finds an event that rare. The attacks do not detect it. They
        // *construct* it: they choose the input, and do not draw it.
        //
        // The first step of that construction is already here. A linearized function, with exclusive-or in place of
        // addition in the schedule as well as the state, makes difference propagation deterministic. A search for a
        // difference then replaces a measurement. Published differential work on SHA-2 starts the same way: find a
        // low-weight characteristic in the linear model, then bound the real probability against it.
        //
        // This sweep covers every member of its set, and is not a sample. The answer is therefore a true minimum, and
        // not the smallest thing that a sample happened to contain.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  Constructed differences: the sparsest the linear model carries\n");
        printf("================================================================\n");

        printf("\n  Sampling asks what deviates from chance and stops at round 23. This asks\n");
        printf("  what an attacker could choose, which is a different question and the one the\n");
        printf("  published attacks answer. The linearised function is deterministic, so every\n");
        printf("  candidate is evaluated exactly and the minimum is a true minimum.\n");

        printf("\n  Weight of the output difference, over all 512 single-bit message\n");
        printf("  differences and all %llu pairs.\n", (unsigned long long)((INPUT_BITS * (INPUT_BITS - 1u)) / 2u));

        unsigned long long *device_best = nullptr;
        cudaMalloc((void **)&device_best, sizeof(unsigned long long));

        printf("\n  %6s %10s %10s %12s %10s %12s\n", "round", "best 1", "at bit", "best 2", "at pair", "random 1");
        printf("  %6s %10s %10s %12s %10s %12s\n", "------", "----------", "----------", "------------",
                    "----------", "------------");

        const unsigned long long SINGLES = INPUT_BITS;
        const unsigned long long PAIRS = ((unsigned long long)INPUT_BITS * (INPUT_BITS - 1u)) / 2u;

        for (unsigned at_round = 8u; at_round <= 40u; at_round += 2u)
        {
            const unsigned long long empty = ~0ull;

            cudaMemcpy(device_best, &empty, sizeof(unsigned long long), cudaMemcpyHostToDevice);
            sweep_linear_weight<<<256, 256>>>(at_round, 0u, SINGLES, device_best);
            cudaDeviceSynchronize();
            unsigned long long one_best = 0ull;
            cudaMemcpy(&one_best, device_best, sizeof(unsigned long long), cudaMemcpyDeviceToHost);

            cudaMemcpy(device_best, &empty, sizeof(unsigned long long), cudaMemcpyHostToDevice);
            sweep_linear_weight<<<1024, 256>>>(at_round, 1u, PAIRS, device_best);
            cudaDeviceSynchronize();
            unsigned long long two_best = 0ull;
            cudaMemcpy(&two_best, device_best, sizeof(unsigned long long), cudaMemcpyDeviceToHost);

            const unsigned one_weight = (unsigned)(one_best >> 40);
            const unsigned long long one_where = one_best & 0xffffffffffull;
            const unsigned two_weight = (unsigned)(two_best >> 40);
            const unsigned long long two_where = two_best & 0xffffffffffull;

            // A random 256-bit difference has weight 128. A characteristic must beat that to be worth anything, and the
            // column puts the comparison on the page.
            printf("  %6u %10u %10llu %12u %10llu %12u\n", at_round, one_weight, one_where, two_weight, two_where,
                        128u);
        }

        cudaFree(device_best);
        cudaFree(device_matrix);

        printf("\n  A weight far below 128 is a characteristic worth having: the difference\n");
        printf("  stays sparse, so the conditions an attacker must force are few. A weight at\n");
        printf("  128 means the linear model has diffused completely and offers nothing to\n");
        printf("  build on at that depth.\n");
        printf("\n  This is the linear model only. The real function's probability of following\n");
        printf("  any of these is a separate and much smaller number, which is exactly the\n");
        printf("  'low-probability uncontrolled part' the published work names as its wall.\n");
        return 0;
    }

    if (coherent_only)
    {
        // ---------------------------------------------------------------------------------------
        // A coherent receiver, with the local oscillator generated, and not tracked.
        //
        // Every reading in this work estimates the structure from the data alone, as a receiver with no reference does.
        // This receiver is also the transmitter: the bench chooses the input difference. It can therefore *generate* a
        // reference, and needs no lock to one.
        //
        // The bench propagates the same difference through the GF(2)-linear version of the round function: no addition,
        // no Choose, no Majority. The map is then linear, and the difference depends only on the difference. That gives
        // an exact phase reference. It is not a tracked one with loop noise, but an exact one, which is better than any
        // real radar gets.
        //
        // The bench mixes the true difference against that reference at every rotation, and takes the rotation that
        // agrees best. That is a phase-locked loop, and the rotation that it locks to is the loop phase. A lock that
        // sits at zero says that the difference is linearly polarized, and the carries only add magnitude. A lock that
        // walks with depth says that the difference vector turns in the plane of its linear part and its carry part.
        // That is circular polarization, and the direction of the walk is a handedness of the round function.
        //
        // What remains after the de-rotation, the true difference exclusive-ored with the rotated reference, is
        // everything that the carries contributed, and only that.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  Coherent detection against a generated reference\n");
        printf("================================================================\n");

        // The reference keeps only what is linear over GF(2): the rotations and the schedule. With addition, Choose and
        // Majority removed, the map is linear. The propagated difference is then a function of the input difference
        // alone, with no dependence on the message.
        const unsigned DEEPEST = 26u;
        const unsigned WHERE = 0u;

        printf("\n  Reference: the same difference through the GF(2)-linear round function,\n");
        printf("  which is exact because a linear map sends a difference to a difference\n");
        printf("  whatever the message is. Input difference is bit %u.\n", WHERE);
        printf("\n  Lock is the rotation of the reference that agrees best with the true\n");
        printf("  difference. The largest of 32 agreements peaks near %.2f sigmas under a\n",
                    sqrt(2.0 * log(32.0)));
        printf("  null whatever the data does, so a lock is only a lock above that.\n");

        printf("\n  %6s %8s %10s %12s %10s %12s\n", "round", "lock", "agreement", "sigmas", "ref weight",
                    "walked");
        printf("  %6s %8s %10s %12s %10s %12s\n", "------", "--------", "----------", "------------", "----------",
                    "------------");

        unsigned long long *device_tally = nullptr;
        cudaMalloc((void **)&device_tally, 32u * sizeof(unsigned long long));
        std::vector<unsigned long long> tally(32u);

        int last_lock = -1;
        for (unsigned at_round = 2u; at_round <= DEEPEST; at_round += 1u)
        {
            // The reference, computed on the device once per depth. It is linear, and the base message is therefore
            // irrelevant: it carries no sampling error at all.
            uint32_t *device_reference = nullptr;
            cudaMalloc((void **)&device_reference, 8u * sizeof(uint32_t));
            make_reference<<<1, 1>>>(at_round, WHERE, device_reference);
            cudaDeviceSynchronize();

            uint32_t reference[8];
            cudaMemcpy(reference, device_reference, 8u * sizeof(uint32_t), cudaMemcpyDeviceToHost);

            unsigned reference_weight = 0u;
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                uint32_t left = reference[slot];
                while (left != 0u)
                {
                    reference_weight += 1u;
                    left &= left - 1u;
                }
            }

            cudaMemset(device_tally, 0, 32u * sizeof(unsigned long long));
            lock_against_reference<<<256, 256>>>(trials, 242424001ull + (at_round * 1000003ull), at_round, WHERE,
                                                 device_reference, device_tally);
            cudaDeviceSynchronize();
            cudaMemcpy(tally.data(), device_tally, 32u * sizeof(unsigned long long), cudaMemcpyDeviceToHost);
            cudaFree(device_reference);

            // The tally counts agreement over the set bits of the reference only. A bit that the reference does not
            // reach carries no phase information, and would dilute the lock toward one half whatever the true rate.
            const double compared = (double)trials * (double)reference_weight;
            // The seed is one half, which is zero distance, and the first real rate displaces it. A seed of zero would
            // be the initial value furthest from one half, and nothing would ever beat it. The whole column would then
            // print the seed.
            unsigned best = 0u;
            double best_rate = 0.5;
            for (unsigned turn_by = 0u; turn_by < 32u; turn_by += 1u)
            {
                const double rate = (compared > 0.0) ? ((double)tally[turn_by] / compared) : 0.5;
                if (fabs(rate - 0.5) > fabs(best_rate - 0.5))
                {
                    best_rate = rate;
                    best = turn_by;
                }
            }
            const double spread = 0.5 / sqrt(compared > 0.0 ? compared : 1.0);
            const double sigmas = (best_rate - 0.5) / spread;

            const char *walked = "";
            if (last_lock >= 0)
            {
                const int step = ((int)best - last_lock + 32) % 32;
                walked = (step == 0) ? "" : "walked";
            }
            last_lock = (int)best;

            printf("  %6u %8u %10.5f %12.1f %10u %12s\n", at_round, best, best_rate, sigmas, reference_weight,
                        walked);
        }

        cudaFree(device_tally);
        cudaFree(device_matrix);

        printf("\n  A lock that stays at one rotation is a difference that keeps its linear\n");
        printf("  part's orientation: linear polarisation, the carries adding magnitude only.\n");
        printf("  A lock that walks with depth is the difference vector rotating in the plane\n");
        printf("  its linear part and its carry part span, and the direction is a handedness.\n");
        printf("\n  The reference weight is the linear part's own growth, which is exact and\n");
        printf("  owes nothing to sampling. Where it saturates the reference stops carrying\n");
        printf("  phase and the lock below it means nothing.\n");
        return 0;
    }

    if (assay_only)
    {
        // ---------------------------------------------------------------------------------------
        // A compositional assay.
        //
        // A radar with measurements of a library of known materials can tell the material of an unknown return, and not
        // merely that something is there. Here, nobody needs to collect the library, because the bench can build any
        // variant of the function on demand. It takes the same residue spectrum from SHA-256 with one sub-function
        // removed at a time. What changes says which channel that sub-function carries.
        //
        // The predictions come before the run, and nobody reads them off it:
        //   - without Sigma1: classes 6, 11 and 25 go, because those are its rotation amounts.
        //   - without Sigma0: classes 2, 13 and 22 go, and they are weak to begin with.
        //   - without addition: class 31 goes, because -1 mod 32 is the carry, and there are no carries without
        //     addition. This is the sharpest of the three.
        //   - without Choose: a large change, because Choose is in T1.
        //   - without Majority: little change, because Majority is in T2, and T2 is the weak side.
        //
        // A variant that removes a named rotation and keeps its residue would refute the reading of the spectrum
        // entirely. That makes this worth a run.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  Compositional assay: the spectrum of known variants\n");
        printf("================================================================\n");

        std::vector<double> field((size_t)INPUT_BITS * OUTPUT_BITS);
        double folded[32];

        const unsigned FROM = 4u;
        const unsigned UNTIL = 22u;

        struct Variant
        {
            const char *name;
            unsigned keep;
        };
        const Variant variants[] = {
            {"full", KEEP_EVERYTHING},
            {"no_majority", KEEP_EVERYTHING & ~KEEP_MAJORITY},
            {"no_choose", KEEP_EVERYTHING & ~KEEP_CHOOSE},
            {"no_sigma0", KEEP_EVERYTHING & ~KEEP_BIG_ZERO},
            {"no_sigma1", KEEP_EVERYTHING & ~KEEP_BIG_ONE},
            {"no_addition", KEEP_EVERYTHING & ~KEEP_ADDITION},
            {"no_schedule", KEEP_EVERYTHING & ~KEEP_SCHEDULE},
            {"no_constant", KEEP_EVERYTHING & ~KEEP_CONSTANT},
        };
        const unsigned HOW_MANY = (unsigned)(sizeof(variants) / sizeof(variants[0]));

        std::FILE *const handle = fopen("assay.csv", "w");
        if (handle == nullptr)
        {
            printf("\n  could not open assay.csv\n");
            cudaFree(device_matrix);
            return 1;
        }
        fprintf(handle, "variant,residue,round,value\n");

        printf("\n  %u variants across rounds %u to %u, each round on its own seed.\n", HOW_MANY, FROM, UNTIL);

        for (unsigned which = 0u; which < HOW_MANY; which += 1u)
        {
            for (unsigned at_round = FROM; at_round <= UNTIL; at_round += 1u)
            {
                cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
                fill_sac_row<<<INPUT_BITS, 256>>>(trials, 616161001ull + ((uint64_t)at_round * 1000003ull), at_round,
                                                  device_matrix, variants[which].keep);
                cudaDeviceSynchronize();
                cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                           cudaMemcpyDeviceToHost);
                fill_deviation_field(matrix, trials, field);
                fold_onto_residue(field, folded);

                for (unsigned at = 0u; at < 32u; at += 1u)
                {
                    fprintf(handle, "%s,%u,%u,%.4f\n", variants[which].name, at, at_round, folded[at]);
                }
            }
            printf("  %-14s done\n", variants[which].name);
        }
        fclose(handle);
        cudaFree(device_matrix);

        printf("\n  Written to assay.csv. Process with utils/maint/radar/radar_assay.py\n");
        return 0;
    }

    if (rotations_only)
    {
        // ---------------------------------------------------------------------------------------
        // Why residue twenty-five and not six or eleven.
        //
        // Every certain cell off the diagonal sits at residue 25, and none at the other two amounts of Sigma1, at
        // rounds eight, ten and twelve alike. The account by carry contamination fails its own prediction, and the
        // mechanism is unknown. The published record does not help: the K constants have a derivation, but the rotation
        // amounts come without one.
        //
        // Two readings differ in what they say about a function that nobody built:
        //   - largest wins: the residue that survives is whichever amount is greatest, and 25 is incidental. A variant
        //     with different amounts moves the certain cells to its own largest.
        //   - twenty-five: something about 25 against the word width or the other two amounts. A variant keeps its
        //     cells near 25, or scatters them.
        //
        // Only Sigma1 changes. A difference in the result belongs to the rotation amounts alone.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  Sigma1's rotation amounts, varied\n");
        printf("================================================================\n");

        struct Variant
        {
            unsigned a;
            unsigned b;
            unsigned c;
            const char *note;
        };
        const Variant tried[] = {
            {6u, 11u, 25u, "SHA-256 as specified"},    {6u, 11u, 19u, "largest lowered to 19"},
            {6u, 11u, 30u, "largest raised to 30"},    {3u, 7u, 25u, "others lowered, 25 kept"},
            {13u, 20u, 25u, "others raised, 25 kept"}, {2u, 13u, 22u, "Sigma0's amounts"},
            {7u, 18u, 25u, "small sigma0's, 25 kept"},
        };
        const unsigned HOW_MANY = (unsigned)(sizeof(tried) / sizeof(tried[0]));
        const unsigned AT_ROUND = 12u;

        printf("\n  Certain cells at round %u, by where they sit. The diagonal at residue 0 is\n", AT_ROUND);
        printf("  addition and is expected in every row; the question is the rest.\n");

        printf("\n  %-26s %8s %8s %10s %s\n", "Sigma1", "certain", "aligned", "skewed", "skewed residues");
        printf("  %-26s %8s %8s %10s %s\n", "--------------------------", "--------", "--------", "----------",
                    "----------------");

        for (unsigned which = 0u; which < HOW_MANY; which += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_rotated_row<<<INPUT_BITS, 256>>>(trials, 979797001ull, AT_ROUND, tried[which].a, tried[which].b,
                                                  tried[which].c, device_matrix);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            unsigned by_residue[32];
            for (unsigned at = 0u; at < 32u; at += 1u)
            {
                by_residue[at] = 0u;
            }
            unsigned total = 0u;
            unsigned aligned = 0u;

            for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
            {
                for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
                {
                    if (matrix[((size_t)row * OUTPUT_BITS) + column] != trials)
                    {
                        continue;
                    }
                    total += 1u;
                    const unsigned residue = ((row % 32u) + 32u - (column % 32u)) % 32u;
                    by_residue[residue] += 1u;
                    if (residue == 0u)
                    {
                        aligned += 1u;
                    }
                }
            }

            char where[128];
            unsigned used = 0u;
            for (unsigned at = 1u; at < 32u; at += 1u)
            {
                if ((by_residue[at] != 0u) && (used < 100u))
                {
                    used += (unsigned)snprintf(&where[used], sizeof(where) - used, "%u:%u ", at, by_residue[at]);
                }
            }
            if (used == 0u)
            {
                snprintf(where, sizeof(where), "none");
            }

            char label[40];
            snprintf(label, sizeof(label), "%u,%u,%u  %s", tried[which].a, tried[which].b, tried[which].c,
                          tried[which].note);
            printf("  %-26s %8u %8u %10u %s\n", label, total, aligned, total - aligned, where);
        }

        cudaFree(device_matrix);

        printf("\n  If the skewed residue follows the largest amount in every row, the reading\n");
        printf("  is that the largest wins and 25 is incidental. If it stays near 25 while the\n");
        printf("  amounts move, it is not. A row where the skew vanishes says the effect needs\n");
        printf("  a relation between the three and not any one of them.\n");
        return 0;
    }

    if (miner_only)
    {
        // ---------------------------------------------------------------------------------------
        // The two shared-work constants of a miner, measured and not derived.
        //
        // A miner can precompute the part of the second block that cannot depend on the nonce. Two constants say how
        // much: 3 shared rounds and 18 shared schedule words. A read of the recurrences gives them. An off-by-one
        // either way is not a slow miner, but a miner that submits wrong shares. They are therefore worth a measurement
        // by the same light-cone argument as the dependency field.
        //
        // A Bitcoin header is eighty bytes, and the second block holds bytes 64 to 79 and its padding. The nonce is
        // bytes 76 to 79, which is W[3] of that block.
        //   - rounds: word w cannot reach the state before round w+1. W[3] therefore leaves rounds 0, 1 and 2
        //     untouched, and the shared round count should measure 3.
        //   - schedule: W[t] reads W[t-2], W[t-7], W[t-15] and W[t-16]. The first expanded word that touches W[3] is
        //     therefore t = 18, and the shared schedule count should measure 18.
        //
        // The arm checks both. It varies only the nonce, and finds the first word and the first round that ever move.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  The miner's shared work, measured\n");
        printf("================================================================\n");

        // The header tail of block 125552. The numbers below are then about a real header, and not a convenient
        // pattern.
        const uint32_t TAIL[4] = {0x9d10aa52u, 0x4dcc1dd0u, 0x1b04864cu, 0x9895d4b1u};

        const unsigned TRIES = 4096u;
        unsigned first_schedule_move = 64u;
        unsigned first_round_move = 64u;

        uint32_t base_schedule[64];

        for (unsigned attempt = 0u; attempt <= TRIES; attempt += 1u)
        {
            uint32_t message[16];
            for (unsigned slot = 0u; slot < 16u; slot += 1u)
            {
                message[slot] = 0u;
            }
            message[0] = TAIL[0];
            message[1] = TAIL[1];
            message[2] = TAIL[2];
            // The nonce. Attempt zero is the header's own, and the rest are random draws.
            message[3] = (attempt == 0u) ? TAIL[3] : (0x9895d4b1u ^ (attempt * 2654435761u));
            message[4] = 0x80000000u;
            message[15] = 640u;

            uint32_t schedule[64];
            for (unsigned slot = 0u; slot < 16u; slot += 1u)
            {
                schedule[slot] = message[slot];
            }
            for (unsigned slot = 16u; slot < 64u; slot += 1u)
            {
                const uint32_t two = schedule[slot - 2u];
                const uint32_t fifteen = schedule[slot - 15u];
                const uint32_t low =
                    ((fifteen >> 7u) | (fifteen << 25u)) ^ ((fifteen >> 18u) | (fifteen << 14u)) ^ (fifteen >> 3u);
                const uint32_t high = ((two >> 17u) | (two << 15u)) ^ ((two >> 19u) | (two << 13u)) ^ (two >> 10u);
                schedule[slot] = schedule[slot - 16u] + low + schedule[slot - 7u] + high;
            }

            // The chaining value from the first block is irrelevant to which words move. The standard initial value
            // therefore stands in for it.
            uint32_t state[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
            uint32_t rounds_seen[64][8];

            for (unsigned round = 0u; round < 64u; round += 1u)
            {
                const uint32_t e = state[4];
                const uint32_t a = state[0];
                const uint32_t big_one =
                    ((e >> 6u) | (e << 26u)) ^ ((e >> 11u) | (e << 21u)) ^ ((e >> 25u) | (e << 7u));
                const uint32_t choose = state[6] ^ (e & (state[5] ^ state[6]));
                const uint32_t big_zero =
                    ((a >> 2u) | (a << 30u)) ^ ((a >> 13u) | (a << 19u)) ^ ((a >> 22u) | (a << 10u));
                const uint32_t many = (a & state[1]) | (state[2] & (a ^ state[1]));
                const uint32_t one = state[7] + big_one + choose + host_constant[round] + schedule[round];
                const uint32_t two = big_zero + many;

                state[7] = state[6];
                state[6] = state[5];
                state[5] = state[4];
                state[4] = state[3] + one;
                state[3] = state[2];
                state[2] = state[1];
                state[1] = state[0];
                state[0] = one + two;

                for (unsigned slot = 0u; slot < 8u; slot += 1u)
                {
                    rounds_seen[round][slot] = state[slot];
                }
            }

            if (attempt == 0u)
            {
                for (unsigned slot = 0u; slot < 64u; slot += 1u)
                {
                    base_schedule[slot] = schedule[slot];
                }
                continue;
            }

            // Only the expanded words. W[0] to W[15] are the message, and W[3] is the nonce itself. A test of whether
            // they move asks nothing: the miner rewrites those per nonce, and shares only the expansion above them.
            for (unsigned slot = 16u; slot < 64u; slot += 1u)
            {
                if ((schedule[slot] != base_schedule[slot]) && (slot < first_schedule_move))
                {
                    first_schedule_move = slot;
                }
            }

            // A round moved when any of its eight state words differs from the same round of the reference nonce. The
            // test compares against the reference on each attempt.
            uint32_t reference[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                     0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
            for (unsigned round = 0u; round < 64u; round += 1u)
            {
                const uint32_t e = reference[4];
                const uint32_t a = reference[0];
                const uint32_t big_one =
                    ((e >> 6u) | (e << 26u)) ^ ((e >> 11u) | (e << 21u)) ^ ((e >> 25u) | (e << 7u));
                const uint32_t choose = reference[6] ^ (e & (reference[5] ^ reference[6]));
                const uint32_t big_zero =
                    ((a >> 2u) | (a << 30u)) ^ ((a >> 13u) | (a << 19u)) ^ ((a >> 22u) | (a << 10u));
                const uint32_t many = (a & reference[1]) | (reference[2] & (a ^ reference[1]));
                const uint32_t one = reference[7] + big_one + choose + host_constant[round] + base_schedule[round];
                const uint32_t two = big_zero + many;
                reference[7] = reference[6];
                reference[6] = reference[5];
                reference[5] = reference[4];
                reference[4] = reference[3] + one;
                reference[3] = reference[2];
                reference[2] = reference[1];
                reference[1] = reference[0];
                reference[0] = one + two;

                bool moved = false;
                for (unsigned slot = 0u; slot < 8u; slot += 1u)
                {
                    if (rounds_seen[round][slot] != reference[slot])
                    {
                        moved = true;
                    }
                }
                if (moved && (round < first_round_move))
                {
                    first_round_move = round;
                }
            }
        }

        printf("\n  Header: block 125552, nonce varied over %u draws, everything else fixed.\n", TRIES);
        printf("  The nonce is W[3] of the second block.\n");

        printf("\n  %-34s %10s %10s %8s\n", "quantity", "measured", "in miner", "verdict");
        printf("  %-34s %10s %10s %8s\n", "----------------------------------", "----------", "----------",
                    "--------");
        printf("  %-34s %10u %10u %8s\n", "first schedule word that moves", first_schedule_move, 18u,
                    (first_schedule_move == 18u) ? "match" : "MISMATCH");
        printf("  %-34s %10u %10u %8s\n", "first round that moves", first_round_move, 3u,
                    (first_round_move == 3u) ? "match" : "MISMATCH");

        printf("\n  Words 0 to %u and rounds 0 to %u cannot see the nonce, so a miner may\n",
                    first_schedule_move - 1u, first_round_move - 1u);
        printf("  compute them once per header. A miner that shares 18 schedule words and 3 rounds\n");
        printf("  claims exactly that. A mismatch above is a miner\n");
        printf("  reusing work the nonce does change, which submits wrong shares instead of\n");
        printf("  slow ones.\n");

        cudaFree(device_matrix);
        return 0;
    }

    if (cells_only)
    {
        // ---------------------------------------------------------------------------------------
        // The identities behind the soliton.
        //
        // The signed projection finds 312 cells that flip with probability one, 135 input bits wide. They translate at
        // exactly 32 bits per round, and keep their form. The projection sums over the output axis, and cannot say
        // *which* output bits those are. The numbers 312 and 135 have no explanation from it.
        //
        // A model worth a test: a flip of bit k of an addend always flips bit k of the sum, because carries only
        // propagate upward. Bit k of a message word therefore flips bit k of T1 deterministically. With it go bit k of
        // a and of e: exactly two cells. Those then ride the copy chains a-b-c-d and e-f-g-h for three further rounds,
        // while nothing mixes them.
        //
        // That model predicts four words of thirty-two bits, with two cells each: 256 cells over 128 bits. The
        // measurement says 312 over 135. The model is close, and wrong by a stated amount: the useful type of wrong.
        // The identities settle it.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  Which cells are certain, and where they sit\n");
        printf("================================================================\n");

        const unsigned LOOKS[] = {8u, 10u, 12u};
        const unsigned HOW_MANY = 3u;

        for (unsigned which = 0u; which < HOW_MANY; which += 1u)
        {
            const unsigned at_round = LOOKS[which];
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_sac_row<<<INPUT_BITS, 256>>>(trials, 686868001ull + ((uint64_t)at_round * 1000003ull), at_round,
                                              device_matrix, KEEP_EVERYTHING);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            // Which state words receive them, and whether the input and output bit positions match within their words.
            // The model says they must, because bit k drives bit k.
            unsigned by_state[8];
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                by_state[slot] = 0u;
            }
            unsigned aligned = 0u;
            unsigned skewed = 0u;
            unsigned total = 0u;
            unsigned per_word[16];
            for (unsigned slot = 0u; slot < 16u; slot += 1u)
            {
                per_word[slot] = 0u;
            }

            for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
            {
                for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
                {
                    if (matrix[((size_t)row * OUTPUT_BITS) + column] != trials)
                    {
                        continue;
                    }
                    total += 1u;
                    by_state[column / 32u] += 1u;
                    per_word[row / 32u] += 1u;
                    if ((row % 32u) == (column % 32u))
                    {
                        aligned += 1u;
                    }
                    else
                    {
                        skewed += 1u;
                    }
                }
            }

            printf("\n  Round %u: %u certain cells\n", at_round, total);
            printf("    by state word   ");
            for (unsigned slot = 0u; slot < 8u; slot += 1u)
            {
                printf("%c:%-5u", (char)('a' + slot), by_state[slot]);
            }
            printf("\n");
            printf("    positions       %u aligned (bit k drives bit k), %u skewed\n", aligned, skewed);
            printf("    by message word ");
            for (unsigned slot = 0u; slot < 16u; slot += 1u)
            {
                if (per_word[slot] != 0u)
                {
                    printf("W%u:%u ", slot, per_word[slot]);
                }
            }
            printf("\n");

            // Every skewed cell by its residue. Sigma1 has three rotation amounts, and a sample of eight would not tell
            // one of them from all three. The count covers the amounts of Sigma0 too, and assumes no answer.
            if (skewed != 0u)
            {
                unsigned by_residue[32];
                unsigned by_word_out[8];
                for (unsigned slot = 0u; slot < 32u; slot += 1u)
                {
                    by_residue[slot] = 0u;
                }
                for (unsigned slot = 0u; slot < 8u; slot += 1u)
                {
                    by_word_out[slot] = 0u;
                }
                for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
                {
                    for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
                    {
                        if ((matrix[((size_t)row * OUTPUT_BITS) + column] != trials) || ((row % 32u) == (column % 32u)))
                        {
                            continue;
                        }
                        by_residue[((row % 32u) + 32u - (column % 32u)) % 32u] += 1u;
                        by_word_out[column / 32u] += 1u;
                    }
                }
                printf("    skewed by residue  ");
                for (unsigned slot = 0u; slot < 32u; slot += 1u)
                {
                    if (by_residue[slot] != 0u)
                    {
                        const char *what = "";
                        if ((slot == 6u) || (slot == 11u) || (slot == 25u))
                        {
                            what = "=S1";
                        }
                        if ((slot == 2u) || (slot == 13u) || (slot == 22u))
                        {
                            what = "=S0";
                        }
                        if (slot == 31u)
                        {
                            what = "=carry";
                        }
                        printf("%u%s:%u  ", slot, what, by_residue[slot]);
                    }
                }
                printf("\n    skewed land in     ");
                for (unsigned slot = 0u; slot < 8u; slot += 1u)
                {
                    if (by_word_out[slot] != 0u)
                    {
                        printf("%c:%u  ", (char)('a' + slot), by_word_out[slot]);
                    }
                }
                printf("\n");
            }
        }

        cudaFree(device_matrix);
        printf("\n  The model predicts every certain cell is aligned and lands in a or e and\n");
        printf("  their copies, two per input bit over four words. Anything skewed, or in a\n");
        printf("  word the copy chain cannot reach, is the model being wrong somewhere\n");
        printf("  specific.\n");
        return 0;
    }

    if (signed_only)
    {
        // ---------------------------------------------------------------------------------------
        // The projection that keeps the sign.
        //
        // Every earlier projection sums z squared, and z squared cannot tell a rate of zero from a rate of one. Both
        // give z = -/+ sqrt(n), and the same square. A bit that changes nothing and a bit that flips its outputs every
        // single time land on the same plateau. They draw as the same color at the same height. Two physically opposite
        // states, one pixel.
        //
        // The naive repair, a sum of signed z, has its own conflation, and a worse one. A row half never-flip and half
        // always-flip sums to zero, and so does a fully mixed row. That would hide the same distinction somewhere else.
        //
        // The three states therefore get counts, and not a sum. A cell is *never* if its count is zero, *always* if its
        // count is every trial, and *mixed* otherwise. Those are exclusive and cover every case, nothing cancels, and
        // each becomes its own surface:
        //   - never: 256 across the whole row is outside the causal cone.
        //   - always: nonzero marks a bit whose effect is still deterministic.
        //   - mixed: rises as the function loses predictability, and is the real frontier.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  The signed projection: never, always, and mixed counted apart\n");
        printf("================================================================\n");

        const unsigned DEEPEST = 40u;

        std::FILE *const handle = fopen("signed.csv", "w");
        if (handle == nullptr)
        {
            printf("\n  could not open signed.csv\n");
            cudaFree(device_matrix);
            return 1;
        }
        fprintf(handle, "type,axis,index,round,value\n");

        printf("\n  %u rounds, each on its own seed. A cell is never, always or mixed, and\n", DEEPEST);
        printf("  those are exclusive, so nothing cancels the way a signed sum would.\n");
        printf("\n  %6s %10s %10s %10s %10s\n", "round", "never", "always", "mixed", "det. bits");
        printf("  %6s %10s %10s %10s %10s\n", "------", "----------", "----------", "----------", "----------");

        for (unsigned at_round = 1u; at_round <= DEEPEST; at_round += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_sac_row<<<INPUT_BITS, 256>>>(trials, 575757001ull + ((uint64_t)at_round * 1000003ull), at_round,
                                              device_matrix, KEEP_EVERYTHING);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            unsigned long long never_all = 0ull;
            unsigned long long always_all = 0ull;
            unsigned long long mixed_all = 0ull;
            unsigned deterministic_rows = 0u;

            for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
            {
                unsigned never_here = 0u;
                unsigned always_here = 0u;
                for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
                {
                    const unsigned long long count = matrix[((size_t)row * OUTPUT_BITS) + column];
                    if (count == 0ull)
                    {
                        never_here += 1u;
                    }
                    else if (count == trials)
                    {
                        always_here += 1u;
                    }
                }
                const unsigned mixed_here = OUTPUT_BITS - never_here - always_here;

                fprintf(handle, "never,inbit,%u,%u,%u\n", row, at_round, never_here);
                fprintf(handle, "always,inbit,%u,%u,%u\n", row, at_round, always_here);
                fprintf(handle, "mixed,inbit,%u,%u,%u\n", row, at_round, mixed_here);

                never_all += never_here;
                always_all += always_here;
                mixed_all += mixed_here;
                if (mixed_here == 0u)
                {
                    deterministic_rows += 1u;
                }
            }

            // The same three counts down the other axis, so that the viewer can read a state bit as well.
            for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
            {
                unsigned never_here = 0u;
                unsigned always_here = 0u;
                for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
                {
                    const unsigned long long count = matrix[((size_t)row * OUTPUT_BITS) + column];
                    if (count == 0ull)
                    {
                        never_here += 1u;
                    }
                    else if (count == trials)
                    {
                        always_here += 1u;
                    }
                }
                fprintf(handle, "never,outbit,%u,%u,%u\n", column, at_round, never_here);
                fprintf(handle, "always,outbit,%u,%u,%u\n", column, at_round, always_here);
                fprintf(handle, "mixed,outbit,%u,%u,%u\n", column, at_round,
                             INPUT_BITS - never_here - always_here);
            }

            printf("  %6u %10llu %10llu %10llu %10u\n", at_round, never_all, always_all, mixed_all,
                        deterministic_rows);
        }

        fclose(handle);
        cudaFree(device_matrix);

        printf("\n  'det. bits' counts input bits whose every output cell is still at an\n");
        printf("  extreme, so the bit's whole effect is predictable. That is the bedrock, and\n");
        printf("  it is now split into the part that changes nothing and the part that\n");
        printf("  changes something every time.\n");
        printf("\n  Written to signed.csv.\n");
        return 0;
    }

    if (sources_only)
    {
        // ---------------------------------------------------------------------------------------
        // The same projections from several sources, for the eye to compare.
        //
        // Every null in this file is a number. This dumps the fields themselves through identical projection code:
        // SHA-256 beside a matrix that is pseudorandom by construction, and beside two ablations. A look can then
        // answer the question "does this look like noise", and not only a statistic.
        //
        // The pseudorandom source runs the same kernel shape, shared tally, atomics and denominator, with eight random
        // words in place of the compression. Anything that the two have in common is the harness. Anything that only
        // SHA-256 has is SHA-256.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  The same field from several sources\n");
        printf("================================================================\n");

        std::vector<double> field((size_t)INPUT_BITS * OUTPUT_BITS);
        double folded[32];

        const unsigned DEEPEST = 48u;

        struct Source
        {
            const char *name;
            unsigned keep;
            bool random;
        };
        const Source sources[] = {
            {"sha256", KEEP_EVERYTHING, false},
            {"psrand", 0u, true},
            {"no_addition", KEEP_EVERYTHING & ~KEEP_ADDITION, false},
            {"no_sigma1", KEEP_EVERYTHING & ~KEEP_BIG_ONE, false},
        };
        const unsigned HOW_MANY = (unsigned)(sizeof(sources) / sizeof(sources[0]));

        std::FILE *const handle = fopen("sources.csv", "w");
        if (handle == nullptr)
        {
            printf("\n  could not open sources.csv\n");
            cudaFree(device_matrix);
            return 1;
        }
        fprintf(handle, "source,type,a,b,round,value\n");

        printf("\n  %u sources across %u rounds, each round on its own seed.\n", HOW_MANY, DEEPEST);

        for (unsigned which = 0u; which < HOW_MANY; which += 1u)
        {
            for (unsigned at_round = 1u; at_round <= DEEPEST; at_round += 1u)
            {
                const uint64_t seed = 464646001ull + ((uint64_t)at_round * 1000003ull);
                cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
                if (sources[which].random)
                {
                    fill_null_row<<<INPUT_BITS, 256>>>(trials, seed, device_matrix, 0u);
                }
                else
                {
                    fill_sac_row<<<INPUT_BITS, 256>>>(trials, seed, at_round, device_matrix, sources[which].keep);
                }
                cudaDeviceSynchronize();
                cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                           cudaMemcpyDeviceToHost);
                fill_deviation_field(matrix, trials, field);

                fold_onto_residue(field, folded);
                for (unsigned at = 0u; at < 32u; at += 1u)
                {
                    fprintf(handle, "%s,residue,%u,0,%u,%.4f\n", sources[which].name, at, at_round, folded[at]);
                }

                for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
                {
                    double along = 0.0;
                    for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
                    {
                        const double here = field[((size_t)row * OUTPUT_BITS) + column];
                        along += here * here;
                    }
                    fprintf(handle, "%s,inbit,%u,0,%u,%.2f\n", sources[which].name, row, at_round,
                                 along - (double)OUTPUT_BITS);
                }

                for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
                {
                    double along = 0.0;
                    for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
                    {
                        const double here = field[((size_t)row * OUTPUT_BITS) + column];
                        along += here * here;
                    }
                    fprintf(handle, "%s,outbit,%u,0,%u,%.2f\n", sources[which].name, column, at_round,
                                 along - (double)INPUT_BITS);
                }
            }
            printf("  %-14s done\n", sources[which].name);
        }
        fclose(handle);
        cudaFree(device_matrix);

        printf("\n  Written to sources.csv. Render with examples/00_blob_viz_tools/build_sources_view.py\n");
        return 0;
    }

    if (shadow_only)
    {
        // ---------------------------------------------------------------------------------------
        // The object, and its shadows.
        //
        // Everything in this file is a projection. The fold onto residue is the object flattened along one axis, and
        // the chi-square is the object flattened all the way to a scalar. Both are shadows, and a shadow only ever
        // comes from one angle. Nothing here looks at the thing from a second one.
        //
        // The object is three-dimensional: 512 input bits by 256 output bits by 64 rounds. That is 8.4 million cells,
        // and not something that anyone holds in their head. The arm therefore dumps it as its projections, and not
        // whole, and the projections are what a viewer can stand on.
        //
        // The arm writes four shadows, each the object collapsed along a different axis:
        //   - residue by round: the signed fold, where the ridge lives.
        //   - inbit by round: every input bit against depth, unfolded.
        //   - outbit by round: every output bit against depth.
        //   - word by word: input word against output word against depth, the coarse block view.
        //
        // Signed where a sign exists, because magnitudes first would fold noise into a positive bias, and manufacture a
        // shape in every cell.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  The object, dumped as its shadows\n");
        printf("================================================================\n");

        std::vector<double> field((size_t)INPUT_BITS * OUTPUT_BITS);
        double folded[32];

        const unsigned DEEPEST = 64u;
        std::vector<double> residue_by_round((size_t)32u * DEEPEST, 0.0);
        std::vector<double> inbit_by_round((size_t)INPUT_BITS * DEEPEST, 0.0);
        std::vector<double> outbit_by_round((size_t)OUTPUT_BITS * DEEPEST, 0.0);
        std::vector<double> word_by_word((size_t)16u * 8u * DEEPEST, 0.0);

        printf("\n  Filling %u rounds. Each is its own seed, so the noise between rounds is\n", DEEPEST);
        printf("  independent and any shape that crosses rounds is the object instead of a\n");
        printf("  fluctuation carried along.\n");

        for (unsigned at_round = 1u; at_round <= DEEPEST; at_round += 1u)
        {
            const unsigned slot = at_round - 1u;
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_sac_row<<<INPUT_BITS, 256>>>(trials, 929292001ull + ((uint64_t)at_round * 1000003ull), at_round,
                                              device_matrix, KEEP_EVERYTHING);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);
            fill_deviation_field(matrix, trials, field);

            fold_onto_residue(field, folded);
            for (unsigned at = 0u; at < 32u; at += 1u)
            {
                residue_by_round[((size_t)at * DEEPEST) + slot] = folded[at];
            }

            for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
            {
                double along = 0.0;
                for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
                {
                    const double here = field[((size_t)row * OUTPUT_BITS) + column];
                    along += here * here;
                }
                // Excess over the expected one per cell, so that a flat row reads zero instead of 256.
                inbit_by_round[((size_t)row * DEEPEST) + slot] = along - (double)OUTPUT_BITS;
            }

            for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
            {
                double along = 0.0;
                for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
                {
                    const double here = field[((size_t)row * OUTPUT_BITS) + column];
                    along += here * here;
                }
                outbit_by_round[((size_t)column * DEEPEST) + slot] = along - (double)INPUT_BITS;
            }

            for (unsigned in_word = 0u; in_word < 16u; in_word += 1u)
            {
                for (unsigned out_word = 0u; out_word < 8u; out_word += 1u)
                {
                    double along = 0.0;
                    for (unsigned bit = 0u; bit < 32u; bit += 1u)
                    {
                        const unsigned row = (in_word * 32u) + bit;
                        for (unsigned other = 0u; other < 32u; other += 1u)
                        {
                            const unsigned column = (out_word * 32u) + other;
                            const double here = field[((size_t)row * OUTPUT_BITS) + column];
                            along += here * here;
                        }
                    }
                    const size_t where = ((((size_t)in_word * 8u) + out_word) * DEEPEST) + slot;
                    word_by_word[where] = along - 1024.0;
                }
            }
        }

        cudaFree(device_matrix);

        std::FILE *const handle = fopen("shadows.csv", "w");
        if (handle == nullptr)
        {
            printf("\n  could not open shadows.csv\n");
            return 1;
        }
        fprintf(handle, "type,a,b,round,value\n");
        for (unsigned at = 0u; at < 32u; at += 1u)
        {
            for (unsigned slot = 0u; slot < DEEPEST; slot += 1u)
            {
                fprintf(handle, "residue,%u,0,%u,%.4f\n", at, slot + 1u,
                             residue_by_round[((size_t)at * DEEPEST) + slot]);
            }
        }
        for (unsigned row = 0u; row < INPUT_BITS; row += 1u)
        {
            for (unsigned slot = 0u; slot < DEEPEST; slot += 1u)
            {
                fprintf(handle, "inbit,%u,0,%u,%.2f\n", row, slot + 1u,
                             inbit_by_round[((size_t)row * DEEPEST) + slot]);
            }
        }
        for (unsigned column = 0u; column < OUTPUT_BITS; column += 1u)
        {
            for (unsigned slot = 0u; slot < DEEPEST; slot += 1u)
            {
                fprintf(handle, "outbit,%u,0,%u,%.2f\n", column, slot + 1u,
                             outbit_by_round[((size_t)column * DEEPEST) + slot]);
            }
        }
        for (unsigned in_word = 0u; in_word < 16u; in_word += 1u)
        {
            for (unsigned out_word = 0u; out_word < 8u; out_word += 1u)
            {
                for (unsigned slot = 0u; slot < DEEPEST; slot += 1u)
                {
                    const size_t where = ((((size_t)in_word * 8u) + out_word) * DEEPEST) + slot;
                    fprintf(handle, "word,%u,%u,%u,%.2f\n", in_word, out_word, slot + 1u, word_by_word[where]);
                }
            }
        }
        fclose(handle);

        printf("\n  Written to shadows.csv: %u residue rows, %u input bits, %u output bits\n", 32u, INPUT_BITS,
                    OUTPUT_BITS);
        printf("  and %u word pairs, each across %u rounds.\n", 16u * 8u, DEEPEST);
        return 0;
    }

    if (dispersion_only)
    {
        // ---------------------------------------------------------------------------------------
        // The dispersion relation, where there is still something to measure.
        //
        // The Doppler scan asks one question of the whole field: is anything moving? This asks a finer one of each
        // frequency separately. It asks at rounds 10 to 23 instead of past 23, because only there does this function
        // still have structure to fingerprint.
        //
        // A fit of a complex exponential to Z_f(r) gives two numbers per frequency:
        //   - drift: the phase advance per round, which is where that component goes.
        //   - decay: the amplitude fall per round, the speed at which the function elides it.
        //
        // Together they are a complex frequency. The shape of drift against frequency identifies a mechanism, and does
        // not merely detect one. A rigid rotation drifts at the same velocity at every frequency. A shear drifts as one
        // over frequency. A diffusive process holds no drift at all as frequency rises. The known 1/32 amplitude law is
        // one point on this curve, and the rest of it has no published measurement.
        //
        // What it can carry is position, and not value. The phase of a component is the residue offset that it sits at,
        // and this therefore locates structure. Nothing here recovers the value of any bit, and the table below makes
        // no such claim.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  The dispersion relation, rounds 10 to 23\n");
        printf("================================================================\n");

        std::vector<double> field((size_t)INPUT_BITS * OUTPUT_BITS);
        double folded[32];

        const unsigned FROM = 10u;
        const unsigned WALK = 14u;
        const unsigned TONES = 16u;

        // Ten standard errors. Under the null, a spectrum line has unit-normal parts. This is therefore far enough
        // above the floor that the log of the amplitude is still the log of a signal.
        const double FLOOR_AT = 10.0;

        printf("\n  Fitting Z_f(r) across %u rounds, each on its own seed. Drift is the phase\n", WALK);
        printf("  advance per round turned into residues per round; decay is the amplitude\n");
        printf("  fall per round. Position is where the component sits, which phase carries\n");
        printf("  and magnitude does not.\n");

        for (unsigned source = 0u; source < 2u; source += 1u)
        {
            std::vector<double> line_real((size_t)TONES * WALK, 0.0);
            std::vector<double> line_imaginary((size_t)TONES * WALK, 0.0);
            const char *label = (source == 0u) ? "walking wave, control" : "SHA-256";

            for (unsigned step = 0u; step < WALK; step += 1u)
            {
                const unsigned at_round = FROM + step;
                const uint64_t seed = 818181001ull + ((uint64_t)at_round * 1000003ull);

                cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
                if (source == 0u)
                {
                    // A wave of fixed amplitude that walks one residue per round. The fit must return drift 1 and decay
                    // 0. Otherwise the fit is wrong, and the SHA-256 row means nothing.
                    fill_wave_row<<<INPUT_BITS, 256>>>(trials, seed, device_matrix, 3u, 9u, step % 32u);
                }
                else
                {
                    fill_sac_row<<<INPUT_BITS, 256>>>(trials, seed, at_round, device_matrix, KEEP_EVERYTHING);
                }
                cudaDeviceSynchronize();
                cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                           cudaMemcpyDeviceToHost);
                fill_deviation_field(matrix, trials, field);
                fold_onto_residue(field, folded);

                for (unsigned tone = 1u; tone < TONES; tone += 1u)
                {
                    double real_part = 0.0;
                    double imaginary = 0.0;
                    residue_spectrum_line(folded, tone, &real_part, &imaginary);
                    line_real[((size_t)tone * WALK) + step] = real_part;
                    line_imaginary[((size_t)tone * WALK) + step] = imaginary;
                }
            }

            printf("\n  %s\n", label);
            printf("  %6s %12s %12s %12s %12s %8s %8s\n", "freq", "amplitude", "position", "drift", "decay/round",
                        "fitted", "holds");
            printf("  %6s %12s %12s %12s %12s %8s %8s\n", "------", "------------", "------------", "------------",
                        "------------", "--------", "--------");

            for (unsigned tone = 1u; tone < TONES; tone += 1u)
            {
                // Phase advance with no unwrap: the argument of the sum of each round against the conjugate of the one
                // before it. The sum weights by amplitude by construction, which is correct when the later rounds are
                // fainter.
                double turned_real = 0.0;
                double turned_imaginary = 0.0;
                for (unsigned at = 0u; (at + 1u) < WALK; at += 1u)
                {
                    const double next_real = line_real[((size_t)tone * WALK) + at + 1u];
                    const double next_imaginary = line_imaginary[((size_t)tone * WALK) + at + 1u];
                    const double here_real = line_real[((size_t)tone * WALK) + at];
                    const double here_imaginary = line_imaginary[((size_t)tone * WALK) + at];
                    turned_real += (next_real * here_real) + (next_imaginary * here_imaginary);
                    turned_imaginary += (next_imaginary * here_real) - (next_real * here_imaginary);
                }
                const double advance = atan2(turned_imaginary, turned_real);
                const double drift = (advance * 32.0) / (6.283185307179586 * (double)tone);

                // Decay from a straight-line fit of the log amplitude. The table also reports the amplitude at the
                // first round, so that the reader can see whether the fit had anything to work with.
                double sum_at = 0.0;
                double sum_log = 0.0;
                double sum_at_log = 0.0;
                double sum_at_at = 0.0;
                double counted = 0.0;
                double first_size = 0.0;
                for (unsigned at = 0u; at < WALK; at += 1u)
                {
                    const double here_real = line_real[((size_t)tone * WALK) + at];
                    const double here_imaginary = line_imaginary[((size_t)tone * WALK) + at];
                    const double size = sqrt((here_real * here_real) + (here_imaginary * here_imaginary));
                    if (at == 0u)
                    {
                        first_size = size;
                    }
                    // Only points clear of the noise floor. A component that falls into the noise stops its fall. An
                    // inclusion of those rounds measures where the floor is, and not how fast the thing decays. It does
                    // so worst for the faintest components, which have least room to fall.
                    //
                    // Without this, the fitted decay correlates with the initial amplitude at r = 0.76 over fifteen
                    // frequencies: the confound in a lab coat. It reads as "every scale dies at the same rate", and
                    // really says "everything ends up at the same floor".
                    if (size <= FLOOR_AT)
                    {
                        continue;
                    }
                    const double when = (double)at;
                    sum_at += when;
                    sum_log += log(size);
                    sum_at_log += when * log(size);
                    sum_at_at += when * when;
                    counted += 1.0;
                }
                double decay = 0.0;
                if (counted > 1.0)
                {
                    const double spread = (counted * sum_at_at) - (sum_at * sum_at);
                    if (spread != 0.0)
                    {
                        decay = ((counted * sum_at_log) - (sum_at * sum_log)) / spread;
                    }
                }

                const double where = line_imaginary[((size_t)tone * WALK)];
                const double whence = line_real[((size_t)tone * WALK)];
                double position = (atan2(where, whence) * 32.0) / (6.283185307179586 * (double)tone);
                const double wrap = 32.0 / (double)tone;
                while (position < 0.0)
                {
                    position += wrap;
                }

                // A drift is only a measurement where the component was loud enough to carry a phase. Below that, the
                // argument of noise is uniform, and the number is furniture. A decay wants points to fit, and a drift
                // wants amplitude to carry a phase. The table reports both, so that a row that says nothing visibly
                // says nothing.
                const char *holds = ((first_size > 20.0) && (counted >= 4.0)) ? "yes" : "-";
                printf("  %6u %12.1f %12.3f %12.3f %12.4f %8.0f %8s\n", tone, first_size, position, drift, decay,
                            counted, holds);
            }
        }

        printf("\n  The control must read drift 1.000 and decay near zero at frequency 3, or\n");
        printf("  the fit is wrong and the SHA-256 table above means nothing. Where a row is\n");
        printf("  marked '-' the component was too faint to carry a phase and its drift is the\n");
        printf("  argument of noise, which is uniform.\n");

        cudaFree(device_matrix);
        return 0;
    }

    if (waves_only)
    {
        // ---------------------------------------------------------------------------------------
        // Waves in, and whether the field is smooth.
        //
        // Every reading before this one collapses 131072 cells to a scalar, and discards where the cells are. Two
        // fields with the same sum of squares can be white noise or a smooth landscape, and only the second matters. A
        // hash has no business with a spatially correlated residual, because correlation between adjacent cells is
        // structure that survived the mix.
        //
        // The claim needs a null, as every other claim here does. Three fields therefore go through identical code:
        //   - the harness alone: a matrix binomial by construction, which is whatever the random stream, the atomics
        //     and the arithmetic contribute by themselves.
        //   - a known wave: the same harness with a plane wave of stated amplitude pushed in. It says what the readout
        //     does when there is something to find.
        //   - SHA-256: the real matrix past the collapse.
        //
        // Smoothness that appears in the third and not the first is SHA-256's. Smoothness that appears in both is the
        // harness's, and that still deserves a record.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  Waves in, and whether the field is smooth\n");
        printf("================================================================\n");

        std::vector<double> field((size_t)INPUT_BITS * OUTPUT_BITS);
        double folded[32];
        const double white_error = 1.0 / sqrt((double)(INPUT_BITS * OUTPUT_BITS));

        printf("\n  A white field correlates with itself at zero for every nonzero lag, with a\n");
        printf("  standard error of %.6f on %u pairs. Anything standing clear of that is a\n", white_error,
                    INPUT_BITS * OUTPUT_BITS);
        printf("  field with a length scale, which is what smooth means when it is measured.\n");

        // -- The calibration ladder ------------------------------------------------------------
        //
        // A known wave at a stated amplitude, faded until the readout loses it. That measures the sensitivity of this
        // instrument, and assumes nothing. It turns a null from "nothing found" into "nothing above this amplitude, at
        // five sigmas".
        const unsigned WAVE_FREQUENCY = 3u;
        const unsigned FAINTEST = 16u;

        printf("\n  Calibration: a plane wave at frequency %u across the residues, faded until\n", WAVE_FREQUENCY);
        printf("  the readout loses it. Amplitude is the bias driven into each cell's rate.\n");
        printf("\n  %10s %14s %16s %12s %10s %10s\n", "amplitude", "excess chi2", "chi2 sigmas", "loudest",
                    "at freq", "found");
        printf("  %10s %14s %16s %12s %10s %10s\n", "----------", "--------------", "----------------",
                    "------------", "----------", "----------");

        for (unsigned strength = 8u; strength <= FAINTEST; strength += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_wave_row<<<INPUT_BITS, 256>>>(trials, 606060001ull + (strength * 7919ull), device_matrix,
                                               WAVE_FREQUENCY, strength, 0u);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);
            fill_deviation_field(matrix, trials, field);

            double total = 0.0;
            for (size_t at = 0u; at < field.size(); at += 1u)
            {
                total += field[at] * field[at];
            }
            const double excess = total - (double)(INPUT_BITS * OUTPUT_BITS);
            const double chi_sigmas = excess / sqrt(2.0 * (double)(INPUT_BITS * OUTPUT_BITS));

            fold_onto_residue(field, folded);
            unsigned tone_at = 0u;
            const double tone = loudest_residue_tone(folded, &tone_at);

            const double amplitude = pow(0.5, (double)(strength + 1u));
            const char *found = ((tone_at == WAVE_FREQUENCY) && (tone > 5.0)) ? "yes" : "-";
            printf("  %10.2e %14.1f %16.2f %12.2f %10u %10s\n", amplitude, excess, chi_sigmas, tone, tone_at,
                        found);
        }

        // -- The three fields, side by side ------------------------------------------------------
        const unsigned LAGS = 8u;
        printf("\n  Correlation of the field with itself, shifted. Zero is white.\n");
        printf("\n  %-22s %8s", "field", "axis");
        for (unsigned lag = 1u; lag <= LAGS; lag += 1u)
        {
            printf(" %8u", lag);
        }
        printf("\n");
        printf("  %-22s %8s", "----------------------", "--------");
        for (unsigned lag = 1u; lag <= LAGS; lag += 1u)
        {
            printf(" %8s", "--------");
        }
        printf("\n");

        for (unsigned which = 0u; which < 4u; which += 1u)
        {
            const char *label = "";
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            if (which == 0u)
            {
                label = "harness alone";
                fill_null_row<<<INPUT_BITS, 256>>>(trials, 424242001ull, device_matrix, 0u);
            }
            else if (which == 1u)
            {
                label = "harness + wave 2^-11";
                fill_wave_row<<<INPUT_BITS, 256>>>(trials, 424242001ull, device_matrix, WAVE_FREQUENCY, 10u, 0u);
            }
            else if (which == 2u)
            {
                label = "SHA-256 round 32";
                fill_sac_row<<<INPUT_BITS, 256>>>(trials, 424242001ull, 32u, device_matrix, KEEP_EVERYTHING);
            }
            else
            {
                label = "SHA-256 round 64";
                fill_sac_row<<<INPUT_BITS, 256>>>(trials, 424242001ull, 64u, device_matrix, KEEP_EVERYTHING);
            }
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);
            fill_deviation_field(matrix, trials, field);

            printf("  %-22s %8s", label, "output");
            for (unsigned lag = 1u; lag <= LAGS; lag += 1u)
            {
                printf(" %8.2f", correlate_along_output(field, lag) / white_error);
            }
            printf("\n");

            printf("  %-22s %8s", "", "input");
            for (unsigned lag = 1u; lag <= LAGS; lag += 1u)
            {
                printf(" %8.2f", correlate_along_input(field, lag) / white_error);
            }
            printf("\n");
        }

        printf("\n  Every number above is in standard errors of a white field. The wave row is\n");
        printf("  the positive control: it must light up, or the readout cannot see anything\n");
        printf("  and the other rows mean nothing. The harness row is the null: whatever it\n");
        printf("  carries is the instrument's own, and SHA-256 only claims what stands above\n");
        printf("  it instead of above zero.\n");

        // ---------------------------------------------------------------------------------------
        // Doppler: what moves, visible at rest or not.
        //
        // Every reading up to here looks at one round at a time, and asks whether anything stands above the noise in
        // that round. Something too faint to clear the floor in any single round is invisible to all of them, however
        // many rounds they look at. They compare the rounds one at a time, and then throw them away.
        //
        // The rounds are not independent samples of an unknown thing. They are a deterministic sequence. The round
        // function, and not chance, relates whatever structure exists at round r to the structure at r+1. The phase of
        // the residue spectrum carries that relation, and every earlier reading discarded it with a magnitude.
        //
        // A structure at rest holds its phase from round to round. A structure that drifts at v residues per round
        // advances it by 2*pi*f*v/32 each round. Noise does neither. Sum the complex spectrum across rounds against a
        // velocity hypothesis. A real structure then adds coherently, as R, and noise incoherently, as sqrt(R). That is
        // a gain of sqrt(R) over any single round, and only what actually moves at that velocity gets it.
        //
        // Each round takes its own seed, and this works because of that. The structure is deterministic, and stays
        // coherent across rounds whatever the seed. The seeds make the sampling noise independent, and it decoheres.
        // One shared seed would keep the noise coherent too, and the gain would be nothing.
        // ---------------------------------------------------------------------------------------
        printf("\n================================================================\n");
        printf("  Doppler: what is moving, seen or not\n");
        printf("================================================================\n");

        const unsigned FIRST_ROUND = 23u;
        const unsigned SPAN = 42u;
        const unsigned TONES = 16u;
        const unsigned STEPS = 129u;
        const unsigned DRIFTS = 320u;
        const double SLOWEST = -8.0;
        const double FASTEST = 8.0;

        printf("\n  Rounds %u to %u, each on its own seed. A structure standing still holds its\n", FIRST_ROUND,
                    (FIRST_ROUND + SPAN) - 1u);
        printf("  phase, one drifting advances it, noise does neither. Summing coherently over\n");
        printf("  %u rounds gains sqrt(%u) = %.2f on whatever moves at the tried velocity.\n", SPAN, SPAN,
                    sqrt((double)SPAN));

        // Three sources through the same scan. The harness alone is the null, a wave that walks one residue per round
        // is the positive control, and the third is SHA-256.
        for (unsigned source = 0u; source < 3u; source += 1u)
        {
            std::vector<double> line_real((size_t)TONES * SPAN, 0.0);
            std::vector<double> line_imaginary((size_t)TONES * SPAN, 0.0);
            const char *label = "";

            for (unsigned step = 0u; step < SPAN; step += 1u)
            {
                const unsigned at_round = FIRST_ROUND + step;
                const uint64_t seed = 515151001ull + ((uint64_t)at_round * 1000003ull);

                cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
                if (source == 0u)
                {
                    label = "harness alone";
                    fill_null_row<<<INPUT_BITS, 256>>>(trials, seed, device_matrix, 0u);
                }
                else if (source == 1u)
                {
                    // Amplitude 2^-14, which the ladder above shows is below the floor of a single round. No single
                    // round can show this wave. It moves one residue per round, and the scan must earn it from the
                    // motion, or not at all.
                    label = "wave 2^-14 walking 1/round";
                    fill_wave_row<<<INPUT_BITS, 256>>>(trials, seed, device_matrix, WAVE_FREQUENCY, 13u, step % 32u);
                }
                else
                {
                    label = "SHA-256";
                    fill_sac_row<<<INPUT_BITS, 256>>>(trials, seed, at_round, device_matrix, KEEP_EVERYTHING);
                }
                cudaDeviceSynchronize();
                cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                           cudaMemcpyDeviceToHost);
                fill_deviation_field(matrix, trials, field);
                fold_onto_residue(field, folded);

                for (unsigned tone = 1u; tone < TONES; tone += 1u)
                {
                    double real_part = 0.0;
                    double imaginary = 0.0;
                    residue_spectrum_line(folded, tone, &real_part, &imaginary);
                    line_real[((size_t)tone * SPAN) + step] = real_part;
                    line_imaginary[((size_t)tone * SPAN) + step] = imaginary;
                }
            }

            double loudest = 0.0;
            unsigned loud_tone = 0u;
            double loud_speed = 0.0;
            double standing_best = 0.0;

            // The phase-only channel, which asks a different question from the one above.
            //
            // A sum of the spectrum weighted by its own magnitude lets a few loud rounds carry the total, and magnitude
            // is where the noise lives. A round with a large fluctuation contributes large, whatever its phase does. A
            // normalization of every round to unit length first discards the amplitude, and keeps only the direction.
            // The question is then whether the phases line up, and not whether anything is big.
            //
            // A structure that is faint but perfectly coherent has a linear phase ramp, and no amplitude to speak of.
            // It is invisible to the magnitude channel by construction, and plain to this one.
            double tightest = 0.0;
            unsigned tight_tone = 0u;
            double tight_speed = 0.0;

            // The scan runs in drift, which is frequency times velocity, and not in velocity.
            //
            // The phase that a round advances is 2*pi*(f*v)*r/32. The coordinate that the readout actually lives in is
            // therefore f*v, and v is a degenerate one. Every frequency's cone passes through v = 0 together, and away
            // from it, the same step in v is a step of f times as much phase. A uniform grid in v is therefore correct
            // at one frequency and wrong at the rest. At f = 15, the step is 1.875 in drift, where 42 rounds resolve
            // 0.195. That is ten times too coarse, and could step straight over a real peak.
            //
            // A uniform scan of drift is the same map with the degenerate point resolved. It is one grid that is
            // correct at every frequency at once, with velocity recovered as drift over frequency where a reader needs
            // it.
            for (unsigned tone = 1u; tone < TONES; tone += 1u)
            {
                for (unsigned step = 0u; step < DRIFTS; step += 1u)
                {
                    const double drift = -16.0 + ((32.0 * (double)step) / (double)DRIFTS);
                    double summed_real = 0.0;
                    double summed_imaginary = 0.0;
                    double turned_real = 0.0;
                    double turned_imaginary = 0.0;
                    for (unsigned at = 0u; at < SPAN; at += 1u)
                    {
                        const double turn = 6.283185307179586 * drift * (double)at / 32.0;
                        const double here_real = line_real[((size_t)tone * SPAN) + at];
                        const double here_imaginary = line_imaginary[((size_t)tone * SPAN) + at];
                        const double rotated_real = (here_real * cos(turn)) + (here_imaginary * sin(turn));
                        const double rotated_imaginary =
                            (here_imaginary * cos(turn)) - (here_real * sin(turn));
                        summed_real += rotated_real;
                        summed_imaginary += rotated_imaginary;

                        const double length =
                            sqrt((rotated_real * rotated_real) + (rotated_imaginary * rotated_imaginary));
                        if (length > 0.0)
                        {
                            turned_real += rotated_real / length;
                            turned_imaginary += rotated_imaginary / length;
                        }
                    }
                    const double magnitude =
                        sqrt(((summed_real * summed_real) + (summed_imaginary * summed_imaginary)) / (double)SPAN);
                    if (magnitude > loudest)
                    {
                        loudest = magnitude;
                        loud_tone = tone;
                        loud_speed = drift / (double)tone;
                    }
                    if ((drift == 0.0) && (magnitude > standing_best))
                    {
                        standing_best = magnitude;
                    }

                    // Unit vectors summed. Under the null, the phases are uniform, and each part has variance SPAN/2.
                    // This reads as a Rayleigh magnitude with unit parts, on the same scale as the magnitude channel.
                    const double concentration = sqrt(
                        ((turned_real * turned_real) + (turned_imaginary * turned_imaginary)) / ((double)SPAN / 2.0));
                    if (concentration > tightest)
                    {
                        tightest = concentration;
                        tight_tone = tone;
                        tight_speed = drift / (double)tone;
                    }
                }
            }

            // The scan takes a maximum over every tone and every velocity, and it peaks under the null whatever the
            // data does. A complex sum with unit-normal parts is Rayleigh, and its maximum over N cells sits near
            // sqrt(2 ln N). The test applies that correction before it reads the number, and not after. The test reads
            // four channels, and each is itself a maximum. A comparison of any one of them to the peak of a single
            // channel is the max-of-N error, one level up. With four maxima, the correction must cover the union, and
            // not the largest of them.
            //
            // The window scan dominates the count by a long way. A slide of the start and the width multiplies the
            // grid, and does not add to it.
            double windows_searched = 0.0;
            {
                const unsigned counted[3] = {8u, 14u, 21u};
                for (unsigned width_at = 0u; width_at < 3u; width_at += 1u)
                {
                    const unsigned starts = (SPAN - counted[width_at]) + 1u;
                    windows_searched += (double)starts * (double)(((STEPS + 3u) / 4u)) * (double)(TONES - 1u);
                }
            }
            const double cells = (double)((TONES - 1u) * DRIFTS);
            const double every_cell = (cells * 2.0) + (double)STEPS + windows_searched;
            const double peaks_at = sqrt(2.0 * log(every_cell));

            // A sum of R unit vectors cannot exceed R. The phase channel therefore has an upper bound of sqrt(2R)
            // whatever the data does: 9.17 at forty-two rounds. Against a null peak near 4.7, that is barely a factor
            // of two of range. It is a weak instrument at this baseline, however good the idea is. The table reports
            // it, so that the ceiling is visible.
            const double phase_ceiling = sqrt(2.0 * (double)SPAN);

            // -- Radius instead of a single cone -------------------------------------------
            //
            // The scan above takes the loudest single (frequency, velocity) cell, which is a matched filter for a pure
            // sinusoid that drifts at a constant rate. A localized feature is not a sinusoid: it carries harmonics at
            // 2f, 3f and so on, and all of them ride at the same velocity. A pool of every frequency at one velocity is
            // the matched filter for that instead. It is strictly better on anything localized, for the same reason
            // that the residue fold beats the chi-square on a residue-structured wave.
            //
            // Geometrically, this sweeps the radius from the center of the map, and asks where the cones intersect, and
            // does not follow one cone out.
            double loudest_radius = 0.0;
            double radius_speed = 0.0;

            for (unsigned step = 0u; step < STEPS; step += 1u)
            {
                const double speed = SLOWEST + (((FASTEST - SLOWEST) * (double)step) / (double)(STEPS - 1u));
                double pooled = 0.0;
                for (unsigned tone = 1u; tone < TONES; tone += 1u)
                {
                    double summed_real = 0.0;
                    double summed_imaginary = 0.0;
                    for (unsigned at = 0u; at < SPAN; at += 1u)
                    {
                        const double turn = 6.283185307179586 * (double)tone * speed * (double)at / 32.0;
                        const double here_real = line_real[((size_t)tone * SPAN) + at];
                        const double here_imaginary = line_imaginary[((size_t)tone * SPAN) + at];
                        summed_real += (here_real * cos(turn)) + (here_imaginary * sin(turn));
                        summed_imaginary += (here_imaginary * cos(turn)) - (here_real * sin(turn));
                    }
                    pooled += ((summed_real * summed_real) + (summed_imaginary * summed_imaginary)) / (double)SPAN;
                }
                if (pooled > loudest_radius)
                {
                    loudest_radius = pooled;
                    radius_speed = speed;
                }
            }

            // Fifteen frequencies, two unit-normal parts each. The pooled power is therefore chi-square with thirty
            // degrees of freedom: mean 30, spread sqrt(60). The harness row is the honest null for it regardless, and
            // the SHA-256 row reads against it.
            const double pooled_sigmas = (loudest_radius - 30.0) / sqrt(60.0);

            // -- Every position of the sphere ------------------------------------------------
            //
            // An integral over the whole span dilutes a structure that lives in a window. A slide of the start and a
            // shorter window are the third axis. The shorter windows get their check with the knowledge that the
            // maximum over more cells peaks higher. The comparison is against the harness, scanned identically, and not
            // against zero.
            double windowed_best = 0.0;
            unsigned windowed_start = 0u;
            unsigned windowed_span = 0u;

            const unsigned WIDTHS = 3u;
            const unsigned widths[WIDTHS] = {8u, 14u, 21u};
            for (unsigned width_at = 0u; width_at < WIDTHS; width_at += 1u)
            {
                const unsigned width = widths[width_at];
                for (unsigned start = 0u; (start + width) <= SPAN; start += 1u)
                {
                    for (unsigned step = 0u; step < STEPS; step += 4u)
                    {
                        const double speed = SLOWEST + (((FASTEST - SLOWEST) * (double)step) / (double)(STEPS - 1u));
                        for (unsigned tone = 1u; tone < TONES; tone += 1u)
                        {
                            double summed_real = 0.0;
                            double summed_imaginary = 0.0;
                            for (unsigned at = 0u; at < width; at += 1u)
                            {
                                const unsigned where = start + at;
                                const double turn = 6.283185307179586 * (double)tone * speed * (double)where / 32.0;
                                const double here_real = line_real[((size_t)tone * SPAN) + where];
                                const double here_imaginary = line_imaginary[((size_t)tone * SPAN) + where];
                                summed_real += (here_real * cos(turn)) + (here_imaginary * sin(turn));
                                summed_imaginary += (here_imaginary * cos(turn)) - (here_real * sin(turn));
                            }
                            const double magnitude = sqrt(
                                ((summed_real * summed_real) + (summed_imaginary * summed_imaginary)) / (double)width);
                            if (magnitude > windowed_best)
                            {
                                windowed_best = magnitude;
                                windowed_start = FIRST_ROUND + start;
                                windowed_span = width;
                            }
                        }
                    }
                }
            }

            printf("\n  %s\n", label);
            printf("    loudest         %8.2f at frequency %u, velocity %+.3f residues/round\n", loudest,
                        loud_tone, loud_speed);
            printf("    phase only      %8.2f at frequency %u, velocity %+.3f  (amplitude\n", tightest, tight_tone,
                        tight_speed);
            printf("                             thrown away; ceiling is %.2f at %u rounds)\n", phase_ceiling,
                        SPAN);
            printf("    pooled radius   %8.2f at velocity %+.3f  (%.2f sigmas of chi2(30))\n", loudest_radius,
                        radius_speed, pooled_sigmas);
            printf("    best window     %8.2f over rounds %u to %u\n", windowed_best, windowed_start,
                        (windowed_start + windowed_span) - 1u);
            printf("    standing still  %8.2f  (velocity zero, the best any single-round\n", standing_best);
            printf("                             reading could ever pool to)\n");
            printf("    null peak       %8.2f  (maximum over %.0f cells, all four channels)\n", peaks_at,
                        every_cell);

            double largest = loudest;
            if (tightest > largest)
            {
                largest = tightest;
            }
            if (windowed_best > largest)
            {
                largest = windowed_best;
            }
            printf("    %s\n", (largest > peaks_at) ? "ABOVE the null peak: something is moving."
                                                         : "under the null peak: nothing is moving that this can see.");
        }

        printf("\n  The walking wave is the positive control and is set below the standing\n");
        printf("  floor on purpose: it cannot be found in any single round, so if the scan\n");
        printf("  recovers it, the gain is real and came from the motion. If it does not, this\n");
        printf("  instrument is blind and the SHA-256 line below it means nothing.\n");

        cudaFree(device_matrix);
        return 0;
    }

    printf("\n  %8s %14s %14s %12s %10s\n", "round", "SAC value", "from half", "sigmas", "verdict");
    printf("  %8s %14s %14s %12s %10s\n", "--------", "--------------", "--------------", "------------",
                "----------");

    for (unsigned rounds = first_round; rounds <= last_round; rounds += 1u)
    {
        cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
        fill_sac_row<<<INPUT_BITS, 256>>>(trials, 20260909ull + (rounds * 7919ull), rounds, device_matrix,
                                          KEEP_EVERYTHING);
        cudaDeviceSynchronize();
        cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long), cudaMemcpyDeviceToHost);

        // Their SAC value: whichever entry of the matrix sits furthest from one half, the strictest single number that
        // the matrix supports.
        double furthest = 0.5;
        double widest = 0.0;
        for (size_t at = 0u; at < matrix.size(); at += 1u)
        {
            const double rate = (double)matrix[at] / (double)trials;
            const double apart = (rate < 0.5) ? (0.5 - rate) : (rate - 0.5);
            if (apart > widest)
            {
                widest = apart;
                furthest = rate;
            }
        }

        const double sigmas = widest / deviation;

        // The matrix holds 131072 entries. The largest deviation among them therefore peaks near sqrt(2 ln 131072) =
        // 4.85 standard deviations, with or without anything there. Anything at or under that is the null, however
        // small the probability of one entry alone.
        const double null_peak = sqrt(2.0 * log((double)matrix.size()));
        const char *verdict = (sigmas > (null_peak + 2.0)) ? "OFF" : "flat";
        printf("  %8u %14.8f %14.8f %12.1f %10s\n", rounds, furthest, widest, sigmas, verdict);
    }

    // ---------------------------------------------------------------------------------------
    // The one claim that does not fall to the maximum argument.
    //
    // Vaughn and Borowczak report that Choose and Majority together give a SAC-mean ratio of 0.508. The two
    // sub-functions jointly deliver about half the diffusion that each provides alone. That is a *mean* over the whole
    // matrix, and not a maximum of it. The null-peak argument against the 54-57 dip therefore says nothing about it,
    // and it deserves a measurement on its own terms.
    //
    // Their composition model treats each sub-function as an independent flip of an output bit. Two of them then flip
    // it when exactly one does: Expect(A,B) = A + B - 2AB. A ratio of measured to expected below one is damping, and
    // above one is amplification.
    //
    // It is also the claim most worth a test. Choose and Majority are the two functions that feed the a half of the
    // chain. In bench_words, the depths per position of the a half are irregular, and the e half is clean at k + 3
    // exactly.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  Do Choose and Majority dampen each other\n");
    printf("================================================================\n");
    printf("\n  The published SAC-mean ratio for this pair is 0.508, meaning the two together\n");
    printf("  deliver about half the diffusion each gives alone. That is a mean over the whole\n");
    printf("  matrix instead of a maximum of it, so the argument that disposes of the 54-57\n");
    printf("  dip does not touch it.\n");
    printf("\n  Their model has each sub-function flipping a bit independently, so two of them\n");
    printf("  flip it when exactly one does: Expect(A,B) = A + B - 2AB.\n");

    {
        const unsigned VARIANTS = 3u;
        const unsigned masks[VARIANTS] = {KEEP_CHOOSE, KEEP_MAJORITY, KEEP_CHOOSE | KEEP_MAJORITY};
        const char *named[VARIANTS] = {"Choose alone", "Majority alone", "both together"};
        const unsigned at_round = 64u;

        double means[VARIANTS];
        printf("\n  %-18s %16s\n", "variant", "SAC-mean");
        printf("  %-18s %16s\n", "------------------", "----------------");

        for (unsigned which = 0u; which < VARIANTS; which += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_sac_row<<<INPUT_BITS, 256>>>(trials, 20260909ull, at_round, device_matrix, masks[which]);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            double total = 0.0;
            for (size_t at = 0u; at < matrix.size(); at += 1u)
            {
                total += (double)matrix[at] / (double)trials;
            }
            means[which] = total / (double)matrix.size();
            printf("  %-18s %16.8f\n", named[which], means[which]);
        }

        const double expected = means[0] + means[1] - (2.0 * means[0] * means[1]);
        const double ratio = (expected > 0.0) ? (means[2] / expected) : 0.0;
        printf("\n  %-34s %16.8f\n", "expected from the two alone", expected);
        printf("  %-34s %16.8f\n", "measured together", means[2]);
        printf("  %-34s %16.4f\n", "ratio", ratio);
        printf("  %-34s %16.4f\n", "published ratio", 0.508);

        const char *verdict =
            (ratio < 0.75) ? "dampening reproduces" : ((ratio > 1.25) ? "amplifies instead" : "neither");
        printf("  %-34s %16s\n", "verdict", verdict);

        printf("\n  A ratio near the published 0.508 reproduces the dampening and makes these\n");
        printf("  two functions worth attacking together instead of separately. A ratio near\n");
        printf("  one says the composition model is doing the work instead of SHA-256, since\n");
        printf("  that model assumes independence which two functions reading overlapping\n");
        printf("  state words do not have.\n");
    }

    // ---------------------------------------------------------------------------------------
    // A null with a known answer, free from the variant machinery.
    //
    // Every null in this work up to here is statistical: a distribution that the measurement goes against. The removal
    // of integer addition gives something better, an analytic one. Without the carry, the whole function is
    // GF(2)-linear, and a one-bit input difference produces a *deterministic* output difference, the same one for every
    // message. Every matrix entry must then be exactly 0 or exactly 1, and never one half. No number of rounds can
    // change that.
    //
    // The variant without addition must therefore never diffuse, at any round, by construction and not by measurement.
    // An instrument that reports it as diffused is broken. One that reports it pinned at a deviation of one half reads
    // correctly. This is the first control here with an answer known before the run.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  A null whose answer is known before the run\n");
    printf("================================================================\n");
    printf("\n  Removing integer addition leaves the function GF(2)-linear, so a one-bit input\n");
    printf("  difference gives a deterministic output difference - the same one for every\n");
    printf("  message. Every matrix entry is then exactly 0 or exactly 1, never one half, and\n");
    printf("  the deviation is pinned at 0.5 forever whatever the round.\n");
    printf("\n  That is an analytic control instead of a statistical one. Every other null in\n");
    printf("  this document is a distribution to compare against; this one has an answer.\n");

    {
        const unsigned CHECKED = 4u;
        const unsigned rounds_at[CHECKED] = {8u, 23u, 40u, 64u};

        printf("\n  %8s %20s %20s\n", "round", "full function", "linear variant");
        printf("  %8s %20s %20s\n", "--------", "--------------------", "--------------------");

        int carry_holds = 1;
        for (unsigned which = 0u; which < CHECKED; which += 1u)
        {
            // A removal of only the carry does not give a linear function: Choose and Majority use AND, and stay
            // nonlinear over GF(2) whatever addition does. The linear variant has all three gone, and keeps rotations,
            // exclusive-or and constants. A constant is affine: it cancels in a difference, and does not spoil the
            // linearity.
            double reading[2];
            const unsigned masks[2] = {KEEP_EVERYTHING, KEEP_SCHEDULE | KEEP_BIG_ZERO | KEEP_BIG_ONE | KEEP_CONSTANT};

            for (unsigned arm = 0u; arm < 2u; arm += 1u)
            {
                cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
                fill_sac_row<<<INPUT_BITS, 256>>>(trials, 20260909ull, rounds_at[which], device_matrix, masks[arm]);
                cudaDeviceSynchronize();
                cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                           cudaMemcpyDeviceToHost);

                double widest = 0.0;
                for (size_t at = 0u; at < matrix.size(); at += 1u)
                {
                    const double rate = (double)matrix[at] / (double)trials;
                    const double apart = (rate < 0.5) ? (0.5 - rate) : (rate - 0.5);
                    widest = (apart > widest) ? apart : widest;
                }
                reading[arm] = widest;
            }

            // The linear arm must read exactly one half. Anything less means a matrix entry landed
            // strictly between 0 and 1, which a deterministic difference cannot produce.
            if (reading[1] < 0.4999999)
            {
                carry_holds = 0;
            }
            printf("  %8u %20.8f %20.8f\n", rounds_at[which], reading[0], reading[1]);
        }

        if (carry_holds != 0)
        {
            printf("\n  The linear arm reads exactly 0.5 at every round, which is what a\n");
            printf("  deterministic difference must give and confirms the instrument reads a\n");
            printf("  known answer correctly.\n");
            printf("\n  What it does not show is that the carry causes the diffusion. Removing\n");
            printf("  addition alone leaves Choose and Majority, which use AND and stay\n");
            printf("  nonlinear, and that variant still reaches 0.0093 by round 64 - the same\n");
            printf("  plateau as the full function. So the nonlinearity is what makes diffusion\n");
            printf("  happen at all, and the carry changes how fast instead of whether. That\n");
            printf("  matches the published finding that removing addition costs about two\n");
            printf("  rounds instead of stopping the function diffusing.\n");
        }
        else
        {
            printf("\n  [!] The linear arm did not read exactly 0.5. A GF(2)-linear function\n");
            printf("  cannot produce a matrix entry strictly between 0 and 1, so either the\n");
            printf("  variant is not actually linear or the measurement is wrong. Nothing else\n");
            printf("  in this bench should be believed until that is settled.\n");
        }
    }

    // ---------------------------------------------------------------------------------------
    // Diffusion speed, which is a different observable from diffusion level.
    //
    // Everything above reads where a variant *arrives*. The ratio from round to round reads how fast it travels, and
    // the two need not order the same way. A variant can start later and move quicker, and the level measurement cannot
    // see that.
    //
    // The linear variant makes this a controlled measurement, and not a descriptive one. Its speed is analytically zero
    // at every round: the deviation stays at 0.5, and cannot move. A ratio of exactly 1.000 is therefore the known
    // null, and the departure of any variant from it is the distance that variant actually travels.
    //
    // The arm uses the mean deviation, and not the largest. The largest is a maximum over 131072 entries, and sits at
    // its own null peak whatever the function does. A speed from maxima would read that peak as it wanders from round
    // to round.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  Diffusion speed, against a null whose speed is exactly zero\n");
    printf("================================================================\n");
    printf("\n  Everything above reads where a variant arrives. This reads how fast it travels,\n");
    printf("  and the two need not order the same way - a variant can start later and move\n");
    printf("  quicker, which a level measurement cannot see.\n");
    printf("\n  The linear variant's speed is analytically zero: its deviation is pinned at 0.5\n");
    printf("  and cannot move, so a ratio of exactly 1.000 is the known null.\n");
    printf("\n  Mean deviation, not the largest, because a maximum over 131072 entries sits at\n");
    printf("  its own null peak whatever the function does and its wander is not speed.\n");

    {
        const unsigned ARMS = 5u;
        const unsigned arm_mask[ARMS] = {KEEP_EVERYTHING, KEEP_EVERYTHING & ~KEEP_ADDITION,
                                         KEEP_EVERYTHING & ~KEEP_CHOOSE, KEEP_EVERYTHING & ~KEEP_MAJORITY,
                                         KEEP_SCHEDULE | KEEP_BIG_ZERO | KEEP_BIG_ONE | KEEP_CONSTANT};
        const char *arm_name[ARMS] = {"full", "no carry", "no Choose", "no Majority", "linear"};

        const unsigned FIRST_SPEED = 17u;
        const unsigned LAST_SPEED = 24u;

        printf("\n  Mean deviation from one half, by round\n");
        printf("\n  %8s", "round");
        for (unsigned arm = 0u; arm < ARMS; arm += 1u)
        {
            printf(" %13s", arm_name[arm]);
        }
        printf("\n  %8s", "--------");
        for (unsigned arm = 0u; arm < ARMS; arm += 1u)
        {
            printf(" %13s", "-------------");
        }
        printf("\n");

        double held[ARMS][32];
        for (unsigned rounds = FIRST_SPEED; rounds <= LAST_SPEED; rounds += 1u)
        {
            printf("  %8u", rounds);
            for (unsigned arm = 0u; arm < ARMS; arm += 1u)
            {
                cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
                fill_sac_row<<<INPUT_BITS, 256>>>(trials, 20260909ull, rounds, device_matrix, arm_mask[arm]);
                cudaDeviceSynchronize();
                cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                           cudaMemcpyDeviceToHost);

                double total = 0.0;
                for (size_t at = 0u; at < matrix.size(); at += 1u)
                {
                    const double rate = (double)matrix[at] / (double)trials;
                    total += (rate < 0.5) ? (0.5 - rate) : (rate - 0.5);
                }
                held[arm][rounds - FIRST_SPEED] = total / (double)matrix.size();
                printf(" %13.8f", held[arm][rounds - FIRST_SPEED]);
            }
            printf("\n");
        }

        printf("\n  Speed, as the ratio of one round's deviation to the next. Above one is\n");
        printf("  travelling; exactly one is standing still.\n");
        printf("\n  %8s", "round");
        for (unsigned arm = 0u; arm < ARMS; arm += 1u)
        {
            printf(" %13s", arm_name[arm]);
        }
        printf("\n  %8s", "--------");
        for (unsigned arm = 0u; arm < ARMS; arm += 1u)
        {
            printf(" %13s", "-------------");
        }
        printf("\n");

        for (unsigned step = 1u; step <= (LAST_SPEED - FIRST_SPEED); step += 1u)
        {
            printf("  %5u->%2u", FIRST_SPEED + step - 1u, FIRST_SPEED + step);
            for (unsigned arm = 0u; arm < ARMS; arm += 1u)
            {
                const double before = held[arm][step - 1u];
                const double after = held[arm][step];
                const double speed = (after > 0.0) ? (before / after) : 0.0;
                printf(" %13.4f", speed);
            }
            printf("\n");
        }

        printf("\n  The linear column must read 1.0000 throughout, since its deviation cannot\n");
        printf("  move. Any other column's excess over that is the speed the sub-functions in\n");
        printf("  it actually contribute, measured against a baseline that is known rather\n");
        printf("  than estimated.\n");
    }

    // ---------------------------------------------------------------------------------------
    // What is Majority for?
    //
    // Three separate diffusion statistics say that Majority contributes nothing measurable: the SAC level, the damping
    // ratio, and the speed from round to round. Both published papers agree. A sub-function in a standard that does
    // nothing detectable is either redundant, or does something that the instrument does not measure. Algebraic degree
    // is the obvious candidate, because degree resists algebraic attack, and not differential probes.
    //
    // Degree has a proof, and needs no sample. A Boolean function of degree below d has every d-th order derivative
    // identically zero. That derivative is the exclusive-or of the output over all corners of a d-dimensional input
    // cube. A fold that comes out nonzero proves the degree is at least d, with no threshold and no sample size.
    //
    // The arm uses several bases per measurement. A single unlucky base can give an impossible order, such as dimension
    // 8 that reaches further than dimension 10, with a correct kernel.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  What is Majority for\n");
    printf("================================================================\n");
    printf("\n  Three diffusion statistics now agree that Majority contributes nothing\n");
    printf("  measurable, and both published papers agree. A sub-function that does nothing\n");
    printf("  detectable is either redundant or doing something else, and algebraic degree is\n");
    printf("  the candidate: degree resists algebraic attack instead of differential probing.\n");
    printf("\n  Degree is proved, not sampled. A function of degree below d has every d-th\n");
    printf("  order derivative zero, and that derivative is the exclusive-or of the output over\n");
    printf("  a d-dimensional input cube. One nonzero fold settles it.\n");

    {
        // A removal of one nonlinear source at a time cannot discriminate, because the others cover for it. Full,
        // no-Majority and no-Choose all give the same degree. An arm that keeps exactly one isolates what that
        // operation can do alone, and that is the question.
        //
        // Every arm keeps the linear spine: the schedule, both Sigmas and the constant. The only difference between the
        // arms is which single nonlinear operation is present.
        const unsigned SPINE = KEEP_SCHEDULE | KEEP_BIG_ZERO | KEEP_BIG_ONE | KEEP_CONSTANT;
        const unsigned ARMS = 5u;
        const unsigned arm_mask[ARMS] = {KEEP_EVERYTHING, SPINE | KEEP_MAJORITY, SPINE | KEEP_CHOOSE,
                                         SPINE | KEEP_ADDITION, SPINE};
        const char *arm_name[ARMS] = {"full", "Maj only", "Choose only", "carry only", "linear"};
        const unsigned MOST_DIMENSION = 18u;
        const unsigned BASES = 6u;

        uint32_t *device_base = nullptr;
        unsigned *device_positions = nullptr;
        uint32_t *device_folded = nullptr;
        cudaMalloc((void **)&device_base, 16u * sizeof(uint32_t));
        cudaMalloc((void **)&device_positions, MOST_DIMENSION * sizeof(unsigned));
        cudaMalloc((void **)&device_folded, 8u * sizeof(uint32_t));

        printf("\n  Degree lower bound, the smallest dimension whose fold is nonzero\n");
        printf("\n  %8s", "rounds");
        for (unsigned arm = 0u; arm < ARMS; arm += 1u)
        {
            printf(" %14s", arm_name[arm]);
        }
        printf("\n  %8s", "--------");
        for (unsigned arm = 0u; arm < ARMS; arm += 1u)
        {
            printf(" %14s", "--------------");
        }
        printf("\n");

        for (unsigned rounds = 2u; rounds <= 7u; rounds += 1u)
        {
            printf("  %8u", rounds);
            for (unsigned arm = 0u; arm < ARMS; arm += 1u)
            {
                // The derivative vanishes *above* the degree, and not below it. The answer is therefore the largest
                // dimension that still folds nonzero. A search for the smallest nonzero one would return 1 for every
                // non-constant function.
                unsigned found = 0u;
                for (unsigned dimension = 1u; dimension <= MOST_DIMENSION; dimension += 1u)
                {
                    int any_nonzero = 0;
                    for (unsigned base_at = 0u; base_at < BASES; base_at += 1u)
                    {
                        uint32_t base[16];
                        unsigned positions[MOST_DIMENSION];
                        uint64_t stream = 20260909ull + (base_at * 104729ull) + (rounds * 31ull);
                        for (unsigned slot = 0u; slot < 16u; slot += 1u)
                        {
                            stream += 0x9e3779b97f4a7c15ull;
                            uint64_t mixed = stream;
                            mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ull;
                            mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebull;
                            base[slot] = (uint32_t)(mixed ^ (mixed >> 31));
                        }
                        // Distinct bit positions, drawn from the first message words so the cube
                        // sits where the nonce would.
                        for (unsigned bit = 0u; bit < dimension; bit += 1u)
                        {
                            positions[bit] = ((base_at * 37u) + (bit * 13u)) % 128u;
                        }

                        cudaMemcpy(device_base, base, sizeof(base), cudaMemcpyHostToDevice);
                        cudaMemcpy(device_positions, positions, dimension * sizeof(unsigned), cudaMemcpyHostToDevice);
                        cudaMemset(device_folded, 0, 8u * sizeof(uint32_t));

                        const unsigned blocks = (unsigned)(((1ull << dimension) + 255ull) / 256ull);
                        fold_cube<<<(blocks > 0u) ? blocks : 1u, 256>>>(device_base, device_positions, dimension,
                                                                        rounds, arm_mask[arm], device_folded);
                        cudaDeviceSynchronize();

                        uint32_t folded[8];
                        cudaMemcpy(folded, device_folded, sizeof(folded), cudaMemcpyDeviceToHost);
                        for (unsigned slot = 0u; slot < 8u; slot += 1u)
                        {
                            any_nonzero |= (folded[slot] != 0u) ? 1 : 0;
                        }
                        if (any_nonzero != 0)
                        {
                            break;
                        }
                    }
                    // A nonzero fold at this dimension proves the degree is at least this high. The search records it
                    // and goes on. The first dimension where every base folds to zero is above the degree, and the
                    // search stops there.
                    if (any_nonzero != 0)
                    {
                        found = dimension;
                    }
                    else
                    {
                        break;
                    }
                }
                if (found >= MOST_DIMENSION)
                {
                    printf(" %14s", ">= 18");
                }
                else
                {
                    printf(" %14u", found);
                }
            }
            printf("\n");
        }

        cudaFree(device_base);
        cudaFree(device_positions);
        cudaFree(device_folded);

        printf("\n  A no-Majority column matching the full one says Majority adds no degree\n");
        printf("  either, and that it is redundant on every measure this tree can make. A\n");
        printf("  column reading lower says Majority is carrying degree instead of diffusion,\n");
        printf("  which is a real job and would explain why it is in the standard while\n");
        printf("  contributing nothing a diffusion statistic can see.\n");
    }

    // ---------------------------------------------------------------------------------------
    // The other axis of the same bucket.
    //
    // Everything measured so far is differential: what a *change* in the input does. Linear cryptanalysis asks a
    // different question: whether the output value *equals* some function of the input more often than chance. A
    // primitive can be strong against one and weak against the other. A bucket labeled "nonlinear" that only a
    // differential probe examined is half unexamined. Majority reads inert on four differential statistics, and that is
    // the case where the other half deserves a look.
    //
    // Isolation again, and not removal, because removal cannot discriminate. The operations mix, and a removal of one
    // leaves the others to cover for it, and every arm reads the same.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  The linear axis, which the differential tests cannot see\n");
    printf("================================================================\n");
    printf("\n  Everything above is differential: what a change in the input does. This asks\n");
    printf("  whether an output bit agrees with an input bit more often than chance, which is\n");
    printf("  what linear cryptanalysis attacks. A function can be strong against one and weak\n");
    printf("  against the other.\n");
    printf("\n  Isolated instead of removed, because the operations mix: taking one out leaves\n");
    printf("  the others covering for it, which is why every removal arm read the same degree.\n");

    {
        const unsigned SPINE = KEEP_SCHEDULE | KEEP_BIG_ZERO | KEEP_BIG_ONE | KEEP_CONSTANT;
        const unsigned ARMS = 5u;
        const unsigned arm_mask[ARMS] = {KEEP_EVERYTHING, SPINE | KEEP_MAJORITY, SPINE | KEEP_CHOOSE,
                                         SPINE | KEEP_ADDITION, SPINE};
        const char *arm_name[ARMS] = {"full", "Maj only", "Choose only", "carry only", "linear"};

        unsigned long long *device_agree = nullptr;
        cudaMalloc((void **)&device_agree, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
        std::vector<unsigned long long> agree((size_t)INPUT_BITS * OUTPUT_BITS);

        printf("\n  Largest linear correlation, |2p-1| over the 512 by 256 masks\n");
        printf("\n  %8s", "rounds");
        for (unsigned arm = 0u; arm < ARMS; arm += 1u)
        {
            printf(" %13s", arm_name[arm]);
        }
        printf("\n  %8s", "--------");
        for (unsigned arm = 0u; arm < ARMS; arm += 1u)
        {
            printf(" %13s", "-------------");
        }
        printf("\n");

        for (unsigned rounds = 4u; rounds <= 24u; rounds += 4u)
        {
            printf("  %8u", rounds);
            for (unsigned arm = 0u; arm < ARMS; arm += 1u)
            {
                cudaMemset(device_agree, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
                fill_linear_row<<<INPUT_BITS, 256>>>(trials, 20260909ull, rounds, arm_mask[arm], device_agree);
                cudaDeviceSynchronize();
                cudaMemcpy(agree.data(), device_agree, agree.size() * sizeof(unsigned long long),
                           cudaMemcpyDeviceToHost);

                double loudest = 0.0;
                for (size_t at = 0u; at < agree.size(); at += 1u)
                {
                    const double rate = (double)agree[at] / (double)trials;
                    const double correlation = (rate * 2.0) - 1.0;
                    const double size = (correlation < 0.0) ? -correlation : correlation;
                    loudest = (size > loudest) ? size : loudest;
                }
                printf(" %13.8f", loudest);
            }
            printf("\n");
        }

        cudaFree(device_agree);

        printf("\n  The linear arm should sit near 1.0, since a GF(2)-linear function's output\n");
        printf("  bit is an exclusive-or of input bits and agrees with one of them perfectly\n");
        printf("  or not at all. A correlation that falls with rounds is the operation in that\n");
        printf("  arm destroying linear structure, and one that stays high is an operation not\n");
        printf("  doing that job whatever it does differentially.\n");
    }

    // ---------------------------------------------------------------------------------------
    // The same matrix, read spectrally, and not by its maximum.
    //
    // Every SAC reading above takes the largest of 131072 cells, and discards the other 131071. That makes it peak at
    // sqrt(2 ln N) whatever the function does. A peak of that type can pass for a leak in SHA-256, when it is only the
    // maximum at work.
    //
    // A structure spread thinly across many cells is invisible to a maximum, and obvious to a sum. SHA-256 is full of
    // rotations: 2, 13, 22 and 6, 11, 25 in the state, and 7, 18, 17, 19 in the schedule. A rotation makes bit j depend
    // on bit j-r. If any of that survives into the dependency matrix, it appears as a function of (input bit - output
    // bit) modulo 32. It spreads over every word pair, and concentrates nowhere.
    //
    // The arm therefore folds the cells onto that residue. Each class holds 512*256/32 = 4096 of them. That makes the
    // aggregate 64 times more sensitive than a single cell. It is unboundedly more sensitive than a maximum, which gets
    // no benefit from the other cells at all.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  The matrix read spectrally instead of by its maximum\n");
    printf("================================================================\n");
    printf("\n  Every reading above takes the largest of 131072 cells and discards the rest,\n");
    printf("  which is why it peaks at sqrt(2 ln N) whatever the function does. A structure\n");
    printf("  spread thinly across many cells is invisible to a maximum and plain to a sum.\n");
    printf("\n  SHA-256 is built from rotations, and a rotation makes bit j depend on bit j-r,\n");
    printf("  so surviving rotational structure appears as a function of (input bit - output\n");
    printf("  bit) mod 32 spread over every word pair. Folding onto that residue puts 4096\n");
    printf("  cells behind each measurement instead of one.\n");

    {
        // A sweep round by round through the window where the ridge lives, and not a sample at a few depths. If the
        // dominant residue *walks* by a fixed amount per round, that amount is the effective rotation that the
        // composition applies. A trace of it unwinds the operation as it runs. A residue that stays put is a fixed
        // point of the composition, which is a different and equally readable answer.
        const unsigned RESIDUES = 32u;
        const unsigned CHECKED = 14u;
        const unsigned rounds_at[CHECKED] = {12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 32u, 64u};

        // 4096 cells per residue class. The standard error of the aggregate is therefore the per-cell one divided by
        // 64.
        const double per_cell = 0.5 / sqrt((double)trials);
        const double per_class = per_cell / sqrt(4096.0);
        const double class_null = sqrt(2.0 * log((double)RESIDUES));

        printf("\n  Per-cell standard error %.8f, per-class %.8f, %u cells per class.\n", per_cell, per_class,
                    4096u);
        printf("  The largest of %u residues peaks near %.2f sigmas under the null.\n", RESIDUES, class_null);

        printf("\n  %8s %16s %14s %10s %12s %8s\n", "rounds", "loudest residue", "its sigmas", "null peak",
                    "verdict", "walked");
        printf("  %8s %16s %14s %10s %12s %8s\n", "--------", "----------------", "--------------", "----------",
                    "------------", "--------");

        unsigned last_residue = 0u;

        for (unsigned which = 0u; which < CHECKED; which += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_sac_row<<<INPUT_BITS, 256>>>(trials, 20260909ull, rounds_at[which], device_matrix, KEEP_EVERYTHING);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            double folded[RESIDUES];
            double counted_in[RESIDUES];
            for (unsigned at = 0u; at < RESIDUES; at += 1u)
            {
                folded[at] = 0.0;
                counted_in[at] = 0.0;
            }

            for (unsigned in_bit = 0u; in_bit < INPUT_BITS; in_bit += 1u)
            {
                for (unsigned out_bit = 0u; out_bit < OUTPUT_BITS; out_bit += 1u)
                {
                    const size_t at = ((size_t)in_bit * OUTPUT_BITS) + out_bit;
                    const double rate = (double)matrix[at] / (double)trials;

                    // Signed, and not absolute. A structure has a direction, and cancels only if it is not there.
                    // Magnitudes first would fold noise into a positive bias, and manufacture a signal in every class.
                    const unsigned residue = ((in_bit % 32u) + 32u - (out_bit % 32u)) % 32u;
                    folded[residue] += (rate - 0.5);
                    counted_in[residue] += 1.0;
                }
            }

            double loudest = 0.0;
            unsigned loud_residue = 0u;
            for (unsigned at = 0u; at < RESIDUES; at += 1u)
            {
                if (counted_in[at] <= 0.0)
                {
                    continue;
                }
                const double mean = folded[at] / counted_in[at];
                const double sigmas = (mean < 0.0) ? (-mean / per_class) : (mean / per_class);
                if (sigmas > loudest)
                {
                    loudest = sigmas;
                    loud_residue = at;
                }
            }

            const char *verdict = (loudest > (class_null + 2.0)) ? "STRUCTURE" : "flat";

            // How far the ridge moved since the previous round. A constant step is the effective rotation under track,
            // and a step of zero is a fixed point. A step that wanders means that the residue is noise and not a ridge,
            // and the flat rounds should show that.
            char walked[16];
            if ((which > 0u) && (loudest > (class_null + 2.0)))
            {
                const unsigned step = (loud_residue + 32u - last_residue) % 32u;
                snprintf(walked, sizeof(walked), "%+d", (step <= 16u) ? (int)step : ((int)step - 32));
            }
            else
            {
                snprintf(walked, sizeof(walked), "%s", "-");
            }
            last_residue = loud_residue;

            printf("  %8u %16u %14.2f %10.2f %12s %8s\n", rounds_at[which], loud_residue, loudest, class_null,
                        verdict, walked);

            // The loudest residue alone is the same error in miniature: one number from thirty-two. The table prints
            // the whole profile, so that peaks and valleys are both visible. It also takes the discrete transform, so
            // that a periodic trend shows as a frequency, and nobody must spot it by eye. A structure with period p
            // appears at frequency 32/p, and its harmonics appear at multiples of that.
            if (which == 2u)
            {
                printf("\n  Full residue profile at round %u, in sigmas\n", rounds_at[which]);
                printf("  ");
                for (unsigned at = 0u; at < RESIDUES; at += 1u)
                {
                    const double mean = folded[at] / counted_in[at];
                    printf("%+6.2f", mean / per_class);
                    if (((at + 1u) % 8u) == 0u)
                    {
                        printf("\n  ");
                    }
                }

                double loudest_tone = 0.0;
                unsigned loud_tone = 0u;
                printf("\n  Transform of that profile, magnitude by frequency\n  ");
                for (unsigned tone = 1u; tone <= (RESIDUES / 2u); tone += 1u)
                {
                    double real_part = 0.0;
                    double other_part = 0.0;
                    for (unsigned at = 0u; at < RESIDUES; at += 1u)
                    {
                        const double mean = (folded[at] / counted_in[at]) / per_class;
                        const double angle = (-6.283185307179586 * (double)tone * (double)at) / (double)RESIDUES;
                        real_part += mean * cos(angle);
                        other_part += mean * sin(angle);
                    }
                    const double size =
                        sqrt((real_part * real_part) + (other_part * other_part)) / sqrt((double)RESIDUES);
                    if (size > loudest_tone)
                    {
                        loudest_tone = size;
                        loud_tone = tone;
                    }
                    printf("f%02u %5.2f  ", tone, size);
                    if ((tone % 4u) == 0u)
                    {
                        printf("\n  ");
                    }
                }

                // A transform of white noise has magnitudes near one after this normalization. The largest of sixteen
                // peaks near sqrt(2 ln 16) = 2.35 by itself.
                const double tone_null = sqrt(2.0 * log((double)(RESIDUES / 2u)));
                printf("\n  loudest tone f%02u at %.2f, null peak %.2f, period %.1f residues\n", loud_tone,
                            loudest_tone, tone_null, (loud_tone > 0u) ? ((double)RESIDUES / (double)loud_tone) : 0.0);
                printf("  %s\n", (loudest_tone > (tone_null + 2.0)) ? "a repeating trend stands clear of the null"
                                                                         : "no repeating trend above the null");
            }
        }

        printf("\n  A residue standing clear of the null peak is rotational structure surviving\n");
        printf("  into the dependency matrix, and it would be invisible to the maximum this\n");
        printf("  document has been reading. A flat column says the rotations leave no trace\n");
        printf("  the fold can find, which is a stronger statement than the maximum could make\n");
        printf("  because it is made with 4096 cells behind each number instead of one.\n");
    }

    // ---------------------------------------------------------------------------------------
    // The particle transfer test.
    //
    // The fold above reports which residue is loudest: one number about a distribution of thirty-two. A view of the
    // profile as a mass distribution gives the two quantities of a transported particle: where it is, and how wide it
    // is.
    //
    // Residues are a circle, and the moments must therefore be circular. The mean of a distribution over (in - out) mod
    // 32 is an angle. The concentration R is one for a distribution that sits on a single residue, and zero for one
    // spread evenly over all of them.
    //
    // The two transport regimes are distinguishable, and predict different things:
    //   - ballistic: the center drifts at a constant rate per round, and R stays high.
    //   - diffusive: the center stays put and R falls, as the width grows with sqrt(rounds).
    //
    // The arm assumes neither, and the measurement says which. A center that does not move while R collapses is
    // diffusion with no drift at all. That would mean that the rotations cancel in aggregate, and transport nothing.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  Particle transfer: where the mass is and how wide\n");
    printf("================================================================\n");
    printf("\n  The fold reports the loudest residue, which is one number about a distribution\n");
    printf("  of thirty-two. Treating the profile as a mass distribution instead gives what a\n");
    printf("  transported particle has: a position and a width.\n");
    printf("\n  Residues are a circle so the moments are circular. R is one for mass on a\n");
    printf("  single residue and zero for mass spread evenly over all of them.\n");
    printf("\n  Ballistic transport drifts at a constant rate with R holding. Diffusive\n");
    printf("  transport stays put with R falling. The measurement says which.\n");

    {
        const unsigned RESIDUES = 32u;
        const unsigned FIRST_SEEN = 12u;
        const unsigned LAST_SEEN = 24u;

        printf("\n  %8s %14s %12s %12s %14s\n", "rounds", "center", "drift", "R", "mass");
        printf("  %8s %14s %12s %12s %14s\n", "--------", "--------------", "------------", "------------",
                    "--------------");

        double last_centre = 0.0;
        int have_last = 0;

        for (unsigned rounds = FIRST_SEEN; rounds <= LAST_SEEN; rounds += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_sac_row<<<INPUT_BITS, 256>>>(trials, 20260909ull, rounds, device_matrix, KEEP_EVERYTHING);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            // Magnitude instead of the signed value, because a mass distribution has no sign. The signed fold is the
            // right reading to find a ridge, and the wrong one to weigh it.
            double mass[RESIDUES];
            for (unsigned at = 0u; at < RESIDUES; at += 1u)
            {
                mass[at] = 0.0;
            }
            for (unsigned in_bit = 0u; in_bit < INPUT_BITS; in_bit += 1u)
            {
                for (unsigned out_bit = 0u; out_bit < OUTPUT_BITS; out_bit += 1u)
                {
                    const size_t at = ((size_t)in_bit * OUTPUT_BITS) + out_bit;
                    const double rate = (double)matrix[at] / (double)trials;
                    const double apart = (rate < 0.5) ? (0.5 - rate) : (rate - 0.5);
                    const unsigned residue = ((in_bit % 32u) + 32u - (out_bit % 32u)) % 32u;
                    mass[residue] += apart;
                }
            }

            double total = 0.0;
            double along = 0.0;
            double across = 0.0;
            for (unsigned at = 0u; at < RESIDUES; at += 1u)
            {
                const double angle = (6.283185307179586 * (double)at) / (double)RESIDUES;
                total += mass[at];
                along += mass[at] * cos(angle);
                across += mass[at] * sin(angle);
            }
            if (total <= 0.0)
            {
                continue;
            }

            const double concentration = sqrt((along * along) + (across * across)) / total;
            double center = atan2(across, along) * (double)RESIDUES / 6.283185307179586;
            if (center < 0.0)
            {
                center += (double)RESIDUES;
            }

            char drift[16];
            if (have_last != 0)
            {
                double moved = center - last_centre;
                while (moved > 16.0)
                {
                    moved -= 32.0;
                }
                while (moved < -16.0)
                {
                    moved += 32.0;
                }
                snprintf(drift, sizeof(drift), "%+.3f", moved);
            }
            else
            {
                snprintf(drift, sizeof(drift), "%s", "-");
            }
            last_centre = center;
            have_last = 1;

            printf("  %8u %14.3f %12s %12.6f %14.2f\n", rounds, center, drift, concentration, total);
        }

        printf("\n  A drift column holding one value is ballistic transport and names the rate.\n");
        printf("  A drift near zero with R falling is diffusion in place, which would mean the\n");
        printf("  rotations cancel in aggregate instead of carrying the mass anywhere. R\n");
        printf("  approaching zero is the particle spread evenly over every residue, which is\n");
        printf("  where a difference stops being locatable at all.\n");
    }

    // ---------------------------------------------------------------------------------------
    // Is the transport circular?
    //
    // Suppose the path of a bit through the function is the same as its neighbor's, shifted by one position. Then the
    // whole dependency matrix comes from one row and thirty-two rotations of it. Rotation-equivariance means exactly
    // that, and a signature would come from it: one path, reused at every offset.
    //
    // A direct check: take row i and row i+s, rotate the second back by s, and correlate. If the transport is circular,
    // the two agree. If the constants break the symmetry, they do not.
    //
    // SHA-256 has reason to fail this. Its rotations are circular, but its additions carry, and a carry has a direction
    // and an end: bit thirty-one carries into nothing. The arithmetic is therefore not rotation-equivariant, even
    // though the rotations are. How much that shows is the measurement.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  Is the transport circular\n");
    printf("================================================================\n");
    printf("\n  If a bit's path is its neighbor's shifted by one, the whole matrix is one row\n");
    printf("  and thirty-two rotations of it. That is where a signature would come from: one\n");
    printf("  path reused at every offset.\n");
    printf("\n  SHA-256 has reason to fail this. Its rotations are circular but its additions\n");
    printf("  carry, and a carry has a direction and an end - bit thirty-one carries into\n");
    printf("  nothing. The arithmetic is not equivariant even though the rotations are.\n");

    {
        const unsigned CHECKED = 6u;
        const unsigned rounds_at[CHECKED] = {8u, 12u, 16u, 20u, 23u, 64u};

        printf("\n  %8s %16s %16s %12s\n", "rounds", "equivariance", "shuffled control", "verdict");
        printf("  %8s %16s %16s %12s\n", "--------", "----------------", "----------------", "------------");

        for (unsigned which = 0u; which < CHECKED; which += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_sac_row<<<INPUT_BITS, 256>>>(trials, 20260909ull, rounds_at[which], device_matrix, KEEP_EVERYTHING);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            // Correlation of row i against row i+s rotated back by s, over every pair of input bits inside one word and
            // every shift. The rotation stays within the word, and does not cross the whole row, because a word
            // boundary is where the circularity would stop.
            double agree_total = 0.0;
            double control_total = 0.0;
            double counted_pairs = 0.0;

            for (unsigned word_in = 0u; word_in < 4u; word_in += 1u)
            {
                for (unsigned base = 0u; base < 32u; base += 1u)
                {
                    for (unsigned shift = 1u; shift < 32u; shift += 1u)
                    {
                        const unsigned first_bit = (word_in * 32u) + base;
                        const unsigned other_bit = (word_in * 32u) + ((base + shift) % 32u);

                        double first_sum = 0.0;
                        double other_sum = 0.0;
                        double cross = 0.0;
                        double first_square = 0.0;
                        double other_square = 0.0;
                        double control_cross = 0.0;

                        for (unsigned out_word = 0u; out_word < 8u; out_word += 1u)
                        {
                            for (unsigned out_bit = 0u; out_bit < 32u; out_bit += 1u)
                            {
                                const unsigned here = (out_word * 32u) + out_bit;
                                // The partner output bit, rotated by the same shift inside its word.
                                const unsigned there = (out_word * 32u) + ((out_bit + shift) % 32u);

                                const double one =
                                    ((double)matrix[((size_t)first_bit * OUTPUT_BITS) + here] / (double)trials) - 0.5;
                                const double two =
                                    ((double)matrix[((size_t)other_bit * OUTPUT_BITS) + there] / (double)trials) - 0.5;

                                // The control pairs the same two rows, and does not rotate the second. That is the same
                                // data with the claimed symmetry removed.
                                const double flat =
                                    ((double)matrix[((size_t)other_bit * OUTPUT_BITS) + here] / (double)trials) - 0.5;

                                first_sum += one;
                                other_sum += two;
                                cross += one * two;
                                control_cross += one * flat;
                                first_square += one * one;
                                other_square += two * two;
                            }
                        }

                        const double spread = sqrt(first_square * other_square);
                        if (spread <= 0.0)
                        {
                            continue;
                        }
                        agree_total += cross / spread;
                        control_total += control_cross / spread;
                        counted_pairs += 1.0;
                    }
                }
            }

            if (counted_pairs <= 0.0)
            {
                continue;
            }
            const double equivariance = agree_total / counted_pairs;
            const double control = control_total / counted_pairs;

            const char *verdict = (equivariance > (control + 0.05)) ? "CIRCULAR" : "not circular";
            printf("  %8u %16.6f %16.6f %12s\n", rounds_at[which], equivariance, control, verdict);
        }

        printf("\n  The control is the same two rows paired without the rotation, which is the\n");
        printf("  identical data with the claimed symmetry removed. Equivariance standing above\n");
        printf("  it is a path reused at every offset; the two agreeing is the rotation buying\n");
        printf("  nothing, which is what a carry that ends at bit thirty-one would do.\n");
    }

    // ---------------------------------------------------------------------------------------
    // Buried or destroyed.
    //
    // The polynomial part of SHA-256 is absolute. Sigma0 is x^2 + x^13 + x^22 in GF(2)[x]/(x^32 + 1), and that holds
    // independently of any message, any round and any run. It is a fact about the polynomial, and not about this
    // system. Its rank is thirty-two at round one and at round sixty-four, and the linear variant reads 0.50000000 at
    // both.
    //
    // The polynomial therefore never decays, and cannot. What decays is whether a prediction built from it still
    // describes the real function, and those are different statements. This measures the second one: how long the
    // carry-free prediction stays right.
    //
    // Agreement starts at one and falls to one half, which is chance for a bit. Where it reaches one half, the
    // polynomial description stops working, and the polynomial is still true.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  Buried or destroyed: how long the carry-free prediction holds\n");
    printf("================================================================\n");
    printf("\n  The polynomial part is absolute. Sigma0 is x^2 + x^13 + x^22 in\n");
    printf("  GF(2)[x]/(x^32+1) independently of any message, round or run, its rank is 32 at\n");
    printf("  round one and at round sixty-four, and the linear variant reads 0.50000000 at\n");
    printf("  both. It does not decay and cannot.\n");
    printf("\n  What decays is whether a prediction built from it still describes the real\n");
    printf("  function. Agreement of one is exact description; one half is chance.\n");

    {
        unsigned long long *device_agreed = nullptr;
        unsigned long long *device_compared = nullptr;
        unsigned long long *device_position = nullptr;
        cudaMalloc((void **)&device_agreed, sizeof(unsigned long long));
        cudaMalloc((void **)&device_compared, sizeof(unsigned long long));
        cudaMalloc((void **)&device_position, 32u * sizeof(unsigned long long));

        // The arm checks the inverse before it reads anything from it. Every direction figure below depends on
        // decompress_to as the inverse of compress_to. This check measures that, and assumes nothing.
        {
            unsigned long long *device_failed = nullptr;
            cudaMalloc((void **)&device_failed, sizeof(unsigned long long));
            printf("\n  Round trip, both variants, before anything is read from the inverse\n");
            printf("\n  %8s %16s\n", "rounds", "failed");
            printf("  %8s %16s\n", "--------", "----------------");

            unsigned long long any_failed = 0u;
            for (unsigned rounds = 1u; rounds <= 24u; rounds += 7u)
            {
                cudaMemset(device_failed, 0, sizeof(unsigned long long));
                round_trip_check<<<256, 128>>>(4096ull, 20260909ull, rounds, device_failed);
                cudaDeviceSynchronize();
                unsigned long long failed = 0u;
                cudaMemcpy(&failed, device_failed, sizeof(failed), cudaMemcpyDeviceToHost);
                any_failed += failed;
                printf("  %8u %16llu\n", rounds, (unsigned long long)failed);
            }
            cudaFree(device_failed);

            if (any_failed != 0u)
            {
                printf("\n  [!] The inverse does not invert. Every direction figure below is\n");
                printf("  meaningless and the anti-correlation it reports is this defect and\n");
                printf("  not a property of SHA-256.\n");
            }
            else
            {
                printf("\n  The inverse inverts exactly, in both variants, at every depth\n");
                printf("  checked. What follows is measuring the function instead of the\n");
                printf("  instrument.\n");
            }
        }

        printf("\n  A walk from wholly unnatural to wholly natural. One is a polynomial\n");
        printf("  describing the object exactly; zero is chance, where no polynomial helps.\n");
        printf("  Both directions are run, because the two are known to differ in depth and in\n");
        printf("  shape and the question is whether they differ in describability too.\n");

        printf("\n  %8s %14s %12s %14s %12s %10s\n", "rounds", "forward", "step", "inverse", "step", "apart");
        printf("  %8s %14s %12s %14s %12s %10s\n", "--------", "--------------", "------------", "--------------",
                    "------------", "----------");

        double last_seen[2] = {0.0, 0.0};
        for (unsigned rounds = 1u; rounds <= 24u; rounds += 1u)
        {
            double above[2];
            for (int backward = 0; backward < 2; backward += 1)
            {
                cudaMemset(device_agreed, 0, sizeof(unsigned long long));
                cudaMemset(device_compared, 0, sizeof(unsigned long long));
                carry_free_agreement<<<2048, 128>>>(trials, 20260909ull, rounds, device_agreed, device_compared,
                                                    backward, device_position);
                cudaDeviceSynchronize();

                unsigned long long agreed = 0u;
                unsigned long long compared = 0u;
                cudaMemcpy(&agreed, device_agreed, sizeof(agreed), cudaMemcpyDeviceToHost);
                cudaMemcpy(&compared, device_compared, sizeof(compared), cudaMemcpyDeviceToHost);
                above[backward] = (compared > 0u) ? ((((double)agreed / (double)compared) - 0.5) * 2.0) : 0.0;
            }

            char forward_step[16];
            char inverse_step[16];
            if (rounds > 1u)
            {
                snprintf(forward_step, sizeof(forward_step), "%+.4f", above[0] - last_seen[0]);
                snprintf(inverse_step, sizeof(inverse_step), "%+.4f", above[1] - last_seen[1]);
            }
            else
            {
                snprintf(forward_step, sizeof(forward_step), "%s", "-");
                snprintf(inverse_step, sizeof(inverse_step), "%s", "-");
            }
            last_seen[0] = above[0];
            last_seen[1] = above[1];

            printf("  %8u %14.8f %12s %14.8f %12s %10.4f\n", rounds, above[0], forward_step, above[1],
                        inverse_step, above[1] - above[0]);
        }

        // Is the wrap what makes the two directions differ? A carry runs upward from bit zero to bit thirty-one. Bit
        // zero has no carry-in, and the prediction must describe it exactly. Bit thirty-one carries the most
        // accumulated uncertainty, and then wraps into nothing. If the wrap is the cause, the two directions wear
        // different profiles across bit position. If not, they wear the same one, and the asymmetry is somewhere else.
        printf("\n  Describability by bit position, at the round each direction is halfway\n");
        printf("\n  %6s %14s %14s %12s\n", "bit", "forward r11", "inverse r6", "apart");
        printf("  %6s %14s %14s %12s\n", "------", "--------------", "--------------", "------------");

        double profile[2][32];
        const unsigned halfway[2] = {11u, 6u};
        for (int backward = 0; backward < 2; backward += 1)
        {
            cudaMemset(device_agreed, 0, sizeof(unsigned long long));
            cudaMemset(device_compared, 0, sizeof(unsigned long long));
            cudaMemset(device_position, 0, 32u * sizeof(unsigned long long));
            carry_free_agreement<<<2048, 128>>>(trials, 20260909ull, halfway[backward], device_agreed, device_compared,
                                                backward, device_position);
            cudaDeviceSynchronize();

            unsigned long long counts[32];
            unsigned long long compared = 0u;
            cudaMemcpy(counts, device_position, sizeof(counts), cudaMemcpyDeviceToHost);
            cudaMemcpy(&compared, device_compared, sizeof(compared), cudaMemcpyDeviceToHost);

            // The arm compares each position once per word per trial. The denominator is therefore the total compared
            // bits divided by the thirty-two positions.
            const double per_position = (double)compared / 32.0;
            for (unsigned bit = 0u; bit < 32u; bit += 1u)
            {
                profile[backward][bit] =
                    (per_position > 0.0) ? (((double)counts[bit] / per_position) - 0.5) * 2.0 : 0.0;
            }
        }

        for (unsigned bit = 0u; bit < 32u; bit += 1u)
        {
            printf("  %6u %14.6f %14.6f %12.6f\n", bit, profile[0][bit], profile[1][bit],
                        profile[1][bit] - profile[0][bit]);
        }

        printf("\n  Bit zero has no carry-in and should be described exactly in both\n");
        printf("  directions. A profile that climbs away from bit zero is the carry chain\n");
        printf("  accumulating, and the two directions differing in that climb is the wrap\n");
        printf("  being responsible for the asymmetry. The same climb in both is the wrap\n");
        printf("  being symmetric and the asymmetry living somewhere else.\n");

        cudaFree(device_agreed);
        cudaFree(device_compared);
        cudaFree(device_position);

        printf("\n  Agreement falling to one half is the description failing, and the round it\n");
        printf("  reaches it is the depth at which the polynomial stops being usable as a\n");
        printf("  prediction. The polynomial itself is unchanged there and is still exactly as\n");
        printf("  true as it was at round one, which is the difference between buried and\n");
        printf("  destroyed stated as a measurement instead of as a position.\n");
    }

    // ---------------------------------------------------------------------------------------
    // Stay put and use every cell.
    //
    // Every reading so far either takes a maximum, or folds onto residues. A maximum uses one cell of 131072, and
    // throws the rest away. A fold sums them into thirty-two bins, and can only see structure that lines up with the
    // fold. Neither uses all the evidence without an extreme, and a test that does is the most sensitive thing that
    // this matrix supports.
    //
    // The sum of squared z over all cells is chi-square with 131072 degrees of freedom under the null: mean 131072, and
    // standard deviation sqrt(2 * 131072) = 512. An excess of a few hundred is therefore detectable. That is a per-cell
    // bias near 0.06 standard errors, against the 4.85 that a maximum needs before it says anything. For structure
    // spread thinly across every cell, the test is roughly eighty times more sensitive than a maximum.
    //
    // A design that punishes wide samples would be least prepared for this test.
    // ---------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  Every cell, without extremising\n");
    printf("================================================================\n");
    printf("\n  A maximum uses one cell of 131072 and discards the rest. A fold sums them into\n");
    printf("  thirty-two bins and sees only what lines up with it. The sum of squared z uses\n");
    printf("  all of them and extremises nothing.\n");
    printf("\n  Under the null it is chi-square with 131072 degrees of freedom: mean 131072,\n");
    printf("  standard deviation 512. That detects a per-cell bias near 0.06 standard errors,\n");
    printf("  where a maximum needs 4.85 in one cell before it says anything.\n");

    {
        // The control first, because a statistic this sensitive is worth nothing until the instrument reads a known
        // zero with it. Forty-two draws of a matrix that is binomial by construction, through the same kernel shape,
        // tally, denominator and arithmetic. Whatever this reads is what the instrument adds. The number from the real
        // arm only means something after the arm subtracts it.
        const unsigned CONTROLS = 42u;
        const double cells_null = (double)(INPUT_BITS * OUTPUT_BITS);
        const double spread_null = sqrt(2.0 * cells_null);

        double control_excess = 0.0;
        double control_square = 0.0;

        for (unsigned draw = 0u; draw < CONTROLS; draw += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_null_row<<<INPUT_BITS, 256>>>(trials, 777000001ull + (draw * 2654435761ull), device_matrix, 0u);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            const double per_cell = 0.5 / sqrt((double)trials);
            double total = 0.0;
            for (size_t at = 0u; at < matrix.size(); at += 1u)
            {
                const double rate = (double)matrix[at] / (double)trials;
                const double score = (rate - 0.5) / per_cell;
                total += score * score;
            }
            const double excess = total - cells_null;
            control_excess += excess;
            control_square += excess * excess;
        }

        const double control_mean = control_excess / (double)CONTROLS;
        const double control_var = (control_square / (double)CONTROLS) - (control_mean * control_mean);
        const double control_sd = sqrt(control_var * (double)CONTROLS / (double)(CONTROLS - 1u));

        printf("\n  Control: %u draws of a matrix that is binomial by construction, through the\n", CONTROLS);
        printf("  same kernel and the same arithmetic.\n");
        printf("    mean excess     %12.1f  (analytic null says 0)\n", control_mean);
        printf("    spread of one   %12.1f  (analytic null says %.1f)\n", control_sd, spread_null);
        printf("    that mean is    %12.2f sigmas off zero, on the measured spread\n",
                    control_mean / (control_sd / sqrt((double)CONTROLS)));

        // The synthetic control says whether the arithmetic and the analytic 512 are right when the cells really are
        // independent. In the real arm they are not independent: all 131072 come from one message set. Its spread per
        // draw is therefore its own quantity, and needs a measurement in place of an assumption. Forty-two seeds at one
        // fixed depth past the collapse are that measurement, and the honest denominator for the pooled claim below.
        const unsigned SPREAD_AT = 32u;
        double real_excess = 0.0;
        double real_square = 0.0;

        for (unsigned draw = 0u; draw < CONTROLS; draw += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            fill_sac_row<<<INPUT_BITS, 256>>>(trials, 313000007ull + (draw * 2654435761ull), SPREAD_AT, device_matrix,
                                              KEEP_EVERYTHING);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            const double per_cell = 0.5 / sqrt((double)trials);
            double total = 0.0;
            for (size_t at = 0u; at < matrix.size(); at += 1u)
            {
                const double rate = (double)matrix[at] / (double)trials;
                const double score = (rate - 0.5) / per_cell;
                total += score * score;
            }
            const double excess = total - cells_null;
            real_excess += excess;
            real_square += excess * excess;
        }

        const double real_mean = real_excess / (double)CONTROLS;
        const double real_var = (real_square / (double)CONTROLS) - (real_mean * real_mean);
        const double real_sd = sqrt(real_var * (double)CONTROLS / (double)(CONTROLS - 1u));

        printf("\n  SHA-256 at round %u, %u seeds, to measure this statistic's own spread when\n", SPREAD_AT,
                    CONTROLS);
        printf("  the cells share one message set as they really do.\n");
        printf("    mean excess     %12.1f\n", real_mean);
        printf("    spread of one   %12.1f  (analytic null says %.1f, ratio %.2f)\n", real_sd, spread_null,
                    real_sd / spread_null);

        // Every round from twenty-three to sixty-four, and not a handful. Six depths give a standard error of 209,
        // where a mean of -323 is only -1.55 sigmas, and settles nothing. Forty-two depths put the error on the mean at
        // 79, where a real -323 would read past four sigmas, and a null would not move.
        //
        // A systematically negative chi-square is less variance than random, and not more. That is a stranger claim
        // than excess would be, and it wants the arithmetic before the story.
        const unsigned CHECKED = 43u;
        unsigned rounds_at[CHECKED];
        for (unsigned at = 0u; at < CHECKED; at += 1u)
        {
            rounds_at[at] = 22u + at;
        }
        const double cells = (double)(INPUT_BITS * OUTPUT_BITS);
        const double null_spread = sqrt(2.0 * cells);

        printf("\n  %8s %18s %14s %12s %12s\n", "rounds", "sum of z^2", "excess", "sigmas", "verdict");
        printf("  %8s %18s %14s %12s %12s\n", "--------", "------------------", "--------------", "------------",
                    "------------");

        double pooled_excess = 0.0;
        double pooled_count = 0.0;

        for (unsigned which = 0u; which < CHECKED; which += 1u)
        {
            cudaMemset(device_matrix, 0, (size_t)INPUT_BITS * OUTPUT_BITS * sizeof(unsigned long long));
            // A different stream per depth. One shared seed across depths means the same messages at different round
            // counts, and their excesses correlate. A pool of them, divided by the square root of the count, would
            // inflate the significance of whatever they share.
            fill_sac_row<<<INPUT_BITS, 256>>>(trials, 20260909ull + (rounds_at[which] * 1000003ull), rounds_at[which],
                                              device_matrix, KEEP_EVERYTHING);
            cudaDeviceSynchronize();
            cudaMemcpy(matrix.data(), device_matrix, matrix.size() * sizeof(unsigned long long),
                       cudaMemcpyDeviceToHost);

            const double per_cell = 0.5 / sqrt((double)trials);
            double total = 0.0;
            for (size_t at = 0u; at < matrix.size(); at += 1u)
            {
                const double rate = (double)matrix[at] / (double)trials;
                const double score = (rate - 0.5) / per_cell;
                total += score * score;
            }

            const double excess = total - cells;
            const double sigmas = excess / null_spread;
            const char *verdict = (sigmas > 5.0) ? "STRUCTURE" : "flat";
            printf("  %8u %18.1f %14.1f %12.2f %12s\n", rounds_at[which], total, excess, sigmas, verdict);

            // The arm pools the rounds past the collapse, because the question is whether their mean sits off zero, and
            // one depth cannot answer it.
            if (rounds_at[which] >= 23u)
            {
                pooled_excess += excess;
                pooled_count += 1.0;
            }
        }

        if (pooled_count > 1.0)
        {
            const double mean_excess = pooled_excess / pooled_count;
            const double mean_error = null_spread / sqrt(pooled_count);

            // Against the measured spread, and not the analytic one. The analytic 512 assumes independent cells, and
            // they are not: all 131072 come from one set of messages. The seed sweep at a fixed depth measures the
            // actual spread on this matrix, and it is the honest denominator.
            const double honest_error = real_sd / sqrt(pooled_count);
            const double against_control = mean_excess / honest_error;

            printf("\n  Pooled over %.0f depths past the collapse: mean excess %.1f.\n", pooled_count,
                        mean_excess);
            printf("    against the analytic null   %8.1f  ->  %6.2f sigmas\n", mean_error,
                        mean_excess / mean_error);
            printf("    against the measured null   %8.1f  ->  %6.2f sigmas\n", honest_error, against_control);
            printf("  %s\n",
                        (fabs(against_control) > 3.0)
                            ? "  Off zero against its own control: the matrix is not carrying binomial variance."
                            : "  On zero against its own control: the tendency is the instrument, not SHA-256.");
        }

        printf("\n  An excess standing clear of the null is structure spread across every cell,\n");
        printf("  invisible to a maximum and to a fold alike, and it would be the first thing\n");
        printf("  found past round twenty-three by anything here. A sum sitting on 131072 is\n");
        printf("  the matrix carrying nothing at all, measured the most sensitive way this\n");
        printf("  data allows instead of the most convenient.\n");
    }

    cudaFree(device_matrix);

    printf("\n  The matrix holds %u entries, so its largest deviation peaks near %.2f sigmas\n",
                INPUT_BITS * OUTPUT_BITS, sqrt(2.0 * log(131072.0)));
    printf("  whatever SHA-256 does - the strictest entry of a large matrix is a maximum and\n");
    printf("  behaves like one. A round reading near that is flat. A dip at 54 to 57 that is\n");
    printf("  real would stand far above it at this sample, and one that is a fluctuation of\n");
    printf("  their smaller sample will not appear at all.\n");
    return 0;
}
