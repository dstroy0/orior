// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_typed.c: the bytes typed into a box stop at a control character, wherever in the text it stands
#include "ui/cli/src/text/typed.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_the_bytes_put_stop_at_a_control_past_a_words_width(void)
{
    CliTyped typed = {NULL, 0ull};
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &typed, .text = "abcdefghij\tkl", .size = 13ull, .max = 64ull,
               .owner = g_cli_text_owner);
    char copied[64];
    (void)EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &typed, .dst = copied, .capacity = sizeof(copied));
    TEST_ASSERT_EQUAL_UINT64(10ull, typed.length);
    TEST_ASSERT_EQUAL_STRING("abcdefghij", copied);
    EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = &typed, .owner = g_cli_text_owner);
}

void test_a_control_first_puts_nothing(void)
{
    CliTyped typed = {NULL, 0ull};
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &typed, .text = "\nabc", .size = 4ull, .max = 64ull,
               .owner = g_cli_text_owner);
    TEST_ASSERT_NULL(typed.cell);
    TEST_ASSERT_EQUAL_UINT64(0ull, typed.length);
}

void test_a_byte_past_ascii_is_put_whole(void)
{
    CliTyped typed = {NULL, 0ull};
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &typed, .text = "abcdefghij\xc3\xa9", .size = 12ull, .max = 64ull,
               .owner = g_cli_text_owner);
    TEST_ASSERT_EQUAL_UINT64(12ull, typed.length);
    EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = &typed, .owner = g_cli_text_owner);
}

void test_an_erase_takes_the_last_character_whole_and_none_from_nothing(void)
{
    CliTyped typed = {NULL, 0ull};
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &typed, .text = "ab\xc3\xa9", .size = 4ull, .max = 64ull,
               .owner = g_cli_text_owner);
    EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &typed);
    char copied[8];
    TEST_ASSERT_EQUAL_UINT64(2ull, EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &typed, .dst = copied, .capacity = sizeof(copied)));
    TEST_ASSERT_EQUAL_STRING("ab", copied);
    EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &typed);
    EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &typed);
    TEST_ASSERT_EQUAL_UINT64(0ull, typed.length);
    EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &typed);
    TEST_ASSERT_EQUAL_UINT64(0ull, typed.length);
    EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = &typed, .owner = g_cli_text_owner);
}
