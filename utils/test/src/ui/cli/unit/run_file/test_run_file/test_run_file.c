// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_run_file.c: running a file: a path is quoted for the shell, every run line of the toolchain manifest closes its
// placeholders, and a language with no run line is refused
#include "ui/cli/src/runs/run_file.h"
#include "ui/cli/src/toolchains/toolchains.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_path_is_quoted_for_the_shell(void)
{
    char quoted[256];
    VerbaLinea line = {quoted, sizeof(quoted), 0ull};
    EMBED_CALL(cli_run_file.quote, CliRunFileCfg, .out = &line, .path = "a b/it's.py");
    CLI_TEST_ASSERT_TEXT("'a b/it'\\''s.py'", quoted, line.at);
}

void test_every_run_line_of_the_manifest_closes_its_placeholders(void)
{
    static CliJson s_json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg, .json_out = &s_json, .nodes_more = 0u, .text_more = 0ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = manifest, .kind = CFG_JSON_ARRAY));
    for (unsigned int tool = s_json.node[manifest].first; tool != CLI_JSON_NONE; tool = s_json.node[tool].next)
    {
        const unsigned int runs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = tool, .name = "runs");
        const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = runs, .kind = CFG_JSON_OBJECT) ? s_json.node[runs].first : CLI_JSON_NONE;
        for (unsigned int run = first; run != CLI_JSON_NONE; run = s_json.node[run].next)
        {
            const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json, .node = run, .length_pointer = NULL);
            int open = 0;
            for (const char *at = (text != NULL) ? text : ""; *at != '\0'; at += 1)
            {
                open += (*at == '{') ? 1 : ((*at == '}') ? -1 : 0);
            }
            TEST_ASSERT_EQUAL_INT_MESSAGE(0, open, text);
        }
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
}

void test_a_language_with_no_run_line_is_refused(void)
{
    char why[1024];
    char ran[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {ran, sizeof(ran), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_run_file.line, CliRunFileCfg, .root = ".", .path = "a.json", .language = "json", .out = &out, .err_msg = &err_msg));
    (void)mmgr_line_finish(&err_msg);
    CLI_TEST_ASSERT_CONTAINS("no way to run", why);
}
