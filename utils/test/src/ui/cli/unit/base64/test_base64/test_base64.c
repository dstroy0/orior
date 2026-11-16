// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_base64.c: bytes written as base64 with `=` to pad and read back, whitespace passed over, and a text that is not
// base64 or does not fit refused
#include "ui/cli/src/text/base64.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// `size` bytes written as base64 into s_written, NUL-terminated
static char s_written[64];
static const char *written_of(const unsigned char *bytes, unsigned long long size)
{
    VerbaLinea out = mmgr_line_over(s_written, sizeof(s_written));
    EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &out, .bytes = bytes, .size = size);
    (void)mmgr_line_finish(&out);
    return s_written;
}

void test_bytes_are_written_with_the_pad_each_remainder_takes(void)
{
    TEST_ASSERT_EQUAL_STRING("", written_of((const unsigned char *)"", 0ull));
    TEST_ASSERT_EQUAL_STRING("Zg==", written_of((const unsigned char *)"f", 1ull));
    TEST_ASSERT_EQUAL_STRING("Zm8=", written_of((const unsigned char *)"fo", 2ull));
    TEST_ASSERT_EQUAL_STRING("Zm9v", written_of((const unsigned char *)"foo", 3ull));
    TEST_ASSERT_EQUAL_STRING("Zm9vYmFy", written_of((const unsigned char *)"foobar", 6ull));
}

void test_base64_is_read_back_with_whitespace_passed_over(void)
{
    unsigned char out[16];
    unsigned long long size = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = "Zm9v\nYmFy", .length = 9ull, .out_bytes = out, .capacity = sizeof(out), .size_pointer = &size));
    TEST_ASSERT_EQUAL_UINT64(6ull, size);
    TEST_ASSERT_EQUAL_MEMORY("foobar", out, 6u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = "Zg==", .length = 4ull, .out_bytes = out, .capacity = sizeof(out), .size_pointer = &size));
    TEST_ASSERT_EQUAL_UINT64(1ull, size);
    TEST_ASSERT_EQUAL_UINT8('f', out[0]);
}

void test_a_text_that_is_not_base64_or_does_not_fit_is_refused(void)
{
    unsigned char out[4];
    unsigned long long size = 0ull;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = "Zm9v*mFy", .length = 8ull, .out_bytes = out, .capacity = sizeof(out), .size_pointer = &size));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = "Zm9vYmFy", .length = 8ull, .out_bytes = out, .capacity = sizeof(out), .size_pointer = &size));
}
