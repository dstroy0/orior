// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_http.c: one request sent over HTTP/1.1 from this process, through the system's TLS for https, and its
// answer read whole: each answer of 1xx but 101 passed over, a chunked body, one of a length and one read to the
// connection's end, the most of it CLI_REQUESTS_BODY and the rest cut; gzip and deflate undone; and how long each
// part took
#include "../comms/tls.h"
#include "../machine/net.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

// how long a connection may take to open, and an answer to come, in milliseconds
#define CLI_REQUESTS_CONNECTING 15000u
#define CLI_REQUESTS_ANSWERING 60000u

// the longest line of a head, and the most read from the connection at once
#define CLI_REQUESTS_LINE (64u << 10u)
#define CLI_REQUESTS_CHUNK (16u << 10u)

// A connection: its socket, its TLS session where it is https, and the room its bytes are read into
typedef struct
{
    CliSocket socket;
    CliTls *tls;
    CliRequestsRoom *room;
} HttpConnection;

// microseconds as milliseconds to a tenth, as Rust's (secs * 10000).round() / 10 has them
static double http_millis(unsigned long long microseconds)
{
    return (double)((microseconds + 50u) / 100u) / 10.0;
}

// more read from the connection after what is held: 1 where some came, 0 at its end, -1 with why said
static int http_fill(HttpConnection *connection, CliLine *said)
{
    CliRequestsRoom *const room = connection->room;
    if (room->start > 0u)
    {
        memor.move_down(&(MemoriaCfg){
            .dst = room->reading, .src = room->reading + room->start, .bytes = (size_t)(room->end - room->start)});
        room->end -= room->start;
        room->start = 0ull;
    }
    const long long read = (connection->tls != NULL)
                               ? cli_tls_read(connection->tls, room->reading + room->end, CLI_REQUESTS_CHUNK, said)
                               : cli_socket_some(&connection->socket, room->reading + room->end, CLI_REQUESTS_CHUNK);
    if (read == CLI_SOCKET_WAITED)
    {
        cli_line_text(said, "no answer came within 60 s");
        return -1;
    }
    if ((read == CLI_SOCKET_FAILED) && (connection->tls == NULL))
    {
        cli_line_text(said, "the connection failed as it was read");
    }
    if (read < 0)
    {
        return -1;
    }
    // a count read is not negative here and is at most CLI_REQUESTS_CHUNK
    room->end += (unsigned long long)read;
    return read > 0;
}

// the next line of the head, its CRLF taken off, `*size` bytes at `*line` of the reading, taken: 1, or 0 with why
// said. The line stands until the next read
static int http_line(HttpConnection *connection, unsigned long long *line, unsigned long long *size, CliLine *said)
{
    CliRequestsRoom *const room = connection->room;
    unsigned long long looked = room->start;
    for (;;)
    {
        while ((looked + 1u < room->end) && ((room->reading[looked] != '\r') || (room->reading[looked + 1u] != '\n')))
        {
            looked += 1u;
        }
        if (looked + 1u < room->end)
        {
            *line = room->start;
            *size = looked - room->start;
            room->start = looked + 2u;
            return 1;
        }
        const unsigned long long seen = looked - room->start;
        const int filled = (room->end - room->start <= CLI_REQUESTS_LINE) ? http_fill(connection, said) : 0;
        if (filled <= 0)
        {
            if (filled == 0)
            {
                cli_line_text(said, "the answer ended in its headers");
            }
            return 0;
        }
        looked = room->start + seen;
    }
}

// `length` bytes of the body read, those that fit in CLI_REQUESTS_BODY put after the `*size` in `raw`, `*cut` set
// where any did not: 1, or 0 with why said
static int http_exactly(HttpConnection *connection, unsigned long long length, unsigned long long *size, int *cut,
                        CliLine *said)
{
    CliRequestsRoom *const room = connection->room;
    unsigned long long left = length;
    while (left > 0u)
    {
        const int filled = (room->start == room->end) ? http_fill(connection, said) : 1;
        if (filled <= 0)
        {
            if (filled == 0)
            {
                cli_line_text(said, "the answer ended before its length");
            }
            return 0;
        }
        const unsigned long long held = room->end - room->start;
        const unsigned long long take = (left < held) ? left : held;
        const unsigned long long fits = CLI_REQUESTS_BODY - *size;
        const unsigned long long put = (take < fits) ? take : fits;
        memor.cpy(&(MemoriaCfg){.dst = room->raw + *size, .src = room->reading + room->start, .bytes = (size_t)put});
        *size += put;
        *cut = *cut || (take > fits);
        room->start += take;
        left -= take;
    }
    return 1;
}

// the body read to the connection's end, cut once it passes CLI_REQUESTS_BODY: 1, or 0 with why said
static int http_to_end(HttpConnection *connection, unsigned long long *size, int *cut, CliLine *said)
{
    CliRequestsRoom *const room = connection->room;
    for (;;)
    {
        const unsigned long long held = room->end - room->start;
        const unsigned long long fits = CLI_REQUESTS_BODY - *size;
        const unsigned long long put = (held < fits) ? held : fits;
        memor.cpy(&(MemoriaCfg){.dst = room->raw + *size, .src = room->reading + room->start, .bytes = (size_t)put});
        *size += put;
        room->start = room->end;
        if (held > fits)
        {
            *cut = 1;
            return 1;
        }
        const int filled = http_fill(connection, said);
        if (filled <= 0)
        {
            return filled == 0;
        }
    }
}

// a chunked body read, its trailer passed over: 1, or 0 with why said
static int http_chunked(HttpConnection *connection, unsigned long long *size, int *cut, CliLine *said)
{
    const unsigned char *const reading = connection->room->reading;
    for (;;)
    {
        unsigned long long line = 0ull;
        unsigned long long length = 0ull;
        if (!http_line(connection, &line, &length, said))
        {
            return 0;
        }
        const char *const text = (const char *)reading + line;
        unsigned long long start = 0ull;
        unsigned long long end = length;
        const char *const semicolon = memor.chr(&(MemoriaCfg){.src = text, .val = ';', .bytes = (size_t)length});
        end = (semicolon != NULL) ? (unsigned long long)(semicolon - text) : end;
        cli_requests_trimmed(text, &start, &end);
        unsigned long long chunk = 0ull;
        if (!cli_requests_number(text + start, end - start, 16u, 0xffffffffffffffffull, &chunk))
        {
            cli_line_text(said, "a chunk's size reads ");
            cli_json_quoted_lossy(said, reading + line, length);
            return 0;
        }
        if (chunk == 0u)
        {
            do
            {
                if (!http_line(connection, &line, &length, said))
                {
                    return 0;
                }
            } while (length > 0u);
            return 1;
        }
        if (!http_exactly(connection, chunk, size, cut, said) || !http_line(connection, &line, &length, said))
        {
            return 0;
        }
    }
}

// the head of the answer read, an answer of 1xx but 101 passed over, the wait for its first line timed from `step`:
// 1, or 0 with why said
static int http_head(HttpConnection *connection, unsigned long long step, CliLine *said)
{
    CliRequestsAnswer *const answer = &connection->room->answer;
    const unsigned char *const reading = connection->room->reading;
    do
    {
        unsigned long long line = 0ull;
        unsigned long long length = 0ull;
        if (!http_line(connection, &line, &length, said))
        {
            return 0;
        }
        answer->times.wait = http_millis(cli_clock_microseconds() - step);
        const char *const text = (const char *)reading + line;
        const char *const space = memor.chr(&(MemoriaCfg){.src = text, .val = ' ', .bytes = (size_t)length});
        const unsigned long long version = (space != NULL) ? (unsigned long long)(space - text) : length;
        const char *const code = (space != NULL) ? (space + 1) : NULL;
        const char *const after =
            (code != NULL) ? memor.chr(&(MemoriaCfg){.src = code, .val = ' ', .bytes = (size_t)(text + length - code)})
                           : NULL;
        const unsigned long long code_size =
            (code == NULL) ? 0ull : (unsigned long long)(((after != NULL) ? after : (text + length)) - code);
        unsigned long long status = 0ull;
        if ((code == NULL) || !cli_requests_number(code, code_size, 10u, 65535u, &status))
        {
            cli_line_text(said, "the answer began ");
            cli_json_quoted_lossy(said, reading + line, length);
            cli_line_text(said, ", which is no status line");
            return 0;
        }
        // a status read is at most 65535
        answer->status = (unsigned int)status;
        answer->headers = 0u;
        answer->head_used = 0ull;
        const unsigned long long reason = (after != NULL) ? (unsigned long long)(after + 1 - text) : length;
        int kept = cli_requests_head_kept(answer, text, version, &answer->version) &&
                   cli_requests_head_kept(answer, text + reason, length - reason, &answer->reason);
        while (kept)
        {
            if (!http_line(connection, &line, &length, said))
            {
                return 0;
            }
            if (length == 0u)
            {
                break;
            }
            kept = cli_requests_head_line(answer, (const char *)reading + line, length);
        }
        if (!kept)
        {
            cli_line_text(said, "the answer's head is larger than orior holds");
            return 0;
        }
    } while ((answer->status >= 100u) && (answer->status < 200u) && (answer->status != 101u));
    return 1;
}

// the request written whole, its head and then its body: 1, or 0 with why said
static int http_written(HttpConnection *connection, const char *host, CliLine *said)
{
    CliRequestsRoom *const room = connection->room;
    // the room's unpacked body holds nothing until the answer is read, so the head is written there
    CliLine head = {(char *)room->unpacked, CLI_REQUESTS_BODY, 0ull};
    cli_requests_head_written(&room->hop, &room->address, &head);
    if (!cli_line_fits(&head))
    {
        cli_line_text(said, "the request is larger than orior holds");
        return 0;
    }
    if (connection->tls != NULL)
    {
        return cli_tls_written(connection->tls, head.out, head.at, said) &&
               cli_tls_written(connection->tls, room->hop.body, room->hop.body_size, said);
    }
    if (!cli_socket_written(&connection->socket, head.out, head.at) ||
        !cli_socket_written(&connection->socket, room->hop.body, room->hop.body_size))
    {
        cli_line_text(said, host);
        cli_line_text(said, ": the request could not be sent");
        return 0;
    }
    return 1;
}

// the answer's body read as its head says it comes, its bytes as they came into `raw`, `*size` of them: 1, or 0 with
// why said
static int http_body(HttpConnection *connection, unsigned long long *size, CliLine *said)
{
    CliRequestsAnswer *const answer = &connection->room->answer;
    const unsigned int status = answer->status;
    const int empty =
        (cellul.eq(&(CatenaFinitaCfg){.src = connection->room->hop.method, .other = "HEAD", .cap = CLI_CELLS_POOL})) ||
        (status == 204u) || (status == 304u) || ((status >= 100u) && (status < 200u));
    const CliRequestsText *const encoding = cli_requests_header(answer, "Transfer-Encoding");
    const CliRequestsText *const length = cli_requests_header(answer, "Content-Length");
    unsigned long long count = 0ull;
    int cut = 0;
    int read = 1;
    *size = 0ull;
    if (empty)
    {
    }
    else if ((encoding != NULL) && cli_requests_holds(answer->head + encoding->at, encoding->size, "chunked"))
    {
        read = http_chunked(connection, size, &cut, said);
    }
    else if ((length != NULL) &&
             cli_requests_number(answer->head + length->at, length->size, 10u, 0xffffffffffffffffull, &count))
    {
        read = http_exactly(connection, count, size, &cut, said);
    }
    else
    {
        read = http_to_end(connection, size, &cut, said);
    }
    answer->cut = cut;
    answer->body = connection->room->raw;
    answer->size = *size;
    return read;
}

int cli_requests_once(CliRequestsRoom *room, int verify, CliLine *said)
{
    const unsigned long long began = cli_clock_microseconds();
    CliRequestsSent *const hop = &room->hop;
    CliRequestsAnswer *const answer = &room->answer;
    const int taken = cli_requests_address(hop->url, &room->address, said);
    const char *const host = room->address.host;
    const int secure = room->address.secure;
    char purpose[CLI_REQUESTS_NAME + 16u];
    CliLine purpose_line = {purpose, sizeof(purpose), 0ull};
    cli_line_text(&purpose_line, "a ");
    cli_line_text(&purpose_line, hop->method);
    cli_line_text(&purpose_line, " request");
    cli_line_closed(&purpose_line);
    if (!taken || !cli_net_reach(hop->url, purpose, said))
    {
        return 0;
    }
    answer->status = 0u;
    answer->headers = 0u;
    answer->head_used = 0ull;
    answer->body = room->raw;
    answer->size = 0ull;
    answer->cut = 0;
    answer->binary = 0;
    memor.set(&(MemoriaCfg){.dst = &answer->times, .val = 0, .bytes = sizeof(answer->times)});
    room->start = 0ull;
    room->end = 0ull;
    HttpConnection connection = {{0ull}, NULL, room};
    unsigned long long resolved = 0ull;
    unsigned long long step = cli_clock_microseconds();
    if (!cli_socket_connected_timed(&connection.socket, host, room->address.port, CLI_REQUESTS_CONNECTING, &resolved,
                                    said))
    {
        return 0;
    }
    answer->times.resolve = http_millis(resolved);
    answer->times.connect = http_millis(cli_clock_microseconds() - step - resolved);
    cli_socket_waited(&connection.socket, CLI_REQUESTS_ANSWERING);
    cli_socket_write_waited(&connection.socket, CLI_REQUESTS_ANSWERING);
    step = cli_clock_microseconds();
    connection.tls = secure ? cli_tls_connect(&connection.socket, host, verify, said) : NULL;
    int done = !secure || (connection.tls != NULL);
    answer->times.tls = secure ? http_millis(cli_clock_microseconds() - step) : 0.0;
    step = cli_clock_microseconds();
    done = done && http_written(&connection, host, said) && http_head(&connection, step, said);
    step = cli_clock_microseconds();
    unsigned long long size = 0ull;
    done = done && http_body(&connection, &size, said);
    answer->times.read = http_millis(cli_clock_microseconds() - step);
    if (connection.tls != NULL)
    {
        cli_tls_closed(connection.tls);
    }
    cli_socket_closed(&connection.socket);
    if (!done || (!answer->cut && !cli_requests_decoded(room, size, said)))
    {
        return 0;
    }
    answer->binary = !cli_requests_is_text(answer);
    answer->times.total = http_millis(cli_clock_microseconds() - began);
    answer->at = cli_wall_milliseconds();
    memor.cpy(&(MemoriaCfg){.dst = answer->url, .src = hop->url, .bytes = sizeof(answer->url)});
    return 1;
}
