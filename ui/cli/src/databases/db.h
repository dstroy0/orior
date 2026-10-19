// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db.h: the database window's connections: the ones a tree names in databases.json at its top folder, each a
// PostgreSQL, a MySQL or MariaDB, or a SQLite database, opened as two links, one that runs the reader's statements and
// one that reads the schema, and the other never waits on.
//
// PostgreSQL and MySQL are spoken by pg.h and mysql.h; SQLite is read through the sqlite3 module of the tree's Python,
// in a process of its own that takes one statement at a time and stops one when asked. A password goes to the
// system's keychain, and never to a file; a connection reached through ssh goes through the system's ssh with the
// reader's own config, keys and agent, or the ssh ORIOR_SSH names, as a tree on another machine does, and a link whose
// connection broke is made again, tunnel and all, at the next statement.
//
// A statement's rows are held as they are read, a page at a time as the grid asks for them, and the rest wait in the
// server. An export writes the rows held and reads the rest from the same run. Before a statement runs, an UPDATE or a
// DELETE with no WHERE, or a TRUNCATE, asks on every connection with the count of the rows it would reach, and on a
// connection marked as one that matters a statement that writes asks, or the connection is opened read-only, the server
// itself refusing a write, as the reader sets it. Each statement run is kept in its connection's history in orior's
// own folder. What a call gives is written as JSON, as the window reads it (db_config.c, db_open.c, db_link.c,
// db_lite.c, db_lite_shim.c, db_tunnel.c, db_run.c, db_held.c, db_export.c, db_children.c, db_history.c,
// platform/windows_keychain.c,
// platform/posix_keychain.c)
#ifndef DB_H
#define DB_H

#include "../text/json_tree.h"
#include "db_wire.h"

// A connection a tree names: its name; its kind, postgres, mysql or sqlite; its host, port, user and database; a SQLite
// database's file, by its path in the tree or whole; the machine, as ssh names it, a tunnel to the server goes
// through; whether it is one that matters, whose writes are guarded; and how, read-only or ask
typedef struct
{
    char name[CLI_NAME_LONGEST];
    char kind[16];
    char host[CLI_NAME_LONGEST];
    unsigned int port;
    char user[CLI_NAME_LONGEST];
    char database[CLI_NAME_LONGEST];
    char file[CLI_PATH_LONGEST];
    char ssh[CLI_NAME_LONGEST];
    int matters;
    char guard[16];
} CliDbConnection;

// A connection open: its links, its tunnel and the rows it holds
typedef struct CliDbOpen CliDbOpen;

// the port a connection's server listens at, its kind's own where none is named; the account the keychain keeps its
// password by into `out`; the connection a tree names `name` into `connection`: 1, or 0 with why said; the connection
// object `node` of `json` read into `connection`: 1, or 0 with why said; the connection kept in databases.json in place
// of the one of its name, or of `was` where it is renamed; and the connection `name` taken out of databases.json and
// its password out of the keychain: each 1, or 0 with why said; and what the window says of each connection, its
// settings and whether it is open, with its transaction's state and its session's zone, the open connections `opens`,
// `count` of them, written into `out`: 1, or 0 with why said (db_config.c)
unsigned int cli_db_port(const CliDbConnection *connection);
void cli_db_account(const CliDbConnection *connection, CliLine *out);
int cli_db_named(const char *root, const char *name, CliDbConnection *connection, CliLine *said);
int cli_db_connection_of(const CliJson *json, unsigned int node, CliDbConnection *connection, CliLine *said);
int cli_db_save(const char *root, const CliDbConnection *connection, const char *was, CliLine *said);
int cli_db_remove(const char *root, const char *name, CliLine *said);
int cli_db_listed(const char *root, CliDbOpen *const *opens, unsigned int count, CliLine *out, CliLine *said);

// `password` kept for `account` in the system's keychain; the password kept for `account` into `out`, which holds
// `room`: 1, or 0 where none is kept; and the password taken out (platform/windows_keychain.c,
// platform/posix_keychain.c)
int cli_keychain_keep(const char *account, const char *password, CliLine *said);
int cli_keychain_read(const char *account, char *out, unsigned long long room);
int cli_keychain_forget(const char *account, CliLine *said);

// `config`'s two links opened, signed in with `password`, or with the one the keychain keeps where it is NULL: the
// connection, or NULL with the failure into `failure`; and its links and its tunnel ended and the connection let go
// (db_open.c)
CliDbOpen *cli_db_connect(const char *root, const CliDbConnection *config, const char *password, CliDbFailure *failure);
void cli_db_close(CliDbOpen *open);
// the connection's name, its server's version, its transaction's state on the query link, busy where a statement holds
// it, and the zone the session reads times in into `out`
const char *cli_db_name(const CliDbOpen *open);
const char *cli_db_version(const CliDbOpen *open);
const char *cli_db_transaction(CliDbOpen *open);
void cli_db_zone(CliDbOpen *open, CliLine *out);

// `size` bytes of `statement` run, asking first where it must and the reader has not `answered`, its first `page` rows
// or what it did written into `out`; the rows held from `from`, `count` of them, read from the run where they are not
// held yet; and the rows held, then the rest the run gives, written to `path` as csv, tsv or json lines, `told` handed
// how many it has written after each 4096 it reads on. Each 1, or 0 with the failure into `failure` (db_run.c,
// db_held.c)
int cli_db_run(CliDbOpen *open, const char *statement, unsigned long long size, int answered, unsigned long long page,
               CliLine *out, CliDbFailure *failure);
int cli_db_rows(CliDbOpen *open, unsigned long long from, unsigned long long count, CliLine *out,
                CliDbFailure *failure);
int cli_db_export(CliDbOpen *open, const char *path, const char *format,
                  void (*told)(void *context, unsigned long long written), void *context, CliLine *out,
                  CliDbFailure *failure);

// the statement running stopped, and the schema's reading stopped: 1, or 0 with why said (db_run.c)
int cli_db_stop(CliDbOpen *open, CliLine *said);
int cli_db_stop_reading(CliDbOpen *open, CliLine *said);

// the level of the schema under `path`, `count` names: the schemas or databases at the top, a schema's objects under
// it, and a table's columns, indexes, keys and triggers under it, written into `out`: 1, or 0 with the failure into
// `failure` (db_children.c)
int cli_db_children(CliDbOpen *open, const char *const *path, unsigned int count, CliLine *out, CliDbFailure *failure);

// the statements run on the connection, the newest first, written into `out`; the span of the statement `size` bytes
// of `text` hold at `offset`, counted in characters, as the editor counts them, as [start, end], null where the place
// stands in no statement; and each statement of `text` by its span in characters (db_history.c)
void cli_db_history(CliDbOpen *open, CliLine *out);
void cli_db_statement_at(const char *text, unsigned long long size, unsigned long long offset, const char *kind,
                         CliLine *out);
void cli_db_statements(const char *text, unsigned long long size, const char *kind, CliLine *out);

#endif
