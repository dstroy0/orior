// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// precepts.h: the alphabet every operation is defined in
#ifndef PRECEPTS_H
#define PRECEPTS_H

// The alphabet. A store of operations as a binary tree of primitives makes a language as light as it can be. The
// section The precepts of src/engine_plan.md quotes the floor of universal operations whole, and what every math
// function comes from. This file is that floor, written down.
//
// Every operation that the compiler decides is a binary tree. Its leaves are operands, and its nodes are the precepts
// below. Nothing here comes apart further: each is a gate, a wire, an arithmetic node or a branch. A target that does
// not have one of these cannot take compiled code at all. Each thing above this file, such as a divide, a comparison or
// a carry chain, is a tree over these and carries no new primitive.
//
// The ruleset of a target tells which subtrees it has a single instruction for, and the clock shows which of those are
// worth it. The alphabet itself belongs to us and to no target. It is the irreducible set, and the program never asks a
// part whether it agrees with it.
//
// The arity is how many children a node takes. No precept takes three, and the tree is binary for that reason.
#define PRECEPTS(precept_)                                                                                             \
    /* the two that the metalanguage needs before any gate: nothing to do, and a thing that it cannot do */            \
    precept_(NOP, "nop", 0u) precept_(                                                                                 \
        ERR, "err",                                                                                                    \
        0u) /* the gates. NAND and NOR are each a tree over NOT and BITAND or BITOR. They are precepts all the same,   \
               because a part that builds from them builds all from them, and two nodes each would hide that */        \
        precept_(NOT, "not", 1u) precept_(BITAND, "bitand", 2u) precept_(BITOR, "bitor", 2u)                           \
            precept_(BITXOR, "bitxor", 2u) precept_(NAND, "nand", 2u) precept_(                                        \
                NOR, "nor", 2u) /* the wires: a copy, and the four ways to move a word along itself. ASR carries the   \
                                   sign in, and ROL and ROR carry the bit that left back in at the other end */        \
        precept_(MOV, "mov", 1u) precept_(SHL, "shl", 2u) precept_(SHR, "shr", 2u) precept_(ASR, "asr", 2u)            \
            precept_(ROL, "rol", 2u) precept_(                                                                         \
                ROR, "ror",                                                                                            \
                2u) /* the arithmetic nodes. A subtract is an add of the negation. It is a precept all the same,       \
                       because a compare is a subtract that discards its difference, and every part has one. A         \
                       multiply is an adder for each bit again, a tree whose count of nodes is the width of the        \
                       register. It is a precept because every part has one, and a rewrite over gates counts the       \
                       width */                                                                                        \
        precept_(ADD, "add", 2u) precept_(SUB, "sub", 2u) precept_(                                                    \
            MUL, "mul", 2u) /* the two branches, and no others. Each reason for a branch in a lane is a tree over      \
                               these two, and not a precept of its own. An error, the top of a loop and the            \
                               next state are such reasons */                                                          \
        precept_(BRA, "bra", 1u) precept_(JCC, "jcc", 2u)

#define PRECEPT_NAMED(name_, text_, arity_) PRECEPT_##name_,

enum Precept
{
    PRECEPTS(PRECEPT_NAMED) PRECEPT_COUNT
};

// A child of a node is another node of the same tree, by its number, or a leaf. The nodes of a tree have numbers from
// 0, and its root is its last node. A child below PRECEPT_ARG reads as a node, and one at or above it as a leaf. The
// leaves are the operands that the tree got, by their place. They are also the two words that a rewrite needs and that
// no operand carries: a word of zeroes and a word of ones
#define PRECEPT_ARG 0xf0u
#define PRECEPT_ARG_AT(place_) ((unsigned char)(PRECEPT_ARG + (place_)))
#define PRECEPT_ZERO 0xfeu
#define PRECEPT_ONES 0xffu
// a node whose precept takes one child leaves the other here
#define PRECEPT_NONE 0xfdu
// the first two operands. That is the own arity of every precept, and how the alphabet web below reads
#define PRECEPT_LEFT PRECEPT_ARG_AT(0u)
#define PRECEPT_RIGHT PRECEPT_ARG_AT(1u)

typedef struct
{
    unsigned char precept;
    unsigned char left;
    unsigned char right;
} PreceptNode;

// the max nodes a tree contains, the Word's own count (word_web.h)
#define PRECEPT_NODES 12u

// One precept written as a tree over the others: what a target that does not have it gets instead. These rewrites are
// the difference between a web and a list. Every gate reaches every other gate. A part that answers yes to NAND alone
// can still take compiled code. A part that answers yes to XOR saves the three nodes that XOR costs from NAND.
//
// The compiler walks the web, and never assumes it. Which of these a target has is a question that the probes put to
// it. The compiler takes a rewrite only where the answer was no. The rewrites run both ways by design: NOT comes from
// NAND, and NAND from NOT. Thus what the target answered bounds the walk, and never the order here.
typedef struct
{
    unsigned char precept;
    unsigned char nodes;
    PreceptNode node[8];
} PreceptRewrite;

// What this does not yet hold is the edge of the design, not an omission: ADD, SUB and MUL over gates. An adder is a
// rank of gates for each bit, and a multiply is an adder for each bit again. Their size is the width of the register,
// where a rewrite below has a fixed count of nodes. A rewrite that counts the width is a second type of node, and its
// design is still open. Every rewrite below has a fixed arity and reads without a width
static const PreceptRewrite s_precept_web[] = {
    // NOT a = a NAND a
    {PRECEPT_NOT, 1u, {{PRECEPT_NAND, PRECEPT_LEFT, PRECEPT_LEFT}}},
    // a NAND b = NOT (a AND b)
    {PRECEPT_NAND, 2u, {{PRECEPT_BITAND, PRECEPT_LEFT, PRECEPT_RIGHT}, {PRECEPT_NOT, 0u, PRECEPT_NONE}}},
    // a NOR b = NOT (a OR b)
    {PRECEPT_NOR, 2u, {{PRECEPT_BITOR, PRECEPT_LEFT, PRECEPT_RIGHT}, {PRECEPT_NOT, 0u, PRECEPT_NONE}}},
    // a AND b = NOT (a NAND b)
    {PRECEPT_BITAND, 2u, {{PRECEPT_NAND, PRECEPT_LEFT, PRECEPT_RIGHT}, {PRECEPT_NOT, 0u, PRECEPT_NONE}}},
    // a OR b = (NOT a) NAND (NOT b)
    {PRECEPT_BITOR,
     3u,
     {{PRECEPT_NOT, PRECEPT_LEFT, PRECEPT_NONE}, {PRECEPT_NOT, PRECEPT_RIGHT, PRECEPT_NONE}, {PRECEPT_NAND, 0u, 1u}}},
    // a XOR b = (a AND NOT b) OR (NOT a AND b)
    {PRECEPT_BITXOR,
     5u,
     {{PRECEPT_NOT, PRECEPT_RIGHT, PRECEPT_NONE},
      {PRECEPT_BITAND, PRECEPT_LEFT, 0u},
      {PRECEPT_NOT, PRECEPT_LEFT, PRECEPT_NONE},
      {PRECEPT_BITAND, 2u, PRECEPT_RIGHT},
      {PRECEPT_BITOR, 1u, 3u}}},
    // MOV a = a OR a, the cheapest node that reads one word and writes it back unchanged
    {PRECEPT_MOV, 1u, {{PRECEPT_BITOR, PRECEPT_LEFT, PRECEPT_LEFT}}},
    // `a SUB b = NOT ((NOT a) ADD b)`. The plainer reading of a subtraction is a plus the complement of b plus one. The
    // one is a word that no leaf here carries. A rewrite has a word of zeroes and a word of ones. A single set bit from
    // either takes a shift by the width, the node that counts the width, and this file does not have it.
    //
    // A complement of the other side needs no constant at all. `~(~a + b)` is `-(~a + b) - 1`, with `~a` as `-a - 1`.
    // That leaves `a - b`, with the same three nodes on every width
    {PRECEPT_SUB,
     3u,
     {{PRECEPT_NOT, PRECEPT_LEFT, PRECEPT_NONE}, {PRECEPT_ADD, 0u, PRECEPT_RIGHT}, {PRECEPT_NOT, 1u, PRECEPT_NONE}}},
    // BRA to a label is a JCC whose condition never fails
    {PRECEPT_BRA, 1u, {{PRECEPT_JCC, PRECEPT_ONES, PRECEPT_LEFT}}},
};

#define PRECEPT_WEB_COUNT (sizeof(s_precept_web) / sizeof(s_precept_web[0]))

#endif
