// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_servers_check.c: the tree check stores every file of a tree and lists them in order, its memory is sized to those files, and
// stopping the check releases all of it
#include "ui/cli/src/servers/servers_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// more files than the check's first allocation fits, each named by its index
#define CHECK_FILES 5000u

static char s_names[CHECK_FILES][64];
static const char *s_files[CHECK_FILES];
static char s_owners[1u << 18u];
static char s_root[CLI_PATH_MAX_LEN];

// the servers' events are ignored
static void events_ignored(const CliEachEventJsonSizeCfg *args)
{
    (void)args;
}

// the bytes in use by the tree check's owners, as the memory audit counts them
static unsigned long long check_bytes_in_use(void)
{
    VerbaLinea out = {s_owners, sizeof(s_owners), 0ull};
    cli_pool_owners_write(&out);
    (void)mmgr_line_finish(&out);
    static const char s_named[] = "the tree's check";
    unsigned long long in_use = 0ull;
    const char *line = s_owners;
    while (*line != '\0')
    {
        const char *tab = line;
        while ((*tab != '\0') && (*tab != '\t'))
        {
            tab += 1u;
        }
        if ((*tab == '\t') && (memor.cmp(&(MemoriaCfg){.src = line, .other = s_named, .bytes = sizeof(s_named) - 1u}) == 0))
        {
            unsigned long long value = 0ull;
            for (const char *digit = tab + 1u; (*digit >= '0') && (*digit <= '9'); digit += 1u)
            {
                value = (value * 10ull) + (unsigned long long)(*digit - '0');
            }
            in_use += value;
        }
        while ((*line != '\0') && (*line != '\n'))
        {
            line += 1u;
        }
        line += (*line != '\0') ? 1u : 0u;
    }
    return in_use;
}

void setUp(void)
{
    cli_unity_begun();
    cli_unity_folder("servers_check", s_root);
    for (unsigned int at = 0u; at < CHECK_FILES; at += 1u)
    {
        VerbaLinea name = {s_names[at], sizeof(s_names[at]), 0ull};
        mmgr_line_put(&name, "notes/a_note_of_the_tree_numbered_");
        mmgr_line_decimal(&name, at, 5u);
        mmgr_line_put(&name, ".txt");
        (void)mmgr_line_finish(&name);
        s_files[at] = s_names[at];
    }
    CliServersSink sink = {.emit = events_ignored};
    (void)EMBED_CALL(cli_servers_root_sink_count.check_tree, CliServersRootSinkCountCfg, .root = s_root, .files = s_files, .count = CHECK_FILES, .sink = &sink);
}

void tearDown(void)
{
    cli_servers_check_let_go();
}

void test_every_file_is_listing_in_order(void)
{
    unsigned long long listed_size = 0ull;
    char *const list_entries = EMBED_CALL(cli_servers_root_size_internal.check_tree_files, CliServersRootSizeCtx, .root = s_root, .size_pointer = &listed_size);
    TEST_ASSERT_NOT_NULL(list_entries);
    unsigned int count = 0u;
    for (unsigned long long at = 0ull; at < listed_size; count += 1u)
    {
        const char *const file = list_entries + at;
        TEST_ASSERT_LESS_THAN_UINT(CHECK_FILES, count);
        TEST_ASSERT_EQUAL_STRING(s_names[count], file);
        at += CLI_UNITY_LENGTH(file) + 1ull;
    }
    TEST_ASSERT_EQUAL_UINT(CHECK_FILES, count);
    g_cli_guard->persistent_buf_release(list_entries, g_cli_servers_owner);
}

void test_another_tree_lists_no_files(void)
{
    unsigned long long listed_size = 1ull;
    TEST_ASSERT_NULL(EMBED_CALL(cli_servers_root_size_internal.check_tree_files, CliServersRootSizeCtx, .root = "another tree", .size_pointer = &listed_size));
    TEST_ASSERT_EQUAL_UINT64(0ull, listed_size);
}

void test_the_checks_memory_is_sized_to_its_files(void)
{
    TEST_ASSERT_LESS_THAN_UINT64(1ull << 20u, check_bytes_in_use());
}

void test_stopping_the_check_releases_its_memory(void)
{
    cli_servers_check_let_go();
    TEST_ASSERT_EQUAL_UINT64(0ull, check_bytes_in_use());
}
