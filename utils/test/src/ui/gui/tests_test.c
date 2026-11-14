// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tests_test.c: the Tests pane's check: a tree of two Python test files reads as a row a file and a row a test under
// each, labeled as pytest labels them; a file's row pressed closes its tests and opens them again; a run's reports, as
// test_runs.h writes them, set a test's outcome, its time and its failure's message, which takes a row of its own,
// the file's worst outcome and its counts, the summary of the run's end and the coverage's rows; the pane's table draws
// the bar with its actions and the rows under the Tests group's title, a press on Run All lands on the action and a
// press on a row lands on it; and no tree leaves no rows
#include "../../../../../includes/window/block.h"
#include "../../../../../ui/cli/src/tests/testing.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "../../../../../ui/gui/src/tests.h"
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

// a run's report `json` handed to the pane as the run's thread hands it
static void reported(const char *json)
{
    cli_tests_pane_event(&(CliEachJsonSizeCfg){
        .context = NULL, .json = json, .size = cellul.len(&(CatenaFinitaCfg){.src = json, .cap = CLI_POOL_BYTES})});
}

// the row `at`, into `row`: whether it is of `type` with the name `name`
static int row_is(unsigned int at, unsigned int type, const char *name, CliTestsRow *row)
{
    return EMBED_CALL(cli_tests_pane.row, CliTestsPaneCfg, .at = at, .row = row) && (row->type == type) &&
           (row->name_length == cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES})) &&
           (memor.cmp(&(MemoriaCfg){.src = row->name, .other = name, .bytes = row->name_length}) == 0);
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

    static char s_root[CLI_PATH_MAX_LEN];
    cli_check_folder("tests_tree", s_root);
    file_written(s_root, "pkg/c_test.py", "class TestThing:\n    def test_c(self):\n        pass\n");
    file_written(s_root, "tests/test_a.py", "def test_a():\n    pass\n\n\ndef test_b():\n    pass\n");

    const GuiPaneTable *const table = &gui_tests_table;
    cli_check(EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "tests") == table, "the Tests tool has the table");
    EMBED_CALL(cli_tests_pane.begin, CliTestsPaneCfg, .woken = NULL, .entries = NULL);
    cli_check(!EMBED_CALL(cli_tests_pane.loaded, CliTestsPaneCfg, .at = 0u), "the tests begin not read");
    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_SHOWN, .root = s_root);
    cli_check(EMBED_CALL(cli_tests_pane.loaded, CliTestsPaneCfg, .at = 0u), "the pane shown reads the tree's tests");
    cli_check(EMBED_CALL(cli_tests_pane.rows, CliTestsPaneCfg, .at = 0u) == 5u,
              "two files and three tests are five rows");
    CliTestsRow row;
    cli_check(row_is(0u, CLI_TESTS_ROW_FILE, "c_test.py", &row) && (row.folder_length == 3ULL) && row.open &&
                  (row.outcome == CLI_TESTS_NONE),
              "the first file's row carries its folder, stands open and has no outcome");
    cli_check(row_is(1u, CLI_TESTS_ROW_TEST, "TestThing.test_c", &row) && (row.line == 1ULL),
              "a method's label joins its class with a dot and opens at its line");
    cli_check(row_is(2u, CLI_TESTS_ROW_FILE, "test_a.py", &row), "the second file follows the first's tests");
    cli_check(row_is(3u, CLI_TESTS_ROW_TEST, "test_a", &row) && row_is(4u, CLI_TESTS_ROW_TEST, "test_b", &row) &&
                  (row.line == 4ULL),
              "the second file's tests follow in the order of their lines");
    cli_check(!EMBED_CALL(cli_tests_pane.row, CliTestsPaneCfg, .at = 5u, .row = &row), "no row stands past the last");
    EMBED_CALL(cli_tests_pane.toggle, CliTestsPaneCfg, .at = 0u);
    cli_check((EMBED_CALL(cli_tests_pane.rows, CliTestsPaneCfg, .at = 0u) == 4u) &&
                  row_is(1u, CLI_TESTS_ROW_FILE, "test_a.py", &row),
              "a file's row pressed closes its tests");
    EMBED_CALL(cli_tests_pane.toggle, CliTestsPaneCfg, .at = 0u);
    cli_check(EMBED_CALL(cli_tests_pane.rows, CliTestsPaneCfg, .at = 0u) == 5u, "pressed again it opens them");
    cli_check(!EMBED_CALL(cli_tests_pane.running, CliTestsPaneCfg, .at = 0u) &&
                  !EMBED_CALL(cli_tests_pane.any_failed, CliTestsPaneCfg, .at = 0u) &&
                  !EMBED_CALL(cli_tests_pane.said, CliTestsPaneCfg, .at = 0u),
              "before a run nothing runs, nothing failed and no summary stands");

    reported("{\"kind\":\"result\",\"run\":1,\"outcome\":{\"id\":\"tests/test_a.py::test_a\",\"outcome\":\"failed\","
             "\"message\":\"assert 1 == 2\\nmore\",\"file\":\"tests/test_a.py\",\"line\":1,\"seconds\":0.4123}}");
    cli_check(EMBED_CALL(cli_tests_pane.changed, CliTestsPaneCfg, .at = 0u) &&
                  !EMBED_CALL(cli_tests_pane.changed, CliTestsPaneCfg, .at = 0u),
              "a report marks a change once");
    cli_check(EMBED_CALL(cli_tests_pane.rows, CliTestsPaneCfg, .at = 0u) == 6u, "a failure's message takes a row");
    cli_check(row_is(3u, CLI_TESTS_ROW_TEST, "test_a", &row) && (row.outcome == CLI_TESTS_FAILED) &&
                  (row.milliseconds == 412u) && (row.line == 1ULL),
              "the failed test carries its outcome, its time and where it failed");
    cli_check(row_is(4u, CLI_TESTS_ROW_MESSAGE, "assert 1 == 2", &row), "the message's first line follows the test");
    cli_check(row_is(2u, CLI_TESTS_ROW_FILE, "test_a.py", &row) && (row.outcome == CLI_TESTS_FAILED) &&
                  (row.failed == 1u) && (row.passed == 0u),
              "the file's row takes the worst outcome and counts the failure");
    cli_check(EMBED_CALL(cli_tests_pane.any_failed, CliTestsPaneCfg, .at = 0u), "a test failed");
    reported("{\"kind\":\"result\",\"run\":1,\"outcome\":{\"id\":\"tests/test_a.py::test_b\",\"outcome\":\"passed\","
             "\"message\":\"\",\"file\":null,\"line\":null,\"seconds\":1.25}}");
    cli_check(row_is(5u, CLI_TESTS_ROW_TEST, "test_b", &row) && (row.outcome == CLI_TESTS_PASSED) &&
                  (row.milliseconds == 1250u) && (row.line == 4ULL),
              "the passed test carries its outcome and time and opens where it is written");
    reported("{\"kind\":\"result\",\"run\":1,\"outcome\":{\"id\":\"tests/test_a.py::test_none\",\"outcome\":\"passed\","
             "\"message\":\"\",\"file\":null,\"line\":null,\"seconds\":0.1}}");
    cli_check(EMBED_CALL(cli_tests_pane.rows, CliTestsPaneCfg, .at = 0u) == 6u, "a test not found adds no row");
    reported("{\"kind\":\"done\",\"run\":1,\"passed\":1,\"failed\":1,\"skipped\":0,\"seconds\":1.66,\"message\":\"\"}");
    char summary[CLI_TESTS_SUMMARY];
    int failed = 0;
    cli_check(EMBED_CALL(cli_tests_pane.said, CliTestsPaneCfg, .at = 0u) &&
                  (EMBED_CALL(cli_tests_pane.summary, CliTestsPaneCfg, .text = summary, .failed_out = &failed) != 0ULL) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = summary,
                                               .cap = sizeof(summary),
                                               .other = "1 passed, 1 failed in 1.7 s",
                                               .other_cap = 28u}) &&
                  failed,
              "the run's end is summed up with its seconds to a tenth");
    reported("{\"kind\":\"coverage\",\"run\":1,\"files\":[{\"file\":\"pkg/c.py\",\"ran\":[1,2,3],\"missed\":[9,5]}]}");
    cli_check(EMBED_CALL(cli_tests_pane.rows, CliTestsPaneCfg, .at = 0u) == 8u, "the coverage takes a head and a row");
    cli_check(EMBED_CALL(cli_tests_pane.row, CliTestsPaneCfg, .at = 6u, .row = &row) &&
                  (row.type == CLI_TESTS_ROW_COVER_HEAD) && (row.passed == 3u) && (row.failed == 2u),
              "the coverage's head counts the lines that ran and were missed");
    cli_check(row_is(7u, CLI_TESTS_ROW_COVER, "pkg/c.py", &row) && (row.passed == 3u) && (row.failed == 2u) &&
                  (row.line == 5ULL),
              "a covered file's row carries its lines and its first line missed");

    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    const int rows_top = EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = table, .frame = &frame);
    cli_check(rows_top == frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PANE_HEAD + GUI_TESTS_BAR +
                              GUI_TESTS_SAID,
              "the rows stand under the pane's head, the bar and the summary");
    const int top_menu_y = frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PANE_HEAD + (GUI_TESTS_BAR / 2);
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.side.x + GUI_TESTS_ACTION_PAD + 4, .y = top_menu_y) == CLI_TESTS_RUN,
              "a press at the bar's left lands on Run All");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.side.x + frame.side.width - GUI_TESTS_ACTION_PAD - 4,
                         .y = top_menu_y) == CLI_TESTS_PARALLEL,
              "a press at the bar's right lands on the Parallel choice");
    cli_check(EMBED_CALL(gui_pane.move, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame, .glyphs = &glyphs,
                         .x = frame.side.x + GUI_TESTS_ACTION_PAD + 4, .y = top_menu_y) &&
                  (pane.action == CLI_TESTS_RUN) && (pane.pointed == GUI_PANE_NONE),
              "the pointer on Run All marks the action and no row");
    const GuiRect first =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 0u);
    unsigned int hit_row = 0u;
    cli_check(EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .x = first.x + 20,
                         .y = first.y + 3, .row_pointer = &hit_row) &&
                  (hit_row == 0u),
              "a press on the first row lands on it");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Tests", .tree = "");
    const GuiRect message =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 4u);
    cli_check(message.y == first.y + (4 * GUI_PANE_ROW), "the message's row stands four rows under the first");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 3u, .open_out = &open) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = open.path, .cap = CLI_POOL_BYTES, .other = "tests/test_a.py", .other_cap = 16u}) &&
                  open.placed && (open.place.line == 1ULL),
              "the failed test's row pressed opens its file where it failed");
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 4u, .open_out = &open),
              "a message's row pressed opens nothing");
    cli_check(EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 7u, .open_out = &open) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = open.path, .cap = CLI_POOL_BYTES, .other = "pkg/c.py", .other_cap = 9u}) &&
                  (open.place.line == 5ULL),
              "a covered file's row pressed opens it at its first line missed");
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 0u, .open_out = &open) &&
                  (EMBED_CALL(cli_tests_pane.rows, CliTestsPaneCfg, .at = 0u) == 7u),
              "a file's row pressed closes its tests and opens no file");

    EMBED_CALL(cli_tests_pane.read, CliTestsPaneCfg, .root = NULL);
    cli_check(EMBED_CALL(cli_tests_pane.rows, CliTestsPaneCfg, .at = 0u) == 2u,
              "no tree leaves no files, the coverage standing");

    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(gui_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
