// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sift.h: where a needle of bytes stands in a text, the engine's sift (sift.c, sift_x86.c): the text read a window at
// a time, the bytes of each window counted, and the needle's anchors placed where the window says they refute most.
// The candidate offsets are ranked by the census, the rarest byte first, and the anchors are then placed one level at
// a time, each at the offset that leaves fewest of the alignments the anchors above it left standing, a level ending
// the descent where it leaves standing every one of them. The first two anchors are tested a block of alignments at a
// time in the processor's vector instructions, the rest one alignment at a time, and every alignment all of them agree
// with is compared whole.
//
// The census and the plan steer and never decide: an alignment stands only where every anchor agrees and the whole
// needle is equal there, whatever placed the anchors. A census taken on a sample and a plan scored on a sample cost
// speed and never a match. The checks hold every path to the plain loop.
//
// A needle searched without case is searched in ASCII case: each letter of the text and of the needle to its lower case
#ifndef SIFT_H
#define SIFT_H

#include "vector.h"

// the most anchors a plan places, and the bytes a window holds, the last needle's length less one of them the next
// window's first too
#define CLI_SIFT_ANCHORS 4u

#define CLI_SIFT_WINDOW (1u << 20u)

// How often each byte stands in a window, read on every so many of its bytes
typedef struct
{
    unsigned long long occurrences[256];
    unsigned long long total;
} CliCensus;

// the census of a window, of every so many of its bytes as keeps the count to a bound
void cli_sift_census(const unsigned char *window, unsigned long long length, int fold, CliCensus *census);
// the anchors placed on a needle for a window into `anchors`, which holds CLI_SIFT_ANCHORS: offsets into the needle,
// the first refuting most; their count
unsigned int cli_sift_plan(const unsigned char *window, unsigned long long length, const unsigned char *needle,
                           unsigned long long needle_length, int fold, unsigned long long *anchors);
// every offset `needle` stands at in `text`, read a window at a time, each handed to `found` in order, on the widest
// path or the path given: their count
unsigned long long cli_sift_find_all(const unsigned char *text, unsigned long long length, const unsigned char *needle,
                                     unsigned long long needle_length, int fold,
                                     void (*found)(void *context, unsigned long long at), void *context);
// every offset `needle` stands at in a text within one window, found by the needle's own shifts, Horspool's, with no
// plan made: their count; what cli_sift_find_all takes for such a text
unsigned long long cli_sift_horspool(const unsigned char *text, unsigned long long length, const unsigned char *needle,
                                     unsigned long long needle_length, int fold,
                                     void (*found)(void *context, unsigned long long at), void *context);
unsigned long long cli_sift_find_all_on(unsigned int path, const unsigned char *text, unsigned long long length,
                                        const unsigned char *needle, unsigned long long needle_length, int fold,
                                        void (*found)(void *context, unsigned long long at), void *context);

// what sift.c and sift_x86.c share: a byte as a search without case reads it; whether the anchors from `from` on
// agree at `at` and the whole needle stands there; the plain loop of a window from the alignment `start`; and the
// window's loops, sixteen or thirty-two alignments at a time. Each hands every alignment found to `found` from `base`
// on, and gives their count
unsigned char cli_sift_folded(unsigned char byte, int fold);
int cli_sift_holds(const unsigned char *window, const unsigned char *needle, unsigned long long needle_length,
                   const unsigned long long *anchors, unsigned int anchor_count, unsigned int from,
                   unsigned long long at, int fold);
unsigned long long cli_sift_plain_window(const unsigned char *window, unsigned long long length,
                                         const unsigned char *needle, unsigned long long needle_length,
                                         const unsigned long long *anchors, unsigned int anchor_count, int fold,
                                         unsigned long long start, unsigned long long base,
                                         void (*found)(void *context, unsigned long long at), void *context);
unsigned long long cli_sift_window_sse2(const unsigned char *window, unsigned long long length,
                                        const unsigned char *needle, unsigned long long needle_length,
                                        const unsigned long long *anchors, unsigned int anchor_count, int fold,
                                        unsigned long long base, void (*found)(void *context, unsigned long long at),
                                        void *context);
unsigned long long cli_sift_window_avx2(const unsigned char *window, unsigned long long length,
                                        const unsigned char *needle, unsigned long long needle_length,
                                        const unsigned long long *anchors, unsigned int anchor_count, int fold,
                                        unsigned long long base, void (*found)(void *context, unsigned long long at),
                                        void *context);

#endif
