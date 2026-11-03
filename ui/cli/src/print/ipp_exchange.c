// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp_exchange.c: an ipp:// address read, and a message exchanged with a printer over HTTP/1.1, its answer read whole
// as the printer sends it: by its length, in chunks, or to the end
#include "../text/text.h"
#include "ipp.h"

// the most bytes of a printer's answer, its HTTP head and its body together; the bytes an answer and an encoded request
// are written in; and the capacity made for each read of the answer
#define CLI_IPP_ANSWER (1u << 22u)

#define CLI_IPP_REQUEST_CAPACITY (1ULL << 20u)

// the answer read, a cell its body is put together in, in place, under the lock while one exchange runs
static unsigned char *s_answer;

typedef struct
{
    const char *text;
    const char *prefix;
    const char *from;
    const char *to;
    unsigned int *port;
    unsigned long long length;
    const char *word_text;
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long at;
    unsigned long long *stop;
    const unsigned char *from_bytes;
    const unsigned char *to_bytes;
    unsigned long long *size_pointer;
    const CliSocket *socket;
    const char *host;
    unsigned int port_value;
    const char *path;
    const CliIppMessage *request;
    const unsigned char *document;
    unsigned long long read;
    int chunked;
    int has_length;
    VerbaLinea *err_msg;
    const char *uri;
    CliIppMessage *answer;
} IppExchangeCtx;
// the text `prefix` begins `text` with, where it does: what follows it, else NULL
EMBED_INLINE const char *cli_after(const IppExchangeCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->prefix, .cap = CLI_CELLS_POOL});
    return EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->text, .other_pointer = args->prefix,
                      .size = length)
               ? (args->text + length)
               : NULL;
}

// a port as the address writes it: 1, or 0 where it writes none
EMBED_INLINE int cli_port(const IppExchangeCtx *args)
{
    unsigned int *port = args->port;
    unsigned long long value = 0ULL;
    if (args->from == args->to)
    {
        return 0;
    }
    for (const char *at = args->from; at < args->to; at += 1)
    {
        if ((at[0] < '0') || (at[0] > '9'))
        {
            return 0;
        }
        value = (value * 10ULL) + (unsigned long long)(at[0] - '0');
        if (value > 0xffffULL)
        {
            return 0;
        }
    }
    // checked against the most 16 bits represent above
    *port = (unsigned int)value;
    return 1;
}

int cli_ipp_address(const CliIppErrMsgCfg *args)
{
    unsigned int *port = args->port;
    const char *rest = EMBED_CALL(cli_after, IppExchangeCtx, .text = args->uri, .prefix = "ipp://");
    if ((rest == NULL) && (EMBED_CALL(cli_after, IppExchangeCtx, .text = args->uri, .prefix = "ipps://") != NULL))
    {
        mmgr_line_put(args->err_msg, args->uri);
        mmgr_line_put(args->err_msg,
                      " takes only encrypted IPP, which orior_cli does not support; the printer's ipp:// address, "
                      "where it has one, reaches it");
        return 0;
    }
    rest = (rest != NULL) ? rest : EMBED_CALL(cli_after, IppExchangeCtx, .text = args->uri, .prefix = "http://");
    if (rest == NULL)
    {
        mmgr_line_put(args->err_msg, args->uri);
        mmgr_line_put(args->err_msg, " is no ipp:// address");
        return 0;
    }
    const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '/'});
    const char *const end =
        (slash != NULL) ? slash : (rest + cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL}));
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = args->path, .text = slash, .otherwise = "/");
    VerbaLinea named = {args->host, CLI_PATH_MAX_LEN, 0ULL};
    *port = 631u;
    if (rest[0] == '[')
    {
        const char *close = rest + 1;
        close += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = close, .at = 0ULL,
                            .size = (unsigned long long)(end - close), .set = "]");
        if (close == end)
        {
            mmgr_line_put(args->err_msg, args->uri);
            mmgr_line_put(args->err_msg, " has an address that does not close");
            return 0;
        }
        mmgr_line_put_n(&named, rest + 1, (unsigned long long)(close - rest - 1));
        mmgr_line_finish(&named);
        if (((close + 1) < end) && (close[1] == ':') &&
            !EMBED_CALL(cli_port, IppExchangeCtx, .from = close + 2, .to = end, .port = port))
        {
            *port = 631u;
        }
        return 1;
    }
    const char *colon = NULL;
    for (const char *at = rest; at < end; at += 1)
    {
        colon = (at[0] == ':') ? at : colon;
    }
    mmgr_line_put_n(&named, rest, (unsigned long long)(((colon != NULL) ? colon : end) - rest));
    mmgr_line_finish(&named);
    if ((colon != NULL) && !EMBED_CALL(cli_port, IppExchangeCtx, .from = colon + 1, .to = end, .port = port))
    {
        mmgr_line_put(args->err_msg, args->uri);
        mmgr_line_put(args->err_msg, " names no port");
        return 0;
    }
    return 1;
}

// whether `length` bytes at `text` are `word` in ASCII's either case
EMBED_INLINE int cli_same(const IppExchangeCtx *args)
{
    if (cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_CELLS_POOL}) != args->length)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        const char letter =
            ((args->text[at] >= 'A') && (args->text[at] <= 'Z')) ? (char)(args->text[at] + 32) : args->text[at];
        if (letter != args->word_text[at])
        {
            return 0;
        }
    }
    return 1;
}

// the line of the answer from `at`, its end into `stop` with its spaces after it left off: where the next starts, or
// the answer's size where it has no end
EMBED_INLINE unsigned long long cli_line(const IppExchangeCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long at = args->at;
    unsigned long long *stop = args->stop;
    unsigned long long end = at;
    end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)args->bytes,
                     .at = end, .size = size, .set = "\n");
    *stop = end;
    *stop = (at + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                             .text = (const char *)(args->bytes + at), .at = *stop - at, .set = "\r \t"));
    return (end < size) ? (end + 1ULL) : size;
}

// a number of hex digits from `from` to `to`, up to a `;`: 1, or 0 where it writes none
EMBED_INLINE int cli_chunk_size(const IppExchangeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    unsigned long long value = 0ULL;
    const unsigned char *at = args->from_bytes;
    at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = (const char *)at, .at = 0ULL,
                     .size = (unsigned long long)(args->to_bytes - at), .set = " \t");
    const unsigned char *const digits = at;
    for (; (at < args->to_bytes) && (at[0] != ';') && (at[0] != ' '); at += 1)
    {
        unsigned int digit;
        if ((at[0] >= '0') && (at[0] <= '9'))
        {
            digit = (unsigned int)(at[0] - '0');
        }
        else if ((at[0] >= 'a') && (at[0] <= 'f'))
        {
            digit = (unsigned int)(at[0] - 'a') + 10u;
        }
        else if ((at[0] >= 'A') && (at[0] <= 'F'))
        {
            digit = (unsigned int)(at[0] - 'A') + 10u;
        }
        else
        {
            digit = 16u;
        }
        if ((digit == 16u) || (value > (CLI_IPP_ANSWER >> 4u)))
        {
            return 0;
        }
        value = (value << 4u) | digit;
    }
    *size = value;
    return at != digits;
}

// The request sent: the HTTP head, the message, the document; 1, or 0 where the printer could not be written to
EMBED_INLINE int cli_send(const IppExchangeCtx *args)
{
    unsigned long long size = args->size;
    char *const body_cell = g_cli_cells->temporary_buf_alloc(CLI_IPP_REQUEST_CAPACITY, &g_cli_print_cells);
    VerbaLinea body = mmgr_line_over(body_cell, CLI_IPP_REQUEST_CAPACITY);
    if (body.out == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_ipp_message_out_verba_linea.encode, CliIppMessageOutVerbaLineaCfg, .message_read = args->request,
               .out_verba_linea_pointer = &body);
    if (!mmgr_line_ok(&body))
    {
        g_cli_cells->persistent_buf_release(body.out, &g_cli_print_cells);
        return 0;
    }
    char head[CLI_PATH_MAX_LEN * 3u];
    VerbaLinea line = {head, sizeof(head), 0ULL};
    mmgr_line_put(&line, "POST ");
    mmgr_line_put(&line, args->path);
    mmgr_line_put(&line, " HTTP/1.1\r\nHost: ");
    const int bracketed = cellul.chr(&(CatenaFinitaCfg){.src = args->host, .cap = CLI_CELLS_POOL, .byte = ':'}) != NULL;
    mmgr_line_put(&line, bracketed ? "[" : "");
    mmgr_line_put(&line, args->host);
    mmgr_line_put(&line, bracketed ? "]:" : ":");
    mmgr_line_decimal(&line, args->port_value, 1u);
    mmgr_line_put(&line, "\r\nContent-Type: application/ipp\r\nContent-Length: ");
    mmgr_line_decimal(&line, body.at + size, 1u);
    mmgr_line_put(&line, "\r\nConnection: close\r\nUser-Agent: orior\r\n\r\n");
    const int sent = mmgr_line_ok(&line) &&
                     EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = args->socket,
                                .bytes = head, .size = line.at) &&
                     EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = args->socket,
                                .bytes = body.out, .size = body.at) &&
                     ((size == 0ULL) || EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx,
                                                   .sock = args->socket, .bytes = args->document, .size = size));
    g_cli_cells->persistent_buf_release(body.out, &g_cli_print_cells);
    return sent;
}

// The answer's body put together in place from what was read past its head, from `at` on: by its length, in chunks,
// each moved down over the chunk sizes before it, or to the end; its size into `size`: 1, or 0 with why given
EMBED_INLINE int cli_body(const IppExchangeCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long *size = args->size_pointer;
    if (!args->chunked)
    {
        const unsigned long long reserved = args->has_length ? args->length : (args->read - at);
        if ((at + reserved) > args->read)
        {
            mmgr_line_put(args->err_msg, "the printer's answer is cut short");
            return 0;
        }
        *size = reserved;
        return 1;
    }
    const unsigned long long body = at;
    unsigned long long put = 0ULL;
    for (;;)
    {
        unsigned long long stop = 0ULL;
        const unsigned long long next =
            EMBED_CALL(cli_line, IppExchangeCtx, .bytes = s_answer, .size = args->read, .at = at, .stop = &stop);
        unsigned long long chunk = 0ULL;
        if (!EMBED_CALL(cli_chunk_size, IppExchangeCtx, .from_bytes = s_answer + at, .to_bytes = s_answer + stop,
                        .size_pointer = &chunk))
        {
            mmgr_line_put(args->err_msg, args->host);
            mmgr_line_put(args->err_msg, " sent a chunk orior_cli cannot read");
            return 0;
        }
        if (chunk == 0ULL)
        {
            break;
        }
        if ((next + chunk) > args->read)
        {
            mmgr_line_put(args->err_msg, "the printer's answer is cut short");
            return 0;
        }
        // a chunk's bytes follow its size's line: where they go is at or below where they are
        memor.move_down(&(MemoriaCfg){.dst = s_answer + body + put, .src = s_answer + next, .bytes = chunk});
        put += chunk;
        at = EMBED_CALL(cli_line, IppExchangeCtx, .bytes = s_answer, .size = args->read, .at = next + chunk,
                        .stop = &stop);
    }
    *size = put;
    return 1;
}

EMBED_INLINE int cli_exchange(const IppExchangeCtx *args)
{
    const char *uri = args->uri;
    unsigned long long size = args->size;
    VerbaLinea *err_msg = args->err_msg;
    char host[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    unsigned int port = 631u;
    if (!EMBED_CALL(cli_ipp_err_msg.address, CliIppErrMsgCfg, .uri = uri, .host = host, .port = &port, .path = path,
                    .err_msg = err_msg))
    {
        return 0;
    }
    CliSocket socket = {0ULL};
    if (!EMBED_CALL(cli_err_msg_internal.socket_connect, CliErrMsgCtx, .sock_out = &socket, .host = host, .port = port,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_sock_internal.socket_wait, CliSockCtx, .sock = &socket, .milliseconds = 30000u);
    if (!EMBED_CALL(cli_send, IppExchangeCtx, .socket = &socket, .host = host, .port_value = port, .path = path,
                    .request = args->request, .document = args->document, .size = size))
    {
        EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &socket);
        mmgr_line_put(err_msg, host);
        mmgr_line_put(err_msg, ": the job did not go");
        return 0;
    }
    unsigned long long read = 0ULL;
    char *const answer_cell = g_cli_cells->persistent_buf_alloc(CLI_IPP_ANSWER, &g_cli_print_cells);
    int read_state = answer_cell != NULL;
    while (read_state && (read < CLI_IPP_ANSWER))
    {
        const long long got = EMBED_CALL(cli_capacity_internal.socket_some, CliCapacityCtx, .sock = &socket,
                                         .bytes_out = answer_cell + read, .capacity = CLI_IPP_ANSWER - read);
        read_state = got > 0LL;
        // a count read is below the capacity requested
        read += read_state ? (unsigned long long)got : 0ULL;
    }
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &socket);
    s_answer = (unsigned char *)answer_cell;
    if (s_answer == NULL)
    {
        mmgr_line_put(err_msg, "a printer's answer could not be given a cell");
        return 0;
    }
    unsigned long long stop = 0ULL;
    unsigned long long at =
        EMBED_CALL(cli_line, IppExchangeCtx, .bytes = s_answer, .size = read, .at = 0ULL, .stop = &stop);
    // the status line's second word, the code
    unsigned long long word_text = 0ULL;
    word_text = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)s_answer,
                           .at = word_text, .size = stop, .set = " ");
    word_text = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = (const char *)s_answer,
                           .at = word_text, .size = stop, .set = " ");
    unsigned long long code = 0ULL;
    unsigned long long digits = 0ULL;
    unsigned long long end = word_text;
    code = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = (const char *)s_answer, .at = word_text,
                      .size = stop, .end = &end);
    digits = ((end - word_text) <= 5ULL) ? (end - word_text) : 0ULL;
    if ((digits == 0ULL) || (((word_text + digits) < stop) && (s_answer[word_text + digits] != ' ')))
    {
        mmgr_line_put(err_msg, host);
        mmgr_line_put(err_msg, " did not answer as a printer: ");
        mmgr_line_put_n(err_msg, (const char *)s_answer, stop);
        return 0;
    }
    unsigned long long length = 0ULL;
    int has_length = 0;
    int chunked = 0;
    for (;;)
    {
        const unsigned long long line_start = at;
        at = EMBED_CALL(cli_line, IppExchangeCtx, .bytes = s_answer, .size = read, .at = at, .stop = &stop);
        if ((stop == line_start) || (at == line_start))
        {
            break;
        }
        unsigned long long colon = line_start;
        colon = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)s_answer,
                           .at = colon, .size = stop, .set = ":");
        unsigned long long key_end = colon;
        key_end = (line_start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                           .text = (const char *)(s_answer + line_start), .at = key_end - line_start,
                                           .set = " "));
        unsigned long long value = colon + 1ULL;
        value = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = (const char *)s_answer,
                           .at = value, .size = stop, .set = " ");
        const char *const key = (const char *)(s_answer + line_start);
        if ((colon < stop) && EMBED_CALL(cli_same, IppExchangeCtx, .text = key, .length = key_end - line_start,
                                         .word_text = "content-length"))
        {
            length = 0ULL;
            has_length = value < stop;
            for (unsigned long long digit = value; digit < stop; digit += 1ULL)
            {
                has_length = has_length && (s_answer[digit] >= '0') && (s_answer[digit] <= '9');
                length = (length * 10ULL) + (unsigned long long)(s_answer[digit] - '0');
            }
        }
        else if ((colon < stop) &&
                 EMBED_CALL(cli_same, IppExchangeCtx, .text = key, .length = key_end - line_start,
                            .word_text = "transfer-encoding") &&
                 EMBED_CALL(cli_same, IppExchangeCtx, .text = (const char *)(s_answer + value), .length = stop - value,
                            .word_text = "chunked"))
        {
            chunked = 1;
        }
    }
    if (code == 401ULL)
    {
        mmgr_line_put(err_msg, host);
        mmgr_line_put(err_msg, " requires a user name: it takes jobs only from a user it knows");
        return 0;
    }
    if (code != 200ULL)
    {
        mmgr_line_put(err_msg, host);
        mmgr_line_put(err_msg, " answered HTTP ");
        mmgr_line_decimal(err_msg, code, 1u);
        return 0;
    }
    unsigned long long body = 0ULL;
    return EMBED_CALL(cli_body, IppExchangeCtx, .host = host, .at = at, .read = read, .chunked = chunked,
                      .has_length = has_length, .length = length, .size_pointer = &body, .err_msg = err_msg) &&
           EMBED_CALL(cli_ipp_message.decode, CliIppMessageCfg, .bytes = s_answer + at, .size = body,
                      .message = args->answer, .err_msg = err_msg);
}

int cli_ipp_exchange(const CliIppSizeCfg *args)
{
    const int done =
        EMBED_CALL(cli_exchange, IppExchangeCtx, .uri = args->uri, .request = args->request, .document = args->document,
                   .size = args->size, .answer = args->answer, .err_msg = args->err_msg);
    // the answer's own texts are in its bytes; a document after them lay in the answer read, which is given back
    args->answer->data = NULL;
    args->answer->data_length = 0ULL;
    g_cli_cells->persistent_buf_release(s_answer, &g_cli_print_cells);
    s_answer = NULL;
    return done;
}
