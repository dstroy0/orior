// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape_search.c: every match of a pattern in a text: the pattern read as its parts, its statement ends at its close
// left off; each place of the code tried in turn, a match's statement ends at its close left off and the search going
// on after it; each match's span and first line found by one walk of the text forward; and the template written with
// each placeholder as the code it took, its statement ends at its close left off. The code's tokens past
// CLI_SHAPE_TOKENS and the pattern's past CLI_SHAPE_PATTERN are not read. One search runs at a time
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "shape_internal.h"

// the bytes a match's template is begun to be written in, doubling as it is
#define CLI_SHAPE_WITH_FIRST 256u

CliCellsTally g_cli_shape_cells = CLI_CELLS_TALLY("cli_shape");

// A search's room, a cell taken with the search's lock and given back with it: the code's tokens and the pattern's
// parts, each part's hole, the holes and the matcher, the tokens' and the holes' lists after it in the same cell, as
// long as the code and the pattern are in bytes
typedef struct
{
    unsigned int *hole_of;
    CliShapeTokens code;
    CliShapeTokens parts;
    CliShapeHoles holes;
    CliShapeMatcher matcher;
} CliShapeSearchRoom;

static CliLock s_search_lock = CLI_LOCK_INIT;
static CliShapeSearchRoom *s_room;

// A walk of the text forward: the byte it stands at, its line and its column in UTF-16 units, and where its line starts
typedef struct
{
    unsigned long long at;
    unsigned int line;
    unsigned int col;
    unsigned long long line_start;
} CliShapeWalk;

// the walk moved forward to `byte`: the place it stands at
static CliTextPlace search_walked(const char *src, unsigned long long size, CliShapeWalk *walk, unsigned long long byte)
{
    while (walk->at < byte)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter(src, size, walk->at, &length);
        walk->at += length;
        if (letter == '\n')
        {
            walk->line += 1u;
            walk->col = 0u;
            walk->line_start = walk->at;
        }
        else
        {
            walk->col += (letter >= 0x10000u) ? 2u : 1u;
        }
    }
    return (CliTextPlace){walk->line, walk->col};
}

// the pattern read as its parts into the matcher's storage: their count
static unsigned int search_pattern(const char *language, const char *pattern, unsigned long long size)
{
    s_room->parts.count = 0u;
    s_room->parts.full = 0;
    s_room->holes.count = 0u;
    cli_shape_tokens(language, pattern, size, &s_room->parts, &s_room->holes);
    for (unsigned int at = 0u; at < s_room->parts.count; at += 1u)
    {
        s_room->hole_of[at] = CLI_SHAPE_HOLES;
    }
    for (unsigned int at = 0u; at < s_room->holes.count; at += 1u)
    {
        if (s_room->holes.hole[at].token < s_room->parts.count)
        {
            s_room->hole_of[s_room->holes.hole[at].token] = at;
        }
    }
    unsigned int count = s_room->parts.count;
    while ((count > 0u) && (s_room->hole_of[count - 1u] == CLI_SHAPE_HOLES) &&
           (s_room->parts.kind[count - 1u] == CLI_SHAPE_BREAK))
    {
        count -= 1u;
    }
    s_room->matcher.parts = &s_room->parts;
    return count;
}

// the template written with each placeholder as the code it took into `out`
static void search_filled(const char *template_text, CliLine *out)
{
    const CliShapeTokens *const code = s_room->matcher.tokens;
    const char *rest = template_text;
    for (const char *open = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '$'});
         open != NULL; open = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '$'}))
    {
        cli_line_counted(out, rest, (unsigned long long)(open - rest));
        const char *const after = open + 1;
        const char *const close = cellul.chr(&(CatenaFinitaCfg){.src = after, .cap = CLI_CELLS_POOL, .byte = '$'});
        if (close == NULL)
        {
            cli_line_character(out, '$');
            rest = after;
            continue;
        }
        const char *const colon = memor.chr(&(MemoriaCfg){.src = after, .val = ':', .bytes = (size_t)(close - after)});
        const unsigned long long name_size = (unsigned long long)(((colon != NULL) ? colon : close) - after);
        const unsigned int bound = cli_shape_bound(&s_room->matcher, after, name_size);
        if (bound == CLI_SHAPE_HOLES)
        {
            cli_line_character(out, '$');
            rest = after;
            continue;
        }
        const unsigned int from = s_room->matcher.bound_from[bound];
        unsigned int to = s_room->matcher.bound_to[bound];
        while ((to > from) && (code->kind[to - 1u] == CLI_SHAPE_BREAK))
        {
            to -= 1u;
        }
        if (to > from)
        {
            cli_line_counted(out, s_room->matcher.src + code->start[from], code->end[to - 1u] - code->start[from]);
        }
        rest = close + 1;
    }
    cli_line_text(out, rest);
}

// the search's room and its ends given back, and the lock with it
static void search_released(void)
{
    cli_shape_ends_given(&s_room->matcher);
    cli_cell_given_by(&g_cli_shape_cells, s_room);
    s_room = NULL;
    cli_lock_give(&s_search_lock);
}

// the search's room taken for `code_room` tokens of the code and `part_room` of the pattern, the lock taken with it:
// 1, or 0 with the lock given back where the cellblock could not meet it
static int search_room_taken(unsigned int code_room, unsigned int part_room)
{
    // the starts and ends of both and each part's hole, then the kinds of both
    const unsigned long long words = (2ull * code_room) + (3ull * part_room);
    cli_lock_take(&s_search_lock);
    s_room =
        cli_cell_taken_by(&g_cli_shape_cells, sizeof(*s_room) + (words * sizeof(unsigned int)) + code_room + part_room,
                          "a search by shape");
    if (s_room == NULL)
    {
        cli_lock_give(&s_search_lock);
        return 0;
    }
    // the lists start past the room, on a boundary of the room's pointers, and the kinds past the words
    unsigned int *const listed = (unsigned int *)(void *)(s_room + 1);
    unsigned char *const kinds = (unsigned char *)(void *)(listed + words);
    s_room->code = (CliShapeTokens){kinds, listed, listed + code_room, 0u, code_room, 0};
    s_room->parts = (CliShapeTokens){
        kinds + code_room, listed + (2ull * code_room), listed + (2ull * code_room) + part_room, 0u, part_room, 0};
    s_room->hole_of = listed + (2ull * code_room) + (2ull * part_room);
    s_room->matcher.ends = NULL;
    s_room->matcher.ends_top = 0u;
    s_room->matcher.ends_room = 0u;
    return 1;
}

void cli_shape_search(const char *language, const char *src, unsigned long long size, const char *pattern,
                      const char *template_text, CliShapeEach each, void *context)
{
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(pattern, cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL}), &from, &to);
    // a token takes a byte at the least, so the code holds no more tokens than bytes, nor the pattern; each below its
    // most, which an unsigned int holds
    const unsigned int code_room = (size < CLI_SHAPE_TOKENS) ? ((unsigned int)size + 1u) : CLI_SHAPE_TOKENS;
    const unsigned int part_room =
        ((to - from) < CLI_SHAPE_PATTERN) ? ((unsigned int)(to - from) + 1u) : CLI_SHAPE_PATTERN;
    if (!search_room_taken(code_room, part_room))
    {
        return;
    }
    s_room->matcher.pattern = pattern + from;
    s_room->matcher.part_count = search_pattern(language, pattern + from, to - from);
    s_room->matcher.hole_of = s_room->hole_of;
    s_room->matcher.holes = &s_room->holes;
    if (s_room->matcher.part_count == 0u)
    {
        search_released();
        return;
    }
    cli_shape_tokens(language, src, size, &s_room->code, NULL);
    s_room->matcher.src = src;
    s_room->matcher.tokens = &s_room->code;
    CliShapeWalk walk = {0ull, 0u, 0u, 0ull};
    for (unsigned int at = 0u; at < s_room->code.count;)
    {
        unsigned int end = 0u;
        s_room->matcher.steps = 0u;
        s_room->matcher.bound_count = 0u;
        if ((s_room->code.kind[at] == CLI_SHAPE_BREAK) || !cli_shape_from(&s_room->matcher, 0u, at, &end))
        {
            at += 1u;
            continue;
        }
        while ((end > at) && (s_room->code.kind[end - 1u] == CLI_SHAPE_BREAK))
        {
            end -= 1u;
        }
        if (end == at)
        {
            at += 1u;
            continue;
        }
        CliShapeFound found = {{0u, 0u}, {0u, 0u}, NULL, 0ull, NULL, 0ull, 0};
        found.from = search_walked(src, size, &walk, s_room->code.start[at]);
        found.line = src + walk.line_start;
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = found.line, .val = '\n', .bytes = (size_t)(size - walk.line_start)});
        const unsigned long long line_size =
            (newline != NULL) ? (unsigned long long)(newline - found.line) : size - walk.line_start;
        found.line_size = cli_text_trimmed_end(found.line, line_size);
        found.to = search_walked(src, size, &walk, s_room->code.end[end - 1u]);
        CliLine with = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
        if (template_text != NULL)
        {
            (void)cli_line_growing(&with, CLI_SHAPE_WITH_FIRST, "a match's template written");
            search_filled(template_text, &with);
            found.has_with = cli_line_fits(&with);
            found.with = with.out;
            found.with_size = with.at;
        }
        each(context, &found);
        cli_cell_given(with.out);
        at = end;
    }
    search_released();
}

void cli_shape_found_written(const char *path, const CliShapeFound *found, CliLine *out)
{
    cli_line_text(out, "{\"path\":");
    cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"from\":");
    cli_text_place_written(found->from, out);
    cli_line_text(out, ",\"to\":");
    cli_text_place_written(found->to, out);
    cli_line_text(out, ",\"line\":");
    cli_json_quoted(out, found->line, found->line_size);
    cli_line_text(out, ",\"with\":");
    if (found->has_with)
    {
        cli_json_quoted(out, found->with, found->with_size);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_character(out, '}');
}
