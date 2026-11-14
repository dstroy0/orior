// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_internal.h: a run as runner_run.c stores it and runner_lines.c carries its lines
#ifndef RUNNER_INTERNAL_H
#define RUNNER_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "runner.h"

typedef struct CliRunnerRunCtx CliRunnerRunCtx;
typedef struct CliRunnerRootCtx CliRunnerRootCtx;
typedef struct CliRunnerRunStreamCtx CliRunnerRunStreamCtx;
typedef struct CliRunnerRunSpawnedCtx CliRunnerRunSpawnedCtx;
typedef struct CliRunnerCtx CliRunnerCtx;

// the max steps a run's job takes
#define CLI_RUN_STEPS 256u

// A run: its number, sink and tree; its job, its catalog's counts retained on the run and its lists and the values it runs
// with positioned in one movable cell sized to the job, each part at its place from the cell's start, read into a cell of the
// call that reads them; its thread; whether it has ended and whether a reader stopped it; the process of its step still
// running and the job it heads; the lines its steps wrote, which are fixed outside the pool; when it began, and when
// the step running began and wrote its first line. The job's cell is taken as the run starts and given back as it ends
typedef struct
{
    int used;
    unsigned long long run;
    CliRunSink sink;
    char root[CLI_PATH_MAX_LEN];
    CliCatalog catalog;
    const CarcerMovableEntry *job_cell;
    unsigned long long job_bytes;
    unsigned long long values_at;
    unsigned long long job_at;
    unsigned long long param_at;
    unsigned long long step_at;
    unsigned long long arg_at;
    unsigned long long list_at;
    unsigned long long text_at;
    CliThread thread;
    int ended;
    int stopped;
    unsigned long long process;
    unsigned long long group;
    CliRunLines *lines;
    unsigned long long began;
    double first;
    int has_first;
} CliRun;

// A run's job and values read into a cell of the call's own: the cell, the catalog with its lists pointed into it, and
// the values
typedef struct
{
    unsigned char *cell;
    CliCatalog catalog;
    CliValues *values;
} CliRunPinned;

// the run's job copied from `catalog` into a cell sized to it, and a cell taken for its lines: 1, or 0 with why given
// and neither taken; and both given back (runner_start.c)
int cli_run_cells_alloc(const CliRunnerRunCtx *args);
// the run's job and values read out of its cell into `pinned`, a cell of this call: 1, or 0 where the cell cannot be
// taken; and `pinned` given back (runner_start.c)
int cli_run_pinned(const CliRunnerRunCtx *args);
void cli_run_unpinned(const CliRunnerRunCtx *args);

// the lock every run's state is read and changed under
// the milliseconds since a run began, to the microsecond (runner_run.c)
double cli_run_since(const CliRunnerCtx *args);
// a line printed on a run's stream, stored among its lines where it is output (runner_lines.c)
void cli_run_line(const CliRunnerRunStreamCtx *args);
// each line of a started process's output and errors carried to the run as it comes, the errors on a thread of their
// own, until both end
void cli_run_output_relay(const CliRunnerRunSpawnedCtx *args);
// the pages a job wrote: every .html file in its view folder, and every .html path its output names that is a file,
// each by its path in the tree, in order and each once, into `views`, sized for CLI_VIEWS_MAX: their count
unsigned int cli_run_pages(const CliRunnerRootCtx *args);

// the runs open, the next run's number, and the lock and wake over them (runner_run.c)
extern CliRun g_cli_runs[CLI_RUNS_MAX];
extern unsigned long long g_cli_next_run;
extern CliWake g_cli_runs_wake;
// a run's steps taken in turn, its end reported after the last: the function its thread runs
void cli_run_steps(void *context);

struct CliRunnerRunCtx
{
    CliRun *run;
    const CliCatalog *catalog;
    unsigned int job;
    VerbaLinea *err_msg;
    CliRunPinned *pinned;
};

struct CliRunnerRootCtx
{
    const char *root;
    const char *view_out;
    const CliRunLines *lines;
    char (*views)[CLI_PATH_MAX_LEN];
};

struct CliRunnerRunStreamCtx
{
    CliRun *run;
    const char *stream;
    const char *text;
};

struct CliRunnerRunSpawnedCtx
{
    CliRun *run;
    const CliSpawned *spawned;
};

struct CliRunnerCtx
{
    const CliRun *run_read;
};

typedef struct
{
    int (*run_cells_alloc)(const CliRunnerRunCtx *args);
    int (*run_pinned)(const CliRunnerRunCtx *args);
    void (*run_unpinned)(const CliRunnerRunCtx *args);
} CliRunnerRunNs;
EMBED_TABLE_LAYOUT(CliRunnerRunNs, run_cells_alloc, run_pinned, run_unpinned);
EMBED_TABLE_STORAGE CliRunnerRunNs cli_runner_run_internal EMBED_UNUSED = {
    .run_cells_alloc = cli_run_cells_alloc,
    .run_pinned = cli_run_pinned,
    .run_unpinned = cli_run_unpinned,
};

typedef struct
{
    void (*run_output_relay)(const CliRunnerRunSpawnedCtx *args);
} CliRunnerRunSpawnedNs;
EMBED_TABLE_LAYOUT(CliRunnerRunSpawnedNs, run_output_relay);
EMBED_TABLE_STORAGE CliRunnerRunSpawnedNs cli_runner_run_spawned_internal EMBED_UNUSED = {
    .run_output_relay = cli_run_output_relay,
};

typedef struct
{
    double (*run_since)(const CliRunnerCtx *args);
} CliRunnerNsTable;
EMBED_TABLE_LAYOUT(CliRunnerNsTable, run_since);
EMBED_TABLE_STORAGE CliRunnerNsTable cli_runner_internal EMBED_UNUSED = {
    .run_since = cli_run_since,
};

typedef struct
{
    void (*run_line)(const CliRunnerRunStreamCtx *args);
} CliRunnerRunStreamNs;
EMBED_TABLE_LAYOUT(CliRunnerRunStreamNs, run_line);
EMBED_TABLE_STORAGE CliRunnerRunStreamNs cli_runner_run_stream_internal EMBED_UNUSED = {
    .run_line = cli_run_line,
};

typedef struct
{
    unsigned int (*run_pages)(const CliRunnerRootCtx *args);
} CliRunnerRootNs;
EMBED_TABLE_LAYOUT(CliRunnerRootNs, run_pages);
EMBED_TABLE_STORAGE CliRunnerRootNs cli_runner_root_internal EMBED_UNUSED = {
    .run_pages = cli_run_pages,
};

#endif
