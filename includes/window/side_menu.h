// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// side_menu.h: the tool side_menu down the window's left edge: an icon a tool window, the top group from the side_menu's head,
// Explorer, Search, Structure, Commit and the more menu, and the foot group up from its foot, Run, Debug, Terminal,
// Profile, Containers, Database, Comms, Problems, Tests, Git and Runs, the tools as listed here. Each tool is a bead
// of water on the side_menu's glass over its icon. The icon of a window that shows is drawn in signal, and the one under
// the pointer bright. A press on a tool flattens its bead and the release springs it back (side_menu.c)
#ifndef GUI_WINDOW_STRIP_H
#define GUI_WINDOW_STRIP_H
#include "frame.h"
#include "icons.h"
#include "palette.h"
#include "button.h"
#include "embed_dispatch_layout.h"

typedef struct GuiSideMenuFrameRasterCfg GuiSideMenuFrameRasterCfg;
typedef struct GuiSideMenuFrameCfg GuiSideMenuFrameCfg;
typedef struct GuiSideMenuFrameXCfg GuiSideMenuFrameXCfg;
typedef struct GuiSideMenuCfg GuiSideMenuCfg;
typedef struct GuiSideMenuInitCfg GuiSideMenuInitCfg;

// no tool; and the sizes in the page's pixels: the side_menu's padding at its head and foot,
// a button's side, the gap between buttons, a button's corner radius, its icon's side, and the icon's line width in
// the icon's units
#define GUI_SIDE_MENU_NONE (-1)
#define GUI_SIDE_MENU_PAD 6
#define GUI_SIDE_MENU_BUTTON 31
// the max tools the side menu holds, which the side retains a pane state for each of
#define GUI_SIDE_MENU_TOOLS_MAX 24u
#define GUI_SIDE_MENU_BUTTON_GAP 4
#define GUI_SIDE_MENU_BUTTON_RADIUS 6
#define GUI_SIDE_MENU_ICON 24
#define GUI_SIDE_MENU_STROKE 1.1f

// the mark's side in the page's pixels and how far in from the button's corner it is
#define GUI_SIDE_MENU_MARK 6
#define GUI_SIDE_MENU_MARK_IN 7

// the groups a tool is in
#define GUI_TOOL_TOP 0u
#define GUI_TOOL_FOOTER 1u

// A tool of the side_menu: its name, its icon's name, its label, its keys, empty where it has none, its group, whether it
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
} GuiTool;

// the tools, the top group then the foot group, and how many (side_menu.c)
extern const GuiTool g_gui_tools[];
extern const unsigned int g_gui_tools_count;

// The side_menu: the tool the pointer is on, the tool whose side window shows and the footer tool whose pane is in
// the panel at the work's foot, each GUI_SIDE_MENU_NONE for none; whether the side tool's pane has collapsed, its width
// back to the editor, the tool unlit while it is so; whether the Problems tool is marked, as it is while errors
// are; how many query runs are live and how many of those have panicked, which mark the Runs tool; and whether the
// terminal's panel is open, which shows the Terminal tool, at `terminal_at` among the tools, as the tool whose window
// shows is; and the tools' beads
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
    GuiButtons buttons;
} GuiSideMenu;

// a side_menu pointing at and showing no tool, its beads at rest
void gui_side_menu_init(const GuiSideMenuInitCfg *args);
// the rectangle of the tool `at`'s button in the frame's side_menu
GuiRect gui_side_menu_button(const GuiSideMenuCfg *args);
// the tool whose button holds the pixel at x, y: 1 with its place into `at`, else 0
int gui_side_menu_hit(const GuiSideMenuFrameCfg *args);
// the pointer moved to x, y: the tool it is on marked: 1 where that changed
int gui_side_menu_move(const GuiSideMenuFrameXCfg *args);
// the pointer gone from the window: no tool pointed
void gui_side_menu_leave(const GuiSideMenuFrameXCfg *args);
// a press on the tool `at` at `time_ms` flattens its bead; the press let go at `time_ms` springs it back
void gui_side_menu_press(const GuiSideMenuFrameXCfg *args);
void gui_side_menu_release(const GuiSideMenuFrameXCfg *args);
// the side_menu drawn at `time_ms`: each tool's icon, in signal where its window shows and bright under the pointer, the
// Problems tool's mark in the error's color while it is marked, the Runs tool's in signal while a run is live and in
// the error's color while a live run has panicked, and each tool's bead over its icon as its press is
void gui_side_menu_draw(const GuiSideMenuFrameRasterCfg *args);
struct GuiSideMenuFrameRasterCfg
{
    const GuiFrame *frame;
    GuiRaster *raster;
    GuiIcons *icons;
    const GuiPalette *palette;
    const GuiSideMenu *side_menu_read;
    GuiButtons *buttons;
    unsigned long long time_ms;
};

struct GuiSideMenuFrameCfg
{
    const GuiFrame *frame;
    int x;
    int y;
    unsigned int *at_pointer;
};

struct GuiSideMenuFrameXCfg
{
    GuiSideMenu *side_menu;
    const GuiFrame *frame;
    int x;
    int y;
    int at;
    unsigned long long time_ms;
};

struct GuiSideMenuCfg
{
    unsigned int at;
    const GuiFrame *frame;
    const GuiRect *strip;
};

struct GuiSideMenuInitCfg
{
    GuiSideMenu *side_menu;
};

typedef struct
{
    void (*init)(const GuiSideMenuInitCfg *args);
} GuiSideMenuInitNs;
EMBED_TABLE_LAYOUT(GuiSideMenuInitNs, init);
EMBED_TABLE_STORAGE GuiSideMenuInitNs gui_side_menu_table EMBED_UNUSED = {
    .init = gui_side_menu_init,
};

typedef struct
{
    GuiRect (*button)(const GuiSideMenuCfg *args);
} GuiSideMenuNs;
EMBED_TABLE_LAYOUT(GuiSideMenuNs, button);
EMBED_TABLE_STORAGE GuiSideMenuNs gui_side_menu EMBED_UNUSED = {
    .button = gui_side_menu_button,
};

typedef struct
{
    int (*hit)(const GuiSideMenuFrameCfg *args);
} GuiSideMenuFrameNs;
EMBED_TABLE_LAYOUT(GuiSideMenuFrameNs, hit);
EMBED_TABLE_STORAGE GuiSideMenuFrameNs gui_side_menu_frame EMBED_UNUSED = {
    .hit = gui_side_menu_hit,
};

typedef struct
{
    int (*move)(const GuiSideMenuFrameXCfg *args);
    void (*leave)(const GuiSideMenuFrameXCfg *args);
    void (*press)(const GuiSideMenuFrameXCfg *args);
    void (*release)(const GuiSideMenuFrameXCfg *args);
} GuiSideMenuFrameXNs;
EMBED_TABLE_LAYOUT(GuiSideMenuFrameXNs, move, leave, press, release);
EMBED_TABLE_STORAGE GuiSideMenuFrameXNs gui_side_menu_frame_x EMBED_UNUSED = {
    .move = gui_side_menu_move,
    .leave = gui_side_menu_leave,
    .press = gui_side_menu_press,
    .release = gui_side_menu_release,
};

typedef struct
{
    void (*draw)(const GuiSideMenuFrameRasterCfg *args);
} GuiSideMenuFrameRasterNs;
EMBED_TABLE_LAYOUT(GuiSideMenuFrameRasterNs, draw);
EMBED_TABLE_STORAGE GuiSideMenuFrameRasterNs gui_side_menu_frame_raster EMBED_UNUSED = {
    .draw = gui_side_menu_draw,
};

#endif
