// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_written.c: a file of requests read and written as serde writes its requests and variables, and the key
// its answers are stored under
#include "../text/json_tree.h"
#include "requests_internal.h"

typedef struct
{
    const CliRequestsParsed *parsed;
    CliRequestsText text;
    VerbaLinea *out;
    const CliRequestsText *names;
    const CliRequestsText *values;
    unsigned int count;
    const CliRequest *request;
} RequestsWrittenCtx;

// a text of the pool written as a JSON string
EMBED_INLINE void parse_quote(const RequestsWrittenCtx *args)
{
    CliRequestsText text = args->text;
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = args->parsed->text + text.at, .length = text.size);
}

// pairs of the pool written as a JSON array of arrays of two strings
EMBED_INLINE void parse_pairs(const RequestsWrittenCtx *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int index = 0u; index < args->count; index += 1u)
    {
        mmgr_line_put(args->out, (index > 0u) ? ",[" : "[");
        EMBED_CALL(parse_quote, RequestsWrittenCtx, .parsed = args->parsed, .text = args->names[index],
                   .out = args->out);
        mmgr_line_ch(args->out, ',');
        EMBED_CALL(parse_quote, RequestsWrittenCtx, .parsed = args->parsed, .text = args->values[index],
                   .out = args->out);
        mmgr_line_ch(args->out, ']');
    }
    mmgr_line_ch(args->out, ']');
}

// a request written as serde writes its struct
EMBED_INLINE void parse_request_write(const RequestsWrittenCtx *args)
{
    mmgr_line_put(args->out, "{\"name\":");
    EMBED_CALL(parse_quote, RequestsWrittenCtx, .parsed = args->parsed, .text = args->request->name, .out = args->out);
    mmgr_line_put(args->out, ",\"start\":");
    mmgr_line_decimal(args->out, args->request->start, 1u);
    mmgr_line_put(args->out, ",\"end\":");
    mmgr_line_decimal(args->out, args->request->end, 1u);
    mmgr_line_put(args->out, ",\"line\":");
    mmgr_line_decimal(args->out, args->request->line, 1u);
    mmgr_line_put(args->out, ",\"method\":");
    EMBED_CALL(parse_quote, RequestsWrittenCtx, .parsed = args->parsed, .text = args->request->method,
               .out = args->out);
    mmgr_line_put(args->out, ",\"url\":");
    EMBED_CALL(parse_quote, RequestsWrittenCtx, .parsed = args->parsed, .text = args->request->url, .out = args->out);
    mmgr_line_put(args->out, ",\"headers\":");
    EMBED_CALL(parse_pairs, RequestsWrittenCtx, .parsed = args->parsed, .names = args->request->header_name,
               .values = args->request->header_value, .count = args->request->headers, .out = args->out);
    mmgr_line_put(args->out, ",\"body\":");
    if (args->request->has_body)
    {
        EMBED_CALL(parse_quote, RequestsWrittenCtx, .parsed = args->parsed, .text = args->request->body,
                   .out = args->out);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"body_file\":");
    if (args->request->has_body_file)
    {
        EMBED_CALL(parse_quote, RequestsWrittenCtx, .parsed = args->parsed, .text = args->request->body_file,
                   .out = args->out);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, args->request->redirect ? ",\"redirect\":true}" : ",\"redirect\":false}");
}

// the reason a file of requests was not read whole
void cli_requests_full_err(const CliRequestsTextCtx *args)
{
    mmgr_line_put(args->err_msg, "the file of requests is larger than orior_cli supports");
}

int cli_requests_on_parse(const CliRequestsTextCfg *args)
{
    const CliRequestsParsed *const parsed = &g_cli_requests_parsed;
    const int whole = EMBED_CALL(cli_requests_text_size_parsed_internal.parse, CliRequestsTextSizeParsedCtx,
                                 .text = args->text, .size = args->size, .parsed = &g_cli_requests_parsed);
    if (whole)
    {
        mmgr_line_put(args->out, "{\"requests\":[");
        for (unsigned int index = 0u; index < parsed->count; index += 1u)
        {
            mmgr_line_put(args->out, (index > 0u) ? "," : "");
            EMBED_CALL(parse_request_write, RequestsWrittenCtx, .parsed = parsed, .request = &parsed->requests[index],
                       .out = args->out);
        }
        mmgr_line_put(args->out, "],\"variables\":");
        EMBED_CALL(parse_pairs, RequestsWrittenCtx, .parsed = parsed, .names = parsed->variable_name,
                   .values = parsed->variable_value, .count = parsed->variables, .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    g_cli_cells->persistent_buf_release(g_cli_requests_parsed.requests, &g_cli_requests_cells);
    g_cli_cells->persistent_buf_release(g_cli_requests_parsed.text, &g_cli_requests_cells);
    g_cli_requests_parsed.requests = NULL;
    g_cli_requests_parsed.requests_capacity = 0u;
    g_cli_requests_parsed.text = NULL;
    g_cli_requests_parsed.text_capacity = 0ULL;
    g_cli_requests_parsed.count = 0u;
    g_cli_requests_parsed.variables = 0u;
    g_cli_requests_parsed.used = 0ULL;
    g_cli_requests_parsed.full = 0;
    if (!whole)
    {
        EMBED_CALL(cli_requests_text_internal.full_err, CliRequestsTextCtx, .err_msg = args->err_msg);
    }
    return whole;
}

void cli_requests_key(const CliRequestsParsedCtx *args)
{
    if (args->request->name.size > 0u)
    {
        mmgr_line_put_n(args->out, args->parsed_read->text + args->request->name.at, args->request->name.size);
        return;
    }
    mmgr_line_put_n(args->out, args->parsed_read->text + args->request->method.at, args->request->method.size);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_put_n(args->out, args->parsed_read->text + args->request->url.at, args->request->url.size);
}

int cli_requests_key_at(const CliRequestsTextCfg *args)
{
    const int whole = EMBED_CALL(cli_requests_text_size_parsed_internal.parse, CliRequestsTextSizeParsedCtx,
                                 .text = args->text, .size = args->size, .parsed = &g_cli_requests_parsed);
    const CliRequest *const request = whole ? EMBED_CALL(cli_requests_internal.at, CliRequestsCtx,
                                                         .parsed_read = &g_cli_requests_parsed, .line_u64 = args->line)
                                            : NULL;
    if (request != NULL)
    {
        EMBED_CALL(cli_requests_parsed_internal.key, CliRequestsParsedCtx, .parsed_read = &g_cli_requests_parsed,
                   .request = request, .out = args->out);
    }
    g_cli_cells->persistent_buf_release(g_cli_requests_parsed.requests, &g_cli_requests_cells);
    g_cli_cells->persistent_buf_release(g_cli_requests_parsed.text, &g_cli_requests_cells);
    g_cli_requests_parsed.requests = NULL;
    g_cli_requests_parsed.requests_capacity = 0u;
    g_cli_requests_parsed.text = NULL;
    g_cli_requests_parsed.text_capacity = 0ULL;
    g_cli_requests_parsed.count = 0u;
    g_cli_requests_parsed.variables = 0u;
    g_cli_requests_parsed.used = 0ULL;
    g_cli_requests_parsed.full = 0;
    if (!whole)
    {
        EMBED_CALL(cli_requests_text_internal.full_err, CliRequestsTextCtx, .err_msg = args->err_msg);
    }
    else if (request == NULL)
    {
        mmgr_line_put(args->err_msg, "the file contains no request");
    }
    return request != NULL;
}
