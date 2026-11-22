// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_read_internal.h: what reading a pattern shares across regexp_read.c and regexp_atoms.c: the reader, and the
// steps of reading one calls in another
#ifndef REGEXP_READ_INTERNAL_H
#define REGEXP_READ_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "regexp.h"

typedef struct RegexpReadTypeCtx RegexpReadTypeCtx;
typedef struct RegexpReadCtx RegexpReadCtx;
typedef struct RegexpReadErrorCtx RegexpReadErrorCtx;
typedef struct RegexpReadLetterCtx RegexpReadLetterCtx;
typedef struct RegexpReadValueCtx RegexpReadValueCtx;
typedef struct RegexpReadLetterTypeCtx RegexpReadLetterTypeCtx;

// the max alternatives, parts of a sequence and items of a class read at one level
#define REGEXP_LEVEL_MAX 256u

#define REGEXP_FAILED 0xffffffffu

// What a read tracks: the pattern's characters, where the read is, the groups counted, and whether the flags hold `u`.
// It also tracks the regexp it reads into, the source, and the line a failure goes onto
typedef struct
{
    unsigned int characters[REGEXP_CHARACTERS];
    unsigned long long length;
    unsigned long long at;
    unsigned int groups;
    int unicode;
    Regexp *regexp;
    const char *source;
    VerbaLinea *err_msg;
    int failed;
} RegexpReader;

// a failure reported, `/<source>/: <why> at <place>`: REGEXP_FAILED
unsigned int regexp_fail(const RegexpReadErrorCtx *args);
// the character where the read is, the character taken, and the character taken where it is what the read wants
int regexp_peek(const RegexpReadLetterTypeCtx *args);
int regexp_next(const RegexpReadLetterCtx *args);
int regexp_eat(const RegexpReadCtx *args);
// a node of `type` made; `count` children put in the parts as one node of `type`; each its index or REGEXP_FAILED
unsigned int regexp_node(const RegexpReadTypeCtx *args);
unsigned int regexp_parent(const RegexpReadTypeCtx *args);
// the decimal digits where reading is into `value`: 1, or 0 where none is there
int regexp_number(const RegexpReadValueCtx *args);
// alternation and an atom (regexp_read.c, regexp_atoms.c)
unsigned int regexp_alternation(const RegexpReadCtx *args);
unsigned int regexp_atom(const RegexpReadCtx *args);

struct RegexpReadTypeCtx
{
    RegexpReader *reader;
    unsigned int type;
    const unsigned int *children;
    unsigned int count;
};

struct RegexpReadCtx
{
    RegexpReader *reader;
    unsigned int required;
};

struct RegexpReadErrorCtx
{
    RegexpReader *reader;
    const char *why;
};

struct RegexpReadLetterCtx
{
    RegexpReader *reader;
    unsigned int *character;
};

struct RegexpReadValueCtx
{
    RegexpReader *reader;
    unsigned int *value;
};

struct RegexpReadLetterTypeCtx
{
    const RegexpReader *reader_read;
    unsigned int *character;
};

typedef struct
{
    unsigned int (*fail)(const RegexpReadErrorCtx *args);
} RegexpReadErrorNs;
EMBED_TABLE_LAYOUT(RegexpReadErrorNs, fail);
EMBED_TABLE_STORAGE RegexpReadErrorNs regexp_read_error_internal EMBED_UNUSED = {
    .fail = regexp_fail,
};

typedef struct
{
    int (*peek)(const RegexpReadLetterTypeCtx *args);
} RegexpReadLetterTypeNs;
EMBED_TABLE_LAYOUT(RegexpReadLetterTypeNs, peek);
EMBED_TABLE_STORAGE RegexpReadLetterTypeNs regexp_read_character_type_internal EMBED_UNUSED = {
    .peek = regexp_peek,
};

typedef struct
{
    int (*next)(const RegexpReadLetterCtx *args);
} RegexpReadLetterNs;
EMBED_TABLE_LAYOUT(RegexpReadLetterNs, next);
EMBED_TABLE_STORAGE RegexpReadLetterNs regexp_read_character_internal EMBED_UNUSED = {
    .next = regexp_next,
};

typedef struct
{
    int (*eat)(const RegexpReadCtx *args);
    unsigned int (*alternation)(const RegexpReadCtx *args);
    unsigned int (*atom)(const RegexpReadCtx *args);
} RegexpReadNs;
EMBED_TABLE_LAYOUT(RegexpReadNs, eat, alternation, atom);
EMBED_TABLE_STORAGE RegexpReadNs regexp_read_internal EMBED_UNUSED = {
    .eat = regexp_eat,
    .alternation = regexp_alternation,
    .atom = regexp_atom,
};

typedef struct
{
    unsigned int (*node)(const RegexpReadTypeCtx *args);
    unsigned int (*parent)(const RegexpReadTypeCtx *args);
} RegexpReadTypeNs;
EMBED_TABLE_LAYOUT(RegexpReadTypeNs, node, parent);
EMBED_TABLE_STORAGE RegexpReadTypeNs regexp_read_type_internal EMBED_UNUSED = {
    .node = regexp_node,
    .parent = regexp_parent,
};

typedef struct
{
    int (*number)(const RegexpReadValueCtx *args);
} RegexpReadValueNs;
EMBED_TABLE_LAYOUT(RegexpReadValueNs, number);
EMBED_TABLE_STORAGE RegexpReadValueNs regexp_read_value_internal EMBED_UNUSED = {
    .number = regexp_number,
};

#endif
