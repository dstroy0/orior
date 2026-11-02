// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editorconfig_names.c: a section's pattern, its braces spread one text at a time, each text matched against the file's
// name or path
#include "../text/regexp.h"
#include "../text/text.h"
#include "editorconfig.h"

// A pattern spread: the file's name or path its texts are matched against, the text spread so far, and how many texts
// were matched
typedef struct
{
    const unsigned int *target;
    unsigned long long target_length;
    unsigned int built[CLI_PATH_MAX_LEN];
    unsigned int tried;
} CliSpreading;
// what of a pattern is left to spread after the piece at hand: a piece, and what is left after it
typedef struct CliPending
{
    const unsigned int *at;
    unsigned long long length;
    const struct CliPending *next;
} CliPending;
// the most texts a pattern's braces spread into
#define CLI_SPREAD_MAX 1024u

typedef struct
{
    const unsigned int *pattern;
    unsigned long long pattern_length;
    const unsigned int *text;
    unsigned long long length;
    CliSpreading *spreading;
    unsigned long long *built_pointer;
    const unsigned int *at;
    long long *number;
    unsigned long long built;
    const CliPending *pending;
    unsigned long long open;
    unsigned long long close;
} EditorconfigNamesCtx;
// whether a pattern with its braces spread names the whole of a text
static int cli_glob(const EditorconfigNamesCtx *args)
{
    if (args->pattern_length == 0ull)
    {
        return args->length == 0ull;
    }
    const unsigned int letter = args->pattern[0];
    if ((letter == '*') && (args->pattern_length > 1ull) && (args->pattern[1] == '*'))
    {
        for (unsigned long long at = 0ull; at <= args->length; at += 1ull)
        {
            if (EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + 2u,
                           .pattern_length = args->pattern_length - 2ull, .text = args->text + at,
                           .length = args->length - at))
            {
                return 1;
            }
        }
        return 0;
    }
    if (letter == '*')
    {
        unsigned long long stop = 0ull;
        while ((stop < args->length) && (args->text[stop] != '/'))
        {
            stop += 1ull;
        }
        for (unsigned long long at = 0ull; at <= stop; at += 1ull)
        {
            if (EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + 1u,
                           .pattern_length = args->pattern_length - 1ull, .text = args->text + at,
                           .length = args->length - at))
            {
                return 1;
            }
        }
        return 0;
    }
    if (letter == '?')
    {
        return (args->length != 0ull) && (args->text[0] != '/') &&
               EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + 1u,
                          .pattern_length = args->pattern_length - 1ull, .text = args->text + 1u,
                          .length = args->length - 1ull);
    }
    unsigned long long close = 1ull;
    while ((letter == '[') && (close < args->pattern_length) && (args->pattern[close] != ']'))
    {
        close += 1ull;
    }
    if ((letter == '[') && (close < args->pattern_length))
    {
        const unsigned int *set = args->pattern + 1u;
        unsigned long long set_length = close - 1ull;
        const int negated = (set_length != 0ull) && (set[0] == '!');
        if (negated)
        {
            set += 1u;
            set_length -= 1ull;
        }
        if (args->length == 0ull)
        {
            return 0;
        }
        int inside = 0;
        unsigned long long at = 0ull;
        while (at < set_length)
        {
            if (((at + 2ull) < set_length) && (set[at + 1u] == '-'))
            {
                inside = inside || ((set[at] <= args->text[0]) && (args->text[0] <= set[at + 2u]));
                at += 3ull;
            }
            else
            {
                inside = inside || (set[at] == args->text[0]);
                at += 1ull;
            }
        }
        return (inside != negated) && EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + close + 1u,
                                                 .pattern_length = args->pattern_length - close - 1ull,
                                                 .text = args->text + 1u, .length = args->length - 1ull);
    }
    if ((letter == '\\') && (args->pattern_length > 1ull))
    {
        return (args->length != 0ull) && (args->text[0] == args->pattern[1]) &&
               EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + 2u,
                          .pattern_length = args->pattern_length - 2ull, .text = args->text + 1u,
                          .length = args->length - 1ull);
    }
    return (args->length != 0ull) && (args->text[0] == letter) &&
           EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + 1u,
                      .pattern_length = args->pattern_length - 1ull, .text = args->text + 1u,
                      .length = args->length - 1ull);
}

// `length` letters at `at` put after the text spread so far, which ends at `built`: 1, or 0 where the text would not
// fit and is not matched
EMBED_INLINE int cli_build(const EditorconfigNamesCtx *args)
{
    unsigned long long *built = args->built_pointer;
    if ((*built + args->length) > CLI_PATH_MAX_LEN)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = args->spreading->built + *built, .src = args->at, .bytes = args->length * sizeof(args->at[0])});
    *built += args->length;
    return 1;
}

// the whole number written in `length` letters at `at`, an optional sign before its digits: 1, or 0 where they are not
// one or it is past what a long long stores
EMBED_INLINE int cli_span_is_whole_word(const EditorconfigNamesCtx *args)
{
    long long *number = args->number;
    const int below = (args->length != 0ull) && (args->at[0] == '-');
    const unsigned long long sign =
        ((args->length != 0ull) && ((args->at[0] == '-') || (args->at[0] == '+'))) ? 1ull : 0ull;
    if (args->length == sign)
    {
        return 0;
    }
    // the size of the number stored unsigned, one past the most a long long stores allowed below zero
    unsigned long long size = 0ull;
    const unsigned long long most = below ? 9223372036854775808ull : 9223372036854775807ull;
    for (unsigned long long step = sign; step < args->length; step += 1ull)
    {
        const unsigned int digit = args->at[step] - '0';
        if ((args->at[step] < '0') || (args->at[step] > '9') || (size > ((most - digit) / 10ull)))
        {
            return 0;
        }
        size = (size * 10ull) + digit;
    }
    // the size fits a long long, or is one past its most and taken below zero as the least
    *number = below ? (long long)(0ull - size) : (long long)size;
    return 1;
}

static int cli_spread_match(const EditorconfigNamesCtx *args);

// what is left after a piece spread: the next piece, or the text matched where nothing is
static int cli_spread_on(const EditorconfigNamesCtx *args)
{
    if (args->pending != NULL)
    {
        return EMBED_CALL(cli_spread_match, EditorconfigNamesCtx, .spreading = args->spreading, .built = args->built,
                          .at = args->pending->at, .length = args->pending->length, .pending = args->pending->next);
    }
    args->spreading->tried += 1u;
    return EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->spreading->built, .pattern_length = args->built,
                      .text = args->spreading->target, .length = args->spreading->target_length);
}

// the braces from `open` to `close` of the piece at `at`: the head before them, each choice inside in turn, and the
// tail after each. `{1..3}` is the whole numbers between, `{a,b}` each text between commas spread in its turn, and
// braces with one choice stand as they are
static int cli_brace(const EditorconfigNamesCtx *args)
{
    unsigned long long built = args->built;
    const unsigned int *const inner = args->at + args->open + 1u;
    const unsigned long long inner_length = args->close - args->open - 1ull;
    const CliPending tail = {args->at + args->close + 1u, args->length - args->close - 1ull, args->pending};
    if (!EMBED_CALL(cli_build, EditorconfigNamesCtx, .spreading = args->spreading, .built_pointer = &built,
                    .at = args->at, .length = args->open))
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
    if (((dots + 1ull) < inner_length) &&
        EMBED_CALL(cli_span_is_whole_word, EditorconfigNamesCtx, .at = inner, .length = dots, .number = &from) &&
        EMBED_CALL(cli_span_is_whole_word, EditorconfigNamesCtx, .at = inner + dots + 2u,
                   .length = inner_length - dots - 2ull, .number = &to))
    {
        const long long low = (from < to) ? from : to;
        const long long high = (from < to) ? to : from;
        for (unsigned long long count = 0ull; count < CLI_SPREAD_MAX; count += 1ull)
        {
            // the count stays below the span from low to high, so the sum stays inside a long long
            const long long number = low + (long long)count;
            char digits[24];
            VerbaLinea line = {digits, sizeof(digits), 0ull};
            mmgr_line_decimal_signed(&line, number);
            unsigned int letters[24];
            for (unsigned long long step = 0ull; step < line.at; step += 1ull)
            {
                letters[step] = (unsigned char)digits[step];
            }
            unsigned long long with = built;
            if (EMBED_CALL(cli_build, EditorconfigNamesCtx, .spreading = args->spreading, .built_pointer = &with,
                           .at = letters, .length = line.at) &&
                EMBED_CALL(cli_spread_on, EditorconfigNamesCtx, .spreading = args->spreading, .built = with,
                           .pending = &tail))
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
        return EMBED_CALL(cli_build, EditorconfigNamesCtx, .spreading = args->spreading, .built_pointer = &built,
                          .at = args->at + args->open, .length = args->close - args->open + 1ull) &&
               EMBED_CALL(cli_spread_on, EditorconfigNamesCtx, .spreading = args->spreading, .built = built,
                          .pending = &tail);
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
            if (EMBED_CALL(cli_spread_match, EditorconfigNamesCtx, .spreading = args->spreading, .built = built,
                           .at = inner + part, .length = step - part, .pending = &tail))
            {
                return 1;
            }
            part = step + 1ull;
        }
    }
    return 0;
}

// the piece at `at` spread: its first pair of braces at depth zero, or the piece as it stands where it has none, and
// what is left after it. A backslash escapes nothing here; it is retained for the glob
static int cli_spread_match(const EditorconfigNamesCtx *args)
{
    unsigned long long built = args->built;
    if (args->spreading->tried >= CLI_SPREAD_MAX)
    {
        return 0;
    }
    unsigned long long depth = 0ull;
    unsigned long long open = 0ull;
    for (unsigned long long step = 0ull; step < args->length; step += 1ull)
    {
        if (args->at[step] == '{')
        {
            open = (depth == 0ull) ? step : open;
            depth += 1ull;
        }
        else if ((args->at[step] == '}') && (depth > 0ull))
        {
            depth -= 1ull;
            if (depth == 0ull)
            {
                return EMBED_CALL(cli_brace, EditorconfigNamesCtx, .spreading = args->spreading, .built = built,
                                  .at = args->at, .length = args->length, .open = open, .close = step,
                                  .pending = args->pending);
            }
        }
    }
    return EMBED_CALL(cli_build, EditorconfigNamesCtx, .spreading = args->spreading, .built_pointer = &built,
                      .at = args->at, .length = args->length) &&
           EMBED_CALL(cli_spread_on, EditorconfigNamesCtx, .spreading = args->spreading, .built = built,
                      .pending = args->pending);
}

// By its name alone where the pattern has no `/`, and by its path from the `.editorconfig`'s folder where it has
// one
int cli_editorconfig_names(const CliEditorconfigPatternCfg *args)
{
    static unsigned int s_pattern[CLI_PATH_MAX_LEN];
    static unsigned int s_target[CLI_PATH_MAX_LEN];
    static CliSpreading s_spreading;
    const unsigned long long length =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->pattern, .size = args->size,
                   .letters = s_pattern, .capacity_u64 = CLI_PATH_MAX_LEN);
    unsigned long long slash = 0ull;
    while ((slash < length) && (s_pattern[slash] != '/'))
    {
        slash += 1ull;
    }
    const char *const target = (slash < length) ? args->relative : args->name;
    unsigned long long from = 0ull;
    while ((slash < length) && (from < length) && (s_pattern[from] == '/'))
    {
        from += 1ull;
    }
    s_spreading.target = s_target;
    s_spreading.target_length = EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = target,
                                           .size = cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}),
                                           .letters = s_target, .capacity_u64 = CLI_PATH_MAX_LEN);
    s_spreading.tried = 0u;
    return EMBED_CALL(cli_spread_match, EditorconfigNamesCtx, .spreading = &s_spreading, .built = 0ull,
                      .at = s_pattern + from, .length = length - from, .pending = NULL);
}
