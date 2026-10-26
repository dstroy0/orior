// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// povm_state_main.cu: the states, the frame run and main
#include "povm_state_internal.h"

static unsigned int povm_states(PovmState *state)
{
    unsigned int count = 0u;
    // pure states on the equator from rational t: ((1 - t^2) / (1 + t^2), 2t / (1 + t^2), 0), t = n / d
    const long long turn[13][2] = {{0ll, 1ll},  {1ll, 3ll}, {1ll, 2ll}, {2ll, 3ll},  {1ll, 1ll},
                                   {3ll, 2ll},  {2ll, 1ll}, {3ll, 1ll}, {-1ll, 2ll}, {-1ll, 1ll},
                                   {-2ll, 1ll}, {1ll, 5ll}, {4ll, 7ll}};
    for (unsigned int at = 0u; at < 13u; at += 1u)
    {
        const long long n = turn[at][0];
        const long long d = turn[at][1];
        state[count].bloch[0] = povm_fraction((d * d) - (n * n), (d * d) + (n * n));
        state[count].bloch[1] = povm_fraction(2ll * n * d, (d * d) + (n * n));
        state[count].bloch[2] = povm_fraction(0ll, 1ll);
        state[count].pure = 1;
        state[count].equator = 1;
        count += 1u;
    }
    state[count].bloch[0] = povm_fraction(-1ll, 1ll);
    state[count].bloch[1] = povm_fraction(0ll, 1ll);
    state[count].bloch[2] = povm_fraction(0ll, 1ll);
    state[count].pure = 1;
    state[count].equator = 1;
    count += 1u;
    // pure states off the equator: rational points of the sphere
    const long long sphere[7][4] = {{2ll, 2ll, 1ll, 3ll}, {1ll, -2ll, 2ll, 3ll}, {2ll, 3ll, 6ll, 7ll},
                                    {0ll, 0ll, 1ll, 1ll}, {0ll, 0ll, -1ll, 1ll}, {-2ll, 1ll, -2ll, 3ll},
                                    {6ll, 6ll, 7ll, 11ll}};
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        for (unsigned int axis = 0u; axis < POVM_AXES; axis += 1u)
        {
            state[count].bloch[axis] = povm_fraction(sphere[at][axis], sphere[at][3]);
        }
        state[count].pure = 1;
        state[count].equator = 0;
        count += 1u;
    }
    // mixed states: pure ones pulled toward the center, and the center itself
    const long long mixed[4][4] = {
        {1ll, 1ll, 1ll, 6ll}, {2ll, 3ll, 6ll, 21ll}, {3ll, 4ll, 0ll, 10ll}, {0ll, 0ll, 0ll, 1ll}};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        state[count].bloch[0] = povm_fraction(2ll * mixed[at][0], 2ll * mixed[at][3]);
        state[count].bloch[1] = povm_fraction(2ll * mixed[at][1], 2ll * mixed[at][3]);
        state[count].bloch[2] = povm_fraction(mixed[at][2], mixed[at][3]);
        state[count].pure = 0;
        state[count].equator = 0;
        count += 1u;
    }
    return count;
}

static void povm_frame_run(SimResults *results, PovmFrame *frame, const PovmState *state, unsigned int states)
{
    VerbaLinea *const line = &results->line;
    PovmNumber length_square = povm_fraction(0ll, 1ll);
    const int complete = povm_frame_open(results, frame, &length_square);
    mmgr_line_put(line, "\n  ");
    mmgr_line_put(line, frame->name);
    mmgr_line_put(line, "\n    |v|^2 = ");
    povm_number_print(line, length_square);
    mmgr_line_put(line, povm_number_equal(length_square, povm_fraction(1ll, 1ll)) ? " (rank one: a SIC)"
                                                                                   : " (not rank one)");
    mmgr_line_put(line, "; the frame is c I, so the state comes back as r = ");
    sim_rational_print(line, frame->factor);
    mmgr_line_put(line, " sum p_i v_i\n");
    if (complete == 0)
    {
        sim_flush(results);
        return;
    }
    unsigned int probabilities = 0u;
    unsigned int returned = 0u;
    unsigned int accepted = 0u;
    unsigned int pure = 0u;
    unsigned int on_boundary = 0u;
    unsigned int mixed = 0u;
    unsigned int inside = 0u;
    unsigned int crossings = 0u;
    // an exact number is four exact integers. The table is stored statically. It is not on the stack
    static PovmNumber equator_answer[POVM_STATES_MAX][POVM_OUTCOMES];
    unsigned int equators = 0u;
    unsigned int same_magnitudes = 0u;
    for (unsigned int at = 0u; at < states; at += 1u)
    {
        PovmNumber answer[POVM_OUTCOMES];
        povm_answers(frame, state[at].bloch, answer);
        PovmNumber total = povm_fraction(0ll, 1ll);
        int each = 1;
        for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
        {
            each = each && (povm_number_sign(answer[outcome]) >= 0);
            total = povm_number_sum(total, answer[outcome]);
        }
        probabilities += (each && povm_number_equal(total, povm_fraction(1ll, 1ll))) ? 1u : 0u;
        PovmNumber bloch[POVM_AXES];
        povm_state_from(frame, answer, bloch);
        int same = 1;
        for (unsigned int axis = 0u; axis < POVM_AXES; axis += 1u)
        {
            same = same && povm_number_equal(bloch[axis], state[at].bloch[axis]);
        }
        returned += same ? 1u : 0u;
        accepted += povm_valid(frame, answer) ? 1u : 0u;
        const PovmNumber square = povm_dot(bloch, bloch);
        if (state[at].pure != 0)
        {
            pure += 1u;
            on_boundary += povm_number_equal(square, povm_fraction(1ll, 1ll)) ? 1u : 0u;
        }
        else
        {
            mixed += 1u;
            inside += (povm_number_sign(povm_number_difference(povm_fraction(1ll, 1ll), square)) > 0) ? 1u : 0u;
        }
        for (unsigned int axis = 0u; axis < POVM_AXES; axis += 1u)
        {
            const PovmNumber direct = povm_number_product(
                povm_fraction(1ll, 2ll), povm_number_sum(povm_fraction(1ll, 1ll), state[at].bloch[axis]));
            crossings += povm_number_equal(povm_cross(frame, answer, axis), direct) ? 1u : 0u;
        }
        if (state[at].equator != 0)
        {
            // the 0/1 POVM reads z: every equator state answers 1/2, 1/2 there
            same_magnitudes += povm_number_equal(povm_cross(frame, answer, 2u), povm_fraction(1ll, 2ll)) ? 1u : 0u;
            for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
            {
                equator_answer[equators][outcome] = answer[outcome];
            }
            equators += 1u;
        }
    }
    unsigned int pairs = 0u;
    unsigned int distinct = 0u;
    for (unsigned int first = 0u; first < equators; first += 1u)
    {
        for (unsigned int second = first + 1u; second < equators; second += 1u)
        {
            int differ = 0;
            for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
            {
                differ = differ || !povm_number_equal(equator_answer[first][outcome], equator_answer[second][outcome]);
            }
            pairs += 1u;
            distinct += differ ? 1u : 0u;
        }
    }
    sim_check(results, probabilities == states, "every state's answers are a probability distribution");
    sim_check(results, returned == states, "state -> answers -> state returns every state exactly");
    sim_check(results, accepted == states, "the valid set accepts every state");
    sim_check(results, on_boundary == pure, "every pure state sits on the valid set's boundary, |r|^2 = 1");
    sim_check(results, inside == mixed, "every mixed state sits strictly inside");
    sim_check(results, crossings == (states * POVM_AXES),
              "the crossing rule reproduces the +x, +y and +z POVMs' answers");
    sim_check(results, same_magnitudes == equators, "every equator state answers 1/2, 1/2 to the 0/1 POVM");
    sim_check(results, distinct == pairs,
              "yet the complete POVM distinguishes every pair of them: the phase falls out");
    mmgr_line_put(line, "    ");
    mmgr_line_decimal(line, states, 1u);
    mmgr_line_put(line, " states (");
    mmgr_line_decimal(line, pure, 1u);
    mmgr_line_put(line, " pure, ");
    mmgr_line_decimal(line, mixed, 1u);
    mmgr_line_put(line, " mixed): answers a distribution ");
    mmgr_line_decimal(line, probabilities, 1u);
    mmgr_line_put(line, ", returned exactly ");
    mmgr_line_decimal(line, returned, 1u);
    mmgr_line_put(line, ", accepted ");
    mmgr_line_decimal(line, accepted, 1u);
    mmgr_line_put(line, ", pure on the boundary ");
    mmgr_line_decimal(line, on_boundary, 1u);
    mmgr_line_put(line, ", mixed inside ");
    mmgr_line_decimal(line, inside, 1u);
    mmgr_line_put(line, ", crossings matched ");
    mmgr_line_decimal(line, crossings, 1u);
    mmgr_line_put(line, " of ");
    mmgr_line_decimal(line, states * POVM_AXES, 1u);
    mmgr_line_put(line, "\n    the phase: ");
    mmgr_line_decimal(line, equators, 1u);
    mmgr_line_put(line, " equator states all answer 1/2, 1/2 to the 0/1 POVM; the complete POVM distinguishes ");
    mmgr_line_decimal(line, distinct, 1u);
    mmgr_line_put(line, " of their ");
    mmgr_line_decimal(line, pairs, 1u);
    mmgr_line_put(line, " pairs\n");

    int negative = 0;
    mmgr_line_put(line, "    the crossing weights onto +x:");
    for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
    {
        const PovmNumber weight = povm_weight(frame, outcome, 0u);
        negative = negative || (povm_number_sign(weight) < 0);
        mmgr_line_put(line, (outcome == 0u) ? " " : ", ");
        povm_number_print(line, weight);
    }
    mmgr_line_ch(line, '\n');
    sim_check(results, negative, "a crossing weight is negative, so the crossing is not classical total probability");

    PovmNumber corner[POVM_OUTCOMES];
    for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
    {
        corner[outcome] = povm_fraction((outcome == 0u) ? 1ll : 0ll, 1ll);
    }
    const PovmNumber corner_cross = povm_cross(frame, corner, 0u);
    sim_check(results, !povm_valid(frame, corner), "the distribution (1, 0, 0, 0) errors: it is no state");
    sim_check(results, !povm_is_probability(corner_cross), "and it crosses onto +x at a value that is no probability");
    mmgr_line_put(line, "    outside the valid set: (1, 0, 0, 0) errors, and crosses onto +x at ");
    povm_number_print(line, corner_cross);
    mmgr_line_ch(line, '\n');

    unsigned int grid = 0u;
    unsigned int valid = 0u;
    unsigned int valid_crossing = 0u;
    unsigned int invalid_axes_fine = 0u;
    for (long long first = 0ll; first <= POVM_GRID; first += 1ll)
    {
        for (long long second = 0ll; (first + second) <= POVM_GRID; second += 1ll)
        {
            for (long long third = 0ll; (first + second + third) <= POVM_GRID; third += 1ll)
            {
                const long long fourth = POVM_GRID - first - second - third;
                PovmNumber answer[POVM_OUTCOMES];
                answer[0] = povm_fraction(first, POVM_GRID);
                answer[1] = povm_fraction(second, POVM_GRID);
                answer[2] = povm_fraction(third, POVM_GRID);
                answer[3] = povm_fraction(fourth, POVM_GRID);
                int axes_fine = 1;
                for (unsigned int axis = 0u; axis < POVM_AXES; axis += 1u)
                {
                    axes_fine = axes_fine && povm_is_probability(povm_cross(frame, answer, axis));
                }
                grid += 1u;
                if (povm_valid(frame, answer))
                {
                    valid += 1u;
                    valid_crossing += axes_fine ? 1u : 0u;
                }
                else
                {
                    invalid_axes_fine += axes_fine ? 1u : 0u;
                }
            }
        }
    }
    sim_check(results, valid_crossing == valid, "every grid distribution in the valid set crosses to probabilities");
    mmgr_line_put(line, "    the grid of distributions with denominator 12 (");
    mmgr_line_decimal(line, grid, 1u);
    mmgr_line_put(line, "): ");
    mmgr_line_decimal(line, valid, 1u);
    mmgr_line_put(line, " are states; of the other ");
    mmgr_line_decimal(line, grid - valid, 1u);
    mmgr_line_put(line, ", ");
    mmgr_line_decimal(line, invalid_axes_fine, 1u);
    mmgr_line_put(line, " still cross to probabilities on the x, y and z POVMs yet are no state\n");
    sim_flush(results);
}

int main(void)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    VerbaLinea *const line = &results.line;
    mmgr_line_put(line,
                   "  the POVM and the state: a qubit carried exactly as the answers of a complete POVM, E_i = (I "
                   "+ v_i . sigma) / 4\n");
    mmgr_line_put(line, "  every value exact in Q(sqrt3): a square root is carried by its relation (sqrt3)^2 = 3\n");
    sim_flush(&results);

    static PovmState state[POVM_STATES_MAX];
    const unsigned int states = povm_states(state);
    int pure_listed = 1;
    for (unsigned int at = 0u; at < states; at += 1u)
    {
        if (state[at].pure != 0)
        {
            pure_listed =
                pure_listed && povm_number_equal(povm_dot(state[at].bloch, state[at].bloch), povm_fraction(1ll, 1ll));
        }
    }
    sim_check(&results, pure_listed, "every listed pure state has |r|^2 = 1 exactly");

    PovmFrame rational;
    PovmFrame sic;
    povm_frame_rational(&rational);
    povm_frame_sic(&sic);
    povm_frame_run(&results, &rational, state, states);
    povm_frame_run(&results, &sic, state, states);

    // the two complete POVMs cross into each other: the SIC's answers give the state, which gives the rational
    // POVM's answers, and they equal the rational POVM measured directly
    unsigned int crossed = 0u;
    for (unsigned int at = 0u; at < states; at += 1u)
    {
        PovmNumber sic_answer[POVM_OUTCOMES];
        PovmNumber bloch[POVM_AXES];
        PovmNumber through[POVM_OUTCOMES];
        PovmNumber direct[POVM_OUTCOMES];
        povm_answers(&sic, state[at].bloch, sic_answer);
        povm_state_from(&sic, sic_answer, bloch);
        povm_answers(&rational, bloch, through);
        povm_answers(&rational, state[at].bloch, direct);
        int same = 1;
        for (unsigned int outcome = 0u; outcome < POVM_OUTCOMES; outcome += 1u)
        {
            same = same && povm_number_equal(through[outcome], direct[outcome]);
        }
        crossed += same ? 1u : 0u;
    }
    sim_check(&results, crossed == states, "the SIC's answers cross into the rational POVM's exactly");
    mmgr_line_put(line, "\n  the two POVMs cross into each other on ");
    mmgr_line_decimal(line, crossed, 1u);
    mmgr_line_put(line, " of ");
    mmgr_line_decimal(line, states, 1u);
    mmgr_line_put(line, " states\n");
    sim_check(&results, g_sim_rational_wide == 0, "every value fit the exact integer's width");
    return sim_close(&results, "POVM and state");
}
