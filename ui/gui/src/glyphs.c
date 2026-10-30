// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs.c: the window's text. A face is read whole from its file into a cell and its character map kept in a cell of
// the ranges it has; a glyph is outlined at its pixel size, measured, rasterized as coverage in a cell of its own
// size and kept as bytes in a slot found by its key, every slot let go together as they fill; a glyph in a color is a
// cell of lanes laid for the one blend, kept the same way, which the text is drawn through
#include "glyphs.h"

#include "../../cli/src/command_line/cli_internal.h"
#include "../../cli/src/text/text.h"

#include <math.h>

static CellOwner s_owner = MMGR_CELL_OWNER("gui_glyphs");

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

// the face `face` read from the file at `path` through the ranges room `ranges`, its ranges then kept in a cell of
// their own count: 1, or 0 where there is no path, no file or no font in it
static int glyphs_face_read(GuiGlyphs *glyphs, unsigned int face, const char *path, unsigned int (*ranges)[3])
{
    if (path[0] == '\0')
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const data = cli_file_read(path, &size);
    if (data == NULL)
    {
        return 0;
    }
    CliFont *const font = &glyphs->face[face];
    if (!cli_font_read(font, data, size, ranges))
    {
        cli_file_let_go(data, size);
        return 0;
    }
    const unsigned long long bytes = (unsigned long long)font->ranges * sizeof(ranges[0]);
    unsigned int(*const kept)[3] = g_cli_cells->persistent_buf_alloc((bytes != 0ull) ? bytes : sizeof(ranges[0]), &s_owner);
    if (kept == NULL)
    {
        cli_file_let_go(data, size);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = kept, .src = ranges, .bytes = bytes});
    font->range = kept;
    glyphs->data[face] = data;
    glyphs->data_size[face] = size;
    return 1;
}

// the cache emptied: every entry's coverage mask freed and every entry marked unused
static void glyphs_emptied(GuiGlyphs *glyphs)
{
    for (unsigned int at = 0u; at < GUI_GLYPH_SLOTS; at += 1u)
    {
        GuiCachedGlyph *const slot = &glyphs->slot[at];
        if (slot->used)
        {
            g_cli_cells->persistent_buf_release(slot->mask.cover, &s_owner);
            slot->mask.cover = NULL;
            slot->used = 0;
        }
    }
    glyphs->cached = 0u;
}

// the cells emptied: every cell's lanes given back and every cell marked unused
static void glyphs_cells_emptied(GuiGlyphs *glyphs)
{
    for (unsigned int at = 0u; at < GUI_CELL_SLOTS; at += 1u)
    {
        GuiGlyphCell *const cell = &glyphs->cell[at];
        if (cell->used)
        {
            g_cli_cells->persistent_buf_release(cell->red_blue, &s_owner);
            cell->red_blue = NULL;
            cell->green = NULL;
            cell->rest = NULL;
            cell->used = 0;
        }
    }
    glyphs->celled = 0u;
    // the ASCII tables point into the cells: they go with them
    memor.set(&(MemoriaCfg){.dst = glyphs->ascii, .val = 0, .bytes = sizeof(glyphs->ascii)});
}

int gui_glyphs_made(GuiGlyphs *glyphs, VerbaLinea *err_msg)
{
    memor.set(&(MemoriaCfg){.dst = glyphs, .val = 0, .bytes = sizeof(*glyphs)});
    unsigned int(*const ranges)[3] = g_cli_cells->persistent_buf_alloc(CLI_FONT_RANGES * sizeof(*ranges), &s_owner);
    glyphs->contours = g_cli_cells->persistent_buf_alloc(sizeof(*glyphs->contours), &s_owner);
    glyphs->slot = g_cli_cells->persistent_buf_alloc(GUI_GLYPH_SLOTS * sizeof(*glyphs->slot), &s_owner);
    glyphs->cell = g_cli_cells->persistent_buf_alloc(GUI_CELL_SLOTS * sizeof(*glyphs->cell), &s_owner);
    if ((ranges == NULL) || (glyphs->contours == NULL) || (glyphs->slot == NULL) || (glyphs->cell == NULL))
    {
        g_cli_cells->persistent_buf_release(ranges, &s_owner);
        gui_glyphs_given(glyphs);
        mmgr_line_put(err_msg, "the cells the window's glyphs are kept in could not be taken");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = glyphs->slot, .val = 0, .bytes = GUI_GLYPH_SLOTS * sizeof(*glyphs->slot)});
    memor.set(&(MemoriaCfg){.dst = glyphs->cell, .val = 0, .bytes = GUI_CELL_SLOTS * sizeof(*glyphs->cell)});
    char paths[4][CLI_PATH_MAX_LEN];
    cli_font_monospace(paths);
    for (unsigned int face = GUI_FACE_CODE; face <= GUI_FACE_CODE_BOLD_ITALIC; face += 1u)
    {
        glyphs->has[face] = glyphs_face_read(glyphs, face, paths[face], ranges);
    }
    cli_font_found(s_sans, sizeof(s_sans) / sizeof(s_sans[0]), paths);
    glyphs->has[GUI_FACE_HEAD] = glyphs_face_read(glyphs, GUI_FACE_HEAD, paths[0], ranges);
    cli_font_found(s_serif, sizeof(s_serif) / sizeof(s_serif[0]), paths);
    glyphs->has[GUI_FACE_TEXT] = glyphs_face_read(glyphs, GUI_FACE_TEXT, paths[0], ranges);
    g_cli_cells->persistent_buf_release(ranges, &s_owner);
    if (!glyphs->has[GUI_FACE_CODE])
    {
        gui_glyphs_given(glyphs);
        mmgr_line_put(err_msg, "the system has no monospace font the window reads: IBM Plex Mono, Consolas, Cascadia "
                               "Mono, DejaVu Sans Mono, Liberation Mono, Menlo or Courier New");
        return 0;
    }
    return 1;
}

void gui_glyphs_given(GuiGlyphs *glyphs)
{
    if (glyphs->slot != NULL)
    {
        glyphs_emptied(glyphs);
    }
    if (glyphs->cell != NULL)
    {
        glyphs_cells_emptied(glyphs);
    }
    g_cli_cells->persistent_buf_release(glyphs->slot, &s_owner);
    g_cli_cells->persistent_buf_release(glyphs->cell, &s_owner);
    g_cli_cells->persistent_buf_release(glyphs->contours, &s_owner);
    glyphs->slot = NULL;
    glyphs->cell = NULL;
    glyphs->contours = NULL;
    for (unsigned int face = 0u; face < GUI_FACES; face += 1u)
    {
        if (glyphs->has[face])
        {
            g_cli_cells->persistent_buf_release(glyphs->face[face].range, &s_owner);
            cli_file_let_go(glyphs->data[face], glyphs->data_size[face]);
        }
        glyphs->has[face] = 0;
        glyphs->data[face] = NULL;
    }
}

unsigned int gui_face_had(const GuiGlyphs *glyphs, unsigned int face)
{
    return ((face < GUI_FACES) && glyphs->has[face]) ? face : GUI_FACE_CODE;
}

// the cache entry keyed by `face`, `glyph` and `size`, or an unused entry for it; the cache is emptied first when
// three quarters of its entries are in use
static GuiCachedGlyph *glyphs_slot(GuiGlyphs *glyphs, unsigned int face, unsigned int glyph, unsigned int size)
{
    const unsigned int start = ((face * 31u) + (glyph * 7u) + (size * 131u)) % GUI_GLYPH_SLOTS;
    unsigned int at = start;
    do
    {
        GuiCachedGlyph *const slot = &glyphs->slot[at];
        if (slot->used && (slot->face == face) && (slot->glyph == glyph) && (slot->size == size))
        {
            return slot;
        }
        if (!slot->used)
        {
            break;
        }
        at = (at + 1u) % GUI_GLYPH_SLOTS;
    } while (at != start);
    if ((glyphs->cached * 4u) >= (GUI_GLYPH_SLOTS * 3u))
    {
        glyphs_emptied(glyphs);
        at = start;
    }
    return &glyphs->slot[at];
}

// the stem darkening at `size` pixels an em: how far each side of a stroke is pushed out, in pixels, GUI_DARKEN_MOST
// at GUI_DARKEN_SMALL pixels and below, falling straight to GUI_DARKEN_LEAST at GUI_DARKEN_LARGE and above, as a
// small glyph's thin stems need the most to read at full weight
static float glyphs_darkening(unsigned int size)
{
    if (size <= GUI_DARKEN_SMALL)
    {
        return GUI_DARKEN_MOST;
    }
    if (size >= GUI_DARKEN_LARGE)
    {
        return GUI_DARKEN_LEAST;
    }
    const float along = (float)(size - GUI_DARKEN_SMALL) / (float)(GUI_DARKEN_LARGE - GUI_DARKEN_SMALL);
    return GUI_DARKEN_MOST + ((GUI_DARKEN_LEAST - GUI_DARKEN_MOST) * along);
}

// the contours' strokes expanded by `reach` pixels a side: each point moved along the miter of its two edges' normals
// to the side away from the ink, the ink lying to the right of a contour's travel as TrueType winds it with y up: an
// outer contour grows and a counter shrinks, both widening the stroke; the miter kept to twice the reach at a sharp
// turn
static void glyphs_darkened(CliContours *contours, float reach)
{
    unsigned int first = 0u;
    for (unsigned int contour = 0u; contour < contours->contours; contour += 1u)
    {
        const unsigned int end = contours->end[contour];
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
            float in_x = contours->x[here] - contours->x[before];
            float in_y = contours->y[here] - contours->y[before];
            float out_x = contours->x[after] - contours->x[here];
            float out_y = contours->y[after] - contours->y[here];
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
            const float scale = (dot > 0.5f) ? (2.0f * reach) / dot : 4.0f * reach;
            moved_x[at] = contours->x[here] + (normal_x * scale * 0.5f);
            moved_y[at] = contours->y[here] + (normal_y * scale * 0.5f);
        }
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            contours->x[first + at] = moved_x[at];
            contours->y[first + at] = moved_y[at];
        }
        first = end;
    }
}

// the coverage lifted through the blend's gamma: each share of 255 raised to one over GUI_TEXT_GAMMA, made once,
// which brings a stroke's partly covered edge pixels up toward its whole ones and reads crisper and bolder
static unsigned char s_lifted[256];
static int s_lifted_made;

static unsigned int glyphs_lifted(unsigned int cover)
{
    if (!s_lifted_made)
    {
        for (unsigned int at = 0u; at < 256u; at += 1u)
        {
            // a share from 0 to 1 raised and scaled to 255, rounded, is a byte
            s_lifted[at] = (unsigned char)((powf((float)at / 255.0f, 1.0f / GUI_TEXT_GAMMA) * 255.0f) + 0.5f);
        }
        s_lifted_made = 1;
    }
    return s_lifted[cover & 0xffu];
}

// the outline in the glyphs' contours drawn as coverage bytes into `mask`, its cover a cell of its own: 1, or 0 where
// a cell could not be taken; a glyph of no pixels is a mask of none
static int glyphs_rasterized(const GuiGlyphs *glyphs, GuiMask *mask)
{
    CliMask measured;
    cli_font_rasterize(glyphs->contours, &measured, NULL, 0ull);
    mask->width = 0u;
    mask->height = 0u;
    mask->left = 0;
    mask->top = 0;
    mask->cover = NULL;
    if (measured.width == 0ull)
    {
        return 1;
    }
    const unsigned long long room = (measured.width * measured.height) + 4ull;
    float *const floats = g_cli_cells->persistent_buf_alloc(room * sizeof(*floats), &s_owner);
    unsigned char *const cover = g_cli_cells->persistent_buf_alloc(measured.width * measured.height, &s_owner);
    if ((floats == NULL) || (cover == NULL))
    {
        g_cli_cells->persistent_buf_release(floats, &s_owner);
        g_cli_cells->persistent_buf_release(cover, &s_owner);
        return 0;
    }
    CliMask drawn;
    cli_font_rasterize(glyphs->contours, &drawn, floats, room);
    for (unsigned long long at = 0ull; at < (drawn.width * drawn.height); at += 1ull)
    {
        // a coverage from 0 to 1 by 255, rounded, is a byte
        cover[at] = (unsigned char)((floats[at] * 255.0f) + 0.5f);
    }
    g_cli_cells->persistent_buf_release(floats, &s_owner);
    // a glyph's sides are far inside what an unsigned int represents
    mask->width = (unsigned int)drawn.width;
    mask->height = (unsigned int)drawn.height;
    mask->left = drawn.left;
    mask->top = drawn.top;
    mask->cover = cover;
    return 1;
}

const GuiMask *gui_glyph(GuiGlyphs *glyphs, unsigned int face, unsigned int letter, unsigned int size, float *advance)
{
    const unsigned int had = gui_face_had(glyphs, face);
    const CliFont *const font = &glyphs->face[had];
    const unsigned int glyph = cli_font_glyph(font, letter);
    const float scale = (float)size / font->units;
    *advance = cli_font_advance(font, glyph) * scale;
    GuiCachedGlyph *const slot = glyphs_slot(glyphs, had, glyph, size);
    if (slot->used)
    {
        return &slot->mask;
    }
    glyphs->contours->points = 0u;
    glyphs->contours->contours = 0u;
    const float matrix[6] = {scale, 0.0f, 0.0f, scale, 0.0f, 0.0f};
    cli_font_outline(font, glyph, matrix, 0u, glyphs->contours);
    glyphs_darkened(glyphs->contours, glyphs_darkening(size));
    if (!glyphs_rasterized(glyphs, &slot->mask))
    {
        return NULL;
    }
    slot->used = 1;
    slot->face = had;
    slot->glyph = glyph;
    slot->size = size;
    slot->advance = *advance;
    glyphs->cached += 1u;
    return &slot->mask;
}

float gui_glyphs_ascent(const GuiGlyphs *glyphs, unsigned int face, unsigned int size)
{
    const CliFont *const font = &glyphs->face[gui_face_had(glyphs, face)];
    return font->ascent * (float)size / font->units;
}

float gui_glyphs_descent(const GuiGlyphs *glyphs, unsigned int face, unsigned int size)
{
    const CliFont *const font = &glyphs->face[gui_face_had(glyphs, face)];
    return fabsf(font->descent) * (float)size / font->units;
}

float gui_glyphs_cell(GuiGlyphs *glyphs, unsigned int size)
{
    float advance = 0.0f;
    (void)gui_glyph(glyphs, GUI_FACE_CODE, 'M', size, &advance);
    return advance;
}

// the cell keyed by `face`, `letter`, `size` and `color`, or an unused cell for it; the cells are emptied first when
// three quarters of them are in use
static GuiGlyphCell *glyphs_cell_slot(GuiGlyphs *glyphs, unsigned int face, unsigned int letter, unsigned int size, GuiColor color)
{
    const unsigned int start = ((face * 31u) + (letter * 7u) + (size * 131u) + (color * 2654435761u)) % GUI_CELL_SLOTS;
    unsigned int at = start;
    do
    {
        GuiGlyphCell *const cell = &glyphs->cell[at];
        if (cell->used && (cell->face == face) && (cell->letter == letter) && (cell->size == size) && (cell->color == color))
        {
            return cell;
        }
        if (!cell->used)
        {
            break;
        }
        at = (at + 1u) % GUI_CELL_SLOTS;
    } while (at != start);
    if ((glyphs->celled * 4u) >= (GUI_CELL_SLOTS * 3u))
    {
        glyphs_cells_emptied(glyphs);
        at = start;
    }
    return &glyphs->cell[at];
}

static const GuiGlyphCell *glyphs_cell_made(GuiGlyphs *glyphs, unsigned int face, unsigned int letter, unsigned int size, GuiColor color,
                                   float *advance)
{
    const unsigned int had = gui_face_had(glyphs, face);
    GuiGlyphCell *const cell = glyphs_cell_slot(glyphs, had, letter, size, color);
    if (cell->used)
    {
        *advance = cell->advance;
        return cell;
    }
    const GuiMask *const mask = gui_glyph(glyphs, had, letter, size, advance);
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
        cell->red_blue = g_cli_cells->persistent_buf_alloc(3ull * pixels * sizeof(unsigned int), &s_owner);
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
            const unsigned int share = ((glyphs_lifted(mask->cover[at]) * alpha) + 127u) / 255u;
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
static GuiAsciiTable *glyphs_ascii_table(GuiGlyphs *glyphs, unsigned int face, unsigned int size, GuiColor color)
{
    GuiAsciiTable *const last = &glyphs->ascii[glyphs->ascii_last];
    if (last->used && (last->face == face) && (last->size == size) && (last->color == color))
    {
        return last;
    }
    for (unsigned int at = 0u; at < GUI_ASCII_TABLES; at += 1u)
    {
        GuiAsciiTable *const table = &glyphs->ascii[at];
        if (table->used && (table->face == face) && (table->size == size) && (table->color == color))
        {
            glyphs->ascii_last = at;
            return table;
        }
    }
    const unsigned int taken = glyphs->ascii_next;
    glyphs->ascii_next = (taken + 1u) % GUI_ASCII_TABLES;
    GuiAsciiTable *const table = &glyphs->ascii[taken];
    memor.set(&(MemoriaCfg){.dst = table, .val = 0, .bytes = sizeof(*table)});
    table->used = 1;
    table->face = face;
    table->size = size;
    table->color = color;
    glyphs->ascii_last = taken;
    return table;
}

const GuiGlyphCell *gui_glyph_cell(GuiGlyphs *glyphs, unsigned int face, unsigned int letter, unsigned int size, GuiColor color,
                                   float *advance)
{
    if ((letter < GUI_ASCII_FIRST) || (letter >= GUI_ASCII_FIRST + GUI_ASCII_COUNT))
    {
        return glyphs_cell_made(glyphs, face, letter, size, color, advance);
    }
    const unsigned int had = gui_face_had(glyphs, face);
    GuiAsciiTable *const table = glyphs_ascii_table(glyphs, had, size, color);
    const unsigned int at = letter - GUI_ASCII_FIRST;
    if (table->cell[at] != NULL)
    {
        *advance = table->advance[at];
        return table->cell[at];
    }
    const unsigned int cells_before = glyphs->celled;
    const GuiGlyphCell *const cell = glyphs_cell_made(glyphs, had, letter, size, color, advance);
    // a cell made may have emptied every cell and every table with them: the table is found again before it is filled
    GuiAsciiTable *const kept = (glyphs->celled < cells_before) ? glyphs_ascii_table(glyphs, had, size, color) : table;
    kept->cell[at] = cell;
    kept->advance[at] = *advance;
    return cell;
}

void gui_glyph_cell_laid(GuiRaster *raster, const GuiGlyphCell *cell, int x, int y, GuiRect clip)
{
    if (cell->red_blue != NULL)
    {
        gui_raster_lanes_laid(raster, cell->red_blue, cell->green, cell->rest, cell->width, cell->height, x + cell->left, y - cell->top, clip);
    }
}

float gui_text_drawn(GuiRaster *raster, GuiGlyphs *glyphs, unsigned int face, unsigned int size, float x, int y,
                     const char *text, unsigned long long length, GuiColor color, GuiRect clip)
{
    unsigned long long at = 0ull;
    while (at < length)
    {
        unsigned long long took = 0ull;
        const unsigned int letter = cli_text_letter(text, length, at, &took);
        at += (took != 0ull) ? took : 1ull;
        float advance = 0.0f;
        const GuiGlyphCell *const cell = gui_glyph_cell(glyphs, face, letter, size, color, &advance);
        if (cell != NULL)
        {
            // a pixel's place, rounded, is far inside what an int represents
            gui_glyph_cell_laid(raster, cell, (int)roundf(x), y, clip);
        }
        x += advance;
    }
    return x;
}

float gui_text_width(GuiGlyphs *glyphs, unsigned int face, unsigned int size, const char *text,
                     unsigned long long length)
{
    float width = 0.0f;
    unsigned long long at = 0ull;
    while (at < length)
    {
        unsigned long long took = 0ull;
        const unsigned int letter = cli_text_letter(text, length, at, &took);
        at += (took != 0ull) ? took : 1ull;
        float advance = 0.0f;
        (void)gui_glyph(glyphs, face, letter, size, &advance);
        width += advance;
    }
    return width;
}
