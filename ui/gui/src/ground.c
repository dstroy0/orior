// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ground.c: the window's ground, the lattice drawn into a raster of its own. The pens are made as the page makes them,
// one a level of disorder, its color the palette's three lattice colors blended by the level and its ink and radius
// by the order; the lattice blends them into its shades. Each drawing fills the ground color and puts every dot down
// where the loop stands, the dot's place and radius taken from the page's pixels to the window's by the scale
#include "ground.h"

#include "../../cli/src/command_line/cli_internal.h"

// which of the lattice's LATTICE_SLOTS are taken by grounds, a bit each
static uint32_t s_slots_taken;

// the color of the pen at `loose`, 0 to 1 from ordered to the least ordered: the palette's three lattice colors, the
// two either side of the level blended by where it stands between them, each channel rounded as the page rounds it
static GuiColor ground_pen_color(const GuiPalette *palette, double loose)
{
    const double along = loose * 2.0;
    // the lower stop is a whole count of 0 or 1
    const unsigned int low = (along < 1.0) ? 0u : 1u;
    const double part = along - (double)low;
    const GuiColor from = palette->lattice[low];
    const GuiColor to = palette->lattice[low + 1u];
    // a channel blended between two bytes, rounded, is a byte
    const unsigned int red =
        (unsigned int)((double)GUI_RED_OF(from) + ((double)GUI_RED_OF(to) - (double)GUI_RED_OF(from)) * part + 0.5);
    const unsigned int green = (unsigned int)((double)GUI_GREEN_OF(from) +
                                              ((double)GUI_GREEN_OF(to) - (double)GUI_GREEN_OF(from)) * part + 0.5);
    const unsigned int blue =
        (unsigned int)((double)GUI_BLUE_OF(from) + ((double)GUI_BLUE_OF(to) - (double)GUI_BLUE_OF(from)) * part + 0.5);
    return GUI_RGB(red, green, blue);
}

// the shades made from `palette`'s pens
static void ground_shaded(GuiGround *ground, const GuiPalette *palette)
{
    LatticePens pens;
    for (unsigned int level = 0u; level < LATTICE_LEVELS; level += 1u)
    {
        const double loose = (double)level / (LATTICE_LEVELS - 1);
        const double order = 1.0 - loose;
        const GuiColor color = ground_pen_color(palette, loose);
        // a byte as a share of 255, and the ink's share from the faintest to the brightest, are shares a float holds
        pens.color[level * 4u] = (float)GUI_RED_OF(color) / 255.0f;
        pens.color[level * 4u + 1u] = (float)GUI_GREEN_OF(color) / 255.0f;
        pens.color[level * 4u + 2u] = (float)GUI_BLUE_OF(color) / 255.0f;
        pens.color[level * 4u + 3u] = (float)(GUI_GROUND_FAINTEST * (1.0 - order) + GUI_GROUND_BRIGHTEST * order);
        pens.radius[level] = (float)(GUI_GROUND_RADIUS + GUI_GROUND_RADIUS_SPREAD * order);
    }
    lattice_shaded(&pens, &ground->shades);
}

int gui_ground_made(GuiGround *ground, unsigned long long now)
{
    memor.set(&(MemoriaCfg){.dst = ground, .val = 0, .bytes = sizeof(*ground)});
    for (uint32_t slot = 0u; slot < LATTICE_SLOTS; slot += 1u)
    {
        if ((s_slots_taken & (1u << slot)) == 0u)
        {
            s_slots_taken |= 1u << slot;
            ground->slot = slot;
            ground->began_ms = now;
            return 1;
        }
    }
    return 0;
}

void gui_ground_given(GuiGround *ground)
{
    // the raster is the region's and goes back with it; the ground lets go of its view of it
    memor.set(&(MemoriaCfg){.dst = &ground->raster, .val = 0, .bytes = sizeof(ground->raster)});
    lattice_given(ground->slot);
    s_slots_taken &= ~(1u << ground->slot);
    ground->laid = 0;
}

double gui_ground_seconds(const GuiGround *ground, unsigned long long now)
{
    const unsigned long long paused_ms = (ground->paused_since_ms != 0ull) ? now - ground->paused_since_ms : 0ull;
    // the milliseconds since the clock began are far inside what a double holds whole
    return (double)(now - ground->began_ms - ground->lost_ms - paused_ms) / 1000.0;
}

int gui_ground_laid(GuiGround *ground, const GuiRaster *raster, unsigned int width, unsigned int height, float scale,
                    const GuiPalette *palette, unsigned long long now)
{
    // the raster is the region's, the framebuffers' pitch wide, the lattice laid over the window's own width
    ground->raster = *raster;
    ground->width = width;
    ground->height = height;
    ground->scale = scale;
    ground->laid = 0;
    ground_shaded(ground, palette);
    // the raster is drawn at the size either way, its ground color alone where the lattice could not be laid
    ground->laid = lattice_lay(ground->slot, (double)width / (double)scale, (double)height / (double)scale) != 0u;
    gui_ground_drawn(ground, palette, now);
    return ground->laid;
}

void gui_ground_drawn(GuiGround *ground, const GuiPalette *palette, unsigned long long now)
{
    GuiRaster *const raster = &ground->raster;
    const double seconds = gui_ground_seconds(ground, now);
    // the raster zeroed first, since the ground color is blended over what the raster holds and a cell kept across
    // sizes holds the ground last drawn at another pitch; a raster's sides are far inside what an int represents
    memor.set(&(MemoriaCfg){.dst = raster->pixels,
                            .val = 0,
                            .bytes = (unsigned long long)raster->width * raster->height * sizeof(GuiColor)});
    gui_raster_filled(raster, (GuiRect){0, 0, (int)raster->width, (int)raster->height}, palette->ground);
    ground->drawn_at = seconds;
    ground->shows_at = lattice_shown_after(seconds, seconds);
    ground->frames += 1u;
    if (!ground->laid)
    {
        return;
    }
    LatticeSlot *const lattice = lattice_slot(ground->slot);
    if (!lattice_dots_moved(lattice, (double)ground->width / (double)ground->scale,
                            (double)ground->height / (double)ground->scale, seconds, &ground->shades))
    {
        return;
    }
    for (uint32_t point = 0u; point < lattice->count; point += 1u)
    {
        const LatticeDot *const dot = &lattice->dots[point];
        // the dot's bytes stand red, green, blue and ink in that order
        const GuiColor color = GUI_RGBA(dot->color & 0xffu, (dot->color >> 8u) & 0xffu, (dot->color >> 16u) & 0xffu,
                                        (dot->color >> 24u) & 0xffu);
        gui_raster_dotted(raster, dot->across * ground->scale, dot->down * ground->scale, dot->radius * ground->scale,
                          color);
    }
}

int gui_ground_ticked(GuiGround *ground, const GuiPalette *palette, unsigned long long now)
{
    if (!ground->laid || (ground->paused_since_ms != 0ull))
    {
        return 0;
    }
    if (!lattice_shows(ground->drawn_at, gui_ground_seconds(ground, now)))
    {
        return 0;
    }
    gui_ground_drawn(ground, palette, now);
    return 1;
}

unsigned long long gui_ground_due(const GuiGround *ground, unsigned long long now)
{
    if (!ground->laid || (ground->paused_since_ms != 0ull))
    {
        return 0ull;
    }
    // the wait until the moment found as the ground was drawn, in whole milliseconds, at least one, is far inside
    // what an unsigned long long holds
    const double wait = (ground->shows_at - gui_ground_seconds(ground, now)) * 1000.0;
    return (wait < 1.0) ? 1ull : (unsigned long long)wait;
}

void gui_ground_paused(GuiGround *ground, int paused, unsigned long long now)
{
    if (paused && (ground->paused_since_ms == 0ull))
    {
        ground->paused_since_ms = now;
    }
    else if (!paused && (ground->paused_since_ms != 0ull))
    {
        ground->lost_ms += now - ground->paused_since_ms;
        ground->paused_since_ms = 0ull;
    }
}
