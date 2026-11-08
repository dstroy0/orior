// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_job_runs.c: the Run pane's store against a catalog of the suite's own: the jobs listed by group in the
// catalog's order, groups opened and closed, the filter typed and erased; a job chosen with its file, its params and
// the choices of the one focused, the rows read agreeing with the sections for every row; the form typed, its choices
// taken, its values with their defaults; a start refused with the runner's sentence; what a run reports taken in on
// the step, its lines plain with their spans, its end ok or failed, the lines past CLI_LINES_MAX and the ended runs
// past CLI_JOB_RUNS_RETAINED released; the follow of the last row; a real run started, its lines read and its end; a
// run stopped; and every cell let go as the catalog is read again and as the store ends
#include "ui/cli/src/runs/job_runs_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the catalog's storage, and the tree a real run runs in
static CliJob s_jobs[16];
static CliParam s_params[32];
static CliStep s_steps[16];
static CliArg s_args[32];
static unsigned int s_lists[64];
static char s_text[1u << 14u];
static CliCatalog s_catalog;
static char s_root[CLI_PATH_MAX_LEN];

// the catalog laid: two build jobs, the first with a choice, a choice of many and a required word, a sim job and a
// view job without a title; the run job's one step a bash script of the tree, `script`
static void catalog_laid(const char *script)
{
    EMBED_CALL(cli_catalog_jobs_max.begin, CliCatalogJobsMaxCfg, .catalog = &s_catalog, .jobs = s_jobs, .jobs_max = 16u, .params = s_params, .params_max = 32u, .steps = s_steps, .steps_max = 16u, .request = s_args, .args_max = 32u, .lists = s_lists, .lists_max = 64u, .text_out = s_text, .text_max = sizeof(s_text));
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "build/alpha", .group = "build", .title = "Alpha build", .file = "src/alpha.sh", .about = "builds the alpha", .opens = "");
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = &s_catalog, .key = "target", .type = "choice", .required = 0);
    EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = &s_catalog, .choice = "x86");
    EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = &s_catalog, .choice = "arm");
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = &s_catalog, .key = "parts", .type = "many", .required = 0);
    EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = &s_catalog, .choice = "one");
    EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = &s_catalog, .choice = "two");
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = &s_catalog, .key = "name", .type = "word", .required = 1);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = &s_catalog, .text = script, .program = CLI_PROGRAM_BASH);
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "build/beta", .group = "build", .title = "Beta", .file = "src/beta.sh", .about = "beta things", .opens = "");
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "sim/gamma", .group = "sim", .title = "Gamma sim", .file = "src/gamma.sh", .about = "a simulation", .opens = "");
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "view/delta", .group = "view", .title = "", .file = "src/delta.sh", .about = "views delta", .opens = "");
}

// the store read from the suite's catalog
static void store_read(void)
{
    catalog_laid("job.sh");
    EMBED_CALL(cli_job_runs_root.read, CliJobRunsRootCfg, .root = NULL, .catalog = &s_catalog);
}

static unsigned int rows(void)
{
    return EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u);
}

// the row `at`, the test failed where there is none
static CliJobRunRow row(unsigned int at)
{
    CliJobRunRow read;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_job_runs_row.row, CliJobRunsRowCfg, .row_at = at, .row_out = &read));
    return read;
}

static int pressed(unsigned int at)
{
    return EMBED_CALL(cli_job_runs_row.press, CliJobRunsRowCfg, .row_at = at);
}

static void typed(const char *text)
{
    EMBED_CALL(cli_job_runs_field.type, CliJobRunsFieldCfg, .bytes = text, .length = CLI_UNITY_LENGTH(text));
}

// the first row of `type` from `from`, the rows' count where none is
static unsigned int row_of(unsigned int type, unsigned int from)
{
    const unsigned int count = rows();
    for (unsigned int at = from; at < count; at += 1u)
    {
        if (row(at).type == type)
        {
            return at;
        }
    }
    return count;
}

// a report of the run `run` handed to the store's sink as its thread hands it
static void reported(unsigned int type, unsigned long long run, const char *job, const char *stream, const char *text, long long code, int stopped)
{
    const CliRunEvent event = {type, run, job, stream, text, type == CLI_JOB_RUNS_ENDED, code, stopped, NULL, 0u, NULL, 0.0};
    EMBED_CALL(cli_job_runs_sink.emit, CliEachEventCfg, .context = NULL, .event = &event);
}

// the first job, its group opened and the job chosen: the row of its first param
static unsigned int alpha_chosen(void)
{
    store_read();
    (void)pressed(0u);
    (void)pressed(1u);
    return row_of(CLI_JOB_RUN_PARAM, 0u);
}

// the store stepped until the run shown ends, at most 30 seconds: 1 where it ended
static int run_ended(void)
{
    const unsigned long long until = cli_clock_milliseconds() + 30000ULL;
    while (cli_clock_milliseconds() < until)
    {
        (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
        if (!EMBED_CALL(cli_job_runs_run.live, CliJobRunsRunCfg, .root = s_root) && (row_of(CLI_JOB_RUN_END, 0u) < rows()))
        {
            return 1;
        }
        cli_thread_yield();
    }
    return 0;
}

void setUp(void)
{
    cli_unity_begun();
    EMBED_CALL(cli_job_runs_woken.begin, CliJobRunsWokenCfg, .woken = NULL);
}

void tearDown(void)
{
    EMBED_CALL(cli_job_runs_woken.end, CliJobRunsWokenCfg, .woken = NULL);
}

void test_read_lists_the_groups_in_the_catalogs_order_each_opened_and_closed(void)
{
    store_read();
    TEST_ASSERT_EQUAL_UINT(3u, rows());
    CLI_TEST_ASSERT_TEXT("build", row(0u).text, row(0u).text_length);
    TEST_ASSERT_EQUAL_UINT64(2ULL, row(0u).number);
    CLI_TEST_ASSERT_TEXT("2", row(0u).second, row(0u).second_length);
    CLI_TEST_ASSERT_TEXT("sim", row(1u).text, row(1u).text_length);
    CLI_TEST_ASSERT_TEXT("view", row(2u).text, row(2u).text_length);
    TEST_ASSERT_EQUAL_UINT(0u, row(0u).flags & CLI_JOB_RUN_OPEN);
    TEST_ASSERT_EQUAL_INT(0, pressed(0u));
    TEST_ASSERT_EQUAL_UINT(5u, rows());
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_OPEN, row(0u).flags & CLI_JOB_RUN_OPEN);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_JOB, row(1u).type);
    CLI_TEST_ASSERT_TEXT("Alpha build", row(1u).text, row(1u).text_length);
    CLI_TEST_ASSERT_TEXT("Beta", row(2u).text, row(2u).text_length);
    (void)pressed(4u);
    CLI_TEST_ASSERT_TEXT("view/delta", row(5u).text, row(5u).text_length);
    (void)pressed(0u);
    TEST_ASSERT_EQUAL_UINT(4u, rows());
}

void test_the_filter_lets_stand_the_jobs_that_contain_it_and_the_counts_follow(void)
{
    store_read();
    (void)pressed(0u);
    typed("BETA");
    unsigned long long length = 0ULL;
    const char *const filter = EMBED_CALL(cli_job_runs_field.filter, CliJobRunsFieldCfg, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("BETA", filter, length);
    TEST_ASSERT_EQUAL_UINT(2u, rows());
    TEST_ASSERT_EQUAL_UINT64(1ULL, row(0u).number);
    CLI_TEST_ASSERT_TEXT("Beta", row(1u).text, row(1u).text_length);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_job_runs_field.key, CliJobRunsFieldCfg, .key_name = "Backspace"));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_job_runs_field.key, CliJobRunsFieldCfg, .key_name = "Tab"));
    typed("x");
    TEST_ASSERT_EQUAL_UINT(0u, rows());
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        (void)EMBED_CALL(cli_job_runs_field.key, CliJobRunsFieldCfg, .key_name = "Backspace");
    }
    TEST_ASSERT_EQUAL_UINT(5u, rows());
}

void test_a_job_chosen_lists_its_file_and_params_and_the_choices_of_the_param_focused(void)
{
    const unsigned int first = alpha_chosen();
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_CHOSEN, row(1u).flags & CLI_JOB_RUN_CHOSEN);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_ABOUT, row(2u).type);
    CLI_TEST_ASSERT_TEXT("src/alpha.sh", row(2u).text, row(2u).text_length);
    CLI_TEST_ASSERT_TEXT("builds the alpha", row(2u).second, row(2u).second_length);
    TEST_ASSERT_EQUAL_UINT(3u, first);
    CLI_TEST_ASSERT_TEXT("target", row(3u).text, row(3u).text_length);
    CLI_TEST_ASSERT_TEXT("x86", row(3u).second, row(3u).second_length);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_DEFAULTED, row(3u).flags & CLI_JOB_RUN_DEFAULTED);
    CLI_TEST_ASSERT_TEXT("name", row(5u).text, row(5u).text_length);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_REQUIRED, row(5u).flags & CLI_JOB_RUN_REQUIRED);
    TEST_ASSERT_EQUAL_UINT(rows(), row_of(CLI_JOB_RUN_CHOICE, 0u));
    TEST_ASSERT_EQUAL_INT(0, pressed(3u));
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_FOCUSED, row(3u).flags & CLI_JOB_RUN_FOCUSED);
    TEST_ASSERT_EQUAL_UINT(4u, row_of(CLI_JOB_RUN_CHOICE, 0u));
    CLI_TEST_ASSERT_TEXT("arm", row(5u).text, row(5u).text_length);
    // every row agrees with a walk of the types in order, the sections' starts sorted
    const unsigned int types[] = {CLI_JOB_RUN_GROUP, CLI_JOB_RUN_JOB, CLI_JOB_RUN_ABOUT, CLI_JOB_RUN_PARAM, CLI_JOB_RUN_CHOICE, CLI_JOB_RUN_CHOICE, CLI_JOB_RUN_PARAM, CLI_JOB_RUN_PARAM, CLI_JOB_RUN_JOB, CLI_JOB_RUN_GROUP, CLI_JOB_RUN_GROUP};
    TEST_ASSERT_EQUAL_UINT(sizeof(types) / sizeof(types[0]), rows());
    for (unsigned int at = 0u; at < rows(); at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(types[at], row(at).type);
    }
    CliJobRunRow past;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_job_runs_row.row, CliJobRunsRowCfg, .row_at = rows(), .row_out = &past));
}

void test_the_form_takes_typed_values_and_choices_and_fills_the_defaults(void)
{
    (void)alpha_chosen();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_job_runs_field.focused, CliJobRunsFieldCfg, .length_out = NULL));
    TEST_ASSERT_EQUAL_INT(1, pressed(5u));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_job_runs_field.focused, CliJobRunsFieldCfg, .length_out = NULL));
    typed("w");
    typed("\xc3\xa9x");
    CLI_TEST_ASSERT_TEXT("w\xc3\xa9x", row(5u).second, row(5u).second_length);
    (void)EMBED_CALL(cli_job_runs_field.key, CliJobRunsFieldCfg, .key_name = "Backspace");
    (void)EMBED_CALL(cli_job_runs_field.key, CliJobRunsFieldCfg, .key_name = "Backspace");
    CLI_TEST_ASSERT_TEXT("w", row(5u).second, row(5u).second_length);
    TEST_ASSERT_EQUAL_INT(0, pressed(4u));
    TEST_ASSERT_EQUAL_UINT(5u, row_of(CLI_JOB_RUN_CHOICE, 0u));
    (void)pressed(6u);
    (void)pressed(5u);
    CLI_TEST_ASSERT_TEXT("two one", row(4u).second, row(4u).second_length);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_CHOSEN, row(5u).flags & CLI_JOB_RUN_CHOSEN);
    (void)pressed(6u);
    CLI_TEST_ASSERT_TEXT("one", row(4u).second, row(4u).second_length);
    TEST_ASSERT_EQUAL_UINT(0u, row(6u).flags & CLI_JOB_RUN_CHOSEN);
    (void)pressed(3u);
    (void)pressed(5u);
    CLI_TEST_ASSERT_TEXT("arm", row(3u).second, row(3u).second_length);
    TEST_ASSERT_EQUAL_UINT(0u, row(3u).flags & CLI_JOB_RUN_DEFAULTED);
    EMBED_CALL(cli_job_runs_field.filter_focus, CliJobRunsFieldCfg, .length_out = NULL);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_job_runs_field.focused, CliJobRunsFieldCfg, .length_out = NULL));
    TEST_ASSERT_EQUAL_UINT(0u, row(3u).flags & CLI_JOB_RUN_FOCUSED);
    TEST_ASSERT_EQUAL_UINT(rows(), row_of(CLI_JOB_RUN_CHOICE, 0u));
}

void test_a_start_with_a_required_value_missing_is_refused_with_the_runners_sentence(void)
{
    (void)alpha_chosen();
    const unsigned int before = rows();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_job_runs_run.start, CliJobRunsRunCfg, .root = "."));
    unsigned long long length = 0ULL;
    const char *const refusal = EMBED_CALL(cli_job_runs_run.refusal, CliJobRunsRunCfg, .length_out = &length);
    TEST_ASSERT_GREATER_THAN_UINT64(0ULL, length);
    CLI_TEST_ASSERT_CONTAINS("name", refusal);
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    TEST_ASSERT_EQUAL_UINT(before, rows());
    TEST_ASSERT_FALSE(EMBED_CALL(cli_job_runs_run.live, CliJobRunsRunCfg, .root = "."));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_job_runs_run.stop, CliJobRunsRunCfg, .root = "."));
}

void test_what_a_run_reports_comes_in_on_the_step_its_lines_plain_with_their_spans(void)
{
    (void)alpha_chosen();
    reported(CLI_JOB_RUNS_STARTED, 901ULL, "build/alpha", NULL, NULL, 0LL, 0);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL));
    const unsigned int run = row_of(CLI_JOB_RUN_RUN, 0u);
    CLI_TEST_ASSERT_TEXT("#901 running", row(run).text, row(run).text_length);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_LIVE | CLI_JOB_RUN_SHOWN, row(run).flags & (CLI_JOB_RUN_LIVE | CLI_JOB_RUN_SHOWN));
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_LIVE, row(1u).flags & CLI_JOB_RUN_LIVE);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_job_runs_run.live, CliJobRunsRunCfg, .root = "."));
    reported(CLI_JOB_RUNS_LINE, 901ULL, NULL, "command", "bash job.sh", 0LL, 0);
    reported(CLI_JOB_RUNS_LINE, 901ULL, NULL, "stdout", "plain \x1b[31mred\x1b[0m", 0LL, 0);
    reported(CLI_JOB_RUNS_LINE, 901ULL, NULL, "stderr", "", 0LL, 0);
    reported(CLI_JOB_RUNS_ENDED, 901ULL, NULL, NULL, NULL, 0LL, 0);
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    CLI_TEST_ASSERT_TEXT("#901 exit 0", row(run).text, row(run).text_length);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_OK, row(run).flags & (CLI_JOB_RUN_OK | CLI_JOB_RUN_FAILED | CLI_JOB_RUN_LIVE));
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_COMMAND, row(run + 1u).stream);
    CliJobRunLine line;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_job_runs_row.line, CliJobRunsRowCfg, .row_at = run + 2u, .line_out = &line));
    CLI_TEST_ASSERT_TEXT("plain red", line.text, line.length);
    TEST_ASSERT_EQUAL_UINT(1u, line.spans);
    TEST_ASSERT_EQUAL_UINT(6u, line.span[0].offset);
    TEST_ASSERT_EQUAL_UINT(3u, line.span[0].length);
    TEST_ASSERT_EQUAL_UINT(1u, line.span[0].color);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_job_runs_row.line, CliJobRunsRowCfg, .row_at = run + 1u, .line_out = &line));
    TEST_ASSERT_EQUAL_UINT(0u, line.spans);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_STDERR, row(run + 3u).stream);
    TEST_ASSERT_EQUAL_UINT64(0ULL, row(run + 3u).text_length);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_END, row(run + 4u).type);
    CLI_TEST_ASSERT_TEXT("exit 0", row(run + 4u).text, row(run + 4u).text_length);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_job_runs_row.line, CliJobRunsRowCfg, .row_at = run, .line_out = &line));
    reported(CLI_JOB_RUNS_STARTED, 902ULL, "build/alpha", NULL, NULL, 0LL, 0);
    reported(CLI_JOB_RUNS_ENDED, 902ULL, NULL, NULL, NULL, 2LL, 0);
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    const unsigned int second = row_of(CLI_JOB_RUN_RUN, run + 1u);
    CLI_TEST_ASSERT_TEXT("#902 exit 2", row(second).text, row(second).text_length);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_FAILED | CLI_JOB_RUN_SHOWN, row(second).flags & (CLI_JOB_RUN_FAILED | CLI_JOB_RUN_SHOWN));
    TEST_ASSERT_EQUAL_UINT(run + 1u, second);
    TEST_ASSERT_EQUAL_INT(0, pressed(run));
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_LINE, row(run + 1u).type);
}

void test_a_run_past_the_lines_bound_lets_its_oldest_lines_go(void)
{
    (void)alpha_chosen();
    reported(CLI_JOB_RUNS_STARTED, 903ULL, "build/alpha", NULL, NULL, 0LL, 0);
    char text[16];
    for (unsigned int at = 0u; at < CLI_LINES_MAX + 10u; at += 1u)
    {
        VerbaLinea line = {text, sizeof(text), 0ULL};
        mmgr_line_decimal(&line, at, 1u);
        (void)mmgr_line_finish(&line);
        reported(CLI_JOB_RUNS_LINE, 903ULL, NULL, "stdout", text, 0LL, 0);
    }
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    const unsigned int run = row_of(CLI_JOB_RUN_RUN, 0u);
    TEST_ASSERT_EQUAL_UINT(run + 1u + CLI_LINES_MAX, row_of(CLI_JOB_RUN_JOB, run + 1u));
    CLI_TEST_ASSERT_TEXT("10", row(run + 1u).text, row(run + 1u).text_length);
}

void test_the_ended_runs_of_a_job_past_the_bound_let_the_oldest_go_and_a_live_one_stands(void)
{
    (void)alpha_chosen();
    reported(CLI_JOB_RUNS_STARTED, 700ULL, "build/alpha", NULL, NULL, 0LL, 0);
    for (unsigned long long run = 1ULL; run <= CLI_JOB_RUNS_RETAINED + 2u; run += 1ULL)
    {
        reported(CLI_JOB_RUNS_STARTED, run, "build/alpha", NULL, NULL, 0LL, 0);
        reported(CLI_JOB_RUNS_ENDED, run, NULL, NULL, NULL, 0LL, 0);
    }
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    unsigned int runs = 0u;
    for (unsigned int at = 0u; at < rows(); at += 1u)
    {
        runs += (row(at).type == CLI_JOB_RUN_RUN) ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUNS_RETAINED + 1u, runs);
    const unsigned int first = row_of(CLI_JOB_RUN_RUN, 0u);
    CLI_TEST_ASSERT_TEXT("#700 running", row(first).text, row(first).text_length);
    CLI_TEST_ASSERT_TEXT("#3 exit 0", row(first + 1u).text, row(first + 1u).text_length);
}

void test_follow_brings_the_last_row_into_view_where_the_pane_stood_at_it(void)
{
    (void)alpha_chosen();
    reported(CLI_JOB_RUNS_STARTED, 904ULL, "build/alpha", NULL, NULL, 0LL, 0);
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    const unsigned int before = rows();
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        reported(CLI_JOB_RUNS_LINE, 904ULL, NULL, "stdout", "line", 0LL, 0);
    }
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    TEST_ASSERT_EQUAL_UINT(before + 20u, rows());
    TEST_ASSERT_EQUAL_UINT(rows() - 5u, EMBED_CALL(cli_job_runs_scroll.follow, CliJobRunsScrollCfg, .scroll_top = before - 5u, .capacity = 5u));
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_job_runs_scroll.follow, CliJobRunsScrollCfg, .scroll_top = 0u, .capacity = 5u));
}

void test_a_real_run_starts_reads_its_lines_and_ends(void)
{
    cli_unity_folder("job_runs_tree", s_root);
    cli_unity_file(s_root, "job.sh", "echo hello\nprintf '\\033[32mgreen\\033[0m\\n'\nexit 3\n");
    catalog_laid("job.sh");
    EMBED_CALL(cli_job_runs_root.read, CliJobRunsRootCfg, .root = s_root, .catalog = &s_catalog);
    (void)pressed(0u);
    (void)pressed(1u);
    (void)pressed(5u);
    typed("someone");
    if (!EMBED_CALL(cli_job_runs_run.start, CliJobRunsRunCfg, .root = s_root))
    {
        unsigned long long length = 0ULL;
        TEST_IGNORE_MESSAGE(EMBED_CALL(cli_job_runs_run.refusal, CliJobRunsRunCfg, .length_out = &length));
    }
    TEST_ASSERT_TRUE(run_ended());
    const unsigned int run = row_of(CLI_JOB_RUN_RUN, 0u);
    CLI_TEST_ASSERT_CONTAINS("exit 3", row(run).text);
    TEST_ASSERT_EQUAL_UINT(CLI_JOB_RUN_FAILED, row(run).flags & CLI_JOB_RUN_FAILED);
    int hello = 0;
    int green = 0;
    for (unsigned int at = run + 1u; at < rows(); at += 1u)
    {
        CliJobRunLine line;
        if (!EMBED_CALL(cli_job_runs_row.line, CliJobRunsRowCfg, .row_at = at, .line_out = &line))
        {
            continue;
        }
        hello = hello || ((line.length == 5ULL) && (memor.cmp(&(MemoriaCfg){.src = line.text, .other = "hello", .bytes = 5ULL}) == 0));
        green = green || ((line.length == 5ULL) && (line.spans == 1u) && (line.span[0].color == 2u));
    }
    TEST_ASSERT_TRUE(hello);
    TEST_ASSERT_TRUE(green);
}

void test_a_live_run_stopped_ends_stopped(void)
{
    cli_unity_folder("job_runs_stop", s_root);
    cli_unity_file(s_root, "job.sh", "echo waiting\nsleep 30\n");
    catalog_laid("job.sh");
    EMBED_CALL(cli_job_runs_root.read, CliJobRunsRootCfg, .root = s_root, .catalog = &s_catalog);
    (void)pressed(0u);
    (void)pressed(1u);
    (void)pressed(5u);
    typed("someone");
    if (!EMBED_CALL(cli_job_runs_run.start, CliJobRunsRunCfg, .root = s_root))
    {
        unsigned long long length = 0ULL;
        TEST_IGNORE_MESSAGE(EMBED_CALL(cli_job_runs_run.refusal, CliJobRunsRunCfg, .length_out = &length));
    }
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_job_runs_run.live, CliJobRunsRunCfg, .root = s_root));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_job_runs_run.stop, CliJobRunsRunCfg, .root = s_root));
    TEST_ASSERT_TRUE(run_ended());
    const unsigned int run = row_of(CLI_JOB_RUN_RUN, 0u);
    CLI_TEST_ASSERT_CONTAINS("stopped", row(run).text);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_job_runs_run.stop, CliJobRunsRunCfg, .root = s_root));
}

void test_every_cell_goes_as_the_catalog_is_read_again_and_as_the_store_ends(void)
{
    (void)alpha_chosen();
    (void)pressed(5u);
    typed("someone");
    reported(CLI_JOB_RUNS_STARTED, 905ULL, "build/alpha", NULL, NULL, 0LL, 0);
    reported(CLI_JOB_RUNS_LINE, 905ULL, NULL, "stdout", "\x1b[1mbold\x1b[0m", 0LL, 0);
    (void)EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL);
    EMBED_CALL(cli_job_runs_root.read, CliJobRunsRootCfg, .root = NULL, .catalog = NULL);
    TEST_ASSERT_EQUAL_UINT(0u, rows());
    EMBED_CALL(cli_job_runs_woken.end, CliJobRunsWokenCfg, .woken = NULL);
    TEST_ASSERT_EQUAL_UINT64(0ULL, mmgr_owner_in_use(g_cli_job_runs_owner));
}
