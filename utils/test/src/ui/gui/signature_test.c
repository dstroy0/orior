// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// signature_test.c: the parameter info part's check: a bracket or a comma typed begins the wait and other text does
// not, the wait up asks once, the server's answer is shown with its parameters, active one and documentation, the
// tree's answer is shown where the server gives none with the parameters the arguments differ from, both null hides
// the box, and the box is drawn over the cursor's row with the active parameter in the type's color
#include "../../../../../ui/gui/src/parts/signature.h"
#include "../cli/cli_check.h"

static const char s_server[] = "{\"own\":null,\"server\":{\"label\":\"int f(int a, char "
                               "b)\",\"params\":[[6,11],[13,19]],\"active\":1,\"doc\":\"Adds them.\",\"count\":1}}";
static const char s_own[] = "{\"own\":{\"name\":\"f\",\"active\":0,\"arguments\":[{\"text\":\"x\",\"type\":\"char\"}],"
                            "\"declarations\":[{\"path\":\"a.c\",\"line\":2,"
                            "\"label\":\"void f(int "
                            "a)\",\"params\":[[7,12]],\"types\":[\"int\"],\"count\":true,\"variadic\":false,"
                            "\"differs\":[0]}],\"reading\":false},\"server\":null}";
static const char s_none[] = "{\"own\":null,\"server\":null}";

// whether any pixel of `rect` is `color`
static int has(const GuiRaster *raster, GuiRect rect, GuiColor color)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] == color)
            {
                return 1;
            }
        }
    }
    return 0;
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiSignature signature;
    EMBED_CALL(gui_signature_json.init, GuiSignatureJsonCfg, .signature = &signature);
    g_test_clock_ms = 1000ull;
    cli_check(!signature.shown && (signature.active == -1) && (EMBED_CALL(gui_signature.next_deadline_ms, GuiSignatureCfg, .signature_read = &signature) == 0ull),
              "a signature made shows nothing and wants no clock");
    g_test_clock_ms = 1000ull;
    cli_check(!EMBED_CALL(gui_signature_text.type, GuiSignatureTextCfg, .signature = &signature, .text = "x", .size = 1ull) && (EMBED_CALL(gui_signature.next_deadline_ms, GuiSignatureCfg, .signature_read = &signature) == 0ull),
              "a letter typed begins no wait");
    g_test_clock_ms = 1000ull;
    cli_check(EMBED_CALL(gui_signature_text.type, GuiSignatureTextCfg, .signature = &signature, .text = "(", .size = 1ull) &&
                  (EMBED_CALL(gui_signature.next_deadline_ms, GuiSignatureCfg, .signature_read = &signature) == 1000ull + GUI_SIGNATURE_DEBOUNCE_MS),
              "a bracket typed begins the wait");
    g_test_clock_ms = 1000ull + GUI_SIGNATURE_DEBOUNCE_MS - 1ull;
    cli_check(!EMBED_CALL(gui_signature_text.tick, GuiSignatureTextCfg, .signature = &signature),
              "the clock before the wait is up asks nothing");
    g_test_clock_ms = 1000ull + GUI_SIGNATURE_DEBOUNCE_MS;
    cli_check(EMBED_CALL(gui_signature_text.tick, GuiSignatureTextCfg, .signature = &signature) &&
                  (EMBED_CALL(gui_signature.next_deadline_ms, GuiSignatureCfg, .signature_read = &signature) == 0ull),
              "the wait up asks once");
    g_test_clock_ms = 3000ull;
    cli_check(EMBED_CALL(gui_signature_text.type, GuiSignatureTextCfg, .signature = &signature, .text = "a, ", .size = 3ull), "a comma among the text typed begins the wait");

    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_server, .size = sizeof(s_server) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_signature_json.put, GuiSignatureJsonCfg, .signature = &signature, .json = &json, .root = root), "the server's answer is taken");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check(signature.shown && (signature.params == 2u) && (signature.active == 1) && (signature.count == 1u),
              "the server's label has two parameters, the second active");
    cli_check((signature.span[1][0] == 13ull) && (signature.span[1][1] == 19ull),
              "a parameter's span is its bytes in the label");
    cli_check((signature.doc != NULL) && (signature.doc_length == 10ull), "the documentation is kept");
    cli_check(signature.length == 20ull, "the label is kept whole");

    // drawn
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 800u, .height = 500u), "a raster is made");
    const GuiRect bounds = {0, 0, 800, 500};
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = bounds, .color = palette->editor_ground);
    EMBED_CALL(gui_signature_raster.draw, GuiSignatureRasterCfg, .signature = &signature, .raster = &raster, .glyphs = &glyphs, .palette = palette, .anchor = (GuiRect){200, 300, 8, 20}, .bounds = bounds, .scale = 100);
    cli_check((signature.rect.y + signature.rect.height == 300 - GUI_PANEL_OFF) &&
                  (signature.rect.height > GUI_PANEL_CODE_LINE),
              "the box stands over the cursor's row");
    cli_check(raster.pixels[((unsigned int)(signature.rect.y + 8) * 800u) + (unsigned int)(signature.rect.x + 8)] ==
                  palette->surface,
              "the box is filled with the widget's ground");
    cli_check(has(&raster, signature.rect, EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = palette, .name_read = "type", .length = 4ull)),
              "the active parameter is drawn in the type's color");
    cli_check(EMBED_CALL(gui_signature_x.holds, GuiSignatureXCfg, .signature_read = &signature, .x = signature.rect.x + 5, .y = signature.rect.y + 5) &&
                  !EMBED_CALL(gui_signature_x.holds, GuiSignatureXCfg, .signature_read = &signature, .x = 5, .y = 5),
              "the box holds its pixels alone");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = bounds, .color = palette->editor_ground);
    EMBED_CALL(gui_signature_raster.draw, GuiSignatureRasterCfg, .signature = &signature, .raster = &raster, .glyphs = &glyphs, .palette = palette, .anchor = (GuiRect){200, 10, 8, 20}, .bounds = bounds, .scale = 100);
    cli_check(signature.rect.y == 10 + 20 + GUI_PANEL_OFF, "with no room above, the box stands under the row");

    // the tree's answer
    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_own, .size = sizeof(s_own) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_signature_json.put, GuiSignatureJsonCfg, .signature = &signature, .json = &json, .root = root), "the tree's answer is taken");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    cli_check(signature.shown && (signature.params == 1u) && (signature.active == 0) && (signature.doc == NULL),
              "the tree's declaration has one parameter, active, and no documentation");
    cli_check((signature.differing == 1u) && (signature.differs[0] == 0u),
              "the parameter the argument differs from is kept");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = bounds, .color = palette->editor_ground);
    EMBED_CALL(gui_signature_raster.draw, GuiSignatureRasterCfg, .signature = &signature, .raster = &raster, .glyphs = &glyphs, .palette = palette, .anchor = (GuiRect){200, 300, 8, 20}, .bounds = bounds, .scale = 100);
    cli_check(has(&raster, signature.rect, palette->fail), "the differing parameter is underlined in the fail color");

    root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_none, .size = sizeof(s_none) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(EMBED_CALL(gui_signature_json.put, GuiSignatureJsonCfg, .signature = &signature, .json = &json, .root = root) && !signature.shown && (signature.label == NULL),
              "both answers null hide the box");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(gui_signature_json.free, GuiSignatureJsonCfg, .signature = &signature);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
