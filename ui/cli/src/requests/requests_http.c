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

// A connection: its socket, its TLS session where it is https, and the room its bytes are read into
typedef struct
{
    CliSocket socket;
    CliTls *tls;
    CliRequestsRoom *room;
} HttpConnection;
// how long a connection may take to open, and an answer to come, in milliseconds
#define CLI_REQUESTS_CONNECTING 15000u
#define CLI_REQUESTS_ANSWERING 60000u

// the longest line of a head, and the most read from the connection at once
#define CLI_REQUESTS_LINE (64u << 10u)
#define CLI_REQUESTS_CHUNK (16u << 10u)

typedef struct
{
    unsigned long long microseconds;
    HttpConnection *connection;
    VerbaLinea *err_msg;
    unsigned long long *line;
    unsigned long long *size;
    CliRequestsRoom *room;
    unsigned long long put;
    unsigned long long length;
    int *cut;
    unsigned long long step;
    const char *host;
} RequestsHttpCtx;
// microseconds as milliseconds to a tenth, as Rust's (secs * 10000).round() / 10 has them
EMBED_INLINE double http_millis(const RequestsHttpCtx *args)
{
    return (double)((args->microseconds + 50u) / 100u) / 10.0;
}

// more read from the connection after what is buffered: 1 where some came, 0 at its end, -1 with why given
EMBED_INLINE int http_fill(const RequestsHttpCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    CliRequestsRoom *const room = args->connection->room;
    if (room->start > 0u)
    {
        memor.move_down(&(MemoriaCfg){
            .dst = room->reading, .src = room->reading + room->start, .bytes = (size_t)(room->end - room->start)});
        room->end -= room->start;
        room->start = 0ull;
    }
    const long long read =
        (args->connection->tls != NULL)
            ? EMBED_CALL(cli_tls_out.read, CliTlsOutCfg, .tls = args->connection->tls, .out = room->reading + room->end,
                         .room = CLI_REQUESTS_CHUNK, .err_msg = err_msg)
            : EMBED_CALL(cli_room_internal.socket_some, CliRoomCtx, .sock = &args->connection->socket,
                         .bytes_out = room->reading + room->end, .room = CLI_REQUESTS_CHUNK);
    if (read == CLI_SOCKET_WAITED)
    {
        mmgr_line_put(err_msg, "no answer came within 60 s");
        return -1;
    }
    if ((read == CLI_SOCKET_FAILED) && (args->connection->tls == NULL))
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
EMBED_INLINE int http_line(const RequestsHttpCtx *args)
{
    unsigned long long *line = args->line;
    unsigned long long *size = args->size;
    CliRequestsRoom *const room = args->connection->room;
    unsigned long long looked = room->start;
    for (;;)
    {
        const unsigned long long ended =
            EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = (const char *)room->reading,
                       .at = looked, .size = room->end, .first = '\r', .second = '\n');
        looked = (ended < room->end) ? ended : ((room->end > (looked + 1u)) ? (room->end - 1u) : looked);
        if (looked + 1u < room->end)
        {
            *line = room->start;
            *size = looked - room->start;
            room->start = looked + 2u;
            return 1;
        }
        const unsigned long long seen = looked - room->start;
        const int filled = (room->end - room->start <= CLI_REQUESTS_LINE) ? http_fill(args) : 0;
        if (filled <= 0)
        {
            if (filled == 0)
            {
                mmgr_line_put(args->err_msg, "the answer ended in its headers");
            }
            return 0;
        }
        looked = room->start + seen;
    }
}

// `put` bytes of the body from the reading put after the `*size` in `raw`, and `*size` raised: 1, or 0 with why given
// where the line is full
EMBED_INLINE int http_put(const RequestsHttpCtx *args)
{
    unsigned long long *size = args->size;
    mmgr_line_put_n(&args->room->raw, (const char *)args->room->reading + args->room->start, args->put);
    if (!mmgr_line_ok(&args->room->raw))
    {
        mmgr_line_put(args->err_msg, "the answer's body could not be stored");
        return 0;
    }
    *size += args->put;
    return 1;
}

// `length` bytes of the body read, those that fit in CLI_REQUESTS_BODY put after the `*size` in `raw`, `*cut` set where
// any did not: 1, or 0 with why given
EMBED_INLINE int http_exactly(const RequestsHttpCtx *args)
{
    int *cut = args->cut;
    CliRequestsRoom *const room = args->connection->room;
    unsigned long long left = args->length;
    while (left > 0u)
    {
        const int filled = (room->start == room->end) ? http_fill(args) : 1;
        if (filled <= 0)
        {
            if (filled == 0)
            {
                mmgr_line_put(args->err_msg, "the answer ended before its length");
            }
            return 0;
        }
        const unsigned long long buffered = room->end - room->start;
        const unsigned long long take = (left < buffered) ? left : buffered;
        const unsigned long long fits = CLI_REQUESTS_BODY - *args->size;
        const unsigned long long put = (take < fits) ? take : fits;
        if (!EMBED_CALL(http_put, RequestsHttpCtx, .room = room, .put = put, .size = args->size,
                        .err_msg = args->err_msg))
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
EMBED_INLINE int http_to_end(const RequestsHttpCtx *args)
{
    int *cut = args->cut;
    CliRequestsRoom *const room = args->connection->room;
    for (;;)
    {
        const unsigned long long buffered = room->end - room->start;
        const unsigned long long fits = CLI_REQUESTS_BODY - *args->size;
        const unsigned long long put = (buffered < fits) ? buffered : fits;
        if (!EMBED_CALL(http_put, RequestsHttpCtx, .room = room, .put = put, .size = args->size,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        room->start = room->end;
        if (buffered > fits)
        {
            *cut = 1;
            return 1;
        }
        const int filled = http_fill(args);
        if (filled <= 0)
        {
            return filled == 0;
        }
    }
}

// a chunked body read, its trailer passed over: 1, or 0 with why given
EMBED_INLINE int http_chunk(const RequestsHttpCtx *args)
{
    unsigned long long *size = args->size;
    const unsigned char *const reading = args->connection->room->reading;
    for (;;)
    {
        unsigned long long line = 0ull;
        unsigned long long length = 0ull;
        if (!EMBED_CALL(http_line, RequestsHttpCtx, .connection = args->connection, .line = &line, .size = &length,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        const char *const text = (const char *)reading + line;
        unsigned long long start = 0ull;
        unsigned long long end = length;
        const char *const semicolon = memor.chr(&(MemoriaCfg){.src = text, .val = ';', .bytes = (size_t)length});
        end = (semicolon != NULL) ? (unsigned long long)(semicolon - text) : end;
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                   .end = &end);
        unsigned long long chunk = 0ull;
        if (!EMBED_CALL(cli_requests_text_size_base_internal.number, CliRequestsTextSizeBaseCtx, .text = text + start,
                        .size = end - start, .base = 16u, .most = 0xffffffffffffffffull, .value_u64s = &chunk))
        {
            mmgr_line_put(args->err_msg, "a chunk's size reads ");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                       .text_bytes = reading + line, .length = length);
            return 0;
        }
        if (chunk == 0u)
        {
            do
            {
                if (!EMBED_CALL(http_line, RequestsHttpCtx, .connection = args->connection, .line = &line,
                                .size = &length, .err_msg = args->err_msg))
                {
                    return 0;
                }
            } while (length > 0u);
            return 1;
        }
        if (!EMBED_CALL(http_exactly, RequestsHttpCtx, .connection = args->connection, .length = chunk, .size = size,
                        .cut = args->cut, .err_msg = args->err_msg) ||
            !EMBED_CALL(http_line, RequestsHttpCtx, .connection = args->connection, .line = &line, .size = &length,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
    }
}

// the head of the answer read, an answer of 1xx but 101 passed over, the wait for its first line timed from `step`:
// 1, or 0 with why given
EMBED_INLINE int http_head(const RequestsHttpCtx *args)
{
    CliRequestsAnswer *const answer = &args->connection->room->answer;
    const unsigned char *const reading = args->connection->room->reading;
    do
    {
        unsigned long long line = 0ull;
        unsigned long long length = 0ull;
        if (!EMBED_CALL(http_line, RequestsHttpCtx, .connection = args->connection, .line = &line, .size = &length,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        answer->times.wait =
            EMBED_CALL(http_millis, RequestsHttpCtx, .microseconds = cli_clock_microseconds() - args->step);
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
        if ((code == NULL) ||
            !EMBED_CALL(cli_requests_text_size_base_internal.number, CliRequestsTextSizeBaseCtx, .text = code,
                        .size = code_size, .base = 10u, .most = 65535u, .value_u64s = &status))
        {
            mmgr_line_put(args->err_msg, "the answer began ");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                       .text_bytes = reading + line, .length = length);
            mmgr_line_put(args->err_msg, ", which is no status line");
            return 0;
        }
        // a status read is at most 65535
        answer->status = (unsigned int)status;
        answer->headers = 0u;
        answer->head_used = 0ull;
        const unsigned long long reason = (after != NULL) ? (unsigned long long)(after + 1 - text) : length;
        int read_ok = EMBED_CALL(cli_requests_text_size_answer_internal.head_store, CliRequestsTextSizeAnswerCtx,
                                 .answer_out = answer, .text = text, .size = version, .text_span = &answer->version) &&
                      EMBED_CALL(cli_requests_text_size_answer_internal.head_store, CliRequestsTextSizeAnswerCtx,
                                 .answer_out = answer, .text = text + reason, .size = length - reason,
                                 .text_span = &answer->reason);
        while (read_ok)
        {
            if (!EMBED_CALL(http_line, RequestsHttpCtx, .connection = args->connection, .line = &line, .size = &length,
                            .err_msg = args->err_msg))
            {
                return 0;
            }
            if (length == 0u)
            {
                break;
            }
            read_ok = EMBED_CALL(cli_requests_size_answer_internal.head_line, CliRequestsSizeAnswerCtx,
                                 .answer_out = answer, .line = (const char *)reading + line, .size = length);
        }
        if (!read_ok)
        {
            mmgr_line_put(args->err_msg, "the answer's head is larger than orior_cli supports");
            return 0;
        }
    } while ((answer->status >= 100u) && (answer->status < 200u) && (answer->status != 101u));
    return 1;
}

// the request written whole, its head and then its body: 1, or 0 with why given
EMBED_INLINE int http_write(const RequestsHttpCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    CliRequestsRoom *const room = args->connection->room;
    char *const head_cell = g_cli_cells->temporary_buf_alloc(CLI_REQUESTS_HEAD_ROOM, &g_cli_requests_cells);
    VerbaLinea head = mmgr_line_over(head_cell, CLI_REQUESTS_HEAD_ROOM);
    if (head.out == NULL)
    {
        mmgr_line_put(err_msg, "the request's head could not be stored");
        return 0;
    }
    EMBED_CALL(cli_requests_out_hop_internal.head_write, CliRequestsOutHopCtx, .hop = &room->hop,
               .address_read = &room->address, .out = &head);
    int written = mmgr_line_ok(&head);
    if (!written)
    {
        mmgr_line_put(err_msg, "the request is larger than orior_cli supports");
    }
    else if (args->connection->tls != NULL)
    {
        written = EMBED_CALL(cli_tls_data.written, CliTlsDataCfg, .tls = args->connection->tls, .data = head.out,
                             .size = head.at, .err_msg = err_msg) &&
                  EMBED_CALL(cli_tls_data.written, CliTlsDataCfg, .tls = args->connection->tls, .data = room->hop.body,
                             .size = room->hop.body_size, .err_msg = err_msg);
    }
    else if (!EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->connection->socket,
                         .bytes = head.out, .size = head.at) ||
             !EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->connection->socket,
                         .bytes = room->hop.body, .size = room->hop.body_size))
    {
        mmgr_line_put(err_msg, args->host);
        mmgr_line_put(err_msg, ": the request could not be sent");
        written = 0;
    }
    // a request's headers may carry a secret, and the pool leaves a cell as it is
    mmgr_zero_buf(head.out, head.at);
    g_cli_cells->persistent_buf_release(head.out, &g_cli_requests_cells);
    return written;
}

// the answer's body read as its head specifies it comes, its bytes as they came into `raw`, `*size` of them: 1, or 0
// with why given
EMBED_INLINE int http_body(const RequestsHttpCtx *args)
{
    unsigned long long *size = args->size;
    CliRequestsAnswer *const answer = &args->connection->room->answer;
    const unsigned int status = answer->status;
    const int empty = (cellul.eq(&(CatenaFinitaCfg){
                          .src = args->connection->room->hop.method, .other = "HEAD", .cap = CLI_CELLS_POOL})) ||
                      (status == 204u) || (status == 304u) || ((status >= 100u) && (status < 200u));
    const CliRequestsText *const encoding =
        EMBED_CALL(cli_requests_name_answer_internal.header, CliRequestsNameAnswerCtx, .answer = answer,
                   .name = "Transfer-Encoding");
    const CliRequestsText *const length = EMBED_CALL(cli_requests_name_answer_internal.header, CliRequestsNameAnswerCtx,
                                                     .answer = answer, .name = "Content-Length");
    unsigned long long count = 0ull;
    int cut = 0;
    int read = 1;
    *size = 0ull;
    args->connection->room->raw.at = 0ull;
    if (empty)
    {
    }
    else if ((encoding != NULL) &&
             EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                        .text = answer->head + encoding->at, .size = encoding->size, .needle = "chunked"))
    {
        read = EMBED_CALL(http_chunk, RequestsHttpCtx, .connection = args->connection, .size = size, .cut = &cut,
                          .err_msg = args->err_msg);
    }
    else if ((length != NULL) && EMBED_CALL(cli_requests_text_size_base_internal.number, CliRequestsTextSizeBaseCtx,
                                            .text = answer->head + length->at, .size = length->size, .base = 10u,
                                            .most = 0xffffffffffffffffull, .value_u64s = &count))
    {
        read = EMBED_CALL(http_exactly, RequestsHttpCtx, .connection = args->connection, .length = count, .size = size,
                          .cut = &cut, .err_msg = args->err_msg);
    }
    else
    {
        read = EMBED_CALL(http_to_end, RequestsHttpCtx, .connection = args->connection, .size = size, .cut = &cut,
                          .err_msg = args->err_msg);
    }
    answer->cut = cut;
    // the body's cell is the cell it ended in: a line moves as it fills
    answer->body = (const unsigned char *)args->connection->room->raw.out;
    answer->size = *size;
    return read;
}

int cli_requests_once(const CliRequestsErrMsgRoomCtx *args)
{
    const unsigned long long began = cli_clock_microseconds();
    CliRequestsSent *const hop = &args->room->hop;
    CliRequestsAnswer *const answer = &args->room->answer;
    const int taken = EMBED_CALL(cli_requests_err_msg_url_internal.address, CliRequestsErrMsgUrlCtx, .url = hop->url,
                                 .address = &args->room->address, .err_msg = args->err_msg);
    const char *const host = args->room->address.host;
    const int secure = args->room->address.secure;
    char purpose[CLI_REQUESTS_NAME + 16u];
    VerbaLinea purpose_line = {purpose, sizeof(purpose), 0ull};
    mmgr_line_put(&purpose_line, "a ");
    mmgr_line_put(&purpose_line, hop->method);
    mmgr_line_put(&purpose_line, " request");
    mmgr_line_finish(&purpose_line);
    if (!taken || !EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = hop->url, .purpose = purpose,
                              .err_msg = args->err_msg))
    {
        return 0;
    }
    answer->status = 0u;
    answer->headers = 0u;
    answer->head_used = 0ull;
    args->room->raw.at = 0ull;
    answer->body = (const unsigned char *)args->room->raw.out;
    answer->size = 0ull;
    answer->cut = 0;
    answer->binary = 0;
    memor.set(&(MemoriaCfg){.dst = &answer->times, .val = 0, .bytes = sizeof(answer->times)});
    args->room->start = 0ull;
    args->room->end = 0ull;
    HttpConnection connection = {{0ull}, NULL, args->room};
    unsigned long long resolved = 0ull;
    unsigned long long step = cli_clock_microseconds();
    if (!EMBED_CALL(cli_err_msg_internal.socket_connected_time, CliErrMsgCtx, .sock_out = &connection.socket,
                    .host = host, .port = args->room->address.port, .milliseconds = CLI_REQUESTS_CONNECTING,
                    .resolved = &resolved, .err_msg = args->err_msg))
    {
        return 0;
    }
    answer->times.resolve = EMBED_CALL(http_millis, RequestsHttpCtx, .microseconds = resolved);
    answer->times.connect =
        EMBED_CALL(http_millis, RequestsHttpCtx, .microseconds = cli_clock_microseconds() - step - resolved);
    EMBED_CALL(cli_sock_internal.socket_wait, CliSockCtx, .sock = &connection.socket,
               .milliseconds = CLI_REQUESTS_ANSWERING);
    EMBED_CALL(cli_sock_internal.socket_write_wait, CliSockCtx, .sock = &connection.socket,
               .milliseconds = CLI_REQUESTS_ANSWERING);
    step = cli_clock_microseconds();
    connection.tls = secure ? EMBED_CALL(cli_tls_table.connect, CliTlsCfg, .socket = &connection.socket, .host = host,
                                         .verify = args->verify, .err_msg = args->err_msg)
                            : NULL;
    int done = !secure || (connection.tls != NULL);
    answer->times.tls =
        secure ? EMBED_CALL(http_millis, RequestsHttpCtx, .microseconds = cli_clock_microseconds() - step) : 0.0;
    step = cli_clock_microseconds();
    done = done &&
           EMBED_CALL(http_write, RequestsHttpCtx, .connection = &connection, .host = host, .err_msg = args->err_msg) &&
           EMBED_CALL(http_head, RequestsHttpCtx, .connection = &connection, .step = step, .err_msg = args->err_msg);
    step = cli_clock_microseconds();
    unsigned long long size = 0ull;
    done = done &&
           EMBED_CALL(http_body, RequestsHttpCtx, .connection = &connection, .size = &size, .err_msg = args->err_msg);
    answer->times.read = EMBED_CALL(http_millis, RequestsHttpCtx, .microseconds = cli_clock_microseconds() - step);
    if (connection.tls != NULL)
    {
        EMBED_CALL(cli_tls_out.close, CliTlsOutCfg, .tls = connection.tls);
    }
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &connection.socket);
    if (!done || (!answer->cut && !EMBED_CALL(cli_requests_size_err_msg_internal.decode, CliRequestsSizeErrMsgCtx,
                                              .room = args->room, .size = size, .err_msg = args->err_msg)))
    {
        return 0;
    }
    answer->binary = !EMBED_CALL(cli_requests_name_answer_internal.is_text, CliRequestsNameAnswerCtx, .answer = answer);
    answer->times.total = EMBED_CALL(http_millis, RequestsHttpCtx, .microseconds = cli_clock_microseconds() - began);
    answer->at = cli_wall_milliseconds();
    memor.cpy(&(MemoriaCfg){.dst = answer->url, .src = hop->url, .bytes = sizeof(answer->url)});
    return 1;
}
