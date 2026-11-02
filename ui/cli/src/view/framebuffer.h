// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer.h: the window's rasters: the two framebuffers, handed between drawing and showing by a ring of MMgr's,
// and the ground's raster, each `gui_raster_pitch` of the window's width wide and starting on a
// GUI_FRAMEBUFFER_POSITION boundary of its pages, where the system's graphics device can place a buffer over it;
// capacity for the largest monitor taken from the system once and given back whole, and nothing of a window's size
// taken from the shared cells (framebuffer.c)
#ifndef GUI_FRAMEBUFFER_H
#define GUI_FRAMEBUFFER_H
typedef struct GuiFramebufferMaxWidthCfg GuiFramebufferMaxWidthCfg;
typedef struct GuiFramebufferCfg GuiFramebufferCfg;
#include "embed_dispatch_layout.h"
#include "raster.h"

// the boundary each raster's pixels start on in its pages, the placement a graphics device takes a buffer at
#define GUI_FRAMEBUFFER_POSITION (64ull << 10u)
// the rasters, the two framebuffers first and the ground's after them
#define GUI_FRAMEBUFFER_RASTERS 3u
#define GUI_FRAMEBUFFER_BACKGROUND 2u

// The framebuffers' pages, the ground's, the pixel bytes each raster has capacity for, and the rasters laid over them
typedef struct
{
    void *pages;
    void *background_pages;
    unsigned long long capacity;
    GuiRaster raster[GUI_FRAMEBUFFER_RASTERS];
} GuiFramebuffer;

// the rasters made for a window `width` by `height`, each the pitch of the width wide, every pixel clear, the first
// framebuffer the one drawn, with capacity for that window or one `max_width` by `max_height`, whichever takes more, to
// the next place boundary and no further: a window past the capacity saves the size it has. 1, or 0 where there is no
// size or the system gave no pages, nothing saved; and given back, the pages with them. One at a time: the rings are
// the program's
int gui_framebuffer_init(const GuiFramebufferMaxWidthCfg *args);
void gui_framebuffer_free(const GuiFramebufferMaxWidthCfg *args);
// whether the rasters have capacity for a window `width` by `height`; and the rasters sized to it, their pixels where
// they stand: 1, or 0 with nothing changed where they have no capacity
int gui_framebuffer_fits(const GuiFramebufferCfg *args);
int gui_framebuffer_fit(const GuiFramebufferMaxWidthCfg *args);
// the framebuffer just drawn handed over to be shown, the one shown before freed, and the raster to draw next
GuiRaster *gui_framebuffer_flip(const GuiFramebufferMaxWidthCfg *args);
struct GuiFramebufferMaxWidthCfg
{
    GuiFramebuffer *framebuffer;
    unsigned int width;
    unsigned int height;
    unsigned int max_width;
    unsigned int max_height;
};

struct GuiFramebufferCfg
{
    unsigned int width;
    unsigned int height;
    const GuiFramebuffer *framebuffer_read;
};

typedef struct
{
    int (*init)(const GuiFramebufferMaxWidthCfg *args);
    void (*free)(const GuiFramebufferMaxWidthCfg *args);
    int (*fit)(const GuiFramebufferMaxWidthCfg *args);
    GuiRaster *(*flip)(const GuiFramebufferMaxWidthCfg *args);
} GuiFramebufferMaxWidthNs;
EMBED_TABLE_LAYOUT(GuiFramebufferMaxWidthNs, init, free, fit, flip);
EMBED_TABLE_STORAGE GuiFramebufferMaxWidthNs gui_framebuffer_max_width EMBED_UNUSED = {
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
