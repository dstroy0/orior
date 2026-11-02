// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_review.c: a change is the declarations it touches, a move is one piece and callers come before what they call, a commit is
// reviewed against its parent with its author, and the compared lines are released
#include "ui/cli/src/code/review.h"
#include "ui/cli/src/code/review_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char *const s_change_paths[3] = {"a.py", "b.py", "notes.md"};
static const char *const s_change_was[3] = {"def main():\n    helper()\n    return 0\n\ndef helper():\n    return "
                                            "1\n\n\ndef moving():\n    x = 1\n    y = 2\n"
                                            "    return x + y\n",
                                            "def other():\n    return 3\n", "one"};
static const char *const s_change_now[3] = {"def main():\n    helper()\n    other()\n    return 0\n\n\ndef helper():\n    return 2\n",
                                            "def other():\n    return moving()\n\n\ndef moving():\n    x = 1\n    y = 2\n    return x + y\n", "two"};
static const char *const s_added_paths[1] = {"c.rs"};
static const char *const s_added_was[1] = {"fn gone() {\n}\n\nfn stays() {\n}\n"};
static const char *const s_added_now[1] = {"fn stays() {\n}\n\nfn fresh() {\n    let a = 1;\n}\n"};

static char s_out[1u << 16u];
static char s_text[1024];
static const CliJson *s_json;
static char s_root[CLI_PATH_MAX_LEN];
static char s_id[128];
static char s_why[512];

// each piece of a review, `path name how from` with `from` `-` where it has none, `|` between them, is `wanted`
static void assert_pieces(const char *wanted, unsigned int review)
{
    VerbaLinea line = {s_text, sizeof(s_text), 0ull};
    const unsigned int pieces = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = review, .name = "pieces");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = pieces, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int piece = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = pieces, .index = at);
        const char *const from = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = piece, .name = "from"), .length_pointer = NULL);
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, cli_unity_member_text(piece, "path"));
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, cli_unity_member_text(piece, "name"));
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, cli_unity_member_text(piece, "how"));
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, (from != NULL) ? from : "-");
    }
    CLI_TEST_ASSERT_TEXT(wanted, s_text, line.at);
}

// a JSON member of a node, written compact, is `wanted`
static void assert_member(const char *wanted, unsigned int node, const char *name)
{
    VerbaLinea line = {s_text, sizeof(s_text), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = node, .name = name), .out = &line, .indent = 0u, .sorted = 0);
    CLI_TEST_ASSERT_TEXT(wanted, s_text, line.at);
}

// the review of `count` files given whole, read into the shared tree: its node
static unsigned int review_of(const char *const *paths, const char *const *was, const char *const *now, unsigned int count)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_review_paths.files, CliReviewPathsCfg, .paths = paths, .was = was, .now = now, .count = count, .out = &out);
    unsigned int review = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &review);
    return review;
}

// a repository of two commits of a.py, the head's id in s_id
static void repository_made(void)
{
    static const char *const s_init[2] = {"init", "-q"};
    static const char *const s_name[3] = {"config", "user.name", "Tester"};
    static const char *const s_email[3] = {"config", "user.email", "tester@example.com"};
    static const char *const s_add[2] = {"add", "."};
    static const char *const s_first[4] = {"commit", "-q", "-m", "first"};
    static const char *const s_second[4] = {"commit", "-q", "-am", "second"};
    const char *const head[4] = {"git", "rev-parse", "HEAD", NULL};
    cli_unity_folder("review_commit", s_root);
    cli_unity_git(s_root, s_init, 2u);
    cli_unity_git(s_root, s_name, 3u);
    cli_unity_git(s_root, s_email, 3u);
    cli_unity_file(s_root, "a.py", "def one():\n    return 1\n");
    cli_unity_git(s_root, s_add, 2u);
    cli_unity_git(s_root, s_first, 4u);
    cli_unity_file(s_root, "a.py", "def one():\n    return two()\n\n\ndef two():\n    return 2\n");
    cli_unity_git(s_root, s_second, 4u);
    CliRan *const ran = cli_ran_take();
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea id = {s_id, sizeof(s_id), 0ull};
    const int ran_well = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = head, .folder = s_root, .settings = NULL, .count = 0u, .ran = ran, .err_msg = &err_msg) && (ran->code == 0);
    for (unsigned long long at = 0ull; ran_well && (at < ran->out_size) && (ran->out[at] > ' '); at += 1ull)
    {
        mmgr_line_ch(&id, ran->out[at]);
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    (void)mmgr_line_finish(&id);
    TEST_ASSERT_TRUE(ran_well);
}

// the review of `id`, the working tree's change where it is NULL, read into the shared tree: its node
static unsigned int commit_reviewed(const char *id)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_review_table.review, CliReviewCfg, .root = s_root, .commit = id, .out = &out, .err_msg = &err_msg));
    unsigned int review = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &review);
    return review;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_change_is_the_declarations_it_touches_and_a_move_is_one_piece(void)
{
    assert_pieces("a.py main changed -|a.py helper changed -|b.py other changed -|b.py moving moved a.py", review_of(s_change_paths, s_change_was, s_change_now, 3u));
}

void test_a_file_of_a_language_not_read_is_named_separately(void)
{
    assert_member("[\"notes.md\"]", review_of(s_change_paths, s_change_was, s_change_now, 3u), "other");
}

void test_a_declarations_trailing_blank_lines_are_left_off(void)
{
    const unsigned int review = review_of(s_change_paths, s_change_was, s_change_now, 3u);
    const unsigned int pieces = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = review, .name = "pieces");
    TEST_ASSERT_EQUAL_STRING("def moving():\n    x = 1\n    y = 2\n    return x + y", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = pieces, .index = 3u), "was"));
}

void test_a_piece_lists_the_pieces_it_calls(void)
{
    const unsigned int review = review_of(s_change_paths, s_change_was, s_change_now, 3u);
    const unsigned int pieces = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = review, .name = "pieces");
    assert_member("[1,2]", EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = pieces, .index = 0u), "calls");
    assert_member("[3]", EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = pieces, .index = 2u), "calls");
}

void test_each_caller_comes_before_what_it_calls_in_one_group(void)
{
    assert_member("[[0,1,2,3]]", review_of(s_change_paths, s_change_was, s_change_now, 3u), "groups");
}

void test_an_added_or_removed_declaration_is_one_piece(void)
{
    assert_pieces("c.rs gone removed -|c.rs fresh added -", review_of(s_added_paths, s_added_was, s_added_now, 1u));
}

void test_pieces_that_call_none_are_separate_groups(void)
{
    assert_member("[[0],[1]]", review_of(s_added_paths, s_added_was, s_added_now, 1u), "groups");
}

void test_two_of_three_lines_the_same_is_a_move(void)
{
    static const char *const s_paths[2] = {"d.py", "e.py"};
    static const char *const s_was[2] = {"def f():\n    a = 1\n    b = 2\n    c = 3\n", NULL};
    static const char *const s_now[2] = {NULL, "def f():\n    a = 1\n    b = 2\n    c = 4\n"};
    assert_pieces("e.py f moved d.py", review_of(s_paths, s_was, s_now, 2u));
}

void test_a_commit_is_reviewed_with_its_author_and_subject(void)
{
    repository_made();
    const unsigned int review = commit_reviewed(s_id);
    TEST_ASSERT_EQUAL_STRING("Tester", cli_unity_member_text(review, "by"));
    TEST_ASSERT_EQUAL_STRING("second", cli_unity_member_text(review, "subject"));
}

void test_a_commit_is_reviewed_against_its_parent(void)
{
    repository_made();
    assert_pieces("a.py one changed -|a.py two added -", commit_reviewed(s_id));
}

void test_a_commits_caller_comes_before_what_it_calls(void)
{
    repository_made();
    assert_member("[[0,1]]", commit_reviewed(s_id), "groups");
}

void test_the_working_trees_change_has_no_author(void)
{
    repository_made();
    cli_unity_file(s_root, "a.py", "def one():\n    return 3\n");
    const unsigned int review = commit_reviewed(NULL);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = review, .name = "by"), .kind = CFG_JSON_NULL));
}

void test_the_working_tree_is_reviewed_against_its_last_commit(void)
{
    repository_made();
    cli_unity_file(s_root, "a.py", "def one():\n    return 3\n");
    assert_pieces("a.py one changed -|a.py two removed -", commit_reviewed(NULL));
}

void test_every_texts_compared_lines_are_released(void)
{
    (void)review_of(s_change_paths, s_change_was, s_change_now, 3u);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_review_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_review_cells.hw));
}
