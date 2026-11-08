// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js.h: JavaScript as the inspections read it: its tokens, a string, a template's text and a regular
// expression each one token, and the code in a template's `${}` tokens of its own. A slash starts a regular
// expression where a value is expected, after an operator, an opening bracket or a word such as `return`, and divides
// elsewhere.
//
// Each brace is a function's body, a block, an object or a class's body, by what comes before it: `=>` or a parameter
// list after a name or `function` opens a function, `class` a class's body, an operator or an opening bracket an
// object, and anything else a block (inspect_js_lex.c, inspect_js_*.c)
#ifndef INSPECT_JS_H
#define INSPECT_JS_H
#include "embed_dispatch_layout.h"
#include "inspect_internal.h"

typedef struct CliInspectJsAtCfg CliInspectJsAtCfg;
typedef struct CliInspectJsAtWordCfg CliInspectJsAtWordCfg;
typedef struct CliInspectJsFromCfg CliInspectJsFromCfg;
typedef struct CliInspectJsOpenCfg CliInspectJsOpenCfg;
typedef struct CliInspectJsCfg CliInspectJsCfg;

// a token's type
#define CLI_JS_NAME 0u
#define CLI_JS_NUMBER 1u
#define CLI_JS_STR 2u
#define CLI_JS_REGEX 3u
#define CLI_JS_PUNCT 4u

// what a brace opens, none where the token is no brace
#define CLI_JS_NONE 0u
#define CLI_JS_FUNCTION 1u
#define CLI_JS_BLOCK 2u
#define CLI_JS_OBJECT 3u
#define CLI_JS_CLASS 4u

// no token
#define CLI_JS_NO 0xffffffffu

// the bracket each bracket matches, what each brace opens, and the brace each token stands directly inside, each by
// the token's index, CLI_JS_NO where there is none (inspect_js_lex.c)
extern unsigned int *g_cli_js_pair;
extern unsigned char *g_cli_js_brace;
extern unsigned int *g_cli_js_within;

// JavaScript's words that are never names a function reads, and the words before a parameter list that are no
// function's name
extern const char *const g_cli_js_keywords[35];
extern const char *const g_cli_js_control[7];

// `size` bytes of `src` read into the reading's tokens, their pairs, braces and the braces they stand in, and the
// spans of its comments; whether the token at `at` is `word`, CLI_JS_NO never; whether it is one of `count` words;
// whether the name at `at` is read: no keyword, no property after a dot, no key of an object and no method's name in an
// object or a class's body; and how many brackets the tokens from `from` up to `to` leave open (inspect_js_lex.c)
void cli_js_read(const CliInspectJsCfg *args);
int cli_js_is(const CliInspectJsAtWordCfg *args);
int cli_js_one_get(const CliInspectJsAtCfg *args);
int cli_js_is_read(const CliInspectJsAtWordCfg *args);
int cli_js_pair_depth(const CliInspectJsFromCfg *args);

// the name of the function whose body opens at `open`, and the index of its token: `function name(`, a method's
// `name(`, and `name = function (` or `name = (...) =>` and `name = x =>`; CLI_JS_NO where it has none
// (inspect_js_facts.c)
unsigned int cli_js_function_name(const CliInspectJsOpenCfg *args);
// the names a function whose body opens at `open` takes as its parameters, plainly or with a default or a rest, put in
// `names`, and the first token of its parameters (inspect_js_parse.c)
unsigned int cli_js_parameters(const CliInspectJsOpenCfg *args);

struct CliInspectJsAtCfg
{
    const char *src;
    unsigned int at;
    const char *const *words;
    unsigned int count;
};

struct CliInspectJsAtWordCfg
{
    const char *src;
    unsigned int at;
    const char *word_text;
};

struct CliInspectJsFromCfg
{
    const char *src;
    unsigned int from;
    unsigned int to;
};

struct CliInspectJsOpenCfg
{
    const char *src;
    unsigned int open;
    CliInspectSet *names;
};

struct CliInspectJsCfg
{
    const char *src;
    unsigned long long size;
};

typedef struct
{
    void (*js_read)(const CliInspectJsCfg *args);
} CliInspectJsNs;
EMBED_TABLE_LAYOUT(CliInspectJsNs, js_read);
EMBED_TABLE_STORAGE CliInspectJsNs cli_inspect_js EMBED_UNUSED = {
    .js_read = cli_js_read,
};

typedef struct
{
    int (*js_is)(const CliInspectJsAtWordCfg *args);
    int (*js_is_read)(const CliInspectJsAtWordCfg *args);
} CliInspectJsAtWordNs;
EMBED_TABLE_LAYOUT(CliInspectJsAtWordNs, js_is, js_is_read);
EMBED_TABLE_STORAGE CliInspectJsAtWordNs cli_inspect_js_at_word EMBED_UNUSED = {
    .js_is = cli_js_is,
    .js_is_read = cli_js_is_read,
};

typedef struct
{
    int (*js_one_get)(const CliInspectJsAtCfg *args);
} CliInspectJsAtNs;
EMBED_TABLE_LAYOUT(CliInspectJsAtNs, js_one_get);
EMBED_TABLE_STORAGE CliInspectJsAtNs cli_inspect_js_at EMBED_UNUSED = {
    .js_one_get = cli_js_one_get,
};

typedef struct
{
    int (*js_pair_depth)(const CliInspectJsFromCfg *args);
} CliInspectJsFromNs;
EMBED_TABLE_LAYOUT(CliInspectJsFromNs, js_pair_depth);
EMBED_TABLE_STORAGE CliInspectJsFromNs cli_inspect_js_from EMBED_UNUSED = {
    .js_pair_depth = cli_js_pair_depth,
};

typedef struct
{
    unsigned int (*js_function_name)(const CliInspectJsOpenCfg *args);
    unsigned int (*js_parameters)(const CliInspectJsOpenCfg *args);
} CliInspectJsOpenNs;
EMBED_TABLE_LAYOUT(CliInspectJsOpenNs, js_function_name, js_parameters);
EMBED_TABLE_STORAGE CliInspectJsOpenNs cli_inspect_js_open EMBED_UNUSED = {
    .js_function_name = cli_js_function_name,
    .js_parameters = cli_js_parameters,
};

#endif
