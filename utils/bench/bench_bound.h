// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_bound.h
 * @brief The larger and the smaller of two values, as the benches compare them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note Each gives the first value where the two are equal, and where they do not compare, as a NaN does not.
 */

#ifndef BENCH_BOUND_H
#define BENCH_BOUND_H

/**
 * @brief Returns the larger of two values.
 *
 * @param[in] left  The first value.
 * @param[in] right The second value.
 * @return          `right` where `left` is below it, and `left` otherwise.
 */
template <typename Value> static inline Value bench_max(Value left, Value right)
{
    return (left < right) ? right : left;
}

/**
 * @brief Returns the smaller of two values.
 *
 * @param[in] left  The first value.
 * @param[in] right The second value.
 * @return          `right` where it is below `left`, and `left` otherwise.
 */
template <typename Value> static inline Value bench_min(Value left, Value right)
{
    return (right < left) ? right : left;
}

#endif
