// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pointer.h: the pointer the window draws itself, a sprite of cells in place of the system's cursor. Each shape is one
// or two layers of a cell table, a layer turned on its diagonal or mirrored where the shape is another's turned. The
// sprites are baked once into a sheet: three passes, the shadow, the line and the fill, each cell a square of pixels at
// the window's scale. They are baked again only when the scale or a pointer color changes. A draw puts the baked
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

// the shapes: the arrow; the text beam; the hand on a part a press acts on; the four resize arrows, up and down, left
// and right, and the two diagonals; the four-way arrow of the autoscroll; and the arrow with an hourglass while the
// window works
#define GUI_POINTER_ARROW 0u
#define GUI_POINTER_TEXT 1u
#define GUI_POINTER_HAND 2u
#define GUI_POINTER_RESIZE_NS 3u
#define GUI_POINTER_RESIZE_EW 4u
#define GUI_POINTER_RESIZE_NWSE 5u
#define GUI_POINTER_RESIZE_NESW 6u
#define GUI_POINTER_MOVE 7u
#define GUI_POINTER_BUSY 8u
#define GUI_POINTER_SHAPES 9u

// a sprite's largest side in cells, its shadow's cell with it; the most pixels a cell is on a side; the pixels saved
// under a sprite at that size; the layers a shape has at most; and the rasters a window flips between
#define GUI_POINTER_SIDE_MAX 20u
#define GUI_POINTER_SCALE_MAX 3u
#define GUI_POINTER_UNDER_MAX                                                                                          \
    ((GUI_POINTER_SIDE_MAX * GUI_POINTER_SCALE_MAX) * (GUI_POINTER_SIDE_MAX * GUI_POINTER_SCALE_MAX))
#define GUI_POINTER_LAYERS 2u
#define GUI_POINTER_RASTERS 2u

// A layer of a sprite: its cell table, `height` rows of `width` cells each with the end of its text after it, where
// ' ' is no cell, '#' the line and 'o' the fill; whether it is turned on its diagonal and whether it is then mirrored
// left for right; and its place in the sprite in cells
typedef struct
{
    const char *cells;
    unsigned int width;
    unsigned int height;
    int turned;
    int mirrored;
    unsigned int x;
    unsigned int y;
} GuiPointerLayer;

// A sprite: its size in cells, the cell its hot point is on, and its layers
typedef struct
{
    unsigned int width;
    unsigned int height;
    unsigned int hot_x;
    unsigned int hot_y;
    unsigned int layers;
    GuiPointerLayer layer[GUI_POINTER_LAYERS];
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

// The sprites baked at one cell size in the palette's three pointer colors. Each shape is a picture of its rectangle,
// its shadow with it, row after row of the rectangle's width. A pixel holds its color multiplied by its alpha, and the
// alpha it covers what is under it with. A draw puts this picture over the raster and walks no cell table. `made` is 0
// until the first bake
typedef struct
{
    int made;
    unsigned int cell;
    GuiColor shadow;
    GuiColor line;
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
// the pixels a cell is on a side at `scale`, the window's percent: the scale rounded to whole cells, 1 to
// GUI_POINTER_SCALE_MAX
unsigned int gui_pointer_cell(const GuiPointerReadCfg *args);
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
    unsigned int (*cell)(const GuiPointerReadCfg *args);
    GuiRect (*rect)(const GuiPointerReadCfg *args);
} GuiPointerReadNs;
EMBED_TABLE_LAYOUT(GuiPointerReadNs, cell, rect);
EMBED_TABLE_STORAGE GuiPointerReadNs gui_pointer_read EMBED_UNUSED = {
    .cell = gui_pointer_cell,
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
