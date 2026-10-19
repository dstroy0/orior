// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_internal.h: a run as runner_run.c holds it and runner_lines.c carries its lines
#ifndef RUNNER_INTERNAL_H
#define RUNNER_INTERNAL_H

#include "runner.h"

// the most runs held at once, and the most steps a run's job takes
#define CLI_RUNS_MOST 16u

#define CLI_RUN_STEPS 256u

// A run: its number, sink and tree; its job, kept in a catalog of its own whose lists are one cell sized to the job,
// and the values it runs with; its thread; whether it has ended and whether a reader stopped it; the process of its
// step still running and the job it heads; the lines its steps wrote, a cell of the run's; when it began, and when the
// step running began and wrote its first line. The cells are taken as the run starts and given back as it ends
typedef struct
{
    int used;
    unsigned long long run;
    CliRunSink sink;
    char root[CLI_PATH_LONGEST];
    CliCatalog catalog;
    void *job_cell;
    CliValues values;
    CliThread thread;
    int ended;
    int stopped;
    unsigned long long process;
    unsigned long long group;
    CliLines *lines;
    unsigned long long began;
    double first;
    int has_first;
} CliRun;

// the run's job copied from `catalog` into a cell sized to it, and a cell taken for its lines: 1, or 0 with why said
// and neither held; and both given back (runner_start.c)
int cli_run_cells_taken(CliRun *run, const CliCatalog *catalog, unsigned int job, CliLine *said);
void cli_run_cells_given(CliRun *run);

// the lock every run's state is read and changed under
void cli_runs_take(void);
void cli_runs_give(void);
// the milliseconds since a run began, to the microsecond (runner_run.c)
double cli_run_since(const CliRun *run);
// a line said on a run's stream, kept among its lines where it is output (runner_lines.c)
void cli_run_line(CliRun *run, const char *stream, const char *text);
// each line of a started process's output and errors carried to the run as it comes, the errors on a thread of their
// own, until both end
void cli_run_carry(CliRun *run, const CliSpawned *spawned);
// the pages a job wrote: every .html file in its view folder, and every .html path its output names that is a file,
// each by its path in the tree, in order and each once, into `views`, which holds CLI_VIEWS_MOST: their count
unsigned int cli_run_pages(const char *root, const char *view_out, const CliLines *lines,
                           char (*views)[CLI_PATH_LONGEST]);

// the runs held, the next run's number, and the lock and wake over them (runner_run.c)
extern CliRun g_cli_runs[CLI_RUNS_MOST];
extern unsigned long long g_cli_next_run;
extern CliLock g_cli_runs_lock;
extern CliWake g_cli_runs_wake;
// a run's steps taken in turn, its end said after the last: the function its thread runs
void cli_run_steps(void *context);

#endif
