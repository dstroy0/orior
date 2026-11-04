// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// terminal_test.c: the terminal pane's check: a line the xterm grammar reads, each escape sequence a keyword of a
// dotted token, drawn through the pane as a tab's line is: the sequences take no column, the letter after
// keyword.foreground.1 stands in the terminal's red, the letter after keyword.reset in the text's color, and the letter
// after keyword.inverse on the text's color as its ground
#include "../../../../../ui/cli/src/plugins/plugins.h"
#include "../../../../../ui/cli/src/view/parts/terminal.h"
#include "../cli/cli_check.h"

#include <string.h>

static const char s_key[] = "check:terminal";

// whether any pixel of `rect` is `color`
static int colored(const GuiRaster *raster, GuiRect rect, GuiColor color)
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
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = 240);
    cli_check(frame.terminal_views.height > 0, "the frame lays the terminal's panel with a pane");

    // R after red, P after a reset, I after inverse
    static const char s_line[] = "\x1b[31mR\x1b[0mP\x1b[7mI";
    char grammar[CLI_NAME_MAX_LEN];
    VerbaLinea refused = {error_text, sizeof(error_text), 0ull};
    cli_check(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "xterm", .key = grammar,
                         .err_msg = &refused),
              "the xterm grammar is read");
    CliShape shape;
    cli_check(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = s_line,
                         .size = sizeof(s_line) - 1u, .shape = &shape),
              "the line is stored");
    cli_check(EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = s_key,
                         .grammar = grammar, .named = 0),
              "and colored by the xterm grammar");
    cli_document_walk();
    cli_check(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = ""), "a view opens on it");
    const CliViewInput sized = {.kind = CLI_VIEW_SIZE, .rows = 3u, .columns = 40u};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &sized, .err_msg = &why);

    GuiTerminal terminal;
    memset(&terminal, 0, sizeof(terminal));
    terminal.panel = (CliTerminalPanel){1, 0, 240u, 1u, 0, -1, 1u};
    terminal.view[0] = (CliTerminalView){s_key, 0ull, 40u, 3u, 0u, 0u, 0, 0};
    terminal.frame[0] = cli_view_frame_alloc(s_key);
    terminal.names[0] =
        EMBED_CALL(cli_document_names.names_alloc, CliDocumentNamesCfg, .key = s_key, .owner = &g_checks_cells);
    cli_check((terminal.frame[0] != NULL) && (terminal.names[0] != NULL), "the pane's frame and names are taken");

    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    EMBED_CALL(gui_terminal.draw, GuiTerminalCfg, .terminal_read = &terminal, .frame = &frame, .glyphs = &glyphs,
               .raster = &raster, .palette = palette);
    const int cell = (int)(EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs,
                                      .size = GUI_TERMINAL_TEXT_SIZE) +
                           0.5f);
    const int left = frame.terminal_views.x + GUI_TERMINAL_PAD_SIDE;
    const int top = frame.terminal_views.y + GUI_TERMINAL_PAD_TOP;
    const GuiRect first = {left, top, cell, GUI_TERMINAL_ROW};
    const GuiRect second = {left + cell, top, cell, GUI_TERMINAL_ROW};
    const GuiRect third = {left + (2 * cell), top, cell, GUI_TERMINAL_ROW};
    cli_check(colored(&raster, first, palette->terminal[1]),
              "the sequences take no column, and the first letter stands in the terminal's red");
    cli_check(colored(&raster, second, palette->text) && !colored(&raster, second, palette->terminal[1]),
              "the letter after the reset stands in the text's color");
    cli_check(raster.pixels[((unsigned int)third.y * raster.width) + (unsigned int)third.x] == palette->text,
              "the letter after inverse stands on the text's color as its ground");

    cli_view_frame_release((CliViewFrame *)terminal.frame[0]);
    g_cli_cells->persistent_buf_release((void *)terminal.names[0], &g_checks_cells);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    return cli_on_check();
}
