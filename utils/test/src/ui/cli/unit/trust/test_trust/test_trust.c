// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_trust.c: workspace trust: a hook is a program and not git's own monitor, a folder contains itself and not a sibling that
// shares its prefix, the code a tree runs is found, and the nearest saved answer wins
#include "ui/cli/src/machine/trust.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_home[CLI_PATH_MAX_LEN];
static char s_trees[CLI_PATH_MAX_LEN];
static char s_tree[CLI_PATH_MAX_LEN];
static char s_answers[CLI_PATH_MAX_LEN];
static char s_error[512];

// the file `name` under the tree written with `text`
static void tree_file(const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = name, .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text, .size = CLI_UNITY_LENGTH(text)));
}

// the answer saved for the tree: 1 trusted, 0 not, -1 none
static int tree_answer(void)
{
    int trusted = 0;
    return EMBED_CALL(cli_trust_tree.answer_in, CliTrustTreeCfg, .path = s_answers, .tree = s_tree, .trusted = &trusted) ? trusted : -1;
}

void setUp(void)
{
    cli_unity_begun();
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("trust", s_home);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "trees", .joined = s_trees);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_trees, .relative = "crate", .joined = s_tree);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "src", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "trust.json", .joined = s_answers);
}

void tearDown(void)
{
}

void test_a_program_as_fsmonitor_is_a_hook(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_trust_table.names_a_hook, CliTrustCfg, .config = "[core]\n\tfsmonitor = .git/hooks/watch\n"));
}

void test_gits_own_monitor_is_not_a_hook(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_trust_table.names_a_hook, CliTrustCfg, .config = "[core]\n\tfsmonitor = true\n"));
}

void test_a_key_of_another_section_is_not_a_hook(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_trust_table.names_a_hook, CliTrustCfg, .config = "[remote \"origin\"]\n\tfsmonitor = run\n"));
}

void test_a_folder_contains_itself_and_what_is_under_it(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_trust_folder_inner.under, CliTrustFolderInnerCfg, .inner = "d:/trees/one", .folder = "d:/trees"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_trust_folder_inner.under, CliTrustFolderInnerCfg, .inner = "d:/trees", .folder = "d:/trees"));
}

void test_a_folder_does_not_contain_a_sibling_sharing_its_prefix(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_trust_folder_inner.under, CliTrustFolderInnerCfg, .inner = "d:/treesmore/one", .folder = "d:/trees"));
}

void test_the_code_a_tree_runs_is_found_and_released(void)
{
    tree_file("Cargo.toml", "[package]\nname = \"crate\"\n");
    tree_file("build.rs", "fn main() {}\n");
    tree_file("src/lib.rs", "");
    tree_file("prettier.config.js", "module.exports = {};\n");
    CliTrustCode code = {NULL, 0ull, 0u};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_trust_tree_code.looked_for, CliTrustTreeCodeCfg, .tree = s_tree, .code = &code));
    TEST_ASSERT_EQUAL_UINT(2u, code.count);
    TEST_ASSERT_EQUAL_STRING("build.rs", code.paths);
    TEST_ASSERT_EQUAL_STRING("prettier.config.js", code.paths + CLI_UNITY_LENGTH(code.paths) + 1u);
    g_cli_guard->persistent_buf_release(code.paths, g_cli_trust_owner);
    *(&code) = (CliTrustCode){NULL, 0ULL, 0u};
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_trust_owner));
}

void test_a_tree_with_no_answer_has_none(void)
{
    TEST_ASSERT_EQUAL_INT(-1, tree_answer());
}

void test_an_answer_set_is_read_back_and_the_last_answer_stands(void)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_trust_folder.answer_set, CliTrustFolderCfg, .folder = s_tree, .trusted_value = 1,
                                .path = s_answers, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_INT(1, tree_answer());
    TEST_ASSERT_TRUE(EMBED_CALL(cli_trust_folder.answer_set, CliTrustFolderCfg, .folder = s_tree, .trusted_value = 0,
                                .path = s_answers, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_INT(0, tree_answer());
    // the folder stands in one list alone after the answers change
    TEST_ASSERT_TRUE(EMBED_CALL(cli_trust_folder.answer_set, CliTrustFolderCfg, .folder = s_tree, .trusted_value = 1,
                                .path = s_answers, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_INT(1, tree_answer());
}

void test_git_in_an_untrusted_tree_runs_no_hook(void)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = ".git", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    tree_file(".git/config", "[core]\n\tfsmonitor = watch\n");
    // a home of the test's own holding no answer: the machine's own answers may trust the folders the tree lies in
    char bare_home[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "bare_home", .joined = bare_home);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = bare_home));
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = bare_home));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "src", .joined = path);
    const char *words[2] = {NULL, NULL};
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_trust_tree_words.git_words, CliTrustTreeWordsCfg, .tree = path, .words = words));
    TEST_ASSERT_EQUAL_STRING("-c", words[0]);
    TEST_ASSERT_EQUAL_STRING("core.fsmonitor=false", words[1]);
}
