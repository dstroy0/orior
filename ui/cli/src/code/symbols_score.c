// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_score.c: how well a query names a name, its letters read whatever their case
#include "../text/regexp.h"
#include "symbols.h"

// the most letters of a query or a name compared
#define CLI_SCORED_LETTERS 1024u

typedef struct
{
    unsigned int letter;
} SymbolsScoreCtx;
// the bytes a letter takes in UTF-8
EMBED_INLINE unsigned long long cli_bytes_get(const SymbolsScoreCtx *args)
{
    return (args->letter < 0x80u) ? 1ull : (args->letter < 0x800u) ? 2ull : (args->letter < 0x10000u) ? 3ull : 4ull;
}

int cli_symbols_score(const CliSymbolsQueryCfg *args)
{
    long long *points = args->points;
    unsigned int query_letters[CLI_SCORED_LETTERS];
    unsigned int letters[CLI_SCORED_LETTERS];
    unsigned int lower[CLI_SCORED_LETTERS];
    const unsigned long long query_length = cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_CELLS_POOL});
    if (query_length == 0ull)
    {
        *points = 0ll;
        return 1;
    }
    const unsigned long long query_count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->query, .size = query_length,
                   .letters = query_letters, .capacity_u64 = CLI_SCORED_LETTERS);
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->name, .size = args->name_length,
                   .letters = letters, .capacity_u64 = CLI_SCORED_LETTERS);
    for (unsigned long long at = 0ull; at < query_count; at += 1ull)
    {
        query_letters[at] = EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = query_letters[at]);
    }
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        lower[at] = EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = letters[at]);
    }
    // a name's length is below what a long long stores
    const long long shortness = -(long long)args->name_length;
    if ((count == query_count) &&
        (memor.cmp(&(MemoriaCfg){.src = lower, .other = query_letters, .bytes = count * sizeof(lower[0])}) == 0))
    {
        const int exact =
            (args->name_length == query_length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->name, .other = args->query, .bytes = args->name_length}) == 0);
        *points = 10000ll + (exact ? 50ll : 0ll);
        return 1;
    }
    if ((count >= query_count) &&
        (memor.cmp(&(MemoriaCfg){.src = lower, .other = query_letters, .bytes = query_count * sizeof(lower[0])}) == 0))
    {
        *points = 8000ll + shortness;
        return 1;
    }
    unsigned long long byte = 0ull;
    for (unsigned long long at = 0ull; (at + query_count) <= count; at += 1ull)
    {
        if (memor.cmp(
                &(MemoriaCfg){.src = lower + at, .other = query_letters, .bytes = query_count * sizeof(lower[0])}) == 0)
        {
            // the place in the lower name's bytes, below what a long long stores
            *points = 6000ll - ((long long)byte * 10ll) + shortness;
            return 1;
        }
        byte += EMBED_CALL(cli_bytes_get, SymbolsScoreCtx, .letter = lower[at]);
    }
    // The first letter of each word: after `_`, `-`, `.`, a space, or a capital after a small letter
    unsigned int initials[CLI_SCORED_LETTERS];
    unsigned long long initial_count = 0ull;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned int letter = letters[at];
        const unsigned int before = (at != 0ull) ? letters[at - 1u] : 0u;
        const int starts = (at == 0ull) || (before == '_') || (before == '-') || (before == '.') || (before == ' ') ||
                           (EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_UPPER,
                                       .letter = letter) &&
                            EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_LOWER,
                                       .letter = before));
        const unsigned int folded = ((letter >= 'A') && (letter <= 'Z')) ? (letter + 32u) : letter;
        if (starts && (EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_LETTER,
                                  .letter = folded) ||
                       EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_NUMBER,
                                  .letter = folded)))
        {
            initials[initial_count] = folded;
            initial_count += 1ull;
        }
    }
    if ((initial_count >= query_count) &&
        (memor.cmp(&(MemoriaCfg){
             .src = initials, .other = query_letters, .bytes = query_count * sizeof(query_letters[0])}) == 0))
    {
        *points = 4000ll + shortness;
        return 1;
    }
    unsigned long long at = 0ull;
    for (unsigned long long required = 0ull; required < query_count; required += 1ull)
    {
        while ((at < count) && (lower[at] != query_letters[required]))
        {
            at += 1ull;
        }
        if (at >= count)
        {
            return 0;
        }
        at += 1ull;
    }
    *points = 1000ll + shortness;
    return 1;
}
