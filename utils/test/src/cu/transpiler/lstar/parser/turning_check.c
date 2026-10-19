// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// turning_check.c: the turning case of a ladder form held: the first case of its relation whose three readings in the
// register pass stand apart, so that no run is placed by a coincidence
//
//   apart    for every relation that has one, the case's words differ, the answer differs from both, and the relation
//            over each word twice differs from the second word and from the answer
//   up       the shift up's case is (3, 4): (1, 2) is passed over, 1 shifted by 1 being the second word, which would
//            read the amount's run as the destination; read swapped, (2, 1) stands
//   none     a relation with no such case gives none
//   predicate  a predicate form's turning cases are every case of its relation, in both orders, whose words differ
//            and over which it holds in the form's reading, one of them with a first word above 0
//   whole    a form's runs are whole where they carry every place of its relation's tuple
//   cut      the turned bits are gathered into runs cut where the fields the part names end: two adjacent turned bits
//            are one run where one field holds both, the narrowest where fields nest, or none holds either
//
//     turning_check
#include "../../../../../../../src/cu/transpiler/lstar/protocol/khw_learn/khw_learn_internal.h"

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

int main(void)
{
    for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        unsigned int word[2] = {0u, 0u};
        unsigned int expected = 0u;
        if (!khw_turning_words(anchor, 0, word, &expected))
        {
            continue;
        }
        const unsigned int first_twice[2] = {word[0], word[0]};
        const unsigned int second_twice[2] = {word[1], word[1]};
        unsigned int first_answer = 0u;
        unsigned int second_answer = 0u;
        const int read = ladder_answer(anchor, first_twice, 2u, &first_answer) &&
                         ladder_answer(anchor, second_twice, 2u, &second_answer);
        char what[160];
        snprintf(what, sizeof(what), "relation %u: the words (%u, %u) and the answer %u stand apart", anchor, word[0],
                 word[1], expected);
        check_that((word[0] != word[1]) && (expected != word[0]) && (expected != word[1]), what);
        snprintf(what, sizeof(what),
                 "relation %u: each word twice answers %u and %u, apart from the second word and the answer", anchor,
                 first_answer, second_answer);
        check_that(read && (first_answer != word[1]) && (second_answer != word[1]) && (first_answer != expected) &&
                       (second_answer != expected),
                   what);
    }
    unsigned int up[2] = {0u, 0u};
    unsigned int shifted = 0u;
    check_that(khw_turning_words((unsigned int)LADDER_UP, 0, up, &shifted) && (up[0] == 3u) && (up[1] == 4u) &&
                   (shifted == 48u),
               "the shift up's turning case is (3, 4) with the answer 48");
    unsigned int swapped[2] = {0u, 0u};
    check_that(khw_turning_words((unsigned int)LADDER_UP, 1, swapped, &shifted) && (swapped[0] == 2u) &&
                   (swapped[1] == 1u) && (shifted == 4u),
               "the shift up's turning case read swapped is (2, 1): 2 shifted by 2 and 1 by 1 are apart from 1 and 4");
    // a predicate form's turning cases: every case of its relation, in both orders, whose words differ and over
    // which the relation holds in the form's reading; at most, read plain, holds over (0, 1), (1, 2), (3, 7) and
    // (0, 0xffffffff), and a case whose first word is above 0 is among them, so that the second word moved to a
    // register holding 0 breaks the relation as the first word moved to one holding more than the second does
    const unsigned int at_most[4][2] = {{0u, 1u}, {1u, 2u}, {3u, 7u}, {0u, 0xffffffffu}};
    unsigned int cases = 0u;
    int every = 1;
    int raised = 0;
    unsigned int pair[2] = {0u, 0u};
    while (khw_predicate_words((unsigned int)LADDER_AT_MOST, 0, 0, cases, pair))
    {
        every = every && (cases < 4u) && (pair[0] == at_most[cases][0]) && (pair[1] == at_most[cases][1]);
        raised = raised || (pair[0] != 0u);
        cases += 1u;
    }
    check_that(every && (cases == 4u) && raised,
               "predicate: at most read plain turns over (0, 1), (1, 2), (3, 7) and (0, 0xffffffff), one first word "
               "above 0");
    unsigned int swapped_cases = 0u;
    int swapped_hold = 1;
    while (khw_predicate_words((unsigned int)LADDER_AT_MOST, 1, 0, swapped_cases, pair))
    {
        swapped_hold = swapped_hold && (pair[1] <= pair[0]) && (pair[0] != pair[1]);
        swapped_cases += 1u;
    }
    check_that((swapped_cases == 4u) && swapped_hold,
               "predicate: at most read swapped turns over the four cases with the words the other way round");
    unsigned int same_cases = 0u;
    while (khw_predicate_words((unsigned int)LADDER_SAME, 0, 0, same_cases, pair))
    {
        same_cases += 1u;
    }
    check_that(same_cases == 0u, "predicate: the same relation holds over no case whose words differ");
    check_that(!khw_predicate_words((unsigned int)LADDER_ADD, 0, 0, 0u, pair),
               "predicate: a relation that is no predicate has no predicate turning case");
    // a form is whole where its runs carry every place of its relation's tuple, the words and the answer
    const unsigned int whole[3] = {0u, 1u, 2u};
    const unsigned int short_of_one[4] = {2u, 0u, 2u, KHW_PLACE_NONE};
    const unsigned int none[2] = {KHW_PLACE_NONE, KHW_PLACE_NONE};
    const unsigned int twice[4] = {0u, 1u, 2u, KHW_PLACE_NONE};
    check_that(khw_places_whole(whole, 3u, 2u), "runs at every place of a two-word relation are whole");
    check_that(!khw_places_whole(short_of_one, 4u, 2u), "runs short of a word's place are not whole");
    check_that(!khw_places_whole(none, 2u, 2u), "runs with no place are not whole");
    check_that(khw_places_whole(twice, 4u, 2u), "a run with no place beside the whole places leaves them whole");
    check_that(!khw_places_whole(whole, 0u, 2u), "no runs are not whole");
    // the turned bits 12 to 16 and 24 to 27 under a guard field at 12 of 3 bits and a destination field at 16 of 8:
    // the guard's bits, the bit no field holds, the destination's one bit and the second field's run
    int turned[KHW_TURN_LAST + 1u];
    memset(turned, 0, sizeof(turned));
    for (unsigned int bit = 12u; bit <= 16u; bit += 1u)
    {
        turned[bit] = 1;
    }
    for (unsigned int bit = 24u; bit <= 27u; bit += 1u)
    {
        turned[bit] = 1;
    }
    const unsigned int field_first[2] = {12u, 16u};
    const unsigned int field_bits[2] = {3u, 8u};
    KhwAnswered cut;
    memset(&cut, 0, sizeof(cut));
    check_that((khw_runs_gathered(turned, 2u, field_first, field_bits, &cut) == 4u) && (cut.runs == 4u) &&
                   (cut.first[0] == 12u) && (cut.last[0] == 14u) && (cut.first[1] == 15u) && (cut.last[1] == 15u) &&
                   (cut.first[2] == 16u) && (cut.last[2] == 16u) && (cut.first[3] == 24u) && (cut.last[3] == 27u),
               "cut: turned bits 12 to 16 under the guard and destination fields are three runs, and 24 to 27 one");
    check_that((cut.place[0] == KHW_PLACE_NONE) && (cut.kind[0] == KHW_KIND_REGISTER) &&
                   (cut.place[2] == KHW_PLACE_NONE) && (cut.kind[2] == KHW_KIND_REGISTER),
               "cut: each run gathered holds no place yet and a register's kind");
    KhwAnswered uncut;
    memset(&uncut, 0, sizeof(uncut));
    check_that((khw_runs_gathered(turned, 0u, field_first, field_bits, &uncut) == 2u) && (uncut.first[0] == 12u) &&
                   (uncut.last[0] == 16u) && (uncut.first[1] == 24u) && (uncut.last[1] == 27u),
               "cut: with no fields named, adjacent turned bits are one run");
    // the second word's field at 32 of 8 bits inside the immediate's at 32 of 32: the narrowest holds the bit, and
    // turned bits 36 to 42 are the second word's 36 to 39 and the immediate's 40 to 42
    memset(turned, 0, sizeof(turned));
    for (unsigned int bit = 36u; bit <= 42u; bit += 1u)
    {
        turned[bit] = 1;
    }
    const unsigned int nested_first[2] = {32u, 32u};
    const unsigned int nested_bits[2] = {32u, 8u};
    KhwAnswered nested;
    memset(&nested, 0, sizeof(nested));
    check_that((khw_runs_gathered(turned, 2u, nested_first, nested_bits, &nested) == 2u) && (nested.first[0] == 36u) &&
                   (nested.last[0] == 39u) && (nested.first[1] == 40u) && (nested.last[1] == 42u),
               "cut: where fields nest, the narrowest holding a bit is the bit's field");
    printf("turning_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
