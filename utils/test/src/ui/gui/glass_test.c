// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glass_test.c: the glass's check: the grain's tile is the same every time, its alphas within the grain's share of
// white and varying across the tile; a pane drawn over the ground is tinted, grained inside and rounded at its
// corners, and two panes lay the same grain at one pixel of the window
#include "../../../../../ui/gui/src/parts/glass.h"
#include "../cli/cli_check.h"

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    const unsigned char *const grain = gui_grain();
    cli_check(grain == gui_grain(), "the grain is made once");
    unsigned int most = 0u;
    unsigned int least = 255u;
    unsigned long long summed = 0ull;
    for (unsigned int at = 0u; at < GUI_GRAIN_TILE * GUI_GRAIN_TILE; at += 1u)
    {
        most = (grain[at] > most) ? grain[at] : most;
        least = (grain[at] < least) ? grain[at] : least;
        summed += grain[at];
    }
    cli_check(most <= (unsigned int)(GUI_GRAIN_ALPHA * 255.0 + 0.5), "no pixel of the grain is more than the grain's share of white");
    cli_check((most > least) && (summed > 0ull), "the grain varies across the tile");

    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 200u, 100u), "a raster is made");
    const GuiRect whole = {0, 0, 200, 100};
    gui_raster_filled(&raster, whole, palette->ground);
    GuiRaster plain;
    cli_check(gui_raster_made(&plain, 200u, 100u), "a raster for the tint alone is made");
    gui_raster_filled(&plain, whole, palette->ground);
    gui_raster_rounded(&plain, (GuiRect){10, 10, 100, 40}, 7u, palette->glass);
    gui_glass_drawn(&raster, (GuiRect){10, 10, 100, 40}, 7u, palette->glass);
    cli_check(raster.pixels[(30u * 200u) + 60u] != palette->ground, "the pane is tinted over the ground");
    unsigned int grained = 0u;
    for (unsigned int x = 20u; x < 100u; x += 1u)
    {
        grained += (raster.pixels[(30u * 200u) + x] != plain.pixels[(30u * 200u) + x]) ? 1u : 0u;
    }
    cli_check(grained > 0u, "the grain lies over the tint");
    cli_check(raster.pixels[(10u * 200u) + 10u] == palette->ground, "the pane's corner is rounded off to the ground");
    cli_check(raster.pixels[(5u * 200u) + 60u] == palette->ground, "the ground above the pane stands");

    // the grain is anchored to the window: a second pane over the first lays the same tile at each pixel, which a
    // pane drawn once over twice the tint shows as the pane drawn twice
    gui_raster_filled(&raster, whole, palette->ground);
    gui_raster_rounded(&raster, (GuiRect){10, 10, 100, 40}, 0u, palette->glass);
    gui_glass_drawn(&raster, (GuiRect){10, 10, 100, 40}, 0u, palette->glass);
    gui_raster_filled(&plain, whole, palette->ground);
    gui_glass_drawn(&plain, (GuiRect){10, 10, 100, 40}, 0u, palette->glass);
    gui_raster_rounded(&plain, (GuiRect){10, 10, 100, 40}, 0u, palette->glass);
    unsigned int alike = 0u;
    for (unsigned int x = 20u; x < 100u; x += 1u)
    {
        alike += (raster.pixels[(30u * 200u) + x] == plain.pixels[(30u * 200u) + x]) ? 1u : 0u;
    }
    cli_check(alike < 80u, "the grain over the tint and the tint over the grain differ, the grain lying where it lies");
    gui_raster_given(&plain);
    gui_raster_given(&raster);
    return cli_checked();
}
