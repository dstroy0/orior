// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ground_bench.c: the ground's draw timed through one loop of the lattice at the window's size. The bench draws the
// ground at each step of the loop and times each draw. It writes one hash of every raster it drew, so a change to the
// draw that moves a pixel changes the hash. It also draws each step again with the float dot below, and counts the
// pixels where the two disagree and by how many levels of 255
#include "../../../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../../../ui/cli/src/view/ground.h"
#include "test_clock.h"

#include <math.h>
#include <stdio.h>

// the window's size in pixels at the scale of one, and the step through the loop in milliseconds
#define BENCH_WIDTH 1440u
#define BENCH_HEIGHT 920u
#define BENCH_STEP_MS 250ull
#define BENCH_LOOP_MS 200000ull
// the most pixels a row of the reference raster holds, past any pitch of the bench's width
#define BENCH_PITCH_MOST 2048u

static GuiColor s_reference[BENCH_PITCH_MOST * BENCH_HEIGHT];
// the pixels whose largest channel gap is each count of levels, from 0 to 255
static unsigned long long s_gaps[256];

// The FNV-1a hash of the raster's pixels, row by row inside the width, folded into `hash`
static unsigned long long raster_hashed(const GuiRaster *raster, unsigned int width, unsigned long long hash)
{
    for (unsigned int row = 0u; row < raster->height; row += 1u)
    {
        const GuiColor *const pixels = &raster->pixels[(unsigned long long)row * raster->width];
        for (unsigned int column = 0u; column < width; column += 1u)
        {
            hash = (hash ^ (unsigned long long)pixels[column]) * 0x100000001b3ull;
        }
    }
    return hash;
}

// One channel of the float dot: the color's share and the threshold over the ground's rest
static unsigned int reference_channel(float color, unsigned int background, float share, float threshold, float rest)
{
    const float laid = (color * share) + threshold;
    const float within = (laid < 0.0f) ? 0.0f : ((laid > 255.0f) ? 255.0f : laid);
    const float out = within + ((float)background * rest);
    return (out >= 255.0f) ? 255u : (unsigned int)(out + 0.5f);
}

// The float dot, the reference the fixed-point dot is checked against
static void reference_dot(GuiColor *pixels, unsigned int pitch, float x, float y, float radius, GuiColor color)
{
    const float ink = (float)GUI_ALPHA_OF(color) / 255.0f;
    const float reach = radius + 1.0f;
    const int left = (int)floorf(x - reach);
    const int top = (int)floorf(y - reach);
    const int right = (int)ceilf(x + reach);
    const int bottom = (int)ceilf(y + reach);
    for (int row = (top < 0) ? 0 : top; (row <= bottom) && (row < (int)BENCH_HEIGHT); row += 1)
    {
        for (int column = (left < 0) ? 0 : left; (column <= right) && (column < (int)BENCH_WIDTH); column += 1)
        {
            const float across = ((float)column + 0.5f) - x;
            const float down = ((float)row + 0.5f) - y;
            const float inside = radius - hypotf(across, down) + 0.5f;
            if (inside <= 0.0f)
            {
                continue;
            }
            const float share = ink * ((inside < 1.0f) ? inside : 1.0f);
            const unsigned int order = (((unsigned int)column & 1u) << 3u) | (((unsigned int)row & 1u) << 2u) |
                                       ((((unsigned int)column >> 1u) & 1u) << 1u) | (((unsigned int)row >> 1u) & 1u);
            const float threshold = ((float)order - 7.5f) / 16.0f;
            const float alpha = (share * 255.0f) + threshold;
            const float rest = (alpha < 0.0f) ? 1.0f : ((alpha > 255.0f) ? 0.0f : (1.0f - (alpha / 255.0f)));
            GuiColor *const at = &pixels[((unsigned int)row * pitch) + (unsigned int)column];
            const GuiColor ground = *at;
            *at = GUI_RGB(reference_channel((float)GUI_RED_OF(color), GUI_RED_OF(ground), share, threshold, rest),
                          reference_channel((float)GUI_GREEN_OF(color), GUI_GREEN_OF(ground), share, threshold, rest),
                          reference_channel((float)GUI_BLUE_OF(color), GUI_BLUE_OF(ground), share, threshold, rest));
        }
    }
}

// The pixels of the drawn raster against the reference: the count that differ, and the most levels by which a
// channel differs, folded into `differ` and `most`
static void rasters_compared(const GuiRaster *raster, unsigned long long *differ, unsigned int *most)
{
    for (unsigned int row = 0u; row < BENCH_HEIGHT; row += 1u)
    {
        for (unsigned int column = 0u; column < BENCH_WIDTH; column += 1u)
        {
            const GuiColor drawn = raster->pixels[(row * raster->width) + column];
            const GuiColor kept = s_reference[(row * raster->width) + column];
            if (drawn == kept)
            {
                continue;
            }
            *differ += 1ull;
            unsigned int widest = 0u;
            const int gaps[3] = {(int)GUI_RED_OF(drawn) - (int)GUI_RED_OF(kept),
                                 (int)GUI_GREEN_OF(drawn) - (int)GUI_GREEN_OF(kept),
                                 (int)GUI_BLUE_OF(drawn) - (int)GUI_BLUE_OF(kept)};
            for (unsigned int channel = 0u; channel < 3u; channel += 1u)
            {
                const unsigned int gap = (unsigned int)((gaps[channel] < 0) ? -gaps[channel] : gaps[channel]);
                *most = (gap > *most) ? gap : *most;
                widest = (gap > widest) ? gap : widest;
            }
            s_gaps[widest] += 1ull;
        }
    }
}

int main(void)
{
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiBackground background_fill;
    g_test_clock_ms = 1000ull;
    GuiRaster raster;
    if (!EMBED_CALL(gui_background.init, GuiBackgroundCfg, .background_fill = &background_fill) ||
        !EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster,
                    .width = EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = BENCH_WIDTH),
                    .height = BENCH_HEIGHT) ||
        (raster.width > BENCH_PITCH_MOST) ||
        !EMBED_CALL(gui_background_raster.layout, GuiBackgroundRasterCfg, .background_fill = &background_fill,
                    .raster = &raster, .width = BENCH_WIDTH, .height = BENCH_HEIGHT, .scale = GUI_SCALE_ONE,
                    .palette = palette))
    {
        (void)printf("the ground did not lay out\n");
        return 1;
    }
    const unsigned int pitch = background_fill.raster.width;
    unsigned long long hash = 0xcbf29ce484222325ull;
    unsigned long long total_us = 0ull;
    unsigned long long most_us = 0ull;
    unsigned long long differ = 0ull;
    unsigned int most_levels = 0u;
    unsigned int draws = 0u;
    for (unsigned long long at = 0ull; at < BENCH_LOOP_MS; at += BENCH_STEP_MS)
    {
        g_test_clock_ms = 1000ull + at;
        const unsigned long long before = cli_clock_microseconds();
        gui_background_raster.draw(&(GuiBackgroundRasterCfg){.background_fill = &background_fill, .palette = palette});
        const unsigned long long spent = cli_clock_microseconds() - before;
        total_us += spent;
        most_us = (spent > most_us) ? spent : most_us;
        draws += 1u;
        hash = raster_hashed(&background_fill.raster, BENCH_WIDTH, hash);
        // the reference: the deep color, then every dot the lattice holds now drawn with the float dot
        for (unsigned int pixel = 0u; pixel < pitch * BENCH_HEIGHT; pixel += 1u)
        {
            s_reference[pixel] = palette->deep | 0xff000000u;
        }
        // a draw where the dots did not move holds the deep color alone, and the reference skips it
        unsigned int dotted = 0u;
        for (unsigned int pixel = 0u; (pixel < pitch * BENCH_HEIGHT) && (dotted == 0u); pixel += 1u)
        {
            dotted = (background_fill.raster.pixels[pixel] != (palette->deep | 0xff000000u)) ? 1u : 0u;
        }
        const LatticeSlot *const lattice = lattice_slot(background_fill.lattice_index);
        for (uint32_t point = 0u; (dotted != 0u) && (point < lattice->count); point += 1u)
        {
            const LatticeDot *const dot = &lattice->dots[point];
            reference_dot(s_reference, pitch, dot->across, dot->down, dot->radius,
                          GUI_RGBA(dot->color & 0xffu, (dot->color >> 8u) & 0xffu, (dot->color >> 16u) & 0xffu,
                                   (dot->color >> 24u) & 0xffu));
        }
        if (dotted != 0u)
        {
            rasters_compared(&background_fill.raster, &differ, &most_levels);
        }
    }
    (void)printf("ground %ux%u dots %u draws %u total %llu us mean %llu us most %llu us hash %016llx\n", BENCH_WIDTH,
                 BENCH_HEIGHT, lattice_slot(background_fill.lattice_index)->count, draws, total_us,
                 total_us / draws, most_us, hash);
    (void)printf("against the float dot: %llu pixels differ over %u draws, the most by %u levels of 255\n", differ,
                 draws, most_levels);
    for (unsigned int gap = 1u; gap <= most_levels; gap += 1u)
    {
        (void)printf("  %u levels: %llu pixels\n", gap, s_gaps[gap]);
    }
    return 0;
}
