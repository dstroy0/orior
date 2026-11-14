// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dialog_test.c: the dialog's check. It checks the ask and its refusals, the size the texts want, the layout of the
// head, the body and the foot, each button's press and release, Enter, Escape and the keys that move the mark, and the
// drawn grounds against the editor's and the tabs row's
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/dialog.h"
#include "../cli/cli_check.h"

// the brightness of a pixel, the sum of its three channels
static unsigned int brightness(GuiColor color)
{
    return GUI_RED_OF(color) + GUI_GREEN_OF(color) + GUI_BLUE_OF(color);
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    static GuiGlyphs glyphs;
    cli_check(EMBED_CALL(gui_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .faces = &g_gui_faces, .err_msg = &why), "the glyphs are read");
    static GuiPaletteOwn own;
    EMBED_CALL(gui_palette_scheme.own_init, GuiPaletteCfg, .own = &own, .scheme = GUI_SCHEME_DARK);
    const GuiPalette *const palette = &own.colors;

    // the ask and its refusals
    static GuiDialog dialog;
    EMBED_CALL(gui_dialog.init, GuiDialogCfg, .dialog = &dialog);
    cli_check(!dialog.displayed, "a dialog starts hidden");
    const char *const labels[3] = {"Save", "Don't Save", "Cancel"};
    cli_check(!EMBED_CALL(gui_dialog_asking.ask, GuiDialogAskCfg, .dialog = &dialog, .title = "Close", .body = "x", .labels = labels, .count = 0u), "an ask with no button is refused");
    cli_check(!EMBED_CALL(gui_dialog_asking.ask, GuiDialogAskCfg, .dialog = &dialog, .title = "Close", .body = "x", .labels = labels, .count = GUI_DIALOG_BUTTONS_MAX + 1u), "and one with too many");
    cli_check(!dialog.displayed, "and a refusal shows nothing");
    cli_check(EMBED_CALL(gui_dialog_asking.ask, GuiDialogAskCfg, .dialog = &dialog, .title = "Close engine.md", .body = "engine.md has changes that are not saved.\nSave them before it closes?", .labels = labels, .count = 3u, .preferred = 0u), "an ask with three buttons shows");
    cli_check(dialog.displayed && (dialog.buttons == 3u) && (dialog.marked == 0u), "the preferred button holds the keys");

    // the size the texts want, and the layout at it
    unsigned int width = 0u;
    unsigned int height = 0u;
    EMBED_CALL(gui_dialog_wanting.want, GuiDialogWantCfg, .dialog_read = &dialog, .glyphs = &glyphs, .scale = GUI_SCALE_ONE, .width_pointer = &width, .height_pointer = &height);
    cli_check(width >= (unsigned int)GUI_DIALOG_MIN_WIDTH, "the dialog is no narrower than its least");
    EMBED_CALL(gui_dialog_width.resize, GuiDialogWidthCfg, .dialog = &dialog, .glyphs = &glyphs, .width = width, .height = height, .scale = GUI_SCALE_ONE);
    const GuiRect head = EMBED_CALL(gui_dialog_read.head_rect, GuiDialogReadCfg, .dialog_read = &dialog);
    const GuiRect close = EMBED_CALL(gui_dialog_read.close_rect, GuiDialogReadCfg, .dialog_read = &dialog);
    const GuiRect body = EMBED_CALL(gui_dialog_read.body_rect, GuiDialogReadCfg, .dialog_read = &dialog);
    const GuiRect first = EMBED_CALL(gui_dialog_read.button_rect, GuiDialogReadCfg, .dialog_read = &dialog, .at = 0u);
    const GuiRect second = EMBED_CALL(gui_dialog_read.button_rect, GuiDialogReadCfg, .dialog_read = &dialog, .at = 1u);
    const GuiRect last = EMBED_CALL(gui_dialog_read.button_rect, GuiDialogReadCfg, .dialog_read = &dialog, .at = 2u);
    cli_check((close.x + close.width <= (int)width) && (close.y + close.height <= head.height), "the close mark is in the head band");
    cli_check((body.y >= head.height) && (body.height == 2 * GUI_DIALOG_LINE), "the body's two lines are under the head band");
    cli_check((first.y >= body.y + body.height) && (last.y + last.height <= (int)height), "the buttons are under the body");
    cli_check((first.x + first.width < second.x) && (second.x + second.width < last.x), "the buttons are in their order");
    cli_check(last.x + last.width == (int)width - (GUI_DIALOG_MAT + GUI_DIALOG_PAD), "and end at the foot's right edge");
    cli_check(first.width >= GUI_DIALOG_BUTTON_MIN, "a button is no narrower than its least");
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = EMBED_CALL(gui_dialog_read.button_rect, GuiDialogReadCfg, .dialog_read = &dialog, .at = 3u)), "a button past the count has no place");

    // the keys move the mark round the buttons
    GuiDialogChange change;
    EMBED_CALL(gui_dialog_keys.key, GuiDialogKeyCfg, .dialog = &dialog, .name = "Tab", .change = &change);
    cli_check((change.type == GUI_DIALOG_DRAWN) && (dialog.marked == 1u), "Tab moves the keys to the next button");
    EMBED_CALL(gui_dialog_keys.key, GuiDialogKeyCfg, .dialog = &dialog, .name = "Right", .change = &change);
    EMBED_CALL(gui_dialog_keys.key, GuiDialogKeyCfg, .dialog = &dialog, .name = "Right", .change = &change);
    cli_check(dialog.marked == 0u, "and past the last runs round to the first");
    EMBED_CALL(gui_dialog_keys.key, GuiDialogKeyCfg, .dialog = &dialog, .name = "Shift+Tab", .change = &change);
    cli_check(dialog.marked == 2u, "Shift+Tab moves it back round to the last");
    EMBED_CALL(gui_dialog_keys.key, GuiDialogKeyCfg, .dialog = &dialog, .name = "Left", .change = &change);
    cli_check(dialog.marked == 1u, "and Left to the one before");
    EMBED_CALL(gui_dialog_keys.key, GuiDialogKeyCfg, .dialog = &dialog, .name = "a", .change = &change);
    cli_check(change.type == GUI_DIALOG_NOTHING, "another key does nothing");
    EMBED_CALL(gui_dialog_keys.key, GuiDialogKeyCfg, .dialog = &dialog, .name = "Enter", .change = &change);
    cli_check((change.type == GUI_DIALOG_ANSWERED) && (change.button == 1u) && !dialog.displayed, "Enter answers the button with the keys and ends the dialog");

    // Escape closes with no answer
    (void)EMBED_CALL(gui_dialog_asking.ask, GuiDialogAskCfg, .dialog = &dialog, .title = "Close engine.md", .body = "engine.md has changes that are not saved.\nSave them before it closes?", .labels = labels, .count = 3u, .preferred = 0u);
    EMBED_CALL(gui_dialog_keys.key, GuiDialogKeyCfg, .dialog = &dialog, .name = "Escape", .change = &change);
    cli_check((change.type == GUI_DIALOG_CLOSED) && !dialog.displayed, "Escape closes with no answer");

    // the pointer: a press and a release on one button answers it, a release elsewhere answers nothing
    (void)EMBED_CALL(gui_dialog_asking.ask, GuiDialogAskCfg, .dialog = &dialog, .title = "Close engine.md", .body = "engine.md has changes that are not saved.\nSave them before it closes?", .labels = labels, .count = 3u, .preferred = 0u);
    EMBED_CALL(gui_dialog_point.move, GuiDialogPointCfg, .dialog = &dialog, .x = last.x + 4, .y = last.y + 4, .change = &change);
    cli_check((change.type == GUI_DIALOG_DRAWN) && (dialog.pointed == 2u), "the pointer over a button marks it");
    EMBED_CALL(gui_dialog_point.move, GuiDialogPointCfg, .dialog = &dialog, .x = last.x + 5, .y = last.y + 4, .change = &change);
    cli_check(change.type == GUI_DIALOG_NOTHING, "a move on the same button draws nothing new");
    EMBED_CALL(gui_dialog_point.press, GuiDialogPointCfg, .dialog = &dialog, .x = second.x + 4, .y = second.y + 4, .change = &change);
    cli_check((dialog.pressed == 1u) && (dialog.marked == 1u), "a press on a button gives it the keys");
    EMBED_CALL(gui_dialog_point.release, GuiDialogPointCfg, .dialog = &dialog, .x = body.x + 2, .y = body.y + 2, .change = &change);
    cli_check((change.type == GUI_DIALOG_DRAWN) && dialog.displayed, "a release off the button answers nothing");
    EMBED_CALL(gui_dialog_point.press, GuiDialogPointCfg, .dialog = &dialog, .x = last.x + 4, .y = last.y + 4, .change = &change);
    EMBED_CALL(gui_dialog_point.release, GuiDialogPointCfg, .dialog = &dialog, .x = last.x + 6, .y = last.y + 6, .change = &change);
    cli_check((change.type == GUI_DIALOG_ANSWERED) && (change.button == 2u) && !dialog.displayed, "a press and a release on a button answers it");
    (void)EMBED_CALL(gui_dialog_asking.ask, GuiDialogAskCfg, .dialog = &dialog, .title = "Close engine.md", .body = "engine.md has changes that are not saved.\nSave them before it closes?", .labels = labels, .count = 3u, .preferred = 0u);
    EMBED_CALL(gui_dialog_point.press, GuiDialogPointCfg, .dialog = &dialog, .x = close.x + (close.width / 2), .y = close.y + (close.height / 2), .change = &change);
    EMBED_CALL(gui_dialog_point.release, GuiDialogPointCfg, .dialog = &dialog, .x = close.x + (close.width / 2), .y = close.y + (close.height / 2), .change = &change);
    cli_check((change.type == GUI_DIALOG_CLOSED) && !dialog.displayed, "a press and a release on the close mark closes it");

    // the grounds as drawn: the body on the editor's ground, the head band on the tabs row's surface, the margin on
    // the glass, and the preferred button on the displayed tab's ground
    (void)EMBED_CALL(gui_dialog_asking.ask, GuiDialogAskCfg, .dialog = &dialog, .title = "Close engine.md", .body = "engine.md has changes that are not saved.\nSave them before it closes?", .labels = labels, .count = 3u, .preferred = 0u);
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = width, .height = height), "a raster to draw on");
    EMBED_CALL(gui_dialog_drawing.draw, GuiDialogDrawCfg, .dialog_read = &dialog, .raster = &raster, .glyphs = &glyphs, .palette = palette, .ground = NULL);
    const GuiColor editor = EMBED_CALL(gui_color.over, GuiRasterCfg, .background_color = palette->deep, .color = palette->editor_background);
    const GuiColor surface = EMBED_CALL(gui_color.over, GuiRasterCfg, .background_color = palette->deep, .color = palette->surface);
    const GuiColor body_pixel = raster.pixels[((unsigned int)(body.y + body.height + 2) * raster.width) + (unsigned int)body.x + 2u];
    const GuiColor head_pixel = raster.pixels[((unsigned int)(head.height - 6) * raster.width) + (unsigned int)body.x + 2u];
    const GuiColor margin_pixel = raster.pixels[((unsigned int)(height / 2u) * raster.width) + 3u];
    cli_check(body_pixel == editor, "the body is on the editor's ground");
    cli_check(head_pixel == surface, "the head band is on the tabs row's surface");
    cli_check((margin_pixel != body_pixel) && (brightness(margin_pixel) > brightness(palette->deep)), "the margin is on the glass, brighter than the bare ground");
    const GuiColor preferred_pixel = raster.pixels[((unsigned int)(first.y + 3) * raster.width) + (unsigned int)first.x + 4u];
    const GuiColor other_pixel = raster.pixels[((unsigned int)(second.y + 3) * raster.width) + (unsigned int)second.x + 4u];
    cli_check(brightness(preferred_pixel) > brightness(other_pixel), "the preferred button is out from the others");

    // the focus border: the signal for a question, the error color for an error, the widget's line without the focus
    const unsigned int border_at = width / 2u;
    dialog.focused = 1;
    EMBED_CALL(gui_dialog_drawing.draw, GuiDialogDrawCfg, .dialog_read = &dialog, .raster = &raster, .glyphs = &glyphs, .palette = palette, .ground = NULL);
    cli_check(raster.pixels[border_at] == (palette->signal | 0xff000000u), "a question with the focus has the signal on its border");
    (void)EMBED_CALL(gui_dialog_asking.ask, GuiDialogAskCfg, .dialog = &dialog, .title = "Save failed", .body = "engine.md could not be written.", .labels = labels + 2, .count = 1u, .error = 1);
    dialog.focused = 1;
    EMBED_CALL(gui_dialog_drawing.draw, GuiDialogDrawCfg, .dialog_read = &dialog, .raster = &raster, .glyphs = &glyphs, .palette = palette, .ground = NULL);
    cli_check(raster.pixels[border_at] == (palette->fail | 0xff000000u), "an error with the focus has the error color on its border");
    dialog.focused = 0;
    EMBED_CALL(gui_dialog_drawing.draw, GuiDialogDrawCfg, .dialog_read = &dialog, .raster = &raster, .glyphs = &glyphs, .palette = palette, .ground = NULL);
    cli_check(raster.pixels[border_at] != (palette->fail | 0xff000000u), "and without the focus it does not");

    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
