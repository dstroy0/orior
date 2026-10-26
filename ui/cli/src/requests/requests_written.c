// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_written.c: a file of requests read and written as serde writes its requests and variables, and the key
// its answers are stored under
#include "../text/json_tree.h"
#include "requests_internal.h"

// a text of the pool written as a JSON string
static void parse_quoted(const CliRequestsParsed *parsed, CliRequestsText text, VerbaLinea *out)
{
    cli_json_quoted(out, parsed->text + text.at, text.size);
}

// pairs of the pool written as a JSON array of arrays of two strings
static void parse_pairs(const CliRequestsParsed *parsed, const CliRequestsText *names, const CliRequestsText *values,
                        unsigned int count, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        mmgr_line_put(out, (index > 0u) ? ",[" : "[");
        parse_quoted(parsed, names[index], out);
        mmgr_line_ch(out, ',');
        parse_quoted(parsed, values[index], out);
        mmgr_line_ch(out, ']');
    }
    mmgr_line_ch(out, ']');
}

// a request written as serde writes its struct
static void parse_request_written(const CliRequestsParsed *parsed, const CliRequest *request, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"name\":");
    parse_quoted(parsed, request->name, out);
    mmgr_line_put(out, ",\"start\":");
    mmgr_line_decimal(out, request->start, 1u);
    mmgr_line_put(out, ",\"end\":");
    mmgr_line_decimal(out, request->end, 1u);
    mmgr_line_put(out, ",\"line\":");
    mmgr_line_decimal(out, request->line, 1u);
    mmgr_line_put(out, ",\"method\":");
    parse_quoted(parsed, request->method, out);
    mmgr_line_put(out, ",\"url\":");
    parse_quoted(parsed, request->url, out);
    mmgr_line_put(out, ",\"headers\":");
    parse_pairs(parsed, request->header_name, request->header_value, request->headers, out);
    mmgr_line_put(out, ",\"body\":");
    if (request->has_body)
    {
        parse_quoted(parsed, request->body, out);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"body_file\":");
    if (request->has_body_file)
    {
        parse_quoted(parsed, request->body_file, out);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, request->redirect ? ",\"redirect\":true}" : ",\"redirect\":false}");
}

// the reason a file of requests was not read whole
void cli_requests_full_err(VerbaLinea *err_msg)
{
    mmgr_line_put(err_msg, "the file of requests is larger than orior_cli supports");
}

int cli_requests_parsed(const char *text, unsigned long long size, VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_requests_lock);
    const CliRequestsParsed *const parsed = &g_cli_requests_parsed;
    const int whole = cli_requests_parse(text, size, &g_cli_requests_parsed);
    if (whole)
    {
        mmgr_line_put(out, "{\"requests\":[");
        for (unsigned int index = 0u; index < parsed->count; index += 1u)
        {
            mmgr_line_put(out, (index > 0u) ? "," : "");
            parse_request_written(parsed, &parsed->requests[index], out);
        }
        mmgr_line_put(out, "],\"variables\":");
        parse_pairs(parsed, parsed->variable_name, parsed->variable_value, parsed->variables, out);
        mmgr_line_ch(out, '}');
    }
    cli_requests_parsed_given(&g_cli_requests_parsed);
    cli_lock_give(&g_cli_requests_lock);
    if (!whole)
    {
        cli_requests_full_err(err_msg);
    }
    return whole;
}

void cli_requests_key(const CliRequestsParsed *parsed, const CliRequest *request, VerbaLinea *out)
{
    if (request->name.size > 0u)
    {
        mmgr_line_put_n(out, parsed->text + request->name.at, request->name.size);
        return;
    }
    mmgr_line_put_n(out, parsed->text + request->method.at, request->method.size);
    mmgr_line_ch(out, ' ');
    mmgr_line_put_n(out, parsed->text + request->url.at, request->url.size);
}

int cli_requests_key_at(const char *text, unsigned long long size, unsigned long long line, VerbaLinea *out,
                        VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_requests_lock);
    const int whole = cli_requests_parse(text, size, &g_cli_requests_parsed);
    const CliRequest *const request = whole ? cli_requests_at(&g_cli_requests_parsed, line) : NULL;
    if (request != NULL)
    {
        cli_requests_key(&g_cli_requests_parsed, request, out);
    }
    cli_requests_parsed_given(&g_cli_requests_parsed);
    cli_lock_give(&g_cli_requests_lock);
    if (!whole)
    {
        cli_requests_full_err(err_msg);
    }
    else if (request == NULL)
    {
        mmgr_line_put(err_msg, "the file contains no request");
    }
    return request != NULL;
}
