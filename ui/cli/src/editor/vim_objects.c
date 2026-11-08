// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vim_objects.c: Vim's text objects, the span each takes about a place: a word or a word of every letter but a blank on
// the line, a quoted text on the line, or a bracketed text across the lines, inside its marks for i and with them for
// a, a word's or a quote's a taking the blanks after it or, where none follow, those before it
#include "vim_internal.h"

#include "../text/text.h"

// the brackets each object's letter takes, the opener then the closer, and the quotes
static const struct
{
    unsigned int of;
    unsigned int open;
    unsigned int close;
} s_pairs[] = {{'(', '(', ')'}, {')', '(', ')'}, {'b', '(', ')'}, {'[', '[', ']'}, {']', '[', ']'},
               {'{', '{', '}'}, {'}', '{', '}'}, {'B', '{', '}'}, {'<', '<', '>'}, {'>', '<', '>'}};

// whether a unit is a blank
EMBED_INLINE int objects_blank(const CliVimCtx *args)
{
    return EMBED_CALL(cli_text_table.white, CliTextCfg, .letter = args->letter);
}

// the word at `from`, a `big` one with every letter but a blank, on its line, into `span`
EMBED_INLINE void objects_word(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = args->from.line, .length_out = &length);
    if ((units == NULL) || (length == 0ULL))
    {
        return;
    }
    const unsigned long long at = (args->from.col < length - 1ULL) ? args->from.col : length - 1ULL;
    const unsigned int sort =
        EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .letter = units[at], .big = args->big);
    unsigned long long start = at;
    unsigned long long end = at + 1ULL;
    while ((start > 0ULL) && (EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .letter = units[start - 1ULL],
                                         .big = args->big) == sort))
    {
        start -= 1ULL;
    }
    while ((end < length) &&
           (EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .letter = units[end], .big = args->big) == sort))
    {
        end += 1ULL;
    }
    if (args->how == 'a')
    {
        unsigned long long after = end;
        while ((after < length) && EMBED_CALL(objects_blank, CliVimCtx, .letter = units[after]))
        {
            after += 1ULL;
        }
        if (after > end)
        {
            end = after;
        }
        else
        {
            while ((start > 0ULL) && EMBED_CALL(objects_blank, CliVimCtx, .letter = units[start - 1ULL]))
            {
                start -= 1ULL;
            }
        }
    }
    *args->span = (CliVimSpan){1, {args->from.line, start}, {args->from.line, end}, 0};
}

// the text the quote `letter` marks about `from` on its line, a quote after a backslash not one, into `span`
EMBED_INLINE void objects_quoted(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = args->from.line, .length_out = &length);
    int opened = 0;
    unsigned long long open = 0ULL;
    for (unsigned long long at = 0ULL; (units != NULL) && (at < length); at += 1ULL)
    {
        if ((units[at] != args->letter) || ((at > 0ULL) && (units[at - 1ULL] == '\\')))
        {
            continue;
        }
        if (!opened)
        {
            opened = 1;
            open = at;
            continue;
        }
        opened = 0;
        if (args->from.col > at)
        {
            continue;
        }
        if (args->how == 'i')
        {
            *args->span = (CliVimSpan){1, {args->from.line, open + 1ULL}, {args->from.line, at}, 0};
            return;
        }
        unsigned long long start = open;
        unsigned long long end = at + 1ULL;
        while ((end < length) && EMBED_CALL(objects_blank, CliVimCtx, .letter = units[end]))
        {
            end += 1ULL;
        }
        if (end == at + 1ULL)
        {
            while ((start > 0ULL) && EMBED_CALL(objects_blank, CliVimCtx, .letter = units[start - 1ULL]))
            {
                start -= 1ULL;
            }
        }
        *args->span = (CliVimSpan){1, {args->from.line, start}, {args->from.line, end}, 0};
        return;
    }
}

// the opener `letter` that holds `place`, the one at it where it stands there and `on`, a closer `how` at `place`
// itself counted out where `on` is 0: 1 with it into `place_out`, or 0
EMBED_INLINE int objects_opener(const CliVimCtx *args)
{
    CliPosition back = args->place;
    int more = 1;
    if (!(args->on && (EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key,
                                  .place = args->place) == args->letter)))
    {
        more = EMBED_CALL(cli_vim_motions_internal.backward, CliVimCtx, .key = args->key, .place = args->place,
                          .place_out = &back);
    }
    unsigned long long depth = 0ULL;
    while (more)
    {
        const unsigned int letter =
            EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key, .place = back);
        const int at_from = (back.line == args->from.line) && (back.col == args->from.col);
        if ((letter == args->how) && !(args->on && at_from))
        {
            depth += 1ULL;
        }
        else if (letter == args->letter)
        {
            if (depth == 0ULL)
            {
                *args->place_out = back;
                return 1;
            }
            depth -= 1ULL;
        }
        more = EMBED_CALL(cli_vim_motions_internal.backward, CliVimCtx, .key = args->key, .place = back,
                          .place_out = &back);
    }
    return 0;
}

// the text the brackets of `letter` hold about `from`, `count` levels out, into `span`
EMBED_INLINE void objects_bracketed(const CliVimCtx *args)
{
    unsigned int open_letter = 0u;
    unsigned int close_letter = 0u;
    for (unsigned int at = 0u; at < sizeof(s_pairs) / sizeof(s_pairs[0]); at += 1u)
    {
        if (s_pairs[at].of == args->letter)
        {
            open_letter = s_pairs[at].open;
            close_letter = s_pairs[at].close;
        }
    }
    if (open_letter == 0u)
    {
        return;
    }
    const unsigned long long levels = args->counted ? args->count : 1ULL;
    CliPosition at = args->from;
    CliPosition open = args->from;
    for (unsigned long long level = 0ULL; level < levels; level += 1ULL)
    {
        if (!EMBED_CALL(objects_opener, CliVimCtx, .key = args->key, .place = at, .from = args->from,
                        .letter = open_letter, .how = close_letter, .on = level == 0ULL, .place_out = &open))
        {
            return;
        }
        at = open;
        if ((level + 1ULL < levels) && !EMBED_CALL(cli_vim_motions_internal.backward, CliVimCtx, .key = args->key,
                                                   .place = open, .place_out = &at))
        {
            at = open;
        }
    }
    unsigned long long depth = 0ULL;
    CliPosition ahead = open;
    int more =
        EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = open, .place_out = &ahead);
    while (more)
    {
        const unsigned int letter =
            EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key, .place = ahead);
        if (letter == open_letter)
        {
            depth += 1ULL;
        }
        else if ((letter == close_letter) && (depth == 0ULL))
        {
            break;
        }
        else if (letter == close_letter)
        {
            depth -= 1ULL;
        }
        more = EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = ahead,
                          .place_out = &ahead);
    }
    if (!more)
    {
        return;
    }
    CliPosition inside = open;
    (void)EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = open,
                     .place_out = &inside);
    CliPosition past = ahead;
    (void)EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = ahead, .place_out = &past);
    *args->span = (args->how == 'i') ? (CliVimSpan){1, inside, ahead, 0} : (CliVimSpan){1, open, past, 0};
}

void cli_vim_object(const CliVimCtx *args)
{
    *args->span = (CliVimSpan){0, args->from, args->from, 0};
    const unsigned int of = args->letter;
    if ((of == 'w') || (of == 'W'))
    {
        EMBED_CALL(objects_word, CliVimCtx, .key = args->key, .from = args->from, .how = args->how, .big = of == 'W',
                   .span = args->span);
        return;
    }
    if ((of == '"') || (of == '\'') || (of == '`'))
    {
        EMBED_CALL(objects_quoted, CliVimCtx, .key = args->key, .from = args->from, .how = args->how, .letter = of,
                   .span = args->span);
        return;
    }
    EMBED_CALL(objects_bracketed, CliVimCtx, .key = args->key, .from = args->from, .how = args->how, .letter = of,
               .count = args->count, .counted = args->counted, .span = args->span);
}
