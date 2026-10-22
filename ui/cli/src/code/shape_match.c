// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape_match.c: the match of a pattern's parts against the code's tokens. A word matches a token of its kind and its
// text, a statement's end any statement's end; a placeholder bound already matches the same tokens again, and one not
// bound tries each end it may reach, in order, until the rest matches. A match from one place takes at most
// CLI_SHAPE_STEPS steps before it is given up. The ends tried are kept on the matcher's stack, CLI_SHAPE_ENDS at the
// most, those past it not tried
#include "shape_internal.h"

// the most steps a match from one place may take, the most ends kept to be tried at once, and the ends the stack is
// begun with, doubling as they are put
#define CLI_SHAPE_STEPS 20000u
#define CLI_SHAPE_ENDS (1u << 20u)
#define CLI_SHAPE_ENDS_FIRST 64u

void cli_shape_ends_given(CliShapeMatcher *matcher)
{
    cli_cell_given_by(&g_cli_shape_cells, matcher->ends);
    matcher->ends = NULL;
    matcher->ends_top = 0u;
    matcher->ends_room = 0u;
}

// the text of the code's token at `at`, its size into `size`
static const char *match_text(const CliShapeMatcher *matcher, unsigned int at, unsigned long long *size)
{
    *size = matcher->tokens->end[at] - matcher->tokens->start[at];
    return matcher->src + matcher->tokens->start[at];
}

// whether the code's token at `at` is `text`
static int match_is(const CliShapeMatcher *matcher, unsigned int at, const char *text)
{
    unsigned long long size = 0ull;
    const char *const own = match_text(matcher, at, &size);
    return (cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) == size) &&
           (memor.cmp(&(MemoriaCfg){.src = own, .other = text, .bytes = (size_t)size}) == 0);
}

// how much a token opens or closes a bracket
static int match_depth_of(const CliShapeMatcher *matcher, unsigned int at)
{
    unsigned long long size = 0ull;
    const char *const text = match_text(matcher, at, &size);
    if (size != 1u)
    {
        return 0;
    }
    return ((text[0] == '(') || (text[0] == '[') || (text[0] == '{'))   ? 1
           : ((text[0] == ')') || (text[0] == ']') || (text[0] == '}')) ? -1
                                                                        : 0;
}

// whether the code's token at `at` ends a statement where it stands outside brackets
static int match_ends_statement(const CliShapeMatcher *matcher, unsigned int at)
{
    const unsigned char kind = matcher->tokens->kind[at];
    return (kind == CLI_SHAPE_BREAK) || ((kind == CLI_SHAPE_PUNCT) && match_is(matcher, at, ";"));
}

// an end put on the stack where it fits: the stack moved into a cell of twice its room where it is full, below
// CLI_SHAPE_ENDS, the end not put where the cellblock cannot meet it
static void match_end_put(CliShapeMatcher *matcher, unsigned int end)
{
    if ((matcher->ends_top == matcher->ends_room) && (matcher->ends_room < CLI_SHAPE_ENDS))
    {
        const unsigned int room = (matcher->ends_room == 0u) ? CLI_SHAPE_ENDS_FIRST : (matcher->ends_room * 2u);
        unsigned int *const grown =
            cli_cell_taken_by(&g_cli_shape_cells, room * sizeof(*grown), "the ends a match by shape tries");
        if (grown != NULL)
        {
            if (matcher->ends_top != 0u)
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = grown, .src = matcher->ends, .bytes = matcher->ends_top * sizeof(*grown)});
            }
            cli_cell_given_by(&g_cli_shape_cells, matcher->ends);
            matcher->ends = grown;
            matcher->ends_room = room;
        }
    }
    if (matcher->ends_top < matcher->ends_room)
    {
        matcher->ends[matcher->ends_top] = end;
        matcher->ends_top += 1u;
    }
}

// the ends after `at` a placeholder of `hole` may reach, in the order they are tried, put on the stack
static void match_ends(CliShapeMatcher *matcher, unsigned int hole, unsigned int at, int last)
{
    const unsigned int first = matcher->ends_top;
    long long depth = 0;
    unsigned int at_end = at;
    for (; at_end < matcher->tokens->count; at_end += 1u)
    {
        const int change = match_depth_of(matcher, at_end);
        if ((change < 0) && (depth == 0))
        {
            break;
        }
        if (hole == CLI_SHAPE_HOLE_NAME)
        {
            if (matcher->tokens->kind[at_end] == CLI_SHAPE_NAME)
            {
                match_end_put(matcher, at_end + 1u);
            }
            break;
        }
        if (hole == CLI_SHAPE_HOLE_EXPR)
        {
            if ((depth == 0) && match_ends_statement(matcher, at_end))
            {
                break;
            }
            depth += change;
            if (depth == 0)
            {
                match_end_put(matcher, at_end + 1u);
            }
            continue;
        }
        depth += change;
        if ((depth == 0) && (match_ends_statement(matcher, at_end) || (match_is(matcher, at_end, "}") && (change < 0))))
        {
            match_end_put(matcher, at_end + 1u);
            if (last)
            {
                break;
            }
        }
    }
    if ((hole == CLI_SHAPE_HOLE_STMT) && (depth == 0) && (at_end > at) &&
        ((matcher->ends_top == first) || (matcher->ends[matcher->ends_top - 1u] != at_end)))
    {
        match_end_put(matcher, at_end);
    }
    if ((hole == CLI_SHAPE_HOLE_EXPR) && last)
    {
        for (unsigned int low = first, high = matcher->ends_top; low + 1u < high; low += 1u, high -= 1u)
        {
            const unsigned int kept = matcher->ends[low];
            matcher->ends[low] = matcher->ends[high - 1u];
            matcher->ends[high - 1u] = kept;
        }
    }
}

unsigned int cli_shape_bound(const CliShapeMatcher *matcher, const char *name, unsigned long long size)
{
    for (unsigned int at = 0u; at < matcher->bound_count; at += 1u)
    {
        const CliShapeHole *const hole = &matcher->holes->hole[matcher->bound[at]];
        if ((hole->name_size == size) &&
            (memor.cmp(&(MemoriaCfg){.src = hole->name, .other = name, .bytes = (size_t)size}) == 0))
        {
            return at;
        }
    }
    return CLI_SHAPE_HOLES;
}

// whether the tokens from `at` are the same as those a bound placeholder took
static int match_same(const CliShapeMatcher *matcher, unsigned int bound, unsigned int at, unsigned int *end)
{
    const unsigned int from = matcher->bound_from[bound];
    const unsigned int length = matcher->bound_to[bound] - from;
    if (at + length > matcher->tokens->count)
    {
        return 0;
    }
    for (unsigned int offset = 0u; offset < length; offset += 1u)
    {
        unsigned long long one_size = 0ull;
        unsigned long long other_size = 0ull;
        const char *const one = match_text(matcher, from + offset, &one_size);
        const char *const other = match_text(matcher, at + offset, &other_size);
        if ((one_size != other_size) ||
            (memor.cmp(&(MemoriaCfg){.src = one, .other = other, .bytes = (size_t)one_size}) != 0))
        {
            return 0;
        }
    }
    *end = at + length;
    return 1;
}

int cli_shape_from(CliShapeMatcher *matcher, unsigned int part, unsigned int at, unsigned int *end)
{
    matcher->steps += 1u;
    if (matcher->steps > CLI_SHAPE_STEPS)
    {
        return 0;
    }
    if (part == matcher->part_count)
    {
        *end = at;
        return 1;
    }
    const unsigned int hole_at = matcher->hole_of[part];
    if (hole_at == CLI_SHAPE_HOLES)
    {
        const unsigned char kind = matcher->parts->kind[part];
        if (at >= matcher->tokens->count || (matcher->tokens->kind[at] != kind))
        {
            return 0;
        }
        const unsigned long long size = matcher->parts->end[part] - matcher->parts->start[part];
        unsigned long long own_size = 0ull;
        const char *const own = match_text(matcher, at, &own_size);
        if ((kind != CLI_SHAPE_BREAK) &&
            ((own_size != size) ||
             (memor.cmp(&(MemoriaCfg){
                  .src = own, .other = matcher->pattern + matcher->parts->start[part], .bytes = (size_t)size}) != 0)))
        {
            return 0;
        }
        return cli_shape_from(matcher, part + 1u, at + 1u, end);
    }
    const CliShapeHole *const hole = &matcher->holes->hole[hole_at];
    const unsigned int bound = cli_shape_bound(matcher, hole->name, hole->name_size);
    if (bound != CLI_SHAPE_HOLES)
    {
        unsigned int after = 0u;
        return match_same(matcher, bound, at, &after) && cli_shape_from(matcher, part + 1u, after, end);
    }
    const unsigned int first = matcher->ends_top;
    match_ends(matcher, hole->hole, at, part + 1u == matcher->part_count);
    const unsigned int top = matcher->ends_top;
    for (unsigned int tried = first; (tried < top) && (matcher->bound_count < CLI_SHAPE_HOLES); tried += 1u)
    {
        // the stack is read again at each try, since a try may move it into a larger cell
        const unsigned int end_at = matcher->ends[tried];
        const unsigned int bound_at = matcher->bound_count;
        matcher->bound[bound_at] = hole_at;
        matcher->bound_from[bound_at] = at;
        matcher->bound_to[bound_at] = end_at;
        matcher->bound_count += 1u;
        if (cli_shape_from(matcher, part + 1u, end_at, end))
        {
            matcher->ends_top = first;
            return 1;
        }
        matcher->bound_count = bound_at;
    }
    matcher->ends_top = first;
    return 0;
}
