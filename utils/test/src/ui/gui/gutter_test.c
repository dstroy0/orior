// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gutter_test.c: the gutter's change marks: a view whose text differs from its last commit's file in a line changed, a
// line put in and a line taken out draws a mark on each of those rows as the editor draws its rows, the three in
// colors of their own, and none on a row the change leaves; a press on the marks' column lands on it and one on the
// numbers does not; and a text read again against the same file leaves no mark
#include "../../../../../ui/cli/src/editor/hunks.h"
#include "../../../../../ui/cli/src/view/frame.h"
#include "../../../../../ui/cli/src/view/parts/editor.h"
#include "../../../../../ui/cli/src/view/parts/gutter.h"
#include "../cli/cli_check.h"

#include <string.h>

static const char s_key[] = "gutter_check";
static const char s_head[] = "one\ntwo\nthree\nfour\nfive\nsix\n";

// the pixel at x, y of `raster`
static GuiColor pixel_at(const GuiRaster *raster, int x, int y)
{
    return raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);

    // the second line changed, a line put in after the fourth, and the sixth taken out
    static const char s_now[] = "one\nTWO\nthree\nfour\nnew\nfive\n";
    CliShape shape;
    cli_check(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = s_now,
                         .size = sizeof(s_now) - 1u, .shape = &shape) &&
                  EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = "check/gutter.txt"),
              "the text is stored with a view open on it");
    char view_why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea view_err = {view_why, sizeof(view_why), 0ULL};
    const CliViewInput sized = {.kind = CLI_VIEW_SIZE, .rows = 20u, .columns = 80u};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &sized, .err_msg = &view_err);
    EMBED_CALL(cli_hunks.read, CliHunksCfg, .key = s_key, .text = s_head, .size = sizeof(s_head) - 1u);
    cli_check(EMBED_CALL(cli_hunks.count, CliHunksCfg, .at = 0u) == 3u, "the text has three changes");

    GuiEditor editor;
    EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &editor);
    editor.frame = cli_view_frame_alloc(s_key);
    cli_check(editor.frame != NULL, "the view's frame is taken");
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 800u, .height = 400u),
              "the raster is taken");
    const GuiRect place = {0, 0, 800, 400};
    EMBED_CALL(gui_editor_position_scale.draw, GuiEditorPositionScaleCfg, .editor = &editor, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .place = place, .scale = 100);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = 100);
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .size = size);
    const int text_x = place.x + EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg,
                                            .editor_read = &editor, .cell = cell, .scale = 100);
    const int column = text_x - GUI_GUTTER_MARK_RIGHT - 1;
    const GuiColor plain = pixel_at(&raster, column, (0 * GUI_CODE_LINE) + (GUI_CODE_LINE / 2));
    const GuiColor changed = pixel_at(&raster, column, (1 * GUI_CODE_LINE) + (GUI_CODE_LINE / 2));
    const GuiColor added = pixel_at(&raster, column, (4 * GUI_CODE_LINE) + (GUI_CODE_LINE / 2));
    const GuiColor removed = pixel_at(&raster, column, (6 * GUI_CODE_LINE) - 1);
    cli_check(changed != plain, "the line changed is marked");
    cli_check(added != plain, "the line put in is marked");
    cli_check(removed != plain, "the lines taken out are marked at the foot of the row above them");
    cli_check((changed != added) && (added != removed) && (changed != removed), "the three marks are each of a color");
    cli_check(pixel_at(&raster, column, (2 * GUI_CODE_LINE) + (GUI_CODE_LINE / 2)) == plain,
              "a line the changes leave carries no mark");

    cli_check(EMBED_CALL(gui_gutter.mark_hit, GuiGutterCfg, .x = column, .text_x = text_x, .scale = 100),
              "a press on the marks' column lands on it");
    cli_check(!EMBED_CALL(gui_gutter.mark_hit, GuiGutterCfg, .x = place.x + 2, .text_x = text_x, .scale = 100),
              "a press on the numbers does not");

    // the text read again against itself: no change and no mark
    EMBED_CALL(cli_hunks.read, CliHunksCfg, .key = s_key, .text = s_now, .size = sizeof(s_now) - 1u);
    EMBED_CALL(gui_editor_position_scale.draw, GuiEditorPositionScaleCfg, .editor = &editor, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .place = place, .scale = 100);
    cli_check(pixel_at(&raster, column, (1 * GUI_CODE_LINE) + (GUI_CODE_LINE / 2)) == plain,
              "a text the same as its last commit's carries no mark");

    cli_view_frame_release((CliViewFrame *)editor.frame);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    EMBED_CALL(cli_hunks.read, CliHunksCfg, .key = NULL);
    return cli_on_check();
}
