// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editor.h: the editor part: the lines of the open file the CLI handed, each with its number and its runs, a column
// then the index of the token's name, the folds that open on them, and the names the file's grammar gives its tokens;
// drawn row by row from the view's top in the code face, each run in its token's color, the line numbers in a gutter,
// a tab laid out to its stop; the view's top moved by the wheel inside the file; and the cursor, selection and bracket
// pair a view's frame places (editor.c)
#ifndef GUI_PARTS_EDITOR_H
#define GUI_PARTS_EDITOR_H
#include "../../../cli/src/text/json_tree.h"
#include "../glyphs.h"
#include "../palette.h"

// the code's size and its line's height in the page's pixels, --code-size and --code-line; a tab's stop every so many
// columns; and the gutter's padding, after the numbers as --ed-num pads them and before them
#define GUI_CODE_SIZE 13u
#define GUI_CODE_LINE 20
#define GUI_TAB_COLUMNS 4u
#define GUI_GUTTER_AFTER 20
#define GUI_GUTTER_BEFORE 8

// A line held: its number in the file, where its text starts among the part's bytes and its length, and where its runs
// start among the part's runs and how many pairs they are
typedef struct
{
    unsigned long long number;
    unsigned long long text;
    unsigned long long length;
    unsigned int runs;
    unsigned int run_count;
} GuiLine;

// A fold that opens on a line held: that line, the last it hides, whether it shows the line it closes on after what it
// hides and whether the view holds it closed, and that line's text, its blanks left off, where it starts among the
// part's closers and its length
typedef struct
{
    unsigned long long first;
    unsigned long long last;
    int shown;
    int closed;
    unsigned long long closer;
    unsigned long long closer_length;
} GuiFold;

// A place in the text: its line and its column in UTF-16 units, as the CLI's view places the cursor
typedef struct
{
    unsigned long long line;
    unsigned long long col;
} GuiPlace;

// Find as a frame shows it: whether its bar is open and the keys are its own; its query, `query_length` bytes in a
// cell, and where its cursor stands in it, in units; whether it seeks a pattern, with case and whole words; how many
// matches there are and the current one, -1 for none
typedef struct
{
    int open;
    int focus;
    char *query;
    unsigned long long query_length;
    unsigned long long at;
    int regex;
    int match_case;
    int word;
    unsigned long long count;
    long long current;
} GuiFind;

// the find bar's field in the page's pixels, its gap from the editor's right edge, its padding, a toggle's width and
// the count's
#define GUI_FIND_FIELD 240
#define GUI_FIND_RIGHT 16
#define GUI_FIND_PAD 6
#define GUI_FIND_TOGGLE 26
#define GUI_FIND_COUNT 88

// The editor: the line at the view's top and how many lines the file has; the lines held, from `from`, `count` of
// them, each after the one before it among the lines shown, their texts in one cell and their runs in another; the
// folds that open on them, in a cell, with their closers' texts in another; the grammar's names, their texts in one
// cell and where each starts in another; the cursor and the anchor the selection runs from, where a view's frame gave
// them; the bracket pair at the cursor, where the frame gave one; find as the frame shows it, with the matches among
// the lines held, each a line, a from and a to column, in a cell; and the indentation the frame gave, tabs 1, spaces
// 0 or -1 where nothing set it, and its step
typedef struct
{
    unsigned long long top;
    unsigned long long lines;
    unsigned long long from;
    unsigned int count;
    GuiLine *line;
    char *bytes;
    unsigned int *run;
    unsigned int run_total;
    GuiFold *fold;
    unsigned int folds;
    char *closers;
    char *names;
    unsigned long long *name_at;
    unsigned int name_count;
    int has_cursor;
    GuiPlace cursor;
    GuiPlace anchor;
    int has_pair;
    GuiPlace pair[2];
    GuiFind find;
    unsigned long long (*match)[3];
    unsigned long long matches;
    int indent_tabs;
    unsigned int indent_size;
} GuiEditor;

// an editor holding nothing; and every cell of its given back
void gui_editor_made(GuiEditor *editor);
void gui_editor_given(GuiEditor *editor);
// how many lines the file has
void gui_editor_shaped(GuiEditor *editor, unsigned long long lines);
// the grammar's names kept from the array `names` of `json`, each a string: 1, or 0 where a cell could not be taken
int gui_editor_named(GuiEditor *editor, const CliJson *json, unsigned int names);
// the lines of the answer `read` of `json`, `{"lines", "colors"}`, kept as the lines from `from`, in place of those
// held: 1, or 0 where a cell could not be taken, the part then holding none; and the runs of the array `colors`,
// each `{"from", "runs"}`, put on the lines held
int gui_editor_lines_put(GuiEditor *editor, const CliJson *json, unsigned int read, unsigned long long from);
int gui_editor_colored(GuiEditor *editor, const CliJson *json, unsigned int colors);
// `length` bytes of a file's whole text kept as its lines from the first, split as the CLI splits a file, at each
// newline with a carriage return before it taken off, a text ending in a newline having an empty line last, the
// file's line count set to theirs: 1, or 0 where a cell could not be taken
int gui_editor_text_put(GuiEditor *editor, const char *text, unsigned long long length);
// a view's frame `frame` of `json`, `{"top", "lines", "from", "rows", "numbers", "folds", "cursor", "anchor", "pair",
// "find", "matches", "colors"}`, taken whole: the top, the line count, the rows kept as the lines numbered in
// `numbers`, from `from` where no numbers come, the folds `[first, last, shown, closed, closer]` with their closers'
// texts, the cursor and the anchor, the bracket pair where it is two places, find where it is open, `{"open", "focus",
// "query", "at", "regex", "case", "word", "count", "current"}`, its matches among the rows, each `[line, from, to]`,
// and the rows' runs: 1, or 0 where a cell could not be taken
int gui_editor_frame_put(GuiEditor *editor, const CliJson *json, unsigned int frame);
// the row and the cell column of the pixel at x, y of the editor drawn in `place` at `scale` with the code cell `cell`,
// the column counted from the text's left edge: 1, or 0 where the pixel is outside the text; and the row of the pixel
// where it stands in the gutter: 1, or 0 where it does not
int gui_editor_cell_at(const GuiEditor *editor, GuiRect place, float cell, float scale, int x, int y,
                       unsigned long long *row, unsigned long long *column);
int gui_editor_row_at(const GuiEditor *editor, GuiRect place, float cell, float scale, int x, int y,
                      unsigned long long *row);
// the view's top moved `delta` lines, no further than the file's first and last lines
void gui_editor_scrolled(GuiEditor *editor, long long delta, unsigned int rows);
// the gutter's width for the file's line count, at the code cell `cell` and `scale`
int gui_editor_gutter(const GuiEditor *editor, float cell, float scale);
// the editor drawn in `place` on `raster` at `scale`: the ground, then each row from the view's top a line held, the
// current line's ground or the selection's, the matches' grounds, its number in the gutter with a mark where it opens
// a fold closed, its runs in their colors, the folded mark and the closer's text after a line that opens a fold
// closed, a box around each bracket of the pair and around the current match, the caret, and find's bar at the top
// right where it is open: its field with the query, the caret where the keys are its own, its toggles and its count
void gui_editor_drawn(GuiEditor *editor, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette,
                      GuiRect place, float scale);
#endif
