// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// relation_registers.c: the relation: the fields queried
#include "khw_learn_internal.h"

// KHW_SETTINGS: the count, 0 where the stage queries the run nothing. A ladder form's runs take the two registers the
// kernel leaves the case's words in, in the first stage alone. A frame form's runs take first the index's and the
// record's registers the part's .ksc names, two that contain addresses, so that no run is read as an address through a
// register containing none; then, the address runs apart, a run inside the field the .ksc names `immediate` takes 0 and
// KHW_OFFSET_WORD, the aligned offset of the record's second word, which a load and a store answer apart and a number
// answers as itself, and every other run 0 and the register the kernel stores
#define KHW_OFFSET_WORD 4u

unsigned int khw_run_settings(const KhwAnswered *form, unsigned int stage, unsigned int run, unsigned int *settings)
{
    // a predicate form's runs are read off its own encoding, and no setting is queried
    if ((form->frame == (unsigned int)FRAME_RELATION_COUNT) && ladder_predicate(form->anchor))
    {
        return 0u;
    }
    if (form->frame == (unsigned int)FRAME_RELATION_COUNT)
    {
        settings[0] = KHW_FIRST_REGISTER;
        settings[1] = KHW_SECOND_REGISTER;
        return (stage == 0u) ? 2u : 0u;
    }
    if ((stage == 0u) || (form->type[run] == KHW_KIND_ADDRESS))
    {
        return 0u;
    }
    char field[64] = "";
    unsigned int first = 0u;
    unsigned int bits = 0u;
    const int immediate = khw_field_covering(form->first[run], form->last[run], field, sizeof(field), &first, &bits) &&
                          (strcmp(field, "immediate") == 0);
    settings[0] = KHW_FIRST_REGISTER;
    settings[1] = immediate ? KHW_OFFSET_WORD : KHW_SECOND_REGISTER;
    return 2u;
}

// the probe place's own form: the classified ladder form whose encoding the kernel's probe place contains, or NULL
// where none is yet
static const KhwAnswered *khw_probe_form(void)
{
    const unsigned long long low = khw_word_read(&s_kernel_text[s_probe_place * KHW_INSTRUCTION], 0u);
    const unsigned long long high = khw_word_read(&s_kernel_text[s_probe_place * KHW_INSTRUCTION], 8u);
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const form = &s_named[at];
        if ((form->phase == KHW_PHASE_CLASSIFIED) && (form->runs != 0u) && (form->low == low) && (form->high == high) &&
            (form->frame == (unsigned int)FRAME_RELATION_COUNT))
        {
            return form;
        }
    }
    return NULL;
}

// The fields of the instruction word, as the part's .ksc gives them, that contain the probe place's own form's operand
// runs, the register fields a frame form's address is looked for in, each once: into `first` and `bits`, KHW_RUNS of
// each. The count, 0 where the probe place's form is not yet classified or the .ksc names no index or record register
// to probe with
unsigned int khw_probe_fields(unsigned int *first, unsigned int *bits)
{
    const KhwAnswered *const probed_form = khw_probe_form();
    unsigned int count = 0u;
    if ((probed_form == NULL) || (khw_fixed_register("index") == KHW_FIXED_NONE) ||
        (khw_fixed_register("record") == KHW_FIXED_NONE))
    {
        return 0u;
    }
    for (unsigned int run = 0u; run < probed_form->runs; run += 1u)
    {
        char field[64];
        unsigned int field_first = 0u;
        unsigned int field_bits = 0u;
        if ((probed_form->place[run] == KHW_PLACE_NONE) ||
            !khw_field_covering(probed_form->first[run], probed_form->last[run], field, sizeof(field), &field_first,
                                &field_bits))
        {
            continue;
        }
        int found = 0;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            found = found || (first[at] == field_first);
        }
        if (!found && (count < KHW_RUNS))
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
// two different words with the same answer carries a register and is an operand's; retained. A run that answers one as
// the form writes its result, and another as the form reads a word, is placed by which. The operand runs retained, each
// with its place in the relation's tuple, and the phase classified
void khw_ladder_registers_read(KhwAnswered *ladder_form, unsigned int at, const KhwStaged *stage)
{
    const unsigned int runs = ladder_form->runs;
    const KhwCandidate form = {ladder_form->anchor, ladder_form->swapped, ladder_form->signed_read};
    const unsigned int first_twice[2] = {stage->word[0][0], stage->word[0][0]};
    const unsigned int second_twice[2] = {stage->word[0][1], stage->word[0][1]};
    const unsigned long long answer = khw_candidate_answer(&form, stage->word[0]);
    const unsigned long long first_twice_answer = khw_candidate_answer(&form, first_twice);
    const unsigned long long second_twice_answer = khw_candidate_answer(&form, second_twice);
    ladder_form->runs = 0u;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        const RunQuestion *const first_question = &s_pool[at + (2u * run)];
        const RunQuestion *const second_question = &s_pool[at + (2u * run) + 1u];
        const int first_answered = (first_question->outcome == RUN_ANSWERED);
        const int second_answered = (second_question->outcome == RUN_ANSWERED);
        const unsigned long long first_answer = first_question->answered[stage->read_at];
        const unsigned long long second_answer = second_question->answered[stage->read_at];
        if (first_answered && second_answered && (first_answer != second_answer) && (ladder_form->runs < KHW_RUNS))
        {
            unsigned int place = KHW_PLACE_NONE;
            // the run that leaves the answer at the word the kernel's own register still contains wrote nothing: that
            // is where this form writes its result, the answer's place, past the words
            if ((first_answer == (unsigned long long)stage->word[0][1]) ||
                (second_answer == (unsigned long long)stage->word[0][1]))
            {
                place = s_ladder_words[ladder_form->anchor];
            }
            // a run that answers the case at the first register and the second word twice at the second reads the
            // first word the host put, and the other way round; the host put the relation's words swapped where the
            // form reads them so
            else if ((first_answer == answer) && (second_answer == second_twice_answer))
            {
                place = (ladder_form->swapped != 0) ? 1u : 0u;
            }
            else if ((second_answer == answer) && (first_answer == first_twice_answer))
            {
                place = (ladder_form->swapped != 0) ? 0u : 1u;
            }
            ladder_form->first[ladder_form->runs] = ladder_form->first[run];
            ladder_form->last[ladder_form->runs] = ladder_form->last[run];
            ladder_form->place[ladder_form->runs] = place;
            ladder_form->type[ladder_form->runs] = KHW_KIND_REGISTER;
            ladder_form->runs += 1u;
        }
    }
    ladder_form->phase = KHW_PHASE_CLASSIFIED;
}

// The address of a frame form read off the first stage's round, two questions a field of the probe place's form
// (khw_probe_fields), the field set whole to the index's register then to the record's, each read at the stage's case.
// A field is the address where the two answer apart and one of them answers the relation: a load reads the case's word
// through the index's register and the record's own through the record's, and a store writes the record through the
// record's register and the case through the index's. A load's is a place of its tuple, and a store's an operand the
// form contains with no place, since the record is where a store writes. The runs the turns found inside such a field
// give way to one run, the field, marked an address; every other run is left for the second stage. The turns alone
// cannot find an address field, since each bit of it turned reads through a register that contains no address
void khw_frame_addresses_read(KhwAnswered *form, unsigned int at, const KhwStaged *stage)
{
    const unsigned long long expected = stage->expected;
    unsigned int first[KHW_RUNS];
    unsigned int bits[KHW_RUNS];
    const unsigned int fields = khw_probe_fields(first, bits);
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
        // the field takes the place of the runs inside it, in the order of first bits the runs retain
        const unsigned int last = first[field] + bits[field] - 1u;
        KhwAnswered copy = *form;
        copy.runs = 0u;
        int placed = 0;
        for (unsigned int run = 0u; run < form->runs; run += 1u)
        {
            if (!placed && (form->first[run] > first[field]) && (copy.runs < KHW_RUNS))
            {
                copy.first[copy.runs] = first[field];
                copy.last[copy.runs] = last;
                copy.place[copy.runs] = frame_place_address(form->frame);
                copy.type[copy.runs] = KHW_KIND_ADDRESS;
                copy.runs += 1u;
                placed = 1;
            }
            if (((form->first[run] >= first[field]) && (form->last[run] <= last)) || (copy.runs == KHW_RUNS))
            {
                continue;
            }
            copy.first[copy.runs] = form->first[run];
            copy.last[copy.runs] = form->last[run];
            copy.place[copy.runs] = form->place[run];
            copy.type[copy.runs] = form->type[run];
            copy.runs += 1u;
        }
        if (!placed && (copy.runs < KHW_RUNS))
        {
            copy.first[copy.runs] = first[field];
            copy.last[copy.runs] = last;
            copy.place[copy.runs] = frame_place_address(form->frame);
            copy.type[copy.runs] = KHW_KIND_ADDRESS;
            copy.runs += 1u;
        }
        *form = copy;
    }
}

// 1 where a run of `form` is at every place of its relation's tuple, exit's guard apart, since an exit with no
// guard contains no run; a frame form containing no run at a place answered no relation it contains a field of
static int khw_frame_whole(const KhwAnswered *form)
{
    const unsigned int places[4] = {frame_place_destination(form->frame), frame_place_address(form->frame),
                                    frame_place_number(form->frame), frame_place_register(form->frame)};
    for (unsigned int each = 0u; each < 4u; each += 1u)
    {
        int found = (places[each] == FRAME_PLACE_NONE);
        for (unsigned int run = 0u; (run < form->runs) && !found; run += 1u)
        {
            found = (form->place[run] == places[each]);
        }
        if (!found)
        {
            return 0;
        }
    }
    return 1;
}

// The register-runs of a frame form read off the second stage's round, its settings a run (khw_run_settings) in
// their order, each read at the stage's case; an address run is retained as the first stage marked it. The answer where
// the probe place writes nothing is the case's second word, `nothing`, and the relation's answer is `expected`. A run
// inside the immediate field is a number where the relation is number and the answers are the settings themselves, and
// the offset where the relation contains a number place and the two settings answer apart, a parameter's the constant
// bank's operand's offset. Any other run is the destination where 0 leaves `nothing` and the register the kernel stores
// answers the relation, or the guard where the relation is exit or the run lies inside the field the .ksc names the
// guard, since P0 is false at the probe place and PT is every lane, exit's guard a place of its tuple and any other
// relation's a predicate with none; the register a store stores where the relation is store and that register answers
// the relation; and an operand with no place where the two answer apart, its bits retained as seen. Each retained with
// its type, and the phase classified, or void where a place of the tuple contains no run
void khw_frame_registers_read(KhwAnswered *form, unsigned int at, const KhwStaged *stage)
{
    const unsigned int runs = form->runs;
    const unsigned long long nothing = (unsigned long long)stage->word[0][1];
    const unsigned long long expected = stage->expected;
    unsigned int first_at = at;
    KhwAnswered copy = *form;
    copy.runs = 0u;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        unsigned int place = form->place[run];
        unsigned int type = form->type[run];
        unsigned int settings[KHW_SETTINGS];
        const unsigned int probes = khw_run_settings(form, 1u, run, settings);
        if (probes != 0u)
        {
            int answered[KHW_SETTINGS];
            unsigned long long answer[KHW_SETTINGS];
            for (unsigned int setting = 0u; setting < probes; setting += 1u)
            {
                const RunQuestion *const question = &s_pool[first_at + setting];
                answered[setting] = (question->outcome == RUN_ANSWERED);
                answer[setting] = question->answered[stage->read_at];
            }
            first_at += probes;
            const int both = answered[0] && answered[1];
            const int immediate = (settings[1] == KHW_OFFSET_WORD);
            place = KHW_PLACE_NONE;
            type = KHW_KIND_REGISTER;
            if (immediate && (form->frame == FRAME_NUMBER) && both && (answer[0] == 0ull) &&
                (answer[1] == KHW_OFFSET_WORD))
            {
                place = frame_place_number(form->frame);
                type = KHW_KIND_IMMEDIATE;
            }
            else if (immediate && (form->frame != FRAME_NUMBER) &&
                     (frame_place_number(form->frame) != FRAME_PLACE_NONE) && both && (answer[0] != answer[1]))
            {
                // a parameter's place is the offset of the constant bank's operand it reads, the operand retained whole
                place = frame_place_number(form->frame);
                type = (form->frame == FRAME_PARAMETER) ? KHW_KIND_CONSTANT : KHW_KIND_IMMEDIATE;
            }
            else if (immediate && (form->frame == FRAME_DIM) && both && (answer[0] != answer[1]))
            {
                // the constant bank's operand: its offset read from the bank answers the block size at 0 and the
                // bank's next word past it, a run the form contains and no parameter takes
                type = KHW_KIND_CONSTANT;
            }
            else if (!immediate && both && (answer[0] == nothing) && (answer[1] == expected))
            {
                // a run inside the field the .ksc names the guard answers so too, P0 false at the probe place and PT
                // every lane: it is the guard, exit's place where the relation is exit and a predicate with no place
                // elsewhere, never the destination
                unsigned int guard_first = 0u;
                unsigned int guard_bits = 0u;
                const int in_guard = khw_field_read("guard", &guard_first, &guard_bits) &&
                                     (form->first[run] >= guard_first) &&
                                     (form->last[run] < (guard_first + guard_bits));
                place = (form->frame == FRAME_EXIT) ? frame_place_guard(form->frame)
                        : in_guard                  ? KHW_PLACE_NONE
                                                    : frame_place_destination(form->frame);
                type = ((form->frame == FRAME_EXIT) || in_guard) ? KHW_KIND_PREDICATE : KHW_KIND_REGISTER;
            }
            else if (!immediate && (form->frame == FRAME_STORE) && both && (answer[1] == expected) &&
                     (answer[0] != answer[1]))
            {
                place = frame_place_register(form->frame);
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
            type = ((place == KHW_PLACE_NONE) && khw_field_read("guard", &guard_first, &guard_bits) &&
                    (form->first[run] <= (guard_first + guard_bits - 1u)) && (form->last[run] >= guard_first))
                       ? KHW_KIND_PREDICATE
                       : type;
        }
        if (copy.runs < KHW_RUNS)
        {
            copy.first[copy.runs] = form->first[run];
            copy.last[copy.runs] = form->last[run];
            copy.place[copy.runs] = place;
            copy.type[copy.runs] = type;
            copy.runs += 1u;
        }
    }
    *form = copy;
    form->phase = khw_frame_whole(form) ? KHW_PHASE_CLASSIFIED : KHW_PHASE_VOID;
}

// the register-runs of `form` read off the round `stage_number` carried, as its relation's type reads them: a ladder
// form's in the first stage, a frame form's addresses in the first and the rest in the second
