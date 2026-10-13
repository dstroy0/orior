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
// otherwise unchanged, over the cases khw_union_written writes, its relation read by the gate. The slot's are
// the kernel with each place but the unreached in turn holding the encoding its unreached places hold, over one case,
// and the slot is the first place whose answer comes back as the case's second word, because nothing then wrote the
// register the kernel stores. The relation that answered is then put once more over its own cases, its launches timed
// into `nanoseconds`, 0 where the part gave no time. 1 where the kernel answered a relation, its slot through `slot`,
// past every place where none answered so
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
    for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
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

// the first case of `anchor` whose two words differ from each other and whose answer differs from both, read in the
// order `swapped` says: the case a turn is asked over, where a changed answer is a changed answer and not a
// coincidence. 1, or 0 where the relation holds no such case
static int khw_turning_case(unsigned int anchor, int swapped, unsigned int *word, unsigned int *answered)
{
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        const LadderQuestion *const held = &s_ladder_cases[at];
        unsigned int gave = 0u;
        if ((held->anchor != anchor) || (held->words < 2u) || !ladder_answer(anchor, held->word, held->words, &gave) ||
            (held->word[0] == held->word[1]) || (gave == held->word[0]) || (gave == held->word[1]))
        {
            continue;
        }
        word[0] = (swapped != 0) ? held->word[1] : held->word[0];
        word[1] = (swapped != 0) ? held->word[0] : held->word[1];
        *answered = gave;
        return 1;
    }
    return 0;
}

// The turns of `held`: each bit from KHW_TURN_FIRST to KHW_TURN_LAST turned one at a time over `word`, each a
// question of the slot, pooled into the round being written. The code a turned bit holds is the form's with that one
// bit over, and no answer is needed to put it: the carrier, with every one of them, waits on the vendor's reader
// before the part sees them. The pooled question of the first turn is at `at` in s_pool, and the rest follow it in
// their order
static void khw_turns_pooled(const KhwAnswered *held, const unsigned int word[1][2])
{
    for (unsigned int bit = KHW_TURN_FIRST; bit <= KHW_TURN_LAST; bit += 1u)
    {
        unsigned long long low = held->low;
        unsigned long long high = held->high;
        khw_run_write(&low, &high, bit, bit, khw_run_read(low, high, bit, bit) ^ 1ull);
        khw_pooled(s_slot, low, high, word, 1u);
    }
}

// The turns of `held` read off the round carried, its turns at `at` in s_pool and after: a bit whose turning changed
// the answer `base` gathered into the runs the operands might sit in, none yet asked whether it carries a register.
// The runs are the candidates a register pass reads, and the phase turned
static void khw_turns_read(KhwAnswered *held, unsigned int at, unsigned long long base)
{
    int turned[KHW_TURN_LAST + 1u];
    for (unsigned int bit = KHW_TURN_FIRST; bit <= KHW_TURN_LAST; bit += 1u)
    {
        const RunQuestion *const question = &s_pool[at + (bit - KHW_TURN_FIRST)];
        turned[bit] = ((question->outcome == RUN_ANSWERED) && (question->answered[0] != base)) ? 1 : 0;
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
        held->runs += 1u;
        bit = end;
    }
    held->phase = KHW_PHASE_TURNED;
}

// The turns of `held` over `word` put and carried as a round of their own, what came back left in s_pool, a question
// a turned bit in its order
void khw_turns_asked(const KhwAnswered *held, const unsigned int word[1][2])
{
    s_pooled = 0u;
    khw_turns_pooled(held, word);
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

// What one form holds through a stage of a pass: the form, its turning case `word` and the answer `expected` of it,
// where its questions begin in the round being written, and the answer the part gave its turning case
typedef struct
{
    KhwAnswered *held;
    unsigned int word[1][2];
    unsigned int expected;
    unsigned int at;
    unsigned long long base;
} KhwStaged;

// the forms of `staged` from `first` to `count` whose turning cases the round carried answered as the host expected:
// each keeps the part's answer as its base and goes on, and one the part answered otherwise or not at all is left
// where it is. The count that go on
static unsigned int khw_turning_read(KhwStaged *staged, unsigned int first, unsigned int count)
{
    unsigned int on = 0u;
    for (unsigned int at = first; at < count; at += 1u)
    {
        const RunQuestion *const question = &s_pool[staged[at].at];
        const int answered = (question->outcome == RUN_ANSWERED) &&
                             (question->answered[0] == (unsigned long long)staged[at].expected);
        staged[at].base = question->answered[0];
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
        unsigned int word[1][2];
        unsigned int expected = 0u;
        if (!khw_turning_case(forms[at]->anchor, forms[at]->swapped, word[0], &expected))
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
        KhwStaged *const stage = &s_staged[staged];
        stage->held = forms[at];
        stage->word[0][0] = word[0][0];
        stage->word[0][1] = word[0][1];
        stage->expected = expected;
        stage->at = khw_pooled(s_slot, forms[at]->low, forms[at]->high, word, 1u);
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
                khw_turns_read(s_staged[read].held, s_staged[read].at, s_staged[read].base);
            }
            turned += at - first;
            first = at;
            s_pooled = 0u;
        }
        s_staged[at].at = s_pooled;
        khw_turns_pooled(s_staged[at].held, s_staged[at].word);
    }
    if (s_pooled != 0u)
    {
        khw_round_carried();
        for (unsigned int read = first; read < on; read += 1u)
        {
            khw_turns_read(s_staged[read].held, s_staged[read].at, s_staged[read].base);
        }
        turned += on - first;
        s_pooled = 0u;
    }
    return turned;
}

// The register-runs of `held` read off the round carried, its questions at `at` in s_pool and after, two a run: each
// run whose two register questions the part answered, in the same case, as two different words with the same answer
// carries a register and is an operand's; kept. A run that answers one as the form writes its result, and another
// as the form reads a word, is placed by which. The operand runs kept, each with its place in the relation's tuple,
// and the phase classified
static void khw_registers_read(KhwAnswered *held, unsigned int at, const unsigned int word[1][2])
{
    const unsigned int runs = held->runs;
    const KhwCandidate form = {held->anchor, held->swapped, held->signed_read};
    const unsigned int first_twice[2] = {word[0][0], word[0][0]};
    const unsigned int second_twice[2] = {word[0][1], word[0][1]};
    const unsigned long long answer = khw_candidate_answer(&form, word[0]);
    const unsigned long long first_twice_answer = khw_candidate_answer(&form, first_twice);
    const unsigned long long second_twice_answer = khw_candidate_answer(&form, second_twice);
    held->runs = 0u;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        const RunQuestion *const first_question = &s_pool[at + (2u * run)];
        const RunQuestion *const second_question = &s_pool[at + (2u * run) + 1u];
        const int first_held = (first_question->outcome == RUN_ANSWERED);
        const int second_held = (second_question->outcome == RUN_ANSWERED);
        const unsigned long long first_answer = first_question->answered[0];
        const unsigned long long second_answer = second_question->answered[0];
        if (first_held && second_held && (first_answer != second_answer) && (held->runs < KHW_RUNS))
        {
            unsigned int place = KHW_PLACE_NONE;
            // the run that leaves the answer at the word the kernel's own register still holds wrote nothing: that is
            // where this form writes its result, the answer's place, past the words
            if ((first_answer == (unsigned long long)word[0][1]) || (second_answer == (unsigned long long)word[0][1]))
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
            held->runs += 1u;
        }
    }
    held->phase = KHW_PHASE_CLASSIFIED;
}

// The register-runs of every turned form of `forms`, `count` of them, the second field-finding pass, in shared
// rounds: each form's turns found set to the number of each of the two registers the kernel left a case word in,
// over the same case, two questions a run, as many forms a round as the pool holds; and what came back read into each
// form's operand runs, its phase classified. The count of forms classified
unsigned int khw_fields_registers(KhwAnswered *const *forms, unsigned int count)
{
    static KhwStaged s_staged[KHW_FORMS];
    unsigned int staged = 0u;
    unsigned int classified = 0u;
    unsigned int first = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        KhwStaged *const stage = &s_staged[staged];
        stage->held = forms[at];
        if (!khw_turning_case(forms[at]->anchor, forms[at]->swapped, stage->word[0], &stage->expected))
        {
            continue;
        }
        if ((s_pooled + (2u * forms[at]->runs)) > SCHEDULER_ROUND_MOST)
        {
            khw_round_carried();
            for (unsigned int read = first; read < staged; read += 1u)
            {
                khw_registers_read(s_staged[read].held, s_staged[read].at, s_staged[read].word);
            }
            classified += staged - first;
            first = staged;
            s_pooled = 0u;
        }
        stage->at = s_pooled;
        for (unsigned int run = 0u; run < forms[at]->runs; run += 1u)
        {
            unsigned long long low = forms[at]->low;
            unsigned long long high = forms[at]->high;
            khw_run_write(&low, &high, forms[at]->first[run], forms[at]->last[run], KHW_FIRST_REGISTER);
            khw_pooled(s_slot, low, high, stage->word, 1u);
            low = forms[at]->low;
            high = forms[at]->high;
            khw_run_write(&low, &high, forms[at]->first[run], forms[at]->last[run], KHW_SECOND_REGISTER);
            khw_pooled(s_slot, low, high, stage->word, 1u);
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
            khw_registers_read(s_staged[read].held, s_staged[read].at, s_staged[read].word);
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
