// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// words.c: the words of one command a program is run with, each put after the words before it; and a letter
// written in UTF-8
#include "cli_internal.h"

void cli_words_append(const CliWordsWordsCtx *args)
{
    const unsigned long long size =
        cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_CELLS_POOL}) + 1ULL;
    if ((args->words_words_pointer->count >= CLI_WORDS_MAX) ||
        ((args->words_words_pointer->used + size) > CLI_WORDS_TEXT))
    {
        args->words_words_pointer->full = 1;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->words_words_pointer->text + args->words_words_pointer->used,
                            .src = args->word_text,
                            .bytes = size});
    args->words_words_pointer->word_text[args->words_words_pointer->count] =
        args->words_words_pointer->text + args->words_words_pointer->used;
    args->words_words_pointer->count += 1u;
    args->words_words_pointer->word_text[args->words_words_pointer->count] = NULL;
    // the size is below CLI_WORDS_TEXT, checked above
    args->words_words_pointer->used += (unsigned int)size;
}

void cli_utf8_append(const CliLineLetterCtx *args)
{
    if (args->letter < 0x80u)
    {
        // below 0x80 a letter is its one byte
        mmgr_line_ch(args->line, (char)args->letter);
        return;
    }
    const unsigned int bytes = (args->letter < 0x800u) ? 2u : (args->letter < 0x10000u) ? 3u : 4u;
    static const unsigned int s_marks[5] = {0x00u, 0x00u, 0xc0u, 0xe0u, 0xf0u};
    // each byte is the letter's bits in turn under its UTF-8 mark, which a char stores
    mmgr_line_ch(args->line, (char)(s_marks[bytes] | (args->letter >> (6u * (bytes - 1u)))));
    for (unsigned int place = bytes - 1u; place > 0u; place -= 1u)
    {
        mmgr_line_ch(args->line, (char)(0x80u | ((args->letter >> (6u * (place - 1u))) & 0x3fu)));
    }
}
