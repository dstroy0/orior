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

void test_a_string_spans_its_inner_bytes_is_the_word_it_is_and_a_value_is_true_where_it_is(void)
{
    TEST_ASSERT_EQUAL_UINT64(4ull, EMBED_CALL(cli_reply_at.string_span, CliReplyAtCfg, .reply = &s_reply, .at = 13ull));
    TEST_ASSERT_EQUAL_UINT64(1ull, EMBED_CALL(cli_reply_at.string_span, CliReplyAtCfg, .reply = &s_reply, .at = 24ull));
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_reply_at.string_span, CliReplyAtCfg, .reply = &s_reply, .at = 6ull));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_reply_at.string_is, CliReplyAtCfg, .reply = &s_reply, .at = 24ull, .word_text = "c"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_reply_at.string_is, CliReplyAtCfg, .reply = &s_reply, .at = 24ull, .word_text = "d"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_reply_at.string_is, CliReplyAtCfg, .reply = &s_reply, .at = 6ull, .word_text = "1"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_reply_at.reply_true, CliReplyAtCfg, .reply = &s_reply, .at = 28ull));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_reply_at.reply_true, CliReplyAtCfg, .reply = &s_reply, .at = 6ull));
}

void test_a_reply_file_is_read_whole_and_let_go(void)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("reply_file", folder);
    cli_unity_file(folder, "reply.json", "{\"ok\":true}");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "reply.json", .joined = path);
    CliReply reply = {NULL, 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_reply_path.read, CliReplyPathCfg, .path = path, .reply_out = &reply));
    TEST_ASSERT_EQUAL_UINT64(11ull, reply.size);
    const unsigned long long ok = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply, .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &reply), .key = "ok");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_reply_at.reply_true, CliReplyAtCfg, .reply = &reply, .at = ok));
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &reply);
    TEST_ASSERT_NULL(reply.text);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "none.json", .joined = path);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_reply_path.read, CliReplyPathCfg, .path = path, .reply_out = &reply));
}
