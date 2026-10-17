// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_relation_check.c: the frame relations held to what the teacher reads of them (frame_relation.h)
//
//   names      each relation's name, in its order
//   answers    the host's answer of each relation for a thread, from the launch and the case alone
//   read at    the thread a frame form's fields are read at has a lane and a block that differ, both past 0
//   places     each relation's tuple places, the destination where the relation writes one
//   carrier    the words the channel names for the carrier's fill and its block are the carrier's own
//
//     frame_relation_check
#include "../../../../../../../src/cu/transpiler/lstar/parser/khw_write/frame_relation.h"
#include "../../../../../../../src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_run/cubin_run_internal.h"

#include <stdio.h>
#include <string.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

int main(void)
{
    check_that(
        (FRAME_RELATION_COUNT == 8u) && (strcmp(s_frame_text[FRAME_LANE], "lane") == 0) &&
            (strcmp(s_frame_text[FRAME_BLOCK], "block") == 0) && (strcmp(s_frame_text[FRAME_LOAD], "load") == 0) &&
            (strcmp(s_frame_text[FRAME_STORE], "store") == 0) && (strcmp(s_frame_text[FRAME_EXIT], "exit") == 0) &&
            (strcmp(s_frame_text[FRAME_NUMBER], "number") == 0) && (strcmp(s_frame_text[FRAME_DIM], "dim") == 0) &&
            (strcmp(s_frame_text[FRAME_PARAMETER], "parameter") == 0),
        "the eight relations are named in their order");
    const unsigned int word[2] = {3u, 5u};
    unsigned long long answered = 0ull;
    check_that(frame_answer(FRAME_LANE, 258u, word, 0ull, &answered) && (answered == 2ull),
               "the lane of thread 258 is 2 at a block of the carrier's size");
    check_that(frame_answer(FRAME_BLOCK, 258u, word, 0ull, &answered) && (answered == 1ull), "and its block 1");
    check_that(frame_answer(FRAME_LANE, 0u, word, 0ull, &answered) && (answered == 0ull) &&
                   frame_answer(FRAME_BLOCK, 255u, word, 0ull, &answered) && (answered == 0ull),
               "thread 0's lane and thread 255's block are 0");
    check_that(frame_answer(FRAME_LOAD, 7u, word, 0ull, &answered) && (answered == 3ull) &&
                   frame_answer(FRAME_LOAD, 7u, word, 1ull, &answered) && (answered == 5ull),
               "a load reads the case's first word or its second by its reading");
    check_that(frame_answer(FRAME_STORE, 7u, word, 1ull, &answered) && (answered == (5ull | (5ull << 32u))),
               "a store leaves the second word's register in the record's second word, the first as the slot found "
               "it");
    check_that(frame_answer(FRAME_EXIT, 7u, word, 0ull, &answered) &&
                   (answered == (unsigned long long)RUN_UNWRITTEN_WORD),
               "an exit leaves the fill word");
    check_that(frame_answer(FRAME_NUMBER, 7u, word, 0x20ull, &answered) && (answered == 0x20ull),
               "a number is its reading on every case");
    check_that(frame_answer(FRAME_DIM, 7u, word, 0ull, &answered) && (answered == RUN_THREADS_A_BLOCK) &&
                   frame_answer(FRAME_DIM, 258u, word, 0ull, &answered) && (answered == RUN_THREADS_A_BLOCK),
               "the block size is the threads a block on every case");
    check_that(frame_answer(FRAME_PARAMETER, 7u, word, RUN_COUNT_PLACE, &answered) && (answered == FRAME_LAUNCHED) &&
                   frame_answer(FRAME_PARAMETER, 514u, word, RUN_COUNT_PLACE, &answered) &&
                   (answered == FRAME_LAUNCHED),
               "a parameter's word is the count the launch hands the kernel on every case");
    check_that((FRAME_LAUNCHED >= FRAME_CASES) && ((FRAME_LAUNCHED % RUN_THREADS_A_BLOCK) == 0u) &&
                   ((FRAME_LAUNCHED - FRAME_CASES) < RUN_THREADS_A_BLOCK),
               "and that count is the fewest whole blocks that give every frame case a thread");
    check_that(!frame_answer(FRAME_RELATION_COUNT, 7u, word, 0ull, &answered),
               "and no relation past the eight answers");
    check_that(((FRAME_READ_AT % RUN_THREADS_A_BLOCK) != (FRAME_READ_AT / RUN_THREADS_A_BLOCK)) &&
                   ((FRAME_READ_AT % RUN_THREADS_A_BLOCK) != 0u) && ((FRAME_READ_AT / RUN_THREADS_A_BLOCK) != 0u),
               "the thread the fields are read at has a lane and a block that differ, both past 0");
    check_that(FRAME_CASES > FRAME_READ_AT, "and the frame's cases reach it");
    check_that((frame_place_destination(FRAME_LANE) == 0u) && (frame_place_destination(FRAME_BLOCK) == 0u) &&
                   (frame_place_destination(FRAME_DIM) == 0u) && (frame_place_number(FRAME_DIM) == FRAME_PLACE_NONE) &&
                   (frame_place_destination(FRAME_NUMBER) == 1u) && (frame_place_destination(FRAME_LOAD) == 2u) &&
                   (frame_place_destination(FRAME_STORE) == FRAME_PLACE_NONE) &&
                   (frame_place_destination(FRAME_EXIT) == FRAME_PLACE_NONE) &&
                   (frame_place_destination(FRAME_PARAMETER) == 1u) && (frame_place_number(FRAME_PARAMETER) == 0u),
               "the destination's place is the tuple's last where the relation writes one");
    check_that((frame_place_address(FRAME_LOAD) == 0u) && (frame_place_address(FRAME_STORE) == FRAME_PLACE_NONE) &&
                   (frame_place_number(FRAME_NUMBER) == 0u) && (frame_place_number(FRAME_LOAD) == 1u) &&
                   (frame_place_number(FRAME_STORE) == 0u) && (frame_place_register(FRAME_STORE) == 1u) &&
                   (frame_place_guard(FRAME_EXIT) == 0u) && (frame_place_guard(FRAME_LANE) == FRAME_PLACE_NONE),
               "the address, number, register and guard places are the relations'");
    check_that((RUN_UNWRITTEN_WORD == CUBIN_RUN_FILL_WORD) && (RUN_THREADS_A_BLOCK == CUBIN_RUN_THREADS) &&
                   (RUN_COUNT_PLACE == CUBIN_RUN_COUNT_PLACE),
               "the channel's fill word, block and count's place are the carrier's own");
    printf("frame_relation_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
