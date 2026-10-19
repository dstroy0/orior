// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_score.c: how well a query names a name, its letters read whatever their case
#include "../text/regexp.h"
#include "symbols.h"

// the most letters of a query or a name compared
#define CLI_SCORED_LETTERS 1024u

// the bytes a letter takes in UTF-8
static unsigned long long cli_bytes_of(unsigned int letter)
{
    return (letter < 0x80u) ? 1ull : (letter < 0x800u) ? 2ull : (letter < 0x10000u) ? 3ull : 4ull;
}

int cli_symbols_score(const char *query, const char *name, unsigned long long name_length, long long *points)
{
    unsigned int asked[CLI_SCORED_LETTERS];
    unsigned int letters[CLI_SCORED_LETTERS];
    unsigned int lower[CLI_SCORED_LETTERS];
    const unsigned long long query_length = cellul.len(&(CatenaFinitaCfg){.src = query, .cap = CLI_CELLS_POOL});
    if (query_length == 0ull)
    {
        *points = 0ll;
        return 1;
    }
    const unsigned long long asked_count = cli_regexp_letters(query, query_length, asked, CLI_SCORED_LETTERS);
    const unsigned long long count = cli_regexp_letters(name, name_length, letters, CLI_SCORED_LETTERS);
    for (unsigned long long at = 0ull; at < asked_count; at += 1ull)
    {
        asked[at] = cli_regexp_lower(asked[at]);
    }
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        lower[at] = cli_regexp_lower(letters[at]);
    }
    // a name's length is below what a long long holds
    const long long shortness = -(long long)name_length;
    if ((count == asked_count) &&
        (memor.cmp(&(MemoriaCfg){.src = lower, .other = asked, .bytes = count * sizeof(lower[0])}) == 0))
    {
        const int exact = (name_length == query_length) &&
                          (memor.cmp(&(MemoriaCfg){.src = name, .other = query, .bytes = name_length}) == 0);
        *points = 10000ll + (exact ? 50ll : 0ll);
        return 1;
    }
    if ((count >= asked_count) &&
        (memor.cmp(&(MemoriaCfg){.src = lower, .other = asked, .bytes = asked_count * sizeof(lower[0])}) == 0))
    {
        *points = 8000ll + shortness;
        return 1;
    }
    unsigned long long byte = 0ull;
    for (unsigned long long at = 0ull; (at + asked_count) <= count; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = lower + at, .other = asked, .bytes = asked_count * sizeof(lower[0])}) == 0)
        {
            // the place in the lower name's bytes, below what a long long holds
            *points = 6000ll - ((long long)byte * 10ll) + shortness;
            return 1;
        }
        byte += cli_bytes_of(lower[at]);
    }
    // The first letter of each word: after `_`, `-`, `.`, a space, or a capital after a small letter
    unsigned int initials[CLI_SCORED_LETTERS];
    unsigned long long initial_count = 0ull;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned int letter = letters[at];
        const unsigned int before = (at != 0ull) ? letters[at - 1u] : 0u;
        const int starts =
            (at == 0ull) || (before == '_') || (before == '-') || (before == '.') || (before == ' ') ||
            (cli_regexp_property(CLI_REGEXP_UPPER, letter) && cli_regexp_property(CLI_REGEXP_LOWER, before));
        const unsigned int folded = ((letter >= 'A') && (letter <= 'Z')) ? (letter + 32u) : letter;
        if (starts &&
            (cli_regexp_property(CLI_REGEXP_LETTER, folded) || cli_regexp_property(CLI_REGEXP_NUMBER, folded)))
        {
            initials[initial_count] = folded;
            initial_count += 1ull;
        }
    }
    if ((initial_count >= asked_count) &&
        (memor.cmp(&(MemoriaCfg){.src = initials, .other = asked, .bytes = asked_count * sizeof(asked[0])}) == 0))
    {
        *points = 4000ll + shortness;
        return 1;
    }
    unsigned long long at = 0ull;
    for (unsigned long long wanted = 0ull; wanted < asked_count; wanted += 1ull)
    {
        while ((at < count) && (lower[at] != asked[wanted]))
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
