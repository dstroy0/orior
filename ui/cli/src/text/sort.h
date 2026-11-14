// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sort.h: `count` records of `size` bytes at `base` put in the order `order` gives any two of them: a quicksort over
// the middle of three as its pivot, each run of CLI_SORT_SHORT records or fewer finished by insertion, and a heap sort
// where the quicksort's depth passes twice the count's width. The records move a word at a time. Two records the order
// reads the same may swap places
#ifndef CLI_SORT_H
#define CLI_SORT_H

#include "embed_compiler_directives.h"
#include "embed_types.h"

// the records a run has at most for insertion to finish it
#define CLI_SORT_SHORT 16ULL

typedef struct
{
    void *base;
    unsigned long long count;
    unsigned long long size;
    int (*order)(const void *one, const void *other);
} CliSortCfg;

// `count` records of `size` bytes at `base` put in the order `order` gives
void cli_sort(const CliSortCfg *args);

#endif
