// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_white.c: the white characters of a file of requests as Rust's char::is_whitespace has them, a part trimmed of
// them as Rust's str::trim trims it, and a text of a pool
#include "../text/json_tree.h"
#include "requests_internal.h"

const char *cli_requests_text(const CliRequestsPoolCtx *args)
{
    return args->pool + args->text_requests_text.at;
}

typedef struct
{
    unsigned int character;
    const char *text;
    unsigned long long start;
    unsigned long long end;
} RequestsWhiteCtx;
// whether a character is white as Rust's char::is_whitespace has it
EMBED_INLINE int white_is(const RequestsWhiteCtx *args)
{
    return ((args->character >= 0x09u) && (args->character <= 0x0du)) || (args->character == 0x20u) || (args->character == 0x85u) ||
           (args->character == 0xa0u) || (args->character == 0x1680u) ||
           ((args->character >= 0x2000u) && (args->character <= 0x200au)) || (args->character == 0x2028u) ||
           (args->character == 0x2029u) || (args->character == 0x202fu) || (args->character == 0x205fu) ||
           (args->character == 0x3000u);
}

unsigned long long cli_requests_white_at(const CliRequestsTextAtCtx *args)
{
    unsigned int character = 0u;
    const unsigned long long reserved = EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg,
                                                   .text_bytes = (const unsigned char *)args->text,
                                                   .length = args->end_value, .at = args->at, .character = &character);
    return EMBED_CALL(white_is, RequestsWhiteCtx, .character = character) ? reserved : 0ULL;
}

// the bytes of the white character that ends at `end`, 0 where the character there is not white
EMBED_INLINE unsigned long long white_before(const RequestsWhiteCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->end - 1u;
    while ((at > args->start) && (((unsigned char)text[at] & 0xc0u) == 0x80u) && (args->end - at < 4u))
    {
        at -= 1u;
    }
    unsigned int character = 0u;
    const unsigned long long reserved =
        EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = (const unsigned char *)text,
                   .length = args->end, .at = at, .character = &character);
    return ((at + reserved == args->end) && EMBED_CALL(white_is, RequestsWhiteCtx, .character = character)) ? reserved : 0ULL;
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
