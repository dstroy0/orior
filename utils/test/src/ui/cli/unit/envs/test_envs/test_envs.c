// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_envs.c: environment discovery: environments are found at any depth with their requirements, dependency folders are not
// searched, and every allocation is released
#include "ui/cli/src/command_line/cells.h"
#include "ui/cli/src/toolchains/envs.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[CLI_PATH_MAX_LEN];
static char s_found[1u << 16u];

// `text` written to the file `name` under the root, its folder created
static void written(const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = name, .joined = path);
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder))
    {
        (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    }
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text, .size = CLI_UNITY_LENGTH(text)));
}

void setUp(void)
{
    cli_unity_begun();
    cli_unity_folder("envs", s_root);
    written(".venv/pyvenv.cfg", "home = x\n");
    written(".venv/Lib/site-packages/pyvenv.cfg", "not searched\n");
    written("setup.cfg", "[metadata]\nname = a\n\n[options]\ninstall_requires =\n    numpy>=1.20\n    scipy\npython_requires = >=3.10\n");
    written("tools/sub/Pipfile", "[[source]]\nurl = \"x\"\n\n[packages]\nrequests = \"*\"\n\"click\" = \">=8\"\n\n[dev-packages]\npytest = \"*\"\n");
    written("nixed/shell.nix", "{ }\n");
    written("node_modules/x/pyvenv.cfg", "not searched\n");
}

void tearDown(void)
{
}

void test_environments_are_found_with_their_requirements_nearest_the_top_first(void)
{
    VerbaLinea out = {s_found, sizeof(s_found), 0ull};
    EMBED_CALL(cli_envs_table.find, CliEnvsCfg, .root = s_root, .out = &out);
    unsigned int found = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_found, out.at, &found);
    static char s_shown[4096];
    VerbaLinea shown = {s_shown, sizeof(s_shown), 0ull};
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = found, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = found, .index = at);
        mmgr_line_put(&shown, (at > 0u) ? "|" : "");
        mmgr_line_put(&shown, cli_unity_member_text(one, "kind"));
        mmgr_line_ch(&shown, ' ');
        mmgr_line_put(&shown, cli_unity_member_text(one, "place"));
        mmgr_line_ch(&shown, ' ');
        const unsigned int needs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = one, .name = "requires");
        for (unsigned int item = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = needs, .index = item) != CLI_JSON_NONE; item += 1u)
        {
            const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = needs, .index = item), .length_pointer = NULL);
            mmgr_line_put(&shown, (item > 0u) ? "," : "");
            mmgr_line_put(&shown, (name != NULL) ? name : "");
        }
    }
    CLI_TEST_ASSERT_TEXT("venv .venv numpy,scipy|nix nixed |pipenv tools/sub requests,click", s_shown, shown.at);
}

void test_the_search_releases_everything_it_allocated(void)
{
    VerbaLinea out = {s_found, sizeof(s_found), 0ull};
    EMBED_CALL(cli_envs_table.find, CliEnvsCfg, .root = s_root, .out = &out);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_envs_cells.in_use));
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, embed_atomic_usize_load(&g_cli_envs_cells.hw));
}
