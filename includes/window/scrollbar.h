// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scrollbar.h: the scrollbar every part that scrolls draws along its side or its foot, as the stylesheet draws one: an
// arrow at each end, the track between them left clear, and the thumb in the third text color with round ends. The
// thumb's length is the share of the whole in view and its place the share before the view. A part whose whole is in
// view has no scrollbar, and the scrollbar draws nothing and answers no press there.
//
// A scrollbar is arranged once for its place, its direction and its scale. The layout makes its two arrows and the
// thumb's two round ends as masks of coverage, as a glyph is made once. A frame positions the masks and fills the thumb's
// straight middle between its ends; the thumb's place is all a frame works out (scrollbar.c)
#ifndef GUI_WINDOW_SCROLLBAR_H
#define GUI_WINDOW_SCROLLBAR_H
#include "palette.h"
#include "../raster/raster.h"
#include "embed_dispatch_layout.h"

typedef struct GuiScrollbarCfg GuiScrollbarCfg;

// the scrollbar's thickness, an arrow's length and the thumb's least length, in unscaled pixels; and the max pixels a
// thickness takes at any scale, which bounds the masks' cells
#define GUI_SCROLLBAR_WIDTH 10
// the pixels the editor's scrollbar is in from the work's line, past the editor's own padding
#define GUI_SCROLLBAR_STANDOFF 2
#define GUI_SCROLLBAR_ARROW 10
#define GUI_SCROLLBAR_THUMB_MIN 16
#define GUI_SCROLLBAR_SIDE_MAX 64u

// what a press lands on: nothing, the arrow before, the arrow after, the thumb, or the track before or after the thumb
#define GUI_SCROLLBAR_NONE 0u
#define GUI_SCROLLBAR_UP 1u
#define GUI_SCROLLBAR_DOWN 2u
#define GUI_SCROLLBAR_THUMB 3u
#define GUI_SCROLLBAR_ABOVE 4u
#define GUI_SCROLLBAR_BELOW 5u

// A scrollbar arranged: whether it runs across, under what scrolls sideways, rather than down beside it; the scale it
// was positioned at; where it is; its track between the arrows; the thumb's thickness, its inset from the scrollbar's
// sides and its least length; the arrows before and after and the thumb's ends before and after as masks; and the
// cells the masks' coverage lies in
typedef struct
{
    int across;
    int scale;
    GuiRect rect;
    GuiRect track;
    int thickness;
    int inset;
    int least;
    GuiMask arrow_before;
    GuiMask arrow_after;
    GuiMask end_before;
    GuiMask end_after;
    unsigned char arrow_cover[2][GUI_SCROLLBAR_SIDE_MAX * GUI_SCROLLBAR_SIDE_MAX];
    unsigned char end_cover[2][GUI_SCROLLBAR_SIDE_MAX * GUI_SCROLLBAR_SIDE_MAX];
} GuiScrollbar;

// A scrollbar, arranged at `rect`, running `across` or down, at `scale`, a percent; the whole's lines `total`, the
// lines in view `displayed` and the first line in view `first`; whether the pointer is on the thumb; a press at x, y;
// and where the first line a dragged thumb puts in view goes
struct GuiScrollbarCfg
{
    GuiScrollbar *bar;
    const GuiScrollbar *bar_read;
    GuiRect rect;
    int across;
    int scale;
    GuiRaster *raster;
    const GuiPalette *palette;
    unsigned long long total;
    unsigned long long displayed;
    unsigned long long first;
    int pointed;
    int x;
    int y;
    unsigned long long *first_out;
};

// the scrollbar arranged at `rect`, `across` or down, at `scale`: its masks made where its thickness or direction
// changed, else retained
void gui_scrollbar_layout(const GuiScrollbarCfg *args);
// whether the whole is more than its view, so that a scrollbar is
int gui_scrollbar_needed(const GuiScrollbarCfg *args);
// the scrollbar drawn, nothing where none is
void gui_scrollbar_draw(const GuiScrollbarCfg *args);
// where the thumb is, empty where no scrollbar is
GuiRect gui_scrollbar_thumb(const GuiScrollbarCfg *args);
// what a press at x, y lands on
unsigned int gui_scrollbar_hit(const GuiScrollbarCfg *args);
// the first line in view that puts the thumb's start at x, or at y where the scrollbar runs down, into `first_out`
void gui_scrollbar_first_at(const GuiScrollbarCfg *args);

typedef struct
{
    void (*layout)(const GuiScrollbarCfg *args);
    int (*needed)(const GuiScrollbarCfg *args);
    void (*draw)(const GuiScrollbarCfg *args);
    GuiRect (*thumb)(const GuiScrollbarCfg *args);
    unsigned int (*hit)(const GuiScrollbarCfg *args);
    void (*first_at)(const GuiScrollbarCfg *args);
} GuiScrollbarNs;
EMBED_TABLE_LAYOUT(GuiScrollbarNs, layout, needed, draw, thumb, hit, first_at);
EMBED_TABLE_STORAGE GuiScrollbarNs gui_scrollbar EMBED_UNUSED = {
    .layout = gui_scrollbar_layout,
    .needed = gui_scrollbar_needed,
    .draw = gui_scrollbar_draw,
    .thumb = gui_scrollbar_thumb,
    .hit = gui_scrollbar_hit,
    .first_at = gui_scrollbar_first_at,
};

#endif
