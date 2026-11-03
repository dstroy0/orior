// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql.h: MySQL and MariaDB, implemented by orior_cli itself in the client/server protocol 4.1 over TCP, with no
// driver: the handshake, a password by mysql_native_password or caching_sha2_password as the account requires, a
// statement by COM_QUERY, and its rows read off the socket a packet at a time, the rest left to wait in the server
// until they are requested. A statement running is stopped by KILL QUERY sent on a connection of its own.
//
// caching_sha2_password signs in by its scramble once the server has the account's password in its cache; until
// then it takes the password itself, which over a connection that is not encrypted goes encrypted by the server's RSA
// key with OAEP padding, as MySQL 8 requires. The connection is not encrypted: a server elsewhere is reached through an
// ssh tunnel (mysql.c, mysql_sign.c, mysql_rows.c,
// mysql_rsa.c)
#ifndef MYSQL_H
#define MYSQL_H
#include "db_wire.h"
#include "embed_dispatch_layout.h"

typedef struct CliMysqlHostCfg CliMysqlHostCfg;
typedef struct CliMysqlFailureCfg CliMysqlFailureCfg;
typedef struct CliMysqlFailureSqlCfg CliMysqlFailureSqlCfg;
typedef struct CliMysqlCfg CliMysqlCfg;
typedef struct CliMysqlCancelCfg CliMysqlCancelCfg;
typedef struct CliMysqlTextCfg CliMysqlTextCfg;

// A connection to a MySQL or MariaDB server
typedef struct CliMysql CliMysql;

// Where and as whom a connection signs in, stored to reach the server again to stop a statement, and the connection's
// id there
typedef struct
{
    char host[CLI_NAME_MAX_LEN];
    unsigned int port;
    char user[CLI_NAME_MAX_LEN];
    char password[CLI_NAME_MAX_LEN];
    unsigned int id;
} CliMysqlCancel;

// a connection to `database` at `host`:`port` as `user`, signed in with `password` as the account requires, within
// `wait` milliseconds: the connection, or NULL with the failure into `failure`; and the connection ended as the
// protocol requires and let go
CliMysql *cli_mysql_connect(const CliMysqlHostCfg *args);
void cli_mysql_close(const CliMysqlFailureSqlCfg *args);

// `size` bytes of `sql` sent as one query and read until it gives rows or ends, how it began recorded as the
// connection's head; up to `count` rows more of the statement running each handed to `each_row`, a tag once it has none
// left into `tag`, sized for CLI_DB_TAG, with `done` set; the rows the statement running has left read past; and every
// row `sql` gives handed to `each_row`, for a question that gives few. Each 1, or 0 with the failure into `failure`
int cli_mysql_start(const CliMysqlFailureSqlCfg *args);
int cli_mysql_rows(const CliMysqlFailureCfg *args);
int cli_mysql_finish(const CliMysqlFailureSqlCfg *args);
int cli_mysql_all(const CliMysqlFailureSqlCfg *args);

// how the statement last started began; whether a transaction is open on the connection; the count of warnings the
// last statement left; the server's version, as it gave it; whether the sign-in sent the password itself, sealed by
// the server's key; and what stops the statement this connection runs, from another thread
const CliDbHead *cli_mysql_head(const CliMysqlCancelCfg *args);
int cli_mysql_in_transaction(const CliMysqlCancelCfg *args);
unsigned int cli_mysql_warnings(const CliMysqlCancelCfg *args);
const char *cli_mysql_version(const CliMysqlCancelCfg *args);
int cli_mysql_seal(const CliMysqlCancelCfg *args);
void cli_mysql_canceller(const CliMysqlCancelCfg *args);

// the statement the connection of `cancel` runs stopped now: 1, or 0 with why given; and `text` as a string MySQL reads
// back as itself
int cli_mysql_cancel_send(const CliMysqlCfg *args);
void cli_mysql_literal(const CliMysqlTextCfg *args);

struct CliMysqlHostCfg
{
    const char *host;
    unsigned int port;
    const char *user;
    const char *password;
    const char *database;
    unsigned long long wait;
    CliDbFailure *failure;
};

struct CliMysqlFailureCfg
{
    CliDbFailure *failure;
    CliMysql *mysql_out;
    unsigned long long count;
    CliDbRowFn each_row;
    void *context;
    int *done;
    char *tag;
};

struct CliMysqlFailureSqlCfg
{
    CliDbFailure *failure;
    CliMysql *mysql_out;
    const char *sql;
    unsigned long long size;
    CliDbRowFn each_row;
    void *context;
};

struct CliMysqlCfg
{
    const CliMysqlCancel *cancel_read;
    VerbaLinea *err_msg;
};

struct CliMysqlCancelCfg
{
    const CliMysql *mysql;
    CliMysqlCancel *cancel;
};

struct CliMysqlTextCfg
{
    const char *text;
    VerbaLinea *out;
};

typedef struct
{
    CliMysql *(*connect)(const CliMysqlHostCfg *args);
} CliMysqlHostNs;
EMBED_TABLE_LAYOUT(CliMysqlHostNs, connect);
EMBED_TABLE_STORAGE CliMysqlHostNs cli_mysql_host EMBED_UNUSED = {
    .connect = cli_mysql_connect,
};

typedef struct
{
    void (*close)(const CliMysqlFailureSqlCfg *args);
    int (*start)(const CliMysqlFailureSqlCfg *args);
    int (*finish)(const CliMysqlFailureSqlCfg *args);
    int (*all)(const CliMysqlFailureSqlCfg *args);
} CliMysqlFailureSqlNs;
EMBED_TABLE_LAYOUT(CliMysqlFailureSqlNs, close, start, finish, all);
EMBED_TABLE_STORAGE CliMysqlFailureSqlNs cli_mysql_failure_sql EMBED_UNUSED = {
    .close = cli_mysql_close,
    .start = cli_mysql_start,
    .finish = cli_mysql_finish,
    .all = cli_mysql_all,
};

typedef struct
{
    int (*rows)(const CliMysqlFailureCfg *args);
} CliMysqlFailureNs;
EMBED_TABLE_LAYOUT(CliMysqlFailureNs, rows);
EMBED_TABLE_STORAGE CliMysqlFailureNs cli_mysql_failure EMBED_UNUSED = {
    .rows = cli_mysql_rows,
};

typedef struct
{
    const CliDbHead *(*head)(const CliMysqlCancelCfg *args);
    int (*in_transaction)(const CliMysqlCancelCfg *args);
    unsigned int (*warnings)(const CliMysqlCancelCfg *args);
    const char *(*version)(const CliMysqlCancelCfg *args);
    int (*seal)(const CliMysqlCancelCfg *args);
    void (*canceller)(const CliMysqlCancelCfg *args);
} CliMysqlCancelNs;
EMBED_TABLE_LAYOUT(CliMysqlCancelNs, head, in_transaction, warnings, version, seal, canceller);
EMBED_TABLE_STORAGE CliMysqlCancelNs cli_mysql_cancel EMBED_UNUSED = {
    .head = cli_mysql_head,
    .in_transaction = cli_mysql_in_transaction,
    .warnings = cli_mysql_warnings,
    .version = cli_mysql_version,
    .seal = cli_mysql_seal,
    .canceller = cli_mysql_canceller,
};

typedef struct
{
    int (*cancel_send)(const CliMysqlCfg *args);
} CliMysqlNs;
EMBED_TABLE_LAYOUT(CliMysqlNs, cancel_send);
EMBED_TABLE_STORAGE CliMysqlNs cli_mysql EMBED_UNUSED = {
    .cancel_send = cli_mysql_cancel_send,
};

typedef struct
{
    void (*literal)(const CliMysqlTextCfg *args);
} CliMysqlTextNs;
EMBED_TABLE_LAYOUT(CliMysqlTextNs, literal);
EMBED_TABLE_STORAGE CliMysqlTextNs cli_mysql_text EMBED_UNUSED = {
    .literal = cli_mysql_literal,
};

#endif
