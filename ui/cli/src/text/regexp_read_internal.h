// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_read_internal.h: what reading a pattern shares across regexp_read.c and regexp_atoms.c: the reader, and the
// steps of reading one calls in another
#ifndef REGEXP_READ_INTERNAL_H
#define REGEXP_READ_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "regexp.h"

typedef struct CliRegexpReadTypeCtx CliRegexpReadTypeCtx;
typedef struct CliRegexpReadCtx CliRegexpReadCtx;
typedef struct CliRegexpReadErrorCtx CliRegexpReadErrorCtx;
typedef struct CliRegexpReadLetterCtx CliRegexpReadLetterCtx;
typedef struct CliRegexpReadValueCtx CliRegexpReadValueCtx;
typedef struct CliRegexpReadLetterTypeCtx CliRegexpReadLetterTypeCtx;

// the max alternatives, parts of a sequence and items of a class read at one level
#define CLI_REGEXP_LEVEL_MAX 256u

#define CLI_REGEXP_FAILED 0xffffffffu

// What reading tracks: the pattern's characters, where reading is, the groups counted, whether `u` is set, the regexp
// it reads into, the source and where a failure is reported
typedef struct
{
    unsigned int characters[CLI_REGEXP_CHARACTERS];
    unsigned long long length;
    unsigned long long at;
    unsigned int groups;
    int unicode;
    CliRegexp *regexp;
    const char *source;
    VerbaLinea *err_msg;
    int failed;
} CliRegexpReader;

// a failure reported, `/<source>/: <why> at <place>`: CLI_REGEXP_FAILED
unsigned int cli_fail(const CliRegexpReadErrorCtx *args);
// the character where reading is, the character taken, and the character taken where it is the one wanted
int cli_peek(const CliRegexpReadLetterTypeCtx *args);
int cli_next(const CliRegexpReadLetterCtx *args);
int cli_eat(const CliRegexpReadCtx *args);
// a node of `type` made; `count` children put in the parts as one node of `type`; each its index or CLI_REGEXP_FAILED
unsigned int cli_node(const CliRegexpReadTypeCtx *args);
unsigned int cli_parent(const CliRegexpReadTypeCtx *args);
// the decimal digits where reading is into `value`: 1, or 0 where none is there
int cli_number(const CliRegexpReadValueCtx *args);
// alternation and an atom (regexp_read.c, regexp_atoms.c)
unsigned int cli_alternation(const CliRegexpReadCtx *args);
unsigned int cli_atom(const CliRegexpReadCtx *args);

struct CliRegexpReadTypeCtx
{
    CliRegexpReader *reader;
    unsigned int type;
    const unsigned int *children;
    unsigned int count;
};

struct CliRegexpReadCtx
{
    CliRegexpReader *reader;
    unsigned int required;
};

struct CliRegexpReadErrorCtx
{
    CliRegexpReader *reader;
    const char *why;
};

struct CliRegexpReadLetterCtx
{
    CliRegexpReader *reader;
    unsigned int *character;
};

struct CliRegexpReadValueCtx
{
    CliRegexpReader *reader;
    unsigned int *value;
};

struct CliRegexpReadLetterTypeCtx
{
    const CliRegexpReader *reader_read;
    unsigned int *character;
};

typedef struct
{
    unsigned int (*fail)(const CliRegexpReadErrorCtx *args);
} CliRegexpReadErrorNs;
EMBED_TABLE_LAYOUT(CliRegexpReadErrorNs, fail);
EMBED_TABLE_STORAGE CliRegexpReadErrorNs cli_regexp_read_error_internal EMBED_UNUSED = {
    .fail = cli_fail,
};

typedef struct
{
    int (*peek)(const CliRegexpReadLetterTypeCtx *args);
} CliRegexpReadLetterTypeNs;
EMBED_TABLE_LAYOUT(CliRegexpReadLetterTypeNs, peek);
EMBED_TABLE_STORAGE CliRegexpReadLetterTypeNs cli_regexp_read_character_type_internal EMBED_UNUSED = {
    .peek = cli_peek,
};

typedef struct
{
    int (*next)(const CliRegexpReadLetterCtx *args);
} CliRegexpReadLetterNs;
EMBED_TABLE_LAYOUT(CliRegexpReadLetterNs, next);
EMBED_TABLE_STORAGE CliRegexpReadLetterNs cli_regexp_read_character_internal EMBED_UNUSED = {
    .next = cli_next,
};

typedef struct
{
    int (*eat)(const CliRegexpReadCtx *args);
    unsigned int (*alternation)(const CliRegexpReadCtx *args);
    unsigned int (*atom)(const CliRegexpReadCtx *args);
} CliRegexpReadNs;
EMBED_TABLE_LAYOUT(CliRegexpReadNs, eat, alternation, atom);
EMBED_TABLE_STORAGE CliRegexpReadNs cli_regexp_read_internal EMBED_UNUSED = {
    .eat = cli_eat,
    .alternation = cli_alternation,
    .atom = cli_atom,
};

typedef struct
{
    unsigned int (*node)(const CliRegexpReadTypeCtx *args);
    unsigned int (*parent)(const CliRegexpReadTypeCtx *args);
} CliRegexpReadTypeNs;
EMBED_TABLE_LAYOUT(CliRegexpReadTypeNs, node, parent);
EMBED_TABLE_STORAGE CliRegexpReadTypeNs cli_regexp_read_type_internal EMBED_UNUSED = {
    .node = cli_node,
    .parent = cli_parent,
};

typedef struct
{
    int (*number)(const CliRegexpReadValueCtx *args);
} CliRegexpReadValueNs;
EMBED_TABLE_LAYOUT(CliRegexpReadValueNs, number);
EMBED_TABLE_STORAGE CliRegexpReadValueNs cli_regexp_read_value_internal EMBED_UNUSED = {
    .number = cli_number,
};

#endif
