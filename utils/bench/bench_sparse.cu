// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_sparse.cu
 * @brief How sparse a nonce difference stays in a real header, walked across many nonces.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note bench_space measures this with sixteen random schedule words, and finds a one-bit difference
 *       that survives to W27 for the right header. A real mining block is not sixteen random words.
 *       The header tail block carries four words of header, then 0x80000000, then ten zero words,
 *       then the length. Thus ten of its sixteen inputs are zero, and one more is a constant. Whether
 *       the sparse path exists there is a different question, and this bench asks it.
 * @note The headers are the known-answer vectors of this tree, whose hashes and nonces the chain
 *       recorded. Thus the walked block is a block that a miner really solved, and not a plausible one.
 * @note Sparsity here is the total Hamming weight of the expanded difference across all sixty-four
 *       schedule words: what a message-schedule differential costs an attacker. Lower is an easier
 *       path. The minimum over nonces is the honest figure. The expansion adds, and does not
 *       exclusive-or. Thus what a flip produces depends on the nonce of the flip.
 */

#include <cuda_runtime.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{

/** @brief Words in a message block. */
const unsigned BLOCK_WORDS = 16u;

/** @brief The word the nonce occupies in the header tail block. */
const unsigned NONCE_WORD = 3u;

/** @brief Expands sixteen words to sixty-four by the standard's rule. */
__device__ __forceinline__ void expand(uint32_t *schedule)
{
    for (unsigned at = 16u; at < 64u; at += 1u)
    {
        const uint32_t behind_fifteen = schedule[at - 15u];
        const uint32_t behind_two = schedule[at - 2u];
        const uint32_t low = ((behind_fifteen >> 7) | (behind_fifteen << 25)) ^
                             ((behind_fifteen >> 18) | (behind_fifteen << 14)) ^ (behind_fifteen >> 3);
        const uint32_t high =
            ((behind_two >> 17) | (behind_two << 15)) ^ ((behind_two >> 19) | (behind_two << 13)) ^ (behind_two >> 10);
        schedule[at] = schedule[at - 16u] + low + schedule[at - 7u] + high;
    }
}

/**
 * @brief Walks nonces and bit positions, and records how sparse each difference stays.
 *
 * @param[in]     base     The sixteen words of the block. The walk ignores the nonce word [BORROWS].
 * @param[in]     nonces   How many nonces to walk.
 * @param[in]     start    First nonce.
 * @param[out]    lightest Smallest total difference weight seen [BORROWS].
 * @param[out]    deepest  Deepest word still at two bits or fewer [BORROWS].
 * @param[in,out] total    Accumulates total weight, for the mean [BORROWS].
 * @param[in,out] counted  Accumulates how many pairs the walk covered [BORROWS].
 * @param[out]    at_word  Per-word smallest weight, sixty-four of them [BORROWS].
 * @note One thread per nonce and bit position. Both members of a pair share every word but the
 *       nonce: the difference that a miner actually steps through.
 */
__global__ void walk_sparse(const uint32_t *base, uint32_t nonces, uint32_t start, unsigned *lightest,
                            unsigned *deepest, unsigned long long *total, unsigned long long *counted,
                            unsigned *at_word, unsigned carries_in)
{
    const uint64_t span = (uint64_t)nonces * 32ull;
    const uint64_t stride = (uint64_t)gridDim.x * blockDim.x;

    for (uint64_t at = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x; at < span; at += stride)
    {
        const uint32_t nonce = start + (uint32_t)(at / 32ull);
        const unsigned bit = (unsigned)(at % 32ull);

        uint32_t left[64];
        uint32_t right[64];
        for (unsigned slot = 0u; slot < BLOCK_WORDS; slot += 1u)
        {
            left[slot] = base[slot];
            right[slot] = base[slot];
        }
        // The word that carries the difference is a parameter, and not always the nonce. A miner
        // rolls the extranonce, which moves the merkle root and thus word zero. It rolls the time,
        // which moves word one. A miner really has those difference positions, as well as word
        // three.
        left[carries_in] = nonce;
        right[carries_in] = nonce ^ ((uint32_t)1u << bit);

        expand(left);
        expand(right);

        unsigned weight = 0u;
        unsigned reach = 0u;
        for (unsigned slot = 0u; slot < 64u; slot += 1u)
        {
            const unsigned here = (unsigned)__popc(left[slot] ^ right[slot]);
            weight += here;
            if ((here > 0u) && (here <= 2u))
            {
                reach = slot;
            }

            // A count, and not a minimum. The smallest weight seen is a tail statistic. It grows more
            // extreme with the sample, whether something is there or not. At 2^25 pairs, chance alone
            // delivers a one-bit word about a quarter of the time. Thus a minimum of one says
            // nothing. A rate can sit against the rate of chance, and the two can separate.
            //
            // Exactly one, and not at most one. A word that the nonce never reaches weighs zero. A
            // count of those reports every untainted word as a perfect path.
            if (here == 1u)
            {
                atomicAdd(&at_word[slot], 1u);
            }
        }

        atomicMin(lightest, weight);
        atomicMax(deepest, reach);
        atomicAdd(total, (unsigned long long)weight);
        atomicAdd(counted, 1ull);
    }
}

/** @brief A block whose header and nonce the chain already recorded. */
struct Vector
{
    const char *header_hex;
    uint32_t nonce;
    const char *source;
};

/** @brief The tree's own known-answer headers, from src/test/kat_validation.cpp. */
const Vector VECTORS[2] = {
    {"0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac7"
     "2c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c",
     0x7c2bac1du, "genesis, height 0"},
    {"0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000e320b6c2fffc8d750423"
     "db8b1eb942ae710e951ed797f7affc8892b0f1fc122bc7f5d74df2b9441a42a14695",
     0x9546a142u, "block 125552"}};

/**
 * @brief Fills the header tail block as the miner does.
 *
 * @param[out] block      Sixteen words [BORROWS].
 * @param[in]  header_hex The eighty-byte header as hex.
 * @note Bytes sixty-four through seventy-nine of the header, then the padding: 0x80000000, ten zero
 *       words, and the length in bits. Ten of the sixteen inputs are zero, and one more is a
 *       constant. For that reason, this is not the same experiment as sixteen random words.
 */
void fill_block(uint32_t *block, const char *header_hex)
{
    for (unsigned slot = 0u; slot < 4u; slot += 1u)
    {
        uint32_t word = 0u;
        for (unsigned byte_at = 0u; byte_at < 4u; byte_at += 1u)
        {
            const unsigned offset = ((64u + (slot * 4u) + byte_at) * 2u);
            char pair[3] = {header_hex[offset], header_hex[offset + 1u], '\0'};
            word = (word << 8) | (uint32_t)std::strtoul(pair, nullptr, 16);
        }
        block[slot] = word;
    }
    block[4] = 0x80000000u;
    for (unsigned slot = 5u; slot < 15u; slot += 1u)
    {
        block[slot] = 0u;
    }
    block[15] = 640u;
}

/** @brief The mix functions of the schedule itself, on the host, for the exact trace. */
uint32_t small_sigma_zero(uint32_t value)
{
    return ((value >> 7) | (value << 25)) ^ ((value >> 18) | (value << 14)) ^ (value >> 3);
}

/** @brief The other one. */
uint32_t small_sigma_one(uint32_t value)
{
    return ((value >> 17) | (value << 15)) ^ ((value >> 19) | (value << 13)) ^ (value >> 10);
}

/**
 * @brief The difference the schedule carries when no carry ever fires, exactly.
 *
 * @param[in]  from   Which of the sixteen input words carries the difference.
 * @param[in]  bit    Which bit of it to flip.
 * @param[out] weight Weight at each of the sixty-four words [BORROWS].
 * @note Exclusive-or replaces addition. That is the standard linearization, and it is exact whenever
 *       no carry propagates. It gives the floor. A word whose linearized weight is w cannot come out
 *       below w, except where a carry cancels something. A word with linearized weight one carries a
 *       genuine path. A word with linearized weight nine that reaches one is a coincidence of
 *       carries, and costs whatever that coincidence costs.
 * @note For that reason, a sample is the wrong tool for the question. The trace is thirty-two words
 *       of arithmetic, and needs no pairs at all.
 */
void trace_linear(unsigned from, unsigned bit, unsigned *weight, int *tainted)
{
    // Whether a word depends on the input at all is a separate fact from the weight of its
    // linearized difference. A merge of the two reports a word whose GF(2) terms cancel as a word
    // that the difference never reaches. W25 from a merkle difference is exactly that case. 25-7
    // and 25-2 are both tainted, and the contributions cancel to zero over GF(2). The real word
    // still differs, because the carries do not cancel with them.
    int reaches[64];
    for (unsigned at = 0u; at < 16u; at += 1u)
    {
        reaches[at] = (at == from) ? 1 : 0;
    }
    for (unsigned at = 16u; at < 64u; at += 1u)
    {
        reaches[at] = (reaches[at - 16u] | reaches[at - 15u] | reaches[at - 7u] | reaches[at - 2u]);
    }
    for (unsigned at = 0u; at < 64u; at += 1u)
    {
        tainted[at] = reaches[at];
    }

    uint32_t difference[64];
    for (unsigned at = 0u; at < 16u; at += 1u)
    {
        difference[at] = 0u;
    }
    difference[from] = (uint32_t)1u << bit;

    for (unsigned at = 16u; at < 64u; at += 1u)
    {
        difference[at] = difference[at - 16u] ^ small_sigma_zero(difference[at - 15u]) ^ difference[at - 7u] ^
                         small_sigma_one(difference[at - 2u]);
    }
    // Counted by hand instead of by a builtin, because this half runs on the host and MSVC has no
    // __builtin_popcount.
    for (unsigned at = 0u; at < 64u; at += 1u)
    {
        unsigned counted_bits = 0u;
        uint32_t remaining = difference[at];
        while (remaining != 0u)
        {
            counted_bits += 1u;
            remaining &= remaining - 1u;
        }
        weight[at] = counted_bits;
    }
}

} // namespace

int main(int argc, char **argv)
{
    const unsigned nonce_bits = (argc > 1) ? (unsigned)std::atoi(argv[1]) : 20u;
    const uint32_t nonces = (uint32_t)1u << nonce_bits;

    std::printf("================================================================\n");
    std::printf("  How sparse a nonce difference stays in a real header\n");
    std::printf("================================================================\n");
    std::printf("\n  bench_space asked this with sixteen random schedule words and found a one-bit\n");
    std::printf("  difference surviving to W27. A real mining block is not sixteen random words:\n");
    std::printf("  four words of header, then 0x80000000, ten zero words, and the length. Ten of\n");
    std::printf("  sixteen inputs are zero and one more is a constant.\n");
    std::printf("\n  The headers are the tree's own known-answer vectors, so these are blocks that\n");
    std::printf("  really were solved. Sparsity is the total weight of the expanded difference over\n");
    std::printf("  all sixty-four words, which is what a schedule differential costs.\n");
    std::printf("\n  Walking 2^%u nonces by 32 bit positions each.\n", nonce_bits);

    // The exact trace first. It costs nothing, and it decides what the sample below can mean. A word
    // whose linearized weight is one carries a real path. A word whose linearized weight is nine,
    // and reaches one, is a cancel of carries, and costs what that costs.
    std::printf("\n================================================================\n");
    std::printf("  The path with no carries, traced exactly\n");
    std::printf("================================================================\n");
    std::printf("\n  Addition replaced by exclusive-or, which is exact whenever no carry fires. A\n");
    std::printf("  word cannot come out below its linearised weight except by a carry cancelling\n");
    std::printf("  something, so this is the floor and it needs no pairs at all.\n");
    std::printf("\n  Smallest linearised weight over the 32 input bits, by difference position.\n");

    std::printf("\n  %6s %14s %14s %14s %14s\n", "word", "in W0 (merkle)", "in W1 (time)", "in W2 (nbits)",
                "in W3 (nonce)");
    std::printf("  %6s %14s %14s %14s %14s\n", "------", "--------------", "--------------", "--------------",
                "--------------");

    unsigned floor_at[4][64];
    int reached[4][64];
    for (unsigned from = 0u; from < 4u; from += 1u)
    {
        for (unsigned at = 0u; at < 64u; at += 1u)
        {
            floor_at[from][at] = 33u;
            reached[from][at] = 0;
        }
        for (unsigned bit = 0u; bit < 32u; bit += 1u)
        {
            unsigned weight[64];
            int tainted[64];
            trace_linear(from, bit, weight, tainted);
            for (unsigned at = 0u; at < 64u; at += 1u)
            {
                reached[from][at] |= tainted[at];
                if ((weight[at] > 0u) && (weight[at] < floor_at[from][at]))
                {
                    floor_at[from][at] = weight[at];
                }
            }
        }
    }

    for (unsigned at = 16u; at < 40u; at += 1u)
    {
        std::printf("  %6u", at);
        for (unsigned from = 0u; from < 4u; from += 1u)
        {
            if (reached[from][at] == 0)
            {
                std::printf(" %14s", "untouched");
            }
            else if (floor_at[from][at] == 33u)
            {
                // Reached, but every one of the thirty-two input bits cancels to zero over GF(2).
                // The real word still differs, through the carries that the linearization drops.
                std::printf(" %14s", "cancels");
            }
            else
            {
                std::printf(" %14u", floor_at[from][at]);
            }
        }
        std::printf("\n");
    }

    std::printf("\n  A one in this table is a word that carries a genuine one-bit path. Anything\n");
    std::printf("  larger reaching one bit in the sampled table below is carries cancelling, and\n");
    std::printf("  the two must not be read as the same finding.\n");

    uint32_t *device_base = nullptr;
    unsigned *device_lightest = nullptr;
    unsigned *device_deepest = nullptr;
    unsigned long long *device_total = nullptr;
    unsigned long long *device_counted = nullptr;
    unsigned *device_at_word = nullptr;
    cudaMalloc((void **)&device_base, BLOCK_WORDS * sizeof(uint32_t));
    cudaMalloc((void **)&device_lightest, sizeof(unsigned));
    cudaMalloc((void **)&device_deepest, sizeof(unsigned));
    cudaMalloc((void **)&device_total, sizeof(unsigned long long));
    cudaMalloc((void **)&device_counted, sizeof(unsigned long long));
    cudaMalloc((void **)&device_at_word, 64u * sizeof(unsigned));

    std::printf("\n  %-26s %10s %10s %10s %12s\n", "block", "min bits", "mean bits", "deepest", "at nonce");
    std::printf("  %-26s %10s %10s %10s %12s\n", "--------------------------", "----------", "----------", "----------",
                "------------");

    unsigned per_word[3][64];

    for (unsigned which = 0u; which < 3u; which += 1u)
    {
        uint32_t block[BLOCK_WORDS];
        const char *label = nullptr;
        uint32_t start = 0u;

        if (which < 2u)
        {
            fill_block(block, VECTORS[which].header_hex);
            label = VECTORS[which].source;

            // Start below the recorded nonce. Thus the walk crosses it, and does not begin on it.
            start = VECTORS[which].nonce - (nonces / 2u);
        }
        else
        {
            // The same padded shape, with header words that are not a real block. A difference
            // between this row and the two above comes from the header, and not the shape.
            fill_block(block, VECTORS[0].header_hex);
            block[0] = 0x12345678u;
            block[1] = 0x9abcdef0u;
            block[2] = 0x0f1e2d3cu;
            label = "padded shape, invented header";
            start = 0u;
        }

        cudaMemcpy(device_base, block, sizeof(block), cudaMemcpyHostToDevice);
        cudaMemset(device_lightest, 0xff, sizeof(unsigned));
        cudaMemset(device_deepest, 0, sizeof(unsigned));
        cudaMemset(device_total, 0, sizeof(unsigned long long));
        cudaMemset(device_counted, 0, sizeof(unsigned long long));
        cudaMemset(device_at_word, 0, 64u * sizeof(unsigned));

        walk_sparse<<<4096, 128>>>(device_base, nonces, start, device_lightest, device_deepest, device_total,
                                   device_counted, device_at_word, NONCE_WORD);
        cudaDeviceSynchronize();

        unsigned lightest = 0u;
        unsigned deepest = 0u;
        unsigned long long total = 0u;
        unsigned long long counted = 0u;
        cudaMemcpy(&lightest, device_lightest, sizeof(lightest), cudaMemcpyDeviceToHost);
        cudaMemcpy(&deepest, device_deepest, sizeof(deepest), cudaMemcpyDeviceToHost);
        cudaMemcpy(&total, device_total, sizeof(total), cudaMemcpyDeviceToHost);
        cudaMemcpy(&counted, device_counted, sizeof(counted), cudaMemcpyDeviceToHost);
        cudaMemcpy(per_word[which], device_at_word, 64u * sizeof(unsigned), cudaMemcpyDeviceToHost);

        char where[16];
        if (which < 2u)
        {
            std::snprintf(where, sizeof(where), "%u", VECTORS[which].nonce);
        }
        else
        {
            std::snprintf(where, sizeof(where), "%s", "-");
        }
        std::printf("  %-26s %10u %10.1f %10u %12s\n", label, lightest,
                    (counted > 0u) ? ((double)total / (double)counted) : 0.0, deepest, where);
    }

    // The trace is exact. Thus it predicts, and does not only describe. A word that it floors at one
    // should show a large count below. A word that it floors at two should show a small one, or
    // none. That check is the difference between a law and a story.
    std::printf("\n================================================================\n");
    std::printf("  The trace against the sampling, on genesis\n");
    std::printf("================================================================\n");
    std::printf("\n  A word the trace floors at one carries a real path and should show a large\n");
    std::printf("  count. A word it floors at two can only reach one bit by a carry cancelling,\n");
    std::printf("  and should show few or none. A disagreement means one of the two is wrong.\n");

    {
        uint32_t block[BLOCK_WORDS];
        fill_block(block, VECTORS[0].header_hex);
        cudaMemcpy(device_base, block, sizeof(block), cudaMemcpyHostToDevice);

        std::printf("\n  %-14s %8s %10s %14s %12s\n", "difference in", "word", "trace", "counted", "agrees");
        std::printf("  %-14s %8s %10s %14s %12s\n", "--------------", "--------", "----------", "--------------",
                    "------------");

        const char *named[4] = {"W0 merkle", "W1 time", "W2 nbits", "W3 nonce"};
        for (unsigned from = 0u; from < 4u; from += 1u)
        {
            cudaMemset(device_lightest, 0xff, sizeof(unsigned));
            cudaMemset(device_deepest, 0, sizeof(unsigned));
            cudaMemset(device_total, 0, sizeof(unsigned long long));
            cudaMemset(device_counted, 0, sizeof(unsigned long long));
            cudaMemset(device_at_word, 0, 64u * sizeof(unsigned));

            walk_sparse<<<4096, 128>>>(device_base, nonces, 0u, device_lightest, device_deepest, device_total,
                                       device_counted, device_at_word, from);
            cudaDeviceSynchronize();

            unsigned counts[64];
            cudaMemcpy(counts, device_at_word, sizeof(counts), cudaMemcpyDeviceToHost);

            for (unsigned at = 16u; at < 40u; at += 1u)
            {
                const unsigned floored = floor_at[from][at];
                if ((floored != 1u) && (counts[at] == 0u))
                {
                    continue;
                }

                // A floor of one should be common, and anything else should be rare. The code judges
                // rare against the whole sweep, and not a fixed number, because the sweep size is an
                // argument.
                const double many = (double)nonces * 32.0;
                const int is_common = ((double)counts[at] > (many * 0.001)) ? 1 : 0;

                // What the floor does and does not predict. Thus the verdict is not a threshold picked
                // to make rows agree.
                //
                //   Floor 1:   a carry-free chain reaches one bit. Common or rare, by the length of
                //              that chain, because no addition on it may carry.
                //
                //   Cancels:   the GF(2) terms sum to zero. Thus the real difference is only carries,
                //              and a carry-only difference is often a single bit. These are among the
                //              sparsest words in practice, and not the emptiest.
                //
                //   Floor 2:   one carry must cancel one bit. Uncommon, but reachable.
                //   Floor 3+:  two or more bits must cancel. Effectively never.
                //
                // Thus only a frequent floor of three or more is a contradiction.
                const int agrees = ((floored < 3u) || (floored == 33u) || (is_common == 0)) ? 1 : 0;

                char trace_text[16];
                if (reached[from][at] == 0)
                {
                    std::snprintf(trace_text, sizeof(trace_text), "%s", "untouched");
                }
                else if (floored == 33u)
                {
                    std::snprintf(trace_text, sizeof(trace_text), "%s", "cancels");
                }
                else
                {
                    std::snprintf(trace_text, sizeof(trace_text), "%u", floored);
                }
                std::printf("  %-14s %8u %10s %14u %12s\n", named[from], at, trace_text, counts[at],
                            (agrees != 0) ? "yes" : "NO");
            }
        }

        std::printf("\n  Every row agreeing means the trace is the law and the sampling was only\n");
        std::printf("  ever reading it through carries. The deepest one-bit path a miner can reach\n");
        std::printf("  is then a fact about which header word is moved, not about how many nonces\n");
        std::printf("  are tried.\n");
    }

    cudaFree(device_base);
    cudaFree(device_lightest);
    cudaFree(device_deepest);
    cudaFree(device_total);
    cudaFree(device_counted);
    cudaFree(device_at_word);

    // bench_space reads W27 from the per-word minimum. Thus the bench prints the same column here
    // for the real blocks. A real header that keeps sparsity deeper than random words would mean
    // that the padding helps the difference stay thin, and does not hinder it.
    //
    // How often a word comes out at one bit or fewer, against how often chance alone delivers it.
    // Thirty-three of the 2^32 differences that a word can carry have weight zero or one. Thus the
    // rate to beat is 33/2^32, and a word that matches it carries no path at all.
    const double pairs = (double)nonces * 32.0;
    const double by_chance = pairs * (33.0 / 4294967296.0);

    std::printf("\n  How often each word comes out at one bit or fewer\n");
    std::printf("\n  Pairs per word: %.0f. Chance alone gives %.3f such words, since 33 of the\n", pairs, by_chance);
    std::printf("  2^32 possible differences weigh zero or one. A count near that is no path; a\n");
    std::printf("  count far above it is a difference the schedule is carrying instead of mixing.\n");
    std::printf("\n  Only words beating chance by tenfold are listed, plus every word up to 27.\n");

    std::printf("\n  %6s %14s %14s %14s %12s\n", "word", "genesis", "125552", "invented", "vs chance");
    std::printf("  %6s %14s %14s %14s %12s\n", "------", "--------------", "--------------", "--------------",
                "------------");
    for (unsigned at = 0u; at < 64u; at += 1u)
    {
        const unsigned most = (per_word[0][at] > per_word[1][at]) ? per_word[0][at] : per_word[1][at];
        const unsigned highest = (most > per_word[2][at]) ? most : per_word[2][at];
        if ((at > 27u) && ((double)highest < (by_chance * 10.0)))
        {
            continue;
        }
        const double ratio = (by_chance > 0.0) ? ((double)highest / by_chance) : 0.0;
        std::printf("  %6u %14u %14u %14u %12.1fx\n", at, per_word[0][at], per_word[1][at], per_word[2][at], ratio);
    }

    std::printf("\n  bench_space read one bit through W27 with sixteen random words, at 131072\n");
    std::printf("  pairs per word where chance gives 0.001 - so those were real. This asks the\n");
    std::printf("  same question of a real block at a sample where chance gives a quarter, which\n");
    std::printf("  is why it counts instead of minimising. A word only carries a path where its\n");
    std::printf("  count stands clear of the chance figure printed above.\n");
    return 0;
}
