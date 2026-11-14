// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sort.c: the sort of sort.h: the runs to sort retained on a stack, the larger half of a split pushed and the smaller
// sorted first, so the stack holds at max a run a bit of the count; a short run finished by insertion and a deep one
// by a heap
#include "sort.h"

// the quicksort's stack: two bounds a level, a level for each bit of a count
#define CLI_SORT_LEVELS (sizeof(unsigned long long) * 8u)

// a word read and written at any alignment, the type the record moves go through
typedef embed_word EMBED_RAW CliSortWord;

typedef struct
{
    unsigned char *base;
    unsigned long long size;
    unsigned long long first;
    unsigned long long last;
    int (*order)(const void *one, const void *other);
} CliSortCtx;

// the records at `one` and `other`, `size` bytes each, exchanged a word at a time, then a byte at a time
EMBED_INLINE void sort_swapped(unsigned char *one, unsigned char *other, unsigned long long size)
{
    unsigned long long at = 0ULL;
    while (at + sizeof(embed_word) <= size)
    {
        CliSortWord *const one_word = (CliSortWord *)(void *)(one + at);
        CliSortWord *const other_word = (CliSortWord *)(void *)(other + at);
        const embed_word retained = *one_word;
        *one_word = *other_word;
        *other_word = retained;
        at += sizeof(embed_word);
    }
    while (at < size)
    {
        const unsigned char retained = one[at];
        one[at] = other[at];
        other[at] = retained;
        at += 1ULL;
    }
}

// the record at `at`
EMBED_INLINE unsigned char *sort_record(const CliSortCtx *args, unsigned long long at)
{
    return args->base + (at * args->size);
}

// the records from `first` through `last` put in order by insertion: each record moved back past the records that
// come after it
EMBED_INLINE void sort_inserted(const CliSortCtx *args)
{
    for (unsigned long long at = args->first + 1ULL; at <= args->last; at += 1ULL)
    {
        unsigned long long place = at;
        while ((place > args->first) && (args->order(sort_record(args, place - 1ULL), sort_record(args, place)) > 0))
        {
            sort_swapped(sort_record(args, place - 1ULL), sort_record(args, place), args->size);
            place -= 1ULL;
        }
    }
}

// the record at `at` of the heap over `first` through `last` sunk below its children until both come after it
EMBED_INLINE void sort_sunk(const CliSortCtx *args, unsigned long long at)
{
    const unsigned long long count = args->last - args->first + 1ULL;
    unsigned long long place = at;
    while (((2ULL * place) + 1ULL) < count)
    {
        const unsigned long long left = (2ULL * place) + 1ULL;
        const unsigned long long right = left + 1ULL;
        unsigned long long larger = left;
        if ((right < count) &&
            (args->order(sort_record(args, args->first + left), sort_record(args, args->first + right)) < 0))
        {
            larger = right;
        }
        if (args->order(sort_record(args, args->first + place), sort_record(args, args->first + larger)) >= 0)
        {
            return;
        }
        sort_swapped(sort_record(args, args->first + place), sort_record(args, args->first + larger), args->size);
        place = larger;
    }
}

// the records from `first` through `last` put in order by a heap: the heap built from the last parent down, then its
// top moved to the end and the heap shortened, until one record is left
EMBED_INLINE void sort_heaped(const CliSortCtx *args)
{
    CliSortCtx heap = *args;
    const unsigned long long count = heap.last - heap.first + 1ULL;
    for (unsigned long long parent = count / 2ULL; parent > 0ULL; parent -= 1ULL)
    {
        sort_sunk(&heap, parent - 1ULL);
    }
    while (heap.last > heap.first)
    {
        sort_swapped(sort_record(&heap, heap.first), sort_record(&heap, heap.last), heap.size);
        heap.last -= 1ULL;
        sort_sunk(&heap, 0ULL);
    }
}

// the records from `first` through `last` split about the middle of their first, middle and last records: the place
// the pivot ends at, every record before it at or before the pivot in the order and every record after it at or after
EMBED_INLINE unsigned long long sort_split(const CliSortCtx *args)
{
    const unsigned long long middle = args->first + ((args->last - args->first) / 2ULL);
    unsigned char *const first = sort_record(args, args->first);
    unsigned char *const center = sort_record(args, middle);
    unsigned char *const last = sort_record(args, args->last);
    // the three ordered among themselves, which leaves their middle at the first place
    if (args->order(center, first) < 0)
    {
        sort_swapped(center, first, args->size);
    }
    if (args->order(last, first) < 0)
    {
        sort_swapped(last, first, args->size);
    }
    if (args->order(center, last) < 0)
    {
        sort_swapped(center, last, args->size);
    }
    sort_swapped(center, first, args->size);
    unsigned long long low = args->first + 1ULL;
    unsigned long long high = args->last;
    while (1)
    {
        while ((low <= high) && (args->order(sort_record(args, low), first) < 0))
        {
            low += 1ULL;
        }
        while ((high > args->first) && (args->order(sort_record(args, high), first) > 0))
        {
            high -= 1ULL;
        }
        if (low >= high)
        {
            break;
        }
        sort_swapped(sort_record(args, low), sort_record(args, high), args->size);
        low += 1ULL;
        high -= 1ULL;
    }
    sort_swapped(first, sort_record(args, high), args->size);
    return high;
}

// A run waiting to be sorted: its bounds and how many splits made it
typedef struct
{
    unsigned long long first;
    unsigned long long last;
    unsigned int level;
} CliSortRun;

// the levels a run goes through before a heap finishes it: twice the bits the count takes
EMBED_INLINE unsigned int sort_levels_max(unsigned long long count)
{
    unsigned int bits = 0u;
    while (count != 0ULL)
    {
        count >>= 1u;
        bits += 1u;
    }
    return 2u * bits;
}

void cli_sort(const CliSortCfg *args)
{
    if (args->count < 2ULL)
    {
        return;
    }
    CliSortRun stack[CLI_SORT_LEVELS];
    stack[0] = (CliSortRun){0ULL, args->count - 1ULL, 0u};
    unsigned int held = 1u;
    const unsigned int levels_max = sort_levels_max(args->count);
    while (held > 0u)
    {
        held -= 1u;
        CliSortRun run = stack[held];
        CliSortCtx bounds = {(unsigned char *)args->base, args->size, run.first, run.last, args->order};
        while (bounds.last > bounds.first)
        {
            if ((bounds.last - bounds.first) < CLI_SORT_SHORT)
            {
                sort_inserted(&bounds);
                break;
            }
            if (run.level >= levels_max)
            {
                sort_heaped(&bounds);
                break;
            }
            run.level += 1u;
            const unsigned long long pivot = sort_split(&bounds);
            // the smaller side sorted next, the larger pushed; a side of no records is dropped
            const unsigned long long before = pivot - bounds.first;
            const unsigned long long after = bounds.last - pivot;
            if (before < after)
            {
                stack[held] = (CliSortRun){pivot + 1ULL, bounds.last, run.level};
                held += 1u;
                bounds.last = (pivot > bounds.first) ? pivot - 1ULL : bounds.first;
            }
            else
            {
                if (pivot > bounds.first)
                {
                    stack[held] = (CliSortRun){bounds.first, pivot - 1ULL, run.level};
                    held += 1u;
                }
                bounds.first = pivot + 1ULL;
            }
        }
    }
}
