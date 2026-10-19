// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql_rows.c: a MySQL statement sent by COM_QUERY, how it began read from its first packet, its rows read a packet
// at a time, a binary value written as MySQL writes a binary literal, the results after a procedure's first read past,
// and a statement running stopped by KILL QUERY on a connection of its own
#include "../text/json_tree.h"
#include "mysql_internal.h"

// the type name of MySQL's column types, and whether their values are numbers: "" where orior names it by its code
static const char *rows_type_name(unsigned int code, int *numeric)
{
    static const char *const s_names[256] = {
        [0] = "decimal", [246] = "decimal", [1] = "tinyint",   [2] = "smallint", [3] = "int",       [4] = "float",
        [5] = "double",  [6] = "null",      [7] = "timestamp", [8] = "bigint",   [9] = "mediumint", [10] = "date",
        [11] = "time",   [12] = "datetime", [13] = "year",     [15] = "varchar", [253] = "varchar", [16] = "bit",
        [245] = "json",  [247] = "enum",    [248] = "set",     [249] = "blob",   [250] = "blob",    [251] = "blob",
        [252] = "blob",  [254] = "char",    [255] = "geometry"};
    *numeric = (code <= 5u) || (code == 8u) || (code == 9u) || (code == 13u) || (code == 246u);
    return ((code < 256u) && (s_names[code] != NULL)) ? s_names[code] : "";
}

// whether a binary column's type is one of the strings and blobs, 249 to 254 or 15
static int rows_stringy(unsigned int code)
{
    return (code == 15u) || ((code >= 249u) && (code <= 254u));
}

// an OK packet's tag, its counts and the server's status and warnings kept
static void rows_ok(CliMysql *mysql, char *tag)
{
    const unsigned char *const payload = mysql->packet;
    const unsigned long long size = mysql->size;
    unsigned long long at = 1ull;
    unsigned long long affected = 0ull;
    unsigned long long inserted = 0ull;
    if (!cli_mysql_lenenc(payload, size, &at, &affected))
    {
        affected = 0ull;
    }
    if (!cli_mysql_lenenc(payload, size, &at, &inserted))
    {
        inserted = 0ull;
    }
    mysql->status = ((at < size) ? payload[at] : 0u) | ((at + 1u < size) ? ((unsigned int)payload[at + 1u] << 8u) : 0u);
    mysql->warnings =
        ((at + 2u < size) ? payload[at + 2u] : 0u) | ((at + 3u < size) ? ((unsigned int)payload[at + 3u] << 8u) : 0u);
    CliLine out = {tag, CLI_DB_TAG, 0ull};
    cli_line_decimal(&out, affected, 1u);
    cli_line_text(&out, (affected == 1ull) ? " row affected" : " rows affected");
    if (inserted > 0ull)
    {
        cli_line_text(&out, ", last insert id ");
        cli_line_decimal(&out, inserted, 1u);
    }
    unsigned long long start = at + 4u;
    unsigned long long end = size;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = payload[start]}) != NULL))
    {
        start += 1u;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = payload[end - 1u]}) != NULL))
    {
        end -= 1u;
    }
    if (start < end)
    {
        cli_line_text(&out, ": ");
        cli_lossy(&out, payload + start, end - start);
    }
    cli_line_closed(&out);
}

// an EOF packet's warnings and status kept
static void rows_eof(CliMysql *mysql)
{
    const unsigned char *const payload = mysql->packet;
    const unsigned long long size = mysql->size;
    mysql->warnings = ((size > 1u) ? payload[1] : 0u) | ((size > 2u) ? ((unsigned int)payload[2] << 8u) : 0u);
    mysql->status = ((size > 3u) ? payload[3] : 0u) | ((size > 4u) ? ((unsigned int)payload[4] << 8u) : 0u);
}

// a row passed over, as finishing reads it
static int rows_passed(void *context, const CliDbValue *values, unsigned int count)
{
    (void)context;
    (void)values;
    (void)count;
    return 1;
}

// the columns of a result read, a packet each, into the connection's head
static int rows_columns(CliMysql *mysql, unsigned long long count, CliDbFailure *failure)
{
    mysql->head.column_count = 0u;
    for (unsigned long long one = 0ull; one < count; one += 1ull)
    {
        if (!cli_mysql_packet(mysql, failure))
        {
            return 0;
        }
        if (mysql->head.column_count == CLI_DB_COLUMNS)
        {
            continue;
        }
        const unsigned char *const definition = mysql->packet;
        const unsigned long long size = mysql->size;
        unsigned long long at = 0ull;
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        for (unsigned int skip = 0u; skip < 4u; skip += 1u)
        {
            (void)cli_mysql_lenenc_bytes(definition, size, &at, &start, &length);
        }
        const int named = cli_mysql_lenenc_bytes(definition, size, &at, &start, &length);
        CliDbColumn *const column = &mysql->head.columns[mysql->head.column_count];
        CliLine name = {column->name, sizeof(column->name), 0ull};
        cli_lossy(&name, definition + start, named ? length : 0ull);
        cli_line_closed(&name);
        unsigned long long fixed = 0ull;
        (void)cli_mysql_lenenc_bytes(definition, size, &at, &start, &length);
        (void)cli_mysql_lenenc(definition, size, &at, &fixed);
        const unsigned int charset =
            ((at < size) ? definition[at] : 0u) | ((at + 1u < size) ? ((unsigned int)definition[at + 1u] << 8u) : 0u);
        const unsigned int code = (at + 6u < size) ? definition[at + 6u] : 253u;
        const char *kind = rows_type_name(code, &column->numeric);
        if ((charset == 63u) && rows_stringy(code))
        {
            kind = ((code >= 249u) && (code <= 252u)) ? "blob" : "binary";
        }
        CliLine kind_line = {column->kind, sizeof(column->kind), 0ull};
        if (kind[0] == '\0')
        {
            cli_line_text(&kind_line, "type ");
            cli_line_decimal(&kind_line, code, 1u);
        }
        else
        {
            cli_line_text(&kind_line, kind);
        }
        cli_line_closed(&kind_line);
        mysql->codes[mysql->head.column_count] = (unsigned char)code;
        mysql->charsets[mysql->head.column_count] = charset;
        mysql->head.column_count += 1u;
    }
    if (!cli_mysql_packet(mysql, failure))
    {
        return 0;
    }
    if ((mysql->size > 0u) && (mysql->packet[0] == 0xfeu))
    {
        rows_eof(mysql);
    }
    mysql->reading = 1;
    return 1;
}

// how a statement began, read from its first packet: done with a tag, a failure, or rows in their columns
static int rows_head(CliMysql *mysql, CliDbFailure *failure);

// the results a statement gave after its first, as a procedure's CALL gives, read past
static int rows_drained(CliMysql *mysql, CliDbFailure *failure)
{
    while ((mysql->status & CLI_MYSQL_MORE_RESULTS) != 0u)
    {
        if (!rows_head(mysql, failure) || (mysql->head.rows && !cli_mysql_finish(mysql, failure)))
        {
            return 0;
        }
    }
    return 1;
}

static int rows_head(CliMysql *mysql, CliDbFailure *failure)
{
    if (!cli_mysql_packet(mysql, failure))
    {
        return 0;
    }
    const unsigned char first = (mysql->size > 0u) ? mysql->packet[0] : 0xeeu;
    mysql->head.rows = 0;
    mysql->head.column_count = 0u;
    if (first == 0x00u)
    {
        rows_ok(mysql, mysql->head.tag);
        return ((mysql->status & CLI_MYSQL_MORE_RESULTS) == 0u) || rows_drained(mysql, failure);
    }
    if (first == 0xffu)
    {
        cli_mysql_failure_of(mysql->packet, mysql->size, failure);
        return 0;
    }
    if (first == 0xfbu)
    {
        (void)cli_mysql_sent(mysql, NULL, 0ull, failure);
        (void)cli_mysql_packet(mysql, failure);
        cli_db_failure_said(failure, "orior sends no file to LOAD DATA LOCAL INFILE");
        return 0;
    }
    unsigned long long at = 0ull;
    unsigned long long count = 0ull;
    if (!cli_mysql_lenenc(mysql->packet, mysql->size, &at, &count))
    {
        count = 0ull;
    }
    mysql->head.rows = 1;
    return rows_columns(mysql, count, failure);
}

// COM_QUERY and the statement sent as one payload, in packets of at most 16 MiB, each piece written from the
// statement where it lies: 1, or 0 with the failure said
static int rows_query_sent(CliMysql *mysql, const char *sql, unsigned long long size, CliDbFailure *failure)
{
    const unsigned long long total = size + 1ull;
    unsigned long long last = 0ull;
    int sent = 1;
    for (unsigned long long at = 0ull; sent && (at < total); at += last)
    {
        last = ((total - at) < 0xffffffull) ? (total - at) : 0xffffffull;
        const unsigned char head[5] = {(unsigned char)(last & 0xffu), (unsigned char)((last >> 8u) & 0xffu),
                                       (unsigned char)((last >> 16u) & 0xffu), mysql->sequence, 0x03u};
        // the first packet carries the command's byte before the statement
        const unsigned long long from = (at == 0ull) ? 0ull : at - 1ull;
        const unsigned long long piece = (at == 0ull) ? last - 1ull : last;
        sent = cli_socket_written(&mysql->socket, head, (at == 0ull) ? 5ull : 4ull) &&
               cli_socket_written(&mysql->socket, sql + from, piece);
        mysql->sequence = (unsigned char)(mysql->sequence + 1u);
    }
    if (sent && (last == 0xffffffull))
    {
        const unsigned char head[4] = {0u, 0u, 0u, mysql->sequence};
        sent = cli_socket_written(&mysql->socket, head, sizeof(head));
        mysql->sequence = (unsigned char)(mysql->sequence + 1u);
    }
    if (!sent)
    {
        cli_db_failure_said(failure, "the connection broke: the server closed it");
    }
    return sent;
}

int cli_mysql_start(CliMysql *mysql, const char *sql, unsigned long long size, CliDbFailure *failure)
{
    if (mysql->reading && !cli_mysql_finish(mysql, failure))
    {
        return 0;
    }
    mysql->sequence = 0u;
    return rows_query_sent(mysql, sql, size, failure) && rows_head(mysql, failure);
}

// a value of a binary column written as MySQL writes a binary literal, the first 256 bytes of a long one and its
// length, into the connection's room for hex: where it starts, NULL where the room is full
static const char *rows_hex(CliMysql *mysql, unsigned long long *used, const unsigned char *bytes,
                            unsigned long long size, unsigned long long *length)
{
    static const char s_digits[] = "0123456789ABCDEF";
    CliLine out = {mysql->hex + *used, sizeof(mysql->hex) - *used, 0ull};
    const unsigned long long shown = (size > 256u) ? 256ull : size;
    cli_line_text(&out, "0x");
    for (unsigned long long at = 0ull; at < shown; at += 1ull)
    {
        cli_line_character(&out, s_digits[bytes[at] >> 4u]);
        cli_line_character(&out, s_digits[bytes[at] & 15u]);
    }
    if (size > 256u)
    {
        cli_line_text(&out, "\xE2\x80\xA6 (");
        cli_line_decimal(&out, size, 1u);
        cli_line_text(&out, " bytes)");
    }
    if (!cli_line_fits(&out))
    {
        return NULL;
    }
    *length = out.at;
    *used += out.at;
    return out.out;
}

// a row packet's values, each pointing into the packet, a binary value that is not text written in hex, into the
// connection's row: their count
static unsigned int rows_row(CliMysql *mysql)
{
    const unsigned char *const said = mysql->packet;
    const unsigned long long size = mysql->size;
    unsigned long long at = 0ull;
    unsigned long long used = 0ull;
    for (unsigned int column = 0u; column < mysql->head.column_count; column += 1u)
    {
        CliDbValue *const value = &mysql->row[column];
        if ((at < size) && (said[at] == 0xfbu))
        {
            at += 1u;
            *value = (CliDbValue){NULL, 0ull, 1};
            continue;
        }
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        if (!cli_mysql_lenenc_bytes(said, size, &at, &start, &length))
        {
            start = at;
            length = 0ull;
        }
        const unsigned int code = mysql->codes[column];
        const int binary = (mysql->charsets[column] == 63u) && (rows_stringy(code) || (code == 16u) || (code == 255u));
        const int as_hex =
            binary && (!cli_utf8_whole((const char *)said + start, length) || (code == 16u) || (code == 255u));
        unsigned long long hex_length = 0ull;
        const char *const hex = as_hex ? rows_hex(mysql, &used, said + start, length, &hex_length) : NULL;
        *value = (hex != NULL) ? (CliDbValue){hex, hex_length, 0} : (CliDbValue){(const char *)said + start, length, 0};
    }
    return mysql->head.column_count;
}

int cli_mysql_rows(CliMysql *mysql, unsigned long long count, CliDbRowTold told, void *context, int *done, char *tag,
                   CliDbFailure *failure)
{
    unsigned long long read = 0ull;
    *done = 0;
    tag[0] = '\0';
    if (!mysql->reading)
    {
        *done = 1;
        return 1;
    }
    while (read < count)
    {
        if (!cli_mysql_packet(mysql, failure))
        {
            return 0;
        }
        const unsigned char first = (mysql->size > 0u) ? mysql->packet[0] : 0u;
        if ((first == 0xfeu) && (mysql->size < 9u))
        {
            rows_eof(mysql);
            mysql->reading = 0;
            if (((mysql->status & CLI_MYSQL_MORE_RESULTS) != 0u) && !rows_drained(mysql, failure))
            {
                return 0;
            }
            CliLine out = {tag, CLI_DB_TAG, 0ull};
            cli_line_decimal(&out, read, 1u);
            cli_line_text(&out, (read == 1ull) ? " row read" : " rows read");
            cli_line_closed(&out);
            *done = 1;
            return 1;
        }
        if (first == 0xffu)
        {
            mysql->reading = 0;
            cli_mysql_failure_of(mysql->packet, mysql->size, failure);
            return 0;
        }
        const unsigned int values = rows_row(mysql);
        read += 1ull;
        if (!told(context, mysql->row, values))
        {
            return 1;
        }
    }
    return 1;
}

int cli_mysql_finish(CliMysql *mysql, CliDbFailure *failure)
{
    char tag[CLI_DB_TAG];
    while (mysql->reading)
    {
        int done = 0;
        if (!cli_mysql_rows(mysql, 4096ull, rows_passed, NULL, &done, tag, failure))
        {
            return 0;
        }
    }
    return 1;
}

int cli_mysql_all(CliMysql *mysql, const char *sql, unsigned long long size, CliDbRowTold told, void *context,
                  CliDbFailure *failure)
{
    char tag[CLI_DB_TAG];
    if (!cli_mysql_start(mysql, sql, size, failure))
    {
        return 0;
    }
    for (int done = !mysql->head.rows; !done;)
    {
        if (!cli_mysql_rows(mysql, 4096ull, told, context, &done, tag, failure))
        {
            return 0;
        }
    }
    return 1;
}

const CliDbHead *cli_mysql_head(const CliMysql *mysql)
{
    return &mysql->head;
}

int cli_mysql_in_transaction(const CliMysql *mysql)
{
    return (mysql->status & CLI_MYSQL_IN_TRANSACTION) != 0u;
}

unsigned int cli_mysql_warnings(const CliMysql *mysql)
{
    return mysql->warnings;
}

const char *cli_mysql_version(const CliMysql *mysql)
{
    return mysql->version;
}

int cli_mysql_sealed(const CliMysql *mysql)
{
    return mysql->sealed;
}

void cli_mysql_canceller(const CliMysql *mysql, CliMysqlCancel *cancel)
{
    *cancel = mysql->cancel;
}

int cli_mysql_cancel_sent(const CliMysqlCancel *cancel, CliLine *said)
{
    CliDbFailure failure;
    CliMysql *const other =
        cli_mysql_connected(cancel->host, cancel->port, cancel->user, cancel->password, "", 5000ull, &failure);
    if (other == NULL)
    {
        cli_db_failure_written(&failure, said);
        return 0;
    }
    char kill[64];
    CliLine statement = {kill, sizeof(kill), 0ull};
    cli_line_text(&statement, "KILL QUERY ");
    cli_line_decimal(&statement, cancel->id, 1u);
    const int killed = cli_mysql_all(other, statement.out, statement.at, rows_passed, NULL, &failure);
    if (!killed)
    {
        cli_db_failure_written(&failure, said);
    }
    cli_mysql_closed(other);
    return killed;
}

void cli_mysql_literal(const char *text, CliLine *out)
{
    cli_line_character(out, '\'');
    for (const char *letter = text; *letter != '\0'; letter += 1)
    {
        if (*letter == '\\')
        {
            cli_line_text(out, "\\\\");
        }
        else if (*letter == '\'')
        {
            cli_line_text(out, "''");
        }
        else
        {
            cli_line_character(out, *letter);
        }
    }
    cli_line_character(out, '\'');
}
