// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_json_tree.c: the size a tree reports for its written form holds everything the tree writes on one line
#include "ui/cli/src/command_line/cells.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// `text` parsed and written into a buffer of the size the tree reports: the output is `text` again, whole
static void assert_round_trip(const char *text)
{
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json, .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = text, .size = CLI_UNITY_LENGTH(text), .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_cli_text_cells);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, root);
    const unsigned long long capacity = EMBED_CALL(cli_json_tree_node_out.json_write_capacity, CliJsonTreeNodeOutCfg, .json_read = &json);
    char *const written = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_text_cells);
    TEST_ASSERT_NOT_NULL(written);
    VerbaLinea line = mmgr_line_over(written, capacity);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = root, .out = &line, .indent = 0u, .sorted = 0);
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
    CLI_TEST_ASSERT_TEXT(text, written, line.at);
    g_cli_cells->persistent_buf_release(written, &g_cli_text_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}

// `text` refused as JSON, the reason containing `reason`
static void assert_refused(const char *text, const char *reason)
{
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json, .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = text, .size = CLI_UNITY_LENGTH(text), .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_cli_text_cells);
    (void)mmgr_line_finish(&err_msg);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, root);
    TEST_ASSERT_NOT_NULL_MESSAGE(cellul.find(&(CatenaFinitaCfg){.src = why, .cap = CLI_CELLS_POOL, .other = reason, .other_cap = CLI_CELLS_POOL}), why);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_empty_object(void)
{
    assert_round_trip("{}");
}

void test_empty_array(void)
{
    assert_round_trip("[]");
}

void test_literals_which_have_no_text(void)
{
    assert_round_trip("[null,null,null,true,false]");
}

void test_nested_arrays_whose_nodes_have_no_text(void)
{
    assert_round_trip("[[[[[[[[]]]]]]]]");
}

void test_control_bytes_each_written_as_six(void)
{
    assert_round_trip("{\"\\u0001\\u0002\":\"\\u0003\\u0004\\u0005\"}");
}

void test_number_escapes_and_empty_key(void)
{
    assert_round_trip("{\"a\":-1.5e300,\"b\":[\"\\\"\",\"\\\\\"],\"\":\"\"}");
}

void test_a_value_after_a_close_with_no_comma_between_is_refused(void)
{
    assert_refused("[[1] 2]", "no comma or close after it");
    assert_refused("{\"a\":{} \"b\":1}", "no comma or close after it");
}
