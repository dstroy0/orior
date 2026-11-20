// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sides.h: the side pane coming and going as the page's panes do: it is with no wait while the pointer is on it or
// on the side_menu beside it, and collapses GUI_SIDES_REST_MS after the pointer leaves both, its width going back to the
// editor, unless at that moment the pointer is back on one of them or a menu is open, when it is looked at again a
// rest later; collapsed, the pointer resting GUI_SIDES_EDGE_REST_MS
// on the view's left edge, GUI_SIDES_EDGE pixels wide, and off it no longer than GUI_SIDES_EDGE_GRACE_MS at a time,
// brings it back; a press on a side tool's icon shows its pane, or collapses it where it shows; and the pointer
// resting GUI_SIDES_ICON_REST_MS on a side tool's icon shows that tool's pane as a preview, which a press on the icon
// or in the pane ends with the pane located, and which the pointer leaving the side_menu and the pane without one takes
// back to the tool and the pane that showed before it. The tool displayed and whether its pane has collapsed are the
// side_menu's (sides.c)
#ifndef GUI_SIDES_H
#define GUI_SIDES_H
#include "claim.h"
#include "embed_dispatch_layout.h"
#include "frame.h"
#include "side_menu.h"

typedef struct GuiSidesCfg GuiSidesCfg;

// the times in milliseconds:
//   - the pointer away before the pane collapses.
//   - its rest on the edge before the pane shows.
//   - how long it can be off the edge and still rest on it.
//   - its rest on an icon before the pane of the icon shows.
//   - the slide of the pane, up and toward the left on the ease of the page, as style.css sets it.
//
// Then the width of the edge in the pixels of the page
#define GUI_SIDES_REST_MS 700ULL
#define GUI_SIDES_EDGE_REST_MS 250ULL
#define GUI_SIDES_EDGE_GRACE_MS 120ULL
#define GUI_SIDES_ICON_REST_MS 350ULL
#define GUI_SIDES_SLIDE_MS 220ULL
#define GUI_SIDES_EDGE 12

// The waits of the pane:
//   - whether the pointer is on the pane, on the side_menu and on the edge.
//   - when the code next looks at the pane for a collapse, 0 for no wait.
//   - when the rest of the pointer on the edge shows the pane, 0 for none.
//   - when its rest on the edge ends while it is off the edge, 0 for none.
//   - the icon that the pointer rests on, GUI_SIDE_MENU_NONE for none, and when its pane shows, 0 for none.
//   - whether a preview exists, the icon that it shows, and the tool and the collapse of the pane before it.
//   - its slide: the share of its way gone, from and to, GUI_FIXED_ONE gone, over GUI_SIDES_SLIDE_MS on the ease of
//     the page. It is at rest where its two ends are the same.
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
    GuiSlide slide;
} GuiSides;

// the sides with no wait, rest or preview
void gui_sides_init(const GuiSidesCfg *args);
// the pane looked at to collapse GUI_SIDES_REST_MS after `time_ms`, as the window begins
void gui_sides_settle(const GuiSidesCfg *args);
// a press on the icon of the side tool `at`, or its keys. Where its pane shows, the pane collapses, and else it shows.
// The tool `at` becomes the tool of the side_menu, at rest with no wait until the pointer leaves it or the side_menu. A
// preview ends, with its pane at rest
void gui_sides_toggle(const GuiSidesCfg *args);
// the pointer moved to x, y at `time_ms`. Where it is begins and ends the waits and rests of the pane. A preview that
// it leaves goes back. The result is 1 where the displayed pane changed
int gui_sides_move(const GuiSidesCfg *args);
// the pointer gone from the window at `time_ms`, as it leaves the pane, the side_menu and the edge
void gui_sides_leave(const GuiSidesCfg *args);
// a press at x, y. Where the press is on an icon or in the pane, a preview ends with its pane at rest. The result is 1
// where the press is on the icon that the preview shows, and the press then does no more
int gui_sides_press(const GuiSidesCfg *args);
// the waits that `time_ms` reaches, run:
//   - the pane collapses where the pointer is on neither it nor the side_menu, and no menu is open.
//   - a rest on the edge shows the pane.
//   - a rest on the icon of a tool shows its pane as a preview.
//
// The result is 1 where the displayed pane changed
int gui_sides_tick(const GuiSidesCfg *args);
// when the next wait ends, 0 for none
unsigned long long gui_sides_next_deadline_ms(const GuiSidesCfg *args);
// the share of its way that the pane went at `time_ms`, GUI_FIXED_ONE gone, 0 at rest. Then whether the side is in the
// work at `time_ms` for the tool `side_menu_read` of the side_menu, 1 or 0. A pane in its collapse keeps its width until
// its slide ends. Then when the slide ends, 0 where none runs at `time_ms`
int gui_sides_away(const GuiSidesCfg *args);
int gui_sides_is(const GuiSidesCfg *args);
unsigned long long gui_sides_slide_end_ms(const GuiSidesCfg *args);
struct GuiSidesCfg
{
    GuiSides *sides;
    const GuiSides *sides_read;
    GuiSideMenu *side_menu;
    const GuiSideMenu *side_menu_read;
    const GuiFrame *frame;
    unsigned int at;
    int x;
    int y;
    int menu_open;
    int still;
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
    int (*away)(const GuiSidesCfg *args);
    int (*is)(const GuiSidesCfg *args);
    unsigned long long (*slide_end_ms)(const GuiSidesCfg *args);
} GuiSidesNs;
EMBED_TABLE_LAYOUT(GuiSidesNs, init, settle, toggle, move, leave, press, tick, next_deadline_ms, away, is,
                   slide_end_ms);
EMBED_TABLE_STORAGE GuiSidesNs gui_sides EMBED_UNUSED = {
    .init = gui_sides_init,
    .settle = gui_sides_settle,
    .toggle = gui_sides_toggle,
    .move = gui_sides_move,
    .leave = gui_sides_leave,
    .press = gui_sides_press,
    .tick = gui_sides_tick,
    .next_deadline_ms = gui_sides_next_deadline_ms,
    .away = gui_sides_away,
    .is = gui_sides_is,
    .slide_end_ms = gui_sides_slide_end_ms,
};

#endif
