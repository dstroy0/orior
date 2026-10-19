// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.c: the pages laid out as pixels, for a printer that takes no PDF: each drawn on white at a printer's
// resolution with the system's monospace font, its own bold and italic where it has them and made from the regular
// face where it does not. A page laid on its side is turned onto the paper as it feeds
#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "font.h"

#include <math.h>

// the most bytes of a page's pixels, the most glyphs kept drawn, and the most coverage they keep
#define CLI_PAGE_BYTES (1u << 26u)

#define CLI_GLYPHS_KEPT 8192u

#define CLI_COVER_KEPT (1u << 23u)

// the color of the line numbers
static const unsigned char s_number_color[3] = {0x88u, 0x88u, 0x88u};

// A glyph kept drawn: its face, glyph, whether it was made bolder or slanted, and its mask
typedef struct
{
    int used;
    unsigned int face;
    unsigned int glyph;
    int bold;
    int slant;
    CliMask mask;
} CliKeptGlyph;

// The faces drawn with, a cell taken as they are read and given back as they are read again: regular, bold, italic
// and bold italic, each where the system has it; and the glyphs kept drawn with them and their coverage
typedef struct
{
    unsigned char face_data[4][CLI_FONT_BYTES];
    unsigned int face_ranges[4][CLI_FONT_RANGES][3];
    CliFont faces[4];
    int has[4];
    CliKeptGlyph kept[CLI_GLYPHS_KEPT];
    float cover[CLI_COVER_KEPT];
    unsigned long long cover_used;
} CliTypefaces;

// The room one rendering takes, a cell taken as it starts and given back as it ends: the page's pixels, a glyph's
// outline, the scratch a glyph is made bolder in, and the letters of a text drawn
typedef struct
{
    unsigned char page[CLI_PAGE_BYTES];
    CliContours contours;
    float scratch[1u << 20u];
    unsigned int letters[CLI_PATH_LONGEST * 4u];
} CliRenderRoom;

static CliTypefaces *s_typefaces;
static CliRenderRoom *s_room;

int cli_print_faces(CliLine *said)
{
    cli_cell_given(s_typefaces);
    s_typefaces = cli_cell_taken(sizeof(*s_typefaces), "the faces drawn with");
    if (s_typefaces == NULL)
    {
        cli_line_text(said, "the faces drawn with could not be taken");
        return 0;
    }
    char paths[4][CLI_PATH_LONGEST];
    cli_font_monospace(paths);
    for (unsigned int face = 0u; face < 4u; face += 1u)
    {
        const unsigned long long size =
            (paths[face][0] != '\0') ? qry_member_file_read(paths[face], s_typefaces->face_data[face], CLI_FONT_BYTES)
                                     : QRY_MEMBER_UNREAD;
        s_typefaces->has[face] =
            (size != QRY_MEMBER_UNREAD) && cli_font_read(&s_typefaces->faces[face], s_typefaces->face_data[face], size,
                                                         s_typefaces->face_ranges[face]);
    }
    for (unsigned int at = 0u; at < CLI_GLYPHS_KEPT; at += 1u)
    {
        s_typefaces->kept[at].used = 0;
    }
    s_typefaces->cover_used = 0ull;
    if (!s_typefaces->has[0])
    {
        cli_line_text(said, "the system has no monospace font orior reads: Consolas, Cascadia Mono, DejaVu Sans Mono, "
                            "Liberation Mono, Menlo or Courier New");
        return 0;
    }
    return 1;
}

// Makes a glyph bolder by spreading its coverage `by` pixels to the right, into the scratch, which it is copied back
// from: 1, or 0 where it would not fit
static int cli_embolden(CliMask *mask, unsigned long long by, unsigned long long room)
{
    const unsigned long long width = mask->width + by;
    if (((width * mask->height) > (sizeof(s_room->scratch) / sizeof(s_room->scratch[0]))) ||
        ((width * mask->height) > room))
    {
        return 0;
    }
    memor.set(
        &(MemoriaCfg){.dst = s_room->scratch, .val = 0, .bytes = width * mask->height * sizeof(s_room->scratch[0])});
    for (unsigned long long y = 0ull; y < mask->height; y += 1ull)
    {
        for (unsigned long long x = 0ull; x < mask->width; x += 1ull)
        {
            const float value = mask->cover[(y * mask->width) + x];
            for (unsigned long long shift = 0ull; shift <= by; shift += 1ull)
            {
                float *const at = &s_room->scratch[(y * width) + x + shift];
                *at = (*at > value) ? *at : value;
            }
        }
    }
    memor.cpy(&(MemoriaCfg){
        .dst = mask->cover, .src = s_room->scratch, .bytes = width * mask->height * sizeof(s_room->scratch[0])});
    mask->width = width;
    return 1;
}

// the mask of a glyph of a face, made bolder or slanted, drawn once and kept: NULL where it cannot be drawn
static const CliMask *cli_mask_of(unsigned int key_face, const CliFont *font, unsigned int glyph, int bold, int slant,
                                  float em)
{
    const unsigned long long bucket_start =
        ((key_face * 31u) + (glyph * 7u) + (bold ? 3u : 0u) + (slant ? 5u : 0u)) % CLI_GLYPHS_KEPT;
    unsigned long long bucket = bucket_start;
    while (s_typefaces->kept[bucket].used)
    {
        const CliKeptGlyph *const kept = &s_typefaces->kept[bucket];
        if ((kept->face == key_face) && (kept->glyph == glyph) && (kept->bold == bold) && (kept->slant == slant))
        {
            return &kept->mask;
        }
        bucket = (bucket + 1u) % CLI_GLYPHS_KEPT;
        if (bucket == bucket_start)
        {
            break;
        }
    }
    // a table or coverage full is let go whole, every glyph drawn again as it is next asked for
    if (s_typefaces->kept[bucket].used || ((CLI_COVER_KEPT - s_typefaces->cover_used) < (1u << 18u)))
    {
        for (unsigned int at = 0u; at < CLI_GLYPHS_KEPT; at += 1u)
        {
            s_typefaces->kept[at].used = 0;
        }
        s_typefaces->cover_used = 0ull;
        bucket = bucket_start;
    }
    const float font_scale = em / font->units;
    const float shear = slant ? (0.2f * font_scale) : 0.0f;
    const float matrix[6] = {font_scale, 0.0f, shear, font_scale, 0.0f, 0.0f};
    s_room->contours.points = 0u;
    s_room->contours.contours = 0u;
    cli_font_outline(font, glyph, matrix, 0u, &s_room->contours);
    CliKeptGlyph *const kept = &s_typefaces->kept[bucket];
    cli_font_rasterize(&s_room->contours, &kept->mask, s_typefaces->cover + s_typefaces->cover_used,
                       CLI_COVER_KEPT - s_typefaces->cover_used);
    if (bold)
    {
        const float spread = em / 28.0f;
        // a spread of whole pixels, at least one
        const unsigned long long by = (unsigned long long)((spread > 1.0f) ? spread : 1.0f);
        if (!cli_embolden(&kept->mask, by, CLI_COVER_KEPT - s_typefaces->cover_used))
        {
            return NULL;
        }
    }
    s_typefaces->cover_used += kept->mask.width * kept->mask.height;
    kept->used = 1;
    kept->face = key_face;
    kept->glyph = glyph;
    kept->bold = bold;
    kept->slant = slant;
    return &kept->mask;
}

// Lays `mask` on `page` with its top left at x, y in `color`
static void cli_blend(CliPixels *page, const CliMask *mask, long long left, long long top, const unsigned char *color)
{
    for (unsigned long long my = 0ull; my < mask->height; my += 1ull)
    {
        // a row's place is far inside what a long long holds
        const long long y = top + (long long)my;
        if ((y < 0ll) || ((unsigned long long)y >= page->height))
        {
            continue;
        }
        for (unsigned long long mx = 0ull; mx < mask->width; mx += 1ull)
        {
            const float cover = mask->cover[(my * mask->width) + mx];
            const long long x = left + (long long)mx;
            if ((cover <= 0.0f) || (x < 0ll) || ((unsigned long long)x >= page->width))
            {
                continue;
            }
            const unsigned long long at =
                (((unsigned long long)y * page->width) + (unsigned long long)x) * page->channels;
            for (unsigned long long channel = 0ull; channel < page->channels; channel += 1ull)
            {
                const float level = (float)color[channel];
                // a blend of two bytes by a share from 0 to 1 is a byte
                page->data[at + channel] =
                    (unsigned char)roundf(((float)page->data[at + channel] * (1.0f - cover)) + (level * cover));
            }
        }
    }
}

// one text drawn at its column on a baseline
static void cli_draw(CliPixels *page, const char *text, unsigned long long length, unsigned long long column,
                     const unsigned char *color, int bold, int italic, float scale, float baseline)
{
    const unsigned int want = (bold ? 1u : 0u) | (italic ? 2u : 0u);
    const unsigned int face = s_typefaces->has[want] ? want : 0u;
    const int make_bold = !s_typefaces->has[want] && bold;
    const int slant = !s_typefaces->has[want] && italic;
    const CliFont *const font = &s_typefaces->faces[face];
    const float em = CLI_PRINT_SIZE * scale;
    const float cell = CLI_PRINT_ADVANCE * scale;
    const float font_scale = em / font->units;
    const unsigned long long count = cli_regexp_letters(text, length, s_room->letters, CLI_PATH_LONGEST * 4u);
    for (unsigned long long offset = 0ull; offset < count; offset += 1ull)
    {
        if (s_room->letters[offset] == ' ')
        {
            continue;
        }
        const unsigned int glyph = cli_font_glyph(font, s_room->letters[offset]);
        const CliMask *const mask = cli_mask_of(face, font, glyph, make_bold, slant, em);
        if (mask == NULL)
        {
            continue;
        }
        const float advance = cli_font_advance(font, glyph) * font_scale;
        const float x = (CLI_PRINT_MARGIN * scale) + ((float)(column + offset) * cell) + ((cell - advance) / 2.0f);
        // a pixel's place, rounded, is far inside what a long long holds
        cli_blend(page, mask, (long long)roundf(x) + mask->left, (long long)roundf(baseline) - mask->top, color);
    }
}

void cli_print_render(const CliPrintListing *listing, const CliPrintLayout *layout, float dpi,
                      void (*each)(void *context, const CliPixels *page, unsigned int index, unsigned int count),
                      void *context)
{
    s_room = (s_typefaces != NULL) ? cli_cell_taken(sizeof(*s_room), "a rendering's room") : NULL;
    if (s_room == NULL)
    {
        return;
    }
    const CliPrintLaid *const laid = cli_print_lay_out(listing, layout);
    const float scale = dpi / 72.0f;
    float width = 0.0f;
    float height = 0.0f;
    cli_print_size(layout, &width, &height);
    CliPixels page = {(unsigned long long)roundf(width * scale), (unsigned long long)roundf(height * scale),
                      layout->gray ? 1ull : 3ull, s_room->page};
    if ((page.width * page.height * page.channels) > CLI_PAGE_BYTES)
    {
        cli_cell_given(s_room);
        s_room = NULL;
        return;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < laid->pages; at += 1u)
    {
        count += cli_print_prints(layout, at + 1ull) ? 1u : 0u;
    }
    unsigned char number_tint[3];
    if (layout->gray)
    {
        cli_print_color("#888888", 7u, 1, number_tint);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = number_tint, .src = s_number_color, .bytes = 3u});
    }
    unsigned int index = 0u;
    for (unsigned int at = 0u; at < laid->pages; at += 1u)
    {
        if (!cli_print_prints(layout, at + 1ull))
        {
            continue;
        }
        memor.set(&(MemoriaCfg){.dst = s_room->page, .val = 255, .bytes = page.width * page.height * page.channels});
        const unsigned int first = at * laid->rows_per_page;
        const unsigned int last =
            ((first + laid->rows_per_page) < laid->rows) ? (first + laid->rows_per_page) : laid->rows;
        for (unsigned int row_at = first; row_at < last; row_at += 1u)
        {
            const CliPrintRow *const row = &laid->row[row_at];
            const float baseline =
                (CLI_PRINT_MARGIN + ((float)(row_at - first) * CLI_PRINT_LEADING) + (CLI_PRINT_LEADING * 0.75f)) *
                scale;
            if (row->numbered)
            {
                char number[32];
                CliLine line = {number, sizeof(number), 0ull};
                const unsigned long long numbers = (laid->number_columns > 2ull) ? (laid->number_columns - 2ull) : 0ull;
                cli_line_decimal_columns(&line, row->number, (unsigned int)numbers);
                const unsigned long long length = cli_line_closed(&line);
                cli_draw(&page, number, length, 0ull, number_tint, 0, 0, scale, baseline);
            }
            for (unsigned int piece_at = 0u; piece_at < row->pieces; piece_at += 1u)
            {
                const CliPrintPiece *const piece = &laid->piece[row->first + piece_at];
                cli_draw(&page, laid->text + piece->text, piece->length, laid->number_columns + piece->column,
                         piece->color, piece->bold, piece->italic, scale, baseline);
            }
        }
        each(context, &page, index, count);
        index += 1u;
    }
    cli_cell_given(s_room);
    s_room = NULL;
}

void cli_print_turned(const CliPixels *page, CliPixels *out)
{
    out->width = page->height;
    out->height = page->width;
    out->channels = page->channels;
    for (unsigned long long y = 0ull; y < page->height; y += 1ull)
    {
        for (unsigned long long x = 0ull; x < page->width; x += 1ull)
        {
            const unsigned long long from = ((y * page->width) + x) * page->channels;
            const unsigned long long to = (((page->width - 1ull - x) * out->width) + y) * page->channels;
            memor.cpy(&(MemoriaCfg){.dst = out->data + to, .src = page->data + from, .bytes = page->channels});
        }
    }
}
