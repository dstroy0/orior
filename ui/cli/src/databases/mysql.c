// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql.c: a MySQL connection's packets, read off the socket through a capacity of their own and sent in pieces of at
// most 16 MiB; and the handshake and sign-in, by mysql_native_password, caching_sha2_password, or the password sealed
// by the server's RSA key where caching_sha2_password requests it whole
#include "../text/digest.h"
#include "../text/json_tree.h"
#include "mysql_internal.h"

typedef struct
{
    CliMysql *mysql;
    unsigned char *out;
    unsigned long long size;
} MysqlCtx;

// `size` bytes read whole off the connection, through its capacity, or passed over where `out` is NULL: 1, or 0 where
// it broke
EMBED_INLINE int mysql_read(const MysqlCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        if (args->mysql->start == args->mysql->end)
        {
            const long long got =
                EMBED_CALL(cli_capacity_internal.socket_some, CliCapacityCtx, .sock = &args->mysql->socket,
                           .bytes_out = args->mysql->in, .capacity = sizeof(args->mysql->in));
            if (got <= 0LL)
            {
                return 0;
            }
            args->mysql->start = 0ULL;
            args->mysql->end = (unsigned long long)got;
        }
        const unsigned long long have = args->mysql->end - args->mysql->start;
        const unsigned long long piece = (have < args->size - at) ? have : args->size - at;
        if (args->out != NULL)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->out + at, .src = args->mysql->in + args->mysql->start, .bytes = (size_t)piece});
        }
        args->mysql->start += piece;
        at += piece;
    }
    return 1;
}

int cli_mysql_packet(const CliMysqlFailureCtx *args)
{
    CliDbFailure *failure = args->failure;
    args->mysql->size = 0ULL;
    int fits = 1;
    for (;;)
    {
        unsigned char head[4];
        if (!EMBED_CALL(mysql_read, MysqlCtx, .mysql = args->mysql, .out = head, .size = sizeof(head)))
        {
            EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                       .message = "the connection broke: the server closed it or did not answer");
            return 0;
        }
        const unsigned long long length =
            (unsigned long long)head[0] | ((unsigned long long)head[1] << 8u) | ((unsigned long long)head[2] << 16u);
        args->mysql->sequence = (unsigned char)(head[3] + 1u);
        fits = fits && (args->mysql->size + length <= sizeof(args->mysql->packet));
        if (!EMBED_CALL(mysql_read, MysqlCtx, .mysql = args->mysql,
                        .out = fits ? args->mysql->packet + args->mysql->size : NULL, .size = length))
        {
            EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                       .message = "the connection broke: the server closed it or did not answer");
            return 0;
        }
        args->mysql->size += fits ? length : 0ULL;
        if (length < 0xffffffULL)
        {
            break;
        }
    }
    if (!fits)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "a packet is past the capacity of the buffer orior_cli reads one in");
    }
    return fits;
}

int cli_mysql_send(const CliMysqlFailureCtx *args)
{
    CliDbFailure *failure = args->failure;
    unsigned long long last = 0ULL;
    int sent = 1;
    if (args->size == 0ULL)
    {
        const unsigned char head[4] = {0u, 0u, 0u, args->mysql->sequence};
        sent = EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->mysql->socket,
                          .bytes = head, .size = sizeof(head));
        args->mysql->sequence = (unsigned char)(args->mysql->sequence + 1u);
    }
    for (unsigned long long at = 0ULL; sent && (at < args->size); at += last)
    {
        last = ((args->size - at) < 0xffffffULL) ? (args->size - at) : 0xffffffULL;
        const unsigned char head[4] = {(unsigned char)(last & 0xffu), (unsigned char)((last >> 8u) & 0xffu),
                                       (unsigned char)((last >> 16u) & 0xffu), args->mysql->sequence};
        sent = EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->mysql->socket,
                          .bytes = head, .size = sizeof(head)) &&
               EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->mysql->socket,
                          .bytes = args->payload + at, .size = last);
        args->mysql->sequence = (unsigned char)(args->mysql->sequence + 1u);
    }
    // a payload whose last piece fills a packet whole is ended by an empty one
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

void cli_mysql_failure_get(const CliMysqlFailureCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->failure, .val = 0, .bytes = sizeof(*args->failure)});
    const unsigned int code =
        ((args->size > 1u) ? args->payload[1] : 0u) | ((args->size > 2u) ? ((unsigned int)args->payload[2] << 8u) : 0u);
    const int has_state = (args->size > 3u) && (args->payload[3] == '#');
    VerbaLinea message = {args->failure->message, sizeof(args->failure->message), 0ULL};
    const unsigned long long from = has_state ? 9ULL : 3ULL;
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &message,
               .text_bytes = args->payload + from, .length = (args->size > from) ? args->size - from : 0ULL);
    mmgr_line_finish(&message);
    VerbaLinea code_line = {args->failure->code, sizeof(args->failure->code), 0ULL};
    mmgr_line_decimal(&code_line, code, 1u);
    if (has_state)
    {
        mmgr_line_ch(&code_line, ' ');
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &code_line,
                   .text_bytes = args->payload + 4u,
                   .length = (args->size >= 9u) ? 5ULL : ((args->size > 4u) ? args->size - 4u : 0ULL));
    }
    mmgr_line_finish(&code_line);
}

int cli_mysql_lenenc(const CliMysqlSizeBytesAtCtx *args)
{
    unsigned long long *at = args->at;
    unsigned long long *value = args->value;
    if (*at >= args->size)
    {
        return 0;
    }
    const unsigned char first = args->bytes[*at];
    *at += 1u;
    const unsigned int width = (first == 0xfcu) ? 2u : (first == 0xfdu) ? 3u : (first == 0xfeu) ? 8u : 0u;
    if (first == 0xfbu)
    {
        return 0;
    }
    if (width == 0u)
    {
        *value = first;
        return 1;
    }
    unsigned long long read = 0ULL;
    for (unsigned int index = 0u; index < width; index += 1u)
    {
        read |= (unsigned long long)((*at + index < args->size) ? args->bytes[*at + index] : 0u) << (8u * index);
    }
    if (*at + width > args->size)
    {
        return 0;
    }
    *at += width;
    *value = read;
    return 1;
}

int cli_mysql_lenenc_bytes(const CliMysqlSizeBytesCtx *args)
{
    unsigned long long *at = args->at;
    unsigned long long *start = args->start;
    unsigned long long *length = args->length_pointer;
    unsigned long long count = 0ULL;
    if (!EMBED_CALL(cli_mysql_size_bytes_at_internal.lenenc, CliMysqlSizeBytesAtCtx, .bytes = args->bytes,
                    .size = args->size, .at = at, .value = &count) ||
        (*at + count > args->size))
    {
        return 0;
    }
    *start = *at;
    *length = count;
    *at += count;
    return 1;
}

// mysql_native_password's answer, SHA1(password) XOR SHA1(scramble, SHA1(SHA1(password))), or caching_sha2_password's,
// SHA256(password) XOR SHA256(SHA256(SHA256(password)), scramble), into `answer`: its length, 0 for an empty password
unsigned int cli_mysql_answer(const CliMysqlSizePasswordCtx *args)
{
    if (args->password[0] == '\0')
    {
        return 0u;
    }
    unsigned char once[32];
    unsigned char twice[32];
    unsigned char mixed[32];
    unsigned char joined[96];
    const unsigned int width = args->caching ? 32u : 20u;
    const unsigned long long copied_len = (args->size < 64u) ? args->size : 64ULL;
    if (args->caching)
    {
        EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = args->password,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->password, .cap = CLI_CELLS_POOL}),
                   .digest = once);
        EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = once, .size = width,
                   .digest = twice);
        memor.cpy(&(MemoriaCfg){.dst = joined, .src = twice, .bytes = width});
        memor.cpy(&(MemoriaCfg){.dst = joined + width, .src = args->scramble, .bytes = (size_t)copied_len});
        EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = joined,
                   .size = width + copied_len, .digest = mixed);
    }
    else
    {
        EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = args->password,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->password, .cap = CLI_CELLS_POOL}),
                   .digest = once);
        EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = once, .size = width,
                   .digest = twice);
        memor.cpy(&(MemoriaCfg){.dst = joined, .src = args->scramble, .bytes = (size_t)copied_len});
        memor.cpy(&(MemoriaCfg){.dst = joined + copied_len, .src = twice, .bytes = width});
        EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = joined, .size = copied_len + width,
                   .digest = mixed);
    }
    for (unsigned int at = 0u; at < width; at += 1u)
    {
        args->answer[at] = (unsigned char)(once[at] ^ mixed[at]);
    }
    return width;
}
