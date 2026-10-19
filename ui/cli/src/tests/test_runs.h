// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_runs.h: running a tree's Python tests: with pytest where the tree's Python has it, and with unittest where it
// has not, unittest running the tests of its `TestCase`s. Its Rust tests run with cargo test in the same run, as
// cargo_tests.h runs them. The files of a run are spread over the machine's cores, a process of pytest to each share,
// the files of most tests first, each to the share that holds the fewest so far; each test's result is passed on as its
// process prints it, and each failure's message and the line it failed at as its process ends, from pytest's report.
//
// A run that records its coverage runs pytest under orior's own shim, which marks each line of the tree's files that
// runs, by `sys.monitoring` where Python has it and `sys.settrace` where it has not, and writes them, with the lines
// each file could run, for the run to put together (test_runs.c, test_read.c, test_python.c, test_lines.c,
// test_reports.c, test_coverage.c)
#ifndef TEST_RUNS_H
#define TEST_RUNS_H

#include "../command_line/cli_internal.h"

// What a run tells as it goes, written as JSON, `{"kind": "started" | "result" | "coverage" | "done", "run", ...}`:
// that it started, with how many tests or files, over how many processes and with which runner; each result, `{"id",
// "outcome", "message", "file", "line", "seconds"}`; the coverage, `{"file", "ran", "missed"}` by file; and its end,
// with its counts. Handed to `told` with its context, on a thread of the run's own
typedef struct
{
    void (*told)(void *context, const char *heard, unsigned long long size);
    void *context;
} CliTell;

// the Python a tree's tests run with, its environment's or the toolchains' own, into `python`, which holds
// CLI_PATH_LONGEST: 1, or 0 where there is none
int cli_test_python(char *python);

// `count` tests by their names or files by their paths, `given`, run in the tree at `root`, any run going stopped
// first: spread over the machine's cores where `parallel`, with coverage recorded where `cover`. The run's number into
// `run`, 1, or 0 with why said; what it finds comes by `tell`. And the run going stopped, where one is
int cli_test_runs_start(const char *root, const char *const *given, unsigned int count, int parallel, int cover,
                        CliTell tell, unsigned long long *run, CliLine *said);
void cli_test_runs_stop(void);

#endif
