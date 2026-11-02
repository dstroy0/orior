// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hover.h: the hover panel, as the controller publishes it: its Markdown in a box under the word's row, a rule between
// its parts. When it is asked for and when it goes are the controller's (hover.c)
#ifndef GUI_PARTS_HOVER_H
#define GUI_PARTS_HOVER_H
typedef struct GuiHoverRasterCfg GuiHoverRasterCfg;
typedef struct GuiHoverXCfg GuiHoverXCfg;
typedef struct GuiHoverCfg GuiHoverCfg;
#include "../../controller/controller_parts.h"
#include "embed_dispatch_layout.h"
#include "panel.h"

// the box's most width and height in the page's pixels, 36rem by 18rem, and its padding down and across
#define GUI_HOVER_WIDTH 576
#define GUI_HOVER_HEIGHT 288
#define GUI_HOVER_PAD_Y 7
#define GUI_HOVER_PAD_X 11

// The hover: the panel as last published, NULL for none, the controller's and read alone; and the box's rectangle where
// it was drawn last
typedef struct
{
    const CliHoverPanel *panel;
    GuiRect rect;
} GuiHover;

// a hover showing nothing
void gui_hover_init(const GuiHoverCfg *args);
// whether the pixel at x, y stands in the box shown
int gui_hover_holds(const GuiHoverXCfg *args);
// the box drawn for the word whose first cell is `anchor`, placed under the word's row inside `bounds` at `scale`, its
// parts as Markdown; the box's rectangle recorded
void gui_hover_draw(const GuiHoverRasterCfg *args);
struct GuiHoverRasterCfg
{
    GuiHover *hover;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    GuiRect anchor;
    GuiRect bounds;
    int scale;
};

struct GuiHoverXCfg
{
    int x;
    int y;
    const GuiHover *hover_read;
};

struct GuiHoverCfg
{
    GuiHover *hover;
};

typedef struct
{
    void (*init)(const GuiHoverCfg *args);
} GuiHoverNs;
EMBED_TABLE_LAYOUT(GuiHoverNs, init);
EMBED_TABLE_STORAGE GuiHoverNs gui_hover EMBED_UNUSED = {
    .init = gui_hover_init,
};

typedef struct
{
    int (*holds)(const GuiHoverXCfg *args);
} GuiHoverXNs;
EMBED_TABLE_LAYOUT(GuiHoverXNs, holds);
EMBED_TABLE_STORAGE GuiHoverXNs gui_hover_x EMBED_UNUSED = {
    .holds = gui_hover_holds,
};

typedef struct
{
    void (*draw)(const GuiHoverRasterCfg *args);
} GuiHoverRasterNs;
EMBED_TABLE_LAYOUT(GuiHoverRasterNs, draw);
EMBED_TABLE_STORAGE GuiHoverRasterNs gui_hover_raster EMBED_UNUSED = {
    .draw = gui_hover_draw,
};

#endif
