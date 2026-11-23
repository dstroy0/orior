// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// asm_printer_layout.cu: the program and rulesets arranged, and the host's lists
#include "asm_printer_internal.h"
#include "memoria_operor/memoria_operor.h"

// the assembly printer's program, as the build stored it, encoded by keymath and arranged by key_schedule into
// `program`'s key and layout: 1 where they are, else 0 and why on `error_message`
static int asm_printer_program_layout(AsmPrinterProgram *program, VerbaLinea *error_message)
{
    EngineError error{};
    // the steps are a few hundred
    const KeymathRecordRequest encode_request = {program->steps,
                                                 program->step_count,
                                                 program->field_bits,
                                                 ASM_PRINTER_FIELDS,
                                                 1u,
                                                 &program->output,
                                                 1u,
                                                 program->tables,
                                                 ASM_PRINTER_TABLES,
                                                 &program->key,
                                                 &error};
    if (keymath_record_encode(&encode_request) == KEYMATH_ERROR)
    {
        mmgr_line_put(error_message, "keymath errored on the assembly printer");
        keymath_record_release(&program->key);
        return 0;
    }
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {ASM_PRINTER_RECORD_LIMBS, 0u, 0u};
    const KeyScheduleRecordRequest layout_request = {
        &program->key, program->field_offset, ASM_PRINTER_FIELDS, in_limbs, 1, &program->layout, &error};
    if (key_schedule_record_layout(&layout_request) == KEY_SCHEDULE_ERROR)
    {
        mmgr_line_put(error_message, "key_schedule errored on the assembly printer");
        key_schedule_record_release(&program->layout);
        keymath_record_release(&program->key);
        return 0;
    }
    return 1;
}

// the schema checked and the scratch taken, which are the reader's, then the build: every word and part a lane the core
// decides can be written in, the lists, the program and its tables, arranged by the core (asm_printer_core.h) the
// device runs as well, its steps given twice the memory each time they run past it; then the program encoded and
// positioned out
int asm_printer_ruleset_build(const Ruleset *rules, const TargetInfo *target, const char *header,
                              AsmPrinterRuleset *text_rules, VerbaLinea *error)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = text_rules, .bytes = sizeof(*text_rules), .val = 0);
    if ((rules->schema->form_count != OPCODE_COUNT) || (rules->schema->bank_count != REGCLASS_COUNT) ||
        (rules->schema->fixed_count != PHYSREG_COUNT))
    {
        mmgr_line_put(error, "the ruleset was read against another code generator's schema than the lane's");
        return 0;
    }
    const unsigned int scratch_banks[3] = {REGCLASS_TEMPORARY, REGCLASS_WIDE, REGCLASS_PREDICATE};
    text_rules->scratch.words = ruleset_scratch(rules, scratch_banks);
    text_rules->scratch.count = (text_rules->scratch.words != NULL) ? (4ULL * OPCODE_COUNT) : 0ULL;
    AsmPrinterFlat flat;
    if ((text_rules->scratch.words == NULL) || (asm_printer_flatten(rules, target, header, &flat) == 0))
    {
        mmgr_line_put(error, "the pool cannot meet the assembly printer");
        return 0;
    }
    AsmPrinterMemory memory;
    memory.cell = NULL;
    AsmPrinterCoreBuild build{};
    build.rules = flat.rules;
    unsigned int step_capacity = ASM_PRINTER_STEP_CAPACITY;
    for (;;)
    {
        asm_printer_memory_ended(&memory);
        asm_printer_capacities(&flat, step_capacity, &build);
        if (asm_printer_memory_size(&memory, &build) == 0)
        {
            mmgr_line_put(error, "the pool cannot meet the assembly printer's build");
            asm_printer_flat_ended(&flat);
            return 0;
        }
        if (asm_printer_core_ruleset_build(&build) != 0)
        {
            break;
        }
        if ((build.end != ASM_PRINTER_CORE_FULL) || (step_capacity >= (1u << 30u)))
        {
            mmgr_line_put(error, asm_printer_ended(build.end));
            asm_printer_memory_ended(&memory);
            asm_printer_flat_ended(&flat);
            return 0;
        }
        step_capacity *= 2u;
    }
    const int stored = asm_printer_store(&build, text_rules);
    asm_printer_memory_ended(&memory);
    asm_printer_flat_ended(&flat);
    if (stored == 0)
    {
        mmgr_line_put(error, "the pool cannot meet the assembly printer's lists");
        return 0;
    }
    return asm_printer_program_layout(&text_rules->program, error);
}

void asm_printer_ruleset_release(AsmPrinterRuleset *text_rules)
{
    key_schedule_record_release(&text_rules->program.layout);
    keymath_record_release(&text_rules->program.key);
    asm_printer_words_ended(&text_rules->scratch);
    asm_printer_words_ended(&text_rules->word_lengths);
    asm_printer_words_ended(&text_rules->part_lanes);
    asm_printer_words_ended(&text_rules->form_part_first);
    asm_printer_words_ended(&text_rules->form_parts);
    asm_printer_words_ended(&text_rules->form_placeholder_first);
    asm_printer_words_ended(&text_rules->placeholder_parameters);
    for (unsigned int table = 0u; table < ASM_PRINTER_TABLES; table += 1u)
    {
        asm_printer_words_ended(&text_rules->program.values[table]);
        text_rules->program.tables[table].values = NULL;
    }
    platform_cell_released(text_rules->program.steps, platform_place_g_program_owner);
    text_rules->program.steps = NULL;
    text_rules->program.step_count = 0u;
}

AsmPrinterLists asm_printer_lists(const AsmPrinterRuleset *text_rules)
{
    AsmPrinterLists forms{};
    forms.word_lengths = text_rules->word_lengths.words;
    forms.part_lanes = text_rules->part_lanes.words;
    forms.form_part_first = text_rules->form_part_first.words;
    forms.form_parts = text_rules->form_parts.words;
    forms.form_placeholder_first = text_rules->form_placeholder_first.words;
    forms.placeholder_parameters = text_rules->placeholder_parameters.words;
    for (unsigned int bank = 0u; bank < REGCLASS_COUNT; bank += 1u)
    {
        forms.bank_before[bank] = text_rules->bank_before[bank];
        forms.bank_after[bank] = text_rules->bank_after[bank];
    }
    for (unsigned int fixed = 0u; fixed < PHYSREG_COUNT; fixed += 1u)
    {
        forms.fixed_word[fixed] = text_rules->fixed_word[fixed];
    }
    forms.minus_word = text_rules->minus_word;
    forms.hash_word = text_rules->hash_word;
    forms.header_part = text_rules->header_part;
    for (unsigned int number = 0u; number < 4u; number += 1u)
    {
        forms.target_numbers[number] = text_rules->target_numbers[number];
    }
    return forms;
}

// the cells of one run of the host oracle, each given back as the run ends
struct AsmPrinterHostRun
{
    unsigned long long *record_first;
    unsigned int *records;
    unsigned long long *record_lanes;
    unsigned int *index;
    unsigned int *out;
};

static void asm_printer_host_ended(AsmPrinterHostRun *run)
{
    platform_cell_released(run->out, platform_place_g_program_owner);
    platform_cell_released(run->index, platform_place_g_program_owner);
    platform_cell_released(run->record_lanes, platform_place_g_program_owner);
    platform_cell_released(run->records, platform_place_g_program_owner);
    platform_cell_released(run->record_first, platform_place_g_program_owner);
}

// a cell of `count` elements of `size` bytes, every byte zero, on the temporary tier, and never empty
static void *asm_printer_host_cell(unsigned long long count, unsigned long long size)
{
    return platform_cell_cleared((count != 0ULL) ? count : 1ULL, size, platform_place_g_program_owner, 1);
}

// the host oracle, in the device's order: each item's records counted and arranged, each record's first lane and the
// index arranged from the running sum of the records' lanes, the program run, and the bytes that are not 0 taken in
// lane order
int asm_printer_host(const AsmPrinterRuleset *text_rules, const MachineInstr *items, unsigned long long count,
                     PlatformText *text, VerbaLinea *error_message)
{
    const AsmPrinterLists forms = asm_printer_lists(text_rules);
    AsmPrinterHostRun run;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &run, .bytes = sizeof(run), .val = 0);
    run.record_first = (unsigned long long *)asm_printer_host_cell(count, sizeof(unsigned long long));
    if (run.record_first == NULL)
    {
        mmgr_line_put(error_message, "the pool cannot meet the host oracle");
        return 0;
    }
    unsigned long long record_count = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        if (!asm_printer_formed(&items[at]))
        {
            mmgr_line_put(error_message, "a form breaks the lane");
            asm_printer_host_ended(&run);
            return 0;
        }
        run.record_first[at] = record_count;
        record_count += asm_printer_record_count(&forms, &items[at]);
    }
    if (record_count >= ASM_PRINTER_LANES_MAX)
    {
        mmgr_line_put(error_message, "the text contains more records than the assembly printer supports");
        asm_printer_host_ended(&run);
        return 0;
    }
    run.records = (unsigned int *)asm_printer_host_cell(record_count * ASM_PRINTER_RECORD_LIMBS, sizeof(unsigned int));
    run.record_lanes = (unsigned long long *)asm_printer_host_cell(record_count, sizeof(unsigned long long));
    if ((run.records == NULL) || (run.record_lanes == NULL))
    {
        mmgr_line_put(error_message, "the pool cannot meet the host oracle");
        asm_printer_host_ended(&run);
        return 0;
    }
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        asm_printer_records(&forms, &items[at], run.record_first[at], run.records, run.record_lanes);
    }
    unsigned long long lanes = 0ULL;
    for (unsigned long long record = 0ULL; record < record_count; record += 1ULL)
    {
        lanes += run.record_lanes[record];
    }
    if ((lanes >= ASM_PRINTER_LANES_MAX) || (lanes == 0ULL))
    {
        mmgr_line_put(error_message, "the text contains more lanes than the assembly printer supports");
        asm_printer_host_ended(&run);
        return 0;
    }
    const unsigned int out_limbs = text_rules->program.layout.out_limbs;
    run.index = (unsigned int *)asm_printer_host_cell(lanes, sizeof(unsigned int));
    run.out = (unsigned int *)asm_printer_host_cell(lanes * out_limbs, sizeof(unsigned int));
    if ((run.index == NULL) || (run.out == NULL))
    {
        mmgr_line_put(error_message, "the pool cannot meet the host oracle");
        asm_printer_host_ended(&run);
        return 0;
    }
    unsigned long long first = 0ULL;
    for (unsigned long long record = 0ULL; record < record_count; record += 1ULL)
    {
        asm_printer_first(run.records, record, first);
        for (unsigned long long lane = 0ULL; lane < run.record_lanes[record]; lane += 1ULL)
        {
            // a record's number is below 2^31
            run.index[first + lane] = (unsigned int)record;
        }
        first += run.record_lanes[record];
    }
    EngineError error{};
    CycleRecordHostRequest request{};
    request.layout = &text_rules->program.layout;
    request.in[0] = run.records;
    request.bodies[0] = record_count;
    request.index = run.index;
    request.count = lanes;
    request.out = run.out;
    request.error = &error;
    if (cycle_record_run_host(&request) == CYCLE_ERROR)
    {
        mmgr_line_put(error_message, "the host oracle errored on the assembly printer's run");
        asm_printer_host_ended(&run);
        return 0;
    }
    // the one output, the byte, lies at its step's offset in each lane's record
    const unsigned int offset = text_rules->program.layout.step_table[text_rules->program.key.output[0]].out_offset;
    platform_text_reset(text);
    for (unsigned long long lane = 0ULL; lane < lanes; lane += 1ULL)
    {
        const unsigned int byte = asm_printer_byte(&run.out[lane * out_limbs], out_limbs, offset);
        if (byte != 0u)
        {
            // the byte is one of the text's, below 256
            mmgr_line_ch(&text->line, (char)byte);
        }
    }
    asm_printer_host_ended(&run);
    return 1;
}
