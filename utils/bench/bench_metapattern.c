// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_metapattern.c
 * @brief Do winners relate ACROSS headers? The meta level.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * docs-check: quoting
 * @note Douglas: "It should be using Grover and shot to find metapatterns to give the
 *       shortest lock spin."
 * docs-check: end quoting
 *
 * Three questions, and only the third is open.
 *
 *   H27               Perturb ONE nonce whose digest carries k zeros. Do the zeros survive?
 *                     No. The sensitivity would catch 1.3x at five sigma.
 *
 *   bench_difference  Do winners WITHIN one header stand in any relation to each other?
 *                     No. Worst bit 1.62 sigma over 523776 pairs, popcount on the null to four
 *                     decimals.
 *
 *   HERE              Does the winner for one header predict anything about the winner for
 *                     ANOTHER? That is the meta level: a pattern across problems, and not inside
 *                     one. It would let a spin start somewhere better than zero.
 *
 * THE BENCH DRAWS THE NULL FROM THE PROCESS ITSELF. If each header is an independent memoryless
 * search, the first nonce to reach k leading zeros is geometric with mean 2^k. Thus:
 *
 *   - The mean first-winner is 2^k.
 *   - Its low bits are uniform. The residue of a geometric variable mod a power of two is flat.
 *   - No header bit correlates with any bit of its own winner.
 *
 * The third decides it. A metapattern is exactly a header bit that tells you something about where
 * its winner sits. The bench reports every correlation in sigma, against the sample actually taken.
 *
 * AND TWO WALLS, BECAUSE ONE READING ON ITS OWN IS NOT A MEASUREMENT.
 *
 * The search reports the WORST of many pairs. The worst of many draws is large, even when nothing
 * is there. An assumed floor "near 3 sigma" is a number that nobody measured. Thus the bench DRAWS
 * the floor. It runs the same search over the same numbers with the pairs of header and winner
 * destroyed. What that search reports is the floor.
 *
 * The other wall is the positive control. A search that cannot find a metapattern returns a null on
 * everything. Thus the bench plants one of known size, and the search must find it. It must land on
 * the planted pair, and not merely move a number. The control runs as a ladder from two sigma up.
 * Thus the output is the sensitivity that this bench HAS, and not the one that it claims.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sha256_core.h"

/** @brief Leading zero bits a first-winner must reach. Small, a header costs about 2^16 hashes. */
#define WANTED_ZEROS 16u

/** @brief How many distinct headers to search. Each contributes one first-winner. */
#define WANTED_HEADERS 1024u

/**
 * @brief How many bits of the winner the bench reads.
 *
 * @note Sixteen. The first-winner is geometric with mean `2^WANTED_ZEROS`. Bits above that mostly
 *       state how far the search ran, and not where the winner sits. `bench_difference` draws the
 *       same boundary, for the same reason: a read past it measures the sample.
 */
#define WINNER_BITS 16u

/** @brief The pair that the positive control plants on. Any pair does; nothing depends on it. */
#define PLANTED_HEADER_BIT 11u
#define PLANTED_WINNER_BIT 5u

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
 * @brief Every header bit against every winner bit. Returns the worst agreement found.
 *
 * @param[in]  seed         One tag per header [BORROWS].
 * @param[in]  winner       The first nonce that wins for each [BORROWS].
 * @param[in]  sample_count How many.
 * @param[out] worst_header Which header bit carried the worst departure [BORROWS].
 * @param[out] worst_winner Which winner bit it paired with [BORROWS].
 * @return                  That departure, in sigma.
 *
 * @note A function, and thus the drawn null and the planted control run THE SAME SEARCH as the
 *       measurement. A control examined by a second copy of the code says nothing about the copy
 *       that read the real data.
 */
static double metapattern_examine(const uint32_t *const seed, const uint32_t *const winner, const uint32_t sample_count,
                                  uint32_t *const worst_header, uint32_t *const worst_winner)
{
    // sqrt(sample_count)/2 by Newton. Thus the spread comes from the sample alone.
    double root = (double)sample_count;
    for (uint32_t step = 0u; step < 64u; ++step)
    {
        root = 0.5 * (root + ((double)sample_count / root));
    }
    const double sigma = root / 2.0;
    const double expected = (double)sample_count / 2.0;

    double worst = 0.0;
    *worst_header = 0u;
    *worst_winner = 0u;

    for (uint32_t header_bit = 0u; header_bit < 32u; ++header_bit)
    {
        for (uint32_t winner_bit = 0u; winner_bit < WINNER_BITS; ++winner_bit)
        {
            uint32_t agree = 0u;
            for (uint32_t at = 0u; at < sample_count; ++at)
            {
                const uint32_t left = (seed[at] >> header_bit) & 1u;
                const uint32_t right = (winner[at] >> winner_bit) & 1u;
                agree += (left == right) ? 1u : 0u;
            }
            const double off = ((double)agree - expected) / sigma;
            const double magnitude = (off < 0.0) ? -off : off;
            if (magnitude > worst)
            {
                worst = magnitude;
                *worst_header = header_bit;
                *worst_winner = winner_bit;
            }
        }
    }
    return worst;
}

/**
 * @brief Break the pairs of header and winner. Keep both sequences exactly as measured.
 *
 * @param[in]  winner       The winners as measured [BORROWS].
 * @param[out] shuffled     The same winners, against other headers [BORROWS].
 * @param[in]  sample_count How many.
 *
 * @note DRAW THE NULL, DO NOT DERIVE IT. The search reports the worst of `32 * WINNER_BITS` pairs.
 *       The worst of that many draws sits well above the spread of one draw. An assumed floor "near
 *       3 sigma" is a number that nobody measured. Thus destroy the pairs, and run the same search
 *       again. What it reports then IS the floor, on this very sample, with every marginal unchanged.
 *
 * @note A rotation by half the count is a permutation for any count. Thus no sample keeps its own
 *       winner, and the shuffle uses none twice.
 */
static void metapattern_shuffle(const uint32_t *const winner, uint32_t *const shuffled, const uint32_t sample_count)
{
    for (uint32_t at = 0u; at < sample_count; ++at)
    {
        shuffled[at] = winner[(at + (sample_count / 2u)) % sample_count];
    }
}

/**
 * @brief Plant a metapattern of a stated size: one winner bit made to agree with one header bit.
 *
 * @param[in]     seed       The header tags [BORROWS].
 * @param[in,out] winner     The winners, with one bit overwritten [BORROWS].
 * @param[in]     sample_count How many.
 * @param[in]     want_sigma   How large the planted agreement should be, in sigma.
 *
 * @note THE CALLER STATES THE SIZE IN SIGMA, AND THE FUNCTION CONVERTS IT HERE. Thus it does not
 *       depend on the sample size. An agreement fraction `share` sits
 *       `(share - 1/2) * sample_count / (sqrt(sample_count)/2)` sigma from the null. Thus
 *       `share = 1/2 + want_sigma / (2 * sqrt(sample_count))`.
 *
 * @note A mix of the index decides which samples carry the agreement. That mix is NOT the mix that
 *       the tags use. Thus the plant lands on the pair that it names. It does not quietly correlate
 *       the winner bit with some other header bit, where the search would recover it in the wrong
 *       place.
 */
static void metapattern_plant(const uint32_t *const seed, uint32_t *const winner, const uint32_t sample_count,
                              const double want_sigma)
{
    double root = (double)sample_count;
    for (uint32_t step = 0u; step < 64u; ++step)
    {
        root = 0.5 * (root + ((double)sample_count / root));
    }
    const double share = 0.5 + (want_sigma / (2.0 * root));
    const uint32_t threshold = (uint32_t)(share * 1000.0);

    for (uint32_t at = 0u; at < sample_count; ++at)
    {
        const uint32_t mixed = ((at + 1u) * 2246822519u) >> 11;
        const uint32_t agreeing = ((mixed % 1000u) < threshold) ? 1u : 0u;
        const uint32_t want = ((seed[at] >> PLANTED_HEADER_BIT) & 1u) ^ (agreeing ^ 1u);
        winner[at] = (winner[at] & ~(1u << PLANTED_WINNER_BIT)) | (want << PLANTED_WINNER_BIT);
    }
}

int main(void)
{
    uint32_t *const winner = (uint32_t *)malloc(sizeof(uint32_t) * WANTED_HEADERS);
    uint32_t *const seed = (uint32_t *)malloc(sizeof(uint32_t) * WANTED_HEADERS);
    if ((winner == NULL) || (seed == NULL))
    {
        free(winner);
        free(seed);
        (void)fprintf(stderr, "no room\n");
        return 1;
    }

    (void)printf("\n  METAPATTERNS ACROSS HEADERS. Does one search predict another?\n\n");

    uint64_t total_spin = 0u;
    uint32_t sample_count = 0u;

    for (uint32_t which = 0u; which < WANTED_HEADERS; ++which)
    {
        /* Each header differs in four bytes. Those four bytes are the `seed` that this test asks
         * about. If a metapattern exists, something in the header predicts something in its
         * winner. */
        uint8_t header[80];
        for (uint32_t at = 0u; at < 80u; ++at)
        {
            header[at] = (uint8_t)((at * 37u) + 11u);
        }
        const uint32_t tag = (which * 2654435761u);
        header[0] = (uint8_t)(tag & 0xFFu);
        header[1] = (uint8_t)((tag >> 8) & 0xFFu);
        header[2] = (uint8_t)((tag >> 16) & 0xFFu);
        header[3] = (uint8_t)((tag >> 24) & 0xFFu);

        for (uint64_t nonce = 0u; nonce <= 0xFFFFFFFFull; ++nonce)
        {
            header[76] = (uint8_t)(nonce & 0xFFu);
            header[77] = (uint8_t)((nonce >> 8) & 0xFFu);
            header[78] = (uint8_t)((nonce >> 16) & 0xFFu);
            header[79] = (uint8_t)((nonce >> 24) & 0xFFu);

            uint8_t digest[32];
            sha256_double_hash(header, 80u, digest);

            if (leading_zeros(digest) >= WANTED_ZEROS)
            {
                winner[sample_count] = (uint32_t)nonce;
                seed[sample_count] = tag;
                total_spin += (nonce + 1u);
                ++sample_count;
                break;
            }
        }
    }

    // THE MEAN SPIN, GRADED LIKE EVERYTHING ELSE. The header of the file claims three nulls, and
    // this is one of them. Thus it gets a sigma, and not two bare numbers for the reader to compare.
    // The first-winner is geometric with success `2^-k`. Its mean is `2^k`, and its standard
    // deviation is about `2^k`. Thus the spread on the MEAN of `sample_count` of them is
    // `2^k/sqrt(sample_count)`.
    {
        double spin_root = (double)sample_count;
        for (uint32_t step = 0u; step < 64u; ++step)
        {
            spin_root = 0.5 * (spin_root + ((double)sample_count / spin_root));
        }
        const double null_spin = (double)(1u << WANTED_ZEROS);
        const double seen_spin = (double)total_spin / (double)sample_count;
        const double spin_sigma = null_spin / spin_root;
        (void)printf("  %u headers searched, mean spin to first winner %.1f, the null is %.1f\n", sample_count,
                     seen_spin, null_spin);
        (void)printf("  that is %.2f sigma, against a spread of %.1f on the mean\n\n",
                     (seen_spin - null_spin) / spin_sigma, spin_sigma);
    }

    if (sample_count < 2u)
    {
        (void)printf("  too few to compare. Nothing is claimed.\n\n");
        free(winner);
        free(seed);
        return 0;
    }

    uint32_t *const scratch = (uint32_t *)malloc(sizeof(uint32_t) * sample_count);
    if (scratch == NULL)
    {
        free(winner);
        free(seed);
        (void)fprintf(stderr, "no room for the control\n");
        return 1;
    }

    (void)printf("  %u header-bit against winner-bit pairings, %u samples each.\n\n", 32u * WINNER_BITS, sample_count);

    uint32_t at_header = 0u;
    uint32_t at_winner = 0u;

    // THE METAPATTERN TEST. For each header bit and each winner bit, how often do they agree?
    // Independence says half. Anything else is a header bit that knows where its winner is.
    const double measured = metapattern_examine(seed, winner, sample_count, &at_header, &at_winner);
    (void)printf("  %-30s worst %5.2f sigma   header bit %2u / winner bit %2u\n", "MEASURED", measured, at_header,
                 at_winner);

    // The floor, drawn, and not asserted: the same search over the same numbers, with the pairs
    // destroyed. Its result is the result of this search when nothing is there.
    metapattern_shuffle(winner, scratch, sample_count);
    const double floor_sigma = metapattern_examine(seed, scratch, sample_count, &at_header, &at_winner);
    (void)printf("  %-30s worst %5.2f sigma   header bit %2u / winner bit %2u\n\n", "DRAWN NULL, pairing broken",
                 floor_sigma, at_header, at_winner);

    // THE POSITIVE CONTROL, AS A LADDER. Thus the bench states its sensitivity, and does not claim
    // one. A rung counts as recovered only if the search lands on the planted pair AND clears the
    // drawn floor. A number that moves is not the same as the right finding.
    (void)printf("  PLANTED LADDER, on header bit %u against winner bit %u.\n\n", PLANTED_HEADER_BIT,
                 PLANTED_WINNER_BIT);

    double sensitivity = 0.0;
    for (uint32_t rung = 2u; rung <= 7u; ++rung)
    {
        (void)memcpy(scratch, winner, sizeof(uint32_t) * sample_count);
        metapattern_plant(seed, scratch, sample_count, (double)rung);

        const double got = metapattern_examine(seed, scratch, sample_count, &at_header, &at_winner);
        const int recovered =
            ((at_header == PLANTED_HEADER_BIT) && (at_winner == PLANTED_WINNER_BIT) && (got > floor_sigma)) ? 1 : 0;
        if (recovered && (sensitivity == 0.0))
        {
            sensitivity = (double)rung;
        }
        (void)printf("    planted %u sigma  ->  worst %5.2f at header %2u / winner %2u   %s\n", rung, got, at_header,
                     at_winner, recovered ? "RECOVERED" : "lost in the floor");
    }

    (void)printf("\n  %s\n", "--------------------------------------------------------");

    if (sensitivity == 0.0)
    {
        (void)printf("  REFUSED. The search did not recover a metapattern it was handed, at any\n");
        (void)printf("  size up to seven sigma. It cannot see one, so it has never been able to,\n");
        (void)printf("  and the measured reading above is not evidence of anything.\n\n");
        free(winner);
        free(seed);
        free(scratch);
        return 1;
    }

    (void)printf("  SENSITIVITY: a planted metapattern is recovered from %.0f sigma up, against a\n", sensitivity);
    (void)printf("  drawn floor of %.2f sigma over %u pairings. MEASURED sits at %.2f.\n\n", floor_sigma,
                 32u * WINNER_BITS, measured);

    if (measured > floor_sigma)
    {
        (void)printf("  The measured reading is ABOVE the floor the null itself produces.\n");
        (void)printf("  Something is there and it is worth asking what.\n\n");
    }
    else
    {
        (void)printf("  The measured reading is at or below the floor the null itself produces,\n");
        (void)printf("  so no header bit knows anything about where its winner sits, and the spin\n");
        (void)printf("  cannot be started anywhere better than zero.\n\n");
    }

    free(winner);
    free(seed);
    free(scratch);
    return 0;
}
