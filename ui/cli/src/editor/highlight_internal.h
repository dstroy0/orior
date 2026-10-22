// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_internal.h: a grammar as highlight.c reads it and highlight_line.c colors by it
#ifndef HIGHLIGHT_INTERNAL_H
#define HIGHLIGHT_INTERNAL_H

#include "../text/regexp.h"
#include "highlight.h"

// none: no token, no next, no state
#define CLI_HIGHLIGHT_NONE 0xffffffffu

// the most of each part one grammar holds as it is read
#define CLI_GRAMMAR_NODES (1u << 17u)

#define CLI_GRAMMAR_ITEMS (1u << 16u)

#define CLI_GRAMMAR_PARTS (1u << 17u)

#define CLI_GRAMMAR_RULES 8192u

#define CLI_GRAMMAR_TOKENS 16384u

#define CLI_GRAMMAR_LISTED 32768u

#define CLI_GRAMMAR_STATES 1024u

#define CLI_GRAMMAR_WORD_LISTS 1024u

#define CLI_GRAMMAR_WORDS 65536u

#define CLI_GRAMMAR_NAMES 4096u

#define CLI_GRAMMAR_TEXT (1u << 20u)

// the most rules one state holds, its includes read in
#define CLI_GRAMMAR_OWN 16384u

// the longest part of a line that is colored, in the page's units, the rest taking no token; the most letters of a
// line read, the most states stacked, and the most bytes the states each line ends in take
#define CLI_HIGHLIGHT_LONGEST 20000u

#define CLI_HIGHLIGHT_LETTERS (1u << 20u)

#define CLI_HIGHLIGHT_STACK 256u

// a token's kinds: a name, cases that pick one by the matched text, or one token for each group of the match
enum
{
    CLI_TOKEN_NAME,
    CLI_TOKEN_CASES,
    CLI_TOKEN_GROUPS
};

// A token: its kind; a name's place among the names; or its cases or its groups, `count` from `first` in the cases
typedef struct
{
    unsigned int kind;
    unsigned int name;
    unsigned int first;
    unsigned int count;
} CliHighlightToken;

// One case of a token, or one group's token: the case's text, none for a group, and its token; the word list the
// case's text names with `@`, none where it names none; and whether the case is `@default`
typedef struct
{
    unsigned int key;
    unsigned int token;
    unsigned int list;
    int fallback;
} CliHighlightCase;

// A rule: its pattern, its arrays at the grammar's places, its token and the state it goes to, each none where it has
// none, the state a text
typedef struct
{
    unsigned int node_first;
    unsigned int nodes;
    unsigned int item_first;
    unsigned int items;
    unsigned int part_first;
    unsigned int parts;
    unsigned int root;
    unsigned int groups;
    int fold;
    CliRegexpFirst first;
    int anchored;
    unsigned int token;
    unsigned int next;
} CliHighlightRule;

// A state, or a list of words: its name's text, and its rules or its words, `count` of the list from `first`; for a
// state, the letters any of its rules that colors or changes the state can start a match at, every such rule's first
// letters joined, and a table of the ASCII ones, 1 at each byte such a rule starts at, by which the colorer strides
// over the letters no rule takes in one lookup a letter
typedef struct
{
    unsigned int name;
    unsigned int first;
    unsigned int count;
    CliRegexpFirst starts;
    unsigned char starts_at[128];
} CliHighlightList;

// A grammar's parts, wherever they are held: the patterns' nodes, items, child places and their first letters; the
// rules, tokens and cases; the rules of each state in turn, and the states; the words, each list's in order, and the
// lists; the names; and the texts, each ended by a NUL. Each count is how many it holds
typedef struct
{
    CliRegexpNode *node;
    CliRegexpItem *item;
    unsigned int *part;
    CliRegexpFirst *part_first;
    CliHighlightRule *rule;
    CliHighlightToken *token;
    CliHighlightCase *cases;
    unsigned int *listed;
    CliHighlightList *state;
    unsigned int *word;
    CliHighlightList *words;
    unsigned int *name;
    char *text;
    unsigned int nodes;
    unsigned int items;
    unsigned int parts;
    unsigned int rules;
    unsigned int tokens;
    unsigned int case_count;
    unsigned int listed_count;
    unsigned int states;
    unsigned int word_count;
    unsigned int word_lists;
    unsigned int names;
    unsigned int text_size;
    int per_line;
    int ignore_case;
} CliGrammar;

// The room a grammar is read into, its parts placed in one cell the keeper takes for one read at the sizes the
// grammar's JSON bounds, each no more than its CLI_GRAMMAR_ limit: the parts as a grammar holds them; each state's own
// rules and includes before the includes are read in, a rule's place or an include's name marked; the room a pattern
// is read into; and how many of each the room holds, the texts in bytes
typedef struct
{
    CliRegexpNode *node;
    CliRegexpItem *item;
    unsigned int *part;
    CliRegexpFirst *part_first;
    CliHighlightRule *rule;
    CliHighlightToken *token;
    CliHighlightCase *cases;
    unsigned int *listed;
    CliHighlightList *state;
    unsigned int *word;
    CliHighlightList *words;
    unsigned int *name;
    char *text;
    unsigned int *own;
    CliHighlightList *own_state;
    CliRegexpRoom *pattern;
    unsigned int nodes_most;
    unsigned int items_most;
    unsigned int parts_most;
    unsigned int rules_most;
    unsigned int tokens_most;
    unsigned int listed_most;
    unsigned int states_most;
    unsigned int words_most;
    unsigned int word_lists_most;
    unsigned int names_most;
    unsigned int own_most;
    unsigned int text_most;
} CliGrammarRoom;

// the bytes each line's ending is given among the endings: its states as JSON and the comma before it; a coloring whose
// endings take more leaves them cut
#define CLI_HIGHLIGHT_ENDING 256ull

// The coloring room, its parts placed in one cell the keeper takes once and keeps, at twice the sizes the largest
// coloring asked so far, taken again only where a larger one comes: a line's letters, `most` of them for the longest
// line; where each starts in UTF-16 units and the line's end last; its runs; the states stacked; the text a case is
// picked by, four bytes a letter; and the state each of `count` lines ends in, the endings holding `endings_size` bytes
typedef struct
{
    unsigned int *letters;
    unsigned int *cols;
    unsigned int *run_col;
    unsigned int *run_name;
    char (*stack)[CLI_NAME_LONGEST];
    char *picked;
    char *key;
    char *endings;
    unsigned long long most;
    unsigned long long count;
    unsigned long long endings_size;
} CliHighlightRoom;

// the grammar at `def` in `json` read into `grammar`, its parts in `room` (highlight.c): 1, or 0 with why said
int cli_highlight_read(const CliJson *json, unsigned int def, CliGrammarRoom *room, CliGrammar *grammar, CliLine *said);
// a rule's pattern as the matcher reads it, its arrays the grammar's
void cli_highlight_pattern(const CliGrammar *grammar, const CliHighlightRule *rule, CliRegexp *pattern);
// the state named `name`, `length` bytes, CLI_HIGHLIGHT_NONE where the grammar has none; whether a list of words holds
// a word, `length` bytes
unsigned int cli_highlight_state(const CliGrammar *grammar, const char *name, unsigned long long length);
int cli_highlight_has_word(const CliGrammar *grammar, const CliHighlightList *list, const char *word,
                           unsigned long long length);
// `count` lines colored with a grammar in `room`, written as cli_highlight_color writes them (highlight_line.c)
void cli_highlight_lines(const CliGrammar *grammar, CliHighlightRoom *room, const char *state, const char *const *lines,
                         unsigned int count, CliLine *out);

#endif
