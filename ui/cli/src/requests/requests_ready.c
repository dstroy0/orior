// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_ready.c: the request at a line of a file of requests made ready to go: its address, headers and body
// filled with the variables of the file over those of the environment chosen, an address with no scheme sent over
// http, and a body named by `< path` read from beside the file
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

static CliRequestsValues s_values;

// the reason a request did not fit what orior_cli supports
static void ready_full_err(VerbaLinea *err_msg)
{
    mmgr_line_put(err_msg, "the request is larger than orior_cli supports");
}

// a text of the parse filled into `out`, sized for `room`, its end closed: 1, or 0 with why given
static int ready_filled(const CliRequestsParsed *parsed, CliRequestsText text, char *out, unsigned long long room,
                        VerbaLinea *err_msg)
{
    VerbaLinea line = {out, room, 0ull};
    if (!cli_requests_filled(parsed->text + text.at, text.size, &s_values, &line, err_msg))
    {
        return 0;
    }
    if (mmgr_line_finish(&line) == 0ull)
    {
        ready_full_err(err_msg);
        return 0;
    }
    return 1;
}

// the file `name`, beside the file at `path`, read whole into `body`, a line: its size into `size`: 1, or 0 with why
// given
static int ready_body_file(const char *root, const char *path, const char *name, VerbaLinea *body,
                           unsigned long long *size, VerbaLinea *err_msg)
{
    char inside[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_inside(root, path, inside, err_msg))
    {
        return 0;
    }
    if (!cli_path_parent(inside, folder))
    {
        mmgr_line_put(err_msg, "the file has no folder");
        return 0;
    }
    char beside[CLI_PATH_MAX_LEN];
    VerbaLinea line = {beside, sizeof(beside), 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    unsigned long long length = 0ull;
    const unsigned char *const bytes =
        (mmgr_line_finish(&line) != 0ull) ? cli_file_taken(beside, &length, &g_cli_requests_cells) : NULL;
    const int whole = (bytes != NULL) && cli_utf8_whole((const char *)bytes, length);
    if (whole)
    {
        mmgr_line_put_n(body, (const char *)bytes, length);
    }
    const int fits = whole && mmgr_line_ok(body);
    if (fits)
    {
        *size = length;
    }
    cli_file_given(bytes, &g_cli_requests_cells);
    if (!whole)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, ": it could not be read");
    }
    else if (!fits)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is larger than orior_cli supports");
    }
    return fits;
}

// the request made ready, the requests' lock acquired and the file read: 1, or 0 with why given
static int ready_made(const char *root, const char *path, const CliRequestsParsed *parsed, const CliRequest *request,
                      const char *env, CliRequestsSent *sent, VerbaLinea *body, VerbaLinea *err_msg)
{
    if (!cli_requests_values(root, path, env, &s_values, err_msg))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < parsed->variables; at += 1u)
    {
        const CliRequestsText name = parsed->variable_name[at];
        const CliRequestsText value = parsed->variable_value[at];
        if (!cli_requests_value_put(&s_values, parsed->text + name.at, name.size, parsed->text + value.at, value.size))
        {
            ready_full_err(err_msg);
            return 0;
        }
    }
    if ((request->method.size >= sizeof(sent->method)) ||
        !ready_filled(parsed, request->url, sent->url, sizeof(sent->url), err_msg))
    {
        if (request->method.size >= sizeof(sent->method))
        {
            ready_full_err(err_msg);
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
            ready_full_err(err_msg);
            return 0;
        }
        memor.move_up(&(MemoriaCfg){.dst = sent->url + 7, .src = sent->url, .bytes = length + 1u});
        memor.cpy(&(MemoriaCfg){.dst = sent->url, .src = "http://", .bytes = 7u});
    }
    sent->headers = request->headers;
    for (unsigned int at = 0u; at < request->headers; at += 1u)
    {
        if (!ready_filled(parsed, request->header_name[at], sent->header_name[at], sizeof(sent->header_name[at]),
                          err_msg) ||
            !ready_filled(parsed, request->header_value[at], sent->header_value[at], sizeof(sent->header_value[at]),
                          err_msg))
        {
            return 0;
        }
    }
    sent->body_size = 0ull;
    sent->redirect = request->redirect;
    body->at = 0ull;
    int made = 1;
    if (request->has_body)
    {
        made = cli_requests_filled(parsed->text + request->body.at, request->body.size, &s_values, body, err_msg);
        if (made && !mmgr_line_ok(body))
        {
            ready_full_err(err_msg);
            made = 0;
        }
        sent->body_size = made ? body->at : 0ull;
    }
    else if (request->has_body_file)
    {
        made = ready_body_file(root, path, parsed->text + request->body_file.at, body, &sent->body_size, err_msg);
    }
    // the body's cell is the one it ended in: a line moves as it fills
    sent->body = body->out;
    return made;
}

int cli_requests_ready(const char *root, const char *path, const char *text, unsigned long long size,
                       unsigned long long line, const char *env, CliRequestsSent *sent, VerbaLinea *body,
                       VerbaLinea *key, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_requests_lock);
    const CliRequestsParsed *const parsed = &g_cli_requests_parsed;
    const int whole = cli_requests_parse(text, size, &g_cli_requests_parsed);
    const CliRequest *const request = whole ? cli_requests_at(parsed, line) : NULL;
    int ready = 0;
    if (!whole)
    {
        cli_requests_full_err(err_msg);
    }
    else if (request == NULL)
    {
        mmgr_line_put(err_msg, "the file contains no request");
    }
    else
    {
        ready = ready_made(root, path, parsed, request, env, sent, body, err_msg);
        cli_requests_values_given(&s_values);
        if (ready && (key != NULL))
        {
            cli_requests_key(parsed, request, key);
        }
    }
    // what is sent was copied out of the file read, so the file read is given back here
    cli_requests_parsed_given(&g_cli_requests_parsed);
    cli_lock_give(&g_cli_requests_lock);
    return ready;
}
