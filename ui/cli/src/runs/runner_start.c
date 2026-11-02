// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_start.c: a run started on a thread of its own, waited on, and stopped with all it started
#include "../command_line/cells.h"
#include "runner_internal.h"

typedef struct
{
    unsigned char **at;
    unsigned long long bytes;
} RunnerStartCtx;

// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
EMBED_INLINE void *run_part(const RunnerStartCtx *args)
{
    unsigned char **at = args->at;
    void *const part = *at;
    *at += (args->bytes + 7ull) & ~7ull;
    return part;
}

int cli_run_cells_take(const CliRunnerRunCtx *args)
{
    CliCatalogExtent extent;
    EMBED_CALL(cli_catalog_from_index.job_extent, CliCatalogFromIndexCfg, .from = args->catalog, .index = args->job,
               .extent = &extent);
    if ((extent.steps > CLI_RUN_STEPS) || (extent.text >= 0xffffffffull))
    {
        mmgr_line_put(args->err_msg, "the job contains more than a run stores");
        return 0;
    }
    const unsigned long long bytes =
        sizeof(CliJob) + ((unsigned long long)extent.params * sizeof(CliParam)) +
        ((unsigned long long)extent.steps * sizeof(CliStep)) + ((unsigned long long)extent.args * sizeof(CliArg)) +
        ((unsigned long long)extent.lists * sizeof(unsigned int)) + extent.text + sizeof(CliValues) + 64ull;
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_runs_cells);
    CliLines *const lines =
        (cell != NULL) ? g_cli_cells->persistent_buf_alloc(sizeof(CliLines), &g_cli_runs_cells) : NULL;
    if (lines == NULL)
    {
        g_cli_cells->persistent_buf_release(cell, &g_cli_runs_cells);
        mmgr_line_put(args->err_msg, "a run's cells could not be taken");
        return 0;
    }
    unsigned char *at = cell;
    args->run->values = EMBED_CALL(run_part, RunnerStartCtx, .at = &at, .bytes = sizeof(CliValues));
    CliJob *const jobs = EMBED_CALL(run_part, RunnerStartCtx, .at = &at, .bytes = sizeof(CliJob));
    CliParam *const params =
        EMBED_CALL(run_part, RunnerStartCtx, .at = &at, .bytes = (unsigned long long)extent.params * sizeof(CliParam));
    CliStep *const steps =
        EMBED_CALL(run_part, RunnerStartCtx, .at = &at, .bytes = (unsigned long long)extent.steps * sizeof(CliStep));
    CliArg *const arguments =
        EMBED_CALL(run_part, RunnerStartCtx, .at = &at, .bytes = (unsigned long long)extent.args * sizeof(CliArg));
    unsigned int *const lists = EMBED_CALL(run_part, RunnerStartCtx, .at = &at,
                                           .bytes = (unsigned long long)extent.lists * sizeof(unsigned int));
    char *const text = EMBED_CALL(run_part, RunnerStartCtx, .at = &at, .bytes = extent.text);
    // the job's text is below an unsigned int's reach, as checked above
    EMBED_CALL(cli_catalog_jobs_most.begin, CliCatalogJobsMostCfg, .catalog = &args->run->catalog, .jobs = jobs,
               .jobs_most = 1u, .params = params, .params_most = extent.params, .steps = steps,
               .steps_most = extent.steps, .request = arguments, .args_most = extent.args, .lists = lists,
               .lists_most = extent.lists, .text_out = text, .text_most = (unsigned int)extent.text);
    if (EMBED_CALL(cli_catalog_from.copy, CliCatalogFromCfg, .from = args->catalog, .index = args->job,
                   .to = &args->run->catalog) == CLI_CATALOG_NONE)
    {
        g_cli_cells->persistent_buf_release(lines, &g_cli_runs_cells);
        g_cli_cells->persistent_buf_release(cell, &g_cli_runs_cells);
        mmgr_line_put(args->err_msg, "the job contains more than a run stores");
        return 0;
    }
    args->run->job_cell = cell;
    args->run->lines = lines;
    return 1;
}

void cli_run_cells_free(const CliRunnerRunSpawnedCtx *args)
{
    g_cli_cells->persistent_buf_release(args->run->job_cell, &g_cli_runs_cells);
    g_cli_cells->persistent_buf_release(args->run->lines, &g_cli_runs_cells);
    args->run->job_cell = NULL;
    args->run->lines = NULL;
}

int cli_runs_start(const CliRunnerErrMsgCfg *args)
{
    CliRunSink sink = args->sink;
    unsigned long long *run_number = args->run_number_pointer;
    static CliCommand s_checked;
    static char s_shown[CLI_COMMAND_TEXT];
    if (!cli_runner_err_msg.check(args))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_runs_lock);
    // every step that runs made once now: a step that cannot be made refuses the run before it starts
    const CliJob *const job_at = &args->catalog->job[args->job];
    for (unsigned int at = 0u; at < job_at->steps; at += 1u)
    {
        const CliStep *const step = &args->catalog->step[job_at->step_first + at];
        VerbaLinea shown = {s_shown, sizeof(s_shown), 0ull};
        if ((step->program != CLI_PROGRAM_MADE) &&
            EMBED_CALL(cli_runner_command_table.runs, CliRunnerCommandCfg, .catalog = args->catalog, .step = step,
                       .values = args->values) &&
            !EMBED_CALL(cli_runner_command_table.command, CliRunnerCommandCfg, .root = args->root,
                        .catalog = args->catalog, .step = step, .values = args->values, .command = &s_checked,
                        .shown = &shown, .err_msg = args->err_msg))
        {
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
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
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
        mmgr_line_put(args->err_msg, "more runs go at once than orior_cli supports");
        return 0;
    }
    if (!EMBED_CALL(cli_runner_run_internal.run_cells_take, CliRunnerRunCtx, .run = run, .catalog = args->catalog,
                    .job = args->job, .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = run->values, .src = args->values, .bytes = sizeof(*args->values)});
    memor.cpy(&(MemoriaCfg){.dst = run->root,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
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
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
    // the run's start reported before any line of it, so that a reader knows the run and its job first
    const CliRunEvent start = {CLI_RUN_EVENT_START,
                               run->run,
                               EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &run->catalog,
                                          .text_u32 = run->catalog.job[0].id),
                               NULL,
                               NULL,
                               0,
                               0ll,
                               0,
                               NULL,
                               0u,
                               NULL,
                               0.0};
    EMBED_CALL(sink.emit, CliEachEventCfg, .context = sink.context, .event = &start);
    *run_number = run->run;
    run->thread.run = cli_run_steps;
    run->thread.context = run;
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &run->thread))
    {
        cli_run_steps(run);
    }
    return 1;
}

void cli_runs_wait(const CliRunnerErrMsgRunNumberCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_runs_lock);
    for (;;)
    {
        int going = 0;
        for (unsigned int at = 0u; at < CLI_RUNS_MOST; at += 1u)
        {
            going = going || (g_cli_runs[at].used && (g_cli_runs[at].run == args->run_number) && !g_cli_runs[at].ended);
        }
        if (!going)
        {
            break;
        }
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &g_cli_runs_wake, .lock = &g_cli_runs_lock);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
}

int cli_runs_stop(const CliRunnerErrMsgRunNumberCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_runs_lock);
    unsigned long long process = 0ull;
    unsigned long long group = 0ull;
    for (unsigned int at = 0u; at < CLI_RUNS_MOST; at += 1u)
    {
        if (g_cli_runs[at].used && (g_cli_runs[at].run == args->run_number))
        {
            g_cli_runs[at].stopped = 1;
            process = g_cli_runs[at].process;
            group = g_cli_runs[at].group;
        }
    }
    EMBED_CALL(cli_group_internal.spawn_group_end, CliGroupCtx, .group = group);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
    if (process != 0ull)
    {
        EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = process);
    }
    return 1;
}
