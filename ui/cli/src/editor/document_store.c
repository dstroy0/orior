// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_store.c: the branches here, by key: stored whole as a branch is read, written by the page's edits, marked
// against the last commit's file, and searched. Each text's lines live in a cell sized to the text with room to grow,
// moved to a larger cell as an edit outgrows it; and the last commit's file in one sized to it. A paged text is a
// branch too, its lines in the pages and only their states here, carried through each edit the pages write
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "document_internal.h"
#include "pages.h"

// the most units and lines of a text stored
#define CLI_DOC_UNITS (1u << 22u)
#define CLI_DOC_LINES (1u << 18u)

// the words each line keeps beside its text: the states it is colored from and ends in, its regions and its runs' place
#define CLI_DOC_LINE_WORDS 4ull

// the least units and lines a text's room is given past what it contains, and the share of what it contains added as
// well, the room an edit fits in where it stands
#define CLI_DOC_SPARE_UNITS 1024ull
#define CLI_DOC_SPARE_LINES 64u
#define CLI_DOC_SPARE_SHARE 8ull

// the first of the texts stored, the lock they are read and written under, and the last serial a text took
static CliStoredText *s_stored;
static CliLock s_lock = CLI_LOCK_INIT;
static unsigned long long s_serial;

void cli_document_lock_take(void)
{
    cli_lock_take(&s_lock);
}

void cli_document_lock_give(void)
{
    cli_lock_give(&s_lock);
}

CliStoredText *cli_document_texts(void)
{
    return s_stored;
}

void cli_document_serial_moved(CliStoredText *one)
{
    s_serial += 1ull;
    one->serial = s_serial;
}

void cli_document_holder_added(CliStoredText *one)
{
    one->readers += 1u;
}

// how many lines `size` bytes of `text` make, one more than its newlines
static unsigned int stored_lines_in(const char *text, unsigned long long size)
{
    unsigned long long count = 1ull;
    for (unsigned long long at = 0ull; at < size; count += 1ull)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        if (newline == NULL)
        {
            break;
        }
        // the newline lies in the text, at or past `at`
        at = (unsigned long long)(newline - text) + 1ull;
    }
    // the lines are bounded by CLI_DOC_LINES, which an unsigned int stores
    return (unsigned int)((count < CLI_DOC_LINES) ? count : CLI_DOC_LINES);
}

// `lines` given a room of `units_most` units and `lines_most` lines in a cell of its own, each bounded by the most a
// text may have: 1, or 0 with `lines` empty where the cells cannot meet it
static int stored_room_enter(CliLines *lines, unsigned long long units_most, unsigned int lines_most, const char *what)
{
    const unsigned long long units = (units_most < CLI_DOC_UNITS) ? units_most : CLI_DOC_UNITS;
    const unsigned int count = (lines_most < CLI_DOC_LINES) ? lines_most : CLI_DOC_LINES;
    // the units' bytes rounded up to a boundary of the starts after them
    const unsigned long long unit_bytes = ((units * sizeof(unsigned short)) + 7ull) & ~7ull;
    // each line's start and length, then the two states it is colored from and ends in, its regions and its runs' place
    const unsigned long long bytes = unit_bytes + (2ull * (unsigned long long)count * sizeof(unsigned long long)) +
                                     (CLI_DOC_LINE_WORDS * (unsigned long long)count * sizeof(unsigned int));
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_editor_cells);
    *lines = (CliLines){NULL, 0ull, 0ull, NULL, NULL, 0u, 0u, 0};
    if (cell == NULL)
    {
        return 0;
    }
    unsigned long long *const start = (unsigned long long *)(void *)(cell + unit_bytes);
    unsigned int *const begun = (unsigned int *)(void *)(start + (2ull * count));
    *lines = (CliLines){(unsigned short *)(void *)cell, units, 0ull, start, start + count, count, 0u, 0};
    lines->begun = begun;
    lines->ended = begun + count;
    lines->regions = begun + (2ull * count);
    lines->runs = begun + (3ull * count);
    return 1;
}

// a room given back, its cell starting at its units or, for a paged text's, at its states, `lines` left empty
static void stored_room_exit(CliLines *lines)
{
    cli_cell_release((lines->unit != NULL) ? (void *)lines->unit : (void *)lines->begun, &g_cli_editor_cells);
    *lines = (CliLines){NULL, 0ull, 0ull, NULL, NULL, 0u, 0u, 0};
}

// a paged text's room grown to hold the states of `count` lines with room to grow past them, its states kept, `lines`
// counting `count`, each line it did not count not colored: 1, or 0 where the cells cannot meet it, the room as it was
static int stored_states_grown(CliLines *lines, unsigned long long count)
{
    if (count > 0xffffffffull)
    {
        return 0;
    }
    if (count > lines->lines_most)
    {
        const unsigned long long room = count + (count / CLI_DOC_SPARE_SHARE) + CLI_DOC_SPARE_LINES;
        // the lines a room holds are counted by an unsigned int
        const unsigned int most = (room < 0xffffffffull) ? (unsigned int)room : 0xffffffffu;
        unsigned int *const begun =
            g_cli_cells->persistent_buf_alloc(CLI_DOC_LINE_WORDS * most * sizeof(unsigned int), &g_cli_editor_cells);
        if (begun == NULL)
        {
            return 0;
        }
        if (lines->lines != 0u)
        {
            memor.cpy(&(MemoriaCfg){.dst = begun, .src = lines->begun, .bytes = lines->lines * sizeof(unsigned int)});
            memor.cpy(
                &(MemoriaCfg){.dst = begun + most, .src = lines->ended, .bytes = lines->lines * sizeof(unsigned int)});
            memor.cpy(&(MemoriaCfg){
                .dst = begun + (2ull * most), .src = lines->regions, .bytes = lines->lines * sizeof(unsigned int)});
            memor.cpy(&(MemoriaCfg){
                .dst = begun + (3ull * most), .src = lines->runs, .bytes = lines->lines * sizeof(unsigned int)});
        }
        const unsigned int counted = lines->lines;
        stored_room_exit(lines);
        lines->begun = begun;
        lines->ended = begun + most;
        lines->regions = begun + (2ull * most);
        lines->runs = begun + (3ull * most);
        lines->lines_most = most;
        lines->lines = counted;
    }
    for (unsigned int line = lines->lines; line < count; line += 1u)
    {
        lines->begun[line] = CLI_DOC_UNCOLORED;
        lines->ended[line] = CLI_DOC_UNCOLORED;
        lines->regions[line] = 0u;
        lines->runs[line] = CLI_DOC_NO_RUNS;
    }
    // the count is no more than the room, which an unsigned int counts
    lines->lines = (count > lines->lines) ? (unsigned int)count : lines->lines;
    return 1;
}

int cli_document_paged_counted(CliStoredText *one)
{
    unsigned long long count = 0ull;
    int whole = 0;
    if (!cli_pages_counted(one->key, &count, &whole))
    {
        return 1;
    }
    one->whole = whole;
    return (count <= one->lines.lines) || stored_states_grown(&one->lines, count);
}

// the room `size` bytes of `text` take with room to grow, taken for `lines` in place of what it contained: 1, or 0
// where the cells cannot meet it
static int stored_room_for(CliLines *lines, const char *text, unsigned long long size, const char *what)
{
    stored_room_exit(lines);
    const unsigned long long units = size + (size / CLI_DOC_SPARE_SHARE) + CLI_DOC_SPARE_UNITS;
    const unsigned int count = stored_lines_in(text, size);
    return stored_room_enter(lines, units, count + (count / CLI_DOC_SPARE_SHARE) + CLI_DOC_SPARE_LINES, what);
}

CliStoredText *cli_document_stored(const char *key)
{
    for (CliStoredText *one = s_stored; one != NULL; one = one->next)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            return one;
        }
    }
    return NULL;
}

// the text stored under `key`, made afresh in a cell of its own where none is: NULL where the cells cannot meet it
static CliStoredText *cli_stored_made(const char *key)
{
    CliStoredText *const there = cli_document_stored(key);
    if (there != NULL)
    {
        return there;
    }
    CliStoredText *const one = g_cli_cells->persistent_buf_alloc(sizeof(*one), &g_cli_editor_cells);
    if (one == NULL)
    {
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    memor.cpy(&(MemoriaCfg){
        .dst = one->key, .src = key, .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    one->next = s_stored;
    s_stored = one;
    return one;
}

// a text's cells given back, the text's own last
static void stored_cells_given(CliStoredText *one)
{
    stored_room_exit(&one->lines);
    stored_room_exit(&one->head);
    cli_cell_release(one->state, &g_cli_editor_cells);
    cli_cell_release(one->state_index, &g_cli_editor_cells);
    cli_cell_release(one->roles, &g_cli_editor_cells);
    cli_cell_release(one->run, &g_cli_editor_cells);
    cli_cell_release(one, &g_cli_editor_cells);
}

// a text stored let go: taken off the texts stored, and its cells given back, at once where nothing holds it, else as
// the last to hold it ends
static void cli_stored_let_go(CliStoredText *one)
{
    CliStoredText **link = &s_stored;
    while ((*link != NULL) && (*link != one))
    {
        link = &(*link)->next;
    }
    if (*link == one)
    {
        *link = one->next;
    }
    one->let_go = 1;
    if (one->readers == 0u)
    {
        stored_cells_given(one);
    }
}

void cli_document_holder_ended(CliStoredText *one)
{
    one->readers -= (one->readers != 0u) ? 1u : 0u;
    if (one->let_go && (one->readers == 0u))
    {
        stored_cells_given(one);
    }
}

// `size` bytes of `text` made the lines of the text stored, in a room sized to them: 1, or 0 where the cells cannot
// meet it, the text let go
static int stored_lines_made(CliStoredText *one, const char *text, unsigned long long size, CliShape *shape)
{
    if (!stored_room_for(&one->lines, text, size, "a text stored's lines"))
    {
        cli_stored_let_go(one);
        return 0;
    }
    cli_document_lines_of(text, size, &one->lines);
    cli_document_serial_moved(one);
    one->head_known = 0;
    one->changes = 0ull;
    one->paged = 0;
    one->whole = 1;
    one->regions_moved = 1;
    one->held_from = 0u;
    one->held_to = CLI_DOC_HELD_ALL;
    *shape = cli_document_shape_of(&one->lines);
    return 1;
}

// the text stored under `key` where it is read whole, NULL where none is or it is a paged text's branch
static CliStoredText *stored_whole_of(const char *key)
{
    CliStoredText *const one = cli_document_stored(key);
    return ((one != NULL) && !one->paged) ? one : NULL;
}

int cli_document_paged(const char *key, unsigned long long from, unsigned long long to)
{
    unsigned long long count = 0ull;
    int whole = 0;
    if (!cli_pages_counted(key, &count, &whole))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    CliStoredText *const one = cli_stored_made(key);
    if (one != NULL)
    {
        stored_room_exit(&one->lines);
        cli_document_serial_moved(one);
        one->paged = 1;
        one->whole = whole;
        one->changes = 0ull;
        one->head_known = 0;
        one->regions_moved = 1;
        // a line the page holds is one the pages count, which an unsigned int counts
        one->held_from = (from < CLI_DOC_HELD_ALL) ? (unsigned int)from : CLI_DOC_HELD_ALL;
        one->held_to = (to < CLI_DOC_HELD_ALL) ? (unsigned int)to : CLI_DOC_HELD_ALL;
    }
    const int held = (one != NULL) && stored_states_grown(&one->lines, count);
    if ((one != NULL) && !held)
    {
        cli_stored_let_go(one);
    }
    cli_lock_give(&s_lock);
    return held;
}

// the states of a paged text's lines carried through one edit the pages wrote: the lines from its first to its last
// taken out and those its text writes put in their place, not colored, and the lines after moved with them: 1, or 0
// where the cells cannot meet the lines it adds
static int stored_states_edited(CliLines *lines, const CliEdit *edit)
{
    unsigned long long written = 1ull;
    for (const char *at = memor.chr(&(MemoriaCfg){.src = edit->text, .val = '\n', .bytes = edit->length}); at != NULL;
         at = memor.chr(&(MemoriaCfg){
             .src = at + 1, .val = '\n', .bytes = edit->length - (unsigned long long)(at + 1 - edit->text)}))
    {
        written += 1ull;
    }
    const unsigned long long first = edit->from.line;
    const unsigned long long after = edit->to.line + 1ull;
    if ((first > edit->to.line) || (after > lines->lines))
    {
        return 1;
    }
    const unsigned long long count = lines->lines - (after - first) + written;
    const unsigned long long moved = lines->lines - after;
    if ((count > lines->lines) && !stored_states_grown(lines, count))
    {
        return 0;
    }
    const unsigned long long to = first + written;
    if (to <= after)
    {
        memor.move_down(&(MemoriaCfg){
            .dst = lines->begun + to, .src = lines->begun + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_down(&(MemoriaCfg){
            .dst = lines->ended + to, .src = lines->ended + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_down(&(MemoriaCfg){
            .dst = lines->regions + to, .src = lines->regions + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_down(&(MemoriaCfg){
            .dst = lines->runs + to, .src = lines->runs + after, .bytes = moved * sizeof(unsigned int)});
    }
    else
    {
        memor.move_up(&(MemoriaCfg){
            .dst = lines->begun + to, .src = lines->begun + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_up(&(MemoriaCfg){
            .dst = lines->ended + to, .src = lines->ended + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_up(&(MemoriaCfg){
            .dst = lines->regions + to, .src = lines->regions + after, .bytes = moved * sizeof(unsigned int)});
        memor.move_up(&(MemoriaCfg){
            .dst = lines->runs + to, .src = lines->runs + after, .bytes = moved * sizeof(unsigned int)});
    }
    for (unsigned long long line = first; line < to; line += 1ull)
    {
        lines->begun[line] = CLI_DOC_UNCOLORED;
        lines->ended[line] = CLI_DOC_UNCOLORED;
        lines->regions[line] = 0u;
        lines->runs[line] = CLI_DOC_NO_RUNS;
    }
    // the count is no more than the room, which an unsigned int counts
    lines->lines = (unsigned int)count;
    return 1;
}

int cli_document_paged_edit(const char *key, const CliEdit *const *changes, const unsigned int *edits,
                            unsigned int count, VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    CliStoredText *const one = cli_document_stored(key);
    CliStoredText *const branch = ((one != NULL) && one->paged) ? one : NULL;
    // the states counted as far as the pages are before the edits, which are placed against those lines
    int carried = (branch == NULL) || cli_document_paged_counted(branch);
    const int written = cli_pages_edit(key, changes, edits, count, out, err_msg);
    for (unsigned int change = 0u; written && carried && (branch != NULL) && (change < count); change += 1u)
    {
        for (unsigned int at = edits[change]; carried && (at > 0u); at -= 1u)
        {
            carried = stored_states_edited(&branch->lines, &changes[change][at - 1u]);
        }
    }
    if (written && (branch != NULL))
    {
        cli_document_serial_moved(branch);
        branch->changes += count;
        branch->regions_moved = 1;
        branch->refused = branch->refused || !carried;
        cli_document_woken(branch);
    }
    cli_lock_give(&s_lock);
    return written;
}

int cli_document_store(const char *key, const char *text, unsigned long long size, CliShape *shape)
{
    cli_lock_take(&s_lock);
    CliStoredText *const one = cli_stored_made(key);
    const int stored_ok = (one != NULL) && stored_lines_made(one, text, size, shape);
    cli_lock_give(&s_lock);
    return stored_ok;
}

void cli_document_release(const char *key)
{
    cli_lock_take(&s_lock);
    CliStoredText *const one = cli_document_stored(key);
    if (one != NULL)
    {
        cli_stored_let_go(one);
    }
    cli_lock_give(&s_lock);
}

int cli_document_shape(const char *key, CliShape *shape)
{
    cli_lock_take(&s_lock);
    const CliStoredText *const one = stored_whole_of(key);
    if (one != NULL)
    {
        *shape = cli_document_shape_of(&one->lines);
    }
    cli_lock_give(&s_lock);
    return one != NULL;
}

// a line's units put after the spare lines, the text of the line being made running on from `open` where it is given
static void cli_units_put(CliLines *lines, const unsigned short *units, unsigned long long count)
{
    if ((lines->used + count) > lines->units_most)
    {
        lines->full = 1;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = lines->unit + lines->used, .src = units, .bytes = count * sizeof(unsigned short)});
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        cli_lines_census_put(lines, units[at]);
    }
    lines->used += count;
}

// a line not colored begun after the spare lines; the line begun last given the states the line `line` of `from` was
// colored from and ends in, an edit's line carried whole; and the line being made ended
static void cli_line_begun(CliLines *lines)
{
    if (lines->lines >= lines->lines_most)
    {
        lines->full = 1;
        return;
    }
    lines->start[lines->lines] = lines->used;
    lines->length[lines->lines] = 0ull;
    lines->begun[lines->lines] = CLI_DOC_UNCOLORED;
    lines->ended[lines->lines] = CLI_DOC_UNCOLORED;
    lines->regions[lines->lines] = 0u;
    lines->runs[lines->lines] = CLI_DOC_NO_RUNS;
    lines->lines += 1u;
}

static void cli_line_carried(CliLines *lines, const CliLines *from, unsigned long long line)
{
    if (lines->lines > 0u)
    {
        lines->begun[lines->lines - 1u] = from->begun[line];
        lines->ended[lines->lines - 1u] = from->ended[line];
        lines->regions[lines->lines - 1u] = from->regions[line];
        lines->runs[lines->lines - 1u] = from->runs[line];
    }
}

static void cli_line_ended(CliLines *lines)
{
    if (lines->lines > 0u)
    {
        lines->length[lines->lines - 1u] = lines->used - lines->start[lines->lines - 1u];
    }
}

// `count` of a text's units moved from `from` to `to`, the way the move needs where the two overlap
static void cli_units_moved(unsigned short *unit, unsigned long long from, unsigned long long to,
                            unsigned long long count)
{
    const MemoriaCfg moving = {.dst = unit + to, .src = unit + from, .bytes = count * sizeof(unsigned short)};
    if (to < from)
    {
        memor.move_down(&moving);
    }
    else if (to > from)
    {
        memor.move_up(&moving);
    }
}

// the starts or the lengths of `count` lines moved from the line `from` to the line `to`, the way the move needs where
// the two overlap; and so their states, their regions or the places of their runs
static void cli_line_places_moved(unsigned long long *place, unsigned long long from, unsigned long long to,
                                  unsigned long long count)
{
    const MemoriaCfg moving = {.dst = place + to, .src = place + from, .bytes = count * sizeof(unsigned long long)};
    if (to < from)
    {
        memor.move_down(&moving);
    }
    else if (to > from)
    {
        memor.move_up(&moving);
    }
}

static void cli_line_states_moved(unsigned int *state, unsigned long long from, unsigned long long to,
                                  unsigned long long count)
{
    const MemoriaCfg moving = {.dst = state + to, .src = state + from, .bytes = count * sizeof(unsigned int)};
    if (to < from)
    {
        memor.move_down(&moving);
    }
    else if (to > from)
    {
        memor.move_up(&moving);
    }
}

// one edit written over the text's lines in the room they stand in, where the result fits it: the units after the
// edit moved to where its text ends and its text put between, split at each "\n", each line it writes not colored and
// each line after it carried with its states, the census given the units the edit takes out and puts in: 1, or 0 with
// the text as it was where the result does not fit the room
static int cli_edit_placed(CliLines *now, const CliEdit *edit)
{
    // the units and the line breaks the edit's text puts in, counted on a first pass over it
    unsigned long long units = 0ull;
    unsigned long long breaks = 0ull;
    for (unsigned long long at = 0ull; at < edit->length;)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)edit->text, edit->length, at, &letter);
        breaks += (letter == '\n') ? 1ull : 0ull;
        units += (letter == '\n') ? 0ull : ((letter > 0xffffu) ? 2ull : 1ull);
    }
    const unsigned long long from = now->start[edit->from.line] + edit->from.col;
    const unsigned long long to = now->start[edit->to.line] + edit->to.col;
    const unsigned long long used = now->used - (to - from) + units;
    const unsigned long long taken_lines = edit->to.line - edit->from.line;
    const unsigned long long lines = (unsigned long long)now->lines - taken_lines + breaks;
    if (now->full || (used > now->units_most) || (lines > now->lines_most))
    {
        return 0;
    }
    for (unsigned long long at = from; at < to; at += 1ull)
    {
        now->census.occurrences[now->unit[at] & 0xffu] -= 1ull;
        now->census.occurrences[now->unit[at] >> 8u] -= 1ull;
        now->census.total -= 2ull;
    }
    const unsigned long long tail = now->length[edit->to.line] - edit->to.col;
    cli_units_moved(now->unit, to, from + units, now->used - to);
    // the lines after the edit moved to stand after the lines it writes, each starting as many units on
    const unsigned long long after = edit->to.line + 1ull;
    const unsigned long long after_to = edit->from.line + breaks + 1ull;
    const unsigned long long after_count = now->lines - after;
    cli_line_places_moved(now->start, after, after_to, after_count);
    cli_line_places_moved(now->length, after, after_to, after_count);
    cli_line_states_moved(now->begun, after, after_to, after_count);
    cli_line_states_moved(now->ended, after, after_to, after_count);
    cli_line_states_moved(now->regions, after, after_to, after_count);
    cli_line_states_moved(now->runs, after, after_to, after_count);
    for (unsigned long long line = after_to; line < lines; line += 1ull)
    {
        now->start[line] = now->start[line] + (from + units) - to;
    }
    // the edit's text put in, each line it writes begun where the one before it ends and not colored
    unsigned long long line = edit->from.line;
    unsigned long long put = from;
    for (unsigned long long at = 0ull; at <= edit->length;)
    {
        unsigned int letter = '\n';
        if (at < edit->length)
        {
            at += cli_lossy_letter((const unsigned char *)edit->text, edit->length, at, &letter);
        }
        else
        {
            at += 1ull;
        }
        if (letter == '\n')
        {
            const unsigned long long ends = (at > edit->length) ? (put + tail) : put;
            now->length[line] = ends - now->start[line];
            now->begun[line] = CLI_DOC_UNCOLORED;
            now->ended[line] = CLI_DOC_UNCOLORED;
            now->regions[line] = 0u;
            now->runs[line] = CLI_DOC_NO_RUNS;
            line += 1ull;
            if (at <= edit->length)
            {
                now->start[line] = put;
            }
            continue;
        }
        // each unit of a letter is below 2^16
        const unsigned short first =
            (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
        now->unit[put] = first;
        cli_lines_census_put(now, first);
        put += 1ull;
        if (letter > 0xffffu)
        {
            const unsigned short second = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
            now->unit[put] = second;
            cli_lines_census_put(now, second);
            put += 1ull;
        }
    }
    now->used = used;
    // the lines counted above to be no more than the room's, which an unsigned int counts
    now->lines = (unsigned int)lines;
    return 1;
}

// one edit written over the text's lines through `spare`, a room that contains the result: the lines before it, its
// text split at each "\n" with the head of its first line before it and the tail of its last after it, and the lines
// after it, each line it leaves whole with its states and each it writes not colored. The result goes back into the
// text's room where it fits, else `spare` becomes the text's room and the old one is given back
static void cli_edit_written(CliStoredText *one, const CliEdit *edit, CliLines *spare)
{
    CliLines *const now = &one->lines;
    for (unsigned long long line = 0ull; line < edit->from.line; line += 1ull)
    {
        cli_line_begun(spare);
        cli_line_carried(spare, now, line);
        cli_units_put(spare, now->unit + now->start[line], now->length[line]);
        cli_line_ended(spare);
    }
    cli_line_begun(spare);
    cli_units_put(spare, now->unit + now->start[edit->from.line], edit->from.col);
    for (unsigned long long at = 0ull; at < edit->length;)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)edit->text, edit->length, at, &letter);
        if (letter == '\n')
        {
            cli_line_ended(spare);
            cli_line_begun(spare);
            continue;
        }
        unsigned short units[2];
        const unsigned long long count = (letter > 0xffffu) ? 2ull : 1ull;
        // each unit of a letter is below 2^16
        units[0] = (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
        units[1] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
        cli_units_put(spare, units, count);
    }
    const unsigned long long tail_line = edit->to.line;
    cli_units_put(spare, now->unit + now->start[tail_line] + edit->to.col, now->length[tail_line] - edit->to.col);
    cli_line_ended(spare);
    for (unsigned long long line = tail_line + 1ull; line < now->lines; line += 1ull)
    {
        cli_line_begun(spare);
        cli_line_carried(spare, now, line);
        cli_units_put(spare, now->unit + now->start[line], now->length[line]);
        cli_line_ended(spare);
    }
    const int full = now->full || spare->full;
    if ((spare->used <= now->units_most) && (spare->lines <= now->lines_most))
    {
        memor.cpy(&(MemoriaCfg){.dst = now->unit, .src = spare->unit, .bytes = spare->used * sizeof(unsigned short)});
        memor.cpy(
            &(MemoriaCfg){.dst = now->start, .src = spare->start, .bytes = spare->lines * sizeof(unsigned long long)});
        memor.cpy(&(MemoriaCfg){
            .dst = now->length, .src = spare->length, .bytes = spare->lines * sizeof(unsigned long long)});
        memor.cpy(&(MemoriaCfg){.dst = now->begun, .src = spare->begun, .bytes = spare->lines * sizeof(unsigned int)});
        memor.cpy(&(MemoriaCfg){.dst = now->ended, .src = spare->ended, .bytes = spare->lines * sizeof(unsigned int)});
        memor.cpy(
            &(MemoriaCfg){.dst = now->regions, .src = spare->regions, .bytes = spare->lines * sizeof(unsigned int)});
        memor.cpy(&(MemoriaCfg){.dst = now->runs, .src = spare->runs, .bytes = spare->lines * sizeof(unsigned int)});
        now->used = spare->used;
        now->lines = spare->lines;
        now->census = spare->census;
        stored_room_exit(spare);
    }
    else
    {
        stored_room_exit(now);
        *now = *spare;
        *spare = (CliLines){NULL, 0ull, 0ull, NULL, NULL, 0u, 0u, 0};
    }
    now->full = full;
}

// whether a place stands in the text: on a line it contains, at or before the line's end
static int cli_fits(const CliLines *lines, const CliPlace *place)
{
    return (place->line < lines->lines) && (place->col <= lines->length[place->line]);
}

// a room that contains the text as `edit` leaves it, with room to grow, taken for `spare`: 1, or 0 where the cells
// cannot meet it
static int stored_spare_taken(const CliLines *now, const CliEdit *edit, CliLines *spare)
{
    // a unit of the edit's text takes a byte at the least: its bytes bound the units it puts in
    const unsigned long long units = now->used + edit->length;
    const unsigned long long lines = (unsigned long long)now->lines + stored_lines_in(edit->text, edit->length);
    const unsigned long long most = (lines < CLI_DOC_LINES) ? lines : CLI_DOC_LINES;
    return stored_room_enter(spare, units + (units / CLI_DOC_SPARE_SHARE) + CLI_DOC_SPARE_UNITS,
                             (unsigned int)(most + (most / CLI_DOC_SPARE_SHARE) + CLI_DOC_SPARE_LINES),
                             "the lines an edit writes");
}

int cli_document_edit(const char *key, const CliEdit *const *changes, const unsigned int *edits, unsigned int count,
                      CliShape *shape, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    CliStoredText *const one = stored_whole_of(key);
    if (one == NULL)
    {
        mmgr_line_put(err_msg, "no text is stored under ");
        mmgr_line_put(err_msg, key);
        cli_lock_give(&s_lock);
        return 0;
    }
    int fits = 1;
    int room = 1;
    for (unsigned int change = 0u; fits && room && (change < count); change += 1u)
    {
        for (unsigned int at = edits[change]; fits && room && (at > 0u); at -= 1u)
        {
            const CliEdit *const edit = &changes[change][at - 1u];
            const int before = (edit->to.line < edit->from.line) ||
                               ((edit->to.line == edit->from.line) && (edit->to.col < edit->from.col));
            fits = cli_fits(&one->lines, &edit->from) && cli_fits(&one->lines, &edit->to) && !before;
            // an edit is written in the text's own room, and a room of its own is taken only where the result
            // outgrows it
            if (fits && !cli_edit_placed(&one->lines, edit))
            {
                CliLines spare;
                room = stored_spare_taken(&one->lines, edit, &spare);
                if (room)
                {
                    cli_edit_written(one, edit, &spare);
                }
            }
        }
    }
    if (!fits || !room)
    {
        cli_stored_let_go(one);
        mmgr_line_put(err_msg, fits ? "the lines an edit writes could not be taken for "
                                    : "an edit does not fit the text stored under ");
        mmgr_line_put(err_msg, key);
        cli_lock_give(&s_lock);
        return 0;
    }
    *shape = cli_document_shape_of(&one->lines);
    cli_document_serial_moved(one);
    one->changes += count;
    one->regions_moved = 1;
    cli_document_woken(one);
    cli_lock_give(&s_lock);
    return 1;
}

int cli_document_census(const char *key, CliCensus *census)
{
    cli_lock_take(&s_lock);
    CliStoredText *const one = stored_whole_of(key);
    if (one != NULL)
    {
        *census = one->lines.census;
    }
    cli_lock_give(&s_lock);
    return one != NULL;
}

int cli_document_marks(const char *key, int head_given, const char *head, unsigned long long head_size, VerbaLinea *out)
{
    cli_lock_take(&s_lock);
    CliStoredText *const one = stored_whole_of(key);
    if (one == NULL)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    if (head_given)
    {
        one->head_known = 1;
        one->head_has = head != NULL;
        stored_room_exit(&one->head);
        if ((head != NULL) && stored_room_enter(&one->head, head_size + 1ull, stored_lines_in(head, head_size) + 1u,
                                                "the last commit's lines of a text stored"))
        {
            cli_document_lines_of(head, head_size, &one->head);
        }
        else
        {
            one->head_known = head == NULL;
        }
    }
    if (!one->head_known)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    if (!one->head_has)
    {
        mmgr_line_put(out, "{\"head\":false,\"changes\":null}");
        cli_lock_give(&s_lock);
        return 1;
    }
    mmgr_line_put(out, "{\"head\":true,\"changes\":");
    if (!cli_diff_line_changes(&one->head, &one->lines, out))
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_ch(out, '}');
    cli_lock_give(&s_lock);
    return 1;
}

int cli_document_has_head(const char *key)
{
    cli_lock_take(&s_lock);
    const CliStoredText *const one = stored_whole_of(key);
    const int has = (one != NULL) && one->head_known;
    cli_lock_give(&s_lock);
    return has;
}

int cli_document_find(const char *key, const CliDocSought *sought, unsigned long long most, VerbaLinea *out,
                      VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    const CliStoredText *const one = stored_whole_of(key);
    if (one == NULL)
    {
        mmgr_line_put(err_msg, "no text is stored under ");
        mmgr_line_put(err_msg, key);
        cli_lock_give(&s_lock);
        return 0;
    }
    const int found = cli_document_find_in(&one->lines, sought, most, out, err_msg);
    cli_lock_give(&s_lock);
    return found;
}
