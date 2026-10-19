// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// words.c: the words of one command a program is run with, each put after the words before it; and a letter
// written in UTF-8
#include "cli_internal.h"

void cli_words_put(CliWords *words, const char *word)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((words->count >= CLI_WORDS_MOST) || ((words->used + size) > CLI_WORDS_TEXT))
    {
        words->full = 1;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = words->text + words->used, .src = word, .bytes = size});
    words->word[words->count] = words->text + words->used;
    words->count += 1u;
    words->word[words->count] = NULL;
    // the size is below CLI_WORDS_TEXT, checked above
    words->used += (unsigned int)size;
}

void cli_utf8_put(CliLine *line, unsigned int letter)
{
    if (letter < 0x80u)
    {
        // below 0x80 a letter is its one byte
        cli_line_character(line, (char)letter);
        return;
    }
    const unsigned int bytes = (letter < 0x800u) ? 2u : (letter < 0x10000u) ? 3u : 4u;
    static const unsigned int s_marks[5] = {0x00u, 0x00u, 0xc0u, 0xe0u, 0xf0u};
    // each byte is the letter's bits in turn under its UTF-8 mark, which a char holds
    cli_line_character(line, (char)(s_marks[bytes] | (letter >> (6u * (bytes - 1u)))));
    for (unsigned int place = bytes - 1u; place > 0u; place -= 1u)
    {
        cli_line_character(line, (char)(0x80u | ((letter >> (6u * (place - 1u))) & 0x3fu)));
    }
}
