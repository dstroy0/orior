// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base.c: the frame's base composed through the map. The map is a list of rectangles, each of one layer, the later
// laid over the earlier; the rectangles' top and bottom edges cut the rows into bands, and in a band the rectangles
// covering it have their left and right edges in order, each span between two edges a run of the layer of the last
// rectangle laid over it. A row is drawn run by run from its band's runs, a run of a part's ground blended with the
// layer's one entry, which the compiler lays across the lanes, and a run of the sheet or a pane blended pixel by
// pixel with the entry of the grain's level there, read straight along the tile tables. An entry at a grain level
// folds the layer's tint and the grain's white into one blend: the lattice keeps (255 - tint) * (255 - grain) / 255
// of itself, and takes the tint's color by its alpha and the grain's rest, and white by the grain; a layer with no
// grain is the fill's own blend to the bit. The map's glass sheet lies everywhere outside the work; the work's inside
// is its parts' grounds, its lines' ring and the map's column the lattice; a hole a part makes in the work is the
// sheet, its fillets' squares kept the part's ground for the outline to round; and a pane that has slid in is its
// thinner tint, square-cornered here, its corners the pane's own drawing
#include "base.h"

#include "../../cli/src/command_line/cli_internal.h"

static CellOwner s_owner = MMGR_CELL_OWNER("gui_base");

// the tile tables' words: the sheet's tile and the pane's, each a word a pixel of the grain's tile, in each of the
// three lanes
#define BASE_TILE_WORDS (2u * GUI_GRAIN_TILE * GUI_GRAIN_TILE)

int gui_base_made(GuiBase *base, unsigned int width, unsigned int height)
{
    memor.set(&(MemoriaCfg){.dst = base, .val = 0, .bytes = sizeof(*base)});
    base->width = width;
    base->height = height;
    base->tile_red_blue = g_cli_cells->persistent_buf_alloc(3ull * BASE_TILE_WORDS * sizeof(unsigned int), &s_owner);
    if (base->tile_red_blue == NULL)
    {
        return 0;
    }
    base->tile_green = base->tile_red_blue + BASE_TILE_WORDS;
    base->tile_rest = base->tile_green + BASE_TILE_WORDS;
    return 1;
}

void gui_base_given(GuiBase *base)
{
    g_cli_cells->persistent_buf_release(base->tile_red_blue, &s_owner);
    base->tile_red_blue = NULL;
    base->tile_green = NULL;
    base->tile_rest = NULL;
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

// `rect` of `layer` laid on the map, over everything laid before, no further than the map; a rectangle past the
// room for them is not laid
static void base_laid(GuiBase *base, GuiRect rect, unsigned int layer)
{
    // the map's sides are far inside what an int represents
    const GuiRect on = gui_rect_met(rect, (GuiRect){0, 0, (int)base->width, (int)base->height});
    if (gui_rect_empty(on) || (base->layers >= GUI_BASE_LAYERS))
    {
        return;
    }
    base->layer[base->layers] = (GuiBaseLayer){on, layer};
    base->layers += 1u;
}

unsigned int gui_base_layer_at(const GuiBase *base, int x, int y)
{
    for (unsigned int at = base->layers; at > 0u; at -= 1u)
    {
        if (gui_rect_holds(base->layer[at - 1u].rect, x, y))
        {
            return base->layer[at - 1u].layer;
        }
    }
    return GUI_BASE_CLEAR;
}

// `value` put among `edges`, `count` of them in rising order, where it is not among them: the count after
static unsigned int base_edge_put(unsigned int *edges, unsigned int count, unsigned int value)
{
    unsigned int at = 0u;
    while ((at < count) && (edges[at] < value))
    {
        at += 1u;
    }
    if ((at < count) && (edges[at] == value))
    {
        return count;
    }
    for (unsigned int back = count; back > at; back -= 1u)
    {
        edges[back] = edges[back - 1u];
    }
    edges[at] = value;
    return count + 1u;
}

// the bands and their runs read from the rectangles laid: the bands cut at every rectangle's top and bottom, and a
// band's runs cut at the left and right of every rectangle covering it, each run the layer of the last rectangle
// laid over its start, runs of one layer joined
static void base_bands_read(GuiBase *base)
{
    unsigned int rows[GUI_BASE_BANDS + 1u];
    unsigned int cuts = 0u;
    cuts = base_edge_put(rows, cuts, 0u);
    cuts = base_edge_put(rows, cuts, base->height);
    for (unsigned int at = 0u; at < base->layers; at += 1u)
    {
        // a rectangle laid lies on the map: its edges are not negative
        const GuiRect rect = base->layer[at].rect;
        cuts = base_edge_put(rows, cuts, (unsigned int)rect.y);
        cuts = base_edge_put(rows, cuts, (unsigned int)(rect.y + rect.height));
    }
    base->bands = cuts - 1u;
    for (unsigned int band = 0u; band < cuts; band += 1u)
    {
        base->band_from[band] = rows[band];
    }
    for (unsigned int band = 0u; band < base->bands; band += 1u)
    {
        // the band's row and width are far inside what an int represents
        const int y = (int)base->band_from[band];
        unsigned int columns[GUI_BASE_RUNS + 1u];
        unsigned int edges = 0u;
        edges = base_edge_put(columns, edges, 0u);
        edges = base_edge_put(columns, edges, base->width);
        for (unsigned int at = 0u; at < base->layers; at += 1u)
        {
            const GuiRect rect = base->layer[at].rect;
            if ((y >= rect.y) && (y < rect.y + rect.height))
            {
                edges = base_edge_put(columns, edges, (unsigned int)rect.x);
                edges = base_edge_put(columns, edges, (unsigned int)(rect.x + rect.width));
            }
        }
        unsigned int runs = 0u;
        for (unsigned int edge = 0u; edge + 1u < edges; edge += 1u)
        {
            const unsigned int layer = gui_base_layer_at(base, (int)columns[edge], y);
            if ((runs > 0u) && (base->run[band][runs - 1u].layer == layer))
            {
                base->run[band][runs - 1u].count += columns[edge + 1u] - columns[edge];
                continue;
            }
            base->run[band][runs] = (GuiBaseRun){columns[edge], columns[edge + 1u] - columns[edge], layer};
            runs += 1u;
        }
        base->runs[band] = runs;
    }
}

int gui_base_laid(GuiBase *base, const GuiFrame *frame, const GuiPalette *palette, int controls_steady)
{
    const int same = base->laid && (memor.cmp(&(MemoriaCfg){.src = &base->frame, .other = frame, .bytes = sizeof(*frame)}) == 0) &&
                     (base->editor_ground == palette->editor_ground) && (base->side_ground == palette->side_ground) &&
                     (base->surface == palette->surface) && (base->glass == palette->glass) && (base->controls_steady == controls_steady);
    if (same || (base->tile_red_blue == NULL))
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
    base->layers = 0u;
    base_laid(base, (GuiRect){0, 0, (int)base->width, (int)base->height}, GUI_BASE_GLASS);
    base_laid(base, frame->work, GUI_BASE_CLEAR);
    base_laid(base, frame->side, GUI_BASE_SIDE);
    base_laid(base, frame->tabs, GUI_BASE_TABS);
    base_laid(base, frame->editor, GUI_BASE_EDITOR);
    // each hole the sheet, its fillets' squares kept what lay there
    GuiRect holes[4];
    const unsigned int count = gui_frame_holes(frame, holes);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        GuiRect kept[4];
        unsigned int under[4];
        const unsigned int fillets = gui_raster_notch_fillets(frame->work, holes[at], frame->radius, kept);
        for (unsigned int fillet = 0u; fillet < fillets; fillet += 1u)
        {
            under[fillet] = gui_base_layer_at(base, kept[fillet].x, kept[fillet].y);
        }
        base_laid(base, holes[at], GUI_BASE_GLASS);
        for (unsigned int fillet = 0u; fillet < fillets; fillet += 1u)
        {
            base_laid(base, kept[fillet], under[fillet]);
        }
    }
    // the panes that have slid in, each the slat its edge shows, and the controls' pane where it stands
    base_laid(base, gui_rect_met(frame->bar_slat, frame->bar), GUI_BASE_PANE);
    base_laid(base, gui_rect_met(frame->strip_slat, frame->strip), GUI_BASE_PANE);
    base_laid(base, gui_rect_met(frame->status_slat, frame->status), GUI_BASE_PANE);
    if (controls_steady)
    {
        base_laid(base, frame->controls_pane, GUI_BASE_PANE);
    }
    base_bands_read(base);
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
    if (!base->laid || (raster->pixels == NULL) || (raster->width != base->width) || (raster->height != base->height))
    {
        return;
    }
    const int from_ground = (ground != NULL) && (ground->pixels != NULL) && (ground->width == base->width) && (ground->height == base->height);
    for (unsigned int band = 0u; band < base->bands; band += 1u)
    {
        const GuiBaseRun *const runs = base->run[band];
        const unsigned int count = base->runs[band];
        for (unsigned int y = base->band_from[band]; y < base->band_from[band + 1u]; y += 1u)
        {
            GuiColor *const out = raster->pixels + (y * base->width);
            const GuiColor *const under = from_ground ? ground->pixels + (y * base->width) : NULL;
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                const GuiBaseRun *const one = &runs[at];
                base_run_drawn(base, out + one->from, (under != NULL) ? under + one->from : NULL, color, one->count, one->layer,
                               y % GUI_GRAIN_TILE, one->from % GUI_GRAIN_TILE);
            }
        }
    }
}
