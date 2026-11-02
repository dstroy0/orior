// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editor.h: the editor part: the lines of the open file the CLI handed, each with its number and its runs, a column
// then the index of the token's name, the folds that open on them, and the names the file's grammar gives its tokens;
// drawn row by row from the view's top in the code face, each run in its token's color, the line numbers in a gutter,
// a tab laid out to its stop; the view's top moved by the wheel inside the file; the cursor, selection and bracket
// pair a view's frame places; and the caret, drawn over the frame last of all with the pixels under it saved, which
// its breath's steps put back and draw it over again alone (editor.c)
#ifndef GUI_PARTS_EDITOR_H
#define GUI_PARTS_EDITOR_H
typedef struct GuiEditorPlaceCellScaleCfg GuiEditorPlaceCellScaleCfg;
typedef struct GuiEditorPlaceCellCfg GuiEditorPlaceCellCfg;
typedef struct GuiEditorPlaceScaleCfg GuiEditorPlaceScaleCfg;
typedef struct GuiEditorFromCfg GuiEditorFromCfg;
typedef struct GuiEditorRasterCfg GuiEditorRasterCfg;
typedef struct GuiEditorPlaceCfg GuiEditorPlaceCfg;
typedef struct GuiEditorJsonReadCfg GuiEditorJsonReadCfg;
typedef struct GuiEditorPaletteCfg GuiEditorPaletteCfg;
typedef struct GuiEditorJsonCfg GuiEditorJsonCfg;
typedef struct GuiEditorJsonFrameCfg GuiEditorJsonFrameCfg;
typedef struct GuiEditorJsonNamesCfg GuiEditorJsonNamesCfg;
typedef struct GuiEditorDeltaCfg GuiEditorDeltaCfg;
typedef struct GuiEditorTextCfg GuiEditorTextCfg;
typedef struct GuiEditorCfg GuiEditorCfg;
#include "../../../cli/src/text/json_tree.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

// the pixels saved under the caret at the most: a caret two of the page's pixels wide over a row at any scale the
// window takes fits
#define GUI_CARET_UNDER_MOST 1024u

// the code's size and its line's height in the page's pixels, --code-size and --code-line; a tab's stop every so many
// columns; and the gutter's padding, after the numbers as --ed-num pads them and before them
#define GUI_CODE_SIZE 13u
#define GUI_CODE_LINE 20
#define GUI_TAB_COLUMNS 4u
#define GUI_GUTTER_AFTER 20
#define GUI_GUTTER_BEFORE 8

// A line stored: its number in the file, where its text starts among the part's bytes and its length, and where its
// runs start among the part's runs and how many pairs they are
typedef struct
{
    unsigned long long number;
    unsigned long long text;
    unsigned long long length;
    unsigned int runs;
    unsigned int run_count;
} GuiLine;

// A fold that opens on a line stored: that line, the last it hides, whether it shows the line it closes on after what
// it hides and whether the view holds it closed, and that line's text, its blanks left off, where it starts among the
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

// The editor: the line at the view's top and how many lines the file has; the lines stored, from `from`, `count` of
// them, each after the one before it among the lines shown, their texts in one cell and their runs in another; the
// folds that open on them, in a cell, with their closers' texts in another; the grammar's names, their texts in one
// cell and where each starts in another; the cursor and the anchor the selection runs from, where a view's frame gave
// them, the caret's ink, 0 to 255 of its color, which the window sets before drawing where its breath stands, the
// caret's rectangle as last drawn, whether the pixels under it are saved and those pixels, row after row; the bracket
// pair at the cursor, where the frame gave one; find as the frame shows it, with the matches among the lines
// stored, each a line, a from and a to column, in a cell; and the indentation the frame gave, tabs 1, spaces 0 or
// -1 where nothing set it, and its step
// The bytes each of the editor's cells has room for, the cells saved from frame to frame and taken again only where a
// frame needs more: the lines, their texts, their runs and the runs' second cell the colors are laid into, the folds,
// their closers, find's query and the matches. A frame writes into the cells it holds, which saves the shared cells'
// persistent end from climbing past every frame's passing cells
typedef struct
{
    unsigned long long line;
    unsigned long long bytes;
    unsigned long long run;
    unsigned long long run_laid;
    unsigned long long fold;
    unsigned long long closers;
    unsigned long long query;
    unsigned long long match;
} GuiEditorRooms;

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
    unsigned int caret_ink;
    GuiRect caret_rect;
    int caret_saved;
    GuiColor caret_under[GUI_CARET_UNDER_MOST];
    int has_pair;
    GuiPlace pair[2];
    GuiFind find;
    unsigned long long (*match)[3];
    unsigned long long matches;
    int indent_tabs;
    unsigned int indent_size;
    unsigned int *run_laid;
    GuiEditorRooms room;
} GuiEditor;

// an editor holding nothing; and every cell of its given back
void gui_editor_init(const GuiEditorCfg *args);
void gui_editor_free(const GuiEditorJsonNamesCfg *args);
// how many lines the file has
void gui_editor_shape(const GuiEditorCfg *args);
// the grammar's names saved from the array `names` of `json`, each a string: 1, or 0 where a cell could not be taken
int gui_editor_by_name(const GuiEditorJsonNamesCfg *args);
// the lines of the answer `read` of `json`, `{"lines", "colors"}`, stored as the lines from `from`, in place of those
// stored before: 1, or 0 where a cell could not be allocated, the part then storing none; and the runs of the array
// `colors`, each `{"from", "runs"}`, applied to the lines stored
int gui_editor_lines_put(const GuiEditorJsonReadCfg *args);
int gui_editor_on_color(const GuiEditorJsonCfg *args);
// `length` bytes of a file's whole text saved as its lines from the first, split as the CLI splits a file, at each
// newline with a carriage return before it taken off, a text ending in a newline having an empty line last, the
// file's line count set to theirs: 1, or 0 where a cell could not be taken
int gui_editor_text_put(const GuiEditorTextCfg *args);
// a view's frame `frame` of `json`, `{"top", "lines", "from", "rows", "numbers", "folds", "cursor", "anchor", "pair",
// "find", "matches", "colors"}`, taken whole: the top, the line count, the rows saved as the lines numbered in
// `numbers`, from `from` where no numbers come, the folds `[first, last, shown, closed, closer]` with their closers'
// texts, the cursor and the anchor, the bracket pair where it is two places, find where it is open, `{"open", "focus",
// "query", "at", "regex", "case", "word", "count", "current"}`, its matches among the rows, each `[line, from, to]`,
// and the rows' runs: 1, or 0 where a cell could not be taken
int gui_editor_frame_put(const GuiEditorJsonFrameCfg *args);
// the row and the cell column of the pixel at x, y of the editor drawn in `place` at `scale` with the code cell `cell`,
// the column counted from the text's left edge: 1, or 0 where the pixel is outside the text; and the row of the pixel
// where it stands in the gutter: 1, or 0 where it does not
int gui_editor_cell_at(const GuiEditorPlaceCellScaleCfg *args);
int gui_editor_row_at(const GuiEditorPlaceCellScaleCfg *args);
// the bytes of the stored line `line` between the units `from` and `to`, each cut to the line's end, into `text` and
// `length`: 1, or 0 where the line is not stored
int gui_editor_units_text(const GuiEditorFromCfg *args);
// the pixels the unit at `col` of `line` takes in the editor drawn in `place` at `scale` with the code cell `cell`,
// one cell wide and a row tall, into `rect`: 1, or 0 where the line is not among the rows drawn
int gui_editor_place_rect(const GuiEditorPlaceCellCfg *args);
// the view's top moved `delta` lines, no further than the file's first and last lines
void gui_editor_scroll(const GuiEditorDeltaCfg *args);
// the gutter's width for the file's line count, at the code cell `cell` and `scale`
int gui_editor_gutter(const GuiEditorPlaceCellCfg *args);
// the caret's rectangle in the editor drawn at `place` and `scale`, two of the page's pixels wide on the cursor's
// cell, cut to the text's columns: empty where no cursor stands, find holds the keys or the cursor's row is out of
// view
GuiRect gui_editor_caret_rect(const GuiEditorPlaceCfg *args);
// the caret drawn over the frame at `caret` at the ink the editor holds, the pixels under it saved where its
// rectangle's pixels fit the room saved for them; and the caret drawn again at the ink the editor holds now, the pixels
// saved put back first: each answers the caret's rectangle, the latter empty where none was saved
GuiRect gui_editor_caret_draw(const GuiEditorRasterCfg *args);
GuiRect gui_editor_caret_redrawn(const GuiEditorRasterCfg *args);
// the color of the token named by the index `name` among the grammar's names, the editor's text color where no name
// has the index
GuiColor gui_editor_color(const GuiEditorPaletteCfg *args);
// the editor drawn in `place` on `raster` at `scale`: the ground, then each row from the view's top a line stored, the
// current line's ground or the selection's, the matches' grounds, its number in the gutter with a mark where it opens
// a fold closed, its runs in their colors, the folded mark and the closer's text after a line that opens a fold
// closed, a box around each bracket of the pair and around the current match, the caret, and find's bar at the top
// right where it is open: its field with the query, the caret where the keys are its own, its toggles and its count
void gui_editor_draw(const GuiEditorPlaceScaleCfg *args);
struct GuiEditorPlaceCellScaleCfg
{
    const GuiEditor *editor_read;
    GuiRect place;
    float cell;
    int scale;
    int x;
    int y;
    unsigned long long *row;
    unsigned long long *column;
};

struct GuiEditorPlaceCellCfg
{
    const GuiEditor *editor_read;
    GuiRect place;
    float cell;
    int scale;
    unsigned long long line;
    unsigned long long col;
    GuiRect *rect;
};

struct GuiEditorPlaceScaleCfg
{
    GuiEditor *editor;
    GuiRect place;
    int scale;
    GuiGlyphs *glyphs;
    GuiRaster *raster;
    const GuiPalette *palette;
};

struct GuiEditorFromCfg
{
    unsigned long long from;
    const GuiEditor *editor_read;
    unsigned long long line;
    unsigned long long to;
    const char **text_pointer;
    unsigned long long *length_pointer;
};

struct GuiEditorRasterCfg
{
    GuiEditor *editor;
    GuiRaster *raster;
    const GuiPalette *palette;
    GuiRect caret;
};

struct GuiEditorPlaceCfg
{
    const GuiEditor *editor_read;
    GuiRect place;
    int scale;
    GuiGlyphs *glyphs;
};

struct GuiEditorJsonReadCfg
{
    GuiEditor *editor;
    const CliJson *json;
    unsigned int read;
    unsigned long long from;
};

struct GuiEditorPaletteCfg
{
    const GuiEditor *editor_read;
    const GuiPalette *palette;
    unsigned int name;
};

struct GuiEditorJsonCfg
{
    GuiEditor *editor;
    const CliJson *json;
    unsigned int colors;
};

struct GuiEditorJsonFrameCfg
{
    GuiEditor *editor;
    const CliJson *json;
    unsigned int frame;
};

struct GuiEditorJsonNamesCfg
{
    GuiEditor *editor;
    const CliJson *json;
    unsigned int names;
};

struct GuiEditorDeltaCfg
{
    GuiEditor *editor;
    long long delta;
    unsigned int rows;
};

struct GuiEditorTextCfg
{
    GuiEditor *editor;
    const char *text;
    unsigned long long length;
};

struct GuiEditorCfg
{
    GuiEditor *editor;
    unsigned long long lines;
};

typedef struct
{
    void (*init)(const GuiEditorCfg *args);
    void (*shape)(const GuiEditorCfg *args);
} GuiEditorNs;
EMBED_TABLE_LAYOUT(GuiEditorNs, init, shape);
EMBED_TABLE_STORAGE GuiEditorNs gui_editor_table EMBED_UNUSED = {
    .init = gui_editor_init,
    .shape = gui_editor_shape,
};

typedef struct
{
    void (*free)(const GuiEditorJsonNamesCfg *args);
    int (*by_name)(const GuiEditorJsonNamesCfg *args);
} GuiEditorJsonNamesNs;
EMBED_TABLE_LAYOUT(GuiEditorJsonNamesNs, free, by_name);
EMBED_TABLE_STORAGE GuiEditorJsonNamesNs gui_editor_json_names EMBED_UNUSED = {
    .free = gui_editor_free,
    .by_name = gui_editor_by_name,
};

typedef struct
{
    int (*lines_put)(const GuiEditorJsonReadCfg *args);
} GuiEditorJsonReadNs;
EMBED_TABLE_LAYOUT(GuiEditorJsonReadNs, lines_put);
EMBED_TABLE_STORAGE GuiEditorJsonReadNs gui_editor_json_read EMBED_UNUSED = {
    .lines_put = gui_editor_lines_put,
};

typedef struct
{
    int (*color)(const GuiEditorJsonCfg *args);
} GuiEditorJsonNs;
EMBED_TABLE_LAYOUT(GuiEditorJsonNs, color);
EMBED_TABLE_STORAGE GuiEditorJsonNs gui_editor_json EMBED_UNUSED = {
    .color = gui_editor_on_color,
};

typedef struct
{
    int (*text_put)(const GuiEditorTextCfg *args);
} GuiEditorTextNs;
EMBED_TABLE_LAYOUT(GuiEditorTextNs, text_put);
EMBED_TABLE_STORAGE GuiEditorTextNs gui_editor_text EMBED_UNUSED = {
    .text_put = gui_editor_text_put,
};

typedef struct
{
    int (*frame_put)(const GuiEditorJsonFrameCfg *args);
} GuiEditorJsonFrameNs;
EMBED_TABLE_LAYOUT(GuiEditorJsonFrameNs, frame_put);
EMBED_TABLE_STORAGE GuiEditorJsonFrameNs gui_editor_json_frame EMBED_UNUSED = {
    .frame_put = gui_editor_frame_put,
};

typedef struct
{
    int (*cell_at)(const GuiEditorPlaceCellScaleCfg *args);
    int (*row_at)(const GuiEditorPlaceCellScaleCfg *args);
} GuiEditorPlaceCellScaleNs;
EMBED_TABLE_LAYOUT(GuiEditorPlaceCellScaleNs, cell_at, row_at);
EMBED_TABLE_STORAGE GuiEditorPlaceCellScaleNs gui_editor_place_cell_scale EMBED_UNUSED = {
    .cell_at = gui_editor_cell_at,
    .row_at = gui_editor_row_at,
};

typedef struct
{
    int (*units_text)(const GuiEditorFromCfg *args);
} GuiEditorFromNs;
EMBED_TABLE_LAYOUT(GuiEditorFromNs, units_text);
EMBED_TABLE_STORAGE GuiEditorFromNs gui_editor_from EMBED_UNUSED = {
    .units_text = gui_editor_units_text,
};

typedef struct
{
    int (*place_rect)(const GuiEditorPlaceCellCfg *args);
    int (*gutter)(const GuiEditorPlaceCellCfg *args);
} GuiEditorPlaceCellNs;
EMBED_TABLE_LAYOUT(GuiEditorPlaceCellNs, place_rect, gutter);
EMBED_TABLE_STORAGE GuiEditorPlaceCellNs gui_editor_place_cell EMBED_UNUSED = {
    .place_rect = gui_editor_place_rect,
    .gutter = gui_editor_gutter,
};

typedef struct
{
    void (*scroll)(const GuiEditorDeltaCfg *args);
} GuiEditorDeltaNs;
EMBED_TABLE_LAYOUT(GuiEditorDeltaNs, scroll);
EMBED_TABLE_STORAGE GuiEditorDeltaNs gui_editor_delta EMBED_UNUSED = {
    .scroll = gui_editor_scroll,
};

typedef struct
{
    GuiRect (*caret_rect)(const GuiEditorPlaceCfg *args);
} GuiEditorPlaceNs;
EMBED_TABLE_LAYOUT(GuiEditorPlaceNs, caret_rect);
EMBED_TABLE_STORAGE GuiEditorPlaceNs gui_editor_place EMBED_UNUSED = {
    .caret_rect = gui_editor_caret_rect,
};

typedef struct
{
    GuiRect (*caret_draw)(const GuiEditorRasterCfg *args);
    GuiRect (*caret_redrawn)(const GuiEditorRasterCfg *args);
} GuiEditorRasterNs;
EMBED_TABLE_LAYOUT(GuiEditorRasterNs, caret_draw, caret_redrawn);
EMBED_TABLE_STORAGE GuiEditorRasterNs gui_editor_raster EMBED_UNUSED = {
    .caret_draw = gui_editor_caret_draw,
    .caret_redrawn = gui_editor_caret_redrawn,
};

typedef struct
{
    GuiColor (*color)(const GuiEditorPaletteCfg *args);
} GuiEditorPaletteNs;
EMBED_TABLE_LAYOUT(GuiEditorPaletteNs, color);
EMBED_TABLE_STORAGE GuiEditorPaletteNs gui_editor_palette EMBED_UNUSED = {
    .color = gui_editor_color,
};

typedef struct
{
    void (*draw)(const GuiEditorPlaceScaleCfg *args);
} GuiEditorPlaceScaleNs;
EMBED_TABLE_LAYOUT(GuiEditorPlaceScaleNs, draw);
EMBED_TABLE_STORAGE GuiEditorPlaceScaleNs gui_editor_place_scale EMBED_UNUSED = {
    .draw = gui_editor_draw,
};

#endif
