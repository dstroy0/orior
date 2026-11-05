// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glass_test.c: the glass's check: the grain's tile is the same every time, its alphas within the grain's share of
// white and varying across the tile; a pane drawn over the ground is tinted, grained inside and rounded at its
// corners, and two panes lay the same grain at one pixel of the window
#include "../../../../../ui/cli/src/view/parts/glass.h"
#include "../cli/cli_check.h"

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    const unsigned char *const grain = gui_grain();
    cli_check(grain == gui_grain(), "the grain is made once");
    unsigned int most = 0u;
    unsigned int min = 255u;
    unsigned long long summed = 0ull;
    for (unsigned int at = 0u; at < GUI_GRAIN_TILE * GUI_GRAIN_TILE; at += 1u)
    {
        most = (grain[at] > most) ? grain[at] : most;
        min = (grain[at] < min) ? grain[at] : min;
        summed += grain[at];
    }
    cli_check(most <= (unsigned int)(GUI_GRAIN_ALPHA * 255.0 + 0.5), "no pixel of the grain is more than the grain's share of white");
    cli_check((most > min) && (summed > 0ull), "the grain varies across the tile");

    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 200u, .height = 100u), "a raster is made");
    const GuiRect whole = {0, 0, 200, 100};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = palette->background_fill);
    GuiRaster plain;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &plain, .width = 200u, .height = 100u), "a raster for the tint alone is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &plain, .rect = whole, .color = palette->background_fill);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = &plain, .rect = (GuiRect){10, 10, 100, 40}, .radius = 7u, .color = palette->glass);
    EMBED_CALL(gui_glass_raster.draw, GuiGlassRasterCfg, .raster = &raster, .rect = (GuiRect){10, 10, 100, 40}, .radius = 7u, .tint = palette->glass);
    cli_check(raster.pixels[(30u * 200u) + 60u] != palette->background_fill, "the pane is tinted over the ground");
    unsigned int grained = 0u;
    for (unsigned int x = 20u; x < 100u; x += 1u)
    {
        grained += (raster.pixels[(30u * 200u) + x] != plain.pixels[(30u * 200u) + x]) ? 1u : 0u;
    }
    cli_check(grained > 0u, "the grain lies over the tint");
    cli_check(raster.pixels[(10u * 200u) + 10u] == palette->background_fill, "the pane's corner is rounded off to the ground");
    cli_check(raster.pixels[(5u * 200u) + 60u] == palette->background_fill, "the ground above the pane stands");

    // the grain is anchored to the window: a second pane over the first lays the same tile at each pixel, which a
    // pane drawn once over twice the tint shows as the pane drawn twice
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = whole, .color = palette->background_fill);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){10, 10, 100, 40}, .radius = 0u, .color = palette->glass);
    EMBED_CALL(gui_glass_raster.draw, GuiGlassRasterCfg, .raster = &raster, .rect = (GuiRect){10, 10, 100, 40}, .radius = 0u, .tint = palette->glass);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &plain, .rect = whole, .color = palette->background_fill);
    EMBED_CALL(gui_glass_raster.draw, GuiGlassRasterCfg, .raster = &plain, .rect = (GuiRect){10, 10, 100, 40}, .radius = 0u, .tint = palette->glass);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = &plain, .rect = (GuiRect){10, 10, 100, 40}, .radius = 0u, .color = palette->glass);
    unsigned int alike = 0u;
    for (unsigned int x = 20u; x < 100u; x += 1u)
    {
        alike += (raster.pixels[(30u * 200u) + x] == plain.pixels[(30u * 200u) + x]) ? 1u : 0u;
    }
    cli_check(alike < 80u, "the grain over the tint and the tint over the grain differ, the grain lying where it lies");
    g_cli_cells->persistent_buf_release(plain.pixels, &g_gui_raster_cells);
    plain.pixels = NULL;
    plain.width = 0u;
    plain.height = 0u;
    g_cli_cells->persistent_buf_release(raster.pixels, &g_gui_raster_cells);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    return cli_on_check();
}
