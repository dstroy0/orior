// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// povm_state_number.cu: exact numbers, frames and answers
#include "povm_state_internal.h"

static PovmNumber povm_number(SimRational rational, SimRational root)
{
    PovmNumber value;
    value.rational = rational;
    value.root = root;
    return value;
}

PovmNumber povm_fraction(long long numerator, long long denominator)
{
    return povm_number(sim_rational(numerator, denominator), sim_rational(0ll, 1ll));
}

PovmNumber povm_number_sum(PovmNumber left, PovmNumber right)
{
    return povm_number(sim_rational_sum(left.rational, right.rational), sim_rational_sum(left.root, right.root));
}

PovmNumber povm_number_difference(PovmNumber left, PovmNumber right)
{
    return povm_number(sim_rational_difference(left.rational, right.rational),
                       sim_rational_difference(left.root, right.root));
}

// (a + b sqrt 3)(c + d sqrt 3) = (ac + 3bd) + (ad + bc) sqrt 3: the square root's defining relation
PovmNumber povm_number_product(PovmNumber left, PovmNumber right)
{
    const SimRational roots = sim_rational_product(left.root, right.root);
    const SimRational rational = sim_rational_sum(sim_rational_product(left.rational, right.rational),
                                                  sim_rational_product(sim_rational(3ll, 1ll), roots));
    const SimRational root = sim_rational_sum(sim_rational_product(left.rational, right.root),
                                              sim_rational_product(left.root, right.rational));
    return povm_number(rational, root);
}

// the sign of a + b sqrt 3, exactly: with opposite signs the larger of a^2 and 3 b^2 decides, and they are never
// equal unless both are 0, since sqrt 3 is irrational
int povm_number_sign(PovmNumber value)
{
    const int rational = sim_rational_sign(value.rational);
    const int root = sim_rational_sign(value.root);
    if ((root == 0) || (rational == root))
    {
        return rational;
    }
    if (rational == 0)
    {
        return root;
    }
    const SimRational square = sim_rational_product(value.rational, value.rational);
    const SimRational root_square =
        sim_rational_product(sim_rational(3ll, 1ll), sim_rational_product(value.root, value.root));
    return (sim_rational_sign(sim_rational_difference(square, root_square)) > 0) ? rational : root;
}

// 1 and sqrt 3 are independent over Q. Two numbers are equal exactly when both parts are
int povm_number_equal(PovmNumber left, PovmNumber right)
{
    return sim_rational_equal(left.rational, right.rational) && sim_rational_equal(left.root, right.root);
}

void povm_number_print(VerbaLinea *line, PovmNumber value)
{
    const int root_sign = sim_rational_sign(value.root);
    if (root_sign == 0)
    {
        sim_rational_print(line, value.rational);
        return;
    }
    if (sim_rational_sign(value.rational) != 0)
    {
        sim_rational_print(line, value.rational);
        mmgr_line_put(line, (root_sign > 0) ? " + " : " - ");
        sim_rational_print(line, (root_sign > 0) ? value.root : sim_rational_negative(value.root));
    }
    else
    {
        sim_rational_print(line, value.root);
    }
    mmgr_line_put(line, " sqrt3");
}

static const long long s_povm_signs[POVM_OUTCOMES][POVM_AXES] = {
    {1ll, 1ll, 1ll}, {1ll, -1ll, -1ll}, {-1ll, 1ll, -1ll}, {-1ll, -1ll, 1ll}};

void povm_frame_rational(PovmFrame *frame)
{
    frame->name = "rational tetrahedral POVM, v = (+-1, +-1, +-1) / 2";
    for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
    {
        for (unsigned int axis = 0u; axis < POVM_AXES; axis += 1u)
        {
            frame->vector[outcome][axis] = povm_fraction(s_povm_signs[outcome][axis], 2ll);
        }
    }
}

// (+-1) / sqrt 3 = (+-1/3) sqrt 3
void povm_frame_sic(PovmFrame *frame)
{
    frame->name = "the SIC, v = (+-1, +-1, +-1) / sqrt3, in Q(sqrt3)";
    for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
    {
        for (unsigned int axis = 0u; axis < POVM_AXES; axis += 1u)
        {
            frame->vector[outcome][axis] =
                povm_number(sim_rational(0ll, 1ll), sim_rational(s_povm_signs[outcome][axis], 3ll));
        }
    }
}

PovmNumber povm_dot(const PovmNumber *left, const PovmNumber *right)
{
    PovmNumber total = povm_fraction(0ll, 1ll);
    for (unsigned int axis = 0u; axis < POVM_AXES; axis += 1u)
    {
        total = povm_number_sum(total, povm_number_product(left[axis], right[axis]));
    }
    return total;
}

// it is a POVM, and a complete one whose frame sum v v^T is c I. A state comes back as r = (4 / c) sum p v
int povm_frame_open(SimResults *results, PovmFrame *frame, PovmNumber *length_square)
{
    int sums_zero = 1;
    for (unsigned int axis = 0u; axis < POVM_AXES; axis += 1u)
    {
        PovmNumber total = povm_fraction(0ll, 1ll);
        for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
        {
            total = povm_number_sum(total, frame->vector[outcome][axis]);
        }
        sums_zero = sums_zero && (povm_number_sign(total) == 0);
    }
    sim_check(results, sums_zero, "the POVM's operators sum to the identity (the vectors sum to 0)");
    int positive = 1;
    for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
    {
        const PovmNumber square = povm_dot(frame->vector[outcome], frame->vector[outcome]);
        positive = positive && (povm_number_sign(povm_number_difference(povm_fraction(1ll, 1ll), square)) >= 0);
        *length_square = square;
    }
    sim_check(results, positive, "every operator of the POVM is positive (|v|^2 at most 1)");
    int identity = 1;
    PovmNumber diagonal = povm_fraction(0ll, 1ll);
    for (unsigned int row = 0u; row < POVM_AXES; row += 1u)
    {
        for (unsigned int column = 0u; column < POVM_AXES; column += 1u)
        {
            PovmNumber entry = povm_fraction(0ll, 1ll);
            for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
            {
                entry = povm_number_sum(
                    entry, povm_number_product(frame->vector[outcome][row], frame->vector[outcome][column]));
            }
            if (row == column)
            {
                identity = identity && ((row == 0u) || povm_number_equal(entry, diagonal));
                diagonal = entry;
            }
            else
            {
                identity = identity && (povm_number_sign(entry) == 0);
            }
        }
    }
    identity = identity && (sim_rational_sign(diagonal.root) == 0) && (sim_rational_sign(diagonal.rational) != 0);
    sim_check(results, identity, "the POVM's frame is a multiple of the identity, so it is complete");
    frame->factor = sim_rational(0ll, 1ll);
    if (identity != 0)
    {
        frame->factor = sim_rational_product(sim_rational(4ll, 1ll), sim_rational_reciprocal(diagonal.rational));
    }
    return identity;
}

void povm_answers(const PovmFrame *frame, const PovmNumber *bloch, PovmNumber *answer)
{
    for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
    {
        answer[outcome] = povm_number_product(
            povm_fraction(1ll, 4ll), povm_number_sum(povm_fraction(1ll, 1ll), povm_dot(bloch, frame->vector[outcome])));
    }
}

void povm_state_from(const PovmFrame *frame, const PovmNumber *answer, PovmNumber *bloch)
{
    const PovmNumber factor = povm_number(frame->factor, sim_rational(0ll, 1ll));
    for (unsigned int axis = 0u; axis < POVM_AXES; axis += 1u)
    {
        PovmNumber total = povm_fraction(0ll, 1ll);
        for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
        {
            total = povm_number_sum(total, povm_number_product(answer[outcome], frame->vector[outcome][axis]));
        }
        bloch[axis] = povm_number_product(factor, total);
    }
}

// the crossing rule's weight for outcome i on the POVM "+axis": 1/2 + (c^-1 . 2) v_i, from sum p = 1
PovmNumber povm_weight(const PovmFrame *frame, unsigned int outcome, unsigned int axis)
{
    const PovmNumber half_factor =
        povm_number(sim_rational_product(frame->factor, sim_rational(1ll, 2ll)), sim_rational(0ll, 1ll));
    return povm_number_sum(povm_fraction(1ll, 2ll), povm_number_product(half_factor, frame->vector[outcome][axis]));
}

PovmNumber povm_cross(const PovmFrame *frame, const PovmNumber *answer, unsigned int axis)
{
    PovmNumber total = povm_fraction(0ll, 1ll);
    for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
    {
        total = povm_number_sum(total, povm_number_product(answer[outcome], povm_weight(frame, outcome, axis)));
    }
    return total;
}

int povm_is_probability(PovmNumber value)
{
    return (povm_number_sign(value) >= 0) &&
           (povm_number_sign(povm_number_difference(povm_fraction(1ll, 1ll), value)) >= 0);
}

int povm_valid(const PovmFrame *frame, const PovmNumber *answer)
{
    PovmNumber bloch[POVM_AXES];
    povm_state_from(frame, answer, bloch);
    return povm_number_sign(povm_number_difference(povm_fraction(1ll, 1ll), povm_dot(bloch, bloch))) >= 0;
}
