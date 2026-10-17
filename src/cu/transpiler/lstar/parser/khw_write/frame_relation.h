// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_relation.h: the frame relations, what the host computes for a case from the launch and the case alone, for the
// teacher's frame round and the check that holds them (The frame forms asked, Open 6.4)
#ifndef FRAME_RELATION_H
#define FRAME_RELATION_H

#include "../../protocol/teacher/run_channel.h"

#ifdef __cplusplus
extern "C"
{
#endif

// the frame relations and their names, in the order the candidates are read: the first survivor is the form's
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

// the name a frame relation is written under in the answers and the working set, beside the ladder's
#define FRAME_READS "frame."

// the thread a frame form's fields are read at, whose lane and block differ from each other and from 0, and the cases
// a frame question carries: two blocks and the lanes past them, the second block reached, each case a thread
#define FRAME_READ_AT (RUN_THREADS_A_BLOCK + 2u)
#define FRAME_CASES ((2u * RUN_THREADS_A_BLOCK) + 3u)

// the threads a frame question's launch gives a case each, the count among its parameters: blocks of the carrier's
// size, as many as give every case a thread
#define FRAME_LAUNCHED (RUN_THREADS_A_BLOCK * ((FRAME_CASES + RUN_THREADS_A_BLOCK - 1u) / RUN_THREADS_A_BLOCK))

// the place of a tuple a relation does not hold
#define FRAME_PLACE_NONE 0xffffffffu

    // The host's answer of `relation` for the thread `at`, put with the case's two words `word` under the relation's
    // reading `reading`: the word a load reads, 0 or 1; the word of the register a store's variant set, 1 for the
    // second; a number's constant. The answer is the record's two words as one value, the first word low: a lane's
    // number, a block's, a word loaded or a number stand in the first with the second 0, as the stores before the
    // slot leave it; a store leaves the case's second word in the first, as the slot found it, and the register's
    // word in the second; an exit leaves the fill word in the first; the block size, `dim`, is the threads a block
    // on every case, which the launch alone sets; and a parameter's word, `parameter`, is the count the launch hands
    // the kernel on every case, its reading the count's place among the parameters. 1, or 0 where the relation is
    // none of the eight
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
            *answered = (reading == 0ull) ? word[0] : word[1];
            return 1;
        case FRAME_STORE:
            *answered = (unsigned long long)word[1] | ((unsigned long long)word[(reading == 0ull) ? 0u : 1u] << 32u);
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

    // each relation's tuple places: the destination, last where the relation writes a register; the address a load
    // reads through; the number, a number's value, a load's or a store's offset, or a parameter's place among the
    // parameters; the register a store keeps; and the guard an exit reads. FRAME_PLACE_NONE where the relation holds
    // none
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
