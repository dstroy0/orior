// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_http.c: one request sent over HTTP/1.1 from this process, through the system's TLS for https, and its
// answer read whole: each answer of 1xx but 101 passed over, a chunked body, one of a length and one read to the
// connection's end, the most of it CLI_REQUESTS_BODY and the rest cut; gzip and deflate undone; and how long each
// part took
#include "../command_line/cells.h"
#include "../comms/tls.h"
#include "../machine/net.h"
#include "../text/json_tree.h"
#include "../text/text.h"
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

// more read from the connection after what is buffered: 1 where some came, 0 at its end, -1 with why given
static int http_fill(HttpConnection *connection, VerbaLinea *err_msg)
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
                               ? cli_tls_read(connection->tls, room->reading + room->end, CLI_REQUESTS_CHUNK, err_msg)
                               : cli_socket_some(&connection->socket, room->reading + room->end, CLI_REQUESTS_CHUNK);
    if (read == CLI_SOCKET_WAITED)
    {
        mmgr_line_put(err_msg, "no answer came within 60 s");
        return -1;
    }
    if ((read == CLI_SOCKET_FAILED) && (connection->tls == NULL))
    {
        mmgr_line_put(err_msg, "the connection failed as it was read");
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
// given. The line stands until the next read
static int http_line(HttpConnection *connection, unsigned long long *line, unsigned long long *size,
                     VerbaLinea *err_msg)
{
    CliRequestsRoom *const room = connection->room;
    unsigned long long looked = room->start;
    for (;;)
    {
        const unsigned long long ended = cli_text_to_pair((const char *)room->reading, looked, room->end, '\r', '\n');
        looked = (ended < room->end) ? ended : ((room->end > (looked + 1u)) ? (room->end - 1u) : looked);
        if (looked + 1u < room->end)
        {
            *line = room->start;
            *size = looked - room->start;
            room->start = looked + 2u;
            return 1;
        }
        const unsigned long long seen = looked - room->start;
        const int filled = (room->end - room->start <= CLI_REQUESTS_LINE) ? http_fill(connection, err_msg) : 0;
        if (filled <= 0)
        {
            if (filled == 0)
            {
                mmgr_line_put(err_msg, "the answer ended in its headers");
            }
            return 0;
        }
        looked = room->start + seen;
    }
}

// `put` bytes of the body from the reading put after the `*size` in `raw`, and `*size` raised: 1, or 0 with why given
// where the line is full
static int http_put(CliRequestsRoom *room, unsigned long long put, unsigned long long *size, VerbaLinea *err_msg)
{
    mmgr_line_put_n(&room->raw, (const char *)room->reading + room->start, put);
    if (!mmgr_line_ok(&room->raw))
    {
        mmgr_line_put(err_msg, "the answer's body could not be stored");
        return 0;
    }
    *size += put;
    return 1;
}

// `length` bytes of the body read, those that fit in CLI_REQUESTS_BODY put after the `*size` in `raw`, `*cut` set where
// any did not: 1, or 0 with why given
static int http_exactly(HttpConnection *connection, unsigned long long length, unsigned long long *size, int *cut,
                        VerbaLinea *err_msg)
{
    CliRequestsRoom *const room = connection->room;
    unsigned long long left = length;
    while (left > 0u)
    {
        const int filled = (room->start == room->end) ? http_fill(connection, err_msg) : 1;
        if (filled <= 0)
        {
            if (filled == 0)
            {
                mmgr_line_put(err_msg, "the answer ended before its length");
            }
            return 0;
        }
        const unsigned long long buffered = room->end - room->start;
        const unsigned long long take = (left < buffered) ? left : buffered;
        const unsigned long long fits = CLI_REQUESTS_BODY - *size;
        const unsigned long long put = (take < fits) ? take : fits;
        if (!http_put(room, put, size, err_msg))
        {
            return 0;
        }
        *cut = *cut || (take > fits);
        room->start += take;
        left -= take;
    }
    return 1;
}

// the body read to the connection's end, cut once it passes CLI_REQUESTS_BODY: 1, or 0 with why given
static int http_to_end(HttpConnection *connection, unsigned long long *size, int *cut, VerbaLinea *err_msg)
{
    CliRequestsRoom *const room = connection->room;
    for (;;)
    {
        const unsigned long long buffered = room->end - room->start;
        const unsigned long long fits = CLI_REQUESTS_BODY - *size;
        const unsigned long long put = (buffered < fits) ? buffered : fits;
        if (!http_put(room, put, size, err_msg))
        {
            return 0;
        }
        room->start = room->end;
        if (buffered > fits)
        {
            *cut = 1;
            return 1;
        }
        const int filled = http_fill(connection, err_msg);
        if (filled <= 0)
        {
            return filled == 0;
        }
    }
}

// a chunked body read, its trailer passed over: 1, or 0 with why given
static int http_chunked(HttpConnection *connection, unsigned long long *size, int *cut, VerbaLinea *err_msg)
{
    const unsigned char *const reading = connection->room->reading;
    for (;;)
    {
        unsigned long long line = 0ull;
        unsigned long long length = 0ull;
        if (!http_line(connection, &line, &length, err_msg))
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
            mmgr_line_put(err_msg, "a chunk's size reads ");
            cli_json_quoted_lossy(err_msg, reading + line, length);
            return 0;
        }
        if (chunk == 0u)
        {
            do
            {
                if (!http_line(connection, &line, &length, err_msg))
                {
                    return 0;
                }
            } while (length > 0u);
            return 1;
        }
        if (!http_exactly(connection, chunk, size, cut, err_msg) || !http_line(connection, &line, &length, err_msg))
        {
            return 0;
        }
    }
}

// the head of the answer read, an answer of 1xx but 101 passed over, the wait for its first line timed from `step`:
// 1, or 0 with why given
static int http_head(HttpConnection *connection, unsigned long long step, VerbaLinea *err_msg)
{
    CliRequestsAnswer *const answer = &connection->room->answer;
    const unsigned char *const reading = connection->room->reading;
    do
    {
        unsigned long long line = 0ull;
        unsigned long long length = 0ull;
        if (!http_line(connection, &line, &length, err_msg))
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
            mmgr_line_put(err_msg, "the answer began ");
            cli_json_quoted_lossy(err_msg, reading + line, length);
            mmgr_line_put(err_msg, ", which is no status line");
            return 0;
        }
        // a status read is at most 65535
        answer->status = (unsigned int)status;
        answer->headers = 0u;
        answer->head_used = 0ull;
        const unsigned long long reason = (after != NULL) ? (unsigned long long)(after + 1 - text) : length;
        int read_ok = cli_requests_head_stored(answer, text, version, &answer->version) &&
                      cli_requests_head_stored(answer, text + reason, length - reason, &answer->reason);
        while (read_ok)
        {
            if (!http_line(connection, &line, &length, err_msg))
            {
                return 0;
            }
            if (length == 0u)
            {
                break;
            }
            read_ok = cli_requests_head_line(answer, (const char *)reading + line, length);
        }
        if (!read_ok)
        {
            mmgr_line_put(err_msg, "the answer's head is larger than orior_cli supports");
            return 0;
        }
    } while ((answer->status >= 100u) && (answer->status < 200u) && (answer->status != 101u));
    return 1;
}

// the request written whole, its head and then its body: 1, or 0 with why given
static int http_written(HttpConnection *connection, const char *host, VerbaLinea *err_msg)
{
    CliRequestsRoom *const room = connection->room;
    char *const head_cell = g_cli_cells->temporary_buf_alloc(CLI_REQUESTS_HEAD_ROOM, &g_cli_requests_cells);
    VerbaLinea head = mmgr_line_over(head_cell, CLI_REQUESTS_HEAD_ROOM);
    if (head.out == NULL)
    {
        mmgr_line_put(err_msg, "the request's head could not be stored");
        return 0;
    }
    cli_requests_head_written(&room->hop, &room->address, &head);
    int written = mmgr_line_ok(&head);
    if (!written)
    {
        mmgr_line_put(err_msg, "the request is larger than orior_cli supports");
    }
    else if (connection->tls != NULL)
    {
        written = cli_tls_written(connection->tls, head.out, head.at, err_msg) &&
                  cli_tls_written(connection->tls, room->hop.body, room->hop.body_size, err_msg);
    }
    else if (!cli_socket_written(&connection->socket, head.out, head.at) ||
             !cli_socket_written(&connection->socket, room->hop.body, room->hop.body_size))
    {
        mmgr_line_put(err_msg, host);
        mmgr_line_put(err_msg, ": the request could not be sent");
        written = 0;
    }
    g_cli_cells->persistent_buf_release(head.out, &g_cli_requests_cells);
    return written;
}

// the answer's body read as its head specifies it comes, its bytes as they came into `raw`, `*size` of them: 1, or 0
// with why given
static int http_body(HttpConnection *connection, unsigned long long *size, VerbaLinea *err_msg)
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
    connection->room->raw.at = 0ull;
    if (empty)
    {
    }
    else if ((encoding != NULL) && cli_requests_contains(answer->head + encoding->at, encoding->size, "chunked"))
    {
        read = http_chunked(connection, size, &cut, err_msg);
    }
    else if ((length != NULL) &&
             cli_requests_number(answer->head + length->at, length->size, 10u, 0xffffffffffffffffull, &count))
    {
        read = http_exactly(connection, count, size, &cut, err_msg);
    }
    else
    {
        read = http_to_end(connection, size, &cut, err_msg);
    }
    answer->cut = cut;
    // the body's cell is the cell it ended in: a line moves as it fills
    answer->body = (const unsigned char *)connection->room->raw.out;
    answer->size = *size;
    return read;
}

int cli_requests_once(CliRequestsRoom *room, int verify, VerbaLinea *err_msg)
{
    const unsigned long long began = cli_clock_microseconds();
    CliRequestsSent *const hop = &room->hop;
    CliRequestsAnswer *const answer = &room->answer;
    const int taken = cli_requests_address(hop->url, &room->address, err_msg);
    const char *const host = room->address.host;
    const int secure = room->address.secure;
    char purpose[CLI_REQUESTS_NAME + 16u];
    VerbaLinea purpose_line = {purpose, sizeof(purpose), 0ull};
    mmgr_line_put(&purpose_line, "a ");
    mmgr_line_put(&purpose_line, hop->method);
    mmgr_line_put(&purpose_line, " request");
    mmgr_line_finish(&purpose_line);
    if (!taken || !cli_net_reach(hop->url, purpose, err_msg))
    {
        return 0;
    }
    answer->status = 0u;
    answer->headers = 0u;
    answer->head_used = 0ull;
    room->raw.at = 0ull;
    answer->body = (const unsigned char *)room->raw.out;
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
                                    err_msg))
    {
        return 0;
    }
    answer->times.resolve = http_millis(resolved);
    answer->times.connect = http_millis(cli_clock_microseconds() - step - resolved);
    cli_socket_waited(&connection.socket, CLI_REQUESTS_ANSWERING);
    cli_socket_write_waited(&connection.socket, CLI_REQUESTS_ANSWERING);
    step = cli_clock_microseconds();
    connection.tls = secure ? cli_tls_connect(&connection.socket, host, verify, err_msg) : NULL;
    int done = !secure || (connection.tls != NULL);
    answer->times.tls = secure ? http_millis(cli_clock_microseconds() - step) : 0.0;
    step = cli_clock_microseconds();
    done = done && http_written(&connection, host, err_msg) && http_head(&connection, step, err_msg);
    step = cli_clock_microseconds();
    unsigned long long size = 0ull;
    done = done && http_body(&connection, &size, err_msg);
    answer->times.read = http_millis(cli_clock_microseconds() - step);
    if (connection.tls != NULL)
    {
        cli_tls_closed(connection.tls);
    }
    cli_socket_closed(&connection.socket);
    if (!done || (!answer->cut && !cli_requests_decoded(room, size, err_msg)))
    {
        return 0;
    }
    answer->binary = !cli_requests_is_text(answer);
    answer->times.total = http_millis(cli_clock_microseconds() - began);
    answer->at = cli_wall_milliseconds();
    memor.cpy(&(MemoriaCfg){.dst = answer->url, .src = hop->url, .bytes = sizeof(answer->url)});
    return 1;
}
