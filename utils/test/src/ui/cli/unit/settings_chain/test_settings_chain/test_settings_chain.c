// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_settings_chain.c: a tree's settings follow each file the one before takes, the one taken first; a file taken
// twice, a file taken that is not there, a file that is no object and a `takes` that is no path are each a problem
#include "ui/cli/src/machine/settings.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[CLI_PATH_MAX_LEN];
static CliChain s_chain;

// the tree's chain read into s_chain, the one before given back
static void chain_read(void)
{
    EMBED_CALL(cli_settings_path_table.tree, CliSettingsPathCfg, .root = s_root, .chain = &s_chain);
}

void setUp(void)
{
    cli_unity_begun();
    char settings[CLI_PATH_MAX_LEN];
    cli_unity_folder("settings_chain", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = ".orior", .joined = settings);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = settings));
}

void tearDown(void)
{
    g_cli_cells->persistent_buf_release(s_chain.json.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = s_chain.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_chain.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    s_chain.files = 0u;
    s_chain.problems = 0u;
}

void test_a_chain_follows_each_file_taken_the_one_taken_first(void)
{
    cli_unity_file(s_root, ".orior/settings.json", "{\"takes\": \"base.json\", \"a\": 1}");
    cli_unity_file(s_root, ".orior/base.json", "{\"takes\": \"deeper.json\", \"b\": 2}");
    cli_unity_file(s_root, ".orior/deeper.json", "{\"c\": 3}");
    chain_read();
    TEST_ASSERT_EQUAL_UINT(3u, s_chain.files);
    TEST_ASSERT_EQUAL_UINT(0u, s_chain.problems);
    CLI_TEST_ASSERT_CONTAINS("deeper.json", s_chain.file[0]);
    CLI_TEST_ASSERT_CONTAINS("base.json", s_chain.file[1]);
    CLI_TEST_ASSERT_CONTAINS("settings.json", s_chain.file[2]);
}

void test_a_file_taken_twice_is_read_once_and_a_problem(void)
{
    cli_unity_file(s_root, ".orior/settings.json", "{\"takes\": \"base.json\"}");
    cli_unity_file(s_root, ".orior/base.json", "{\"takes\": \"settings.json\"}");
    chain_read();
    TEST_ASSERT_EQUAL_UINT(2u, s_chain.files);
    TEST_ASSERT_EQUAL_UINT(1u, s_chain.problems);
    CLI_TEST_ASSERT_CONTAINS("is taken twice in one chain", s_chain.problem[0]);
}

void test_a_file_taken_that_is_not_there_is_a_problem(void)
{
    cli_unity_file(s_root, ".orior/settings.json", "{\"takes\": \"gone.json\"}");
    chain_read();
    TEST_ASSERT_EQUAL_UINT(1u, s_chain.files);
    TEST_ASSERT_EQUAL_UINT(1u, s_chain.problems);
    CLI_TEST_ASSERT_CONTAINS("gone.json: it is not there", s_chain.problem[0]);
}

void test_a_file_that_is_no_object_and_a_takes_that_is_no_path_are_problems(void)
{
    cli_unity_file(s_root, ".orior/settings.json", "{\"takes\": \"list.json\"}");
    cli_unity_file(s_root, ".orior/list.json", "[1, 2]");
    chain_read();
    TEST_ASSERT_EQUAL_UINT(1u, s_chain.files);
    TEST_ASSERT_EQUAL_UINT(1u, s_chain.problems);
    CLI_TEST_ASSERT_CONTAINS("is not an object of settings", s_chain.problem[0]);
    g_cli_cells->persistent_buf_release(s_chain.json.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = s_chain.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_chain.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    s_chain.files = 0u;
    s_chain.problems = 0u;
    cli_unity_file(s_root, ".orior/settings.json", "{\"takes\": 3}");
    chain_read();
    TEST_ASSERT_EQUAL_UINT(1u, s_chain.files);
    TEST_ASSERT_EQUAL_UINT(1u, s_chain.problems);
    CLI_TEST_ASSERT_CONTAINS("`takes` is the path of a file", s_chain.problem[0]);
}

void test_a_tree_with_no_settings_has_an_empty_chain_and_no_problem(void)
{
    chain_read();
    TEST_ASSERT_EQUAL_UINT(0u, s_chain.files);
    TEST_ASSERT_EQUAL_UINT(0u, s_chain.problems);
}
