// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs.h: the tabs row above the editor: a tab for each view open, read where the views stand (editor/view.h), its
// file's name in the code face with a close mark after it, ● before the name in the hover's color while its text has
// changes not saved and ×? for the mark while its close waits on a second, the one shown on its own ground with a line
// along its top and down its left; a press on a tab's name shows its file and one on its close mark closes it (tabs.c)
#ifndef GUI_PARTS_TABS_H
#define GUI_PARTS_TABS_H
#include "../../editor/view.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiTabsGlyphsFrameCountCfg GuiTabsGlyphsFrameCountCfg;
typedef struct GuiTabsGlyphsFrameCfg GuiTabsGlyphsFrameCfg;
typedef struct GuiTabsGlyphsCfg GuiTabsGlyphsCfg;
typedef struct GuiTabsCfg GuiTabsCfg;

// The sizes in the page's pixels: a tab's text size, its padding at its left and its right, and the gap between its
// name and its close mark. Then its top corners' radius, the line along the shown tab's top and left, and the close
// mark's padding each side. Last, the room the row keeps clear of the work's lines above the tabs and before the first
#define GUI_TAB_SIZE 12u
#define GUI_TAB_PAD_LEFT 14
#define GUI_TAB_PAD_RIGHT 6
#define GUI_TAB_GAP 7
#define GUI_TAB_RADIUS 6
#define GUI_TAB_LINE 2
#define GUI_TAB_CLOSE_PAD 5
#define GUI_TAB_CLEAR 3
// the space between one tab's box and the next
#define GUI_TAB_SPACE 2
// the pixels the tabs row moves for an arrow of its scrollbar and for one step of the wheel
#define GUI_TABS_STEP 40

// the name the tab `at` shows, the last part of its file's path, its length into `length`
const char *gui_tab_name(const GuiTabsCfg *args);
// the rectangle of the tab `at` in the frame's tabs row, and its close mark's
GuiRect gui_tab_rect(const GuiTabsGlyphsCfg *args);
GuiRect gui_tab_close_rect(const GuiTabsGlyphsCfg *args);
// the width of the strip of all tabs, from the row's left edge with the strip at its start, in pixels; `at` is not read
int gui_tabs_whole(const GuiTabsGlyphsCfg *args);
// the tab at x, y: 1 with its place into `at` and whether the pixel stands on its close mark into `closing`, else 0
int gui_tabs_hit(const GuiTabsGlyphsFrameCountCfg *args);
// the row drawn in the frame's tabs rectangle: its ground, the line along its foot, and each tab, the one shown on its
// own ground with its lines
void gui_tabs_draw(const GuiTabsGlyphsFrameCfg *args);
struct GuiTabsGlyphsFrameCountCfg
{
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    int x;
    int y;
    unsigned int *at_pointer;
    int *closing;
};

struct GuiTabsGlyphsFrameCfg
{
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    GuiRaster *raster;
    const GuiPalette *palette;
};

struct GuiTabsGlyphsCfg
{
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    unsigned int at;
};

struct GuiTabsCfg
{
    unsigned int at;
    unsigned long long *length;
};

typedef struct
{
    const char *(*tab_name)(const GuiTabsCfg *args);
} GuiTabsNs;
EMBED_TABLE_LAYOUT(GuiTabsNs, tab_name);
EMBED_TABLE_STORAGE GuiTabsNs gui_tabs EMBED_UNUSED = {
    .tab_name = gui_tab_name,
};

typedef struct
{
    GuiRect (*tab_rect)(const GuiTabsGlyphsCfg *args);
    GuiRect (*tab_close_rect)(const GuiTabsGlyphsCfg *args);
    int (*whole)(const GuiTabsGlyphsCfg *args);
} GuiTabsGlyphsNs;
EMBED_TABLE_LAYOUT(GuiTabsGlyphsNs, tab_rect, tab_close_rect, whole);
EMBED_TABLE_STORAGE GuiTabsGlyphsNs gui_tabs_glyphs EMBED_UNUSED = {
    .tab_rect = gui_tab_rect,
    .tab_close_rect = gui_tab_close_rect,
    .whole = gui_tabs_whole,
};

typedef struct
{
    int (*hit)(const GuiTabsGlyphsFrameCountCfg *args);
} GuiTabsGlyphsFrameCountNs;
EMBED_TABLE_LAYOUT(GuiTabsGlyphsFrameCountNs, hit);
EMBED_TABLE_STORAGE GuiTabsGlyphsFrameCountNs gui_tabs_glyphs_frame_count EMBED_UNUSED = {
    .hit = gui_tabs_hit,
};

typedef struct
{
    void (*draw)(const GuiTabsGlyphsFrameCfg *args);
} GuiTabsGlyphsFrameNs;
EMBED_TABLE_LAYOUT(GuiTabsGlyphsFrameNs, draw);
EMBED_TABLE_STORAGE GuiTabsGlyphsFrameNs gui_tabs_glyphs_frame EMBED_UNUSED = {
    .draw = gui_tabs_draw,
};

#endif
