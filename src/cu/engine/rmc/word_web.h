// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// word_web.h: the compiler's operations as trees over the alphabet
#ifndef WORD_WEB_H
#define WORD_WEB_H

#include "precepts.h"

#include <stddef.h>

// The word web, the layer above precepts.h. The language's alphabet web comes first, then the word web, and then the
// coherence the clock measures.
//
// A word is an operation that the compiler decides in, written as the tree that it is over the alphabet. The schema
// names them, and a ruleset gives each a line of the text of the target. Neither one tells what the operation *is*.
// Then nothing can find whether the single instruction of a target for a word is worth it. Nothing can write a word
// out for a target that does not have it. The tree tells both:
//
//  - A target with no instruction for a word gets the tree of the word, each node a precept that it has. The alphabet
//    web also writes out any precept that it does not have. Nobody must write a thing by hand for a target twice.
//  - The program weighs a word that the target has an instruction for against its own tree, in the clock of the part.
//    Where the instruction beats the tree, it is a word of the language of that target. Where it does not, the target
//    writes that word out node by node, and the .kdm records which. That reading is the coherence, and the part gives
//    it.
//
// The leaves of a word are its operands by place, PRECEPT_ARG_AT(0) first. They are in the order that the form of the
// schema takes them after the destination. The form of word_bitand is `word_bitand to left right`, which puts left at
// place 0 and right at place 1. The destination is where the root lands, and is never a leaf.
typedef struct
{
    const char *name;
    unsigned char reads;
    unsigned char nodes;
    PreceptNode node[12];
} Word;

// Every word here is a tree of fixed arity: its operands are all that a read of it takes. That is a hard line, and
// most of the schema falls on the other side of it. The list below names the absences, so that nobody must find them.
//
// Not here, and not an omission:
//
//  - The declarations and the notes (declare_file, program_note, step_note, the opening and close of the lane). These
//    name no operation and have no tree. They are the kernel that a lane goes into.
//  - The words over memory and over the launch (global_load_constant_word, record_store_word, launch_load_wide,
//    global_add_atomic_word). A load is not a gate, and reduces to nothing in the alphabet: a target reaches memory
//    or does not.
//  - Every word whose tree counts the width of the register. This is the larger half, and the absence of one node
//    causes all of it. An adder is a rank of gates for each bit. A rotate written over the shifts is a shift left by
//    the amount, ORed with a shift right by the width less the amount. ROL and ROR alone are one node each, and are
//    here.
//
//    A sign spread is a carry right by the width less one. A test against zero is the word ORed down to a single bit,
//    with as many ranks as the width has powers of two. Each of those is a real tree, and none of them has a fixed
//    arity, because the width sets the count of nodes. A node that counts the width is a second type of node, and its
//    design is still open.
//
//    A fixed tree for any of these in the meantime would put a wrong tree in the web. A wrong tree is worse than no
//    tree. ADD, SUB and MUL are precepts for this same reason: they are where the trees of fixed arity stop, and each
//    is one node here.
//
// This table is the floor of the word web and not all of it. What it does contain, it contains exactly.
static const Word s_word_web[] = {
    // the gates, each its own precept and one node
    {"word_bitand", 2u, 1u, {{PRECEPT_BITAND, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"word_bitor", 2u, 1u, {{PRECEPT_BITOR, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"word_bitxor", 2u, 1u, {{PRECEPT_BITXOR, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"word_not", 1u, 1u, {{PRECEPT_NOT, PRECEPT_ARG_AT(0u), PRECEPT_NONE}}},
    {"word_nand", 2u, 1u, {{PRECEPT_NAND, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"word_nor", 2u, 1u, {{PRECEPT_NOR, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    // a copy
    {"word_copy", 1u, 1u, {{PRECEPT_MOV, PRECEPT_ARG_AT(0u), PRECEPT_NONE}}},
    // the shifts and the rotates, whose amount is an operand and needs no width. The signed shift carries the sign in,
    // and a rotate carries the bits that leave back in at the other end
    {"word_shl", 2u, 1u, {{PRECEPT_SHL, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"word_shr", 2u, 1u, {{PRECEPT_SHR, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"signed_word_shr", 2u, 1u, {{PRECEPT_ASR, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"word_rol", 2u, 1u, {{PRECEPT_ROL, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"word_ror", 2u, 1u, {{PRECEPT_ROR, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    // one limb of arithmetic, the precept itself with nothing around it. The carry chains running these over limbs are
    // width counted and are not here
    {"word_add", 2u, 1u, {{PRECEPT_ADD, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"word_sub", 2u, 1u, {{PRECEPT_SUB, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"word_mul", 2u, 1u, {{PRECEPT_MUL, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    // gates over words that carry one bit, which need no width because there is only the one bit
    {"predicate_bitxor", 2u, 1u, {{PRECEPT_BITXOR, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"predicate_bitand", 2u, 1u, {{PRECEPT_BITAND, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    // the branches of the lane. Every other branch that the schema has is one of these two, with a reason written
    // around it. None is a word of its own. Examples are the error, the top of a loop, and the next state
    {"loop_back_if", 2u, 1u, {{PRECEPT_JCC, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
    {"error_if", 2u, 1u, {{PRECEPT_JCC, PRECEPT_ARG_AT(0u), PRECEPT_ARG_AT(1u)}}},
};

#define WORD_WEB_COUNT (sizeof(s_word_web) / sizeof(s_word_web[0]))

// the word of the web whose tree is the precept `precept` alone over its operands in order, or NULL where none is
static inline const Word *word_web_word_for(unsigned int precept)
{
    for (unsigned int at = 0u; at < WORD_WEB_COUNT; at += 1u)
    {
        const Word *const word = &s_word_web[at];
        if ((word->nodes == 1u) && (word->node[0].precept == precept) && (word->node[0].left == PRECEPT_ARG_AT(0u)))
        {
            return word;
        }
    }
    return NULL;
}

#endif
