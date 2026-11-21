// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef LADDER_H
#define LADDER_H

// What the compiler knows before it meets a system: relations.
//
// A relation is a tuple of words and the word that goes with them, as 1,1 -> 2. That relation is not an addition,
// because addition is a writing and this file contains none. Every system that computes agrees about 1,1 -> 2. The
// compiler can assume nothing else about a system that nobody met. Arithmetic is the shared ground, and the writing
// that reaches it is the part that differs. The work of the compiler is to find that writing.
//
// The names below are coherence anchors, and only labels. They let the compiler report the relations that it
// found a writing for, and compose them into larger ones. The compiler never gives a target a name, and never queries a
// target about a name.
//
// Two anchors are necessary. A system that supports SAME and ADD can get the rest from them. TAKE is ADD over a
// complement, PRODUCT is UP and ADD stacked, and the gates reach one another through rewrites. The compiler cannot
// address a system that supports neither.

// The anchors, the relations that fix each one, how many words a question of it carries, and whether the system
// alone fixes its answer. The cases show the relations, and no formula does. A target can answer a case, and it
// cannot answer a formula.
//
// The last column separates the two types of question on the ladder. A relation's answer follows from the words of
// its question, the same way on every system that has the relation. The compiler can find a chain of primitives for
// it, and check the chain before it queries a system. A measure's answer follows from the system: the count of places
// in a word is true of the part and of nothing else. No chain produces it, and only a query gets it. A chain builder
// that gets a measure finds arrangements that hit the one answer by accident.
#define LADDER_ANCHORS(anchor_)                                                                                        \
    /* a word against itself. Shows the channel carries an answer at all, and what this system answers with when */    \
    /* two things are the same. Required */                                                                            \
    anchor_(SAME, "same", 2u, 0)                                                                                       \
    /* how many places a word contains: the count of moves that a one makes up the word until it goes out */         \
    anchor_(PLACES, "places", 1u, 1)                                                                                   \
    /* 1,1 -> 2 and the rest of its cases. Required: nothing reaches it without a rank of gates for every place, */    \
    /* and the count of places is what PLACES measures */                                                              \
    anchor_(ADD, "add", 2u, 0)                                                                                         \
    /* 3,1 -> 2 */                                                                                                     \
    anchor_(TAKE, "take", 2u, 0)                                                                                       \
    /* 1,1 -> 2 as well, and 1,2 -> 4: a word moved toward the end it grows at */                                      \
    anchor_(UP, "up", 2u, 0)                                                                                           \
    /* 4,1 -> 2 */                                                                                                     \
    anchor_(DOWN, "down", 2u, 0)                                                                                       \
    /* 3,3 -> 9 */                                                                                                     \
    anchor_(PRODUCT, "product", 2u, 0)                                                                                 \
    /* the orderings and the inequality. Each is a predicate relation (ladder_predicate), with an answer of the form */ \
    /* that SAME gives. That answer is a word of ones where it is true, and of zeroes where it is false. 0,1 -> ones */ \
    /* for less. The part reads 0xffffffff,0 as it reads a word, unsigned or signed */                                 \
    anchor_(LESS, "less", 2u, 0)                                                                                       \
    anchor_(AT_MAX, "at_max", 2u, 0)                                                                                 \
    anchor_(MORE, "more", 2u, 0)                                                                                       \
    anchor_(AT_LEAST, "at_least", 2u, 0)                                                                               \
    anchor_(APART, "apart", 2u, 0)                                                                                     \
    /* 7,2 -> 3: the count of whole times the second word goes into the first. No answer for a second word of 0 */      \
    anchor_(QUOTIENT, "quotient", 2u, 0)                                                                               \
    /* 7,2 -> 1: what stays of the first word after the quotient. No answer for a second word of 0 */                \
    anchor_(REMAINDER, "remainder", 2u, 0)                                                                             \
    /* 12,10 -> 8: each place that holds a one in both words */                                                         \
    anchor_(BOTH, "both", 2u, 0)                                                                                       \
    /* 12,10 -> 14: each place that holds a one in either word */                                                       \
    anchor_(EITHER, "either", 2u, 0)                                                                                   \
    /* 12,10 -> 6: each place where the two words differ */                                                             \
    anchor_(DIFFER, "differ", 2u, 0)                                                                                   \
    /* the anchors of one word, which a kernel that loads two words does not ask. 1 -> 0xffffffff: the word whose */    \
    /* sum with the first word is 0 */                                                                      \
    anchor_(NEGATE, "negate", 1u, 0)                                                                                   \
    /* 5 -> 0xfffffffa: each place of the word changed */                                                               \
    anchor_(INVERT, "invert", 1u, 0)                                                                                   \
    /* 0 -> ones: a predicate, true where the word is 0, with an answer of the form that SAME gives */                                        \
    anchor_(ZERO, "zero", 1u, 0)                                                                                       \
    /* 5 -> 5: the word itself */                                                                                      \
    anchor_(COPY, "copy", 1u, 0)

#define LADDER_NAMED(name_, text_, words_, measured_) LADDER_##name_,

typedef enum
{
    LADDER_ANCHORS(LADDER_NAMED) LADDER_ANCHOR_COUNT
} LadderAnchor;

#undef LADDER_NAMED

#define LADDER_MEASURED(name_, text_, words_, measured_) measured_,

// whether each anchor's answer is a property of the system and not of the words of its question
static const int s_ladder_measured[] = {LADDER_ANCHORS(LADDER_MEASURED)};

#undef LADDER_MEASURED

#define LADDER_WORDS_OF(name_, text_, words_, measured_) words_,

// how many words a question of each anchor carries, and the place of the answer in its tuple
static const unsigned int s_ladder_words[] = {LADDER_ANCHORS(LADDER_WORDS_OF)};

#undef LADDER_WORDS_OF

// the max words a question carries
#define LADDER_WORDS 4u

// One question: its words, and the word that must come back. `anchor` is the compiler's own label for the relation,
// and nothing that a target reads contains it. `answered` gets what came back
typedef struct
{
    unsigned int anchor;
    unsigned int word[LADDER_WORDS];
    unsigned int words;
    unsigned int expected;
    unsigned int answered;
    int agreed;
} LadderQuestion;

// The width of a relation before the answer to PLACES comes back. The first questions go at this width, and the
// answer to PLACES sets the width of the rest
#define LADDER_PLACES_ASSUMED 32u

// The cases, in the order the ladder climbs. SAME is first: until something comes back, there is no channel and no
// reading of what this system reports for true. PLACES is next, and every case after it uses its count as the width.
// ADD follows, the other necessary anchor. Then the rest follow. Each comes from those two, and the ladder queries it
// all the same: a system that has it outright needs no chain.
//
// The protocol puts each case in its own right, and never in the order of this list. In this order, a part can
// answer the second case from the first. Such an answer shows nothing about whether the part supports the relation.
//
// This list writes true as a word of ones and false as a word of zeroes. That is the anchor of this compiler, and not
// a claim about a system. The ladder queries SAME because what comes back can be a one, a sign bit, or an unknown
// word. Whatever comes back is the true of that system from then on.
static const LadderQuestion s_ladder_cases[] = {
    {LADDER_SAME, {1u, 1u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_SAME, {0u, 0u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_SAME, {1u, 0u}, 2u, 0u, 0u, 0},
    {LADDER_SAME, {0u, 1u}, 2u, 0u, 0u, 0},
    {LADDER_SAME, {5u, 5u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_SAME, {5u, 7u}, 2u, 0u, 0u, 0},
    // two that differ at the end of the word where a system can read a sign. An arrangement that spreads one place
    // across the word answers them wrongly, and the cases above let it through
    {LADDER_SAME, {0x80000000u, 0u}, 2u, 0u, 0u, 0},
    {LADDER_SAME, {0xffffffffu, 0x7fffffffu}, 2u, 0u, 0u, 0},
    {LADDER_SAME, {0x80000000u, 0x80000000u}, 2u, 0xffffffffu, 0u, 0},
    // a one that moves until it goes out. The answer is the count of places, and no arrangement of gates gets it from
    // its word. Only a query measures it, and no computation gets it
    {LADDER_PLACES, {1u}, 1u, 32u, 0u, 0},
    {LADDER_ADD, {1u, 1u}, 2u, 2u, 0u, 0},
    {LADDER_ADD, {1u, 0u}, 2u, 1u, 0u, 0},
    {LADDER_ADD, {0u, 1u}, 2u, 1u, 0u, 0},
    {LADDER_ADD, {0u, 0u}, 2u, 0u, 0u, 0},
    {LADDER_ADD, {3u, 5u}, 2u, 8u, 0u, 0},
    {LADDER_ADD, {7u, 9u}, 2u, 16u, 0u, 0},
    // the two that report what this system does at the end of a word: one place past the largest, and the largest
    // added to itself
    {LADDER_ADD, {0xffffffffu, 1u}, 2u, 0u, 0u, 0},
    {LADDER_ADD, {0x7fffffffu, 1u}, 2u, 0x80000000u, 0u, 0},
    {LADDER_TAKE, {3u, 1u}, 2u, 2u, 0u, 0},
    {LADDER_TAKE, {1u, 1u}, 2u, 0u, 0u, 0},
    {LADDER_TAKE, {5u, 0u}, 2u, 5u, 0u, 0},
    {LADDER_TAKE, {9u, 7u}, 2u, 2u, 0u, 0},
    // one taken from nothing, which reports what this system does at the other end of a word
    {LADDER_TAKE, {0u, 1u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_TAKE, {0x80000000u, 1u}, 2u, 0x7fffffffu, 0u, 0},
    {LADDER_UP, {1u, 1u}, 2u, 2u, 0u, 0},
    {LADDER_UP, {1u, 2u}, 2u, 4u, 0u, 0},
    {LADDER_UP, {1u, 0u}, 2u, 1u, 0u, 0},
    {LADDER_UP, {3u, 4u}, 2u, 48u, 0u, 0},
    {LADDER_UP, {0x80000000u, 1u}, 2u, 0u, 0u, 0},
    {LADDER_DOWN, {4u, 1u}, 2u, 2u, 0u, 0},
    {LADDER_DOWN, {8u, 3u}, 2u, 1u, 0u, 0},
    {LADDER_DOWN, {5u, 0u}, 2u, 5u, 0u, 0},
    {LADDER_DOWN, {0xffffffffu, 4u}, 2u, 0x0fffffffu, 0u, 0},
    {LADDER_PRODUCT, {3u, 3u}, 2u, 9u, 0u, 0},
    {LADDER_PRODUCT, {0u, 5u}, 2u, 0u, 0u, 0},
    {LADDER_PRODUCT, {1u, 7u}, 2u, 7u, 0u, 0},
    {LADDER_PRODUCT, {6u, 7u}, 2u, 42u, 0u, 0},
    {LADDER_PRODUCT, {11u, 13u}, 2u, 143u, 0u, 0},
    // the two that report what this system does at the end of a word: a product one past the largest, and two times
    // the largest
    {LADDER_PRODUCT, {0x10000u, 0x10000u}, 2u, 0u, 0u, 0},
    {LADDER_PRODUCT, {0xffffffffu, 2u}, 2u, 0xfffffffeu, 0u, 0},
    // the orderings. Each is true and false on small words, and true or false on equal words. Each reads 0xffffffff
    // against 0 one way unsigned and the other way signed
    {LADDER_LESS, {0u, 1u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_LESS, {1u, 0u}, 2u, 0u, 0u, 0},
    {LADDER_LESS, {1u, 1u}, 2u, 0u, 0u, 0},
    {LADDER_LESS, {3u, 7u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_LESS, {0xffffffffu, 0u}, 2u, 0u, 0u, 0},
    {LADDER_AT_MAX, {1u, 1u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_AT_MAX, {0u, 1u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_AT_MAX, {2u, 1u}, 2u, 0u, 0u, 0},
    {LADDER_AT_MAX, {7u, 3u}, 2u, 0u, 0u, 0},
    {LADDER_AT_MAX, {0xffffffffu, 0u}, 2u, 0u, 0u, 0},
    {LADDER_MORE, {1u, 0u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_MORE, {0u, 1u}, 2u, 0u, 0u, 0},
    {LADDER_MORE, {1u, 1u}, 2u, 0u, 0u, 0},
    {LADDER_MORE, {7u, 3u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_MORE, {0xffffffffu, 0u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_AT_LEAST, {1u, 1u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_AT_LEAST, {1u, 0u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_AT_LEAST, {0u, 1u}, 2u, 0u, 0u, 0},
    {LADDER_AT_LEAST, {3u, 7u}, 2u, 0u, 0u, 0},
    {LADDER_AT_LEAST, {0xffffffffu, 0u}, 2u, 0xffffffffu, 0u, 0},
    // the inequality, SAME's complement
    {LADDER_APART, {1u, 0u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_APART, {1u, 1u}, 2u, 0u, 0u, 0},
    {LADDER_APART, {0u, 0u}, 2u, 0u, 0u, 0},
    {LADDER_APART, {5u, 7u}, 2u, 0xffffffffu, 0u, 0},
    {LADDER_APART, {0x80000000u, 0x80000000u}, 2u, 0u, 0u, 0},
    // the quotient and the remainder: small words, a word into a smaller one, and the high place
    {LADDER_QUOTIENT, {7u, 2u}, 2u, 3u, 0u, 0},
    {LADDER_QUOTIENT, {8u, 2u}, 2u, 4u, 0u, 0},
    {LADDER_QUOTIENT, {9u, 3u}, 2u, 3u, 0u, 0},
    {LADDER_QUOTIENT, {5u, 7u}, 2u, 0u, 0u, 0},
    {LADDER_QUOTIENT, {1u, 1u}, 2u, 1u, 0u, 0},
    {LADDER_QUOTIENT, {0xffffffffu, 2u}, 2u, 0x7fffffffu, 0u, 0},
    {LADDER_QUOTIENT, {0x80000000u, 0x80000000u}, 2u, 1u, 0u, 0},
    {LADDER_REMAINDER, {7u, 2u}, 2u, 1u, 0u, 0},
    {LADDER_REMAINDER, {9u, 3u}, 2u, 0u, 0u, 0},
    {LADDER_REMAINDER, {5u, 7u}, 2u, 5u, 0u, 0},
    {LADDER_REMAINDER, {10u, 4u}, 2u, 2u, 0u, 0},
    {LADDER_REMAINDER, {0xffffffffu, 2u}, 2u, 1u, 0u, 0},
    {LADDER_REMAINDER, {0x80000000u, 3u}, 2u, 2u, 0u, 0},
    // the three of the places: each place read in both words alike, the high place among them
    {LADDER_BOTH, {12u, 10u}, 2u, 8u, 0u, 0},
    {LADDER_BOTH, {0u, 7u}, 2u, 0u, 0u, 0},
    {LADDER_BOTH, {6u, 3u}, 2u, 2u, 0u, 0},
    {LADDER_BOTH, {0xffffffffu, 5u}, 2u, 5u, 0u, 0},
    {LADDER_BOTH, {0x80000000u, 0xffffffffu}, 2u, 0x80000000u, 0u, 0},
    {LADDER_EITHER, {12u, 10u}, 2u, 14u, 0u, 0},
    {LADDER_EITHER, {0u, 7u}, 2u, 7u, 0u, 0},
    {LADDER_EITHER, {6u, 3u}, 2u, 7u, 0u, 0},
    {LADDER_EITHER, {0u, 0u}, 2u, 0u, 0u, 0},
    {LADDER_EITHER, {0x80000000u, 1u}, 2u, 0x80000001u, 0u, 0},
    {LADDER_DIFFER, {12u, 10u}, 2u, 6u, 0u, 0},
    {LADDER_DIFFER, {5u, 5u}, 2u, 0u, 0u, 0},
    {LADDER_DIFFER, {6u, 3u}, 2u, 5u, 0u, 0},
    {LADDER_DIFFER, {0xffffffffu, 1u}, 2u, 0xfffffffeu, 0u, 0},
    {LADDER_DIFFER, {0u, 0x80000000u}, 2u, 0x80000000u, 0u, 0},
    // the anchors of one word
    {LADDER_NEGATE, {1u}, 1u, 0xffffffffu, 0u, 0},
    {LADDER_NEGATE, {0u}, 1u, 0u, 0u, 0},
    {LADDER_NEGATE, {5u}, 1u, 0xfffffffbu, 0u, 0},
    {LADDER_NEGATE, {0x80000000u}, 1u, 0x80000000u, 0u, 0},
    {LADDER_INVERT, {0u}, 1u, 0xffffffffu, 0u, 0},
    {LADDER_INVERT, {5u}, 1u, 0xfffffffau, 0u, 0},
    {LADDER_INVERT, {0xffffffffu}, 1u, 0u, 0u, 0},
    {LADDER_INVERT, {0x80000000u}, 1u, 0x7fffffffu, 0u, 0},
    {LADDER_ZERO, {0u}, 1u, 0xffffffffu, 0u, 0},
    {LADDER_ZERO, {1u}, 1u, 0u, 0u, 0},
    {LADDER_ZERO, {5u}, 1u, 0u, 0u, 0},
    {LADDER_ZERO, {0x80000000u}, 1u, 0u, 0u, 0},
    {LADDER_COPY, {5u}, 1u, 5u, 0u, 0},
    {LADDER_COPY, {0u}, 1u, 0u, 0u, 0},
    {LADDER_COPY, {0xffffffffu}, 1u, 0xffffffffu, 0u, 0},
    {LADDER_COPY, {0x80000000u}, 1u, 0x80000000u, 0u, 0},
};

#define LADDER_CASE_COUNT (sizeof(s_ladder_cases) / sizeof(s_ladder_cases[0]))

// What `anchor` answers for `words` of them, through `answered`. The result is 1, or 0 where there is no answer. A
// measure has no answer here, because its answer is the system's and follows from no words. A quotient and a
// remainder over 0 have no answer either.
//
// The cases above are the order the ladder climbs, and not the whole of a relation. A relation is true for every
// word, and this function states it for each of them. Thus a check of an arrangement can go past the few small words
// of the ladder. It is arithmetic, and it runs wherever the compiler runs. The compiler knows 1,1 -> 2 because the
// system under it computes, and every system that computes agrees. This function learns nothing about a target and
// queries no target.
//
// The count for UP and DOWN comes from the low places of the second word, the way precept_value.h reads one. Thus
// the check puts the same question to a chain and to the relation for it.
static inline int ladder_answer(unsigned int anchor, const unsigned int *word, unsigned int words,
                                unsigned int *answered)
{
    // the anchors of one word
    if (words >= 1u)
    {
        switch (anchor)
        {
        case LADDER_NEGATE:
            *answered = 0u - word[0];
            return 1;
        case LADDER_INVERT:
            *answered = ~word[0];
            return 1;
        case LADDER_ZERO:
            *answered = (word[0] == 0u) ? 0xffffffffu : 0u;
            return 1;
        case LADDER_COPY:
            *answered = word[0];
            return 1;
        default:
            break;
        }
    }
    if (words < 2u)
    {
        return 0;
    }
    const unsigned int count = word[1] & (LADDER_PLACES_ASSUMED - 1u);
    switch (anchor)
    {
    case LADDER_SAME:
        *answered = (word[0] == word[1]) ? 0xffffffffu : 0u;
        return 1;
    case LADDER_ADD:
        *answered = word[0] + word[1];
        return 1;
    case LADDER_TAKE:
        *answered = word[0] - word[1];
        return 1;
    case LADDER_UP:
        *answered = (count == 0u) ? word[0] : (word[0] << count);
        return 1;
    case LADDER_DOWN:
        *answered = (count == 0u) ? word[0] : (word[0] >> count);
        return 1;
    case LADDER_PRODUCT:
        *answered = word[0] * word[1];
        return 1;
    case LADDER_LESS:
        *answered = (word[0] < word[1]) ? 0xffffffffu : 0u;
        return 1;
    case LADDER_AT_MAX:
        *answered = (word[0] <= word[1]) ? 0xffffffffu : 0u;
        return 1;
    case LADDER_MORE:
        *answered = (word[0] > word[1]) ? 0xffffffffu : 0u;
        return 1;
    case LADDER_AT_LEAST:
        *answered = (word[0] >= word[1]) ? 0xffffffffu : 0u;
        return 1;
    case LADDER_APART:
        *answered = (word[0] != word[1]) ? 0xffffffffu : 0u;
        return 1;
    case LADDER_QUOTIENT:
        if (word[1] == 0u)
        {
            return 0;
        }
        *answered = word[0] / word[1];
        return 1;
    case LADDER_REMAINDER:
        if (word[1] == 0u)
        {
            return 0;
        }
        *answered = word[0] % word[1];
        return 1;
    case LADDER_BOTH:
        *answered = word[0] & word[1];
        return 1;
    case LADDER_EITHER:
        *answered = word[0] | word[1];
        return 1;
    case LADDER_DIFFER:
        *answered = word[0] ^ word[1];
        return 1;
    default:
        return 0;
    }
}

// 1 where `anchor` is a predicate relation: its answer is a truth and no word. These are SAME, the orderings, the
// inequality and ZERO, read off the part through a store guarded by the predicate the probe writes
static inline int ladder_predicate(unsigned int anchor)
{
    return (anchor == (unsigned int)LADDER_SAME) || (anchor == (unsigned int)LADDER_LESS) ||
           (anchor == (unsigned int)LADDER_AT_MAX) || (anchor == (unsigned int)LADDER_MORE) ||
           (anchor == (unsigned int)LADDER_AT_LEAST) || (anchor == (unsigned int)LADDER_APART) ||
           (anchor == (unsigned int)LADDER_ZERO);
}

// What the host answers for `anchor` on the two words `word` where it reads a word as signed. The two readings of a
// case differ where a word moves toward the low end. There the high place comes back as itself when signed, and as
// nothing when unsigned. They differ for the orderings, the quotient and the remainder too. Every other relation of
// the ladder gives the same answer both ways at one width
static inline unsigned int ladder_signed_answer(unsigned int anchor, const unsigned int *word)
{
    unsigned int answered = 0u;
    // the orderings read signed: the high place a sign, 0xffffffff below 0
    const int left = (int)word[0];
    const int right = (int)word[1];
    switch (anchor)
    {
    case LADDER_LESS:
        return (left < right) ? 0xffffffffu : 0u;
    case LADDER_AT_MAX:
        return (left <= right) ? 0xffffffffu : 0u;
    case LADDER_MORE:
        return (left > right) ? 0xffffffffu : 0u;
    case LADDER_AT_LEAST:
        return (left >= right) ? 0xffffffffu : 0u;
    case LADDER_QUOTIENT:
    case LADDER_REMAINDER:
        // a quotient read signed cuts toward 0. A divisor of 0 has no answer. The lowest word over all ones wraps
        // to the lowest word, with a remainder of 0
        if (right == 0)
        {
            return 0u;
        }
        if ((word[0] == 0x80000000u) && (word[1] == 0xffffffffu))
        {
            return (anchor == (unsigned int)LADDER_QUOTIENT) ? 0x80000000u : 0u;
        }
        return (unsigned int)((anchor == (unsigned int)LADDER_QUOTIENT) ? (left / right) : (left % right));
    default:
        break;
    }
    if (anchor != (unsigned int)LADDER_DOWN)
    {
        return ladder_answer(anchor, word, 2u, &answered) ? answered : 0u;
    }
    const unsigned int count = word[1] & (LADDER_PLACES_ASSUMED - 1u);
    if (count == 0u)
    {
        return word[0];
    }
    const unsigned int moved = word[0] >> count;
    const unsigned int filled = ((word[0] & 0x80000000u) != 0u) ? (0xffffffffu << (LADDER_PLACES_ASSUMED - count)) : 0u;
    return moved | filled;
}

// the words of a sweep, from a generator in this file. The same words come back on every run, and two runs of the
// builder give the same chains
static inline unsigned int ladder_swept(unsigned int *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

#endif
