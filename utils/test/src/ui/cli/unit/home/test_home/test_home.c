// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_home.c: orior's home folder: the generated menus file is JSON with empty lists, and state writes merge into what is saved
#include "ui/cli/src/window/serve_internal.h"
#include "ui/cli/src/window/window_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_why[1024];
static char s_out[4096];
static char s_text[1u << 16u];

// the home-folder call `name` answered from the JSON `args`, into s_out: its size, the test failing where the call fails
static unsigned long long called(const char *name, const char *args)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    for (unsigned int at = 0u; at < g_cli_window_home_count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_window_home_calls[at].name, .other = name, .cap = 64ull}))
        {
            TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_serve_size_internal.answer, CliServeSizeCtx, .answer = g_cli_window_home_calls[at].answer, .args_text = args, .size = CLI_UNITY_LENGTH(args), .out = &out, .err_msg = &err_msg), name);
            return out.at;
        }
    }
    TEST_FAIL_MESSAGE(name);
    return 0ull;
}

// the generated menus file parsed: its tree, its root in `*made`
static const CliJson *menus_made(unsigned int *made)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    char path[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_err_msg_kind_internal.home_ensure_menus, CliPathErrMsgKindCtx, .path_out = path, .err_msg = &err_msg));
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    return cli_unity_parsed(s_text, size, made);
}

// the member `key` of the generated menus file is an empty list
static void assert_empty_list(const char *key)
{
    unsigned int made = CLI_JSON_NONE;
    const CliJson *const json = menus_made(&made);
    const unsigned int list = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = made, .name = key);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = list, .kind = CFG_JSON_ARRAY), key);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, json->node[list].count, key);
}

void setUp(void)
{
    cli_unity_begun();
    char home[CLI_PATH_MAX_LEN];
    cli_unity_folder("home_home", home);
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home));
}

void tearDown(void)
{
}

void test_the_menus_file_describes_hide(void)
{
    unsigned int made = CLI_JSON_NONE;
    (void)menus_made(&made);
    CLI_TEST_ASSERT_CONTAINS("hide", cli_unity_member_text(made, "about"));
}

void test_the_menus_list_starts_empty(void)
{
    assert_empty_list("menus");
}

void test_the_hide_list_starts_empty(void)
{
    assert_empty_list("hide");
}

void test_the_toolbar_list_starts_empty(void)
{
    assert_empty_list("toolbar");
}

void test_a_state_write_changes_one_entry_and_leaves_the_others(void)
{
    (void)called("state_write", "{\"changes\":{\"orior.one\":\"a\",\"orior.two\":\"b\"}}");
    (void)called("state_write", "{\"changes\":{\"orior.one\":\"c\",\"orior.three\":\"x\"}}");
    unsigned int root = CLI_JSON_NONE;
    const unsigned long long size = called("state_read", "{}");
    const CliJson *const json = cli_unity_parsed(s_out, size, &root);
    TEST_ASSERT_EQUAL_STRING("c", cli_unity_member_text(root, "orior.one"));
    TEST_ASSERT_EQUAL_STRING("b", cli_unity_member_text(root, "orior.two"));
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "orior.three"));
}

void test_a_null_state_value_removes_its_entry_and_leaves_the_others(void)
{
    (void)called("state_write", "{\"changes\":{\"orior.one\":\"a\",\"orior.two\":\"b\"}}");
    (void)called("state_write", "{\"changes\":{\"orior.two\":null}}");
    unsigned int root = CLI_JSON_NONE;
    const unsigned long long size = called("state_read", "{}");
    const CliJson *const json = cli_unity_parsed(s_out, size, &root);
    TEST_ASSERT_EQUAL(CLI_JSON_NONE, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "orior.two"));
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "orior.one"));
}
