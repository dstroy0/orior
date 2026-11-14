// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// job_runs.h: the Run pane as the CLI carries it. The tree's jobs listed by their group, each group opened or closed,
// those whose id, title or about contain the filter typed alone; under the job chosen its file and about, a field
// each param, the choices of the param focused under it, a row each run of the job and under the run displayed its lines,
// each plain with its color spans as the xterm reads them, and the end of a run that ended. A job is started with the
// form's values, refused with the runner's sentence where they do not do, and stopped; its runs' threads store what
// they report under a lock and wake the view's thread, whose step takes it into the rows. The rows are in sections,
// each its first row, and a row is found by a binary search over those starts; the counts change as the data changes
// (job_runs.c the store, the sink and the runs; job_runs_rows.c the sections and the rows; job_runs_form.c the filter
// and the form)
#ifndef CLI_RUNS_JOB_RUNS_H
#define CLI_RUNS_JOB_RUNS_H
#include "../command_line/catalog.h"
#include "../command_line/pool.h"
#include "../terminal/xterm.h"
#include "embed_dispatch_layout.h"

typedef struct CliJobRunsReportCfg CliJobRunsReportCfg;
typedef struct CliJobRunsWokenCfg CliJobRunsWokenCfg;
typedef struct CliJobRunsRootCfg CliJobRunsRootCfg;
typedef struct CliJobRunsRowCfg CliJobRunsRowCfg;
typedef struct CliJobRunsFieldCfg CliJobRunsFieldCfg;
typedef struct CliJobRunsScrollCfg CliJobRunsScrollCfg;
typedef struct CliJobRunsRunCfg CliJobRunsRunCfg;

// the ended runs of a job retained, the oldest released with its lines past them; and the max bytes the filter or a
// field takes with its end
#define CLI_JOB_RUNS_RETAINED 10u
#define CLI_JOB_RUNS_FIELD_MAX 4096u

// the rows' types: a group, a job, the chosen job's file and about, a param's field, a choice of the param focused, a
// run of the chosen job, a line of the run displayed, and the end of the run displayed where it ended
#define CLI_JOB_RUN_GROUP 0u
#define CLI_JOB_RUN_JOB 1u
#define CLI_JOB_RUN_ABOUT 2u
#define CLI_JOB_RUN_PARAM 3u
#define CLI_JOB_RUN_CHOICE 4u
#define CLI_JOB_RUN_RUN 5u
#define CLI_JOB_RUN_LINE 6u
#define CLI_JOB_RUN_END 7u

// a row's flags: a group open, the job chosen or a choice taken, a run going or a job with one going, a param
// required, the field that takes the keys, a param's default displayed for its empty value, a run that ended with 0, one
// that ended otherwise or was stopped, and the run displayed
#define CLI_JOB_RUN_OPEN 0x001u
#define CLI_JOB_RUN_CHOSEN 0x002u
#define CLI_JOB_RUN_LIVE 0x004u
#define CLI_JOB_RUN_REQUIRED 0x008u
#define CLI_JOB_RUN_FOCUSED 0x010u
#define CLI_JOB_RUN_DEFAULTED 0x020u
#define CLI_JOB_RUN_OK 0x040u
#define CLI_JOB_RUN_FAILED 0x080u
#define CLI_JOB_RUN_DISPLAYED 0x100u

// a line's stream: what a step wrote to its output, to its errors, and the command line a step runs
#define CLI_JOB_RUN_STDOUT 0u
#define CLI_JOB_RUN_STDERR 1u
#define CLI_JOB_RUN_COMMAND 2u

// what a run reports, in its order: its start, each line, and its end
#define CLI_JOB_RUNS_STARTED 0u
#define CLI_JOB_RUNS_LINE 1u
#define CLI_JOB_RUNS_ENDED 2u

// A row: its type; its text and its bytes, the group's name, the job's title, the file, the param's key, the choice,
// the run's number and state, the line's plain text or the end's word; a second text and its bytes, the about, the
// param's value or its default; the guides it is in; its flags; a line's stream; and a number, the jobs a group
// lists or the run's
typedef struct
{
    unsigned int type;
    const char *text;
    unsigned long long text_length;
    const char *second;
    unsigned long long second_length;
    unsigned int depth;
    unsigned int flags;
    unsigned int stream;
    unsigned long long number;
} CliJobRunRow;

// A line of the run displayed: its plain text and its bytes, its color spans and how many, and its stream
typedef struct
{
    const char *text;
    unsigned long long length;
    const CliTerminalXtermSpan *span;
    unsigned int spans;
    unsigned int stream;
} CliJobRunLine;

// the store begun, `woken` waking the view's thread as a run's thread stores what it reports; what the runs' threads
// stored taken into the runs and the rows on the view's thread: 1 where the rows changed; and every cell the store
// takes released, the store empty
void cli_job_runs_begin(const CliJobRunsWokenCfg *args);
int cli_job_runs_step(const CliJobRunsWokenCfg *args);
void cli_job_runs_end(const CliJobRunsWokenCfg *args);
// what a run reports stored from its thread under the lock and the view's thread woken, as the sink of a run the pane
// starts stores it: its type; its run's number; for its start, its job's id; for a line, its stream and its text; for
// its end, its exit code where it has one and whether it was stopped
void cli_job_runs_reported(const CliJobRunsReportCfg *args);
// the jobs listed anew from `catalog` where it is passed, which then outlives the store's use of it, else from the
// catalog read of the tree at `root`, in the store's own cell; the groups' open flags following their names, and the
// job chosen following its id, dropped where the catalog no longer lists it
void cli_job_runs_read(const CliJobRunsRootCfg *args);
// how many rows the pane has; the row `row_at` into `row_out`: 1, or 0 past the last; the line of the LINE row
// `row_at` into `line_out`: 1, or 0 where the row is no line; and the row `row_at` pressed: a group opened or closed,
// a job chosen or let go, a param focused, a choice taken or a choice of many turned, a run displayed: 1 where the row
// takes the keys
unsigned int cli_job_runs_row_count(const CliJobRunsRowCfg *args);
int cli_job_runs_row_read(const CliJobRunsRowCfg *args);
int cli_job_runs_line_read(const CliJobRunsRowCfg *args);
int cli_job_runs_row_press(const CliJobRunsRowCfg *args);
// `length` bytes of `bytes` typed into the field that has the keys, the filter or the param focused; the key
// `key_name` pressed there, Backspace erasing its last character: 1 where it was taken; the filter, its bytes into
// `length_out`; whether the filter is the field that takes the keys; and the keys moved to the filter
void cli_job_runs_type(const CliJobRunsFieldCfg *args);
int cli_job_runs_key(const CliJobRunsFieldCfg *args);
unsigned long long cli_job_runs_filter(const CliJobRunsFieldCfg *args);
int cli_job_runs_filter_focused(const CliJobRunsFieldCfg *args);
void cli_job_runs_filter_focus(const CliJobRunsFieldCfg *args);
// the scroll top that brings the last row into view for a pane `capacity` rows tall scrolled to `scroll_top`, where it
// was at the last row before the rows last grew, else `scroll_top`
unsigned int cli_job_runs_follow(const CliJobRunsScrollCfg *args);
// the chosen job started in the tree at `root` with the form's values: 1, or 0 with the refusal located; the run
// displayed stopped where it goes: 1, or 0 where none goes; the refusal of the last start, its bytes into `length_out`,
// empty where it started; and whether the run displayed goes
int cli_job_runs_start(const CliJobRunsRunCfg *args);
int cli_job_runs_stop(const CliJobRunsRunCfg *args);
const char *cli_job_runs_refusal(const CliJobRunsRunCfg *args);
int cli_job_runs_live(const CliJobRunsRunCfg *args);

struct CliJobRunsReportCfg
{
    unsigned int type;
    unsigned long long run;
    const char *job;
    unsigned int stream;
    const char *text;
    int has_code;
    long long code;
    int stopped;
};

struct CliJobRunsWokenCfg
{
    void (*woken)(void);
};

struct CliJobRunsRootCfg
{
    const char *root;
    const CliCatalog *catalog;
};

struct CliJobRunsRowCfg
{
    unsigned int row_at;
    CliJobRunRow *row_out;
    CliJobRunLine *line_out;
};

struct CliJobRunsFieldCfg
{
    const char *bytes;
    unsigned long long length;
    const char *key_name;
    unsigned long long *length_out;
    char *text_out;
};

struct CliJobRunsScrollCfg
{
    unsigned int scroll_top;
    unsigned int capacity;
};

struct CliJobRunsRunCfg
{
    const char *root;
    unsigned long long *length_out;
};

typedef struct
{
    void (*begin)(const CliJobRunsWokenCfg *args);
    int (*step)(const CliJobRunsWokenCfg *args);
    void (*end)(const CliJobRunsWokenCfg *args);
} CliJobRunsWokenNs;
EMBED_TABLE_LAYOUT(CliJobRunsWokenNs, begin, step, end);
EMBED_TABLE_STORAGE CliJobRunsWokenNs cli_job_runs_woken EMBED_UNUSED = {
    .begin = cli_job_runs_begin,
    .step = cli_job_runs_step,
    .end = cli_job_runs_end,
};

typedef struct
{
    void (*store)(const CliJobRunsReportCfg *args);
} CliJobRunsReportNs;
EMBED_TABLE_LAYOUT(CliJobRunsReportNs, store);
EMBED_TABLE_STORAGE CliJobRunsReportNs cli_job_runs_report EMBED_UNUSED = {
    .store = cli_job_runs_reported,
};

typedef struct
{
    void (*read)(const CliJobRunsRootCfg *args);
} CliJobRunsRootNs;
EMBED_TABLE_LAYOUT(CliJobRunsRootNs, read);
EMBED_TABLE_STORAGE CliJobRunsRootNs cli_job_runs_root EMBED_UNUSED = {
    .read = cli_job_runs_read,
};

typedef struct
{
    unsigned int (*rows)(const CliJobRunsRowCfg *args);
    int (*row)(const CliJobRunsRowCfg *args);
    int (*line)(const CliJobRunsRowCfg *args);
    int (*press)(const CliJobRunsRowCfg *args);
} CliJobRunsRowNs;
EMBED_TABLE_LAYOUT(CliJobRunsRowNs, rows, row, line, press);
EMBED_TABLE_STORAGE CliJobRunsRowNs cli_job_runs_row EMBED_UNUSED = {
    .rows = cli_job_runs_row_count,
    .row = cli_job_runs_row_read,
    .line = cli_job_runs_line_read,
    .press = cli_job_runs_row_press,
};

typedef struct
{
    void (*type)(const CliJobRunsFieldCfg *args);
    int (*key)(const CliJobRunsFieldCfg *args);
    unsigned long long (*filter)(const CliJobRunsFieldCfg *args);
    int (*focused)(const CliJobRunsFieldCfg *args);
    void (*filter_focus)(const CliJobRunsFieldCfg *args);
} CliJobRunsFieldNs;
EMBED_TABLE_LAYOUT(CliJobRunsFieldNs, type, key, filter, focused, filter_focus);
EMBED_TABLE_STORAGE CliJobRunsFieldNs cli_job_runs_field EMBED_UNUSED = {
    .type = cli_job_runs_type,
    .key = cli_job_runs_key,
    .filter = cli_job_runs_filter,
    .focused = cli_job_runs_filter_focused,
    .filter_focus = cli_job_runs_filter_focus,
};

typedef struct
{
    unsigned int (*follow)(const CliJobRunsScrollCfg *args);
} CliJobRunsScrollNs;
EMBED_TABLE_LAYOUT(CliJobRunsScrollNs, follow);
EMBED_TABLE_STORAGE CliJobRunsScrollNs cli_job_runs_scroll EMBED_UNUSED = {
    .follow = cli_job_runs_follow,
};

typedef struct
{
    int (*start)(const CliJobRunsRunCfg *args);
    int (*stop)(const CliJobRunsRunCfg *args);
    const char *(*refusal)(const CliJobRunsRunCfg *args);
    int (*live)(const CliJobRunsRunCfg *args);
} CliJobRunsRunNs;
EMBED_TABLE_LAYOUT(CliJobRunsRunNs, start, stop, refusal, live);
EMBED_TABLE_STORAGE CliJobRunsRunNs cli_job_runs_run EMBED_UNUSED = {
    .start = cli_job_runs_start,
    .stop = cli_job_runs_stop,
    .refusal = cli_job_runs_refusal,
    .live = cli_job_runs_live,
};

#endif
