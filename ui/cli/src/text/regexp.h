// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp.h: regular expressions as the page's grammars write them, in JavaScript's syntax, matched at one place of a
// line as a sticky pattern is. What is read: characters and escapes (`\d \w \s` and their opposites, `\b \B`,
// `\t \n \r \f \v \0`, `\xHH`, `\uHHHH`, `\u{H...}`, an escaped sign), `.`, classes with ranges and escapes, `^` and
// `$` at the line's ends, groups that capture and groups that do not, lookahead that holds and that does not,
// alternation, the quantifiers `* + ? {n} {n,} {n,m}` greedy and lazy, and backreferences. The flags read are `i`,
// which compares letters without case, and `u`, under which `\p{...}` and `\P{...}` read letters, numbers, and upper
// and lower case; any other flag is no matter for a single line. Lookbehind and named groups are refused, and a
// grammar that holds one is colored by the page instead.
//
// Matching backtracks. A quantifier over what takes the text one way at a place or not at all, one character, a
// sequence of them, or alternatives of those no two of which start with the same character, steps along without going
// deeper for each pass it takes, and a long run costs no stack; a string's `"(?:[^"\\]|\\.)*"` is one. Any other
// repeats by going one level deeper a pass. A match that takes more than CLI_REGEXP_STEPS steps, or whose passes take
// more than CLI_REGEXP_STACK_MOST bytes of the stack below where it began, is given up as no match. A pattern is read
// into a CliRegexp the caller holds; the text is its letters, each a code point (regexp_read.c, regexp_match.c)
#ifndef REGEXP_H
#define REGEXP_H

#include "../command_line/cli_internal.h"

// the most nodes, class items, child places and groups one pattern holds, and the most letters it is read from
#define CLI_REGEXP_NODES 1024u

#define CLI_REGEXP_ITEMS 1024u

#define CLI_REGEXP_PARTS 2048u

#define CLI_REGEXP_GROUPS 64u

#define CLI_REGEXP_LETTERS 4096u

// the most steps one attempt at a match takes before it is given up, and the most bytes of the stack a match's passes
// take before it is given up: well inside the stack any thread of the window is given
#define CLI_REGEXP_STEPS 200000ull

#define CLI_REGEXP_STACK_MOST (512u * 1024u)

// a quantifier with no most, and a node or a group of none
#define CLI_REGEXP_UNBOUNDED 0xffffffffu

#define CLI_REGEXP_NONE 0xffffffffu

// The characters a match can start with: each ASCII one, two words of bits, whether any other can, and whether a match
// can take nothing
typedef struct
{
    unsigned long long ascii[2];
    int other;
    int empty;
} CliRegexpFirst;

// One item of a class: a range from `low` to `high`; a digit, a word's character or a space, or one of the Unicode
// properties in `low`, each `negated` where it is its opposite
typedef struct
{
    unsigned int kind;
    unsigned int low;
    unsigned int high;
    int negated;
} CliRegexpItem;

// One node of a pattern: its kind; its character; its class items, or its parts, from `first`, `count` of them; its
// child; the repeat's fewest and most, greedy, and whether a pass takes the text one way or none; its group's index or
// CLI_REGEXP_NONE; and the flag of a boundary, a lookahead and a class, its opposite
typedef struct
{
    unsigned int kind;
    unsigned int letter;
    unsigned int first;
    unsigned int count;
    unsigned int child;
    unsigned int min;
    unsigned int max;
    unsigned int group;
    int greedy;
    int one_way;
    int negated;
} CliRegexpNode;

// A pattern read and ready to match: its nodes, items and parts, the first characters of each alternative of an
// alternation at its part's place, its root, its count of groups, `fold` under the `i` flag, and its first characters
// The arrays are where a room or a keeper of patterns holds them: a pattern is read into a room, which holds the most
// any pattern takes, and a keeper copies what it took, so many patterns are kept in the room of one
typedef struct
{
    CliRegexpNode *node;
    unsigned int nodes;
    CliRegexpItem *item;
    unsigned int items;
    unsigned int *part;
    CliRegexpFirst *part_first;
    unsigned int parts;
    unsigned int root;
    unsigned int groups;
    int fold;
    CliRegexpFirst first;
} CliRegexp;

// The room one pattern is read into: the most nodes, class items and child places a pattern holds
typedef struct
{
    CliRegexpNode node[CLI_REGEXP_NODES];
    CliRegexpItem item[CLI_REGEXP_ITEMS];
    unsigned int part[CLI_REGEXP_PARTS];
    CliRegexpFirst part_first[CLI_REGEXP_PARTS];
} CliRegexpRoom;

// the pattern's arrays pointed at the room's, read into next
void cli_regexp_roomed(CliRegexp *regexp, CliRegexpRoom *room);

// one group's span in the text, from one letter up to another, and 1 in `set` where the group took part in the match
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    int set;
} CliRegexpSpan;

enum
{
    CLI_REGEXP_EMPTY,
    CLI_REGEXP_CHAR,
    CLI_REGEXP_ANY,
    CLI_REGEXP_CLASS,
    CLI_REGEXP_START,
    CLI_REGEXP_END,
    CLI_REGEXP_BOUNDARY,
    CLI_REGEXP_GROUP,
    CLI_REGEXP_LOOK,
    CLI_REGEXP_CONCAT,
    CLI_REGEXP_ALT,
    CLI_REGEXP_REPEAT,
    CLI_REGEXP_BACK
};

enum
{
    CLI_REGEXP_RANGE,
    CLI_REGEXP_DIGIT,
    CLI_REGEXP_WORD,
    CLI_REGEXP_SPACE,
    CLI_REGEXP_PROPERTY
};

// the Unicode properties `\p{...}` reads under the `u` flag
enum
{
    CLI_REGEXP_LETTER,
    CLI_REGEXP_NUMBER,
    CLI_REGEXP_UPPER,
    CLI_REGEXP_LOWER
};

// `size` bytes of UTF-8 decoded into code points in `letters`, which holds `room`: their count
unsigned long long cli_regexp_letters(const char *text, unsigned long long size, unsigned int *letters,
                                      unsigned long long room);
// the pattern `source` with `flags` read into `regexp`, its arrays pointed at a room: 1, or 0 with why said,
// `/<source>/: <why> at <place>`
int cli_regexp_read(CliRegexp *regexp, const char *source, const char *flags, CliLine *said);
// whether a match starts at `at` in the text, `length` letters, each group's span written into `groups`, which holds
// the regexp's groups and one, the whole match first: 1, or 0
int cli_regexp_match_into(const CliRegexp *regexp, const unsigned int *text, unsigned long long length,
                          unsigned long long at, CliRegexpSpan *groups);
// the regexp's first characters and whether a pass of a node takes the text one way (regexp_match.c), which reading
// a pattern fills in
CliRegexpFirst cli_regexp_first_of(const CliRegexp *regexp, unsigned int node);
int cli_regexp_one_way(const CliRegexp *regexp, unsigned int node);
// the letter's properties and case as the matcher reads them (regexp_letters.c)
int cli_regexp_property(unsigned int property, unsigned int letter);
unsigned int cli_regexp_lower(unsigned int letter);
unsigned int cli_regexp_upper(unsigned int letter);

#endif
