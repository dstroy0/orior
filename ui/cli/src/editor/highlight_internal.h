// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_internal.h: a grammar as highlight.c reads it and highlight_line.c colors by it
#ifndef HIGHLIGHT_INTERNAL_H
#define HIGHLIGHT_INTERNAL_H
#include "../text/regexp.h"
#include "embed_dispatch_layout.h"
#include "highlight.h"

typedef struct CliHighlightWorkStateStateCtx CliHighlightWorkStateStateCtx;
typedef struct CliHighlightWorkStateCtx CliHighlightWorkStateCtx;
typedef struct CliHighlightJsonCtx CliHighlightJsonCtx;
typedef struct CliHighlightLengthCtx CliHighlightLengthCtx;
typedef struct CliHighlightCtx CliHighlightCtx;
typedef struct CliHighlightNameCtx CliHighlightNameCtx;

// none: no token, no next, no state
#define CLI_HIGHLIGHT_NONE 0xffffffffu

// the max of each part one grammar contains as it is read
#define CLI_GRAMMAR_NODES (1u << 16u)

#define CLI_GRAMMAR_ITEMS (1u << 15u)

#define CLI_GRAMMAR_PARTS (1u << 16u)

#define CLI_GRAMMAR_RULES 8192u

#define CLI_GRAMMAR_TOKENS 16384u

#define CLI_GRAMMAR_LISTING 32768u

#define CLI_GRAMMAR_STATES 1024u

#define CLI_GRAMMAR_WORD_LISTS 1024u

#define CLI_GRAMMAR_WORDS 32768u

#define CLI_GRAMMAR_NAMES 4096u

#define CLI_GRAMMAR_TEXT (1u << 19u)

// the max rules one state contains, its includes read in
#define CLI_GRAMMAR_OWN 16384u

// the max characters of a line read, the max states stacked, and the max bytes the states each line ends in take
#define CLI_HIGHLIGHT_CHARACTERS (1u << 19u)

#define CLI_HIGHLIGHT_STACK 256u

// a token's types: a name, cases that pick one by the matched text, or one token for each group of the match
enum
{
    CLI_TOKEN_NAME,
    CLI_TOKEN_CASES,
    CLI_TOKEN_GROUPS
};

// A token: its type; a name's place among the names; or its cases or its groups, `count` from `first` in the cases
typedef struct
{
    unsigned int type;
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
// state, the characters any of its rules that colors or changes the state can start a match at, every such rule's first
// characters joined, and a table of the ASCII ones, 1 at each byte such a rule starts at, by which the colorer strides
// over the characters no rule takes in one lookup a character; for a state the grammar's `indent` runs, the place among the
// names of the name its close takes, none for any other; and whether a line begun with it on top begins a statement
typedef struct
{
    unsigned int name;
    unsigned int first;
    unsigned int count;
    CliRegexpFirst starts;
    unsigned char starts_at[128];
    unsigned int close;
    int begins;
} CliHighlightList;

// A grammar's parts, wherever they are stored: the patterns' nodes, items, child places and their first characters; the
// rules, tokens and cases; the rules of each state in turn, and the states; the words, each list's in order, and the
// lists; the names; and the texts, each ended by a NUL. Each count is how many it contains; whether the grammar starts
// each line in root and sets case aside; and of its `indent`, whether it has one, the columns a tab reaches to the
// next multiple of, and the rule whose pattern matches a line it passes over, a rule of no state, none for none
typedef struct
{
    CliRegexpNode *node;
    CliRegexpItem *item;
    unsigned int *part;
    CliRegexpFirst *part_first;
    CliHighlightRule *rule;
    CliHighlightToken *token;
    CliHighlightCase *cases;
    unsigned int *list_entries;
    CliHighlightList *state;
    unsigned int *word_text;
    CliHighlightList *words;
    unsigned int *name;
    char *text;
    unsigned int nodes;
    unsigned int items;
    unsigned int parts;
    unsigned int rules;
    unsigned int tokens;
    unsigned int case_count;
    unsigned int listing_count;
    unsigned int states;
    unsigned int word_count;
    unsigned int word_lists;
    unsigned int names;
    unsigned int text_size;
    int per_line;
    int ignore_case;
    int indents;
    unsigned int tab;
    unsigned int skip;
} CliGrammar;

// The work state a grammar is read into, its parts placed in one cell the store takes for one read at the sizes the
// grammar's JSON bounds, each no more than its CLI_GRAMMAR_ limit: the parts as a grammar contains them; each state's
// own rules and includes before the includes are read in, a rule's place or an include's name marked; the work state a
// pattern is read into; and how many of each the work state contains, the texts in bytes
typedef struct
{
    CliRegexpNode *node;
    CliRegexpItem *item;
    unsigned int *part;
    CliRegexpFirst *part_first;
    CliHighlightRule *rule;
    CliHighlightToken *token;
    CliHighlightCase *cases;
    unsigned int *list_entries;
    CliHighlightList *state;
    unsigned int *word_text;
    CliHighlightList *words;
    unsigned int *name;
    char *text;
    unsigned int *own;
    CliHighlightList *own_state;
    CliRegexpWorkState *pattern;
    unsigned int nodes_max;
    unsigned int items_max;
    unsigned int parts_max;
    unsigned int rules_max;
    unsigned int tokens_max;
    unsigned int listing_max;
    unsigned int states_max;
    unsigned int words_max;
    unsigned int word_lists_max;
    unsigned int names_max;
    unsigned int own_max;
    unsigned int text_max;
} CliGrammarWorkState;

// the bytes each line's ending is given among the endings: its states as JSON and the comma before it; a coloring whose
// endings take more leaves them cut
#define CLI_HIGHLIGHT_ENDING 256ull

// The coloring work state, its parts placed in one cell the store takes once and retains, at twice the sizes the
// largest coloring requested so far, taken again only where a larger one comes: a line's characters, `max` of them for
// the longest line; where each starts in UTF-16 units and the line's end last; its runs; the states stacked; the text a
// case is picked by, four bytes a character; and the state each of `count` lines ends in, the endings occupying
// `endings_size` bytes
typedef struct
{
    unsigned int *characters;
    unsigned int *cols;
    unsigned int *run_col;
    unsigned int *run_name;
    char (*stack)[CLI_NAME_MAX_LEN];
    char *picked;
    char *key;
    char *endings;
    unsigned long long max;
    unsigned long long count;
    unsigned long long endings_size;
} CliHighlightWorkState;

// the grammar at `def` in `json` read into `grammar`, its parts in `capacity` (highlight.c): 1, or 0 with why given
int cli_highlight_read(const CliHighlightJsonCtx *args);
// a rule's pattern as the matcher reads it, its arrays the grammar's
void cli_highlight_pattern(const CliHighlightCtx *args);
// the state named `name`, `length` bytes, CLI_HIGHLIGHT_NONE where the grammar has none; whether a list of words
// contains a word, `length` bytes
unsigned int cli_highlight_state(const CliHighlightNameCtx *args);
int cli_highlight_has_word(const CliHighlightLengthCtx *args);
// `count` lines colored with a grammar in `capacity`, written as cli_highlight_color writes them; and the same lines'
// leaves passed to `each` as cli_highlight_leaves passes them (highlight_line.c)
void cli_highlight_lines(const CliHighlightWorkStateCtx *args);
void cli_highlight_leaves_get(const CliHighlightWorkStateStateCtx *args);

struct CliHighlightWorkStateStateCtx
{
    const CliGrammar *grammar;
    CliHighlightWorkState *work_state;
    const char *state;
    const char *const *lines;
    unsigned int count;
    int ends;
    CliLeavesEach each;
    void *context;
    VerbaLinea *ended;
};

struct CliHighlightWorkStateCtx
{
    const CliGrammar *grammar;
    CliHighlightWorkState *work_state;
    const char *state;
    const char *const *lines;
    unsigned int count;
    VerbaLinea *out;
};

struct CliHighlightJsonCtx
{
    const CliJson *json;
    unsigned int def;
    CliGrammarWorkState *grammar_work_state_pointer;
    CliGrammar *grammar_out;
    VerbaLinea *err_msg;
};

struct CliHighlightLengthCtx
{
    const CliGrammar *grammar;
    unsigned long long length;
    const CliHighlightList *list;
    const char *word_text;
};

struct CliHighlightCtx
{
    const CliGrammar *grammar;
    const CliHighlightRule *rule;
    CliRegexp *pattern;
};

struct CliHighlightNameCtx
{
    const CliGrammar *grammar;
    const char *name;
    unsigned long long length;
};

typedef struct
{
    int (*read)(const CliHighlightJsonCtx *args);
} CliHighlightJsonNs;
EMBED_TABLE_LAYOUT(CliHighlightJsonNs, read);
EMBED_TABLE_STORAGE CliHighlightJsonNs cli_highlight_json_internal EMBED_UNUSED = {
    .read = cli_highlight_read,
};

typedef struct
{
    void (*pattern)(const CliHighlightCtx *args);
} CliHighlightNsTable;
EMBED_TABLE_LAYOUT(CliHighlightNsTable, pattern);
EMBED_TABLE_STORAGE CliHighlightNsTable cli_highlight_internal EMBED_UNUSED = {
    .pattern = cli_highlight_pattern,
};

typedef struct
{
    unsigned int (*state)(const CliHighlightNameCtx *args);
} CliHighlightNameNs;
EMBED_TABLE_LAYOUT(CliHighlightNameNs, state);
EMBED_TABLE_STORAGE CliHighlightNameNs cli_highlight_name_internal EMBED_UNUSED = {
    .state = cli_highlight_state,
};

typedef struct
{
    int (*has_word)(const CliHighlightLengthCtx *args);
} CliHighlightLengthNs;
EMBED_TABLE_LAYOUT(CliHighlightLengthNs, has_word);
EMBED_TABLE_STORAGE CliHighlightLengthNs cli_highlight_length_internal EMBED_UNUSED = {
    .has_word = cli_highlight_has_word,
};

typedef struct
{
    void (*lines)(const CliHighlightWorkStateCtx *args);
} CliHighlightWorkStateNs;
EMBED_TABLE_LAYOUT(CliHighlightWorkStateNs, lines);
EMBED_TABLE_STORAGE CliHighlightWorkStateNs cli_highlight_capacity_internal EMBED_UNUSED = {
    .lines = cli_highlight_lines,
};

typedef struct
{
    void (*leaves_get)(const CliHighlightWorkStateStateCtx *args);
} CliHighlightWorkStateStateNs;
EMBED_TABLE_LAYOUT(CliHighlightWorkStateStateNs, leaves_get);
EMBED_TABLE_STORAGE CliHighlightWorkStateStateNs cli_highlight_capacity_state_internal EMBED_UNUSED = {
    .leaves_get = cli_highlight_leaves_get,
};

#endif
