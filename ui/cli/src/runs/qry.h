// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry.h: query runs as the window watches them, each a .qry in a folder of runs, read through the run's member and the
// .qry reader (`qry_member.h`, `qry_read.h`), never a copy of them (qry.c reads and appends, qry_queue.c queues the
// backend's records, qry_listed.c lists a tree's runs).
//
// A run whose writer runs is viewed in its shared memory, read alone: its records are read where the writer put them,
// in the order of the writer's schedule, and each read goes on from the place the last stopped.
//
// A finished run's .qry is its records end to end (qry.h), each held to its CRC; a read of it goes on from the byte the
// last stopped at, and a record not yet whole where a writer still writes it is read by the next read.
//
// What a record says is read for a panic or a critical record, a conference, and a record of at most 4096 bytes, and
// at most 65536 bytes of it; of any other record, its name and its count
#ifndef CLI_QRY_H
#define CLI_QRY_H

#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"

// the cells a tree's runs are listed in (qry_listed.c)
extern CliCellsTally g_cli_qry_listed_cells;

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

// a read of the run at `path`, going on from `from`, written into `out` as `{"live", "blobs": [{"name", "sequence",
// "nanoseconds", "priority", "size", "text": string or null, "whole": bool}...], "next", "stepped", "table",
// "panicked"}`: from its shared memory while its writer runs, the records past place `from` of its schedule, "table"
// false; from its .qry where none does, the records past the byte `from`, "table" true. 1, or 0 with why said
int cli_qry_read(const char *path, unsigned long long from, CliLine *out, CliLine *said);
// a .qry file read going on from the byte `from`, written as cli_qry_read writes it: 1, or 0 with why said
int cli_qry_read_file_from(const char *path, unsigned long long from, CliLine *out, CliLine *said);
// the bytes of every record of the run at `path` named `name`, joined in the order the writer wrote them, put into
// `out`: from its shared memory while its writer runs, from its .qry where none does. 1, or 0 with why said
int cli_qry_joined_bytes(const char *path, const char *name, CliLine *out, CliLine *said);
// the names of the records of the .qry whose bytes are `data`, in the order of their first records, each into
// `names`, which holds `room` of CLI_NAME_LONGEST: their count; and each record of `name` joined into `out`
unsigned int cli_qry_member_names(const unsigned char *data, unsigned long long size, char (*names)[CLI_NAME_LONGEST],
                                  unsigned int room);
void cli_qry_member_joined(const unsigned char *data, unsigned long long size, const char *name, CliLine *out);
// the bytes the blobs of `name` in `size` bytes of a .qry join to
unsigned long long cli_qry_member_size(const unsigned char *data, unsigned long long size, const char *name);
// `size` bytes appended to the run at `path` as a record of `name` at `priority`, the window a member of that run; a
// panic or a critical record waited on until it is on the disk: 1, or 0 with why said
int cli_qry_append(const char *path, const char *name, const void *bytes, unsigned long long size,
                   unsigned int priority, CliLine *said);
// 1 once the writer of the run at `path` has made its shared memory and writes, waiting up to thirty seconds
int cli_qry_ready(const char *path);
// the writer of the run at `path` asked to write the rest of the run and stop, the window a member of the run for the
// asking and of none after: 1, or 0 with why said
int cli_qry_stop(const char *path, CliLine *said);
// the run the window's own work is appended to, into `path`, which holds CLI_PATH_LONGEST: the one QRY names where the
// window was started inside a run, or the one it has last joined to; 1, or 0 where none. And the window joined to the
// run at `path`, or to none where `path` is NULL
int cli_qry_joined(char *path);
void cli_qry_join_window(const char *path);
// `size` bytes queued to be appended to the run at `path` as the record `name` at `priority`, on `channel`; a panic
// or a critical record on the signal channel whatever channel is named, and a record larger than half a channel's
// ring appended before the call returns; and what the queue has done so far written as
// `{"handed", "dumped", "failed", "said"}` (qry_queue.c)
void cli_qry_queue(unsigned int channel, const char *path, const char *name, const void *bytes, unsigned long long size,
                   unsigned int priority);
void cli_qry_queued(CliLine *out);
// whether the run at `path` is live, and the place of the panic on its line where it is
int cli_qry_standing(const char *path, unsigned long long *panicked);
// every run of the tree at `root`, the newest first, and its folders of runs with their lessons, written as
// `{"runs": [{"path", "folder", "size", "changed", "live", "panicked"}...], "folders": [{"folder", "lessons":
// [{"cause", "carrier", "stand_in"}...]}...]}` (qry_listed.c)
void cli_qry_listed(const char *root, CliLine *out);
// the lessons in `size` bytes of an overseer's text, one `lesson <cause> <carrier or *> <stand-in>` a line, each
// written as
// `{"cause", "carrier", "stand_in"}`, a comma between them, for an array's items; any other line passed over
// (qry_listed.c)
void cli_qry_lessons_written(const char *text, unsigned long long size, CliLine *out);

#endif
