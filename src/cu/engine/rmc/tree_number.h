// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tree_number.h: a tree of precepts as one number
#ifndef TREE_NUMBER_H
#define TREE_NUMBER_H

#include "precepts.h"

// A tree over the alphabet is a number, and needs no table. Most of the code is unfurls and binary trees in the
// compiler, and math holds that with ease.
//
// The alphabet gives every symbol a fixed arity. A tree written in prefix order with fixed arities needs no parentheses
// and no child pointers. Read left to right, each symbol takes exactly as many subtrees as its arity, and where the
// next subtree ends is never in doubt. A tree is then a string over the alphabet. A string over an alphabet of
// TREE_SYMBOLS symbols is a number in base TREE_SYMBOLS.
//
// The alphabet is the PRECEPT_COUNT precepts and the leaves that a word can name. Those are its operands, and the word
// of zeroes and the word of ones that a rewrite needs and no operand carries. 24 symbols and 13 places is `24^13`, or
// 8.76e17, which sits inside an unsigned long long. Every tree in either web is one 8-byte number.
//
// This buys no bytes. The check web_check measures both, and the node lists win at this size. 21 trees hold 32 nodes
// between them, three bytes each, against eight bytes for each tree for the numbers. Most words are a single node, and
// a tree must reach five before a number pays for itself.
//
// It buys one form for each tree. A node list carries an order that the tree does not have. The same tree written by
// two passes numbers its nodes differently, and the two lists differ where the trees do not. As a number, one tree is
// one value. Two subtrees are the same subtree where their numbers are equal, a compare in place of a walk. A .kdm
// keyed on subtrees rests on that, as does the read of the compiler of its own trees.
//
// Past 13 places, the number outgrows an unsigned long long, and the container changes where the encoding does not. The
// engine already supports an exact integer of any width: OriorExactInteger in
// includes/exact_types/integers/exact_integer_api.h. Its limbs go least significant first, with the sign stored apart.
// The program writes the number of a tree in base TREE_SYMBOLS into one in the same way that it writes it into a word
// here.
//
// Nothing reaches for it yet, and this is the size where that stops. A fixed-arity tree never passes 5 nodes, and an
// unfurled one passes 13 at once. A ripple adder is a rank of gates for each bit. That puts a 32-bit add over a hundred
// nodes, and a 64-bit multiply in the thousands. The node that counts the width has no design yet. When it has one,
// its numbers go in the exact integer.
#define TREE_LEAVES 6u
#define TREE_SYMBOLS ((unsigned int)PRECEPT_COUNT + TREE_LEAVES)
#define TREE_PLACES 13u

// the leaves in the one numbering the tree's digits use: the four operands a word may read, then zero and ones. A
// digit below PRECEPT_COUNT is a precept, and one at or above it is a leaf
#define TREE_LEAF_AT(place_) ((unsigned int)PRECEPT_COUNT + (place_))
#define TREE_LEAF_ZERO ((unsigned int)PRECEPT_COUNT + 4u)
#define TREE_LEAF_ONES ((unsigned int)PRECEPT_COUNT + 5u)

// one symbol of a tree's prefix reading, from its number: place 0 is the root
static unsigned int tree_symbol(unsigned long long number, unsigned int place)
{
    for (unsigned int step = 0u; step < place; step += 1u)
    {
        number /= TREE_SYMBOLS;
    }
    return (unsigned int)(number % TREE_SYMBOLS);
}

// a symbol written into a tree's number at `place`, the rest of it left alone
static unsigned long long tree_written(unsigned long long number, unsigned int place, unsigned int symbol)
{
    unsigned long long weight = 1ULL;
    for (unsigned int step = 0u; step < place; step += 1u)
    {
        weight *= TREE_SYMBOLS;
    }
    return number + ((unsigned long long)symbol * weight);
}

#endif
