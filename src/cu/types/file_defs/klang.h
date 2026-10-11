// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klang.h: a block of another language in a file, `__<lang>__(` its text `)__<lang>__` (klang.oracle.tsv). A file is
// read in the language its extension names; a block hands the text inside it to the parser of the language it names,
// and that language's rules govern everything inside it. `)__<lang>__` has no other use than to end the block, so the
// block's end is found by it alone, whatever the text inside holds; a block of a language inside a block of the same
// language is counted, each `__<lang>__(` against each `)__<lang>__`. Plain C, every function inline, so that every
// reader, the engine's among them, holds the one rule
#ifndef KLANG_H
#define KLANG_H

#include <stddef.h>
#include <string.h>

// the most letters of a language's name
#define KLANG_NAME 32u

// One block found: where its opener starts and where its text starts, where its text ends and where its closer ends,
// each a place in the text searched
typedef struct
{
    size_t opened;
    size_t text;
    size_t text_end;
    size_t closed;
} KlangBlock;

// 1 where the `length` letters at `at` are `token`, the `token_length` of them
static inline int klang_token_at(const char *text, size_t length, size_t at, const char *token, size_t token_length)
{
    return ((at + token_length) <= length) && (memcmp(&text[at], token, token_length) == 0);
}

// The first block of `lang` in the `length` letters at `text` from `from` on into `block`: 1, or 0 where none opens or
// the one that opens is never closed
static inline int klang_block_find(const char *text, size_t length, size_t from, const char *lang, KlangBlock *block)
{
    char opener[KLANG_NAME + 8u];
    char closer[KLANG_NAME + 8u];
    const size_t name = strlen(lang);
    if ((name == 0u) || (name >= KLANG_NAME))
    {
        return 0;
    }
    memcpy(opener, "__", 2u);
    memcpy(&opener[2], lang, name);
    memcpy(&opener[2u + name], "__(", 4u);
    memcpy(closer, ")__", 3u);
    memcpy(&closer[3], lang, name);
    memcpy(&closer[3u + name], "__", 3u);
    const size_t opener_length = name + 5u;
    const size_t closer_length = name + 5u;
    for (size_t at = from; at < length; at += 1u)
    {
        if (!klang_token_at(text, length, at, opener, opener_length))
        {
            continue;
        }
        unsigned int depth = 1u;
        for (size_t inside = at + opener_length; inside < length; inside += 1u)
        {
            if (klang_token_at(text, length, inside, opener, opener_length))
            {
                depth += 1u;
                inside += opener_length - 1u;
                continue;
            }
            if (!klang_token_at(text, length, inside, closer, closer_length))
            {
                continue;
            }
            depth -= 1u;
            if (depth == 0u)
            {
                block->opened = at;
                block->text = at + opener_length;
                block->text_end = inside;
                block->closed = inside + closer_length;
                return 1;
            }
            inside += closer_length - 1u;
        }
        return 0;
    }
    return 0;
}

#endif
