// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_flat.cu: the schema and a read ruleset flattened for the .krs reader's core, and what the core read kept
#include "ruleset_flat.h"

#include <stdio.h>
#include <string.h>

// the folder a part's files are read from: src/cu/transpiler/lstar/coherence in the tree this file was built from, where
// the part's machine file, its .kdm and its .ksc are
static std::string ruleset_part_folder(void)
{
    const std::string file = __FILE__;
    const size_t slash = file.find_last_of("/\\");
    const std::string here = (slash == std::string::npos) ? std::string() : file.substr(0u, slash + 1u);
    return here + "../../../transpiler/lstar/coherence";
}

// the file at `path` read whole onto the end of `text`: 1, or 0 where it could not be opened. Reading stops once the
// text reaches RULESET_FILE_MAX letters, which the caller refuses
static int ruleset_append(const std::string &path, std::string *text)
{
    FILE *const file = fopen(path.c_str(), "rb");
    if (file == NULL)
    {
        return 0;
    }
    char block[4096];
    size_t read = fread(block, 1u, sizeof(block), file);
    while ((read != 0u) && (text->size() < RULESET_FILE_MAX))
    {
        text->append(block, read);
        read = fread(block, 1u, sizeof(block), file);
    }
    fclose(file);
    return 1;
}

// the lines of a part's .kdm or of a .ksc a ruleset reads, each with its newline: those that open with bank, fixed,
// form, err or nop. The arrangements, the pipes, the questions and the comments are the file's own and are passed over
static std::string ruleset_lines_read(const std::string &kdm)
{
    std::string lines;
    size_t at = 0u;
    while (at < kdm.size())
    {
        const size_t end = (kdm.find('\n', at) == std::string::npos) ? kdm.size() : kdm.find('\n', at);
        const std::string line = kdm.substr(at, end - at);
        const std::string first = line.substr(0u, line.find_first_of(" \t\r"));
        if ((first == "bank") || (first == "fixed") || (first == "form") || (first == "err") || (first == "nop"))
        {
            lines += line + "\n";
        }
        at = end + 1u;
    }
    return lines;
}

// the part the ruleset at `path` names on its line `part <part>`, empty where it names none or cannot be opened
static std::string ruleset_part_named(const std::string &path)
{
    std::string text;
    if (ruleset_append(path, &text) == 0)
    {
        return std::string();
    }
    const std::string named = "part ";
    size_t at = 0u;
    while (at < text.size())
    {
        const size_t end = (text.find('\n', at) == std::string::npos) ? text.size() : text.find('\n', at);
        const std::string line = text.substr(at, end - at);
        if (line.compare(0u, named.size(), named) == 0)
        {
            return line.substr(named.size(), line.find_first_of(" \t\r", named.size()) - named.size());
        }
        at = end + 1u;
    }
    return std::string();
}

unsigned int ruleset_paths(const RulesetSchema *schema, const std::string &path, std::string paths[RULESET_PATHS_MAX])
{
    (void)schema;
    unsigned int count = 0u;
    paths[count] = path;
    count += 1u;
    const std::string named = ruleset_part_named(path);
    if (!named.empty())
    {
        const std::string part = ruleset_part_folder() + "/" + named;
        paths[count] = part + ".krs";
        paths[count + 1u] = part + ".kdm";
        count += 2u;
    }
    // the folds the .ksc beside the file holds as forms, where it has one
    const std::string ksc = path.substr(0u, path.find_last_of('.')) + ".ksc";
    FILE *const file = fopen(ksc.c_str(), "rb");
    if (file != NULL)
    {
        fclose(file);
        paths[count] = ksc;
        count += 1u;
    }
    return count;
}

int ruleset_text(const RulesetSchema *schema, const std::string &path, std::string *text, std::string *error)
{
    text->clear();
    std::string paths[RULESET_PATHS_MAX];
    const unsigned int count = ruleset_paths(schema, path, paths);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        std::string read;
        if (ruleset_append(paths[at], (at == 0u) ? text : &read) == 0)
        {
            *error = (at == 0u) ? std::string("it could not be opened")
                                : ("the part's " + paths[at] + " could not be opened");
            return 0;
        }
        if (at == 0u)
        {
            continue;
        }
        if ((!text->empty()) && (text->back() != '\n'))
        {
            text->push_back('\n');
        }
        // a part's .krs opens with krs 1 as every ruleset does, and the ruleset's own file has given that line; of a
        // .kdm and a .ksc only the ruleset lines are read
        const size_t opening = read.find('\n');
        const int krs = paths[at].compare(paths[at].size() - 4u, 4u, ".krs") == 0;
        text->append((krs == 0) ? ruleset_lines_read(read)
                                : ((opening == std::string::npos) ? std::string() : read.substr(opening + 1u)));
    }
    if (text->size() >= RULESET_FILE_MAX)
    {
        *error = "it is more than the reader holds";
        return 0;
    }
    return 1;
}

const char *ruleset_relation_said(unsigned int kind)
{
    static const char *const s_said[RULESET_CORE_RELATION_KINDS] = {
        "is given again after",
        "has the text of",
        "is under another name",
        "is, but for its modifiers and one word of its name,",
        "opens literal operands of",
        "writes a name no file gives",
        "gives another count of arguments than",
        "reaches itself through its line",
        "never writes its parameter",
        "writes a place its head does not take, at letter",
        "has nothing after its equals",
        "uses the numeric name of the register of",
        "is not named by the schema",
        "is named by the schema and given by no file",
        "takes another count of parameters than the schema gives",
        "is the arrangement of",
        "counts other than the questions written, which are",
        "answers with a class its file does not name",
    };
    return (kind < RULESET_CORE_RELATION_KINDS) ? s_said[kind] : "";
}

const char *ruleset_map_said(unsigned int kind)
{
    static const char *const s_said[RULESET_CORE_MAP_KINDS] = {
        "is one text in the first set and two in the second with",
        "is two texts in the first set and one in the second with",
        "is given as another kind in the second set:",
        "is given by no file of the second set",
    };
    return (kind < RULESET_CORE_MAP_KINDS) ? s_said[kind] : "";
}

int ruleset_relations_read(const std::string *paths, unsigned int count, const RulesetCoreSchema *schema,
                           RulesetRelationMemory *memory, RulesetCoreRelations *relations, std::string *error)
{
    *memory = RulesetRelationMemory{};
    unsigned int lines = count + 1u;
    for (unsigned int file = 0u; file < count; file += 1u)
    {
        std::string read;
        if (ruleset_append(paths[file], &read) == 0)
        {
            *error = paths[file];
            return 0;
        }
        // a set of files is held under RULESET_FILE_MAX letters, which 32 bits count
        memory->files.push_back(RulesetCoreSpan{(unsigned int)memory->text.size(), (unsigned int)read.size()});
        memory->text.insert(memory->text.end(), read.begin(), read.end());
        memory->labels.push_back(paths[file].substr(paths[file].find_last_of("/\\") + 1u));
        for (const char letter : read)
        {
            lines += (letter == '\n') ? 1u : 0u;
        }
    }
    const RulesetCoreSchema none = {NULL, NULL, NULL, 0u, NULL, 0u, NULL, 0u};
    relations->schema = (schema != NULL) ? *schema : none;
    // a vector's data is NULL where it is empty
    memory->text.push_back(0u);
    memory->files.push_back(RulesetCoreSpan{0u, 0u});
    memory->entries.assign(lines, RulesetCoreEntry{});
    memory->marks.assign(lines, 0u);
    memory->stack.assign(lines, 0u);
    memory->relations.assign((lines * RULESET_CORE_RELATION_KINDS) + relations->schema.form_count +
                                 relations->schema.bank_count + relations->schema.fixed_count,
                             RulesetCoreRelation{});
    relations->text = memory->text.data();
    relations->files = memory->files.data();
    relations->file_count = count;
    relations->entries = memory->entries.data();
    relations->entry_capacity = lines;
    relations->marks = memory->marks.data();
    relations->stack = memory->stack.data();
    relations->relations = memory->relations.data();
    relations->relation_capacity = (unsigned int)memory->relations.size();
    ruleset_core_relations(relations);
    return 1;
}

int ruleset_file(Ruleset *rules, const std::string &path, std::string *text)
{
    const RulesetSchema *const schema = rules->schema;
    rules->path = path;
    rules->banks.assign(schema->bank_count, InstrTemplate());
    rules->fixed.assign(schema->fixed_count, std::string());
    rules->forms.assign(schema->form_count, InstrTemplate());
    rules->constructs.assign(schema->form_count, Pseudo());
    rules->building = schema->form_count;
    rules->bank_given.assign(schema->bank_count, 0u);
    rules->fixed_given.assign(schema->fixed_count, 0u);
    rules->form_given.assign(schema->form_count, 0u);
    return ruleset_text(schema, path, text, &rules->error);
}

// `names` taken on into the flattened schema's letters, each name's span into `spans`
static void ruleset_flat_names(const RulesetName *names, unsigned int count, RulesetFlat *flat,
                               std::vector<RulesetCoreSpan> *spans)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        // a schema's names are a few words each, a few thousand letters in all
        const RulesetCoreSpan span = {(unsigned int)flat->letters.size(), (unsigned int)strlen(names[at].text)};
        flat->letters.insert(flat->letters.end(), names[at].text, names[at].text + span.length);
        spans->push_back(span);
    }
}

void ruleset_flat_schema(const RulesetSchema *schema, RulesetFlat *flat)
{
    *flat = RulesetFlat{};
    ruleset_flat_names(schema->forms, schema->form_count, flat, &flat->forms);
    ruleset_flat_names(schema->banks, schema->bank_count, flat, &flat->banks);
    ruleset_flat_names(schema->fixed, schema->fixed_count, flat, &flat->fixed);
    for (unsigned int form = 0u; form < schema->form_count; form += 1u)
    {
        flat->form_parameters.push_back(schema->forms[form].parameters);
    }
    // a vector's data is NULL where it is empty
    flat->letters.push_back(0u);
    flat->forms.push_back(RulesetCoreSpan{0u, 0u});
    flat->form_parameters.push_back(0u);
    flat->banks.push_back(RulesetCoreSpan{0u, 0u});
    flat->fixed.push_back(RulesetCoreSpan{0u, 0u});
    flat->schema.letters = flat->letters.data();
    flat->schema.forms = flat->forms.data();
    flat->schema.form_parameters = flat->form_parameters.data();
    flat->schema.form_count = schema->form_count;
    flat->schema.banks = flat->banks.data();
    flat->schema.bank_count = schema->bank_count;
    flat->schema.fixed = flat->fixed.data();
    flat->schema.fixed_count = schema->fixed_count;
}

void ruleset_capacities(unsigned int text_length, RulesetCoreRead *read)
{
    // every letter written is one of the file's or stands for an escape of two; every piece begins a text or follows a
    // slot, each at least three letters of the file, and every text is at least a line's; every word is at least a
    // letter and a space, and a head's parameters are its words
    // a form's fixed registers are folded into its text past the letters read, at most once each
    read->letter_capacity = (2u * text_length) + 2u;
    read->piece_capacity = (2u * text_length) + 2u;
    read->word_capacity = text_length + 2u;
}

void ruleset_memory_size(RulesetMemory *memory, RulesetCoreRead *read)
{
    const RulesetCoreSchema *const schema = &read->schema;
    // an empty vector's data is NULL, and every list is given at least one entry
    memory->letters.assign((size_t)read->letter_capacity, 0u);
    memory->pieces.assign((size_t)read->piece_capacity, RulesetCoreSpan{});
    memory->slots.assign((size_t)read->piece_capacity, 0u);
    memory->words.assign((size_t)read->word_capacity, RulesetCoreSpan{});
    memory->banks.assign((size_t)schema->bank_count + 1u, RulesetCoreTemplate{});
    memory->fixed.assign((size_t)schema->fixed_count + 1u, RulesetCoreSpan{});
    memory->forms.assign((size_t)schema->form_count + 1u, RulesetCoreTemplate{});
    memory->constructs.assign((size_t)schema->form_count + 1u, RulesetCoreConstruct{});
    memory->lines.assign((size_t)read->text_length + 1u, RulesetCoreLine{});
    memory->arguments.assign((size_t)read->text_length + 1u, RulesetCoreArgument{});
    memory->bank_given.assign((size_t)schema->bank_count + 1u, 0u);
    memory->fixed_given.assign((size_t)schema->fixed_count + 1u, 0u);
    memory->form_given.assign((size_t)schema->form_count + 1u, 0u);
    memory->building_parameters.assign((size_t)read->word_capacity, RulesetCoreSpan{});
    read->letters = memory->letters.data();
    read->pieces = memory->pieces.data();
    read->slots = memory->slots.data();
    read->words = memory->words.data();
    read->banks = memory->banks.data();
    read->fixed = memory->fixed.data();
    read->forms = memory->forms.data();
    read->constructs = memory->constructs.data();
    read->lines = memory->lines.data();
    read->arguments = memory->arguments.data();
    read->bank_given = memory->bank_given.data();
    read->fixed_given = memory->fixed_given.data();
    read->form_given = memory->form_given.data();
    read->building_parameters = memory->building_parameters.data();
}

// the file's span `span` as a string
static std::string ruleset_flat_text(const std::string &text, RulesetCoreSpan span)
{
    return text.substr(span.first, span.length);
}

// the template `form` of `read` as the host holds it
static InstrTemplate ruleset_flat_template(const RulesetCoreRead *read, const RulesetCoreTemplate *form)
{
    InstrTemplate kept;
    for (unsigned int piece = 0u; piece < form->piece_count; piece += 1u)
    {
        const RulesetCoreSpan span = read->pieces[form->piece_first + piece];
        kept.pieces.push_back(std::string((const char *)&read->letters[span.first], span.length));
    }
    for (unsigned int slot = 0u; (slot + 1u) < form->piece_count; slot += 1u)
    {
        kept.slots.push_back(read->slots[form->slot_first + slot]);
    }
    return kept;
}

// why `read` did not read the file, as the host's reader gives it
static std::string ruleset_flat_why(const RulesetCoreRead *read, const std::string &text, const RulesetSchema *schema)
{
    const std::string word = ruleset_flat_text(text, read->word);
    const std::string building = (read->at < schema->form_count) ? std::string(schema->forms[read->at].text) : "";
    switch (read->end)
    {
    case RULESET_CORE_NOT_KRS:
        return "it is not krs 1";
    case RULESET_CORE_NOT_ONE_WORD:
        return word + " is not one word given once";
    case RULESET_CORE_BANK_WRONG:
        return "the bank " + word + " is not one the code generator takes from, or is given twice or written wrong";
    case RULESET_CORE_REGISTER_WRONG:
        return "the register " + word + " is not one the code generator names, or is given twice or written wrong";
    case RULESET_CORE_FORM_WRONG:
        return "the form " + word +
               " is not one the code generator writes, takes other parameters, or is given twice or "
               "written wrong";
    case RULESET_CORE_CONSTRUCT_WRONG:
        return "the construct " + word +
               " is not a form the code generator writes, takes other parameters, or is given "
               "twice";
    case RULESET_CORE_NO_KIND:
        return "no entry is of the kind " + word;
    case RULESET_CORE_NO_LINES:
        return "the construct " + building + " has no lines";
    case RULESET_CORE_LINE_FORM:
        return "the construct " + building + " writes " + word +
               ", which the file has not given before it or which takes other arguments";
    case RULESET_CORE_SCRATCH_WORD:
        return "the construct " + building + " takes a scratch register " + word +
               " of no bank the ruleset gives, or with no number";
    case RULESET_CORE_NO_END:
        return "the construct " + building + " has no end";
    case RULESET_CORE_BANK_MISSING:
        return "the bank " + std::string(schema->banks[read->at].text) + " is not given";
    case RULESET_CORE_REGISTER_MISSING:
        return "the register " + std::string(schema->fixed[read->at].text) + " is not given";
    case RULESET_CORE_FORM_MISSING:
        return "the form " + building + " is not given";
    default:
        return "its ruleset, toolchain or header is not named";
    }
}

void ruleset_keep(const RulesetCoreRead *read, const std::string &text, Ruleset *rules)
{
    const RulesetSchema *const schema = rules->schema;
    for (unsigned int bank = 0u; bank < schema->bank_count; bank += 1u)
    {
        rules->banks[bank] = ruleset_flat_template(read, &read->banks[bank]);
        rules->bank_given[bank] = read->bank_given[bank];
    }
    for (unsigned int fixed = 0u; fixed < schema->fixed_count; fixed += 1u)
    {
        const RulesetCoreSpan span = read->fixed[fixed];
        rules->fixed[fixed] = std::string((const char *)&read->letters[span.first], span.length);
        rules->fixed_given[fixed] = read->fixed_given[fixed];
    }
    for (unsigned int form = 0u; form < schema->form_count; form += 1u)
    {
        rules->forms[form] = ruleset_flat_template(read, &read->forms[form]);
        rules->form_given[form] = read->form_given[form];
        const RulesetCoreConstruct *const construct = &read->constructs[form];
        rules->constructs[form].lines.clear();
        for (unsigned int line = 0u; line < construct->line_count; line += 1u)
        {
            const RulesetCoreLine *const read_line = &read->lines[construct->line_first + line];
            PseudoLine kept;
            kept.form = read_line->form;
            for (unsigned int given = 0u; given < read_line->argument_count; given += 1u)
            {
                const RulesetCoreArgument *const argument = &read->arguments[read_line->argument_first + given];
                const PseudoOperand operand = {(PseudoOperandKind)argument->kind, argument->slot, argument->number,
                                               ruleset_flat_text(text, argument->text)};
                kept.arguments.push_back(operand);
            }
            rules->constructs[form].lines.push_back(kept);
        }
    }
    rules->name = ruleset_flat_text(text, read->name);
    rules->toolchain = ruleset_flat_text(text, read->toolchain);
    rules->header = ruleset_flat_text(text, read->header);
    rules->shared = ruleset_flat_text(text, read->shared);
    rules->write_ports = ruleset_flat_text(text, read->write_ports);
    rules->part = ruleset_flat_text(text, read->part);
    rules->building = read->building;
    rules->building_parameters.clear();
    for (unsigned int parameter = 0u; parameter < read->building_parameter_count; parameter += 1u)
    {
        rules->building_parameters.push_back(ruleset_flat_text(text, read->building_parameters[parameter]));
    }
    rules->error.clear();
    if (read->end != RULESET_CORE_OK)
    {
        char where[32];
        snprintf(where, sizeof(where), "line %u: ", read->line);
        rules->error =
            ((read->line != 0u) ? std::string(where) : std::string()) + ruleset_flat_why(read, text, schema);
    }
}

void ruleset_flat_constructs(const Ruleset *rules, RulesetFlatConstructs *flat)
{
    *flat = RulesetFlatConstructs{};
    for (const Pseudo &construct : rules->constructs)
    {
        // a ruleset's lines and arguments are a few thousand
        const RulesetCoreConstruct flattened = {(unsigned int)flat->lines.size(), (unsigned int)construct.lines.size()};
        flat->constructs.push_back(flattened);
        for (const PseudoLine &line : construct.lines)
        {
            const RulesetCoreLine flat_line = {line.form, (unsigned int)flat->arguments.size(),
                                               (unsigned int)line.arguments.size()};
            flat->lines.push_back(flat_line);
            for (const PseudoOperand &given : line.arguments)
            {
                const RulesetCoreArgument argument = {(unsigned int)given.kind, given.slot, given.number,
                                                      RulesetCoreSpan{0u, 0u}};
                flat->arguments.push_back(argument);
            }
        }
    }
    // a vector's data is NULL where it is empty
    flat->constructs.push_back(RulesetCoreConstruct{0u, 0u});
    flat->lines.push_back(RulesetCoreLine{0u, 0u, 0u});
    flat->arguments.push_back(RulesetCoreArgument{0u, 0u, 0u, RulesetCoreSpan{0u, 0u}});
}
