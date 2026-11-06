// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_files_rank.c: quick open and the editor's windows: a name as typed ranks above every looser match and above recently opened files, with
// fuzzy matching off only the text as typed answers, a quick open answers from the recent files until the tree is read, and a typed name
// matches its letters in the name
#include "ui/cli/src/code/symbols.h"
#include "ui/cli/src/editor/files.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char *const s_typed[3] = {"src/review.rs", "src/vim_ew.rs", "src/view.rs"};

static char s_shown[1u << 16u];
static char s_requested[128];
static char s_error[1024];
static VerbaLinea s_err_msg;
static unsigned int s_hits[CLI_FUZZY_LETTERS];

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

// the ranked tree's folder, and the answer of the last find
static char s_tree[CLI_PATH_MAX_LEN];
static CliFilesRanked s_ranked;

// the file `name` of the ranked tree written, its folder `folder` made first where it is not NULL
static void tree_file(const char *folder, const char *name)
{
    if (folder != NULL)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = folder, .joined = path);
        (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
    }
    cli_unity_file(s_tree, name, "x");
}

// the tree of the ranked finds laid and listed the first time it is asked
static void tree_listed(void)
{
    if (s_tree[0] == '\0')
    {
        cli_unity_folder("build/harness/files_rank_tree", s_tree);
        // a repository of its own, so that git lists its files and not the ignored folder it stands in
        const char *const init[2] = {"init", "-q"};
        cli_unity_git(s_tree, init, 2u);
        tree_file("src", "src/view.rs");
        tree_file(NULL, "src/view_model.rs");
        tree_file(NULL, "src/review.rs");
        tree_file("a", "a/notes.txt");
        tree_file("b", "b/notes.txt");
    }
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_tree);
}

// the files of `root` that answer `query` with `fuzzy`, `recents` of `recent` ranked lately, into s_ranked: how many
static unsigned int ranked(const char *root, const char *query, int fuzzy, const char *const *recent,
                           unsigned int recents)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_rows.ranked_find, CliFilesRootRowsCfg, .root = root, .rows = &s_ranked,
                                .query = query, .recent = recent, .recents = recents, .most = 10u, .fuzzy = fuzzy,
                                .owner = g_cli_files_views_owner));
    return s_ranked.count;
}

void test_a_name_as_typed_outranks_one_that_starts_with_it_which_outranks_a_longer_match(void)
{
    tree_listed();
    TEST_ASSERT_EQUAL_UINT(3u, ranked(s_tree, "view", 1, NULL, 0u));
    TEST_ASSERT_EQUAL_STRING("src/view.rs", s_ranked.row[0].path);
    TEST_ASSERT_EQUAL_STRING("src/view_model.rs", s_ranked.row[1].path);
    TEST_ASSERT_EQUAL_STRING("src/review.rs", s_ranked.row[2].path);
    TEST_ASSERT_TRUE(s_ranked.row[0].score > s_ranked.row[1].score);
    TEST_ASSERT_TRUE(s_ranked.row[1].score > s_ranked.row[2].score);
    TEST_ASSERT_FALSE(s_ranked.reading);
    // the name's letters the match took, counted in the path's letters
    TEST_ASSERT_EQUAL_UINT(4u, s_ranked.row[0].hit_count);
    TEST_ASSERT_EQUAL_UINT(4u, s_ranked.hit[s_ranked.row[0].hit_first]);
    TEST_ASSERT_EQUAL_UINT(7u, s_ranked.hit[s_ranked.row[0].hit_first + 3u]);
}

void test_a_file_opened_lately_outranks_an_equal_match(void)
{
    tree_listed();
    TEST_ASSERT_EQUAL_UINT(2u, ranked(s_tree, "notes", 1, NULL, 0u));
    TEST_ASSERT_EQUAL_STRING("a/notes.txt", s_ranked.row[0].path);
    const char *const recent[1] = {"b/notes.txt"};
    TEST_ASSERT_EQUAL_UINT(2u, ranked(s_tree, "notes", 1, recent, 1u));
    TEST_ASSERT_EQUAL_STRING("b/notes.txt", s_ranked.row[0].path);
    TEST_ASSERT_EQUAL_STRING("a/notes.txt", s_ranked.row[1].path);
}

void test_with_fuzzy_off_a_path_answers_only_where_it_contains_the_query(void)
{
    tree_listed();
    TEST_ASSERT_EQUAL_UINT(1u, ranked(s_tree, "vmr", 1, NULL, 0u));
    TEST_ASSERT_EQUAL_UINT(0u, ranked(s_tree, "vmr", 0, NULL, 0u));
    TEST_ASSERT_NULL(s_ranked.cell);
    TEST_ASSERT_EQUAL_UINT(2u, ranked(s_tree, "ew.r", 0, NULL, 0u));
    TEST_ASSERT_EQUAL_UINT(4u, s_ranked.row[0].hit_count);
}

void test_with_the_tree_unread_only_the_recent_files_answer_and_reading_is_1(void)
{
    tree_listed();
    static char s_other[CLI_PATH_MAX_LEN];
    cli_unity_folder("build/harness/files_rank_other", s_other);
    cli_unity_file(s_other, "one.c", "x");
    const char *const recent[2] = {"x/one.c", "y/two.c"};
    TEST_ASSERT_EQUAL_UINT(1u, ranked(s_other, "one", 1, recent, 2u));
    TEST_ASSERT_EQUAL_STRING("x/one.c", s_ranked.row[0].path);
    TEST_ASSERT_TRUE(s_ranked.reading);
    g_cli_guard->persistent_buf_release(s_ranked.cell, g_cli_files_views_owner);
    s_ranked = (CliFilesRanked){NULL, 0u, NULL, NULL, 0};
}
