// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vector.h: the inner loops of the page's slow modules, over text held in UTF-16 units as the page's own strings hold
// it, each written in the processor's vector instructions: SSE2, and AVX2 where the machine has it, on x86-64
// (vector_x86.c); NEON on aarch64 (vector_neon.c); and a plain loop where neither is there (vector.c). Each path
// answers as the plain loop does, which the checks hold each one to on every path the machine runs.
//
// A search without case folds as the page's own does with the `i` and `u` flags: each unit of the text and of what is
// sought to its simple case fold. Only text sought in ASCII is searched here; two units outside ASCII fold into it,
// `ſ` to `s` and the Kelvin sign to `k`, and are folded too
#ifndef VECTOR_H
#define VECTOR_H

#include "../command_line/cli_internal.h"

// none found
#define CLI_VECTOR_NONE 0xffffffffffffffffull

// the two units outside ASCII that fold into it
#define CLI_LONG_S 0x017fu

#define CLI_KELVIN 0x212au

// the ways the loops run: the plain loop, or one of the processor's sets of vector instructions
enum
{
    CLI_VECTOR_PLAIN,
    CLI_VECTOR_SSE2,
    CLI_VECTOR_AVX2,
    CLI_VECTOR_NEON
};

// the paths this machine runs into `paths`, which holds 4, the plain loop first and the widest last: their count
unsigned int cli_vector_paths(unsigned int *paths);
// the widest path this machine runs
unsigned int cli_vector_best(void);
// a unit folded as the page's search without case folds it, for units that fold into ASCII
unsigned short cli_vector_fold(unsigned short unit);
// where `unit` first stands in `text`; where `sought` first stands in `text` from `from` on, unit for unit, and
// without case where `sought` is ASCII; whether two runs of units are the same; how many of the numbers at the starts
// of `a` and `b` are the same, one for one. Each on the widest path, or on the path given
unsigned long long cli_vector_find_unit(const unsigned short *text, unsigned long long length, unsigned short unit);
unsigned long long cli_vector_find(const unsigned short *text, unsigned long long length, const unsigned short *sought,
                                   unsigned long long sought_length, unsigned long long from);
unsigned long long cli_vector_find_folded(const unsigned short *text, unsigned long long length,
                                          const unsigned short *sought, unsigned long long sought_length,
                                          unsigned long long from);
int cli_vector_same(const unsigned short *a, unsigned long long a_length, const unsigned short *b,
                    unsigned long long b_length);
unsigned long long cli_vector_common(const unsigned long long *a, unsigned long long a_length,
                                     const unsigned long long *b, unsigned long long b_length);
unsigned long long cli_vector_find_unit_on(unsigned int path, const unsigned short *text, unsigned long long length,
                                           unsigned short unit);
unsigned long long cli_vector_find_on(unsigned int path, const unsigned short *text, unsigned long long length,
                                      const unsigned short *sought, unsigned long long sought_length,
                                      unsigned long long from, int folded);
int cli_vector_same_on(unsigned int path, const unsigned short *a, unsigned long long a_length, const unsigned short *b,
                       unsigned long long b_length);
unsigned long long cli_vector_common_on(unsigned int path, const unsigned long long *a, const unsigned long long *b,
                                        unsigned long long length);

// what vector.c and the paths share: whether `sought` stands in `text` at `at`, each unit folded where `folded` is 1;
// the plain loop of a find from `start` on; and each path's loops, where the machine has them
int cli_vector_stands_at(const unsigned short *text, const unsigned short *sought, unsigned long long sought_length,
                         unsigned long long at, int folded);
unsigned long long cli_vector_plain_find(const unsigned short *text, unsigned long long length,
                                         const unsigned short *sought, unsigned long long sought_length, int folded,
                                         unsigned long long start);
int cli_vector_avx2_present(void);
unsigned long long cli_vector_find_unit_sse2(const unsigned short *text, unsigned long long length,
                                             unsigned short unit);
unsigned long long cli_vector_find_unit_avx2(const unsigned short *text, unsigned long long length,
                                             unsigned short unit);
unsigned long long cli_vector_find_unit_neon(const unsigned short *text, unsigned long long length,
                                             unsigned short unit);
unsigned long long cli_vector_common_sse2(const unsigned long long *a, const unsigned long long *b,
                                          unsigned long long length);
unsigned long long cli_vector_common_avx2(const unsigned long long *a, const unsigned long long *b,
                                          unsigned long long length);
unsigned long long cli_vector_common_neon(const unsigned long long *a, const unsigned long long *b,
                                          unsigned long long length);
int cli_vector_same_sse2(const unsigned short *a, const unsigned short *b, unsigned long long length);
int cli_vector_same_avx2(const unsigned short *a, const unsigned short *b, unsigned long long length);
int cli_vector_same_neon(const unsigned short *a, const unsigned short *b, unsigned long long length);
unsigned long long cli_vector_find_sse2(const unsigned short *text, unsigned long long length,
                                        const unsigned short *sought, unsigned long long sought_length, int folded);
unsigned long long cli_vector_find_avx2(const unsigned short *text, unsigned long long length,
                                        const unsigned short *sought, unsigned long long sought_length, int folded);
unsigned long long cli_vector_find_neon(const unsigned short *text, unsigned long long length,
                                        const unsigned short *sought, unsigned long long sought_length, int folded);

#endif
