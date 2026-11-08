// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_query_tools.c: the query's builds: compiles pace as the interface's queue does, every pattern of every tool reaches a file of the tree,
// paths given the compilers use forward slashes, no two sources of a tool share an object, a tool is stale where its binary is missing or
// older than a source, a missing compiler is reported before any build, and the writer builds and runs
#include "ui/cli/src/text/text.h"
#include "ui/query/src/tools/tools_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define WRITER_TOOL "orior_qry_writer"

static char s_root[QUERY_PATH_MAX_LEN];
static QueryPaths s_found;
static char s_header[1u << 16u];
static char s_error[1024];

// the number the interface's header defines under `name`, read from its `#define` line; the test failed where it has none
static unsigned long long interface_number(const char *name)
{
    char path[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src/cu/transpiler/lstar/interface/interface.h", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_header, sizeof(s_header) - 1u);
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    s_header[size] = '\0';
    const unsigned long long length = CLI_UNITY_LENGTH(name);
    for (const char *line = s_header; line[0] != '\0';)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = line, .other = "#define ", .cap = CLI_POOL_BYTES}) && EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = line + 8, .other_pointer = name, .size = length) && ((line[8u + length] == ' ') || (line[8u + length] == '\t')))
        {
            const char *digit = line + 8u + length;
            while ((digit[0] == ' ') || (digit[0] == '\t'))
            {
                digit += 1;
            }
            unsigned long long value = 0ull;
            while ((digit[0] >= '0') && (digit[0] <= '9'))
            {
                value = (value * 10ull) + (unsigned long long)(digit[0] - '0');
                digit += 1;
            }
            return value;
        }
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES, .byte = '\n'});
        line = (end != NULL) ? (end + 1) : (line + CLI_UNITY_LENGTH(line));
    }
    TEST_FAIL_MESSAGE(name);
    return 0ull;
}

// one pattern names a file under the tree
static void assert_reaches(const char *pattern)
{
    const char *const patterns[2] = {pattern, NULL};
    (void)EMBED_CALL(query_paths_internal.paths_alloc, QueryPathsCtx, .paths = &s_found, .count = QUERY_SOURCES_MAX);
    const unsigned int found = EMBED_CALL(query_tools_root_internal.sources, QueryToolsRootCtx, .root = s_root, .patterns = patterns, .found = &s_found);
    g_cli_guard->persistent_buf_release(s_found.path, g_query_owner);
    *(&s_found) = (QueryPaths){NULL, 0u, 0u};
    TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(0u, found, pattern);
}

// no program is found, each reported as the window reports it
static int no_program(const QueryEachRolePathCapacityErrMsgCfg *args)
{
    QueryRole role = args->role;
    args->path[0] = '\0';
    mmgr_line_put(args->err_msg, EMBED_CALL(query_table.role_name, QueryCfg, .role = role));
    mmgr_line_put(args->err_msg, " is not found: File, Toolchains opens its install page");
    return 0;
}

// no PATH beyond the process's own
static const char *no_path(void *context)
{
    (void)context;
    return "";
}

// the writer's binary path into `binary`, under a folder made empty, its tool returned
static const QueryTool *writer_binary_named(char *binary)
{
    char out[QUERY_PATH_MAX_LEN];
    char name[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/query_test/stale", .joined = out);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = out);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = out));
    EMBED_CALL(query_name_capacity_internal.binary_name, QueryNameCapacityCtx, .name = WRITER_TOOL, .binary = name, .capacity = sizeof(name));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = out, .relative = name, .joined = binary);
    const QueryTool *const tool = EMBED_CALL(query_tools_internal.tool_by_name, QueryToolsCtx, .name = WRITER_TOOL);
    TEST_ASSERT_NOT_NULL(tool);
    return tool;
}

// the writer built into `out` and its path into `binary`; the test ignored where there is no C compiler
static void writer_built(char *binary, unsigned long long capacity)
{
    const QueryPrograms programs = query_on_path();
    char compiler[QUERY_PATH_MAX_LEN];
    char out[QUERY_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    if (!EMBED_CALL(programs.program, QueryEachRolePathCapacityErrMsgCfg, .context = programs.context, .role = QUERY_ROLE_C, .path = compiler, .capacity = sizeof(compiler), .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no C compiler to build the writer with");
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/query_test/tools", .joined = out);
    err_msg.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_capacity_internal.tool_built_into, QueryNameCapacityCtx, .root = s_root, .programs = &programs, .name = WRITER_TOOL, .out_text = out, .binary = binary, .capacity = capacity, .err_msg = &err_msg));
}

void setUp(void)
{
    char found[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    if (!EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = found))
    {
        TEST_IGNORE_MESSAGE("no tree is found");
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root, .src = found, .bytes = CLI_UNITY_LENGTH(found) + 1ull});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
}

void tearDown(void)
{
}

void test_the_compiles_run_as_many_at_once_as_the_interfaces_queue(void)
{
    TEST_ASSERT_EQUAL_UINT64(interface_number("INTERFACE_LAUNCHES_AT_ONCE"), QUERY_LAUNCHES_AT_ONCE);
}

void test_the_compiles_run_as_far_apart_as_the_interfaces_queue(void)
{
    TEST_ASSERT_EQUAL_UINT64(interface_number("INTERFACE_LAUNCH_SPACING_MILLISECONDS"), QUERY_LAUNCH_SPACING_MILLISECONDS);
}

void test_every_pattern_of_every_tool_reaches_a_file_of_the_tree(void)
{
    for (const QueryTool *tool = g_query_tools; tool->name != NULL; tool += 1)
    {
        for (unsigned int group = 0u; group < QUERY_TOOL_GROUPS; group += 1u)
        {
            const char *const *const lists[2] = {tool->c[group], tool->cxx[group]};
            for (unsigned int list = 0u; list < 2u; list += 1u)
            {
                for (unsigned int at = 0u; (lists[list] != NULL) && (lists[list][at] != NULL); at += 1u)
                {
                    assert_reaches(lists[list][at]);
                }
            }
        }
        for (unsigned int at = 0u; (tool->cxx_tool != NULL) && (tool->cxx_tool[at] != NULL); at += 1u)
        {
            assert_reaches(tool->cxx_tool[at]);
        }
    }
}

void test_no_two_sources_of_a_tool_share_an_object(void)
{
    static char s_named[3u * QUERY_SOURCES_MAX][QUERY_PATH_MAX_LEN];
    for (const QueryTool *tool = g_query_tools; tool->name != NULL; tool += 1)
    {
        unsigned int c = 0u;
        unsigned int cxx = 0u;
        unsigned int tools = 0u;
        EMBED_CALL(query_tools_root_tool_c_internal.every_source, QueryToolsRootToolCCtx, .root = s_root, .tool = tool, .c = &c, .cxx = &cxx, .tools = &tools);
        unsigned int count = 0u;
        char (*const lists[3])[QUERY_PATH_MAX_LEN] = {g_query_c_sources.path, g_query_cxx_sources.path, g_query_tool_sources.path};
        const unsigned int counts[3] = {c, cxx, tools};
        for (unsigned int list = 0u; list < 3u; list += 1u)
        {
            for (unsigned int at = 0u; at < counts[list]; at += 1u)
            {
                EMBED_CALL(query_tools_root_objects_internal.object_get, QueryToolsRootObjectsCtx, .root = s_root, .objects = "objects", .source_text = lists[list][at], .object_capacity = s_named[count]);
                count += 1u;
            }
        }
        int apart = 1;
        for (unsigned int one = 0u; apart && (one < count); one += 1u)
        {
            for (unsigned int other = one + 1u; apart && (other < count); other += 1u)
            {
                apart = !cellul.eq(&(CatenaFinitaCfg){.src = s_named[one], .other = s_named[other], .cap = CLI_POOL_BYTES});
            }
        }
        g_cli_guard->persistent_buf_release(g_query_c_sources.path, g_query_owner);
        *(&g_query_c_sources) = (QueryPaths){NULL, 0u, 0u};
        g_cli_guard->persistent_buf_release(g_query_cxx_sources.path, g_query_owner);
        *(&g_query_cxx_sources) = (QueryPaths){NULL, 0u, 0u};
        g_cli_guard->persistent_buf_release(g_query_tool_sources.path, g_query_owner);
        *(&g_query_tool_sources) = (QueryPaths){NULL, 0u, 0u};
        g_cli_guard->persistent_buf_release(g_query_objects.path, g_query_owner);
        *(&g_query_objects) = (QueryPaths){NULL, 0u, 0u};
        TEST_ASSERT_TRUE_MESSAGE(apart, tool->name);
    }
}

void test_the_file_definitions_are_what_every_program_links(void)
{
    (void)EMBED_CALL(query_paths_internal.paths_alloc, QueryPathsCtx, .paths = &s_found, .count = QUERY_SOURCES_MAX);
    const unsigned int count = EMBED_CALL(query_tools_root_internal.sources, QueryToolsRootCtx, .root = s_root, .patterns = g_query_file_defs, .found = &s_found);
    int kfile_lines = 0;
    int kdm = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const found = s_found.path[at];
        const unsigned long long length = CLI_UNITY_LENGTH(found);
        kfile_lines = kfile_lines || ((length >= 14ull) && cellul.eq(&(CatenaFinitaCfg){.src = found + length - 14u, .other = "/kfile_lines.c", .cap = CLI_POOL_BYTES}));
        kdm = kdm || ((length >= 6ull) && cellul.eq(&(CatenaFinitaCfg){.src = found + length - 6u, .other = "/kdm.c", .cap = CLI_POOL_BYTES}));
    }
    const unsigned int capacity = s_found.capacity;
    g_cli_guard->persistent_buf_release(s_found.path, g_query_owner);
    *(&s_found) = (QueryPaths){NULL, 0u, 0u};
    TEST_ASSERT_TRUE(kfile_lines);
    TEST_ASSERT_TRUE(kdm);
    TEST_ASSERT_GREATER_THAN_UINT(10u, count);
    TEST_ASSERT_EQUAL_UINT(QUERY_SOURCES_MAX, capacity);
}

void test_paths_input_the_compilers_use_forward_slashes(void)
{
    char source[QUERY_PATH_MAX_LEN];
    char objects[QUERY_PATH_MAX_LEN];
    char object[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src/cu/includes/file_defs/kfile_lines/kfile_lines.c", .joined = source);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/objects", .joined = objects);
    EMBED_CALL(query_tools_root_objects_internal.object_get, QueryToolsRootObjectsCtx, .root = s_root, .objects = objects, .source_text = source, .object_capacity = object);
    TEST_ASSERT_NULL(cellul.chr(&(CatenaFinitaCfg){.src = source, .cap = CLI_POOL_BYTES, .byte = '\\'}));
    TEST_ASSERT_NULL(cellul.chr(&(CatenaFinitaCfg){.src = object, .cap = CLI_POOL_BYTES, .byte = '\\'}));
}

void test_a_missing_binary_is_stale(void)
{
    char binary[QUERY_PATH_MAX_LEN];
    const QueryTool *const tool = writer_binary_named(binary);
    TEST_ASSERT_TRUE(EMBED_CALL(query_tools_root_tool_internal.stale, QueryToolsRootToolCtx, .root = s_root, .tool = tool, .binary = binary));
}

void test_a_binary_newer_than_every_source_is_not_stale(void)
{
    char binary[QUERY_PATH_MAX_LEN];
    const QueryTool *const tool = writer_binary_named(binary);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = binary, .bytes = "", .size = 0ull));
    TEST_ASSERT_FALSE(EMBED_CALL(query_tools_root_tool_internal.stale, QueryToolsRootToolCtx, .root = s_root, .tool = tool, .binary = binary));
}

void test_a_binary_older_than_its_sources_is_stale(void)
{
    char binary[QUERY_PATH_MAX_LEN];
    const QueryTool *const tool = writer_binary_named(binary);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = binary, .bytes = "", .size = 0ull));
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_type_internal.modified_set, QueryPathSecondsTypeCtx, .path = binary, .seconds_value = 86400ull));
    TEST_ASSERT_TRUE(EMBED_CALL(query_tools_root_tool_internal.stale, QueryToolsRootToolCtx, .root = s_root, .tool = tool, .binary = binary));
}

void test_an_object_newer_than_its_source_and_every_header_is_fresh(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_tools_source_known_internal.fresh, QueryToolsSourceKnownCtx, .source_known = 1, .source = 10ull, .object_known = 1, .object = 20ull, .newest_header = 5ull));
}

void test_a_header_newer_than_the_object_compiles_it_again(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(query_tools_source_known_internal.fresh, QueryToolsSourceKnownCtx, .source_known = 1, .source = 10ull, .object_known = 1, .object = 20ull, .newest_header = 30ull));
}

void test_a_source_newer_than_the_object_compiles_it_again(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(query_tools_source_known_internal.fresh, QueryToolsSourceKnownCtx, .source_known = 1, .source = 30ull, .object_known = 1, .object = 20ull, .newest_header = 0ull));
}

void test_a_missing_object_is_compiled(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(query_tools_source_known_internal.fresh, QueryToolsSourceKnownCtx, .source_known = 1, .source = 10ull, .object_known = 0, .object = 0ull, .newest_header = 0ull));
}

void test_an_object_with_no_newer_header_is_fresh(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_tools_source_known_internal.fresh, QueryToolsSourceKnownCtx, .source_known = 1, .source = 10ull, .object_known = 1, .object = 20ull, .newest_header = 0ull));
}

void test_a_missing_compiler_is_reported_and_nothing_is_built(void)
{
    const QueryPrograms none = {no_program, no_path, NULL};
    char out[QUERY_PATH_MAX_LEN];
    char binary[QUERY_PATH_MAX_LEN];
    char name[QUERY_PATH_MAX_LEN];
    char path[QUERY_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/query_test/noc", .joined = out);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = out);
    TEST_ASSERT_FALSE(EMBED_CALL(query_name_capacity_internal.tool_built_into, QueryNameCapacityCtx, .root = s_root, .programs = &none, .name = "orior_sass_listing_seeds", .out_text = out, .binary = binary, .capacity = sizeof(binary), .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_line_finish(&err_msg));
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_error, .other = "the C compiler is not found", .cap = CLI_POOL_BYTES}));
    EMBED_CALL(query_name_capacity_internal.binary_name, QueryNameCapacityCtx, .name = "orior_sass_listing_seeds", .binary = name, .capacity = sizeof(name));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = out, .relative = name, .joined = path);
    TEST_ASSERT_FALSE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
}

void test_the_writer_with_no_words_exits_2(void)
{
    char binary[QUERY_PATH_MAX_LEN];
    writer_built(binary, sizeof(binary));
    const char *const words[2] = {binary, NULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    const int run_result_well = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &err_msg);
    const long long code = run_result->code;
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    TEST_ASSERT_TRUE(run_result_well);
    TEST_ASSERT_EQUAL_INT64(2ll, code);
}

void test_a_built_tool_is_not_stale(void)
{
    char binary[QUERY_PATH_MAX_LEN];
    writer_built(binary, sizeof(binary));
    const QueryTool *const tool = EMBED_CALL(query_tools_internal.tool_by_name, QueryToolsCtx, .name = WRITER_TOOL);
    TEST_ASSERT_NOT_NULL(tool);
    TEST_ASSERT_FALSE(EMBED_CALL(query_tools_root_tool_internal.stale, QueryToolsRootToolCtx, .root = s_root, .tool = tool, .binary = binary));
}
