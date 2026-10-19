// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base64.c: bytes as base64 and back. Three bytes make four letters, the last group padded with `=`; reading back,
// a group stops at its first `=`
#include "base64.h"

void cli_base64(CliLine *out, const unsigned char *bytes, unsigned long long size)
{
    static const char s_letters[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (unsigned long long at = 0ull; at < size; at += 3ull)
    {
        const unsigned long long length = ((size - at) < 3ull) ? (size - at) : 3ull;
        const unsigned int group = ((unsigned int)bytes[at] << 16u) |
                                   ((length > 1ull) ? ((unsigned int)bytes[at + 1ull] << 8u) : 0u) |
                                   ((length > 2ull) ? (unsigned int)bytes[at + 2ull] : 0u);
        for (unsigned int place = 0u; place < 4u; place += 1u)
        {
            const char letter = s_letters[(group >> (18u - (6u * place))) & 63u];
            cli_line_character(out, (place <= length) ? letter : '=');
        }
    }
}

// the six bits a base64 letter stands for, or 64 where it is none
static unsigned int cli_sextet(char letter)
{
    if ((letter >= 'A') && (letter <= 'Z'))
    {
        return (unsigned int)(letter - 'A');
    }
    if ((letter >= 'a') && (letter <= 'z'))
    {
        return (unsigned int)(letter - 'a') + 26u;
    }
    if ((letter >= '0') && (letter <= '9'))
    {
        return (unsigned int)(letter - '0') + 52u;
    }
    return (letter == '+') ? 62u : ((letter == '/') ? 63u : 64u);
}

int cli_unbase64(const char *text, unsigned long long length, unsigned char *out, unsigned long long room,
                 unsigned long long *size)
{
    *size = 0ull;
    unsigned int group = 0u;
    unsigned int held = 0u;
    unsigned int taken = 0u;
    int padded = 0;
    for (unsigned long long at = 0ull; at <= length; at += 1ull)
    {
        const int ended = at == length;
        const char letter = ended ? '\0' : text[at];
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
            const unsigned int sextet = cli_sextet(letter);
            if (!padded && (sextet == 64u))
            {
                return 0;
            }
            if (!padded)
            {
                group = (group << 6u) | sextet;
                held += 1u;
            }
            taken += 1u;
        }
        if ((taken == 4u) || (ended && (taken > 0u)))
        {
            group <<= 6u * (4u - held);
            const unsigned int bytes = (held > 1u) ? (held - 1u) : 0u;
            if ((*size + bytes) > room)
            {
                return 0;
            }
            for (unsigned int place = 0u; place < bytes; place += 1u)
            {
                // each byte is eight bits of the group
                out[*size] = (unsigned char)(group >> (16u - (8u * place)));
                *size += 1ull;
            }
            group = 0u;
            held = 0u;
            taken = 0u;
            padded = 0;
        }
    }
    return 1;
}
