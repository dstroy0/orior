// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_fill.c: Fill Paragraph: a comment wraps to the margin leaving its neighbours, a docstring saves its quotes, a doc
// comment aligns its tags, and plain text wraps by blank lines
#include "ui/cli/src/editor/fill.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_written[1u << 14u];
static char s_after[1u << 14u];

static const char s_docstring[] = "def f():\n    \"\"\"Return the area of the shape that the\n    reader draws.\n\n"
                                  "    More text that runs on and on past the margin.\n    \"\"\"\n";

// `text` with the paragraph at `line` wrapped to `width` is `wanted`
static void assert_filled(const char *wanted, const char *text, unsigned int line, unsigned int width)
{
    VerbaLinea written = {s_written, sizeof(s_written), 0ull};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliTextEdit edit;
    VerbaLinea after = {s_after, sizeof(s_after), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_fill_table.fill, CliFillCfg, .text = text, .size = CLI_UNITY_LENGTH(text), .line = line, .width = width, .written = &written, .edit = &edit, .err_msg = &err_msg));
    (void)EMBED_CALL(cli_edits_out.text_apply, CliEditsOutCfg, .text = text, .size = CLI_UNITY_LENGTH(text), .edits = &edit, .count = 1u, .out = &after);
    CLI_TEST_ASSERT_TEXT(wanted, s_after, after.at);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_comment_wraps_to_the_margin_and_leaves_its_neighbours(void)
{
    assert_filled("x = 1\n    # one two three four\n    # five six seven\n    # eight nine\n    #\n    # ten\ny = 2\n",
                  "x = 1\n    # one two three four five six seven\n    # eight nine\n    #\n    # ten\ny = 2\n", 2u, 24u);
}

void test_a_docstring_first_paragraph_saves_its_opening_quotes(void)
{
    assert_filled("def f():\n    \"\"\"Return the area of the\n    shape that the reader\n    draws.\n\n"
                  "    More text that runs on and on past the margin.\n    \"\"\"\n",
                  s_docstring, 1u, 30u);
}

void test_a_docstring_last_paragraph_saves_its_closing_quotes(void)
{
    assert_filled("def f():\n    \"\"\"Return the area of the shape that the\n    reader draws.\n\n"
                  "    More text that runs on and\n    on past the margin.\n    \"\"\"\n",
                  s_docstring, 4u, 30u);
}

void test_a_one_line_docstring_saves_both_quotes(void)
{
    assert_filled("def g():\n    \"\"\"One two three four five\n    six seven eight.\"\"\"\n",
                  "def g():\n    \"\"\"One two three four five six seven eight.\"\"\"\n", 1u, 30u);
}

void test_a_doc_comment_aligns_its_tags_and_wraps_its_text(void)
{
    assert_filled("/**\n * The area of a shape, as the reader draws it on\n * the page.\n *\n"
                  " * @param {number} width  the width\n * @param          height the height of the shape,\n"
                  " *                        which runs long\n * @returns {number} the area\n */\n"
                  "function area(width, height) {}\n",
                  "/**\n * The area of a shape, as the reader draws it on the page.\n *\n * @param {number} width the width\n"
                  " * @param height the height of the shape, which runs long\n * @returns {number} the area\n */\n"
                  "function area(width, height) {}\n",
                  4u, 50u);
}

void test_plain_text_wraps_by_blank_lines(void)
{
    assert_filled("One two three\nfour five six\nseven.\n\nNext.\n", "One two three\nfour five six seven.\n\nNext.\n", 0u, 15u);
}
