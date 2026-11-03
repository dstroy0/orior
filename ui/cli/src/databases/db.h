// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db.h: the database window's connections: the ones a tree names in databases.json at its top folder, each a
// PostgreSQL, a MySQL or MariaDB, or a SQLite database, opened as two links, one that runs the reader's statements and
// one that reads the schema, and the other never waits on.
//
// PostgreSQL and MySQL are implemented by pg.h and mysql.h; SQLite is read through the sqlite3 module of the tree's
// Python, in a process of its own that takes one statement at a time and stops one when requested. A password goes to
// the system's keychain, and never to a file; a connection reached through ssh goes through the system's ssh with the
// reader's own config, keys and agent, or the ssh ORIOR_SSH names, as a tree on another machine does, and a link whose
// connection broke is made again, tunnel and all, at the next statement.
//
// A statement's rows are buffered as they are read, a page at a time as the grid requests them, and the rest wait in
// the server. An export writes the rows buffered and reads the rest from the same run. Before a statement runs, an
// UPDATE or a DELETE with no WHERE, or a TRUNCATE, confirms on every connection with the count of the rows it would
// reach, and on a connection marked as one that matters a statement that writes confirms, or the connection is opened
// read-only, the server itself refusing a write, as the reader sets it. Each statement run is recorded in its
// connection's history in orior's own folder. What a call gives is written as JSON, as the window reads it
// (db_config.c, db_open.c, db_link.c, db_lite.c, db_lite_shim.c, db_tunnel.c, db_run.c, db_results.c, db_export.c,
// db_children.c, db_history.c, platform/windows_keychain.c, platform/posix_keychain.c)
#ifndef DB_H
#define DB_H
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "db_wire.h"
#include "embed_dispatch_layout.h"

typedef struct CliDbOutCfg CliDbOutCfg;
typedef struct CliDbOutFailureCfg CliDbOutFailureCfg;
typedef struct CliDbOutCountCfg CliDbOutCountCfg;
typedef struct CliDbOutRootCfg CliDbOutRootCfg;
typedef struct CliDbOutFailureOpenCfg CliDbOutFailureOpenCfg;
typedef struct CliDbOutSizeCfg CliDbOutSizeCfg;
typedef struct CliDbRootCfg CliDbRootCfg;
typedef struct CliDbConnectionErrMsgCfg CliDbConnectionErrMsgCfg;
typedef struct CliDbRootNameCfg CliDbRootNameCfg;
typedef struct CliDbConnectionCfg CliDbConnectionCfg;
typedef struct CliDbConnectionOutKindCfg CliDbConnectionOutKindCfg;
typedef struct CliDbErrMsgCfg CliDbErrMsgCfg;
typedef struct CliDbCfg CliDbCfg;

// A connection a tree names: its name; its kind, postgres, mysql or sqlite; its host, port, user and database; a SQLite
// database's file, by its path in the tree or whole; the machine, as ssh names it, a tunnel to the server goes
// through; whether it is one that matters, whose writes are guarded; and how, read-only or confirm
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    char kind[16];
    char host[CLI_NAME_MAX_LEN];
    unsigned int port;
    char user[CLI_NAME_MAX_LEN];
    char database[CLI_NAME_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char ssh[CLI_NAME_MAX_LEN];
    int matters;
    char guard[16];
} CliDbConnection;

// A connection open: its links, its tunnel and the rows it buffers
typedef struct CliDbOpen CliDbOpen;

// the port a connection's server listens at, its kind's own where none is named; the account the keychain stores its
// password by into `out`; the connection a tree names `name` into `connection`: 1, or 0 with why given; the connection
// object `node` of `json` read into `connection`: 1, or 0 with why given; the connection stored in databases.json in
// place of the one of its name, or of `was` where it is renamed; and the connection `name` taken out of databases.json
// and its password out of the keychain: each 1, or 0 with why given; and what the window shows of each connection, its
// settings and whether it is open, with its transaction's state and its session's zone, the open connections `opens`,
// `count` of them, written into `out`: 1, or 0 with why given (db_config.c)
unsigned int cli_db_port(const CliDbConnectionOutKindCfg *args);
void cli_db_account(const CliDbConnectionOutKindCfg *args);
int cli_db_by_name(const CliDbRootNameCfg *args);
int cli_db_connection_get(const CliDbConnectionErrMsgCfg *args);
int cli_db_save(const CliDbConnectionCfg *args);
int cli_db_remove(const CliDbRootNameCfg *args);
int cli_db_list(const CliDbOutRootCfg *args);

// `config`'s two links opened, signed in with `password`, or with the one the keychain stores where it is NULL: the
// connection, or NULL with the failure into `failure`; and its links and its tunnel ended and the connection let go
// (db_open.c)
CliDbOpen *cli_db_connect(const CliDbRootCfg *args);
void cli_db_close(const CliDbErrMsgCfg *args);
// the connection's name, its server's version, its transaction's state on the query link, busy where a statement
// occupies it, and the zone the session reads times in into `out`
const char *cli_db_name(const CliDbCfg *args);
const char *cli_db_version(const CliDbCfg *args);
const char *cli_db_transaction(const CliDbErrMsgCfg *args);
void cli_db_zone(const CliDbOutCountCfg *args);

// `size` bytes of `statement` run, confirming first where it must and the reader has not `answered`, its first `page`
// rows or what it did written into `out`; the rows buffered from `from`, `count` of them, read from the run where they
// are not buffered yet; and the rows buffered, then the rest the run gives, written to `path` as csv, tsv or json
// lines, `progress` handed how many it has written after each 4096 it reads on. Each 1, or 0 with the failure into
// `failure` (db_run.c, db_results.c)
int cli_db_run(const CliDbOutFailureCfg *args);
int cli_db_rows(const CliDbOutFailureOpenCfg *args);
int cli_db_export(const CliDbOutCfg *args);

// the statement running stopped, and the schema's reading stopped: 1, or 0 with why given (db_run.c)
int cli_db_stop(const CliDbErrMsgCfg *args);
int cli_db_stop_read_state(const CliDbErrMsgCfg *args);

// the level of the schema under `path`, `count` names: the schemas or databases at the top, a schema's objects under
// it, and a table's columns, indexes, keys and triggers under it, written into `out`: 1, or 0 with the failure into
// `failure` (db_children.c)
int cli_db_children(const CliDbOutCountCfg *args);

// the statements run on the connection, the newest first, written into `out`; the span of the statement `size` bytes
// of `text` occupy at `offset`, counted in characters, as the editor counts them, as [start, end], null where the place
// stands in no statement; and each statement of `text` by its span in characters (db_history.c)
void cli_db_history(const CliDbOutCountCfg *args);
void cli_db_statement_at(const CliDbOutSizeCfg *args);
void cli_db_statements(const CliDbOutSizeCfg *args);

struct CliDbOutCfg
{
    VerbaLinea *out;
    CliDbFailure *failure;
    CliDbOpen *open;
    const char *path;
    const char *format;
    void (*progress)(const CliEachWrittenCfg *args);
    void *context;
};

struct CliDbOutFailureCfg
{
    VerbaLinea *out;
    CliDbFailure *failure;
    CliDbOpen *open;
    const char *statement;
    unsigned long long size;
    int answered;
    unsigned long long page;
};

struct CliDbOutCountCfg
{
    VerbaLinea *out;
    unsigned int count;
    CliDbFailure *failure;
    CliDbOpen *open;
    const char *const *path_pointer;
};

struct CliDbOutRootCfg
{
    VerbaLinea *out;
    const char *root;
    VerbaLinea *err_msg;
    CliDbOpen *const *opens;
    unsigned int count;
};

struct CliDbOutFailureOpenCfg
{
    VerbaLinea *out;
    CliDbFailure *failure;
    CliDbOpen *open;
    unsigned long long from;
    unsigned long long count_u64;
};

struct CliDbOutSizeCfg
{
    VerbaLinea *out;
    unsigned long long size;
    const char *text;
    unsigned long long offset;
    const char *kind;
};

struct CliDbRootCfg
{
    const char *root;
    const CliDbConnection *config;
    const char *password;
    CliDbFailure *failure;
};

struct CliDbConnectionErrMsgCfg
{
    CliDbConnection *connection_out;
    VerbaLinea *err_msg;
    const CliJson *json;
    unsigned int node;
};

struct CliDbRootNameCfg
{
    const char *root;
    const char *name;
    CliDbConnection *connection_out;
    VerbaLinea *err_msg;
};

struct CliDbConnectionCfg
{
    const CliDbConnection *connection;
    const char *root;
    VerbaLinea *err_msg;
    const char *was;
};

struct CliDbConnectionOutKindCfg
{
    const CliDbConnection *connection;
    VerbaLinea *out;
};

struct CliDbErrMsgCfg
{
    VerbaLinea *err_msg;
    CliDbOpen *open;
};

struct CliDbCfg
{
    const CliDbOpen *open_read;
};

typedef struct
{
    unsigned int (*port)(const CliDbConnectionOutKindCfg *args);
    void (*account)(const CliDbConnectionOutKindCfg *args);
} CliDbConnectionOutKindNs;
EMBED_TABLE_LAYOUT(CliDbConnectionOutKindNs, port, account);
EMBED_TABLE_STORAGE CliDbConnectionOutKindNs cli_db_connection_out_kind EMBED_UNUSED = {
    .port = cli_db_port,
    .account = cli_db_account,
};

typedef struct
{
    int (*by_name)(const CliDbRootNameCfg *args);
    int (*remove)(const CliDbRootNameCfg *args);
} CliDbRootNameNs;
EMBED_TABLE_LAYOUT(CliDbRootNameNs, by_name, remove);
EMBED_TABLE_STORAGE CliDbRootNameNs cli_db_root_name EMBED_UNUSED = {
    .by_name = cli_db_by_name,
    .remove = cli_db_remove,
};

typedef struct
{
    int (*connection_get)(const CliDbConnectionErrMsgCfg *args);
} CliDbConnectionErrMsgNs;
EMBED_TABLE_LAYOUT(CliDbConnectionErrMsgNs, connection_get);
EMBED_TABLE_STORAGE CliDbConnectionErrMsgNs cli_db_connection_err_msg EMBED_UNUSED = {
    .connection_get = cli_db_connection_get,
};

typedef struct
{
    int (*save)(const CliDbConnectionCfg *args);
} CliDbConnectionNs;
EMBED_TABLE_LAYOUT(CliDbConnectionNs, save);
EMBED_TABLE_STORAGE CliDbConnectionNs cli_db_connection EMBED_UNUSED = {
    .save = cli_db_save,
};

typedef struct
{
    int (*list)(const CliDbOutRootCfg *args);
} CliDbOutRootNs;
EMBED_TABLE_LAYOUT(CliDbOutRootNs, list);
EMBED_TABLE_STORAGE CliDbOutRootNs cli_db_out_root EMBED_UNUSED = {
    .list = cli_db_list,
};

typedef struct
{
    CliDbOpen *(*connect)(const CliDbRootCfg *args);
} CliDbRootNs;
EMBED_TABLE_LAYOUT(CliDbRootNs, connect);
EMBED_TABLE_STORAGE CliDbRootNs cli_db_root EMBED_UNUSED = {
    .connect = cli_db_connect,
};

typedef struct
{
    void (*close)(const CliDbErrMsgCfg *args);
    const char *(*transaction)(const CliDbErrMsgCfg *args);
    int (*stop)(const CliDbErrMsgCfg *args);
    int (*stop_read_state)(const CliDbErrMsgCfg *args);
} CliDbErrMsgNs;
EMBED_TABLE_LAYOUT(CliDbErrMsgNs, close, transaction, stop, stop_read_state);
EMBED_TABLE_STORAGE CliDbErrMsgNs cli_db_err_msg EMBED_UNUSED = {
    .close = cli_db_close,
    .transaction = cli_db_transaction,
    .stop = cli_db_stop,
    .stop_read_state = cli_db_stop_read_state,
};

typedef struct
{
    const char *(*name)(const CliDbCfg *args);
    const char *(*version)(const CliDbCfg *args);
} CliDbNs;
EMBED_TABLE_LAYOUT(CliDbNs, name, version);
EMBED_TABLE_STORAGE CliDbNs cli_db EMBED_UNUSED = {
    .name = cli_db_name,
    .version = cli_db_version,
};

typedef struct
{
    void (*zone)(const CliDbOutCountCfg *args);
    int (*children)(const CliDbOutCountCfg *args);
    void (*history)(const CliDbOutCountCfg *args);
} CliDbOutCountNs;
EMBED_TABLE_LAYOUT(CliDbOutCountNs, zone, children, history);
EMBED_TABLE_STORAGE CliDbOutCountNs cli_db_out_count EMBED_UNUSED = {
    .zone = cli_db_zone,
    .children = cli_db_children,
    .history = cli_db_history,
};

typedef struct
{
    int (*run)(const CliDbOutFailureCfg *args);
} CliDbOutFailureNs;
EMBED_TABLE_LAYOUT(CliDbOutFailureNs, run);
EMBED_TABLE_STORAGE CliDbOutFailureNs cli_db_out_failure EMBED_UNUSED = {
    .run = cli_db_run,
};

typedef struct
{
    int (*rows)(const CliDbOutFailureOpenCfg *args);
} CliDbOutFailureOpenNs;
EMBED_TABLE_LAYOUT(CliDbOutFailureOpenNs, rows);
EMBED_TABLE_STORAGE CliDbOutFailureOpenNs cli_db_out_failure_open EMBED_UNUSED = {
    .rows = cli_db_rows,
};

typedef struct
{
    int (*export)(const CliDbOutCfg *args);
} CliDbOutNs;
EMBED_TABLE_LAYOUT(CliDbOutNs, export);
EMBED_TABLE_STORAGE CliDbOutNs cli_db_out EMBED_UNUSED = {
    .export = cli_db_export,
};

typedef struct
{
    void (*statement_at)(const CliDbOutSizeCfg *args);
    void (*statements)(const CliDbOutSizeCfg *args);
} CliDbOutSizeNs;
EMBED_TABLE_LAYOUT(CliDbOutSizeNs, statement_at, statements);
EMBED_TABLE_STORAGE CliDbOutSizeNs cli_db_out_size EMBED_UNUSED = {
    .statement_at = cli_db_statement_at,
    .statements = cli_db_statements,
};

#endif
