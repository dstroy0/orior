// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer.h: the window's two framebuffers: one region of the system's pages, a cellblock of MMgr's laid over it,
// and two rasters taken from it as cells, each `gui_raster_pitch` of the window's width wide and starting on a
// GUI_FRAMEBUFFER_PLACE boundary of the region, where the system's graphics device can place a buffer over it; the
// region given back to the system whole (framebuffer.c)
#ifndef GUI_FRAMEBUFFER_H
#define GUI_FRAMEBUFFER_H
#include "raster.h"

// the boundary each framebuffer's pixels start on in the region, the placement a graphics device takes a buffer at
#define GUI_FRAMEBUFFER_PLACE (64ull << 10u)

// The framebuffers: the region, its bytes, the cellblock laid over it, the cell each raster was taken as, and the two
// rasters, their pixels inside their cells on the place boundary
typedef struct
{
    void *pages;
    unsigned long long bytes;
    CarcerCellBlock cellblock;
    void *cell[2];
    GuiRaster raster[2];
} GuiFramebuffer;

// the two framebuffers made for a window `width` by `height`, each raster the pitch of the width wide, every pixel
// clear: 1, or 0 where the system gave no pages or the cellblock no cell, the framebuffers holding none; and given
// back, the region with them
int gui_framebuffer_made(GuiFramebuffer *framebuffer, unsigned int width, unsigned int height);
void gui_framebuffer_given(GuiFramebuffer *framebuffer);
#endif
