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

void cli_inspect_py_sort_layout(const CliInspectPartitionCtx *args)
{
    const unsigned long long lines = CLI_INSPECT_LINES;
    s_starts = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                          .partition = args->partition, .bytes = lines * sizeof(unsigned long long));
    s_ends = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                        .bytes = lines * sizeof(unsigned long long));
    s_firsts = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                          .partition = args->partition, .bytes = lines * sizeof(unsigned int));
    s_lasts = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                         .bytes = lines * sizeof(unsigned int));
    s_names = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                         .bytes = lines * sizeof(const char *));
    s_name_sizes = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                              .partition = args->partition, .bytes = lines * sizeof(unsigned int));
}

typedef struct
{
    const char *src;
    unsigned int index;
} InspectPySortCtx;
// whether the physical line at `index` is a comment, past the white letters at its start
EMBED_INLINE int sort_comment(const InspectPySortCtx *args)
{
    unsigned long long at = s_starts[args->index];
    while (at < s_ends[args->index])
    {
        unsigned int letter = 0u;
        const unsigned long long reserved = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                       .text_bytes = (const unsigned char *)args->src,
                                                       .length = s_ends[args->index], .at = at, .letter = &letter);
        if (!EMBED_CALL(cli_inspect_internal.white, CliInspectCtx, .letter = letter))
        {
            return letter == '#';
        }
        at += reserved;
    }
    return 0;
}

int cli_inspect_py_sort_methods(const CliInspectSizeSrcCtx *args)
{
    CliInspectEdit *edit = args->edit;
    EMBED_CALL(cli_inspect_py_src_size.py_read, CliInspectPySrcSizeCfg, .src = args->src, .size = args->size);
    const unsigned int at = EMBED_CALL(cli_inspect_py_src_line_head.py_around, CliInspectPySrcLineHeadCfg,
                                       .src = args->src, .line_u32 = args->line, .head = "class");
    if (at == g_cli_py_lines)
    {
        mmgr_line_put(args->err_msg, "Sort Methods by Name sorts the class the cursor is in.");
        return 0;
    }
    const unsigned int end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at);
    if (end <= at + 1u)
    {
        mmgr_line_put(args->err_msg, "The class's body stands on its header's line.");
        return 0;
    }
    const unsigned int indent = g_cli_py_line[at + 1u].indent;
    const unsigned int physical =
        EMBED_CALL(cli_inspect_size_src_starts_internal.physical, CliInspectSizeSrcStartsCtx, .src = args->src,
                   .size = args->size, .starts = s_starts, .ends = s_ends, .most = CLI_INSPECT_LINES);
    unsigned int members = 0u;
    unsigned int index = at + 1u;
    unsigned int floor = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = &g_cli_py_line[at],
                                    .at = g_cli_py_line[at].count - 1u)
                             ->to.line +
                         1u;
    while ((index < end) && (members < CLI_INSPECT_LINES))
    {
        const CliPyLine *const one = &g_cli_py_line[index];
        if (one->indent != indent)
        {
            index += 1u;
            continue;
        }
        unsigned int first = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = one, .at = 0u)->from.line;
        // decorators and the comments over a member go with it
        unsigned int def_at = index;
        while ((def_at < end) && (g_cli_py_line[def_at].indent == indent) &&
               EMBED_CALL(
                   cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                   .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = &g_cli_py_line[def_at], .at = 0u),
                   .word = "@"))
        {
            def_at += 1u;
        }
        def_at = (def_at < end - 1u) ? def_at : (end - 1u);
        while ((first > floor) && (first - 1u < physical) &&
               EMBED_CALL(sort_comment, InspectPySortCtx, .src = args->src, .index = first - 1u))
        {
            first -= 1u;
        }
        const unsigned int member_end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = def_at);
        const CliPyLine *const closing = &g_cli_py_line[member_end - 1u];
        const unsigned int last =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = closing, .at = closing->count - 1u)->to.line;
        const CliPyLine *const defined = &g_cli_py_line[def_at];
        s_names[members] = NULL;
        s_name_sizes[members] = 0u;
        if (EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = args->src, .line = defined,
                       .word = "def"))
        {
            for (unsigned int part = 0u; part + 1u < defined->count; part += 1u)
            {
                if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src,
                               .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = defined, .at = part),
                               .word = "def"))
                {
                    const CliInspectTok *const name =
                        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = defined, .at = part + 1u);
                    s_names[members] = args->src + name->start;
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
        mmgr_line_put(args->err_msg, "The class has no members.");
        return 0;
    }
    const unsigned int first = s_firsts[0];
    const unsigned int last = s_lasts[members - 1u];
    if ((last >= physical) ||
        !EMBED_CALL(cli_inspect_size_internal.sorted_body, CliInspectSizeCtx, .src = args->src, .size = args->size,
                    .line_starts = s_starts, .line_ends = s_ends, .firsts = s_firsts, .lasts = s_lasts,
                    .names = s_names, .name_sizes = s_name_sizes, .members = members, .out = args->text))
    {
        mmgr_line_put(args->err_msg, "The class's methods stand in the order of their names.");
        return 0;
    }
    unsigned int end_col = 0u;
    for (unsigned long long letter_at = s_starts[last]; letter_at < s_ends[last];)
    {
        unsigned int letter = 0u;
        letter_at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                .text_bytes = (const unsigned char *)args->src, .length = s_ends[last], .at = letter_at,
                                .letter = &letter);
        end_col += (letter >= 0x10000u) ? 2u : 1u;
    }
    *edit = (CliInspectEdit){{first, 0u}, {last, end_col}, {0u, 0u}};
    return 1;
}
