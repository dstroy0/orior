// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp.h: regular expressions in JavaScript's syntax, matched at one place of a line as a sticky pattern is. The
// reader takes characters and escapes (`\d \w \s` and their opposites, `\b \B`,
// `\t \n \r \f \v \0`, `\xHH`, `\uHHHH`, `\u{H...}`, an escaped sign), `.`, classes with ranges and escapes, `^` and
// `$` at the line's ends, groups that capture and groups that do not, lookahead that matches and that does not,
// alternation, the quantifiers `* + ? {n} {n,} {n,m}` greedy and lazy, and backreferences. The reader takes the flag
// `i`, which compares characters without case. It takes the flag `u`, under which `\p{...}` and `\P{...}` read
// characters, numbers, and upper and lower case. Any other flag is no matter for a single line. The reader refuses
// lookbehind and named groups.
//
// A match backtracks. Some quantifiers take the text one way at a place or not at all. They are those over one
// character or a sequence of them, or over alternatives where no two start with the same character. Such a
// quantifier steps along at one level for all its passes, and a long run costs no stack. A string's
// `"(?:[^"\\]|\\.)*"` is one. Any other quantifier goes one level deeper a pass.
//
// A match ends as no match where it takes more than REGEXP_STEPS steps. It also ends so where its passes take more
// than REGEXP_STACK_MAX bytes of the stack below where it began. The reader puts a pattern into a Regexp the caller
// owns. The text is its characters, each a code point (regexp_read.c, regexp_match.c)
#ifndef REGEXP_H
#define REGEXP_H
#include "embed_dispatch_layout.h"
#include "regexp_base.h"

EMBED_BEGIN_DECLS

typedef struct RegexpTextU32sCfg RegexpTextU32sCfg;
typedef struct RegexpTextCfg RegexpTextCfg;
typedef struct RegexpSourceCfg RegexpSourceCfg;
typedef struct RegexpCfg RegexpCfg;
typedef struct RegexpIndexCfg RegexpIndexCfg;
typedef struct RegexpLetterCfg RegexpLetterCfg;
typedef struct RegexpWorkStateCfg RegexpWorkStateCfg;

// the max nodes, class items, child places and groups one pattern contains, and the max characters of its source
#define REGEXP_NODES 1024u

#define REGEXP_ITEMS 1024u

#define REGEXP_PARTS 2048u

#define REGEXP_GROUPS 64u

#define REGEXP_CHARACTERS 4096u

// The max steps one attempt at a match takes, and the max bytes of the stack that a match's passes take. Past
// either, the match stops. The bytes are well inside the stack of any thread of the window
#define REGEXP_STEPS 200000ULL

#define REGEXP_STACK_MAX (512u * 1024u)

// a quantifier with no max, and a node or a group of none
#define REGEXP_UNBOUNDED 0xffffffffu

#define REGEXP_NONE 0xffffffffu

// The characters a match can start with: each ASCII one, two words of bits, whether any other can, and whether a match
// can take nothing
typedef struct
{
    unsigned long long ascii[2];
    int other;
    int empty;
} RegexpFirst;

// One item of a class. It is a range from `low` to `high`, a digit, a word's character or a space. Else it is one of
// the Unicode properties in `low`. Each is `negated` where it is its opposite
typedef struct
{
    unsigned int type;
    unsigned int low;
    unsigned int high;
    int negated;
} RegexpItem;

// One node of a pattern: its type, its character, and its class items or its parts, from `first`, `count` of them. It
// holds its child, and the repeat's fewest and max, greedy, and whether a pass takes the text one way or none. It
// holds its group's index or REGEXP_NONE, and the flag of the opposite of a boundary, a lookahead and a class
typedef struct
{
    unsigned int type;
    unsigned int character;
    unsigned int first;
    unsigned int count;
    unsigned int child;
    unsigned int min;
    unsigned int max;
    unsigned int group;
    int greedy;
    int one_way;
    int negated;
    // the ASCII characters the node takes as one character, as two words of bits, known once the read of the pattern
    // ends
    unsigned long long ascii[2];
    int ascii_known;
} RegexpNode;

// A pattern read and ready to match: its nodes, items and parts. It holds the first characters of each alternative
// of an alternation at its part's place. It holds its root, its count of groups, `fold` under the `i` flag, and its
// first characters.
//
// A work state or a store of patterns holds the arrays. The reader puts a pattern into a work state, which holds the
// max any pattern takes. A store copies what the pattern took. Many patterns then take the room of one work state
typedef struct
{
    RegexpNode *node;
    unsigned int nodes;
    RegexpItem *item;
    unsigned int items;
    unsigned int *part;
    RegexpFirst *part_first;
    unsigned int parts;
    unsigned int root;
    unsigned int groups;
    int fold;
    RegexpFirst first;
    // whether the pattern starts with ^, and a match can start at the text's first character alone
    int anchored;
} Regexp;

// The work state that the reader puts one pattern into: the max nodes, class items and child places a pattern holds
typedef struct
{
    RegexpNode node[REGEXP_NODES];
    RegexpItem item[REGEXP_ITEMS];
    unsigned int part[REGEXP_PARTS];
    RegexpFirst part_first[REGEXP_PARTS];
} RegexpWorkState;

// the pattern's arrays pointed at the work state's, read into next
void regexp_on_capacity(const RegexpWorkStateCfg *args);

// one group's span in the text, from one character up to another, and 1 in `set` where the group took part in the match
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    int set;
} RegexpSpan;

enum
{
    REGEXP_EMPTY,
    REGEXP_CHAR,
    REGEXP_ANY,
    REGEXP_CLASS,
    REGEXP_START,
    REGEXP_END,
    REGEXP_BOUNDARY,
    REGEXP_GROUP,
    REGEXP_LOOK,
    REGEXP_CONCAT,
    REGEXP_ALT,
    REGEXP_REPEAT,
    REGEXP_BACK
};

enum
{
    REGEXP_RANGE,
    REGEXP_DIGIT,
    REGEXP_WORD,
    REGEXP_SPACE,
    REGEXP_PROPERTY
};

// the Unicode properties `\p{...}` reads under the `u` flag
enum
{
    REGEXP_CHARACTER,
    REGEXP_NUMBER,
    REGEXP_UPPER,
    REGEXP_LOWER
};

// `size` bytes of UTF-8 decoded into code points in `characters`, sized for `capacity`: their count
unsigned long long regexp_characters(const RegexpTextCfg *args);
// the pattern `source` with `flags` read into `regexp`, its arrays pointed at a work state: 1, or 0 with why given,
// `/<source>/: <why> at <place>`
int regexp_read(const RegexpSourceCfg *args);
// whether a first set allows a match to start at `character`: its ASCII bits, or `other` past them
int regexp_allows(const RegexpCfg *args);
// Whether a match starts at `at` in the text, `length` characters: 1, or 0. Each group's span goes into `groups`, the
// whole match first, with room for the regexp's groups and one
int regexp_match_into(const RegexpTextU32sCfg *args);
// the regexp's first characters and whether a pass of a node takes the text one way (regexp_match.c), which reading
// a pattern fills in
RegexpFirst regexp_first_get(const RegexpIndexCfg *args);
int regexp_one_way(const RegexpIndexCfg *args);
// the character's properties and case as the matcher reads them (regexp_characters.c)
int regexp_property(const RegexpLetterCfg *args);
unsigned int regexp_lower(const RegexpLetterCfg *args);
unsigned int regexp_upper(const RegexpLetterCfg *args);

struct RegexpTextU32sCfg
{
    const Regexp *regexp;
    const unsigned int *text_u32s;
    unsigned long long length;
    unsigned long long at;
    RegexpSpan *groups;
};

struct RegexpTextCfg
{
    const char *text;
    unsigned long long size;
    unsigned int *characters;
    unsigned long long capacity_u64;
};

struct RegexpSourceCfg
{
    Regexp *regexp_out;
    const char *source;
    const char *flags;
    VerbaLinea *err_msg;
};

struct RegexpCfg
{
    const RegexpFirst *first;
    unsigned int character;
};

struct RegexpIndexCfg
{
    const Regexp *regexp;
    unsigned int index;
};

struct RegexpLetterCfg
{
    unsigned int character;
    unsigned int property;
};

struct RegexpWorkStateCfg
{
    Regexp *regexp_out;
    RegexpWorkState *work_state;
};

EMBED_END_DECLS

// the tables of the calls, which a C caller reads; a C++ caller calls each function by its name
#if !defined(__cplusplus)

typedef struct
{
    void (*capacity)(const RegexpWorkStateCfg *args);
} RegexpWorkStateNs;
EMBED_TABLE_LAYOUT(RegexpWorkStateNs, capacity);
EMBED_TABLE_STORAGE RegexpWorkStateNs regexp_capacity EMBED_UNUSED = {
    .capacity = regexp_on_capacity,
};

typedef struct
{
    unsigned long long (*characters)(const RegexpTextCfg *args);
} RegexpTextNs;
EMBED_TABLE_LAYOUT(RegexpTextNs, characters);
EMBED_TABLE_STORAGE RegexpTextNs regexp_text EMBED_UNUSED = {
    .characters = regexp_characters,
};

typedef struct
{
    int (*read)(const RegexpSourceCfg *args);
} RegexpSourceNs;
EMBED_TABLE_LAYOUT(RegexpSourceNs, read);
EMBED_TABLE_STORAGE RegexpSourceNs regexp_source EMBED_UNUSED = {
    .read = regexp_read,
};

typedef struct
{
    int (*allows)(const RegexpCfg *args);
} RegexpNs;
EMBED_TABLE_LAYOUT(RegexpNs, allows);
EMBED_TABLE_STORAGE RegexpNs regexp_first_set EMBED_UNUSED = {
    .allows = regexp_allows,
};

typedef struct
{
    int (*match_into)(const RegexpTextU32sCfg *args);
} RegexpTextU32sNs;
EMBED_TABLE_LAYOUT(RegexpTextU32sNs, match_into);
EMBED_TABLE_STORAGE RegexpTextU32sNs regexp_text_u_32s EMBED_UNUSED = {
    .match_into = regexp_match_into,
};

typedef struct
{
    RegexpFirst (*first_get)(const RegexpIndexCfg *args);
    int (*one_way)(const RegexpIndexCfg *args);
} RegexpIndexNs;
EMBED_TABLE_LAYOUT(RegexpIndexNs, first_get, one_way);
EMBED_TABLE_STORAGE RegexpIndexNs regexp_index EMBED_UNUSED = {
    .first_get = regexp_first_get,
    .one_way = regexp_one_way,
};

typedef struct
{
    int (*property)(const RegexpLetterCfg *args);
    unsigned int (*lower)(const RegexpLetterCfg *args);
    unsigned int (*upper)(const RegexpLetterCfg *args);
} RegexpLetterNs;
EMBED_TABLE_LAYOUT(RegexpLetterNs, property, lower, upper);
EMBED_TABLE_STORAGE RegexpLetterNs regexp_character EMBED_UNUSED = {
    .property = regexp_property,
    .lower = regexp_lower,
    .upper = regexp_upper,
};

#endif

#endif
