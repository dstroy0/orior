// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.h: the pixels the window draws and all that is drawn into them: a raster of BGRA words in a cell of the
// cellblock, a rectangle filled, blended by its color's alpha and clipped to the raster, a rectangle filled with its
// corners rounded, a line, and a mask of coverage bytes blended in a color inside a clip. Nothing else in the window
// touches a pixel (raster.c)
#ifndef GUI_RASTER_H
#define GUI_RASTER_H
#include "../../cli/src/command_line/cells.h"

// A color: alpha, red, green and blue, each a byte, as 0xAARRGGBB, the word a BGRA pixel holds in memory on a
// little-endian machine
typedef unsigned int GuiColor;
#define GUI_RGBA(red_, green_, blue_, alpha_)                                                                        \
    (((GuiColor)(alpha_) << 24u) | ((GuiColor)(red_) << 16u) | ((GuiColor)(green_) << 8u) | (GuiColor)(blue_))
#define GUI_RGB(red_, green_, blue_) GUI_RGBA((red_), (green_), (blue_), 0xffu)
#define GUI_ALPHA_OF(color_) (((color_) >> 24u) & 0xffu)
#define GUI_RED_OF(color_) (((color_) >> 16u) & 0xffu)
#define GUI_GREEN_OF(color_) (((color_) >> 8u) & 0xffu)
#define GUI_BLUE_OF(color_) ((color_) & 0xffu)

// A rectangle in pixels: its top left and its size, empty where either side is 0 or less
typedef struct
{
    int x;
    int y;
    int width;
    int height;
} GuiRect;

// A raster: its size and its pixels, row after row with no gap, in a cell of the cellblock
typedef struct
{
    unsigned int width;
    unsigned int height;
    GuiColor *pixels;
} GuiRaster;

// A mask of coverage: its size, where its top left stands from the pen on the baseline, and each pixel's share of
// what it covers, 0 to 255, row after row
typedef struct
{
    unsigned int width;
    unsigned int height;
    int left;
    int top;
    unsigned char *cover;
} GuiMask;

// whether a rectangle is empty; whether it holds the pixel at x, y; and the overlap of two, empty where they are apart
int gui_rect_empty(GuiRect rect);
int gui_rect_holds(GuiRect rect, int x, int y);
GuiRect gui_rect_met(GuiRect one, GuiRect other);
// a raster of `width` by `height` made clear in a cell of its own: 1, or 0 where the cell could not be taken; and its
// cell given back, the raster left holding no pixels
int gui_raster_made(GuiRaster *raster, unsigned int width, unsigned int height);
void gui_raster_given(GuiRaster *raster);
// `rect` filled with `color`, blended by its alpha over what is there, no further than the raster; the same with its
// corners rounded by `radius` pixels, each corner's pixels covered by their share inside the arc; and a line from
// x0, y0 to x1, y1 in `color`, one pixel wide
void gui_raster_filled(GuiRaster *raster, GuiRect rect, GuiColor color);
void gui_raster_rounded(GuiRaster *raster, GuiRect rect, unsigned int radius, GuiColor color);
// the corners of `rect` cut round by `radius`: each corner pixel blended with `ground` by the share of it outside the
// arc, what was drawn inside the rectangle left standing inside the arcs
void gui_raster_corners_cut(GuiRaster *raster, GuiRect rect, unsigned int radius, GuiColor ground);
// `notch`, a hole in `work` reaching one or more of its edges, cut out of what was drawn in `work`: the notch filled
// with `ground`, and the work's outline rounded by `radius` at each turn it makes round the notch, a corner of the
// notch inside the work filleted, what was drawn left standing outside the arc, and a corner of the notch on the
// work's edge cut, the ground laid outside the arc; a notch reaching none of the work's edges is cut as a rounded
// hole
void gui_raster_notched(GuiRaster *raster, GuiRect work, GuiRect notch, unsigned int radius, GuiColor ground);
void gui_raster_line(GuiRaster *raster, int x0, int y0, int x1, int y1, GuiColor color);
// `mask` laid with its top left at x, y, each pixel blended in `color` by its coverage and the color's alpha, no
// further than `clip` and the raster
void gui_raster_masked(GuiRaster *raster, const GuiMask *mask, int x, int y, GuiColor color, GuiRect clip);
#endif
