// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_branch.c: a held text colored as a branch of the tree is walked, its grammar applied a line at a time, the
// runs of each line colored handed on and the states it is colored from and ends in kept with the line. The walk works
// out from a line, the view's first, the lines nearest it first, below it before above it at each distance. A run of
// lines below is entered in the state the line above it ends in; a run above starts from root, as far above as it
// reaches; and the lines after a run are colored again until one is entered in the state it was entered in before.
// Once every line is colored, a line entered in a state the line above does not end in is colored again the same way,
// until each line is entered in the state the line above ends in, root for the first. Only the lines colored go on.
// A paged text is walked the same way, its lines read through the pages as far as their index has read, once the page
// holds some of them; every line is colored and only the runs of those the page holds go on
#include "../command_line/cells.h"
#include "../command_line/file_writing.h"
#include "document_internal.h"
#include "highlight.h"
#include "pages.h"

// root's place among a colored text's states, the first written; the most lines a step of the walk colors in a run, and
// again after it; the lines colored at once where a coloring stops at the first entered as before, and where it does
// not; the room a step's colors are begun in, a state's longest text, the room a text's states are begun in, and no line
#define CLI_DOC_ROOT 0u
#define CLI_DOC_STEP 256u
#define CLI_DOC_BATCH 64u
#define CLI_DOC_RUN 4096u
#define CLI_DOC_COLORS_FIRST 4096u
#define CLI_DOC_STATE 1024u
#define CLI_DOC_STATES_FIRST 256u
#define CLI_DOC_NONE 0xffffffffu

// where the colors go, whether a thread walks them out, whether one is walking, and the thread
static CliDocColored s_colored;
static int s_threaded;
static int s_walking;
static CliThread s_walker;

// the place of the state `text` among the text's states, written after them where it is not there: root's place where
// the cells cannot meet it, the text's colors refused
static unsigned int branch_state(CliStoredText *one, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    for (unsigned int at = 0u; at < one->state_used;)
    {
        const char *const there = one->state + at;
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = there, .cap = CLI_CELLS_POOL});
        if ((size == length) && (memor.cmp(&(MemoriaCfg){.src = there, .other = text, .bytes = length}) == 0))
        {
            return at;
        }
        // a state's text lies within the states' room, which an unsigned int counts
        at += (unsigned int)size + 1u;
    }
    const unsigned long long needed = (unsigned long long)one->state_used + length + 1ull;
    if (needed > one->state_room)
    {
        unsigned long long room = (one->state_room != 0u) ? one->state_room : CLI_DOC_STATES_FIRST;
        while (room < needed)
        {
            room *= 2ull;
        }
        char *const grown = (room <= 0xffffffffull) ? g_cli_cells->persistent_buf_alloc(room, NULL) : NULL;
        if (grown == NULL)
        {
            one->refused = 1;
            return CLI_DOC_ROOT;
        }
        if (one->state_used != 0u)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = one->state, .bytes = one->state_used});
        }
        g_cli_cells->persistent_buf_release(one->state, NULL);
        one->state = grown;
        // a room checked above to be no more than an unsigned int counts
        one->state_room = (unsigned int)room;
    }
    memor.cpy(&(MemoriaCfg){.dst = one->state + one->state_used, .src = text, .bytes = length + 1ull});
    const unsigned int at = one->state_used;
    // the states take no more than their room
    one->state_used = (unsigned int)needed;
    return at;
}

// the line `line` of `lines` written as UTF-8 into `out`, a surrogate pair read as the one letter it makes
static void branch_line_put(const CliLines *lines, unsigned int line, VerbaLinea *out)
{
    const unsigned short *const unit = lines->unit + lines->start[line];
    const unsigned long long count = lines->length[line];
    for (unsigned long long place = 0ull; place < count; place += 1ull)
    {
        unsigned int letter = unit[place];
        const int paired = (letter >= 0xd800u) && (letter < 0xdc00u) && ((place + 1ull) < count) &&
                           (unit[place + 1ull] >= 0xdc00u) && (unit[place + 1ull] < 0xe000u);
        if (paired)
        {
            letter = 0x10000u + ((letter - 0xd800u) << 10u) + (unit[place + 1ull] - 0xdc00u);
            place += 1ull;
        }
        cli_utf8_put(out, letter);
    }
}

// the lines from `from` to `to` of `lines` written as UTF-8 into `text`, `room` bytes, each ended by a NUL and each
// line's start put into `line`
static void branch_lines_written(const CliLines *lines, unsigned int from, unsigned int to, char *text,
                                 unsigned long long room, const char **line)
{
    VerbaLinea out = {text, room, 0ull};
    for (unsigned int at = from; at < to; at += 1u)
    {
        line[at - from] = text + out.at;
        branch_line_put(lines, at, &out);
        mmgr_line_ch(&out, '\0');
    }
}

// the lines from `first` to `end` of `one` as UTF-8, each ended by a NUL, in a cell taken for them, each line's start
// put into `line`, a text read whole read from its units and a paged text through the pages: the cell, given back with
// g_cli_cells->persistent_buf_release, the line after the last read into `read`; NULL where the cells cannot meet it or
// the pages could not read the lines
static char *branch_text_taken(const CliStoredText *one, unsigned int first, unsigned int end, const char **line,
                               unsigned int *read)
{
    *read = first;
    if (one->paged)
    {
        unsigned long long count = 0ull;
        char *const paged = cli_pages_lines_taken(one->key, first, end, line, &count);
        // no more lines are read than were asked, which an unsigned int counts
        *read = first + (unsigned int)count;
        return paged;
    }
    const CliLines *const lines = &one->lines;
    // each unit written as three bytes at the most, and each line's NUL
    unsigned long long room = 1ull;
    for (unsigned int at = first; at < end; at += 1u)
    {
        room += (3ull * lines->length[at]) + 1ull;
    }
    char *const text = g_cli_cells->persistent_buf_alloc(room, NULL);
    if (text != NULL)
    {
        branch_lines_written(lines, first, end, text, room, line);
        *read = end;
    }
    return text;
}

// The runs of a run of lines handed on as they are written: whether its `{"from", "runs"}` is begun, how many lines'
// runs it holds, whether a run of lines handed on with it was written before it, and where they are written
typedef struct
{
    int begun;
    unsigned int written;
    int *any;
    VerbaLinea *out;
} CliBranchRange;

// A coloring under way: the text, its first line, the line handed next and the state it is entered in, whether it
// stops at the first line entered in the state it was entered in before and whether it has, and the run its lines'
// runs are handed on in
typedef struct
{
    CliStoredText *one;
    unsigned int line;
    unsigned int entering;
    int stopping;
    int stopped;
    CliBranchRange *range;
} CliBranchColoring;

// the runs of the line `line` written into the run of lines handed on, `[column, name, column, name, ...]`, the run
// begun at it as `{"from", "runs"}` where it is the first
static void branch_runs_put(CliBranchRange *range, unsigned int line, const CliLeaves *leaves)
{
    VerbaLinea *const out = range->out;
    if (!range->begun)
    {
        mmgr_line_put(out, *range->any ? ",{\"from\":" : "{\"from\":");
        mmgr_line_decimal(out, line, 1u);
        mmgr_line_put(out, ",\"runs\":[");
        range->begun = 1;
        *range->any = 1;
    }
    mmgr_line_put(out, (range->written != 0u) ? ",[" : "[");
    for (unsigned int run = 0u; run < leaves->runs; run += 1u)
    {
        mmgr_line_put(out, (run != 0u) ? "," : "");
        mmgr_line_decimal(out, leaves->col[run], 1u);
        mmgr_line_ch(out, ',');
        mmgr_line_decimal(out, leaves->name[run], 1u);
    }
    mmgr_line_ch(out, ']');
    range->written += 1u;
}

// one line's leaves: its states kept and its runs handed on where the page holds the line, or the coloring stopped
static void branch_line(void *context, const CliLeaves *leaves)
{
    CliBranchColoring *const coloring = (CliBranchColoring *)context;
    CliStoredText *const one = coloring->one;
    CliLines *const lines = &one->lines;
    const unsigned int line = coloring->line;
    if (coloring->stopped || (coloring->stopping && (lines->begun[line] == coloring->entering)))
    {
        coloring->stopped = 1;
        return;
    }
    lines->begun[line] = coloring->entering;
    lines->ended[line] = branch_state(one, leaves->ended);
    if ((line >= one->held_from) && (line < one->held_to))
    {
        branch_runs_put(coloring->range, line, leaves);
    }
    coloring->entering = lines->ended[line];
    coloring->line += 1u;
}

// the lines from `from` to `to` of `one` colored, the first entered in the state the line above it ends in, root where
// it is the first or the line above is not colored; where `stopping` is 1, stopped at the first entered in the state it
// was entered in before. The runs of those the page holds handed on in `range`: the line after the last colored
static unsigned int branch_colored(CliStoredText *one, unsigned int from, unsigned int to, int stopping,
                                   CliBranchRange *range)
{
    const CliLines *const lines = &one->lines;
    const unsigned int above = (from != 0u) ? lines->ended[from - 1u] : CLI_DOC_UNCOLORED;
    CliBranchColoring coloring = {one, from, (above != CLI_DOC_UNCOLORED) ? above : CLI_DOC_ROOT, stopping, 0, range};
    while (!coloring.stopped && !one->refused && (coloring.line < to))
    {
        const unsigned int first = coloring.line;
        const unsigned int most = stopping ? CLI_DOC_BATCH : CLI_DOC_RUN;
        const unsigned int end = ((to - first) > most) ? (first + most) : to;
        const char **const line =
            g_cli_cells->persistent_buf_alloc((unsigned long long)(end - first) * sizeof(*line), NULL);
        unsigned int read = first;
        char *const text = (line != NULL) ? branch_text_taken(one, first, end, line, &read) : NULL;
        // the state the first line is entered in, copied before a state written during the coloring moves the states
        char entering[CLI_DOC_STATE];
        VerbaLinea state = {entering, sizeof(entering), 0ull};
        mmgr_line_put(&state, one->state + coloring.entering);
        const int stated = mmgr_line_finish(&state) != 0ull;
        if ((text != NULL) && (read > first))
        {
            char ended[CLI_DOC_STATE];
            VerbaLinea last = {ended, sizeof(ended), 0ull};
            char why[CLI_NAME_MAX_LEN];
            VerbaLinea err_msg = {why, sizeof(why), 0ull};
            one->refused = !cli_highlight_leaves(one->grammar, stated ? entering : "root", line, read - first,
                                                 (read == lines->lines) && one->whole, branch_line, &coloring, &last,
                                                 &err_msg);
        }
        else
        {
            one->refused = 1;
        }
        g_cli_cells->persistent_buf_release(line, NULL);
        g_cli_cells->persistent_buf_release(text, NULL);
    }
    return coloring.line;
}

// the run of lines not colored about `at`, below the walk's origin where `down` is 1: its first and the line after its
// last into `first` and `end`, no more than CLI_DOC_STEP of them reaching from `at`, down or up; a run down whose line
// above ends in no known state reaching as far above as CLI_DOC_LEAD lines not colored reach
static void branch_run_of(const CliLines *lines, unsigned int at, int down, unsigned int *first, unsigned int *end)
{
    unsigned int start = at;
    unsigned int stop = at + 1u;
    while (down && (stop < lines->lines) && (lines->begun[stop] == CLI_DOC_UNCOLORED) && ((stop - at) < CLI_DOC_STEP))
    {
        stop += 1u;
    }
    while (!down && (start > 0u) && (lines->begun[start - 1u] == CLI_DOC_UNCOLORED) &&
           ((at + 1u - start) < CLI_DOC_STEP))
    {
        start -= 1u;
    }
    for (unsigned int lead = 0u;
         down && (start > 0u) && (lines->begun[start - 1u] == CLI_DOC_UNCOLORED) && (lead < CLI_DOC_LEAD); lead += 1u)
    {
        start -= 1u;
    }
    *first = start;
    *end = stop;
}

// the lines from `first` to `end` of `one` colored and the lines after them colored again until one is entered in the
// state it was entered in before, no more than CLI_DOC_STEP of those, the runs of those the page holds written into
// `out` as `{"from", "runs"}`, after a comma where `*any` is set, and `*any` set where they are
static void branch_range_colored(CliStoredText *one, unsigned int first, unsigned int end, int *any, VerbaLinea *out)
{
    const CliLines *const lines = &one->lines;
    CliBranchRange range = {0, 0u, any, out};
    const unsigned int line = branch_colored(one, first, end, 0, &range);
    if ((line == end) && (end < lines->lines) && (lines->begun[end] != CLI_DOC_UNCOLORED))
    {
        const unsigned int most = ((lines->lines - end) > CLI_DOC_STEP) ? (end + CLI_DOC_STEP) : lines->lines;
        (void)branch_colored(one, end, most, 1, &range);
    }
    if (range.begun)
    {
        mmgr_line_put(out, "]}");
    }
}

// colors handed on begun: `{"key":` the text's key `,"seq":` the changes written to it, `,"names":` the names its
// grammar's tokens take where they were not handed on since it was colored by the grammar, `,"colors":`
static void branch_colors_begun(CliStoredText *one, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"key\":");
    cli_json_quoted(out, one->key, cellul.len(&(CatenaFinitaCfg){.src = one->key, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"seq\":");
    mmgr_line_decimal(out, one->changes, 1u);
    if (!one->named)
    {
        mmgr_line_put(out, ",\"names\":");
        if (!cli_highlight_names_written(one->grammar, out))
        {
            mmgr_line_put(out, "[\"\"]");
        }
        one->named = 1;
    }
    mmgr_line_put(out, ",\"colors\":");
}

// one step of the walk of `one`: the run of lines not colored nearest its origin colored, or where every line is, the
// first entered in a state the line above does not end in colored again, written into `out` as `{"key", "seq",
// "colors"}`, `*handed` set where it holds the runs of a line the page holds: 1 where it colored a line, else 0. A paged
// text's lines are counted again first, and it is not walked until the page holds some of its lines
static int branch_step(CliStoredText *one, VerbaLinea *out, int *handed)
{
    const CliLines *const lines = &one->lines;
    one->refused = one->refused || (one->paged && !cli_document_paged_counted(one));
    if ((one->grammar[0] == '\0') || one->refused || (lines->lines == 0u) || (one->held_to <= one->held_from))
    {
        return 0;
    }
    const unsigned int origin = (one->origin < lines->lines) ? one->origin : (lines->lines - 1u);
    unsigned int at = CLI_DOC_NONE;
    int down = 1;
    for (unsigned int distance = 0u; (at == CLI_DOC_NONE) && (((origin + distance) < lines->lines) || (distance < origin));
         distance += 1u)
    {
        if (((origin + distance) < lines->lines) && (lines->begun[origin + distance] == CLI_DOC_UNCOLORED))
        {
            at = origin + distance;
        }
        else if ((distance < origin) && (lines->begun[origin - distance - 1u] == CLI_DOC_UNCOLORED))
        {
            at = origin - distance - 1u;
            down = 0;
        }
    }
    unsigned int first = CLI_DOC_NONE;
    unsigned int end = 0u;
    if (at != CLI_DOC_NONE)
    {
        branch_run_of(lines, at, down, &first, &end);
    }
    for (unsigned int line = 0u; (at == CLI_DOC_NONE) && (first == CLI_DOC_NONE) && (line < lines->lines); line += 1u)
    {
        if (lines->begun[line] != ((line == 0u) ? CLI_DOC_ROOT : lines->ended[line - 1u]))
        {
            first = line;
            end = line + 1u;
        }
    }
    if (first == CLI_DOC_NONE)
    {
        return 0;
    }
    // the names go with the first colors handed on, so a step that hands none on leaves them to the next
    const int named = one->named;
    branch_colors_begun(one, out);
    mmgr_line_ch(out, '[');
    branch_range_colored(one, first, end, handed, out);
    mmgr_line_put(out, "]}");
    one->named = *handed ? one->named : named;
    return 1;
}

// the walk taken step by step until no text has a line to color, the lock given back between steps and each step's
// colors handed on once it is; on the walk's own thread, marked as walking no more as it ends
static void branch_walked(int own)
{
    for (;;)
    {
        VerbaLinea colors;
        const int made = cli_growing_line_made(&colors, CLI_DOC_COLORS_FIRST);
        cli_document_lock_take();
        int stepped = 0;
        int handed = 0;
        for (CliStoredText *one = cli_document_texts(); made && !stepped && (one != NULL); one = one->next)
        {
            stepped = branch_step(one, &colors, &handed);
        }
        s_walking = (own && !stepped) ? 0 : s_walking;
        const CliDocColored colored = s_colored;
        cli_document_lock_give();
        if (handed && (colored != NULL) && mmgr_line_ok(&colors))
        {
            colored(colors.out, colors.at);
        }
        g_cli_cells->persistent_buf_release(colors.out, NULL);
        if (!stepped)
        {
            return;
        }
    }
}

static void branch_walk_run(void *context)
{
    (void)context;
    branch_walked(1);
}

void cli_document_woken(void)
{
    if (s_threaded && !s_walking)
    {
        s_walking = 1;
        s_walker = (CliThread){branch_walk_run, NULL, CLI_PRIORITY_WORK};
        s_walking = cli_thread_started(&s_walker);
    }
}

void cli_document_colors_to(CliDocColored colored, int threaded)
{
    cli_document_lock_take();
    s_colored = colored;
    s_threaded = threaded;
    cli_document_lock_give();
}

int cli_document_colored_by(const char *key, const char *grammar, int named)
{
    cli_document_lock_take();
    CliStoredText *const one = cli_document_stored(key);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = grammar, .cap = CLI_CELLS_POOL});
    if ((one != NULL) && (length < CLI_NAME_MAX_LEN))
    {
        memor.cpy(&(MemoriaCfg){.dst = one->grammar, .src = grammar, .bytes = length + 1ull});
        one->state_used = 0u;
        one->refused = 0;
        one->named = named;
        (void)branch_state(one, "root");
        for (unsigned int line = 0u; line < one->lines.lines; line += 1u)
        {
            one->lines.begun[line] = CLI_DOC_UNCOLORED;
            one->lines.ended[line] = CLI_DOC_UNCOLORED;
        }
        cli_document_woken();
    }
    cli_document_lock_give();
    return one != NULL;
}

int cli_document_viewed(const char *key, unsigned long long first, unsigned long long last, VerbaLinea *out)
{
    VerbaLinea colors;
    const int made = (out == NULL) && cli_growing_line_made(&colors, CLI_DOC_COLORS_FIRST);
    VerbaLinea *const into = (out != NULL) ? out : &colors;
    cli_document_lock_take();
    CliStoredText *const one = cli_document_stored(key);
    int any = 0;
    if ((one != NULL) && ((out != NULL) || made))
    {
        one->refused = one->refused || (one->paged && !cli_document_paged_counted(one));
        const CliLines *const lines = &one->lines;
        // a view past the text's last line is read at its last
        one->origin = (first < lines->lines) ? (unsigned int)first : ((lines->lines != 0u) ? lines->lines - 1u : 0u);
        // the names go with the first colors handed on, so a view that hands none on leaves them to the walk
        const int named = one->named;
        if (out == NULL)
        {
            branch_colors_begun(one, into);
        }
        mmgr_line_ch(into, '[');
        for (unsigned long long line = first;
             (one->grammar[0] != '\0') && !one->refused && (line <= last) && (line < lines->lines);)
        {
            // the view's lines stand below the text's last, which an unsigned int counts
            const unsigned int at = (unsigned int)line;
            if (lines->begun[at] != CLI_DOC_UNCOLORED)
            {
                line += 1ull;
                continue;
            }
            unsigned int run_first = 0u;
            unsigned int run_end = 0u;
            branch_run_of(lines, at, 1, &run_first, &run_end);
            branch_range_colored(one, run_first, run_end, &any, into);
            line = run_end;
        }
        mmgr_line_ch(into, ']');
        mmgr_line_put(into, (out == NULL) ? "}" : "");
        one->named = any ? one->named : named;
        cli_document_woken();
    }
    const CliDocColored colored = s_colored;
    cli_document_lock_give();
    if (any && (out == NULL) && (colored != NULL) && mmgr_line_ok(&colors))
    {
        colored(colors.out, colors.at);
    }
    if (made)
    {
        g_cli_cells->persistent_buf_release(colors.out, NULL);
    }
    return one != NULL;
}

int cli_document_held(const char *key, unsigned long long from, unsigned long long to)
{
    cli_document_lock_take();
    CliStoredText *const one = cli_document_stored(key);
    if (one != NULL)
    {
        one->refused = one->refused || (one->paged && !cli_document_paged_counted(one));
        CliLines *const lines = &one->lines;
        // the lines the page holds stand among those the pages count, which an unsigned int counts
        const unsigned int held_from = (from < lines->lines) ? (unsigned int)from : lines->lines;
        const unsigned int held_to = (to < lines->lines) ? (unsigned int)to : lines->lines;
        // a line the page comes to hold is colored again to hand its runs on, the state it ends in kept so the line
        // below it is entered in the state it was
        for (unsigned int line = held_from; line < held_to; line += 1u)
        {
            if ((line < one->held_from) || (line >= one->held_to))
            {
                lines->begun[line] = CLI_DOC_UNCOLORED;
            }
        }
        one->held_from = held_from;
        one->held_to = held_to;
        cli_document_woken();
    }
    cli_document_lock_give();
    return one != NULL;
}

int cli_document_runs_written(const char *key, unsigned long long first, unsigned long long last, VerbaLinea *out)
{
    cli_document_lock_take();
    CliStoredText *const one = cli_document_stored(key);
    int any = 0;
    mmgr_line_ch(out, '[');
    if (one != NULL)
    {
        one->refused = one->refused || (one->paged && !cli_document_paged_counted(one));
        const CliLines *const lines = &one->lines;
        // the lines asked stand among those the text counts, which an unsigned int counts
        const unsigned int from = (first < lines->lines) ? (unsigned int)first : lines->lines;
        const unsigned int to = (last < lines->lines) ? ((unsigned int)last + 1u) : lines->lines;
        // the lines asked held for this coloring alone, so their runs are written whatever lines the page holds
        const unsigned int held_from = one->held_from;
        const unsigned int held_to = one->held_to;
        one->held_from = from;
        one->held_to = to;
        if ((one->grammar[0] != '\0') && !one->refused && (from < to))
        {
            branch_range_colored(one, from, to, &any, out);
        }
        one->held_from = held_from;
        one->held_to = held_to;
    }
    mmgr_line_ch(out, ']');
    cli_document_lock_give();
    return one != NULL;
}

void cli_document_walked(void)
{
    branch_walked(0);
}

int cli_document_text_written(const char *key, unsigned long long first, unsigned long long last, VerbaLinea *out,
                              char *state, unsigned long long room)
{
    VerbaLinea text;
    const int made = cli_growing_line_made(&text, CLI_DOC_COLORS_FIRST);
    cli_document_lock_take();
    const CliStoredText *const one = cli_document_stored(key);
    VerbaLinea entered = {state, room, 0ull};
    if (made && (one != NULL))
    {
        const CliLines *const lines = &one->lines;
        // the lines asked stand below the text's last, which an unsigned int counts
        const unsigned int to = (last < lines->lines) ? (unsigned int)last : (lines->lines - 1u);
        const unsigned int from = (first <= to) ? (unsigned int)first : to;
        const unsigned int begun = (lines->lines != 0u) ? lines->begun[from] : CLI_DOC_UNCOLORED;
        mmgr_line_put(&entered, ((begun != CLI_DOC_UNCOLORED) && (one->state != NULL)) ? (one->state + begun) : "root");
        const char **const line = (one->paged && (lines->lines != 0u))
                                      ? g_cli_cells->persistent_buf_alloc((to - from + 1ull) * sizeof(*line), NULL)
                                      : NULL;
        unsigned int read = from;
        char *const paged = (line != NULL) ? branch_text_taken(one, from, to + 1u, line, &read) : NULL;
        for (unsigned int at = from; (paged != NULL) && (at < read); at += 1u)
        {
            mmgr_line_put(&text, line[at - from]);
            mmgr_line_put(&text, (at < to) ? "\n" : "");
        }
        g_cli_cells->persistent_buf_release(paged, NULL);
        g_cli_cells->persistent_buf_release(line, NULL);
        for (unsigned int at = from; !one->paged && (lines->lines != 0u) && (at <= to); at += 1u)
        {
            branch_line_put(lines, at, &text);
            mmgr_line_put(&text, (at < to) ? "\n" : "");
        }
    }
    cli_document_lock_give();
    if (made && (one != NULL) && mmgr_line_ok(&text))
    {
        cli_json_quoted(out, text.out, text.at);
    }
    if ((mmgr_line_finish(&entered) == 0ull) && (room != 0ull))
    {
        state[0] = '\0';
    }
    if (made)
    {
        g_cli_cells->persistent_buf_release(text.out, NULL);
    }
    return made && (one != NULL);
}
