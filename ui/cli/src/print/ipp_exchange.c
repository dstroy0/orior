// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp_exchange.c: an ipp:// address read, and a message exchanged with a printer over HTTP/1.1, its answer read whole
// as the printer sends it: by its length, in chunks, or to the end
#include "../text/text.h"
#include "ipp.h"

// the most bytes of a printer's answer, its HTTP head and its body together, and of an encoded request
#define CLI_IPP_ANSWER (1u << 22u)

#define CLI_IPP_REQUEST (1u << 21u)

// the answer read, its body put together, and a request encoded, under the lock while one exchange runs
static unsigned char s_answer[CLI_IPP_ANSWER];
static unsigned char s_body[CLI_IPP_ANSWER];
static char s_request[CLI_IPP_REQUEST];
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
    // checked against the most 16 bits hold above
    *port = (unsigned int)value;
    return 1;
}

int cli_ipp_address(const char *uri, char *host, unsigned int *port, char *path, CliLine *said)
{
    const char *rest = cli_after(uri, "ipp://");
    if ((rest == NULL) && (cli_after(uri, "ipps://") != NULL))
    {
        cli_line_text(said, uri);
        cli_line_text(said, " takes only encrypted IPP, which orior does not speak; the printer's ipp:// address, "
                            "where it has one, reaches it");
        return 0;
    }
    rest = (rest != NULL) ? rest : cli_after(uri, "http://");
    if (rest == NULL)
    {
        cli_line_text(said, uri);
        cli_line_text(said, " is no ipp:// address");
        return 0;
    }
    const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '/'});
    const char *const end =
        (slash != NULL) ? slash : (rest + cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL}));
    memor.cpy(&(MemoriaCfg){
        .dst = path,
        .src = (slash != NULL) ? slash : "/",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (slash != NULL) ? slash : "/", .cap = CLI_CELLS_POOL}) + 1u});
    CliLine named = {host, CLI_PATH_LONGEST, 0ull};
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
            cli_line_text(said, uri);
            cli_line_text(said, " has an address that does not close");
            return 0;
        }
        cli_line_counted(&named, rest + 1, (unsigned long long)(close - rest - 1));
        cli_line_closed(&named);
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
    cli_line_counted(&named, rest, (unsigned long long)(((colon != NULL) ? colon : end) - rest));
    cli_line_closed(&named);
    if ((colon != NULL) && !cli_port(colon + 1, end, port))
    {
        cli_line_text(said, uri);
        cli_line_text(said, " names no port");
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
    CliLine body = {s_request, sizeof(s_request), 0ull};
    cli_ipp_encode(request, &body);
    if (!cli_line_fits(&body))
    {
        return 0;
    }
    char head[CLI_PATH_LONGEST * 3u];
    CliLine line = {head, sizeof(head), 0ull};
    cli_line_text(&line, "POST ");
    cli_line_text(&line, path);
    cli_line_text(&line, " HTTP/1.1\r\nHost: ");
    const int bracketed = cellul.chr(&(CatenaFinitaCfg){.src = host, .cap = CLI_CELLS_POOL, .byte = ':'}) != NULL;
    cli_line_text(&line, bracketed ? "[" : "");
    cli_line_text(&line, host);
    cli_line_text(&line, bracketed ? "]:" : ":");
    cli_line_decimal(&line, port, 1u);
    cli_line_text(&line, "\r\nContent-Type: application/ipp\r\nContent-Length: ");
    cli_line_decimal(&line, body.at + size, 1u);
    cli_line_text(&line, "\r\nConnection: close\r\nUser-Agent: orior\r\n\r\n");
    return cli_line_fits(&line) && cli_socket_written(socket, head, line.at) &&
           cli_socket_written(socket, s_request, body.at) &&
           ((size == 0ull) || cli_socket_written(socket, document, size));
}

// The answer's body put together from what was read past its head: by its length, in chunks, or to the end; its size
// into `size`: 1, or 0 with why said
static int cli_body(const char *host, unsigned long long at, unsigned long long read, int chunked, int has_length,
                    unsigned long long length, unsigned long long *size, CliLine *said)
{
    if (!chunked)
    {
        const unsigned long long taken = has_length ? length : (read - at);
        if ((at + taken) > read)
        {
            cli_line_text(said, "the printer's answer is cut short");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_body, .src = s_answer + at, .bytes = taken});
        *size = taken;
        return 1;
    }
    unsigned long long put = 0ull;
    for (;;)
    {
        unsigned long long stop = 0ull;
        const unsigned long long next = cli_line(s_answer, read, at, &stop);
        unsigned long long chunk = 0ull;
        if (!cli_chunk_size(s_answer + at, s_answer + stop, &chunk))
        {
            cli_line_text(said, host);
            cli_line_text(said, " sent a chunk orior cannot read");
            return 0;
        }
        if (chunk == 0ull)
        {
            break;
        }
        if (((next + chunk) > read) || ((put + chunk) > sizeof(s_body)))
        {
            cli_line_text(said, "the printer's answer is cut short");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_body + put, .src = s_answer + next, .bytes = chunk});
        put += chunk;
        at = cli_line(s_answer, read, next + chunk, &stop);
    }
    *size = put;
    return 1;
}

static int cli_exchanged(const char *uri, const CliIppMessage *request, const unsigned char *document,
                         unsigned long long size, CliIppMessage *answer, CliLine *said)
{
    char host[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    unsigned int port = 631u;
    if (!cli_ipp_address(uri, host, &port, path, said))
    {
        return 0;
    }
    CliSocket socket = {0ull};
    if (!cli_socket_connected(&socket, host, port, said))
    {
        return 0;
    }
    cli_socket_waited(&socket, 30000u);
    if (!cli_sent(&socket, host, port, path, request, document, size))
    {
        cli_socket_closed(&socket);
        cli_line_text(said, host);
        cli_line_text(said, ": the job did not go");
        return 0;
    }
    unsigned long long read = 0ull;
    for (;;)
    {
        const long long got = cli_socket_some(&socket, s_answer + read, sizeof(s_answer) - read);
        if (got <= 0ll)
        {
            break;
        }
        // a count read is below the room asked
        read += (unsigned long long)got;
    }
    cli_socket_closed(&socket);
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
        cli_line_text(said, host);
        cli_line_text(said, " did not answer as a printer: ");
        cli_line_counted(said, (const char *)s_answer, stop);
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
        cli_line_text(said, host);
        cli_line_text(said, " asks who prints: it takes jobs only from a user it knows");
        return 0;
    }
    if (code != 200ull)
    {
        cli_line_text(said, host);
        cli_line_text(said, " answered HTTP ");
        cli_line_decimal(said, code, 1u);
        return 0;
    }
    unsigned long long body = 0ull;
    return cli_body(host, at, read, chunked, has_length, length, &body, said) &&
           cli_ipp_decode(s_body, body, answer, said);
}

int cli_ipp_exchange(const char *uri, const CliIppMessage *request, const unsigned char *document,
                     unsigned long long size, CliIppMessage *answer, CliLine *said)
{
    cli_lock_take(&s_lock);
    const int done = cli_exchanged(uri, request, document, size, answer, said);
    cli_lock_give(&s_lock);
    return done;
}
