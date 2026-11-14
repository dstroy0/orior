// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_diff.c: the changes between an older text's lines and a newer's: the same text has none; a line changed is one
// change of one line for one; a line put in and one taken out are two changes; an older text of one empty line is one
// change putting every new line in; lines that hash alike and differ are told apart; and texts too far apart are
// refused
#include "ui/cli/src/editor/document.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the units and lines each side's text is positioned into, and the changes found, at their max
#define DIFF_UNITS (1u << 16u)
#define DIFF_LINES (1u << 12u)
#define DIFF_HUNKS 64u
#define FAR_LINES 2200u

// A side: its units, its lines' starts and lengths, and the lines over them
typedef struct
{
    unsigned short unit[DIFF_UNITS];
    unsigned long long start[DIFF_LINES];
    unsigned long long length[DIFF_LINES];
    CliLines lines;
} DiffSide;

static DiffSide s_then;
static DiffSide s_now;
static DiffSide s_inner_then;
static DiffSide s_inner_now;
static CliEachHunkCfg s_hunk[DIFF_HUNKS];
static unsigned int s_hunks;
static unsigned int s_inner_hunks;
static int s_inner_runs;
static char s_far_then[FAR_LINES * 8u];
static char s_far_now[FAR_LINES * 8u];

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// the side's lines positioned from `text`
static void side_positioned(DiffSide *side, const char *text, unsigned long long size)
{
    side->lines = (CliLines){side->unit, DIFF_UNITS, 0ULL, side->start, side->length, DIFF_LINES, 0u, 0};
    EMBED_CALL(cli_document_text_size.lines_get, CliDocumentTextSizeCfg, .text = text, .size = size,
               .lines_out = &side->lines);
}

// each change of an inner edit counted
static void inner_hunk_each(const CliEachHunkCfg *args)
{
    (void)args;
    s_inner_hunks += 1u;
}

// each change found, in order; and where an inner edit is asked for, a second edit run from inside the first's
// callback, as a worker's edit runs beside the view thread's
static void hunk_each(const CliEachHunkCfg *args)
{
    if (s_hunks < DIFF_HUNKS)
    {
        s_hunk[s_hunks] = *args;
    }
    s_hunks += 1u;
    if (s_inner_runs)
    {
        s_inner_runs = 0;
        static const char s_inner_then_text[] = "alpha\nbeta\ngamma\ndelta\n";
        static const char s_inner_now_text[] = "alpha\nB\ngamma\nD\n";
        side_positioned(&s_inner_then, s_inner_then_text, sizeof(s_inner_then_text) - 1u);
        side_positioned(&s_inner_now, s_inner_now_text, sizeof(s_inner_now_text) - 1u);
        (void)EMBED_CALL(cli_document_out_then.line_changes, CliDocumentOutThenCfg, .then = &s_inner_then.lines,
                         .now = &s_inner_now.lines, .each = inner_hunk_each);
    }
}

// the changes between `then` and `now`, into the hunks found: what the diff answers
static int changes(const char *then, unsigned long long then_size, const char *now, unsigned long long now_size)
{
    side_positioned(&s_then, then, then_size);
    side_positioned(&s_now, now, now_size);
    s_hunks = 0u;
    return EMBED_CALL(cli_document_out_then.line_changes, CliDocumentOutThenCfg, .then = &s_then.lines,
                      .now = &s_now.lines, .each = hunk_each);
}

static void hunk_is(unsigned int at, unsigned int now_from, unsigned int now_to, unsigned int then_from,
                    unsigned int then_to)
{
    TEST_ASSERT_EQUAL_UINT(now_from, s_hunk[at].now_from);
    TEST_ASSERT_EQUAL_UINT(now_to, s_hunk[at].now_to);
    TEST_ASSERT_EQUAL_UINT(then_from, s_hunk[at].then_from);
    TEST_ASSERT_EQUAL_UINT(then_to, s_hunk[at].then_to);
}

void test_the_same_text_has_no_change(void)
{
    static const char s_text[] = "one\ntwo\nthree\n";
    TEST_ASSERT_EQUAL_INT(1, changes(s_text, sizeof(s_text) - 1u, s_text, sizeof(s_text) - 1u));
    TEST_ASSERT_EQUAL_UINT(0u, s_hunks);
}

void test_a_line_changed_is_one_change_of_one_line_for_one(void)
{
    static const char s_then_text[] = "one\ntwo\nthree\n";
    static const char s_now_text[] = "one\n2\nthree\n";
    TEST_ASSERT_EQUAL_INT(1, changes(s_then_text, sizeof(s_then_text) - 1u, s_now_text, sizeof(s_now_text) - 1u));
    TEST_ASSERT_EQUAL_UINT(1u, s_hunks);
    hunk_is(0u, 1u, 2u, 1u, 2u);
}

void test_a_line_put_in_and_one_taken_out_are_two_changes(void)
{
    static const char s_then_text[] = "one\ntwo\nthree\nfour\n";
    static const char s_now_text[] = "one\ntwo\nnew\nthree\n";
    TEST_ASSERT_EQUAL_INT(1, changes(s_then_text, sizeof(s_then_text) - 1u, s_now_text, sizeof(s_now_text) - 1u));
    TEST_ASSERT_EQUAL_UINT(2u, s_hunks);
    hunk_is(0u, 2u, 3u, 2u, 2u);
    hunk_is(1u, 4u, 4u, 3u, 4u);
}

void test_an_older_text_of_one_empty_line_is_one_change_putting_every_new_line_in(void)
{
    static const char s_now_text[] = "one\ntwo\n";
    TEST_ASSERT_EQUAL_INT(1, changes("", 0ULL, s_now_text, sizeof(s_now_text) - 1u));
    TEST_ASSERT_EQUAL_UINT(1u, s_hunks);
    hunk_is(0u, 0u, 2u, 0u, 0u);
}

void test_lines_that_hash_alike_and_differ_are_told_apart(void)
{
    static const char s_then_text[] = "costarring\nliquid\n";
    static const char s_now_text[] = "liquid\nliquid\n";
    TEST_ASSERT_EQUAL_INT(1, changes(s_then_text, sizeof(s_then_text) - 1u, s_now_text, sizeof(s_now_text) - 1u));
    unsigned int out = 0u;
    unsigned int in = 0u;
    for (unsigned int at = 0u; at < s_hunks; at += 1u)
    {
        out += s_hunk[at].then_to - s_hunk[at].then_from;
        in += s_hunk[at].now_to - s_hunk[at].now_from;
    }
    TEST_ASSERT_EQUAL_UINT(1u, out);
    TEST_ASSERT_EQUAL_UINT(1u, in);
}

void test_texts_too_far_apart_are_refused(void)
{
    VerbaLinea then = {s_far_then, sizeof(s_far_then), 0ULL};
    VerbaLinea now = {s_far_now, sizeof(s_far_now), 0ULL};
    for (unsigned int line = 0u; line < FAR_LINES; line += 1u)
    {
        mmgr_line_put(&then, "a");
        mmgr_line_decimal(&then, line, 1u);
        mmgr_line_put(&then, "\n");
        mmgr_line_put(&now, "b");
        mmgr_line_decimal(&now, line, 1u);
        mmgr_line_put(&now, "\n");
    }
    TEST_ASSERT_EQUAL_INT(0, changes(s_far_then, then.at, s_far_now, now.at));
}

void test_an_edit_run_inside_another_leaves_the_first_its_own_changes(void)
{
    static const char s_then_text[] = "one\ntwo\nthree\nfour\nfive\n";
    static const char s_now_text[] = "one\n2\nthree\n4\nfive\n";
    s_inner_hunks = 0u;
    s_inner_runs = 1;
    TEST_ASSERT_EQUAL_INT(1, changes(s_then_text, sizeof(s_then_text) - 1u, s_now_text, sizeof(s_now_text) - 1u));
    TEST_ASSERT_EQUAL_UINT(2u, s_inner_hunks);
    TEST_ASSERT_EQUAL_UINT(2u, s_hunks);
    hunk_is(0u, 1u, 2u, 1u, 2u);
    hunk_is(1u, 3u, 4u, 3u, 4u);
}
