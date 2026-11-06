// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_crumbs.c: the crumbs of a file shown: its folders, the file and the declarations a line stands inside, outermost
// first, for a nested outline; none of the declarations before the first; the declarations beside one, those at its
// depth under the one it stands in and at the top those of the file, each running symbol-go with its line; the line a
// word reads as; and the setting set off answering no crumbs, standing across the state's save and read
#include "ui/cli/src/code/crumbs.h"
#include "ui/cli/src/command_line/pool.h"
#include "ui/cli/src/command_line/commands.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#include <stdlib.h>
#include <string.h>

static const char s_key[] = "crumbs_check";
static const char s_path[] = "src/code/a.py";
static const char s_text[] = "import os\n"
                             "\n"
                             "class Box:\n"
                             "    def put(self):\n"
                             "        return 1\n"
                             "\n"
                             "    def take(self):\n"
                             "        return 2\n"
                             "\n"
                             "def solo():\n"
                             "    return 3\n";

void setUp(void)
{
    cli_unity_begun();
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = s_text,
                                .size = sizeof(s_text) - 1u, .shape = &shape));
    EMBED_CALL(cli_crumbs.begin, CliCrumbsCfg, .entries = NULL);
}

void tearDown(void)
{
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .path = NULL);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
}

// the crumb `at` asserted of `kind` reading `text`
static void crumb_is(unsigned int at, unsigned int kind, const char *text)
{
    CliCrumbRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_crumbs.row, CliCrumbsCfg, .at = at, .row = &row));
    TEST_ASSERT_EQUAL_UINT(kind, row.kind);
    CLI_TEST_ASSERT_TEXT(text, row.text, row.length);
}

// the menu row `at` asserted to read `label` and run `command` with `word`
static void menu_row_is(unsigned int at, const char *label, const char *command, const char *word)
{
    unsigned int count = 0u;
    const CliItem *const rows = cli_menus_own_rows(&count);
    TEST_ASSERT_TRUE(at < count);
    TEST_ASSERT_EQUAL_STRING(label, rows[at].label);
    TEST_ASSERT_EQUAL_STRING(command, rows[at].command);
    TEST_ASSERT_EQUAL_STRING(word, rows[at].args);
}

void test_read_lists_folders_file_and_the_declarations_a_line_stands_inside(void)
{
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = s_key, .path = s_path, .line = 4ULL);
    TEST_ASSERT_EQUAL_UINT(5u, EMBED_CALL(cli_crumbs.rows, CliCrumbsCfg, .at = 0u));
    crumb_is(0u, CLI_CRUMB_FOLDER, "src");
    crumb_is(1u, CLI_CRUMB_FOLDER, "code");
    crumb_is(2u, CLI_CRUMB_FILE, "a.py");
    crumb_is(3u, CLI_CRUMB_SYMBOL, "Box");
    crumb_is(4u, CLI_CRUMB_SYMBOL, "put");
    CliCrumbRow row;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_crumbs.row, CliCrumbsCfg, .at = 5u, .row = &row));
}

void test_a_line_inside_the_next_declaration_reads_it(void)
{
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = s_key, .path = s_path, .line = 4ULL);
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = s_key, .path = s_path, .line = 10ULL);
    TEST_ASSERT_EQUAL_UINT(4u, EMBED_CALL(cli_crumbs.rows, CliCrumbsCfg, .at = 0u));
    crumb_is(3u, CLI_CRUMB_SYMBOL, "solo");
}

void test_a_line_before_the_first_declaration_lists_none(void)
{
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = s_key, .path = s_path, .line = 1ULL);
    TEST_ASSERT_EQUAL_UINT(3u, EMBED_CALL(cli_crumbs.rows, CliCrumbsCfg, .at = 0u));
    crumb_is(2u, CLI_CRUMB_FILE, "a.py");
}

void test_no_key_lists_the_folders_and_the_file_alone(void)
{
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = NULL, .path = "top.txt", .line = 0ULL);
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_crumbs.rows, CliCrumbsCfg, .at = 0u));
    crumb_is(0u, CLI_CRUMB_FILE, "top.txt");
}

void test_the_menu_of_a_declaration_lists_those_at_its_depth_under_the_same_one(void)
{
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = s_key, .path = s_path, .line = 4ULL);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_crumbs.menu, CliCrumbsCfg, .at = 4u));
    unsigned int count = 0u;
    (void)cli_menus_own_rows(&count);
    TEST_ASSERT_EQUAL_UINT(2u, count);
    menu_row_is(0u, "put", "symbol-go", "3");
    menu_row_is(1u, "take", "symbol-go", "6");
}

void test_the_menu_of_a_declaration_at_the_top_lists_the_files(void)
{
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = s_key, .path = s_path, .line = 4ULL);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_crumbs.menu, CliCrumbsCfg, .at = 3u));
    unsigned int count = 0u;
    (void)cli_menus_own_rows(&count);
    TEST_ASSERT_EQUAL_UINT(2u, count);
    menu_row_is(0u, "Box", "symbol-go", "2");
    menu_row_is(1u, "solo", "symbol-go", "9");
}

void test_go_answers_the_line_a_word_reads_as(void)
{
    CliPosition place = {0ULL, 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_crumbs.go, CliCrumbsCfg, .text = "42", .place_out = &place));
    TEST_ASSERT_EQUAL_UINT64(42ULL, place.line);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_crumbs.go, CliCrumbsCfg, .text = "4x", .place_out = &place));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_crumbs.go, CliCrumbsCfg, .text = "", .place_out = &place));
}

void test_the_setting_off_lists_none_and_stands_across_the_save_and_read(void)
{
    char home[CLI_PATH_MAX_LEN];
    cli_unity_folder("crumbs_home", home);
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", home));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", home, 1));
#endif
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = s_key, .path = s_path, .line = 4ULL);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_crumbs.shown, CliCrumbsCfg, .at = 0u));
    EMBED_CALL(cli_crumbs.shown_set, CliCrumbsCfg, .on = 0);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_crumbs.shown, CliCrumbsCfg, .at = 0u));
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_crumbs.rows, CliCrumbsCfg, .at = 0u));
    CliStateEntries *const entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = home, .owner = g_checks_owner});
    TEST_ASSERT_NOT_NULL(entries);
    EMBED_CALL(cli_crumbs.begin, CliCrumbsCfg, .entries = entries);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_crumbs.shown, CliCrumbsCfg, .at = 0u));
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = s_key, .path = s_path, .line = 4ULL);
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_crumbs.rows, CliCrumbsCfg, .at = 0u));
    g_cli_guard->persistent_buf_release(entries, g_checks_owner);
    EMBED_CALL(cli_crumbs.shown_set, CliCrumbsCfg, .on = 1);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_crumbs.shown, CliCrumbsCfg, .at = 0u));
}
