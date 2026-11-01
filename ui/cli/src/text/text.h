// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text.h: a text of UTF-8 read letter by letter: its whitespace as Unicode reads it, a text trimmed of it, a text
// split at it into words, and a text's letters and UTF-16 units counted (text.c)
#ifndef TEXT_H
#define TEXT_H

#include "../command_line/cli_internal.h"

// A line of a text: where it stops, at its "\n" or the text's end; where it stops with a "\r" before its "\n" left
// out; and where the next starts, past its "\n" or at the text's end
typedef struct
{
    unsigned long long stop;
    unsigned long long cut;
    unsigned long long next;
} CliTextLine;

// the line of `size` bytes of `text` that starts at `at`
CliTextLine cli_text_line_at(const char *text, unsigned long long at, unsigned long long size);
// the letter at `at` of `size` bytes of `text` and the bytes it takes into `length`; and the letter that ends before
// `end` and the bytes it takes
unsigned int cli_text_letter(const char *text, unsigned long long size, unsigned long long at,
                             unsigned long long *length);
unsigned int cli_text_letter_before(const char *text, unsigned long long end, unsigned long long *length);

// whether a letter is whitespace, as Unicode reads it; and whether it is alphabetic, and alphanumeric, as Rust's char
// defines, by the Unicode properties the regular expressions read
int cli_text_white(unsigned int letter);
int cli_text_alphabetic(unsigned int letter);
int cli_text_alphanumeric(unsigned int letter);

// `size` bytes of `text` with each byte `from` made `to`, and with each capital made small; how many of them are
// `byte`; and whether every one is below 0x80. Each a word at a time
void cli_text_swapped(char *text, unsigned long long size, char from, char to);
void cli_text_lowered(char *text, unsigned long long size);
// the small letters of `size` bytes made capitals; and `text` copied into `out`, sized for `room`, in lower case: its
// length, the copy ended with a NUL
void cli_text_raised(char *text, unsigned long long size);
unsigned long long cli_text_lowered_into(char *out, unsigned long long room, const char *text);
// the text `text`, `otherwise` where it is NULL, copied into `out` with its terminator
void cli_text_copied_or(char *out, const char *text, const char *otherwise);
// `size` bytes with each byte neither of `classes` nor of `set` made `other`
void cli_text_kept(char *text, unsigned long long size, unsigned int classes, const char *set, char other);
unsigned long long cli_text_counted(const char *text, unsigned long long size, char byte);
int cli_text_ascii(const char *text, unsigned long long size);
// the text written to `out`: with each byte `from` as `to`, answering the text's bytes; with `mark` before each byte of
// `set`; with `with` in the place of each byte of `set`; and with `value` in the place of each `name`
unsigned long long cli_text_put_swapped(VerbaLinea *out, const char *text, char from, char to);
void cli_text_put_marked(VerbaLinea *out, const char *text, const char *set, char mark);
void cli_text_put_replaced(VerbaLinea *out, const char *text, const char *set, const char *with);
void cli_text_put_filled(VerbaLinea *out, const char *text, const char *name, const char *value);
// the text written to `out`: `size` bytes of it with `mark` before each byte of `set`; with each byte of `set` twice;
// in lower case; with each byte neither of `classes` nor of `set` as `other`; and `size` bytes of it with each %xx made
// the byte it writes
void cli_text_put_marked_n(VerbaLinea *out, const char *text, unsigned long long size, const char *set, char mark);
void cli_text_put_doubled(VerbaLinea *out, const char *text, const char *set);
void cli_text_put_lowered(VerbaLinea *out, const char *text);
void cli_text_put_kept(VerbaLinea *out, const char *text, unsigned int classes, const char *set, char other);
void cli_text_put_unescaped(VerbaLinea *out, const char *text, unsigned long long size);

// the ASCII whitespace, a set of bytes for the walks below
#define CLI_TEXT_WHITE " \t\n\v\f\r"
// the kinds of byte a class walk passes over: ASCII letters, decimal digits, the bytes past ASCII, the capitals and the
// small letters
#define CLI_TEXT_LETTERS 1u
#define CLI_TEXT_DIGITS 2u
#define CLI_TEXT_PAST_ASCII 4u
#define CLI_TEXT_UPPER 8u
#define CLI_TEXT_LOWER 16u

// the first place from `at` below `size` whose byte the set `set` holds, and whose byte it does not: `size` where none
// is, `at` where it stands at or past `size`; and the place past the last byte before `at` the set holds, and it does
// not: 0 where none is. A set is at most 16 bytes. Each walks the widest lane the build has, then a word at a time, and
// reads no byte past the bounds it is given
unsigned long long cli_text_to_any(const char *text, unsigned long long at, unsigned long long size, const char *set);
unsigned long long cli_text_past_any(const char *text, unsigned long long at, unsigned long long size, const char *set);
unsigned long long cli_text_back_any(const char *text, unsigned long long at, const char *set);
unsigned long long cli_text_back_past(const char *text, unsigned long long at, const char *set);
// the first place from `at` below `size` whose byte is neither of `classes` nor of the set `set`, `size` where none is;
// the place past the last such byte before `at`, 0 where none is; the first place from `at` that holds `first` and then
// `second`, `size` where none does; and the number the decimal digits from `at` below `size` write, all ones where they
// are more than 19, the place past them into `end` where it is not NULL. Each a word at a time
unsigned long long cli_text_past_class(const char *text, unsigned long long at, unsigned long long size,
                                       unsigned int classes, const char *set);
unsigned long long cli_text_back_past_class(const char *text, unsigned long long at, unsigned int classes,
                                            const char *set);
unsigned long long cli_text_to_pair(const char *text, unsigned long long at, unsigned long long size, char first,
                                    char second);
unsigned long long cli_text_number(const char *text, unsigned long long at, unsigned long long size,
                                   unsigned long long *end);

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

// the letters `size` bytes of `text` contain, and the UTF-16 units
unsigned long long cli_text_letters(const char *text, unsigned long long size);
unsigned long long cli_text_units(const char *text, unsigned long long size);
// whether `size` bytes are UTF-8 whole, every byte starting or inside a letter: a word of ASCII bytes passes in one
// test, a word containing a high bit is read letter by letter (text.c)
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
void cli_paths_sorted(char (*paths)[CLI_PATH_MAX_LEN], unsigned int count);

// whether the first `size` bytes of `one` and `other` are the same bytes
int cli_text_same(const char *one, const char *other, unsigned long long size);

// the bytes of the text `text` before the first that the text `set` contains, or before its terminator; and the bytes
// at its start that `set` contains
unsigned long long cli_text_upto(const char *text, const char *set);
unsigned long long cli_text_over(const char *text, const char *set);

#endif
