// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the goodstein_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef GOODSTEIN_INTERNAL_H
#define GOODSTEIN_INTERNAL_H

// Goodstein's sequences, exact and never expanded. The sources are Goodstein, "On the restricted ordinal theorem",
// Journal of Symbolic Logic 9, 1944. Then Kirby and Paris, "Accessible independence results for Peano arithmetic",
// Bulletin of the London Mathematical Society 14, 1982.
//
// The sim writes n in hereditary base b, and every exponent in base b again. A step writes every b as b + 1 and
// subtracts one. With b read as omega, the same tree is an ordinal below epsilon_0 in
// Cantor normal form. The bump leaves the tree as it is, and the subtraction lowers it. Every sequence reaches 0.
//
// The start 2^^k is omega^^k, and 16 is omega^omega^omega. The values grow as towers, and the sim never forms them. A
// form is the tree itself, with the base a separate number. The sim holds b^L - 1 at a large L as one run of
// coefficient b - 1. The run goes over the exponents 0 to L - 1 of the base that made the run.
//
// 1. The bump preserves the tree. Take each n below GOODSTEIN_LEMMA_BELOW at bases 2 to 6. Its numeric bump, in base
//    b + 1, is the tree of n in base b.
// 2. 2^^k in hereditary base 2 is omega^^k for k = 1 to 4, and the towers climb.
// 3. The starts 1, 2 and 3 reach 0 in 1, 3 and 5 steps, every value the numeric sequence's.
// 4. The starts 4, 16 and 65536 run GOODSTEIN_STEPS steps. Every ordinal is strictly below the one before.
//    Every value equals the numeric sequence's wherever that fits 64 bits.

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "sim.h"

#define GOODSTEIN_STEPS 1000000ULL

#define GOODSTEIN_LEMMA_BELOW 20000ULL

#define GOODSTEIN_LEMMA_BASE_MAX 6ULL

#define GOODSTEIN_TOWER 4u

#define GOODSTEIN_SMALL_STEPS_MAX 64ULL

// the max term comparisons one ordinal comparison may make before the sim reports it unfinished
#define GOODSTEIN_COMPARE_TERMS 1000000ULL

// the max characters the sim prints an ordinal with
#define GOODSTEIN_PRINT_CAPACITY 360u

// The blocks of one form at max, every exponent's blocks among them. A 64-bit value in hereditary base 2 holds a few
// hundred. A form past the bound ends the run with the reason
#define GOODSTEIN_BLOCKS_MAX 1024u

// the forms that the calls build and give back before they return, at max at once
#define GOODSTEIN_STACK_MAX 64u

// One term c b^E, of origin 0. Or a run, c b^tree(k) for every integer k from high down to low. The tree of k is k
// written hereditarily in the base that made the run (origin). The blocks of its exponent E follow it in the form,
// `exponent_blocks` of them, and a run's exponent holds none
struct GoodsteinBlock
{
    unsigned long long coefficient;
    unsigned long long origin;
    unsigned long long low;
    unsigned long long high;
    unsigned int exponent_blocks;
};

// A hereditary form, its terms highest first, each followed by the blocks of its exponent: `count` blocks inline at a
// bound. The empty form is 0
struct GoodsteinNotation
{
    GoodsteinBlock blocks[GOODSTEIN_BLOCKS_MAX];
    unsigned int count;
};

// a form read in place: `count` blocks from `blocks`, the whole of a notation or the exponent of one of its terms
typedef struct
{
    const GoodsteinBlock *blocks;
    unsigned int count;
} GoodsteinForm;

typedef struct
{
    unsigned long long terms;
    int exhausted;
} GoodsteinBudget;

// a place among the terms of a form: the block of the term and, in a run, the k of the term
typedef struct
{
    GoodsteinForm form;
    unsigned int block;
    unsigned long long high;
} GoodsteinCursor;

GoodsteinForm goodstein_form(const GoodsteinNotation *notation);

GoodsteinForm goodstein_exponent(GoodsteinForm form, unsigned int block);

unsigned int goodstein_terms(GoodsteinForm form);

void goodstein_copied(GoodsteinNotation *to, const GoodsteinNotation *from);

GoodsteinNotation *goodstein_taken(void);

void goodstein_given(void);

void goodstein_of(unsigned long long value, unsigned long long base, GoodsteinNotation *out);

void goodstein_tower(unsigned int height, GoodsteinNotation *out);

int goodstein_compare(GoodsteinForm left, GoodsteinForm right, GoodsteinBudget *budget);

int goodstein_power(unsigned long long base, unsigned long long exponent, unsigned long long *out);

int goodstein_add_term(unsigned long long *total, unsigned long long coefficient, unsigned long long base,
                       unsigned long long exponent);

int goodstein_value(GoodsteinForm form, unsigned long long base, unsigned long long *out);

int goodstein_decrement(GoodsteinNotation *form, unsigned long long base);

#endif
