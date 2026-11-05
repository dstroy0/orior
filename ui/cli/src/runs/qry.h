// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry.h: query runs as the window watches them, each a .qry in a folder of runs, read through the run's member and the
// .qry reader (`qry_member.h`, `qry_read.h`), never a copy of them (qry.c reads and appends, qry_queue.c queues the
// backend's records, qry_listed.c lists a tree's runs for the Runs pane).
//
// A run whose writer runs is viewed in its shared memory, read alone: its records are read where the writer put them,
// in the order of the writer's schedule, and each read goes on from the place the last stopped.
//
// A finished run's .qry is its records end to end (qry.h), each checked against its CRC; a read of it goes on from the
// byte the last stopped at, and a record not yet whole where a writer still writes it is read by the next read.
//
// What a record contains is read for a panic or a critical record, a conference, and a record of at most 4096 bytes,
// and at most 65536 bytes of it; of any other record, its name and its count
#ifndef CLI_QRY_H
#define CLI_QRY_H
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliQryPathCfg CliQryPathCfg;
typedef struct CliQryPathNameCfg CliQryPathNameCfg;
typedef struct CliQryPathOutCfg CliQryPathOutCfg;
typedef struct CliQryOutCfg CliQryOutCfg;
typedef struct CliQryDataCfg CliQryDataCfg;
typedef struct CliQryPathFromCfg CliQryPathFromCfg;
typedef struct CliQryOutSizeCfg CliQryOutSizeCfg;
typedef struct CliQryPathRootCfg CliQryPathRootCfg;
typedef struct CliQryOutRootCfg CliQryOutRootCfg;
typedef struct CliQryPathPanickedCfg CliQryPathPanickedCfg;
typedef struct CliQryPathFromEachCfg CliQryPathFromEachCfg;
typedef struct CliQryCfg CliQryCfg;
typedef struct CliRunsPaneCfg CliRunsPaneCfg;

// the cells a tree's runs are listed in (qry_listed.c)
extern CellOwner g_cli_qry_listing_cells;

// A record of a run handed on as it is read: the record where it stands, and its sequence, its count among the run's
// records from 0
typedef struct
{
    void *context;
    const QryRecord *record;
    unsigned long long sequence;
} CliEachRecordCfg;

// the prefix of a conference's records, its question's identity after it
#define CLI_CONFERENCE "conference_"

// the backend's channels to a run, each with a thread of its own: the primary channel, for the regular traps a party
// gathers, its traces and the files it hands; the control channel, for what steers a run; and the signal channel, for
// its panics and critical records. A record on one never waits behind the records queued on another
enum
{
    CLI_CHANNEL_PRIMARY,
    CLI_CHANNEL_CONTROL,
    CLI_CHANNEL_SIGNAL,
    CLI_CHANNELS
};

// the bytes of every record of the run at `path` named `name`, joined in the order the writer wrote them, put into
// `out`: from its shared memory while its writer runs, from its .qry where none does. 1, or 0 with why given
int cli_qry_joined_bytes(const CliQryPathOutCfg *args);
// the names of the records of the .qry whose bytes are `data`, in the order of their first records, each into
// `names`, sized for `capacity` of CLI_NAME_MAX_LEN: their count; and each record of `name` joined into `out`
unsigned int cli_qry_member_names(const CliQryDataCfg *args);
void cli_qry_member_join(const CliQryOutCfg *args);
// the bytes the blobs of `name` in `size` bytes of a .qry join to
unsigned long long cli_qry_member_size(const CliQryOutCfg *args);
// `size` bytes appended to the run at `path` as a record of `name` at `priority`, the window a member of that run; a
// panic or a critical record waited on until it is on the disk: 1, or 0 with why given
int cli_qry_append(const CliQryPathCfg *args);
// 1 once the writer of the run at `path` has made its shared memory and writes, waiting up to thirty seconds
int cli_qry_ready(const CliQryPathPanickedCfg *args);
// the writer of the run at `path` requested to write the rest of the run and stop, the window a member of the run for
// the requesting and of none after: 1, or 0 with why given
int cli_qry_stop(const CliQryPathOutCfg *args);
// the run the window's own work is appended to, into `path`, sized for CLI_PATH_MAX_LEN: the one QRY names where the
// window was started inside a run, or the one it has last joined to; 1, or 0 where none. And the window joined to the
// run at `path`, or to none where `path` is NULL
int cli_qry_join(const CliQryCfg *args);
void cli_qry_join_window(const CliQryPathPanickedCfg *args);
// `size` bytes queued to be appended to the run at `path` as the record `name` at `priority`, on `channel`; a panic
// or a critical record on the signal channel whatever channel is named, and a record larger than half a channel's
// ring appended before the call returns; and what the queue has done so far written as
// `{"handed", "dumped", "failed", "message"}` (qry_queue.c)
void cli_qry_queue(const CliQryPathNameCfg *args);
// whether the run at `path` is live, and the place of the panic on its line where it is
int cli_qry_existing(const CliQryPathPanickedCfg *args);
// each record of the run at `path` from the place `from` handed to `each` with `context`, counted on from `sequence`:
// from its shared memory while its writer runs, `from` a place in the writer's schedule; from its .qry where none does,
// `from` a byte of it. The place past the last record into `next`, and the .qry's bytes past the last whole record
// into `rest`, 0 while the writer runs. 1 while the writer runs, else 0
int cli_qry_records(const CliQryPathFromEachCfg *args);
// a file the watch of the tree at `root` saw written, `length` bytes of its path from the top of the tree: where it is
// a run or a folder's overseer.txt, its folder put among the folders the next listing reads and the listing marked
// stale; and the watch's changes lost, so the tree is walked again for its folders of runs at the next listing
// (qry_listed.c)
void cli_qry_run_write(const CliQryPathRootCfg *args);
void cli_qry_runs_lose(const CliQryOutRootCfg *args);

// The Runs pane as the window lists it (qry_listed.c): every query run of the tree, each a .qry in a folder of runs,
// found wherever its script was started, and live while its writer runs. The runs are listed at the first step after
// the tree opens, as the pane comes into sight, as the tree's watch sees a run or an overseer's lessons written or
// loses changes, and every CLI_RUNS_LIST_LIVE_MS while a run is live. A run opened in the pane is read whole on its
// opening, read on from where the last read stopped every CLI_RUNS_READ_LIVE_MS while it is live and the pane shows,
// and read whole again once it has ended. While no run is live nothing is asked. A run's first panic is posted as a
// failed notice the moment a listing sees it, and the strip's Runs icon is marked while a run is live and in the
// error's color while a live run has panicked. A conference, the records `conference_` and its question's identity,
// each space written as an underscore, shows each line `<expert>: <account>` of each of its records, and a line that
// starts `overseer:` as the ruling. The lessons of each folder's overseer.txt, one `lesson <cause> <carrier or *>
// <stand-in>` a line, show under the folder's runs
// the beats while a run is live: the runs listed again, and an opened live run read on
#define CLI_RUNS_LIST_LIVE_MS 1000ULL
#define CLI_RUNS_READ_LIVE_MS 400ULL
// the latest records an opened run lists as rows; its panics and conferences show however many came before
#define CLI_RUNS_SHOWN_BLOBS 200u
// the rows: the tree has no run; a folder of runs; a run; a panic's head and a line of its text; a conference's
// question and a line of its accounts; an opened run's count of records; one of its latest records; and the lessons'
// head and a lesson
#define CLI_RUNS_ROW_EMPTY 0u
#define CLI_RUNS_ROW_FOLDER 1u
#define CLI_RUNS_ROW_RUN 2u
#define CLI_RUNS_ROW_PANIC 3u
#define CLI_RUNS_ROW_PANIC_LINE 4u
#define CLI_RUNS_ROW_QUESTION 5u
#define CLI_RUNS_ROW_ACCOUNT 6u
#define CLI_RUNS_ROW_COUNT 7u
#define CLI_RUNS_ROW_BLOB 8u
#define CLI_RUNS_ROW_LESSONS 9u
#define CLI_RUNS_ROW_LESSON 10u

// A row of the pane: its kind; a run row's path from the top of the tree; its texts, a folder row's folder, a run's
// name, a panic line's text, a question, an account's text with the expert in `second`, a record's name, a lesson's
// cause with its carrier in `second` and its stand-in in `third`; `number`, a panic's or a record's sequence or the
// count row's records; `size`, a run's or a record's bytes, or the count row's bytes after the .qry's last whole
// record; a record's priority; whether a run is live and the place plus 1 of its panic, 0 where none; whether a run
// row is opened; and whether an account is the ruling
typedef struct
{
    unsigned int kind;
    const char *path;
    const char *text;
    unsigned long long text_length;
    const char *second;
    unsigned long long second_length;
    const char *third;
    unsigned long long third_length;
    unsigned long long number;
    unsigned long long size;
    unsigned int priority;
    int live;
    unsigned long long panicked;
    int open;
    int ruling;
} CliRunsRow;

// what wakes the window as the watch marks the listing stale, `woken`, from the watch's thread
void cli_runs_pane_begin(const CliRunsPaneCfg *args);
// the runs of the tree at `root` listed now
void cli_runs_pane_relist(const CliRunsPaneCfg *args);
// one step of the window at `now_ms` for the tree at `root`, NULL where none is open, the pane in sight where `shown`:
// the runs listed where none were, where the listing is stale or where one is live and CLI_RUNS_LIST_LIVE_MS passed;
// and where the pane shows, each opened live run read on where CLI_RUNS_READ_LIVE_MS passed: 1 where the rows changed.
// And when the next beat falls, 0 where none is awaited
int cli_runs_pane_step(const CliRunsPaneCfg *args);
unsigned long long cli_runs_pane_deadline_ms(const CliRunsPaneCfg *args);
// how many rows there are; the row `at` into `row`: 1, or 0 past the last; and the run row `at` opened and read, or
// closed where it is opened, for the tree at `root`
unsigned int cli_runs_pane_rows(const CliRunsPaneCfg *args);
int cli_runs_pane_row(const CliRunsPaneCfg *args);
void cli_runs_pane_toggle(const CliRunsPaneCfg *args);
// how many runs are live into `live_out`, and how many of those have panicked into `panicked_out`
void cli_runs_pane_marks(const CliRunsPaneCfg *args);
struct CliRunsPaneCfg
{
    void (*woken)(void);
    const char *root;
    unsigned long long now_ms;
    int shown;
    unsigned int at;
    CliRunsRow *row;
    unsigned int *live_out;
    unsigned int *panicked_out;
};
struct CliQryPathCfg
{
    const char *path;
    VerbaLinea *err_msg;
    const char *name;
    unsigned long long size;
    const void *bytes;
    unsigned int priority;
};

struct CliQryPathNameCfg
{
    const char *path;
    const char *name;
    unsigned long long size;
    const void *bytes;
    unsigned int priority;
    unsigned int channel_named;
};

struct CliQryPathOutCfg
{
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *name;
};

struct CliQryOutCfg
{
    VerbaLinea *out;
    const char *name;
    const unsigned char *data;
    unsigned long long size;
};

struct CliQryDataCfg
{
    const unsigned char *data;
    unsigned long long size;
    char (*names)[CLI_NAME_MAX_LEN];
    unsigned int capacity;
};

struct CliQryPathFromCfg
{
    const char *path;
    unsigned long long from;
    VerbaLinea *out;
    VerbaLinea *err_msg;
};

struct CliQryOutSizeCfg
{
    VerbaLinea *out;
    unsigned long long size;
    const char *text;
};

struct CliQryPathRootCfg
{
    const char *path;
    const char *root;
    unsigned long long length;
};

struct CliQryOutRootCfg
{
    VerbaLinea *out;
    const char *root;
};

struct CliQryPathPanickedCfg
{
    const char *path;
    unsigned long long *panicked;
};

struct CliQryPathFromEachCfg
{
    const char *path;
    unsigned long long from;
    unsigned long long sequence;
    void (*each)(const CliEachRecordCfg *args);
    void *context;
    unsigned long long *next;
    unsigned long long *rest;
};

struct CliQryCfg
{
    char *path_out;
};

typedef struct
{
    int (*joined_bytes)(const CliQryPathOutCfg *args);
    int (*stop)(const CliQryPathOutCfg *args);
} CliQryPathOutNs;
EMBED_TABLE_LAYOUT(CliQryPathOutNs, joined_bytes, stop);
EMBED_TABLE_STORAGE CliQryPathOutNs cli_qry_path_out EMBED_UNUSED = {
    .joined_bytes = cli_qry_joined_bytes,
    .stop = cli_qry_stop,
};

typedef struct
{
    unsigned int (*member_names)(const CliQryDataCfg *args);
} CliQryDataNs;
EMBED_TABLE_LAYOUT(CliQryDataNs, member_names);
EMBED_TABLE_STORAGE CliQryDataNs cli_qry_data EMBED_UNUSED = {
    .member_names = cli_qry_member_names,
};

typedef struct
{
    void (*member_join)(const CliQryOutCfg *args);
    unsigned long long (*member_size)(const CliQryOutCfg *args);
} CliQryOutNs;
EMBED_TABLE_LAYOUT(CliQryOutNs, member_join, member_size);
EMBED_TABLE_STORAGE CliQryOutNs cli_qry_out EMBED_UNUSED = {
    .member_join = cli_qry_member_join,
    .member_size = cli_qry_member_size,
};

typedef struct
{
    int (*append)(const CliQryPathCfg *args);
} CliQryPathNs;
EMBED_TABLE_LAYOUT(CliQryPathNs, append);
EMBED_TABLE_STORAGE CliQryPathNs cli_qry_path EMBED_UNUSED = {
    .append = cli_qry_append,
};

typedef struct
{
    int (*ready)(const CliQryPathPanickedCfg *args);
    void (*join_window)(const CliQryPathPanickedCfg *args);
    int (*existing)(const CliQryPathPanickedCfg *args);
} CliQryPathPanickedNs;
EMBED_TABLE_LAYOUT(CliQryPathPanickedNs, ready, join_window, existing);
EMBED_TABLE_STORAGE CliQryPathPanickedNs cli_qry_path_panicked EMBED_UNUSED = {
    .ready = cli_qry_ready,
    .join_window = cli_qry_join_window,
    .existing = cli_qry_existing,
};

typedef struct
{
    int (*join)(const CliQryCfg *args);
} CliQryNs;
EMBED_TABLE_LAYOUT(CliQryNs, join);
EMBED_TABLE_STORAGE CliQryNs cli_qry EMBED_UNUSED = {
    .join = cli_qry_join,
};

typedef struct
{
    void (*queue)(const CliQryPathNameCfg *args);
} CliQryPathNameNs;
EMBED_TABLE_LAYOUT(CliQryPathNameNs, queue);
EMBED_TABLE_STORAGE CliQryPathNameNs cli_qry_path_name EMBED_UNUSED = {
    .queue = cli_qry_queue,
};

typedef struct
{
    void (*runs_lose)(const CliQryOutRootCfg *args);
} CliQryOutRootNs;
EMBED_TABLE_LAYOUT(CliQryOutRootNs, runs_lose);
EMBED_TABLE_STORAGE CliQryOutRootNs cli_qry_out_root EMBED_UNUSED = {
    .runs_lose = cli_qry_runs_lose,
};

typedef struct
{
    void (*run_write)(const CliQryPathRootCfg *args);
} CliQryPathRootNs;
EMBED_TABLE_LAYOUT(CliQryPathRootNs, run_write);
EMBED_TABLE_STORAGE CliQryPathRootNs cli_qry_path_root EMBED_UNUSED = {
    .run_write = cli_qry_run_write,
};

typedef struct
{
    int (*records)(const CliQryPathFromEachCfg *args);
} CliQryPathFromEachNs;
EMBED_TABLE_LAYOUT(CliQryPathFromEachNs, records);
EMBED_TABLE_STORAGE CliQryPathFromEachNs cli_qry_path_from_each EMBED_UNUSED = {
    .records = cli_qry_records,
};

typedef struct
{
    void (*begin)(const CliRunsPaneCfg *args);
    void (*relist)(const CliRunsPaneCfg *args);
    int (*step)(const CliRunsPaneCfg *args);
    unsigned long long (*deadline_ms)(const CliRunsPaneCfg *args);
    unsigned int (*rows)(const CliRunsPaneCfg *args);
    int (*row)(const CliRunsPaneCfg *args);
    void (*toggle)(const CliRunsPaneCfg *args);
    void (*marks)(const CliRunsPaneCfg *args);
} CliRunsPaneNs;
EMBED_TABLE_LAYOUT(CliRunsPaneNs, begin, relist, step, deadline_ms, rows, row, toggle, marks);
EMBED_TABLE_STORAGE CliRunsPaneNs cli_runs_pane EMBED_UNUSED = {
    .begin = cli_runs_pane_begin,
    .relist = cli_runs_pane_relist,
    .step = cli_runs_pane_step,
    .deadline_ms = cli_runs_pane_deadline_ms,
    .rows = cli_runs_pane_rows,
    .row = cli_runs_pane_row,
    .toggle = cli_runs_pane_toggle,
    .marks = cli_runs_pane_marks,
};

#endif
