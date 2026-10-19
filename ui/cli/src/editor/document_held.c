// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_held.c: the texts the page holds here, by key: held whole or a part at a time, written by the page's edits,
// marked against the last commit's file, and searched
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "document.h"

// the most units and lines of a text held, and the most bytes of a text given in parts
#define CLI_DOC_UNITS (1u << 22u)
#define CLI_DOC_LINES (1u << 18u)
#define CLI_DOC_GIVEN (1u << 23u)

// The storage of one text's lines
typedef struct
{
    unsigned short unit[CLI_DOC_UNITS];
    unsigned long long start[CLI_DOC_LINES];
    unsigned long long length[CLI_DOC_LINES];
} CliLineRoom;

// A text held, a cell of its own taken as it is first held and given back as it is let go, as many held as the cells
// meet: the next held, its key, its lines, whether the last commit's file is known and whether the commit holds one,
// its lines, and the parts given so far
typedef struct CliHeldText
{
    struct CliHeldText *next;
    char key[CLI_NAME_LONGEST];
    CliLineRoom room;
    CliLines lines;
    int head_known;
    int head_has;
    CliLineRoom head_room;
    CliLines head;
    char given[CLI_DOC_GIVEN];
    unsigned long long given_size;
} CliHeldText;

// the first of the texts held, and the lock they are read and written under
static CliHeldText *s_held;
static CliLock s_lock = CLI_LOCK_INIT;

// a line room seen as lines
static CliLines cli_lines_over(CliLineRoom *room)
{
    return (CliLines){room->unit, CLI_DOC_UNITS, 0ull, room->start, room->length, CLI_DOC_LINES, 0u, 0};
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
    memor.cpy(&(MemoriaCfg){
        .dst = one->key, .src = key, .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    one->lines = cli_lines_over(&one->room);
    one->head = cli_lines_over(&one->head_room);
    one->head_known = 0;
    one->head_has = 0;
    one->given_size = 0ull;
    one->next = s_held;
    s_held = one;
    return one;
}

// a text held let go: taken off the texts held and its cell given back
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
    cli_cell_given(one);
}

int cli_document_hold(const char *key, const char *text, unsigned long long size, CliShape *shape)
{
    cli_lock_take(&s_lock);
    CliHeldText *const one = cli_held_made(key);
    if (one != NULL)
    {
        cli_document_lines_of(text, size, &one->lines);
        one->head_known = 0;
        one->given_size = 0ull;
        *shape = cli_document_shape_of(&one->lines);
    }
    cli_lock_give(&s_lock);
    return one != NULL;
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
    const unsigned long long room = CLI_DOC_GIVEN - one->given_size;
    const unsigned long long kept = (size < room) ? size : room;
    memor.cpy(&(MemoriaCfg){.dst = one->given + one->given_size, .src = part, .bytes = kept});
    one->given_size += kept;
    if (done)
    {
        cli_document_lines_of(one->given, one->given_size, &one->lines);
        one->given_size = 0ull;
        one->head_known = 0;
        *shape = cli_document_shape_of(&one->lines);
    }
    cli_lock_give(&s_lock);
    return done;
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

// one edit written over the text's lines, through the lines of `room`: the lines before it, its text split at each
// "\n" with the head of its first line before it and the tail of its last after it, and the lines after it
static void cli_edit_written(CliHeldText *one, const CliEdit *edit, CliLineRoom *room)
{
    CliLines *const now = &one->lines;
    CliLines spare = cli_lines_over(room);
    for (unsigned long long line = 0ull; line < edit->from.line; line += 1ull)
    {
        cli_line_begun(&spare);
        cli_units_put(&spare, now->unit + now->start[line], now->length[line]);
        cli_line_ended(&spare);
    }
    cli_line_begun(&spare);
    cli_units_put(&spare, now->unit + now->start[edit->from.line], edit->from.col);
    for (unsigned long long at = 0ull; at < edit->length;)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)edit->text, edit->length, at, &letter);
        if (letter == '\n')
        {
            cli_line_ended(&spare);
            cli_line_begun(&spare);
            continue;
        }
        unsigned short units[2];
        const unsigned long long count = (letter > 0xffffu) ? 2ull : 1ull;
        // each unit of a letter is below 2^16
        units[0] = (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
        units[1] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
        cli_units_put(&spare, units, count);
    }
    const unsigned long long tail_line = edit->to.line;
    cli_units_put(&spare, now->unit + now->start[tail_line] + edit->to.col, now->length[tail_line] - edit->to.col);
    cli_line_ended(&spare);
    for (unsigned long long line = tail_line + 1ull; line < now->lines; line += 1ull)
    {
        cli_line_begun(&spare);
        cli_units_put(&spare, now->unit + now->start[line], now->length[line]);
        cli_line_ended(&spare);
    }
    memor.cpy(&(MemoriaCfg){.dst = now->unit, .src = spare.unit, .bytes = spare.used * sizeof(unsigned short)});
    memor.cpy(&(MemoriaCfg){.dst = now->start, .src = spare.start, .bytes = spare.lines * sizeof(unsigned long long)});
    memor.cpy(
        &(MemoriaCfg){.dst = now->length, .src = spare.length, .bytes = spare.lines * sizeof(unsigned long long)});
    now->used = spare.used;
    now->lines = spare.lines;
    now->full = now->full || spare.full;
}

// whether a place stands in the text: on a line it holds, at or before the line's end
static int cli_fits(const CliLines *lines, const CliPlace *place)
{
    return (place->line < lines->lines) && (place->col <= lines->length[place->line]);
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
    CliLineRoom *const room = cli_cell_taken(sizeof(*room), "the lines an edit writes");
    if (room == NULL)
    {
        cli_line_text(said, "the lines an edit writes could not be taken");
        cli_lock_give(&s_lock);
        return 0;
    }
    int fits = 1;
    for (unsigned int change = 0u; fits && (change < count); change += 1u)
    {
        for (unsigned int at = edits[change]; fits && (at > 0u); at -= 1u)
        {
            const CliEdit *const edit = &changes[change][at - 1u];
            const int before = (edit->to.line < edit->from.line) ||
                               ((edit->to.line == edit->from.line) && (edit->to.col < edit->from.col));
            fits = cli_fits(&one->lines, &edit->from) && cli_fits(&one->lines, &edit->to) && !before;
            if (fits)
            {
                cli_edit_written(one, edit, room);
            }
        }
    }
    cli_cell_given(room);
    if (!fits)
    {
        cli_held_let_go(one);
        cli_line_text(said, "an edit does not fit the text held under ");
        cli_line_text(said, key);
        cli_lock_give(&s_lock);
        return 0;
    }
    *shape = cli_document_shape_of(&one->lines);
    cli_lock_give(&s_lock);
    return 1;
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
        if (head != NULL)
        {
            cli_document_lines_of(head, head_size, &one->head);
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
