// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_catalog.c: the jobs catalog: case names reads the line after the case, a build script is traced by default and quiet where
// requested, a short name stores the folder and the name, a driver's parts join its literals, and the tree has a job of every group
#include "ui/cli/src/command_line/catalog_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/runs/runner.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliJob s_jobs[2048];
static CliParam s_params[8192];
static CliStep s_steps[4096];
static CliArg s_args[16384];
static unsigned int s_lists[32768];
static char s_text[1u << 21u];
static CliCatalog s_catalog;
static CliValues s_values;
static char s_shown[512];
static char s_root[CLI_PATH_MAX_LEN];
static char s_script[CLI_PATH_MAX_LEN];

// the catalog emptied
static CliCatalog *catalog_emptied(void)
{
    EMBED_CALL(cli_catalog_jobs_max.begin, CliCatalogJobsMaxCfg, .catalog = &s_catalog, .jobs = s_jobs, .jobs_max = 2048u, .params = s_params, .params_max = 8192u, .steps = s_steps, .steps_max = 4096u, .request = s_args, .args_max = 16384u, .lists = s_lists, .lists_max = 32768u, .text_out = s_text, .text_max = sizeof(s_text));
    return &s_catalog;
}

// the param `key` of the job `job`, NULL where it has none
static const CliParam *param_of(unsigned int job, const char *key)
{
    const CliJob *const one = &s_catalog.job[job];
    for (unsigned int at = 0u; at < one->params; at += 1u)
    {
        const CliParam *const param = &s_catalog.param[one->param_first + at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = param->key), .other = key, .cap = CLI_POOL_BYTES}))
        {
            return param;
        }
    }
    return NULL;
}

// the line the job's step shows recorded
static void line_recorded(const CliEachDisplayedCfg *args)
{
    memor.cpy(&(MemoriaCfg){.dst = s_shown, .src = args->displayed, .bytes = CLI_UNITY_LENGTH(args->displayed) + 1ull});
}

// the line the job shows with its trace `trace`, in s_shown, the test ignored where it shows none for want of bash
static void line_displayed(const char *trace)
{
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = &s_values);
    EMBED_CALL(cli_catalog_key_values.values_append, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = CLI_TRACE, .value = trace);
    s_shown[0] = '\0';
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_runner_err_msg_root.show, CliRunnerErrMsgRootCfg, .root = s_root, .catalog = &s_catalog, .job = 0u, .values = &s_values, .each = line_recorded, .context = NULL, .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("the job shows no line, the machine having no bash");
    }
}

// a build script in a tree, read as the catalog's one job of the group `group`
static void script_read(const char *group)
{
    char src[CLI_PATH_MAX_LEN];
    cli_unity_folder("catalog_trace", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src", .joined = src);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = src));
    cli_unity_file(s_root, "src/build_it.sh", "#!/usr/bin/env bash\necho built\n");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src/build_it.sh", .joined = s_script);
    EMBED_CALL(cli_catalog_path_internal.bash_alone, CliCatalogPathCtx, .catalog = catalog_emptied(), .root = s_root, .group = group, .path = s_script);
}

// the catalog read from the tree, the test ignored where no tree is found
static void tree_read(void)
{
    if (!EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = s_root))
    {
        TEST_IGNORE_MESSAGE("no tree is found");
    }
    EMBED_CALL(cli_catalog_root.read, CliCatalogRootCfg, .root = s_root, .catalog = catalog_emptied());
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_case_names_reads_the_line_after_the_case(void)
{
    char names[8][CLI_NAME_MAX_LEN];
    TEST_ASSERT_EQUAL_UINT(3u, EMBED_CALL(cli_catalog_text_variable_internal.case_names, CliCatalogTextVariableCtx, .text_read = "case \"$SIM\" in\n    a|b_c|d) ;;\n    *) exit 2 ;;\nesac\n", .variable = "SIM", .names_callback = names, .capacity = 8u));
    TEST_ASSERT_EQUAL_STRING("a", names[0]);
    TEST_ASSERT_EQUAL_STRING("b_c", names[1]);
    TEST_ASSERT_EQUAL_STRING("d", names[2]);
}

void test_a_builds_trace_is_verbose_by_default_or_quiet(void)
{
    script_read("build");
    const CliParam *const trace = param_of(0u, CLI_TRACE);
    TEST_ASSERT_NOT_NULL(trace);
    TEST_ASSERT_EQUAL_STRING(CLI_TRACE_VERBOSE, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = trace->fallback));
    TEST_ASSERT_EQUAL_UINT(2u, trace->choices);
    TEST_ASSERT_EQUAL_STRING(CLI_TRACE_VERBOSE, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.list[trace->first]));
    TEST_ASSERT_EQUAL_STRING(CLI_TRACE_QUIET, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.list[trace->first + 1u]));
}

void test_a_traced_build_runs_bash_with_xtrace(void)
{
    script_read("build");
    line_displayed(CLI_TRACE_VERBOSE);
    TEST_ASSERT_EQUAL_STRING("SHELLOPTS=xtrace bash src/build_it.sh", s_shown);
}

void test_a_quiet_build_runs_bash_without_xtrace(void)
{
    script_read("build");
    line_displayed(CLI_TRACE_QUIET);
    TEST_ASSERT_EQUAL_STRING("bash src/build_it.sh", s_shown);
}

void test_a_script_that_is_not_a_build_is_not_traced(void)
{
    script_read("run");
    TEST_ASSERT_NULL(param_of(0u, CLI_TRACE));
}

void test_a_short_name_stores_the_folder_and_the_name(void)
{
    char shortened[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_out_internal.catalog_short, CliCatalogOutCtx, .file = "examples/cell_tracking/build_driver.sh", .out = shortened);
    TEST_ASSERT_EQUAL_STRING("cell_tracking/build_driver.sh", shortened);
}

void test_a_short_name_of_a_path_one_folder_deep_is_unchanged(void)
{
    char shortened[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_out_internal.catalog_short, CliCatalogOutCtx, .file = "src/build_engine.sh", .out = shortened);
    TEST_ASSERT_EQUAL_STRING("src/build_engine.sh", shortened);
}

void test_a_short_name_at_the_top_is_the_name_alone(void)
{
    char shortened[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_out_internal.catalog_short, CliCatalogOutCtx, .file = "run.sh", .out = shortened);
    TEST_ASSERT_EQUAL_STRING("run.sh", shortened);
}

void test_a_drivers_parts_join_its_literals(void)
{
    static const char s_source[] = "\"  --run names one part; the parts running in the order given: schedule, iapx-prove,\"\n"
                                   "            \" entropy, floor\\n\"";
    char parts[8][CLI_NAME_MAX_LEN];
    TEST_ASSERT_EQUAL_UINT(4u, EMBED_CALL(cli_catalog_capacity_internal.driver_parts, CliCatalogCapacityCtx, .source = s_source, .parts = parts, .capacity = 8u));
    TEST_ASSERT_EQUAL_STRING("schedule", parts[0]);
    TEST_ASSERT_EQUAL_STRING("iapx-prove", parts[1]);
    TEST_ASSERT_EQUAL_STRING("entropy", parts[2]);
    TEST_ASSERT_EQUAL_STRING("floor", parts[3]);
}

void test_the_tree_has_a_job_of_every_group(void)
{
    static const char *const s_groups[10] = {"build", "protocol", "ingest", "run", "render", "sim", "view", "stage", "pipeline", "test"};
    tree_read();
    for (unsigned int group = 0u; group < 10u; group += 1u)
    {
        int found = 0;
        for (unsigned int at = 0u; !found && (at < s_catalog.jobs); at += 1u)
        {
            found = cellul.eq(&(CatenaFinitaCfg){.src = s_catalog.job[at].group, .other = s_groups[group], .cap = CLI_POOL_BYTES});
        }
        TEST_ASSERT_TRUE_MESSAGE(found, s_groups[group]);
    }
}

void test_a_catalog_that_grows_reads_the_tree_as_one_of_fixed_size(void)
{
    tree_read();
    CliCatalog grown;
    EMBED_CALL(cli_catalog_cells.take, CliCatalogCellsCfg, .catalog = &grown, .cells = g_checks_grown_owner, .jobs_max = 2048u);
    EMBED_CALL(cli_catalog_root.read, CliCatalogRootCfg, .root = s_root, .catalog = &grown);
    TEST_ASSERT_FALSE(grown.full);
    TEST_ASSERT_EQUAL_UINT(s_catalog.jobs, grown.jobs);
    TEST_ASSERT_EQUAL_UINT(s_catalog.params, grown.params);
    TEST_ASSERT_EQUAL_UINT(s_catalog.steps, grown.steps);
    TEST_ASSERT_EQUAL_UINT(s_catalog.args, grown.args);
    TEST_ASSERT_EQUAL_UINT(s_catalog.lists, grown.lists);
    TEST_ASSERT_EQUAL_UINT64(s_catalog.used, grown.used);
    TEST_ASSERT_EQUAL_MEMORY(s_catalog.text, grown.text, grown.used);
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, mmgr_owner_in_use(g_checks_grown_owner));
    const CellOwner *const cells = grown.cells;
    if (cells != NULL)
    {
        g_cli_guard->persistent_buf_release(grown.job, cells);
    }
    EMBED_CALL(cli_catalog_jobs_max.begin, CliCatalogJobsMaxCfg, .catalog = (&grown), .jobs = NULL, .jobs_max = 0u,
               .params = NULL, .params_max = 0u, .steps = NULL, .steps_max = 0u, .request = NULL, .args_max = 0u,
               .lists = NULL, .lists_max = 0u, .text_out = NULL, .text_max = 0u);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_checks_grown_owner));
}

void test_a_drivers_parts_are_read_from_its_usage(void)
{
    tree_read();
    const unsigned int run = EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = &s_catalog, .id = "run/track_driver");
    TEST_ASSERT_NOT_EQUAL(CLI_CATALOG_NONE, run);
    const CliParam *const parts = param_of(run, "run");
    TEST_ASSERT_NOT_NULL(parts);
    int track = 0;
    for (unsigned int at = 0u; at < parts->choices; at += 1u)
    {
        track |= cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.list[parts->first + at]), .other = "track", .cap = CLI_POOL_BYTES});
    }
    TEST_ASSERT_TRUE(track);
}

void test_the_catalogs_memory_is_released_as_the_tree_is_read(void)
{
    tree_read();
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_catalog_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_catalog_owner));
}
