// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document.c: a text's lines of UTF-16 units as the page splits them, and find over them where its answer is the
// page's own
#include "document.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/sift.h"
#include "../text/vector.h"

// the most units of a text's lines joined, and the most lines
#define CLI_JOINED_UNITS (1u << 24u)
#define CLI_JOINED_LINES (1u << 20u)

// the search not the page's own here
#define CLI_PAGE_OWN "this search is the page's own"

// a letter put after the lines' units, as UTF-16
static void cli_unit_put(CliLines *lines, unsigned int letter)
{
    const unsigned long long need = (letter > 0xffffu) ? 2ull : 1ull;
    if ((lines->used + need) > lines->units_most)
    {
        lines->full = 1;
        return;
    }
    if (letter > 0xffffu)
    {
        const unsigned int rest = letter - 0x10000u;
        // each half of a pair of surrogates is below 2^16
        lines->unit[lines->used] = (unsigned short)(0xd800u + (rest >> 10u));
        lines->unit[lines->used + 1ull] = (unsigned short)(0xdc00u + (rest & 0x3ffu));
    }
    else
    {
        // a letter of the basic plane is below 2^16
        lines->unit[lines->used] = (unsigned short)letter;
    }
    lines->used += need;
}

void cli_document_lines_of(const char *text, unsigned long long size, CliLines *lines)
{
    lines->used = 0ull;
    lines->lines = 0u;
    lines->full = 0;
    unsigned long long at = 0ull;
    for (;;)
    {
        const char *const newline =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at}) : NULL;
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        unsigned long long length = end - at;
        length -= ((length > 0ull) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
        if (lines->lines >= lines->lines_most)
        {
            lines->full = 1;
            return;
        }
        lines->start[lines->lines] = lines->used;
        for (unsigned long long place = at; place < (at + length);)
        {
            unsigned int letter = 0u;
            place += cli_lossy_letter((const unsigned char *)text, at + length, place, &letter);
            cli_unit_put(lines, letter);
        }
        lines->length[lines->lines] = lines->used - lines->start[lines->lines];
        lines->lines += 1u;
        if (newline == NULL)
        {
            return;
        }
        at = end + 1ull;
    }
}

CliShape cli_document_shape_of(const CliLines *lines)
{
    CliShape shape = {lines->lines, 0ull};
    for (unsigned int at = 0u; at < lines->lines; at += 1u)
    {
        shape.units += lines->length[at];
    }
    return shape;
}

void cli_document_units_written(CliLine *out, const unsigned short *units, unsigned long long count)
{
    char bytes[8];
    cli_line_character(out, '"');
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        unsigned int letter = units[at];
        if ((letter >= 0xd800u) && (letter <= 0xdbffu) && ((at + 1ull) < count) && (units[at + 1ull] >= 0xdc00u) &&
            (units[at + 1ull] <= 0xdfffu))
        {
            letter = 0x10000u + ((letter - 0xd800u) << 10u) + (units[at + 1ull] - 0xdc00u);
            at += 1ull;
        }
        else if ((letter >= 0xd800u) && (letter <= 0xdfffu))
        {
            letter = 0xfffdu;
        }
        CliLine one = {bytes, sizeof(bytes), 0ull};
        cli_utf8_put(&one, letter);
        cli_json_escaped_lossy(out, (const unsigned char *)bytes, one.at);
    }
    cli_line_character(out, '"');
}

// A text's lines joined, each ended by "\n", and the unit each starts at, the units' count last
typedef struct
{
    unsigned short unit[CLI_JOINED_UNITS];
    unsigned long long used;
    unsigned long long start[CLI_JOINED_LINES + 1u];
    unsigned int lines;
} CliJoined;

// the lines joined into `joined`: 1, or 0 where they do not fit
static int cli_joined_of(const CliLines *lines, CliJoined *joined)
{
    joined->used = 0ull;
    joined->lines = 0u;
    if (lines->lines > CLI_JOINED_LINES)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < lines->lines; at += 1u)
    {
        if ((joined->used + lines->length[at] + 1ull) > CLI_JOINED_UNITS)
        {
            return 0;
        }
        joined->start[at] = joined->used;
        memor.cpy(&(MemoriaCfg){.dst = joined->unit + joined->used,
                                .src = lines->unit + lines->start[at],
                                .bytes = lines->length[at] * sizeof(unsigned short)});
        joined->used += lines->length[at];
        joined->unit[joined->used] = '\n';
        joined->used += 1ull;
    }
    joined->start[lines->lines] = joined->used;
    joined->lines = lines->lines;
    return 1;
}

// a word's letter as the page's find reads one for whole words, where it is ASCII: a letter, a digit, `_` or `$`: 1 or
// 0, or -1 for a unit outside ASCII, which the page reads by its own tables
static int cli_word_unit(unsigned short unit)
{
    if (unit >= 0x80u)
    {
        return -1;
    }
    return ((unit >= 'a') && (unit <= 'z')) || ((unit >= 'A') && (unit <= 'Z')) || ((unit >= '0') && (unit <= '9')) ||
           (unit == '_') || (unit == '$');
}

// whether the span [from, to) of a line is a whole word: 1 or 0, or -1 where a unit beside it is outside ASCII
static int cli_whole(const unsigned short *line, unsigned long long length, unsigned long long from,
                     unsigned long long to)
{
    const int before = (from == 0ull) ? 0 : cli_word_unit(line[from - 1ull]);
    const int after = (to == length) ? 0 : cli_word_unit(line[to]);
    if ((before < 0) || (after < 0))
    {
        return -1;
    }
    return !before && !after;
}

// the matches found, held until the search is known to be this side's, and the most bytes of them written
#define CLI_FIND_MATCHES (1u << 20u)
#define CLI_FIND_WRITTEN (1u << 24u)

// A search's room, a cell of the search's own: the text's lines joined, the units sought, and the matches found
typedef struct
{
    CliJoined joined;
    unsigned short wanted[1u << 16u];
    unsigned long long match[CLI_FIND_MATCHES][3];
} CliFindRoom;

// the search of `size` units sought through the room's lines joined, each match handed to `each`: 1, or 0 with why
// said where the page searches itself
static int cli_found_each(CliFindRoom *room, unsigned long long size, const CliDocSought *sought,
                          unsigned long long most, CliFound3 each, void *context, CliLine *said);

int cli_document_find_each(const CliLines *lines, const CliDocSought *sought, unsigned long long most, CliFound3 each,
                           void *context, CliLine *said)
{
    int ascii = 1;
    for (const char *at = sought->query; at[0] != '\0'; at += 1)
    {
        ascii = ascii && ((unsigned char)at[0] < 0x80u);
    }
    if (sought->regex || (!sought->match_case && !ascii))
    {
        cli_line_text(said, CLI_PAGE_OWN);
        return 0;
    }
    CliFindRoom *const room = cli_cell_taken(sizeof(*room), "a search of a text");
    if (room == NULL)
    {
        cli_line_text(said, "the text could not be searched: the cells cannot meet it");
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned long long query_size = cellul.len(&(CatenaFinitaCfg){.src = sought->query, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at < query_size;)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)sought->query, query_size, at, &letter);
        const unsigned long long need = (letter > 0xffffu) ? 2ull : 1ull;
        if ((size + need) > (sizeof(room->wanted) / sizeof(room->wanted[0])))
        {
            break;
        }
        // each unit of a letter is below 2^16
        room->wanted[size] = (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
        room->wanted[size + 1ull] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
        size += need;
    }
    int newline = 0;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        newline = newline || (room->wanted[at] == '\n');
    }
    const int done = (size == 0ull) || newline || !cli_joined_of(lines, &room->joined) ||
                     cli_found_each(room, size, sought, most, each, context, said);
    cli_cell_given(room);
    return done;
}

// A search's walk over the joined units as the sift hands each offset it finds the needle at: the matches taken in
// order, each past the one before, on a unit's boundary, a whole word where one is sought, as the page searches each
// line whole and stops before the first line it starts with `most` found
typedef struct
{
    CliFindRoom *room;
    const CliDocSought *sought;
    unsigned long long size;
    unsigned long long most;
    unsigned long long found;
    unsigned int line;
    unsigned int last;
    unsigned long long next;
    int stopped;
    int own;
} CliFindWalk;

static void cli_found_at(void *context, unsigned long long byte)
{
    CliFindWalk *const walk = (CliFindWalk *)context;
    const unsigned long long hit = byte / 2ull;
    // a hit between two units, or inside the match before, is none
    if (walk->stopped || walk->own || ((byte % 2ull) != 0ull) || (hit < walk->next))
    {
        return;
    }
    const CliJoined *const joined = &walk->room->joined;
    while (joined->start[walk->line + 1u] <= hit)
    {
        walk->line += 1u;
    }
    if ((walk->last != 0xffffffffu) && (walk->line > walk->last))
    {
        walk->stopped = 1;
        return;
    }
    const unsigned long long from = hit - joined->start[walk->line];
    const unsigned long long to = from + walk->size;
    if (walk->sought->word)
    {
        const unsigned long long length = joined->start[walk->line + 1u] - joined->start[walk->line] - 1ull;
        const int whole = cli_whole(joined->unit + joined->start[walk->line], length, from, to);
        if (whole < 0)
        {
            walk->own = 1;
            return;
        }
        if (!whole)
        {
            walk->next = hit + 1ull;
            return;
        }
    }
    if (walk->found < CLI_FIND_MATCHES)
    {
        walk->room->match[walk->found][0] = walk->line;
        walk->room->match[walk->found][1] = from;
        walk->room->match[walk->found][2] = to;
    }
    walk->found += 1ull;
    if ((walk->found >= walk->most) && (walk->last == 0xffffffffu))
    {
        walk->last = walk->line;
    }
    walk->next = hit + walk->size;
}

static int cli_found_each(CliFindRoom *room, unsigned long long size, const CliDocSought *sought,
                          unsigned long long most, CliFound3 each, void *context, CliLine *said)
{
    // a search without case folds the room's units and the needle's first, each unit to the one the page's search
    // folds it to, so the sift reads them as they are: the units searched as their bytes, the needle's laid the same
    // way, a match starting on an even byte; a text within one of the sift's windows goes by Horspool's shifts and a
    // longer one by the sift's plan
    if (!sought->match_case)
    {
        for (unsigned long long at = 0ull; at < room->joined.used; at += 1ull)
        {
            room->joined.unit[at] = cli_vector_fold(room->joined.unit[at]);
        }
        for (unsigned long long at = 0ull; at < size; at += 1ull)
        {
            room->wanted[at] = cli_vector_fold(room->wanted[at]);
        }
    }
    CliFindWalk walk = {room, sought, size, most, 0ull, 0u, 0xffffffffu, 0ull, 0, 0};
    (void)cli_sift_find_all((const unsigned char *)room->joined.unit, room->joined.used * 2ull,
                            (const unsigned char *)room->wanted, size * 2ull, 0, cli_found_at, &walk);
    if (walk.own)
    {
        cli_line_text(said, CLI_PAGE_OWN);
        return 0;
    }
    for (unsigned long long one = 0ull; (one < walk.found) && (one < CLI_FIND_MATCHES); one += 1ull)
    {
        each(context, room->match[one][0], room->match[one][1], room->match[one][2]);
    }
    return 1;
}

// A match written as `[line, from, to]`, a comma before it where it is not the first
typedef struct
{
    CliLine *out;
    unsigned long long written;
} CliMatchesOut;

static void cli_match_written(void *context, unsigned long long line, unsigned long long from, unsigned long long to)
{
    CliMatchesOut *const matches = (CliMatchesOut *)context;
    cli_line_text(matches->out, (matches->written != 0ull) ? ",[" : "[");
    cli_line_decimal(matches->out, line, 0u);
    cli_line_character(matches->out, ',');
    cli_line_decimal(matches->out, from, 0u);
    cli_line_character(matches->out, ',');
    cli_line_decimal(matches->out, to, 0u);
    cli_line_character(matches->out, ']');
    matches->written += 1ull;
}

int cli_document_find_in(const CliLines *lines, const CliDocSought *sought, unsigned long long most, CliLine *out,
                         CliLine *said)
{
    char *const text = cli_cell_taken(CLI_FIND_WRITTEN, "the matches of a search written");
    if (text == NULL)
    {
        cli_line_text(said, "the matches could not be written: the cells cannot meet them");
        return 0;
    }
    CliLine written = {text, CLI_FIND_WRITTEN, 0ull};
    CliMatchesOut matches = {&written, 0ull};
    const int found = cli_document_find_each(lines, sought, most, cli_match_written, &matches, said);
    if (found)
    {
        cli_line_character(out, '[');
        cli_line_counted(out, text, written.at);
        cli_line_character(out, ']');
    }
    cli_cell_given(text);
    return found;
}
