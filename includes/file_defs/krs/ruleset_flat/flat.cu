// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// flat.cu: the flat ruleset: the relations read, a file, the names and the schema, the capacities, the memory's size,
// the template, what is stored, and the constructs
#include "ruleset_flat_internal.h"

// a cell of `bytes` from the pool, every byte zero, on the program's owner
static void *ruleset_cell_taken(unsigned long long bytes)
{
    return platform_cell_cleared((bytes != 0ULL) ? bytes : 1ULL, 1ULL, platform_place_g_program_owner, 0);
}

// The `length` bytes at `bytes` placed on `plan` with a terminator after them. On a pass with a base, the copy, else
// NULL
static const char *ruleset_text_placed(RulesetCellPlan *plan, const void *bytes, size_t length)
{
    char *const placed = (char *)ruleset_cell_place(plan, (unsigned long long)length + 1ULL, 1ULL);
    if (placed != NULL)
    {
        mmgr_bytes_copy(placed, bytes, length);
        placed[length] = '\0';
    }
    return placed;
}

// the lists of a relation read laid out on `plan`, for `characters` characters of files, `count` files whose labels
// take `label_bytes` in all, and `lines` lines. `labels` is NULL on a pass with no base
static void ruleset_relation_layout(RulesetCellPlan *plan, unsigned long long characters, unsigned int count,
                                    unsigned int lines, unsigned int relation_capacity, RulesetCoreRelations *relations,
                                    const char ***labels)
{
    relations->text = (const unsigned char *)ruleset_cell_place(plan, characters + 1ULL, 1ULL);
    relations->files =
        (const RulesetCoreSpan *)ruleset_cell_place(plan, (unsigned long long)count + 1ULL, sizeof(RulesetCoreSpan));
    relations->entries = (RulesetCoreEntry *)ruleset_cell_place(plan, lines, sizeof(RulesetCoreEntry));
    relations->marks = (unsigned int *)ruleset_cell_place(plan, lines, sizeof(unsigned int));
    relations->stack = (unsigned int *)ruleset_cell_place(plan, lines, sizeof(unsigned int));
    relations->relations =
        (RulesetCoreRelation *)ruleset_cell_place(plan, relation_capacity, sizeof(RulesetCoreRelation));
    *labels = (const char **)ruleset_cell_place(plan, (unsigned long long)count + 1ULL, sizeof(const char *));
}

// the part of `path` after its last separator, the name that a report knows a file by
static const char *ruleset_label(const char *path)
{
    const char *label = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        label = ((*at == '/') || (*at == '\\')) ? (at + 1) : label;
    }
    return label;
}

int ruleset_relations_read(const char (*paths)[PLATFORM_PATH_BYTES], unsigned int count,
                           const RulesetCoreSchema *schema, RulesetRelationMemory *memory,
                           RulesetCoreRelations *relations, VerbaLinea *error)
{
    memory->cell = NULL;
    memory->labels = NULL;
    PlatformText all;
    ruleset_text_begun(&all);
    PlatformArray files;
    platform_array_begun(&files, sizeof(RulesetCoreSpan), platform_place_g_program_owner, 0);
    unsigned int lines = count + 1u;
    for (unsigned int file = 0u; file < count; file += 1u)
    {
        const unsigned long long first = platform_text_size(&all);
        if (ruleset_bytes_read(paths[file], &all) == 0)
        {
            mmgr_line_put(error, paths[file]);
            platform_text_ended(&all);
            platform_array_ended(&files);
            return 0;
        }
        // a set of files is bounded under RULESET_FILE_MAX characters, which 32 bits count
        const RulesetCoreSpan span = {(unsigned int)first, (unsigned int)(platform_text_size(&all) - first)};
        (void)platform_array_appended(&files, &span);
        const char *const read = platform_text_bytes(&all);
        for (unsigned long long at = first; at < (first + span.length); at += 1ULL)
        {
            lines += (read[at] == '\n') ? 1u : 0u;
        }
    }
    const RulesetCoreSchema none = {NULL, NULL, NULL, 0u, NULL, 0u, NULL, 0u};
    relations->schema = (schema != NULL) ? *schema : none;
    const unsigned int relation_capacity = (lines * RULESET_CORE_RELATION_TYPES) + relations->schema.form_count +
                                           relations->schema.bank_count + relations->schema.fixed_count;
    const unsigned long long characters = platform_text_size(&all);
    unsigned long long label_bytes = 0ULL;
    for (unsigned int file = 0u; file < count; file += 1u)
    {
        label_bytes += mmgr_text_length(ruleset_label(paths[file]), PLATFORM_PATH_BYTES) + 16ULL;
    }
    const char **labels = NULL;
    RulesetCellPlan plan = {NULL, 0ULL};
    ruleset_relation_layout(&plan, characters, count, lines, relation_capacity, relations, &labels);
    plan.used += label_bytes;
    memory->cell = ruleset_cell_taken(plan.used);
    const int taken = (memory->cell != NULL) && (files.count == (unsigned long long)count) && mmgr_line_ok(&all.line);
    if (taken == 0)
    {
        mmgr_line_put(error, "the pool cannot meet the read");
        platform_cell_released(memory->cell, platform_place_g_program_owner);
        memory->cell = NULL;
        platform_text_ended(&all);
        platform_array_ended(&files);
        return 0;
    }
    plan.base = (unsigned char *)memory->cell;
    plan.used = 0ULL;
    ruleset_relation_layout(&plan, characters, count, lines, relation_capacity, relations, &labels);
    mmgr_bytes_copy((void *)relations->text, platform_text_bytes(&all), (size_t)characters);
    mmgr_bytes_copy((void *)relations->files, files.cell, (size_t)count * sizeof(RulesetCoreSpan));
    for (unsigned int file = 0u; file < count; file += 1u)
    {
        const char *const label = ruleset_label(paths[file]);
        labels[file] = ruleset_text_placed(&plan, label, mmgr_text_length(label, PLATFORM_PATH_BYTES));
    }
    memory->labels = labels;
    platform_text_ended(&all);
    platform_array_ended(&files);
    relations->file_count = count;
    relations->entry_capacity = lines;
    relations->relation_capacity = relation_capacity;
    ruleset_core_relations(relations);
    return 1;
}

void ruleset_relation_memory_ended(RulesetRelationMemory *memory)
{
    platform_cell_released(memory->cell, platform_place_g_program_owner);
    memory->cell = NULL;
    memory->labels = NULL;
}

int ruleset_file(Ruleset *rules, const char *path, PlatformText *text)
{
    const RulesetSchema *const schema = rules->schema;
    (void)mmgr_text_copy(rules->path, path, sizeof(rules->path));
    rules->building = schema->form_count;
    VerbaLinea error = mmgr_line_over(rules->error, sizeof(rules->error));
    const int read = ruleset_text(schema, path, text, &error);
    (void)mmgr_line_finish(&error);
    return read;
}

// the characters of the names of `count` names
static unsigned long long ruleset_name_characters(const RulesetName *names, unsigned int count)
{
    unsigned long long characters = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        characters += mmgr_text_length(names[at].text, PLATFORM_TEXT_TO_TERMINATOR);
    }
    return characters;
}

void ruleset_flat_layout(const RulesetSchema *schema, RulesetCellPlan *plan, RulesetCoreSchema *placed)
{
    const unsigned long long characters = ruleset_name_characters(schema->forms, schema->form_count) +
                                          ruleset_name_characters(schema->banks, schema->bank_count) +
                                          ruleset_name_characters(schema->fixed, schema->fixed_count);
    // every list takes at least one entry, and none is empty
    placed->characters = (const unsigned char *)ruleset_cell_place(plan, characters + 1ULL, 1ULL);
    placed->forms = (const RulesetCoreSpan *)ruleset_cell_place(plan, (unsigned long long)schema->form_count + 1ULL,
                                                                sizeof(RulesetCoreSpan));
    placed->form_parameters = (const unsigned int *)ruleset_cell_place(
        plan, (unsigned long long)schema->form_count + 1ULL, sizeof(unsigned int));
    placed->banks = (const RulesetCoreSpan *)ruleset_cell_place(plan, (unsigned long long)schema->bank_count + 1ULL,
                                                                sizeof(RulesetCoreSpan));
    placed->fixed = (const RulesetCoreSpan *)ruleset_cell_place(plan, (unsigned long long)schema->fixed_count + 1ULL,
                                                                sizeof(RulesetCoreSpan));
    placed->form_count = schema->form_count;
    placed->bank_count = schema->bank_count;
    placed->fixed_count = schema->fixed_count;
}

// `names` taken on into the flattened schema's characters from `characters` on, each name's span into `spans`. The
// answer is the characters after the last name
static unsigned int ruleset_flat_names(const RulesetName *names, unsigned int count, unsigned char *characters,
                                       unsigned int first, RulesetCoreSpan *spans)
{
    unsigned int at_character = first;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        // a schema's names are a few words each, a few thousand characters in all
        const RulesetCoreSpan span = {at_character,
                                      (unsigned int)mmgr_text_length(names[at].text, PLATFORM_TEXT_TO_TERMINATOR)};
        mmgr_bytes_copy(&characters[at_character], names[at].text, span.length);
        spans[at] = span;
        at_character += span.length;
    }
    return at_character;
}

void ruleset_flat_schema(const RulesetSchema *schema, RulesetFlat *flat)
{
    RulesetCellPlan plan = {NULL, 0ULL};
    ruleset_flat_layout(schema, &plan, &flat->schema);
    flat->bytes = plan.used;
    flat->cell = ruleset_cell_taken(plan.used);
    plan.base = (unsigned char *)flat->cell;
    plan.used = 0ULL;
    if (flat->cell == NULL)
    {
        // a schema with no cell reads as one that names nothing
        const RulesetCoreSchema none = {NULL, NULL, NULL, 0u, NULL, 0u, NULL, 0u};
        flat->schema = none;
        flat->bytes = 0ULL;
        return;
    }
    ruleset_flat_layout(schema, &plan, &flat->schema);
    // the cell is the flat's own, and the code writes its lists once here
    unsigned char *const characters = (unsigned char *)flat->schema.characters;
    unsigned int written =
        ruleset_flat_names(schema->forms, schema->form_count, characters, 0u, (RulesetCoreSpan *)flat->schema.forms);
    written = ruleset_flat_names(schema->banks, schema->bank_count, characters, written,
                                 (RulesetCoreSpan *)flat->schema.banks);
    (void)ruleset_flat_names(schema->fixed, schema->fixed_count, characters, written,
                             (RulesetCoreSpan *)flat->schema.fixed);
    unsigned int *const parameters = (unsigned int *)flat->schema.form_parameters;
    for (unsigned int form = 0u; form < schema->form_count; form += 1u)
    {
        parameters[form] = schema->forms[form].parameters;
    }
}

void ruleset_flat_ended(RulesetFlat *flat)
{
    platform_cell_released(flat->cell, platform_place_g_program_owner);
    flat->cell = NULL;
    flat->bytes = 0ULL;
}

void ruleset_capacities(unsigned int text_length, RulesetCoreRead *read)
{
    // every character written is one of the file's or is for an escape of two; every piece begins a text or follows a
    // placeholder, each at least three characters of the file, and every text is at least a line's; every word is at
    // least a character and a space, and a head's parameters are its words a form's fixed registers are folded into its
    // text past the characters read, at max once each
    read->character_capacity = (2u * text_length) + 2u;
    read->piece_capacity = (2u * text_length) + 2u;
    read->word_capacity = text_length + 2u;
}

void ruleset_memory_layout(RulesetCellPlan *plan, RulesetCoreRead *read)
{
    const RulesetCoreSchema *const schema = &read->schema;
    // every list takes at least one entry
    read->characters = (unsigned char *)ruleset_cell_place(plan, read->character_capacity, 1ULL);
    read->pieces = (RulesetCoreSpan *)ruleset_cell_place(plan, read->piece_capacity, sizeof(RulesetCoreSpan));
    read->placeholders = (unsigned int *)ruleset_cell_place(plan, read->piece_capacity, sizeof(unsigned int));
    read->words = (RulesetCoreSpan *)ruleset_cell_place(plan, read->word_capacity, sizeof(RulesetCoreSpan));
    read->banks = (RulesetCoreTemplate *)ruleset_cell_place(plan, (unsigned long long)schema->bank_count + 1ULL,
                                                            sizeof(RulesetCoreTemplate));
    read->fixed = (RulesetCoreSpan *)ruleset_cell_place(plan, (unsigned long long)schema->fixed_count + 1ULL,
                                                        sizeof(RulesetCoreSpan));
    read->forms = (RulesetCoreTemplate *)ruleset_cell_place(plan, (unsigned long long)schema->form_count + 1ULL,
                                                            sizeof(RulesetCoreTemplate));
    read->constructs = (RulesetCoreConstruct *)ruleset_cell_place(plan, (unsigned long long)schema->form_count + 1ULL,
                                                                  sizeof(RulesetCoreConstruct));
    read->lines = (RulesetCoreLine *)ruleset_cell_place(plan, (unsigned long long)read->text_length + 1ULL,
                                                        sizeof(RulesetCoreLine));
    read->arguments = (RulesetCoreArgument *)ruleset_cell_place(plan, (unsigned long long)read->text_length + 1ULL,
                                                                sizeof(RulesetCoreArgument));
    read->bank_given = (unsigned char *)ruleset_cell_place(plan, (unsigned long long)schema->bank_count + 1ULL, 1ULL);
    read->fixed_given = (unsigned char *)ruleset_cell_place(plan, (unsigned long long)schema->fixed_count + 1ULL, 1ULL);
    read->form_given = (unsigned char *)ruleset_cell_place(plan, (unsigned long long)schema->form_count + 1ULL, 1ULL);
    read->building_parameters =
        (RulesetCoreSpan *)ruleset_cell_place(plan, read->word_capacity, sizeof(RulesetCoreSpan));
}

int ruleset_memory_size(RulesetMemory *memory, RulesetCoreRead *read)
{
    RulesetCellPlan plan = {NULL, 0ULL};
    ruleset_memory_layout(&plan, read);
    memory->bytes = plan.used;
    memory->cell = ruleset_cell_taken(plan.used);
    plan.base = (unsigned char *)memory->cell;
    plan.used = 0ULL;
    ruleset_memory_layout(&plan, read);
    return (memory->cell != NULL) ? 1 : 0;
}

void ruleset_memory_ended(RulesetMemory *memory)
{
    platform_cell_released(memory->cell, platform_place_g_program_owner);
    memory->cell = NULL;
    memory->bytes = 0ULL;
}

// the file's span `span` placed on `plan` as a terminated text
static const char *ruleset_flat_text(RulesetCellPlan *plan, const char *text, RulesetCoreSpan span)
{
    return ruleset_text_placed(plan, &text[span.first], span.length);
}

// The template `form` of `read` placed on `plan` as the host stores it, into `made`. A template that the file did not
// give has one empty piece
static void ruleset_flat_template(RulesetCellPlan *plan, const RulesetCoreRead *read, const RulesetCoreTemplate *form,
                                  InstrTemplate *made)
{
    const unsigned int pieces = (form->piece_count != 0u) ? form->piece_count : 1u;
    made->placeholder_count = pieces - 1u;
    made->pieces = (const char **)ruleset_cell_place(plan, pieces, sizeof(const char *));
    made->placeholders = (unsigned int *)ruleset_cell_place(plan, (unsigned long long)made->placeholder_count + 1ULL,
                                                            sizeof(unsigned int));
    for (unsigned int piece = 0u; piece < pieces; piece += 1u)
    {
        const RulesetCoreSpan span =
            (form->piece_count != 0u) ? read->pieces[form->piece_first + piece] : RulesetCoreSpan{0u, 0u};
        const char *const text = ruleset_text_placed(plan, &read->characters[span.first], span.length);
        if (made->pieces != NULL)
        {
            made->pieces[piece] = text;
        }
    }
    for (unsigned int at = 0u; (made->placeholders != NULL) && (at < made->placeholder_count); at += 1u)
    {
        made->placeholders[at] = read->placeholders[form->placeholder_first + at];
    }
}

// why `read` did not read the file, as the host's reader gives it, onto `why`
static void ruleset_flat_why(const RulesetCoreRead *read, const char *text, const RulesetSchema *schema,
                             VerbaLinea *why)
{
    const char *const word = &text[read->word.first];
    const size_t length = read->word.length;
    const char *const building = (read->at < schema->form_count) ? schema->forms[read->at].text : "";
    switch (read->end)
    {
    case RULESET_CORE_NOT_KRS:
        mmgr_line_put(why, "it is not krs 1");
        break;
    case RULESET_CORE_NOT_ONE_WORD:
        (void)mmgr_line_put_n(why, word, length);
        mmgr_line_put(why, " is not one word given once");
        break;
    case RULESET_CORE_BANK_WRONG:
        mmgr_line_put(why, "the bank ");
        (void)mmgr_line_put_n(why, word, length);
        mmgr_line_put(why, " is not one the code generator takes from, or is given twice or written wrong");
        break;
    case RULESET_CORE_REGISTER_WRONG:
        mmgr_line_put(why, "the register ");
        (void)mmgr_line_put_n(why, word, length);
        mmgr_line_put(why, " is not one the code generator names, or is given twice or written wrong");
        break;
    case RULESET_CORE_FORM_WRONG:
        mmgr_line_put(why, "the form ");
        (void)mmgr_line_put_n(why, word, length);
        mmgr_line_put(why, " is not one the code generator writes, takes other parameters, or is given twice or "
                           "written wrong");
        break;
    case RULESET_CORE_CONSTRUCT_WRONG:
        mmgr_line_put(why, "the construct ");
        (void)mmgr_line_put_n(why, word, length);
        mmgr_line_put(why, " is not a form the code generator writes, takes other parameters, or is given twice");
        break;
    case RULESET_CORE_NO_TYPE:
        mmgr_line_put(why, "no entry is of the type ");
        (void)mmgr_line_put_n(why, word, length);
        break;
    case RULESET_CORE_NO_LINES:
        mmgr_line_put(why, "the construct ");
        mmgr_line_put(why, building);
        mmgr_line_put(why, " has no lines");
        break;
    case RULESET_CORE_LINE_FORM:
        mmgr_line_put(why, "the construct ");
        mmgr_line_put(why, building);
        mmgr_line_put(why, " writes ");
        (void)mmgr_line_put_n(why, word, length);
        mmgr_line_put(why, ", which the file has not given before it or which takes other arguments");
        break;
    case RULESET_CORE_SCRATCH_WORD:
        mmgr_line_put(why, "the construct ");
        mmgr_line_put(why, building);
        mmgr_line_put(why, " takes a scratch register ");
        (void)mmgr_line_put_n(why, word, length);
        mmgr_line_put(why, " of no bank the ruleset gives, or with no number");
        break;
    case RULESET_CORE_NO_END:
        mmgr_line_put(why, "the construct ");
        mmgr_line_put(why, building);
        mmgr_line_put(why, " has no end");
        break;
    case RULESET_CORE_BANK_MISSING:
        mmgr_line_put(why, "the bank ");
        mmgr_line_put(why, schema->banks[read->at].text);
        mmgr_line_put(why, " is not given");
        break;
    case RULESET_CORE_REGISTER_MISSING:
        mmgr_line_put(why, "the register ");
        mmgr_line_put(why, schema->fixed[read->at].text);
        mmgr_line_put(why, " is not given");
        break;
    default:
        mmgr_line_put(why, "its ruleset, toolchain or header is not named");
        break;
    }
}

// What `read` read placed on `plan` as the host stores it, into `rules`. A pass with no base counts the bytes and
// writes no list
static void ruleset_store_pass(RulesetCellPlan *plan, const RulesetCoreRead *read, const char *text, Ruleset *rules)
{
    const RulesetSchema *const schema = rules->schema;
    const int placing = plan->base != NULL;
    InstrTemplate *const banks = (InstrTemplate *)ruleset_cell_place(plan, schema->bank_count, sizeof(InstrTemplate));
    InstrTemplate *const forms = (InstrTemplate *)ruleset_cell_place(plan, schema->form_count, sizeof(InstrTemplate));
    const char **const fixed = (const char **)ruleset_cell_place(plan, schema->fixed_count, sizeof(const char *));
    Pseudo *const constructs = (Pseudo *)ruleset_cell_place(plan, schema->form_count, sizeof(Pseudo));
    unsigned char *const bank_given = (unsigned char *)ruleset_cell_place(plan, schema->bank_count, 1ULL);
    unsigned char *const fixed_given = (unsigned char *)ruleset_cell_place(plan, schema->fixed_count, 1ULL);
    unsigned char *const form_given = (unsigned char *)ruleset_cell_place(plan, schema->form_count, 1ULL);
    for (unsigned int bank = 0u; bank < schema->bank_count; bank += 1u)
    {
        InstrTemplate made;
        ruleset_flat_template(plan, read, &read->banks[bank], &made);
        if (placing)
        {
            banks[bank] = made;
            bank_given[bank] = read->bank_given[bank];
        }
    }
    for (unsigned int at = 0u; at < schema->fixed_count; at += 1u)
    {
        const RulesetCoreSpan span = read->fixed[at];
        const char *const written = ruleset_text_placed(plan, &read->characters[span.first], span.length);
        if (placing)
        {
            fixed[at] = written;
            fixed_given[at] = read->fixed_given[at];
        }
    }
    for (unsigned int form = 0u; form < schema->form_count; form += 1u)
    {
        InstrTemplate made;
        ruleset_flat_template(plan, read, &read->forms[form], &made);
        const RulesetCoreConstruct *const construct = &read->constructs[form];
        PseudoLine *const lines = (PseudoLine *)ruleset_cell_place(plan, construct->line_count, sizeof(PseudoLine));
        for (unsigned int line = 0u; line < construct->line_count; line += 1u)
        {
            const RulesetCoreLine *const read_line = &read->lines[construct->line_first + line];
            PseudoOperand *const arguments =
                (PseudoOperand *)ruleset_cell_place(plan, read_line->argument_count, sizeof(PseudoOperand));
            for (unsigned int given = 0u; given < read_line->argument_count; given += 1u)
            {
                const RulesetCoreArgument *const argument = &read->arguments[read_line->argument_first + given];
                const char *const argument_text = ruleset_flat_text(plan, text, argument->text);
                if (placing)
                {
                    const PseudoOperand operand = {(PseudoOperandType)argument->type, argument->placeholder,
                                                   argument->number, argument_text};
                    arguments[given] = operand;
                }
            }
            if (placing)
            {
                const PseudoLine made_line = {read_line->form, arguments, read_line->argument_count};
                lines[line] = made_line;
            }
        }
        if (placing)
        {
            forms[form] = made;
            form_given[form] = read->form_given[form];
            const Pseudo made_construct = {lines, construct->line_count};
            constructs[form] = made_construct;
        }
    }
    const char *const name = ruleset_flat_text(plan, text, read->name);
    const char *const toolchain = ruleset_flat_text(plan, text, read->toolchain);
    const char *const header = ruleset_flat_text(plan, text, read->header);
    const char *const shared = ruleset_flat_text(plan, text, read->shared);
    const char *const write_ports = ruleset_flat_text(plan, text, read->write_ports);
    const char *const part = ruleset_flat_text(plan, text, read->part);
    const char **const building_parameters =
        (const char **)ruleset_cell_place(plan, read->building_parameter_count, sizeof(const char *));
    for (unsigned int parameter = 0u; parameter < read->building_parameter_count; parameter += 1u)
    {
        const char *const written = ruleset_flat_text(plan, text, read->building_parameters[parameter]);
        if (placing)
        {
            building_parameters[parameter] = written;
        }
    }
    if (placing)
    {
        rules->banks = banks;
        rules->forms = forms;
        rules->fixed = fixed;
        rules->constructs = constructs;
        rules->bank_given = bank_given;
        rules->fixed_given = fixed_given;
        rules->form_given = form_given;
        rules->name = name;
        rules->toolchain = toolchain;
        rules->header = header;
        rules->shared = shared;
        rules->write_ports = write_ports;
        rules->part = part;
        rules->building_parameters = building_parameters;
        rules->building_parameter_count = read->building_parameter_count;
    }
}

void ruleset_store(const RulesetCoreRead *read, const char *text, Ruleset *rules)
{
    platform_cell_released(rules->cell, platform_place_g_program_owner);
    rules->cell = NULL;
    RulesetCellPlan plan = {NULL, 0ULL};
    ruleset_store_pass(&plan, read, text, rules);
    rules->cell = ruleset_cell_taken(plan.used);
    rules->building = read->building;
    VerbaLinea error = mmgr_line_over(rules->error, sizeof(rules->error));
    if (rules->cell == NULL)
    {
        mmgr_line_put(&error, "the pool cannot meet the ruleset");
        (void)mmgr_line_finish(&error);
        return;
    }
    plan.base = (unsigned char *)rules->cell;
    plan.used = 0ULL;
    ruleset_store_pass(&plan, read, text, rules);
    if (read->end != RULESET_CORE_OK)
    {
        if (read->line != 0u)
        {
            mmgr_line_put(&error, "line ");
            mmgr_line_decimal(&error, read->line, 1u);
            mmgr_line_put(&error, ": ");
        }
        ruleset_flat_why(read, text, rules->schema, &error);
    }
    (void)mmgr_line_finish(&error);
}

void ruleset_released(Ruleset *rules)
{
    platform_cell_released(rules->cell, platform_place_g_program_owner);
    platform_array_ended(&rules->folds);
    rules->cell = NULL;
    rules->banks = NULL;
    rules->forms = NULL;
    rules->fixed = NULL;
    rules->constructs = NULL;
    rules->bank_given = NULL;
    rules->fixed_given = NULL;
    rules->form_given = NULL;
    rules->name = "";
    rules->toolchain = "";
    rules->header = "";
    rules->shared = "";
    rules->write_ports = "";
    rules->part = "";
    rules->building_parameters = NULL;
    rules->building_parameter_count = 0u;
}

// the lists of a construct's flat form laid out on `plan`, for the counts in `flat`, into the pointers of `flat`
static void ruleset_constructs_layout(RulesetCellPlan *plan, RulesetFlatConstructs *flat)
{
    // every list takes at least one entry, and none is empty
    flat->constructs = (RulesetCoreConstruct *)ruleset_cell_place(plan, (unsigned long long)flat->form_count + 1ULL,
                                                                  sizeof(RulesetCoreConstruct));
    flat->lines = (RulesetCoreLine *)ruleset_cell_place(plan, (unsigned long long)flat->line_count + 1ULL,
                                                        sizeof(RulesetCoreLine));
    flat->arguments = (RulesetCoreArgument *)ruleset_cell_place(plan, (unsigned long long)flat->argument_count + 1ULL,
                                                                sizeof(RulesetCoreArgument));
    flat->form_placeholder_first =
        (unsigned int *)ruleset_cell_place(plan, (unsigned long long)flat->form_count + 1ULL, sizeof(unsigned int));
    flat->form_placeholders = (unsigned int *)ruleset_cell_place(
        plan, (unsigned long long)flat->placeholder_count + 1ULL, sizeof(unsigned int));
}

int ruleset_flat_constructs(const Ruleset *rules, RulesetFlatConstructs *flat)
{
    const unsigned int forms = rules->schema->form_count;
    flat->form_count = forms;
    flat->line_count = 0u;
    flat->argument_count = 0u;
    flat->placeholder_count = 0u;
    for (unsigned int form = 0u; form < forms; form += 1u)
    {
        const Pseudo *const construct = &rules->constructs[form];
        flat->line_count += construct->line_count;
        for (unsigned int line = 0u; line < construct->line_count; line += 1u)
        {
            flat->argument_count += construct->lines[line].argument_count;
        }
        flat->placeholder_count += rules->forms[form].placeholder_count;
    }
    RulesetCellPlan plan = {NULL, 0ULL};
    ruleset_constructs_layout(&plan, flat);
    flat->bytes = plan.used;
    flat->cell = ruleset_cell_taken(plan.used);
    if (flat->cell == NULL)
    {
        return 0;
    }
    plan.base = (unsigned char *)flat->cell;
    plan.used = 0ULL;
    ruleset_constructs_layout(&plan, flat);
    unsigned int line_at = 0u;
    unsigned int argument_at = 0u;
    unsigned int placeholder_at = 0u;
    for (unsigned int form = 0u; form < forms; form += 1u)
    {
        const Pseudo *const construct = &rules->constructs[form];
        const RulesetCoreConstruct flattened = {line_at, construct->line_count};
        flat->constructs[form] = flattened;
        for (unsigned int line = 0u; line < construct->line_count; line += 1u)
        {
            const PseudoLine *const given_line = &construct->lines[line];
            const RulesetCoreLine flat_line = {given_line->form, argument_at, given_line->argument_count};
            flat->lines[line_at] = flat_line;
            line_at += 1u;
            for (unsigned int given = 0u; given < given_line->argument_count; given += 1u)
            {
                const PseudoOperand *const operand = &given_line->arguments[given];
                const RulesetCoreArgument argument = {(unsigned int)operand->type, operand->placeholder,
                                                      operand->number, RulesetCoreSpan{0u, 0u}};
                flat->arguments[argument_at] = argument;
                argument_at += 1u;
            }
        }
        // each form's placeholders end to end, the scratch its text names among them
        flat->form_placeholder_first[form] = placeholder_at;
        for (unsigned int at = 0u; at < rules->forms[form].placeholder_count; at += 1u)
        {
            flat->form_placeholders[placeholder_at] = rules->forms[form].placeholders[at];
            placeholder_at += 1u;
        }
    }
    flat->form_placeholder_first[forms] = placeholder_at;
    return 1;
}

void ruleset_flat_constructs_at(const RulesetFlatConstructs *flat, unsigned char *base, RulesetCoreScratch *arranged)
{
    RulesetFlatConstructs placed = *flat;
    RulesetCellPlan plan = {base, 0ULL};
    ruleset_constructs_layout(&plan, &placed);
    arranged->constructs = placed.constructs;
    arranged->lines = placed.lines;
    arranged->arguments = placed.arguments;
    arranged->form_placeholder_first = placed.form_placeholder_first;
    arranged->form_placeholders = placed.form_placeholders;
}

void ruleset_flat_constructs_ended(RulesetFlatConstructs *flat)
{
    platform_cell_released(flat->cell, platform_place_g_program_owner);
    flat->cell = NULL;
    flat->bytes = 0ULL;
}
