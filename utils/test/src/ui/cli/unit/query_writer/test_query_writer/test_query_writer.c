// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_query_writer.c: the query's programs and the writer: every role has a readable name, the C compiler is found on the PATH, a program
// beside nvcc is looked for beside it, a missing program is reported by name, a run is started, written, read and stopped, and a program's
// output is teed into the run
#include "ui/query/src/tools/tools_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// What a run's life showed, read once for the process
typedef struct
{
    int run_result;
    int running_before_start;
    int started;
    int ready;
    int joined;
    int record_read;
    int absent_read;
    int running_after_stop;
    int read_after_stop;
    int patterns_read;
    char whole[64];
    unsigned long long full_size;
} WriterRun;

static char s_root[QUERY_PATH_MAX_LEN];
static QueryPrograms s_programs;
static WriterRun s_life;
static char s_error[1024];

// a printed line discarded
static void print_discarded(const CliEachLineSizeCfg *args)
{
    (void)args;
}

// the folder of `path`, its last part cut, into `folder`, QUERY_PATH_MAX_LEN bytes
static void folder_of(const char *path, char *folder)
{
    memor.cpy(&(MemoriaCfg){.dst = folder, .src = path, .bytes = CLI_UNITY_LENGTH(path) + 1ull});
    char *cut = folder + CLI_UNITY_LENGTH(folder);
    while ((cut > folder) && (cut[-1] != '/') && (cut[-1] != '\\'))
    {
        cut -= 1;
    }
    cut[(cut > folder) ? -1 : 0] = '\0';
}

// nothing is found, each reported by its name
static int nothing_found(const QueryEachRolePathCapacityErrMsgCfg *args)
{
    QueryRole role = args->role;
    args->path[0] = '\0';
    mmgr_line_put(args->err_msg, EMBED_CALL(query_table.role_name, QueryCfg, .role = role));
    mmgr_line_put(args->err_msg, " is not found");
    return 0;
}

// the tree's root in s_root with forward slashes, the test ignored where no tree is found
static void tree_required(void)
{
    char found[CLI_PATH_MAX_LEN];
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

// a writer built into build/query_test/`name`, its run's .qry there; the test ignored where there is no C compiler
static void writer_built(const char *name, char *out, QueryWriter *writer)
{
    char compiler[QUERY_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    tree_required();
    if (!EMBED_CALL(s_programs.program, QueryEachRolePathCapacityErrMsgCfg, .context = s_programs.context, .role = QUERY_ROLE_C, .path = compiler, .capacity = sizeof(compiler), .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no C compiler to build the writer with");
    }
    char under[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {under, sizeof(under), 0ull};
    mmgr_line_put(&line, "build/query_test/");
    mmgr_line_put(&line, name);
    (void)mmgr_line_finish(&line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = under, .joined = out);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = out);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = out));
    memor.set(&(MemoriaCfg){.dst = writer, .val = 0, .bytes = sizeof(*writer)});
    err_msg.at = 0ull;
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_name_capacity_internal.tool_built_into, QueryNameCapacityCtx, .root = s_root, .programs = &s_programs, .name = "orior_qry_writer", .out_text = out, .binary = writer->binary, .capacity = sizeof(writer->binary), .err_msg = &err_msg), "the writer is built");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = out, .relative = "query1-test.qry", .joined = writer->qry);
    memor.cpy(&(MemoriaCfg){.dst = writer->top, .src = s_root, .bytes = CLI_UNITY_LENGTH(s_root) + 1ull});
    writer->path = s_programs.run_path(s_programs.context);
}

// whether the latest record `name` of the run contains `text`
static int latest_is(const QueryWriter *writer, const char *name, const char *text)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    return EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = writer, .name = name, .bytes_u8s = &bytes, .size_pointer = &size) && (size == CLI_UNITY_LENGTH(text)) && (memor.cmp(&(MemoriaCfg){.src = bytes, .other = text, .bytes = size}) == 0);
}

// a run started, written, read and stopped, once for the process, what it showed in s_life
static void run_lived(void)
{
    static QueryWriter s_writer;
    if (s_life.run_result)
    {
        return;
    }
    char out[QUERY_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    writer_built("writer", out, &s_writer);
    s_life.run_result = 1;
    s_life.running_before_start = EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &s_writer);
    unsigned long long process = 0ull;
    s_life.started = EMBED_CALL(query_writer_internal.writer_start, QueryWriterCtx, .writer = &s_writer, .process_u64s = &process, .err_msg = &err_msg);
    s_life.ready = EMBED_CALL(query_writer_internal.writer_init, QueryWriterCtx, .writer = &s_writer) && EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &s_writer);
    s_life.joined = EMBED_CALL(query_err_msg_writer_internal.writer_join, QueryErrMsgWriterCtx, .writer_out = &s_writer);
    err_msg.at = 0ull;
    (void)EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx, .writer = &s_writer, .name = "a.txt", .bytes = "one\ntwo\n", .size = 8ull, .priority = QRY_ORDINARY, .err_msg = &err_msg);
    s_life.record_read = latest_is(&s_writer, "a.txt", "one\ntwo\n");
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    s_life.absent_read = EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &s_writer, .name = "none.txt", .bytes_u8s = &bytes, .size_pointer = &size);
    EMBED_CALL(query_err_msg_writer_internal.writer_part, QueryErrMsgWriterCtx, .writer_out = &s_writer);
    err_msg.at = 0ull;
    (void)EMBED_CALL(query_err_msg_writer_internal.writer_stop, QueryErrMsgWriterCtx, .writer_out = &s_writer, .err_msg = &err_msg);
    QueryProcess waited = {process, 0ull};
    (void)EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &waited);
    s_life.running_after_stop = EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &s_writer);
    s_life.read_after_stop = latest_is(&s_writer, "a.txt", "one\ntwo\n");
    VerbaLinea line = {s_life.whole, sizeof(s_life.whole), 0ull};
    s_life.patterns_read = EMBED_CALL(query_count_name_internal.writer_read, QueryCountNameCtx, .writer = &s_writer, .name = "a.txt", .joined = &line);
    s_life.full_size = line.at;
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = out);
}

void setUp(void)
{
    cli_unity_begun();
    s_programs = query_on_path();
}

void tearDown(void)
{
}

void test_every_role_has_a_readable_name(void)
{
    // the role count is a small enumerator
    for (unsigned int role = 0u; role < (unsigned int)QUERY_ROLE_COUNT; role += 1u)
    {
        const char *const name = EMBED_CALL(query_table.role_name, QueryCfg, .role = (QueryRole)role);
        TEST_ASSERT_NOT_NULL(name);
        TEST_ASSERT_NOT_EQUAL('\0', name[0]);
        CLI_TEST_ASSERT_NOT_CONTAINS("Role", name);
    }
}

void test_the_c_compiler_is_found_on_the_path_or_reported(void)
{
    char path[QUERY_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    const int found = EMBED_CALL(s_programs.program, QueryEachRolePathCapacityErrMsgCfg, .context = s_programs.context, .role = QUERY_ROLE_C, .path = path, .capacity = sizeof(path), .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    if (found)
    {
        TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
    }
    else
    {
        CLI_TEST_ASSERT_CONTAINS("the C compiler", s_error);
    }
}

void test_a_program_adjacent_nvcc_is_looked_for_adjacent_it(void)
{
    char nvcc[QUERY_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    if (!EMBED_CALL(s_programs.program, QueryEachRolePathCapacityErrMsgCfg, .context = s_programs.context, .role = QUERY_ROLE_NVCC, .path = nvcc, .capacity = sizeof(nvcc), .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no nvcc is found");
    }
    char nvdisasm[QUERY_PATH_MAX_LEN];
    char one[QUERY_PATH_MAX_LEN];
    char other[QUERY_PATH_MAX_LEN];
    err_msg.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(s_programs.program, QueryEachRolePathCapacityErrMsgCfg, .context = s_programs.context, .role = QUERY_ROLE_NVDISASM, .path = nvdisasm, .capacity = sizeof(nvdisasm), .err_msg = &err_msg));
    folder_of(nvcc, one);
    folder_of(nvdisasm, other);
    TEST_ASSERT_EQUAL_STRING(one, other);
}

void test_a_missing_program_is_reported_by_its_name(void)
{
    const QueryPrograms nothing = {nothing_found, NULL, NULL};
    char path[QUERY_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(nothing.program, QueryEachRolePathCapacityErrMsgCfg, .context = nothing.context, .role = QUERY_ROLE_PYTHON, .path = path, .capacity = sizeof(path), .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_line_finish(&err_msg));
    TEST_ASSERT_EQUAL_STRING("python is not found", s_error);
}

void test_no_writer_runs_before_it_is_started(void)
{
    run_lived();
    TEST_ASSERT_FALSE(s_life.running_before_start);
}

void test_the_writer_starts_and_comes_ready(void)
{
    run_lived();
    TEST_ASSERT_TRUE(s_life.started);
    TEST_ASSERT_TRUE(s_life.ready);
}

void test_this_process_joins_the_run(void)
{
    run_lived();
    TEST_ASSERT_TRUE(s_life.joined);
}

void test_a_record_is_read_back(void)
{
    run_lived();
    TEST_ASSERT_TRUE(s_life.record_read);
}

void test_a_record_not_written_is_not_read(void)
{
    run_lived();
    TEST_ASSERT_FALSE(s_life.absent_read);
}

void test_the_writer_stops(void)
{
    run_lived();
    TEST_ASSERT_FALSE(s_life.running_after_stop);
}

void test_a_record_is_read_from_the_qry_once_the_writer_stopped(void)
{
    run_lived();
    TEST_ASSERT_TRUE(s_life.read_after_stop);
}

void test_a_name_is_read_complete_as_written(void)
{
    run_lived();
    TEST_ASSERT_TRUE(s_life.patterns_read);
    CLI_TEST_ASSERT_TEXT("one\ntwo\n", s_life.whole, s_life.full_size);
}

void test_a_programs_exit_code_and_output_are_teed_into_the_run(void)
{
    static QueryWriter s_writer;
    static char s_console[4096];
    char out[QUERY_PATH_MAX_LEN];
    char python[QUERY_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    writer_built("tee", out, &s_writer);
    if (!EMBED_CALL(s_programs.program, QueryEachRolePathCapacityErrMsgCfg, .context = s_programs.context, .role = QUERY_ROLE_PYTHON, .path = python, .capacity = sizeof(python), .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no Python to tee with");
    }
    unsigned long long process = 0ull;
    err_msg.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(query_writer_internal.writer_start, QueryWriterCtx, .writer = &s_writer, .process_u64s = &process, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(query_writer_internal.writer_init, QueryWriterCtx, .writer = &s_writer));
    TEST_ASSERT_TRUE(EMBED_CALL(query_err_msg_writer_internal.writer_join, QueryErrMsgWriterCtx, .writer_out = &s_writer));
    QueryContext context = {s_root, &s_programs, {print_discarded, NULL}, QUERY_LEAVE_ALL, query_terminal(), {{0}}, 0u, ""};
    const char *const words[4] = {python, "-c", "import sys; print('out'); print('err', file=sys.stderr); sys.exit(3)", NULL};
    long long code = 0ll;
    err_msg.at = 0ull;
    const int teed = EMBED_CALL(query_words_name_internal.writer_tee, QueryWordsNameCtx, .writer = &s_writer, .context = &context, .name = "teed", .words = words, .code = &code, .err_msg = &err_msg);
    VerbaLinea console = {s_console, sizeof(s_console), 0ull};
    // the tee writes a piece at a time, a blob each: every blob joined is the console
    (void)EMBED_CALL(query_count_name_internal.writer_read, QueryCountNameCtx, .writer = &s_writer, .name = "teed.console", .joined = &console);
    (void)mmgr_line_finish(&console);
    EMBED_CALL(query_err_msg_writer_internal.writer_part, QueryErrMsgWriterCtx, .writer_out = &s_writer);
    err_msg.at = 0ull;
    (void)EMBED_CALL(query_err_msg_writer_internal.writer_stop, QueryErrMsgWriterCtx, .writer_out = &s_writer, .err_msg = &err_msg);
    QueryProcess waited = {process, 0ull};
    (void)EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &waited);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = out);
    TEST_ASSERT_TRUE(teed);
    TEST_ASSERT_EQUAL_INT64(3ll, code);
    CLI_TEST_ASSERT_CONTAINS("out", s_console);
    CLI_TEST_ASSERT_CONTAINS("err", s_console);
}
