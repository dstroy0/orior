// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.h: the pixels the view draws and all that is drawn into them: a raster of BGRA words in a cell of the
// cellblock, a rectangle filled, blended by its color's alpha and clipped to the raster, a rectangle filled with its
// corners rounded, a line, a mask of coverage bytes blended in a color inside a clip, and a soft-edged dot. Nothing
// else in the view touches a pixel (raster.c)
#ifndef GUI_RASTER_H
#define GUI_RASTER_H
// a scale that leaves a size as it is: every scale in the window is a percent
#define GUI_SCALE_ONE 100

#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"

typedef struct GuiRasterWidthXCfg GuiRasterWidthXCfg;
typedef struct GuiRasterRadiusWorkCfg GuiRasterRadiusWorkCfg;
typedef struct GuiRasterWidthCfg GuiRasterWidthCfg;
typedef struct GuiRasterColorX0Cfg GuiRasterColorX0Cfg;
typedef struct GuiRasterXYColorCfg GuiRasterXYColorCfg;
typedef struct GuiRasterRectRadiusBackgroundCfg GuiRasterRectRadiusBackgroundCfg;
typedef struct GuiRasterXYColorCountCfg GuiRasterXYColorCountCfg;
typedef struct GuiRasterColorCfg GuiRasterColorCfg;
typedef struct GuiRasterRectRadiusCfg GuiRasterRectRadiusCfg;
typedef struct GuiRasterRadiusBackgroundCfg GuiRasterRadiusBackgroundCfg;
typedef struct GuiRasterXCfg GuiRasterXCfg;
typedef struct GuiRasterXYCfg GuiRasterXYCfg;
typedef struct GuiRasterRectColorCfg GuiRasterRectColorCfg;
typedef struct GuiRasterRadiusCfg GuiRasterRadiusCfg;
typedef struct GuiRasterReachCfg GuiRasterReachCfg;
typedef struct GuiRasterRectCfg GuiRasterRectCfg;
typedef struct GuiRasterCfg GuiRasterCfg;
typedef struct GuiRasterOneCfg GuiRasterOneCfg;

// A color: alpha, red, green and blue, each a byte, as 0xAARRGGBB, the word a BGRA pixel holds in memory on a
// little-endian machine
typedef unsigned int GuiColor;
#define GUI_RGBA(red_, green_, blue_, alpha_)                                                                          \
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
// NULL for any other raster. Last, the part a frame writes: an empty clip lets the frame write the whole raster, and a
// clip that is not empty stops every write outside it
typedef struct
{
    unsigned int width;
    unsigned int height;
    GuiColor *pixels;
    void *pages;
    unsigned long long pages_bytes;
    unsigned long long positioned;
    GuiRect clip;
} GuiRaster;

// the pixels a frame's row is laid in: its width rounded up to GUI_RASTER_PITCH, which lays every row on a 256-byte
// step a copy into the system's own surfaces reads whole
#define GUI_RASTER_PITCH 64u
unsigned int gui_raster_pitch(const GuiRasterWidthCfg *args);

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
int gui_rect_empty(const GuiRasterRectCfg *args);
int gui_rect_holds(const GuiRasterRectCfg *args);
GuiRect gui_rect_intersect(const GuiRasterOneCfg *args);
// a raster of `width` by `height` made clear in a cell of its own: 1, or 0 where the cell could not be taken; and its
// cell given back, the raster left holding no pixels
int gui_raster_init(const GuiRasterWidthXCfg *args);
// `rect` filled with `color`, blended by its alpha over what is there, no further than the raster; the same with its
// corners rounded by `radius` pixels, each corner's pixels covered by their share inside the arc; and a line from
// x0, y0 to x1, y1 in `color`, one pixel wide
void gui_raster_fill(const GuiRasterRectColorCfg *args);
void gui_raster_round(const GuiRasterRectColorCfg *args);
// The ground a cut lays bare: the raster it is read from, pixel for pixel, or `color` where the raster is NULL, with
// `over` blended over it by its alpha, and `grain_color` blended over that by the alpha the tile of `grain` bytes,
// `tile` a side, gives the pixel at the grain color's own alpha, where a grain is given; the tile is laid from the
// raster's own top left and repeated
typedef struct
{
    const GuiRaster *raster;
    GuiColor over;
    const unsigned char *grain;
    unsigned int tile;
    GuiColor color;
    GuiColor grain_color;
} GuiUnder;

// `color` blended over `background_fill` by the color's alpha, the pixel left opaque
GuiColor gui_color_over(const GuiRasterCfg *args);
// the corners of `rect` cut round by `radius`: each corner pixel blended with the ground by the share of it outside
// the arc, what was drawn inside the rectangle left standing inside the arcs
void gui_raster_corners_cut(const GuiRasterRectRadiusBackgroundCfg *args);
// a pane of `inner` tint on the table `background_fill` lays: every pixel of `rect`, or of its corners' squares alone
// where `corners_only` is set, the ground's own pixel with the ink at an alpha between the table's tint's outside the
// pane's arcs of `radius` and `inner`'s inside them, by the pixel's share inside, then the ground's grain; the two
// tints share one ink, and a pane's edge is the ink thickening, never a second layer
void gui_raster_pane(const GuiRasterRectRadiusBackgroundCfg *args);
// The lines the work stands in: along each edge its color and its width in pixels, the lines bending round the
// work's corners from one into the next
typedef struct
{
    GuiColor top;
    GuiColor right;
    GuiColor bottom;
    GuiColor left;
    int top_width;
    int right_width;
    int bottom_width;
    int left_width;
} GuiOutline;

// the work's outline laid: `work` cut round by `radius` to the `table`, `inside`, the work less its lines, cut round
// by a pixel less to the `clear` ground, the lines between the two laid along each edge and round each corner; and
// round each of the `count` holes the parts make in the work: the hole's corners inside the work filleted by the
// radius, the table laid inside each fillet's arc, the work's corners on the hole turned out to the table, and the
// line of the edge the hole reaches run along the hole's sides inside the work, round its fillets and its turns; a
// corner of the work a hole takes whole is the hole's
void gui_raster_outline(const GuiRasterRadiusWorkCfg *args);
// the squares of `notch`'s corners inside `work`, each the notch's fillet there, `radius` a side, no more than half
// the notch's shorter side, into `squares`, sized for four: their count
unsigned int gui_raster_notch_fillets(const GuiRasterRadiusCfg *args);
// `notch`, a hole in `work` reaching one or more of its edges, cut out of what was drawn in `work`: the notch filled
// with the ground, and the work's outline rounded by `radius` at each turn it makes round the notch, a corner of the
// notch inside the work filleted, what was drawn left standing outside the arc, and a corner of the notch on the
// work's edge cut, the ground laid outside the arc; a notch reaching none of the work's edges is cut as a rounded
// hole
void gui_raster_notch(const GuiRasterRadiusBackgroundCfg *args);
// `rect` grained inside its corners rounded by `radius`: each pixel blended with white by the alpha the tile of
// `alpha` bytes, `tile` a side, gives it, the tile laid from the raster's own top left and repeated
void gui_raster_grain(const GuiRasterRectRadiusCfg *args);
// a line from x0, y0 to x1, y1, each pixel on it blended in `color` by the color's alpha, cut to `clip` where `clip` is
// not empty
void gui_raster_line(const GuiRasterColorX0Cfg *args);
// `mask` laid with its top left at x, y, each pixel blended in `color` by its coverage and the color's alpha, no
// further than `clip` and the raster
void gui_raster_mask(const GuiRasterXYColorCfg *args);
// a picture's lanes laid with its top left at x, y, no further than `clip` and the raster: `width` by `height`
// pixels, row after row, each with the ink's red and blue lanes by its share with half of 255 in, `red_blue`, its
// green lane the same, `green`, and the rest of 255 the ground saves, `rest`; each pixel of the raster takes the
// ground's lanes by the rest plus the ink's, the one blend every drawing goes through, laid straight along
void gui_raster_lanes_layout(const GuiRasterWidthXCfg *args);
// a row of `width` coverage bytes stretched across the raster's row `y` from `left` to `right`, each a count of pixels
// with sixteen bits of fraction, no further than the raster: each pixel blended in `color` by the mean of what of the
// row lands on it and the share of it the row covers
unsigned int gui_raster_row_cover(const GuiRasterWidthCfg *args);
// a span of `count` pixels of coverage laid in `color` at x, y, y off the raster laying nothing: the blit a stage of a
// movement is made of
void gui_raster_span_layout(const GuiRasterXYColorCountCfg *args);
// the share of 255 of a pixel inside an arc of `reach` pixels, the pixel `across` and `down` whole pixels from the
// arc's center, read from a table made once for the reach: every rounded corner, fillet and turn of a frame reads it
unsigned int gui_raster_arc_cover(const GuiRasterReachCfg *args);
// the raster made `mask`'s picture in `color`: each pixel the color with the mask's cover as its alpha, straight
// over nothing, the raster's size the mask's, for an icon the system takes with its own alpha
void gui_raster_stencil(const GuiRasterXYColorCfg *args);
// `mask` in `color` laid at x, y over a picture that saves its own alpha, straight, no further than the picture; and
// such a picture laid on `raster` at x, y, no further than the raster, each pixel blended by its alpha and `opacity`
// of 255
void gui_raster_stenciled_over(const GuiRasterXYCfg *args);
void gui_raster_picture_layout(const GuiRasterXCfg *args);
// the 4 by 4 ordered threshold at x, y, 0 to 15, as the page's shader orders it: the pixel's own parity across and
// down weighs most, its pair's parity the rest
unsigned int gui_raster_order(const GuiRasterRectCfg *args);
// a dot about x, y of `radius`, each in pixels, soft-edged: each pixel takes `color` by the color's alpha and the
// pixel's share inside the dot, a pixel-wide ramp from all of it at the dot's edge to none a pixel out, the share
// dithered by a 4 by 4 ordered threshold below one level of 255, which puts a share between two levels down as both,
// cut to `clip` where `clip` is not empty
void gui_raster_dot(const GuiRasterColorCfg *args);
struct GuiRasterWidthXCfg
{
    unsigned int width;
    int x;
    int y;
    GuiRaster *raster;
    unsigned int height;
    GuiRect clip;
    const unsigned int *red_blue;
    const unsigned int *green;
    const unsigned int *rest;
};

struct GuiRasterRadiusWorkCfg
{
    GuiRaster *raster;
    unsigned int radius;
    GuiRect work;
    GuiRect inside;
    const GuiOutline *lines;
    const GuiRect *holes;
    unsigned int count;
    GuiUnder table;
    GuiUnder clear;
};

struct GuiRasterWidthCfg
{
    unsigned int width;
    const unsigned char *cover;
    int left;
    int right;
    unsigned int raster_width;
    unsigned char *out;
    int *x_pointer;
};

struct GuiRasterColorX0Cfg
{
    GuiRaster *raster;
    GuiColor color;
    int x0;
    int y0;
    int x1;
    int y1;
    GuiRect clip;
};

struct GuiRasterXYColorCfg
{
    int x;
    int y;
    GuiRaster *raster;
    GuiColor color;
    const GuiMask *mask_bits;
    GuiRect clip;
};

struct GuiRasterRectRadiusBackgroundCfg
{
    GuiRect rect;
    GuiRaster *raster;
    unsigned int radius;
    GuiUnder background_fill;
    GuiColor inner;
    int corners_only;
};

struct GuiRasterXYColorCountCfg
{
    int x;
    int y;
    GuiRaster *raster;
    GuiColor color;
    unsigned int count;
    const unsigned char *cover;
};

struct GuiRasterColorCfg
{
    GuiRaster *raster;
    GuiColor color;
    float x_f32;
    float y_f32;
    float radius_f32;
    GuiRect clip;
};

struct GuiRasterRectRadiusCfg
{
    GuiRect rect;
    GuiRaster *raster;
    unsigned int radius;
    const unsigned char *alpha;
    unsigned int tile;
    GuiColor color;
};

struct GuiRasterRadiusBackgroundCfg
{
    GuiRaster *raster;
    unsigned int radius;
    GuiUnder background_fill;
    GuiRect work;
    GuiRect notch;
};

struct GuiRasterXCfg
{
    int x;
    int y;
    GuiRaster *raster;
    const GuiRaster *picture_read;
    unsigned int opacity;
};

struct GuiRasterXYCfg
{
    int x;
    int y;
    GuiColor color;
    const GuiMask *mask_bits;
    GuiRaster *picture;
};

// The corners a rounded fill rounds, a bit each; none set rounds all four. A rounded fill with a `stroke` draws a line
// that many pixels wide in place of the whole: along each edge that meets a rounded corner, and round those corners
#define GUI_RASTER_CORNER_TOP_LEFT 1u
#define GUI_RASTER_CORNER_TOP_RIGHT 2u
#define GUI_RASTER_CORNER_BOTTOM_LEFT 4u
#define GUI_RASTER_CORNER_BOTTOM_RIGHT 8u

struct GuiRasterRectColorCfg
{
    GuiRect rect;
    GuiRaster *raster;
    GuiColor color;
    unsigned int radius;
    unsigned int corners;
    int stroke;
};

struct GuiRasterRadiusCfg
{
    unsigned int radius;
    GuiRect work;
    GuiRect notch;
    GuiRect *squares;
};

struct GuiRasterReachCfg
{
    unsigned int reach;
    unsigned int across;
    unsigned int down;
};

struct GuiRasterRectCfg
{
    GuiRect rect;
    int x;
    int y;
};

struct GuiRasterCfg
{
    GuiColor color;
    GuiColor background_color;
};

struct GuiRasterOneCfg
{
    GuiRect one;
    GuiRect other;
};

typedef struct
{
    unsigned int (*pitch)(const GuiRasterWidthCfg *args);
    unsigned int (*row_cover)(const GuiRasterWidthCfg *args);
} GuiRasterWidthNs;
EMBED_TABLE_LAYOUT(GuiRasterWidthNs, pitch, row_cover);
EMBED_TABLE_STORAGE GuiRasterWidthNs gui_raster_width EMBED_UNUSED = {
    .pitch = gui_raster_pitch,
    .row_cover = gui_raster_row_cover,
};

typedef struct
{
    int (*rect_empty)(const GuiRasterRectCfg *args);
    int (*rect_holds)(const GuiRasterRectCfg *args);
    unsigned int (*order)(const GuiRasterRectCfg *args);
} GuiRasterRectNs;
EMBED_TABLE_LAYOUT(GuiRasterRectNs, rect_empty, rect_holds, order);
EMBED_TABLE_STORAGE GuiRasterRectNs gui_raster_rect EMBED_UNUSED = {
    .rect_empty = gui_rect_empty,
    .rect_holds = gui_rect_holds,
    .order = gui_raster_order,
};

typedef struct
{
    GuiRect (*rect_intersect)(const GuiRasterOneCfg *args);
} GuiRasterOneNs;
EMBED_TABLE_LAYOUT(GuiRasterOneNs, rect_intersect);
EMBED_TABLE_STORAGE GuiRasterOneNs gui_raster_one EMBED_UNUSED = {
    .rect_intersect = gui_rect_intersect,
};

typedef struct
{
    int (*init)(const GuiRasterWidthXCfg *args);
    void (*lanes_layout)(const GuiRasterWidthXCfg *args);
} GuiRasterWidthXNs;
EMBED_TABLE_LAYOUT(GuiRasterWidthXNs, init, lanes_layout);
EMBED_TABLE_STORAGE GuiRasterWidthXNs gui_raster_width_x EMBED_UNUSED = {
    .init = gui_raster_init,
    .lanes_layout = gui_raster_lanes_layout,
};

typedef struct
{
    void (*fill)(const GuiRasterRectColorCfg *args);
    void (*round)(const GuiRasterRectColorCfg *args);
} GuiRasterRectColorNs;
EMBED_TABLE_LAYOUT(GuiRasterRectColorNs, fill, round);
EMBED_TABLE_STORAGE GuiRasterRectColorNs gui_raster_rect_color EMBED_UNUSED = {
    .fill = gui_raster_fill,
    .round = gui_raster_round,
};

typedef struct
{
    GuiColor (*color_over)(const GuiRasterCfg *args);
} CliRasterNs;
EMBED_TABLE_LAYOUT(CliRasterNs, color_over);
EMBED_TABLE_STORAGE CliRasterNs cli_raster EMBED_UNUSED = {
    .color_over = gui_color_over,
};

typedef struct
{
    void (*corners_cut)(const GuiRasterRectRadiusBackgroundCfg *args);
    void (*pane)(const GuiRasterRectRadiusBackgroundCfg *args);
} GuiRasterRectRadiusBackgroundNs;
EMBED_TABLE_LAYOUT(GuiRasterRectRadiusBackgroundNs, corners_cut, pane);
EMBED_TABLE_STORAGE GuiRasterRectRadiusBackgroundNs gui_raster_rect_radius_background EMBED_UNUSED = {
    .corners_cut = gui_raster_corners_cut,
    .pane = gui_raster_pane,
};

typedef struct
{
    void (*outline)(const GuiRasterRadiusWorkCfg *args);
} GuiRasterRadiusWorkNs;
EMBED_TABLE_LAYOUT(GuiRasterRadiusWorkNs, outline);
EMBED_TABLE_STORAGE GuiRasterRadiusWorkNs gui_raster_radius_work EMBED_UNUSED = {
    .outline = gui_raster_outline,
};

typedef struct
{
    unsigned int (*notch_fillets)(const GuiRasterRadiusCfg *args);
} GuiRasterRadiusNs;
EMBED_TABLE_LAYOUT(GuiRasterRadiusNs, notch_fillets);
EMBED_TABLE_STORAGE GuiRasterRadiusNs gui_raster_radius EMBED_UNUSED = {
    .notch_fillets = gui_raster_notch_fillets,
};

typedef struct
{
    void (*notch)(const GuiRasterRadiusBackgroundCfg *args);
} GuiRasterRadiusBackgroundNs;
EMBED_TABLE_LAYOUT(GuiRasterRadiusBackgroundNs, notch);
EMBED_TABLE_STORAGE GuiRasterRadiusBackgroundNs gui_raster_radius_background EMBED_UNUSED = {
    .notch = gui_raster_notch,
};

typedef struct
{
    void (*grain)(const GuiRasterRectRadiusCfg *args);
} GuiRasterRectRadiusNs;
EMBED_TABLE_LAYOUT(GuiRasterRectRadiusNs, grain);
EMBED_TABLE_STORAGE GuiRasterRectRadiusNs gui_raster_rect_radius EMBED_UNUSED = {
    .grain = gui_raster_grain,
};

typedef struct
{
    void (*line)(const GuiRasterColorX0Cfg *args);
} GuiRasterColorX0Ns;
EMBED_TABLE_LAYOUT(GuiRasterColorX0Ns, line);
EMBED_TABLE_STORAGE GuiRasterColorX0Ns gui_raster_color_x_0 EMBED_UNUSED = {
    .line = gui_raster_line,
};

typedef struct
{
    void (*mask_bits)(const GuiRasterXYColorCfg *args);
    void (*stencil)(const GuiRasterXYColorCfg *args);
} GuiRasterXYColorNs;
EMBED_TABLE_LAYOUT(GuiRasterXYColorNs, mask_bits, stencil);
EMBED_TABLE_STORAGE GuiRasterXYColorNs gui_raster_xy_color EMBED_UNUSED = {
    .mask_bits = gui_raster_mask,
    .stencil = gui_raster_stencil,
};

typedef struct
{
    void (*span_layout)(const GuiRasterXYColorCountCfg *args);
} GuiRasterXYColorCountNs;
EMBED_TABLE_LAYOUT(GuiRasterXYColorCountNs, span_layout);
EMBED_TABLE_STORAGE GuiRasterXYColorCountNs gui_raster_xy_color_count EMBED_UNUSED = {
    .span_layout = gui_raster_span_layout,
};

typedef struct
{
    unsigned int (*arc_cover)(const GuiRasterReachCfg *args);
} GuiRasterReachNs;
EMBED_TABLE_LAYOUT(GuiRasterReachNs, arc_cover);
EMBED_TABLE_STORAGE GuiRasterReachNs gui_raster_reach EMBED_UNUSED = {
    .arc_cover = gui_raster_arc_cover,
};

typedef struct
{
    void (*stenciled_over)(const GuiRasterXYCfg *args);
} GuiRasterXYNs;
EMBED_TABLE_LAYOUT(GuiRasterXYNs, stenciled_over);
EMBED_TABLE_STORAGE GuiRasterXYNs gui_raster_xy EMBED_UNUSED = {
    .stenciled_over = gui_raster_stenciled_over,
};

typedef struct
{
    void (*picture_layout)(const GuiRasterXCfg *args);
} GuiRasterXNs;
EMBED_TABLE_LAYOUT(GuiRasterXNs, picture_layout);
EMBED_TABLE_STORAGE GuiRasterXNs gui_raster_x EMBED_UNUSED = {
    .picture_layout = gui_raster_picture_layout,
};

typedef struct
{
    void (*dot)(const GuiRasterColorCfg *args);
} GuiRasterColorNs;
EMBED_TABLE_LAYOUT(GuiRasterColorNs, dot);
EMBED_TABLE_STORAGE GuiRasterColorNs gui_raster_color EMBED_UNUSED = {
    .dot = gui_raster_dot,
};

#endif
