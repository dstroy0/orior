// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// relation.c: the relation: the union of the candidates written, a form's relation found, read and asked, the first
// round, the turning case, and the turns and the fields asked
#include "khw_write_internal.h"

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

// What one form holds through a stage of a pass: the form, its turning case `word` and the answer `expected` of it,
// where its questions begin in the round being written, and the answer the part gave its turning case `base`. A
// ladder form's questions carry one case, read at 0; a frame form's carry FRAME_READ_AT + 1 cases of its word pair,
// read at FRAME_READ_AT, the thread whose lane and block differ from each other and from 0
typedef struct
{
    KhwAnswered *held;
    unsigned int word[1][2];
    unsigned long long expected;
    unsigned int at;
    unsigned long long base;
    unsigned int count;
    unsigned int read_at;
} KhwStaged;

// The turning case of `held` into `stage`: for a ladder form, the first case of its relation whose readings in the
// register pass stand apart (khw_turning_words), in the order the form reads its words, so that a changed answer is a
// changed answer and not a coincidence; for a frame form, the words 3 and 5 over the frame's shape. 1, or 0 where the
// relation holds no such case
static int khw_turning_case(KhwAnswered *held, KhwStaged *stage)
{
    stage->held = held;
    stage->at = 0u;
    stage->base = 0ull;
    if (held->frame != (unsigned int)FRAME_RELATION_COUNT)
    {
        stage->word[0][0] = 3u;
        stage->word[0][1] = 5u;
        stage->count = FRAME_READ_AT + 1u;
        stage->read_at = FRAME_READ_AT;
        return frame_answer(held->frame, FRAME_READ_AT, stage->word[0], held->reading, &stage->expected);
    }
    stage->count = 1u;
    stage->read_at = 0u;
    unsigned int gave = 0u;
    if (ladder_predicate(held->anchor))
    {
        // a predicate form's case: one of its relation's where the relation holds for the form's reading of the
        // words and the words differ, so that the guarded store writes the second word and a turn that stops the
        // predicate, or moves a word, leaves the fill word or writes it where the relation fails
        const KhwCandidate form = {held->anchor, held->swapped, held->signed_read};
        for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
        {
            const LadderQuestion *const question = &s_ladder_cases[at];
            if ((question->anchor != held->anchor) || (question->word[0] == question->word[1]))
            {
                continue;
            }
            for (unsigned int order = 0u; order < 2u; order += 1u)
            {
                stage->word[0][0] = (order == 0u) ? question->word[0] : question->word[1];
                stage->word[0][1] = (order == 0u) ? question->word[1] : question->word[0];
                if (khw_candidate_answer(&form, stage->word[0]) == stage->word[0][1])
                {
                    stage->expected = stage->word[0][1];
                    return 1;
                }
            }
        }
        return 0;
    }
    const int found = khw_turning_words(held->anchor, held->swapped, stage->word[0], &gave);
    stage->expected = gave;
    return found;
}

// `low` and `high` pooled as a question of `stage`'s shape: a ladder form's at the slot over its one case, a frame
// form's whole over its cases, each case the stage's words
static unsigned int khw_staged_pooled(const KhwStaged *stage, unsigned long long low, unsigned long long high)
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
// runs are the candidates a register pass reads, and the phase turned
static void khw_turns_read(KhwAnswered *held, unsigned int at, unsigned long long base, unsigned int read_at)
{
    int turned[KHW_TURN_LAST + 1u];
    memset(turned, 0, sizeof(turned));
    for (unsigned int bit = KHW_TURN_FIRST; bit <= KHW_TURN_LAST; bit += 1u)
    {
        const RunQuestion *const question = &s_pool[at + (bit - KHW_TURN_FIRST)];
        turned[bit] = ((question->outcome == RUN_ANSWERED) && (question->answered[read_at] != base)) ? 1 : 0;
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
// into each form's candidate runs. The count of forms turned
unsigned int khw_fields_turns(KhwAnswered *const *forms, unsigned int count)
{
    static KhwStaged s_staged[KHW_FORMS];
    unsigned int staged = 0u;
    unsigned int on = 0u;
    // the turning cases, a round at a time; a round carried leaves the forms that go on packed at the front
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        KhwStaged stage;
        memset(&stage, 0, sizeof(stage));
        if (!khw_turning_case(forms[at], &stage))
        {
            continue;
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
    if (s_pooled != 0u)
    {
        khw_round_carried();
        on = khw_turning_read(s_staged, on, staged);
        s_pooled = 0u;
    }
    // the turns, as many forms a round as the pool holds
    unsigned int turned = 0u;
    unsigned int first = 0u;
    for (unsigned int at = 0u; at < on; at += 1u)
    {
        if ((s_pooled + KHW_TURNS) > SCHEDULER_ROUND_MOST)
        {
            khw_round_carried();
            for (unsigned int read = first; read < at; read += 1u)
            {
                khw_turns_read(s_staged[read].held, s_staged[read].at, s_staged[read].base, s_staged[read].read_at);
            }
            turned += at - first;
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
            khw_turns_read(s_staged[read].held, s_staged[read].at, s_staged[read].base, s_staged[read].read_at);
        }
        turned += on - first;
        s_pooled = 0u;
    }
    return turned;
}

// The settings `run` of `held` is probed with in `stage` of the register pass, into `settings`, which holds
// KHW_SETTINGS: the count, 0 where the stage asks the run nothing. A ladder form's runs take the two registers the
// kernel leaves the case's words in, in the first stage alone. A frame form's runs take first the index's and the
// record's registers the part's .ksc names, two that hold addresses, so that no run is read as an address through a
// register holding none; then, the address runs apart, a run inside the field the .ksc names `immediate` takes 0 and
// KHW_OFFSET_WORD, the aligned offset of the record's second word, which a load and a store answer apart and a number
// answers as itself, and every other run 0 and the register the kernel stores
#define KHW_OFFSET_WORD 4u

static unsigned int khw_run_settings(const KhwAnswered *held, unsigned int stage, unsigned int run,
                                     unsigned int *settings)
{
    // a predicate form's runs are read off its own encoding, and no setting is asked
    if ((held->frame == (unsigned int)FRAME_RELATION_COUNT) && ladder_predicate(held->anchor))
    {
        return 0u;
    }
    if (held->frame == (unsigned int)FRAME_RELATION_COUNT)
    {
        settings[0] = KHW_FIRST_REGISTER;
        settings[1] = KHW_SECOND_REGISTER;
        return (stage == 0u) ? 2u : 0u;
    }
    if ((stage == 0u) || (held->kind[run] == KHW_KIND_ADDRESS))
    {
        return 0u;
    }
    char field[64] = "";
    unsigned int first = 0u;
    unsigned int bits = 0u;
    const int immediate = khw_field_holding(held->first[run], held->last[run], field, sizeof(field), &first, &bits) &&
                          (strcmp(field, "immediate") == 0);
    settings[0] = KHW_FIRST_REGISTER;
    settings[1] = immediate ? KHW_OFFSET_WORD : KHW_SECOND_REGISTER;
    return 2u;
}

// the slot's own form: the classified ladder form whose encoding the kernel's slot holds, or NULL where none is yet
static const KhwAnswered *khw_slot_form(void)
{
    const unsigned long long low = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 0u);
    const unsigned long long high = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 8u);
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const held = &s_named[at];
        if ((held->phase == KHW_PHASE_CLASSIFIED) && (held->runs != 0u) && (held->low == low) && (held->high == high) &&
            (held->frame == (unsigned int)FRAME_RELATION_COUNT))
        {
            return held;
        }
    }
    return NULL;
}

// The fields of the instruction word, as the part's .ksc gives them, that hold the slot's own form's operand runs, the
// register fields a frame form's address is looked for in, each once: into `first` and `bits`, KHW_RUNS of each. The
// count, 0 where the slot's form is not yet classified or the .ksc names no index or record register to probe with
static unsigned int khw_slot_fields(unsigned int *first, unsigned int *bits)
{
    const KhwAnswered *const slot_form = khw_slot_form();
    unsigned int count = 0u;
    if ((slot_form == NULL) || (khw_fixed_register("index") == KHW_FIXED_NONE) ||
        (khw_fixed_register("record") == KHW_FIXED_NONE))
    {
        return 0u;
    }
    for (unsigned int run = 0u; run < slot_form->runs; run += 1u)
    {
        char field[64];
        unsigned int field_first = 0u;
        unsigned int field_bits = 0u;
        if ((slot_form->place[run] == KHW_PLACE_NONE) ||
            !khw_field_holding(slot_form->first[run], slot_form->last[run], field, sizeof(field), &field_first,
                               &field_bits))
        {
            continue;
        }
        int held = 0;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            held = held || (first[at] == field_first);
        }
        if (!held && (count < KHW_RUNS))
        {
            first[count] = field_first;
            bits[count] = field_bits;
            count += 1u;
        }
    }
    return count;
}

// The register-runs of a ladder form read off the round carried, its questions at `at` in s_pool and after, two a
// run, each read at the stage's case: each run whose two register questions the part answered, in the same case, as
// two different words with the same answer carries a register and is an operand's; kept. A run that answers one as
// the form writes its result, and another as the form reads a word, is placed by which. The operand runs kept, each
// with its place in the relation's tuple, and the phase classified
static void khw_ladder_registers_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage)
{
    const unsigned int runs = held->runs;
    const KhwCandidate form = {held->anchor, held->swapped, held->signed_read};
    const unsigned int first_twice[2] = {stage->word[0][0], stage->word[0][0]};
    const unsigned int second_twice[2] = {stage->word[0][1], stage->word[0][1]};
    const unsigned long long answer = khw_candidate_answer(&form, stage->word[0]);
    const unsigned long long first_twice_answer = khw_candidate_answer(&form, first_twice);
    const unsigned long long second_twice_answer = khw_candidate_answer(&form, second_twice);
    held->runs = 0u;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        const RunQuestion *const first_question = &s_pool[at + (2u * run)];
        const RunQuestion *const second_question = &s_pool[at + (2u * run) + 1u];
        const int first_held = (first_question->outcome == RUN_ANSWERED);
        const int second_held = (second_question->outcome == RUN_ANSWERED);
        const unsigned long long first_answer = first_question->answered[stage->read_at];
        const unsigned long long second_answer = second_question->answered[stage->read_at];
        if (first_held && second_held && (first_answer != second_answer) && (held->runs < KHW_RUNS))
        {
            unsigned int place = KHW_PLACE_NONE;
            // the run that leaves the answer at the word the kernel's own register still holds wrote nothing: that is
            // where this form writes its result, the answer's place, past the words
            if ((first_answer == (unsigned long long)stage->word[0][1]) ||
                (second_answer == (unsigned long long)stage->word[0][1]))
            {
                place = s_ladder_words[held->anchor];
            }
            // a run that answers the case at the first register and the second word twice at the second reads the
            // first word the host put, and the other way round; the host put the relation's words swapped where the
            // form reads them so
            else if ((first_answer == answer) && (second_answer == second_twice_answer))
            {
                place = (held->swapped != 0) ? 1u : 0u;
            }
            else if ((second_answer == answer) && (first_answer == first_twice_answer))
            {
                place = (held->swapped != 0) ? 0u : 1u;
            }
            held->first[held->runs] = held->first[run];
            held->last[held->runs] = held->last[run];
            held->place[held->runs] = place;
            held->kind[held->runs] = KHW_KIND_REGISTER;
            held->runs += 1u;
        }
    }
    held->phase = KHW_PHASE_CLASSIFIED;
}

// The address of a frame form read off the first stage's round, two questions a field of the slot's form
// (khw_slot_fields), the field set whole to the index's register then to the record's, each read at the stage's case.
// A field is the address where the two answer apart and one of them answers the relation: a load reads the case's word
// through the index's register and the record's own through the record's, and a store writes the record through the
// record's register and the case through the index's. A load's is a place of its tuple, and a store's an operand the
// form holds with no place, since the record is where a store writes. The runs the turns found inside such a field
// give way to one run, the field, marked an address; every other run is left for the second stage. The turns alone
// cannot find an address field, since each bit of it turned reads through a register that holds no address
static void khw_frame_addresses_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage)
{
    const unsigned long long expected = stage->expected;
    unsigned int first[KHW_RUNS];
    unsigned int bits[KHW_RUNS];
    const unsigned int fields = khw_slot_fields(first, bits);
    for (unsigned int field = 0u; field < fields; field += 1u)
    {
        const RunQuestion *const by_index = &s_pool[at + (2u * field)];
        const RunQuestion *const by_record = &s_pool[at + (2u * field) + 1u];
        const int answered = (by_index->outcome == RUN_ANSWERED) && (by_record->outcome == RUN_ANSWERED);
        const unsigned long long from_index = by_index->answered[stage->read_at];
        const unsigned long long from_record = by_record->answered[stage->read_at];
        if (!answered || (from_index == from_record) || ((from_index != expected) && (from_record != expected)))
        {
            continue;
        }
        // the field takes the place of the runs inside it, in the order of first bits the runs keep
        const unsigned int last = first[field] + bits[field] - 1u;
        KhwAnswered kept = *held;
        kept.runs = 0u;
        int placed = 0;
        for (unsigned int run = 0u; run < held->runs; run += 1u)
        {
            if (!placed && (held->first[run] > first[field]) && (kept.runs < KHW_RUNS))
            {
                kept.first[kept.runs] = first[field];
                kept.last[kept.runs] = last;
                kept.place[kept.runs] = frame_place_address(held->frame);
                kept.kind[kept.runs] = KHW_KIND_ADDRESS;
                kept.runs += 1u;
                placed = 1;
            }
            if (((held->first[run] >= first[field]) && (held->last[run] <= last)) || (kept.runs == KHW_RUNS))
            {
                continue;
            }
            kept.first[kept.runs] = held->first[run];
            kept.last[kept.runs] = held->last[run];
            kept.place[kept.runs] = held->place[run];
            kept.kind[kept.runs] = held->kind[run];
            kept.runs += 1u;
        }
        if (!placed && (kept.runs < KHW_RUNS))
        {
            kept.first[kept.runs] = first[field];
            kept.last[kept.runs] = last;
            kept.place[kept.runs] = frame_place_address(held->frame);
            kept.kind[kept.runs] = KHW_KIND_ADDRESS;
            kept.runs += 1u;
        }
        *held = kept;
    }
}

// 1 where a run of `held` stands at every place of its relation's tuple, exit's guard apart, since an exit with no
// guard holds no run; a frame form holding no run at a place answered no relation it holds a field of
static int khw_frame_whole(const KhwAnswered *held)
{
    const unsigned int places[4] = {frame_place_destination(held->frame), frame_place_address(held->frame),
                                    frame_place_number(held->frame), frame_place_register(held->frame)};
    for (unsigned int each = 0u; each < 4u; each += 1u)
    {
        int found = (places[each] == FRAME_PLACE_NONE);
        for (unsigned int run = 0u; (run < held->runs) && !found; run += 1u)
        {
            found = (held->place[run] == places[each]);
        }
        if (!found)
        {
            return 0;
        }
    }
    return 1;
}

// The register-runs of a frame form read off the second stage's round, its settings a run (khw_run_settings) in
// their order, each read at the stage's case; an address run is kept as the first stage marked it. The answer where
// the slot writes nothing is the case's second word, `nothing`, and the relation's answer is `expected`. A run inside
// the immediate field is a number where the relation is number and the answers are the settings themselves, and the
// offset where the relation holds a number place and the two settings answer apart. Any other run is the destination
// where 0 leaves `nothing` and the register the kernel stores answers the relation, or the guard where the relation is
// exit or the run lies inside the field the .ksc names the guard, since P0 is false at the slot and PT is every lane,
// exit's guard a place of its tuple and any other relation's a predicate with none; the register a store keeps where
// the relation is store and that register answers the relation; and an operand with no place where the two answer
// apart, its bits kept as seen. Each kept with its kind, and the phase classified, or void where a place of the tuple
// holds no run
static void khw_frame_registers_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage)
{
    const unsigned int runs = held->runs;
    const unsigned long long nothing = (unsigned long long)stage->word[0][1];
    const unsigned long long expected = stage->expected;
    unsigned int asked = at;
    KhwAnswered kept = *held;
    kept.runs = 0u;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        unsigned int place = held->place[run];
        unsigned int kind = held->kind[run];
        unsigned int settings[KHW_SETTINGS];
        const unsigned int probes = khw_run_settings(held, 1u, run, settings);
        if (probes != 0u)
        {
            int answered[KHW_SETTINGS];
            unsigned long long answer[KHW_SETTINGS];
            for (unsigned int setting = 0u; setting < probes; setting += 1u)
            {
                const RunQuestion *const question = &s_pool[asked + setting];
                answered[setting] = (question->outcome == RUN_ANSWERED);
                answer[setting] = question->answered[stage->read_at];
            }
            asked += probes;
            const int both = answered[0] && answered[1];
            const int immediate = (settings[1] == KHW_OFFSET_WORD);
            place = KHW_PLACE_NONE;
            kind = KHW_KIND_REGISTER;
            if (immediate && (held->frame == FRAME_NUMBER) && both && (answer[0] == 0ull) &&
                (answer[1] == KHW_OFFSET_WORD))
            {
                place = frame_place_number(held->frame);
                kind = KHW_KIND_IMMEDIATE;
            }
            else if (immediate && (held->frame != FRAME_NUMBER) &&
                     (frame_place_number(held->frame) != FRAME_PLACE_NONE) && both && (answer[0] != answer[1]))
            {
                place = frame_place_number(held->frame);
                kind = KHW_KIND_IMMEDIATE;
            }
            else if (immediate && (held->frame == FRAME_DIM) && both && (answer[0] != answer[1]))
            {
                // the constant bank's operand: its offset read from the bank answers the block size at 0 and the
                // bank's next word past it, a run the form holds and no parameter takes
                kind = KHW_KIND_CONSTANT;
            }
            else if (!immediate && both && (answer[0] == nothing) && (answer[1] == expected))
            {
                // a run inside the field the .ksc names the guard answers so too, P0 false at the slot and PT every
                // lane: it is the guard, exit's place where the relation is exit and a predicate with no place
                // elsewhere, never the destination
                unsigned int guard_first = 0u;
                unsigned int guard_bits = 0u;
                const int in_guard = khw_field_read("guard", &guard_first, &guard_bits) &&
                                     (held->first[run] >= guard_first) &&
                                     (held->last[run] < (guard_first + guard_bits));
                place = (held->frame == FRAME_EXIT) ? frame_place_guard(held->frame)
                        : in_guard                  ? KHW_PLACE_NONE
                                                    : frame_place_destination(held->frame);
                kind = ((held->frame == FRAME_EXIT) || in_guard) ? KHW_KIND_PREDICATE : KHW_KIND_REGISTER;
            }
            else if (!immediate && (held->frame == FRAME_STORE) && both && (answer[1] == expected) &&
                     (answer[0] != answer[1]))
            {
                place = frame_place_register(held->frame);
            }
            else if (immediate || !(both && (answer[0] != answer[1])))
            {
                continue;
            }
            // an operand with no place that reaches into the field the .ksc names the guard is a predicate, which the
            // vendor writes before the operation, as nothing at every predicate, and with the bit past the field's
            // for a predicate read the other way
            unsigned int guard_first = 0u;
            unsigned int guard_bits = 0u;
            kind = ((place == KHW_PLACE_NONE) && khw_field_read("guard", &guard_first, &guard_bits) &&
                    (held->first[run] <= (guard_first + guard_bits - 1u)) && (held->last[run] >= guard_first))
                       ? KHW_KIND_PREDICATE
                       : kind;
        }
        if (kept.runs < KHW_RUNS)
        {
            kept.first[kept.runs] = held->first[run];
            kept.last[kept.runs] = held->last[run];
            kept.place[kept.runs] = place;
            kept.kind[kept.runs] = kind;
            kept.runs += 1u;
        }
    }
    *held = kept;
    held->phase = khw_frame_whole(held) ? KHW_PHASE_CLASSIFIED : KHW_PHASE_VOID;
}

// the register-runs of `held` read off the round `stage_number` carried, as its relation's kind reads them: a ladder
// form's in the first stage, a frame form's addresses in the first and the rest in the second
// The runs of a predicate form read off its own encoding, no question asked: a run inside a field the .ksc names
// for a predicate that holds P0 is the destination, the answer's place, a predicate, and one holding another
// predicate an operand with no place; a run inside a register field holding the first word's register is the first
// word's place and the second word's register the second's, the other way round where the form reads the words
// swapped; every other run is an operand with no place, kept as seen. The phase classified where a run stands at every
// place of the relation's tuple, void otherwise
static void khw_predicate_runs_read(KhwAnswered *held)
{
    static const char *const s_predicate_fields[] = {"guard", "pl", "pu", "pv", "pg", "po", "pt"};
    const unsigned int words = s_ladder_words[held->anchor];
    for (unsigned int run = 0u; run < held->runs; run += 1u)
    {
        char field[64] = "";
        unsigned int first = 0u;
        unsigned int bits = 0u;
        held->place[run] = KHW_PLACE_NONE;
        held->kind[run] = KHW_KIND_REGISTER;
        if (!khw_field_holding(held->first[run], held->last[run], field, sizeof(field), &first, &bits))
        {
            continue;
        }
        const unsigned long long value = khw_run_read(held->low, held->high, first, first + bits - 1u);
        int predicate = 0;
        for (unsigned int each = 0u; each < (sizeof(s_predicate_fields) / sizeof(s_predicate_fields[0])); each += 1u)
        {
            predicate = predicate || (strcmp(field, s_predicate_fields[each]) == 0);
        }
        if (predicate)
        {
            held->kind[run] = KHW_KIND_PREDICATE;
            held->place[run] = ((value == 0ull) && (strcmp(field, "guard") != 0)) ? words : KHW_PLACE_NONE;
        }
        else if (value == KHW_FIRST_REGISTER)
        {
            held->place[run] = (held->swapped != 0) ? 1u : 0u;
        }
        else if (value == KHW_SECOND_REGISTER)
        {
            held->place[run] = (held->swapped != 0) ? 0u : 1u;
        }
    }
    held->phase = khw_places_whole(held->place, held->runs, words) ? KHW_PHASE_CLASSIFIED : KHW_PHASE_VOID;
}

static void khw_registers_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage, unsigned int stage_number)
{
    if (held->frame != (unsigned int)FRAME_RELATION_COUNT)
    {
        (stage_number == 0u) ? khw_frame_addresses_read(held, at, stage) : khw_frame_registers_read(held, at, stage);
        return;
    }
    if (ladder_predicate(held->anchor))
    {
        khw_predicate_runs_read(held);
        return;
    }
    khw_ladder_registers_read(held, at, stage);
}

// the forms of `s_staged` from `first` to `staged` read off the round carried, each as `stage_number` reads it; the
// count whose phase is then classified
static unsigned int khw_staged_read(KhwStaged *staged_forms, unsigned int first, unsigned int staged,
                                    unsigned int stage_number)
{
    unsigned int classified = 0u;
    for (unsigned int read = first; read < staged; read += 1u)
    {
        khw_registers_read(staged_forms[read].held, staged_forms[read].at, &staged_forms[read], stage_number);
        classified += (staged_forms[read].held->phase == KHW_PHASE_CLASSIFIED) ? 1u : 0u;
    }
    return classified;
}

// the questions a stage asks of `held`, pooled into `stage_held` where it is not NULL: a frame form's first stage two
// a field of the slot's form, the field set whole to the index's register and to the record's; otherwise each run's
// settings for the stage, the run set to each. The count
static unsigned int khw_stage_pooled(const KhwAnswered *held, unsigned int stage, const KhwStaged *stage_held)
{
    unsigned int asks = 0u;
    if ((held->frame != (unsigned int)FRAME_RELATION_COUNT) && (stage == 0u))
    {
        unsigned int first[KHW_RUNS];
        unsigned int bits[KHW_RUNS];
        const unsigned int fields = khw_slot_fields(first, bits);
        const unsigned int settings[2] = {khw_fixed_register("index"), khw_fixed_register("record")};
        for (unsigned int field = 0u; field < fields; field += 1u)
        {
            for (unsigned int setting = 0u; setting < 2u; setting += 1u)
            {
                unsigned long long low = held->low;
                unsigned long long high = held->high;
                khw_run_write(&low, &high, first[field], first[field] + bits[field] - 1u, settings[setting]);
                if (stage_held != NULL)
                {
                    khw_staged_pooled(stage_held, low, high);
                }
                asks += 1u;
            }
        }
        return asks;
    }
    for (unsigned int run = 0u; run < held->runs; run += 1u)
    {
        unsigned int settings[KHW_SETTINGS];
        const unsigned int probes = khw_run_settings(held, stage, run, settings);
        for (unsigned int setting = 0u; setting < probes; setting += 1u)
        {
            unsigned long long low = held->low;
            unsigned long long high = held->high;
            khw_run_write(&low, &high, held->first[run], held->last[run], settings[setting]);
            if (stage_held != NULL)
            {
                khw_staged_pooled(stage_held, low, high);
            }
            asks += 1u;
        }
    }
    return asks;
}

// The register-runs of every turned form of `forms`, `count` of them, the second field-finding pass, in shared
// rounds over KHW_STAGES stages: in each stage, each form's turns found set to each of the settings the stage gives
// the run (khw_run_settings), over the same case, a question a setting a run, as many forms a round as the pool
// holds; and what came back read into each form's operand runs, the frame forms' addresses after the first stage and
// every form's phase classified, or void, after its last. The count of forms classified
unsigned int khw_fields_registers(KhwAnswered *const *forms, unsigned int count)
{
    static KhwStaged s_staged[KHW_FORMS];
    unsigned int classified = 0u;
    for (unsigned int stage_number = 0u; stage_number < KHW_STAGES; stage_number += 1u)
    {
        unsigned int staged = 0u;
        unsigned int first = 0u;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            KhwStaged *const stage = &s_staged[staged];
            memset(stage, 0, sizeof(*stage));
            const unsigned int asks = khw_stage_pooled(forms[at], stage_number, NULL);
            // a predicate form asks nothing: its runs are read off its own encoding in the first stage
            if ((asks == 0u) && (stage_number == 0u) && (forms[at]->frame == (unsigned int)FRAME_RELATION_COUNT) &&
                ladder_predicate(forms[at]->anchor))
            {
                khw_predicate_runs_read(forms[at]);
                classified += (forms[at]->phase == KHW_PHASE_CLASSIFIED) ? 1u : 0u;
                continue;
            }
            if ((asks == 0u) || !khw_turning_case(forms[at], stage))
            {
                continue;
            }
            if ((s_pooled + asks) > SCHEDULER_ROUND_MOST)
            {
                khw_round_carried();
                classified += khw_staged_read(s_staged, first, staged, stage_number);
                first = staged;
                s_pooled = 0u;
            }
            stage->at = s_pooled;
            khw_stage_pooled(forms[at], stage_number, stage);
            staged += 1u;
        }
        if (staged != first)
        {
            if (s_pooled != 0u)
            {
                khw_round_carried();
            }
            classified += khw_staged_read(s_staged, first, staged, stage_number);
            s_pooled = 0u;
        }
    }
    return classified;
}

// A form's fields asked whole, the turns then the register-runs, for the learn loop that asks both of a form in one
// run. The split passes ask them apart over every form at once, a cross-check reading the register-runs the turns
// find between
void khw_fields_asked(KhwAnswered *held)
{
    KhwAnswered *const one[1] = {held};
    if (khw_fields_turns(one, 1u) != 0u)
    {
        khw_fields_registers(one, 1u);
    }
}
