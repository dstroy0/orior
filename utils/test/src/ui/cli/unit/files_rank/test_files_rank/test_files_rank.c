// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_files_rank.c: quick open and the editor's windows: a name as typed ranks above every looser match and above recently opened files, with
// fuzzy matching off only the text as typed answers, a quick open answers from the recent files until the tree is read, a slice and the
// slices around it are whole numbered lines, and a typed name matches its letters in the name
#include "ui/cli/src/editor/files.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// A window's members as it was written
typedef struct
{
    unsigned long long start;
    unsigned long long end;
    unsigned long long size;
    char text[1u << 16u];
} RankWindow;

static const char *const s_typed[3] = {"src/review.rs", "src/vim_ew.rs", "src/view.rs"};

static char s_out[1u << 18u];
static char s_text[1u << 18u];
static char s_shown[1u << 16u];
static char s_root[CLI_PATH_MAX_LEN];
static char s_requested[128];
static char s_error[1024];
static VerbaLinea s_err_msg;
static RankWindow s_window;
static RankWindow s_nearby;
static unsigned int s_hits[CLI_FUZZY_LETTERS];
static unsigned long long s_window_line;
static unsigned long long s_window_size;

// a whole number member of the shared tree's node
static unsigned long long integer_member(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = node, .name = name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; (member != CLI_JSON_NONE) && (at < json->node[member].length); at += 1ull)
    {
        value = (value * 10ull) + (unsigned long long)(json->text[json->node[member].text + at] - '0');
    }
    return value;
}

// the slice of lines.txt from `from` to `to` read into `window`
static void slice_read(unsigned long long from, unsigned long long to, RankWindow *window)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_out_err_msg_file.slice, CliFilesRootOutErrMsgFileCfg, .root = s_root, .file = "lines.txt", .start = from, .end = to, .out = &out, .err_msg = &s_err_msg));
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &read);
    window->start = integer_member(json, read, "start");
    window->end = integer_member(json, read, "end");
    window->size = integer_member(json, read, "size");
    const char *const text = cli_unity_member_text(read, "text");
    memor.cpy(&(MemoriaCfg){.dst = window->text, .src = text, .bytes = CLI_UNITY_LENGTH(text) + 1ull});
}

// line `number` of the file, as written
static void line_written(VerbaLinea *out, unsigned long long number)
{
    mmgr_line_put(out, "line ");
    mmgr_line_decimal(out, number, 1u);
    mmgr_line_ch(out, ' ');
    for (unsigned long long letter = 0ull; letter < (number % 37ull); letter += 1ull)
    {
        mmgr_line_ch(out, 'x');
    }
    mmgr_line_ch(out, '\n');
}

// a file of 5000 numbered lines and the slice about its byte 2500 read into s_window, its first line's number in s_window_line
static void lines_sliced(void)
{
    cli_unity_folder("files_window", s_root);
    VerbaLinea text = {s_text, sizeof(s_text), 0ull};
    for (unsigned long long number = 0ull; number < 5000ull; number += 1ull)
    {
        line_written(&text, number);
    }
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "lines.txt", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = s_text, .size = text.at));
    slice_read(2500ull, 3000ull, &s_window);
    s_window_line = 0ull;
    for (unsigned long long at = 5ull; (s_window.text[at] >= '0') && (s_window.text[at] <= '9'); at += 1ull)
    {
        s_window_line = (s_window_line * 10ull) + (unsigned long long)(s_window.text[at] - '0');
    }
    s_window_size = CLI_UNITY_LENGTH(s_window.text);
}

// `query` matches `text` past `from` with the hits `required`
static void assert_hits(const char *required, const char *query, const char *text, unsigned long long from)
{
    double score = 0.0;
    unsigned int count = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_query.fuzzy, CliFilesQueryCfg, .query = query, .text = text, .from_u64 = from, .score = &score, .hits = s_hits, .count_pointer = &count));
    char hits[256];
    VerbaLinea line = {hits, sizeof(hits), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "," : "");
        mmgr_line_decimal(&line, s_hits[at], 1u);
    }
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING(required, hits);
}

// `query` matches `text` past `from` with the score `required`
static void assert_score(double required, const char *query, const char *text, unsigned long long from)
{
    double score = 0.0;
    unsigned int count = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_query.fuzzy, CliFilesQueryCfg, .query = query, .text = text, .from_u64 = from, .score = &score, .hits = s_hits, .count_pointer = &count));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, required, score);
}

// `query` does not match `text` past `from`
static void assert_no_fuzzy_match(const char *query, const char *text, unsigned long long from)
{
    double score = 0.0;
    unsigned int count = 0u;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_query.fuzzy, CliFilesQueryCfg, .query = query, .text = text, .from_u64 = from, .score = &score, .hits = s_hits, .count_pointer = &count));
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
}

void tearDown(void)
{
}

void test_a_slice_is_read_about_its_bytes(void)
{
    lines_sliced();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(2500ull, s_window.start);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(3000ull, s_window.end);
    TEST_ASSERT_GREATER_THAN_UINT64(s_window.start, s_window.end);
}

void test_a_slice_is_complete_lines_from_a_lines_first_byte(void)
{
    lines_sliced();
    VerbaLinea first = {s_requested, sizeof(s_requested), 0ull};
    line_written(&first, s_window_line);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = s_window.text, .other_pointer = s_requested, .size = first.at));
    TEST_ASSERT_TRUE((s_window.start == 0ull) || (s_text[s_window.start - 1ull] == '\n'));
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, s_window_size);
    TEST_ASSERT_EQUAL_CHAR('\n', s_window.text[s_window_size - 1u]);
}

void test_the_slice_below_starts_where_it_ends_with_the_line_after_its_last(void)
{
    lines_sliced();
    slice_read(s_window.end, s_window.end + 2000ull, &s_nearby);
    TEST_ASSERT_EQUAL_UINT64(s_window.end, s_nearby.start);
    unsigned long long lines = 0ull;
    for (unsigned long long at = 0ull; at < s_window_size; at += 1ull)
    {
        lines += (s_window.text[at] == '\n') ? 1ull : 0ull;
    }
    VerbaLinea next = {s_requested, sizeof(s_requested), 0ull};
    mmgr_line_put(&next, "line ");
    mmgr_line_decimal(&next, s_window_line + lines, 1u);
    mmgr_line_ch(&next, ' ');
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = s_nearby.text, .other_pointer = s_requested, .size = next.at));
}

void test_the_slice_above_ends_where_it_starts_with_the_line_before_its_first(void)
{
    lines_sliced();
    slice_read((s_window.start > 2000ull) ? (s_window.start - 2000ull) : 0ull, s_window.start, &s_nearby);
    TEST_ASSERT_EQUAL_UINT64(s_window.start, s_nearby.end);
    VerbaLinea before = {s_requested, sizeof(s_requested), 0ull};
    line_written(&before, s_window_line - 1ull);
    const unsigned long long above = CLI_UNITY_LENGTH(s_nearby.text);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(before.at, above);
    TEST_ASSERT_EQUAL_MEMORY(s_requested, s_nearby.text + above - before.at, before.at);
}

void test_a_slice_end_the_end_ends_at_the_files_end(void)
{
    lines_sliced();
    slice_read(999999ull, 1000099ull, &s_nearby);
    TEST_ASSERT_EQUAL_UINT64(s_nearby.size, s_nearby.end);
}

void test_a_typed_name_matches_its_letters_in_the_name(void)
{
    assert_hits("11,12,13,14", "edit", "src/ui/src/edit.js", 11ull);
}

void test_a_name_without_the_letters_does_not_match(void)
{
    assert_no_fuzzy_match("xyz", "src/ui/src/edit.js", 11ull);
}

void test_an_empty_query_matches_with_no_hits(void)
{
    assert_hits("", "", "a.rs", 0ull);
}

void test_each_score_is_the_palettes_own(void)
{
    assert_score(42.2, "view", "src/ui/src/editor/view.js", 18ull);
    assert_score(42.34, "edit", "src/ui/src/edit.js", 11ull);
}

void test_a_match_too_torn_is_no_match(void)
{
    assert_no_fuzzy_match("view", "src/vendor/item/eventwatch.js", 21ull);
}

void test_a_query_torn_into_too_many_pieces_is_no_match(void)
{
    assert_no_fuzzy_match("abcd", "a/x/b/x/c/x/d", 12ull);
}
