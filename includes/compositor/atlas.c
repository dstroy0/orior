// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// atlas.c: the compositor's glyph atlas. Pages of coverage bytes, each packed by a skyline: a list of segments across
// the page, each with the height the glyphs under it reach. A glyph goes where it sits lowest, then furthest left. A
// glyph of a reserved range is found by its place in the range; any other by a hash of its key. A page with no room
// lets the page drawn from least recently go, unless a range keeps it. The contrast tables lift a byte of coverage
// more where the text's brightness is near its ground's
#include "atlas_internal.h"

#include "../arith/arith.h"

// the alignment of the parts of a block in bytes, and the gutter of a page, the empty pixels right of and below a glyph
#define ATLAS_ALIGN 64ULL
#define ATLAS_ALIGNED(bytes_) ((((embed_u64)(bytes_)) + ATLAS_ALIGN - 1ULL) & ~(ATLAS_ALIGN - 1ULL))
#define ATLAS_GUTTER 1u

// the gamma that each contrast band lifts coverage through, the band of the most contrast first. That band lifts text
// as the text gamma of the window lifts it. Each band of less contrast lifts it more
static const float s_band_gamma[GUI_COMPOSITOR_BANDS] = {1.4f, 1.6f, 1.8f, 2.0f};

typedef struct
{
    GuiCompositor *compositor;
    const GuiCompositor *compositor_read;
    GuiCompositorPage *page;
    embed_u64 key;
    embed_u32 width;
    embed_u32 height;
    embed_u32 at;
    embed_u32 *x_out;
    embed_u32 *y_out;
} AtlasCtx;

// the entries of the hash for `glyphs_max` glyphs: a power of 2 at least twice them
static embed_u32 atlas_hash_size(embed_u32 glyphs_max)
{
    embed_u32 size = 16u;
    while (size < (glyphs_max * 2u))
    {
        size <<= 1u;
    }
    return size;
}

embed_u64 gui_compositor_atlas_bytes(const GuiCompositorLimitsCfg *limits)
{
    const embed_u32 side = limits->atlas_side;
    if ((side < 64u) || (side > 4096u) || ((side & (side - 1u)) != 0u) || (limits->atlas_pages == 0u) ||
        (limits->atlas_pages >= 0xffffu) || (limits->glyphs_max == 0u) || (limits->glyphs_max > 0x00ffffffu) ||
        (limits->items_max == 0u))
    {
        return 0ULL;
    }
    const embed_u64 pages = limits->atlas_pages;
    return ATLAS_ALIGNED(pages * side * side) + ATLAS_ALIGNED(pages * sizeof(GuiCompositorPage)) +
           ATLAS_ALIGNED(pages * 3ULL * side * sizeof(embed_u16)) +
           ATLAS_ALIGNED((embed_u64)limits->glyphs_max * sizeof(GuiCompositorGlyphEntry)) +
           ATLAS_ALIGNED((embed_u64)atlas_hash_size(limits->glyphs_max) * sizeof(embed_u32));
}

// the page's skyline made one segment across it at height 0
EMBED_INLINE void atlas_page_clear(const AtlasCtx *args)
{
    GuiCompositorPage *const page = args->page;
    page->segments = 1u;
    page->segment_x[0] = 0u;
    page->segment_y[0] = 0u;
    // a page's side is at most 4096, inside 16 bits
    page->segment_width[0] = (embed_u16)args->compositor_read->limits.atlas_side;
}

unsigned char *gui_compositor_atlas_place(GuiCompositor *compositor, unsigned char *place)
{
    const GuiCompositorLimitsCfg *const limits = &compositor->limits;
    const embed_u64 side = limits->atlas_side;
    compositor->coverage = place;
    place += ATLAS_ALIGNED((embed_u64)limits->atlas_pages * side * side);
    compositor->page = (GuiCompositorPage *)(void *)place;
    place += ATLAS_ALIGNED((embed_u64)limits->atlas_pages * sizeof(GuiCompositorPage));
    embed_u16 *const segments = (embed_u16 *)(void *)place;
    place += ATLAS_ALIGNED((embed_u64)limits->atlas_pages * 3ULL * side * sizeof(embed_u16));
    for (embed_u32 at = 0u; at < limits->atlas_pages; at += 1u)
    {
        GuiCompositorPage *const page = &compositor->page[at];
        page->segment_x = segments + ((embed_u64)at * 3ULL * side);
        page->segment_y = page->segment_x + side;
        page->segment_width = page->segment_y + side;
        page->stamp = 0u;
        page->pinned = 0;
        EMBED_CALL(atlas_page_clear, AtlasCtx, .page = page, .compositor_read = compositor);
    }
    compositor->entry = (GuiCompositorGlyphEntry *)(void *)place;
    place += ATLAS_ALIGNED((embed_u64)limits->glyphs_max * sizeof(GuiCompositorGlyphEntry));
    __builtin_memset(compositor->entry, 0, (embed_u64)limits->glyphs_max * sizeof(GuiCompositorGlyphEntry));
    const embed_u32 hash_size = atlas_hash_size(limits->glyphs_max);
    compositor->hash = (embed_u32 *)(void *)place;
    place += ATLAS_ALIGNED((embed_u64)hash_size * sizeof(embed_u32));
    __builtin_memset(compositor->hash, 0, (embed_u64)hash_size * sizeof(embed_u32));
    compositor->hash_mask = hash_size - 1u;
    compositor->ranges = 0u;
    compositor->reserved = 0u;
    compositor->stamp = 0u;
    for (embed_u32 band = 0u; band < GUI_COMPOSITOR_BANDS; band += 1u)
    {
        for (embed_u32 at = 0u; at < 256u; at += 1u)
        {
            const float lifted = arith_pow_f32((float)at / 255.0f, 1.0f / s_band_gamma[band]);
            // a share from 0 to 1 is a byte once scaled
            compositor->contrast[band][at] = (unsigned char)((lifted * 255.0f) + 0.5f);
        }
    }
    return place;
}

// the key of a glyph: its face, size, place across a pixel and glyph packed in one word
static embed_u64 atlas_key(embed_u32 face, embed_u32 size, embed_u32 glyph, embed_u32 phase)
{
    return ((embed_u64)glyph << 32u) | ((embed_u64)(phase & 3u) << 30u) | ((embed_u64)(size & 0x3fffffu) << 8u) |
           (embed_u64)(face & 0xffu);
}

// the first place in the hash `key` probes from
static embed_u32 atlas_hash_start(const GuiCompositor *compositor, embed_u64 key)
{
    // the high half of the product spreads every bit of the key
    return (embed_u32)((key * 0x9e3779b97f4a7c15ULL) >> 32u) & compositor->hash_mask;
}

// the entry of a range's glyph: GUI_COMPOSITOR_NO_GLYPH where no range holds it
static embed_u32 atlas_range_entry(const GuiCompositor *compositor, embed_u32 face, embed_u32 size, embed_u32 glyph,
                                   embed_u32 phase)
{
    for (embed_u32 at = 0u; at < compositor->ranges; at += 1u)
    {
        const GuiCompositorGlyphRange *const range = &compositor->range[at];
        if ((range->face == face) && (range->size == size) && (glyph >= range->first) &&
            ((glyph - range->first) < range->count))
        {
            return range->base + ((glyph - range->first) * GUI_COMPOSITOR_PHASES) + (phase & 3u);
        }
    }
    return GUI_COMPOSITOR_NO_GLYPH;
}

// the entry the hash holds for `key`: GUI_COMPOSITOR_NO_GLYPH where it holds none
EMBED_INLINE embed_u32 atlas_hash_find(const AtlasCtx *args)
{
    const GuiCompositor *const compositor = args->compositor_read;
    embed_u32 at = atlas_hash_start(compositor, args->key);
    while (compositor->hash[at] != 0u)
    {
        const embed_u32 entry = compositor->hash[at] - 1u;
        if (compositor->entry[entry].baked && (compositor->entry[entry].key == args->key))
        {
            return entry;
        }
        at = (at + 1u) & compositor->hash_mask;
    }
    return GUI_COMPOSITOR_NO_GLYPH;
}

// the entry `at` put in the hash under its key
EMBED_INLINE void atlas_hash_put(const AtlasCtx *args)
{
    GuiCompositor *const compositor = args->compositor;
    embed_u32 place = atlas_hash_start(compositor, compositor->entry[args->at].key);
    while (compositor->hash[place] != 0u)
    {
        place = (place + 1u) & compositor->hash_mask;
    }
    compositor->hash[place] = args->at + 1u;
}

embed_u32 gui_compositor_glyph_find(const GuiCompositorGlyphFindCfg *args)
{
    const GuiCompositor *const compositor = args->compositor_read;
    const embed_u32 ranged = atlas_range_entry(compositor, args->face, args->size, args->glyph, args->phase);
    if (ranged != GUI_COMPOSITOR_NO_GLYPH)
    {
        return compositor->entry[ranged].baked ? ranged : GUI_COMPOSITOR_NO_GLYPH;
    }
    return EMBED_CALL(atlas_hash_find, AtlasCtx, .compositor_read = compositor,
                      .key = atlas_key(args->face, args->size, args->glyph, args->phase));
}

int gui_compositor_glyph_range(const GuiCompositorGlyphRangeCfg *args)
{
    GuiCompositor *const compositor = args->compositor;
    const embed_u64 entries = (embed_u64)args->count * GUI_COMPOSITOR_PHASES;
    if ((compositor->ranges >= GUI_COMPOSITOR_RANGES) || (args->count == 0u) ||
        ((compositor->reserved + entries) > compositor->limits.glyphs_max))
    {
        return 0;
    }
    // a range takes entries that hold no baked glyph yet
    for (embed_u64 at = compositor->reserved; at < (compositor->reserved + entries); at += 1u)
    {
        if (compositor->entry[at].baked)
        {
            return 0;
        }
    }
    GuiCompositorGlyphRange *const range = &compositor->range[compositor->ranges];
    range->face = args->face;
    range->size = args->size;
    range->first = args->first;
    range->count = args->count;
    range->base = compositor->reserved;
    compositor->ranges += 1u;
    // the entries are fewer than glyphs_max, inside 32 bits
    compositor->reserved += (embed_u32)entries;
    return 1;
}

// The place on the page where a glyph `width` by `height` sits lowest, then furthest left, into `x_out` and `y_out`.
// The result is the segment where it starts, or the segments of the page where it fits nowhere
EMBED_INLINE embed_u32 atlas_page_fit(const AtlasCtx *args)
{
    const GuiCompositorPage *const page = args->page;
    const embed_u32 side = args->compositor_read->limits.atlas_side;
    embed_u32 best = page->segments;
    embed_u32 best_x = 0u;
    embed_u32 best_y = side;
    for (embed_u32 start = 0u; start < page->segments; start += 1u)
    {
        const embed_u32 x = page->segment_x[start];
        if ((x + args->width) > side)
        {
            break;
        }
        embed_u32 y = 0u;
        embed_u32 covered = 0u;
        embed_u32 at = start;
        while ((covered < args->width) && (at < page->segments))
        {
            y = (page->segment_y[at] > y) ? page->segment_y[at] : y;
            covered += page->segment_width[at];
            at += 1u;
        }
        if ((covered >= args->width) && ((y + args->height) <= side) && (y < best_y))
        {
            best = start;
            best_x = x;
            best_y = y;
        }
    }
    *args->x_out = best_x;
    *args->y_out = best_y;
    return best;
}

// The box of the glyph, `width` by `height`, put on the skyline of the page at the segment `at`. It starts at the left
// of that segment, and sits on its height at `y_out`.
// A new segment goes on its top, and the segments under it get shorter or go. Segments side by side of one height join
EMBED_INLINE void atlas_page_take(const AtlasCtx *args)
{
    GuiCompositorPage *const page = args->page;
    const embed_u32 x = page->segment_x[args->at];
    const embed_u32 right = x + args->width;
    for (embed_u32 move = page->segments; move > args->at; move -= 1u)
    {
        page->segment_x[move] = page->segment_x[move - 1u];
        page->segment_y[move] = page->segment_y[move - 1u];
        page->segment_width[move] = page->segment_width[move - 1u];
    }
    page->segments += 1u;
    // a page's places are at most 4096, inside 16 bits
    page->segment_x[args->at] = (embed_u16)x;
    page->segment_y[args->at] = (embed_u16)(*args->y_out + args->height);
    page->segment_width[args->at] = (embed_u16)args->width;
    embed_u32 next = args->at + 1u;
    while (next < page->segments)
    {
        const embed_u32 start = page->segment_x[next];
        const embed_u32 end = start + page->segment_width[next];
        if (start >= right)
        {
            break;
        }
        if (end > right)
        {
            page->segment_x[next] = (embed_u16)right;
            page->segment_width[next] = (embed_u16)(end - right);
            break;
        }
        for (embed_u32 move = next; (move + 1u) < page->segments; move += 1u)
        {
            page->segment_x[move] = page->segment_x[move + 1u];
            page->segment_y[move] = page->segment_y[move + 1u];
            page->segment_width[move] = page->segment_width[move + 1u];
        }
        page->segments -= 1u;
    }
    for (embed_u32 at = 0u; (at + 1u) < page->segments;)
    {
        if (page->segment_y[at] == page->segment_y[at + 1u])
        {
            page->segment_width[at] = (embed_u16)(page->segment_width[at] + page->segment_width[at + 1u]);
            for (embed_u32 move = at + 1u; (move + 1u) < page->segments; move += 1u)
            {
                page->segment_x[move] = page->segment_x[move + 1u];
                page->segment_y[move] = page->segment_y[move + 1u];
                page->segment_width[move] = page->segment_width[move + 1u];
            }
            page->segments -= 1u;
            continue;
        }
        at += 1u;
    }
}

// The page `at` let go: its skyline cleared, every glyph baked on it no longer baked, and the hash made again from
// the glyphs left
EMBED_INLINE void atlas_page_evict(const AtlasCtx *args)
{
    GuiCompositor *const compositor = args->compositor;
    EMBED_CALL(atlas_page_clear, AtlasCtx, .page = &compositor->page[args->at], .compositor_read = compositor);
    for (embed_u32 entry = 0u; entry < compositor->limits.glyphs_max; entry += 1u)
    {
        if (compositor->entry[entry].baked && (compositor->entry[entry].page == args->at))
        {
            compositor->entry[entry].baked = 0u;
        }
    }
    __builtin_memset(compositor->hash, 0, ((embed_u64)compositor->hash_mask + 1ULL) * sizeof(embed_u32));
    for (embed_u32 entry = compositor->reserved; entry < compositor->limits.glyphs_max; entry += 1u)
    {
        if (compositor->entry[entry].baked)
        {
            EMBED_CALL(atlas_hash_put, AtlasCtx, .compositor = compositor, .at = entry);
        }
    }
}

// A place on a page for a box `width` by `height`, with the page in the place of `at`. The result is the page. It is
// atlas_pages where each page is full and a range keeps each full page. The page with the oldest draw goes first
EMBED_INLINE embed_u32 atlas_room(const AtlasCtx *args)
{
    GuiCompositor *const compositor = args->compositor;
    for (embed_u32 page = 0u; page < compositor->limits.atlas_pages; page += 1u)
    {
        if (EMBED_CALL(atlas_page_fit, AtlasCtx, .page = &compositor->page[page], .compositor_read = compositor,
                       .width = args->width, .height = args->height, .x_out = args->x_out,
                       .y_out = args->y_out) < compositor->page[page].segments)
        {
            return page;
        }
    }
    embed_u32 oldest = compositor->limits.atlas_pages;
    for (embed_u32 page = 0u; page < compositor->limits.atlas_pages; page += 1u)
    {
        if (!compositor->page[page].pinned &&
            ((oldest == compositor->limits.atlas_pages) || (compositor->page[page].stamp < compositor->page[oldest].stamp)))
        {
            oldest = page;
        }
    }
    if (oldest == compositor->limits.atlas_pages)
    {
        return oldest;
    }
    EMBED_CALL(atlas_page_evict, AtlasCtx, .compositor = compositor, .at = oldest);
    const embed_u32 fit = EMBED_CALL(atlas_page_fit, AtlasCtx, .page = &compositor->page[oldest],
                                     .compositor_read = compositor, .width = args->width, .height = args->height,
                                     .x_out = args->x_out, .y_out = args->y_out);
    return (fit < compositor->page[oldest].segments) ? oldest : compositor->limits.atlas_pages;
}

// a free entry past the ranges: GUI_COMPOSITOR_NO_GLYPH where every one holds a glyph
static embed_u32 atlas_entry_free(const GuiCompositor *compositor)
{
    for (embed_u32 at = compositor->reserved; at < compositor->limits.glyphs_max; at += 1u)
    {
        if (!compositor->entry[at].baked)
        {
            return at;
        }
    }
    return GUI_COMPOSITOR_NO_GLYPH;
}

embed_u32 gui_compositor_glyph_bake(const GuiCompositorGlyphBakeCfg *args)
{
    GuiCompositor *const compositor = args->compositor;
    const embed_u32 side = compositor->limits.atlas_side;
    if (((args->width != 0u) && (args->height != 0u) && (args->coverage == NULL)) ||
        ((args->width + ATLAS_GUTTER) > side) || ((args->height + ATLAS_GUTTER) > side))
    {
        return GUI_COMPOSITOR_NO_GLYPH;
    }
    const embed_u64 key = atlas_key(args->face, args->size, args->glyph, args->phase);
    const embed_u32 ranged = atlas_range_entry(compositor, args->face, args->size, args->glyph, args->phase);
    embed_u32 entry = ranged;
    if (entry == GUI_COMPOSITOR_NO_GLYPH)
    {
        entry = EMBED_CALL(atlas_hash_find, AtlasCtx, .compositor_read = compositor, .key = key);
    }
    const int hashed = (ranged == GUI_COMPOSITOR_NO_GLYPH) && (entry == GUI_COMPOSITOR_NO_GLYPH);
    if (hashed)
    {
        entry = atlas_entry_free(compositor);
        if (entry == GUI_COMPOSITOR_NO_GLYPH)
        {
            return entry;
        }
    }
    embed_u32 page = 0u;
    embed_u32 x = 0u;
    embed_u32 y = 0u;
    if ((args->width != 0u) && (args->height != 0u))
    {
        page = EMBED_CALL(atlas_room, AtlasCtx, .compositor = compositor, .width = args->width + ATLAS_GUTTER,
                          .height = args->height + ATLAS_GUTTER, .x_out = &x, .y_out = &y);
        if (page == compositor->limits.atlas_pages)
        {
            return GUI_COMPOSITOR_NO_GLYPH;
        }
        // a page let go to make room can have had this glyph's own entry: the entry is this glyph's again below
        GuiCompositorPage *const taken = &compositor->page[page];
        embed_u32 x_found = x;
        const embed_u32 at = EMBED_CALL(atlas_page_fit, AtlasCtx, .page = taken, .compositor_read = compositor,
                                        .width = args->width + ATLAS_GUTTER, .height = args->height + ATLAS_GUTTER,
                                        .x_out = &x_found, .y_out = &y);
        EMBED_CALL(atlas_page_take, AtlasCtx, .page = taken, .width = args->width + ATLAS_GUTTER,
                   .height = args->height + ATLAS_GUTTER, .at = at, .y_out = &y);
        x = x_found;
        unsigned char *const bytes = compositor->coverage + ((embed_u64)page * side * side);
        for (embed_u32 row = 0u; row < args->height; row += 1u)
        {
            __builtin_memcpy(bytes + ((embed_u64)(y + row) * side) + x, args->coverage + ((embed_u64)row * args->width),
                             args->width);
        }
        taken->pinned |= (ranged != GUI_COMPOSITOR_NO_GLYPH);
        taken->stamp = compositor->stamp;
    }
    GuiCompositorGlyphEntry *const put = &compositor->entry[entry];
    put->key = key;
    // a page's number, its places and a glyph's box are inside 16 bits by the limits
    put->page = (embed_u16)page;
    put->x = (embed_u16)x;
    put->y = (embed_u16)y;
    put->width = (embed_u16)args->width;
    put->height = (embed_u16)args->height;
    // a glyph's box is a few hundred pixels from its pen at the most, inside 16 bits
    put->left = (embed_i16)args->left;
    put->top = (embed_i16)args->top;
    put->advance = args->advance;
    const int was_baked = (put->baked != 0u);
    put->baked = 1u;
    if (hashed || ((ranged == GUI_COMPOSITOR_NO_GLYPH) && !was_baked))
    {
        EMBED_CALL(atlas_hash_put, AtlasCtx, .compositor = compositor, .at = entry);
    }
    return entry;
}
