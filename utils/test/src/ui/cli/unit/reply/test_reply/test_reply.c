// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_reply.c: a JSON reply's values are passed over by their brackets, the brackets and quotes inside a string no
// part of the count
#include "ui/cli/src/text/reply.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// an object whose array holds an object whose string holds brackets and an escaped quote
static const char s_text[] = "{\"a\":[1,{\"b\":\"]}\\\"\"},3],\"c\":true}";
static const CliReply s_reply = {s_text, sizeof(s_text) - 1u};

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_the_end_of_the_whole_reply_is_its_size(void)
{
    TEST_ASSERT_EQUAL_UINT64(s_reply.size, cli_reply_value_end(&(CliReplyAtCfg){.reply = &s_reply, .at = 0ull}));
}

void test_the_end_of_an_array_is_past_its_bracket(void)
{
    TEST_ASSERT_EQUAL_UINT64(23ull, cli_reply_value_end(&(CliReplyAtCfg){.reply = &s_reply, .at = 5ull}));
}

void test_the_end_of_an_object_passes_the_brackets_inside_its_string(void)
{
    TEST_ASSERT_EQUAL_UINT64(20ull, cli_reply_value_end(&(CliReplyAtCfg){.reply = &s_reply, .at = 8ull}));
}

void test_the_end_of_a_string_is_past_its_closing_quote(void)
{
    TEST_ASSERT_EQUAL_UINT64(19ull, cli_reply_value_end(&(CliReplyAtCfg){.reply = &s_reply, .at = 13ull}));
}

void test_the_end_of_a_word_is_at_the_byte_that_follows_it(void)
{
    TEST_ASSERT_EQUAL_UINT64(32ull, cli_reply_value_end(&(CliReplyAtCfg){.reply = &s_reply, .at = 28ull}));
}

void test_an_unclosed_array_ends_at_the_size(void)
{
    const char text[] = "[1,[2,3],\"x\"";
    const CliReply reply = {text, sizeof(text) - 1u};
    TEST_ASSERT_EQUAL_UINT64(reply.size, cli_reply_value_end(&(CliReplyAtCfg){.reply = &reply, .at = 0ull}));
}
