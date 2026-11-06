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
    unsigned long long stop;
    long long low;
    long long high;
} EditorconfigNamesCtx;

static int cli_glob(const EditorconfigNamesCtx *args);

// whether a pattern, `pattern_length` letters at `pattern`, names the whole of what is left of a text, `length` letters
// at `text`, past any of its first `stop` letters, or past none of them
EMBED_INLINE int cli_glob_past_any(const EditorconfigNamesCtx *args)
{
    for (unsigned long long at = 0ULL; at <= args->stop; at += 1ULL)
    {
        if (EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern, .pattern_length = args->pattern_length,
                       .text = args->text + at, .length = args->length - at))
        {
            return 1;
        }
    }
    return 0;
}

// whether `text`'s first letter is in the set of a pattern's `[` at `pattern`, its `]` at `close`: a `!` first
// negating the set, and `a-z` standing for each letter from a to z
EMBED_INLINE int cli_glob_set(const EditorconfigNamesCtx *args)
{
    const unsigned int *set = args->pattern + 1u;
    unsigned long long set_length = args->close - 1ULL;
    const int negated = (set_length != 0ULL) && (set[0] == '!');
    if (negated)
    {
        set += 1u;
        set_length -= 1ULL;
    }
    int inside = 0;
    unsigned long long at = 0ULL;
    while (at < set_length)
    {
        if (((at + 2ULL) < set_length) && (set[at + 1u] == '-'))
        {
            inside = inside || ((set[at] <= args->text[0]) && (args->text[0] <= set[at + 2u]));
            at += 3ULL;
        }
        else
        {
            inside = inside || (set[at] == args->text[0]);
            at += 1ULL;
        }
    }
    return inside != negated;
}

// whether a pattern with its braces spread names the whole of a text
static int cli_glob(const EditorconfigNamesCtx *args)
{
    if (args->pattern_length == 0ULL)
    {
        return args->length == 0ULL;
    }
    const unsigned int letter = args->pattern[0];
    if ((letter == '*') && (args->pattern_length > 1ULL) && (args->pattern[1] == '*'))
    {
        return EMBED_CALL(cli_glob_past_any, EditorconfigNamesCtx, .pattern = args->pattern + 2u,
                          .pattern_length = args->pattern_length - 2ULL, .text = args->text, .length = args->length,
                          .stop = args->length);
    }
    if (letter == '*')
    {
        unsigned long long stop = 0ULL;
        while ((stop < args->length) && (args->text[stop] != '/'))
        {
            stop += 1ULL;
        }
        return EMBED_CALL(cli_glob_past_any, EditorconfigNamesCtx, .pattern = args->pattern + 1u,
                          .pattern_length = args->pattern_length - 1ULL, .text = args->text, .length = args->length,
                          .stop = stop);
    }
    if (letter == '?')
    {
        return (args->length != 0ULL) && (args->text[0] != '/') &&
               EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + 1u,
                          .pattern_length = args->pattern_length - 1ULL, .text = args->text + 1u,
                          .length = args->length - 1ULL);
    }
    unsigned long long close = 1ULL;
    while ((letter == '[') && (close < args->pattern_length) && (args->pattern[close] != ']'))
    {
        close += 1ULL;
    }
    if ((letter == '[') && (close < args->pattern_length))
    {
        return (args->length != 0ULL) &&
               EMBED_CALL(cli_glob_set, EditorconfigNamesCtx, .pattern = args->pattern, .close = close,
                          .text = args->text) &&
               EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + close + 1u,
                          .pattern_length = args->pattern_length - close - 1ULL, .text = args->text + 1u,
                          .length = args->length - 1ULL);
    }
    if ((letter == '\\') && (args->pattern_length > 1ULL))
    {
        return (args->length != 0ULL) && (args->text[0] == args->pattern[1]) &&
               EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + 2u,
                          .pattern_length = args->pattern_length - 2ULL, .text = args->text + 1u,
                          .length = args->length - 1ULL);
    }
    return (args->length != 0ULL) && (args->text[0] == letter) &&
           EMBED_CALL(cli_glob, EditorconfigNamesCtx, .pattern = args->pattern + 1u,
                      .pattern_length = args->pattern_length - 1ULL, .text = args->text + 1u,
                      .length = args->length - 1ULL);
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
    const int below = (args->length != 0ULL) && (args->at[0] == '-');
    const unsigned long long sign =
        ((args->length != 0ULL) && ((args->at[0] == '-') || (args->at[0] == '+'))) ? 1ULL : 0ULL;
    if (args->length == sign)
    {
        return 0;
    }
    // the size of the number stored unsigned, one past the most a long long stores allowed below zero
    unsigned long long size = 0ULL;
    const unsigned long long most = below ? 9223372036854775808ULL : 9223372036854775807ULL;
    for (unsigned long long step = sign; step < args->length; step += 1ULL)
    {
        const unsigned int digit = args->at[step] - '0';
        if ((args->at[step] < '0') || (args->at[step] > '9') || (size > ((most - digit) / 10ULL)))
        {
            return 0;
        }
        size = (size * 10ULL) + digit;
    }
    // the size fits a long long, or is one past its most and taken below zero as the least
    *number = below ? (long long)(0ULL - size) : (long long)size;
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

// how the letter at `at` moves the depth of braces: 1 for an open brace, -1 for a close, else 0
EMBED_INLINE long long cli_depth_step(const EditorconfigNamesCtx *args)
{
    if (args->at[0] == '{')
    {
        return 1LL;
    }
    if (args->at[0] == '}')
    {
        return -1LL;
    }
    return 0LL;
}

// the whole numbers from `low` to `high`, at most CLI_SPREAD_MAX of them, each put in its turn after the text spread
// so far, which ends at `built`, with what is `pending` after the braces: 1 where a text is matched, else 0
EMBED_INLINE int cli_brace_numbers(const EditorconfigNamesCtx *args)
{
    for (unsigned long long count = 0ULL; count < CLI_SPREAD_MAX; count += 1ULL)
    {
        // the count stays below the span from low to high, so the sum stays inside a long long
        const long long number = args->low + (long long)count;
        char digits[24];
        VerbaLinea line = {digits, sizeof(digits), 0ULL};
        mmgr_line_decimal_signed(&line, number);
        unsigned int letters[24];
        for (unsigned long long step = 0ULL; step < line.at; step += 1ULL)
        {
            letters[step] = (unsigned char)digits[step];
        }
        unsigned long long with = args->built;
        if (EMBED_CALL(cli_build, EditorconfigNamesCtx, .spreading = args->spreading, .built_pointer = &with,
                       .at = letters, .length = line.at) &&
            EMBED_CALL(cli_spread_on, EditorconfigNamesCtx, .spreading = args->spreading, .built = with,
                       .pending = args->pending))
        {
            return 1;
        }
        if (number == args->high)
        {
            return 0;
        }
    }
    return 0;
}

// how many choices the inside of braces, `length` letters at `at`, holds: one more than its commas at depth zero
EMBED_INLINE int cli_brace_parts(const EditorconfigNamesCtx *args)
{
    long long depth = 0LL;
    int parts = 1;
    for (unsigned long long step = 0ULL; step < args->length; step += 1ULL)
    {
        depth += EMBED_CALL(cli_depth_step, EditorconfigNamesCtx, .at = args->at + step);
        parts += ((args->at[step] == ',') && (depth == 0LL)) ? 1 : 0;
    }
    return parts;
}

// each choice of the inside of braces, `length` letters at `at`, a text between its commas at depth zero, spread in
// its turn after the text spread so far, which ends at `built`, with what is `pending` after the braces: 1 where a text
// is matched, else 0
EMBED_INLINE int cli_brace_choices(const EditorconfigNamesCtx *args)
{
    long long depth = 0LL;
    unsigned long long part = 0ULL;
    for (unsigned long long step = 0ULL; step <= args->length; step += 1ULL)
    {
        const int ends = (step == args->length) || ((args->at[step] == ',') && (depth == 0LL));
        depth += (step < args->length) ? EMBED_CALL(cli_depth_step, EditorconfigNamesCtx, .at = args->at + step) : 0LL;
        if (ends)
        {
            if (EMBED_CALL(cli_spread_match, EditorconfigNamesCtx, .spreading = args->spreading, .built = args->built,
                           .at = args->at + part, .length = step - part, .pending = args->pending))
            {
                return 1;
            }
            part = step + 1ULL;
        }
    }
    return 0;
}

// the braces from `open` to `close` of the piece at `at`: the head before them, each choice inside in turn, and the
// tail after each. `{1..3}` is the whole numbers between, `{a,b}` each text between commas spread in its turn, and
// braces with one choice stand as they are
static int cli_brace(const EditorconfigNamesCtx *args)
{
    unsigned long long built = args->built;
    const unsigned int *const inner = args->at + args->open + 1u;
    const unsigned long long inner_length = args->close - args->open - 1ULL;
    const CliPending tail = {args->at + args->close + 1u, args->length - args->close - 1ULL, args->pending};
    if (!EMBED_CALL(cli_build, EditorconfigNamesCtx, .spreading = args->spreading, .built_pointer = &built,
                    .at = args->at, .length = args->open))
    {
        return 0;
    }
    unsigned long long dots = 0ULL;
    while (((dots + 1ULL) < inner_length) && !((inner[dots] == '.') && (inner[dots + 1u] == '.')))
    {
        dots += 1ULL;
    }
    long long from = 0LL;
    long long to = 0LL;
    if (((dots + 1ULL) < inner_length) &&
        EMBED_CALL(cli_span_is_whole_word, EditorconfigNamesCtx, .at = inner, .length = dots, .number = &from) &&
        EMBED_CALL(cli_span_is_whole_word, EditorconfigNamesCtx, .at = inner + dots + 2u,
                   .length = inner_length - dots - 2ULL, .number = &to))
    {
        return EMBED_CALL(cli_brace_numbers, EditorconfigNamesCtx, .spreading = args->spreading, .built = built,
                          .low = (from < to) ? from : to, .high = (from < to) ? to : from, .pending = &tail);
    }
    if (EMBED_CALL(cli_brace_parts, EditorconfigNamesCtx, .at = inner, .length = inner_length) == 1)
    {
        return EMBED_CALL(cli_build, EditorconfigNamesCtx, .spreading = args->spreading, .built_pointer = &built,
                          .at = args->at + args->open, .length = args->close - args->open + 1ULL) &&
               EMBED_CALL(cli_spread_on, EditorconfigNamesCtx, .spreading = args->spreading, .built = built,
                          .pending = &tail);
    }
    return EMBED_CALL(cli_brace_choices, EditorconfigNamesCtx, .spreading = args->spreading, .built = built,
                      .at = inner, .length = inner_length, .pending = &tail);
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
    unsigned long long depth = 0ULL;
    unsigned long long open = 0ULL;
    for (unsigned long long step = 0ULL; step < args->length; step += 1ULL)
    {
        if (args->at[step] == '{')
        {
            open = (depth == 0ULL) ? step : open;
            depth += 1ULL;
        }
        else if ((args->at[step] == '}') && (depth > 0ULL))
        {
            depth -= 1ULL;
            if (depth == 0ULL)
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
    unsigned long long slash = 0ULL;
    while ((slash < length) && (s_pattern[slash] != '/'))
    {
        slash += 1ULL;
    }
    const char *const target = (slash < length) ? args->relative : args->name;
    unsigned long long from = 0ULL;
    while ((slash < length) && (from < length) && (s_pattern[from] == '/'))
    {
        from += 1ULL;
    }
    s_spreading.target = s_target;
    s_spreading.target_length = EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = target,
                                           .size = cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_POOL_BYTES}),
                                           .letters = s_target, .capacity_u64 = CLI_PATH_MAX_LEN);
    s_spreading.tried = 0u;
    return EMBED_CALL(cli_spread_match, EditorconfigNamesCtx, .spreading = &s_spreading, .built = 0ULL,
                      .at = s_pattern + from, .length = length - from, .pending = NULL);
}
