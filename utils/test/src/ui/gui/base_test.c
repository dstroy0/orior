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
    (void)cli_cells_made(CLI_CELLS_POOL);
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    GuiFrame frame;
    gui_frame_laid(&frame, 800u, 600u, 1.0f);
    GuiBase base;
    cli_check(gui_base_made(&base, 800u, 600u), "a base for a window of eight hundred by six hundred is made");
    cli_check(!base.laid && (base.laid_count == 0u), "a base made is laid for nothing yet");
    cli_check(gui_base_laid(&base, &frame, palette, 0), "the map is laid for the frame");
    cli_check(!gui_base_laid(&base, &frame, palette, 0) && (base.laid_count == 1u), "laid again for the same frame it stands");
    cli_check(GUI_BASE_ENTRIES <= 256u, "the entries fit a byte");
    cli_check(GUI_BASE_GRAIN_LEVELS == (unsigned int)((GUI_GRAIN_ALPHA * 255.0) + 0.5) + 1u, "the grain's levels run to its most alpha");

    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 800u, 600u), "a raster for the frame is made");
    gui_base_drawn(&base, &raster, NULL, palette->ground);
    const int editor_x = frame.editor.x + (frame.editor.width / 2);
    const int editor_y = frame.editor.y + (frame.editor.height / 2);
    cli_check(pixel(&raster, editor_x, editor_y) == gui_color_over(palette->ground, palette->editor_ground),
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
    const GuiColor pane = gui_glass_pane_tint(palette->glass);
    cli_check(GUI_ALPHA_OF(pane) < GUI_ALPHA_OF(palette->glass), "a pane's tint is thinner than the glass's");
    cli_check(near(pixel(&raster, bar_x, bar_y), over(over(palette->ground, pane), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(bar_x, bar_y)))),
              "a pixel of the bar's pane is the pane's tint with the grain");
    cli_check(pixel(&raster, bar_x, bar_y) != pixel(&raster, gap_x, gap_y) || (grain_at(bar_x, bar_y) != grain_at(gap_x, gap_y)),
              "the pane reads apart from the sheet");

    // the lattice as a raster of the window's size is read pixel for pixel
    GuiRaster ground;
    cli_check(gui_raster_made(&ground, 800u, 600u), "a lattice raster is made");
    gui_raster_filled(&ground, (GuiRect){0, 0, 800, 600}, GUI_RGB(0x20u, 0x40u, 0x60u));
    gui_base_drawn(&base, &raster, &ground, palette->ground);
    cli_check(pixel(&raster, editor_x, editor_y) == gui_color_over(GUI_RGB(0x20u, 0x40u, 0x60u), palette->editor_ground),
              "with a lattice raster the editor's ground lies over the lattice's pixel");
    cli_check(pixel(&raster, frame.map.x + 10, frame.map.y + 100) == GUI_RGB(0x20u, 0x40u, 0x60u), "and the map's column is the lattice's pixel");
    GuiRaster small;
    cli_check(gui_raster_made(&small, 8u, 4u), "a raster of another size is made");
    gui_base_drawn(&base, &small, NULL, palette->ground);
    cli_check(small.pixels[0] == 0u, "a raster not the base's size is left alone");
    gui_raster_given(&small);

    // the claim holds with the bar in: its slat makes a hole in the work, the sheet, with the slat's pane in it
    GuiClaimNumbers numbers = frame.claim;
    numbers.claim = 1.0f;
    numbers.reveal_top = 1.0f;
    numbers.grown_top = 1.0f;
    gui_frame_claimed(&frame, &numbers);
    GuiRect holes[4];
    cli_check(gui_frame_holes(&frame, holes) >= 1u, "the bar's slat makes a hole in the work");
    cli_check(gui_base_laid(&base, &frame, palette, 0) && (base.laid_count == 2u), "the map is laid again for the claim's frame");
    gui_base_drawn(&base, &raster, NULL, palette->ground);
    const int hole_x = holes[0].x + (holes[0].width / 2);
    const int below_slat = frame.bar_slat.y + frame.bar_slat.height + 4;
    cli_check(near(pixel(&raster, hole_x, below_slat), sheet_at(palette, palette->ground, hole_x, below_slat)),
              "a pixel of the hole under the slat is the sheet");
    const int slat_y = frame.bar_slat.y + (frame.bar_slat.height / 2);
    cli_check(near(pixel(&raster, hole_x, slat_y), over(over(palette->ground, pane), GUI_RGBA(0xffu, 0xffu, 0xffu, grain_at(hole_x, slat_y)))),
              "a pixel of the slat in the hole is the pane");
    cli_check(!gui_base_laid(&base, &frame, palette, 0), "laid again for the claim's frame it stands");
    cli_check(gui_base_laid(&base, &frame, palette, 1) && (base.laid_count == 3u), "the controls' pane standing lays the map again");

    gui_raster_given(&ground);
    gui_raster_given(&raster);
    gui_base_given(&base);
    cli_check((base.tile_red_blue == NULL) && !base.laid, "a base given back holds no tables");
    return cli_checked();
}
