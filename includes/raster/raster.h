// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.h: pixels and all that is drawn into them: a raster of BGRA words over pixels its caller passes it, a
// rectangle filled, blended by its color's alpha and clipped to the raster, a rectangle filled with its corners
// rounded, a line, a mask of coverage bytes blended in a color inside a clip, and a soft-edged dot. Nothing else
// touches a pixel (raster.c)
#ifndef GUI_RASTER_H
#define GUI_RASTER_H
// a scale that leaves a size as it is: every scale is a percent
#define GUI_SCALE_ONE 100

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

// A raster. It holds its size and its pixels, row after row with no gap, in a cell of the cellblock. Where its pixels
// belong to a framebuffer, it holds their pages, the bytes of those pages and how far into them the pixels start. The
// pages are NULL for any other raster. Last, it holds the part that a frame writes. An empty clip lets the frame write
// the whole raster, and a clip that is not empty stops each write outside it
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

// the pixels that hold a row of a frame: its width, rounded up to GUI_RASTER_PITCH. That puts each row on a 256-byte
// step, which a copy into the surfaces of the system reads whole
#define GUI_RASTER_PITCH 64u
unsigned int gui_raster_pitch(const GuiRasterWidthCfg *args);

// A mask of coverage. It holds its size, and its top left from the pen on the baseline. It holds the share of each
// pixel of what it covers, 0 to 255, row after row
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
// a raster of `width` by `height` made clear over `pixels`, which hold width by height words: 1, or 0 where there is
// no size or no pixels
int gui_raster_init(const GuiRasterWidthXCfg *args);
// `rect` filled with `color`, blended by its alpha over what is there, no further than the raster. The second call
// does the same with its corners rounded by `radius` pixels. The pixels of each corner get their share inside the arc.
// The third call draws a line from x0, y0 to x1, y1 in `color`, one pixel wide
void gui_raster_fill(const GuiRasterRectColorCfg *args);
void gui_raster_round(const GuiRasterRectColorCfg *args);
// The ground that a cut lays bare. It is the source raster, pixel for pixel, or `color` where the raster is NULL. `over`
// goes over it by its alpha.
//
// Where the caller gives a grain, `grain_color` goes over that. Its alpha is the alpha that the tile of `grain` bytes,
// `tile` a side, gives the pixel, at the alpha of the grain color. The tile starts from the top left of the raster and
// repeats
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
// the corners of `rect` cut round by `radius`. Each corner pixel blends with the ground by its share outside the arc.
// The pixels inside the rectangle stay inside the arcs
void gui_raster_corners_cut(const GuiRasterRectRadiusBackgroundCfg *args);
// a pane of `inner` tint on the table that `background_fill` positions. It writes each pixel of `rect`, or of the
// squares of its corners alone where `corners_only` is on.
//
// Each pixel is the pixel of the ground with the ink. Outside the arcs of `radius` of the pane, the ink has the alpha of
// the tint of the table. Inside them it has the alpha of `inner`, by the share of the pixel inside. The grain of the
// ground goes over that.
//
// The two tints share one ink. The edge of a pane is where the ink gets thicker, and never a second layer
void gui_raster_pane(const GuiRasterRectRadiusBackgroundCfg *args);
// The lines round the work. Each edge has its color and its width in pixels. The lines bend round the corners of the
// work from one into the next
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

// the outline of the work. `work` gets its corners cut round by `radius` to the `table`. `inside`, the work less its
// lines, gets its corners cut round by a pixel less to the `clear` ground. The lines go between the two, along each edge
// and round each corner.
//
// The parts make `count` holes in the work. The corners of each hole inside the work get a fillet of the radius, with
// the table inside the arc of each fillet. The corners of the work on the hole turn out to the table.
//
// The line of the edge that the hole reaches runs along the sides of the hole inside the work, round its fillets and
// turns. A corner of the work that a hole takes whole belongs to the hole
void gui_raster_outline(const GuiRasterRadiusWorkCfg *args);
// the squares of the corners of `notch` inside `work`, into `squares`, with room for four. Each is the fillet of the
// notch there, `radius` a side, half of the shorter side of the notch at max. The result is their count
unsigned int gui_raster_notch_fillets(const GuiRasterRadiusCfg *args);
// `notch`, a hole in `work` that reaches one or more of its edges, cut out of the pixels of `work`. The ground fills
// the notch. The outline of the work gets a round of `radius` at each turn that it makes round the notch.
//
// A corner of the notch inside the work gets a fillet, and the pixels outside the arc stay. A corner of the notch on
// the edge of the work gets a cut, with the ground outside the arc. A notch that reaches no edge of the work is a
// rounded hole
void gui_raster_notch(const GuiRasterRadiusBackgroundCfg *args);
// a grain inside `rect`, with its corners rounded by `radius`. Each pixel blends with white by the alpha that the tile
// of `alpha` bytes, `tile` a side, gives it. The tile starts from the top left of the raster and repeats
void gui_raster_grain(const GuiRasterRectRadiusCfg *args);
// a line from x0, y0 to x1, y1, each pixel on it blended in `color` by the color's alpha, cut to `clip` where `clip` is
// not empty
void gui_raster_line(const GuiRasterColorX0Cfg *args);
// `mask` positioned with its top left at x, y, each pixel blended in `color` by its coverage and the color's alpha, no
// further than `clip` and the raster
void gui_raster_mask(const GuiRasterXYColorCfg *args);
// the lanes of a picture, with its top left at x, y, no further than `clip` and the raster. The picture is `width` by
// `height` pixels, row after row.
//
// Each pixel has the red and blue lanes of the ink by its share, with half of 255 in, in `red_blue`. Its green lane is
// the same, in `green`. The rest of 255, the part of the ground that stays, is in `rest`.
//
// Each pixel of the raster takes the lanes of the ground by the rest, plus the lanes of the ink. Each drawing goes
// through this blend, straight along
void gui_raster_lanes_layout(const GuiRasterWidthXCfg *args);
// a row of `width` coverage bytes, stretched across the row `y` of the raster from `left` to `right`. Each is a count
// of pixels with sixteen bits of fraction, no further than the raster. Each pixel blends in `color` by two things. They
// are the mean of the part of the row on it, and the share of it that the row covers
unsigned int gui_raster_row_cover(const GuiRasterWidthCfg *args);
// a span of `count` pixels of coverage in `color` at x, y. A y off the raster positions nothing. This is the blit that
// makes a stage of a movement
void gui_raster_span_layout(const GuiRasterXYColorCountCfg *args);
// the share of 255 of a pixel inside an arc of `reach` pixels. The pixel is `across` and `down` whole pixels from the
// center of the arc. The share comes from a table made once for the reach. Each rounded corner, fillet and turn of a
// frame reads it
unsigned int gui_raster_arc_cover(const GuiRasterReachCfg *args);
// the raster made into the picture of `mask` in `color`. Each pixel is the color, with the cover of the mask as its
// alpha, straight over nothing. The raster has the size of the mask. This is for an icon that the system takes with
// its own alpha
void gui_raster_stencil(const GuiRasterXYColorCfg *args);
// `mask` in `color` positioned at x, y over a picture that saves its own alpha, straight, no further than the picture. And
// such a picture positioned on `raster` at x, y, no further than the raster, each pixel blended by its alpha and `opacity`
// of 255
void gui_raster_stenciled_over(const GuiRasterXYCfg *args);
void gui_raster_picture_layout(const GuiRasterXCfg *args);
// the 4 by 4 ordered threshold at x, y, 0 to 15, in the order of the shader of the page. The parity of the pixel across
// and down weighs most, and the parity of its pair weighs the rest
unsigned int gui_raster_order(const GuiRasterRectCfg *args);
// a soft dot about x, y of `radius`, each in pixels. Each pixel takes `color` by the alpha of the color and the share of
// the pixel inside the dot.
//
// The share is a ramp one pixel wide, from all of it at the edge of the dot to none a pixel out. A 4 by 4 ordered
// threshold below one level of 255 dithers the share. A share between two levels then goes down as both. The dot cuts
// to `clip` where `clip` is not empty
void gui_raster_dot(const GuiRasterColorCfg *args);
struct GuiRasterWidthXCfg
{
    unsigned int width;
    int x;
    int y;
    GuiRaster *raster;
    unsigned int height;
    GuiRect clip;
    GuiColor *pixels;
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

// The corners that a rounded fill rounds, a bit each. With no bit on, it rounds all four. A rounded fill with a `stroke`
// draws a line that many pixels wide, in place of the whole fill. The line runs along each edge that meets a rounded
// corner, and round those corners
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
    GuiColor (*over)(const GuiRasterCfg *args);
} GuiColorNs;
EMBED_TABLE_LAYOUT(GuiColorNs, over);
EMBED_TABLE_STORAGE GuiColorNs gui_color EMBED_UNUSED = {
    .over = gui_color_over,
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
