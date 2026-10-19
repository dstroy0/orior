// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py.h: Python as the inspections read it: its text as logical lines of tokens, each line's indent the width
// of its first physical line's, a tab reaching the next multiple of eight. A string is one token, and the names an
// f-string's fields read are tokens of their own (inspect_py_lex.c, inspect_py_*.c)
#ifndef INSPECT_PY_H
#define INSPECT_PY_H

#include "inspect_internal.h"

// a token's kind
#define CLI_PY_NAME 0u
#define CLI_PY_NUMBER 1u
#define CLI_PY_STR 2u
#define CLI_PY_OP 3u

// A logical line: its indent, and its tokens, `count` of them from `first` among the reading's, never none
typedef struct
{
    unsigned int indent;
    unsigned int first;
    unsigned int count;
} CliPyLine;

// the logical lines of the reading and their count (inspect_py_lex.c)
extern CliPyLine *g_cli_py_line;
extern unsigned int g_cli_py_lines;

// Python's own words, which are never names a line reads, and how many there are
extern const char *const g_cli_py_keywords[35];

// `size` bytes of `src` read into the reading's tokens, its logical lines and the spans of its comments; the token at
// `at` of a line; whether the token's text is `word`; whether a token's text is one of Python's own words; whether a
// line opens a block, ending in a colon outside its brackets; the index past the last line of the block of the line at
// `at`; whether the word a line starts with, past `async`, is `word`; and whether a run of tokens holds `word`
// (inspect_py_lex.c)
void cli_py_read(const char *src, unsigned long long size);
const CliInspectTok *cli_py_tok(const CliPyLine *line, unsigned int at);
int cli_py_is(const char *src, const CliInspectTok *tok, const char *word);
int cli_py_keyword(const char *src, const CliInspectTok *tok);
int cli_py_opens(const char *src, const CliPyLine *line);
unsigned int cli_py_block_end(unsigned int at);
int cli_py_head_is(const char *src, const CliPyLine *line, const char *word);
int cli_py_holds(const char *src, const CliInspectTok *toks, unsigned int count, const char *word);

// how many brackets are open before the token at `offset` of `count` tokens (inspect_py_unread.c)
int cli_py_depth_at(const char *src, const CliInspectTok *toks, unsigned int offset);
// the span from the start of the line `from` to the end of the line `to` whole, its line end with it where another
// line follows, as a fix takes lines out (inspect_py_unread.c)
void cli_py_whole_lines(const char *src, unsigned long long size, unsigned int from, unsigned int to,
                        CliInspectPlace *start, CliInspectPlace *end);

// the names a function sets and never reads, for the function whose header is the line at `at`; and the code after a
// return, a raise, a break or a continue in the same block, which no path reaches (inspect_py_unread.c,
// inspect_py_reach.c)
void cli_py_unread(const char *src, unsigned long long size, unsigned int at, CliFacts *facts);
void cli_py_unreachable(const char *src, unsigned long long size, CliFacts *facts);
// the docstrings out of PEP 257's form: the module's, and each function's and class's (inspect_py_doc.c)
void cli_py_docstrings(const char *src, unsigned long long size, CliFacts *facts);
// the innermost function, where `head` is "def", or class, where it is "class", whose header or block holds `line`:
// its line's index, or g_cli_py_lines for none (inspect_py_write.c)
unsigned int cli_py_around(const char *src, unsigned int line, const char *head);
// the imports a file makes as it runs, the classes it declares at its top level, and the functions it declares
// (inspect_py_facts.c, inspect_py_classes.c)
void cli_py_imports(const char *src, CliFacts *facts);
void cli_py_classes(const char *src, CliFacts *facts);
void cli_py_functions(const char *src, CliFacts *facts);

#endif
