// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer.c: a window's rasters: the two framebuffers, the two segments of the frames ring, one drawn while the
// other is shown; and the ground's raster, the one segment of the background ring. A ring saves its state beside its
// bytes, so that nothing but pixels lies in the segments, and each segment starts where its caller laid it
#include "framebuffer.h"

#include "memoria_operor/memoria_operor.h"

typedef struct
{
    GuiRaster *raster;
    mmgr_ring *ring;
    embed_word segment;
    void *pages;
    unsigned long long pages_bytes;
} FramebufferCtx;

// a raster laid over a ring's segment, the pages it lies in and how far into them
EMBED_INLINE void framebuffer_raster_layout(const FramebufferCtx *args)
{
    embed_u8 *const pixels = anularis.seg_at(&(AnularisCfg){.ring = args->ring, .index = args->segment});
    args->raster->pixels = (GuiColor *)(void *)pixels;
    args->raster->pages = args->pages;
    args->raster->pages_bytes = args->pages_bytes;
    // the segment's distance into the pages, which the ring's capacity bounds
    args->raster->positioned = (unsigned long long)(pixels - (embed_u8 *)args->pages);
}

unsigned long long gui_framebuffer_bytes(const GuiFramebufferCfg *args)
{
    return (unsigned long long)EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = args->width) *
           args->height * sizeof(GuiColor);
}

int gui_framebuffer_init(const GuiFramebufferRingsCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->framebuffer, .val = 0, .bytes = sizeof(*args->framebuffer)});
    const unsigned long long window =
        EMBED_CALL(gui_framebuffer.bytes, GuiFramebufferCfg, .width = args->width, .height = args->height);
    if ((window == 0ULL) || (args->capacity == 0ULL) || (window > args->capacity) || (args->frames == NULL) ||
        (args->background == NULL))
    {
        return 0;
    }
    args->framebuffer->frames = args->frames;
    args->framebuffer->background = args->background;
    args->framebuffer->capacity = args->capacity;
    const unsigned long long frames_bytes = 2ULL * args->capacity;
    args->framebuffer->pages = anularis.seg_at(&(AnularisCfg){.ring = args->frames, .index = 0u});
    args->framebuffer->background_pages = anularis.seg_at(&(AnularisCfg){.ring = args->background, .index = 0u});
    for (embed_word segment = 0u; segment < 2u; segment += 1u)
    {
        EMBED_CALL(framebuffer_raster_layout, FramebufferCtx, .raster = &args->framebuffer->raster[segment],
                   .ring = args->frames, .segment = segment, .pages = args->framebuffer->pages,
                   .pages_bytes = frames_bytes);
    }
    EMBED_CALL(framebuffer_raster_layout, FramebufferCtx,
               .raster = &args->framebuffer->raster[GUI_FRAMEBUFFER_BACKGROUND], .ring = args->background,
               .segment = 0u, .pages = args->framebuffer->background_pages, .pages_bytes = args->capacity);
    // the capacity holds the window
    (void)gui_framebuffer_fit(args);
    return 1;
}

int gui_framebuffer_fits(const GuiFramebufferCfg *args)
{
    return (args->framebuffer_read->pages != NULL) &&
           (EMBED_CALL(gui_framebuffer.bytes, GuiFramebufferCfg, .width = args->width, .height = args->height) <=
            args->framebuffer_read->capacity);
}

int gui_framebuffer_fit(const GuiFramebufferRingsCfg *args)
{
    const unsigned int pitch = EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = args->width);
    if (!EMBED_CALL(gui_framebuffer.fits, GuiFramebufferCfg, .framebuffer_read = args->framebuffer,
                    .width = args->width, .height = args->height))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < GUI_FRAMEBUFFER_RASTERS; at += 1u)
    {
        args->framebuffer->raster[at].width = pitch;
        args->framebuffer->raster[at].height = args->height;
    }
    return 1;
}

GuiRaster *gui_framebuffer_flip(const GuiFramebufferRingsCfg *args)
{
    mmgr_ring *const frames = args->framebuffer->frames;
    anularis.seg_publish(&(AnularisCfg){.ring = frames});
    if (anularis.seg_inflight(&(AnularisCfg){.ring = frames}) == 2u)
    {
        anularis.seg_release(&(AnularisCfg){.ring = frames});
    }
    embed_word next = 0u;
    // with the framebuffer before it released, one of the two is always free
    (void)anularis.seg_next(&(AnularisCfg){.ring = frames, .out_index = &next});
    return &args->framebuffer->raster[next];
}

void gui_framebuffer_free(const GuiFramebufferRingsCfg *args)
{
    // the rings and their pages are the caller's, given back by it
    memor.set(&(MemoriaCfg){.dst = args->framebuffer, .val = 0, .bytes = sizeof(*args->framebuffer)});
}
