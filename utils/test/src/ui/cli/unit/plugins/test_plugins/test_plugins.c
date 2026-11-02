// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_plugins.c: every bundled plugin reads and names itself, a file opens by its name, its path or its extension, a name becomes an id, a plugin from
// nothing contains what was requested, a plugin from another has its grammar, creating writes once and replaces only when requested, a grammar is made
// whole from the tree's tables, and a plugin serves each folder containing its table
#include "ui/cli/src/editor/highlight.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/plugins/plugins.h"
#include "ui/cli/src/window/window.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliJsonNode s_node[1u << 16u];
static char s_text[1u << 20u];
static CfgJsonToken s_token[1u << 16u];
static CliJson s_json;
static char s_out[1u << 18u];
static CliLanguages s_languages;
static int s_languages_live;
static char s_why[256];
static char s_dir[CLI_PATH_MAX_LEN];
static char s_folder[CLI_PATH_MAX_LEN];
static int s_folders_ran;
static char s_folders_alpha[CLI_NAME_MAX_LEN];
static char s_folders_beta[CLI_NAME_MAX_LEN];
static char s_folders_gamma[CLI_NAME_MAX_LEN];
static char s_folders_delta[CLI_NAME_MAX_LEN];

// `size` bytes of JSON read into the tree, begun again: its root
static unsigned int tree_read(const char *text, unsigned long long size)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &s_json, .node_json_node_pointer = s_node, .nodes_most = sizeof(s_node) / sizeof(s_node[0]), .text_out = s_text, .text_most = sizeof(s_text));
    return EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &s_json, .text = text, .size = size, .tokens = s_token, .tokens_most = sizeof(s_token) / sizeof(s_token[0]), .err_msg = &err_msg);
}

// a member of a node of the tree written compact, asserted against `wanted`
static void assert_node(unsigned int node, const char *wanted)
{
    char text[512];
    VerbaLinea line = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &s_json, .node = node, .out = &line, .indent = 0u, .sorted = 0);
    CLI_TEST_ASSERT_TEXT(wanted, text, line.at);
}

// the languages read into s_languages, released at the case's end
static void languages_read(void)
{
    EMBED_CALL(cli_plugins_table.languages, CliPluginsCfg, .languages = &s_languages);
    s_languages_live = 1;
}

// the language `path` opens as, empty where none
static const char *language_of(const char *path)
{
    static char s_id[CLI_NAME_MAX_LEN];
    return EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg, .languages_read = &s_languages, .path = path, .id_out = s_id) ? s_id : "";
}

// the id a name becomes
static const char *id_of(const char *name)
{
    static char s_id[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_plugins_id_name.slug, CliPluginsIdNameCfg, .name = name, .id_out = s_id);
    return s_id;
}

// the toy language's answers read into the tree: their node
static unsigned int toy_answers(void)
{
    static const char s_spec[] = "{\"name\": \"Toy Lang\", \"extensions\": [\".toy\"], \"lineComment\": \"--\", "
                                 "\"blockComment\": [\"{-\", \"-}\"], \"keywords\": [\"let\", \"in\", \"let\"], "
                                 "\"types\": [\"Int\"], \"constants\": [\"true\"]}";
    return tree_read(s_spec, sizeof(s_spec) - 1u);
}

// a plugin drafted from the toy answers: its node
static unsigned int toy_drafted(void)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    const unsigned int plugin = EMBED_CALL(cli_plugins_err_msg_json_spec_kind.draft, CliPluginsErrMsgJsonSpecKindCfg, .json_out = &s_json, .spec = toy_answers(), .err_msg = &err_msg);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, plugin);
    return plugin;
}

// a plugin drafted from rust: its node
static unsigned int rusty_drafted(void)
{
    static const char s_rusty[] = "{\"name\": \"Rusty\", \"extensions\": [\"rsy\"], \"from\": \"rust\", \"keywords\": [\"fn\"]}";
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    const unsigned int plugin = EMBED_CALL(cli_plugins_err_msg_json_spec_kind.draft, CliPluginsErrMsgJsonSpecKindCfg, .json_out = &s_json, .spec = tree_read(s_rusty, sizeof(s_rusty) - 1u), .err_msg = &err_msg);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, plugin);
    return plugin;
}

// a folder of the folder counted
static void folder_counted(const CliEachNameIsFolderCfg *args)
{
    unsigned int *const count = args->context;
    *count += args->is_folder ? 1u : 0u;
}

// a toy plugin created in a fresh folder, its folder in s_folder
static void toy_created(void)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    cli_unity_folder("plugins", s_dir);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_err_msg.create_in, CliPluginsErrMsgCfg, .dir = s_dir, .json = &s_json, .spec = toy_answers(), .replace = 0, .folder = s_folder, .err_msg = &err_msg));
}

// the reader's plugin recorded where the plugins list it as the reader's
static void reader_plugin_found(const CliEachPluginCfg *args)
{
    int *const found = args->context;
    *found |= cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = "readers", .cap = CLI_CELLS_POOL}) && cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->source, .other = "user", .cap = CLI_CELLS_POOL});
}

// `text` written to the file `name` of the folder `folder`, the folders it is in made
static void file_written(const char *folder, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = name, .joined = path);
    for (char *at = path + CLI_UNITY_LENGTH(folder) + 1u; at[0] != '\0'; at += 1)
    {
        if (at[0] == '/')
        {
            at[0] = '\0';
            EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
            at[0] = '/';
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text, .size = CLI_UNITY_LENGTH(text)), name);
}

// a reader's plugin written in a fresh ORIOR_HOME
static void reader_plugin_written(void)
{
    static const char s_manifest[] = "{\"id\": \"readers\", \"name\": \"Readers\", \"version\": \"1.0.0\", \"kind\": "
                                     "\"script\", \"api\": 1}";
    char home[CLI_PATH_MAX_LEN];
    cli_unity_folder("plugins_reader_home", home);
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    file_written(home, "plugins/readers/plugin.json", s_manifest);
}

// a script plugin whose `main` names `main_list`, with its code, written in a fresh ORIOR_HOME
static void parts_plugin_written(const char *main_list)
{
    char home[CLI_PATH_MAX_LEN];
    char manifest[512];
    VerbaLinea line = {manifest, sizeof(manifest), 0ull};
    cli_unity_folder("plugins_code_home", home);
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    mmgr_line_put(&line, "{\"id\": \"parts\", \"name\": \"Parts\", \"version\": \"1.0.0\", \"kind\": \"script\", \"main\": ");
    mmgr_line_put(&line, main_list);
    mmgr_line_ch(&line, '}');
    (void)mmgr_line_finish(&line);
    file_written(home, "plugins/parts/plugin.json", manifest);
    file_written(home, "plugins/parts/one.js", "const one = 1;");
    file_written(home, "plugins/parts/lib/two.js", "const two = one + 1;");
}

// a plugin that serves the folders of a tree, and what it says of each, read once for the process
static void folders_read(void)
{
    char home[CLI_PATH_MAX_LEN];
    char tree[CLI_PATH_MAX_LEN];
    char empty[CLI_PATH_MAX_LEN];
    if (s_folders_ran)
    {
        return;
    }
    s_folders_ran = 1;
    cli_unity_folder("plugins_folders_home", home);
    cli_unity_folder("plugins_folders_tree", tree);
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    file_written(home, "plugins/folders/plugin.json", "{\"id\": \"folders\", \"name\": \"Folders\", \"version\": \"1.0.0\", \"kind\": \"language\", "
                                                      "\"each\": {\"folder\": \"defs\", \"table\": \"{name}/{name}.table.tsv\", \"unless\": {\"column\": "
                                                      "\"where\", \"match\": \"^head \\\\d\"}, \"except\": [\"gamma\"]}, \"grammar\": {\"keywords\": "
                                                      "{\"table\": \"defs/{name}/{name}.table.tsv\", \"columns\": [\"form\"], \"match\": \"^([a-z]+)\"}, "
                                                      "\"tokenizer\": {\"root\": [[\"[a-z]+\", {\"cases\": {\"@keywords\": \"keyword\", \"@default\": "
                                                      "\"identifier\"}}], [\"\\\\s+\", \"\"]]}}}");
    file_written(tree, "defs/alpha/alpha.table.tsv", "where\tform\nline 1\tgo <x>\n");
    file_written(tree, "defs/beta/beta.table.tsv", "where\tform\nhead 0\tmagic\n");
    file_written(tree, "defs/gamma/gamma.table.tsv", "where\tform\nline 1\tgo <x>\n");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = tree, .relative = "defs/delta", .joined = empty);
    EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = empty);
    EMBED_CALL(cli_plugins_root.set_tree, CliPluginsRootCfg, .root = tree);
    EMBED_CALL(cli_plugins_table.languages, CliPluginsCfg, .languages = &s_languages);
    (void)EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg, .languages_read = &s_languages, .path = "x/one.alpha", .id_out = s_folders_alpha);
    (void)EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg, .languages_read = &s_languages, .path = "x/one.beta", .id_out = s_folders_beta);
    (void)EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg, .languages_read = &s_languages, .path = "x/one.gamma", .id_out = s_folders_gamma);
    (void)EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg, .languages_read = &s_languages, .path = "x/one.delta", .id_out = s_folders_delta);
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &s_languages);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    if (s_languages_live)
    {
        s_languages_live = 0;
        EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &s_languages);
    }
}

void test_every_bundled_plugin_reads_and_names_itself_and_a_script_carries_its_code(void)
{
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(16u, g_cli_bundled_count);
    for (unsigned int at = 0u; at < g_cli_bundled_count; at += 1u)
    {
        const unsigned int root = tree_read((const char *)g_cli_bundled[at].text, g_cli_bundled[at].size);
        const char *const id = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = root, .name = "id"), .length_pointer = NULL);
        const char *const kind = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = root, .name = "kind"), .length_pointer = NULL);
        TEST_ASSERT_NOT_NULL_MESSAGE(id, g_cli_bundled[at].id);
        TEST_ASSERT_EQUAL_STRING(g_cli_bundled[at].id, id);
        TEST_ASSERT_NOT_NULL_MESSAGE(kind, g_cli_bundled[at].id);
        if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "language", .cap = CLI_CELLS_POOL}))
        {
            static const char *const s_keys[3] = {"extensions", "names", "paths"};
            // a language opens files by their extensions, names or paths, or serves the folders of its `each`
            int any = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = root, .name = "each"), .kind = CFG_JSON_OBJECT);
            for (unsigned int key = 0u; key < 3u; key += 1u)
            {
                const unsigned int list = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = root, .name = s_keys[key]);
                any |= EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = list, .kind = CFG_JSON_ARRAY) && (s_json.node[list].count > 0u);
            }
            TEST_ASSERT_TRUE_MESSAGE(any, id);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "tool", .cap = CLI_CELLS_POOL}))
        {
            const unsigned int list = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = root, .name = "languages");
            TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = list, .kind = CFG_JSON_ARRAY) && (s_json.node[list].count > 0u), id);
        }
        else
        {
            TEST_ASSERT_EQUAL_STRING_MESSAGE("script", kind, id);
            // a script plugin that comes with orior runs the code built into the program beside its plugin.json
            TEST_ASSERT_NOT_NULL_MESSAGE(g_cli_bundled[at].code, id);
            TEST_ASSERT_GREATER_THAN_UINT64(0ull, g_cli_bundled[at].code_size);
        }
    }
}

void test_a_plugin_written_in_oriors_folder_is_listed_as_the_readers(void)
{
    int found = 0;
    reader_plugin_written();
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = reader_plugin_found, .context = &found);
    TEST_ASSERT_TRUE(found);
}

void test_the_windows_list_of_plugins_includes_the_readers(void)
{
    const char *text = NULL;
    unsigned long long size = 0ull;
    reader_plugin_written();
    const unsigned int room = cli_window_called(1, "plugins_read", "{}", 2ull);
    const int answered = cli_window_answer(room, &text, &size);
    const int listed = answered && (cellul.find(&(CatenaFinitaCfg){.src = text, .cap = size, .other = "\"id\":\"readers\"", .other_cap = CLI_CELLS_POOL}) != NULL);
    cli_window_given(room);
    TEST_ASSERT_TRUE(listed);
}

void test_a_plugins_code_is_each_file_its_main_names_in_order(void)
{
    char code[256];
    char why[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea out = {code, sizeof(code), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    parts_plugin_written("[\"one.js\", \"lib/two.js\"]");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id.code_get, CliPluginsIdCfg, .id = "parts", .out = &out, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    TEST_ASSERT_EQUAL_STRING("const one = 1;\nconst two = one + 1;", code);
}

void test_a_file_main_names_outside_its_folder_is_refused(void)
{
    char code[256];
    char why[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea out = {code, sizeof(code), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    parts_plugin_written("[\"one.js\", \"../two.js\"]");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_plugins_id.code_get, CliPluginsIdCfg, .id = "parts", .out = &out, .err_msg = &err_msg));
}

void test_a_file_opens_by_its_whole_name(void)
{
    languages_read();
    TEST_ASSERT_EQUAL_STRING("python", language_of("src/SConscript"));
    TEST_ASSERT_EQUAL_STRING("jenkins", language_of("ci/Jenkinsfile"));
}

void test_a_file_opens_by_its_path(void)
{
    languages_read();
    TEST_ASSERT_EQUAL_STRING("ansible", language_of("deploy/roles/web/tasks/main.yml"));
}

void test_a_path_opens_whichever_way_its_slashes_lean(void)
{
    languages_read();
    TEST_ASSERT_EQUAL_STRING("ansible", language_of("deploy\\playbooks\\site.yaml"));
}

void test_a_file_opens_by_its_extension(void)
{
    languages_read();
    TEST_ASSERT_EQUAL_STRING("yaml", language_of("config/app.yml"));
    TEST_ASSERT_EQUAL_STRING("zig", language_of("build.zig"));
}

void test_the_languages_keys_are_in_a_room_at_their_most(void)
{
    languages_read();
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_languages.count);
    TEST_ASSERT_EQUAL_UINT(CLI_LANGUAGE_PAIRS, s_languages.room);
}

void test_the_plugins_are_read_in_cells_each_released(void)
{
    EMBED_CALL(cli_plugins_table.languages, CliPluginsCfg, .languages = &s_languages);
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &s_languages);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_plugins_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_plugins_cells.hw));
}

void test_a_name_becomes_an_id(void)
{
    TEST_ASSERT_EQUAL_STRING("toy-lang", id_of("Toy Lang"));
}

void test_each_run_of_other_characters_is_one_dash(void)
{
    TEST_ASSERT_EQUAL_STRING("c-more", id_of("  C++ & More!! "));
}

void test_a_name_of_no_letters_has_no_id(void)
{
    TEST_ASSERT_EQUAL_STRING("", id_of("!!!"));
}

void test_a_drafted_plugins_id_is_its_names(void)
{
    assert_node(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = toy_drafted(), .name = "id"), "\"toy-lang\"");
}

void test_a_drafted_plugins_extensions_have_no_dots(void)
{
    assert_node(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = toy_drafted(), .name = "extensions"), "[\"toy\"]");
}

void test_a_drafted_plugin_has_its_comments(void)
{
    assert_node(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = toy_drafted(), .name = "comments"), "{\"line\":\"--\",\"block\":[\"{-\",\"-}\"]}");
}

void test_a_drafted_plugin_has_each_keyword_once(void)
{
    const unsigned int grammar = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = toy_drafted(), .name = "grammar");
    assert_node(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = grammar, .name = "keywords"), "[\"let\",\"in\"]");
}

void test_a_block_comment_opens_the_comment_state(void)
{
    const unsigned int grammar = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = toy_drafted(), .name = "grammar");
    const unsigned int whitespace = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = grammar, .name = "tokenizer"), .name = "whitespace");
    assert_node(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = whitespace, .index = 1u), "[\"\\\\{-\",\"comment\",\"@comment\"]");
}

void test_a_line_comment_runs_to_its_end(void)
{
    const unsigned int grammar = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = toy_drafted(), .name = "grammar");
    const unsigned int whitespace = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = grammar, .name = "tokenizer"), .name = "whitespace");
    assert_node(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = whitespace, .index = 2u), "[\"--.*$\",\"comment\"]");
}

void test_a_drafted_plugins_text_reads_back_as_itself(void)
{
    char written[8192];
    char again[8192];
    unsigned int root = CLI_JSON_NONE;
    const unsigned int plugin = toy_drafted();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_plugins_json_out.text_get, CliPluginsJsonOutCfg, .json = &s_json, .plugin = plugin, .out = &out);
    VerbaLinea line = {written, sizeof(written), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &s_json, .node = plugin, .out = &line, .indent = 0u, .sorted = 1);
    const CliJson *const read = cli_unity_parsed(s_out, out.at, &root);
    VerbaLinea again_line = {again, sizeof(again), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = read, .node = root, .out = &again_line, .indent = 0u, .sorted = 1);
    TEST_ASSERT_EQUAL_UINT64(line.at, again_line.at);
    TEST_ASSERT_EQUAL_MEMORY(written, again, line.at);
}

void test_a_plugin_from_another_has_its_own_id_extensions_and_words(void)
{
    const unsigned int plugin = rusty_drafted();
    const unsigned int grammar = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = plugin, .name = "grammar");
    assert_node(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = plugin, .name = "id"), "\"rusty\"");
    assert_node(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = plugin, .name = "extensions"), "[\"rsy\"]");
    assert_node(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = grammar, .name = "words"), "[\"fn\"]");
}

void test_a_plugin_from_another_has_the_others_grammar(void)
{
    const unsigned int grammar = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = rusty_drafted(), .name = "grammar");
    const unsigned int types = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = grammar, .name = "types");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = types, .kind = CFG_JSON_ARRAY));
    TEST_ASSERT_GREATER_THAN_UINT(10u, s_json.node[types].count);
}

void test_a_plugin_from_one_not_there_is_refused(void)
{
    static const char s_none[] = "{\"name\": \"x\", \"extensions\": [\"x\"], \"from\": \"nothing-here\"}";
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, EMBED_CALL(cli_plugins_err_msg_json_spec_kind.draft, CliPluginsErrMsgJsonSpecKindCfg, .json_out = &s_json, .spec = tree_read(s_none, sizeof(s_none) - 1u), .err_msg = &err_msg));
}

void test_a_plugin_created_has_its_manifest(void)
{
    char path[CLI_PATH_MAX_LEN];
    toy_created();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "plugin.json", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
}

void test_a_plugin_created_has_a_sample_in_its_language(void)
{
    // sized for the sample a plugin of one language is written with, a few hundred bytes, and its 0
    static char s_sample[1u << 16u];
    char path[CLI_PATH_MAX_LEN];
    toy_created();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "sample.toy", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_sample, sizeof(s_sample) - 1u);
    s_sample[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_sample, .other = "-- Toy Lang", .cap = CLI_CELLS_POOL}));
}

void test_a_plugin_is_not_overwritten_unless_requested_and_stays_one_plugin(void)
{
    unsigned int count = 0u;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    toy_created();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_plugins_err_msg.create_in, CliPluginsErrMsgCfg, .dir = s_dir, .json = &s_json, .spec = toy_answers(), .replace = 0, .folder = s_folder, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_err_msg.create_in, CliPluginsErrMsgCfg, .dir = s_dir, .json = &s_json, .spec = toy_answers(), .replace = 1, .folder = s_folder, .err_msg = &err_msg));
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = s_dir, .each = folder_counted, .context_bytess = &count);
    TEST_ASSERT_EQUAL_UINT(1u, count);
}

void test_a_plugin_starts_from_another_larger_than_the_answers_own_tree(void)
{
    // answers read into a tree of their own size, starting from a plugin many times the room they use
    static const char s_from[] = "{\"name\": \"Snake\", \"extensions\": [\"snk\"], \"from\": \"python\"}";
    CliJson tight = {0};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    cli_unity_folder("plugins_from", s_dir);
    const unsigned int answers = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &tight, .text = s_from, .size = sizeof(s_from) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    err_msg.at = 0ull;
    const int created = EMBED_CALL(cli_plugins_err_msg.create_in, CliPluginsErrMsgCfg, .dir = s_dir, .json = &tight, .spec = answers, .replace = 0, .folder = s_folder, .err_msg = &err_msg);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &tight);
    TEST_ASSERT_TRUE(created);
}

void test_a_grammar_is_made_whole_from_the_trees_tables(void)
{
    static const char s_grammar[] = "{\"types\": {\"table\": \"lng/words.tsv\", \"columns\": [\"word\"], \"rows\": {\"column\": \"kind\", "
                                    "\"match\": "
                                    "\"^type$\"}, \"match\": \"^[a-z]+\"}, \"others\": {\"table\": \"lng/words.tsv\", \"columns\": [\"word\"], "
                                    "\"rows\": {\"column\": \"kind\", \"skip\": \"^type$\"}}, \"arrows\": {\"table\": \"lng/{name}.tsv\", "
                                    "\"columns\": [\"arrow\"], \"skip\": \"^(?:[a-z]+|-)?$\"}, \"none\": {\"table\": \"lng/missing.tsv\", "
                                    "\"columns\": [\"word\"]}, \"tokenizer\": {\"root\": [[{\"words\": \"arrows\", \"before\": \"\\\\s\", "
                                    "\"after\": \"$\"}, \"operator\"], [{\"words\": \"none\"}, \"keyword\"], [\"[a-z]+\", \"identifier\"]]}}";
    char tree[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("plugins_tables_tree", tree);
    file_written(tree, "lng/words.tsv", "# the words of the check\nword\tkind\tarrow\nalpha\ttype\t->\nbeta\tplain\t-\ngamma-x\ttype\tx>\n"
                                        "\tplain\t\n");
    const unsigned int grammar = tree_read(s_grammar, sizeof(s_grammar) - 1u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_err_msg_json.grammar_whole, CliPluginsErrMsgJsonCfg, .json = &s_json, .grammar = grammar, .tree = tree, .name = "words", .out = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT("{\"types\":[\"alpha\",\"gamma\"],\"others\":[\"beta\"],\"arrows\":[\"->\",\"x>\"],\"none\":[],"
                         "\"tokenizer\":{\"root\":[[\"\\\\s(?:->|x>)$\",\"operator\"],[\"[a-z]+\",\"identifier\"]]}}",
                         s_out, out.at);
}

void test_a_plugin_serves_a_folder_containing_its_table_named_by_the_folder(void)
{
    folders_read();
    TEST_ASSERT_EQUAL_STRING("folders:alpha", s_folders_alpha);
}

void test_a_plugin_serves_no_folder_unless_matches_except_names_or_with_no_table(void)
{
    folders_read();
    TEST_ASSERT_EQUAL_STRING("", s_folders_beta);
    TEST_ASSERT_EQUAL_STRING("", s_folders_gamma);
    TEST_ASSERT_EQUAL_STRING("", s_folders_delta);
}

void test_a_folders_grammar_is_stored_under_the_plugin_and_folder(void)
{
    char key[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    folders_read();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "folders:alpha", .key = key, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING("plugin:folders:alpha", key);
}

void test_a_word_of_the_folders_table_is_colored_as_a_keyword(void)
{
    char key[CLI_NAME_MAX_LEN];
    const char *const lines[1] = {"go x"};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    folders_read();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "folders:alpha", .key = key, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_highlight_key_err_msg.color, CliHighlightKeyErrMsgCfg, .key = key, .state = "root", .lines = lines, .count = 1u, .out = &out, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    CLI_TEST_ASSERT_CONTAINS("\"keyword\"", s_out);
}
