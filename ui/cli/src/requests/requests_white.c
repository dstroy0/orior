// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_white.c: the white letters of a file of requests as Rust's char::is_whitespace has them, a part trimmed of
// them as Rust's str::trim trims it, and a text of a pool
#include "../text/json_tree.h"
#include "requests_internal.h"

const char *cli_requests_text(const char *pool, CliRequestsText text)
{
    return pool + text.at;
}

// whether a letter is white as Rust's char::is_whitespace has it
static int white_is(unsigned int letter)
{
    return ((letter >= 0x09u) && (letter <= 0x0du)) || (letter == 0x20u) || (letter == 0x85u) || (letter == 0xa0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u);
}

unsigned long long cli_requests_white_at(const char *text, unsigned long long at, unsigned long long end)
{
    unsigned int letter = 0u;
    const unsigned long long taken = cli_lossy_letter((const unsigned char *)text, end, at, &letter);
    return white_is(letter) ? taken : 0ull;
}

// the bytes of the white letter that ends at `end`, 0 where the letter there is not white
static unsigned long long white_before(const char *text, unsigned long long start, unsigned long long end)
{
    unsigned long long at = end - 1u;
    while ((at > start) && (((unsigned char)text[at] & 0xc0u) == 0x80u) && (end - at < 4u))
    {
        at -= 1u;
    }
    unsigned int letter = 0u;
    const unsigned long long taken = cli_lossy_letter((const unsigned char *)text, end, at, &letter);
    return ((at + taken == end) && white_is(letter)) ? taken : 0ull;
}

void cli_requests_trimmed(const char *text, unsigned long long *start, unsigned long long *end)
{
    unsigned long long taken = 1ull;
    while ((*start < *end) && (taken > 0ull))
    {
        taken = cli_requests_white_at(text, *start, *end);
        *start += taken;
    }
    taken = 1ull;
    while ((*end > *start) && (taken > 0ull))
    {
        taken = white_before(text, *start, *end);
        *end -= taken;
    }
}
