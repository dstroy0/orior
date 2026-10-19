// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_unread.c: the names a Python function sets and never reads: each name set by an assignment of it alone,
// by `with ... as` or by `except ... as`, directly in the function, and read nowhere in it or in what it holds. A name
// that starts with an underscore, is declared global or nonlocal, or sits in a function that reads its locals whole,
// is left alone
#include "../text/json_tree.h"
#include "inspect_py.h"

// the most offsets of a statement's tokens that set a name, which are no reads
#define CLI_PY_SKIPS 4096u

// how a set name's fix takes it out: it has none; `as name` taken out; `name =` taken out and its value kept; the line
// that sets it taken out
#define CLI_PY_FIX_NONE 0u
#define CLI_PY_FIX_AS 1u
#define CLI_PY_FIX_VALUE 2u
#define CLI_PY_FIX_LINE 3u

// A place a name is set that may go unread: the name's token, and the fix that takes it out and its span
typedef struct
{
    CliInspectTok tok;
    unsigned int fix;
    CliInspectPlace from;
    CliInspectPlace to;
} PySet;

static PySet *s_sets;
static unsigned int s_set_count;
static CliInspectSet s_read;
static CliInspectSet s_declared;
static unsigned int *s_equals;

void cli_inspect_py_unread_laid(CliInspectCarve *carve)
{
    s_sets = cli_inspect_carved(carve, (unsigned long long)CLI_INSPECT_LINES * sizeof(PySet));
    cli_inspect_set_laid(&s_read, carve);
    cli_inspect_set_laid(&s_declared, carve);
    s_equals = cli_inspect_carved(carve, (unsigned long long)(CLI_INSPECT_TOKENS / 8u + 1u) * sizeof(unsigned int));
}
static unsigned int s_skip[CLI_PY_SKIPS];
static unsigned int s_skips;

int cli_py_depth_at(const char *src, const CliInspectTok *toks, unsigned int offset)
{
    int depth = 0;
    for (unsigned int at = 0u; at < offset; at += 1u)
    {
        depth += (cli_py_is(src, &toks[at], "(") || cli_py_is(src, &toks[at], "[") || cli_py_is(src, &toks[at], "{"))
                     ? 1
                     : 0;
        depth -= (cli_py_is(src, &toks[at], ")") || cli_py_is(src, &toks[at], "]") || cli_py_is(src, &toks[at], "}"))
                     ? 1
                     : 0;
    }
    return depth;
}

void cli_py_whole_lines(const char *src, unsigned long long size, unsigned int from, unsigned int to,
                        CliInspectPlace *start, CliInspectPlace *end)
{
    unsigned int count = 1u;
    unsigned long long last_start = 0ull;
    unsigned long long last_end = size;
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        if (src[at] == '\n')
        {
            if (count - 1u == to)
            {
                last_end = at;
            }
            count += 1u;
            last_start = (count - 1u == to) ? (at + 1u) : last_start;
        }
    }
    *start = (CliInspectPlace){from, 0u};
    if (to + 1u < count)
    {
        *end = (CliInspectPlace){to + 1u, 0u};
        return;
    }
    unsigned int units = 0u;
    for (unsigned long long at = last_start; at < last_end;)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)src, last_end, at, &letter);
        units += (letter >= 0x10000u) ? 2u : 1u;
    }
    *end = (CliInspectPlace){to, units};
}

// whether the token at `offset` of `count` is a name read: a name that is no keyword, no attribute after a dot, and no
// keyword argument's name
static int unread_is_read(const char *src, const CliInspectTok *toks, unsigned int count, unsigned int offset)
{
    const CliInspectTok *const tok = &toks[offset];
    if (tok->kind != CLI_PY_NAME)
    {
        return 0;
    }
    if (tok->field)
    {
        return 1;
    }
    if (cli_py_keyword(src, tok))
    {
        return 0;
    }
    const CliInspectTok *const before = (offset > 0u) ? &toks[offset - 1u] : NULL;
    if ((before != NULL) && cli_py_is(src, before, "."))
    {
        return 0;
    }
    const int equals = (offset + 1u < count) && cli_py_is(src, &toks[offset + 1u], "=");
    const int opened = (before != NULL) && (cli_py_is(src, before, "(") || cli_py_is(src, before, ","));
    return !(equals && opened && (cli_py_depth_at(src, toks, offset) > 0));
}

// the names `count` tokens read put in the set of names read
static void unread_reads_of(const char *src, const CliInspectTok *toks, unsigned int count)
{
    for (unsigned int offset = 0u; offset < count; offset += 1u)
    {
        if (unread_is_read(src, toks, count, offset))
        {
            cli_inspect_set_put(&s_read, src + toks[offset].start, toks[offset].end - toks[offset].start);
        }
    }
}

// a name that may go unread noted
static void unread_set(const CliInspectTok *tok, unsigned int fix, CliInspectPlace from, CliInspectPlace to)
{
    if (s_set_count < CLI_INSPECT_LINES)
    {
        s_sets[s_set_count] = (PySet){*tok, fix, from, to};
        s_set_count += 1u;
    }
}

// an offset that sets a name, and so is no read, kept
static void unread_skip(unsigned int offset)
{
    if (s_skips < CLI_PY_SKIPS)
    {
        s_skip[s_skips] = offset;
        s_skips += 1u;
    }
}

// the names `with ... as name` and `except ... as name` set noted
static void unread_as(const char *src, const CliInspectTok *toks, unsigned int count)
{
    int depth = 0;
    for (unsigned int offset = 0u; offset < count; offset += 1u)
    {
        const CliInspectTok *const tok = &toks[offset];
        depth += (cli_py_is(src, tok, "(") || cli_py_is(src, tok, "[") || cli_py_is(src, tok, "{")) ? 1 : 0;
        depth -= (cli_py_is(src, tok, ")") || cli_py_is(src, tok, "]") || cli_py_is(src, tok, "}")) ? 1 : 0;
        const int named = (offset + 1u < count) && (toks[offset + 1u].kind == CLI_PY_NAME);
        const int ended = (offset + 2u < count) &&
                          (cli_py_is(src, &toks[offset + 2u], ",") || cli_py_is(src, &toks[offset + 2u], ":"));
        if ((depth == 0) && cli_py_is(src, tok, "as") && named && ended && (offset > 0u))
        {
            unread_skip(offset + 1u);
            unread_set(&toks[offset + 1u], CLI_PY_FIX_AS, toks[offset - 1u].to, toks[offset + 1u].to);
        }
    }
}

// the names a statement of `count` tokens sets that may go unread noted, and the offsets of its tokens that set them,
// which are no reads, kept in s_skip
static void unread_setting(const char *src, unsigned long long size, const CliInspectTok *toks, unsigned int count,
                           int lone, const CliPyLine *line)
{
    s_skips = 0u;
    const int async_with = cli_py_is(src, &toks[0], "async") && (count > 1u) && cli_py_is(src, &toks[1], "with");
    if (cli_py_is(src, &toks[0], "with") || cli_py_is(src, &toks[0], "except") || async_with)
    {
        unread_as(src, toks, count);
        return;
    }
    unsigned int equals = 0u;
    int depth = 0;
    for (unsigned int offset = 0u; offset < count; offset += 1u)
    {
        const CliInspectTok *const tok = &toks[offset];
        depth += (cli_py_is(src, tok, "(") || cli_py_is(src, tok, "[") || cli_py_is(src, tok, "{")) ? 1 : 0;
        depth -= (cli_py_is(src, tok, ")") || cli_py_is(src, tok, "]") || cli_py_is(src, tok, "}")) ? 1 : 0;
        if ((depth == 0) && cli_py_is(src, tok, "=") && (tok->kind == CLI_PY_OP) && (equals < CLI_INSPECT_TOKENS / 8u))
        {
            s_equals[equals] = offset;
            equals += 1u;
        }
    }
    if (equals == 0u)
    {
        return;
    }
    const unsigned int last = s_equals[equals - 1u];
    unsigned int start = 0u;
    for (unsigned int index = 0u; index < equals; index += 1u)
    {
        const unsigned int equal = s_equals[index];
        const unsigned int targets = equal - start;
        const int annotated = (targets > 2u) && (start == 0u) && (count > 1u) && cli_py_is(src, &toks[1], ":");
        if (((targets == 1u) || annotated) && (toks[start].kind == CLI_PY_NAME) && !cli_py_keyword(src, &toks[start]))
        {
            unread_skip(start);
            unsigned int fix = CLI_PY_FIX_NONE;
            CliInspectPlace from = toks[0].from;
            CliInspectPlace to = toks[0].from;
            if ((equals == 1u) && lone && !annotated)
            {
                const int calls = cli_py_holds(src, &toks[last + 1u], count - last - 1u, "(");
                fix = calls ? CLI_PY_FIX_VALUE : CLI_PY_FIX_LINE;
                if (calls)
                {
                    to = toks[last + 1u].from;
                }
                else
                {
                    const unsigned int end_line = g_cli_inspect_tok[line->first + line->count - 1u].to.line;
                    cli_py_whole_lines(src, size, toks[0].from.line, end_line, &from, &to);
                }
            }
            if (last + 1u < count)
            {
                unread_set(&toks[start], fix, from, to);
            }
        }
        else
        {
            int names_only = 1;
            int comma = 0;
            for (unsigned int at = start; at < equal; at += 1u)
            {
                names_only = names_only && ((toks[at].kind == CLI_PY_NAME) || cli_py_is(src, &toks[at], ","));
                comma = comma || cli_py_is(src, &toks[at], ",");
            }
            // a tuple's names are set, and not reported
            for (unsigned int at = start; names_only && comma && (at < equal); at += 1u)
            {
                if (toks[at].kind == CLI_PY_NAME)
                {
                    unread_skip(at);
                }
            }
        }
        start = equal + 1u;
    }
}

// whether `offset` is among the offsets that set a name
static int unread_skipped(unsigned int offset)
{
    for (unsigned int at = 0u; at < s_skips; at += 1u)
    {
        if (s_skip[at] == offset)
        {
            return 1;
        }
    }
    return 0;
}

// each statement of a line read: the names it declares global or nonlocal, those it sets, and those it reads
static void unread_line(const char *src, unsigned long long size, const CliPyLine *line)
{
    const CliInspectTok *const toks = &g_cli_inspect_tok[line->first];
    unsigned int start = 0u;
    int depth = 0;
    for (unsigned int at = 0u; at <= line->count; at += 1u)
    {
        const int ends =
            (at == line->count) || ((toks[at].kind == CLI_PY_OP) && (depth == 0) && cli_py_is(src, &toks[at], ";"));
        if ((at < line->count) && (toks[at].kind == CLI_PY_OP))
        {
            depth +=
                (cli_py_is(src, &toks[at], "(") || cli_py_is(src, &toks[at], "[") || cli_py_is(src, &toks[at], "{"))
                    ? 1
                    : 0;
            depth -=
                (cli_py_is(src, &toks[at], ")") || cli_py_is(src, &toks[at], "]") || cli_py_is(src, &toks[at], "}"))
                    ? 1
                    : 0;
        }
        if (!ends)
        {
            continue;
        }
        const CliInspectTok *const statement = &toks[start];
        const unsigned int count = at - start;
        start = at + 1u;
        if (count == 0u)
        {
            continue;
        }
        if (cli_py_is(src, &statement[0], "global") || cli_py_is(src, &statement[0], "nonlocal"))
        {
            for (unsigned int name = 1u; name < count; name += 1u)
            {
                if (statement[name].kind == CLI_PY_NAME)
                {
                    cli_inspect_set_put(&s_declared, src + statement[name].start,
                                        statement[name].end - statement[name].start);
                }
            }
            continue;
        }
        unread_setting(src, size, statement, count, line->count == count, line);
        for (unsigned int offset = 0u; offset < count; offset += 1u)
        {
            if (!unread_skipped(offset) && unread_is_read(src, statement, count, offset))
            {
                cli_inspect_set_put(&s_read, src + statement[offset].start,
                                    statement[offset].end - statement[offset].start);
            }
        }
    }
}

// a set name that goes unread reported, with its fix
static void unread_reported(const char *src, const PySet *set, CliFacts *facts)
{
    const char *const name = src + set->tok.start;
    const unsigned int length = set->tok.end - set->tok.start;
    char message[CLI_NAME_LONGEST * 2u];
    CliLine said = {message, sizeof(message), 0ull};
    cli_line_character(&said, '`');
    cli_line_counted(&said, name, length);
    cli_line_text(&said, "` is set and never read.");
    cli_inspect_finding_put(facts, set->tok.from, set->tok.to, CLI_INSPECT_WARNING, &said, "unread", NULL);
    if (set->fix == CLI_PY_FIX_NONE)
    {
        return;
    }
    char title[CLI_NAME_LONGEST * 2u];
    CliLine titled = {title, sizeof(title), 0ull};
    cli_line_text(&titled, (set->fix == CLI_PY_FIX_AS)      ? "Take out `as "
                           : (set->fix == CLI_PY_FIX_VALUE) ? "Take out `"
                                                            : "Take out the line that sets ");
    cli_line_counted(&titled, name, length);
    cli_line_text(&titled, (set->fix == CLI_PY_FIX_AS)      ? "`"
                           : (set->fix == CLI_PY_FIX_VALUE) ? " =` and keep its value"
                                                            : "");
    cli_inspect_fix_put(facts, &titled);
    cli_inspect_edit_put(facts, set->from, set->to, "", 0ull);
}

void cli_py_unread(const char *src, unsigned long long size, unsigned int at, CliFacts *facts)
{
    const unsigned int end = cli_py_block_end(at);
    s_set_count = 0u;
    cli_inspect_set_cleared(&s_read);
    cli_inspect_set_cleared(&s_declared);
    unsigned int nested_end = 0u;
    for (unsigned int index = at + 1u; index < end; index += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[index];
        const int nested = (index < nested_end);
        if (!nested && (cli_py_head_is(src, line, "def") || cli_py_head_is(src, line, "class")) &&
            cli_py_opens(src, line))
        {
            nested_end = cli_py_block_end(index);
            unread_reads_of(src, &g_cli_inspect_tok[line->first], line->count);
            continue;
        }
        if (nested)
        {
            unread_reads_of(src, &g_cli_inspect_tok[line->first], line->count);
            continue;
        }
        unread_line(src, size, line);
    }
    static const char *const s_whole[4] = {"locals", "vars", "eval", "exec"};
    for (unsigned int word = 0u; word < 4u; word += 1u)
    {
        if (cli_inspect_set_has(
                &s_read, s_whole[word],
                (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = s_whole[word], .cap = CLI_CELLS_POOL})))
        {
            return;
        }
    }
    for (unsigned int index = 0u; index < s_set_count; index += 1u)
    {
        const PySet *const set = &s_sets[index];
        const char *const name = src + set->tok.start;
        const unsigned int length = set->tok.end - set->tok.start;
        if ((name[0] == '_') || cli_inspect_set_has(&s_read, name, length) ||
            cli_inspect_set_has(&s_declared, name, length))
        {
            continue;
        }
        unread_reported(src, set, facts);
    }
}
