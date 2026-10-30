// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document.c: a text's lines of UTF-16 units as the page splits them, and find over them: text sought by the sift, and
// a pattern, or text sought without case outside ASCII, by the regular expressions the colorer reads
#include "document.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/sift.h"
#include "../text/vector.h"

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
        cli_lines_census_put(lines, lines->unit[lines->used + 1ull]);
    }
    else
    {
        // a letter of the basic plane is below 2^16
        lines->unit[lines->used] = (unsigned short)letter;
    }
    cli_lines_census_put(lines, lines->unit[lines->used]);
    lines->used += need;
}

void cli_document_lines_of(const char *text, unsigned long long size, CliLines *lines)
{
    lines->used = 0ull;
    lines->lines = 0u;
    lines->full = 0;
    memor.set(&(MemoriaCfg){.dst = &lines->census, .val = 0, .bytes = sizeof(lines->census)});
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
        if (lines->begun != NULL)
        {
            lines->begun[lines->lines] = CLI_DOC_UNCOLORED;
            lines->ended[lines->lines] = CLI_DOC_UNCOLORED;
            lines->regions[lines->lines] = 0u;
            lines->runs[lines->lines] = CLI_DOC_NO_RUNS;
        }
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

unsigned int cli_document_unit_letter(const unsigned short *units, unsigned long long count, unsigned long long at,
                                      unsigned long long *took)
{
    unsigned int letter = units[at];
    *took = 1ull;
    if ((letter >= 0xd800u) && (letter <= 0xdbffu) && ((at + 1ull) < count) && (units[at + 1ull] >= 0xdc00u) &&
        (units[at + 1ull] <= 0xdfffu))
    {
        letter = 0x10000u + ((letter - 0xd800u) << 10u) + (units[at + 1ull] - 0xdc00u);
        *took = 2ull;
    }
    else if ((letter >= 0xd800u) && (letter <= 0xdfffu))
    {
        letter = 0xfffdu;
    }
    return letter;
}

void cli_document_units_written(VerbaLinea *out, const unsigned short *units, unsigned long long count)
{
    char bytes[8];
    mmgr_line_ch(out, '"');
    unsigned long long at = 0ull;
    while (at < count)
    {
        unsigned long long took = 0ull;
        const unsigned int letter = cli_document_unit_letter(units, count, at, &took);
        at += took;
        VerbaLinea one = {bytes, sizeof(bytes), 0ull};
        cli_utf8_put(&one, letter);
        cli_json_escaped_lossy(out, (const unsigned char *)bytes, one.at);
    }
    mmgr_line_ch(out, '"');
}

void cli_document_units_utf8(VerbaLinea *out, const unsigned short *units, unsigned long long count)
{
    unsigned long long at = 0ull;
    while (at < count)
    {
        unsigned long long took = 0ull;
        const unsigned int letter = cli_document_unit_letter(units, count, at, &took);
        at += took;
        cli_utf8_put(out, letter);
    }
}

// A text's lines joined, each ended by "\n", and the unit each starts at, the units' count last, in a cell sized to them
typedef struct
{
    unsigned short *unit;
    unsigned long long used;
    unsigned long long *start;
    unsigned int lines;
} CliJoined;

// the lines joined into `joined`, in a cell taken for them, given back with cli_joined_let_go: 1, or 0 where the cells
// cannot meet them
static int cli_joined_of(const CliLines *lines, CliJoined *joined)
{
    unsigned long long units = lines->lines;
    for (unsigned int at = 0u; at < lines->lines; at += 1u)
    {
        units += lines->length[at];
    }
    // the units' bytes rounded up to a boundary of the starts after them
    const unsigned long long unit_bytes = ((units * sizeof(unsigned short)) + 7ull) & ~7ull;
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(
        unit_bytes + (((unsigned long long)lines->lines + 1ull) * sizeof(unsigned long long)), NULL);
    *joined = (CliJoined){(unsigned short *)(void *)cell, 0ull, (unsigned long long *)(void *)(cell + unit_bytes), 0u};
    if (cell == NULL)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < lines->lines; at += 1u)
    {
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

// the cell the lines were joined in given back
static void cli_joined_let_go(CliJoined *joined)
{
    cli_cell_release(joined->unit, NULL);
    *joined = (CliJoined){NULL, 0ull, NULL, 0u};
}

// a word's letter as find reads one for whole words, where it is ASCII: a letter, a digit, `_` or `$`: 1 or 0, or -1
// for a unit outside ASCII, which the regular expressions' tables read
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

// the most matches found that are buffered until the sift's search is known to be whole
#define CLI_FIND_MATCHES (1u << 20u)

// whether `letter` is a letter of a word as find reads one for whole words: a letter, a number, `_` or `$`
static int cli_word_letter(unsigned int letter)
{
    return (letter == '_') || (letter == '$') || cli_regexp_property(CLI_REGEXP_LETTER, letter) ||
           cli_regexp_property(CLI_REGEXP_NUMBER, letter);
}

// the query of `sought` written into `out` as a pattern's source: the query itself where it is a pattern, else with
// each sign a pattern reads escaped
static void cli_pattern_source(const CliDocSought *sought, VerbaLinea *out)
{
    if (sought->regex)
    {
        mmgr_line_put(out, sought->query);
        return;
    }
    static const char s_signs[] = ".*+?^${}()|[]\\";
    for (const char *at = sought->query; at[0] != '\0'; at += 1)
    {
        if (cellul.chr(&(CatenaFinitaCfg){.src = s_signs, .cap = CLI_CELLS_POOL, .byte = at[0]}) != NULL)
        {
            mmgr_line_ch(out, '\\');
        }
        mmgr_line_ch(out, at[0]);
    }
}

// the line `line` of `lines` as code points into `letters`, each one's col in UTF-16 units into `cols` and the line's
// length after the last: how many
static unsigned long long cli_line_letters(const CliLines *lines, unsigned int line, unsigned int *letters,
                                           unsigned int *cols)
{
    const unsigned short *const unit = lines->unit + lines->start[line];
    const unsigned long long length = lines->length[line];
    unsigned long long count = 0ull;
    for (unsigned long long place = 0ull; place < length; count += 1ull)
    {
        const int paired = (unit[place] >= 0xd800u) && (unit[place] < 0xdc00u) && ((place + 1ull) < length) &&
                           (unit[place + 1ull] >= 0xdc00u) && (unit[place + 1ull] < 0xe000u);
        // a line's cols stand below its length, which an unsigned int counts
        cols[count] = (unsigned int)place;
        letters[count] = paired ? (0x10000u + ((unit[place] - 0xd800u) << 10u) + (unit[place + 1ull] - 0xdc00u))
                                : unit[place];
        place += paired ? 2ull : 1ull;
    }
    // the length stands below 2^32, as a line's cols do
    cols[count] = (unsigned int)length;
    return count;
}

// the matches of `sought` read as a pattern by the regular expressions the colorer reads, under the `u` flag, and `i`
// where it runs without case, the `u` let go where the pattern reads only without it and whole words are not sought, as
// the page's find reads one: each line matched from each letter a match can start at, a match of no letters passed
// over and one that is no whole word where whole words are sought, each handed to `each` with its line and its cols in
// UTF-16 units, the lines searched whole until one ends with `most` found: 1, or 0 with why given where the pattern
// does not read or the cells cannot meet it
static int cli_found_patterned(const CliLines *lines, const CliDocSought *sought, unsigned long long most,
                               CliFound3 each, void *context, VerbaLinea *err_msg)
{
    char source[2u * CLI_REGEXP_LETTERS];
    VerbaLinea written = {source, sizeof(source), 0ull};
    cli_pattern_source(sought, &written);
    if (mmgr_line_finish(&written) == 0ull)
    {
        mmgr_line_put(err_msg, "the query is too long to read as a pattern");
        return 0;
    }
    unsigned long long longest = 0ull;
    for (unsigned int line = 0u; line < lines->lines; line += 1u)
    {
        longest = (lines->length[line] > longest) ? lines->length[line] : longest;
    }
    const unsigned long long bytes = sizeof(CliRegexpRoom) + (2ull * (longest + 1ull) * sizeof(unsigned int));
    unsigned char *const cell = cli_lane_alloc(bytes, NULL);
    if (cell == NULL)
    {
        mmgr_line_put(err_msg, "the text could not be searched: the cells cannot meet it");
        return 0;
    }
    unsigned int *const letters = (unsigned int *)(void *)(cell + sizeof(CliRegexpRoom));
    unsigned int *const cols = letters + longest + 1ull;
    CliRegexp regexp;
    cli_regexp_roomed(&regexp, (CliRegexpRoom *)(void *)cell);
    const unsigned long long said = err_msg->at;
    int read = cli_regexp_read(&regexp, source, sought->match_case ? "u" : "iu", err_msg);
    if (!read && !sought->word)
    {
        err_msg->at = said;
        cli_regexp_roomed(&regexp, (CliRegexpRoom *)(void *)cell);
        read = cli_regexp_read(&regexp, source, sought->match_case ? "" : "i", err_msg);
    }
    unsigned long long found = 0ull;
    for (unsigned int line = 0u; read && (line < lines->lines) && (found < most); line += 1u)
    {
        const unsigned long long count = cli_line_letters(lines, line, letters, cols);
        for (unsigned long long at = 0ull; at < count;)
        {
            CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
            const int starts = (!regexp.anchored || (at == 0ull)) &&
                               (regexp.first.empty || cli_regexp_allows(&regexp.first, letters[at]));
            const int matched = starts && cli_regexp_match_into(&regexp, letters, count, at, groups) &&
                                (groups[0].to > groups[0].from);
            const unsigned long long to = matched ? groups[0].to : at;
            const int whole = !sought->word || (((at == 0ull) || !cli_word_letter(letters[at - 1ull])) &&
                                                ((to == count) || !cli_word_letter(letters[to])));
            if (matched && whole)
            {
                each(context, line, cols[at], cols[to]);
                found += 1ull;
                at = to;
                continue;
            }
            at += 1ull;
        }
    }
    cli_cell_release(cell, NULL);
    return read;
}

// A search's room, each part in a cell sized to what it holds: the text's lines joined, the units sought, and the
// matches found, room for as many as the joined units hold of the units sought, CLI_FIND_MATCHES at the most
typedef struct
{
    CliJoined joined;
    unsigned short *wanted;
    unsigned long long (*match)[3];
    unsigned long long match_room;
} CliFindRoom;

// the search of `size` units sought through the room's lines joined, the lines `lines`, each match handed to `each`,
// searched again as a pattern where whole words are sought and a unit beside a match is outside ASCII: 1, or 0 with why
// given
static int cli_found_each(CliFindRoom *room, const CliLines *lines, const CliCensus *census, unsigned long long size,
                          const CliDocSought *sought, unsigned long long most, CliFound3 each, void *context,
                          VerbaLinea *err_msg);

int cli_document_find_each(const CliLines *lines, const CliDocSought *sought, unsigned long long most, CliFound3 each,
                           void *context, VerbaLinea *err_msg)
{
    int ascii = 1;
    for (const char *at = sought->query; at[0] != '\0'; at += 1)
    {
        ascii = ascii && ((unsigned char)at[0] < 0x80u);
    }
    if (sought->regex || (!sought->match_case && !ascii))
    {
        return cli_found_patterned(lines, sought, most, each, context, err_msg);
    }
    const unsigned long long query_size = cellul.len(&(CatenaFinitaCfg){.src = sought->query, .cap = CLI_CELLS_POOL});
    // a unit of the query takes a byte of it at the least, and the unit after the last is written as a pair's second
    CliFindRoom room = {{NULL, 0ull, NULL, 0u},
                        g_cli_cells->persistent_buf_alloc((query_size + 1ull) * sizeof(unsigned short), NULL),
                        NULL,
                        0ull};
    if (room.wanted == NULL)
    {
        mmgr_line_put(err_msg, "the text could not be searched: the cells cannot meet it");
        return 0;
    }
    unsigned long long size = 0ull;
    for (unsigned long long at = 0ull; at < query_size;)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)sought->query, query_size, at, &letter);
        const unsigned long long need = (letter > 0xffffu) ? 2ull : 1ull;
        // each unit of a letter is below 2^16
        room.wanted[size] = (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
        room.wanted[size + 1ull] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
        size += need;
    }
    int newline = 0;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        newline = newline || (room.wanted[at] == '\n');
    }
    // the search reads the text's stored census, with the newline the join puts after each line, and folded as the
    // search folds where it runs without case
    CliCensus census = lines->census;
    census.occurrences['\n'] += lines->lines;
    census.occurrences[0] += lines->lines;
    census.total += 2ull * lines->lines;
    if (!sought->match_case)
    {
        CliCensus folded;
        cli_sift_census_folded(&census, &folded);
        census = folded;
    }
    const int sought_any = (size != 0ull) && !newline;
    const int joined = sought_any && cli_joined_of(lines, &room.joined);
    if (joined)
    {
        const unsigned long long fit = (room.joined.used / size) + 1ull;
        room.match_room = (fit < CLI_FIND_MATCHES) ? fit : CLI_FIND_MATCHES;
        room.match = g_cli_cells->persistent_buf_alloc(room.match_room * sizeof(*room.match), NULL);
    }
    const int roomed = !sought_any || (joined && (room.match != NULL));
    if (!roomed)
    {
        mmgr_line_put(err_msg, "the text could not be searched: the cells cannot meet it");
    }
    const int done =
        roomed && (!sought_any || cli_found_each(&room, lines, &census, size, sought, most, each, context, err_msg));
    cli_cell_release(room.match, NULL);
    cli_joined_let_go(&room.joined);
    cli_cell_release(room.wanted, NULL);
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
    int patterned;
} CliFindWalk;

static void cli_found_at(void *context, unsigned long long byte)
{
    CliFindWalk *const walk = (CliFindWalk *)context;
    const unsigned long long hit = byte / 2ull;
    // a hit between two units, or inside the match before, is none
    if (walk->stopped || walk->patterned || ((byte % 2ull) != 0ull) || (hit < walk->next))
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
            walk->patterned = 1;
            return;
        }
        if (!whole)
        {
            walk->next = hit + 1ull;
            return;
        }
    }
    if (walk->found < walk->room->match_room)
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

static int cli_found_each(CliFindRoom *room, const CliLines *lines, const CliCensus *census, unsigned long long size,
                          const CliDocSought *sought, unsigned long long most, CliFound3 each, void *context,
                          VerbaLinea *err_msg)
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
    (void)cli_sift_find_all_with(census, (const unsigned char *)room->joined.unit, room->joined.used * 2ull,
                                 (const unsigned char *)room->wanted, size * 2ull, 0, cli_found_at, &walk);
    if (walk.patterned)
    {
        return cli_found_patterned(lines, sought, most, each, context, err_msg);
    }
    for (unsigned long long one = 0ull; (one < walk.found) && (one < room->match_room); one += 1ull)
    {
        each(context, room->match[one][0], room->match[one][1], room->match[one][2]);
    }
    return 1;
}

// A match written as `[line, from, to]`, a comma before it where it is not the first
typedef struct
{
    VerbaLinea *out;
    unsigned long long written;
} CliMatchesOut;

static void cli_match_written(void *context, unsigned long long line, unsigned long long from, unsigned long long to)
{
    CliMatchesOut *const matches = (CliMatchesOut *)context;
    mmgr_line_put(matches->out, (matches->written != 0ull) ? ",[" : "[");
    mmgr_line_decimal(matches->out, line, 0u);
    mmgr_line_ch(matches->out, ',');
    mmgr_line_decimal(matches->out, from, 0u);
    mmgr_line_ch(matches->out, ',');
    mmgr_line_decimal(matches->out, to, 0u);
    mmgr_line_ch(matches->out, ']');
    matches->written += 1ull;
}

int cli_document_find_in(const CliLines *lines, const CliDocSought *sought, unsigned long long most, VerbaLinea *out,
                         VerbaLinea *err_msg)
{
    // the matches written into `out` as they come, taken back out where the search is refused
    const unsigned long long start = out->at;
    mmgr_line_ch(out, '[');
    CliMatchesOut matches = {out, 0ull};
    const int found = cli_document_find_each(lines, sought, most, cli_match_written, &matches, err_msg);
    mmgr_line_ch(out, ']');
    out->at = found ? out->at : start;
    return found;
}
