// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer.c: the window's rasters of its size in pages of the system's. The region holds the two framebuffers and
// the ground's raster, each in a cell a place boundary larger than its pixels, the pixels set forward to the first
// place boundary inside the cell, and the cellblock's own room for its cells' marks; the cellblock's record lies in
// the framebuffers, the region all cells
#include "framebuffer.h"

#include "system.h"

static CellOwner s_owner = MMGR_CELL_OWNER("gui_framebuffer");

// the room the cellblock keeps beside its cells' bytes, its marks and alignment, a place boundary's worth
#define FRAMEBUFFER_ROOM GUI_FRAMEBUFFER_PLACE

int gui_framebuffer_made(GuiFramebuffer *framebuffer, unsigned int width, unsigned int height)
{
    memor.set(&(MemoriaCfg){.dst = framebuffer, .val = 0, .bytes = sizeof(*framebuffer)});
    const unsigned int pitch = gui_raster_pitch(width);
    const unsigned long long pixels = (unsigned long long)pitch * height * sizeof(GuiColor);
    const unsigned long long cell = pixels + GUI_FRAMEBUFFER_PLACE;
    const unsigned long long bytes = ((unsigned long long)GUI_FRAMEBUFFER_RASTERS * cell) + FRAMEBUFFER_ROOM;
    framebuffer->pages = gui_system_pages(bytes);
    if (framebuffer->pages == NULL)
    {
        return 0;
    }
    framebuffer->bytes = bytes;
    framebuffer->room = pixels;
    // the cellblock's record laid as MMgr lays its own, byte by byte from a record made over the region
    const CarcerCellBlock made = MMGR_CARCER_STATE((uint8_t *)framebuffer->pages, (size_t)bytes);
    memor.cpy(&(MemoriaCfg){.dst = &framebuffer->cellblock, .src = &made, .bytes = sizeof(made)});
    for (unsigned int at = 0u; at < GUI_FRAMEBUFFER_RASTERS; at += 1u)
    {
        // a cell's size is far inside what a size_t holds
        framebuffer->cell[at] = mmgr_persistent_buf_alloc(&framebuffer->cellblock, (size_t)cell, &s_owner);
        if (framebuffer->cell[at] == NULL)
        {
            gui_framebuffer_given(framebuffer);
            return 0;
        }
        // the cell's address and the region's, read as integers, give the first place boundary inside the cell
        const unsigned long long region = (unsigned long long)(uintptr_t)framebuffer->pages;
        const unsigned long long start = (unsigned long long)(uintptr_t)framebuffer->cell[at] - region;
        const unsigned long long placed = (start + (GUI_FRAMEBUFFER_PLACE - 1ull)) & ~(GUI_FRAMEBUFFER_PLACE - 1ull);
        GuiRaster *const raster = &framebuffer->raster[at];
        raster->width = pitch;
        raster->height = height;
        raster->pixels = (GuiColor *)(void *)((unsigned char *)framebuffer->pages + placed);
        raster->pages = framebuffer->pages;
        raster->pages_bytes = bytes;
        raster->placed = placed;
    }
    return 1;
}

int gui_framebuffer_fits(const GuiFramebuffer *framebuffer, unsigned int width, unsigned int height)
{
    return (framebuffer->pages != NULL) &&
           ((unsigned long long)gui_raster_pitch(width) * height * sizeof(GuiColor) <= framebuffer->room);
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

void gui_framebuffer_given(GuiFramebuffer *framebuffer)
{
    for (unsigned int at = 0u; at < GUI_FRAMEBUFFER_RASTERS; at += 1u)
    {
        if (framebuffer->cell[at] != NULL)
        {
            mmgr_persistent_buf_release(&framebuffer->cellblock, framebuffer->cell[at], &s_owner);
        }
    }
    if (framebuffer->pages != NULL)
    {
        gui_system_pages_given(framebuffer->pages, framebuffer->bytes);
    }
    memor.set(&(MemoriaCfg){.dst = framebuffer, .val = 0, .bytes = sizeof(*framebuffer)});
}
