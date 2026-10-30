// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base.h: the frame's base, the pixels every frame starts from, composed in one pass: a map of one byte a pixel
// says which layer lies over the lattice there, nothing, the editor's ground, the side's, the tabs row's surface,
// the glass sheet about the work or a pane's thinner tint, the sheet and a pane with the grain over them; each
// layer at each grain level an entry of the ink's lanes and the lattice's rest, the frame's start one read of the
// lattice and one write through the map, run by run of one layer. The map is made for a layout and made again only
// as the layout, the claim's stage or the palette changes (base.c)
#ifndef GUI_BASE_H
#define GUI_BASE_H
#include "frame.h"
#include "palette.h"
#include "parts/glass.h"

// the grain's levels, 0 to its most alpha; the layers: nothing over the lattice, the editor's ground, the side's,
// the tabs row's surface, the glass sheet at each grain level and a pane at each; and the entries in all
#define GUI_BASE_GRAIN_LEVELS ((unsigned int)((GUI_GRAIN_ALPHA * 255.0) + 0.5) + 1u)
#define GUI_BASE_CLEAR 0u
#define GUI_BASE_EDITOR 1u
#define GUI_BASE_SIDE 2u
#define GUI_BASE_TABS 3u
#define GUI_BASE_GLASS 4u
#define GUI_BASE_PANE (GUI_BASE_GLASS + GUI_BASE_GRAIN_LEVELS)
#define GUI_BASE_ENTRIES (GUI_BASE_PANE + GUI_BASE_GRAIN_LEVELS)

// An entry: a layer's lanes over the lattice, red and blue in one word and green in another, each lane the layer's
// share of its color with half of 255 in it, and the rest of 255 the lattice's pixel keeps
typedef struct
{
    unsigned int red_blue;
    unsigned int green;
    unsigned int rest;
} GuiBaseEntry;

// the most runs of one layer a row of the map is recorded in; a row of more is read pixel by pixel
#define GUI_BASE_RUNS 32u

// A run of one layer along a row: where it starts, its pixels and the layer
typedef struct
{
    unsigned int from;
    unsigned int count;
    unsigned int layer;
} GuiBaseRun;

// The base: the size of its map, a byte a pixel in a cell of the cellblock; the entries; the sheet's and a pane's
// entries laid out as the grain's tile is, one for each of the tile's pixels, lane by lane in a cell of their own,
// the sheet's tile first and the pane's after, which a run of either reads straight along; each row's runs,
// GUI_BASE_RUNS of room a row, and how many each row has, read from the map as it is laid; the frame and the colors
// the map was laid for, whether it was laid with the controls' pane standing, and whether it is laid; and how many
// times it has been laid
typedef struct
{
    unsigned int width;
    unsigned int height;
    unsigned char *map;
    GuiBaseEntry entry[GUI_BASE_ENTRIES];
    unsigned int *tile_red_blue;
    unsigned int *tile_green;
    unsigned int *tile_rest;
    GuiBaseRun *run;
    unsigned int *runs;
    GuiFrame frame;
    GuiColor editor_ground;
    GuiColor side_ground;
    GuiColor surface;
    GuiColor glass;
    int controls_steady;
    int laid;
    unsigned int laid_count;
} GuiBase;

// the base made for a window `width` by `height`, its map in a cell of its own, laid for nothing yet: 1, or 0 where
// the cell could not be taken; and its cell given back
int gui_base_made(GuiBase *base, unsigned int width, unsigned int height);
void gui_base_given(GuiBase *base);
// the map laid for `frame` in `palette`'s colors, the controls' pane laid as a pane where `controls_steady` is set,
// where the base is not already laid for them: 1 where it was laid, 0 where it stood
int gui_base_laid(GuiBase *base, const GuiFrame *frame, const GuiPalette *palette, int controls_steady);
// every pixel of `raster` written once from `ground`'s pixel, or `color` where the ground is not the raster's
// size, through the map's entry there
void gui_base_drawn(const GuiBase *base, GuiRaster *raster, const GuiRaster *ground, GuiColor color);
#endif
