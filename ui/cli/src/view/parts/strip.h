// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// strip.h: the tool strip down the window's left edge: an icon a tool window, the top group from the strip's head,
// Explorer, Search, Structure, Commit and the more menu, and the foot group up from its foot, Run, Debug, Terminal,
// Profile, Containers, Database, Comms, Problems, Tests, Git and Runs, the tools as listed here. The icon of the window
// that shows is drawn in signal on a square of it, and the one under the pointer on the pointed ground (strip.c)
#ifndef GUI_PARTS_STRIP_H
#define GUI_PARTS_STRIP_H
#include "../frame.h"
#include "../icons.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiStripFrameRasterCfg GuiStripFrameRasterCfg;
typedef struct GuiStripFrameCfg GuiStripFrameCfg;
typedef struct GuiStripFrameXCfg GuiStripFrameXCfg;
typedef struct GuiStripCfg GuiStripCfg;
typedef struct GuiStripInitCfg GuiStripInitCfg;

// no tool; and the sizes in the page's pixels: the strip's padding at its head and foot,
// a button's side, the gap between buttons, a button's corner radius, its icon's side, and the icon's line width in
// the icon's units
#define GUI_STRIP_NONE (-1)
#define GUI_STRIP_PAD 6
#define GUI_STRIP_BUTTON 31
#define GUI_STRIP_BUTTON_GAP 4
#define GUI_STRIP_BUTTON_RADIUS 6
#define GUI_STRIP_ICON 24
#define GUI_STRIP_STROKE 1.1f

// the mark's side in the page's pixels and how far in from the button's corner it stands
#define GUI_STRIP_MARK 6
#define GUI_STRIP_MARK_IN 7

// the groups a tool stands in
#define CLI_TOOL_TOP 0u
#define CLI_TOOL_FOOTER 1u

// A tool of the strip: its name, its icon's name, its label, its keys, empty where it has none, its group, whether it
// opens a menu in place of a window of its own, and whether its window is the side pane beside the editor
typedef struct
{
    const char *name;
    const char *glyph;
    const char *label;
    const char *keys;
    unsigned int group;
    int menu;
    int side;
} CliTool;

// the tools, the top group then the foot group, and how many (strip.c)
extern const CliTool g_cli_tools[];
extern const unsigned int g_cli_tools_count;

// The strip: the tool the pointer stands on, the tool whose side window shows and the footer tool whose pane stands in
// the panel at the work's foot, each GUI_STRIP_NONE for none; whether the side tool's pane has collapsed, its width
// back to the editor, the tool unlit while it stands so; whether the Problems tool is marked, as it is while errors
// stand; how many query runs are live and how many of those have panicked, which mark the Runs tool; and whether the
// terminal's panel is open, which shows the Terminal tool, at `terminal_at` among the tools, as the tool whose window
// shows is
typedef struct
{
    int pointed;
    int displayed;
    int foot;
    int collapsed;
    int problems_marked;
    unsigned int runs_live;
    unsigned int runs_panicked;
    int terminal_open;
    unsigned int terminal_at;
} GuiStrip;

// a strip pointing at and showing no tool
void gui_strip_init(const GuiStripInitCfg *args);
// the rectangle of the tool `at`'s button in the frame's strip
GuiRect gui_strip_button(const GuiStripCfg *args);
// the tool whose button holds the pixel at x, y: 1 with its place into `at`, else 0
int gui_strip_hit(const GuiStripFrameCfg *args);
// the pointer moved to x, y: the tool it stands on marked: 1 where that changed
int gui_strip_move(const GuiStripFrameXCfg *args);
// the strip drawn: its tint and the frame's gap's, and each tool's button, the one whose window shows on its ground in
// signal, the one pointed on the pointed ground, the Problems tool's mark in the error's color while it is marked, and
// the Runs tool's in signal while a run is live and in the error's color while a live run has panicked
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

struct GuiStripInitCfg
{
    GuiStrip *strip;
};

typedef struct
{
    void (*init)(const GuiStripInitCfg *args);
} GuiStripInitNs;
EMBED_TABLE_LAYOUT(GuiStripInitNs, init);
EMBED_TABLE_STORAGE GuiStripInitNs gui_strip_table EMBED_UNUSED = {
    .init = gui_strip_init,
};

typedef struct
{
    GuiRect (*button)(const GuiStripCfg *args);
} GuiStripNs;
EMBED_TABLE_LAYOUT(GuiStripNs, button);
EMBED_TABLE_STORAGE GuiStripNs gui_strip EMBED_UNUSED = {
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
} GuiStripFrameXNs;
EMBED_TABLE_LAYOUT(GuiStripFrameXNs, move);
EMBED_TABLE_STORAGE GuiStripFrameXNs gui_strip_frame_x EMBED_UNUSED = {
    .move = gui_strip_move,
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
