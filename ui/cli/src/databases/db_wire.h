// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_wire.h: what the database connections share, PostgreSQL's and MySQL's alike: what a server said went wrong, a
// result's columns, how a statement began, and each row's values as the server wrote them (db_wire.c)
#ifndef DB_WIRE_H
#define DB_WIRE_H

#include "../command_line/cli_internal.h"

// the bytes of each field of a failure; the most columns a result holds, as PostgreSQL's 1664 targets; the bytes of a
// column's type name; and of the tag a statement ended with
#define CLI_DB_FIELD 2048u
#define CLI_DB_COLUMNS 1664u
#define CLI_DB_KIND 64u
#define CLI_DB_TAG 512u

// What a server said went wrong, in its own fields: the place in the statement it names, counted in characters from 1,
// or 0
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
    char name[CLI_NAME_LONGEST];
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

// a row read, `count` values of it, handed on: 1 to read on, 0 to stop
typedef int (*CliDbRowTold)(void *context, const CliDbValue *values, unsigned int count);

// a failure of nothing but `message`; and a failure written as the reader reads it: its message, its code after it,
// then its detail and its hint each on a line of its own
void cli_db_failure_said(CliDbFailure *failure, const char *message);
void cli_db_failure_written(const CliDbFailure *failure, CliLine *out);

#endif
