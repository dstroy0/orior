// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs.c: the window's text. A face is read whole from its file into a cell and its character map saved in a cell of
// the ranges it has; a glyph is outlined at its pixel size, measured, rasterized as coverage in a cell of its own
// size and saved as bytes in a entry found by its key, every entry let go together as they fill; a glyph in a color is
// a cell of lanes laid for the one blend, saved the same way, which the text is drawn through
#include "glyphs.h"

#include "../command_line/cli_internal.h"
#include "../text/text.h"

#include <math.h>

// the families the interface face and the reading face are looked for among, in the order they are preferred
static const char *const s_sans[6][4] = {
    {"segoeui.ttf", "segoeuib.ttf", "segoeuii.ttf", "segoeuiz.ttf"},
    {"DejaVuSans.ttf", "DejaVuSans-Bold.ttf", "DejaVuSans-Oblique.ttf", "DejaVuSans-BoldOblique.ttf"},
    {"NotoSans-Regular.ttf", "NotoSans-Bold.ttf", "NotoSans-Italic.ttf", "NotoSans-BoldItalic.ttf"},
    {"LiberationSans-Regular.ttf", "LiberationSans-Bold.ttf", "LiberationSans-Italic.ttf",
     "LiberationSans-BoldItalic.ttf"},
    {"Helvetica.ttc", "", "", ""},
    {"arial.ttf", "arialbd.ttf", "ariali.ttf", "arialbi.ttf"}};
static const char *const s_serif[6][4] = {
    {"georgia.ttf", "georgiab.ttf", "georgiai.ttf", "georgiaz.ttf"},
    {"DejaVuSerif.ttf", "DejaVuSerif-Bold.ttf", "DejaVuSerif-Italic.ttf", "DejaVuSerif-BoldItalic.ttf"},
    {"NotoSerif-Regular.ttf", "NotoSerif-Bold.ttf", "NotoSerif-Italic.ttf", "NotoSerif-BoldItalic.ttf"},
    {"LiberationSerif-Regular.ttf", "LiberationSerif-Bold.ttf", "LiberationSerif-Italic.ttf",
     "LiberationSerif-BoldItalic.ttf"},
    {"Times New Roman.ttf", "", "", ""},
    {"times.ttf", "timesbd.ttf", "timesi.ttf", "timesbi.ttf"}};

typedef struct
{
    GuiGlyphs *glyphs;
    unsigned int face;
    const char *path;
    unsigned int (*ranges)[3];
    unsigned int glyph;
    unsigned int size;
    CliContours *contours;
    float reach;
    unsigned int cover;
    const GuiGlyphs *glyphs_read;
    GuiMask *mask;
    unsigned int letter;
    GuiColor color;
    float *advance;
} GlyphsCtx;
// the face `face` read from the file at `path` through the ranges capacity `ranges`, its ranges then saved in a cell of
// their own count: 1, or 0 where there is no path, no file or no font in it
EMBED_INLINE int glyphs_face_read(const GlyphsCtx *args)
{
    const char *path = args->path;
    if (path[0] == '\0')
    {
        return 0;
    }
    // the face's file saved as long as its font is drawn from, in a persistent cell of its size and the slack
    unsigned long long size = 0ull;
    unsigned char *const data =
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size)
            ? g_cli_cells->persistent_buf_alloc(size + CLI_FILE_SLACK, &g_gui_glyphs_cells)
            : NULL;
    if ((data == NULL) || !EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = path, .from = 0ull,
                                      .bytes_bytes = data, .count_u64 = size, .got = &size))
    {
        g_cli_cells->persistent_buf_release(data, &g_gui_glyphs_cells);
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = data + size, .val = 0, .bytes = CLI_FILE_SLACK});
    CliFont *const font = &args->glyphs->face[args->face];
    if (!EMBED_CALL(cli_font_data.read, CliFontDataCfg, .font_out = font, .data = data, .size = size,
                    .ranges = args->ranges))
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = data, .owner = &g_gui_glyphs_cells);
        return 0;
    }
    const unsigned long long bytes = (unsigned long long)font->ranges * sizeof(args->ranges[0]);
    unsigned int (*const saved)[3] =
        g_cli_cells->persistent_buf_alloc((bytes != 0ull) ? bytes : sizeof(args->ranges[0]), &g_gui_glyphs_cells);
    if (saved == NULL)
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = data, .owner = &g_gui_glyphs_cells);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = saved, .src = args->ranges, .bytes = bytes});
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
            g_cli_cells->persistent_buf_release(entry->mask.cover, &g_gui_glyphs_cells);
            entry->mask.cover = NULL;
            entry->used = 0;
        }
    }
    args->glyphs->cached = 0u;
}

// the face's font and file given back where it was read
EMBED_INLINE void glyphs_face_free(const GlyphsCtx *args)
{
    if (args->glyphs->has[args->face])
    {
        g_cli_cells->persistent_buf_release(args->glyphs->face[args->face].range, &g_gui_glyphs_cells);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = args->glyphs->data[args->face],
                   .owner = &g_gui_glyphs_cells);
    }
    args->glyphs->has[args->face] = 0;
    args->glyphs->data[args->face] = NULL;
}

// the cells emptied: every cell's lanes given back and every cell marked unused
EMBED_INLINE void glyphs_cells_clear(const GlyphsCtx *args)
{
    for (unsigned int at = 0u; at < GUI_CELL_CACHE_ENTRIES; at += 1u)
    {
        GuiGlyphCell *const cell = &args->glyphs->cell[at];
        if (cell->used)
        {
            g_cli_cells->persistent_buf_release(cell->red_blue, &g_gui_glyphs_cells);
            cell->red_blue = NULL;
            cell->green = NULL;
            cell->rest = NULL;
            cell->used = 0;
        }
    }
    args->glyphs->celled = 0u;
    // the ASCII tables point into the cells: they go with them
    memor.set(&(MemoriaCfg){.dst = args->glyphs->ascii, .val = 0, .bytes = sizeof(args->glyphs->ascii)});
}

int gui_glyphs_init(const GuiGlyphsCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->glyphs, .val = 0, .bytes = sizeof(*args->glyphs)});
    unsigned int (*const ranges)[3] =
        g_cli_cells->persistent_buf_alloc(CLI_FONT_RANGES * sizeof(*ranges), &g_gui_glyphs_cells);
    args->glyphs->contours = g_cli_cells->persistent_buf_alloc(sizeof(*args->glyphs->contours), &g_gui_glyphs_cells);
    args->glyphs->entry =
        g_cli_cells->persistent_buf_alloc(GUI_GLYPH_CACHE_ENTRIES * sizeof(*args->glyphs->entry), &g_gui_glyphs_cells);
    args->glyphs->cell =
        g_cli_cells->persistent_buf_alloc(GUI_CELL_CACHE_ENTRIES * sizeof(*args->glyphs->cell), &g_gui_glyphs_cells);
    if ((ranges == NULL) || (args->glyphs->contours == NULL) || (args->glyphs->entry == NULL) ||
        (args->glyphs->cell == NULL))
    {
        g_cli_cells->persistent_buf_release(ranges, &g_gui_glyphs_cells);
        cli_glyphs.free(args);
        mmgr_line_put(args->err_msg, "the cells the window's glyphs are kept in could not be taken");
        return 0;
    }
    memor.set(&(MemoriaCfg){
        .dst = args->glyphs->entry, .val = 0, .bytes = GUI_GLYPH_CACHE_ENTRIES * sizeof(*args->glyphs->entry)});
    memor.set(&(MemoriaCfg){
        .dst = args->glyphs->cell, .val = 0, .bytes = GUI_CELL_CACHE_ENTRIES * sizeof(*args->glyphs->cell)});
    char paths[4][CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_font_families.monospace, CliFontFamiliesCfg, .paths = paths);
    for (unsigned int face = GUI_FACE_CODE; face <= GUI_FACE_CODE_BOLD_ITALIC; face += 1u)
    {
        args->glyphs->has[face] = EMBED_CALL(glyphs_face_read, GlyphsCtx, .glyphs = args->glyphs, .face = face,
                                             .path = paths[face], .ranges = ranges);
    }
    EMBED_CALL(cli_font_families.find, CliFontFamiliesCfg, .families = s_sans,
               .count = sizeof(s_sans) / sizeof(s_sans[0]), .paths = paths);
    args->glyphs->has[GUI_FACE_HEAD] = EMBED_CALL(glyphs_face_read, GlyphsCtx, .glyphs = args->glyphs,
                                                  .face = GUI_FACE_HEAD, .path = paths[0], .ranges = ranges);
    EMBED_CALL(cli_font_families.find, CliFontFamiliesCfg, .families = s_serif,
               .count = sizeof(s_serif) / sizeof(s_serif[0]), .paths = paths);
    args->glyphs->has[GUI_FACE_TEXT] = EMBED_CALL(glyphs_face_read, GlyphsCtx, .glyphs = args->glyphs,
                                                  .face = GUI_FACE_TEXT, .path = paths[0], .ranges = ranges);
    g_cli_cells->persistent_buf_release(ranges, &g_gui_glyphs_cells);
    if (!args->glyphs->has[GUI_FACE_CODE])
    {
        cli_glyphs.free(args);
        mmgr_line_put(args->err_msg,
                      "the system has no monospace font the window reads: IBM Plex Mono, Consolas, Cascadia "
                      "Mono, DejaVu Sans Mono, Liberation Mono, Menlo or Courier New");
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
    g_cli_cells->persistent_buf_release(args->glyphs->entry, &g_gui_glyphs_cells);
    g_cli_cells->persistent_buf_release(args->glyphs->cell, &g_gui_glyphs_cells);
    g_cli_cells->persistent_buf_release(args->glyphs->contours, &g_gui_glyphs_cells);
    args->glyphs->entry = NULL;
    args->glyphs->cell = NULL;
    args->glyphs->contours = NULL;
    for (unsigned int face = 0u; face < GUI_FACES; face += 1u)
    {
        EMBED_CALL(glyphs_face_free, GlyphsCtx, .glyphs = args->glyphs, .face = face);
    }
}

int gui_glyphs_head_bold_read(const GuiGlyphsCfg *args)
{
    if (args->glyphs->has[GUI_FACE_HEAD_BOLD])
    {
        return 0;
    }
    unsigned int (*const ranges)[3] =
        g_cli_cells->temporary_buf_alloc(CLI_FONT_RANGES * sizeof(*ranges), &g_gui_glyphs_cells);
    if (ranges == NULL)
    {
        return 0;
    }
    char paths[4][CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_font_families.find, CliFontFamiliesCfg, .families = s_sans,
               .count = sizeof(s_sans) / sizeof(s_sans[0]), .paths = paths);
    args->glyphs->has[GUI_FACE_HEAD_BOLD] = EMBED_CALL(glyphs_face_read, GlyphsCtx, .glyphs = args->glyphs,
                                                       .face = GUI_FACE_HEAD_BOLD, .path = paths[1], .ranges = ranges);
    g_cli_cells->persistent_buf_release(ranges, &g_gui_glyphs_cells);
    return args->glyphs->has[GUI_FACE_HEAD_BOLD];
}

void gui_glyphs_head_bold_free(const GuiGlyphsCfg *args)
{
    // its letters' masks go with it: every mask let go, the others drawn again as they are asked for
    EMBED_CALL(glyphs_clear, GlyphsCtx, .glyphs = args->glyphs);
    EMBED_CALL(glyphs_face_free, GlyphsCtx, .glyphs = args->glyphs, .face = GUI_FACE_HEAD_BOLD);
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
// small glyph's thin stems need the most to read at full weight
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
        float moved_x[CLI_OUTLINE_POINTS];
        float moved_y[CLI_OUTLINE_POINTS];
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const unsigned int here = first + at;
            const unsigned int before = first + ((at + count - 1u) % count);
            const unsigned int after = first + ((at + 1u) % count);
            float in_x = args->contours->x[here] - args->contours->x[before];
            float in_y = args->contours->y[here] - args->contours->y[before];
            float out_x = args->contours->x[after] - args->contours->x[here];
            float out_y = args->contours->y[after] - args->contours->y[here];
            const float in_length = sqrtf((in_x * in_x) + (in_y * in_y));
            const float out_length = sqrtf((out_x * out_x) + (out_y * out_y));
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
            s_lifted[at] = (unsigned char)((powf((float)at / 255.0f, 1.0f / GUI_TEXT_GAMMA) * 255.0f) + 0.5f);
        }
        s_lifted_created = 1;
    }
    return s_lifted[args->cover & 0xffu];
}

// the outline in the glyphs' contours drawn as coverage bytes into `mask`, its cover a cell of its own: 1, or 0 where
// a cell could not be taken; a glyph of no pixels is a mask of none
EMBED_INLINE int glyphs_rasterize(const GlyphsCtx *args)
{
    GuiMask *mask = args->mask;
    CliMask measured;
    EMBED_CALL(cli_font_contours.rasterize, CliFontContoursCfg, .contours = args->glyphs_read->contours,
               .mask = &measured, .cover = NULL, .capacity = 0ull);
    mask->width = 0u;
    mask->height = 0u;
    mask->left = 0;
    mask->top = 0;
    mask->cover = NULL;
    if (measured.width == 0ull)
    {
        return 1;
    }
    const unsigned long long capacity = (measured.width * measured.height) + 4ull;
    float *const floats = g_cli_cells->temporary_buf_alloc(capacity * sizeof(*floats), &g_gui_glyphs_cells);
    unsigned char *const cover =
        g_cli_cells->persistent_buf_alloc(measured.width * measured.height, &g_gui_glyphs_cells);
    if ((floats == NULL) || (cover == NULL))
    {
        g_cli_cells->persistent_buf_release(floats, &g_gui_glyphs_cells);
        g_cli_cells->persistent_buf_release(cover, &g_gui_glyphs_cells);
        return 0;
    }
    CliMask drawn;
    EMBED_CALL(cli_font_contours.rasterize, CliFontContoursCfg, .contours = args->glyphs_read->contours, .mask = &drawn,
               .cover = floats, .capacity = capacity);
    for (unsigned long long at = 0ull; at < (drawn.width * drawn.height); at += 1ull)
    {
        // a coverage from 0 to 1 by 255, rounded, is a byte
        cover[at] = (unsigned char)((floats[at] * 255.0f) + 0.5f);
    }
    g_cli_cells->persistent_buf_release(floats, &g_gui_glyphs_cells);
    // a glyph's sides are far inside what an unsigned int represents
    mask->width = (unsigned int)drawn.width;
    mask->height = (unsigned int)drawn.height;
    mask->left = drawn.left;
    mask->top = drawn.top;
    mask->cover = cover;
    return 1;
}

const GuiMask *gui_glyph(const GuiGlyphsFaceLetterCfg *args)
{
    unsigned int face = args->face;
    unsigned int size = args->size;
    float *advance = args->advance;
    const unsigned int had =
        EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs, .face = face);
    const CliFont *const font = &args->glyphs->face[had];
    const unsigned int glyph = EMBED_CALL(cli_font.glyph, CliFontCfg, .font = font, .letter = args->letter);
    const float scale = (float)size / font->units;
    *advance = EMBED_CALL(cli_font_glyph_table.advance, CliFontGlyphCfg, .font = font, .glyph = glyph) * scale;
    GuiCachedGlyph *const entry =
        EMBED_CALL(glyphs_entry, GlyphsCtx, .glyphs = args->glyphs, .face = had, .glyph = glyph, .size = size);
    if (entry->used)
    {
        return &entry->mask;
    }
    args->glyphs->contours->points = 0u;
    args->glyphs->contours->contours = 0u;
    const float matrix[6] = {scale, 0.0f, 0.0f, scale, 0.0f, 0.0f};
    EMBED_CALL(cli_font_glyph_matrix.outline, CliFontGlyphMatrixCfg, .font = font, .glyph = glyph, .matrix = matrix,
               .depth = 0u, .out = args->glyphs->contours);
    EMBED_CALL(glyphs_darken, GlyphsCtx, .contours = args->glyphs->contours,
               .reach = EMBED_CALL(glyphs_darkening, GlyphsCtx, .size = size));
    if (!EMBED_CALL(glyphs_rasterize, GlyphsCtx, .glyphs_read = args->glyphs, .mask = &entry->mask))
    {
        return NULL;
    }
    entry->used = 1;
    entry->face = had;
    entry->glyph = glyph;
    entry->size = size;
    entry->advance = *advance;
    args->glyphs->cached += 1u;
    return &entry->mask;
}

float gui_glyphs_ascent(const GuiGlyphsFaceCfg *args)
{
    const CliFont *const font = &args->glyphs_read->face[gui_glyphs_face.face_had(args)];
    return font->ascent * (float)args->size / font->units;
}

float gui_glyphs_descent(const GuiGlyphsFaceCfg *args)
{
    const CliFont *const font = &args->glyphs_read->face[gui_glyphs_face.face_had(args)];
    return fabsf(font->descent) * (float)args->size / font->units;
}

float gui_glyphs_cell(const GuiGlyphsFaceLetterCfg *args)
{
    float advance = 0.0f;
    (void)EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = args->glyphs,
                     .face = GUI_FACE_CODE, .letter = 'M', .size = args->size, .advance = &advance);
    return advance;
}

// the cell keyed by `face`, `letter`, `size` and `color`, or an unused cell for it; the cells are emptied first when
// three quarters of them are in use
EMBED_INLINE GuiGlyphCell *glyphs_cell_entry(const GlyphsCtx *args)
{
    const unsigned int start =
        ((args->face * 31u) + (args->letter * 7u) + (args->size * 131u) + (args->color * 2654435761u)) %
        GUI_CELL_CACHE_ENTRIES;
    unsigned int at = start;
    do
    {
        GuiGlyphCell *const cell = &args->glyphs->cell[at];
        if (cell->used && (cell->face == args->face) && (cell->letter == args->letter) && (cell->size == args->size) &&
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
    unsigned int letter = args->letter;
    unsigned int size = args->size;
    GuiColor color = args->color;
    float *advance = args->advance;
    const unsigned int had =
        EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face);
    GuiGlyphCell *const cell = EMBED_CALL(glyphs_cell_entry, GlyphsCtx, .glyphs = glyphs, .face = had, .letter = letter,
                                          .size = size, .color = color);
    if (cell->used)
    {
        *advance = cell->advance;
        return cell;
    }
    const GuiMask *const mask = EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = glyphs,
                                           .face = had, .letter = letter, .size = size, .advance = advance);
    if (mask == NULL)
    {
        return NULL;
    }
    cell->face = had;
    cell->letter = letter;
    cell->size = size;
    cell->color = color;
    cell->advance = *advance;
    cell->width = mask->width;
    cell->height = mask->height;
    cell->left = mask->left;
    cell->top = mask->top;
    cell->red_blue = NULL;
    cell->green = NULL;
    cell->rest = NULL;
    const unsigned long long pixels = (unsigned long long)mask->width * mask->height;
    if (pixels != 0ull)
    {
        cell->red_blue = g_cli_cells->persistent_buf_alloc(3ull * pixels * sizeof(unsigned int), &g_gui_glyphs_cells);
        if (cell->red_blue == NULL)
        {
            return NULL;
        }
        cell->green = cell->red_blue + pixels;
        cell->rest = cell->green + pixels;
        // each pixel's share of the color is its cover by the color's alpha, two shares of 255 multiplied being a share
        // of 255 once divided by 255; the lanes are the color's channels by the share with half of 255 in, as the
        // raster's blend lays them
        const unsigned int alpha = GUI_ALPHA_OF(color);
        for (unsigned long long at = 0ull; at < pixels; at += 1ull)
        {
            const unsigned int share =
                ((EMBED_CALL(glyphs_lift, GlyphsCtx, .cover = mask->cover[at]) * alpha) + 127u) / 255u;
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
    if ((args->letter < GUI_ASCII_FIRST) || (args->letter >= GUI_ASCII_FIRST + GUI_ASCII_COUNT))
    {
        return EMBED_CALL(glyphs_cell_init, GlyphsCtx, .glyphs = args->glyphs, .face = args->face,
                          .letter = args->letter, .size = args->size, .color = args->color, .advance = advance);
    }
    const unsigned int had =
        EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs, .face = args->face);
    GuiAsciiTable *const table = EMBED_CALL(glyphs_ascii_table, GlyphsCtx, .glyphs = args->glyphs, .face = had,
                                            .size = args->size, .color = args->color);
    const unsigned int at = args->letter - GUI_ASCII_FIRST;
    if (table->cell[at] != NULL)
    {
        *advance = table->advance[at];
        return table->cell[at];
    }
    const unsigned int cells_before = args->glyphs->celled;
    const GuiGlyphCell *const cell =
        EMBED_CALL(glyphs_cell_init, GlyphsCtx, .glyphs = args->glyphs, .face = had, .letter = args->letter,
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
    unsigned long long at = 0ull;
    while (at < args->length)
    {
        unsigned long long consumed = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text,
                                               .size = args->length, .at = at, .length = &consumed);
        at += (consumed != 0ull) ? consumed : 1ull;
        float advance = 0.0f;
        const GuiGlyphCell *const cell = EMBED_CALL(gui_glyphs_face_letter.glyph_cell, GuiGlyphsFaceLetterCfg,
                                                    .glyphs = args->glyphs, .face = args->face, .letter = letter,
                                                    .size = args->size, .color = args->color, .advance = &advance);
        if (cell != NULL)
        {
            // a pixel's place, rounded, is far inside what an int represents
            EMBED_CALL(gui_glyphs_raster.glyph_cell_layout, GuiGlyphsRasterCfg, .raster = args->raster, .cell = cell,
                       .x = (int)roundf(x), .y = args->y, .clip = args->clip);
        }
        x += advance;
    }
    return x;
}

float gui_text_width(const GuiGlyphsFaceSizeCfg *args)
{
    float width = 0.0f;
    unsigned long long at = 0ull;
    while (at < args->length)
    {
        unsigned long long consumed = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text,
                                               .size = args->length, .at = at, .length = &consumed);
        at += (consumed != 0ull) ? consumed : 1ull;
        float advance = 0.0f;
        (void)EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = args->glyphs,
                         .face = args->face, .letter = letter, .size = args->size, .advance = &advance);
        width += advance;
    }
    return width;
}
