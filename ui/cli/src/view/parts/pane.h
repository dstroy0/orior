// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pane.h: what the side's panes of rows share under their group's title: the line under the title, the pane's head with
// its arrow and its name, the body on the editor's ground, rows of one height from the head down, the row at a point,
// the scroll the wheel turns no further than the last row fills the pane, a row's baseline, the guides down a row's
// levels and the arrow before a row (pane.c)
#ifndef GUI_PARTS_PANE_H
#define GUI_PARTS_PANE_H
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiPaneCfg GuiPaneCfg;

// no row; and the sizes in the page's pixels: a row, a row's padding at the left, a guide's width, the arrow's side,
// the head's text size and a row's
#define GUI_PANE_NONE (-1)
#define GUI_PANE_ROW 22
#define GUI_PANE_PAD_LEFT 8
#define GUI_PANE_GUIDE 12
#define GUI_PANE_ARROW 5
#define GUI_PANE_HEAD_SIZE 12u
#define GUI_PANE_ROW_SIZE 13u

// the top of the rows in `frame`'s side, under the group's title and the pane's head; the rectangle of the row `row`
// with `scroll` rows scrolled off the top; and the row at x, y: 1 with its number into `row_pointer`, else 0, `count`
// rows standing
int gui_pane_rows_top(const GuiPaneCfg *args);
GuiRect gui_pane_row_rect(const GuiPaneCfg *args);
int gui_pane_hit(const GuiPaneCfg *args);
// `*scroll_pointer` turned `rows` rows, down where positive, of `count` rows, no further than the last fills the pane:
// 1 where it changed
int gui_pane_scrolled(const GuiPaneCfg *args);
// the baseline of text in `face` at `size` centered in a row of `pixel_height` from `top`
int gui_pane_baseline(const GuiPaneCfg *args);
// the pane drawn under the group's title: its line, its head with the arrow open and `title`, and its body on the
// editor's ground, inside the work's inside: the body's rectangle, which the rows are cut to, empty where none shows
GuiRect gui_pane_body(const GuiPaneCfg *args);
// a guide down each of `depth` levels from the row `rect`'s left padding, cut to `clip`: the x past them
int gui_pane_guides(const GuiPaneCfg *args);
// an arrow `side` pixels across at x, y, down where `open` and right where not, drawn whole inside `clip` or not at
// all, in `color`
void gui_pane_arrow_draw(const GuiPaneCfg *args);
struct GuiPaneCfg
{
    const GuiFrame *frame;
    unsigned int row;
    unsigned int scroll;
    unsigned int *scroll_pointer;
    unsigned int count;
    int rows;
    int x;
    int y;
    unsigned int *row_pointer;
    GuiGlyphs *glyphs;
    unsigned int face;
    unsigned int size;
    int top;
    int pixel_height;
    GuiRaster *raster;
    const GuiPalette *palette;
    const char *title;
    GuiRect rect;
    GuiRect clip;
    unsigned int depth;
    int side;
    int open;
    GuiColor color;
};

typedef struct
{
    int (*rows_top)(const GuiPaneCfg *args);
    GuiRect (*row_rect)(const GuiPaneCfg *args);
    int (*hit)(const GuiPaneCfg *args);
    int (*scrolled)(const GuiPaneCfg *args);
    int (*baseline)(const GuiPaneCfg *args);
    GuiRect (*body)(const GuiPaneCfg *args);
    int (*guides)(const GuiPaneCfg *args);
    void (*arrow_draw)(const GuiPaneCfg *args);
} GuiPaneNs;
EMBED_TABLE_LAYOUT(GuiPaneNs, rows_top, row_rect, hit, scrolled, baseline, body, guides, arrow_draw);
EMBED_TABLE_STORAGE GuiPaneNs gui_pane EMBED_UNUSED = {
    .rows_top = gui_pane_rows_top,
    .row_rect = gui_pane_row_rect,
    .hit = gui_pane_hit,
    .scrolled = gui_pane_scrolled,
    .baseline = gui_pane_baseline,
    .body = gui_pane_body,
    .guides = gui_pane_guides,
    .arrow_draw = gui_pane_arrow_draw,
};

#endif
