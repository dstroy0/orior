// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_head.c: the head of a request as it goes out, the head of its answer kept as it is read, the answer's body
// undone where it was packed, and whether that body is text
#include "../command_line/cells.h"
#include "../editor/archive.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

int cli_requests_head_kept(CliRequestsAnswer *answer, const char *text, unsigned long long size, CliRequestsText *kept)
{
    if (answer->head_used + size + 1u > sizeof(answer->head))
    {
        return 0;
    }
    kept->at = answer->head_used;
    kept->size = size;
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
    if (!cli_requests_head_kept(answer, line + name_start, name_end - name_start, &answer->header_name[at]) ||
        !cli_requests_head_kept(answer, line + value_start, value_end - value_start, &answer->header_value[at]))
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
            cli_requests_holds(hop->header_name[at], length, name))
        {
            return 1;
        }
    }
    return 0;
}

// a header written where the request names none of its name
static void head_given(const CliRequestsSent *hop, const char *name, const char *lower, const char *value, CliLine *out)
{
    if (!head_has(hop, lower))
    {
        cli_line_text(out, name);
        cli_line_text(out, ": ");
        cli_line_text(out, value);
        cli_line_text(out, "\r\n");
    }
}

void cli_requests_head_written(const CliRequestsSent *hop, const CliRequestsAddress *address, CliLine *out)
{
    cli_line_text(out, hop->method);
    cli_line_character(out, ' ');
    cli_line_text(out, address->target);
    cli_line_text(out, " HTTP/1.1\r\n");
    char host[CLI_REQUESTS_ADDRESS + 16u];
    CliLine host_line = {host, sizeof(host), 0ull};
    const int bracketed =
        (cellul.chr(&(CatenaFinitaCfg){.src = address->host, .cap = CLI_CELLS_POOL, .byte = ':'}) != NULL);
    cli_line_text(&host_line, bracketed ? "[" : "");
    cli_line_text(&host_line, address->host);
    cli_line_text(&host_line, bracketed ? "]" : "");
    if (address->port != (address->secure ? 443u : 80u))
    {
        cli_line_character(&host_line, ':');
        cli_line_decimal(&host_line, address->port, 1u);
    }
    cli_line_closed(&host_line);
    head_given(hop, "Host", "host", host, out);
    for (unsigned int at = 0u; at < hop->headers; at += 1u)
    {
        cli_line_text(out, hop->header_name[at]);
        cli_line_text(out, ": ");
        cli_line_text(out, hop->header_value[at]);
        cli_line_text(out, "\r\n");
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
        CliLine length_line = {length, sizeof(length), 0ull};
        cli_line_decimal(&length_line, hop->body_size, 1u);
        cli_line_closed(&length_line);
        head_given(hop, "Content-Length", "content-length", length, out);
    }
    head_given(hop, "Connection", "connection", "close", out);
    cli_line_text(out, "\r\n");
}

// whether a header's value, ASCII case aside, is `lower`
static int head_is(const CliRequestsAnswer *answer, const CliRequestsText *value, const char *lower)
{
    return (value != NULL) && (value->size == cellul.len(&(CatenaFinitaCfg){.src = lower, .cap = CLI_CELLS_POOL})) &&
           cli_requests_holds(answer->head + value->at, value->size, lower);
}

// the room's unpacked body a cell of `bytes`, CLI_REQUESTS_BODY at the most, the one before given back where it is
// smaller: 1, or 0 with why said where the cell is refused
static int head_unpacked_room(CliRequestsRoom *room, unsigned long long bytes, CliLine *said)
{
    const unsigned long long wanted = (bytes < 1ull) ? 1ull : ((bytes > CLI_REQUESTS_BODY) ? CLI_REQUESTS_BODY : bytes);
    if ((room->unpacked != NULL) && (room->unpacked_room >= wanted))
    {
        return 1;
    }
    unsigned char *const cell = cli_cell_taken(wanted, "an answer's body undone");
    if (cell == NULL)
    {
        cli_line_text(said, "the answer's body undone could not be held");
        return 0;
    }
    cli_cell_given(room->unpacked);
    room->unpacked = cell;
    room->unpacked_room = wanted;
    return 1;
}

int cli_requests_decoded(CliRequestsRoom *room, unsigned long long size, CliLine *said)
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
        const unsigned long long told =
            (size >= 4ull)
                ? ((unsigned long long)raw[size - 4ull] | ((unsigned long long)raw[size - 3ull] << 8u) |
                   ((unsigned long long)raw[size - 2ull] << 16u) | ((unsigned long long)raw[size - 1ull] << 24u))
                : 0ull;
        if (!head_unpacked_room(room, told, said) ||
            !cli_gunzip(raw, size, room->unpacked, room->unpacked_room, &unpacked, said))
        {
            return 0;
        }
    }
    else if (head_is(answer, encoding, "deflate"))
    {
        // a deflate says no size: the room begins at four times the packed bytes and doubles while it fills
        const int zlib =
            (size > 2u) && ((raw[0] & 0x0fu) == 8u) && ((((unsigned int)raw[0] << 8u) | raw[1]) % 31u == 0u);
        const unsigned long long said_at = said->at;
        unsigned long long room_bytes = 4ull * size;
        for (;;)
        {
            unsigned long long used = 0ull;
            if (!head_unpacked_room(room, room_bytes, said))
            {
                return 0;
            }
            if (cli_inflate(zlib ? (raw + 2) : raw, zlib ? (size - 2u) : size, room->unpacked, room->unpacked_room,
                            &unpacked, &used, said))
            {
                break;
            }
            if ((unpacked < room->unpacked_room) || (room->unpacked_room >= CLI_REQUESTS_BODY))
            {
                return 0;
            }
            said->at = said_at;
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
    int text = (size >= 5u) && cli_requests_holds(kind, 5u, "text/");
    for (unsigned int at = 0u; !text && (at < 7u); at += 1u)
    {
        text = cli_requests_holds(kind, size, texts[at]);
    }
    if (text)
    {
        return 1;
    }
    static const char *const starts[3] = {"image/", "audio/", "video/"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        if ((size >= 6u) && cli_requests_holds(kind, 6u, starts[at]))
        {
            return 0;
        }
    }
    if (cli_requests_holds(kind, size, "octet-stream") || cli_requests_holds(kind, size, "pdf") ||
        cli_requests_holds(kind, size, "zip"))
    {
        return 0;
    }
    return cli_utf8_whole((const char *)answer->body, answer->size) &&
           (memor.chr(&(MemoriaCfg){.src = answer->body, .val = 0, .bytes = (size_t)answer->size}) == NULL);
}
