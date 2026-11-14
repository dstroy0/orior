// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font.c: a font's tables read, its character map, and a glyph's advance and place
#include "font.h"

#include "memoria_operor/memoria_operor.h"

unsigned int font_u16(const FontAtCfg *args)
{
    return ((args->at + 2ULL) <= args->font->size)
               ? (((unsigned int)args->font->data[args->at] << 8u) | args->font->data[args->at + 1u])
               : 0u;
}

int font_i16(const FontAtCfg *args)
{
    const unsigned int value = font_at.u16(args);
    // the two bytes read as the signed number they write
    return (value >= 0x8000u) ? ((int)value - 0x10000) : (int)value;
}

typedef struct
{
    const Font *font;
    unsigned long long at;
    const char *tag;
    unsigned long long *at_pointer;
    Font *font_out;
    unsigned int start;
    unsigned int end;
    unsigned int glyph;
} FontCtx;
EMBED_INLINE unsigned long long font_u32(const FontCtx *args)
{
    const Font *font = args->font;
    unsigned long long at = args->at;
    return ((at + 4ULL) <= font->size)
               ? (((unsigned long long)EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = at) << 16u) |
                  EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = at + 2u))
               : 0ULL;
}

// the place of the table `tag`, 1 where it is found
EMBED_INLINE int font_table_find(const FontCtx *args)
{
    unsigned long long *at = args->at_pointer;
    for (unsigned int index = args->font->tables; index > 0u; index -= 1u)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->font->tag[index - 1u], .other = args->tag, .bytes = 4u}) == 0)
        {
            *at = args->font->table[index - 1u];
            return 1;
        }
    }
    return 0;
}

// a range of characters put after the map's
EMBED_INLINE void font_range(const FontCtx *args)
{
    if (args->font_out->ranges < FONT_RANGES)
    {
        args->font_out->range[args->font_out->ranges][0] = args->start;
        args->font_out->range[args->font_out->ranges][1] = args->end;
        args->font_out->range[args->font_out->ranges][2] = args->glyph;
        args->font_out->ranges += 1u;
    }
}

// The character map as ranges of characters and the glyph each range starts at: format 12 where the font has it,
// else format 4. 1, or 0 where it has neither
EMBED_INLINE int font_cmap_read(const FontCtx *args)
{
    Font *font = args->font_out;
    unsigned long long cmap = 0ULL;
    if (!EMBED_CALL(font_table_find, FontCtx, .font = font, .tag = "cmap", .at_pointer = &cmap))
    {
        return 0;
    }
    const unsigned int count = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = cmap + 2u);
    unsigned int best_rank = 0u;
    unsigned long long best = 0ULL;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const unsigned long long at = cmap + 4u + (index * 8ULL);
        const unsigned int platform = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = at);
        const unsigned int encoding = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = at + 2u);
        const unsigned long long offset = cmap + EMBED_CALL(font_u32, FontCtx, .font = font, .at = at + 4u);
        const unsigned int format = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = offset);
        unsigned int rank;
        if (((platform == 3u) && (encoding == 10u) && (format == 12u)) ||
            ((platform == 0u) && ((encoding == 4u) || (encoding == 6u)) && (format == 12u)))
        {
            rank = 3u;
        }
        else if (((platform == 3u) && (encoding == 1u) && (format == 4u)) || ((platform == 0u) && (format == 4u)))
        {
            rank = 2u;
        }
        else
        {
            rank = 0u;
        }
        if (rank > best_rank)
        {
            best_rank = rank;
            best = offset;
        }
    }
    if (best_rank == 0u)
    {
        return 0;
    }
    font->ranges = 0u;
    if (EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = best) == 12u)
    {
        const unsigned long long groups = EMBED_CALL(font_u32, FontCtx, .font = font, .at = best + 12u);
        for (unsigned long long group = 0ULL; group < groups; group += 1ULL)
        {
            const unsigned long long at = best + 16u + (group * 12ULL);
            // each is a 32 bit field of the table
            EMBED_CALL(font_range, FontCtx, .font_out = font,
                       .start = (unsigned int)EMBED_CALL(font_u32, FontCtx, .font = font, .at = at),
                       .end = (unsigned int)EMBED_CALL(font_u32, FontCtx, .font = font, .at = at + 4u),
                       .glyph = (unsigned int)EMBED_CALL(font_u32, FontCtx, .font = font, .at = at + 8u));
        }
        return 1;
    }
    const unsigned int segments = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = best + 6u) / 2u;
    const unsigned long long ends = best + 14u;
    const unsigned long long starts = ends + (segments * 2ULL) + 2ULL;
    const unsigned long long deltas = starts + (segments * 2ULL);
    const unsigned long long offsets = deltas + (segments * 2ULL);
    for (unsigned int segment = 0u; segment < segments; segment += 1u)
    {
        const unsigned int end = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = ends + (segment * 2ULL));
        const unsigned int start =
            EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = starts + (segment * 2ULL));
        const unsigned int delta =
            EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = deltas + (segment * 2ULL));
        const unsigned int range_offset =
            EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = offsets + (segment * 2ULL));
        if (start == 0xffffu)
        {
            continue;
        }
        for (unsigned int code = start; code <= end; code += 1u)
        {
            unsigned int glyph = (code + delta) & 0xffffu;
            if (range_offset != 0u)
            {
                const unsigned long long at = offsets + (segment * 2ULL) + range_offset + ((code - start) * 2ULL);
                const unsigned int found = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = at);
                glyph = (found == 0u) ? 0u : ((found + delta) & 0xffffu);
            }
            EMBED_CALL(font_range, FontCtx, .font_out = font, .start = code, .end = code, .glyph = glyph);
        }
    }
    return 1;
}

int font_read(const FontDataCfg *args)
{
    Font *font = args->font_out;
    const unsigned char *data = args->data;
    unsigned long long size = args->size;
    unsigned int (*ranges)[3] = args->ranges;
    font->data = data;
    font->size = size;
    font->range = ranges;
    font->ranges = 0u;
    font->tables = 0u;
    const unsigned long long base =
        ((size >= 4u) && (memor.cmp(&(MemoriaCfg){.src = data, .other = "ttcf", .bytes = 4u}) == 0))
            ? EMBED_CALL(font_u32, FontCtx, .font = font, .at = 12u)
            : 0ULL;
    const unsigned int count = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = base + 4u);
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const unsigned long long at = base + 12u + (index * 16ULL);
        if (((at + 4ULL) > size) || (font->tables >= FONT_TABLES))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = font->tag[font->tables], .src = data + at, .bytes = 4u});
        font->table[font->tables] = EMBED_CALL(font_u32, FontCtx, .font = font, .at = at + 8u);
        font->tables += 1u;
    }
    unsigned long long head = 0ULL;
    unsigned long long hhea = 0ULL;
    unsigned long long maxp = 0ULL;
    unsigned long long place = 0ULL;
    if (!EMBED_CALL(font_table_find, FontCtx, .font = font, .tag = "head", .at_pointer = &head) ||
        !EMBED_CALL(font_table_find, FontCtx, .font = font, .tag = "hhea", .at_pointer = &hhea) ||
        !EMBED_CALL(font_table_find, FontCtx, .font = font, .tag = "maxp", .at_pointer = &maxp) ||
        !EMBED_CALL(font_table_find, FontCtx, .font = font, .tag = "glyf", .at_pointer = &place) ||
        !EMBED_CALL(font_table_find, FontCtx, .font = font, .tag = "loca", .at_pointer = &place))
    {
        return 0;
    }
    font->units = (float)EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = head + 18u);
    font->long_loca = EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = head + 50u) == 1;
    font->ascent = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = hhea + 4u);
    font->descent = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = hhea + 6u);
    font->metrics = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = hhea + 34u);
    font->glyphs = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = maxp + 4u);
    return EMBED_CALL(font_cmap_read, FontCtx, .font_out = font);
}

unsigned int font_glyph(const FontCfg *args)
{
    // the first range whose last character is not before the character, found by halves
    unsigned int low = 0u;
    unsigned int high = args->font->ranges;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (args->font->range[middle][1] < args->character)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    if ((low < args->font->ranges) && (args->font->range[low][0] <= args->character))
    {
        return (args->font->range[low][2] + (args->character - args->font->range[low][0])) & 0xffffu;
    }
    return 0u;
}

float font_advance(const FontGlyphCfg *args)
{
    unsigned long long hmtx = 0ULL;
    if (!EMBED_CALL(font_table_find, FontCtx, .font = args->font, .tag = "hmtx", .at_pointer = &hmtx))
    {
        return args->font->units / 2.0f;
    }
    const unsigned int last = (args->font->metrics > 0u) ? (args->font->metrics - 1u) : 0u;
    const unsigned int index = (args->glyph < last) ? args->glyph : last;
    return (float)EMBED_CALL(font_at.u16, FontAtCfg, .font = args->font, .at = hmtx + (index * 4ULL));
}

int font_glyph_range(const FontGlyphCfg *args)
{
    unsigned long long *from = args->from;
    unsigned long long *to = args->to;
    unsigned long long loca = 0ULL;
    unsigned long long glyf = 0ULL;
    if ((args->glyph >= args->font->glyphs) ||
        !EMBED_CALL(font_table_find, FontCtx, .font = args->font, .tag = "loca", .at_pointer = &loca) ||
        !EMBED_CALL(font_table_find, FontCtx, .font = args->font, .tag = "glyf", .at_pointer = &glyf))
    {
        return 0;
    }
    const unsigned long long start =
        args->font->long_loca
            ? EMBED_CALL(font_u32, FontCtx, .font = args->font, .at = loca + (args->glyph * 4ULL))
            : (EMBED_CALL(font_at.u16, FontAtCfg, .font = args->font, .at = loca + (args->glyph * 2ULL)) * 2ULL);
    const unsigned long long end =
        args->font->long_loca
            ? EMBED_CALL(font_u32, FontCtx, .font = args->font, .at = loca + (args->glyph * 4ULL) + 4u)
            : (EMBED_CALL(font_at.u16, FontAtCfg, .font = args->font, .at = loca + (args->glyph * 2ULL) + 2u) *
               2ULL);
    *from = glyf + start;
    *to = glyf + end;
    return end > start;
}
