// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the povm_state_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef POVM_STATE_INTERNAL_H
#define POVM_STATE_INTERNAL_H

// The POVM and the state: a qubit's state carried exactly as the answer distribution of a complete POVM.
// Two POVMs, E_i = (I + v_i . sigma) / 4 over four outcomes: a rational tetrahedral POVM, v_i = (+-1, +-1, +-1) / 2,
// all in Q; and the true SIC, v_i = (+-1, +-1, +-1) / sqrt 3, in Q(sqrt 3), where the square root is carried by its
// defining relation (sqrt 3)^2 = 3. Every value stays exact. Proved for each POVM: it is a
// POVM; state -> answers -> state returns the state; the phase falls out of the POVM; the valid set accepts every
// state and puts the pure ones on its boundary; the crossing rule reproduces the other POVMs' answers and carries
// a negative weight; a distribution outside the valid set crosses to a non-probability; the two POVMs cross into
// each other. Measured: how many of a grid of distributions are states.

#include "sim_rational.h"

#define POVM_OUTCOMES 4u

#define POVM_AXES 3u

#define POVM_GRID 12ll

#define POVM_STATES_MAX 32u

// rational + root . sqrt 3
typedef struct
{
    SimRational rational;
    SimRational root;
} PovmNumber;

typedef struct
{
    const char *name;
    PovmNumber vector[POVM_OUTCOMES][POVM_AXES];
    SimRational factor;
} PovmFrame;

typedef struct
{
    PovmNumber bloch[POVM_AXES];
    int pure;
    int equator;
} PovmState;

PovmNumber povm_fraction(long long numerator, long long denominator);

PovmNumber povm_number_sum(PovmNumber left, PovmNumber right);

PovmNumber povm_number_difference(PovmNumber left, PovmNumber right);

PovmNumber povm_number_product(PovmNumber left, PovmNumber right);

int povm_number_sign(PovmNumber value);

int povm_number_equal(PovmNumber left, PovmNumber right);

void povm_number_print(VerbaLinea *line, PovmNumber value);

void povm_frame_rational(PovmFrame *frame);

void povm_frame_sic(PovmFrame *frame);

PovmNumber povm_dot(const PovmNumber *left, const PovmNumber *right);

int povm_frame_open(SimResults *results, PovmFrame *frame, PovmNumber *length_square);

void povm_answers(const PovmFrame *frame, const PovmNumber *bloch, PovmNumber *answer);

void povm_state_from(const PovmFrame *frame, const PovmNumber *answer, PovmNumber *bloch);

PovmNumber povm_weight(const PovmFrame *frame, unsigned int outcome, unsigned int axis);

PovmNumber povm_cross(const PovmFrame *frame, const PovmNumber *answer, unsigned int axis);

int povm_is_probability(PovmNumber value);

int povm_valid(const PovmFrame *frame, const PovmNumber *answer);

#endif
