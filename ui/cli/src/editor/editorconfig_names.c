// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editorconfig_names.c: a section's pattern, its braces spread one text at a time, each text held against the file's
// name or path
#include "../text/regexp.h"
#include "editorconfig.h"

// the most texts a pattern's braces spread into
#define CLI_SPREAD_MOST 1024u

// A pattern spread: the file's name or path its texts are held against, the text spread so far, and how many texts
// were held
typedef struct
{
    const unsigned int *target;
    unsigned long long target_length;
    unsigned int built[CLI_PATH_LONGEST];
    unsigned int tried;
} CliSpreading;

// what of a pattern is left to spread after the piece at hand: a piece, and what is left after it
typedef struct CliPending
{
    const unsigned int *at;
    unsigned long long length;
    const struct CliPending *next;
} CliPending;

// whether a pattern with its braces spread names the whole of a text
static int cli_glob(const unsigned int *pattern, unsigned long long pattern_length, const unsigned int *text,
                    unsigned long long length)
{
    if (pattern_length == 0ull)
    {
        return length == 0ull;
    }
    const unsigned int letter = pattern[0];
    if ((letter == '*') && (pattern_length > 1ull) && (pattern[1] == '*'))
    {
        for (unsigned long long at = 0ull; at <= length; at += 1ull)
        {
            if (cli_glob(pattern + 2u, pattern_length - 2ull, text + at, length - at))
            {
                return 1;
            }
        }
        return 0;
    }
    if (letter == '*')
    {
        unsigned long long stop = 0ull;
        while ((stop < length) && (text[stop] != '/'))
        {
            stop += 1ull;
        }
        for (unsigned long long at = 0ull; at <= stop; at += 1ull)
        {
            if (cli_glob(pattern + 1u, pattern_length - 1ull, text + at, length - at))
            {
                return 1;
            }
        }
        return 0;
    }
    if (letter == '?')
    {
        return (length != 0ull) && (text[0] != '/') &&
               cli_glob(pattern + 1u, pattern_length - 1ull, text + 1u, length - 1ull);
    }
    unsigned long long close = 1ull;
    while ((letter == '[') && (close < pattern_length) && (pattern[close] != ']'))
    {
        close += 1ull;
    }
    if ((letter == '[') && (close < pattern_length))
    {
        const unsigned int *set = pattern + 1u;
        unsigned long long set_length = close - 1ull;
        const int negated = (set_length != 0ull) && (set[0] == '!');
        if (negated)
        {
            set += 1u;
            set_length -= 1ull;
        }
        if (length == 0ull)
        {
            return 0;
        }
        int inside = 0;
        unsigned long long at = 0ull;
        while (at < set_length)
        {
            if (((at + 2ull) < set_length) && (set[at + 1u] == '-'))
            {
                inside = inside || ((set[at] <= text[0]) && (text[0] <= set[at + 2u]));
                at += 3ull;
            }
            else
            {
                inside = inside || (set[at] == text[0]);
                at += 1ull;
            }
        }
        return (inside != negated) &&
               cli_glob(pattern + close + 1u, pattern_length - close - 1ull, text + 1u, length - 1ull);
    }
    if ((letter == '\\') && (pattern_length > 1ull))
    {
        return (length != 0ull) && (text[0] == pattern[1]) &&
               cli_glob(pattern + 2u, pattern_length - 2ull, text + 1u, length - 1ull);
    }
    return (length != 0ull) && (text[0] == letter) &&
           cli_glob(pattern + 1u, pattern_length - 1ull, text + 1u, length - 1ull);
}

// `length` letters at `at` put after the text spread so far, which ends at `built`: 1, or 0 where the text would not
// fit and is not held
static int cli_built(CliSpreading *spreading, unsigned long long *built, const unsigned int *at,
                     unsigned long long length)
{
    if ((*built + length) > CLI_PATH_LONGEST)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = spreading->built + *built, .src = at, .bytes = length * sizeof(at[0])});
    *built += length;
    return 1;
}

// the whole number written in `length` letters at `at`, an optional sign before its digits: 1, or 0 where they are not
// one or it is past what a long long holds
static int cli_whole(const unsigned int *at, unsigned long long length, long long *number)
{
    const int below = (length != 0ull) && (at[0] == '-');
    const unsigned long long sign = ((length != 0ull) && ((at[0] == '-') || (at[0] == '+'))) ? 1ull : 0ull;
    if (length == sign)
    {
        return 0;
    }
    // the size of the number kept unsigned, one past the most a long long holds allowed below zero
    unsigned long long size = 0ull;
    const unsigned long long most = below ? 9223372036854775808ull : 9223372036854775807ull;
    for (unsigned long long step = sign; step < length; step += 1ull)
    {
        const unsigned int digit = at[step] - '0';
        if ((at[step] < '0') || (at[step] > '9') || (size > ((most - digit) / 10ull)))
        {
            return 0;
        }
        size = (size * 10ull) + digit;
    }
    // the size fits a long long, or is one past its most and taken below zero as the least
    *number = below ? (long long)(0ull - size) : (long long)size;
    return 1;
}

static int cli_spread_matched(CliSpreading *spreading, unsigned long long built, const unsigned int *at,
                              unsigned long long length, const CliPending *pending);

// what is left after a piece spread: the next piece, or the text held where nothing is
static int cli_spread_on(CliSpreading *spreading, unsigned long long built, const CliPending *pending)
{
    if (pending != NULL)
    {
        return cli_spread_matched(spreading, built, pending->at, pending->length, pending->next);
    }
    spreading->tried += 1u;
    return cli_glob(spreading->built, built, spreading->target, spreading->target_length);
}

// the braces from `open` to `close` of the piece at `at`: the head before them, each choice inside in turn, and the
// tail after each. `{1..3}` is the whole numbers between, `{a,b}` each text between commas spread in its turn, and
// braces with one choice stand as they are
static int cli_braced(CliSpreading *spreading, unsigned long long built, const unsigned int *at,
                      unsigned long long length, unsigned long long open, unsigned long long close,
                      const CliPending *pending)
{
    const unsigned int *const inner = at + open + 1u;
    const unsigned long long inner_length = close - open - 1ull;
    const CliPending tail = {at + close + 1u, length - close - 1ull, pending};
    if (!cli_built(spreading, &built, at, open))
    {
        return 0;
    }
    unsigned long long dots = 0ull;
    while (((dots + 1ull) < inner_length) && !((inner[dots] == '.') && (inner[dots + 1u] == '.')))
    {
        dots += 1ull;
    }
    long long from = 0ll;
    long long to = 0ll;
    if (((dots + 1ull) < inner_length) && cli_whole(inner, dots, &from) &&
        cli_whole(inner + dots + 2u, inner_length - dots - 2ull, &to))
    {
        const long long low = (from < to) ? from : to;
        const long long high = (from < to) ? to : from;
        for (unsigned long long count = 0ull; count < CLI_SPREAD_MOST; count += 1ull)
        {
            // the count stays below the span from low to high, so the sum stays inside a long long
            const long long number = low + (long long)count;
            char digits[24];
            CliLine line = {digits, sizeof(digits), 0ull};
            cli_line_signed(&line, number);
            unsigned int letters[24];
            for (unsigned long long step = 0ull; step < line.at; step += 1ull)
            {
                letters[step] = (unsigned char)digits[step];
            }
            unsigned long long with = built;
            if (cli_built(spreading, &with, letters, line.at) && cli_spread_on(spreading, with, &tail))
            {
                return 1;
            }
            if (number == high)
            {
                break;
            }
        }
        return 0;
    }
    long long depth = 0ll;
    unsigned long long part = 0ull;
    int parts = 1;
    for (unsigned long long step = 0ull; step < inner_length; step += 1ull)
    {
        depth += (inner[step] == '{') ? 1ll : (inner[step] == '}') ? -1ll : 0ll;
        parts += ((inner[step] == ',') && (depth == 0ll)) ? 1 : 0;
    }
    if (parts == 1)
    {
        return cli_built(spreading, &built, at + open, close - open + 1ull) && cli_spread_on(spreading, built, &tail);
    }
    depth = 0ll;
    for (unsigned long long step = 0ull; step <= inner_length; step += 1ull)
    {
        const int ends = (step == inner_length) || ((inner[step] == ',') && (depth == 0ll));
        if (step < inner_length)
        {
            depth += (inner[step] == '{') ? 1ll : (inner[step] == '}') ? -1ll : 0ll;
        }
        if (ends)
        {
            if (cli_spread_matched(spreading, built, inner + part, step - part, &tail))
            {
                return 1;
            }
            part = step + 1ull;
        }
    }
    return 0;
}

// the piece at `at` spread: its first pair of braces at depth zero, or the piece as it stands where it has none, and
// what is left after it. A backslash escapes nothing here; it is kept for the glob
static int cli_spread_matched(CliSpreading *spreading, unsigned long long built, const unsigned int *at,
                              unsigned long long length, const CliPending *pending)
{
    if (spreading->tried >= CLI_SPREAD_MOST)
    {
        return 0;
    }
    unsigned long long depth = 0ull;
    unsigned long long open = 0ull;
    for (unsigned long long step = 0ull; step < length; step += 1ull)
    {
        if (at[step] == '{')
        {
            open = (depth == 0ull) ? step : open;
            depth += 1ull;
        }
        else if ((at[step] == '}') && (depth > 0ull))
        {
            depth -= 1ull;
            if (depth == 0ull)
            {
                return cli_braced(spreading, built, at, length, open, step, pending);
            }
        }
    }
    return cli_built(spreading, &built, at, length) && cli_spread_on(spreading, built, pending);
}

// By its name alone where the pattern has no `/`, and by its path from the `.editorconfig`'s folder where it has
// one
int cli_editorconfig_names(const char *pattern, unsigned long long size, const char *relative, const char *name)
{
    static unsigned int s_pattern[CLI_PATH_LONGEST];
    static unsigned int s_target[CLI_PATH_LONGEST];
    static CliSpreading s_spreading;
    const unsigned long long length = cli_regexp_letters(pattern, size, s_pattern, CLI_PATH_LONGEST);
    unsigned long long slash = 0ull;
    while ((slash < length) && (s_pattern[slash] != '/'))
    {
        slash += 1ull;
    }
    const char *const target = (slash < length) ? relative : name;
    unsigned long long from = 0ull;
    while ((slash < length) && (from < length) && (s_pattern[from] == '/'))
    {
        from += 1ull;
    }
    s_spreading.target = s_target;
    s_spreading.target_length = cli_regexp_letters(
        target, cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}), s_target, CLI_PATH_LONGEST);
    s_spreading.tried = 0u;
    return cli_spread_matched(&s_spreading, 0ull, s_pattern + from, length - from, NULL);
}
