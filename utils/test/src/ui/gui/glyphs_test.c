// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs_test.c: the glyphs' check: the system's code font is read, a letter's glyph is drawn once and saved, a space
// has an advance and no pixels, the code face's cell is its glyphs' advance, and a text drawn lands its pixels where
// its letters stand and ends where its width says
#include "../../../../../ui/gui/src/glyphs.h"
#include "../cli/cli_check.h"

// whether any pixel of `rect` on the raster is not `background_fill`
static int painted(const GuiRaster *raster, GuiRect rect, GuiColor background_fill)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = (GuiRect){0, 0, (int)raster->width, (int)raster->height}, .x = x, .y = y) &&
                (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != background_fill))
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
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    const int made = EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why);
    cli_check(made, "the system's fonts are read");
    if (!made)
    {
        mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    cli_check(glyphs.has[GUI_FACE_CODE], "the code face is read");
    cli_check(EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_CODE_BOLD) < GUI_FACES, "a face asked for is answered with one read");
    cli_check(EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_HEAD_BOLD) != GUI_FACE_HEAD_BOLD, "the interface's bold is not read until it is asked for");
    float regular_width = 0.0f;
    (void)EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_HEAD, .letter = (unsigned int)'o', .size = 88u, .advance = &regular_width);
    const int bold_read = EMBED_CALL(cli_glyphs.head_bold_read, GuiGlyphsCfg, .glyphs = &glyphs);
    cli_check(!bold_read || (EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_HEAD_BOLD) == GUI_FACE_HEAD_BOLD), "asked for, it is read where the system has it");
    float bold_width = 0.0f;
    (void)EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_HEAD_BOLD, .letter = (unsigned int)'o', .size = 88u, .advance = &bold_width);
    cli_check(!bold_read || (bold_width > regular_width), "the bold's letters stand wider than the regular's");
    if (bold_read)
    {
        EMBED_CALL(cli_glyphs.head_bold_free, GuiGlyphsCfg, .glyphs = &glyphs);
    }
    cli_check((EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_HEAD_BOLD) != GUI_FACE_HEAD_BOLD) && (glyphs.data[GUI_FACE_HEAD_BOLD] == NULL), "given back it holds no file");

    float advance = 0.0f;
    const GuiMask *const letter = EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .letter = 'M', .size = 13u, .advance = &advance);
    cli_check(letter != NULL, "the glyph of M at thirteen pixels is drawn");
    cli_check((letter != NULL) && (letter->width > 4u) && (letter->height > 6u), "the glyph has pixels of an M's size");
    cli_check((advance > 5.0f) && (advance < 12.0f), "the glyph's advance is a code cell at thirteen pixels");
    cli_check((letter != NULL) && (letter->top > 0) && (letter->top <= 13), "the glyph's top stands above the baseline");
    float again = 0.0f;
    cli_check(EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .letter = 'M', .size = 13u, .advance = &again) == letter, "the same glyph asked again is the one kept");
    cli_check(off(again, advance) < 1e-6f, "the glyph kept keeps its advance");
    cli_check(EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .letter = 'M', .size = 26u, .advance = &again) != letter, "the glyph at another size is drawn anew");
    cli_check(off(again, 2.0f * advance) < 0.01f, "the advance at twice the size is twice");

    float space_advance = 0.0f;
    const GuiMask *const space = EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .letter = ' ', .size = 13u, .advance = &space_advance);
    cli_check((space != NULL) && (space->width == 0u) && (space->cover == NULL), "a space has no pixels");
    cli_check(off(space_advance, advance) < 1e-6f, "a space advances a cell as a letter does");

    cli_check(off(EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .size = 13u), advance) < 1e-6f, "the code face's cell is its glyphs' advance");

    // a glyph in a color is a cell of lanes, saved by its key, with no lanes where the glyph has no pixels
    float cell_advance = 0.0f;
    const GuiGlyphCell *const cell = EMBED_CALL(gui_glyphs_face_letter.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .letter = 'M', .size = 13u, .color = GUI_RGB(0xffu, 0xffu, 0xffu), .advance = &cell_advance);
    cli_check((cell != NULL) && (cell->width == letter->width) && (cell->height == letter->height) && (cell->left == letter->left) &&
                  (cell->top == letter->top) && (cell->red_blue != NULL),
              "the cell of M in white is the glyph's box with lanes");
    cli_check(off(cell_advance, advance) < 1e-6f, "the cell keeps the glyph's advance");
    cli_check(EMBED_CALL(gui_glyphs_face_letter.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .letter = 'M', .size = 13u, .color = GUI_RGB(0xffu, 0xffu, 0xffu), .advance = &cell_advance) == cell,
              "the same cell asked again is the one kept");
    cli_check(EMBED_CALL(gui_glyphs_face_letter.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .letter = 'M', .size = 13u, .color = GUI_RGB(0xffu, 0x00u, 0x00u), .advance = &cell_advance) != cell,
              "the glyph in another color is another cell");
    cli_check((glyphs.ascii[glyphs.ascii_last].cell['M' - GUI_ASCII_FIRST] != NULL) && (glyphs.ascii[glyphs.ascii_last].color == GUI_RGB(0xffu, 0x00u, 0x00u)),
              "an ASCII letter's cell stands in its face, size and color's table at its index");
    const GuiGlyphCell *const space_cell = EMBED_CALL(gui_glyphs_face_letter.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .letter = ' ', .size = 13u, .color = GUI_RGB(0xffu, 0xffu, 0xffu), .advance = &cell_advance);
    cli_check((space_cell != NULL) && (space_cell->red_blue == NULL), "a space's cell has no lanes");
    unsigned int whole = 0u;
    unsigned int bare = 0u;
    for (unsigned long long at = 0ull; (cell != NULL) && (at < (unsigned long long)cell->width * cell->height); at += 1ull)
    {
        whole += (cell->rest[at] == 0u) ? 1u : 0u;
        bare += (cell->rest[at] == 255u) ? 1u : 0u;
    }
    cli_check((whole > 0u) && (bare > 0u), "a cell's lanes keep none of the ground where the glyph covers a pixel whole and all of it where it covers none");
    cli_check(EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_CODE, .size = 13u) > 8.0f, "the ascent at thirteen pixels is most of them");
    cli_check(EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_CODE, .size = 13u) > 1.0f, "the descent at thirteen pixels is below the baseline");

    GuiRaster raster;
    const GuiColor black = GUI_RGB(0u, 0u, 0u);
    const GuiColor white = GUI_RGB(0xffu, 0xffu, 0xffu);
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 64u, .height = 24u), "a raster for the text is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 64, 24}, .color = black);
    const float after = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = &raster, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = 13u, .x_f32 = 2.0f, .y = 18, .text = "Hi", .length = 2ull, .color = white, .clip = (GuiRect){0, 0, 64, 24});
    cli_check(off(after, 2.0f + (2.0f * advance)) < 0.01f, "a text drawn ends two cells on");
    cli_check(painted(&raster, (GuiRect){2, 5, (int)advance, 14}, black), "the first letter paints its cell");
    cli_check(painted(&raster, (GuiRect){2 + (int)advance, 5, (int)advance, 14}, black), "the second letter paints its cell");
    cli_check(!painted(&raster, (GuiRect){2 + (int)(2.0f * advance) + 1, 0, 40, 24}, black),
              "nothing is painted past the text");
    cli_check(!painted(&raster, (GuiRect){0, 19, 64, 5}, black), "an H and an i paint nothing below the baseline");
    cli_check(off(EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = 13u, .text = "Hi", .length = 2ull), 2.0f * advance) < 0.01f,
              "a text's width is its letters' advances");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 64, 24}, .color = black);
    EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = &raster, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = 13u, .x_f32 = 2.0f, .y = 18, .text = "Hi", .length = 2ull, .color = white, .clip = (GuiRect){0, 0, 1, 1});
    cli_check(!painted(&raster, (GuiRect){1, 1, 63, 23}, black), "a text is drawn inside the clip alone");
    EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = &raster, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = 13u, .x_f32 = 2.0f, .y = 18, .text = "\xc3\xa9", .length = 2ull, .color = white, .clip = (GuiRect){0, 0, 64, 24});
    cli_check(painted(&raster, (GuiRect){2, 2, (int)advance, 16}, black), "a letter of two bytes draws one glyph");

    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    cli_check(glyphs.entry == NULL, "the glyphs given back keep no slots");
    return cli_on_check();
}
