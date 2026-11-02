// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_internal.h: what the database sources share: a connection's place, its two links and what stops each, its ssh
// tunnel, SQLite's process, and the rows a statement gave, buffered in a file of their own as they are read
#ifndef DB_INTERNAL_H
#define DB_INTERNAL_H
typedef struct CliDbLinkCtx CliDbLinkCtx;
typedef struct CliDbLinkFailureCtx CliDbLinkFailureCtx;
typedef struct CliDbFailureCtx CliDbFailureCtx;
typedef struct CliDbLinkFailureDoneCtx CliDbLinkFailureDoneCtx;
typedef struct CliDbLinkFailureStatementCtx CliDbLinkFailureStatementCtx;
typedef struct CliDbStatementCtx CliDbStatementCtx;
typedef struct CliDbContextCtx CliDbContextCtx;
typedef struct CliDbHostCtx CliDbHostCtx;
typedef struct CliDbFailureStatementCtx CliDbFailureStatementCtx;
typedef struct CliDbFailureContextCtx CliDbFailureContextCtx;
typedef struct CliDbLinkContextCtx CliDbLinkContextCtx;
typedef struct CliDbFailureFileCtx CliDbFailureFileCtx;
typedef struct CliDbOutCtx CliDbOutCtx;
typedef struct CliDbOutNameCtx CliDbOutNameCtx;
typedef struct CliDbCtx CliDbCtx;
typedef struct CliDbErrMsgCtx CliDbErrMsgCtx;
typedef struct CliDbStopCtx CliDbStopCtx;

#include "../command_line/callback_cfg.h"
#include "../toolchains/toolchains.h"
#include "db.h"
#include "embed_dispatch_layout.h"
#include "mysql.h"
#include "pg.h"

// what the connections and SQLite's processes occupy in the cellblock, each a cell taken as it opens and given back as
// it closes (db_open.c)
extern CellOwner g_cli_db_cells;

// the most connections open at once; how long, in milliseconds, a connection, a tunnel or a sign-in is waited for; how
// many statements a connection's history stores; and how often, in rows, the place of a row buffered is marked, and how
// many marks are stored
#define CLI_DB_OPENS 4u
#define CLI_DB_WAIT 15000ull
#define CLI_DB_HISTORY 1000u
#define CLI_DB_RESULTS_EVERY 64u
#define CLI_DB_RESULTS_MARKS (1u << 18u)

// the SQLite processes at once, two links for each connection; the bytes SQLite's process is read in at a time, the
// most bytes of a line it prints, and of the texts of the rows of one line; and the rows requested from it in one line
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

// A SQLite database open in a process of the tree's Python: the process, what it printed read a line at a time, whether
// a transaction is open, whether the statement running has rows left, how many it gave, the sqlite3 module's version,
// the lock its input is guarded by, the texts of the rows of a line, how the statement began, the values of a row, and
// the PATH its process runs with
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

// The rows of the last statement run that gave rows, as they have been read: their columns, the file they are buffered
// in a line each, how many are buffered and the bytes they take, the place of every CLI_DB_RESULTS_EVERYth, the tag the
// statement ended with where it has, and the rows read past those buffered, as an export wrote them out
typedef struct
{
    int has_columns;
    CliDbColumn columns[CLI_DB_COLUMNS];
    unsigned int column_count;
    char path[CLI_PATH_MAX_LEN];
    unsigned long long rows;
    unsigned long long size;
    unsigned long long marks[CLI_DB_RESULTS_MARKS];
    int has_done;
    char done[CLI_DB_TAG];
    unsigned long long beyond;
    char pending[1u << 20u];
    unsigned long long pending_size;
} CliDbResults;

struct CliDbOpen
{
    CliDbConnection config;
    char root[CLI_PATH_MAX_LEN];
    char password[CLI_NAME_MAX_LEN];
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
    CliDbResults results;
    CliLock results_lock;
    char version[CLI_NAME_MAX_LEN];
    char zone[CLI_NAME_MAX_LEN];
    CliLock zone_lock;
};

// a link opened to `config`'s database at `host`:`port`, read-only where `read_only`: 1, or 0 with the failure given;
// how a statement began on it, its head, stored by the link; up to `count` rows more handed to `each_row`; every row a
// statement gives handed to `each_row`; the transaction's state, idle, open or failed; what stops it into `stop`: 1, or
// 0 where nothing does; what the server noted while the last statement ran, each handed to `each`; the link ended; and
// a stop sent (db_link.c)
int cli_db_link_open(const CliDbLinkCtx *args);
int cli_db_link_start(const CliDbLinkFailureStatementCtx *args);
const CliDbHead *cli_db_link_head(const CliDbCtx *args);
int cli_db_link_rows(const CliDbLinkFailureCtx *args);
int cli_db_link_all(const CliDbLinkFailureStatementCtx *args);
const char *cli_db_link_transaction(const CliDbCtx *args);
int cli_db_link_stopper(const CliDbCtx *args);
void cli_db_link_notices(const CliDbLinkContextCtx *args);
void cli_db_link_close(const CliDbLinkCtx *args);
int cli_db_stop_send(const CliDbStopCtx *args);

// SQLite's process started on the database `file`, read-only where `read_only`; a statement started on it; up to
// `count` rows more of it; whether a transaction is open; an interrupt sent to the statement it runs; and the process
// ended (db_lite.c)
CliDbLite *cli_db_lite_open(const CliDbFailureFileCtx *args);
int cli_db_lite_start(const CliDbFailureStatementCtx *args);
int cli_db_lite_rows(const CliDbFailureCtx *args);
int cli_db_lite_interrupt(const CliDbErrMsgCtx *args);
void cli_db_lite_close(const CliDbErrMsgCtx *args);

// the shim SQLite's process runs (db_lite_shim.c)
extern const char g_cli_db_lite_shim[];

// a tunnel opened through the ssh `through` names to `host`:`port`: 1, or 0 with the failure given; whether it stands;
// and it ended (db_tunnel.c)
int cli_db_tunnel_open(const CliDbHostCtx *args);
int cli_db_tunnel_alive(const CliDbHostCtx *args);
void cli_db_tunnel_close(const CliDbHostCtx *args);

typedef struct
{
    void *context;
    CliDbLink *link;
    CliDbFailure *failure;
} CliEachLinkFailureCfg;

// the query link acquired and handed to `work`, made again where its connection broke; and the same of the schema's
// link. Each what `work` gives, 0 with the failure given (db_open.c)
int cli_db_with_query(const CliDbFailureContextCtx *args);
int cli_db_with_meta(const CliDbFailureContextCtx *args);
// the zone the session reads times in read again (db_open.c)
void cli_db_zone_read(const CliDbFailureContextCtx *args);

// the rows of a statement that began on the query link buffered anew, its first `page` rows read into the file and
// written into `out` as a JSON array: 1, or 0 with the failure given; and the statement recorded in the connection's
// history with how long it took and its tag, or the failure where `failed` (db_results.c, db_history.c)
int cli_db_results_begin(const CliDbLinkFailureDoneCtx *args);
// the lines of the rows buffered from `start` to `end`, each a JSON array of the row's values, handed to `each`, under
// the buffered rows' lock (db_results.c)
void cli_db_results_lines(const CliDbContextCtx *args);
void cli_db_remembered(const CliDbStatementCtx *args);

// `name` quoted as the dialect quotes a name, and `text` as a string it reads back as itself (db_children.c)
void cli_db_quoted_name(const CliDbOutNameCtx *args);
void cli_db_literal(const CliDbOutCtx *args);

struct CliDbLinkCtx
{
    CliDbLink *link;
    const char *root;
    const CliDbConnection *config;
    const char *password;
    const char *host;
    unsigned int port;
    int runs;
    CliDbFailure *failure;
};

struct CliDbLinkFailureCtx
{
    CliDbLink *link;
    CliDbFailure *failure;
    unsigned long long count;
    CliDbRowFn each_row;
    void *context;
    int *done;
    char *tag;
};

struct CliDbFailureCtx
{
    CliDbFailure *failure;
    unsigned long long count;
    CliDbRowFn each_row;
    void *context;
    int *done;
    char *tag;
    CliDbLite *lite;
};

struct CliDbLinkFailureDoneCtx
{
    CliDbLink *link;
    CliDbFailure *failure;
    int *done;
    char *tag;
    CliDbOpen *open;
    unsigned long long page;
    VerbaLinea *out;
};

struct CliDbLinkFailureStatementCtx
{
    CliDbLink *link;
    CliDbFailure *failure;
    const char *statement;
    unsigned long long size;
    CliDbRowFn each_row;
    void *context;
};

struct CliDbStatementCtx
{
    const char *statement;
    unsigned long long size;
    CliDbOpen *open;
    unsigned long long took;
    const char *err_text;
    int failed;
};

struct CliDbContextCtx
{
    void *context;
    CliDbResults *results;
    unsigned long long start;
    unsigned long long end;
    void (*each_callback)(const CliEachLineSizeCfg *args);
};

struct CliDbHostCtx
{
    const char *host;
    unsigned int port;
    CliDbFailure *failure;
    CliDbTunnel *tunnel;
    const char *through;
};

struct CliDbFailureStatementCtx
{
    CliDbFailure *failure;
    const char *statement;
    unsigned long long size;
    CliDbLite *lite;
};

struct CliDbFailureContextCtx
{
    CliDbFailure *failure;
    void *context;
    CliDbOpen *open;
    int (*work)(const CliEachLinkFailureCfg *args);
};

struct CliDbLinkContextCtx
{
    CliDbLink *link;
    void *context;
    void (*each)(const CliEachNoticeCfg *args);
};

struct CliDbFailureFileCtx
{
    CliDbFailure *failure;
    const char *file;
    int read_only;
};

struct CliDbOutCtx
{
    VerbaLinea *out;
    unsigned int dialect;
    const char *text;
};

struct CliDbOutNameCtx
{
    VerbaLinea *out;
    const char *name;
    unsigned int dialect;
};

struct CliDbCtx
{
    const CliDbLink *link_read;
    CliDbStop *stop;
};

struct CliDbErrMsgCtx
{
    VerbaLinea *err_msg;
    CliDbLite *lite;
};

struct CliDbStopCtx
{
    const CliDbStop *stop_read;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*link_open)(const CliDbLinkCtx *args);
    void (*link_close)(const CliDbLinkCtx *args);
} CliDbLinkNs;
EMBED_TABLE_LAYOUT(CliDbLinkNs, link_open, link_close);
EMBED_TABLE_STORAGE CliDbLinkNs cli_db_link_internal EMBED_UNUSED = {
    .link_open = cli_db_link_open,
    .link_close = cli_db_link_close,
};

typedef struct
{
    int (*link_start)(const CliDbLinkFailureStatementCtx *args);
    int (*link_all)(const CliDbLinkFailureStatementCtx *args);
} CliDbLinkFailureStatementNs;
EMBED_TABLE_LAYOUT(CliDbLinkFailureStatementNs, link_start, link_all);
EMBED_TABLE_STORAGE CliDbLinkFailureStatementNs cli_db_link_failure_statement_internal EMBED_UNUSED = {
    .link_start = cli_db_link_start,
    .link_all = cli_db_link_all,
};

typedef struct
{
    const CliDbHead *(*link_head)(const CliDbCtx *args);
    const char *(*link_transaction)(const CliDbCtx *args);
    int (*link_stopper)(const CliDbCtx *args);
} CliDbNsTable;
EMBED_TABLE_LAYOUT(CliDbNsTable, link_head, link_transaction, link_stopper);
EMBED_TABLE_STORAGE CliDbNsTable cli_db_internal EMBED_UNUSED = {
    .link_head = cli_db_link_head,
    .link_transaction = cli_db_link_transaction,
    .link_stopper = cli_db_link_stopper,
};

typedef struct
{
    int (*link_rows)(const CliDbLinkFailureCtx *args);
} CliDbLinkFailureNs;
EMBED_TABLE_LAYOUT(CliDbLinkFailureNs, link_rows);
EMBED_TABLE_STORAGE CliDbLinkFailureNs cli_db_link_failure_internal EMBED_UNUSED = {
    .link_rows = cli_db_link_rows,
};

typedef struct
{
    void (*link_notices)(const CliDbLinkContextCtx *args);
} CliDbLinkContextNs;
EMBED_TABLE_LAYOUT(CliDbLinkContextNs, link_notices);
EMBED_TABLE_STORAGE CliDbLinkContextNs cli_db_link_context_internal EMBED_UNUSED = {
    .link_notices = cli_db_link_notices,
};

typedef struct
{
    int (*stop_send)(const CliDbStopCtx *args);
} CliDbStopNs;
EMBED_TABLE_LAYOUT(CliDbStopNs, stop_send);
EMBED_TABLE_STORAGE CliDbStopNs cli_db_stop_internal EMBED_UNUSED = {
    .stop_send = cli_db_stop_send,
};

typedef struct
{
    CliDbLite *(*lite_open)(const CliDbFailureFileCtx *args);
} CliDbFailureFileNs;
EMBED_TABLE_LAYOUT(CliDbFailureFileNs, lite_open);
EMBED_TABLE_STORAGE CliDbFailureFileNs cli_db_failure_file_internal EMBED_UNUSED = {
    .lite_open = cli_db_lite_open,
};

typedef struct
{
    int (*lite_start)(const CliDbFailureStatementCtx *args);
} CliDbFailureStatementNs;
EMBED_TABLE_LAYOUT(CliDbFailureStatementNs, lite_start);
EMBED_TABLE_STORAGE CliDbFailureStatementNs cli_db_failure_statement_internal EMBED_UNUSED = {
    .lite_start = cli_db_lite_start,
};

typedef struct
{
    int (*lite_rows)(const CliDbFailureCtx *args);
} CliDbFailureNs;
EMBED_TABLE_LAYOUT(CliDbFailureNs, lite_rows);
EMBED_TABLE_STORAGE CliDbFailureNs cli_db_failure_internal EMBED_UNUSED = {
    .lite_rows = cli_db_lite_rows,
};

typedef struct
{
    int (*lite_interrupt)(const CliDbErrMsgCtx *args);
    void (*lite_close)(const CliDbErrMsgCtx *args);
} CliDbErrMsgNsTable;
EMBED_TABLE_LAYOUT(CliDbErrMsgNsTable, lite_interrupt, lite_close);
EMBED_TABLE_STORAGE CliDbErrMsgNsTable cli_db_err_msg_internal EMBED_UNUSED = {
    .lite_interrupt = cli_db_lite_interrupt,
    .lite_close = cli_db_lite_close,
};

typedef struct
{
    int (*tunnel_open)(const CliDbHostCtx *args);
    int (*tunnel_alive)(const CliDbHostCtx *args);
    void (*tunnel_close)(const CliDbHostCtx *args);
} CliDbHostNs;
EMBED_TABLE_LAYOUT(CliDbHostNs, tunnel_open, tunnel_alive, tunnel_close);
EMBED_TABLE_STORAGE CliDbHostNs cli_db_host_internal EMBED_UNUSED = {
    .tunnel_open = cli_db_tunnel_open,
    .tunnel_alive = cli_db_tunnel_alive,
    .tunnel_close = cli_db_tunnel_close,
};

typedef struct
{
    int (*with_query)(const CliDbFailureContextCtx *args);
    int (*with_meta)(const CliDbFailureContextCtx *args);
    void (*zone_read)(const CliDbFailureContextCtx *args);
} CliDbFailureContextNs;
EMBED_TABLE_LAYOUT(CliDbFailureContextNs, with_query, with_meta, zone_read);
EMBED_TABLE_STORAGE CliDbFailureContextNs cli_db_failure_context_internal EMBED_UNUSED = {
    .with_query = cli_db_with_query,
    .with_meta = cli_db_with_meta,
    .zone_read = cli_db_zone_read,
};

typedef struct
{
    int (*results_begin)(const CliDbLinkFailureDoneCtx *args);
} CliDbLinkFailureDoneNs;
EMBED_TABLE_LAYOUT(CliDbLinkFailureDoneNs, results_begin);
EMBED_TABLE_STORAGE CliDbLinkFailureDoneNs cli_db_link_failure_done_internal EMBED_UNUSED = {
    .results_begin = cli_db_results_begin,
};

typedef struct
{
    void (*results_lines)(const CliDbContextCtx *args);
} CliDbContextNs;
EMBED_TABLE_LAYOUT(CliDbContextNs, results_lines);
EMBED_TABLE_STORAGE CliDbContextNs cli_db_context_internal EMBED_UNUSED = {
    .results_lines = cli_db_results_lines,
};

typedef struct
{
    void (*remembered)(const CliDbStatementCtx *args);
} CliDbStatementNs;
EMBED_TABLE_LAYOUT(CliDbStatementNs, remembered);
EMBED_TABLE_STORAGE CliDbStatementNs cli_db_statement_internal EMBED_UNUSED = {
    .remembered = cli_db_remembered,
};

typedef struct
{
    void (*quoted_name)(const CliDbOutNameCtx *args);
} CliDbOutNameNs;
EMBED_TABLE_LAYOUT(CliDbOutNameNs, quoted_name);
EMBED_TABLE_STORAGE CliDbOutNameNs cli_db_out_name_internal EMBED_UNUSED = {
    .quoted_name = cli_db_quoted_name,
};

typedef struct
{
    void (*literal)(const CliDbOutCtx *args);
} CliDbOutNsTable;
EMBED_TABLE_LAYOUT(CliDbOutNsTable, literal);
EMBED_TABLE_STORAGE CliDbOutNsTable cli_db_out_internal EMBED_UNUSED = {
    .literal = cli_db_literal,
};

#endif
