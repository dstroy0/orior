// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes_test.c: the Changes pane's check: a repository with two files git does not yet track lists them as rows,
// each not yet tracked, the deeper one with its folder; the pane's table draws the rows under the Commit group's title
// on the side, the row pointed on a ground of its own; a press lands on a row and opens its file; the table reads on the
// pane's showing and a save and not on the view's change; and no tree leaves no rows
#include "../../../../../ui/cli/src/code/git.h"
#include "../../../../../ui/cli/src/command_line/commands.h"
#include "../../../../../ui/cli/src/view/parts/changes.h"
#include "../cli/cli_check.h"

#include <string.h>

// the row whose name is `name`, into `row`: 1, or 0 where none is
static int row_with_name(const char *name, CliChangedRow *row)
{
    for (unsigned int at = 0u; EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = at, .row = row); at += 1u)
    {
        if (strcmp(name, row->name) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// the file `relative` of the tree at `root` written with `text`
static void file_written(const char *root, const char *relative, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = relative, .joined = path);
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
                  (strncmp(deep.path, "sub", 3u) == 0),
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
                  (strcmp(open.path, pressed.path) == 0) && !open.placed,
              "a row pressed opens its file where the path's own place puts it");
    cli_check(!EMBED_CALL(gui_pane.wheel, GuiPaneCfg, .table = table, .pane = &pane, .frame = &frame, .rows = 2),
              "two rows do not scroll");
    unsigned int built = 0u;
    const CliItem *rows = NULL;
    cli_check(EMBED_CALL(gui_pane.menu, GuiPaneCfg, .table = table, .row = 1u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&built)) != NULL) && (built == 3u) &&
                  (strcmp(rows[0].label, "Open File") == 0) && (rows[0].needs[0] == '\0') &&
                  (strcmp(rows[0].args, pressed.path) == 0) && rows[1].line &&
                  (strcmp(rows[2].command, "file-copy-path") == 0),
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
                  (strcmp(EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .length_out = &length),
                          "The commit message is empty.") == 0),
              "a commit with no message is refused with why");
    EMBED_CALL(cli_changes.type, CliGitChangesCfg, .text = "first", .size = 5ULL);
    cli_check((strcmp(EMBED_CALL(cli_changes.message, CliGitChangesCfg, .length_out = &length), "first") == 0) &&
                  (length == 5ULL),
              "the text typed is the message");
    unsigned int deep_row = 0u;
    for (CliChangedRow each; EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = deep_row, .row = &each) &&
                             (strcmp(each.name, "deep.txt") != 0);)
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

    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_SAVED, .root = NULL);
    cli_check(EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u) == 0u, "no tree leaves no rows");

    g_cli_cells->persistent_buf_release(raster.pixels, &g_gui_raster_cells);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
