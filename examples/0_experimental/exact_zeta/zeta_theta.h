// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_theta.h
 * @brief The theta function of Riemann and Siegel, by the series of Stirling, in two routes that must meet.
 *
 * @note theta(t) = Im ln Gamma(1/4 + it/2) - (t/2) ln pi. Let z = 1/4 + it/2 and w = z + M. Then
 *       ln Gamma(z) = ln Gamma(w) - sum over k < M of ln(z + k). The series of Stirling with K Bernoulli terms
 *       gives ln Gamma(w).
 * @note Each arg is an arctangent of a rational, by two series that must agree. One is the series of Euler, with
 *       its own pi from the identity of Euler. The other is the Taylor series about 1/2, with its own pi from the
 *       identity of Machin. Two routes run, at M = K = N and at 2N.
 */
#ifndef ZETA_THETA_H
#define ZETA_THETA_H

#include "zeta_exact.h"

/**
 * @brief theta at the places by the two routes, N and 2N, each read toward zero.
 *
 * @param[in]  t         The point [BORROWS].
 * @param[in]  places    The places.
 * @param[in]  n_sum     The cut N.
 * @param[in]  bernoulli 1 to keep the Bernoulli terms, 0 to drop them.
 * @param[out] one       The first route [BORROWS].
 * @param[out] two       The second route [BORROWS].
 */
void theta_routes(const DecimalPair *t, int32_t places, int64_t n_sum, int bernoulli, OriorExactInteger *one,
                  OriorExactInteger *two);

/**
 * @brief i pi read toward zero at the places that big_pi carries, less the guard.
 *
 * @param[in]  i      The multiple.
 * @param[in]  big_pi Pi at the places plus the guard [BORROWS].
 * @param[out] result The value [BORROWS].
 */
void theta_pi_times(int64_t i, const OriorExactInteger *big_pi, OriorExactInteger *result);

#endif
