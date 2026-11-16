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
            unsigned int end = at;
            unsigned int back = at;
            unsigned int back_end = at;
            while ((to < size) && !byte_in_set(s_text[to], set))
            {
                to += 1u;
            }
            while ((end < size) && byte_in_set(s_text[end], set))
            {
                end += 1u;
            }
            while ((back > 0u) && !byte_in_set(s_text[back - 1u], set))
            {
                back -= 1u;
            }
            while ((back_end > 0u) && byte_in_set(s_text[back_end - 1u], set))
            {
                back_end -= 1u;
            }
            TEST_ASSERT_EQUAL_UINT64(to, EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = s_text, .at = at, .size = size, .set = set));
            TEST_ASSERT_EQUAL_UINT64(end, EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = s_text, .at = at, .size = size, .set = set));
            TEST_ASSERT_EQUAL_UINT64(back, EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = s_text, .at = at, .set = set));
            TEST_ASSERT_EQUAL_UINT64(back_end, EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = s_text, .at = at, .set = set));
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

void test_one_byte_is_found_end_a_lanes_width(void)
{
    assert_scans_agree("\n", 97u);
}

void test_a_set_of_two_scans_to_the_first_and_end_the_run(void)
{
    assert_scans_agree(" x", 7u);
}

void test_the_whitespace_set_agrees(void)
{
    assert_scans_agree(CLI_TEXT_WHITE, 31u);
}

void test_a_position_end_the_bound_stays_for_the_forward_scan(void)
{
    TEST_ASSERT_EQUAL_UINT64(9ull, EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = s_text, .at = 9u, .size = 3u, .set = "x"));
}

void test_a_position_end_the_bound_stays_for_the_end_scan(void)
{
    TEST_ASSERT_EQUAL_UINT64(9ull, EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = s_text, .at = 9u, .size = 3u, .set = "x"));
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

void test_an_empty_line_has_no_characters(void)
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

void test_utf16_units_are_counted_across_words(void)
{
    // ten ASCII bytes past a word's width, then a two, a three and a four byte character: 10, 1, 1 and 2 units
    const char text[] = "abcdefghij\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80";
    TEST_ASSERT_EQUAL_UINT64(14ull, EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = text, .size = sizeof(text) - 1u));
    TEST_ASSERT_EQUAL_UINT64(10ull, EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = text, .size = 10ull));
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = text, .size = 0ull));
}

void test_characters_are_counted_across_words(void)
{
    // ten ASCII characters past a word's width, then a two, a three and a four byte character
    const char text[] = "abcdefghij\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80";
    TEST_ASSERT_EQUAL_UINT64(13ull, EMBED_CALL(cli_text_size_byte.characters, CliTextSizeByteCfg, .text = text, .size = sizeof(text) - 1u));
    TEST_ASSERT_EQUAL_UINT64(11ull, EMBED_CALL(cli_text_size_byte.characters, CliTextSizeByteCfg, .text = text, .size = 12ull));
}

void test_characters_are_alphabetic_or_alphanumeric_as_unicode_reads_them(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_table.alphabetic, CliTextCfg, .character = 'a'));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_table.alphabetic, CliTextCfg, .character = 0xE9u));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_text_table.alphabetic, CliTextCfg, .character = '7'));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .character = '7'));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .character = 0xE9u));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .character = ' '));
}

// `out` begun empty over `buffer`
static VerbaLinea begun(char *buffer, unsigned long long capacity)
{
    return mmgr_line_over(buffer, capacity);
}

void test_a_text_is_written_marked_doubled_replaced_filled_lowered_swapped_and_unescaped(void)
{
    char buffer[64];
    VerbaLinea out = begun(buffer, sizeof(buffer));
    EMBED_CALL(cli_text_size_set.append_mark, CliTextSizeSetCfg, .text = "a'b", .set = "'", .mark = '\\', .out = &out);
    CLI_TEST_ASSERT_TEXT("a\\'b", buffer, out.at);
    out = begun(buffer, sizeof(buffer));
    EMBED_CALL(cli_text_size_set.append_marked_n, CliTextSizeSetCfg, .text = "a'b'c", .size = 3ull, .set = "'", .mark = '\\', .out = &out);
    CLI_TEST_ASSERT_TEXT("a\\'b", buffer, out.at);
    out = begun(buffer, sizeof(buffer));
    EMBED_CALL(cli_text_set_table.append_double, CliTextSetCfg, .text = "it's", .set = "'", .out = &out);
    CLI_TEST_ASSERT_TEXT("it''s", buffer, out.at);
    out = begun(buffer, sizeof(buffer));
    EMBED_CALL(cli_text_set_table.append_replace, CliTextSetCfg, .text = "a-b_c", .set = "-_", .with = ".", .out = &out);
    CLI_TEST_ASSERT_TEXT("a.b.c", buffer, out.at);
    out = begun(buffer, sizeof(buffer));
    EMBED_CALL(cli_text_out_name.append_fill, CliTextOutNameCfg, .text = "x=<n>,<n>", .name = "<n>", .value = "7", .out = &out);
    CLI_TEST_ASSERT_TEXT("x=7,7", buffer, out.at);
    out = begun(buffer, sizeof(buffer));
    EMBED_CALL(cli_text_out_name.append_lower, CliTextOutNameCfg, .text = "AbC", .out = &out);
    CLI_TEST_ASSERT_TEXT("abc", buffer, out.at);
    out = begun(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_UINT64(5ull, EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .text = "a.b.c", .from = '.', .to = '/', .out = &out));
    CLI_TEST_ASSERT_TEXT("a/b/c", buffer, out.at);
    out = begun(buffer, sizeof(buffer));
    EMBED_CALL(cli_text_size_set.append_unescaped, CliTextSizeSetCfg, .text = "a%20b%2Fc", .size = 9ull, .out = &out);
    CLI_TEST_ASSERT_TEXT("a b/c", buffer, out.at);
    out = begun(buffer, sizeof(buffer));
    EMBED_CALL(cli_text_classes.append_save, CliTextClassesCfg, .text = "a b_c", .classes = CLI_TEXT_CHARACTERS | CLI_TEXT_DIGITS, .set = "_", .other = '-', .out = &out);
    CLI_TEST_ASSERT_TEXT("a-b_c", buffer, out.at);
}

void test_a_text_is_sorted_searched_trimmed_lowered_and_measured(void)
{
    unsigned long long at = 9ull;
    unsigned long long length = 0ull;
    unsigned long long end = 0ull;
    char lowered[8];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_size_one.before_folded, CliTextSizeOneCfg, .one = "Apple", .size = 5ull, .other_pointer = "banana"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_text_size_one.before_folded, CliTextSizeOneCfg, .one = "banana", .size = 6ull, .other_pointer = "Apple"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_size_at.find_last, CliTextSizeAtCfg, .text = "ab ab", .length_value = 5ull, .required = "ab", .size = 2ull, .at_pointer = &at));
    TEST_ASSERT_EQUAL_UINT64(3ull, at);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_text_size_at.find_last, CliTextSizeAtCfg, .text = "ab ab", .length_value = 5ull, .required = "zz", .size = 2ull, .at_pointer = &at));
    TEST_ASSERT_EQUAL_UINT(15u, EMBED_CALL(cli_text_byte.hex_digit, CliTextByteCfg, .byte = 'f'));
    TEST_ASSERT_EQUAL_UINT(16u, EMBED_CALL(cli_text_byte.hex_digit, CliTextByteCfg, .byte = 'G'));
    TEST_ASSERT_EQUAL_UINT64(2ull, EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = "ab  \n", .size = 5ull));
    TEST_ASSERT_EQUAL_UINT64(3ull, EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = "abc=def", .set = "=:"));
    TEST_ASSERT_EQUAL_UINT64(3ull, EMBED_CALL(cli_text_out_capacity.lowered_into, CliTextOutCapacityCfg, .text = "AbC", .out_capacity = lowered, .capacity = sizeof(lowered)));
    TEST_ASSERT_EQUAL_STRING("abc", lowered);
    TEST_ASSERT_EQUAL_UINT(0xE9u, EMBED_CALL(cli_text_length.character_before, CliTextLengthCfg, .text = "a\xc3\xa9", .end_value = 3ull, .length = &length));
    TEST_ASSERT_EQUAL_UINT64(2ull, length);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_size_type.utf8_valid, CliTextSizeTypeCfg, .text_bytes = (const unsigned char *)"a\xc3\xa9", .size = 3ull));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_text_size_type.utf8_valid, CliTextSizeTypeCfg, .text_bytes = (const unsigned char *)"a\xc3", .size = 2ull));
    // a quote at 2 runs to the quote that closes it, the backslash carrying the quote after it
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_at_size.quoted_end, CliTextAtSizeCfg, .text = "x 'a\\'b' y", .at = 2ull, .size = 10ull, .end = &end));
    TEST_ASSERT_EQUAL_UINT64(8ull, end);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_text_at_size.quoted_end, CliTextAtSizeCfg, .text = "x 'a' y", .at = 0ull, .size = 7ull, .end = &end));
}

void test_the_byte_of_a_utf16_unit_is_found_across_words(void)
{
    // ten ASCII bytes, then a two, a three and a four byte character: the units start at bytes 0 to 9, 10, 12, 15,
    // and the four byte character's second unit is inside it: the next character's byte is 19, the end
    const char text[] = "abcdefghij\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80";
    const unsigned long long size = sizeof(text) - 1u;
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = text, .size = size, .at = 0ull));
    TEST_ASSERT_EQUAL_UINT64(9ull, EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = text, .size = size, .at = 9ull));
    TEST_ASSERT_EQUAL_UINT64(10ull, EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = text, .size = size, .at = 10ull));
    TEST_ASSERT_EQUAL_UINT64(12ull, EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = text, .size = size, .at = 11ull));
    TEST_ASSERT_EQUAL_UINT64(15ull, EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = text, .size = size, .at = 12ull));
    TEST_ASSERT_EQUAL_UINT64(19ull, EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = text, .size = size, .at = 14ull));
    TEST_ASSERT_EQUAL_UINT64(19ull, EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = text, .size = size, .at = 40ull));
}

void test_a_text_is_copied(void)
{
    char copied[8];
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = copied, .text = "ab", .otherwise = "c");
    TEST_ASSERT_EQUAL_STRING("ab", copied);
}

void test_the_fallback_is_copied_where_the_text_is_null(void)
{
    char copied[8];
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = copied, .text = NULL, .otherwise = "c");
    TEST_ASSERT_EQUAL_STRING("c", copied);
}

void test_characters_are_raised_to_upper_case(void)
{
    char upper[4] = "aZ9";
    EMBED_CALL(cli_text_size_from.raise, CliTextSizeFromCfg, .text_out = upper, .size = 3ull);
    TEST_ASSERT_EQUAL_STRING("AZ9", upper);
}

void test_a_number_reads_its_digits_and_reports_where_they_stop(void)
{
    unsigned long long end = 0ull;
    TEST_ASSERT_EQUAL_UINT64(42ull, EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = "  42x", .at = 2ull, .size = 5ull, .end = &end));
    TEST_ASSERT_EQUAL_UINT64(4ull, end);
}

void test_a_pair_is_found_where_it_is(void)
{
    TEST_ASSERT_EQUAL_UINT64(2ull, EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = "a--b", .at = 0ull, .size = 4ull, .first = '-', .second = 'b'));
}

void test_a_pair_that_is_absent_reports_the_end(void)
{
    TEST_ASSERT_EQUAL_UINT64(4ull, EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = "a--b", .at = 0ull, .size = 4ull, .first = 'x', .second = 'y'));
}

void test_a_scan_end_classes_stops_at_the_first_byte_of_neither(void)
{
    TEST_ASSERT_EQUAL_UINT64(3ull, EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = "ab1 ", .at = 0ull, .size = 4ull, .classes = CLI_TEXT_CHARACTERS | CLI_TEXT_DIGITS, .set = ""));
}

void test_a_control_byte_is_found_by_its_class(void)
{
    // ten ASCII characters past a word's width, then a control byte; a tab first; a byte past ASCII is no control
    TEST_ASSERT_EQUAL_UINT64(10ull, EMBED_CALL(cli_text_at_size_classes.to_class, CliTextAtSizeClassesCfg, .text = "abcdefghij\x01k", .at = 0ull, .size = 12ull, .classes = CLI_TEXT_CONTROLS, .set = ""));
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_text_at_size_classes.to_class, CliTextAtSizeClassesCfg, .text = "\tab", .at = 0ull, .size = 3ull, .classes = CLI_TEXT_CONTROLS, .set = ""));
    TEST_ASSERT_EQUAL_UINT64(5ull, EMBED_CALL(cli_text_at_size_classes.to_class, CliTextAtSizeClassesCfg, .text = "abc\xc3\xa9", .at = 0ull, .size = 5ull, .classes = CLI_TEXT_CONTROLS, .set = ""));
}

void test_the_folded_compare_agrees_across_words(void)
{
    // the capitals alone fold to their small characters: a byte past ASCII that differs in the same bit stays
    // different, as do a bracket and a brace
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .one = "HelloWorldABC", .size = 13ull, .other_pointer = "helloworldabc"));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .one = "HelloWorldABD", .size = 13ull, .other_pointer = "helloworldabc"));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .one = "HelloWorld", .size = 10ull, .other_pointer = "helloworldabc"));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .one = "abcdefghij\xc3\xa9", .size = 12ull, .other_pointer = "ABCDEFGHIJ\xc3\xa9"));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .one = "abcdefghij\xc3", .size = 11ull, .other_pointer = "abcdefghij\xe3"));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .one = "abcdefghij[", .size = 11ull, .other_pointer = "abcdefghij{"));
}
