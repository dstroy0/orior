// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// change_peek_test.c: the change peek's check: a view with one line changed shows its change under the line, a removed
// row with its minus in the first mark's color and an added row with its plus in the second's; a press lands on the box
// and on each control; the revert puts the last commit's text back and one undo brings the change again; with two
// changes the next wraps from the second to the first and the previous from the first to the last; Escape closes the
// peek, after which no box is drawn or pressed; and a change just displayed has not settled against a scroll
#include "../../../../../includes/window/block.h"
#include "../../../../../ui/cli/src/editor/document_changes.h"
#include "../../../../../ui/cli/src/editor/view_edits.h"
#include "../../../../../includes/window/frame.h"
#include "../../../../../ui/gui/src/change_peek.h"
#include "../cli/cli_check.h"

static const char s_key[] = "change_peek_check";
static const char s_head[] = "one\ntwo\nthree\nfour\nfive\n";

// how far two colors are apart, the channels' differences summed
static int apart(GuiColor one, GuiColor other)
{
    const int red = (int)GUI_RED_OF(one) - (int)GUI_RED_OF(other);
    const int green = (int)GUI_GREEN_OF(one) - (int)GUI_GREEN_OF(other);
    const int blue = (int)GUI_BLUE_OF(one) - (int)GUI_BLUE_OF(other);
    return ((red < 0) ? -red : red) + ((green < 0) ? -green : green) + ((blue < 0) ? -blue : blue);
}

// whether some pixel of `rect` is near `color`, nearer it than `other`: the text drawn in it, its edges blended
static int colored(const GuiRaster *raster, GuiRect rect, GuiColor color, GuiColor other)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            const GuiColor pixel = raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
            if ((apart(pixel, color) < 90) && (apart(pixel, color) < apart(pixel, other)))
            {
                return 1;
            }
        }
    }
    return 0;
}

// the store told nothing of a change: the head alone is stored by the call
static void head_stored_each(const CliEachHunkCfg *args)
{
    (void)args;
}

// `now` stored with a view open on it, sized, the head text stored beside it, and the editor's frame taken
static void opened(const char *now, GuiEditor *editor)
{
    CliShape shape;
    EMBED_CALL(gui_change_peek.close, GuiChangePeekCfg, .key = s_key);
    cli_view_frame_release((CliViewFrame *)editor->frame);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    cli_check(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = now,
                         .size = cellul.len(&(CatenaFinitaCfg){.src = now, .cap = CLI_POOL_BYTES}),
                         .shape = &shape) &&
                  EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = "check/peek.txt"),
              "the text is stored with a view open on it");
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const CliViewInput sized = {.type = CLI_VIEW_SIZE, .rows = 20u, .columns = 80u};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &sized, .err_msg = &err_msg);
    (void)EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = s_key,
                     .head_given = 1, .head = s_head, .head_size = sizeof(s_head) - 1u, .each = head_stored_each,
                     .context = NULL);
    editor->frame = cli_view_frame_alloc(s_key);
}

// the whole text read as `required`
static int text_is(const char *required)
{
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = s_key,
                                  .owner = g_checks_owner, .size_pointer = &size);
    const int same = (text != NULL) && (size == cellul.len(&(CatenaFinitaCfg){.src = required, .cap = CLI_POOL_BYTES})) &&
                     (memor.cmp(&(MemoriaCfg){.src = text, .other = required, .bytes = size}) == 0);
    g_cli_guard->persistent_buf_release(text, g_checks_owner);
    return same;
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
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 800u, .height = 400u),
              "the raster is taken");
    const GuiRect place = {0, 0, 800, 400};
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = 100);
    const float cell = EMBED_CALL(gui_glyphs_face_character.cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .size = size);
    GuiEditor editor;
    EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &editor);

    // one line changed: the peek under it and its two rows
    opened("one\nTWO\nthree\nfour\nfive\n", &editor);
    cli_check(changes_count() == 1u, "the text has one change");
    cli_check(EMBED_CALL(gui_change_peek.show, GuiChangePeekCfg, .key = s_key, .line = 1ULL),
              "the changed line's change is displayed");
    cli_check(!EMBED_CALL(gui_change_peek.settled, GuiChangePeekCfg, .key = s_key),
              "a change just displayed has not settled");
    EMBED_CALL(gui_change_peek.draw, GuiChangePeekCfg, .raster = &raster, .glyphs = &glyphs, .palette = palette,
               .editor_read = &editor, .place = place, .bounds = place, .cell = cell, .scale = 100);
    const GuiRect box = EMBED_CALL(gui_change_peek.box, GuiChangePeekCfg, .scale = 100);
    cli_check((box.width > 0) && (box.y == 2 * GUI_CODE_LINE), "the box is under the changed line");
    cli_check(box.height == GUI_CHANGE_PEEK_HEAD + (2 * GUI_CHANGE_PEEK_ROW), "the box holds the head and two rows");
    const GuiRect removed_row = {box.x, box.y + GUI_CHANGE_PEEK_HEAD, box.width, GUI_CHANGE_PEEK_ROW};
    const GuiRect added_row = {box.x, removed_row.y + GUI_CHANGE_PEEK_ROW, box.width, GUI_CHANGE_PEEK_ROW};
    cli_check(colored(&raster, removed_row, palette->mark[0], palette->mark[1]) &&
                  !colored(&raster, removed_row, palette->mark[1], palette->mark[0]),
              "the line the last commit had is drawn in the first mark's color");
    cli_check(colored(&raster, added_row, palette->mark[1], palette->mark[0]) &&
                  !colored(&raster, added_row, palette->mark[0], palette->mark[1]),
              "the line now is drawn in the second mark's color");

    unsigned int action = 99u;
    cli_check(EMBED_CALL(gui_change_peek.hit, GuiChangePeekCfg, .x = box.x + 4, .y = added_row.y + 4,
                         .action_out = &action) &&
                  (action == GUI_CHANGE_PEEK_BOX),
              "a press on the box off its controls lands on the box");
    const int control_y = box.y + (GUI_CHANGE_PEEK_HEAD / 2);
    const int close_x = box.x + box.width - GUI_CHANGE_PEEK_PAD - (GUI_CHANGE_PEEK_CONTROL / 2);
    cli_check(EMBED_CALL(gui_change_peek.hit, GuiChangePeekCfg, .x = close_x, .y = control_y, .action_out = &action) &&
                  (action == GUI_CHANGE_PEEK_CLOSE),
              "a press on the head's last control is close");
    cli_check(EMBED_CALL(gui_change_peek.hit, GuiChangePeekCfg, .x = close_x - (3 * GUI_CHANGE_PEEK_CONTROL),
                         .y = control_y, .action_out = &action) &&
                  (action == GUI_CHANGE_PEEK_REVERT),
              "a press three controls before close is revert");
    cli_check(!EMBED_CALL(gui_change_peek.hit, GuiChangePeekCfg, .x = box.x + 4, .y = box.y + box.height + 4,
                          .action_out = &action),
              "a press under the box lands on none of it");

    // the revert puts the last commit's text back as one step
    CliEdit edit;
    char edit_why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea edit_err = {edit_why, sizeof(edit_why), 0ULL};
    const int reverted = EMBED_CALL(gui_change_peek.revert, GuiChangePeekCfg, .edit_out = &edit, .err_msg = &edit_err);
    cli_check(reverted && EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = s_key, .edit = &edit, .count = 1u,
                                     .err_msg = &edit_err),
              "the revert's edit is written into the view");
    if (reverted)
    {
        EMBED_CALL(gui_change_peek.revert_end, GuiChangePeekCfg, .edit_out = &edit);
    }
    cli_check(text_is(s_head), "the view's text is the last commit's again");
    const CliViewInput undo = {.type = CLI_VIEW_KEY, .name = "Mod+Z"};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &undo, .err_msg = &edit_err);
    cli_check(text_is("one\nTWO\nthree\nfour\nfive\n"), "one undo brings the change back");

    // two changes: the next wraps from the second to the first, the previous from the first to the last
    opened("ONE\ntwo\nthree\nfour\nFIVE\n", &editor);
    unsigned long long line = 99ULL;
    cli_check(changes_count() == 2u, "the text has two changes");
    cli_check(EMBED_CALL(gui_change_peek.show, GuiChangePeekCfg, .key = s_key, .line = 4ULL),
              "the second change is displayed");
    cli_check(EMBED_CALL(gui_change_peek.step, GuiChangePeekCfg, .key = s_key, .by = 1, .line_out = &line) &&
                  (line == 0ULL),
              "the next from the last wraps to the first");
    cli_check(EMBED_CALL(gui_change_peek.step, GuiChangePeekCfg, .key = s_key, .by = -1, .line_out = &line) &&
                  (line == 4ULL),
              "the previous from the first wraps to the last");

    // Escape closes the peek: no box drawn and none pressed
    cli_check(EMBED_CALL(gui_change_peek.key, GuiChangePeekCfg, .name = "Escape"),
              "Escape is taken while the peek is");
    cli_check(!EMBED_CALL(gui_change_peek.key, GuiChangePeekCfg, .name = "Escape"),
              "Escape is not taken once it is closed");
    cli_check(!EMBED_CALL(gui_change_peek.hit, GuiChangePeekCfg, .x = box.x + 4, .y = added_row.y + 4,
                          .action_out = &action),
              "a closed peek takes no press where its box was");
    EMBED_CALL(gui_change_peek.draw, GuiChangePeekCfg, .raster = &raster, .glyphs = &glyphs, .palette = palette,
               .editor_read = &editor, .place = place, .bounds = place, .cell = cell, .scale = 100);
    const GuiRect closed = EMBED_CALL(gui_change_peek.box, GuiChangePeekCfg, .scale = 100);
    cli_check(closed.width == 0, "a closed peek draws no box");
    cli_check(EMBED_CALL(gui_change_peek.settled, GuiChangePeekCfg, .key = s_key),
              "with no change displayed a scroll moves the view");

    cli_view_frame_release((CliViewFrame *)editor.frame);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
