// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// relation.c: the relation: the union of the candidates written, a form's relation found, read and asked, the first round, the turning case, and the turns and the fields asked
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
int khw_relation_of(const RunQuestion *question, KhwAnswered *held)
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
        for (unsigned int place = 0u; place < s_union_cases; place += 1u)
        {
            const unsigned int host = khw_candidate_answer(&s_candidate[candidate], s_union_word[place]);
            const AnswerBracket bracket = {1, host, host};
            s_fails[candidate][place] =
                (answer_read(&bracket, question->outcome, question->answered[place]) != ANSWER_ALIKE) ? 1u : 0u;
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
    return khw_relation_of(&s_question, held);
}

// the relation `low` and `high` at the slot answers, read as khw_relation_read reads it. 1 where one relation answered
int khw_relation_asked(unsigned long long low, unsigned long long high, KhwAnswered *held)
{
    held->low = low;
    held->high = high;
    khw_code_put(s_code, s_slot, low, high);
    return khw_relation_read(held);
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
    const int related = khw_relation_of(&s_pool[kernel_at], kernel);
    *slot = (s_kernel_slot < s_kernel_places) ? s_kernel_slot : *slot;
    for (unsigned int at = kernel_at + 1u; (at < s_pooled) && (*slot == s_kernel_places); at += 1u)
    {
        *slot = ((s_pool[at].outcome == RUN_ANSWERED) && (s_pool[at].answered[0] == (unsigned long long)slot_case[0][1]))
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
    const int timed = khw_asked(s_word, count, RUN_SLOT_NONE, s_answered);
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

// The turning case of `held` into `stage`: for a ladder form, the first case of its relation whose two words differ
// from each other and whose answer differs from both, read in the order the form reads them, so that a changed answer
// is a changed answer and not a coincidence; for a frame form, the words 3 and 5 over the frame's shape. 1, or 0
// where the relation holds no such case
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
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        const LadderQuestion *const question = &s_ladder_cases[at];
        unsigned int gave = 0u;
        if ((question->anchor != held->anchor) || (question->words < 2u) ||
            !ladder_answer(held->anchor, question->word, question->words, &gave) ||
            (question->word[0] == question->word[1]) || (gave == question->word[0]) || (gave == question->word[1]))
        {
            continue;
        }
        stage->word[0][0] = (held->swapped != 0) ? question->word[1] : question->word[0];
        stage->word[0][1] = (held->swapped != 0) ? question->word[0] : question->word[1];
        stage->expected = gave;
        return 1;
    }
    return 0;
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
    return (stage->held->frame != (unsigned int)FRAME_RELATION_COUNT)
               ? khw_frame_pooled(low, high, s_words, stage->count)
               : khw_pooled(s_slot, low, high, s_words, stage->count);
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
// `read_at`: a bit whose turning changed the answer `base` gathered into the runs the operands might sit in, none yet
// asked whether it carries a register. The runs are the candidates a register pass reads, and the phase turned
static void khw_turns_read(KhwAnswered *held, unsigned int at, unsigned long long base, unsigned int read_at)
{
    int turned[KHW_TURN_LAST + 1u];
    for (unsigned int bit = KHW_TURN_FIRST; bit <= KHW_TURN_LAST; bit += 1u)
    {
        const RunQuestion *const question = &s_pool[at + (bit - KHW_TURN_FIRST)];
        turned[bit] = ((question->outcome == RUN_ANSWERED) && (question->answered[read_at] != base)) ? 1 : 0;
    }
    held->runs = 0u;
    // the adjacent bits that changed the answer, the candidate runs a register pass reads
    for (unsigned int bit = KHW_TURN_FIRST; (bit <= KHW_TURN_LAST) && (held->runs < KHW_RUNS); bit += 1u)
    {
        if (turned[bit] == 0)
        {
            continue;
        }
        unsigned int end = bit;
        while ((end < KHW_TURN_LAST) && (turned[end + 1u] != 0))
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

// the settings a run is probed with: a ladder form's runs with the two registers the kernel leaves the case's words
// in; a frame form's with those, the index's and the record's registers the part's .ksc names, and 8, an aligned
// offset a number run is read against 0 by. Into `settings`, which holds KHW_SETTINGS: the count
#define KHW_SETTINGS 5u

static unsigned int khw_settings_of(const KhwAnswered *held, unsigned int *settings)
{
    settings[0] = KHW_FIRST_REGISTER;
    settings[1] = KHW_SECOND_REGISTER;
    if (held->frame == (unsigned int)FRAME_RELATION_COUNT)
    {
        return 2u;
    }
    const unsigned int index = khw_fixed_register("index");
    const unsigned int record = khw_fixed_register("record");
    settings[2] = (index == KHW_FIXED_NONE) ? 0u : index;
    settings[3] = (record == KHW_FIXED_NONE) ? 0u : record;
    settings[4] = 8u;
    return KHW_SETTINGS;
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

// The register-runs of a frame form read off the round carried, KHW_SETTINGS questions a run in the settings' order,
// each read at the stage's case. The answer where the slot writes nothing is the case's second word, `nothing`, and
// the relation's answer is `expected`. A run is the destination where its first setting leaves `nothing` and its
// second the relation's answer, or the guard where the relation is exit, since P0 is false at the slot and PT is
// every lane; a number where the relation is number and the answers are the settings themselves; the address where
// the two registers are refused and the index and the record answer apart; the register a store keeps where the
// relation is store and the second setting answers the relation; the offset where the relation holds a number place
// past those and the settings 0 and 8 answer apart; and an operand with no place where the two registers answer
// apart, its bits kept as seen. Each kept with its kind, and the phase classified
static void khw_frame_registers_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage)
{
    const unsigned int runs = held->runs;
    const unsigned long long nothing = (unsigned long long)stage->word[0][1];
    const unsigned long long expected = stage->expected;
    held->runs = 0u;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        int answered[KHW_SETTINGS];
        unsigned long long answer[KHW_SETTINGS];
        for (unsigned int setting = 0u; setting < KHW_SETTINGS; setting += 1u)
        {
            const RunQuestion *const question = &s_pool[at + (KHW_SETTINGS * run) + setting];
            answered[setting] = (question->outcome == RUN_ANSWERED);
            answer[setting] = question->answered[stage->read_at];
        }
        const int registers = answered[0] && answered[1];
        unsigned int place = KHW_PLACE_NONE;
        unsigned int kind = KHW_KIND_REGISTER;
        if (registers && (answer[0] == nothing) && (answer[1] == expected))
        {
            place = (held->frame == FRAME_EXIT) ? frame_place_guard(held->frame) : frame_place_destination(held->frame);
            kind = (held->frame == FRAME_EXIT) ? KHW_KIND_PREDICATE : KHW_KIND_REGISTER;
        }
        else if ((held->frame == FRAME_NUMBER) && registers && answered[4] && (answer[0] == 0ull) &&
                 (answer[1] == 7ull) && (answer[4] == 8ull))
        {
            place = frame_place_number(held->frame);
            kind = KHW_KIND_IMMEDIATE;
        }
        else if (!registers && answered[2] && answered[3] && (answer[2] != answer[3]))
        {
            place = frame_place_address(held->frame);
            kind = KHW_KIND_ADDRESS;
        }
        else if ((held->frame == FRAME_STORE) && registers && (answer[1] == expected) && (answer[0] != answer[1]))
        {
            place = frame_place_register(held->frame);
        }
        else if ((frame_place_number(held->frame) != FRAME_PLACE_NONE) && (held->frame != FRAME_NUMBER) &&
                 answered[0] && answered[4] && (answer[0] != answer[4]))
        {
            place = frame_place_number(held->frame);
            kind = KHW_KIND_IMMEDIATE;
        }
        else if (!(registers && (answer[0] != answer[1])))
        {
            continue;
        }
        if (held->runs < KHW_RUNS)
        {
            held->first[held->runs] = held->first[run];
            held->last[held->runs] = held->last[run];
            held->place[held->runs] = place;
            held->kind[held->runs] = kind;
            held->runs += 1u;
        }
    }
    held->phase = KHW_PHASE_CLASSIFIED;
}

// the register-runs of `held` read off the round carried, as its relation's kind reads them
static void khw_registers_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage)
{
    if (held->frame != (unsigned int)FRAME_RELATION_COUNT)
    {
        khw_frame_registers_read(held, at, stage);
        return;
    }
    khw_ladder_registers_read(held, at, stage);
}

// The register-runs of every turned form of `forms`, `count` of them, the second field-finding pass, in shared
// rounds: each form's turns found set to each of its settings, over the same case, a question a setting a run, as
// many forms a round as the pool holds; and what came back read into each form's operand runs, its phase
// classified. The count of forms classified
unsigned int khw_fields_registers(KhwAnswered *const *forms, unsigned int count)
{
    static KhwStaged s_staged[KHW_FORMS];
    unsigned int staged = 0u;
    unsigned int classified = 0u;
    unsigned int first = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        KhwStaged *const stage = &s_staged[staged];
        memset(stage, 0, sizeof(*stage));
        if (!khw_turning_case(forms[at], stage))
        {
            continue;
        }
        unsigned int settings[KHW_SETTINGS];
        const unsigned int probes = khw_settings_of(forms[at], settings);
        if ((s_pooled + (probes * forms[at]->runs)) > SCHEDULER_ROUND_MOST)
        {
            khw_round_carried();
            for (unsigned int read = first; read < staged; read += 1u)
            {
                khw_registers_read(s_staged[read].held, s_staged[read].at, &s_staged[read]);
            }
            classified += staged - first;
            first = staged;
            s_pooled = 0u;
        }
        stage->at = s_pooled;
        for (unsigned int run = 0u; run < forms[at]->runs; run += 1u)
        {
            for (unsigned int setting = 0u; setting < probes; setting += 1u)
            {
                unsigned long long low = forms[at]->low;
                unsigned long long high = forms[at]->high;
                khw_run_write(&low, &high, forms[at]->first[run], forms[at]->last[run], settings[setting]);
                khw_staged_pooled(stage, low, high);
            }
        }
        staged += 1u;
    }
    if (staged != first)
    {
        if (s_pooled != 0u)
        {
            khw_round_carried();
        }
        for (unsigned int read = first; read < staged; read += 1u)
        {
            khw_registers_read(s_staged[read].held, s_staged[read].at, &s_staged[read]);
        }
        classified += staged - first;
        s_pooled = 0u;
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
