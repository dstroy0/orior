// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_runner.c: stop ends a build and every process it started at once, a setting is set in the step's environment and leads its line,
// a variable set when a param has a value is set only then, and words split as a shell splits them
#include "ui/cli/src/command_line/catalog.h"
#include "ui/cli/src/runs/runner.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliJob s_jobs[256];
static CliParam s_params[1024];
static CliStep s_steps[256];
static CliArg s_args[2048];
static unsigned int s_lists[4096];
static char s_text[1u << 18u];
static CliCatalog s_catalog;
static CliValues s_values;
static CliCommand s_command;
static char s_shown[256];
static char s_words_text[256];

// the catalog emptied
static CliCatalog *catalog_emptied(void)
{
    EMBED_CALL(cli_catalog_jobs_most.begin, CliCatalogJobsMostCfg, .catalog = &s_catalog, .jobs = s_jobs, .jobs_most = 256u, .params = s_params, .params_most = 1024u, .steps = s_steps, .steps_most = 256u, .request = s_args, .args_most = 2048u, .lists = s_lists, .lists_most = 4096u, .text_out = s_text, .text_most = sizeof(s_text));
    return &s_catalog;
}

// a run's end ignored
static void run_ended(const CliEachEventCfg *args)
{
    (void)args;
}

// the test ignored where the machine has no bash
static void bash_required(void)
{
    char bash[CLI_PATH_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = bash, .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("the machine has no bash");
    }
}

// the command of the catalog's first step with the values, its line into s_shown
static void command_made(void)
{
    VerbaLinea line = {s_shown, sizeof(s_shown), 0ull};
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_runner_command_table.command, CliRunnerCommandCfg, .root = ".", .catalog = &s_catalog, .step = &s_catalog.step[0], .values = &s_values, .command = &s_command, .shown = &line, .err_msg = &err_msg));
    (void)mmgr_line_finish(&line);
}

// a job of a step with a value and two settings, the first given
static void setting_job_made(void)
{
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = catalog_emptied(), .id = "run", .group = "build", .title = "run", .file = "run.sh", .about = "", .opens = "");
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = &s_catalog, .program = CLI_PROGRAM_BASH, .text = "run.sh", .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = &s_catalog, .kind_u32 = CLI_ARG_VALUE, .a = "mode", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = &s_catalog, .kind_u32 = CLI_ARG_ENV, .a = "KLQ_SEED", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = &s_catalog, .kind_u32 = CLI_ARG_ENV, .a = "KLQ_TRACE", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = &s_values);
    EMBED_CALL(cli_catalog_key_values.values_put, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = "mode", .value = "pair");
    EMBED_CALL(cli_catalog_key_values.values_put, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = "KLQ_SEED", .value = "2");
}

// a job of a step whose variable is set when the param `trace` is verbose, the param set to `trace`
static void conditional_job_made(const char *trace)
{
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = catalog_emptied(), .id = "build", .group = "build", .title = "build", .file = "build.sh", .about = "", .opens = "");
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = &s_catalog, .program = CLI_PROGRAM_BASH, .text = "build.sh", .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = &s_catalog, .kind_u32 = CLI_ARG_SET_WHEN, .a = "trace", .b = "verbose", .c = "SHELLOPTS", .d = "xtrace", .separated = 0);
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = &s_values);
    EMBED_CALL(cli_catalog_key_values.values_put, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = "trace", .value = trace);
}

// the words of `text` split, `|` between them, into s_words_text: 1, 0 where they do not split, -1 where there are none
static int words_split(const char *text)
{
    static CliWords s_words;
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_runner_text.shell_words, CliRunnerTextCfg, .text = text, .words = &s_words, .err_msg = &err_msg))
    {
        return 0;
    }
    VerbaLinea line = {s_words_text, sizeof(s_words_text), 0ull};
    for (unsigned int at = 0u; at < s_words.count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, s_words.word[at]);
    }
    (void)mmgr_line_finish(&line);
    return (s_words.count > 0u) ? 1 : -1;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_stop_ends_a_build_at_once(void)
{
    bash_required();
    char root[CLI_PATH_MAX_LEN];
    char src[CLI_PATH_MAX_LEN];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)cli_container_init();
    cli_unity_folder("runner_stop", root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "src", .joined = src);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = src));
    cli_unity_file(root, "src/build_slow.sh", "#!/usr/bin/env bash\nsleep 30\necho late\n");
    EMBED_CALL(cli_catalog_root.read, CliCatalogRootCfg, .root = root, .catalog = catalog_emptied());
    const unsigned int job = EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = &s_catalog, .id = "build/src/build_slow.sh");
    TEST_ASSERT_NOT_EQUAL(CLI_CATALOG_NONE, job);
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = &s_values);
    EMBED_CALL(cli_catalog_key_values.values_put, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = "arguments", .value = "");
    EMBED_CALL(cli_catalog_key_values.values_put, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = "trace", .value = "quiet");
    unsigned long long run = 0ull;
    const CliRunSink sink = {run_ended, NULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_runner_err_msg.runs_start, CliRunnerErrMsgCfg, .sink = sink, .root = root, .catalog = &s_catalog, .job = job, .values = &s_values, .run_number_pointer = &run, .err_msg = &err_msg));
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 1500u);
    const unsigned long long stop_ms = cli_clock_milliseconds();
    (void)EMBED_CALL(cli_runner_err_msg_run_number.runs_stop, CliRunnerErrMsgRunNumberCfg, .run_number = run, .err_msg = &err_msg);
    EMBED_CALL(cli_runner_err_msg_run_number.runs_wait, CliRunnerErrMsgRunNumberCfg, .run_number = run);
    TEST_ASSERT_LESS_THAN_UINT64(5000ull, cli_clock_milliseconds() - stop_ms);
}

void test_a_setting_is_set_in_the_steps_environment(void)
{
    bash_required();
    setting_job_made();
    command_made();
    TEST_ASSERT_EQUAL_UINT(1u, s_command.settings);
    TEST_ASSERT_EQUAL_STRING("KLQ_SEED", s_command.setting[0].name);
    TEST_ASSERT_EQUAL_STRING("2", s_command.setting[0].value);
}

void test_a_setting_leads_the_steps_line(void)
{
    bash_required();
    setting_job_made();
    command_made();
    TEST_ASSERT_EQUAL_STRING("KLQ_SEED=2 bash run.sh pair", s_shown);
}

void test_a_variable_is_set_when_its_param_has_its_value(void)
{
    bash_required();
    conditional_job_made("verbose");
    command_made();
    TEST_ASSERT_EQUAL_UINT(1u, s_command.settings);
    TEST_ASSERT_EQUAL_STRING("SHELLOPTS", s_command.setting[0].name);
    TEST_ASSERT_EQUAL_STRING("xtrace", s_command.setting[0].value);
    TEST_ASSERT_EQUAL_STRING("SHELLOPTS=xtrace bash build.sh", s_shown);
}

void test_a_variable_is_not_set_for_another_value(void)
{
    bash_required();
    conditional_job_made("quiet");
    command_made();
    TEST_ASSERT_EQUAL_UINT(0u, s_command.settings);
    TEST_ASSERT_EQUAL_STRING("bash build.sh", s_shown);
}

void test_words_split_on_quotes_and_escapes(void)
{
    TEST_ASSERT_EQUAL_INT(1, words_split("a  'b c' \"d \\\"e\\\"\""));
    TEST_ASSERT_EQUAL_STRING("a|b c|d \"e\"", s_words_text);
}

void test_an_empty_text_has_no_words(void)
{
    TEST_ASSERT_EQUAL_INT(-1, words_split(""));
}

void test_an_empty_quote_is_one_empty_word(void)
{
    TEST_ASSERT_EQUAL_INT(1, words_split("''"));
    TEST_ASSERT_EQUAL_STRING("", s_words_text);
}

void test_a_quote_left_open_does_not_split(void)
{
    TEST_ASSERT_EQUAL_INT(0, words_split("'open"));
}
