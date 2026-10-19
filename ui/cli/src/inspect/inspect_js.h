// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js.h: JavaScript as the inspections read it: its tokens, a string, a template's text and a regular
// expression each one token, and the code in a template's `${}` tokens of its own. A slash starts a regular
// expression where a value is due, after an operator, an opening bracket or a word such as `return`, and divides
// elsewhere.
//
// Each brace is a function's body, a block, an object or a class's body, by what comes before it: `=>` or a parameter
// list after a name or `function` opens a function, `class` a class's body, an operator or an opening bracket an
// object, and anything else a block (inspect_js_lex.c, inspect_js_*.c)
#ifndef INSPECT_JS_H
#define INSPECT_JS_H

#include "inspect_internal.h"

// a token's kind
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
void cli_js_read(const char *src, unsigned long long size);
int cli_js_is(const char *src, unsigned int at, const char *word);
int cli_js_one_of(const char *src, unsigned int at, const char *const *words, unsigned int count);
int cli_js_is_read(const char *src, unsigned int at);
int cli_js_pair_depth(const char *src, unsigned int from, unsigned int to);

// the name of the function whose body opens at `open`, and the index of its token: `function name(`, a method's
// `name(`, and `name = function (` or `name = (...) =>` and `name = x =>`; CLI_JS_NO where it has none
// (inspect_js_facts.c)
unsigned int cli_js_function_name(const char *src, unsigned int open);
// the names a function whose body opens at `open` takes as its parameters, plainly or with a default or a rest, put in
// `names`, and the first token of its parameters (inspect_js_parse.c)
unsigned int cli_js_parameters(const char *src, unsigned int open, CliInspectSet *names);

#endif
