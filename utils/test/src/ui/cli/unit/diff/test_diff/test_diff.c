// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_diff.c: the editor's diff: added, removed and changed lines are marked, the same text has no marks, texts too far apart have
// none at all, lines that hash alike and differ are distinguished, and a long text marks each line changed far apart
#include "ui/cli/src/editor/document.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

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

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_run_ends_at_a_line_that_hashes_alike_and_differs(void)
{
    static const unsigned short s_line_a[3] = {1u, 2u, 3u};
    static const unsigned short s_line_b[3] = {1u, 2u, 4u};
    static unsigned long long s_start[2] = {0ull, 2ull};
    static unsigned long long s_length[2] = {2ull, 1ull};
    static unsigned long long s_start_b[2] = {0ull, 2ull};
    static unsigned long long s_length_b[2] = {2ull, 1ull};
    static const unsigned long long s_hashes[2] = {7ull, 7ull};
    const CliLines lines_then = {(unsigned short *)s_line_a, 3ull, 3ull, s_start, s_length, 2u, 2u, 0};
    const CliLines lines_now = {(unsigned short *)s_line_b, 3ull, 3ull, s_start_b, s_length_b, 2u, 2u, 0};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_document_then.diff_run_get, CliDocumentThenCfg, .then = &lines_then, .now = &lines_now, .hash_then = s_hashes, .hash_now = s_hashes, .x = 0, .y = 0));
}

