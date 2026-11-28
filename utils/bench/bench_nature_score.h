// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_nature_score.h
 * @brief The only place that turns difference histograms into bits of grip.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note Both the host sweep and the device sweep produce the same thing, a 256-bin histogram of
 *       differences. Both need it turned into bits above what the alphabet alone explains. That is
 *       two short functions. A second copy of them would disagree: failure mode fifteen.
 * @note The baseline is exact, and not sampled. It draws without replacement, because a shuffle
 *       draws two distinct positions, and so does the live reading. Against eight actual shuffles,
 *       the differential between the two languages comes out at -0.18 and +0.54 standard
 *       deviations: negligible. The noise of one shuffle can look like a bias of 0.0035 bits.
 */

#ifndef BENCH_NATURE_SCORE_H
#define BENCH_NATURE_SCORE_H

#include "memoria_operor/memoria_operor.h"
#include <math.h>
#include <stdint.h>

/**
 * @brief Collision entropy shortfall of a 256-bin distribution, in bits below flat.
 *
 * @param[in] counts 256 bin counts [BORROWS].
 * @param[in] total  Sum of the counts.
 * @return           Eight minus the collision entropy.
 * @note The unbiased estimator. The naive sum of squared frequencies has an upward bias at sparsity,
 *       and would report grip that is not there.
 */
static inline double bench_nature_shortfall(const uint64_t *counts, uint64_t total)
{
    if (total < 2u)
    {
        return 0.0;
    }

    long double collisions = 0.0L;
    for (unsigned bin = 0u; bin < 256u; bin += 1u)
    {
        const long double here = (long double)counts[bin];
        collisions += here * (here - 1.0L);
    }
    const long double possible = (long double)total * ((long double)total - 1.0L);
    if (collisions <= 0.0L)
    {
        return 8.0;
    }
    return 8.0 + log2((double)(collisions / possible));
}

/**
 * @brief What a shuffle of the same bytes would give, computed exactly.
 *
 * @param[in] marginal How often each byte value occurs [BORROWS].
 * @param[in] total    How many bytes.
 * @param[in] additive Nonzero for the integer difference, zero for exclusive-or.
 * @return             The baseline shortfall.
 * @note Without replacement, matching both a shuffle and the live reading, where positions i and
 *       i+lag are distinct and never one member counted twice.
 */
static inline double bench_nature_baseline(const uint64_t *marginal, uint64_t total, int additive)
{
    const double count = (double)total;
    const double pairs = count * (count - 1.0);

    double spread[256];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = spread, .bytes = sizeof(spread), .val = 0);
    for (unsigned left = 0u; left < 256u; left += 1u)
    {
        const double left_count = (double)marginal[left];
        if (left_count == 0.0)
        {
            continue;
        }
        for (unsigned right = 0u; right < 256u; right += 1u)
        {
            const double right_count = (double)marginal[right] - ((left == right) ? 1.0 : 0.0);
            if (right_count <= 0.0)
            {
                continue;
            }
            const unsigned at = (additive != 0) ? ((left - right) & 0xffu) : (left ^ right);
            spread[at] += (left_count * right_count) / pairs;
        }
    }

    double collisions = 0.0;
    for (unsigned bin = 0u; bin < 256u; bin += 1u)
    {
        collisions += spread[bin] * spread[bin];
    }
    return (collisions > 0.0) ? (8.0 + log2(collisions)) : 8.0;
}

#endif
