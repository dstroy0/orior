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

#include <math.h>

// the `slot` of the lattice the computing is checked in, past those the grounds take
#define CHECK_SLOT 7u

static int near(double value, double wanted, double within)
{
    return fabs(value - wanted) <= within;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    const double step = 22.0 / sqrt(3.0) / sqrt(8.0);

    // laid out
    cli_check(lattice_lay(CHECK_SLOT, 100.0, 50.0) == 242u, "a lattice over 100 by 50 has 22 columns of 11 rows");
    LatticeSlot *const lattice = lattice_slot(CHECK_SLOT);
    cli_check((lattice->columns == 22u) && (lattice->rows == 11u) && (lattice->count == 242u) &&
                  near(lattice->step, step, 1e-9) && near(lattice->half, step / 2.0, 1e-9),
              "the points stand a step apart from half a step in");
    const int8_t first_across = lattice->offsets[0];
    const int8_t first_down = lattice->offsets[1];
    const int8_t last_across = lattice->offsets[241u * 2u];
    cli_check(lattice_lay(CHECK_SLOT, 100.0, 50.0) == 242u, "the lattice laid again has the same count");
    cli_check((lattice->offsets[0] == first_across) && (lattice->offsets[1] == first_down) &&
                  (lattice->offsets[241u * 2u] == last_across),
              "the offsets are seeded and the same every time");
    cli_check((lattice_lay(CHECK_SLOT, 200.0, 100.0) == 45u * 22u) && (lattice->offsets[0] == first_across) &&
                  (lattice->offsets[1] == first_down),
              "a point's offsets stay its own wherever the box ends");
    cli_check(lattice_lay(LATTICE_SLOTS, 100.0, 50.0) == 0u, "a slot past the last lays nothing");
    cli_check(lattice_lay(CHECK_SLOT, 1.0, 1.0) == 1u, "a box smaller than a step holds one point");

    // the loop's stages
    const LatticeStageAt *at = lattice_stage(0.0);
    cli_check((at->kind == LATTICE_SIDED) && (at->angle == 0.0) && (at->part == 0.0),
              "the loop begins at rest, the left side ordered");
    at = lattice_stage(36.0);
    cli_check((at->kind == LATTICE_SPREAD) && near(at->front, 0.55, 1e-9) && near(at->part, 0.5, 1e-9),
              "halfway through the spread the front stands halfway across the ramp, eased");
    at = lattice_stage(66.0);
    cli_check((at->kind == LATTICE_WHOLE) && near(at->part, 0.5, 1e-9),
              "halfway through the inversion the whole lattice is half loose");
    at = lattice_stage(90.0);
    cli_check((at->kind == LATTICE_GATHER) && near(at->angle, LATTICE_PI / 2.0, 1e-9),
              "the gathering orders from the top");
    at = lattice_stage(120.0);
    cli_check((at->kind == LATTICE_SIDED) && near(at->angle, LATTICE_PI * 0.75, 1e-9),
              "halfway through the first turn the side stands at three eighths");
    at = lattice_stage(152.0);
    cli_check((at->kind == LATTICE_SIDED) && near(at->angle, LATTICE_PI * 1.25, 1e-9),
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
    cli_check(near(shades.radius[0], 1.0 * LATTICE_FITS, 1e-6) &&
                  near(shades.radius[LATTICE_SHADES - 1u], 2.0 * LATTICE_FITS, 1e-6),
              "a shade's radius is the pen's at the share that fits");
    cli_check((shades.color[128] & 0xffu) < 0xffu && (shades.color[128] & 0xffu) > 0u,
              "a shade between two pens blends them");

    // moved
    cli_check(lattice_lay(CHECK_SLOT, 100.0, 50.0) == 242u, "the lattice is laid over 100 by 50 again");
    cli_check(lattice_dots_moved(lattice, 100.0, 50.0, 0.0, &shades), "the dots are moved at the loop's start");
    cli_check(near(lattice->dots[0].across, step / 2.0, 1e-5) && near(lattice->dots[0].down, step / 2.0, 1e-5) &&
                  (lattice->dots[0].color == shades.color[0]),
              "a point on the ordered side stands at its origin in the first shade");
    const LatticeDot *const last = &lattice->dots[241];
    const double reach = LATTICE_REACH / LATTICE_OFFSET_MOST;
    cli_check(near(last->across, step / 2.0 + 21.0 * step + lattice->offsets[241u * 2u] * reach, 1e-4) &&
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
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    GuiGround ground;
    cli_check(gui_ground_made(&ground, 1000ull), "a ground is made with a slot of its own");
    GuiRaster ground_raster;
    cli_check(gui_raster_made(&ground_raster, gui_raster_pitch(200u), 100u),
              "a raster for the ground is made at the pitch");
    cli_check(gui_ground_laid(&ground, &ground_raster, 200u, 100u, 2.0f, palette, 1000ull),
              "the ground is laid out at twice the scale");
    cli_check((ground.raster.width == gui_raster_pitch(200u)) && (ground.raster.height == 100u) &&
                  (lattice_slot(ground.slot)->count == 242u),
              "the lattice is laid in the page's pixels, half the window's");
    cli_check(ground.frames == 1u, "the ground laid is drawn once");
    const GuiColor origin = ground.raster.pixels[(4u * 200u) + 4u];
    cli_check(origin != palette->ground, "a dot stands at the first point's origin");
    cli_check(ground.raster.pixels[(99u * 200u) + 0u] == palette->ground,
              "the corner clear of every dot is the ground color");
    const unsigned long long due = gui_ground_due(&ground, 1000ull);
    cli_check((due >= 16000ull) && (due <= 20000ull), "the ground is due as the first change shows");
    cli_check(!gui_ground_ticked(&ground, palette, 6000ull) && (ground.frames == 1u),
              "a tick through the rest draws nothing");
    cli_check(gui_ground_ticked(&ground, palette, 31000ull) && (ground.frames == 2u),
              "a tick through the spread draws the ground again");
    cli_check(near(ground.drawn_at, 30.0, 1e-9), "the ground keeps the moment it was drawn at");
    gui_ground_paused(&ground, 1, 32000ull);
    cli_check(gui_ground_due(&ground, 40000ull) == 0ull, "the ground lattice is never due");
    cli_check(near(gui_ground_seconds(&ground, 40000ull), 31.0, 1e-9), "the clock stands while the ground is lattice");
    cli_check(!gui_ground_ticked(&ground, palette, 40000ull), "a tick while lattice draws nothing");
    gui_ground_paused(&ground, 0, 40000ull);
    cli_check(near(gui_ground_seconds(&ground, 41000ull), 32.0, 1e-9), "the clock takes up where it stood");
    GuiGround other;
    cli_check(gui_ground_made(&other, 0ull) && (other.slot != ground.slot), "another ground takes another slot");
    gui_ground_given(&other);
    gui_ground_given(&ground);
    cli_check((ground.raster.pixels == NULL) && !ground.laid, "the ground given holds no raster");
    gui_raster_given(&ground_raster);
    lattice_given(CHECK_SLOT);
    return cli_checked();
}
