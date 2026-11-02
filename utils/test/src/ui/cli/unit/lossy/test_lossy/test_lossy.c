// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_lossy.c: lossy UTF-8 decoding: valid sequences of one to four bytes pass through, each byte of an invalid sequence
// becomes U+FFFD, and a line that fills ends on a whole character
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_out[64];

// `size` bytes of `text` decoded into a line of `capacity` bytes, written to s_out: whether they fit
static int decoded(const char *text, unsigned long long size, unsigned long long capacity)
{
    VerbaLinea line = {s_out, capacity, 0ull};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line, .text_bytes = (const unsigned char *)text, .length = size);
    const int fits = mmgr_line_ok(&line);
    (void)mmgr_line_finish(&line);
    return fits;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_ascii_passes_through(void)
{
    const char ascii[] = "plain text, every byte a letter";
    TEST_ASSERT_TRUE(decoded(ascii, sizeof(ascii) - 1u, sizeof(s_out)));
    TEST_ASSERT_EQUAL_STRING(ascii, s_out);
}

void test_two_three_and_four_byte_sequences_pass_through(void)
{
    const char letters[] = "a\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80 z";
    TEST_ASSERT_TRUE(decoded(letters, sizeof(letters) - 1u, sizeof(s_out)));
    TEST_ASSERT_EQUAL_STRING(letters, s_out);
}

void test_an_invalid_lead_byte_and_a_truncated_sequence_become_replacement_characters(void)
{
    const char broken[] = "a\xff"
                          "b\xc3";
    (void)decoded(broken, sizeof(broken) - 1u, sizeof(s_out));
    TEST_ASSERT_EQUAL_STRING("a\xef\xbf\xbd"
                             "b\xef\xbf\xbd",
                             s_out);
}

void test_each_byte_of_a_surrogate_becomes_a_replacement_character(void)
{
    const char surrogate[] = "\xed\xa0\x80"
                             "c";
    (void)decoded(surrogate, sizeof(surrogate) - 1u, sizeof(s_out));
    TEST_ASSERT_EQUAL_STRING("\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd"
                             "c",
                             s_out);
}

void test_a_full_line_stops_before_a_character_that_does_not_fit(void)
{
    // "ab", the euro sign's three bytes, "cd": five bytes of room hold four and the euro sign does not fit
    const char text[] = "ab\xe2\x82\xac"
                        "cd";
    TEST_ASSERT_FALSE(decoded(text, sizeof(text) - 1u, 5ull));
    TEST_ASSERT_EQUAL_STRING("ab", s_out);
}

void test_a_full_line_stops_before_a_replacement_that_does_not_fit(void)
{
    // "ab", a byte that starts no character, "c": four bytes of room hold three and U+FFFD's three bytes do not fit after "ab"
    const char text[] = "ab\xff"
                        "c";
    TEST_ASSERT_FALSE(decoded(text, sizeof(text) - 1u, 4ull));
    TEST_ASSERT_EQUAL_STRING("ab", s_out);
}

void test_a_full_line_saves_a_character_that_fits_whole(void)
{
    const char text[] = "ab\xe2\x82\xac"
                        "cd";
    TEST_ASSERT_FALSE(decoded(text, sizeof(text) - 1u, 6ull));
    TEST_ASSERT_EQUAL_STRING("ab\xe2\x82\xac", s_out);
}
