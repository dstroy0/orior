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

// The changes: each run of lines taken out and put in between lines kept, with the row it starts at
static void cli_hunks_found(CliCompare *compare, unsigned int steps)
{
    unsigned int at = 0u;
    while (at < steps)
    {
        if (g_cli_structure->line_steps[at].kind == CLI_STEP_KEEP)
        {
            g_cli_structure->row_left[compare->rows] = g_cli_structure->line_steps[at].x;
            g_cli_structure->row_right[compare->rows] = g_cli_structure->line_steps[at].y;
            compare->rows += 1u;
            at += 1u;
            continue;
        }
        CliHunk hunk = {compare->outs, 0u, compare->ins, 0u};
        while ((at < steps) && (g_cli_structure->line_steps[at].kind != CLI_STEP_KEEP))
        {
            if (g_cli_structure->line_steps[at].kind == CLI_STEP_OUT)
            {
                g_cli_structure->outs[compare->outs] = g_cli_structure->line_steps[at].x;
                compare->outs += 1u;
                hunk.out_count += 1u;
            }
            else
            {
                g_cli_structure->ins[compare->ins] = g_cli_structure->line_steps[at].y;
                compare->ins += 1u;
                hunk.in_count += 1u;
            }
            at += 1u;
        }
        g_cli_structure->starts[compare->starts] = compare->rows;
        compare->starts += 1u;
        const unsigned int count = (hunk.out_count > hunk.in_count) ? hunk.out_count : hunk.in_count;
        for (unsigned int row = 0u; row < count; row += 1u)
        {
            g_cli_structure->row_left[compare->rows] =
                (row < hunk.out_count) ? g_cli_structure->outs[hunk.out_first + row] : CLI_STRUCTURE_NONE;
            g_cli_structure->row_right[compare->rows] =
                (row < hunk.in_count) ? g_cli_structure->ins[hunk.in_first + row] : CLI_STRUCTURE_NONE;
            compare->rows += 1u;
        }
        g_cli_structure->hunks[compare->hunks] = hunk;
        compare->hunks += 1u;
    }
}

static unsigned int cli_line_tokens(const CliSide *side, unsigned int line)
{
    return side->line_first[line + 1u] - side->line_first[line];
}

// whether the line beside `line` on either side is moved
static int cli_beside_moved(const unsigned char *kinds, unsigned int count, unsigned int line)
{
    return ((line > 0u) && (kinds[line - 1u] == CLI_MOVED)) ||
           (((line + 1u) < count) && (kinds[line + 1u] == CLI_MOVED));
}

// Moved: a line taken out and one put in elsewhere with the same tokens, matched in order; then a moved line takes
// along a changed line beside it with tokens, where one beside a moved line on the other side holds the same tokens:
// a block's closing sign moves with the block
static void cli_moved_found(CliCompare *compare)
{
    memor.set(&(MemoriaCfg){.dst = g_cli_structure->out_used, .val = 0, .bytes = compare->outs});
    for (unsigned int in = 0u; in < compare->ins; in += 1u)
    {
        const unsigned int line = g_cli_structure->ins[in];
        for (unsigned int out = 0u;
             (cli_line_tokens(&g_cli_structure->new, line) >= CLI_MOVED_TOKENS) && (out < compare->outs); out += 1u)
        {
            const unsigned int from = g_cli_structure->outs[out];
            if (!g_cli_structure->out_used[out] && (cli_line_tokens(&g_cli_structure->old, from) >= CLI_MOVED_TOKENS) &&
                (g_cli_structure->old.line_name[from] == g_cli_structure->new.line_name[line]))
            {
                g_cli_structure->out_used[out] = 1u;
                g_cli_structure->left[from] = CLI_MOVED;
                g_cli_structure->right[line] = CLI_MOVED;
                compare->moved += 1u;
                break;
            }
        }
    }
    int grown = 1;
    while (grown)
    {
        grown = 0;
        for (unsigned int out = 0u; out < compare->outs; out += 1u)
        {
            const unsigned int from = g_cli_structure->outs[out];
            g_cli_structure->out_used[out] =
                !((g_cli_structure->left[from] != CLI_MOVED) && (cli_line_tokens(&g_cli_structure->old, from) != 0u) &&
                  cli_beside_moved(g_cli_structure->left, g_cli_structure->old.lines, from));
        }
        for (unsigned int in = 0u; in < compare->ins; in += 1u)
        {
            const unsigned int line = g_cli_structure->ins[in];
            if ((g_cli_structure->right[line] == CLI_MOVED) || (cli_line_tokens(&g_cli_structure->new, line) == 0u) ||
                !cli_beside_moved(g_cli_structure->right, g_cli_structure->new.lines, line))
            {
                continue;
            }
            for (unsigned int out = 0u; out < compare->outs; out += 1u)
            {
                const unsigned int from = g_cli_structure->outs[out];
                if (!g_cli_structure->out_used[out] &&
                    (g_cli_structure->old.line_name[from] == g_cli_structure->new.line_name[line]))
                {
                    g_cli_structure->out_used[out] = 1u;
                    g_cli_structure->left[from] = CLI_MOVED;
                    g_cli_structure->right[line] = CLI_MOVED;
                    compare->moved += 1u;
                    grown = 1;
                    break;
                }
            }
        }
    }
}

// a run of tokens taken out and put in ended: kept for its marks, and its tokens paired where they are as many and
// all names
static void cli_flushed(CliCompare *compare, unsigned int run_out, unsigned int run_in)
{
    const unsigned int out_count = compare->runs_out - run_out;
    const unsigned int in_count = compare->runs_in - run_in;
    int named = out_count == in_count;
    for (unsigned int at = 0u; named && (at < out_count); at += 1u)
    {
        named = cli_structure_starts_name(&g_cli_structure->old, g_cli_structure->run_out[run_out + at]) &&
                cli_structure_starts_name(&g_cli_structure->new, g_cli_structure->run_in[run_in + at]);
    }
    for (unsigned int at = 0u; named && (at < out_count); at += 1u)
    {
        g_cli_structure->pair_x[compare->pairs] = g_cli_structure->run_out[run_out + at];
        g_cli_structure->pair_y[compare->pairs] = g_cli_structure->run_in[run_in + at];
        compare->pairs += 1u;
    }
    const CliHunk touched = {run_out, out_count, run_in, in_count};
    g_cli_structure->touched[compare->touched] = touched;
    compare->touched += 1u;
}

// What is left of each change, token by token: 1, or 0 where a change is too far apart
static int cli_tokens_compared(CliCompare *compare)
{
    for (unsigned int each = 0u; each < compare->hunks; each += 1u)
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
        const unsigned int steps = cli_structure_shortest(g_cli_structure->a, outs, g_cli_structure->b, ins,
                                                          g_cli_structure->token_steps, CLI_STRUCTURE_STEPS);
        if (steps == CLI_STRUCTURE_NONE)
        {
            return 0;
        }
        unsigned int run_out = compare->runs_out;
        unsigned int run_in = compare->runs_in;
        for (unsigned int at = 0u; at < steps; at += 1u)
        {
            const CliStep *const step = &g_cli_structure->token_steps[at];
            if (step->kind == CLI_STEP_OUT)
            {
                g_cli_structure->run_out[compare->runs_out] = g_cli_structure->out_tokens[step->x];
                compare->runs_out += 1u;
            }
            else if (step->kind == CLI_STEP_IN)
            {
                g_cli_structure->run_in[compare->runs_in] = g_cli_structure->in_tokens[step->y];
                compare->runs_in += 1u;
            }
            else if ((compare->runs_out != run_out) || (compare->runs_in != run_in))
            {
                cli_flushed(compare, run_out, run_in);
                run_out = compare->runs_out;
                run_in = compare->runs_in;
            }
        }
        if ((compare->runs_out != run_out) || (compare->runs_in != run_in))
        {
            cli_flushed(compare, run_out, run_in);
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

void cli_structure_compare(const char *then, unsigned long long then_size, const char *now, unsigned long long now_size,
                           CliLine *out)
{
    cli_lock_take(&s_lock);
    g_cli_structure = cli_cell_taken(sizeof(*g_cli_structure), "a compare by structure");
    if (g_cli_structure == NULL)
    {
        cli_line_text(out, "null");
        cli_lock_give(&s_lock);
        return;
    }
    CliCompare compare;
    memor.set(&(MemoriaCfg){.dst = &compare, .val = 0, .bytes = sizeof(compare)});
    const int read = cli_structure_lex(then, then_size, &g_cli_structure->old) &&
                     cli_structure_lex(now, now_size, &g_cli_structure->new) &&
                     cli_structure_named(&g_cli_structure->old, &g_cli_structure->new);
    const unsigned int steps = read ? cli_structure_shortest(g_cli_structure->old.line_name, g_cli_structure->old.lines,
                                                             g_cli_structure->new.line_name, g_cli_structure->new.lines,
                                                             g_cli_structure->line_steps, CLI_STRUCTURE_STEPS)
                                    : CLI_STRUCTURE_NONE;
    int near = steps != CLI_STRUCTURE_NONE;
    if (near)
    {
        cli_hunks_found(&compare, steps);
        memor.set(&(MemoriaCfg){.dst = g_cli_structure->left, .val = CLI_SAME, .bytes = g_cli_structure->old.lines});
        memor.set(&(MemoriaCfg){.dst = g_cli_structure->right, .val = CLI_SAME, .bytes = g_cli_structure->new.lines});
        cli_moved_found(&compare);
        cli_structure_moved_marked(&compare);
        near = cli_tokens_compared(&compare);
    }
    if (!near)
    {
        cli_line_text(out, "null");
        cli_cell_given(g_cli_structure);
        g_cli_structure = NULL;
        cli_lock_give(&s_lock);
        return;
    }
    const unsigned int renames = cli_structure_renames_found(&compare);
    cli_structure_touched_marked(&compare);
    cli_structure_lines_kinded(&compare);
    qsort(g_cli_structure->left_marks, compare.left_marks, sizeof(g_cli_structure->left_marks[0]), cli_mark_order);
    qsort(g_cli_structure->right_marks, compare.right_marks, sizeof(g_cli_structure->right_marks[0]), cli_mark_order);
    cli_structure_written(&compare, renames, out);
    cli_cell_given(g_cli_structure);
    g_cli_structure = NULL;
    cli_lock_give(&s_lock);
}
