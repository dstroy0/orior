// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diff.c: the lines of a text that differ from an older text of it, as the changes between the two, each handed to the
// caller as it is found. The lines the two share at their start and end are set aside first, compared a block of units
// at a time, and Myers' shortest edit runs on what is left, each line read as its hash, its runs of the same lines
// found a block of hashes at a time and each line of a run checked against the other's, so that two lines that hash
// alike and differ end the run. The edit's capacity, the hashes, the frontier's band and the steps, is one cell sized
// to the two texts' lines for the one edit, and each round's band is traced in a cell of its own for the walk back;
// both go as the edit ends. Past CLI_DIFF_MAX_EDITS rounds the texts read as too far apart
#include "../command_line/pool.h"
#include "../editor/document.h"
#include "vector.h"

// The two runs a shortest edit compares: the lines of each from where the shared start ends, and how many
typedef struct
{
    const CliLines *a;
    const CliLines *b;
    unsigned long long a_from;
    unsigned long long b_from;
    long long n;
    long long m;
} CliRuns;

// the most rounds a shortest edit takes before the texts read as too far apart, and the most lines of each text
#define CLI_DIFF_MAX_EDITS 3000LL
// the rounds the trace holds room for at first, doubled as a search runs past them
#define CLI_DIFF_ROUNDS_FIRST 64LL
#define CLI_DIFF_LINES (1u << 20u)

// The edit's capacity, laid out in a cell sized to the two texts' lines, taken for the one edit and released as it
// ends: the hashes of each side's lines, the furthest reach on each diagonal, and the steps found; and each round's
// reaches, the trace, in a cell of its own at their most. Each edit has a capacity of its own, so edits on two
// threads run beside each other
typedef struct
{
    unsigned char *cell;
    unsigned long long *hash_then;
    unsigned long long *hash_now;
    long long *v;
    long long *trace;
    unsigned char *step_type;
    unsigned int *step_index;
    unsigned int steps;
} CliDiff;

// a step of the edit: a line of the older text retained, taken out, or a line of the newer put in
enum
{
    DIFF_STEP_EQUAL,
    DIFF_STEP_OUT,
    DIFF_STEP_IN
};

typedef struct
{
    CliDiff *diff;
    unsigned long long *at;
    unsigned long long bytes;
    unsigned long long then_lines;
    unsigned long long now_lines;
    const unsigned short *line;
    unsigned long long length;
    const CliLines *a;
    unsigned long long one;
    const CliLines *b;
    unsigned long long other;
    const CliRuns *runs;
    long long x;
    long long y;
    unsigned char type;
    long long index;
} DiffCtx;

// `bytes` of the capacity's cell from `*at` for an array, the start moved past them on an eight-byte boundary
EMBED_INLINE void *diff_part(const DiffCtx *args)
{
    unsigned long long *at = args->at;
    void *const part = args->diff->cell + *at;
    *at += (args->bytes + 7ULL) & ~7ULL;
    return part;
}

// the capacity taken for `then_lines` lines against `now_lines`: 1, or 0 where the cells cannot meet it
EMBED_INLINE int diff_reserve(const DiffCtx *args)
{
    const unsigned long long both = args->then_lines + args->now_lines;
    const unsigned long long most =
        (both < (unsigned long long)CLI_DIFF_MAX_EDITS) ? both : (unsigned long long)CLI_DIFF_MAX_EDITS;
    const unsigned long long bytes =
        (8ULL * both) + (8ULL * ((2ULL * most) + 3ULL)) + both + (4ULL * both) + (8ULL * 8ULL);
    memor.set(&(MemoriaCfg){.dst = args->diff, .val = 0, .bytes = sizeof(*args->diff)});
    args->diff->cell = g_cli_guard->temporary_buf_alloc(bytes, g_cli_text_owner);
    if (args->diff->cell == NULL)
    {
        return 0;
    }
    unsigned long long at = 0ULL;
    args->diff->hash_then =
        EMBED_CALL(diff_part, DiffCtx, .diff = args->diff, .at = &at, .bytes = 8ULL * args->then_lines);
    args->diff->hash_now =
        EMBED_CALL(diff_part, DiffCtx, .diff = args->diff, .at = &at, .bytes = 8ULL * args->now_lines);
    args->diff->v =
        EMBED_CALL(diff_part, DiffCtx, .diff = args->diff, .at = &at, .bytes = 8ULL * ((2ULL * most) + 3ULL));
    args->diff->step_type = EMBED_CALL(diff_part, DiffCtx, .diff = args->diff, .at = &at, .bytes = both);
    args->diff->step_index = EMBED_CALL(diff_part, DiffCtx, .diff = args->diff, .at = &at, .bytes = 4ULL * both);
    return 1;
}

// a line's hash: its units four at a time, each word multiplied and turned in, then its length
EMBED_INLINE unsigned long long cli_hash_get(const DiffCtx *args)
{
    unsigned long long hash = 0xcbf29ce484222325ULL;
    unsigned long long at = 0ULL;
    for (; (at + 4ULL) <= args->length; at += 4ULL)
    {
        const unsigned long long packed =
            (unsigned long long)args->line[at] | ((unsigned long long)args->line[at + 1ULL] << 16u) |
            ((unsigned long long)args->line[at + 2ULL] << 32u) | ((unsigned long long)args->line[at + 3ULL] << 48u);
        hash = (hash ^ packed) * 0x9e3779b97f4a7c15ULL;
        hash = (hash << 29u) | (hash >> 35u);
    }
    for (; at < args->length; at += 1ULL)
    {
        hash = (hash ^ args->line[at]) * 0x9e3779b97f4a7c15ULL;
        hash = (hash << 29u) | (hash >> 35u);
    }
    return (hash ^ args->length) * 0xff51afd7ed558ccdULL;
}

// whether line `one` of `a` is line `other` of `b`
EMBED_INLINE int cli_same_line(const DiffCtx *args)
{
    const CliLines *a = args->a;
    const CliLines *b = args->b;
    return EMBED_CALL(cli_vector_a.same, CliVectorACfg, .a = a->unit + a->start[args->one],
                      .a_length = a->length[args->one], .b = b->unit + b->start[args->other],
                      .b_length = b->length[args->other]);
}

// how many lines of `a` from `x` and of `b` from `y` are the same, one for one
EMBED_INLINE long long cli_run(const DiffCtx *args)
{
    const unsigned long long alike =
        EMBED_CALL(cli_vector_a_length.common, CliVectorALengthCfg, .a_u64s = args->diff->hash_then + args->x,
                   .a_length = (unsigned long long)(args->runs->n - args->x), .b_u64s = args->diff->hash_now + args->y,
                   .b_length = (unsigned long long)(args->runs->m - args->y));
    unsigned long long at = 0ULL;
    while ((at < alike) && EMBED_CALL(cli_same_line, DiffCtx, .diff = args->diff, .a = args->runs->a,
                                      .one = args->runs->a_from + (unsigned long long)args->x + at, .b = args->runs->b,
                                      .other = args->runs->b_from + (unsigned long long)args->y + at))
    {
        at += 1ULL;
    }
    // a run is at most the lines left, which a long long represents
    return (long long)at;
}

// a step put: the type and the line it concerns
EMBED_INLINE void cli_step(const DiffCtx *args)
{
    // the steps are at most the lines of both, which the capacity contains, and an index is below 2^32
    args->diff->step_type[args->diff->steps] = args->type;
    args->diff->step_index[args->diff->steps] = (unsigned int)args->index;
    args->diff->steps += 1u;
}

// the shortest edit from `a` to `b`, its steps in order into the capacity: 1, or 0 past CLI_DIFF_MAX_EDITS or where the
// cells cannot meet the trace
EMBED_INLINE int cli_shortest(const DiffCtx *args)
{
    const long long n = args->runs->n;
    const long long m = args->runs->m;
    for (long long at = 0; at < n; at += 1)
    {
        const unsigned long long line = args->runs->a_from + (unsigned long long)at;
        args->diff->hash_then[at] =
            EMBED_CALL(cli_hash_get, DiffCtx, .diff = args->diff,
                       .line = args->runs->a->unit + args->runs->a->start[line], .length = args->runs->a->length[line]);
    }
    for (long long at = 0; at < m; at += 1)
    {
        const unsigned long long line = args->runs->b_from + (unsigned long long)at;
        args->diff->hash_now[at] =
            EMBED_CALL(cli_hash_get, DiffCtx, .diff = args->diff,
                       .line = args->runs->b->unit + args->runs->b->start[line], .length = args->runs->b->length[line]);
    }
    const long long most = ((n + m) < CLI_DIFF_MAX_EDITS) ? (n + m) : CLI_DIFF_MAX_EDITS;
    const long long middle = most + 1;
    // every round's frontier in one cell of the square of the rounds it holds room for: CLI_DIFF_ROUNDS_FIRST rounds
    // first, the room doubled as the rounds pass it, the frontiers moved into the larger cell
    long long rounds = (most < CLI_DIFF_ROUNDS_FIRST) ? (most + 1) : CLI_DIFF_ROUNDS_FIRST;
    args->diff->trace =
        g_cli_guard->temporary_buf_alloc((unsigned long long)(rounds * rounds) * sizeof(long long), g_cli_text_owner);
    if (args->diff->trace == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){
        .dst = args->diff->v, .val = 0, .bytes = (unsigned long long)((2 * most) + 3) * sizeof(long long)});
    long long found = -1;
    for (long long d = 0; (found < 0) && (d <= most); d += 1)
    {
        if (d >= rounds)
        {
            const long long wider = ((rounds * 2) < (most + 1)) ? (rounds * 2) : (most + 1);
            long long *const grown = g_cli_guard->temporary_buf_alloc(
                (unsigned long long)(wider * wider) * sizeof(long long), g_cli_text_owner);
            if (grown == NULL)
            {
                return 0;
            }
            // the rounds before `d` stand in the first d * d entries
            memor.cpy(&(MemoriaCfg){
                .dst = grown, .src = args->diff->trace, .bytes = (unsigned long long)(d * d) * sizeof(long long)});
            g_cli_guard->persistent_buf_release(args->diff->trace, g_cli_text_owner);
            args->diff->trace = grown;
            rounds = wider;
        }
        memor.cpy(&(MemoriaCfg){.dst = args->diff->trace + (d * d),
                                .src = args->diff->v + (middle - d),
                                .bytes = (unsigned long long)((2 * d) + 1) * sizeof(long long)});
        for (long long k = -d; k <= d; k += 2)
        {
            const int down = (k == -d) || ((k != d) && (args->diff->v[middle + k - 1] < args->diff->v[middle + k + 1]));
            long long x = down ? args->diff->v[middle + k + 1] : (args->diff->v[middle + k - 1] + 1);
            long long y = x - k;
            if ((x < n) && (y < m))
            {
                const long long same =
                    EMBED_CALL(cli_run, DiffCtx, .diff = args->diff, .runs = args->runs, .x = x, .y = y);
                x += same;
                y += same;
            }
            args->diff->v[middle + k] = x;
            if ((x >= n) && (y >= m))
            {
                found = d;
                break;
            }
        }
    }
    if (found < 0)
    {
        return 0;
    }
    args->diff->steps = 0u;
    long long x = n;
    long long y = m;
    for (long long d = found; d >= 1; d -= 1)
    {
        const long long *const band = args->diff->trace + (d * d);
        const long long k = x - y;
        const int down = (k == -d) || ((k != d) && (band[k - 1 + d] < band[k + 1 + d]));
        const long long back = down ? (k + 1) : (k - 1);
        const long long back_x = band[back + d];
        const long long back_y = back_x - back;
        while ((x > back_x) && (y > back_y))
        {
            x -= 1;
            y -= 1;
            EMBED_CALL(cli_step, DiffCtx, .diff = args->diff, .type = DIFF_STEP_EQUAL, .index = x);
        }
        if (x == back_x)
        {
            y -= 1;
            EMBED_CALL(cli_step, DiffCtx, .diff = args->diff, .type = DIFF_STEP_IN, .index = y);
        }
        else
        {
            x -= 1;
            EMBED_CALL(cli_step, DiffCtx, .diff = args->diff, .type = DIFF_STEP_OUT, .index = x);
        }
    }
    while ((x > 0) && (y > 0))
    {
        x -= 1;
        y -= 1;
        EMBED_CALL(cli_step, DiffCtx, .diff = args->diff, .type = DIFF_STEP_EQUAL, .index = x);
    }
    // the steps were found from the end
    for (unsigned int at = 0u; at < (args->diff->steps / 2u); at += 1u)
    {
        const unsigned int other = args->diff->steps - 1u - at;
        const unsigned char type = args->diff->step_type[at];
        const unsigned int index = args->diff->step_index[at];
        args->diff->step_type[at] = args->diff->step_type[other];
        args->diff->step_index[at] = args->diff->step_index[other];
        args->diff->step_type[other] = type;
        args->diff->step_index[other] = index;
    }
    return 1;
}

int cli_diff_line_changes(const CliDocumentOutThenCfg *args)
{
    CliDiff diff;
    if ((args->then->lines > CLI_DIFF_LINES) || (args->now->lines > CLI_DIFF_LINES) ||
        !EMBED_CALL(diff_reserve, DiffCtx, .diff = &diff, .then_lines = args->then->lines,
                    .now_lines = args->now->lines))
    {
        return 0;
    }
    unsigned int start = 0u;
    while ((start < args->then->lines) && (start < args->now->lines) &&
           EMBED_CALL(cli_same_line, DiffCtx, .diff = &diff, .a = args->then, .one = start, .b = args->now,
                      .other = start))
    {
        start += 1u;
    }
    unsigned int end = 0u;
    while ((end < (args->then->lines - start)) && (end < (args->now->lines - start)) &&
           EMBED_CALL(cli_same_line, DiffCtx, .diff = &diff, .a = args->then, .one = args->then->lines - 1u - end,
                      .b = args->now, .other = args->now->lines - 1u - end))
    {
        end += 1u;
    }
    const CliRuns runs = {args->then,
                          args->now,
                          start,
                          start,
                          (long long)(args->then->lines - start - end),
                          (long long)(args->now->lines - start - end)};
    const int found = EMBED_CALL(cli_shortest, DiffCtx, .diff = &diff, .runs = &runs);
    // the lines put in and taken out since the last line retained, handed on as one change as the next is retained
    unsigned int line = start;
    unsigned int then_line = start;
    unsigned int puts = 0u;
    unsigned int taken = 0u;
    for (unsigned int at = 0u; found && (at <= diff.steps); at += 1u)
    {
        const unsigned char type = (at < diff.steps) ? diff.step_type[at] : (unsigned char)DIFF_STEP_EQUAL;
        if (type == DIFF_STEP_OUT)
        {
            taken += 1u;
            then_line += 1u;
            continue;
        }
        if (type == DIFF_STEP_IN)
        {
            puts += 1u;
            line += 1u;
            continue;
        }
        if ((puts != 0u) || (taken != 0u))
        {
            args->each(&(CliEachHunkCfg){args->context, line - puts, line, then_line - taken, then_line});
        }
        puts = 0u;
        taken = 0u;
        line += (at < diff.steps) ? 1u : 0u;
        then_line += (at < diff.steps) ? 1u : 0u;
    }
    g_cli_guard->persistent_buf_release(diff.trace, g_cli_text_owner);
    g_cli_guard->persistent_buf_release(diff.cell, g_cli_text_owner);
    return found;
}
