// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs.c: the window's text. A face is read whole from its file into a cell and its character map kept in a cell of
// the ranges it has; a glyph is outlined at its pixel size, measured, rasterized as coverage in a cell of its own
// size and kept as bytes in a slot found by its key, every slot let go together as they fill
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

int gui_glyphs_made(GuiGlyphs *glyphs, VerbaLinea *err_msg)
{
    memor.set(&(MemoriaCfg){.dst = glyphs, .val = 0, .bytes = sizeof(*glyphs)});
    unsigned int(*const ranges)[3] = g_cli_cells->persistent_buf_alloc(CLI_FONT_RANGES * sizeof(*ranges), &s_owner);
    glyphs->contours = g_cli_cells->persistent_buf_alloc(sizeof(*glyphs->contours), &s_owner);
    glyphs->slot = g_cli_cells->persistent_buf_alloc(GUI_GLYPH_SLOTS * sizeof(*glyphs->slot), &s_owner);
    if ((ranges == NULL) || (glyphs->contours == NULL) || (glyphs->slot == NULL))
    {
        g_cli_cells->persistent_buf_release(ranges, &s_owner);
        gui_glyphs_given(glyphs);
        mmgr_line_put(err_msg, "the cells the window's glyphs are kept in could not be taken");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = glyphs->slot, .val = 0, .bytes = GUI_GLYPH_SLOTS * sizeof(*glyphs->slot)});
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
    g_cli_cells->persistent_buf_release(glyphs->slot, &s_owner);
    g_cli_cells->persistent_buf_release(glyphs->contours, &s_owner);
    glyphs->slot = NULL;
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
        const GuiMask *const mask = gui_glyph(glyphs, face, letter, size, &advance);
        if ((mask != NULL) && (mask->cover != NULL))
        {
            // a pixel's place, rounded, is far inside what an int represents
            gui_raster_masked(raster, mask, (int)roundf(x) + mask->left, y - mask->top, color, clip);
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
