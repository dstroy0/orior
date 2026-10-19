// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_answer.c: an answer written as serde writes it, with what was sent: its body as text, read as Rust's
// from_utf8_lossy reads it, or as base64 where it is not text, cut where it is longer than the writer is given
#include "../text/base64.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

// the bytes of a UTF-8 letter
static unsigned long long answer_letter_size(unsigned int letter)
{
    return (letter < 0x80u) ? 1u : (letter < 0x800u) ? 2u : (letter < 0x10000u) ? 3u : 4u;
}

// the body written as a JSON string, its text cut to `most` bytes at a letter's start: 1 where it was cut
static int answer_body(const CliRequestsAnswer *answer, unsigned long long most, CliLine *out)
{
    if (answer->binary)
    {
        const unsigned long long whole = ((answer->size + 2u) / 3u) * 4u;
        const unsigned long long letters = (whole < most) ? whole : most;
        const unsigned long long groups = letters / 4u;
        const unsigned long long rest = letters % 4u;
        cli_line_character(out, '"');
        cli_base64(out, answer->body, groups * 3u);
        if (rest > 0u)
        {
            char last[8];
            CliLine last_line = {last, sizeof(last), 0ull};
            const unsigned long long left = answer->size - (groups * 3u);
            cli_base64(&last_line, answer->body + (groups * 3u), (left < 3u) ? left : 3u);
            cli_line_counted(out, last, rest);
        }
        cli_line_character(out, '"');
        return letters < whole;
    }
    unsigned long long bytes = 0ull;
    unsigned long long text = 0ull;
    while (bytes < answer->size)
    {
        unsigned int letter = 0u;
        const unsigned long long taken = cli_lossy_letter(answer->body, answer->size, bytes, &letter);
        if (text + answer_letter_size(letter) > most)
        {
            break;
        }
        text += answer_letter_size(letter);
        bytes += taken;
    }
    cli_json_quoted_lossy(out, answer->body, bytes);
    return bytes < answer->size;
}

// pairs of texts written as a JSON array of arrays of two strings: the request's own, or the answer's, kept in its head
static void answer_sent_headers(const CliRequestsSent *sent, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < sent->headers; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? ",[" : "[");
        cli_json_quoted(out, sent->header_name[at],
                        cellul.len(&(CatenaFinitaCfg){.src = sent->header_name[at], .cap = CLI_CELLS_POOL}));
        cli_line_character(out, ',');
        cli_json_quoted(out, sent->header_value[at],
                        cellul.len(&(CatenaFinitaCfg){.src = sent->header_value[at], .cap = CLI_CELLS_POOL}));
        cli_line_character(out, ']');
    }
    cli_line_character(out, ']');
}

static void answer_headers(const CliRequestsAnswer *answer, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < answer->headers; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? ",[" : "[");
        const CliRequestsText name = answer->header_name[at];
        const CliRequestsText value = answer->header_value[at];
        cli_json_quoted_lossy(out, (const unsigned char *)answer->head + name.at, name.size);
        cli_line_character(out, ',');
        cli_json_quoted_lossy(out, (const unsigned char *)answer->head + value.at, value.size);
        cli_line_character(out, ']');
    }
    cli_line_character(out, ']');
}

// how long each part took, written as serde writes the times
static void answer_times(const CliRequestsTimes *times, CliLine *out)
{
    static const char *const names[6] = {
        "{\"resolve\":", ",\"connect\":", ",\"tls\":", ",\"wait\":", ",\"read\":", ",\"total\":"};
    const double values[6] = {times->resolve, times->connect, times->tls, times->wait, times->read, times->total};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        cli_line_text(out, names[at]);
        cli_json_double(out, values[at]);
    }
    cli_line_character(out, '}');
}

void cli_requests_answer_written(const CliRequestsAnswer *answer, const CliRequestsSent *sent, unsigned long long most,
                                 CliLine *out)
{
    cli_line_text(out, "{\"at\":");
    cli_line_decimal(out, answer->at, 1u);
    cli_line_text(out, ",\"sent\":{\"method\":");
    cli_json_quoted(out, sent->method, cellul.len(&(CatenaFinitaCfg){.src = sent->method, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"url\":");
    cli_json_quoted(out, sent->url, cellul.len(&(CatenaFinitaCfg){.src = sent->url, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"headers\":");
    answer_sent_headers(sent, out);
    cli_line_text(out, ",\"body\":");
    cli_json_quoted(out, sent->body, sent->body_size);
    cli_line_text(out, sent->redirect ? ",\"redirect\":true}" : ",\"redirect\":false}");
    cli_line_text(out, ",\"url\":");
    cli_json_quoted(out, answer->url, cellul.len(&(CatenaFinitaCfg){.src = answer->url, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"status\":");
    cli_line_decimal(out, answer->status, 1u);
    cli_line_text(out, ",\"reason\":");
    cli_json_quoted_lossy(out, (const unsigned char *)answer->head + answer->reason.at, answer->reason.size);
    cli_line_text(out, ",\"version\":");
    cli_json_quoted_lossy(out, (const unsigned char *)answer->head + answer->version.at, answer->version.size);
    cli_line_text(out, ",\"headers\":");
    answer_headers(answer, out);
    cli_line_text(out, ",\"body\":");
    const int cut = answer_body(answer, most, out);
    cli_line_text(out, answer->binary ? ",\"binary\":true,\"size\":" : ",\"binary\":false,\"size\":");
    cli_line_decimal(out, answer->size, 1u);
    cli_line_text(out, (answer->cut || cut) ? ",\"cut\":true,\"times\":" : ",\"cut\":false,\"times\":");
    answer_times(&answer->times, out);
    cli_line_text(out, ",\"redirects\":[");
    for (unsigned int at = 0u; at < answer->redirects; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? ",[" : "[");
        cli_line_decimal(out, answer->redirect_status[at], 1u);
        cli_line_character(out, ',');
        cli_json_quoted(out, answer->redirect_url[at],
                        cellul.len(&(CatenaFinitaCfg){.src = answer->redirect_url[at], .cap = CLI_CELLS_POOL}));
        cli_line_character(out, ']');
    }
    cli_line_text(out, "],\"failed\":");
    if (answer->has_failed)
    {
        cli_json_quoted(out, answer->failed,
                        cellul.len(&(CatenaFinitaCfg){.src = answer->failed, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_character(out, '}');
}
