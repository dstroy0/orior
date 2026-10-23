// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_held.c: the texts the page holds here, by key: held whole or a part at a time, written by the page's edits,
// marked against the last commit's file, and searched. Each text's lines live in a cell sized to the text with room to
// grow, moved to a larger cell as an edit outgrows it; the last commit's file in one sized to it; and the parts of a
// text given in parts in one that doubles as they come, given back as the last part makes the lines
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "document.h"

// the most units and lines of a text held
#define CLI_DOC_UNITS (1u << 22u)
#define CLI_DOC_LINES (1u << 18u)

// the least units and lines a text's room is given past what it holds, and the share of what it holds added as well, so
// an edit fits where it stands
#define CLI_DOC_SPARE_UNITS 1024ull
#define CLI_DOC_SPARE_LINES 64u
#define CLI_DOC_SPARE_SHARE 8ull

// the bytes the parts of a text are first gathered in
#define CLI_DOC_GIVEN_FIRST (1ull << 16u)

// A text held, a cell of its own taken as it is first held and given back as it is let go, as many held as the cells
// meet: the next held, its key, its lines in a room of their own, whether the last commit's file is known and whether
// the commit holds one, that file's lines in a room of their own, and the parts given so far in a cell of theirs
typedef struct CliHeldText
{
    struct CliHeldText *next;
    char key[CLI_NAME_LONGEST];
    CliLines lines;
    int head_known;
    int head_has;
    CliLines head;
    char *given;
    unsigned long long given_size;
    unsigned long long given_room;
} CliHeldText;

// the first of the texts held, and the lock they are read and written under
static CliHeldText *s_held;
static CliLock s_lock = CLI_LOCK_INIT;

// how many lines `size` bytes of `text` make, one more than its newlines
static unsigned int held_lines_in(const char *text, unsigned long long size)
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
    // the lines are held to CLI_DOC_LINES, which an unsigned int holds
    return (unsigned int)((count < CLI_DOC_LINES) ? count : CLI_DOC_LINES);
}

// `lines` given a room of `units_most` units and `lines_most` lines in a cell of its own, each held to the most a text
// may have: 1, or 0 with `lines` empty where the cells cannot meet it
static int held_room_taken(CliLines *lines, unsigned long long units_most, unsigned int lines_most, const char *what)
{
    const unsigned long long units = (units_most < CLI_DOC_UNITS) ? units_most : CLI_DOC_UNITS;
    const unsigned int count = (lines_most < CLI_DOC_LINES) ? lines_most : CLI_DOC_LINES;
    // the units' bytes rounded up to a boundary of the starts after them
    const unsigned long long unit_bytes = ((units * sizeof(unsigned short)) + 7ull) & ~7ull;
    const unsigned long long bytes = unit_bytes + (2ull * (unsigned long long)count * sizeof(unsigned long long));
    unsigned char *const cell = cli_cell_taken(bytes, what);
    *lines = (CliLines){NULL, 0ull, 0ull, NULL, NULL, 0u, 0u, 0};
    if (cell == NULL)
    {
        return 0;
    }
    unsigned long long *const start = (unsigned long long *)(void *)(cell + unit_bytes);
    *lines = (CliLines){(unsigned short *)(void *)cell, units, 0ull, start, start + count, count, 0u, 0};
    return 1;
}

// a room given back, `lines` left empty
static void held_room_given(CliLines *lines)
{
    cli_cell_given(lines->unit);
    *lines = (CliLines){NULL, 0ull, 0ull, NULL, NULL, 0u, 0u, 0};
}

// the room `size` bytes of `text` take with room to grow, taken for `lines` in place of what it held: 1, or 0 where
// the cells cannot meet it
static int held_room_for(CliLines *lines, const char *text, unsigned long long size, const char *what)
{
    held_room_given(lines);
    const unsigned long long units = size + (size / CLI_DOC_SPARE_SHARE) + CLI_DOC_SPARE_UNITS;
    const unsigned int count = held_lines_in(text, size);
    return held_room_taken(lines, units, count + (count / CLI_DOC_SPARE_SHARE) + CLI_DOC_SPARE_LINES, what);
}

// the text held under `key`, or NULL
static CliHeldText *cli_held_of(const char *key)
{
    for (CliHeldText *one = s_held; one != NULL; one = one->next)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            return one;
        }
    }
    return NULL;
}

// the text held under `key`, made afresh in a cell of its own where none is: NULL where the cells cannot meet it
static CliHeldText *cli_held_made(const char *key)
{
    CliHeldText *const there = cli_held_of(key);
    if (there != NULL)
    {
        return there;
    }
    CliHeldText *const one = cli_cell_taken(sizeof(*one), "a text held");
    if (one == NULL)
    {
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    memor.cpy(&(MemoriaCfg){
        .dst = one->key, .src = key, .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    one->next = s_held;
    s_held = one;
    return one;
}

// the parts gathered given back
static void held_given_dropped(CliHeldText *one)
{
    cli_cell_given(one->given);
    one->given = NULL;
    one->given_size = 0ull;
    one->given_room = 0ull;
}

// a text held let go: taken off the texts held and its cells given back
static void cli_held_let_go(CliHeldText *one)
{
    CliHeldText **link = &s_held;
    while ((*link != NULL) && (*link != one))
    {
        link = &(*link)->next;
    }
    if (*link == one)
    {
        *link = one->next;
    }
    held_room_given(&one->lines);
    held_room_given(&one->head);
    held_given_dropped(one);
    cli_cell_given(one);
}

// `size` bytes of `text` made the lines of the text held, in a room sized to them: 1, or 0 where the cells cannot meet
// it, the text let go
static int held_lines_made(CliHeldText *one, const char *text, unsigned long long size, CliShape *shape)
{
    if (!held_room_for(&one->lines, text, size, "a text held's lines"))
    {
        cli_held_let_go(one);
        return 0;
    }
    cli_document_lines_of(text, size, &one->lines);
    one->head_known = 0;
    *shape = cli_document_shape_of(&one->lines);
    return 1;
}

int cli_document_hold(const char *key, const char *text, unsigned long long size, CliShape *shape)
{
    cli_lock_take(&s_lock);
    CliHeldText *const one = cli_held_made(key);
    const int held = (one != NULL) && held_lines_made(one, text, size, shape);
    if (held)
    {
        held_given_dropped(one);
    }
    cli_lock_give(&s_lock);
    return held;
}

// `size` bytes of `part` gathered after the parts given, the cell they gather in doubled where they do not fit: 1, or 0
// where the cells cannot meet it
static int held_part_gathered(CliHeldText *one, const char *part, unsigned long long size)
{
    const unsigned long long needed = one->given_size + size;
    if (needed > one->given_room)
    {
        unsigned long long room = (one->given_room > 0ull) ? one->given_room : CLI_DOC_GIVEN_FIRST;
        while (room < needed)
        {
            room *= 2ull;
        }
        char *const grown = cli_cell_taken(room, "the parts of a text held");
        if (grown == NULL)
        {
            return 0;
        }
        if (one->given_size > 0ull)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = one->given, .bytes = one->given_size});
        }
        cli_cell_given(one->given);
        one->given = grown;
        one->given_room = room;
    }
    memor.cpy(&(MemoriaCfg){.dst = one->given + one->given_size, .src = part, .bytes = size});
    one->given_size = needed;
    return 1;
}

int cli_document_hold_part(const char *key, const char *part, unsigned long long size, int first, int done,
                           CliShape *shape)
{
    cli_lock_take(&s_lock);
    CliHeldText *const one = cli_held_made(key);
    if (one == NULL)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    if (first)
    {
        one->given_size = 0ull;
    }
    if (!held_part_gathered(one, part, size))
    {
        cli_held_let_go(one);
        cli_lock_give(&s_lock);
        return 0;
    }
    int made = done;
    if (done)
    {
        made = held_lines_made(one, one->given, one->given_size, shape);
        if (made)
        {
            held_given_dropped(one);
        }
    }
    cli_lock_give(&s_lock);
    return made;
}

void cli_document_release(const char *key)
{
    cli_lock_take(&s_lock);
    CliHeldText *const one = cli_held_of(key);
    if (one != NULL)
    {
        cli_held_let_go(one);
    }
    cli_lock_give(&s_lock);
}

int cli_document_shape(const char *key, CliShape *shape)
{
    cli_lock_take(&s_lock);
    const CliHeldText *const one = cli_held_of(key);
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

// a line begun after the spare lines, and the line being made ended
static void cli_line_begun(CliLines *lines)
{
    if (lines->lines >= lines->lines_most)
    {
        lines->full = 1;
        return;
    }
    lines->start[lines->lines] = lines->used;
    lines->length[lines->lines] = 0ull;
    lines->lines += 1u;
}

static void cli_line_ended(CliLines *lines)
{
    if (lines->lines > 0u)
    {
        lines->length[lines->lines - 1u] = lines->used - lines->start[lines->lines - 1u];
    }
}

// one edit written over the text's lines through `spare`, a room that holds the result: the lines before it, its text
// split at each "\n" with the head of its first line before it and the tail of its last after it, and the lines after
// it. The result goes back into the text's room where it fits, else `spare` becomes the text's room and the old one is
// given back
static void cli_edit_written(CliHeldText *one, const CliEdit *edit, CliLines *spare)
{
    CliLines *const now = &one->lines;
    for (unsigned long long line = 0ull; line < edit->from.line; line += 1ull)
    {
        cli_line_begun(spare);
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
        now->used = spare->used;
        now->lines = spare->lines;
        now->census = spare->census;
        held_room_given(spare);
    }
    else
    {
        held_room_given(now);
        *now = *spare;
        *spare = (CliLines){NULL, 0ull, 0ull, NULL, NULL, 0u, 0u, 0};
    }
    now->full = full;
}

// whether a place stands in the text: on a line it holds, at or before the line's end
static int cli_fits(const CliLines *lines, const CliPlace *place)
{
    return (place->line < lines->lines) && (place->col <= lines->length[place->line]);
}

// a room that holds the text as `edit` leaves it, with room to grow, taken for `spare`: 1, or 0 where the cells cannot
// meet it
static int held_spare_taken(const CliLines *now, const CliEdit *edit, CliLines *spare)
{
    // a unit of the edit's text takes a byte at the least, so its bytes bound the units it puts in
    const unsigned long long units = now->used + edit->length;
    const unsigned long long lines = (unsigned long long)now->lines + held_lines_in(edit->text, edit->length);
    const unsigned long long most = (lines < CLI_DOC_LINES) ? lines : CLI_DOC_LINES;
    return held_room_taken(spare, units + (units / CLI_DOC_SPARE_SHARE) + CLI_DOC_SPARE_UNITS,
                           (unsigned int)(most + (most / CLI_DOC_SPARE_SHARE) + CLI_DOC_SPARE_LINES),
                           "the lines an edit writes");
}

int cli_document_edit(const char *key, const CliEdit *const *changes, const unsigned int *edits, unsigned int count,
                      CliShape *shape, CliLine *said)
{
    cli_lock_take(&s_lock);
    CliHeldText *const one = cli_held_of(key);
    if (one == NULL)
    {
        cli_line_text(said, "no text is held under ");
        cli_line_text(said, key);
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
            CliLines spare;
            room = !fits || held_spare_taken(&one->lines, edit, &spare);
            if (fits && room)
            {
                cli_edit_written(one, edit, &spare);
            }
        }
    }
    if (!fits || !room)
    {
        cli_held_let_go(one);
        cli_line_text(said, fits ? "the lines an edit writes could not be taken for "
                                 : "an edit does not fit the text held under ");
        cli_line_text(said, key);
        cli_lock_give(&s_lock);
        return 0;
    }
    *shape = cli_document_shape_of(&one->lines);
    cli_lock_give(&s_lock);
    return 1;
}

int cli_document_census(const char *key, CliCensus *census)
{
    cli_lock_take(&s_lock);
    CliHeldText *const one = cli_held_of(key);
    if (one != NULL)
    {
        *census = one->lines.census;
    }
    cli_lock_give(&s_lock);
    return one != NULL;
}

int cli_document_marks(const char *key, int head_given, const char *head, unsigned long long head_size, CliLine *out)
{
    cli_lock_take(&s_lock);
    CliHeldText *const one = cli_held_of(key);
    if (one == NULL)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    if (head_given)
    {
        one->head_known = 1;
        one->head_has = head != NULL;
        held_room_given(&one->head);
        if ((head != NULL) && held_room_taken(&one->head, head_size + 1ull, held_lines_in(head, head_size) + 1u,
                                              "the last commit's lines of a text held"))
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
        cli_line_text(out, "{\"head\":false,\"changes\":null}");
        cli_lock_give(&s_lock);
        return 1;
    }
    cli_line_text(out, "{\"head\":true,\"changes\":");
    if (!cli_diff_line_changes(&one->head, &one->lines, out))
    {
        cli_line_text(out, "null");
    }
    cli_line_character(out, '}');
    cli_lock_give(&s_lock);
    return 1;
}

int cli_document_has_head(const char *key)
{
    cli_lock_take(&s_lock);
    const CliHeldText *const one = cli_held_of(key);
    const int has = (one != NULL) && one->head_known;
    cli_lock_give(&s_lock);
    return has;
}

int cli_document_find(const char *key, const CliDocSought *sought, unsigned long long most, CliLine *out, CliLine *said)
{
    cli_lock_take(&s_lock);
    const CliHeldText *const one = cli_held_of(key);
    if (one == NULL)
    {
        cli_line_text(said, "no text is held under ");
        cli_line_text(said, key);
        cli_lock_give(&s_lock);
        return 0;
    }
    const int found = cli_document_find_in(&one->lines, sought, most, out, said);
    cli_lock_give(&s_lock);
    return found;
}
