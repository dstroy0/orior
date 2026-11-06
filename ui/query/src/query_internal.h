// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_internal.h: what the modules of the query share: its includes, its bounds, the writer, the run and the
// context a loop runs under, the calls the platforms give (platform/windows.c, platform/posix.c), and the functions
// one module reads from another
#ifndef QUERY_INTERNAL_H
#define QUERY_INTERNAL_H
#include "../../../src/cu/includes/file_defs/qry/qry_read.h"
#include "../../../src/cu/includes/file_defs/qry/writer/qry_member.h"
#include "../../cli/src/command_line/callback_cfg.h"
#include "../../cli/src/command_line/pool.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "embed_dispatch_layout.h"
#include "memoria_operor/memoria_operor.h"
#include "query.h"

typedef struct QueryCountNameCtx QueryCountNameCtx;
typedef struct QueryNameCapacityCtx QueryNameCapacityCtx;
typedef struct QueryFolderContextCtx QueryFolderContextCtx;
typedef struct QueryPathTreeCtx QueryPathTreeCtx;
typedef struct QueryCountFolderCtx QueryCountFolderCtx;
typedef struct QueryNameBytesCtx QueryNameBytesCtx;
typedef struct QueryWordsNameCtx QueryWordsNameCtx;
typedef struct QueryContextErrMsgCtx QueryContextErrMsgCtx;
typedef struct QueryContextErrMsgRunCtx QueryContextErrMsgRunCtx;
typedef struct QueryErrMsgRootCtx QueryErrMsgRootCtx;
typedef struct QueryPathBytesSizeCtx QueryPathBytesSizeCtx;
typedef struct QueryCapacityPathNameCtx QueryCapacityPathNameCtx;
typedef struct QueryCapacityQryPathCtx QueryCapacityQryPathCtx;
typedef struct QueryStartedCtx QueryStartedCtx;
typedef struct QueryCountErrMsgCtx QueryCountErrMsgCtx;
typedef struct QueryFolderNumberCtx QueryFolderNumberCtx;
typedef struct QueryFromWriterCtx QueryFromWriterCtx;
typedef struct QueryContextUnderCtx QueryContextUnderCtx;
typedef struct QueryCapacityRootCtx QueryCapacityRootCtx;
typedef struct QueryNameValueCtx QueryNameValueCtx;
typedef struct QueryFolderEachCtx QueryFolderEachCtx;
typedef struct QueryFolderCapacityCtx QueryFolderCapacityCtx;
typedef struct QueryCapacityPathCtx QueryCapacityPathCtx;
typedef struct QueryProcessPieceCtx QueryProcessPieceCtx;
typedef struct QueryErrorsCtx QueryErrorsCtx;
typedef struct QueryWriterCtx QueryWriterCtx;
typedef struct QueryNameCtx QueryNameCtx;
typedef struct QueryNameOutCtx QueryNameOutCtx;
typedef struct QueryFromCtx QueryFromCtx;
typedef struct QueryPathSecondsCtx QueryPathSecondsCtx;
typedef struct QueryPathSecondsKindCtx QueryPathSecondsKindCtx;
typedef struct QueryPathsCtx QueryPathsCtx;
typedef struct QueryTextCtx QueryTextCtx;
typedef struct QueryFolderMaxCtx QueryFolderMaxCtx;
typedef struct QueryFolderRunsCtx QueryFolderRunsCtx;
typedef struct QueryCapacityLineCtx QueryCapacityLineCtx;
typedef struct QueryErrMsgWriterCtx QueryErrMsgWriterCtx;
typedef struct QueryMillisecondsCtx QueryMillisecondsCtx;
typedef struct QueryProcessCtx QueryProcessCtx;

// what the query's modules occupy in the cellblock, their own count, every cell the query takes and gives back counted

// the most files a run reads or leaves, the most settings a context appends, the most words a command takes, and the
// most runs a folder contains that the query walks
#define QUERY_LEAVES_MAX 256u

#define QUERY_SETTINGS_MAX 8u

#define QUERY_WORDS_MAX 64u

#define QUERY_RUNS_MAX 4096u

// the writer's own wait for its shared memory to be made, in steps of ten milliseconds
#define QUERY_READY_STEPS 3000u

// how many runs of a folder are retained where QRY_RETAINED is not set
#define QUERY_RETAIN_MAX 64u

// A list of paths, each in a row of QUERY_PATH_MAX_LEN, in a cell of `capacity` taken at its most with
// query_paths_alloc and released through persistent_buf_release
typedef struct
{
    char (*path)[QUERY_PATH_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
} QueryPaths;

// the cell of `count` paths taken in place of what the list contained: 1, or 0 where it is refused and the list has no
// capacity; and the paths' cell given back, the list left empty (tools/tools.c)
int query_paths_alloc(const QueryPathsCtx *args);

// The writer of one run: its program, its .qry, the top of the tree every current file is named under, the PATH every
// program of the run runs with, and 1 in `member` while this process is a member of the run
typedef struct
{
    char binary[QUERY_PATH_MAX_LEN];
    char qry[QUERY_PATH_MAX_LEN];
    char top[QUERY_PATH_MAX_LEN];
    const char *path;
    int member;
} QueryWriter;

// One run, started by this process or joined: its folder, its writer, the writer's process where this process started
// it, and the files it read and may leave
typedef struct
{
    char folder[QUERY_PATH_MAX_LEN];
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
    QueryPrompter prompter;
    QuerySetting setting[QUERY_SETTINGS_MAX];
    unsigned int settings;
    char open[QUERY_PATH_MAX_LEN];
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
// NULL after the last, in `folder`, its standard input nothing and its output and errors where `output` specifies; a
// piece of its output read, 0 at its end; and the process waited on, its exit code, or -1 where a signal ended it
int query_process_start(const QueryStartedCtx *args);
unsigned long long query_process_read(const QueryProcessPieceCtx *args);
long long query_process_wait(const QueryProcessCtx *args);
// the most bytes of one environment variable's value as UTF-8 with its zero: Windows stores 32767 letters of UTF-16 in
// one, each at most three bytes of UTF-8, and a POSIX system's is no longer than that in practice
#define QUERY_ENVIRONMENT_MAX_LEN ((32767u * 3u) + 1u)

// an environment variable of this process set, which every process it starts after inherits, or read into `value`,
// sized for `capacity`; and the variable `name` written after what `out` contains, nothing written where it is not set:
// 1, or 0 where it is not set or does not fit
int query_environment_set(const QueryNameCtx *args);
int query_environment_read(const QueryNameValueCtx *args);
int query_environment_write(const QueryNameOutCtx *args);
// each entry of `folder` handed to `each` by its name, 1 where it is a folder; 0 where the folder could not be read
int query_folder_each(const QueryFolderEachCtx *args);
// `folder` made with every folder above it; the file or the folder at `path` removed whole, a read-only file among them
int query_folder_init(const QueryFolderContextCtx *args);
int query_remove(const QueryPathSecondsCtx *args);
// the file at `from` copied whole to `to`; moved to `to`, any file there replaced
int query_file_copy(const QueryFromCtx *args);
int query_file_rename(const QueryFromCtx *args);
// `size` bytes at `bytes` written whole as the file at `path`, for a program outside the run that reads it from the
// disk: a compiler's source
int query_file_write(const QueryPathBytesSizeCtx *args);
// 1 where `path` is a file, or a folder; the moment the file at `path` last changed, in seconds since 1970, and the
// moment now, read off the system clock as universal time
int query_is_file(const QueryPathSecondsCtx *args);
int query_is_folder(const QueryPathSecondsCtx *args);
int query_modify(const QueryPathSecondsCtx *args);
unsigned long long query_now(void);
// the moment the file at `path` last changed set to `seconds` since 1970: 1, or 0 where it could not be set
int query_modified_set(const QueryPathSecondsKindCtx *args);
// this thread paused `milliseconds`
void query_pause(const QueryMillisecondsCtx *args);
// `path` as a full path, without the `\\?\` prefix Windows gives a canonical one, into `full`, sized for `capacity`
int query_path_full(const QueryCapacityPathCtx *args);
// 1 where the standard input is a terminal; a line read from it into `line`, sized for `capacity`, as much of it as
// fits and a 0 after, as snprintf writes: the line's whole length, `capacity` or more where it was cut, or
// QRY_MEMBER_UNREAD at the input's end; `length` bytes of `text` written to the standard output, or its errors
int query_terminal_is(void);
unsigned long long query_terminal_line_read(const QueryCapacityLineCtx *args);
void query_terminal_write(const QueryErrorsCtx *args);

// `text` printed through the context's sink, and handed to the run this process is a member of as its console
void query_print(const QueryTextCtx *args);
// the path `under` the top of the tree written into `path`, sized for QUERY_PATH_MAX_LEN
void query_at(const QueryContextUnderCtx *args);

// the writer's words (writer.c, writer_machine_transfer.c)
int query_writer_start(const QueryWriterCtx *args);
int query_writer_init(const QueryWriterCtx *args);
int query_writer_runs(const QueryWriterCtx *args);
int query_writer_join(const QueryErrMsgWriterCtx *args);
int query_writer_is_member(const QueryCountNameCtx *args);
void query_writer_part(const QueryErrMsgWriterCtx *args);
int query_writer_stop(const QueryErrMsgWriterCtx *args);
int query_writer_append(const QueryNameBytesCtx *args);
int query_writer_latest(const QueryCountNameCtx *args);
int query_writer_file(const QueryPathBytesSizeCtx *args);
int query_writer_leave(const QueryPathBytesSizeCtx *args);
int query_writer_machine_transfer(const QueryCountNameCtx *args);
int query_writer_machine_transfer_list(const QueryFromWriterCtx *args);
int query_writer_read(const QueryCountNameCtx *args);
// every record of `name` joined, as query_writer_read joins them, in a cell of their size with a 0 after, their size
// into `size`: the cell, or NULL where none was there or the cell could not be taken
char *query_writer_read_alloc(const QueryCountNameCtx *args);
int query_writer_current(const QueryCountNameCtx *args);
int query_writer_tee(const QueryWordsNameCtx *args);

// The runs of a folder, newest first: each run's number and path, `count` of them, each in a cell of their count
typedef struct
{
    unsigned long long *number;
    char (*path)[QUERY_PATH_MAX_LEN];
    unsigned int count;
} QueryRuns;

// the runs of a folder and a run's lifecycle (run.c, run_folder.c); the runs of `folder` counted; the same counted,
// then taken into `runs`: 1, or 0 where the cells cannot meet them; and given back
unsigned int query_runs_each(const QueryFolderNumberCtx *args);
unsigned int query_runs_count(const QueryFolderMaxCtx *args);
int query_runs_alloc(const QueryFolderRunsCtx *args);
void query_next_name(const QueryFolderCapacityCtx *args);
void query_by_name(const QueryCapacityPathNameCtx *args);
int query_open(const QueryCapacityQryPathCtx *args);
int query_crystal(const QueryCapacityRootCtx *args);
unsigned int query_retain_max(void);
void query_runs_prune(const QueryFolderMaxCtx *args);
int query_run_start(const QueryFolderContextCtx *args);
int query_run_joining(const QueryErrMsgRootCtx *args);
int query_run_machine_transfer(const QueryCountFolderCtx *args);
int query_run_read(const QueryCountErrMsgCtx *args);
void query_run_leave(const QueryCountErrMsgCtx *args);
int query_run_end(const QueryContextErrMsgRunCtx *args);
int query_run_stop(const QueryContextErrMsgRunCtx *args);

// the leaves decided (leaves.c, leaves_diff.c)
int query_leaves_settle(const QueryContextErrMsgCtx *args);
void query_leaf_show(const QueryPathTreeCtx *args);

// the tools built (tools.c, tools_table.c)
int query_tool_build(const QueryNameCapacityCtx *args);
int query_tool_built_into(const QueryNameCapacityCtx *args);
void query_binary_name(const QueryNameCapacityCtx *args);

struct QueryCountNameCtx
{
    unsigned int count;
    const char *name;
    const char *path;
    const char *from;
    const QueryWriter *writer;
    VerbaLinea *err_msg;
    const unsigned char **bytes_u8s;
    unsigned long long *size_pointer;
    const char *const *names;
    VerbaLinea *joined;
};

struct QueryNameCapacityCtx
{
    const char *name;
    size_t capacity;
    VerbaLinea *err_msg;
    const char *root;
    const QueryPrograms *programs;
    char *binary;
    const char *out_text;
};

struct QueryFolderContextCtx
{
    const char *folder;
    QueryContext *context;
    VerbaLinea *err_msg;
    const char *root;
    QueryRun *run;
    const QueryPrograms *programs;
};

struct QueryPathTreeCtx
{
    const char *path;
    const unsigned char *tree;
    unsigned long long tree_size;
    const unsigned char *stored;
    unsigned long long stored_size;
    VerbaLinea *displayed;
};

struct QueryCountFolderCtx
{
    unsigned int count;
    const char *folder;
    VerbaLinea *err_msg;
    const char *crystal;
    QueryRun *run;
    const char *const *files;
};

struct QueryNameBytesCtx
{
    const char *name;
    const void *bytes;
    unsigned long long size;
    const QueryWriter *writer;
    VerbaLinea *err_msg;
    unsigned int priority;
};

struct QueryWordsNameCtx
{
    const char *const *words;
    const char *name;
    QueryContext *context;
    const QueryWriter *writer;
    VerbaLinea *err_msg;
    long long *code;
};

struct QueryContextErrMsgCtx
{
    QueryContext *context;
    VerbaLinea *err_msg;
    QueryLeave leave;
    QueryPrompter prompter;
    const QueryRun *run_read;
};

struct QueryContextErrMsgRunCtx
{
    QueryContext *context;
    VerbaLinea *err_msg;
    QueryRun *run;
    QueryLeave leave;
    QueryPrompter prompter;
};

struct QueryErrMsgRootCtx
{
    VerbaLinea *err_msg;
    const char *root;
    QueryRun *run;
    const QueryPrograms *programs;
    const char *qry;
};

struct QueryPathBytesSizeCtx
{
    const char *path;
    const void *bytes;
    unsigned long long size;
    const QueryWriter *writer;
    VerbaLinea *err_msg;
};

struct QueryCapacityPathNameCtx
{
    size_t capacity;
    const char *path;
    char *name_out;
    const char *top;
};

struct QueryCapacityQryPathCtx
{
    size_t capacity;
    const char *qry_path;
    const char *crystal;
    char *named;
};

struct QueryStartedCtx
{
    QueryProcess *started;
    const char *const *words;
    const char *folder;
    int output;
};

struct QueryCountErrMsgCtx
{
    unsigned int count;
    VerbaLinea *err_msg;
    QueryRun *run;
    const char *const *paths_letters;
};

struct QueryFolderNumberCtx
{
    const char *folder;
    unsigned long long *number;
    char (*path_callback)[QUERY_PATH_MAX_LEN];
    unsigned int capacity_u32;
};

struct QueryFromWriterCtx
{
    const char *from;
    const QueryWriter *writer;
    VerbaLinea *err_msg;
    const char *list_entries;
};

struct QueryContextUnderCtx
{
    const QueryContext *context_read;
    const char *under;
    char *path_out;
};

struct QueryCapacityRootCtx
{
    size_t capacity;
    const char *root;
    char *program;
};

struct QueryNameValueCtx
{
    const char *name;
    char *value_out;
    size_t capacity;
};

struct QueryFolderEachCtx
{
    const char *folder;
    void (*each)(const CliEachNameIsFolderCfg *args);
    void *context_bytess;
};

struct QueryFolderCapacityCtx
{
    const char *folder;
    size_t capacity;
    char *name_out;
};

struct QueryCapacityPathCtx
{
    size_t capacity;
    const char *path;
    char *full;
};

struct QueryProcessPieceCtx
{
    const QueryProcess *process;
    char *piece;
    unsigned long long capacity_u64;
};

struct QueryErrorsCtx
{
    int errors;
    const char *text;
    unsigned long long length;
};

struct QueryWriterCtx
{
    const QueryWriter *writer;
    unsigned long long *process_u64s;
    VerbaLinea *err_msg;
};

struct QueryNameCtx
{
    const char *name;
    const char *value;
};

struct QueryNameOutCtx
{
    const char *name;
    VerbaLinea *out;
};

struct QueryFromCtx
{
    const char *from;
    const char *to;
};

struct QueryPathSecondsCtx
{
    const char *path;
    unsigned long long *seconds;
};

struct QueryPathSecondsKindCtx
{
    const char *path;
    unsigned long long seconds_value;
};

struct QueryPathsCtx
{
    QueryPaths *paths;
    unsigned int count;
};

struct QueryTextCtx
{
    const char *text;
    QueryContext *context;
};

struct QueryFolderMaxCtx
{
    const char *folder;
    unsigned int most;
};

struct QueryFolderRunsCtx
{
    const char *folder;
    QueryRuns *runs;
};

struct QueryCapacityLineCtx
{
    size_t capacity;
    char *line;
};

struct QueryErrMsgWriterCtx
{
    VerbaLinea *err_msg;
    QueryWriter *writer_out;
};

struct QueryMillisecondsCtx
{
    unsigned int milliseconds;
};

struct QueryProcessCtx
{
    QueryProcess *process_out;
};

typedef struct
{
    int (*paths_alloc)(const QueryPathsCtx *args);
} QueryPathsNs;
EMBED_TABLE_LAYOUT(QueryPathsNs, paths_alloc);
EMBED_TABLE_STORAGE QueryPathsNs query_paths_internal EMBED_UNUSED = {
    .paths_alloc = query_paths_alloc,
};

typedef struct
{
    int (*process_start)(const QueryStartedCtx *args);
} QueryStartedNs;
EMBED_TABLE_LAYOUT(QueryStartedNs, process_start);
EMBED_TABLE_STORAGE QueryStartedNs query_started_internal EMBED_UNUSED = {
    .process_start = query_process_start,
};

typedef struct
{
    unsigned long long (*process_read)(const QueryProcessPieceCtx *args);
} QueryProcessPieceNs;
EMBED_TABLE_LAYOUT(QueryProcessPieceNs, process_read);
EMBED_TABLE_STORAGE QueryProcessPieceNs query_process_piece_internal EMBED_UNUSED = {
    .process_read = query_process_read,
};

typedef struct
{
    long long (*process_wait)(const QueryProcessCtx *args);
} QueryProcessNs;
EMBED_TABLE_LAYOUT(QueryProcessNs, process_wait);
EMBED_TABLE_STORAGE QueryProcessNs query_process_internal EMBED_UNUSED = {
    .process_wait = query_process_wait,
};

typedef struct
{
    int (*environment_set)(const QueryNameCtx *args);
} QueryNameNsTable;
EMBED_TABLE_LAYOUT(QueryNameNsTable, environment_set);
EMBED_TABLE_STORAGE QueryNameNsTable query_name_internal EMBED_UNUSED = {
    .environment_set = query_environment_set,
};

typedef struct
{
    int (*environment_read)(const QueryNameValueCtx *args);
} QueryNameValueNs;
EMBED_TABLE_LAYOUT(QueryNameValueNs, environment_read);
EMBED_TABLE_STORAGE QueryNameValueNs query_name_value_internal EMBED_UNUSED = {
    .environment_read = query_environment_read,
};

typedef struct
{
    int (*environment_write)(const QueryNameOutCtx *args);
} QueryNameOutNs;
EMBED_TABLE_LAYOUT(QueryNameOutNs, environment_write);
EMBED_TABLE_STORAGE QueryNameOutNs query_name_out_internal EMBED_UNUSED = {
    .environment_write = query_environment_write,
};

typedef struct
{
    int (*folder_each)(const QueryFolderEachCtx *args);
} QueryFolderEachNs;
EMBED_TABLE_LAYOUT(QueryFolderEachNs, folder_each);
EMBED_TABLE_STORAGE QueryFolderEachNs query_folder_each_internal EMBED_UNUSED = {
    .folder_each = query_folder_each,
};

typedef struct
{
    int (*folder_init)(const QueryFolderContextCtx *args);
    int (*run_start)(const QueryFolderContextCtx *args);
} QueryFolderContextNs;
EMBED_TABLE_LAYOUT(QueryFolderContextNs, folder_init, run_start);
EMBED_TABLE_STORAGE QueryFolderContextNs query_folder_context_internal EMBED_UNUSED = {
    .folder_init = query_folder_init,
    .run_start = query_run_start,
};

typedef struct
{
    int (*remove)(const QueryPathSecondsCtx *args);
    int (*is_file)(const QueryPathSecondsCtx *args);
    int (*is_folder)(const QueryPathSecondsCtx *args);
    int (*modify)(const QueryPathSecondsCtx *args);
} QueryPathSecondsNs;
EMBED_TABLE_LAYOUT(QueryPathSecondsNs, remove, is_file, is_folder, modify);
EMBED_TABLE_STORAGE QueryPathSecondsNs query_path_seconds_internal EMBED_UNUSED = {
    .remove = query_remove,
    .is_file = query_is_file,
    .is_folder = query_is_folder,
    .modify = query_modify,
};

typedef struct
{
    int (*file_copy)(const QueryFromCtx *args);
    int (*file_rename)(const QueryFromCtx *args);
} QueryFromNs;
EMBED_TABLE_LAYOUT(QueryFromNs, file_copy, file_rename);
EMBED_TABLE_STORAGE QueryFromNs query_from_internal EMBED_UNUSED = {
    .file_copy = query_file_copy,
    .file_rename = query_file_rename,
};

typedef struct
{
    int (*file_write)(const QueryPathBytesSizeCtx *args);
    int (*writer_file)(const QueryPathBytesSizeCtx *args);
    int (*writer_leave)(const QueryPathBytesSizeCtx *args);
} QueryPathBytesSizeNs;
EMBED_TABLE_LAYOUT(QueryPathBytesSizeNs, file_write, writer_file, writer_leave);
EMBED_TABLE_STORAGE QueryPathBytesSizeNs query_path_bytes_size_internal EMBED_UNUSED = {
    .file_write = query_file_write,
    .writer_file = query_writer_file,
    .writer_leave = query_writer_leave,
};

typedef struct
{
    int (*modified_set)(const QueryPathSecondsKindCtx *args);
} QueryPathSecondsKindNs;
EMBED_TABLE_LAYOUT(QueryPathSecondsKindNs, modified_set);
EMBED_TABLE_STORAGE QueryPathSecondsKindNs query_path_seconds_kind_internal EMBED_UNUSED = {
    .modified_set = query_modified_set,
};

typedef struct
{
    void (*pause)(const QueryMillisecondsCtx *args);
} QueryMillisecondsNs;
EMBED_TABLE_LAYOUT(QueryMillisecondsNs, pause);
EMBED_TABLE_STORAGE QueryMillisecondsNs query_milliseconds_internal EMBED_UNUSED = {
    .pause = query_pause,
};

typedef struct
{
    int (*path_full)(const QueryCapacityPathCtx *args);
} QueryCapacityPathNs;
EMBED_TABLE_LAYOUT(QueryCapacityPathNs, path_full);
EMBED_TABLE_STORAGE QueryCapacityPathNs query_capacity_path_internal EMBED_UNUSED = {
    .path_full = query_path_full,
};

typedef struct
{
    unsigned long long (*terminal_line_read)(const QueryCapacityLineCtx *args);
} QueryCapacityLineNs;
EMBED_TABLE_LAYOUT(QueryCapacityLineNs, terminal_line_read);
EMBED_TABLE_STORAGE QueryCapacityLineNs query_capacity_line_internal EMBED_UNUSED = {
    .terminal_line_read = query_terminal_line_read,
};

typedef struct
{
    void (*terminal_write)(const QueryErrorsCtx *args);
} QueryErrorsNs;
EMBED_TABLE_LAYOUT(QueryErrorsNs, terminal_write);
EMBED_TABLE_STORAGE QueryErrorsNs query_errors_internal EMBED_UNUSED = {
    .terminal_write = query_terminal_write,
};

typedef struct
{
    void (*print)(const QueryTextCtx *args);
} QueryTextNsTable;
EMBED_TABLE_LAYOUT(QueryTextNsTable, print);
EMBED_TABLE_STORAGE QueryTextNsTable query_text_internal EMBED_UNUSED = {
    .print = query_print,
};

typedef struct
{
    void (*at)(const QueryContextUnderCtx *args);
} QueryContextUnderNs;
EMBED_TABLE_LAYOUT(QueryContextUnderNs, at);
EMBED_TABLE_STORAGE QueryContextUnderNs query_context_under_internal EMBED_UNUSED = {
    .at = query_at,
};

typedef struct
{
    int (*writer_start)(const QueryWriterCtx *args);
    int (*writer_init)(const QueryWriterCtx *args);
    int (*writer_runs)(const QueryWriterCtx *args);
} QueryWriterNs;
EMBED_TABLE_LAYOUT(QueryWriterNs, writer_start, writer_init, writer_runs);
EMBED_TABLE_STORAGE QueryWriterNs query_writer_internal EMBED_UNUSED = {
    .writer_start = query_writer_start,
    .writer_init = query_writer_init,
    .writer_runs = query_writer_runs,
};

typedef struct
{
    int (*writer_join)(const QueryErrMsgWriterCtx *args);
    void (*writer_part)(const QueryErrMsgWriterCtx *args);
    int (*writer_stop)(const QueryErrMsgWriterCtx *args);
} QueryErrMsgWriterNs;
EMBED_TABLE_LAYOUT(QueryErrMsgWriterNs, writer_join, writer_part, writer_stop);
EMBED_TABLE_STORAGE QueryErrMsgWriterNs query_err_msg_writer_internal EMBED_UNUSED = {
    .writer_join = query_writer_join,
    .writer_part = query_writer_part,
    .writer_stop = query_writer_stop,
};

typedef struct
{
    int (*writer_is_member)(const QueryCountNameCtx *args);
    int (*writer_latest)(const QueryCountNameCtx *args);
    int (*writer_machine_transfer)(const QueryCountNameCtx *args);
    int (*writer_read)(const QueryCountNameCtx *args);
    char *(*writer_read_alloc)(const QueryCountNameCtx *args);
    int (*writer_current)(const QueryCountNameCtx *args);
} QueryCountNameNs;
EMBED_TABLE_LAYOUT(QueryCountNameNs, writer_is_member, writer_latest, writer_machine_transfer, writer_read,
                   writer_read_alloc, writer_current);
EMBED_TABLE_STORAGE QueryCountNameNs query_count_name_internal EMBED_UNUSED = {
    .writer_is_member = query_writer_is_member,
    .writer_latest = query_writer_latest,
    .writer_machine_transfer = query_writer_machine_transfer,
    .writer_read = query_writer_read,
    .writer_read_alloc = query_writer_read_alloc,
    .writer_current = query_writer_current,
};

typedef struct
{
    int (*writer_append)(const QueryNameBytesCtx *args);
} QueryNameBytesNs;
EMBED_TABLE_LAYOUT(QueryNameBytesNs, writer_append);
EMBED_TABLE_STORAGE QueryNameBytesNs query_name_bytes_internal EMBED_UNUSED = {
    .writer_append = query_writer_append,
};

typedef struct
{
    int (*writer_machine_transfer_list)(const QueryFromWriterCtx *args);
} QueryFromWriterNs;
EMBED_TABLE_LAYOUT(QueryFromWriterNs, writer_machine_transfer_list);
EMBED_TABLE_STORAGE QueryFromWriterNs query_from_writer_internal EMBED_UNUSED = {
    .writer_machine_transfer_list = query_writer_machine_transfer_list,
};

typedef struct
{
    int (*writer_tee)(const QueryWordsNameCtx *args);
} QueryWordsNameNs;
EMBED_TABLE_LAYOUT(QueryWordsNameNs, writer_tee);
EMBED_TABLE_STORAGE QueryWordsNameNs query_words_name_internal EMBED_UNUSED = {
    .writer_tee = query_writer_tee,
};

typedef struct
{
    unsigned int (*runs_each)(const QueryFolderNumberCtx *args);
} QueryFolderNumberNs;
EMBED_TABLE_LAYOUT(QueryFolderNumberNs, runs_each);
EMBED_TABLE_STORAGE QueryFolderNumberNs query_folder_number_internal EMBED_UNUSED = {
    .runs_each = query_runs_each,
};

typedef struct
{
    unsigned int (*runs_count)(const QueryFolderMaxCtx *args);
    void (*runs_prune)(const QueryFolderMaxCtx *args);
} QueryFolderMaxNs;
EMBED_TABLE_LAYOUT(QueryFolderMaxNs, runs_count, runs_prune);
EMBED_TABLE_STORAGE QueryFolderMaxNs query_folder_max_internal EMBED_UNUSED = {
    .runs_count = query_runs_count,
    .runs_prune = query_runs_prune,
};

typedef struct
{
    int (*runs_alloc)(const QueryFolderRunsCtx *args);
} QueryFolderRunsNs;
EMBED_TABLE_LAYOUT(QueryFolderRunsNs, runs_alloc);
EMBED_TABLE_STORAGE QueryFolderRunsNs query_folder_runs_internal EMBED_UNUSED = {
    .runs_alloc = query_runs_alloc,
};

typedef struct
{
    void (*next_name)(const QueryFolderCapacityCtx *args);
} QueryFolderCapacityNs;
EMBED_TABLE_LAYOUT(QueryFolderCapacityNs, next_name);
EMBED_TABLE_STORAGE QueryFolderCapacityNs query_folder_capacity_internal EMBED_UNUSED = {
    .next_name = query_next_name,
};

typedef struct
{
    void (*by_name)(const QueryCapacityPathNameCtx *args);
} QueryCapacityPathNameNs;
EMBED_TABLE_LAYOUT(QueryCapacityPathNameNs, by_name);
EMBED_TABLE_STORAGE QueryCapacityPathNameNs query_capacity_path_name_internal EMBED_UNUSED = {
    .by_name = query_by_name,
};

typedef struct
{
    int (*open)(const QueryCapacityQryPathCtx *args);
} QueryCapacityQryPathNs;
EMBED_TABLE_LAYOUT(QueryCapacityQryPathNs, open);
EMBED_TABLE_STORAGE QueryCapacityQryPathNs query_capacity_qry_path_internal EMBED_UNUSED = {
    .open = query_open,
};

typedef struct
{
    int (*crystal)(const QueryCapacityRootCtx *args);
} QueryCapacityRootNs;
EMBED_TABLE_LAYOUT(QueryCapacityRootNs, crystal);
EMBED_TABLE_STORAGE QueryCapacityRootNs query_capacity_root_internal EMBED_UNUSED = {
    .crystal = query_crystal,
};

typedef struct
{
    int (*run_joining)(const QueryErrMsgRootCtx *args);
} QueryErrMsgRootNs;
EMBED_TABLE_LAYOUT(QueryErrMsgRootNs, run_joining);
EMBED_TABLE_STORAGE QueryErrMsgRootNs query_err_msg_root_internal EMBED_UNUSED = {
    .run_joining = query_run_joining,
};

typedef struct
{
    int (*run_machine_transfer)(const QueryCountFolderCtx *args);
} QueryCountFolderNs;
EMBED_TABLE_LAYOUT(QueryCountFolderNs, run_machine_transfer);
EMBED_TABLE_STORAGE QueryCountFolderNs query_count_folder_internal EMBED_UNUSED = {
    .run_machine_transfer = query_run_machine_transfer,
};

typedef struct
{
    int (*run_read)(const QueryCountErrMsgCtx *args);
    void (*run_leave)(const QueryCountErrMsgCtx *args);
} QueryCountErrMsgNs;
EMBED_TABLE_LAYOUT(QueryCountErrMsgNs, run_read, run_leave);
EMBED_TABLE_STORAGE QueryCountErrMsgNs query_count_err_msg_internal EMBED_UNUSED = {
    .run_read = query_run_read,
    .run_leave = query_run_leave,
};

typedef struct
{
    int (*run_end)(const QueryContextErrMsgRunCtx *args);
    int (*run_stop)(const QueryContextErrMsgRunCtx *args);
} QueryContextErrMsgRunNs;
EMBED_TABLE_LAYOUT(QueryContextErrMsgRunNs, run_end, run_stop);
EMBED_TABLE_STORAGE QueryContextErrMsgRunNs query_context_err_msg_run_internal EMBED_UNUSED = {
    .run_end = query_run_end,
    .run_stop = query_run_stop,
};

typedef struct
{
    int (*leaves_settle)(const QueryContextErrMsgCtx *args);
} QueryContextErrMsgNs;
EMBED_TABLE_LAYOUT(QueryContextErrMsgNs, leaves_settle);
EMBED_TABLE_STORAGE QueryContextErrMsgNs query_context_err_msg_internal EMBED_UNUSED = {
    .leaves_settle = query_leaves_settle,
};

typedef struct
{
    void (*leaf_show)(const QueryPathTreeCtx *args);
} QueryPathTreeNs;
EMBED_TABLE_LAYOUT(QueryPathTreeNs, leaf_show);
EMBED_TABLE_STORAGE QueryPathTreeNs query_path_tree_internal EMBED_UNUSED = {
    .leaf_show = query_leaf_show,
};

typedef struct
{
    int (*tool_build)(const QueryNameCapacityCtx *args);
    int (*tool_built_into)(const QueryNameCapacityCtx *args);
    void (*binary_name)(const QueryNameCapacityCtx *args);
} QueryNameCapacityNs;
EMBED_TABLE_LAYOUT(QueryNameCapacityNs, tool_build, tool_built_into, binary_name);
EMBED_TABLE_STORAGE QueryNameCapacityNs query_name_capacity_internal EMBED_UNUSED = {
    .tool_build = query_tool_build,
    .tool_built_into = query_tool_built_into,
    .binary_name = query_binary_name,
};

#endif
