// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// top_menu.h: the top menu, the row across the window's head: its pane tinted with the ink over the ground, the
// tree's name, and its four tools, Preferences, Notifications, Toolchains and Definitions, each a bead of water over
// its icon; and which tool the pointer stands on (top_menu.c). The menu bar's titles on the row are the menu bar's
// (menubar.h) and the window controls at its right end are their own (controls.h)
#ifndef GUI_WINDOW_TOP_MENU_H
#define GUI_WINDOW_TOP_MENU_H
#include "frame.h"
#include "glyphs.h"
#include "icons.h"
#include "palette.h"
#include "button.h"
#include "embed_dispatch_layout.h"

typedef struct GuiTopMenuRasterCfg GuiTopMenuRasterCfg;
typedef struct GuiTopMenuCfg GuiTopMenuCfg;

// the interface text's size in the page's pixels
#define GUI_HEAD_SIZE 13u

// the top menu's tools, left to right, and none; and their sizes in the page's pixels: a tool's icon's side, the
// icon's line width in the icon's units, a letter icon's size, the unread mark's side and how far in from the
// button's corner the mark stands
#define GUI_TOP_MENU_TOOL_PREFERENCES 0u
#define GUI_TOP_MENU_TOOL_NOTIFICATIONS 1u
#define GUI_TOP_MENU_TOOL_TOOLCHAINS 2u
#define GUI_TOP_MENU_TOOL_DEFINITIONS 3u
#define GUI_TOP_MENU_TOOL_NONE 4u
#define GUI_TOP_MENU_TOOL_ICON 18
#define GUI_TOP_MENU_TOOL_STROKE 1.4f
#define GUI_TOP_MENU_TOOL_LETTER 16u
#define GUI_TOP_MENU_TOOL_MARK 6
#define GUI_TOP_MENU_TOOL_MARK_IN 5

// A tool of the top menu: the command it runs, its icon's name, a letter where the name is one character, its label
// and its keys, empty where it has none
typedef struct
{
    const char *name;
    const char *glyph;
    const char *label;
    const char *keys;
} GuiTopMenuTool;

// the top menu's tools, left to right (top_menu.c)
extern const GuiTopMenuTool g_gui_top_menu_tools[GUI_TOP_MENU_TOOLS];

// The top menu drawn at `time_ms` on its pane, which the frame's base lays: `title`, the tree's name, in the
// interface face at its right before the tools; the tools, each its icon on the glass, the one `pointed` in the
// bright color, the notifications tool marked while `unread` is 1, and each tool's bead over its icon, the beads'
// atlas laid out at the tools' rest places and each tool's frames baked the first time it is drawn
void gui_top_menu_draw(const GuiTopMenuRasterCfg *args);
// the rectangle of the tool `which`; and the tool the pointer at x, y stands on, GUI_TOP_MENU_TOOL_NONE off them all
GuiRect gui_top_menu_tool_rect(const GuiTopMenuCfg *args);
unsigned int gui_top_menu_tools_hit(const GuiTopMenuCfg *args);
struct GuiTopMenuRasterCfg
{
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    GuiIcons *icons;
    const GuiPalette *palette;
    const GuiFrame *frame;
    const char *title;
    GuiButtons *buttons;
    unsigned long long time_ms;
    unsigned int pointed;
    int unread;
};

struct GuiTopMenuCfg
{
    const GuiFrame *frame;
    const GuiRect *tools;
    int x;
    int y;
    unsigned int which;
};

typedef struct
{
    void (*draw)(const GuiTopMenuRasterCfg *args);
} GuiTopMenuRasterNs;
EMBED_TABLE_LAYOUT(GuiTopMenuRasterNs, draw);
EMBED_TABLE_STORAGE GuiTopMenuRasterNs gui_top_menu_raster EMBED_UNUSED = {
    .draw = gui_top_menu_draw,
};

typedef struct
{
    unsigned int (*tools_hit)(const GuiTopMenuCfg *args);
    GuiRect (*tool_rect)(const GuiTopMenuCfg *args);
} GuiTopMenuNs;
EMBED_TABLE_LAYOUT(GuiTopMenuNs, tools_hit, tool_rect);
EMBED_TABLE_STORAGE GuiTopMenuNs gui_top_menu EMBED_UNUSED = {
    .tools_hit = gui_top_menu_tools_hit,
    .tool_rect = gui_top_menu_tool_rect,
};

#endif
