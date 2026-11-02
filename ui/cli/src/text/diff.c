// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diff.c: the lines of a text that differ from an older text of it, as the page's own diff marks them, step for step:
// added, changed, and where lines were taken out. The lines the two share at their start and end are set aside first,
// compared a block of units at a time, and Myers' shortest edit runs on what is left, each line read as its hash, its
// runs of the same lines found a block of hashes at a time and each line of a run checked against the other's, so
// that two lines that hash alike and differ end the run. Past CLI_MAX_EDITS the texts read as too far apart to mark
#include "../command_line/cells.h"
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
// the most edits a shortest edit takes before the texts read as too far apart, and the most lines of each text
#define CLI_MAX_EDITS 3000ll
#define CLI_DIFF_LINES (1u << 20u)

// The diff's capacity, laid out in a cell sized to the two texts' lines, taken with the diff's lock and given back with
// it: the hashes of each side's lines, the furthest reach on each diagonal, the steps found, and the lines marked and
// the hunks; and each round's reaches, the trace, in a cell of its own at their most
typedef struct
{
    unsigned char *cell;
    unsigned long long *hash_then;
    unsigned long long *hash_now;
    long long *v;
    long long *trace;
    long long trace_rounds;
    unsigned char *step_kind;
    unsigned int *step_index;
    unsigned int steps;
    unsigned int *added;
    unsigned int addeds;
    unsigned int *changed;
    unsigned int changeds;
    unsigned int *removed;
    unsigned int removeds;
    unsigned int (*hunk)[4];
    unsigned int hunks;
    unsigned int *put;
    unsigned int appends;
} CliDiff;

static CliDiff s_capacity;
static CliDiff *s_diff = &s_capacity;

typedef struct
{
    unsigned long long *at;
    unsigned long long bytes;
    unsigned long long then_lines;
    unsigned long long now_lines;
    long long round;
    const unsigned short *line;
    unsigned long long length;
    const CliLines *a;
    unsigned long long one;
    const CliLines *b;
    unsigned long long other;
    const CliRuns *runs;
    long long x;
    long long y;
    unsigned char kind;
    long long index;
    unsigned int line_u32;
    unsigned int then_line;
    unsigned int *out;
    VerbaLinea *out_verba_linea_pointer;
    const unsigned int *numbers;
    unsigned int count;
} DiffCtx;
// `bytes` of the capacity's cell from `*at` for an array, the start moved past them on an eight-byte boundary
EMBED_INLINE void *diff_part(const DiffCtx *args)
{
    unsigned long long *at = args->at;
    void *const part = s_diff->cell + *at;
    *at += (args->bytes + 7ull) & ~7ull;
    return part;
}

// the lock taken and the capacity with it for `then_lines` lines against `now_lines`: 1, or 0 with the lock given back
// where the cells cannot meet it; and the capacity given back with the lock
EMBED_INLINE int diff_reserve(const DiffCtx *args)
{
    const unsigned long long both = args->then_lines + args->now_lines + 1ull;
    const unsigned long long most =
        (both < (unsigned long long)CLI_MAX_EDITS) ? both : (unsigned long long)CLI_MAX_EDITS;
    const unsigned long long bytes = (8ull * (args->then_lines + args->now_lines + (2ull * most) + 3ull)) +
                                     (5ull * both) + (4ull * (args->now_lines * 3ull + args->then_lines)) +
                                     (16ull * both) + (8ull * 12ull);
    memor.set(&(MemoriaCfg){.dst = s_diff, .val = 0, .bytes = sizeof(*s_diff)});
    s_diff->cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_text_cells);
    if (s_diff->cell == NULL)
    {
        return 0;
    }
    unsigned long long at = 0ull;
    s_diff->hash_then = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = 8ull * args->then_lines);
    s_diff->hash_now = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = 8ull * args->now_lines);
    s_diff->v = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = 8ull * ((2ull * most) + 3ull));
    s_diff->step_kind = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = both);
    s_diff->step_index = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = 4ull * both);
    s_diff->added = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = 4ull * args->now_lines);
    s_diff->changed = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = 4ull * args->now_lines);
    s_diff->removed = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = 4ull * args->then_lines);
    s_diff->hunk = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = 16ull * both);
    s_diff->put = EMBED_CALL(diff_part, DiffCtx, .at = &at, .bytes = 4ull * args->now_lines);
    return 1;
}

static void diff_release(void)
{
    g_cli_cells->persistent_buf_release(s_diff->trace, &g_cli_text_cells);
    g_cli_cells->persistent_buf_release(s_diff->cell, &g_cli_text_cells);
    memor.set(&(MemoriaCfg){.dst = s_diff, .val = 0, .bytes = sizeof(*s_diff)});
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
        EMBED_CALL(cli_vector_a_length.common, CliVectorALengthCfg, .a_u64s = s_diff->hash_then + args->x,
                   .a_length = (unsigned long long)(args->runs->n - args->x), .b_u64s = s_diff->hash_now + args->y,
                   .b_length = (unsigned long long)(args->runs->m - args->y));
    unsigned long long at = 0ull;
    while ((at < alike) && EMBED_CALL(cli_same_line, DiffCtx, .a = args->runs->a,
                                      .one = args->runs->a_from + (unsigned long long)args->x + at, .b = args->runs->b,
                                      .other = args->runs->b_from + (unsigned long long)args->y + at))
    {
        at += 1ull;
    }
    // a run is at most the lines left, which a long long represents
    return (long long)at;
}

long long cli_diff_run_get(const CliDocumentThenCfg *args)
{
    // the lines of each are below CLI_DIFF_LINES, which a long long represents
    const CliRuns runs = {args->then, args->now, 0ull, 0ull, (long long)args->then->lines, (long long)args->now->lines};
    if ((args->then->lines > CLI_DIFF_LINES) || (args->now->lines > CLI_DIFF_LINES) ||
        !EMBED_CALL(diff_reserve, DiffCtx, .then_lines = args->then->lines, .now_lines = args->now->lines))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->then->lines; at += 1u)
    {
        s_diff->hash_then[at] = args->hash_then[at];
    }
    for (unsigned int at = 0u; at < args->now->lines; at += 1u)
    {
        s_diff->hash_now[at] = args->hash_now[at];
    }
    const long long run = EMBED_CALL(cli_run, DiffCtx, .runs = &runs, .x = args->x, .y = args->y);
    diff_release();
    return run;
}
