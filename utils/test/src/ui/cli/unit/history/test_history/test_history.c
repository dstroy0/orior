// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_history.c: Local History stores a snapshot each time a file is written, the text before it ahead of the first,
// none the same as the one before it, lists their stamps oldest first and reads one back
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/editor/history.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[CLI_PATH_MAX_LEN];
static char s_home[CLI_PATH_MAX_LEN];
static char s_out[256];
static char s_error[256];

void setUp(void)
{
    cli_unity_begun();
    // orior's home a folder of this test's own, made empty once for the process: no snapshot of an earlier run is read
    if (s_home[0] == '\0')
    {
        cli_unity_folder("history_home", s_home);
    }
    TEST_ASSERT_TRUE(
        EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = s_home));
}

void tearDown(void)
{
}

// `size` bytes of `text` stored as a.txt's newest snapshot, `before` ahead of the first: whether it was
static int snapshot_saved(const char *before, const char *text)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    return EMBED_CALL(cli_history_before.save, CliHistoryBeforeCfg, .root = s_root, .path = "a.txt", .before = before,
                      .before_size = (before != NULL) ? CLI_UNITY_LENGTH(before) : 0ull, .text = text,
                      .size = CLI_UNITY_LENGTH(text), .err_msg = &err_msg);
}

// the snapshot at `stamp` read into s_out
static const char *snapshot_read(unsigned long long stamp)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_history.read, CliHistoryCfg, .root = s_root, .path = "a.txt", .at = stamp,
                                .out = &out, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    return s_out;
}

void test_a_files_snapshots_are_stored_with_the_text_before_the_first_and_read_back_oldest_first(void)
{
    unsigned long long stamps[8];
    cli_unity_folder("history_tree", s_root);
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_history_stamps_table.stamps, CliHistoryStampsCfg, .root = s_root,
                                          .path = "a.txt", .stamps_out = stamps, .capacity = 8u));
    TEST_ASSERT_TRUE(snapshot_saved("one\n", "two\n"));
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_history_stamps_table.stamps, CliHistoryStampsCfg, .root = s_root,
                                          .path = "a.txt", .stamps_out = stamps, .capacity = 8u));
    TEST_ASSERT_TRUE(stamps[0] <= stamps[1]);
    TEST_ASSERT_EQUAL_STRING("one\n", snapshot_read(stamps[0]));
    TEST_ASSERT_EQUAL_STRING("two\n", snapshot_read(stamps[1]));
    // a text the same as the newest is not stored again; a changed one is, after the clock moves
    TEST_ASSERT_TRUE(snapshot_saved(NULL, "two\n"));
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_history_stamps_table.stamps, CliHistoryStampsCfg, .root = s_root,
                                          .path = "a.txt", .stamps_out = stamps, .capacity = 8u));
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 3u);
    TEST_ASSERT_TRUE(snapshot_saved(NULL, "three\n"));
    TEST_ASSERT_EQUAL_UINT(3u, EMBED_CALL(cli_history_stamps_table.stamps, CliHistoryStampsCfg, .root = s_root,
                                          .path = "a.txt", .stamps_out = stamps, .capacity = 8u));
    TEST_ASSERT_EQUAL_STRING("three\n", snapshot_read(stamps[2]));
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_history_stamps_table.stamps, CliHistoryStampsCfg, .root = s_root,
                                          .path = "a.txt", .stamps_out = stamps, .capacity = 1u));
}

void test_a_snapshot_no_file_has_is_refused_with_why(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("history_none", s_root);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_history.read, CliHistoryCfg, .root = s_root, .path = "b.txt", .at = 1ull,
                                 .out = &out, .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ull, err_msg.at);
}
