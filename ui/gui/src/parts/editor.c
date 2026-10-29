// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editor.c: the editor part. The lines an answer holds are copied out of its tree into cells of the part's own, so the
// tree goes as the answer is read; a run's column counts the page's units, UTF-16's, as the CLI writes them, so a
// letter past the basic plane counts two
#include "editor.h"

#include "../../../cli/src/text/text.h"
#include "../answer.h"
#include "../frame.h"

#include <math.h>

static CellOwner s_owner = MMGR_CELL_OWNER("gui_editor");

void gui_editor_made(GuiEditor *editor)
{
    memor.set(&(MemoriaCfg){.dst = editor, .val = 0, .bytes = sizeof(*editor)});
    editor->caret_ink = 255u;
}

// the lines stored freed, with the folds that open on them, find's query and the matches
static void editor_lines_given(GuiEditor *editor)
{
    g_cli_cells->persistent_buf_release(editor->line, &s_owner);
    g_cli_cells->persistent_buf_release(editor->bytes, &s_owner);
    g_cli_cells->persistent_buf_release(editor->run, &s_owner);
    g_cli_cells->persistent_buf_release(editor->fold, &s_owner);
    g_cli_cells->persistent_buf_release(editor->closers, &s_owner);
    g_cli_cells->persistent_buf_release(editor->find.query, &s_owner);
    g_cli_cells->persistent_buf_release(editor->match, &s_owner);
    editor->line = NULL;
    editor->bytes = NULL;
    editor->run = NULL;
    editor->fold = NULL;
    editor->closers = NULL;
    editor->match = NULL;
    memor.set(&(MemoriaCfg){.dst = &editor->find, .val = 0, .bytes = sizeof(editor->find)});
    editor->run_total = 0u;
    editor->count = 0u;
    editor->folds = 0u;
    editor->matches = 0ull;
}

void gui_editor_given(GuiEditor *editor)
{
    editor_lines_given(editor);
    g_cli_cells->persistent_buf_release(editor->names, &s_owner);
    g_cli_cells->persistent_buf_release(editor->name_at, &s_owner);
    editor->names = NULL;
    editor->name_at = NULL;
    editor->name_count = 0u;
}

void gui_editor_shaped(GuiEditor *editor, unsigned long long lines)
{
    editor->lines = lines;
    if ((lines != 0ull) && (editor->top >= lines))
    {
        editor->top = lines - 1ull;
    }
}

int gui_editor_named(GuiEditor *editor, const CliJson *json, unsigned int names)
{
    g_cli_cells->persistent_buf_release(editor->names, &s_owner);
    g_cli_cells->persistent_buf_release(editor->name_at, &s_owner);
    editor->names = NULL;
    editor->name_at = NULL;
    editor->name_count = 0u;
    if (!cli_json_is(json, names, CFG_JSON_ARRAY))
    {
        return 0;
    }
    const unsigned int count = json->node[names].count;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        (void)cli_json_string(json, cli_json_item(json, names, at), &length);
        bytes += length + 1ull;
    }
    editor->names = g_cli_cells->persistent_buf_alloc((bytes != 0ull) ? bytes : 1ull, &s_owner);
    editor->name_at = g_cli_cells->persistent_buf_alloc(((count != 0u) ? count : 1u) * sizeof(*editor->name_at), &s_owner);
    if ((editor->names == NULL) || (editor->name_at == NULL))
    {
        gui_editor_given(editor);
        return 0;
    }
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(json, cli_json_item(json, names, at), &length);
        editor->name_at[at] = used;
        memor.cpy(&(MemoriaCfg){.dst = editor->names + used, .src = (text != NULL) ? text : "", .bytes = length});
        editor->names[used + length] = '\0';
        used += length + 1ull;
    }
    editor->name_count = count;
    return 1;
}

// the line the row `at` of the array `lines` is: the item `at` of the array `numbers`, or `from` + `at` where the
// array gives none
static unsigned long long editor_number_of(const CliJson *json, unsigned int numbers, unsigned long long from,
                                           unsigned int at)
{
    const unsigned int node = cli_json_is(json, numbers, CFG_JSON_ARRAY) ? cli_json_item(json, numbers, at) : CLI_JSON_NONE;
    return (node != CLI_JSON_NONE) ? gui_answer_whole(json, node, from + at) : from + at;
}

// a line's runs array node of `colors`, each `{"from", "runs"}`, CLI_JSON_NONE where none colors the line; and the
// runs counted for the lines numbered as `numbers` gives the `count` rows from `from`: how many pairs they hold in all
static unsigned int editor_runs_of(const CliJson *json, unsigned int colors, unsigned long long line);

static unsigned int editor_runs_counted(const CliJson *json, unsigned int colors, unsigned int numbers,
                                        unsigned long long from, unsigned int count)
{
    unsigned int total = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int flat = editor_runs_of(json, colors, editor_number_of(json, numbers, from, at));
        total += cli_json_is(json, flat, CFG_JSON_ARRAY) ? (json->node[flat].count / 2u) : 0u;
    }
    return total;
}

static unsigned int editor_runs_of(const CliJson *json, unsigned int colors, unsigned long long line)
{
    const unsigned int ranges = cli_json_is(json, colors, CFG_JSON_ARRAY) ? json->node[colors].count : 0u;
    unsigned int found = CLI_JSON_NONE;
    for (unsigned int range = 0u; range < ranges; range += 1u)
    {
        const unsigned int one = cli_json_item(json, colors, range);
        const unsigned long long start = gui_answer_member_whole(json, one, "from", 0ull);
        const unsigned int runs = cli_json_member(json, one, "runs");
        const unsigned int lines = cli_json_is(json, runs, CFG_JSON_ARRAY) ? json->node[runs].count : 0u;
        if ((line >= start) && (line < start + lines))
        {
            // the last range to color a line wins
            found = cli_json_item(json, runs, (unsigned int)(line - start));
        }
    }
    return found;
}

// the pairs of the flat runs array `flat` put after `used` of `run`: how many were put
static unsigned int editor_runs_put(const CliJson *json, unsigned int flat, unsigned int *run, unsigned int used)
{
    const unsigned int values = cli_json_is(json, flat, CFG_JSON_ARRAY) ? json->node[flat].count : 0u;
    unsigned int put = 0u;
    for (unsigned int at = 0u; (at + 1u) < values; at += 2u)
    {
        // a column and a name's index are far inside what an unsigned int represents
        run[used + (2u * put)] = (unsigned int)gui_answer_whole(json, cli_json_item(json, flat, at), 0ull);
        run[used + (2u * put) + 1u] = (unsigned int)gui_answer_whole(json, cli_json_item(json, flat, at + 1u), 0ull);
        put += 1u;
    }
    return put;
}

// the strings of the array `lines` kept as the lines numbered as the array `numbers` gives them, from `from` where it
// gives none, each with its runs from `colors`
static int editor_array_put(GuiEditor *editor, const CliJson *json, unsigned int lines, unsigned int colors,
                            unsigned int numbers, unsigned long long from)
{
    editor_lines_given(editor);
    if (!cli_json_is(json, lines, CFG_JSON_ARRAY))
    {
        return 0;
    }
    const unsigned int count = json->node[lines].count;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        (void)cli_json_string(json, cli_json_item(json, lines, at), &length);
        bytes += length;
    }
    const unsigned int pairs = editor_runs_counted(json, colors, numbers, from, count);
    editor->line = g_cli_cells->persistent_buf_alloc(((count != 0u) ? count : 1u) * sizeof(*editor->line), &s_owner);
    editor->bytes = g_cli_cells->persistent_buf_alloc((bytes != 0ull) ? bytes : 1ull, &s_owner);
    editor->run = g_cli_cells->persistent_buf_alloc(((pairs != 0u) ? pairs : 1u) * 2u * sizeof(*editor->run), &s_owner);
    if ((editor->line == NULL) || (editor->bytes == NULL) || (editor->run == NULL))
    {
        editor_lines_given(editor);
        return 0;
    }
    unsigned long long used = 0ull;
    unsigned int runs_used = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(json, cli_json_item(json, lines, at), &length);
        GuiLine *const line = &editor->line[at];
        line->number = editor_number_of(json, numbers, from, at);
        line->text = used;
        line->length = length;
        memor.cpy(&(MemoriaCfg){.dst = editor->bytes + used, .src = (text != NULL) ? text : "", .bytes = length});
        used += length;
        line->runs = runs_used;
        line->run_count = editor_runs_put(json, editor_runs_of(json, colors, line->number), editor->run, runs_used);
        runs_used += 2u * line->run_count;
    }
    editor->from = (count != 0u) ? editor->line[0].number : from;
    editor->count = count;
    editor->run_total = runs_used;
    return 1;
}

int gui_editor_lines_put(GuiEditor *editor, const CliJson *json, unsigned int read, unsigned long long from)
{
    return editor_array_put(editor, json, cli_json_member(json, read, "lines"), cli_json_member(json, read, "colors"),
                            CLI_JSON_NONE, from);
}

// the folds of the array `folds`, each `[first, last, shown, closed, closer]`, kept with their closers' texts: 1, or 0
// where a cell could not be taken, the part then holding none
static int editor_folds_put(GuiEditor *editor, const CliJson *json, unsigned int folds)
{
    const unsigned int count = cli_json_is(json, folds, CFG_JSON_ARRAY) ? json->node[folds].count : 0u;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        (void)cli_json_string(json, cli_json_item(json, cli_json_item(json, folds, at), 4u), &length);
        bytes += length;
    }
    editor->fold = g_cli_cells->persistent_buf_alloc(((count != 0u) ? count : 1u) * sizeof(*editor->fold), &s_owner);
    editor->closers = g_cli_cells->persistent_buf_alloc((bytes != 0ull) ? bytes : 1ull, &s_owner);
    if ((editor->fold == NULL) || (editor->closers == NULL))
    {
        g_cli_cells->persistent_buf_release(editor->fold, &s_owner);
        g_cli_cells->persistent_buf_release(editor->closers, &s_owner);
        editor->fold = NULL;
        editor->closers = NULL;
        return 0;
    }
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int one = cli_json_item(json, folds, at);
        unsigned long long length = 0ull;
        const char *const closer = cli_json_string(json, cli_json_item(json, one, 4u), &length);
        GuiFold *const fold = &editor->fold[at];
        fold->first = gui_answer_whole(json, cli_json_item(json, one, 0u), 0ull);
        fold->last = gui_answer_whole(json, cli_json_item(json, one, 1u), 0ull);
        fold->shown = gui_answer_whole(json, cli_json_item(json, one, 2u), 0ull) != 0ull;
        fold->closed = gui_answer_whole(json, cli_json_item(json, one, 3u), 0ull) != 0ull;
        fold->closer = used;
        fold->closer_length = length;
        memor.cpy(&(MemoriaCfg){.dst = editor->closers + used, .src = (closer != NULL) ? closer : "", .bytes = length});
        used += length;
    }
    editor->folds = count;
    return 1;
}

// a place `{"line", "col"}` read
static GuiPlace editor_place_of(const CliJson *json, unsigned int node)
{
    const GuiPlace place = {gui_answer_member_whole(json, node, "line", 0ull), gui_answer_member_whole(json, node, "col", 0ull)};
    return place;
}

// find's object `find`, null where its bar is closed, and the array `matches` of `[line, from, to]` kept: 1, or 0
// where a cell could not be taken
static int editor_find_put(GuiEditor *editor, const CliJson *json, unsigned int find, unsigned int matches)
{
    editor->find.open = cli_json_is(json, find, CFG_JSON_OBJECT);
    editor->find.current = -1ll;
    if (!editor->find.open)
    {
        return 1;
    }
    unsigned long long length = 0ull;
    const char *const query = cli_json_string(json, cli_json_member(json, find, "query"), &length);
    editor->find.query = g_cli_cells->persistent_buf_alloc((length != 0ull) ? length : 1ull, &s_owner);
    const unsigned long long count = cli_json_is(json, matches, CFG_JSON_ARRAY) ? json->node[matches].count : 0u;
    editor->match = g_cli_cells->persistent_buf_alloc(((count != 0ull) ? count : 1ull) * sizeof(*editor->match), &s_owner);
    if ((editor->find.query == NULL) || (editor->match == NULL))
    {
        g_cli_cells->persistent_buf_release(editor->find.query, &s_owner);
        g_cli_cells->persistent_buf_release(editor->match, &s_owner);
        editor->find.query = NULL;
        editor->match = NULL;
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = editor->find.query, .src = (query != NULL) ? query : "", .bytes = length});
    editor->find.query_length = length;
    editor->find.focus = gui_answer_member_true(json, find, "focus") || (gui_answer_member_whole(json, find, "focus", 0ull) != 0ull);
    editor->find.at = gui_answer_member_whole(json, find, "at", 0ull);
    editor->find.regex = gui_answer_member_whole(json, find, "regex", 0ull) != 0ull;
    editor->find.match_case = gui_answer_member_whole(json, find, "case", 0ull) != 0ull;
    editor->find.word = gui_answer_member_whole(json, find, "word", 0ull) != 0ull;
    editor->find.count = gui_answer_member_whole(json, find, "count", 0ull);
    const unsigned int current = cli_json_member(json, find, "current");
    // the current match is counted among the matches, which a long long counts
    editor->find.current = cli_json_is(json, current, CFG_JSON_NUMBER) ? (long long)gui_answer_whole(json, current, 0ull) : -1ll;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        // the matches among the rows are far inside what an unsigned int represents
        const unsigned int one = cli_json_item(json, matches, (unsigned int)at);
        editor->match[at][0] = gui_answer_whole(json, cli_json_item(json, one, 0u), 0ull);
        editor->match[at][1] = gui_answer_whole(json, cli_json_item(json, one, 1u), 0ull);
        editor->match[at][2] = gui_answer_whole(json, cli_json_item(json, one, 2u), 0ull);
    }
    editor->matches = count;
    return 1;
}

int gui_editor_frame_put(GuiEditor *editor, const CliJson *json, unsigned int frame)
{
    const unsigned long long from = gui_answer_member_whole(json, frame, "from", 0ull);
    if (!editor_array_put(editor, json, cli_json_member(json, frame, "rows"), cli_json_member(json, frame, "colors"),
                          cli_json_member(json, frame, "numbers"), from) ||
        !editor_folds_put(editor, json, cli_json_member(json, frame, "folds")) ||
        !editor_find_put(editor, json, cli_json_member(json, frame, "find"), cli_json_member(json, frame, "matches")))
    {
        return 0;
    }
    editor->top = gui_answer_member_whole(json, frame, "top", 0ull);
    editor->lines = gui_answer_member_whole(json, frame, "lines", 0ull);
    // the indentation's tabs come as 1, 0 or null, null and a frame without one read as nothing set
    const unsigned int indent = cli_json_member(json, frame, "indent");
    const unsigned long long tabs = gui_answer_member_whole(json, indent, "tabs", 2ull);
    editor->indent_tabs = (tabs == 1ull) ? 1 : ((tabs == 0ull) ? 0 : -1);
    // a step is far inside what an unsigned int represents
    editor->indent_size = (unsigned int)gui_answer_member_whole(json, indent, "size", 4ull);
    editor->cursor = editor_place_of(json, cli_json_member(json, frame, "cursor"));
    editor->anchor = editor_place_of(json, cli_json_member(json, frame, "anchor"));
    editor->has_cursor = cli_json_member(json, frame, "cursor") != CLI_JSON_NONE;
    const unsigned int pair = cli_json_member(json, frame, "pair");
    editor->has_pair = cli_json_is(json, pair, CFG_JSON_ARRAY) && (json->node[pair].count == 2u);
    editor->pair[0] = editor_place_of(json, cli_json_item(json, pair, 0u));
    editor->pair[1] = editor_place_of(json, cli_json_item(json, pair, 1u));
    return 1;
}

int gui_editor_colored(GuiEditor *editor, const CliJson *json, unsigned int colors)
{
    if (editor->count == 0u)
    {
        return 1;
    }
    // the runs made anew: each line's from the colors where they color it, else as it keeps them
    unsigned int pairs = 0u;
    for (unsigned int at = 0u; at < editor->count; at += 1u)
    {
        const unsigned int flat = editor_runs_of(json, colors, editor->line[at].number);
        pairs += (flat != CLI_JSON_NONE) ? (cli_json_is(json, flat, CFG_JSON_ARRAY) ? json->node[flat].count / 2u : 0u)
                                         : editor->line[at].run_count;
    }
    unsigned int *const run = g_cli_cells->persistent_buf_alloc(((pairs != 0u) ? pairs : 1u) * 2u * sizeof(*run), &s_owner);
    if (run == NULL)
    {
        return 0;
    }
    unsigned int used = 0u;
    for (unsigned int at = 0u; at < editor->count; at += 1u)
    {
        GuiLine *const line = &editor->line[at];
        const unsigned int flat = editor_runs_of(json, colors, line->number);
        const unsigned int start = used;
        if (flat != CLI_JSON_NONE)
        {
            line->run_count = editor_runs_put(json, flat, run, used);
        }
        else
        {
            memor.cpy(&(MemoriaCfg){
                .dst = run + used, .src = editor->run + line->runs, .bytes = 2ull * line->run_count * sizeof(*run)});
        }
        line->runs = start;
        used += 2u * line->run_count;
    }
    g_cli_cells->persistent_buf_release(editor->run, &s_owner);
    editor->run = run;
    editor->run_total = used;
    return 1;
}

int gui_editor_text_put(GuiEditor *editor, const char *text, unsigned long long length)
{
    editor_lines_given(editor);
    unsigned int count = 1u;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        count += (text[at] == '\n') ? 1u : 0u;
    }
    editor->line = g_cli_cells->persistent_buf_alloc(count * sizeof(*editor->line), &s_owner);
    editor->bytes = g_cli_cells->persistent_buf_alloc((length != 0ull) ? length : 1ull, &s_owner);
    editor->run = g_cli_cells->persistent_buf_alloc(2u * sizeof(*editor->run), &s_owner);
    if ((editor->line == NULL) || (editor->bytes == NULL) || (editor->run == NULL))
    {
        editor_lines_given(editor);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = editor->bytes, .src = text, .bytes = length});
    unsigned int line = 0u;
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; at <= length; at += 1ull)
    {
        if ((at == length) || (text[at] == '\n'))
        {
            const unsigned long long end = ((at > start) && (text[at - 1ull] == '\r')) ? at - 1ull : at;
            editor->line[line] = (GuiLine){line, start, end - start, 0u, 0u};
            line += 1u;
            start = at + 1ull;
        }
    }
    editor->from = 0ull;
    editor->count = count;
    editor->run_total = 0u;
    editor->lines = count;
    return 1;
}

void gui_editor_scrolled(GuiEditor *editor, long long delta, unsigned int rows)
{
    (void)rows;
    const unsigned long long last = (editor->lines != 0ull) ? editor->lines - 1ull : 0ull;
    if (delta < 0ll)
    {
        // a move up is clamped to the top: it is the count of lines above the top
        const unsigned long long up = (unsigned long long)(-delta);
        editor->top = (up > editor->top) ? 0ull : editor->top - up;
        return;
    }
    // a move down, which is not negative here
    const unsigned long long down = (unsigned long long)delta;
    editor->top = ((editor->top + down) > last) ? last : editor->top + down;
}

// how many digits the file's line count takes, three at the least, as the page gives its gutter room
static unsigned int editor_digits(unsigned long long lines)
{
    unsigned int digits = 1u;
    for (unsigned long long left = lines; left >= 10ull; left /= 10ull)
    {
        digits += 1u;
    }
    return (digits < 3u) ? 3u : digits;
}

int gui_editor_gutter(const GuiEditor *editor, float cell, float scale)
{
    const float numbers = (float)editor_digits(editor->lines) * cell;
    return (int)(numbers + 0.5f) + (int)(((float)(GUI_GUTTER_BEFORE + GUI_GUTTER_AFTER) * scale) + 0.5f);
}

// the cell column a line's unit column stands at, tabs laid to their stops, a letter past the basic plane counting
// two units; and the line's cells in all where `col` is past its end
static unsigned long long editor_cell_of(const GuiEditor *editor, const GuiLine *line, unsigned long long col)
{
    const char *const text = editor->bytes + line->text;
    unsigned long long at = 0ull;
    unsigned long long units = 0ull;
    unsigned long long cell = 0ull;
    while ((at < line->length) && (units < col))
    {
        unsigned long long took = 0ull;
        const unsigned int letter = cli_text_letter(text, line->length, at, &took);
        at += (took != 0ull) ? took : 1ull;
        units += (letter >= 0x10000u) ? 2ull : 1ull;
        cell += (letter == '\t') ? (GUI_TAB_COLUMNS - (cell % GUI_TAB_COLUMNS)) : 1ull;
    }
    return cell;
}

int gui_editor_cell_at(const GuiEditor *editor, GuiRect place, float cell, float scale, int x, int y,
                       unsigned long long *row, unsigned long long *column)
{
    const int line_tall = (int)(((float)GUI_CODE_LINE * scale) + 0.5f);
    const int text_x = place.x + gui_editor_gutter(editor, cell, scale);
    if ((line_tall <= 0) || (cell <= 0.0f) || (y < place.y) || (y >= place.y + place.height) || (x < text_x) ||
        (x >= place.x + place.width))
    {
        return 0;
    }
    // the row and the column are not negative, the pixel standing in the text
    *row = (unsigned long long)((y - place.y) / line_tall);
    *column = (unsigned long long)(((float)(x - text_x) / cell) + 0.5f);
    return 1;
}

int gui_editor_row_at(const GuiEditor *editor, GuiRect place, float cell, float scale, int x, int y,
                      unsigned long long *row)
{
    const int line_tall = (int)(((float)GUI_CODE_LINE * scale) + 0.5f);
    const int text_x = place.x + gui_editor_gutter(editor, cell, scale);
    if ((line_tall <= 0) || (y < place.y) || (y >= place.y + place.height) || (x < place.x) || (x >= text_x))
    {
        return 0;
    }
    // the row is not negative, the pixel standing in the gutter
    *row = (unsigned long long)((y - place.y) / line_tall);
    return 1;
}

// the byte of the stored line `held` where its unit `col` starts, the line's length where the column is past it
static unsigned long long editor_byte_of(const GuiEditor *editor, const GuiLine *held, unsigned long long col)
{
    const char *const text = editor->bytes + held->text;
    unsigned long long at = 0ull;
    unsigned long long units = 0ull;
    while ((at < held->length) && (units < col))
    {
        unsigned long long took = 0ull;
        const unsigned int letter = cli_text_letter(text, held->length, at, &took);
        at += (took != 0ull) ? took : 1ull;
        units += (letter >= 0x10000u) ? 2ull : 1ull;
    }
    return at;
}

int gui_editor_units_text(const GuiEditor *editor, unsigned long long line, unsigned long long from, unsigned long long to,
                          const char **text, unsigned long long *length)
{
    for (unsigned int at = 0u; at < editor->count; at += 1u)
    {
        const GuiLine *const held = &editor->line[at];
        if (held->number == line)
        {
            const unsigned long long start = editor_byte_of(editor, held, from);
            const unsigned long long end = editor_byte_of(editor, held, (to > from) ? to : from);
            *text = editor->bytes + held->text + start;
            *length = end - start;
            return 1;
        }
    }
    return 0;
}

int gui_editor_place_rect(const GuiEditor *editor, GuiRect place, float cell, float scale, unsigned long long line,
                          unsigned long long col, GuiRect *rect)
{
    const int line_tall = (int)(((float)GUI_CODE_LINE * scale) + 0.5f);
    const float text_x = (float)(place.x + gui_editor_gutter(editor, cell, scale));
    unsigned int start = 0u;
    while ((start < editor->count) && (editor->line[start].number < editor->top))
    {
        start += 1u;
    }
    for (unsigned int at = start; at < editor->count; at += 1u)
    {
        if (editor->line[at].number == line)
        {
            // the row's count is far inside what an int represents
            const int row = (int)(at - start);
            const int y = place.y + (row * line_tall);
            if (y + line_tall > place.y + place.height)
            {
                return 0;
            }
            const int x = (int)roundf(text_x + ((float)editor_cell_of(editor, &editor->line[at], col) * cell));
            *rect = (GuiRect){x, y, (int)(cell + 0.5f), line_tall};
            return 1;
        }
    }
    return 0;
}

// the fold that opens on `line`, or NULL
static const GuiFold *editor_fold_on(const GuiEditor *editor, unsigned long long line)
{
    for (unsigned int at = 0u; at < editor->folds; at += 1u)
    {
        if (editor->fold[at].first == line)
        {
            return &editor->fold[at];
        }
    }
    return NULL;
}

// the gutter's mark of a fold closed: a triangle pointing right, `tall` pixels tall, its point at the right, drawn
// with its left side at `x` and its middle on `middle`
static void editor_fold_marked(GuiRaster *raster, int x, int middle, int tall, GuiColor color, GuiRect clip)
{
    const int half = tall / 2;
    for (int step = 0; step <= half; step += 1)
    {
        const int wide = half - step + 1;
        gui_raster_filled(raster, gui_rect_met((GuiRect){x, middle - step, wide, 1}, clip), color);
        gui_raster_filled(raster, gui_rect_met((GuiRect){x, middle + step, wide, 1}, clip), color);
    }
}

// the folded mark after a line that opens a fold closed, from `x`: a rounded ground two cells wide holding three
// dots, then the closer's text half a cell after it; how far it reaches
static float editor_folded_drawn(const GuiEditor *editor, const GuiFold *fold, GuiRaster *raster, GuiGlyphs *glyphs,
                                 const GuiPalette *palette, float x, int top, int baseline, int line_tall, float cell,
                                 unsigned int size, float scale, GuiRect clip)
{
    const int wide = (int)roundf(2.0f * cell);
    const int inset = gui_scaled(3, scale);
    const GuiRect box = {(int)roundf(x), top + inset, wide, line_tall - (2 * inset)};
    const GuiRect met = gui_rect_met(box, clip);
    if (!gui_rect_empty(met))
    {
        gui_raster_rounded(raster, met, (unsigned int)inset, palette->same);
    }
    const int dot = gui_scaled(2, scale);
    const int middle = top + (line_tall / 2);
    for (int at = 0; at < 3; at += 1)
    {
        const int dot_x = box.x + ((wide * (at + 1)) / 4) - (dot / 2);
        gui_raster_filled(raster, gui_rect_met((GuiRect){dot_x, middle - (dot / 2), dot, dot}, clip), palette->text_2);
    }
    float reach = x + (float)wide + (cell / 2.0f);
    if (fold->shown && (fold->closer_length != 0ull))
    {
        const char *const closer = editor->closers + fold->closer;
        gui_text_drawn(raster, glyphs, GUI_FACE_CODE, size, reach, baseline, closer, fold->closer_length,
                       palette->editor_text, clip);
        reach += gui_text_width(glyphs, GUI_FACE_CODE, size, closer, fold->closer_length);
    }
    return reach;
}

GuiColor gui_editor_color(const GuiEditor *editor, const GuiPalette *palette, unsigned int name)
{
    if (name >= editor->name_count)
    {
        return palette->editor_text;
    }
    const char *const text = editor->names + editor->name_at[name];
    unsigned long long length = 0ull;
    while (text[length] != '\0')
    {
        length += 1ull;
    }
    return gui_palette_token(palette, text, length);
}

// one line's text drawn from `x` on the baseline `y`, each letter in its run's color, a tab laid out to its stop
static void editor_line_drawn(const GuiEditor *editor, const GuiLine *line, GuiRaster *raster, GuiGlyphs *glyphs,
                              const GuiPalette *palette, float x, int y, float cell, unsigned int size, GuiRect clip)
{
    const char *const text = editor->bytes + line->text;
    const unsigned int *const run = editor->run + line->runs;
    unsigned long long at = 0ull;
    unsigned long long units = 0ull;
    unsigned long long column = 0ull;
    unsigned int next = 0u;
    GuiColor color = palette->editor_text;
    const float start = x;
    while (at < line->length)
    {
        while ((next < line->run_count) && (units >= run[2u * next]))
        {
            color = gui_editor_color(editor, palette, run[(2u * next) + 1u]);
            next += 1u;
        }
        unsigned long long took = 0ull;
        const unsigned int letter = cli_text_letter(text, line->length, at, &took);
        at += (took != 0ull) ? took : 1ull;
        units += (letter >= 0x10000u) ? 2ull : 1ull;
        if (letter == '\t')
        {
            column += GUI_TAB_COLUMNS - (column % GUI_TAB_COLUMNS);
        }
        else
        {
            float advance = 0.0f;
            const GuiMask *const mask = gui_glyph(glyphs, GUI_FACE_CODE, letter, size, &advance);
            if ((mask != NULL) && (mask->cover != NULL))
            {
                const float here = start + ((float)column * cell);
                // a pixel's place, rounded, is far inside what an int represents
                gui_raster_masked(raster, mask, (int)roundf(here) + mask->left, y - mask->top, color, clip);
            }
            column += 1ull;
        }
        if (start + ((float)column * cell) > (float)(clip.x + clip.width))
        {
            return;
        }
    }
}

// a box drawn around `rect` on `raster`, its lines `thick` pixels wide inside the rect, in `color`, within `clip`
static void editor_box_drawn(GuiRaster *raster, GuiRect rect, int thick, GuiColor color, GuiRect clip)
{
    gui_raster_filled(raster, gui_rect_met((GuiRect){rect.x, rect.y, rect.width, thick}, clip), color);
    gui_raster_filled(raster, gui_rect_met((GuiRect){rect.x, rect.y + rect.height - thick, rect.width, thick}, clip), color);
    gui_raster_filled(raster, gui_rect_met((GuiRect){rect.x, rect.y, thick, rect.height}, clip), color);
    gui_raster_filled(raster, gui_rect_met((GuiRect){rect.x + rect.width - thick, rect.y, thick, rect.height}, clip), color);
}

// the matches on the line `line` drawn on the row `row_rect`, each cell they span in the match ground, the current one
// in the stronger ground and boxed where `boxed` is 1
static void editor_matches_drawn(const GuiEditor *editor, const GuiLine *held, unsigned long long line, GuiRaster *raster,
                                 const GuiPalette *palette, GuiRect row_rect, float text_x, float cell, int boxed,
                                 float scale, GuiRect clip)
{
    for (unsigned long long at = 0ull; at < editor->matches; at += 1ull)
    {
        if (editor->match[at][0] != line)
        {
            continue;
        }
        const int current = (editor->find.current >= 0ll) && ((unsigned long long)editor->find.current == at);
        const int left = (int)roundf(text_x + ((float)editor_cell_of(editor, held, editor->match[at][1]) * cell));
        const int right = (int)roundf(text_x + ((float)editor_cell_of(editor, held, editor->match[at][2]) * cell));
        const GuiRect span = {left, row_rect.y, right - left, row_rect.height};
        if (boxed)
        {
            if (current)
            {
                editor_box_drawn(raster, span, gui_scaled(1, scale), palette->signal, clip);
            }
        }
        else
        {
            gui_raster_filled(raster, gui_rect_met(span, clip), current ? palette->match_on : palette->match);
        }
    }
}

// find's bar drawn at the top right of `place`: its field with the query, or "Find" dim where the query is empty, the
// caret where the keys are its own, the toggles for case, whole words and a pattern, each lit where it is on, and the
// count, "No results" where there is none
static void editor_find_drawn(const GuiEditor *editor, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette,
                              GuiRect place, float scale, unsigned int size, int line_tall)
{
    const int pad = gui_scaled(GUI_FIND_PAD, scale);
    const int gap = gui_scaled(4, scale);
    const int field_wide = gui_scaled(GUI_FIND_FIELD, scale);
    const int toggle_wide = gui_scaled(GUI_FIND_TOGGLE, scale);
    const int count_wide = gui_scaled(GUI_FIND_COUNT, scale);
    const int field_tall = line_tall + gui_scaled(4, scale);
    const int wide = pad + field_wide + gap + (3 * (toggle_wide + gap)) + count_wide + pad;
    const GuiRect bar = {place.x + place.width - gui_scaled(GUI_FIND_RIGHT, scale) - wide, place.y, wide,
                         field_tall + (2 * pad)};
    const GuiRect met = gui_rect_met(bar, place);
    if (gui_rect_empty(met))
    {
        return;
    }
    gui_raster_rounded(raster, met, (unsigned int)gui_scaled(4, scale), palette->surface);
    editor_box_drawn(raster, bar, gui_scaled(1, scale), palette->widget_line, place);
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_HEAD, size);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_HEAD, size);
    const GuiRect field = {bar.x + pad, bar.y + pad, field_wide, field_tall};
    const int baseline = field.y + (int)((((float)field_tall - ascent - descent) / 2.0f) + ascent + 0.5f);
    gui_raster_filled(raster, gui_rect_met(field, place), palette->editor_ground);
    editor_box_drawn(raster, field, gui_scaled(1, scale), editor->find.focus ? palette->hover : palette->line, place);
    const float text_x = (float)(field.x + gui_scaled(7, scale));
    const GuiRect field_clip = gui_rect_met(field, place);
    if (editor->find.query_length == 0ull)
    {
        gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, text_x, baseline, "Find", 4ull, palette->text_3, field_clip);
    }
    else
    {
        gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, text_x, baseline, editor->find.query, editor->find.query_length,
                       palette->editor_text, field_clip);
    }
    if (editor->find.focus)
    {
        // the caret after as many letters as the cursor's units count, a letter past the basic plane two of them
        unsigned long long bytes = 0ull;
        unsigned long long units = 0ull;
        while ((bytes < editor->find.query_length) && (units < editor->find.at))
        {
            unsigned long long took = 0ull;
            const unsigned int letter = cli_text_letter(editor->find.query, editor->find.query_length, bytes, &took);
            bytes += (took != 0ull) ? took : 1ull;
            units += (letter >= 0x10000u) ? 2ull : 1ull;
        }
        const float before = gui_text_width(glyphs, GUI_FACE_HEAD, size, editor->find.query, bytes);
        gui_raster_filled(raster,
                          gui_rect_met((GuiRect){(int)roundf(text_x + before), field.y + gui_scaled(3, scale), gui_scaled(1, scale),
                                                 field_tall - gui_scaled(6, scale)},
                                       place),
                          palette->caret);
    }
    static const char *const s_toggle[3] = {"Aa", "ab", ".*"};
    const int on[3] = {editor->find.match_case, editor->find.word, editor->find.regex};
    int x = field.x + field_wide + gap;
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const GuiRect toggle = {x, field.y, toggle_wide, field_tall};
        if (on[at])
        {
            gui_raster_rounded(raster, gui_rect_met(toggle, place), (unsigned int)gui_scaled(4, scale), palette->selection);
            editor_box_drawn(raster, toggle, gui_scaled(1, scale), palette->hover, place);
        }
        const float label_wide = gui_text_width(glyphs, GUI_FACE_HEAD, size, s_toggle[at], 2ull);
        gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, (float)toggle.x + (((float)toggle_wide - label_wide) / 2.0f),
                       baseline, s_toggle[at], 2ull, on[at] ? palette->editor_text : palette->text_2, place);
        x += toggle_wide + gap;
    }
    char count_text[64];
    VerbaLinea count = {count_text, sizeof(count_text), 0ull};
    if (editor->find.count == 0ull)
    {
        mmgr_line_put(&count, "No results");
    }
    else
    {
        // the current match is counted among the matches
        mmgr_line_decimal(&count, (editor->find.current >= 0ll) ? (unsigned long long)editor->find.current + 1ull : 0ull, 1u);
        mmgr_line_put(&count, " of ");
        mmgr_line_decimal(&count, editor->find.count, 1u);
    }
    gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, (float)(x + gui_scaled(4, scale)), baseline, count_text, count.at,
                   palette->text_2, place);
}

void gui_editor_drawn(GuiEditor *editor, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette,
                      GuiRect place, float scale)
{
    if (gui_rect_empty(place))
    {
        return;
    }
    gui_raster_filled(raster, place, palette->editor_ground);
    // the code's size and its line in pixels at the scale, the size a whole pixel
    const unsigned int size = (unsigned int)(((float)GUI_CODE_SIZE * scale) + 0.5f);
    const int line_tall = (int)(((float)GUI_CODE_LINE * scale) + 0.5f);
    const float cell = gui_glyphs_cell(glyphs, size);
    const int gutter = gui_editor_gutter(editor, cell, scale);
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_CODE, size);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_CODE, size);
    // the baseline inside a row: the row's height less the face's, halved, then the ascent down
    const int baseline = (int)((((float)line_tall - ascent - descent) / 2.0f) + ascent + 0.5f);
    const int numbers_right = place.x + gutter - (int)(((float)GUI_GUTTER_AFTER * scale) + 0.5f);
    const float text_x = (float)(place.x + gutter);
    const GuiRect text_clip = gui_rect_met(place, (GuiRect){place.x + gutter, place.y, place.width - gutter, place.height});
    const int rows = (line_tall > 0) ? (place.height + line_tall - 1) / line_tall : 0;
    // the selection's ends in order
    const int anchored = (editor->anchor.line < editor->cursor.line) ||
                         ((editor->anchor.line == editor->cursor.line) && (editor->anchor.col < editor->cursor.col));
    const GuiPlace selected_from = anchored ? editor->anchor : editor->cursor;
    const GuiPlace selected_to = anchored ? editor->cursor : editor->anchor;
    const int selected = editor->has_cursor && ((selected_from.line != selected_to.line) || (selected_from.col != selected_to.col));
    // the first row drawn is the first line stored at or below the view's top
    unsigned int start = 0u;
    while ((start < editor->count) && (editor->line[start].number < editor->top))
    {
        start += 1u;
    }
    for (int row = 0; row < rows; row += 1)
    {
        // a row's count is not negative
        const unsigned int at = start + (unsigned int)row;
        if ((at >= editor->count) || (editor->line[at].number >= editor->lines))
        {
            break;
        }
        const unsigned long long line = editor->line[at].number;
        const int y = place.y + (row * line_tall) + baseline;
        const GuiRect row_rect = {text_clip.x, place.y + (row * line_tall), text_clip.width, line_tall};
        const GuiLine *const held = &editor->line[at];
        const GuiFold *const fold = editor_fold_on(editor, line);
        // the current line's ground, the selection's span on the line, and the caret
        if (editor->has_cursor && (line == editor->cursor.line) && !selected)
        {
            gui_raster_filled(raster, gui_rect_met(row_rect, place), palette->current);
        }
        if (selected && (line >= selected_from.line) && (line <= selected_to.line))
        {
            const unsigned long long start_cell = (line == selected_from.line) ? editor_cell_of(editor, held, selected_from.col) : 0ull;
            const unsigned long long end_cell = (line == selected_to.line) ? editor_cell_of(editor, held, selected_to.col)
                                                                            : editor_cell_of(editor, held, ~0ull) + 1ull;
            const int left = (int)roundf(text_x + ((float)start_cell * cell));
            const int right = (int)roundf(text_x + ((float)end_cell * cell));
            gui_raster_filled(raster, gui_rect_met((GuiRect){left, row_rect.y, right - left, line_tall}, text_clip), palette->selection);
        }
        if (editor->find.open)
        {
            editor_matches_drawn(editor, held, line, raster, palette, row_rect, text_x, cell, 0, scale, text_clip);
        }
        char digits[24];
        VerbaLinea number = {digits, sizeof(digits), 0ull};
        mmgr_line_decimal(&number, line + 1ull, 1u);
        const float number_wide = gui_text_width(glyphs, GUI_FACE_CODE, size, digits, number.at);
        gui_text_drawn(raster, glyphs, GUI_FACE_CODE, size, (float)numbers_right - number_wide, y, digits, number.at,
                       palette->number, place);
        if ((fold != NULL) && fold->closed)
        {
            editor_fold_marked(raster, numbers_right + gui_scaled(4, scale), row_rect.y + (line_tall / 2),
                               gui_scaled(7, scale), palette->number, place);
        }
        editor_line_drawn(editor, held, raster, glyphs, palette, text_x, y, cell, size, text_clip);
        if ((fold != NULL) && fold->closed)
        {
            const float after = text_x + ((float)(editor_cell_of(editor, held, ~0ull) + 1ull) * cell);
            (void)editor_folded_drawn(editor, fold, raster, glyphs, palette, after, row_rect.y, y, line_tall, cell, size,
                                      scale, text_clip);
        }
        for (unsigned int each = 0u; editor->has_pair && (each < 2u); each += 1u)
        {
            if (editor->pair[each].line == line)
            {
                const int left = (int)roundf(text_x + ((float)editor_cell_of(editor, held, editor->pair[each].col) * cell));
                const int right = (int)roundf(text_x + ((float)(editor_cell_of(editor, held, editor->pair[each].col) + 1ull) * cell));
                editor_box_drawn(raster, (GuiRect){left, row_rect.y, right - left, line_tall}, gui_scaled(1, scale),
                                 palette->caret, text_clip);
            }
        }
        if (editor->find.open)
        {
            editor_matches_drawn(editor, held, line, raster, palette, row_rect, text_x, cell, 1, scale, text_clip);
        }
        if (editor->has_cursor && (editor->caret_ink != 0u) && (line == editor->cursor.line) && !(editor->find.open && editor->find.focus))
        {
            const int caret_x = (int)roundf(text_x + ((float)editor_cell_of(editor, held, editor->cursor.col) * cell));
            // the caret's color at the ink's share of its alpha: a byte's share of a byte, rounded, is a byte
            const GuiColor inked = GUI_RGBA(GUI_RED_OF(palette->caret), GUI_GREEN_OF(palette->caret), GUI_BLUE_OF(palette->caret),
                                            ((GUI_ALPHA_OF(palette->caret) * editor->caret_ink) + 127u) / 255u);
            gui_raster_filled(raster, gui_rect_met((GuiRect){caret_x, row_rect.y, gui_scaled(2, scale), line_tall}, text_clip),
                              inked);
        }
    }
    if (editor->find.open)
    {
        editor_find_drawn(editor, raster, glyphs, palette, place, scale, size, line_tall);
    }
}
