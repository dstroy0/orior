// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_view.c: a view opened on a stored text holds the cursor and the selection; the keys move the cursor by letters, words, lines, the line's ends and
// pages, Shift extending the selection; text typed replaces the selection; Backspace, Delete, Enter and Tab edit; undo and redo step the text back and
// forth, typing joined into one step; the pointer places the cursor and drags a selection, a double click selects the word; the view follows the cursor and
// the frame carries the rows in view, the cursor, the anchor and the lines' runs. Each case opens its own view
#include "ui/cli/src/editor/document.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/editor/pages.h"
#include "ui/cli/src/editor/view.h"
#include "ui/cli/src/plugins/plugins.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_key[] = "check:view";
static const char s_key2[] = "check:words";
static const char s_paged[] = "check:paged";
static const char s_text[] = "int main\n{\n    return 0;\n}\n";
static const char s_words_text[] = "alpha beta\nalp gamma\n";

static char s_out[1u << 16u];
static char s_error[256];
static int s_folder_created;
static char s_folder[CLI_PATH_MAX_LEN];
static char s_numbered_path[CLI_PATH_MAX_LEN];
static char s_numbered[300 * 16];

static CliViewInput view_key(const char *name)
{
    CliViewInput input;
    memor.set(&(MemoriaCfg){.dst = &input, .val = 0, .bytes = sizeof(input)});
    input.kind = CLI_VIEW_KEY;
    input.name = name;
    return input;
}

static CliViewInput view_text(const char *text)
{
    CliViewInput input;
    memor.set(&(MemoriaCfg){.dst = &input, .val = 0, .bytes = sizeof(input)});
    input.kind = CLI_VIEW_TEXT;
    input.text = text;
    input.length = CLI_UNITY_LENGTH(text);
    return input;
}

static CliViewInput view_pointer(unsigned int pointer, unsigned long long row, unsigned long long column, unsigned int clicks, int shift)
{
    CliViewInput input;
    memor.set(&(MemoriaCfg){.dst = &input, .val = 0, .bytes = sizeof(input)});
    input.kind = CLI_VIEW_POINTER;
    input.pointer = pointer;
    input.row = row;
    input.column = column;
    input.clicks = clicks;
    input.shift = shift;
    return input;
}

static CliViewInput view_size(unsigned int rows)
{
    CliViewInput input = view_key("");
    input.kind = CLI_VIEW_SIZE;
    input.rows = rows;
    input.columns = 80u;
    return input;
}

// the input sent to the view `key`, asserted accepted
static void input_sent_to(const char *key, const CliViewInput *input)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    const int accepted = EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = input, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    TEST_ASSERT_TRUE_MESSAGE(accepted, s_error);
}

// the input sent to the view, asserted accepted
static void input_sent(const CliViewInput *input)
{
    input_sent_to(s_key, input);
}

// a key sent to the view, asserted accepted
static void key(const CliNameCfg *args)
{
    const CliViewInput input = view_key(args->name);
    input_sent(&input);
}

// a key sent `count` times
static void key_times(const char *name, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(key, CliNameCfg, .name = name);
    }
}

// text typed into the view, asserted accepted
static void typed(const char *text)
{
    const CliViewInput input = view_text(text);
    input_sent(&input);
}

// the cursor asserted at line, col and the anchor at anchor_line, anchor_col
static void assert_at(unsigned long long line, unsigned long long col, unsigned long long anchor_line, unsigned long long anchor_col)
{
    CliPosition cursor;
    CliPosition anchor;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = s_key, .cursor = &cursor, .anchor = &anchor));
    TEST_ASSERT_EQUAL_UINT64(line, cursor.line);
    TEST_ASSERT_EQUAL_UINT64(col, cursor.col);
    TEST_ASSERT_EQUAL_UINT64(anchor_line, anchor.line);
    TEST_ASSERT_EQUAL_UINT64(anchor_col, anchor.col);
}

// the text's lines from `first` to before `last` asserted to read as `required`, joined by "\n"
static void assert_reads(unsigned long long first, unsigned long long last, const char *required)
{
    unsigned long long size = 0ull;
    char *const lines = EMBED_CALL(cli_document_key.lines_alloc, CliDocumentKeyCfg, .key = s_key, .first = first, .last = last - 1ull, .size_pointer = &size, .state = NULL, .capacity = 0ull, .owner = &g_checks_cells);
    TEST_ASSERT_NOT_NULL(lines);
    CLI_TEST_ASSERT_TEXT(required, lines, size);
    g_cli_cells->persistent_buf_release(lines, &g_checks_cells);
}

// how many lines the text has
static unsigned long long lines_now(void)
{
    CliShape shape;
    return EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = s_key, .shape = &shape) ? shape.lines : 0ull;
}

// the whole text asserted to read as `required`, its lines joined by "\n"
static void assert_full_text(const char *required)
{
    assert_reads(0ull, lines_now(), required);
}

// a view opened anew on `text` as the file `path`, its language the plugin's by the extension, sized twenty rows
static void fresh_named(const char *text, const char *path)
{
    CliShape shape;
    const CliViewInput size = view_size(20u);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = text, .size = CLI_UNITY_LENGTH(text), .shape = &shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = path));
    input_sent(&size);
}

// a view opened anew on `text`, as a file of the C plugin's language
static void fresh(const char *text)
{
    fresh_named(text, "check/view.c");
}

// a view opened anew on the C text, colored by the C plugin's grammar as the branch of a file is, its regions making it folds, sized three rows
static void main_view(void)
{
    char id[CLI_NAME_MAX_LEN];
    char grammar[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    const CliViewInput size = view_size(3u);
    VerbaLinea why = {s_error, sizeof(s_error), 0ull};
    fresh_named(s_text, "check/view.c");
    EMBED_CALL(cli_plugins_table.languages, CliPluginsCfg, .languages = &languages);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg, .languages_read = &languages, .path = "check/view.c", .id_out = id));
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &languages);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = id, .key = grammar, .err_msg = &why));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = s_key, .grammar = grammar, .named = 1));
    cli_document_walk();
    input_sent(&size);
}

// the C view with `(void)` typed after main, the cursor after it
static void main_typed(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "End");
    typed("(void)");
}

// the C view with the cursor at the start of its third line
static void main_at_line_two(void)
{
    main_view();
    key_times("Down", 2u);
}

// the C view with `(void)` typed and the region opened on its second line folded, the cursor at that line's end
static void main_folded(void)
{
    main_typed();
    cli_document_walk();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Home");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Shift+[");
}

// a second text stored and viewed, its cursor pressed to after `alp` on the second line
static void words_view(void)
{
    const CliViewInput sized = {CLI_VIEW_SIZE, NULL, NULL, 0ull, 0u, 0ull, 0ull, 0u, 0, 0ull, 10u, 40u, 0};
    const CliViewInput pressed = {CLI_VIEW_POINTER, NULL, NULL, 0ull, CLI_VIEW_POINTER_DOWN, 1ull, 3ull, 1u, 0, 0ull, 0u, 0u, 0};
    const CliViewInput released = {CLI_VIEW_POINTER, NULL, NULL, 0ull, CLI_VIEW_POINTER_UP, 1ull, 3ull, 1u, 0, 0ull, 0u, 0u, 0};
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key2, .text = s_words_text, .size = sizeof(s_words_text) - 1u, .shape = &shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key2, .path = "check/words.txt"));
    input_sent_to(s_key2, &sized);
    input_sent_to(s_key2, &pressed);
    input_sent_to(s_key2, &released);
}

// a file of three hundred numbered lines written once for the process, opened in the pages with a branch over it, indexed, and viewed sized ten rows
static void paged_view(void)
{
    const CliViewInput size = view_size(10u);
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    VerbaLinea opened = {s_out, sizeof(s_out), 0ull};
    unsigned long long lines = 0ull;
    int whole = 0;
    if (!s_folder_created)
    {
        VerbaLinea numbered = {s_numbered, sizeof(s_numbered), 0ull};
        s_folder_created = 1;
        cli_unity_folder("view", s_folder);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "numbered.txt", .joined = s_numbered_path);
        for (unsigned long long line = 0ull; line < 300ull; line += 1ull)
        {
            mmgr_line_put(&numbered, "line ");
            mmgr_line_decimal(&numbered, line, 1u);
            mmgr_line_put(&numbered, " here\n");
        }
        TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_numbered_path, .bytes = s_numbered, .size = numbered.at));
    }
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = s_paged, .path = s_numbered_path, .out = &opened, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_key_from.page, CliDocumentKeyFromCfg, .key = s_paged, .from = 0ull, .to = 40ull));
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while ((!EMBED_CALL(cli_pages.count, CliPagesCfg, .key = s_paged, .lines = &lines, .whole = &whole) || !whole) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 1u);
    }
    TEST_ASSERT_TRUE(whole);
    TEST_ASSERT_EQUAL_UINT64(301ull, lines);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_paged, .path = s_numbered_path));
    input_sent_to(s_paged, &size);
}

// the paged view closed and its text released
static void paged_closed(void)
{
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_paged);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_paged);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = s_paged);
}

// a key sent to the paged view, asserted accepted
static void paged_key(const char *name)
{
    const CliViewInput input = view_key(name);
    input_sent_to(s_paged, &input);
}

// the paged cursor asserted at line, col, the anchor there too
static void assert_paged_at(unsigned long long line, unsigned long long col)
{
    CliPosition cursor;
    CliPosition anchor;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = s_paged, .cursor = &cursor, .anchor = &anchor));
    TEST_ASSERT_EQUAL_UINT64(line, cursor.line);
    TEST_ASSERT_EQUAL_UINT64(col, cursor.col);
    TEST_ASSERT_EQUAL_UINT64(line, anchor.line);
    TEST_ASSERT_EQUAL_UINT64(col, anchor.col);
}

// the paged view moved to the start of the word `here` on the line before last
static void paged_at_word(void)
{
    paged_view();
    paged_key("Mod+End");
    paged_key("Up");
    paged_key("End");
    paged_key("Mod+Left");
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
}

void test_a_view_does_not_open_on_a_text_not_stored(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = "check:none", .path = ""));
}

void test_the_cursor_begins_at_the_texts_start(void)
{
    main_view();
    assert_at(0ull, 0ull, 0ull, 0ull);
}

void test_right_moves_a_letter(void)
{
    main_view();
    key_times("Right", 3u);
    assert_at(0ull, 3ull, 0ull, 3ull);
}

void test_end_moves_to_the_lines_end(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "End");
    assert_at(0ull, 8ull, 0ull, 8ull);
}

void test_right_at_the_end_moves_to_the_next_lines_start(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "End");
    EMBED_CALL(key, CliNameCfg, .name = "Right");
    assert_at(1ull, 0ull, 1ull, 0ull);
}

void test_left_at_the_start_moves_to_the_line_befores_end(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "Left");
    assert_at(0ull, 8ull, 0ull, 8ull);
}

void test_down_stores_the_column_as_far_as_the_line_reaches(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "End");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    assert_at(1ull, 1ull, 1ull, 1ull);
}

void test_down_again_finds_the_column_stored(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "End");
    key_times("Down", 2u);
    assert_at(2ull, 8ull, 2ull, 8ull);
}

void test_home_moves_to_the_lines_first_letter(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "End");
    key_times("Down", 2u);
    EMBED_CALL(key, CliNameCfg, .name = "Home");
    assert_at(2ull, 4ull, 2ull, 4ull);
}

void test_home_again_moves_to_the_lines_start(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "End");
    key_times("Down", 2u);
    key_times("Home", 2u);
    assert_at(2ull, 0ull, 2ull, 0ull);
}

void test_mod_right_moves_end_the_word(void)
{
    main_at_line_two();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Right");
    assert_at(2ull, 10ull, 2ull, 10ull);
}

void test_mod_left_moves_to_the_words_start(void)
{
    main_at_line_two();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Right");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Left");
    assert_at(2ull, 4ull, 2ull, 4ull);
}

void test_shift_end_extends_the_selection_to_the_end(void)
{
    main_at_line_two();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Right");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Left");
    EMBED_CALL(key, CliNameCfg, .name = "Shift+End");
    assert_at(2ull, 13ull, 2ull, 4ull);
}

void test_left_with_a_selection_goes_to_its_start(void)
{
    main_at_line_two();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Right");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Left");
    EMBED_CALL(key, CliNameCfg, .name = "Shift+End");
    EMBED_CALL(key, CliNameCfg, .name = "Left");
    assert_at(2ull, 4ull, 2ull, 4ull);
}

void test_mod_end_moves_to_the_texts_end(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    assert_at(4ull, 0ull, 4ull, 0ull);
}

void test_mod_home_moves_to_the_texts_start(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Home");
    assert_at(0ull, 0ull, 0ull, 0ull);
}

void test_page_down_moves_the_rows_less_one(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "PageDown");
    assert_at(2ull, 0ull, 2ull, 0ull);
}

void test_down_stops_at_the_last_line(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "PageDown");
    key_times("Down", 3u);
    assert_at(4ull, 0ull, 4ull, 0ull);
}

void test_text_typed_is_written_at_the_cursor_which_follows_it(void)
{
    main_typed();
    assert_reads(0ull, 1ull, "int main(void)");
    assert_at(0ull, 14ull, 0ull, 14ull);
}

void test_backspace_removes_the_letter_before_the_cursor(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Backspace");
    assert_reads(0ull, 1ull, "int main(void");
    assert_at(0ull, 13ull, 0ull, 13ull);
}

void test_undo_restores_the_letter_and_the_cursor_after_it(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Backspace");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    assert_reads(0ull, 1ull, "int main(void)");
    assert_at(0ull, 14ull, 0ull, 14ull);
}

void test_undo_removes_the_typing_as_one_step(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    assert_reads(0ull, 1ull, "int main");
    assert_at(0ull, 8ull, 0ull, 8ull);
}

void test_redo_restores_the_typing(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Y");
    assert_reads(0ull, 1ull, "int main(void)");
}

void test_enter_breaks_the_line(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    assert_reads(0ull, 2ull, "int main(void)\n");
    TEST_ASSERT_EQUAL_UINT64(6ull, lines_now());
    assert_at(1ull, 0ull, 1ull, 0ull);
}

void test_backspace_at_a_lines_start_joins_it_to_the_one_before(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    EMBED_CALL(key, CliNameCfg, .name = "Backspace");
    TEST_ASSERT_EQUAL_UINT64(5ull, lines_now());
    assert_at(0ull, 14ull, 0ull, 14ull);
}

void test_delete_at_a_lines_end_joins_the_next_to_it(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Delete");
    assert_reads(0ull, 1ull, "int main(void){");
    assert_at(0ull, 14ull, 0ull, 14ull);
}

void test_undo_parts_the_joined_lines(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Delete");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    TEST_ASSERT_EQUAL_UINT64(5ull, lines_now());
    assert_reads(0ull, 1ull, "int main(void)");
}

void test_enter_preserves_the_lines_indentation(void)
{
    main_typed();
    key_times("Down", 2u);
    EMBED_CALL(key, CliNameCfg, .name = "Home");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    assert_reads(2ull, 4ull, "    \n    return 0;");
    assert_at(3ull, 4ull, 3ull, 4ull);
}

void test_undo_removes_the_break(void)
{
    main_typed();
    key_times("Down", 2u);
    EMBED_CALL(key, CliNameCfg, .name = "Home");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    TEST_ASSERT_EQUAL_UINT64(5ull, lines_now());
}

void test_tab_indents_by_a_step(void)
{
    main_at_line_two();
    EMBED_CALL(key, CliNameCfg, .name = "Tab");
    assert_reads(2ull, 3ull, "        return 0;");
    assert_at(2ull, 4ull, 2ull, 4ull);
}

void test_backspace_in_the_indentation_removes_a_step(void)
{
    main_at_line_two();
    EMBED_CALL(key, CliNameCfg, .name = "Tab");
    EMBED_CALL(key, CliNameCfg, .name = "Backspace");
    assert_reads(2ull, 3ull, "    return 0;");
    assert_at(2ull, 0ull, 2ull, 0ull);
}

void test_shift_right_extends(void)
{
    main_at_line_two();
    key_times("Shift+Right", 2u);
    assert_at(2ull, 2ull, 2ull, 0ull);
}

void test_escape_collapses_the_selection(void)
{
    main_at_line_two();
    key_times("Shift+Right", 2u);
    EMBED_CALL(key, CliNameCfg, .name = "Escape");
    assert_at(2ull, 2ull, 2ull, 2ull);
}

void test_text_typed_replaces_the_selection(void)
{
    main_at_line_two();
    key_times("Right", 2u);
    key_times("Shift+Left", 2u);
    typed("\t");
    assert_reads(2ull, 3ull, "\t  return 0;");
    assert_at(2ull, 1ull, 2ull, 1ull);
}

void test_undo_restores_the_selection(void)
{
    main_at_line_two();
    key_times("Right", 2u);
    key_times("Shift+Left", 2u);
    typed("\t");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    assert_reads(2ull, 3ull, "    return 0;");
    assert_at(2ull, 0ull, 2ull, 2ull);
}

void test_mod_a_selects_the_complete_text(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    assert_at(4ull, 0ull, 0ull, 0ull);
}

void test_delete_removes_the_selection_and_undo_restores_it(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    EMBED_CALL(key, CliNameCfg, .name = "Delete");
    TEST_ASSERT_EQUAL_UINT64(1ull, lines_now());
    assert_reads(0ull, 1ull, "");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    TEST_ASSERT_EQUAL_UINT64(5ull, lines_now());
    assert_reads(0ull, 1ull, "int main(void)");
}

void test_the_pointer_pressed_positions_the_cursor_at_its_cell(void)
{
    const CliViewInput down = view_pointer(CLI_VIEW_POINTER_DOWN, 2ull, 6ull, 1u, 0);
    main_view();
    input_sent(&down);
    assert_at(2ull, 6ull, 2ull, 6ull);
}

void test_the_pointer_dragged_extends_the_selection(void)
{
    const CliViewInput down = view_pointer(CLI_VIEW_POINTER_DOWN, 2ull, 6ull, 1u, 0);
    const CliViewInput dragged = view_pointer(CLI_VIEW_POINTER_MOVED, 2ull, 10ull, 1u, 0);
    main_view();
    input_sent(&down);
    input_sent(&dragged);
    assert_at(2ull, 10ull, 2ull, 6ull);
}

void test_a_double_press_selects_the_word(void)
{
    const CliViewInput twice = view_pointer(CLI_VIEW_POINTER_DOWN, 2ull, 6ull, 2u, 0);
    main_view();
    input_sent(&twice);
    assert_at(2ull, 10ull, 2ull, 4ull);
}

void test_the_pointer_end_a_lines_end_positions_the_cursor_at_its_end(void)
{
    const CliViewInput end = view_pointer(CLI_VIEW_POINTER_DOWN, 0ull, 40ull, 1u, 0);
    main_typed();
    input_sent(&end);
    assert_at(0ull, 14ull, 0ull, 14ull);
}

void test_the_word_at_a_position_is_the_word_the_position_touches(void)
{
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    char word[64];
    words_view();
    const int found = EMBED_CALL(cli_view_line.word_at, CliViewLineCfg, .key = s_key2, .line_value = 0ull, .col_value = 7ull, .from = &from, .to = &to, .word_text = word, .capacity = sizeof(word));
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key2);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key2);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_UINT64(6ull, from);
    TEST_ASSERT_EQUAL_UINT64(10ull, to);
    TEST_ASSERT_EQUAL_STRING("beta", word);
}

void test_a_position_at_a_words_end_touches_it(void)
{
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    char word[64];
    words_view();
    const int found = EMBED_CALL(cli_view_line.word_at, CliViewLineCfg, .key = s_key2, .line_value = 0ull, .col_value = 10ull, .from = &from, .to = &to, .word_text = word, .capacity = sizeof(word));
    const int found_first = EMBED_CALL(cli_view_line.word_at, CliViewLineCfg, .key = s_key2, .line_value = 0ull, .col_value = 5ull, .from = &from, .to = &to, .word_text = word, .capacity = sizeof(word));
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key2);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key2);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_TRUE(found_first);
    TEST_ASSERT_EQUAL_UINT64(0ull, from);
    TEST_ASSERT_EQUAL_UINT64(5ull, to);
}

void test_an_empty_line_has_no_word(void)
{
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    char word[64];
    words_view();
    const int found = EMBED_CALL(cli_view_line.word_at, CliViewLineCfg, .key = s_key2, .line_value = 2ull, .col_value = 0ull, .from = &from, .to = &to, .word_text = word, .capacity = sizeof(word));
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key2);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key2);
    TEST_ASSERT_FALSE(found);
}

void test_a_row_and_a_cell_from_the_views_top_position_a_line_and_a_column(void)
{
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    words_view();
    const int positioned = EMBED_CALL(cli_view_row.position_at, CliViewRowCfg, .key = s_key2, .row = 1ull, .column = 7ull, .line = &line, .col = &col);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key2);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key2);
    TEST_ASSERT_TRUE(positioned);
    TEST_ASSERT_EQUAL_UINT64(1ull, line);
    TEST_ASSERT_EQUAL_UINT64(7ull, col);
}

void test_a_cell_end_the_lines_end_positions_its_end(void)
{
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    words_view();
    const int positioned = EMBED_CALL(cli_view_row.position_at, CliViewRowCfg, .key = s_key2, .row = 0ull, .column = 40ull, .line = &line, .col = &col);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key2);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key2);
    TEST_ASSERT_TRUE(positioned);
    TEST_ASSERT_EQUAL_UINT64(0ull, line);
    TEST_ASSERT_EQUAL_UINT64(10ull, col);
}

void test_the_prefix_is_the_word_before_the_cursor(void)
{
    unsigned long long from = 0ull;
    char word[64];
    words_view();
    const int found = EMBED_CALL(cli_view_from.prefix, CliViewFromCfg, .key = s_key2, .from = &from, .prefix = word, .capacity = sizeof(word));
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key2);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key2);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_UINT64(0ull, from);
    TEST_ASSERT_EQUAL_STRING("alp", word);
}

void test_a_position_input_moves_the_cursor_to_the_line_and_unit(void)
{
    const CliViewInput positioned = {CLI_VIEW_POSITION, NULL, NULL, 0ull, 0u, 0ull, 7ull, 0u, 0, 0ull, 0u, 0u, 0};
    CliPosition cursor;
    CliPosition anchor;
    words_view();
    input_sent_to(s_key2, &positioned);
    const int read = EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = s_key2, .cursor = &cursor, .anchor = &anchor);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key2);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key2);
    TEST_ASSERT_TRUE(read);
    TEST_ASSERT_EQUAL_UINT64(0ull, cursor.line);
    TEST_ASSERT_EQUAL_UINT64(7ull, cursor.col);
    TEST_ASSERT_EQUAL_UINT64(7ull, anchor.col);
}

void test_a_position_end_the_text_is_cut_to_its_last_line_and_that_lines_end(void)
{
    const CliViewInput positioned_far = {CLI_VIEW_POSITION, NULL, NULL, 0ull, 0u, 9ull, 90ull, 0u, 0, 0ull, 0u, 0u, 0};
    CliPosition cursor;
    CliPosition anchor;
    words_view();
    input_sent_to(s_key2, &positioned_far);
    const int read = EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = s_key2, .cursor = &cursor, .anchor = &anchor);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key2);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key2);
    TEST_ASSERT_TRUE(read);
    TEST_ASSERT_EQUAL_UINT64(2ull, cursor.line);
    TEST_ASSERT_EQUAL_UINT64(0ull, cursor.col);
}

void test_a_view_served_records_the_servers_path_found_until_it_closes(void)
{
    char full[CLI_PATH_MAX_LEN];
    words_view();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = s_key2, .full_out = full));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_full.served_set, CliViewFullCfg, .key = s_key2, .full = "/tree/check/words.txt"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = s_key2, .full_out = full));
    TEST_ASSERT_EQUAL_STRING("/tree/check/words.txt", full);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_full.serving, CliViewFullCfg, .full = "/tree/check/words.txt"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_view_full.serving, CliViewFullCfg, .full = "/tree/check/other.txt"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_path_kind.path, CliViewPathKindCfg, .key = s_key2, .path_out = full));
    TEST_ASSERT_EQUAL_STRING("check/words.txt", full);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key2);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_view_full.serving, CliViewFullCfg, .full = "/tree/check/words.txt"));
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key2);
}

void test_mod_shift_bracket_folds_the_region_opened_on_the_line_the_cursor_at_its_end(void)
{
    main_folded();
    assert_at(1ull, 1ull, 1ull, 1ull);
}

void test_down_steps_over_the_lines_hidden(void)
{
    main_folded();
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    assert_at(4ull, 0ull, 4ull, 0ull);
}

void test_up_steps_back_over_the_lines_hidden(void)
{
    main_folded();
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "Up");
    assert_at(1ull, 1ull, 1ull, 1ull);
}

void test_right_at_the_folded_lines_end_goes_to_the_line_after_the_fold(void)
{
    main_folded();
    EMBED_CALL(key, CliNameCfg, .name = "Right");
    assert_at(4ull, 0ull, 4ull, 0ull);
}

void test_left_after_the_fold_goes_back_to_the_folded_lines_end(void)
{
    main_folded();
    EMBED_CALL(key, CliNameCfg, .name = "Right");
    EMBED_CALL(key, CliNameCfg, .name = "Left");
    assert_at(1ull, 1ull, 1ull, 1ull);
}

void test_an_opener_typed_adds_its_closer_the_cursor_between_them(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    typed("(");
    assert_reads(4ull, 5ull, "()");
    assert_at(4ull, 1ull, 4ull, 1ull);
}

void test_a_closer_typed_before_itself_steps_over_it(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    typed("(");
    typed(")");
    assert_reads(4ull, 5ull, "()");
    assert_at(4ull, 2ull, 4ull, 2ull);
}

void test_a_quote_typed_adds_its_peer(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    typed("\"");
    assert_reads(4ull, 5ull, "\"\"");
    assert_at(4ull, 1ull, 4ull, 1ull);
}

void test_a_quote_typed_before_its_peer_steps_over_it(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    typed("\"");
    typed("\"");
    assert_reads(4ull, 5ull, "\"\"");
    assert_at(4ull, 2ull, 4ull, 2ull);
}

void test_backspace_between_a_pair_of_quotes_removes_both(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    typed("\"");
    EMBED_CALL(key, CliNameCfg, .name = "Backspace");
    assert_reads(4ull, 5ull, "");
    assert_at(4ull, 0ull, 4ull, 0ull);
}

void test_backspace_between_a_bracket_pair_removes_both(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    typed("(");
    EMBED_CALL(key, CliNameCfg, .name = "Backspace");
    assert_reads(4ull, 5ull, "");
    assert_at(4ull, 0ull, 4ull, 0ull);
}

void test_an_opener_typed_around_a_selection_wraps_it_the_selection_inside(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Home");
    EMBED_CALL(key, CliNameCfg, .name = "End");
    key_times("Shift+Left", 6u);
    typed("[");
    assert_reads(0ull, 1ull, "int main[(void)]");
    assert_at(0ull, 15ull, 0ull, 9ull);
}

void test_undo_removes_the_wrap(void)
{
    main_typed();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Home");
    EMBED_CALL(key, CliNameCfg, .name = "End");
    key_times("Shift+Left", 6u);
    typed("[");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    assert_reads(0ull, 1ull, "int main(void)");
}

void test_enter_between_a_pair_moves_the_closer_to_its_own_line_the_cursor_a_step_in(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    typed("{");
    assert_reads(4ull, 5ull, "{}");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    assert_reads(4ull, 7ull, "{\n    \n}");
    assert_at(5ull, 4ull, 5ull, 4ull);
}

void test_a_closing_bracket_typed_on_a_blank_prefix_moves_the_prefix_back_a_step(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    typed("{");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    typed("}");
    assert_reads(5ull, 6ull, "}");
    assert_at(5ull, 1ull, 5ull, 1ull);
}

void test_enter_after_an_opener_the_line_leaves_open_indents_the_new_line_a_step(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    typed("f");
    typed("(");
    assert_reads(5ull, 6ull, "f()");
    EMBED_CALL(key, CliNameCfg, .name = "Delete");
    assert_reads(5ull, 6ull, "f(");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    assert_reads(6ull, 7ull, "    ");
    assert_at(6ull, 4ull, 6ull, 4ull);
}

void test_enter_after_a_plain_line_preserves_its_indentation(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+End");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    typed("f");
    typed("(");
    EMBED_CALL(key, CliNameCfg, .name = "Delete");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    typed("x");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    assert_reads(7ull, 8ull, "    ");
    assert_at(7ull, 4ull, 7ull, 4ull);
}

void test_mod_f_reads_a_selection_on_one_line_as_the_query_and_f3_seeks_it_with_find_closed(void)
{
    main_view();
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Shift+Right");
    assert_at(0ull, 3ull, 0ull, 0ull);
    EMBED_CALL(key, CliNameCfg, .name = "Mod+F");
    EMBED_CALL(key, CliNameCfg, .name = "Alt+W");
    EMBED_CALL(key, CliNameCfg, .name = "Enter");
    assert_at(0ull, 3ull, 0ull, 0ull);
    EMBED_CALL(key, CliNameCfg, .name = "Escape");
    EMBED_CALL(key, CliNameCfg, .name = "End");
    EMBED_CALL(key, CliNameCfg, .name = "F3");
    assert_at(0ull, 3ull, 0ull, 0ull);
}

void test_a_view_closed_is_removed(void)
{
    main_view();
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = s_key, .cursor = NULL, .anchor = NULL));
}

void test_mod_end_moves_to_the_paged_texts_last_line(void)
{
    paged_view();
    paged_key("Mod+End");
    assert_paged_at(300ull, 0ull);
    paged_closed();
}

void test_end_finds_the_lines_end_through_the_pages(void)
{
    paged_view();
    paged_key("Mod+End");
    paged_key("Up");
    paged_key("End");
    assert_paged_at(299ull, 13ull);
    paged_closed();
}

void test_mod_left_moves_a_word_through_the_pages(void)
{
    paged_at_word();
    assert_paged_at(299ull, 9ull);
    paged_closed();
}

void test_enter_breaks_a_paged_line_and_backspace_joins_it(void)
{
    unsigned long long lines = 0ull;
    int whole = 0;
    paged_at_word();
    paged_key("Enter");
    assert_paged_at(300ull, 0ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages.count, CliPagesCfg, .key = s_paged, .lines = &lines, .whole = &whole));
    TEST_ASSERT_EQUAL_UINT64(302ull, lines);
    paged_key("Backspace");
    assert_paged_at(299ull, 9ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages.count, CliPagesCfg, .key = s_paged, .lines = &lines, .whole = &whole));
    TEST_ASSERT_EQUAL_UINT64(301ull, lines);
    paged_closed();
}

void test_mod_j_joins_the_line_to_the_next_with_one_space_and_undo_restores_it(void)
{
    fresh("a  \n   b\n\nc");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+J");
    assert_full_text("a b\n\nc");
    assert_at(0ull, 3ull, 0ull, 0ull);
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    assert_full_text("a  \n   b\n\nc");
}

void test_join_lines_joins_the_selections_lines_a_blank_line_adding_nothing(void)
{
    fresh("one\n  two\n\nthree");
    key_times("Shift+Down", 3u);
    EMBED_CALL(key, CliNameCfg, .name = "join-lines");
    assert_full_text("one two three");
    assert_at(0ull, 13ull, 0ull, 0ull);
}

void test_a_blank_line_joined_adds_no_space(void)
{
    fresh("   \nb");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+J");
    assert_full_text("b");
}

void test_the_last_line_joins_nothing(void)
{
    fresh("x\ny");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+J");
    assert_full_text("x\ny");
}

void test_alt_down_moves_the_line_down_the_cursor_with_it(void)
{
    fresh("1\n2\n3\n4");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "Alt+Down");
    assert_full_text("1\n3\n2\n4");
    assert_at(2ull, 0ull, 2ull, 0ull);
}

void test_alt_up_moves_the_line_up(void)
{
    fresh("1\n2\n3\n4");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "Alt+Down");
    key_times("Alt+Up", 2u);
    assert_full_text("2\n1\n3\n4");
    assert_at(0ull, 0ull, 0ull, 0ull);
}

void test_the_first_line_does_not_move_up_and_each_move_is_a_step_of_its_own(void)
{
    fresh("1\n2\n3\n4");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "Alt+Down");
    key_times("Alt+Up", 2u);
    EMBED_CALL(key, CliNameCfg, .name = "Alt+Up");
    assert_full_text("2\n1\n3\n4");
    key_times("Mod+Z", 3u);
    assert_full_text("1\n2\n3\n4");
}

void test_a_selection_ending_at_a_lines_start_leaves_that_line_out_and_moves_with_its_line(void)
{
    fresh("1\n2\n3\n4");
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Down");
    EMBED_CALL(key, CliNameCfg, .name = "move-line-down");
    assert_full_text("2\n1\n3\n4");
    assert_at(2ull, 0ull, 1ull, 0ull);
}

void test_shift_alt_down_copies_the_line_down_the_cursor_on_the_copy(void)
{
    fresh("1\n2\n3");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Alt+Down");
    assert_full_text("1\n2\n2\n3");
    assert_at(2ull, 0ull, 2ull, 0ull);
}

void test_alt_shift_up_copies_the_line_up_the_cursor_staying(void)
{
    fresh("1\n2\n3");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Alt+Down");
    EMBED_CALL(key, CliNameCfg, .name = "Alt+Shift+Up");
    assert_full_text("1\n2\n2\n2\n3");
    assert_at(2ull, 0ull, 2ull, 0ull);
}

void test_mod_shift_k_deletes_the_line_the_cursor_preserving_its_column(void)
{
    fresh("one\ntwo\nthree");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "End");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Shift+K");
    assert_full_text("one\nthree");
    assert_at(1ull, 3ull, 1ull, 3ull);
}

void test_the_last_line_deleted_removes_the_line_break_before_it(void)
{
    fresh("one\ntwo\nthree");
    EMBED_CALL(key, CliNameCfg, .name = "Down");
    EMBED_CALL(key, CliNameCfg, .name = "End");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Shift+K");
    EMBED_CALL(key, CliNameCfg, .name = "End");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Shift+K");
    assert_full_text("one");
    assert_at(0ull, 3ull, 0ull, 3ull);
}

void test_the_only_line_deleted_leaves_the_text_empty(void)
{
    fresh("one");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Shift+K");
    assert_full_text("");
    assert_at(0ull, 0ull, 0ull, 0ull);
}

void test_mod_bracket_indents_the_line_and_outdents_it(void)
{
    fresh("a\n\nb");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+]");
    assert_full_text("    a\n\nb");
    assert_at(0ull, 4ull, 0ull, 4ull);
    EMBED_CALL(key, CliNameCfg, .name = "Mod+[");
    assert_full_text("a\n\nb");
    assert_at(0ull, 0ull, 0ull, 0ull);
}

void test_the_lines_selected_indent_a_blank_one_left(void)
{
    fresh("a\n\nb");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+]");
    assert_full_text("    a\n\n    b");
    assert_at(2ull, 5ull, 0ull, 0ull);
    EMBED_CALL(key, CliNameCfg, .name = "Mod+[");
    assert_full_text("a\n\nb");
    assert_at(2ull, 1ull, 0ull, 0ull);
}

void test_tab_over_lines_indents_and_shift_tab_outdents(void)
{
    fresh("a\n\nb");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    EMBED_CALL(key, CliNameCfg, .name = "Tab");
    assert_full_text("    a\n\n    b");
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Tab");
    assert_full_text("a\n\nb");
}

void test_an_outdent_removes_a_tab(void)
{
    fresh("\tx");
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Tab");
    assert_full_text("x");
}

void test_mod_slash_marks_the_filled_lines_at_their_min_indentation(void)
{
    fresh("  a\n\n    b");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+/");
    assert_full_text("  // a\n\n  //   b");
    assert_at(2ull, 8ull, 0ull, 0ull);
}

void test_mod_slash_removes_the_marks_and_a_space_after_them(void)
{
    fresh("  a\n\n    b");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    key_times("Mod+/", 2u);
    assert_full_text("  a\n\n    b");
    assert_at(2ull, 5ull, 0ull, 0ull);
}

void test_undo_restores_the_marked_lines_as_one_step(void)
{
    fresh("  a\n\n    b");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    key_times("Mod+/", 2u);
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    assert_full_text("  // a\n\n  //   b");
}

void test_the_comment_command_marks_a_cursors_line(void)
{
    fresh("x");
    EMBED_CALL(key, CliNameCfg, .name = "comment");
    assert_full_text("// x");
    assert_at(0ull, 3ull, 0ull, 0ull);
}

void test_blank_lines_get_no_mark(void)
{
    fresh("\n\n");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+/");
    assert_full_text("\n\n");
}

void test_mod_slash_wraps_the_selection_in_a_languages_block_marks(void)
{
    fresh_named("x y", "check/view.html");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+/");
    assert_full_text("<!-- x y -->");
    assert_at(0ull, 8ull, 0ull, 5ull);
}

void test_comment_over_a_wrapped_selection_removes_the_marks_and_the_space_inside_each(void)
{
    fresh_named("x y", "check/view.html");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+/");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    EMBED_CALL(key, CliNameCfg, .name = "comment");
    assert_full_text("x y");
    assert_at(0ull, 3ull, 0ull, 0ull);
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Z");
    assert_full_text("<!-- x y -->");
}

void test_mod_shift_backslash_moves_end_the_bracket_paired_with_the_one_at_the_cursor(void)
{
    fresh("f(a[1]) b");
    EMBED_CALL(key, CliNameCfg, .name = "Right");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Shift+\\");
    assert_at(0ull, 7ull, 0ull, 7ull);
}

void test_the_bracket_command_works_from_an_inner_bracket(void)
{
    fresh("f(a[1]) b");
    key_times("Right", 3u);
    EMBED_CALL(key, CliNameCfg, .name = "bracket");
    assert_at(0ull, 6ull, 0ull, 6ull);
}

void test_where_no_bracket_is_the_cursor_stays(void)
{
    fresh("abc");
    EMBED_CALL(key, CliNameCfg, .name = "bracket");
    assert_at(0ull, 0ull, 0ull, 0ull);
}

void test_expand_selection_grows_through_the_word_the_brackets_and_the_complete_text(void)
{
    fresh("f(ab)");
    key_times("Right", 3u);
    EMBED_CALL(key, CliNameCfg, .name = "expand-selection");
    assert_at(0ull, 4ull, 0ull, 2ull);
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Alt+Right");
    assert_at(0ull, 5ull, 0ull, 1ull);
    EMBED_CALL(key, CliNameCfg, .name = "Alt+Shift+Right");
    assert_at(0ull, 5ull, 0ull, 0ull);
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Alt+Right");
    assert_at(0ull, 5ull, 0ull, 0ull);
}

void test_shrink_selection_steps_back_to_where_the_cursor_was_and_stays(void)
{
    fresh("f(ab)");
    key_times("Right", 3u);
    EMBED_CALL(key, CliNameCfg, .name = "expand-selection");
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Alt+Right");
    EMBED_CALL(key, CliNameCfg, .name = "Alt+Shift+Right");
    EMBED_CALL(key, CliNameCfg, .name = "shrink-selection");
    assert_at(0ull, 5ull, 0ull, 1ull);
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Alt+Left");
    assert_at(0ull, 4ull, 0ull, 2ull);
    EMBED_CALL(key, CliNameCfg, .name = "Alt+Shift+Left");
    assert_at(0ull, 3ull, 0ull, 3ull);
    EMBED_CALL(key, CliNameCfg, .name = "Shift+Alt+Left");
    assert_at(0ull, 3ull, 0ull, 3ull);
}

void test_a_selection_moved_since_it_grew_has_no_step_to_undo(void)
{
    fresh("f(ab)");
    key_times("Right", 3u);
    EMBED_CALL(key, CliNameCfg, .name = "expand-selection");
    EMBED_CALL(key, CliNameCfg, .name = "Right");
    EMBED_CALL(key, CliNameCfg, .name = "shrink-selection");
    assert_at(0ull, 4ull, 0ull, 4ull);
}

void test_mod_enter_opens_a_line_below_at_the_lines_indentation(void)
{
    fresh("  a\nb");
    EMBED_CALL(key, CliNameCfg, .name = "End");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Enter");
    assert_full_text("  a\n  \nb");
    assert_at(1ull, 2ull, 1ull, 2ull);
}

void test_mod_shift_enter_opens_a_line_above(void)
{
    fresh("  a\nb");
    EMBED_CALL(key, CliNameCfg, .name = "End");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Enter");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Shift+Enter");
    assert_full_text("  a\n  \n  \nb");
    assert_at(1ull, 2ull, 1ull, 2ull);
}

void test_mod_l_selects_the_line_again_the_next_the_last_to_its_end(void)
{
    fresh("a\nb\nc");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+L");
    assert_at(1ull, 0ull, 0ull, 0ull);
    EMBED_CALL(key, CliNameCfg, .name = "Mod+L");
    assert_at(2ull, 0ull, 0ull, 0ull);
    EMBED_CALL(key, CliNameCfg, .name = "select-line");
    assert_at(2ull, 1ull, 0ull, 0ull);
}

void test_mod_backspace_removes_the_word_before_the_cursor(void)
{
    fresh("foo bar");
    EMBED_CALL(key, CliNameCfg, .name = "End");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Backspace");
    assert_full_text("foo ");
    assert_at(0ull, 4ull, 0ull, 4ull);
}

void test_mod_delete_removes_the_word_after_the_cursor(void)
{
    fresh("foo ");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Delete");
    assert_full_text(" ");
    assert_at(0ull, 0ull, 0ull, 0ull);
}

void test_the_lines_sort_by_the_numbers_in_them_marks_before_digits_before_letters(void)
{
    fresh("b10\nB2\na\n_x\n9");
    EMBED_CALL(key, CliNameCfg, .name = "sort-ascending");
    assert_full_text("_x\n9\na\nB2\nb10");
    assert_at(4ull, 3ull, 0ull, 0ull);
    EMBED_CALL(key, CliNameCfg, .name = "sort-descending");
    assert_full_text("b10\nB2\na\n9\n_x");
}

void test_a_small_letter_sorts_before_its_capital(void)
{
    fresh("A\na\nb\na");
    EMBED_CALL(key, CliNameCfg, .name = "sort-ascending");
    assert_full_text("a\na\nA\nb");
}

void test_a_selection_sorts_its_own_lines(void)
{
    fresh("c\nb\na\nz");
    key_times("Shift+Down", 3u);
    EMBED_CALL(key, CliNameCfg, .name = "sort-ascending");
    assert_full_text("a\nb\nc\nz");
}

void test_the_duplicate_lines_are_removed_the_first_of_each_stored(void)
{
    fresh("a\nb\na\nc\nb");
    EMBED_CALL(key, CliNameCfg, .name = "unique-lines");
    assert_full_text("a\nb\nc");
}

void test_upper_case_changes_the_word_at_the_cursor(void)
{
    fresh("foo Bar");
    EMBED_CALL(key, CliNameCfg, .name = "upper-case");
    assert_full_text("FOO Bar");
    assert_at(0ull, 3ull, 0ull, 0ull);
}

void test_lower_case_and_title_case_change_the_selection(void)
{
    fresh("FOO Bar");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+A");
    EMBED_CALL(key, CliNameCfg, .name = "lower-case");
    assert_full_text("foo bar");
    EMBED_CALL(key, CliNameCfg, .name = "title-case");
    assert_full_text("Foo Bar");
}

void test_a_letter_end_ascii_changes_case(void)
{
    fresh("\xc3\xa9t\xc3\xa9");
    EMBED_CALL(key, CliNameCfg, .name = "upper-case");
    assert_full_text("\xc3\x89T\xc3\x89");
}

void test_a_cursor_on_no_word_changes_nothing(void)
{
    fresh(" a");
    EMBED_CALL(key, CliNameCfg, .name = "upper-case");
    assert_full_text(" a");
}

void test_a_key_that_adds_a_cursor_moves_none(void)
{
    fresh("0\n1\n2");
    EMBED_CALL(key, CliNameCfg, .name = "Mod+Alt+Down");
    assert_at(0ull, 0ull, 0ull, 0ull);
}

void test_a_name_the_view_has_no_use_for_does_nothing(void)
{
    fresh("0\n1\n2");
    EMBED_CALL(key, CliNameCfg, .name = "no-such-command");
    assert_at(0ull, 0ull, 0ull, 0ull);
}
