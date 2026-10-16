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

// 1 where `runs` runs with the places `place` carry every place of a relation's tuple of `words` words and its answer,
// the places 0 to `words`: a form so placed writes its relation whole, and a word of the web can be written by it;
// a form short of a place is a fragment the widening found, and writes none
int khw_places_whole(const unsigned int *place, unsigned int runs, unsigned int words)
{
    for (unsigned int wanted = 0u; wanted <= words; wanted += 1u)
    {
        int found = 0;
        for (unsigned int run = 0u; run < runs; run += 1u)
        {
            found = found || (place[run] == wanted);
        }
        if (!found)
        {
            return 0;
        }
    }
    return 1;
}

// the field of `bit` among `fields`, the narrowest holding it, or `fields` where none does
static unsigned int khw_field_of(unsigned int bit, unsigned int fields, const unsigned int *field_first,
                                 const unsigned int *field_bits)
{
    unsigned int found = fields;
    for (unsigned int field = 0u; field < fields; field += 1u)
    {
        if ((field_first[field] <= bit) && (bit < (field_first[field] + field_bits[field])) &&
            ((found == fields) || (field_bits[field] < field_bits[found])))
        {
            found = field;
        }
    }
    return found;
}

unsigned int khw_runs_gathered(const int *turned, unsigned int fields, const unsigned int *field_first,
                               const unsigned int *field_bits, KhwAnswered *held)
{
    held->runs = 0u;
    for (unsigned int bit = KHW_TURN_FIRST; (bit <= KHW_TURN_LAST) && (held->runs < KHW_RUNS); bit += 1u)
    {
        if (turned[bit] == 0)
        {
            continue;
        }
        const unsigned int field = khw_field_of(bit, fields, field_first, field_bits);
        unsigned int end = bit;
        while ((end < KHW_TURN_LAST) && (turned[end + 1u] != 0) &&
               (khw_field_of(end + 1u, fields, field_first, field_bits) == field))
        {
            end += 1u;
        }
        held->first[held->runs] = bit;
        held->last[held->runs] = end;
        held->place[held->runs] = KHW_PLACE_NONE;
        held->kind[held->runs] = KHW_KIND_REGISTER;
        held->runs += 1u;
        bit = end;
    }
    return held->runs;
}
