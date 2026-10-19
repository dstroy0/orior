// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_classes.c: the classes a Python file declares at its top level: each one's name, the bases it names,
// what its body and its methods set, and the attributes its methods read of `self`
#include "inspect_py.h"

// the most attributes one class sets and reads that are kept, and the bytes of the base being read
#define CLI_PY_ATTRIBUTES 16384u
#define CLI_PY_BASE (4u * CLI_NAME_LONGEST)

// the operators that set a name to a value made from its own
static const char *const s_augmented[13] = {
    "+=", "-=", "*=", "/=", "//=", "%=", "**=", "|=", "&=", "^=", "@=", "<<=", ">>="};

// the attributes a class sets, each `size` bytes at `at`, and the tokens of those it reads
static const char *s_has[CLI_PY_ATTRIBUTES];
static unsigned int s_has_size[CLI_PY_ATTRIBUTES];
static unsigned int s_has_count;
static const CliInspectTok *s_reads[CLI_PY_ATTRIBUTES];
static unsigned int s_read_count;

// whether a token is one of the operators that set a name from its own
static int classes_augmented(const char *src, const CliInspectTok *tok)
{
    for (unsigned int at = 0u; at < 13u; at += 1u)
    {
        if (cli_py_is(src, tok, s_augmented[at]))
        {
            return 1;
        }
    }
    return 0;
}

// an attribute the class sets kept, once
static void classes_has(const char *at, unsigned int size)
{
    for (unsigned int index = 0u; index < s_has_count; index += 1u)
    {
        if ((s_has_size[index] == size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_has[index], .other = at, .bytes = size}) == 0))
        {
            return;
        }
    }
    if (s_has_count < CLI_PY_ATTRIBUTES)
    {
        s_has[s_has_count] = at;
        s_has_size[s_has_count] = size;
        s_has_count += 1u;
    }
}

// whether `self.name` at `offset` of a line's tokens stands before the line's assignment, as one of its targets:
// before its last `=` outside brackets, as each of a chain's targets does, or before an operator that sets it from
// itself, or before the colon of its annotation
static int classes_setting_target(const char *src, const CliInspectTok *toks, unsigned int count, unsigned int offset)
{
    int depth = 0;
    int has_last = 0;
    unsigned int last = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliInspectTok *const tok = &toks[at];
        if (cli_py_is(src, tok, "(") || cli_py_is(src, tok, "[") || cli_py_is(src, tok, "{"))
        {
            depth += 1;
        }
        else if (cli_py_is(src, tok, ")") || cli_py_is(src, tok, "]") || cli_py_is(src, tok, "}"))
        {
            depth -= 1;
        }
        else if ((depth == 0) && cli_py_is(src, tok, "="))
        {
            has_last = 1;
            last = at;
        }
        else if ((depth == 0) && classes_augmented(src, tok) && !has_last)
        {
            return offset < at;
        }
        else if ((depth == 0) && cli_py_is(src, tok, ":") && (at == offset + 3u))
        {
            return 1;
        }
    }
    return has_last && (offset < last);
}

// the bases a class's header names after its name at `name_at`, put on the facts' names
static void classes_bases(const char *src, const CliPyLine *line, unsigned int name_at, CliFacts *facts)
{
    if ((name_at + 1u >= line->count) || !cli_py_is(src, cli_py_tok(line, name_at + 1u), "("))
    {
        return;
    }
    int depth = 0;
    char current[CLI_PY_BASE];
    CliLine base = {current, sizeof(current), 0ull};
    int keyword = 0;
    for (unsigned int offset = name_at + 2u; offset < line->count; offset += 1u)
    {
        const CliInspectTok *const tok = cli_py_tok(line, offset);
        const int closes = cli_py_is(src, tok, ")") || cli_py_is(src, tok, "]") || cli_py_is(src, tok, "}");
        if (cli_py_is(src, tok, "(") || cli_py_is(src, tok, "[") || cli_py_is(src, tok, "{"))
        {
            depth += 1;
        }
        else if (closes && (depth > 0))
        {
            depth -= 1;
        }
        else if (cli_py_is(src, tok, ")") || ((depth == 0) && cli_py_is(src, tok, ",")))
        {
            const unsigned long long cut =
                (memor.chr(&(MemoriaCfg){.src = current, .val = '[', .bytes = (size_t)base.at}) != NULL)
                    ? (unsigned long long)((const char *)memor.chr(
                                               &(MemoriaCfg){.src = current, .val = '[', .bytes = (size_t)base.at}) -
                                           current)
                    : base.at;
            if (!keyword && (cut > 0u) && cli_line_fits(&base))
            {
                (void)cli_inspect_name_put(facts, current, cut, NULL, 0ull, tok->from, tok->from);
            }
            if (cli_py_is(src, tok, ")"))
            {
                return;
            }
            base.at = 0ull;
            keyword = 0;
            continue;
        }
        else if ((depth == 0) && cli_py_is(src, tok, "="))
        {
            keyword = 1;
        }
        if ((depth == 0) && !cli_py_is(src, tok, "(") && !cli_py_is(src, tok, ")") && (tok->kind != CLI_PY_STR))
        {
            cli_line_counted(&base, src + tok->start, tok->end - tok->start);
        }
        else if ((depth > 0) && cli_py_is(src, tok, "["))
        {
            cli_line_character(&base, '[');
        }
    }
}

// what a line of a class's body at its own indent sets: a method's or a class's name, or each target of an
// assignment
static void classes_body_line(const char *src, const CliPyLine *inner, int *dynamic)
{
    if (cli_py_head_is(src, inner, "def") || cli_py_head_is(src, inner, "class"))
    {
        unsigned int offset = 1u;
        for (unsigned int at = 0u; at < inner->count; at += 1u)
        {
            if (cli_py_is(src, cli_py_tok(inner, at), "def") || cli_py_is(src, cli_py_tok(inner, at), "class"))
            {
                offset = at + 1u;
                break;
            }
        }
        if (offset < inner->count)
        {
            const CliInspectTok *const name = cli_py_tok(inner, offset);
            classes_has(src + name->start, name->end - name->start);
            *dynamic = *dynamic || cli_py_is(src, name, "__getattr__") || cli_py_is(src, name, "__getattribute__");
        }
        return;
    }
    if (cli_py_tok(inner, 0u)->kind != CLI_PY_NAME)
    {
        return;
    }
    for (unsigned int offset = 1u; offset < inner->count; offset += 1u)
    {
        const CliInspectTok *const tok = cli_py_tok(inner, offset);
        if (!cli_py_is(src, tok, "=") && !cli_py_is(src, tok, ":"))
        {
            continue;
        }
        for (unsigned int target = 0u; target < offset; target += 1u)
        {
            const CliInspectTok *const one = cli_py_tok(inner, target);
            if (!cli_py_is(src, one, ",") && !cli_py_is(src, one, "(") && !cli_py_is(src, one, ")"))
            {
                classes_has(src + one->start, one->end - one->start);
            }
        }
        return;
    }
}

// what a line of a method sets and reads of `self`
static void classes_method_line(const char *src, const CliPyLine *inner, int *dynamic)
{
    const CliInspectTok *const toks = &g_cli_inspect_tok[inner->first];
    const unsigned int count = inner->count;
    *dynamic = *dynamic || cli_py_holds(src, toks, count, "setattr") || cli_py_holds(src, toks, count, "__dict__");
    for (unsigned int offset = 0u; offset + 2u < count; offset += 1u)
    {
        const int attribute = cli_py_is(src, &toks[offset], "self") && cli_py_is(src, &toks[offset + 1u], ".") &&
                              (toks[offset + 2u].kind == CLI_PY_NAME) &&
                              ((offset == 0u) || !cli_py_is(src, &toks[offset - 1u], "."));
        if (attribute)
        {
            const CliInspectTok *const name = &toks[offset + 2u];
            const CliInspectTok *const next = (offset + 3u < count) ? &toks[offset + 3u] : NULL;
            const int setter = (next != NULL) && (cli_py_is(src, next, "=") || cli_py_is(src, next, ",") ||
                                                  cli_py_is(src, next, ":") || classes_augmented(src, next));
            const int sets =
                (cli_py_depth_at(src, toks, offset) == 0) && setter && classes_setting_target(src, toks, count, offset);
            if (sets)
            {
                classes_has(src + name->start, name->end - name->start);
            }
            else if (s_read_count < CLI_PY_ATTRIBUTES)
            {
                s_reads[s_read_count] = name;
                s_read_count += 1u;
            }
        }
        // `getattr(self, "name", ...)` and `hasattr(self, "name")` ask whether it is set, and take it as set
        const int asks = (cli_py_is(src, &toks[offset], "getattr") || cli_py_is(src, &toks[offset], "hasattr")) &&
                         cli_py_is(src, &toks[offset + 1u], "(") && cli_py_is(src, &toks[offset + 2u], "self") &&
                         (offset + 3u < count) && cli_py_is(src, &toks[offset + 3u], ",");
        if (asks && (offset + 4u < count) && (toks[offset + 4u].kind == CLI_PY_STR))
        {
            const CliInspectTok *const named = &toks[offset + 4u];
            unsigned int start = named->start;
            unsigned int end = named->end;
            while ((start < end) && ((src[start] == '\'') || (src[start] == '"')))
            {
                start += 1u;
            }
            while ((end > start) && ((src[end - 1u] == '\'') || (src[end - 1u] == '"')))
            {
                end -= 1u;
            }
            classes_has(src + start, end - start);
        }
    }
}

void cli_py_classes(const char *src, CliFacts *facts)
{
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[at];
        if ((line->indent != 0u) || !cli_py_head_is(src, line, "class") || !cli_py_opens(src, line))
        {
            continue;
        }
        unsigned int name_at = line->count;
        for (unsigned int offset = 0u; (name_at == line->count) && (offset < line->count); offset += 1u)
        {
            name_at = cli_py_is(src, cli_py_tok(line, offset), "class") ? (offset + 1u) : name_at;
        }
        if (name_at >= line->count)
        {
            continue;
        }
        const unsigned int base_first = facts->names;
        classes_bases(src, line, name_at, facts);
        const unsigned int bases = facts->names - base_first;
        const unsigned int end = cli_py_block_end(at);
        const unsigned int body_indent = (at + 1u < g_cli_py_lines) ? g_cli_py_line[at + 1u].indent : 0u;
        s_has_count = 0u;
        s_read_count = 0u;
        int dynamic = 0;
        for (unsigned int index = at + 1u; index < end; index += 1u)
        {
            const CliPyLine *const inner = &g_cli_py_line[index];
            if (inner->indent == body_indent)
            {
                classes_body_line(src, inner, &dynamic);
            }
            else
            {
                classes_method_line(src, inner, &dynamic);
            }
        }
        const unsigned int has_first = facts->names;
        for (unsigned int index = 0u; index < s_has_count; index += 1u)
        {
            const CliInspectPlace none = {0u, 0u};
            (void)cli_inspect_name_put(facts, s_has[index], s_has_size[index], NULL, 0ull, none, none);
        }
        const unsigned int read_first = facts->names;
        for (unsigned int index = 0u; index < s_read_count; index += 1u)
        {
            const CliInspectTok *const read = s_reads[index];
            (void)cli_inspect_name_put(facts, src + read->start, read->end - read->start, NULL, 0ull, read->from,
                                       read->to);
        }
        if (facts->classes == facts->classes_most)
        {
            facts->full = 1;
            return;
        }
        const CliInspectTok *const name = cli_py_tok(line, name_at);
        facts->class_of[facts->classes] =
            (CliInspectClass){cli_inspect_kept(facts, src + name->start, name->end - name->start),
                              name->from,
                              name->to,
                              base_first,
                              bases,
                              has_first,
                              read_first - has_first,
                              read_first,
                              facts->names - read_first,
                              dynamic};
        facts->classes += 1u;
    }
}
