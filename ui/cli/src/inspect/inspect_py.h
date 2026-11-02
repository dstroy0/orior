// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py.h: Python as the inspections read it: its text as logical lines of tokens, each line's indent the width
// of its first physical line's, a tab reaching the next multiple of eight. A string is one token, and the names an
// f-string's fields read are tokens of their own (inspect_py_lex.c, inspect_py_*.c)
#ifndef INSPECT_PY_H
#define INSPECT_PY_H
typedef struct CliInspectPySrcCfg CliInspectPySrcCfg;
typedef struct CliInspectPySrcWordCfg CliInspectPySrcWordCfg;
typedef struct CliInspectPySrcSizeCfg CliInspectPySrcSizeCfg;
typedef struct CliInspectPySrcLineHeadCfg CliInspectPySrcLineHeadCfg;
typedef struct CliInspectPySrcToksCfg CliInspectPySrcToksCfg;
typedef struct CliInspectPySrcLineCfg CliInspectPySrcLineCfg;
typedef struct CliInspectPySrcTokCfg CliInspectPySrcTokCfg;
typedef struct CliInspectPyCfg CliInspectPyCfg;

#include "embed_dispatch_layout.h"
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
// `at`; whether the word a line starts with, past `async`, is `word`; and whether a run of tokens contains `word`
// (inspect_py_lex.c)
void cli_py_read(const CliInspectPySrcSizeCfg *args);
const CliInspectTok *cli_py_tok(const CliInspectPyCfg *args);
int cli_py_is(const CliInspectPySrcTokCfg *args);
int cli_py_keyword(const CliInspectPySrcTokCfg *args);
int cli_py_opens(const CliInspectPySrcLineCfg *args);
unsigned int cli_py_block_end(const CliInspectPyCfg *args);
int cli_py_head_is(const CliInspectPySrcLineCfg *args);
int cli_py_contains(const CliInspectPySrcWordCfg *args);

// how many brackets are open before the token at `offset` of `count` tokens (inspect_py_unread.c)
int cli_py_depth_at(const CliInspectPySrcToksCfg *args);
// the span from the start of the line `from` to the end of the line `to` whole, its line end with it where another
// line follows, as a fix takes lines out (inspect_py_unread.c)
void cli_py_full_line_span(const CliInspectPySrcCfg *args);

// the names a function sets and never reads, for the function whose header is the line at `at`; and the code after a
// return, a raise, a break or a continue in the same block, which no path reaches (inspect_py_unread.c,
// inspect_py_reach.c)
void cli_py_unread(const CliInspectPySrcSizeCfg *args);
void cli_py_unreachable(const CliInspectPySrcSizeCfg *args);
// the docstrings out of PEP 257's form: the module's, and each function's and class's (inspect_py_doc.c)
void cli_py_docstrings(const CliInspectPySrcSizeCfg *args);
// the innermost function, where `head` is "def", or class, where it is "class", whose header or block contains `line`:
// its line's index, or g_cli_py_lines for none (inspect_py_write.c)
unsigned int cli_py_around(const CliInspectPySrcLineHeadCfg *args);
// the imports a file makes as it runs, the classes it declares at its top level, and the functions it declares
// (inspect_py_facts.c, inspect_py_classes.c)
void cli_py_imports(const CliInspectPySrcSizeCfg *args);
void cli_py_classes(const CliInspectPySrcSizeCfg *args);
void cli_py_functions(const CliInspectPySrcSizeCfg *args);

struct CliInspectPySrcCfg
{
    const char *src;
    unsigned long long size;
    unsigned int from;
    unsigned int to;
    CliInspectPosition *start;
    CliInspectPosition *end;
};

struct CliInspectPySrcWordCfg
{
    const char *src;
    const char *word;
    const CliInspectTok *toks;
    unsigned int count;
};

struct CliInspectPySrcSizeCfg
{
    const char *src;
    unsigned long long size;
    unsigned int at;
    CliFacts *facts;
};

struct CliInspectPySrcLineHeadCfg
{
    const char *src;
    unsigned int line_u32;
    const char *head;
};

struct CliInspectPySrcToksCfg
{
    const char *src;
    const CliInspectTok *toks;
    unsigned int offset;
};

struct CliInspectPySrcLineCfg
{
    const char *src;
    const CliPyLine *line;
    const char *word;
};

struct CliInspectPySrcTokCfg
{
    const char *src;
    const CliInspectTok *tok;
    const char *word;
};

struct CliInspectPyCfg
{
    const CliPyLine *line;
    unsigned int at;
};

typedef struct
{
    void (*py_read)(const CliInspectPySrcSizeCfg *args);
    void (*py_unread)(const CliInspectPySrcSizeCfg *args);
    void (*py_unreachable)(const CliInspectPySrcSizeCfg *args);
    void (*py_docstrings)(const CliInspectPySrcSizeCfg *args);
    void (*py_imports)(const CliInspectPySrcSizeCfg *args);
    void (*py_classes)(const CliInspectPySrcSizeCfg *args);
    void (*py_functions)(const CliInspectPySrcSizeCfg *args);
} CliInspectPySrcSizeNs;
EMBED_TABLE_LAYOUT(CliInspectPySrcSizeNs, py_read, py_unread, py_unreachable, py_docstrings, py_imports, py_classes,
                   py_functions);
EMBED_TABLE_STORAGE CliInspectPySrcSizeNs cli_inspect_py_src_size EMBED_UNUSED = {
    .py_read = cli_py_read,
    .py_unread = cli_py_unread,
    .py_unreachable = cli_py_unreachable,
    .py_docstrings = cli_py_docstrings,
    .py_imports = cli_py_imports,
    .py_classes = cli_py_classes,
    .py_functions = cli_py_functions,
};

typedef struct
{
    const CliInspectTok *(*py_tok)(const CliInspectPyCfg *args);
    unsigned int (*py_block_end)(const CliInspectPyCfg *args);
} CliInspectPyNs;
EMBED_TABLE_LAYOUT(CliInspectPyNs, py_tok, py_block_end);
EMBED_TABLE_STORAGE CliInspectPyNs cli_inspect_py EMBED_UNUSED = {
    .py_tok = cli_py_tok,
    .py_block_end = cli_py_block_end,
};

typedef struct
{
    int (*py_is)(const CliInspectPySrcTokCfg *args);
    int (*py_keyword)(const CliInspectPySrcTokCfg *args);
} CliInspectPySrcTokNs;
EMBED_TABLE_LAYOUT(CliInspectPySrcTokNs, py_is, py_keyword);
EMBED_TABLE_STORAGE CliInspectPySrcTokNs cli_inspect_py_src_tok EMBED_UNUSED = {
    .py_is = cli_py_is,
    .py_keyword = cli_py_keyword,
};

typedef struct
{
    int (*py_opens)(const CliInspectPySrcLineCfg *args);
    int (*py_head_is)(const CliInspectPySrcLineCfg *args);
} CliInspectPySrcLineNs;
EMBED_TABLE_LAYOUT(CliInspectPySrcLineNs, py_opens, py_head_is);
EMBED_TABLE_STORAGE CliInspectPySrcLineNs cli_inspect_py_src_line EMBED_UNUSED = {
    .py_opens = cli_py_opens,
    .py_head_is = cli_py_head_is,
};

typedef struct
{
    int (*py_contains)(const CliInspectPySrcWordCfg *args);
} CliInspectPySrcWordNs;
EMBED_TABLE_LAYOUT(CliInspectPySrcWordNs, py_contains);
EMBED_TABLE_STORAGE CliInspectPySrcWordNs cli_inspect_py_src_word EMBED_UNUSED = {
    .py_contains = cli_py_contains,
};

typedef struct
{
    int (*py_depth_at)(const CliInspectPySrcToksCfg *args);
} CliInspectPySrcToksNs;
EMBED_TABLE_LAYOUT(CliInspectPySrcToksNs, py_depth_at);
EMBED_TABLE_STORAGE CliInspectPySrcToksNs cli_inspect_py_src_toks EMBED_UNUSED = {
    .py_depth_at = cli_py_depth_at,
};

typedef struct
{
    void (*py_full_line_span)(const CliInspectPySrcCfg *args);
} CliInspectPySrcNs;
EMBED_TABLE_LAYOUT(CliInspectPySrcNs, py_full_line_span);
EMBED_TABLE_STORAGE CliInspectPySrcNs cli_inspect_py_src EMBED_UNUSED = {
    .py_full_line_span = cli_py_full_line_span,
};

typedef struct
{
    unsigned int (*py_around)(const CliInspectPySrcLineHeadCfg *args);
} CliInspectPySrcLineHeadNs;
EMBED_TABLE_LAYOUT(CliInspectPySrcLineHeadNs, py_around);
EMBED_TABLE_STORAGE CliInspectPySrcLineHeadNs cli_inspect_py_src_line_head EMBED_UNUSED = {
    .py_around = cli_py_around,
};

#endif
