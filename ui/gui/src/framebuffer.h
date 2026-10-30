// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// framebuffer.h: the window's rasters of its size: one region of the system's pages, a cellblock of MMgr's laid over
// it, and three rasters taken from it as cells, the two framebuffers and the ground's, each `gui_raster_pitch` of the
// window's width wide and starting on a GUI_FRAMEBUFFER_PLACE boundary of the region, where the system's graphics
// device can place a buffer over it; the region given back to the system whole, and nothing of a window's size taken
// from the shared cells (framebuffer.c)
#ifndef GUI_FRAMEBUFFER_H
#define GUI_FRAMEBUFFER_H
#include "raster.h"

// the boundary each raster's pixels start on in the region, the placement a graphics device takes a buffer at
#define GUI_FRAMEBUFFER_PLACE (64ull << 10u)
// the rasters the region holds, the two framebuffers first and the ground's after them
#define GUI_FRAMEBUFFER_RASTERS 3u
#define GUI_FRAMEBUFFER_GROUND 2u

// The region: its pages, its bytes, the pixel bytes each raster has room for, the cellblock laid over it, the cell
// each raster was taken as, and the rasters, their pixels inside their cells on the place boundary
typedef struct
{
    void *pages;
    unsigned long long bytes;
    unsigned long long room;
    CarcerCellBlock cellblock;
    void *cell[GUI_FRAMEBUFFER_RASTERS];
    GuiRaster raster[GUI_FRAMEBUFFER_RASTERS];
} GuiFramebuffer;

// the rasters made for a window `width` by `height`, each the pitch of the width wide, every pixel clear: 1, or 0
// where the system gave no pages or the cellblock no cell, the region holding none; and given back, the region with
// them
int gui_framebuffer_made(GuiFramebuffer *framebuffer, unsigned int width, unsigned int height);
void gui_framebuffer_given(GuiFramebuffer *framebuffer);
// whether the region has room for the rasters of a window `width` by `height`; and the rasters sized to it in the
// region they stand in, the pages and their places kept: 1, or 0 with nothing changed where the region has no room
int gui_framebuffer_fits(const GuiFramebuffer *framebuffer, unsigned int width, unsigned int height);
int gui_framebuffer_fitted(GuiFramebuffer *framebuffer, unsigned int width, unsigned int height);
#endif
