// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer.c: the window's rasters: the two framebuffers, the two segments of a ring of MMgr's over pages of the
// system's, one drawn while the other is shown; and the ground's raster, the one segment of a ring of its own. The
// rings' addresses are settled by the linker and the pages behind them are given once at the start, at the size the
// largest monitor takes. A ring keeps its state beside its bytes, so that nothing but pixels lies in the pages, and
// each segment starts on a boundary the system's graphics device can place a buffer at
#include "framebuffer.h"

#include "memoria_anularis/memoria_anularis.h"
#include "system.h"

static CellOwner s_owner = MMGR_CELL_OWNER("gui_framebuffer");

ParsMemoriaeHospes(gui_frames_pool);
ParsMemoriaeHospes(gui_ground_pool);
MemoriaAnularis(s_frames, gui_frames_pool);
MemoriaAnularis(s_ground, gui_ground_pool);

// the bytes a window `width` by `height` draws in one raster
static unsigned long long framebuffer_window_bytes(unsigned int width, unsigned int height)
{
    return (unsigned long long)gui_raster_pitch(width) * height * sizeof(GuiColor);
}

// a raster laid over a ring's segment, the pages it lies in and how far into them
static void framebuffer_raster_laid(GuiRaster *raster, mmgr_ring *ring, size_t segment, void *pages,
                                    unsigned long long pages_bytes)
{
    uint8_t *const pixels = anularis.seg_at(&(AnularisCfg){.ring = ring, .index = segment});
    raster->pixels = (GuiColor *)(void *)pixels;
    raster->pages = pages;
    raster->pages_bytes = pages_bytes;
    // the segment's distance into the pages, which the ring's capacity bounds
    raster->placed = (unsigned long long)(pixels - (uint8_t *)pages);
}

int gui_framebuffer_made(GuiFramebuffer *framebuffer, unsigned int width, unsigned int height, unsigned int most_width,
                         unsigned int most_height)
{
    memor.set(&(MemoriaCfg){.dst = framebuffer, .val = 0, .bytes = sizeof(*framebuffer)});
    const unsigned long long window = framebuffer_window_bytes(width, height);
    const unsigned long long most = framebuffer_window_bytes(most_width, most_height);
    const unsigned long long larger = (window > most) ? window : most;
    // the next raster's place lies at the end of this one's room, which so ends on a place boundary, a power of two
    framebuffer->room = (larger + GUI_FRAMEBUFFER_PLACE - 1ull) & ~(GUI_FRAMEBUFFER_PLACE - 1ull);
    if (framebuffer->room == 0ull)
    {
        return 0;
    }
    const unsigned long long frames_bytes = 2ull * framebuffer->room;
    framebuffer->pages = gui_system_pages(frames_bytes);
    framebuffer->ground_pages = gui_system_pages(framebuffer->room);
    // the pages lie in this process's memory, whose sizes a size_t counts
    if ((framebuffer->pages == NULL) || (framebuffer->ground_pages == NULL) ||
        !anularis.init(&(AnularisCfg){.ring = &s_frames,
                                      .buf = framebuffer->pages,
                                      .capacity = (size_t)frames_bytes,
                                      .segment_count = 2u,
                                      .segment_bytes = (size_t)framebuffer->room}) ||
        !anularis.init(&(AnularisCfg){.ring = &s_ground,
                                      .buf = framebuffer->ground_pages,
                                      .capacity = (size_t)framebuffer->room,
                                      .segment_count = 1u,
                                      .segment_bytes = (size_t)framebuffer->room}))
    {
        gui_framebuffer_given(framebuffer);
        return 0;
    }
    // a registration refused is the owner registered by the window before this one, or the queue full
    (void)mmgr_owner_register(&s_owner);
    mmgr_owner_bytes_taken(&s_owner, (size_t)(frames_bytes + framebuffer->room));
    for (size_t segment = 0u; segment < 2u; segment += 1u)
    {
        framebuffer_raster_laid(&framebuffer->raster[segment], &s_frames, segment, framebuffer->pages, frames_bytes);
    }
    framebuffer_raster_laid(&framebuffer->raster[GUI_FRAMEBUFFER_GROUND], &s_ground, 0u, framebuffer->ground_pages,
                            framebuffer->room);
    // the room holds the window
    (void)gui_framebuffer_fitted(framebuffer, width, height);
    return 1;
}

int gui_framebuffer_fits(const GuiFramebuffer *framebuffer, unsigned int width, unsigned int height)
{
    return (framebuffer->pages != NULL) && (framebuffer_window_bytes(width, height) <= framebuffer->room);
}

int gui_framebuffer_fitted(GuiFramebuffer *framebuffer, unsigned int width, unsigned int height)
{
    const unsigned int pitch = gui_raster_pitch(width);
    if (!gui_framebuffer_fits(framebuffer, width, height))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < GUI_FRAMEBUFFER_RASTERS; at += 1u)
    {
        framebuffer->raster[at].width = pitch;
        framebuffer->raster[at].height = height;
    }
    return 1;
}

GuiRaster *gui_framebuffer_flipped(GuiFramebuffer *framebuffer)
{
    anularis.seg_publish(&(AnularisCfg){.ring = &s_frames});
    if (anularis.seg_inflight(&(AnularisCfg){.ring = &s_frames}) == 2u)
    {
        anularis.seg_release(&(AnularisCfg){.ring = &s_frames});
    }
    size_t next = 0u;
    // with the framebuffer before it released, one of the two is always free
    (void)anularis.seg_next(&(AnularisCfg){.ring = &s_frames, .out_index = &next});
    return &framebuffer->raster[next];
}

void gui_framebuffer_given(GuiFramebuffer *framebuffer)
{
    const unsigned long long frames_bytes = 2ull * framebuffer->room;
    if ((framebuffer->pages != NULL) && (framebuffer->raster[0].pixels != NULL))
    {
        // the pages lie in this process's memory, whose sizes a size_t counts
        mmgr_owner_bytes_given(&s_owner, (size_t)(frames_bytes + framebuffer->room));
    }
    if (framebuffer->pages != NULL)
    {
        gui_system_pages_given(framebuffer->pages, frames_bytes);
    }
    if (framebuffer->ground_pages != NULL)
    {
        gui_system_pages_given(framebuffer->ground_pages, framebuffer->room);
    }
    memor.set(&(MemoriaCfg){.dst = framebuffer, .val = 0, .bytes = sizeof(*framebuffer)});
}
