// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// turning.c: the turning case of a ladder form: the first case of its relation whose readings in the register pass stand apart
#include "khw_write_internal.h"

// The turning case of the relation `anchor`, read swapped where `swapped`, into `word`, with the relation's answer to
// it in `expected`: the first case of the relation, in the order the ladder holds them, whose two words differ, whose
// answer differs from both, and whose answer to each word taken twice differs from the second word and from the
// answer. The register pass reads a run three ways over this case, as the destination where it leaves the second word,
// as the first source where it reads the second word twice, as the second where it reads the first twice: a case whose
// readings coincide would place a run by a coincidence, and is passed over. 1, or 0 where the relation holds no such
// case
int khw_turning_words(unsigned int anchor, int swapped, unsigned int word[2], unsigned int *expected)
{
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        const LadderQuestion *const question = &s_ladder_cases[at];
        unsigned int gave = 0u;
        if ((question->anchor != anchor) || (question->words < 2u) ||
            !ladder_answer(anchor, question->word, question->words, &gave) || (question->word[0] == question->word[1]) ||
            (gave == question->word[0]) || (gave == question->word[1]))
        {
            continue;
        }
        const unsigned int first = (swapped != 0) ? question->word[1] : question->word[0];
        const unsigned int second = (swapped != 0) ? question->word[0] : question->word[1];
        const unsigned int first_twice[2] = {first, first};
        const unsigned int second_twice[2] = {second, second};
        unsigned int first_answer = 0u;
        unsigned int second_answer = 0u;
        if (!ladder_answer(anchor, first_twice, 2u, &first_answer) || !ladder_answer(anchor, second_twice, 2u, &second_answer) ||
            (first_answer == second) || (second_answer == second) || (first_answer == gave) || (second_answer == gave))
        {
            continue;
        }
        word[0] = first;
        word[1] = second;
        *expected = gave;
        return 1;
    }
    return 0;
}
