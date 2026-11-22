// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// target_rulesets.cu: rulesets read, loaded and written
#include "../engine_note.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "memoria_operor/memoria_linea.h"
#include "target_internal.h"

// the ruleset at `path` read whole into `rules` against its schema: 1 where its first line is krs 1, every entry is
// valid, and every bank, fixed register and form the code generator names is given with the ruleset's name, toolchain
// and header, else 0 with the reason in rules->error. A line that begins with # is a comment, and a blank line is
// nothing
static int ruleset_read(Ruleset *rules, const char *path)
{
    PlatformText ruleset_text;
    ruleset_text_begun(&ruleset_text);
    if (ruleset_file(rules, path, &ruleset_text) == 0)
    {
        platform_text_ended(&ruleset_text);
        return 0;
    }
    // the file read by the core (ruleset_core.h) the device runs as well
    RulesetFlat flat;
    ruleset_flat_schema(rules->schema, &flat);
    RulesetCoreRead read{};
    read.schema = flat.schema;
    const char *const text = platform_text_bytes(&ruleset_text);
    read.text = (const unsigned char *)text;
    // ruleset_file holds a ruleset below RULESET_FILE_MAX characters
    read.text_length = (unsigned int)platform_text_size(&ruleset_text);
    ruleset_capacities(read.text_length, &read);
    RulesetMemory memory;
    if ((flat.cell == NULL) || (ruleset_memory_size(&memory, &read) == 0))
    {
        VerbaLinea error = mmgr_line_over(rules->error, sizeof(rules->error));
        mmgr_line_put(&error, "the pool cannot meet the read");
        (void)mmgr_line_finish(&error);
        ruleset_flat_ended(&flat);
        platform_text_ended(&ruleset_text);
        return 0;
    }
    ruleset_core_read(&read);
    ruleset_store(&read, text, rules);
    ruleset_memory_ended(&memory);
    ruleset_flat_ended(&flat);
    platform_text_ended(&ruleset_text);
    return (rules->error[0] == '\0') ? 1 : 0;
}

// the bytes of a scratch register's name, its terminator among them
#define RULESET_SCRATCH_NAME_BYTES 64u

// a scratch register that one writing took: what the writing knows it by, and its name as the writer gave it
struct RulesetScratchTaken
{
    unsigned int key;
    unsigned int number;
    char name[RULESET_SCRATCH_NAME_BYTES];
};

// the words of the `length` characters at `line`, cut at white space, into `words`, at max `max` of them. The answer is
// the count of words, which can pass `max`
static unsigned int ruleset_words(const char *line, size_t length, RulesetCoreSpan *words, unsigned int max)
{
    unsigned int count = 0u;
    size_t at = 0u;
    while (at < length)
    {
        while ((at < length) && ((line[at] == ' ') || (line[at] == '\t') || (line[at] == '\r') || (line[at] == '\n')))
        {
            at += 1u;
        }
        const size_t first = at;
        while ((at < length) && (line[at] != ' ') && (line[at] != '\t') && (line[at] != '\r') && (line[at] != '\n'))
        {
            at += 1u;
        }
        if (at > first)
        {
            if (count < max)
            {
                // a line of a ruleset is below RULESET_FILE_MAX characters, which 32 bits count
                const RulesetCoreSpan word = {(unsigned int)first, (unsigned int)(at - first)};
                words[count] = word;
            }
            count += 1u;
        }
    }
    return count;
}

// 1 where the word `word` of `line` is the text `text`
static int ruleset_word_is(const char *line, RulesetCoreSpan word, const char *text)
{
    return (mmgr_text_length(text, PLATFORM_TEXT_TO_TERMINATOR) == word.length) &&
           (mmgr_bytes_compare(&line[word.first], text, word.length) == 0);
}

// the place of `needle` in the `length` characters at `text` from `from` on, or `length` where it is not there
static size_t ruleset_span_find(const char *text, size_t length, const char *needle, size_t from)
{
    const size_t needle_length = mmgr_text_length(needle, PLATFORM_TEXT_TO_TERMINATOR);
    for (size_t at = from; (at + needle_length) <= length; at += 1u)
    {
        if (mmgr_bytes_compare(&text[at], needle, needle_length) == 0)
        {
            return at;
        }
    }
    return length;
}

// the most words of one line of a ruleset that a reader of its parameters reads
#define RULESET_LINE_WORDS 64u

unsigned int ruleset_parameter_place(const Ruleset *rules, const char *form, const char *parameter)
{
    PlatformText text;
    ruleset_text_begun(&text);
    char why[RULESET_ERROR_BYTES];
    VerbaLinea error = mmgr_line_over(why, sizeof(why));
    (void)ruleset_text(rules->schema, rules->path, &text, &error);
    const char *const bytes = platform_text_bytes(&text);
    const size_t size = (size_t)platform_text_size(&text);
    unsigned int place = RULESET_NO_PARAMETER;
    size_t at = 0u;
    while (at < size)
    {
        const char *const newline = mmgr_text_char(&bytes[at], '\n', size - at);
        // a line ends at or past its start
        const size_t end = (newline != NULL) ? (size_t)(newline - bytes) : size;
        const char *const line = &bytes[at];
        RulesetCoreSpan words[RULESET_LINE_WORDS];
        const unsigned int counted = ruleset_words(line, end - at, words, RULESET_LINE_WORDS);
        const unsigned int count = (counted < RULESET_LINE_WORDS) ? counted : RULESET_LINE_WORDS;
        at = end + 1u;
        const int named = (count >= 2u) && ruleset_word_is(line, words[1], form) &&
                          (ruleset_word_is(line, words[0], "form") || ruleset_word_is(line, words[0], "err") ||
                           ruleset_word_is(line, words[0], "nop") || ruleset_word_is(line, words[0], "construct"));
        for (unsigned int word = 2u; (named != 0) && (word < count) && !ruleset_word_is(line, words[word], "=");
             word += 1u)
        {
            place = ((place == RULESET_NO_PARAMETER) && ruleset_word_is(line, words[word], parameter)) ? (word - 2u)
                                                                                                       : place;
        }
        if (named != 0)
        {
            break;
        }
    }
    platform_text_ended(&text);
    return place;
}

// the `length` characters at `bytes` copied into `out`, `bytes_out` of it, terminated: 1 where they fit
static int ruleset_span_copied(const char *bytes, size_t length, char *out, size_t bytes_out)
{
    VerbaLinea line = mmgr_line_over(out, bytes_out);
    (void)mmgr_line_put_n(&line, bytes, length);
    return (mmgr_line_finish(&line) == length) && (mmgr_line_ok(&line) != 0);
}

void ruleset_folds_read(Ruleset *rules)
{
    (void)platform_array_counted(&rules->folds, 0ULL);
    // the .ksc beside the ruleset as the run stores it (ruleset_bytes_read)
    char ksc_path[PLATFORM_PATH_BYTES];
    const char *const dot = mmgr_text_char_last(rules->path, '.', sizeof(rules->path));
    const size_t stem =
        (dot != NULL) ? (size_t)(dot - rules->path) : mmgr_text_length(rules->path, sizeof(rules->path));
    VerbaLinea path_line = mmgr_line_over(ksc_path, sizeof(ksc_path));
    (void)mmgr_line_put_n(&path_line, rules->path, stem);
    mmgr_line_put(&path_line, ".ksc");
    (void)mmgr_line_finish(&path_line);
    PlatformText ksc;
    ruleset_text_begun(&ksc);
    if (!ruleset_bytes_read(ksc_path, &ksc))
    {
        platform_text_ended(&ksc);
        return;
    }
    const char *const folds = "compile folds ";
    const char *const system = ": the system folds the ";
    const char *const put = " put for ";
    const size_t folds_length = mmgr_text_length(folds, PLATFORM_TEXT_TO_TERMINATOR);
    const size_t system_length = mmgr_text_length(system, PLATFORM_TEXT_TO_TERMINATOR);
    const size_t put_length = mmgr_text_length(put, PLATFORM_TEXT_TO_TERMINATOR);
    const char *const bytes = platform_text_bytes(&ksc);
    const size_t size = (size_t)platform_text_size(&ksc);
    size_t line_at = 0u;
    while (line_at < size)
    {
        const char *const newline = mmgr_text_char(&bytes[line_at], '\n', size - line_at);
        // a line ends at or past its start
        const size_t line_end = (newline != NULL) ? (size_t)(newline - bytes) : size;
        const char *const line = &bytes[line_at];
        const size_t length = line_end - line_at;
        line_at = line_end + 1u;
        const size_t colon = ruleset_span_find(line, length, system, 0u);
        const size_t type_end = (colon < length) ? ruleset_span_find(line, length, put, colon + system_length) : length;
        if ((length < folds_length) || (mmgr_bytes_compare(line, folds, folds_length) != 0) || (colon == length) ||
            (type_end == length))
        {
            continue;
        }
        RulesetCoreSpan head[5];
        const unsigned int head_count = ruleset_words(line, colon, head, 5u);
        RulesetCoreSpan role[2];
        const unsigned int role_count =
            ruleset_words(&line[type_end + put_length], length - (type_end + put_length), role, 2u);
        const RulesetSchema *const schema = rules->schema;
        const unsigned int form = (head_count == 4u) ? ruleset_find_span(schema->forms, schema->form_count,
                                                                         &line[head[3].first], head[3].length)
                                                     : schema->form_count;
        RulesetFold fold;
        mmgr_bytes_set(&fold, 0, sizeof(fold));
        char role_name[RULESET_SCRATCH_NAME_BYTES];
        const int named = (form != schema->form_count) && (role_count == 1u) &&
                          ruleset_span_copied(&line[type_end + put_length + role[0].first], role[0].length, role_name,
                                              sizeof(role_name)) &&
                          ruleset_span_copied(&line[colon + system_length], type_end - colon - system_length, fold.type,
                                              sizeof(fold.type));
        const unsigned int place =
            (named != 0) ? ruleset_parameter_place(rules, schema->forms[form].text, role_name) : RULESET_NO_PARAMETER;
        if (place != RULESET_NO_PARAMETER)
        {
            fold.form = form;
            fold.parameter = place;
            (void)platform_array_appended(&rules->folds, &fold);
        }
    }
    platform_text_ended(&ksc);
}

// a ruleset read once a process into `rules` from its schema's file; NULL where it errors. Where the schema names a
// toolchain and header, a ruleset naming others errors with the rest
static const Ruleset *ruleset_load(Ruleset *rules, const RulesetSchema *schema, int report)
{
    if (rules->tried != 0)
    {
        return (rules->ready != 0) ? rules : NULL;
    }
    rules->tried = 1;
    rules->schema = schema;
    char folder[PLATFORM_PATH_BYTES];
    char path[PLATFORM_PATH_BYTES];
    VerbaLinea path_line = mmgr_line_over(path, sizeof(path));
    if (ruleset_folder(folder, sizeof(folder)) != 0)
    {
        mmgr_line_put(&path_line, folder);
        mmgr_line_ch(&path_line, '/');
        mmgr_line_put(&path_line, schema->file);
    }
    (void)mmgr_line_finish(&path_line);
    rules->ready = ruleset_read(rules, path);
    if ((rules->ready != 0) && (schema->toolchain != NULL) &&
        ((mmgr_text_order(rules->toolchain, schema->toolchain, PLATFORM_TEXT_TO_TERMINATOR) != 0) ||
         (mmgr_text_order(rules->header, schema->header, PLATFORM_TEXT_TO_TERMINATOR) != 0)))
    {
        rules->ready = 0;
        VerbaLinea error = mmgr_line_over(rules->error, sizeof(rules->error));
        mmgr_line_put(&error, "its path builds with ");
        mmgr_line_put(&error, schema->toolchain);
        mmgr_line_put(&error, " and takes its header from ");
        mmgr_line_put(&error, schema->header);
        (void)mmgr_line_finish(&error);
    }
    if (rules->ready != 0)
    {
        ruleset_folds_read(rules);
    }
    if ((report != 0) && (rules->ready != 0))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: the ruleset ");
        mmgr_line_put(&engine_note.line, rules->name);
        mmgr_line_put(&engine_note.line, " read from ");
        mmgr_line_put(&engine_note.line, rules->path);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    else if (report != 0)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: the ruleset at ");
        mmgr_line_put(&engine_note.line, rules->path);
        mmgr_line_put(&engine_note.line, " errors (");
        mmgr_line_put(&engine_note.line, rules->error);
        mmgr_line_put(&engine_note.line, ")");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return (rules->ready != 0) ? rules : NULL;
}

// The base retains its language's ruleset from construction, unread, in a cell of the pool, and reads it at the first
// call to ruleset(). A base whose cell the pool refused reads no ruleset
Target::Target(const RulesetSchema *schema)
    : rules((Ruleset *)platform_cell_cleared(1ULL, sizeof(Ruleset), platform_place_g_program_owner, 0))
{
    if (rules != NULL)
    {
        rules->schema = schema;
        platform_array_begun(&rules->folds, sizeof(RulesetFold), platform_place_g_program_owner, 0);
        ruleset_released(rules);
    }
}

// The ruleset given back where the pool is there. A target that is a static ends after main returns. The pool gave
// its pages back by then, with every cell on them, and the target then reads none
Target::~Target()
{
    if ((rules != NULL) && platform_pool_placed())
    {
        ruleset_released(rules);
        platform_cell_released(rules, platform_place_g_program_owner);
    }
}

const Ruleset *Target::ruleset(int report)
{
    return ((rules != NULL) && (rules->schema != NULL)) ? ruleset_load(rules, rules->schema, report) : NULL;
}

const RulesetSchema *Target::schema(void) const
{
    return (rules != NULL) ? rules->schema : NULL;
}

const Ruleset *Target::ready(void) const
{
    return ((rules != NULL) && (rules->ready != 0)) ? rules : NULL;
}

void Target::folds_read(void)
{
    if ((rules != NULL) && (rules->ready != 0))
    {
        ruleset_folds_read(rules);
    }
}

// The scratch register that `taken` holds under `key` and `number`, or one taken of `scratch` and appended. NULL where
// `scratch` gives none or the pool cannot meet it
static const char *ruleset_scratch_taken(PlatformArray *taken, unsigned int key, unsigned int number,
                                         const ScratchRegisters *scratch, unsigned int bank)
{
    for (unsigned long long at = 0ULL; at < taken->count; at += 1ULL)
    {
        const RulesetScratchTaken *const one = (const RulesetScratchTaken *)platform_array_at(taken, at);
        if ((one->key == key) && (one->number == number))
        {
            return one->name;
        }
    }
    RulesetScratchTaken made;
    mmgr_bytes_set(&made, 0, sizeof(made));
    made.key = key;
    made.number = number;
    VerbaLinea name = mmgr_line_over(made.name, sizeof(made.name));
    const int given = (scratch != NULL) && (scratch->taken(scratch->context, bank, &name) != 0);
    const size_t length = mmgr_line_finish(&name);
    if ((given == 0) || (length == 0u) || (mmgr_line_ok(&name) == 0) || (platform_array_appended(taken, &made) == 0))
    {
        return NULL;
    }
    return ((const RulesetScratchTaken *)platform_array_at(taken, taken->count - 1ULL))->name;
}

// Form `name` of `rules` written onto `text`, `argument` holding as many arguments as it takes, in the order of its
// parameters. That is its text cut at them. Where the ruleset gives it as a construct, each line of the construct is
// written the same way, and a scratch register comes from `scratch` the first time a writing names it. `broken` set
// where a scratch register cannot be taken
static void ruleset_opcode_text(const Ruleset *rules, VerbaLinea *text, unsigned int name, const char *const *argument,
                                const ScratchRegisters *scratch, int *broken)
{
    const Pseudo *const construct = &rules->constructs[name];
    PlatformArray taken;
    platform_array_begun(&taken, sizeof(RulesetScratchTaken), platform_place_g_program_owner, 1);
    if (construct->line_count == 0u)
    {
        const InstrTemplate *const form = &rules->forms[name];
        // an err is the operation being an error on this language: it writes nothing and breaks what it was written
        // into. A program that needs it is refused. It is not written with a hole in it. A form the ruleset does not
        // give is open (P10) and breaks the writing the same way. A nop writes nothing and the writing goes on
        if ((rules->form_given[name] == (unsigned char)RULESET_CORE_GIVEN_ERR) || (rules->form_given[name] == 0u))
        {
            *broken = 1;
            return;
        }
        // a scratch register the text names comes from `scratch` the first time the writing names it, in the order the
        // text names them, as the core counts them (ruleset_core_scratch)
        mmgr_line_put(text, form->pieces[0]);
        for (unsigned int at = 0u; at < form->placeholder_count; at += 1u)
        {
            const unsigned int placeholder = form->placeholders[at];
            if (RULESET_CORE_IS_SCRATCH(placeholder))
            {
                const char *const register_name =
                    ruleset_scratch_taken(&taken, placeholder, 0u, scratch, RULESET_CORE_SCRATCH_BANK(placeholder));
                *broken = *broken || (register_name == NULL);
                mmgr_line_put(text, (register_name != NULL) ? register_name : "");
            }
            else
            {
                mmgr_line_put(text, argument[placeholder]);
            }
            mmgr_line_put(text, form->pieces[at + 1u]);
        }
        platform_array_ended(&taken);
        return;
    }
    // the scratch registers this writing has taken, each by its bank and number
    for (unsigned int line_at = 0u; line_at < construct->line_count; line_at += 1u)
    {
        const PseudoLine *const line = &construct->lines[line_at];
        // every scratch register of the line taken first, then the arguments named, and the names do not move under
        // them
        for (unsigned int given_at = 0u; given_at < line->argument_count; given_at += 1u)
        {
            const PseudoOperand *const given = &line->arguments[given_at];
            if (given->type == PSEUDO_SCRATCH)
            {
                *broken = *broken || (ruleset_scratch_taken(&taken, given->placeholder, given->number, scratch,
                                                            given->placeholder) == NULL);
            }
        }
        const char **const arguments =
            (const char **)platform_cell_taken(((unsigned long long)line->argument_count + 1ULL) * sizeof(const char *),
                                               platform_place_g_program_owner, 1);
        if (arguments == NULL)
        {
            *broken = 1;
            break;
        }
        for (unsigned int given_at = 0u; given_at < line->argument_count; given_at += 1u)
        {
            const PseudoOperand *const given = &line->arguments[given_at];
            const char *const scratch_name =
                (given->type == PSEUDO_SCRATCH)
                    ? ruleset_scratch_taken(&taken, given->placeholder, given->number, NULL, given->placeholder)
                    : NULL;
            arguments[given_at] = (given->type == PSEUDO_TEXT)        ? given->text
                                  : (given->type == PSEUDO_PARAMETER) ? argument[given->placeholder]
                                  : (scratch_name != NULL)            ? scratch_name
                                                                      : "";
        }
        ruleset_opcode_text(rules, text, line->form, arguments, scratch, broken);
        platform_cell_released(arguments, platform_place_g_program_owner);
    }
    platform_array_ended(&taken);
}

void ruleset_write_taking(const Ruleset *rules, VerbaLinea *text, unsigned int name, const char *const *arguments,
                          unsigned int count, const ScratchRegisters *scratch, int *broken)
{
    if (count != rules->schema->forms[name].parameters)
    {
        *broken = 1;
        return;
    }
    ruleset_opcode_text(rules, text, name, arguments, scratch, broken);
}

unsigned int *ruleset_scratch(const Ruleset *rules, const unsigned int *banks)
{
    // arranged by the core (ruleset_core_scratch.h) the device runs as well
    const unsigned int forms = rules->schema->form_count;
    RulesetFlatConstructs flat;
    if (ruleset_flat_constructs(rules, &flat) == 0)
    {
        return NULL;
    }
    const unsigned long long arguments = (unsigned long long)flat.argument_count + 1ULL;
    unsigned int *const scratch = (unsigned int *)platform_cell_cleared((4ULL * forms) + 1ULL, sizeof(unsigned int),
                                                                        platform_place_g_program_owner, 0);
    unsigned char *const counted = (unsigned char *)platform_cell_cleared((unsigned long long)forms + 1ULL, 1ULL,
                                                                          platform_place_g_program_owner, 1);
    RulesetCoreFrame *const frames = (RulesetCoreFrame *)platform_cell_cleared(
        (unsigned long long)forms + 1ULL, sizeof(RulesetCoreFrame), platform_place_g_program_owner, 1);
    unsigned int *const taken_bank =
        (unsigned int *)platform_cell_cleared(arguments, sizeof(unsigned int), platform_place_g_program_owner, 1);
    unsigned int *const taken_number =
        (unsigned int *)platform_cell_cleared(arguments, sizeof(unsigned int), platform_place_g_program_owner, 1);
    const int ready =
        (scratch != NULL) && (counted != NULL) && (frames != NULL) && (taken_bank != NULL) && (taken_number != NULL);
    if (ready != 0)
    {
        RulesetCoreScratch arranged{};
        ruleset_flat_constructs_at(&flat, (unsigned char *)flat.cell, &arranged);
        arranged.form_count = forms;
        arranged.banks[0] = banks[0];
        arranged.banks[1] = banks[1];
        arranged.banks[2] = banks[2];
        arranged.scratch = scratch;
        arranged.counted = counted;
        arranged.frames = frames;
        arranged.taken_bank = taken_bank;
        arranged.taken_number = taken_number;
        ruleset_core_scratch(&arranged);
    }
    platform_cell_released(taken_number, platform_place_g_program_owner);
    platform_cell_released(taken_bank, platform_place_g_program_owner);
    platform_cell_released(frames, platform_place_g_program_owner);
    platform_cell_released(counted, platform_place_g_program_owner);
    ruleset_flat_constructs_ended(&flat);
    if (ready == 0)
    {
        platform_cell_released(scratch, platform_place_g_program_owner);
        return NULL;
    }
    return scratch;
}

// what a scratch register by its bank's name needs to answer one by the bank's place: the ruleset whose schema names
// the bank, and the scratch by name
struct RulesetNamedContext
{
    const RulesetSchema *schema;
    const RulesetNamedScratch *named;
};

static int ruleset_scratch_by_name(void *context, unsigned int bank, VerbaLinea *name)
{
    const RulesetNamedContext *const by_name = (const RulesetNamedContext *)context;
    return (by_name->named != NULL) &&
           (by_name->named->taken(by_name->named->context, by_name->schema->banks[bank].text, name) != 0);
}

// a form written by the name its .krs file gives it, for a reader outside the code generator (target.h), a construct's
// scratch taken of `scratch` by its bank's name
int ruleset_form_written(const Ruleset *rules, const char *name, const char *const *arguments, unsigned int count,
                         const RulesetNamedScratch *scratch, VerbaLinea *text)
{
    const RulesetSchema *const schema = rules->schema;
    const unsigned int named = ruleset_find(schema->forms, schema->form_count, name);
    if ((named == schema->form_count) || (count != schema->forms[named].parameters))
    {
        return 0;
    }
    int broken = 0;
    RulesetNamedContext context = {schema, scratch};
    const ScratchRegisters by_place = {ruleset_scratch_by_name, &context};
    ruleset_opcode_text(rules, text, named, arguments, &by_place, &broken);
    return (broken == 0) ? 1 : 0;
}

int ruleset_register_written(const Ruleset *rules, const char *bank, unsigned int number, VerbaLinea *text)
{
    const RulesetSchema *const schema = rules->schema;
    const unsigned int named = ruleset_find(schema->banks, schema->bank_count, bank);
    if (named == schema->bank_count)
    {
        return 0;
    }
    // a bank's written form takes one parameter, the register's number, in every placeholder it is cut at
    const InstrTemplate *const form = &rules->banks[named];
    mmgr_line_put(text, form->pieces[0]);
    for (unsigned int at = 0u; at < form->placeholder_count; at += 1u)
    {
        mmgr_line_decimal(text, number, 1u);
        mmgr_line_put(text, form->pieces[at + 1u]);
    }
    return 1;
}

const char *ruleset_fixed_named(const Ruleset *rules, const char *name)
{
    const RulesetSchema *const schema = rules->schema;
    const unsigned int named = ruleset_find(schema->fixed, schema->fixed_count, name);
    return (named == schema->fixed_count) ? NULL : rules->fixed[named];
}
