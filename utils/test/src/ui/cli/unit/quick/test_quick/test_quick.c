// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_quick.c: the quick open: a leading > lists the commands, the ones run last first with nothing after it and the
// best match first with a query, at most CLI_QUICK_MOST; a leading : answers the line sentence clamped to the shown
// view's lines, and a sentence with no view; @ lists the shown view's declarations with type and line, and # the
// tree's with path and line; Search Everywhere answers at most CLI_QUICK_EACH of each type under headings, a type with
// no rows standing with no heading; a step passes over a heading; the keys edit the typed text and move its caret;
// `path:line:column` opens the path at the place counted from 0; the keys of a command open the box as the command
// does; the double Shift opens Search Everywhere; and the rows asked again while the tree is read keep the chosen row
// chosen where it stands still
#include "ui/cli/src/code/symbols.h"
#include "ui/cli/src/command_line/commands.h"
#include "ui/cli/src/editor/quick.h"
#include "ui/cli/src/editor/view.h"
#include "ui/cli/src/machine/recent.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the suite's own folder for orior's state, its tree, the shown view's key and the text it shows
static char s_home[CLI_PATH_MAX_LEN];
static char s_tree[CLI_PATH_MAX_LEN];
static int s_begun;
#define VIEW_KEY "quick:check.c"
static const char s_view_text[] = "int top;\n"
                                  "static void helper(void)\n"
                                  "{\n"
                                  "}\n"
                                  "\n"
                                  "int main(void)\n"
                                  "{\n"
                                  "    return 0;\n"
                                  "}\n";

// the file `name` of the tree written with `text`, its folder `folder` made first where it is not NULL
static void tree_file(const char *folder, const char *name, const char *text)
{
    if (folder != NULL)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = folder, .joined = path);
        (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
    }
    cli_unity_file(s_tree, name, text);
}

void setUp(void)
{
    cli_unity_begun();
    if (s_begun)
    {
        return;
    }
    s_begun = 1;
    cli_unity_folder("build/harness/quick_home", s_home);
    TEST_ASSERT_TRUE(
        EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = s_home));
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &err_msg));
    cli_unity_folder("build/harness/quick_tree", s_tree);
    const char *const init[2] = {"init", "-q"};
    cli_unity_git(s_tree, init, 2u);
    tree_file("src", "src/view.rs", "x");
    tree_file(NULL, "src/view_model.rs", "x");
    tree_file(NULL, "decl.c", "int gamma_value(void)\n{\n    return 3;\n}\n");
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = VIEW_KEY, .text = s_view_text,
                                .size = sizeof(s_view_text) - 1u, .shape = &shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = VIEW_KEY, .path = "check.c"));
}

void tearDown(void)
{
    EMBED_CALL(cli_quick.close, CliQuickCfg, .at = 0u);
}

// what the box shows
static CliQuickView viewed(void)
{
    CliQuickView view;
    EMBED_CALL(cli_quick.view, CliQuickCfg, .view_out = &view);
    return view;
}

// the row `at`
static CliQuickRow row_at(unsigned int at)
{
    CliQuickRow row = {0};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.row, CliQuickCfg, .at = at, .row_out = &row));
    return row;
}

// whether `length` bytes of `text` are `requested`
static int text_is(const char *text, unsigned long long length, const char *requested)
{
    return (length == CLI_UNITY_LENGTH(requested)) &&
           ((length == 0ULL) || (memor.cmp(&(MemoriaCfg){.src = text, .other = requested, .bytes = length}) == 0));
}

// whether `length` bytes of `text` contain `part`
static int text_holds(const char *text, unsigned long long length, const char *part)
{
    return cellul.find(&(CatenaFinitaCfg){
               .src = text, .cap = length, .other = part, .other_cap = CLI_UNITY_LENGTH(part)}) != NULL;
}

// the box opened by `command` for the tree, the view shown where `shown_view`, with `text` typed after
static void opened(const char *command, int shown_view, const char *text)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.open, CliQuickCfg, .command = command, .root = s_tree,
                                .key = shown_view ? VIEW_KEY : NULL, .shown = 8u, .now_ms = 1000ULL));
    if (text != NULL)
    {
        EMBED_CALL(cli_quick.type, CliQuickCfg, .text = text, .length = CLI_UNITY_LENGTH(text), .now_ms = 1000ULL);
    }
}

void test_a_command_none_of_the_quick_opens_nothing(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_quick.open, CliQuickCfg, .command = "save", .root = s_tree));
    TEST_ASSERT_FALSE(viewed().open);
}

void test_a_leading_greater_lists_the_commands_the_ones_run_last_first(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "save-all");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "save");
    opened("palette", 0, NULL);
    const CliQuickView view = viewed();
    TEST_ASSERT_TRUE(view.open);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_COMMANDS, view.mode);
    TEST_ASSERT_TRUE(text_is(view.text, view.length, ">"));
    TEST_ASSERT_TRUE(text_is(view.placeholder, view.placeholder_length, "Type the name of a command to run."));
    const CliQuickRow first = row_at(0u);
    const CliQuickRow second = row_at(1u);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_COMMAND, first.type);
    TEST_ASSERT_TRUE(text_is(first.label, first.label_length, "File: Save"));
    TEST_ASSERT_TRUE(text_is(first.detail, first.detail_length, "recently used"));
    TEST_ASSERT_TRUE(text_is(first.keys, first.keys_length, "Ctrl+S"));
    TEST_ASSERT_TRUE(text_is(second.label, second.label_length, "File: Save All"));
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_MOST, view.rows);
}

void test_a_query_after_the_greater_marks_the_letters_it_matched(void)
{
    opened("palette", 0, "go to line");
    const CliQuickRow first = row_at(0u);
    TEST_ASSERT_TRUE(text_is(first.label, first.label_length, "Go: Go to Line…"));
    TEST_ASSERT_EQUAL_UINT(0u, first.detail_length);
    TEST_ASSERT_TRUE(text_is(first.keys, first.keys_length, "Ctrl+G"));
    TEST_ASSERT_TRUE(first.label_mark_count >= 1u);
    // the last mark ends at the last letter of the label's own `Line`
    const unsigned int last = first.label_mark_count - 1u;
    TEST_ASSERT_EQUAL_UINT(14u, first.label_marks[2u * last] + first.label_marks[(2u * last) + 1u]);
    opened("palette", 0, "zzqqxx");
    const CliQuickRow none = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(1u, viewed().rows);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_TEXT, none.type);
    TEST_ASSERT_TRUE(text_is(none.label, none.label_length, "No matching commands"));
}

void test_a_leading_colon_answers_the_line_clamped_to_the_view(void)
{
    CliShape shape = {0ULL, 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = VIEW_KEY, .shape = &shape));
    char asked[96];
    VerbaLinea asked_line = {asked, sizeof(asked), 0ULL};
    mmgr_line_put(&asked_line, "Type a line number from 1 to ");
    mmgr_line_decimal(&asked_line, shape.lines, 1u);
    mmgr_line_put(&asked_line, " to go to it.");
    (void)mmgr_line_finish(&asked_line);
    char gone[64];
    VerbaLinea gone_line = {gone, sizeof(gone), 0ULL};
    mmgr_line_put(&gone_line, "Go to line ");
    mmgr_line_decimal(&gone_line, shape.lines, 1u);
    mmgr_line_put(&gone_line, ", column 3");
    (void)mmgr_line_finish(&gone_line);
    opened("line", 1, NULL);
    CliQuickRow row = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_TEXT, row.type);
    TEST_ASSERT_TRUE(text_is(row.label, row.label_length, asked));
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "40:3", .length = 4ULL, .now_ms = 1000ULL);
    row = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_LINE, row.type);
    TEST_ASSERT_TRUE(text_is(row.label, row.label_length, gone));
    CliQuickChoice choice;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.choose, CliQuickCfg, .at = CLI_QUICK_CHOSEN, .choice_out = &choice));
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_CHOSE_PATH, choice.type);
    TEST_ASSERT_EQUAL_STRING("check.c", choice.path);
    TEST_ASSERT_TRUE(choice.placed);
    TEST_ASSERT_EQUAL_UINT64(shape.lines - 1ULL, choice.place.line);
    TEST_ASSERT_EQUAL_UINT64(2ULL, choice.place.col);
    TEST_ASSERT_FALSE(viewed().open);
    opened("line", 0, "4");
    row = row_at(0u);
    TEST_ASSERT_TRUE(text_is(row.label, row.label_length, "Open a file to go to a line in it."));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_quick.choose, CliQuickCfg, .at = CLI_QUICK_CHOSEN, .choice_out = &choice));
    TEST_ASSERT_TRUE(viewed().open);
}

void test_an_at_lists_the_views_declarations_with_type_and_line(void)
{
    opened("symbol", 1, NULL);
    const CliQuickView view = viewed();
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_SYMBOLS, view.mode);
    TEST_ASSERT_TRUE(view.rows >= 2u);
    unsigned int helper = view.rows;
    for (unsigned int at = 0u; at < view.rows; at += 1u)
    {
        const CliQuickRow row = row_at(at);
        helper = text_is(row.label, row.label_length, "helper") ? at : helper;
    }
    TEST_ASSERT_TRUE(helper < view.rows);
    const CliQuickRow found = row_at(helper);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_SYMBOL, found.type);
    TEST_ASSERT_TRUE(text_is(found.detail, found.detail_length, "function, line 2"));
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "main", .length = 4ULL, .now_ms = 1000ULL);
    const CliQuickRow best = row_at(0u);
    TEST_ASSERT_TRUE(text_is(best.label, best.label_length, "main"));
    TEST_ASSERT_EQUAL_UINT(4u, best.label_marks[1]);
    opened("symbol", 0, NULL);
    const CliQuickRow none = row_at(0u);
    TEST_ASSERT_TRUE(text_is(none.label, none.label_length, "Open a file to go to what it declares."));
}

void test_a_hash_lists_the_trees_declarations_with_path_and_line(void)
{
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_tree);
    const char *const paths[1] = {"decl.c"};
    EMBED_CALL(cli_symbols_root.tree_branches_walk, CliSymbolsRootCfg, .root = s_tree, .paths = paths, .count = 1u);
    opened("symbol-tree", 0, "gamma");
    const CliQuickRow row = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_SYMBOL, row.type);
    TEST_ASSERT_TRUE(text_is(row.label, row.label_length, "gamma_value"));
    TEST_ASSERT_TRUE(text_holds(row.detail, row.detail_length, ", decl.c:1"));
    CliQuickChoice choice;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.choose, CliQuickCfg, .at = 0u, .choice_out = &choice));
    TEST_ASSERT_EQUAL_STRING("decl.c", choice.path);
    TEST_ASSERT_EQUAL_UINT64(0ULL, choice.place.line);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.open, CliQuickCfg, .command = "symbol-tree", .root = NULL));
    const CliQuickRow none = row_at(0u);
    TEST_ASSERT_TRUE(text_is(none.label, none.label_length, "Open a tree to go to what its files declare."));
}

void test_a_path_with_line_and_column_opens_the_place_counted_from_0(void)
{
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_tree);
    opened("file", 0, "view.rs:12:5");
    const CliQuickRow row = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_FILE, row.type);
    TEST_ASSERT_TRUE(text_is(row.label, row.label_length, "view.rs"));
    TEST_ASSERT_TRUE(text_is(row.detail, row.detail_length, "src"));
    TEST_ASSERT_EQUAL_UINT(1u, row.label_mark_count);
    TEST_ASSERT_EQUAL_UINT(0u, row.label_marks[0]);
    TEST_ASSERT_EQUAL_UINT(7u, row.label_marks[1]);
    TEST_ASSERT_EQUAL_UINT(0u, row.detail_mark_count);
    CliQuickChoice choice;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.choose, CliQuickCfg, .at = CLI_QUICK_CHOSEN, .choice_out = &choice));
    TEST_ASSERT_EQUAL_STRING("src/view.rs", choice.path);
    TEST_ASSERT_TRUE(choice.placed);
    TEST_ASSERT_EQUAL_UINT64(11ULL, choice.place.line);
    TEST_ASSERT_EQUAL_UINT64(4ULL, choice.place.col);
    // the file chosen stands first among the files opened last, which nothing typed lists
    opened("file", 0, NULL);
    const CliQuickRow opened_last = row_at(0u);
    TEST_ASSERT_TRUE(text_is(opened_last.label, opened_last.label_length, "view.rs"));
    TEST_ASSERT_TRUE(text_is(opened_last.detail, opened_last.detail_length, "src  recently opened"));
}

void test_search_everywhere_answers_each_type_under_its_heading(void)
{
    opened("search-everywhere", 1, NULL);
    const CliQuickView view = viewed();
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_EVERYWHERE, view.mode);
    TEST_ASSERT_TRUE(
        text_is(view.placeholder, view.placeholder_length, "Search everywhere: files, symbols and actions"));
    unsigned int headings = 0u;
    unsigned int run = 0u;
    for (unsigned int at = 0u; at < view.rows; at += 1u)
    {
        const CliQuickRow row = row_at(at);
        if (row.type == CLI_QUICK_ROW_HEADING)
        {
            headings += 1u;
            run = 0u;
            continue;
        }
        run += 1u;
        TEST_ASSERT_TRUE(run <= CLI_QUICK_EACH);
    }
    TEST_ASSERT_EQUAL_UINT(3u, headings);
    const CliQuickRow first = row_at(0u);
    TEST_ASSERT_TRUE(text_is(first.label, first.label_length, "Files"));
    TEST_ASSERT_EQUAL_UINT(1u, view.chosen);
    // a query no file and no declaration answers leaves those types with no heading
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "save all", .length = 8ULL, .now_ms = 1000ULL);
    const CliQuickRow actions = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_HEADING, actions.type);
    TEST_ASSERT_TRUE(text_is(actions.label, actions.label_length, "Actions"));
}

void test_a_step_passes_over_a_heading_and_stops_at_the_first(void)
{
    opened("search-everywhere", 1, NULL);
    const CliQuickView view = viewed();
    TEST_ASSERT_EQUAL_UINT(1u, view.chosen);
    EMBED_CALL(cli_quick.step, CliQuickCfg, .by = -1);
    TEST_ASSERT_EQUAL_UINT(1u, viewed().chosen);
    unsigned int heading = 0u;
    for (unsigned int at = 1u; (heading == 0u) && (at < view.rows); at += 1u)
    {
        heading = (row_at(at).type == CLI_QUICK_ROW_HEADING) ? at : 0u;
    }
    TEST_ASSERT_TRUE(heading > 1u);
    for (unsigned int at = 1u; at < heading; at += 1u)
    {
        EMBED_CALL(cli_quick.step, CliQuickCfg, .by = 1);
    }
    TEST_ASSERT_EQUAL_UINT(heading + 1u, viewed().chosen);
    EMBED_CALL(cli_quick.step, CliQuickCfg, .by = -1);
    TEST_ASSERT_EQUAL_UINT(heading - 1u, viewed().chosen);
    EMBED_CALL(cli_quick.step, CliQuickCfg, .by = 1000);
    TEST_ASSERT_EQUAL_UINT(view.rows - 1u, viewed().chosen);
    TEST_ASSERT_TRUE((viewed().scroll + 8u) > viewed().chosen);
    EMBED_CALL(cli_quick.step, CliQuickCfg, .scroll_by = -1000);
    TEST_ASSERT_EQUAL_UINT(0u, viewed().scroll);
}

void test_the_keys_edit_the_typed_text_and_move_its_caret(void)
{
    opened("file", 0, "abc");
    TEST_ASSERT_EQUAL_UINT64(3ULL, viewed().caret);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Left"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Backspace"));
    CliQuickView view = viewed();
    TEST_ASSERT_TRUE(text_is(view.text, view.length, "ac"));
    TEST_ASSERT_EQUAL_UINT64(1ULL, view.caret);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Delete"));
    view = viewed();
    TEST_ASSERT_TRUE(text_is(view.text, view.length, "a"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Home"));
    TEST_ASSERT_EQUAL_UINT64(0ULL, viewed().caret);
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "\xc3\xa9", .length = 2ULL);
    TEST_ASSERT_EQUAL_UINT64(2ULL, viewed().caret);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Left"));
    TEST_ASSERT_EQUAL_UINT64(0ULL, viewed().caret);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Right"));
    TEST_ASSERT_EQUAL_UINT64(2ULL, viewed().caret);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "End"));
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "z\ny", .length = 3ULL);
    view = viewed();
    TEST_ASSERT_TRUE(text_is(view.text, view.length,
                             "\xc3\xa9"
                             "az"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "F7"));
}

void test_the_keys_of_a_command_open_the_box_as_it_does(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Mod+Alt+Shift+F11", .root = s_tree));
    TEST_ASSERT_FALSE(viewed().open);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.key, CliQuickCfg, .name = "Mod+Shift+P", .root = s_tree));
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_COMMANDS, viewed().mode);
}

void test_shift_twice_within_its_time_opens_search_everywhere(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_quick.shift, CliQuickCfg, .name = "Shift", .now_ms = 10000ULL));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_quick.shift, CliQuickCfg, .name = "a", .now_ms = 10100ULL));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_quick.shift, CliQuickCfg, .name = "Shift", .now_ms = 10200ULL));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_quick.shift, CliQuickCfg, .name = "Shift", .now_ms = 10700ULL));
    TEST_ASSERT_FALSE(viewed().open);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.shift, CliQuickCfg, .name = "Shift", .now_ms = 10900ULL, .root = s_tree));
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_EVERYWHERE, viewed().mode);
}

void test_a_command_chosen_answers_its_word_and_runs_first_after(void)
{
    opened("palette", 0, "close all editors");
    const CliQuickRow row = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_COMMAND, row.type);
    CliQuickChoice choice;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.choose, CliQuickCfg, .at = 0u, .choice_out = &choice));
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_CHOSE_COMMAND, choice.type);
    TEST_ASSERT_EQUAL_STRING("close-all", choice.command);
    const char *recent[2];
    char *recent_cell = NULL;
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_recent.list, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .most = 1u,
                                          .rows_out = recent, .cell_out = &recent_cell, .owner = g_cli_machine_owner));
    TEST_ASSERT_EQUAL_STRING("close-all", recent[0]);
    g_cli_guard->persistent_buf_release(recent_cell, g_cli_machine_owner);
}

void test_the_rows_asked_again_while_the_tree_is_read_keep_the_chosen_row(void)
{
    static char s_unread[CLI_PATH_MAX_LEN];
    cli_unity_folder("build/harness/quick_unread", s_unread);
    const char *const init[2] = {"init", "-q"};
    cli_unity_git(s_unread, init, 2u);
    cli_unity_file(s_unread, "one.c", "x");
    cli_unity_file(s_unread, "onex.c", "x");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "onex.c");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "one.c");
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_tree);
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_quick.open, CliQuickCfg, .command = "file", .root = s_unread, .shown = 8u, .now_ms = 5000ULL));
    EMBED_CALL(cli_quick.type, CliQuickCfg, .text = "one", .length = 3ULL, .now_ms = 5000ULL);
    const CliQuickView view = viewed();
    TEST_ASSERT_EQUAL_UINT(3u, view.rows);
    TEST_ASSERT_EQUAL_UINT(CLI_QUICK_ROW_READING, row_at(2u).type);
    TEST_ASSERT_EQUAL_UINT64(5250ULL, EMBED_CALL(cli_quick.deadline_ms, CliQuickCfg, .at = 0u));
    EMBED_CALL(cli_quick.step, CliQuickCfg, .by = 1);
    const CliQuickRow chosen = row_at(1u);
    TEST_ASSERT_TRUE(text_is(chosen.label, chosen.label_length, "onex.c"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_quick.again, CliQuickCfg, .now_ms = 5100ULL));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_quick.again, CliQuickCfg, .now_ms = 5300ULL));
    const CliQuickView again = viewed();
    const CliQuickRow still = row_at(again.chosen);
    TEST_ASSERT_TRUE(text_is(still.label, still.label_length, "onex.c"));
}
