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

// the most edits a shortest edit takes before the texts read as too far apart, and the most lines of each text
#define CLI_MOST_EDITS 3000ll
#define CLI_DIFF_LINES (1u << 20u)

// The diff's room, laid out in a cell sized to the two texts' lines, taken with the diff's lock and given back with
// it: the hashes of each side's lines, the furthest reach on each diagonal, the steps found, and the lines marked and
// the hunks; and each round's reaches, the trace, in a cell of its own grown as the rounds go
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

// the rounds the trace first holds, doubled as the rounds go past them
#define CLI_DIFF_TRACE_FIRST 64ll

static CliDiff s_room;
static CliDiff *s_diff = &s_room;
static CliLock s_lock = CLI_LOCK_INIT;

// `bytes` of the room's cell from `*at` for an array, the start moved past them on an eight-byte boundary
static void *diff_part(unsigned long long *at, unsigned long long bytes)
{
    void *const part = s_diff->cell + *at;
    *at += (bytes + 7ull) & ~7ull;
    return part;
}

// the lock taken and the room with it for `then_lines` lines against `now_lines`: 1, or 0 with the lock given back
// where the cells cannot meet it; and the room given back with the lock
static int diff_held(unsigned long long then_lines, unsigned long long now_lines)
{
    cli_lock_take(&s_lock);
    const unsigned long long both = then_lines + now_lines + 1ull;
    const unsigned long long most =
        (both < (unsigned long long)CLI_MOST_EDITS) ? both : (unsigned long long)CLI_MOST_EDITS;
    const unsigned long long bytes = (8ull * (then_lines + now_lines + (2ull * most) + 3ull)) + (5ull * both) +
                                     (4ull * (now_lines * 3ull + then_lines)) + (16ull * both) + (8ull * 12ull);
    memor.set(&(MemoriaCfg){.dst = s_diff, .val = 0, .bytes = sizeof(*s_diff)});
    s_diff->cell = cli_cell_taken(bytes, "a diff's room");
    if (s_diff->cell == NULL)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    unsigned long long at = 0ull;
    s_diff->hash_then = diff_part(&at, 8ull * then_lines);
    s_diff->hash_now = diff_part(&at, 8ull * now_lines);
    s_diff->v = diff_part(&at, 8ull * ((2ull * most) + 3ull));
    s_diff->step_kind = diff_part(&at, both);
    s_diff->step_index = diff_part(&at, 4ull * both);
    s_diff->added = diff_part(&at, 4ull * now_lines);
    s_diff->changed = diff_part(&at, 4ull * now_lines);
    s_diff->removed = diff_part(&at, 4ull * then_lines);
    s_diff->hunk = diff_part(&at, 16ull * both);
    s_diff->put = diff_part(&at, 4ull * now_lines);
    return 1;
}

static void diff_released(void)
{
    cli_cell_given(s_diff->trace);
    cli_cell_given(s_diff->cell);
    memor.set(&(MemoriaCfg){.dst = s_diff, .val = 0, .bytes = sizeof(*s_diff)});
    cli_lock_give(&s_lock);
}

// the trace given room for round `round`, doubled in a new cell where it has none: 1, or 0 where the cells cannot
// meet it
static int diff_trace_room(long long round)
{
    if (round < s_diff->trace_rounds)
    {
        return 1;
    }
    long long rounds = (s_diff->trace_rounds > 0) ? s_diff->trace_rounds : CLI_DIFF_TRACE_FIRST;
    while (rounds <= round)
    {
        rounds *= 2;
    }
    rounds = (rounds < (CLI_MOST_EDITS + 1)) ? rounds : (CLI_MOST_EDITS + 1);
    // a round is at most CLI_MOST_EDITS, so the rounds squared fit an unsigned long long
    long long *const grown =
        cli_cell_taken((unsigned long long)(rounds * rounds) * sizeof(long long), "a diff's trace");
    if (grown == NULL)
    {
        return 0;
    }
    if (s_diff->trace != NULL)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = grown, .src = s_diff->trace, .bytes = (unsigned long long)(round * round) * sizeof(long long)});
    }
    cli_cell_given(s_diff->trace);
    s_diff->trace = grown;
    s_diff->trace_rounds = rounds;
    return 1;
}

// a line's hash: its units four at a time, each word multiplied and turned in, then its length
static unsigned long long cli_hash_of(const unsigned short *line, unsigned long long length)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    unsigned long long at = 0ull;
    for (; (at + 4ull) <= length; at += 4ull)
    {
        const unsigned long long packed = (unsigned long long)line[at] | ((unsigned long long)line[at + 1ull] << 16u) |
                                          ((unsigned long long)line[at + 2ull] << 32u) |
                                          ((unsigned long long)line[at + 3ull] << 48u);
        hash = (hash ^ packed) * 0x9e3779b97f4a7c15ull;
        hash = (hash << 29u) | (hash >> 35u);
    }
    for (; at < length; at += 1ull)
    {
        hash = (hash ^ line[at]) * 0x9e3779b97f4a7c15ull;
        hash = (hash << 29u) | (hash >> 35u);
    }
    return (hash ^ length) * 0xff51afd7ed558ccdull;
}

// whether line `one` of `a` is line `other` of `b`
static int cli_same_line(const CliLines *a, unsigned long long one, const CliLines *b, unsigned long long other)
{
    return cli_vector_same(a->unit + a->start[one], a->length[one], b->unit + b->start[other], b->length[other]);
}

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

// how many lines of `a` from `x` and of `b` from `y` are the same, one for one
static long long cli_run(const CliRuns *runs, long long x, long long y)
{
    const unsigned long long alike = cli_vector_common(s_diff->hash_then + x, (unsigned long long)(runs->n - x),
                                                       s_diff->hash_now + y, (unsigned long long)(runs->m - y));
    unsigned long long at = 0ull;
    while ((at < alike) && cli_same_line(runs->a, runs->a_from + (unsigned long long)x + at, runs->b,
                                         runs->b_from + (unsigned long long)y + at))
    {
        at += 1ull;
    }
    // a run is at most the lines left, which a long long holds
    return (long long)at;
}

// a step put: 0 keeps a line of `a`, 1 takes it out, 2 puts a line of `b` in
static void cli_step(unsigned char kind, long long index)
{
    // the steps are at most the lines of both, which the room holds, and an index is below 2^32
    s_diff->step_kind[s_diff->steps] = kind;
    s_diff->step_index[s_diff->steps] = (unsigned int)index;
    s_diff->steps += 1u;
}

// the shortest edit from `a` to `b`, its steps in order into the room: 1, or 0 past CLI_MOST_EDITS
static int cli_shortest(const CliRuns *runs)
{
    const long long n = runs->n;
    const long long m = runs->m;
    for (long long at = 0; at < n; at += 1)
    {
        const unsigned long long line = runs->a_from + (unsigned long long)at;
        s_diff->hash_then[at] = cli_hash_of(runs->a->unit + runs->a->start[line], runs->a->length[line]);
    }
    for (long long at = 0; at < m; at += 1)
    {
        const unsigned long long line = runs->b_from + (unsigned long long)at;
        s_diff->hash_now[at] = cli_hash_of(runs->b->unit + runs->b->start[line], runs->b->length[line]);
    }
    const long long most = ((n + m) < CLI_MOST_EDITS) ? (n + m) : CLI_MOST_EDITS;
    const long long middle = most + 1;
    memor.set(
        &(MemoriaCfg){.dst = s_diff->v, .val = 0, .bytes = (unsigned long long)((2 * most) + 3) * sizeof(long long)});
    long long found = -1;
    for (long long d = 0; (found < 0) && (d <= most); d += 1)
    {
        if (!diff_trace_room(d))
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
                const long long same = cli_run(runs, x, y);
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
            cli_step(0u, x);
        }
        if (x == back_x)
        {
            y -= 1;
            cli_step(2u, y);
        }
        else
        {
            x -= 1;
            cli_step(1u, x);
        }
    }
    while ((x > 0) && (y > 0))
    {
        x -= 1;
        y -= 1;
        cli_step(0u, x);
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

// the lines put in and taken out since the last kept line closed: changed where some were taken out, added where none
// were, a line taken out marked where none were put in, and their hunk kept
static void cli_closed(unsigned int line, unsigned int then_line, unsigned int *out)
{
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
        s_diff->removed[s_diff->removeds] = line;
        s_diff->removeds += 1u;
    }
    if ((s_diff->puts != 0u) || (*out > 0u))
    {
        s_diff->hunk[s_diff->hunks][0] = line - s_diff->puts;
        s_diff->hunk[s_diff->hunks][1] = line;
        s_diff->hunk[s_diff->hunks][2] = then_line - *out;
        s_diff->hunk[s_diff->hunks][3] = then_line;
        s_diff->hunks += 1u;
    }
    *out = 0u;
    s_diff->puts = 0u;
}

// a list of line numbers written as a JSON array
static void cli_numbers_written(CliLine *out, const unsigned int *numbers, unsigned int count)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_line_decimal(out, numbers[at], 0u);
    }
    cli_line_character(out, ']');
}

int cli_diff_line_changes(const CliLines *then, const CliLines *now, CliLine *out)
{
    if ((then->lines > CLI_DIFF_LINES) || (now->lines > CLI_DIFF_LINES) || !diff_held(then->lines, now->lines))
    {
        return 0;
    }
    unsigned int start = 0u;
    while ((start < then->lines) && (start < now->lines) && cli_same_line(then, start, now, start))
    {
        start += 1u;
    }
    unsigned int end = 0u;
    while ((end < (then->lines - start)) && (end < (now->lines - start)) &&
           cli_same_line(then, then->lines - 1u - end, now, now->lines - 1u - end))
    {
        end += 1u;
    }
    const CliRuns runs = {
        then, now, start, start, (long long)(then->lines - start - end), (long long)(now->lines - start - end)};
    if (!cli_shortest(&runs))
    {
        diff_released();
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
            cli_closed(line, then_line, &taken);
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
    cli_closed(line, then_line, &taken);
    cli_line_text(out, "{\"added\":");
    cli_numbers_written(out, s_diff->added, s_diff->addeds);
    cli_line_text(out, ",\"changed\":");
    cli_numbers_written(out, s_diff->changed, s_diff->changeds);
    cli_line_text(out, ",\"removed\":");
    cli_numbers_written(out, s_diff->removed, s_diff->removeds);
    cli_line_text(out, ",\"hunks\":[");
    for (unsigned int at = 0u; at < s_diff->hunks; at += 1u)
    {
        const unsigned int *const hunk = s_diff->hunk[at];
        cli_line_text(out, (at != 0u) ? ",{\"now\":" : "{\"now\":");
        cli_numbers_written(out, hunk, 2u);
        cli_line_text(out, ",\"then\":");
        cli_numbers_written(out, hunk + 2, 2u);
        if (hunk[3] > hunk[2])
        {
            cli_line_text(out, ",\"thenLines\":[");
            for (unsigned int one = hunk[2]; one < hunk[3]; one += 1u)
            {
                cli_line_text(out, (one != hunk[2]) ? "," : "");
                cli_document_units_written(out, then->unit + then->start[one], then->length[one]);
            }
            cli_line_character(out, ']');
        }
        cli_line_character(out, '}');
    }
    cli_line_text(out, "]}");
    diff_released();
    return 1;
}

long long cli_diff_run_of(const CliLines *then, const CliLines *now, const unsigned long long *hash_then,
                          const unsigned long long *hash_now, long long x, long long y)
{
    // the lines of each are below CLI_DIFF_LINES, which a long long holds
    const CliRuns runs = {then, now, 0ull, 0ull, (long long)then->lines, (long long)now->lines};
    if ((then->lines > CLI_DIFF_LINES) || (now->lines > CLI_DIFF_LINES) || !diff_held(then->lines, now->lines))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < then->lines; at += 1u)
    {
        s_diff->hash_then[at] = hash_then[at];
    }
    for (unsigned int at = 0u; at < now->lines; at += 1u)
    {
        s_diff->hash_now[at] = hash_now[at];
    }
    const long long run = cli_run(&runs, x, y);
    diff_released();
    return run;
}
