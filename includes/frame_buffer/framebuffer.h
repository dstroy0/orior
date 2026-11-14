// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer.h: a window's rasters: the two framebuffers, passed between drawing and showing by a ring of MMgr's
// with two segments, and the ground's raster, the one segment of a ring of its own, each `gui_raster_pitch` of the
// window's width wide. The rings and the capacity of a segment are the caller's: the caller positions the rings over pages
// it retains, commits them as far as the window draws, and gives them back (framebuffer.c)
#ifndef GUI_FRAMEBUFFER_H
#define GUI_FRAMEBUFFER_H
#include "../raster/raster.h"
#include "embed_dispatch_layout.h"
#include "memoria_anularis/memoria_anularis.h"

typedef struct GuiFramebufferRingsCfg GuiFramebufferRingsCfg;
typedef struct GuiFramebufferCfg GuiFramebufferCfg;

// the rasters, the two framebuffers first and the ground's after them
#define GUI_FRAMEBUFFER_RASTERS 3u
#define GUI_FRAMEBUFFER_BACKGROUND 2u

// The framebuffers' ring and its pages, the ground's ring and its pages, the pixel bytes each raster has capacity for,
// and the rasters positioned over the rings' segments
typedef struct
{
    mmgr_ring *frames;
    mmgr_ring *background;
    void *pages;
    void *background_pages;
    unsigned long long capacity;
    GuiRaster raster[GUI_FRAMEBUFFER_RASTERS];
} GuiFramebuffer;

// the bytes a window `width` by `height` draws in one raster: the pitch of the width by the height, a word a pixel
unsigned long long gui_framebuffer_bytes(const GuiFramebufferCfg *args);
// the rasters made for a window `width` by `height` over the `frames` ring of two segments and the `background` ring
// of one, each segment `capacity` bytes, the first framebuffer the one drawn: 1, or 0 where there is no size, no ring
// or the window passes the capacity, nothing retained; and the rasters let go, the rings their caller's to give back. One
// at a time: the rings are the caller's
int gui_framebuffer_init(const GuiFramebufferRingsCfg *args);
void gui_framebuffer_free(const GuiFramebufferRingsCfg *args);
// whether the rasters have capacity for a window `width` by `height`; and the rasters sized to it, their pixels where
// they are: 1, or 0 with nothing changed where they have no capacity
int gui_framebuffer_fits(const GuiFramebufferCfg *args);
int gui_framebuffer_fit(const GuiFramebufferRingsCfg *args);
// the framebuffer just drawn passed over to be displayed, the one displayed before freed, and the raster to draw next
GuiRaster *gui_framebuffer_flip(const GuiFramebufferRingsCfg *args);
struct GuiFramebufferRingsCfg
{
    GuiFramebuffer *framebuffer;
    mmgr_ring *frames;
    mmgr_ring *background;
    unsigned long long capacity;
    unsigned int width;
    unsigned int height;
};

struct GuiFramebufferCfg
{
    unsigned int width;
    unsigned int height;
    const GuiFramebuffer *framebuffer_read;
};

typedef struct
{
    int (*init)(const GuiFramebufferRingsCfg *args);
    void (*free)(const GuiFramebufferRingsCfg *args);
    int (*fit)(const GuiFramebufferRingsCfg *args);
    GuiRaster *(*flip)(const GuiFramebufferRingsCfg *args);
} GuiFramebufferRingsNs;
EMBED_TABLE_LAYOUT(GuiFramebufferRingsNs, init, free, fit, flip);
EMBED_TABLE_STORAGE GuiFramebufferRingsNs gui_framebuffer_rings EMBED_UNUSED = {
    .init = gui_framebuffer_init,
    .free = gui_framebuffer_free,
    .fit = gui_framebuffer_fit,
    .flip = gui_framebuffer_flip,
};

typedef struct
{
    unsigned long long (*bytes)(const GuiFramebufferCfg *args);
    int (*fits)(const GuiFramebufferCfg *args);
} GuiFramebufferNs;
EMBED_TABLE_LAYOUT(GuiFramebufferNs, bytes, fits);
EMBED_TABLE_STORAGE GuiFramebufferNs gui_framebuffer EMBED_UNUSED = {
    .bytes = gui_framebuffer_bytes,
    .fits = gui_framebuffer_fits,
};

#endif
