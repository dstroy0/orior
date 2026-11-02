// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape_match.c: the match of a pattern's parts against the code's tokens. A word matches a token of its kind and its
// text, a statement's end any statement's end; a placeholder bound already matches the same tokens again, and one not
// bound tries each end it may reach, in order, until the rest matches, its brackets and its statements' ends read by
// the roles the tokens' grammar names. A match from one place takes at most
// CLI_SHAPE_STEPS steps before it is given up. The ends tried are stored on the matcher's stack, CLI_SHAPE_ENDS at the
// most, those past it not tried
#include "shape_internal.h"

// the most steps a match from one place may take, the most ends stored to be tried at once, and the ends the stack is
// taken with
#define CLI_SHAPE_STEPS 20000u
#define CLI_SHAPE_ENDS (1u << 20u)

void cli_shape_ends_free(const CliShapeMatcherCtx *args)
{
    g_cli_cells->persistent_buf_release(args->matcher->ends, &g_cli_shape_cells);
    args->matcher->ends = NULL;
    args->matcher->ends_top = 0u;
    args->matcher->ends_room = 0u;
}

typedef struct
{
    const CliShapeMatcher *matcher;
    unsigned int at;
    unsigned long long *size;
    CliShapeMatcher *matcher_out;
    unsigned int end;
    unsigned int hole;
    int last;
    unsigned int bound;
    unsigned int *end_pointer;
} ShapeMatchCtx;
// the text of the code's token at `at`, its size into `size`
EMBED_INLINE const char *match_text(const ShapeMatchCtx *args)
{
    unsigned long long *size = args->size;
    *size = args->matcher->tokens->end[args->at] - args->matcher->tokens->start[args->at];
    return args->matcher->src + args->matcher->tokens->start[args->at];
}

// how much a token opens or closes a bracket, as its roles name it
EMBED_INLINE int match_depth_get(const ShapeMatchCtx *args)
{
    const unsigned char role = args->matcher->tokens->role[args->at];
    return ((role & CLI_SHAPE_OPENS) != 0u) ? 1 : ((role & CLI_SHAPE_CLOSES) != 0u) ? -1 : 0;
}

// whether the code's token at `at` ends a statement where it stands outside brackets
EMBED_INLINE int match_ends_statement(const ShapeMatchCtx *args)
{
    return (args->matcher->tokens->kind[args->at] == CLI_SHAPE_BREAK) ||
           ((args->matcher->tokens->role[args->at] & CLI_SHAPE_STATEMENT) != 0u);
}

// an end put on the stack where it fits: the stack one cell, taken as its first end is put, of a token's end for each
// part of the pattern at the most, the end not put where it is full or the cellblock cannot meet it
EMBED_INLINE void match_end_put(const ShapeMatchCtx *args)
{
    if (args->matcher_out->ends_room == 0u)
    {
        const unsigned long long wanted = ((unsigned long long)args->matcher_out->tokens->count + 1ull) *
                                          ((unsigned long long)args->matcher_out->part_count + 1ull);
        const unsigned int room = (wanted < CLI_SHAPE_ENDS) ? (unsigned int)wanted : CLI_SHAPE_ENDS;
        args->matcher_out->ends =
            g_cli_cells->temporary_buf_alloc(room * sizeof(*args->matcher_out->ends), &g_cli_shape_cells);
        args->matcher_out->ends_room = (args->matcher_out->ends != NULL) ? room : 0u;
    }
    if (args->matcher_out->ends_top < args->matcher_out->ends_room)
    {
        args->matcher_out->ends[args->matcher_out->ends_top] = args->end;
        args->matcher_out->ends_top += 1u;
    }
}

// the ends after `at` a placeholder of `hole` may reach, in the order they are tried, put on the stack
EMBED_INLINE void match_ends(const ShapeMatchCtx *args)
{
    const unsigned int first = args->matcher_out->ends_top;
    long long depth = 0;
    unsigned int at_end = args->at;
    for (; at_end < args->matcher_out->tokens->count; at_end += 1u)
    {
        const int change = EMBED_CALL(match_depth_get, ShapeMatchCtx, .matcher = args->matcher_out, .at = at_end);
        if ((change < 0) && (depth == 0))
        {
            break;
        }
        if (args->hole == CLI_SHAPE_HOLE_NAME)
        {
            if (args->matcher_out->tokens->kind[at_end] == CLI_SHAPE_NAME)
            {
                EMBED_CALL(match_end_put, ShapeMatchCtx, .matcher_out = args->matcher_out, .end = at_end + 1u);
            }
            break;
        }
        if (args->hole == CLI_SHAPE_HOLE_EXPR)
        {
            if ((depth == 0) &&
                EMBED_CALL(match_ends_statement, ShapeMatchCtx, .matcher = args->matcher_out, .at = at_end))
            {
                break;
            }
            depth += change;
            if (depth == 0)
            {
                EMBED_CALL(match_end_put, ShapeMatchCtx, .matcher_out = args->matcher_out, .end = at_end + 1u);
            }
            continue;
        }
        depth += change;
        if ((depth == 0) &&
            (EMBED_CALL(match_ends_statement, ShapeMatchCtx, .matcher = args->matcher_out, .at = at_end) ||
             (((args->matcher_out->tokens->role[at_end] & CLI_SHAPE_REGION) != 0u) && (change < 0))))
        {
            EMBED_CALL(match_end_put, ShapeMatchCtx, .matcher_out = args->matcher_out, .end = at_end + 1u);
            if (args->last)
            {
                break;
            }
        }
    }
    if ((args->hole == CLI_SHAPE_HOLE_STMT) && (depth == 0) && (at_end > args->at) &&
        ((args->matcher_out->ends_top == first) ||
         (args->matcher_out->ends[args->matcher_out->ends_top - 1u] != at_end)))
    {
        EMBED_CALL(match_end_put, ShapeMatchCtx, .matcher_out = args->matcher_out, .end = at_end);
    }
    if ((args->hole == CLI_SHAPE_HOLE_EXPR) && args->last)
    {
        for (unsigned int low = first, high = args->matcher_out->ends_top; low + 1u < high; low += 1u, high -= 1u)
        {
            const unsigned int pivot = args->matcher_out->ends[low];
            args->matcher_out->ends[low] = args->matcher_out->ends[high - 1u];
            args->matcher_out->ends[high - 1u] = pivot;
        }
    }
}

unsigned int cli_shape_bind(const CliShapeCtx *args)
{
    for (unsigned int at = 0u; at < args->matcher_read->bound_count; at += 1u)
    {
        const CliShapeHole *const hole = &args->matcher_read->holes->hole[args->matcher_read->bound[at]];
        if ((hole->name_size == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = hole->name, .other = args->name, .bytes = (size_t)args->size}) == 0))
        {
            return at;
        }
    }
    return CLI_SHAPE_HOLES;
}

// whether the tokens from `at` are the same as those a bound placeholder took
EMBED_INLINE int match_same(const ShapeMatchCtx *args)
{
    unsigned int *end = args->end_pointer;
    const unsigned int from = args->matcher->bound_from[args->bound];
    const unsigned int length = args->matcher->bound_to[args->bound] - from;
    if (args->at + length > args->matcher->tokens->count)
    {
        return 0;
    }
    for (unsigned int offset = 0u; offset < length; offset += 1u)
    {
        unsigned long long one_size = 0ull;
        unsigned long long other_size = 0ull;
        const char *const one =
            EMBED_CALL(match_text, ShapeMatchCtx, .matcher = args->matcher, .at = from + offset, .size = &one_size);
        const char *const other = EMBED_CALL(match_text, ShapeMatchCtx, .matcher = args->matcher,
                                             .at = args->at + offset, .size = &other_size);
        if ((one_size != other_size) ||
            (memor.cmp(&(MemoriaCfg){.src = one, .other = other, .bytes = (size_t)one_size}) != 0))
        {
            return 0;
        }
    }
    *end = args->at + length;
    return 1;
}

int cli_shape_from(const CliShapeMatcherCtx *args)
{
    unsigned int *end = args->end;
    args->matcher->steps += 1u;
    if (args->matcher->steps > CLI_SHAPE_STEPS)
    {
        return 0;
    }
    if (args->part == args->matcher->part_count)
    {
        *end = args->at;
        return 1;
    }
    const unsigned int hole_at = args->matcher->hole_of[args->part];
    if (hole_at == CLI_SHAPE_HOLES)
    {
        const unsigned char kind = args->matcher->parts->kind[args->part];
        if (args->at >= args->matcher->tokens->count || (args->matcher->tokens->kind[args->at] != kind))
        {
            return 0;
        }
        const unsigned long long size = args->matcher->parts->end[args->part] - args->matcher->parts->start[args->part];
        unsigned long long own_size = 0ull;
        const char *const own =
            EMBED_CALL(match_text, ShapeMatchCtx, .matcher = args->matcher, .at = args->at, .size = &own_size);
        if ((kind != CLI_SHAPE_BREAK) &&
            ((own_size != size) ||
             (memor.cmp(&(MemoriaCfg){.src = own,
                                      .other = args->matcher->pattern + args->matcher->parts->start[args->part],
                                      .bytes = (size_t)size}) != 0)))
        {
            return 0;
        }
        return EMBED_CALL(cli_shape_matcher_internal.from, CliShapeMatcherCtx, .matcher = args->matcher,
                          .part = args->part + 1u, .at = args->at + 1u, .end = end);
    }
    const CliShapeHole *const hole = &args->matcher->holes->hole[hole_at];
    const unsigned int bound = EMBED_CALL(cli_shape_internal.bind, CliShapeCtx, .matcher_read = args->matcher,
                                          .name = hole->name, .size = hole->name_size);
    if (bound != CLI_SHAPE_HOLES)
    {
        unsigned int after = 0u;
        return EMBED_CALL(match_same, ShapeMatchCtx, .matcher = args->matcher, .bound = bound, .at = args->at,
                          .end_pointer = &after) &&
               EMBED_CALL(cli_shape_matcher_internal.from, CliShapeMatcherCtx, .matcher = args->matcher,
                          .part = args->part + 1u, .at = after, .end = end);
    }
    const unsigned int first = args->matcher->ends_top;
    EMBED_CALL(match_ends, ShapeMatchCtx, .matcher_out = args->matcher, .hole = hole->hole, .at = args->at,
               .last = args->part + 1u == args->matcher->part_count);
    const unsigned int top = args->matcher->ends_top;
    for (unsigned int tried = first; (tried < top) && (args->matcher->bound_count < CLI_SHAPE_HOLES); tried += 1u)
    {
        const unsigned int end_at = args->matcher->ends[tried];
        const unsigned int bound_at = args->matcher->bound_count;
        args->matcher->bound[bound_at] = hole_at;
        args->matcher->bound_from[bound_at] = args->at;
        args->matcher->bound_to[bound_at] = end_at;
        args->matcher->bound_count += 1u;
        if (EMBED_CALL(cli_shape_matcher_internal.from, CliShapeMatcherCtx, .matcher = args->matcher,
                       .part = args->part + 1u, .at = end_at, .end = end))
        {
            args->matcher->ends_top = first;
            return 1;
        }
        args->matcher->bound_count = bound_at;
    }
    args->matcher->ends_top = first;
    return 0;
}
