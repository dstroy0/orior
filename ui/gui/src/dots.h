// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dots.h: the state dots that the overseer's records give, drawn on the head of the panel at the work's foot. A record
// that starts or is up has the green of statusDot.up, one that stops or has link errors the yellow of
// statusDot.stopping, and one in error or failed the red of statusDot.failed. Starting, stopping and error blink: the
// dot shows for half of each blink (dots.c)
#ifndef GUI_WINDOW_DOTS_H
#define GUI_WINDOW_DOTS_H
#include "../../../includes/raster/raster.h"
#include "../../../includes/window/palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiDotsCfg GuiDotsCfg;

// the most dots of one `role` on a head, and the side of a dot and the gap after it, in the page's pixels
#define GUI_DOTS_MAX 16u
#define GUI_DOTS_SIDE 7
#define GUI_DOTS_GAP 5

// The dots of the records of the `role` given, one each, drawn right to left from `right` in the row `top`,
// `height` high, at the frame's scale `scale`, within `clip`. Where `worst` is 1, one dot shows the heaviest state of
// those records.
// The x of the left edge of the dots, `right` where none shows
int gui_dots_draw(const GuiDotsCfg *args);
// the color of a dot of the state `state`, transparent where the state has no dot
GuiColor gui_dots_color(const GuiDotsCfg *args);

struct GuiDotsCfg
{
    GuiRaster *raster;
    const GuiPalette *palette;
    GuiRect clip;
    int right;
    int top;
    int height;
    int scale;
    unsigned int role;
    int worst;
    unsigned int state;
    unsigned long long now_ms;
};

typedef struct
{
    int (*draw)(const GuiDotsCfg *args);
} GuiDotsNs;
EMBED_TABLE_LAYOUT(GuiDotsNs, draw);
EMBED_TABLE_STORAGE GuiDotsNs gui_dots EMBED_UNUSED = {
    .draw = gui_dots_draw,
};

typedef struct
{
    GuiColor (*color)(const GuiDotsCfg *args);
} GuiDotsColorNs;
EMBED_TABLE_LAYOUT(GuiDotsColorNs, color);
EMBED_TABLE_STORAGE GuiDotsColorNs gui_dots_color_of EMBED_UNUSED = {
    .color = gui_dots_color,
};

#endif
