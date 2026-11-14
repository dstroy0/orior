// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.c: the pages laid out as pixels, for a printer that takes no PDF: each drawn on white at a printer's
// resolution with the system's monospace font, its own bold and italic where it has them and made from the regular
// face where it does not. A page laid on its side is turned onto the paper as it feeds
#include "../../../../includes/arith/arith.h"
#include "../../../includes/font/font.h"
#include "../command_line/pool.h"
#include "../text/regexp.h"
#include "font_families.h"

// the most bytes of a page's pixels, the most glyphs cached drawn, and the most coverage they cover
#define CLI_PAGE_BYTES (1u << 26u)

#define CLI_GLYPH_CACHE 8192u

#define CLI_COVER_CACHE (1u << 23u)
// the coverage floats a glyph is given work state for ahead of its drawing
#define CLI_COVER_MIN (1u << 18u)

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
    FontMask mask_bits;
} CliCachedGlyph;

// The faces drawn with, a print's own: temporary cells taken as the faces are read and given back as the render ends,
// or as the faces are read again before one. Regular, bold, italic and bold italic, each where the system has it, each
// face's bytes a cell of its file's size; and the glyphs cached drawn with them and their coverage
typedef struct
{
    const unsigned char *face_data[4];
    unsigned long long face_size[4];
    unsigned int face_ranges[4][FONT_RANGES][3];
    Font faces[4];
    int has[4];
    CliCachedGlyph glyphs[CLI_GLYPH_CACHE];
    float *cover;
    unsigned long long cover_capacity;
    unsigned long long cover_used;
} CliTypefaces;

// The work state one rendering takes, a cell taken as it starts and given back as it ends: the page's pixels, a cell of
// their own sized to the page, a glyph's outline, the scratch a glyph is made bolder in, and the letters of a text
// drawn
typedef struct
{
    unsigned char *page;
    FontContours contours;
    float scratch[1u << 20u];
    unsigned int letters[CLI_PATH_MAX_LEN * 4u];
} CliRenderWorkState;

static CliTypefaces *s_typefaces;
static CliRenderWorkState *s_work_state;

// the faces' cells given back: each face's file, the coverage cache and the record
static void raster_faces_release(void)
{
    if (s_typefaces != NULL)
    {
        for (unsigned int face = 0u; face < 4u; face += 1u)
        {
            g_cli_guard->persistent_buf_release((void *)(embed_word)(s_typefaces->face_data[face]),
                                                (g_cli_print_owner));
        }
        g_cli_guard->persistent_buf_release(s_typefaces->cover, g_cli_print_owner);
    }
    g_cli_guard->persistent_buf_release(s_typefaces, g_cli_print_owner);
    s_typefaces = NULL;
}

int cli_print_faces(const CliPrintJsonCfg *args)
{
    raster_faces_release();
    s_typefaces = g_cli_guard->temporary_buf_alloc(sizeof(*s_typefaces), g_cli_print_owner);
    if (s_typefaces == NULL)
    {
        mmgr_line_put(args->err_msg, "the faces drawn with could not be taken");
        return 0;
    }
    s_typefaces->cover = g_cli_guard->temporary_buf_alloc(CLI_COVER_CACHE * sizeof(float), g_cli_print_owner);
    s_typefaces->cover_capacity = (s_typefaces->cover != NULL) ? CLI_COVER_CACHE : 0ULL;
    s_typefaces->cover_used = 0ULL;
    char paths[4][CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_font_families.monospace, CliFontFamiliesCfg, .paths = paths);
    for (unsigned int face = 0u; face < 4u; face += 1u)
    {
        // a face's file read whole and kept as long as the print draws from it, in a temporary cell of its size and the
        // slack, a file past FONT_BYTES let go unread
        unsigned long long size = 0ULL;
        unsigned char *data =
            ((paths[face][0] != '\0') &&
             EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = paths[face], .size_pointer = &size) &&
             (size <= FONT_BYTES))
                ? g_cli_guard->temporary_buf_alloc(size + CLI_FILE_SLACK, g_cli_print_owner)
                : NULL;
        if ((data == NULL) || !EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = paths[face],
                                          .from = 0ULL, .bytes_bytes = data, .count_u64 = size, .got = &size))
        {
            g_cli_guard->persistent_buf_release(data, g_cli_print_owner);
            data = NULL;
            size = 0ULL;
        }
        else
        {
            memor.set(&(MemoriaCfg){.dst = data + size, .val = 0, .bytes = CLI_FILE_SLACK});
        }
        s_typefaces->face_data[face] = data;
        s_typefaces->face_size[face] = size;
        s_typefaces->has[face] =
            (data != NULL) && EMBED_CALL(font_data.read, FontDataCfg, .font_out = &s_typefaces->faces[face],
                                         .data = data, .size = size, .ranges = s_typefaces->face_ranges[face]);
    }
    for (unsigned int at = 0u; at < CLI_GLYPH_CACHE; at += 1u)
    {
        s_typefaces->glyphs[at].used = 0;
    }
    s_typefaces->cover_used = 0ULL;
    if (!s_typefaces->has[0])
    {
        mmgr_line_put(args->err_msg,
                      "the system has no monospace font impero reads: Consolas, Cascadia Mono, DejaVu Sans Mono, "
                      "Liberation Mono, Menlo or Courier New");
        return 0;
    }
    return 1;
}

typedef struct
{
    FontMask *mask_bits;
    unsigned long long by;
    unsigned long long capacity;
    unsigned int key_face;
    const Font *font;
    unsigned int glyph;
    int bold;
    int slant;
    float em;
    CliPixels *page;
    const FontMask *mask_read;
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
    const unsigned long long width = args->mask_bits->width + args->by;
    if (((width * args->mask_bits->height) > (sizeof(s_work_state->scratch) / sizeof(s_work_state->scratch[0]))) ||
        ((width * args->mask_bits->height) > args->capacity))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = s_work_state->scratch,
                            .val = 0,
                            .bytes = width * args->mask_bits->height * sizeof(s_work_state->scratch[0])});
    for (unsigned long long y = 0ULL; y < args->mask_bits->height; y += 1ULL)
    {
        for (unsigned long long x = 0ULL; x < args->mask_bits->width; x += 1ULL)
        {
            const float value = args->mask_bits->cover[(y * args->mask_bits->width) + x];
            for (unsigned long long shift = 0ULL; shift <= args->by; shift += 1ULL)
            {
                float *const at = &s_work_state->scratch[(y * width) + x + shift];
                *at = (*at > value) ? *at : value;
            }
        }
    }
    memor.cpy(&(MemoriaCfg){.dst = args->mask_bits->cover,
                            .src = s_work_state->scratch,
                            .bytes = width * args->mask_bits->height * sizeof(s_work_state->scratch[0])});
    args->mask_bits->width = width;
    return 1;
}

// every cached glyph forgotten, each drawn again as it is next requested, and the coverage empty
static void cli_glyphs_forgotten(void)
{
    for (unsigned int at = 0u; at < CLI_GLYPH_CACHE; at += 1u)
    {
        s_typefaces->glyphs[at].used = 0;
    }
    s_typefaces->cover_used = 0ULL;
}

// the coverage given work state for a glyph, CLI_COVER_MIN floats past those used: where it has none left the glyphs
// are forgotten and the coverage begins again: 1, or 0 where no coverage can be had at all
static int cli_cover_capacity(void)
{
    if ((s_typefaces->cover_capacity - s_typefaces->cover_used) >= CLI_COVER_MIN)
    {
        return 1;
    }
    cli_glyphs_forgotten();
    return s_typefaces->cover_capacity >= CLI_COVER_MIN;
}

// the mask of a glyph of a face, made bolder or slanted, drawn once and cached: NULL where it cannot be drawn
EMBED_INLINE const FontMask *cli_mask_get(const RasterCtx *args)
{
    const Font *font = args->font;
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
            return &cached->mask_bits;
        }
        bucket = (bucket + 1u) % CLI_GLYPH_CACHE;
        if (bucket == bucket_start)
        {
            break;
        }
    }
    // a table full is let go whole, every glyph drawn again as it is next requested; the coverage is given work state
    // for the glyph, and where that forgets the glyphs the bucket is the first again
    if (s_typefaces->glyphs[bucket].used)
    {
        cli_glyphs_forgotten();
        bucket = bucket_start;
    }
    if (!cli_cover_capacity())
    {
        return NULL;
    }
    bucket = s_typefaces->glyphs[bucket_start].used ? bucket : bucket_start;
    const float font_scale = args->em / font->units;
    const float shear = slant ? (0.2f * font_scale) : 0.0f;
    const float matrix[6] = {font_scale, 0.0f, shear, font_scale, 0.0f, 0.0f};
    s_work_state->contours.points = 0u;
    s_work_state->contours.contours = 0u;
    EMBED_CALL(font_glyph_matrix.outline, FontGlyphMatrixCfg, .font = font, .glyph = glyph, .matrix = matrix,
               .depth = 0u, .out = &s_work_state->contours);
    CliCachedGlyph *const cached = &s_typefaces->glyphs[bucket];
    // the edges lie in the work's scratch, which the bolder takes only after the glyph is drawn
    EMBED_CALL(font_contours.rasterize, FontContoursCfg, .contours = &s_work_state->contours,
               .mask_bits = &cached->mask_bits, .cover = s_typefaces->cover + s_typefaces->cover_used,
               .capacity = s_typefaces->cover_capacity - s_typefaces->cover_used, .scratch = s_work_state->scratch,
               .scratch_bytes = sizeof(s_work_state->scratch));
    if (bold)
    {
        const float spread = args->em / 28.0f;
        // a spread of whole pixels, at least one
        const unsigned long long by = (unsigned long long)((spread > 1.0f) ? spread : 1.0f);
        if (!EMBED_CALL(cli_embolden, RasterCtx, .mask_bits = &cached->mask_bits, .by = by,
                        .capacity = s_typefaces->cover_capacity - s_typefaces->cover_used))
        {
            return NULL;
        }
    }
    s_typefaces->cover_used += cached->mask_bits.width * cached->mask_bits.height;
    cached->used = 1;
    cached->face = args->key_face;
    cached->glyph = glyph;
    cached->bold = bold;
    cached->slant = slant;
    return &cached->mask_bits;
}

// Lays `mask` on `page` with its top left at x, y in `color`
EMBED_INLINE void cli_blend(const RasterCtx *args)
{
    for (unsigned long long my = 0ULL; my < args->mask_read->height; my += 1ULL)
    {
        // a row's place is far inside what a long long represents
        const long long y = args->top + (long long)my;
        if ((y < 0LL) || ((unsigned long long)y >= args->page->height))
        {
            continue;
        }
        for (unsigned long long mx = 0ULL; mx < args->mask_read->width; mx += 1ULL)
        {
            const float cover = args->mask_read->cover[(my * args->mask_read->width) + mx];
            const long long x = args->left + (long long)mx;
            if ((cover <= 0.0f) || (x < 0LL) || ((unsigned long long)x >= args->page->width))
            {
                continue;
            }
            const unsigned long long at =
                (((unsigned long long)y * args->page->width) + (unsigned long long)x) * args->page->channels;
            for (unsigned long long channel = 0ULL; channel < args->page->channels; channel += 1ULL)
            {
                const float level = (float)args->color[channel];
                // a blend of two bytes by a share from 0 to 1 is a byte
                args->page->data[at + channel] = (unsigned char)arith_round_f32(
                    ((float)args->page->data[at + channel] * (1.0f - cover)) + (level * cover));
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
    const Font *const font = &s_typefaces->faces[face];
    const float em = CLI_PRINT_SIZE * scale;
    const float cell = CLI_PRINT_ADVANCE * scale;
    const float font_scale = em / font->units;
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = args->length,
                   .letters = s_work_state->letters, .capacity_u64 = CLI_PATH_MAX_LEN * 4u);
    for (unsigned long long offset = 0ULL; offset < count; offset += 1ULL)
    {
        if (s_work_state->letters[offset] == ' ')
        {
            continue;
        }
        const unsigned int glyph =
            EMBED_CALL(font_letter.glyph, FontCfg, .font = font, .letter = s_work_state->letters[offset]);
        const FontMask *const mask_bits = EMBED_CALL(cli_mask_get, RasterCtx, .key_face = face, .font = font,
                                                     .glyph = glyph, .bold = make_bold, .slant = slant, .em = em);
        if (mask_bits == NULL)
        {
            continue;
        }
        const float advance =
            EMBED_CALL(font_glyph_table.advance, FontGlyphCfg, .font = font, .glyph = glyph) * font_scale;
        const float x =
            (CLI_PRINT_MARGIN * scale) + ((float)(args->column + offset) * cell) + ((cell - advance) / 2.0f);
        // a pixel's place, rounded, is far inside what a long long represents
        EMBED_CALL(cli_blend, RasterCtx, .page = args->page, .mask_read = mask_bits,
                   .left = (long long)arith_round_f32(x) + mask_bits->left,
                   .top = (long long)arith_round_f32(args->baseline) - mask_bits->top, .color = args->color);
    }
}

void cli_print_render(const CliPrintLayoutListingCfg *args)
{
    const CliPrintListing *listing = args->listing;
    const CliPrintLayout *layout = args->layout;
    s_work_state =
        (s_typefaces != NULL) ? g_cli_guard->temporary_buf_alloc(sizeof(*s_work_state), g_cli_print_owner) : NULL;
    if (s_work_state == NULL)
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
    CliPixels page = {(unsigned long long)arith_round_f32(width * scale),
                      (unsigned long long)arith_round_f32(height * scale), layout->gray ? 1ULL : 3ULL, NULL};
    // the page's pixels a cell of their own, sized to the page at the resolution asked
    const unsigned long long bytes = page.width * page.height * page.channels;
    s_work_state->page = (bytes <= CLI_PAGE_BYTES) ? g_cli_guard->temporary_buf_alloc(bytes, g_cli_print_owner) : NULL;
    page.data = s_work_state->page;
    if (s_work_state->page == NULL)
    {
        g_cli_guard->persistent_buf_release(s_work_state, g_cli_print_owner);
        s_work_state = NULL;
        return;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < laid->pages; at += 1u)
    {
        count += EMBED_CALL(cli_print_table.prints, CliPrintCfg, .layout = layout, .page_u64 = at + 1ULL) ? 1u : 0u;
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
        if (!EMBED_CALL(cli_print_table.prints, CliPrintCfg, .layout = layout, .page_u64 = at + 1ULL))
        {
            continue;
        }
        memor.set(
            &(MemoriaCfg){.dst = s_work_state->page, .val = 255, .bytes = page.width * page.height * page.channels});
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
                VerbaLinea line = {number, sizeof(number), 0ULL};
                const unsigned long long numbers = (laid->number_columns > 2ULL) ? (laid->number_columns - 2ULL) : 0ULL;
                mmgr_line_decimal_columns(&line, row->number, (unsigned int)numbers);
                const unsigned long long length = mmgr_line_finish(&line);
                EMBED_CALL(cli_draw, RasterCtx, .page = &page, .text = number, .length = length, .column = 0ULL,
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
    g_cli_guard->persistent_buf_release(s_work_state->page, g_cli_print_owner);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_print_owner);
    s_work_state = NULL;
    // the print is drawn: its faces go back with its work state
    raster_faces_release();
}

void cli_print_turn(const CliPrintPageCfg *args)
{
    args->out_pixels_pointer->width = args->page->height;
    args->out_pixels_pointer->height = args->page->width;
    args->out_pixels_pointer->channels = args->page->channels;
    for (unsigned long long y = 0ULL; y < args->page->height; y += 1ULL)
    {
        for (unsigned long long x = 0ULL; x < args->page->width; x += 1ULL)
        {
            const unsigned long long from = ((y * args->page->width) + x) * args->page->channels;
            const unsigned long long to =
                (((args->page->width - 1ULL - x) * args->out_pixels_pointer->width) + y) * args->page->channels;
            memor.cpy(&(MemoriaCfg){.dst = args->out_pixels_pointer->data + to,
                                    .src = args->page->data + from,
                                    .bytes = args->page->channels});
        }
    }
}
