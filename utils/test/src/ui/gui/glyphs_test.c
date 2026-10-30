// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs_test.c: the glyphs' check: the system's code font is read, a letter's glyph is drawn once and kept, a space
// has an advance and no pixels, the code face's cell is its glyphs' advance, and a text drawn lands its pixels where
// its letters stand and ends where its width says
#include "../../../../../ui/gui/src/glyphs.h"
#include "../cli/cli_check.h"

// whether any pixel of `rect` on the raster is not `ground`
static int painted(const GuiRaster *raster, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (gui_rect_holds((GuiRect){0, 0, (int)raster->width, (int)raster->height}, x, y) &&
                (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground))
            {
                return 1;
            }
        }
    }
    return 0;
}

static float off(float one, float other)
{
    return (one > other) ? one - other : other - one;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    const int made = gui_glyphs_made(&glyphs, &why);
    cli_check(made, "the system's fonts are read");
    if (!made)
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_checked();
    }
    cli_check(glyphs.has[GUI_FACE_CODE], "the code face is read");
    cli_check(gui_face_had(&glyphs, GUI_FACE_CODE_BOLD) < GUI_FACES, "a face asked for is answered with one read");

    float advance = 0.0f;
    const GuiMask *const letter = gui_glyph(&glyphs, GUI_FACE_CODE, 'M', 13u, &advance);
    cli_check(letter != NULL, "the glyph of M at thirteen pixels is drawn");
    cli_check((letter != NULL) && (letter->width > 4u) && (letter->height > 6u), "the glyph has pixels of an M's size");
    cli_check((advance > 5.0f) && (advance < 12.0f), "the glyph's advance is a code cell at thirteen pixels");
    cli_check((letter != NULL) && (letter->top > 0) && (letter->top <= 13), "the glyph's top stands above the baseline");
    float again = 0.0f;
    cli_check(gui_glyph(&glyphs, GUI_FACE_CODE, 'M', 13u, &again) == letter, "the same glyph asked again is the one kept");
    cli_check(off(again, advance) < 1e-6f, "the glyph kept keeps its advance");
    cli_check(gui_glyph(&glyphs, GUI_FACE_CODE, 'M', 26u, &again) != letter, "the glyph at another size is drawn anew");
    cli_check(off(again, 2.0f * advance) < 0.01f, "the advance at twice the size is twice");

    float space_advance = 0.0f;
    const GuiMask *const space = gui_glyph(&glyphs, GUI_FACE_CODE, ' ', 13u, &space_advance);
    cli_check((space != NULL) && (space->width == 0u) && (space->cover == NULL), "a space has no pixels");
    cli_check(off(space_advance, advance) < 1e-6f, "a space advances a cell as a letter does");

    cli_check(off(gui_glyphs_cell(&glyphs, 13u), advance) < 1e-6f, "the code face's cell is its glyphs' advance");

    // a glyph in a color is a cell of lanes, kept by its key, with no lanes where the glyph has no pixels
    float cell_advance = 0.0f;
    const GuiGlyphCell *const cell = gui_glyph_cell(&glyphs, GUI_FACE_CODE, 'M', 13u, GUI_RGB(0xffu, 0xffu, 0xffu), &cell_advance);
    cli_check((cell != NULL) && (cell->width == letter->width) && (cell->height == letter->height) && (cell->left == letter->left) &&
                  (cell->top == letter->top) && (cell->red_blue != NULL),
              "the cell of M in white is the glyph's box with lanes");
    cli_check(off(cell_advance, advance) < 1e-6f, "the cell keeps the glyph's advance");
    cli_check(gui_glyph_cell(&glyphs, GUI_FACE_CODE, 'M', 13u, GUI_RGB(0xffu, 0xffu, 0xffu), &cell_advance) == cell,
              "the same cell asked again is the one kept");
    cli_check(gui_glyph_cell(&glyphs, GUI_FACE_CODE, 'M', 13u, GUI_RGB(0xffu, 0x00u, 0x00u), &cell_advance) != cell,
              "the glyph in another color is another cell");
    const GuiGlyphCell *const space_cell = gui_glyph_cell(&glyphs, GUI_FACE_CODE, ' ', 13u, GUI_RGB(0xffu, 0xffu, 0xffu), &cell_advance);
    cli_check((space_cell != NULL) && (space_cell->red_blue == NULL), "a space's cell has no lanes");
    unsigned int whole = 0u;
    unsigned int bare = 0u;
    for (unsigned long long at = 0ull; (cell != NULL) && (at < (unsigned long long)cell->width * cell->height); at += 1ull)
    {
        whole += (cell->rest[at] == 0u) ? 1u : 0u;
        bare += (cell->rest[at] == 255u) ? 1u : 0u;
    }
    cli_check((whole > 0u) && (bare > 0u), "a cell's lanes keep none of the ground where the glyph covers a pixel whole and all of it where it covers none");
    cli_check(gui_glyphs_ascent(&glyphs, GUI_FACE_CODE, 13u) > 8.0f, "the ascent at thirteen pixels is most of them");
    cli_check(gui_glyphs_descent(&glyphs, GUI_FACE_CODE, 13u) > 1.0f, "the descent at thirteen pixels is below the baseline");

    GuiRaster raster;
    const GuiColor black = GUI_RGB(0u, 0u, 0u);
    const GuiColor white = GUI_RGB(0xffu, 0xffu, 0xffu);
    cli_check(gui_raster_made(&raster, 64u, 24u), "a raster for the text is made");
    gui_raster_filled(&raster, (GuiRect){0, 0, 64, 24}, black);
    const float after = gui_text_drawn(&raster, &glyphs, GUI_FACE_CODE, 13u, 2.0f, 18, "Hi", 2ull, white,
                                       (GuiRect){0, 0, 64, 24});
    cli_check(off(after, 2.0f + (2.0f * advance)) < 0.01f, "a text drawn ends two cells on");
    cli_check(painted(&raster, (GuiRect){2, 5, (int)advance, 14}, black), "the first letter paints its cell");
    cli_check(painted(&raster, (GuiRect){2 + (int)advance, 5, (int)advance, 14}, black), "the second letter paints its cell");
    cli_check(!painted(&raster, (GuiRect){2 + (int)(2.0f * advance) + 1, 0, 40, 24}, black),
              "nothing is painted past the text");
    cli_check(!painted(&raster, (GuiRect){0, 19, 64, 5}, black), "an H and an i paint nothing below the baseline");
    cli_check(off(gui_text_width(&glyphs, GUI_FACE_CODE, 13u, "Hi", 2ull), 2.0f * advance) < 0.01f,
              "a text's width is its letters' advances");
    gui_raster_filled(&raster, (GuiRect){0, 0, 64, 24}, black);
    gui_text_drawn(&raster, &glyphs, GUI_FACE_CODE, 13u, 2.0f, 18, "Hi", 2ull, white, (GuiRect){0, 0, 1, 1});
    cli_check(!painted(&raster, (GuiRect){1, 1, 63, 23}, black), "a text is drawn inside the clip alone");
    gui_text_drawn(&raster, &glyphs, GUI_FACE_CODE, 13u, 2.0f, 18, "\xc3\xa9", 2ull, white, (GuiRect){0, 0, 64, 24});
    cli_check(painted(&raster, (GuiRect){2, 2, (int)advance, 16}, black), "a letter of two bytes draws one glyph");

    gui_raster_given(&raster);
    gui_glyphs_given(&glyphs);
    cli_check(glyphs.slot == NULL, "the glyphs given back keep no slots");
    return cli_checked();
}
