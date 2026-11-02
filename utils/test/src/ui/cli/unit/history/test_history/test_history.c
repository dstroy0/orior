// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_history.c: Local History lists saves newest first without repeats, under a home folder of the test's own
#include "ui/cli/src/editor/history.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_list[1u << 16u];

// the digits of a number node of the tree
static unsigned long long number_of(const CliJson *json, unsigned int node)
{
    unsigned long long value = 0ull;
    for (unsigned long long digit = 0ull; EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = node, .kind = CFG_JSON_NUMBER) && (digit < json->node[node].length); digit += 1ull)
    {
        // a digit's difference from zero is not negative
        value = (value * 10ull) + (unsigned long long)(json->text[json->node[node].text + digit] - '0');
    }
    return value;
}

void setUp(void)
{
    cli_unity_begun();
    char home[CLI_PATH_MAX_LEN];
    cli_unity_folder("history_home", home);
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home));
}

void tearDown(void)
{
}

void test_saves_read_newest_first_with_the_repeat_left_out(void)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_history_before.save, CliHistoryBeforeCfg, .root = "/tree", .path = "src/a.c", .before = "zero", .before_size = 4ull, .text = "one", .size = 3ull, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_history_before.save, CliHistoryBeforeCfg, .root = "/tree", .path = "src/a.c", .before = "one", .before_size = 3ull, .text = "one", .size = 3ull, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_history_before.save, CliHistoryBeforeCfg, .root = "/tree", .path = "src/a.c", .before = "one", .before_size = 3ull, .text = "two", .size = 3ull, .err_msg = &err_msg));
    VerbaLinea list = {s_list, sizeof(s_list), 0ull};
    EMBED_CALL(cli_history.list, CliHistoryCfg, .root = "/tree", .path = "src/a.c", .out = &list);
    unsigned int root = 0u;
    const CliJson *const json = cli_unity_parsed(s_list, list.at, &root);
    static char s_texts[4096];
    VerbaLinea texts = {s_texts, sizeof(s_texts), 0ull};
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = root, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        char text[64];
        VerbaLinea one = {text, sizeof(text), 0ull};
        err_msg.at = 0ull;
        (void)EMBED_CALL(cli_history.read, CliHistoryCfg, .root = "/tree", .path = "src/a.c", .at = number_of(json, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = root, .index = at), .name = "at")), .out = &one, .err_msg = &err_msg);
        mmgr_line_put(&texts, (at > 0u) ? "," : "");
        mmgr_line_put_n(&texts, text, mmgr_line_ok(&one) ? one.at : 0ull);
    }
    CLI_TEST_ASSERT_TEXT("two,one,zero", s_texts, texts.at);
}

void test_a_file_never_saved_has_an_empty_history(void)
{
    VerbaLinea list = {s_list, sizeof(s_list), 0ull};
    EMBED_CALL(cli_history.list, CliHistoryCfg, .root = "/tree", .path = "src/b.c", .out = &list);
    CLI_TEST_ASSERT_TEXT("[]", s_list, list.at);
}
