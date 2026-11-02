// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// strip.h: the tool strip down the window's left edge: an icon a tool window, the top group from the strip's head,
// Explorer, Search, Structure, Commit and the more menu, and the foot group up from its foot, Run, Debug, Terminal,
// Profile, Containers, Database, Comms, Problems, Tests, Git and Runs. A press on a tool shows its window and a press
// on the one shown closes it, as its keys do; the icon of the window that shows is drawn in signal on a square of it,
// and the one under the pointer on the pointed ground (strip.c)
#ifndef GUI_PARTS_STRIP_H
#define GUI_PARTS_STRIP_H
typedef struct GuiStripFrameRasterCfg GuiStripFrameRasterCfg;
typedef struct GuiStripFrameCfg GuiStripFrameCfg;
typedef struct GuiStripFrameXCfg GuiStripFrameXCfg;
typedef struct GuiStripCfg GuiStripCfg;
typedef struct GuiStripNameCfg GuiStripNameCfg;
typedef struct GuiStripAtCfg GuiStripAtCfg;
#include "../frame.h"
#include "../icons.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

// the groups a tool stands in; no tool; and the sizes in the page's pixels: the strip's padding at its head and foot,
// a button's side, the gap between buttons, a button's corner radius, its icon's side, and the icon's line width in
// the icon's units
#define GUI_STRIP_TOP 0u
#define GUI_STRIP_FOOT 1u
#define GUI_STRIP_NONE (-1)
#define GUI_STRIP_PAD 6
#define GUI_STRIP_BUTTON 38
#define GUI_STRIP_BUTTON_GAP 4
#define GUI_STRIP_BUTTON_RADIUS 7
#define GUI_STRIP_ICON 29
#define GUI_STRIP_STROKE 1.1f

// A tool: its name, its icon's name, its label, its keys as the page names them, empty where it has none, its
// group, whether it opens a menu in place of a window of its own, and whether its window is the side pane beside the
// editor
typedef struct
{
    const char *name;
    const char *glyph;
    const char *label;
    const char *keys;
    unsigned int group;
    int menu;
    int side;
} GuiTool;

// the mark's side in the page's pixels and how far in from the button's corner it stands
#define GUI_STRIP_MARK 6
#define GUI_STRIP_MARK_IN 7

// The strip: the tool the pointer stands on and the tool whose window shows, each GUI_STRIP_NONE for none; and
// whether the Problems tool is marked, as it is while errors stand
typedef struct
{
    int pointed;
    int shown;
    int problems_marked;
} GuiStrip;

// a strip pointing at and showing no tool
void gui_strip_init(const GuiStripNameCfg *args);
// how many tools there are, and the tool `at`
unsigned int gui_strip_tools(void);
const GuiTool *gui_strip_tool(const GuiStripCfg *args);
// the rectangle of the tool `at`'s button in the frame's strip
GuiRect gui_strip_button(const GuiStripCfg *args);
// the tool whose button holds the pixel at x, y: 1 with its place into `at`, else 0
int gui_strip_hit(const GuiStripFrameCfg *args);
// the pointer moved to x, y: the tool it stands on marked: 1 where that changed
int gui_strip_move(const GuiStripFrameXCfg *args);
// a press at x, y: the tool pressed shown, or closed where it was the one shown: 1 where the press was the strip's
int gui_strip_press(const GuiStripFrameXCfg *args);
// a key named as the page's editor names it: the tool whose keys it is shown, or closed where it was the one shown:
// 1 where a tool has the keys
int gui_strip_key(const GuiStripNameCfg *args);
// the tool `at` shown, or closed where it was the one shown; a tool that opens a menu changes nothing
void gui_strip_toggle(const GuiStripAtCfg *args);
// the strip drawn: its tint and the frame's gap's, and each tool's button, the one shown on its ground in signal,
// the one pointed on the pointed ground, and the Problems tool's mark in the error's color while it is marked
void gui_strip_draw(const GuiStripFrameRasterCfg *args);
struct GuiStripFrameRasterCfg
{
    const GuiFrame *frame;
    GuiRaster *raster;
    GuiIcons *icons;
    const GuiPalette *palette;
    const GuiStrip *strip_read;
};

struct GuiStripFrameCfg
{
    const GuiFrame *frame;
    int x;
    int y;
    unsigned int *at_pointer;
};

struct GuiStripFrameXCfg
{
    GuiStrip *strip;
    const GuiFrame *frame;
    int x;
    int y;
};

struct GuiStripCfg
{
    unsigned int at;
    const GuiFrame *frame;
};

struct GuiStripNameCfg
{
    GuiStrip *strip;
    const char *name;
};

struct GuiStripAtCfg
{
    GuiStrip *strip;
    unsigned int at;
};

typedef struct
{
    void (*init)(const GuiStripNameCfg *args);
    int (*key)(const GuiStripNameCfg *args);
} GuiStripNameNs;
EMBED_TABLE_LAYOUT(GuiStripNameNs, init, key);
EMBED_TABLE_STORAGE GuiStripNameNs gui_strip_name EMBED_UNUSED = {
    .init = gui_strip_init,
    .key = gui_strip_key,
};

typedef struct
{
    const GuiTool *(*tool)(const GuiStripCfg *args);
    GuiRect (*button)(const GuiStripCfg *args);
} GuiStripNs;
EMBED_TABLE_LAYOUT(GuiStripNs, tool, button);
EMBED_TABLE_STORAGE GuiStripNs gui_strip EMBED_UNUSED = {
    .tool = gui_strip_tool,
    .button = gui_strip_button,
};

typedef struct
{
    int (*hit)(const GuiStripFrameCfg *args);
} GuiStripFrameNs;
EMBED_TABLE_LAYOUT(GuiStripFrameNs, hit);
EMBED_TABLE_STORAGE GuiStripFrameNs gui_strip_frame EMBED_UNUSED = {
    .hit = gui_strip_hit,
};

typedef struct
{
    int (*move)(const GuiStripFrameXCfg *args);
    int (*press)(const GuiStripFrameXCfg *args);
} GuiStripFrameXNs;
EMBED_TABLE_LAYOUT(GuiStripFrameXNs, move, press);
EMBED_TABLE_STORAGE GuiStripFrameXNs gui_strip_frame_x EMBED_UNUSED = {
    .move = gui_strip_move,
    .press = gui_strip_press,
};

typedef struct
{
    void (*toggle)(const GuiStripAtCfg *args);
} GuiStripAtNs;
EMBED_TABLE_LAYOUT(GuiStripAtNs, toggle);
EMBED_TABLE_STORAGE GuiStripAtNs gui_strip_at EMBED_UNUSED = {
    .toggle = gui_strip_toggle,
};

typedef struct
{
    void (*draw)(const GuiStripFrameRasterCfg *args);
} GuiStripFrameRasterNs;
EMBED_TABLE_LAYOUT(GuiStripFrameRasterNs, draw);
EMBED_TABLE_STORAGE GuiStripFrameRasterNs gui_strip_frame_raster EMBED_UNUSED = {
    .draw = gui_strip_draw,
};

#endif
