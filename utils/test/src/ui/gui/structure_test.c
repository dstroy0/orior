// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure_test.c: the Outline pane's check: a C text stored under a key reads as rows of declarations in the order of
// their lines, each with its table, its name, its line and its depth; the pane's table draws them on the side under the
// Structure group's title, a row its number of rows under the pane's head, the row of the declaration the cursor is
// in on the idle selection's ground and the row pointed on a ground of its own; a press lands on a row and on no row
// over the group's title; the head offers no action; and a key with no text under it leaves no rows
#include "../../../../../includes/window/block.h"
#include "../../../../../ui/cli/src/code/symbols.h"
#include "../../../../../ui/cli/src/editor/document.h"
#include "../../../../../ui/gui/src/structure.h"
#include "../cli/cli_check.h"

static const char s_key[] = "structure_check";

// the row whose name is `name`, into `row`: 1, or 0 where none is
static int row_with_name(const char *name, CliOutlineRow *row)
{
    for (unsigned int at = 0u; EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at, .row = row); at += 1u)
    {
        if ((row->length == cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES})) &&
            (memor.cmp(&(MemoriaCfg){.src = name, .other = row->name, .bytes = row->length}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(gui_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .faces = &g_gui_faces, .err_msg = &why))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);

    static const char s_text[] = "int top;\n"
                                 "static void helper(void)\n"
                                 "{\n"
                                 "}\n"
                                 "\n"
                                 "int main(void)\n"
                                 "{\n"
                                 "    return 0;\n"
                                 "}\n";
    CliShape shape;
    cli_check(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = s_text,
                         .size = sizeof(s_text) - 1u, .shape = &shape),
              "the text is stored");
    EMBED_CALL(cli_outline.read, CliOutlineCfg, .key = s_key, .path = "check.c");
    const unsigned int rows = EMBED_CALL(cli_outline.rows, CliOutlineCfg, .at = 0u);
    cli_check(rows >= 2u, "the text's declarations are read as rows");
    CliOutlineRow helper;
    CliOutlineRow entry;
    cli_check(row_with_name("helper", &helper) && (helper.line == 1ULL) && (helper.depth == 0u) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = helper.type, .cap = CLI_POOL_BYTES, .other = "function", .other_cap = 9u}),
              "helper is a function on the second line at the top level");
    cli_check(row_with_name("main", &entry) && (entry.line == 5ULL), "main is on the sixth line");
    CliOutlineRow first;
    CliOutlineRow second;
    cli_check(EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = 0u, .row = &first) &&
                  EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = 1u, .row = &second) && (first.line <= second.line),
              "the rows are in the order of their lines");
    cli_check(!EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = rows, .row = &first), "no row is past the last");

    const GuiPaneTable *const table = &gui_structure_table;
    cli_check(EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "structure") == table, "the Structure tool has the table");
    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    cli_check((pane.pointed == GUI_PANE_NONE) && (pane.scroll == 0u) && (pane.action == 0u),
              "the pane begins pointing at no row and no action");
    unsigned int main_row = 0u;
    for (unsigned int at = 0u; EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at, .row = &first); at += 1u)
    {
        main_row = (first.line == entry.line) ? at : main_row;
    }
    cli_check(EMBED_CALL(table->current, GuiPaneCfg, .cursor_line = 7ULL) == (int)main_row,
              "the cursor inside main is in main's row");
    cli_check(EMBED_CALL(table->current, GuiPaneCfg, .cursor_line = 0ULL) <= 0,
              "the cursor on the first line is in the first row or none");

    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Structure", .tree = "", .cursor_line = 7ULL);
    const GuiRect main_rect =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = main_row);
    const unsigned int other_row = (main_row == 0u) ? 1u : 0u;
    const GuiRect other_rect =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = other_row);
    const GuiColor on_main =
        raster.pixels[((unsigned int)(main_rect.y + 2) * raster.width) + (unsigned int)main_rect.x + 2u];
    const GuiColor on_other =
        raster.pixels[((unsigned int)(other_rect.y + 2) * raster.width) + (unsigned int)other_rect.x + 2u];
    cli_check(on_main != on_other, "the cursor's row is drawn on a ground of its own");
    cli_check(main_rect.y == EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = table, .frame = &frame) +
                                 ((int)main_row * GUI_PANE_ROW),
              "a row is its number of rows under the pane's head");

    unsigned int hit_row = 0u;
    cli_check(EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .x = main_rect.x + 20, .y = main_rect.y + (main_rect.height / 2), .row_pointer = &hit_row) &&
                  (hit_row == main_row),
              "a press on main's row lands on it");
    cli_check(!EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                          .x = main_rect.x + 20, .y = frame.side.y + 10, .row_pointer = &hit_row),
              "a press on the group's title lands on no row");
    cli_check(EMBED_CALL(gui_pane.move, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame, .glyphs = &glyphs,
                         .x = main_rect.x + 20, .y = main_rect.y + 3) &&
                  (pane.pointed == (int)main_row),
              "the pointer on main's row marks it");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Structure", .tree = "", .cursor_line = 0ULL);
    cli_check(raster.pixels[((unsigned int)(main_rect.y + 2) * raster.width) + (unsigned int)main_rect.x + 2u] !=
                  on_main,
              "the row pointed is drawn on the line's ground");
    cli_check(!EMBED_CALL(gui_pane.wheel, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame, .rows = 3),
              "a few rows do not scroll");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = main_rect.x + 20, .y = main_rect.y - 10) == 0u,
              "the Outline's head offers no action");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = main_row, .open_out = &open),
              "a row pressed with no view displayed opens nothing");

    EMBED_CALL(cli_outline.read, CliOutlineCfg, .key = "check:none", .path = "none.c");
    cli_check(EMBED_CALL(cli_outline.rows, CliOutlineCfg, .at = 0u) == 0u, "no text under a key leaves no rows");

    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(gui_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
