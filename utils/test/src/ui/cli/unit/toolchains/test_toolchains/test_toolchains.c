// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_toolchains.c: a login shell's PATH is read; the jobs need git, bash and python; a tree uses only the tools its files call for; what each search
// found is stored for the next, and a changed PATH searches again; the manifest reads and each id is one tool; the reader adds and removes toolchains
// and groups; a star matches any run and versions sort as numbers; a folder on the PATH is not added twice; a tool in a chosen folder is found there
// first; and the PATH a step runs with begins with the folders put ahead of it
#include "ui/cli/src/toolchains/toolchains_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#if defined(_WIN32)
#define TOOLCHAINS_SEPARATOR ";"
#define TOOLCHAINS_GHDL "ghdl.exe"
#else
#define TOOLCHAINS_SEPARATOR ":"
#define TOOLCHAINS_GHDL "ghdl"
#endif

static char s_out[1u << 18u];
static int s_home_set;

static int s_reader_run_result;
static int s_zig_added;
static char s_zig_id[CLI_NAME_MAX_LEN];
static int s_zig_added_again;
static char s_zig_again_error[1024];
static int s_named_git_added;
static int s_empty_group_added;
static char s_groups_stored[1024];
static char s_groups_every[1024];
static unsigned long long s_groups_every_size;
static int s_used_group_removed;
static int s_git_removed;
static char s_git_removed_error[1024];
static int s_zig_removed;
static int s_empty_group_removed;
static char s_groups_after[1024];

// whether a tree of Python files alone contains a file of the name or the ending `kind`
static int python_tree_contains(const CliEachKindCfg *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = ".py", .cap = CLI_CELLS_POOL});
}

// whether the check reported tool `id` of its array `tools` is used
static int tool_used(const CliJson *json, unsigned int tools, const char *id)
{
    for (unsigned int tool = json->node[tools].first; tool != CLI_JSON_NONE; tool = json->node[tool].next)
    {
        const char *const named = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = tool, .name = "id"), .length_pointer = NULL);
        if ((named != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = named, .other = id, .cap = CLI_CELLS_POOL}))
        {
            return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = tool, .name = "used"), .kind = CFG_JSON_TRUE);
        }
    }
    return 0;
}

// the reader's toolchain added: 1, its id into `id`, or 0 with why into `why`, which is sized for `capacity`
static int toolchain_added(const char *tool, char *id, char *why, unsigned long long capacity)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(tool, CLI_UNITY_LENGTH(tool), &root);
    VerbaLinea err_msg = {why, capacity, 0ull};
    const int added = EMBED_CALL(cli_toolchains_err_msg.add, CliToolchainsErrMsgCfg, .supplied = json, .tool = root, .id_out = id, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    return added;
}

// the groups of the reader's file, joined by `|`, into `out`, which is sized for `capacity`
static void reader_groups_read(char *out, unsigned long long capacity)
{
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {out, capacity, 0ull};
    (void)EMBED_CALL(cli_toolchains_internal.file, CliToolchainsCtx, .name = "user_toolchains.json", .path = path);
    const unsigned int read = EMBED_CALL(cli_toolchains_json_internal.file_read, CliToolchainsJsonCtx, .json = &json, .path_read = path, .nodes_more = 0u, .text_more = 0ull);
    const unsigned int groups = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = read, .name = "groups");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = groups, .kind = CFG_JSON_ARRAY) ? json.node[groups].first : CLI_JSON_NONE; item != CLI_JSON_NONE; item = json.node[item].next)
    {
        const char *const group = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json, .node = item, .length_pointer = NULL);
        mmgr_line_put(&line, (line.at > 0ull) ? "|" : "");
        mmgr_line_put(&line, (group != NULL) ? group : "");
    }
    (void)mmgr_line_finish(&line);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}

// a group written after the others, joined by `|`
static void group_recorded(const CliEachGroupCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_put(out, args->group);
}

// a reader's toolchains and groups added and removed, what each answered recorded, once for the process
static void reader_run_result(void)
{
    static const char s_zig[] = "{\"id\":\"\",\"name\":\"Zig 0.13\",\"group\":\"Systems\",\"for\":\"\",\"programs\":[\"zig\"]}";
    static const char s_git[] = "{\"id\":\"\",\"name\":\"Git\",\"group\":\"Systems\",\"for\":\"\",\"programs\":[\"zig\"]}";
    char id[CLI_NAME_MAX_LEN];
    char why[1024];
    if (s_reader_run_result)
    {
        return;
    }
    s_reader_run_result = 1;
    s_zig_added = toolchain_added(s_zig, s_zig_id, why, sizeof(why));
    s_zig_added_again = toolchain_added(s_zig, id, s_zig_again_error, sizeof(s_zig_again_error));
    s_named_git_added = toolchain_added(s_git, id, why, sizeof(why));
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    s_empty_group_added = EMBED_CALL(cli_toolchains_err_msg_name.add_group, CliToolchainsErrMsgNameCfg, .name = "Proof assistants", .err_msg = &err_msg);
    reader_groups_read(s_groups_stored, sizeof(s_groups_stored));
    VerbaLinea every = {s_groups_every, sizeof(s_groups_every), 0ull};
    EMBED_CALL(cli_toolchains_each.groups, CliToolchainsEachCfg, .each = group_recorded, .context = &every);
    s_groups_every_size = mmgr_line_finish(&every);
    err_msg.at = 0ull;
    s_used_group_removed = EMBED_CALL(cli_toolchains_err_msg_name.remove_group, CliToolchainsErrMsgNameCfg, .name = "Systems", .err_msg = &err_msg);
    VerbaLinea git_error = {s_git_removed_error, sizeof(s_git_removed_error), 0ull};
    s_git_removed = EMBED_CALL(cli_toolchains_id_err_msg.remove, CliToolchainsIdErrMsgCfg, .id = "git", .err_msg = &git_error);
    (void)mmgr_line_finish(&git_error);
    err_msg.at = 0ull;
    s_zig_removed = EMBED_CALL(cli_toolchains_id_err_msg.remove, CliToolchainsIdErrMsgCfg, .id = "zig-0-13", .err_msg = &err_msg);
    err_msg.at = 0ull;
    s_empty_group_removed = EMBED_CALL(cli_toolchains_err_msg_name.remove_group, CliToolchainsErrMsgNameCfg, .name = "Systems", .err_msg = &err_msg);
    reader_groups_read(s_groups_after, sizeof(s_groups_after));
}

// a chosen folder counted
static void chosen_counted(const CliEachIdFolderCfg *args)
{
    void *context = args->context;
    *(unsigned int *)context += 1u;
}

void setUp(void)
{
    char home[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    if (!s_home_set)
    {
        s_home_set = 1;
        cli_unity_folder("toolchains_home", home);
        (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    }
}

void tearDown(void)
{
}

void test_a_read_the_cells_refuse_reports_no_chosen_folder(void)
{
    unsigned int reported = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = 256ull));
    EMBED_CALL(cli_toolchains_context.chosen_each, CliToolchainsContextCfg, .each_callback = chosen_counted, .context = &reported);
    const int remade = EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    TEST_ASSERT_EQUAL_UINT(0u, reported);
    TEST_ASSERT_TRUE(remade);
}

void test_posix_a_login_shells_path_is_read(void)
{
#if defined(_WIN32)
    TEST_IGNORE_MESSAGE("a login shell is POSIX's");
#else
    static const char s_shell[] = "#!/bin/sh\nprintf '%s' /opt/from/profile:/usr/bin\n";
    char dir[CLI_PATH_MAX_LEN];
    char shell[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("toolchains_shell", dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "shell", .joined = shell);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = shell, .bytes = s_shell, .size = sizeof(s_shell) - 1u));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_file_internal.file_runnable, CliPathFileCtx, .path = shell));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_toolchains_shell_internal.path_of_shell, CliToolchainsShellCtx, .shell = shell, .path = path, .size = sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/opt/from/profile:/usr/bin", path);
#endif
}

void test_posix_a_shell_that_prints_nothing_has_no_path(void)
{
#if defined(_WIN32)
    TEST_IGNORE_MESSAGE("a login shell is POSIX's");
#else
    static const char s_quiet[] = "#!/bin/sh\nexit 0\n";
    char dir[CLI_PATH_MAX_LEN];
    char quiet[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("toolchains_shell", dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "quiet", .joined = quiet);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = quiet, .bytes = s_quiet, .size = sizeof(s_quiet) - 1u));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_file_internal.file_runnable, CliPathFileCtx, .path = quiet));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_toolchains_shell_internal.path_of_shell, CliToolchainsShellCtx, .shell = quiet, .path = path, .size = sizeof(path)));
#endif
}

void test_the_jobs_need_git_bash_and_python(void)
{
    CliJson json;
    char needed[256];
    VerbaLinea line = {needed, sizeof(needed), 0ull};
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg, .json_out = &json, .nodes_more = 0u, .text_more = 0ull);
    for (unsigned int tool = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = manifest, .kind = CFG_JSON_ARRAY) ? json.node[manifest].first : CLI_JSON_NONE; tool != CLI_JSON_NONE; tool = json.node[tool].next)
    {
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = tool, .name = "needed"), .kind = CFG_JSON_TRUE))
        {
            const char *const id = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = tool, .name = "id"), .length_pointer = NULL);
            mmgr_line_put(&line, (line.at > 0ull) ? "|" : "");
            mmgr_line_put(&line, (id != NULL) ? id : "");
        }
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    CLI_TEST_ASSERT_TEXT("git|bash|python", needed, line.at);
}

void test_a_tree_of_python_files_uses_pythons_tools_and_the_tools_the_jobs_need(void)
{
    unsigned int tools = CLI_JSON_NONE;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_toolchains_versions.check, CliToolchainsVersionsCfg, .versions = 0, .seen = python_tree_contains, .context_read = NULL, .out = &out);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &tools);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = tools, .kind = CFG_JSON_ARRAY));
    TEST_ASSERT_TRUE(tool_used(json, tools, "black"));
    TEST_ASSERT_TRUE(tool_used(json, tools, "ruff"));
    TEST_ASSERT_TRUE(tool_used(json, tools, "git"));
}

void test_a_tree_uses_no_tool_whose_files_it_does_not_contain(void)
{
    unsigned int tools = CLI_JSON_NONE;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_toolchains_versions.check, CliToolchainsVersionsCfg, .versions = 0, .seen = python_tree_contains, .context_read = NULL, .out = &out);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &tools);
    TEST_ASSERT_FALSE(tool_used(json, tools, "lean"));
    TEST_ASSERT_FALSE(tool_used(json, tools, "rust"));
    TEST_ASSERT_FALSE(tool_used(json, tools, "docker"));
}

void test_with_no_tree_open_only_the_tools_the_jobs_need_are_used(void)
{
    unsigned int tools = CLI_JSON_NONE;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_toolchains_versions.check, CliToolchainsVersionsCfg, .versions = 0, .seen = NULL, .context_read = NULL, .out = &out);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &tools);
    TEST_ASSERT_FALSE(tool_used(json, tools, "black"));
    TEST_ASSERT_TRUE(tool_used(json, tools, "python"));
}

void test_a_check_stores_what_each_search_found_in_oriors_folder(void)
{
    char found_file[CLI_PATH_MAX_LEN];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_toolchains_versions.check, CliToolchainsVersionsCfg, .versions = 0, .seen = NULL, .context_read = NULL, .out = &out);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_toolchains_internal.file, CliToolchainsCtx, .name = "toolchains_found.json", .path = found_file));
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = found_file));
}

void test_the_next_search_reads_what_was_stored(void)
{
    unsigned int count = 0u;
    CliFound found;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_toolchains_versions.check, CliToolchainsVersionsCfg, .versions = 0, .seen = NULL, .context_read = NULL, .out = &out);
    char (*const path)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&count, &g_checks_cells);
    const int cached = EMBED_CALL(cli_toolchains_id_internal.found_cache, CliToolchainsIdCtx, .id = "git", .path_callback = (const char (*)[CLI_PATH_MAX_LEN])path, .count = count, .found = &found);
    g_cli_cells->persistent_buf_release(path, &g_checks_cells);
    TEST_ASSERT_TRUE(cached);
}

void test_a_changed_path_searches_again(void)
{
    static const char s_other[1][CLI_PATH_MAX_LEN] = {"/a/folder/put/on/the/path"};
    CliFound found;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_toolchains_versions.check, CliToolchainsVersionsCfg, .versions = 0, .seen = NULL, .context_read = NULL, .out = &out);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_toolchains_id_internal.found_cache, CliToolchainsIdCtx, .id = "git", .path_callback = s_other, .count = 1u, .found = &found));
}

void test_the_manifest_reads_and_each_id_is_one_tool_with_a_program_and_an_https_page(void)
{
    static char s_ids[512][CLI_NAME_MAX_LEN];
    CliJson json;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    // the program's own bytes are toolchains.json's text
    const unsigned int read = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = (const char *)g_cli_toolchains_json, .size = g_cli_toolchains_json_size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    const unsigned int tools = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = read, .name = "tools");
    unsigned int count = 0u;
    int once = 1;
    int named = 1;
    int paged = 1;
    for (unsigned int tool = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = tools, .kind = CFG_JSON_ARRAY) ? json.node[tools].first : CLI_JSON_NONE; (tool != CLI_JSON_NONE) && (count < 512u); tool = json.node[tool].next)
    {
        const char *const id = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = tool, .name = "id"), .length_pointer = NULL);
        const char *const id_text = (id != NULL) ? id : "";
        once = once && (id != NULL);
        for (unsigned int at = 0u; once && (at < count); at += 1u)
        {
            once = !cellul.eq(&(CatenaFinitaCfg){.src = s_ids[at], .other = id_text, .cap = CLI_CELLS_POOL});
        }
        memor.cpy(&(MemoriaCfg){.dst = s_ids[count], .src = id_text, .bytes = CLI_UNITY_LENGTH(id_text) + 1u});
        count += 1u;
        const unsigned int programs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = tool, .name = "programs");
        named = named && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = programs, .kind = CFG_JSON_ARRAY) && (json.node[programs].count > 0u);
        const unsigned int install = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = tool, .name = "install");
        paged = paged && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = install, .kind = CFG_JSON_OBJECT) && (json.node[install].count > 0u);
        for (unsigned int page = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = install, .kind = CFG_JSON_OBJECT) ? json.node[install].first : CLI_JSON_NONE; page != CLI_JSON_NONE; page = json.node[page].next)
        {
            const char *const address = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json, .node = page, .length_pointer = NULL);
            paged = paged && (address != NULL) && cellul.starts(&(CatenaFinitaCfg){.src = address, .other = "https://", .cap = CLI_CELLS_POOL});
        }
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    TEST_ASSERT_GREATER_THAN_UINT(0u, count);
    TEST_ASSERT_TRUE(once);
    TEST_ASSERT_TRUE(named);
    TEST_ASSERT_TRUE(paged);
}

void test_a_toolchain_is_added_its_id_created_from_its_name(void)
{
    reader_run_result();
    TEST_ASSERT_TRUE(s_zig_added);
    TEST_ASSERT_EQUAL_STRING("zig-0-13", s_zig_id);
}

void test_a_toolchain_is_not_added_twice(void)
{
    reader_run_result();
    TEST_ASSERT_FALSE(s_zig_added_again);
    CLI_TEST_ASSERT_CONTAINS("already", s_zig_again_error);
}

void test_an_id_orior_cli_has_is_refused(void)
{
    reader_run_result();
    TEST_ASSERT_FALSE(s_named_git_added);
}

void test_an_empty_group_is_added_and_the_readers_groups_are_stored(void)
{
    reader_run_result();
    TEST_ASSERT_TRUE(s_empty_group_added);
    TEST_ASSERT_EQUAL_STRING("Systems|Proof assistants", s_groups_stored);
}

void test_the_readers_groups_come_after_oriors_own(void)
{
    static const char s_last[] = "|Systems|Proof assistants";
    reader_run_result();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(sizeof(s_last) - 1u, s_groups_every_size);
    TEST_ASSERT_EQUAL_STRING(s_last, s_groups_every + s_groups_every_size - (sizeof(s_last) - 1u));
}

void test_a_group_containing_a_tool_stays(void)
{
    reader_run_result();
    TEST_ASSERT_FALSE(s_used_group_removed);
}

void test_a_toolchain_orior_cli_has_is_not_removed(void)
{
    reader_run_result();
    TEST_ASSERT_FALSE(s_git_removed);
    CLI_TEST_ASSERT_CONTAINS("comes with orior", s_git_removed_error);
}

void test_a_readers_toolchain_and_its_empty_group_are_removed_and_the_other_group_stays(void)
{
    reader_run_result();
    TEST_ASSERT_TRUE(s_zig_removed);
    TEST_ASSERT_TRUE(s_empty_group_removed);
    TEST_ASSERT_EQUAL_STRING("Proof assistants", s_groups_after);
}

void test_a_star_matches_the_run_after_it(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_toolchains_pattern_internal.matches, CliToolchainsPatternCtx, .pattern = "Python3*", .name = "Python314"));
}

void test_a_star_matches_a_run_with_dots(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_toolchains_pattern_internal.matches, CliToolchainsPatternCtx, .pattern = "R-*", .name = "R-4.6.1"));
}

void test_the_end_after_a_star_must_match(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_toolchains_pattern_internal.matches, CliToolchainsPatternCtx, .pattern = "v*x", .name = "v13.3"));
}

void test_versions_sort_as_numbers(void)
{
    TEST_ASSERT_GREATER_THAN_INT(0, EMBED_CALL(cli_toolchains_a_internal.natural, CliToolchainsACtx, .a = "Python314", .b = "Python312"));
    TEST_ASSERT_GREATER_THAN_INT(0, EMBED_CALL(cli_toolchains_a_internal.natural, CliToolchainsACtx, .a = "Python312", .b = "Python39"));
}

void test_a_folder_is_added_at_the_paths_end(void)
{
    char out[256];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_toolchains_folder.join, CliToolchainsFolderCfg, .path_read = "a" TOOLCHAINS_SEPARATOR "b" TOOLCHAINS_SEPARATOR, .folder_read = "c", .out_capacity = out, .size = sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("a" TOOLCHAINS_SEPARATOR "b" TOOLCHAINS_SEPARATOR "c", out);
}

void test_a_folder_already_on_the_path_is_not_added_twice(void)
{
    char out[256];
    TEST_ASSERT_FALSE(EMBED_CALL(cli_toolchains_folder.join, CliToolchainsFolderCfg, .path_read = "a" TOOLCHAINS_SEPARATOR "c", .folder_read = "c", .out_capacity = out, .size = sizeof(out)));
}

void test_an_empty_path_contains_the_folder_added(void)
{
    char out[256];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_toolchains_folder.join, CliToolchainsFolderCfg, .path_read = "", .folder_read = "c", .out_capacity = out, .size = sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("c", out);
}

void test_windows_a_path_folder_matches_regardless_of_case_and_trailing_separator(void)
{
#if defined(_WIN32)
    char out[256];
    TEST_ASSERT_FALSE(EMBED_CALL(cli_toolchains_folder.join, CliToolchainsFolderCfg, .path_read = "C:\\Tools\\", .folder_read = "c:\\tools", .out_capacity = out, .size = sizeof(out)));
#else
    TEST_IGNORE_MESSAGE("a path's case is ignored on Windows");
#endif
}

void test_a_tool_in_a_chosen_folder_is_found_there_first(void)
{
    char dir[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    char why[1024];
    char required[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_folder("toolchains_chosen", dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = TOOLCHAINS_GHDL, .joined = file);
    (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file, .bytes = "", .size = 0ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_toolchains_id.choose, CliToolchainsIdCfg, .id = "ghdl", .folder_read = dir, .program = program, .err_msg = &err_msg));
    CliJson json;
    err_msg.at = 0ull;
    const unsigned int tool = EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &json, .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg, .json_out = &json, .nodes_more = 0u, .text_more = 0ull), .id = "ghdl", .err_msg = &err_msg);
    CliFound found;
    EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &json, .tool = tool, .path_callback = NULL, .count = 0u, .found = &found);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, tool);
    TEST_ASSERT_EQUAL_STRING("chosen", found.state);
    EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = file, .key = required);
    EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = found.program, .key = key);
    TEST_ASSERT_TRUE(found.has_program);
    TEST_ASSERT_EQUAL_STRING(required, key);
    err_msg.at = 0ull;
    (void)EMBED_CALL(cli_toolchains_id_err_msg.forget, CliToolchainsIdErrMsgCfg, .id = "ghdl", .err_msg = &err_msg);
}

void test_the_path_a_step_runs_with_begins_with_the_folders_append_ahead_each_once(void)
{
    char ahead[2][CLI_PATH_MAX_LEN];
    cli_unity_folder("toolchains_ahead", ahead[0]);
    memor.cpy(&(MemoriaCfg){.dst = ahead[1], .src = ahead[0], .bytes = sizeof(ahead[0])});
    char *const path = g_cli_cells->persistent_buf_alloc(CLI_PATH_TEXT, &g_checks_cells);
    TEST_ASSERT_NOT_NULL(path);
    EMBED_CALL(cli_toolchains_count_path.run_path_with, CliToolchainsCountPathCfg, .ahead = (const char (*)[CLI_PATH_MAX_LEN])ahead, .count = 2u, .path = path, .size = CLI_PATH_TEXT);
    const unsigned long long length = CLI_UNITY_LENGTH(ahead[0]);
    const int first = cellul.starts(&(CatenaFinitaCfg){.src = path, .other = ahead[0], .cap = CLI_CELLS_POOL}) && (path[length] == TOOLCHAINS_SEPARATOR[0]);
    const int once = cellul.find(&(CatenaFinitaCfg){.src = path + length, .cap = CLI_CELLS_POOL, .other = ahead[0], .other_cap = CLI_CELLS_POOL}) == NULL;
    const int goes_on = path[length + 1u] != '\0';
    g_cli_cells->persistent_buf_release(path, &g_checks_cells);
    TEST_ASSERT_TRUE(first);
    TEST_ASSERT_TRUE(once);
    TEST_ASSERT_TRUE(goes_on);
}
