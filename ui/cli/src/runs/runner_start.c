// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_start.c: a run started on a thread of its own, waited on, and stopped with all it started
#include "../command_line/cells.h"
#include "runner_internal.h"

// the next part of a cell, `bytes` long, taken from `*at`, its start kept on a word's boundary: its start
static void *run_part(unsigned char **at, unsigned long long bytes)
{
    void *const part = *at;
    *at += (bytes + 7ull) & ~7ull;
    return part;
}

int cli_run_cells_taken(CliRun *run, const CliCatalog *catalog, unsigned int job, CliLine *said)
{
    CliCatalogExtent extent;
    cli_catalog_job_extent(catalog, job, &extent);
    if ((extent.steps > CLI_RUN_STEPS) || (extent.text >= 0xffffffffull))
    {
        cli_line_text(said, "the job holds more than a run keeps");
        return 0;
    }
    const unsigned long long bytes =
        sizeof(CliJob) + ((unsigned long long)extent.params * sizeof(CliParam)) +
        ((unsigned long long)extent.steps * sizeof(CliStep)) + ((unsigned long long)extent.args * sizeof(CliArg)) +
        ((unsigned long long)extent.lists * sizeof(unsigned int)) + extent.text + sizeof(CliValues) + 64ull;
    unsigned char *const cell = cli_cell_taken(bytes, "a run's job");
    CliLines *const lines = (cell != NULL) ? cli_cell_taken(sizeof(CliLines), "a run's lines") : NULL;
    if (lines == NULL)
    {
        cli_cell_given(cell);
        cli_line_text(said, "a run's cells could not be taken");
        return 0;
    }
    unsigned char *at = cell;
    run->values = run_part(&at, sizeof(CliValues));
    CliJob *const jobs = run_part(&at, sizeof(CliJob));
    CliParam *const params = run_part(&at, (unsigned long long)extent.params * sizeof(CliParam));
    CliStep *const steps = run_part(&at, (unsigned long long)extent.steps * sizeof(CliStep));
    CliArg *const args = run_part(&at, (unsigned long long)extent.args * sizeof(CliArg));
    unsigned int *const lists = run_part(&at, (unsigned long long)extent.lists * sizeof(unsigned int));
    char *const text = run_part(&at, extent.text);
    // the job's text is below an unsigned int's reach, as checked above
    cli_catalog_begun(&run->catalog, jobs, 1u, params, extent.params, steps, extent.steps, args, extent.args, lists,
                      extent.lists, text, (unsigned int)extent.text);
    if (cli_catalog_copy(catalog, job, &run->catalog) == CLI_CATALOG_NONE)
    {
        cli_cell_given(lines);
        cli_cell_given(cell);
        cli_line_text(said, "the job holds more than a run keeps");
        return 0;
    }
    run->job_cell = cell;
    run->lines = lines;
    return 1;
}

void cli_run_cells_given(CliRun *run)
{
    cli_cell_given(run->job_cell);
    cli_cell_given(run->lines);
    run->job_cell = NULL;
    run->lines = NULL;
}

int cli_runs_start(CliRunSink sink, const char *root, const CliCatalog *catalog, unsigned int job,
                   const CliValues *values, unsigned long long *run_number, CliLine *said)
{
    static CliCommand s_checked;
    static char s_shown[CLI_COMMAND_TEXT];
    if (!cli_runner_check(catalog, job, values, said))
    {
        return 0;
    }
    cli_lock_take(&g_cli_runs_lock);
    // every step that runs made once now, so a step that cannot be made refuses the run before it starts
    const CliJob *const held = &catalog->job[job];
    for (unsigned int at = 0u; at < held->steps; at += 1u)
    {
        const CliStep *const step = &catalog->step[held->step_first + at];
        CliLine shown = {s_shown, sizeof(s_shown), 0ull};
        if ((step->program != CLI_PROGRAM_MADE) && cli_runner_runs(catalog, step, values) &&
            !cli_runner_command(root, catalog, step, values, &s_checked, &shown, said))
        {
            cli_lock_give(&g_cli_runs_lock);
            return 0;
        }
    }
    CliRun *run = NULL;
    for (unsigned int at = 0u; (run == NULL) && (at < CLI_RUNS_MOST); at += 1u)
    {
        run = (!g_cli_runs[at].used || g_cli_runs[at].ended) ? &g_cli_runs[at] : NULL;
    }
    if (run == NULL)
    {
        cli_lock_give(&g_cli_runs_lock);
        cli_line_text(said, "more runs go at once than orior_cli holds");
        return 0;
    }
    if (!cli_run_cells_taken(run, catalog, job, said))
    {
        cli_lock_give(&g_cli_runs_lock);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = run->values, .src = values, .bytes = sizeof(*values)});
    memor.cpy(&(MemoriaCfg){.dst = run->root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    run->sink = sink;
    g_cli_next_run += 1ull;
    run->run = g_cli_next_run;
    run->used = 1;
    run->ended = 0;
    run->stopped = 0;
    run->process = 0ull;
    run->group = 0ull;
    run->lines->count = 0u;
    run->lines->used = 0u;
    run->has_first = 0;
    run->began = cli_clock_milliseconds();
    cli_lock_give(&g_cli_runs_lock);
    // the run's start said before any line of it, so that a hearer knows the run and its job first
    const CliSaid start = {CLI_SAID_START,
                           run->run,
                           cli_catalog_at(&run->catalog, run->catalog.job[0].id),
                           NULL,
                           NULL,
                           0,
                           0ll,
                           0,
                           NULL,
                           0u,
                           NULL,
                           0.0};
    sink.said(sink.context, &start);
    *run_number = run->run;
    run->thread.run = cli_run_steps;
    run->thread.context = run;
    if (!cli_thread_started(&run->thread))
    {
        cli_run_steps(run);
    }
    return 1;
}

void cli_runs_wait(unsigned long long run_number)
{
    cli_lock_take(&g_cli_runs_lock);
    for (;;)
    {
        int going = 0;
        for (unsigned int at = 0u; at < CLI_RUNS_MOST; at += 1u)
        {
            going = going || (g_cli_runs[at].used && (g_cli_runs[at].run == run_number) && !g_cli_runs[at].ended);
        }
        if (!going)
        {
            break;
        }
        cli_wake_wait(&g_cli_runs_wake, &g_cli_runs_lock);
    }
    cli_lock_give(&g_cli_runs_lock);
}

int cli_runs_stop(unsigned long long run_number, CliLine *said)
{
    (void)said;
    cli_lock_take(&g_cli_runs_lock);
    unsigned long long process = 0ull;
    unsigned long long group = 0ull;
    for (unsigned int at = 0u; at < CLI_RUNS_MOST; at += 1u)
    {
        if (g_cli_runs[at].used && (g_cli_runs[at].run == run_number))
        {
            g_cli_runs[at].stopped = 1;
            process = g_cli_runs[at].process;
            group = g_cli_runs[at].group;
        }
    }
    cli_spawn_group_end(group);
    cli_lock_give(&g_cli_runs_lock);
    if (process != 0ull)
    {
        cli_spawn_tree_end(process);
    }
    return 1;
}
