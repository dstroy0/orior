// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sides.h: the side pane coming and going as the page's panes do: it collapses GUI_SIDES_REST_MS after the pointer
// leaves it, its width going back to the editor, unless the pointer stands on the strip beside it or a menu is open,
// each looked at again every GUI_SIDES_REST_MS while it stands; collapsed, the pointer resting GUI_SIDES_EDGE_REST_MS
// on the view's left edge, GUI_SIDES_EDGE pixels wide, and off it no longer than GUI_SIDES_EDGE_GRACE_MS at a time,
// brings it back; a press on a side tool's icon shows its pane, or collapses it where it shows; and the pointer
// resting GUI_SIDES_ICON_REST_MS on a side tool's icon shows that tool's pane as a preview, which a press on the icon
// or in the pane ends with the pane standing, and which the pointer leaving the strip and the pane without one takes
// back to the tool and the pane that showed before it. The tool shown and whether its pane has collapsed are the
// strip's (sides.c)
#ifndef GUI_SIDES_H
#define GUI_SIDES_H
#include "embed_dispatch_layout.h"
#include "frame.h"
#include "parts/strip.h"

typedef struct GuiSidesCfg GuiSidesCfg;

// the times in milliseconds: the pointer away before the pane collapses, its rest on the edge before the pane shows,
// how long it may be off the edge and still rest on it, and its rest on an icon before the icon's pane shows; and the
// edge's width in the page's pixels
#define GUI_SIDES_REST_MS 700ULL
#define GUI_SIDES_EDGE_REST_MS 250ULL
#define GUI_SIDES_EDGE_GRACE_MS 120ULL
#define GUI_SIDES_ICON_REST_MS 350ULL
#define GUI_SIDES_EDGE 12

// The pane's waits: whether the pointer stands on the pane, on the strip and on the edge; when the pane is looked at to
// collapse, 0 for no wait; when the pointer's rest on the edge shows the pane, 0 for none; when its rest on the edge
// ends while it is off the edge, 0 for none; the icon the pointer rests on, GUI_STRIP_NONE for none, and when its pane
// shows, 0 for none; and whether a preview stands, the icon it shows and the tool and the pane's collapse before it
typedef struct
{
    int over_pane;
    int over_strip;
    int over_edge;
    unsigned long long collapse_ms;
    unsigned long long edge_ms;
    unsigned long long edge_off_ms;
    int icon;
    unsigned long long icon_ms;
    int previewing;
    int previewed;
    int before_displayed;
    int before_collapsed;
} GuiSides;

// no wait, no rest and no preview
void gui_sides_init(const GuiSidesCfg *args);
// the pane looked at to collapse GUI_SIDES_REST_MS after `time_ms`, as the window begins
void gui_sides_settle(const GuiSidesCfg *args);
// a press on the side tool `at`'s icon, or its keys: its pane collapsed where it shows, else shown, the tool `at` the
// strip's, standing while the pointer is on it or on the strip, and any preview ended with its pane standing
void gui_sides_toggle(const GuiSidesCfg *args);
// the pointer moved to x, y at `time_ms`: the pane's waits and rests begun and ended by where it stands, and a preview
// it leaves taken back: 1 where the pane shown changed
int gui_sides_move(const GuiSidesCfg *args);
// the pointer gone from the window at `time_ms`, as it leaves the pane, the strip and the edge
void gui_sides_leave(const GuiSidesCfg *args);
// a press at x, y: a preview ended with its pane standing where the press is on an icon or in the pane: 1 where the
// press is on the icon the preview shows, which does nothing more
int gui_sides_press(const GuiSidesCfg *args);
// the waits reached by `time_ms` run: the pane collapsed where the pointer stands on neither it nor the strip and no
// menu is open, shown by a rest on the edge, or a tool's pane shown as a preview by a rest on its icon: 1 where the
// pane shown changed
int gui_sides_tick(const GuiSidesCfg *args);
// when the next wait ends, 0 for none
unsigned long long gui_sides_next_deadline_ms(const GuiSidesCfg *args);
struct GuiSidesCfg
{
    GuiSides *sides;
    const GuiSides *sides_read;
    GuiStrip *strip;
    const GuiFrame *frame;
    unsigned int at;
    int x;
    int y;
    int menu_open;
    unsigned long long time_ms;
};

typedef struct
{
    void (*init)(const GuiSidesCfg *args);
    void (*settle)(const GuiSidesCfg *args);
    void (*toggle)(const GuiSidesCfg *args);
    int (*move)(const GuiSidesCfg *args);
    void (*leave)(const GuiSidesCfg *args);
    int (*press)(const GuiSidesCfg *args);
    int (*tick)(const GuiSidesCfg *args);
    unsigned long long (*next_deadline_ms)(const GuiSidesCfg *args);
} GuiSidesNs;
EMBED_TABLE_LAYOUT(GuiSidesNs, init, settle, toggle, move, leave, press, tick, next_deadline_ms);
EMBED_TABLE_STORAGE GuiSidesNs gui_sides EMBED_UNUSED = {
    .init = gui_sides_init,
    .settle = gui_sides_settle,
    .toggle = gui_sides_toggle,
    .move = gui_sides_move,
    .leave = gui_sides_leave,
    .press = gui_sides_press,
    .tick = gui_sides_tick,
    .next_deadline_ms = gui_sides_next_deadline_ms,
};

#endif
