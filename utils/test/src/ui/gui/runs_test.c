// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runs_test.c: the Runs pane's check: a tree with a folder of one run and an overseer's lessons lists a folder row, a
// run row with its bytes, the lessons' head and a lesson a row; a tree without a run lists one row that reads so; no
// run live, nothing is listed again until the watch marks a run written; the run's row pressed opens it, its records
// read as the rows of its panics' heads and lines, its conference's question and accounts with the overseer's ruling,
// its count and its latest records, and pressed again closes it; the pane's table draws the rows under the Runs group's
// title and a press on a row lands on it; and no tree leaves no rows
#include "../../../../../src/cu/includes/file_defs/qry/qry_read.h"
#include "../../../../../ui/cli/src/runs/qry.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "../../../../../ui/cli/src/view/parts/runs.h"
#include "../cli/cli_check.h"

// the file `relative` of the tree at `root` written with `size` bytes of `bytes`, its folder put in place first
static void file_written(const char *root, const char *relative, const void *bytes, unsigned long long size)
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
    cli_check(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = bytes,
                         .size = size),
              relative);
}

// a record of `name` with the text `blob` at `priority` appended to `line` as a .qry stores one: its head, the name,
// the blob, zeros to a whole word and the CRC of every byte before it
static void record_appended(VerbaLinea *line, const char *name, const char *blob, unsigned int priority)
{
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
    const unsigned long long blob_length = cellul.len(&(CatenaFinitaCfg){.src = blob, .cap = CLI_POOL_BYTES});
    const unsigned long long whole = QRY_RECORD_BYTES(name_length, blob_length);
    unsigned char bytes[512];
    memor.set(&(MemoriaCfg){.dst = bytes, .val = 0, .bytes = sizeof(bytes)});
    // the record's name is far shorter than its limit
    const QryRecord head = {7ULL, blob_length, (unsigned int)name_length, priority};
    memor.cpy(&(MemoriaCfg){.dst = bytes, .src = &head, .bytes = sizeof(head)});
    memor.cpy(&(MemoriaCfg){.dst = bytes + sizeof(head), .src = name, .bytes = name_length});
    memor.cpy(&(MemoriaCfg){.dst = bytes + sizeof(head) + name_length, .src = blob, .bytes = blob_length});
    const unsigned long long crc = qry_record_crc(bytes, whole - 8ULL);
    memor.cpy(&(MemoriaCfg){.dst = bytes + whole - 8ULL, .src = &crc, .bytes = sizeof(crc)});
    (void)mmgr_line_put_n(line, (const char *)bytes, whole);
}

// the row `at`, into `row`: whether it is of `type` with the text `text`
static int row_is(unsigned int at, unsigned int type, const char *text, CliRunsRow *row)
{
    return EMBED_CALL(cli_runs_pane.row, CliRunsPaneCfg, .at = at, .row = row) && (row->type == type) &&
           ((text == NULL) ||
            ((row->text_length == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})) &&
             (memor.cmp(&(MemoriaCfg){.src = row->text, .other = text, .bytes = row->text_length}) == 0)));
}

// whether the row's second text is `text`
static int second_is(const CliRunsRow *row, const char *text)
{
    return (row->second_length == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})) &&
           (memor.cmp(&(MemoriaCfg){.src = row->second, .other = text, .bytes = row->second_length}) == 0);
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
    static char s_empty[CLI_PATH_MAX_LEN];
    cli_check_folder("runs_tree", s_root);
    cli_check_folder("runs_empty", s_empty);
    char qry[2048];
    VerbaLinea run = {qry, sizeof(qry), 0ULL};
    record_appended(&run, "hello", "x", QRY_ORDINARY);
    record_appended(&run, "conference_why_it_failed", "alpha: because\n\noverseer: ruled so\n", QRY_ORDINARY);
    record_appended(&run, "panic", "boom\nline two\n", QRY_PANIC);
    file_written(s_root, "runs/a.qry", qry, run.at);
    const char *const lessons = "lesson cause_a * stand_a\nnot a lesson\nlesson cause_b carrier_b stand_b\n";
    file_written(s_root, "runs/overseer.txt", lessons,
                 cellul.len(&(CatenaFinitaCfg){.src = lessons, .cap = CLI_POOL_BYTES}));

    const GuiPaneTable *const table = &gui_runs_table;
    cli_check(EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "runs") == table, "the Runs tool has the table");
    EMBED_CALL(cli_runs_pane.begin, CliRunsPaneCfg, .woken = NULL);
    cli_check(EMBED_CALL(cli_runs_pane.rows, CliRunsPaneCfg, .at = 0u) == 0u, "nothing is listed before a step");
    cli_check(EMBED_CALL(cli_runs_pane.step, CliRunsPaneCfg, .root = s_root, .now_ms = 1000ULL, .shown = 1),
              "the first step lists the tree's runs");
    cli_check(EMBED_CALL(cli_runs_pane.rows, CliRunsPaneCfg, .at = 0u) == 5u,
              "a folder, its run, the lessons' head and two lessons are five rows");
    CliRunsRow row;
    cli_check(row_is(0u, CLI_RUNS_ROW_FOLDER, "runs", &row), "the folder's row carries its path");
    cli_check(row_is(1u, CLI_RUNS_ROW_RUN, "a", &row) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = row.path, .cap = CLI_POOL_BYTES, .other = "runs/a.qry", .other_cap = 11u}) &&
                  (row.size == run.at) &&
                  !row.live && !row.open && (row.panicked == 0ULL),
              "the run's row carries its name without the ending, its path, its bytes and no writer");
    cli_check(row_is(2u, CLI_RUNS_ROW_LESSONS, NULL, &row), "the lessons' head follows the runs");
    cli_check(row_is(3u, CLI_RUNS_ROW_LESSON, "cause_a", &row) && second_is(&row, "*") &&
                  (row.third_length == 7ULL) &&
                  (memor.cmp(&(MemoriaCfg){.src = row.third, .other = "stand_a", .bytes = 7u}) == 0),
              "a lesson's row carries its cause, its carrier and its stand-in");
    cli_check(row_is(4u, CLI_RUNS_ROW_LESSON, "cause_b", &row) && second_is(&row, "carrier_b"),
              "a line that is no lesson is passed over and the next lesson follows");
    cli_check(!EMBED_CALL(cli_runs_pane.row, CliRunsPaneCfg, .at = 5u, .row = &row), "no row stands past the last");
    unsigned int live = 9u;
    unsigned int panicked = 9u;
    EMBED_CALL(cli_runs_pane.marks, CliRunsPaneCfg, .live_out = &live, .panicked_out = &panicked);
    cli_check((live == 0u) && (panicked == 0u), "no run is live and none has panicked");
    cli_check(!EMBED_CALL(cli_runs_pane.step, CliRunsPaneCfg, .root = s_root, .now_ms = 3000ULL, .shown = 1),
              "with no run live a later step lists nothing again");
    cli_check(EMBED_CALL(cli_runs_pane.deadline_ms, CliRunsPaneCfg, .root = s_root, .shown = 1) == 0ULL,
              "with no run live no beat is awaited");

    char other[512];
    VerbaLinea second_run = {other, sizeof(other), 0ULL};
    record_appended(&second_run, "one", "y", QRY_CRITICAL);
    file_written(s_root, "runs/b.qry", other, second_run.at);
    EMBED_CALL(cli_qry_path_root.run_write, CliQryPathRootCfg, .root = s_root, .path = "runs/b.qry",
               .length = sizeof("runs/b.qry") - 1u);
    cli_check(EMBED_CALL(cli_runs_pane.step, CliRunsPaneCfg, .root = s_root, .now_ms = 3100ULL, .shown = 1),
              "a run the watch saw written has the runs listed again");
    cli_check((EMBED_CALL(cli_runs_pane.rows, CliRunsPaneCfg, .at = 0u) == 6u) &&
                  row_is(2u, CLI_RUNS_ROW_RUN, "b", &row) && (row.size == second_run.at),
              "the new run follows the first in the order of their paths");

    EMBED_CALL(cli_runs_pane.toggle, CliRunsPaneCfg, .at = 1u, .root = s_root);
    cli_check(EMBED_CALL(cli_runs_pane.rows, CliRunsPaneCfg, .at = 0u) == 16u,
              "the run opened adds its panic's head and two lines, the question and two accounts, the count and three "
              "records");
    cli_check(row_is(1u, CLI_RUNS_ROW_RUN, "a", &row) && row.open, "the run's row stands opened");
    cli_check(row_is(2u, CLI_RUNS_ROW_PANIC, NULL, &row) && (row.number == 2ULL), "the panic's head carries its sequence");
    cli_check(row_is(3u, CLI_RUNS_ROW_PANIC_LINE, "boom", &row) && row_is(4u, CLI_RUNS_ROW_PANIC_LINE, "line two", &row),
              "the panic's text takes a row a line");
    cli_check(row_is(5u, CLI_RUNS_ROW_QUESTION, "why it failed", &row),
              "the conference's question reads its name with each underscore a space");
    cli_check(row_is(6u, CLI_RUNS_ROW_ACCOUNT, "because", &row) && second_is(&row, "alpha") && !row.ruling,
              "an account's row carries the expert and the expert's account");
    cli_check(row_is(7u, CLI_RUNS_ROW_ACCOUNT, "ruled so", &row) && second_is(&row, "overseer") && row.ruling,
              "the overseer's line is the ruling, the blank line between passed over");
    cli_check(row_is(8u, CLI_RUNS_ROW_COUNT, NULL, &row) && (row.number == 3ULL) && !row.live && (row.size == 0ULL),
              "the count row counts the records, none live and no bytes left over");
    cli_check(row_is(9u, CLI_RUNS_ROW_BLOB, "hello", &row) && (row.number == 0ULL) && (row.size == 1ULL) &&
                  (row.priority == QRY_ORDINARY),
              "the first record's row carries its sequence, name, bytes and priority");
    cli_check(row_is(10u, CLI_RUNS_ROW_BLOB, "conference_why_it_failed", &row) && (row.number == 1ULL),
              "the conference's record follows");
    cli_check(row_is(11u, CLI_RUNS_ROW_BLOB, "panic", &row) && (row.priority == QRY_PANIC),
              "the panic's record carries its priority");
    cli_check(row_is(12u, CLI_RUNS_ROW_RUN, "b", &row) && !row.open, "the second run follows the detail");
    cli_check(row_is(13u, CLI_RUNS_ROW_LESSONS, NULL, &row), "the lessons follow the folder's runs");

    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    const int rows_top = EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = table, .frame = &frame);
    cli_check(rows_top == frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PANE_HEAD,
              "the rows stand under the pane's head alone");
    const GuiRect first =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 0u);
    unsigned int hit_row = 0u;
    cli_check(EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .x = first.x + 20,
                         .y = first.y + GUI_PANE_ROW + 3, .row_pointer = &hit_row) &&
                  (hit_row == 1u),
              "a press on the second row lands on the run");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Runs", .tree = "");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 1u, .open_out = &open, .root = s_root) &&
                  (EMBED_CALL(cli_runs_pane.rows, CliRunsPaneCfg, .at = 0u) == 6u),
              "the run's row pressed closes it and opens no file");
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 0u, .open_out = &open, .root = s_root) &&
                  (EMBED_CALL(cli_runs_pane.rows, CliRunsPaneCfg, .at = 0u) == 6u),
              "a folder's row pressed changes nothing");

    cli_check(EMBED_CALL(cli_runs_pane.step, CliRunsPaneCfg, .root = s_empty, .now_ms = 4000ULL, .shown = 1) &&
                  (EMBED_CALL(cli_runs_pane.rows, CliRunsPaneCfg, .at = 0u) == 1u) &&
                  row_is(0u, CLI_RUNS_ROW_EMPTY, NULL, &row),
              "another tree with no run lists one row that reads so");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Runs", .tree = "");
    cli_check(EMBED_CALL(cli_runs_pane.step, CliRunsPaneCfg, .root = NULL, .now_ms = 5000ULL, .shown = 0) &&
                  (EMBED_CALL(cli_runs_pane.rows, CliRunsPaneCfg, .at = 0u) == 0u),
              "no tree leaves no rows");

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
