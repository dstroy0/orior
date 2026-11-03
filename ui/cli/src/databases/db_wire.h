// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_wire.h: what the database connections share, PostgreSQL's and MySQL's alike: what a server reported went wrong, a
// result's columns, how a statement began, and each row's values as the server wrote them (db_wire.c)
#ifndef DB_WIRE_H
#define DB_WIRE_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliDbWireCfg CliDbWireCfg;
typedef struct CliDbWireOutCfg CliDbWireOutCfg;

// the bytes of each field of a failure; the most columns a result has, as PostgreSQL's 1664 targets; the bytes of a
// column's type name; and of the tag a statement ended with
#define CLI_DB_FIELD 2048u
#define CLI_DB_COLUMNS 1664u
#define CLI_DB_KIND 64u
#define CLI_DB_TAG 512u

// What a server reported went wrong, in its own fields: the place in the statement it names, counted in characters from
// 1, or 0
typedef struct
{
    char message[CLI_DB_FIELD];
    char code[64];
    char detail[CLI_DB_FIELD];
    char hint[CLI_DB_FIELD];
    unsigned long long position;
} CliDbFailure;

// A column of a result: its name, its type's name, and whether its values are numbers, drawn to the right
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    char kind[CLI_DB_KIND];
    int numeric;
} CliDbColumn;

// How a statement began: with rows to read in its columns, or done with the tag the server gave
typedef struct
{
    int rows;
    CliDbColumn columns[CLI_DB_COLUMNS];
    unsigned int column_count;
    char tag[CLI_DB_TAG];
} CliDbHead;

// A value of a row, `size` bytes of its text as the server wrote it, or NULL
typedef struct
{
    const char *text;
    unsigned long long size;
    int null;
} CliDbValue;

typedef struct
{
    void *context;
    const CliDbValue *values;
    unsigned int count;
} CliDbRowCfg;

// a row read, `count` values of it, handed on: 1 to read on, 0 to stop
typedef int (*CliDbRowFn)(const CliDbRowCfg *args);

// a failure of nothing but `message`; and a failure written as the reader reads it: its message, its code after it,
// then its detail and its hint each on a line of its own
void cli_db_failure_set(const CliDbWireCfg *args);
void cli_db_failure_write(const CliDbWireOutCfg *args);

struct CliDbWireCfg
{
    CliDbFailure *failure;
    const char *message;
};

struct CliDbWireOutCfg
{
    const CliDbFailure *failure_read;
    VerbaLinea *out;
};

typedef struct
{
    void (*db_failure_set)(const CliDbWireCfg *args);
} CliDbWireNs;
EMBED_TABLE_LAYOUT(CliDbWireNs, db_failure_set);
EMBED_TABLE_STORAGE CliDbWireNs cli_db_wire EMBED_UNUSED = {
    .db_failure_set = cli_db_failure_set,
};

typedef struct
{
    void (*db_failure_write)(const CliDbWireOutCfg *args);
} CliDbWireOutNs;
EMBED_TABLE_LAYOUT(CliDbWireOutNs, db_failure_write);
EMBED_TABLE_STORAGE CliDbWireOutNs cli_db_wire_out EMBED_UNUSED = {
    .db_failure_write = cli_db_failure_write,
};

#endif
