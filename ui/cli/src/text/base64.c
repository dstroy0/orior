// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base64.c: bytes as base64 and back. Three bytes make four letters, the last group padded with `=`; reading back,
// a group stops at its first `=`
#include "base64.h"

void cli_base64(const CliBase64Cfg *args)
{
    static const char s_letters[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (unsigned long long at = 0ull; at < args->size; at += 3ull)
    {
        const unsigned long long length = ((args->size - at) < 3ull) ? (args->size - at) : 3ull;
        const unsigned int group = ((unsigned int)args->bytes[at] << 16u) |
                                   ((length > 1ull) ? ((unsigned int)args->bytes[at + 1ull] << 8u) : 0u) |
                                   ((length > 2ull) ? (unsigned int)args->bytes[at + 2ull] : 0u);
        for (unsigned int place = 0u; place < 4u; place += 1u)
        {
            const char letter = s_letters[(group >> (18u - (6u * place))) & 63u];
            mmgr_line_ch(args->out, (place <= length) ? letter : '=');
        }
    }
}

typedef struct
{
    char letter;
} Base64Ctx;
// the six bits a base64 letter stands for, or 64 where it is none
EMBED_INLINE unsigned int cli_sextet(const Base64Ctx *args)
{
    if ((args->letter >= 'A') && (args->letter <= 'Z'))
    {
        return (unsigned int)(args->letter - 'A');
    }
    if ((args->letter >= 'a') && (args->letter <= 'z'))
    {
        return (unsigned int)(args->letter - 'a') + 26u;
    }
    if ((args->letter >= '0') && (args->letter <= '9'))
    {
        return (unsigned int)(args->letter - '0') + 52u;
    }
    return (args->letter == '+') ? 62u : ((args->letter == '/') ? 63u : 64u);
}

int cli_unbase64(const CliBase64TextCfg *args)
{
    unsigned long long *size = args->size_pointer;
    *size = 0ull;
    unsigned int group = 0u;
    unsigned int bits = 0u;
    unsigned int reserved = 0u;
    int padded = 0;
    for (unsigned long long at = 0ull; at <= args->length; at += 1ull)
    {
        const int ended = at == args->length;
        const char letter = ended ? '\0' : args->text[at];
        const int white =
            (letter == ' ') || (letter == '\t') || (letter == '\n') || (letter == '\f') || (letter == '\r');
        if (!ended && white)
        {
            continue;
        }
        if (!ended)
        {
            // a letter after `=` in its group is passed over, as is `=` itself
            padded = padded || (letter == '=');
            const unsigned int sextet = EMBED_CALL(cli_sextet, Base64Ctx, .letter = letter);
            if (!padded && (sextet == 64u))
            {
                return 0;
            }
            if (!padded)
            {
                group = (group << 6u) | sextet;
                bits += 1u;
            }
            reserved += 1u;
        }
        if ((reserved == 4u) || (ended && (reserved > 0u)))
        {
            group <<= 6u * (4u - bits);
            const unsigned int bytes = (bits > 1u) ? (bits - 1u) : 0u;
            if ((*size + bytes) > args->capacity)
            {
                return 0;
            }
            for (unsigned int place = 0u; place < bytes; place += 1u)
            {
                // each byte is eight bits of the group
                args->out_bytes[*size] = (unsigned char)(group >> (16u - (8u * place)));
                *size += 1ull;
            }
            group = 0u;
            bits = 0u;
            reserved = 0u;
            padded = 0;
        }
    }
    return 1;
}
