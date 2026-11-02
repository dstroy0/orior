// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lattice_test.c: the lattice's check: a lattice laid out over a box has as many points as stand inside it, each
// point's offsets seeded and the same every time and wherever the box ends; the loop's stage at each moment; the
// shades blended from the pens; a point on the ordered side standing at its origin and one on the disordered side
// moved by its offset's share of the reach; the loop showing no change through the rest and a change through the
// spread, and the soonest moment it shows one; and the window's ground: laid out at a scale, drawn with a dot at a
// point's origin, due at the moment the loop next shows, drawn again on a tick past it and lattice still off the screen
#include "../../../../../ui/gui/src/ground.h"
#include "../cli/cli_check.h"
#include "test_clock.h"

#include <math.h>

// the `entry` of the lattice the computing is checked in, past those the grounds take
#define CHECK_ENTRY 7u

static int adjacent(double value, double required, double within)
{
    return fabs(value - required) <= within;
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    const double step = 22.0 / sqrt(3.0) / sqrt(8.0);

    // laid out
    cli_check(lattice_lay(CHECK_ENTRY, 100.0, 50.0) == 242u, "a lattice over 100 by 50 has 22 columns of 11 rows");
    LatticeSlot *const lattice = lattice_slot(CHECK_ENTRY);
    cli_check((lattice->columns == 22u) && (lattice->rows == 11u) && (lattice->count == 242u) &&
                  adjacent(lattice->step, step, 1e-9) && adjacent(lattice->half, step / 2.0, 1e-9),
              "the points stand a step apart from half a step in");
    const int8_t first_across = lattice->offsets[0];
    const int8_t first_down = lattice->offsets[1];
    const int8_t last_across = lattice->offsets[241u * 2u];
    cli_check(lattice_lay(CHECK_ENTRY, 100.0, 50.0) == 242u, "the lattice laid again has the same count");
    cli_check((lattice->offsets[0] == first_across) && (lattice->offsets[1] == first_down) &&
                  (lattice->offsets[241u * 2u] == last_across),
              "the offsets are seeded and the same every time");
    cli_check((lattice_lay(CHECK_ENTRY, 200.0, 100.0) == 45u * 22u) && (lattice->offsets[0] == first_across) &&
                  (lattice->offsets[1] == first_down),
              "a point's offsets stay its own wherever the box ends");
    cli_check(lattice_lay(LATTICE_SLOTS, 100.0, 50.0) == 0u, "a slot past the last lays nothing");
    cli_check(lattice_lay(CHECK_ENTRY, 1.0, 1.0) == 1u, "a box smaller than a step holds one point");

    // the loop's stages
    const LatticeStageAt *at = lattice_stage(0.0);
    cli_check((at->kind == LATTICE_SIDED) && (at->angle == 0.0) && (at->part == 0.0),
              "the loop begins at rest, the left side ordered");
    at = lattice_stage(36.0);
    cli_check((at->kind == LATTICE_SPREAD) && adjacent(at->front, 0.55, 1e-9) && adjacent(at->part, 0.5, 1e-9),
              "halfway through the spread the front stands halfway across the ramp, eased");
    at = lattice_stage(66.0);
    cli_check((at->kind == LATTICE_WHOLE) && adjacent(at->part, 0.5, 1e-9),
              "halfway through the inversion the whole lattice is half loose");
    at = lattice_stage(90.0);
    cli_check((at->kind == LATTICE_GATHER) && adjacent(at->angle, LATTICE_PI / 2.0, 1e-9),
              "the gathering orders from the top");
    at = lattice_stage(120.0);
    cli_check((at->kind == LATTICE_SIDED) && adjacent(at->angle, LATTICE_PI * 0.75, 1e-9),
              "halfway through the first turn the side stands at three eighths");
    at = lattice_stage(152.0);
    cli_check((at->kind == LATTICE_SIDED) && adjacent(at->angle, LATTICE_PI * 1.25, 1e-9),
              "the second turn counts the first");
    at = lattice_stage(200.0);
    cli_check((at->kind == LATTICE_SIDED) && (at->angle == 0.0) && (at->part == 0.0),
              "the loop comes round after two hundred seconds");

    // the shades
    LatticePens pens;
    for (unsigned int level = 0u; level < LATTICE_LEVELS; level += 1u)
    {
        const float share = (float)level / (float)(LATTICE_LEVELS - 1);
        pens.color[level * 4u] = 1.0f - share;
        pens.color[level * 4u + 1u] = 0.0f;
        pens.color[level * 4u + 2u] = share;
        pens.color[level * 4u + 3u] = 0.5f;
        pens.radius[level] = 1.0f + share;
    }
    LatticeShades shades;
    lattice_shaded(&pens, &shades);
    cli_check(shades.color[0] == 0x800000ffu, "the first shade is the first pen, red first in memory and the ink last");
    cli_check(shades.color[LATTICE_SHADES - 1u] == 0x80ff0000u, "the last shade is the last pen");
    cli_check(adjacent(shades.radius[0], 1.0 * LATTICE_FITS, 1e-6) &&
                  adjacent(shades.radius[LATTICE_SHADES - 1u], 2.0 * LATTICE_FITS, 1e-6),
              "a shade's radius is the pen's at the share that fits");
    cli_check((shades.color[128] & 0xffu) < 0xffu && (shades.color[128] & 0xffu) > 0u,
              "a shade between two pens blends them");

    // moved
    cli_check(lattice_lay(CHECK_ENTRY, 100.0, 50.0) == 242u, "the lattice is laid over 100 by 50 again");
    cli_check(lattice_dots_moved(lattice, 100.0, 50.0, 0.0, &shades), "the dots are moved at the loop's start");
    cli_check(adjacent(lattice->dots[0].across, step / 2.0, 1e-5) && adjacent(lattice->dots[0].down, step / 2.0, 1e-5) &&
                  (lattice->dots[0].color == shades.color[0]),
              "a point on the ordered side stands at its origin in the first shade");
    const LatticeDot *const last = &lattice->dots[241];
    const double reach = LATTICE_REACH / LATTICE_OFFSET_MOST;
    cli_check(adjacent(last->across, step / 2.0 + 21.0 * step + lattice->offsets[241u * 2u] * reach, 1e-4) &&
                  (last->color == shades.color[LATTICE_SHADES - 1u]),
              "a point on the disordered side is moved by its offset's share of the reach in the last shade");

    // shown
    cli_check(!lattice_shows(0.0, 15.0), "the loop shows no change through the rest");
    cli_check(lattice_shows(0.0, 30.0), "the loop shows a change through the spread");
    const double after = lattice_shown_after(0.0, 0.0);
    cli_check((after >= 16.0) && (after <= 20.0), "the first change shows soon after the spread begins");
    cli_check(lattice_shown_after(0.0, after + 0.1) >= after + 0.1,
              "the soonest moment is never before the moment asked from");

    // the ground
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiBackground background_fill;
    g_test_clock_ms = 1000ull;
    cli_check(EMBED_CALL(gui_background.init, GuiBackgroundCfg, .background_fill = &background_fill), "a ground takes its own lattice instance");
    GuiRaster background_raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &background_raster, .width = EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 200u), .height = 100u),
              "a raster for the ground is made at the pitch");
    g_test_clock_ms = 1000ull;
    cli_check(EMBED_CALL(gui_background_raster.layout, GuiBackgroundRasterCfg, .background_fill = &background_fill, .raster = &background_raster, .width = 200u, .height = 100u, .scale = 200, .palette = palette),
              "the ground is laid out at twice the scale");
    cli_check((background_fill.raster.width == EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 200u)) && (background_fill.raster.height == 100u) &&
                  (lattice_slot(background_fill.lattice_index)->count == 242u),
              "the lattice is laid in the page's pixels, half the window's");
    cli_check(background_fill.frames == 1u, "the ground laid is drawn once");
    const GuiColor origin = background_fill.raster.pixels[(4u * 200u) + 4u];
    cli_check(origin != palette->deep, "a dot stands at the first point's origin");
    cli_check(background_fill.raster.pixels[(99u * 200u) + 0u] == palette->deep,
              "the corner clear of every dot is the deep color");
    g_test_clock_ms = 1000ull;
    const unsigned long long wait_ms = EMBED_CALL(gui_background_clock.next_deadline_ms, GuiBackgroundClockCfg, .background_fill = &background_fill) - 1000ull;
    cli_check((wait_ms >= 16000ull) && (wait_ms <= 20000ull), "the next draw's deadline falls as the first change shows");
    g_test_clock_ms = 6000ull;
    cli_check(!EMBED_CALL(gui_background_raster.tick, GuiBackgroundRasterCfg, .background_fill = &background_fill, .palette = palette) && (background_fill.frames == 1u),
              "a tick through the rest draws nothing");
    g_test_clock_ms = 31000ull;
    cli_check(EMBED_CALL(gui_background_raster.tick, GuiBackgroundRasterCfg, .background_fill = &background_fill, .palette = palette) && (background_fill.frames == 2u),
              "a tick through the spread draws the ground again");
    cli_check(background_fill.next_draw_ms > 30000ull, "the next draw falls after this one");
    g_test_clock_ms = 32000ull;
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &background_fill, .paused = 1);
    g_test_clock_ms = 40000ull;
    cli_check(EMBED_CALL(gui_background_clock.next_deadline_ms, GuiBackgroundClockCfg, .background_fill = &background_fill) == 0ull, "the ground off screen has no deadline");
    g_test_clock_ms = 40000ull;
    cli_check(EMBED_CALL(gui_background_clock.elapsed_ms, GuiBackgroundClockCfg, .background_fill = &background_fill) == 31000ull, "the clock stands while the ground is lattice");
    g_test_clock_ms = 40000ull;
    cli_check(!EMBED_CALL(gui_background_raster.tick, GuiBackgroundRasterCfg, .background_fill = &background_fill, .palette = palette), "a tick while lattice draws nothing");
    g_test_clock_ms = 40000ull;
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &background_fill, .paused = 0);
    g_test_clock_ms = 41000ull;
    cli_check(EMBED_CALL(gui_background_clock.elapsed_ms, GuiBackgroundClockCfg, .background_fill = &background_fill) == 32000ull, "the clock takes up where it stood");
    GuiBackground other;
    g_test_clock_ms = 0ull;
    cli_check(EMBED_CALL(gui_background.init, GuiBackgroundCfg, .background_fill = &other) && (other.lattice_index != background_fill.lattice_index), "another ground takes another lattice instance");
    EMBED_CALL(gui_background.free, GuiBackgroundCfg, .background_fill = &other);
    EMBED_CALL(gui_background.free, GuiBackgroundCfg, .background_fill = &background_fill);
    cli_check((background_fill.raster.pixels == NULL) && !background_fill.laid, "the ground given holds no raster");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &background_raster);
    lattice_given(CHECK_ENTRY);
    return cli_on_check();
}
