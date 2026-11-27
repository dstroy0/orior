// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file subtraction_record.h
 * @brief Subtraction games on the record machine: the Grundy sequence of each game, its exact period, and its values
 *        as records of their own.
 *
 * @note A player takes any number of the move set from one heap, and the player who cannot move loses. The value at n
 *       is the least whole number absent from the values that a move reaches from n. A record program steps every game
 *       one position a sweep. The state of a game holds its last `largest` values.
 * @note The exact period is the shortest shift that the sequence repeats under, checked to the end. Its start is one
 *       past the last place where the shift fails. The start must lie below half the length, and below the length less
 *       three periods. A second program marks each pair a shift apart that differs past that bound. The sum of the
 *       marks of a game is 0 at its period.
 * @note The value of a sum of two games is the exclusive or of their values, by the theorem of Sprague and Grundy. A
 *       third program writes that value for a pair of positions of one game.
 */
#ifndef SUBTRACTION_RECORD_H
#define SUBTRACTION_RECORD_H

#include "../../src/platform/platform.h"
#include "game_record.h"
#include "memoria_operor/memoria_operor.h"

/** @brief The games of a sweep on the device, and the state of each after each position. */
typedef struct
{
    unsigned int games;       /**< The games. */
    unsigned int positions;   /**< The positions of each sequence. */
    unsigned int largest;     /**< The largest move. */
    unsigned int width;       /**< The bits of a value. */
    unsigned int state_limbs; /**< The limbs of a state record. */
    GameProgram step;         /**< The step of the sequences. */
    GameProgram differ;       /**< The mark of a pair a shift apart. */
    GameProgram gather;       /**< A value as a record of its own. */
    GameProgram exclusive;    /**< The exclusive or of two values of one game. */
    unsigned int differ_step; /**< The step of the mark. */
    unsigned int *masks;      /**< The moves of each game on the device [OWNS]. */
    unsigned int *places;     /**< The positions 0 to positions - 1 on the device [OWNS]. */
    unsigned int *zero;       /**< One state of zeros [OWNS]. */
    unsigned int *history;    /**< The state after each position, position by position, game by game [OWNS]. */
    unsigned int *index;      /**< The index of a sweep [OWNS]. */
    unsigned int *out;        /**< The records of a sweep [OWNS]. */
    unsigned long long room;  /**< The lanes that a sweep has room for. */
    unsigned int *host_index; /**< The index on the host [OWNS]. */
    unsigned int *sums;       /**< The sums of a sweep of marks [OWNS]. */
} SubtractionGames;

/**
 * @brief The step of the Grundy sequences: from the last `largest` values of each game, its value at position n.
 *
 * @param[in]  largest The largest move, and the values that a state holds.
 * @param[in]  most    The max moves, and so the largest value.
 * @param[in]  width   The bits of a value.
 * @param[out] program The program [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 * @note Member 0 is the state: the value d back at field d - 1. Member 1 is the moves of the game, bit d - 1 for the
 *       move d. Member 2 is the position. The value is the least whole number that no move of at most n reaches.
 * @note The outputs are the new state, the new value first, each value narrowed to its own bits. Each output is one
 *       bit wider than its register. A state field is then width + 1 bits.
 */
static inline int subtraction_make_step(unsigned int largest, unsigned int most, unsigned int width,
                                        GameProgram *program, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    unsigned int state[32];
    for (unsigned int back = 0u; back < largest; back += 1u)
    {
        const unsigned int field = exact_record_member_field(&built, width + 1u, back * (width + 1u));
        state[back] = exact_record_read_unsigned(&built, field, 0u);
    }
    const unsigned int moves = exact_record_read_unsigned(&built, exact_record_member_field(&built, largest, 0u), 1u);
    const unsigned int position = exact_record_read_unsigned(&built, exact_record_member_field(&built, 32u, 0u), 2u);
    unsigned int on[32];
    for (unsigned int back = 0u; back < largest; back += 1u)
    {
        const unsigned int has = exact_record_bit(&built, moves, back);
        const unsigned int reach = exact_record_above(&built, position, exact_record_constant(&built, back));
        on[back] = exact_record_product(&built, has, reach);
    }
    unsigned int value = exact_record_constant(&built, most + 1u);
    for (unsigned int candidate = most + 1u; candidate > 0u; candidate -= 1u)
    {
        const unsigned int number = exact_record_constant(&built, candidate - 1u);
        unsigned int hits = exact_record_constant(&built, 0u);
        for (unsigned int back = 0u; back < largest; back += 1u)
        {
            hits = exact_record_sum(
                &built, hits, exact_record_product(&built, on[back], exact_record_equal(&built, state[back], number)));
        }
        const unsigned int seen = exact_record_above(&built, hits, exact_record_constant(&built, 0u));
        value = exact_record_select(&built, seen, value, number);
    }
    unsigned int outputs[32];
    outputs[0] = exact_record_narrow(&built, value, width);
    for (unsigned int back = 1u; back < largest; back += 1u)
    {
        outputs[back] = exact_record_narrow(&built, state[back - 1u], width);
    }
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {((largest * (width + 1u)) + 31u) / 32u, 1u, 1u};
    const int ok = game_load(&built, 3u, outputs, largest, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief A program of two values of state records: the mark 1 where they differ, or their exclusive or.
 *
 * @param[in]  width      The bits of a value.
 * @param[in]  in_limbs_0 The limbs of a state record.
 * @param[in]  exclusive  1 for the exclusive or, 0 for the mark.
 * @param[out] program    The program [BORROWS].
 * @param[out] output     The step of its value [BORROWS].
 * @param[out] error      The error [BORROWS].
 * @return                1 where it loaded.
 */
static inline int subtraction_make_pair(unsigned int width, unsigned int in_limbs_0, int exclusive,
                                        GameProgram *program, unsigned int *output, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int field = exact_record_member_field(&built, width + 1u, 0u);
    const unsigned int here = exact_record_read_unsigned(&built, field, 0u);
    const unsigned int there = exact_record_read_unsigned(&built, field, 1u);
    const unsigned int value =
        exclusive ? exact_record_narrow(&built, exact_record_step(&built, ENGINE_RECORD_XOR, here, there, 0u), width)
                  : exact_record_difference(&built, exact_record_constant(&built, 1u),
                                            exact_record_equal(&built, here, there));
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {in_limbs_0, in_limbs_0, 0u};
    *output = value;
    const int ok = game_load(&built, 2u, &value, 1u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief The gather of a value: the value of a state record, written as a record of its own.
 *
 * @param[in]  width      The bits of a value.
 * @param[in]  in_limbs_0 The limbs of a state record.
 * @param[out] program    The program [BORROWS].
 * @param[out] error      The error [BORROWS].
 * @return                1 where it loaded.
 */
static inline int subtraction_make_gather(unsigned int width, unsigned int in_limbs_0, GameProgram *program,
                                          EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int read = exact_record_read_unsigned(&built, exact_record_member_field(&built, width + 1u, 0u), 0u);
    const unsigned int value = exact_record_narrow(&built, read, width);
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {in_limbs_0, 0u, 0u};
    const int ok = game_load(&built, 1u, &value, 1u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/** @brief The device bytes that a sweep of `games` games over `positions` positions asks for. */
static inline unsigned long long subtraction_bytes(unsigned long long games, unsigned long long positions,
                                                   unsigned long long room)
{
    const unsigned long long lanes = (room > (positions * games)) ? room : (positions * games);
    return (((positions * games) + 1ULL) * 4ULL * 4ULL) + (lanes * 3ULL * 4ULL) + (lanes * 4ULL * 4ULL) +
           (positions * 4ULL) + (games * 4ULL) + 64ULL;
}

/** @brief Sends the index of a sweep. */
static inline int subtraction_send(SimResults *results, SubtractionGames *set, unsigned long long entries)
{
    return sim_status_check(results, cudaMemcpy(set->index, set->host_index, entries * 4ULL, cudaMemcpyHostToDevice),
                            "index sent");
}

/** @brief Gives a sweep back. */
static inline void subtraction_close(SubtractionGames *set)
{
    game_release(&set->step);
    game_release(&set->differ);
    game_release(&set->gather);
    game_release(&set->exclusive);
    cudaFree(set->masks);
    cudaFree(set->places);
    cudaFree(set->zero);
    cudaFree(set->history);
    cudaFree(set->index);
    cudaFree(set->out);
    platform_cell_released(set->host_index, platform_place_g_program_owner);
    platform_cell_released(set->sums, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = set, .bytes = sizeof(*set), .val = 0);
}

/**
 * @brief Loads the programs and steps every game through every position on the device.
 *
 * @param[in,out] results   The results [BORROWS].
 * @param[out]    set       The sweep [BORROWS].
 * @param[in]     masks     The moves of each game, bit d - 1 for the move d [BORROWS].
 * @param[in]     games     The games.
 * @param[in]     largest   The largest move.
 * @param[in]     most      The most moves of any game, and so the largest value.
 * @param[in]     positions The positions of each sequence.
 * @param[in]     room      The lanes that a later sweep of the caller needs, at least.
 * @return                  1 where every program loaded and every sweep ran.
 * @note The state of game g after position n is record n games + g of the history. Its value is that at n.
 */
static inline int subtraction_open(SimResults *results, SubtractionGames *set, const unsigned int *masks,
                                   unsigned int games, unsigned int largest, unsigned int most, unsigned int positions,
                                   unsigned long long room)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = set, .bytes = sizeof(*set), .val = 0);
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    set->games = games;
    set->positions = positions;
    set->largest = largest;
    set->width = exact_record_bits_of(most);
    unsigned int exclusive_step = 0u;
    int ok = subtraction_make_step(largest, most, set->width, &set->step, &error);
    set->state_limbs = set->step.layout.out_limbs;
    ok = ok && subtraction_make_pair(set->width, set->state_limbs, 0, &set->differ, &set->differ_step, &error) &&
         subtraction_make_pair(set->width, set->state_limbs, 1, &set->exclusive, &exclusive_step, &error) &&
         subtraction_make_gather(set->width, set->state_limbs, &set->gather, &error);
    sim_check(results, ok, "the four record programs of the subtraction games imprint, arrange and load");
    const unsigned long long records = ((unsigned long long)positions * games) + 1ULL;
    set->room = (room > ((unsigned long long)positions * games)) ? room : ((unsigned long long)positions * games);
    ok = ok && sim_status_check(results, cudaMalloc((void **)&set->masks, games * 4ULL), "games") &&
         sim_status_check(results, cudaMalloc((void **)&set->places, positions * 4ULL), "places") &&
         sim_status_check(results, cudaMalloc((void **)&set->zero, set->state_limbs * 4ULL), "zero state") &&
         sim_status_check(results, cudaMalloc((void **)&set->history, records * set->state_limbs * 4ULL), "history") &&
         sim_status_check(results, cudaMalloc((void **)&set->index, set->room * 3ULL * 4ULL), "index") &&
         sim_status_check(results, cudaMalloc((void **)&set->out, set->room * 4ULL * 4ULL), "out");
    set->host_index =
        (unsigned int *)platform_cell_taken((size_t)(set->room * 3ULL * 4ULL), platform_place_g_program_owner, 0);
    set->sums =
        (unsigned int *)platform_cell_taken((size_t)((games + 1ULL) * 2ULL * 4ULL), platform_place_g_program_owner, 0);
    unsigned int *const places =
        (unsigned int *)platform_cell_taken((size_t)(positions * 4ULL), platform_place_g_program_owner, 0);
    ok = ok && (set->host_index != NULL) && (set->sums != NULL) && (places != NULL);
    for (unsigned int place = 0u; ok && (place < positions); place += 1u)
    {
        places[place] = place;
    }
    ok = ok &&
         sim_status_check(results, cudaMemcpy(set->masks, masks, games * 4ULL, cudaMemcpyHostToDevice), "games sent") &&
         sim_status_check(results, cudaMemcpy(set->places, places, positions * 4ULL, cudaMemcpyHostToDevice),
                          "places sent") &&
         sim_status_check(results, cudaMemset(set->zero, 0, set->state_limbs * 4ULL), "zero state set") &&
         sim_status_check(results, cudaMemset(set->history, 0, records * set->state_limbs * 4ULL), "history cleared");
    platform_cell_released(places, platform_place_g_program_owner);
    // one sweep a position: lane g reads its state, its game and the position. The first position reads one state of
    // zeros for every game.
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        set->host_index[(3u * game) + 0u] = 0u;
        set->host_index[(3u * game) + 1u] = game;
        set->host_index[(3u * game) + 2u] = 0u;
    }
    ok = ok && subtraction_send(results, set, games * 3ULL);
    for (unsigned int place = 0u; ok && (place < positions); place += 1u)
    {
        if (place == 1u)
        {
            for (unsigned int game = 0u; game < games; game += 1u)
            {
                set->host_index[3u * game] = game;
            }
            ok = subtraction_send(results, set, games * 3ULL);
        }
        const unsigned int *const before =
            (place == 0u) ? set->zero : (set->history + ((unsigned long long)(place - 1u) * games * set->state_limbs));
        const CycleRecordRunRequest run = {set->step.record,
                                           {before, set->masks, set->places + place},
                                           {(place == 0u) ? 1ULL : games, games, 1ULL},
                                           set->index,
                                           games,
                                           set->history + ((unsigned long long)place * games * set->state_limbs),
                                           &error};
        ok = ok && (cycle_record_run(&run) == (long)games);
    }
    sim_check(results, ok, "the Grundy step sweeps every position of every game");
    return ok;
}

/**
 * @brief The exact period of each game: the first shift where the pairs that differ past the bound sum to 0.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in,out] set     The sweep [BORROWS].
 * @param[out]    periods The period of each game, or 0 where it does not settle inside the sequence [BORROWS].
 * @return                1 where every sweep and sum ran.
 */
static inline int subtraction_periods(SimResults *results, SubtractionGames *set, unsigned int *periods)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const unsigned int games = set->games;
    const unsigned int length = set->positions;
    const unsigned long long records = (unsigned long long)length * games;
    unsigned int *const open =
        (unsigned int *)platform_cell_taken((games + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    int ok = open != NULL;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = periods, .bytes = games * sizeof(unsigned int), .val = 0);
    unsigned int settled = 0u;
    for (unsigned int shift = 1u; ok && (settled < games) && (shift <= (length / 4u)); shift += 1u)
    {
        const unsigned int cap = length / 2u;
        const unsigned int room = length - (3u * shift);
        const unsigned int bound = (cap < room) ? cap : room;
        const unsigned int from = bound - 1u;
        const unsigned int to = length - shift;
        const unsigned int run_length = (to > from) ? (to - from) : 0u;
        unsigned int open_count = 0u;
        for (unsigned int game = 0u; game < games; game += 1u)
        {
            if (periods[game] == 0u)
            {
                open[open_count] = game;
                open_count += 1u;
            }
        }
        if (run_length == 0u)
        {
            for (unsigned int at = 0u; at < open_count; at += 1u)
            {
                periods[open[at]] = shift;
            }
            settled += open_count;
            continue;
        }
        unsigned long long lane = 0ULL;
        for (unsigned int at = 0u; at < open_count; at += 1u)
        {
            for (unsigned int place = from; place < to; place += 1u)
            {
                set->host_index[2ULL * lane] = (place * games) + open[at];
                set->host_index[(2ULL * lane) + 1ULL] = ((place + shift) * games) + open[at];
                lane += 1ULL;
            }
        }
        ok = subtraction_send(results, set, lane * 2ULL);
        const CycleRecordRunRequest run = {set->differ.record,
                                           {set->history, set->history, NULL},
                                           {records, records, 0ULL},
                                           set->index,
                                           lane,
                                           set->out,
                                           &error};
        ok = ok && (cycle_record_run(&run) == (long)lane) &&
             game_sum(&set->differ, set->differ_step, set->out, lane, run_length, set->sums, &error);
        for (unsigned int at = 0u; ok && (at < open_count); at += 1u)
        {
            if (game_sum_value(set->sums, at) == 0LL)
            {
                periods[open[at]] = shift;
                settled += 1u;
            }
        }
    }
    sim_check(results, ok, "the shifts sweep and sum on the device");
    platform_cell_released(open, platform_place_g_program_owner);
    return ok;
}

/**
 * @brief Writes values as records of their own, one a lane, each from a state record of the history.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in,out] set     The sweep [BORROWS].
 * @param[in]     from    The state record of each value, or two records a value for the exclusive or [BORROWS].
 * @param[in]     count   The values.
 * @param[in]     pairs   1 where each value is the exclusive or of two records, 0 where it is one.
 * @param[out]    to      The value records on the device, one limb each [BORROWS].
 * @return                1 where the sweep ran.
 */
static inline int subtraction_write(SimResults *results, SubtractionGames *set, const unsigned int *from,
                                    unsigned long long count, int pairs, unsigned int *to)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const unsigned long long records = (unsigned long long)set->positions * set->games;
    const unsigned long long entries = count * (pairs ? 2ULL : 1ULL);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = set->host_index, .src = from, .bytes = (size_t)(entries * 4ULL));
    const CycleRecordRunRequest run = {pairs ? set->exclusive.record : set->gather.record,
                                       {set->history, pairs ? set->history : NULL, NULL},
                                       {records, pairs ? records : 0ULL, 0ULL},
                                       set->index,
                                       count,
                                       to,
                                       &error};
    return subtraction_send(results, set, entries) && (cycle_record_run(&run) == (long)count);
}

#endif
