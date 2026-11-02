// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glass.h: a pane of glass over the lattice, as the page lays the bar, the strip, the status bar and the controls'
// pane: the ink's tint, as thick as the bar opacity, with a fine grain over it, the grain's tile anchored to the
// window as the page's fixed attachment anchors it, and the pane's corners rounded (glass.c)
#ifndef GUI_PARTS_GLASS_H
#define GUI_PARTS_GLASS_H
typedef struct GuiGlassRasterCfg GuiGlassRasterCfg;
typedef struct GuiGlassCfg GuiGlassCfg;
#include "../palette.h"
#include "embed_dispatch_layout.h"

// the grain's tile, its side in pixels, as the page's SVG tile is; the noise's base frequency in cells a pixel and
// its octaves; the seed of the noise; and the most of the grain's alpha, 0.07 of white
#define GUI_GRAIN_TILE 180u
#define GUI_GRAIN_FREQUENCY 0.85
#define GUI_GRAIN_OCTAVES 2u
#define GUI_GRAIN_SEED 4099u
#define GUI_GRAIN_ALPHA 0.07

// the share of the glass's tint a pane that slides in over the glass lays: the bar, the strip, the status bar and
// the controls' pane each read a little thinner than the sheet about the work
#define GUI_PANE_SHARE 0.82f

// the grain: each pixel of the tile the alpha of white laid there, 0 to GUI_GRAIN_ALPHA of 255, row after row, made
// once from the seeded noise, the same every time
const unsigned char *gui_grain(void);
// the glass's tint as a pane lays it: the same ink at GUI_PANE_SHARE of the tint's alpha
GuiColor gui_glass_pane_tint(const GuiGlassCfg *args);
// the table every frame lies on: the lattice's `background_fill` raster, or `color` where none is the window's size,
// the glass's tint over it and the grain over that
GuiUnder gui_glass_table(const GuiGlassCfg *args);
// a pane drawn over `rect`, its corners rounded by `radius`: `tint` blended over what stands, then the grain
// blended over it inside the corners, the tile's pixel for each of the raster's
void gui_glass_draw(const GuiGlassRasterCfg *args);
struct GuiGlassRasterCfg
{
    GuiRaster *raster;
    GuiRect rect;
    unsigned int radius;
    GuiColor tint;
};

struct GuiGlassCfg
{
    GuiColor glass;
    const GuiRaster *background_fill;
    GuiColor color;
};

typedef struct
{
    GuiColor (*pane_tint)(const GuiGlassCfg *args);
    GuiUnder (*table)(const GuiGlassCfg *args);
} GuiGlassNs;
EMBED_TABLE_LAYOUT(GuiGlassNs, pane_tint, table);
EMBED_TABLE_STORAGE GuiGlassNs gui_glass EMBED_UNUSED = {
    .pane_tint = gui_glass_pane_tint,
    .table = gui_glass_table,
};

typedef struct
{
    void (*draw)(const GuiGlassRasterCfg *args);
} GuiGlassRasterNs;
EMBED_TABLE_LAYOUT(GuiGlassRasterNs, draw);
EMBED_TABLE_STORAGE GuiGlassRasterNs gui_glass_raster EMBED_UNUSED = {
    .draw = gui_glass_draw,
};

#endif
