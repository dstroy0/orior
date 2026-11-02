// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.h: the pixels the window draws and all that is drawn into them: a raster of BGRA words in a cell of the
// cellblock, a rectangle filled, blended by its color's alpha and clipped to the raster, a rectangle filled with its
// corners rounded, a line, a mask of coverage bytes blended in a color inside a clip, and a soft-edged dot. Nothing
// else in the window touches a pixel (raster.c)
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

// A raster: its size and its pixels, row after row with no gap, in a cell of the cellblock; and where its pixels are
// a framebuffer's, the pages they lie in, the bytes of those pages and how far into them the pixels start, the pages
// NULL for any other raster
typedef struct
{
    unsigned int width;
    unsigned int height;
    GuiColor *pixels;
    void *pages;
    unsigned long long pages_bytes;
    unsigned long long placed;
} GuiRaster;

// the pixels a frame's row is laid in: its width rounded up to GUI_RASTER_PITCH, which lays every row on a 256-byte
// step a copy into the system's own surfaces reads whole
#define GUI_RASTER_PITCH 64u
unsigned int gui_raster_pitch(unsigned int width);

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
// The ground a cut lays bare: the raster it is read from, pixel for pixel, or `color` where the raster is NULL, with
// `over` blended over it by its alpha, and white blended over that by the alpha the tile of `grain` bytes, `tile` a
// side, gives the pixel, where a grain is given; the tile is laid from the raster's own top left and repeated
typedef struct
{
    const GuiRaster *raster;
    GuiColor over;
    const unsigned char *grain;
    unsigned int tile;
    GuiColor color;
} GuiUnder;

// `color` blended over `ground` by the color's alpha, the pixel left opaque
GuiColor gui_color_over(GuiColor ground, GuiColor color);
// the corners of `rect` cut round by `radius`: each corner pixel blended with the ground by the share of it outside
// the arc, what was drawn inside the rectangle left standing inside the arcs
void gui_raster_corners_cut(GuiRaster *raster, GuiRect rect, unsigned int radius, GuiUnder ground);
// a pane of `inner` tint on the table `ground` lays: every pixel of `rect`, or of its corners' squares alone where
// `corners_only` is set, the ground's own pixel with the ink at an alpha between the table's tint's outside the
// pane's arcs of `radius` and `inner`'s inside them, by the pixel's share inside, then the ground's grain; the two
// tints share one ink, and a pane's edge is the ink thickening, never a second layer
void gui_raster_paned(GuiRaster *raster, GuiUnder ground, GuiRect rect, unsigned int radius, GuiColor inner,
                      int corners_only);
// The lines the work stands in: along each edge its color and its width in pixels, the lines bending round the
// work's corners from one into the next
typedef struct
{
    GuiColor top;
    GuiColor right;
    GuiColor bottom;
    GuiColor left;
    int top_wide;
    int right_wide;
    int bottom_wide;
    int left_wide;
} GuiOutline;

// the work's outline laid: `work` cut round by `radius` to the `table`, `inside`, the work less its lines, cut round
// by a pixel less to the `clear` ground, the lines between the two laid along each edge and round each corner; and
// round each of the `count` holes the parts make in the work: the hole's corners inside the work filleted by the
// radius, the table laid inside each fillet's arc, the work's corners on the hole turned out to the table, and the
// line of the edge the hole reaches run along the hole's sides inside the work, round its fillets and its turns; a
// corner of the work a hole takes whole is the hole's
void gui_raster_outlined(GuiRaster *raster, GuiRect work, GuiRect inside, unsigned int radius, const GuiOutline *lines,
                         const GuiRect *holes, unsigned int count, GuiUnder table, GuiUnder clear);
// the squares of `notch`'s corners inside `work`, each the notch's fillet there, `radius` a side, no more than half
// the notch's shorter side, into `squares`, sized for four: their count
unsigned int gui_raster_notch_fillets(GuiRect work, GuiRect notch, unsigned int radius, GuiRect *squares);
// `notch`, a hole in `work` reaching one or more of its edges, cut out of what was drawn in `work`: the notch filled
// with the ground, and the work's outline rounded by `radius` at each turn it makes round the notch, a corner of the
// notch inside the work filleted, what was drawn left standing outside the arc, and a corner of the notch on the
// work's edge cut, the ground laid outside the arc; a notch reaching none of the work's edges is cut as a rounded
// hole
void gui_raster_notched(GuiRaster *raster, GuiRect work, GuiRect notch, unsigned int radius, GuiUnder ground);
// `rect` grained inside its corners rounded by `radius`: each pixel blended with white by the alpha the tile of
// `alpha` bytes, `tile` a side, gives it, the tile laid from the raster's own top left and repeated
void gui_raster_grained(GuiRaster *raster, GuiRect rect, unsigned int radius, const unsigned char *alpha, unsigned int tile);
void gui_raster_line(GuiRaster *raster, int x0, int y0, int x1, int y1, GuiColor color);
// `mask` laid with its top left at x, y, each pixel blended in `color` by its coverage and the color's alpha, no
// further than `clip` and the raster
void gui_raster_masked(GuiRaster *raster, const GuiMask *mask, int x, int y, GuiColor color, GuiRect clip);
// a picture's lanes laid with its top left at x, y, no further than `clip` and the raster: `width` by `height`
// pixels, row after row, each with the ink's red and blue lanes by its share with half of 255 in, `red_blue`, its
// green lane the same, `green`, and the rest of 255 the ground keeps, `rest`; each pixel of the raster takes the
// ground's lanes by the rest plus the ink's, the one blend every drawing goes through, laid straight along
void gui_raster_lanes_laid(GuiRaster *raster, const unsigned int *red_blue, const unsigned int *green, const unsigned int *rest,
                           unsigned int width, unsigned int height, int x, int y, GuiRect clip);
// a row of `width` coverage bytes stretched across the raster's row `y` from `left` to `right`, each a count of pixels
// with sixteen bits of fraction, no further than the raster: each pixel blended in `color` by the mean of what of the
// row lands on it and the share of it the row covers
unsigned int gui_raster_row_covered(const unsigned char *cover, unsigned int width, int left, int right, unsigned int raster_width,
                                    unsigned char *out, int *x);
// a span of `count` pixels of coverage laid in `color` at x, y, y off the raster laying nothing: the blit a stage of a
// movement is made of
void gui_raster_span_laid(GuiRaster *raster, int x, int y, const unsigned char *cover, unsigned int count, GuiColor color);
// the share of 255 of a pixel inside an arc of `reach` pixels, the pixel `across` and `down` whole pixels from the arc's
// center, read from a table made once for the reach: every rounded corner, fillet and turn of a frame reads it
unsigned int gui_raster_arc_cover(unsigned int reach, unsigned int across, unsigned int down);
// the raster made `mask`'s picture in `color`: each pixel the color with the mask's cover as its alpha, straight
// over nothing, the raster's size the mask's, for an icon the system takes with its own alpha
void gui_raster_stenciled(GuiRaster *raster, const GuiMask *mask, GuiColor color);
// `mask` in `color` laid at x, y over a picture that keeps its own alpha, straight, no further than the picture; and
// such a picture laid on `raster` at x, y, no further than the raster, each pixel blended by its alpha and `opacity`
// of 255
void gui_raster_stenciled_over(GuiRaster *picture, const GuiMask *mask, int x, int y, GuiColor color);
void gui_raster_picture_laid(GuiRaster *raster, const GuiRaster *picture, int x, int y, unsigned int opacity);
// the 4 by 4 ordered threshold at x, y, 0 to 15, as the page's shader orders it: the pixel's own parity across and
// down weighs most, its pair's parity the rest
unsigned int gui_raster_ordered(int x, int y);
// a dot about x, y of `radius`, each in pixels, soft-edged: each pixel takes `color` by the color's alpha and the
// pixel's share inside the dot, a pixel-wide ramp from all of it at the dot's edge to none a pixel out, the share
// dithered by a 4 by 4 ordered threshold below one level of 255, which puts a share between two levels down as both
void gui_raster_dotted(GuiRaster *raster, float x, float y, float radius, GuiColor color);
#endif
