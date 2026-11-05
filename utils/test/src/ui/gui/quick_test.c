// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// quick_test.c: the quick open's check: with nothing typed and no file opened lately it lists that nothing matches; a
// name typed lists the tree's file of that name first with its folder and the letters that matched, the tree's files read
// behind the window first and the rows asked for again; a line after the name opens the file at that line; a file
// opened lately is listed first with nothing typed; > lists the menus' commands, one typed found by its label and
// listed first once run; : asks for a line of the file shown and goes to it; @ lists what the file shown declares; #
// lists what the tree's files declare once the tree is walked; the keys move the chosen row and Escape closes; and the
// box drawn under the bar lists the rows where a press lands
#include "../../../../../ui/cli/src/code/symbols.h"
#include "../../../../../ui/cli/src/command_line/commands.h"
#include "../../../../../ui/cli/src/editor/files.h"
#include "../../../../../ui/cli/src/editor/view.h"
#include "../../../../../ui/cli/src/view/parts/quick.h"
#include "../cli/cli_check.h"

#include <string.h>

static char s_root[CLI_PATH_MAX_LEN];

// the file `relative` of the tree written with `text`, its folder put in place first
static void file_written(const char *relative, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = relative, .joined = path);
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

// the row `at` into `row`: whether its label reads `label`
static int row_is(unsigned int at, const char *label, CliQuickRow *row)
{
    return EMBED_CALL(cli_quick.row, CliQuickCfg, .at = at, .row = row) && (row->label_length == strlen(label)) &&
           (strncmp(row->label, label, row->label_length) == 0);
}

// whether the row's detail reads `detail`
static int detail_is(const CliQuickRow *row, const char *detail)
{
    return (row->detail_length == strlen(detail)) && (strncmp(row->detail, detail, row->detail_length) == 0);
}

// the row whose label reads `label`, into `row`: 1, or 0 where none is
static int row_with_label(const char *label, CliQuickRow *row)
{
    const unsigned int rows = EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u);
    for (unsigned int at = 0u; at < rows; at += 1u)
    {
        if (row_is(at, label, row))
        {
            return 1;
        }
    }
    return 0;
}

// the tree's files waited for, read behind the window, ten seconds at the most
static void listed(void)
{
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while (EMBED_CALL(cli_files_root_paths.unread, CliFilesRootPathsCfg, .root = s_root) &&
           (cli_clock_milliseconds() < last_ms))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 10ULL);
    }
}

// the tree's branches waited for, walked on their own thread, ten seconds at the most
static void walked(void)
{
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while (cli_tree_unwalked() && (cli_clock_milliseconds() < last_ms))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 10ULL);
    }
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
    cli_check(EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &why), "the catalog is read");

    cli_check_folder("quick_tree", s_root);
    file_written("src/main.c", "int top;\n"
                               "static void helper(void)\n"
                               "{\n"
                               "}\n"
                               "\n"
                               "int main(void)\n"
                               "{\n"
                               "    return 0;\n"
                               "}\n");
    file_written("notes.txt", "one\ntwo\nthree\n");
    file_written("src/deep/alpha_beta.h", "#define ALPHA 1\n");

    EMBED_CALL(cli_quick.begin, CliQuickCfg, .entries = NULL);
    cli_check(!EMBED_CALL(cli_quick.shown, CliQuickCfg, .at = 0u), "the quick open is closed until opened");
    EMBED_CALL(cli_quick.open, CliQuickCfg, .text = "", .root = s_root, .now_ms = 1000ULL);
    CliQuickRow row;
    cli_check(EMBED_CALL(cli_quick.shown, CliQuickCfg, .at = 0u) &&
                  (EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u) == 1u) &&
                  row_is(0u, "No matching results", &row) && !row.goes,
              "nothing typed and no file opened lately lists that nothing matches");

    // the tree's files are read behind the window: the last row reads so, and the rows are asked for again
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "main", .size = 4ULL, .root = s_root, .now_ms = 1000ULL);
    unsigned long long length = 0ULL;
    cli_check(strcmp(EMBED_CALL(cli_quick.text, CliQuickCfg, .length_out = &length), "main") == 0 && (length == 4ULL),
              "the text typed stands as typed");
    const unsigned int first_rows = EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u);
    const int reading = row_is(first_rows - 1u, "Reading the tree's files: more may be found.", &row) && !row.goes;
    cli_check(!reading || (EMBED_CALL(cli_quick.deadline_ms, CliQuickCfg, .at = 0u) == 1000ULL + CLI_QUICK_AGAIN_MS),
              "while the tree's files are read the rows are asked for again a rest later");
    listed();
    cli_check(!reading || EMBED_CALL(cli_quick.step, CliQuickCfg, .root = s_root, .now_ms = 2000ULL),
              "the step past the rest asks for the rows again");
    cli_check(row_is(0u, "main.c", &row) && row.goes && detail_is(&row, "src") && (row.hit_count == 4u) &&
                  (row.hits[0] == 0u) && (row.hits[3] == 3u) && (row.detail_hit_count == 0u),
              "the file whose name was typed stands first with its folder, the letters that matched marked");
    cli_check(EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u) == 1u, "a file that does not match is not listed");
    cli_check(EMBED_CALL(cli_quick.deadline_ms, CliQuickCfg, .at = 0u) == 0ULL,
              "once the files are read no rest waits");
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = ":3", .size = 2ULL, .root = s_root, .now_ms = 2000ULL);
    CliQuickGo go;
    cli_check(row_is(0u, "main.c", &row) && EMBED_CALL(cli_quick.press, CliQuickCfg, .at = 0u, .go = &go) &&
                  (go.kind == CLI_QUICK_GO_FILE) && (strcmp(go.path, "src/main.c") == 0) && go.placed &&
                  (go.line == 2ULL) && (go.col == 0ULL) && !EMBED_CALL(cli_quick.shown, CliQuickCfg, .at = 0u),
              "a line after the name opens the file at that line, and the quick open closes");

    // a file opened lately stands first with nothing typed
    EMBED_CALL(cli_quick.opened, CliQuickCfg, .path = "notes.txt", .root = s_root);
    EMBED_CALL(cli_quick.open, CliQuickCfg, .text = "", .root = s_root, .now_ms = 3000ULL);
    cli_check(row_is(0u, "notes.txt", &row) && row.goes && detail_is(&row, "recently opened"),
              "a file opened lately is listed first with nothing typed");
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "s", .size = 1ULL, .root = s_root, .now_ms = 3000ULL);
    cli_check(row_is(0u, "notes.txt", &row) && (EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u) == 3u),
              "a file opened lately scores over the two others that match as well");
    cli_check(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Escape", .root = s_root, .now_ms = 3000ULL) &&
                  !EMBED_CALL(cli_quick.shown, CliQuickCfg, .at = 0u),
              "Escape closes");

    // the menus' commands
    EMBED_CALL(cli_quick.open, CliQuickCfg, .text = ">", .root = s_root, .now_ms = 4000ULL);
    cli_check(EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u) > 10u, "> lists the menus' commands");
    cli_check(!row_with_label("View: Command Palette", &row), "the quick open's own command is not among them");
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "save all", .size = 8ULL, .root = s_root, .now_ms = 4000ULL);
    cli_check(row_is(0u, "File: Save All", &row) && row.goes && (row.hit_count == 7u) && (row.hits[0] == 6u),
              "a command typed is found by its label, the letters that matched marked past the menu's title");
    cli_check(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Enter", .root = s_root, .now_ms = 4000ULL) &&
                  EMBED_CALL(cli_quick.go, CliQuickCfg, .go = &go) && (go.kind == CLI_QUICK_GO_COMMAND) &&
                  (strcmp(go.command, "save-all") == 0) && !EMBED_CALL(cli_quick.shown, CliQuickCfg, .at = 0u),
              "Enter runs the command chosen and closes");
    cli_check(!EMBED_CALL(cli_quick.go, CliQuickCfg, .go = &go), "the choice is read once");
    EMBED_CALL(cli_quick.open, CliQuickCfg, .text = ">", .root = s_root, .now_ms = 5000ULL);
    cli_check(row_is(0u, "File: Save All", &row) && detail_is(&row, "recently used"),
              "a command run last stands first with nothing typed");

    // a line of the file shown
    EMBED_CALL(cli_quick.open, CliQuickCfg, .text = ":", .root = s_root, .now_ms = 6000ULL);
    cli_check(row_is(0u, "Open a file to go to a line in it.", &row) && !row.goes,
              "with no file shown a line has nowhere to go");
    cli_check(cli_view_file_open(s_root, "notes.txt", 40u, 100u, NULL, &why) >= 0, "a view opens on notes.txt");
    EMBED_CALL(cli_quick.open, CliQuickCfg, .text = ":", .root = s_root, .now_ms = 6000ULL);
    cli_check(EMBED_CALL(cli_quick.row, CliQuickCfg, .at = 0u, .row = &row) && !row.goes &&
                  (strncmp(row.label, "Type a line number from 1 to ", 29u) == 0),
              "with a file shown the line is asked for, with the count of lines");
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "2", .size = 1ULL, .root = s_root, .now_ms = 6000ULL);
    cli_check(row_is(0u, "Go to line 2", &row) && EMBED_CALL(cli_quick.press, CliQuickCfg, .at = 0u, .go = &go) &&
                  (go.kind == CLI_QUICK_GO_FILE) && (strcmp(go.path, "notes.txt") == 0) && go.placed &&
                  (go.line == 1ULL) && (go.col == 0ULL),
              "a line typed goes to it in the file shown");

    // what the file shown declares
    cli_check(cli_view_file_open(s_root, "src/main.c", 40u, 100u, NULL, &why) >= 0, "a view opens on main.c");
    EMBED_CALL(cli_quick.open, CliQuickCfg, .text = "@", .root = s_root, .now_ms = 7000ULL);
    cli_check(row_with_label("helper", &row) && row.goes && detail_is(&row, "function, line 2") &&
                  row_with_label("main", &row) && detail_is(&row, "function, line 6"),
              "@ lists what the file shown declares with each one's kind and line");
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "ma", .size = 2ULL, .root = s_root, .now_ms = 7000ULL);
    cli_check(row_is(0u, "main", &row) && (row.hit_count == 2u) &&
                  EMBED_CALL(cli_quick.press, CliQuickCfg, .at = 0u, .go = &go) && (go.kind == CLI_QUICK_GO_FILE) &&
                  (strcmp(go.path, "src/main.c") == 0) && (go.line == 5ULL),
              "a name typed finds the declaration, which goes to its line");

    // what the tree's files declare
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = s_root);
    walked();
    EMBED_CALL(cli_quick.open, CliQuickCfg, .text = "#helper", .root = s_root, .now_ms = 8000ULL);
    cli_check(row_with_label("helper", &row) && row.goes && detail_is(&row, "function, src/main.c:2"),
              "# lists what the tree's files declare with each one's kind, file and line");

    // the keys move the chosen row
    EMBED_CALL(cli_quick.open, CliQuickCfg, .text = "@", .root = s_root, .now_ms = 9000ULL);
    const unsigned int declared = EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u);
    cli_check((declared >= 2u) && (EMBED_CALL(cli_quick.chosen, CliQuickCfg, .at = 0u) == 0u),
              "the first row is chosen as the rows open");
    cli_check(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Down", .root = s_root, .now_ms = 9000ULL) &&
                  (EMBED_CALL(cli_quick.chosen, CliQuickCfg, .at = 0u) == 1u),
              "Down chooses the next row");
    cli_check(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "PageDown", .root = s_root, .now_ms = 9000ULL) &&
                  (EMBED_CALL(cli_quick.chosen, CliQuickCfg, .at = 0u) == declared - 1u),
              "PageDown stops at the last row");
    cli_check(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "PageUp", .root = s_root, .now_ms = 9000ULL) &&
                  EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Up", .root = s_root, .now_ms = 9000ULL) &&
                  (EMBED_CALL(cli_quick.chosen, CliQuickCfg, .at = 0u) == 0u),
              "PageUp and Up stop at the first row");
    cli_check(!EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "F5", .root = s_root, .now_ms = 9000ULL),
              "a key that is not the quick open's is not taken");
    cli_check(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Backspace", .root = s_root, .now_ms = 9000ULL) &&
                  (EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u) >= 1u) &&
                  (strcmp(EMBED_CALL(cli_quick.text, CliQuickCfg, .length_out = &length), "") == 0),
              "Backspace erases the last letter and the rows follow");

    // the box drawn under the bar
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    GuiQuick quick;
    EMBED_CALL(gui_quick.init, GuiQuickCfg, .quick = &quick);
    EMBED_CALL(gui_quick_raster.draw, GuiQuickRasterCfg, .quick = &quick, .raster = &raster, .glyphs = &glyphs,
               .palette = palette, .frame = &frame);
    cli_check((quick.rect.y == GUI_BAR_HEIGHT) && (quick.rect.width == GUI_QUICK_WIDTH) &&
                  (quick.rect.x == (1440 - GUI_QUICK_WIDTH) / 2) && (quick.rect.height > quick.field.height),
              "the box stands under the bar, centered, 40rem wide");
    cli_check((quick.shown >= 1u) && (quick.list.height == (int)quick.shown * GUI_QUICK_ROW),
              "the list is a row for each row shown");
    unsigned int at = 9u;
    cli_check(EMBED_CALL(gui_quick_x.row_at, GuiQuickXCfg, .quick_read = &quick, .x = quick.list.x + 10,
                         .y = quick.list.y + 5, .at = &at) &&
                  (at == 0u),
              "a press on the first row lands on it");
    cli_check(EMBED_CALL(gui_quick_x.holds, GuiQuickXCfg, .quick_read = &quick, .x = quick.rect.x + 2,
                         .y = quick.rect.y + 2) &&
                  !EMBED_CALL(gui_quick_x.holds, GuiQuickXCfg, .quick_read = &quick, .x = quick.rect.x - 2,
                              .y = quick.rect.y + 2),
              "the box takes a press inside it and none beside it");
    cli_check(!EMBED_CALL(gui_quick.wheel, GuiQuickCfg, .quick = &quick, .rows = 3), "a few rows do not scroll");
    EMBED_CALL(cli_quick.close, CliQuickCfg, .at = 0u);
    EMBED_CALL(gui_quick_raster.draw, GuiQuickRasterCfg, .quick = &quick, .raster = &raster, .glyphs = &glyphs,
               .palette = palette, .frame = &frame);
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = quick.rect),
              "closed, the box stands nowhere");

    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
