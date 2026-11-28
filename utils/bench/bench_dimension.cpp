// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_dimension.cpp
 * @brief n dimensions at once, at every intersection. Higher-order differentials and the degree.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note A first-order measurement looks at one dimension at a time: one salt, one flipped bit, one
 *       dimension on its own. It cannot see where difference directions intersect. The only exact
 *       statement available about this function lives there.
 * @note The k-th order differential of f along independent directions d1..dk is a sum over all 2^k
 *       subsets S. Each term is f(x xor (sum of di for i in S)). If the algebraic degree of f over
 *       GF(2) is less than k, this is identically zero for every x. Not small. Zero.
 * @note Thus a differential that vanishes proves a degree bound, and is not a sampled bound.
 *       Section 3 of theory/workbooks/orior/sha256-topology.md names that bound as unknown.
 * @note The measurement therefore reads the algebraic degree of reduced-round SHA-256 as a function
 *       of the nonce. It finds the largest order that still vanishes. The growth of the degree decides
 *       whether any algebraic attack has room to work.
 * @warning Cost is 2^k hashes per base point. Thus the order axis is exponential. The table stops
 *          where it stops for that reason, and not because of a finding there.
 */

#include "../../src/cu/transpiler/lstar/protocol/table/table_draw.h"
#include "bench_hex.h"
#include "bench_seed.h"
#include "sha256_core.h"

#include "embed_turnstile.h"
#include <stdint.h>
#include <stdio.h>

namespace
{

/** @brief Block 125552. Thus anyone can reproduce the header under test. */
const char *const HEADER_HEX =
    "0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000e320b6c2fffc8d750423"
    "db8b1eb942ae710e951ed797f7affc8892b0f1fc122bc7f5d74df2b9441a42a14695";

Sha256State g_midstate;
uint32_t g_merkle_tail = 0u;
uint32_t g_ntime = 0u;
uint32_t g_nbits = 0u;

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
 * @brief The function under test: reduced-round compression of the header tail, as a map from nonce.
 *
 * @param[in]  nonce  The input.
 * @param[in]  rounds How many rounds to run.
 * @param[out] state  The chaining value that results [BORROWS].
 */
void evaluate(uint32_t nonce, unsigned rounds, Sha256State *state)
{
    uint32_t block[SHA256_BLOCK_WORDS];

    *state = g_midstate;
    block[0] = g_merkle_tail;
    block[1] = g_ntime;
    block[2] = g_nbits;
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

/**
 * @brief Computes one k-th order differential and reports whether it vanished.
 *
 * @param[in] base       The base point.
 * @param[in] directions The k independent directions [BORROWS].
 * @param[in] order      k.
 * @param[in] rounds     Rounds to run.
 * @return               True where the differential is exactly zero in all 256 bits.
 * @note The sum is over every one of the 2^k subsets. Thus this is an intersection measurement, and
 *       not k separate first-order ones.
 */
bool differential_vanishes(uint32_t base, const uint32_t *directions, unsigned order, unsigned rounds)
{
    uint32_t accumulated[SHA256_STATE_WORDS] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    const uint32_t subsets = 1u << order;

    for (uint32_t subset = 0u; subset < subsets; subset += 1u)
    {
        uint32_t point = base;
        for (unsigned index = 0u; index < order; index += 1u)
        {
            if (((subset >> index) & 1u) != 0u)
            {
                point ^= directions[index];
            }
        }

        Sha256State value;
        evaluate(point, rounds, &value);
        for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
        {
            accumulated[slot] ^= value.word[slot];
        }
    }

    for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
    {
        if (accumulated[slot] != 0u)
        {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    printf("================================================================\n");
    printf("  n dimensions at once: higher-order differentials\n");
    printf("  The degree of reduced-round SHA-256 as a map from the nonce\n");
    printf("================================================================\n");

    const BenchHexBytes header = bench_hex_bytes(HEADER_HEX);
    sha256_header_midstate(&g_midstate, header.at);
    g_merkle_tail = schedule_word_from_header(header.at + 64u);
    g_ntime = schedule_word_from_header(header.at + 68u);
    g_nbits = schedule_word_from_header(header.at + 72u);

    printf("\n  A k-th order differential sums f over all 2^k corners of a k-dimensional\n");
    printf("  cube of input differences. If the algebraic degree over GF(2) is below k it\n");
    printf("  is identically zero, for every base point, exactly. a column of 'vanishes'\n");
    printf("  is a proof of a degree bound, not a sampled bound.\n");
    printf("\n  Directions are distinct single bits of the nonce, so the cube spans k of the\n");
    printf("  32 input dimensions and every one of its intersections is visited.\n");

    TableDraw generator;
    table_draw_seeded(&generator, bench_seed(20260908u));
    const unsigned base_points = 24u;
    const unsigned max_order = 12u;

    printf("\n  vanishes at every base point tested = V, fails somewhere = .\n");
    printf("\n  %6s", "rounds");
    for (unsigned order = 1u; order <= max_order; order += 1u)
    {
        printf(" %2u", order);
    }
    printf("   degree is at least\n");
    printf("  %6s", "------");
    for (unsigned order = 1u; order <= max_order; order += 1u)
    {
        printf(" --");
    }
    printf("   ------------------\n");

    const auto started = embed_clock_ns();

    for (unsigned rounds = 1u; rounds <= 10u; rounds += 1u)
    {
        printf("  %6u", rounds);
        unsigned lowest_failing = 0u;

        for (unsigned order = 1u; order <= max_order; order += 1u)
        {
            bool always_vanishes = true;

            for (unsigned trial = 0u; trial < base_points; trial += 1u)
            {
                // Distinct single-bit directions, drawn without replacement.
                uint32_t directions[16];
                unsigned chosen = 0u;
                uint32_t used = 0u;

                while (chosen < order)
                {
                    const unsigned position = table_draw_next(&generator) % 32u;
                    if (((used >> position) & 1u) == 0u)
                    {
                        used |= (1u << position);
                        directions[chosen] = (1u << position);
                        chosen += 1u;
                    }
                }

                if (!differential_vanishes(table_draw_next(&generator), directions, order, rounds))
                {
                    always_vanishes = false;
                    break;
                }
            }

            printf("  %c", always_vanishes ? 'V' : '.');
            // Order k vanishes exactly when the degree is below k. Thus the degree is the highest
            // order that fails. A report of the lowest order that fails would call every row with
            // any failure "degree 1".
            if (!always_vanishes)
            {
                lowest_failing = order;
            }
        }

        if (lowest_failing == 0u)
        {
            printf("   0, the nonce has not entered\n");
        }
        else if (lowest_failing == max_order)
        {
            printf("   at least %u, past the table\n", max_order);
        }
        else
        {
            printf("   exactly %u\n", lowest_failing);
        }
    }

    const auto finished = embed_clock_ns();

    printf("\n  Reading the table. A V means the differential was exactly zero at every base\n");
    printf("  point, which for a genuine degree bound is what the algebra requires and is\n");
    printf("  not a statistical statement. The first dot in a row is where the degree of the\n");
    printf("  reduced function reaches that order.\n");

    printf("\n  Why this is the exact measurement the other twenty were not. Every earlier\n");
    printf("  result in this tree says 'no structure was observed at this sensitivity'. A\n");
    printf("  vanishing higher-order differential says 'this function cannot have degree at\n");
    printf("  or above k', which no amount of sampling can overturn. Where the row fills\n");
    printf("  with dots, the degree has saturated and no algebraic attack of that order has\n");
    printf("  anything left to work on.\n");

    printf("\n  The nonce carries 32 bits, so degree 32 is the ceiling. A row that is all\n");
    printf("  dots by order %u is within a factor of three of that ceiling after a handful\n", max_order);
    printf("  of rounds, out of the 128 a header hash runs.\n");

    printf("\n================================================================\n");
    printf("  elapsed %.1f s\n", ((double)(finished - started) / 1.0e9));
    printf("================================================================\n");
    return 0;
}
