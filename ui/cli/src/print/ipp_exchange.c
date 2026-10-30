// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp_exchange.c: an ipp:// address read, and a message exchanged with a printer over HTTP/1.1, its answer read whole
// as the printer sends it: by its length, in chunks, or to the end
#include "../text/text.h"
#include "ipp.h"

// the most bytes of a printer's answer, its HTTP head and its body together; the bytes an answer and an encoded
// request are begun in before they grow; and the room made for each read of the answer
#define CLI_IPP_ANSWER (1u << 22u)

#define CLI_IPP_FIRST (16ull << 10u)
#define CLI_IPP_READ (16ull << 10u)

// the answer read, a growing cell its body is put together in, in place, under the lock while one exchange runs
static unsigned char *s_answer;
static CliLock s_lock = CLI_LOCK_INIT;

// the text `prefix` begins `text` with, where it does: what follows it, else NULL
static const char *cli_after(const char *text, const char *prefix)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
    return cli_text_same(text, prefix, length) ? (text + length) : NULL;
}

// a port as the address writes it: 1, or 0 where it writes none
static int cli_port(const char *from, const char *to, unsigned int *port)
{
    unsigned long long value = 0ull;
    if (from == to)
    {
        return 0;
    }
    for (const char *at = from; at < to; at += 1)
    {
        if ((at[0] < '0') || (at[0] > '9'))
        {
            return 0;
        }
        value = (value * 10ull) + (unsigned long long)(at[0] - '0');
        if (value > 0xffffull)
        {
            return 0;
        }
    }
    // checked against the most 16 bits represent above
    *port = (unsigned int)value;
    return 1;
}

int cli_ipp_address(const char *uri, char *host, unsigned int *port, char *path, VerbaLinea *err_msg)
{
    const char *rest = cli_after(uri, "ipp://");
    if ((rest == NULL) && (cli_after(uri, "ipps://") != NULL))
    {
        mmgr_line_put(err_msg, uri);
        mmgr_line_put(err_msg,
                      " takes only encrypted IPP, which orior_cli does not support; the printer's ipp:// address, "
                      "where it has one, reaches it");
        return 0;
    }
    rest = (rest != NULL) ? rest : cli_after(uri, "http://");
    if (rest == NULL)
    {
        mmgr_line_put(err_msg, uri);
        mmgr_line_put(err_msg, " is no ipp:// address");
        return 0;
    }
    const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '/'});
    const char *const end =
        (slash != NULL) ? slash : (rest + cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL}));
    memor.cpy(&(MemoriaCfg){
        .dst = path,
        .src = (slash != NULL) ? slash : "/",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (slash != NULL) ? slash : "/", .cap = CLI_CELLS_POOL}) + 1u});
    VerbaLinea named = {host, CLI_PATH_MAX_LEN, 0ull};
    *port = 631u;
    if (rest[0] == '[')
    {
        const char *close = rest + 1;
        while ((close < end) && (close[0] != ']'))
        {
            close += 1;
        }
        if (close == end)
        {
            mmgr_line_put(err_msg, uri);
            mmgr_line_put(err_msg, " has an address that does not close");
            return 0;
        }
        mmgr_line_put_n(&named, rest + 1, (unsigned long long)(close - rest - 1));
        mmgr_line_finish(&named);
        if (((close + 1) < end) && (close[1] == ':') && !cli_port(close + 2, end, port))
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
    if ((colon != NULL) && !cli_port(colon + 1, end, port))
    {
        mmgr_line_put(err_msg, uri);
        mmgr_line_put(err_msg, " names no port");
        return 0;
    }
    return 1;
}

// whether `length` bytes at `text` are `word` in ASCII's either case
static int cli_same(const char *text, unsigned long long length, const char *word)
{
    if (cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}) != length)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char letter = ((text[at] >= 'A') && (text[at] <= 'Z')) ? (char)(text[at] + 32) : text[at];
        if (letter != word[at])
        {
            return 0;
        }
    }
    return 1;
}

// the line of the answer from `at`, its end into `stop` with its spaces after it left off: where the next starts, or
// the answer's size where it has no end
static unsigned long long cli_line(const unsigned char *bytes, unsigned long long size, unsigned long long at,
                                   unsigned long long *stop)
{
    unsigned long long end = at;
    while ((end < size) && (bytes[end] != '\n'))
    {
        end += 1ull;
    }
    *stop = end;
    while ((*stop > at) && ((bytes[*stop - 1u] == '\r') || (bytes[*stop - 1u] == ' ') || (bytes[*stop - 1u] == '\t')))
    {
        *stop -= 1ull;
    }
    return (end < size) ? (end + 1ull) : size;
}

// a number of hex digits from `from` to `to`, up to a `;`: 1, or 0 where it writes none
static int cli_chunk_size(const unsigned char *from, const unsigned char *to, unsigned long long *size)
{
    unsigned long long value = 0ull;
    const unsigned char *at = from;
    while ((at < to) && ((at[0] == ' ') || (at[0] == '\t')))
    {
        at += 1;
    }
    const unsigned char *const digits = at;
    for (; (at < to) && (at[0] != ';') && (at[0] != ' '); at += 1)
    {
        const unsigned int digit = ((at[0] >= '0') && (at[0] <= '9'))   ? (unsigned int)(at[0] - '0')
                                   : ((at[0] >= 'a') && (at[0] <= 'f')) ? (unsigned int)(at[0] - 'a') + 10u
                                   : ((at[0] >= 'A') && (at[0] <= 'F')) ? (unsigned int)(at[0] - 'A') + 10u
                                                                        : 16u;
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
static int cli_sent(const CliSocket *socket, const char *host, unsigned int port, const char *path,
                    const CliIppMessage *request, const unsigned char *document, unsigned long long size)
{
    VerbaLinea body;
    if (!cli_growing_line_made(&body, CLI_IPP_FIRST, &g_cli_print_cells))
    {
        return 0;
    }
    cli_ipp_encode(request, &body);
    if (!mmgr_line_ok(&body))
    {
        cli_cell_release(body.out, &g_cli_print_cells);
        return 0;
    }
    char head[CLI_PATH_MAX_LEN * 3u];
    VerbaLinea line = {head, sizeof(head), 0ull};
    mmgr_line_put(&line, "POST ");
    mmgr_line_put(&line, path);
    mmgr_line_put(&line, " HTTP/1.1\r\nHost: ");
    const int bracketed = cellul.chr(&(CatenaFinitaCfg){.src = host, .cap = CLI_CELLS_POOL, .byte = ':'}) != NULL;
    mmgr_line_put(&line, bracketed ? "[" : "");
    mmgr_line_put(&line, host);
    mmgr_line_put(&line, bracketed ? "]:" : ":");
    mmgr_line_decimal(&line, port, 1u);
    mmgr_line_put(&line, "\r\nContent-Type: application/ipp\r\nContent-Length: ");
    mmgr_line_decimal(&line, body.at + size, 1u);
    mmgr_line_put(&line, "\r\nConnection: close\r\nUser-Agent: orior\r\n\r\n");
    const int sent = mmgr_line_ok(&line) && cli_socket_written(socket, head, line.at) &&
                     cli_socket_written(socket, body.out, body.at) &&
                     ((size == 0ull) || cli_socket_written(socket, document, size));
    cli_cell_release(body.out, &g_cli_print_cells);
    return sent;
}

// The answer's body put together in place from what was read past its head, from `at` on: by its length, in chunks,
// each moved down over the chunk sizes before it, or to the end; its size into `size`: 1, or 0 with why given
static int cli_body(const char *host, unsigned long long at, unsigned long long read, int chunked, int has_length,
                    unsigned long long length, unsigned long long *size, VerbaLinea *err_msg)
{
    if (!chunked)
    {
        const unsigned long long taken = has_length ? length : (read - at);
        if ((at + taken) > read)
        {
            mmgr_line_put(err_msg, "the printer's answer is cut short");
            return 0;
        }
        *size = taken;
        return 1;
    }
    const unsigned long long body = at;
    unsigned long long put = 0ull;
    for (;;)
    {
        unsigned long long stop = 0ull;
        const unsigned long long next = cli_line(s_answer, read, at, &stop);
        unsigned long long chunk = 0ull;
        if (!cli_chunk_size(s_answer + at, s_answer + stop, &chunk))
        {
            mmgr_line_put(err_msg, host);
            mmgr_line_put(err_msg, " sent a chunk orior_cli cannot read");
            return 0;
        }
        if (chunk == 0ull)
        {
            break;
        }
        if ((next + chunk) > read)
        {
            mmgr_line_put(err_msg, "the printer's answer is cut short");
            return 0;
        }
        // a chunk's bytes follow its size's line: where they go is at or below where they are
        memor.move_down(&(MemoriaCfg){.dst = s_answer + body + put, .src = s_answer + next, .bytes = chunk});
        put += chunk;
        at = cli_line(s_answer, read, next + chunk, &stop);
    }
    *size = put;
    return 1;
}

static int cli_exchanged(const char *uri, const CliIppMessage *request, const unsigned char *document,
                         unsigned long long size, CliIppMessage *answer, VerbaLinea *err_msg)
{
    char host[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    unsigned int port = 631u;
    if (!cli_ipp_address(uri, host, &port, path, err_msg))
    {
        return 0;
    }
    CliSocket socket = {0ull};
    if (!cli_socket_connected(&socket, host, port, err_msg))
    {
        return 0;
    }
    cli_socket_waited(&socket, 30000u);
    if (!cli_sent(&socket, host, port, path, request, document, size))
    {
        cli_socket_closed(&socket);
        mmgr_line_put(err_msg, host);
        mmgr_line_put(err_msg, ": the job did not go");
        return 0;
    }
    unsigned long long read = 0ull;
    VerbaLinea answer_line;
    int reading = cli_growing_line_made(&answer_line, CLI_IPP_FIRST, &g_cli_print_cells);
    while (reading && (read < CLI_IPP_ANSWER) && cli_growing_line_room(&answer_line, CLI_IPP_READ))
    {
        const unsigned long long room = answer_line.capacity - read - 1ull;
        const unsigned long long chunk = ((read + room) < CLI_IPP_ANSWER) ? room : (CLI_IPP_ANSWER - read);
        const long long got = cli_socket_some(&socket, answer_line.out + read, chunk);
        reading = got > 0ll;
        // a count read is below the room requested
        read += reading ? (unsigned long long)got : 0ull;
        answer_line.at = read;
    }
    cli_socket_closed(&socket);
    s_answer = (unsigned char *)answer_line.out;
    if (s_answer == NULL)
    {
        mmgr_line_put(err_msg, "a printer's answer could not be given a cell");
        return 0;
    }
    unsigned long long stop = 0ull;
    unsigned long long at = cli_line(s_answer, read, 0ull, &stop);
    // the status line's second word, the code
    unsigned long long word = 0ull;
    while ((word < stop) && (s_answer[word] != ' '))
    {
        word += 1ull;
    }
    while ((word < stop) && (s_answer[word] == ' '))
    {
        word += 1ull;
    }
    unsigned long long code = 0ull;
    unsigned long long digits = 0ull;
    while (((word + digits) < stop) && (s_answer[word + digits] >= '0') && (s_answer[word + digits] <= '9') &&
           (digits < 5ull))
    {
        code = (code * 10ull) + (unsigned long long)(s_answer[word + digits] - '0');
        digits += 1ull;
    }
    if ((digits == 0ull) || (((word + digits) < stop) && (s_answer[word + digits] != ' ')))
    {
        mmgr_line_put(err_msg, host);
        mmgr_line_put(err_msg, " did not answer as a printer: ");
        mmgr_line_put_n(err_msg, (const char *)s_answer, stop);
        return 0;
    }
    unsigned long long length = 0ull;
    int has_length = 0;
    int chunked = 0;
    for (;;)
    {
        const unsigned long long line_start = at;
        at = cli_line(s_answer, read, at, &stop);
        if ((stop == line_start) || (at == line_start))
        {
            break;
        }
        unsigned long long colon = line_start;
        while ((colon < stop) && (s_answer[colon] != ':'))
        {
            colon += 1ull;
        }
        unsigned long long key_end = colon;
        while ((key_end > line_start) && (s_answer[key_end - 1u] == ' '))
        {
            key_end -= 1ull;
        }
        unsigned long long value = colon + 1ull;
        while ((value < stop) && (s_answer[value] == ' '))
        {
            value += 1ull;
        }
        const char *const key = (const char *)(s_answer + line_start);
        if ((colon < stop) && cli_same(key, key_end - line_start, "content-length"))
        {
            length = 0ull;
            has_length = value < stop;
            for (unsigned long long digit = value; digit < stop; digit += 1ull)
            {
                has_length = has_length && (s_answer[digit] >= '0') && (s_answer[digit] <= '9');
                length = (length * 10ull) + (unsigned long long)(s_answer[digit] - '0');
            }
        }
        else if ((colon < stop) && cli_same(key, key_end - line_start, "transfer-encoding") &&
                 cli_same((const char *)(s_answer + value), stop - value, "chunked"))
        {
            chunked = 1;
        }
    }
    if (code == 401ull)
    {
        mmgr_line_put(err_msg, host);
        mmgr_line_put(err_msg, " requires a user name: it takes jobs only from a user it knows");
        return 0;
    }
    if (code != 200ull)
    {
        mmgr_line_put(err_msg, host);
        mmgr_line_put(err_msg, " answered HTTP ");
        mmgr_line_decimal(err_msg, code, 1u);
        return 0;
    }
    unsigned long long body = 0ull;
    return cli_body(host, at, read, chunked, has_length, length, &body, err_msg) &&
           cli_ipp_decode(s_answer + at, body, answer, err_msg);
}

int cli_ipp_exchange(const char *uri, const CliIppMessage *request, const unsigned char *document,
                     unsigned long long size, CliIppMessage *answer, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    const int done = cli_exchanged(uri, request, document, size, answer, err_msg);
    // the answer's own texts are in its bytes; a document after them lay in the answer read, which is given back
    answer->data = NULL;
    answer->data_length = 0ull;
    cli_cell_release(s_answer, &g_cli_print_cells);
    s_answer = NULL;
    cli_lock_give(&s_lock);
    return done;
}
