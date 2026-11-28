// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_relation.h: the frame relations, what the host computes for a case from the launch and the case alone, for the
// teacher's frame round and the check that checks them (Open 6.4)
#ifndef FRAME_RELATION_H
#define FRAME_RELATION_H

#include "../teacher/run_channel.h"

#ifdef __cplusplus
extern "C"
{
#endif

// the frame relations and their names, in the order that the program reads the candidates. The first survivor is
// that of the form
#define FRAME_RELATIONS(relation_)                                                                                     \
    relation_(LANE, "lane") relation_(BLOCK, "block") relation_(LOAD, "load") relation_(STORE, "store")                \
        relation_(EXIT, "exit") relation_(NUMBER, "number") relation_(DIM, "dim") relation_(PARAMETER, "parameter")

#define FRAME_NAMED(name_, text_) FRAME_##name_,

    typedef enum
    {
        FRAME_RELATIONS(FRAME_NAMED) FRAME_RELATION_COUNT
    } FrameRelation;

#undef FRAME_NAMED

#define FRAME_TEXT(name_, text_) text_,

    static const char *const s_frame_text[] = {FRAME_RELATIONS(FRAME_TEXT)};

#undef FRAME_TEXT

// the name that a frame relation has in the answers and the working set, beside those of the ladder
#define FRAME_READS "frame."

// the thread that the program reads the fields of a frame form at, whose lane and block differ from each other and from
// 0. Then the cases that a frame question carries: two blocks and the lanes past them, with the second block reached.
// Each case has one thread
#define FRAME_READ_AT (RUN_THREADS_A_BLOCK + 2u)
#define FRAME_CASES ((2u * RUN_THREADS_A_BLOCK) + 3u)

// the threads that the launch of a frame question gives one case each, the count among its parameters. They are
// blocks of the size of the carrier, as many as give every case a thread
#define FRAME_LAUNCHED (RUN_THREADS_A_BLOCK * ((FRAME_CASES + RUN_THREADS_A_BLOCK - 1u) / RUN_THREADS_A_BLOCK))

// the place of a tuple a relation does not have
#define FRAME_PLACE_NONE 0xffffffffu

    // The answer of the host for `relation` at the thread `at`, put with the two words `word` of the case, under the
    // reading `reading` of the relation. The reading is the word that a load reads, 0 or 1. It is the word of the
    // register that the variant of a store set, 1 for the second, or the constant of a number.
    //
    // The answer is the two words of the record as one value, with the first word low:
    //
    // - the number of a lane or of a block, a word loaded or a number is in the first, with the second 0. That is as
    //   the stores before the probe place leave it
    // - a store leaves the second word of the case in the first, as the probe place found it. It leaves the word of the
    //   register in the second
    // - an exit leaves the fill word in the first
    // - the block size, `dim`, is the threads in a block on every case, which the launch alone sets
    // - the word of a parameter, `parameter`, is the count that the launch passes the kernel on every case. Its
    //   reading is the place of the count among the parameters.
    //
    // The return is 1, or 0 where the relation is none of the eight
    static inline int frame_answer(unsigned int relation, unsigned int at, const unsigned int *word,
                                   unsigned long long reading, unsigned long long *answered)
    {
        switch (relation)
        {
        case FRAME_LANE:
            *answered = at % RUN_THREADS_A_BLOCK;
            return 1;
        case FRAME_BLOCK:
            *answered = at / RUN_THREADS_A_BLOCK;
            return 1;
        case FRAME_LOAD:
            *answered = (reading == 0ULL) ? word[0] : word[1];
            return 1;
        case FRAME_STORE:
            *answered = (unsigned long long)word[1] | ((unsigned long long)word[(reading == 0ULL) ? 0u : 1u] << 32u);
            return 1;
        case FRAME_EXIT:
            *answered = RUN_UNWRITTEN_WORD;
            return 1;
        case FRAME_NUMBER:
            *answered = reading;
            return 1;
        case FRAME_DIM:
            *answered = RUN_THREADS_A_BLOCK;
            return 1;
        case FRAME_PARAMETER:
            *answered = FRAME_LAUNCHED;
            return 1;
        default:
            return 0;
        }
    }

    // the tuple places of each relation:
    //
    // - the destination, last where the relation writes a register
    // - the address that a load reads through
    // - the number: the value of a number, the offset of a load or a store, or the place of a parameter among the
    //   parameters
    // - the register that a store stores
    // - the guard that an exit reads
    //
    // FRAME_PLACE_NONE where the relation has none
    static inline unsigned int frame_place_destination(unsigned int relation)
    {
        return ((relation == FRAME_LANE) || (relation == FRAME_BLOCK) || (relation == FRAME_DIM)) ? 0u
               : ((relation == FRAME_NUMBER) || (relation == FRAME_PARAMETER))                    ? 1u
               : (relation == FRAME_LOAD)                                                         ? 2u
                                                                                                  : FRAME_PLACE_NONE;
    }

    static inline unsigned int frame_place_address(unsigned int relation)
    {
        return (relation == FRAME_LOAD) ? 0u : FRAME_PLACE_NONE;
    }

    static inline unsigned int frame_place_number(unsigned int relation)
    {
        return ((relation == FRAME_NUMBER) || (relation == FRAME_STORE) || (relation == FRAME_PARAMETER)) ? 0u
               : (relation == FRAME_LOAD)                                                                 ? 1u
                                                                                                          : FRAME_PLACE_NONE;
    }

    static inline unsigned int frame_place_register(unsigned int relation)
    {
        return (relation == FRAME_STORE) ? 1u : FRAME_PLACE_NONE;
    }

    static inline unsigned int frame_place_guard(unsigned int relation)
    {
        return (relation == FRAME_EXIT) ? 0u : FRAME_PLACE_NONE;
    }

#ifdef __cplusplus
}
#endif

#endif
