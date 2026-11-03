// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.h: the parts and their rectangles for the window's size and the system's scale, as the page lays them out:
// the bar across the top with the three controls at its right end, the status bar across the foot, the strip down the
// left and the frame's gap down the right, and the work between them, its inside the work less its padding, a side
// pane at the inside's left where one is shown, its tabs row at its head past the side, the minimap's column at its
// right under the tabs row and the editor filling the rest; every size the page's pixels times the scale (frame.c)
#ifndef GUI_FRAME_H
#define GUI_FRAME_H
#include "embed_dispatch_layout.h"
#include "raster.h"

typedef struct GuiFrameScaleCfg GuiFrameScaleCfg;
typedef struct GuiFrameCfg GuiFrameCfg;
typedef struct GuiFrameHolesCfg GuiFrameHolesCfg;
typedef struct GuiFrameDisplayedCfg GuiFrameDisplayedCfg;
typedef struct GuiFramePixelsCfg GuiFramePixelsCfg;

// the parts' sizes in the page's pixels: the bar's height, --bar-height; the status bar's; the strip's width,
// --strip-width; the frame's gap at the right, --frame-right; the side pane's width, --side; the tabs row's height,
// --tabs-tall; the minimap's width, .ed-mini; the work's padding, top, right, bottom and left; the work's corner
// radius; a window control's width, the gap between the controls and how far their row ends from the bar's right end
#define GUI_BAR_HEIGHT 41
#define GUI_STATUS_HEIGHT 27
#define GUI_STRIP_WIDTH 44
#define GUI_FRAME_GAP 12
#define GUI_SIDE_WIDTH 304
#define GUI_TABS_HEIGHT 31
#define GUI_MAP_WIDTH 84
#define GUI_WORK_PAD_TOP 2
#define GUI_WORK_PAD_RIGHT 2
#define GUI_WORK_PAD_BOTTOM 1
#define GUI_WORK_PAD_LEFT 1
#define GUI_WORK_RADIUS 7
#define GUI_CONTROL_WIDTH 34
#define GUI_CONTROL_GAP 2
#define GUI_CONTROLS_END 5
#define GUI_CONTROLS 3

// the controls' tab's lip in the page's pixels, how far its glass reaches left of the controls; and how many gaps in
// from the work's edge a part's hole snaps to the edge
#define GUI_CONTROLS_BEVEL 8
#define GUI_HOLE_SNAP 2

// the terminal's panel in the page's pixels: its least height, min-height; what it leaves the views above it at the
// least, the 16rem of its max-height; the share of the window's height it reaches at the most, in percent, the 75vh of
// its max-height; its top line; and its strip of tabs under that with the strip's foot line
#define GUI_TERMINAL_LEAST 96
#define GUI_TERMINAL_SHELL_LEAST 256
#define GUI_TERMINAL_MOST_PERCENT 75
#define GUI_TERMINAL_LINE 1
#define GUI_TERMINAL_STRIP 27

// a number of 1 in the frame's arithmetic: every share the parts move on is a whole word, 16 bits of it a fraction
#define GUI_FIXED_ONE 65536

// The claim's numbers, each 0 to GUI_FIXED_ONE as the page's claim runs them: the claim, GUI_FIXED_ONE while the work
// has taken the edges' parts' inset; each edge's reveal, 1 while its part is back; the controls' way down from the
// bar's place while the bar is away; each edge's inset, 1 once the work has given the part its inset; each slat's grown
// share along its edge; the controls' opacity; and the point along each edge its slat is pulled from, in pixels, -1 for
// the middle
typedef struct
{
    int claim;
    int reveal_top;
    int reveal_bottom;
    int reveal_left;
    int reveal_right;
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
} GuiAutohideNumbers;

// The four edges of a pane in the frame's words, each a count of pixels with sixteen bits of fraction: where the
// pane's tint runs, an edge that falls between two pixel rows or columns laid at its share of the one it falls in
typedef struct
{
    int left;
    int top;
    int right;
    int bottom;
} GuiEdges;

// The frame: the window's size and scale it was laid out for, whether a side is shown, the terminal's height asked in
// the page's pixels, 0 while its panel is closed, and the claim's numbers it is laid for; each part's rectangle, the
// side's of no width where none is shown, the terminal's panel at the inside's foot under both and its views under its
// strip, none while it is closed, the map's at the editor's right, the
// bar's, the strip's and the status bar's standing off the window while the claim holds them away; each of those
// three as the slat its edge shows, the part's rectangle cut to its grown share, and the edges of each slat's pane and
// of the controls' in the frame's words; the controls' glass pane with its
// lip and the pane's share of the glass, 0 while the controls stand in the bar; the controls' opacity; the panes'
// corner radius; and the work's
typedef struct
{
    unsigned int width;
    unsigned int height;
    int scale;
    int side_displayed;
    int terminal_height;
    GuiAutohideNumbers claim;
    GuiRect bar;
    GuiRect controls;
    GuiRect status;
    GuiRect strip;
    GuiRect gap;
    GuiRect work;
    GuiRect inside;
    GuiRect side;
    GuiRect terminal;
    GuiRect terminal_views;
    GuiRect tabs;
    GuiRect editor;
    GuiRect map;
    GuiRect bar_band;
    GuiRect strip_band;
    GuiRect status_band;
    GuiRect controls_pane;
    GuiEdges bar_edges;
    GuiEdges strip_edges;
    GuiEdges status_edges;
    GuiEdges controls_edges;
    int controls_pane_share;
    int controls_alpha;
    unsigned int pane_radius;
    unsigned int radius;
} GuiFrame;

// a size in the page's pixels at `scale`, a percent, rounded to a whole pixel
int gui_scale(const GuiFramePixelsCfg *args);
// the parts laid out for a window `width` by `height` at `scale`, with no side shown and no claim
void gui_frame_layout(const GuiFrameScaleCfg *args);
// the work's inside laid again with a side shown at its left where `displayed` is 1, the tabs row and the editor past
// it, or with no side where it is 0; and with the terminal's panel `height` page's pixels tall at its foot, between
// its least and its most, or with none where `height` is 0
void gui_frame_side(const GuiFrameDisplayedCfg *args);
void gui_frame_terminal(const GuiFrameDisplayedCfg *args);
// the parts laid again for the claim's `numbers`: the work grown to the window less the gap on every side as the
// claim runs, the bar, the strip and the status bar slid off and back as their reveals run, each a pane standing in
// from the window's edges by the gap, the work giving each its inset as its inset runs, the tabs row and the map away
// inside the work until their edges' reveals, and the controls moved down from the bar's place as their way down runs
void gui_frame_autohide(const GuiFrameCfg *args);
// the holes the parts shown make in the work while the claim holds, each a slat's rectangle grown by the claim's
// share of the gap, cut to the work and snapped to the work's edges it comes within two gaps of, into `holes`, sized
// for four: their count
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
} GuiFrameDisplayedNs;
EMBED_TABLE_LAYOUT(GuiFrameDisplayedNs, side, terminal);
EMBED_TABLE_STORAGE GuiFrameDisplayedNs gui_frame_displayed EMBED_UNUSED = {
    .side = gui_frame_side,
    .terminal = gui_frame_terminal,
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
} CliFrameHolesNs;
EMBED_TABLE_LAYOUT(CliFrameHolesNs, holes);
EMBED_TABLE_STORAGE CliFrameHolesNs cli_frame_holes EMBED_UNUSED = {
    .holes = gui_frame_holes,
};

#endif
