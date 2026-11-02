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
typedef struct GuiEditorPositionCellScaleCfg GuiEditorPositionCellScaleCfg;
typedef struct GuiEditorPositionCellCfg GuiEditorPositionCellCfg;
typedef struct GuiEditorPositionScaleCfg GuiEditorPositionScaleCfg;
typedef struct GuiEditorRasterCfg GuiEditorRasterCfg;
typedef struct GuiEditorPositionCfg GuiEditorPositionCfg;
typedef struct GuiEditorPaletteCfg GuiEditorPaletteCfg;
typedef struct GuiEditorCfg GuiEditorCfg;
#include "../../../cli/src/editor/view.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

// the pixels saved under the caret at the most: a caret two of the page's pixels wide over a row at any scale the
// window takes fits
#define GUI_CARET_UNDER_MAX 1024u

// the code's size and its line's height in the page's pixels, --code-size and --code-line; a tab's stop every so many
// columns; and the gutter's padding, after the numbers as --ed-num pads them and before them
#define GUI_CODE_SIZE 13u
#define GUI_CODE_LINE 20
#define GUI_TAB_COLUMNS 4u
#define GUI_GUTTER_AFTER 20
#define GUI_GUTTER_BEFORE 8

// the find bar's field in the page's pixels, its gap from the editor's right edge, its padding, a toggle's width and
// the count's
#define GUI_FIND_FIELD 240
#define GUI_FIND_RIGHT 16
#define GUI_FIND_PAD 6
#define GUI_FIND_TOGGLE 26
#define GUI_FIND_COUNT 88

// The editor: the frame and the token names as last published, NULL for none, each the controller's and read alone; the
// caret's ink, 0 to 255 of its color, which the window sets before drawing where its breath stands, the caret's
// rectangle as last drawn, whether the pixels under it are saved and those pixels, row after row
typedef struct
{
    const CliViewFrame *frame;
    const CliDocumentNames *names;
    unsigned int caret_color;
    GuiRect caret_rect;
    int caret_saved;
    GuiColor caret_under[GUI_CARET_UNDER_MAX];
} GuiEditor;

// an editor showing nothing
void gui_editor_init(const GuiEditorCfg *args);
// the row and the cell column of the pixel at x, y of the editor drawn in `place` at `scale` with the code cell `cell`,
// the column counted from the text's left edge: 1, or 0 where the pixel is outside the text; and the row of the pixel
// where it stands in the gutter: 1, or 0 where it does not
int gui_editor_cell_at(const GuiEditorPositionCellScaleCfg *args);
int gui_editor_row_at(const GuiEditorPositionCellScaleCfg *args);
// the pixels the unit at `col` of `line` takes in the editor drawn in `place` at `scale` with the code cell `cell`,
// one cell wide and a row tall, into `rect`: 1, or 0 where the line is not among the rows drawn
int gui_editor_position_rect(const GuiEditorPositionCellCfg *args);
// the gutter's width for the file's line count, at the code cell `cell` and `scale`
int gui_editor_gutter(const GuiEditorPositionCellCfg *args);
// the caret's rectangle in the editor drawn at `place` and `scale`, two of the page's pixels wide on the cursor's
// cell, cut to the text's columns: empty where no cursor stands, find holds the keys or the cursor's row is out of
// view
GuiRect gui_editor_caret_rect(const GuiEditorPositionCfg *args);
// the caret drawn over the frame at `caret` at the ink the editor holds, the pixels under it saved where its
// rectangle's pixels fit the capacity saved for them; and the caret drawn again at the ink the editor holds now, the
// pixels saved put back first: each answers the caret's rectangle, the latter empty where none was saved
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
};

struct GuiEditorPositionScaleCfg
{
    GuiEditor *editor;
    GuiRect place;
    int scale;
    GuiGlyphs *glyphs;
    GuiRaster *raster;
    const GuiPalette *palette;
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
} GuiEditorPositionCellScaleNs;
EMBED_TABLE_LAYOUT(GuiEditorPositionCellScaleNs, cell_at, row_at);
EMBED_TABLE_STORAGE GuiEditorPositionCellScaleNs gui_editor_position_cell_scale EMBED_UNUSED = {
    .cell_at = gui_editor_cell_at,
    .row_at = gui_editor_row_at,
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
} GuiEditorPaletteNs;
EMBED_TABLE_LAYOUT(GuiEditorPaletteNs, color);
EMBED_TABLE_STORAGE GuiEditorPaletteNs gui_editor_palette EMBED_UNUSED = {
    .color = gui_editor_color,
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
