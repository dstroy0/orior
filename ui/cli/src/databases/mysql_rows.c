// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql_rows.c: a MySQL statement sent by COM_QUERY, how it began read from its first packet, its rows read a packet
// at a time, a binary value written as MySQL writes a binary literal, the results after a procedure's first read past,
// and a statement running stopped by KILL QUERY on a connection of its own
#include "../text/json_tree.h"
#include "../text/text.h"
#include "mysql_internal.h"

typedef struct
{
    unsigned int code;
    int *numeric;
    CliMysql *mysql;
    char *tag;
    unsigned long long count;
    CliDbFailure *failure;
    const char *sql;
    unsigned long long size;
    unsigned long long *used;
    const unsigned char *bytes;
    unsigned long long *length;
} MysqlRowsCtx;

// the type name of MySQL's column types, and whether their values are numbers: "" where impero names it by its code
EMBED_INLINE const char *rows_type_name(const MysqlRowsCtx *args)
{
    int *numeric = args->numeric;
    static const char *const s_names[256] = {
        [0] = "decimal", [246] = "decimal", [1] = "tinyint",   [2] = "smallint", [3] = "int",       [4] = "float",
        [5] = "double",  [6] = "null",      [7] = "timestamp", [8] = "bigint",   [9] = "mediumint", [10] = "date",
        [11] = "time",   [12] = "datetime", [13] = "year",     [15] = "varchar", [253] = "varchar", [16] = "bit",
        [245] = "json",  [247] = "enum",    [248] = "set",     [249] = "blob",   [250] = "blob",    [251] = "blob",
        [252] = "blob",  [254] = "char",    [255] = "geometry"};
    *numeric =
        (args->code <= 5u) || (args->code == 8u) || (args->code == 9u) || (args->code == 13u) || (args->code == 246u);
    return ((args->code < 256u) && (s_names[args->code] != NULL)) ? s_names[args->code] : "";
}

// whether a binary column's type is one of the strings and blobs, 249 to 254 or 15
EMBED_INLINE int rows_stringy(const MysqlRowsCtx *args)
{
    return (args->code == 15u) || ((args->code >= 249u) && (args->code <= 254u));
}

// an OK packet's tag, its counts and the server's status and warnings recorded
EMBED_INLINE void rows_ok(const MysqlRowsCtx *args)
{
    const unsigned char *const payload = args->mysql->packet;
    const unsigned long long size = args->mysql->size;
    unsigned long long at = 1ULL;
    unsigned long long affected = 0ULL;
    unsigned long long inserted = 0ULL;
    if (!EMBED_CALL(cli_mysql_size_bytes_at_internal.lenenc, CliMysqlSizeBytesAtCtx, .bytes = payload, .size = size,
                    .at = &at, .value = &affected))
    {
        affected = 0ULL;
    }
    if (!EMBED_CALL(cli_mysql_size_bytes_at_internal.lenenc, CliMysqlSizeBytesAtCtx, .bytes = payload, .size = size,
                    .at = &at, .value = &inserted))
    {
        inserted = 0ULL;
    }
    args->mysql->status =
        ((at < size) ? payload[at] : 0u) | ((at + 1u < size) ? ((unsigned int)payload[at + 1u] << 8u) : 0u);
    args->mysql->warnings =
        ((at + 2u < size) ? payload[at + 2u] : 0u) | ((at + 3u < size) ? ((unsigned int)payload[at + 3u] << 8u) : 0u);
    VerbaLinea out = {args->tag, CLI_DB_TAG, 0ULL};
    mmgr_line_decimal(&out, affected, 1u);
    mmgr_line_put(&out, (affected == 1ULL) ? " row affected" : " rows affected");
    if (inserted > 0ULL)
    {
        mmgr_line_put(&out, ", last insert id ");
        mmgr_line_decimal(&out, inserted, 1u);
    }
    unsigned long long start = at + 4u;
    unsigned long long end = size;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_POOL_BYTES, .byte = payload[start]}) != NULL))
    {
        start += 1u;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_POOL_BYTES, .byte = payload[end - 1u]}) != NULL))
    {
        end -= 1u;
    }
    if (start < end)
    {
        mmgr_line_put(&out, ": ");
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &out, .text_bytes = payload + start,
                   .length = end - start);
    }
    mmgr_line_finish(&out);
}

// an EOF packet's warnings and status recorded
EMBED_INLINE void rows_eof(const MysqlRowsCtx *args)
{
    const unsigned char *const payload = args->mysql->packet;
    const unsigned long long size = args->mysql->size;
    args->mysql->warnings = ((size > 1u) ? payload[1] : 0u) | ((size > 2u) ? ((unsigned int)payload[2] << 8u) : 0u);
    args->mysql->status = ((size > 3u) ? payload[3] : 0u) | ((size > 4u) ? ((unsigned int)payload[4] << 8u) : 0u);
}

// a row passed over, as finishing reads it
static int rows_pass(const CliDbRowCfg *args)
{
    (void)args;
    return 1;
}

// the columns of a result read, a packet each, into the connection's head
EMBED_INLINE int rows_columns(const MysqlRowsCtx *args)
{
    CliMysql *mysql = args->mysql;
    CliDbFailure *failure = args->failure;
    mysql->head.column_count = 0u;
    for (unsigned long long one = 0ULL; one < args->count; one += 1ULL)
    {
        if (!EMBED_CALL(cli_mysql_failure_internal.packet, CliMysqlFailureCtx, .mysql = mysql, .failure = failure))
        {
            return 0;
        }
        if (mysql->head.column_count == CLI_DB_COLUMNS)
        {
            continue;
        }
        const unsigned char *const definition = mysql->packet;
        const unsigned long long size = mysql->size;
        unsigned long long at = 0ULL;
        unsigned long long start = 0ULL;
        unsigned long long length = 0ULL;
        for (unsigned int skip = 0u; skip < 4u; skip += 1u)
        {
            (void)EMBED_CALL(cli_mysql_size_bytes_internal.lenenc_bytes, CliMysqlSizeBytesCtx, .bytes = definition,
                             .size = size, .at = &at, .start = &start, .length_pointer = &length);
        }
        const int named =
            EMBED_CALL(cli_mysql_size_bytes_internal.lenenc_bytes, CliMysqlSizeBytesCtx, .bytes = definition,
                       .size = size, .at = &at, .start = &start, .length_pointer = &length);
        CliDbColumn *const column = &mysql->head.columns[mysql->head.column_count];
        VerbaLinea name = {column->name, sizeof(column->name), 0ULL};
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &name,
                   .text_bytes = definition + start, .length = named ? length : 0ULL);
        mmgr_line_finish(&name);
        unsigned long long fixed = 0ULL;
        (void)EMBED_CALL(cli_mysql_size_bytes_internal.lenenc_bytes, CliMysqlSizeBytesCtx, .bytes = definition,
                         .size = size, .at = &at, .start = &start, .length_pointer = &length);
        (void)EMBED_CALL(cli_mysql_size_bytes_at_internal.lenenc, CliMysqlSizeBytesAtCtx, .bytes = definition,
                         .size = size, .at = &at, .value = &fixed);
        const unsigned int charset =
            ((at < size) ? definition[at] : 0u) | ((at + 1u < size) ? ((unsigned int)definition[at + 1u] << 8u) : 0u);
        const unsigned int code = (at + 6u < size) ? definition[at + 6u] : 253u;
        const char *type = EMBED_CALL(rows_type_name, MysqlRowsCtx, .code = code, .numeric = &column->numeric);
        if ((charset == 63u) && EMBED_CALL(rows_stringy, MysqlRowsCtx, .code = code))
        {
            type = ((code >= 249u) && (code <= 252u)) ? "blob" : "binary";
        }
        VerbaLinea type_line = {column->type, sizeof(column->type), 0ULL};
        if (type[0] == '\0')
        {
            mmgr_line_put(&type_line, "type ");
            mmgr_line_decimal(&type_line, code, 1u);
        }
        else
        {
            mmgr_line_put(&type_line, type);
        }
        mmgr_line_finish(&type_line);
        mysql->codes[mysql->head.column_count] = (unsigned char)code;
        mysql->charsets[mysql->head.column_count] = charset;
        mysql->head.column_count += 1u;
    }
    if (!EMBED_CALL(cli_mysql_failure_internal.packet, CliMysqlFailureCtx, .mysql = mysql, .failure = failure))
    {
        return 0;
    }
    if ((mysql->size > 0u) && (mysql->packet[0] == 0xfeu))
    {
        EMBED_CALL(rows_eof, MysqlRowsCtx, .mysql = mysql);
    }
    mysql->read_state = 1;
    return 1;
}

// how a statement began, read from its first packet: done with a tag, a failure, or rows in their columns
static int rows_head(const MysqlRowsCtx *args);

// the results a statement gave after its first, as a procedure's CALL gives, read past
static int rows_drain(const MysqlRowsCtx *args)
{
    CliMysql *mysql = args->mysql;
    CliDbFailure *failure = args->failure;
    while ((mysql->status & CLI_MYSQL_MORE_RESULTS) != 0u)
    {
        if (!EMBED_CALL(rows_head, MysqlRowsCtx, .mysql = mysql, .failure = failure) ||
            (mysql->head.rows &&
             !EMBED_CALL(cli_mysql_failure_sql.finish, CliMysqlFailureSqlCfg, .mysql_out = mysql, .failure = failure)))
        {
            return 0;
        }
    }
    return 1;
}

static int rows_head(const MysqlRowsCtx *args)
{
    CliMysql *mysql = args->mysql;
    CliDbFailure *failure = args->failure;
    if (!EMBED_CALL(cli_mysql_failure_internal.packet, CliMysqlFailureCtx, .mysql = mysql, .failure = failure))
    {
        return 0;
    }
    const unsigned char first = (mysql->size > 0u) ? mysql->packet[0] : 0xeeu;
    mysql->head.rows = 0;
    mysql->head.column_count = 0u;
    if (first == 0x00u)
    {
        EMBED_CALL(rows_ok, MysqlRowsCtx, .mysql = mysql, .tag = mysql->head.tag);
        return ((mysql->status & CLI_MYSQL_MORE_RESULTS) == 0u) ||
               EMBED_CALL(rows_drain, MysqlRowsCtx, .mysql = mysql, .failure = failure);
    }
    if (first == 0xffu)
    {
        EMBED_CALL(cli_mysql_failure_internal.failure_get, CliMysqlFailureCtx, .payload = mysql->packet,
                   .size = mysql->size, .failure = failure);
        return 0;
    }
    if (first == 0xfbu)
    {
        (void)EMBED_CALL(cli_mysql_failure_internal.send, CliMysqlFailureCtx, .mysql = mysql, .payload = NULL,
                         .size = 0ULL, .failure = failure);
        (void)EMBED_CALL(cli_mysql_failure_internal.packet, CliMysqlFailureCtx, .mysql = mysql, .failure = failure);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "impero sends no file to LOAD DATA LOCAL INFILE");
        return 0;
    }
    unsigned long long at = 0ULL;
    unsigned long long count = 0ULL;
    if (!EMBED_CALL(cli_mysql_size_bytes_at_internal.lenenc, CliMysqlSizeBytesAtCtx, .bytes = mysql->packet,
                    .size = mysql->size, .at = &at, .value = &count))
    {
        count = 0ULL;
    }
    mysql->head.rows = 1;
    return EMBED_CALL(rows_columns, MysqlRowsCtx, .mysql = mysql, .count = count, .failure = failure);
}

// COM_QUERY and the statement sent as one payload, in packets of at max 16 MiB, each piece written from the
// statement where it lies: 1, or 0 with the failure given
EMBED_INLINE int rows_query_send(const MysqlRowsCtx *args)
{
    unsigned long long size = args->size;
    CliDbFailure *failure = args->failure;
    const unsigned long long total = size + 1ULL;
    unsigned long long last = 0ULL;
    int sent = 1;
    unsigned long long at = 0ULL;
    while (sent && (at < total))
    {
        last = ((total - at) < 0xffffffULL) ? (total - at) : 0xffffffULL;
        const unsigned char head[5] = {(unsigned char)(last & 0xffu), (unsigned char)((last >> 8u) & 0xffu),
                                       (unsigned char)((last >> 16u) & 0xffu), args->mysql->sequence, 0x03u};
        // the first packet carries the command's byte before the statement
        const unsigned long long from = (at == 0ULL) ? 0ULL : at - 1ULL;
        const unsigned long long piece = (at == 0ULL) ? last - 1ULL : last;
        sent = EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->mysql->socket,
                          .bytes = head, .size = (at == 0ULL) ? 5ULL : 4ULL) &&
               EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->mysql->socket,
                          .bytes = args->sql + from, .size = piece);
        args->mysql->sequence = (unsigned char)(args->mysql->sequence + 1u);
        at += last;
    }
    if (sent && (last == 0xffffffULL))
    {
        const unsigned char head[4] = {0u, 0u, 0u, args->mysql->sequence};
        sent = EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->mysql->socket,
                          .bytes = head, .size = sizeof(head));
        args->mysql->sequence = (unsigned char)(args->mysql->sequence + 1u);
    }
    if (!sent)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the connection broke: the server closed it");
    }
    return sent;
}

int cli_mysql_start(const CliMysqlFailureSqlCfg *args)
{
    if (args->mysql_out->read_state && !cli_mysql_failure_sql.finish(args))
    {
        return 0;
    }
    args->mysql_out->sequence = 0u;
    return EMBED_CALL(rows_query_send, MysqlRowsCtx, .mysql = args->mysql_out, .sql = args->sql, .size = args->size,
                      .failure = args->failure) &&
           EMBED_CALL(rows_head, MysqlRowsCtx, .mysql = args->mysql_out, .failure = args->failure);
}

// a value of a binary column written as MySQL writes a binary literal, the first 256 bytes of a long one and its
// length, into the connection's capacity for hex: where it starts, NULL where the capacity is full
EMBED_INLINE const char *rows_hex(const MysqlRowsCtx *args)
{
    unsigned long long *used = args->used;
    unsigned long long *length = args->length;
    static const char s_digits[] = "0123456789ABCDEF";
    VerbaLinea out = {args->mysql->hex + *used, sizeof(args->mysql->hex) - *used, 0ULL};
    const unsigned long long displayed = (args->size > 256u) ? 256ULL : args->size;
    mmgr_line_put(&out, "0x");
    for (unsigned long long at = 0ULL; at < displayed; at += 1ULL)
    {
        mmgr_line_ch(&out, s_digits[args->bytes[at] >> 4u]);
        mmgr_line_ch(&out, s_digits[args->bytes[at] & 15u]);
    }
    if (args->size > 256u)
    {
        mmgr_line_put(&out, "\xE2\x80\xA6 (");
        mmgr_line_decimal(&out, args->size, 1u);
        mmgr_line_put(&out, " bytes)");
    }
    if (!mmgr_line_ok(&out))
    {
        return NULL;
    }
    *length = out.at;
    *used += out.at;
    return out.out;
}

// a row packet's values, each pointing into the packet, a binary value that is not text written in hex, into the
// connection's row: their count
EMBED_INLINE unsigned int rows_row(const MysqlRowsCtx *args)
{
    const unsigned char *const packet = args->mysql->packet;
    const unsigned long long size = args->mysql->size;
    unsigned long long at = 0ULL;
    unsigned long long used = 0ULL;
    for (unsigned int column = 0u; column < args->mysql->head.column_count; column += 1u)
    {
        CliDbValue *const value = &args->mysql->row[column];
        if ((at < size) && (packet[at] == 0xfbu))
        {
            at += 1u;
            *value = (CliDbValue){NULL, 0ULL, 1};
            continue;
        }
        unsigned long long start = 0ULL;
        unsigned long long length = 0ULL;
        if (!EMBED_CALL(cli_mysql_size_bytes_internal.lenenc_bytes, CliMysqlSizeBytesCtx, .bytes = packet, .size = size,
                        .at = &at, .start = &start, .length_pointer = &length))
        {
            start = at;
            length = 0ULL;
        }
        const unsigned int code = args->mysql->codes[column];
        const int binary = (args->mysql->charsets[column] == 63u) &&
                           (EMBED_CALL(rows_stringy, MysqlRowsCtx, .code = code) || (code == 16u) || (code == 255u));
        const int as_hex = binary && (!EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg,
                                                  .bytes = (const char *)packet + start, .size = length) ||
                                      (code == 16u) || (code == 255u));
        unsigned long long hex_length = 0ULL;
        const char *const hex = as_hex ? EMBED_CALL(rows_hex, MysqlRowsCtx, .mysql = args->mysql, .used = &used,
                                                    .bytes = packet + start, .size = length, .length = &hex_length)
                                       : NULL;
        *value =
            (hex != NULL) ? (CliDbValue){hex, hex_length, 0} : (CliDbValue){(const char *)packet + start, length, 0};
    }
    return args->mysql->head.column_count;
}

int cli_mysql_rows(const CliMysqlFailureCfg *args)
{
    int *done = args->done;
    unsigned long long read = 0ULL;
    *done = 0;
    args->tag[0] = '\0';
    if (!args->mysql_out->read_state)
    {
        *done = 1;
        return 1;
    }
    while (read < args->count)
    {
        if (!EMBED_CALL(cli_mysql_failure_internal.packet, CliMysqlFailureCtx, .mysql = args->mysql_out,
                        .failure = args->failure))
        {
            return 0;
        }
        const unsigned char first = (args->mysql_out->size > 0u) ? args->mysql_out->packet[0] : 0u;
        if ((first == 0xfeu) && (args->mysql_out->size < 9u))
        {
            EMBED_CALL(rows_eof, MysqlRowsCtx, .mysql = args->mysql_out);
            args->mysql_out->read_state = 0;
            if (((args->mysql_out->status & CLI_MYSQL_MORE_RESULTS) != 0u) &&
                !EMBED_CALL(rows_drain, MysqlRowsCtx, .mysql = args->mysql_out, .failure = args->failure))
            {
                return 0;
            }
            VerbaLinea out = {args->tag, CLI_DB_TAG, 0ULL};
            mmgr_line_decimal(&out, read, 1u);
            mmgr_line_put(&out, (read == 1ULL) ? " row read" : " rows read");
            mmgr_line_finish(&out);
            *done = 1;
            return 1;
        }
        if (first == 0xffu)
        {
            args->mysql_out->read_state = 0;
            EMBED_CALL(cli_mysql_failure_internal.failure_get, CliMysqlFailureCtx, .payload = args->mysql_out->packet,
                       .size = args->mysql_out->size, .failure = args->failure);
            return 0;
        }
        const unsigned int values = EMBED_CALL(rows_row, MysqlRowsCtx, .mysql = args->mysql_out);
        read += 1ULL;
        if (!EMBED_CALL(args->each_row, CliDbRowCfg, .context = args->context, .values = args->mysql_out->row,
                        .count = values))
        {
            return 1;
        }
    }
    return 1;
}

int cli_mysql_finish(const CliMysqlFailureSqlCfg *args)
{
    char tag[CLI_DB_TAG];
    while (args->mysql_out->read_state)
    {
        int done = 0;
        if (!EMBED_CALL(cli_mysql_failure.rows, CliMysqlFailureCfg, .mysql_out = args->mysql_out, .count = 4096ULL,
                        .each_row = rows_pass, .context = NULL, .done = &done, .tag = tag, .failure = args->failure))
        {
            return 0;
        }
    }
    return 1;
}

int cli_mysql_all(const CliMysqlFailureSqlCfg *args)
{
    char tag[CLI_DB_TAG];
    if (!cli_mysql_failure_sql.start(args))
    {
        return 0;
    }
    for (int done = !args->mysql_out->head.rows; !done;)
    {
        if (!EMBED_CALL(cli_mysql_failure.rows, CliMysqlFailureCfg, .mysql_out = args->mysql_out, .count = 4096ULL,
                        .each_row = args->each_row, .context = args->context, .done = &done, .tag = tag,
                        .failure = args->failure))
        {
            return 0;
        }
    }
    return 1;
}

const CliDbHead *cli_mysql_head(const CliMysqlCancelCfg *args)
{
    return &args->mysql->head;
}

int cli_mysql_in_transaction(const CliMysqlCancelCfg *args)
{
    return (args->mysql->status & CLI_MYSQL_IN_TRANSACTION) != 0u;
}

unsigned int cli_mysql_warnings(const CliMysqlCancelCfg *args)
{
    return args->mysql->warnings;
}

const char *cli_mysql_version(const CliMysqlCancelCfg *args)
{
    return args->mysql->version;
}

int cli_mysql_seal(const CliMysqlCancelCfg *args)
{
    return args->mysql->sealed;
}

void cli_mysql_canceller(const CliMysqlCancelCfg *args)
{
    CliMysqlCancel *cancel = args->cancel;
    *cancel = args->mysql->cancel;
}

int cli_mysql_cancel_send(const CliMysqlCfg *args)
{
    CliDbFailure failure;
    CliMysql *const other =
        EMBED_CALL(cli_mysql_host.connect, CliMysqlHostCfg, .host = args->cancel_read->host,
                   .port = args->cancel_read->port, .user = args->cancel_read->user,
                   .password = args->cancel_read->password, .database = "", .wait = 5000ULL, .failure = &failure);
    if (other == NULL)
    {
        EMBED_CALL(cli_db_wire_out.db_failure_write, CliDbWireOutCfg, .failure_read = &failure, .out = args->err_msg);
        return 0;
    }
    char kill[64];
    VerbaLinea statement = {kill, sizeof(kill), 0ULL};
    mmgr_line_put(&statement, "KILL QUERY ");
    mmgr_line_decimal(&statement, args->cancel_read->id, 1u);
    const int killed =
        EMBED_CALL(cli_mysql_failure_sql.all, CliMysqlFailureSqlCfg, .mysql_out = other, .sql = statement.out,
                   .size = statement.at, .each_row = rows_pass, .context = NULL, .failure = &failure);
    if (!killed)
    {
        EMBED_CALL(cli_db_wire_out.db_failure_write, CliDbWireOutCfg, .failure_read = &failure, .out = args->err_msg);
    }
    EMBED_CALL(cli_mysql_failure_sql.close, CliMysqlFailureSqlCfg, .mysql_out = other);
    return killed;
}

void cli_mysql_literal(const CliMysqlTextCfg *args)
{
    mmgr_line_ch(args->out, '\'');
    EMBED_CALL(cli_text_set_table.append_double, CliTextSetCfg, .out = args->out, .text = args->text, .set = "\\'");
    mmgr_line_ch(args->out, '\'');
}
