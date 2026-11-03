// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect.c: the inspections' entry points, each taking the inspections' lock and handing the text to the reader of
// its language; the facts' own writers; a finding written as the protocol's diagnostic; and a class's body written
// again with its methods sorted by name
#include "../text/json_tree.h"
#include "inspect_internal.h"

#include "../text/text.h"

const char *const g_cli_inspect_classes[CLI_INSPECT_CLASSES] = {
    "",           "t-keyword", "t-string",    "t-number",   "t-comment", "t-operator",   "t-delimiter",
    "t-function", "t-type",    "t-parameter", "t-property", "t-module",  "t-predefined", "t-attribute"};

const char *const g_cli_inspect_docstring_forms[4] = {"google", "numpy", "rest", "plain"};

const char *cli_inspect_language_get(const CliInspectCfg *args)
{
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    if ((dot == NULL) || (dot == name))
    {
        return NULL;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".py", .cap = CLI_CELLS_POOL}))
    {
        return "python";
    }
    return ((cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".js", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".mjs", .cap = CLI_CELLS_POOL})))
               ? "javascript"
               : NULL;
}

const char *cli_inspect_text(const CliInspectPoolCfg *args)
{
    return args->pool + args->text_inspect_text.at;
}

void cli_facts_begin(const CliInspectFactsCfg *args)
{
    CliFacts *facts = args->facts;
    *facts = (CliFacts){args->imports,
                        0u,
                        args->imports_max,
                        args->classes,
                        0u,
                        args->classes_max,
                        args->functions,
                        0u,
                        args->functions_max,
                        args->findings,
                        0u,
                        args->findings_max,
                        args->fixes,
                        0u,
                        args->fixes_max,
                        args->edits,
                        0u,
                        args->edits_max,
                        args->names,
                        0u,
                        args->names_max,
                        args->text_out,
                        0u,
                        args->text_max,
                        0,
                        0};
    if (args->text_max > 0u)
    {
        args->text_out[0] = '\0';
        facts->used = 1u;
    }
}

void cli_facts_counting(const CliInspectFactsLanguageCfg *args)
{
    CliFacts *facts = args->facts;
    // no capacity is ever full while counting: a count is an unsigned int's, which no text reaches
    *facts = (CliFacts){NULL, 0u, 0xffffffffu, NULL, 0u, 0xffffffffu, NULL, 0u, 0xffffffffu,
                        NULL, 0u, 0xffffffffu, NULL, 0u, 0xffffffffu, NULL, 0u, 0xffffffffu,
                        NULL, 0u, 0xffffffffu, NULL, 1u, 0xffffffffu, 0,    1};
}

CliInspectText cli_inspect_text_store(const CliInspectTextCtx *args)
{
    unsigned long long size = args->size;
    CliInspectText stored = {0u, 0u};
    if (args->facts->counting)
    {
        // a text's places are an unsigned int's, so a text past one's reach counts as one that fills
        const unsigned long long counted = (unsigned long long)args->facts->used + size + 1ULL;
        args->facts->used = (counted < 0xffffffffULL) ? (unsigned int)counted : 0xffffffffu;
        return stored;
    }
    if (args->facts->used + size + 1u > args->facts->text_max)
    {
        args->facts->full = 1;
        return stored;
    }
    // a pool contains at most 4 GiB, which its places count
    stored.at = args->facts->used;
    stored.size = (unsigned int)size;
    memor.cpy(
        &(MemoriaCfg){.dst = args->facts->text + args->facts->used, .src = args->text_text, .bytes = (size_t)size});
    args->facts->text[args->facts->used + size] = '\0';
    args->facts->used += (unsigned int)size + 1u;
    return stored;
}

CliInspectText cli_inspect_line_store(const CliInspectFactsLineVerbaLineaCtx *args)
{
    if (!mmgr_line_ok(args->line_verba_linea_pointer))
    {
        args->facts->full = 1;
        return (CliInspectText){0u, 0u};
    }
    return EMBED_CALL(cli_inspect_text_internal.text_store, CliInspectTextCtx, .facts = args->facts,
                      .text_text = args->line_verba_linea_pointer->out, .size = args->line_verba_linea_pointer->at);
}

void cli_inspect_finding_append(const CliInspectFromCtx *args)
{
    if (args->facts->findings == args->facts->findings_max)
    {
        args->facts->full = 1;
        return;
    }
    const CliInspectText stored =
        EMBED_CALL(cli_inspect_facts_line_verba_linea_internal.line_store, CliInspectFactsLineVerbaLineaCtx,
                   .facts = args->facts, .line_verba_linea_pointer = args->message);
    if (!args->facts->counting)
    {
        args->facts->finding[args->facts->findings] = (CliInspectFinding){
            args->from, args->to, args->severity, stored, args->code, args->href, args->facts->fixes, 0u};
    }
    args->facts->findings += 1u;
}

void cli_inspect_fix_append(const CliInspectFactsCtx *args)
{
    if ((args->facts->findings == 0u) || (args->facts->fixes == args->facts->fixes_max))
    {
        args->facts->full = 1;
        return;
    }
    const CliInspectFix fix = {EMBED_CALL(cli_inspect_facts_line_verba_linea_internal.line_store,
                                          CliInspectFactsLineVerbaLineaCtx, .facts = args->facts,
                                          .line_verba_linea_pointer = args->title),
                               args->facts->edits, 0u};
    if (!args->facts->counting)
    {
        args->facts->fix[args->facts->fixes] = fix;
        args->facts->finding[args->facts->findings - 1u].fixes += 1u;
    }
    args->facts->fixes += 1u;
}

void cli_inspect_fix_title(const CliInspectFactsTitleCtx *args)
{
    char text[CLI_NAME_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&line, args->title_text);
    EMBED_CALL(cli_inspect_facts_internal.fix_append, CliInspectFactsCtx, .facts = args->facts, .title = &line);
}

void cli_inspect_edit_append(const CliInspectTextCtx *args)
{
    if ((args->facts->fixes == 0u) || (args->facts->edits == args->facts->edits_max))
    {
        args->facts->full = 1;
        return;
    }
    const CliInspectEdit edit = {args->from, args->to, cli_inspect_text_internal.text_store(args)};
    if (!args->facts->counting)
    {
        args->facts->edit[args->facts->edits] = edit;
        args->facts->fix[args->facts->fixes - 1u].edits += 1u;
    }
    args->facts->edits += 1u;
}

unsigned int cli_inspect_name_append(const CliInspectSizeFromCtx *args)
{
    if (args->facts->names == args->facts->names_max)
    {
        args->facts->full = 1;
        return CLI_INSPECT_FULL;
    }
    const CliInspectText stored = EMBED_CALL(cli_inspect_text_internal.text_store, CliInspectTextCtx,
                                             .facts = args->facts, .text_text = args->name, .size = args->size);
    const CliInspectText alias_stored =
        (args->alias != NULL) ? EMBED_CALL(cli_inspect_text_internal.text_store, CliInspectTextCtx,
                                           .facts = args->facts, .text_text = args->alias, .size = args->alias_size)
                              : (CliInspectText){0u, 0u};
    if (!args->facts->counting)
    {
        args->facts->name[args->facts->names] =
            (CliInspectName){stored, alias_stored, args->alias != NULL, args->from, args->to};
    }
    args->facts->names += 1u;
    return args->facts->names - 1u;
}

int cli_inspect_facts(const CliInspectFactsLanguageCfg *args)
{
    const int python = (cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "python", .cap = CLI_CELLS_POOL}));
    if (!python &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "javascript", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    args->facts->imports = 0u;
    args->facts->classes = 0u;
    args->facts->functions = 0u;
    args->facts->findings = 0u;
    args->facts->fixes = 0u;
    args->facts->edits = 0u;
    args->facts->names = 0u;
    args->facts->used = ((args->facts->text_max > 0u) || args->facts->counting) ? 1u : 0u;
    args->facts->full = 0;
    if (!EMBED_CALL(cli_inspect_language_internal.read_state_reserve, CliInspectLanguageCtx, .language = args->language,
                    .text_text = args->text, .size = args->size))
    {
        args->facts->full = 1;
    }
    else if (python)
    {
        EMBED_CALL(cli_inspect_size_src_facts_internal.py_facts, CliInspectSizeSrcFactsCtx, .src = args->text,
                   .size = args->size, .facts = args->facts);
    }
    else
    {
        EMBED_CALL(cli_inspect_size_src_facts_internal.js_facts, CliInspectSizeSrcFactsCtx, .src = args->text,
                   .size = args->size, .facts = args->facts);
    }
    cli_inspect_read_state_free();
    return 1;
}

void cli_parsed_begin(const CliInspectParsedCfg *args)
{
    CliParsed *parsed = args->parsed;
    *parsed = (CliParsed){args->runs,
                          0u,
                          args->runs_max,
                          args->line_first,
                          args->line_runs,
                          0u,
                          args->lines_max,
                          args->fold_first,
                          args->fold_last,
                          0u,
                          args->folds_max,
                          args->ranges,
                          0u,
                          args->ranges_max,
                          0};
}

int cli_inspect_parse(const CliInspectLanguageTextCfg *args)
{
    const int python = (cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "python", .cap = CLI_CELLS_POOL}));
    if (!python &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "javascript", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    const int reserved = EMBED_CALL(cli_inspect_language_internal.read_state_reserve, CliInspectLanguageCtx,
                                    .language = args->language, .text_text = args->text, .size = args->size);
    if (reserved && python)
    {
        EMBED_CALL(cli_inspect_size_src_parsed_internal.py_parse, CliInspectSizeSrcParsedCtx, .src = args->text,
                   .size = args->size, .parsed = args->parsed);
    }
    else if (reserved)
    {
        EMBED_CALL(cli_inspect_size_src_parsed_internal.js_parse, CliInspectSizeSrcParsedCtx, .src = args->text,
                   .size = args->size, .parsed = args->parsed);
    }
    cli_inspect_read_state_free();
    return reserved;
}

int cli_inspect_docstring(const CliInspectLanguageCfg *args)
{
    const int python = (cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "python", .cap = CLI_CELLS_POOL}));
    if (!python &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "javascript", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(args->err_msg, "Write Docstring reads Python and JavaScript.");
        return 0;
    }
    const int reserved = EMBED_CALL(cli_inspect_language_internal.read_state_reserve, CliInspectLanguageCtx,
                                    .language = args->language, .text_text = args->source, .size = args->size);
    const int written =
        reserved &&
        (python
             ? EMBED_CALL(cli_inspect_size_src_internal.py_docstring, CliInspectSizeSrcCtx, .src = args->source,
                          .size = args->size, .line = args->line, .form = args->form, .edit = args->edit,
                          .text = args->text_verba_linea_pointer, .summary = args->summary, .err_msg = args->err_msg)
             : EMBED_CALL(cli_inspect_size_src_internal.js_docstring, CliInspectSizeSrcCtx, .src = args->source,
                          .size = args->size, .line = args->line, .edit = args->edit,
                          .text = args->text_verba_linea_pointer, .summary = args->summary, .err_msg = args->err_msg));
    cli_inspect_read_state_free();
    if (!reserved)
    {
        mmgr_line_put(args->err_msg, "a reading of the text could not be taken");
    }
    return written;
}

int cli_inspect_sort_methods(const CliInspectLanguageCfg *args)
{
    const int python = (cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "python", .cap = CLI_CELLS_POOL}));
    if (!python &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "javascript", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(args->err_msg, "Sort Methods by Name reads Python and JavaScript.");
        return 0;
    }
    const int reserved = EMBED_CALL(cli_inspect_language_internal.read_state_reserve, CliInspectLanguageCtx,
                                    .language = args->language, .text_text = args->source, .size = args->size);
    const int sorted =
        reserved &&
        (python ? EMBED_CALL(cli_inspect_size_src_internal.py_sort_methods, CliInspectSizeSrcCtx, .src = args->source,
                             .size = args->size, .line = args->line, .edit = args->edit,
                             .text = args->text_verba_linea_pointer, .err_msg = args->err_msg)
                : EMBED_CALL(cli_inspect_size_src_internal.js_sort_methods, CliInspectSizeSrcCtx, .src = args->source,
                             .size = args->size, .line = args->line, .edit = args->edit,
                             .text = args->text_verba_linea_pointer, .err_msg = args->err_msg));
    cli_inspect_read_state_free();
    if (!reserved)
    {
        mmgr_line_put(args->err_msg, "a reading of the text could not be taken");
    }
    return sorted;
}

typedef struct
{
    CliInspectPosition place;
    const char *col;
    VerbaLinea *out;
    const char *one;
    unsigned int one_size;
    const char *other;
    unsigned int other_size;
} InspectCtx;
// a place written as the protocol's `{"line", "character"}`, or as an edit's `{"line", "col"}`
EMBED_INLINE void inspect_position_write(const InspectCtx *args)
{
    mmgr_line_put(args->out, "{\"line\":");
    mmgr_line_decimal(args->out, args->place.line, 1u);
    mmgr_line_put(args->out, args->col);
    mmgr_line_decimal(args->out, args->place.col, 1u);
    mmgr_line_ch(args->out, '}');
}

void cli_inspect_finding_write(const CliInspectFactsFindingCfg *args)
{
    const CliInspectFinding *const found = &args->facts_read->finding[args->finding];
    mmgr_line_put(args->out, "{\"range\":{\"start\":");
    EMBED_CALL(inspect_position_write, InspectCtx, .place = found->from, .col = ",\"character\":", .out = args->out);
    mmgr_line_put(args->out, ",\"end\":");
    EMBED_CALL(inspect_position_write, InspectCtx, .place = found->to, .col = ",\"character\":", .out = args->out);
    mmgr_line_put(args->out, (found->severity == CLI_INSPECT_WARNING) ? "},\"severity\":2" : "},\"severity\":3");
    mmgr_line_put(args->out, ",\"message\":{\"kind\":\"markdown\",\"value\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->facts_read->text + found->message.at, .length = found->message.size);
    mmgr_line_put(args->out, "},\"source\":\"orior\",\"code\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = found->code,
               .length = cellul.len(&(CatenaFinitaCfg){.src = found->code, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"data\":{\"orior\":[");
    for (unsigned int fix = 0u; fix < found->fixes; fix += 1u)
    {
        const CliInspectFix *const one = &args->facts_read->fix[found->fix_first + fix];
        mmgr_line_put(args->out, (fix > 0u) ? ",{\"title\":" : "{\"title\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = args->facts_read->text + one->title.at, .length = one->title.size);
        mmgr_line_put(args->out, ",\"edits\":[");
        for (unsigned int edit = 0u; edit < one->edits; edit += 1u)
        {
            const CliInspectEdit *const change = &args->facts_read->edit[one->edit_first + edit];
            mmgr_line_put(args->out, (edit > 0u) ? ",{\"from\":" : "{\"from\":");
            EMBED_CALL(inspect_position_write, InspectCtx, .place = change->from, .col = ",\"col\":", .out = args->out);
            mmgr_line_put(args->out, ",\"to\":");
            EMBED_CALL(inspect_position_write, InspectCtx, .place = change->to, .col = ",\"col\":", .out = args->out);
            mmgr_line_put(args->out, ",\"text\":");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                       .text = args->facts_read->text + change->text.at, .length = change->text.size);
            mmgr_line_ch(args->out, '}');
        }
        mmgr_line_put(args->out, "]}");
    }
    mmgr_line_put(args->out, "]}");
    if (found->href != NULL)
    {
        mmgr_line_put(args->out, ",\"codeDescription\":{\"href\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = found->href,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = found->href, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, '}');
}

unsigned int cli_inspect_physical(const CliInspectSizeSrcStartsCtx *args)
{
    const char *src = args->src;
    unsigned int count = 0u;
    for (unsigned long long at = 0ULL; (at <= args->size) && (count < args->most);)
    {
        const char *const newline =
            (at < args->size)
                ? memor.chr(&(MemoriaCfg){.src = src + at, .val = '\n', .bytes = (size_t)(args->size - at)})
                : NULL;
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - src) : args->size;
        args->starts[count] = at;
        args->ends[count] = end;
        count += 1u;
        at = end + 1u;
    }
    return count;
}

// whether method `one` sorts before method `other`: by their names in lower case, then by their bytes
EMBED_INLINE int inspect_sorted_before(const InspectCtx *args)
{
    const char *other = args->other;
    const unsigned int min = (args->one_size < args->other_size) ? args->one_size : args->other_size;
    for (unsigned int at = 0u; at < min; at += 1u)
    {
        const char left =
            ((args->one[at] >= 'A') && (args->one[at] <= 'Z')) ? (char)(args->one[at] - 'A' + 'a') : args->one[at];
        const char right = ((other[at] >= 'A') && (other[at] <= 'Z')) ? (char)(other[at] - 'A' + 'a') : other[at];
        if (left != right)
        {
            return (unsigned char)left < (unsigned char)right;
        }
    }
    if (args->one_size != args->other_size)
    {
        return args->one_size < args->other_size;
    }
    const embed_iword order = memor.cmp(&(MemoriaCfg){.src = args->one, .other = other, .bytes = min});
    return order < 0;
}

// the methods of a body sorted, a list of the reading's cell, one for each token at the most
static unsigned int *s_methods;

void cli_inspect_sort_layout(const CliInspectPartitionCtx *args)
{
    s_methods =
        EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                   .bytes = (unsigned long long)CLI_INSPECT_TOKENS * sizeof(unsigned int));
}

int cli_inspect_sorted_body(const CliInspectSizeCtx *args)
{
    unsigned int methods = 0u;
    for (unsigned int at = 0u; (at < args->members) && (methods < CLI_INSPECT_TOKENS); at += 1u)
    {
        if (args->names[at] != NULL)
        {
            s_methods[methods] = at;
            methods += 1u;
        }
    }
    if (methods < 2u)
    {
        return 0;
    }
    int moved = 0;
    for (unsigned int at = 1u; at < methods; at += 1u)
    {
        const unsigned int pivot = s_methods[at];
        unsigned int place = at;
        while ((place > 0u) &&
               EMBED_CALL(inspect_sorted_before, InspectCtx, .one = args->names[pivot],
                          .one_size = args->name_sizes[pivot], .other = args->names[s_methods[place - 1u]],
                          .other_size = args->name_sizes[s_methods[place - 1u]]))
        {
            s_methods[place] = s_methods[place - 1u];
            place -= 1u;
            moved = 1;
        }
        s_methods[place] = pivot;
    }
    if (!moved)
    {
        return 0;
    }
    unsigned int next = 0u;
    unsigned int at_line = args->firsts[0];
    int first_line = 1;
    for (unsigned int member = 0u; member < args->members; member += 1u)
    {
        const unsigned int chosen = (args->names[member] != NULL) ? s_methods[next] : member;
        next += (args->names[member] != NULL) ? 1u : 0u;
        for (unsigned int line = at_line; line < args->firsts[member]; line += 1u)
        {
            mmgr_line_put(args->out, first_line ? "" : "\n");
            mmgr_line_put_n(args->out, args->src + args->line_starts[line],
                            args->line_ends[line] - args->line_starts[line]);
            first_line = 0;
        }
        for (unsigned int line = args->firsts[chosen]; line <= args->lasts[chosen]; line += 1u)
        {
            mmgr_line_put(args->out, first_line ? "" : "\n");
            mmgr_line_put_n(args->out, args->src + args->line_starts[line],
                            args->line_ends[line] - args->line_starts[line]);
            first_line = 0;
        }
        at_line = args->lasts[member] + 1u;
    }
    return 1;
}
