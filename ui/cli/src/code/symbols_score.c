// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_score.c: how well a query names a name, its characters read whatever their case
#include "../text/regexp.h"
#include "symbols.h"

// the max characters of a query or a name compared
#define CLI_SCORED_CHARACTERS 1024u

typedef struct
{
    unsigned int character;
} SymbolsScoreCtx;
// the bytes a character takes in UTF-8
EMBED_INLINE unsigned long long cli_bytes_get(const SymbolsScoreCtx *args)
{
    if (args->character < 0x80u)
    {
        return 1ULL;
    }
    if (args->character < 0x800u)
    {
        return 2ULL;
    }
    if (args->character < 0x10000u)
    {
        return 3ULL;
    }
    return 4ULL;
}

int cli_symbols_score(const CliSymbolsQueryCfg *args)
{
    long long *points = args->points;
    unsigned int query_characters[CLI_SCORED_CHARACTERS];
    unsigned int characters[CLI_SCORED_CHARACTERS];
    unsigned int lower[CLI_SCORED_CHARACTERS];
    const unsigned long long query_length = cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_POOL_BYTES});
    if (query_length == 0ULL)
    {
        *points = 0LL;
        return 1;
    }
    const unsigned long long query_count =
        EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = args->query, .size = query_length,
                   .characters = query_characters, .capacity_u64 = CLI_SCORED_CHARACTERS);
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = args->name, .size = args->name_length,
                   .characters = characters, .capacity_u64 = CLI_SCORED_CHARACTERS);
    for (unsigned long long at = 0ULL; at < query_count; at += 1ULL)
    {
        query_characters[at] = EMBED_CALL(cli_regexp_character.lower, CliRegexpLetterCfg, .character = query_characters[at]);
    }
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        lower[at] = EMBED_CALL(cli_regexp_character.lower, CliRegexpLetterCfg, .character = characters[at]);
    }
    // a name's length is below what a long long stores
    const long long shortness = -(long long)args->name_length;
    if ((count == query_count) &&
        (memor.cmp(&(MemoriaCfg){.src = lower, .other = query_characters, .bytes = count * sizeof(lower[0])}) == 0))
    {
        const int exact =
            (args->name_length == query_length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->name, .other = args->query, .bytes = args->name_length}) == 0);
        *points = 10000LL + (exact ? 50LL : 0LL);
        return 1;
    }
    if ((count >= query_count) &&
        (memor.cmp(&(MemoriaCfg){.src = lower, .other = query_characters, .bytes = query_count * sizeof(lower[0])}) == 0))
    {
        *points = 8000LL + shortness;
        return 1;
    }
    unsigned long long byte = 0ULL;
    for (unsigned long long at = 0ULL; (at + query_count) <= count; at += 1ULL)
    {
        if (memor.cmp(
                &(MemoriaCfg){.src = lower + at, .other = query_characters, .bytes = query_count * sizeof(lower[0])}) == 0)
        {
            // the place in the lower name's bytes, below what a long long stores
            *points = 6000LL - ((long long)byte * 10LL) + shortness;
            return 1;
        }
        byte += EMBED_CALL(cli_bytes_get, SymbolsScoreCtx, .character = lower[at]);
    }
    // The first character of each word: after `_`, `-`, `.`, a space, or a capital after a small character
    unsigned int initials[CLI_SCORED_CHARACTERS];
    unsigned long long initial_count = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        const unsigned int character = characters[at];
        const unsigned int before = (at != 0ULL) ? characters[at - 1u] : 0u;
        const int starts = (at == 0ULL) || (before == '_') || (before == '-') || (before == '.') || (before == ' ') ||
                           (EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_UPPER,
                                       .character = character) &&
                            EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_LOWER,
                                       .character = before));
        const unsigned int folded = ((character >= 'A') && (character <= 'Z')) ? (character + 32u) : character;
        if (starts && (EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_CHARACTER,
                                  .character = folded) ||
                       EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_NUMBER,
                                  .character = folded)))
        {
            initials[initial_count] = folded;
            initial_count += 1ULL;
        }
    }
    if ((initial_count >= query_count) &&
        (memor.cmp(&(MemoriaCfg){
             .src = initials, .other = query_characters, .bytes = query_count * sizeof(query_characters[0])}) == 0))
    {
        *points = 4000LL + shortness;
        return 1;
    }
    unsigned long long at = 0ULL;
    for (unsigned long long required = 0ULL; required < query_count; required += 1ULL)
    {
        while ((at < count) && (lower[at] != query_characters[required]))
        {
            at += 1ULL;
        }
        if (at >= count)
        {
            return 0;
        }
        at += 1ULL;
    }
    *points = 1000LL + shortness;
    return 1;
}
