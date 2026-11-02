// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_text_walks.c: the scans over a text's bytes answer as a scan of one byte at a time does, from every position and over every
// length around a lane's and a word's width, and read nothing past their bounds
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define WALK_BYTES 300u

static char s_text[WALK_BYTES + 1u];

static int byte_in_set(char byte, const char *set)
{
    for (const char *one = set; *one != '\0'; one += 1)
    {
        if (*one == byte)
        {
            return 1;
        }
    }
    return 0;
}

// the text filled with a newline every `every` bytes, 0 for none, a word's and a lane's scan turning on them
static void text_filled(unsigned int every)
{
    for (unsigned int at = 0u; at < WALK_BYTES; at += 1u)
    {
        s_text[at] = ((every != 0u) && ((at % every) == (every - 1u))) ? '\n' : ((at % 2u) != 0u) ? ' ' : 'x';
    }
    s_text[WALK_BYTES] = '\n';
}

// the four scans over `set` answer as a scan of one byte at a time, for every position and length
static void assert_scans_agree(const char *set, unsigned int every)
{
    text_filled(every);
    for (unsigned int size = 0u; size <= WALK_BYTES; size += (size < 80u) ? 1u : 37u)
    {
        for (unsigned int at = 0u; at <= size; at += (at < 80u) ? 1u : 29u)
        {
            unsigned int to = at;
            unsigned int past = at;
            unsigned int back = at;
            unsigned int back_past = at;
            while ((to < size) && !byte_in_set(s_text[to], set))
            {
                to += 1u;
            }
            while ((past < size) && byte_in_set(s_text[past], set))
            {
                past += 1u;
            }
            while ((back > 0u) && !byte_in_set(s_text[back - 1u], set))
            {
                back -= 1u;
            }
            while ((back_past > 0u) && byte_in_set(s_text[back_past - 1u], set))
            {
                back_past -= 1u;
            }
            TEST_ASSERT_EQUAL_UINT64(to, EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = s_text, .at = at, .size = size, .set = set));
            TEST_ASSERT_EQUAL_UINT64(past, EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = s_text, .at = at, .size = size, .set = set));
            TEST_ASSERT_EQUAL_UINT64(back, EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = s_text, .at = at, .set = set));
            TEST_ASSERT_EQUAL_UINT64(back_past, EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = s_text, .at = at, .set = set));
        }
    }
}

void setUp(void)
{
    cli_unity_begun();
    text_filled(0u);
}

void tearDown(void)
{
}

void test_a_byte_the_text_does_not_contain_scans_to_each_end(void)
{
    assert_scans_agree("\n", 0u);
}

void test_one_byte_is_found_at_its_first_and_last_position(void)
{
    assert_scans_agree("\n", 7u);
}

void test_one_byte_is_found_past_a_lanes_width(void)
{
    assert_scans_agree("\n", 97u);
}

void test_a_set_of_two_scans_to_the_first_and_past_the_run(void)
{
    assert_scans_agree(" x", 7u);
}

void test_the_whitespace_set_agrees(void)
{
    assert_scans_agree(CLI_TEXT_WHITE, 31u);
}

void test_a_position_past_the_bound_stays_for_the_forward_scan(void)
{
    TEST_ASSERT_EQUAL_UINT64(9ull, EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = s_text, .at = 9u, .size = 3u, .set = "x"));
}

void test_a_position_past_the_bound_stays_for_the_past_scan(void)
{
    TEST_ASSERT_EQUAL_UINT64(9ull, EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = s_text, .at = 9u, .size = 3u, .set = "x"));
}

void test_a_line_stops_at_its_newline_and_cuts_before_its_carriage_return(void)
{
    const CliTextLine line = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = "ab\r\ncd\n\ne\r", .at = 0ull, .size = 11ull);
    TEST_ASSERT_EQUAL_UINT64(3ull, line.stop);
    TEST_ASSERT_EQUAL_UINT64(2ull, line.cut);
    TEST_ASSERT_EQUAL_UINT64(4ull, line.next);
}

void test_a_line_with_no_carriage_return_cuts_where_it_stops(void)
{
    const CliTextLine line = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = "ab\r\ncd\n\ne\r", .at = 4ull, .size = 11ull);
    TEST_ASSERT_EQUAL_UINT64(6ull, line.stop);
    TEST_ASSERT_EQUAL_UINT64(6ull, line.cut);
    TEST_ASSERT_EQUAL_UINT64(7ull, line.next);
}

void test_an_empty_line_has_no_letters(void)
{
    const CliTextLine line = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = "ab\r\ncd\n\ne\r", .at = 7ull, .size = 11ull);
    TEST_ASSERT_EQUAL_UINT64(7ull, line.stop);
    TEST_ASSERT_EQUAL_UINT64(7ull, line.cut);
    TEST_ASSERT_EQUAL_UINT64(8ull, line.next);
}

void test_a_last_line_with_no_newline_stops_at_the_end(void)
{
    const CliTextLine line = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = "ab\r\ncd\n\ne\r", .at = 8ull, .size = 11ull);
    TEST_ASSERT_EQUAL_UINT64(11ull, line.stop);
    TEST_ASSERT_EQUAL_UINT64(11ull, line.cut);
    TEST_ASSERT_EQUAL_UINT64(11ull, line.next);
}

void test_a_text_is_copied(void)
{
    char copied[8];
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_room = copied, .text = "ab", .otherwise = "c");
    TEST_ASSERT_EQUAL_STRING("ab", copied);
}

void test_the_fallback_is_copied_where_the_text_is_null(void)
{
    char copied[8];
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_room = copied, .text = NULL, .otherwise = "c");
    TEST_ASSERT_EQUAL_STRING("c", copied);
}

void test_letters_are_raised_to_upper_case(void)
{
    char upper[4] = "aZ9";
    EMBED_CALL(cli_text_size_from.raise, CliTextSizeFromCfg, .text_out = upper, .size = 3ull);
    TEST_ASSERT_EQUAL_STRING("AZ9", upper);
}

void test_a_number_reads_its_digits_and_reports_where_they_stop(void)
{
    unsigned long long past = 0ull;
    TEST_ASSERT_EQUAL_UINT64(42ull, EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = "  42x", .at = 2ull, .size = 5ull, .end = &past));
    TEST_ASSERT_EQUAL_UINT64(4ull, past);
}

void test_a_pair_is_found_where_it_is(void)
{
    TEST_ASSERT_EQUAL_UINT64(2ull, EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = "a--b", .at = 0ull, .size = 4ull, .first = '-', .second = 'b'));
}

void test_a_pair_that_is_absent_reports_the_end(void)
{
    TEST_ASSERT_EQUAL_UINT64(4ull, EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = "a--b", .at = 0ull, .size = 4ull, .first = 'x', .second = 'y'));
}

void test_a_scan_past_classes_stops_at_the_first_byte_of_neither(void)
{
    TEST_ASSERT_EQUAL_UINT64(3ull, EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = "ab1 ", .at = 0ull, .size = 4ull, .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS, .set = ""));
}
