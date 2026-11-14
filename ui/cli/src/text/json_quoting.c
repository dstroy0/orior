// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_quoting.c: a line whose bytes go into a JSON string in another line as it fills, escaped as
// cli_json_quoted_lossy escapes them, a letter a piece ends inside of carried over to the next, so a text of any length
// is quoted into an answer with no capacity of its own
#include "json_tree.h"

typedef struct
{
    unsigned char lead;
} JsonQuotingCtx;

// the bytes a UTF-8 letter takes, by its first byte; a byte that starts none takes one
EMBED_INLINE unsigned long long json_letter_bytes(const JsonQuotingCtx *args)
{
    if ((args->lead >= 0xc0u) && (args->lead <= 0xdfu))
    {
        return 2ULL;
    }
    if ((args->lead >= 0xe0u) && (args->lead <= 0xefu))
    {
        return 3ULL;
    }
    if ((args->lead >= 0xf0u) && (args->lead <= 0xf7u))
    {
        return 4ULL;
    }
    return 1ULL;
}

// `size` bytes of a piece escaped into the quoting's line, the letter the last piece ended inside of made whole from
// this one's first bytes, and the letter this one ends inside of carried over: 1, or 0 where the line is full
static int json_quoting_append(void *context, const char *bytes, size_t size)
{
    CliJsonQuoting *const quoting = (CliJsonQuoting *)context;
    const unsigned char *at = (const unsigned char *)bytes;
    unsigned long long left = size;
    if (quoting->count != 0u)
    {
        const unsigned long long required = EMBED_CALL(json_letter_bytes, JsonQuotingCtx, .lead = quoting->pending[0]);
        while ((quoting->count < required) && (left > 0ULL) && ((*at & 0xc0u) == 0x80u))
        {
            quoting->pending[quoting->count] = *at;
            quoting->count += 1u;
            at += 1;
            left -= 1ULL;
        }
        if (left == 0ULL)
        {
            return mmgr_line_ok(quoting->out);
        }
        EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = quoting->out,
                   .text_bytes = quoting->pending, .length = quoting->count);
        quoting->count = 0u;
    }
    unsigned long long whole = left;
    for (unsigned long long back = 1ULL; (back <= 3ULL) && (back <= left); back += 1ULL)
    {
        const unsigned char letter = at[left - back];
        if ((letter & 0xc0u) != 0x80u)
        {
            whole = (EMBED_CALL(json_letter_bytes, JsonQuotingCtx, .lead = letter) > back) ? (left - back) : left;
            break;
        }
    }
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = quoting->out,
               .text_bytes = at, .length = whole);
    for (unsigned long long tail = whole; tail < left; tail += 1ULL)
    {
        quoting->pending[quoting->count] = at[tail];
        quoting->count += 1u;
    }
    return mmgr_line_ok(quoting->out);
}

void cli_json_quoting_begin(const CliJsonTreeOutCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *line = args->line;
    args->quoting->out = out;
    args->quoting->count = 0u;
    mmgr_line_ch(out, '"');
    *line = (VerbaLinea){args->piece, args->piece_size, 0ULL, json_quoting_append, args->quoting, 0ULL};
}

void cli_json_quoting_end(const CliJsonTreeOutCfg *args)
{
    (void)mmgr_line_flush(args->line);
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->quoting->out,
               .text_bytes = args->quoting->pending, .length = args->quoting->count);
    args->quoting->count = 0u;
    mmgr_line_ch(args->quoting->out, '"');
}
