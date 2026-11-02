// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_keyed.c: a keyed store's listed size is exactly the bytes its keys and side values write
#include "ui/cli/src/command_line/cells.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/text/keyed.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliKeyed s_keyed;

// a key and its side value written to the line `context`, each with its terminating 0
static void listing_append(const CliEachKeyAdjacentTextSizeCfg *args)
{
    VerbaLinea *const line = args->context;
    (void)mmgr_line_put_n(line, args->key, CLI_UNITY_LENGTH(args->key) + 1ull);
    (void)mmgr_line_put_n(line, args->adjacent, CLI_UNITY_LENGTH(args->adjacent) + 1ull);
}

// the store lists to `required` bytes: a line of that size and its 0 holds them, and a line a byte shorter does not
static void assert_listing_size(unsigned long long required)
{
    const unsigned long long size = EMBED_CALL(cli_keyed_table.listed_size, CliKeyedCfg, .keyed = &s_keyed);
    TEST_ASSERT_EQUAL_UINT64(required, size);
    char text[256];
    VerbaLinea line = {text, size + 1ull, 0ull};
    EMBED_CALL(cli_keyed_each_table.each, CliKeyedEachCfg, .keyed = &s_keyed, .each = listing_append, .context = &line);
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
    TEST_ASSERT_EQUAL_UINT64(size, line.at);
    if (size != 0ull)
    {
        VerbaLinea short_line = {text, size, 0ull};
        EMBED_CALL(cli_keyed_each_table.each, CliKeyedEachCfg, .keyed = &s_keyed, .each = listing_append, .context = &short_line);
        TEST_ASSERT_FALSE(mmgr_line_ok(&short_line));
    }
}

void setUp(void)
{
    cli_unity_begun();
    EMBED_CALL(cli_keyed_name.begin, CliKeyedNameCfg, .keyed_out = &s_keyed, .name = "a test's texts", .entries_max = 16u);
}

void tearDown(void)
{
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &s_keyed);
}

void test_an_empty_store_lists_nothing(void)
{
    assert_listing_size(0ull);
}

void test_keys_and_side_values_count_with_their_terminators_and_values_do_not(void)
{
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_keyed, .key = "one", .adjacent_value = "beside", .text = "text", .size_value = 4ull);
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_keyed, .key = "three", .adjacent_value = "", .text = "", .size_value = 0ull);
    assert_listing_size(18ull);
}

void test_replacing_an_entry_counts_the_new_side_value(void)
{
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_keyed, .key = "one", .adjacent_value = "beside", .text = "text", .size_value = 4ull);
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_keyed, .key = "three", .adjacent_value = "", .text = "", .size_value = 0ull);
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_keyed, .key = "one", .adjacent_value = "b", .text = "a longer text than it was", .size_value = 25ull);
    assert_listing_size(13ull);
}

void test_a_removed_entry_no_longer_counts(void)
{
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_keyed, .key = "one", .adjacent_value = "b", .text = "text", .size_value = 4ull);
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_keyed, .key = "three", .adjacent_value = "", .text = "", .size_value = 0ull);
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &s_keyed, .key = "three");
    assert_listing_size(6ull);
}

void test_clearing_lists_nothing(void)
{
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_keyed, .key = "one", .adjacent_value = "b", .text = "text", .size_value = 4ull);
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &s_keyed);
    assert_listing_size(0ull);
}
