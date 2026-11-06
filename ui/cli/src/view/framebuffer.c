// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer.c: the window's rasters: the two framebuffers, the two segments of a ring of MMgr's over pages of the
// system's, one drawn while the other is shown; and the ground's raster, the one segment of a ring of its own. The
// rings' addresses are settled by the linker and the pages behind them are given once at the start, at the size the
// largest monitor takes. A ring saves its state beside its bytes, so that nothing but pixels lies in the pages, and
// each segment starts on a boundary the system's graphics device can place a buffer at
#include "framebuffer.h"

#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "memoria_anularis/memoria_anularis.h"

ParsMemoriaeHospes(gui_frames_pool);
ParsMemoriaeHospes(gui_background_pool);
MemoriaAnularis(s_frames, gui_frames_pool);
MemoriaAnularis(s_background, gui_background_pool);

typedef struct
{
    unsigned int width;
    unsigned int height;
    GuiRaster *raster;
    mmgr_ring *ring;
    size_t segment;
    void *pages;
    unsigned long long pages_bytes;
} FramebufferCtx;
// the bytes a window `width` by `height` draws in one raster
EMBED_INLINE unsigned long long framebuffer_window_bytes(const FramebufferCtx *args)
{
    unsigned int width = args->width;
    return (unsigned long long)EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = width) * args->height *
           sizeof(GuiColor);
}

// a raster laid over a ring's segment, the pages it lies in and how far into them
EMBED_INLINE void framebuffer_raster_layout(const FramebufferCtx *args)
{
    mmgr_ring *ring = args->ring;
    void *pages = args->pages;
    unsigned long long pages_bytes = args->pages_bytes;
    uint8_t *const pixels = anularis.seg_at(&(AnularisCfg){.ring = ring, .index = args->segment});
    args->raster->pixels = (GuiColor *)(void *)pixels;
    args->raster->pages = pages;
    args->raster->pages_bytes = pages_bytes;
    // the segment's distance into the pages, which the ring's capacity bounds
    args->raster->positioned = (unsigned long long)(pixels - (uint8_t *)pages);
}

int gui_framebuffer_init(const GuiFramebufferMaxWidthCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->framebuffer, .val = 0, .bytes = sizeof(*args->framebuffer)});
    const unsigned long long window =
        EMBED_CALL(framebuffer_window_bytes, FramebufferCtx, .width = args->width, .height = args->height);
    const unsigned long long most =
        EMBED_CALL(framebuffer_window_bytes, FramebufferCtx, .width = args->max_width, .height = args->max_height);
    const unsigned long long larger = (window > most) ? window : most;
    // the next raster's place lies at the end of this one's capacity, which so ends on a place boundary, a power of two
    args->framebuffer->capacity = (larger + GUI_FRAMEBUFFER_POSITION - 1ULL) & ~(GUI_FRAMEBUFFER_POSITION - 1ULL);
    if (args->framebuffer->capacity == 0ULL)
    {
        return 0;
    }
    const unsigned long long frames_bytes = 2ULL * args->framebuffer->capacity;
    args->framebuffer->pages = EMBED_CALL(cli_size_bytes_internal.pages_alloc, CliSizeBytesCtx, .size = frames_bytes);
    args->framebuffer->background_pages =
        EMBED_CALL(cli_size_bytes_internal.pages_alloc, CliSizeBytesCtx, .size = args->framebuffer->capacity);
    // the pages lie in this process's memory, whose sizes a size_t counts
    if ((args->framebuffer->pages == NULL) || (args->framebuffer->background_pages == NULL) ||
        !anularis.init(&(AnularisCfg){.ring = &s_frames,
                                      .buf = args->framebuffer->pages,
                                      .capacity = (size_t)frames_bytes,
                                      .segment_count = 2u,
                                      .segment_bytes = (size_t)args->framebuffer->capacity}) ||
        !anularis.init(&(AnularisCfg){.ring = &s_background,
                                      .buf = args->framebuffer->background_pages,
                                      .capacity = (size_t)args->framebuffer->capacity,
                                      .segment_count = 1u,
                                      .segment_bytes = (size_t)args->framebuffer->capacity}))
    {
        gui_framebuffer_max_width.free(args);
        return 0;
    }
    mmgr_owner_bytes_taken(g_gui_framebuffer_owner, (size_t)(frames_bytes + args->framebuffer->capacity));
    for (size_t segment = 0u; segment < 2u; segment += 1u)
    {
        EMBED_CALL(framebuffer_raster_layout, FramebufferCtx, .raster = &args->framebuffer->raster[segment],
                   .ring = &s_frames, .segment = segment, .pages = args->framebuffer->pages,
                   .pages_bytes = frames_bytes);
    }
    EMBED_CALL(framebuffer_raster_layout, FramebufferCtx,
               .raster = &args->framebuffer->raster[GUI_FRAMEBUFFER_BACKGROUND], .ring = &s_background, .segment = 0u,
               .pages = args->framebuffer->background_pages, .pages_bytes = args->framebuffer->capacity);
    // the capacity holds the window
    (void)gui_framebuffer_max_width.fit(args);
    return 1;
}

int gui_framebuffer_fits(const GuiFramebufferCfg *args)
{
    return (args->framebuffer_read->pages != NULL) &&
           (EMBED_CALL(framebuffer_window_bytes, FramebufferCtx, .width = args->width, .height = args->height) <=
            args->framebuffer_read->capacity);
}

int gui_framebuffer_fit(const GuiFramebufferMaxWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    const unsigned int pitch = EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = width);
    if (!EMBED_CALL(cli_framebuffer.fits, GuiFramebufferCfg, .framebuffer_read = args->framebuffer, .width = width,
                    .height = height))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < GUI_FRAMEBUFFER_RASTERS; at += 1u)
    {
        args->framebuffer->raster[at].width = pitch;
        args->framebuffer->raster[at].height = height;
    }
    return 1;
}

GuiRaster *gui_framebuffer_flip(const GuiFramebufferMaxWidthCfg *args)
{
    anularis.seg_publish(&(AnularisCfg){.ring = &s_frames});
    if (anularis.seg_inflight(&(AnularisCfg){.ring = &s_frames}) == 2u)
    {
        anularis.seg_release(&(AnularisCfg){.ring = &s_frames});
    }
    size_t next = 0u;
    // with the framebuffer before it released, one of the two is always free
    (void)anularis.seg_next(&(AnularisCfg){.ring = &s_frames, .out_index = &next});
    return &args->framebuffer->raster[next];
}

void gui_framebuffer_free(const GuiFramebufferMaxWidthCfg *args)
{
    const unsigned long long frames_bytes = 2ULL * args->framebuffer->capacity;
    if ((args->framebuffer->pages != NULL) && (args->framebuffer->raster[0].pixels != NULL))
    {
        // the pages lie in this process's memory, whose sizes a size_t counts
        mmgr_owner_bytes_given(g_gui_framebuffer_owner, (size_t)(frames_bytes + args->framebuffer->capacity));
    }
    if (args->framebuffer->pages != NULL)
    {
        EMBED_CALL(cli_size_bytes_internal.pages_free, CliSizeBytesCtx, .bytes_out = args->framebuffer->pages,
                   .size = frames_bytes);
    }
    if (args->framebuffer->background_pages != NULL)
    {
        EMBED_CALL(cli_size_bytes_internal.pages_free, CliSizeBytesCtx,
                   .bytes_out = args->framebuffer->background_pages, .size = args->framebuffer->capacity);
    }
    memor.set(&(MemoriaCfg){.dst = args->framebuffer, .val = 0, .bytes = sizeof(*args->framebuffer)});
}
