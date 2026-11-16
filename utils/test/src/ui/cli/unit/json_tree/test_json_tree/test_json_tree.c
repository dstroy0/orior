// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_json_tree.c: the size a tree reports for its written form holds everything the tree writes on one line
#include "ui/cli/src/command_line/pool.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// `text` parsed and written into a buffer of the size the tree reports: the output is `text` again, whole
static void assert_round_trip(const char *text)
{
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json, .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = text, .size = CLI_UNITY_LENGTH(text), .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = g_cli_text_owner);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, root);
    const unsigned long long capacity = EMBED_CALL(cli_json_tree_node_out.json_write_capacity, CliJsonTreeNodeOutCfg, .json_read = &json);
    char *const written = g_cli_guard->temporary_buf_alloc(capacity, g_cli_text_owner);
    TEST_ASSERT_NOT_NULL(written);
    VerbaLinea line = mmgr_line_over(written, capacity);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = root, .out = &line, .indent = 0u, .sorted = 0);
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
    CLI_TEST_ASSERT_TEXT(text, written, line.at);
    g_cli_guard->persistent_buf_release(written, g_cli_text_owner);
    g_cli_guard->persistent_buf_release(json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

// `text` refused as JSON, the reason containing `reason`
static void assert_refused(const char *text, const char *reason)
{
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json, .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = text, .size = CLI_UNITY_LENGTH(text), .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = g_cli_text_owner);
    (void)mmgr_line_finish(&err_msg);
    g_cli_guard->persistent_buf_release(json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, root);
    TEST_ASSERT_NOT_NULL_MESSAGE(cellul.find(&(CatenaFinitaCfg){.src = why, .cap = CLI_POOL_BYTES, .other = reason, .other_cap = CLI_POOL_BYTES}), why);
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

void test_a_string_is_quoted_with_its_escapes_past_a_words_width(void)
{
    // ten plain bytes, then a quote, a backslash and a control byte, each escaped, and a byte past ASCII as it is
    char written[64];
    VerbaLinea line = mmgr_line_over(written, sizeof(written));
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .text = "abcdefghij\"k\\l\x01m\xc3\xa9", .length = 18ull, .out = &line);
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
    CLI_TEST_ASSERT_TEXT("\"abcdefghij\\\"k\\\\l\\u0001m\xc3\xa9\"", written, line.at);
}

static char s_written[512];
static char s_why[256];

// a tree read from `text` with room for `nodes_more` nodes and `text_more` bytes to build on: its root
static unsigned int tree_read(CliJson *json, const char *text, unsigned int nodes_more, unsigned long long text_more)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = json, .text = text, .size = CLI_UNITY_LENGTH(text), .nodes_more = nodes_more, .text_more = text_more, .err_msg = &err_msg, .owner = g_cli_text_owner);
}

// the tree's cell given back
static void tree_released(CliJson *json)
{
    g_cli_guard->persistent_buf_release(json->node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json->node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
}

// the node `node` written on one line, NUL-terminated in s_written
static const char *written_of(const CliJson *json, unsigned int node)
{
    VerbaLinea line = mmgr_line_over(s_written, sizeof(s_written));
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = node, .out = &line, .indent = 0u, .sorted = 0);
    (void)mmgr_line_finish(&line);
    return s_written;
}

void test_a_tree_is_built_with_members_set_removed_sorted_appended_copied_and_measured(void)
{
    CliJson json;
    const unsigned int root = tree_read(&json, "{}", 32u, 256ull);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, root);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_nodes.json_capacity, CliJsonTreeNodesCfg, .json = &json, .nodes = 4u, .text_u64 = 16ull));
    const unsigned int number = EMBED_CALL(cli_json_tree_text_length_type.json_number_init, CliJsonTreeTextLengthTypeCfg, .json = &json, .text = "12", .length = 2ull);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = root, .name = "b", .value = number);
    const unsigned int array = EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = &json, .type = CFG_JSON_ARRAY);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = &json, .array = array, .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg, .json = &json, .text = "x", .length = 1ull));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = root, .name = "a", .value = array);
    TEST_ASSERT_EQUAL_STRING("{\"b\":12,\"a\":[\"x\"]}", written_of(&json, root));
    // a member set again takes the place of the one it had
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = root, .name = "b", .value = EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = &json, .type = CFG_JSON_TRUE));
    TEST_ASSERT_EQUAL_STRING("{\"b\":true,\"a\":[\"x\"]}", written_of(&json, root));
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = &json, .object = root);
    TEST_ASSERT_EQUAL_STRING("{\"a\":[\"x\"],\"b\":true}", written_of(&json, root));
    EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = &json, .object = root, .name = "b");
    TEST_ASSERT_EQUAL_STRING("{\"a\":[\"x\"]}", written_of(&json, root));
    unsigned int nodes = 0u;
    unsigned long long text = 0ull;
    EMBED_CALL(cli_json_tree_node_nodes.json_extent, CliJsonTreeNodeNodesCfg, .json_read = &json, .node = root, .nodes_pointer = &nodes, .text_u64s = &text);
    TEST_ASSERT_EQUAL_UINT(3u, nodes);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(2ull, text);
    const unsigned int copied = EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = &json, .node = array);
    TEST_ASSERT_EQUAL_STRING("[\"x\"]", written_of(&json, copied));
    CliJson other;
    const unsigned int other_root = tree_read(&other, "[]", 8u, 64ull);
    const unsigned int moved = EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .node = root, .from = &json, .to = &other);
    TEST_ASSERT_EQUAL_STRING("{\"a\":[\"x\"]}", written_of(&other, moved));
    TEST_ASSERT_EQUAL_STRING("[]", written_of(&other, other_root));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(EMBED_CALL(cli_json_tree_node_out.json_write_capacity, CliJsonTreeNodeOutCfg, .json_read = &json, .node = root), EMBED_CALL(cli_json_tree_node_out.json_pretty_capacity, CliJsonTreeNodeOutCfg, .json_read = &json, .node = root, .indent = 2u));
    // a capacity the tree has not marks it full, and a clear takes the mark off
    TEST_ASSERT_FALSE(EMBED_CALL(cli_json_tree_nodes.json_capacity, CliJsonTreeNodesCfg, .json = &json, .nodes = 100000u, .text_u64 = 16ull));
    EMBED_CALL(cli_json_tree.json_clear, CliJsonTreeCfg, .json = &json);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_nodes.json_capacity, CliJsonTreeNodesCfg, .json = &json, .nodes = 4u, .text_u64 = 16ull));
    tree_released(&other);
    tree_released(&json);
}

void test_json_is_written_compact_a_double_as_serde_writes_it_and_a_values_end_found(void)
{
    char buffer[64];
    VerbaLinea out = mmgr_line_over(buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = " { \"a\" : [ 1 , 2 ] } ", .size = 21ull, .out = &out));
    CLI_TEST_ASSERT_TEXT("{\"a\":[1,2]}", buffer, out.at);
    out = mmgr_line_over(buffer, sizeof(buffer));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = "{\"a\":", .size = 5ull, .out = &out));
    out = mmgr_line_over(buffer, sizeof(buffer));
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = &out, .value_f64 = 0.5);
    CLI_TEST_ASSERT_TEXT("0.5", buffer, out.at);
    out = mmgr_line_over(buffer, sizeof(buffer));
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = &out, .value_f64 = 100000.0);
    CLI_TEST_ASSERT_TEXT("100000.0", buffer, out.at);
    out = mmgr_line_over(buffer, sizeof(buffer));
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = &out, .value_f64 = 1.5e300);
    CLI_TEST_ASSERT_TEXT("1.5e300", buffer, out.at);
    out = mmgr_line_over(buffer, sizeof(buffer));
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .text_bytes = (const unsigned char *)"a\"b\n", .length = 4ull, .out = &out);
    CLI_TEST_ASSERT_TEXT("a\\\"b\\n", buffer, out.at);
    TEST_ASSERT_EQUAL_UINT64(4ull, EMBED_CALL(cli_json_tree_text_size.json_value_end, CliJsonTreeTextSizeCfg, .text = "  12 ,", .size = 6ull, .at = 0ull));
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_json_tree_text_size.json_value_end, CliJsonTreeTextSizeCfg, .text = "[1,", .size = 3ull, .at = 0ull));
}

void test_a_file_is_read_into_a_tree_and_a_missing_file_is_refused_with_why(void)
{
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea joined = {path, sizeof(path), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    cli_unity_folder("json_file", root);
    cli_unity_file(root, "a.json", "{\"k\":[1]}");
    mmgr_line_put(&joined, root);
    mmgr_line_put(&joined, "/a.json");
    (void)mmgr_line_finish(&joined);
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json, .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    const unsigned int read = EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = &json, .path = path, .err_msg = &err_msg, .owner = g_cli_text_owner);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, read);
    TEST_ASSERT_EQUAL_STRING("{\"k\":[1]}", written_of(&json, read));
    tree_released(&json);
    joined.at = 0ull;
    mmgr_line_put(&joined, root);
    mmgr_line_put(&joined, "/none.json");
    (void)mmgr_line_finish(&joined);
    err_msg.at = 0ull;
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = &json, .path = path, .err_msg = &err_msg, .owner = g_cli_text_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, err_msg.at);
}

void test_a_tree_is_stored_in_one_cell_and_pinned_back_as_it_was(void)
{
    CliJson json;
    CliJson pinned;
    CliJsonStored stored = {NULL, NULL, 0u, 0ull};
    const unsigned int root = tree_read(&json, "{\"a\":[1,2],\"b\":\"text\"}", 0u, 0ull);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, root);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_stored.json_stored, CliJsonTreeStoredCfg, .json_read = &json, .stored = &stored, .owner = g_cli_text_owner));
    TEST_ASSERT_NOT_NULL(stored.cell);
    TEST_ASSERT_EQUAL_UINT(json.nodes, stored.nodes);
    tree_released(&json);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_stored.json_pinned, CliJsonTreeStoredCfg, .stored_read = &stored, .json = &pinned));
    TEST_ASSERT_EQUAL_STRING("{\"a\":[1,2],\"b\":\"text\"}", written_of(&pinned, root));
    tree_released(&pinned);
    cli_json_stored_release(&(CliJsonStoredCfg){.stored = &stored});
    TEST_ASSERT_NULL(stored.cell);
}

void test_a_string_is_quoted_piece_by_piece_with_its_escapes_and_a_character_split_over_two_pieces(void)
{
    char buffer[128];
    char piece[5];
    VerbaLinea out = {buffer, sizeof(buffer), 0ull};
    VerbaLinea line;
    CliJsonQuoting quoting;
    EMBED_CALL(cli_json_tree_out.json_quoting_begin, CliJsonTreeOutCfg, .out = &out, .quoting = &quoting, .line = &line, .piece = piece, .piece_size = sizeof(piece));
    mmgr_line_put(&line, "ab\"c\\d\n");
    mmgr_line_put(&line, "\xe2\x82\xac!");
    EMBED_CALL(cli_json_tree_out.json_quoting_end, CliJsonTreeOutCfg, .out = &out, .quoting = &quoting, .line = &line);
    CLI_TEST_ASSERT_TEXT("\"ab\\\"c\\\\d\\n\xe2\x82\xac!\"", buffer, out.at);
}
