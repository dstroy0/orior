// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_document.c: a text is split as the page splits a file; edits are written from the last to the first; the marks show whether the commit contains the file
// and carry each change's lines; a text is stored until it is released; find returns each match with case and without, a pattern's by the regular expressions the
// colorer reads; a branch is colored by a grammar and read through its view; its uses and brackets read off its
// grammar's roles; texts colored at once by walkers on threads of their own read as one walked alone; and a paged text is colored the same way through the pages
#include "ui/cli/src/editor/document.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/editor/highlight.h"
#include "ui/cli/src/editor/pages.h"
#include "ui/cli/src/plugins/plugins.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the cells the pool is allocated in for a search in little capacity, and the most of them
#define DOCUMENT_TAKEN_CELL (8ull << 20u)
#define DOCUMENT_TAKEN_MOST 64u

// the lines of the branch, a comment opened on the first and closed on the one before the last
#define DOCUMENT_BRANCH_LINES 401u

// the lines of the text an edit moves the state of every line after, and the capacity the text is written in
#define DOCUMENT_ONWARD_LINES 40000ull
#define DOCUMENT_ONWARD_CAPACITY (4ull << 20u)

// the lines of the texts colored at once, a comment opened on the first and closed on the middle one, and how many texts
#define DOCUMENT_AT_ONCE_LINES 3000u
#define DOCUMENT_AT_ONCE_TEXTS 4u

// a grammar whose comments run over lines: `""` is its name 0, `comment` 1 and `identifier` 2
static const char s_branch_grammar[] = "{\"tokenizer\": {\"root\": [[\"\\\\/\\\\*\", \"comment\", \"@comment\"], [\"[a-z]+\", \"identifier\"], "
                                       "[\"\\\\s+\", \"\"]], \"comment\": [[\"\\\\*\\\\/\", \"comment\", \"@pop\"], [\"[^*]+\", \"comment\"], "
                                       "[\"\\\\*\", \"comment\"]]}}";

static const char *const s_at_once_keys[DOCUMENT_AT_ONCE_TEXTS] = {"at_once_0", "at_once_1", "at_once_2", "at_once_3"};

static unsigned short s_units[1u << 12u];
static unsigned long long s_start[256];
static unsigned long long s_length[256];
static CliLines s_lines;
static char s_out[1u << 14u];
static char s_error[256];
static char s_branch[4u * DOCUMENT_BRANCH_LINES];
static char s_folder[CLI_PATH_MAX_LEN];
static int s_begun;

// the colors walkers send from threads of their own counted, under a lock of the suite's own
static unsigned int s_counted;
static char s_at_once[4u * DOCUMENT_AT_ONCE_LINES];
static char s_printed[DOCUMENT_AT_ONCE_TEXTS + 1u][1u << 16u];

// what each step of the live sequences recorded
static int s_branch_run_result;
static int s_branch_step[12];
static int s_at_once_run_result;
static int s_at_once_during;
static int s_at_once_ended;
static unsigned int s_at_once_counted;
static int s_at_once_after;
static int s_paged_run_result;
static int s_paged_step[12];

// `text` split into s_lines as the page splits a file
static const CliLines *lines_of(const char *text)
{
    s_lines = (CliLines){s_units, sizeof(s_units) / sizeof(s_units[0]), 0ull, s_start, s_length, 256u, 0u, 0};
    EMBED_CALL(cli_document_text_size.lines_get, CliDocumentTextSizeCfg, .text = text, .size = CLI_UNITY_LENGTH(text), .lines_out = &s_lines);
    return &s_lines;
}

// what is sought, as the find bar sets it
static CliDocNeedle needle_of(const char *query, int regex, int match_case, int word)
{
    const CliDocNeedle needle = {query, regex, match_case, word};
    return needle;
}

// the matches of what is sought in the lines, asserted against `required`
static void assert_found(const CliLines *lines, CliDocNeedle needle, unsigned long long most, const char *required)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_lines_out.find_in, CliDocumentLinesOutCfg, .lines = lines, .needle = &needle, .most = most, .out = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT(required, s_out, out.at);
}

// the matches of `query` in the text stored under `key`, asserted against `required`
static void assert_stored_found(const char *key, const char *query, const char *required)
{
    const CliDocNeedle needle = needle_of(query, 0, 1, 0);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_document_out_needle.find, CliDocumentOutNeedleCfg, .key = key, .needle = &needle, .most = 100ull, .out = &out, .err_msg = &err_msg);
    CLI_TEST_ASSERT_TEXT(required, s_out, out.at);
}

// the text `edits` stored and edited, its shape in `shape`, asserted accepted
static void edits_written(CliShape *shape)
{
    static const CliEdit s_first[2] = {{{0u, 0u}, {0u, 5u}, "1st", 3u}, {{1u, 7u}, {2u, 2u}, "joined\nand ", 11u}};
    static const CliEdit s_second[1] = {{{2u, 0u}, {2u, 0u}, "", 0u}};
    const CliEdit *const changes[2] = {s_first, s_second};
    const unsigned int counts[2] = {2u, 1u};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "edits", .text = "first line\nsecond line\nthird", .size = 28ull, .shape = shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "edits", .changes = changes, .edits = counts, .count_u32 = 2u, .shape = shape, .err_msg = &err_msg));
}

// the branch's text written into s_branch: its bytes
static unsigned long long branch_text_written(void)
{
    VerbaLinea text = {s_branch, sizeof(s_branch), 0ull};
    mmgr_line_put(&text, "/*\n");
    for (unsigned int line = 1u; line < (DOCUMENT_BRANCH_LINES - 2u); line += 1u)
    {
        mmgr_line_put(&text, "b\n");
    }
    mmgr_line_put(&text, "*/\nc");
    return text.at;
}

// a branch viewed far from its first line, walked and edited, what each step answered recorded, once for the process
static void branch_run_result(void)
{
    CliShape shape;
    if (s_branch_run_result)
    {
        return;
    }
    s_branch_run_result = 1;
    const unsigned long long size = branch_text_written();
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "branch", .text = s_branch, .size = size, .shape = &shape);
    s_branch_step[0] = EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "branch", .grammar = "branch", .named = 1);
    cli_document_walk();
    s_branch_step[6] = EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = "branch", .from = 100ull, .to = 110ull);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "branch");
}

// a paged text viewed far from its first line, read, walked and edited, what each step answered recorded, once for the process
static void paged_run_result(void)
{
    char path[CLI_PATH_MAX_LEN];
    unsigned long long lines = 0ull;
    int whole = 0;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (s_paged_run_result)
    {
        return;
    }
    s_paged_run_result = 1;
    cli_unity_folder("document", s_folder);
    const unsigned long long size = branch_text_written();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "paged.c", .joined = path);
    (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = s_branch, .size = size);
    s_paged_step[0] = EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = "paged", .path = path, .out = &out, .err_msg = &err_msg);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while ((!EMBED_CALL(cli_pages.count, CliPagesCfg, .key = "paged", .lines = &lines, .whole = &whole) || !whole) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 1u);
    }
    s_paged_step[1] = EMBED_CALL(cli_document_key_from.page, CliDocumentKeyFromCfg, .key = "paged", .from = 350ull, .to = 361ull) && EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "paged", .grammar = "branch", .named = 1);
    cli_document_walk();
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "paged");
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "paged");
}

void setUp(void)
{
    unsigned int root = CLI_JSON_NONE;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    cli_unity_begun();
    if (!s_begun)
    {
        s_begun = 1;
        const CliJson *const json = cli_unity_parsed(s_branch_grammar, sizeof(s_branch_grammar) - 1u, &root);
        TEST_ASSERT_TRUE(EMBED_CALL(cli_highlight_key_json.store, CliHighlightKeyJsonCfg, .key = "branch", .json = json, .def = root, .err_msg = &err_msg));
        EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = NULL, .threaded = 0);
    }
}

void tearDown(void)
{
}

void test_a_text_is_split_as_the_page_splits_a_file_a_char_end_the_basic_plane_two_units(void)
{
    const CliLines *const lines = lines_of("one\r\ntwo\nthree\r\r\n\xF0\x9F\x98\x80");
    TEST_ASSERT_EQUAL_UINT(4u, lines->lines);
    TEST_ASSERT_EQUAL_UINT64(6ull, lines->length[2]);
    TEST_ASSERT_EQUAL_UINT16('\r', lines->unit[lines->start[2] + 5u]);
    TEST_ASSERT_EQUAL_UINT64(2ull, lines->length[3]);
}

void test_a_large_text_accepts_letters_one_edit_at_a_time_each_where_its_edit_writes_it(void)
{
    static const CliEdit s_letter[1] = {{{50000u, 0u}, {50000u, 0u}, "x", 1u}};
    const CliEdit *const changes[1] = {s_letter};
    const unsigned int counts[1] = {1u};
    // 100000 lines of 12 letters, in a cell released once the text is stored
    const unsigned long long lines = 100000ull;
    const unsigned long long size = lines * 13ull;
    CliShape shape;
    unsigned long long pool = 0ull;
    unsigned long long before = 0ull;
    unsigned long long after = 0ull;
    unsigned long long temporary = 0ull;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    char *const text = g_cli_guard->persistent_buf_alloc(size, g_checks_owner);
    TEST_ASSERT_NOT_NULL(text);
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        text[at] = ((at % 13ull) == 12ull) ? '\n' : 'a';
    }
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "large", .text = text, .size = size, .shape = &shape);
    g_cli_guard->persistent_buf_release(text, g_checks_owner);
    cli_pool_report(&pool, &before, &temporary);
    int edited = 1;
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        edited = edited && EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "large", .changes = changes, .edits = counts, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg);
    }
    cli_pool_report(&pool, &after, &temporary);
    TEST_ASSERT_TRUE(edited);
    TEST_ASSERT_EQUAL_UINT64(lines + 1ull, shape.lines);
    TEST_ASSERT_EQUAL_UINT64((lines * 12ull) + 20ull, shape.units);
    assert_stored_found("large", "xxxxxxxxxxxxxxxxxxxxa", "[[50000,0,21]]");
    // no edit uses a capacity of the text: the cellblock's peak rises by less than one
    TEST_ASSERT_TRUE((before == 0ull) || ((after - before) < (size * 2ull)));
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "large");
}

void test_edits_are_written_from_the_last_to_the_first_with_the_texts_shape_after_them(void)
{
    CliShape shape;
    edits_written(&shape);
    TEST_ASSERT_EQUAL_UINT64(3ull, shape.lines);
    TEST_ASSERT_EQUAL_UINT64(8ull + 13ull + 7ull, shape.units);
    assert_stored_found("edits", "1st line", "[[0,0,8]]");
    assert_stored_found("edits", "second joined", "[[1,0,13]]");
    assert_stored_found("edits", "and ird", "[[2,0,7]]");
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "edits");
}

void test_an_edit_end_the_text_is_refused_and_the_text_released(void)
{
    static const CliEdit s_end[1] = {{{9u, 0u}, {9u, 0u}, "x", 1u}};
    const CliEdit *const end[1] = {s_end};
    const unsigned int one[1] = {1u};
    CliShape shape;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    edits_written(&shape);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "edits", .changes = end, .edits = one, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = "edits", .shape = &shape));
}

void test_a_text_is_stored_with_its_shape_until_it_is_released(void)
{
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "stored", .text = "one\ntwo\nthree", .size = 13ull, .shape = &shape));
    TEST_ASSERT_EQUAL_UINT64(3ull, shape.lines);
    TEST_ASSERT_EQUAL_UINT64(11ull, shape.units);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "stored");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = "stored", .shape = &shape));
}

// the text stored under `key` put as its file had it, asserted to read `required`
static void assert_file_put(const char *key, const char *required)
{
    char put[64];
    char why[128];
    VerbaLinea out = {put, sizeof(put), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_out.file_put, CliDocumentOutCfg, .key = key, .out = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT(required, put, out.at);
}

void test_a_text_is_put_as_its_file_holds_it_its_line_ends_as_they_were_read(void)
{
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "put", .text = "one\n\xC5\xBFtwo\n", .size = 10ull, .shape = &shape));
    assert_file_put("put", "one\n\xC5\xBFtwo\n");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "put", .text = "a\r\nb", .size = 4ull, .shape = &shape));
    assert_file_put("put", "a\r\nb");
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "put");
    char why[128];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    char put[8];
    VerbaLinea out = {put, sizeof(put), 0ull};
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(cli_document_out.file_put, CliDocumentOutCfg, .key = "put", .out = &out, .err_msg = &err_msg), "no text is stored under the key");
}

void test_find_with_case(void)
{
    assert_found(lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), needle_of("line", 0, 1, 0), 100ull, "[[0,9,13],[1,2,6]]");
}

void test_find_without_case(void)
{
    assert_found(lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), needle_of("line", 0, 0, 0), 100ull, "[[0,0,4],[0,9,13],[1,2,6],[1,7,11]]");
}

void test_find_complete_words(void)
{
    assert_found(lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), needle_of("line", 0, 0, 1), 100ull, "[[0,0,4],[0,9,13],[1,7,11]]");
}

void test_find_folds_the_long_s_and_the_kelvin_sign(void)
{
    assert_found(lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), needle_of("star king", 0, 0, 0), 100ull, "[[2,0,9]]");
}

void test_the_line_that_reaches_the_max_is_searched_complete(void)
{
    unsigned int found = CLI_JSON_NONE;
    const CliDocNeedle most = needle_of("line", 0, 0, 0);
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_document_lines_out.find_in, CliDocumentLinesOutCfg, .lines = lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), .needle = &most, .most = 1ull, .out = &out, .err_msg = &err_msg);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &found);
    TEST_ASSERT_EQUAL_UINT(2u, json->node[found].count);
}

void test_find_without_case_finds_a_letter_outside_ascii_in_either_case(void)
{
    assert_found(lines_of("Caf\xC3\xA9 caf\xC3\x89\nnone"), needle_of("\xC3\xA9", 0, 0, 0), 100ull, "[[0,3,4],[0,8,9]]");
}

void test_complete_words_adjacent_text_outside_ascii_a_mark_ends_a_word_a_letter_does_not(void)
{
    assert_found(lines_of("a line\xD9\x8B here, \xC3\xA9line"), needle_of("line", 0, 1, 1), 100ull, "[[0,2,6]]");
}

void test_a_pattern_is_found_by_the_regular_expressions_the_colorer_reads(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3\xF0\x9F\x98\x80"
                          "ab"),
                 needle_of("x_\\d+", 1, 1, 0), 100ull, "[[0,0,3],[1,0,4]]");
}

void test_a_patterns_cols_are_in_utf16_units_end_a_letter_outside_the_basic_plane(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3\xF0\x9F\x98\x80"
                          "ab"),
                 needle_of("[a-z]b", 1, 1, 0), 100ull, "[[1,10,12]]");
}

void test_a_pattern_that_does_not_read_is_refused(void)
{
    const CliDocNeedle needle = needle_of("(", 1, 1, 0);
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_document_lines_out.find_in, CliDocumentLinesOutCfg, .lines = lines_of("x_1 = 10"), .needle = &needle, .most = 100ull, .out = &out, .err_msg = &err_msg));
}

void test_a_finds_cols_are_in_utf16_units(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3\xF0\x9F\x98\x80"
                          "ab"),
                 needle_of("ab", 0, 1, 0), 100ull, "[[1,10,12]]");
}

void test_a_find_without_case_finds_an_underscored_prefix(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3"), needle_of("X_", 0, 0, 0), 100ull, "[[0,0,2],[1,0,2]]");
}

void test_nothing_found_runs_over_a_lines_end(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3"), needle_of("10\nx", 0, 1, 0), 100ull, "[]");
}

void test_a_small_text_is_searched_while_the_pool_has_little_capacity(void)
{
    // the pool allocated in cells of DOCUMENT_TAKEN_CELL until it refuses, one of them released, the search still answers
    static void *s_allocated[DOCUMENT_TAKEN_MOST];
    CliShape shape;
    unsigned int reserved = 0u;
    while (reserved < DOCUMENT_TAKEN_MOST)
    {
        s_allocated[reserved] = g_cli_guard->persistent_buf_alloc(DOCUMENT_TAKEN_CELL, g_checks_owner);
        if (s_allocated[reserved] == NULL)
        {
            break;
        }
        reserved += 1u;
    }
    if (reserved != 0u)
    {
        reserved -= 1u;
        g_cli_guard->persistent_buf_release(s_allocated[reserved], g_checks_owner);
    }
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "little", .text = "one line\ntwo lines", .size = 18ull, .shape = &shape);
    assert_stored_found("little", "line", "[[0,4,8],[1,4,8]]");
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "little");
    for (unsigned int at = 0u; at < reserved; at += 1u)
    {
        g_cli_guard->persistent_buf_release(s_allocated[at], g_checks_owner);
    }
}

void test_a_stored_text_is_found_and_its_joined_lines_created_again_after_an_edit(void)
{
    static const CliEdit s_split[1] = {{{0u, 3u}, {1u, 0u}, "\n", 1u}};
    const CliEdit *const changes[1] = {s_split};
    const unsigned int counts[1] = {1u};
    CliShape shape;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "joined", .text = "one two\nthree", .size = 13ull, .shape = &shape);
    assert_stored_found("joined", "three", "[[1,0,5]]");
    (void)EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "joined", .changes = changes, .edits = counts, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg);
    assert_stored_found("joined", "three", "[[1,0,5]]");
    assert_stored_found("joined", "two", "[]");
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "joined");
}

void test_a_branch_is_colored_by_a_grammar(void)
{
    branch_run_result();
    TEST_ASSERT_TRUE(s_branch_step[0]);
}

void test_a_text_read_complete_names_the_lines_the_page_stores(void)
{
    branch_run_result();
    TEST_ASSERT_TRUE(s_branch_step[6]);
}

void test_the_branchs_file_opens_as_a_paged_text_colored_by_a_grammar(void)
{
    paged_run_result();
    TEST_ASSERT_TRUE(s_paged_step[0]);
    TEST_ASSERT_TRUE(s_paged_step[1]);
}

