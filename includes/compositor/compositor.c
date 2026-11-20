// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compositor.c: the compositor's block, its surfaces and layers, the commands they record, the tiles they mark, and
// the compose that makes the dirty tiles bottom layer to top. The blend puts a pixel multiplied by its alpha over
// another: four pixels in a vector of bytes where the target has one, else one at a time, the same pixels both ways.
// A glyph run draws each glyph's coverage out of the atlas, lifted through the contrast band its ground gives
#include "atlas_internal.h"

#include "embed_compiler_directives.h"

// whether the build has the vector blend: the target has vectors and the compiler has the builtins that turn and widen
// them
#if EMBED_HAS_VECTOR_SIZE && EMBED_HAS_BUILTIN(__builtin_shufflevector) && EMBED_HAS_BUILTIN(__builtin_convertvector)
#define COMPOSITOR_VECTOR 1
#else
#define COMPOSITOR_VECTOR 0
#endif

// the alignment of the parts of a block, in bytes: a cache line
#define COMPOSITOR_ALIGN 64ULL
#define COMPOSITOR_ALIGNED(bytes_) ((((embed_u64)(bytes_)) + COMPOSITOR_ALIGN - 1ULL) & ~(COMPOSITOR_ALIGN - 1ULL))
// the pixels of a tile
#define COMPOSITOR_TILE_PIXELS (GUI_COMPOSITOR_TILE * GUI_COMPOSITOR_TILE)

typedef struct
{
    GuiCompositor *compositor;
    GuiCompositorSurface *surface;
    const GuiCompositorSurface *surface_read;
    GuiCompositorLayer *layer;
    const GuiCompositorLimitsCfg *limits;
    const GuiCompositorCommand *command;
    const GuiCompositorCommand *other;
    GuiCompositorCommand *range;
    embed_u64 *bits;
    embed_u32 *target;
    embed_u32 stride;
    embed_u32 color;
    embed_u32 count;
    embed_u32 cell;
    embed_i32 left;
    embed_i32 top;
    embed_i32 right;
    embed_i32 bottom;
    GuiCompositorRect rect;
} CompositorCtx;

// the cells of a surface's grid at the widest and tallest the limits allow, and the words of one set of its bits
EMBED_INLINE embed_u32 compositor_cells_max(const CompositorCtx *args)
{
    const embed_u32 columns = (args->limits->width_max + GUI_COMPOSITOR_TILE - 1u) >> GUI_COMPOSITOR_TILE_SHIFT;
    const embed_u32 rows = (args->limits->height_max + GUI_COMPOSITOR_TILE - 1u) >> GUI_COMPOSITOR_TILE_SHIFT;
    return columns * rows;
}

// the bytes of one surface's part of the block: its bits, its ring, its pool indices, its commands and its runs'
// glyphs, each part aligned
EMBED_INLINE embed_u64 compositor_surface_bytes(const CompositorCtx *args)
{
    const embed_u64 cells = EMBED_CALL(compositor_cells_max, CompositorCtx, .limits = args->limits);
    const embed_u64 words = (cells + 63ULL) >> 6u;
    return COMPOSITOR_ALIGNED(words * (GUI_COMPOSITOR_LAYERS + 1ULL) * sizeof(embed_u64)) +
           COMPOSITOR_ALIGNED((embed_u64)args->limits->ring_max * sizeof(GuiCompositorRect)) +
           COMPOSITOR_ALIGNED(cells * GUI_COMPOSITOR_LAYERS * sizeof(embed_u16)) +
           COMPOSITOR_ALIGNED((embed_u64)args->limits->commands_max * GUI_COMPOSITOR_LAYERS * 2ULL *
                              sizeof(GuiCompositorCommand)) +
           COMPOSITOR_ALIGNED((embed_u64)args->limits->items_max * GUI_COMPOSITOR_LAYERS * 2ULL *
                              sizeof(GuiCompositorGlyph));
}

embed_u64 gui_compositor_bytes(const GuiCompositorLimitsCfg *args)
{
    const embed_u64 atlas = gui_compositor_atlas_bytes(args);
    if ((args->width_max == 0u) || (args->height_max == 0u) || (args->commands_max == 0u) || (args->ring_max == 0u) ||
        (args->tiles_max == 0u) || (args->tiles_max >= GUI_COMPOSITOR_NONE) || (atlas == 0ULL))
    {
        return 0ULL;
    }
    return COMPOSITOR_ALIGN + COMPOSITOR_ALIGNED((embed_u64)args->tiles_max * COMPOSITOR_TILE_PIXELS * sizeof(embed_u32)) +
           COMPOSITOR_ALIGNED((embed_u64)args->tiles_max * sizeof(embed_u16)) + atlas +
           (GUI_COMPOSITOR_SURFACES * EMBED_CALL(compositor_surface_bytes, CompositorCtx, .limits = args));
}

int gui_compositor_init(const GuiCompositorInitCfg *args)
{
    GuiCompositor *const compositor = args->compositor;
    const embed_u64 needed = gui_compositor_bytes(&args->limits);
    if ((needed == 0ULL) || (args->block == NULL) || (args->bytes < needed))
    {
        return 0;
    }
    __builtin_memset(compositor, 0, sizeof(*compositor));
    compositor->limits = args->limits;
    const GuiCompositorLimitsCfg *const limits = &compositor->limits;
    // the block's first byte on a cache line: the block is the caller's, its address a plain number here
    unsigned char *place = (unsigned char *)(void *)(((embed_word)args->block + COMPOSITOR_ALIGN - 1ULL) &
                                                     ~(embed_word)(COMPOSITOR_ALIGN - 1ULL));
    compositor->pool = (embed_u32 *)(void *)place;
    place += COMPOSITOR_ALIGNED((embed_u64)limits->tiles_max * COMPOSITOR_TILE_PIXELS * sizeof(embed_u32));
    compositor->free_tiles = (embed_u16 *)(void *)place;
    place += COMPOSITOR_ALIGNED((embed_u64)limits->tiles_max * sizeof(embed_u16));
    for (embed_u32 at = 0u; at < limits->tiles_max; at += 1u)
    {
        // a tile's number is below GUI_COMPOSITOR_NONE, which the limits keep inside 16 bits
        compositor->free_tiles[at] = (embed_u16)(limits->tiles_max - 1u - at);
    }
    compositor->free_count = limits->tiles_max;
    place = gui_compositor_atlas_place(compositor, place);
    const embed_u32 cells = EMBED_CALL(compositor_cells_max, CompositorCtx, .limits = limits);
    const embed_u32 words = (cells + 63u) >> 6u;
    for (embed_u32 at = 0u; at < GUI_COMPOSITOR_SURFACES; at += 1u)
    {
        GuiCompositorSurface *const surface = &compositor->surface[at];
        embed_u64 *const bits = (embed_u64 *)(void *)place;
        place += COMPOSITOR_ALIGNED((embed_u64)words * (GUI_COMPOSITOR_LAYERS + 1ULL) * sizeof(embed_u64));
        surface->dirty = bits;
        surface->ring = (GuiCompositorRect *)(void *)place;
        place += COMPOSITOR_ALIGNED((embed_u64)limits->ring_max * sizeof(GuiCompositorRect));
        embed_u16 *const slots = (embed_u16 *)(void *)place;
        place += COMPOSITOR_ALIGNED((embed_u64)cells * GUI_COMPOSITOR_LAYERS * sizeof(embed_u16));
        GuiCompositorCommand *const commands = (GuiCompositorCommand *)(void *)place;
        place += COMPOSITOR_ALIGNED((embed_u64)limits->commands_max * GUI_COMPOSITOR_LAYERS * 2ULL *
                                    sizeof(GuiCompositorCommand));
        GuiCompositorGlyph *const glyphs = (GuiCompositorGlyph *)(void *)place;
        place += COMPOSITOR_ALIGNED((embed_u64)limits->items_max * GUI_COMPOSITOR_LAYERS * 2ULL *
                                    sizeof(GuiCompositorGlyph));
        for (embed_u32 layer = 0u; layer < GUI_COMPOSITOR_LAYERS; layer += 1u)
        {
            surface->layer[layer].dirty = bits + ((embed_u64)words * (layer + 1u));
            surface->layer[layer].slots = slots + ((embed_u64)cells * layer);
            surface->layer[layer].shown = commands + ((embed_u64)limits->commands_max * layer * 2u);
            surface->layer[layer].recorded = surface->layer[layer].shown + limits->commands_max;
            surface->layer[layer].glyphs_shown = glyphs + ((embed_u64)limits->items_max * layer * 2u);
            surface->layer[layer].glyphs_recorded = surface->layer[layer].glyphs_shown + limits->items_max;
        }
    }
    return 1;
}

int gui_compositor_image(const GuiCompositorImageCfg *args)
{
    if (args->image >= GUI_COMPOSITOR_IMAGES)
    {
        return 0;
    }
    GuiCompositorImage *const image = &args->compositor->image[args->image];
    image->pixels = args->pixels;
    image->width = (args->pixels != NULL) ? args->width : 0u;
    image->height = (args->pixels != NULL) ? args->height : 0u;
    return 1;
}

// the bit of the cell `cell` set in `bits`
EMBED_INLINE void compositor_bit_set(const CompositorCtx *args)
{
    args->bits[args->cell >> 6u] |= 1ULL << (args->cell & 63u);
}

// every tile of the command's range set in `bits`
EMBED_INLINE void compositor_command_mark(const CompositorCtx *args)
{
    const GuiCompositorCommand *const command = args->command;
    for (embed_u32 row = command->tile_top; row <= command->tile_bottom; row += 1u)
    {
        for (embed_u32 column = command->tile_left; column <= command->tile_right; column += 1u)
        {
            EMBED_CALL(compositor_bit_set, CompositorCtx, .bits = args->bits,
                       .cell = (row * args->surface_read->columns) + column);
        }
    }
}

// the tiles that the pixels `left` to `right` across and `top` to `bottom` down cover, both ends in. The call cuts
// them to the surface and puts them in the command. The result is 1, or 0 where none of them is on the surface
EMBED_INLINE int compositor_range(const CompositorCtx *args)
{
    const GuiCompositorSurface *const surface = args->surface_read;
    // a surface's sides are below the limits, far inside an int
    if ((args->right < 0) || (args->bottom < 0) || (args->left >= (embed_i32)surface->width) ||
        (args->top >= (embed_i32)surface->height) || (args->right < args->left) || (args->bottom < args->top))
    {
        return 0;
    }
    const embed_i32 left = (args->left < 0) ? 0 : args->left;
    const embed_i32 top = (args->top < 0) ? 0 : args->top;
    const embed_i32 right = (args->right >= (embed_i32)surface->width) ? ((embed_i32)surface->width - 1) : args->right;
    const embed_i32 bottom =
        (args->bottom >= (embed_i32)surface->height) ? ((embed_i32)surface->height - 1) : args->bottom;
    GuiCompositorCommand *const command = args->range;
    // the code cut the pixels to the surface. They are not negative, and the number of a tile fits 16 bits
    command->tile_left = (embed_u16)((embed_u32)left >> GUI_COMPOSITOR_TILE_SHIFT);
    command->tile_top = (embed_u16)((embed_u32)top >> GUI_COMPOSITOR_TILE_SHIFT);
    command->tile_right = (embed_u16)((embed_u32)right >> GUI_COMPOSITOR_TILE_SHIFT);
    command->tile_bottom = (embed_u16)((embed_u32)bottom >> GUI_COMPOSITOR_TILE_SHIFT);
    return 1;
}

// the layer `layer` of `surface`, NULL where either is past its table or the surface is not open
static GuiCompositorLayer *compositor_layer(GuiCompositor *compositor, embed_u32 surface, embed_u32 layer)
{
    if ((surface >= GUI_COMPOSITOR_SURFACES) || (layer >= GUI_COMPOSITOR_LAYERS) || !compositor->surface[surface].open)
    {
        return NULL;
    }
    return &compositor->surface[surface].layer[layer];
}

// The command, put last on the recorded list of the layer, with its tiles found from its place and size in 16.16. The
// result is 1, or 0 where the layer does not record or its list is full. A command off the surface records nothing
// and is 1
static int compositor_record(GuiCompositor *compositor, embed_u32 surface_index, embed_u32 layer_index,
                             const GuiCompositorCommand *command)
{
    GuiCompositorLayer *const layer = compositor_layer(compositor, surface_index, layer_index);
    if ((layer == NULL) || !layer->recording || (layer->recorded_count >= compositor->limits.commands_max))
    {
        return 0;
    }
    if ((command->width <= 0) || (command->height <= 0))
    {
        return 1;
    }
    GuiCompositorCommand *const put = &layer->recorded[layer->recorded_count];
    *put = *command;
    const int on = EMBED_CALL(compositor_range, CompositorCtx, .surface_read = &compositor->surface[surface_index],
                              .range = put, .left = GUI_COMPOSITOR_PIXEL(command->x),
                              .top = GUI_COMPOSITOR_PIXEL(command->y),
                              .right = GUI_COMPOSITOR_PIXEL(command->x + command->width - 1),
                              .bottom = GUI_COMPOSITOR_PIXEL(command->y + command->height - 1));
    layer->recorded_count += on ? 1u : 0u;
    return 1;
}

// the pool tile of the cell `cell` given back to the pool where the cell has one
EMBED_INLINE void compositor_slot_free(const CompositorCtx *args)
{
    embed_u16 *const slot = &args->layer->slots[args->cell];
    if (*slot != GUI_COMPOSITOR_NONE)
    {
        args->compositor->free_tiles[args->compositor->free_count] = *slot;
        args->compositor->free_count += 1u;
        *slot = GUI_COMPOSITOR_NONE;
    }
}

int gui_compositor_surface_open(const GuiCompositorSurfaceCfg *args)
{
    GuiCompositor *const compositor = args->compositor;
    if ((args->surface >= GUI_COMPOSITOR_SURFACES) || (args->width == 0u) || (args->height == 0u) ||
        (args->width > compositor->limits.width_max) || (args->height > compositor->limits.height_max))
    {
        return 0;
    }
    gui_compositor_surface_shut(args);
    GuiCompositorSurface *const surface = &compositor->surface[args->surface];
    surface->open = 1;
    surface->width = args->width;
    surface->height = args->height;
    surface->columns = (args->width + GUI_COMPOSITOR_TILE - 1u) >> GUI_COMPOSITOR_TILE_SHIFT;
    surface->rows = (args->height + GUI_COMPOSITOR_TILE - 1u) >> GUI_COMPOSITOR_TILE_SHIFT;
    surface->clear = args->clear;
    surface->retained = args->retained;
    surface->ring_count = 0u;
    surface->words = ((surface->columns * surface->rows) + 63u) >> 6u;
    const embed_u32 cells = surface->columns * surface->rows;
    for (embed_u32 cell = 0u; cell < cells; cell += 1u)
    {
        EMBED_CALL(compositor_bit_set, CompositorCtx, .bits = surface->dirty, .cell = cell);
    }
    for (embed_u32 at = 0u; at < GUI_COMPOSITOR_LAYERS; at += 1u)
    {
        GuiCompositorLayer *const layer = &surface->layer[at];
        layer->shown_count = 0u;
        layer->recorded_count = 0u;
        layer->glyphs_shown_count = 0u;
        layer->glyphs_recorded_count = 0u;
        layer->recording = 0;
        __builtin_memset(layer->dirty, 0, (embed_u64)surface->words * sizeof(embed_u64));
        for (embed_u32 cell = 0u; cell < cells; cell += 1u)
        {
            layer->slots[cell] = GUI_COMPOSITOR_NONE;
        }
    }
    return 1;
}

void gui_compositor_surface_shut(const GuiCompositorSurfaceCfg *args)
{
    if (args->surface >= GUI_COMPOSITOR_SURFACES)
    {
        return;
    }
    GuiCompositorSurface *const surface = &args->compositor->surface[args->surface];
    if (!surface->open)
    {
        return;
    }
    for (embed_u32 at = 0u; at < GUI_COMPOSITOR_LAYERS; at += 1u)
    {
        for (embed_u32 cell = 0u; cell < (surface->columns * surface->rows); cell += 1u)
        {
            EMBED_CALL(compositor_slot_free, CompositorCtx, .compositor = args->compositor,
                       .layer = &surface->layer[at], .cell = cell);
        }
    }
    surface->open = 0;
}

void gui_compositor_record_begin(const GuiCompositorLayerCfg *args)
{
    GuiCompositorLayer *const layer = compositor_layer(args->compositor, args->surface, args->layer);
    if (layer != NULL)
    {
        layer->recording = 1;
        layer->recorded_count = 0u;
        layer->glyphs_recorded_count = 0u;
    }
}

// whether two commands draw the same: every field the same, the tiles they cover with them
EMBED_INLINE int compositor_same(const CompositorCtx *args)
{
    const GuiCompositorCommand *const one = args->command;
    const GuiCompositorCommand *const other = args->other;
    return (one->kind == other->kind) && (one->color == other->color) && (one->x == other->x) &&
           (one->y == other->y) && (one->width == other->width) && (one->height == other->height) &&
           (one->image == other->image) && (one->count == other->count) && (one->digest == other->digest) &&
           (one->tile_left == other->tile_left) &&
           (one->tile_top == other->tile_top) && (one->tile_right == other->tile_right) &&
           (one->tile_bottom == other->tile_bottom);
}

// how many bits of `bits` are 1, `words` words of them
static embed_u32 compositor_bits_count(const embed_u64 *bits, embed_u32 words)
{
    embed_u32 count = 0u;
    for (embed_u32 at = 0u; at < words; at += 1u)
    {
        // a count of a word's bits is at most 64
        count += (embed_u32)__builtin_popcountll(bits[at]);
    }
    return count;
}

embed_u32 gui_compositor_record_end(const GuiCompositorLayerCfg *args)
{
    GuiCompositorLayer *const layer = compositor_layer(args->compositor, args->surface, args->layer);
    if ((layer == NULL) || !layer->recording)
    {
        return 0u;
    }
    const GuiCompositorSurface *const surface = &args->compositor->surface[args->surface];
    const embed_u32 before = compositor_bits_count(layer->dirty, surface->words);
    const embed_u32 most = (layer->recorded_count > layer->shown_count) ? layer->recorded_count : layer->shown_count;
    for (embed_u32 at = 0u; at < most; at += 1u)
    {
        const GuiCompositorCommand *const recorded = (at < layer->recorded_count) ? &layer->recorded[at] : NULL;
        const GuiCompositorCommand *const shown = (at < layer->shown_count) ? &layer->shown[at] : NULL;
        if ((recorded != NULL) && (shown != NULL) &&
            EMBED_CALL(compositor_same, CompositorCtx, .command = recorded, .other = shown))
        {
            continue;
        }
        if (recorded != NULL)
        {
            EMBED_CALL(compositor_command_mark, CompositorCtx, .surface_read = surface, .bits = layer->dirty,
                       .command = recorded);
        }
        if (shown != NULL)
        {
            EMBED_CALL(compositor_command_mark, CompositorCtx, .surface_read = surface, .bits = layer->dirty,
                       .command = shown);
        }
    }
    GuiCompositorCommand *const swapped = layer->shown;
    layer->shown = layer->recorded;
    layer->recorded = swapped;
    layer->shown_count = layer->recorded_count;
    layer->recorded_count = 0u;
    GuiCompositorGlyph *const glyphs_swapped = layer->glyphs_shown;
    layer->glyphs_shown = layer->glyphs_recorded;
    layer->glyphs_recorded = glyphs_swapped;
    layer->glyphs_shown_count = layer->glyphs_recorded_count;
    layer->glyphs_recorded_count = 0u;
    layer->recording = 0;
    return compositor_bits_count(layer->dirty, surface->words) - before;
}

int gui_compositor_fill(const GuiCompositorFillCfg *args)
{
    const GuiCompositorCommand command = {GUI_COMPOSITOR_FILL, args->color, args->x, args->y, args->width,
                                          args->height, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    return compositor_record(args->compositor, args->surface, args->layer, &command);
}

int gui_compositor_put(const GuiCompositorPutCfg *args)
{
    if ((args->image >= GUI_COMPOSITOR_IMAGES) || (args->compositor->image[args->image].pixels == NULL))
    {
        return 0;
    }
    const GuiCompositorImage *const image = &args->compositor->image[args->image];
    const GuiCompositorCommand command = {GUI_COMPOSITOR_IMAGE,
                                          0u,
                                          GUI_COMPOSITOR_FIXED(GUI_COMPOSITOR_PIXEL(args->x)),
                                          GUI_COMPOSITOR_FIXED(GUI_COMPOSITOR_PIXEL(args->y)),
                                          GUI_COMPOSITOR_FIXED(image->width),
                                          GUI_COMPOSITOR_FIXED(image->height),
                                          args->image,
                                          0u,
                                          0u,
                                          0u,
                                          0u,
                                          0u,
                                          0u};
    return compositor_record(args->compositor, args->surface, args->layer, &command);
}

// the 32-bit FNV-1a digest of `count` glyphs, their pens and entries
static embed_u32 compositor_glyphs_digest(const GuiCompositorGlyph *glyphs, embed_u32 count)
{
    embed_u32 digest = 2166136261u;
    for (embed_u32 at = 0u; at < count; at += 1u)
    {
        // the code takes the place of a pen as its bits, its sign with them
        const embed_u32 words[3] = {(embed_u32)glyphs[at].x, (embed_u32)glyphs[at].y, glyphs[at].entry};
        for (embed_u32 word = 0u; word < 3u; word += 1u)
        {
            for (embed_u32 shift = 0u; shift < 32u; shift += 8u)
            {
                digest = (digest ^ ((words[word] >> shift) & 0xffu)) * 16777619u;
            }
        }
    }
    return digest;
}

int gui_compositor_glyphs(const GuiCompositorGlyphsCfg *args)
{
    GuiCompositor *const compositor = args->compositor;
    GuiCompositorLayer *const layer = compositor_layer(compositor, args->surface, args->layer);
    if ((layer == NULL) || !layer->recording ||
        ((layer->glyphs_recorded_count + args->count) > compositor->limits.items_max))
    {
        return 0;
    }
    embed_i32 left = 0x7fffffff;
    embed_i32 top = 0x7fffffff;
    embed_i32 right = -0x7fffffff;
    embed_i32 bottom = -0x7fffffff;
    for (embed_u32 at = 0u; at < args->count; at += 1u)
    {
        const GuiCompositorGlyph *const glyph = &args->glyphs[at];
        if ((glyph->entry >= compositor->limits.glyphs_max) || !compositor->entry[glyph->entry].baked)
        {
            return 0;
        }
        const GuiCompositorGlyphEntry *const entry = &compositor->entry[glyph->entry];
        if ((entry->width == 0u) || (entry->height == 0u))
        {
            continue;
        }
        const embed_i32 x = GUI_COMPOSITOR_PIXEL(glyph->x) + entry->left;
        const embed_i32 y = GUI_COMPOSITOR_PIXEL(glyph->y) - entry->top;
        left = (x < left) ? x : left;
        top = (y < top) ? y : top;
        right = ((x + (embed_i32)entry->width) > right) ? (x + (embed_i32)entry->width) : right;
        bottom = ((y + (embed_i32)entry->height) > bottom) ? (y + (embed_i32)entry->height) : bottom;
    }
    if (right <= left)
    {
        return 1;
    }
    const embed_u32 first = layer->glyphs_recorded_count;
    __builtin_memcpy(layer->glyphs_recorded + first, args->glyphs, (embed_u64)args->count * sizeof(GuiCompositorGlyph));
    layer->glyphs_recorded_count += args->count;
    const GuiCompositorCommand command = {GUI_COMPOSITOR_GLYPH_RUN,
                                          args->color,
                                          GUI_COMPOSITOR_FIXED(left),
                                          GUI_COMPOSITOR_FIXED(top),
                                          GUI_COMPOSITOR_FIXED(right - left),
                                          GUI_COMPOSITOR_FIXED(bottom - top),
                                          first,
                                          args->count,
                                          compositor_glyphs_digest(args->glyphs, args->count),
                                          0u,
                                          0u,
                                          0u,
                                          0u};
    if (!compositor_record(compositor, args->surface, args->layer, &command))
    {
        layer->glyphs_recorded_count = first;
        return 0;
    }
    return 1;
}

int gui_compositor_replay(const GuiCompositorReplayCfg *args)
{
    GuiCompositor *const compositor = args->compositor;
    GuiCompositorLayer *const layer = compositor_layer(compositor, args->surface, args->layer);
    if ((layer == NULL) || !layer->recording ||
        ((layer->glyphs_recorded_count + args->glyph_count) > compositor->limits.items_max))
    {
        return 0;
    }
    // the runs' glyphs go after those the layer holds, and each run's first glyph moves on by as many
    const embed_u32 base = layer->glyphs_recorded_count;
    if (args->glyph_count != 0u)
    {
        __builtin_memcpy(layer->glyphs_recorded + base, args->glyphs,
                         (embed_u64)args->glyph_count * sizeof(GuiCompositorGlyph));
        layer->glyphs_recorded_count += args->glyph_count;
    }
    for (embed_u32 at = 0u; at < args->count; at += 1u)
    {
        GuiCompositorCommand command = args->commands[at];
        command.image += (command.kind == GUI_COMPOSITOR_GLYPH_RUN) ? base : 0u;
        if (!compositor_record(compositor, args->surface, args->layer, &command))
        {
            return 0;
        }
    }
    return 1;
}

// the rectangles of the surface's clip ring set in its bits, the ring empty after
EMBED_INLINE void compositor_ring_fold(const CompositorCtx *args)
{
    GuiCompositorSurface *const surface = args->surface;
    for (embed_u32 at = 0u; at < surface->ring_count; at += 1u)
    {
        const GuiCompositorRect rect = surface->ring[at];
        GuiCompositorCommand range = {0u, 0u, 0, 0, 0, 0, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        if (EMBED_CALL(compositor_range, CompositorCtx, .surface_read = surface, .range = &range,
                       .left = rect.x, .top = rect.y, .right = rect.x + rect.width - 1,
                       .bottom = rect.y + rect.height - 1))
        {
            EMBED_CALL(compositor_command_mark, CompositorCtx, .surface_read = surface, .bits = surface->dirty,
                       .command = &range);
        }
    }
    surface->ring_count = 0u;
}

void gui_compositor_damage(const GuiCompositorDamageCfg *args)
{
    if ((args->surface >= GUI_COMPOSITOR_SURFACES) || !args->compositor->surface[args->surface].open ||
        (args->rect.width <= 0) || (args->rect.height <= 0))
    {
        return;
    }
    GuiCompositorSurface *const surface = &args->compositor->surface[args->surface];
    if (surface->ring_count >= args->compositor->limits.ring_max)
    {
        EMBED_CALL(compositor_ring_fold, CompositorCtx, .surface = surface);
    }
    surface->ring[surface->ring_count] = args->rect;
    surface->ring_count += 1u;
}

// one channel of `target` kept by `keep` of 255, rounded, as the vector rounds it
EMBED_INLINE embed_u32 compositor_keep(embed_u32 value, embed_u32 keep)
{
    const embed_u32 product = (value * keep) + 128u;
    return (product + (product >> 8u)) >> 8u;
}

// `source` put over `target`, one pixel: each channel the source's plus the target's kept by what the source's alpha
// leaves, clamped to 255
EMBED_INLINE embed_u32 compositor_over_one(embed_u32 target, embed_u32 source)
{
    const embed_u32 keep = 255u - (source >> 24u);
    embed_u32 made = 0u;
    for (embed_u32 shift = 0u; shift < 32u; shift += 8u)
    {
        const embed_u32 sum = compositor_keep((target >> shift) & 0xffu, keep) + ((source >> shift) & 0xffu);
        made |= ((sum > 255u) ? 255u : sum) << shift;
    }
    return made;
}

#if COMPOSITOR_VECTOR
// four pixels as sixteen bytes, and as sixteen lanes of 16 bits for the products
typedef unsigned char CompositorBytes EMBED_VECTOR(16);
typedef unsigned short CompositorWide EMBED_VECTOR(32);
typedef embed_u32 CompositorWords EMBED_VECTOR(16);

// four pixels of `source` put over four of `target` in one vector, as compositor_over_one puts one
EMBED_INLINE CompositorBytes compositor_over_four(CompositorBytes target, CompositorBytes source)
{
    const CompositorBytes alpha =
        __builtin_shufflevector(source, source, 3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15);
    const CompositorBytes keep = (CompositorBytes)(255u - alpha);
    CompositorWide product =
        (__builtin_convertvector(target, CompositorWide) * __builtin_convertvector(keep, CompositorWide)) + 128u;
    product = (product + (product >> 8u)) >> 8u;
    CompositorWide sum = product + __builtin_convertvector(source, CompositorWide);
    // a lane past 255 comes down to 255. The compare is all ones in such a lane
    sum = sum - ((CompositorWide)(sum > 255u) & (sum - 255u));
    return __builtin_convertvector(sum, CompositorBytes);
}
#endif

void gui_compositor_over_scalar(const GuiCompositorOverCfg *args)
{
    for (embed_u32 at = 0u; at < args->count; at += 1u)
    {
        args->target[at] = compositor_over_one(args->target[at], args->source[at]);
    }
}

void gui_compositor_over(const GuiCompositorOverCfg *args)
{
    embed_u32 at = 0u;
#if COMPOSITOR_VECTOR
    for (; (at + 4u) <= args->count; at += 4u)
    {
        CompositorBytes target;
        CompositorBytes source;
        __builtin_memcpy(&target, args->target + at, sizeof(target));
        __builtin_memcpy(&source, args->source + at, sizeof(source));
        const CompositorBytes made = compositor_over_four(target, source);
        __builtin_memcpy(args->target + at, &made, sizeof(made));
    }
#endif
    for (; at < args->count; at += 1u)
    {
        args->target[at] = compositor_over_one(args->target[at], args->source[at]);
    }
}

// `count` pixels of `target` with `color` put over each: a store where the color is opaque, nothing where it is clear
EMBED_INLINE void compositor_over_color(const CompositorCtx *args)
{
    const embed_u32 alpha = args->color >> 24u;
    if (alpha == 0u)
    {
        return;
    }
    if (alpha == 255u)
    {
        for (embed_u32 at = 0u; at < args->count; at += 1u)
        {
            args->target[at] = args->color;
        }
        return;
    }
    embed_u32 at = 0u;
#if COMPOSITOR_VECTOR
    const CompositorWords words = {args->color, args->color, args->color, args->color};
    const CompositorBytes source = (CompositorBytes)words;
    for (; (at + 4u) <= args->count; at += 4u)
    {
        CompositorBytes target;
        __builtin_memcpy(&target, args->target + at, sizeof(target));
        const CompositorBytes made = compositor_over_four(target, source);
        __builtin_memcpy(args->target + at, &made, sizeof(made));
    }
#endif
    for (; at < args->count; at += 1u)
    {
        args->target[at] = compositor_over_one(args->target[at], args->color);
    }
}

// `color` with each channel scaled by `cover` of 256
EMBED_INLINE embed_u32 compositor_covered(embed_u32 color, embed_u32 cover)
{
    if (cover >= 256u)
    {
        return color;
    }
    const embed_u32 red_blue = (((color & 0x00ff00ffu) * cover) >> 8u) & 0x00ff00ffu;
    const embed_u32 alpha_green = ((((color >> 8u) & 0x00ff00ffu) * cover) >> 8u) & 0x00ff00ffu;
    return red_blue | (alpha_green << 8u);
}

// the share of 256 of the pixel `pixel` that the span `from` to `to` in 16.16 covers
EMBED_INLINE embed_u32 compositor_share(embed_i32 pixel, embed_i32 from, embed_i32 to)
{
    const embed_i32 start = GUI_COMPOSITOR_FIXED(pixel);
    const embed_i32 low = (from > start) ? from : start;
    const embed_i32 high = (to < (start + 65536)) ? to : (start + 65536);
    // a share inside one pixel is 0 to 65536, and 256 after the shift
    return (high > low) ? ((embed_u32)(high - low) >> 8u) : 0u;
}

// The fill `command`, drawn into the target. The pixels `left` to `right` across and `top` to `bottom` down are the part
// that it draws, both ends in. The top left of the target is the pixel at the x and y of `rect`, `stride` pixels a row.
// Each pixel takes the color scaled by the share of it that the rectangle covers across, times the share down
EMBED_INLINE void compositor_fill_draw(const CompositorCtx *args)
{
    const GuiCompositorCommand *const command = args->command;
    const embed_i32 from_x = command->x;
    const embed_i32 to_x = command->x + command->width;
    const embed_i32 from_y = command->y;
    const embed_i32 to_y = command->y + command->height;
    // the full columns: those the rectangle covers whole across
    const embed_i32 full_left = GUI_COMPOSITOR_PIXEL(from_x + 65535);
    const embed_i32 full_right = GUI_COMPOSITOR_PIXEL(to_x) - 1;
    for (embed_i32 row = args->top; row <= args->bottom; row += 1)
    {
        const embed_u32 down = compositor_share(row, from_y, to_y);
        if (down == 0u)
        {
            continue;
        }
        // the row lies inside the target: its place is not negative
        embed_u32 *const line = args->target + ((embed_u64)(embed_u32)(row - args->rect.y) * args->stride);
        const embed_i32 inner_left = (full_left > args->left) ? full_left : args->left;
        const embed_i32 inner_right = (full_right < args->right) ? full_right : args->right;
        for (embed_i32 column = args->left; column <= args->right; column += 1)
        {
            if ((column >= inner_left) && (column <= inner_right))
            {
                EMBED_CALL(compositor_over_color, CompositorCtx,
                           .target = line + (embed_u32)(column - args->rect.x),
                           .color = compositor_covered(command->color, down),
                           .count = (embed_u32)(inner_right - column + 1));
                column = inner_right;
                continue;
            }
            const embed_u32 across = compositor_share(column, from_x, to_x);
            const embed_u32 cover = (across * down) >> 8u;
            if (cover != 0u)
            {
                line[column - args->rect.x] =
                    compositor_over_one(line[column - args->rect.x], compositor_covered(command->color, cover));
            }
        }
    }
}

// The image that `command` names, drawn into the target as compositor_fill_draw draws a fill. Each row of the part that
// it draws goes over the row of the target in one blend
EMBED_INLINE void compositor_image_draw(const CompositorCtx *args)
{
    const GuiCompositorCommand *const command = args->command;
    const GuiCompositorImage *const image = &args->compositor->image[command->image];
    if (image->pixels == NULL)
    {
        return;
    }
    const embed_i32 image_x = GUI_COMPOSITOR_PIXEL(command->x);
    const embed_i32 image_y = GUI_COMPOSITOR_PIXEL(command->y);
    for (embed_i32 row = args->top; row <= args->bottom; row += 1)
    {
        // the rows and columns drawn lie inside both the image and the target: no place here is negative
        embed_u32 *const line = args->target + ((embed_u64)(embed_u32)(row - args->rect.y) * args->stride);
        const embed_u32 *const source =
            image->pixels + ((embed_u64)(embed_u32)(row - image_y) * image->width) + (embed_u32)(args->left - image_x);
        gui_compositor_over(&(GuiCompositorOverCfg){.target = line + (embed_u32)(args->left - args->rect.x),
                                                    .source = source,
                                                    .count = (embed_u32)(args->right - args->left + 1)});
    }
}

// The brightness of a color, 0 to 255: its red, green and blue weighted as the eye weighs them, a sum of 256 parts
EMBED_INLINE embed_u32 compositor_brightness(embed_u32 color)
{
    return ((((color >> 16u) & 0xffu) * 54u) + (((color >> 8u) & 0xffu) * 183u) + ((color & 0xffu) * 19u)) >> 8u;
}

// The run `command` drawn into the target as compositor_fill_draw draws a fill, its part `left` to `right` and `top`
// to `bottom`. The code first reads the brightness of the ground under that part, a pixel in four each way. The
// nearer it is to that of the text, the more the band lifts the coverage.
// The coverage of each glyph out of its page then takes the color of the text through the band. The code marks each
// page that it draws from with the stamp of the compose
EMBED_INLINE void compositor_run_draw(const CompositorCtx *args)
{
    GuiCompositor *const compositor = args->compositor;
    const GuiCompositorCommand *const command = args->command;
    embed_u32 ground = 0u;
    embed_u32 samples = 0u;
    for (embed_i32 row = args->top; row <= args->bottom; row += 4)
    {
        // the rows and columns read lie inside the target: no place here is negative
        const embed_u32 *const line = args->target + ((embed_u64)(embed_u32)(row - args->rect.y) * args->stride);
        for (embed_i32 column = args->left; column <= args->right; column += 4)
        {
            ground += compositor_brightness(line[column - args->rect.x]);
            samples += 1u;
        }
    }
    const embed_u32 text = compositor_brightness(command->color);
    const embed_u32 under = (samples != 0u) ? (ground / samples) : 0u;
    const embed_u32 apart = (text > under) ? (text - under) : (under - text);
    const embed_u32 band = (apart >= 192u) ? 0u : (apart >= 128u) ? 1u : (apart >= 64u) ? 2u : 3u;
    const unsigned char *const lift = compositor->contrast[band];
    const embed_u32 side = compositor->limits.atlas_side;
    const GuiCompositorLayer *const layer = args->layer;
    for (embed_u32 at = 0u; at < command->count; at += 1u)
    {
        const GuiCompositorGlyph *const glyph = &layer->glyphs_shown[command->image + at];
        const GuiCompositorGlyphEntry *const entry = &compositor->entry[glyph->entry];
        if (!entry->baked || (entry->width == 0u) || (entry->height == 0u))
        {
            continue;
        }
        compositor->page[entry->page].stamp = compositor->stamp;
        const embed_i32 x = GUI_COMPOSITOR_PIXEL(glyph->x) + entry->left;
        const embed_i32 y = GUI_COMPOSITOR_PIXEL(glyph->y) - entry->top;
        const embed_i32 from_x = (x > args->left) ? x : args->left;
        const embed_i32 from_y = (y > args->top) ? y : args->top;
        const embed_i32 to_x = ((x + (embed_i32)entry->width - 1) < args->right) ? (x + (embed_i32)entry->width - 1)
                                                                                  : args->right;
        const embed_i32 to_y = ((y + (embed_i32)entry->height - 1) < args->bottom)
                                   ? (y + (embed_i32)entry->height - 1)
                                   : args->bottom;
        const unsigned char *const page = compositor->coverage + ((embed_u64)entry->page * side * side);
        for (embed_i32 row = from_y; row <= to_y; row += 1)
        {
            // the rows and columns drawn lie inside both the glyph's box and the target: no place here is negative
            embed_u32 *const line = args->target + ((embed_u64)(embed_u32)(row - args->rect.y) * args->stride);
            const unsigned char *const cover =
                page + ((embed_u64)(entry->y + (embed_u32)(row - y)) * side) + entry->x + (embed_u32)(from_x - x);
            for (embed_i32 column = from_x; column <= to_x; column += 1)
            {
                const embed_u32 lifted = lift[cover[column - from_x]];
                if (lifted != 0u)
                {
                    embed_u32 *const pixel = &line[column - args->rect.x];
                    *pixel = compositor_over_one(*pixel, compositor_covered(command->color, lifted + (lifted >> 7u)));
                }
            }
        }
    }
}

// The layer's commands that cover the tile `rect` drawn into the target in their order, each cut to the tile. The
// target's top left is the tile's, `stride` pixels a row
EMBED_INLINE void compositor_layer_draw(const CompositorCtx *args)
{
    const GuiCompositorLayer *const layer = args->layer;
    // a tile's place is not negative and its number fits 16 bits
    const embed_u32 column = (embed_u32)args->rect.x >> GUI_COMPOSITOR_TILE_SHIFT;
    const embed_u32 row = (embed_u32)args->rect.y >> GUI_COMPOSITOR_TILE_SHIFT;
    for (embed_u32 at = 0u; at < layer->shown_count; at += 1u)
    {
        const GuiCompositorCommand *const command = &layer->shown[at];
        if ((column < command->tile_left) || (column > command->tile_right) || (row < command->tile_top) ||
            (row > command->tile_bottom))
        {
            continue;
        }
        const embed_i32 tile_right = args->rect.x + args->rect.width - 1;
        const embed_i32 tile_bottom = args->rect.y + args->rect.height - 1;
        const embed_i32 command_right = GUI_COMPOSITOR_PIXEL(command->x + command->width - 1);
        const embed_i32 command_bottom = GUI_COMPOSITOR_PIXEL(command->y + command->height - 1);
        const embed_i32 left = (GUI_COMPOSITOR_PIXEL(command->x) > args->rect.x) ? GUI_COMPOSITOR_PIXEL(command->x)
                                                                                 : args->rect.x;
        const embed_i32 top = (GUI_COMPOSITOR_PIXEL(command->y) > args->rect.y) ? GUI_COMPOSITOR_PIXEL(command->y)
                                                                                : args->rect.y;
        const embed_i32 right = (command_right < tile_right) ? command_right : tile_right;
        const embed_i32 bottom = (command_bottom < tile_bottom) ? command_bottom : tile_bottom;
        if ((right < left) || (bottom < top))
        {
            continue;
        }
        if (command->kind == GUI_COMPOSITOR_FILL)
        {
            EMBED_CALL(compositor_fill_draw, CompositorCtx, .command = command, .target = args->target,
                       .stride = args->stride, .rect = args->rect, .left = left, .top = top, .right = right,
                       .bottom = bottom);
        }
        else if (command->kind == GUI_COMPOSITOR_IMAGE)
        {
            EMBED_CALL(compositor_image_draw, CompositorCtx, .compositor = args->compositor, .command = command,
                       .target = args->target, .stride = args->stride, .rect = args->rect, .left = left, .top = top,
                       .right = right, .bottom = bottom);
        }
        else if (command->kind == GUI_COMPOSITOR_GLYPH_RUN)
        {
            EMBED_CALL(compositor_run_draw, CompositorCtx, .compositor = args->compositor, .layer = layer,
                       .command = command, .target = args->target, .stride = args->stride, .rect = args->rect,
                       .left = left, .top = top, .right = right, .bottom = bottom);
        }
    }
}

// The tile `cell` of the kept layer. Where the layer marked the tile or the pool tile is new, the code draws its pool
// tile again from the commands of the layer. It then puts them over the tile of the raster.
// A layer with no tile left in the pool draws its commands straight into the raster, which gives the same pixels
EMBED_INLINE void compositor_retained_compose(const CompositorCtx *args)
{
    GuiCompositor *const compositor = args->compositor;
    GuiCompositorLayer *const layer = args->layer;
    embed_u16 *const slot = &layer->slots[args->cell];
    int draw = (layer->dirty[args->cell >> 6u] >> (args->cell & 63u)) & 1u;
    if (*slot == GUI_COMPOSITOR_NONE)
    {
        if (compositor->free_count == 0u)
        {
            EMBED_CALL(compositor_layer_draw, CompositorCtx, .compositor = compositor, .layer = layer,
                       .target = args->target, .stride = args->stride, .rect = args->rect);
            return;
        }
        compositor->free_count -= 1u;
        *slot = compositor->free_tiles[compositor->free_count];
        draw = 1;
    }
    embed_u32 *const tile = compositor->pool + ((embed_u64)*slot * COMPOSITOR_TILE_PIXELS);
    if (draw)
    {
        __builtin_memset(tile, 0, COMPOSITOR_TILE_PIXELS * sizeof(embed_u32));
        EMBED_CALL(compositor_layer_draw, CompositorCtx, .compositor = compositor, .layer = layer, .target = tile,
                   .stride = GUI_COMPOSITOR_TILE, .rect = args->rect);
    }
    // the tile's rows lie inside the raster's: its height is not negative
    for (embed_u32 row = 0u; row < (embed_u32)args->rect.height; row += 1u)
    {
        gui_compositor_over(&(GuiCompositorOverCfg){.target = args->target + ((embed_u64)row * args->stride),
                                                    .source = tile + ((embed_u64)row * GUI_COMPOSITOR_TILE),
                                                    .count = (embed_u32)args->rect.width});
    }
}

embed_u32 gui_compositor_compose(const GuiCompositorComposeCfg *args)
{
    GuiCompositor *const compositor = args->compositor;
    GuiRaster *const raster = args->raster;
    if ((args->surface >= GUI_COMPOSITOR_SURFACES) || !compositor->surface[args->surface].open || (raster == NULL) ||
        (raster->pixels == NULL))
    {
        return 0u;
    }
    GuiCompositorSurface *const surface = &compositor->surface[args->surface];
    if ((raster->width != surface->width) || (raster->height != surface->height))
    {
        return 0u;
    }
    EMBED_CALL(compositor_ring_fold, CompositorCtx, .surface = surface);
    compositor->stamp += 1u;
    embed_u32 made = 0u;
    for (embed_u32 word = 0u; word < surface->words; word += 1u)
    {
        embed_u64 marked = surface->dirty[word];
        for (embed_u32 at = 0u; at < GUI_COMPOSITOR_LAYERS; at += 1u)
        {
            marked |= surface->layer[at].dirty[word];
        }
        while (marked != 0ULL)
        {
            // the lowest set bit's place is below 64
            const embed_u32 cell = (word << 6u) + (embed_u32)__builtin_ctzll(marked);
            marked &= marked - 1ULL;
            const embed_u32 column = cell % surface->columns;
            const embed_u32 row = cell / surface->columns;
            const embed_u32 x = column << GUI_COMPOSITOR_TILE_SHIFT;
            const embed_u32 y = row << GUI_COMPOSITOR_TILE_SHIFT;
            const embed_u32 width = ((x + GUI_COMPOSITOR_TILE) > surface->width) ? (surface->width - x) : GUI_COMPOSITOR_TILE;
            const embed_u32 height =
                ((y + GUI_COMPOSITOR_TILE) > surface->height) ? (surface->height - y) : GUI_COMPOSITOR_TILE;
            // a tile's place and size are below the surface's, far inside an int
            const GuiCompositorRect rect = {(embed_i32)x, (embed_i32)y, (embed_i32)width, (embed_i32)height};
            embed_u32 *const target = raster->pixels + ((embed_u64)y * raster->width) + x;
            for (embed_u32 line = 0u; line < height; line += 1u)
            {
                for (embed_u32 at = 0u; at < width; at += 1u)
                {
                    target[((embed_u64)line * raster->width) + at] = surface->clear;
                }
            }
            for (embed_u32 at = 0u; at < GUI_COMPOSITOR_LAYERS; at += 1u)
            {
                GuiCompositorLayer *const layer = &surface->layer[at];
                if (layer->shown_count == 0u)
                {
                    EMBED_CALL(compositor_slot_free, CompositorCtx, .compositor = compositor, .layer = layer,
                               .cell = cell);
                    continue;
                }
                if ((surface->retained >> at) & 1u)
                {
                    EMBED_CALL(compositor_retained_compose, CompositorCtx, .compositor = compositor, .layer = layer,
                               .cell = cell, .target = target, .stride = raster->width, .rect = rect);
                }
                else
                {
                    EMBED_CALL(compositor_layer_draw, CompositorCtx, .compositor = compositor, .layer = layer,
                               .target = target, .stride = raster->width, .rect = rect);
                }
            }
            made += 1u;
        }
        surface->dirty[word] = 0ULL;
        for (embed_u32 at = 0u; at < GUI_COMPOSITOR_LAYERS; at += 1u)
        {
            surface->layer[at].dirty[word] = 0ULL;
        }
    }
    return made;
}
