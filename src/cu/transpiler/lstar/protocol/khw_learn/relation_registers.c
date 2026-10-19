// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// relation_registers.c: the relation: the fields asked
#include "khw_learn_internal.h"

// KHW_SETTINGS: the count, 0 where the stage asks the run nothing. A ladder form's runs take the two registers the
// kernel leaves the case's words in, in the first stage alone. A frame form's runs take first the index's and the
// record's registers the part's .ksc names, two that hold addresses, so that no run is read as an address through a
// register holding none; then, the address runs apart, a run inside the field the .ksc names `immediate` takes 0 and
// KHW_OFFSET_WORD, the aligned offset of the record's second word, which a load and a store answer apart and a number
// answers as itself, and every other run 0 and the register the kernel stores
#define KHW_OFFSET_WORD 4u

unsigned int khw_run_settings(const KhwAnswered *held, unsigned int stage, unsigned int run,
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
unsigned int khw_slot_fields(unsigned int *first, unsigned int *bits)
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
void khw_ladder_registers_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage)
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
void khw_frame_addresses_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage)
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
// offset where the relation holds a number place and the two settings answer apart, a parameter's the constant bank's
// operand's offset. Any other run is the destination
// where 0 leaves `nothing` and the register the kernel stores answers the relation, or the guard where the relation is
// exit or the run lies inside the field the .ksc names the guard, since P0 is false at the slot and PT is every lane,
// exit's guard a place of its tuple and any other relation's a predicate with none; the register a store keeps where
// the relation is store and that register answers the relation; and an operand with no place where the two answer
// apart, its bits kept as seen. Each kept with its kind, and the phase classified, or void where a place of the tuple
// holds no run
void khw_frame_registers_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage)
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
                // a parameter's place is the offset of the constant bank's operand it reads, the operand kept whole
                place = frame_place_number(held->frame);
                kind = (held->frame == FRAME_PARAMETER) ? KHW_KIND_CONSTANT : KHW_KIND_IMMEDIATE;
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
