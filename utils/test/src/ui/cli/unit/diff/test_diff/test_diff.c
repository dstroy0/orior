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

// `size` bytes of `text` read into `side`'s lines
static const CliLines *lines_of(DiffSide *side, const char *text, unsigned long long size)
{
    side->lines = (CliLines){side->unit, DIFF_UNITS, 0ull, side->start, side->length, DIFF_LINES, 0u, 0};
    EMBED_CALL(cli_document_text_size.lines_get, CliDocumentTextSizeCfg, .text = text, .size = size, .lines_out = &side->lines);
    return &side->lines;
}

// the marks of `now` against `then`, written into s_out: their size, 0 where the texts are too far apart
static unsigned long long changes_of(const char *then, unsigned long long then_size, const char *now, unsigned long long now_size)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    return EMBED_CALL(cli_document_out_then.diff_line_changes, CliDocumentOutThenCfg, .then = lines_of(&s_then, then, then_size), .now = lines_of(&s_now, now, now_size), .out = &out) ? out.at : 0ull;
}

// the member `name` of the marks, written compact, is `wanted`
static void assert_member(const char *wanted, unsigned long long size, const char *name)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_out, size, &root);
    char text[1024];
    VerbaLinea line = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = name), .out = &line, .indent = 0u, .sorted = 0);
    CLI_TEST_ASSERT_TEXT(wanted, text, line.at);
}

// the marks of a text of five lines with one changed, one added and one removed: their size
static unsigned long long small_changes(void)
{
    return changes_of("a\nb\nc\nd\ne", 9ull, "a\nB\nc\nnew\nd", 11ull);
}

// the marks of two texts of 50000 lines, every 5000th from 1000 changed: their size
static unsigned long long long_changes(void)
{
    VerbaLinea then = {s_then_text, sizeof(s_then_text), 0ull};
    VerbaLinea now = {s_now_text, sizeof(s_now_text), 0ull};
    for (unsigned int at = 0u; at < 50000u; at += 1u)
    {
        mmgr_line_put(&then, (at > 0u) ? "\nx_" : "x_");
        mmgr_line_decimal(&then, at, 1u);
        mmgr_line_put(&then, " = ");
        mmgr_line_decimal(&then, at, 1u);
        mmgr_line_put(&then, " * 2");
        mmgr_line_put(&now, (at > 0u) ? "\nx_" : "x_");
        mmgr_line_decimal(&now, at, 1u);
        mmgr_line_put(&now, " = ");
        mmgr_line_decimal(&now, at, 1u);
        mmgr_line_put(&now, ((at >= 1000u) && (((at - 1000u) % 5000u) == 0u)) ? " * 2 changed" : " * 2");
    }
    return changes_of(s_then_text, then.at, s_now_text, now.at);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_changed_line_is_marked_changed(void)
{
    assert_member("[1]", small_changes(), "changed");
}

void test_an_added_line_is_marked_added(void)
{
    assert_member("[3]", small_changes(), "added");
}

void test_a_removed_line_is_marked_below_the_last_line(void)
{
    assert_member("[5]", small_changes(), "removed");
}

void test_each_change_has_its_hunk(void)
{
    assert_member("[{\"now\":[1,2],\"then\":[1,2],\"thenLines\":[\"b\"]},{\"now\":[3,4],\"then\":[3,3]},"
                  "{\"now\":[5,5],\"then\":[4,5],\"thenLines\":[\"e\"]}]",
                  small_changes(), "hunks");
}

void test_the_same_text_has_no_marks(void)
{
    const unsigned long long size = changes_of("a\nb", 3ull, "a\nb", 3ull);
    assert_member("[]", size, "added");
    assert_member("[]", size, "changed");
    assert_member("[]", size, "removed");
    assert_member("[]", size, "hunks");
}

void test_texts_too_far_apart_have_no_marks_at_all(void)
{
    VerbaLinea then = {s_then_text, sizeof(s_then_text), 0ull};
    VerbaLinea now = {s_now_text, sizeof(s_now_text), 0ull};
    for (unsigned int at = 0u; at < 4000u; at += 1u)
    {
        mmgr_line_put(&then, "old ");
        mmgr_line_decimal(&then, at, 1u);
        mmgr_line_ch(&then, '\n');
        mmgr_line_put(&now, "new ");
        mmgr_line_decimal(&now, at, 1u);
        mmgr_line_ch(&now, '\n');
    }
    TEST_ASSERT_EQUAL_UINT64(0ull, changes_of(s_then_text, then.at, s_now_text, now.at));
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

void test_a_long_text_marks_each_line_changed_far_apart(void)
{
    const unsigned long long size = long_changes();
    char wanted[256];
    VerbaLinea line = {wanted, sizeof(wanted), 0ull};
    mmgr_line_ch(&line, '[');
    for (unsigned int at = 1000u; at < 50000u; at += 5000u)
    {
        mmgr_line_put(&line, (at > 1000u) ? "," : "");
        mmgr_line_decimal(&line, at, 1u);
    }
    mmgr_line_ch(&line, ']');
    (void)mmgr_line_finish(&line);
    assert_member(wanted, size, "changed");
}

void test_a_long_text_of_changed_lines_adds_and_removes_none(void)
{
    const unsigned long long size = long_changes();
    assert_member("[]", size, "added");
    assert_member("[]", size, "removed");
}
