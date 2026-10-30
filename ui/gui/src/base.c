// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base.c: the frame's base composed through the map. The map holds a layer a pixel; a row is drawn run by run of one
// layer, a run of a part's ground blended with the layer's one entry, which the compiler lays across the lanes, and a
// run of the sheet or a pane blended pixel by pixel with the entry of the grain's level there, read from the tile.
// An entry at a grain level folds the layer's tint and the grain's white into one blend: the lattice keeps
// (255 - tint) * (255 - grain) / 255 of itself, and takes the tint's color by its alpha and the grain's rest, and
// white by the grain; a layer with no grain is the fill's own blend to the bit. The map's glass sheet lies everywhere
// outside the work; the work's inside is its parts' grounds, its lines' ring and the map's column the lattice; a hole
// a part makes in the work is the sheet, its fillets' squares kept the part's ground for the outline to round; and a
// pane that has slid in is its thinner tint, square-cornered here, its corners the pane's own drawing
#include "base.h"

#include "../../cli/src/command_line/cli_internal.h"

static CellOwner s_owner = MMGR_CELL_OWNER("gui_base");

// the tile tables' words: the sheet's tile and the pane's, each a word a pixel of the grain's tile, in each of the
// three lanes
#define BASE_TILE_WORDS (2u * GUI_GRAIN_TILE * GUI_GRAIN_TILE)

int gui_base_made(GuiBase *base, unsigned int width, unsigned int height)
{
    const unsigned long long bytes = (unsigned long long)width * height;
    memor.set(&(MemoriaCfg){.dst = base, .val = 0, .bytes = sizeof(*base)});
    base->width = width;
    base->height = height;
    base->map = g_cli_cells->persistent_buf_alloc(bytes, &s_owner);
    base->tile_red_blue = g_cli_cells->persistent_buf_alloc(3ull * BASE_TILE_WORDS * sizeof(unsigned int), &s_owner);
    base->run = g_cli_cells->persistent_buf_alloc((unsigned long long)height * GUI_BASE_RUNS * sizeof(GuiBaseRun), &s_owner);
    base->runs = g_cli_cells->persistent_buf_alloc((unsigned long long)height * sizeof(unsigned int), &s_owner);
    if ((base->map == NULL) || (base->tile_red_blue == NULL) || (base->run == NULL) || (base->runs == NULL))
    {
        gui_base_given(base);
        return 0;
    }
    base->tile_green = base->tile_red_blue + BASE_TILE_WORDS;
    base->tile_rest = base->tile_green + BASE_TILE_WORDS;
    memor.set(&(MemoriaCfg){.dst = base->map, .val = 0, .bytes = bytes});
    memor.set(&(MemoriaCfg){.dst = base->runs, .val = 0, .bytes = (unsigned long long)height * sizeof(unsigned int)});
    return 1;
}

void gui_base_given(GuiBase *base)
{
    g_cli_cells->persistent_buf_release(base->map, &s_owner);
    g_cli_cells->persistent_buf_release(base->tile_red_blue, &s_owner);
    g_cli_cells->persistent_buf_release(base->run, &s_owner);
    g_cli_cells->persistent_buf_release(base->runs, &s_owner);
    base->map = NULL;
    base->tile_red_blue = NULL;
    base->tile_green = NULL;
    base->tile_rest = NULL;
    base->run = NULL;
    base->runs = NULL;
    base->width = 0u;
    base->height = 0u;
    base->laid = 0;
}

// the entry of `color` over the lattice with the grain's `grain` of white over that: each lane the color's channel by
// its alpha and the grain's rest, over 255, plus white by the grain, with half of 255 in; the rest the lattice keeps
// the alpha's rest by the grain's rest, over 255; a lane's sum with the lattice's channel by the rest is below 65536
static void base_entry_set(GuiBaseEntry *entry, GuiColor color, unsigned int grain)
{
    const unsigned int alpha = GUI_ALPHA_OF(color);
    const unsigned int grain_rest = 255u - grain;
    const unsigned int white = 255u * grain;
    const unsigned int red = (((GUI_RED_OF(color) * alpha * grain_rest) + 127u) / 255u) + white;
    const unsigned int green = (((GUI_GREEN_OF(color) * alpha * grain_rest) + 127u) / 255u) + white;
    const unsigned int blue = (((GUI_BLUE_OF(color) * alpha * grain_rest) + 127u) / 255u) + white;
    entry->red_blue = (red << 16u) + blue + 0x00800080u;
    entry->green = green + 0x80u;
    entry->rest = (((255u - alpha) * grain_rest) + 127u) / 255u;
}

// `rect` of the map set to `layer`, no further than the map, less the `count` squares of `kept`, which keep what
// they hold
static void base_rect_set(GuiBase *base, GuiRect rect, unsigned int layer, const GuiRect *kept, unsigned int count)
{
    // the map's sides are far inside what an int represents
    const GuiRect on = gui_rect_met(rect, (GuiRect){0, 0, (int)base->width, (int)base->height});
    if (gui_rect_empty(on))
    {
        return;
    }
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        // the rectangle lies on the map: its coordinates are not negative; a layer is a byte
        unsigned char *const row = base->map + ((unsigned int)y * base->width);
        if (count == 0u)
        {
            memor.set(&(MemoriaCfg){.dst = row + (unsigned int)on.x, .val = (int)layer, .bytes = (unsigned long long)on.width});
            continue;
        }
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            int held = 0;
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                held = held || gui_rect_holds(kept[at], x, y);
            }
            if (!held)
            {
                row[(unsigned int)x] = (unsigned char)layer;
            }
        }
    }
}

int gui_base_laid(GuiBase *base, const GuiFrame *frame, const GuiPalette *palette, int controls_steady)
{
    const int same = base->laid && (memor.cmp(&(MemoriaCfg){.src = &base->frame, .other = frame, .bytes = sizeof(*frame)}) == 0) &&
                     (base->editor_ground == palette->editor_ground) && (base->side_ground == palette->side_ground) &&
                     (base->surface == palette->surface) && (base->glass == palette->glass) && (base->controls_steady == controls_steady);
    if (same || (base->map == NULL))
    {
        return 0;
    }
    base->frame = *frame;
    base->editor_ground = palette->editor_ground;
    base->side_ground = palette->side_ground;
    base->surface = palette->surface;
    base->glass = palette->glass;
    base->controls_steady = controls_steady;
    const GuiColor pane = gui_glass_pane_tint(palette->glass);
    base_entry_set(&base->entry[GUI_BASE_CLEAR], 0u, 0u);
    base_entry_set(&base->entry[GUI_BASE_EDITOR], palette->editor_ground, 0u);
    base_entry_set(&base->entry[GUI_BASE_SIDE], palette->side_ground, 0u);
    base_entry_set(&base->entry[GUI_BASE_TABS], palette->surface, 0u);
    for (unsigned int level = 0u; level < GUI_BASE_GRAIN_LEVELS; level += 1u)
    {
        base_entry_set(&base->entry[GUI_BASE_GLASS + level], palette->glass, level);
        base_entry_set(&base->entry[GUI_BASE_PANE + level], pane, level);
    }
    // the sheet's and the pane's entries laid out as the tile is
    const unsigned char *const grain = gui_grain();
    for (unsigned int at = 0u; at < GUI_GRAIN_TILE * GUI_GRAIN_TILE; at += 1u)
    {
        const GuiBaseEntry *const sheet = &base->entry[GUI_BASE_GLASS + grain[at]];
        const GuiBaseEntry *const paned = &base->entry[GUI_BASE_PANE + grain[at]];
        base->tile_red_blue[at] = sheet->red_blue;
        base->tile_green[at] = sheet->green;
        base->tile_rest[at] = sheet->rest;
        base->tile_red_blue[(GUI_GRAIN_TILE * GUI_GRAIN_TILE) + at] = paned->red_blue;
        base->tile_green[(GUI_GRAIN_TILE * GUI_GRAIN_TILE) + at] = paned->green;
        base->tile_rest[(GUI_GRAIN_TILE * GUI_GRAIN_TILE) + at] = paned->rest;
    }
    // the sheet everywhere, the work's inside its parts' grounds, the map's column and the lines' ring the lattice
    // the map's sides are far inside what an int represents
    base_rect_set(base, (GuiRect){0, 0, (int)base->width, (int)base->height}, GUI_BASE_GLASS, NULL, 0u);
    base_rect_set(base, frame->work, GUI_BASE_CLEAR, NULL, 0u);
    base_rect_set(base, frame->side, GUI_BASE_SIDE, NULL, 0u);
    base_rect_set(base, frame->tabs, GUI_BASE_TABS, NULL, 0u);
    base_rect_set(base, frame->editor, GUI_BASE_EDITOR, NULL, 0u);
    // each hole the sheet, its fillets' squares kept
    GuiRect holes[4];
    const unsigned int count = gui_frame_holes(frame, holes);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        GuiRect kept[4];
        const unsigned int fillets = gui_raster_notch_fillets(frame->work, holes[at], frame->radius, kept);
        base_rect_set(base, holes[at], GUI_BASE_GLASS, kept, fillets);
    }
    // the panes that have slid in, each the slat its edge shows, and the controls' pane where it stands
    base_rect_set(base, gui_rect_met(frame->bar_slat, frame->bar), GUI_BASE_PANE, NULL, 0u);
    base_rect_set(base, gui_rect_met(frame->strip_slat, frame->strip), GUI_BASE_PANE, NULL, 0u);
    base_rect_set(base, gui_rect_met(frame->status_slat, frame->status), GUI_BASE_PANE, NULL, 0u);
    if (controls_steady)
    {
        base_rect_set(base, frame->controls_pane, GUI_BASE_PANE, NULL, 0u);
    }
    // each row's runs read from the map, a row of more than the room is read pixel by pixel as it is drawn
    for (unsigned int y = 0u; y < base->height; y += 1u)
    {
        const unsigned char *const map_row = base->map + (y * base->width);
        GuiBaseRun *const row = base->run + (y * GUI_BASE_RUNS);
        unsigned int runs = 0u;
        unsigned int x = 0u;
        while ((x < base->width) && (runs < GUI_BASE_RUNS))
        {
            const unsigned int layer = map_row[x];
            unsigned int run = x + 1u;
            while ((run < base->width) && (map_row[run] == layer))
            {
                run += 1u;
            }
            row[runs] = (GuiBaseRun){x, run - x, layer};
            runs += 1u;
            x = run;
        }
        base->runs[y] = (x < base->width) ? 0u : runs;
    }
    base->laid = 1;
    base->laid_count += 1u;
    return 1;
}

// `lattice` with the entry's layer over it
static GuiColor base_blended(GuiColor lattice, const GuiBaseEntry *entry)
{
    const unsigned int red_blue = entry->red_blue + ((lattice & 0x00ff00ffu) * entry->rest);
    const unsigned int green = entry->green + (((lattice >> 8u) & 0xffu) * entry->rest);
    const unsigned int red_blue_out = ((red_blue + ((red_blue >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu;
    const unsigned int green_out = ((green + (green >> 8u)) >> 8u) & 0xffu;
    return 0xff000000u | red_blue_out | (green_out << 8u);
}

// `count` pixels of the sheet or a pane drawn from the lattice's `under` pixels, or `color` where under is NULL,
// into `out`, the tile tables read straight along from `row`, the tile's row, and `column`, wrapping at the tile's
// side; each pixel's lanes are the lattice's channels by the rest the table gives plus the lanes it gives
static void base_tiled_drawn(const GuiBase *base, GuiColor *out, const GuiColor *under, GuiColor color, unsigned int count,
                             unsigned int tile, unsigned int row, unsigned int column)
{
    unsigned int at = 0u;
    while (at < count)
    {
        const unsigned int chunk = ((count - at) < (GUI_GRAIN_TILE - column)) ? count - at : GUI_GRAIN_TILE - column;
        const unsigned int from = tile + (row * GUI_GRAIN_TILE) + column;
        const unsigned int *const red_blue = base->tile_red_blue + from;
        const unsigned int *const green = base->tile_green + from;
        const unsigned int *const rest = base->tile_rest + from;
        GuiColor *const run = out + at;
        if (under != NULL)
        {
            const GuiColor *const lattice = under + at;
            for (unsigned int px = 0u; px < chunk; px += 1u)
            {
                const unsigned int lanes = red_blue[px] + ((lattice[px] & 0x00ff00ffu) * rest[px]);
                const unsigned int lane = green[px] + (((lattice[px] >> 8u) & 0xffu) * rest[px]);
                run[px] = 0xff000000u | (((lanes + ((lanes >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu) |
                          ((((lane + (lane >> 8u)) >> 8u) & 0xffu) << 8u);
            }
        }
        else
        {
            for (unsigned int px = 0u; px < chunk; px += 1u)
            {
                const unsigned int lanes = red_blue[px] + ((color & 0x00ff00ffu) * rest[px]);
                const unsigned int lane = green[px] + (((color >> 8u) & 0xffu) * rest[px]);
                run[px] = 0xff000000u | (((lanes + ((lanes >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu) |
                          ((((lane + (lane >> 8u)) >> 8u) & 0xffu) << 8u);
            }
        }
        at += chunk;
        column = 0u;
    }
}

// a run of `count` pixels of one layer drawn from the lattice's `under` pixels, or `color` where under is NULL, into
// `out`: nothing over the lattice copied whole, a part's ground blended with its one entry across the run, and the
// sheet or a pane through the tile tables from the tile's `row` and `column`
static void base_run_drawn(const GuiBase *base, GuiColor *out, const GuiColor *under, GuiColor color, unsigned int count,
                           unsigned int layer, unsigned int row, unsigned int column)
{
    if (layer == GUI_BASE_CLEAR)
    {
        if (under != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = out, .src = under, .bytes = (unsigned long long)count * sizeof(GuiColor)});
            return;
        }
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            out[at] = color;
        }
        return;
    }
    if (layer < GUI_BASE_GLASS)
    {
        const GuiBaseEntry *const entry = &base->entry[layer];
        if (under != NULL)
        {
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                out[at] = base_blended(under[at], entry);
            }
            return;
        }
        const GuiColor one = base_blended(color, entry);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            out[at] = one;
        }
        return;
    }
    base_tiled_drawn(base, out, under, color, count, (layer == GUI_BASE_GLASS) ? 0u : GUI_GRAIN_TILE * GUI_GRAIN_TILE, row, column);
}

void gui_base_drawn(const GuiBase *base, GuiRaster *raster, const GuiRaster *ground, GuiColor color)
{
    if ((base->map == NULL) || (raster->pixels == NULL) || (raster->width != base->width) || (raster->height != base->height))
    {
        return;
    }
    const int from_ground = (ground != NULL) && (ground->pixels != NULL) && (ground->width == base->width) && (ground->height == base->height);
    for (unsigned int y = 0u; y < base->height; y += 1u)
    {
        const unsigned char *const map_row = base->map + (y * base->width);
        GuiColor *const out = raster->pixels + (y * base->width);
        const GuiColor *const under = from_ground ? ground->pixels + (y * base->width) : NULL;
        const unsigned int runs = base->runs[y];
        if (runs != 0u)
        {
            const GuiBaseRun *const row = base->run + (y * GUI_BASE_RUNS);
            for (unsigned int at = 0u; at < runs; at += 1u)
            {
                const GuiBaseRun *const one = &row[at];
                base_run_drawn(base, out + one->from, (under != NULL) ? under + one->from : NULL, color, one->count, one->layer,
                               y % GUI_GRAIN_TILE, one->from % GUI_GRAIN_TILE);
            }
            continue;
        }
        unsigned int x = 0u;
        while (x < base->width)
        {
            const unsigned int layer = map_row[x];
            unsigned int run = x + 1u;
            while ((run < base->width) && (map_row[run] == layer))
            {
                run += 1u;
            }
            base_run_drawn(base, out + x, (under != NULL) ? under + x : NULL, color, run - x, layer, y % GUI_GRAIN_TILE, x % GUI_GRAIN_TILE);
            x = run;
        }
    }
}
