// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base_test.c: the base's check: the map positioned for a frame and positioned again only as the frame changes, the frame's
// start drawn through it in one pass, the editor's and the tabs row's grounds the fill's own blend, the sheet and a
// pane the ink with the grain, the lines' ring and the map's column the lattice, and a hole the sheet
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/base.h"
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

// whether two colors are within a level of each other on every channel
static int adjacent(GuiColor one, GuiColor other)
{
    const int red = (int)GUI_RED_OF(one) - (int)GUI_RED_OF(other);
    const int green = (int)GUI_GREEN_OF(one) - (int)GUI_GREEN_OF(other);
    const int blue = (int)GUI_BLUE_OF(one) - (int)GUI_BLUE_OF(other);
    return (red >= -1) && (red <= 1) && (green >= -1) && (green <= 1) && (blue >= -1) && (blue <= 1);
}

// the grain's alpha at x, y as the window positions it
static unsigned int grain_at(int x, int y)
{
    return gui_grain()[(((unsigned int)y % GUI_GRAIN_TILE) * GUI_GRAIN_TILE) + ((unsigned int)x % GUI_GRAIN_TILE)];
}

// the sheet's pixel over `background_fill` at x, y: the glass's tint with the grain's white over it
static GuiColor sheet_at(const GuiPalette *palette, GuiColor background_fill, int x, int y)
{
    return over(over(background_fill, palette->glass), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(x, y)));
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 800u, .height = 600u, .scale = 100);
    GuiBase base;
    cli_check(EMBED_CALL(gui_base_width.init, GuiBaseWidthCfg, .base = &base, .width = 800u, .height = 600u), "a base for a window of eight hundred by six hundred is made");
    cli_check(!base.positioned && (base.positioned_count == 0u), "a base made is positioned for nothing yet");
    cli_check(EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0), "the map is positioned for the frame");
    cli_check(!EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0) && (base.positioned_count == 1u), "positioned again for the same frame it is");
    cli_check(GUI_BASE_GRAIN_LEVELS == (unsigned int)((GUI_GRAIN_ALPHA * 255.0) + 0.5) + 1u, "the grain's levels run to its max alpha");

    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 800u, .height = 600u), "a raster for the frame is made");
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    const int editor_x = frame.editor.x + (frame.editor.width / 2);
    const int editor_y = frame.editor.y + (frame.editor.height / 2);
    cli_check(pixel(&raster, editor_x, editor_y) == EMBED_CALL(gui_color.over, GuiRasterCfg, .background_color = palette->background_fill, .color = palette->editor_background),
              "a pixel of the editor is its ground over the lattice, the fill's own blend");
    cli_check(pixel(&raster, frame.tabs.x + 10, frame.tabs.y + 10) == palette->surface, "a pixel of the tabs row is the row's surface");
    cli_check(pixel(&raster, frame.map.x + 10, frame.map.y + 100) == palette->background_fill, "a pixel of the map's column is the lattice");
    cli_check(pixel(&raster, frame.work.x, frame.work.y + 100) == palette->background_fill, "a pixel of the lines' ring is the lattice");
    const int gap_x = frame.gap.x + (frame.gap.width / 2);
    const int gap_y = frame.gap.y + (frame.gap.height / 2);
    cli_check(adjacent(pixel(&raster, gap_x, gap_y), sheet_at(palette, palette->background_fill, gap_x, gap_y)),
              "a pixel of the frame's gap is the sheet, the glass with the grain");
    const int top_menu_x = frame.top_menu.x + (frame.top_menu.width / 2);
    const int top_menu_y = frame.top_menu.y + (frame.top_menu.height / 2);
    const GuiColor pane = EMBED_CALL(gui_glass.pane_tint, GuiGlassCfg, .glass = palette->glass);
    cli_check(GUI_ALPHA_OF(pane) < GUI_ALPHA_OF(palette->glass), "a pane's tint is thinner than the glass's");
    cli_check(adjacent(pixel(&raster, top_menu_x, top_menu_y), over(over(palette->background_fill, pane), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(top_menu_x, top_menu_y)))),
              "a pixel of the bar's pane is the pane's tint with the grain");
    cli_check(pixel(&raster, top_menu_x, top_menu_y) != pixel(&raster, gap_x, gap_y) || (grain_at(top_menu_x, top_menu_y) != grain_at(gap_x, gap_y)),
              "the pane reads apart from the sheet");

    // the lattice as a raster of the window's size is read pixel for pixel
    GuiRaster background_fill;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &background_fill, .width = 800u, .height = 600u, .part = GUI_BLOCK_FRAMES, .segment = 1u), "a lattice raster is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &background_fill, .rect = (GuiRect){0, 0, 800, 600}, .color = GUI_RGB(0x20u, 0x40u, 0x60u));
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = &background_fill, .color = palette->background_fill);
    cli_check(pixel(&raster, editor_x, editor_y) == EMBED_CALL(gui_color.over, GuiRasterCfg, .background_color = GUI_RGB(0x20u, 0x40u, 0x60u), .color = palette->editor_background),
              "with a lattice raster the editor's ground lies over the lattice's pixel");
    cli_check(pixel(&raster, frame.map.x + 10, frame.map.y + 100) == GUI_RGB(0x20u, 0x40u, 0x60u), "and the map's column is the lattice's pixel");
    GuiRaster small;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &small, .width = 8u, .height = 4u, .part = GUI_BLOCK_BACKGROUND), "a raster of another size is made");
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &small, .background_fill = NULL, .color = palette->background_fill);
    cli_check(small.pixels[0] == 0u, "a raster not the base's size is left alone");
    small.pixels = NULL;
    small.width = 0u;
    small.height = 0u;

    // the claim holds with the bar in: its slat makes a hole in the work, the sheet, with the slat's pane in it
    GuiAutohideNumbers numbers = frame.claim;
    numbers.claim = GUI_FIXED_ONE;
    numbers.reveal_top = GUI_FIXED_ONE;
    numbers.grown_top = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    GuiRect holes[4];
    cli_check(EMBED_CALL(gui_frame_holes_table.holes, GuiFrameHolesCfg, .frame_read = &frame, .holes = holes) >= 1u, "the bar's slat makes a hole in the work");
    cli_check(EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0) && (base.positioned_count == 2u), "the map is positioned again for the claim's frame");
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    const int hole_x = holes[0].x + (holes[0].width / 2);
    const int below_band = frame.top_menu_band.y + frame.top_menu_band.height + 4;
    cli_check(adjacent(pixel(&raster, hole_x, below_band), sheet_at(palette, palette->background_fill, hole_x, below_band)),
              "a pixel of the hole under the slat is the sheet");
    const int band_y = frame.top_menu_band.y + (frame.top_menu_band.height / 2);
    cli_check(adjacent(pixel(&raster, hole_x, band_y), over(over(palette->background_fill, pane), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(hole_x, band_y)))),
              "a pixel of the slat in the hole is the pane");
    cli_check(!EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0), "positioned again for the claim's frame it is");
    // the bar a quarter revealed is a quarter of the gap down less three quarters of its height, half a pixel
    // past a row's edge: its foot falls in the middle of a row, which takes the ink at an alpha halfway between the
    // pane's and the sheet's
    numbers.reveal_top = GUI_FIXED_ONE / 4;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    const int foot_row = (GUI_FRAME_GAP / 4) + GUI_TOP_MENU_HEIGHT - (((3 * GUI_TOP_MENU_HEIGHT) + 2) / 4);
    cli_check((frame.bar_edges.bottom == (foot_row * GUI_FIXED_ONE) + (GUI_FIXED_ONE / 2)), "the bar's foot falls in the middle of a row");
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    const GuiColor between = (pane & 0x00ffffffu) | (((GUI_ALPHA_OF(palette->glass) + GUI_ALPHA_OF(pane) + 1u) / 2u) << 24u);
    cli_check(adjacent(pixel(&raster, hole_x, foot_row), over(over(palette->background_fill, between), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(hole_x, foot_row)))),
              "the row the foot falls in is the ink at the alpha between");
    cli_check(adjacent(pixel(&raster, hole_x, foot_row - 1), over(over(palette->background_fill, pane), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(hole_x, foot_row - 1)))),
              "the row above it is the pane whole");
    cli_check(adjacent(pixel(&raster, hole_x, foot_row + 1), sheet_at(palette, palette->background_fill, hole_x, foot_row + 1)), "and the row below it the sheet");
    numbers.reveal_top = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    cli_check(EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 1) && (base.positioned_count == 5u), "the controls' pane located positions the map again");
    // the controls' tab halfway down with the bar away: its pane positioned by the base at the part halfway from the sheet's
    // tint to a pane's, as a slat's edge is
    numbers.reveal_top = 0;
    numbers.inset_top = 0;
    numbers.reveal_down = GUI_FIXED_ONE / 2;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    cli_check((frame.controls_pane_share == GUI_FIXED_ONE / 2) &&
                  (EMBED_CALL(gui_base_table.layer_at, GuiBaseCfg, .base_read = &base, .x = frame.controls_pane.x + (frame.controls_pane.width / 2),
                              .y = frame.controls_pane.y + (frame.controls_pane.height / 2)) ==
                   GUI_BASE_PANE_PART + (((GUI_BASE_PARTS / 2u) - 1u) * GUI_BASE_GRAIN_LEVELS)),
              "the controls' pane coming down is positioned by the base at its share's part");
    numbers.reveal_down = 0;
    // halfway through the claim's slide with the head not revealed, the tabs row's surface lies at half its alpha
    numbers.claim = GUI_FIXED_ONE / 2;
    numbers.reveal_top = 0;
    numbers.inset_top = 0;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    const GuiColor half_surface = (palette->surface & 0x00ffffffu) | ((((GUI_ALPHA_OF(palette->surface) * 8u) + 8u) / 16u) << 24u);
    cli_check(pixel(&raster, frame.tabs.x + 10, frame.tabs.y + (frame.tabs.height / 2)) == EMBED_CALL(gui_color.over, GuiRasterCfg, .background_color = palette->background_fill, .color = half_surface),
              "halfway through the claim a pixel of the tabs row is its surface at half its alpha over the lattice");
    // the side pane halfway gone: its ground where it is at the part halfway, and the side it has left the lattice
    numbers.claim = 0;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1, .away = GUI_FIXED_ONE / 2);
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    cli_check((EMBED_CALL(gui_base_table.layer_at, GuiBaseCfg, .base_read = &base, .x = frame.side.x + 2, .y = frame.side.y + 2) ==
               GUI_BASE_SIDE_PART + ((GUI_BASE_PARTS / 2u) - 1u)) &&
                  (EMBED_CALL(gui_base_table.layer_at, GuiBaseCfg, .base_read = &base, .x = frame.side.x + frame.side.width - 2,
                              .y = frame.side.y + frame.side.height - 2) == GUI_BASE_CLEAR),
              "the side pane halfway gone lies at its halfway part where it is, and the side it has left is the lattice");
    // the terminal's panel halfway through its arrival: its ground and its views' where they are, each at the part
    // halfway, and the band above it that it has yet to come up to the lattice
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 0);
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = 240, .away = GUI_FIXED_ONE / 2);
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    cli_check((frame.terminal_drawn.y > frame.terminal.y) &&
                  (EMBED_CALL(gui_base_table.layer_at, GuiBaseCfg, .base_read = &base, .x = frame.terminal_drawn.x + 2, .y = frame.terminal_drawn.y + 2) ==
                   GUI_BASE_SIDE_PART + ((GUI_BASE_PARTS / 2u) - 1u)) &&
                  (EMBED_CALL(gui_base_table.layer_at, GuiBaseCfg, .base_read = &base, .x = frame.terminal_views_drawn.x + 2, .y = frame.terminal_views_drawn.y + 2) ==
                   GUI_BASE_EDITOR_PART + ((GUI_BASE_PARTS / 2u) - 1u)) &&
                  (EMBED_CALL(gui_base_table.layer_at, GuiBaseCfg, .base_read = &base, .x = frame.terminal.x + 2, .y = frame.terminal.y + 2) == GUI_BASE_CLEAR),
              "the panel halfway through its arrival lies at its halfway part where it is, its views at the editor's, and the band above it is the lattice");

    background_fill.pixels = NULL;
    background_fill.width = 0u;
    background_fill.height = 0u;
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    base.tile_red_blue = NULL;
    base.tile_green = NULL;
    base.tile_remainder = NULL;
    base.width = 0u;
    base.height = 0u;
    base.positioned = 0;
    cli_check((base.tile_red_blue == NULL) && !base.positioned, "a base given back holds no tables");
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
