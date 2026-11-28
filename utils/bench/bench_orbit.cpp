// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_orbit.cpp
 * @brief Do independent anchors stay independent, or do they collapse onto one orbit?
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note H2 of the digest domain measures 7.999999 bits per byte. That figure does not show maximum
 *       entropy with nothing left to exploit, and this bench exists to show why.
 * @note `utils/bench/bench_scaling.c` shows why that inference is unsound. A corpus of period sixteen
 *       is a single orbit under translation. Position p carries p mod 16. A needle taken at offset s
 *       matches an alignment at `at` exactly when (at + o) = (s + o) mod 16, and the offset cancels.
 *       Every anchor tests the same congruence at any offset.
 * @note Thus four probes ask one question four times. The predicted 2^-16 arrives as 1/16, wrong by
 *       4096. The histogram reads H2 exactly 4.0
 *       throughout, and cannot see any of it.
 * @note Thus H2 is blind to orbit structure, and a maximum-entropy reading rules out nothing. Ask
 *       the question directly: at independent positions, do anchors multiply?
 * @note H1 does not answer it. H1 uses nested leading-zero prefixes, where k=16 implies k=8. Nested
 *       conditions scale as 2^-k whether the domain is an orbit or not. Independent positions are the
 *       test. The bench uses three placements, because an orbit under word rotation would show in one
 *       of them and not the others.
 */

#include "../../src/cu/transpiler/lstar/protocol/table/table_draw.h"
#include "bench_hex.h"
#include "bench_seed.h"
#include "memoria_operor/memoria_operor.h"
#include "sha256_core.h"

#include "embed_turnstile.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "bench_job_ring.h"
#include "bench_load_limit.h"

namespace
{

/** @brief Block 125552. Thus anyone can reproduce the header under test. */
const char *const HEADER_HEX =
    "0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000e320b6c2fffc8d750423"
    "db8b1eb942ae710e951ed797f7affc8892b0f1fc122bc7f5d74df2b9441a42a14695";

Sha256ScanRequest g_base;

/**
 * @brief Reads four header bytes as the big-endian word SHA-256 schedules.
 *
 * @param[in] bytes Four bytes [BORROWS].
 * @return          The schedule word.
 */
uint32_t schedule_word_from_header(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/**
 * @brief Hashes one nonce and returns the digest words in protocol order.
 *
 * @param[in]  nonce   The candidate.
 * @param[out] ordered Eight words, most significant first [BORROWS].
 */
void digest_for_nonce(uint32_t nonce, uint32_t *ordered)
{
    uint32_t block[SHA256_BLOCK_WORDS];
    Sha256State first_pass = g_base.midstate;

    block[0] = g_base.merkle_root_tail;
    block[1] = g_base.ntime;
    block[2] = g_base.nbits;
    block[3] =
        ((nonce >> 24) & 0xFFu) | ((nonce >> 8) & 0xFF00u) | ((nonce << 8) & 0xFF0000u) | ((nonce << 24) & 0xFF000000u);
    block[4] = 0x80000000u;
    for (unsigned slot = 5u; slot < 15u; slot += 1u)
    {
        block[slot] = 0u;
    }
    block[15] = 0x00000280u;
    sha256_block_compress(&first_pass, block);

    Sha256State second_pass;
    sha256_state_init(&second_pass);
    for (unsigned slot = 0u; slot < 8u; slot += 1u)
    {
        block[slot] = first_pass.word[slot];
    }
    block[8] = 0x80000000u;
    for (unsigned slot = 9u; slot < 15u; slot += 1u)
    {
        block[slot] = 0u;
    }
    block[15] = 0x00000100u;
    sha256_block_compress(&second_pass, block);

    for (unsigned rank = 0u; rank < 8u; rank += 1u)
    {
        const uint32_t word = second_pass.word[7u - rank];
        ordered[rank] = ((word >> 24) & 0x000000FFu) | ((word >> 8) & 0x0000FF00u) | ((word << 8) & 0x00FF0000u) |
                        ((word << 24) & 0xFF000000u);
    }
}

/** @brief Where the anchors sit: the whole experiment. */
enum class Placement
{
    Scattered, /**< Independent positions anywhere in the digest. */
    SameWord,  /**< All in one word, different bits. */
    SameBit,   /**< Same bit index across different words. An orbit under word rotation shows here. */
    Nested     /**< Leading-zero prefix, the control that scales trivially. */
};

/** @brief Anchor positions, `count` of them, each in [0,256). */
struct Positions
{
    unsigned at[256];
    unsigned count;
};

/**
 * @brief Chooses k anchor positions under one placement rule.
 *
 * @param[in]     count     How many anchors.
 * @param[in]     placement Which rule.
 * @param[in,out] generator Random source [BORROWS].
 * @return                  The chosen bit positions, each in [0,256).
 */
Positions choose_positions(unsigned count, Placement placement, TableDraw &generator)
{
    Positions positions;
    positions.count = 0u;

    if (placement == Placement::Nested)
    {
        for (unsigned at = 0u; at < count; at += 1u)
        {
            positions.at[positions.count] = at;
            positions.count += 1u;
        }
        return positions;
    }
    if (placement == Placement::SameWord)
    {
        // One word carries 32 bits. Thus this rule caps at 32 anchors.
        const unsigned word = table_draw_next(&generator) % 8u;
        unsigned bits[32];
        for (unsigned bit = 0u; bit < 32u; bit += 1u)
        {
            bits[bit] = bit;
        }
        table_shuffled(bits, 32u, &generator);
        for (unsigned at = 0u; at < count; at += 1u)
        {
            positions.at[positions.count] = (word * 32u) + bits[at];
            positions.count += 1u;
        }
        return positions;
    }
    if (placement == Placement::SameBit)
    {
        // Eight words. Thus this rule caps at 8 anchors. If the digest were an orbit under word
        // rotation, these would all test one condition, exactly as the offsets of periodic16 do.
        const unsigned bit = table_draw_next(&generator) % 32u;
        unsigned words[8];
        for (unsigned word = 0u; word < 8u; word += 1u)
        {
            words[word] = word;
        }
        table_shuffled(words, 8u, &generator);
        for (unsigned at = 0u; at < count; at += 1u)
        {
            positions.at[positions.count] = (words[at] * 32u) + bit;
            positions.count += 1u;
        }
        return positions;
    }

    unsigned all[256];
    for (unsigned at = 0u; at < 256u; at += 1u)
    {
        all[at] = at;
    }
    table_shuffled(all, 256u, &generator);
    for (unsigned at = 0u; at < count; at += 1u)
    {
        positions.at[positions.count] = all[at];
        positions.count += 1u;
    }
    return positions;
}

/** @brief The shares of the seats of count_survivors: the anchors, a tally for each seat, and the nonces of each. */
struct SurvivorShares
{
    const Positions *positions; /**< Where the anchors sit [BORROWS]. */
    uint64_t *tallies;          /**< A tally for each seat [BORROWS]. */
    uint32_t share;             /**< The nonces of each seat. */
};

/**
 * @brief Counts nonces whose digest carries a zero at every anchor position.
 *
 * @param[in] positions Where the anchors sit [BORROWS].
 * @param[in] span      How many nonces to test.
 * @return              How many survived every anchor.
 */
uint64_t count_survivors(const Positions &positions, uint32_t span)
{
    const unsigned workers = (bench_processors() > 1u) ? (bench_processors() - 1u) : 1u;
    // a tally for each seat, at most BENCH_JOBS_MAX of them
    uint64_t tallies[BENCH_JOBS_MAX] = {0u};
    if (workers > BENCH_JOBS_MAX)
    {
        fprintf(stderr, "bench_orbit: %u seats, past the %u the ring holds\n", workers, BENCH_JOBS_MAX);
        exit(1);
    }
    SurvivorShares shares = {&positions, tallies, span / workers};

    bench_jobs_run(
        [](void *context, unsigned seat) {
            const SurvivorShares *const seat_shares = (const SurvivorShares *)context;
            const uint32_t share = seat_shares->share;
            uint64_t found = 0u;
            for (uint32_t step = 0u; step < share; step += 1u)
            {
                uint32_t ordered[8];
                digest_for_nonce((seat * share) + step, ordered);

                bool held = true;
                for (unsigned at = 0u; at < seat_shares->positions->count; at += 1u)
                {
                    const unsigned position = seat_shares->positions->at[at];
                    // Position zero is the most significant bit of the number the target reads.
                    const uint32_t word = ordered[position / 32u];
                    if (((word >> (31u - (position % 32u))) & 1u) != 0u)
                    {
                        held = false;
                        break;
                    }
                }
                found += held ? 1u : 0u;
            }
            seat_shares->tallies[seat] = found;
        },
        &shares, workers, workers);

    uint64_t total = 0u;
    for (unsigned seat = 0u; seat < workers; seat += 1u)
    {
        total += tallies[seat];
    }
    return total;
}

} // namespace

int main()
{
    printf("================================================================\n");
    printf("  The orbit test: do independent anchors multiply?\n");
    printf("================================================================\n");
    printf("\n  anchor_sift's bench_scaling found the probe bound failing by 4096x on a\n");
    printf("  periodic corpus, because every anchor offset cancels and four probes ask one\n");
    printf("  question four times. H2 read exactly 4.0 throughout and could not see it.\n");
    printf("\n  So H2 = 7.999999 on this domain rules out nothing, and the question has to be\n");
    printf("  asked directly. k anchors, each demanding a zero bit. Independent anchors leave\n");
    printf("  2^-k survivors. Anchors collapsed onto one orbit leave far more.\n");

    const BenchHexBytes header = bench_hex_bytes(HEADER_HEX);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &g_base, .bytes = sizeof(g_base), .val = 0);
    sha256_header_midstate(&g_base.midstate, header.at);
    g_base.merkle_root_tail = schedule_word_from_header(header.at + 64u);
    g_base.ntime = schedule_word_from_header(header.at + 68u);
    g_base.nbits = schedule_word_from_header(header.at + 72u);

    const uint32_t span = 1u << 24;
    TableDraw generator;
    table_draw_seeded(&generator, bench_seed(20260908u));
    const auto started = embed_clock_ns();

    const struct
    {
        Placement placement;
        const char *name;
        unsigned ceiling;
    } rules[] = {
        {Placement::Nested, "nested prefix (control, must scale)", 16u},
        {Placement::Scattered, "scattered, independent positions", 16u},
        {Placement::SameWord, "all in one word, different bits", 16u},
        {Placement::SameBit, "same bit index across words", 8u},
    };

    for (const auto &rule : rules)
    {
        printf("\n================================================================\n");
        printf("  %s\n", rule.name);
        printf("================================================================\n");
        printf("\n  %8s %16s %16s %12s\n", "anchors", "predicted", "survivors", "ratio");
        printf("  %8s %16s %16s %12s\n", "--------", "----------------", "----------------", "------------");

        for (unsigned count = 1u; count <= rule.ceiling; count += (count < 4u) ? 1u : 4u)
        {
            const Positions positions = choose_positions(count, rule.placement, generator);
            const uint64_t survivors = count_survivors(positions, span);
            const double predicted = (double)span * pow(0.5, (double)count);

            printf("  %8u %16.1f %16llu %12.4f\n", count, predicted, (unsigned long long)survivors,
                   (predicted > 0.0) ? ((double)survivors / predicted) : 0.0);
        }
    }

    const auto finished = embed_clock_ns();

    printf("\n================================================================\n");
    printf("  Reading it\n");
    printf("================================================================\n");
    printf("\n  A ratio near 1.0 in every row means the anchors are independent and no orbit\n");
    printf("  collapse is present at this placement. A ratio far above 1.0 in one placement\n");
    printf("  and not the others names the group the orbit sits under, exactly as periodic16\n");
    printf("  named translation.\n");
    printf("\n  The nested row is the control. It must scale, because k=16 implies k=8 there,\n");
    printf("  and it would scale on an orbit too. That is precisely why H1 could not answer\n");
    printf("  this question and why the other three rows exist.\n");
    printf("\n  elapsed %.1f s\n", ((double)(finished - started) / 1.0e9));
    printf("================================================================\n");
    return 0;
}
