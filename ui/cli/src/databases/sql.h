// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sql.h: SQL as the database window reads it before it runs (sql.c reads tokens and statements, sql_does.c what a
// statement does): the statements of a text, the one a place in it is in, whether one writes, an UPDATE or a
// DELETE that reaches every row of its table, and the object a statement that changes the schema names.
//
// The text is read as each database writes it: PostgreSQL's dollar quotes, its `E''` strings and its nested comments;
// MySQL's `#` comments, its backslash escapes and its backquoted names; SQLite's names in brackets. A semicolon inside
// a string, a quoted name, a comment, a dollar quote or the BEGIN and END of a trigger's or a routine's body does not
// end a statement
#ifndef SQL_H
#define SQL_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliSqlTextCfg CliSqlTextCfg;
typedef struct CliSqlTextLengthCfg CliSqlTextLengthCfg;
typedef struct CliSqlTextLengthDialectCfg CliSqlTextLengthDialectCfg;
typedef struct CliSqlLengthCfg CliSqlLengthCfg;
typedef struct CliSqlLengthDialectCfg CliSqlLengthDialectCfg;
typedef struct CliSqlCfg CliSqlCfg;

// the SQL each database writes
enum
{
    CLI_SQL_POSTGRES,
    CLI_SQL_MYSQL,
    CLI_SQL_SQLITE
};

// a token's types
enum
{
    CLI_SQL_WORD,
    CLI_SQL_NAME,
    CLI_SQL_TEXT,
    CLI_SQL_NUMBER,
    CLI_SQL_MARK,
    CLI_SQL_COMMENT,
    CLI_SQL_SPACE
};

// what a statement does: reads and changes nothing, changes rows, changes the schema, begins, commits or rolls back a
// transaction, or anything else, a setting, a database chosen
enum
{
    CLI_DOES_READS,
    CLI_DOES_WRITES,
    CLI_DOES_SHAPES,
    CLI_DOES_TRANSACTION,
    CLI_DOES_OTHER
};

// A token: its type and where it starts and ends in the text
typedef struct
{
    unsigned int type;
    unsigned long long start;
    unsigned long long end;
} CliSqlToken;

// A statement of a text: where it starts and ends, past the spaces and comments around it and before its semicolon
typedef struct
{
    unsigned long long start;
    unsigned long long end;
} CliSqlSpan;

// The object a statement that changes the schema names: what it does to it, its type and its name as written, and,
// for an index or a trigger, the table it is on where `has_on` is 1
typedef struct
{
    char verb[CLI_NAME_MAX_LEN];
    char type[CLI_NAME_MAX_LEN];
    char name[CLI_PATH_MAX_LEN];
    char on[CLI_PATH_MAX_LEN];
    int has_on;
} CliShaped;

// the dialect a database's name writes: mysql and mariadb, sqlite, and PostgreSQL's for any other
unsigned int cli_sql_dialect(const CliSqlCfg *args);
// the token that starts at `*at` of `length` bytes of `text`, every byte in one, into `token`, `*at` moved past it: 1,
// or 0 at the text's end
int cli_sql_token(const CliSqlTextLengthDialectCfg *args);
typedef struct
{
    void *context;
    CliSqlSpan span;
} CliEachSpanCfg;

// each statement of a text passed to `each` in order
void cli_sql_statements(const CliSqlTextLengthCfg *args);
// the statement of a text the place `at` is in, into `span`: within it, at its semicolon, or after it on its last
// line with nothing but spaces and a comment between. 1, or 0 where the place is in no statement, on a blank line
// or in a comment of its own
int cli_sql_statement_at(const CliSqlTextCfg *args);
// what a statement does (sql_does.c); whether running it changes anything the database stores; the query that counts
// the rows it reaches into `out` where it is an UPDATE or a DELETE with no WHERE at its own level, or a TRUNCATE: 1,
// or 0 where it is none; and the object a statement that shapes one makes, changes or drops: 1, or 0 where it shapes
// none
unsigned int cli_sql_does(const CliSqlLengthCfg *args);
int cli_sql_changes(const CliSqlLengthCfg *args);
int cli_sql_unbounded(const CliSqlLengthDialectCfg *args);
int cli_sql_shape(const CliSqlLengthCfg *args);

struct CliSqlTextCfg
{
    const char *text;
    unsigned long long length;
    unsigned int dialect;
    unsigned long long at_value;
    CliSqlSpan *span;
};

struct CliSqlTextLengthCfg
{
    const char *text;
    unsigned long long length;
    unsigned int dialect;
    void (*each)(const CliEachSpanCfg *args);
    void *context;
};

struct CliSqlTextLengthDialectCfg
{
    const char *text;
    unsigned long long length;
    unsigned int dialect;
    unsigned long long *at;
    CliSqlToken *token;
};

struct CliSqlLengthCfg
{
    unsigned long long length;
    unsigned int dialect;
    const char *statement;
    CliShaped *shaped;
};

struct CliSqlLengthDialectCfg
{
    unsigned long long length;
    unsigned int dialect;
    const char *statement;
    VerbaLinea *out;
};

struct CliSqlCfg
{
    const char *name;
};

typedef struct
{
    unsigned int (*dialect)(const CliSqlCfg *args);
} CliSqlNs;
EMBED_TABLE_LAYOUT(CliSqlNs, dialect);
EMBED_TABLE_STORAGE CliSqlNs cli_sql EMBED_UNUSED = {
    .dialect = cli_sql_dialect,
};

typedef struct
{
    int (*token)(const CliSqlTextLengthDialectCfg *args);
} CliSqlTextLengthDialectNs;
EMBED_TABLE_LAYOUT(CliSqlTextLengthDialectNs, token);
EMBED_TABLE_STORAGE CliSqlTextLengthDialectNs cli_sql_text_length_dialect EMBED_UNUSED = {
    .token = cli_sql_token,
};

typedef struct
{
    void (*statements)(const CliSqlTextLengthCfg *args);
} CliSqlTextLengthNs;
EMBED_TABLE_LAYOUT(CliSqlTextLengthNs, statements);
EMBED_TABLE_STORAGE CliSqlTextLengthNs cli_sql_text_length EMBED_UNUSED = {
    .statements = cli_sql_statements,
};

typedef struct
{
    int (*statement_at)(const CliSqlTextCfg *args);
} CliSqlTextNs;
EMBED_TABLE_LAYOUT(CliSqlTextNs, statement_at);
EMBED_TABLE_STORAGE CliSqlTextNs cli_sql_text EMBED_UNUSED = {
    .statement_at = cli_sql_statement_at,
};

typedef struct
{
    unsigned int (*does)(const CliSqlLengthCfg *args);
    int (*changes)(const CliSqlLengthCfg *args);
    int (*shape)(const CliSqlLengthCfg *args);
} CliSqlLengthNs;
EMBED_TABLE_LAYOUT(CliSqlLengthNs, does, changes, shape);
EMBED_TABLE_STORAGE CliSqlLengthNs cli_sql_length EMBED_UNUSED = {
    .does = cli_sql_does,
    .changes = cli_sql_changes,
    .shape = cli_sql_shape,
};

typedef struct
{
    int (*unbounded)(const CliSqlLengthDialectCfg *args);
} CliSqlLengthDialectNs;
EMBED_TABLE_LAYOUT(CliSqlLengthDialectNs, unbounded);
EMBED_TABLE_STORAGE CliSqlLengthDialectNs cli_sql_length_dialect EMBED_UNUSED = {
    .unbounded = cli_sql_unbounded,
};

#endif
