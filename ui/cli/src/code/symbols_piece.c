// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_piece.c: a piece of a line read: trimmed, matched against a word, and the name it opens with
#include "symbols_internal.h"

void cli_piece_put(const CliOut *out, CliPiece name, const char *kind)
{
    out->each(out->context, name.at, name.length, kind, out->line);
}

int cli_piece_space(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

int cli_piece_alpha(char letter)
{
    return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z'));
}

int cli_piece_alnum(char letter)
{
    return cli_piece_alpha(letter) || ((letter >= '0') && (letter <= '9'));
}

CliPiece cli_piece_trim_start(CliPiece text)
{
    while ((text.length != 0ull) && cli_piece_space(text.at[0]))
    {
        text.at += 1;
        text.length -= 1ull;
    }
    return text;
}

CliPiece cli_piece_trim_end(CliPiece text)
{
    while ((text.length != 0ull) && cli_piece_space(text.at[text.length - 1u]))
    {
        text.length -= 1ull;
    }
    return text;
}

CliPiece cli_piece_from(CliPiece text, unsigned long long at)
{
    const CliPiece rest = {text.at + at, text.length - at};
    return rest;
}

int cli_piece_starts(CliPiece text, const char *prefix)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
    return (text.length >= length) && (memor.cmp(&(MemoriaCfg){.src = text.at, .other = prefix, .bytes = length}) == 0);
}

int cli_piece_equals(CliPiece text, const char *word)
{
    return (text.length == cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL})) &&
           cli_piece_starts(text, word);
}

// the first place `letter` stands in a text, or its length where none
unsigned long long cli_piece_find(CliPiece text, char letter)
{
    unsigned long long at = 0ull;
    while ((at < text.length) && (text.at[at] != letter))
    {
        at += 1ull;
    }
    return at;
}

// The name a line starts with, as a language writes names: a letter or `_` first, then letters, digits and `_`, with
// `$` too where `dollar`; the rest of the line after it into `rest`. 1, or 0 where none starts it
int cli_piece_name_at(CliPiece text, int dollar, CliPiece *name, CliPiece *rest)
{
    if ((text.length == 0ull) ||
        !(cli_piece_alpha(text.at[0]) || (text.at[0] == '_') || (dollar && (text.at[0] == '$'))))
    {
        return 0;
    }
    unsigned long long end = 0ull;
    while ((end < text.length) &&
           (cli_piece_alnum(text.at[end]) || (text.at[end] == '_') || (dollar && (text.at[end] == '$'))))
    {
        end += 1ull;
    }
    name->at = text.at;
    name->length = end;
    *rest = cli_piece_from(text, end);
    return 1;
}

// `text` with `word` and the spaces after it taken off its front, into `rest`, where it starts with the word: 1, or 0
int cli_piece_after(CliPiece text, const char *word, CliPiece *rest)
{
    if (!cli_piece_starts(text, word))
    {
        return 0;
    }
    const CliPiece past = cli_piece_from(text, cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}));
    if ((past.length != 0ull) && (cli_piece_alnum(past.at[0]) || (past.at[0] == '_')))
    {
        return 0;
    }
    *rest = cli_piece_trim_start(past);
    return 1;
}

// `text` with any of `words` taken off its front, as often as they stand there
CliPiece cli_piece_without(CliPiece text, const char *const *words, unsigned int count)
{
    for (;;)
    {
        const CliPiece before = text;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            CliPiece rest;
            if (cli_piece_after(text, words[at], &rest))
            {
                text = rest;
            }
        }
        if (text.at == before.at)
        {
            return text;
        }
    }
}

int cli_piece_in(CliPiece word, const char *const *words, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cli_piece_equals(word, words[at]))
        {
            return 1;
        }
    }
    return 0;
}
