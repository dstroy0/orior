// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill_test.c: Fill Paragraph's check: a comment's paragraph is filled to the margin and its neighbours left, a
// docstring's paragraph keeps its quotes where they stand, a doc comment's tags are lined up and its text filled, and
// plain text fills by blank lines
#include "../../../../../ui/cli/src/editor/fill.h"
#include "cli_check.h"

static char s_written[1u << 14u];
static char s_after[1u << 14u];

// `text` with the paragraph at `line` filled to `width`, checked against `wanted`
static void fill_checked(const char *text, unsigned int line, unsigned int width, const char *wanted, const char *what)
{
    CliLine written = {s_written, sizeof(s_written), 0ull};
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliTextEdit edit;
    CliLine after = {s_after, sizeof(s_after), 0ull};
    if (cli_fill(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), line, width, &written, &edit,
                 &said))
    {
        (void)cli_text_applied(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &edit, 1u,
                               &after);
    }
    cli_check_text(s_after, after.at, wanted, what);
}

int main(void)
{
    fill_checked("x = 1\n    # one two three four five six seven\n    # eight nine\n    #\n    # ten\ny = 2\n", 2u, 24u,
                 "x = 1\n    # one two three four\n    # five six seven\n    # eight nine\n    #\n    # ten\ny = 2\n",
                 "a comment's paragraph is filled to the margin and its neighbours left");
    static const char s_docstring[] = "def f():\n    \"\"\"Return the area of the shape that the\n    reader draws.\n\n"
                                      "    More text that runs on and on past the margin.\n    \"\"\"\n";
    fill_checked(s_docstring, 1u, 30u,
                 "def f():\n    \"\"\"Return the area of the\n    shape that the reader\n    draws.\n\n"
                 "    More text that runs on and on past the margin.\n    \"\"\"\n",
                 "a docstring's first paragraph keeps its opening quotes");
    fill_checked(s_docstring, 4u, 30u,
                 "def f():\n    \"\"\"Return the area of the shape that the\n    reader draws.\n\n"
                 "    More text that runs on and\n    on past the margin.\n    \"\"\"\n",
                 "and its last its closing quotes where they stand");
    fill_checked("def g():\n    \"\"\"One two three four five six seven eight.\"\"\"\n", 1u, 30u,
                 "def g():\n    \"\"\"One two three four five\n    six seven eight.\"\"\"\n",
                 "a docstring of one line keeps both");
    fill_checked(
        "/**\n * The area of a shape, as the reader draws it on the page.\n *\n * @param {number} width the width\n"
        " * @param height the height of the shape, which runs long\n * @returns {number} the area\n */\n"
        "function area(width, height) {}\n",
        4u, 50u,
        "/**\n * The area of a shape, as the reader draws it on\n * the page.\n *\n"
        " * @param {number} width  the width\n * @param          height the height of the shape,\n"
        " *                        which runs long\n * @returns {number} the area\n */\n"
        "function area(width, height) {}\n",
        "a doc comment's tags are lined up and its text filled");
    fill_checked("One two three\nfour five six seven.\n\nNext.\n", 0u, 15u,
                 "One two three\nfour five six\nseven.\n\nNext.\n", "plain text fills by blank lines");
    return cli_checked();
}
