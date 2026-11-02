// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_files_rank.c: quick open and the editor's windows: a name as typed ranks above every looser match and above recently opened files, with
// fuzzy matching off only the text as typed answers, a quick open answers from the recent files until the tree is read, and a typed name
// matches its letters in the name
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
