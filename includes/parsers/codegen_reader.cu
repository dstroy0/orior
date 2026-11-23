// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// codegen_reader.cu: a ruleset's .krs file read, and its scratch arranged, on the device, and checked against the
// host's
#include "../../src/cu/engine/rmc/codegen_device.h"
#include "../../src/cu/engine/rmc/codegen_device_internal.h"
#include "../file_defs/krs/ruleset_flat.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int codegen_reader_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// 1 where two texts are the same
static int ruleset_same_text(const char *left, const char *right)
{
    return codegen_reader_text_order(left, right, PLATFORM_TEXT_TO_TERMINATOR) == 0;
}

// 1 where two lists of `count` templates are the same: their pieces and placeholders
static int ruleset_same_templates(const InstrTemplate *left, const InstrTemplate *right, unsigned int count)
{
    int same = 1;
    for (unsigned int at = 0u; (same != 0) && (at < count); at += 1u)
    {
        same = (left[at].placeholder_count == right[at].placeholder_count) &&
               ((left[at].placeholder_count == 0u) ||
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left[at].placeholders, .other = right[at].placeholders,
                            .bytes = (size_t)left[at].placeholder_count * sizeof(unsigned int)) == 0));
        for (unsigned int piece = 0u; (same != 0) && (piece <= left[at].placeholder_count); piece += 1u)
        {
            same = ruleset_same_text(left[at].pieces[piece], right[at].pieces[piece]);
        }
    }
    return same;
}

// 1 where two rulesets' `count` constructs are the same: each line's form and each argument's type, placeholder, number
// and text
static int ruleset_same_constructs(const Pseudo *left, const Pseudo *right, unsigned int count)
{
    int same = 1;
    for (unsigned int construct = 0u; (same != 0) && (construct < count); construct += 1u)
    {
        const Pseudo *const one = &left[construct];
        const Pseudo *const other = &right[construct];
        same = one->line_count == other->line_count;
        for (unsigned int line = 0u; (same != 0) && (line < one->line_count); line += 1u)
        {
            same = (one->lines[line].form == other->lines[line].form) &&
                   (one->lines[line].argument_count == other->lines[line].argument_count);
            for (unsigned int at = 0u; (same != 0) && (at < one->lines[line].argument_count); at += 1u)
            {
                const PseudoOperand *const given = &one->lines[line].arguments[at];
                const PseudoOperand *const taken = &other->lines[line].arguments[at];
                same = (given->type == taken->type) && (given->placeholder == taken->placeholder) &&
                       (given->number == taken->number) && ruleset_same_text(given->text, taken->text);
            }
        }
    }
    return same;
}

// 1 where two lists of `count` texts are the same
static int ruleset_same_texts(const char *const *left, const char *const *right, unsigned int count)
{
    int same = 1;
    for (unsigned int at = 0u; (same != 0) && (at < count); at += 1u)
    {
        same = ruleset_same_text(left[at], right[at]);
    }
    return same;
}

int ruleset_same(const Ruleset *left, const Ruleset *right)
{
    const RulesetSchema *const schema = left->schema;
    const int heads = (left->schema == right->schema) && ruleset_same_text(left->path, right->path) &&
                      ruleset_same_text(left->error, right->error);
    // a ruleset whose file did not open stores no lists, and two such are the same where their heads are
    if ((left->cell == NULL) || (right->cell == NULL))
    {
        return heads && (left->cell == right->cell);
    }
    return heads && ruleset_same_text(left->name, right->name) &&
           ruleset_same_text(left->toolchain, right->toolchain) && ruleset_same_text(left->header, right->header) &&
           ruleset_same_text(left->shared, right->shared) && ruleset_same_text(left->write_ports, right->write_ports) &&
           ruleset_same_text(left->part, right->part) &&
           ruleset_same_templates(left->banks, right->banks, schema->bank_count) &&
           ruleset_same_texts(left->fixed, right->fixed, schema->fixed_count) &&
           ruleset_same_templates(left->forms, right->forms, schema->form_count) &&
           ruleset_same_constructs(left->constructs, right->constructs, schema->form_count) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->bank_given, .other = right->bank_given,
                       .bytes = schema->bank_count) == 0) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->fixed_given, .other = right->fixed_given,
                       .bytes = schema->fixed_count) == 0) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->form_given, .other = right->form_given,
                       .bytes = schema->form_count) == 0) &&
           (left->building == right->building) && (left->building_parameter_count == right->building_parameter_count) &&
           ruleset_same_texts(left->building_parameters, right->building_parameters, left->building_parameter_count);
}

#if (defined(__CUDACC__))

// the read in one thread (ruleset_core_read), since each line is read against every line before it. The read stored in
// device memory and left there as it ended
__global__ void codegen_reader_read(RulesetCoreRead *read)
{
    if (codegen_device_thread() != 0ULL)
    {
        return;
    }
    ruleset_core_read(read);
}

// the scratch in one thread (ruleset_core_scratch), since each form's goes on from the forms its lines write
__global__ void codegen_reader_scratch(RulesetCoreScratch *scratch)
{
    if (codegen_device_thread() != 0ULL)
    {
        return;
    }
    ruleset_core_scratch(scratch);
}

int ruleset_read_device(Ruleset *rules, const char *path, VerbaLinea *error)
{
    PlatformText ruleset_text;
    ruleset_text_begun(&ruleset_text);
    if (ruleset_file(rules, path, &ruleset_text) == 0)
    {
        platform_text_ended(&ruleset_text);
        return 1;
    }
    const char *const text = platform_text_bytes(&ruleset_text);
    RulesetFlat flat;
    ruleset_flat_schema(rules->schema, &flat);
    DeviceArena reader_arena{};
    reader_arena.ok = (flat.cell != NULL) ? 1 : 0;
    RulesetCoreRead read{};
    read.schema = flat.schema;
    // the schema's one cell copied whole, and the device's view of it at the same offsets
    unsigned char *const device_schema =
        codegen_device_copy(&reader_arena, (const unsigned char *)flat.cell, flat.bytes);
    RulesetCellPlan schema_plan = {device_schema, 0ULL};
    ruleset_flat_layout(rules->schema, &schema_plan, &read.schema);
    // ruleset_file holds a ruleset below RULESET_FILE_MAX characters
    read.text_length = (unsigned int)platform_text_size(&ruleset_text);
    read.text = codegen_device_copy(&reader_arena, (const unsigned char *)text, read.text_length);
    ruleset_capacities(read.text_length, &read);
    RulesetMemory memory = {NULL, 0ULL};
    RulesetCoreRead sized = read;
    reader_arena.ok = (reader_arena.ok != 0) && ruleset_memory_size(&memory, &sized);
    // the memory's one cell taken whole on the device, and the read pointed into it at the same offsets
    unsigned char *const device_memory = codegen_device_take<unsigned char>(&reader_arena, memory.bytes);
    RulesetCellPlan memory_plan = {device_memory, 0ULL};
    ruleset_memory_layout(&memory_plan, &read);
    RulesetCoreRead *const device_read = codegen_device_copy(&reader_arena, &read, 1ULL);
    if (reader_arena.ok != 0)
    {
        codegen_reader_read<<<1u, 1u>>>(device_read);
        codegen_device_launched(&reader_arena);
    }
    RulesetCoreRead ended{};
    codegen_device_read(&reader_arena, &ended, device_read, 1ULL);
    // what the device read, its memory read back whole into the host's cell, and the host's view of it
    codegen_device_read(&reader_arena, (unsigned char *)memory.cell, device_memory, memory.bytes);
    RulesetCoreRead host = ended;
    host.schema = flat.schema;
    host.text = (const unsigned char *)text;
    RulesetCellPlan host_plan = {(unsigned char *)memory.cell, 0ULL};
    ruleset_memory_layout(&host_plan, &host);
    const int ok = reader_arena.ok;
    codegen_device_release(&reader_arena);
    if (ok != 0)
    {
        ruleset_store(&host, text, rules);
    }
    ruleset_memory_ended(&memory);
    ruleset_flat_ended(&flat);
    platform_text_ended(&ruleset_text);
    if (ok == 0)
    {
        mmgr_line_put(error, "the device errored on a call");
        return 0;
    }
    return 1;
}

int ruleset_scratch_device(const Ruleset *rules, const unsigned int *banks, AsmPrinterWords *scratch, VerbaLinea *error)
{
    const unsigned int forms = rules->schema->form_count;
    scratch->words = NULL;
    scratch->count = 0ULL;
    RulesetFlatConstructs flat;
    if (ruleset_flat_constructs(rules, &flat) == 0)
    {
        mmgr_line_put(error, "the pool cannot meet the constructs");
        return 0;
    }
    const unsigned long long words = (4ULL * forms) + 1ULL;
    scratch->words =
        (unsigned int *)platform_cell_cleared(words, sizeof(unsigned int), platform_place_g_program_owner, 0);
    DeviceArena scratch_arena{};
    scratch_arena.ok = (scratch->words != NULL) ? 1 : 0;
    RulesetCoreScratch arranged{};
    // the constructs' one cell copied whole, and the device's view of it at the same offsets
    unsigned char *const device_constructs =
        codegen_device_copy(&scratch_arena, (const unsigned char *)flat.cell, flat.bytes);
    ruleset_flat_constructs_at(&flat, device_constructs, &arranged);
    arranged.form_count = forms;
    arranged.banks[0] = banks[0];
    arranged.banks[1] = banks[1];
    arranged.banks[2] = banks[2];
    arranged.scratch = codegen_device_take<unsigned int>(&scratch_arena, words);
    scratch_arena.ok = (scratch_arena.ok != 0) &&
                       (cudaMemset(arranged.scratch, 0, (size_t)words * sizeof(unsigned int)) == cudaSuccess);
    arranged.counted = codegen_device_take<unsigned char>(&scratch_arena, (unsigned long long)forms + 1ULL);
    arranged.frames = codegen_device_take<RulesetCoreFrame>(&scratch_arena, (unsigned long long)forms + 1ULL);
    arranged.taken_bank =
        codegen_device_take<unsigned int>(&scratch_arena, (unsigned long long)flat.argument_count + 1ULL);
    arranged.taken_number =
        codegen_device_take<unsigned int>(&scratch_arena, (unsigned long long)flat.argument_count + 1ULL);
    RulesetCoreScratch *const device_scratch = codegen_device_copy(&scratch_arena, &arranged, 1ULL);
    if (scratch_arena.ok != 0)
    {
        codegen_reader_scratch<<<1u, 1u>>>(device_scratch);
        codegen_device_launched(&scratch_arena);
    }
    codegen_device_read(&scratch_arena, scratch->words, arranged.scratch, words);
    const int ok = scratch_arena.ok;
    codegen_device_release(&scratch_arena);
    ruleset_flat_constructs_ended(&flat);
    if (ok == 0)
    {
        platform_cell_released(scratch->words, platform_place_g_program_owner);
        scratch->words = NULL;
        mmgr_line_put(error, "the device errored on a call");
        return 0;
    }
    scratch->count = 4ULL * forms;
    return 1;
}

#else

int ruleset_read_device(Ruleset *rules, const char *path, VerbaLinea *error)
{
    (void)rules;
    (void)path;
    mmgr_line_put(error, "the build has no device");
    return 0;
}

int ruleset_scratch_device(const Ruleset *rules, const unsigned int *banks, AsmPrinterWords *scratch, VerbaLinea *error)
{
    (void)rules;
    (void)banks;
    scratch->words = NULL;
    scratch->count = 0ULL;
    mmgr_line_put(error, "the build has no device");
    return 0;
}

#endif
