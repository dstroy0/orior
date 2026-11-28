// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the floor_match_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef FLOOR_MATCH_INTERNAL_H
#define FLOOR_MATCH_INTERNAL_H

// A search for x on the floor 2 of a, for less than half of a. The frame a is one camera-law frame of n samples. The
// tower of the engine lifts it, and floor 2 is the approximation after two levels: the `16^3` corner, `m = n / 64`
// values. The sim reads it from the engine by a lowering of that corner as its own tower. It checks it against the own
// two-level lift of the host.
//
// The sim arranges the values of floor 2 once as 16 bit planes. A query x then reads no value at all. The positions
// that hold x are the and, over the planes, of each plane where the x bit is 1, and its complement where it is 0. That
// is 32 positions for each word, and the read of a word stops after its mask is empty.
//
// The sim checks every match set against the scan of floor 2 by the host, for values on floor 2 and values not on it.
// It counts the reads of every query against `n / 2`. It builds the index once: the lift reads the n samples of a, and
// the planes read the m values of floor 2. Every query after reads only plane words.
#include "sim_camera.h"

#include "../../../../../cu/engine/analysis/tower/tower.h"

#define MATCH_KEY 0x464C4F4F52ULL

#define MATCH_FOUNDER_PURPOSE 0x464F554EULL

#define MATCH_PRESENT_PURPOSE 0x50524553ULL

#define MATCH_ABSENT_PURPOSE 0x41425345ULL

#define MATCH_SIDE 64ULL

#define MATCH_FLOOR 2u

// floor 2's side: two halvings of every axis
#define MATCH_FLOOR_SIDE (MATCH_SIDE >> MATCH_FLOOR)

#define MATCH_SAMPLES (MATCH_SIDE * MATCH_SIDE * MATCH_SIDE)

#define MATCH_FLOOR_VALUES (MATCH_FLOOR_SIDE * MATCH_FLOOR_SIDE * MATCH_FLOOR_SIDE)

// a floor-2 value's width: the engine's lowering confines every value to the 16-bit lane
#define MATCH_BITS 16u

#define MATCH_WORD_BITS 32ULL

#define MATCH_WORDS (MATCH_FLOOR_VALUES / MATCH_WORD_BITS)

#define MATCH_VALUES_MAX 65536ULL

#define MATCH_FOUNDERS 6u

// queries of each type: values on floor 2, drawn at keyed positions, and values not on it
#define MATCH_QUERIES_EACH 1024u

#define MATCH_QUERIES (2u * MATCH_QUERIES_EACH)

#define MATCH_THREADS 256ULL

static_assert((MATCH_FLOOR_VALUES % MATCH_WORD_BITS) == 0ULL, "floor 2 fills whole plane words, one warp a word");

static_assert((MATCH_FLOOR_VALUES % MATCH_THREADS) == 0ULL, "the plane kernel's blocks are whole warps");

// what the sim puts on the device at once: the frame, the two kept coefficient arrays of the tower, floor 2, and the
// planes. Then the queries, and the match word and reads of each query for every plane word
#define MATCH_DECLARED                                                                                                 \
    ((MATCH_SAMPLES * sizeof(unsigned short)) + (2ULL * MATCH_SAMPLES * sizeof(int)) +                                 \
     (MATCH_FLOOR_VALUES * sizeof(unsigned short)) + (MATCH_BITS * MATCH_WORDS * sizeof(unsigned int)) +               \
     (MATCH_QUERIES * sizeof(unsigned int)) +                                                                          \
     ((unsigned long long)MATCH_QUERIES * MATCH_WORDS * (sizeof(unsigned int) + sizeof(unsigned char))))

void match_floor_host(const unsigned short *lanes, long long *work, long long *before, long long *floor_values);

int match_floor_engine(SimResults *results, const unsigned short *device_lanes, int *crystal, int *corner,
                       unsigned short *floor_values);

__global__ void match_planes_kernel(const unsigned short *floor_values, unsigned int *planes);

__global__ void match_query_kernel(const unsigned int *planes, const unsigned int *queries, unsigned int *matches,
                                   unsigned char *reads);

typedef struct
{
    unsigned long long total;
    unsigned long long max;
    unsigned long long found;
    unsigned long long found_max;
    unsigned long long matched;
} MatchSide;

void match_print_side(VerbaLinea *line, const char *name, const MatchSide *side);

#endif
