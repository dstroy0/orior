// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_internal.h: what the modules of the query share: its includes, its bounds, the writer, the run and the
// context a loop runs under, the calls the platforms give (platform/windows.c, platform/posix.c), and the functions
// one module reads from another
#ifndef QUERY_INTERNAL_H
#define QUERY_INTERNAL_H

#include "../../../src/cu/includes/file_defs/qry/qry_read.h"
#include "../../../src/cu/includes/file_defs/qry/writer/qry_member.h"
#include "../../cli/src/command_line/cells.h"
#include "../../cli/src/command_line/file_writing.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "query.h"

// what the query's modules hold in the cellblock, their own count, every cell the query takes and gives back counted
// to it, its report told as `cli_query` (commands.c)
extern CliCellsTally g_query_cells;

// the most files a run reads or leaves, the most settings a context appends, the most words a command takes, and the
// most runs a folder holds that the query walks
#define QUERY_LEAVES_MOST 256u

#define QUERY_SETTINGS_MOST 8u

#define QUERY_WORDS_MOST 64u

#define QUERY_RUNS_MOST 4096u

// the writer's own wait for its shared memory to be made, in steps of ten milliseconds
#define QUERY_READY_STEPS 3000u

// how many runs of a folder are kept where QRY_KEPT is not set
#define QUERY_KEPT_MOST 64u

// the paths a list's room is begun with before it doubles
#define QUERY_PATHS_FIRST 64u

// A list of paths, each in a row of QUERY_PATH_LONGEST, in a cell whose room doubles as they are put, given back with
// query_paths_given
typedef struct
{
    char (*path)[QUERY_PATH_LONGEST];
    unsigned int count;
    unsigned int room;
} QueryPaths;

// room made for one path more in `paths`, `most` at the most: 1, or 0 where it is at the most or its cell is refused;
// room made for `count` paths in all in one cell: 1, or 0 where its cell is refused; and the paths' cell given back,
// the list left empty (tools/tools.c)
int query_paths_room(QueryPaths *paths, unsigned int most);
int query_paths_held(QueryPaths *paths, unsigned int count);
void query_paths_given(QueryPaths *paths);

// The writer of one run: its program, its .qry, the top of the tree every current file is named under, the PATH every
// program of the run runs with, and 1 in `member` while this process is a member of the run
typedef struct
{
    char binary[QUERY_PATH_LONGEST];
    char qry[QUERY_PATH_LONGEST];
    char top[QUERY_PATH_LONGEST];
    const char *path;
    int member;
} QueryWriter;

// One run, started by this process or joined: its folder, its writer, the writer's process where this process started
// it, and the files it read and may leave
typedef struct
{
    char folder[QUERY_PATH_LONGEST];
    QueryWriter writer;
    unsigned long long process;
    int joined;
    QueryPaths read;
    QueryPaths left;
} QueryRun;

// One setting of a context, a record of the run appended before a program starts: its name and its blob
typedef struct
{
    const char *name;
    const void *blob;
    unsigned long long size;
} QuerySetting;

// What every loop runs under: the tree, where its programs are found, where its lines go, how the run's leaves are
// decided, the settings its programs read from the run, and the run a loop of many steps opened, the course's, that
// every step joins; an empty `open` where each loop opens its own
typedef struct
{
    const char *root;
    const QueryPrograms *programs;
    QuerySink sink;
    QueryLeave leave;
    QueryAsker asker;
    QuerySetting setting[QUERY_SETTINGS_MOST];
    unsigned int settings;
    char open[QUERY_PATH_LONGEST];
} QueryContext;

// a process started: its handle, and the pipe its output and its errors come through, 0 where it has none
typedef struct
{
    unsigned long long process;
    unsigned long long output;
} QueryProcess;

// where a started process's output and errors go: nowhere, piped back to this process, where this process's own go,
// or its output alone piped back and its errors nowhere
#define QUERY_OUTPUT_NONE 0

#define QUERY_OUTPUT_PIPED 1

#define QUERY_OUTPUT_OWN 2

#define QUERY_OUTPUT_PIPED_ALONE 3

// The platform's calls (platform/windows.c, platform/posix.c): a process started with `words`, the program first and
// NULL after the last, in `folder`, its standard input nothing and its output and errors where `output` says; a piece
// of its output read, 0 at its end; and the process waited on, its exit code, or -1 where a signal ended it
int query_process_start(QueryProcess *started, const char *const *words, const char *folder, int output);
unsigned long long query_process_read(const QueryProcess *process, char *piece, unsigned long long room);
long long query_process_wait(QueryProcess *process);
// the most bytes of one environment variable's value as UTF-8 with its zero: Windows holds 32767 letters of UTF-16 in
// one, each at most three bytes of UTF-8, and a POSIX system's is no longer than that in practice
#define QUERY_ENVIRONMENT_LONGEST ((32767u * 3u) + 1u)

// an environment variable of this process set, which every process it starts after inherits, or read into `value`,
// which holds `room`; and the variable `name` written after what `out` holds, nothing written where it is not set:
// 1, or 0 where it is not set or does not fit
int query_environment_set(const char *name, const char *value);
int query_environment_read(const char *name, char *value, size_t room);
int query_environment_said(const char *name, CliLine *out);
// each entry of `folder` handed to `each` by its name, 1 where it is a folder; 0 where the folder could not be read
int query_folder_each(const char *folder, void (*each)(void *context, const char *name, int is_folder), void *context);
// `folder` made with every folder above it; the file or the folder at `path` removed whole
int query_folder_made(const char *folder);
int query_removed(const char *path);
// the file at `from` copied whole to `to`; moved to `to`, any file there replaced
int query_file_copied(const char *from, const char *to);
int query_file_renamed(const char *from, const char *to);
// `size` bytes at `bytes` written whole as the file at `path`, for a program outside the run that reads it from the
// disk: a compiler's source
int query_file_written(const char *path, const void *bytes, unsigned long long size);
// 1 where `path` is a file, or a folder; the moment the file at `path` last changed, in seconds since 1970, and the
// moment now, read off the system clock as universal time
int query_is_file(const char *path);
int query_is_folder(const char *path);
int query_modified(const char *path, unsigned long long *seconds);
unsigned long long query_now(void);
// the moment the file at `path` last changed set to `seconds` since 1970: 1, or 0 where it could not be set
int query_modified_set(const char *path, unsigned long long seconds);
// this thread paused `milliseconds`
void query_paused(unsigned int milliseconds);
// `path` as a full path, without the `\\?\` prefix Windows gives a canonical one, into `full`, which holds `room`
int query_path_full(const char *path, char *full, size_t room);
// 1 where the standard input is a terminal; a line read from it into `line`, which holds `room`, as much of it as fits
// and a 0 after, as snprintf writes: the line's whole length, `room` or more where it was cut, or QRY_MEMBER_UNREAD at
// the input's end; `length` bytes of `text` written to the standard output, or its errors
int query_terminal_is(void);
unsigned long long query_terminal_line_read(char *line, size_t room);
void query_terminal_written(int errors, const char *text, unsigned long long length);

// `text` said through the context's sink, and handed to the run this process is a member of as its console
void query_said(QueryContext *context, const char *text);
// the path `under` the top of the tree written into `path`, which holds QUERY_PATH_LONGEST
void query_at(const QueryContext *context, const char *under, char *path);

// the writer's words (writer.c, writer_carry.c)
int query_writer_start(const QueryWriter *writer, unsigned long long *process, CliLine *said);
int query_writer_made(const QueryWriter *writer);
int query_writer_runs(const QueryWriter *writer);
int query_writer_joined(QueryWriter *writer);
int query_writer_member_held(const QueryWriter *writer);
void query_writer_parted(QueryWriter *writer);
int query_writer_stop(QueryWriter *writer, CliLine *said);
int query_writer_append(const QueryWriter *writer, const char *name, const void *bytes, unsigned long long size,
                        unsigned int priority, CliLine *said);
int query_writer_latest(const QueryWriter *writer, const char *name, const unsigned char **bytes,
                        unsigned long long *size);
int query_writer_file(const QueryWriter *writer, const char *path, const void *bytes, unsigned long long size,
                      CliLine *said);
int query_writer_left(const QueryWriter *writer, const char *path, CliLine *said);
int query_writer_carry(const QueryWriter *writer, const char *from, const char *const *names, unsigned int count,
                       CliLine *said);
int query_writer_carry_listed(const QueryWriter *writer, const char *from, const char *listed, CliLine *said);
int query_writer_read(const QueryWriter *writer, const char *name, CliLine *joined);
// every record of `name` joined, as query_writer_read joins them, in a cell of their size with a 0 after, their size
// into `size`: the cell, or NULL where none was there or the cell could not be taken
char *query_writer_read_taken(const QueryWriter *writer, const char *name, unsigned long long *size);
int query_writer_current(const QueryWriter *writer, const char *path, const unsigned char **bytes,
                         unsigned long long *size);
int query_writer_tee(const QueryWriter *writer, QueryContext *context, const char *name, const char *const *words,
                     long long *code, CliLine *said);

// The runs of a folder, newest first: each run's number and path, `count` of them, each in a cell of their count
typedef struct
{
    unsigned long long *number;
    char (*path)[QUERY_PATH_LONGEST];
    unsigned int count;
} QueryRuns;

// the runs of a folder and a run's lifecycle (run.c, run_folder.c); the runs of `folder` counted; the same counted,
// then taken into `runs`: 1, or 0 where the cells cannot meet them; and given back
unsigned int query_runs_each(const char *folder, unsigned long long *number, char (*path)[QUERY_PATH_LONGEST],
                             unsigned int room);
unsigned int query_runs_counted(const char *folder);
int query_runs_taken(const char *folder, QueryRuns *runs);
void query_runs_given(QueryRuns *runs);
void query_next_name(const char *folder, char *name, size_t room);
void query_named(const char *top, const char *path, char *name, size_t room);
int query_opened(const char *held, const char *crystal, char *named, size_t room);
int query_crystal(const char *root, char *program, size_t room);
unsigned int query_kept_most(void);
void query_kept(const char *folder, unsigned int most);
int query_run_started(QueryRun *run, const char *root, const char *folder, const QueryPrograms *programs,
                      QueryContext *context, CliLine *said);
int query_run_joining(QueryRun *run, const char *root, const char *qry, const QueryPrograms *programs, CliLine *said);
int query_run_carried(QueryRun *run, const char *folder, const char *const *files, unsigned int count,
                      const char *crystal, CliLine *said);
int query_run_read(QueryRun *run, const char *const *paths, unsigned int count, CliLine *said);
void query_run_left(QueryRun *run, const char *const *paths, unsigned int count);
int query_run_ended(QueryRun *run, QueryLeave leave, QueryAsker asker, QueryContext *context, CliLine *said);
int query_run_stopped(QueryRun *run, CliLine *said);

// the leaves decided (leaves.c, leaves_diff.c)
int query_leaves_settled(const QueryRun *run, QueryLeave leave, QueryAsker asker, QueryContext *context, CliLine *said);
void query_leaf_shown(const char *path, const unsigned char *tree, unsigned long long tree_size,
                      const unsigned char *held, unsigned long long held_size, CliLine *shown);

// the tools built (tools.c, tools_table.c)
int query_tool_built(const char *root, const QueryPrograms *programs, const char *name, char *binary, size_t room,
                     CliLine *said);
int query_tool_built_into(const char *root, const QueryPrograms *programs, const char *name, const char *out,
                          char *binary, size_t room, CliLine *said);
void query_binary_name(const char *name, char *binary, size_t room);

#endif
