// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_query_jobs.c: every command of the query is a job of the protocol group, a job's command line is its values, and the toolchains
// find the C compiler where one is installed
#include "ui/cli/src/command_line/catalog.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/runs/protocol.h"
#include "ui/cli/src/runs/runner.h"
#include "ui/query/src/query_loops.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliJob s_jobs[64];
static CliParam s_params[512];
static CliStep s_steps[64];
static CliArg s_args[1024];
static unsigned int s_lists[2048];
static char s_text[1u << 17u];
static CliCatalog s_catalog;
static CliValues s_values;
static char s_shown[4][1024];
static unsigned int s_shown_count;
static char s_root[CLI_PATH_MAX_LEN];

// a line a step shows recorded
static void line_recorded(const CliEachShownCfg *args)
{
    if (s_shown_count < 4u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_shown[s_shown_count], .src = args->shown, .bytes = CLI_UNITY_LENGTH(args->shown) + 1ull});
        s_shown_count += 1u;
    }
}

// the lines the job `job` shows with the values `pairs`, `count` of them, each `key=value`: how many
static unsigned int lines_shown(unsigned int job, const char *const *pairs, unsigned int count)
{
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = &s_values);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char key[CLI_NAME_MAX_LEN];
        const char *const equals = cellul.chr(&(CatenaFinitaCfg){.src = pairs[at], .cap = CLI_CELLS_POOL, .byte = '='});
        // the `=` lies in the pair, past its start
        const unsigned long long length = (equals != NULL) ? (unsigned long long)(equals - pairs[at]) : 0ull;
        memor.cpy(&(MemoriaCfg){.dst = key, .src = pairs[at], .bytes = length});
        key[length] = '\0';
        EMBED_CALL(cli_catalog_key_values.values_put, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = key, .value = (equals != NULL) ? equals + 1 : "");
    }
    s_shown_count = 0u;
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)EMBED_CALL(cli_runner_err_msg_root.show, CliRunnerErrMsgRootCfg, .root = s_root, .catalog = &s_catalog, .job = job, .values = &s_values, .each = line_recorded, .context = NULL, .err_msg = &err_msg);
    return s_shown_count;
}

// the first line the job shows with the values ends in `end`
static void assert_shown_ends(const char *end, unsigned int job, const char *const *pairs, unsigned int count)
{
    TEST_ASSERT_GREATER_THAN_UINT(0u, lines_shown(job, pairs, count));
    const unsigned long long size = CLI_UNITY_LENGTH(s_shown[0]);
    const unsigned long long end_size = CLI_UNITY_LENGTH(end);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(end_size, size);
    TEST_ASSERT_EQUAL_STRING(end, s_shown[0] + size - end_size);
}

// a choice of a param, the empty string past its last
static const char *choice_of(const CliParam *param, unsigned int at)
{
    return (at < param->choices) ? EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.list[param->first + at]) : "";
}

void setUp(void)
{
    static const char *const s_whole[9] = {"Lstar.klq", "cu.klm", "cu.krs", "sass.klm", "sass.krs", "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "c.krs"};
    char table[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    cli_unity_folder("query_jobs", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = QUERY_TABLE, .joined = table);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = table));
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = table, .relative = s_whole[at], .joined = path);
        TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "", .size = 0ull));
    }
    EMBED_CALL(cli_catalog_jobs_most.begin, CliCatalogJobsMostCfg, .catalog = &s_catalog, .jobs = s_jobs, .jobs_most = 64u, .params = s_params, .params_most = 512u, .steps = s_steps, .steps_most = 64u, .request = s_args, .args_most = 1024u, .lists = s_lists, .lists_most = 2048u, .text_out = s_text, .text_most = sizeof(s_text));
    EMBED_CALL(cli_catalog_root.query_jobs, CliCatalogRootCfg, .root = s_root, .catalog = &s_catalog);
}

void tearDown(void)
{
}

void test_every_command_of_the_query_is_a_protocol_job_run_by_orior(void)
{
    static const char *const s_ids[11] = {"protocol/course", "protocol/tables", "protocol/decode", "protocol/query", "protocol/learn", "protocol/cross", "protocol/write", "protocol/pairs", "protocol/stick", "protocol/retention", "protocol/sweep"};
    TEST_ASSERT_EQUAL_UINT(11u, s_catalog.jobs);
    for (unsigned int at = 0u; at < 11u; at += 1u)
    {
        const CliJob *const job = &s_catalog.job[at];
        TEST_ASSERT_EQUAL_STRING(s_ids[at], EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = job->id));
        TEST_ASSERT_EQUAL_STRING("protocol", job->group);
        TEST_ASSERT_GREATER_THAN_UINT(0u, job->steps);
        TEST_ASSERT_EQUAL_INT(CLI_PROGRAM_ORIOR, s_catalog.step[job->step_first].program);
    }
}

void test_the_course_is_titled_protocol(void)
{
    TEST_ASSERT_EQUAL_STRING("protocol", EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.job[0].title));
}

void test_the_course_starts_from_the_vendors_tables(void)
{
    TEST_ASSERT_EQUAL_STRING("vendor tables", choice_of(&s_catalog.param[s_catalog.job[0].param_first], 0u));
}

void test_the_courses_command_leaves_every_file(void)
{
    static const char *const s_all[1] = {"leave=all"};
    assert_shown_ends("protocol course --leave all", 0u, s_all, 1u);
}

void test_the_courses_command_starts_where_requested(void)
{
    static const char *const s_from[2] = {"from=costs", "leave=none"};
    assert_shown_ends("protocol course --from costs --leave none", 0u, s_from, 2u);
}

void test_a_ruleset_with_no_map_is_not_a_language(void)
{
    const CliParam *const from = &s_catalog.param[s_catalog.job[3].param_first];
    TEST_ASSERT_EQUAL_UINT(2u, from->choices);
    TEST_ASSERT_EQUAL_STRING("cu", choice_of(from, 0u));
    TEST_ASSERT_EQUAL_STRING("sass", choice_of(from, 1u));
}

void test_a_part_is_a_choice_and_not_required(void)
{
    const CliParam *const part = &s_catalog.param[s_catalog.job[3].param_first + 2u];
    TEST_ASSERT_EQUAL_UINT(1u, part->choices);
    TEST_ASSERT_EQUAL_STRING("sm_86", choice_of(part, 0u));
    TEST_ASSERT_FALSE(part->required);
}

void test_a_querys_command_is_its_values(void)
{
    static const char *const s_query[4] = {"from=cu", "to=sass", "part=sm_86", "dry=dry"};
    TEST_ASSERT_EQUAL_UINT(1u, lines_shown(3u, s_query, 4u));
    assert_shown_ends("protocol query --from cu --to sass --part sm_86 --dry", 3u, s_query, 4u);
}

void test_a_query_with_no_part_names_the_bridge_alone(void)
{
    static const char *const s_bridge[2] = {"from=cu", "to=sass"};
    assert_shown_ends("protocol query --from cu --to sass", 3u, s_bridge, 2u);
}

void test_a_learn_jobs_command_is_its_values(void)
{
    static const char *const s_learn[3] = {"step=turns", "base=a.cu", "leave=none"};
    assert_shown_ends("protocol learn turns --base a.cu --leave none", EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = &s_catalog, .id = "protocol/learn"), s_learn, 3u);
}

void test_the_toolchains_find_the_c_compiler_or_report_why(void)
{
    const QueryPrograms programs = cli_protocol_programs();
    char path[CLI_PATH_MAX_LEN];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int found = EMBED_CALL(programs.program, QueryEachRolePathRoomErrMsgCfg, .context = programs.context, .role = QUERY_ROLE_C, .path = path, .room = sizeof(path), .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    if (found)
    {
        TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
    }
    else
    {
        CLI_TEST_ASSERT_CONTAINS("Toolchains", why);
    }
}

void test_every_program_runs_with_a_path(void)
{
    const QueryPrograms programs = cli_protocol_programs();
    const char *const run_path = programs.run_path(programs.context);
    TEST_ASSERT_NOT_NULL(run_path);
    TEST_ASSERT_NOT_EQUAL('\0', run_path[0]);
}
