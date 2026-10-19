// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_internal.h: what the server's sources share: the tree open and where what the server tells goes, the comms
// keys it speaks and is spoken to with, the door's address, the database connections and kernels open; a call's
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

// The server, under its lock: the tree open, the folder the runs started over a link are kept in, where what it tells
// goes before the comms layer carries it, the key it speaks every channel with, the held key the command line joins
// the door with, and the door's address
typedef struct
{
    CliLock lock;
    char root[CLI_PATH_LONGEST];
    int has_root;
    char keeping[CLI_PATH_LONGEST];
    int has_keeping;
    CliServeEmit emit;
    void *emit_context;
    int has_emit;
    char speaker[CLI_COMMS_KEY];
    int speaking;
    char command_line[CLI_COMMS_KEY];
    int has_command_line;
    char door[CLI_PATH_LONGEST];
    int has_door;
} CliServeState;

extern CliServeState g_cli_serve;

// what the server tells sent where it goes, or nowhere before it was told where (serve.c)
void cli_serve_emitted(const char *event, const char *body, unsigned long long size);
// where what the servers of `tree` tell goes, each path as it stands in `tree`
CliServersEmit cli_serve_emitter(const char *tree);
// the page's answer to the command of `id` said to the command line that asked it: 1, or 0 with why said
int cli_serve_command_answer(const char *id, int done, const char *said_text, CliLine *said);
// the tree at `path` made the tree open, a tree that moves letting go of every kernel, database connection, mount,
// language server and symbol of the one before and watching the new one: its full path into `out`, 1, or 0 with why
// said
int cli_serve_root_set(const char *path, CliLine *out, CliLine *said);

// every database connection closed; and every kernel the notebooks ran on ended (serve_databases.c, serve_kernels.c)
void cli_serve_databases_closed(void);
void cli_serve_kernels_stopped(void);

// A call's arguments: the tree they are read into and the object that holds them
typedef struct
{
    const CliJson *json;
    unsigned int object;
} CliServeArgs;

// the argument `key`, by its own name or by the page's, CLI_JSON_NONE where there is none; a string argument, NULL with
// why said where it is none; the same, `otherwise` where it is none or null; a whole number, 1, or 0 with why said;
// the same, `otherwise` where it is none or null; a whole number that may be below 0; whether a boolean is true,
// `otherwise` where it is none or null; and a boolean, 1, or 0 with why said (serve_args.c)
unsigned int cli_serve_arg(const CliServeArgs *args, const char *key);
const char *cli_serve_string(const CliServeArgs *args, const char *key, CliLine *said);
const char *cli_serve_string_or(const CliServeArgs *args, const char *key, const char *otherwise);
int cli_serve_whole(const CliServeArgs *args, const char *key, unsigned long long *value, CliLine *said);
unsigned long long cli_serve_whole_or(const CliServeArgs *args, const char *key, unsigned long long otherwise);
int cli_serve_signed(const CliServeArgs *args, const char *key, long long *value, CliLine *said);
int cli_serve_bool_or(const CliServeArgs *args, const char *key, int otherwise);
int cli_serve_bool(const CliServeArgs *args, const char *key, int *value, CliLine *said);
// why the argument at `node`, named `key`, of the wrong kind is refused, as serde says it: its name, what it is, and
// `expected`
void cli_serve_refused(const CliServeArgs *args, unsigned int node, const char *key, const char *expected,
                       CliLine *said);
// the strings of an array argument into `items`, which holds `room`: their count into `count`, 1, or 0 with why said
// where it is no array of strings; the same where none or null is an empty array
int cli_serve_strings(const CliServeArgs *args, const char *key, const char **items, unsigned int room,
                      unsigned int *count, CliLine *said);
int cli_serve_strings_or(const CliServeArgs *args, const char *key, const char **items, unsigned int room,
                         unsigned int *count, CliLine *said);
// the tree open into `root`, 1, or 0 with why said; and the argument `path` joined to it into `full`, each of which
// holds CLI_PATH_LONGEST: 1, or 0 with why said
int cli_serve_tree(char *root, CliLine *said);
int cli_serve_at(const CliServeArgs *args, char *root, char *full, CliLine *said);
// a cell of `size` for one call's own use, named `what`: it, or NULL with why said where the cells cannot meet it;
// given back with cli_cell_given (serve_args.c)
void *cli_serve_cell(unsigned long long size, const char *what, CliLine *said);

// The tree's files as the view of it holds them, held in place for one call, and a pointer to each, `count` of them in
// the same cell
typedef struct
{
    const CliFiles *files;
    unsigned int count;
    const char **paths;
} CliServeFiles;

// the view of the tree at `root` held and a pointer to each of its files listed into a cell: it, or NULL with why
// said; and the view let go and the cell given back, NULL given back as nothing (serve_args.c)
CliServeFiles *cli_serve_files_listed(const char *root, CliLine *said);
void cli_serve_files_given(CliServeFiles *listed);
// a place argument `{line, col}` read into `place`: 1, or 0 with why said
int cli_serve_place(const CliServeArgs *args, const char *key, CliTextPlace *place, CliLine *said);

// the tree's catalog read for the page into a cell, its lock taken: the catalog, or NULL with why said and the lock
// given back where no room for it is left; and the cell and its lock given back; the values of the
// argument `values`, an object of each param's values, read, under the catalog's lock; a job started on this machine,
// its lines going to the page fifty milliseconds' worth at a time: its run's number written, 1, or 0 with why said;
// and the tree's jobs as the window lists them, each with how many steps it runs (serve_jobs.c)
CliCatalog *cli_serve_catalog_taken(const char *root, CliLine *said);
void cli_serve_catalog_given(void);
CliValues *cli_serve_values_of(const CliServeArgs *args);
int cli_serve_job_start(const char *job, const CliServeArgs *args, CliLine *out, CliLine *said);
int cli_serve_catalog_read(const CliServeArgs *args, CliLine *out, CliLine *said);

// the most bytes of a line a kept run says; and the file of the kept run `base` of `kind`, `job`, `said`, `pid` or
// `stop`, into `path`, which holds CLI_PATH_LONGEST (serve_kept_run.c)
#define CLI_SERVE_KEPT_LINE (1u << 20u)
void cli_serve_kept_file(const char *base, const char *kind, char *path);
// `job` started as a kept run in the folder `keep`, a process of its own whose lines are kept on this machine, and
// followed: its number written, 1, or 0 with why said; the kept run of the argument `run` followed; the kept runs of
// the tree open listed, the oldest first, each `{"run", "job", "ended"}`, those kept a week let go; and a run stopped,
// a kept run by its stop file (serve_kept.c)
int cli_serve_kept_start(const char *keep, const char *job, const CliServeArgs *args, CliLine *out, CliLine *said);
int cli_serve_run_follow(const CliServeArgs *args, CliLine *out, CliLine *said);
int cli_serve_runs_kept(const CliServeArgs *args, CliLine *out, CliLine *said);
int cli_serve_job_stop(const CliServeArgs *args, CliLine *out, CliLine *said);

// A call the server answers: its name and the function that answers it, its answer as JSON into `out`, 1, or 0 with
// why said
typedef int (*CliServeAnswer)(const CliServeArgs *args, CliLine *out, CliLine *said);
typedef struct
{
    const char *name;
    CliServeAnswer answer;
} CliServeCall;

// `answer` given `size` bytes of its arguments as a JSON object, read into a room of their own, one of those the calls
// answered at once share: its answer as JSON into `out`, 1, or 0 with why said (serve_call.c)
int cli_serve_answered(CliServeAnswer answer, const char *args, unsigned long long size, CliLine *out, CliLine *said);

// `length` bytes of `text` written to the file at `path` of the tree, its text before kept in its local history, a
// history that cannot be kept said on the errors: 1, or 0 with why said (serve_call_files.c)
int cli_serve_file_write(const char *path, const char *text, unsigned long long length, CliLine *said);

// the statement at the argument `offset` of the argument `text`, and every statement of it, each as the argument
// `kind` of database splits them (serve_statements.c)
int cli_serve_db_statement_at(const CliServeArgs *args, CliLine *out, CliLine *said);
int cli_serve_db_statements(const CliServeArgs *args, CliLine *out, CliLine *said);

// the calls of each source, each table's count beside it (serve_call_*.c)
extern const CliServeCall g_cli_serve_tree_calls[];
extern const unsigned int g_cli_serve_tree_count;
extern const CliServeCall g_cli_serve_files_calls[];
extern const unsigned int g_cli_serve_files_count;

// files from outside the window copied into a folder of the tree, and a free name in a folder for a file brought
// (serve_call_bring.c)
int cli_serve_files_bring(const CliServeArgs *args, CliLine *out, CliLine *said);
int cli_serve_bring_name(const CliServeArgs *args, CliLine *out, CliLine *said);
extern const CliServeCall g_cli_serve_git_calls[];
extern const unsigned int g_cli_serve_git_count;
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

#endif
