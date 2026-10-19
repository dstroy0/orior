// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql.h: MySQL and MariaDB, spoken by orior itself in the client/server protocol 4.1 over TCP, with no driver: the
// handshake, a password by mysql_native_password or caching_sha2_password as the account asks, a statement by
// COM_QUERY, and its rows read off the socket a packet at a time, the rest left to wait in the server until they are
// asked for. A statement running is stopped by KILL QUERY sent on a connection of its own.
//
// caching_sha2_password signs in by its scramble once the server holds the account's password in its cache; until
// then it takes the password itself, which over a connection that is not encrypted goes encrypted by the server's RSA
// key with OAEP padding, as MySQL 8 asks. The connection is not encrypted: a server elsewhere is reached through an
// ssh tunnel (mysql.c, mysql_sign.c, mysql_rows.c,
// mysql_rsa.c)
#ifndef MYSQL_H
#define MYSQL_H

#include "db_wire.h"

// A connection to a MySQL or MariaDB server
typedef struct CliMysql CliMysql;

// Where and as whom a connection signs in, kept to reach the server again to stop a statement, and the connection's
// id there
typedef struct
{
    char host[CLI_NAME_LONGEST];
    unsigned int port;
    char user[CLI_NAME_LONGEST];
    char password[CLI_NAME_LONGEST];
    unsigned int id;
} CliMysqlCancel;

// a connection to `database` at `host`:`port` as `user`, signed in with `password` as the account asks, within `wait`
// milliseconds: the connection, or NULL with the failure into `failure`; and the connection ended as the protocol asks
// and let go
CliMysql *cli_mysql_connected(const char *host, unsigned int port, const char *user, const char *password,
                              const char *database, unsigned long long wait, CliDbFailure *failure);
void cli_mysql_closed(CliMysql *mysql);

// `size` bytes of `sql` sent as one query and read until it gives rows or ends, how it began kept as the connection's
// head; up to `count` rows more of the statement running each handed to `told`, a tag once it has none left into
// `tag`, which holds CLI_DB_TAG, with `done` set; the rows the statement running has left read past; and every row
// `sql` gives handed to `told`, for a question that gives few. Each 1, or 0 with the failure into `failure`
int cli_mysql_start(CliMysql *mysql, const char *sql, unsigned long long size, CliDbFailure *failure);
int cli_mysql_rows(CliMysql *mysql, unsigned long long count, CliDbRowTold told, void *context, int *done, char *tag,
                   CliDbFailure *failure);
int cli_mysql_finish(CliMysql *mysql, CliDbFailure *failure);
int cli_mysql_all(CliMysql *mysql, const char *sql, unsigned long long size, CliDbRowTold told, void *context,
                  CliDbFailure *failure);

// how the statement last started began; whether a transaction is open on the connection; the count of warnings the
// last statement left; the server's version, as it gave it; whether the sign-in sent the password itself, sealed by
// the server's key; and what stops the statement this connection runs, from another thread
const CliDbHead *cli_mysql_head(const CliMysql *mysql);
int cli_mysql_in_transaction(const CliMysql *mysql);
unsigned int cli_mysql_warnings(const CliMysql *mysql);
const char *cli_mysql_version(const CliMysql *mysql);
int cli_mysql_sealed(const CliMysql *mysql);
void cli_mysql_canceller(const CliMysql *mysql, CliMysqlCancel *cancel);

// the statement the connection of `cancel` runs stopped now: 1, or 0 with why said; and `text` as a string MySQL reads
// back as itself
int cli_mysql_cancel_sent(const CliMysqlCancel *cancel, CliLine *said);
void cli_mysql_literal(const char *text, CliLine *out);

#endif
