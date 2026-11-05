// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pane_test.c: the side pane's check through the explorer's and the problems' kinds: a tool without a pane has no
// kind and the side draws its title alone; the explorer's kind lists a tree's folder and file as rows headed by the
// tree's name, a folder's row pressed opens it and a file's opens the file; the problems' kind draws its own head in
// place of the pane's, the whole head offering the check, its rows of two heights; and a pointer leaving the side
// marks no row
#include "../../../../../ui/cli/src/command_line/commands.h"
#include "../../../../../ui/cli/src/view/parts/explorer.h"
#include "../../../../../ui/cli/src/view/parts/problems.h"
#include "../cli/cli_check.h"

#include <string.h>

// the file `relative` of the tree at `root` written with `text`, its folder put in place first
static void file_written(const char *root, const char *relative, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = relative, .joined = path);
    char *const slash = strrchr(path, '/');
    if (slash != NULL)
    {
        slash[0] = '\0';
        (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
        slash[0] = '/';
    }
    cli_check(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                         .size = strlen(text)),
              relative);
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
    GuiIcons icons;
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &icons);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");

    cli_check(EMBED_CALL(gui_pane.kind_of, GuiPaneCfg, .tool = "debug") == NULL, "a tool without a pane has no kind");
    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .kind = NULL, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Debug", .tree = "");
    cli_check(1, "the side draws its title alone for a tool without a pane");

    static char s_root[CLI_PATH_MAX_LEN];
    cli_check_folder("pane_tree", s_root);
    file_written(s_root, "src/main.c", "int main(void)\n{\n    return 0;\n}\n");
    file_written(s_root, "notes.txt", "one\n");
    const GuiPaneKind *const explorer = EMBED_CALL(gui_pane.kind_of, GuiPaneCfg, .tool = "explorer");
    cli_check(explorer == &gui_explorer_kind, "the explorer tool has its kind");
    EMBED_CALL(gui_pane.read, GuiPaneCfg, .kind = explorer, .event = GUI_PANE_SHOWN, .root = s_root);
    cli_check(EMBED_CALL(explorer->rows, GuiPaneCfg, .kind = explorer) == 2u, "the tree's folder and file are rows");
    cli_check(strcmp(EMBED_CALL(explorer->title, GuiPaneCfg, .tree = "pane_tree"), "pane_tree") == 0,
              "the explorer's pane is headed by the tree's name");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .kind = explorer, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Explorer", .tree = "pane_tree");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .kind = explorer, .row = 0u, .open_out = &open) &&
                  (EMBED_CALL(explorer->rows, GuiPaneCfg, .kind = explorer) == 3u),
              "the folder's row pressed opens it, its file a row under it");
    cli_check(EMBED_CALL(gui_pane.press, GuiPaneCfg, .kind = explorer, .row = 1u, .open_out = &open) &&
                  (strcmp(open.path, "src/main.c") == 0) && !open.placed,
              "the file's row pressed opens the file");
    const GuiRect first =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .kind = explorer, .frame = &frame, .pane_read = &pane, .row = 0u);
    cli_check(EMBED_CALL(gui_pane.move, GuiPaneCfg, .kind = explorer, .pane = &pane, .frame = &frame, .glyphs = &glyphs,
                         .x = first.x + 20, .y = first.y + 3) &&
                  (pane.pointed == 0),
              "the pointer on the folder's row marks it");
    cli_check(EMBED_CALL(gui_pane.move, GuiPaneCfg, .kind = explorer, .pane = &pane, .frame = &frame, .glyphs = &glyphs,
                         .x = frame.side.x + frame.side.width + 40, .y = first.y + 3) &&
                  (pane.pointed == GUI_PANE_NONE),
              "the pointer leaving the side marks no row");
    unsigned int built = 0u;
    const CliItem *rows = NULL;
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .kind = explorer, .row = 0u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) && (built == 3u) &&
                  (strcmp(rows[0].label, "Close") == 0) && (strcmp(rows[0].args, "src\n") == 0) &&
                  (strcmp(rows[2].command, "file-copy-path") == 0),
              "the open folder's menu closes it and copies its path");
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .kind = explorer, .row = 1u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) && (strcmp(rows[0].label, "Open") == 0) &&
                  (strcmp(rows[0].args, "src/main.c\n") == 0),
              "the file's menu opens it");
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .kind = explorer, .row = GUI_PANE_OFF_ROWS, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) && (built == 2u) &&
                  (strcmp(rows[0].command, "explorer-refresh") == 0) &&
                  (strcmp(rows[1].command, "explorer-collapse") == 0),
              "the pane's own menu lists again and collapses");
    cli_files_explorer_refresh();
    cli_check(EMBED_CALL(explorer->rows, GuiPaneCfg, .kind = explorer) == 3u,
              "the explorer listed again stands with its folder open");
    cli_files_explorer_collapse();
    cli_check(EMBED_CALL(explorer->rows, GuiPaneCfg, .kind = explorer) == 2u, "the folders collapsed close");
    cli_check(cli_files_explorer_open("src") && (EMBED_CALL(explorer->rows, GuiPaneCfg, .kind = explorer) == 3u) &&
                  !cli_files_explorer_open("notes.txt"),
              "a folder opened by its path, a file not");

    const GuiPaneKind *const problems = EMBED_CALL(gui_pane.kind_of, GuiPaneCfg, .tool = "problems");
    cli_check((problems == &gui_problems_kind) && (problems->title == NULL), "the problems' kind draws its own head");
    GuiPane problems_pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &problems_pane);
    cli_check(EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .kind = problems, .frame = &frame) ==
                  frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PROBLEMS_HEAD,
              "the problems' rows stand under the side's head, the line and the problems' head");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .kind = problems, .pane_read = &problems_pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.side.x + 20,
                         .y = frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + (GUI_PROBLEMS_HEAD / 2)) ==
                  GUI_PROBLEMS_CHECK,
              "the whole problems' head offers the check");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .kind = problems, .pane_read = &problems_pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.side.x + 20, .y = frame.side.y + 10) == 0u,
              "the group's title offers no action");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .kind = problems, .pane_read = &problems_pane, .frame = &frame,
               .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Problems",
               .tree = "");
    unsigned int hit_row = 0u;
    cli_check(!EMBED_CALL(gui_pane.hit, GuiPaneCfg, .kind = problems, .pane_read = &problems_pane, .frame = &frame,
                          .x = frame.side.x + 20, .y = frame.side.y + frame.side.height - 10, .row_pointer = &hit_row),
              "no problem stands, so no row is pressed");

    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(cli_icons.free, GuiIconsCfg, .icons = &icons);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
