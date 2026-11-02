// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ground.h: the window's ground: the lattice behind every part, laid out over the window in the page's pixels, its
// dots drawn soft-edged and dithered over the deep color into a raster of the ground's own that every frame starts
// from; drawn again only at a moment the loop shows a change, which the lattice finds, and its clock standing still
// while the window is off the screen, the loop taking up where it stood (ground.c)
#ifndef GUI_GROUND_H
#define GUI_GROUND_H
#include "lattice.h"
#include "palette.h"

// the pens the dots are shaded from, as the page's lattice/pens.js writes them: the share of ink a point puts down
// from the least ordered, the faintest, to the most ordered, the brightest, the faintest dimmed by the dim; and a
// point's radius in the page's pixels, the least ordered the smallest, growing by the spread as it orders
#define GUI_GROUND_DIM 0.85
#define GUI_GROUND_FAINTEST (0.426 * GUI_GROUND_DIM)
#define GUI_GROUND_BRIGHTEST 0.42
#define GUI_GROUND_RADIUS 0.59
#define GUI_GROUND_RADIUS_SPREAD 0.16

// The ground: its raster, the deep color with the dots over it, a view of the region's ground raster; its `slot`
// of the lattice; the size and scale it is laid out for and whether it is; the shades its dots take; its clock, when
// it began, the milliseconds lost to every stretch it was kept still and when the stretch holding it now began, 0
// while none does; the loop's moment it was last drawn at, in seconds, and the moment the loop next shows a change
// after it, found once as it is drawn; and how many times it has been drawn
typedef struct
{
    GuiRaster raster;
    uint32_t slot;
    unsigned int width;
    unsigned int height;
    float scale;
    int laid;
    LatticeShades shades;
    unsigned long long began_ms;
    unsigned long long lost_ms;
    unsigned long long paused_since_ms;
    double drawn_at;
    double shows_at;
    unsigned int frames;
} GuiGround;

// the ground made at `now`, its `slot` of the lattice taken: 1, or 0 where all LATTICE_SLOTS are taken; and given
// back, its raster and its `slot` with it
int gui_ground_made(GuiGround *ground, unsigned long long now);
void gui_ground_given(GuiGround *ground);
// the ground laid out for a window `width` by `height` at `scale` in `palette`'s colors at `now` on `raster`, the
// region's ground raster at that size, which the ground draws into and never gives back: the lattice laid over the
// window in the page's pixels, its shades made from the palette's lattice colors, and the raster drawn: 1, or 0 where
// the lattice's cells could not be taken, the raster drawn in the deep color alone
int gui_ground_laid(GuiGround *ground, const GuiRaster *raster, unsigned int width, unsigned int height, float scale,
                    const GuiPalette *palette, unsigned long long now);
// the ground drawn at the loop's moment at `now`: the deep color, and every dot over it where it stands
void gui_ground_drawn(GuiGround *ground, const GuiPalette *palette, unsigned long long now);
// the tick come at `now`: the ground drawn again where the loop has moved since it was drawn enough to be seen: 1
// where it was, a frame owed, else 0
int gui_ground_ticked(GuiGround *ground, const GuiPalette *palette, unsigned long long now);
// the milliseconds from `now` until the loop next shows a change, 0 for never while the ground is kept still or not
// laid
unsigned long long gui_ground_due(const GuiGround *ground, unsigned long long now);
// the ground kept still from `now` where `paused` is 1, its clock standing, or let go where 0, the clock taking up
// where it stood
void gui_ground_paused(GuiGround *ground, int paused, unsigned long long now);
// the loop's moment at `now`, in seconds
double gui_ground_seconds(const GuiGround *ground, unsigned long long now);
#endif
