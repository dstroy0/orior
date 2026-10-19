// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_ready.c: the request at a line of a file of requests made ready to go: its address, headers and body
// filled with the variables of the file over those of the environment chosen, an address with no scheme sent over
// http, and a body named by `< path` read from beside the file
#include "../text/json_tree.h"
#include "requests_internal.h"

static CliRequestsValues s_values;

// the reason a request did not fit what orior holds
static void ready_full_said(CliLine *said)
{
    cli_line_text(said, "the request is larger than orior holds");
}

// a text of the parse filled into `out`, which holds `room`, its end closed: 1, or 0 with why said
static int ready_filled(const CliRequestsParsed *parsed, CliRequestsText text, char *out, unsigned long long room,
                        CliLine *said)
{
    CliLine line = {out, room, 0ull};
    if (!cli_requests_filled(parsed->text + text.at, text.size, &s_values, &line, said))
    {
        return 0;
    }
    if (cli_line_closed(&line) == 0ull)
    {
        ready_full_said(said);
        return 0;
    }
    return 1;
}

// the file `name`, beside the file at `path`, read whole into `body`, which holds `room`: its size into `size`: 1, or
// 0 with why said
static int ready_body_file(const char *root, const char *path, const char *name, char *body, unsigned long long room,
                           unsigned long long *size, CliLine *said)
{
    char inside[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    if (!cli_inside(root, path, inside, said))
    {
        return 0;
    }
    if (!cli_path_parent(inside, folder))
    {
        cli_line_text(said, "the file has no folder");
        return 0;
    }
    char beside[CLI_PATH_LONGEST];
    CliLine line = {beside, sizeof(beside), 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    unsigned long long length = 0ull;
    const unsigned char *const bytes =
        (cli_line_closed(&line) != 0ull) ? qry_member_file_mapped(beside, &length) : NULL;
    const int whole = (bytes != NULL) && cli_utf8_whole((const char *)bytes, length);
    const int fits = whole && (length <= room);
    if (fits)
    {
        memor.cpy(&(MemoriaCfg){.dst = body, .src = bytes, .bytes = (size_t)length});
        *size = length;
    }
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, length);
    }
    if (!whole)
    {
        cli_line_text(said, name);
        cli_line_text(said, ": it could not be read");
    }
    else if (!fits)
    {
        cli_line_text(said, name);
        cli_line_text(said, " is larger than orior holds");
    }
    return fits;
}

// the request made ready, the requests' lock held and the file read: 1, or 0 with why said
static int ready_made(const char *root, const char *path, const CliRequestsParsed *parsed, const CliRequest *request,
                      const char *env, CliRequestsSent *sent, char *body, unsigned long long room, CliLine *said)
{
    if (!cli_requests_values(root, path, env, &s_values, said))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < parsed->variables; at += 1u)
    {
        const CliRequestsText name = parsed->variable_name[at];
        const CliRequestsText value = parsed->variable_value[at];
        if (!cli_requests_value_put(&s_values, parsed->text + name.at, name.size, parsed->text + value.at, value.size))
        {
            ready_full_said(said);
            return 0;
        }
    }
    if ((request->method.size >= sizeof(sent->method)) ||
        !ready_filled(parsed, request->url, sent->url, sizeof(sent->url), said))
    {
        if (request->method.size >= sizeof(sent->method))
        {
            ready_full_said(said);
        }
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = sent->method, .src = parsed->text + request->method.at, .bytes = (size_t)request->method.size + 1u});
    if (cellul.find(&(CatenaFinitaCfg){
            .src = sent->url, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) == NULL)
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = sent->url, .cap = CLI_CELLS_POOL});
        if (length + 8u > sizeof(sent->url))
        {
            ready_full_said(said);
            return 0;
        }
        memor.move_up(&(MemoriaCfg){.dst = sent->url + 7, .src = sent->url, .bytes = length + 1u});
        memor.cpy(&(MemoriaCfg){.dst = sent->url, .src = "http://", .bytes = 7u});
    }
    sent->headers = request->headers;
    for (unsigned int at = 0u; at < request->headers; at += 1u)
    {
        if (!ready_filled(parsed, request->header_name[at], sent->header_name[at], sizeof(sent->header_name[at]),
                          said) ||
            !ready_filled(parsed, request->header_value[at], sent->header_value[at], sizeof(sent->header_value[at]),
                          said))
        {
            return 0;
        }
    }
    sent->body = body;
    sent->body_size = 0ull;
    sent->redirect = request->redirect;
    if (request->has_body)
    {
        CliLine line = {body, room, 0ull};
        if (!cli_requests_filled(parsed->text + request->body.at, request->body.size, &s_values, &line, said))
        {
            return 0;
        }
        if (!cli_line_fits(&line))
        {
            ready_full_said(said);
            return 0;
        }
        sent->body_size = line.at;
        return 1;
    }
    if (request->has_body_file)
    {
        return ready_body_file(root, path, parsed->text + request->body_file.at, body, room, &sent->body_size, said);
    }
    return 1;
}

int cli_requests_ready(const char *root, const char *path, const char *text, unsigned long long size,
                       unsigned long long line, const char *env, CliRequestsSent *sent, char *body,
                       unsigned long long room, CliLine *key, CliLine *said)
{
    cli_lock_take(&g_cli_requests_lock);
    const CliRequestsParsed *const parsed = &g_cli_requests_parsed;
    const int whole = cli_requests_parse(text, size, &g_cli_requests_parsed);
    const CliRequest *const request = whole ? cli_requests_at(parsed, line) : NULL;
    int ready = 0;
    if (!whole)
    {
        cli_requests_full_said(said);
    }
    else if (request == NULL)
    {
        cli_line_text(said, "the file holds no request");
    }
    else
    {
        ready = ready_made(root, path, parsed, request, env, sent, body, room, said);
        if (ready && (key != NULL))
        {
            cli_requests_key(parsed, request, key);
        }
    }
    cli_lock_give(&g_cli_requests_lock);
    return ready;
}
