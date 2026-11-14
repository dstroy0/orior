// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_recent.c: the files opened last and the commands run last: a push puts the newest first, a repeat moves to the
// front, the list stops at CLI_RECENT_MOST, the two lists stand apart, an empty entry or one with a line end changes
// nothing, a list asked for fewer answers its newest, and each list stands in orior's folder a line an entry
#include "ui/cli/src/machine/recent.h"
#include "ui/cli/src/machine/state.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the folder orior's state stands in for this suite, set once before the lists are first read
static char s_home[CLI_PATH_MAX_LEN];
static int s_home_set;

void setUp(void)
{
    cli_unity_begun();
    if (!s_home_set)
    {
        cli_unity_folder("build/harness/recent_home", s_home);
        TEST_ASSERT_TRUE(
            EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = s_home));
        s_home_set = 1;
    }
}

void tearDown(void)
{
}

// the list `list` into `rows`, sized for CLI_RECENT_MOST: how many; the rows stand in one cell let go at the next call
static char *s_listed_cell;
static unsigned int listed(unsigned int list, const char **rows)
{
    g_cli_guard->persistent_buf_release(s_listed_cell, g_cli_machine_owner);
    return EMBED_CALL(cli_recent.list, CliRecentCfg, .list = list, .most = CLI_RECENT_MOST, .rows_out = rows,
                      .cell_out = &s_listed_cell, .owner = g_cli_machine_owner);
}

void test_a_push_puts_the_newest_first(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "src/a.c");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "src/b.c");
    const char *rows[CLI_RECENT_MOST];
    const unsigned int count = listed(CLI_RECENT_FILES, rows);
    TEST_ASSERT_TRUE(count >= 2u);
    TEST_ASSERT_EQUAL_STRING("src/b.c", rows[0]);
    TEST_ASSERT_EQUAL_STRING("src/a.c", rows[1]);
}

void test_a_repeat_moves_to_the_front_and_stands_once(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "src/one.c");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "src/two.c");
    const char *rows[CLI_RECENT_MOST];
    const unsigned int before = listed(CLI_RECENT_FILES, rows);
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "src/one.c");
    const unsigned int after = listed(CLI_RECENT_FILES, rows);
    TEST_ASSERT_EQUAL_UINT(before, after);
    TEST_ASSERT_EQUAL_STRING("src/one.c", rows[0]);
    TEST_ASSERT_EQUAL_STRING("src/two.c", rows[1]);
    for (unsigned int at = 1u; at < after; at += 1u)
    {
        TEST_ASSERT_FALSE(
            cellul.eq(&(CatenaFinitaCfg){.src = rows[at], .other = "src/one.c", .cap = CLI_PATH_MAX_LEN}));
    }
}

void test_the_list_stops_at_its_most(void)
{
    char entry[32];
    for (unsigned int at = 0u; at < (CLI_RECENT_MOST + 5u); at += 1u)
    {
        VerbaLinea line = {entry, sizeof(entry), 0ULL};
        mmgr_line_put(&line, "file_");
        mmgr_line_decimal(&line, at, 2u);
        (void)mmgr_line_finish(&line);
        EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = entry);
    }
    const char *rows[CLI_RECENT_MOST];
    TEST_ASSERT_EQUAL_UINT(CLI_RECENT_MOST, listed(CLI_RECENT_FILES, rows));
    TEST_ASSERT_EQUAL_STRING("file_24", rows[0]);
    TEST_ASSERT_EQUAL_STRING("file_05", rows[CLI_RECENT_MOST - 1u]);
}

void test_the_files_and_the_commands_stand_apart(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "save");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "palette");
    const char *commands[CLI_RECENT_MOST];
    const char *files[CLI_RECENT_MOST];
    // the commands' rows in a cell of this test's own, since the helper's cell goes as the files are listed
    char *commands_cell = NULL;
    const unsigned int command_count =
        EMBED_CALL(cli_recent.list, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .most = CLI_RECENT_MOST,
                   .rows_out = commands, .cell_out = &commands_cell, .owner = g_cli_machine_owner);
    const unsigned int file_count = listed(CLI_RECENT_FILES, files);
    TEST_ASSERT_EQUAL_UINT(2u, command_count);
    TEST_ASSERT_EQUAL_STRING("palette", commands[0]);
    TEST_ASSERT_EQUAL_STRING("save", commands[1]);
    for (unsigned int at = 0u; at < file_count; at += 1u)
    {
        TEST_ASSERT_FALSE(cellul.eq(&(CatenaFinitaCfg){.src = files[at], .other = "palette", .cap = CLI_PATH_MAX_LEN}));
    }
    g_cli_guard->persistent_buf_release(commands_cell, g_cli_machine_owner);
}

void test_an_empty_entry_or_one_with_a_line_end_changes_nothing(void)
{
    const char *rows[CLI_RECENT_MOST];
    const unsigned int before = listed(CLI_RECENT_COMMANDS, rows);
    char first[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = first,
                            .src = rows[0],
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = rows[0], .cap = CLI_PATH_MAX_LEN}) + 1u});
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "two\nlines");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_LISTS, .entry = "no list");
    TEST_ASSERT_EQUAL_UINT(before, listed(CLI_RECENT_COMMANDS, rows));
    TEST_ASSERT_EQUAL_STRING(first, rows[0]);
    char *none = NULL;
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_recent.list, CliRecentCfg, .list = CLI_RECENT_LISTS, .most = 3u,
                                          .rows_out = rows, .cell_out = &none, .owner = g_cli_machine_owner));
    TEST_ASSERT_NULL(none);
}

void test_a_list_asked_for_fewer_answers_its_newest(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "line");
    const char *rows[2];
    char *cell = NULL;
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_recent.list, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .most = 1u,
                                          .rows_out = rows, .cell_out = &cell, .owner = g_cli_machine_owner));
    TEST_ASSERT_EQUAL_STRING("line", rows[0]);
    g_cli_guard->persistent_buf_release(cell, g_cli_machine_owner);
}

void test_each_list_stands_in_orior_folder_a_line_an_entry(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "file");
    CliStateEntries entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = s_home, .owner = g_cli_machine_owner});
    TEST_ASSERT_NOT_NULL(entries.cell);
    char commands_text[256];
    char files_text[CLI_PATH_MAX_LEN];
    TEST_ASSERT_EQUAL_STRING("file\nline\npalette\nsave",
                             cli_state_entry_value(&entries, "recent.commands", commands_text, sizeof(commands_text)));
    const char *const files = cli_state_entry_value(&entries, "recent.files", files_text, sizeof(files_text));
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL_UINT8('f', (unsigned char)files[0]);
    cli_state_entries_release(&entries);
}
