// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base_test.c: the base's check: the map laid for a frame and laid again only as the frame changes, the frame's
// start drawn through it in one pass, the editor's and the tabs row's grounds the fill's own blend, the sheet and a
// pane the ink with the grain, the lines' ring and the map's column the lattice, and a hole the sheet
#include "../../../../../ui/gui/src/base.h"
#include "../cli/cli_check.h"

// the pixel of `raster` at x, y
static GuiColor pixel(const GuiRaster *raster, int x, int y)
{
    return raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
}

// `color` over `under` by the color's alpha, each channel rounded, as a check reads a blend
static GuiColor over(GuiColor under, GuiColor color)
{
    const unsigned int share = GUI_ALPHA_OF(color);
    const unsigned int rest = 255u - share;
    return GUI_RGB(((GUI_RED_OF(color) * share) + (GUI_RED_OF(under) * rest) + 127u) / 255u,
                   ((GUI_GREEN_OF(color) * share) + (GUI_GREEN_OF(under) * rest) + 127u) / 255u,
                   ((GUI_BLUE_OF(color) * share) + (GUI_BLUE_OF(under) * rest) + 127u) / 255u);
}

// whether two colors stand within a level of each other on every channel
static int near(GuiColor one, GuiColor other)
{
    const int red = (int)GUI_RED_OF(one) - (int)GUI_RED_OF(other);
    const int green = (int)GUI_GREEN_OF(one) - (int)GUI_GREEN_OF(other);
    const int blue = (int)GUI_BLUE_OF(one) - (int)GUI_BLUE_OF(other);
    return (red >= -1) && (red <= 1) && (green >= -1) && (green <= 1) && (blue >= -1) && (blue <= 1);
}

// the grain's alpha at x, y as the window lays it
static unsigned int grain_at(int x, int y)
{
    return gui_grain()[(((unsigned int)y % GUI_GRAIN_TILE) * GUI_GRAIN_TILE) + ((unsigned int)x % GUI_GRAIN_TILE)];
}

// the sheet's pixel over `ground` at x, y: the glass's tint with the grain's white over it
static GuiColor sheet_at(const GuiPalette *palette, GuiColor ground, int x, int y)
{
    return over(over(ground, palette->glass), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(x, y)));
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 800u, .height = 600u, .scale = 100);
    GuiBase base;
    cli_check(EMBED_CALL(gui_base_width.init, GuiBaseWidthCfg, .base = &base, .width = 800u, .height = 600u), "a base for a window of eight hundred by six hundred is made");
    cli_check(!base.laid && (base.laid_count == 0u), "a base made is laid for nothing yet");
    cli_check(EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0), "the map is laid for the frame");
    cli_check(!EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0) && (base.laid_count == 1u), "laid again for the same frame it stands");
    cli_check(GUI_BASE_GRAIN_LEVELS == (unsigned int)((GUI_GRAIN_ALPHA * 255.0) + 0.5) + 1u, "the grain's levels run to its most alpha");

    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 800u, .height = 600u), "a raster for the frame is made");
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .ground = NULL, .color = palette->ground);
    const int editor_x = frame.editor.x + (frame.editor.width / 2);
    const int editor_y = frame.editor.y + (frame.editor.height / 2);
    cli_check(pixel(&raster, editor_x, editor_y) == EMBED_CALL(cli_raster.color_over, GuiRasterCfg, .ground_color = palette->ground, .color = palette->editor_ground),
              "a pixel of the editor is its ground over the lattice, the fill's own blend");
    cli_check(pixel(&raster, frame.tabs.x + 10, frame.tabs.y + 10) == palette->surface, "a pixel of the tabs row is the row's surface");
    cli_check(pixel(&raster, frame.map.x + 10, frame.map.y + 100) == palette->ground, "a pixel of the map's column is the lattice");
    cli_check(pixel(&raster, frame.work.x, frame.work.y + 100) == palette->ground, "a pixel of the lines' ring is the lattice");
    const int gap_x = frame.gap.x + (frame.gap.width / 2);
    const int gap_y = frame.gap.y + (frame.gap.height / 2);
    cli_check(near(pixel(&raster, gap_x, gap_y), sheet_at(palette, palette->ground, gap_x, gap_y)),
              "a pixel of the frame's gap is the sheet, the glass with the grain");
    const int bar_x = frame.bar.x + (frame.bar.width / 2);
    const int bar_y = frame.bar.y + (frame.bar.height / 2);
    const GuiColor pane = EMBED_CALL(gui_glass.pane_tint, GuiGlassCfg, .glass = palette->glass);
    cli_check(GUI_ALPHA_OF(pane) < GUI_ALPHA_OF(palette->glass), "a pane's tint is thinner than the glass's");
    cli_check(near(pixel(&raster, bar_x, bar_y), over(over(palette->ground, pane), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(bar_x, bar_y)))),
              "a pixel of the bar's pane is the pane's tint with the grain");
    cli_check(pixel(&raster, bar_x, bar_y) != pixel(&raster, gap_x, gap_y) || (grain_at(bar_x, bar_y) != grain_at(gap_x, gap_y)),
              "the pane reads apart from the sheet");

    // the lattice as a raster of the window's size is read pixel for pixel
    GuiRaster ground;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &ground, .width = 800u, .height = 600u), "a lattice raster is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &ground, .rect = (GuiRect){0, 0, 800, 600}, .color = GUI_RGB(0x20u, 0x40u, 0x60u));
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .ground = &ground, .color = palette->ground);
    cli_check(pixel(&raster, editor_x, editor_y) == EMBED_CALL(cli_raster.color_over, GuiRasterCfg, .ground_color = GUI_RGB(0x20u, 0x40u, 0x60u), .color = palette->editor_ground),
              "with a lattice raster the editor's ground lies over the lattice's pixel");
    cli_check(pixel(&raster, frame.map.x + 10, frame.map.y + 100) == GUI_RGB(0x20u, 0x40u, 0x60u), "and the map's column is the lattice's pixel");
    GuiRaster small;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &small, .width = 8u, .height = 4u), "a raster of another size is made");
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &small, .ground = NULL, .color = palette->ground);
    cli_check(small.pixels[0] == 0u, "a raster not the base's size is left alone");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &small);

    // the claim holds with the bar in: its slat makes a hole in the work, the sheet, with the slat's pane in it
    GuiClaimNumbers numbers = frame.claim;
    numbers.claim = GUI_WHOLE;
    numbers.reveal_top = GUI_WHOLE;
    numbers.grown_top = GUI_WHOLE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    GuiRect holes[4];
    cli_check(EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = &frame, .holes = holes) >= 1u, "the bar's slat makes a hole in the work");
    cli_check(EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0) && (base.laid_count == 2u), "the map is laid again for the claim's frame");
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .ground = NULL, .color = palette->ground);
    const int hole_x = holes[0].x + (holes[0].width / 2);
    const int below_slat = frame.bar_slat.y + frame.bar_slat.height + 4;
    cli_check(near(pixel(&raster, hole_x, below_slat), sheet_at(palette, palette->ground, hole_x, below_slat)),
              "a pixel of the hole under the slat is the sheet");
    const int slat_y = frame.bar_slat.y + (frame.bar_slat.height / 2);
    cli_check(near(pixel(&raster, hole_x, slat_y), over(over(palette->ground, pane), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(hole_x, slat_y)))),
              "a pixel of the slat in the hole is the pane");
    cli_check(!EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0), "laid again for the claim's frame it stands");
    // the bar half revealed stands fourteen and a half pixels above the window: its foot falls in the middle of a
    // row, which takes the ink at an alpha halfway between the pane's and the sheet's
    numbers.reveal_top = GUI_WHOLE / 2;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.bar_edges.bottom == (26 * GUI_WHOLE) + (GUI_WHOLE / 2)), "the bar's foot falls in the middle of the row of twenty-six");
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .ground = NULL, .color = palette->ground);
    const GuiColor between = (pane & 0x00ffffffu) | (((GUI_ALPHA_OF(palette->glass) + GUI_ALPHA_OF(pane) + 1u) / 2u) << 24u);
    cli_check(near(pixel(&raster, hole_x, 26), over(over(palette->ground, between), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(hole_x, 26)))),
              "the row the foot falls in is the ink at the alpha between");
    cli_check(near(pixel(&raster, hole_x, 25), over(over(palette->ground, pane), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(hole_x, 25)))),
              "the row above it is the pane whole");
    cli_check(near(pixel(&raster, hole_x, 27), sheet_at(palette, palette->ground, hole_x, 27)), "and the row below it the sheet");
    numbers.reveal_top = GUI_WHOLE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    cli_check(EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 1) && (base.laid_count == 5u), "the controls' pane standing lays the map again");

    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &ground);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(gui_base_width.free, GuiBaseWidthCfg, .base = &base);
    cli_check((base.tile_red_blue == NULL) && !base.laid, "a base given back holds no tables");
    return cli_on_check();
}
