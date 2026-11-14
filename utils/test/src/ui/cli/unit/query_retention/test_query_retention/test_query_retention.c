// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_query_retention.c: retention: a folder whose newest writer runs is left, the writer's account is read from its record, a process
// of the tree on the part is found however its path is written, the part's allowed use is read, folders with runs skip git's and the
// restored, and the median rate names the runs under half of it
#include "ui/query/src/runs/retention_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[QUERY_PATH_MAX_LEN];
static char s_lines[8][1024];
static unsigned int s_line_count;
static QueryRun s_live;
static QueryPrograms s_programs;
static QueryContext s_context;
static unsigned char s_blob[48];
static QueryAccount s_account;

// a line printed, recorded
static void line_printed(const CliEachLineSizeCfg *args)
{
    if (s_line_count < 8u)
    {
        VerbaLinea line = {s_lines[s_line_count], sizeof(s_lines[0]), 0ull};
        mmgr_line_put_n(&line, args->line, args->size);
        (void)mmgr_line_finish(&line);
        s_line_count += 1u;
    }
}

// whether a printed line contains `text`
static int printed_contains(const char *text)
{
    for (unsigned int at = 0u; at < s_line_count; at += 1u)
    {
        if (cellul.find(&(CatenaFinitaCfg){.src = s_lines[at], .cap = CLI_POOL_BYTES, .other = text, .other_cap = CLI_POOL_BYTES}) != NULL)
        {
            return 1;
        }
    }
    return 0;
}

// a writer's record of 3 records, 1234567 bytes and 2.25 ms, in s_blob
static void account_written(void)
{
    static const unsigned long long s_words[6] = {3ull, 1234567ull, 0ull, 0ull, 0ull, 2250000ull};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            // a byte of the word, the low byte first
            s_blob[(at * 8u) + byte] = (unsigned char)((s_words[at] >> (8u * byte)) & 0xffu);
        }
    }
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
    s_programs = query_on_path();
    s_context = (QueryContext){s_root, &s_programs, {line_printed, NULL}, QUERY_LEAVE_ALL, query_terminal(), {{0}}, 0u, ""};
    s_line_count = 0u;
    account_written();
}

void tearDown(void)
{
}

void test_the_writers_account_is_read_from_its_record(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_retention_blob_internal.account_read, QueryRetentionBlobCtx, .blob = s_blob, .size = sizeof(s_blob), .account = &s_account));
    TEST_ASSERT_EQUAL_UINT64(3ull, s_account.records);
    TEST_ASSERT_EQUAL_UINT64(1234567ull, s_account.bytes);
    TEST_ASSERT_EQUAL_DOUBLE(2.25, s_account.ms);
    TEST_ASSERT_EQUAL_UINT64(0ull, s_account.panics);
}

void test_an_accounts_rate_is_in_mebibytes_a_second(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_retention_blob_internal.account_read, QueryRetentionBlobCtx, .blob = s_blob, .size = sizeof(s_blob), .account = &s_account));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, (1234567.0 / 1048576.0) / (2.25 / 1000.0), EMBED_CALL(query_retention_internal.account_rate, QueryRetentionCtx, .account_read = &s_account));
}

void test_bytes_that_are_no_account_are_refused(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(query_retention_blob_internal.account_read, QueryRetentionBlobCtx, .blob = (const unsigned char *)"nothing here", .size = 12ull, .account = &s_account));
}

void test_no_time_writing_is_no_rate(void)
{
    memor.set(&(MemoriaCfg){.dst = s_blob + 40u, .val = 0, .bytes = 8u});
    TEST_ASSERT_TRUE(EMBED_CALL(query_retention_blob_internal.account_read, QueryRetentionBlobCtx, .blob = s_blob, .size = sizeof(s_blob), .account = &s_account));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, EMBED_CALL(query_retention_internal.account_rate, QueryRetentionCtx, .account_read = &s_account));
}

void test_a_process_of_the_tree_is_found_with_backslashes(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_retention_root_internal.on_the_part, QueryRetentionRootCtx, .apps = "C:\\Users\\x\\orior-engine\\build\\engine\\orior_cubin_run.exe\n", .root = "C:/Users/x/orior-engine"));
}

void test_a_process_of_the_tree_is_found_in_either_case(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_retention_root_internal.on_the_part, QueryRetentionRootCtx, .apps = "c:/users/x/orior-engine/build/engine/khw_write.exe\n", .root = "C:/Users/x/orior-engine"));
}

void test_another_trees_process_is_not_this_trees(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(query_retention_root_internal.on_the_part, QueryRetentionRootCtx, .apps = "C:\\Users\\x\\other_tree\\build\\engine\\orior_cubin_run.exe\n", .root = "C:/Users/x/orior-engine"));
}

void test_no_process_is_not_this_trees(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(query_retention_root_internal.on_the_part, QueryRetentionRootCtx, .apps = "", .root = "C:/Users/x/orior-engine"));
}

void test_the_parts_allowed_use_defaults_to_ten(void)
{
    TEST_ASSERT_EQUAL_INT64(10ll, EMBED_CALL(query_retention_input_internal.part_used_max, QueryRetentionInputCtx, .supplied = NULL));
}

void test_the_parts_allowed_use_is_read(void)
{
    TEST_ASSERT_EQUAL_INT64(25ll, EMBED_CALL(query_retention_input_internal.part_used_max, QueryRetentionInputCtx, .supplied = "25"));
}

void test_a_negative_bound_is_read_and_marks_the_part_used(void)
{
    TEST_ASSERT_EQUAL_INT64(-1ll, EMBED_CALL(query_retention_input_internal.part_used_max, QueryRetentionInputCtx, .supplied = "-1"));
}

void test_a_bound_that_is_not_a_number_uses_the_default(void)
{
    TEST_ASSERT_EQUAL_INT64(10ll, EMBED_CALL(query_retention_input_internal.part_used_max, QueryRetentionInputCtx, .supplied = "many"));
}

void test_folders_with_runs_skip_gits_and_the_restored(void)
{
    static const char *const s_folders[3] = {"a/qry/restored", ".git/qry", "b"};
    static const char *const s_files[4] = {"a/qry/query1-x.qry", "a/qry/restored/query1-x.qry", ".git/qry/query1-x.qry", "b/other.qry"};
    static char s_found[64][QUERY_PATH_MAX_LEN];
    char base[QUERY_PATH_MAX_LEN];
    char path[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/query_test/folders", .joined = base);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = base);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = s_folders[at], .joined = path);
        TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    }
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = s_files[at], .joined = path);
        TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "", .size = 0ull));
    }
    const unsigned int count = EMBED_CALL(query_retention_root_found_internal.folders_with_runs, QueryRetentionRootFoundCtx, .root = base, .found = s_found, .capacity = 64u);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "a/qry", .joined = path);
    TEST_ASSERT_EQUAL_UINT(1u, count);
    TEST_ASSERT_EQUAL_STRING(path, s_found[0]);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = base);
}

void test_the_part_in_use_is_printed_on_its_own_line(void)
{
    EMBED_CALL(query_retention_context_free_part_internal.crystals_reject, QueryRetentionContextFreePartCtx, .context = &s_context, .free_part = 0, .why = "the part is used 3 percent", .built = 0);
    TEST_ASSERT_EQUAL_UINT(2u, s_line_count);
    TEST_ASSERT_EQUAL_STRING("  no run is stored as its crystal this time:", s_lines[0]);
    TEST_ASSERT_EQUAL_STRING("  the part is used 3 percent", s_lines[1]);
}

void test_an_unbuilt_crystal_is_printed(void)
{
    EMBED_CALL(query_retention_context_free_part_internal.crystals_reject, QueryRetentionContextFreePartCtx, .context = &s_context, .free_part = 1, .why = "", .built = 0);
    TEST_ASSERT_EQUAL_UINT(1u, s_line_count);
    TEST_ASSERT_EQUAL_STRING("  no run is stored as its crystal this time: the crystal's program is not built", s_lines[0]);
}

void test_nothing_is_printed_where_the_crystal_is_built(void)
{
    EMBED_CALL(query_retention_context_free_part_internal.crystals_reject, QueryRetentionContextFreePartCtx, .context = &s_context, .free_part = 1, .why = "", .built = 1);
    TEST_ASSERT_EQUAL_UINT(0u, s_line_count);
}

void test_the_median_rate_is_printed(void)
{
    static char s_names[3][QUERY_PATH_MAX_LEN] = {"query1-a.qry", "query2-b.qry", "query3-c.qry"};
    static const double s_rates[3] = {10.0, 4.0, 12.0};
    EMBED_CALL(query_retention_context_folder_internal.median_print, QueryRetentionContextFolderCtx, .context = &s_context, .folder = "x/qry", .names = s_names, .rates = s_rates, .count = 3u);
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_line_count);
    CLI_TEST_ASSERT_CONTAINS("3 runs read, the median 10 MiB/s", s_lines[0]);
}

void test_the_runs_under_half_the_median_are_named(void)
{
    static char s_names[3][QUERY_PATH_MAX_LEN] = {"query1-a.qry", "query2-b.qry", "query3-c.qry"};
    static const double s_rates[3] = {10.0, 4.0, 12.0};
    EMBED_CALL(query_retention_context_folder_internal.median_print, QueryRetentionContextFolderCtx, .context = &s_context, .folder = "x/qry", .names = s_names, .rates = s_rates, .count = 3u);
    TEST_ASSERT_TRUE(printed_contains("query2-b.qry wrote at under half the median"));
}

void test_no_runs_print_nothing(void)
{
    static char s_names[1][QUERY_PATH_MAX_LEN] = {""};
    static const double s_rates[1] = {0.0};
    EMBED_CALL(query_retention_context_folder_internal.median_print, QueryRetentionContextFolderCtx, .context = &s_context, .folder = "x/qry", .names = s_names, .rates = s_rates, .count = 0u);
    TEST_ASSERT_EQUAL_UINT(0u, s_line_count);
}

void test_a_folder_whose_newest_writer_runs_is_left(void)
{
    char compiler[QUERY_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(s_programs.program, QueryEachRolePathCapacityErrMsgCfg, .context = s_programs.context, .role = QUERY_ROLE_C, .path = compiler, .capacity = sizeof(compiler), .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no C compiler to build the writer with");
    }
    char base[QUERY_PATH_MAX_LEN];
    char folder[QUERY_PATH_MAX_LEN];
    char old[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/query_test/retained_live", .joined = base);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = base);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "qry", .joined = folder);
    err_msg.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.run_start, QueryFolderContextCtx, .run = &s_live, .root = s_root, .folder = folder, .programs = &s_programs, .context = &s_context, .err_msg = &err_msg));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "query0-20250101T000000.qry", .joined = old);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = old, .bytes = "", .size = 0ull));
    s_line_count = 0u;
    EMBED_CALL(query_retention_context_own_internal.folder, QueryRetentionContextOwnCtx, .context = &s_context, .folder = folder, .crystal = NULL, .max = 64u, .own = &s_live.writer);
    const int printed = printed_contains("left as it is, its newest run's writer runs");
    const int stayed = EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = old);
    err_msg.at = 0ull;
    (void)EMBED_CALL(query_context_err_msg_run_internal.run_stop, QueryContextErrMsgRunCtx, .run = &s_live, .err_msg = &err_msg);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = base);
    TEST_ASSERT_TRUE(printed);
    TEST_ASSERT_TRUE(stayed);
}
