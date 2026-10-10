// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// survivors_check.c: the gate held to P2 on the ladder's own relations, each a candidate against each as the relation
//
//   conjunction  a candidate survives only where it fails no case, and the relation itself always does
//   order        every order of the cases leaves the same survivors
//   descent      the descent puts first a case the most candidates fail, and its price is what the price of that
//                order counts
//   no worse     the descent's price is no more than the price of the cases in the order the ladder holds them
#include "../../../../../../../src/cu/transpiler/lstar/protocol/counterexample/ladder.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/gate/survivors.h"

#include <stdio.h>
#include <string.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

static unsigned char s_fails[GATE_CANDIDATES][GATE_CASES];

#define LADDER_TEXT(name_, text_, words_, measured_) text_,
static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};
#undef LADDER_TEXT

int main(void)
{
    unsigned int asked = 0u;
    unsigned int prices_no_worse = 0u;
    for (unsigned int relation = 0u; relation < (unsigned int)LADDER_ANCHOR_COUNT; relation += 1u)
    {
        if (s_ladder_measured[relation] != 0)
        {
            continue;
        }
        // the relation's own cases, every relation of the ladder a candidate on them
        unsigned int cases = 0u;
        unsigned int word[GATE_CASES][2];
        for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
        {
            if ((s_ladder_cases[at].anchor == relation) && (s_ladder_cases[at].words == 2u))
            {
                word[cases][0] = s_ladder_cases[at].word[0];
                word[cases][1] = s_ladder_cases[at].word[1];
                cases += 1u;
            }
        }
        unsigned int candidates = 0u;
        unsigned int own = GATE_CANDIDATES;
        for (unsigned int candidate = 0u; candidate < (unsigned int)LADDER_ANCHOR_COUNT; candidate += 1u)
        {
            if (s_ladder_measured[candidate] != 0)
            {
                continue;
            }
            own = (candidate == relation) ? candidates : own;
            for (unsigned int at = 0u; at < cases; at += 1u)
            {
                unsigned int wanted = 0u;
                unsigned int given = 0u;
                ladder_answer(relation, word[at], 2u, &wanted);
                ladder_answer(candidate, word[at], 2u, &given);
                s_fails[candidates][at] = (given != wanted) ? 1u : 0u;
            }
            candidates += 1u;
        }
        unsigned char survives[GATE_CANDIDATES];
        const unsigned int count = gate_survivors(s_fails, candidates, cases, survives);
        check_that(survives[own] == 1u, "the relation itself survives its own cases");
        check_that(count >= 1u, "and the survivors hold at least it");

        // the cases reversed leave the same survivors
        unsigned char reversed_fails[GATE_CANDIDATES][GATE_CASES];
        for (unsigned int candidate = 0u; candidate < candidates; candidate += 1u)
        {
            for (unsigned int at = 0u; at < cases; at += 1u)
            {
                reversed_fails[candidate][at] = s_fails[candidate][cases - 1u - at];
            }
        }
        unsigned char reversed_survives[GATE_CANDIDATES];
        gate_survivors(reversed_fails, candidates, cases, reversed_survives);
        check_that(memcmp(survives, reversed_survives, candidates) == 0, "every order of the cases leaves one S");

        unsigned int order[GATE_CASES];
        const unsigned long long price = gate_descent(s_fails, candidates, cases, order);
        check_that(price == gate_price(s_fails, candidates, cases, order), "the descent's price is its order's");
        unsigned int most = 0u;
        unsigned int first = 0u;
        for (unsigned int at = 0u; at < cases; at += 1u)
        {
            unsigned int failing = 0u;
            for (unsigned int candidate = 0u; candidate < candidates; candidate += 1u)
            {
                failing += s_fails[candidate][at];
            }
            most = (failing > most) ? failing : most;
            first = (at == order[0]) ? failing : first;
        }
        check_that(first == most, "the descent puts first a case the most candidates fail");
        unsigned int held_order[GATE_CASES];
        for (unsigned int at = 0u; at < cases; at += 1u)
        {
            held_order[at] = at;
        }
        prices_no_worse += (price <= gate_price(s_fails, candidates, cases, held_order)) ? 1u : 0u;
        asked += 1u;
        printf("  %-8s %u cases, %u candidates, %u surviving, the descent's price %llu against %llu in the ladder's "
               "order\n",
               s_anchor_text[relation], cases, candidates, count, price,
               gate_price(s_fails, candidates, cases, held_order));
    }
    check_that(prices_no_worse == asked, "the descent's price is no more than the ladder's order's on every relation");
    printf("survivors_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
