// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg_rows.c: a PostgreSQL statement started by the simple query protocol and its rows read a page at a time, the
// rest left to wait in the server; what the server said and noted read back; and a statement stopped by a cancel
// request on a connection of its own
#include "../text/json_tree.h"
#include "pg_internal.h"

// a big-endian number of `width` bytes at `at`
static unsigned long long rows_number(const unsigned char *at, unsigned int width)
{
    unsigned long long value = 0ull;
    for (unsigned int byte = 0u; byte < width; byte += 1u)
    {
        value = (value << 8u) | at[byte];
    }
    return value;
}

// the type name of the types a result most often holds, by their oid, and whether they are numbers: "" where orior
// names it by its oid
static const char *rows_type_name(unsigned long long oid, int *numeric)
{
    static const struct
    {
        unsigned int oid;
        const char *name;
        int numeric;
    } s_types[] = {{16u, "bool", 0},       {17u, "bytea", 0},    {18u, "char", 0},        {19u, "name", 0},
                   {20u, "int8", 1},       {21u, "int2", 1},     {23u, "int4", 1},        {25u, "text", 0},
                   {26u, "oid", 1},        {114u, "json", 0},    {142u, "xml", 0},        {700u, "float4", 1},
                   {701u, "float8", 1},    {790u, "money", 1},   {1042u, "bpchar", 0},    {1043u, "varchar", 0},
                   {1082u, "date", 0},     {1083u, "time", 0},   {1114u, "timestamp", 0}, {1184u, "timestamptz", 0},
                   {1186u, "interval", 0}, {1266u, "timetz", 0}, {1700u, "numeric", 1},   {2950u, "uuid", 0},
                   {3802u, "jsonb", 0}};
    for (unsigned int at = 0u; at < sizeof(s_types) / sizeof(s_types[0]); at += 1u)
    {
        if (s_types[at].oid == oid)
        {
            *numeric = s_types[at].numeric;
            return s_types[at].name;
        }
    }
    *numeric = 0;
    return "";
}

// the columns a RowDescription names, into the connection's head
static void rows_columns(CliPg *pg)
{
    const unsigned char *const body = pg->message;
    const unsigned long long size = pg->size;
    const unsigned long long count = (size >= 2u) ? rows_number(body, 2u) : 0ull;
    unsigned long long at = 2ull;
    pg->head.column_count = 0u;
    for (unsigned long long one = 0ull; (one < count) && (pg->head.column_count < CLI_DB_COLUMNS); one += 1ull)
    {
        const unsigned char *const nul =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = body + at, .val = 0, .bytes = (size_t)(size - at)}) : NULL;
        const unsigned long long end = (nul != NULL) ? (unsigned long long)(nul - body) : size;
        CliDbColumn *const column = &pg->head.columns[pg->head.column_count];
        CliLine name = {column->name, sizeof(column->name), 0ull};
        cli_lossy(&name, body + at, end - at);
        cli_line_closed(&name);
        at = end + 1ull;
        const unsigned long long oid = (at + 10u <= size) ? rows_number(body + at + 6u, 4u) : 0ull;
        at += 18ull;
        const char *const kind = rows_type_name(oid, &column->numeric);
        CliLine kind_line = {column->kind, sizeof(column->kind), 0ull};
        if (kind[0] == '\0')
        {
            cli_line_text(&kind_line, "oid ");
            cli_line_decimal(&kind_line, oid, 1u);
        }
        else
        {
            cli_line_text(&kind_line, kind);
        }
        cli_line_closed(&kind_line);
        pg->head.column_count += 1u;
    }
}

// a DataRow's values, each pointing into the message, into the connection's row: their count
static unsigned int rows_row(CliPg *pg)
{
    const unsigned char *const body = pg->message;
    const unsigned long long size = pg->size;
    const unsigned long long count = (size >= 2u) ? rows_number(body, 2u) : 0ull;
    unsigned long long at = 2ull;
    unsigned int values = 0u;
    for (unsigned long long one = 0ull; (one < count) && (values < CLI_DB_COLUMNS) && (at + 4u <= size); one += 1ull)
    {
        // the length is a signed number of four bytes, -1 for NULL
        const unsigned long long raw = rows_number(body + at, 4u);
        at += 4ull;
        CliDbValue *const value = &pg->row[values];
        value->null = (raw & 0x80000000ull) != 0ull;
        const unsigned long long length = value->null ? 0ull : raw;
        const unsigned long long end = (at + length < size) ? at + length : size;
        value->text = (const char *)body + at;
        value->size = end - at;
        at = end;
        values += 1u;
    }
    return values;
}

// a CommandComplete's tag into `tag`
static void rows_tag(const CliPg *pg, char *tag)
{
    const unsigned long long size = ((pg->size > 0u) && (pg->message[pg->size - 1u] == 0u)) ? pg->size - 1u : pg->size;
    CliLine line = {tag, CLI_DB_TAG, 0ull};
    cli_lossy(&line, pg->message, size);
    cli_line_closed(&line);
}

int cli_pg_start(CliPg *pg, const char *sql, unsigned long long size, CliDbFailure *failure)
{
    char tag[CLI_DB_TAG];
    if (pg->reading && !cli_pg_finish(pg, tag, failure))
    {
        return 0;
    }
    pg->notice_count = 0u;
    pg->has_failed = 0;
    const void *const parts[2] = {sql, ""};
    const unsigned long long sizes[2] = {size, 1ull};
    if (!cli_pg_sent(pg, 'Q', parts, sizes, 2u, failure))
    {
        return 0;
    }
    pg->head.rows = 0;
    pg->head.tag[0] = '\0';
    for (;;)
    {
        unsigned char kind = 0u;
        if (!cli_pg_message(pg, &kind, failure))
        {
            return 0;
        }
        if (kind == 'T')
        {
            pg->reading = 1;
            pg->head.rows = 1;
            rows_columns(pg);
            return 1;
        }
        if (kind == 'C')
        {
            rows_tag(pg, pg->head.tag);
        }
        else if (kind == 'E')
        {
            cli_pg_failure_of(pg->message, pg->size, &pg->failed);
            pg->has_failed = 1;
        }
        else if (kind == 'N')
        {
            cli_pg_notice_kept(pg);
        }
        else if (kind == 'S')
        {
            cli_pg_parameter_kept(pg, pg->message, pg->size);
        }
        else if (kind == 'G')
        {
            static const char s_reason[] = "orior_cli sends no data to COPY FROM STDIN";
            const void *const reason[1] = {s_reason};
            const unsigned long long reason_size[1] = {sizeof(s_reason)};
            if (!cli_pg_sent(pg, 'f', reason, reason_size, 1u, failure))
            {
                return 0;
            }
        }
        else if (kind == 'H')
        {
            pg->reading = 1;
            pg->head.rows = 1;
            pg->head.column_count = 1u;
            memor.cpy(
                &(MemoriaCfg){.dst = pg->head.columns[0].name,
                              .src = "copy",
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = "copy", .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(
                &(MemoriaCfg){.dst = pg->head.columns[0].kind,
                              .src = "text",
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = "text", .cap = CLI_CELLS_POOL}) + 1u});
            pg->head.columns[0].numeric = 0;
            return 1;
        }
        else if (kind == 'Z')
        {
            pg->status = (pg->size > 0u) ? (char)pg->message[0] : 'I';
            if (pg->has_failed)
            {
                pg->has_failed = 0;
                *failure = pg->failed;
                return 0;
            }
            return 1;
        }
    }
}

int cli_pg_rows(CliPg *pg, unsigned long long count, CliDbRowTold told, void *context, int *done, char *tag,
                CliDbFailure *failure)
{
    unsigned long long read = 0ull;
    int has_tag = 0;
    *done = 0;
    tag[0] = '\0';
    if (!pg->reading)
    {
        *done = 1;
        return 1;
    }
    // once the tag has come the rows are read on to the ReadyForQuery that follows it
    while ((read < count) || has_tag)
    {
        unsigned char kind = 0u;
        if (!cli_pg_message(pg, &kind, failure))
        {
            return 0;
        }
        if (kind == 'D')
        {
            const unsigned int values = rows_row(pg);
            read += 1ull;
            if (!told(context, pg->row, values))
            {
                return 1;
            }
        }
        else if (kind == 'd')
        {
            unsigned long long size = pg->size;
            while ((size > 0u) && (pg->message[size - 1u] == '\n'))
            {
                size -= 1u;
            }
            pg->row[0] = (CliDbValue){(const char *)pg->message, size, 0};
            read += 1ull;
            if (!told(context, pg->row, 1u))
            {
                return 1;
            }
        }
        else if (kind == 'C')
        {
            rows_tag(pg, tag);
            has_tag = 1;
        }
        else if (kind == 'E')
        {
            cli_pg_failure_of(pg->message, pg->size, &pg->failed);
            pg->has_failed = 1;
        }
        else if (kind == 'N')
        {
            cli_pg_notice_kept(pg);
        }
        else if (kind == 'S')
        {
            cli_pg_parameter_kept(pg, pg->message, pg->size);
        }
        else if (kind == 'Z')
        {
            pg->status = (pg->size > 0u) ? (char)pg->message[0] : 'I';
            pg->reading = 0;
            if (pg->has_failed)
            {
                pg->has_failed = 0;
                *failure = pg->failed;
                return 0;
            }
            *done = 1;
            return 1;
        }
    }
    return 1;
}

// a row passed over, as finishing reads it
static int rows_passed(void *context, const CliDbValue *values, unsigned int count)
{
    (void)context;
    (void)values;
    (void)count;
    return 1;
}

int cli_pg_finish(CliPg *pg, char *tag, CliDbFailure *failure)
{
    char done_tag[CLI_DB_TAG];
    tag[0] = '\0';
    while (pg->reading)
    {
        int done = 0;
        if (!cli_pg_rows(pg, 4096ull, rows_passed, NULL, &done, done_tag, failure))
        {
            return 0;
        }
        if (done)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = tag,
                              .src = done_tag,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = done_tag, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    return 1;
}

int cli_pg_all(CliPg *pg, const char *sql, unsigned long long size, CliDbRowTold told, void *context,
               CliDbFailure *failure)
{
    char tag[CLI_DB_TAG];
    if (!cli_pg_start(pg, sql, size, failure))
    {
        return 0;
    }
    for (int done = !pg->head.rows; !done;)
    {
        if (!cli_pg_rows(pg, 4096ull, told, context, &done, tag, failure))
        {
            return 0;
        }
    }
    return 1;
}
