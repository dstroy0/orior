// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pointer.h: the pointer the window draws itself, a sprite of glass in place of the system's cursor. Each shape is a
// set of figures, each a polygon with rounded corners, in design units of one pixel at the scale of 100. A shape can
// be another's figures turned in eighths of a turn. The sprites are baked once into a sheet at the window's scale in
// the palette's pointer colors. A baked sprite is a body of tinted glass, a rim that goes from one color to the other
// across the shape, a glint inside the upper left of the rim, and a soft shadow below. Each edge is smoothed against
// the pixels under it. The sheet is baked again only when the scale or a pointer color changes. A draw puts the baked
// picture over the raster. The pixels under a sprite are saved for each of the two rasters the window flips between.
// A move restores them and draws the sprite at its new place (pointer.c)
#ifndef GUI_WINDOW_POINTER_H
#define GUI_WINDOW_POINTER_H
#include "../raster/raster.h"
#include "palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiPointerCfg GuiPointerCfg;
typedef struct GuiPointerPlaceCfg GuiPointerPlaceCfg;
typedef struct GuiPointerReadCfg GuiPointerReadCfg;
typedef struct GuiPointerRasterCfg GuiPointerRasterCfg;
typedef struct GuiPointerShapeCfg GuiPointerShapeCfg;

// The shapes: the arrow; the text beam; the hand on a part a press acts on; the four resize arrows, up and down, left
// and right, and the two diagonals; the compass of the glide at rest; and the arrow with an hourglass while the window
// works. Then the compass with one of its eight arrows lit, the way the glide goes: north, north east, east, south
// east, south, south west, west and north west
#define GUI_POINTER_ARROW 0u
#define GUI_POINTER_TEXT 1u
#define GUI_POINTER_HAND 2u
#define GUI_POINTER_RESIZE_NS 3u
#define GUI_POINTER_RESIZE_EW 4u
#define GUI_POINTER_RESIZE_NWSE 5u
#define GUI_POINTER_RESIZE_NESW 6u
#define GUI_POINTER_MOVE 7u
#define GUI_POINTER_BUSY 8u
#define GUI_POINTER_GLIDE_N 9u
#define GUI_POINTER_GLIDE_NE 10u
#define GUI_POINTER_GLIDE_E 11u
#define GUI_POINTER_GLIDE_SE 12u
#define GUI_POINTER_GLIDE_S 13u
#define GUI_POINTER_GLIDE_SW 14u
#define GUI_POINTER_GLIDE_W 15u
#define GUI_POINTER_GLIDE_NW 16u
#define GUI_POINTER_SHAPES 17u

// A sprite's largest side in design units. The least and the most scale a sprite is baked at, in the window's percent.
// The pixels of margin round a sprite for its smoothed edge and its shadow. The largest side of a baked sprite in
// pixels, and the pixels saved under it. The points a figure has at most and the figures a shape has at most. The
// rasters a window flips between
#define GUI_POINTER_SIDE_MAX 24u
#define GUI_POINTER_SCALE_LEAST 100
#define GUI_POINTER_SCALE_MOST 300
#define GUI_POINTER_MARGIN 3u
#define GUI_POINTER_PIXELS_MAX ((GUI_POINTER_SIDE_MAX * 3u) + (2u * GUI_POINTER_MARGIN))
#define GUI_POINTER_UNDER_MAX (GUI_POINTER_PIXELS_MAX * GUI_POINTER_PIXELS_MAX)
#define GUI_POINTER_POINTS 12u
#define GUI_POINTER_FIGURES 9u
#define GUI_POINTER_RASTERS 2u
_Static_assert((GUI_POINTER_SCALE_MOST / GUI_SCALE_ONE) == 3, "GUI_POINTER_PIXELS_MAX holds a sprite at the most scale");

// A figure of a sprite: its points in design units, in order round its edge, and the radius in design units its
// corners are rounded by. The rounding grows the figure by the radius. A figure of one point is a disc of that radius
typedef struct
{
    unsigned int points;
    double corner;
    double point[GUI_POINTER_POINTS][2];
} GuiPointerFigure;

// A sprite: its size in design units and its hot point in them; the eighths of a turn its figures are turned by, round
// the center of the sprite; the place in the figures' units that the center of the sprite shows; its figures; and the
// figure that is lit, its body the rim's color, 0 for none; and the figures not drawn, a bit for each
typedef struct
{
    double width;
    double height;
    double hot_x;
    double hot_y;
    unsigned int eighths;
    double center_x;
    double center_y;
    unsigned int figures;
    GuiPointerFigure figure[GUI_POINTER_FIGURES];
    unsigned int lit;
    unsigned int hidden;
} GuiPointerSprite;

// The pixels saved under a sprite on one raster: the raster's pixels they came from, the rectangle, whether any are
// saved, and the pixels, row after row of the rectangle's width
typedef struct
{
    const GuiColor *pixels;
    GuiRect rect;
    int saved;
    GuiColor under[GUI_POINTER_UNDER_MAX];
} GuiPointerMark;

// The sprites baked at one scale in the palette's pointer colors. Each shape is a picture of its rectangle, its margin
// with it, row after row of the rectangle's width. A pixel holds its color multiplied by its alpha, and the alpha it
// covers what is under it with. A draw puts this picture over the raster and computes no figure. `made` is 0 until the
// first bake
typedef struct
{
    int made;
    int scale;
    GuiColor shadow;
    GuiColor line;
    GuiColor line_end;
    GuiColor glint;
    GuiColor fill;
    GuiColor pixels[GUI_POINTER_SHAPES][GUI_POINTER_UNDER_MAX];
} GuiPointerSheet;

// The pointer: where its hot point is in the client's pixels, its shape, whether it is on the window, the pixels
// saved under it on each raster, and its sprites baked
typedef struct
{
    int x;
    int y;
    unsigned int shape;
    int shown;
    GuiPointerMark mark[GUI_POINTER_RASTERS];
    GuiPointerSheet sheet;
} GuiPointer;

// the pointer made off the window with nothing saved
void gui_pointer_init(const GuiPointerCfg *args);
// the pointer put at x, y in `shape`, on the window or off it: 1 where any of them changed
int gui_pointer_place(const GuiPointerPlaceCfg *args);
// the sprite of `shape`, the arrow's for a shape not known
const GuiPointerSprite *gui_pointer_sprite(const GuiPointerShapeCfg *args);
// the scale a sprite is baked at for `scale`, the window's percent: the scale clamped to GUI_POINTER_SCALE_LEAST to
// GUI_POINTER_SCALE_MOST
int gui_pointer_scale(const GuiPointerReadCfg *args);
// the rectangle the sprite covers at its place at `scale`, its shadow with it: empty where the pointer is off the
// window
GuiRect gui_pointer_rect(const GuiPointerReadCfg *args);
// The sprite drawn on `raster` at its place, the pixels under it saved first: the rectangle drawn. A pointer off the
// window draws nothing and forgets the pixels saved on `raster`, which the caller has drawn the frame over: the
// rectangle is then empty. A raster with a clip has each write cut to it; the caller gives a clip that holds the
// whole rectangle, and the pixels saved are then the frame's
GuiRect gui_pointer_draw(const GuiPointerRasterCfg *args);
// the pixels saved under the sprite on `raster` put back, nothing saved after: the rectangle put back, empty where none
// is saved
GuiRect gui_pointer_restore(const GuiPointerRasterCfg *args);
// the sprite moved on `raster`: the pixels saved put back and the sprite drawn at its place: the smallest rectangle
// that holds both, empty where neither is
GuiRect gui_pointer_redrawn(const GuiPointerRasterCfg *args);
// the rectangle of the pixels saved under the sprite on `raster`, empty where none is saved
GuiRect gui_pointer_saved(const GuiPointerRasterCfg *args);

struct GuiPointerCfg
{
    GuiPointer *pointer;
};

struct GuiPointerPlaceCfg
{
    GuiPointer *pointer;
    int x;
    int y;
    unsigned int shape;
    int shown;
};

struct GuiPointerReadCfg
{
    const GuiPointer *pointer_read;
    int scale;
};

struct GuiPointerRasterCfg
{
    GuiPointer *pointer;
    GuiRaster *raster;
    const GuiPalette *palette;
    int scale;
};

struct GuiPointerShapeCfg
{
    unsigned int shape;
};

typedef struct
{
    void (*init)(const GuiPointerCfg *args);
} GuiPointerNs;
EMBED_TABLE_LAYOUT(GuiPointerNs, init);
EMBED_TABLE_STORAGE GuiPointerNs gui_pointer EMBED_UNUSED = {
    .init = gui_pointer_init,
};

typedef struct
{
    int (*place)(const GuiPointerPlaceCfg *args);
} GuiPointerPlaceNs;
EMBED_TABLE_LAYOUT(GuiPointerPlaceNs, place);
EMBED_TABLE_STORAGE GuiPointerPlaceNs gui_pointer_place_table EMBED_UNUSED = {
    .place = gui_pointer_place,
};

typedef struct
{
    const GuiPointerSprite *(*sprite)(const GuiPointerShapeCfg *args);
} GuiPointerShapeNs;
EMBED_TABLE_LAYOUT(GuiPointerShapeNs, sprite);
EMBED_TABLE_STORAGE GuiPointerShapeNs gui_pointer_shape EMBED_UNUSED = {
    .sprite = gui_pointer_sprite,
};

typedef struct
{
    int (*scale)(const GuiPointerReadCfg *args);
    GuiRect (*rect)(const GuiPointerReadCfg *args);
} GuiPointerReadNs;
EMBED_TABLE_LAYOUT(GuiPointerReadNs, scale, rect);
EMBED_TABLE_STORAGE GuiPointerReadNs gui_pointer_read EMBED_UNUSED = {
    .scale = gui_pointer_scale,
    .rect = gui_pointer_rect,
};

typedef struct
{
    GuiRect (*draw)(const GuiPointerRasterCfg *args);
    GuiRect (*restore)(const GuiPointerRasterCfg *args);
    GuiRect (*redrawn)(const GuiPointerRasterCfg *args);
    GuiRect (*saved)(const GuiPointerRasterCfg *args);
} GuiPointerRasterNs;
EMBED_TABLE_LAYOUT(GuiPointerRasterNs, draw, restore, redrawn, saved);
EMBED_TABLE_STORAGE GuiPointerRasterNs gui_pointer_raster EMBED_UNUSED = {
    .draw = gui_pointer_draw,
    .restore = gui_pointer_restore,
    .redrawn = gui_pointer_redrawn,
    .saved = gui_pointer_saved,
};

#endif
