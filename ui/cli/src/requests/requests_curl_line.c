// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_curl_line.c: the curl line that sends the request at a line of a file, for bash and the shells that read
// quotes as it does, each part on a line of its own, written as a JSON string
#include "../text/json_tree.h"
#include "requests_internal.h"

// `size` bytes of `text` written inside the JSON string
static void line_put(CliLine *out, const char *text, unsigned long long size)
{
    cli_json_escaped_lossy(out, (const unsigned char *)text, size);
}

// `size` bytes of `text` written in single quotes, each quote in it closed, escaped and opened again
static void line_quoted(CliLine *out, const char *text, unsigned long long size)
{
    line_put(out, "'", 1u);
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if (text[at] == '\'')
        {
            line_put(out, text + from, at - from);
            line_put(out, "'\\''", 4u);
            from = at + 1u;
        }
    }
    line_put(out, text + from, size - from);
    line_put(out, "'", 1u);
}

// the parts between curl's and the next, each on a line of its own
static void line_joined(CliLine *out)
{
    line_put(out, " \\\n  ", 5u);
}

// the curl line that sends `sent`, written inside a JSON string
void cli_requests_curl_written(const CliRequestsSent *sent, CliLine *out)
{
    cli_line_character(out, '"');
    line_put(out, "curl", 4u);
    const char *const fallback = (sent->body_size == 0u) ? "GET" : "POST";
    if (cellul.eq(&(CatenaFinitaCfg){.src = sent->method, .other = "HEAD", .cap = CLI_CELLS_POOL}))
    {
        line_joined(out);
        line_put(out, "--head", 6u);
    }
    else if (!cellul.eq(&(CatenaFinitaCfg){.src = sent->method, .other = fallback, .cap = CLI_CELLS_POOL}))
    {
        line_joined(out);
        line_put(out, "-X ", 3u);
        line_put(out, sent->method, cellul.len(&(CatenaFinitaCfg){.src = sent->method, .cap = CLI_CELLS_POOL}));
    }
    line_joined(out);
    line_quoted(out, sent->url, cellul.len(&(CatenaFinitaCfg){.src = sent->url, .cap = CLI_CELLS_POOL}));
    for (unsigned int at = 0u; at < sent->headers; at += 1u)
    {
        char header[CLI_REQUESTS_NAME + CLI_REQUESTS_VALUE + 2u];
        CliLine header_line = {header, sizeof(header), 0ull};
        cli_line_text(&header_line, sent->header_name[at]);
        cli_line_text(&header_line, ": ");
        cli_line_text(&header_line, sent->header_value[at]);
        line_joined(out);
        line_put(out, "-H ", 3u);
        line_quoted(out, header, header_line.at);
    }
    if (sent->body_size > 0u)
    {
        line_joined(out);
        line_put(out, "--data-raw ", 11u);
        line_quoted(out, sent->body, sent->body_size);
    }
    if (sent->redirect)
    {
        line_joined(out);
        line_put(out, "-L", 2u);
    }
    cli_line_character(out, '"');
}

int cli_requests_curl(const char *root, const char *path, const char *text, unsigned long long size,
                      unsigned long long line, const char *env, CliLine *out, CliLine *said)
{
    CliRequestsRoom *const room = cli_requests_room_taken();
    if (room == NULL)
    {
        cli_line_text(said, "the request's room could not be taken");
        return 0;
    }
    const int ready = cli_requests_ready(root, path, text, size, line, env, &room->sent, &room->body, NULL, said);
    if (ready)
    {
        cli_requests_curl_written(&room->sent, out);
    }
    cli_requests_room_given(room);
    return ready;
}
