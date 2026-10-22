// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_facts.c: what one Python file says alone, and its facts for the tree's checks: the imports it makes as it
// runs, those outside every function and class and outside a block that runs only for a type checker; and the
// functions it declares, methods among them, with the calls in each outside the functions it holds: a name before a
// bracket, after no dot or after `self.` or `cls.`
#include "inspect_py.h"

void cli_py_functions(const char *src, CliFacts *facts)
{
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        if (!cli_py_head_is(src, line, "def") || !cli_py_opens(src, line))
        {
            continue;
        }
        unsigned int name_at = line->count;
        for (unsigned int offset = 0u; (name_at == line->count) && (offset < line->count); offset += 1u)
        {
            name_at = cli_py_is(src, cli_py_tok(line, offset), "def") ? (offset + 1u) : name_at;
        }
        if (name_at >= line->count)
        {
            continue;
        }
        const unsigned int end = cli_py_block_end(at);
        const unsigned int call_first = facts->names;
        unsigned int nested_end = 0u;
        for (unsigned int index = at + 1u; index < end; index += 1u)
        {
            const CliPyLine *const inner = &g_cli_py_line[index];
            if (index < nested_end)
            {
                continue;
            }
            if (cli_py_head_is(src, inner, "def") && cli_py_opens(src, inner))
            {
                nested_end = cli_py_block_end(index);
                continue;
            }
            for (unsigned int offset = 0u; offset + 1u < inner->count; offset += 1u)
            {
                const CliInspectTok *const tok = cli_py_tok(inner, offset);
                if ((tok->kind != CLI_PY_NAME) || !cli_py_is(src, cli_py_tok(inner, offset + 1u), "(") ||
                    cli_py_keyword(src, tok))
                {
                    continue;
                }
                const CliInspectTok *const before = (offset >= 1u) ? cli_py_tok(inner, offset - 1u) : NULL;
                const CliInspectTok *const owner = (offset >= 2u) ? cli_py_tok(inner, offset - 2u) : NULL;
                const int dotted = (before != NULL) && cli_py_is(src, before, ".");
                const int own = (owner != NULL) && (cli_py_is(src, owner, "self") || cli_py_is(src, owner, "cls"));
                const int declared =
                    (before != NULL) && (cli_py_is(src, before, "def") || cli_py_is(src, before, "class"));
                if ((dotted && !own) || declared)
                {
                    continue;
                }
                (void)cli_inspect_name_put(facts, src + tok->start, tok->end - tok->start, NULL, 0ull, tok->from,
                                           tok->to);
            }
        }
        const CliInspectTok *const name = cli_py_tok(line, name_at);
        const CliPyLine *const last = &g_cli_py_line[end - 1u];
        if (facts->functions == facts->functions_most)
        {
            facts->full = 1;
            return;
        }
        const CliInspectFunction function = {cli_inspect_kept(facts, src + name->start, name->end - name->start),
                                             name->from,
                                             name->to,
                                             cli_py_tok(line, 0u)->from.line,
                                             cli_py_tok(last, last->count - 1u)->to.line,
                                             call_first,
                                             facts->names - call_first};
        if (!facts->counting)
        {
            facts->function[facts->functions] = function;
        }
        facts->functions += 1u;
    }
}

// an import put on the facts: its spec, its names from `name_first`, its alias, and the span of its line
static void facts_import(CliFacts *facts, const CliLine *spec, unsigned int name_first, const CliInspectTok *alias,
                         const char *src, const CliPyLine *line)
{
    if (facts->imports == facts->imports_most)
    {
        facts->full = 1;
        return;
    }
    const CliInspectText kept_alias = (alias != NULL)
                                          ? cli_inspect_kept(facts, src + alias->start, alias->end - alias->start)
                                          : (CliInspectText){0u, 0u};
    const CliInspectImport import = {cli_inspect_line_kept(facts, spec),
                                     name_first,
                                     facts->names - name_first,
                                     kept_alias,
                                     alias != NULL,
                                     cli_py_tok(line, 0u)->from,
                                     cli_py_tok(line, line->count - 1u)->to};
    if (!facts->counting)
    {
        facts->import[facts->imports] = import;
    }
    facts->imports += 1u;
}

// an `import a.b as c, d` line's imports put on the facts
static void facts_plain_import(const char *src, const CliPyLine *line, CliFacts *facts)
{
    unsigned int start = 1u;
    for (unsigned int at = 1u; at <= line->count; at += 1u)
    {
        if ((at < line->count) && !cli_py_is(src, cli_py_tok(line, at), ","))
        {
            continue;
        }
        char spec[CLI_PATH_LONGEST];
        CliLine dotted = {spec, sizeof(spec), 0ull};
        const CliInspectTok *alias = NULL;
        for (unsigned int part = start; part < at; part += 1u)
        {
            const CliInspectTok *const tok = cli_py_tok(line, part);
            if (cli_py_is(src, tok, "as"))
            {
                alias = (part + 1u < at) ? cli_py_tok(line, part + 1u) : NULL;
                break;
            }
            cli_line_counted(&dotted, src + tok->start, tok->end - tok->start);
        }
        if (dotted.at > 0u)
        {
            facts_import(facts, &dotted, facts->names, alias, src, line);
        }
        start = at + 1u;
    }
}

// a `from a import b as c, (d)` line's import put on the facts
static void facts_from_import(const char *src, const CliPyLine *line, CliFacts *facts)
{
    unsigned int import_at = line->count;
    for (unsigned int at = 0u; (import_at == line->count) && (at < line->count); at += 1u)
    {
        import_at = cli_py_is(src, cli_py_tok(line, at), "import") ? at : import_at;
    }
    if (import_at == line->count)
    {
        return;
    }
    char spec[CLI_PATH_LONGEST];
    CliLine dotted = {spec, sizeof(spec), 0ull};
    for (unsigned int at = 1u; at < import_at; at += 1u)
    {
        const CliInspectTok *const tok = cli_py_tok(line, at);
        cli_line_counted(&dotted, src + tok->start, tok->end - tok->start);
    }
    const unsigned int name_first = facts->names;
    unsigned int start = import_at + 1u;
    for (unsigned int at = start; at <= line->count; at += 1u)
    {
        if ((at < line->count) && !cli_py_is(src, cli_py_tok(line, at), ","))
        {
            continue;
        }
        const CliInspectTok *name = NULL;
        const CliInspectTok *alias = NULL;
        for (unsigned int part = start; part < at; part += 1u)
        {
            const CliInspectTok *const tok = cli_py_tok(line, part);
            if (cli_py_is(src, tok, "(") || cli_py_is(src, tok, ")"))
            {
                continue;
            }
            if (name == NULL)
            {
                name = tok;
            }
            else if (cli_py_is(src, tok, "as") && (alias == NULL))
            {
                for (unsigned int after = part + 1u; (alias == NULL) && (after < at); after += 1u)
                {
                    const CliInspectTok *const next = cli_py_tok(line, after);
                    alias = (cli_py_is(src, next, "(") || cli_py_is(src, next, ")")) ? NULL : next;
                }
                break;
            }
        }
        if ((name != NULL) && !cli_py_is(src, name, "*"))
        {
            (void)cli_inspect_name_put(facts, src + name->start, name->end - name->start,
                                       (alias != NULL) ? (src + alias->start) : NULL,
                                       (alias != NULL) ? (alias->end - alias->start) : 0ull, name->from, name->to);
        }
        start = at + 1u;
    }
    facts_import(facts, &dotted, name_first, NULL, src, line);
}

void cli_py_imports(const char *src, CliFacts *facts)
{
    unsigned int skip_to = 0u;
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        if (at < skip_to)
        {
            continue;
        }
        const CliPyLine *const line = &g_cli_py_line[at];
        const int declares = cli_py_head_is(src, line, "def") || cli_py_head_is(src, line, "class");
        const int checking = cli_py_head_is(src, line, "if") &&
                             cli_py_holds(src, &g_cli_inspect_tok[line->first], line->count, "TYPE_CHECKING");
        if ((declares || checking) && cli_py_opens(src, line))
        {
            skip_to = cli_py_block_end(at);
            continue;
        }
        if (cli_py_is(src, cli_py_tok(line, 0u), "import"))
        {
            facts_plain_import(src, line, facts);
        }
        else if (cli_py_is(src, cli_py_tok(line, 0u), "from"))
        {
            facts_from_import(src, line, facts);
        }
    }
}

void cli_inspect_py_facts(const char *src, unsigned long long size, CliFacts *facts)
{
    cli_py_read(src, size);
    cli_py_unreachable(src, size, facts);
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        if (cli_py_head_is(src, line, "def") && cli_py_opens(src, line))
        {
            cli_py_unread(src, size, at, facts);
        }
    }
    cli_py_docstrings(src, size, facts);
    cli_py_imports(src, facts);
    cli_py_classes(src, facts);
    cli_py_functions(src, facts);
    facts->full = facts->full || g_cli_inspect_over;
}
