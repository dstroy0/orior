// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// relation_turns.c: the relation: the turn case, and the turns queried
#include "khw_learn_internal.h"
#include "memoria_operor/memoria_operor.h"

// The turn case of `form` into `stage`. For a ladder form, it is the first case of its relation that the register
// pass reads apart (khw_turning_words). The words go in the order that the form reads them. Thus a changed answer is
// a real change and not a coincidence. For a frame form, it is the words 3 and 5 over the shape of the frame, and for a
// parameter over every case of the frame. The return is 1, or 0 where the relation holds no such case
int khw_turning_case(KhwAnswered *form, KhwStaged *stage)
{
    stage->form = form;
    stage->at = 0u;
    stage->base = 0ULL;
    if (form->frame != (unsigned int)FRAME_RELATION_COUNT)
    {
        stage->word[0][0] = 3u;
        stage->word[0][1] = 5u;
        // a parameter's answer is the count the launch passes the kernel, which the frame's cases set
        stage->count = (form->frame == (unsigned int)FRAME_PARAMETER) ? FRAME_CASES : (FRAME_READ_AT + 1u);
        stage->read_at = FRAME_READ_AT;
        return frame_answer(form->frame, FRAME_READ_AT, stage->word[0], form->reading, &stage->expected);
    }
    stage->count = 1u;
    stage->read_at = 0u;
    unsigned int gave = 0u;
    if (ladder_predicate(form->anchor))
    {
        // the case of a predicate form: the turn case of the stage for its relation. In it, the relation is true for
        // the reading of the words by the form, and the words differ. Thus the guarded store writes the second word. A
        // turn that stops the predicate, or moves a word, leaves the fill word, or writes it where the relation fails
        if (!khw_predicate_words(form->anchor, form->swapped, form->signed_read, stage->turning, stage->word[0]))
        {
            return 0;
        }
        stage->expected = stage->word[0][1];
        return 1;
    }
    const int found = khw_turning_words(form->anchor, form->swapped, stage->word[0], &gave);
    stage->expected = gave;
    return found;
}

// `low` and `high` pooled as a question of the shape of `stage`. A ladder form goes at the probe place over its one
// case, and a frame form goes whole over its cases. Each case has the words of the stage
unsigned int khw_staged_pooled(const KhwStaged *stage, unsigned long long low, unsigned long long high)
{
    static unsigned int s_words[RUN_CASES_MAX][2];
    for (unsigned int at = 0u; at < stage->count; at += 1u)
    {
        s_words[at][0] = stage->word[0][0];
        s_words[at][1] = stage->word[0][1];
    }
    if (stage->form->frame != (unsigned int)FRAME_RELATION_COUNT)
    {
        return khw_frame_pooled(low, high, s_words, stage->count);
    }
    // the call puts the questions of a predicate form with the predicate that the form writes as the guard of the store
    khw_store_guarded(ladder_predicate(stage->form->anchor));
    const unsigned int at = khw_pooled(s_probe_place, low, high, s_words, stage->count);
    khw_store_guarded(0);
    return at;
}

// The turns of the form of `stage`. Each bit from KHW_TURN_FIRST to KHW_TURN_LAST turns one at a time over its case.
// Each turn is a question of the shape of the stage, pooled into the round that the call now writes. The code of a
// turned bit is the code of the form with that one bit turned over.
//
// The call needs no answer to put it. The carrier, with all of them, waits on the reader of the vendor before the part
// sees them. The pooled question of the first turn is at `at` in s_pool, and the rest follow it in their order
static void khw_turns_pooled(const KhwStaged *stage)
{
    for (unsigned int bit = KHW_TURN_FIRST; bit <= KHW_TURN_LAST; bit += 1u)
    {
        unsigned long long low = stage->form->low;
        unsigned long long high = stage->form->high;
        khw_run_write(&low, &high, bit, bit, khw_run_read(low, high, bit, bit) ^ 1ULL);
        khw_staged_pooled(stage, low, high);
    }
}

// The turns of `form`, read from the carried round. Its turns are at `at` in s_pool and after, and the call reads each
// at the case `read_at`. A bit whose turn changed the answer `base` goes into the runs that the operands can sit in.
// The call cuts the runs where the fields that the .ksc of the part names end (khw_runs_gathered). It does not yet
// query whether a run carries a register. The runs are the candidates that a register pass reads, and the phase
// becomes turned.
//
// For a predicate form, the turns over a turn case after its first (`turning` above 0) join the
// bits of the runs that its earlier cases turned
static void khw_turns_read(KhwAnswered *form, unsigned int at, unsigned long long base, unsigned int read_at,
                           unsigned int turning)
{
    int turned[KHW_TURN_LAST + 1u];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = turned, .bytes = sizeof(turned), .val = 0);
    for (unsigned int run = 0u; (turning != 0u) && (form->phase == KHW_PHASE_TURNED) && (run < form->runs); run += 1u)
    {
        for (unsigned int bit = form->first[run]; (bit <= form->last[run]) && (bit <= KHW_TURN_LAST); bit += 1u)
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
    khw_runs_gathered(turned, fields, field_first, field_bits, form);
    form->phase = KHW_PHASE_TURNED;
}

// The turns of `form` over `word`, put and carried as a round of their own. What came back stays in s_pool, one
// question for each turned bit in its order, in the shape that the relation of the form takes
void khw_turns_queried(KhwAnswered *form, const unsigned int word[1][2])
{
    KhwStaged stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &stage, .bytes = sizeof(stage), .val = 0);
    if (!khw_turning_case(form, &stage))
    {
        stage.word[0][0] = word[0][0];
        stage.word[0][1] = word[0][1];
    }
    s_pooled = 0u;
    khw_turns_pooled(&stage);
    khw_round_carried();
}

// the forms of the working set in `phase`, into `forms`, which has room for KHW_FORMS: their count
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

// The void comparison forms of the working set, added to `forms` after its `count`. The runs of a comparison form are
// what its turns found, and a turn case shows the move of one word and not the other. Thus the call turns a void
// form again over every turn case of its relation (khw_predicate_words), and reads its runs as the part answers
// them. The return is the count with them
unsigned int khw_comparisons_void(KhwAnswered **forms, unsigned int count)
{
    for (unsigned int at = 0u; (at < s_names) && (count < KHW_FORMS); at += 1u)
    {
        KhwAnswered *const form = &s_named[at];
        if ((form->phase == KHW_PHASE_VOID) && (form->frame == (unsigned int)FRAME_RELATION_COUNT) &&
            ladder_predicate(form->anchor))
        {
            forms[count] = form;
            count += 1u;
        }
    }
    return count;
}

// The classified ladder forms of the working set where two runs carry one place of the tuple of the relation, added to
// `forms` after its `count`. Two runs cover a place that a word of a relation takes once only where the turn case
// read a run by a coincidence. The call then queries the register pass of the form again over a case whose readings
// are apart (khw_turning_words). The return is the count with them
unsigned int khw_forms_placed_twice(KhwAnswered **forms, unsigned int count)
{
    for (unsigned int at = 0u; (at < s_names) && (count < KHW_FORMS); at += 1u)
    {
        KhwAnswered *const form = &s_named[at];
        if ((form->phase != KHW_PHASE_CLASSIFIED) || (form->frame != (unsigned int)FRAME_RELATION_COUNT))
        {
            continue;
        }
        int twice = 0;
        for (unsigned int run = 0u; run < form->runs; run += 1u)
        {
            for (unsigned int other = run + 1u; other < form->runs; other += 1u)
            {
                twice = twice || ((form->place[run] != KHW_PLACE_NONE) && (form->place[run] == form->place[other]));
            }
        }
        if (twice)
        {
            forms[count] = form;
            count += 1u;
        }
    }
    return count;
}

// the forms of `staged` from `first` to `count` whose turn cases the carried round answered as the host expected.
// Each keeps the answer of the part as its base and goes on. The call leaves a form that the part answered otherwise,
// or not at all, where it is. The return is the count that go on
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

// The first field pass over every discovered form of `forms`, `count` of them, in shared rounds, stage by stage. First
// come the turn cases, one question for each form, as many forms to a round as the pool holds. A form that the part
// answers as the host expects goes on. The call leaves each other form where it is.
//
// Then come the turns. The call turns the bits 12 to 104 of each form one at a time over its case. A round takes as
// many forms as the pool holds. It reads what came back into the candidate runs of each form. The call stages a
// predicate form once for each of its turn cases, and the bits that any of them turned are its runs. The return is the
// count of forms turned
unsigned int khw_fields_turns(KhwAnswered *const *forms, unsigned int count)
{
    static KhwStaged s_staged[KHW_FORMS];
    unsigned int staged = 0u;
    unsigned int on = 0u;
    // the turn cases, a round at a time. A carried round leaves the forms that go on packed at the front
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const int predicate =
            (forms[at]->frame == (unsigned int)FRAME_RELATION_COUNT) && ladder_predicate(forms[at]->anchor);
        for (unsigned int turning = 0u; staged < KHW_FORMS; turning += 1u)
        {
            KhwStaged stage;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &stage, .bytes = sizeof(stage), .val = 0);
            stage.turning = turning;
            if (((turning != 0u) && !predicate) || !khw_turning_case(forms[at], &stage))
            {
                break;
            }
            if (s_pooled == SCHEDULER_ROUND_MAX)
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
    // the turns, as many stages to a round as the pool holds. The call reads the stages of a form in the order that it
    // staged them
    unsigned int first = 0u;
    for (unsigned int at = 0u; at < on; at += 1u)
    {
        if ((s_pooled + KHW_TURNS) > SCHEDULER_ROUND_MAX)
        {
            khw_round_carried();
            for (unsigned int read = first; read < at; read += 1u)
            {
                khw_turns_read(s_staged[read].form, s_staged[read].at, s_staged[read].base, s_staged[read].read_at,
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
            khw_turns_read(s_staged[read].form, s_staged[read].at, s_staged[read].base, s_staged[read].read_at,
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

unsigned int khw_turning_cases(const KhwAnswered *form)
{
    if ((form->frame != (unsigned int)FRAME_RELATION_COUNT) || !ladder_predicate(form->anchor))
    {
        return 1u;
    }
    unsigned int cases = 0u;
    unsigned int word[2] = {0u, 0u};
    while (khw_predicate_words(form->anchor, form->swapped, form->signed_read, cases, word))
    {
        cases += 1u;
    }
    return cases;
}
