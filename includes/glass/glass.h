// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glass.h: a pane of glass over the lattice, as the page positions the bar, the side_menu, the status bar and the
// controls' pane: the ink's tint, as thick as the bar opacity, with a fine grain over it, the grain's tile anchored to
// the window as the page's fixed attachment anchors it, and the pane's corners rounded (glass.c)
#ifndef GUI_GLASS_H
#define GUI_GLASS_H
#include "../raster/raster.h"
#include "embed_dispatch_layout.h"

typedef struct GuiGlassRasterCfg GuiGlassRasterCfg;
typedef struct GuiGlassCfg GuiGlassCfg;
typedef struct GuiGlassButtonCfg GuiGlassButtonCfg;

// the grain's tile, its side in pixels, as the page's SVG tile is. The noise's base frequency in cells a pixel and
// its octaves; the seed of the noise; and the max of the grain's alpha, 0.07 of white
#define GUI_GRAIN_TILE 180u
#define GUI_GRAIN_FREQUENCY 0.85
#define GUI_GRAIN_OCTAVES 2u
#define GUI_GRAIN_SEED 4099u
#define GUI_GRAIN_ALPHA 0.07

// the share of the tint of the glass that a pane positions when it slides in over the glass. The bar, the side_menu,
// the status bar and the pane of the controls each read a little thinner than the sheet about the work
#define GUI_PANE_SHARE 0.82f

// the grain: each pixel of the tile is the alpha of white there, 0 to GUI_GRAIN_ALPHA of 255, row after row. The call
// makes it once from the seeded noise, the same every time
const unsigned char *gui_grain(void);
// the glass's tint as a pane positions it: the same ink at GUI_PANE_SHARE of the tint's alpha
GuiColor gui_glass_pane_tint(const GuiGlassCfg *args);
// the table that every frame lies on: the `background_fill` raster of the lattice, or `color` where no raster has the
// size of the window. The tint of the glass goes over it, and the grain over that
GuiUnder gui_glass_table(const GuiGlassCfg *args);
// a pane drawn over `rect`, with its corners rounded by `radius`. The tint `tint` goes over what is there. Then the
// grain goes over it inside the corners, one pixel of the tile for each pixel of the raster
void gui_glass_draw(const GuiGlassRasterCfg *args);
// A button as a pane of glass: `rect` rounded by `radius`, with `tint` over what is there and the grain over the tint.
// Its lip is a line of `lip`, `lip_width` pixels wide, round it inside its edge. A button at rest takes the button
// glass of the palette and its line. A button pointed, pressed or on takes the colors that its part names
void gui_glass_button_draw(const GuiGlassButtonCfg *args);
struct GuiGlassButtonCfg
{
    GuiRaster *raster;
    GuiRect rect;
    unsigned int radius;
    GuiColor tint;
    GuiColor grain;
    GuiColor lip;
    int lip_width;
};
struct GuiGlassRasterCfg
{
    GuiRaster *raster;
    GuiRect rect;
    unsigned int radius;
    GuiColor tint;
    GuiColor grain;
};

struct GuiGlassCfg
{
    GuiColor glass;
    const GuiRaster *background_fill;
    GuiColor color;
    GuiColor grain;
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
    void (*draw)(const GuiGlassButtonCfg *args);
} GuiGlassButtonNs;
EMBED_TABLE_LAYOUT(GuiGlassButtonNs, draw);
EMBED_TABLE_STORAGE GuiGlassButtonNs gui_glass_button EMBED_UNUSED = {
    .draw = gui_glass_button_draw,
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
