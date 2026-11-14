// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_start.c: a run started on a thread of its own, waited on, and stopped with all it started
#include "../command_line/pool.h"
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
    *at += (args->bytes + 7ULL) & ~7ULL;
    return part;
}

int cli_run_cells_alloc(const CliRunnerRunCtx *args)
{
    CliCatalogExtent extent;
    EMBED_CALL(cli_catalog_from_index.job_extent, CliCatalogFromIndexCfg, .from = args->catalog, .index = args->job,
               .extent = &extent);
    if ((extent.steps > CLI_RUN_STEPS) || (extent.text >= 0xffffffffULL))
    {
        mmgr_line_put(args->err_msg, "the job contains more than a run stores");
        return 0;
    }
    const unsigned long long bytes =
        sizeof(CliJob) + ((unsigned long long)extent.params * sizeof(CliParam)) +
        ((unsigned long long)extent.steps * sizeof(CliStep)) + ((unsigned long long)extent.args * sizeof(CliArg)) +
        ((unsigned long long)extent.lists * sizeof(unsigned int)) + extent.text + sizeof(CliValues) + 64ULL;
    // the job laid in a cell of this call, then written into the run's movable cell
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(bytes, g_cli_runs_owner);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "a run's cells could not be taken");
        return 0;
    }
    // the run's lines are rings over the run place's span of the rings block, where they punch no hole in the
    // persistent tier; emptied for this run
    static CliRunLines s_lines[CLI_RUNS_MAX];
    const unsigned long long place = (unsigned long long)(args->run - g_cli_runs);
    CliRunLines *const lines = &s_lines[place];
    memor.set(&(MemoriaCfg){.dst = lines, .val = 0, .bytes = sizeof(*lines)});
    unsigned char *const span =
        EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_RUNNER_LINES) + (place * CLI_RUN_LINES_SET_BYTES);
    lines->text_bytes = span + CLI_RUN_LINES_TEXT_AT;
    lines->line_bytes = (CliRunnerLine *)(void *)(span + CLI_RUN_LINES_LINE_AT);
    unsigned char *at = cell;
    CliValues *const values = EMBED_CALL(run_part, RunnerStartCtx, .at = &at, .bytes = sizeof(CliValues));
    memor.set(&(MemoriaCfg){.dst = values, .val = 0, .bytes = sizeof(*values)});
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
    EMBED_CALL(cli_catalog_jobs_max.begin, CliCatalogJobsMaxCfg, .catalog = &args->run->catalog, .jobs = jobs,
               .jobs_max = 1u, .params = params, .params_max = extent.params, .steps = steps, .steps_max = extent.steps,
               .request = arguments, .args_max = extent.args, .lists = lists, .lists_max = extent.lists,
               .text_out = text, .text_max = (unsigned int)extent.text);
    const CarcerMovableEntry *const stored =
        (EMBED_CALL(cli_catalog_from.copy, CliCatalogFromCfg, .from = args->catalog, .index = args->job,
                    .to = &args->run->catalog) != CLI_CATALOG_NONE)
            ? g_cli_guard->movable_buf_alloc(bytes, g_cli_runs_owner)
            : NULL;
    if (stored != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = stored, .offset = 0ULL, .src = cell, .bytes = bytes});
    }
    args->run->job_cell = stored;
    args->run->job_bytes = bytes;
    args->run->values_at = (unsigned long long)((unsigned char *)values - cell);
    args->run->job_at = (unsigned long long)((unsigned char *)jobs - cell);
    args->run->param_at = (unsigned long long)((unsigned char *)params - cell);
    args->run->step_at = (unsigned long long)((unsigned char *)steps - cell);
    args->run->arg_at = (unsigned long long)((unsigned char *)arguments - cell);
    args->run->list_at = (unsigned long long)((unsigned char *)lists - cell);
    args->run->text_at = (unsigned long long)((unsigned char *)text - cell);
    // the catalog's lists point into no cell between pins; its counts stand
    args->run->catalog.job = NULL;
    args->run->catalog.param = NULL;
    args->run->catalog.step = NULL;
    args->run->catalog.arg = NULL;
    args->run->catalog.list = NULL;
    args->run->catalog.text = NULL;
    g_cli_guard->persistent_buf_release(cell, g_cli_runs_owner);
    args->run->lines = lines;
    if (stored == NULL)
    {
        mmgr_line_put(args->err_msg, "the job contains more than a run stores");
        return 0;
    }
    return 1;
}

int cli_run_pinned(const CliRunnerRunCtx *args)
{
    CliRunPinned *const pinned = args->pinned;
    const CliRun *const run = args->run;
    pinned->cell = (run->job_cell != NULL) ? g_cli_guard->temporary_buf_alloc(run->job_bytes, g_cli_runs_owner) : NULL;
    if (pinned->cell == NULL)
    {
        return 0;
    }
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = run->job_cell, .offset = 0ULL, .dst = pinned->cell, .bytes = run->job_bytes});
    pinned->catalog = run->catalog;
    pinned->catalog.job = (CliJob *)(void *)(pinned->cell + run->job_at);
    pinned->catalog.param = (CliParam *)(void *)(pinned->cell + run->param_at);
    pinned->catalog.step = (CliStep *)(void *)(pinned->cell + run->step_at);
    pinned->catalog.arg = (CliArg *)(void *)(pinned->cell + run->arg_at);
    pinned->catalog.list = (unsigned int *)(void *)(pinned->cell + run->list_at);
    pinned->catalog.text = (char *)(void *)(pinned->cell + run->text_at);
    pinned->values = (CliValues *)(void *)(pinned->cell + run->values_at);
    return 1;
}

void cli_run_unpinned(const CliRunnerRunCtx *args)
{
    g_cli_guard->persistent_buf_release(args->pinned->cell, g_cli_runs_owner);
    args->pinned->cell = NULL;
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
    // every step that runs made once now: a step that cannot be made refuses the run before it starts
    const CliJob *const job_at = &args->catalog->job[args->job];
    for (unsigned int at = 0u; at < job_at->steps; at += 1u)
    {
        const CliStep *const step = &args->catalog->step[job_at->step_first + at];
        VerbaLinea displayed = {s_shown, sizeof(s_shown), 0ULL};
        if ((step->program != CLI_PROGRAM_CREATED) &&
            EMBED_CALL(cli_runner_command_table.runs, CliRunnerCommandCfg, .catalog = args->catalog, .step = step,
                       .values = args->values) &&
            !EMBED_CALL(cli_runner_command_table.command, CliRunnerCommandCfg, .root = args->root,
                        .catalog = args->catalog, .step = step, .values = args->values, .command = &s_checked,
                        .displayed = &displayed, .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    CliRun *run = NULL;
    {
        unsigned int at = 0u;
        while ((run == NULL) && (at < CLI_RUNS_MAX))
        {
            run = (!g_cli_runs[at].used || g_cli_runs[at].ended) ? &g_cli_runs[at] : NULL;
            at += 1u;
        }
    }
    if (run == NULL)
    {
        mmgr_line_put(args->err_msg, "more runs go at once than impero supports");
        return 0;
    }
    if (!EMBED_CALL(cli_runner_run_internal.run_cells_alloc, CliRunnerRunCtx, .run = run, .catalog = args->catalog,
                    .job = args->job, .err_msg = args->err_msg))
    {
        return 0;
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = run->job_cell, .offset = run->values_at, .src = args->values, .bytes = sizeof(*args->values)});
    memor.cpy(&(MemoriaCfg){.dst = run->root,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES}) + 1u});
    run->sink = sink;
    g_cli_next_run += 1ULL;
    run->run = g_cli_next_run;
    run->used = 1;
    run->ended = 0;
    run->stopped = 0;
    run->process = 0ULL;
    run->group = 0ULL;
    cli_lines_clear(run->lines);
    run->has_first = 0;
    run->began = cli_clock_milliseconds();
    // the run's start reported before any line of it, so that a reader knows the run and its job first, the job read
    // out of the run's cell for the report
    CliRunPinned pinned;
    const int pinned_read =
        EMBED_CALL(cli_runner_run_internal.run_pinned, CliRunnerRunCtx, .run = run, .pinned = &pinned);
    const CliRunEvent start = {CLI_RUN_EVENT_START,
                               run->run,
                               pinned_read
                                   ? EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                .catalog_read = &pinned.catalog, .text_u32 = pinned.catalog.job[0].id)
                                   : "",
                               NULL,
                               NULL,
                               0,
                               0LL,
                               0,
                               NULL,
                               0u,
                               NULL,
                               0.0};
    EMBED_CALL(sink.emit, CliEachEventCfg, .context = sink.context, .event = &start);
    if (pinned_read)
    {
        EMBED_CALL(cli_runner_run_internal.run_unpinned, CliRunnerRunCtx, .pinned = &pinned);
    }
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
    for (;;)
    {
        int going = 0;
        for (unsigned int at = 0u; at < CLI_RUNS_MAX; at += 1u)
        {
            going = going || (g_cli_runs[at].used && (g_cli_runs[at].run == args->run_number) && !g_cli_runs[at].ended);
        }
        if (!going)
        {
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &g_cli_runs_wake);
    }
}

int cli_runs_stop(const CliRunnerErrMsgRunNumberCfg *args)
{
    unsigned long long process = 0ULL;
    unsigned long long group = 0ULL;
    for (unsigned int at = 0u; at < CLI_RUNS_MAX; at += 1u)
    {
        if (g_cli_runs[at].used && (g_cli_runs[at].run == args->run_number))
        {
            g_cli_runs[at].stopped = 1;
            process = g_cli_runs[at].process;
            group = g_cli_runs[at].group;
        }
    }
    EMBED_CALL(cli_group_internal.spawn_group_end, CliGroupCtx, .group = group);
    if (process != 0ULL)
    {
        EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = process);
    }
    return 1;
}
