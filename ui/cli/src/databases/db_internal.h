// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_internal.h: what the database sources share: a connection's place, its two links and what stops each, its ssh
// tunnel, SQLite's process, and the rows a statement gave, held in a file of their own as they are read
#ifndef DB_INTERNAL_H
#define DB_INTERNAL_H

#include "../toolchains/toolchains.h"
#include "db.h"
#include "mysql.h"
#include "pg.h"

// what the connections and SQLite's processes hold in the cellblock, each a cell taken as it opens and given back as
// it closes (db_open.c)
extern CliCellsTally g_cli_db_cells;

// the most connections open at once; how long, in milliseconds, a connection, a tunnel or a sign-in is waited for; how
// many statements a connection's history keeps; and how often, in rows, the place of a row held is marked, and how
// many marks are kept
#define CLI_DB_OPENS 4u
#define CLI_DB_WAIT 15000ull
#define CLI_DB_HISTORY 1000u
#define CLI_DB_HELD_EVERY 64u
#define CLI_DB_HELD_MARKS (1u << 18u)

// the SQLite processes at once, two links for each connection; the bytes SQLite's process is read in at a time, the
// most bytes of a line it says, and of the texts of the rows of one line; and the rows asked of it in one line
#define CLI_DB_LITES (CLI_DB_OPENS * 2u)
#define CLI_DB_LITE_READ (1u << 16u)
#define CLI_DB_LITE_LINE (1u << 20u)
#define CLI_DB_LITE_ROWS 64u

// what kind a link is: none, PostgreSQL, MySQL or SQLite
enum
{
    CLI_DB_LINK_NONE,
    CLI_DB_LINK_PG,
    CLI_DB_LINK_MY,
    CLI_DB_LINK_LITE
};

// A tunnel to the server through ssh: its process, the port of this machine's loopback it forwards, and whether it
// stands
typedef struct
{
    CliSpawned child;
    unsigned int port;
    int open;
} CliDbTunnel;

// A SQLite database open in a process of the tree's Python: the process, what it said read a line at a time, whether a
// transaction is open, whether the statement running has rows left, how many it gave, the sqlite3 module's version,
// the lock its input is held by, the texts of the rows of a line, how the statement began, the values of a row, and the
// PATH its process runs with
typedef struct
{
    CliSpawned child;
    unsigned char in[CLI_DB_LITE_READ];
    unsigned long long start;
    unsigned long long end;
    char line[CLI_DB_LITE_LINE];
    unsigned long long size;
    int in_transaction;
    int reading;
    unsigned long long count;
    char version[64];
    CliLock input_lock;
    char text[CLI_DB_LITE_LINE];
    CliDbHead head;
    CliDbValue row[CLI_DB_COLUMNS];
    char path_text[CLI_PATH_TEXT];
} CliDbLite;

// One link to a database: its kind and the connection of that kind
typedef struct
{
    unsigned int kind;
    CliPg *pg;
    CliMysql *my;
    CliDbLite *lite;
} CliDbLink;

// What stops the statement a link runs, from another thread: its kind and what reaches the server
typedef struct
{
    unsigned int kind;
    CliPgCancel pg;
    CliMysqlCancel my;
    CliDbLite *lite;
} CliDbStop;

// The rows of the last statement run that gave rows, as they have been read: their columns, the file they are held in
// a line each, how many are held and the bytes they take, the place of every CLI_DB_HELD_EVERYth, the tag the
// statement ended with where it has, and the rows read past those held, as an export wrote them out
typedef struct
{
    int has_columns;
    CliDbColumn columns[CLI_DB_COLUMNS];
    unsigned int column_count;
    char path[CLI_PATH_LONGEST];
    unsigned long long rows;
    unsigned long long size;
    unsigned long long marks[CLI_DB_HELD_MARKS];
    int has_done;
    char done[CLI_DB_TAG];
    unsigned long long beyond;
    char pending[1u << 20u];
    unsigned long long pending_size;
} CliDbHeld;

struct CliDbOpen
{
    CliDbConnection config;
    char root[CLI_PATH_LONGEST];
    char password[CLI_NAME_LONGEST];
    CliDbLink query;
    CliLock query_lock;
    CliDbLink meta;
    CliLock meta_lock;
    CliDbStop stop;
    int has_stop;
    CliDbStop meta_stop;
    int has_meta_stop;
    int busy;
    CliLock stop_lock;
    CliDbTunnel tunnel;
    CliLock tunnel_lock;
    CliDbHeld held;
    CliLock held_lock;
    char version[CLI_NAME_LONGEST];
    char zone[CLI_NAME_LONGEST];
    CliLock zone_lock;
};

// a link opened to `config`'s database at `host`:`port`, read-only where `read_only`: 1, or 0 with the failure said;
// how a statement began on it, its head, kept by the link; up to `count` rows more handed to `told`; every row a
// statement gives handed to `told`; the transaction's state, idle, open or failed; what stops it into `stop`: 1, or 0
// where nothing does; what the server noted while the last statement ran, each handed to `each`; the link ended; and a
// stop sent (db_link.c)
int cli_db_link_open(CliDbLink *link, const char *root, const CliDbConnection *config, const char *password,
                     const char *host, unsigned int port, int runs, CliDbFailure *failure);
int cli_db_link_start(CliDbLink *link, const char *statement, unsigned long long size, CliDbFailure *failure);
const CliDbHead *cli_db_link_head(const CliDbLink *link);
int cli_db_link_rows(CliDbLink *link, unsigned long long count, CliDbRowTold told, void *context, int *done, char *tag,
                     CliDbFailure *failure);
int cli_db_link_all(CliDbLink *link, const char *statement, unsigned long long size, CliDbRowTold told, void *context,
                    CliDbFailure *failure);
const char *cli_db_link_transaction(const CliDbLink *link);
int cli_db_link_stopper(const CliDbLink *link, CliDbStop *stop);
void cli_db_link_notices(CliDbLink *link, void (*each)(void *context, const char *notice), void *context);
void cli_db_link_closed(CliDbLink *link);
int cli_db_stop_sent(const CliDbStop *stop, CliLine *said);

// SQLite's process started on the database `file`, read-only where `read_only`; a statement started on it; up to
// `count` rows more of it; whether a transaction is open; an interrupt sent to the statement it runs; and the process
// ended (db_lite.c)
CliDbLite *cli_db_lite_open(const char *file, int read_only, CliDbFailure *failure);
int cli_db_lite_start(CliDbLite *lite, const char *statement, unsigned long long size, CliDbFailure *failure);
int cli_db_lite_rows(CliDbLite *lite, unsigned long long count, CliDbRowTold told, void *context, int *done, char *tag,
                     CliDbFailure *failure);
int cli_db_lite_interrupted(CliDbLite *lite, CliLine *said);
void cli_db_lite_closed(CliDbLite *lite);

// the shim SQLite's process runs (db_lite_shim.c)
extern const char g_cli_db_lite_shim[];

// a tunnel opened through the ssh `through` names to `host`:`port`: 1, or 0 with the failure said; whether it stands;
// and it ended (db_tunnel.c)
int cli_db_tunnel_open(CliDbTunnel *tunnel, const char *through, const char *host, unsigned int port,
                       CliDbFailure *failure);
int cli_db_tunnel_alive(CliDbTunnel *tunnel);
void cli_db_tunnel_closed(CliDbTunnel *tunnel);

// the query link held and handed to `work`, made again where its connection broke; and the same of the schema's link.
// Each what `work` gives, 0 with the failure said (db_open.c)
int cli_db_with_query(CliDbOpen *open, int (*work)(void *context, CliDbLink *link, CliDbFailure *failure),
                      void *context, CliDbFailure *failure);
int cli_db_with_meta(CliDbOpen *open, int (*work)(void *context, CliDbLink *link, CliDbFailure *failure), void *context,
                     CliDbFailure *failure);
// the zone the session reads times in read again (db_open.c)
void cli_db_zone_read(CliDbOpen *open);

// the rows of a statement that began on the query link held anew, its first `page` rows read into the file and
// written into `out` as a JSON array: 1, or 0 with the failure said; and the statement kept in the connection's history
// with how long it took and its tag, or the failure where `failed` (db_held.c, db_history.c)
int cli_db_held_begun(CliDbOpen *open, CliDbLink *link, unsigned long long page, CliLine *out, int *done, char *tag,
                      CliDbFailure *failure);
// the lines of the rows held from `start` to `end`, each a JSON array of the row's values, handed to `each`, under the
// held rows' lock (db_held.c)
void cli_db_held_lines(CliDbHeld *held, unsigned long long start, unsigned long long end,
                       void (*each)(void *context, const char *line, unsigned long long size), void *context);
void cli_db_remembered(CliDbOpen *open, const char *statement, unsigned long long size, unsigned long long took,
                       const char *said, int failed);

// `name` quoted as the dialect quotes a name, and `text` as a string it reads back as itself (db_children.c)
void cli_db_quoted_name(const char *name, unsigned int dialect, CliLine *out);
void cli_db_literal(const char *text, unsigned int dialect, CliLine *out);

#endif
