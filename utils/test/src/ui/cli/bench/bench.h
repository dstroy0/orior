// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bench.h: what the bench's parts share: a held file, a pass's measure, the row written for it, and the rule bench
#ifndef CLI_BENCH_H
#define CLI_BENCH_H

#include "../../../../../../ui/cli/src/command_line/cli_internal.h"

// One file of the corpus kept: its bytes and their count
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
} BenchHeld;

// A pass's measure: the bytes it went through and the least microseconds a repeat took
typedef struct
{
    unsigned long long bytes;
    unsigned long long least;
} BenchMeasure;

// The corpus: the tree's top, each file's path and its bytes kept, how many, their bytes together, and how many times
// each pass runs
typedef struct
{
    const char *root;
    char (*paths)[CLI_PATH_LONGEST];
    const BenchHeld *held;
    unsigned int count;
    unsigned long long whole;
    unsigned int repeat;
} BenchCorpus;

// a row written for a pass: its name, bytes, microseconds and megabytes a second (cli_bench.c)
void bench_row(const char *name, BenchMeasure measure);

// every file of the corpus read and let go by `threads` threads at once, through the cells, or through the system's
// allocator alone where `plain`; the view of the tree's files under ui/cli/src picked as a search picks it; and the
// tree under ui/cli/src searched for `needle` as the window's search does it (cli_bench_files.c)
BenchMeasure bench_held_by(const BenchCorpus *corpus, unsigned int threads, int plain);
BenchMeasure bench_picked(const BenchCorpus *corpus);
BenchMeasure bench_search(const BenchCorpus *corpus, const char *needle);

// every kept file's lines colored with the grammar kept under `key`: the measure (cli_bench.c)
BenchMeasure bench_colored(const char *key, const BenchHeld *held, unsigned int count, unsigned int repeat,
                           unsigned long long largest);

// each rule of the C grammar's root tried at every letter of every kept file, a row for each; the cost of a
// line's coloring is seen rule by rule; then the corpus colored with grammars cut down to a few of the C grammar's
// rules, a row for each; the cost of each part of a line's coloring is therefore seen apart (cli_bench_rules.c)
void bench_rules(const char *root, const BenchHeld *held, unsigned int count, unsigned int repeat,
                 unsigned long long largest);

#endif
