// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_head.c: the head of a request as it goes out, the head of its answer stored as it is read, the answer's body
// undone where it was packed, and whether that body is text
#include "../command_line/pool.h"
#include "../editor/archive.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

int cli_requests_head_store(const CliRequestsTextSizeAnswerCtx *args)
{
    unsigned long long size = args->size;
    if (args->answer_out->head_used + size + 1u > sizeof(args->answer_out->head))
    {
        return 0;
    }
    args->text_span->at = args->answer_out->head_used;
    args->text_span->size = size;
    memor.cpy(&(MemoriaCfg){
        .dst = args->answer_out->head + args->answer_out->head_used, .src = args->text, .bytes = (embed_word)size});
    args->answer_out->head[args->answer_out->head_used + size] = '\0';
    args->answer_out->head_used += size + 1u;
    return 1;
}

int cli_requests_head_line(const CliRequestsSizeAnswerCtx *args)
{
    const char *const colon = memor.chr(&(MemoriaCfg){.src = args->line, .val = ':', .bytes = (embed_word)args->size});
    if (colon == NULL)
    {
        return 1;
    }
    if (args->answer_out->headers == CLI_REQUESTS_ANSWER_HEADERS)
    {
        return 0;
    }
    unsigned long long name_start = 0ULL;
    unsigned long long name_end = (unsigned long long)(colon - args->line);
    unsigned long long value_start = name_end + 1u;
    unsigned long long value_end = args->size;
    EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = args->line, .start = &name_start,
               .end = &name_end);
    EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = args->line,
               .start = &value_start, .end = &value_end);
    const unsigned int at = args->answer_out->headers;
    if (!EMBED_CALL(cli_requests_text_size_answer_internal.head_store, CliRequestsTextSizeAnswerCtx,
                    .answer_out = args->answer_out, .text = args->line + name_start, .size = name_end - name_start,
                    .text_span = &args->answer_out->header_name[at]) ||
        !EMBED_CALL(cli_requests_text_size_answer_internal.head_store, CliRequestsTextSizeAnswerCtx,
                    .answer_out = args->answer_out, .text = args->line + value_start, .size = value_end - value_start,
                    .text_span = &args->answer_out->header_value[at]))
    {
        return 0;
    }
    args->answer_out->headers += 1u;
    return 1;
}

typedef struct
{
    const CliRequestsSent *hop;
    const char *name;
    const char *lower;
    const char *value;
    VerbaLinea *out;
    const CliRequestsAnswer *answer;
    const CliRequestsText *value_requests_text_pointer;
    CliRequestsWorkState *work_state;
    unsigned long long bytes;
    VerbaLinea *err_msg;
} RequestsHeadCtx;
// whether the request has a header named `name`, ASCII case aside
EMBED_INLINE int head_has(const RequestsHeadCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    for (unsigned int at = 0u; at < args->hop->headers; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = args->hop->header_name[at], .cap = CLI_POOL_BYTES}) == length) &&
            EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                       .text = args->hop->header_name[at], .size = length, .needle = args->name))
        {
            return 1;
        }
    }
    return 0;
}

// a header written where the request names none of its name
EMBED_INLINE void head_free(const RequestsHeadCtx *args)
{
    if (!EMBED_CALL(head_has, RequestsHeadCtx, .hop = args->hop, .name = args->lower))
    {
        mmgr_line_put(args->out, args->name);
        mmgr_line_put(args->out, ": ");
        mmgr_line_put(args->out, args->value);
        mmgr_line_put(args->out, "\r\n");
    }
}

void cli_requests_head_write(const CliRequestsOutHopCtx *args)
{
    mmgr_line_put(args->out, args->hop->method);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_put(args->out, args->address_read->target);
    mmgr_line_put(args->out, " HTTP/1.1\r\n");
    char host[CLI_REQUESTS_ADDRESS + 16u];
    VerbaLinea host_line = {host, sizeof(host), 0ULL};
    const int bracketed =
        (cellul.chr(&(CatenaFinitaCfg){.src = args->address_read->host, .cap = CLI_POOL_BYTES, .byte = ':'}) != NULL);
    mmgr_line_put(&host_line, bracketed ? "[" : "");
    mmgr_line_put(&host_line, args->address_read->host);
    mmgr_line_put(&host_line, bracketed ? "]" : "");
    if (args->address_read->port != (args->address_read->secure ? 443u : 80u))
    {
        mmgr_line_ch(&host_line, ':');
        mmgr_line_decimal(&host_line, args->address_read->port, 1u);
    }
    mmgr_line_finish(&host_line);
    EMBED_CALL(head_free, RequestsHeadCtx, .hop = args->hop, .name = "Host", .lower = "host", .value = host,
               .out = args->out);
    for (unsigned int at = 0u; at < args->hop->headers; at += 1u)
    {
        mmgr_line_put(args->out, args->hop->header_name[at]);
        mmgr_line_put(args->out, ": ");
        mmgr_line_put(args->out, args->hop->header_value[at]);
        mmgr_line_put(args->out, "\r\n");
    }
    EMBED_CALL(head_free, RequestsHeadCtx, .hop = args->hop, .name = "User-Agent", .lower = "user-agent",
               .value = "orior/" CLI_VERSION, .out = args->out);
    EMBED_CALL(head_free, RequestsHeadCtx, .hop = args->hop, .name = "Accept", .lower = "accept", .value = "*/*",
               .out = args->out);
    EMBED_CALL(head_free, RequestsHeadCtx, .hop = args->hop, .name = "Accept-Encoding", .lower = "accept-encoding",
               .value = "gzip, deflate", .out = args->out);
    const int sends =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->hop->method, .other = "POST", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->hop->method, .other = "PUT", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->hop->method, .other = "PATCH", .cap = CLI_POOL_BYTES}));
    if (((args->hop->body_size > 0u) || sends) &&
        !EMBED_CALL(head_has, RequestsHeadCtx, .hop = args->hop, .name = "transfer-encoding"))
    {
        char length[24];
        VerbaLinea length_line = {length, sizeof(length), 0ULL};
        mmgr_line_decimal(&length_line, args->hop->body_size, 1u);
        mmgr_line_finish(&length_line);
        EMBED_CALL(head_free, RequestsHeadCtx, .hop = args->hop, .name = "Content-Length", .lower = "content-length",
                   .value = length, .out = args->out);
    }
    EMBED_CALL(head_free, RequestsHeadCtx, .hop = args->hop, .name = "Connection", .lower = "connection",
               .value = "close", .out = args->out);
    mmgr_line_put(args->out, "\r\n");
}

// whether a header's value, ASCII case aside, is `lower`
EMBED_INLINE int head_is(const RequestsHeadCtx *args)
{
    return (args->value_requests_text_pointer != NULL) &&
           (args->value_requests_text_pointer->size ==
            cellul.len(&(CatenaFinitaCfg){.src = args->lower, .cap = CLI_POOL_BYTES})) &&
           EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                      .text = args->answer->head + args->value_requests_text_pointer->at,
                      .size = args->value_requests_text_pointer->size, .needle = args->lower);
}

int cli_requests_decode(const CliRequestsSizeErrMsgCtx *args)
{
    unsigned long long size = args->size;
    CliRequestsAnswer *const answer = &args->work_state->answer;
    const CliRequestsText *const encoding =
        EMBED_CALL(cli_requests_name_answer_internal.header, CliRequestsNameAnswerCtx, .answer = answer,
                   .name = "Content-Encoding");
    const unsigned char *const raw = (const unsigned char *)args->work_state->raw.out;
    unsigned long long unpacked = 0ULL;
    answer->body = raw;
    answer->size = size;
    if (EMBED_CALL(head_is, RequestsHeadCtx, .answer = answer, .value_requests_text_pointer = encoding,
                   .lower = "gzip") ||
        EMBED_CALL(head_is, RequestsHeadCtx, .answer = answer, .value_requests_text_pointer = encoding,
                   .lower = "x-gzip"))
    {
        // the work state's undone bytes hold a body at max, where the gzip's bytes go
        if (!EMBED_CALL(cli_archive_bytes.gunzip, CliArchiveBytesCfg, .bytes = raw, .size = size,
                        .out = args->work_state->unpacked, .capacity = args->work_state->unpacked_capacity,
                        .out_size = &unpacked, .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    else if (EMBED_CALL(head_is, RequestsHeadCtx, .answer = answer, .value_requests_text_pointer = encoding,
                        .lower = "deflate"))
    {
        const int zlib =
            (size > 2u) && ((raw[0] & 0x0fu) == 8u) && ((((unsigned int)raw[0] << 8u) | raw[1]) % 31u == 0u);
        unsigned long long used = 0ULL;
        if (!EMBED_CALL(cli_archive_size.inflate, CliArchiveSizeCfg, .data = zlib ? (raw + 2) : raw,
                        .size = zlib ? (size - 2u) : size, .out = args->work_state->unpacked,
                        .capacity = args->work_state->unpacked_capacity, .out_size = &unpacked, .used = &used,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    else
    {
        return 1;
    }
    answer->body = args->work_state->unpacked;
    answer->size = unpacked;
    return 1;
}

int cli_requests_is_text(const CliRequestsNameAnswerCtx *args)
{
    const CliRequestsText *const content_type =
        EMBED_CALL(cli_requests_name_answer_internal.header, CliRequestsNameAnswerCtx, .answer = args->answer,
                   .name = "Content-Type");
    const char *type = "";
    unsigned long long size = 0ULL;
    if (content_type != NULL)
    {
        type = args->answer->head + content_type->at;
        size = content_type->size;
    }
    static const char *const texts[7] = {"json", "xml", "javascript", "x-www-form-urlencoded",
                                         "yaml", "csv", "graphql"};
    int text = (size >= 5u) && EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                                          .text = type, .size = 5u, .needle = "text/");
    {
        unsigned int at = 0u;
        while (!text && (at < 7u))
        {
            text = EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                              .text = type, .size = size, .needle = texts[at]);
            at += 1u;
        }
    }
    if (text)
    {
        return 1;
    }
    static const char *const starts[3] = {"image/", "audio/", "video/"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        if ((size >= 6u) && EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                                       .text = type, .size = 6u, .needle = starts[at]))
        {
            return 0;
        }
    }
    if (EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx, .text = type,
                   .size = size, .needle = "octet-stream") ||
        EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx, .text = type,
                   .size = size, .needle = "pdf") ||
        EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx, .text = type,
                   .size = size, .needle = "zip"))
    {
        return 0;
    }
    return EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)args->answer->body,
                      .size = args->answer->size) &&
           (memor.chr(&(MemoriaCfg){.src = args->answer->body, .val = 0, .bytes = (embed_word)args->answer->size}) ==
            NULL);
}
