// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.c: the pages laid out as pixels, for a printer that takes no PDF: each drawn on white at a printer's
// resolution with the system's monospace font, its own bold and italic where it has them and made from the regular
// face where it does not. A page laid on its side is turned onto the paper as it feeds
#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "font.h"

#include <math.h>

// the most bytes of a page's pixels, the most glyphs cached drawn, and the most coverage they cover
#define CLI_PAGE_BYTES (1u << 26u)

#define CLI_GLYPH_CACHE 8192u

#define CLI_COVER_CACHE (1u << 23u)
// the coverage floats a glyph is given room for ahead of its drawing
#define CLI_COVER_LEAST (1u << 18u)

// the color of the line numbers
static const unsigned char s_number_color[3] = {0x88u, 0x88u, 0x88u};

// A glyph cached drawn: its face, glyph, whether it was made bolder or slanted, and its mask
typedef struct
{
    int used;
    unsigned int face;
    unsigned int glyph;
    int bold;
    int slant;
    CliMask mask;
} CliCachedGlyph;

// The faces drawn with, a cell taken as they are read and given back as they are read again: regular, bold, italic
// and bold italic, each where the system has it, each face's bytes a cell of its file's size; and the glyphs cached
// drawn with them and their coverage
typedef struct
{
    const unsigned char *face_data[4];
    unsigned long long face_size[4];
    unsigned int face_ranges[4][CLI_FONT_RANGES][3];
    CliFont faces[4];
    int has[4];
    CliCachedGlyph glyphs[CLI_GLYPH_CACHE];
    float *cover;
    unsigned long long cover_room;
    unsigned long long cover_used;
} CliTypefaces;

// The room one rendering takes, a cell taken as it starts and given back as it ends: the page's pixels, a cell of
// their own sized to the page, a glyph's outline, the scratch a glyph is made bolder in, and the letters of a text
// drawn
typedef struct
{
    unsigned char *page;
    CliContours contours;
    float scratch[1u << 20u];
    unsigned int letters[CLI_PATH_MAX_LEN * 4u];
} CliRenderRoom;

static CliTypefaces *s_typefaces;
static CliRenderRoom *s_room;

int cli_print_faces(const CliPrintJsonCfg *args)
{
    if (s_typefaces != NULL)
    {
        for (unsigned int face = 0u; face < 4u; face += 1u)
        {
            EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = s_typefaces->face_data[face],
                       .owner = &g_cli_print_cells);
        }
        g_cli_cells->persistent_buf_release(s_typefaces->cover, &g_cli_print_cells);
    }
    g_cli_cells->persistent_buf_release(s_typefaces, &g_cli_print_cells);
    s_typefaces = g_cli_cells->persistent_buf_alloc(sizeof(*s_typefaces), &g_cli_print_cells);
    if (s_typefaces == NULL)
    {
        mmgr_line_put(args->err_msg, "the faces drawn with could not be taken");
        return 0;
    }
    s_typefaces->cover = g_cli_cells->persistent_buf_alloc(CLI_COVER_CACHE * sizeof(float), &g_cli_print_cells);
    s_typefaces->cover_room = (s_typefaces->cover != NULL) ? CLI_COVER_CACHE : 0ull;
    s_typefaces->cover_used = 0ull;
    char paths[4][CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_font_families.monospace, CliFontFamiliesCfg, .paths = paths);
    for (unsigned int face = 0u; face < 4u; face += 1u)
    {
        // a face's file read whole and saved as long as it is drawn from, in a persistent cell of its size and the
        // slack, a file past CLI_FONT_BYTES let go unread
        unsigned long long size = 0ull;
        unsigned char *data =
            ((paths[face][0] != '\0') &&
             EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = paths[face], .size_pointer = &size) &&
             (size <= CLI_FONT_BYTES))
                ? g_cli_cells->persistent_buf_alloc(size + CLI_FILE_SLACK, &g_cli_print_cells)
                : NULL;
        if ((data == NULL) || !EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = paths[face],
                                          .from = 0ull, .bytes_bytes = data, .count_u64 = size, .got = &size))
        {
            g_cli_cells->persistent_buf_release(data, &g_cli_print_cells);
            data = NULL;
            size = 0ull;
        }
        else
        {
            memor.set(&(MemoriaCfg){.dst = data + size, .val = 0, .bytes = CLI_FILE_SLACK});
        }
        s_typefaces->face_data[face] = data;
        s_typefaces->face_size[face] = size;
        s_typefaces->has[face] =
            (data != NULL) && EMBED_CALL(cli_font_data.read, CliFontDataCfg, .font_out = &s_typefaces->faces[face],
                                         .data = data, .size = size, .ranges = s_typefaces->face_ranges[face]);
    }
    for (unsigned int at = 0u; at < CLI_GLYPH_CACHE; at += 1u)
    {
        s_typefaces->glyphs[at].used = 0;
    }
    s_typefaces->cover_used = 0ull;
    if (!s_typefaces->has[0])
    {
        mmgr_line_put(args->err_msg,
                      "the system has no monospace font orior_cli reads: Consolas, Cascadia Mono, DejaVu Sans Mono, "
                      "Liberation Mono, Menlo or Courier New");
        return 0;
    }
    return 1;
}

typedef struct
{
    CliMask *mask;
    unsigned long long by;
    unsigned long long room;
    unsigned int key_face;
    const CliFont *font;
    unsigned int glyph;
    int bold;
    int slant;
    float em;
    CliPixels *page;
    const CliMask *mask_read;
    long long left;
    long long top;
    const unsigned char *color;
    const char *text;
    unsigned long long length;
    unsigned long long column;
    int italic;
    float scale;
    float baseline;
} RasterCtx;
// Makes a glyph bolder by spreading its coverage `by` pixels to the right, into the scratch, which it is copied back
// from: 1, or 0 where it would not fit
EMBED_INLINE int cli_embolden(const RasterCtx *args)
{
    const unsigned long long width = args->mask->width + args->by;
    if (((width * args->mask->height) > (sizeof(s_room->scratch) / sizeof(s_room->scratch[0]))) ||
        ((width * args->mask->height) > args->room))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){
        .dst = s_room->scratch, .val = 0, .bytes = width * args->mask->height * sizeof(s_room->scratch[0])});
    for (unsigned long long y = 0ull; y < args->mask->height; y += 1ull)
    {
        for (unsigned long long x = 0ull; x < args->mask->width; x += 1ull)
        {
            const float value = args->mask->cover[(y * args->mask->width) + x];
            for (unsigned long long shift = 0ull; shift <= args->by; shift += 1ull)
            {
                float *const at = &s_room->scratch[(y * width) + x + shift];
                *at = (*at > value) ? *at : value;
            }
        }
    }
    memor.cpy(&(MemoriaCfg){.dst = args->mask->cover,
                            .src = s_room->scratch,
                            .bytes = width * args->mask->height * sizeof(s_room->scratch[0])});
    args->mask->width = width;
    return 1;
}

// every cached glyph forgotten, each drawn again as it is next requested, and the coverage empty
static void cli_glyphs_forgotten(void)
{
    for (unsigned int at = 0u; at < CLI_GLYPH_CACHE; at += 1u)
    {
        s_typefaces->glyphs[at].used = 0;
    }
    s_typefaces->cover_used = 0ull;
}

// the coverage given room for a glyph, CLI_COVER_LEAST floats past those used: where it has none left the glyphs are
// forgotten and the coverage begins again: 1, or 0 where no coverage can be had at all
static int cli_cover_room(void)
{
    if ((s_typefaces->cover_room - s_typefaces->cover_used) >= CLI_COVER_LEAST)
    {
        return 1;
    }
    cli_glyphs_forgotten();
    return s_typefaces->cover_room >= CLI_COVER_LEAST;
}

// the mask of a glyph of a face, made bolder or slanted, drawn once and cached: NULL where it cannot be drawn
EMBED_INLINE const CliMask *cli_mask_get(const RasterCtx *args)
{
    const CliFont *font = args->font;
    unsigned int glyph = args->glyph;
    int bold = args->bold;
    int slant = args->slant;
    const unsigned long long bucket_start =
        ((args->key_face * 31u) + (glyph * 7u) + (bold ? 3u : 0u) + (slant ? 5u : 0u)) % CLI_GLYPH_CACHE;
    unsigned long long bucket = bucket_start;
    while (s_typefaces->glyphs[bucket].used)
    {
        const CliCachedGlyph *const cached = &s_typefaces->glyphs[bucket];
        if ((cached->face == args->key_face) && (cached->glyph == glyph) && (cached->bold == bold) &&
            (cached->slant == slant))
        {
            return &cached->mask;
        }
        bucket = (bucket + 1u) % CLI_GLYPH_CACHE;
        if (bucket == bucket_start)
        {
            break;
        }
    }
    // a table full is let go whole, every glyph drawn again as it is next requested; the coverage is given room for
    // the glyph, and where that forgets the glyphs the bucket is the first again
    if (s_typefaces->glyphs[bucket].used)
    {
        cli_glyphs_forgotten();
        bucket = bucket_start;
    }
    if (!cli_cover_room())
    {
        return NULL;
    }
    bucket = s_typefaces->glyphs[bucket_start].used ? bucket : bucket_start;
    const float font_scale = args->em / font->units;
    const float shear = slant ? (0.2f * font_scale) : 0.0f;
    const float matrix[6] = {font_scale, 0.0f, shear, font_scale, 0.0f, 0.0f};
    s_room->contours.points = 0u;
    s_room->contours.contours = 0u;
    EMBED_CALL(cli_font_glyph_matrix.outline, CliFontGlyphMatrixCfg, .font = font, .glyph = glyph, .matrix = matrix,
               .depth = 0u, .out = &s_room->contours);
    CliCachedGlyph *const cached = &s_typefaces->glyphs[bucket];
    EMBED_CALL(cli_font_contours.rasterize, CliFontContoursCfg, .contours = &s_room->contours, .mask = &cached->mask,
               .cover = s_typefaces->cover + s_typefaces->cover_used,
               .room = s_typefaces->cover_room - s_typefaces->cover_used);
    if (bold)
    {
        const float spread = args->em / 28.0f;
        // a spread of whole pixels, at least one
        const unsigned long long by = (unsigned long long)((spread > 1.0f) ? spread : 1.0f);
        if (!EMBED_CALL(cli_embolden, RasterCtx, .mask = &cached->mask, .by = by,
                        .room = s_typefaces->cover_room - s_typefaces->cover_used))
        {
            return NULL;
        }
    }
    s_typefaces->cover_used += cached->mask.width * cached->mask.height;
    cached->used = 1;
    cached->face = args->key_face;
    cached->glyph = glyph;
    cached->bold = bold;
    cached->slant = slant;
    return &cached->mask;
}

// Lays `mask` on `page` with its top left at x, y in `color`
EMBED_INLINE void cli_blend(const RasterCtx *args)
{
    for (unsigned long long my = 0ull; my < args->mask_read->height; my += 1ull)
    {
        // a row's place is far inside what a long long represents
        const long long y = args->top + (long long)my;
        if ((y < 0ll) || ((unsigned long long)y >= args->page->height))
        {
            continue;
        }
        for (unsigned long long mx = 0ull; mx < args->mask_read->width; mx += 1ull)
        {
            const float cover = args->mask_read->cover[(my * args->mask_read->width) + mx];
            const long long x = args->left + (long long)mx;
            if ((cover <= 0.0f) || (x < 0ll) || ((unsigned long long)x >= args->page->width))
            {
                continue;
            }
            const unsigned long long at =
                (((unsigned long long)y * args->page->width) + (unsigned long long)x) * args->page->channels;
            for (unsigned long long channel = 0ull; channel < args->page->channels; channel += 1ull)
            {
                const float level = (float)args->color[channel];
                // a blend of two bytes by a share from 0 to 1 is a byte
                args->page->data[at + channel] =
                    (unsigned char)roundf(((float)args->page->data[at + channel] * (1.0f - cover)) + (level * cover));
            }
        }
    }
}

// one text drawn at its column on a baseline
EMBED_INLINE void cli_draw(const RasterCtx *args)
{
    const char *text = args->text;
    float scale = args->scale;
    const unsigned int want = (args->bold ? 1u : 0u) | (args->italic ? 2u : 0u);
    const unsigned int face = s_typefaces->has[want] ? want : 0u;
    const int make_bold = !s_typefaces->has[want] && args->bold;
    const int slant = !s_typefaces->has[want] && args->italic;
    const CliFont *const font = &s_typefaces->faces[face];
    const float em = CLI_PRINT_SIZE * scale;
    const float cell = CLI_PRINT_ADVANCE * scale;
    const float font_scale = em / font->units;
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = args->length,
                   .letters = s_room->letters, .room_u64 = CLI_PATH_MAX_LEN * 4u);
    for (unsigned long long offset = 0ull; offset < count; offset += 1ull)
    {
        if (s_room->letters[offset] == ' ')
        {
            continue;
        }
        const unsigned int glyph =
            EMBED_CALL(cli_font.glyph, CliFontCfg, .font = font, .letter = s_room->letters[offset]);
        const CliMask *const mask = EMBED_CALL(cli_mask_get, RasterCtx, .key_face = face, .font = font, .glyph = glyph,
                                               .bold = make_bold, .slant = slant, .em = em);
        if (mask == NULL)
        {
            continue;
        }
        const float advance =
            EMBED_CALL(cli_font_glyph_table.advance, CliFontGlyphCfg, .font = font, .glyph = glyph) * font_scale;
        const float x =
            (CLI_PRINT_MARGIN * scale) + ((float)(args->column + offset) * cell) + ((cell - advance) / 2.0f);
        // a pixel's place, rounded, is far inside what a long long represents
        EMBED_CALL(cli_blend, RasterCtx, .page = args->page, .mask_read = mask,
                   .left = (long long)roundf(x) + mask->left, .top = (long long)roundf(args->baseline) - mask->top,
                   .color = args->color);
    }
}

void cli_print_render(const CliPrintLayoutListingCfg *args)
{
    const CliPrintListing *listing = args->listing;
    const CliPrintLayout *layout = args->layout;
    s_room = (s_typefaces != NULL) ? g_cli_cells->temporary_buf_alloc(sizeof(*s_room), &g_cli_print_cells) : NULL;
    if (s_room == NULL)
    {
        return;
    }
    const CliPrintLaid *const laid = EMBED_CALL(cli_print_layout_listing_out.lay_out, CliPrintLayoutListingOutCfg,
                                                .listing = listing, .layout = layout);
    const float scale = args->dpi_f32 / 72.0f;
    float width = 0.0f;
    float height = 0.0f;
    EMBED_CALL(cli_print_layout_width.size, CliPrintLayoutWidthCfg, .layout = layout, .width = &width,
               .height = &height);
    CliPixels page = {(unsigned long long)roundf(width * scale), (unsigned long long)roundf(height * scale),
                      layout->gray ? 1ull : 3ull, NULL};
    // the page's pixels a cell of their own, sized to the page at the resolution asked
    const unsigned long long bytes = page.width * page.height * page.channels;
    s_room->page = (bytes <= CLI_PAGE_BYTES) ? g_cli_cells->temporary_buf_alloc(bytes, &g_cli_print_cells) : NULL;
    page.data = s_room->page;
    if (s_room->page == NULL)
    {
        g_cli_cells->persistent_buf_release(s_room, &g_cli_print_cells);
        s_room = NULL;
        return;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < laid->pages; at += 1u)
    {
        count += EMBED_CALL(cli_print_table.prints, CliPrintCfg, .layout = layout, .page_u64 = at + 1ull) ? 1u : 0u;
    }
    unsigned char number_tint[3];
    if (layout->gray)
    {
        EMBED_CALL(cli_print_text.color, CliPrintTextCfg, .text = "#888888", .length = 7u, .gray = 1,
                   .rgb = number_tint);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = number_tint, .src = s_number_color, .bytes = 3u});
    }
    unsigned int index = 0u;
    for (unsigned int at = 0u; at < laid->pages; at += 1u)
    {
        if (!EMBED_CALL(cli_print_table.prints, CliPrintCfg, .layout = layout, .page_u64 = at + 1ull))
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
                VerbaLinea line = {number, sizeof(number), 0ull};
                const unsigned long long numbers = (laid->number_columns > 2ull) ? (laid->number_columns - 2ull) : 0ull;
                mmgr_line_decimal_columns(&line, row->number, (unsigned int)numbers);
                const unsigned long long length = mmgr_line_finish(&line);
                EMBED_CALL(cli_draw, RasterCtx, .page = &page, .text = number, .length = length, .column = 0ull,
                           .color = number_tint, .bold = 0, .italic = 0, .scale = scale, .baseline = baseline);
            }
            for (unsigned int piece_at = 0u; piece_at < row->pieces; piece_at += 1u)
            {
                const CliPrintPiece *const piece = &laid->piece[row->first + piece_at];
                EMBED_CALL(cli_draw, RasterCtx, .page = &page, .text = laid->text + piece->text,
                           .length = piece->length, .column = laid->number_columns + piece->column,
                           .color = piece->color, .bold = piece->bold, .italic = piece->italic, .scale = scale,
                           .baseline = baseline);
            }
        }
        EMBED_CALL(args->each, CliEachPageIndexCountCfg, .context = args->context, .page = &page, .index = index,
                   .count = count);
        index += 1u;
    }
    g_cli_cells->persistent_buf_release(s_room->page, &g_cli_print_cells);
    g_cli_cells->persistent_buf_release(s_room, &g_cli_print_cells);
    s_room = NULL;
}

void cli_print_turn(const CliPrintPageCfg *args)
{
    args->out_pixels_pointer->width = args->page->height;
    args->out_pixels_pointer->height = args->page->width;
    args->out_pixels_pointer->channels = args->page->channels;
    for (unsigned long long y = 0ull; y < args->page->height; y += 1ull)
    {
        for (unsigned long long x = 0ull; x < args->page->width; x += 1ull)
        {
            const unsigned long long from = ((y * args->page->width) + x) * args->page->channels;
            const unsigned long long to =
                (((args->page->width - 1ull - x) * args->out_pixels_pointer->width) + y) * args->page->channels;
            memor.cpy(&(MemoriaCfg){.dst = args->out_pixels_pointer->data + to,
                                    .src = args->page->data + from,
                                    .bytes = args->page->channels});
        }
    }
}
