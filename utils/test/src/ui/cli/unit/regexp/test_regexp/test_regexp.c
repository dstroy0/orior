// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_regexp.c: patterns: characters, classes and quantifiers, sticky matches with anchors and boundaries, alternation, groups, lookahead
// and laziness, backreferences and case, Unicode's properties under the u flag, refused syntax, a string of any length taken a pass at a
// time, and a pattern that goes too deep or runs away gives up. Each runs on the program's own thread, whose stack has room for more than
// a match may use
#include "ui/cli/src/text/regexp.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_string[] = "\"(?:[^\"\\\\]|\\\\.)*\"";
static const char s_keyed[] = "\"(?:[^\"\\\\]|\\\\.)*\"(?=\\s*:)";

static CliRegexpWorkState s_work_state;
static CliRegexp s_regexp;
static unsigned int s_characters[1u << 18u];
static unsigned long long s_offset[(1u << 18u) + 1u];
static CliRegexpSpan s_groups[CLI_REGEXP_GROUPS + 1u];
static char s_text[1u << 18u];
static char s_found[1u << 18u];

// `source` with `flags` read: 1, or 0 where it is refused
static int regexp_read(const char *source, const char *flags)
{
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &s_regexp, .work_state = &s_work_state);
    return EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &s_regexp, .source = source, .flags = flags, .err_msg = &err_msg);
}

// `text` decoded into s_characters, the byte each character starts at into s_offset: the count of characters
static unsigned long long characters_of(const char *text)
{
    const unsigned long long size = CLI_UNITY_LENGTH(text);
    const unsigned long long count = EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = text, .size = size, .characters = s_characters, .capacity_u64 = sizeof(s_characters) / sizeof(s_characters[0]));
    unsigned long long character = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if ((((unsigned char)text[at]) & 0xc0u) != 0x80u)
        {
            s_offset[character] = at;
            character += 1ull;
        }
    }
    s_offset[count] = size;
    return count;
}

// the match of `source` with `flags` at character `from` of `text`, into s_found: 1 with the characters it uses in `taken`, or 0 where none matches
static int matched_at(const char *source, const char *flags, const char *text, unsigned long long from, unsigned long long *taken)
{
    const unsigned long long count = characters_of(text);
    if (!regexp_read(source, flags) || !EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &s_regexp, .text_u32s = s_characters, .length = count, .at = from, .groups = s_groups))
    {
        return 0;
    }
    const unsigned long long start = s_offset[s_groups[0].from];
    const unsigned long long end = s_offset[s_groups[0].to];
    memor.cpy(&(MemoriaCfg){.dst = s_found, .src = text + start, .bytes = end - start});
    s_found[end - start] = '\0';
    *taken = s_groups[0].to - s_groups[0].from;
    return 1;
}

// the match of `source` with `flags` at character `from` of `text` is `wanted`
static void assert_match(const char *wanted, const char *source, const char *flags, const char *text, unsigned long long from)
{
    unsigned long long taken = 0ull;
    TEST_ASSERT_TRUE_MESSAGE(matched_at(source, flags, text, from, &taken), source);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(wanted, s_found, source);
}

// `source` with `flags` does not match at character `from` of `text`
static void assert_no_match(const char *source, const char *flags, const char *text, unsigned long long from)
{
    unsigned long long taken = 0ull;
    TEST_ASSERT_FALSE_MESSAGE(matched_at(source, flags, text, from, &taken), source);
}

// each group of `source`'s match at the start of `text`, joined by `|`, `-` for one that took no part, is `wanted`
static void assert_groups(const char *wanted, const char *source, const char *text)
{
    const unsigned long long count = characters_of(text);
    char out[256];
    VerbaLinea line = {out, sizeof(out), 0ull};
    TEST_ASSERT_TRUE(regexp_read(source, ""));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &s_regexp, .text_u32s = s_characters, .length = count, .at = 0ull, .groups = s_groups));
    for (unsigned int group = 0u; group <= s_regexp.groups; group += 1u)
    {
        mmgr_line_put(&line, (group > 0u) ? "|" : "");
        if (s_groups[group].set)
        {
            const unsigned long long start = s_offset[s_groups[group].from];
            mmgr_line_put_n(&line, text + start, s_offset[s_groups[group].to] - start);
        }
        else
        {
            mmgr_line_ch(&line, '-');
        }
    }
    CLI_TEST_ASSERT_TEXT(wanted, out, line.at);
}

// `count` characters a and the string `after` into s_text
static void run_written(unsigned int count, const char *after)
{
    memor.set(&(MemoriaCfg){.dst = s_text, .val = 'a', .bytes = count});
    memor.cpy(&(MemoriaCfg){.dst = s_text + count, .src = after, .bytes = CLI_UNITY_LENGTH(after) + 1ull});
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_digit_class_repeats(void)
{
    assert_match("123", "\\d+", "", "123abc", 0ull);
}

void test_a_set_of_ranges_and_a_word_class(void)
{
    assert_match("abc_1$", "[a-z_][\\w$]*", "", "abc_1$ x", 0ull);
}

void test_a_negated_set_with_escapes(void)
{
    assert_match("ab", "[^\\]\\\\]+", "", "ab\\]", 0ull);
}

void test_a_hexadecimal_number_with_a_suffix(void)
{
    assert_match("0xFFul", "0[xX][\\da-fA-F_]+[uUlLzZ]*", "", "0xFFul;", 0ull);
}

void test_a_bounded_repeat_ends_at_a_boundary(void)
{
    assert_match("#fff", "#[\\da-fA-F]{3,8}\\b", "", "#fff;", 0ull);
}

void test_a_bounded_repeat_with_no_boundary_does_not_match(void)
{
    assert_no_match("#[\\da-fA-F]{3,8}\\b", "", "#ffg", 0ull);
}

void test_an_exact_repeat(void)
{
    assert_match("aa", "a{2}", "", "aaa", 0ull);
}

void test_an_open_repeat(void)
{
    assert_match("aaaa", "a{2,}", "", "aaaa", 0ull);
}

void test_a_brace_with_no_least_is_literal(void)
{
    assert_match("a{,2}", "a{,2}", "", "a{,2}", 0ull);
}

void test_an_escaped_dot_is_a_dot(void)
{
    assert_no_match("\\.", "", "x", 0ull);
}

void test_a_dot_stops_at_a_line_end(void)
{
    assert_match("ab", ".*", "", "ab\ncd", 0ull);
}

void test_hexadecimal_and_code_point_escapes(void)
{
    assert_match("ABC", "A\\x42\\u{43}", "", "ABC", 0ull);
}

void test_a_match_is_sticky_at_its_position(void)
{
    assert_match("word", "\\w+", "", "  word", 2ull);
}

void test_a_match_does_not_search_ahead(void)
{
    assert_no_match("\\w+", "", "  word", 0ull);
}

void test_a_start_anchor_at_the_start(void)
{
    assert_match("  :label", "^\\s*:\\w+", "", "  :label", 0ull);
}

void test_a_start_anchor_past_the_start_does_not_match(void)
{
    assert_no_match("^:\\w+", "", "x:label", 1ull);
}

void test_an_end_anchor_at_the_end(void)
{
    assert_match("// note", "\\/\\/.*$", "", "a // note", 2ull);
}

void test_an_end_anchor_alone_matches_nothing_at_the_end(void)
{
    assert_match("", "$", "", "ab", 2ull);
}

void test_word_boundaries_around_a_word(void)
{
    assert_match("if", "\\bif\\b", "", "if(", 0ull);
}

void test_word_boundaries_inside_a_longer_word_do_not_match(void)
{
    assert_no_match("\\bif\\b", "", "iff", 0ull);
}

void test_optional_groups_read_a_number(void)
{
    assert_match("3.25e-4", "(?:\\d[\\d_']*)?\\.?\\d[\\d_']*(?:[eE][\\-+]?\\d+)?", "", "3.25e-4 ", 0ull);
}

void test_a_lookahead_that_holds(void)
{
    assert_match("color", "[\\w-]+(?=\\s*:)", "", "color : red", 0ull);
}

void test_a_lookahead_that_fails(void)
{
    assert_no_match("[\\w-]+(?=\\s*:)", "", "color red", 0ull);
}

void test_a_negative_lookahead_backs_off(void)
{
    assert_match("cal", "\\w+(?!\\()", "", "call(", 0ull);
}

void test_the_first_alternative_wins(void)
{
    assert_match("cat", "cat|category", "", "category", 0ull);
}

void test_a_lazy_repeat_stops_early(void)
{
    assert_match("<a>", "<.*?>", "", "<a><b>", 0ull);
}

void test_a_greedy_repeat_runs_far(void)
{
    assert_match("<a><b>", "<.*>", "", "<a><b>", 0ull);
}

void test_a_repeated_group(void)
{
    assert_match("abab", "(ab)+", "", "ababx", 0ull);
}

void test_groups_backtrack_to_a_whole_match(void)
{
    assert_match("abcd", "(a|ab)(c|bcd)(d*)", "", "abcd", 0ull);
}

void test_a_repeated_repeat_ends(void)
{
    assert_match("aaab", "(a*)*b", "", "aaab", 0ull);
}

void test_a_repeat_of_nothing_ends(void)
{
    assert_match("", "(x?)+$", "", "", 0ull);
}

void test_an_empty_set_matches_nothing(void)
{
    assert_no_match("[]a", "", "a", 0ull);
}

void test_a_negated_empty_set_matches_anything(void)
{
    assert_match("a]b", "[^]+", "", "a]b", 0ull);
}

void test_each_group_reads_its_part(void)
{
    assert_groups("one two|one|two", "(\\w+)\\s(\\w+)", "one two");
}

void test_a_group_that_took_no_part_is_not_set(void)
{
    assert_groups("b|-|b", "(a)|(b)", "b");
}

void test_a_backreference_with_two_groups(void)
{
    assert_match("r\"-(x)-\"", "[rR](['\"])(-*)[\\[({].*?[\\])}]\\2\\1", "", "r\"-(x)-\" tail", 0ull);
}

void test_a_backreference_to_the_opening_quote(void)
{
    assert_match("'a\"b'", "(['\"]).*?\\1", "", "'a\"b' c", 0ull);
}

void test_the_case_flag_folds_a_literal(void)
{
    assert_match("SELECT", "select", "i", "SELECT x", 0ull);
}

void test_the_case_flag_folds_a_range(void)
{
    assert_match("MiXed", "[a-z]+", "i", "MiXed1", 0ull);
}

void test_with_no_case_flag_case_matters(void)
{
    assert_no_match("select", "", "SELECT", 0ull);
}

void test_character_and_number_properties_under_the_u_flag(void)
{
    assert_match("\xCE\xB1\xCE\xB2_1'", "[\\p{L}_][\\p{L}\\p{N}_']*", "u", "\xCE\xB1\xCE\xB2_1' x", 0ull);
}

void test_a_negated_property(void)
{
    assert_match("ab", "\\P{N}+", "u", "ab3", 0ull);
}

void test_case_properties(void)
{
    assert_match("\xC3\x8Atre", "\\p{Lu}\\p{Ll}+", "u", "\xC3\x8Atre!", 0ull);
}

void test_with_no_u_flag_a_property_is_literal(void)
{
    assert_match("p{L}", "\\p{L}", "", "p{L}", 0ull);
}

void test_lookbehind_is_refused(void)
{
    TEST_ASSERT_FALSE(regexp_read("(?<=a)b", ""));
}

void test_a_named_group_is_refused(void)
{
    TEST_ASSERT_FALSE(regexp_read("(?<name>a)", ""));
}

void test_a_property_not_read_is_refused(void)
{
    TEST_ASSERT_FALSE(regexp_read("\\p{Script=Greek}", "u"));
}

void test_an_unclosed_group_is_refused(void)
{
    TEST_ASSERT_FALSE(regexp_read("(a", ""));
}

void test_an_unopened_group_is_refused(void)
{
    TEST_ASSERT_FALSE(regexp_read("a)", ""));
}

void test_a_quantifier_of_nothing_is_refused(void)
{
    TEST_ASSERT_FALSE(regexp_read("*a", ""));
}

void test_a_string_with_escapes_matches(void)
{
    assert_match("\"a\\\"b\\\\\"", s_string, "", "\"a\\\"b\\\\\" rest", 0ull);
}

void test_a_string_followed_by_a_colon_is_a_key(void)
{
    assert_match("\"key\"", s_keyed, "", "\"key\" : 1", 0ull);
}

void test_a_string_with_no_colon_is_not_a_key(void)
{
    assert_no_match(s_keyed, "", "\"value\", 1", 0ull);
}

void test_a_string_of_any_length_is_taken_a_pass_at_a_time(void)
{
    unsigned long long taken = 0ull;
    VerbaLinea line = {s_text, sizeof(s_text), 0ull};
    mmgr_line_ch(&line, '"');
    for (unsigned int at = 0u; at < 200000u; at += 1u)
    {
        mmgr_line_ch(&line, ((at % 997u) == 0u) ? '\\' : 'x');
    }
    mmgr_line_put(&line, "\", next");
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_TRUE(matched_at(s_string, "", s_text, 0ull, &taken));
    TEST_ASSERT_EQUAL_UINT64(200002ull, taken);
}

void test_a_repetition_a_pass_at_a_time_matches_as_backtracking_does(void)
{
    assert_match("ababc", "(?:ab|c)*", "", "ababcx", 0ull);
    assert_match("abc", "(?:ab|c)+?c", "", "abcc", 0ull);
    assert_match("abab", "(?:ab|c){2}", "", "ababab", 0ull);
    assert_match("ababcc", "(?:ab|c){2,3}c", "", "ababcc", 0ull);
    assert_match("ababcde", "(?:ab|cd)*e", "", "ababcde", 0ull);
    assert_no_match("(?:ab|cd)+", "", "ac", 0ull);
    assert_match("aba", "(?:ab|a)*", "", "aba", 0ull);
    assert_match("AbA", "(?:a|b)+", "i", "AbAc", 0ull);
    assert_match("1x2", "(?:\\d|x)+", "", "1x2y", 0ull);
}

void test_a_repetition_that_goes_too_deep_gives_up(void)
{
    unsigned long long taken = 0ull;
    run_written(100000u, "!");
    TEST_ASSERT_FALSE(matched_at("(?:a|ab)*!", "", s_text, 0ull, &taken));
}

void test_a_repetition_inside_its_stack_matches(void)
{
    unsigned long long taken = 0ull;
    run_written(20u, "!");
    TEST_ASSERT_TRUE(matched_at("(?:a|ab)*!", "", s_text, 0ull, &taken));
    TEST_ASSERT_EQUAL_UINT64(21ull, taken);
}

void test_a_runaway_pattern_gives_up(void)
{
    unsigned long long taken = 0ull;
    run_written(40u, "!");
    TEST_ASSERT_FALSE(matched_at("(a|aa)+$", "", s_text, 0ull, &taken));
}
