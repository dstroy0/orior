// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_facts.c: what one Python file shows alone, and its facts for the tree's checks: the imports it makes as it
// runs, those outside every function and class and outside a block that runs only for a type checker; and the
// functions it declares, methods among them, with the calls in each outside the functions it contains: a name before a
// bracket, after no dot or after `self.` or `cls.`
#include "inspect_py.h"

typedef struct
{
    CliFacts *facts;
    const VerbaLinea *spec;
    unsigned int name_first;
    const CliInspectTok *alias;
    const char *src;
    const CliPyLine *line;
    unsigned int at;
    unsigned int end;
    unsigned int first;
    const CliInspectTok *tok;
} InspectPyFactsCtx;

// the place of the name a `def` line declares, after its `def`; the line's count of tokens where it gives none
EMBED_INLINE unsigned int facts_def_name(const InspectPyFactsCtx *args)
{
    const CliPyLine *const line = args->line;
    for (unsigned int offset = 0u; offset < line->count; offset += 1u)
    {
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = offset),
                       .word_text = "def"))
        {
            return offset + 1u;
        }
    }
    return line->count;
}

// the token at `at` of `line` put among the facts' names where it is a call: a name, no keyword, before a bracket,
// after no dot or after `self.` or `cls.`, and not what a `def` or a `class` declares
EMBED_INLINE void facts_call_put(const InspectPyFactsCtx *args)
{
    const char *const src = args->src;
    const CliPyLine *const inner = args->line;
    const unsigned int offset = args->at;
    const CliInspectTok *const tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = inner, .at = offset);
    if ((tok->type != CLI_PY_NAME) ||
        !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                    .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = inner, .at = offset + 1u),
                    .word_text = "(") ||
        EMBED_CALL(cli_inspect_py_src_tok.py_keyword, CliInspectPySrcTokCfg, .src = src, .tok = tok))
    {
        return;
    }
    const CliInspectTok *const before =
        (offset >= 1u) ? EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = inner, .at = offset - 1u) : NULL;
    const CliInspectTok *const owner =
        (offset >= 2u) ? EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = inner, .at = offset - 2u) : NULL;
    const int dotted = (before != NULL) && EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                                      .tok = before, .word_text = ".");
    const int own = (owner != NULL) && (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                                   .tok = owner, .word_text = "self") ||
                                        EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                                                   .tok = owner, .word_text = "cls"));
    const int declared = (before != NULL) && (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                         .src = src, .tok = before, .word_text = "def") ||
                                              EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                         .src = src, .tok = before, .word_text = "class"));
    if ((dotted && !own) || declared)
    {
        return;
    }
    (void)EMBED_CALL(cli_inspect_size_from_internal.name_append, CliInspectSizeFromCtx, .facts = args->facts,
                     .name = src + tok->start, .size = tok->end - tok->start, .alias = NULL, .alias_size = 0ULL,
                     .from = tok->from, .to = tok->to);
}

// the calls on the lines after the `def` line at `at` up to `end` put among the facts' names, the functions declared
// inside passed over
EMBED_INLINE void facts_calls_put(const InspectPyFactsCtx *args)
{
    unsigned int nested_end = 0u;
    for (unsigned int index = args->at + 1u; index < args->end; index += 1u)
    {
        const CliPyLine *const inner = &g_cli_py_line[index];
        if (index < nested_end)
        {
            continue;
        }
        if (EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = inner,
                       .word_text = "def") &&
            EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = args->src, .line = inner))
        {
            nested_end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = index);
            continue;
        }
        for (unsigned int offset = 0u; offset + 1u < inner->count; offset += 1u)
        {
            EMBED_CALL(facts_call_put, InspectPyFactsCtx, .facts = args->facts, .src = args->src, .line = inner,
                       .at = offset);
        }
    }
}

// the function the `def` line `line` declares put on the facts: its name at `at`, its lines to `end`, and its calls
// from `first` among the facts' names; 1, or 0 with the facts marked full where no more fit
EMBED_INLINE int facts_function_put(const InspectPyFactsCtx *args)
{
    CliFacts *const facts = args->facts;
    const CliPyLine *const line = args->line;
    const CliInspectTok *const name = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = args->at);
    const CliPyLine *const last = &g_cli_py_line[args->end - 1u];
    if (facts->functions == facts->functions_max)
    {
        facts->full = 1;
        return 0;
    }
    const CliInspectFunction function = {
        EMBED_CALL(cli_inspect_text_internal.text_store, CliInspectTextCtx, .facts = facts,
                   .text_text = args->src + name->start, .size = name->end - name->start),
        name->from,
        name->to,
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u)->from.line,
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = last, .at = last->count - 1u)->to.line,
        args->first,
        facts->names - args->first};
    if (!facts->counting)
    {
        facts->function[facts->functions] = function;
    }
    facts->functions += 1u;
    return 1;
}

void cli_py_functions(const CliInspectPySrcSizeCfg *args)
{
    CliFacts *facts = args->facts;
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        if (!EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = line,
                        .word_text = "def") ||
            !EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = args->src, .line = line))
        {
            continue;
        }
        const unsigned int name_at = EMBED_CALL(facts_def_name, InspectPyFactsCtx, .src = args->src, .line = line);
        if (name_at >= line->count)
        {
            continue;
        }
        const unsigned int end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at);
        const unsigned int call_first = facts->names;
        EMBED_CALL(facts_calls_put, InspectPyFactsCtx, .facts = facts, .src = args->src, .at = at, .end = end);
        if (!EMBED_CALL(facts_function_put, InspectPyFactsCtx, .facts = facts, .src = args->src, .line = line,
                        .at = name_at, .end = end, .first = call_first))
        {
            return;
        }
    }
}
// an import put on the facts: its spec, its names from `name_first`, its alias, and the span of its line
EMBED_INLINE void facts_import(const InspectPyFactsCtx *args)
{
    CliFacts *facts = args->facts;
    const CliPyLine *line = args->line;
    if (facts->imports == facts->imports_max)
    {
        facts->full = 1;
        return;
    }
    const CliInspectText alias_stored =
        (args->alias != NULL)
            ? EMBED_CALL(cli_inspect_text_internal.text_store, CliInspectTextCtx, .facts = facts,
                         .text_text = args->src + args->alias->start, .size = args->alias->end - args->alias->start)
            : (CliInspectText){0u, 0u};
    const CliInspectImport import = {
        EMBED_CALL(cli_inspect_facts_line_verba_linea_internal.line_store, CliInspectFactsLineVerbaLineaCtx,
                   .facts = facts, .line_verba_linea_pointer = args->spec),
        args->name_first,
        facts->names - args->name_first,
        alias_stored,
        args->alias != NULL,
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u)->from,
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = line->count - 1u)->to};
    if (!facts->counting)
    {
        facts->import[facts->imports] = import;
    }
    facts->imports += 1u;
}

// an `import a.b as c, d` line's imports put on the facts
EMBED_INLINE void facts_plain_import(const InspectPyFactsCtx *args)
{
    const char *src = args->src;
    const CliPyLine *line = args->line;
    unsigned int start = 1u;
    for (unsigned int at = 1u; at <= line->count; at += 1u)
    {
        if ((at < line->count) &&
            !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                        .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = at),
                        .word_text = ","))
        {
            continue;
        }
        char spec[CLI_PATH_MAX_LEN];
        VerbaLinea dotted = {spec, sizeof(spec), 0ULL};
        const CliInspectTok *alias = NULL;
        for (unsigned int part = start; part < at; part += 1u)
        {
            const CliInspectTok *const tok =
                EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = part);
            if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                           .word_text = "as"))
            {
                alias = (part + 1u < at)
                            ? EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = part + 1u)
                            : NULL;
                break;
            }
            mmgr_line_put_n(&dotted, src + tok->start, tok->end - tok->start);
        }
        if (dotted.at > 0u)
        {
            EMBED_CALL(facts_import, InspectPyFactsCtx, .facts = args->facts, .spec = &dotted,
                       .name_first = args->facts->names, .alias = alias, .src = src, .line = line);
        }
        start = at + 1u;
    }
}

// whether the token `tok` is a bracket
EMBED_INLINE int facts_bracket(const InspectPyFactsCtx *args)
{
    return EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src, .tok = args->tok,
                      .word_text = "(") ||
           EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src, .tok = args->tok,
                      .word_text = ")");
}

// the first token of `line` from `first` up to `at` that is no bracket; NULL where each is one
EMBED_INLINE const CliInspectTok *facts_unbracketed(const InspectPyFactsCtx *args)
{
    for (unsigned int after = args->first; after < args->at; after += 1u)
    {
        const CliInspectTok *const next =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->line, .at = after);
        if (!EMBED_CALL(facts_bracket, InspectPyFactsCtx, .src = args->src, .tok = next))
        {
            return next;
        }
    }
    return NULL;
}

// the name one entry of a `from` import's list brings in, the tokens of `line` from `first` up to `at`, put among the
// facts' names with its alias, the brackets passed over; nothing for a `*`
EMBED_INLINE void facts_from_name_put(const InspectPyFactsCtx *args)
{
    const char *const src = args->src;
    const CliInspectTok *name = NULL;
    const CliInspectTok *alias = NULL;
    unsigned int part = args->first;
    int going = 1;
    while (going && (part < args->at))
    {
        const CliInspectTok *const tok =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->line, .at = part);
        const int bracket = EMBED_CALL(facts_bracket, InspectPyFactsCtx, .src = src, .tok = tok);
        if (!bracket && (name == NULL))
        {
            name = tok;
        }
        else if (!bracket && EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                                        .word_text = "as"))
        {
            alias = EMBED_CALL(facts_unbracketed, InspectPyFactsCtx, .src = src, .line = args->line, .first = part + 1u,
                               .at = args->at);
            going = 0;
        }
        part += 1u;
    }
    if ((name != NULL) &&
        !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = name, .word_text = "*"))
    {
        (void)EMBED_CALL(cli_inspect_size_from_internal.name_append, CliInspectSizeFromCtx, .facts = args->facts,
                         .name = src + name->start, .size = name->end - name->start,
                         .alias = (alias != NULL) ? (src + alias->start) : NULL,
                         .alias_size = (alias != NULL) ? (alias->end - alias->start) : 0ULL, .from = name->from,
                         .to = name->to);
    }
}

// a `from a import b as c, (d)` line's import put on the facts
EMBED_INLINE void facts_from_import(const InspectPyFactsCtx *args)
{
    const char *src = args->src;
    const CliPyLine *line = args->line;
    CliFacts *facts = args->facts;
    unsigned int import_at = line->count;
    for (unsigned int at = 0u; (import_at == line->count) && (at < line->count); at += 1u)
    {
        import_at = EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                               .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = at),
                               .word_text = "import")
                        ? at
                        : import_at;
    }
    if (import_at == line->count)
    {
        return;
    }
    char spec[CLI_PATH_MAX_LEN];
    VerbaLinea dotted = {spec, sizeof(spec), 0ULL};
    for (unsigned int at = 1u; at < import_at; at += 1u)
    {
        const CliInspectTok *const tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = at);
        mmgr_line_put_n(&dotted, src + tok->start, tok->end - tok->start);
    }
    const unsigned int name_first = facts->names;
    unsigned int start = import_at + 1u;
    for (unsigned int at = start; at <= line->count; at += 1u)
    {
        if ((at < line->count) &&
            !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                        .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = at),
                        .word_text = ","))
        {
            continue;
        }
        EMBED_CALL(facts_from_name_put, InspectPyFactsCtx, .facts = facts, .src = src, .line = line, .first = start,
                   .at = at);
        start = at + 1u;
    }
    EMBED_CALL(facts_import, InspectPyFactsCtx, .facts = facts, .spec = &dotted, .name_first = name_first,
               .alias = NULL, .src = src, .line = line);
}

void cli_py_imports(const CliInspectPySrcSizeCfg *args)
{
    unsigned int skip_to = 0u;
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        if (at < skip_to)
        {
            continue;
        }
        const CliPyLine *const line = &g_cli_py_line[at];
        const int declares = EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src,
                                        .line = line, .word_text = "def") ||
                             EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src,
                                        .line = line, .word_text = "class");
        const int checking =
            EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = line,
                       .word_text = "if") &&
            EMBED_CALL(cli_inspect_py_src_word.py_contains, CliInspectPySrcWordCfg, .src = args->src,
                       .toks = &g_cli_inspect_tok[line->first], .count = line->count, .word_text = "TYPE_CHECKING");
        if ((declares || checking) &&
            EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = args->src, .line = line))
        {
            skip_to = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at);
            continue;
        }
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u),
                       .word_text = "import"))
        {
            EMBED_CALL(facts_plain_import, InspectPyFactsCtx, .src = args->src, .line = line, .facts = args->facts);
        }
        else if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                            .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u),
                            .word_text = "from"))
        {
            EMBED_CALL(facts_from_import, InspectPyFactsCtx, .src = args->src, .line = line, .facts = args->facts);
        }
    }
}

void cli_inspect_py_facts(const CliInspectSizeSrcFactsCtx *args)
{
    EMBED_CALL(cli_inspect_py_src_size.py_read, CliInspectPySrcSizeCfg, .src = args->src, .size = args->size);
    EMBED_CALL(cli_inspect_py_src_size.py_unreachable, CliInspectPySrcSizeCfg, .src = args->src, .size = args->size,
               .facts = args->facts);
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        if (EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = line,
                       .word_text = "def") &&
            EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = args->src, .line = line))
        {
            EMBED_CALL(cli_inspect_py_src_size.py_unread, CliInspectPySrcSizeCfg, .src = args->src, .size = args->size,
                       .at = at, .facts = args->facts);
        }
    }
    EMBED_CALL(cli_inspect_py_src_size.py_docstrings, CliInspectPySrcSizeCfg, .src = args->src, .size = args->size,
               .facts = args->facts);
    EMBED_CALL(cli_inspect_py_src_size.py_imports, CliInspectPySrcSizeCfg, .src = args->src, .facts = args->facts);
    EMBED_CALL(cli_inspect_py_src_size.py_classes, CliInspectPySrcSizeCfg, .src = args->src, .facts = args->facts);
    EMBED_CALL(cli_inspect_py_src_size.py_functions, CliInspectPySrcSizeCfg, .src = args->src, .facts = args->facts);
    args->facts->full = args->facts->full || g_cli_inspect_over;
}
