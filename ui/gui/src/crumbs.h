// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// crumbs.h: the crumbs at the status bar's left as the CLI reads them (code/crumbs.h): each folder in the bar's text,
// the file in its bright text and each declaration in the link's color, the crumbs' mark between them in the path's;
// laid from an x as far as a right edge, a crumb that does not fit whole left out with every one after it; each crumb's
// rectangle stands as last laid, in the order of x, so a press finds its crumb by halving (crumbs.c)
#ifndef GUI_WINDOW_CRUMBS_H
#define GUI_WINDOW_CRUMBS_H
#include "../../../includes/window/glyphs.h"
#include "../../../includes/window/palette.h"
#include "../../cli/src/code/crumbs.h"
#include "embed_dispatch_layout.h"

typedef struct GuiCrumbsCfg GuiCrumbsCfg;

// the gap about the crumbs' mark in the page's pixels
#define GUI_CRUMBS_GAP 5

// the crumbs laid from `x_f32` on `baseline` in the head face at `size`, cut to `clip`, and drawn on `raster` where it
// is not NULL, else laid alone: the x after the last laid, `x_f32` where none is; how many were laid; the rectangle
// of the crumb `index` as laid, empty past the last; and the crumb at x, y: 1 with its place into `index_out`, else 0
float gui_crumbs_draw(const GuiCrumbsCfg *args);
unsigned int gui_crumbs_count(const GuiCrumbsCfg *args);
GuiRect gui_crumbs_place(const GuiCrumbsCfg *args);
int gui_crumbs_hit(const GuiCrumbsCfg *args);
struct GuiCrumbsCfg
{
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    unsigned int size;
    float x_f32;
    int baseline;
    GuiRect clip;
    int scale;
    unsigned int index;
    int x;
    int y;
    unsigned int *index_out;
};

typedef struct
{
    float (*draw)(const GuiCrumbsCfg *args);
    unsigned int (*count)(const GuiCrumbsCfg *args);
    GuiRect (*place)(const GuiCrumbsCfg *args);
    int (*hit)(const GuiCrumbsCfg *args);
} GuiCrumbsNs;
EMBED_TABLE_LAYOUT(GuiCrumbsNs, draw, count, place, hit);
EMBED_TABLE_STORAGE GuiCrumbsNs gui_crumbs EMBED_UNUSED = {
    .draw = gui_crumbs_draw,
    .count = gui_crumbs_count,
    .place = gui_crumbs_place,
    .hit = gui_crumbs_hit,
};

#endif
