// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_written.c: a file of requests read and written as serde writes its requests and variables, and the key
// its answers are kept under
#include "../text/json_tree.h"
#include "requests_internal.h"

// a text of the pool written as a JSON string
static void parse_quoted(const CliRequestsParsed *parsed, CliRequestsText text, CliLine *out)
{
    cli_json_quoted(out, parsed->text + text.at, text.size);
}

// pairs of the pool written as a JSON array of arrays of two strings
static void parse_pairs(const CliRequestsParsed *parsed, const CliRequestsText *names, const CliRequestsText *values,
                        unsigned int count, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        cli_line_text(out, (index > 0u) ? ",[" : "[");
        parse_quoted(parsed, names[index], out);
        cli_line_character(out, ',');
        parse_quoted(parsed, values[index], out);
        cli_line_character(out, ']');
    }
    cli_line_character(out, ']');
}

// a request written as serde writes its struct
static void parse_request_written(const CliRequestsParsed *parsed, const CliRequest *request, CliLine *out)
{
    cli_line_text(out, "{\"name\":");
    parse_quoted(parsed, request->name, out);
    cli_line_text(out, ",\"start\":");
    cli_line_decimal(out, request->start, 1u);
    cli_line_text(out, ",\"end\":");
    cli_line_decimal(out, request->end, 1u);
    cli_line_text(out, ",\"line\":");
    cli_line_decimal(out, request->line, 1u);
    cli_line_text(out, ",\"method\":");
    parse_quoted(parsed, request->method, out);
    cli_line_text(out, ",\"url\":");
    parse_quoted(parsed, request->url, out);
    cli_line_text(out, ",\"headers\":");
    parse_pairs(parsed, request->header_name, request->header_value, request->headers, out);
    cli_line_text(out, ",\"body\":");
    if (request->has_body)
    {
        parse_quoted(parsed, request->body, out);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"body_file\":");
    if (request->has_body_file)
    {
        parse_quoted(parsed, request->body_file, out);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, request->redirect ? ",\"redirect\":true}" : ",\"redirect\":false}");
}

// the reason a file of requests was not read whole
void cli_requests_full_said(CliLine *said)
{
    cli_line_text(said, "the file of requests is larger than orior_cli holds");
}

int cli_requests_parsed(const char *text, unsigned long long size, CliLine *out, CliLine *said)
{
    cli_lock_take(&g_cli_requests_lock);
    const CliRequestsParsed *const parsed = &g_cli_requests_parsed;
    const int whole = cli_requests_parse(text, size, &g_cli_requests_parsed);
    if (whole)
    {
        cli_line_text(out, "{\"requests\":[");
        for (unsigned int index = 0u; index < parsed->count; index += 1u)
        {
            cli_line_text(out, (index > 0u) ? "," : "");
            parse_request_written(parsed, &parsed->requests[index], out);
        }
        cli_line_text(out, "],\"variables\":");
        parse_pairs(parsed, parsed->variable_name, parsed->variable_value, parsed->variables, out);
        cli_line_character(out, '}');
    }
    cli_requests_parsed_given(&g_cli_requests_parsed);
    cli_lock_give(&g_cli_requests_lock);
    if (!whole)
    {
        cli_requests_full_said(said);
    }
    return whole;
}

void cli_requests_key(const CliRequestsParsed *parsed, const CliRequest *request, CliLine *out)
{
    if (request->name.size > 0u)
    {
        cli_line_counted(out, parsed->text + request->name.at, request->name.size);
        return;
    }
    cli_line_counted(out, parsed->text + request->method.at, request->method.size);
    cli_line_character(out, ' ');
    cli_line_counted(out, parsed->text + request->url.at, request->url.size);
}

int cli_requests_key_at(const char *text, unsigned long long size, unsigned long long line, CliLine *out, CliLine *said)
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
        cli_requests_full_said(said);
    }
    else if (request == NULL)
    {
        cli_line_text(said, "the file holds no request");
    }
    return request != NULL;
}
