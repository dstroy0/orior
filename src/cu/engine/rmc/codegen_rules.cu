// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// codegen_rules.cu: the assembly printer's ruleset built on the device, and checked against the host's
#include "asm_printer_internal.h"
#include "codegen_device.h"
#include "codegen_device_internal.h"
#include "memoria_operor/memoria_operor.h"

int asm_printer_ruleset_same(const AsmPrinterRuleset *left, const AsmPrinterRuleset *right)
{
    const AsmPrinterProgram &left_program = left->program;
    const AsmPrinterProgram &right_program = right->program;
    int same = asm_printer_words_same(&left->scratch, &right->scratch) &&
               asm_printer_words_same(&left->word_lengths, &right->word_lengths) &&
               asm_printer_words_same(&left->part_lanes, &right->part_lanes) &&
               asm_printer_words_same(&left->form_part_first, &right->form_part_first) &&
               asm_printer_words_same(&left->form_parts, &right->form_parts) &&
               asm_printer_words_same(&left->form_placeholder_first, &right->form_placeholder_first) &&
               asm_printer_words_same(&left->placeholder_parameters, &right->placeholder_parameters) &&
               (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->bank_before, .other = right->bank_before,
                           .bytes = sizeof(left->bank_before)) == 0) &&
               (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->bank_after, .other = right->bank_after,
                           .bytes = sizeof(left->bank_after)) == 0) &&
               (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->fixed_word, .other = right->fixed_word,
                           .bytes = sizeof(left->fixed_word)) == 0) &&
               (left->minus_word == right->minus_word) && (left->hash_word == right->hash_word) &&
               (left->header_part == right->header_part) &&
               (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->target_numbers, .other = right->target_numbers,
                           .bytes = sizeof(left->target_numbers)) == 0) &&
               (left_program.step_count == right_program.step_count) &&
               (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left_program.field_bits, .other = right_program.field_bits,
                           .bytes = sizeof(left_program.field_bits)) == 0) &&
               (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left_program.field_offset, .other = right_program.field_offset,
                           .bytes = sizeof(left_program.field_offset)) == 0) &&
               (left_program.output == right_program.output);
    for (unsigned int table = 0u; (same != 0) && (table < ASM_PRINTER_TABLES); table += 1u)
    {
        same = asm_printer_words_same(&left_program.values[table], &right_program.values[table]);
    }
    for (unsigned int step = 0u; (same != 0) && (step < left_program.step_count); step += 1u)
    {
        const EngineRecordStep &one = left_program.steps[step];
        const EngineRecordStep &other = right_program.steps[step];
        same = (one.operation == other.operation) && (one.left == other.left) && (one.right == other.right) &&
               (one.member == other.member);
    }
    for (unsigned int table = 0u; (same != 0) && (table < ASM_PRINTER_TABLES); table += 1u)
    {
        same = (left_program.tables[table].index_bits == right_program.tables[table].index_bits) &&
               (left_program.tables[table].out_bits == right_program.tables[table].out_bits);
    }
    return (same != 0) && (layout_same(&left_program.layout, &right_program.layout) != 0);
}

#if (defined(__CUDACC__))

// the build in one thread (asm_printer_core_ruleset_build), because the number of each word depends on every word
// before it. The build stays in device memory as it ended, with pointers of the device. Its arrays for each form are
// far larger than the parameters of a kernel, thus the call does not pass it by value
__global__ void codegen_rules_build(AsmPrinterCoreBuild *build)
{
    if (codegen_device_thread() != 0ULL)
    {
        return;
    }
    asm_printer_core_ruleset_build(build);
}

// `build`'s memory taken on the device, sized to its capacities, and `build` pointed at it
static void codegen_rules_memory(DeviceArena *memory, AsmPrinterCoreBuild *build)
{
    build->word_start = codegen_device_take<unsigned int>(memory, (unsigned long long)build->word_capacity + 1ULL);
    build->word_length = codegen_device_take<unsigned int>(memory, (unsigned long long)build->word_capacity + 1ULL);
    build->word_characters =
        codegen_device_take<unsigned char>(memory, (unsigned long long)build->character_capacity + 1ULL);
    build->part_pieces = codegen_device_take<unsigned int>(
        memory, ((unsigned long long)build->part_capacity * ASM_PRINTER_PIECES) + 1ULL);
    build->part_lanes = codegen_device_take<unsigned int>(memory, (unsigned long long)build->part_capacity + 1ULL);
    build->form_parts = codegen_device_take<unsigned int>(memory, (unsigned long long)build->form_part_capacity + 1ULL);
    build->placeholder_parameters =
        codegen_device_take<unsigned int>(memory, (unsigned long long)build->placeholder_capacity + 1ULL);
    build->steps = codegen_device_take<EngineRecordStep>(memory, build->step_capacity);
    for (unsigned int table = 0u; table < ASM_PRINTER_TABLES; table += 1u)
    {
        build->values[table] = codegen_device_take<unsigned int>(memory, build->table_capacity[table]);
    }
}

// what the device's build arranged, read back into `memory`, and `host` pointed at it with the build's counts
static void codegen_rules_read(DeviceArena *device_arena, const AsmPrinterCoreBuild *ended, AsmPrinterMemory *memory,
                               AsmPrinterCoreBuild *host)
{
    *host = *ended;
    device_arena->ok = (device_arena->ok != 0) && asm_printer_memory_size(memory, host);
    codegen_device_read(device_arena, host->word_start, ended->word_start, ended->words);
    codegen_device_read(device_arena, host->word_length, ended->word_length, ended->words);
    codegen_device_read(device_arena, host->word_characters, ended->word_characters, ended->characters);
    codegen_device_read(device_arena, host->part_pieces, ended->part_pieces,
                        (unsigned long long)ended->parts * ASM_PRINTER_PIECES);
    codegen_device_read(device_arena, host->part_lanes, ended->part_lanes, ended->parts);
    codegen_device_read(device_arena, host->form_parts, ended->form_parts, ended->form_part_count);
    codegen_device_read(device_arena, host->placeholder_parameters, ended->placeholder_parameters,
                        ended->placeholder_count);
    codegen_device_read(device_arena, host->steps, ended->steps, ended->step_count);
    for (unsigned int table = 0u; table < ASM_PRINTER_TABLES; table += 1u)
    {
        const unsigned long long rows = 1ULL << ended->tables[table].index_bits;
        codegen_device_read(device_arena, host->values[table], ended->values[table], rows);
        host->tables[table].values = host->values[table];
    }
}

int asm_printer_ruleset_device(const Ruleset *rules, const TargetInfo *target, const char *header,
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
    if (ruleset_scratch_device(rules, scratch_banks, &text_rules->scratch, error) == 0)
    {
        return 0;
    }
    AsmPrinterFlat flat;
    if (asm_printer_flatten(rules, target, header, &flat) == 0)
    {
        mmgr_line_put(error, "the pool cannot meet the assembly printer");
        return 0;
    }
    DeviceArena rules_arena{};
    rules_arena.ok = 1;
    AsmPrinterCoreBuild build{};
    build.rules = flat.rules;
    build.rules.characters = codegen_device_copy(&rules_arena, flat.rules.characters, flat.character_count);
    build.rules.texts = codegen_device_copy(&rules_arena, flat.rules.texts, flat.text_count);
    build.rules.placeholders = codegen_device_copy(&rules_arena, flat.rules.placeholders, flat.placeholder_count);
    unsigned int step_capacity = ASM_PRINTER_STEP_CAPACITY;
    int built = 0;
    AsmPrinterMemory memory;
    memory.cell = NULL;
    AsmPrinterCoreBuild host{};
    while ((rules_arena.ok != 0) && (built == 0))
    {
        DeviceArena build_arena{};
        build_arena.ok = 1;
        asm_printer_capacities(&flat, step_capacity, &build);
        codegen_rules_memory(&build_arena, &build);
        AsmPrinterCoreBuild *const device_ended = codegen_device_copy(&build_arena, &build, 1ULL);
        AsmPrinterCoreBuild ended{};
        if (build_arena.ok != 0)
        {
            codegen_rules_build<<<1u, 1u>>>(device_ended);
            codegen_device_launched(&build_arena);
        }
        codegen_device_read(&build_arena, &ended, device_ended, 1ULL);
        if ((build_arena.ok != 0) && (ended.end == ASM_PRINTER_CORE_OK))
        {
            codegen_rules_read(&build_arena, &ended, &memory, &host);
            built = (build_arena.ok != 0) ? 1 : 0;
        }
        const int ok = build_arena.ok;
        codegen_device_release(&build_arena);
        if (ok == 0)
        {
            rules_arena.ok = 0;
        }
        else if ((built == 0) && ((ended.end != ASM_PRINTER_CORE_FULL) || (step_capacity >= (1u << 30u))))
        {
            codegen_device_release(&rules_arena);
            asm_printer_flat_ended(&flat);
            mmgr_line_put(error, asm_printer_ended(ended.end));
            return 0;
        }
        step_capacity *= 2u;
    }
    const int ok = rules_arena.ok;
    codegen_device_release(&rules_arena);
    asm_printer_flat_ended(&flat);
    const int stored = (ok != 0) && asm_printer_store(&host, text_rules);
    asm_printer_memory_ended(&memory);
    if (ok == 0)
    {
        mmgr_line_put(error, "the device errored on a call");
        return 0;
    }
    if (stored == 0)
    {
        mmgr_line_put(error, "the pool cannot meet the assembly printer's lists");
        return 0;
    }
    // the program, arranged by the device as well. The call does not keep the key of the host, and the output that it
    // names is that of the program
    return asm_printer_program_device(&text_rules->program, &text_rules->program.layout, error);
}

#else

int asm_printer_ruleset_device(const Ruleset *rules, const TargetInfo *target, const char *header,
                               AsmPrinterRuleset *text_rules, VerbaLinea *error)
{
    (void)rules;
    (void)target;
    (void)header;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = text_rules, .bytes = sizeof(*text_rules), .val = 0);
    mmgr_line_put(error, "the build has no device");
    return 0;
}

#endif
