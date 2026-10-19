// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// relation_turns.c: the relation: the turning case, and the turns asked
#include "khw_learn_internal.h"

// The turning case of `held` into `stage`: for a ladder form, the first case of its relation whose readings in the
// register pass stand apart (khw_turning_words), in the order the form reads its words, so that a changed answer is a
// changed answer and not a coincidence; for a frame form, the words 3 and 5 over the frame's shape, a parameter's over
// the frame's every case. 1, or 0 where the relation holds no such case
int khw_turning_case(KhwAnswered *held, KhwStaged *stage)
{
    stage->held = held;
    stage->at = 0u;
    stage->base = 0ull;
    if (held->frame != (unsigned int)FRAME_RELATION_COUNT)
    {
        stage->word[0][0] = 3u;
        stage->word[0][1] = 5u;
        // a parameter's answer is the count the launch hands the kernel, which the frame's cases set
        stage->count = (held->frame == (unsigned int)FRAME_PARAMETER) ? FRAME_CASES : (FRAME_READ_AT + 1u);
        stage->read_at = FRAME_READ_AT;
        return frame_answer(held->frame, FRAME_READ_AT, stage->word[0], held->reading, &stage->expected);
    }
    stage->count = 1u;
    stage->read_at = 0u;
    unsigned int gave = 0u;
    if (ladder_predicate(held->anchor))
    {
        // a predicate form's case: the stage's turning case of its relation, one where the relation holds for the
        // form's reading of the words and the words differ, so that the guarded store writes the second word and a
        // turn that stops the predicate, or moves a word, leaves the fill word or writes it where the relation fails
        if (!khw_predicate_words(held->anchor, held->swapped, held->signed_read, stage->turning, stage->word[0]))
        {
            return 0;
        }
        stage->expected = stage->word[0][1];
        return 1;
    }
    const int found = khw_turning_words(held->anchor, held->swapped, stage->word[0], &gave);
    stage->expected = gave;
    return found;
}

// `low` and `high` pooled as a question of `stage`'s shape: a ladder form's at the slot over its one case, a frame
// form's whole over its cases, each case the stage's words
unsigned int khw_staged_pooled(const KhwStaged *stage, unsigned long long low, unsigned long long high)
{
    static unsigned int s_words[RUN_CASES_MOST][2];
    for (unsigned int at = 0u; at < stage->count; at += 1u)
    {
        s_words[at][0] = stage->word[0][0];
        s_words[at][1] = stage->word[0][1];
    }
    if (stage->held->frame != (unsigned int)FRAME_RELATION_COUNT)
    {
        return khw_frame_pooled(low, high, s_words, stage->count);
    }
    // a predicate form's questions are put with the store guarded by the predicate it writes
    khw_store_guarded(ladder_predicate(stage->held->anchor));
    const unsigned int at = khw_pooled(s_slot, low, high, s_words, stage->count);
    khw_store_guarded(0);
    return at;
}

// The turns of `stage`'s form: each bit from KHW_TURN_FIRST to KHW_TURN_LAST turned one at a time over its case, each a
// question of the stage's shape, pooled into the round being written. The code a turned bit holds is the form's with
// that one bit over, and no answer is needed to put it: the carrier, with every one of them, waits on the vendor's
// reader before the part sees them. The pooled question of the first turn is at `at` in s_pool, and the rest follow
// it in their order
static void khw_turns_pooled(const KhwStaged *stage)
{
    for (unsigned int bit = KHW_TURN_FIRST; bit <= KHW_TURN_LAST; bit += 1u)
    {
        unsigned long long low = stage->held->low;
        unsigned long long high = stage->held->high;
        khw_run_write(&low, &high, bit, bit, khw_run_read(low, high, bit, bit) ^ 1ull);
        khw_staged_pooled(stage, low, high);
    }
}

// The turns of `held` read off the round carried, its turns at `at` in s_pool and after, each read at the case
// `read_at`: a bit whose turning changed the answer `base` gathered into the runs the operands might sit in, cut
// where the fields the part's .ksc names end (khw_runs_gathered), none yet asked whether it carries a register. The
// runs are the candidates a register pass reads, and the phase turned. A predicate form's turns over a turning case
// after its first, `turning` above 0, join the bits of the runs its earlier cases turned
static void khw_turns_read(KhwAnswered *held, unsigned int at, unsigned long long base, unsigned int read_at,
                           unsigned int turning)
{
    int turned[KHW_TURN_LAST + 1u];
    memset(turned, 0, sizeof(turned));
    for (unsigned int run = 0u; (turning != 0u) && (held->phase == KHW_PHASE_TURNED) && (run < held->runs); run += 1u)
    {
        for (unsigned int bit = held->first[run]; (bit <= held->last[run]) && (bit <= KHW_TURN_LAST); bit += 1u)
        {
            turned[bit] = 1;
        }
    }
    for (unsigned int bit = KHW_TURN_FIRST; bit <= KHW_TURN_LAST; bit += 1u)
    {
        const RunQuestion *const question = &s_pool[at + (bit - KHW_TURN_FIRST)];
        turned[bit] = turned[bit] || ((question->outcome == RUN_ANSWERED) && (question->answered[read_at] != base));
    }
    unsigned int field_first[KHW_FIELDS];
    unsigned int field_bits[KHW_FIELDS];
    const unsigned int fields = khw_fields_listed(field_first, field_bits, KHW_FIELDS);
    khw_runs_gathered(turned, fields, field_first, field_bits, held);
    held->phase = KHW_PHASE_TURNED;
}

// The turns of `held` over `word` put and carried as a round of their own, what came back left in s_pool, a question
// a turned bit in its order, in the shape the form's relation takes
void khw_turns_asked(KhwAnswered *held, const unsigned int word[1][2])
{
    KhwStaged stage;
    memset(&stage, 0, sizeof(stage));
    if (!khw_turning_case(held, &stage))
    {
        stage.word[0][0] = word[0][0];
        stage.word[0][1] = word[0][1];
    }
    s_pooled = 0u;
    khw_turns_pooled(&stage);
    khw_round_carried();
}

// the forms of the working set in `phase`, into `forms`, which holds KHW_FORMS: their count
unsigned int khw_forms_in_phase(unsigned int phase, KhwAnswered **forms)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        if (s_named[at].phase == phase)
        {
            forms[count] = &s_named[at];
            count += 1u;
        }
    }
    return count;
}

// The void comparison forms of the working set, added to `forms` after its `count`: a comparison form's runs are what
// its turns found, and a turning case shows the move of one word and not the other: a void one is turned again over
// every turning case of its relation (khw_predicate_words) and its runs read as the part answers them. The count with
// them
unsigned int khw_comparisons_void(KhwAnswered **forms, unsigned int count)
{
    for (unsigned int at = 0u; (at < s_names) && (count < KHW_FORMS); at += 1u)
    {
        KhwAnswered *const held = &s_named[at];
        if ((held->phase == KHW_PHASE_VOID) && (held->frame == (unsigned int)FRAME_RELATION_COUNT) &&
            ladder_predicate(held->anchor))
        {
            forms[count] = held;
            count += 1u;
        }
    }
    return count;
}

// The classified ladder forms of the working set two of whose runs carry one place of the relation's tuple, added to
// `forms` after its `count`: a place a relation's word takes once is held by two runs only where the turning case
// read a run by a coincidence, and the form is asked its register pass again over a case whose readings stand apart
// (khw_turning_words). The count with them
unsigned int khw_forms_placed_twice(KhwAnswered **forms, unsigned int count)
{
    for (unsigned int at = 0u; (at < s_names) && (count < KHW_FORMS); at += 1u)
    {
        KhwAnswered *const held = &s_named[at];
        if ((held->phase != KHW_PHASE_CLASSIFIED) || (held->frame != (unsigned int)FRAME_RELATION_COUNT))
        {
            continue;
        }
        int twice = 0;
        for (unsigned int run = 0u; run < held->runs; run += 1u)
        {
            for (unsigned int other = run + 1u; other < held->runs; other += 1u)
            {
                twice = twice || ((held->place[run] != KHW_PLACE_NONE) && (held->place[run] == held->place[other]));
            }
        }
        if (twice)
        {
            forms[count] = held;
            count += 1u;
        }
    }
    return count;
}

// the forms of `staged` from `first` to `count` whose turning cases the round carried answered as the host expected:
// each keeps the part's answer as its base and goes on, and one the part answered otherwise or not at all is left
// where it is. The count that go on
static unsigned int khw_turning_read(KhwStaged *staged, unsigned int first, unsigned int count)
{
    unsigned int on = 0u;
    for (unsigned int at = first; at < count; at += 1u)
    {
        const RunQuestion *const question = &s_pool[staged[at].at];
        const int answered =
            (question->outcome == RUN_ANSWERED) && (question->answered[staged[at].read_at] == staged[at].expected);
        staged[at].base = question->answered[staged[at].read_at];
        staged[on] = staged[at];
        on += answered ? 1u : 0u;
    }
    return on;
}

// The first field-finding pass over every discovered form of `forms`, `count` of them, in shared rounds, stage by
// stage. The turning cases first, one question a form, as many forms a round as the pool holds: a form the part
// answers as the host expects goes on, and one it does not is left where it is. Then the turns: each form's bits from
// 12 to 104 turned one at a time over its case, as many forms a round as the pool holds, and what came back read
// into each form's candidate runs. A predicate form is staged once for each of its turning cases, and the bits any of
// them turned are its runs. The count of forms turned
unsigned int khw_fields_turns(KhwAnswered *const *forms, unsigned int count)
{
    static KhwStaged s_staged[KHW_FORMS];
    unsigned int staged = 0u;
    unsigned int on = 0u;
    // the turning cases, a round at a time; a round carried leaves the forms that go on packed at the front
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const int predicate =
            (forms[at]->frame == (unsigned int)FRAME_RELATION_COUNT) && ladder_predicate(forms[at]->anchor);
        for (unsigned int turning = 0u; staged < KHW_FORMS; turning += 1u)
        {
            KhwStaged stage;
            memset(&stage, 0, sizeof(stage));
            stage.turning = turning;
            if (((turning != 0u) && !predicate) || !khw_turning_case(forms[at], &stage))
            {
                break;
            }
            if (s_pooled == SCHEDULER_ROUND_MOST)
            {
                khw_round_carried();
                on = khw_turning_read(s_staged, on, staged);
                staged = on;
                s_pooled = 0u;
            }
            stage.at = khw_staged_pooled(&stage, forms[at]->low, forms[at]->high);
            s_staged[staged] = stage;
            staged += 1u;
        }
    }
    if (s_pooled != 0u)
    {
        khw_round_carried();
        on = khw_turning_read(s_staged, on, staged);
        s_pooled = 0u;
    }
    // the turns, as many stages a round as the pool holds, a form's stages read in the order they were staged
    unsigned int first = 0u;
    for (unsigned int at = 0u; at < on; at += 1u)
    {
        if ((s_pooled + KHW_TURNS) > SCHEDULER_ROUND_MOST)
        {
            khw_round_carried();
            for (unsigned int read = first; read < at; read += 1u)
            {
                khw_turns_read(s_staged[read].held, s_staged[read].at, s_staged[read].base, s_staged[read].read_at,
                               s_staged[read].turning);
            }
            first = at;
            s_pooled = 0u;
        }
        s_staged[at].at = s_pooled;
        khw_turns_pooled(&s_staged[at]);
    }
    if (s_pooled != 0u)
    {
        khw_round_carried();
        for (unsigned int read = first; read < on; read += 1u)
        {
            khw_turns_read(s_staged[read].held, s_staged[read].at, s_staged[read].base, s_staged[read].read_at,
                           s_staged[read].turning);
        }
        s_pooled = 0u;
    }
    unsigned int turned = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        turned += (forms[at]->phase == KHW_PHASE_TURNED) ? 1u : 0u;
    }
    return turned;
}

unsigned int khw_turning_cases(const KhwAnswered *held)
{
    if ((held->frame != (unsigned int)FRAME_RELATION_COUNT) || !ladder_predicate(held->anchor))
    {
        return 1u;
    }
    unsigned int cases = 0u;
    unsigned int word[2] = {0u, 0u};
    while (khw_predicate_words(held->anchor, held->swapped, held->signed_read, cases, word))
    {
        cases += 1u;
    }
    return cases;
}

// The settings `run` of `held` is probed with in `stage` of the register pass, into `settings`, which holds
