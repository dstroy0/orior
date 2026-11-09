// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// search_test.c: the Search pane's check: text typed rests before the tree is searched, then a tree of three files
// lists a row a file with its count and a row a line found under it with the match's place and bytes, case aside; the
// case choice flipped searches at once and drops the line of another case; a file's row pressed closes its lines and
// opens them again; the query erased lists nothing, one that is found nowhere reports so, and Enter searches without
// the rest; the pane's table draws the field, the choices and the rows, a press on the field takes the keys, which
// typed text then reaches and Escape lets go, and a line's row pressed opens the file at the match
#include "../../../../../ui/cli/src/editor/files.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "../../../../../ui/cli/src/view/parts/search.h"
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

// the row `at`, into `row`: whether it is of `type` with the name or text `text`
static int row_is(unsigned int at, unsigned int type, const char *text, CliSearchRow *row)
{
    if (!EMBED_CALL(cli_search_pane.row, CliSearchPaneCfg, .at = at, .row = row) || (row->type != type))
    {
        return 0;
    }
    const char *const one = (type == CLI_SEARCH_ROW_FILE) ? row->name : row->text;
    const unsigned long long length = (type == CLI_SEARCH_ROW_FILE) ? row->name_length : row->text_length;
    return (length == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})) &&
           (memor.cmp(&(MemoriaCfg){.src = one, .other = text, .bytes = length}) == 0);
}

// whether the report reads `text`
static int report_is(const char *text)
{
    unsigned long long length = 0ULL;
    const char *const report = EMBED_CALL(cli_search_pane.report, CliSearchPaneCfg, .length_out = &length);
    return cellul.eq(&(CatenaFinitaCfg){.src = report, .cap = CLI_POOL_BYTES, .other = text, .other_cap = CLI_POOL_BYTES});
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
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

    static char s_root[CLI_PATH_MAX_LEN];
    cli_check_folder("search_tree", s_root);
    file_written(s_root, "a.txt", "alpha beta\ngamma alpha\n");
    file_written(s_root, "sub/b.txt", "Alpha\n");
    file_written(s_root, "c.txt", "nothing here\n");

    const GuiPaneTable *const table = &gui_search_table;
    cli_check(EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "search") == table, "the Search tool has the table");
    EMBED_CALL(cli_search_pane.begin, CliSearchPaneCfg, .woken = NULL, .entries = NULL);
    cli_check((EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 0u) && report_is(""),
              "nothing is listed and nothing reported before a search");
    cli_check(EMBED_CALL(cli_search_pane.deadline_ms, CliSearchPaneCfg, .at = 0u) == 0ULL,
              "no search waits before typing");
    EMBED_CALL(cli_search_pane.type, CliSearchPaneCfg, .text = "alpha", .size = 5ULL, .now_ms = 1000ULL);
    unsigned long long length = 0ULL;
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_search_pane.query, CliSearchPaneCfg, .length_out = &length),
                                           .cap = CLI_POOL_BYTES,
                                           .other = "alpha",
                                           .other_cap = 6u}) &&
                  (length == 5ULL),
              "the text typed is the query");
    cli_check(EMBED_CALL(cli_search_pane.deadline_ms, CliSearchPaneCfg, .at = 0u) == 1000ULL + CLI_SEARCH_REST_MS,
              "the search waits the rest after the typing");
    cli_check(!EMBED_CALL(cli_search_pane.step, CliSearchPaneCfg, .root = s_root, .now_ms = 1100ULL),
              "a step inside the rest searches nothing");
    cli_check(EMBED_CALL(cli_search_pane.step, CliSearchPaneCfg, .root = s_root, .now_ms = 1300ULL),
              "the step at the rest's end searches the tree");
    cli_check(EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 5u,
              "two files with three lines found are five rows");
    CliSearchRow row;
    cli_check(row_is(0u, CLI_SEARCH_ROW_FILE, "a.txt", &row) && (row.count == 2u) && row.open &&
                  (row.folder_length == 0ULL) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = row.path, .cap = CLI_POOL_BYTES, .other = "a.txt", .other_cap = 6u}),
              "the first file's row carries its name, count and path, its lines showing");
    cli_check(row_is(1u, CLI_SEARCH_ROW_HIT, "alpha beta", &row) && (row.line == 1ULL) && (row.col == 1ULL) &&
                  (row.match_at == 0ULL) && (row.match_length == 5ULL),
              "the first line found carries its line, column and the match's place and bytes");
    cli_check(row_is(2u, CLI_SEARCH_ROW_HIT, "gamma alpha", &row) && (row.line == 2ULL) && (row.match_at == 6ULL),
              "the second line found stands under the same file");
    cli_check(row_is(3u, CLI_SEARCH_ROW_FILE, "b.txt", &row) && (row.folder_length == 3ULL) &&
                  cellul.starts(&(CatenaFinitaCfg){
                      .src = row.path, .cap = CLI_POOL_BYTES, .other = "sub", .other_cap = 4u}) &&
                  (row.count == 1u),
              "the second file's row carries its folder");
    cli_check(row_is(4u, CLI_SEARCH_ROW_HIT, "Alpha", &row), "a line of another case is found while case is off");
    cli_check(report_is("3 results in 2 files"), "the report counts the results and files");
    cli_check(!EMBED_CALL(cli_search_pane.row, CliSearchPaneCfg, .at = 5u, .row = &row), "no row stands past the last");

    EMBED_CALL(cli_search_pane.option, CliSearchPaneCfg, .which = CLI_SEARCH_CASE, .now_ms = 2000ULL);
    cli_check(EMBED_CALL(cli_search_pane.on, CliSearchPaneCfg, .which = CLI_SEARCH_CASE) &&
                  !EMBED_CALL(cli_search_pane.on, CliSearchPaneCfg, .which = CLI_SEARCH_WORD),
              "the case choice flipped is on, the others off");
    cli_check(EMBED_CALL(cli_search_pane.step, CliSearchPaneCfg, .root = s_root, .now_ms = 2000ULL) &&
                  (EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 3u) && report_is("2 results in 1 file"),
              "a choice flipped searches at once and the other case's line drops");
    EMBED_CALL(cli_search_pane.toggle, CliSearchPaneCfg, .at = 0u);
    cli_check((EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 1u) &&
                  row_is(0u, CLI_SEARCH_ROW_FILE, "a.txt", &row) && !row.open,
              "a file's row pressed closes its lines");
    EMBED_CALL(cli_search_pane.toggle, CliSearchPaneCfg, .at = 0u);
    cli_check(EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 3u, "pressed again it opens them");

    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        cli_check(EMBED_CALL(cli_search_pane.key, CliSearchPaneCfg, .name = "Backspace", .now_ms = 3000ULL),
                  "Backspace is taken");
    }
    cli_check(!EMBED_CALL(cli_search_pane.key, CliSearchPaneCfg, .name = "ArrowLeft", .now_ms = 3000ULL),
              "a key the field has no use for is not taken");
    cli_check(EMBED_CALL(cli_search_pane.step, CliSearchPaneCfg, .root = s_root, .now_ms = 3300ULL) &&
                  (EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 0u) && report_is(""),
              "the query erased lists nothing and reports nothing");
    EMBED_CALL(cli_search_pane.type, CliSearchPaneCfg, .text = "zzz", .size = 3ULL, .now_ms = 4000ULL);
    cli_check(EMBED_CALL(cli_search_pane.key, CliSearchPaneCfg, .name = "Enter", .now_ms = 4000ULL) &&
                  EMBED_CALL(cli_search_pane.step, CliSearchPaneCfg, .root = s_root, .now_ms = 4000ULL),
              "Enter searches at the next step without the rest");
    cli_check((EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 0u) && report_is("No results found."),
              "a query found nowhere reports so");
    EMBED_CALL(cli_search_pane.option, CliSearchPaneCfg, .which = CLI_SEARCH_CASE, .now_ms = 5000ULL);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        (void)EMBED_CALL(cli_search_pane.key, CliSearchPaneCfg, .name = "Backspace", .now_ms = 5000ULL);
    }
    EMBED_CALL(cli_search_pane.type, CliSearchPaneCfg, .text = "alpha", .size = 5ULL, .now_ms = 5000ULL);
    cli_check(EMBED_CALL(cli_search_pane.step, CliSearchPaneCfg, .root = s_root, .now_ms = 5300ULL) &&
                  (EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 5u),
              "the query typed again finds the three lines");

    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    const int rows_top = EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = table, .frame = &frame);
    cli_check(rows_top == frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PANE_HEAD + GUI_PANE_BOX +
                              GUI_SEARCH_REPORT,
              "the rows stand under the pane's head, the field and the report, no set listed");
    const int field_y = frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PANE_HEAD + (GUI_PANE_BOX / 2);
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.side.x + GUI_PANE_PAD_LEFT + 20, .y = field_y) == CLI_SEARCH_FIELD,
              "a press in the field lands on it");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.side.x + frame.side.width - GUI_PANE_PAD_LEFT - 8,
                         .y = field_y) == CLI_SEARCH_REGEX,
              "a press at the field's right end lands on the regular expression choice");
    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane, .which = CLI_SEARCH_FIELD, .root = s_root);
    cli_check(pane.keys, "the field pressed takes the keys");
    cli_check(EMBED_CALL(gui_pane.text, GuiPaneCfg, .table = table, .pane = &pane, .text = " beta", .length = 5ULL,
                         .root = s_root) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = EMBED_CALL(cli_search_pane.query, CliSearchPaneCfg, .length_out = &length),
                      .cap = CLI_POOL_BYTES,
                      .other = "alpha beta",
                      .other_cap = 11u}),
              "text typed while the field has the keys reaches the query");
    cli_check(EMBED_CALL(gui_pane.key, GuiPaneCfg, .table = table, .pane = &pane, .name = "Escape", .root = s_root) &&
                  !pane.keys,
              "Escape lets the keys go");
    cli_check(!EMBED_CALL(gui_pane.text, GuiPaneCfg, .table = table, .pane = &pane, .text = "x", .length = 1ULL,
                          .root = s_root),
              "text typed without the keys goes elsewhere");
    cli_check(EMBED_CALL(cli_search_pane.step, CliSearchPaneCfg, .root = s_root, .now_ms = 9000ULL) &&
                  (EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 2u) &&
                  row_is(1u, CLI_SEARCH_ROW_HIT, "alpha beta", &row) && (row.match_length == 10ULL),
              "the longer query finds its one line with the match's bytes");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Search", .tree = "");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 1u, .open_out = &open) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = open.path, .cap = CLI_POOL_BYTES, .other = "a.txt", .other_cap = 6u}) &&
                  open.placed && (open.place.line == 0ULL) &&
                  (open.place.col == 0ULL),
              "a line's row pressed opens the file at the match, counted from zero");
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 0u, .open_out = &open) &&
                  (EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u) == 1u),
              "a file's row pressed closes its lines and opens no file");

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
