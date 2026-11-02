// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_ready.c: the request at a line of a file of requests made ready to go: its address, headers and body
// filled with the variables of the file over those of the environment chosen, an address with no scheme sent over
// http, and a body named by `< path` read from beside the file
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

static CliRequestsValues s_values;

typedef struct
{
    VerbaLinea *err_msg;
    const CliRequestsParsed *parsed;
    CliRequestsText text;
    char *out;
    unsigned long long capacity;
    const char *root;
    const char *path;
    const char *name;
    VerbaLinea *body;
    unsigned long long *size;
    const CliRequest *request;
    const char *env;
    CliRequestsSent *sent;
} RequestsReadyCtx;
// the reason a request did not fit what orior_cli supports
EMBED_INLINE void ready_full_err(const RequestsReadyCtx *args)
{
    mmgr_line_put(args->err_msg, "the request is larger than orior_cli supports");
}

// a text of the parse filled into `out`, sized for `capacity`, its end closed: 1, or 0 with why given
EMBED_INLINE int ready_fill(const RequestsReadyCtx *args)
{
    CliRequestsText text = args->text;
    char *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    VerbaLinea line = {out, args->capacity, 0ull};
    if (!EMBED_CALL(cli_requests_text_size_internal.fill, CliRequestsTextSizeCtx, .text = args->parsed->text + text.at,
                    .size = text.size, .values_read = &s_values, .out = &line, .err_msg = err_msg))
    {
        return 0;
    }
    if (mmgr_line_finish(&line) == 0ull)
    {
        EMBED_CALL(ready_full_err, RequestsReadyCtx, .err_msg = err_msg);
        return 0;
    }
    return 1;
}

// the file `name`, beside the file at `path`, read whole into `body`, a line: its size into `size`: 1, or 0 with why
// given
EMBED_INLINE int ready_body_file(const RequestsReadyCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    unsigned long long *size = args->size;
    VerbaLinea *err_msg = args->err_msg;
    char inside[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = path, .joined = inside,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = inside, .out = folder))
    {
        mmgr_line_put(err_msg, "the file has no folder");
        return 0;
    }
    char adjacent[CLI_PATH_MAX_LEN];
    VerbaLinea line = {adjacent, sizeof(adjacent), 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    unsigned long long length = 0ull;
    const unsigned char *const bytes = (mmgr_line_finish(&line) != 0ull)
                                           ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = adjacent,
                                                        .size_pointer = &length, .owner = &g_cli_requests_cells)
                                           : NULL;
    const int whole = (bytes != NULL) && EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg,
                                                    .bytes = (const char *)bytes, .size = length);
    if (whole)
    {
        mmgr_line_put_n(args->body, (const char *)bytes, length);
    }
    const int fits = whole && mmgr_line_ok(args->body);
    if (fits)
    {
        *size = length;
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_requests_cells);
    if (!whole)
    {
        mmgr_line_put(err_msg, args->name);
        mmgr_line_put(err_msg, ": it could not be read");
    }
    else if (!fits)
    {
        mmgr_line_put(err_msg, args->name);
        mmgr_line_put(err_msg, " is larger than orior_cli supports");
    }
    return fits;
}

// the request made ready, the requests' lock acquired and the file read: 1, or 0 with why given
EMBED_INLINE int ready_init(const RequestsReadyCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    const char *env = args->env;
    VerbaLinea *body = args->body;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(cli_requests_err_msg_root_internal.values, CliRequestsErrMsgRootCtx, .root = root, .path = path,
                    .env = env, .values = &s_values, .err_msg = err_msg))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->parsed->variables; at += 1u)
    {
        const CliRequestsText name = args->parsed->variable_name[at];
        const CliRequestsText value = args->parsed->variable_value[at];
        if (!EMBED_CALL(cli_requests_name_internal.value_append, CliRequestsNameCtx, .values = &s_values,
                        .name = args->parsed->text + name.at, .name_size = name.size,
                        .value = args->parsed->text + value.at, .value_size = value.size))
        {
            EMBED_CALL(ready_full_err, RequestsReadyCtx, .err_msg = err_msg);
            return 0;
        }
    }
    if ((args->request->method.size >= sizeof(args->sent->method)) ||
        !EMBED_CALL(ready_fill, RequestsReadyCtx, .parsed = args->parsed, .text = args->request->url,
                    .out = args->sent->url, .capacity = sizeof(args->sent->url), .err_msg = err_msg))
    {
        if (args->request->method.size >= sizeof(args->sent->method))
        {
            EMBED_CALL(ready_full_err, RequestsReadyCtx, .err_msg = err_msg);
        }
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->sent->method,
                            .src = args->parsed->text + args->request->method.at,
                            .bytes = (size_t)args->request->method.size + 1u});
    if (cellul.find(&(CatenaFinitaCfg){
            .src = args->sent->url, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) == NULL)
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->sent->url, .cap = CLI_CELLS_POOL});
        if (length + 8u > sizeof(args->sent->url))
        {
            EMBED_CALL(ready_full_err, RequestsReadyCtx, .err_msg = err_msg);
            return 0;
        }
        memor.move_up(&(MemoriaCfg){.dst = args->sent->url + 7, .src = args->sent->url, .bytes = length + 1u});
        memor.cpy(&(MemoriaCfg){.dst = args->sent->url, .src = "http://", .bytes = 7u});
    }
    args->sent->headers = args->request->headers;
    for (unsigned int at = 0u; at < args->request->headers; at += 1u)
    {
        if (!EMBED_CALL(ready_fill, RequestsReadyCtx, .parsed = args->parsed, .text = args->request->header_name[at],
                        .out = args->sent->header_name[at], .capacity = sizeof(args->sent->header_name[at]),
                        .err_msg = err_msg) ||
            !EMBED_CALL(ready_fill, RequestsReadyCtx, .parsed = args->parsed, .text = args->request->header_value[at],
                        .out = args->sent->header_value[at], .capacity = sizeof(args->sent->header_value[at]),
                        .err_msg = err_msg))
        {
            return 0;
        }
    }
    args->sent->body_size = 0ull;
    args->sent->redirect = args->request->redirect;
    body->at = 0ull;
    int made = 1;
    if (args->request->has_body)
    {
        made = EMBED_CALL(cli_requests_text_size_internal.fill, CliRequestsTextSizeCtx,
                          .text = args->parsed->text + args->request->body.at, .size = args->request->body.size,
                          .values_read = &s_values, .out = body, .err_msg = err_msg);
        if (made && !mmgr_line_ok(body))
        {
            EMBED_CALL(ready_full_err, RequestsReadyCtx, .err_msg = err_msg);
            made = 0;
        }
        args->sent->body_size = made ? body->at : 0ull;
    }
    else if (args->request->has_body_file)
    {
        made = EMBED_CALL(ready_body_file, RequestsReadyCtx, .root = root, .path = path,
                          .name = args->parsed->text + args->request->body_file.at, .body = body,
                          .size = &args->sent->body_size, .err_msg = err_msg);
    }
    // the body's cell is the one it ended in: a line moves as it fills
    args->sent->body = body->out;
    return made;
}

int cli_requests_ready(const CliRequestsTextCtx *args)
{
    const CliRequestsParsed *const parsed = &g_cli_requests_parsed;
    const int whole = EMBED_CALL(cli_requests_text_size_parsed_internal.parse, CliRequestsTextSizeParsedCtx,
                                 .text = args->text, .size = args->size, .parsed = &g_cli_requests_parsed);
    const CliRequest *const request =
        whole ? EMBED_CALL(cli_requests_internal.at, CliRequestsCtx, .parsed_read = parsed, .line_u64 = args->line_u64)
              : NULL;
    int ready = 0;
    if (!whole)
    {
        cli_requests_text_internal.full_err(args);
    }
    else if (request == NULL)
    {
        mmgr_line_put(args->err_msg, "the file contains no request");
    }
    else
    {
        ready = EMBED_CALL(ready_init, RequestsReadyCtx, .root = args->root, .path = args->path, .parsed = parsed,
                           .request = request, .env = args->env, .sent = args->sent_out, .body = args->body,
                           .err_msg = args->err_msg);
        EMBED_CALL(cli_requests_err_msg_root_internal.values_free, CliRequestsErrMsgRootCtx, .values = &s_values);
        if (ready && (args->key != NULL))
        {
            EMBED_CALL(cli_requests_parsed_internal.key, CliRequestsParsedCtx, .parsed_read = parsed,
                       .request = request, .out = args->key);
        }
    }
    // what is sent was copied out of the file read, so the file read is given back here
    EMBED_CALL(cli_requests_text_size_parsed_internal.parsed_free, CliRequestsTextSizeParsedCtx,
               .parsed = &g_cli_requests_parsed);
    return ready;
}
