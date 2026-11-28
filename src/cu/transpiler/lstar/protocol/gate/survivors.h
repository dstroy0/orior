// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef SURVIVORS_H
#define SURVIVORS_H

// The gate (P2). The gate checks a relation on cases `K = {k_1, ..., k_m}`, and a candidate c survives where it fails
// no case:
//
//   S = {c : c(k) = r(k) for every k in K}
//
// The survivors are a conjunction. S does not depend on the order of K, and no order of the cases changes which
// candidates survive.
//
// The order is the price. To put K in order π costs `Σ_c f_π(c)` evaluations, with `f_π(c)` the place of the first case
// that c fails, counted from 1, and m for a survivor. The descent puts first the case that the most candidates that
// remain fail. No cost enters S. A candidate that fails a case is wrong on every part, and the gate queries no part
// about it.
//
// The caller decides what a candidate fails, one row of `fails` for each candidate, with 1 at each case that it fails.
// The gate reads those rows alone.

// the max candidates and the max cases the gate supports
#define GATE_CANDIDATES 64u
#define GATE_CASES 256u

// the survivors of `candidates` rows of `fails` over `cases` cases, 1 at each in `survives`. The count
unsigned int gate_survivors(const unsigned char fails[][GATE_CASES], unsigned int candidates, unsigned int cases,
                            unsigned char *survives);

// The order of the descent of the `cases` cases into `order`. Each next case is the case that the most candidates that
// still survive fail, with the lower place first where two fail alike. The return is the price of that order,
// `Σ_c f_π(c)`
unsigned long long gate_descent(const unsigned char fails[][GATE_CASES], unsigned int candidates, unsigned int cases,
                                unsigned int *order);

// the price `Σ_c f_π(c)` of the cases put in `order`
unsigned long long gate_price(const unsigned char fails[][GATE_CASES], unsigned int candidates, unsigned int cases,
                              const unsigned int *order);

#endif
