// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// suggest.h: the completion panel, as the panels over the editor rank and carry it (servers/servers.h): listed under
// the prefix's first cell, the chosen row on the selection's ground, each row its table's mark, its label with the
// prefix in signal and its detail quieter at the right; a detail pane beside the list for the chosen item. What keys
// and presses do to it is the panels' (suggest.c)
#ifndef GUI_WINDOW_SUGGEST_H
#define GUI_WINDOW_SUGGEST_H
#include "../../../includes/window/panel.h"
#include "../../cli/src/servers/servers.h"
#include "embed_dispatch_layout.h"

typedef struct GuiSuggestRasterCfg GuiSuggestRasterCfg;
typedef struct GuiSuggestXCfg GuiSuggestXCfg;
typedef struct GuiSuggestCfg GuiSuggestCfg;

// the sizes in the page's pixels: a row's height, the list's least and max width, 16rem and 26rem, the detail pane's
// width, 18rem, and its max height, 14rem, a table's mark, 0.55rem, a row's padding across, 0.55rem, the gap between a
// row's parts, 0.5rem, the list's padding down, 0.2rem, how far the list's left is before the prefix's cell, and
// the detail's padding
#define GUI_SUGGEST_ROW 22
#define GUI_SUGGEST_MIN 256
#define GUI_SUGGEST_MAX 416
#define GUI_SUGGEST_DETAIL 288
#define GUI_SUGGEST_DETAIL_HEIGHT 224
#define GUI_SUGGEST_TYPE 9
#define GUI_SUGGEST_PAD_X 9
#define GUI_SUGGEST_GAP 8
#define GUI_SUGGEST_PAD_Y 3
#define GUI_SUGGEST_BACK 22
#define GUI_SUGGEST_DETAIL_PAD_X 11
#define GUI_SUGGEST_DETAIL_PAD_Y 6

// The completion: the panel displayed, NULL for none, its rows read from the panels; and the list's rectangle, the rows'
// and the detail pane's, where they were drawn last
typedef struct
{
    const CliCompletionPanel *panel;
    GuiRect rect;
    GuiRect list;
    GuiRect detail;
} GuiSuggest;

// a completion showing nothing
void gui_suggest_init(const GuiSuggestCfg *args);
// whether the pixel at x, y is in the list or its detail pane; and the row's place among the items at x, y into
// `at`: 1, or 0 where the pixel is on no row
int gui_suggest_holds(const GuiSuggestXCfg *args);
int gui_suggest_row_at(const GuiSuggestXCfg *args);
// the list drawn for the prefix whose first cell is `anchor`, under its row inside `bounds` at `scale`, the detail
// pane beside it where the chosen item has a detail; the rectangles recorded
void gui_suggest_draw(const GuiSuggestRasterCfg *args);
struct GuiSuggestRasterCfg
{
    GuiSuggest *suggest;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    GuiRect anchor;
    GuiRect bounds;
    int scale;
};

struct GuiSuggestXCfg
{
    const GuiSuggest *suggest_read;
    int x;
    int y;
    unsigned int *at;
};

struct GuiSuggestCfg
{
    GuiSuggest *suggest;
};

typedef struct
{
    void (*init)(const GuiSuggestCfg *args);
} GuiSuggestNs;
EMBED_TABLE_LAYOUT(GuiSuggestNs, init);
EMBED_TABLE_STORAGE GuiSuggestNs gui_suggest EMBED_UNUSED = {
    .init = gui_suggest_init,
};

typedef struct
{
    int (*holds)(const GuiSuggestXCfg *args);
    int (*row_at)(const GuiSuggestXCfg *args);
} GuiSuggestXNs;
EMBED_TABLE_LAYOUT(GuiSuggestXNs, holds, row_at);
EMBED_TABLE_STORAGE GuiSuggestXNs gui_suggest_x EMBED_UNUSED = {
    .holds = gui_suggest_holds,
    .row_at = gui_suggest_row_at,
};

typedef struct
{
    void (*draw)(const GuiSuggestRasterCfg *args);
} GuiSuggestRasterNs;
EMBED_TABLE_LAYOUT(GuiSuggestRasterNs, draw);
EMBED_TABLE_STORAGE GuiSuggestRasterNs gui_suggest_raster EMBED_UNUSED = {
    .draw = gui_suggest_draw,
};

#endif
