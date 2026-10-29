// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glass.c: a pane of glass over the lattice. The grain is the page's fractal noise made in C: value noise on a grid
// of GUI_GRAIN_FREQUENCY cells a pixel, each corner's value drawn from the seed and its place, the tile's edges
// meeting as the page's stitched tile meets its own, the pixel's value blended from its cell's four corners on the
// ease curve; GUI_GRAIN_OCTAVES octaves, each twice the cells and half the weight of the one before, summed and
// scaled to 0 to 1, give the pixel's share of the grain's alpha
#include "glass.h"

#include <math.h>

static unsigned char s_grain[GUI_GRAIN_TILE * GUI_GRAIN_TILE];
static int s_grain_made;

// the noise's value at the grid corner `row`, `column` of the octave `octave`, 0 to 1, drawn from the seed, the
// corner and the octave mixed, the same every time
static double grain_corner(unsigned int octave, unsigned int row, unsigned int column)
{
    // the row, the column and the octave mixed into the seed, each by an odd constant
    unsigned int state = GUI_GRAIN_SEED ^ (row * 0x9e3779b1u) ^ (column * 0x85ebca77u) ^ (octave * 0xc2b2ae3du);
    state += 0x6d2b79f5u;
    unsigned int mixed = state;
    mixed = (mixed ^ (mixed >> 15)) * (mixed | 1u);
    mixed ^= mixed + (mixed ^ (mixed >> 7)) * (mixed | 61u);
    return (double)(mixed ^ (mixed >> 14)) / 4294967296.0;
}

static double grain_ease(double part)
{
    return part * part * (3.0 - 2.0 * part);
}

// the noise of `octave` at the tile's pixel x, y, 0 to 1: the pixel's cell on a grid of `cells` across the tile,
// its four corners blended by where the pixel stands in the cell, the grid wrapping at the tile's edges
static double grain_noise(unsigned int octave, unsigned int cells, unsigned int x, unsigned int y)
{
    const double across = ((double)x + 0.5) * (double)cells / (double)GUI_GRAIN_TILE;
    const double down = ((double)y + 0.5) * (double)cells / (double)GUI_GRAIN_TILE;
    // a cell's corner is a whole count below the cells
    const unsigned int column = (unsigned int)floor(across);
    const unsigned int row = (unsigned int)floor(down);
    const double part_x = grain_ease(across - (double)column);
    const double part_y = grain_ease(down - (double)row);
    const unsigned int next_column = (column + 1u) % cells;
    const unsigned int next_row = (row + 1u) % cells;
    const double top = grain_corner(octave, row, column) + (grain_corner(octave, row, next_column) - grain_corner(octave, row, column)) * part_x;
    const double bottom = grain_corner(octave, next_row, column) +
                          (grain_corner(octave, next_row, next_column) - grain_corner(octave, next_row, column)) * part_x;
    return top + (bottom - top) * part_y;
}

const unsigned char *gui_grain(void)
{
    if (s_grain_made)
    {
        return s_grain;
    }
    // the base octave's cells across the tile, a whole count, and the octaves' weights summed
    const unsigned int base = (unsigned int)floor((double)GUI_GRAIN_TILE * GUI_GRAIN_FREQUENCY);
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
                summed += grain_noise(octave, base << octave, x, y) / (double)(1u << octave);
            }
            // a share from 0 to 1 of the grain's alpha, rounded, is a byte
            s_grain[(y * GUI_GRAIN_TILE) + x] = (unsigned char)((summed / weights) * GUI_GRAIN_ALPHA * 255.0 + 0.5);
        }
    }
    s_grain_made = 1;
    return s_grain;
}

void gui_glass_drawn(GuiRaster *raster, GuiRect rect, unsigned int radius, GuiColor tint)
{
    gui_raster_rounded(raster, rect, radius, tint);
    gui_raster_grained(raster, rect, radius, gui_grain(), GUI_GRAIN_TILE);
}
