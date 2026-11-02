// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_raised.c: an elevated write tells a permission refusal from any other failure, and the helper copies the staged file
// over the target
#include "ui/cli/src/machine/raised.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_windows_access_denied_is_a_refusal(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_raised.is_reject, CliRaisedCfg, .err_text = "a.txt: Access is denied. (os error 5)"));
}

void test_permission_denied_is_a_refusal(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_raised.is_reject, CliRaisedCfg, .err_text = "a.txt: Permission denied (os error 13)"));
}

void test_read_only_file_system_is_a_refusal(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_raised.is_reject, CliRaisedCfg, .err_text = "a.txt: Read-only file system (os error 30)"));
}

void test_path_not_found_is_not_a_refusal(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_raised.is_reject, CliRaisedCfg, .err_text = "a.txt: The system cannot find the path specified. (os error 3)"));
}

void test_full_device_is_not_a_refusal(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_raised.is_reject, CliRaisedCfg, .err_text = "a.txt: No space left on device (os error 28)"));
}

void test_copy_over_replaces_the_target_with_the_staged_file(void)
{
    char folder[CLI_PATH_MAX_LEN];
    char staged[CLI_PATH_MAX_LEN];
    char target[CLI_PATH_MAX_LEN];
    cli_unity_folder("raised", folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "staged.txt", .joined = staged);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "target.txt", .joined = target);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = staged, .bytes = "new text", .size = 8ull));
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = target, .bytes = "old", .size = 3ull));
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_raised_from.copy_over, CliRaisedFromCfg, .from = staged, .to = target, .err_msg = &err_msg));
    char text[64];
    const unsigned long long size = qry_member_file_read(target, (unsigned char *)text, sizeof(text));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    CLI_TEST_ASSERT_TEXT("new text", text, size);
}
