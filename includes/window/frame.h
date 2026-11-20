// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.h: the parts and their rectangles for the window's size and the system's scale, as the page positions them out:
// the bar across the top with the three controls at its right end, the status bar across the foot, the side_menu down the
// left and the frame's gap down the right, and the work between them, its inside the work less its padding, a side
// pane at the inside's left where one is displayed, its tabs row at its head past the side, the minimap's column at its
// right under the tabs row and the editor filling the rest; every size the page's pixels times the scale (frame.c)
#ifndef GUI_FRAME_H
#define GUI_FRAME_H
#include "embed_dispatch_layout.h"
#include "../raster/raster.h"

typedef struct GuiFrameScaleCfg GuiFrameScaleCfg;
typedef struct GuiFrameCfg GuiFrameCfg;
typedef struct GuiFrameHolesCfg GuiFrameHolesCfg;
typedef struct GuiFrameDisplayedCfg GuiFrameDisplayedCfg;
typedef struct GuiFramePixelsCfg GuiFramePixelsCfg;

// the sizes of the parts in the pixels of the page:
//   - the height of the bar, --bar-height, and the height of the status bar.
//   - the width of the side_menu, --side_menu-width, and the gap of the frame at the right, --frame-right.
//   - the width of the side pane, --side, and the height of the tabs row, --tabs-tall.
//   - the width of the minimap, .ed-mini.
//   - the padding of the work, top, right, bottom and left, and the radius of its corners.
//   - the width of a window control, and the gap between the controls.
//   - how far their row ends from the right end of the bar. That is the gap of the frame, and the right edge of the
//     close control is then on the right edge of the work.
#define GUI_TOP_MENU_HEIGHT 42
#define GUI_STATUS_HEIGHT 22
#define GUI_SIDE_MENU_WIDTH 47
#define GUI_FRAME_GAP 8
#define GUI_SIDE_WIDTH 304
#define GUI_TABS_HEIGHT 31
// the pixels between the work's border line and the tabs row under it: the beads stay clear of the line
#define GUI_TABS_GAP 3
#define GUI_MAP_WIDTH 84
#define GUI_WORK_PAD_TOP 2
#define GUI_WORK_PAD_RIGHT 2
#define GUI_WORK_PAD_BOTTOM 1
#define GUI_WORK_PAD_LEFT 1
#define GUI_WORK_RADIUS 7
#define GUI_CONTROL_WIDTH 30
#define GUI_CONTROL_GAP 2
#define GUI_CONTROLS_END 8
#define GUI_CONTROLS 3

// the controls' tab's lip in the page's pixels, how far its glass reaches left of the controls. And how many gaps in
// from the work's edge a part's hole snaps to the edge
#define GUI_CONTROLS_BEVEL 8
#define GUI_HOLE_SNAP 2

// the tools of the bar, left of the controls, in the pixels of the page. They are the side of a button, the gap between
// buttons and how many buttons there are. Last is the gap between them and the controls, the 0.7rem of the bar
#define GUI_TOP_MENU_TOOL 30
#define GUI_TOP_MENU_TOOL_GAP 2
#define GUI_TOP_MENU_TOOLS 4
#define GUI_TOP_MENU_TOOLS_GAP 11

// the panel of the terminal in the pixels of the page:
//   - its least height, min-height.
//   - the least that it leaves the views above it, the 16rem of its max-height.
//   - the most share of the height of the window that it reaches, in percent, the 75vh of its max-height.
//   - its top line, and its side_menu of tabs under that, with the foot line of the side_menu.
//   - how far below its place the panel starts as it shows. It then comes up to its place.
#define GUI_TERMINAL_MIN 96
#define GUI_TERMINAL_SHELL_MIN 256
#define GUI_TERMINAL_MAX_PERCENT 75
#define GUI_TERMINAL_LINE 1
#define GUI_TERMINAL_STRIP 27
#define GUI_TERMINAL_ARRIVES 14
// how far either side of the panel's top line its grip reaches, the 6px grip set 3px above it
#define GUI_TERMINAL_GRIP 3

// the preview's modes as the frame positions them, the numbers of the CLI's CLI_PREVIEW_OFF, CLI_PREVIEW_BESIDE and
// CLI_PREVIEW_ALONE
#define GUI_FRAME_PREVIEW_OFF 0
#define GUI_FRAME_PREVIEW_BESIDE 1
#define GUI_FRAME_PREVIEW_ALONE 2

// a number of 1 in the frame's arithmetic: every share the parts move on is a whole word, 16 bits of it a fraction
#define GUI_FIXED_ONE 65536
// the parts where the share of a part that fades can be, in sixteenths. The base positions the ground of the part at
// these parts, and its colors fade by them. Each stage of a fade then positions the colors that each earlier fade
// positioned
#define GUI_FRAME_PARTS 16
// `color_` at `share_` of its alpha, a number of GUI_FIXED_ONE from 0 to it, rounded. That is the color of a part that
// fades. At a whole share it is the color itself, with no arithmetic
#define GUI_FRAME_FADED(color_, share_)                                                                                \
    (((share_) >= GUI_FIXED_ONE)                                                                                       \
         ? (GuiColor)(color_)                                                                                          \
         : (((color_) & 0x00ffffffu) |                                                                                 \
            ((GuiColor)((((unsigned long long)GUI_ALPHA_OF(color_) * (unsigned long long)(share_)) +                   \
                         (unsigned long long)(GUI_FIXED_ONE / 2)) /                                                    \
                        (unsigned long long)GUI_FIXED_ONE)                                                             \
             << 24u)))

// The numbers of the claim, each 0 to GUI_FIXED_ONE, as the claim of the page runs them:
//   - the claim, GUI_FIXED_ONE while the work takes the inset of the parts of the edges.
//   - the reveal of each edge, 1 while its part is back.
//   - the way down of the controls from the place of the bar, while the bar is away.
//   - the inset of each edge, 1 after the work gives the part its inset.
//   - the grown share of each slat along its edge, and the opacity of the controls.
//   - the point along each edge that its slat pulls from, in pixels, -1 for the middle.
//   - the reveals of the head, the foot, the left and the controls, read again one lag behind. The far edge of a pane
//     trails at that lag.
typedef struct
{
    int claim;
    int reveal_top;
    int reveal_bottom;
    int reveal_left;
    int reveal_right;
    int reveal_tabs;
    int reveal_down;
    int inset_top;
    int inset_left;
    int inset_bottom;
    int grown_top;
    int grown_left;
    int grown_bottom;
    int controls_alpha;
    int pulled_top;
    int pulled_left;
    int pulled_bottom;
    int lag_top;
    int lag_bottom;
    int lag_left;
    int lag_down;
} GuiAutohideNumbers;

// The four edges of a pane in the words of the frame, each a count of pixels with sixteen bits of fraction. They show
// where the tint of the pane runs. An edge between two pixel rows or columns goes at its share of the row or column
// that holds it
typedef struct
{
    int left;
    int top;
    int right;
    int bottom;
} GuiEdges;

// The frame. It holds:
//   - the size and scale of the window of its layout, and whether a side shows.
//   - the requested height of the terminal in the pixels of the page, 0 while its panel does not show.
//   - the numbers of the claim that it positions for.
//   - the rectangle of each part. The side has no width where none shows.
//   - the panel of the terminal at the foot of the inside, under both, with its views under its side_menu. It has none
//     while the panel does not show.
//   - the rectangle of the map at the right of the editor.
//   - the rectangles of the bar, the side_menu and the status bar, off the window while the claim holds them away.
//   - each of those three as the slat that its edge shows. That is the rectangle of the part, cut to its grown share.
//     It spans from the place of the part to its place at the lagged number of its reveal.
//   - the edges of the pane of each slat and of the controls in the words of the frame, with the same span.
//   - each slat met with the own span across of its part. The frame draws the pane there.
//   - the glass pane of the controls with its lip, from the place of the controls to their lagged place.
//   - the share of the glass of that pane, 0 while the controls are in the bar, and the opacity of the controls.
//   - the radius of the corners of the panes, and of the work.
//   - the whole tabs row, with its foot at the foot of the shown row. It rises out of the head of the work as the claim
//     takes it away.
//   - the shown share of the tabs row, which is its opacity, GUI_FIXED_ONE whole.
//   - the whole map, with its left at the left of the column of the map. It goes past the right edge of the editor as
//     the claim shuts the column.
//   - the side pane as it slides: the share of its way gone, GUI_FIXED_ONE gone and 0 in place.
//   - the rectangle of the side, moved up and toward the left by that share of its own size. The pane draws there.
//   - the shown share of the side pane, its opacity at the parts of a part that fades.
//   - the panel of the terminal as it arrives: the share of its way left, GUI_FIXED_ONE as it starts and 0 in place.
//   - the rectangles of the panel and of its views, moved down by that share of GUI_TERMINAL_ARRIVES. The panel draws
//     there.
//   - the shown share of the panel, its opacity at those parts.
//   - the mode of the preview, one of GUI_FRAME_PREVIEW_*, with its rectangle. It has none while it is off.
//   - beside the text, the preview has the right half of the editor. Alone, it has the whole rectangle of the editor,
//     and the editor keeps no width under it.
//   - the two scrollbars of the editor, one down its right side and one along its foot. Each takes the width of a
//     scrollbar from the own rectangle of the editor. There are none where the editor has no room for them.
//   - `tabs_left`. The tabs are in one side_menu, and the tabs row is a window on that side_menu. `tabs_left` is the
//     number of pixels of the side_menu left of the row. The window sets it, and the layout does not change it.
typedef struct
{
    unsigned int width;
    unsigned int height;
    int scale;
    int side_displayed;
    int terminal_height;
    int preview_mode;
    GuiRect preview;
    GuiAutohideNumbers claim;
    GuiRect top_menu;
    GuiRect controls;
    GuiRect top_menu_tools;
    GuiRect status;
    GuiRect side_menu;
    GuiRect gap;
    GuiRect work;
    GuiRect inside;
    GuiRect side;
    GuiRect terminal;
    GuiRect terminal_views;
    GuiRect tabs;
    GuiRect editor;
    GuiRect editor_bar;
    GuiRect editor_bar_across;
    GuiRect map;
    GuiRect top_menu_band;
    GuiRect side_menu_band;
    GuiRect status_band;
    GuiRect top_menu_pane;
    GuiRect side_menu_pane;
    GuiRect status_pane;
    GuiRect controls_pane;
    GuiEdges bar_edges;
    GuiEdges side_menu_edges;
    GuiEdges status_edges;
    GuiEdges controls_edges;
    int controls_pane_share;
    int controls_alpha;
    unsigned int pane_radius;
    unsigned int radius;
    GuiRect tabs_row;
    int tabs_share;
    int tabs_left;
    GuiRect map_whole;
    int side_away;
    GuiRect side_pane;
    int side_share;
    int terminal_away;
    GuiRect terminal_drawn;
    GuiRect terminal_views_drawn;
    int terminal_share;
    GuiRect top_menu_rest;
    GuiRect controls_rest;
    GuiRect top_menu_tools_rest;
    GuiRect side_menu_rest;
    GuiRect tabs_rest;
} GuiFrame;

// a size in the page's pixels at `scale`, a percent, rounded to a whole pixel
int gui_scale(const GuiFramePixelsCfg *args);
// the parts arranged for a window `width` by `height` at `scale`, with no side displayed and no claim
void gui_frame_layout(const GuiFrameScaleCfg *args);
// the inside of the work, positioned again. Where `displayed` is 1, a side shows at its left, and the tabs row and the
// editor are past it. Its pane is `away` of its way gone. Where `displayed` is 0, there is no side.
//
// The panel of the terminal is `height` pixels of the page tall, at its foot, between its least and its max. It is
// `away` of its way below its place as it arrives. There is no panel where `height` is 0
void gui_frame_side(const GuiFrameDisplayedCfg *args);
void gui_frame_terminal(const GuiFrameDisplayedCfg *args);
// the work's inside positioned again with the preview's mode `displayed`: off, the editor whole. Beside, the editor's right
// half the preview's; alone, the editor's rectangle the preview's and the editor left none of its width
void gui_frame_preview(const GuiFrameDisplayedCfg *args);
// the parts positioned again for the `numbers` of the claim:
//   - the work grows to the window less the gap on each side as the claim runs.
//   - the bar, the side_menu and the status bar slide off and back as their reveals run. Each is a pane, in from the
//     edges of the window by the gap.
//   - the work gives each part its inset as its inset runs.
//   - the tabs row and the map are away inside the work until the reveals of their edges.
//   - the controls move down from the place of the bar as their way down runs.
void gui_frame_autohide(const GuiFrameCfg *args);
// the holes that the shown parts make in the work while the claim holds, into `holes`, with room for four. Each is the
// rectangle of a slat, grown by the share of the gap that the claim gives. It cuts to the work, and snaps to each edge
// of the work within two gaps of it. The result is their count
unsigned int gui_frame_holes(const GuiFrameHolesCfg *args);
struct GuiFrameScaleCfg
{
    int scale;
    GuiFrame *frame;
    unsigned int width;
    unsigned int height;
};

struct GuiFrameCfg
{
    GuiFrame *frame;
    const GuiAutohideNumbers *numbers;
};

struct GuiFrameHolesCfg
{
    const GuiFrame *frame_read;
    GuiRect *holes;
};

struct GuiFrameDisplayedCfg
{
    GuiFrame *frame;
    int displayed;
    int height;
    int away;
};

struct GuiFramePixelsCfg
{
    int pixels;
    int scale;
};

typedef struct
{
    int (*scale)(const GuiFramePixelsCfg *args);
} GuiFramePixelsNs;
EMBED_TABLE_LAYOUT(GuiFramePixelsNs, scale);
EMBED_TABLE_STORAGE GuiFramePixelsNs gui_frame_pixels EMBED_UNUSED = {
    .scale = gui_scale,
};

typedef struct
{
    void (*layout)(const GuiFrameScaleCfg *args);
} GuiFrameScaleNs;
EMBED_TABLE_LAYOUT(GuiFrameScaleNs, layout);
EMBED_TABLE_STORAGE GuiFrameScaleNs gui_frame_scale EMBED_UNUSED = {
    .layout = gui_frame_layout,
};

typedef struct
{
    void (*side)(const GuiFrameDisplayedCfg *args);
    void (*terminal)(const GuiFrameDisplayedCfg *args);
    void (*preview)(const GuiFrameDisplayedCfg *args);
} GuiFrameDisplayedNs;
EMBED_TABLE_LAYOUT(GuiFrameDisplayedNs, side, terminal, preview);
EMBED_TABLE_STORAGE GuiFrameDisplayedNs gui_frame_displayed EMBED_UNUSED = {
    .side = gui_frame_side,
    .terminal = gui_frame_terminal,
    .preview = gui_frame_preview,
};

typedef struct
{
    void (*claim)(const GuiFrameCfg *args);
} GuiFrameNs;
EMBED_TABLE_LAYOUT(GuiFrameNs, claim);
EMBED_TABLE_STORAGE GuiFrameNs gui_frame EMBED_UNUSED = {
    .claim = gui_frame_autohide,
};

typedef struct
{
    unsigned int (*holes)(const GuiFrameHolesCfg *args);
} GuiFrameHolesNs;
EMBED_TABLE_LAYOUT(GuiFrameHolesNs, holes);
EMBED_TABLE_STORAGE GuiFrameHolesNs gui_frame_holes_table EMBED_UNUSED = {
    .holes = gui_frame_holes,
};

#endif
