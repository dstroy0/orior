// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base.c: the frame's base composed through the map. The map is a list of rectangles, each of one layer, the later
// laid over the earlier; the rectangles' top and bottom edges cut the rows into bands, and in a band the rectangles
// covering it have their left and right edges in order, each span between two edges a run of the layer of the last
// rectangle laid over it. A row is drawn run by run from its band's runs, a run of a part's ground blended with the
// layer's one entry, which the compiler lays across the lanes, and a run of the sheet or a pane blended pixel by
// pixel with the entry of the grain's level there, read straight along the tile tables. An entry at a grain level
// folds the layer's tint and the grain's white into one blend: the lattice saves (255 - tint) * (255 - grain) / 255
// of itself, and takes the tint's color by its alpha and the grain's rest, and white by the grain; a layer with no
// grain is the fill's own blend to the bit. The map's glass sheet lies everywhere outside the work; the work's inside
// is its parts' grounds, its lines' ring and the map's column the lattice; a hole a part makes in the work is the
// sheet, its fillets' squares saved the part's ground for the outline to round; and a pane that has slid in is its
// thinner tint laid from its edges in the frame's words, the row or column an edge falls in at the edge's share of it,
// square-cornered here, its corners the pane's own drawing
#include "base.h"

#include "../command_line/cli_internal.h"

// the tile tables' words: the sheet's tile and the pane's, each a word a pixel of the grain's tile, in each of the
// three lanes
#define BASE_TILE_WORDS (2u * GUI_GRAIN_TILE * GUI_GRAIN_TILE)

int gui_base_init(const GuiBaseWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    memor.set(&(MemoriaCfg){.dst = args->base, .val = 0, .bytes = sizeof(*args->base)});
    args->base->width = width;
    args->base->height = height;
    args->base->tile_red_blue =
        g_cli_guard->persistent_buf_alloc(3ULL * BASE_TILE_WORDS * sizeof(unsigned int), g_gui_base_owner);
    if (args->base->tile_red_blue == NULL)
    {
        return 0;
    }
    args->base->tile_green = args->base->tile_red_blue + BASE_TILE_WORDS;
    args->base->tile_remainder = args->base->tile_green + BASE_TILE_WORDS;
    return 1;
}

void gui_base_resize(const GuiBaseWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    args->base->width = width;
    args->base->height = height;
    args->base->layers = 0u;
    args->base->bands = 0u;
    args->base->laid = 0;
}

typedef struct
{
    GuiBaseEntry *entry;
    GuiColor color;
    unsigned int grain;
    GuiBase *base;
    GuiRect rect;
    unsigned int layer;
    int edge;
    int share;
    GuiEdges edges;
    unsigned int *edges_u32s;
    unsigned int count;
    unsigned int value;
    GuiColor lattice;
    const GuiBaseEntry *entry_read;
    const GuiBase *base_read;
    GuiColor *out;
    const GuiColor *under;
    unsigned int tile;
    unsigned int row;
    unsigned int column;
} BaseCtx;
// the entry of `color` over the lattice with the grain's `grain` of white over that: each lane the color's channel by
// its alpha and the grain's rest, over 255, plus white by the grain, with half of 255 in; the rest the lattice saves
// the alpha's rest by the grain's rest, over 255; a lane's sum with the lattice's channel by the rest is below 65536
EMBED_INLINE void base_entry_set(const BaseCtx *args)
{
    const unsigned int alpha = GUI_ALPHA_OF(args->color);
    const unsigned int grain_remainder = 255u - args->grain;
    const unsigned int white = 255u * args->grain;
    const unsigned int red = (((GUI_RED_OF(args->color) * alpha * grain_remainder) + 127u) / 255u) + white;
    const unsigned int green = (((GUI_GREEN_OF(args->color) * alpha * grain_remainder) + 127u) / 255u) + white;
    const unsigned int blue = (((GUI_BLUE_OF(args->color) * alpha * grain_remainder) + 127u) / 255u) + white;
    args->entry->red_blue = (red << 16u) + blue + 0x00800080u;
    args->entry->green = green + 0x80u;
    args->entry->rest = (((255u - alpha) * grain_remainder) + 127u) / 255u;
}

// the entries from `entry` of `color` at each of the parts a fading part's share stands at but the whole, the first at
// a sixteenth of its alpha and the last at fifteen, with no grain
EMBED_INLINE void base_parts_set(const BaseCtx *args)
{
    for (unsigned int part = 1u; part < GUI_BASE_PARTS; part += 1u)
    {
        // an alpha by a share of sixteenths, rounded, is a byte
        const GuiColor faded = (args->color & 0x00ffffffu) |
                               ((((GUI_ALPHA_OF(args->color) * part) + (GUI_BASE_PARTS / 2u)) / GUI_BASE_PARTS) << 24u);
        EMBED_CALL(base_entry_set, BaseCtx, .entry = &args->entry[part - 1u], .color = faded, .grain = 0u);
    }
}

// the part a fading part's `share` of GUI_FIXED_ONE stands at, GUI_BASE_PARTS whole and 0 none; a share is no more
// than GUI_FIXED_ONE and not negative
EMBED_INLINE unsigned int base_part_of(const BaseCtx *args)
{
    return (args->share >= GUI_FIXED_ONE) ? GUI_BASE_PARTS
                                          : (((unsigned int)args->share * GUI_BASE_PARTS) / GUI_FIXED_ONE);
}

// `rect` of `layer` laid on the map, over everything laid before, no further than the map; a rectangle past the
// capacity for them is not laid
EMBED_INLINE void base_layout(const BaseCtx *args)
{
    GuiRect rect = args->rect;
    // the map's sides are far inside what an int represents
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect,
                                  .other = (GuiRect){0, 0, (int)args->base->width, (int)args->base->height});
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on) || (args->base->layers >= GUI_BASE_LAYERS))
    {
        return;
    }
    args->base->layer[args->base->layers] = (GuiBaseLayer){on, args->layer};
    args->base->layers += 1u;
}

// the pixel an edge in the frame's words falls in, the floor of its whole part
EMBED_INLINE int base_pixel_get(const BaseCtx *args)
{
    return (args->edge >= 0) ? args->edge / GUI_FIXED_ONE : -((-args->edge + (GUI_FIXED_ONE - 1)) / GUI_FIXED_ONE);
}

// the layer of a pane's edge at `share` of GUI_FIXED_ONE of the pixel it falls in: the pane itself at a whole share,
// the part at the nearest sixteenth, or nothing at none
EMBED_INLINE unsigned int base_part_layer(const BaseCtx *args)
{
    // a share of GUI_FIXED_ONE in sixteenths, rounded, is a small count
    const int part = ((args->share * (int)GUI_BASE_PARTS) + (GUI_FIXED_ONE / 2)) / GUI_FIXED_ONE;
    if (part >= (int)GUI_BASE_PARTS)
    {
        return GUI_BASE_PANE;
    }
    return (part <= 0) ? GUI_BASE_CLEAR : GUI_BASE_PANE_PART + (((unsigned int)part - 1u) * GUI_BASE_GRAIN_LEVELS);
}

// a pane laid from its `edges` in the frame's words: the pixels it covers whole its layer, and the row or column an
// edge falls in laid at the edge's share of it, the rows across the pane's whole width and the columns down its whole
// height, a corner the column's
static void base_pane_layout(const BaseCtx *args)
{
    if ((args->edges.right <= args->edges.left) || (args->edges.bottom <= args->edges.top))
    {
        return;
    }
    const int left_pixel = EMBED_CALL(base_pixel_get, BaseCtx, .edge = args->edges.left);
    const int top_pixel = EMBED_CALL(base_pixel_get, BaseCtx, .edge = args->edges.top);
    const int right_pixel = EMBED_CALL(base_pixel_get, BaseCtx, .edge = args->edges.right);
    const int bottom_pixel = EMBED_CALL(base_pixel_get, BaseCtx, .edge = args->edges.bottom);
    const int left_share = GUI_FIXED_ONE - (args->edges.left - (left_pixel * GUI_FIXED_ONE));
    const int top_share = GUI_FIXED_ONE - (args->edges.top - (top_pixel * GUI_FIXED_ONE));
    const int right_share = args->edges.right - (right_pixel * GUI_FIXED_ONE);
    const int bottom_share = args->edges.bottom - (bottom_pixel * GUI_FIXED_ONE);
    const unsigned int left_layer = EMBED_CALL(base_part_layer, BaseCtx, .share = left_share);
    const unsigned int top_layer = EMBED_CALL(base_part_layer, BaseCtx, .share = top_share);
    const unsigned int right_layer = EMBED_CALL(base_part_layer, BaseCtx, .share = right_share);
    const unsigned int bottom_layer = EMBED_CALL(base_part_layer, BaseCtx, .share = bottom_share);
    // the pixels covered whole: from the edge's pixel where the edge covers it whole, else from the next
    const int full_left = (left_layer == GUI_BASE_PANE) ? left_pixel : left_pixel + 1;
    const int full_top = (top_layer == GUI_BASE_PANE) ? top_pixel : top_pixel + 1;
    const int full_right = right_pixel + ((right_layer == GUI_BASE_PANE) ? 1 : 0);
    const int full_bottom = bottom_pixel + ((bottom_layer == GUI_BASE_PANE) ? 1 : 0);
    const int across = right_pixel + ((right_layer == GUI_BASE_CLEAR) ? 0 : 1) - left_pixel;
    const int down = bottom_pixel + ((bottom_layer == GUI_BASE_CLEAR) ? 0 : 1) - top_pixel;
    EMBED_CALL(base_layout, BaseCtx, .base = args->base,
               .rect = (GuiRect){full_left, full_top, full_right - full_left, full_bottom - full_top},
               .layer = GUI_BASE_PANE);
    if ((top_layer != GUI_BASE_PANE) && (top_layer != GUI_BASE_CLEAR))
    {
        EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = (GuiRect){left_pixel, top_pixel, across, 1},
                   .layer = top_layer);
    }
    if ((bottom_layer != GUI_BASE_PANE) && (bottom_layer != GUI_BASE_CLEAR))
    {
        EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = (GuiRect){left_pixel, bottom_pixel, across, 1},
                   .layer = bottom_layer);
    }
    if ((left_layer != GUI_BASE_PANE) && (left_layer != GUI_BASE_CLEAR))
    {
        EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = (GuiRect){left_pixel, top_pixel, 1, down},
                   .layer = left_layer);
    }
    if ((right_layer != GUI_BASE_PANE) && (right_layer != GUI_BASE_CLEAR))
    {
        EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = (GuiRect){right_pixel, top_pixel, 1, down},
                   .layer = right_layer);
    }
}

unsigned int gui_base_layer_at(const GuiBaseCfg *args)
{
    for (unsigned int at = args->base_read->layers; at > 0u; at -= 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->base_read->layer[at - 1u].rect,
                       .x = args->x, .y = args->y))
        {
            return args->base_read->layer[at - 1u].layer;
        }
    }
    return GUI_BASE_CLEAR;
}

// `value` put among `edges`, `count` of them in rising order, where it is not among them: the count after
EMBED_INLINE unsigned int base_edge_append(const BaseCtx *args)
{
    unsigned int at = 0u;
    while ((at < args->count) && (args->edges_u32s[at] < args->value))
    {
        at += 1u;
    }
    if ((at < args->count) && (args->edges_u32s[at] == args->value))
    {
        return args->count;
    }
    for (unsigned int back = args->count; back > at; back -= 1u)
    {
        args->edges_u32s[back] = args->edges_u32s[back - 1u];
    }
    args->edges_u32s[at] = args->value;
    return args->count + 1u;
}

// the bands and their runs read from the rectangles laid: the bands cut at every rectangle's top and bottom, and a
// band's runs cut at the left and right of every rectangle covering it, each run the layer of the last rectangle
// laid over its start, runs of one layer joined
EMBED_INLINE void base_bands_read(const BaseCtx *args)
{
    GuiBase *base = args->base;
    unsigned int rows[GUI_BASE_BANDS + 1u];
    unsigned int cuts = 0u;
    cuts = EMBED_CALL(base_edge_append, BaseCtx, .edges_u32s = rows, .count = cuts, .value = 0u);
    cuts = EMBED_CALL(base_edge_append, BaseCtx, .edges_u32s = rows, .count = cuts, .value = base->height);
    for (unsigned int at = 0u; at < base->layers; at += 1u)
    {
        // a rectangle laid lies on the map: its edges are not negative
        const GuiRect rect = base->layer[at].rect;
        cuts = EMBED_CALL(base_edge_append, BaseCtx, .edges_u32s = rows, .count = cuts, .value = (unsigned int)rect.y);
        cuts = EMBED_CALL(base_edge_append, BaseCtx, .edges_u32s = rows, .count = cuts,
                          .value = (unsigned int)(rect.y + rect.height));
    }
    base->bands = cuts - 1u;
    for (unsigned int band = 0u; band < cuts; band += 1u)
    {
        base->band_from[band] = rows[band];
    }
    unsigned int laid_runs = 0u;
    for (unsigned int band = 0u; band < base->bands; band += 1u)
    {
        base->run_first[band] = laid_runs;
        GuiBaseRun *const band_runs = &base->run[laid_runs];
        // the band's row and width are far inside what an int represents
        const int y = (int)base->band_from[band];
        unsigned int columns[GUI_BASE_RUNS + 1u];
        unsigned int edges = 0u;
        edges = EMBED_CALL(base_edge_append, BaseCtx, .edges_u32s = columns, .count = edges, .value = 0u);
        edges = EMBED_CALL(base_edge_append, BaseCtx, .edges_u32s = columns, .count = edges, .value = base->width);
        for (unsigned int at = 0u; at < base->layers; at += 1u)
        {
            const GuiRect rect = base->layer[at].rect;
            if ((y >= rect.y) && (y < rect.y + rect.height))
            {
                edges = EMBED_CALL(base_edge_append, BaseCtx, .edges_u32s = columns, .count = edges,
                                   .value = (unsigned int)rect.x);
                edges = EMBED_CALL(base_edge_append, BaseCtx, .edges_u32s = columns, .count = edges,
                                   .value = (unsigned int)(rect.x + rect.width));
            }
        }
        unsigned int runs = 0u;
        for (unsigned int edge = 0u; edge + 1u < edges; edge += 1u)
        {
            const unsigned int layer =
                EMBED_CALL(gui_base_table.layer_at, GuiBaseCfg, .base_read = base, .x = (int)columns[edge], .y = y);
            if ((runs > 0u) && (band_runs[runs - 1u].layer == layer))
            {
                band_runs[runs - 1u].count += columns[edge + 1u] - columns[edge];
                continue;
            }
            band_runs[runs] = (GuiBaseRun){columns[edge], columns[edge + 1u] - columns[edge], layer};
            runs += 1u;
        }
        laid_runs += runs;
    }
    base->run_first[base->bands] = laid_runs;
}

void gui_base_paint(const GuiBaseFrameCfg *args)
{
    if ((args->base->tile_red_blue == NULL) ||
        (args->base->painted && (args->base->editor_background == args->palette->editor_background) &&
         (args->base->side_background == args->palette->side_background) &&
         (args->base->surface == args->palette->surface) && (args->base->glass == args->palette->glass)))
    {
        return;
    }
    args->base->painted = 1;
    args->base->laid = 0;
    args->base->editor_background = args->palette->editor_background;
    args->base->side_background = args->palette->side_background;
    args->base->surface = args->palette->surface;
    args->base->glass = args->palette->glass;
    const GuiColor pane = EMBED_CALL(gui_glass.pane_tint, GuiGlassCfg, .glass = args->palette->glass);
    EMBED_CALL(base_entry_set, BaseCtx, .entry = &args->base->entry[GUI_BASE_CLEAR], .color = 0u, .grain = 0u);
    EMBED_CALL(base_entry_set, BaseCtx, .entry = &args->base->entry[GUI_BASE_EDITOR],
               .color = args->palette->editor_background, .grain = 0u);
    EMBED_CALL(base_entry_set, BaseCtx, .entry = &args->base->entry[GUI_BASE_SIDE],
               .color = args->palette->side_background, .grain = 0u);
    EMBED_CALL(base_entry_set, BaseCtx, .entry = &args->base->entry[GUI_BASE_TABS], .color = args->palette->surface,
               .grain = 0u);
    // the tabs row's surface, the side's ground and the editor's at each sixteenth of their alpha, as the claim fades
    // the row, the side's pane slides and the terminal's panel arrives
    EMBED_CALL(base_parts_set, BaseCtx, .entry = &args->base->entry[GUI_BASE_TABS_PART],
               .color = args->palette->surface);
    EMBED_CALL(base_parts_set, BaseCtx, .entry = &args->base->entry[GUI_BASE_SIDE_PART],
               .color = args->palette->side_background);
    EMBED_CALL(base_parts_set, BaseCtx, .entry = &args->base->entry[GUI_BASE_EDITOR_PART],
               .color = args->palette->editor_background);
    for (unsigned int level = 0u; level < GUI_BASE_GRAIN_LEVELS; level += 1u)
    {
        EMBED_CALL(base_entry_set, BaseCtx, .entry = &args->base->entry[GUI_BASE_GLASS + level],
                   .color = args->palette->glass, .grain = level);
        EMBED_CALL(base_entry_set, BaseCtx, .entry = &args->base->entry[GUI_BASE_PANE + level], .color = pane,
                   .grain = level);
    }
    // a pane's edge at each sixteenth: the ink at an alpha between the sheet's and the pane's by the share
    for (unsigned int part = 1u; part < GUI_BASE_PARTS; part += 1u)
    {
        // an alpha between two bytes by a share of sixteenths, rounded, is a byte
        const int sheet_alpha = (int)GUI_ALPHA_OF(args->palette->glass);
        const int pane_alpha = (int)GUI_ALPHA_OF(pane);
        const GuiColor alpha =
            (GuiColor)(sheet_alpha +
                       ((((pane_alpha - sheet_alpha) * (int)part) + ((int)GUI_BASE_PARTS / 2)) / (int)GUI_BASE_PARTS));
        const GuiColor edge = (pane & 0x00ffffffu) | (alpha << 24u);
        for (unsigned int level = 0u; level < GUI_BASE_GRAIN_LEVELS; level += 1u)
        {
            EMBED_CALL(base_entry_set, BaseCtx,
                       .entry = &args->base->entry[GUI_BASE_PANE_PART + ((part - 1u) * GUI_BASE_GRAIN_LEVELS) + level],
                       .color = edge, .grain = level);
        }
    }
    // the sheet's and the pane's entries laid out as the tile is
    const unsigned char *const grain = gui_grain();
    for (unsigned int at = 0u; at < GUI_GRAIN_TILE * GUI_GRAIN_TILE; at += 1u)
    {
        const GuiBaseEntry *const sheet = &args->base->entry[GUI_BASE_GLASS + grain[at]];
        const GuiBaseEntry *const paned = &args->base->entry[GUI_BASE_PANE + grain[at]];
        args->base->tile_red_blue[at] = sheet->red_blue;
        args->base->tile_green[at] = sheet->green;
        args->base->tile_remainder[at] = sheet->rest;
        args->base->tile_red_blue[(GUI_GRAIN_TILE * GUI_GRAIN_TILE) + at] = paned->red_blue;
        args->base->tile_green[(GUI_GRAIN_TILE * GUI_GRAIN_TILE) + at] = paned->green;
        args->base->tile_remainder[(GUI_GRAIN_TILE * GUI_GRAIN_TILE) + at] = paned->rest;
    }
}

int gui_base_layout(const GuiBaseFrameCfg *args)
{
    const GuiFrame *frame = args->frame;
    int controls_steady = args->controls_steady;
    gui_base_frame.paint(args);
    const int same =
        args->base->laid &&
        (memor.cmp(&(MemoriaCfg){.src = &args->base->frame, .other = frame, .bytes = sizeof(*frame)}) == 0) &&
        (args->base->controls_steady == controls_steady);
    if (same || (args->base->tile_red_blue == NULL))
    {
        return 0;
    }
    args->base->frame = *frame;
    args->base->controls_steady = controls_steady;
    // the sheet everywhere, the work's inside its parts' grounds, the map's column and the lines' ring the lattice
    // the map's sides are far inside what an int represents
    args->base->layers = 0u;
    EMBED_CALL(base_layout, BaseCtx, .base = args->base,
               .rect = (GuiRect){0, 0, (int)args->base->width, (int)args->base->height}, .layer = GUI_BASE_GLASS);
    EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = frame->work, .layer = GUI_BASE_CLEAR);
    // the side's ground where its pane stands as it slides, inside the work's inside, at the share of it shown by
    // sixteenths, the side it leaves the lattice
    const unsigned int side_part = EMBED_CALL(base_part_of, BaseCtx, .share = frame->side_share);
    if (side_part != 0u)
    {
        EMBED_CALL(base_layout, BaseCtx, .base = args->base,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->side_pane,
                                      .other = frame->inside),
                   .layer = (side_part >= GUI_BASE_PARTS) ? GUI_BASE_SIDE : (GUI_BASE_SIDE_PART + side_part - 1u));
    }
    // the tabs row's surface at the share of it shown, by sixteenths, none of it leaving the lattice
    const unsigned int tabs_part = EMBED_CALL(base_part_of, BaseCtx, .share = frame->tabs_share);
    if (tabs_part != 0u)
    {
        EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = frame->tabs,
                   .layer = (tabs_part >= GUI_BASE_PARTS) ? GUI_BASE_TABS : (GUI_BASE_TABS_PART + tabs_part - 1u));
    }
    EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = frame->editor, .layer = GUI_BASE_EDITOR);
    // the terminal's panel on the ink at half as the side is, its views on the editor's ground, each where it stands as
    // it arrives, inside the work's inside, at the part of its opacity reached
    const unsigned int terminal_part = EMBED_CALL(base_part_of, BaseCtx, .share = frame->terminal_share);
    if (terminal_part != 0u)
    {
        EMBED_CALL(base_layout, BaseCtx, .base = args->base,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->terminal_drawn,
                                      .other = frame->inside),
                   .layer =
                       (terminal_part >= GUI_BASE_PARTS) ? GUI_BASE_SIDE : (GUI_BASE_SIDE_PART + terminal_part - 1u));
        EMBED_CALL(base_layout, BaseCtx, .base = args->base,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = frame->terminal_views_drawn, .other = frame->inside),
                   .layer = (terminal_part >= GUI_BASE_PARTS) ? GUI_BASE_EDITOR
                                                              : (GUI_BASE_EDITOR_PART + terminal_part - 1u));
    }
    // each hole the sheet, its fillets' squares saved what lay there
    GuiRect holes[4];
    const unsigned int count = EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = frame, .holes = holes);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        GuiRect saved[4];
        unsigned int under[4];
        const unsigned int fillets =
            EMBED_CALL(gui_raster_radius.notch_fillets, GuiRasterRadiusCfg, .work = frame->work, .notch = holes[at],
                       .radius = frame->radius, .squares = saved);
        for (unsigned int fillet = 0u; fillet < fillets; fillet += 1u)
        {
            under[fillet] = EMBED_CALL(gui_base_table.layer_at, GuiBaseCfg, .base_read = args->base,
                                       .x = saved[fillet].x, .y = saved[fillet].y);
        }
        EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = holes[at], .layer = GUI_BASE_GLASS);
        for (unsigned int fillet = 0u; fillet < fillets; fillet += 1u)
        {
            EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = saved[fillet], .layer = under[fillet]);
        }
    }
    // the panes that have slid in, each the slat its edge shows, and the controls' pane where it stands, each laid
    // from its edges in the frame's words; the controls' pane coming down at its share's part, the tint between the
    // sheet's and a pane's
    EMBED_CALL(base_pane_layout, BaseCtx, .base = args->base, .edges = frame->bar_edges);
    EMBED_CALL(base_pane_layout, BaseCtx, .base = args->base, .edges = frame->strip_edges);
    EMBED_CALL(base_pane_layout, BaseCtx, .base = args->base, .edges = frame->status_edges);
    if (controls_steady)
    {
        EMBED_CALL(base_pane_layout, BaseCtx, .base = args->base, .edges = frame->controls_edges);
    }
    else if (frame->controls_pane_share > 0)
    {
        EMBED_CALL(base_layout, BaseCtx, .base = args->base, .rect = frame->controls_pane,
                   .layer = EMBED_CALL(base_part_layer, BaseCtx, .share = frame->controls_pane_share));
    }
    EMBED_CALL(base_bands_read, BaseCtx, .base = args->base);
    args->base->laid = 1;
    args->base->laid_count += 1u;
    return 1;
}

// `lattice` with the entry's layer over it
EMBED_INLINE GuiColor base_blend(const BaseCtx *args)
{
    const unsigned int red_blue = args->entry_read->red_blue + ((args->lattice & 0x00ff00ffu) * args->entry_read->rest);
    const unsigned int green = args->entry_read->green + (((args->lattice >> 8u) & 0xffu) * args->entry_read->rest);
    const unsigned int red_blue_out = ((red_blue + ((red_blue >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu;
    const unsigned int green_out = ((green + (green >> 8u)) >> 8u) & 0xffu;
    return 0xff000000u | red_blue_out | (green_out << 8u);
}

// `count` pixels of the sheet or a pane drawn from the lattice's `under` pixels, or `color` where under is NULL,
// into `out`, the tile tables read straight along from `row`, the tile's row, and `column`, wrapping at the tile's
// side; each pixel's lanes are the lattice's channels by the rest the table gives plus the lanes it gives
EMBED_INLINE void base_tiled_draw(const BaseCtx *args)
{
    unsigned int column = args->column;
    unsigned int at = 0u;
    while (at < args->count)
    {
        const unsigned int chunk =
            ((args->count - at) < (GUI_GRAIN_TILE - column)) ? args->count - at : GUI_GRAIN_TILE - column;
        const unsigned int from = args->tile + (args->row * GUI_GRAIN_TILE) + column;
        const unsigned int *const red_blue = args->base_read->tile_red_blue + from;
        const unsigned int *const green = args->base_read->tile_green + from;
        const unsigned int *const rest = args->base_read->tile_remainder + from;
        GuiColor *const run = args->out + at;
        if (args->under != NULL)
        {
            const GuiColor *const lattice = args->under + at;
            for (unsigned int px = 0u; px < chunk; px += 1u)
            {
                const unsigned int lanes = red_blue[px] + ((lattice[px] & 0x00ff00ffu) * rest[px]);
                const unsigned int lane_at = green[px] + (((lattice[px] >> 8u) & 0xffu) * rest[px]);
                run[px] = 0xff000000u | (((lanes + ((lanes >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu) |
                          ((((lane_at + (lane_at >> 8u)) >> 8u) & 0xffu) << 8u);
            }
        }
        else
        {
            for (unsigned int px = 0u; px < chunk; px += 1u)
            {
                const unsigned int lanes = red_blue[px] + ((args->color & 0x00ff00ffu) * rest[px]);
                const unsigned int lane_at = green[px] + (((args->color >> 8u) & 0xffu) * rest[px]);
                run[px] = 0xff000000u | (((lanes + ((lanes >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu) |
                          ((((lane_at + (lane_at >> 8u)) >> 8u) & 0xffu) << 8u);
            }
        }
        at += chunk;
        column = 0u;
    }
}

// a run of `count` pixels of one layer drawn from the lattice's `under` pixels, or `color` where under is NULL, into
// `out`: nothing over the lattice copied whole, a part's ground blended with its one entry across the run, and the
// sheet or a pane through the tile tables from the tile's `row` and `column`
EMBED_INLINE void base_run_draw(const BaseCtx *args)
{
    unsigned int column = args->column;
    if (args->layer == GUI_BASE_CLEAR)
    {
        if (args->under != NULL)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->out, .src = args->under, .bytes = (unsigned long long)args->count * sizeof(GuiColor)});
            return;
        }
        for (unsigned int at = 0u; at < args->count; at += 1u)
        {
            args->out[at] = args->color;
        }
        return;
    }
    if (args->layer < GUI_BASE_GLASS)
    {
        const GuiBaseEntry *const entry = &args->base_read->entry[args->layer];
        if (args->under != NULL)
        {
            for (unsigned int at = 0u; at < args->count; at += 1u)
            {
                args->out[at] = EMBED_CALL(base_blend, BaseCtx, .lattice = args->under[at], .entry_read = entry);
            }
            return;
        }
        const GuiColor one = EMBED_CALL(base_blend, BaseCtx, .lattice = args->color, .entry_read = entry);
        for (unsigned int at = 0u; at < args->count; at += 1u)
        {
            args->out[at] = one;
        }
        return;
    }
    if (args->layer >= GUI_BASE_PANE_PART)
    {
        // a pane's edge, a row or a column of pixels: each pixel's entry its part at the grain's level there, read
        // from the tile
        const unsigned char *const tile_row = gui_grain() + (args->row * GUI_GRAIN_TILE);
        for (unsigned int at = 0u; at < args->count; at += 1u)
        {
            args->out[at] =
                EMBED_CALL(base_blend, BaseCtx, .lattice = (args->under != NULL) ? args->under[at] : args->color,
                           .entry_read = &args->base_read->entry[args->layer + tile_row[column]]);
            column = (column + 1u == GUI_GRAIN_TILE) ? 0u : column + 1u;
        }
        return;
    }
    EMBED_CALL(base_tiled_draw, BaseCtx, .base_read = args->base_read, .out = args->out, .under = args->under,
               .color = args->color, .count = args->count,
               .tile = (args->layer == GUI_BASE_GLASS) ? 0u : GUI_GRAIN_TILE * GUI_GRAIN_TILE, .row = args->row,
               .column = column);
}

void gui_base_draw(const GuiBaseRasterCfg *args)
{
    if (args->base_read->laid)
    {
        const GuiBaseMap own = {args->base_read->bands, args->base_read->band_from, args->base_read->run_first,
                                args->base_read->run};
        EMBED_CALL(gui_base_raster.map_draw, GuiBaseRasterCfg, .base_read = args->base_read, .map = &own,
                   .raster = args->raster, .background_fill = args->background_fill, .color = args->color);
    }
}

void gui_base_map_draw(const GuiBaseRasterCfg *args)
{
    if ((args->raster->pixels == NULL) || (args->raster->width != args->base_read->width) ||
        (args->raster->height != args->base_read->height))
    {
        return;
    }
    const int from_background = (args->background_fill != NULL) && (args->background_fill->pixels != NULL) &&
                                (args->background_fill->width == args->base_read->width) &&
                                (args->background_fill->height == args->base_read->height);
    for (unsigned int band = 0u; band < args->map->bands; band += 1u)
    {
        const GuiBaseRun *const runs = &args->map->run[args->map->run_first[band]];
        const unsigned int count = args->map->run_first[band + 1u] - args->map->run_first[band];
        for (unsigned int y = args->map->band_from[band]; y < args->map->band_from[band + 1u]; y += 1u)
        {
            GuiColor *const out = args->raster->pixels + (y * args->base_read->width);
            const GuiColor *const under =
                from_background ? args->background_fill->pixels + (y * args->base_read->width) : NULL;
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                const GuiBaseRun *const one = &runs[at];
                EMBED_CALL(base_run_draw, BaseCtx, .base_read = args->base_read, .out = out + one->from,
                           .under = (under != NULL) ? under + one->from : NULL, .color = args->color,
                           .count = one->count, .layer = one->layer, .row = y % GUI_GRAIN_TILE,
                           .column = one->from % GUI_GRAIN_TILE);
            }
        }
    }
}
