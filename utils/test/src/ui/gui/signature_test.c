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
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    if (!gui_glyphs_made(&glyphs, &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_checked();
    }
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    GuiSignature signature;
    gui_signature_made(&signature);
    cli_check(!signature.shown && (signature.active == -1) && (gui_signature_due(&signature, 1000ull) == 0ull),
              "a signature made shows nothing and wants no clock");
    cli_check(!gui_signature_typed(&signature, "x", 1ull, 1000ull) && (gui_signature_due(&signature, 1000ull) == 0ull),
              "a letter typed begins no wait");
    cli_check(gui_signature_typed(&signature, "(", 1ull, 1000ull) &&
                  (gui_signature_due(&signature, 1000ull) == GUI_SIGNATURE_WAIT),
              "a bracket typed begins the wait");
    cli_check(!gui_signature_ticked(&signature, 1000ull + GUI_SIGNATURE_WAIT - 1ull),
              "the clock before the wait is up asks nothing");
    cli_check(gui_signature_ticked(&signature, 1000ull + GUI_SIGNATURE_WAIT) &&
                  (gui_signature_due(&signature, 2000ull) == 0ull),
              "the wait up asks once");
    cli_check(gui_signature_typed(&signature, "a, ", 3ull, 3000ull), "a comma among the text typed begins the wait");

    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    unsigned int root = cli_json_taken(&json, s_server, sizeof(s_server) - 1u, 0u, 0ull, &err_msg, &g_checks_cells);
    cli_check(gui_signature_put(&signature, &json, root), "the server's answer is taken");
    cli_json_given(&json);
    cli_check(signature.shown && (signature.params == 2u) && (signature.active == 1) && (signature.count == 1u),
              "the server's label has two parameters, the second active");
    cli_check((signature.span[1][0] == 13ull) && (signature.span[1][1] == 19ull),
              "a parameter's span is its bytes in the label");
    cli_check((signature.doc != NULL) && (signature.doc_length == 10ull), "the documentation is kept");
    cli_check(signature.length == 20ull, "the label is kept whole");

    // drawn
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 800u, 500u), "a raster is made");
    const GuiRect bounds = {0, 0, 800, 500};
    gui_raster_filled(&raster, bounds, palette->editor_ground);
    gui_signature_drawn(&signature, &raster, &glyphs, palette, (GuiRect){200, 300, 8, 20}, bounds, 1.0f);
    cli_check((signature.rect.y + signature.rect.height == 300 - GUI_PANEL_OFF) &&
                  (signature.rect.height > GUI_PANEL_CODE_LINE),
              "the box stands over the cursor's row");
    cli_check(raster.pixels[((unsigned int)(signature.rect.y + 8) * 800u) + (unsigned int)(signature.rect.x + 8)] ==
                  palette->surface,
              "the box is filled with the widget's ground");
    cli_check(has(&raster, signature.rect, gui_palette_token(palette, "type", 4ull)),
              "the active parameter is drawn in the type's color");
    cli_check(gui_signature_holds(&signature, signature.rect.x + 5, signature.rect.y + 5) &&
                  !gui_signature_holds(&signature, 5, 5),
              "the box holds its pixels alone");
    gui_raster_filled(&raster, bounds, palette->editor_ground);
    gui_signature_drawn(&signature, &raster, &glyphs, palette, (GuiRect){200, 10, 8, 20}, bounds, 1.0f);
    cli_check(signature.rect.y == 10 + 20 + GUI_PANEL_OFF, "with no room above, the box stands under the row");

    // the tree's answer
    root = cli_json_taken(&json, s_own, sizeof(s_own) - 1u, 0u, 0ull, &err_msg, &g_checks_cells);
    cli_check(gui_signature_put(&signature, &json, root), "the tree's answer is taken");
    cli_json_given(&json);
    cli_check(signature.shown && (signature.params == 1u) && (signature.active == 0) && (signature.doc == NULL),
              "the tree's declaration has one parameter, active, and no documentation");
    cli_check((signature.differing == 1u) && (signature.differs[0] == 0u),
              "the parameter the argument differs from is kept");
    gui_raster_filled(&raster, bounds, palette->editor_ground);
    gui_signature_drawn(&signature, &raster, &glyphs, palette, (GuiRect){200, 300, 8, 20}, bounds, 1.0f);
    cli_check(has(&raster, signature.rect, palette->fail), "the differing parameter is underlined in the fail color");

    root = cli_json_taken(&json, s_none, sizeof(s_none) - 1u, 0u, 0ull, &err_msg, &g_checks_cells);
    cli_check(gui_signature_put(&signature, &json, root) && !signature.shown && (signature.label == NULL),
              "both answers null hide the box");
    cli_json_given(&json);
    gui_raster_given(&raster);
    gui_signature_given(&signature);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
