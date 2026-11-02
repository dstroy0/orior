// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_settings.c: a settings file includes another and a chain that includes a file twice stops, and the tree takes precedence over the
// reader, a system's part over its file, and values name others
#include "ui/cli/src/machine/settings.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_tree_chain[] = "{\"files\":[{\"file\":\"" CLI_SETTINGS_TREE "\",\"values\":{\"zoom\":1.25,"
                                   "\"signing.windows\":\"mine\",\"crumbs\":\"${system}\",\"path\":\"${env:PATH}\","
                                   "\"lost\":\"${nothing}\",\"[markdown]\":{\"theme.dark\":\"Paper\",\"vim\":true}}}],"
                                   "\"problems\":[]}";

static CliChain s_chain;
static CliChain s_other;
static CliLayers s_layers;
static char s_written[1u << 20u];
static char s_reader[4096];
static char s_root[CLI_PATH_MAX_LEN];
static char s_tree[CLI_PATH_MAX_LEN];
static const CliJson *s_json;
static unsigned int s_read;
static const char *s_system;
static int s_chain_taken;
static int s_layers_taken;

// the chain written and read into the shared tree: s_read its node
static void chain_read(void)
{
    VerbaLinea out = {s_written, sizeof(s_written), 0ull};
    EMBED_CALL(cli_settings_table.chain_write, CliSettingsCfg, .chain_read = &s_chain, .out = &out);
    s_json = cli_unity_parsed(s_written, out.at, &s_read);
}

// whether the strings of an array include one that contains `part`
static int any_contains(unsigned int array, const char *part)
{
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = array, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = array, .index = at), .length_pointer = NULL);
        if ((text != NULL) && (cellul.find(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .other = part, .other_cap = CLI_CELLS_POOL}) != NULL))
        {
            return 1;
        }
    }
    return 0;
}

// a tree's settings including shared.json including base.json, read into the shared tree
static void tree_chain_read(void)
{
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("settings_chain", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "tree/.orior", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "tree", .joined = s_tree);
    cli_unity_file(s_root, "tree/" CLI_SETTINGS_TREE, "{\n  // the shared settings, changed\n  \"takes\": \"../../shared.json\",\n  \"zoom\": 1.25,\n}\n");
    cli_unity_file(s_root, "shared.json", "{ \"takes\": \"base.json\", \"zoom\": 1.1, \"vim\": true }");
    cli_unity_file(s_root, "base.json", "{ \"sticky\": false }");
    EMBED_CALL(cli_settings_path_table.tree, CliSettingsPathCfg, .root = s_tree, .chain = &s_chain);
    s_chain_taken = 1;
    chain_read();
}

// the base file rewritten with `text` and the chain read again
static void base_rewritten(const char *text)
{
    cli_unity_file(s_root, "base.json", text);
    EMBED_CALL(cli_settings_path_table.chain_free, CliSettingsPathCfg, .chain = &s_chain);
    EMBED_CALL(cli_settings_path_table.tree, CliSettingsPathCfg, .root = s_tree, .chain = &s_chain);
    chain_read();
}

// the reader's chain and the tree's chain merged into layers, read into the shared tree: s_read its node
static void layers_read(void)
{
    s_system = cli_settings_system();
    const char *const other = cellul.eq(&(CatenaFinitaCfg){.src = s_system, .other = "windows", .cap = CLI_CELLS_POOL}) ? "linux" : "windows";
    VerbaLinea reader = {s_reader, sizeof(s_reader), 0ull};
    mmgr_line_put(&reader, "{\"files\":[{\"file\":\"base.json\",\"values\":{\"vim\":\"true\",\"margin\":\"true\"}},"
                           "{\"file\":\"settings.json\",\"values\":{\"takes\":\"base.json\",\"zoom\":1.1,"
                           "\"margin\":\"false\",\"");
    mmgr_line_put(&reader, s_system);
    mmgr_line_put(&reader, "\":{\"sticky\":\"false\"},\"");
    mmgr_line_put(&reader, other);
    mmgr_line_put(&reader, "\":{\"zoom\":3},\"fonts\":{\"--code\":\"${setting:code}\"},\"code\":\"Fira Code\","
                           "\"machine\":{\"brackets\":\"false\"}}}],\"problems\":[]}");
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_chain_table.chain_read, CliSettingsChainCfg, .text = s_reader, .length = reader.at, .chain = &s_chain, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_chain_table.chain_read, CliSettingsChainCfg, .text = s_tree_chain, .length = sizeof(s_tree_chain) - 1u, .chain = &s_other, .err_msg = &err_msg));
    EMBED_CALL(cli_settings_root.layers, CliSettingsRootCfg, .reader = &s_chain, .tree = &s_other, .root = "D:/tree", .layers = &s_layers);
    s_layers_taken = 1;
    VerbaLinea out = {s_written, sizeof(s_written), 0ull};
    EMBED_CALL(cli_settings_out.layers_write, CliSettingsOutCfg, .layers_read = &s_layers, .out = &out);
    s_json = cli_unity_parsed(s_written, out.at, &s_read);
}

// a string of the layers' map `map` at `key`, the empty string where it has none
static const char *layer_text(const char *map, const char *key)
{
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = map), .name = key), .length_pointer = NULL);
    return (text != NULL) ? text : "";
}

// the node of `key` in the layers' map `map`
static unsigned int layer_member(const char *map, const char *key)
{
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = map), .name = key);
}

void setUp(void)
{
    cli_unity_begun();
    s_chain_taken = 0;
    s_layers_taken = 0;
}

void tearDown(void)
{
    if (s_layers_taken)
    {
        EMBED_CALL(cli_settings_root.layers_free, CliSettingsRootCfg, .layers = &s_layers);
        EMBED_CALL(cli_settings_path_table.chain_free, CliSettingsPathCfg, .chain = &s_other);
    }
    if (s_chain_taken || s_layers_taken)
    {
        EMBED_CALL(cli_settings_path_table.chain_free, CliSettingsPathCfg, .chain = &s_chain);
    }
}

void test_a_chain_of_files_that_exist_has_no_problem(void)
{
    tree_chain_read();
    const unsigned int problems = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "problems");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = problems, .kind = CFG_JSON_ARRAY));
    TEST_ASSERT_EQUAL_UINT(0u, s_json->node[problems].count);
}

void test_the_files_run_from_the_first_included_to_the_trees_own(void)
{
    tree_chain_read();
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "files");
    TEST_ASSERT_EQUAL_UINT(3u, s_json->node[files].count);
    const char *const first = cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = 0u), "file");
    const char *const second = cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = 1u), "file");
    const unsigned long long first_size = CLI_UNITY_LENGTH(first);
    const unsigned long long second_size = CLI_UNITY_LENGTH(second);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(9ull, first_size);
    TEST_ASSERT_EQUAL_STRING("base.json", first + first_size - 9u);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(11ull, second_size);
    TEST_ASSERT_EQUAL_STRING("shared.json", second + second_size - 11u);
    TEST_ASSERT_EQUAL_STRING(CLI_SETTINGS_TREE, cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = 2u), "file"));
}

void test_a_chain_that_includes_a_file_twice_stops_and_reports_it(void)
{
    tree_chain_read();
    base_rewritten("{ \"takes\": \"shared.json\" }");
    TEST_ASSERT_EQUAL_UINT(3u, s_json->node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "files")].count);
    TEST_ASSERT_TRUE(any_contains(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "problems"), "taken twice"));
}

void test_an_included_file_that_is_missing_is_reported(void)
{
    tree_chain_read();
    base_rewritten("{ \"takes\": \"gone.json\" }");
    TEST_ASSERT_TRUE(any_contains(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "problems"), "gone.json"));
}

void test_a_missing_first_file_is_no_problem(void)
{
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("settings_chain", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "none.json", .joined = path);
    EMBED_CALL(cli_settings_path_table.chain, CliSettingsPathCfg, .path = path, .root = s_root, .chain = &s_chain);
    s_chain_taken = 1;
    chain_read();
    TEST_ASSERT_EQUAL_UINT(0u, s_json->node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "problems")].count);
}

void test_the_trees_value_takes_precedence_over_the_readers(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_STRING("1.25", layer_text("over", "zoom"));
}

void test_each_file_that_sets_a_value_is_named_the_active_one_first(void)
{
    layers_read();
    const unsigned int zoom = layer_member("from", "zoom");
    TEST_ASSERT_EQUAL_STRING(CLI_SETTINGS_TREE, cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = zoom, .index = 0u), "file"));
    TEST_ASSERT_EQUAL_STRING("1.25", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = zoom, .index = 0u), "text"));
    TEST_ASSERT_EQUAL_STRING("settings.json", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = zoom, .index = 1u), "file"));
    TEST_ASSERT_EQUAL_STRING("1.1", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = zoom, .index = 1u), "text"));
}

void test_the_systems_part_takes_precedence_over_the_others(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_STRING("false", layer_text("over", "sticky"));
    TEST_ASSERT_EQUAL_STRING(s_system, cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = layer_member("from", "sticky"), .index = 0u), "part"));
}

void test_the_readers_own_settings_are_the_pages_own(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, layer_member("over", "margin"));
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, layer_member("under", "margin"));
}

void test_a_file_the_reader_includes_is_under_it(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_STRING("true", layer_text("under", "vim"));
}

void test_a_value_names_anothers(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_STRING("{\"--code\":\"Fira Code\"}", layer_text("over", "fonts"));
}

void test_fonts_are_the_readers_own(void)
{
    layers_read();
    const unsigned int owns = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "own");
    int own = 0;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = owns, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const key = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = owns, .index = at), .length_pointer = NULL);
        own |= (key != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "fonts", .cap = CLI_CELLS_POOL});
    }
    TEST_ASSERT_TRUE(own);
}

void test_the_system_variable_names_the_system(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_STRING(s_system, layer_text("over", "crumbs"));
}

void test_an_environment_variable_names_its_value(void)
{
    static char s_path[1u << 15u];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH", .value_out = s_path, .room = sizeof(s_path)))
    {
        TEST_IGNORE_MESSAGE("no PATH is set");
    }
    layers_read();
    TEST_ASSERT_EQUAL_STRING(s_path, layer_text("over", "path"));
}

void test_a_setting_with_a_system_in_its_name_the_file_does_not_take_is_a_problem(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, layer_member("over", "signing.windows"));
    TEST_ASSERT_TRUE(any_contains(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "problems"), "signing.windows"));
}

void test_a_value_that_names_nothing_is_a_problem(void)
{
    layers_read();
    TEST_ASSERT_TRUE(any_contains(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "problems"), "${nothing}"));
}

void test_a_kinds_setting_is_stored_for_it(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_STRING("Paper", cli_unity_member_text(layer_member("kinds", "markdown"), "theme.dark"));
}

void test_a_kind_may_not_set_what_no_kind_sets(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = layer_member("kinds", "markdown"), .name = "vim"));
    TEST_ASSERT_TRUE(any_contains(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "problems"), "[markdown] sets vim"));
}

void test_a_kinds_setting_names_its_file(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_STRING(CLI_SETTINGS_TREE, cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = layer_member("from", "[markdown] theme.dark"), .index = 0u), "file"));
}

void test_the_files_are_the_readers_then_the_trees(void)
{
    layers_read();
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "files");
    TEST_ASSERT_EQUAL_STRING("base.json", EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = 0u), .length_pointer = NULL));
    TEST_ASSERT_EQUAL_STRING("settings.json", EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = 1u), .length_pointer = NULL));
    TEST_ASSERT_EQUAL_STRING(CLI_SETTINGS_TREE, EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = 2u), .length_pointer = NULL));
}

void test_the_readers_own_file_is_named(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_STRING("settings.json", cli_unity_member_text(s_read, "own_file"));
}

void test_this_machines_part_takes_precedence_over_the_rest(void)
{
    layers_read();
    TEST_ASSERT_EQUAL_STRING("false", layer_text("over", "brackets"));
    TEST_ASSERT_EQUAL_STRING("machine", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = layer_member("from", "brackets"), .index = 0u), "part"));
}

void test_the_settings_memory_is_released(void)
{
    layers_read();
    EMBED_CALL(cli_settings_root.layers_free, CliSettingsRootCfg, .layers = &s_layers);
    EMBED_CALL(cli_settings_path_table.chain_free, CliSettingsPathCfg, .chain = &s_chain);
    EMBED_CALL(cli_settings_path_table.chain_free, CliSettingsPathCfg, .chain = &s_other);
    s_layers_taken = 0;
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_settings_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_settings_cells.hw));
}
