// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editor.h: the editor part: the lines of the open file the CLI passed, each with its number and its runs, a column
// then the index of the token's name, the folds that open on them, and the names the file's grammar gives its tokens;
// drawn row by row from the view's top in the code face, each run in its token's color, the line numbers in a gutter,
// a tab arranged to its stop; the view's top moved by the wheel inside the file; the cursor, selection and bracket
// pair a view's frame places; and the caret, drawn over the frame last of all with the pixels under it saved, which
// its breath's steps put back and draw it over again alone (editor.c)
#ifndef GUI_WINDOW_EDITOR_H
#define GUI_WINDOW_EDITOR_H
#include "../../../includes/window/glyphs.h"
#include "../../../includes/window/palette.h"
#include "../../cli/src/editor/view.h"
#include "embed_dispatch_layout.h"

typedef struct GuiEditorPositionCellScaleCfg GuiEditorPositionCellScaleCfg;
typedef struct GuiEditorPositionCellCfg GuiEditorPositionCellCfg;
typedef struct GuiEditorPositionScaleCfg GuiEditorPositionScaleCfg;
typedef struct GuiEditorRasterCfg GuiEditorRasterCfg;
typedef struct GuiEditorPositionCfg GuiEditorPositionCfg;
typedef struct GuiEditorPaletteCfg GuiEditorPaletteCfg;
typedef struct GuiEditorCfg GuiEditorCfg;

// The pixels kept under the caret at max. A caret two of the page's pixels wide over a row fits, at any scale the
// window takes
#define GUI_CARET_UNDER_MAX 1024u
// the token names whose colors the editor retains
#define GUI_EDITOR_NAME_COLORS 256u

// The code's size and its line's height in the page's pixels, --code-size and --code-line. Then a tab's stop every so
// many columns where a frame sets no indentation size. Last, the gutter's padding, after the numbers as --ed-num pads
// them, and before them
#define GUI_CODE_SIZE 13u
#define GUI_CODE_LINE 20
#define GUI_TAB_COLUMNS 4u
// the whitespace marks: a middle dot for a space and a rightwards arrow for a tab
#define GUI_EDITOR_SPACE_MARK 0xb7u
#define GUI_EDITOR_TAB_MARK 0x2192u
// the pixels either side of a table's column line a press grips it within
#define GUI_EDITOR_GRIP 3
#define GUI_GUTTER_AFTER 20
#define GUI_GUTTER_BEFORE 8

// the find bar's field in the page's pixels, its gap from the editor's right edge, its padding, a toggle's width and
// the count's
#define GUI_FIND_FIELD 240
#define GUI_FIND_RIGHT 16
#define GUI_FIND_PAD 6
#define GUI_FIND_TOGGLE 26
#define GUI_FIND_COUNT 88

// The editor. It holds the displayed view's frame, read for the window's call alone (editor/view.h), NULL for none.
// Then the key of the text whose token names it colors, empty for none. Then the count and the serial of its names
// table as the window last read them (editor/document.h).
//
// Then the caret's ink, 0 to 255 of its color, which the window sets before a draw where its breath is. Then the
// caret's rectangle as last drawn, and whether the window kept the pixels under it, with those pixels, row after row.
//
// Then the part of a line the view scrolled past its first line and first cell column. `scroll_y` is the pixels of the
// frame's first row above the editor's top, 0 to a line's height less 1. `scroll_x` is the pixels of the first cell
// column left of the text's edge, 0 to a cell less a part of a pixel.
//
// Last, the color of each token name, kept for the names table's serial `colored_serial`, its count `name_colors` and
// the palette of the last read, `colored_palette`. The window reads a name at GUI_EDITOR_NAME_COLORS or past it from
// the palette each time
typedef struct
{
    const CliViewFrame *frame;
    char names_key[CLI_NAME_MAX_LEN];
    unsigned int names_count;
    unsigned int names_serial;
    unsigned int caret_color;
    GuiRect caret_rect;
    int caret_saved;
    GuiColor caret_under[GUI_CARET_UNDER_MAX];
    int scroll_y;
    float scroll_x;
    GuiColor name_color[GUI_EDITOR_NAME_COLORS];
    unsigned int name_colors;
    unsigned int colored_serial;
    const GuiPalette *colored_palette;
    // whether a space draws as a middle dot and a tab as an arrow, in the quietest text color
    int whitespace;
} GuiEditor;

// an editor that shows nothing
void gui_editor_init(const GuiEditorCfg *args);
// The row and the cell column of the pixel at x, y of the editor drawn in `place` at `scale` with the code cell `cell`.
// The column counts from the text's left edge. Returns 1, or 0 where the pixel is outside the text. Then the row of the
// pixel where it is in the gutter: 1, or 0 where it is not
int gui_editor_cell_at(const GuiEditorPositionCellScaleCfg *args);
int gui_editor_row_at(const GuiEditorPositionCellScaleCfg *args);
// The line of the sticky row the pixel at x, y is on, into `row`. The sticky rows are the rows pinned over the
// editor's top. Returns 1, or 0 where it is on none
int gui_editor_sticky_at(const GuiEditorPositionCellScaleCfg *args);
// The index, counted from one, of the table column line within GUI_EDITOR_GRIP scaled pixels of the pixel at x, y. The
// editor draws in `place` with the code cell `cell`. Returns 0 where the frame is no table or the pixel is on no line
unsigned int gui_editor_column_edge(const GuiEditorPositionCellScaleCfg *args);
// The pixels the unit at `col` of `line` takes in the editor drawn in `place` at `scale` with the code cell `cell`.
// They are one cell wide and a row tall, into `rect`. Returns 1, or 0 where the line is not among the rows drawn. A
// unit the view slid past, left of the text or past the editor's right edge, also gives 0. Where `clamped` is 1, such
// a unit gives 1, and its rect is at the edge it is past
int gui_editor_position_rect(const GuiEditorPositionCellCfg *args);
// the gutter's width for the file's line count, at the code cell `cell` and `scale`
int gui_editor_gutter(const GuiEditorPositionCellCfg *args);
// The caret's rectangle in the editor drawn at `place` and `scale`, two of the page's pixels wide on the cursor's
// cell, cut to the text's columns. It is empty where no cursor is, find has the keys, or the cursor's row is out of
// view
GuiRect gui_editor_caret_rect(const GuiEditorPositionCfg *args);
// The caret drawn over the frame at `caret` at the ink the editor has. The pixels under it stay kept where its
// rectangle's pixels fit the capacity for them. Then the caret drawn again at the ink the editor has now, the pixels
// kept put back first. Each answers the caret's rectangle, the second empty where it kept none
GuiRect gui_editor_caret_draw(const GuiEditorRasterCfg *args);
GuiRect gui_editor_caret_redrawn(const GuiEditorRasterCfg *args);
// the color of the token named by the index `name` among the grammar's names, the editor's text color where no name
// has the index
GuiColor gui_editor_color(const GuiEditorPaletteCfg *args);
// The color of each token name kept in the editor for its names and `palette`. The editor reads the colors from the
// palette again only where the names table, its count or the palette changed since the last read
void gui_editor_colors_retain(const GuiEditorPaletteCfg *args);
// The editor drawn in `place` on `raster` at `scale`. First the ground, then each row from the view's top a line of the
// store. Each row gets the current line's ground or the selection's, and the matches' grounds. Its number goes in the
// gutter, with a mark where it opens a fold closed, and its runs go in their colors. After a line that opens a fold
// closed come the folded mark and the closer's text. Then a box goes around each bracket of the pair and around the
// current match, and the caret goes on.
//
// Last, find's bar at the top right where it is open. It has its field with the query, the caret where the keys are
// its own, its toggles and its count
void gui_editor_draw(const GuiEditorPositionScaleCfg *args);
struct GuiEditorPositionCellScaleCfg
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

struct GuiEditorPositionCellCfg
{
    const GuiEditor *editor_read;
    GuiRect place;
    float cell;
    int scale;
    unsigned long long line;
    unsigned long long col;
    GuiRect *rect;
    int clamped;
};

// `notch` is the pane down over the editor's top corner, empty where none is, and `radius` the bend the work's border
// makes round it. The sticky rows stop short of the notch by the radius
struct GuiEditorPositionScaleCfg
{
    GuiEditor *editor;
    GuiRect place;
    GuiRect notch;
    unsigned int radius;
    int scale;
    GuiGlyphs *glyphs;
    GuiRaster *raster;
    const GuiPalette *palette;
    const char *key;
};

struct GuiEditorRasterCfg
{
    GuiEditor *editor;
    GuiRaster *raster;
    const GuiPalette *palette;
    GuiRect caret;
};

struct GuiEditorPositionCfg
{
    const GuiEditor *editor_read;
    GuiRect place;
    int scale;
    GuiGlyphs *glyphs;
};

struct GuiEditorPaletteCfg
{
    GuiEditor *editor;
    const GuiEditor *editor_read;
    const GuiPalette *palette;
    unsigned int name;
};

struct GuiEditorCfg
{
    GuiEditor *editor;
};

typedef struct
{
    void (*init)(const GuiEditorCfg *args);
} GuiEditorNs;
EMBED_TABLE_LAYOUT(GuiEditorNs, init);
EMBED_TABLE_STORAGE GuiEditorNs gui_editor_table EMBED_UNUSED = {
    .init = gui_editor_init,
};

typedef struct
{
    int (*cell_at)(const GuiEditorPositionCellScaleCfg *args);
    int (*row_at)(const GuiEditorPositionCellScaleCfg *args);
    int (*sticky_at)(const GuiEditorPositionCellScaleCfg *args);
    unsigned int (*column_edge)(const GuiEditorPositionCellScaleCfg *args);
} GuiEditorPositionCellScaleNs;
EMBED_TABLE_LAYOUT(GuiEditorPositionCellScaleNs, cell_at, row_at, sticky_at, column_edge);
EMBED_TABLE_STORAGE GuiEditorPositionCellScaleNs gui_editor_position_cell_scale EMBED_UNUSED = {
    .cell_at = gui_editor_cell_at,
    .row_at = gui_editor_row_at,
    .sticky_at = gui_editor_sticky_at,
    .column_edge = gui_editor_column_edge,
};

typedef struct
{
    int (*position_rect)(const GuiEditorPositionCellCfg *args);
    int (*gutter)(const GuiEditorPositionCellCfg *args);
} GuiEditorPositionCellNs;
EMBED_TABLE_LAYOUT(GuiEditorPositionCellNs, position_rect, gutter);
EMBED_TABLE_STORAGE GuiEditorPositionCellNs gui_editor_position_cell EMBED_UNUSED = {
    .position_rect = gui_editor_position_rect,
    .gutter = gui_editor_gutter,
};

typedef struct
{
    GuiRect (*caret_rect)(const GuiEditorPositionCfg *args);
} GuiEditorPositionNs;
EMBED_TABLE_LAYOUT(GuiEditorPositionNs, caret_rect);
EMBED_TABLE_STORAGE GuiEditorPositionNs gui_editor_position EMBED_UNUSED = {
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
    void (*colors_retain)(const GuiEditorPaletteCfg *args);
} GuiEditorPaletteNs;
EMBED_TABLE_LAYOUT(GuiEditorPaletteNs, color, colors_retain);
EMBED_TABLE_STORAGE GuiEditorPaletteNs gui_editor_palette EMBED_UNUSED = {
    .color = gui_editor_color,
    .colors_retain = gui_editor_colors_retain,
};

typedef struct
{
    void (*draw)(const GuiEditorPositionScaleCfg *args);
} GuiEditorPositionScaleNs;
EMBED_TABLE_LAYOUT(GuiEditorPositionScaleNs, draw);
EMBED_TABLE_STORAGE GuiEditorPositionScaleNs gui_editor_position_scale EMBED_UNUSED = {
    .draw = gui_editor_draw,
};

#endif
