// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diff.c: the lines of a text that differ from an older text of it, as the page's own diff marks them, step for step:
// added, changed, and where lines were taken out. The lines the two share at their start and end are set aside first,
// compared a block of units at a time, and Myers' shortest edit runs on what is left, each line read as its hash, its
// runs of the same lines found a block of hashes at a time and each line of a run checked against the other's, so
// that two lines that hash alike and differ end the run. Past CLI_MOST_EDITS the texts read as too far apart to mark
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
#define CLI_MOST_EDITS 3000ll
#define CLI_DIFF_LINES (1u << 20u)

// The diff's room, laid out in a cell sized to the two texts' lines, taken with the diff's lock and given back with it:
// the hashes of each side's lines, the furthest reach on each diagonal, the steps found, and the lines marked and the
// hunks; and each round's reaches, the trace, in a cell of its own at their most
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
    unsigned int puts;
} CliDiff;

static CliDiff s_room;
static CliDiff *s_diff = &s_room;

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
// `bytes` of the room's cell from `*at` for an array, the start moved past them on an eight-byte boundary
EMBED_INLINE void *diff_part(const DiffCtx *args)
{
    unsigned long long *at = args->at;
    void *const part = s_diff->cell + *at;
    *at += (args->bytes + 7ull) & ~7ull;
    return part;
}

// the lock taken and the room with it for `then_lines` lines against `now_lines`: 1, or 0 with the lock given back
// where the cells cannot meet it; and the room given back with the lock
EMBED_INLINE int diff_reserve(const DiffCtx *args)
{
    const unsigned long long both = args->then_lines + args->now_lines + 1ull;
    const unsigned long long most =
        (both < (unsigned long long)CLI_MOST_EDITS) ? both : (unsigned long long)CLI_MOST_EDITS;
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

// whether the trace, taken at the rounds the texts can take, has room for round `round`
EMBED_INLINE int diff_trace_room(const DiffCtx *args)
{
    (void)args;
    return s_diff->trace != NULL;
}

// a line's hash: its units four at a time, each word multiplied and turned in, then its length
EMBED_INLINE unsigned long long cli_hash_get(const DiffCtx *args)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    unsigned long long at = 0ull;
    for (; (at + 4ull) <= args->length; at += 4ull)
    {
        const unsigned long long packed =
            (unsigned long long)args->line[at] | ((unsigned long long)args->line[at + 1ull] << 16u) |
            ((unsigned long long)args->line[at + 2ull] << 32u) | ((unsigned long long)args->line[at + 3ull] << 48u);
        hash = (hash ^ packed) * 0x9e3779b97f4a7c15ull;
        hash = (hash << 29u) | (hash >> 35u);
    }
    for (; at < args->length; at += 1ull)
    {
        hash = (hash ^ args->line[at]) * 0x9e3779b97f4a7c15ull;
        hash = (hash << 29u) | (hash >> 35u);
    }
    return (hash ^ args->length) * 0xff51afd7ed558ccdull;
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

// a step put: 0 retains a line of `a`, 1 takes it out, 2 puts a line of `b` in
EMBED_INLINE void cli_step(const DiffCtx *args)
{
    // the steps are at most the lines of both, which the room contains, and an index is below 2^32
    s_diff->step_kind[s_diff->steps] = args->kind;
    s_diff->step_index[s_diff->steps] = (unsigned int)args->index;
    s_diff->steps += 1u;
}

// the shortest edit from `a` to `b`, its steps in order into the room: 1, or 0 past CLI_MOST_EDITS
EMBED_INLINE int cli_shortest(const DiffCtx *args)
{
    const long long n = args->runs->n;
    const long long m = args->runs->m;
    for (long long at = 0; at < n; at += 1)
    {
        const unsigned long long line = args->runs->a_from + (unsigned long long)at;
        s_diff->hash_then[at] =
            EMBED_CALL(cli_hash_get, DiffCtx, .line = args->runs->a->unit + args->runs->a->start[line],
                       .length = args->runs->a->length[line]);
    }
    for (long long at = 0; at < m; at += 1)
    {
        const unsigned long long line = args->runs->b_from + (unsigned long long)at;
        s_diff->hash_now[at] =
            EMBED_CALL(cli_hash_get, DiffCtx, .line = args->runs->b->unit + args->runs->b->start[line],
                       .length = args->runs->b->length[line]);
    }
    const long long most = ((n + m) < CLI_MOST_EDITS) ? (n + m) : CLI_MOST_EDITS;
    const long long middle = most + 1;
    // every round's frontier, the rounds no more than `most`, in one cell of their squares' bound
    const unsigned long long trace_bytes = (unsigned long long)((most + 1) * (most + 1)) * sizeof(long long);
    s_diff->trace = g_cli_cells->temporary_buf_alloc(trace_bytes, &g_cli_text_cells);
    s_diff->trace_rounds = (s_diff->trace != NULL) ? (most + 1) : 0;
    memor.set(
        &(MemoriaCfg){.dst = s_diff->v, .val = 0, .bytes = (unsigned long long)((2 * most) + 3) * sizeof(long long)});
    long long found = -1;
    for (long long d = 0; (found < 0) && (d <= most); d += 1)
    {
        if (!EMBED_CALL(diff_trace_room, DiffCtx, .round = d))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_diff->trace + (d * d),
                                .src = s_diff->v + (middle - d),
                                .bytes = (unsigned long long)((2 * d) + 1) * sizeof(long long)});
        for (long long k = -d; k <= d; k += 2)
        {
            const int down = (k == -d) || ((k != d) && (s_diff->v[middle + k - 1] < s_diff->v[middle + k + 1]));
            long long x = down ? s_diff->v[middle + k + 1] : (s_diff->v[middle + k - 1] + 1);
            long long y = x - k;
            if ((x < n) && (y < m))
            {
                const long long same = EMBED_CALL(cli_run, DiffCtx, .runs = args->runs, .x = x, .y = y);
                x += same;
                y += same;
            }
            s_diff->v[middle + k] = x;
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
    s_diff->steps = 0u;
    long long x = n;
    long long y = m;
    for (long long d = found; d >= 1; d -= 1)
    {
        const long long *const band = s_diff->trace + (d * d);
        const long long k = x - y;
        const int down = (k == -d) || ((k != d) && (band[k - 1 + d] < band[k + 1 + d]));
        const long long back = down ? (k + 1) : (k - 1);
        const long long back_x = band[back + d];
        const long long back_y = back_x - back;
        while ((x > back_x) && (y > back_y))
        {
            x -= 1;
            y -= 1;
            EMBED_CALL(cli_step, DiffCtx, .kind = 0u, .index = x);
        }
        if (x == back_x)
        {
            y -= 1;
            EMBED_CALL(cli_step, DiffCtx, .kind = 2u, .index = y);
        }
        else
        {
            x -= 1;
            EMBED_CALL(cli_step, DiffCtx, .kind = 1u, .index = x);
        }
    }
    while ((x > 0) && (y > 0))
    {
        x -= 1;
        y -= 1;
        EMBED_CALL(cli_step, DiffCtx, .kind = 0u, .index = x);
    }
    // the steps were found from the end
    for (unsigned int at = 0u; at < (s_diff->steps / 2u); at += 1u)
    {
        const unsigned int other = s_diff->steps - 1u - at;
        const unsigned char kind = s_diff->step_kind[at];
        const unsigned int index = s_diff->step_index[at];
        s_diff->step_kind[at] = s_diff->step_kind[other];
        s_diff->step_index[at] = s_diff->step_index[other];
        s_diff->step_kind[other] = kind;
        s_diff->step_index[other] = index;
    }
    return 1;
}

// the lines put in and taken out since the last unchanged line closed: changed where some were taken out, added where
// none were, a line taken out marked where none were put in, and their hunk recorded
EMBED_INLINE void cli_close(const DiffCtx *args)
{
    unsigned int *out = args->out;
    for (unsigned int at = 0u; at < s_diff->puts; at += 1u)
    {
        if (*out > 0u)
        {
            s_diff->changed[s_diff->changeds] = s_diff->put[at];
            s_diff->changeds += 1u;
        }
        else
        {
            s_diff->added[s_diff->addeds] = s_diff->put[at];
            s_diff->addeds += 1u;
        }
    }
    if ((s_diff->puts == 0u) && (*out > 0u))
    {
        s_diff->removed[s_diff->removeds] = args->line_u32;
        s_diff->removeds += 1u;
    }
    if ((s_diff->puts != 0u) || (*out > 0u))
    {
        s_diff->hunk[s_diff->hunks][0] = args->line_u32 - s_diff->puts;
        s_diff->hunk[s_diff->hunks][1] = args->line_u32;
        s_diff->hunk[s_diff->hunks][2] = args->then_line - *out;
        s_diff->hunk[s_diff->hunks][3] = args->then_line;
        s_diff->hunks += 1u;
    }
    *out = 0u;
    s_diff->puts = 0u;
}

// a list of line numbers written as a JSON array
EMBED_INLINE void cli_numbers_write(const DiffCtx *args)
{
    mmgr_line_ch(args->out_verba_linea_pointer, '[');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(args->out_verba_linea_pointer, (at != 0u) ? "," : "");
        mmgr_line_decimal(args->out_verba_linea_pointer, args->numbers[at], 0u);
    }
    mmgr_line_ch(args->out_verba_linea_pointer, ']');
}

int cli_diff_line_changes(const CliDocumentOutThenCfg *args)
{
    if ((args->then->lines > CLI_DIFF_LINES) || (args->now->lines > CLI_DIFF_LINES) ||
        !EMBED_CALL(diff_reserve, DiffCtx, .then_lines = args->then->lines, .now_lines = args->now->lines))
    {
        return 0;
    }
    unsigned int start = 0u;
    while ((start < args->then->lines) && (start < args->now->lines) &&
           EMBED_CALL(cli_same_line, DiffCtx, .a = args->then, .one = start, .b = args->now, .other = start))
    {
        start += 1u;
    }
    unsigned int end = 0u;
    while ((end < (args->then->lines - start)) && (end < (args->now->lines - start)) &&
           EMBED_CALL(cli_same_line, DiffCtx, .a = args->then, .one = args->then->lines - 1u - end, .b = args->now,
                      .other = args->now->lines - 1u - end))
    {
        end += 1u;
    }
    const CliRuns runs = {args->then,
                          args->now,
                          start,
                          start,
                          (long long)(args->then->lines - start - end),
                          (long long)(args->now->lines - start - end)};
    if (!EMBED_CALL(cli_shortest, DiffCtx, .runs = &runs))
    {
        diff_release();
        return 0;
    }
    s_diff->addeds = 0u;
    s_diff->changeds = 0u;
    s_diff->removeds = 0u;
    s_diff->hunks = 0u;
    s_diff->puts = 0u;
    unsigned int line = start;
    unsigned int then_line = start;
    unsigned int taken = 0u;
    for (unsigned int at = 0u; at < s_diff->steps; at += 1u)
    {
        if (s_diff->step_kind[at] == 0u)
        {
            EMBED_CALL(cli_close, DiffCtx, .line_u32 = line, .then_line = then_line, .out = &taken);
            line += 1u;
            then_line += 1u;
        }
        else if (s_diff->step_kind[at] == 1u)
        {
            taken += 1u;
            then_line += 1u;
        }
        else
        {
            s_diff->put[s_diff->puts] = line;
            s_diff->puts += 1u;
            line += 1u;
        }
    }
    EMBED_CALL(cli_close, DiffCtx, .line_u32 = line, .then_line = then_line, .out = &taken);
    mmgr_line_put(args->out, "{\"added\":");
    EMBED_CALL(cli_numbers_write, DiffCtx, .out_verba_linea_pointer = args->out, .numbers = s_diff->added,
               .count = s_diff->addeds);
    mmgr_line_put(args->out, ",\"changed\":");
    EMBED_CALL(cli_numbers_write, DiffCtx, .out_verba_linea_pointer = args->out, .numbers = s_diff->changed,
               .count = s_diff->changeds);
    mmgr_line_put(args->out, ",\"removed\":");
    EMBED_CALL(cli_numbers_write, DiffCtx, .out_verba_linea_pointer = args->out, .numbers = s_diff->removed,
               .count = s_diff->removeds);
    mmgr_line_put(args->out, ",\"hunks\":[");
    for (unsigned int at = 0u; at < s_diff->hunks; at += 1u)
    {
        const unsigned int *const hunk = s_diff->hunk[at];
        mmgr_line_put(args->out, (at != 0u) ? ",{\"now\":" : "{\"now\":");
        EMBED_CALL(cli_numbers_write, DiffCtx, .out_verba_linea_pointer = args->out, .numbers = hunk, .count = 2u);
        mmgr_line_put(args->out, ",\"then\":");
        EMBED_CALL(cli_numbers_write, DiffCtx, .out_verba_linea_pointer = args->out, .numbers = hunk + 2, .count = 2u);
        if (hunk[3] > hunk[2])
        {
            mmgr_line_put(args->out, ",\"thenLines\":[");
            for (unsigned int one = hunk[2]; one < hunk[3]; one += 1u)
            {
                mmgr_line_put(args->out, (one != hunk[2]) ? "," : "");
                EMBED_CALL(cli_document_units_count.units_write, CliDocumentUnitsCountCfg, .out = args->out,
                           .units = args->then->unit + args->then->start[one], .count = args->then->length[one]);
            }
            mmgr_line_ch(args->out, ']');
        }
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_put(args->out, "]}");
    diff_release();
    return 1;
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
