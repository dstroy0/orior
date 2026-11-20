// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base.h: the frame's base, the pixels every frame starts from, composed in one pass: a map of layers over the
// lattice, each a rectangle, the later over the earlier, nothing, the editor's ground, the side's, the tabs row's
// surface, the glass sheet about the work or a pane's thinner tint, the sheet and a pane with the grain over them;
// the map read into bands of rows and each band's runs of one layer as it is positioned, each layer at each grain level an
// entry of the ink's lanes and the lattice's rest, and the frame's start one read of the lattice and one write
// through the runs. The map is positioned for a layout and positioned again only as the layout, the claim's stage or the
// palette changes (base.c)
#ifndef GUI_BASE_H
#define GUI_BASE_H
#include "embed_dispatch_layout.h"
#include "frame.h"
#include "palette.h"
#include "../glass/glass.h"

typedef struct GuiBaseRasterCfg GuiBaseRasterCfg;
typedef struct GuiBaseFrameCfg GuiBaseFrameCfg;
typedef struct GuiBaseCfg GuiBaseCfg;
typedef struct GuiBaseWidthCfg GuiBaseWidthCfg;

// the levels of the grain, 0 to its max alpha, and the sixteenths that a share sits in. Then the layers:
//   - nothing over the lattice.
//   - the ground of the editor, the ground of the side, and the surface of the tabs row.
//   - that surface at each of its sixteenths of its alpha, as the claim fades the row.
//   - the ground of the side at each of its sixteenths, as its pane slides and the panel of the terminal arrives.
//   - the ground of the editor at each of its sixteenths, as the views of the panel arrive.
//   - the glass sheet at each grain level, and a pane at each.
//   - the edge of a pane at each of its sixteenths of a pixel, at each grain level. The tint of the edge is between the
//     sheet and the pane, by its share.
//
// Then the entries in all
#define GUI_BASE_GRAIN_LEVELS 19u
#define GUI_BASE_PARTS ((unsigned int)GUI_FRAME_PARTS)
#define GUI_BASE_CLEAR 0u
#define GUI_BASE_EDITOR 1u
#define GUI_BASE_SIDE 2u
#define GUI_BASE_TABS 3u
#define GUI_BASE_TABS_PART 4u
#define GUI_BASE_SIDE_PART (GUI_BASE_TABS_PART + GUI_BASE_PARTS - 1u)
#define GUI_BASE_EDITOR_PART (GUI_BASE_SIDE_PART + GUI_BASE_PARTS - 1u)
#define GUI_BASE_GLASS (GUI_BASE_EDITOR_PART + GUI_BASE_PARTS - 1u)
#define GUI_BASE_PANE (GUI_BASE_GLASS + GUI_BASE_GRAIN_LEVELS)
#define GUI_BASE_PANE_PART (GUI_BASE_PANE + GUI_BASE_GRAIN_LEVELS)
#define GUI_BASE_ENTRIES (GUI_BASE_PANE_PART + ((GUI_BASE_PARTS - 1u) * GUI_BASE_GRAIN_LEVELS))

// An entry: the lanes of a layer over the lattice, red and blue in one word and green in another. Each lane is the share
// of the layer of its color, with half of 255 in it. Then the rest of 255 that the pixel of the lattice keeps
typedef struct
{
    unsigned int red_blue;
    unsigned int green;
    unsigned int rest;
} GuiBaseEntry;

// the most rectangles in the map, and the most bands of rows that their edges cut it into. Then the most runs that a
// band holds, each an edge of a rectangle or of the map at max
#define GUI_BASE_LAYERS 48u
#define GUI_BASE_BANDS (2u * GUI_BASE_LAYERS)
#define GUI_BASE_RUNS (2u * GUI_BASE_LAYERS)

// A rectangle of one layer positioned on the map
typedef struct
{
    GuiRect rect;
    unsigned int layer;
} GuiBaseLayer;

// A run of one layer along a row: where it starts, its pixels and the layer
typedef struct
{
    unsigned int from;
    unsigned int count;
    unsigned int layer;
} GuiBaseRun;

// A map read for drawing: how many bands; the row each starts at, the last's end after them. Where each band's runs
// start among the runs, the last's end after them; and the runs
typedef struct
{
    unsigned int bands;
    const unsigned int *band_from;
    const unsigned int *run_first;
    const GuiBaseRun *run;
} GuiBaseMap;

// The base:
//   - the size of its map, and the entries.
//   - the entries of the sheet and of a pane, in the order of the tile of the grain, one for each pixel of the tile.
//     They are lane by lane in a cell of their own, the tile of the sheet first and that of the pane after. A run of
//     either reads them straight along.
//   - the positioned rectangles, in order.
//   - the bands, each the row where it starts, with the end of the last after them. Then where the runs of each band
//     start among the runs.
//   - the frame and the colors of the layout of the map. Whether the pane of the controls had a place in that layout,
//     and whether the map has its layout.
//   - whether the code made the entries for the colors.
//   - how many layouts the map had.
typedef struct
{
    unsigned int width;
    unsigned int height;
    GuiBaseEntry entry[GUI_BASE_ENTRIES];
    unsigned int *tile_red_blue;
    unsigned int *tile_green;
    unsigned int *tile_remainder;
    GuiBaseLayer layer[GUI_BASE_LAYERS];
    unsigned int layers;
    unsigned int band_from[GUI_BASE_BANDS + 1u];
    unsigned int run_first[GUI_BASE_BANDS + 1u];
    unsigned int bands;
    GuiBaseRun run[GUI_BASE_BANDS * GUI_BASE_RUNS];
    GuiFrame frame;
    GuiColor editor_background;
    GuiColor side_background;
    GuiColor surface;
    GuiColor glass;
    int controls_steady;
    int positioned;
    int painted;
    unsigned int positioned_count;
} GuiBase;

// the base made for a window `width` by `height`, with its tile tables in a cell of their own, and no layout yet. The
// result is 1, or 0 where the call got no cell. Then its cell given back
int gui_base_init(const GuiBaseWidthCfg *args);
// the map of the base, sized to a window `width` by `height`, with its tile tables kept. It has no layout until the next
// layout
void gui_base_resize(const GuiBaseWidthCfg *args);
// the map positioned for `frame` in the colors of `palette`, where the base does not already have that layout. Where
// `controls_steady` is not 0, the pane of the controls gets a place as a pane. The result is 1 where the call made the
// layout, 0 where it was already in place
int gui_base_layout(const GuiBaseFrameCfg *args);
// the layer the map holds at x, y: the last rectangle positioned over the pixel, or nothing
unsigned int gui_base_layer_at(const GuiBaseCfg *args);
// the entries made for `palette`'s colors, where they are not already; a map read over them saves its runs
void gui_base_paint(const GuiBaseFrameCfg *args);
// every pixel of `raster` written once from `background_fill`'s pixel, or `color` where the ground is not the raster's
// size, through the runs of its row's band. And the same through `map`, a map made for a stage, which the base's own
// entries color
void gui_base_draw(const GuiBaseRasterCfg *args);
void gui_base_map_draw(const GuiBaseRasterCfg *args);
struct GuiBaseRasterCfg
{
    const GuiBase *base_read;
    GuiRaster *raster;
    const GuiRaster *background_fill;
    GuiColor color;
    const GuiBaseMap *map;
};

struct GuiBaseFrameCfg
{
    GuiBase *base;
    const GuiFrame *frame;
    const GuiPalette *palette;
    int controls_steady;
};

struct GuiBaseCfg
{
    const GuiBase *base_read;
    int x;
    int y;
};

struct GuiBaseWidthCfg
{
    GuiBase *base;
    unsigned int width;
    unsigned int height;
};

typedef struct
{
    int (*init)(const GuiBaseWidthCfg *args);
    void (*resize)(const GuiBaseWidthCfg *args);
} GuiBaseWidthNs;
EMBED_TABLE_LAYOUT(GuiBaseWidthNs, init, resize);
EMBED_TABLE_STORAGE GuiBaseWidthNs gui_base_width EMBED_UNUSED = {
    .init = gui_base_init,
    .resize = gui_base_resize,
};

typedef struct
{
    int (*layout)(const GuiBaseFrameCfg *args);
    void (*paint)(const GuiBaseFrameCfg *args);
} GuiBaseFrameNs;
EMBED_TABLE_LAYOUT(GuiBaseFrameNs, layout, paint);
EMBED_TABLE_STORAGE GuiBaseFrameNs gui_base_frame EMBED_UNUSED = {
    .layout = gui_base_layout,
    .paint = gui_base_paint,
};

typedef struct
{
    unsigned int (*layer_at)(const GuiBaseCfg *args);
} GuiBaseNs;
EMBED_TABLE_LAYOUT(GuiBaseNs, layer_at);
EMBED_TABLE_STORAGE GuiBaseNs gui_base_table EMBED_UNUSED = {
    .layer_at = gui_base_layer_at,
};

typedef struct
{
    void (*draw)(const GuiBaseRasterCfg *args);
    void (*map_draw)(const GuiBaseRasterCfg *args);
} GuiBaseRasterNs;
EMBED_TABLE_LAYOUT(GuiBaseRasterNs, draw, map_draw);
EMBED_TABLE_STORAGE GuiBaseRasterNs gui_base_raster EMBED_UNUSED = {
    .draw = gui_base_draw,
    .map_draw = gui_base_map_draw,
};

#endif
