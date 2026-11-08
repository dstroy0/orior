// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// job_runs_internal.h: the Run pane's store as its three sources share it: the catalog listed, the groups, the job
// chosen and its form, the runs with their lines, and the sections of the rows; and the entries one source answers for
// the others (job_runs.c, job_runs_rows.c, job_runs_form.c)
#ifndef CLI_RUNS_JOB_RUNS_INTERNAL_H
#define CLI_RUNS_JOB_RUNS_INTERNAL_H
#include "../text/typed.h"
#include "job_runs.h"
#include "runner.h"

// the field that takes the keys where no param is focused: the filter
#define CLI_JOB_RUNS_FILTER 0xffffffffu

// the param types whose choices are the value itself: one of them, and several
#define CLI_JOB_RUNS_TYPE_CHOICE "choice"
#define CLI_JOB_RUNS_TYPE_MANY "many"

// A group of the jobs: its name, the place of its first job among the jobs in group order and how many it has, how many
// of them the filter lets stand and that count's digits with their bytes, and whether it is open
typedef struct
{
    const char *name;
    unsigned int first;
    unsigned int count;
    unsigned int listed;
    char listed_text[12];
    unsigned int listed_length;
    int open;
} CliJobRunsGroup;

// A line of a run: the place of its first byte in the run's text and its bytes, its stream, and its spans, the place
// of the first among the run's spans and how many
typedef struct
{
    unsigned int start;
    unsigned int length;
    unsigned int stream;
    unsigned int span_first;
    unsigned int spans;
} CliJobRunsLine;

// A run: its number; its job's place in the catalog, CLI_CATALOG_NONE where the catalog lists it no more, and the job's
// id with its end in a cell; whether it goes, whether it was stopped, and its exit code where it has one; its state's
// words, `#n running` and the like, their bytes and where the last word starts in them; and its lines: their plain
// text, the lines and their spans, each in a cell with what it has in use and what it can take
typedef struct
{
    unsigned long long run;
    unsigned int job;
    char *job_id;
    int live;
    int stopped;
    int has_code;
    long long code;
    char state[48];
    unsigned int state_length;
    unsigned int word;
    char *text;
    unsigned long long text_used;
    unsigned long long text_capacity;
    CliJobRunsLine *line;
    unsigned int lines;
    unsigned int lines_capacity;
    CliTerminalXtermSpan *span;
    unsigned int spans;
    unsigned int spans_capacity;
} CliJobRunsRun;

// A section of the rows: its first row, its rows, their type, the item they read, the group's place, the job's, the
// param's among the job's or the run's, and the flags its one row carries where it has one
typedef struct
{
    unsigned int start;
    unsigned int count;
    unsigned int type;
    unsigned int item;
    unsigned int flags;
} CliJobRunsSection;

// The store: the catalog the tree's read stands in, its cell the store's own, and the catalog listed, that one or one
// handed; the groups; the jobs in group order and a byte a job, 1 where the filter lets it stand; the job chosen, its
// id in a cell; the runs; the run shown's number, 0 for none; the sections and the rows, with the rows before they last
// grew; the filter; the form, a start a param of the chosen job and one past them, then the values' bytes, and the
// param focused, CLI_JOB_RUNS_FILTER for the filter; and the refusal of the last start with its bytes
typedef struct
{
    CliCatalog own;
    const CliCatalog *catalog;
    CliJobRunsGroup *group;
    unsigned int groups;
    unsigned int *order;
    unsigned char *passes;
    unsigned int chosen;
    char *chosen_id;
    CliJobRunsRun *run;
    unsigned int runs;
    unsigned long long shown;
    CliJobRunsSection *section;
    unsigned int sections;
    unsigned int rows;
    unsigned int rows_before;
    CliTyped filter;
    unsigned int *form;
    unsigned int form_params;
    unsigned int focus;
    char *refusal;
    unsigned long long refusal_length;
} CliJobRunsStore;

// the one store the three sources share, read and written on the view's thread alone (job_runs.c)
extern CliJobRunsStore g_cli_job_runs_store;

// the sink a run the pane starts reports to, its emit storing each event as a report from the run's thread
// (job_runs.c)
extern const CliRunSink cli_job_runs_sink;

typedef struct CliJobRunsCtx CliJobRunsCtx;

// the sections and the rows laid anew from the store (job_runs_rows.c); the form laid empty for the job chosen, none
// where none is; the byte a job set anew from the filter, each group's count of those it lets stand; and the values of
// the form's params into `values`, each param the form leaves empty taking its default; the value of the param
// `param` in the form's cell, its bytes into `length_out`; and the choice `choice` of the param `param` taken, or let
// go where it was, a choice of many turned among the others taken (job_runs_form.c)
void cli_job_runs_sections_settle(const CliJobRunsCtx *args);
void cli_job_runs_form_clear(const CliJobRunsCtx *args);
void cli_job_runs_filter_apply(const CliJobRunsCtx *args);
void cli_job_runs_values_fill(const CliJobRunsCtx *args);
const char *cli_job_runs_form_value(const CliJobRunsCtx *args);
void cli_job_runs_choice_take(const CliJobRunsCtx *args);

struct CliJobRunsCtx
{
    CliValues *values;
    unsigned int param;
    unsigned int choice;
    unsigned long long *length_out;
};

typedef struct
{
    void (*sections_settle)(const CliJobRunsCtx *args);
    void (*form_clear)(const CliJobRunsCtx *args);
    void (*filter_apply)(const CliJobRunsCtx *args);
    void (*values_fill)(const CliJobRunsCtx *args);
    const char *(*form_value)(const CliJobRunsCtx *args);
    void (*choice_take)(const CliJobRunsCtx *args);
} CliJobRunsInternalNs;
EMBED_TABLE_LAYOUT(CliJobRunsInternalNs, sections_settle, form_clear, filter_apply, values_fill, form_value,
                   choice_take);
EMBED_TABLE_STORAGE CliJobRunsInternalNs cli_job_runs_internal EMBED_UNUSED = {
    .sections_settle = cli_job_runs_sections_settle,
    .form_clear = cli_job_runs_form_clear,
    .filter_apply = cli_job_runs_filter_apply,
    .values_fill = cli_job_runs_values_fill,
    .form_value = cli_job_runs_form_value,
    .choice_take = cli_job_runs_choice_take,
};

#endif
