// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// relation.c: the relation: the union of the candidates written, a form's relation found, read and queried, and the
// first round
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
            int already_present = 0;
            for (unsigned int place = 0u; (place < s_union_cases) && !already_present; place += 1u)
            {
                already_present = (s_union_word[place][0] == first) && (s_union_word[place][1] == second);
            }
            if (!already_present && (s_union_cases < GATE_CASES))
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

// The relation that `question` answers, put over the cases that khw_union_written writes, and read by the gate (P2).
// The gate reads each case of each candidate as P11 reads it, against the answer of the host. A candidate fails a case
// that it does not read alike, and no cost enters S, and the survivors are the conjunction over every case. The call
// keeps in `form` the first survivor in the order of the ladder, in order before swapped and plain before signed. It
// keeps it with the count of survivors and its reading (Q11). The return is 1 where one survived, 0 where none did
int khw_relation_of(const RunQuestion *question, KhwAnswered *form, int guarded)
{
    static unsigned char s_fails[GATE_CANDIDATES][GATE_CASES];
    form->anchor = (unsigned int)LADDER_ANCHOR_COUNT;
    form->frame = (unsigned int)FRAME_RELATION_COUNT;
    form->swapped = 0;
    form->signed_read = 0;
    form->signedness_queried = 0;
    form->survivors = 0u;
    for (unsigned int candidate = 0u; candidate < s_candidates; candidate += 1u)
    {
        // the call reads a question put plain against the register relations alone. It reads one put with the store
        // guarded against the predicate relations alone. Each kernel reads the relations of the other as nothing
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
    form->survivors = gate_survivors(s_fails, s_candidates, s_union_cases, survives);
    for (unsigned int candidate = 0u; candidate < s_candidates; candidate += 1u)
    {
        if (survives[candidate] == 0u)
        {
            continue;
        }
        form->anchor = s_candidate[candidate].anchor;
        form->swapped = s_candidate[candidate].swapped;
        form->signed_read = s_candidate[candidate].signed_read;
        // a relation no case of which reads two ways leaves the part nothing to answer about signedness
        for (unsigned int other = 0u; other < s_candidates; other += 1u)
        {
            form->signedness_queried = form->signedness_queried || ((s_candidate[other].anchor == form->anchor) &&
                                                                    (s_candidate[other].swapped == form->swapped) &&
                                                                    s_candidate[other].signed_read);
        }
        return 1;
    }
    return 0;
}

// The relation the code in s_code answers, queried of the part in one query over the cases khw_union_written writes and
// read by the gate (khw_relation_of). 1 where one relation survived
static int khw_relation_read(KhwAnswered *form)
{
    static unsigned long long s_answered[RUN_CASES_MAX];
    khw_union_written();
    khw_question_put(s_union_word, s_union_cases, s_probe_place, s_answered);
    return khw_relation_of(&s_question, form, khw_store_is_guarded());
}

// 1 where `question`, put plain, answered the second word on every case. The probe did not change the register that
// the kernel stores, as a form that writes a predicate does. The call then puts the question again with the store
// guarded
int khw_second_word_located(const RunQuestion *question)
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

// the relation that `low` and `high` at the probe place answers, read as khw_relation_read reads it. The call reads it
// plain. Where no relation answered and the second word stayed as it was on every case, it reads it with the store
// guarded. The return is 1 where one relation answered
int khw_relation_queried(unsigned long long low, unsigned long long high, KhwAnswered *form)
{
    form->low = low;
    form->high = high;
    khw_store_guarded(0);
    khw_code_put(s_code, s_probe_place, low, high);
    int answered = khw_relation_read(form);
    if (!answered && khw_second_word_located(&s_question))
    {
        khw_store_guarded(1);
        khw_code_put(s_code, s_probe_place, low, high);
        answered = khw_relation_read(form);
        khw_store_guarded(0);
    }
    return answered;
}

// The first round: the own relation of the kernel and the probe place, put together, because neither waits on the
// answer of the other. The question of the kernel is the kernel that the system accepted, over the cases that
// khw_union_written writes. Every instruction of it waits on all six barriers, with no other change. The gate reads
// its relation.
//
// Where the kernel listing names a probe place, that is the probe place, the answer of the part in its .ksc. The call
// puts nothing for it. Else the questions of the probe place are the kernel over one case. In turn, each place but the
// unreached ones holds the encoding of its unreached places. The probe place is the first place whose answer comes back
// as the second word of the case. Then nothing wrote the register that the kernel stores.
//
// The call then puts the relation that answered once more, over its own cases, and times its launches into
// `nanoseconds`, 0 where the part gave no time. The return is 1 where the kernel answered a relation, with its probe
// place through `kernel_place`. Where none answered so, the probe place is past every place
int khw_first_round(KhwAnswered *kernel, unsigned int *kernel_place, unsigned long long *nanoseconds)
{
    static unsigned int s_word[RUN_CASES_MAX][2];
    static unsigned int s_plain[RUN_CASES_MAX];
    static unsigned int s_signed[RUN_CASES_MAX];
    static unsigned long long s_answered[RUN_CASES_MAX];
    unsigned int place_of[SCHEDULER_ROUND_MAX];
    const unsigned int probed_case[1][2] = {{3u, 5u}};
    *nanoseconds = 0ULL;
    *kernel_place = s_kernel_places;
    s_unreached_low = khw_word_read(&s_kernel_text[(s_kernel_places - 1u) * KHW_INSTRUCTION], 0u);
    s_unreached_high = khw_word_read(&s_kernel_text[(s_kernel_places - 1u) * KHW_INSTRUCTION], 8u);
    khw_union_written();
    s_pooled = 0u;
    const unsigned int kernel_at = khw_pooled(s_kernel_places, 0ULL, 0ULL, s_union_word, s_union_cases);
    // the call puts the places only where the kernel listing names no probe place. A place put with the unreached
    // encoding runs the kernel without that place, and a place that the addresses depend on faults the part
    for (unsigned int place = 0u; (s_kernel_probe_place >= s_kernel_places) && (place < s_kernel_places); place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        if ((low == s_unreached_low) && (high == s_unreached_high))
        {
            continue;
        }
        const unsigned int at = khw_pooled(place, s_unreached_low, s_unreached_high, probed_case, 1u);
        if (at < SCHEDULER_ROUND_MAX)
        {
            place_of[at] = place;
        }
    }
    khw_round_carried();
    const int related = khw_relation_of(&s_pool[kernel_at], kernel, 0);
    *kernel_place = (s_kernel_probe_place < s_kernel_places) ? s_kernel_probe_place : *kernel_place;
    for (unsigned int at = kernel_at + 1u; (at < s_pooled) && (*kernel_place == s_kernel_places); at += 1u)
    {
        *kernel_place =
            ((s_pool[at].outcome == RUN_ANSWERED) && (s_pool[at].answered[0] == (unsigned long long)probed_case[0][1]))
                ? place_of[at]
                : *kernel_place;
    }
    s_pooled = 0u;
    if (!related)
    {
        return 0;
    }
    khw_kernel_put(s_code);
    const unsigned int count = khw_anchor_cases(kernel->anchor, kernel->swapped, s_word, s_plain, s_signed);
    s_launches = KHW_LAUNCHES;
    const int timed = khw_question_put(s_word, count, s_probe_place, s_answered);
    s_launches = 0u;
    *nanoseconds = timed ? s_question.nanoseconds : 0ULL;
    return 1;
}
