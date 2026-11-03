// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_white.c: the white letters of a file of requests as Rust's char::is_whitespace has them, a part trimmed of
// them as Rust's str::trim trims it, and a text of a pool
#include "../text/json_tree.h"
#include "requests_internal.h"

const char *cli_requests_text(const CliRequestsPoolCtx *args)
{
    return args->pool + args->text_requests_text.at;
}

typedef struct
{
    unsigned int letter;
    const char *text;
    unsigned long long start;
    unsigned long long end;
} RequestsWhiteCtx;
// whether a letter is white as Rust's char::is_whitespace has it
EMBED_INLINE int white_is(const RequestsWhiteCtx *args)
{
    return ((args->letter >= 0x09u) && (args->letter <= 0x0du)) || (args->letter == 0x20u) || (args->letter == 0x85u) ||
           (args->letter == 0xa0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202fu) || (args->letter == 0x205fu) ||
           (args->letter == 0x3000u);
}

unsigned long long cli_requests_white_at(const CliRequestsTextAtCtx *args)
{
    unsigned int letter = 0u;
    const unsigned long long reserved = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                   .text_bytes = (const unsigned char *)args->text,
                                                   .length = args->end_value, .at = args->at, .letter = &letter);
    return EMBED_CALL(white_is, RequestsWhiteCtx, .letter = letter) ? reserved : 0ULL;
}

// the bytes of the white letter that ends at `end`, 0 where the letter there is not white
EMBED_INLINE unsigned long long white_before(const RequestsWhiteCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->end - 1u;
    while ((at > args->start) && (((unsigned char)text[at] & 0xc0u) == 0x80u) && (args->end - at < 4u))
    {
        at -= 1u;
    }
    unsigned int letter = 0u;
    const unsigned long long reserved =
        EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg, .text_bytes = (const unsigned char *)text,
                   .length = args->end, .at = at, .letter = &letter);
    return ((at + reserved == args->end) && EMBED_CALL(white_is, RequestsWhiteCtx, .letter = letter)) ? reserved : 0ULL;
}

void cli_requests_trim(const CliRequestsTextStartCtx *args)
{
    unsigned long long *start = args->start;
    unsigned long long *end = args->end;
    unsigned long long reserved = 1ULL;
    while ((*start < *end) && (reserved > 0ULL))
    {
        reserved = EMBED_CALL(cli_requests_text_at_internal.white_at, CliRequestsTextAtCtx, .text = args->text,
                              .at = *start, .end_value = *end);
        *start += reserved;
    }
    reserved = 1ULL;
    while ((*end > *start) && (reserved > 0ULL))
    {
        reserved = EMBED_CALL(white_before, RequestsWhiteCtx, .text = args->text, .start = *start, .end = *end);
        *end -= reserved;
    }
}
