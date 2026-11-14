// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_test.c: the Git pane's check: a repository with two commits lists the branch's row with the commits counted and a
// row a commit, the newest first, with its subject, its day, the branch's name at HEAD and its strokes; a commit's row
// pressed opens the files it changed under it with their states and folders and closes them again; text typed rests
// before the history is searched, then lists what the query finds; the authors setting shows each commit's author and
// searches by it; the pane's table is the field under the head, a press on the field takes the keys, which typed
// text then reaches and Escape lets go, the branch's row is taller than a commit's, and a file's row pressed opens the
// file
#include "../../../../../includes/window/block.h"
#include "../../../../../ui/cli/src/code/git.h"
#include "../../../../../ui/cli/src/command_line/commands.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "../../../../../ui/gui/src/git.h"
#include "../cli/cli_check.h"

static char s_root[CLI_PATH_MAX_LEN];

// the file `relative` of the tree written with `text`, its folder put in place first
static void file_written(const char *relative, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = relative, .joined = path);
    // the place past the path's last slash, 0 where it has none
    const unsigned long long past_slash = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                     .text = path,
                                                     .at = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = sizeof(path)}),
                                                     .set = "/");
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

// every file of the tree committed as `message`, the committer set to the check's own and the commit unsigned
static void committed(const char *message)
{
    char out_text[4096];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea out = {out_text, sizeof(out_text), 0ULL};
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const char *const added[7] = {"-c", "user.name=Check", "-c", "user.email=check@orior.test", "add", "-A", "."};
    cli_check(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root, .out = &out,
                         .count = 7u, .err_msg = &err_msg, .request = added),
              "the files are added");
    out.at = 0ULL;
    const char *const commit[10] = {"-c",     "user.name=Check",
                                    "-c",     "user.email=check@orior.test",
                                    "-c",     "commit.gpgsign=false",
                                    "commit", "-q",
                                    "-m",     message};
    cli_check(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root, .out = &out,
                         .count = 10u, .err_msg = &err_msg, .request = commit),
              message);
}

// the row `at`, into `row`: whether it is of `type` with the text `text`
static int row_is(unsigned int at, unsigned int type, const char *text, CliGitLogRow *row)
{
    if (!EMBED_CALL(cli_git_log.row, CliGitLogCfg, .at = at, .row = row) || (row->type != type))
    {
        return 0;
    }
    return (row->text_length == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})) &&
           (memor.cmp(&(MemoriaCfg){.src = row->text, .other = text, .bytes = row->text_length}) == 0);
}

// whether the row's second text reads `text`
static int second_is(const CliGitLogRow *row, const char *text)
{
    return (row->second_length == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})) &&
           (memor.cmp(&(MemoriaCfg){.src = row->second, .other = text, .bytes = row->second_length}) == 0);
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

    cli_check_folder("git_tree", s_root);
    char out_text[4096];
    VerbaLinea out = {out_text, sizeof(out_text), 0ULL};
    cli_check(EMBED_CALL(cli_git_out.init, CliGitOutCfg, .folder_read = s_root, .out = &out, .err_msg = &why),
              "the tree is a repository");
    file_written("a.txt", "one\n");
    file_written("sub/b.txt", "two\n");
    committed("first commit");
    file_written("a.txt", "one two\n");
    committed("second commit");
    char branch[CLI_NAME_MAX_LEN];
    cli_check(EMBED_CALL(cli_git_table.branch, CliGitCfg, .root = s_root, .name = branch), "the tree is on a branch");

    const GuiPaneTable *const table = &gui_git_table;
    cli_check(EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "git") == table, "the Git tool has the table");
    EMBED_CALL(cli_git_log.begin, CliGitLogCfg, .woken = NULL, .entries = NULL);
    cli_check(EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 0u, "nothing is listed before the pane shows");
    cli_check(!EMBED_CALL(cli_git_log.authors, CliGitLogCfg, .at = 0u), "authors are off until turned on");
    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_DISPLAYED, .root = s_root);
    cli_check(EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 3u,
              "the pane displayed lists the branch's row and a row a commit");
    CliGitLogRow row;
    cli_check(row_is(0u, CLI_GIT_LOG_ROW_BRANCH, branch, &row) && second_is(&row, "2 commits displayed"),
              "the branch's row carries the branch's name and the commits counted");
    cli_check(row_is(1u, CLI_GIT_LOG_ROW_COMMIT, "second commit", &row) && !row.open && !row.merge &&
                  (row.lane_at == 0u) && (row.across == 1u) && (row.second_length == 0ULL) && (row.refs >= 1u),
              "the newest commit is first in the first column, closed, with no author displayed and a name at it");
    cli_check((cellul.len(&(CatenaFinitaCfg){.src = row.day, .cap = CLI_GIT_LOG_DAY}) == CLI_GIT_LOG_DAY - 1u) && (row.day[4] == '-') && (row.day[7] == '-') &&
                  (row.day[0] == '2'),
              "the commit's day reads as a year, a month and a day");
    cli_check((row.strokes == 1u) && (row.stroke[0][0] == 0u) && (row.stroke[0][1] == 0u) && (row.stroke[0][2] == 1u),
              "the newest commit's one stroke runs down its column from the middle to its parent");
    CliGitLogRef ref;
    cli_check(EMBED_CALL(cli_git_log.ref, CliGitLogCfg, .at = 1u, .which = 0u, .ref = &ref) && ref.head && !ref.tag &&
                  (ref.length == cellul.len(&(CatenaFinitaCfg){.src = branch, .cap = sizeof(branch)})) &&
                  (memor.cmp(&(MemoriaCfg){.src = ref.name, .other = branch, .bytes = ref.length}) == 0),
              "the branch's name is at the newest commit with HEAD");
    cli_check(!EMBED_CALL(cli_git_log.ref, CliGitLogCfg, .at = 1u, .which = row.refs, .ref = &ref),
              "no name is past the commit's last");
    cli_check(row_is(2u, CLI_GIT_LOG_ROW_COMMIT, "first commit", &row) && (row.refs == 0u) && (row.strokes == 1u) &&
                  (row.stroke[0][2] == 0u),
              "the first commit is last, with no name and the stroke from its child ending at its dot");
    cli_check(!EMBED_CALL(cli_git_log.row, CliGitLogCfg, .at = 3u, .row = &row), "no row is past the last");

    EMBED_CALL(cli_git_log.toggle, CliGitLogCfg, .at = 1u, .root = s_root);
    cli_check((EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 4u) &&
                  row_is(1u, CLI_GIT_LOG_ROW_COMMIT, "second commit", &row) && row.open,
              "the newest commit opened lists the one file it changed under it");
    cli_check(row_is(2u, CLI_GIT_LOG_ROW_FILE, "a.txt", &row) && (row.state == 'M') && (row.second_length == 0ULL) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = row.path, .cap = CLI_POOL_BYTES, .other = "a.txt", .other_cap = 6u}),
              "the file changed carries its name, state and path, in no folder");
    EMBED_CALL(cli_git_log.toggle, CliGitLogCfg, .at = 3u, .root = s_root);
    cli_check(EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 6u,
              "the first commit opened lists the two files it added");
    cli_check(row_is(4u, CLI_GIT_LOG_ROW_FILE, "a.txt", &row) && (row.state == 'A'),
              "the first file added carries its state");
    cli_check(row_is(5u, CLI_GIT_LOG_ROW_FILE, "b.txt", &row) && (row.state == 'A') && (row.second_length == 3ULL) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = row.path, .cap = CLI_POOL_BYTES, .other = "sub/b.txt", .other_cap = 10u}),
              "the deeper file added carries its folder");
    EMBED_CALL(cli_git_log.toggle, CliGitLogCfg, .at = 1u, .root = s_root);
    cli_check((EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 5u) &&
                  row_is(2u, CLI_GIT_LOG_ROW_COMMIT, "first commit", &row) && row.open,
              "the newest commit closed hides its file, the first commit's files located");

    cli_check(EMBED_CALL(cli_git_log.deadline_ms, CliGitLogCfg, .at = 0u) == 0ULL, "no change waits before typing");
    EMBED_CALL(cli_git_log.type, CliGitLogCfg, .text = "second", .size = 6ULL, .now_ms = 1000ULL);
    char query[CLI_GIT_LOG_QUERY_MAX];
    const unsigned long long length = EMBED_CALL(cli_git_log.query, CliGitLogCfg, .text_out = query);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = query, .cap = CLI_POOL_BYTES, .other = "second", .other_cap = 7u}) &&
                  (length == 6ULL),
              "the text typed is the query");
    cli_check(EMBED_CALL(cli_git_log.deadline_ms, CliGitLogCfg, .at = 0u) == 1000ULL + CLI_GIT_LOG_REST_MS,
              "the search waits the rest after the typing");
    cli_check(!EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = s_root, .now_ms = 1100ULL, .on = 1),
              "a step inside the rest reads nothing");
    cli_check(!EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = s_root, .now_ms = 1300ULL, .on = 0),
              "a step while the pane is out of sight reads nothing");
    cli_check(EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = s_root, .now_ms = 1300ULL, .on = 1),
              "the step at the rest's end searches the history");
    cli_check((EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 2u) &&
                  row_is(0u, CLI_GIT_LOG_ROW_BRANCH, branch, &row) && second_is(&row, "1 commit found") &&
                  row_is(1u, CLI_GIT_LOG_ROW_COMMIT, "second commit", &row),
              "the query finds the one commit whose subject contains it, counted as found");
    for (unsigned int erased = 0u; erased < 6u; erased += 1u)
    {
        cli_check(EMBED_CALL(cli_git_log.key, CliGitLogCfg, .name = "Backspace", .now_ms = 2000ULL),
                  "Backspace erases a character");
    }
    cli_check(EMBED_CALL(cli_git_log.key, CliGitLogCfg, .name = "Enter", .now_ms = 2000ULL) &&
                  EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = s_root, .now_ms = 2000ULL, .on = 1) &&
                  (EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 5u),
              "the query erased and Enter pressed lists the graph again without the rest, the first commit's files "
              "located");
    cli_check(!EMBED_CALL(cli_git_log.key, CliGitLogCfg, .name = "Tab", .now_ms = 2000ULL),
              "another key goes elsewhere");

    EMBED_CALL(cli_git_log.authors_set, CliGitLogCfg, .on = 1, .now_ms = 3000ULL);
    cli_check(EMBED_CALL(cli_git_log.authors, CliGitLogCfg, .at = 0u) &&
                  EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = s_root, .now_ms = 3000ULL, .on = 1) &&
                  row_is(1u, CLI_GIT_LOG_ROW_COMMIT, "second commit", &row) && second_is(&row, "Check"),
              "authors turned on reads the graph again and shows each commit's author");
    EMBED_CALL(cli_git_log.type, CliGitLogCfg, .text = "check", .size = 5ULL, .now_ms = 4000ULL);
    cli_check(
        EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = s_root, .now_ms = 4000ULL + CLI_GIT_LOG_REST_MS, .on = 1) &&
            (EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 5u) &&
            row_is(0u, CLI_GIT_LOG_ROW_BRANCH, branch, &row) && second_is(&row, "2 commits found"),
        "a query searches the authors while they show");
    EMBED_CALL(cli_git_log.key, CliGitLogCfg, .name = "Backspace", .now_ms = 5000ULL);
    for (unsigned int erased = 0u; erased < 4u; erased += 1u)
    {
        EMBED_CALL(cli_git_log.key, CliGitLogCfg, .name = "Backspace", .now_ms = 5000ULL);
    }
    EMBED_CALL(cli_git_log.authors_set, CliGitLogCfg, .on = 0, .now_ms = 5000ULL);
    cli_check(EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = s_root, .now_ms = 5000ULL, .on = 1) &&
                  row_is(1u, CLI_GIT_LOG_ROW_COMMIT, "second commit", &row) && (row.second_length == 0ULL),
              "authors turned off hides the authors");
    EMBED_CALL(cli_git_log.stale, CliGitLogCfg, .at = 0u);
    cli_check(EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = s_root, .now_ms = 5001ULL, .on = 1) &&
                  !EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = s_root, .now_ms = 5002ULL, .on = 1),
              "the graph marked stale is read again once");
    unsigned int repos = 0u;
    cli_check((EMBED_CALL(cli_git_log.repo, CliGitLogCfg, .count_out = &repos) != NULL) && (repos == 1u),
              "the tree has one repository");

    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    const int rows_top = EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = table, .frame = &frame);
    cli_check(rows_top == frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PANE_HEAD + GUI_PANE_BOX,
              "the rows are under the pane's head and the field, no repository choice for one repository");
    const int field_y = frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PANE_HEAD + (GUI_PANE_BOX / 2);
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = frame.side.x + GUI_PANE_PAD_LEFT + 20,
                         .y = field_y) == CLI_GIT_LOG_FIELD,
              "a press in the field lands on it");
    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane, .which = CLI_GIT_LOG_FIELD, .root = s_root);
    cli_check(pane.keys, "the field pressed takes the keys");
    cli_check(EMBED_CALL(gui_pane.text, GuiPaneCfg, .table = table, .pane = &pane, .text = "first", .length = 5ULL,
                         .root = s_root) &&
                  (EMBED_CALL(cli_git_log.query, CliGitLogCfg, .text_out = query) == 5ULL) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = query, .cap = CLI_POOL_BYTES, .other = "first", .other_cap = 6u}),
              "text typed while the field has the keys reaches the query");
    cli_check(EMBED_CALL(gui_pane.key, GuiPaneCfg, .table = table, .pane = &pane, .name = "Escape", .root = s_root) &&
                  !pane.keys,
              "Escape lets the keys go");
    cli_check(!EMBED_CALL(gui_pane.text, GuiPaneCfg, .table = table, .pane = &pane, .text = "x", .length = 1ULL,
                          .root = s_root),
              "text typed without the keys goes elsewhere");
    // the menus: a commit's rows open or close its files and copy its id and message, a file's row copies its path
    cli_check(row_is(1u, CLI_GIT_LOG_ROW_COMMIT, "second commit", &row) && !row.open, "the newest commit is closed");
    char id[CLI_NAME_MAX_LEN];
    VerbaLinea id_line = {id, sizeof(id), 0ULL};
    mmgr_line_put(&id_line, row.id);
    (void)mmgr_line_finish(&id_line);
    unsigned int built = 0u;
    const CliItem *rows = NULL;
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = table, .row = 1u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) && (built == 4u) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].label, .cap = CLI_POOL_BYTES, .other = "Open Files", .other_cap = 11u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].command, .cap = CLI_POOL_BYTES, .other = "commit-files", .other_cap = 13u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].args, .cap = CLI_POOL_BYTES, .other = id, .other_cap = sizeof(id)}) &&
                  rows[1].line &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[2].command, .cap = CLI_POOL_BYTES, .other = "commit-copy-id", .other_cap = 15u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[3].command, .cap = CLI_POOL_BYTES, .other = "commit-copy-message", .other_cap = 20u}),
              "a closed commit's menu opens its files and copies its id and message");
    cli_check(EMBED_CALL(cli_git_log.toggle_id, CliGitLogCfg, .name = id, .root = s_root) &&
                  row_is(1u, CLI_GIT_LOG_ROW_COMMIT, "second commit", &row) && row.open &&
                  EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = table, .row = 1u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].label, .cap = CLI_POOL_BYTES, .other = "Close Files", .other_cap = 12u}),
              "the commit opened by its id has its menu close its files");
    cli_check(!EMBED_CALL(cli_git_log.toggle_id, CliGitLogCfg, .name = "0000000", .root = s_root),
              "an id the graph has not opens nothing");
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = table, .row = 2u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) && (built == 1u) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].command, .cap = CLI_POOL_BYTES, .other = "file-copy-path", .other_cap = 15u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].args, .cap = CLI_POOL_BYTES, .other = "a.txt", .other_cap = 6u}),
              "a file's menu copies its path");
    cli_check(!EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = table, .row = 0u, .root = s_root),
              "the branch's row has no menu");
    char message_text[1024];
    VerbaLinea message = {message_text, sizeof(message_text), 0ULL};
    cli_check(EMBED_CALL(cli_git_log.message, CliGitLogCfg, .name = id, .out = &message, .root = s_root) &&
                  mmgr_line_finish(&message) &&
                  cellul.starts(&(CatenaFinitaCfg){.src = message_text,
                                                   .cap = sizeof(message_text),
                                                   .other = "second commit",
                                                   .other_cap = 14u}),
              "the commit's message is read by its id");
    EMBED_CALL(cli_git_log.toggle, CliGitLogCfg, .at = 1u, .root = s_root);
    const GuiRect first =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 0u);
    const GuiRect second =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 1u);
    cli_check((first.height == GUI_GIT_BRANCH_ROW) && (second.height == GUI_PANE_ROW) &&
                  (second.y == first.y + first.height),
              "the branch's row is taller than a commit's, which is under it");
    cli_check(EMBED_CALL(gui_pane.move, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame, .glyphs = &glyphs,
                         .x = second.x + 20, .y = second.y + 3) &&
                  (pane.pointed == 1),
              "the pointer on the commit's row marks it");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Git", .tree = "");
    const GuiColor on_first = raster.pixels[((unsigned int)(first.y + 2) * raster.width) + (unsigned int)first.x + 2u];
    const GuiColor on_second =
        raster.pixels[((unsigned int)(second.y + 2) * raster.width) + (unsigned int)second.x + 2u];
    cli_check(on_first != on_second, "the row pointed is drawn on a ground of its own");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 2u, .open_out = &open, .root = s_root) &&
                  (EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 3u),
              "the first commit's row pressed closes its files and opens no file");
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 1u, .open_out = &open, .root = s_root) &&
                  EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 2u, .open_out = &open, .root = s_root) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = open.path, .cap = CLI_POOL_BYTES, .other = "a.txt", .other_cap = 6u}) &&
                  !open.placed,
              "the newest commit's row pressed opens its file's row, which pressed opens the file");
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 0u, .open_out = &open, .root = s_root),
              "the branch's row pressed opens nothing");

    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_DISPLAYED, .root = NULL);
    cli_check(EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u) == 0u, "no tree leaves no rows");

    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(gui_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
