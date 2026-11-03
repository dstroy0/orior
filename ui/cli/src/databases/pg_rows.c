// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg_rows.c: a PostgreSQL statement started by the simple query protocol and its rows read a page at a time, the
// rest left to wait in the server; what the server reported and noted read back; and a statement stopped by a cancel
// request on a connection of its own
#include "../text/json_tree.h"
#include "../text/text.h"
#include "pg_internal.h"

typedef struct
{
    const unsigned char *at;
    unsigned int width;
    unsigned long long oid;
    int *numeric;
    CliPg *pg;
    const CliPg *pg_read;
    char *tag;
} PgRowsCtx;

// a big-endian number of `width` bytes at `at`
EMBED_INLINE unsigned long long rows_number(const PgRowsCtx *args)
{
    unsigned long long value = 0ULL;
    for (unsigned int byte = 0u; byte < args->width; byte += 1u)
    {
        value = (value << 8u) | args->at[byte];
    }
    return value;
}

// the type name of the types a result most often contains, by their oid, and whether they are numbers: "" where orior
// names it by its oid
EMBED_INLINE const char *rows_type_name(const PgRowsCtx *args)
{
    unsigned long long oid = args->oid;
    int *numeric = args->numeric;
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
EMBED_INLINE void rows_columns(const PgRowsCtx *args)
{
    const unsigned char *const body = args->pg->message;
    const unsigned long long size = args->pg->size;
    const unsigned long long count = (size >= 2u) ? EMBED_CALL(rows_number, PgRowsCtx, .at = body, .width = 2u) : 0ULL;
    unsigned long long at = 2ULL;
    args->pg->head.column_count = 0u;
    for (unsigned long long one = 0ULL; (one < count) && (args->pg->head.column_count < CLI_DB_COLUMNS); one += 1ULL)
    {
        const unsigned char *const nul =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = body + at, .val = 0, .bytes = (size_t)(size - at)}) : NULL;
        const unsigned long long end = (nul != NULL) ? (unsigned long long)(nul - body) : size;
        CliDbColumn *const column = &args->pg->head.columns[args->pg->head.column_count];
        VerbaLinea name = {column->name, sizeof(column->name), 0ULL};
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &name, .text_bytes = body + at,
                   .length = end - at);
        mmgr_line_finish(&name);
        at = end + 1ULL;
        const unsigned long long oid =
            (at + 10u <= size) ? EMBED_CALL(rows_number, PgRowsCtx, .at = body + at + 6u, .width = 4u) : 0ULL;
        at += 18ULL;
        const char *const kind = EMBED_CALL(rows_type_name, PgRowsCtx, .oid = oid, .numeric = &column->numeric);
        VerbaLinea kind_line = {column->kind, sizeof(column->kind), 0ULL};
        if (kind[0] == '\0')
        {
            mmgr_line_put(&kind_line, "oid ");
            mmgr_line_decimal(&kind_line, oid, 1u);
        }
        else
        {
            mmgr_line_put(&kind_line, kind);
        }
        mmgr_line_finish(&kind_line);
        args->pg->head.column_count += 1u;
    }
}

// a DataRow's values, each pointing into the message, into the connection's row: their count
EMBED_INLINE unsigned int rows_row(const PgRowsCtx *args)
{
    const unsigned char *const body = args->pg->message;
    const unsigned long long size = args->pg->size;
    const unsigned long long count = (size >= 2u) ? EMBED_CALL(rows_number, PgRowsCtx, .at = body, .width = 2u) : 0ULL;
    unsigned long long at = 2ULL;
    unsigned int values = 0u;
    unsigned long long one = 0ULL;
    while ((one < count) && (values < CLI_DB_COLUMNS) && (at + 4u <= size))
    {
        // the length is a signed number of four bytes, -1 for NULL
        const unsigned long long raw = EMBED_CALL(rows_number, PgRowsCtx, .at = body + at, .width = 4u);
        at += 4ULL;
        CliDbValue *const value = &args->pg->row[values];
        value->null = (raw & 0x80000000ULL) != 0ULL;
        const unsigned long long length = value->null ? 0ULL : raw;
        const unsigned long long end = (at + length < size) ? at + length : size;
        value->text = (const char *)body + at;
        value->size = end - at;
        at = end;
        values += 1u;
        one += 1ULL;
    }
    return values;
}

// a CommandComplete's tag into `tag`
EMBED_INLINE void rows_tag(const PgRowsCtx *args)
{
    const unsigned long long size =
        ((args->pg_read->size > 0u) && (args->pg_read->message[args->pg_read->size - 1u] == 0u))
            ? args->pg_read->size - 1u
            : args->pg_read->size;
    VerbaLinea line = {args->tag, CLI_DB_TAG, 0ULL};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line,
               .text_bytes = args->pg_read->message, .length = size);
    mmgr_line_finish(&line);
}

int cli_pg_start(const CliPgFailureSqlCfg *args)
{
    CliDbFailure *failure = args->failure;
    char tag[CLI_DB_TAG];
    if (args->pg->read_state &&
        !EMBED_CALL(cli_pg_failure.finish, CliPgFailureCfg, .pg = args->pg, .tag = tag, .failure = failure))
    {
        return 0;
    }
    args->pg->notice_count = 0u;
    args->pg->has_failed = 0;
    const void *const parts[2] = {args->sql, ""};
    const unsigned long long sizes[2] = {args->size, 1ULL};
    if (!EMBED_CALL(cli_pg_failure_kind_internal.send, CliPgFailureKindCtx, .pg = args->pg, .kind_value = 'Q',
                    .parts = parts, .sizes = sizes, .count = 2u, .failure = failure))
    {
        return 0;
    }
    args->pg->head.rows = 0;
    args->pg->head.tag[0] = '\0';
    for (;;)
    {
        unsigned char kind = 0u;
        if (!EMBED_CALL(cli_pg_kind_internal.message, CliPgKindCtx, .pg = args->pg, .kind = &kind, .failure = failure))
        {
            return 0;
        }
        if (kind == 'T')
        {
            args->pg->read_state = 1;
            args->pg->head.rows = 1;
            EMBED_CALL(rows_columns, PgRowsCtx, .pg = args->pg);
            return 1;
        }
        if (kind == 'C')
        {
            EMBED_CALL(rows_tag, PgRowsCtx, .pg_read = args->pg, .tag = args->pg->head.tag);
        }
        else if (kind == 'E')
        {
            EMBED_CALL(cli_pg_internal.failure_get, CliPgCtx, .body = args->pg->message, .size = args->pg->size,
                       .failure = &args->pg->failed);
            args->pg->has_failed = 1;
        }
        else if (kind == 'N')
        {
            EMBED_CALL(cli_pg_failure_password_internal.notice_store, CliPgFailurePasswordCtx, .pg = args->pg);
        }
        else if (kind == 'S')
        {
            EMBED_CALL(cli_pg_body_internal.parameter_store, CliPgBodyCtx, .pg = args->pg, .body = args->pg->message,
                       .size = args->pg->size);
        }
        else if (kind == 'G')
        {
            static const char s_reason[] = "orior_cli sends no data to COPY FROM STDIN";
            const void *const reason[1] = {s_reason};
            const unsigned long long reason_size[1] = {sizeof(s_reason)};
            if (!EMBED_CALL(cli_pg_failure_kind_internal.send, CliPgFailureKindCtx, .pg = args->pg, .kind_value = 'f',
                            .parts = reason, .sizes = reason_size, .count = 1u, .failure = failure))
            {
                return 0;
            }
        }
        else if (kind == 'H')
        {
            args->pg->read_state = 1;
            args->pg->head.rows = 1;
            args->pg->head.column_count = 1u;
            memor.cpy(
                &(MemoriaCfg){.dst = args->pg->head.columns[0].name,
                              .src = "copy",
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = "copy", .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(
                &(MemoriaCfg){.dst = args->pg->head.columns[0].kind,
                              .src = "text",
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = "text", .cap = CLI_CELLS_POOL}) + 1u});
            args->pg->head.columns[0].numeric = 0;
            return 1;
        }
        else if (kind == 'Z')
        {
            args->pg->status = (args->pg->size > 0u) ? (char)args->pg->message[0] : 'I';
            if (args->pg->has_failed)
            {
                args->pg->has_failed = 0;
                *failure = args->pg->failed;
                return 0;
            }
            return 1;
        }
    }
}

int cli_pg_rows(const CliPgFailureCfg *args)
{
    int *done = args->done;
    CliDbFailure *failure = args->failure;
    unsigned long long read = 0ULL;
    int has_tag = 0;
    *done = 0;
    args->tag[0] = '\0';
    if (!args->pg->read_state)
    {
        *done = 1;
        return 1;
    }
    // once the tag has come the rows are read on to the ReadyForQuery that follows it
    while ((read < args->count) || has_tag)
    {
        unsigned char kind = 0u;
        if (!EMBED_CALL(cli_pg_kind_internal.message, CliPgKindCtx, .pg = args->pg, .kind = &kind, .failure = failure))
        {
            return 0;
        }
        if (kind == 'D')
        {
            const unsigned int values = EMBED_CALL(rows_row, PgRowsCtx, .pg = args->pg);
            read += 1ULL;
            if (!EMBED_CALL(args->each_row, CliDbRowCfg, .context = args->context, .values = args->pg->row,
                            .count = values))
            {
                return 1;
            }
        }
        else if (kind == 'd')
        {
            unsigned long long size = args->pg->size;
            size = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->pg->message,
                              .at = size, .set = "\n");
            args->pg->row[0] = (CliDbValue){(const char *)args->pg->message, size, 0};
            read += 1ULL;
            if (!EMBED_CALL(args->each_row, CliDbRowCfg, .context = args->context, .values = args->pg->row,
                            .count = 1u))
            {
                return 1;
            }
        }
        else if (kind == 'C')
        {
            EMBED_CALL(rows_tag, PgRowsCtx, .pg_read = args->pg, .tag = args->tag);
            has_tag = 1;
        }
        else if (kind == 'E')
        {
            EMBED_CALL(cli_pg_internal.failure_get, CliPgCtx, .body = args->pg->message, .size = args->pg->size,
                       .failure = &args->pg->failed);
            args->pg->has_failed = 1;
        }
        else if (kind == 'N')
        {
            EMBED_CALL(cli_pg_failure_password_internal.notice_store, CliPgFailurePasswordCtx, .pg = args->pg);
        }
        else if (kind == 'S')
        {
            EMBED_CALL(cli_pg_body_internal.parameter_store, CliPgBodyCtx, .pg = args->pg, .body = args->pg->message,
                       .size = args->pg->size);
        }
        else if (kind == 'Z')
        {
            args->pg->status = (args->pg->size > 0u) ? (char)args->pg->message[0] : 'I';
            args->pg->read_state = 0;
            if (args->pg->has_failed)
            {
                args->pg->has_failed = 0;
                *failure = args->pg->failed;
                return 0;
            }
            *done = 1;
            return 1;
        }
    }
    return 1;
}

// a row passed over, as finishing reads it
static int rows_pass(const CliDbRowCfg *args)
{
    (void)args;
    return 1;
}

int cli_pg_finish(const CliPgFailureCfg *args)
{
    char done_tag[CLI_DB_TAG];
    args->tag[0] = '\0';
    while (args->pg->read_state)
    {
        int done = 0;
        if (!EMBED_CALL(cli_pg_failure.rows, CliPgFailureCfg, .pg = args->pg, .count = 4096ULL, .each_row = rows_pass,
                        .context = NULL, .done = &done, .tag = done_tag, .failure = args->failure))
        {
            return 0;
        }
        if (done)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->tag,
                              .src = done_tag,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = done_tag, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    return 1;
}

int cli_pg_all(const CliPgFailureSqlCfg *args)
{
    char tag[CLI_DB_TAG];
    if (!cli_pg_failure_sql.start(args))
    {
        return 0;
    }
    for (int done = !args->pg->head.rows; !done;)
    {
        if (!EMBED_CALL(cli_pg_failure.rows, CliPgFailureCfg, .pg = args->pg, .count = 4096ULL,
                        .each_row = args->each_row, .context = args->context, .done = &done, .tag = tag,
                        .failure = args->failure))
        {
            return 0;
        }
    }
    return 1;
}
