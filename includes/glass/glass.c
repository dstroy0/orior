// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glass.c: a pane of glass over the lattice. The grain is the page's fractal noise made in C: value noise on a grid
// of GUI_GRAIN_FREQUENCY cells a pixel, each corner's value drawn from the seed and its place, the tile's edges
// meeting as the page's stitched tile meets its own, the pixel's value blended from its cell's four corners on the
// ease curve; GUI_GRAIN_OCTAVES octaves, each twice the cells and half the weight of the one before, summed and
// scaled to 0 to 1, give the pixel's share of the grain's alpha
#include "glass.h"
#include "../arith/arith.h"

static unsigned char s_grain[GUI_GRAIN_TILE * GUI_GRAIN_TILE];
static int s_grain_created;

typedef struct
{
    unsigned int octave;
    unsigned int row;
    unsigned int column;
    double part;
    unsigned int cells;
    unsigned int x;
    unsigned int y;
} GlassCtx;
// the noise's value at the grid corner `row`, `column` of the octave `octave`, 0 to 1, drawn from the seed, the
// corner and the octave mixed, the same every time
EMBED_INLINE double grain_corner(const GlassCtx *args)
{
    // the row, the column and the octave mixed into the seed, each by an odd constant
    unsigned int state =
        GUI_GRAIN_SEED ^ (args->row * 0x9e3779b1u) ^ (args->column * 0x85ebca77u) ^ (args->octave * 0xc2b2ae3du);
    state += 0x6d2b79f5u;
    unsigned int mixed = state;
    mixed = (mixed ^ (mixed >> 15)) * (mixed | 1u);
    mixed ^= mixed + (mixed ^ (mixed >> 7)) * (mixed | 61u);
    return (double)(mixed ^ (mixed >> 14)) / 4294967296.0;
}

EMBED_INLINE double grain_ease(const GlassCtx *args)
{
    return args->part * args->part * (3.0 - 2.0 * args->part);
}

// the noise of `octave` at the tile's pixel x, y, 0 to 1: the pixel's cell on a grid of `cells` across the tile,
// its four corners blended by where the pixel is in the cell, the grid wrapping at the tile's edges
EMBED_INLINE double grain_noise(const GlassCtx *args)
{
    const double across = ((double)args->x + 0.5) * (double)args->cells / (double)GUI_GRAIN_TILE;
    const double down = ((double)args->y + 0.5) * (double)args->cells / (double)GUI_GRAIN_TILE;
    // a cell's corner is a whole count below the cells
    const unsigned int column = (unsigned int)arith_floor(across);
    const unsigned int row = (unsigned int)arith_floor(down);
    const double part_x = EMBED_CALL(grain_ease, GlassCtx, .part = across - (double)column);
    const double part_y = EMBED_CALL(grain_ease, GlassCtx, .part = down - (double)row);
    const unsigned int next_column = (column + 1u) % args->cells;
    const unsigned int next_row = (row + 1u) % args->cells;
    const double top = EMBED_CALL(grain_corner, GlassCtx, .octave = args->octave, .row = row, .column = column) +
                       (EMBED_CALL(grain_corner, GlassCtx, .octave = args->octave, .row = row, .column = next_column) -
                        EMBED_CALL(grain_corner, GlassCtx, .octave = args->octave, .row = row, .column = column)) *
                           part_x;
    const double bottom =
        EMBED_CALL(grain_corner, GlassCtx, .octave = args->octave, .row = next_row, .column = column) +
        (EMBED_CALL(grain_corner, GlassCtx, .octave = args->octave, .row = next_row, .column = next_column) -
         EMBED_CALL(grain_corner, GlassCtx, .octave = args->octave, .row = next_row, .column = column)) *
            part_x;
    return top + (bottom - top) * part_y;
}

const unsigned char *gui_grain(void)
{
    if (s_grain_created)
    {
        return s_grain;
    }
    // the base octave's cells across the tile, a whole count, and the octaves' weights summed
    const unsigned int base = (unsigned int)arith_floor((double)GUI_GRAIN_TILE * GUI_GRAIN_FREQUENCY);
    double weights = 0.0;
    for (unsigned int octave = 0u; octave < GUI_GRAIN_OCTAVES; octave += 1u)
    {
        weights += 1.0 / (double)(1u << octave);
    }
    for (unsigned int y = 0u; y < GUI_GRAIN_TILE; y += 1u)
    {
        for (unsigned int x = 0u; x < GUI_GRAIN_TILE; x += 1u)
        {
            double summed = 0.0;
            for (unsigned int octave = 0u; octave < GUI_GRAIN_OCTAVES; octave += 1u)
            {
                summed += EMBED_CALL(grain_noise, GlassCtx, .octave = octave, .cells = base << octave, .x = x, .y = y) /
                          (double)(1u << octave);
            }
            // a share from 0 to 1 of the grain's alpha, rounded, is a byte
            s_grain[(y * GUI_GRAIN_TILE) + x] = (unsigned char)((summed / weights) * GUI_GRAIN_ALPHA * 255.0 + 0.5);
        }
    }
    s_grain_created = 1;
    return s_grain;
}

void gui_glass_draw(const GuiGlassRasterCfg *args)
{
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster, .rect = args->rect,
               .radius = args->radius, .color = args->tint);
    EMBED_CALL(gui_raster_rect_radius.grain, GuiRasterRectRadiusCfg, .raster = args->raster, .rect = args->rect,
               .radius = args->radius, .alpha = gui_grain(), .tile = GUI_GRAIN_TILE, .color = args->grain);
}

void gui_glass_button_draw(const GuiGlassButtonCfg *args)
{
    EMBED_CALL(gui_glass_raster.draw, GuiGlassRasterCfg, .raster = args->raster, .rect = args->rect,
               .radius = args->radius, .tint = args->tint, .grain = args->grain);
    if (args->lip_width <= 0)
    {
        return;
    }
    if (args->radius != 0u)
    {
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster, .rect = args->rect,
                   .radius = args->radius, .stroke = args->lip_width, .color = args->lip);
        return;
    }
    // a lip with square corners: a line along each edge
    const GuiRect rect = args->rect;
    const GuiRect edges[4] = {{rect.x, rect.y, rect.width, args->lip_width},
                              {rect.x, rect.y + rect.height - args->lip_width, rect.width, args->lip_width},
                              {rect.x, rect.y, args->lip_width, rect.height},
                              {rect.x + rect.width - args->lip_width, rect.y, args->lip_width, rect.height}};
    for (unsigned int edge = 0u; edge < 4u; edge += 1u)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = edges[edge],
                   .color = args->lip);
    }
}

GuiColor gui_glass_pane_tint(const GuiGlassCfg *args)
{
    // an alpha by a share from 0 to 1, rounded, is a byte
    const GuiColor alpha = (GuiColor)(((float)GUI_ALPHA_OF(args->glass) * GUI_PANE_SHARE) + 0.5f);
    return (args->glass & 0x00ffffffu) | (alpha << 24u);
}

GuiUnder gui_glass_table(const GuiGlassCfg *args)
{
    const GuiUnder table = {args->background_fill, args->glass, gui_grain(), GUI_GRAIN_TILE, args->color, args->grain};
    return table;
}
