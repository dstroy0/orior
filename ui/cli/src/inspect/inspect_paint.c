// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_paint.c: each line's class at each column of a parse, painted span by span, a later span over an earlier,
// and read back as runs of one class; the regions of a parse that fold; and the spans Expand Selection steps through
#include "../text/json_tree.h"
#include "../text/sort.h"
#include "inspect_internal.h"

// the class of each column of the text, line after line, where each line starts among them and how many it contains,
// and how many lines the text contains
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

void cli_inspect_paint_layout(const CliInspectPartitionCtx *args)
{
    const unsigned long long lines = CLI_INSPECT_LINES;
    s_column = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                          .partition = args->partition, .bytes = CLI_INSPECT_COLUMNS);
    s_line_start = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                              .partition = args->partition, .bytes = lines * sizeof(unsigned long long));
    s_line_width = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                              .partition = args->partition, .bytes = lines * sizeof(unsigned long long));
    s_folds = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                         .bytes = 4ULL * lines * sizeof(PaintFold));
}

void cli_inspect_paint_begin(const CliInspectSizeSrcParsedCtx *args)
{
    s_lines = 0u;
    unsigned long long columns = 0ULL;
    unsigned long long width = 0ULL;
    for (unsigned long long at = 0ULL; at <= args->size;)
    {
        if ((at == args->size) || (args->src[at] == '\n'))
        {
            if (s_lines < CLI_INSPECT_LINES)
            {
                s_line_start[s_lines] = columns;
                s_line_width[s_lines] = ((columns + width) <= CLI_INSPECT_COLUMNS) ? width : 0ULL;
                columns += s_line_width[s_lines];
                s_lines += 1u;
            }
            width = 0ULL;
            at += 1u;
            continue;
        }
        unsigned int letter = 0u;
        at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                         .text_bytes = (const unsigned char *)args->src, .length = args->size, .at = at,
                         .letter = &letter);
        width += (letter >= 0x10000u) ? 2u : 1u;
    }
    memor.set(&(MemoriaCfg){.dst = s_column,
                            .val = 0,
                            .bytes = (embed_word)((columns < CLI_INSPECT_COLUMNS) ? columns : CLI_INSPECT_COLUMNS)});
}

void cli_inspect_paint(const CliInspectFromToCtx *args)
{
    const unsigned int last = ((s_lines > 0u) && (args->to.line > s_lines - 1u)) ? (s_lines - 1u) : args->to.line;
    for (unsigned int line = args->from.line; (s_lines > 0u) && (line <= last); line += 1u)
    {
        const unsigned long long width = s_line_width[line];
        const unsigned long long start = (line == args->from.line) ? args->from.col : 0ULL;
        unsigned long long end;
        if (line == args->to.line)
        {
            if (args->to.col < width)
            {
                end = args->to.col;
            }
            else
            {
                end = width;
            }
        }
        else
        {
            end = width;
        }
        const unsigned long long begin = (start < end) ? start : end;
        memor.set(&(MemoriaCfg){
            .dst = s_column + s_line_start[line] + begin, .val = args->class_of, .bytes = (embed_word)(end - begin)});
    }
}

void cli_inspect_paint_runs(const CliInspectSizeSrcParsedCtx *args)
{
    args->parsed->runs = 0u;
    args->parsed->lines = 0u;
    // a parse handed no storage for its lines counts the runs and lines it would contain
    const int has_lines = (args->parsed->line_first != NULL);
    for (unsigned int line = 0u; line < s_lines; line += 1u)
    {
        if (args->parsed->lines == args->parsed->lines_max)
        {
            args->parsed->full = 1;
            return;
        }
        if (has_lines)
        {
            args->parsed->line_first[args->parsed->lines] = args->parsed->runs;
            args->parsed->line_runs[args->parsed->lines] = 0u;
        }
        const unsigned char *const columns = s_column + s_line_start[line];
        int has_run = 0;
        unsigned char class_of = 0u;
        for (unsigned long long col = 0ULL; col < s_line_width[line]; col += 1u)
        {
            const int extends = has_run && (class_of == columns[col]);
            if (extends && has_lines)
            {
                // a column of a line stored is at most 16M, which a run's ends count
                args->parsed->run[args->parsed->runs - 1u].to = (unsigned int)col + 1u;
            }
            if (extends)
            {
                continue;
            }
            if (args->parsed->runs == args->parsed->runs_max)
            {
                args->parsed->full = 1;
                return;
            }
            if (has_lines)
            {
                args->parsed->run[args->parsed->runs] =
                    (CliInspectRun){(unsigned int)col, (unsigned int)col + 1u, columns[col]};
                args->parsed->line_runs[args->parsed->lines] += 1u;
            }
            args->parsed->runs += 1u;
            has_run = 1;
            class_of = columns[col];
        }
        args->parsed->lines += 1u;
    }
}

void cli_inspect_fold_append(const CliInspectParsedCtx *args)
{
    if (args->parsed->folds == args->parsed->folds_max)
    {
        args->parsed->full = 1;
        return;
    }
    if (args->parsed->fold_first != NULL)
    {
        args->parsed->fold_first[args->parsed->folds] = args->first;
        args->parsed->fold_last[args->parsed->folds] = args->last;
    }
    args->parsed->folds += 1u;
}

void cli_inspect_range_append(const CliInspectFromToParsedCtx *args)
{
    if (args->parsed->ranges == args->parsed->ranges_max)
    {
        args->parsed->full = 1;
        return;
    }
    if (args->parsed->range != NULL)
    {
        args->parsed->range[args->parsed->ranges] = (CliInspectSpan){args->from, args->to};
    }
    args->parsed->ranges += 1u;
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
    if (left->last < right->last)
    {
        return -1;
    }
    if (left->last > right->last)
    {
        return 1;
    }
    return 0;
}

unsigned int cli_inspect_folds_get(const CliInspectCountCfg *args)
{
    unsigned int fold_count = 0u;
    for (unsigned int at = 0u; (at < args->count) && (fold_count < (4u * CLI_INSPECT_LINES)); at += 1u)
    {
        if (args->last[at] > args->first[at])
        {
            s_folds[fold_count] = (PaintFold){args->first[at], args->last[at]};
            fold_count += 1u;
        }
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_folds, .count = fold_count, .size = sizeof(s_folds[0]),
               .order = paint_fold_order);
    unsigned int out = 0u;
    for (unsigned int at = 0u; at < fold_count; at += 1u)
    {
        if ((out > 0u) && (args->first[out - 1u] == s_folds[at].first))
        {
            args->last[out - 1u] = s_folds[at].last;
            continue;
        }
        args->first[out] = s_folds[at].first;
        args->last[out] = s_folds[at].last;
        out += 1u;
    }
    return out;
}

void cli_inspect_folds_finish(const CliInspectSizeSrcParsedCtx *args)
{
    // a parse that counts records the folds put as the most it contains
    if (args->parsed->fold_first != NULL)
    {
        args->parsed->folds =
            EMBED_CALL(cli_inspect_count.folds_get, CliInspectCountCfg, .first = args->parsed->fold_first,
                       .last = args->parsed->fold_last, .count = args->parsed->folds);
    }
}

typedef struct
{
    CliInspectPosition one;
    CliInspectPosition other;
} InspectPaintCtx;
// whether `one` stands at or before `other`
EMBED_INLINE int paint_before(const InspectPaintCtx *args)
{
    return (args->one.line < args->other.line) ||
           ((args->one.line == args->other.line) && (args->one.col <= args->other.col));
}

// two spans in the order Expand Selection steps through them: the later start first, then the earlier end
static int paint_range_order(const void *one, const void *other)
{
    const CliInspectSpan *const left = (const CliInspectSpan *)one;
    const CliInspectSpan *const right = (const CliInspectSpan *)other;
    if ((left->from.line != right->from.line) || (left->from.col != right->from.col))
    {
        return EMBED_CALL(paint_before, InspectPaintCtx, .one = left->from, .other = right->from) ? 1 : -1;
    }
    if ((left->to.line != right->to.line) || (left->to.col != right->to.col))
    {
        return EMBED_CALL(paint_before, InspectPaintCtx, .one = left->to, .other = right->to) ? -1 : 1;
    }
    return 0;
}

unsigned int cli_inspect_ranges_at(const CliInspectRangesCfg *args)
{
    unsigned int range_count = 0u;
    for (unsigned int at = 0u; (at < args->count) && (range_count < args->most); at += 1u)
    {
        const CliInspectSpan span = args->ranges_read[at];
        const int same = (span.from.line == args->from.line) && (span.from.col == args->from.col) &&
                         (span.to.line == args->to.line) && (span.to.col == args->to.col);
        if (EMBED_CALL(paint_before, InspectPaintCtx, .one = span.from, .other = args->from) &&
            EMBED_CALL(paint_before, InspectPaintCtx, .one = args->to, .other = span.to) && !same)
        {
            args->out_inspect_span_pointer[range_count] = span;
            range_count += 1u;
        }
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = args->out_inspect_span_pointer, .count = range_count,
               .size = sizeof(args->out_inspect_span_pointer[0]), .order = paint_range_order);
    unsigned int unique = 0u;
    for (unsigned int at = 0u; at < range_count; at += 1u)
    {
        if ((unique == 0u) ||
            (paint_range_order(&args->out_inspect_span_pointer[unique - 1u], &args->out_inspect_span_pointer[at]) != 0))
        {
            args->out_inspect_span_pointer[unique] = args->out_inspect_span_pointer[at];
            unique += 1u;
        }
    }
    return unique;
}
