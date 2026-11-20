// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_depth_score.h
 * @brief The only place that turns a sum of Hamming distances into a distance from saturation.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note Both the host scan and the device scan produce the same thing: a sum of Hamming distances,
 *       and a count. Both need it turned into standard errors from saturation. That is four lines of
 *       arithmetic. Failure mode fifteen in theory/workbooks/orior/failure-modes.md is a formula
 *       written twice.
 * @note The null is exact, and not fitted. Once a difference diffuses, an output word differs in a
 *       Binomial(32, 1/2) number of bits. Thus the mean sits at 16, with a standard deviation of
 *       sqrt(32)/2. Nothing about that depends on the function under test.
 */

#ifndef BENCH_DEPTH_SCORE_H
#define BENCH_DEPTH_SCORE_H

#include <cmath>

/** @brief Bits in the word whose Hamming distance the code reads. */
#define BENCH_DEPTH_WORD_BITS 32.0

/**
 * @brief How far a mean Hamming distance sits from saturation, at a stated word width.
 *
 * @param[in] total   Sum of the observed Hamming distances.
 * @param[in] counted How many observations went into that sum.
 * @param[in] bits    Bits per word, which sets the null.
 * @return            Distance from the saturated mean, in standard errors, or zero where the count
 *                    is zero.
 * @note A narrowed word saturates at bits/2, with deviation sqrt(bits)/2. Thus the null moves with the
 *       width. A fixed 32-bit null would report every row of a width sweep as biased.
 */
static inline double bench_depth_score_at_width(double total, double counted, double bits)
{
    if ((counted <= 0.0) || (bits <= 0.0))
    {
        return 0.0;
    }
    const double mean = total / counted;
    const double deviation = std::sqrt(bits) / 2.0;
    const double standard_error = deviation / std::sqrt(counted);
    return (mean - (bits / 2.0)) / standard_error;
}

/**
 * @brief The largest magnitude that this statistic can reach: a completely untouched word.
 *
 * @param[in] counted How many observations went into the sum.
 * @param[in] bits    Bits per word.
 * @return            The ceiling, or zero where the count is zero.
 * @note A word that no difference reached yet reads a mean distance of zero: the furthest from
 *       saturation that the statistic goes. Successive rounds all pin there, and read identically.
 *       Thus those rounds carry no decay information. Keep them out of any fitted slope. With them
 *       in, the fit flattens toward the ceiling, and not toward the trend.
 */
static inline double bench_depth_ceiling(double counted, double bits)
{
    if ((counted <= 0.0) || (bits <= 0.0))
    {
        return 0.0;
    }
    return (bits / 2.0) / ((std::sqrt(bits) / 2.0) / std::sqrt(counted));
}

/**
 * @brief How far a mean Hamming distance sits from saturation, in standard errors.
 *
 * @param[in] total   Sum of the observed Hamming distances.
 * @param[in] counted How many observations went into that sum.
 * @return            Distance from the saturated mean, in standard errors, or zero where the count
 *                    is zero.
 * @note Positive and negative are both significant. The caller takes the magnitude where it wants a
 *       depth. A difference that did not diffuse reads far below sixteen, because the output still
 *       resembles the input.
 */
static inline double bench_depth_score(double total, double counted)
{
    return bench_depth_score_at_width(total, counted, BENCH_DEPTH_WORD_BITS);
}

/**
 * @brief How far the difference rate of one bit position sits from a coin, in standard errors.
 *
 * @param[in] differed How often that position differed.
 * @param[in] counted  How many observations went into that count.
 * @return             Distance from one half, in standard errors, or zero where the count is zero.
 * @note The mean Hamming distance that a word reports is the sum of thirty-two of these. A pair of
 *       positions that lean opposite ways cancels there, and survives here. Thus this is a strict
 *       refinement, and not a second opinion. It sees everything that the word statistic sees, plus
 *       what a sum throws away.
 * @note The null is a coin. Thus the deviation is exactly one half. Like the word null, it does not
 *       depend on the function under test.
 */
static inline double bench_depth_bit_score(double differed, double counted)
{
    if (counted <= 0.0)
    {
        return 0.0;
    }
    const double rate = differed / counted;
    const double standard_error = 0.5 / std::sqrt(counted);
    return (rate - 0.5) / standard_error;
}

#endif
