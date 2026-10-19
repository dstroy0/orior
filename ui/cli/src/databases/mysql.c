// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql.c: a MySQL connection's packets, read off the socket through a room of their own and sent in pieces of at
// most 16 MiB; and the handshake and sign-in, by mysql_native_password, caching_sha2_password, or the password sealed
// by the server's RSA key where caching_sha2_password asks for it whole
#include "../text/digest.h"
#include "../text/json_tree.h"
#include "mysql_internal.h"

// `size` bytes read whole off the connection, through its room, or passed over where `out` is NULL: 1, or 0 where it
// broke
static int mysql_read(CliMysql *mysql, unsigned char *out, unsigned long long size)
{
    for (unsigned long long at = 0ull; at < size;)
    {
        if (mysql->start == mysql->end)
        {
            const long long got = cli_socket_some(&mysql->socket, mysql->in, sizeof(mysql->in));
            if (got <= 0ll)
            {
                return 0;
            }
            mysql->start = 0ull;
            mysql->end = (unsigned long long)got;
        }
        const unsigned long long have = mysql->end - mysql->start;
        const unsigned long long piece = (have < size - at) ? have : size - at;
        if (out != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = out + at, .src = mysql->in + mysql->start, .bytes = (size_t)piece});
        }
        mysql->start += piece;
        at += piece;
    }
    return 1;
}

int cli_mysql_packet(CliMysql *mysql, CliDbFailure *failure)
{
    mysql->size = 0ull;
    int fits = 1;
    for (;;)
    {
        unsigned char head[4];
        if (!mysql_read(mysql, head, sizeof(head)))
        {
            cli_db_failure_said(failure, "the connection broke: the server closed it or did not answer");
            return 0;
        }
        const unsigned long long length =
            (unsigned long long)head[0] | ((unsigned long long)head[1] << 8u) | ((unsigned long long)head[2] << 16u);
        mysql->sequence = (unsigned char)(head[3] + 1u);
        fits = fits && (mysql->size + length <= sizeof(mysql->packet));
        if (!mysql_read(mysql, fits ? mysql->packet + mysql->size : NULL, length))
        {
            cli_db_failure_said(failure, "the connection broke: the server closed it or did not answer");
            return 0;
        }
        mysql->size += fits ? length : 0ull;
        if (length < 0xffffffull)
        {
            break;
        }
    }
    if (!fits)
    {
        cli_db_failure_said(failure, "a packet is past the room orior reads one in");
    }
    return fits;
}

int cli_mysql_sent(CliMysql *mysql, const unsigned char *payload, unsigned long long size, CliDbFailure *failure)
{
    unsigned long long last = 0ull;
    int sent = 1;
    if (size == 0ull)
    {
        const unsigned char head[4] = {0u, 0u, 0u, mysql->sequence};
        sent = cli_socket_written(&mysql->socket, head, sizeof(head));
        mysql->sequence = (unsigned char)(mysql->sequence + 1u);
    }
    for (unsigned long long at = 0ull; sent && (at < size); at += last)
    {
        last = ((size - at) < 0xffffffull) ? (size - at) : 0xffffffull;
        const unsigned char head[4] = {(unsigned char)(last & 0xffu), (unsigned char)((last >> 8u) & 0xffu),
                                       (unsigned char)((last >> 16u) & 0xffu), mysql->sequence};
        sent = cli_socket_written(&mysql->socket, head, sizeof(head)) &&
               cli_socket_written(&mysql->socket, payload + at, last);
        mysql->sequence = (unsigned char)(mysql->sequence + 1u);
    }
    // a payload whose last piece fills a packet whole is ended by an empty one
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

void cli_mysql_failure_of(const unsigned char *payload, unsigned long long size, CliDbFailure *failure)
{
    memor.set(&(MemoriaCfg){.dst = failure, .val = 0, .bytes = sizeof(*failure)});
    const unsigned int code = ((size > 1u) ? payload[1] : 0u) | ((size > 2u) ? ((unsigned int)payload[2] << 8u) : 0u);
    const int has_state = (size > 3u) && (payload[3] == '#');
    CliLine message = {failure->message, sizeof(failure->message), 0ull};
    const unsigned long long from = has_state ? 9ull : 3ull;
    cli_lossy(&message, payload + from, (size > from) ? size - from : 0ull);
    cli_line_closed(&message);
    CliLine code_line = {failure->code, sizeof(failure->code), 0ull};
    cli_line_decimal(&code_line, code, 1u);
    if (has_state)
    {
        cli_line_character(&code_line, ' ');
        cli_lossy(&code_line, payload + 4u, (size >= 9u) ? 5ull : ((size > 4u) ? size - 4u : 0ull));
    }
    cli_line_closed(&code_line);
}

int cli_mysql_lenenc(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
                     unsigned long long *value)
{
    if (*at >= size)
    {
        return 0;
    }
    const unsigned char first = bytes[*at];
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
    unsigned long long read = 0ull;
    for (unsigned int index = 0u; index < width; index += 1u)
    {
        read |= (unsigned long long)((*at + index < size) ? bytes[*at + index] : 0u) << (8u * index);
    }
    if (*at + width > size)
    {
        return 0;
    }
    *at += width;
    *value = read;
    return 1;
}

int cli_mysql_lenenc_bytes(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
                           unsigned long long *start, unsigned long long *length)
{
    unsigned long long count = 0ull;
    if (!cli_mysql_lenenc(bytes, size, at, &count) || (*at + count > size))
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
unsigned int cli_mysql_answer(const char *password, const unsigned char *scramble, unsigned long long size, int caching,
                              unsigned char *answer)
{
    if (password[0] == '\0')
    {
        return 0u;
    }
    unsigned char once[32];
    unsigned char twice[32];
    unsigned char mixed[32];
    unsigned char joined[96];
    const unsigned int width = caching ? 32u : 20u;
    const unsigned long long kept = (size < 64u) ? size : 64ull;
    if (caching)
    {
        cli_sha256_bytes(password, cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_CELLS_POOL}), once);
        cli_sha256_bytes(once, width, twice);
        memor.cpy(&(MemoriaCfg){.dst = joined, .src = twice, .bytes = width});
        memor.cpy(&(MemoriaCfg){.dst = joined + width, .src = scramble, .bytes = (size_t)kept});
        cli_sha256_bytes(joined, width + kept, mixed);
    }
    else
    {
        cli_sha1_bytes(password, cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_CELLS_POOL}), once);
        cli_sha1_bytes(once, width, twice);
        memor.cpy(&(MemoriaCfg){.dst = joined, .src = scramble, .bytes = (size_t)kept});
        memor.cpy(&(MemoriaCfg){.dst = joined + kept, .src = twice, .bytes = width});
        cli_sha1_bytes(joined, kept + width, mixed);
    }
    for (unsigned int at = 0u; at < width; at += 1u)
    {
        answer[at] = (unsigned char)(once[at] ^ mixed[at]);
    }
    return width;
}
