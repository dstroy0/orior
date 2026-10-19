// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// relation.c: the relation: the union of the candidates written, a form's relation found, read and asked, and the first
// round
#include "khw_learn_internal.h"

void khw_union_written(void)
{
    if (s_union_cases != 0u)
    {
        return;
    }
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        const LadderQuestion *const question = &s_ladder_cases[at];
        if ((question->words < 2u) || (s_ladder_measured[question->anchor] != 0))
        {
            continue;
        }
        for (unsigned int order = 0u; order < 2u; order += 1u)
        {
            const unsigned int first = (order == 0u) ? question->word[0] : question->word[1];
            const unsigned int second = (order == 0u) ? question->word[1] : question->word[0];
            int held_already = 0;
            for (unsigned int place = 0u; (place < s_union_cases) && !held_already; place += 1u)
            {
                held_already = (s_union_word[place][0] == first) && (s_union_word[place][1] == second);
            }
            if (!held_already && (s_union_cases < GATE_CASES))
            {
                s_union_word[s_union_cases][0] = first;
                s_union_word[s_union_cases][1] = second;
                s_union_cases += 1u;
            }
        }
    }
    for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        if (s_ladder_measured[anchor] != 0)
        {
            continue;
        }
        for (int swapped = 0; swapped < 2; swapped += 1)
        {
            const KhwCandidate plain = {anchor, swapped, 0};
            const KhwCandidate read_signed = {anchor, swapped, 1};
            int reads_two_ways = 0;
            for (unsigned int place = 0u; place < s_union_cases; place += 1u)
            {
                reads_two_ways = reads_two_ways || (khw_candidate_answer(&plain, s_union_word[place]) !=
                                                    khw_candidate_answer(&read_signed, s_union_word[place]));
            }
            if (s_candidates < GATE_CANDIDATES)
            {
                s_candidate[s_candidates] = plain;
                s_candidates += 1u;
            }
            if (reads_two_ways && (s_candidates < GATE_CANDIDATES))
            {
                s_candidate[s_candidates] = read_signed;
                s_candidates += 1u;
            }
        }
    }
}

// The relation `question`, put over the cases khw_union_written writes, answers, read by the gate (P2). Each case of
// each candidate is read as P11 reads it against the host's answer, and a candidate fails a case it does not read
// alike: no cost enters S, and the survivors are the conjunction over every case. The first survivor in the ladder's
// order, in order before swapped and plain before signed, is kept in `held` with the count of survivors, its reading
// (Q11). 1 where one survived, 0 where none did
int khw_relation_of(const RunQuestion *question, KhwAnswered *held, int guarded)
{
    static unsigned char s_fails[GATE_CANDIDATES][GATE_CASES];
    held->anchor = (unsigned int)LADDER_ANCHOR_COUNT;
    held->frame = (unsigned int)FRAME_RELATION_COUNT;
    held->swapped = 0;
    held->signed_read = 0;
    held->signedness_asked = 0;
    held->survivors = 0u;
    for (unsigned int candidate = 0u; candidate < s_candidates; candidate += 1u)
    {
        // a question put plain is read against the register relations alone, and one put with the store guarded
        // against the predicate relations alone: each kernel reads the other's relations as nothing
        const int predicate = ladder_predicate(s_candidate[candidate].anchor);
        for (unsigned int place = 0u; place < s_union_cases; place += 1u)
        {
            const unsigned int host = khw_candidate_answer(&s_candidate[candidate], s_union_word[place]);
            const AnswerBracket bracket = {1, host, host};
            s_fails[candidate][place] =
                ((predicate != (guarded != 0)) ||
                 (answer_read(&bracket, question->outcome, question->answered[place]) != ANSWER_ALIKE))
                    ? 1u
                    : 0u;
        }
    }
    unsigned char survives[GATE_CANDIDATES];
    held->survivors = gate_survivors(s_fails, s_candidates, s_union_cases, survives);
    for (unsigned int candidate = 0u; candidate < s_candidates; candidate += 1u)
    {
        if (survives[candidate] == 0u)
        {
            continue;
        }
        held->anchor = s_candidate[candidate].anchor;
        held->swapped = s_candidate[candidate].swapped;
        held->signed_read = s_candidate[candidate].signed_read;
        // a relation no case of which reads two ways leaves the part nothing to answer about signedness
        for (unsigned int other = 0u; other < s_candidates; other += 1u)
        {
            held->signedness_asked = held->signedness_asked ||
                                     ((s_candidate[other].anchor == held->anchor) &&
                                      (s_candidate[other].swapped == held->swapped) && s_candidate[other].signed_read);
        }
        return 1;
    }
    return 0;
}

// The relation the code in s_code answers, asked of the part in one ask over the cases khw_union_written writes and
// read by the gate (khw_relation_of). 1 where one relation survived
static int khw_relation_read(KhwAnswered *held)
{
    static unsigned long long s_answered[RUN_CASES_MOST];
    khw_union_written();
    khw_asked(s_union_word, s_union_cases, s_slot, s_answered);
    return khw_relation_of(&s_question, held, khw_store_is_guarded());
}

// 1 where `question`, put plain, answered the second word on every case: the slot left the register the kernel
// stores alone, as a form that writes a predicate does, and the question is put again with the store guarded
int khw_second_word_standing(const RunQuestion *question)
{
    if (question->outcome != RUN_ANSWERED)
    {
        return 0;
    }
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        if (question->answered[place] != (unsigned long long)question->word[place][1])
        {
            return 0;
        }
    }
    return 1;
}

// the relation `low` and `high` at the slot answers, read as khw_relation_read reads it, plain and then, where no
// relation answered and the second word stood on every case, with the store guarded. 1 where one relation answered
int khw_relation_asked(unsigned long long low, unsigned long long high, KhwAnswered *held)
{
    held->low = low;
    held->high = high;
    khw_store_guarded(0);
    khw_code_put(s_code, s_slot, low, high);
    int answered = khw_relation_read(held);
    if (!answered && khw_second_word_standing(&s_question))
    {
        khw_store_guarded(1);
        khw_code_put(s_code, s_slot, low, high);
        answered = khw_relation_read(held);
        khw_store_guarded(0);
    }
    return answered;
}

// The first round: the kernel's own relation and the slot, put together, since neither waits on the other's answer.
// The kernel's question is the kernel the system accepted, every instruction of it waiting on all six barriers and
// otherwise unchanged, over the cases khw_union_written writes, its relation read by the gate. The slot is the kernel
// listing's where it names one, the part's answer in its .ksc, and nothing is put for it; otherwise the slot's
// questions are the kernel with each place but the unreached in turn holding the encoding its unreached places hold,
// over one case, and the slot is the first place whose answer comes back as the case's second word, because nothing
// then wrote the register the kernel stores. The relation that answered is then put once more over its own cases, its
// launches timed into `nanoseconds`, 0 where the part gave no time. 1 where the kernel answered a relation, its slot
// through `slot`, past every place where none answered so
int khw_first_round(KhwAnswered *kernel, unsigned int *slot, unsigned long long *nanoseconds)
{
    static unsigned int s_word[RUN_CASES_MOST][2];
    static unsigned int s_plain[RUN_CASES_MOST];
    static unsigned int s_signed[RUN_CASES_MOST];
    static unsigned long long s_answered[RUN_CASES_MOST];
    unsigned int place_of[SCHEDULER_ROUND_MOST];
    const unsigned int slot_case[1][2] = {{3u, 5u}};
    *nanoseconds = 0ull;
    *slot = s_kernel_places;
    s_unreached_low = khw_word_read(&s_kernel_text[(s_kernel_places - 1u) * KHW_INSTRUCTION], 0u);
    s_unreached_high = khw_word_read(&s_kernel_text[(s_kernel_places - 1u) * KHW_INSTRUCTION], 8u);
    khw_union_written();
    s_pooled = 0u;
    const unsigned int kernel_at = khw_pooled(s_kernel_places, 0ull, 0ull, s_union_word, s_union_cases);
    // the places are put only where the kernel listing names no slot: a place put with the unreached encoding runs
    // the kernel without that place, and a place the addresses depend on faults the part
    for (unsigned int place = 0u; (s_kernel_slot >= s_kernel_places) && (place < s_kernel_places); place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        if ((low == s_unreached_low) && (high == s_unreached_high))
        {
            continue;
        }
        const unsigned int at = khw_pooled(place, s_unreached_low, s_unreached_high, slot_case, 1u);
        if (at < SCHEDULER_ROUND_MOST)
        {
            place_of[at] = place;
        }
    }
    khw_round_carried();
    const int related = khw_relation_of(&s_pool[kernel_at], kernel, 0);
    *slot = (s_kernel_slot < s_kernel_places) ? s_kernel_slot : *slot;
    for (unsigned int at = kernel_at + 1u; (at < s_pooled) && (*slot == s_kernel_places); at += 1u)
    {
        *slot =
            ((s_pool[at].outcome == RUN_ANSWERED) && (s_pool[at].answered[0] == (unsigned long long)slot_case[0][1]))
                ? place_of[at]
                : *slot;
    }
    s_pooled = 0u;
    if (!related)
    {
        return 0;
    }
    khw_kernel_put(s_code);
    const unsigned int count = khw_anchor_cases(kernel->anchor, kernel->swapped, s_word, s_plain, s_signed);
    s_launches = KHW_LAUNCHES;
    const int timed = khw_asked(s_word, count, s_slot, s_answered);
    s_launches = 0u;
    *nanoseconds = timed ? s_question.nanoseconds : 0ull;
    return 1;
}
