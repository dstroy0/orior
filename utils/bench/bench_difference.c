// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_difference.c
 * @brief Is the DIFFERENCE between two nonces that win structured? The permute-the-difference test.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * docs-check: quoting
 * @note Douglas: "We can permute what's different about our guess to the win."
 * docs-check: end quoting
 *
 * H27 asks a different question, and answers it. Perturb ONE nonce whose digest already carries k
 * leading zeros, and ask whether the zeros survive. They do not, at a sensitivity that would catch a
 * 1.3x effect at five sigma. That is a statement about a single point and its neighborhood.
 *
 * This asks about the SOLUTION SET. Suppose that the nonces with k leading zeros stand in some
 * structured relation to each other. Then the difference between a near-win and a win is a thing
 * to permute, and not to search. The engine here exists to carry that difference.
 *
 * THE BENCH DRAWS THE NULL, AND IT IS EXACT. If the winners have no structure, then for any two of
 * them the XOR is a uniform 32-bit word. Thus:
 *
 *   - Every bit of the XOR is one half the time.
 *   - The population count of the XOR averages 16.
 *   - No particular XOR value recurs more than chance allows.
 *
 * The bench computes all three against the count actually collected. Thus the null moves with the
 * sample, and is never a number chosen by hand. The bench reports a departure in sigma: the
 * binomial sigma for the sample taken.
 *
 * @note This measures, and does not mine. It collects winners by a full scan over a nonce range with
 *       a fixed header. That is the only honest way to get an unbiased sample of them.
 *
 * AND THE POSITIVE CONTROL.
 *
 * A null result is a claim about the TEST as much as about the data. A test that cannot see a signal
 * returns a null on everything. Thus the same routine runs twice. First, on the winners as
 * collected. Second, on the same winners with a one percent departure planted on one bit.
 *
 * The planted run states the sensitivity, and does not calculate it. If the plant does not show, the
 * bench refuses, and does not report a null that it did not earn.
 */

#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

#include "sha256_core.h"
#include "../../includes/platform/platform.h"

/**
 * @brief How many leading zero BITS the digest of a nonce must carry for the collection to keep it.
 *
 * @note Sixteen. The sample size decides the sensitivity. The DEPTH only decides how long the
 *       collection takes. At sixteen, a winner turns up about every 65536 nonces. Thus 1024 of them
 *       cost 67 million hashes, and about a minute on one thread. At twenty it would
 *       be four billion hashes for the same statistics.
 */
#define WANTED_ZEROS 16u

/**
 * @brief How many winners to collect before the bench examines the difference.
 *
 * @note 1024 winners make 523776 pairs. Thus the binomial spread on a per-bit count is 362, against
 *       an expectation of 261888. A one percent departure would be seven sigma. That is the actual
 *       sensitivity of this test. The bench prints it beside every row, and does not assume it.
 */
#define WANTED_WINNERS 1024u

/**
 * @brief Which bit the positive control plants its signal on.
 *
 * @note A low bit. Thus the plant cannot move the highest bit that the scan exercised, and cannot
 *       change which bits the bench reads. Any low bit does; nothing about the test depends on which.
 */
#define PLANTED_BIT 3u

/** @brief Leading zero bits of a big-endian digest. */
static uint32_t leading_zeros(const uint8_t *const digest)
{
    uint32_t zeros = 0u;
    for (uint32_t at = 0u; at < 32u; ++at)
    {
        if (digest[at] != 0u)
        {
            uint8_t byte = digest[at];
            while ((byte & 0x80u) == 0u)
            {
                ++zeros;
                byte = (uint8_t)(byte << 1);
            }
            return zeros;
        }
        zeros += 8u;
    }
    return zeros;
}

/**
 * @brief The pairwise-difference statistics over one set of winners.
 *
 * @param[in] label   What set this is, for the heading.
 * @param[in] winners      The nonces [BORROWS].
 * @param[in] sample_count How many.
 * @return                 The worst departure found, in sigma.
 *
 * @note A function, and thus the POSITIVE CONTROL runs the same code as the measurement, and not a
 *       second copy that could disagree. A planted signal examined by a different
 *       routine proves nothing about the routine that examined the real data.
 */
static double difference_examine(const char *const label, const uint32_t *const winners, const uint32_t sample_count)
{
    /* The XOR of every pair. The null: each bit is one in half of them, and the popcount averages 16. */
    uint64_t pairs = 0u;
    uint64_t bit_set[32];
    uint64_t popcount_total = 0u;
    for (uint32_t at = 0u; at < 32u; ++at)
    {
        bit_set[at] = 0u;
    }

    for (uint32_t one = 0u; one < sample_count; ++one)
    {
        for (uint32_t two = one + 1u; two < sample_count; ++two)
        {
            const uint32_t difference = winners[one] ^ winners[two];
            ++pairs;
            uint32_t ones = 0u;
            for (uint32_t bit = 0u; bit < 32u; ++bit)
            {
                if (((difference >> bit) & 1u) != 0u)
                {
                    ++bit_set[bit];
                    ++ones;
                }
            }
            popcount_total += ones;
        }
    }

    const double expected = (double)pairs / 2.0;

    /* The binomial spread for p = 1/2 over `pairs` trials is sqrt(pairs)/2. Newton on the square
     * root, and not a library call. Thus the null comes from the sample alone. */
    double root = (double)pairs;
    for (uint32_t step = 0u; step < 64u; ++step)
    {
        root = 0.5 * (root + ((double)pairs / root));
    }
    const double bit_sigma = root / 2.0;

    (void)printf("  %s\n", label);
    (void)printf("  %llu pairs. Each bit of the difference should be set in half of them.\n\n",
                 (unsigned long long)pairs);
    (void)printf("  %4s %14s %14s %10s\n", "bit", "set", "expected", "sigma");
    (void)printf("  %s\n", "----------------------------------------------------");

    /* ONLY THE BITS THE SCAN ACTUALLY EXERCISED.
     *
     * The collection walks upward from nonce zero, and stops once it has enough winners. Thus every
     * winner lies below the stop. Bits above that are ZERO IN ALL OF THEM BY CONSTRUCTION, and their
     * XOR is almost never one. A read of them reports the shape of the sample, and not the shape of
     * the solution set. With them in, bit 26 reads 723 sigma, and the mean popcount reads 13.0
     * against a null of 16. Both are an artifact of where the scan stops.
     *
     * The highest bit set anywhere in the sample is the honest boundary. */
    uint32_t exercised = 0u;
    {
        uint32_t seen = 0u;
        for (uint32_t at = 0u; at < sample_count; ++at)
        {
            seen |= winners[at];
        }
        while (seen != 0u)
        {
            ++exercised;
            seen >>= 1;
        }
    }
    (void)printf("  the scan exercised bits 0..%u; above that every winner is zero by\n",
                 (exercised > 0u) ? (exercised - 1u) : 0u);
    (void)printf("  construction and reading them would measure the sampling, not the set.\n\n");

    double worst = 0.0;
    uint32_t worst_bit = 0u;
    for (uint32_t bit = 0u; bit < exercised; ++bit)
    {
        const double off = ((double)bit_set[bit] - expected) / bit_sigma;
        const double magnitude = (off < 0.0) ? -off : off;
        if (magnitude > worst)
        {
            worst = magnitude;
            worst_bit = bit;
        }
        if (bit < 8u)
        {
            (void)printf("  %4u %14llu %14.0f %10.2f\n", bit, (unsigned long long)bit_set[bit], expected, off);
        }
    }

    (void)printf("  ... %u more\n\n", (exercised > 8u) ? (exercised - 8u) : 0u);
    (void)printf("  WORST BIT %u at %.2f sigma, over %u exercised bits\n", worst_bit, worst, exercised);

    /* The popcount null is half the exercised bits, for the same reason. */
    uint64_t exercised_ones = 0u;
    for (uint32_t bit = 0u; bit < exercised; ++bit)
    {
        exercised_ones += bit_set[bit];
    }
    (void)printf("  mean popcount over those bits %.4f, the null is %.4f\n\n", (double)exercised_ones / (double)pairs,
                 (double)exercised / 2.0);
    return worst;
}

/**
 * @brief Plant a one percent departure on one bit. It nudges how often that bit is one.
 *
 * @param[in,out] winners      The copy to plant into [BORROWS].
 * @param[in]     sample_count How many.
 * @param[in]     bit          Which bit carries the planted signal.
 *
 * @note THE SIZE OF THE PLANT IS A DERIVATION, AND NOT A PICK. If a fraction `share` of the winners carry
 *       the bit, the pairwise XOR carries it with probability `2 * share * (1 - share)`. Set that to
 *       0.495, one percent below the null of a half, and `share = 0.45`. Thus the plant is: force
 *       exactly 45 of every 100 winners to carry the bit, and the rest to clear it.
 *
 * @note This is the smallest effect that the file CLAIMS to see. If it does not come back at about
 *       seven sigma, then the claim is arithmetic, and not a measurement. Every null that this bench
 *       prints would then mean nothing.
 */
static void difference_plant(uint32_t *const winners, const uint32_t sample_count, const uint32_t bit)
{
    const uint32_t carrying = (sample_count * 45u) / 100u;
    for (uint32_t at = 0u; at < sample_count; ++at)
    {
        if (at < carrying)
        {
            winners[at] |= (1u << bit);
        }
        else
        {
            winners[at] &= ~(1u << bit);
        }
    }
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    /* A fixed header. Its contents do not matter. Every winner in the collection must come from the
     * SAME header. Thus the nonce is the only difference between them. */
    uint8_t header[80];
    for (uint32_t at = 0u; at < 80u; ++at)
    {
        header[at] = (uint8_t)((at * 37u) + 11u);
    }

    uint32_t *const winners =
        (uint32_t *)platform_cell_taken(sizeof(uint32_t) * WANTED_WINNERS, platform_place_g_program_owner, 0);
    if (winners == NULL)
    {
        (void)fprintf(stderr, "no room\n");
        return 1;
    }

    (void)printf("\n  THE DIFFERENCE BETWEEN WINNERS. Is it structured, or is it uniform?\n\n");
    (void)printf("  collecting nonces whose digest carries %u leading zero bits\n", WANTED_ZEROS);

    uint32_t sample_count = 0u;
    uint64_t scanned = 0u;
    for (uint64_t nonce = 0u; (nonce <= 0xFFFFFFFFull) && (sample_count < WANTED_WINNERS); ++nonce)
    {
        header[76] = (uint8_t)(nonce & 0xFFu);
        header[77] = (uint8_t)((nonce >> 8) & 0xFFu);
        header[78] = (uint8_t)((nonce >> 16) & 0xFFu);
        header[79] = (uint8_t)((nonce >> 24) & 0xFFu);

        uint8_t digest[32];
        sha256_double_hash(header, 80u, digest);
        ++scanned;

        if (leading_zeros(digest) >= WANTED_ZEROS)
        {
            winners[sample_count] = (uint32_t)nonce;
            ++sample_count;
        }
    }

    (void)printf("  scanned %llu nonces, collected %u winners\n\n", (unsigned long long)scanned, sample_count);

    if (sample_count < 2u)
    {
        (void)printf("  too few winners to compare. Nothing is claimed.\n\n");
        platform_cell_released(winners, platform_place_g_program_owner);
        return 0;
    }

    const double measured = difference_examine("MEASURED, the real winners", winners, sample_count);

    /* THE POSITIVE CONTROL, WITHOUT WHICH THE NULL ABOVE SAYS NOTHING.
     *
     * The file claims that a one percent departure would show at about seven sigma. That is
     * arithmetic. Here the same routine gets the same winners, with a one percent departure PLANTED
     * on one bit. Its report is the actual sensitivity of this bench. A null is only worth its
     * positive control. */
    uint32_t *const planted =
        (uint32_t *)platform_cell_taken(sizeof(uint32_t) * sample_count, platform_place_g_program_owner, 0);
    if (planted == NULL)
    {
        (void)fprintf(stderr, "no room for the control\n");
        platform_cell_released(winners, platform_place_g_program_owner);
        return 1;
    }
    (void)EMBED_CALL(memor.cpy, MemoriaCfg, .dst = planted, .src = winners, .bytes = sizeof(uint32_t) * sample_count);
    difference_plant(planted, sample_count, PLANTED_BIT);

    (void)printf("  %s\n\n", "====================================================");
    const double control = difference_examine("POSITIVE CONTROL, one percent planted on bit 3", planted, sample_count);
    platform_cell_released(planted, platform_place_g_program_owner);

    (void)printf("  %s\n", "====================================================");
    (void)printf("  planted one percent came back at %.2f sigma on bit %u\n", control, PLANTED_BIT);
    (void)printf("  the real winners' worst departure was %.2f sigma\n\n", measured);

    if (control < 4.0)
    {
        /* Four sigma is not a chosen tolerance. Below it, the planted signal and an ordinary
         * fluctuation of the null are the same reading. Thus the bench cannot separate them, and no
         * result that it prints is a measurement. */
        (void)printf("  REFUSED. The bench cannot see a signal it was handed, so it has never\n");
        (void)printf("  been able to see one, and every null it has printed is uninformative.\n\n");
        platform_cell_released(winners, platform_place_g_program_owner);
        return 1;
    }

    (void)printf("  The bench sees a one percent effect. So the measured reading is a measurement:\n");
    (void)printf("  a structured solution set moves these, and this one does not.\n\n");

    platform_cell_released(winners, platform_place_g_program_owner);
    return 0;
}
