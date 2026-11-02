// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_facts.c: what one Python file shows alone, and its facts for the tree's checks: the imports it makes as it
// runs, those outside every function and class and outside a block that runs only for a type checker; and the
// functions it declares, methods among them, with the calls in each outside the functions it contains: a name before a
// bracket, after no dot or after `self.` or `cls.`
#include "inspect_py.h"

void cli_py_functions(const CliInspectPySrcSizeCfg *args)
{
    CliFacts *facts = args->facts;
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        if (!EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = line,
                        .word = "def") ||
            !EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = args->src, .line = line))
        {
            continue;
        }
        unsigned int name_at = line->count;
        for (unsigned int offset = 0u; (name_at == line->count) && (offset < line->count); offset += 1u)
        {
            name_at = EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                                 .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = offset),
                                 .word = "def")
                          ? (offset + 1u)
                          : name_at;
        }
        if (name_at >= line->count)
        {
            continue;
        }
        const unsigned int end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at);
        const unsigned int call_first = facts->names;
        unsigned int nested_end = 0u;
        for (unsigned int index = at + 1u; index < end; index += 1u)
        {
            const CliPyLine *const inner = &g_cli_py_line[index];
            if (index < nested_end)
            {
                continue;
            }
            if (EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = inner,
                           .word = "def") &&
                EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = args->src, .line = inner))
            {
                nested_end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = index);
                continue;
            }
            for (unsigned int offset = 0u; offset + 1u < inner->count; offset += 1u)
            {
                const CliInspectTok *const tok =
                    EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = inner, .at = offset);
                if ((tok->kind != CLI_PY_NAME) ||
                    !EMBED_CALL(
                        cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                        .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = inner, .at = offset + 1u),
                        .word = "(") ||
                    EMBED_CALL(cli_inspect_py_src_tok.py_keyword, CliInspectPySrcTokCfg, .src = args->src, .tok = tok))
                {
                    continue;
                }
                const CliInspectTok *const before = (offset >= 1u) ? EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg,
                                                                                .line = inner, .at = offset - 1u)
                                                                   : NULL;
                const CliInspectTok *const owner = (offset >= 2u) ? EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg,
                                                                               .line = inner, .at = offset - 2u)
                                                                  : NULL;
                const int dotted = (before != NULL) && EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                                  .src = args->src, .tok = before, .word = ".");
                const int own = (owner != NULL) && (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                               .src = args->src, .tok = owner, .word = "self") ||
                                                    EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                               .src = args->src, .tok = owner, .word = "cls"));
                const int declared =
                    (before != NULL) && (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                    .src = args->src, .tok = before, .word = "def") ||
                                         EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                    .src = args->src, .tok = before, .word = "class"));
                if ((dotted && !own) || declared)
                {
                    continue;
                }
                (void)EMBED_CALL(cli_inspect_size_from_internal.name_append, CliInspectSizeFromCtx, .facts = facts,
                                 .name = args->src + tok->start, .size = tok->end - tok->start, .alias = NULL,
                                 .alias_size = 0ull, .from = tok->from, .to = tok->to);
            }
        }
        const CliInspectTok *const name =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = name_at);
        const CliPyLine *const last = &g_cli_py_line[end - 1u];
        if (facts->functions == facts->functions_max)
        {
            facts->full = 1;
            return;
        }
        const CliInspectFunction function = {
            EMBED_CALL(cli_inspect_text_internal.text_store, CliInspectTextCtx, .facts = facts,
                       .text_text = args->src + name->start, .size = name->end - name->start),
            name->from,
            name->to,
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u)->from.line,
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = last, .at = last->count - 1u)->to.line,
            call_first,
            facts->names - call_first};
        if (!facts->counting)
        {
            facts->function[facts->functions] = function;
        }
        facts->functions += 1u;
    }
}

typedef struct
{
    CliFacts *facts;
    const VerbaLinea *spec;
    unsigned int name_first;
    const CliInspectTok *alias;
    const char *src;
    const CliPyLine *line;
} InspectPyFactsCtx;
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
                        .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = at), .word = ","))
        {
            continue;
        }
        char spec[CLI_PATH_MAX_LEN];
        VerbaLinea dotted = {spec, sizeof(spec), 0ull};
        const CliInspectTok *alias = NULL;
        for (unsigned int part = start; part < at; part += 1u)
        {
            const CliInspectTok *const tok =
                EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = part);
            if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "as"))
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
                               .word = "import")
                        ? at
                        : import_at;
    }
    if (import_at == line->count)
    {
        return;
    }
    char spec[CLI_PATH_MAX_LEN];
    VerbaLinea dotted = {spec, sizeof(spec), 0ull};
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
                        .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = at), .word = ","))
        {
            continue;
        }
        const CliInspectTok *name = NULL;
        const CliInspectTok *alias = NULL;
        for (unsigned int part = start; part < at; part += 1u)
        {
            const CliInspectTok *const tok =
                EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = part);
            if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = "(") ||
                EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word = ")"))
            {
                continue;
            }
            if (name == NULL)
            {
                name = tok;
            }
            else if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok,
                                .word = "as") &&
                     (alias == NULL))
            {
                for (unsigned int after = part + 1u; (alias == NULL) && (after < at); after += 1u)
                {
                    const CliInspectTok *const next =
                        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = after);
                    alias = (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = next,
                                        .word = "(") ||
                             EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = next,
                                        .word = ")"))
                                ? NULL
                                : next;
                }
                break;
            }
        }
        if ((name != NULL) &&
            !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = name, .word = "*"))
        {
            (void)EMBED_CALL(cli_inspect_size_from_internal.name_append, CliInspectSizeFromCtx, .facts = facts,
                             .name = src + name->start, .size = name->end - name->start,
                             .alias = (alias != NULL) ? (src + alias->start) : NULL,
                             .alias_size = (alias != NULL) ? (alias->end - alias->start) : 0ull, .from = name->from,
                             .to = name->to);
        }
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
                                        .line = line, .word = "def") ||
                             EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src,
                                        .line = line, .word = "class");
        const int checking =
            EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = line,
                       .word = "if") &&
            EMBED_CALL(cli_inspect_py_src_word.py_contains, CliInspectPySrcWordCfg, .src = args->src,
                       .toks = &g_cli_inspect_tok[line->first], .count = line->count, .word = "TYPE_CHECKING");
        if ((declares || checking) &&
            EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = args->src, .line = line))
        {
            skip_to = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at);
            continue;
        }
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u),
                       .word = "import"))
        {
            EMBED_CALL(facts_plain_import, InspectPyFactsCtx, .src = args->src, .line = line, .facts = args->facts);
        }
        else if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                            .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u),
                            .word = "from"))
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
                       .word = "def") &&
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
