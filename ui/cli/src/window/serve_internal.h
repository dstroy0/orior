// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_internal.h: what the server's sources share: the tree open and where what the server emits goes, the comms
// keys it sends and is reached with, the door's address, the database connections and kernels open; a call's
// arguments as they are read; and the tables of calls each source answers
#ifndef SERVE_INTERNAL_H
#define SERVE_INTERNAL_H
typedef struct CliServeErrMsgCtx CliServeErrMsgCtx;
typedef struct CliServeSizeCtx CliServeSizeCtx;
typedef struct CliServeErrMsgOutCtx CliServeErrMsgOutCtx;
typedef struct CliServeErrMsgRequestCtx CliServeErrMsgRequestCtx;
typedef struct CliServeErrMsgRequestRootCtx CliServeErrMsgRequestRootCtx;
typedef struct CliServeIdCtx CliServeIdCtx;
typedef struct CliServeErrMsgOutRequestCtx CliServeErrMsgOutRequestCtx;
typedef struct CliServeErrMsgPathCtx CliServeErrMsgPathCtx;
typedef struct CliServeSizeErrMsgCtx CliServeSizeErrMsgCtx;
typedef struct CliServeBaseCtx CliServeBaseCtx;
typedef struct CliServeEventCtx CliServeEventCtx;
typedef struct CliServeErrMsgRequestFolderCtx CliServeErrMsgRequestFolderCtx;
typedef struct CliServeErrMsgPathOutCtx CliServeErrMsgPathOutCtx;
typedef struct CliServeErrMsgRootCtx CliServeErrMsgRootCtx;
typedef struct CliServeCtx CliServeCtx;
typedef struct CliServeListedCtx CliServeListedCtx;
typedef struct CliServeItemsCtx CliServeItemsCtx;

#include "../command_line/catalog.h"
#include "../command_line/cells.h"
#include "../comms/comms.h"
#include "../editor/files.h"
#include "../servers/servers_internal.h"
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"
#include "serve.h"

// The server, under its lock: the tree open, the folder the runs started over a link are stored in, where what it emits
// goes before the comms layer carries it, the key it sends on every channel with, the persistent key the command line
// joins the door with, and the door's address
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    int has_root;
    char detached_dir[CLI_PATH_MAX_LEN];
    int has_detached_dir;
    CliServeEmit emit;
    void *emit_context;
    int has_emit;
    char sender[CLI_COMMS_KEY];
    int sending;
    char command_line[CLI_COMMS_KEY];
    int has_command_line;
    char door[CLI_PATH_MAX_LEN];
    int has_door;
} CliServeState;

extern CliServeState g_cli_serve;

// what the server emits sent where it goes, or nowhere before it was given where (serve.c)
void cli_serve_on_emit(const CliServeEventCtx *args);
// where what the servers of `tree` emit goes, each path as it stands in `tree`
CliServersSink cli_serve_emitter(const CliServeCtx *args);
// the page's answer to the command of `id` sent to the command line that requested it: 1, or 0 with why reported
int cli_serve_command_answer(const CliServeIdCtx *args);
// the tree at `path` made the tree open, a tree that moves letting go of every kernel, database connection, mount,
// language server and symbol of the one before and watching the new one: its full path into `out`, 1, or 0 with why
// given
int cli_serve_root_set(const CliServeErrMsgPathOutCtx *args);

// every database connection closed; and every kernel the notebooks ran on ended (serve_databases.c, serve_kernels.c)
void cli_serve_databases_close(void);
void cli_serve_kernels_stop(void);

// A call's arguments: the tree they are read into and the object that contains them
typedef struct
{
    const CliJson *json;
    unsigned int object;
} CliServeArgs;

// the text of the argument `key`, where it is one, zeroed where it lies: a secret a call carries
void cli_serve_secret_wiped(const CliServeErrMsgCtx *args);

// the argument `key`, by its own name or by the page's, CLI_JSON_NONE where there is none; a string argument, NULL with
// why given where it is none; the same, `otherwise` where it is none or null; a whole number, 1, or 0 with why given;
// the same, `otherwise` where it is none or null; a whole number that may be below 0; whether a boolean is true,
// `otherwise` where it is none or null; and a boolean, 1, or 0 with why given (serve_args.c)
unsigned int cli_serve_arg(const CliServeErrMsgCtx *args);
const char *cli_serve_string(const CliServeErrMsgCtx *args);
const char *cli_serve_string_or(const CliServeErrMsgCtx *args);
int cli_serve_whole(const CliServeErrMsgCtx *args);
unsigned long long cli_serve_whole_or(const CliServeErrMsgCtx *args);
int cli_serve_sign(const CliServeErrMsgCtx *args);
int cli_serve_bool_or(const CliServeErrMsgCtx *args);
int cli_serve_bool(const CliServeErrMsgCtx *args);
// why the argument at `node`, named `key`, of the wrong kind is refused, as serde reports it: its name, what it is, and
// `expected`
void cli_serve_reject(const CliServeErrMsgRequestCtx *args);
// the strings of an array argument into `items`, sized for `room`: their count into `count`, 1, or 0 with why given
// where it is no array of strings; the same where none or null is an empty array
int cli_serve_strings(const CliServeErrMsgCtx *args);
int cli_serve_strings_or(const CliServeErrMsgCtx *args);
// the strings of an array argument, `most` of them at the most, in a cell of their count and one more, the place after
// the last NULL, into `*items`, given back with cli_serve_strings_free either way: their count into `count`, 1, or 0
// with why given where it is no array of strings or the cellblock could not meet it; and the cells those calls occupy
extern CellOwner g_cli_serve_strings_cells;
int cli_serve_strings_take(const CliServeErrMsgCtx *args);
void cli_serve_strings_free(const CliServeItemsCtx *args);
// the tree open into `root`, 1, or 0 with why given; and the argument `path` joined to it into `full`, each of which
// is sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
int cli_serve_tree(const CliServeErrMsgRequestRootCtx *args);
int cli_serve_at(const CliServeErrMsgRequestRootCtx *args);
// a cell of `size` for one call's own use, named `what`: it, or NULL with why given where the cells cannot meet it;
// given back with g_cli_cells->persistent_buf_release (serve_args.c)
void *cli_serve_cell(const CliServeSizeErrMsgCtx *args);

// The tree's files laid flat for one call, and a pointer to each, `count` of them in the same cell
typedef struct
{
    const CliFiles *files;
    unsigned int count;
    const char **paths;
} CliServeFiles;

// the files of the tree at `root` laid flat and a pointer to each listed into a cell: it, or NULL with why given; and
// the list and the cell given back, NULL given back as nothing (serve_args.c)
CliServeFiles *cli_serve_files_list(const CliServeErrMsgRootCtx *args);
void cli_serve_files_free(const CliServeListedCtx *args);
// a place argument `{line, col}` read into `place`: 1, or 0 with why given
int cli_serve_place(const CliServeErrMsgCtx *args);

// the tree's catalog read for the page into a cell, its lock taken: the catalog, or NULL with why given and the lock
// given back where no room for it is left; and the cell and its lock given back; the values of the
// argument `values`, an object of each param's values, read, under the catalog's lock; a job started on this machine,
// its lines going to the page fifty milliseconds' worth at a time: its run's number written, 1, or 0 with why given;
// and the tree's jobs as the window lists them, each with how many steps it runs (serve_jobs.c)
CliCatalog *cli_serve_catalog_take(const CliServeErrMsgRootCtx *args);
void cli_serve_catalog_free(void);
CliValues *cli_serve_values_get(const CliServeErrMsgOutCtx *args);
int cli_serve_job_start(const CliServeErrMsgOutCtx *args);
// the gatherings of the runs started here that are open
extern CellOwner g_cli_serve_gathering_cells;
int cli_serve_catalog_read(const CliServeErrMsgOutRequestCtx *args);

// the most bytes of a line a detached run prints; and the file of the detached run `base` of `kind`, `job`, `output`,
// `pid` or `stop`, into `path`, sized for CLI_PATH_MAX_LEN (serve_detached_run.c)
#define CLI_SERVE_DETACHED_LINE (1u << 20u)
void cli_serve_detached_file(const CliServeBaseCtx *args);
// `job` started as a detached run in the folder `detached_dir`, a process of its own whose lines are stored on this
// machine, and followed: its number written, 1, or 0 with why given; the detached run of the argument `run` followed;
// the detached runs of the tree open listed, the oldest first, each `{"run", "job", "ended"}`, those retained a week
// let go; and a run stopped, a detached run by its stop file (serve_detached.c)
int cli_serve_detached_start(const CliServeErrMsgOutCtx *args);
typedef struct
{
    const CliServeArgs *request;
    VerbaLinea *out;
    VerbaLinea *err_msg;
} CliServeAnswerCfg;

int cli_serve_run_follow(const CliServeAnswerCfg *args);
int cli_serve_runs_detach(const CliServeAnswerCfg *args);
int cli_serve_job_stop(const CliServeAnswerCfg *args);

// A call the server answers: its name and the function that answers it, its answer as JSON into `out`, 1, or 0 with
// why given
typedef int (*CliServeAnswer)(const CliServeAnswerCfg *args);
typedef struct
{
    const char *name;
    CliServeAnswer answer;
} CliServeCall;

// `answer` given `size` bytes of its arguments as a JSON object, read into a room of their own, one of those the calls
// answered at once share: its answer as JSON into `out`, 1, or 0 with why given (serve_call.c)
int cli_serve_on_answer(const CliServeSizeCtx *args);
// `answer` run on a thread of its own where the arguments carry `request`, the page sent its answer as the event
// `call-answered` with that number, `{"deferred":true}` answered at once; run in place where they do not
// (serve_deferred.c)
int cli_serve_deferred_or(const CliServeErrMsgOutRequestCtx *args);
// the call `name` as one answered later where its arguments carry `request`, running `now`
#define CLI_SERVE_DEFERRED(name, now)                                                                                  \
    static int name(const CliServeAnswerCfg *args)                                                                     \
    {                                                                                                                  \
        return EMBED_CALL(cli_serve_deferred_or, CliServeErrMsgOutRequestCtx, .request = args->request, .answer = now, \
                          .out = args->out, .err_msg = args->err_msg);                                                 \
    }

// `length` bytes of `text` written to the file at `path` of the tree, its text before recorded in its local history, a
// history that cannot be recorded reported on the errors: 1, or 0 with why given (serve_call_files.c)
int cli_serve_file_write(const CliServeErrMsgPathCtx *args);
// the lock every write of a file of the tree acquires, so that one write lands at a time (serve_call_files.c)

// the statement at the argument `offset` of the argument `text`, and every statement of it, each as the argument
// `kind` of database splits them (serve_statements.c)
int cli_serve_db_statement_at(const CliServeAnswerCfg *args);
int cli_serve_db_statements(const CliServeAnswerCfg *args);

// the calls of each source, each table's count beside it (serve_call_*.c)
extern const CliServeCall g_cli_serve_tree_calls[];
extern const unsigned int g_cli_serve_tree_count;
extern const CliServeCall g_cli_serve_files_calls[];
extern const unsigned int g_cli_serve_files_count;

// files from outside the window copied into a folder of the tree; the same read here and written over the link into a
// folder of the tree on another machine; and a free name in a folder for a file brought (serve_call_bring.c)
int cli_serve_files_bring(const CliServeAnswerCfg *args);
int cli_serve_files_bring_over(const CliServeAnswerCfg *args);
int cli_serve_bring_name(const CliServeAnswerCfg *args);
// the folder of the repository the argument `repo` names, the tree's own where it names none, into `folder`, which
// is sized for CLI_PATH_MAX_LEN: 1, or 0 with why given (serve_call_git.c)
int cli_serve_git_repo_get(const CliServeErrMsgRequestFolderCtx *args);
extern const CliServeCall g_cli_serve_git_calls[];
extern const unsigned int g_cli_serve_git_count;
extern const CliServeCall g_cli_serve_git_acts_calls[];
extern const unsigned int g_cli_serve_git_acts_count;
extern const CliServeCall g_cli_serve_run_calls[];
extern const unsigned int g_cli_serve_run_count;
extern const CliServeCall g_cli_serve_lsp_calls[];
extern const unsigned int g_cli_serve_lsp_count;
extern const CliServeCall g_cli_serve_debug_calls[];
extern const unsigned int g_cli_serve_debug_count;
extern const CliServeCall g_cli_serve_db_calls[];
extern const unsigned int g_cli_serve_db_count;
extern const CliServeCall g_cli_serve_kernel_calls[];
extern const unsigned int g_cli_serve_kernel_count;
extern const CliServeCall g_cli_serve_tool_calls[];
extern const unsigned int g_cli_serve_tool_count;
extern const CliServeCall g_cli_serve_build_calls[];
extern const unsigned int g_cli_serve_build_count;
extern const CliServeCall g_cli_serve_shape_calls[];
extern const unsigned int g_cli_serve_shape_count;
extern const CliServeCall g_cli_serve_page_calls[];
extern const unsigned int g_cli_serve_page_count;
extern const CliServeCall g_cli_serve_notebook_calls[];
extern const unsigned int g_cli_serve_notebook_count;

struct CliServeErrMsgCtx
{
    VerbaLinea *err_msg;
    const CliServeArgs *request;
    const char *key;
    const char *otherwise;
    unsigned long long *value;
    unsigned long long otherwise_u64;
    long long *value_i64s;
    int otherwise_i32;
    int *value_i32s;
    const char **items;
    unsigned int room;
    unsigned int *count;
    unsigned int most;
    const char ***items_pointer;
    CliTextPlace *place;
};

struct CliServeSizeCtx
{
    unsigned long long size;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    CliServeAnswer answer;
    const char *args_text;
};

struct CliServeErrMsgOutCtx
{
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const CliServeArgs *request;
    const char *job;
    const char *detached_dir;
};

struct CliServeErrMsgRequestCtx
{
    VerbaLinea *err_msg;
    const CliServeArgs *request;
    const char *key;
    unsigned int node;
    const char *expected;
};

struct CliServeErrMsgRequestRootCtx
{
    VerbaLinea *err_msg;
    const CliServeArgs *request;
    char *root;
    char *full;
};

struct CliServeIdCtx
{
    const char *id;
    int done;
    const char *answer_text;
    VerbaLinea *err_msg;
};

struct CliServeErrMsgOutRequestCtx
{
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const CliServeArgs *request;
    CliServeAnswer answer;
};

struct CliServeErrMsgPathCtx
{
    VerbaLinea *err_msg;
    const char *path;
    const char *text;
    unsigned long long length;
};

struct CliServeSizeErrMsgCtx
{
    unsigned long long size;
    VerbaLinea *err_msg;
    const char *what;
};

struct CliServeBaseCtx
{
    const char *base;
    const char *kind;
    char *path_out;
};

struct CliServeEventCtx
{
    const char *event;
    const char *body;
    unsigned long long size;
};

struct CliServeErrMsgRequestFolderCtx
{
    VerbaLinea *err_msg;
    const CliServeArgs *request;
    char *folder;
};

struct CliServeErrMsgPathOutCtx
{
    VerbaLinea *err_msg;
    const char *path;
    VerbaLinea *out;
};

struct CliServeErrMsgRootCtx
{
    VerbaLinea *err_msg;
    const char *root_read;
};

struct CliServeCtx
{
    const char *tree;
};

struct CliServeListedCtx
{
    CliServeFiles *listed;
};

struct CliServeItemsCtx
{
    const char **items;
};

typedef struct
{
    void (*emit)(const CliServeEventCtx *args);
} CliServeEventNs;
EMBED_TABLE_LAYOUT(CliServeEventNs, emit);
EMBED_TABLE_STORAGE CliServeEventNs cli_serve_event_internal EMBED_UNUSED = {
    .emit = cli_serve_on_emit,
};

typedef struct
{
    CliServersSink (*emitter)(const CliServeCtx *args);
} CliServeNsTable;
EMBED_TABLE_LAYOUT(CliServeNsTable, emitter);
EMBED_TABLE_STORAGE CliServeNsTable cli_serve_internal EMBED_UNUSED = {
    .emitter = cli_serve_emitter,
};

typedef struct
{
    int (*command_answer)(const CliServeIdCtx *args);
} CliServeIdNs;
EMBED_TABLE_LAYOUT(CliServeIdNs, command_answer);
EMBED_TABLE_STORAGE CliServeIdNs cli_serve_id_internal EMBED_UNUSED = {
    .command_answer = cli_serve_command_answer,
};

typedef struct
{
    int (*root_set)(const CliServeErrMsgPathOutCtx *args);
} CliServeErrMsgPathOutNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgPathOutNs, root_set);
EMBED_TABLE_STORAGE CliServeErrMsgPathOutNs cli_serve_err_msg_path_out_internal EMBED_UNUSED = {
    .root_set = cli_serve_root_set,
};

typedef struct
{
    void (*secret_wiped)(const CliServeErrMsgCtx *args);
    unsigned int (*arg)(const CliServeErrMsgCtx *args);
    const char *(*string)(const CliServeErrMsgCtx *args);
    const char *(*string_or)(const CliServeErrMsgCtx *args);
    int (*whole)(const CliServeErrMsgCtx *args);
    unsigned long long (*whole_or)(const CliServeErrMsgCtx *args);
    int (*serve_sign)(const CliServeErrMsgCtx *args);
    int (*bool_or)(const CliServeErrMsgCtx *args);
    int (*serve_bool)(const CliServeErrMsgCtx *args);
    int (*strings)(const CliServeErrMsgCtx *args);
    int (*strings_or)(const CliServeErrMsgCtx *args);
    int (*strings_take)(const CliServeErrMsgCtx *args);
    int (*place)(const CliServeErrMsgCtx *args);
} CliServeErrMsgNsTable;
EMBED_TABLE_LAYOUT(CliServeErrMsgNsTable, secret_wiped, arg, string, string_or, whole, whole_or, serve_sign, bool_or,
                   serve_bool, strings, strings_or, strings_take, place);
EMBED_TABLE_STORAGE CliServeErrMsgNsTable cli_serve_err_msg_internal EMBED_UNUSED = {
    .secret_wiped = cli_serve_secret_wiped,
    .arg = cli_serve_arg,
    .string = cli_serve_string,
    .string_or = cli_serve_string_or,
    .whole = cli_serve_whole,
    .whole_or = cli_serve_whole_or,
    .serve_sign = cli_serve_sign,
    .bool_or = cli_serve_bool_or,
    .serve_bool = cli_serve_bool,
    .strings = cli_serve_strings,
    .strings_or = cli_serve_strings_or,
    .strings_take = cli_serve_strings_take,
    .place = cli_serve_place,
};

typedef struct
{
    void (*reject)(const CliServeErrMsgRequestCtx *args);
} CliServeErrMsgRequestNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgRequestNs, reject);
EMBED_TABLE_STORAGE CliServeErrMsgRequestNs cli_serve_err_msg_request_internal EMBED_UNUSED = {
    .reject = cli_serve_reject,
};

typedef struct
{
    void (*strings_free)(const CliServeItemsCtx *args);
} CliServeItemsNs;
EMBED_TABLE_LAYOUT(CliServeItemsNs, strings_free);
EMBED_TABLE_STORAGE CliServeItemsNs cli_serve_items_internal EMBED_UNUSED = {
    .strings_free = cli_serve_strings_free,
};

typedef struct
{
    int (*tree)(const CliServeErrMsgRequestRootCtx *args);
    int (*at)(const CliServeErrMsgRequestRootCtx *args);
} CliServeErrMsgRequestRootNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgRequestRootNs, tree, at);
EMBED_TABLE_STORAGE CliServeErrMsgRequestRootNs cli_serve_err_msg_request_root_internal EMBED_UNUSED = {
    .tree = cli_serve_tree,
    .at = cli_serve_at,
};

typedef struct
{
    void *(*cell)(const CliServeSizeErrMsgCtx *args);
} CliServeSizeErrMsgNs;
EMBED_TABLE_LAYOUT(CliServeSizeErrMsgNs, cell);
EMBED_TABLE_STORAGE CliServeSizeErrMsgNs cli_serve_size_err_msg_internal EMBED_UNUSED = {
    .cell = cli_serve_cell,
};

typedef struct
{
    CliServeFiles *(*files_list)(const CliServeErrMsgRootCtx *args);
    CliCatalog *(*catalog_take)(const CliServeErrMsgRootCtx *args);
} CliServeErrMsgRootNsTable;
EMBED_TABLE_LAYOUT(CliServeErrMsgRootNsTable, files_list, catalog_take);
EMBED_TABLE_STORAGE CliServeErrMsgRootNsTable cli_serve_err_msg_root_internal EMBED_UNUSED = {
    .files_list = cli_serve_files_list,
    .catalog_take = cli_serve_catalog_take,
};

typedef struct
{
    void (*files_free)(const CliServeListedCtx *args);
} CliServeListedNs;
EMBED_TABLE_LAYOUT(CliServeListedNs, files_free);
EMBED_TABLE_STORAGE CliServeListedNs cli_serve_listed_internal EMBED_UNUSED = {
    .files_free = cli_serve_files_free,
};

typedef struct
{
    CliValues *(*values_get)(const CliServeErrMsgOutCtx *args);
    int (*job_start)(const CliServeErrMsgOutCtx *args);
    int (*detached_start)(const CliServeErrMsgOutCtx *args);
} CliServeErrMsgOutNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgOutNs, values_get, job_start, detached_start);
EMBED_TABLE_STORAGE CliServeErrMsgOutNs cli_serve_err_msg_out_internal EMBED_UNUSED = {
    .values_get = cli_serve_values_get,
    .job_start = cli_serve_job_start,
    .detached_start = cli_serve_detached_start,
};

typedef struct
{
    int (*catalog_read)(const CliServeErrMsgOutRequestCtx *args);
    int (*deferred_or)(const CliServeErrMsgOutRequestCtx *args);
} CliServeErrMsgOutRequestNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgOutRequestNs, catalog_read, deferred_or);
EMBED_TABLE_STORAGE CliServeErrMsgOutRequestNs cli_serve_err_msg_out_request_internal EMBED_UNUSED = {
    .catalog_read = cli_serve_catalog_read,
    .deferred_or = cli_serve_deferred_or,
};

typedef struct
{
    void (*detached_file)(const CliServeBaseCtx *args);
} CliServeBaseNsTable;
EMBED_TABLE_LAYOUT(CliServeBaseNsTable, detached_file);
EMBED_TABLE_STORAGE CliServeBaseNsTable cli_serve_base_internal EMBED_UNUSED = {
    .detached_file = cli_serve_detached_file,
};

typedef struct
{
    int (*run_follow)(const CliServeAnswerCfg *args);
    int (*runs_detach)(const CliServeAnswerCfg *args);
    int (*job_stop)(const CliServeAnswerCfg *args);
    int (*db_statement_at)(const CliServeAnswerCfg *args);
    int (*db_statements)(const CliServeAnswerCfg *args);
    int (*files_bring)(const CliServeAnswerCfg *args);
    int (*files_bring_over)(const CliServeAnswerCfg *args);
    int (*bring_name)(const CliServeAnswerCfg *args);
} CliServeAnswerNs;
EMBED_TABLE_LAYOUT(CliServeAnswerNs, run_follow, runs_detach, job_stop, db_statement_at, db_statements, files_bring,
                   files_bring_over, bring_name);
EMBED_TABLE_STORAGE CliServeAnswerNs cli_serve_answer EMBED_UNUSED = {
    .run_follow = cli_serve_run_follow,
    .runs_detach = cli_serve_runs_detach,
    .job_stop = cli_serve_job_stop,
    .db_statement_at = cli_serve_db_statement_at,
    .db_statements = cli_serve_db_statements,
    .files_bring = cli_serve_files_bring,
    .files_bring_over = cli_serve_files_bring_over,
    .bring_name = cli_serve_bring_name,
};

typedef struct
{
    int (*answer)(const CliServeSizeCtx *args);
} CliServeSizeNs;
EMBED_TABLE_LAYOUT(CliServeSizeNs, answer);
EMBED_TABLE_STORAGE CliServeSizeNs cli_serve_size_internal EMBED_UNUSED = {
    .answer = cli_serve_on_answer,
};

typedef struct
{
    int (*file_write)(const CliServeErrMsgPathCtx *args);
} CliServeErrMsgPathNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgPathNs, file_write);
EMBED_TABLE_STORAGE CliServeErrMsgPathNs cli_serve_err_msg_path_internal EMBED_UNUSED = {
    .file_write = cli_serve_file_write,
};

typedef struct
{
    int (*git_repo_get)(const CliServeErrMsgRequestFolderCtx *args);
} CliServeErrMsgRequestFolderNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgRequestFolderNs, git_repo_get);
EMBED_TABLE_STORAGE CliServeErrMsgRequestFolderNs cli_serve_err_msg_request_folder_internal EMBED_UNUSED = {
    .git_repo_get = cli_serve_git_repo_get,
};

#endif
