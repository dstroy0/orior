// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text.h: a text of UTF-8 read letter by letter: its whitespace as Unicode reads it, a text trimmed of it, a text
// split at it into words, and a text's letters and UTF-16 units counted (text.c)
#ifndef TEXT_H
#define TEXT_H
typedef struct CliTextSizeAtCfg CliTextSizeAtCfg;
typedef struct CliTextSizeCfg CliTextSizeCfg;
typedef struct CliTextAtSizeClassesCfg CliTextAtSizeClassesCfg;
typedef struct CliTextClassesCfg CliTextClassesCfg;
typedef struct CliTextSizeSetCfg CliTextSizeSetCfg;
typedef struct CliTextAtSizeFirstCfg CliTextAtSizeFirstCfg;
typedef struct CliTextSizeLengthCfg CliTextSizeLengthCfg;
typedef struct CliTextSizeFromU64sCfg CliTextSizeFromU64sCfg;
typedef struct CliTextAtCfg CliTextAtCfg;
typedef struct CliTextAtSizeCfg CliTextAtSizeCfg;
typedef struct CliTextOutNameCfg CliTextOutNameCfg;
typedef struct CliTextSetCfg CliTextSetCfg;
typedef struct CliTextFromCfg CliTextFromCfg;
typedef struct CliTextSizeFromCfg CliTextSizeFromCfg;
typedef struct CliTextOutCfg CliTextOutCfg;
typedef struct CliTextSizeByteCfg CliTextSizeByteCfg;
typedef struct CliTextSizeSuffixCfg CliTextSizeSuffixCfg;
typedef struct CliTextByteCfg CliTextByteCfg;
typedef struct CliTextLengthCfg CliTextLengthCfg;
typedef struct CliTextOutCapacityCfg CliTextOutCapacityCfg;
typedef struct CliTextSizeOneCfg CliTextSizeOneCfg;
typedef struct CliTextSizePrefixCfg CliTextSizePrefixCfg;
typedef struct CliTextPathsCfg CliTextPathsCfg;
typedef struct CliTextSizeKindCfg CliTextSizeKindCfg;
typedef struct CliTextCfg CliTextCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// A line of a text: where it stops, at its "\n" or the text's end; where it stops with a "\r" before its "\n" left
// out; and where the next starts, past its "\n" or at the text's end
typedef struct
{
    unsigned long long stop;
    unsigned long long cut;
    unsigned long long next;
} CliTextLine;

// the line of `size` bytes of `text` that starts at `at`
CliTextLine cli_text_line_at(const CliTextAtCfg *args);
// the letter at `at` of `size` bytes of `text` and the bytes it takes into `length`; and the letter that ends before
// `end` and the bytes it takes
unsigned int cli_text_letter(const CliTextAtCfg *args);
unsigned int cli_text_letter_before(const CliTextLengthCfg *args);

// whether a letter is whitespace, as Unicode reads it; and whether it is alphabetic, and alphanumeric, as Rust's char
// defines, by the Unicode properties the regular expressions read
int cli_text_white(const CliTextCfg *args);
int cli_text_alphabetic(const CliTextCfg *args);
int cli_text_alphanumeric(const CliTextCfg *args);

// `size` bytes of `text` with each byte `from` made `to`, and with each capital made small; how many of them are
// `byte`; and whether every one is below 0x80. Each a word at a time
void cli_text_swap(const CliTextSizeFromCfg *args);
void cli_text_lower(const CliTextSizeFromCfg *args);
// the small letters of `size` bytes made capitals; and `text` copied into `out`, sized for `capacity`, in lower case:
// its length, the copy ended with a NUL
void cli_text_raise(const CliTextSizeFromCfg *args);
unsigned long long cli_text_lowered_into(const CliTextOutCapacityCfg *args);
// the text `text`, `otherwise` where it is NULL, copied into `out` with its terminator
void cli_text_copied_or(const CliTextOutCfg *args);
// `size` bytes with each byte neither of `classes` nor of `set` made `other`
void cli_text_save(const CliTextSizeCfg *args);
unsigned long long cli_text_count(const CliTextSizeByteCfg *args);
int cli_text_ascii(const CliTextSizeByteCfg *args);
// the text written to `out`: with each byte `from` as `to`, answering the text's bytes; with `mark` before each byte of
// `set`; with `with` in the place of each byte of `set`; and with `value` in the place of each `name`
unsigned long long cli_text_append_swap(const CliTextFromCfg *args);
void cli_text_append_mark(const CliTextSizeSetCfg *args);
void cli_text_append_replace(const CliTextSetCfg *args);
void cli_text_append_fill(const CliTextOutNameCfg *args);
// the text written to `out`: `size` bytes of it with `mark` before each byte of `set`; with each byte of `set` twice;
// in lower case; with each byte neither of `classes` nor of `set` as `other`; and `size` bytes of it with each %xx made
// the byte it writes
void cli_text_append_marked_n(const CliTextSizeSetCfg *args);
void cli_text_append_double(const CliTextSetCfg *args);
void cli_text_append_lower(const CliTextOutNameCfg *args);
void cli_text_append_save(const CliTextClassesCfg *args);
void cli_text_append_unescaped(const CliTextSizeSetCfg *args);

// the ASCII whitespace, a set of bytes for the walks below
#define CLI_TEXT_WHITE " \t\n\v\f\r"
// the kinds of byte a class walk passes over: ASCII letters, decimal digits, the bytes past ASCII, the capitals and the
// small letters
#define CLI_TEXT_LETTERS 1u
#define CLI_TEXT_DIGITS 2u
#define CLI_TEXT_END_ASCII 4u
#define CLI_TEXT_UPPER 8u
#define CLI_TEXT_LOWER 16u

// the first place from `at` below `size` whose byte the set `set` holds, and whose byte it does not: `size` where none
// is, `at` where it stands at or past `size`; and the place past the last byte before `at` the set holds, and it does
// not: 0 where none is. A set is at most 16 bytes. Each walks the widest lane the build has, then a word at a time, and
// reads no byte past the bounds it is given
unsigned long long cli_text_to_any(const CliTextAtSizeClassesCfg *args);
unsigned long long cli_text_end_any(const CliTextAtSizeClassesCfg *args);
unsigned long long cli_text_back_any(const CliTextAtSizeClassesCfg *args);
unsigned long long cli_text_back_end(const CliTextAtSizeClassesCfg *args);
// the first place from `at` below `size` whose byte is of `classes` or of the set `set`, `size` where none is; the
// first place from `at` below `size` whose byte is neither of `classes` nor of the set `set`, `size` where none is; the
// place past the last such byte before `at`, 0 where none is; the first place from `at` that holds `first` and then
// `second`, `size` where none does; and the number the decimal digits from `at` below `size` write, all ones where they
// are more than 19, the place past them into `end` where it is not NULL. Each a word at a time
unsigned long long cli_text_to_class(const CliTextAtSizeClassesCfg *args);
unsigned long long cli_text_end_class(const CliTextAtSizeClassesCfg *args);
unsigned long long cli_text_back_end_class(const CliTextAtSizeClassesCfg *args);
unsigned long long cli_text_to_pair(const CliTextAtSizeFirstCfg *args);
unsigned long long cli_text_number(const CliTextAtSizeCfg *args);

// the bytes of `size` bytes of `text` its whitespace at the start takes; and the length of the text with its
// whitespace at the end taken off
unsigned long long cli_text_prefix(const CliTextSizeFromU64sCfg *args);
unsigned long long cli_text_trimmed_end(const CliTextSizeByteCfg *args);
// the bounds of `size` bytes of `text` with the whitespace at both ends taken off
void cli_text_trim(const CliTextSizeFromU64sCfg *args);

// the next word of `size` bytes of `text` from `*at`, a run of letters between whitespace, its start and its size
// into `word` and `length` and `*at` past it: 1, or 0 where there is none
int cli_text_word(const CliTextSizeLengthCfg *args);

// the letters `size` bytes of `text` contain, and the UTF-16 units
unsigned long long cli_text_letters(const CliTextSizeByteCfg *args);
unsigned long long cli_text_units(const CliTextSizeByteCfg *args);
// whether `size` bytes are UTF-8 whole, every byte starting or inside a letter: a word of ASCII bytes passes in one
// test, a word containing a high bit is read letter by letter (text.c)
int cli_text_utf8_valid(const CliTextSizeKindCfg *args);

// whether `size` bytes of `text` start with `prefix`, and end with `suffix`
int cli_text_starts(const CliTextSizePrefixCfg *args);
int cli_text_ends(const CliTextSizeSuffixCfg *args);

// the first place of `size` bytes of `required` in `length` bytes of `text`: 1 with it into `at`, or 0 where it is not
// there; and the last
int cli_text_find(const CliTextSizeAtCfg *args);
int cli_text_find_last(const CliTextSizeAtCfg *args);

// the end of a quoted text that starts at `at` of `size` bytes of `text`, past its closing quote: a text in three
// quotes, """ or ''', runs to the three that close it, and one in a quote to the quote that closes it on its line, a
// backslash carrying the byte after it; an unclosed text runs to the end of the line, or of the bytes. 1 with the end
// into `end`, or 0 where no quote starts at `at`
int cli_text_quoted_end(const CliTextAtSizeCfg *args);

// the last place of `byte` in `length` bytes of `text`, or NULL where it is not there
const char *cli_text_last(const CliTextByteCfg *args);

// the order of the texts `one` and `other`, byte by byte as unsigned through `one`'s terminator: below 0 where `one`
// comes first, 0 where they are the same, above 0 where `other` comes first
int cli_text_order(const CliTextSizeOneCfg *args);
// `count` paths put in the order cli_text_order gives their bytes (text.c)
void cli_paths_sort(const CliTextPathsCfg *args);

// whether the first `size` bytes of `one` and `other` are the same bytes
int cli_text_same(const CliTextSizeOneCfg *args);

// the bytes of the text `text` before the first that the text `set` contains, or before its terminator; and the bytes
// at its start that `set` contains
unsigned long long cli_text_upto(const CliTextSetCfg *args);
unsigned long long cli_text_over(const CliTextSetCfg *args);

struct CliTextSizeAtCfg
{
    const char *text;
    unsigned long long size;
    unsigned long long *at_pointer;
    unsigned long long length_value;
    const char *required;
};

struct CliTextSizeCfg
{
    unsigned long long size;
    char *text_out;
    unsigned int classes;
    const char *set;
    char other;
};

struct CliTextAtSizeClassesCfg
{
    const char *text;
    unsigned long long at;
    unsigned long long size;
    unsigned int classes;
    const char *set;
};

struct CliTextClassesCfg
{
    const char *text;
    unsigned int classes;
    const char *set;
    char other;
    VerbaLinea *out;
};

struct CliTextSizeSetCfg
{
    const char *text;
    unsigned long long size;
    const char *set;
    VerbaLinea *out;
    char mark;
};

struct CliTextAtSizeFirstCfg
{
    const char *text;
    unsigned long long at;
    unsigned long long size;
    char first;
    char second;
};

struct CliTextSizeLengthCfg
{
    const char *text;
    unsigned long long size;
    unsigned long long *length;
    unsigned long long *at_pointer;
    unsigned long long *word;
};

struct CliTextSizeFromU64sCfg
{
    const char *text;
    unsigned long long size;
    unsigned long long *from_u64s;
    unsigned long long *to_u64s;
};

struct CliTextAtCfg
{
    const char *text;
    unsigned long long at;
    unsigned long long size;
    unsigned long long *length;
};

struct CliTextAtSizeCfg
{
    const char *text;
    unsigned long long at;
    unsigned long long size;
    unsigned long long *end;
};

struct CliTextOutNameCfg
{
    const char *text;
    VerbaLinea *out;
    const char *name;
    const char *value;
};

struct CliTextSetCfg
{
    const char *text;
    const char *set;
    VerbaLinea *out;
    const char *with;
};

struct CliTextFromCfg
{
    const char *text;
    char from;
    char to;
    VerbaLinea *out;
};

struct CliTextSizeFromCfg
{
    unsigned long long size;
    char *text_out;
    char from;
    char to;
};

struct CliTextOutCfg
{
    const char *text;
    char *out_capacity;
    const char *otherwise;
};

struct CliTextSizeByteCfg
{
    const char *text;
    unsigned long long size;
    char byte;
};

struct CliTextSizeSuffixCfg
{
    const char *text;
    unsigned long long size;
    const char *suffix;
};

struct CliTextByteCfg
{
    const char *text;
    char byte;
    unsigned long long length_value;
};

struct CliTextLengthCfg
{
    const char *text;
    unsigned long long *length;
    unsigned long long end_value;
};

struct CliTextOutCapacityCfg
{
    const char *text;
    char *out_capacity;
    unsigned long long capacity;
};

struct CliTextSizeOneCfg
{
    unsigned long long size;
    const char *one;
    const char *other_pointer;
};

struct CliTextSizePrefixCfg
{
    const char *text;
    unsigned long long size;
    const char *prefix;
};

struct CliTextPathsCfg
{
    char (*paths)[CLI_PATH_MAX_LEN];
    unsigned int count;
};

struct CliTextSizeKindCfg
{
    unsigned long long size;
    const unsigned char *text_bytes;
};

struct CliTextCfg
{
    unsigned int letter;
};

typedef struct
{
    CliTextLine (*line_at)(const CliTextAtCfg *args);
    unsigned int (*letter)(const CliTextAtCfg *args);
} CliTextAtNs;
EMBED_TABLE_LAYOUT(CliTextAtNs, line_at, letter);
EMBED_TABLE_STORAGE CliTextAtNs cli_text_at EMBED_UNUSED = {
    .line_at = cli_text_line_at,
    .letter = cli_text_letter,
};

typedef struct
{
    unsigned int (*letter_before)(const CliTextLengthCfg *args);
} CliTextLengthNs;
EMBED_TABLE_LAYOUT(CliTextLengthNs, letter_before);
EMBED_TABLE_STORAGE CliTextLengthNs cli_text_length EMBED_UNUSED = {
    .letter_before = cli_text_letter_before,
};

typedef struct
{
    int (*white)(const CliTextCfg *args);
    int (*alphabetic)(const CliTextCfg *args);
    int (*alphanumeric)(const CliTextCfg *args);
} CliTextNs;
EMBED_TABLE_LAYOUT(CliTextNs, white, alphabetic, alphanumeric);
EMBED_TABLE_STORAGE CliTextNs cli_text_table EMBED_UNUSED = {
    .white = cli_text_white,
    .alphabetic = cli_text_alphabetic,
    .alphanumeric = cli_text_alphanumeric,
};

typedef struct
{
    void (*swap)(const CliTextSizeFromCfg *args);
    void (*lower)(const CliTextSizeFromCfg *args);
    void (*raise)(const CliTextSizeFromCfg *args);
} CliTextSizeFromNs;
EMBED_TABLE_LAYOUT(CliTextSizeFromNs, swap, lower, raise);
EMBED_TABLE_STORAGE CliTextSizeFromNs cli_text_size_from EMBED_UNUSED = {
    .swap = cli_text_swap,
    .lower = cli_text_lower,
    .raise = cli_text_raise,
};

typedef struct
{
    unsigned long long (*lowered_into)(const CliTextOutCapacityCfg *args);
} CliTextOutCapacityNs;
EMBED_TABLE_LAYOUT(CliTextOutCapacityNs, lowered_into);
EMBED_TABLE_STORAGE CliTextOutCapacityNs cli_text_out_capacity EMBED_UNUSED = {
    .lowered_into = cli_text_lowered_into,
};

typedef struct
{
    void (*copied_or)(const CliTextOutCfg *args);
} CliTextOutNs;
EMBED_TABLE_LAYOUT(CliTextOutNs, copied_or);
EMBED_TABLE_STORAGE CliTextOutNs cli_text_out EMBED_UNUSED = {
    .copied_or = cli_text_copied_or,
};

typedef struct
{
    void (*save)(const CliTextSizeCfg *args);
} CliTextSizeNs;
EMBED_TABLE_LAYOUT(CliTextSizeNs, save);
EMBED_TABLE_STORAGE CliTextSizeNs cli_text_size EMBED_UNUSED = {
    .save = cli_text_save,
};

typedef struct
{
    unsigned long long (*count)(const CliTextSizeByteCfg *args);
    int (*ascii)(const CliTextSizeByteCfg *args);
    unsigned long long (*trimmed_end)(const CliTextSizeByteCfg *args);
    unsigned long long (*letters)(const CliTextSizeByteCfg *args);
    unsigned long long (*units)(const CliTextSizeByteCfg *args);
} CliTextSizeByteNs;
EMBED_TABLE_LAYOUT(CliTextSizeByteNs, count, ascii, trimmed_end, letters, units);
EMBED_TABLE_STORAGE CliTextSizeByteNs cli_text_size_byte EMBED_UNUSED = {
    .count = cli_text_count,
    .ascii = cli_text_ascii,
    .trimmed_end = cli_text_trimmed_end,
    .letters = cli_text_letters,
    .units = cli_text_units,
};

typedef struct
{
    unsigned long long (*append_swap)(const CliTextFromCfg *args);
} CliTextFromNs;
EMBED_TABLE_LAYOUT(CliTextFromNs, append_swap);
EMBED_TABLE_STORAGE CliTextFromNs cli_text_from EMBED_UNUSED = {
    .append_swap = cli_text_append_swap,
};

typedef struct
{
    void (*append_mark)(const CliTextSizeSetCfg *args);
    void (*append_marked_n)(const CliTextSizeSetCfg *args);
    void (*append_unescaped)(const CliTextSizeSetCfg *args);
} CliTextSizeSetNs;
EMBED_TABLE_LAYOUT(CliTextSizeSetNs, append_mark, append_marked_n, append_unescaped);
EMBED_TABLE_STORAGE CliTextSizeSetNs cli_text_size_set EMBED_UNUSED = {
    .append_mark = cli_text_append_mark,
    .append_marked_n = cli_text_append_marked_n,
    .append_unescaped = cli_text_append_unescaped,
};

typedef struct
{
    void (*append_replace)(const CliTextSetCfg *args);
    void (*append_double)(const CliTextSetCfg *args);
    unsigned long long (*upto)(const CliTextSetCfg *args);
    unsigned long long (*over)(const CliTextSetCfg *args);
} CliTextSetNs;
EMBED_TABLE_LAYOUT(CliTextSetNs, append_replace, append_double, upto, over);
EMBED_TABLE_STORAGE CliTextSetNs cli_text_set_table EMBED_UNUSED = {
    .append_replace = cli_text_append_replace,
    .append_double = cli_text_append_double,
    .upto = cli_text_upto,
    .over = cli_text_over,
};

typedef struct
{
    void (*append_fill)(const CliTextOutNameCfg *args);
    void (*append_lower)(const CliTextOutNameCfg *args);
} CliTextOutNameNs;
EMBED_TABLE_LAYOUT(CliTextOutNameNs, append_fill, append_lower);
EMBED_TABLE_STORAGE CliTextOutNameNs cli_text_out_name EMBED_UNUSED = {
    .append_fill = cli_text_append_fill,
    .append_lower = cli_text_append_lower,
};

typedef struct
{
    void (*append_save)(const CliTextClassesCfg *args);
} CliTextClassesNs;
EMBED_TABLE_LAYOUT(CliTextClassesNs, append_save);
EMBED_TABLE_STORAGE CliTextClassesNs cli_text_classes EMBED_UNUSED = {
    .append_save = cli_text_append_save,
};

typedef struct
{
    unsigned long long (*to_any)(const CliTextAtSizeClassesCfg *args);
    unsigned long long (*end_any)(const CliTextAtSizeClassesCfg *args);
    unsigned long long (*back_any)(const CliTextAtSizeClassesCfg *args);
    unsigned long long (*back_end)(const CliTextAtSizeClassesCfg *args);
    unsigned long long (*to_class)(const CliTextAtSizeClassesCfg *args);
    unsigned long long (*end_class)(const CliTextAtSizeClassesCfg *args);
    unsigned long long (*back_end_class)(const CliTextAtSizeClassesCfg *args);
} CliTextAtSizeClassesNs;
EMBED_TABLE_LAYOUT(CliTextAtSizeClassesNs, to_any, end_any, back_any, back_end, to_class, end_class, back_end_class);
EMBED_TABLE_STORAGE CliTextAtSizeClassesNs cli_text_at_size_classes EMBED_UNUSED = {
    .to_any = cli_text_to_any,
    .end_any = cli_text_end_any,
    .back_any = cli_text_back_any,
    .back_end = cli_text_back_end,
    .to_class = cli_text_to_class,
    .end_class = cli_text_end_class,
    .back_end_class = cli_text_back_end_class,
};

typedef struct
{
    unsigned long long (*to_pair)(const CliTextAtSizeFirstCfg *args);
} CliTextAtSizeFirstNs;
EMBED_TABLE_LAYOUT(CliTextAtSizeFirstNs, to_pair);
EMBED_TABLE_STORAGE CliTextAtSizeFirstNs cli_text_at_size_first EMBED_UNUSED = {
    .to_pair = cli_text_to_pair,
};

typedef struct
{
    unsigned long long (*number)(const CliTextAtSizeCfg *args);
    int (*quoted_end)(const CliTextAtSizeCfg *args);
} CliTextAtSizeNs;
EMBED_TABLE_LAYOUT(CliTextAtSizeNs, number, quoted_end);
EMBED_TABLE_STORAGE CliTextAtSizeNs cli_text_at_size EMBED_UNUSED = {
    .number = cli_text_number,
    .quoted_end = cli_text_quoted_end,
};

typedef struct
{
    unsigned long long (*lead)(const CliTextSizeFromU64sCfg *args);
    void (*trim)(const CliTextSizeFromU64sCfg *args);
} CliTextSizeFromU64sNs;
EMBED_TABLE_LAYOUT(CliTextSizeFromU64sNs, lead, trim);
EMBED_TABLE_STORAGE CliTextSizeFromU64sNs cli_text_size_from_u_64s EMBED_UNUSED = {
    .lead = cli_text_prefix,
    .trim = cli_text_trim,
};

typedef struct
{
    int (*word)(const CliTextSizeLengthCfg *args);
} CliTextSizeLengthNs;
EMBED_TABLE_LAYOUT(CliTextSizeLengthNs, word);
EMBED_TABLE_STORAGE CliTextSizeLengthNs cli_text_size_length EMBED_UNUSED = {
    .word = cli_text_word,
};

typedef struct
{
    int (*utf8_valid)(const CliTextSizeKindCfg *args);
} CliTextSizeKindNs;
EMBED_TABLE_LAYOUT(CliTextSizeKindNs, utf8_valid);
EMBED_TABLE_STORAGE CliTextSizeKindNs cli_text_size_kind EMBED_UNUSED = {
    .utf8_valid = cli_text_utf8_valid,
};

typedef struct
{
    int (*starts)(const CliTextSizePrefixCfg *args);
} CliTextSizePrefixNs;
EMBED_TABLE_LAYOUT(CliTextSizePrefixNs, starts);
EMBED_TABLE_STORAGE CliTextSizePrefixNs cli_text_size_prefix EMBED_UNUSED = {
    .starts = cli_text_starts,
};

typedef struct
{
    int (*ends)(const CliTextSizeSuffixCfg *args);
} CliTextSizeSuffixNs;
EMBED_TABLE_LAYOUT(CliTextSizeSuffixNs, ends);
EMBED_TABLE_STORAGE CliTextSizeSuffixNs cli_text_size_suffix EMBED_UNUSED = {
    .ends = cli_text_ends,
};

typedef struct
{
    int (*find)(const CliTextSizeAtCfg *args);
    int (*find_last)(const CliTextSizeAtCfg *args);
} CliTextSizeAtNs;
EMBED_TABLE_LAYOUT(CliTextSizeAtNs, find, find_last);
EMBED_TABLE_STORAGE CliTextSizeAtNs cli_text_size_at EMBED_UNUSED = {
    .find = cli_text_find,
    .find_last = cli_text_find_last,
};

typedef struct
{
    const char *(*last)(const CliTextByteCfg *args);
} CliTextByteNs;
EMBED_TABLE_LAYOUT(CliTextByteNs, last);
EMBED_TABLE_STORAGE CliTextByteNs cli_text_byte EMBED_UNUSED = {
    .last = cli_text_last,
};

typedef struct
{
    int (*order)(const CliTextSizeOneCfg *args);
    int (*same)(const CliTextSizeOneCfg *args);
} CliTextSizeOneNs;
EMBED_TABLE_LAYOUT(CliTextSizeOneNs, order, same);
EMBED_TABLE_STORAGE CliTextSizeOneNs cli_text_size_one EMBED_UNUSED = {
    .order = cli_text_order,
    .same = cli_text_same,
};

typedef struct
{
    void (*paths_sort)(const CliTextPathsCfg *args);
} CliTextPathsNs;
EMBED_TABLE_LAYOUT(CliTextPathsNs, paths_sort);
EMBED_TABLE_STORAGE CliTextPathsNs cli_text_paths EMBED_UNUSED = {
    .paths_sort = cli_paths_sort,
};

#endif
