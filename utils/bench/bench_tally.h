// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_tally.h
 * @brief How often each 32-bit value appears among a count of values. The values sort once, then read as runs.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The caller holds the values and the room of the sort, each a static table at the bound of its draws. The
 *       sort orders the values in place. Each run is then one value and its count, in ascending order of value.
 */

#ifndef BENCH_TALLY_H
#define BENCH_TALLY_H

#include "memoria_operor/memoria_operor.h"

#include <stddef.h>
#include <stdint.h>

/** @brief One value and how often it appears. */
typedef struct
{
    uint32_t value;
    uint32_t count;
} BenchTallyRun;

/**
 * @brief Returns the order of two values, for memor.sort.
 *
 * @param[in] left  One value [BORROWS].
 * @param[in] right The other value [BORROWS].
 * @return          -1, 0 or 1 as the first is below, equal to or above the second.
 */
static inline int bench_tally_order(const void *left, const void *right)
{
    const uint32_t one = *(const uint32_t *)left;
    const uint32_t other = *(const uint32_t *)right;
    return (one < other) ? -1 : ((one > other) ? 1 : 0);
}

/**
 * @brief Sorts `count` values in place.
 *
 * @param[in,out] values  The values [BORROWS].
 * @param[in]     scratch Room for `count` values, which the sort writes [BORROWS].
 * @param[in]     count   The count of values.
 */
static inline void bench_tally_sorted(uint32_t *values, uint32_t *scratch, size_t count)
{
    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = values, .bytes = sizeof(uint32_t), .scratch = scratch,
                     .count = count, .order = bench_tally_order);
}

/**
 * @brief Returns the run of equal values that starts at `*at` among sorted values, and moves `*at` past it.
 *
 * @param[in]     values The sorted values [BORROWS].
 * @param[in]     count  The count of values.
 * @param[in,out] at     The place of the run, below `count`; past the run on return.
 * @return               The run's value and its count.
 */
static inline BenchTallyRun bench_tally_run(const uint32_t *values, size_t count, size_t *at)
{
    const size_t first = *at;
    size_t end = first + 1u;
    while ((end < count) && (values[end] == values[first]))
    {
        end += 1u;
    }
    *at = end;
    // Explicit cast narrows the length of a run, at most the count of values, below 2^32 at each call site
    const BenchTallyRun run = {values[first], (uint32_t)(end - first)};
    return run;
}

#endif
