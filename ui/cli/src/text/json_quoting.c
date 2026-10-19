// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_quoting.c: a line whose bytes go into a JSON string in another line as it fills, escaped as
// cli_json_quoted_lossy escapes them, a letter a piece ends inside of held over to the next, so a text of any length is
// quoted into an answer with no room of its own
#include "json_tree.h"

// the bytes a UTF-8 letter takes, by its first byte; a byte that starts none takes one
static unsigned long long json_letter_bytes(unsigned char lead)
{
    return ((lead >= 0xc0u) && (lead <= 0xdfu))   ? 2ull
           : ((lead >= 0xe0u) && (lead <= 0xefu)) ? 3ull
           : ((lead >= 0xf0u) && (lead <= 0xf7u)) ? 4ull
                                                  : 1ull;
}

// `size` bytes of a piece escaped into the quoting's line, the letter the last piece ended inside of made whole from
// this one's first bytes, and the letter this one ends inside of held: 1, or 0 where the line is full
static int json_quoting_put(void *context, const char *bytes, unsigned long long size)
{
    CliJsonQuoting *const quoting = (CliJsonQuoting *)context;
    const unsigned char *at = (const unsigned char *)bytes;
    unsigned long long left = size;
    if (quoting->count != 0u)
    {
        const unsigned long long wanted = json_letter_bytes(quoting->held[0]);
        while ((quoting->count < wanted) && (left > 0ull) && ((*at & 0xc0u) == 0x80u))
        {
            quoting->held[quoting->count] = *at;
            quoting->count += 1u;
            at += 1;
            left -= 1ull;
        }
        if (left == 0ull)
        {
            return cli_line_fits(quoting->out);
        }
        cli_json_escaped_lossy(quoting->out, quoting->held, quoting->count);
        quoting->count = 0u;
    }
    unsigned long long whole = left;
    for (unsigned long long back = 1ull; (back <= 3ull) && (back <= left); back += 1ull)
    {
        const unsigned char letter = at[left - back];
        if ((letter & 0xc0u) != 0x80u)
        {
            whole = (json_letter_bytes(letter) > back) ? (left - back) : left;
            break;
        }
    }
    cli_json_escaped_lossy(quoting->out, at, whole);
    for (unsigned long long tail = whole; tail < left; tail += 1ull)
    {
        quoting->held[quoting->count] = at[tail];
        quoting->count += 1u;
    }
    return cli_line_fits(quoting->out);
}

void cli_json_quoting_begun(CliJsonQuoting *quoting, CliLine *out, CliLine *line, char *piece,
                            unsigned long long piece_size)
{
    quoting->out = out;
    quoting->count = 0u;
    cli_line_character(out, '"');
    *line = (CliLine){piece, piece_size, 0ull, json_quoting_put, quoting, 0ull};
}

void cli_json_quoting_ended(CliJsonQuoting *quoting, CliLine *line)
{
    (void)cli_line_flushed(line);
    cli_json_escaped_lossy(quoting->out, quoting->held, quoting->count);
    quoting->count = 0u;
    cli_line_character(quoting->out, '"');
}
