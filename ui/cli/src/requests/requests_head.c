// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_head.c: the head of a request as it goes out, the head of its answer stored as it is read, the answer's body
// undone where it was packed, and whether that body is text
#include "../command_line/cells.h"
#include "../editor/archive.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

int cli_requests_head_stored(CliRequestsAnswer *answer, const char *text, unsigned long long size,
                             CliRequestsText *text_span)
{
    if (answer->head_used + size + 1u > sizeof(answer->head))
    {
        return 0;
    }
    text_span->at = answer->head_used;
    text_span->size = size;
    memor.cpy(&(MemoriaCfg){.dst = answer->head + answer->head_used, .src = text, .bytes = (size_t)size});
    answer->head[answer->head_used + size] = '\0';
    answer->head_used += size + 1u;
    return 1;
}

int cli_requests_head_line(CliRequestsAnswer *answer, const char *line, unsigned long long size)
{
    const char *const colon = memor.chr(&(MemoriaCfg){.src = line, .val = ':', .bytes = (size_t)size});
    if (colon == NULL)
    {
        return 1;
    }
    if (answer->headers == CLI_REQUESTS_ANSWER_HEADERS)
    {
        return 0;
    }
    unsigned long long name_start = 0ull;
    unsigned long long name_end = (unsigned long long)(colon - line);
    unsigned long long value_start = name_end + 1u;
    unsigned long long value_end = size;
    cli_requests_trimmed(line, &name_start, &name_end);
    cli_requests_trimmed(line, &value_start, &value_end);
    const unsigned int at = answer->headers;
    if (!cli_requests_head_stored(answer, line + name_start, name_end - name_start, &answer->header_name[at]) ||
        !cli_requests_head_stored(answer, line + value_start, value_end - value_start, &answer->header_value[at]))
    {
        return 0;
    }
    answer->headers += 1u;
    return 1;
}

// whether the request has a header named `name`, ASCII case aside
static int head_has(const CliRequestsSent *hop, const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    for (unsigned int at = 0u; at < hop->headers; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = hop->header_name[at], .cap = CLI_CELLS_POOL}) == length) &&
            cli_requests_contains(hop->header_name[at], length, name))
        {
            return 1;
        }
    }
    return 0;
}

// a header written where the request names none of its name
static void head_given(const CliRequestsSent *hop, const char *name, const char *lower, const char *value,
                       VerbaLinea *out)
{
    if (!head_has(hop, lower))
    {
        mmgr_line_put(out, name);
        mmgr_line_put(out, ": ");
        mmgr_line_put(out, value);
        mmgr_line_put(out, "\r\n");
    }
}

void cli_requests_head_written(const CliRequestsSent *hop, const CliRequestsAddress *address, VerbaLinea *out)
{
    mmgr_line_put(out, hop->method);
    mmgr_line_ch(out, ' ');
    mmgr_line_put(out, address->target);
    mmgr_line_put(out, " HTTP/1.1\r\n");
    char host[CLI_REQUESTS_ADDRESS + 16u];
    VerbaLinea host_line = {host, sizeof(host), 0ull};
    const int bracketed =
        (cellul.chr(&(CatenaFinitaCfg){.src = address->host, .cap = CLI_CELLS_POOL, .byte = ':'}) != NULL);
    mmgr_line_put(&host_line, bracketed ? "[" : "");
    mmgr_line_put(&host_line, address->host);
    mmgr_line_put(&host_line, bracketed ? "]" : "");
    if (address->port != (address->secure ? 443u : 80u))
    {
        mmgr_line_ch(&host_line, ':');
        mmgr_line_decimal(&host_line, address->port, 1u);
    }
    mmgr_line_finish(&host_line);
    head_given(hop, "Host", "host", host, out);
    for (unsigned int at = 0u; at < hop->headers; at += 1u)
    {
        mmgr_line_put(out, hop->header_name[at]);
        mmgr_line_put(out, ": ");
        mmgr_line_put(out, hop->header_value[at]);
        mmgr_line_put(out, "\r\n");
    }
    head_given(hop, "User-Agent", "user-agent", "orior/" CLI_VERSION, out);
    head_given(hop, "Accept", "accept", "*/*", out);
    head_given(hop, "Accept-Encoding", "accept-encoding", "gzip, deflate", out);
    const int sends = (cellul.eq(&(CatenaFinitaCfg){.src = hop->method, .other = "POST", .cap = CLI_CELLS_POOL})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = hop->method, .other = "PUT", .cap = CLI_CELLS_POOL})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = hop->method, .other = "PATCH", .cap = CLI_CELLS_POOL}));
    if (((hop->body_size > 0u) || sends) && !head_has(hop, "transfer-encoding"))
    {
        char length[24];
        VerbaLinea length_line = {length, sizeof(length), 0ull};
        mmgr_line_decimal(&length_line, hop->body_size, 1u);
        mmgr_line_finish(&length_line);
        head_given(hop, "Content-Length", "content-length", length, out);
    }
    head_given(hop, "Connection", "connection", "close", out);
    mmgr_line_put(out, "\r\n");
}

// whether a header's value, ASCII case aside, is `lower`
static int head_is(const CliRequestsAnswer *answer, const CliRequestsText *value, const char *lower)
{
    return (value != NULL) && (value->size == cellul.len(&(CatenaFinitaCfg){.src = lower, .cap = CLI_CELLS_POOL})) &&
           cli_requests_contains(answer->head + value->at, value->size, lower);
}

// the room's unpacked body a cell of `bytes`, CLI_REQUESTS_BODY at the most, the one before given back where it is
// smaller: 1, or 0 with why given where the cell is refused
static int head_unpacked_room(CliRequestsRoom *room, unsigned long long bytes, VerbaLinea *err_msg)
{
    const unsigned long long wanted = (bytes < 1ull) ? 1ull : ((bytes > CLI_REQUESTS_BODY) ? CLI_REQUESTS_BODY : bytes);
    if ((room->unpacked != NULL) && (room->unpacked_room >= wanted))
    {
        return 1;
    }
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(wanted, NULL);
    if (cell == NULL)
    {
        mmgr_line_put(err_msg, "the answer's body undone could not be stored");
        return 0;
    }
    cli_cell_release(room->unpacked, NULL);
    room->unpacked = cell;
    room->unpacked_room = wanted;
    return 1;
}

int cli_requests_decoded(CliRequestsRoom *room, unsigned long long size, VerbaLinea *err_msg)
{
    CliRequestsAnswer *const answer = &room->answer;
    const CliRequestsText *const encoding = cli_requests_header(answer, "Content-Encoding");
    const unsigned char *const raw = (const unsigned char *)room->raw.out;
    unsigned long long unpacked = 0ull;
    answer->body = raw;
    answer->size = size;
    if (head_is(answer, encoding, "gzip") || head_is(answer, encoding, "x-gzip"))
    {
        // a gzip ends with its unpacked size, so the room is taken once at that size
        const unsigned long long unpacked_size =
            (size >= 4ull)
                ? ((unsigned long long)raw[size - 4ull] | ((unsigned long long)raw[size - 3ull] << 8u) |
                   ((unsigned long long)raw[size - 2ull] << 16u) | ((unsigned long long)raw[size - 1ull] << 24u))
                : 0ull;
        if (!head_unpacked_room(room, unpacked_size, err_msg) ||
            !cli_gunzip(raw, size, room->unpacked, room->unpacked_room, &unpacked, err_msg))
        {
            return 0;
        }
    }
    else if (head_is(answer, encoding, "deflate"))
    {
        // a deflate gives no size: the room begins at four times the packed bytes and doubles while it fills
        const int zlib =
            (size > 2u) && ((raw[0] & 0x0fu) == 8u) && ((((unsigned int)raw[0] << 8u) | raw[1]) % 31u == 0u);
        const unsigned long long err_at = err_msg->at;
        unsigned long long room_bytes = 4ull * size;
        for (;;)
        {
            unsigned long long used = 0ull;
            if (!head_unpacked_room(room, room_bytes, err_msg))
            {
                return 0;
            }
            if (cli_inflate(zlib ? (raw + 2) : raw, zlib ? (size - 2u) : size, room->unpacked, room->unpacked_room,
                            &unpacked, &used, err_msg))
            {
                break;
            }
            if ((unpacked < room->unpacked_room) || (room->unpacked_room >= CLI_REQUESTS_BODY))
            {
                return 0;
            }
            err_msg->at = err_at;
            room_bytes = 2ull * room->unpacked_room;
        }
    }
    else
    {
        return 1;
    }
    answer->body = room->unpacked;
    answer->size = unpacked;
    return 1;
}

int cli_requests_is_text(const CliRequestsAnswer *answer)
{
    const CliRequestsText *const type = cli_requests_header(answer, "Content-Type");
    const char *const kind = (type != NULL) ? (answer->head + type->at) : "";
    const unsigned long long size = (type != NULL) ? type->size : 0ull;
    static const char *const texts[7] = {"json", "xml", "javascript", "x-www-form-urlencoded",
                                         "yaml", "csv", "graphql"};
    int text = (size >= 5u) && cli_requests_contains(kind, 5u, "text/");
    for (unsigned int at = 0u; !text && (at < 7u); at += 1u)
    {
        text = cli_requests_contains(kind, size, texts[at]);
    }
    if (text)
    {
        return 1;
    }
    static const char *const starts[3] = {"image/", "audio/", "video/"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        if ((size >= 6u) && cli_requests_contains(kind, 6u, starts[at]))
        {
            return 0;
        }
    }
    if (cli_requests_contains(kind, size, "octet-stream") || cli_requests_contains(kind, size, "pdf") ||
        cli_requests_contains(kind, size, "zip"))
    {
        return 0;
    }
    return cli_utf8_whole((const char *)answer->body, answer->size) &&
           (memor.chr(&(MemoriaCfg){.src = answer->body, .val = 0, .bytes = (size_t)answer->size}) == NULL);
}
