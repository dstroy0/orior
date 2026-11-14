// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pane_test.c: the side pane's check through the explorer's and the problems' tables: a tool without a pane has no
// table and the side draws its title alone; the explorer's table lists a tree's folder and file as rows headed by the
// tree's name, a folder's row pressed opens it and a file's opens the file; the problems' table draws its own head in
// place of the pane's, the whole head offering the check, its rows of two heights; and a pointer leaving the side
// marks no row
#include "../../../../../includes/window/block.h"
#include "../../../../../ui/cli/src/command_line/commands.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "../../../../../ui/gui/src/explorer.h"
#include "../../../../../ui/gui/src/problems.h"
#include "../cli/cli_check.h"

// the file `relative` of the tree at `root` written with `text`, its folder put in place first
static void file_written(const char *root, const char *relative, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = relative, .joined = path);
    // the place past the path's last slash, 0 where it has none
    const unsigned long long past_slash =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = sizeof(path)}), .set = "/");
    if (past_slash != 0ULL)
    {
        path[past_slash - 1ULL] = '\0';
        (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
        path[past_slash - 1ULL] = '/';
    }
    cli_check(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                         .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})),
              relative);
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
    GuiIcons icons;
    EMBED_CALL(gui_icons.init, GuiIconsCfg, .icons = &icons);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");

    cli_check(EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "more") == NULL, "a tool without a pane has no table");
    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = NULL, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Debug", .tree = "");
    cli_check(1, "the side draws its title alone for a tool without a pane");

    static char s_root[CLI_PATH_MAX_LEN];
    cli_check_folder("pane_tree", s_root);
    file_written(s_root, "src/main.c", "int main(void)\n{\n    return 0;\n}\n");
    file_written(s_root, "notes.txt", "one\n");
    const GuiPaneTable *const explorer = EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "explorer");
    cli_check(explorer == &gui_explorer_table, "the explorer tool has its table");
    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = explorer, .event = GUI_PANE_DISPLAYED, .root = s_root);
    cli_check(EMBED_CALL(explorer->rows, GuiPaneCfg, .table = explorer) == 2u, "the tree's folder and file are rows");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(explorer->title, GuiPaneCfg, .tree = "pane_tree"),
                                           .cap = CLI_POOL_BYTES,
                                           .other = "pane_tree",
                                           .other_cap = 10u}),
              "the explorer's pane is headed by the tree's name");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = explorer, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Explorer", .tree = "pane_tree");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = explorer, .row = 0u, .open_out = &open) &&
                  (EMBED_CALL(explorer->rows, GuiPaneCfg, .table = explorer) == 3u),
              "the folder's row pressed opens it, its file a row under it");
    cli_check(EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = explorer, .row = 1u, .open_out = &open) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = open.path, .cap = CLI_POOL_BYTES, .other = "src/main.c", .other_cap = 11u}) &&
                  !open.placed,
              "the file's row pressed opens the file");
    const GuiRect first =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = explorer, .frame = &frame, .pane_read = &pane, .row = 0u);
    cli_check(EMBED_CALL(gui_pane.move, GuiPaneCfg, .table = explorer, .pane = &pane, .frame = &frame, .glyphs = &glyphs,
                         .x = first.x + 20, .y = first.y + 3) &&
                  (pane.pointed == 0),
              "the pointer on the folder's row marks it");
    cli_check(EMBED_CALL(gui_pane.move, GuiPaneCfg, .table = explorer, .pane = &pane, .frame = &frame, .glyphs = &glyphs,
                         .x = frame.side.x + frame.side.width + 40, .y = first.y + 3) &&
                  (pane.pointed == GUI_PANE_NONE),
              "the pointer leaving the side marks no row");
    // the rows' menus are the catalog's Files and Explorer popups, each row run with the row's own path
    char read_text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea read_err_msg = {read_text, sizeof(read_text), 0ULL};
    cli_check(EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &read_err_msg), "the catalog is read");
    unsigned int built = 0u;
    const CliItem *rows = NULL;
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = explorer, .row = 0u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) && (built > 8u) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].label, .cap = CLI_POOL_BYTES, .other = "Close", .other_cap = 6u}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = rows[0].args, .cap = CLI_POOL_BYTES, .other = "src", .other_cap = 4u}) &&
                  rows[3].line &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[8].command, .cap = CLI_POOL_BYTES, .other = "file-copy-path", .other_cap = 15u}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = rows[8].args, .cap = CLI_POOL_BYTES, .other = "src", .other_cap = 4u}),
              "the open folder's menu closes it and copies its path");
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = explorer, .row = 1u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].label, .cap = CLI_POOL_BYTES, .other = "Open", .other_cap = 5u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].command, .cap = CLI_POOL_BYTES, .other = "file-open", .other_cap = 10u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].args, .cap = CLI_POOL_BYTES, .other = "src/main.c", .other_cap = 11u}),
              "the file's menu opens it");
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = explorer, .row = GUI_PANE_OFF_ROWS, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) && (built >= 2u) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].command, .cap = CLI_POOL_BYTES, .other = "explorer-refresh", .other_cap = 17u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[1].command, .cap = CLI_POOL_BYTES, .other = "explorer-collapse", .other_cap = 18u}),
              "the pane's own menu lists again and collapses");
    cli_files_explorer_refresh();
    cli_check(EMBED_CALL(explorer->rows, GuiPaneCfg, .table = explorer) == 3u,
              "the explorer listed again is with its folder open");
    cli_files_explorer_collapse();
    cli_check(EMBED_CALL(explorer->rows, GuiPaneCfg, .table = explorer) == 2u, "the folders collapsed close");
    cli_check(cli_files_explorer_open("src") && (EMBED_CALL(explorer->rows, GuiPaneCfg, .table = explorer) == 3u) &&
                  !cli_files_explorer_open("notes.txt"),
              "a folder opened by its path, a file not");

    const GuiPaneTable *const problems = EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "problems");
    cli_check((problems == &gui_problems_table) && (problems->title == NULL), "the problems' table draws its own head");
    GuiPane problems_pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &problems_pane);
    cli_check(EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = problems, .frame = &frame) ==
                  frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PROBLEMS_HEAD,
              "the problems' rows are under the side's head, the line and the problems' head");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = problems, .pane_read = &problems_pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.side.x + 20,
                         .y = frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + (GUI_PROBLEMS_HEAD / 2)) ==
                  GUI_PROBLEMS_CHECK,
              "the whole problems' head offers the check");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = problems, .pane_read = &problems_pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.side.x + 20, .y = frame.side.y + 10) == 0u,
              "the group's title offers no action");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = problems, .pane_read = &problems_pane, .frame = &frame,
               .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Problems",
               .tree = "");
    unsigned int hit_row = 0u;
    cli_check(!EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = problems, .pane_read = &problems_pane, .frame = &frame,
                          .x = frame.side.x + 20, .y = frame.side.y + frame.side.height - 10, .row_pointer = &hit_row),
              "no problem is, so no row is pressed");

    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    for (unsigned int at = 0u; at < GUI_ICON_CACHE_ENTRIES; at += 1u)
    {
        if (icons.entry[at].used)
        {
            icons.entry[at].mask_bits.cover = NULL;
            icons.entry[at].used = 0;
        }
    }
    icons.count = 0u;
    EMBED_CALL(gui_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
