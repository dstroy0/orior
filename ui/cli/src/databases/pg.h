// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg.h: PostgreSQL, spoken by orior itself in the frontend and backend protocol 3.0 over TCP, with no driver: the
// startup, a password by SCRAM-SHA-256, md5 or in the clear as the server asks, a statement by the simple query
// protocol, and its rows read off the socket a page at a time, the rest left to wait in the server until they are
// asked for. A statement running is stopped by a cancel request on a connection of its own, by the key the server gave
// at the startup.
//
// Each value comes as the server writes it in text, a time with a zone in the session's zone with its offset. The
// connection is not encrypted: a server elsewhere is reached through an ssh tunnel (pg.c, pg_sign.c, pg_rows.c,
// pg_cancel.c)
#ifndef PG_H
#define PG_H

#include "db_wire.h"

// A connection to a PostgreSQL server
typedef struct CliPg CliPg;

// What reaches the server to stop the statement a connection runs: its host and port, the backend's number and its
// secret
typedef struct
{
    char host[CLI_NAME_LONGEST];
    unsigned int port;
    unsigned int pid;
    unsigned char secret[256];
    unsigned int secret_size;
} CliPgCancel;

// a connection to `database` at `host`:`port` as `user`, signed in with `password` as the server asks, within `wait`
// milliseconds: the connection, or NULL with the failure into `failure`; and the connection ended as the protocol asks
// and let go
CliPg *cli_pg_connected(const char *host, unsigned int port, const char *user, const char *password,
                        const char *database, unsigned long long wait, CliDbFailure *failure);
void cli_pg_closed(CliPg *pg);

// `size` bytes of `sql` sent as one query and read until it gives rows or ends, how it began kept as the connection's
// head; up to `count` rows more of the statement running each handed to `told`, its tag into `tag`, which holds
// CLI_DB_TAG, and `done` set once it has none left; the rows the statement running has left read past, its tag into
// `tag`; and every row `sql` gives handed to `told`, for a question that gives few. Each 1, or 0 with the failure into
// `failure`
int cli_pg_start(CliPg *pg, const char *sql, unsigned long long size, CliDbFailure *failure);
int cli_pg_rows(CliPg *pg, unsigned long long count, CliDbRowTold told, void *context, int *done, char *tag,
                CliDbFailure *failure);
int cli_pg_finish(CliPg *pg, char *tag, CliDbFailure *failure);
int cli_pg_all(CliPg *pg, const char *sql, unsigned long long size, CliDbRowTold told, void *context,
               CliDbFailure *failure);

// how the statement last started began; the transaction's state at the last ReadyForQuery: I idle, T open, E failed;
// what the server said of itself, `name`'s value, NULL where it said none; what it noted while the last statement ran,
// the `at`th, NULL past the last; and what stops the statement this connection runs, from another thread: 1, or 0
// where the server gave no key
const CliDbHead *cli_pg_head(const CliPg *pg);
char cli_pg_status(const CliPg *pg);
const char *cli_pg_parameter(const CliPg *pg, const char *name);
const char *cli_pg_notice(const CliPg *pg, unsigned int at);
int cli_pg_canceller(const CliPg *pg, CliPgCancel *cancel);

// the server asked to stop the statement the connection of `cancel` runs now: 1, or 0 with why said; and `text` as a
// string PostgreSQL reads back as itself
int cli_pg_cancel_sent(const CliPgCancel *cancel, CliLine *said);
void cli_pg_literal(const char *text, CliLine *out);

#endif
