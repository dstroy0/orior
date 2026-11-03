// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_compile_db.c: the compilation database: a CMake reply gives each source the build's compiler, includes and defines, a tree's own
// database is read for its compilers, and a command line's quoted words stay whole
#include "ui/cli/src/builds/compile_db_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_text[1u << 16u];
static CliJsonNode s_node[1u << 12u];
static char s_tree_text[1u << 16u];
static CfgJsonToken s_token[1u << 12u];
static char s_written[1u << 16u];
static CliWords s_words;
static CliDatabase s_cmake;
static char s_cmake_dir[CLI_PATH_MAX_LEN];
static char s_source[CLI_PATH_MAX_LEN];
static char s_built[CLI_PATH_MAX_LEN];
static unsigned long long s_in_use_after_first;
static unsigned long long s_in_use_after_second;
static int s_cmake_run_result;
static CliInputs s_inputs;
static CliDatabase s_own;
static char s_own_dir[CLI_PATH_MAX_LEN];
static int s_files_in_temporary_tier;
static int s_own_run_result;

// a path with each `\` a `/` into `out`, CLI_PATH_MAX_LEN bytes
static void forward_slashed(const char *path, char *out)
{
    memor.cpy(&(MemoriaCfg){.dst = out, .src = path, .bytes = CLI_UNITY_LENGTH(path) + 1ull});
    for (char *at = out; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
}

// whether the words clangd starts with contain `word`
static int words_contain(const char *word)
{
    for (unsigned int at = 0u; at < s_words.count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word_text[at], .other = word, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// the entry of `name` in the folder `source` as the database should write it, built in `built` by the `count` words `flags`
static void entry_written(VerbaLinea *line, const char *built, const char *source, const char *name, const char *const *flags, unsigned int count)
{
    char file[CLI_PATH_MAX_LEN];
    VerbaLinea named = {file, sizeof(file), 0ull};
    mmgr_line_put(&named, source);
    mmgr_line_put(&named, "/src/");
    mmgr_line_put(&named, name);
    (void)mmgr_line_finish(&named);
    mmgr_line_put(line, "{\"directory\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = line, .text = built, .length = CLI_UNITY_LENGTH(built));
    mmgr_line_put(line, ",\"file\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = line, .text = file, .length = named.at);
    mmgr_line_put(line, ",\"arguments\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = line, .text = flags[at], .length = CLI_UNITY_LENGTH(flags[at]));
        mmgr_line_ch(line, ',');
    }
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = line, .text = file, .length = named.at);
    mmgr_line_put(line, "]}");
}

// a CMake reply of two configurations refreshed twice, two seconds apart, once for the process
static void cmake_refreshed(void)
{
    static const char s_index[] = "{\"reply\": {\"codemodel-v2\": {\"jsonFile\": \"codemodel-v2-a.json\"}, "
                                  "\"toolchains-v1\": {\"jsonFile\": \"toolchains-v1-a.json\"}}}";
    static const char s_toolchains[] = "{\"toolchains\": [{\"language\": \"C\", \"compiler\": {\"path\": \"/usr/bin/gcc\", \"id\": \"GNU\"}}, "
                                       "{\"language\": \"CXX\", \"compiler\": {\"path\": \"C:/VS/bin/cl.exe\", \"id\": \"MSVC\"}}]}";
    static const char s_target[] = "{\"sources\": [{\"path\": \"src/main.c\"}, {\"path\": \"src/more.cpp\"}, {\"path\": \"src/kernel.cu\"}], "
                                   "\"compileGroups\": [{\"language\": \"C\", \"compileCommandFragments\": [{\"fragment\": \"-g -std=gnu11\"}], "
                                   "\"includes\": [{\"path\": \"/x/include\"}, {\"path\": \"/x/sys\", \"isSystem\": true}], \"defines\": "
                                   "[{\"define\": \"NEEDED=1\"}], \"sourceIndexes\": [0]}, {\"language\": \"CXX\", \"compileCommandFragments\": "
                                   "[{\"fragment\": \"/W3\"}], \"includes\": [{\"path\": \"C:/x/include\"}], \"defines\": [{\"define\": "
                                   "\"WIN\"}], \"sourceIndexes\": [1]}, {\"language\": \"CUDA\", \"compileCommandFragments\": [], "
                                   "\"sourceIndexes\": [2]}]}";
    if (s_cmake_run_result)
    {
        return;
    }
    s_cmake_run_result = 1;
    char build[CLI_PATH_MAX_LEN];
    char reply[CLI_PATH_MAX_LEN];
    cli_unity_folder("compile_db_cmake", s_cmake_dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_cmake_dir, .relative = "build/cmake/Debug", .joined = build);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = build, .relative = ".cmake/api/v1/reply", .joined = reply);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = reply));
    forward_slashed(s_cmake_dir, s_source);
    forward_slashed(build, s_built);
    cli_unity_file(reply, "index-2026-01-01T00-00-00-0000.json", s_index);
    VerbaLinea line = {s_text, sizeof(s_text), 0ull};
    mmgr_line_put(&line, "{\"paths\": {\"source\": ");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = s_source, .length = CLI_UNITY_LENGTH(s_source));
    mmgr_line_put(&line, ", \"build\": ");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = s_built, .length = CLI_UNITY_LENGTH(s_built));
    mmgr_line_put(&line, "}, \"configurations\": [{\"name\": \"Release\", \"targets\": []}, {\"name\": \"Debug\", \"targets\": [{\"name\": \"probe\", \"jsonFile\": \"target-probe.json\"}]}]}");
    (void)mmgr_line_finish(&line);
    cli_unity_file(reply, "codemodel-v2-a.json", s_text);
    cli_unity_file(reply, "toolchains-v1-a.json", s_toolchains);
    cli_unity_file(reply, "target-probe.json", s_target);
    (void)EMBED_CALL(cli_compile_db_root_database.refresh, CliCompileDbRootDatabaseCfg, .root = s_cmake_dir, .database = &s_cmake);
    s_in_use_after_first = embed_atomic_usize_load(&g_cli_compile_cells.in_use);
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 2100u);
    (void)EMBED_CALL(cli_compile_db_root_database.refresh, CliCompileDbRootDatabaseCfg, .root = s_cmake_dir, .database = &s_cmake);
    s_in_use_after_second = embed_atomic_usize_load(&g_cli_compile_cells.in_use);
}

// a tree whose .clangd names its own database read and gathered, once for the process
static void own_gathered(void)
{
    if (s_own_run_result)
    {
        return;
    }
    s_own_run_result = 1;
    char out[CLI_PATH_MAX_LEN];
    cli_unity_folder("compile_db_own", s_own_dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_own_dir, .relative = "out", .joined = out);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = out));
    cli_unity_file(s_own_dir, ".clangd", "CompileFlags:\n  CompilationDatabase: out  # the tree's own\n");
    cli_unity_file(out, "compile_commands.json",
                   "[{\"directory\": \"/t\", \"file\": \"/t/a.c\", \"command\": "
                   "\"\\\"/opt/cross/bin/arm-none-eabi-gcc\\\" -c a.c\"}]");
    const size_t top_before = g_cli_cells->temporary_buf_mark();
    EMBED_CALL(cli_compile_db_inputs_internal.compile_inputs, CliCompileDbInputsCtx, .root = s_own_dir, .inputs = &s_inputs);
    s_files_in_temporary_tier = g_cli_cells->temporary_buf_mark() < top_before;
    EMBED_CALL(cli_compile_db_root_internal.compile_gather, CliCompileDbRootCtx, .root = s_own_dir, .inputs_read = &s_inputs, .database = &s_own);
}

// a word of a command line appended, `|` between them
static void word_recorded(const CliEachWordSizeCfg *args)
{
    VerbaLinea *const line = args->context;
    mmgr_line_put(line, (line->at > 0ull) ? "|" : "");
    mmgr_line_put_n(line, args->word_text, args->size);
}

void setUp(void)
{
    cli_unity_begun();
    s_words.count = 0u;
    s_words.used = 0u;
}

void tearDown(void)
{
}

void test_a_look_stores_only_the_files_it_read_and_a_second_look_stores_no_more(void)
{
    cmake_refreshed();
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_compile_cells.hw));
    TEST_ASSERT_NOT_EQUAL(0ull, s_in_use_after_first);
    TEST_ASSERT_EQUAL_UINT64(s_in_use_after_first, s_in_use_after_second);
}

void test_a_cmake_reply_releases_each_source_the_debug_configurations_command(void)
{
    static const char *const s_c[7] = {"/usr/bin/gcc", "-g", "-std=gnu11", "-I/x/include", "-isystem/x/sys", "-DNEEDED=1", "-c"};
    static const char *const s_cxx[5] = {"C:/VS/bin/cl.exe", "/W3", "/IC:/x/include", "/DWIN", "/c"};
    cmake_refreshed();
    VerbaLinea line = {s_text, sizeof(s_text), 0ull};
    mmgr_line_ch(&line, '[');
    entry_written(&line, s_built, s_source, "main.c", s_c, 7u);
    mmgr_line_ch(&line, ',');
    entry_written(&line, s_built, s_source, "more.cpp", s_cxx, 5u);
    mmgr_line_ch(&line, ']');
    CliJson required;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &required, .node_json_node_pointer = s_node, .nodes_max = sizeof(s_node) / sizeof(s_node[0]), .text_out = s_tree_text, .text_max = sizeof(s_tree_text));
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int requested_root = EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &required, .text = s_text, .size = line.at, .tokens = s_token, .tokens_max = sizeof(s_token) / sizeof(s_token[0]), .err_msg = &err_msg);
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_cmake_dir, .relative = CLI_COMPILE_WRITTEN, .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_written, sizeof(s_written));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    unsigned int written = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_written, size, &written);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = json, .one = written, .other_json = &required, .other = requested_root));
}

void test_clangd_is_input_the_written_folder(void)
{
    char folder[CLI_PATH_MAX_LEN];
    cmake_refreshed();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_cmake_dir, .relative = "build/orior", .joined = folder);
    TEST_ASSERT_TRUE(s_cmake.has_folder);
    TEST_ASSERT_EQUAL_STRING(folder, s_cmake.folder);
}

void test_a_database_that_names_cl_is_msvcs(void)
{
    cmake_refreshed();
    TEST_ASSERT_TRUE(s_cmake.msvc);
}

void test_cl_is_not_queried_for_its_headers(void)
{
    cmake_refreshed();
    EMBED_CALL(cli_compile_db_database.words, CliCompileDbDatabaseCfg, .database_read = &s_cmake, .words = &s_words);
    TEST_ASSERT_TRUE(words_contain("--query-driver=/usr/bin/gcc"));
    TEST_ASSERT_EQUAL_UINT(2u, s_words.count);
}

void test_the_files_a_look_reads_are_in_the_temporary_tier(void)
{
    own_gathered();
    TEST_ASSERT_TRUE(s_files_in_temporary_tier);
}

void test_the_database_a_trees_clangd_names_is_its_own(void)
{
    char own[CLI_PATH_MAX_LEN];
    own_gathered();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_own_dir, .relative = "out/compile_commands.json", .joined = own);
    TEST_ASSERT_TRUE(s_inputs.own);
    TEST_ASSERT_EQUAL_UINT(1u, s_inputs.count);
    TEST_ASSERT_EQUAL_STRING(own, s_inputs.file[0].path);
}

void test_a_trees_own_database_is_read_for_its_compiler(void)
{
    own_gathered();
    TEST_ASSERT_FALSE(s_own.has_folder);
    TEST_ASSERT_EQUAL_UINT(1u, s_own.compilers);
    TEST_ASSERT_EQUAL_STRING("/opt/cross/bin/arm-none-eabi-gcc", s_own.compiler[0]);
    TEST_ASSERT_FALSE(s_own.msvc);
}

void test_clangd_may_query_a_trees_own_compiler(void)
{
    own_gathered();
    EMBED_CALL(cli_compile_db_database.words, CliCompileDbDatabaseCfg, .database_read = &s_own, .words = &s_words);
    TEST_ASSERT_EQUAL_UINT(1u, s_words.count);
    TEST_ASSERT_TRUE(words_contain("--query-driver=/opt/cross/bin/arm-none-eabi-gcc"));
}

void test_nothing_is_written_over_a_trees_own_database(void)
{
    char path[CLI_PATH_MAX_LEN];
    own_gathered();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_own_dir, .relative = CLI_COMPILE_WRITTEN, .joined = path);
    TEST_ASSERT_FALSE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
}

void test_a_command_lines_quoted_words_stay_complete(void)
{
    static const char s_line[] = "\"C:/Program Files/LLVM/bin/clang.exe\" -I \"a b\" -c x.c";
    VerbaLinea words = {s_text, sizeof(s_text), 0ull};
    EMBED_CALL(cli_compile_db_text_internal.compile_words_each, CliCompileDbTextCtx, .text = s_line, .length = sizeof(s_line) - 1u, .each = word_recorded, .context = &words);
    CLI_TEST_ASSERT_TEXT("C:/Program Files/LLVM/bin/clang.exe|-I|a b|-c|x.c", s_text, words.at);
}
