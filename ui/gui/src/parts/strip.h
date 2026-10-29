// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// strip.h: the tool strip down the window's left edge: an icon a tool window, the top group from the strip's head,
// Explorer, Search, Structure, Commit and the more menu, and the foot group up from its foot, Run, Debug, Terminal,
// Profile, Containers, Database, Comms, Problems, Tests, Git and Runs. A press on a tool shows its window and a press
// on the one shown closes it, as its keys do; the icon of the window that shows is drawn in signal on a square of it,
// and the one under the pointer on the pointed ground (strip.c)
#ifndef GUI_PARTS_STRIP_H
#define GUI_PARTS_STRIP_H
#include "../frame.h"
#include "../icons.h"
#include "../palette.h"

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

// The strip: the tool the pointer stands on and the tool whose window shows, each GUI_STRIP_NONE for none
typedef struct
{
    int pointed;
    int shown;
} GuiStrip;

// a strip pointing at and showing no tool
void gui_strip_made(GuiStrip *strip);
// how many tools there are, and the tool `at`
unsigned int gui_strip_tools(void);
const GuiTool *gui_strip_tool(unsigned int at);
// the rectangle of the tool `at`'s button in the frame's strip
GuiRect gui_strip_button(const GuiFrame *frame, unsigned int at);
// the tool whose button holds the pixel at x, y: 1 with its place into `at`, else 0
int gui_strip_hit(const GuiFrame *frame, int x, int y, unsigned int *at);
// the pointer moved to x, y: the tool it stands on marked: 1 where that changed
int gui_strip_moved(GuiStrip *strip, const GuiFrame *frame, int x, int y);
// a press at x, y: the tool pressed shown, or closed where it was the one shown: 1 where the press was the strip's
int gui_strip_pressed(GuiStrip *strip, const GuiFrame *frame, int x, int y);
// a key named as the page's editor names it: the tool whose keys it is shown, or closed where it was the one shown:
// 1 where a tool has the keys
int gui_strip_keyed(GuiStrip *strip, const char *name);
// the tool `at` shown, or closed where it was the one shown; a tool that opens a menu changes nothing
void gui_strip_toggled(GuiStrip *strip, unsigned int at);
// the strip drawn: its tint and the frame's gap's, and each tool's button, the one shown on its ground in signal,
// the one pointed on the pointed ground
void gui_strip_drawn(GuiRaster *raster, GuiIcons *icons, const GuiPalette *palette, const GuiFrame *frame,
                     const GuiStrip *strip);
#endif
