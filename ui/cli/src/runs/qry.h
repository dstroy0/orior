// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry.h: query runs as the window watches them, each a .qry in a folder of runs, read through the run's member and the
// .qry reader (`qry_member.h`, `qry_read.h`), never a copy of them (qry.c reads and appends, qry_queue.c queues the
// backend's records, qry_listing.c lists a tree's runs).
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
typedef struct CliQryCfg CliQryCfg;

// the cells a tree's runs are listed in (qry_listing.c)
extern CellOwner g_cli_qry_listing_cells;

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
// a file the watch of the tree at `root` saw written, `length` bytes of its path from the top of the tree: where it is
// a run, its folder put among the tree's folders of runs; and the watch's changes lost, so the tree is walked again for
// its folders of runs at its next listing (qry_listing.c)
void cli_qry_run_write(const CliQryPathRootCfg *args);
void cli_qry_runs_lose(const CliQryOutRootCfg *args);
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

#endif
