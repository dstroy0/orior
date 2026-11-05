// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chrome.h: the parts about the work: the bar with the tree's name and the three window controls at its right end,
// tinted with the ink over the ground; and which control the pointer stands on (bar.c, controls.c). The menu bar's
// titles on the bar are the menu bar part's (menubar.h), the strip down the left is its own part (strip.h) and the
// status bar across the foot its own (statusbar.h)
#ifndef GUI_PARTS_CHROME_H
#define GUI_PARTS_CHROME_H
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiChromeRasterCfg GuiChromeRasterCfg;
typedef struct GuiChromeCfg GuiChromeCfg;

// the controls, left to right, and none
#define GUI_CONTROL_MINIMIZE 0u
#define GUI_CONTROL_MAXIMIZE 1u
#define GUI_CONTROL_CLOSE 2u
#define GUI_CONTROL_NONE 3u
// the interface text's size in the page's pixels, and a control's glyph's box
#define GUI_HEAD_SIZE 13u
#define GUI_CONTROL_GLYPH 14

// the bar drawn on its pane, which the frame's base lays: `title`, the tree's name, in the interface face at its
// right before the controls, and the controls, their own pane and its shadow on the `table`
void gui_bar_draw(const GuiChromeRasterCfg *args);
// the controls' own pane while it comes down from the bar's place: its shadow, saved in `shadow` once it is wanted and
// made again for a pane of another size, laid at the pane's share of the glass; a pane on the `table` thickening from
// the sheet's tint to a pane's by the frame's share of it, whole; and the three controls drawn in the frame's controls
// rectangle, minimize, maximize and close; the shadow's cell given back; and the control the pointer at x, y stands
// on, GUI_CONTROL_NONE off them all
void gui_controls_draw(const GuiChromeRasterCfg *args);
unsigned int gui_controls_hit(const GuiChromeCfg *args);
// the rectangle of the control `which`
GuiRect gui_control_rect(const GuiChromeCfg *args);
struct GuiChromeRasterCfg
{
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    const GuiFrame *frame;
    const char *title;
    GuiUnder table;
    GuiMask *shadow;
};

struct GuiChromeCfg
{
    const GuiFrame *frame;
    int x;
    int y;
    unsigned int which;
};

typedef struct
{
    void (*bar_draw)(const GuiChromeRasterCfg *args);
    void (*controls_draw)(const GuiChromeRasterCfg *args);
} GuiChromeRasterNs;
EMBED_TABLE_LAYOUT(GuiChromeRasterNs, bar_draw, controls_draw);
EMBED_TABLE_STORAGE GuiChromeRasterNs gui_chrome_raster EMBED_UNUSED = {
    .bar_draw = gui_bar_draw,
    .controls_draw = gui_controls_draw,
};

typedef struct
{
    unsigned int (*controls_hit)(const GuiChromeCfg *args);
    GuiRect (*control_rect)(const GuiChromeCfg *args);
} GuiChromeNs;
EMBED_TABLE_LAYOUT(GuiChromeNs, controls_hit, control_rect);
EMBED_TABLE_STORAGE GuiChromeNs gui_chrome EMBED_UNUSED = {
    .controls_hit = gui_controls_hit,
    .control_rect = gui_control_rect,
};

#endif
