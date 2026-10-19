// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// line_read.h: a text's lines read one at a time out of its bytes, and a line's words and numbers read one at a time
#ifndef LINE_READ_H
#define LINE_READ_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // the line at `*at` of the `size` bytes at `bytes` read into `line`, which holds `room`, as much of it as fits,
    // and `*at` moved past it: 1, or 0 where no line is left
    int line_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at, char *line, size_t room);

    // the word at `*at`, past the spaces and tabs before it and up to the next space, tab or line's end, into `word`,
    // which holds `room`, and `*at` moved past it: 1, or 0 where no word is left or it does not fit
    int line_word(const char **at, char *word, size_t room);

    // the text at `*at` up to the next `end` or the line's end, into `text`, which holds `room`, and `*at` moved to
    // the `end` it stopped at: 1, or 0 where no letter is there or it does not fit
    int line_until(const char **at, char end, char *text, size_t room);

    // the number at `*at`, past the spaces and tabs before it, in decimal or, with `hex`, in hex with or without a
    // leading 0x, into `value`, and `*at` moved past it: 1, or 0 where no digit is there or it passes 64 bits
    int line_number(const char **at, int hex, unsigned long long *value);

    // the decimal number at `*at`, past the spaces and tabs before it, into `value`, and `*at` moved past it: 1, or 0
    // where no digit is there or an unsigned int does not hold it
    int line_unsigned(const char **at, unsigned int *value);

    // the decimal number at `*at`, past the spaces and tabs before it and a `-` before its digits, into `value`, and
    // `*at` moved past it: 1, or 0 where no digit is there or an int does not hold it
    int line_signed(const char **at, int *value);

    // the letters `text` at `*at`, past the spaces and tabs before them, and `*at` moved past them: 1, or 0 where they
    // are not there
    int line_letters(const char **at, const char *text);

#ifdef __cplusplus
}
#endif

#endif
