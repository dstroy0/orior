// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gutter_test.c: the gutter's change marks: a view whose text differs from its last commit's file in a line changed, a
// line put in and a line taken out draws a mark on each of those rows as the editor draws its rows, the three in
// colors of their own, and none on a row the change leaves; a press on the marks' column lands on it and one on the
// numbers does not; and a text stored against the same file leaves no mark
#include "../../../../../includes/window/block.h"
#include "../../../../../ui/cli/src/editor/document_changes.h"
#include "../../../../../includes/window/frame.h"
#include "../../../../../ui/gui/src/editor.h"
#include "../../../../../ui/gui/src/gutter.h"
#include "../cli/cli_check.h"

static const char s_key[] = "gutter_check";
static const char s_head[] = "one\ntwo\nthree\nfour\nfive\nsix\n";

// the pixel at x, y of `raster`
static GuiColor pixel_at(const GuiRaster *raster, int x, int y)
{
    return raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
}

// the store told nothing of a change: the head alone is stored by the call
static void head_stored_each(const CliEachHunkCfg *args)
{
    (void)args;
}

// the last commit's file of the text stored under the key set to `head`
static void head_stored(const char *head, unsigned long long size)
{
    (void)EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = s_key,
                     .head_given = 1, .head = head, .head_size = size, .each = head_stored_each, .context = NULL);
}

// how many changes the text stored under the key has against its last commit's file
static unsigned int changes_count(void)
{
    unsigned int count = 0u;
    (void)EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = s_key, .at = 0u, .count_out = &count);
    return count;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(gui_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .faces = &g_gui_faces, .err_msg = &why))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);

    // the second line changed, a line put in after the fourth, and the sixth taken out
    static const char s_now[] = "one\nTWO\nthree\nfour\nnew\nfive\n";
    CliShape shape;
    cli_check(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = s_now,
                         .size = sizeof(s_now) - 1u, .shape = &shape) &&
                  EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = "check/gutter.txt"),
              "the text is stored with a view open on it");
    char view_why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea view_err = {view_why, sizeof(view_why), 0ULL};
    const CliViewInput sized = {.type = CLI_VIEW_SIZE, .rows = 20u, .columns = 80u};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &sized, .err_msg = &view_err);
    head_stored(s_head, sizeof(s_head) - 1u);
    cli_check(changes_count() == 3u, "the text has three changes");

    GuiEditor editor;
    EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &editor);
    editor.frame = cli_view_frame_alloc(s_key);
    cli_check(editor.frame != NULL, "the view's frame is taken");
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 800u, .height = 400u),
              "the raster is taken");
    const GuiRect place = {0, 0, 800, 400};
    EMBED_CALL(gui_editor_position_scale.draw, GuiEditorPositionScaleCfg, .editor = &editor, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .place = place, .scale = 100, .key = s_key);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = 100);
    const float cell = EMBED_CALL(gui_glyphs_face_character.cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .size = size);
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

    // the text stored against itself: no change and no mark
    head_stored(s_now, sizeof(s_now) - 1u);
    cli_check(changes_count() == 0u, "a text the same as its last commit's has no change");
    // the editor's ground is see-through: a frame starts clear, as the window's frame starts from the ground
    memor.set(&(MemoriaCfg){.dst = raster.pixels, .val = 0,
                            .bytes = (unsigned long long)raster.width * raster.height * sizeof(*raster.pixels)});
    EMBED_CALL(gui_editor_position_scale.draw, GuiEditorPositionScaleCfg, .editor = &editor, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .place = place, .scale = 100, .key = s_key);
    cli_check(pixel_at(&raster, column, (1 * GUI_CODE_LINE) + (GUI_CODE_LINE / 2)) == plain,
              "a text the same as its last commit's carries no mark");

    cli_view_frame_release((CliViewFrame *)editor.frame);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
