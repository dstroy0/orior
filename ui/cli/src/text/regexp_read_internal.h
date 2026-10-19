// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_read_internal.h: what reading a pattern shares across regexp_read.c and regexp_atoms.c: the reader, and the
// steps of reading one calls in another
#ifndef REGEXP_READ_INTERNAL_H
#define REGEXP_READ_INTERNAL_H

#include "regexp.h"

// the most alternatives, parts of a sequence and items of a class read at one level
#define CLI_REGEXP_LEVEL_MOST 256u

#define CLI_REGEXP_FAILED 0xffffffffu

// What reading keeps: the pattern's letters, where reading stands, the groups counted, whether `u` is set, the regexp
// it reads into, the source and where a failure is said
typedef struct
{
    unsigned int letters[CLI_REGEXP_LETTERS];
    unsigned long long length;
    unsigned long long at;
    unsigned int groups;
    int unicode;
    CliRegexp *regexp;
    const char *source;
    CliLine *said;
    int failed;
} CliRegexpReader;

// a failure said, `/<source>/: <why> at <place>`: CLI_REGEXP_FAILED
unsigned int cli_failed(CliRegexpReader *reader, const char *why);
// the letter where reading stands, the letter taken, and the letter taken where it is the one wanted
int cli_peek(const CliRegexpReader *reader, unsigned int *letter);
int cli_next(CliRegexpReader *reader, unsigned int *letter);
int cli_eat(CliRegexpReader *reader, unsigned int wanted);
// a node of `kind` made; `count` children put in the parts as one node of `kind`; each its index or CLI_REGEXP_FAILED
unsigned int cli_node(CliRegexpReader *reader, unsigned int kind);
unsigned int cli_parented(CliRegexpReader *reader, unsigned int kind, const unsigned int *children, unsigned int count);
// the decimal digits where reading stands into `value`: 1, or 0 where none stands there
int cli_number(CliRegexpReader *reader, unsigned int *value);
// alternation and an atom (regexp_read.c, regexp_atoms.c)
unsigned int cli_alternation(CliRegexpReader *reader);
unsigned int cli_atom(CliRegexpReader *reader);

#endif
