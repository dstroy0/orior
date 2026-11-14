// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_runs_internal.h: what the test run sources share: a run, its processes each read on a thread of its own, the
// results each has received, and what a run reports
#ifndef TEST_RUNS_INTERNAL_H
#define TEST_RUNS_INTERNAL_H
#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "cargo_tests.h"
#include "embed_dispatch_layout.h"
#include "test_runs.h"

typedef struct CliTestRunsOutcomeCtx CliTestRunsOutcomeCtx;
typedef struct CliTestRunsRunCtx CliTestRunsRunCtx;
typedef struct CliTestRunsTextCtx CliTestRunsTextCtx;
typedef struct CliTestRunsTextSizeCtx CliTestRunsTextSizeCtx;
typedef struct CliTestRunsSizeCtx CliTestRunsSizeCtx;
typedef struct CliTestRunsRunLengthCtx CliTestRunsRunLengthCtx;
typedef struct CliTestRunsSizeLineCtx CliTestRunsSizeLineCtx;
typedef struct CliTestRunsRunInputCtx CliTestRunsRunInputCtx;
typedef struct CliTestRunsLengthCtx CliTestRunsLengthCtx;
typedef struct CliTestRunsTextLengthCtx CliTestRunsTextLengthCtx;
typedef struct CliTestRunsLengthPathCtx CliTestRunsLengthPathCtx;
typedef struct CliTestRunsTextLengthNeedleCtx CliTestRunsTextLengthNeedleCtx;
typedef struct CliTestRunsRunTextCtx CliTestRunsRunTextCtx;
typedef struct CliTestRunsWorkerCtx CliTestRunsWorkerCtx;
typedef struct CliTestRunsRunCountCtx CliTestRunsRunCountCtx;
typedef struct CliTestRunsTextWorkerCtx CliTestRunsTextWorkerCtx;
typedef struct CliTestRunsCtx CliTestRunsCtx;
typedef struct CliTestRunsPathCtx CliTestRunsPathCtx;
typedef struct CliTestRunsPythonCtx CliTestRunsPythonCtx;
typedef struct CliTestRunsWorkerLineCtx CliTestRunsWorkerLineCtx;

// the runs open at once, a run going and the one before it ending; the max processes a run starts; the longest line
// a process writes that is read whole, and the max bytes one thing a run reports takes; the bytes the names a run is
// given take; the max tests a cargo run names, the results received and the bytes their names take; and the bytes of
// what a run reports at its end
#define CLI_TEST_RUNS 2u
#define CLI_TEST_WORKERS 64u
#define CLI_TEST_LINE (1u << 16u)
#define CLI_TEST_EVENT (1u << 16u)
#define CLI_TEST_TEXT (1u << 22u)
#define CLI_TEST_NAMED 16384u
#define CLI_TEST_SEEN_MAX (1u << 16u)
#define CLI_TEST_SEEN_TEXT_MAX (1u << 22u)
#define CLI_TEST_MESSAGE_MAX_LEN 4096u

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
// written from; its threads, whether its errors are read and how many results it received; the line being read, the
// output read and not yet written, and what it reports being written
typedef struct
{
    CliTestRun *run;
    unsigned int index;
    int cargo;
    int pytest;
    CliSpawned spawned;
    char report[CLI_PATH_MAX_LEN];
    char covered[CLI_PATH_MAX_LEN];
    char errors[CLI_PATH_MAX_LEN];
    char seen[CLI_PATH_MAX_LEN];
    unsigned int named;
    unsigned int named_count;
    char workspace[CLI_PATH_MAX_LEN];
    CliThread thread;
    CliThread error_thread;
    int errors_read;
    unsigned int seen_count;
    char line[CLI_TEST_LINE];
    unsigned long long line_size;
    char batch[CLI_TEST_LINE];
    unsigned long long batch_size;
    char event[CLI_TEST_EVENT];
} CliTestWorker;

// A run: whether its place is taken, its number, where it reports, its tree and own folder and when it started; under
// its lock, the threads still going, its counts of passed, failed and skipped, and what it reports at its end; its
// processes; the tests its cargo processes run by name; the Python tests given, by their place in the order the shares
// take them, and each share's first place and size; the texts it stores; and the results received, each by its process
// and name, with its outcome
struct CliTestRun
{
    unsigned int place;
    unsigned long long number;
    CliJsonSink sink;
    char root[CLI_PATH_MAX_LEN];
    char scratch[CLI_PATH_MAX_LEN];
    unsigned long long started;
    CliWake wake;
    unsigned int going;
    unsigned long long passed;
    unsigned long long failed;
    unsigned long long skipped;
    char message[CLI_TEST_MESSAGE_MAX_LEN];
    CliTestWorker workers[CLI_TEST_WORKERS];
    unsigned int worker_count;
    CliTestNamed named[CLI_TEST_NAMED];
    unsigned int named_count;
    const char *supplied[CLI_TEST_NAMED];
    unsigned int order[CLI_TEST_NAMED];
    unsigned int share_from[CLI_TEST_WORKERS];
    unsigned int share_size[CLI_TEST_WORKERS];
    unsigned int shares;
    char text[CLI_TEST_TEXT];
    unsigned long long used;
    unsigned long long seen_name[CLI_TEST_SEEN_MAX];
    const char *seen_outcome[CLI_TEST_SEEN_MAX];
    char seen_text[CLI_TEST_SEEN_TEXT_MAX];
    unsigned long long seen_used;
    CliThread terminator;
};

// whether the run going was stopped, under the runs' lock (test_runs.c)
extern int g_cli_test_stopped;

// whether the run going was stopped; `length` bytes put in the run's texts, ended by a 0, "" where they do not fit;
// an outcome counted; what the run reports at its end, where it reports nothing yet; a result received of a process, by
// its name: 1 where it is new, 0 where it was received before, its first outcome recorded; the outcome a process
// received of a name, NULL for none; and whether a process received any result (test_runs.c)
int cli_test_stop(void);
const char *cli_test_append(const CliTestRunsRunTextCtx *args);
void cli_test_outcome_count(const CliTestRunsRunLengthCtx *args);
void cli_test_message_set(const CliTestRunsRunTextCtx *args);
int cli_test_visit(const CliTestRunsRunLengthCtx *args);
const char *cli_test_seen_outcome(const CliTestRunsRunLengthCtx *args);
int cli_test_seen_any(const CliTestRunsRunLengthCtx *args);

// a result reported, `{"type": "result", "run", "outcome": {...}}`, its file NULL and its line -1 where it has none;
// the lines a process writes to its output read until its end or the run's stop, each stored in its output file and
// passed to `line`; its errors read whole into its errors file, on a thread of their own, waited for; and a file of the
// run's folder read whole, NULL where it is not there (test_runs.c, test_read.c)
void cli_test_result_emit(const CliTestRunsOutcomeCtx *args);
typedef struct
{
    CliTestWorker *worker;
    const char *text;
    unsigned long long size;
} CliWorkerTextSizeCfg;

void cli_test_lines_read(const CliTestRunsWorkerLineCtx *args);
void cli_test_errors_start(const CliTestRunsWorkerLineCtx *args);
void cli_test_errors_wait(const CliTestRunsWorkerLineCtx *args);
const char *cli_test_file_read(const CliTestRunsPathCtx *args);
void cli_test_file_let_go(const CliTestRunsTextWorkerCtx *args);

// the cargo processes a run starts for the Rust tests `supplied`, `count` of them, each with a thread of its own: 1, or
// 0 with why given (cargo_run.c)
int cli_cargo_workers_start(const CliTestRunsRunInputCtx *args);

// whether `python` has pytest, queried in the tree at `root`; and the shares the Python tests given, `count` of them,
// are spread over, at max `max`: whole files to a share, the files of max first, each to the share that has the
// fewest so far, into the run's order and shares (test_python.c)
int cli_test_has_pytest(const CliTestRunsPythonCtx *args);
void cli_test_shares(const CliTestRunsRunCountCtx *args);

// the Python processes a run starts, a share of `supplied` to each, with pytest where `pytest` and coverage where
// `cover`: 1, or 0 with why given (test_python.c)
int cli_python_workers_start(const CliTestRunsRunCtx *args);

// the outcomes of pytest's report in JUnit's form each reported, and the cells the report's tags and texts occupy
void cli_test_report_emit(const CliTestRunsWorkerCtx *args);
void cli_test_unittest_failures_emit(const CliTestRunsTextWorkerCtx *args);
// where `needle` is first found in `length` bytes of `text`, `length` where it is not; a whole text read as a 32-bit
// count, a + before it as Rust takes one: 1, or 0 where it is none; whether `length` bytes of a path, its backslashes
// read as slashes, end with `required`; the next line of `size` bytes from `at`, a "\r" before its "\n" taken off: 1
// with its start and end, or 0 where none is left; and the last line of a text that is not blank, trimmed, a line of
// `---` passed over where `skip_rules`, as its start and size: 1, or 0 where every line is blank (test_reports.c)
unsigned long long cli_report_find(const CliTestRunsTextLengthNeedleCtx *args);
int cli_report_count(const CliTestRunsTextLengthCtx *args);
int cli_report_ends_with(const CliTestRunsLengthPathCtx *args);
int cli_report_line(const CliTestRunsTextSizeCtx *args);
int cli_report_last_line(const CliTestRunsTextCtx *args);
// a line pytest prints in its verbose report as a test ends: the test's name, its start and size in the line, and its
// outcome: 1, or 0 where the line is no such line; a line unittest prints so, `test_x (pkg.test_a.Case.test_x) ...
// ok`: the test's name as pytest gives it into `id`, sized for `capacity`, and its outcome, the same way; a pytest
// name as unittest takes it, `pkg/test_a.py::Case::test_x` as `pkg.test_a.Case.test_x`, put after the words; and a
// unittest name, `pkg.test_a.Case.test_x`, as pytest names it, into `id`, sized for `capacity`: 1, or 0 where it has
// no class (test_lines.c)
int cli_test_pytest_line(const CliTestRunsSizeCtx *args);
int cli_test_unittest_line(const CliTestRunsSizeLineCtx *args);
void cli_test_dot(const CliTestRunsCtx *args);
int cli_test_unittest_id(const CliTestRunsLengthCtx *args);

// the shim a run that records its coverage runs pytest under; and the coverage the run's Python processes wrote, put
// together and reported (test_coverage.c)
extern const char g_cli_test_shim[];
void cli_test_coverage_emit(const CliTestRunsRunTextCtx *args);

struct CliTestRunsOutcomeCtx
{
    const char *outcome;
    CliTestWorker *worker;
    const char *id;
    const char *message;
    unsigned long long message_size;
    const char *file;
    long long line_i64;
    double seconds;
};

struct CliTestRunsRunCtx
{
    CliTestRun *run;
    VerbaLinea *err_msg;
    const char *python;
    int pytest;
    int cover;
};

struct CliTestRunsTextCtx
{
    const char *text;
    unsigned long long size;
    int skip_rules;
    unsigned long long *last;
    unsigned long long *last_size;
};

struct CliTestRunsTextSizeCtx
{
    const char *text;
    unsigned long long size;
    unsigned long long *at;
    unsigned long long *start;
    unsigned long long *end;
};

struct CliTestRunsSizeCtx
{
    unsigned long long size;
    const char *line;
    unsigned long long *id_u64s;
    unsigned long long *id_size;
    const char **outcome_pointer;
};

struct CliTestRunsRunLengthCtx
{
    CliTestRun *run;
    unsigned long long length;
    const char *outcome;
    unsigned int worker_u32;
    const char *name;
};

struct CliTestRunsSizeLineCtx
{
    unsigned long long size;
    const char *line;
    const char **outcome_pointer;
    char *id_out;
    unsigned long long capacity;
};

struct CliTestRunsRunInputCtx
{
    CliTestRun *run;
    const char *const *supplied;
    unsigned int count;
    VerbaLinea *err_msg;
};

struct CliTestRunsLengthCtx
{
    unsigned long long length;
    char *id_out;
    unsigned long long capacity;
    const char *dotted;
};

struct CliTestRunsTextLengthCtx
{
    const char *text;
    unsigned long long length;
    long long *value;
};

struct CliTestRunsLengthPathCtx
{
    unsigned long long length;
    const char *path;
    const char *required;
};

struct CliTestRunsTextLengthNeedleCtx
{
    const char *text;
    unsigned long long length;
    const char *needle;
};

struct CliTestRunsRunTextCtx
{
    CliTestRun *run;
    const char *text;
    unsigned long long length;
};

struct CliTestRunsWorkerCtx
{
    CliTestWorker *worker;
    unsigned long long size;
    const char *xml;
};

struct CliTestRunsRunCountCtx
{
    CliTestRun *run;
    unsigned int count;
    unsigned int max;
};

struct CliTestRunsTextWorkerCtx
{
    const char *text;
    CliTestWorker *worker;
    unsigned long long size;
};

struct CliTestRunsCtx
{
    const char *id;
    CliWords *words;
};

struct CliTestRunsPathCtx
{
    const char *path;
    unsigned long long *size_pointer;
};

struct CliTestRunsPythonCtx
{
    const char *python;
    const char *root;
};

struct CliTestRunsWorkerLineCtx
{
    CliTestWorker *worker;
    void (*line_callback)(const CliWorkerTextSizeCfg *args);
};

typedef struct
{
    const char *(*test_append)(const CliTestRunsRunTextCtx *args);
    void (*test_message_set)(const CliTestRunsRunTextCtx *args);
    void (*test_coverage_emit)(const CliTestRunsRunTextCtx *args);
} CliTestRunsRunTextNs;
EMBED_TABLE_LAYOUT(CliTestRunsRunTextNs, test_append, test_message_set, test_coverage_emit);
EMBED_TABLE_STORAGE CliTestRunsRunTextNs cli_test_runs_run_text_internal EMBED_UNUSED = {
    .test_append = cli_test_append,
    .test_message_set = cli_test_message_set,
    .test_coverage_emit = cli_test_coverage_emit,
};

typedef struct
{
    void (*test_outcome_count)(const CliTestRunsRunLengthCtx *args);
    int (*test_visit)(const CliTestRunsRunLengthCtx *args);
    const char *(*test_seen_outcome)(const CliTestRunsRunLengthCtx *args);
    int (*test_seen_any)(const CliTestRunsRunLengthCtx *args);
} CliTestRunsRunLengthNs;
EMBED_TABLE_LAYOUT(CliTestRunsRunLengthNs, test_outcome_count, test_visit, test_seen_outcome, test_seen_any);
EMBED_TABLE_STORAGE CliTestRunsRunLengthNs cli_test_runs_run_length_internal EMBED_UNUSED = {
    .test_outcome_count = cli_test_outcome_count,
    .test_visit = cli_test_visit,
    .test_seen_outcome = cli_test_seen_outcome,
    .test_seen_any = cli_test_seen_any,
};

typedef struct
{
    void (*test_result_emit)(const CliTestRunsOutcomeCtx *args);
} CliTestRunsOutcomeNs;
EMBED_TABLE_LAYOUT(CliTestRunsOutcomeNs, test_result_emit);
EMBED_TABLE_STORAGE CliTestRunsOutcomeNs cli_test_runs_outcome_internal EMBED_UNUSED = {
    .test_result_emit = cli_test_result_emit,
};

typedef struct
{
    void (*test_lines_read)(const CliTestRunsWorkerLineCtx *args);
    void (*test_errors_start)(const CliTestRunsWorkerLineCtx *args);
    void (*test_errors_wait)(const CliTestRunsWorkerLineCtx *args);
} CliTestRunsWorkerLineNs;
EMBED_TABLE_LAYOUT(CliTestRunsWorkerLineNs, test_lines_read, test_errors_start, test_errors_wait);
EMBED_TABLE_STORAGE CliTestRunsWorkerLineNs cli_test_runs_worker_line_internal EMBED_UNUSED = {
    .test_lines_read = cli_test_lines_read,
    .test_errors_start = cli_test_errors_start,
    .test_errors_wait = cli_test_errors_wait,
};

typedef struct
{
    const char *(*test_file_read)(const CliTestRunsPathCtx *args);
} CliTestRunsPathNs;
EMBED_TABLE_LAYOUT(CliTestRunsPathNs, test_file_read);
EMBED_TABLE_STORAGE CliTestRunsPathNs cli_test_runs_path_internal EMBED_UNUSED = {
    .test_file_read = cli_test_file_read,
};

typedef struct
{
    void (*test_file_let_go)(const CliTestRunsTextWorkerCtx *args);
    void (*test_unittest_failures_emit)(const CliTestRunsTextWorkerCtx *args);
} CliTestRunsTextWorkerNs;
EMBED_TABLE_LAYOUT(CliTestRunsTextWorkerNs, test_file_let_go, test_unittest_failures_emit);
EMBED_TABLE_STORAGE CliTestRunsTextWorkerNs cli_test_runs_text_worker_internal EMBED_UNUSED = {
    .test_file_let_go = cli_test_file_let_go,
    .test_unittest_failures_emit = cli_test_unittest_failures_emit,
};

typedef struct
{
    int (*cargo_workers_start)(const CliTestRunsRunInputCtx *args);
} CliTestRunsRunInputNs;
EMBED_TABLE_LAYOUT(CliTestRunsRunInputNs, cargo_workers_start);
EMBED_TABLE_STORAGE CliTestRunsRunInputNs cli_test_runs_run_input_internal EMBED_UNUSED = {
    .cargo_workers_start = cli_cargo_workers_start,
};

typedef struct
{
    int (*test_has_pytest)(const CliTestRunsPythonCtx *args);
} CliTestRunsPythonNsTable;
EMBED_TABLE_LAYOUT(CliTestRunsPythonNsTable, test_has_pytest);
EMBED_TABLE_STORAGE CliTestRunsPythonNsTable cli_test_runs_python_internal EMBED_UNUSED = {
    .test_has_pytest = cli_test_has_pytest,
};

typedef struct
{
    void (*test_shares)(const CliTestRunsRunCountCtx *args);
} CliTestRunsRunCountNs;
EMBED_TABLE_LAYOUT(CliTestRunsRunCountNs, test_shares);
EMBED_TABLE_STORAGE CliTestRunsRunCountNs cli_test_runs_run_count_internal EMBED_UNUSED = {
    .test_shares = cli_test_shares,
};

typedef struct
{
    int (*python_workers_start)(const CliTestRunsRunCtx *args);
} CliTestRunsRunNs;
EMBED_TABLE_LAYOUT(CliTestRunsRunNs, python_workers_start);
EMBED_TABLE_STORAGE CliTestRunsRunNs cli_test_runs_run_internal EMBED_UNUSED = {
    .python_workers_start = cli_python_workers_start,
};

typedef struct
{
    void (*test_report_emit)(const CliTestRunsWorkerCtx *args);
} CliTestRunsWorkerNs;
EMBED_TABLE_LAYOUT(CliTestRunsWorkerNs, test_report_emit);
EMBED_TABLE_STORAGE CliTestRunsWorkerNs cli_test_runs_worker_internal EMBED_UNUSED = {
    .test_report_emit = cli_test_report_emit,
};

typedef struct
{
    unsigned long long (*report_find)(const CliTestRunsTextLengthNeedleCtx *args);
} CliTestRunsTextLengthNeedleNs;
EMBED_TABLE_LAYOUT(CliTestRunsTextLengthNeedleNs, report_find);
EMBED_TABLE_STORAGE CliTestRunsTextLengthNeedleNs cli_test_runs_text_length_needle_internal EMBED_UNUSED = {
    .report_find = cli_report_find,
};

typedef struct
{
    int (*report_count)(const CliTestRunsTextLengthCtx *args);
} CliTestRunsTextLengthNs;
EMBED_TABLE_LAYOUT(CliTestRunsTextLengthNs, report_count);
EMBED_TABLE_STORAGE CliTestRunsTextLengthNs cli_test_runs_text_length_internal EMBED_UNUSED = {
    .report_count = cli_report_count,
};

typedef struct
{
    int (*report_ends_with)(const CliTestRunsLengthPathCtx *args);
} CliTestRunsLengthPathNs;
EMBED_TABLE_LAYOUT(CliTestRunsLengthPathNs, report_ends_with);
EMBED_TABLE_STORAGE CliTestRunsLengthPathNs cli_test_runs_length_path_internal EMBED_UNUSED = {
    .report_ends_with = cli_report_ends_with,
};

typedef struct
{
    int (*report_line)(const CliTestRunsTextSizeCtx *args);
} CliTestRunsTextSizeNs;
EMBED_TABLE_LAYOUT(CliTestRunsTextSizeNs, report_line);
EMBED_TABLE_STORAGE CliTestRunsTextSizeNs cli_test_runs_text_size_internal EMBED_UNUSED = {
    .report_line = cli_report_line,
};

typedef struct
{
    int (*report_last_line)(const CliTestRunsTextCtx *args);
} CliTestRunsTextNs;
EMBED_TABLE_LAYOUT(CliTestRunsTextNs, report_last_line);
EMBED_TABLE_STORAGE CliTestRunsTextNs cli_test_runs_text_internal EMBED_UNUSED = {
    .report_last_line = cli_report_last_line,
};

typedef struct
{
    int (*test_pytest_line)(const CliTestRunsSizeCtx *args);
} CliTestRunsSizeNs;
EMBED_TABLE_LAYOUT(CliTestRunsSizeNs, test_pytest_line);
EMBED_TABLE_STORAGE CliTestRunsSizeNs cli_test_runs_size_internal EMBED_UNUSED = {
    .test_pytest_line = cli_test_pytest_line,
};

typedef struct
{
    int (*test_unittest_line)(const CliTestRunsSizeLineCtx *args);
} CliTestRunsSizeLineNs;
EMBED_TABLE_LAYOUT(CliTestRunsSizeLineNs, test_unittest_line);
EMBED_TABLE_STORAGE CliTestRunsSizeLineNs cli_test_runs_size_line_internal EMBED_UNUSED = {
    .test_unittest_line = cli_test_unittest_line,
};

typedef struct
{
    void (*test_dot)(const CliTestRunsCtx *args);
} CliTestRunsNsTable;
EMBED_TABLE_LAYOUT(CliTestRunsNsTable, test_dot);
EMBED_TABLE_STORAGE CliTestRunsNsTable cli_test_runs_internal EMBED_UNUSED = {
    .test_dot = cli_test_dot,
};

typedef struct
{
    int (*test_unittest_id)(const CliTestRunsLengthCtx *args);
} CliTestRunsLengthNs;
EMBED_TABLE_LAYOUT(CliTestRunsLengthNs, test_unittest_id);
EMBED_TABLE_STORAGE CliTestRunsLengthNs cli_test_runs_length_internal EMBED_UNUSED = {
    .test_unittest_id = cli_test_unittest_id,
};

#endif
