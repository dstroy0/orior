// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_query_run.c: runs of a folder are numbered newest first, retention removes all but the newest and the restored, a folder's runs in use are
// counted to the query, the crystal's program is found where it is built, a path is named under the top of the tree, a failed loop still stops its
// writer, a step joins the run left open, carried reads a name from the latest run that contains it or else the disk, and a file read is written
// under its path and recorded as a leaf
#include "ui/query/src/query_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[QUERY_PATH_MAX_LEN];
static QueryRun s_run;
static QueryRun s_step;
static QueryPrograms s_programs;
static QueryContext s_context;
static int s_ready;

// a line printed, discarded
static void print_discarded(const CliEachLineSizeCfg *args)
{
    (void)args;
}

// the folder `under` the tree's top into `folder`, made empty
static void folder_emptied(const char *under, char *folder)
{
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = under, .joined = folder);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
}

// the runs of the folder, their numbers newest first joined by `,`, in a static text
static const char *run_numbers(const char *folder)
{
    static unsigned long long s_numbers[64];
    static char s_paths[64][QUERY_PATH_MAX_LEN];
    static char s_out[256];
    const unsigned int count = EMBED_CALL(query_folder_number_internal.runs_each, QueryFolderNumberCtx, .folder = folder, .number = s_numbers, .path_callback = s_paths, .capacity_u32 = 64u);
    VerbaLinea line = {s_out, sizeof(s_out), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "," : "");
        mmgr_line_decimal(&line, s_numbers[at], 1u);
    }
    (void)mmgr_line_finish(&line);
    return s_out;
}

// a folder of three runs among names that are not runs
static void runs_numbered(char *folder)
{
    static const char *const s_names[4] = {"query3-20260101T000000.qry", "query12-20260102T000000.qry", "queryx-bad.qry", "other.txt"};
    char path[QUERY_PATH_MAX_LEN];
    folder_emptied("build/query_test/each", folder);
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = s_names[at], .joined = path);
        (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "", .size = 0ull);
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "query7-20260101T000001", .joined = path);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
}

// a folder of five runs and a restored folder, pruned to two
static void runs_pruned(char *folder)
{
    char path[QUERY_PATH_MAX_LEN];
    folder_emptied("build/query_test/retained", folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "restored", .joined = path);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
    for (unsigned int number = 1u; number <= 5u; number += 1u)
    {
        char name[64];
        VerbaLinea line = {name, sizeof(name), 0ull};
        mmgr_line_put(&line, "query");
        mmgr_line_decimal(&line, number, 1u);
        mmgr_line_put(&line, "-20260101T00000");
        mmgr_line_decimal(&line, number, 1u);
        mmgr_line_put(&line, ".qry");
        (void)mmgr_line_finish(&line);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = name, .joined = path);
        (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "", .size = 0ull);
    }
    EMBED_CALL(query_folder_max_internal.runs_prune, QueryFolderMaxCtx, .folder = folder, .most = 2u);
}

// the case ignored on a machine with no C compiler to build the writer with
static void compiler_required(void)
{
    char compiler[QUERY_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(s_programs.program, QueryEachRolePathCapacityErrMsgCfg, .context = s_programs.context, .role = QUERY_ROLE_C, .path = compiler, .capacity = sizeof(compiler), .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no C compiler to build the writer with");
    }
}

// a run started in the folder `under`: 1, or 0
static int run_started(QueryRun *run, const char *under)
{
    char folder[QUERY_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = under, .joined = folder);
    return EMBED_CALL(query_folder_context_internal.run_start, QueryFolderContextCtx, .run = run, .root = s_root, .folder = folder, .programs = &s_programs, .context = &s_context, .err_msg = &err_msg);
}

// the run ended, every file written as it leaves it
static void run_ended(QueryRun *run)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)EMBED_CALL(query_context_err_msg_run_internal.run_end, QueryContextErrMsgRunCtx, .run = run, .leave = QUERY_LEAVE_ALL, .prompter = query_terminal(), .context = &s_context, .err_msg = &err_msg);
}

// whether the latest record `name` of the run is `text`
static int latest_is(const QueryRun *run, const char *name, const char *text)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    return EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &run->writer, .name = name, .bytes_u8s = &bytes, .size_pointer = &size) && (size == CLI_UNITY_LENGTH(text)) && (memor.cmp(&(MemoriaCfg){.src = bytes, .other = text, .bytes = size}) == 0);
}

// a run stopped, its error discarded
static void run_stopped(QueryRun *run)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)EMBED_CALL(query_context_err_msg_run_internal.run_stop, QueryContextErrMsgRunCtx, .run = run, .err_msg = &err_msg);
}

// a run left open with a step joined to it
static void step_joined(char *folder)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    folder_emptied("build/query_test/joining", folder);
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/joining/qry"));
    TEST_ASSERT_TRUE(EMBED_CALL(query_err_msg_root_internal.run_joining, QueryErrMsgRootCtx, .run = &s_step, .root = s_root, .qry = s_run.writer.qry, .programs = &s_programs, .err_msg = &err_msg));
}

void setUp(void)
{
    char found[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    if (s_ready)
    {
        return;
    }
    if (!EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = found))
    {
        TEST_IGNORE_MESSAGE("no tree is found");
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root, .src = found, .bytes = CLI_UNITY_LENGTH(found) + 1u});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    s_programs = query_on_path();
    s_context = (QueryContext){s_root, &s_programs, {print_discarded, NULL}, QUERY_LEAVE_ALL, query_terminal(), {{0}}, 0u, ""};
    s_ready = 1;
}

void tearDown(void)
{
}

void test_runs_of_a_folder_are_numbered_newest_first(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    runs_numbered(folder);
    TEST_ASSERT_EQUAL_STRING("12,7,3", run_numbers(folder));
}

void test_the_next_run_is_numbered_after_the_newest(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    char next[QUERY_PATH_MAX_LEN];
    runs_numbered(folder);
    EMBED_CALL(query_folder_capacity_internal.next_name, QueryFolderCapacityCtx, .folder = folder, .name_out = next, .capacity = sizeof(next));
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = next, .other = "query13-", .cap = CLI_POOL_BYTES}));
}

void test_retention_stores_the_newest(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    runs_pruned(folder);
    TEST_ASSERT_EQUAL_STRING("5,4", run_numbers(folder));
}

void test_retention_removes_the_restored(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    char path[QUERY_PATH_MAX_LEN];
    runs_pruned(folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "restored", .joined = path);
    TEST_ASSERT_FALSE(EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path));
}

void test_a_folders_runs_are_counted_to_the_query_while_in_use(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    char path[QUERY_PATH_MAX_LEN];
    folder_emptied("build/query_test/cells", folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "query1-20260101T000000.qry", .joined = path);
    (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "", .size = 0ull);
    const unsigned long long before = mmgr_owner_in_use(g_query_owner);
    QueryRuns runs;
    const int list_entries = EMBED_CALL(query_folder_runs_internal.runs_alloc, QueryFolderRunsCtx, .folder = folder, .runs = &runs);
    const unsigned long long in_use_during = mmgr_owner_in_use(g_query_owner);
    g_cli_guard->persistent_buf_release(runs.number, g_query_owner);
    g_cli_guard->persistent_buf_release(runs.path, g_query_owner);
    *(&runs) = (QueryRuns){NULL, NULL, 0u};
    TEST_ASSERT_TRUE(list_entries);
    TEST_ASSERT_EQUAL_UINT(0u, runs.count);
    TEST_ASSERT_GREATER_THAN_UINT64(before, in_use_during);
    TEST_ASSERT_EQUAL_UINT64(before, mmgr_owner_in_use(g_query_owner));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(in_use_during, mmgr_owner_hw(g_query_owner));
}

void test_no_crystal_is_found_where_none_is_built(void)
{
    char root[QUERY_PATH_MAX_LEN];
    char found[QUERY_PATH_MAX_LEN];
    folder_emptied("build/query_test/crystal", root);
    TEST_ASSERT_FALSE(EMBED_CALL(query_capacity_root_internal.crystal, QueryCapacityRootCtx, .root = root, .program = found, .capacity = sizeof(found)));
}

void test_the_crystals_program_is_found_where_it_is_built(void)
{
    char root[QUERY_PATH_MAX_LEN];
    char engine[QUERY_PATH_MAX_LEN];
    char name[QUERY_PATH_MAX_LEN];
    char program[QUERY_PATH_MAX_LEN];
    char found[QUERY_PATH_MAX_LEN];
    folder_emptied("build/query_test/crystal", root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "build/qry_crystal/engine", .joined = engine);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = engine);
    EMBED_CALL(query_name_capacity_internal.binary_name, QueryNameCapacityCtx, .name = "qry_crystal", .binary = name, .capacity = sizeof(name));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = engine, .relative = name, .joined = program);
    (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = program, .bytes = "", .size = 0ull);
    TEST_ASSERT_TRUE(EMBED_CALL(query_capacity_root_internal.crystal, QueryCapacityRootCtx, .root = root, .program = found, .capacity = sizeof(found)));
    TEST_ASSERT_EQUAL_STRING(program, found);
}

void test_a_path_is_named_under_the_top_of_the_tree(void)
{
    char path[QUERY_PATH_MAX_LEN];
    char name[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src/cu/transpiler/lstar/protocol/table/sm_86.khw", .joined = path);
    EMBED_CALL(query_capacity_path_name_internal.by_name, QueryCapacityPathNameCtx, .top = s_root, .path = path, .name_out = name, .capacity = sizeof(name));
    TEST_ASSERT_EQUAL_STRING("src/cu/transpiler/lstar/protocol/table/sm_86.khw", name);
}

void test_windows_a_path_on_a_drive_outside_the_tree_is_named_by_its_file(void)
{
#if defined(_WIN32)
    char name[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_capacity_path_name_internal.by_name, QueryCapacityPathNameCtx, .top = s_root, .path = "C:/elsewhere/forms.txt", .name_out = name, .capacity = sizeof(name));
    TEST_ASSERT_EQUAL_STRING("forms.txt", name);
#else
    TEST_IGNORE_MESSAGE("a drive is Windows's");
#endif
}

void test_posix_a_path_at_the_root_outside_the_tree_is_named_by_its_file(void)
{
#if defined(_WIN32)
    TEST_IGNORE_MESSAGE("a path from the root is POSIX's");
#else
    char name[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_capacity_path_name_internal.by_name, QueryCapacityPathNameCtx, .top = s_root, .path = "/elsewhere/forms.txt", .name_out = name, .capacity = sizeof(name));
    TEST_ASSERT_EQUAL_STRING("forms.txt", name);
#endif
}

void test_a_run_starts_its_writer(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    compiler_required();
    folder_emptied("build/query_test/failed", folder);
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/failed/qry"));
    TEST_ASSERT_FALSE(s_run.joined);
    TEST_ASSERT_TRUE(EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &s_run.writer));
    run_stopped(&s_run);
}

void test_a_failed_loop_still_stops_its_writer_and_stores_its_run(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    compiler_required();
    folder_emptied("build/query_test/failed", folder);
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/failed/qry"));
    run_stopped(&s_run);
    TEST_ASSERT_FALSE(EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &s_run.writer));
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = s_run.writer.qry));
}

void test_a_step_joins_a_run_left_open(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    compiler_required();
    step_joined(folder);
    const int joined = s_step.joined;
    const int same_qry = cellul.eq(&(CatenaFinitaCfg){.src = s_step.writer.qry, .other = s_run.writer.qry, .cap = CLI_POOL_BYTES});
    run_stopped(&s_step);
    run_stopped(&s_run);
    TEST_ASSERT_TRUE(joined);
    TEST_ASSERT_TRUE(same_qry);
}

void test_a_steps_end_leaves_the_writer_running_with_its_line(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    char why[1024];
    char line[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    compiler_required();
    step_joined(folder);
    (void)EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx, .writer = &s_step.writer, .name = "step.console", .bytes = "a step's line\n", .size = 14ull, .priority = QRY_ORDINARY, .err_msg = &err_msg);
    run_stopped(&s_step);
    VerbaLinea joined = {line, sizeof(line), 0ull};
    const int running = EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &s_run.writer);
    const int read = EMBED_CALL(query_count_name_internal.writer_read, QueryCountNameCtx, .writer = &s_run.writer, .name = "step.console", .joined = &joined);
    run_stopped(&s_run);
    TEST_ASSERT_TRUE(running);
    TEST_ASSERT_TRUE(read);
    CLI_TEST_ASSERT_TEXT("a step's line\n", line, joined.at);
}

void test_no_step_joins_a_run_whose_writer_has_stopped(void)
{
    char folder[QUERY_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    compiler_required();
    folder_emptied("build/query_test/joining", folder);
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/joining/qry"));
    run_stopped(&s_run);
    TEST_ASSERT_FALSE(EMBED_CALL(query_err_msg_root_internal.run_joining, QueryErrMsgRootCtx, .run = &s_step, .root = s_root, .qry = s_run.writer.qry, .programs = &s_programs, .err_msg = &err_msg));
}

void test_transferred_reads_a_name_from_the_latest_run_else_the_disk_and_none_from_nowhere(void)
{
    static const char *const s_names[3] = {"carried_check_forms.txt", "carried_check_faulted.txt", "carried_check_nowhere.txt"};
    char base[QUERY_PATH_MAX_LEN];
    char folder[QUERY_PATH_MAX_LEN];
    char faulted[QUERY_PATH_MAX_LEN];
    char files[3][QUERY_PATH_MAX_LEN];
    char why[1024];
    const char *paths[3];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    compiler_required();
    folder_emptied("build/query_test/carried", base);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "qry", .joined = folder);
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/carried/qry"));
    (void)EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx, .writer = &s_run.writer, .name = "carried_check_forms.txt", .bytes = "from the run before\n", .size = 20ull, .priority = QRY_ORDINARY, .err_msg = &err_msg);
    run_ended(&s_run);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "carried_check_faulted.txt", .joined = faulted);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = faulted, .bytes = "from the disk\n", .size = 14ull));
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/carried/qry"));
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = s_names[at], .joined = files[at]);
        paths[at] = files[at];
    }
    err_msg.at = 0ull;
    const int carried = EMBED_CALL(query_count_folder_internal.run_output_relay, QueryCountFolderCtx, .run = &s_run, .folder = folder, .files = paths, .count = 3u, .crystal = NULL, .err_msg = &err_msg);
    const int from_run = latest_is(&s_run, "carried_check_forms.txt", "from the run before\n");
    const int from_disk = latest_is(&s_run, "carried_check_faulted.txt", "from the disk\n");
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    const int from_nowhere = EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &s_run.writer, .name = "carried_check_nowhere.txt", .bytes_u8s = &bytes, .size_pointer = &size);
    run_ended(&s_run);
    TEST_ASSERT_TRUE(carried);
    TEST_ASSERT_TRUE(from_run);
    TEST_ASSERT_TRUE(from_disk);
    TEST_ASSERT_FALSE(from_nowhere);
}

void test_a_file_read_is_written_under_its_path_and_recorded_as_a_leaf(void)
{
    char base[QUERY_PATH_MAX_LEN];
    char file[QUERY_PATH_MAX_LEN];
    char absent[QUERY_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    compiler_required();
    folder_emptied("build/query_test/read", base);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "sm_86.khw", .joined = file);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "absent.khw", .joined = absent);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file, .bytes = "forms\n", .size = 6ull));
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/read/qry"));
    const char *const read[2] = {file, absent};
    const int was_read = EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run, .paths_letters = read, .count = 2u, .err_msg = &err_msg);
    const int written = latest_is(&s_run, "build/query_test/read/sm_86.khw", "forms\n");
    const unsigned int count = s_run.read.count;
    const int leaf_named = cellul.eq(&(CatenaFinitaCfg){.src = s_run.read.path[0], .other = file, .cap = CLI_POOL_BYTES});
    run_ended(&s_run);
    TEST_ASSERT_TRUE(was_read);
    TEST_ASSERT_TRUE(written);
    TEST_ASSERT_EQUAL_UINT(1u, count);
    TEST_ASSERT_TRUE(leaf_named);
}

void test_a_file_written_is_recorded_as_a_leaf(void)
{
    char base[QUERY_PATH_MAX_LEN];
    char file[QUERY_PATH_MAX_LEN];
    char left[QUERY_PATH_MAX_LEN];
    compiler_required();
    folder_emptied("build/query_test/read", base);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "sm_86.khw", .joined = file);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "left.txt", .joined = left);
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/read/qry"));
    const char *const lefts[2] = {file, left};
    EMBED_CALL(query_count_err_msg_internal.run_leave, QueryCountErrMsgCtx, .run = &s_run, .paths_letters = lefts, .count = 2u);
    const unsigned int count = s_run.left.count;
    const int leaf_named = cellul.eq(&(CatenaFinitaCfg){.src = s_run.left.path[1], .other = left, .cap = CLI_POOL_BYTES});
    const unsigned int read_capacity = s_run.read.capacity;
    const unsigned int left_capacity = s_run.left.capacity;
    run_ended(&s_run);
    TEST_ASSERT_EQUAL_UINT(2u, count);
    TEST_ASSERT_TRUE(leaf_named);
    TEST_ASSERT_EQUAL_UINT(QUERY_LEAVES_MAX, read_capacity);
    TEST_ASSERT_EQUAL_UINT(QUERY_LEAVES_MAX, left_capacity);
}

void test_a_later_step_reads_the_runs_own_not_the_disks_older_copy(void)
{
    char base[QUERY_PATH_MAX_LEN];
    char file[QUERY_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    compiler_required();
    folder_emptied("build/query_test/read", base);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "sm_86.khw", .joined = file);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file, .bytes = "forms\n", .size = 6ull));
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/read/qry"));
    const char *const read[1] = {file};
    (void)EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run, .paths_letters = read, .count = 1u, .err_msg = &err_msg);
    err_msg.at = 0ull;
    (void)EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx, .writer = &s_run.writer, .name = "build/query_test/read/sm_86.khw", .bytes = "forms an earlier step wrote\n", .size = 28ull, .priority = QRY_ORDINARY, .err_msg = &err_msg);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file, .bytes = "forms on the disk\n", .size = 18ull));
    err_msg.at = 0ull;
    const int was_read = EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run, .paths_letters = read, .count = 1u, .err_msg = &err_msg);
    const int run_own = latest_is(&s_run, "build/query_test/read/sm_86.khw", "forms an earlier step wrote\n");
    run_ended(&s_run);
    TEST_ASSERT_TRUE(was_read);
    TEST_ASSERT_TRUE(run_own);
}

void test_a_runs_end_releases_the_cells_of_its_leaves(void)
{
    char base[QUERY_PATH_MAX_LEN];
    char file[QUERY_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    compiler_required();
    const unsigned long long before = mmgr_owner_in_use(g_query_owner);
    folder_emptied("build/query_test/read", base);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "sm_86.khw", .joined = file);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file, .bytes = "forms\n", .size = 6ull));
    TEST_ASSERT_TRUE(run_started(&s_run, "build/query_test/read/qry"));
    const char *const read[1] = {file};
    (void)EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run, .paths_letters = read, .count = 1u, .err_msg = &err_msg);
    run_ended(&s_run);
    TEST_ASSERT_NULL(s_run.read.path);
    TEST_ASSERT_NULL(s_run.left.path);
    TEST_ASSERT_EQUAL_UINT64(before, mmgr_owner_in_use(g_query_owner));
}
