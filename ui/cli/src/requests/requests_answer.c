// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_answer.c: an answer written as serde writes it, with what was sent: its body as text, read as Rust's
// from_utf8_lossy reads it, or as base64 where it is not text, cut where it is longer than the writer is given
#include "../text/base64.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

typedef struct
{
    unsigned int letter;
    const CliRequestsAnswer *answer;
    unsigned long long most;
    VerbaLinea *out;
    const CliRequestsSent *sent;
    const CliRequestsTimes *times;
} RequestsAnswerCtx;

// the bytes of a UTF-8 letter
EMBED_INLINE unsigned long long answer_letter_size(const RequestsAnswerCtx *args)
{
    if (args->letter < 0x80u)
    {
        return 1u;
    }
    if (args->letter < 0x800u)
    {
        return 2u;
    }
    if (args->letter < 0x10000u)
    {
        return 3u;
    }
    return 4u;
}

// the body written as a JSON string, its text cut to `most` bytes at a letter's start: 1 where it was cut
EMBED_INLINE int answer_body(const RequestsAnswerCtx *args)
{
    VerbaLinea *out = args->out;
    if (args->answer->binary)
    {
        const unsigned long long whole = ((args->answer->size + 2u) / 3u) * 4u;
        const unsigned long long letters = (whole < args->most) ? whole : args->most;
        const unsigned long long groups = letters / 4u;
        const unsigned long long rest = letters % 4u;
        mmgr_line_ch(out, '"');
        EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = out, .bytes = args->answer->body, .size = groups * 3u);
        if (rest > 0u)
        {
            char last[8];
            VerbaLinea last_line = {last, sizeof(last), 0ULL};
            const unsigned long long left = args->answer->size - (groups * 3u);
            EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &last_line,
                       .bytes = args->answer->body + (groups * 3u), .size = (left < 3u) ? left : 3u);
            mmgr_line_put_n(out, last, rest);
        }
        mmgr_line_ch(out, '"');
        return letters < whole;
    }
    unsigned long long bytes = 0ULL;
    unsigned long long text = 0ULL;
    while (bytes < args->answer->size)
    {
        unsigned int letter = 0u;
        const unsigned long long reserved =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg, .text_bytes = args->answer->body,
                       .length = args->answer->size, .at = bytes, .letter = &letter);
        if (text + EMBED_CALL(answer_letter_size, RequestsAnswerCtx, .letter = letter) > args->most)
        {
            break;
        }
        text += EMBED_CALL(answer_letter_size, RequestsAnswerCtx, .letter = letter);
        bytes += reserved;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = args->answer->body, .length = bytes);
    return bytes < args->answer->size;
}

// pairs of texts written as a JSON array of arrays of two strings: the request's own, or the answer's, stored in its
// head
EMBED_INLINE void answer_sent_headers(const RequestsAnswerCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < args->sent->headers; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? ",[" : "[");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = args->sent->header_name[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->sent->header_name[at], .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, ',');
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = args->sent->header_value[at],
                   .length =
                       cellul.len(&(CatenaFinitaCfg){.src = args->sent->header_value[at], .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, ']');
    }
    mmgr_line_ch(out, ']');
}

EMBED_INLINE void answer_headers(const RequestsAnswerCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < args->answer->headers; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? ",[" : "[");
        const CliRequestsText name = args->answer->header_name[at];
        const CliRequestsText value = args->answer->header_value[at];
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->answer->head + name.at, .length = name.size);
        mmgr_line_ch(out, ',');
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->answer->head + value.at, .length = value.size);
        mmgr_line_ch(out, ']');
    }
    mmgr_line_ch(out, ']');
}

// how long each part took, written as serde writes the times
EMBED_INLINE void answer_times(const RequestsAnswerCtx *args)
{
    VerbaLinea *out = args->out;
    static const char *const names[6] = {
        "{\"resolve\":", ",\"connect\":", ",\"tls\":", ",\"wait\":", ",\"read\":", ",\"total\":"};
    const double values[6] = {args->times->resolve, args->times->connect, args->times->tls,
                              args->times->wait,    args->times->read,    args->times->total};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        mmgr_line_put(out, names[at]);
        EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out, .value_f64 = values[at]);
    }
    mmgr_line_ch(out, '}');
}

void cli_requests_answer_write(const CliRequestsOutCtx *args)
{
    mmgr_line_put(args->out, "{\"at\":");
    mmgr_line_decimal(args->out, args->answer->at, 1u);
    mmgr_line_put(args->out, ",\"sent\":{\"method\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->sent->method,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->sent->method, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"url\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->sent->url,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->sent->url, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"headers\":");
    EMBED_CALL(answer_sent_headers, RequestsAnswerCtx, .sent = args->sent, .out = args->out);
    mmgr_line_put(args->out, ",\"body\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->sent->body, .length = args->sent->body_size);
    mmgr_line_put(args->out, args->sent->redirect ? ",\"redirect\":true}" : ",\"redirect\":false}");
    mmgr_line_put(args->out, ",\"url\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->answer->url,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->answer->url, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"status\":");
    mmgr_line_decimal(args->out, args->answer->status, 1u);
    mmgr_line_put(args->out, ",\"reason\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)args->answer->head + args->answer->reason.at,
               .length = args->answer->reason.size);
    mmgr_line_put(args->out, ",\"version\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)args->answer->head + args->answer->version.at,
               .length = args->answer->version.size);
    mmgr_line_put(args->out, ",\"headers\":");
    EMBED_CALL(answer_headers, RequestsAnswerCtx, .answer = args->answer, .out = args->out);
    mmgr_line_put(args->out, ",\"body\":");
    const int cut =
        EMBED_CALL(answer_body, RequestsAnswerCtx, .answer = args->answer, .most = args->most, .out = args->out);
    mmgr_line_put(args->out, args->answer->binary ? ",\"binary\":true,\"size\":" : ",\"binary\":false,\"size\":");
    mmgr_line_decimal(args->out, args->answer->size, 1u);
    mmgr_line_put(args->out, (args->answer->cut || cut) ? ",\"cut\":true,\"times\":" : ",\"cut\":false,\"times\":");
    EMBED_CALL(answer_times, RequestsAnswerCtx, .times = &args->answer->times, .out = args->out);
    mmgr_line_put(args->out, ",\"redirects\":[");
    for (unsigned int at = 0u; at < args->answer->redirects; at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? ",[" : "[");
        mmgr_line_decimal(args->out, args->answer->redirect_status[at], 1u);
        mmgr_line_ch(args->out, ',');
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = args->answer->redirect_url[at],
                   .length =
                       cellul.len(&(CatenaFinitaCfg){.src = args->answer->redirect_url[at], .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(args->out, ']');
    }
    mmgr_line_put(args->out, "],\"failed\":");
    if (args->answer->has_failed)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = args->answer->failed,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->answer->failed, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_ch(args->out, '}');
}
