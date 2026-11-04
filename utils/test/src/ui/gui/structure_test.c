// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure_test.c: the Outline pane's check: a C text stored under a key has its declarations read as rows in the order
// of their lines, each as deep as its line's indent in steps of the smallest; the row the cursor stands in is the last
// at or before its line; the pane draws the rows under the Structure group's title on the side, the cursor's row on a
// ground of its own; a press lands on a row; and no text under the key leaves no rows
#include "../../../../../ui/cli/src/editor/document.h"
#include "../../../../../ui/cli/src/view/parts/pane.h"
#include "../../../../../ui/cli/src/view/parts/structure.h"
#include "../cli/cli_check.h"

#include <string.h>

static const char s_key[] = "check:outline";

// the row whose name is `name`, into `row`: 1, or 0 where none is
static int row_with_name(const char *name, CliOutlineRow *row)
{
    for (unsigned int at = 0u; EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at, .row = row); at += 1u)
    {
        if ((strlen(name) == row->length) && (memcmp(name, row->name, row->length) == 0))
        {
            return 1;
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
                  (strcmp(helper.kind, "function") == 0),
              "helper is a function on the second line at the top level");
    cli_check(row_with_name("main", &entry) && (entry.line == 5ULL), "main is on the sixth line");
    CliOutlineRow first;
    CliOutlineRow second;
    cli_check(EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = 0u, .row = &first) &&
                  EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = 1u, .row = &second) && (first.line <= second.line),
              "the rows stand in the order of their lines");
    cli_check(!EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = rows, .row = &first), "no row stands past the last");

    GuiStructure structure;
    EMBED_CALL(gui_structure.init, GuiStructureCfg, .structure_out = &structure);
    cli_check((structure.pointed == GUI_PANE_NONE) && (structure.scroll == 0u), "the pane begins pointing at no row");
    unsigned int main_row = 0u;
    for (unsigned int at = 0u; EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at, .row = &first); at += 1u)
    {
        main_row = (first.line == entry.line) ? at : main_row;
    }
    cli_check(EMBED_CALL(gui_structure_row_frame.current, GuiStructureRowFrameCfg, .structure = &structure,
                         .frame = &frame, .cursor_line = 7ULL) == (int)main_row,
              "the cursor inside main stands in main's row");
    cli_check(EMBED_CALL(gui_structure_row_frame.current, GuiStructureRowFrameCfg, .structure = &structure,
                         .frame = &frame, .cursor_line = 0ULL) <= 0,
              "the cursor on the first line stands in the first row or none");

    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    EMBED_CALL(gui_structure_row_frame.draw, GuiStructureRowFrameCfg, .structure = &structure, .frame = &frame,
               .cursor_line = 7ULL, .raster = &raster, .glyphs = &glyphs, .palette = palette);
    const GuiRect main_rect = EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .frame = &frame, .row = main_row, .scroll = 0u);
    const unsigned int other_row = (main_row == 0u) ? 1u : 0u;
    const GuiRect other_rect =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .frame = &frame, .row = other_row, .scroll = 0u);
    const GuiColor on_main =
        raster.pixels[((unsigned int)(main_rect.y + 2) * raster.width) + (unsigned int)main_rect.x + 2u];
    const GuiColor on_other =
        raster.pixels[((unsigned int)(other_rect.y + 2) * raster.width) + (unsigned int)other_rect.x + 2u];
    cli_check(on_main != on_other, "the cursor's row is drawn on a ground of its own");
    cli_check(main_rect.y ==
                  EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .frame = &frame) + ((int)main_row * GUI_PANE_ROW),
              "a row stands its number of rows under the pane's head");

    unsigned int hit_row = 0u;
    cli_check(EMBED_CALL(gui_structure_row_frame.hit, GuiStructureRowFrameCfg, .structure = &structure,
                         .frame = &frame, .x = main_rect.x + 20, .y = main_rect.y + (main_rect.height / 2),
                         .row_pointer = &hit_row) &&
                  (hit_row == main_row),
              "a press on main's row lands on it");
    cli_check(!EMBED_CALL(gui_structure_row_frame.hit, GuiStructureRowFrameCfg, .structure = &structure,
                          .frame = &frame, .x = main_rect.x + 20, .y = frame.side.y + 10, .row_pointer = &hit_row),
              "a press on the group's title lands on no row");
    cli_check(EMBED_CALL(gui_structure_frame.move, GuiStructureFrameCfg, .structure_out = &structure, .frame = &frame,
                         .x = main_rect.x + 20, .y = main_rect.y + 3) &&
                  (structure.pointed == (int)main_row),
              "the pointer on main's row marks it");
    cli_check(!EMBED_CALL(gui_structure_row_frame.wheel, GuiStructureRowFrameCfg, .structure_out = &structure,
                          .frame = &frame, .rows = 3),
              "a few rows do not scroll");

    EMBED_CALL(cli_outline.read, CliOutlineCfg, .key = "check:none", .path = "none.c");
    cli_check(EMBED_CALL(cli_outline.rows, CliOutlineCfg, .at = 0u) == 0u, "no text under a key leaves no rows");

    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
