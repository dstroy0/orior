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
// the face `face` read from the file the caller's system holds for it, the character map's ranges read through the
// glyphs' work part and then saved after the file in the face's segment of the block: 1, or 0 where the system has no
// such face, the file passes the segment or no font is in it
EMBED_INLINE int glyphs_face_read(const GlyphsCtx *args)
{
    unsigned long long size = 0ULL;
    if (!args->glyphs->faces->size(args->face, &size))
    {
        return 0;
    }
    // the face's file saved as long as its font is drawn from, in the face's segment of the block with its slack and
    // its ranges after it; a file past the segment is let go unread
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
    args->glyphs->cells_used = 0ULL;
    // the ASCII tables point into the cells: they go with them
    memor.set(&(MemoriaCfg){.dst = args->glyphs->ascii, .val = 0, .bytes = sizeof(args->glyphs->ascii)});
}

int gui_glyphs_init(const GuiGlyphsCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->glyphs, .val = 0, .bytes = sizeof(*args->glyphs)});
    args->glyphs->faces = args->faces;
    // the outline, the cache's entries and the cells lie in the block's tables part, one after another on word boundaries
    const unsigned long long contours_bytes = (sizeof(*args->glyphs->contours) + 7ULL) & ~7ULL;
    const unsigned long long entries_bytes = (GUI_GLYPH_CACHE_ENTRIES * sizeof(*args->glyphs->entry) + 7ULL) & ~7ULL;
    const unsigned long long cells_bytes = GUI_CELL_CACHE_ENTRIES * sizeof(*args->glyphs->cell);
    unsigned char *const tables = EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_GLYPH_TABLES,
                                             .bytes = contours_bytes + entries_bytes + cells_bytes);
    if ((args->faces == NULL) || (tables == NULL))
    {
        gui_glyphs.free(args);
        mmgr_line_put(args->err_msg, "the cells the window's glyphs are retained in could not be taken");
        return 0;
    }
    args->glyphs->contours = (FontContours *)(void *)tables;
    args->glyphs->entry = (GuiCachedGlyph *)(void *)(tables + contours_bytes);
    args->glyphs->cell = (GuiGlyphCell *)(void *)(tables + contours_bytes + entries_bytes);
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
    // its characters' masks go with it: every mask let go, the others drawn again as they are asked for
    EMBED_CALL(glyphs_clear, GlyphsCtx, .glyphs = args->glyphs);
    // its file and ranges lie in the face's segment of the block, taken again as the face is read again
    args->glyphs->has[GUI_FACE_HEAD_BOLD] = 0;
    args->glyphs->data[GUI_FACE_HEAD_BOLD] = NULL;
}

// the symbol face read from the file the caller's system holds for it, looked for once: 1 where it is read
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

// the cache entry keyed by `face`, `glyph` and `size`, or an unused entry for it; the cache is emptied first when
// three quarters of its entries are in use
EMBED_INLINE GuiCachedGlyph *glyphs_entry(const GlyphsCtx *args)
{
    const unsigned int start =
        ((args->face * 31u) + (args->glyph * 7u) + (args->size * 131u)) % GUI_GLYPH_CACHE_ENTRIES;
    unsigned int at = start;
    do
    {
        GuiCachedGlyph *const entry = &args->glyphs->entry[at];
        if (entry->used && (entry->face == args->face) && (entry->glyph == args->glyph) && (entry->size == args->size))
        {
            return entry;
        }
        if (!entry->used)
        {
            break;
        }
        at = (at + 1u) % GUI_GLYPH_CACHE_ENTRIES;
    } while (at != start);
    if ((args->glyphs->cached * 4u) >= (GUI_GLYPH_CACHE_ENTRIES * 3u))
    {
        glyphs_clear(args);
        at = start;
    }
    return &args->glyphs->entry[at];
}

// the stem darkening at `size` pixels an em: how far each side of a stroke is pushed out, in pixels, GUI_DARKEN_MAX
// at GUI_DARKEN_SMALL pixels and below, falling straight to GUI_DARKEN_MIN at GUI_DARKEN_LARGE and above, as a
// small glyph's thin stems need the max to read at full weight
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

// the contours' strokes expanded by `reach` pixels a side: each point moved along the miter of its two edges' normals
// to the side away from the ink, the ink lying to the right of a contour's travel as TrueType winds it with y up: an
// outer contour grows and a counter shrinks, both widening the stroke; the miter saved to twice the reach at a sharp
// turn
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

// the coverage lifted through the blend's gamma: each share of 255 raised to one over GUI_TEXT_GAMMA, made once,
// which brings a stroke's partly covered edge pixels up toward its whole ones and reads crisper and bolder
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

// the outline in the glyphs' contours drawn as coverage bytes into `mask`, its cover a cell of its own: 1, or 0 where
// a cell could not be taken; a glyph of no pixels is a mask of none
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
    // the cover is taken from the masks ring; a ring with no room for it is emptied first, as the cache is of its entries
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

const GuiMask *gui_glyph(const GuiGlyphsFaceLetterCfg *args)
{
    unsigned int face = args->face;
    unsigned int size = args->size;
    float *advance = args->advance;
    unsigned int had =
        EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs, .face = face);
    const Font *font = &args->glyphs->face[had];
    unsigned int glyph = EMBED_CALL(font_character.glyph, FontCfg, .font = font, .character = args->character);
    // a character its face has none of is drawn from the symbol face where that face has it, as the system's text does
    if ((glyph == 0u) && (args->character > 0x20u) && (had != GUI_FACE_SYMBOL) &&
        EMBED_CALL(glyphs_symbol_read, GlyphsCtx, .glyphs = args->glyphs))
    {
        const unsigned int symbol = EMBED_CALL(font_character.glyph, FontCfg, .font = &args->glyphs->face[GUI_FACE_SYMBOL],
                                               .character = args->character);
        if (symbol != 0u)
        {
            had = GUI_FACE_SYMBOL;
            font = &args->glyphs->face[had];
            glyph = symbol;
        }
    }
    const float scale = (float)size / font->units;
    *advance = EMBED_CALL(font_glyph_table.advance, FontGlyphCfg, .font = font, .glyph = glyph) * scale;
    GuiCachedGlyph *const entry =
        EMBED_CALL(glyphs_entry, GlyphsCtx, .glyphs = args->glyphs, .face = had, .glyph = glyph, .size = size);
    if (entry->used)
    {
        return &entry->mask_bits;
    }
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
    args->glyphs->cached += 1u;
    return &entry->mask_bits;
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

// the cell keyed by `face`, `character`, `size` and `color`, or an unused cell for it; the cells are emptied first when
// three quarters of them are in use
EMBED_INLINE GuiGlyphCell *glyphs_cell_entry(const GlyphsCtx *args)
{
    const unsigned int start =
        ((args->face * 31u) + (args->character * 7u) + (args->size * 131u) + (args->color * 2654435761u)) %
        GUI_CELL_CACHE_ENTRIES;
    unsigned int at = start;
    do
    {
        GuiGlyphCell *const cell = &args->glyphs->cell[at];
        if (cell->used && (cell->face == args->face) && (cell->character == args->character) && (cell->size == args->size) &&
            (cell->color == args->color))
        {
            return cell;
        }
        if (!cell->used)
        {
            break;
        }
        at = (at + 1u) % GUI_CELL_CACHE_ENTRIES;
    } while (at != start);
    if ((args->glyphs->celled * 4u) >= (GUI_CELL_CACHE_ENTRIES * 3u))
    {
        glyphs_cells_clear(args);
        at = start;
    }
    return &args->glyphs->cell[at];
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
    GuiGlyphCell *const cell = EMBED_CALL(glyphs_cell_entry, GlyphsCtx, .glyphs = glyphs, .face = had, .character = character,
                                          .size = size, .color = color);
    if (cell->used)
    {
        *advance = cell->advance;
        return cell;
    }
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
        // the lanes are taken from the cells ring; a ring with no room for them is emptied first, as the cache is of
        // its cells, this one not among them yet
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
        // each pixel's share of the color is its cover by the color's alpha, two shares of 255 multiplied being a share
        // of 255 once divided by 255; the lanes are the color's channels by the share with half of 255 in, as the
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
    glyphs->celled += 1u;
    return cell;
}

// the ASCII table keyed by `face`, `size` and `color`: the one last found where it is the key's, else found among the
// tables, else the next taken in turn and cleared for the key
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
    // a cell made may have emptied every cell and every table with them: the table is found again before it is filled
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
