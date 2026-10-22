// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text.h: a text of UTF-8 read letter by letter: its whitespace as Unicode reads it, a text trimmed of it, a text
// split at it into words, and a text's letters and UTF-16 units counted (text.c)
#ifndef TEXT_H
#define TEXT_H

#include "../command_line/cli_internal.h"

// the letter at `at` of `size` bytes of `text` and the bytes it takes into `length`; and the letter that ends before
// `end` and the bytes it takes
unsigned int cli_text_letter(const char *text, unsigned long long size, unsigned long long at,
                             unsigned long long *length);
unsigned int cli_text_letter_before(const char *text, unsigned long long end, unsigned long long *length);

// whether a letter is whitespace, as Unicode reads it; and whether it is alphabetic, and alphanumeric, as Rust's char
// says, by the Unicode properties the regular expressions read
int cli_text_white(unsigned int letter);
int cli_text_alphabetic(unsigned int letter);
int cli_text_alphanumeric(unsigned int letter);

// the bytes of `size` bytes of `text` its whitespace at the start takes; and the length of the text with its
// whitespace at the end taken off
unsigned long long cli_text_lead(const char *text, unsigned long long size);
unsigned long long cli_text_trimmed_end(const char *text, unsigned long long size);
// the bounds of `size` bytes of `text` with the whitespace at both ends taken off
void cli_text_trimmed(const char *text, unsigned long long size, unsigned long long *from, unsigned long long *to);

// the next word of `size` bytes of `text` from `*at`, a run of letters between whitespace, its start and its size
// into `word` and `length` and `*at` past it: 1, or 0 where there is none
int cli_text_word(const char *text, unsigned long long size, unsigned long long *at, unsigned long long *word,
                  unsigned long long *length);

// the letters `size` bytes of `text` hold, and the UTF-16 units
unsigned long long cli_text_letters(const char *text, unsigned long long size);
unsigned long long cli_text_units(const char *text, unsigned long long size);
// whether `size` bytes are UTF-8 whole, every byte starting or inside a letter: a word of ASCII bytes passes in one
// test, a word holding a high bit is read letter by letter (text.c)
int cli_text_utf8_whole(const unsigned char *text, unsigned long long size);

// whether `size` bytes of `text` start with `prefix`, and end with `suffix`
int cli_text_starts(const char *text, unsigned long long size, const char *prefix);
int cli_text_ends(const char *text, unsigned long long size, const char *suffix);

// the first place of `size` bytes of `wanted` in `length` bytes of `text`: 1 with it into `at`, or 0 where it is not
// there; and the last
int cli_text_find(const char *text, unsigned long long length, const char *wanted, unsigned long long size,
                  unsigned long long *at);
int cli_text_find_last(const char *text, unsigned long long length, const char *wanted, unsigned long long size,
                       unsigned long long *at);

// the end of a quoted text that starts at `at` of `size` bytes of `text`, past its closing quote: a text in three
// quotes, """ or ''', runs to the three that close it, and one in a quote to the quote that closes it on its line, a
// backslash carrying the byte after it; an unclosed text runs to the end of the line, or of the bytes. 1 with the end
// into `end`, or 0 where no quote starts at `at`
int cli_text_quoted_end(const char *text, unsigned long long size, unsigned long long at, unsigned long long *end);

// the last place of `byte` in `length` bytes of `text`, or NULL where it is not there
const char *cli_text_last(const char *text, unsigned long long length, char byte);

// the order of the texts `one` and `other`, byte by byte as unsigned through `one`'s terminator: below 0 where `one`
// comes first, 0 where they are the same, above 0 where `other` comes first
int cli_text_order(const char *one, const char *other);
// `count` paths put in the order cli_text_order gives their bytes (text.c)
void cli_paths_sorted(char (*paths)[CLI_PATH_LONGEST], unsigned int count);

// whether the first `size` bytes of `one` and `other` are the same bytes
int cli_text_same(const char *one, const char *other, unsigned long long size);

// the bytes of the text `text` before the first that the text `set` holds, or before its terminator; and the bytes
// at its start that `set` holds
unsigned long long cli_text_upto(const char *text, const char *set);
unsigned long long cli_text_over(const char *text, const char *set);

#endif
