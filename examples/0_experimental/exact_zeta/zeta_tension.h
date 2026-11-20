// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_tension.h
 * @brief The zeros that the machine of Turing certifies, each placed, for the readings of how the zeros hold together.
 *
 * @note Each cell runs once on the device by the multiple evaluation, with every point listed. Its lattice is extra
 *       steps finer than four points a zero. The method of Turing certifies N at the F of the cells. Else the caller
 *       gives N where a run of the machine closes over them.
 * @note Every zero sits at a change of sign of the Z of the device between adjacent points. It is on the line through Z
 *       at the two. A close pair with no point between shows where |Z| dips without a change of sign. The code lists
 *       that dip again 64 times finer by pairs on the device. The zeros placed must number the difference of N between
 *       the two F.
 */
#ifndef ZETA_TENSION_H
#define ZETA_TENSION_H

#include "zeta_machine.h"

/** @brief The zeros from the first F where the method certifies N to the last, each placed. */
typedef struct
{
    double *zeros;    /**< The zeros in s = t / 2 pi, ascending [OWNS]. */
    size_t count;     /**< How many. */
    int64_t n_start;  /**< N at the first F. */
    int64_t start_nu; /**< The cell of the first F. */
    int64_t end_nu;   /**< The cell of the last F. */
} TensionZeros;

/**
 * @brief theta / pi at t = 2 pi s, the series of the point stage.
 *
 * @param[in] s The point.
 * @return      theta / pi.
 */
double tension_theta_pi(double s);

/**
 * @brief Every prime power p^r up to the limit, as (p, r), by p and then by r.
 *
 * @param[in]  limit  The limit.
 * @param[out] powers The prime powers [BORROWS]. Allocated.
 * @return            How many.
 */
size_t tension_prime_powers(int64_t limit, int64_t (**powers)[2]);

/**
 * @brief The zeros from the first F where the method certifies N to the last, every one placed.
 *
 * @param[in]  binary The device program [BORROWS].
 * @param[in]  first  The first cell.
 * @param[in]  last   The last cell.
 * @param[in]  extra  The steps finer than four points a zero.
 * @param[in]  given  N at the F of cells first and last, where a run of the machine closes over them, or NULL
 *                    [BORROWS].
 * @param[out] zeros  The zeros [BORROWS].
 * @note The program stops with a message where N holds one value at fewer than two F. It also stops where the zeros
 *       placed do not number the difference of N.
 */
void tension_certified_zeros(const char *binary, int64_t first, int64_t last, int64_t extra, const int64_t *given,
                             TensionZeros *zeros);

#endif
