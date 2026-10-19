// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diff_test.c: the page's diff's check: lines put in, taken out and changed are marked as the page marks them, the same
// text has no marks and texts too far apart have none at all, lines that hash alike and differ are told apart, and a
// long text with lines changed far apart marks each
#include "../../../../../ui/cli/src/editor/document.h"
#include "cli_check.h"

// the units and lines each side's text is read into
#define DIFF_UNITS (1u << 21u)
#define DIFF_LINES (1u << 17u)

// A side's storage and its lines
typedef struct
{
    unsigned short unit[DIFF_UNITS];
    unsigned long long start[DIFF_LINES];
    unsigned long long length[DIFF_LINES];
    CliLines lines;
} DiffSide;

static DiffSide s_then;
static DiffSide s_now;
static char s_out[1u << 20u];
static char s_then_text[1u << 21u];
static char s_now_text[1u << 21u];

// `size` bytes of `text` read into `side`'s lines
static const CliLines *diff_lines(DiffSide *side, const char *text, unsigned long long size)
{
    side->lines = (CliLines){side->unit, DIFF_UNITS, 0ull, side->start, side->length, DIFF_LINES, 0u, 0};
    cli_document_lines_of(text, size, &side->lines);
    return &side->lines;
}

// the marks of `now` against `then`, written into s_out: their size, 0 where the texts are too far apart
static unsigned long long diff_changes(const char *then, unsigned long long then_size, const char *now,
                                       unsigned long long now_size)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    return cli_diff_line_changes(diff_lines(&s_then, then, then_size), diff_lines(&s_now, now, now_size), &out) ? out.at
                                                                                                                : 0ull;
}

// a member of the marks written compact, checked against `wanted`
static void diff_member(unsigned long long size, const char *name, const char *wanted, const char *what)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, size, &root);
    char text[1024];
    CliLine line = {text, sizeof(text), 0ull};
    cli_json_write(json, cli_json_member(json, root, name), &line, 0u, 0);
    cli_check_text(text, line.at, wanted, what);
}

int main(void)
{
    unsigned long long size = diff_changes("a\nb\nc\nd\ne", 9ull, "a\nB\nc\nnew\nd", 11ull);
    diff_member(size, "changed", "[1]", "a line changed is marked changed");
    diff_member(size, "added", "[3]", "a line put in is marked added");
    diff_member(size, "removed", "[5]", "e taken out below the last line");
    diff_member(size, "hunks",
                "[{\"now\":[1,2],\"then\":[1,2],\"thenLines\":[\"b\"]},{\"now\":[3,4],\"then\":[3,3]},"
                "{\"now\":[5,5],\"then\":[4,5],\"thenLines\":[\"e\"]}]",
                "each change's hunk");
    size = diff_changes("a\nb", 3ull, "a\nb", 3ull);
    diff_member(size, "added", "[]", "the same text has nothing added");
    diff_member(size, "changed", "[]", "nothing changed");
    diff_member(size, "removed", "[]", "nothing taken out");
    diff_member(size, "hunks", "[]", "and no hunk");
    CliLine then = {s_then_text, sizeof(s_then_text), 0ull};
    CliLine now = {s_now_text, sizeof(s_now_text), 0ull};
    for (unsigned int at = 0u; at < 4000u; at += 1u)
    {
        cli_line_text(&then, "old ");
        cli_line_decimal(&then, at, 1u);
        cli_line_character(&then, '\n');
        cli_line_text(&now, "new ");
        cli_line_decimal(&now, at, 1u);
        cli_line_character(&now, '\n');
    }
    cli_check(diff_changes(s_then_text, then.at, s_now_text, now.at) == 0ull,
              "texts too far apart have no marks at all");
    static const unsigned short s_line_a[3] = {1u, 2u, 3u};
    static const unsigned short s_line_b[3] = {1u, 2u, 4u};
    static unsigned long long s_start[2] = {0ull, 2ull};
    static unsigned long long s_length[2] = {2ull, 1ull};
    static unsigned long long s_start_b[2] = {0ull, 2ull};
    static unsigned long long s_length_b[2] = {2ull, 1ull};
    const CliLines lines_then = {(unsigned short *)s_line_a, 3ull, 3ull, s_start, s_length, 2u, 2u, 0};
    const CliLines lines_now = {(unsigned short *)s_line_b, 3ull, 3ull, s_start_b, s_length_b, 2u, 2u, 0};
    static const unsigned long long s_hashes[2] = {7ull, 7ull};
    cli_check(cli_diff_run_of(&lines_then, &lines_now, s_hashes, s_hashes, 0, 0) == 1,
              "the run ends at the line that differs");
    then.at = 0ull;
    now.at = 0ull;
    for (unsigned int at = 0u; at < 50000u; at += 1u)
    {
        cli_line_text(&then, (at > 0u) ? "\nx_" : "x_");
        cli_line_decimal(&then, at, 1u);
        cli_line_text(&then, " = ");
        cli_line_decimal(&then, at, 1u);
        cli_line_text(&then, " * 2");
        cli_line_text(&now, (at > 0u) ? "\nx_" : "x_");
        cli_line_decimal(&now, at, 1u);
        cli_line_text(&now, " = ");
        cli_line_decimal(&now, at, 1u);
        cli_line_text(&now, ((at >= 1000u) && (((at - 1000u) % 5000u) == 0u)) ? " * 2 changed" : " * 2");
    }
    size = diff_changes(s_then_text, then.at, s_now_text, now.at);
    char wanted[256];
    CliLine line = {wanted, sizeof(wanted), 0ull};
    cli_line_character(&line, '[');
    for (unsigned int at = 1000u; at < 50000u; at += 5000u)
    {
        cli_line_text(&line, (at > 1000u) ? "," : "");
        cli_line_decimal(&line, at, 1u);
    }
    cli_line_character(&line, ']');
    (void)cli_line_closed(&line);
    diff_member(size, "changed", wanted, "a long text with lines changed far apart marks each");
    diff_member(size, "added", "[]", "and puts none in");
    diff_member(size, "removed", "[]", "and takes none out");
    return cli_checked();
}
