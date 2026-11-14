// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// utf8.h: a text of UTF-8 read character by character, and walked by a set of bytes: the character at a place and the character
// before a place, each with the bytes it takes, a byte that starts no character or a character cut short read as one character
// of its own byte; and the first place from a place whose byte a set holds, or does not hold, and the place past the
// last such byte before a place, each the widest lane the build has, then a word at a time (utf8.c)
#ifndef UTF8_H
#define UTF8_H
#include "embed_dispatch_layout.h"
#include "embed_lane.h"

typedef struct Utf8AtCfg Utf8AtCfg;
typedef struct Utf8EndCfg Utf8EndCfg;
typedef struct Utf8SetCfg Utf8SetCfg;
typedef struct Utf8SetFillCfg Utf8SetFillCfg;
typedef struct Utf8WordCfg Utf8WordCfg;

// the max bytes of a set a walk tests for
#define UTF8_SET_MAX 16u

// A set's bytes each filled across a word, and across the target's lane
typedef struct
{
    unsigned int count;
    embed_word word_text[UTF8_SET_MAX];
    embed_lane lane_at[UTF8_SET_MAX];
} Utf8Set;

// the character at `at` of `size` bytes of `text` and the bytes it takes into `length`; and the character that ends before
// `end` and the bytes it takes
unsigned int utf8_character(const Utf8AtCfg *args);
unsigned int utf8_character_before(const Utf8EndCfg *args);
// the first place from `at` below `size` whose byte the set `set` holds, `size` where none is; the first whose byte it
// does not hold; the place past the last byte before `at` the set holds, and the place past the last it does not hold,
// 0 where none is. A set is at max UTF8_SET_MAX bytes, and no walk reads a byte past the bounds it is given
unsigned long long utf8_to_any(const Utf8SetCfg *args);
unsigned long long utf8_end_any(const Utf8SetCfg *args);
unsigned long long utf8_back_any(const Utf8SetCfg *args);
unsigned long long utf8_back_end(const Utf8SetCfg *args);
// `set`'s bytes filled across the words and lanes of `filled_out`; and the lanes of the word `loaded` that `filled`
// holds, for a walk of the caller's own
void utf8_set_fill(const Utf8SetFillCfg *args);
embed_word utf8_word_in(const Utf8WordCfg *args);

struct Utf8AtCfg
{
    const char *text;
    unsigned long long at;
    unsigned long long size;
    unsigned long long *length;
};

struct Utf8EndCfg
{
    const char *text;
    unsigned long long end;
    unsigned long long *length;
};

struct Utf8SetCfg
{
    const char *text;
    unsigned long long at;
    unsigned long long size;
    const char *set;
};

struct Utf8SetFillCfg
{
    Utf8Set *filled_out;
    const char *set;
};

struct Utf8WordCfg
{
    embed_word loaded;
    const Utf8Set *filled;
};

typedef struct
{
    unsigned int (*character)(const Utf8AtCfg *args);
} Utf8AtNs;
EMBED_TABLE_LAYOUT(Utf8AtNs, character);
EMBED_TABLE_STORAGE Utf8AtNs utf8_at EMBED_UNUSED = {
    .character = utf8_character,
};

typedef struct
{
    unsigned int (*character_before)(const Utf8EndCfg *args);
} Utf8EndNs;
EMBED_TABLE_LAYOUT(Utf8EndNs, character_before);
EMBED_TABLE_STORAGE Utf8EndNs utf8_end EMBED_UNUSED = {
    .character_before = utf8_character_before,
};

typedef struct
{
    unsigned long long (*to_any)(const Utf8SetCfg *args);
    unsigned long long (*end_any)(const Utf8SetCfg *args);
    unsigned long long (*back_any)(const Utf8SetCfg *args);
    unsigned long long (*back_end)(const Utf8SetCfg *args);
} Utf8SetNs;
EMBED_TABLE_LAYOUT(Utf8SetNs, to_any, end_any, back_any, back_end);
EMBED_TABLE_STORAGE Utf8SetNs utf8_set EMBED_UNUSED = {
    .to_any = utf8_to_any,
    .end_any = utf8_end_any,
    .back_any = utf8_back_any,
    .back_end = utf8_back_end,
};

typedef struct
{
    void (*fill)(const Utf8SetFillCfg *args);
} Utf8SetFillNs;
EMBED_TABLE_LAYOUT(Utf8SetFillNs, fill);
EMBED_TABLE_STORAGE Utf8SetFillNs utf8_set_fill_table EMBED_UNUSED = {
    .fill = utf8_set_fill,
};

typedef struct
{
    embed_word (*in)(const Utf8WordCfg *args);
} Utf8WordNs;
EMBED_TABLE_LAYOUT(Utf8WordNs, in);
EMBED_TABLE_STORAGE Utf8WordNs utf8_word EMBED_UNUSED = {
    .in = utf8_word_in,
};

#endif
