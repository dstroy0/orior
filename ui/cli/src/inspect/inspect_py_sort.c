// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_sort.c: the Python class around a line with its methods sorted by name, each method with the comments
// and decorators over it, the class's other members and its blank lines staying where they stand
#include "../text/json_tree.h"
#include "inspect_py.h"

// the physical lines of the text, each where it starts and ends; and the class's members, each its first and last
// physical lines and its name where it is a method
static unsigned long long *s_starts;
static unsigned long long *s_ends;
static unsigned int *s_firsts;
static unsigned int *s_lasts;
static const char **s_names;
static unsigned int *s_name_sizes;

void cli_inspect_py_sort_laid(CliInspectCarve *carve)
{
    const unsigned long long lines = CLI_INSPECT_LINES;
    s_starts = cli_inspect_carved(carve, lines * sizeof(unsigned long long));
    s_ends = cli_inspect_carved(carve, lines * sizeof(unsigned long long));
    s_firsts = cli_inspect_carved(carve, lines * sizeof(unsigned int));
    s_lasts = cli_inspect_carved(carve, lines * sizeof(unsigned int));
    s_names = cli_inspect_carved(carve, lines * sizeof(const char *));
    s_name_sizes = cli_inspect_carved(carve, lines * sizeof(unsigned int));
}

// whether the physical line at `index` is a comment, past the white letters at its start
static int sort_comment(const char *src, unsigned int index)
{
    unsigned long long at = s_starts[index];
    while (at < s_ends[index])
    {
        unsigned int letter = 0u;
        const unsigned long long taken = cli_lossy_letter((const unsigned char *)src, s_ends[index], at, &letter);
        if (!cli_inspect_white(letter))
        {
            return letter == '#';
        }
        at += taken;
    }
    return 0;
}

int cli_inspect_py_sort_methods(const char *src, unsigned long long size, unsigned int line, CliInspectEdit *edit,
                                CliLine *text, CliLine *said)
{
    cli_py_read(src, size);
    const unsigned int at = cli_py_around(src, line, "class");
    if (at == g_cli_py_lines)
    {
        cli_line_text(said, "Sort Methods by Name sorts the class the cursor is in.");
        return 0;
    }
    const unsigned int end = cli_py_block_end(at);
    if (end <= at + 1u)
    {
        cli_line_text(said, "The class's body stands on its header's line.");
        return 0;
    }
    const unsigned int indent = g_cli_py_line[at + 1u].indent;
    const unsigned int physical = cli_inspect_physical(src, size, s_starts, s_ends, CLI_INSPECT_LINES);
    unsigned int members = 0u;
    unsigned int index = at + 1u;
    unsigned int floor = cli_py_tok(&g_cli_py_line[at], g_cli_py_line[at].count - 1u)->to.line + 1u;
    while ((index < end) && (members < CLI_INSPECT_LINES))
    {
        const CliPyLine *const one = &g_cli_py_line[index];
        if (one->indent != indent)
        {
            index += 1u;
            continue;
        }
        unsigned int first = cli_py_tok(one, 0u)->from.line;
        // decorators and the comments over a member go with it
        unsigned int def_at = index;
        while ((def_at < end) && (g_cli_py_line[def_at].indent == indent) &&
               cli_py_is(src, cli_py_tok(&g_cli_py_line[def_at], 0u), "@"))
        {
            def_at += 1u;
        }
        def_at = (def_at < end - 1u) ? def_at : (end - 1u);
        while ((first > floor) && (first - 1u < physical) && sort_comment(src, first - 1u))
        {
            first -= 1u;
        }
        const unsigned int member_end = cli_py_block_end(def_at);
        const CliPyLine *const closing = &g_cli_py_line[member_end - 1u];
        const unsigned int last = cli_py_tok(closing, closing->count - 1u)->to.line;
        const CliPyLine *const defined = &g_cli_py_line[def_at];
        s_names[members] = NULL;
        s_name_sizes[members] = 0u;
        if (cli_py_head_is(src, defined, "def"))
        {
            for (unsigned int part = 0u; part + 1u < defined->count; part += 1u)
            {
                if (cli_py_is(src, cli_py_tok(defined, part), "def"))
                {
                    const CliInspectTok *const name = cli_py_tok(defined, part + 1u);
                    s_names[members] = src + name->start;
                    s_name_sizes[members] = name->end - name->start;
                    break;
                }
            }
        }
        s_firsts[members] = first;
        s_lasts[members] = last;
        members += 1u;
        floor = last + 1u;
        index = member_end;
    }
    if (members == 0u)
    {
        cli_line_text(said, "The class has no members.");
        return 0;
    }
    const unsigned int first = s_firsts[0];
    const unsigned int last = s_lasts[members - 1u];
    if ((last >= physical) ||
        !cli_inspect_sorted_body(src, size, s_starts, s_ends, s_firsts, s_lasts, s_names, s_name_sizes, members, text))
    {
        cli_line_text(said, "The class's methods stand in the order of their names.");
        return 0;
    }
    unsigned int end_col = 0u;
    for (unsigned long long letter_at = s_starts[last]; letter_at < s_ends[last];)
    {
        unsigned int letter = 0u;
        letter_at += cli_lossy_letter((const unsigned char *)src, s_ends[last], letter_at, &letter);
        end_col += (letter >= 0x10000u) ? 2u : 1u;
    }
    *edit = (CliInspectEdit){{first, 0u}, {last, end_col}, {0u, 0u}};
    return 1;
}
