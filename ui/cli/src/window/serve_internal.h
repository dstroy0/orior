// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_internal.h: what the server's sources share: the tree open and where what the server emits goes, the comms
// keys it sends and is reached with, the door's address, the database connections and kernels open; a call's
// arguments as they are read; and the tables of calls each source answers
#ifndef SERVE_INTERNAL_H
#define SERVE_INTERNAL_H

#include "../command_line/catalog.h"
#include "../command_line/cells.h"
#include "../comms/comms.h"
#include "../editor/files.h"
#include "../servers/servers_internal.h"
#include "../text/json_tree.h"
#include "serve.h"

// The server, under its lock: the tree open, the folder the runs started over a link are stored in, where what it emits
// goes before the comms layer carries it, the key it sends on every channel with, the persistent key the command line
// joins the door with, and the door's address
typedef struct
{
    CliLock lock;
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
void cli_serve_emitted(const char *event, const char *body, unsigned long long size);
// where what the servers of `tree` emit goes, each path as it stands in `tree`
CliServersSink cli_serve_emitter(const char *tree);
// the page's answer to the command of `id` sent to the command line that requested it: 1, or 0 with why reported
int cli_serve_command_answer(const char *id, int done, const char *answer_text, VerbaLinea *err_msg);
// the tree at `path` made the tree open, a tree that moves letting go of every kernel, database connection, mount,
// language server and symbol of the one before and watching the new one: its full path into `out`, 1, or 0 with why
// given
int cli_serve_root_set(const char *path, VerbaLinea *out, VerbaLinea *err_msg);

// every database connection closed; and every kernel the notebooks ran on ended (serve_databases.c, serve_kernels.c)
void cli_serve_databases_closed(void);
void cli_serve_kernels_stopped(void);

// A call's arguments: the tree they are read into and the object that contains them
typedef struct
{
    const CliJson *json;
    unsigned int object;
} CliServeArgs;

// the text of the argument `key`, where it is one, zeroed where it lies: a secret a call carries
void cli_serve_secret_wiped(const CliServeArgs *args, const char *key);

// the argument `key`, by its own name or by the page's, CLI_JSON_NONE where there is none; a string argument, NULL with
// why given where it is none; the same, `otherwise` where it is none or null; a whole number, 1, or 0 with why given;
// the same, `otherwise` where it is none or null; a whole number that may be below 0; whether a boolean is true,
// `otherwise` where it is none or null; and a boolean, 1, or 0 with why given (serve_args.c)
unsigned int cli_serve_arg(const CliServeArgs *args, const char *key);
const char *cli_serve_string(const CliServeArgs *args, const char *key, VerbaLinea *err_msg);
const char *cli_serve_string_or(const CliServeArgs *args, const char *key, const char *otherwise);
int cli_serve_whole(const CliServeArgs *args, const char *key, unsigned long long *value, VerbaLinea *err_msg);
unsigned long long cli_serve_whole_or(const CliServeArgs *args, const char *key, unsigned long long otherwise);
int cli_serve_signed(const CliServeArgs *args, const char *key, long long *value, VerbaLinea *err_msg);
int cli_serve_bool_or(const CliServeArgs *args, const char *key, int otherwise);
int cli_serve_bool(const CliServeArgs *args, const char *key, int *value, VerbaLinea *err_msg);
// why the argument at `node`, named `key`, of the wrong kind is refused, as serde reports it: its name, what it is, and
// `expected`
void cli_serve_refused(const CliServeArgs *args, unsigned int node, const char *key, const char *expected,
                       VerbaLinea *err_msg);
// the strings of an array argument into `items`, sized for `room`: their count into `count`, 1, or 0 with why given
// where it is no array of strings; the same where none or null is an empty array
int cli_serve_strings(const CliServeArgs *args, const char *key, const char **items, unsigned int room,
                      unsigned int *count, VerbaLinea *err_msg);
int cli_serve_strings_or(const CliServeArgs *args, const char *key, const char **items, unsigned int room,
                         unsigned int *count, VerbaLinea *err_msg);
// the strings of an array argument, `most` of them at the most, in a cell of their count and one more, the place after
// the last NULL, into `*items`, given back with cli_serve_strings_given either way: their count into `count`, 1, or 0
// with why given where it is no array of strings or the cellblock could not meet it; and the cells those calls occupy
extern CellOwner g_cli_serve_strings_cells;
int cli_serve_strings_taken(const CliServeArgs *args, const char *key, unsigned int most, const char ***items,
                            unsigned int *count, VerbaLinea *err_msg);
void cli_serve_strings_given(const char **items);
// the tree open into `root`, 1, or 0 with why given; and the argument `path` joined to it into `full`, each of which
// is sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
int cli_serve_tree(char *root, VerbaLinea *err_msg);
int cli_serve_at(const CliServeArgs *args, char *root, char *full, VerbaLinea *err_msg);
// a cell of `size` for one call's own use, named `what`: it, or NULL with why given where the cells cannot meet it;
// given back with g_cli_cells->persistent_buf_release (serve_args.c)
void *cli_serve_cell(unsigned long long size, const char *what, VerbaLinea *err_msg);

// The tree's files laid flat for one call, and a pointer to each, `count` of them in the same cell
typedef struct
{
    const CliFiles *files;
    unsigned int count;
    const char **paths;
} CliServeFiles;

// the files of the tree at `root` laid flat and a pointer to each listed into a cell: it, or NULL with why given; and
// the list and the cell given back, NULL given back as nothing (serve_args.c)
CliServeFiles *cli_serve_files_listed(const char *root, VerbaLinea *err_msg);
void cli_serve_files_given(CliServeFiles *listed);
// a place argument `{line, col}` read into `place`: 1, or 0 with why given
int cli_serve_place(const CliServeArgs *args, const char *key, CliTextPlace *place, VerbaLinea *err_msg);

// the tree's catalog read for the page into a cell, its lock taken: the catalog, or NULL with why given and the lock
// given back where no room for it is left; and the cell and its lock given back; the values of the
// argument `values`, an object of each param's values, read, under the catalog's lock; a job started on this machine,
// its lines going to the page fifty milliseconds' worth at a time: its run's number written, 1, or 0 with why given;
// and the tree's jobs as the window lists them, each with how many steps it runs (serve_jobs.c)
CliCatalog *cli_serve_catalog_taken(const char *root, VerbaLinea *err_msg);
void cli_serve_catalog_given(void);
CliValues *cli_serve_values_of(const CliServeArgs *args);
int cli_serve_job_start(const char *job, const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);
// the gatherings of the runs started here that are open
extern CellOwner g_cli_serve_gathering_cells;
int cli_serve_catalog_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);

// the most bytes of a line a detached run prints; and the file of the detached run `base` of `kind`, `job`, `output`,
// `pid` or `stop`, into `path`, sized for CLI_PATH_MAX_LEN (serve_detached_run.c)
#define CLI_SERVE_DETACHED_LINE (1u << 20u)
void cli_serve_detached_file(const char *base, const char *kind, char *path);
// `job` started as a detached run in the folder `detached_dir`, a process of its own whose lines are stored on this
// machine, and followed: its number written, 1, or 0 with why given; the detached run of the argument `run` followed;
// the detached runs of the tree open listed, the oldest first, each `{"run", "job", "ended"}`, those retained a week
// let go; and a run stopped, a detached run by its stop file (serve_detached.c)
int cli_serve_detached_start(const char *detached_dir, const char *job, const CliServeArgs *args, VerbaLinea *out,
                             VerbaLinea *err_msg);
int cli_serve_run_follow(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);
int cli_serve_runs_detached(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);
int cli_serve_job_stop(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);

// A call the server answers: its name and the function that answers it, its answer as JSON into `out`, 1, or 0 with
// why given
typedef int (*CliServeAnswer)(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);
typedef struct
{
    const char *name;
    CliServeAnswer answer;
} CliServeCall;

// `answer` given `size` bytes of its arguments as a JSON object, read into a room of their own, one of those the calls
// answered at once share: its answer as JSON into `out`, 1, or 0 with why given (serve_call.c)
int cli_serve_answered(CliServeAnswer answer, const char *args, unsigned long long size, VerbaLinea *out,
                       VerbaLinea *err_msg);
// `answer` run on a thread of its own where the arguments carry `request`, the page sent its answer as the event
// `call-answered` with that number, `{"deferred":true}` answered at once; run in place where they do not
// (serve_deferred.c)
int cli_serve_deferred_or(const CliServeArgs *args, CliServeAnswer answer, VerbaLinea *out, VerbaLinea *err_msg);
// the call `name` as one answered later where its arguments carry `request`, running `now`
#define CLI_SERVE_DEFERRED(name, now)                                                                                  \
    static int name(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)                                    \
    {                                                                                                                  \
        return cli_serve_deferred_or(args, now, out, err_msg);                                                         \
    }

// `length` bytes of `text` written to the file at `path` of the tree, its text before recorded in its local history, a
// history that cannot be recorded reported on the errors: 1, or 0 with why given (serve_call_files.c)
int cli_serve_file_write(const char *path, const char *text, unsigned long long length, VerbaLinea *err_msg);
// the lock every write of a file of the tree acquires, so that one write lands at a time (serve_call_files.c)
extern CliLock g_cli_serve_write_lock;

// the statement at the argument `offset` of the argument `text`, and every statement of it, each as the argument
// `kind` of database splits them (serve_statements.c)
int cli_serve_db_statement_at(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);
int cli_serve_db_statements(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);

// the calls of each source, each table's count beside it (serve_call_*.c)
extern const CliServeCall g_cli_serve_tree_calls[];
extern const unsigned int g_cli_serve_tree_count;
extern const CliServeCall g_cli_serve_files_calls[];
extern const unsigned int g_cli_serve_files_count;

// files from outside the window copied into a folder of the tree; the same read here and written over the link into a
// folder of the tree on another machine; and a free name in a folder for a file brought (serve_call_bring.c)
int cli_serve_files_bring(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);
int cli_serve_files_bring_over(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);
int cli_serve_bring_name(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg);
// the folder of the repository the argument `repo` names, the tree's own where it names none, into `folder`, which
// is sized for CLI_PATH_MAX_LEN: 1, or 0 with why given (serve_call_git.c)
int cli_serve_git_repo_of(const CliServeArgs *args, char *folder, VerbaLinea *err_msg);
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

#endif
