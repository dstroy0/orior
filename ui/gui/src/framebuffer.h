// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer.h: the window's rasters: the two framebuffers, handed between drawing and showing by a ring of MMgr's,
// and the ground's raster, each `gui_raster_pitch` of the window's width wide and starting on a GUI_FRAMEBUFFER_PLACE
// boundary of its pages, where the system's graphics device can place a buffer over it; room for the largest monitor
// taken from the system once and given back whole, and nothing of a window's size taken from the shared cells
// (framebuffer.c)
#ifndef GUI_FRAMEBUFFER_H
#define GUI_FRAMEBUFFER_H
typedef struct GuiFramebufferMostWidthCfg GuiFramebufferMostWidthCfg;
typedef struct GuiFramebufferCfg GuiFramebufferCfg;
#include "embed_dispatch_layout.h"
#include "raster.h"

// the boundary each raster's pixels start on in its pages, the placement a graphics device takes a buffer at
#define GUI_FRAMEBUFFER_PLACE (64ull << 10u)
// the rasters, the two framebuffers first and the ground's after them
#define GUI_FRAMEBUFFER_RASTERS 3u
#define GUI_FRAMEBUFFER_GROUND 2u

// The framebuffers' pages, the ground's, the pixel bytes each raster has room for, and the rasters laid over them
typedef struct
{
    void *pages;
    void *ground_pages;
    unsigned long long room;
    GuiRaster raster[GUI_FRAMEBUFFER_RASTERS];
} GuiFramebuffer;

// the rasters made for a window `width` by `height`, each the pitch of the width wide, every pixel clear, the first
// framebuffer the one drawn, with room for that window or one `most_width` by `most_height`, whichever takes more, to
// the next place boundary and no further: a window past the room saves the size it has. 1, or 0 where there is no size
// or the system gave no pages, nothing saved; and given back, the pages with them. One at a time: the rings are the
// program's
int gui_framebuffer_init(const GuiFramebufferMostWidthCfg *args);
void gui_framebuffer_free(const GuiFramebufferMostWidthCfg *args);
// whether the rasters have room for a window `width` by `height`; and the rasters sized to it, their pixels where they
// stand: 1, or 0 with nothing changed where they have no room
int gui_framebuffer_fits(const GuiFramebufferCfg *args);
int gui_framebuffer_fit(const GuiFramebufferMostWidthCfg *args);
// the framebuffer just drawn handed over to be shown, the one shown before freed, and the raster to draw next
GuiRaster *gui_framebuffer_flip(const GuiFramebufferMostWidthCfg *args);
struct GuiFramebufferMostWidthCfg
{
    GuiFramebuffer *framebuffer;
    unsigned int width;
    unsigned int height;
    unsigned int most_width;
    unsigned int most_height;
};

struct GuiFramebufferCfg
{
    unsigned int width;
    unsigned int height;
    const GuiFramebuffer *framebuffer_read;
};

typedef struct
{
    int (*init)(const GuiFramebufferMostWidthCfg *args);
    void (*free)(const GuiFramebufferMostWidthCfg *args);
    int (*fit)(const GuiFramebufferMostWidthCfg *args);
    GuiRaster *(*flip)(const GuiFramebufferMostWidthCfg *args);
} GuiFramebufferMostWidthNs;
EMBED_TABLE_LAYOUT(GuiFramebufferMostWidthNs, init, free, fit, flip);
EMBED_TABLE_STORAGE GuiFramebufferMostWidthNs gui_framebuffer_most_width EMBED_UNUSED = {
    .init = gui_framebuffer_init,
    .free = gui_framebuffer_free,
    .fit = gui_framebuffer_fit,
    .flip = gui_framebuffer_flip,
};

typedef struct
{
    int (*fits)(const GuiFramebufferCfg *args);
} CliFramebufferNs;
EMBED_TABLE_LAYOUT(CliFramebufferNs, fits);
EMBED_TABLE_STORAGE CliFramebufferNs cli_framebuffer EMBED_UNUSED = {
    .fits = gui_framebuffer_fits,
};

#endif
