// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_recent.c: the files opened last and the commands run last: a push puts the newest first, a repeat moves to the
// front, the list stops at CLI_RECENT_MAX, the two lists are apart, an empty entry or one with a line end changes
// nothing, a list asked for fewer answers its newest, and each list is in orior's folder a line an entry
#include "ui/cli/src/machine/recent.h"
#include "ui/cli/src/machine/state.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the folder orior's state is in for this suite, set once before the lists are first read
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
// The test's copy of the rows the list passes. It holds the text of each row, the rows that point into it
// and how many calls the copier took
// the calls to the copier
typedef struct
{
    char (*text)[CLI_PATH_MAX_LEN];
    const char **rows;
    unsigned int calls;
} RecentCopy;

// each row the list passes copied into the test's text, and the row pointed at its copy
static void recent_copied(const CliEachRowsCountCfg *args)
{
    RecentCopy *const copy = args->context;
    copy->calls += 1u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = args->rows[at], .cap = CLI_PATH_MAX_LEN - 1u});
        memor.cpy(&(MemoriaCfg){.dst = copy->text[at], .src = args->rows[at], .bytes = length});
        copy->text[at][length] = '\0';
        copy->rows[at] = copy->text[at];
    }
}

// the list `list`, at max `max` rows, copied into `text` and `rows`: how many
static unsigned int listed_into(unsigned int list, unsigned int max, char (*text)[CLI_PATH_MAX_LEN], const char **rows)
{
    RecentCopy copy = {text, rows, 0u};
    return EMBED_CALL(cli_recent.each, CliRecentCfg, .list = list, .max = max, .each = recent_copied, .context = &copy);
}

// the list `list` into `rows`, sized for CLI_RECENT_MAX: how many. The rows point into the suite's one copy, and the
// next call writes over it
static char s_listed[CLI_RECENT_MAX][CLI_PATH_MAX_LEN];
static unsigned int listed(unsigned int list, const char **rows)
{
    return listed_into(list, CLI_RECENT_MAX, s_listed, rows);
}

void test_a_push_puts_the_newest_first(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "src/a.c");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "src/b.c");
    const char *rows[CLI_RECENT_MAX];
    const unsigned int count = listed(CLI_RECENT_FILES, rows);
    TEST_ASSERT_TRUE(count >= 2u);
    TEST_ASSERT_EQUAL_STRING("src/b.c", rows[0]);
    TEST_ASSERT_EQUAL_STRING("src/a.c", rows[1]);
}

void test_a_repeat_moves_to_the_front_and_is_once(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "src/one.c");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = "src/two.c");
    const char *rows[CLI_RECENT_MAX];
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

void test_the_list_stops_at_its_max(void)
{
    char entry[32];
    for (unsigned int at = 0u; at < (CLI_RECENT_MAX + 5u); at += 1u)
    {
        VerbaLinea line = {entry, sizeof(entry), 0ULL};
        mmgr_line_put(&line, "file_");
        mmgr_line_decimal(&line, at, 2u);
        (void)mmgr_line_finish(&line);
        EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_FILES, .entry = entry);
    }
    const char *rows[CLI_RECENT_MAX];
    TEST_ASSERT_EQUAL_UINT(CLI_RECENT_MAX, listed(CLI_RECENT_FILES, rows));
    TEST_ASSERT_EQUAL_STRING("file_24", rows[0]);
    TEST_ASSERT_EQUAL_STRING("file_05", rows[CLI_RECENT_MAX - 1u]);
}

void test_the_files_and_the_commands_are_apart(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "save");
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "palette");
    const char *commands[CLI_RECENT_MAX];
    const char *files[CLI_RECENT_MAX];
    // the commands' rows in a copy of this test's own: the helper writes over its copy when it lists the files
    static char s_commands[CLI_RECENT_MAX][CLI_PATH_MAX_LEN];
    const unsigned int command_count = listed_into(CLI_RECENT_COMMANDS, CLI_RECENT_MAX, s_commands, commands);
    const unsigned int file_count = listed(CLI_RECENT_FILES, files);
    TEST_ASSERT_EQUAL_UINT(2u, command_count);
    TEST_ASSERT_EQUAL_STRING("palette", commands[0]);
    TEST_ASSERT_EQUAL_STRING("save", commands[1]);
    for (unsigned int at = 0u; at < file_count; at += 1u)
    {
        TEST_ASSERT_FALSE(cellul.eq(&(CatenaFinitaCfg){.src = files[at], .other = "palette", .cap = CLI_PATH_MAX_LEN}));
    }
}

void test_an_empty_entry_or_one_with_a_line_end_changes_nothing(void)
{
    const char *rows[CLI_RECENT_MAX];
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
    // a list past the lists answers none and does not go to the copier
    RecentCopy none = {s_listed, rows, 0u};
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_recent.each, CliRecentCfg, .list = CLI_RECENT_LISTS, .max = 3u,
                                          .each = recent_copied, .context = &none));
    TEST_ASSERT_EQUAL_UINT(0u, none.calls);
}

void test_a_list_asked_for_fewer_answers_its_newest(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "line");
    const char *rows[2];
    TEST_ASSERT_EQUAL_UINT(1u, listed_into(CLI_RECENT_COMMANDS, 1u, s_listed, rows));
    TEST_ASSERT_EQUAL_STRING("line", rows[0]);
}

void test_each_list_is_in_orior_folder_a_line_an_entry(void)
{
    EMBED_CALL(cli_recent.push, CliRecentCfg, .list = CLI_RECENT_COMMANDS, .entry = "file");
    CliStateEntries entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = s_home, .owner = g_cli_machine_owner});
    TEST_ASSERT_NOT_NULL(entries.cell);
    char commands_text[256];
    char files_text[CLI_PATH_MAX_LEN];
    TEST_ASSERT_EQUAL_STRING(
        "file\nline\npalette\nsave",
        cli_state_entry_value(&(CliStateEntryCfg){
            .entries = &entries, .key = "recent.commands", .out = commands_text, .capacity = sizeof(commands_text)}));
    const char *const files = cli_state_entry_value(&(CliStateEntryCfg){
        .entries = &entries, .key = "recent.files", .out = files_text, .capacity = sizeof(files_text)});
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL_UINT8('f', (unsigned char)files[0]);
    cli_state_entries_release(&(CliStateEntriesCfg){.entries = &entries});
}
