// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_url.c: an address of a request taken apart, the address a redirect names read from the one it came from,
// a header of an answer found by its name, and the integers a head writes read as Rust reads them
#include "requests_internal.h"

#include "../text/text.h"

typedef struct
{
    char letter;
    const char *url;
    const char *authority;
    unsigned long long size;
    CliRequestsAddress *address;
    VerbaLinea *err_msg;
} RequestsUrlCtx;

// a letter in ASCII's lower case
EMBED_INLINE char url_lower(const RequestsUrlCtx *args)
{
    return ((args->letter >= 'A') && (args->letter <= 'Z')) ? (char)(args->letter - 'A' + 'a') : args->letter;
}

int cli_requests_number(const CliRequestsTextSizeBaseCtx *args)
{
    unsigned long long *value = args->value_u64s;
    unsigned long long at = ((args->size > 1u) && (args->text[0] == '+')) ? 1u : 0u;
    *value = 0ull;
    if (at == args->size)
    {
        return 0;
    }
    for (; at < args->size; at += 1u)
    {
        const char letter = EMBED_CALL(url_lower, RequestsUrlCtx, .letter = args->text[at]);
        const unsigned int digit = ((letter >= '0') && (letter <= '9')) ? (unsigned int)(letter - '0')
                                   : ((args->base == 16u) && (letter >= 'a') && (letter <= 'f'))
                                       ? (unsigned int)(letter - 'a' + 10)
                                       : 16u;
        if ((digit >= args->base) || (*value > (args->most - digit) / args->base))
        {
            return 0;
        }
        *value = (*value * args->base) + digit;
    }
    return 1;
}

int cli_requests_contains(const CliRequestsTextSizeNeedleCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->needle, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at + length <= args->size; at += 1ull)
    {
        unsigned long long same = 0ull;
        while ((same < length) &&
               (EMBED_CALL(url_lower, RequestsUrlCtx, .letter = args->text[at + same]) == args->needle[same]))
        {
            same += 1ull;
        }
        if (same == length)
        {
            return 1;
        }
    }
    return 0;
}

// the host and the port of an address's authority into `address`, after its user: 1, or 0 with why given
EMBED_INLINE int url_host(const RequestsUrlCtx *args)
{
    unsigned long long size = args->size;
    const char *port = NULL;
    unsigned long long port_size = 0ull;
    const char *host = args->authority;
    unsigned long long host_size = size;
    if ((size > 0u) && (args->authority[0] == '['))
    {
        const char *const close = memor.chr(&(MemoriaCfg){.src = args->authority, .val = ']', .bytes = (size_t)size});
        if (close == NULL)
        {
            mmgr_line_put(args->err_msg, args->url);
            mmgr_line_put(args->err_msg, " has an address that does not close");
            return 0;
        }
        host = args->authority + 1;
        host_size = (unsigned long long)(close - host);
        const int has_port = (close + 1 < args->authority + size) && (close[1] == ':');
        port = has_port ? (close + 2) : NULL;
        port_size = has_port ? (unsigned long long)(args->authority + size - port) : 0ull;
    }
    else
    {
        const char *const colon =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->authority, .length_value = size, .byte = ':');
        host_size = size;
        port_size = 0ull;
        if (colon != NULL)
        {
            port = colon + 1;
            host_size = (unsigned long long)(colon - args->authority);
            port_size = (unsigned long long)(args->authority + size - port);
        }
    }
    unsigned long long number = args->address->secure ? 443u : 80u;
    if ((port != NULL) &&
        !EMBED_CALL(cli_requests_text_size_base_internal.number, CliRequestsTextSizeBaseCtx, .text = port,
                    .size = port_size, .base = 10u, .most = 65535u, .value_u64s = &number))
    {
        mmgr_line_put_n(args->err_msg, port, port_size);
        mmgr_line_put(args->err_msg, " is not a port");
        return 0;
    }
    // a port read is at most 65535
    args->address->port = (unsigned int)number;
    if (host_size == 0u)
    {
        mmgr_line_put(args->err_msg, args->url);
        mmgr_line_put(args->err_msg, " names no host");
        return 0;
    }
    VerbaLinea host_line = {args->address->host, sizeof(args->address->host), 0ull};
    mmgr_line_put_n(&host_line, host, host_size);
    if (mmgr_line_finish(&host_line) == 0ull)
    {
        mmgr_line_put(args->err_msg, "the request is larger than orior_cli supports");
        return 0;
    }
    return 1;
}

int cli_requests_address(const CliRequestsErrMsgUrlCtx *args)
{
    const char *const scheme_end = cellul.find(
        &(CatenaFinitaCfg){.src = args->url, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL});
    if (scheme_end == NULL)
    {
        mmgr_line_put(args->err_msg, args->url);
        mmgr_line_put(args->err_msg, " names no scheme");
        return 0;
    }
    const unsigned long long scheme = (unsigned long long)(scheme_end - args->url);
    args->address->secure =
        (scheme == 5u) && EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                                     .text = args->url, .size = 5u, .needle = "https");
    if (!args->address->secure &&
        !((scheme == 4u) && EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                                       .text = args->url, .size = 4u, .needle = "http")))
    {
        for (unsigned long long at = 0ull; at < scheme; at += 1ull)
        {
            mmgr_line_ch(args->err_msg, EMBED_CALL(url_lower, RequestsUrlCtx, .letter = args->url[at]));
        }
        mmgr_line_put(args->err_msg, " is not http or https");
        return 0;
    }
    const char *const rest = scheme_end + 3;
    const unsigned long long authority_end =
        EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = rest, .set = "/?#");
    const char *authority = rest;
    for (const char *at = rest; at < rest + authority_end; at += 1)
    {
        authority = (*at == '@') ? (at + 1) : authority;
    }
    const char *const target = rest + authority_end;
    const unsigned long long target_size =
        EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = target, .set = "#");
    VerbaLinea target_line = {args->address->target, sizeof(args->address->target), 0ull};
    mmgr_line_put(&target_line, ((target_size == 0u) || (target[0] == '?')) ? "/" : "");
    mmgr_line_put_n(&target_line, target, target_size);
    if (mmgr_line_finish(&target_line) == 0ull)
    {
        mmgr_line_put(args->err_msg, "the request is larger than orior_cli supports");
        return 0;
    }
    return EMBED_CALL(url_host, RequestsUrlCtx, .url = args->url, .authority = authority,
                      .size = (unsigned long long)(rest + authority_end - authority), .address = args->address,
                      .err_msg = args->err_msg);
}

void cli_requests_join(const CliRequestsOutBaseCtx *args)
{
    if (cellul.find(&(CatenaFinitaCfg){
            .src = args->location, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) != NULL)
    {
        mmgr_line_put(args->out, args->location);
        return;
    }
    const char *const scheme_end = cellul.find(
        &(CatenaFinitaCfg){.src = args->base_text, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL});
    const char *const rest = (scheme_end != NULL) ? (scheme_end + 3) : args->base_text;
    if (scheme_end != NULL)
    {
        mmgr_line_put_n(args->out, args->base_text, (unsigned long long)(scheme_end - args->base_text));
    }
    else
    {
        mmgr_line_put(args->out, "http");
    }
    mmgr_line_put(args->out, "://");
    if ((args->location[0] == '/') && (args->location[1] == '/'))
    {
        mmgr_line_put(args->out, args->location + 2);
        return;
    }
    const unsigned long long authority = EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = rest, .set = "/?#");
    mmgr_line_put_n(args->out, rest, authority);
    if (args->location[0] != '/')
    {
        const char *const path = rest + authority;
        const unsigned long long path_size =
            EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = path, .set = "?#");
        unsigned long long folder = 0ull;
        for (unsigned long long at = 0ull; at < path_size; at += 1ull)
        {
            folder = (path[at] == '/') ? (at + 1u) : folder;
        }
        if (folder == 0u)
        {
            mmgr_line_ch(args->out, '/');
        }
        mmgr_line_put_n(args->out, path, folder);
    }
    mmgr_line_put(args->out, args->location);
}

const CliRequestsText *cli_requests_header(const CliRequestsNameAnswerCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    for (unsigned int at = 0u; at < args->answer->headers; at += 1u)
    {
        const CliRequestsText *const name_at = &args->answer->header_name[at];
        unsigned long long same = 0ull;
        while ((name_at->size == length) && (same < length) &&
               (EMBED_CALL(url_lower, RequestsUrlCtx, .letter = args->answer->head[name_at->at + same]) ==
                EMBED_CALL(url_lower, RequestsUrlCtx, .letter = args->name[same])))
        {
            same += 1ull;
        }
        if ((name_at->size == length) && (same == length))
        {
            return &args->answer->header_value[at];
        }
    }
    return NULL;
}
