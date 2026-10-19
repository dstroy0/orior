// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_test.c: the runner's check: stop ends a build and every process it started at once, a setting given is set in
// the step's environment and leads its line, a variable set when a param holds a value is set then alone, and words
// split as a shell splits them
#include "../../../../../ui/cli/src/command_line/catalog.h"
#include "../../../../../ui/cli/src/runs/runner.h"
#include "cli_check.h"

static CliJob s_jobs[256];
static CliParam s_params[1024];
static CliStep s_steps[256];
static CliArg s_args[2048];
static unsigned int s_lists[4096];
static char s_text[1u << 18u];
static CliCatalog s_catalog;
static CliValues s_values;
static CliCommand s_command;

// the catalog made empty
static CliCatalog *runner_catalog(void)
{
    cli_catalog_begun(&s_catalog, s_jobs, 256u, s_params, 1024u, s_steps, 256u, s_args, 2048u, s_lists, 4096u, s_text,
                      sizeof(s_text));
    return &s_catalog;
}

// a run's end heard
static void runner_said(void *context, const CliSaid *said)
{
    (void)context;
    (void)said;
}

// stop ends a build and every process it started at once
static void runner_stop(void)
{
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    (void)cli_container_held();
    cli_check_folder("runner_stop", root);
    cli_full(root, "src", path);
    (void)query_folder_made(path);
    cli_full(root, "src/build_slow.sh", path);
    (void)query_file_written(path, "#!/usr/bin/env bash\nsleep 30\necho late\n", 39ull);
    cli_catalog_read(root, runner_catalog());
    const unsigned int job = cli_catalog_find(&s_catalog, "build/src/build_slow.sh");
    cli_check(job != CLI_CATALOG_NONE, "the build reads as a job");
    if (job == CLI_CATALOG_NONE)
    {
        return;
    }
    cli_values_begun(&s_values);
    cli_values_put(&s_values, "arguments", "");
    cli_values_put(&s_values, "trace", "quiet");
    unsigned long long run = 0ull;
    const CliRunSink sink = {runner_said, NULL};
    cli_check(cli_runs_start(sink, root, &s_catalog, job, &s_values, &run, &said), "the build starts");
    query_paused(1500u);
    const unsigned long long asked = cli_clock_milliseconds();
    (void)cli_runs_stop(run, &said);
    cli_runs_wait(run);
    cli_check(cli_clock_milliseconds() - asked < 5000ull, "the run ends at once after Stop");
}

// the command the one step of the catalog's last job runs with the values, its line into `shown`, which holds 256
static void runner_command(char *shown)
{
    CliLine line = {shown, 256u, 0ull};
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_runner_command(".", &s_catalog, &s_catalog.step[0], &s_values, &s_command, &line, &said),
              "the step's command is made");
    (void)cli_line_closed(&line);
}

// whether the command sets only `name` to `value`, or nothing where `name` is NULL
static int runner_sets(const char *name, const char *value)
{
    if (name == NULL)
    {
        return s_command.settings == 0u;
    }
    return (s_command.settings == 1u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = s_command.setting[0].name, .other = name, .cap = CLI_CELLS_POOL})) &&
           (s_command.setting[0].value != NULL) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = s_command.setting[0].value, .other = value, .cap = CLI_CELLS_POOL}));
}

// a setting given is set in the step's environment and leads its line; a variable set when a param holds a value is
// set then alone
static void runner_settings(void)
{
    char shown[256];
    (void)cli_catalog_job(runner_catalog(), "run", "build", "run", "run.sh", "", "");
    cli_catalog_step(&s_catalog, CLI_PROGRAM_BASH, "run.sh", NULL, 0);
    cli_catalog_arg(&s_catalog, CLI_ARG_VALUE, "mode", NULL, NULL, NULL, 0);
    cli_catalog_arg(&s_catalog, CLI_ARG_ENV, "KLQ_SEED", NULL, NULL, NULL, 0);
    cli_catalog_arg(&s_catalog, CLI_ARG_ENV, "KLQ_TRACE", NULL, NULL, NULL, 0);
    cli_values_begun(&s_values);
    cli_values_put(&s_values, "mode", "pair");
    cli_values_put(&s_values, "KLQ_SEED", "2");
    runner_command(shown);
    cli_check(runner_sets("KLQ_SEED", "2"), "a setting given is set in the step's environment");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = shown, .other = "KLQ_SEED=2 bash run.sh pair", .cap = CLI_CELLS_POOL}),
        "and leads its line");
    (void)cli_catalog_job(runner_catalog(), "build", "build", "build", "build.sh", "", "");
    cli_catalog_step(&s_catalog, CLI_PROGRAM_BASH, "build.sh", NULL, 0);
    cli_catalog_arg(&s_catalog, CLI_ARG_SET_WHEN, "trace", "verbose", "SHELLOPTS", "xtrace", 0);
    cli_values_begun(&s_values);
    cli_values_put(&s_values, "trace", "verbose");
    runner_command(shown);
    cli_check(runner_sets("SHELLOPTS", "xtrace") &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = shown, .other = "SHELLOPTS=xtrace bash build.sh", .cap = CLI_CELLS_POOL})),
              "a variable is set when its param holds its value");
    cli_values_begun(&s_values);
    cli_values_put(&s_values, "trace", "quiet");
    runner_command(shown);
    cli_check(runner_sets(NULL, NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = shown, .other = "bash build.sh", .cap = CLI_CELLS_POOL})),
              "and then alone");
}

// the words of `text` split, `|` between them, into `out`, which holds 256: 1, or 0 where they do not split
static int runner_split(const char *text, char *out)
{
    static CliWords s_words;
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_runner_shell_words(text, &s_words, &said))
    {
        return 0;
    }
    CliLine line = {out, 256u, 0ull};
    for (unsigned int at = 0u; at < s_words.count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, s_words.word[at]);
    }
    (void)cli_line_closed(&line);
    return (s_words.count > 0u) ? 1 : -1;
}

int main(void)
{
    char bash[CLI_PATH_LONGEST];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    if (cli_runner_bash(bash, &said))
    {
        runner_stop();
        runner_settings();
    }
    else
    {
        cli_check(1, "the machine has no bash, and no step runs");
    }
    char words[256];
    cli_check((runner_split("a  'b c' \"d \\\"e\\\"\"", words) == 1) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = words, .other = "a|b c|d \"e\"", .cap = CLI_CELLS_POOL})),
              "words split on quotes and escapes");
    cli_check(runner_split("", words) == -1, "nothing is no words");
    cli_check((runner_split("''", words) == 1) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = words, .other = "", .cap = CLI_CELLS_POOL})),
              "an empty quote is one empty word");
    cli_check(runner_split("'open", words) == 0, "and a quote left open does not split");
    return cli_checked();
}
