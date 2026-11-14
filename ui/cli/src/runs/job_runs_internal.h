// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// job_runs_internal.h: the Run pane's store as its three sources share it: the catalog listed, the groups, the job
// chosen and its form, the runs with their lines, and the sections of the rows; and the entries one source answers for
// the others (job_runs.c, job_runs_rows.c, job_runs_form.c). Every group, run and section of the store is a movable
// cell of its own, taken as it comes and given back as it goes, its token in the store's table of its kind; the jobs'
// order and their passes are each one cell sized to the jobs listed, taken anew at each read. A record is read whole
// into a copy and a copy is written back; a reader of a text copies it out. A run's lines stand in MMgr's rings over
// the pane's part of the rings block
#ifndef CLI_RUNS_JOB_RUNS_INTERNAL_H
#define CLI_RUNS_JOB_RUNS_INTERNAL_H

#include "../command_line/command_line_internal.h"
#include "../command_line/rings.h"
#include "../text/typed.h"
#include "embed_ticket_ring.h"
#include "job_runs.h"
#include "memoria_anularis/memoria_anularis.h"
#include "runner.h"

// the field that takes the keys where no param is focused: the filter
#define CLI_JOB_RUNS_FILTER 0xffffffffu

// the param types whose choices are the value itself: one of them, and several
#define CLI_JOB_RUNS_TYPE_CHOICE "choice"
#define CLI_JOB_RUNS_TYPE_MANY "many"

// the store's mosts, each the places of one table of tokens: the runs kept, the oldest ended released past them; the
// params of a job the form lays; the sections the rows stand in, a group's and a job's each of the catalog's jobs and
// the chosen job's; a line's plain bytes kept; the spans one line carries; the plain bytes, and the spans, one run's
// window of CLI_LINES_MAX lines holds, the oldest lines released as a new one needs the room; and the reports on
// their way from the runs' threads to the view's thread at once, the slots of their ticket ring as a two's power, and
// the bytes the slots take, the reports' part of the rings block
#define CLI_JOB_RUNS_MAX 32u
#define CLI_JOB_RUNS_PARAMS_MAX 64u
#define CLI_JOB_RUNS_SECTIONS_MAX ((2u * CLI_LINE_JOBS) + 1u + (2u * CLI_JOB_RUNS_PARAMS_MAX) + (3u * CLI_JOB_RUNS_MAX))
#define CLI_JOB_RUNS_LINE_MAX 4096u
#define CLI_JOB_RUNS_LINE_SPANS 64u
#define CLI_JOB_RUNS_TEXT_MAX (CLI_LINES_MAX * 64u)
#define CLI_JOB_RUNS_SPANS_MAX (CLI_LINES_MAX * 4u)
#define CLI_JOB_RUNS_REPORTS_SHIFT 8u
#define CLI_JOB_RUNS_REPORTS_RING_BYTES ((1ULL << CLI_JOB_RUNS_REPORTS_SHIFT) * sizeof(EmbedTicketRingSlot))

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

// A line of a run: the count of the run's text bytes put before its first, its bytes, its stream, and its spans, the
// segment of the first in the run's spans ring and how many
typedef struct
{
    unsigned int start;
    unsigned int length;
    unsigned int stream;
    unsigned int span_first;
    unsigned int spans;
} CliJobRunsLine;

// The rings of a run's lines, MMgr's, over one set's span of the rings block: the plain text, a byte ring; the line
// records and the spans, each a segment ring with a record a segment; the bytes each ring streams; and the text bytes
// ever put and ever consumed, whose difference places a line's bytes ahead of the text ring's tail. A set's span lays
// its text, its records and its spans each on the block's boundary; a set for each run the pane holds makes its part
typedef struct
{
    mmgr_ring text;
    mmgr_ring line;
    mmgr_ring span;
    embed_u8 *text_bytes;
    CliJobRunsLine *line_bytes;
    CliTerminalXtermSpan *span_bytes;
    unsigned int text_put;
    unsigned int text_taken;
} CliJobRunsRings;
#define CLI_JOB_RUNS_SET_TEXT_AT 0ULL
#define CLI_JOB_RUNS_SET_LINE_AT CLI_RINGS_ALIGNED(CLI_JOB_RUNS_TEXT_MAX)
#define CLI_JOB_RUNS_SET_SPAN_AT                                                                                       \
    (CLI_JOB_RUNS_SET_LINE_AT + CLI_RINGS_ALIGNED((unsigned long long)CLI_LINES_MAX * sizeof(CliJobRunsLine)))
#define CLI_JOB_RUNS_SET_BYTES                                                                                         \
    (CLI_JOB_RUNS_SET_SPAN_AT +                                                                                        \
     CLI_RINGS_ALIGNED((unsigned long long)CLI_JOB_RUNS_SPANS_MAX * sizeof(CliTerminalXtermSpan)))
#define CLI_JOB_RUNS_RING_BYTES ((unsigned long long)CLI_JOB_RUNS_MAX * CLI_JOB_RUNS_SET_BYTES)

// the ring lays its byte stream and its segments only over counts of two's powers
EMBED_STATIC_ASSERT(MMGR_RING_POW2(CLI_JOB_RUNS_TEXT_MAX), "the text ring's bytes are a power of two");
EMBED_STATIC_ASSERT(MMGR_RING_POW2(CLI_LINES_MAX), "the line ring's segments are a power of two");
EMBED_STATIC_ASSERT(MMGR_RING_POW2(CLI_JOB_RUNS_SPANS_MAX), "the spans ring's segments are a power of two");

// A run: its number; its job's place in the catalog, CLI_CATALOG_NONE where the catalog lists it no more, and the job's
// id with its bytes in a cell; whether it goes, whether it was stopped, and its exit code where it has one; its state's
// words, `#n running` and the like, their bytes and where the last word starts in them; and its lines' rings
typedef struct
{
    unsigned long long run;
    unsigned int job;
    const CarcerMovableEntry *job_id;
    unsigned long long job_id_length;
    int live;
    int stopped;
    int has_code;
    long long code;
    char state[48];
    unsigned int state_length;
    unsigned int word;
    CliJobRunsRings *rings;
    unsigned char *rings_taken;
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
// handed; the groups' cells, one a group, and how many; the jobs in group order and a byte a job, 1 where the filter
// lets it stand, each one cell of the jobs listed; the job chosen and its id with its end, empty for none; the runs'
// cells, one a run, and how many; the run shown's number, 0 for none; the sections' cells, one a section, how many
// stand, and the rows, with the rows before they last grew; the filter; the form, a start a param of the chosen job and
// one past them, then the values' bytes, CLI_JOB_RUNS_FIELD_MAX a param, and the param focused, CLI_JOB_RUNS_FILTER for
// the filter; and the refusal of the last start with its bytes
typedef struct
{
    CliCatalog own;
    const CliCatalog *catalog;
    const CarcerMovableEntry *group[CLI_LINE_JOBS];
    unsigned int groups;
    const CarcerMovableEntry *order;
    const CarcerMovableEntry *passes;
    unsigned int chosen;
    char chosen_id[CLI_PATH_MAX_LEN];
    const CarcerMovableEntry *run[CLI_JOB_RUNS_MAX];
    unsigned int runs;
    unsigned long long shown;
    const CarcerMovableEntry *section[CLI_JOB_RUNS_SECTIONS_MAX];
    unsigned int sections;
    unsigned int rows;
    unsigned int rows_before;
    CliTyped filter;
    const CarcerMovableEntry *form;
    unsigned int form_params;
    unsigned int focus;
    char refusal[CLI_ERR_MSG_MAX_LEN];
    unsigned long long refusal_length;
} CliJobRunsStore;

// the one store the three sources share, read and written on the view's thread alone (job_runs.c)
extern CliJobRunsStore g_cli_job_runs_store;

// the sink a run the pane starts reports to, its emit storing each event as a report from the run's thread
// (job_runs.c)
extern const CliRunSink cli_job_runs_sink;

// a group, a run or a section `at` read whole out of its cell into the copy, and a copy written whole into the cell at
// `at`, the cell taken where none stands there: 1, or 0 where the cells cannot meet it (job_runs.c)
typedef struct
{
    unsigned int at;
    CliJobRunsGroup *group;
    CliJobRunsRun *run;
    CliJobRunsSection *section;
} CliJobRunsCellCfg;
void cli_job_runs_group_read(const CliJobRunsCellCfg *args);
int cli_job_runs_group_write(const CliJobRunsCellCfg *args);
void cli_job_runs_run_read(const CliJobRunsCellCfg *args);
int cli_job_runs_run_write(const CliJobRunsCellCfg *args);
void cli_job_runs_section_read(const CliJobRunsCellCfg *args);
int cli_job_runs_section_write(const CliJobRunsCellCfg *args);

typedef struct
{
    void (*group_read)(const CliJobRunsCellCfg *args);
    int (*group_write)(const CliJobRunsCellCfg *args);
    void (*run_read)(const CliJobRunsCellCfg *args);
    int (*run_write)(const CliJobRunsCellCfg *args);
    void (*section_read)(const CliJobRunsCellCfg *args);
    int (*section_write)(const CliJobRunsCellCfg *args);
} CliJobRunsCellsNs;
EMBED_TABLE_LAYOUT(CliJobRunsCellsNs, group_read, group_write, run_read, run_write, section_read, section_write);
EMBED_TABLE_STORAGE CliJobRunsCellsNs cli_job_runs_cells EMBED_UNUSED = {
    .group_read = cli_job_runs_group_read,
    .group_write = cli_job_runs_group_write,
    .run_read = cli_job_runs_run_read,
    .run_write = cli_job_runs_run_write,
    .section_read = cli_job_runs_section_read,
    .section_write = cli_job_runs_section_write,
};

typedef struct CliJobRunsCtx CliJobRunsCtx;

// the sections and the rows laid anew from the store (job_runs_rows.c); the form laid empty for the job chosen, none
// where none is; the byte a job set anew from the filter, each group's count of those it lets stand; the values of
// the form's params into `values`, each param the form leaves empty taking its default; the value of the param
// `param` copied out of the form's cell into `value_out` of CLI_JOB_RUNS_FIELD_MAX bytes with its end: its bytes; and
// the choice `choice` of the param `param` taken, or let go where it was, a choice of many turned among the others
// taken (job_runs_form.c)
void cli_job_runs_sections_settle(const CliJobRunsCtx *args);
void cli_job_runs_form_clear(const CliJobRunsCtx *args);
void cli_job_runs_filter_apply(const CliJobRunsCtx *args);
void cli_job_runs_values_fill(const CliJobRunsCtx *args);
unsigned long long cli_job_runs_form_value(const CliJobRunsCtx *args);
void cli_job_runs_choice_take(const CliJobRunsCtx *args);

struct CliJobRunsCtx
{
    CliValues *values;
    unsigned int param;
    unsigned int choice;
    char *value_out;
};

typedef struct
{
    void (*sections_settle)(const CliJobRunsCtx *args);
    void (*form_clear)(const CliJobRunsCtx *args);
    void (*filter_apply)(const CliJobRunsCtx *args);
    void (*values_fill)(const CliJobRunsCtx *args);
    unsigned long long (*form_value)(const CliJobRunsCtx *args);
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
