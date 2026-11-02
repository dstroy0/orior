// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs.h: the tabs row above the editor: a tab for each file open, its name in the code face with a close mark after
// it, the one shown on its own ground with a line along its top and down its left; a press on a tab's name shows its
// file and one on its close mark closes it (tabs.c)
#ifndef GUI_PARTS_TABS_H
#define GUI_PARTS_TABS_H
typedef struct GuiTabsGlyphsFrameCountCfg GuiTabsGlyphsFrameCountCfg;
typedef struct GuiTabsGlyphsFrameCfg GuiTabsGlyphsFrameCfg;
typedef struct GuiTabsGlyphsCfg GuiTabsGlyphsCfg;
typedef struct GuiTabsCfg GuiTabsCfg;
#include "../../controller/controller_parts.h"
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

// the sizes in the page's pixels: a tab's text size, its padding at its left and its right, the gap between its name
// and its close mark, its corner radius, the line along the shown tab's top and left, and the close mark's padding each
// side
#define GUI_TAB_SIZE 12u
#define GUI_TAB_PAD_LEFT 14
#define GUI_TAB_PAD_RIGHT 6
#define GUI_TAB_GAP 7
#define GUI_TAB_RADIUS 6
#define GUI_TAB_LINE 2
#define GUI_TAB_CLOSE_PAD 5

// the name a tab shows, the last part of its path, into `name`, its length
const char *gui_tab_name(const GuiTabsCfg *args);
// the rectangle of the tab `at` of the `count` tabs in the frame's tabs row, and its close mark's
GuiRect gui_tab_rect(const GuiTabsGlyphsCfg *args);
GuiRect gui_tab_close_rect(const GuiTabsGlyphsCfg *args);
// the tab at x, y: 1 with its place into `at` and whether the pixel stands on its close mark into `closing`, else 0
int gui_tabs_hit(const GuiTabsGlyphsFrameCountCfg *args);
// the row drawn in the frame's tabs rectangle: its ground, the line along its foot, and each tab, the one `active`
// on its own ground with its lines, -1 for none
void gui_tabs_draw(const GuiTabsGlyphsFrameCfg *args);
struct GuiTabsGlyphsFrameCountCfg
{
    const CliTabRow *tab;
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    unsigned int count;
    int x;
    int y;
    unsigned int *at_pointer;
    int *closing;
};

struct GuiTabsGlyphsFrameCfg
{
    const CliTabRow *tab;
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    unsigned int count;
    GuiRaster *raster;
    const GuiPalette *palette;
    int active;
};

struct GuiTabsGlyphsCfg
{
    const CliTabRow *tab;
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    unsigned int count;
    unsigned int at;
};

struct GuiTabsCfg
{
    const CliTabRow *tab;
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
} GuiTabsGlyphsNs;
EMBED_TABLE_LAYOUT(GuiTabsGlyphsNs, tab_rect, tab_close_rect);
EMBED_TABLE_STORAGE GuiTabsGlyphsNs gui_tabs_glyphs EMBED_UNUSED = {
    .tab_rect = gui_tab_rect,
    .tab_close_rect = gui_tab_close_rect,
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
