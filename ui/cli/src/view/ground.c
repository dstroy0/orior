// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ground.c: the lattice drawn into a raster of its own. Doubles appear only where this file calls the lattice, whose
// interface takes seconds and page pixels.
#include "ground.h"

#include "../command_line/cli_internal.h"

// which lattice instances grounds have taken, a bit each
static uint32_t s_lattice_allocated;

typedef struct
{
    const GuiPalette *palette;
    double loose;
    GuiBackground *background_fill;
} GroundCtx;

// The color at `loose`, 0 for ordered to 1 for the least ordered: the two lattice colors either side, blended.
EMBED_INLINE GuiColor background_pen_color(const GroundCtx *args)
{
    const double along = args->loose * 2.0;
    const unsigned int low = (along < 1.0) ? 0u : 1u;
    const double part = along - (double)low;
    const GuiColor from = args->palette->lattice[low];
    const GuiColor to = args->palette->lattice[low + 1u];
    // a channel blended between two bytes, rounded, is a byte
    const unsigned int red =
        (unsigned int)((double)GUI_RED_OF(from) + ((double)GUI_RED_OF(to) - (double)GUI_RED_OF(from)) * part + 0.5);
    const unsigned int green = (unsigned int)((double)GUI_GREEN_OF(from) +
                                              ((double)GUI_GREEN_OF(to) - (double)GUI_GREEN_OF(from)) * part + 0.5);
    const unsigned int blue =
        (unsigned int)((double)GUI_BLUE_OF(from) + ((double)GUI_BLUE_OF(to) - (double)GUI_BLUE_OF(from)) * part + 0.5);
    return GUI_RGB(red, green, blue);
}

EMBED_INLINE void background_shade(const GroundCtx *args)
{
    LatticePens pens;
    for (unsigned int level = 0u; level < LATTICE_LEVELS; level += 1u)
    {
        const double loose = (double)level / (LATTICE_LEVELS - 1);
        const double order = 1.0 - loose;
        const GuiColor color = EMBED_CALL(background_pen_color, GroundCtx, .palette = args->palette, .loose = loose);
        // a byte over 255, and the ink between its two bounds, each fit a float
        pens.color[level * 4u] = (float)GUI_RED_OF(color) / 255.0f;
        pens.color[level * 4u + 1u] = (float)GUI_GREEN_OF(color) / 255.0f;
        pens.color[level * 4u + 2u] = (float)GUI_BLUE_OF(color) / 255.0f;
        pens.color[level * 4u + 3u] =
            (float)(GUI_BACKGROUND_FAINTEST * (1.0 - order) + GUI_BACKGROUND_BRIGHTEST * order);
        pens.radius[level] = (float)(GUI_BACKGROUND_RADIUS + GUI_BACKGROUND_RADIUS_SPREAD * order);
    }
    lattice_shaded(&pens, &args->background_fill->shades);
}

int gui_background_init(const GuiBackgroundCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    memor.set(&(MemoriaCfg){.dst = args->background_fill, .val = 0, .bytes = sizeof(*args->background_fill)});
    for (uint32_t index = 0u; index < LATTICE_SLOTS; index += 1u)
    {
        if ((s_lattice_allocated & (1u << index)) == 0u)
        {
            s_lattice_allocated |= 1u << index;
            args->background_fill->lattice_index = index;
            args->background_fill->start_ms = now_ms;
            return 1;
        }
    }
    return 0;
}

void gui_background_free(const GuiBackgroundCfg *args)
{
    memor.set(
        &(MemoriaCfg){.dst = &args->background_fill->raster, .val = 0, .bytes = sizeof(args->background_fill->raster)});
    lattice_given(args->background_fill->lattice_index);
    s_lattice_allocated &= ~(1u << args->background_fill->lattice_index);
    args->background_fill->laid = 0;
}

void gui_background_pause(const GuiBackgroundCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (args->paused && (args->background_fill->paused_ms == 0ull))
    {
        args->background_fill->paused_ms = now_ms;
    }
    else if (!args->paused && (args->background_fill->paused_ms != 0ull))
    {
        args->background_fill->start_ms += now_ms - args->background_fill->paused_ms;
        args->background_fill->paused_ms = 0ull;
    }
}

unsigned long long gui_background_elapsed_ms(const GuiBackgroundClockCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    const unsigned long long stopped =
        (args->background_fill->paused_ms != 0ull) ? args->background_fill->paused_ms : now_ms;
    return stopped - args->background_fill->start_ms;
}

unsigned long long gui_background_next_deadline_ms(const GuiBackgroundClockCfg *args)
{
    if (!args->background_fill->laid || (args->background_fill->paused_ms != 0ull))
    {
        return 0ull;
    }
    return args->background_fill->start_ms + args->background_fill->next_draw_ms;
}

int gui_background_layout(const GuiBackgroundRasterCfg *args)
{
    // the raster is as wide as the framebuffers' pitch; the lattice lies over the window's own width
    args->background_fill->raster = *args->raster;
    args->background_fill->width = args->width;
    args->background_fill->height = args->height;
    args->background_fill->scale = args->scale;
    EMBED_CALL(background_shade, GroundCtx, .background_fill = args->background_fill, .palette = args->palette);
    args->background_fill->laid =
        lattice_lay(args->background_fill->lattice_index, ((double)args->width * GUI_SCALE_ONE) / (double)args->scale,
                    ((double)args->height * GUI_SCALE_ONE) / (double)args->scale) != 0u;
    gui_background_raster.draw(args);
    return args->background_fill->laid;
}

void gui_background_draw(const GuiBackgroundRasterCfg *args)
{
    GuiBackground *const background_fill = args->background_fill;
    GuiRaster *const raster = &background_fill->raster;
    const double seconds =
        (double)EMBED_CALL(gui_background_clock.elapsed_ms, GuiBackgroundClockCfg, .background_fill = background_fill) /
        1000.0;
    // the deep color is blended over what the raster holds, and a raster reused across sizes holds an old drawing
    memor.set(&(MemoriaCfg){.dst = raster->pixels,
                            .val = 0,
                            .bytes = (unsigned long long)raster->width * raster->height * sizeof(GuiColor)});
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){0, 0, (int)raster->width, (int)raster->height}, .color = args->palette->deep);
    // a moment within one loop of the lattice, in whole milliseconds, fits
    background_fill->next_draw_ms = (unsigned long long)(lattice_shown_after(seconds, seconds) * 1000.0);
    background_fill->frames += 1u;
    if (!background_fill->laid)
    {
        return;
    }
    LatticeSlot *const lattice = lattice_slot(background_fill->lattice_index);
    if (!lattice_dots_moved(lattice, ((double)background_fill->width * GUI_SCALE_ONE) / (double)background_fill->scale,
                            ((double)background_fill->height * GUI_SCALE_ONE) / (double)background_fill->scale, seconds,
                            &background_fill->shades))
    {
        return;
    }
    const float scale = (float)background_fill->scale / (float)GUI_SCALE_ONE;
    for (uint32_t point = 0u; point < lattice->count; point += 1u)
    {
        const LatticeDot *const dot = &lattice->dots[point];
        // a dot's color bytes run red, green, blue, ink
        const GuiColor color = GUI_RGBA(dot->color & 0xffu, (dot->color >> 8u) & 0xffu, (dot->color >> 16u) & 0xffu,
                                        (dot->color >> 24u) & 0xffu);
        EMBED_CALL(gui_raster_color.dot, GuiRasterColorCfg, .raster = raster, .x_f32 = dot->across * scale,
                   .y_f32 = dot->down * scale, .radius_f32 = dot->radius * scale, .color = color);
    }
}

int gui_background_tick(const GuiBackgroundRasterCfg *args)
{
    if (!args->background_fill->laid || (args->background_fill->paused_ms != 0ull))
    {
        return 0;
    }
    if (EMBED_CALL(gui_background_clock.elapsed_ms, GuiBackgroundClockCfg, .background_fill = args->background_fill) <
        args->background_fill->next_draw_ms)
    {
        return 0;
    }
    gui_background_raster.draw(args);
    return 1;
}
