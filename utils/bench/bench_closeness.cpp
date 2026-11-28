// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_closeness.cpp
 * @brief The first premise of the engine, measured, and the round at which it fails.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The engine rests on one premise: know a corpus completely, and everything close to it is
 *       true. That is a manifold assumption. Closeness in the representation implies membership in
 *       the set. For that reason, the construction works on language, on crystal boundaries and on
 *       protein constructs. Those domains have it.
 * @note It is also, exactly, the property that a cryptographic hash must destroy. The avalanche
 *       criterion states that closeness in the input maps to independence in the output. The design
 *       goal is its deliberate destruction, and not a weaker or obscured form.
 * @note Thus this file measures the premise directly, and not its consequences. Five separate
 *       negative results in the workbook, H3, H4, H13, H16 and H27, are not five findings. They are
 *       this one fact, reported five ways. The correlation below is the fact itself.
 * @note The round axis makes it a measurement, and not a restatement. At low round counts
 *       the premise holds and the engine would work; at high ones it does not. Where it crosses is
 *       the boundary of the engine's applicability on this domain, expressed as a number.
 */

#include "../../src/cu/transpiler/lstar/protocol/table/table_draw.h"
#include "bench_seed.h"
#include "memoria_operor/memoria_operor.h"
#include "sha256_core.h"

#include "embed_turnstile.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>

namespace
{

/**
 * @brief Counts the bits that differ across a chaining value.
 *
 * @param[in] left  Eight words [BORROWS].
 * @param[in] right Eight words [BORROWS].
 * @return          How many of the 256 bits differ.
 */
unsigned state_distance(const Sha256State *left, const Sha256State *right)
{
    unsigned total = 0u;

    for (unsigned slot = 0u; slot < 8u; slot += 1u)
    {
        total += (unsigned)__builtin_popcount(left->word[slot] ^ right->word[slot]);
    }
    return total;
}

/**
 * @brief Runs a nonce through a chosen number of rounds.
 *
 * @param[in]  nonce  The input.
 * @param[in]  rounds How many rounds.
 * @param[out] state  Where the result lands [BORROWS].
 */
void evaluate(uint32_t nonce, unsigned rounds, Sha256State *state)
{
    uint32_t block[SHA256_BLOCK_WORDS];

    sha256_state_init(state);
    block[0] = 0xc7f5d74du;
    block[1] = 0xf2b9441au;
    block[2] = 0x42a14695u;
    block[3] =
        ((nonce >> 24) & 0xFFu) | ((nonce >> 8) & 0xFF00u) | ((nonce << 8) & 0xFF0000u) | ((nonce << 24) & 0xFF000000u);
    block[4] = 0x80000000u;
    for (unsigned slot = 5u; slot < 15u; slot += 1u)
    {
        block[slot] = 0u;
    }
    block[15] = 0x00000280u;
    sha256_block_compress_partial(state, block, rounds);
}

} // namespace

int main()
{
    printf("================================================================\n");
    printf("  Does closeness imply closeness? The engine's premise, measured\n");
    printf("================================================================\n");
    printf("\n  The premise: know a corpus completely and everything close to it is true.\n");
    printf("  Operationally that requires closeness in the input to survive into the output,\n");
    printf("  because a representation where it does not carries no neighbourhoods and there\n");
    printf("  is nothing for a sift to generalise across.\n");
    printf("\n  Correlation between input distance and output distance, over 200,000 pairs at\n");
    printf("  each round count. Pairs are drawn at every input distance from 1 to 32 so the\n");
    printf("  correlation has a range to work with.\n");

    TableDraw generator;
    table_draw_seeded(&generator, bench_seed(20260908u));
    const unsigned pairs = 200000u;

    // -------------------------------------------------------------------------------------
    // The whole function first. A part is not the object. None of these parts stands for the
    // whole: reduced rounds; one output word out of eight; a 64-bit prefix of a 256-bit digest.
    //
    // Nor these: one frame; one constant; chunk two without the header.
    //
    // The whole is the full 128-round doubled hash over the complete 80-byte header. The bench
    // reads it across all 256 output bits. It takes the difference from the whole, and does not
    // assemble it from parts.
    // -------------------------------------------------------------------------------------
    printf("\n================================================================\n");
    printf("  The whole function\n");
    printf("================================================================\n");
    printf("\n  Full SHA256d over the complete 80-byte header, all 128 rounds, and the whole\n");
    printf("  256-bit digest read instead of a projection of it. The difference is taken\n");
    printf("  from the whole, which is the natural direction.\n");

    {
        const uint8_t base_header[80] = {
            0x01, 0x00, 0x00, 0x00, 0x81, 0xcd, 0x02, 0xab, 0x7e, 0x56, 0x9e, 0x8b, 0xcd, 0x93, 0x17, 0xe2,
            0xfe, 0x99, 0xf2, 0xde, 0x44, 0xd4, 0x9a, 0xb2, 0xb8, 0x85, 0x1b, 0xa4, 0xa3, 0x08, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0xe3, 0x20, 0xb6, 0xc2, 0xff, 0xfc, 0x8d, 0x75, 0x04, 0x23, 0xdb, 0x8b,
            0x1e, 0xb9, 0x42, 0xae, 0x71, 0x0e, 0x95, 0x1e, 0xd7, 0x97, 0xf7, 0xaf, 0xfc, 0x88, 0x92, 0xb0,
            0xf1, 0xfc, 0x12, 0x2b, 0xc7, 0xf5, 0xd7, 0x4d, 0xf2, 0xb9, 0x44, 0x1a, 0x42, 0xa1, 0x46, 0x95};

        double sum_in = 0.0;
        double sum_out = 0.0;
        double sum_in_squared = 0.0;
        double sum_out_squared = 0.0;
        double sum_product = 0.0;
        unsigned lowest_out = 256u;

        for (unsigned trial = 0u; trial < pairs; trial += 1u)
        {
            uint8_t left_header[80];
            uint8_t right_header[80];
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = left_header, .src = base_header, .bytes = sizeof(left_header));

            // Vary the whole header, and not only one field. Any of the 640 bits may move.
            for (unsigned scatter = 0u; scatter < 8u; scatter += 1u)
            {
                left_header[table_draw_next(&generator) % 80u] = (uint8_t)table_draw_next(&generator);
            }
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = right_header, .src = left_header, .bytes = sizeof(right_header));

            // Distinct positions. A draw with replacement can hit the same bit twice and flip it
            // back. That produces an identical pair, and an output distance of zero that reads as
            // a collision.
            const unsigned wanted = 1u + (table_draw_next(&generator) % 64u);
            uint8_t touched[80] = {0};
            unsigned placed = 0u;
            while (placed < wanted)
            {
                const unsigned position = table_draw_next(&generator) % 640u;
                const uint8_t bit = (uint8_t)(1u << (position % 8u));

                if ((touched[position / 8u] & bit) == 0u)
                {
                    touched[position / 8u] |= bit;
                    right_header[position / 8u] ^= bit;
                    placed += 1u;
                }
            }

            unsigned in_distance = 0u;
            for (unsigned at = 0u; at < 80u; at += 1u)
            {
                in_distance += (unsigned)__builtin_popcount((unsigned)(left_header[at] ^ right_header[at]));
            }

            uint8_t left_digest[32];
            uint8_t right_digest[32];
            sha256_double_hash(left_header, sizeof(left_header), left_digest);
            sha256_double_hash(right_header, sizeof(right_header), right_digest);

            unsigned out_distance = 0u;
            for (unsigned at = 0u; at < 32u; at += 1u)
            {
                out_distance += (unsigned)__builtin_popcount((unsigned)(left_digest[at] ^ right_digest[at]));
            }
            lowest_out = (out_distance < lowest_out) ? out_distance : lowest_out;

            sum_in += (double)in_distance;
            sum_out += (double)out_distance;
            sum_in_squared += (double)in_distance * (double)in_distance;
            sum_out_squared += (double)out_distance * (double)out_distance;
            sum_product += (double)in_distance * (double)out_distance;
        }

        const double count = (double)pairs;
        const double covariance = (sum_product / count) - ((sum_in / count) * (sum_out / count));
        const double spread_in = sqrt((sum_in_squared / count) - ((sum_in / count) * (sum_in / count)));
        const double spread_out = sqrt((sum_out_squared / count) - ((sum_out / count) * (sum_out / count)));
        const double correlation = covariance / (spread_in * spread_out);

        printf("\n  pairs                       : %u, whole header varied, whole digest read\n", pairs);
        printf("  mean input distance         : %.2f bits of 640\n", sum_in / count);
        printf("  mean output distance        : %.2f bits of 256\n", sum_out / count);
        printf("  smallest output distance    : %u bits\n", lowest_out);
        printf("  correlation, input to output: %+.6f\n", correlation);
        printf("  in standard errors          : %+.2f\n", correlation * sqrt(count));
        printf("\n  An input pair one bit apart and an input pair sixty-four bits apart produce\n");
        printf("  output pairs the same distance apart. The smallest output distance seen over\n");
        printf("  %u pairs is %u bits, against 128 expected, so nothing came close to\n", pairs, lowest_out);
        printf("  anything. On the whole function there are no neighbourhoods at all.\n");
    }

    printf("\n================================================================\n");
    printf("  The partial view, and where the premise actually dies\n");
    printf("================================================================\n");
    printf("\n  Reduced rounds are a partial and are labeled as one. They are useful for a\n");
    printf("  single purpose: locating the round at which the whole stops carrying closeness.\n");

    printf("\n  %8s %16s %18s %18s\n", "rounds", "correlation", "mean out distance", "premise holds");
    printf("  %8s %16s %18s %18s\n", "--------", "----------------", "------------------", "------------------");

    const auto started = embed_clock_ns();
    unsigned last_holding = 0u;

    const unsigned listed_rounds[] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 12u, 16u, 32u, 64u};
    for (unsigned rounds : listed_rounds)
    {
        double sum_in = 0.0;
        double sum_out = 0.0;
        double sum_in_squared = 0.0;
        double sum_out_squared = 0.0;
        double sum_product = 0.0;

        for (unsigned trial = 0u; trial < pairs; trial += 1u)
        {
            const uint32_t base = table_draw_next(&generator);
            // Draw an input distance uniformly. Thus the correlation covers more than one distance.
            const unsigned wanted = 1u + (table_draw_next(&generator) % 32u);
            uint32_t moved = base;
            unsigned placed = 0u;
            while (placed < wanted)
            {
                const uint32_t bit = 1u << (table_draw_next(&generator) % 32u);
                if ((moved & bit) == (base & bit))
                {
                    moved ^= bit;
                    placed += 1u;
                }
            }

            Sha256State left;
            Sha256State right;
            evaluate(base, rounds, &left);
            evaluate(moved, rounds, &right);

            const double in_distance = (double)__builtin_popcount(base ^ moved);
            const double out_distance = (double)state_distance(&left, &right);

            sum_in += in_distance;
            sum_out += out_distance;
            sum_in_squared += in_distance * in_distance;
            sum_out_squared += out_distance * out_distance;
            sum_product += in_distance * out_distance;
        }

        const double count = (double)pairs;
        const double covariance = (sum_product / count) - ((sum_in / count) * (sum_out / count));
        const double spread_in = sqrt((sum_in_squared / count) - ((sum_in / count) * (sum_in / count)));
        const double spread_out = sqrt((sum_out_squared / count) - ((sum_out / count) * (sum_out / count)));
        const double correlation =
            ((spread_in > 0.0) && (spread_out > 0.0)) ? (covariance / (spread_in * spread_out)) : 0.0;

        // A premise that holds shows as a correlation far outside sampling noise. Rounds where
        // the output never moves are a separate case. Do not label them as a measured absence.
        // The nonce sits at schedule word three, and does not reach the state until round three.
        // Thus those rows report that the input did not arrive yet.
        const double score = correlation * sqrt(count);
        const bool arrived = ((sum_out / count) > 0.0);
        const bool holds = arrived && (fabs(score) > 10.0);
        if (holds)
        {
            last_holding = rounds;
        }

        printf("  %8u %16.6f %18.2f %18s\n", rounds, correlation, sum_out / count,
               arrived ? (holds ? "yes" : "no") : "input absent");
    }

    const auto finished = embed_clock_ns();

    printf("\n================================================================\n");
    printf("  Reading it\n");
    printf("================================================================\n");
    printf("\n  The premise holds through round %u and not past it.\n", last_holding);
    printf("\n  Below that boundary the domain has neighbourhoods, closeness carries, and the\n");
    printf("  engine's construction applies exactly as it does to a language corpus. Above it\n");
    printf("  the correlation is gone and there is no manifold left to generalise across.\n");
    printf("\n  A header hash runs 128 rounds. So the engine's premise is true of SHA-256 for\n");
    printf("  the first few percent of the computation and false for the rest, and every\n");
    printf("  negative result in this tree is a consequence of that single fact instead of\n");
    printf("  an independent finding.\n");
    printf("\n  This is not a defect in the engine. It is a statement about where the engine\n");
    printf("  applies. The domains it was built for, language and crystal boundaries and\n");
    printf("  protein constructs, all carry the premise. This one had it removed on purpose,\n");
    printf("  and the avalanche criterion is the name of the removal.\n");
    printf("\n  elapsed %.1f s\n", ((double)(finished - started) / 1.0e9));
    printf("================================================================\n");
    return 0;
}
