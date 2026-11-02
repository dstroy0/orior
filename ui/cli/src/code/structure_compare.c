// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure_compare.c: two texts compared by structure: the lines changed, moved and reshaped, and the tokens of each
// change changed or renamed, written as the window reads them
#include "../command_line/cells.h"
#include "structure_internal.h"

#include <stdlib.h>

// the one compare's state
CliStructureState *g_cli_structure;

static CliLock s_lock = CLI_LOCK_INIT;

typedef struct
{
    CliCompare *compare;
    unsigned int steps;
    const CliSide *side;
    unsigned int line;
    const unsigned char *kinds;
    unsigned int count;
    unsigned int run_out;
    unsigned int run_in;
} StructureCompareCtx;
// The changes: each run of lines taken out and put in between lines unchanged, with the row it starts at
EMBED_INLINE void cli_hunks_find(const StructureCompareCtx *args)
{
    unsigned int at = 0u;
    while (at < args->steps)
    {
        if (g_cli_structure->line_steps[at].kind == CLI_STEP_EQUAL)
        {
            g_cli_structure->row_left[args->compare->rows] = g_cli_structure->line_steps[at].x;
            g_cli_structure->row_right[args->compare->rows] = g_cli_structure->line_steps[at].y;
            args->compare->rows += 1u;
            at += 1u;
            continue;
        }
        CliHunk hunk = {args->compare->outs, 0u, args->compare->ins, 0u};
        while ((at < args->steps) && (g_cli_structure->line_steps[at].kind != CLI_STEP_EQUAL))
        {
            if (g_cli_structure->line_steps[at].kind == CLI_STEP_OUT)
            {
                g_cli_structure->outs[args->compare->outs] = g_cli_structure->line_steps[at].x;
                args->compare->outs += 1u;
                hunk.out_count += 1u;
            }
            else
            {
                g_cli_structure->ins[args->compare->ins] = g_cli_structure->line_steps[at].y;
                args->compare->ins += 1u;
                hunk.in_count += 1u;
            }
            at += 1u;
        }
        g_cli_structure->starts[args->compare->starts] = args->compare->rows;
        args->compare->starts += 1u;
        const unsigned int count = (hunk.out_count > hunk.in_count) ? hunk.out_count : hunk.in_count;
        for (unsigned int row = 0u; row < count; row += 1u)
        {
            g_cli_structure->row_left[args->compare->rows] =
                (row < hunk.out_count) ? g_cli_structure->outs[hunk.out_first + row] : CLI_STRUCTURE_NONE;
            g_cli_structure->row_right[args->compare->rows] =
                (row < hunk.in_count) ? g_cli_structure->ins[hunk.in_first + row] : CLI_STRUCTURE_NONE;
            args->compare->rows += 1u;
        }
        g_cli_structure->hunks[args->compare->hunks] = hunk;
        args->compare->hunks += 1u;
    }
}

EMBED_INLINE unsigned int cli_line_tokens(const StructureCompareCtx *args)
{
    return args->side->line_first[args->line + 1u] - args->side->line_first[args->line];
}

// whether the line beside `line` on either side is moved
EMBED_INLINE int cli_beside_move(const StructureCompareCtx *args)
{
    return ((args->line > 0u) && (args->kinds[args->line - 1u] == CLI_MOVED)) ||
           (((args->line + 1u) < args->count) && (args->kinds[args->line + 1u] == CLI_MOVED));
}

// Moved: a line taken out and one put in elsewhere with the same tokens, matched in order; then a moved line takes
// along a changed line beside it with tokens, where one beside a moved line on the other side contains the same tokens:
// a block's closing sign moves with the block
EMBED_INLINE void cli_moved_find(const StructureCompareCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = g_cli_structure->out_used, .val = 0, .bytes = args->compare->outs});
    for (unsigned int in = 0u; in < args->compare->ins; in += 1u)
    {
        const unsigned int line = g_cli_structure->ins[in];
        for (unsigned int out = 0u; (EMBED_CALL(cli_line_tokens, StructureCompareCtx, .side = &g_cli_structure->new,
                                                .line = line) >= CLI_MOVED_TOKENS) &&
                                    (out < args->compare->outs);
             out += 1u)
        {
            const unsigned int from = g_cli_structure->outs[out];
            if (!g_cli_structure->out_used[out] &&
                (EMBED_CALL(cli_line_tokens, StructureCompareCtx, .side = &g_cli_structure->old, .line = from) >=
                 CLI_MOVED_TOKENS) &&
                (g_cli_structure->old.line_name[from] == g_cli_structure->new.line_name[line]))
            {
                g_cli_structure->out_used[out] = 1u;
                g_cli_structure->left[from] = CLI_MOVED;
                g_cli_structure->right[line] = CLI_MOVED;
                args->compare->moved += 1u;
                break;
            }
        }
    }
    int grown = 1;
    while (grown)
    {
        grown = 0;
        for (unsigned int out = 0u; out < args->compare->outs; out += 1u)
        {
            const unsigned int from = g_cli_structure->outs[out];
            g_cli_structure->out_used[out] = !(
                (g_cli_structure->left[from] != CLI_MOVED) &&
                (EMBED_CALL(cli_line_tokens, StructureCompareCtx, .side = &g_cli_structure->old, .line = from) != 0u) &&
                EMBED_CALL(cli_beside_move, StructureCompareCtx, .kinds = g_cli_structure->left,
                           .count = g_cli_structure->old.lines, .line = from));
        }
        for (unsigned int in = 0u; in < args->compare->ins; in += 1u)
        {
            const unsigned int line = g_cli_structure->ins[in];
            if ((g_cli_structure->right[line] == CLI_MOVED) ||
                (EMBED_CALL(cli_line_tokens, StructureCompareCtx, .side = &g_cli_structure->new, .line = line) == 0u) ||
                !EMBED_CALL(cli_beside_move, StructureCompareCtx, .kinds = g_cli_structure->right,
                            .count = g_cli_structure->new.lines, .line = line))
            {
                continue;
            }
            for (unsigned int out = 0u; out < args->compare->outs; out += 1u)
            {
                const unsigned int from = g_cli_structure->outs[out];
                if (!g_cli_structure->out_used[out] &&
                    (g_cli_structure->old.line_name[from] == g_cli_structure->new.line_name[line]))
                {
                    g_cli_structure->out_used[out] = 1u;
                    g_cli_structure->left[from] = CLI_MOVED;
                    g_cli_structure->right[line] = CLI_MOVED;
                    args->compare->moved += 1u;
                    grown = 1;
                    break;
                }
            }
        }
    }
}

// a run of tokens taken out and put in ended: retained for its marks, and its tokens paired where they are as many and
// all names
EMBED_INLINE void cli_flush(const StructureCompareCtx *args)
{
    const unsigned int out_count = args->compare->runs_out - args->run_out;
    const unsigned int in_count = args->compare->runs_in - args->run_in;
    int named = out_count == in_count;
    for (unsigned int at = 0u; named && (at < out_count); at += 1u)
    {
        named = EMBED_CALL(cli_structure_side_internal.starts_name, CliStructureSideCtx,
                           .side_read = &g_cli_structure->old, .token = g_cli_structure->run_out[args->run_out + at]) &&
                EMBED_CALL(cli_structure_side_internal.starts_name, CliStructureSideCtx,
                           .side_read = &g_cli_structure->new, .token = g_cli_structure->run_in[args->run_in + at]);
    }
    for (unsigned int at = 0u; named && (at < out_count); at += 1u)
    {
        g_cli_structure->pair_x[args->compare->pairs] = g_cli_structure->run_out[args->run_out + at];
        g_cli_structure->pair_y[args->compare->pairs] = g_cli_structure->run_in[args->run_in + at];
        args->compare->pairs += 1u;
    }
    const CliHunk touched = {args->run_out, out_count, args->run_in, in_count};
    g_cli_structure->touched[args->compare->touched] = touched;
    args->compare->touched += 1u;
}

// What is left of each change, token by token: 1, or 0 where a change is too far apart
EMBED_INLINE int cli_tokens_compare(const StructureCompareCtx *args)
{
    for (unsigned int each = 0u; each < args->compare->hunks; each += 1u)
    {
        const CliHunk *const hunk = &g_cli_structure->hunks[each];
        unsigned int outs = 0u;
        unsigned int ins = 0u;
        for (unsigned int at = 0u; at < hunk->out_count; at += 1u)
        {
            const unsigned int line = g_cli_structure->outs[hunk->out_first + at];
            for (unsigned int token = g_cli_structure->old.line_first[line];
                 (g_cli_structure->left[line] != CLI_MOVED) && (token < g_cli_structure->old.line_first[line + 1u]);
                 token += 1u)
            {
                g_cli_structure->out_tokens[outs] = token;
                g_cli_structure->a[outs] = g_cli_structure->old.token_name[token];
                outs += 1u;
            }
        }
        for (unsigned int at = 0u; at < hunk->in_count; at += 1u)
        {
            const unsigned int line = g_cli_structure->ins[hunk->in_first + at];
            for (unsigned int token = g_cli_structure->new.line_first[line];
                 (g_cli_structure->right[line] != CLI_MOVED) && (token < g_cli_structure->new.line_first[line + 1u]);
                 token += 1u)
            {
                g_cli_structure->in_tokens[ins] = token;
                g_cli_structure->b[ins] = g_cli_structure->new.token_name[token];
                ins += 1u;
            }
        }
        const unsigned int steps = EMBED_CALL(cli_structure_a_internal.shortest, CliStructureACtx,
                                              .a = g_cli_structure->a, .n = outs, .b = g_cli_structure->b, .m = ins,
                                              .steps = g_cli_structure->token_steps, .room = CLI_STRUCTURE_STEPS);
        if (steps == CLI_STRUCTURE_NONE)
        {
            return 0;
        }
        unsigned int run_out = args->compare->runs_out;
        unsigned int run_in = args->compare->runs_in;
        for (unsigned int at = 0u; at < steps; at += 1u)
        {
            const CliStep *const step = &g_cli_structure->token_steps[at];
            if (step->kind == CLI_STEP_OUT)
            {
                g_cli_structure->run_out[args->compare->runs_out] = g_cli_structure->out_tokens[step->x];
                args->compare->runs_out += 1u;
            }
            else if (step->kind == CLI_STEP_IN)
            {
                g_cli_structure->run_in[args->compare->runs_in] = g_cli_structure->in_tokens[step->y];
                args->compare->runs_in += 1u;
            }
            else if ((args->compare->runs_out != run_out) || (args->compare->runs_in != run_in))
            {
                EMBED_CALL(cli_flush, StructureCompareCtx, .compare = args->compare, .run_out = run_out,
                           .run_in = run_in);
                run_out = args->compare->runs_out;
                run_in = args->compare->runs_in;
            }
        }
        if ((args->compare->runs_out != run_out) || (args->compare->runs_in != run_in))
        {
            EMBED_CALL(cli_flush, StructureCompareCtx, .compare = args->compare, .run_out = run_out, .run_in = run_in);
        }
    }
    return 1;
}

// two marks in order: by line, then from, then to, then kind
static int cli_mark_order(const void *one, const void *other)
{
    const unsigned int *const left = (const unsigned int *)one;
    const unsigned int *const right = (const unsigned int *)other;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (left[at] != right[at])
        {
            return (left[at] < right[at]) ? -1 : 1;
        }
    }
    return 0;
}

void cli_structure_compare(const CliStructureCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    g_cli_structure = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_structure), &g_cli_code_cells);
    if (g_cli_structure == NULL)
    {
        mmgr_line_put(args->out, "null");
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return;
    }
    CliCompare compare;
    memor.set(&(MemoriaCfg){.dst = &compare, .val = 0, .bytes = sizeof(compare)});
    const int read = EMBED_CALL(cli_structure_text_internal.lex, CliStructureTextCtx, .text = args->then,
                                .size = args->then_size, .side = &g_cli_structure->old) &&
                     EMBED_CALL(cli_structure_text_internal.lex, CliStructureTextCtx, .text = args->now,
                                .size = args->now_size, .side = &g_cli_structure->new) &&
                     EMBED_CALL(cli_structure_old_internal.by_name, CliStructureOldCtx, .old = &g_cli_structure->old,
                                .now = &g_cli_structure->new);
    const unsigned int steps = read ? EMBED_CALL(cli_structure_a_internal.shortest, CliStructureACtx,
                                                 .a = g_cli_structure->old.line_name, .n = g_cli_structure->old.lines,
                                                 .b = g_cli_structure->new.line_name, .m = g_cli_structure->new.lines,
                                                 .steps = g_cli_structure->line_steps, .room = CLI_STRUCTURE_STEPS)
                                    : CLI_STRUCTURE_NONE;
    int near = steps != CLI_STRUCTURE_NONE;
    if (near)
    {
        EMBED_CALL(cli_hunks_find, StructureCompareCtx, .compare = &compare, .steps = steps);
        memor.set(&(MemoriaCfg){.dst = g_cli_structure->left, .val = CLI_SAME, .bytes = g_cli_structure->old.lines});
        memor.set(&(MemoriaCfg){.dst = g_cli_structure->right, .val = CLI_SAME, .bytes = g_cli_structure->new.lines});
        EMBED_CALL(cli_moved_find, StructureCompareCtx, .compare = &compare);
        EMBED_CALL(cli_structure_internal.moved_mark, CliStructureCtx, .compare = &compare);
        near = EMBED_CALL(cli_tokens_compare, StructureCompareCtx, .compare = &compare);
    }
    if (!near)
    {
        mmgr_line_put(args->out, "null");
        g_cli_cells->persistent_buf_release(g_cli_structure, &g_cli_code_cells);
        g_cli_structure = NULL;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return;
    }
    const unsigned int renames = EMBED_CALL(cli_structure_compare_renames_internal.renames_find,
                                            CliStructureCompareRenamesCtx, .compare_read = &compare);
    EMBED_CALL(cli_structure_internal.touched_mark, CliStructureCtx, .compare = &compare);
    EMBED_CALL(cli_structure_internal.lines_kind, CliStructureCtx, .compare = &compare);
    qsort(g_cli_structure->left_marks, compare.left_marks, sizeof(g_cli_structure->left_marks[0]), cli_mark_order);
    qsort(g_cli_structure->right_marks, compare.right_marks, sizeof(g_cli_structure->right_marks[0]), cli_mark_order);
    EMBED_CALL(cli_structure_compare_renames_internal.write, CliStructureCompareRenamesCtx, .compare_read = &compare,
               .renames = renames, .out = args->out);
    g_cli_cells->persistent_buf_release(g_cli_structure, &g_cli_code_cells);
    g_cli_structure = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}
