// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_paint.c: each line's class at each column of a parse, painted span by span, a later span over an earlier,
// and read back as runs of one class; the regions of a parse that fold; and the spans Expand Selection steps through
#include "../text/json_tree.h"
#include "inspect_internal.h"

#include <stdlib.h>

// the class of each column of the text, line after line, where each line starts among them and how many it holds,
// and how many lines the text holds
static unsigned char *s_column;
static unsigned long long *s_line_start;
static unsigned long long *s_line_width;
static unsigned int s_lines;

// A fold: its first and last lines
typedef struct
{
    unsigned int first;
    unsigned int last;
} PaintFold;

static PaintFold *s_folds;

void cli_inspect_paint_laid(CliInspectCarve *carve)
{
    const unsigned long long lines = CLI_INSPECT_LINES;
    s_column = cli_inspect_carved(carve, CLI_INSPECT_COLUMNS);
    s_line_start = cli_inspect_carved(carve, lines * sizeof(unsigned long long));
    s_line_width = cli_inspect_carved(carve, lines * sizeof(unsigned long long));
    s_folds = cli_inspect_carved(carve, 4ull * lines * sizeof(PaintFold));
}

void cli_inspect_paint_begun(const char *src, unsigned long long size)
{
    s_lines = 0u;
    unsigned long long columns = 0ull;
    unsigned long long width = 0ull;
    for (unsigned long long at = 0ull; at <= size;)
    {
        if ((at == size) || (src[at] == '\n'))
        {
            if (s_lines < CLI_INSPECT_LINES)
            {
                s_line_start[s_lines] = columns;
                s_line_width[s_lines] = ((columns + width) <= CLI_INSPECT_COLUMNS) ? width : 0ull;
                columns += s_line_width[s_lines];
                s_lines += 1u;
            }
            width = 0ull;
            at += 1u;
            continue;
        }
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)src, size, at, &letter);
        width += (letter >= 0x10000u) ? 2u : 1u;
    }
    memor.set(&(MemoriaCfg){
        .dst = s_column, .val = 0, .bytes = (size_t)((columns < CLI_INSPECT_COLUMNS) ? columns : CLI_INSPECT_COLUMNS)});
}

void cli_inspect_paint(CliInspectPlace from, CliInspectPlace to, unsigned char class_of)
{
    const unsigned int last = ((s_lines > 0u) && (to.line > s_lines - 1u)) ? (s_lines - 1u) : to.line;
    for (unsigned int line = from.line; (s_lines > 0u) && (line <= last); line += 1u)
    {
        const unsigned long long width = s_line_width[line];
        const unsigned long long start = (line == from.line) ? from.col : 0ull;
        const unsigned long long end = (line == to.line) ? ((to.col < width) ? to.col : width) : width;
        const unsigned long long begin = (start < end) ? start : end;
        memor.set(&(MemoriaCfg){
            .dst = s_column + s_line_start[line] + begin, .val = class_of, .bytes = (size_t)(end - begin)});
    }
}

void cli_inspect_paint_runs(CliParsed *parsed)
{
    parsed->runs = 0u;
    parsed->lines = 0u;
    // a parse handed no storage for its lines counts the runs and lines it would hold
    const int kept = (parsed->line_first != NULL);
    for (unsigned int line = 0u; line < s_lines; line += 1u)
    {
        if (parsed->lines == parsed->lines_most)
        {
            parsed->full = 1;
            return;
        }
        if (kept)
        {
            parsed->line_first[parsed->lines] = parsed->runs;
            parsed->line_runs[parsed->lines] = 0u;
        }
        const unsigned char *const columns = s_column + s_line_start[line];
        int has_run = 0;
        unsigned char class_of = 0u;
        for (unsigned long long col = 0ull; col < s_line_width[line]; col += 1u)
        {
            if (has_run && (class_of == columns[col]))
            {
                if (kept)
                {
                    // a column of a line held is at most 16M, which a run's ends count
                    parsed->run[parsed->runs - 1u].to = (unsigned int)col + 1u;
                }
                continue;
            }
            if (parsed->runs == parsed->runs_most)
            {
                parsed->full = 1;
                return;
            }
            if (kept)
            {
                parsed->run[parsed->runs] = (CliInspectRun){(unsigned int)col, (unsigned int)col + 1u, columns[col]};
                parsed->line_runs[parsed->lines] += 1u;
            }
            parsed->runs += 1u;
            has_run = 1;
            class_of = columns[col];
        }
        parsed->lines += 1u;
    }
}

void cli_inspect_fold_put(CliParsed *parsed, unsigned int first, unsigned int last)
{
    if (parsed->folds == parsed->folds_most)
    {
        parsed->full = 1;
        return;
    }
    if (parsed->fold_first != NULL)
    {
        parsed->fold_first[parsed->folds] = first;
        parsed->fold_last[parsed->folds] = last;
    }
    parsed->folds += 1u;
}

void cli_inspect_range_put(CliParsed *parsed, CliInspectPlace from, CliInspectPlace to)
{
    if (parsed->ranges == parsed->ranges_most)
    {
        parsed->full = 1;
        return;
    }
    if (parsed->range != NULL)
    {
        parsed->range[parsed->ranges] = (CliInspectSpan){from, to};
    }
    parsed->ranges += 1u;
}

// two folds in the order of their first lines, then of their last
static int paint_fold_order(const void *one, const void *other)
{
    const PaintFold *const left = (const PaintFold *)one;
    const PaintFold *const right = (const PaintFold *)other;
    if (left->first != right->first)
    {
        return (left->first < right->first) ? -1 : 1;
    }
    return (left->last < right->last) ? -1 : (left->last > right->last) ? 1 : 0;
}

unsigned int cli_inspect_folds_of(unsigned int *first, unsigned int *last, unsigned int count)
{
    unsigned int kept = 0u;
    for (unsigned int at = 0u; (at < count) && (kept < (4u * CLI_INSPECT_LINES)); at += 1u)
    {
        if (last[at] > first[at])
        {
            s_folds[kept] = (PaintFold){first[at], last[at]};
            kept += 1u;
        }
    }
    qsort(s_folds, kept, sizeof(s_folds[0]), paint_fold_order);
    unsigned int out = 0u;
    for (unsigned int at = 0u; at < kept; at += 1u)
    {
        if ((out > 0u) && (first[out - 1u] == s_folds[at].first))
        {
            last[out - 1u] = s_folds[at].last;
            continue;
        }
        first[out] = s_folds[at].first;
        last[out] = s_folds[at].last;
        out += 1u;
    }
    return out;
}

void cli_inspect_folds_done(CliParsed *parsed)
{
    // a parse that counts keeps the folds put as the most it holds
    if (parsed->fold_first != NULL)
    {
        parsed->folds = cli_inspect_folds_of(parsed->fold_first, parsed->fold_last, parsed->folds);
    }
}

// whether `one` stands at or before `other`
static int paint_before(CliInspectPlace one, CliInspectPlace other)
{
    return (one.line < other.line) || ((one.line == other.line) && (one.col <= other.col));
}

// two spans in the order Expand Selection steps through them: the later start first, then the earlier end
static int paint_range_order(const void *one, const void *other)
{
    const CliInspectSpan *const left = (const CliInspectSpan *)one;
    const CliInspectSpan *const right = (const CliInspectSpan *)other;
    if ((left->from.line != right->from.line) || (left->from.col != right->from.col))
    {
        return paint_before(left->from, right->from) ? 1 : -1;
    }
    if ((left->to.line != right->to.line) || (left->to.col != right->to.col))
    {
        return paint_before(left->to, right->to) ? -1 : 1;
    }
    return 0;
}

unsigned int cli_inspect_ranges_at(const CliInspectSpan *ranges, unsigned int count, CliInspectPlace from,
                                   CliInspectPlace to, CliInspectSpan *out, unsigned int most)
{
    unsigned int kept = 0u;
    for (unsigned int at = 0u; (at < count) && (kept < most); at += 1u)
    {
        const CliInspectSpan span = ranges[at];
        const int same = (span.from.line == from.line) && (span.from.col == from.col) && (span.to.line == to.line) &&
                         (span.to.col == to.col);
        if (paint_before(span.from, from) && paint_before(to, span.to) && !same)
        {
            out[kept] = span;
            kept += 1u;
        }
    }
    qsort(out, kept, sizeof(out[0]), paint_range_order);
    unsigned int unique = 0u;
    for (unsigned int at = 0u; at < kept; at += 1u)
    {
        if ((unique == 0u) || (paint_range_order(&out[unique - 1u], &out[at]) != 0))
        {
            out[unique] = out[at];
            unique += 1u;
        }
    }
    return unique;
}
