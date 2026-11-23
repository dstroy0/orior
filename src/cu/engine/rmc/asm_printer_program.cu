// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// asm_printer_program.cu: the ruleset flattened for the core's build, its memory, and what the build arranged stored
#include "asm_printer_internal.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// the lists of a flat ruleset as the fill writes them: the characters and texts so far
struct AsmPrinterFlatFill
{
    unsigned char *characters;
    AsmPrinterCoreText *texts;
    unsigned int character_count;
    unsigned int text_count;
};

// `text` taken on as a text of the flattened ruleset, its number returned. A fill with no lists counts it alone
static unsigned int asm_printer_flat_text(AsmPrinterFlatFill *fill, const char *text)
{
    // the ruleset's texts and characters are far fewer than 2^32; the build errors on more than its tables contain
    const unsigned int length =
        (unsigned int)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if (fill->texts != NULL)
    {
        const AsmPrinterCoreText taken = {fill->character_count, length};
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &fill->characters[fill->character_count], .src = text,
                   .bytes = length);
        fill->texts[fill->text_count] = taken;
    }
    fill->character_count += length;
    fill->text_count += 1u;
    return fill->text_count - 1u;
}

// The ruleset, the target and the header taken on into `fill`, in the host's order. `flattened` gets each form's first
// piece and placeholder, and whether the ruleset gives the form as a construct. It also gets each bank's pieces and
// count of placeholders, and each fixed register's text and the header's. The answer is the count of the forms'
// placeholders
static unsigned int asm_printer_flat_fill(const Ruleset *rules, const char *header, AsmPrinterFlatFill *fill,
                                          unsigned int *placeholders, AsmPrinterCoreRules *flattened)
{
    unsigned int placeholder_count = 0u;
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        // the forms' pieces and placeholders are a few hundred
        const InstrTemplate *const written = &rules->forms[form];
        flattened->form_piece_first[form] = fill->text_count;
        flattened->form_placeholder_first[form] = placeholder_count;
        flattened->form_construct[form] = (rules->constructs[form].line_count == 0u) ? 0u : 1u;
        for (unsigned int piece = 0u; piece <= written->placeholder_count; piece += 1u)
        {
            (void)asm_printer_flat_text(fill, written->pieces[piece]);
        }
        for (unsigned int at = 0u; at < written->placeholder_count; at += 1u)
        {
            if (placeholders != NULL)
            {
                placeholders[placeholder_count] = written->placeholders[at];
            }
            placeholder_count += 1u;
        }
    }
    flattened->form_piece_first[OPCODE_COUNT] = fill->text_count;
    flattened->form_placeholder_first[OPCODE_COUNT] = placeholder_count;
    for (unsigned int bank = 0u; bank < REGCLASS_COUNT; bank += 1u)
    {
        const InstrTemplate *const bank_form = &rules->banks[bank];
        flattened->bank_piece_first[bank] = fill->text_count;
        // a bank's placeholders are one where the read takes it, and a count far below 2^32 where it does not
        flattened->bank_placeholders[bank] = bank_form->placeholder_count;
        for (unsigned int piece = 0u; piece <= bank_form->placeholder_count; piece += 1u)
        {
            (void)asm_printer_flat_text(fill, bank_form->pieces[piece]);
        }
    }
    for (unsigned int fixed = 0u; fixed < PHYSREG_COUNT; fixed += 1u)
    {
        flattened->fixed_text[fixed] = asm_printer_flat_text(fill, rules->fixed[fixed]);
    }
    flattened->header_text = asm_printer_flat_text(fill, header);
    return placeholder_count;
}

int asm_printer_flatten(const Ruleset *rules, const TargetInfo *target, const char *header, AsmPrinterFlat *flat)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = flat, .bytes = sizeof(*flat), .val = 0);
    AsmPrinterCoreRules *const flattened = &flat->rules;
    AsmPrinterFlatFill count = {NULL, NULL, 0u, 0u};
    const unsigned int placeholders = asm_printer_flat_fill(rules, header, &count, NULL, flattened);
    // the core reads the characters at 0 for the empty word, and every list takes at least one entry
    RulesetCellPlan plan = {NULL, 0ULL};
    (void)ruleset_cell_place(&plan, (unsigned long long)count.character_count + 1ULL, 1ULL);
    (void)ruleset_cell_place(&plan, (unsigned long long)count.text_count + 1ULL, sizeof(AsmPrinterCoreText));
    (void)ruleset_cell_place(&plan, (unsigned long long)placeholders + 1ULL, sizeof(unsigned int));
    flat->cell = platform_cell_cleared(plan.used, 1ULL, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (flat->cell == NULL)
    {
        return 0;
    }
    plan.base = (unsigned char *)flat->cell;
    plan.used = 0ULL;
    unsigned char *const characters =
        (unsigned char *)ruleset_cell_place(&plan, (unsigned long long)count.character_count + 1ULL, 1ULL);
    AsmPrinterCoreText *const texts = (AsmPrinterCoreText *)ruleset_cell_place(
        &plan, (unsigned long long)count.text_count + 1ULL, sizeof(AsmPrinterCoreText));
    unsigned int *const placeholder_list =
        (unsigned int *)ruleset_cell_place(&plan, (unsigned long long)placeholders + 1ULL, sizeof(unsigned int));
    AsmPrinterFlatFill fill = {characters, texts, 0u, 0u};
    (void)asm_printer_flat_fill(rules, header, &fill, placeholder_list, flattened);
    flattened->block_hash = target->block_hash;
    // a device's compute capability and NVRTC's version are small counts, never negative
    flattened->target_numbers[0] = (unsigned int)target->major;
    flattened->target_numbers[1] = (unsigned int)target->minor;
    flattened->target_numbers[2] = (unsigned int)target->nvrtc_major;
    flattened->target_numbers[3] = (unsigned int)target->nvrtc_minor;
    flattened->characters = characters;
    flattened->texts = texts;
    flattened->placeholders = placeholder_list;
    flat->text_count = fill.text_count;
    flat->character_count = fill.character_count + 1u;
    flat->placeholder_count = placeholders + 1u;
    return 1;
}

void asm_printer_flat_ended(AsmPrinterFlat *flat)
{
    platform_cell_released(flat->cell, platform_place_g_program_owner);
    flat->cell = NULL;
}

void asm_printer_capacities(const AsmPrinterFlat *flat, unsigned int step_capacity, AsmPrinterCoreBuild *build)
{
    // Every word is a text of the ruleset, the empty word, the minus or the hash. Every character is a text's, the
    // minus's or the hash's. Each form takes a part for each four placeholders and one for none, and the header one
    // more
    unsigned int form_parts = 0u;
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        const unsigned int placeholder_count =
            flat->rules.form_placeholder_first[form + 1u] - flat->rules.form_placeholder_first[form];
        form_parts += (placeholder_count == 0u)
                          ? 1u
                          : ((placeholder_count + ASM_PRINTER_PLACEHOLDERS - 1u) / ASM_PRINTER_PLACEHOLDERS);
    }
    // the texts, characters and placeholders are a few thousand
    build->word_capacity = flat->text_count + 3u;
    build->character_capacity = flat->character_count + 1u + ASM_PRINTER_HASH_DIGITS;
    build->form_part_capacity = form_parts;
    build->part_capacity = form_parts + 1u;
    build->placeholder_capacity = flat->rules.form_placeholder_first[OPCODE_COUNT];
    build->step_capacity = step_capacity;
    // past ASM_PRINTER_TABLE_BITS_MAX the build ends before its tables: no table is wider
    for (unsigned int table = 0u; table < ASM_PRINTER_TABLE_POWER; table += 1u)
    {
        build->table_capacity[table] = 1u << ASM_PRINTER_TABLE_BITS_MAX;
    }
    build->table_capacity[ASM_PRINTER_TABLE_POWER] = ASM_PRINTER_POWER_ROWS;
    build->table_capacity[ASM_PRINTER_TABLE_BYTE] = ASM_PRINTER_BYTE_ROWS;
}

void asm_printer_memory_layout(RulesetCellPlan *plan, AsmPrinterCoreBuild *build)
{
    // every list takes at least one word
    build->word_start =
        (unsigned int *)ruleset_cell_place(plan, (unsigned long long)build->word_capacity + 1ULL, sizeof(unsigned int));
    build->word_length =
        (unsigned int *)ruleset_cell_place(plan, (unsigned long long)build->word_capacity + 1ULL, sizeof(unsigned int));
    build->word_characters =
        (unsigned char *)ruleset_cell_place(plan, (unsigned long long)build->character_capacity + 1ULL, 1ULL);
    build->part_pieces = (unsigned int *)ruleset_cell_place(
        plan, ((unsigned long long)build->part_capacity * ASM_PRINTER_PIECES) + 1ULL, sizeof(unsigned int));
    build->part_lanes =
        (unsigned int *)ruleset_cell_place(plan, (unsigned long long)build->part_capacity + 1ULL, sizeof(unsigned int));
    build->form_parts = (unsigned int *)ruleset_cell_place(plan, (unsigned long long)build->form_part_capacity + 1ULL,
                                                           sizeof(unsigned int));
    build->placeholder_parameters = (unsigned int *)ruleset_cell_place(
        plan, (unsigned long long)build->placeholder_capacity + 1ULL, sizeof(unsigned int));
    build->steps = (EngineRecordStep *)ruleset_cell_place(plan, build->step_capacity, sizeof(EngineRecordStep));
    for (unsigned int table = 0u; table < ASM_PRINTER_TABLES; table += 1u)
    {
        build->values[table] =
            (unsigned int *)ruleset_cell_place(plan, build->table_capacity[table], sizeof(unsigned int));
    }
}

int asm_printer_memory_size(AsmPrinterMemory *memory, AsmPrinterCoreBuild *build)
{
    RulesetCellPlan plan = {NULL, 0ULL};
    asm_printer_memory_layout(&plan, build);
    memory->cell = platform_cell_cleared(plan.used, 1ULL, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    plan.base = (unsigned char *)memory->cell;
    plan.used = 0ULL;
    asm_printer_memory_layout(&plan, build);
    return (memory->cell != NULL) ? 1 : 0;
}

void asm_printer_memory_ended(AsmPrinterMemory *memory)
{
    platform_cell_released(memory->cell, platform_place_g_program_owner);
    memory->cell = NULL;
}

int asm_printer_words_taken(AsmPrinterWords *words, const unsigned int *from, unsigned long long count)
{
    // a list takes at least one word, and is never empty
    words->words = (unsigned int *)platform_cell_cleared((count != 0ULL) ? count : 1ULL, sizeof(unsigned int),
                                                         platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    words->count = (words->words != NULL) ? count : 0ULL;
    if ((words->words != NULL) && (count != 0ULL))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = words->words, .src = from,
                   .bytes = (size_t)(count * sizeof(unsigned int)));
    }
    return (words->words != NULL) ? 1 : 0;
}

void asm_printer_words_ended(AsmPrinterWords *words)
{
    platform_cell_released(words->words, platform_place_g_program_owner);
    words->words = NULL;
    words->count = 0ULL;
}

int asm_printer_words_same(const AsmPrinterWords *left, const AsmPrinterWords *right)
{
    return (left->count == right->count) &&
           ((left->count == 0ULL) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->words, .other = right->words,
                                                 .bytes = (size_t)(left->count * sizeof(unsigned int))) == 0));
}

int asm_printer_store(const AsmPrinterCoreBuild *build, AsmPrinterRuleset *text_rules)
{
    int ok = asm_printer_words_taken(&text_rules->word_lengths, build->word_length, build->words) &&
             asm_printer_words_taken(&text_rules->part_lanes, build->part_lanes, build->parts) &&
             asm_printer_words_taken(&text_rules->form_part_first, build->form_part_first, OPCODE_COUNT + 1u) &&
             asm_printer_words_taken(&text_rules->form_parts, build->form_parts, build->form_part_count) &&
             asm_printer_words_taken(&text_rules->form_placeholder_first, build->form_placeholder_first,
                                     OPCODE_COUNT + 1u) &&
             asm_printer_words_taken(&text_rules->placeholder_parameters, build->placeholder_parameters,
                                     build->placeholder_count);
    for (unsigned int bank = 0u; bank < REGCLASS_COUNT; bank += 1u)
    {
        text_rules->bank_before[bank] = build->bank_before[bank];
        text_rules->bank_after[bank] = build->bank_after[bank];
    }
    for (unsigned int fixed = 0u; fixed < PHYSREG_COUNT; fixed += 1u)
    {
        text_rules->fixed_word[fixed] = build->fixed_word[fixed];
    }
    text_rules->minus_word = build->minus_word;
    text_rules->hash_word = build->hash_word;
    text_rules->header_part = build->header_part;
    for (unsigned int number = 0u; number < 4u; number += 1u)
    {
        text_rules->target_numbers[number] = build->target_numbers[number];
    }
    // what keymath and key_schedule take, stored for the device's layout. Each table points at its values where the
    // store holds them
    AsmPrinterProgram *const program = &text_rules->program;
    program->steps = (EngineRecordStep *)platform_cell_cleared((build->step_count != 0u) ? build->step_count : 1u,
                                                               sizeof(EngineRecordStep), platform_place_g_program_owner,
                                                               PLATFORM_TIER_TEMPORARY);
    program->step_count = (program->steps != NULL) ? build->step_count : 0u;
    ok = ok && (program->steps != NULL);
    if (program->steps != NULL)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = program->steps, .src = build->steps,
                   .bytes = (size_t)build->step_count * sizeof(EngineRecordStep));
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = program->field_bits, .src = build->field_bits,
               .bytes = sizeof(program->field_bits));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = program->field_offset, .src = build->field_offset,
               .bytes = sizeof(program->field_offset));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = program->tables, .src = build->tables, .bytes = sizeof(program->tables));
    for (unsigned int table = 0u; table < ASM_PRINTER_TABLES; table += 1u)
    {
        const unsigned long long rows = 1ULL << build->tables[table].index_bits;
        ok = asm_printer_words_taken(&program->values[table], build->values[table], rows) && ok;
        program->tables[table].values = program->values[table].words;
    }
    program->output = build->output;
    return ok;
}

const char *asm_printer_ended(unsigned int end)
{
    switch (end)
    {
    case ASM_PRINTER_CORE_CONSTRUCT:
        return "a form is given as a construct, whose scratch the code generator takes";
    case ASM_PRINTER_CORE_HEADER:
        return "the header contains a byte 0";
    case ASM_PRINTER_CORE_BANK:
        return "a bank writes its register with other than one number";
    case ASM_PRINTER_CORE_TABLES:
        return "the ruleset and the header contain more words or characters than the assembly printer's tables support";
    default:
        return "the assembly printer's build ran past its memory";
    }
}
