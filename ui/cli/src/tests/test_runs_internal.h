// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_runs_internal.h: what the test run sources share: a run, its processes each read on a thread of its own, the
// results each has heard, and what a run tells
#ifndef TEST_RUNS_INTERNAL_H
#define TEST_RUNS_INTERNAL_H

#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "cargo_tests.h"
#include "test_runs.h"

// the runs held at once, a run going and the one before it ending; the most processes a run starts; the longest line
// a process writes that is read whole, and the most bytes one thing a run tells takes; the bytes the names a run is
// given take; the most tests a cargo run names, the results heard and the bytes their names take; and the bytes of
// what a run says at its end
#define CLI_TEST_RUNS 2u
#define CLI_TEST_WORKERS 64u
#define CLI_TEST_LINE (1u << 16u)
#define CLI_TEST_EVENT (1u << 16u)
#define CLI_TEST_TEXT (1u << 22u)
#define CLI_TEST_NAMED 16384u
#define CLI_TEST_HEARD (1u << 16u)
#define CLI_TEST_HEARD_TEXT (1u << 22u)
#define CLI_TEST_SAID 4096u

// A test a cargo process runs by its name: its id, file, line, and the name the harness gives it
typedef struct
{
    const char *id;
    const char *file;
    unsigned int line;
    const char *full;
} CliTestNamed;

typedef struct CliTestRun CliTestRun;

// A process of a run: its run and place in it; whether it is cargo's, and pytest's where it is Python's; the process;
// the files in the run's folder its report, coverage, errors and output go to; for cargo, the tests it runs by name,
// for Python its share of the tests given, as their first place and count, and for cargo the folder their paths are
// written from; its threads, whether its errors are read and how many results it heard; the line being read, the output
// read and not yet written, and what it tells being written
typedef struct
{
    CliTestRun *run;
    unsigned int index;
    int cargo;
    int pytest;
    CliSpawned spawned;
    char report[CLI_PATH_LONGEST];
    char covered[CLI_PATH_LONGEST];
    char errors[CLI_PATH_LONGEST];
    char seen[CLI_PATH_LONGEST];
    unsigned int named;
    unsigned int named_count;
    char workspace[CLI_PATH_LONGEST];
    CliThread thread;
    CliThread error_thread;
    int errors_read;
    unsigned int heard;
    char line[CLI_TEST_LINE];
    unsigned long long line_size;
    char batch[CLI_TEST_LINE];
    unsigned long long batch_size;
    char event[CLI_TEST_EVENT];
} CliTestWorker;

// A run: whether its place is taken, its number, where it tells, its tree and own folder and when it started; under
// its lock, the threads still going, its counts of passed, failed and skipped, and what it says at its end; its
// processes; the tests its cargo processes run by name; the Python tests given, by their place in the order the shares
// take them, and each share's first place and size; the texts it keeps; and the results heard, each by its process
// and name, with its outcome
struct CliTestRun
{
    unsigned int place;
    unsigned long long number;
    CliTell tell;
    char root[CLI_PATH_LONGEST];
    char scratch[CLI_PATH_LONGEST];
    unsigned long long started;
    CliLock lock;
    CliWake wake;
    unsigned int going;
    unsigned long long passed;
    unsigned long long failed;
    unsigned long long skipped;
    char said[CLI_TEST_SAID];
    CliTestWorker workers[CLI_TEST_WORKERS];
    unsigned int worker_count;
    CliTestNamed named[CLI_TEST_NAMED];
    unsigned int named_count;
    const char *given[CLI_TEST_NAMED];
    unsigned int order[CLI_TEST_NAMED];
    unsigned int share_from[CLI_TEST_WORKERS];
    unsigned int share_size[CLI_TEST_WORKERS];
    unsigned int shares;
    char text[CLI_TEST_TEXT];
    unsigned long long used;
    unsigned long long heard_name[CLI_TEST_HEARD];
    const char *heard_outcome[CLI_TEST_HEARD];
    char heard_text[CLI_TEST_HEARD_TEXT];
    unsigned long long heard_used;
    CliThread ender;
};

// whether the run going was stopped, under the runs' lock (test_runs.c)
extern CliLock g_cli_test_lock;
extern int g_cli_test_stopped;

// whether the run going was stopped; `length` bytes put in the run's texts, ended by a 0, "" where they do not fit;
// an outcome counted; what the run says at its end, where it says nothing yet; a result heard of a process, by its
// name: 1 where it is new, 0 where it was heard before, its first outcome kept; the outcome a process heard of a name,
// NULL for none; and whether a process heard any result (test_runs.c)
int cli_test_stopped(void);
const char *cli_test_put(CliTestRun *run, const char *text, unsigned long long length);
void cli_test_tally(CliTestRun *run, const char *outcome);
void cli_test_said(CliTestRun *run, const char *text, unsigned long long length);
int cli_test_heard(CliTestRun *run, unsigned int worker, const char *name, unsigned long long length,
                   const char *outcome);
const char *cli_test_heard_outcome(CliTestRun *run, unsigned int worker, const char *name, unsigned long long length);
int cli_test_heard_any(CliTestRun *run, unsigned int worker);

// a result told, `{"kind": "result", "run", "outcome": {...}}`, its file NULL and its line -1 where it has none; the
// lines a process writes to its output read until its end or the run's stop, each kept in its output file and handed
// to `line`; its errors read whole into its errors file, on a thread of their own, waited for; and a file of the run's
// folder read whole, NULL where it is not there (test_runs.c, test_read.c)
void cli_test_result_told(CliTestWorker *worker, const char *id, const char *outcome, const char *message,
                          unsigned long long message_size, const char *file, long long line, double seconds);
void cli_test_lines_read(CliTestWorker *worker,
                         void (*line)(CliTestWorker *worker, const char *text, unsigned long long size));
void cli_test_errors_started(CliTestWorker *worker);
void cli_test_errors_waited(CliTestWorker *worker);
const char *cli_test_file_read(const char *path, unsigned long long *size);
void cli_test_file_let_go(const char *text, unsigned long long size);

// the cargo processes a run starts for the Rust tests `given`, `count` of them, each with a thread of its own: 1, or 0
// with why said (cargo_run.c)
int cli_cargo_workers_started(CliTestRun *run, const char *const *given, unsigned int count, CliLine *said);

// whether `python` has pytest, asked in the tree at `root`; and the shares the Python tests given, `count` of them, are
// spread over, at most `most`: whole files to a share, the files of most first, each to the share that holds the
// fewest so far, into the run's order and shares (test_python.c)
int cli_test_has_pytest(const char *python, const char *root);
void cli_test_shares(CliTestRun *run, unsigned int count, unsigned int most);

// the Python processes a run starts, a share of `given` to each, with pytest where `pytest` and coverage where `cover`:
// 1, or 0 with why said (test_python.c)
int cli_python_workers_started(CliTestRun *run, const char *python, int pytest, int cover, CliLine *said);

// the outcomes of pytest's report in JUnit's form each told, and the cells the report's tags and texts hold
// (test_reports.c); and the failures of unittest's report each told (test_unittest_report.c)
extern CliCellsTally g_cli_test_report_cells;
void cli_test_report_told(CliTestWorker *worker, const char *xml, unsigned long long size);
void cli_test_unittest_failures_told(CliTestWorker *worker, const char *text, unsigned long long size);
// where `needle` is first found in `length` bytes of `text`, `length` where it is not; a whole text read as a 32-bit
// count, a + before it as Rust takes one: 1, or 0 where it is none; whether `length` bytes of a path, its backslashes
// read as slashes, end with `wanted`; the next line of `size` bytes from `at`, a "\r" before its "\n" taken off: 1
// with its start and end, or 0 where none is left; and the last line of a text that is not blank, trimmed, a line of
// `---` passed over where `skip_rules`, as its start and size: 1, or 0 where every line is blank (test_reports.c)
unsigned long long cli_report_find(const char *text, unsigned long long length, const char *needle);
int cli_report_count(const char *text, unsigned long long length, long long *value);
int cli_report_ends_with(const char *path, unsigned long long length, const char *wanted);
int cli_report_line(const char *text, unsigned long long size, unsigned long long *at, unsigned long long *start,
                    unsigned long long *end);
int cli_report_last_line(const char *text, unsigned long long size, int skip_rules, unsigned long long *last,
                         unsigned long long *last_size);
// a line pytest prints in its verbose report as a test ends: the test's name, its start and size in the line, and its
// outcome: 1, or 0 where the line is no such line; a line unittest prints so, `test_x (pkg.test_a.Case.test_x) ...
// ok`: the test's name as pytest gives it into `id`, which holds `room`, and its outcome, the same way; a pytest
// name as unittest takes it, `pkg/test_a.py::Case::test_x` as `pkg.test_a.Case.test_x`, put after the words; and a
// unittest name, `pkg.test_a.Case.test_x`, as pytest names it, into `id`, which holds `room`: 1, or 0 where it has
// no class (test_lines.c)
int cli_test_pytest_line(const char *line, unsigned long long size, unsigned long long *id, unsigned long long *id_size,
                         const char **outcome);
int cli_test_unittest_line(const char *line, unsigned long long size, char *id, unsigned long long room,
                           const char **outcome);
void cli_test_dotted(CliWords *words, const char *id);
int cli_test_unittest_id(const char *dotted, unsigned long long length, char *id, unsigned long long room);

// the shim a run that records its coverage runs pytest under; and the coverage the run's Python processes wrote, put
// together and told (test_coverage.c)
extern const char g_cli_test_shim[];
void cli_test_coverage_told(CliTestRun *run);

#endif
