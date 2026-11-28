// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// relation_fields.c: the relation: the fields queried
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "khw_learn_internal.h"
#include "memoria_operor/memoria_operor.h"

// The runs of a predicate form, read from its own encoding, with no question queried:
//
// - a run inside a field that the .ksc names for a predicate that holds P0 is the destination, the place of the
//   answer, a predicate. One that holds another predicate is an operand with no place.
// - a run inside a register field that holds the register of the first word is the place of the first word. One that
//   holds the register of the second word is the place of the second. It is the other way round where the form reads
//   the words swapped.
// - every other run is an operand with no place, kept as seen.
//
// The phase reads as classified where a run is at every place of the tuple of the relation, and as void otherwise
static void khw_predicate_runs_read(KhwAnswered *form)
{
    static const char *const s_predicate_fields[] = {"guard", "pl", "pu", "pv", "pg", "po", "pt"};
    const unsigned int words = s_ladder_words[form->anchor];
    for (unsigned int run = 0u; run < form->runs; run += 1u)
    {
        char field[64] = "";
        unsigned int first = 0u;
        unsigned int bits = 0u;
        form->place[run] = KHW_PLACE_NONE;
        form->type[run] = KHW_TYPE_REGISTER;
        if (!khw_field_covering(form->first[run], form->last[run], field, sizeof(field), &first, &bits))
        {
            continue;
        }
        const unsigned long long value = khw_run_read(form->low, form->high, first, first + bits - 1u);
        int predicate = 0;
        for (unsigned int each = 0u; each < (sizeof(s_predicate_fields) / sizeof(s_predicate_fields[0])); each += 1u)
        {
            predicate =
                predicate || (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = field, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                         .other = s_predicate_fields[each], .ci = EMBED_FALSE));
        }
        if (predicate)
        {
            form->type[run] = KHW_TYPE_PREDICATE;
            form->place[run] = ((value == 0ULL) &&
                                (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = field,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "guard", .ci = EMBED_FALSE)))
                                   ? words
                                   : KHW_PLACE_NONE;
        }
        else if (value == KHW_FIRST_REGISTER)
        {
            form->place[run] = (form->swapped != 0) ? 1u : 0u;
        }
        else if (value == KHW_SECOND_REGISTER)
        {
            form->place[run] = (form->swapped != 0) ? 0u : 1u;
        }
    }
    form->phase = khw_places_whole(form->place, form->runs, words) ? KHW_PHASE_CLASSIFIED : KHW_PHASE_VOID;
}

static void khw_registers_read(KhwAnswered *form, unsigned int at, const KhwStaged *stage, unsigned int stage_number)
{
    if (form->frame != (unsigned int)FRAME_RELATION_COUNT)
    {
        (stage_number == 0u) ? khw_frame_addresses_read(form, at, stage) : khw_frame_registers_read(form, at, stage);
        return;
    }
    if (ladder_predicate(form->anchor))
    {
        khw_predicate_runs_read(form);
        return;
    }
    khw_ladder_registers_read(form, at, stage);
}

// the forms of `s_staged` from `first` to `staged`, read from the round carried, each as `stage_number` reads it. The
// return is the count whose phase then reads as classified
static unsigned int khw_staged_read(KhwStaged *staged_forms, unsigned int first, unsigned int staged,
                                    unsigned int stage_number)
{
    unsigned int classified = 0u;
    for (unsigned int read = first; read < staged; read += 1u)
    {
        khw_registers_read(staged_forms[read].form, staged_forms[read].at, &staged_forms[read], stage_number);
        classified += (staged_forms[read].form->phase == KHW_PHASE_CLASSIFIED) ? 1u : 0u;
    }
    return classified;
}

// the questions that a stage queries of `form`, pooled into `staged` where it is not NULL. For the first stage of a
// frame form, they are two for each field of the form at the probe place. The field goes whole to the register of the
// index, and to that of the record. Otherwise they are the settings of each run for the stage, with the run set to
// each. The return is the count
static unsigned int khw_stage_pooled(const KhwAnswered *form, unsigned int stage, const KhwStaged *staged)
{
    unsigned int pooled = 0u;
    if ((form->frame != (unsigned int)FRAME_RELATION_COUNT) && (stage == 0u))
    {
        unsigned int first[KHW_RUNS];
        unsigned int bits[KHW_RUNS];
        const unsigned int fields = khw_probe_fields(first, bits);
        const unsigned int settings[2] = {khw_fixed_register("index"), khw_fixed_register("record")};
        for (unsigned int field = 0u; field < fields; field += 1u)
        {
            for (unsigned int setting = 0u; setting < 2u; setting += 1u)
            {
                unsigned long long low = form->low;
                unsigned long long high = form->high;
                khw_run_write(&low, &high, first[field], first[field] + bits[field] - 1u, settings[setting]);
                if (staged != NULL)
                {
                    khw_staged_pooled(staged, low, high);
                }
                pooled += 1u;
            }
        }
        return pooled;
    }
    for (unsigned int run = 0u; run < form->runs; run += 1u)
    {
        unsigned int settings[KHW_SETTINGS];
        const unsigned int probes = khw_run_settings(form, stage, run, settings);
        for (unsigned int setting = 0u; setting < probes; setting += 1u)
        {
            unsigned long long low = form->low;
            unsigned long long high = form->high;
            khw_run_write(&low, &high, form->first[run], form->last[run], settings[setting]);
            if (staged != NULL)
            {
                khw_staged_pooled(staged, low, high);
            }
            pooled += 1u;
        }
    }
    return pooled;
}

// The register runs of every turned form of `forms`, `count` of them: the second field pass, in shared rounds over
// KHW_STAGES stages. In each stage, the call sets the turns found of each form to each setting that the stage gives the
// run (khw_run_settings). That is over the same case, one question for each setting of each run, with as many forms in
// a round as the pool holds.
//
// The call reads what came back into the operand runs of each form. It reads the addresses of the frame forms after
// the first stage, and the phase of every form, classified or void, after its last. The return is the count of forms
// classified
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
            EMBED_CALL(memor.set, MemoriaCfg, .dst = stage, .bytes = sizeof(*stage), .val = 0);
            const unsigned int pooled = khw_stage_pooled(forms[at], stage_number, NULL);
            // a predicate form queries nothing. The call reads its runs from its own encoding in the first stage
            if ((pooled == 0u) && (stage_number == 0u) && (forms[at]->frame == (unsigned int)FRAME_RELATION_COUNT) &&
                ladder_predicate(forms[at]->anchor))
            {
                khw_predicate_runs_read(forms[at]);
                classified += (forms[at]->phase == KHW_PHASE_CLASSIFIED) ? 1u : 0u;
                continue;
            }
            if ((pooled == 0u) || !khw_turning_case(forms[at], stage))
            {
                continue;
            }
            if ((s_pooled + pooled) > SCHEDULER_ROUND_MAX)
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

// A form's fields queried whole, the turns then the register-runs, for the learn loop that queries both of a form in
// one run. The split passes query them apart over every form at once, a cross-check reading the register-runs the turns
// find between
void khw_fields_queried(KhwAnswered *form)
{
    KhwAnswered *const one[1] = {form};
    if (khw_fields_turns(one, 1u) != 0u)
    {
        khw_fields_registers(one, 1u);
    }
}
