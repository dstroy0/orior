// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_branch.c: a stored text colored as a branch of the tree is walked, its grammar applied a line at a time, the
// runs of each line colored handed on and the states it is colored from and ends in kept with the line. The walk works
// out from a line, the view's first, the lines nearest it first, below it before above it at each distance. A run of
// lines below is entered in the state the line above it ends in; a run above starts from root, as far above as it
// reaches; and the lines after a run are colored again until one is entered in the state it was entered in before.
// Once every line is colored, a line entered in a state the line above does not end in is colored again the same way,
// until each line is entered in the state the line above ends in, root for the first. Only the lines colored go on.
// A paged text is walked the same way, its lines read through the pages as far as their index has read, once the page
// mirrors some of them; every line is colored and only the runs of those the page mirrors go on.
// As each line is colored the regions its grammar's roles open and close on it are kept with it, and once every line
// of a text is colored and its regions moved since, its folds go on: each region from the line it opens on to the last
// line the fold hides, a paged text's those that meet the lines the page mirrors.
// The lock is held only to claim lines and to put in what a coloring found: each batch of lines is claimed under it,
// its lines and the state the first is entered in copied out, read and colored with the lock given back, in a work of
// the coloring's own, and put in under the lock again where the text did not move meanwhile. So the walk, the views
// and the reads color at once, each on its own thread, and every call that reads a text waits on none of them. The walk
// runs on as many threads as the machine has processors to spare, each walking one text at a time, and every coloring
// hands its colors on in the order they were put in
#include "../code/symbols.h"
#include "../code/symbols_internal.h"
#include "../command_line/cells.h"
#include "../command_line/file_writing.h"
#include "document_internal.h"
#include "highlight.h"
#include "pages.h"

#include <stdatomic.h>

// root's place among a colored text's states, the first written; the most lines a step of the walk colors in a run, and
// again after it, and the most bytes of lines it colors, the last line it colors maybe past them, so its colors are
// handed on in steps, and the bytes of a coloring that colors every line asked; the lines colored at once where a
// coloring stops at the first entered as before, and where it does not, CLI_DOC_STEP where its bytes reach no further;
// the room a step's colors are begun in, a state's longest text, the room a text's states hold past them and the places
// of its index they are found by, the bytes a coloring's room is begun in, the share of what a text's room for its
// states or its runs is taken for that it holds past it, and no line
#define CLI_DOC_ROOT 0u
#define CLI_DOC_STEP 256u
#define CLI_DOC_STEP_BYTES 16384ull
#define CLI_DOC_ALL_BYTES 0xffffffffffffffffull
#define CLI_DOC_BATCH 64u
#define CLI_DOC_RUN 4096u
#define CLI_DOC_COLORS_FIRST 4096u
#define CLI_DOC_STATE 8192u
#define CLI_DOC_STATES_FIRST 256u
#define CLI_DOC_INDEX_FIRST 64u
#define CLI_DOC_ROOM_FIRST 4096u
#define CLI_DOC_ROOM_SHARE 8ull
#define CLI_DOC_NONE 0xffffffffu

// the depths of pairs a name each, a deeper pair taking the name of its depth less as many
#define CLI_DOC_DEPTHS 16u

// A walker: a thread of its own walking one text at a time, what it runs, the line its steps' colors are written into,
// taken once and emptied for each step, and whether it walks, under the lock
typedef struct
{
    CliThread thread;
    VerbaLinea step;
    int walking;
} CliBranchWalker;

// the most walkers; where the colors go, whether walkers walk them out, and how many may walk at once, the machine's
// processors but one, one at the fewest; the walkers, and past them the walk a caller takes on its own thread; and the
// lock the colors are handed on under, taken before the texts' lock is given back, so they go in the order they were
// put in
#define CLI_DOC_WALKERS 8u
static CliDocColored s_colored;
static int s_threaded;
static unsigned int s_walkers_most;
static CliBranchWalker s_walkers[CLI_DOC_WALKERS + 1u];
static CliLock s_handing = CLI_LOCK_INIT;

// the rooms the folds are found in, each taken once, grown as a use needs more and used again by every use after,
// under the lock: the regions open above a line, and the widest fold that opens on each line
static void *s_opened;
static unsigned long long s_opened_room;
static void *s_widest;
static unsigned long long s_widest_room;

// A coloring's work, its rooms taken as the coloring needs them, grown as it needs more and given back as it ends: the
// UTF-8 of the lines it reads, where each of those lines starts, the names each run of a line takes, the roles of its
// grammar's names, and what it found on each line, as it is put in
typedef struct
{
    void *text;
    unsigned long long text_room;
    void *starts;
    unsigned long long starts_room;
    void *named;
    unsigned long long named_room;
    void *roles;
    unsigned long long roles_room;
    void *found;
    unsigned long long found_room;
} CliBranchWork;

// the room `*cell` of `*room` bytes made `wanted` bytes at least, doubling from CLI_DOC_ROOM_FIRST, its first `kept`
// bytes carried into it where it grows: 1, or 0 where the cells cannot meet it, the room as it was
static int branch_room_made(void **cell, unsigned long long *room, unsigned long long wanted, unsigned long long kept)
{
    if (wanted <= *room)
    {
        return 1;
    }
    unsigned long long grown = (*room != 0ull) ? *room : CLI_DOC_ROOM_FIRST;
    while (grown < wanted)
    {
        grown *= 2ull;
    }
    void *const made = g_cli_cells->persistent_buf_alloc(grown, NULL);
    if (made == NULL)
    {
        return 0;
    }
    if (kept != 0ull)
    {
        memor.cpy(&(MemoriaCfg){.dst = made, .src = *cell, .bytes = kept});
    }
    cli_cell_release(*cell, NULL);
    *cell = made;
    *room = grown;
    return 1;
}

// a work of no rooms, as a coloring begins
static void branch_work_emptied(CliBranchWork *work)
{
    work->text = NULL;
    work->text_room = 0ull;
    work->starts = NULL;
    work->starts_room = 0ull;
    work->named = NULL;
    work->named_room = 0ull;
    work->roles = NULL;
    work->roles_room = 0ull;
    work->found = NULL;
    work->found_room = 0ull;
}

// a work's rooms given back, as its coloring ends
static void branch_work_given(CliBranchWork *work)
{
    cli_cell_release(work->text, NULL);
    cli_cell_release(work->starts, NULL);
    cli_cell_release(work->named, NULL);
    cli_cell_release(work->roles, NULL);
    cli_cell_release(work->found, NULL);
}

// the hash of `length` bytes of `text`, FNV-1a over its bytes
static unsigned int branch_state_hash(const char *text, unsigned long long length)
{
    unsigned int hash = 2166136261u;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        hash = (hash ^ (unsigned char)text[at]) * 16777619u;
    }
    return hash;
}

// whether the state at the place `at` among the text's states is `length` bytes of `text`
static int branch_state_is(const CliStoredText *one, unsigned int at, const char *text, unsigned long long length)
{
    return ((at + length) < one->state_used) && (one->state[at + length] == '\0') &&
           (memor.cmp(&(MemoriaCfg){.src = one->state + at, .other = text, .bytes = length}) == 0);
}

// the state at the place `at` put at the first place of the index free from its hash's
static void branch_index_put(CliStoredText *one, unsigned int at)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = one->state + at, .cap = CLI_CELLS_POOL});
    unsigned int entry = branch_state_hash(one->state + at, length) & (one->state_indexed - 1u);
    while (one->state_index[entry] != 0u)
    {
        entry = (entry + 1u) & (one->state_indexed - 1u);
    }
    one->state_index[entry] = at + 1u;
}

// the index twice as large where half of it holds states, every state put again: 1, or 0 where the cells cannot meet it
static int branch_index_grown(CliStoredText *one)
{
    if ((one->state_indexed != 0u) && ((2u * (one->state_count + 1u)) <= one->state_indexed))
    {
        return 1;
    }
    const unsigned long long places = (one->state_indexed != 0u) ? (2ull * one->state_indexed) : CLI_DOC_INDEX_FIRST;
    unsigned int *const grown =
        (places <= 0x80000000ull) ? g_cli_cells->persistent_buf_alloc(places * sizeof(*grown), NULL) : NULL;
    if (grown == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = grown, .val = 0, .bytes = places * sizeof(*grown)});
    cli_cell_release(one->state_index, NULL);
    one->state_index = grown;
    // a count checked above to be no more than an unsigned int's half
    one->state_indexed = (unsigned int)places;
    for (unsigned int at = 0u; at < one->state_used;)
    {
        branch_index_put(one, at);
        // a state's text lies within the states' room, which an unsigned int counts
        at += (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = one->state + at, .cap = CLI_CELLS_POOL}) + 1u;
    }
    return 1;
}

// the place of the state `text` among the text's states, found by its hash, the place found last tried first, and
// written after them where it is not there: root's place where the cells cannot meet it, the text's colors refused
static unsigned int branch_state(CliStoredText *one, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    if ((one->state_count != 0u) && branch_state_is(one, one->state_last, text, length))
    {
        return one->state_last;
    }
    if (!branch_index_grown(one))
    {
        one->refused = 1;
        return CLI_DOC_ROOT;
    }
    for (unsigned int entry = branch_state_hash(text, length) & (one->state_indexed - 1u);
         one->state_index[entry] != 0u; entry = (entry + 1u) & (one->state_indexed - 1u))
    {
        if (branch_state_is(one, one->state_index[entry] - 1u, text, length))
        {
            one->state_last = one->state_index[entry] - 1u;
            return one->state_last;
        }
    }
    const unsigned long long needed = (unsigned long long)one->state_used + length + 1ull;
    if (needed > one->state_room)
    {
        // the states with a share of them past those, as a runs' room holds its runs
        const unsigned long long room = needed + (needed / CLI_DOC_ROOM_SHARE) + CLI_DOC_STATES_FIRST;
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
        cli_cell_release(one->state, NULL);
        one->state = grown;
        // a room checked above to be no more than an unsigned int counts
        one->state_room = (unsigned int)room;
    }
    memor.cpy(&(MemoriaCfg){.dst = one->state + one->state_used, .src = text, .bytes = length + 1ull});
    const unsigned int at = one->state_used;
    // the states take no more than their room
    one->state_used = (unsigned int)needed;
    branch_index_put(one, at);
    one->state_count += 1u;
    one->state_last = at;
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

// the lines from `first` to `end` of a text read whole, `lines`, as UTF-8 in the rooms of `work`, each ended by a NUL
// and each line's start put into its starts, as many as `bytes` of their units reach, one at the least, a unit taking a
// byte of UTF-8 or more: the starts, the line after the last read into `read`, NULL where the cells cannot meet the
// rooms; under the lock
static const char **branch_lines_copied(const CliLines *lines, unsigned int first, unsigned int end,
                                        unsigned long long bytes, CliBranchWork *work, unsigned int *read)
{
    *read = first;
    // each unit written as three bytes at the most, and each line's NUL
    unsigned long long room = 1ull;
    unsigned long long spent = 0ull;
    unsigned int to = first;
    while ((to < end) && ((to == first) || (bytes == CLI_DOC_ALL_BYTES) || (spent < bytes)))
    {
        room += (3ull * lines->length[to]) + 1ull;
        spent += lines->length[to] + 1ull;
        to += 1u;
    }
    if ((to <= first) ||
        !branch_room_made(&work->starts, &work->starts_room, (unsigned long long)(to - first) * sizeof(const char *),
                          0ull) ||
        !branch_room_made(&work->text, &work->text_room, room, 0ull))
    {
        return NULL;
    }
    const char **const line = (const char **)work->starts;
    branch_lines_written(lines, first, to, (char *)work->text, room, line);
    *read = to;
    return line;
}

// the lines from `first` to `end` of the paged text under `key` as UTF-8, read through the pages into a cell they take
// for them, `*taken`, to be given back with g_cli_cells->persistent_buf_release, each line's start put into the starts
// of `work`: the starts, the line after the last read into `read`, NULL where the cells cannot meet the room or the
// pages could not read the lines; with no lock held
static const char **branch_lines_paged(const char *key, unsigned int first, unsigned int end, CliBranchWork *work,
                                       unsigned int *read, char **taken)
{
    *read = first;
    *taken = NULL;
    if ((end <= first) ||
        !branch_room_made(&work->starts, &work->starts_room,
                          (unsigned long long)(end - first) * sizeof(const char *), 0ull))
    {
        return NULL;
    }
    const char **const line = (const char **)work->starts;
    unsigned long long count = 0ull;
    *taken = cli_pages_lines_taken(key, first, end, line, &count);
    // no more lines are read than were asked, which an unsigned int counts
    *read = first + (unsigned int)count;
    return (*taken != NULL) ? line : NULL;
}

// The runs of runs of lines handed on as they are written: whether a `{"from", "runs"}` is begun, how many lines' runs
// it holds, whether a run of lines handed on with it was written before it, where they are written, the columns of
// each line whose runs are handed on, 0 for every one, how many names the grammar's tokens take, the place of the
// first a run that counts depth takes by its depth, 0 for a grammar that names none, the bytes of lines it colors yet,
// CLI_DOC_ALL_BYTES where it colors every line asked, and the line after the last whose runs were written; whether the
// text moved while it colored, so it colors no more; whether the lines whose runs are handed on are those it asks,
// from `hand_from` to before `hand_to`, else those the page holds as each is put in; and the line it colors no further
// than as it goes on past the lines asked. Its columns read as the text's as each line is put in, where it names them
typedef struct
{
    int begun;
    unsigned int written;
    int *any;
    VerbaLinea *out;
    unsigned long long columns;
    unsigned int names;
    unsigned long long bytes;
    unsigned int next;
    int stale;
    int asked;
    unsigned int hand_from;
    unsigned int hand_to;
    unsigned int high;
} CliBranchRange;

// the mark a line's state carries the place of the scope it stands in after, past the colorer's own state, the scope
// written among the text's states once; the most names a scope holds and the bytes their texts take
#define CLI_DOC_SCOPE_MARK '\x1e'
#define CLI_DOC_SCOPE_NAMES 128u
#define CLI_DOC_SCOPE_TEXT 4096u

// The scope a line stands in as the walk reads it: the depth of the regions open, the depth of the pairs open that count
// it, and the names declared in the regions that a use of the name is named as, each its text's place among the texts
// and its bytes, the name its declaration's run takes, the depth of the region it belongs to, and whether it waits for
// that region to open, as a function's parameter waits for its body; how many, and the texts' bytes used; and its
// place among the text's states, found again only where it moved since, CLI_DOC_NONE where it was not found
typedef struct
{
    unsigned int depth;
    unsigned int pairs;
    unsigned int count;
    unsigned int text_at[CLI_DOC_SCOPE_NAMES];
    unsigned int size[CLI_DOC_SCOPE_NAMES];
    unsigned int named[CLI_DOC_SCOPE_NAMES];
    unsigned int belongs[CLI_DOC_SCOPE_NAMES];
    unsigned char waits[CLI_DOC_SCOPE_NAMES];
    unsigned int used;
    char text[CLI_DOC_SCOPE_TEXT];
    unsigned int place;
    int moved;
} CliBranchScope;

// A batch of lines as it is colored with no lock held: the work it is written into; the scope the line colored next
// stands in, and whether the place among the text's states of the scope the line before it ended in is known; the names
// the grammar's tokens take, and the roles of each, read as the lines were claimed, `roles_count` of them; the words of
// what it found written; and whether the cells refused the room for them. What it finds on each line is a record of
// words: how many runs, the line's regions, the bytes of the colorer's state the line ends in, and the bytes of the
// scope it ends in, CLI_DOC_NONE for a scope of no depth, no pairs and no names, CLI_DOC_SCOPE_SAME for the one the line
// before it ended in; then the runs' columns, their names as the walk names them, the colorer's state and the scope,
// each of the two from a word of its own
typedef struct
{
    CliBranchWork *work;
    CliBranchScope scope;
    int placed;
    unsigned int names;
    const unsigned int *roles;
    unsigned int roles_count;
    unsigned long long used;
    int refused;
} CliBranchFinding;

// a line's scope the one the line before it ended in, and the words of a record before its runs
#define CLI_DOC_SCOPE_SAME 0xfffffffeu
#define CLI_DOC_FOUND_HEAD 4u

// a run of lines handed on in `range` ended where one is begun
static void branch_range_ended(CliBranchRange *range)
{
    if (range->begun)
    {
        mmgr_line_put(range->out, "]}");
        range->begun = 0;
    }
}

// the runs of the line `line` written into the run of lines handed on, `[column, name, column, name, ...]`, those that
// start at or past the range's columns left out where it names them, the first kept; a run of lines begun at it as
// `{"from", "runs"}` where it is the first or the line before it was not the last written, the one before ended
static void branch_runs_put(CliBranchRange *range, unsigned int line, const CliLeaves *leaves)
{
    CliLeaves shown = *leaves;
    while ((range->columns != 0ull) && (shown.runs > 1u) && (shown.col[shown.runs - 1u] >= range->columns))
    {
        shown.runs -= 1u;
    }
    VerbaLinea *const out = range->out;
    if (range->begun && (line != range->next))
    {
        mmgr_line_put(out, "]}");
        range->begun = 0;
    }
    if (!range->begun)
    {
        mmgr_line_put(out, *range->any ? ",{\"from\":" : "{\"from\":");
        mmgr_line_decimal(out, line, 1u);
        mmgr_line_put(out, ",\"runs\":[");
        range->begun = 1;
        range->written = 0u;
        *range->any = 1;
    }
    mmgr_line_put(out, (range->written != 0u) ? "," : "");
    cli_highlight_runs_written(&shown, out);
    range->written += 1u;
    range->next = line + 1u;
}

// the words a runs' room holds past the runs it is taken for, as many again as their share CLI_DOC_ROOM_SHARE holds
#define CLI_DOC_RUNS_SPARE 4096ull

// the words of the runs the lines of `one` keep, and a room the cells give them taken afresh, those runs moved into
// it in the order of their lines, so it holds them with `more` words after them and a share of them past those: 1, or
// 0 where the cells cannot meet it, the room as it was
static int branch_runs_roomed(CliStoredText *one, unsigned long long more)
{
    CliLines *const lines = &one->lines;
    unsigned long long kept = 0ull;
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        kept += (lines->runs[line] != CLI_DOC_NO_RUNS) ? (1ull + (2ull * one->run[lines->runs[line]])) : 0ull;
    }
    const unsigned long long wanted = kept + more;
    const unsigned long long room = wanted + (wanted / CLI_DOC_ROOM_SHARE) + CLI_DOC_RUNS_SPARE;
    unsigned int *const moved =
        (room <= 0xffffffffull) ? g_cli_cells->persistent_buf_alloc(room * sizeof(*moved), NULL) : NULL;
    if (moved == NULL)
    {
        return 0;
    }
    unsigned int used = 0u;
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        const unsigned int at = lines->runs[line];
        if (at != CLI_DOC_NO_RUNS)
        {
            // a line's runs lie within the words kept, which an unsigned int counts
            const unsigned int words = 1u + (2u * one->run[at]);
            memor.cpy(&(MemoriaCfg){.dst = moved + used, .src = one->run + at, .bytes = words * sizeof(*moved)});
            lines->runs[line] = used;
            used += words;
        }
    }
    cli_cell_release(one->run, NULL);
    one->run = moved;
    // a room checked above to be no more than an unsigned int counts
    one->run_room = (unsigned int)room;
    one->run_used = used;
    return 1;
}

// the runs of `leaves`, the line `line` of `one` as the walk named them, kept in place of those it kept, none where the
// cells cannot meet them
static void branch_runs_kept(CliStoredText *one, unsigned int line, const CliLeaves *leaves)
{
    CliLines *const lines = &one->lines;
    lines->runs[line] = CLI_DOC_NO_RUNS;
    const unsigned long long words = 1ull + (2ull * leaves->runs);
    if (((one->run_used + words) > one->run_room) && !branch_runs_roomed(one, words))
    {
        return;
    }
    unsigned int *const record = one->run + one->run_used;
    record[0] = leaves->runs;
    memor.cpy(&(MemoriaCfg){.dst = record + 1, .src = leaves->col, .bytes = leaves->runs * sizeof(*record)});
    memor.cpy(&(MemoriaCfg){
        .dst = record + 1 + leaves->runs, .src = leaves->name, .bytes = leaves->runs * sizeof(*record)});
    lines->runs[line] = one->run_used;
    // the words written lie within the room, which an unsigned int counts
    one->run_used += (unsigned int)words;
}

// whether the line `line` of `one` keeps runs it may hand on as they are: it is colored, keeps runs, and was entered
// in the state the line above it ends in, or from root where that line is not colored
static int branch_runs_standing(const CliStoredText *one, unsigned int line)
{
    const CliLines *const lines = &one->lines;
    const unsigned int above = (line != 0u) ? lines->ended[line - 1u] : CLI_DOC_UNCOLORED;
    return (lines->begun[line] != CLI_DOC_UNCOLORED) && (lines->runs[line] != CLI_DOC_NO_RUNS) &&
           ((above == CLI_DOC_UNCOLORED) ? (lines->begun[line] == CLI_DOC_ROOT) : (lines->begun[line] == above));
}

// the runs the line `line` of `one` keeps written through `range`
static void branch_runs_handed(const CliStoredText *one, unsigned int line, CliBranchRange *range)
{
    const unsigned int *const record = one->run + one->lines.runs[line];
    const CliLeaves kept = {NULL, record[0], record + 1, record + 1 + record[0], NULL, NULL, NULL, 0u};
    branch_runs_put(range, line, &kept);
}

// the roles that count the depth of a pair
#define CLI_DOC_PAIR_ROLES (CLI_ROLE_DEPTH_OPENS | CLI_ROLE_DEPTH_CLOSES)

// one name of the grammar of the text at `context` its roles read and kept among the text's roles at its place
static void branch_role_read(void *context, unsigned int place, const char *name)
{
    CliStoredText *const one = (CliStoredText *)context;
    if (place < one->roles_room)
    {
        one->roles[place] = CLI_DOC_ROLES_KNOWN | cli_symbols_name_roles(name);
    }
}

// every name of the grammar of `one` its roles read off its segments and kept among the text's roles by its place,
// and above them one more than how many names before it count the depth of a pair, once for the grammar, so a
// coloring copies them as it claims its lines; under the lock
static void branch_roles_read(CliStoredText *one)
{
    const unsigned int names = (one->names != 0u) ? one->names : 1u;
    if (one->roles_whole || (names >= CLI_DOC_NONE))
    {
        return;
    }
    if (names > one->roles_room)
    {
        unsigned int *const grown =
            g_cli_cells->persistent_buf_alloc((unsigned long long)names * sizeof(*grown), NULL);
        if (grown == NULL)
        {
            return;
        }
        cli_cell_release(one->roles, NULL);
        one->roles = grown;
        one->roles_room = names;
    }
    memor.set(&(MemoriaCfg){
        .dst = one->roles, .val = 0, .bytes = (unsigned long long)one->roles_room * sizeof(*one->roles)});
    (void)cli_highlight_names_each(one->grammar, branch_role_read, one);
    unsigned int rank = 0u;
    for (unsigned int place = 0u; place < one->roles_room; place += 1u)
    {
        // the rank kept plus 1, a name's place among no more than CLI_DOC_RANKS
        one->roles[place] |= (rank < CLI_DOC_RANKS) ? ((rank + 1u) << CLI_DOC_RANK_AT) : 0u;
        rank += ((one->roles[place] & CLI_DOC_PAIR_ROLES) != 0u) ? 1u : 0u;
    }
    one->roles_whole = 1;
}

// the roles the name at `name` among the names of `leaves` gives a run: those the text kept as the batch's lines were
// claimed, else read off its segments
static unsigned int branch_roles(const CliBranchFinding *finding, const CliLeaves *leaves, unsigned int name)
{
    return ((name < finding->roles_count) && ((finding->roles[name] & CLI_DOC_ROLES_KNOWN) != 0u))
               ? (finding->roles[name] & (CLI_DOC_ROLES_KNOWN - 1u))
               : cli_symbols_name_roles(leaves->text + leaves->name_at[name]);
}

// how many of the names of `leaves` before the name at `name` count the depth of a pair: as the text kept it as the
// batch's lines were claimed, else counted
static unsigned int branch_depth_rank(const CliBranchFinding *finding, const CliLeaves *leaves, unsigned int name)
{
    const unsigned int kept = (name < finding->roles_count) ? (finding->roles[name] >> CLI_DOC_RANK_AT) : 0u;
    if (kept != 0u)
    {
        return kept - 1u;
    }
    unsigned int rank = 0u;
    for (unsigned int before = 0u; before < name; before += 1u)
    {
        rank += ((branch_roles(finding, leaves, before) & CLI_DOC_PAIR_ROLES) != 0u) ? 1u : 0u;
    }
    return rank;
}

// the regions of one line's leaves recorded as a line's regions are: each run in turn, a region it opens left open
// until a run after it on the line closes it, and one it closes that no run before it on the line opened counted among
// those opened above
static unsigned int branch_regions_of(const CliBranchFinding *finding, const CliLeaves *leaves)
{
    unsigned int opens = 0u;
    unsigned int closes = 0u;
    unsigned int unled = 0u;
    unsigned int found = CLI_DOC_REGION_CLOSERS;
    for (unsigned int run = 0u; run < leaves->runs; run += 1u)
    {
        const unsigned int name = leaves->name[run];
        const unsigned int end = ((run + 1u) < leaves->runs) ? leaves->col[run + 1u] : leaves->end;
        const int mark = end <= leaves->col[run];
        const unsigned int roles = branch_roles(finding, leaves, name);
        if ((roles & CLI_ROLE_REGION_OPENS) != 0u)
        {
            opens += (opens < CLI_DOC_REGION_COUNT) ? 1u : 0u;
        }
        else if (((roles & CLI_ROLE_REGION_CLOSES) != 0u) && (opens > 0u))
        {
            opens -= 1u;
        }
        else if ((roles & CLI_ROLE_REGION_CLOSES) != 0u)
        {
            found |= ((closes == 0u) && mark) ? CLI_DOC_REGION_MARK : 0u;
            unled += (((found & CLI_DOC_REGION_FILLED) == 0u) && (unled < CLI_DOC_REGION_COUNT)) ? 1u : 0u;
            closes += (closes < CLI_DOC_REGION_COUNT) ? 1u : 0u;
        }
        // a leaf is a run its grammar names of a letter or more, the empty name the first
        const int leaf = (name != 0u) && !mark;
        found &= (leaf && ((roles & CLI_ROLE_CLOSING) == 0u)) ? ~CLI_DOC_REGION_CLOSERS : ~0u;
        found |= leaf ? CLI_DOC_REGION_FILLED : 0u;
    }
    found &= (closes != 0u) ? ~0u : ~CLI_DOC_REGION_CLOSERS;
    return found | opens | (closes << CLI_DOC_REGION_CLOSES_AT) | (unled << CLI_DOC_REGION_UNLED_AT);
}

// The names a branch's grammar's tokens take written as the page reads them: the names, then each name that counts the
// depth of a pair followed by `.` and each depth, as the walk names a run of it; the place of the first of those, and
// how many of the names are written
typedef struct
{
    VerbaLinea *out;
    int depths;
    unsigned int written;
} CliBranchNames;

// one name of a grammar written, or where the names' depths are written, written once for each depth where it counts
// the depth of a pair
static void branch_name_written(void *context, unsigned int place, const char *name)
{
    (void)place;
    CliBranchNames *const names = (CliBranchNames *)context;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (!names->depths)
    {
        mmgr_line_put(names->out, (names->written != 0u) ? "," : "");
        cli_json_quoted(names->out, name, size);
        names->written += 1u;
        return;
    }
    if ((cli_symbols_name_roles(name) & CLI_DOC_PAIR_ROLES) == 0u)
    {
        return;
    }
    for (unsigned int depth = 0u; depth < CLI_DOC_DEPTHS; depth += 1u)
    {
        char deep[CLI_NAME_MAX_LEN + 8u];
        VerbaLinea text = {deep, sizeof(deep), 0ull};
        mmgr_line_put_n(&text, name, size);
        mmgr_line_ch(&text, '.');
        mmgr_line_decimal(&text, depth, 1u);
        const unsigned long long length = mmgr_line_finish(&text);
        mmgr_line_ch(names->out, ',');
        cli_json_quoted(names->out, deep, length);
    }
}

// one name of a grammar passed over, as its names are counted
static void branch_name_passed(void *context, unsigned int place, const char *name)
{
    (void)context;
    (void)place;
    (void)name;
}

void cli_document_names_written(const char *grammar, VerbaLinea *out)
{
    CliBranchNames names = {out, 0, 0u};
    mmgr_line_ch(out, '[');
    if ((grammar[0] == '\0') || (cli_highlight_names_each(grammar, branch_name_written, &names) == 0u))
    {
        mmgr_line_put(out, "\"\"]");
        return;
    }
    names.depths = 1;
    (void)cli_highlight_names_each(grammar, branch_name_written, &names);
    mmgr_line_ch(out, ']');
}

// the names of `scope` kept only where the region each belongs to is at most `depth` deep and, where `waiting` is 1,
// none waits for its region to open, the texts of those kept moved down over those let go
static void branch_scope_left(CliBranchScope *scope, unsigned int depth, int waiting)
{
    unsigned int kept = 0u;
    unsigned int used = 0u;
    for (unsigned int at = 0u; at < scope->count; at += 1u)
    {
        if ((scope->belongs[at] > depth) || (waiting && (scope->waits[at] != 0u)))
        {
            continue;
        }
        memor.move_down(&(MemoriaCfg){
            .dst = scope->text + used, .src = scope->text + scope->text_at[at], .bytes = scope->size[at]});
        scope->text_at[kept] = used;
        scope->size[kept] = scope->size[at];
        scope->named[kept] = scope->named[at];
        scope->belongs[kept] = scope->belongs[at];
        scope->waits[kept] = scope->waits[at];
        used += scope->size[kept];
        kept += 1u;
    }
    scope->moved = scope->moved || (kept != scope->count);
    scope->count = kept;
    scope->used = used;
}

// `size` bytes of `text` declared in `scope`, a use of it named `named`, belonging to the region `belongs` deep and
// waiting for it to open where `waits` is 1; passed over where the scope holds no more
static void branch_scope_put(CliBranchScope *scope, const char *text, unsigned int size, unsigned int named,
                             unsigned int belongs, int waits)
{
    if ((scope->count == CLI_DOC_SCOPE_NAMES) || ((scope->used + size) > CLI_DOC_SCOPE_TEXT))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = scope->text + scope->used, .src = text, .bytes = size});
    scope->text_at[scope->count] = scope->used;
    scope->size[scope->count] = size;
    scope->named[scope->count] = named;
    scope->belongs[scope->count] = belongs;
    // waiting is 0 or 1
    scope->waits[scope->count] = (unsigned char)waits;
    scope->used += size;
    scope->count += 1u;
    scope->moved = 1;
}

// the name a use of `size` bytes of `text` takes in `scope`, its last declaration's that does not wait: CLI_DOC_NONE
// where none declares it
static unsigned int branch_scope_named(const CliBranchScope *scope, const char *text, unsigned long long size)
{
    for (unsigned int at = scope->count; at > 0u; at -= 1u)
    {
        const unsigned int one = at - 1u;
        if ((scope->waits[one] == 0u) && (scope->size[one] == size) &&
            (memor.cmp(&(MemoriaCfg){.src = scope->text + scope->text_at[one], .other = text, .bytes = size}) == 0))
        {
            return scope->named[one];
        }
    }
    return CLI_DOC_NONE;
}

// `scope` written into `out`: its depth and the depth of its pairs as `depth.pairs`, then each name as
// `;named.belongs.waits.size:` and its text
static void branch_scope_written(const CliBranchScope *scope, VerbaLinea *out)
{
    mmgr_line_decimal(out, scope->depth, 1u);
    mmgr_line_ch(out, '.');
    mmgr_line_decimal(out, scope->pairs, 1u);
    for (unsigned int at = 0u; at < scope->count; at += 1u)
    {
        mmgr_line_ch(out, ';');
        mmgr_line_decimal(out, scope->named[at], 1u);
        mmgr_line_ch(out, '.');
        mmgr_line_decimal(out, scope->belongs[at], 1u);
        mmgr_line_ch(out, '.');
        mmgr_line_decimal(out, scope->waits[at], 1u);
        mmgr_line_ch(out, '.');
        mmgr_line_decimal(out, scope->size[at], 1u);
        mmgr_line_ch(out, ':');
        mmgr_line_put_n(out, scope->text + scope->text_at[at], scope->size[at]);
    }
}

// the number the digits from `*from` write, `*from` moved past them and past the letter `after` where it follows them,
// never past the NUL that ends them
static unsigned int branch_digits(const char **from, char after)
{
    unsigned int value = 0u;
    while (((*from)[0] >= '0') && ((*from)[0] <= '9'))
    {
        value = (value * 10u) + (unsigned int)((*from)[0] - '0');
        *from += 1;
    }
    *from += (((*from)[0] == after) && (after != '\0')) ? 1 : 0;
    return value;
}

// the scope written at the place `place` among the states of `one` read into `scope`, as far as each name's text
// stands within it, its place kept; empty where `place` is CLI_DOC_NONE or past the states
static void branch_scope_read(const CliStoredText *one, unsigned int place, CliBranchScope *scope)
{
    scope->depth = 0u;
    scope->pairs = 0u;
    scope->count = 0u;
    scope->used = 0u;
    scope->place = CLI_DOC_NONE;
    scope->moved = 1;
    if ((place == CLI_DOC_NONE) || (place >= one->state_used))
    {
        return;
    }
    const char *at = one->state + place;
    scope->depth = branch_digits(&at, '.');
    scope->pairs = branch_digits(&at, '\0');
    while (at[0] == ';')
    {
        at += 1;
        const unsigned int named = branch_digits(&at, '.');
        const unsigned int belongs = branch_digits(&at, '.');
        const int waits = branch_digits(&at, '.') != 0u;
        const unsigned int size = branch_digits(&at, ':');
        if (cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}) < size)
        {
            return;
        }
        branch_scope_put(scope, at, size, named, belongs, waits);
        at += size;
    }
    scope->place = place;
    scope->moved = 0;
}

// the scope the line `finding` colored last ends in, written into `out` where its place among the text's states is to
// be found as it is put in, written again only where it moved since the one before it was: its bytes, CLI_DOC_NONE for
// a scope of no depth, no pairs and no names or one too large to write, and CLI_DOC_SCOPE_SAME for the one before it
static unsigned int branch_scope_found(CliBranchFinding *finding, VerbaLinea *out)
{
    CliBranchScope *const scope = &finding->scope;
    if ((scope->depth == 0u) && (scope->pairs == 0u) && (scope->count == 0u))
    {
        return CLI_DOC_NONE;
    }
    if (!scope->moved && finding->placed)
    {
        return CLI_DOC_SCOPE_SAME;
    }
    branch_scope_written(scope, out);
    const unsigned long long size = mmgr_line_finish(out);
    scope->moved = 0;
    finding->placed = size != 0ull;
    // a scope written within its room, which an unsigned int counts
    return (size != 0ull) ? (unsigned int)size : CLI_DOC_NONE;
}

// one line's leaves walked through the scope of `finding`: a region its runs open deepens it and its function's
// parameters waiting
// for it open in it, one they close lets go the names that belong to it, a local is declared in the region it stands
// in and a parameter in the one the next region opens, a statement's end or a directive lets go the parameters still
// waiting, at once and at the line's end; each bare name a use of the last declaration of it in the scope takes that
// declaration's name, but one reached through the operator before it, blanks between; and a pair it opens deepens the
// scope's pairs and one it closes lets one go, each run that opens or closes one named by the depth it stands at, the
// first of its names' `names` on, as `cli_document_names_written` writes them. The leaves with each run's name so
// taken, in the work's room for them, the leaves as they came where the cells cannot meet that room
static CliLeaves branch_uses_named(CliBranchFinding *finding, const CliLeaves *leaves)
{
    CliLeaves named = *leaves;
    CliBranchScope *const scope = &finding->scope;
    const unsigned int names = finding->names;
    CliBranchWork *const work = finding->work;
    const int renaming = (leaves->runs != 0u) &&
                         branch_room_made(&work->named, &work->named_room,
                                          (unsigned long long)leaves->runs * sizeof(unsigned int), 0ull);
    unsigned int *const renamed = (unsigned int *)work->named;
    const char *const line = leaves->line;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
    unsigned long long byte = 0ull;
    unsigned int col = 0u;
    int ended = 0;
    int reached = 0;
    for (unsigned int run = 0u; run < leaves->runs; run += 1u)
    {
        const unsigned int name = leaves->name[run];
        const unsigned int roles = branch_roles(finding, leaves, name);
        const int through = reached;
        // the empty name, a blank's, stands between an operator and the name it reaches
        reached = ((roles & CLI_ROLE_REACHES) != 0u) || (reached && (name == 0u));
        const unsigned long long from = cli_symbols_byte_at(line, size, leaves->col[run], &byte, &col);
        const unsigned int end = ((run + 1u) < leaves->runs) ? leaves->col[run + 1u] : leaves->end;
        const unsigned long long to = cli_symbols_byte_at(line, size, end, &byte, &col);
        // a run's bytes lie within its line, which an unsigned int counts
        const unsigned int bytes = (unsigned int)(to - from);
        if (renaming)
        {
            renamed[run] = name;
        }
        if ((roles & CLI_ROLE_REGION_OPENS) != 0u)
        {
            scope->depth += 1u;
            scope->moved = 1;
            for (unsigned int at = 0u; at < scope->count; at += 1u)
            {
                scope->waits[at] = (scope->belongs[at] == scope->depth) ? 0u : scope->waits[at];
            }
        }
        else if (((roles & CLI_ROLE_REGION_CLOSES) != 0u) && (scope->depth > 0u))
        {
            scope->depth -= 1u;
            scope->moved = 1;
            branch_scope_left(scope, scope->depth, 0);
        }
        if ((roles & CLI_ROLE_PARAMETER) != 0u)
        {
            branch_scope_put(scope, line + from, bytes, name, scope->depth + 1u, 1);
        }
        else if ((roles & CLI_ROLE_LOCAL) != 0u)
        {
            branch_scope_put(scope, line + from, bytes, name, scope->depth, 0);
        }
        else if (renaming && !through && ((roles & CLI_ROLE_NAME) != 0u))
        {
            const unsigned int declared = branch_scope_named(scope, line + from, bytes);
            renamed[run] = (declared != CLI_DOC_NONE) ? declared : name;
        }
        if ((roles & CLI_ROLE_ENDS) != 0u)
        {
            branch_scope_left(scope, CLI_DOC_NONE, 1);
            ended = 1;
        }
        // a pair that closes stands at the depth of the one it closes
        const int closes = (roles & CLI_ROLE_DEPTH_CLOSES) != 0u;
        const int opens = (roles & CLI_ROLE_DEPTH_OPENS) != 0u;
        scope->pairs -= (closes && (scope->pairs > 0u)) ? 1u : 0u;
        const unsigned int rank = ((closes || opens) && renaming && (names != 0u))
                                      ? branch_depth_rank(finding, leaves, name)
                                      : CLI_DOC_NONE;
        if (rank != CLI_DOC_NONE)
        {
            renamed[run] = names + (rank * CLI_DOC_DEPTHS) + (scope->pairs % CLI_DOC_DEPTHS);
        }
        scope->pairs += opens ? 1u : 0u;
        scope->moved = scope->moved || closes || opens;
    }
    if (ended)
    {
        branch_scope_left(scope, CLI_DOC_NONE, 1);
    }
    named.name = renaming ? renamed : leaves->name;
    return named;
}

// one line's leaves, colored with no lock held: its runs named through the scope, its regions read, and the state the
// colorer leaves it in and the scope it ends in written after what the batch found on the lines before it, a record of
// its own; nothing where the cells refused the room for one before it or refuse it
static void branch_found(void *context, const CliLeaves *leaves)
{
    CliBranchFinding *const finding = (CliBranchFinding *)context;
    if (finding->refused)
    {
        return;
    }
    const CliLeaves named = branch_uses_named(finding, leaves);
    const unsigned int regions = branch_regions_of(finding, leaves);
    char scoped[CLI_DOC_STATE];
    VerbaLinea scope = {scoped, sizeof(scoped), 0ull};
    const unsigned int scope_size = branch_scope_found(finding, &scope);
    const unsigned int scope_bytes =
        ((scope_size != CLI_DOC_NONE) && (scope_size != CLI_DOC_SCOPE_SAME)) ? scope_size : 0u;
    // the colorer's state, no longer than the room it is written in, which an unsigned int counts
    const unsigned int ended =
        (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = leaves->ended, .cap = CLI_CELLS_POOL});
    const unsigned long long ended_words = (ended + 3ull) / 4ull;
    const unsigned long long words =
        CLI_DOC_FOUND_HEAD + (2ull * named.runs) + ended_words + ((scope_bytes + 3ull) / 4ull);
    CliBranchWork *const work = finding->work;
    if (!branch_room_made(&work->found, &work->found_room, (finding->used + words) * sizeof(unsigned int),
                          finding->used * sizeof(unsigned int)))
    {
        finding->refused = 1;
        return;
    }
    unsigned int *const record = (unsigned int *)work->found + finding->used;
    record[0] = named.runs;
    record[1] = regions;
    record[2] = ended;
    record[3] = scope_size;
    unsigned int *const cols = record + CLI_DOC_FOUND_HEAD;
    memor.cpy(&(MemoriaCfg){.dst = cols, .src = named.col, .bytes = named.runs * sizeof(*cols)});
    memor.cpy(&(MemoriaCfg){.dst = cols + named.runs, .src = named.name, .bytes = named.runs * sizeof(*cols)});
    char *const text = (char *)(cols + (2u * named.runs));
    memor.cpy(&(MemoriaCfg){.dst = text, .src = leaves->ended, .bytes = ended});
    memor.cpy(&(MemoriaCfg){.dst = text + (4ull * ended_words), .src = scoped, .bytes = scope_bytes});
    finding->used += words;
}

// whether a coloring under way claims the line `line` of `one`
static int branch_claimed(const CliStoredText *one, unsigned int line)
{
    for (unsigned int at = 0u; at < CLI_DOC_CLAIMS; at += 1u)
    {
        if ((line >= one->claim_from[at]) && (line < one->claim_to[at]))
        {
            return 1;
        }
    }
    return 0;
}

// whether a coloring under way claims any line of `one`
static int branch_claims_any(const CliStoredText *one)
{
    for (unsigned int at = 0u; at < CLI_DOC_CLAIMS; at += 1u)
    {
        if (one->claim_to[at] > one->claim_from[at])
        {
            return 1;
        }
    }
    return 0;
}

// the lines from `first` to before `end` of `one` claimed by a coloring under way: the claim's place among those of the
// text, CLI_DOC_CLAIMS where every place is taken and none is claimed
static unsigned int branch_claim_put(CliStoredText *one, unsigned int first, unsigned int end)
{
    for (unsigned int at = 0u; at < CLI_DOC_CLAIMS; at += 1u)
    {
        if (one->claim_from[at] == one->claim_to[at])
        {
            one->claim_from[at] = first;
            one->claim_to[at] = end;
            return at;
        }
    }
    return CLI_DOC_CLAIMS;
}

// the claim at `at` among those of `one` let go, and the walk woken for the lines it claimed, which it left to it
static void branch_claim_dropped(CliStoredText *one, unsigned int at)
{
    if (at < CLI_DOC_CLAIMS)
    {
        one->claim_from[at] = 0u;
        one->claim_to[at] = 0u;
    }
    cli_document_woken(one);
}

// what `finding` found on the lines from `first` put into `one` in turn, from the state `*entering` the first is
// entered in and the place `place` among the text's states of the scope it is entered in: each line's state it is
// entered in and the state it ends in, its regions, its runs kept, a paged text's where the page holds the line or the
// range hands it on, and handed on where the range hands it on, each no further than the text's columns where the range
// names them; where `stopping` is 1, stopped at the first line entered in the state it was entered in before,
// `*stopped` set. The line after the last put; under the lock
static unsigned int branch_found_put(CliStoredText *one, const CliBranchFinding *finding, unsigned int first,
                                     unsigned int place, int stopping, unsigned int *entering, int *stopped,
                                     CliBranchRange *range)
{
    CliLines *const lines = &one->lines;
    const unsigned int hand_from = range->asked ? range->hand_from : one->held_from;
    const unsigned int hand_to = range->asked ? range->hand_to : one->held_to;
    range->columns = (range->columns != 0ull) ? one->columns : 0ull;
    const unsigned int *record = (const unsigned int *)finding->work->found;
    const unsigned int *const last = record + finding->used;
    unsigned int line = first;
    unsigned int scoped = place;
    while ((record < last) && (line < lines->lines) && !one->refused)
    {
        if (stopping && (lines->begun[line] == *entering))
        {
            *stopped = 1;
            break;
        }
        const unsigned int runs = record[0];
        const unsigned int ended_words = (record[2] + 3u) / 4u;
        const char *const ended = (const char *)(record + CLI_DOC_FOUND_HEAD + (2u * runs));
        const unsigned int scope_size = record[3];
        const int written = (scope_size != CLI_DOC_NONE) && (scope_size != CLI_DOC_SCOPE_SAME);
        if (written)
        {
            char scope[CLI_DOC_STATE];
            memor.cpy(&(MemoriaCfg){.dst = scope, .src = ended + (4u * ended_words), .bytes = scope_size});
            scope[scope_size] = '\0';
            scoped = branch_state(one, scope);
        }
        // the state the line ends in the colorer's with the place of its scope after the mark, the colorer's alone
        // where the scope has no place or the two do not fit
        char state[CLI_DOC_STATE];
        VerbaLinea out = {state, sizeof(state), 0ull};
        mmgr_line_put_n(&out, ended, record[2]);
        const unsigned long long colorer = out.at;
        if ((scope_size != CLI_DOC_NONE) && (scoped != CLI_DOC_NONE))
        {
            mmgr_line_ch(&out, CLI_DOC_SCOPE_MARK);
            mmgr_line_decimal(&out, scoped, 1u);
        }
        if (mmgr_line_finish(&out) == 0ull)
        {
            state[colorer] = '\0';
        }
        lines->begun[line] = *entering;
        lines->ended[line] = branch_state(one, state);
        one->regions_moved = one->regions_moved || (lines->regions[line] != record[1]);
        lines->regions[line] = record[1];
        const CliLeaves kept = {
            NULL, runs, record + CLI_DOC_FOUND_HEAD, record + CLI_DOC_FOUND_HEAD + runs, NULL, NULL, NULL, 0u};
        const int handed = (line >= hand_from) && (line < hand_to);
        const int held = (line >= one->held_from) && (line < one->held_to);
        if (!one->paged || held || handed)
        {
            branch_runs_kept(one, line, &kept);
        }
        else
        {
            lines->runs[line] = CLI_DOC_NO_RUNS;
        }
        if (handed)
        {
            branch_runs_put(range, line, &kept);
        }
        *entering = lines->ended[line];
        line += 1u;
        record += CLI_DOC_FOUND_HEAD + (2u * runs) + ended_words + (written ? ((scope_size + 3u) / 4u) : 0u);
    }
    return line;
}

// one batch of the lines from `first` to before `to` of `one` colored from the state `*entering` the first is entered
// in: no more than CLI_DOC_BATCH lines where `stopping` is 1, else CLI_DOC_RUN, CLI_DOC_STEP where the range's bytes
// reach no further, and of those as many as its bytes reach, one at the least, those spent taken from them. The lines
// are claimed under the lock, read and colored in `work` with it given back, and put in under it again, the text held
// across it by its caller; where the text moved meanwhile none is put in and the range is marked stale, and where the
// grammar or the cells refused them none is and the text is marked refused. The line after the last put; under the lock
static unsigned int branch_batch_colored(CliStoredText *one, unsigned int first, unsigned int to, int stopping,
                                         CliBranchWork *work, unsigned int *entering, int *stopped,
                                         CliBranchRange *range)
{
    const unsigned int most =
        stopping ? CLI_DOC_BATCH : ((range->bytes != CLI_DOC_ALL_BYTES) ? CLI_DOC_STEP : CLI_DOC_RUN);
    const unsigned int end = ((to - first) > most) ? (first + most) : to;
    // what the batch reads with the lock given back, copied as its lines are claimed: the text's serial, its key and
    // its grammar's, whether it is paged and whether its last line ends it, and how many lines it counts
    const unsigned long long serial = one->serial;
    char key[CLI_NAME_MAX_LEN];
    char grammar[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = key, .src = one->key, .bytes = sizeof(key)});
    memor.cpy(&(MemoriaCfg){.dst = grammar, .src = one->grammar, .bytes = sizeof(grammar)});
    const int paged = one->paged;
    const int whole = one->whole;
    const unsigned int count = one->lines.lines;
    CliBranchFinding finding;
    finding.work = work;
    finding.names = one->names;
    finding.used = 0ull;
    finding.refused = 0;
    // the state the first line is entered in, copied out, the colorer's own handed to it and the scope at the place
    // after the mark read into the batch
    char state[CLI_DOC_STATE];
    VerbaLinea entered = {state, sizeof(state), 0ull};
    mmgr_line_put(&entered, one->state + *entering);
    const int stated = mmgr_line_finish(&entered) != 0ull;
    const char *const mark =
        stated ? cellul.chr(&(CatenaFinitaCfg){.src = state, .cap = CLI_CELLS_POOL, .byte = CLI_DOC_SCOPE_MARK}) : NULL;
    const char *place = (mark != NULL) ? (mark + 1) : NULL;
    branch_scope_read(one, (place != NULL) ? branch_digits(&place, '\0') : CLI_DOC_NONE, &finding.scope);
    finding.placed = finding.scope.place != CLI_DOC_NONE;
    if (mark != NULL)
    {
        // the mark stands within the state copied
        state[mark - state] = '\0';
    }
    // the roles of the grammar's names copied, read first where they were not
    branch_roles_read(one);
    const unsigned int roles = one->roles_whole ? one->roles_room : 0u;
    finding.roles_count =
        ((roles != 0u) &&
         branch_room_made(&work->roles, &work->roles_room, (unsigned long long)roles * sizeof(unsigned int), 0ull))
            ? roles
            : 0u;
    if (finding.roles_count != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = work->roles, .src = one->roles, .bytes = roles * sizeof(unsigned int)});
    }
    finding.roles = (const unsigned int *)work->roles;
    unsigned int read = first;
    const char **line = paged ? NULL : branch_lines_copied(&one->lines, first, end, range->bytes, work, &read);
    const unsigned int claim = branch_claim_put(one, first, end);
    cli_document_lock_give();
    char *taken = NULL;
    if (paged)
    {
        line = branch_lines_paged(key, first, end, work, &read, &taken);
    }
    // the lines read colored as far as the range's bytes reach, one at the least
    unsigned int spend = 0u;
    unsigned long long spent = 0ull;
    while ((line != NULL) && ((first + spend) < read) &&
           ((spend == 0u) || (range->bytes == CLI_DOC_ALL_BYTES) || (spent < range->bytes)))
    {
        spent += (range->bytes != CLI_DOC_ALL_BYTES)
                     ? (cellul.len(&(CatenaFinitaCfg){.src = line[spend], .cap = CLI_CELLS_POOL}) + 1ull)
                     : 0ull;
        spend += 1u;
    }
    if (range->bytes != CLI_DOC_ALL_BYTES)
    {
        range->bytes -= (spent < range->bytes) ? spent : range->bytes;
    }
    int colored = 0;
    if ((line != NULL) && (spend != 0u))
    {
        char ended[CLI_DOC_STATE];
        VerbaLinea last = {ended, sizeof(ended), 0ull};
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        colored = cli_highlight_leaves(grammar, stated ? state : "root", line, spend,
                                       ((first + spend) == count) && whole, branch_found, &finding, &last, &err_msg);
    }
    cli_cell_release(taken, NULL);
    cli_document_lock_take();
    branch_claim_dropped(one, claim);
    if (one->let_go || (one->serial != serial))
    {
        range->stale = 1;
        return first;
    }
    if (!colored)
    {
        one->refused = 1;
        return first;
    }
    const unsigned int put =
        branch_found_put(one, &finding, first, finding.scope.place, stopping, entering, stopped, range);
    one->refused = one->refused || finding.refused;
    return put;
}

// the lines from `from` to `to` of `one` colored, the first entered in the state the line above it ends in, root where
// it is the first or the line above is not colored; where `stopping` is 1, stopped at the first entered in the state it
// was entered in before, each line colored whole from left to right, and no more lines than the range's bytes reach, the
// last maybe past them, those spent taken from them, a batch at a time in a work claimed for them. The runs of those the
// range hands on handed on in it, each line's no further than the range's columns where it names them: the line after
// the last colored. Under the lock, which it gives back as each batch is colored, the text held across it by its caller
static unsigned int branch_colored(CliStoredText *one, unsigned int from, unsigned int to, int stopping,
                                   CliBranchRange *range)
{
    const unsigned int above = (from != 0u) ? one->lines.ended[from - 1u] : CLI_DOC_UNCOLORED;
    unsigned int entering = (above != CLI_DOC_UNCOLORED) ? above : CLI_DOC_ROOT;
    unsigned int line = from;
    int stopped = 0;
    CliBranchWork work;
    branch_work_emptied(&work);
    while (!stopped && !one->refused && !range->stale && (line < to) && (range->bytes != 0ull))
    {
        const unsigned int next = branch_batch_colored(one, line, to, stopping, &work, &entering, &stopped, range);
        // a batch that puts no line in ends the coloring
        stopped = stopped || (next == line);
        line = next;
    }
    branch_work_given(&work);
    return line;
}

// whether the line `line` of `one` is one to color: it is not colored, and where `yielding` is 1, as the walk yields,
// no coloring under way claims it
static int branch_uncolored(const CliStoredText *one, unsigned int line, int yielding)
{
    return (one->lines.begun[line] == CLI_DOC_UNCOLORED) && !(yielding && branch_claimed(one, line));
}

// the run of lines of `one` to color about `at`, below the walk's origin where `down` is 1: its first and the line after
// its last into `first` and `end`, no more than CLI_DOC_STEP of them reaching from `at`, down or up, none outside the
// lines from `low` to before `high` the walk colors, and where `yielding` is 1 none a coloring under way claims; a run
// down whose line above ends in no known state reaching as far above as CLI_DOC_LEAD lines to color reach
static void branch_run_of(const CliStoredText *one, unsigned int at, int down, unsigned int low, unsigned int high,
                          int yielding, unsigned int *first, unsigned int *end)
{
    unsigned int start = at;
    unsigned int stop = at + 1u;
    while (down && (stop < high) && branch_uncolored(one, stop, yielding) && ((stop - at) < CLI_DOC_STEP))
    {
        stop += 1u;
    }
    while (!down && (start > low) && branch_uncolored(one, start - 1u, yielding) &&
           ((at + 1u - start) < CLI_DOC_STEP))
    {
        start -= 1u;
    }
    for (unsigned int lead = 0u;
         down && (start > low) && branch_uncolored(one, start - 1u, yielding) && (lead < CLI_DOC_LEAD); lead += 1u)
    {
        start -= 1u;
    }
    *first = start;
    *end = stop;
}

// the lines of `one` the walk colors, from `*low` to before `*high`: a paged text's those the page holds alone, the
// first of them entered from root, and every line of any other
static void branch_bounds(const CliStoredText *one, unsigned int *low, unsigned int *high)
{
    const unsigned int lines = one->lines.lines;
    *high = (one->paged && (one->held_to < lines)) ? one->held_to : lines;
    *low = one->paged ? ((one->held_from < *high) ? one->held_from : *high) : 0u;
}

// a run of lines handed on in `out` begun: their runs written as `{"from", "runs"}`, after a comma where `*any` is set,
// and `*any` set where they are, each line's no further than `columns` letters where it is not 0, no more lines than
// `bytes` reach, those the page holds handed on, and none colored past `high`
static CliBranchRange branch_range_begun(const CliStoredText *one, int *any, VerbaLinea *out,
                                         unsigned long long columns, unsigned long long bytes, unsigned int high)
{
    const CliBranchRange range = {0, 0u, any, out, columns, one->names, bytes, 0u, 0, 0, 0u, 0u, high};
    return range;
}

// the lines from `first` to `end` of `one` colored in `range` and the lines after them colored again until one is
// entered in the state it was entered in before, no more than CLI_DOC_STEP of those and none past the range's last;
// where its columns are not 0, the lines after them left to the walk unless `onward` is 1, as the walk's own step colors
// them. Under the lock, which each batch gives back as it is colored, the range marked stale where the text moved
static void branch_range_colored(CliStoredText *one, unsigned int first, unsigned int end, int onward,
                                 CliBranchRange *range)
{
    const unsigned int line = branch_colored(one, first, end, 0, range);
    const unsigned int high = range->high;
    if ((onward || (range->columns == 0ull)) && !range->stale && (line == end) && (end < high) &&
        (end < one->lines.lines) && (one->lines.begun[end] != CLI_DOC_UNCOLORED))
    {
        const unsigned int most = ((high - end) > CLI_DOC_STEP) ? (end + CLI_DOC_STEP) : high;
        (void)branch_colored(one, end, most, 1, range);
    }
}

// colors handed on begun: `{"key":` the text's key `,"seq":` the changes written to it `,"colors":`
static void branch_colors_begun(const CliStoredText *one, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"key\":");
    cli_json_quoted(out, one->key, cellul.len(&(CatenaFinitaCfg){.src = one->key, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"seq\":");
    mmgr_line_decimal(out, one->changes, 1u);
    mmgr_line_put(out, ",\"colors\":");
}

// colors handed on ended: `,"names":` the names the text's grammar's tokens take where `any` colors go and they were not
// handed on since it was colored by the grammar, the first colors handed on carrying them, and `}`; under the lock the
// colors are handed on after, so no colors go before the names they are named by
static void branch_colors_ended(CliStoredText *one, int any, VerbaLinea *out)
{
    if (any && !one->named)
    {
        mmgr_line_put(out, ",\"names\":");
        cli_document_names_written(one->grammar, out);
        one->named = 1;
    }
    mmgr_line_ch(out, '}');
}

// one step of the walk of `one`: the run of lines to color nearest its origin colored, or where every line is colored
// or claimed, the first entered in a state the line above does not end in colored again, written into `out` as
// `{"key", "seq", "colors"}`, `*handed` set where it holds the runs of a line the page holds: 1 where it colored a line,
// else 0. A paged text's lines are counted again first, and it is walked across the lines the page holds alone. Under
// the lock, which it gives back as it colors, the text held across it by its caller
static int branch_step(CliStoredText *one, VerbaLinea *out, int *handed)
{
    const CliLines *const lines = &one->lines;
    one->refused = one->refused || (one->paged && !cli_document_paged_counted(one));
    unsigned int low = 0u;
    unsigned int high = 0u;
    branch_bounds(one, &low, &high);
    if ((one->grammar[0] == '\0') || one->refused || (high <= low) || (one->held_to <= one->held_from))
    {
        return 0;
    }
    const unsigned int origin = (one->origin < low) ? low : ((one->origin < high) ? one->origin : (high - 1u));
    unsigned int at = CLI_DOC_NONE;
    int down = 1;
    for (unsigned int distance = 0u;
         (at == CLI_DOC_NONE) && (((origin + distance) < high) || (distance < (origin - low))); distance += 1u)
    {
        if (((origin + distance) < high) && branch_uncolored(one, origin + distance, 1))
        {
            at = origin + distance;
        }
        else if ((distance < (origin - low)) && branch_uncolored(one, origin - distance - 1u, 1))
        {
            at = origin - distance - 1u;
            down = 0;
        }
    }
    unsigned int first = CLI_DOC_NONE;
    unsigned int end = 0u;
    if (at != CLI_DOC_NONE)
    {
        branch_run_of(one, at, down, low, high, 1, &first, &end);
    }
    // the first line of the lines walked entered in whatever state it was entered in, where it is not the text's first
    for (unsigned int line = (low != 0u) ? (low + 1u) : 0u;
         (at == CLI_DOC_NONE) && (first == CLI_DOC_NONE) && (line < high); line += 1u)
    {
        if ((lines->begun[line] != ((line == 0u) ? CLI_DOC_ROOT : lines->ended[line - 1u])) &&
            !branch_claimed(one, line))
        {
            first = line;
            end = line + 1u;
        }
    }
    if (first == CLI_DOC_NONE)
    {
        return 0;
    }
    branch_colors_begun(one, out);
    mmgr_line_ch(out, '[');
    CliBranchRange range = branch_range_begun(one, handed, out, one->columns, CLI_DOC_STEP_BYTES, high);
    branch_range_colored(one, first, end, 1, &range);
    branch_range_ended(&range);
    mmgr_line_ch(out, ']');
    branch_colors_ended(one, *handed, out);
    return 1;
}

// the mark of a fold that shows on its first line the line it closes on, after what it hides
#define CLI_DOC_FOLD_SHOWN 0x80000000u

// the fold of the region opened on the line `opened` that the line `line` closes, the `close`th region it closes, the
// line's regions `regions`, the last line above it that holds a leaf `filled`: the last line it hides, plus 1, marked
// CLI_DOC_FOLD_SHOWN where it shows the line it closes on, or 0 where it hides no line. A mark of no letters closes a
// region on the line it ends with a leaf before it, else on the last line above that holds one; a bracket closes it on
// the line before its own where a leaf stands before it or another leaf on its line closes nothing, and on its own,
// shown, where its line holds closes alone
static unsigned int branch_fold_of(unsigned int opened, unsigned int line, unsigned int close, unsigned int regions,
                                   unsigned int filled)
{
    const int mark = (regions & CLI_DOC_REGION_MARK) != 0u;
    const int led = close >= ((regions >> CLI_DOC_REGION_UNLED_AT) & CLI_DOC_REGION_COUNT);
    const int shown = !mark && !led && ((regions & CLI_DOC_REGION_CLOSERS) != 0u);
    const unsigned int last = (mark && led) ? line : mark ? filled : shown ? line : (line - 1u);
    // a line shown on the fold's first line is no line hidden
    const unsigned int hidden = shown ? (last - 1u) : last;
    if ((last == CLI_DOC_NONE) || (hidden <= opened))
    {
        return 0u;
    }
    return (last + 1u) | (shown ? CLI_DOC_FOLD_SHOWN : 0u);
}

// The regions open above a line as its folds are found: the line each opens on, the innermost last, in the room the
// walk keeps for them, how many, and whether the cells refused it
typedef struct
{
    unsigned int *line;
    unsigned int count;
    int refused;
} CliBranchOpen;

// a region opened on `line` put after those open: refused where the cells cannot meet it
static void branch_open_put(CliBranchOpen *open, unsigned int line)
{
    const unsigned long long held = (unsigned long long)open->count * sizeof(unsigned int);
    open->refused = open->refused || !branch_room_made(&s_opened, &s_opened_room, held + sizeof(unsigned int), held);
    if (!open->refused)
    {
        open->line = (unsigned int *)s_opened;
        open->line[open->count] = line;
        open->count += 1u;
    }
}

// the folds of `one` found, each handed to `each` with its first line, the last line it hides and whether it shows the
// line it closes on after what it hides: each fold the widest of the regions that open on its first line, in the order
// of their first lines; a paged text's those of the lines the page mirrors alone, and none where the cells cannot meet
// the room they are found in. Under the texts' lock
static void branch_folds_each(const CliStoredText *one, CliFoldEach each, void *context)
{
    const CliLines *const lines = &one->lines;
    unsigned int first = 0u;
    unsigned int end = 0u;
    branch_bounds(one, &first, &end);
    const unsigned int kept = end - first;
    const unsigned long long widest_bytes = (unsigned long long)kept * sizeof(unsigned int);
    const int roomed = (kept != 0u) && branch_room_made(&s_widest, &s_widest_room, widest_bytes, 0ull);
    unsigned int *const widest = roomed ? (unsigned int *)s_widest : NULL;
    CliBranchOpen open = {NULL, 0u, !roomed};
    if (widest != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = widest, .val = 0, .bytes = widest_bytes});
    }
    unsigned int filled = CLI_DOC_NONE;
    for (unsigned int line = first; !open.refused && (line < end); line += 1u)
    {
        const unsigned int regions = lines->regions[line];
        const unsigned int closes = (regions >> CLI_DOC_REGION_CLOSES_AT) & CLI_DOC_REGION_COUNT;
        for (unsigned int close = 0u; (close < closes) && (open.count != 0u); close += 1u)
        {
            open.count -= 1u;
            const unsigned int opened = open.line[open.count];
            const unsigned int fold = branch_fold_of(opened, line, close, regions, filled);
            if ((fold & ~CLI_DOC_FOLD_SHOWN) > (widest[opened - first] & ~CLI_DOC_FOLD_SHOWN))
            {
                widest[opened - first] = fold;
            }
        }
        for (unsigned int opens = regions & CLI_DOC_REGION_COUNT; opens != 0u; opens -= 1u)
        {
            branch_open_put(&open, line);
        }
        filled = ((regions & CLI_DOC_REGION_FILLED) != 0u) ? line : filled;
    }
    for (unsigned int at = 0u; !open.refused && (at < kept); at += 1u)
    {
        if (widest[at] != 0u)
        {
            each(context, first + at, (widest[at] & ~CLI_DOC_FOLD_SHOWN) - 1u, (widest[at] & CLI_DOC_FOLD_SHOWN) != 0u);
        }
    }
}

// a fold written into the line `context`, `[first, last, shown]` after a comma where one was written before it
static void branch_fold_written(void *context, unsigned long long first, unsigned long long last, int shown)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->out[out->at - 1ull] == '[') ? "[" : ",[");
    mmgr_line_decimal(out, first, 1u);
    mmgr_line_ch(out, ',');
    mmgr_line_decimal(out, last, 1u);
    mmgr_line_put(out, shown ? ",1]" : ",0]");
}

// the folds of `one` written into `out` as `{"key", "seq", "folds": [[first, last, shown], ...]}`, `seq` the changes
// written to it, each as branch_folds_each finds them. Its regions marked as handed on
static void branch_folds_written(CliStoredText *one, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"key\":");
    cli_json_quoted(out, one->key, cellul.len(&(CatenaFinitaCfg){.src = one->key, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"seq\":");
    mmgr_line_decimal(out, one->changes, 1u);
    mmgr_line_put(out, ",\"folds\":[");
    branch_folds_each(one, branch_fold_written, out);
    mmgr_line_put(out, "]}");
    one->regions_moved = 0;
}

int cli_document_folds_each(const char *key, CliFoldEach each, void *context)
{
    cli_document_lock_take();
    const CliStoredText *const one = cli_document_stored(key);
    if (one != NULL)
    {
        branch_folds_each(one, each, context);
    }
    cli_document_lock_give();
    return one != NULL;
}

unsigned int cli_document_openers(const char *key, unsigned int line, unsigned int *opener, unsigned int most)
{
    cli_document_lock_take();
    const CliStoredText *const one = cli_document_stored(key);
    CliBranchOpen open = {NULL, 0u, 0};
    const unsigned int end = (one != NULL) ? ((line < one->lines.lines) ? line : one->lines.lines) : 0u;
    for (unsigned int at = 0u; !open.refused && (at < end); at += 1u)
    {
        const unsigned int regions = one->lines.regions[at];
        const unsigned int closes = (regions >> CLI_DOC_REGION_CLOSES_AT) & CLI_DOC_REGION_COUNT;
        open.count -= (closes < open.count) ? closes : open.count;
        for (unsigned int opens = regions & CLI_DOC_REGION_COUNT; opens != 0u; opens -= 1u)
        {
            branch_open_put(&open, at);
        }
    }
    const unsigned int count = (open.refused || (open.count < most)) ? (open.refused ? 0u : open.count) : most;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        opener[at] = open.line[open.count - count + at];
    }
    cli_document_lock_give();
    return count;
}

// whether the folds of `one` go on: its regions moved since they last went, every line of it colored, the walk's step
// over it having colored none and no coloring under way claiming any, and the page holding lines of it
static int branch_folds_due(const CliStoredText *one)
{
    return one->regions_moved && !one->refused && (one->held_to > one->held_from) && (one->lines.lines != 0u) &&
           !branch_claims_any(one);
}

// colors handed on to where they go, `colors` where `any` line's runs or folds were written into it: the lock they are
// handed on under taken before the texts' lock is given back, so they go in the order they were put in; the texts' lock
// given back either way
static void branch_handed_out(int any, VerbaLinea *colors)
{
    const CliDocColored colored = s_colored;
    const int handing = any && (colored != NULL) && mmgr_line_ok(colors);
    if (handing)
    {
        cli_lock_take(&s_handing);
    }
    cli_document_lock_give();
    if (handing)
    {
        colored(colors->out, colors->at);
        cli_lock_give(&s_handing);
    }
}

// the walk taken by `walker` step by step until no text it may walk has a line to color: each step on a text no other
// walker walks and the walk did not find colored whole since it last moved, the text held across the lock given back
// as it colors, and the step's colors handed on once it is; on a walker's own thread where `own` is 1, marked as
// walking no more as it ends
static void branch_walked(CliBranchWalker *walker, int own)
{
    for (;;)
    {
        // the line a step's colors are written into, taken as the walk begins and emptied for each step
        const int made = (walker->step.out != NULL) || cli_growing_line_made(&walker->step, CLI_DOC_COLORS_FIRST);
        walker->step.at = 0u;
        walker->step.flushed = 0u;
        cli_document_lock_take();
        CliStoredText *one = made ? cli_document_texts() : NULL;
        while ((one != NULL) && (one->walked || one->resting))
        {
            one = one->next;
        }
        int handed = 0;
        if (one != NULL)
        {
            one->walked = 1;
            cli_document_holder_added(one);
            int stepped = branch_step(one, &walker->step, &handed);
            // a text the step colored none of is colored whole: its folds go on where its regions moved
            if (!stepped && branch_folds_due(one))
            {
                branch_folds_written(one, &walker->step);
                stepped = 1;
                handed = 1;
            }
            one->resting = !stepped;
            one->walked = 0;
            cli_document_holder_ended(one);
        }
        if (one == NULL)
        {
            // the step's line given back as the walk ends, before the walker is marked free to walk again
            cli_cell_release(walker->step.out, NULL);
            walker->step.out = NULL;
            walker->walking = own ? 0 : walker->walking;
            cli_document_lock_give();
            return;
        }
        branch_handed_out(handed, &walker->step);
    }
}

static void branch_walk_run(void *context)
{
    branch_walked((CliBranchWalker *)context, 1);
}

void cli_document_woken(CliStoredText *one)
{
    if (one->let_go)
    {
        return;
    }
    one->resting = 0;
    // the walkers walking and the first free, the texts a walker walks now, and those that wait for one
    unsigned int walking = 0u;
    unsigned int free_at = CLI_DOC_WALKERS;
    for (unsigned int at = 0u; at < CLI_DOC_WALKERS; at += 1u)
    {
        walking += s_walkers[at].walking ? 1u : 0u;
        free_at = (!s_walkers[at].walking && (free_at == CLI_DOC_WALKERS)) ? at : free_at;
    }
    unsigned int walked = 0u;
    unsigned int waiting = 0u;
    for (const CliStoredText *text = cli_document_texts(); text != NULL; text = text->next)
    {
        walked += text->walked ? 1u : 0u;
        waiting += (!text->walked && !text->resting) ? 1u : 0u;
    }
    // a walker started where more texts wait than the walkers between steps take up, and fewer walk than may
    const unsigned int between = (walking > walked) ? (walking - walked) : 0u;
    if (s_threaded && (free_at != CLI_DOC_WALKERS) && (walking < s_walkers_most) && (waiting > between))
    {
        CliBranchWalker *const walker = &s_walkers[free_at];
        walker->thread = (CliThread){branch_walk_run, walker, CLI_PRIORITY_WORK};
        walker->walking = cli_thread_started(&walker->thread);
    }
}

void cli_document_colors_to(CliDocColored colored, int threaded)
{
    const unsigned int spare = cli_processors() - 1u;
    cli_document_lock_take();
    s_colored = colored;
    s_threaded = threaded;
    s_walkers_most = (spare == 0u) ? 1u : ((spare < CLI_DOC_WALKERS) ? spare : CLI_DOC_WALKERS);
    cli_document_lock_give();
}

int cli_document_colored_by(const char *key, const char *grammar, int named)
{
    // the names the grammar's tokens take counted before the texts' lock is taken
    const unsigned int names = (grammar[0] != '\0') ? cli_highlight_names_each(grammar, branch_name_passed, NULL) : 0u;
    cli_document_lock_take();
    CliStoredText *const one = cli_document_stored(key);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = grammar, .cap = CLI_CELLS_POOL});
    if ((one != NULL) && (length < CLI_NAME_MAX_LEN))
    {
        memor.cpy(&(MemoriaCfg){.dst = one->grammar, .src = grammar, .bytes = length + 1ull});
        cli_document_serial_moved(one);
        one->names = names;
        one->state_used = 0u;
        one->state_count = 0u;
        if (one->state_indexed != 0u)
        {
            memor.set(&(MemoriaCfg){
                .dst = one->state_index, .val = 0, .bytes = one->state_indexed * sizeof(*one->state_index)});
        }
        one->refused = 0;
        one->named = named;
        (void)branch_state(one, "root");
        for (unsigned int line = 0u; line < one->lines.lines; line += 1u)
        {
            one->lines.begun[line] = CLI_DOC_UNCOLORED;
            one->lines.ended[line] = CLI_DOC_UNCOLORED;
            one->lines.regions[line] = 0u;
            one->lines.runs[line] = CLI_DOC_NO_RUNS;
        }
        one->run_used = 0u;
        // the roles kept were the last grammar's names', read again for this one as its lines are first claimed
        one->roles_whole = 0;
        one->regions_moved = 1;
        cli_document_woken(one);
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
    const int found = one != NULL;
    int any = 0;
    if ((one != NULL) && ((out != NULL) || made))
    {
        cli_document_holder_added(one);
        one->refused = one->refused || (one->paged && !cli_document_paged_counted(one));
        const CliLines *const lines = &one->lines;
        // a view past the text's last line is read at its last
        one->origin = (first < lines->lines) ? (unsigned int)first : ((lines->lines != 0u) ? lines->lines - 1u : 0u);
        if (out == NULL)
        {
            branch_colors_begun(one, into);
        }
        mmgr_line_ch(into, '[');
        // a paged text's view colored within the lines the page holds alone
        unsigned int low = 0u;
        unsigned int high = 0u;
        branch_bounds(one, &low, &high);
        CliBranchRange range = branch_range_begun(one, &any, into, one->columns, CLI_DOC_ALL_BYTES, high);
        for (unsigned long long line = (first > low) ? first : low;
             (one->grammar[0] != '\0') && !one->refused && !range.stale && (line <= last) && (line < high);)
        {
            // the view's lines stand below the text's last, which an unsigned int counts
            const unsigned int at = (unsigned int)line;
            // a line colored is passed over; one the walk claims is colored here as well, the view waiting on no
            // coloring of another
            if (lines->begun[at] != CLI_DOC_UNCOLORED)
            {
                line += 1ull;
                continue;
            }
            unsigned int run_first = 0u;
            unsigned int run_end = 0u;
            branch_run_of(one, at, 1, low, high, 0, &run_first, &run_end);
            // the view's lines alone, those below it left to the walk, which colors them on a thread of its own; a
            // line past the view's last stands below the run's end, which an unsigned int counts
            run_end = ((unsigned long long)run_end > (last + 1ull)) ? (unsigned int)(last + 1ull) : run_end;
            branch_range_colored(one, run_first, run_end, 0, &range);
            line = run_end;
        }
        branch_range_ended(&range);
        mmgr_line_ch(into, ']');
        if (out == NULL)
        {
            branch_colors_ended(one, any, into);
        }
        cli_document_woken(one);
        cli_document_holder_ended(one);
    }
    branch_handed_out(any && (out == NULL), &colors);
    if (made)
    {
        cli_cell_release(colors.out, NULL);
    }
    return found;
}

// the lines from `from` to before `to` of `one`, those from `passed_from` to before `passed_to` passed over, handed on
// again into `out` as the walk hands its colors, `{"key", "seq", "colors"}`: the runs each keeps where they stand, and
// each that keeps none that stand not colored, the walk or a view coloring it and handing it on as it comes to it, as
// every line is where `out` is NULL: 1 where any line's runs were written, else 0
static int branch_runs_rehanded(CliStoredText *one, unsigned int from, unsigned int to, unsigned int passed_from,
                                unsigned int passed_to, VerbaLinea *out)
{
    int any = 0;
    if (out != NULL)
    {
        branch_colors_begun(one, out);
        mmgr_line_ch(out, '[');
    }
    CliBranchRange range = branch_range_begun(one, &any, out, one->columns, CLI_DOC_ALL_BYTES, to);
    for (unsigned int line = from; line < to; line += 1u)
    {
        if ((line >= passed_from) && (line < passed_to))
        {
            continue;
        }
        if ((out != NULL) && branch_runs_standing(one, line))
        {
            branch_runs_handed(one, line, &range);
        }
        else
        {
            one->lines.begun[line] = CLI_DOC_UNCOLORED;
        }
    }
    if (out != NULL)
    {
        branch_range_ended(&range);
        mmgr_line_ch(out, ']');
        branch_colors_ended(one, any, out);
    }
    return any;
}

// colors handed on again written into `colors`, handed to where the colors go as the lock is given back, and its cell
// given back where `made` is 1
static void branch_rehanded_sent(int any, VerbaLinea *colors, int made)
{
    branch_handed_out(any, colors);
    if (made)
    {
        cli_cell_release(colors->out, NULL);
    }
}

int cli_document_held(const char *key, unsigned long long from, unsigned long long to)
{
    VerbaLinea colors;
    const int made = cli_growing_line_made(&colors, CLI_DOC_COLORS_FIRST);
    cli_document_lock_take();
    CliStoredText *const one = cli_document_stored(key);
    int any = 0;
    if (one != NULL)
    {
        one->refused = one->refused || (one->paged && !cli_document_paged_counted(one));
        CliLines *const lines = &one->lines;
        // the lines the page mirrors stand among those the pages count, which an unsigned int counts
        const unsigned int held_from = (from < lines->lines) ? (unsigned int)from : lines->lines;
        const unsigned int held_to = (to < lines->lines) ? (unsigned int)to : lines->lines;
        const unsigned int was_from = one->held_from;
        const unsigned int was_to = (one->held_to < lines->lines) ? one->held_to : lines->lines;
        // a paged text's lines the page lets go let their runs go with them
        for (unsigned int line = was_from; one->paged && (line < was_to); line += 1u)
        {
            lines->runs[line] = ((line < held_from) || (line >= held_to)) ? CLI_DOC_NO_RUNS : lines->runs[line];
        }
        // a line the page comes to mirror handed the runs it keeps, or colored again where it keeps none that stand,
        // the state it ends in kept: the line below it is entered in the state it was
        any = branch_runs_rehanded(one, held_from, held_to, was_from, was_to, made ? &colors : NULL);
        // a paged text's folds are those of the lines the page mirrors
        one->regions_moved =
            one->regions_moved || (one->paged && ((held_from != one->held_from) || (held_to != one->held_to)));
        one->held_from = held_from;
        one->held_to = held_to;
        cli_document_woken(one);
    }
    branch_rehanded_sent(any, &colors, made);
    return one != NULL;
}

int cli_document_columns(const char *key, unsigned long long columns)
{
    VerbaLinea colors;
    const int made = cli_growing_line_made(&colors, CLI_DOC_COLORS_FIRST);
    cli_document_lock_take();
    CliStoredText *const one = cli_document_stored(key);
    int any = 0;
    if (one != NULL)
    {
        // a view drawing farther than the lines' runs were handed has the lines the page mirrors handed again as far;
        // one drawing less leaves them as they are
        const int farther = (one->columns != 0ull) && ((columns == 0ull) || (columns > one->columns));
        one->columns = (farther || (one->columns == 0ull)) ? columns : one->columns;
        if (farther)
        {
            const unsigned int held_to = (one->held_to < one->lines.lines) ? one->held_to : one->lines.lines;
            any = branch_runs_rehanded(one, one->held_from, held_to, 0u, 0u, made ? &colors : NULL);
            cli_document_woken(one);
        }
    }
    branch_rehanded_sent(any, &colors, made);
    return one != NULL;
}

int cli_document_runs_written(const char *key, unsigned long long first, unsigned long long last, int mirrored,
                              VerbaLinea *out)
{
    cli_document_lock_take();
    CliStoredText *const one = cli_document_stored(key);
    const int found = one != NULL;
    int any = 0;
    mmgr_line_ch(out, '[');
    if (one != NULL)
    {
        cli_document_holder_added(one);
        one->refused = one->refused || (one->paged && !cli_document_paged_counted(one));
        const CliLines *const lines = &one->lines;
        // the lines asked stand among those the text counts, which an unsigned int counts
        const unsigned int from = (first < lines->lines) ? (unsigned int)first : lines->lines;
        const unsigned int to = (last < lines->lines) ? ((unsigned int)last + 1u) : lines->lines;
        // each line's runs the walk kept where they stand, and the lines that keep none colored, none past the last
        // asked, their runs written whatever lines the page mirrors
        const unsigned long long columns = mirrored ? one->columns : 0ull;
        CliBranchRange range = branch_range_begun(one, &any, out, columns, CLI_DOC_ALL_BYTES, to);
        range.asked = 1;
        range.hand_from = from;
        range.hand_to = to;
        for (unsigned int line = from; (one->grammar[0] != '\0') && !one->refused && !range.stale && (line < to);)
        {
            if (branch_runs_standing(one, line))
            {
                branch_runs_handed(one, line, &range);
                line += 1u;
                continue;
            }
            unsigned int end = line + 1u;
            while ((end < to) && !branch_runs_standing(one, end))
            {
                end += 1u;
            }
            branch_range_colored(one, line, end, 0, &range);
            // the lines after them colored again with them written with them
            line = (range.next > end) ? range.next : end;
        }
        branch_range_ended(&range);
        // the lines the page mirrors take the lines asked in, joined to them where the two meet and in their place where
        // they do not
        if (mirrored)
        {
            const unsigned int held_from = one->held_from;
            const unsigned int held_to = one->held_to;
            const int meet = (from <= held_to) && (to >= held_from) && (held_from < held_to);
            one->held_from = (meet && (held_from < from)) ? held_from : from;
            one->held_to = (meet && (held_to > to)) ? held_to : to;
        }
        cli_document_holder_ended(one);
    }
    mmgr_line_ch(out, ']');
    cli_document_lock_give();
    return found;
}

void cli_document_walked(void)
{
    branch_walked(&s_walkers[CLI_DOC_WALKERS], 0);
}

unsigned int cli_document_walkers(void)
{
    cli_document_lock_take();
    unsigned int walking = 0u;
    for (unsigned int at = 0u; at < CLI_DOC_WALKERS; at += 1u)
    {
        walking += s_walkers[at].walking ? 1u : 0u;
    }
    cli_document_lock_give();
    return walking;
}

char *cli_document_text_taken(const char *key, unsigned long long *size)
{
    cli_document_lock_take();
    const CliStoredText *const one = cli_document_stored(key);
    char *taken = NULL;
    if ((one != NULL) && !one->paged)
    {
        const CliLines *const lines = &one->lines;
        // each unit written as three bytes at the most, each line's newline, and the NUL after the last
        unsigned long long room = 1ull;
        for (unsigned int line = 0u; line < lines->lines; line += 1u)
        {
            room += (3ull * lines->length[line]) + 1ull;
        }
        taken = cli_lane_alloc(room, NULL);
        if (taken != NULL)
        {
            VerbaLinea text = {taken, room, 0ull};
            for (unsigned int line = 0u; line < lines->lines; line += 1u)
            {
                branch_line_put(lines, line, &text);
                mmgr_line_put(&text, ((line + 1u) < lines->lines) ? "\n" : "");
            }
            taken[text.at] = '\0';
            *size = text.at;
        }
    }
    cli_document_lock_give();
    return taken;
}

char *cli_document_lines_taken(const char *key, unsigned long long first, unsigned long long last,
                               unsigned long long *size, char *state, unsigned long long room)
{
    VerbaLinea written;
    const int made = cli_growing_line_made(&written, CLI_DOC_COLORS_FIRST);
    VerbaLinea *const text = &written;
    cli_document_lock_take();
    const CliStoredText *const one = cli_document_stored(key);
    const int found = one != NULL;
    VerbaLinea entered = {state, room, 0ull};
    // a paged text's lines, read through the pages with the lock given back: its key, and the lines asked
    char paged[CLI_NAME_MAX_LEN];
    paged[0] = '\0';
    unsigned int from = 0u;
    unsigned int to = 0u;
    if (made && (one != NULL))
    {
        const CliLines *const lines = &one->lines;
        // the lines asked stand below the text's last, which an unsigned int counts
        to = (last < lines->lines) ? (unsigned int)last : (lines->lines - 1u);
        from = (first <= to) ? (unsigned int)first : to;
        const unsigned int begun = (lines->lines != 0u) ? lines->begun[from] : CLI_DOC_UNCOLORED;
        // the colorer's own state the first is entered in, the scope after the mark left out
        const char *const begun_in =
            ((begun != CLI_DOC_UNCOLORED) && (one->state != NULL)) ? (one->state + begun) : "root";
        const char *const mark =
            cellul.chr(&(CatenaFinitaCfg){.src = begun_in, .cap = CLI_CELLS_POOL, .byte = CLI_DOC_SCOPE_MARK});
        mmgr_line_put_n(&entered, begun_in,
                        (mark != NULL) ? (unsigned long long)(mark - begun_in)
                                       : cellul.len(&(CatenaFinitaCfg){.src = begun_in, .cap = CLI_CELLS_POOL}));
        if (one->paged && (lines->lines != 0u))
        {
            memor.cpy(&(MemoriaCfg){.dst = paged, .src = one->key, .bytes = sizeof(paged)});
        }
        for (unsigned int at = from; !one->paged && (lines->lines != 0u) && (at <= to); at += 1u)
        {
            branch_line_put(lines, at, text);
            mmgr_line_put(text, (at < to) ? "\n" : "");
        }
    }
    cli_document_lock_give();
    if (paged[0] != '\0')
    {
        CliBranchWork work;
        branch_work_emptied(&work);
        unsigned int read = from;
        char *taken = NULL;
        const char **const line = branch_lines_paged(paged, from, to + 1u, &work, &read, &taken);
        for (unsigned int at = from; (line != NULL) && (at < read); at += 1u)
        {
            mmgr_line_put(text, line[at - from]);
            mmgr_line_put(text, (at < to) ? "\n" : "");
        }
        cli_cell_release(taken, NULL);
        branch_work_given(&work);
    }
    if ((mmgr_line_finish(&entered) == 0ull) && (room != 0ull))
    {
        state[0] = '\0';
    }
    const int taken = made && found && mmgr_line_ok(text);
    if (made && !taken)
    {
        cli_cell_release(written.out, NULL);
    }
    *size = taken ? text->at : 0ull;
    return taken ? written.out : NULL;
}

int cli_document_bounds(const char *key, unsigned long long *low, unsigned long long *high)
{
    cli_document_lock_take();
    const CliStoredText *const one = cli_document_stored(key);
    unsigned int from = 0u;
    unsigned int to = 0u;
    if (one != NULL)
    {
        branch_bounds(one, &from, &to);
    }
    cli_document_lock_give();
    *low = from;
    *high = to;
    return one != NULL;
}

int cli_document_text_written(const char *key, unsigned long long first, unsigned long long last, VerbaLinea *out,
                              char *state, unsigned long long room)
{
    unsigned long long size = 0ull;
    char *const text = cli_document_lines_taken(key, first, last, &size, state, room);
    if (text != NULL)
    {
        cli_json_quoted(out, text, size);
        cli_cell_release(text, NULL);
    }
    return text != NULL;
}
