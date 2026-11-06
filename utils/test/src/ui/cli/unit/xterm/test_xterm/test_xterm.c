// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_xterm.c: a line's SGR read through the xterm's walk into plain text and color spans: a line without a sequence
// is its own text with no span; a color and its reset are one span of the sixteen's index with the escape bytes gone;
// bold, a bright color and a change of color are a span each; a color of the 256 past the sixteen and a direct color
// are plain; and a line longer than the plain bytes handed in is cut at them
#include "ui/cli/src/terminal/xterm.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the plain bytes and the spans a line is read into, and their counts
static char s_plain[256];
static unsigned long long s_plain_size;
static CliTerminalXtermSpan s_span[16];
static unsigned int s_spans;

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// the line `text` read into the plain bytes and the spans, `capacity` plain bytes at most
static void line_read(const char *text, unsigned long long capacity)
{
    s_plain_size = 0ULL;
    s_spans = EMBED_CALL(cli_terminal_xterm_spans_table.spans, CliTerminalXtermSpansCfg, .text = text,
                         .size = CLI_UNITY_LENGTH(text), .plain_out = s_plain, .plain_capacity = capacity,
                         .plain_size_out = &s_plain_size, .spans_out = s_span, .spans_capacity = 16u);
}

static void span_is(unsigned int at, unsigned int offset, unsigned int length, unsigned int color, int bold)
{
    TEST_ASSERT_EQUAL_UINT(offset, s_span[at].offset);
    TEST_ASSERT_EQUAL_UINT(length, s_span[at].length);
    TEST_ASSERT_EQUAL_UINT(color, s_span[at].color);
    TEST_ASSERT_EQUAL_INT(bold, s_span[at].bold);
}

void test_a_line_without_a_sequence_is_its_text_with_no_span(void)
{
    line_read("plain words", sizeof(s_plain));
    CLI_TEST_ASSERT_TEXT("plain words", s_plain, s_plain_size);
    TEST_ASSERT_EQUAL_UINT(0u, s_spans);
}

void test_a_color_and_its_reset_are_one_span_without_the_escape_bytes(void)
{
    line_read("ok \x1b[31mred\x1b[0m after", sizeof(s_plain));
    CLI_TEST_ASSERT_TEXT("ok red after", s_plain, s_plain_size);
    TEST_ASSERT_EQUAL_UINT(1u, s_spans);
    span_is(0u, 3u, 3u, 1u, 0);
}

void test_bold_a_bright_color_and_a_change_of_color_are_a_span_each(void)
{
    line_read("\x1b[1mB\x1b[0m\x1b[92mG\x1b[34mb\x1b[39m.", sizeof(s_plain));
    CLI_TEST_ASSERT_TEXT("BGb.", s_plain, s_plain_size);
    TEST_ASSERT_EQUAL_UINT(3u, s_spans);
    span_is(0u, 0u, 1u, CLI_TERMINAL_SPAN_PLAIN, 1);
    span_is(1u, 1u, 1u, 10u, 0);
    span_is(2u, 2u, 1u, 4u, 0);
}

void test_a_wide_or_direct_color_is_plain(void)
{
    line_read("\x1b[38;5;200mw\x1b[38;2;1;2;3md\x1b[0m", sizeof(s_plain));
    CLI_TEST_ASSERT_TEXT("wd", s_plain, s_plain_size);
    TEST_ASSERT_EQUAL_UINT(0u, s_spans);
}

void test_a_line_is_cut_at_the_plain_bytes_handed_in(void)
{
    line_read("\x1b[31mabcdef", 4ULL);
    CLI_TEST_ASSERT_TEXT("abcd", s_plain, s_plain_size);
    TEST_ASSERT_EQUAL_UINT(1u, s_spans);
    span_is(0u, 0u, 4u, 1u, 0);
}
