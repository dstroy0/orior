// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klang.h: a block of another language in a file, `__<lang>__(` its text `)__<lang>__` (klang.oracle.tsv). A file is
// read in the language its extension names; a block passes the text inside it to the parser of the language it names,
// and that language's rules govern everything inside it. `)__<lang>__` has no other use than to end the block, so the
// block's end is found by it alone, whatever the text inside contains; a block of a language inside a block of the same
// language is counted, each `__<lang>__(` against each `)__<lang>__`. Plain C, every function inline, so that every
// reader, the engine's among them, implements the one rule
#ifndef KLANG_H
#define KLANG_H

#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stddef.h>

// the max characters of a language's name
#define KLANG_NAME 32u

// One block found: where its opener starts and where its text starts. Then where its text ends and where its closer
// ends, each a place in the text searched
typedef struct
{
    size_t opened;
    size_t text;
    size_t text_end;
    size_t closed;
} KlangBlock;

// the place of the first `token`, the `token_length` characters of it, in the `length` characters at `text` from `at`
// on, or `length` where none is there
static inline size_t klang_token_next(const char *text, size_t length, size_t at, const char *token,
                                      size_t token_length)
{
    const char *const found = (at < length)
                                  ? EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = &text[at], .cap = length - at,
                                               .other = token, .other_cap = token_length, .ci = EMBED_FALSE)
                                  : NULL;
    // a found token lies within the text, at or past `at`
    return (found != NULL) ? (size_t)(found - text) : length;
}

// The first block of `lang` in the `length` characters at `text` from `from` on into `block`: 1, or 0 where none opens
// or the one that opens is never closed. The walk jumps from token to token: an opener inside the block counts one
// deeper, and a closer one less. The search for the next token starts at the end of the token before it
static inline int klang_block_find(const char *text, size_t length, size_t from, const char *lang, KlangBlock *block)
{
    char opener[KLANG_NAME + 8u];
    char closer[KLANG_NAME + 8u];
    const size_t name = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = lang, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if ((name == 0u) || (name >= KLANG_NAME))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = opener, .src = "__", .bytes = 2u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &opener[2], .src = lang, .bytes = name);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &opener[2u + name], .src = "__(", .bytes = 4u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = closer, .src = ")__", .bytes = 3u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &closer[3], .src = lang, .bytes = name);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &closer[3u + name], .src = "__", .bytes = 3u);
    const size_t opener_length = name + 5u;
    const size_t closer_length = name + 5u;
    const size_t at = klang_token_next(text, length, from, opener, opener_length);
    if (at == length)
    {
        return 0;
    }
    unsigned int depth = 1u;
    size_t inside = at + opener_length;
    while (inside < length)
    {
        const size_t open = klang_token_next(text, length, inside, opener, opener_length);
        const size_t close = klang_token_next(text, length, inside, closer, closer_length);
        if (close == length)
        {
            return 0;
        }
        if (open < close)
        {
            depth += 1u;
            inside = open + opener_length;
            continue;
        }
        depth -= 1u;
        if (depth == 0u)
        {
            block->opened = at;
            block->text = at + opener_length;
            block->text_end = close;
            block->closed = close + closer_length;
            return 1;
        }
        inside = close + closer_length;
    }
    return 0;
}

#endif
