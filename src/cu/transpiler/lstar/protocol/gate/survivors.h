// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef SURVIVORS_H
#define SURVIVORS_H

// The gate (P2). A relation is held on cases K = {k_1, ..., k_m}, and a candidate c survives where it fails no case:
//
//   S = {c : c(k) = r(k) for every k in K}
//
// The survivors are a conjunction: S does not depend on the order of K, and no order of the cases changes which
// candidates survive. The order is the price: putting K in order π costs Σ_c f_π(c) evaluations, f_π(c) the place of
// the first case c fails, counted from 1, and m for a survivor. The descent puts first the case the most remaining
// candidates fail. No cost enters S: a candidate that fails a case is wrong on every part, and no part is asked about
// it.
//
// What a candidate fails is the caller's to say, a row a candidate of `fails`, 1 at each case it fails. The gate reads
// those rows alone.

// the most candidates and the most cases the gate holds
#define GATE_CANDIDATES 64u
#define GATE_CASES 256u

// the survivors of `candidates` rows of `fails` over `cases` cases, 1 at each in `survives`. The count
unsigned int gate_survivors(const unsigned char fails[][GATE_CASES], unsigned int candidates, unsigned int cases,
                            unsigned char *survives);

// The descent's order of the `cases` cases into `order`, each next case the one the most candidates still standing
// fail, and the lower place first where two fail alike. The price of that order, Σ_c f_π(c)
unsigned long long gate_descent(const unsigned char fails[][GATE_CASES], unsigned int candidates, unsigned int cases,
                                unsigned int *order);

// the price Σ_c f_π(c) of putting the cases in `order`
unsigned long long gate_price(const unsigned char fails[][GATE_CASES], unsigned int candidates, unsigned int cases,
                              const unsigned int *order);

#endif
