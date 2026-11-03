// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg.h: PostgreSQL, implemented by orior_cli itself in the frontend and backend protocol 3.0 over TCP, with no driver:
// the startup, a password by SCRAM-SHA-256, md5 or in the clear as the server requires, a statement by the simple query
// protocol, and its rows read off the socket a page at a time, the rest left to wait in the server until they are
// requested. A statement running is stopped by a cancel request on a connection of its own, by the key the server gave
// at the startup.
//
// Each value comes as the server writes it in text, a time with a zone in the session's zone with its offset. The
// connection is not encrypted: a server elsewhere is reached through an ssh tunnel (pg.c, pg_sign.c, pg_rows.c,
// pg_cancel.c)
#ifndef PG_H
#define PG_H
#include "db_wire.h"
#include "embed_dispatch_layout.h"

typedef struct CliPgHostCfg CliPgHostCfg;
typedef struct CliPgFailureCfg CliPgFailureCfg;
typedef struct CliPgFailureSqlCfg CliPgFailureSqlCfg;
typedef struct CliPgCfg CliPgCfg;
typedef struct CliPgCancelCfg CliPgCancelCfg;
typedef struct CliPgTextCfg CliPgTextCfg;
typedef struct CliPgAtCfg CliPgAtCfg;
typedef struct CliPgNameCfg CliPgNameCfg;

// A connection to a PostgreSQL server
typedef struct CliPg CliPg;

// What reaches the server to stop the statement a connection runs: its host and port, the backend's number and its
// secret
typedef struct
{
    char host[CLI_NAME_MAX_LEN];
    unsigned int port;
    unsigned int pid;
    unsigned char secret[256];
    unsigned int secret_size;
} CliPgCancel;

// a connection to `database` at `host`:`port` as `user`, signed in with `password` as the server requires, within
// `wait` milliseconds: the connection, or NULL with the failure into `failure`; and the connection ended as the
// protocol requires and let go
CliPg *cli_pg_connect(const CliPgHostCfg *args);
void cli_pg_close(const CliPgFailureSqlCfg *args);

// `size` bytes of `sql` sent as one query and read until it gives rows or ends, how it began recorded as the
// connection's head; up to `count` rows more of the statement running each handed to `each_row`, its tag into `tag`,
// sized for CLI_DB_TAG, and `done` set once it has none left; the rows the statement running has left read past, its
// tag into `tag`; and every row `sql` gives handed to `each_row`, for a question that gives few. Each 1, or 0 with the
// failure into `failure`
int cli_pg_start(const CliPgFailureSqlCfg *args);
int cli_pg_rows(const CliPgFailureCfg *args);
int cli_pg_finish(const CliPgFailureCfg *args);
int cli_pg_all(const CliPgFailureSqlCfg *args);

// how the statement last started began; the transaction's state at the last ReadyForQuery: I idle, T open, E failed;
// what the server reported of itself, `name`'s value, NULL where it reported none; what it noted while the last
// statement ran, the `at`th, NULL past the last; and what stops the statement this connection runs, from another
// thread: 1, or 0 where the server gave no key
const CliDbHead *cli_pg_head(const CliPgCancelCfg *args);
char cli_pg_status(const CliPgCancelCfg *args);
const char *cli_pg_parameter(const CliPgNameCfg *args);
const char *cli_pg_notice(const CliPgAtCfg *args);
int cli_pg_canceller(const CliPgCancelCfg *args);

// the server requested to stop the statement the connection of `cancel` runs now: 1, or 0 with why given; and `text` as
// a string PostgreSQL reads back as itself
int cli_pg_cancel_send(const CliPgCfg *args);
void cli_pg_literal(const CliPgTextCfg *args);

struct CliPgHostCfg
{
    const char *host;
    unsigned int port;
    const char *user;
    const char *password;
    const char *database;
    unsigned long long wait;
    CliDbFailure *failure;
};

struct CliPgFailureCfg
{
    CliDbFailure *failure;
    CliPg *pg;
    unsigned long long count;
    CliDbRowFn each_row;
    void *context;
    int *done;
    char *tag;
};

struct CliPgFailureSqlCfg
{
    CliDbFailure *failure;
    CliPg *pg;
    const char *sql;
    unsigned long long size;
    CliDbRowFn each_row;
    void *context;
};

struct CliPgCfg
{
    const CliPgCancel *cancel_read;
    VerbaLinea *err_msg;
};

struct CliPgCancelCfg
{
    const CliPg *pg_read;
    CliPgCancel *cancel;
};

struct CliPgTextCfg
{
    const char *text;
    VerbaLinea *out;
};

struct CliPgAtCfg
{
    const CliPg *pg_read;
    unsigned int at;
};

struct CliPgNameCfg
{
    const CliPg *pg_read;
    const char *name;
};

typedef struct
{
    CliPg *(*connect)(const CliPgHostCfg *args);
} CliPgHostNs;
EMBED_TABLE_LAYOUT(CliPgHostNs, connect);
EMBED_TABLE_STORAGE CliPgHostNs cli_pg_host EMBED_UNUSED = {
    .connect = cli_pg_connect,
};

typedef struct
{
    void (*close)(const CliPgFailureSqlCfg *args);
    int (*start)(const CliPgFailureSqlCfg *args);
    int (*all)(const CliPgFailureSqlCfg *args);
} CliPgFailureSqlNs;
EMBED_TABLE_LAYOUT(CliPgFailureSqlNs, close, start, all);
EMBED_TABLE_STORAGE CliPgFailureSqlNs cli_pg_failure_sql EMBED_UNUSED = {
    .close = cli_pg_close,
    .start = cli_pg_start,
    .all = cli_pg_all,
};

typedef struct
{
    int (*rows)(const CliPgFailureCfg *args);
    int (*finish)(const CliPgFailureCfg *args);
} CliPgFailureNs;
EMBED_TABLE_LAYOUT(CliPgFailureNs, rows, finish);
EMBED_TABLE_STORAGE CliPgFailureNs cli_pg_failure EMBED_UNUSED = {
    .rows = cli_pg_rows,
    .finish = cli_pg_finish,
};

typedef struct
{
    const CliDbHead *(*head)(const CliPgCancelCfg *args);
    char (*status)(const CliPgCancelCfg *args);
    int (*canceller)(const CliPgCancelCfg *args);
} CliPgCancelNs;
EMBED_TABLE_LAYOUT(CliPgCancelNs, head, status, canceller);
EMBED_TABLE_STORAGE CliPgCancelNs cli_pg_cancel EMBED_UNUSED = {
    .head = cli_pg_head,
    .status = cli_pg_status,
    .canceller = cli_pg_canceller,
};

typedef struct
{
    const char *(*parameter)(const CliPgNameCfg *args);
} CliPgNameNs;
EMBED_TABLE_LAYOUT(CliPgNameNs, parameter);
EMBED_TABLE_STORAGE CliPgNameNs cli_pg_name EMBED_UNUSED = {
    .parameter = cli_pg_parameter,
};

typedef struct
{
    const char *(*notice)(const CliPgAtCfg *args);
} CliPgAtNs;
EMBED_TABLE_LAYOUT(CliPgAtNs, notice);
EMBED_TABLE_STORAGE CliPgAtNs cli_pg_at EMBED_UNUSED = {
    .notice = cli_pg_notice,
};

typedef struct
{
    int (*cancel_send)(const CliPgCfg *args);
} CliPgNs;
EMBED_TABLE_LAYOUT(CliPgNs, cancel_send);
EMBED_TABLE_STORAGE CliPgNs cli_pg EMBED_UNUSED = {
    .cancel_send = cli_pg_cancel_send,
};

typedef struct
{
    void (*literal)(const CliPgTextCfg *args);
} CliPgTextNs;
EMBED_TABLE_LAYOUT(CliPgTextNs, literal);
EMBED_TABLE_STORAGE CliPgTextNs cli_pg_text EMBED_UNUSED = {
    .literal = cli_pg_literal,
};

#endif
