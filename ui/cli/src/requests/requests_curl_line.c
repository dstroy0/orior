// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_curl_line.c: the curl line that sends the request at a line of a file, for bash and the shells that read
// quotes as it does, each part on a line of its own, written as a JSON string
#include "../text/json_tree.h"
#include "requests_internal.h"

typedef struct
{
    VerbaLinea *out;
    const char *text;
    unsigned long long size;
} RequestsCurlLineCtx;

// `size` bytes of `text` written inside the JSON string
EMBED_INLINE void line_append(const RequestsCurlLineCtx *args)
{
    VerbaLinea *out = args->out;
    const char *text = args->text;
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)text, .length = args->size);
}

// `size` bytes of `text` written in single quotes, each quote in it closed, escaped and opened again
EMBED_INLINE void line_quote(const RequestsCurlLineCtx *args)
{
    EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = "'", .size = 1u);
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        if (args->text[at] == '\'')
        {
            EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = args->text + from,
                       .size = at - from);
            EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = "'\\''", .size = 4u);
            from = at + 1u;
        }
    }
    EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = args->text + from,
               .size = args->size - from);
    EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = "'", .size = 1u);
}

// the parts between curl's and the next, each on a line of its own
EMBED_INLINE void line_join(const RequestsCurlLineCtx *args)
{
    EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = " \\\n  ", .size = 5u);
}

// the curl line that sends `sent`, written inside a JSON string
void cli_requests_curl_write(const CliRequestsOutCtx *args)
{
    mmgr_line_ch(args->out, '"');
    EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = "curl", .size = 4u);
    const char *const fallback = (args->sent->body_size == 0u) ? "GET" : "POST";
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->sent->method, .other = "HEAD", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(line_join, RequestsCurlLineCtx, .out = args->out);
        EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = "--head", .size = 6u);
    }
    else if (!cellul.eq(&(CatenaFinitaCfg){.src = args->sent->method, .other = fallback, .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(line_join, RequestsCurlLineCtx, .out = args->out);
        EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = "-X ", .size = 3u);
        EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = args->sent->method,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->sent->method, .cap = CLI_CELLS_POOL}));
    }
    EMBED_CALL(line_join, RequestsCurlLineCtx, .out = args->out);
    EMBED_CALL(line_quote, RequestsCurlLineCtx, .out = args->out, .text = args->sent->url,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->sent->url, .cap = CLI_CELLS_POOL}));
    for (unsigned int at = 0u; at < args->sent->headers; at += 1u)
    {
        char header[CLI_REQUESTS_NAME + CLI_REQUESTS_VALUE + 2u];
        VerbaLinea header_line = {header, sizeof(header), 0ull};
        mmgr_line_put(&header_line, args->sent->header_name[at]);
        mmgr_line_put(&header_line, ": ");
        mmgr_line_put(&header_line, args->sent->header_value[at]);
        EMBED_CALL(line_join, RequestsCurlLineCtx, .out = args->out);
        EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = "-H ", .size = 3u);
        EMBED_CALL(line_quote, RequestsCurlLineCtx, .out = args->out, .text = header, .size = header_line.at);
    }
    if (args->sent->body_size > 0u)
    {
        EMBED_CALL(line_join, RequestsCurlLineCtx, .out = args->out);
        EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = "--data-raw ", .size = 11u);
        EMBED_CALL(line_quote, RequestsCurlLineCtx, .out = args->out, .text = args->sent->body,
                   .size = args->sent->body_size);
    }
    if (args->sent->redirect)
    {
        EMBED_CALL(line_join, RequestsCurlLineCtx, .out = args->out);
        EMBED_CALL(line_append, RequestsCurlLineCtx, .out = args->out, .text = "-L", .size = 2u);
    }
    mmgr_line_ch(args->out, '"');
}

int cli_requests_curl(const CliRequestsTextCfg *args)
{
    CliRequestsWorkState *const work_state = cli_requests_work_state_alloc();
    if (work_state == NULL)
    {
        mmgr_line_put(args->err_msg, "the request's work state could not be allocated");
        return 0;
    }
    const int ready =
        EMBED_CALL(cli_requests_text_internal.ready, CliRequestsTextCtx, .root = args->root, .path = args->path,
                   .text = args->text, .size = args->size, .line_u64 = args->line, .env = args->env,
                   .sent_out = &work_state->sent, .body = &work_state->body, .key = NULL, .err_msg = args->err_msg);
    if (ready)
    {
        EMBED_CALL(cli_requests_out_internal.curl_write, CliRequestsOutCtx, .sent = &work_state->sent,
                   .out = args->out);
    }
    EMBED_CALL(cli_requests_size_err_msg_internal.capacity_free, CliRequestsSizeErrMsgCtx, .work_state = work_state);
    return ready;
}
