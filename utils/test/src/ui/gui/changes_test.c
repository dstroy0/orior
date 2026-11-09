// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes_test.c: the Changes pane's check: a repository with two files git does not yet track lists them as rows,
// each not yet tracked, the deeper one with its folder; the pane's table draws the rows under the Commit group's title
// on the side, the row pointed on a ground of its own; a press lands on a row and opens its file; the table reads on the
// pane's showing and a save and not on the view's change; the next commit takes the files whose boxes are lit; the
// bar pushes, pulls and rolls back against a remote of the check's own, its tools and the all box found under the
// pointer; and no tree leaves no rows
#include "../../../../../ui/cli/src/code/git.h"
#include "../../../../../ui/cli/src/command_line/commands.h"
#include "../../../../../ui/cli/src/view/parts/changes.h"
#include "../cli/cli_check.h"

// the row whose name is `name`, into `row`: 1, or 0 where none is
static int row_with_name(const char *name, CliChangedRow *row)
{
    for (unsigned int at = 0u; EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = at, .row = row); at += 1u)
    {
        if ((row->type == CLI_CHANGES_ROW_FILE) &&
            cellul.eq(&(CatenaFinitaCfg){
                .src = name, .cap = CLI_POOL_BYTES, .other = row->name, .other_cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

// the row of the file `name`, past the last where none is
static unsigned int row_of(const char *name)
{
    unsigned int at = 0u;
    for (CliChangedRow each; EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = at, .row = &each) &&
                             ((each.type != CLI_CHANGES_ROW_FILE) ||
                              !cellul.eq(&(CatenaFinitaCfg){
                                  .src = each.name, .cap = CLI_POOL_BYTES, .other = name, .other_cap = CLI_POOL_BYTES}));)
    {
        at += 1u;
    }
    return at;
}

// whether the line row `at` is `gone` and reads `text`
static int line_row_is(unsigned int at, int gone, const char *text)
{
    CliChangedRow row;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = at, .row = &row) || (row.type != CLI_CHANGES_ROW_LINE) ||
        (row.gone != gone) || (row.count != cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})))
    {
        return 0;
    }
    for (unsigned long long unit = 0ULL; unit < row.count; unit += 1ULL)
    {
        if (row.units[unit] != (unsigned short)(unsigned char)text[unit])
        {
            return 0;
        }
    }
    return 1;
}

// the text of `relative` as the last commit has it, into `text`
static void head_read(const char *root, const char *relative, char *text, unsigned long long size)
{
    VerbaLinea out = {text, size, 0ULL};
    (void)EMBED_CALL(cli_git_root_file_stage.head_text, CliGitRootFileStageCfg, .root = root, .file = relative,
                     .out = &out);
    (void)mmgr_line_finish(&out);
}

// git run at `root` with `request`, `count` words: 1, or 0 with why as the check's failure
static int git_ran(const char *root, const char *const *request, unsigned int count, const char *why_not)
{
    char out_text[4096];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea out = {out_text, sizeof(out_text), 0ULL};
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int ran = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = root,
                               .out = &out, .count = count, .err_msg = &err_msg, .request = request);
    cli_check(ran, why_not);
    return ran;
}

// the subject of the last commit of the repository at `root` into `subject`, sized for 256 bytes
static void last_subject(const char *root, char *subject)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea out = {subject, 256ULL, 0ULL};
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const char *const log[3] = {"log", "-1", "--format=%s"};
    (void)EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = root, .out = &out,
                     .count = 3u, .err_msg = &err_msg, .request = log);
    (void)mmgr_line_finish(&out);
    const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = subject, .cap = 256ULL, .byte = '\n'});
    if (end != NULL)
    {
        subject[end - subject] = '\0';
    }
}

// the file `relative` of the tree at `root` written with `text`
static void file_written(const char *root, const char *relative, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = relative, .joined = path);
    cli_check(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                         .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})),
              relative);
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
    cli_check_folder("changes_tree", s_root);
    char out_text[1024];
    VerbaLinea out = {out_text, sizeof(out_text), 0ull};
    cli_check(EMBED_CALL(cli_git_out.init, CliGitOutCfg, .folder_read = s_root, .out = &out, .err_msg = &why),
              "a repository stands in the check's folder");
    file_written(s_root, "fresh.txt", "one\n");
    char sub[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "sub", .joined = sub);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = sub);
    file_written(s_root, "sub/deep.txt", "two\n");

    const GuiPaneTable *const table = &gui_changes_table;
    cli_check(EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "commit") == table, "the Commit tool has the table");
    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_VIEW, .root = s_root);
    cli_check(EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 0u, "the view's change reads no changes");
    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_SHOWN, .root = s_root);
    cli_check(EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 2u,
              "the pane shown reads the two files not yet tracked as rows");
    CliChangedRow fresh;
    CliChangedRow deep;
    cli_check(row_with_name("fresh.txt", &fresh) && (fresh.state == 'U') && (fresh.folder_length == 0ULL),
              "the file at the top is not yet tracked and stands in no folder");
    cli_check(row_with_name("deep.txt", &deep) && (deep.state == 'U') && (deep.folder_length == 3ULL) &&
                  cellul.starts(&(CatenaFinitaCfg){
                      .src = deep.path, .cap = CLI_PATH_MAX_LEN, .other = "sub", .other_cap = 4u}),
              "the deeper file carries its folder");
    cli_check(!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = 2u, .row = &fresh), "no row stands past the last");

    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    const GuiRect first =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 0u);
    const GuiRect second =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 1u);
    cli_check(EMBED_CALL(gui_pane.move, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame, .glyphs = &glyphs,
                         .x = first.x + 20, .y = first.y + 3) &&
                  (pane.pointed == 0),
              "the pointer on the first row marks it");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Commit", .tree = "");
    const GuiColor on_first = raster.pixels[((unsigned int)(first.y + 2) * raster.width) + (unsigned int)first.x + 2u];
    const GuiColor on_second =
        raster.pixels[((unsigned int)(second.y + 2) * raster.width) + (unsigned int)second.x + 2u];
    cli_check(on_first != on_second, "the row pointed is drawn on a ground of its own");
    unsigned int hit_row = 0u;
    cli_check(EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .x = second.x + 20, .y = second.y + (second.height / 2), .row_pointer = &hit_row) &&
                  (hit_row == 1u),
              "a press on the second row lands on it");
    cli_check(!EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                          .x = second.x + 20, .y = second.y + second.height + 4, .row_pointer = &hit_row),
              "a press under the last row lands on none");
    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    CliChangedRow pressed;
    cli_check(EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = 1u, .row = &pressed) &&
                  EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 1u, .open_out = &open) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = open.path, .cap = CLI_POOL_BYTES, .other = pressed.path, .other_cap = CLI_POOL_BYTES}) &&
                  !open.placed,
              "a row pressed opens its file where the path's own place puts it");
    cli_check(!EMBED_CALL(gui_pane.wheel, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame, .rows = 2),
              "two rows do not scroll");
    unsigned int built = 0u;
    const CliItem *rows = NULL;
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = table, .row = 1u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) && (built == 3u) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].label, .cap = CLI_POOL_BYTES, .other = "Open File", .other_cap = 10u}) &&
                  (rows[0].needs[0] == '\0') &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].args, .cap = CLI_POOL_BYTES, .other = pressed.path, .other_cap = CLI_POOL_BYTES}) &&
                  rows[1].line &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[2].command, .cap = CLI_POOL_BYTES, .other = "file-copy-path", .other_cap = 15u}),
              "a changed file's menu opens it and copies its path");
    cli_check(!EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = table, .row = 2u, .root = s_root),
              "no menu stands past the last row");

    // the next commit: refused without a message, the message typed, the deeper file left out through its box, and
    // the commit taking the other file
    out.at = 0ULL;
    why.at = 0ULL;
    const char *const name_set[3] = {"config", "user.name", "Check"};
    const char *const email_set[3] = {"config", "user.email", "check@orior.test"};
    cli_check(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root, .out = &out,
                         .count = 3u, .err_msg = &why, .request = name_set) &&
                  EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root,
                             .out = &out, .count = 3u, .err_msg = &why, .request = email_set),
              "the repository has a committer");
    unsigned long long length = 0ULL;
    cli_check(!EMBED_CALL(cli_changes.commit, CliGitChangesCfg, .root = s_root) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .length_out = &length),
                      .cap = CLI_POOL_BYTES,
                      .other = "The commit message is empty.",
                      .other_cap = 29u}),
              "a commit with no message is refused with why");
    EMBED_CALL(cli_changes.type, CliGitChangesCfg, .text = "first", .size = 5ULL);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = EMBED_CALL(cli_changes.message, CliGitChangesCfg, .length_out = &length),
                  .cap = CLI_POOL_BYTES,
                  .other = "first",
                  .other_cap = 6u}) &&
                  (length == 5ULL),
              "the text typed is the message");
    unsigned int deep_row = 0u;
    for (CliChangedRow each; EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = deep_row, .row = &each) &&
                             !cellul.eq(&(CatenaFinitaCfg){
                                 .src = each.name, .cap = CLI_POOL_BYTES, .other = "deep.txt", .other_cap = 9u});)
    {
        deep_row += 1u;
    }
    const GuiRect deep_rect =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = deep_row);
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane,
                          .row = deep_row, .x = deep_rect.x + 10, .open_out = &open) &&
                  row_with_name("deep.txt", &deep) && !deep.taken && row_with_name("fresh.txt", &fresh) && fresh.taken,
              "a press on the deeper file's box leaves it out of the commit, the other file taken");
    const int committed = EMBED_CALL(cli_changes.commit, CliGitChangesCfg, .root = s_root);
    cli_check(committed, committed ? "Commit commits the file taken"
                                   : EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .length_out = &length));
    cli_check((EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 1u) && row_with_name("deep.txt", &deep) &&
                  !deep.taken,
              "the file left out stands alone in the list, left out still");
    cli_check((EMBED_CALL(cli_changes.message, CliGitChangesCfg, .length_out = &length) != NULL) && (length == 0ULL),
              "the commit clears the message");
    cli_check((EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .length_out = &length) != NULL) && (length == 0ULL),
              "a commit taken leaves no refusal");

    // the bar: the branch follows no remote until one is set and pushed to; a commit without a push stands ahead and
    // Push sends it; Commit and Push leaves nothing ahead; a commit pushed from elsewhere stands behind and Pull brings
    // it; the files taken roll back on a second press inside the window and arm again past it; and the all box leaves
    // every file out or takes every file
    unsigned int ahead = 9u;
    unsigned int behind = 9u;
    cli_check(!EMBED_CALL(cli_changes.sync, CliGitChangesCfg, .ahead = &ahead, .behind = &behind) && (ahead == 0u) &&
                  (behind == 0u),
              "the branch follows no remote before one is set");
    static char s_remote[CLI_PATH_MAX_LEN];
    cli_check_folder("changes_remote", s_remote);
    const char *const bare[2] = {"init", "--bare"};
    const char *const origin[4] = {"remote", "add", "origin", s_remote};
    const char *const upstream[5] = {"push", "-q", "-u", "origin", "HEAD"};
    (void)git_ran(s_remote, bare, 2u, "the remote is a bare repository");
    (void)git_ran(s_root, origin, 4u, "the tree adds the remote origin");
    (void)git_ran(s_root, upstream, 5u, "the branch is pushed to the remote and follows it");
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = s_root);
    cli_check(EMBED_CALL(cli_changes.sync, CliGitChangesCfg, .ahead = &ahead, .behind = &behind) && (ahead == 0u) &&
                  (behind == 0u),
              "the branch follows the remote, level with it");
    file_written(s_root, "fresh.txt", "one more\n");
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = s_root);
    EMBED_CALL(cli_changes.type, CliGitChangesCfg, .text = "second", .size = 6ULL);
    cli_check(EMBED_CALL(cli_changes.commit, CliGitChangesCfg, .root = s_root) &&
                  EMBED_CALL(cli_changes.sync, CliGitChangesCfg, .ahead = &ahead, .behind = &behind) && (ahead == 1u),
              "a commit without a push stands one ahead of the remote");
    cli_check(EMBED_CALL(cli_changes.push, CliGitChangesCfg, .root = s_root) &&
                  EMBED_CALL(cli_changes.sync, CliGitChangesCfg, .ahead = &ahead, .behind = &behind) && (ahead == 0u),
              "Push sends the commit, the branch level again");
    char subject[256];
    last_subject(s_remote, subject);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = subject, .cap = 256u, .other = "second", .other_cap = 7u}),
              "the remote's last commit is the one pushed");
    file_written(s_root, "fresh.txt", "one more again\n");
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = s_root);
    EMBED_CALL(cli_changes.type, CliGitChangesCfg, .text = "third", .size = 5ULL);
    cli_check(EMBED_CALL(cli_changes.commit, CliGitChangesCfg, .root = s_root, .push = 1) &&
                  EMBED_CALL(cli_changes.sync, CliGitChangesCfg, .ahead = &ahead, .behind = &behind) && (ahead == 0u),
              "Commit and Push leaves nothing ahead");
    last_subject(s_remote, subject);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = subject, .cap = 256u, .other = "third", .other_cap = 6u}),
              "the remote's last commit is the one committed and pushed");
    static char s_other[CLI_PATH_MAX_LEN];
    cli_check_folder("changes_other", s_other);
    const char *const cloned[4] = {"clone", "-q", s_remote, "."};
    const char *const added[7] = {"-c", "user.name=Check", "-c", "user.email=check@orior.test", "add", "-A", "."};
    const char *const elsewhere[10] = {"-c",     "user.name=Check",
                                       "-c",     "user.email=check@orior.test",
                                       "-c",     "commit.gpgsign=false",
                                       "commit", "-q",
                                       "-m",     "elsewhere"};
    const char *const sent[2] = {"push", "-q"};
    (void)git_ran(s_other, cloned, 4u, "the remote is cloned elsewhere");
    file_written(s_other, "fresh.txt", "from elsewhere\n");
    (void)git_ran(s_other, added, 7u, "the clone's change is added");
    (void)git_ran(s_other, elsewhere, 10u, "the clone commits");
    (void)git_ran(s_other, sent, 2u, "the clone pushes");
    const char *const fetched[2] = {"fetch", "-q"};
    (void)git_ran(s_root, fetched, 2u, "the tree fetches the remote");
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = s_root);
    cli_check(EMBED_CALL(cli_changes.sync, CliGitChangesCfg, .ahead = &ahead, .behind = &behind) && (behind == 1u),
              "the commit from elsewhere stands one behind");
    cli_check(EMBED_CALL(cli_changes.pull, CliGitChangesCfg, .root = s_root) &&
                  EMBED_CALL(cli_changes.sync, CliGitChangesCfg, .ahead = &ahead, .behind = &behind) && (behind == 0u),
              "Pull brings the commit from elsewhere");
    last_subject(s_root, subject);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = subject, .cap = 256u, .other = "elsewhere", .other_cap = 10u}),
              "the tree's last commit is the one pulled");

    file_written(s_root, "fresh.txt", "rolled\n");
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = s_root);
    cli_check((EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 2u) &&
                  (EMBED_CALL(cli_changes.taken, CliGitChangesCfg, .at = 0u) == 1u),
              "the changed file is taken, the file left out before standing left out");
    cli_check(!EMBED_CALL(cli_changes.rollback, CliGitChangesCfg, .root = s_root, .now_ms = 1000ULL),
              "the first press arms the roll back");
    cli_check(EMBED_CALL(cli_changes.rollback, CliGitChangesCfg, .root = s_root, .now_ms = 2000ULL),
              "the second press inside the window rolls back");
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = s_root);
    cli_check((EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 1u) && row_with_name("deep.txt", &deep),
              "the file rolled back stands as the last commit left it, the file left out standing");
    file_written(s_root, "fresh.txt", "rolled again\n");
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = s_root);
    cli_check(!EMBED_CALL(cli_changes.rollback, CliGitChangesCfg, .root = s_root, .now_ms = 10000ULL) &&
                  !EMBED_CALL(cli_changes.rollback, CliGitChangesCfg, .root = s_root, .now_ms = 20000ULL),
              "a second press past the window arms again");
    EMBED_CALL(cli_changes.take_all, CliGitChangesCfg, .on = 0);
    cli_check((EMBED_CALL(cli_changes.taken, CliGitChangesCfg, .at = 0u) == 0u) && row_with_name("fresh.txt", &fresh) &&
                  !fresh.taken,
              "the all box off leaves every file out");
    EMBED_CALL(cli_changes.take_all, CliGitChangesCfg, .on = 1);
    cli_check((EMBED_CALL(cli_changes.taken, CliGitChangesCfg, .at = 0u) == 2u) && row_with_name("deep.txt", &deep) &&
                  deep.taken,
              "the all box on takes every file");
    EMBED_CALL(cli_changes.rollback, CliGitChangesCfg, .root = s_root, .now_ms = 30000ULL);
    cli_check(EMBED_CALL(cli_changes.rollback, CliGitChangesCfg, .root = s_root, .now_ms = 31000ULL),
              "the roll back of a file the last commit does not contain goes through");
    unsigned long long refused = 0ULL;
    cli_check((cellul.len(&(CatenaFinitaCfg){
                   .src = EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .length_out = &refused),
                   .cap = CLI_POOL_BYTES}) == refused) &&
                  (refused != 0ULL),
              "the file the last commit does not contain is refused with why");

    // a file's changes opened under its row: a line changed and a line put in are one change of three lines; the line
    // put in left out commits the file in part, the other file whole; the change left stands alone after; the arrow
    // pressed closes the changes; and a change's row pressed opens the file at its first line
    file_written(s_root, "fresh.txt", "changed\nput in\n");
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = s_root);
    const unsigned int fresh_row = row_of("fresh.txt");
    cli_check(row_with_name("fresh.txt", &fresh) && fresh.opens && !fresh.open && row_with_name("deep.txt", &deep) &&
                  deep.opens,
              "a changed file's changes can open, and are not yet");
    cli_check(EMBED_CALL(cli_changes.open, CliGitChangesCfg, .root = s_root, .at = fresh_row) &&
                  (EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 6u) &&
                  (EMBED_CALL(cli_changes.files, CliGitChangesCfg, .at = 0u) == 2u) &&
                  row_with_name("fresh.txt", &fresh) && fresh.open,
              "the file's changes open under its row, one change and its three lines");
    CliChangedRow hunk;
    cli_check(EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = fresh_row + 1u, .row = &hunk) &&
                  (hunk.type == CLI_CHANGES_ROW_HUNK) && (hunk.from == 0u) && (hunk.to == 2u) && hunk.taken &&
                  !hunk.part,
              "the change covers the newer text's first two lines, taken whole");
    cli_check(line_row_is(fresh_row + 2u, 1, "from elsewhere") && line_row_is(fresh_row + 3u, 0, "changed") &&
                  line_row_is(fresh_row + 4u, 0, "put in"),
              "the lines stand in order: the line taken out, then the two put in");
    EMBED_CALL(cli_changes.toggle, CliGitChangesCfg, .at = fresh_row + 4u);
    CliChangedRow line;
    cli_check(EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = fresh_row + 4u, .row = &line) && !line.taken &&
                  EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = fresh_row + 1u, .row = &hunk) && hunk.taken &&
                  hunk.part && row_with_name("fresh.txt", &fresh) && fresh.taken && fresh.part,
              "a line left out marks its change and its file as taken in part");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Commit", .tree = s_root);
    EMBED_CALL(cli_changes.type, CliGitChangesCfg, .text = "part", .size = 4ULL);
    const int part_committed = EMBED_CALL(cli_changes.commit, CliGitChangesCfg, .root = s_root);
    cli_check(part_committed, part_committed
                                  ? "the commit takes the file in part and the other whole"
                                  : EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .length_out = &length));
    char head_text[64];
    head_read(s_root, "fresh.txt", head_text, sizeof(head_text));
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = head_text, .cap = sizeof(head_text), .other = "changed\n", .other_cap = 9u}),
              "the last commit has the file with the line left out missing");
    head_read(s_root, "sub/deep.txt", head_text, sizeof(head_text));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = head_text, .cap = sizeof(head_text), .other = "two\n", .other_cap = 5u}),
              "the last commit has the other file whole");
    cli_check((EMBED_CALL(cli_changes.files, CliGitChangesCfg, .at = 0u) == 1u) &&
                  (EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 1u) && row_with_name("fresh.txt", &fresh) &&
                  !fresh.open && (fresh.state == 'M'),
              "the file with the line left out stands alone after, its changes closed");
    cli_check(EMBED_CALL(cli_changes.open, CliGitChangesCfg, .root = s_root, .at = 0u) &&
                  (EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 3u) &&
                  EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = 1u, .row = &hunk) && (hunk.from == 1u) &&
                  (hunk.to == 2u) && line_row_is(2u, 0, "put in"),
              "the change left is the line put in");
    const GuiRect hunk_rect =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 1u);
    cli_check(EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 1u,
                         .x = hunk_rect.x + hunk_rect.width - 20, .open_out = &open) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = open.path, .cap = CLI_POOL_BYTES, .other = "fresh.txt", .other_cap = 10u}) &&
                  open.placed && (open.place.line == 1ULL),
              "a change's row pressed opens the file at the change's first line");
    const GuiRect fresh_rect =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 0u);
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 0u,
                          .root = s_root,
                          .x = fresh_rect.x + GUI_PANE_PAD_LEFT + GUI_CHANGES_BOX + GUI_CHANGES_BOX_GAP + 4,
                          .open_out = &open) &&
                  (EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 1u),
              "the arrow pressed closes the file's changes");
    EMBED_CALL(cli_changes.toggle, CliGitChangesCfg, .at = 0u);
    EMBED_CALL(cli_changes.type, CliGitChangesCfg, .text = "left", .size = 4ULL);
    file_written(s_root, "sub/deep.txt", "two\nthree\n");
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = s_root);
    cli_check(EMBED_CALL(cli_changes.commit, CliGitChangesCfg, .root = s_root) &&
                  (EMBED_CALL(cli_changes.files, CliGitChangesCfg, .at = 0u) == 1u) && row_with_name("fresh.txt", &fresh),
              "the file left out stands after the other's commit");
    EMBED_CALL(cli_changes.toggle, CliGitChangesCfg, .at = 0u);

    const int bar_y = frame.side.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + GUI_PANE_HEAD + (GUI_PANE_CHOICE / 2);
    const int left = frame.side.x + GUI_PANE_PAD_LEFT;
    const int right = frame.side.x + frame.side.width - GUI_PANE_PAD_LEFT;
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = left + (GUI_CHANGES_TOOL / 2), .y = bar_y) == CLI_CHANGES_READ,
              "a press on the bar's first tool reads the tree again");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = left + GUI_CHANGES_TOOL + GUI_CHANGES_TOOL_GAP + (GUI_CHANGES_TOOL / 2),
                         .y = bar_y) == CLI_CHANGES_ROLLBACK,
              "a press on the bar's second tool rolls back");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = right - GUI_CHANGES_COUNT - (GUI_CHANGES_TOOL / 2),
                         .y = bar_y) == CLI_CHANGES_PUSH,
              "a press on the bar's last tool pushes");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs,
                         .x = right - GUI_CHANGES_COUNT - GUI_CHANGES_TOOL - GUI_CHANGES_TOOL_GAP - GUI_CHANGES_COUNT -
                              (GUI_CHANGES_TOOL / 2),
                         .y = bar_y) == CLI_CHANGES_PULL,
              "a press on the tool before it pulls");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = left + 4, .y = bar_y + GUI_PANE_CHOICE) == CLI_CHANGES_ALL,
              "a press on the taken row takes or leaves every file");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = left + 20,
                         .y = bar_y + GUI_PANE_CHOICE + (GUI_PANE_CHOICE / 2) + (GUI_PANE_BOX / 2)) == CLI_CHANGES_FIELD,
              "a press in the message box lands on it");
    const int buttons_y = bar_y + GUI_PANE_CHOICE + (GUI_PANE_CHOICE / 2) + GUI_PANE_BOX + (GUI_PANE_CHOICE / 2);
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = left + 20, .y = buttons_y) == CLI_CHANGES_COMMIT,
              "a press in the buttons' left half lands on Commit");
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .glyphs = &glyphs, .x = right - 20, .y = buttons_y) == CLI_CHANGES_COMMIT_PUSH,
              "a press in the buttons' right half lands on Commit and Push");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Commit", .tree = s_root);

    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_SAVED, .root = NULL);
    cli_check(EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 0u, "no tree leaves no rows");

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
