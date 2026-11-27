// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs.c: the window's text. A face's file is read whole through the caller's faces into its segment of the block
// and its character map saved after it; a glyph is outlined at its pixel size, measured, rasterized as coverage in a cell of its own
// size and saved as bytes in a entry found by its key, every entry let go together as they fill; a glyph in a color is
// a cell of lanes positioned for the one blend, saved the same way, which the text is drawn through
#include "glyphs.h"

#include "block.h"
#include "memoria_operor/memoria_operor.h"
#include "../arith/arith.h"

#include "../utf8/utf8.h"

// the bytes retained clear after a face's file, as the reader reads past a table's end by a word
#define GLYPHS_FILE_SLACK 64ULL

typedef struct
{
    GuiGlyphs *glyphs;
    unsigned int face;
    unsigned int glyph;
    unsigned int size;
    FontContours *contours;
    float reach;
    unsigned int cover;
    const GuiGlyphs *glyphs_read;
    GuiMask *mask_bits;
    unsigned int character;
    GuiColor color;
    float *advance;
} GlyphsCtx;
// the face `face`, read from the file that the system of the caller holds for it. The call reads the ranges of the
// character map through the work part of the glyphs. It then keeps them after the file, in the segment of the face in
// the block. The result is 1, or 0 where the system has no such face. It is also 0 where the file passes the segment,
// or where it holds no font
EMBED_INLINE int glyphs_face_read(const GlyphsCtx *args)
{
    unsigned long long size = 0ULL;
    if (!args->glyphs->faces->size(args->face, &size))
    {
        return 0;
    }
    // the file of the face, kept while the code draws from its font, in the segment of the face in the block. Its slack
    // and its ranges come after it. The call lets go of a file past the segment, and does not read it
    const unsigned long long ranges_bytes = (unsigned long long)FONT_RANGES * sizeof(unsigned int[3]);
    const unsigned long long data_bytes = (size + GLYPHS_FILE_SLACK + 7ULL) & ~7ULL;
    unsigned char *const data = EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_FACES,
                                          .segment = args->face, .bytes = data_bytes + ranges_bytes);
    unsigned int (*const ranges)[3] = (unsigned int (*)[3])(void *)EMBED_CALL(
        gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_GLYPH_WORK, .bytes = ranges_bytes);
    if ((data == NULL) || (ranges == NULL) || !args->glyphs->faces->read(args->face, data, size))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = data + size, .val = 0, .bytes = GLYPHS_FILE_SLACK});
    Font *const font = &args->glyphs->face[args->face];
    if (!EMBED_CALL(font_data.read, FontDataCfg, .font_out = font, .data = data, .size = size, .ranges = ranges))
    {
        return 0;
    }
    const unsigned long long bytes = (unsigned long long)font->ranges * sizeof(ranges[0]);
    // the ranges lie after the file's bytes and slack, on a word's boundary
    unsigned int (*const saved)[3] = (unsigned int (*)[3])(void *)(data + data_bytes);
    memor.cpy(&(MemoriaCfg){.dst = saved, .src = ranges, .bytes = bytes});
    font->range = saved;
    args->glyphs->data[args->face] = data;
    args->glyphs->data_size[args->face] = size;
    return 1;
}

// the cache emptied: every entry's coverage mask freed and every entry marked unused
EMBED_INLINE void glyphs_clear(const GlyphsCtx *args)
{
    for (unsigned int at = 0u; at < GUI_GLYPH_CACHE_ENTRIES; at += 1u)
    {
        GuiCachedGlyph *const entry = &args->glyphs->entry[at];
        if (entry->used)
        {
            entry->mask_bits.cover = NULL;
            entry->used = 0;
        }
    }
    args->glyphs->cached = 0u;
    args->glyphs->entry_order.ordered = 0u;
    args->glyphs->entry_order.run_count = 0u;
    args->glyphs->masks_used = 0ULL;
}

// the cells emptied: every cell's lanes given back and every cell marked unused
EMBED_INLINE void glyphs_cells_clear(const GlyphsCtx *args)
{
    for (unsigned int at = 0u; at < GUI_CELL_CACHE_ENTRIES; at += 1u)
    {
        GuiGlyphCell *const cell = &args->glyphs->cell[at];
        if (cell->used)
        {
            cell->red_blue = NULL;
            cell->green = NULL;
            cell->rest = NULL;
            cell->used = 0;
        }
    }
    args->glyphs->celled = 0u;
    args->glyphs->cell_order.ordered = 0u;
    args->glyphs->cell_order.run_count = 0u;
    args->glyphs->cells_used = 0ULL;
    // the ASCII tables point into the cells: they go with them
    memor.set(&(MemoriaCfg){.dst = args->glyphs->ascii, .val = 0, .bytes = sizeof(args->glyphs->ascii)});
}

int gui_glyphs_init(const GuiGlyphsCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->glyphs, .val = 0, .bytes = sizeof(*args->glyphs)});
    args->glyphs->faces = args->faces;
    // the outline, the cache's entries, the cells and the two orders of their keys lie in the block's tables part, one
    // after another on word boundaries. An order holds the places of its cache, then its run and the room of its sort
    const unsigned long long contours_bytes = (sizeof(*args->glyphs->contours) + 7ULL) & ~7ULL;
    const unsigned long long entries_bytes = (GUI_GLYPH_CACHE_ENTRIES * sizeof(*args->glyphs->entry) + 7ULL) & ~7ULL;
    const unsigned long long cells_bytes = (GUI_CELL_CACHE_ENTRIES * sizeof(*args->glyphs->cell) + 7ULL) & ~7ULL;
    const unsigned long long entry_order_bytes =
        ((GUI_GLYPH_CACHE_ENTRIES + (2ULL * GUI_GLYPH_RUN)) * sizeof(GuiGlyphPlace) + 7ULL) & ~7ULL;
    const unsigned long long cell_order_bytes =
        (GUI_CELL_CACHE_ENTRIES + (2ULL * GUI_GLYPH_RUN)) * sizeof(GuiGlyphPlace);
    unsigned char *const tables =
        EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_GLYPH_TABLES,
                   .bytes = contours_bytes + entries_bytes + cells_bytes + entry_order_bytes + cell_order_bytes);
    if ((args->faces == NULL) || (tables == NULL))
    {
        gui_glyphs.free(args);
        mmgr_line_put(args->err_msg, "the cells the window's glyphs are retained in could not be taken");
        return 0;
    }
    args->glyphs->contours = (FontContours *)(void *)tables;
    args->glyphs->entry = (GuiCachedGlyph *)(void *)(tables + contours_bytes);
    args->glyphs->cell = (GuiGlyphCell *)(void *)(tables + contours_bytes + entries_bytes);
    GuiGlyphPlace *const entry_places =
        (GuiGlyphPlace *)(void *)(tables + contours_bytes + entries_bytes + cells_bytes);
    GuiGlyphPlace *const cell_places =
        (GuiGlyphPlace *)(void *)(tables + contours_bytes + entries_bytes + cells_bytes + entry_order_bytes);
    args->glyphs->entry_order = (GuiGlyphOrder){entry_places, 0u, entry_places + GUI_GLYPH_CACHE_ENTRIES, 0u,
                                                entry_places + GUI_GLYPH_CACHE_ENTRIES + GUI_GLYPH_RUN};
    args->glyphs->cell_order = (GuiGlyphOrder){cell_places, 0u, cell_places + GUI_CELL_CACHE_ENTRIES, 0u,
                                               cell_places + GUI_CELL_CACHE_ENTRIES + GUI_GLYPH_RUN};
    memor.set(&(MemoriaCfg){.dst = args->glyphs->contours, .val = 0, .bytes = sizeof(*args->glyphs->contours)});
    memor.set(&(MemoriaCfg){
        .dst = args->glyphs->entry, .val = 0, .bytes = GUI_GLYPH_CACHE_ENTRIES * sizeof(*args->glyphs->entry)});
    memor.set(&(MemoriaCfg){
        .dst = args->glyphs->cell, .val = 0, .bytes = GUI_CELL_CACHE_ENTRIES * sizeof(*args->glyphs->cell)});
    for (unsigned int face = GUI_FACE_CODE; face <= GUI_FACE_CODE_BOLD_ITALIC; face += 1u)
    {
        args->glyphs->has[face] = EMBED_CALL(glyphs_face_read, GlyphsCtx, .glyphs = args->glyphs, .face = face);
    }
    args->glyphs->has[GUI_FACE_HEAD] =
        EMBED_CALL(glyphs_face_read, GlyphsCtx, .glyphs = args->glyphs, .face = GUI_FACE_HEAD);
    args->glyphs->has[GUI_FACE_TEXT] =
        EMBED_CALL(glyphs_face_read, GlyphsCtx, .glyphs = args->glyphs, .face = GUI_FACE_TEXT);
    if (!args->glyphs->has[GUI_FACE_CODE])
    {
        gui_glyphs.free(args);
        mmgr_line_put(args->err_msg, "the system has no monospace font the window reads");
        return 0;
    }
    return 1;
}

void gui_glyphs_free(const GuiGlyphsCfg *args)
{
    if (args->glyphs->entry != NULL)
    {
        EMBED_CALL(glyphs_clear, GlyphsCtx, .glyphs = args->glyphs);
    }
    if (args->glyphs->cell != NULL)
    {
        EMBED_CALL(glyphs_cells_clear, GlyphsCtx, .glyphs = args->glyphs);
    }
    args->glyphs->entry = NULL;
    args->glyphs->cell = NULL;
    args->glyphs->contours = NULL;
    for (unsigned int face = 0u; face < GUI_FACES; face += 1u)
    {
        args->glyphs->has[face] = 0;
        args->glyphs->data[face] = NULL;
    }
}

int gui_glyphs_head_bold_read(const GuiGlyphsCfg *args)
{
    if (args->glyphs->has[GUI_FACE_HEAD_BOLD])
    {
        return 0;
    }
    args->glyphs->has[GUI_FACE_HEAD_BOLD] =
        EMBED_CALL(glyphs_face_read, GlyphsCtx, .glyphs = args->glyphs, .face = GUI_FACE_HEAD_BOLD);
    return args->glyphs->has[GUI_FACE_HEAD_BOLD];
}

void gui_glyphs_head_bold_free(const GuiGlyphsCfg *args)
{
    // the masks of its characters go with it. The call lets go of each mask, and the code draws the others again when a
    // caller asks for them
    EMBED_CALL(glyphs_clear, GlyphsCtx, .glyphs = args->glyphs);
    // its file and ranges lie in the segment of the face in the block. The next read of the face takes them again
    args->glyphs->has[GUI_FACE_HEAD_BOLD] = 0;
    args->glyphs->data[GUI_FACE_HEAD_BOLD] = NULL;
}

// the symbol face, read from the file that the system of the caller holds for it. The call looks for it once. The
// result is 1 where the read succeeds
EMBED_INLINE int glyphs_symbol_read(const GlyphsCtx *args)
{
    GuiGlyphs *const glyphs = args->glyphs;
    if (glyphs->symbol_looked)
    {
        return glyphs->has[GUI_FACE_SYMBOL];
    }
    glyphs->symbol_looked = 1;
    glyphs->has[GUI_FACE_SYMBOL] = EMBED_CALL(glyphs_face_read, GlyphsCtx, .glyphs = glyphs, .face = GUI_FACE_SYMBOL);
    return glyphs->has[GUI_FACE_SYMBOL];
}

unsigned int gui_face_had(const GuiGlyphsFaceCfg *args)
{
    if ((args->face == GUI_FACE_HEAD_BOLD) && !args->glyphs_read->has[args->face] &&
        args->glyphs_read->has[GUI_FACE_HEAD])
    {
        return GUI_FACE_HEAD;
    }
    return ((args->face < GUI_FACES) && args->glyphs_read->has[args->face]) ? args->face : GUI_FACE_CODE;
}

// the order of two places, by their keys, the face first: below 0, 0 or above 0
static int glyphs_place_order(const void *left, const void *right)
{
    const GuiGlyphPlace *const one = (const GuiGlyphPlace *)left;
    const GuiGlyphPlace *const other = (const GuiGlyphPlace *)right;
    for (unsigned int part = 0u; part < 4u; part += 1u)
    {
        if (one->key[part] != other->key[part])
        {
            return (one->key[part] < other->key[part]) ? -1 : 1;
        }
    }
    return 0;
}

// the place of the key `key` in the cache of `order`, GUI_GLYPH_NONE where the cache holds none: the run first, then
// the order by a binary search
static unsigned int glyphs_place_found(const GuiGlyphOrder *order, const GuiGlyphPlace *key)
{
    for (unsigned int each = 0u; each < order->run_count; each += 1u)
    {
        if (glyphs_place_order(&order->run[each], key) == 0)
        {
            return order->run[each].place;
        }
    }
    unsigned int low = 0u;
    unsigned int high = order->ordered;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (glyphs_place_order(&order->order[middle], key) < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < order->ordered) && (glyphs_place_order(&order->order[low], key) == 0)) ? order->order[low].place
                                                                                          : GUI_GLYPH_NONE;
}

// the key `key` noted in `order`: into the run, the run sorted once and merged into the order from its end first where
// it is full
static void glyphs_place_noted(GuiGlyphOrder *order, GuiGlyphPlace key)
{
    if (order->run_count == GUI_GLYPH_RUN)
    {
        (void)memor.sort(&(MemoriaCfg){.dst = order->run,
                                       .scratch = order->room,
                                       .count = order->run_count,
                                       .bytes = sizeof(GuiGlyphPlace),
                                       .order = glyphs_place_order});
        unsigned int from_order = order->ordered;
        unsigned int from_run = order->run_count;
        unsigned int to = order->ordered + order->run_count;
        while (from_run != 0u)
        {
            const int order_first = (from_order != 0u) && (glyphs_place_order(&order->order[from_order - 1u],
                                                                              &order->run[from_run - 1u]) > 0);
            to -= 1u;
            if (order_first)
            {
                from_order -= 1u;
                order->order[to] = order->order[from_order];
            }
            else
            {
                from_run -= 1u;
                order->order[to] = order->run[from_run];
            }
        }
        order->ordered += order->run_count;
        order->run_count = 0u;
    }
    order->run[order->run_count] = key;
    order->run_count += 1u;
}

// the cache entry keyed by `face`, `glyph` and `size`, NULL where the cache holds none. Where it holds none and three
// quarters of its entries are in use, the call first empties the cache
EMBED_INLINE GuiCachedGlyph *glyphs_entry(const GlyphsCtx *args)
{
    const GuiGlyphPlace key = {{args->face, args->glyph, args->size, 0u}, 0u};
    const unsigned int place = glyphs_place_found(&args->glyphs->entry_order, &key);
    if (place != GUI_GLYPH_NONE)
    {
        return &args->glyphs->entry[place];
    }
    if ((args->glyphs->cached * 4u) >= (GUI_GLYPH_CACHE_ENTRIES * 3u))
    {
        glyphs_clear(args);
    }
    return NULL;
}

// the stem dark at `size` pixels an em: how far the code pushes out each side of a stroke, in pixels. It is
// GUI_DARKEN_MAX at GUI_DARKEN_SMALL pixels and below. It falls in a straight line to GUI_DARKEN_MIN at
// GUI_DARKEN_LARGE and above. The thin stems of a small glyph need the max to read at full weight
EMBED_INLINE float glyphs_darkening(const GlyphsCtx *args)
{
    if (args->size <= GUI_DARKEN_SMALL)
    {
        return GUI_DARKEN_MAX;
    }
    if (args->size >= GUI_DARKEN_LARGE)
    {
        return GUI_DARKEN_MIN;
    }
    const float along = (float)(args->size - GUI_DARKEN_SMALL) / (float)(GUI_DARKEN_LARGE - GUI_DARKEN_SMALL);
    return GUI_DARKEN_MAX + ((GUI_DARKEN_MIN - GUI_DARKEN_MAX) * along);
}

// the strokes of the contours, expanded by `reach` pixels a side. Each point moves along the miter of the normals of
// its two edges, to the side away from the ink. The ink lies to the right of the travel of a contour, as TrueType
// winds it with y up.
//
// An outer contour grows and a counter shrinks, and both make the stroke wider. At a sharp turn, the miter stops at
// twice the reach
EMBED_INLINE void glyphs_darken(const GlyphsCtx *args)
{
    unsigned int first = 0u;
    for (unsigned int contour = 0u; contour < args->contours->contours; contour += 1u)
    {
        const unsigned int end = args->contours->end[contour];
        const unsigned int count = (end > first) ? end - first : 0u;
        if (count < 3u)
        {
            first = end;
            continue;
        }
        float moved_x[FONT_OUTLINE_POINTS];
        float moved_y[FONT_OUTLINE_POINTS];
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const unsigned int here = first + at;
            const unsigned int before = first + ((at + count - 1u) % count);
            const unsigned int after = first + ((at + 1u) % count);
            float in_x = args->contours->x[here] - args->contours->x[before];
            float in_y = args->contours->y[here] - args->contours->y[before];
            float out_x = args->contours->x[after] - args->contours->x[here];
            float out_y = args->contours->y[after] - args->contours->y[here];
            const float in_length = arith_root_f32((in_x * in_x) + (in_y * in_y));
            const float out_length = arith_root_f32((out_x * out_x) + (out_y * out_y));
            in_x = (in_length > 0.0f) ? in_x / in_length : 0.0f;
            in_y = (in_length > 0.0f) ? in_y / in_length : 0.0f;
            out_x = (out_length > 0.0f) ? out_x / out_length : 0.0f;
            out_y = (out_length > 0.0f) ? out_y / out_length : 0.0f;
            // each edge's normal to the left of its travel, away from the ink, and their miter
            const float normal_x = -(in_y + out_y);
            const float normal_y = in_x + out_x;
            const float dot = 1.0f + (in_x * out_x) + (in_y * out_y);
            const float scale = (dot > 0.5f) ? (2.0f * args->reach) / dot : 4.0f * args->reach;
            moved_x[at] = args->contours->x[here] + (normal_x * scale * 0.5f);
            moved_y[at] = args->contours->y[here] + (normal_y * scale * 0.5f);
        }
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            args->contours->x[first + at] = moved_x[at];
            args->contours->y[first + at] = moved_y[at];
        }
        first = end;
    }
}

// the coverage, lifted through the gamma of the blend. The code raises each share of 255 to one over GUI_TEXT_GAMMA,
// in a table made once. That brings the partly covered edge pixels of a stroke up toward its whole pixels. The text
// then reads crisper and bolder
static unsigned char s_lifted[256];
static int s_lifted_created;

EMBED_INLINE unsigned int glyphs_lift(const GlyphsCtx *args)
{
    if (!s_lifted_created)
    {
        for (unsigned int at = 0u; at < 256u; at += 1u)
        {
            // a share from 0 to 1 raised and scaled to 255, rounded, is a byte
            s_lifted[at] =
                (unsigned char)((arith_pow_f32((float)at / 255.0f, 1.0f / GUI_TEXT_GAMMA) * 255.0f) + 0.5f);
        }
        s_lifted_created = 1;
    }
    return s_lifted[args->cover & 0xffu];
}

// the outline in the contours of the glyphs, drawn as coverage bytes into `mask`, with its cover in a cell of its own.
// The result is 1, or 0 where the call cannot take a cell. A glyph of no pixels is a mask of none
EMBED_INLINE int glyphs_rasterize(const GlyphsCtx *args)
{
    GuiMask *mask_bits = args->mask_bits;
    FontMask measured;
    EMBED_CALL(font_contours.rasterize, FontContoursCfg, .contours = args->glyphs_read->contours,
               .mask_bits = &measured, .cover = NULL, .capacity = 0ULL);
    mask_bits->width = 0u;
    mask_bits->height = 0u;
    mask_bits->left = 0;
    mask_bits->top = 0;
    mask_bits->cover = NULL;
    if (measured.width == 0ULL)
    {
        return 1;
    }
    const unsigned long long capacity = (measured.width * measured.height) + 4ULL;
    // the coverage floats and the edges' scratch lie in the glyphs' work part of the block, one glyph's at a time
    const unsigned long long scratch_bytes =
        EMBED_CALL(font_contours.bytes, FontContoursCfg, .contours = args->glyphs_read->contours);
    float *const floats = (float *)(void *)EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_GLYPH_WORK,
                                                     .bytes = (capacity * sizeof(*floats)) + scratch_bytes);
    // the cover comes from the masks ring. Where the ring has no room for it, the call first empties the ring, as it
    // empties the cache of its entries
    unsigned char *cover = EMBED_CALL(gui_block_part.take, GuiBlockPartCfg, .part = GUI_BLOCK_GLYPH_MASKS,
                                      .cursor = &args->glyphs->masks_used, .bytes = measured.width * measured.height);
    if (cover == NULL)
    {
        EMBED_CALL(glyphs_clear, GlyphsCtx, .glyphs = args->glyphs);
        cover = EMBED_CALL(gui_block_part.take, GuiBlockPartCfg, .part = GUI_BLOCK_GLYPH_MASKS,
                           .cursor = &args->glyphs->masks_used, .bytes = measured.width * measured.height);
    }
    if ((floats == NULL) || (cover == NULL))
    {
        return 0;
    }
    FontMask drawn;
    EMBED_CALL(font_contours.rasterize, FontContoursCfg, .contours = args->glyphs_read->contours,
               .mask_bits = &drawn, .cover = floats, .capacity = capacity, .scratch = floats + capacity,
               .scratch_bytes = scratch_bytes);
    for (unsigned long long at = 0ULL; at < (drawn.width * drawn.height); at += 1ULL)
    {
        // a coverage from 0 to 1 by 255, rounded, is a byte
        cover[at] = (unsigned char)((floats[at] * 255.0f) + 0.5f);
    }
    // a glyph's sides are far inside what an unsigned int represents
    mask_bits->width = (unsigned int)drawn.width;
    mask_bits->height = (unsigned int)drawn.height;
    mask_bits->left = drawn.left;
    mask_bits->top = drawn.top;
    mask_bits->cover = cover;
    return 1;
}

// The glyph of `character` in the face that draws `face`, into `glyph_out`, and that face into `face_out`. Where its
// face does not hold the character and the symbol face does, the symbol face draws it. The text of the system does
// the same
static void glyphs_found(GuiGlyphs *glyphs, unsigned int face, unsigned int character, unsigned int *face_out,
                         unsigned int *glyph_out)
{
    unsigned int had = EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face);
    unsigned int glyph =
        EMBED_CALL(font_character.glyph, FontCfg, .font = &glyphs->face[had], .character = character);
    if ((glyph == 0u) && (character > 0x20u) && (had != GUI_FACE_SYMBOL) &&
        EMBED_CALL(glyphs_symbol_read, GlyphsCtx, .glyphs = glyphs))
    {
        const unsigned int symbol =
            EMBED_CALL(font_character.glyph, FontCfg, .font = &glyphs->face[GUI_FACE_SYMBOL], .character = character);
        if (symbol != 0u)
        {
            had = GUI_FACE_SYMBOL;
            glyph = symbol;
        }
    }
    *face_out = had;
    *glyph_out = glyph;
}

const GuiMask *gui_glyph(const GuiGlyphsFaceLetterCfg *args)
{
    unsigned int size = args->size;
    float *advance = args->advance;
    unsigned int had = 0u;
    unsigned int glyph = 0u;
    glyphs_found(args->glyphs, args->face, args->character, &had, &glyph);
    const Font *const font = &args->glyphs->face[had];
    const float scale = (float)size / font->units;
    *advance = EMBED_CALL(font_glyph_table.advance, FontGlyphCfg, .font = font, .glyph = glyph) * scale;
    GuiCachedGlyph *const found =
        EMBED_CALL(glyphs_entry, GlyphsCtx, .glyphs = args->glyphs, .face = had, .glyph = glyph, .size = size);
    if (found != NULL)
    {
        return &found->mask_bits;
    }
    // the entry is made in this call's own record, and takes the next place of the cache when it is whole. The mask's
    // ring can empty the cache as it is made
    GuiCachedGlyph made;
    memor.set(&(MemoriaCfg){.dst = &made, .val = 0, .bytes = sizeof(made)});
    GuiCachedGlyph *const entry = &made;
    args->glyphs->contours->points = 0u;
    args->glyphs->contours->contours = 0u;
    const float matrix[6] = {scale, 0.0f, 0.0f, scale, 0.0f, 0.0f};
    EMBED_CALL(font_glyph_matrix.outline, FontGlyphMatrixCfg, .font = font, .glyph = glyph, .matrix = matrix,
               .depth = 0u, .out = args->glyphs->contours);
    EMBED_CALL(glyphs_darken, GlyphsCtx, .contours = args->glyphs->contours,
               .reach = EMBED_CALL(glyphs_darkening, GlyphsCtx, .size = size));
    if (!EMBED_CALL(glyphs_rasterize, GlyphsCtx, .glyphs = args->glyphs, .glyphs_read = args->glyphs,
                    .mask_bits = &entry->mask_bits))
    {
        return NULL;
    }
    entry->used = 1;
    entry->face = had;
    entry->glyph = glyph;
    entry->size = size;
    entry->advance = *advance;
    const unsigned int place = args->glyphs->cached;
    args->glyphs->entry[place] = made;
    glyphs_place_noted(&args->glyphs->entry_order, (GuiGlyphPlace){{had, glyph, size, 0u}, place});
    args->glyphs->cached += 1u;
    return &args->glyphs->entry[place].mask_bits;
}

embed_u32 gui_glyph_bake(const GuiGlyphsBakeCfg *args)
{
    GuiGlyphs *const glyphs = args->glyphs;
    const unsigned int phase = args->phase & 3u;
    const embed_u32 found = EMBED_CALL(gui_compositor_atlas.find, GuiCompositorGlyphFindCfg,
                                       .compositor_read = args->compositor, .face = args->face, .size = args->size,
                                       .glyph = args->character, .phase = phase);
    if (found != GUI_COMPOSITOR_NO_GLYPH)
    {
        return found;
    }
    unsigned int had = 0u;
    unsigned int glyph = 0u;
    glyphs_found(glyphs, args->face, args->character, &had, &glyph);
    const Font *const font = &glyphs->face[had];
    const float scale = (float)args->size / font->units;
    const float advance = EMBED_CALL(font_glyph_table.advance, FontGlyphCfg, .font = font, .glyph = glyph) * scale;
    glyphs->contours->points = 0u;
    glyphs->contours->contours = 0u;
    // the outline moves right a quarter of a pixel for each place
    const float matrix[6] = {scale, 0.0f, 0.0f, scale, (float)phase * 0.25f, 0.0f};
    EMBED_CALL(font_glyph_matrix.outline, FontGlyphMatrixCfg, .font = font, .glyph = glyph, .matrix = matrix,
               .depth = 0u, .out = glyphs->contours);
    EMBED_CALL(glyphs_darken, GlyphsCtx, .contours = glyphs->contours,
               .reach = EMBED_CALL(glyphs_darkening, GlyphsCtx, .size = args->size));
    FontMask measured;
    EMBED_CALL(font_contours.rasterize, FontContoursCfg, .contours = glyphs->contours, .mask_bits = &measured,
               .cover = NULL, .capacity = 0ULL);
    // an advance in pixels is a few hundred at the most, far inside 16.16
    const embed_i32 fixed_advance = (embed_i32)((advance * 65536.0f) + 0.5f);
    if (measured.width == 0ULL)
    {
        return EMBED_CALL(gui_compositor_atlas.bake, GuiCompositorGlyphBakeCfg, .compositor = args->compositor,
                          .face = args->face, .size = args->size, .glyph = args->character, .phase = phase,
                          .coverage = NULL, .width = 0u, .height = 0u, .left = 0, .top = 0, .advance = fixed_advance);
    }
    const unsigned long long pixels = measured.width * measured.height;
    const unsigned long long capacity = pixels + 4ULL;
    const unsigned long long scratch_bytes = EMBED_CALL(font_contours.bytes, FontContoursCfg, .contours = glyphs->contours);
    // the coverage floats, the edges' scratch and the coverage bytes lie in the glyphs' work part, one glyph's at a time
    float *const floats = (float *)(void *)EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_GLYPH_WORK,
                                                     .bytes = (capacity * sizeof(*floats)) + scratch_bytes + pixels);
    if (floats == NULL)
    {
        return GUI_COMPOSITOR_NO_GLYPH;
    }
    FontMask drawn;
    EMBED_CALL(font_contours.rasterize, FontContoursCfg, .contours = glyphs->contours, .mask_bits = &drawn,
               .cover = floats, .capacity = capacity, .scratch = floats + capacity, .scratch_bytes = scratch_bytes);
    unsigned char *const bytes = (unsigned char *)(void *)(floats + capacity) + scratch_bytes;
    for (unsigned long long at = 0ULL; at < (drawn.width * drawn.height); at += 1ULL)
    {
        // a coverage from 0 to 1 by 255, rounded, is a byte
        bytes[at] = (unsigned char)((floats[at] * 255.0f) + 0.5f);
    }
    // a glyph's sides are far inside 32 bits
    return EMBED_CALL(gui_compositor_atlas.bake, GuiCompositorGlyphBakeCfg, .compositor = args->compositor,
                      .face = args->face, .size = args->size, .glyph = args->character, .phase = phase,
                      .coverage = bytes, .width = (embed_u32)drawn.width, .height = (embed_u32)drawn.height,
                      .left = drawn.left, .top = drawn.top, .advance = fixed_advance);
}

float gui_glyphs_ascent(const GuiGlyphsFaceCfg *args)
{
    const Font *const font = &args->glyphs_read->face[gui_glyphs_face.face_had(args)];
    return font->ascent * (float)args->size / font->units;
}

float gui_glyphs_descent(const GuiGlyphsFaceCfg *args)
{
    const Font *const font = &args->glyphs_read->face[gui_glyphs_face.face_had(args)];
    return arith_abs_f32(font->descent) * (float)args->size / font->units;
}

int gui_glyphs_baseline(const GuiGlyphsFaceCfg *args)
{
    const float ascent = gui_glyphs_face.ascent(args);
    const float descent = gui_glyphs_face.descent(args);
    // a baseline is far inside what an int represents
    return args->top + (int)((((float)args->pixel_height - ascent - descent) / 2.0f) + ascent + 0.5f);
}

float gui_glyphs_cell(const GuiGlyphsFaceLetterCfg *args)
{
    float advance = 0.0f;
    (void)EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = args->glyphs,
                     .face = GUI_FACE_CODE, .character = 'M', .size = args->size, .advance = &advance);
    return advance;
}

// the cell keyed by `face`, `character`, `size` and `color`, NULL where the cells hold none. Where they hold none and
// three quarters of the cells are in use, the call first empties them
EMBED_INLINE GuiGlyphCell *glyphs_cell_entry(const GlyphsCtx *args)
{
    const GuiGlyphPlace key = {{args->face, args->character, args->size, args->color}, 0u};
    const unsigned int place = glyphs_place_found(&args->glyphs->cell_order, &key);
    if (place != GUI_GLYPH_NONE)
    {
        return &args->glyphs->cell[place];
    }
    if ((args->glyphs->celled * 4u) >= (GUI_CELL_CACHE_ENTRIES * 3u))
    {
        glyphs_cells_clear(args);
    }
    return NULL;
}

static const GuiGlyphCell *glyphs_cell_init(const GlyphsCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    unsigned int face = args->face;
    unsigned int character = args->character;
    unsigned int size = args->size;
    GuiColor color = args->color;
    float *advance = args->advance;
    const unsigned int had =
        EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face);
    GuiGlyphCell *const found = EMBED_CALL(glyphs_cell_entry, GlyphsCtx, .glyphs = glyphs, .face = had,
                                           .character = character, .size = size, .color = color);
    if (found != NULL)
    {
        *advance = found->advance;
        return found;
    }
    // the cell is made in this call's own record, and takes the next place of the cells when it is whole. The lanes'
    // ring can empty the cells as it is made
    GuiGlyphCell made;
    memor.set(&(MemoriaCfg){.dst = &made, .val = 0, .bytes = sizeof(made)});
    GuiGlyphCell *const cell = &made;
    const GuiMask *const mask_bits = EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = glyphs,
                                                .face = had, .character = character, .size = size, .advance = advance);
    if (mask_bits == NULL)
    {
        return NULL;
    }
    cell->face = had;
    cell->character = character;
    cell->size = size;
    cell->color = color;
    cell->advance = *advance;
    cell->width = mask_bits->width;
    cell->height = mask_bits->height;
    cell->left = mask_bits->left;
    cell->top = mask_bits->top;
    cell->red_blue = NULL;
    cell->green = NULL;
    cell->rest = NULL;
    const unsigned long long pixels = (unsigned long long)mask_bits->width * mask_bits->height;
    if (pixels != 0ULL)
    {
        // the lanes come from the cells ring. Where the ring has no room for them, the call first empties the ring, as it
        // empties the cache of its cells. This cell is not among them yet
        cell->red_blue = (unsigned int *)(void *)EMBED_CALL(gui_block_part.take, GuiBlockPartCfg,
                                                            .part = GUI_BLOCK_GLYPH_CELLS, .cursor = &glyphs->cells_used,
                                                            .bytes = 3ULL * pixels * sizeof(unsigned int));
        if (cell->red_blue == NULL)
        {
            EMBED_CALL(glyphs_cells_clear, GlyphsCtx, .glyphs = glyphs);
            cell->red_blue = (unsigned int *)(void *)EMBED_CALL(
                gui_block_part.take, GuiBlockPartCfg, .part = GUI_BLOCK_GLYPH_CELLS, .cursor = &glyphs->cells_used,
                .bytes = 3ULL * pixels * sizeof(unsigned int));
        }
        if (cell->red_blue == NULL)
        {
            return NULL;
        }
        cell->green = cell->red_blue + pixels;
        cell->rest = cell->green + pixels;
        // the share of the color of each pixel is its cover by the alpha of the color. Two shares of 255 multiplied and
        // divided by 255 give a share of 255. The lanes are the color's channels by the share with half of 255 in, as the
        // raster's blend positions them
        const unsigned int alpha = GUI_ALPHA_OF(color);
        for (unsigned long long at = 0ULL; at < pixels; at += 1ULL)
        {
            const unsigned int share =
                ((EMBED_CALL(glyphs_lift, GlyphsCtx, .cover = mask_bits->cover[at]) * alpha) + 127u) / 255u;
            cell->red_blue[at] = ((color & 0x00ff00ffu) * share) + 0x00800080u;
            cell->green[at] = (((color >> 8u) & 0xffu) * share) + 0x80u;
            cell->rest[at] = 255u - share;
        }
    }
    cell->used = 1;
    const unsigned int place = glyphs->celled;
    glyphs->cell[place] = made;
    glyphs_place_noted(&glyphs->cell_order, (GuiGlyphPlace){{had, character, size, color}, place});
    glyphs->celled += 1u;
    return &glyphs->cell[place];
}

// the ASCII table keyed by `face`, `size` and `color`. It is the last table found, where that table has the key. Else
// the call finds it among the tables. Else it takes the next table in turn and clears it for the key
EMBED_INLINE GuiAsciiTable *glyphs_ascii_table(const GlyphsCtx *args)
{
    unsigned int face = args->face;
    unsigned int size = args->size;
    GuiColor color = args->color;
    GuiAsciiTable *const last = &args->glyphs->ascii[args->glyphs->ascii_last];
    if (last->used && (last->face == face) && (last->size == size) && (last->color == color))
    {
        return last;
    }
    for (unsigned int at = 0u; at < GUI_ASCII_TABLES; at += 1u)
    {
        GuiAsciiTable *const table = &args->glyphs->ascii[at];
        if (table->used && (table->face == face) && (table->size == size) && (table->color == color))
        {
            args->glyphs->ascii_last = at;
            return table;
        }
    }
    const unsigned int reserved = args->glyphs->ascii_next;
    args->glyphs->ascii_next = (reserved + 1u) % GUI_ASCII_TABLES;
    GuiAsciiTable *const table = &args->glyphs->ascii[reserved];
    memor.set(&(MemoriaCfg){.dst = table, .val = 0, .bytes = sizeof(*table)});
    table->used = 1;
    table->face = face;
    table->size = size;
    table->color = color;
    args->glyphs->ascii_last = reserved;
    return table;
}

const GuiGlyphCell *gui_glyph_cell(const GuiGlyphsFaceLetterCfg *args)
{
    float *advance = args->advance;
    if ((args->character < GUI_ASCII_FIRST) || (args->character >= GUI_ASCII_FIRST + GUI_ASCII_COUNT))
    {
        return EMBED_CALL(glyphs_cell_init, GlyphsCtx, .glyphs = args->glyphs, .face = args->face,
                          .character = args->character, .size = args->size, .color = args->color, .advance = advance);
    }
    const unsigned int had =
        EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs, .face = args->face);
    GuiAsciiTable *const table = EMBED_CALL(glyphs_ascii_table, GlyphsCtx, .glyphs = args->glyphs, .face = had,
                                            .size = args->size, .color = args->color);
    const unsigned int at = args->character - GUI_ASCII_FIRST;
    if (table->cell[at] != NULL)
    {
        *advance = table->advance[at];
        return table->cell[at];
    }
    const unsigned int cells_before = args->glyphs->celled;
    const GuiGlyphCell *const cell =
        EMBED_CALL(glyphs_cell_init, GlyphsCtx, .glyphs = args->glyphs, .face = had, .character = args->character,
                   .size = args->size, .color = args->color, .advance = advance);
    // a new cell can empty each cell, and each table with them. The call finds the table again before it fills it
    GuiAsciiTable *const saved = (args->glyphs->celled < cells_before)
                                     ? EMBED_CALL(glyphs_ascii_table, GlyphsCtx, .glyphs = args->glyphs, .face = had,
                                                  .size = args->size, .color = args->color)
                                     : table;
    saved->cell[at] = cell;
    saved->advance[at] = *advance;
    return cell;
}

void gui_glyph_cell_layout(const GuiGlyphsRasterCfg *args)
{
    if (args->cell->red_blue != NULL)
    {
        EMBED_CALL(gui_raster_width_x.lanes_layout, GuiRasterWidthXCfg, .raster = args->raster,
                   .red_blue = args->cell->red_blue, .green = args->cell->green, .rest = args->cell->rest,
                   .width = args->cell->width, .height = args->cell->height, .x = args->x + args->cell->left,
                   .y = args->y - args->cell->top, .clip = args->clip);
    }
}

float gui_text_draw(const GuiGlyphsFaceSizeCfg *args)
{
    float x = args->x_f32;
    unsigned long long at = 0ULL;
    while (at < args->length)
    {
        unsigned long long consumed = 0ULL;
        const unsigned int character = EMBED_CALL(utf8_at.character, Utf8AtCfg, .text = args->text,
                                               .size = args->length, .at = at, .length = &consumed);
        at += (consumed != 0ULL) ? consumed : 1ULL;
        float advance = 0.0f;
        const GuiGlyphCell *const cell = EMBED_CALL(gui_glyphs_face_character.glyph_cell, GuiGlyphsFaceLetterCfg,
                                                    .glyphs = args->glyphs, .face = args->face, .character = character,
                                                    .size = args->size, .color = args->color, .advance = &advance);
        if (cell != NULL)
        {
            // a pixel's place, rounded, is far inside what an int represents
            EMBED_CALL(gui_glyphs_raster.glyph_cell_layout, GuiGlyphsRasterCfg, .raster = args->raster, .cell = cell,
                       .x = (int)arith_round_f32(x), .y = args->y, .clip = args->clip);
        }
        x += advance;
    }
    return x;
}

float gui_text_width(const GuiGlyphsFaceSizeCfg *args)
{
    float width = 0.0f;
    unsigned long long at = 0ULL;
    while (at < args->length)
    {
        unsigned long long consumed = 0ULL;
        const unsigned int character = EMBED_CALL(utf8_at.character, Utf8AtCfg, .text = args->text,
                                               .size = args->length, .at = at, .length = &consumed);
        at += (consumed != 0ULL) ? consumed : 1ULL;
        float advance = 0.0f;
        (void)EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = args->glyphs,
                         .face = args->face, .character = character, .size = args->size, .advance = &advance);
        width += advance;
    }
    return width;
}
