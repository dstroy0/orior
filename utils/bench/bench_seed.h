// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_seed.h
 * @brief The seed of a bench. An override tells a result from a draw.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note This is failure mode fourteen in theory/workbooks/orior/failure-modes.md. A statistic that
 *       appears at one seed and not at others is a property of the draw. Thirty-two of the fixed seeds in this
 *       tree are the same constant. A single seed cannot show whether a result moves with the draw.
 * @note The three-arm compiler audit establishes that these benches are deterministic: one seed
 *       gives one answer under three different builds. Determinism is not stability, and the two
 *       are easy to confuse. This override tests the second.
 * @note The override is an environment variable, and not an argument. The benches do not agree on
 *       an argument convention. A new convention across seventeen files would be a larger change
 *       than the thing under test.
 */

#ifndef BENCH_SEED_H
#define BENCH_SEED_H

#include <stdlib.h>

/**
 * @brief Returns the seed for the draw, from the environment where the variable exists.
 *
 * @param[in] when_unset The seed in the source of this bench, used where nothing overrides it.
 * @return               The seed to hand the generator.
 * @note With no override, the function returns the seed in the source. Thus every recorded result
 *       stays exactly reproducible. A sweep adds to the recorded results, and does not break them.
 */
static inline unsigned bench_seed(unsigned when_unset)
{
    const char *const requested = getenv("BENCH_SEED");
    if (requested == nullptr)
    {
        return when_unset;
    }

    const unsigned long parsed = strtoul(requested, nullptr, 10);
    // strtoul returns zero for text that is not a number. A bench with a seed from a typo would
    // look like a run, and not a mistake. Fall back to the seed in the source.
    return (parsed == 0ul) ? when_unset : (unsigned)parsed;
}

#endif
