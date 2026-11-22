// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file twister_record.h
 * @brief The Mersenne Twister MT19937 on the record machine, seeded and drawn as CPython seeds and draws it.
 *
 * @note The generators of a set run in step. Each state word is a record of its own, 624 a generator, generator by
 *       generator. A sweep advances one step of the seed for every generator at once. The host places each column of
 *       outputs in the state.
 * @note A twist is four sweeps over every generator, in the order that its dependences ask. The words 0 to 226 read
 *       the old state alone. The words 227 to 453 read the first new words, and the words 454 to 622 the second. Word
 *       623 reads the new words 0 and 396. A temper sweep then writes the next 624 outputs of each generator to its
 * stream.
 * @note A draw below n is getrandbits of the bit length of n, again until it is below n. A sweep reads the next word
 *       of each generator that draws, at its own place in its own stream. The host then moves each place on.
 * @note The seed enters by init_by_array over one 32-bit word, as CPython seeds from an int of at most 32 bits.
 * @note The generator is that of Matsumoto and Nishimura (1998).
 */
#ifndef TWISTER_RECORD_H
#define TWISTER_RECORD_H

#include "game_record.h"
#include "../../src/platform/platform.h"

/** @brief The words of the state of MT19937, and the reach of its twist. */
#define TWISTER_WORDS 624u
#define TWISTER_REACH 397u

/** @brief The bytes of a word record: a 32-bit word one bit wider for its sign, two limbs. */
#define TWISTER_RECORD_LIMBS 2u
#define TWISTER_RECORD_BYTES (TWISTER_RECORD_LIMBS * 4u)

/** @brief The rounds of 624 outputs that a stream holds before it grows. */
#define TWISTER_ROUNDS_FIRST 4u

/** @brief The programs of the twister. */
enum
{
    TWISTER_GENRAND,   /**< One step of init_genrand. */
    TWISTER_ARRAY_ONE, /**< One step of the first loop of init_by_array. */
    TWISTER_ARRAY_TWO, /**< One step of the second loop of init_by_array. */
    TWISTER_TWIST,     /**< One word of the twist. */
    TWISTER_TEMPER,    /**< The temper of one word. */
    TWISTER_DRAW,      /**< One draw of k bits against a bound. */
    TWISTER_PROGRAMS
};

/** @brief A set of generators on the device, in step. */
typedef struct
{
    GameProgram programs[TWISTER_PROGRAMS]; /**< The programs. */
    unsigned int draw_value;                /**< The step of the drawn value of the draw program. */
    unsigned int draw_accept;               /**< The step of its acceptance. */
    unsigned long long count;               /**< The generators. */
    unsigned int *state;                    /**< The state, 624 words a generator [OWNS]. */
    unsigned int *fresh;                    /**< The state that a twist writes [OWNS]. */
    unsigned int *stream;                   /**< The outputs, round by round, 624 a generator a round [OWNS]. */
    unsigned long long rounds;              /**< The rounds the stream holds. */
    unsigned long long capacity;            /**< The rounds it has room for. */
    unsigned int *steps;                    /**< One shared record a step, the step number [OWNS]. */
    unsigned int *seeds;                    /**< The seed of each generator, a limb a record [OWNS]. */
    unsigned int *requests;                 /**< The shift and the bound of each draw, two limbs a record [OWNS]. */
    unsigned int *index;                    /**< The index of a sweep [OWNS]. */
    unsigned int *out;                      /**< The records of a sweep [OWNS]. */
    unsigned long long room;                /**< The lanes that index and out have room for. */
    unsigned long long *place;              /**< The next output of each generator, on the host [OWNS]. */
    unsigned int *host_index;               /**< The index on the host [OWNS]. */
    unsigned int *host_out;                 /**< The records of a sweep on the host [OWNS]. */
    unsigned int *host_requests;            /**< The requests on the host [OWNS]. */
} TwisterSet;

/** @brief x AND (2^32 - 1), the residue of a value modulo 2^32, as a register of 32 bits. */
static inline unsigned int twister_low_word(ExactRecordProgram *built, unsigned int value)
{
    return exact_record_step(built, ENGINE_RECORD_AND, value, exact_record_mask(built, 32u), 0u);
}

/** @brief x XOR (x >> shift), for a word that is not negative. */
static inline unsigned int twister_mix_down(ExactRecordProgram *built, unsigned int value, unsigned int shift)
{
    return exact_record_step(built, ENGINE_RECORD_XOR, value,
                             exact_record_quotient(built, value, exact_record_power_two(built, shift)), 0u);
}

/**
 * @brief Builds and loads one program of the twister.
 *
 * @param[in]  which   The program.
 * @param[out] program The program [BORROWS].
 * @param[out] value   The step of its value [BORROWS].
 * @param[out] accept  The step of the acceptance of a draw, or 0 [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 */
static inline int twister_make(unsigned int which, GameProgram *program, unsigned int *value, unsigned int *accept,
                               EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int word = exact_record_member_field(&built, 32u, 0u);
    unsigned int outputs[2] = {0u, 0u};
    unsigned int output_count = 1u;
    unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {TWISTER_RECORD_LIMBS, 0u, 0u};
    unsigned int members = 1u;
    if (which == TWISTER_GENRAND)
    {
        // mt[i] = 1812433253 (mt[i - 1] ^ (mt[i - 1] >> 30)) + i, modulo 2^32
        const unsigned int before = exact_record_read_unsigned(&built, word, 0u);
        const unsigned int step = exact_record_read_unsigned(&built, exact_record_member_field(&built, 32u, 0u), 1u);
        const unsigned int mixed = exact_record_product(&built, twister_mix_down(&built, before, 30u),
                                                        exact_record_constant(&built, 1812433253u));
        outputs[0] = twister_low_word(&built, exact_record_sum(&built, mixed, step));
        in_limbs[1] = 1u;
        members = 2u;
    }
    else if ((which == TWISTER_ARRAY_ONE) || (which == TWISTER_ARRAY_TWO))
    {
        // The first loop gives mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) 1664525)) + key[0].
        // The second gives mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) 1566083941)) - i. Each is modulo 2^32.
        const unsigned int before = exact_record_read_unsigned(&built, word, 0u);
        const unsigned int here = exact_record_read_unsigned(&built, word, 1u);
        const unsigned int extra = exact_record_read_unsigned(&built, exact_record_member_field(&built, 32u, 0u), 2u);
        const unsigned long long factor = (which == TWISTER_ARRAY_ONE) ? 1664525ULL : 1566083941ULL;
        const unsigned int mixed =
            twister_low_word(&built, exact_record_product(&built, twister_mix_down(&built, before, 30u),
                                                          exact_record_constant(&built, factor)));
        const unsigned int crossed = exact_record_step(&built, ENGINE_RECORD_XOR, here, mixed, 0u);
        const unsigned int moved = (which == TWISTER_ARRAY_ONE) ? exact_record_sum(&built, crossed, extra)
                                                                : exact_record_difference(&built, crossed, extra);
        outputs[0] = twister_low_word(&built, moved);
        in_limbs[1] = TWISTER_RECORD_LIMBS;
        in_limbs[2] = 1u;
        members = 3u;
    }
    else if (which == TWISTER_TWIST)
    {
        // y = (mt[kk] & 0x80000000) | (mt[kk + 1] & 0x7fffffff); mt[kk] = mt[far] ^ (y >> 1) ^ (y & 1) 0x9908b0df.
        // The two parts of y share no bit, and their sum is their or.
        const unsigned int here = exact_record_read_unsigned(&built, word, 0u);
        const unsigned int next = exact_record_read_unsigned(&built, word, 1u);
        const unsigned int far = exact_record_read_unsigned(&built, word, 2u);
        const unsigned int upper =
            exact_record_step(&built, ENGINE_RECORD_AND, here, exact_record_constant(&built, 0x80000000u), 0u);
        const unsigned int lower =
            exact_record_step(&built, ENGINE_RECORD_AND, next, exact_record_constant(&built, 0x7fffffffu), 0u);
        const unsigned int joined = exact_record_sum(&built, upper, lower);
        const unsigned int half = exact_record_quotient(&built, joined, exact_record_constant(&built, 2u));
        const unsigned int odd = exact_record_remainder(&built, joined, exact_record_constant(&built, 2u));
        const unsigned int matrix = exact_record_product(&built, odd, exact_record_constant(&built, 0x9908b0dfu));
        const unsigned int crossed = exact_record_step(&built, ENGINE_RECORD_XOR, far, half, 0u);
        outputs[0] = twister_low_word(&built, exact_record_step(&built, ENGINE_RECORD_XOR, crossed, matrix, 0u));
        in_limbs[1] = TWISTER_RECORD_LIMBS;
        in_limbs[2] = TWISTER_RECORD_LIMBS;
        members = 3u;
    }
    else if (which == TWISTER_TEMPER)
    {
        // y ^= y >> 11; y ^= (y << 7) & 0x9d2c5680; y ^= (y << 15) & 0xefc60000; y ^= y >> 18
        unsigned int tempered = twister_mix_down(&built, exact_record_read_unsigned(&built, word, 0u), 11u);
        const unsigned int seven = exact_record_step(
            &built, ENGINE_RECORD_AND, exact_record_product(&built, tempered, exact_record_power_two(&built, 7u)),
            exact_record_constant(&built, 0x9d2c5680u), 0u);
        tempered = exact_record_step(&built, ENGINE_RECORD_XOR, tempered, seven, 0u);
        const unsigned int fifteen = exact_record_step(
            &built, ENGINE_RECORD_AND, exact_record_product(&built, tempered, exact_record_power_two(&built, 15u)),
            exact_record_constant(&built, 0xefc60000u), 0u);
        tempered = exact_record_step(&built, ENGINE_RECORD_XOR, tempered, fifteen, 0u);
        outputs[0] = twister_low_word(&built, twister_mix_down(&built, tempered, 18u));
    }
    else
    {
        // getrandbits(k) = word >> (32 - k). The run keeps the draw where it is below the bound.
        const unsigned int drawn = exact_record_read_unsigned(&built, word, 0u);
        const unsigned int shift = exact_record_read_unsigned(&built, exact_record_member_field(&built, 6u, 0u), 1u);
        const unsigned int bound = exact_record_read_unsigned(&built, exact_record_member_field(&built, 32u, 32u), 1u);
        const unsigned int bits = exact_record_quotient(&built, drawn, exact_record_two_to(&built, shift, 6u));
        outputs[0] = twister_low_word(&built, bits);
        outputs[1] = exact_record_above(&built, bound, bits);
        output_count = 2u;
        in_limbs[1] = 2u;
        members = 2u;
    }
    *value = outputs[0];
    *accept = outputs[1];
    const int ok = game_load(&built, members, outputs, output_count, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/** @brief Gives a set back. */
static inline void twister_close(TwisterSet *set)
{
    for (unsigned int which = 0u; which < TWISTER_PROGRAMS; which += 1u)
    {
        game_release(&set->programs[which]);
    }
    cudaFree(set->state);
    cudaFree(set->fresh);
    cudaFree(set->stream);
    cudaFree(set->steps);
    cudaFree(set->seeds);
    cudaFree(set->requests);
    cudaFree(set->index);
    cudaFree(set->out);
    platform_cell_released(set->place, platform_place_g_program_owner);
    platform_cell_released(set->host_index, platform_place_g_program_owner);
    platform_cell_released(set->host_out, platform_place_g_program_owner);
    platform_cell_released(set->host_requests, platform_place_g_program_owner);
    memset(set, 0, sizeof(*set));
}

/** @brief The device bytes that a set of `count` generators asks for, at its first stream. */
static inline unsigned long long twister_bytes(unsigned long long count)
{
    const unsigned long long words = count * TWISTER_WORDS;
    return (3ULL * words * TWISTER_RECORD_BYTES) + (TWISTER_ROUNDS_FIRST * words * TWISTER_RECORD_BYTES) +
           (TWISTER_WORDS * 4ULL) + (count * 12ULL) + (words * 3ULL * 4ULL);
}

/**
 * @brief Runs one program over `lanes` lanes into the sweep buffer.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in]     set     The set [BORROWS].
 * @param[in]     which   The program.
 * @param[in]     inputs  The record of each member on the device [BORROWS].
 * @param[in]     bodies  The records of each member.
 * @param[in]     index   The index on the host, one entry a member a lane [BORROWS].
 * @param[in]     lanes   The lanes.
 * @return                1 where it ran.
 */
static inline int twister_run(SimResults *results, TwisterSet *set, unsigned int which,
                              const unsigned int *const inputs[ENGINE_RECORD_MEMBERS_MAX],
                              const unsigned long long bodies[ENGINE_RECORD_MEMBERS_MAX], const unsigned int *index,
                              unsigned long long lanes)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    unsigned int members = 0u;
    while ((members < ENGINE_RECORD_MEMBERS_MAX) && (inputs[members] != NULL))
    {
        members += 1u;
    }
    const CycleRecordRunRequest run = {set->programs[which].record,
                                       {inputs[0], inputs[1], inputs[2]},
                                       {bodies[0], bodies[1], bodies[2]},
                                       set->index,
                                       lanes,
                                       set->out,
                                       &error};
    return sim_status_check(results, cudaMemcpy(set->index, index, lanes * members * 4ULL, cudaMemcpyHostToDevice),
                            "twister index sent") &&
           (cycle_record_run(&run) == (long)lanes);
}

/**
 * @brief Places one word of each generator, a record a generator in the sweep buffer, at word `at` of the state.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in]     set     The set [BORROWS].
 * @param[in]     from    The records, one a generator, on the device [BORROWS].
 * @param[in]     at      The word.
 * @return                1 where the copy ran.
 */
static inline int twister_place_column(SimResults *results, TwisterSet *set, const unsigned int *from, unsigned int at)
{
    return sim_status_check(results,
                            cudaMemcpy2D(set->state + ((unsigned long long)at * TWISTER_RECORD_LIMBS),
                                         TWISTER_WORDS * TWISTER_RECORD_BYTES, from, TWISTER_RECORD_BYTES,
                                         TWISTER_RECORD_BYTES, set->count, cudaMemcpyDeviceToDevice),
                            "twister column placed");
}

/**
 * @brief One step of the seed: word i of each generator from word i - 1, and from word i itself past init_genrand.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in]     set     The set [BORROWS].
 * @param[in]     which   The program.
 * @param[in]     at      The word i.
 * @param[in]     shared  The record of the step on the device, or the seeds [BORROWS].
 * @param[in]     per     1 where the third member has a record a generator, 0 where it is one shared record.
 * @return                1 where it ran and placed its column.
 */
static inline int twister_seed_step(SimResults *results, TwisterSet *set, unsigned int which, unsigned int at,
                                    const unsigned int *shared, int per)
{
    const unsigned long long count = set->count;
    const int here = which != TWISTER_GENRAND;
    for (unsigned long long one = 0ULL; one < count; one += 1ULL)
    {
        // a word of the state is below 2^32 records
        const unsigned int base = (unsigned int)(one * TWISTER_WORDS);
        if (here)
        {
            set->host_index[(3ULL * one) + 0ULL] = base + at - 1u;
            set->host_index[(3ULL * one) + 1ULL] = base + at;
            set->host_index[(3ULL * one) + 2ULL] = per ? (unsigned int)one : 0u;
        }
        else
        {
            set->host_index[(2ULL * one) + 0ULL] = base + at - 1u;
            set->host_index[(2ULL * one) + 1ULL] = 0u;
        }
    }
    const unsigned long long words = count * TWISTER_WORDS;
    const unsigned int *const inputs[ENGINE_RECORD_MEMBERS_MAX] = {set->state, here ? set->state : shared,
                                                                   here ? shared : NULL};
    const unsigned long long bodies[ENGINE_RECORD_MEMBERS_MAX] = {words, here ? words : 1ULL,
                                                                  here ? (per ? count : 1ULL) : 0ULL};
    return twister_run(results, set, which, inputs, bodies, set->host_index, count) &&
           twister_place_column(results, set, set->out, at);
}

/** @brief The copy mt[0] = mt[623] of every generator. */
static inline int twister_wrap(SimResults *results, TwisterSet *set)
{
    return sim_status_check(results,
                            cudaMemcpy2D(set->state, TWISTER_WORDS * TWISTER_RECORD_BYTES,
                                         set->state + ((TWISTER_WORDS - 1ULL) * TWISTER_RECORD_LIMBS),
                                         TWISTER_WORDS * TWISTER_RECORD_BYTES, TWISTER_RECORD_BYTES, set->count,
                                         cudaMemcpyDeviceToDevice),
                            "twister wrap");
}

/**
 * @brief One twist of every generator, and the temper of its 624 new words into the next round of its stream.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in]     set     The set [BORROWS].
 * @return                1 where it ran.
 */
static inline int twister_refill(SimResults *results, TwisterSet *set)
{
    const unsigned long long count = set->count;
    const unsigned long long words = count * TWISTER_WORDS;
    // the four parts of the twist: [first, last], and where word kk + 1 and word far come from
    const unsigned int parts[4][2] = {{0u, 226u}, {227u, 453u}, {454u, 622u}, {623u, 623u}};
    int ok = 1;
    for (unsigned int part = 0u; ok && (part < 4u); part += 1u)
    {
        const unsigned int first = parts[part][0];
        const unsigned int last = parts[part][1];
        const unsigned int width = (last - first) + 1u;
        unsigned long long lane = 0ULL;
        for (unsigned long long one = 0ULL; one < count; one += 1ULL)
        {
            // a word of the state is below 2^32 records
            const unsigned int base = (unsigned int)(one * TWISTER_WORDS);
            for (unsigned int word = first; word <= last; word += 1u)
            {
                // kk + 1 of the last word is word 0 of the new state. Far is kk + 397 of the old state below 227, and
                // kk - 227 of the new state past it. The old state sits in members 0 and 1, and the new in member 2,
                // or in member 1 for word 623.
                set->host_index[(3ULL * lane) + 0ULL] = base + word;
                set->host_index[(3ULL * lane) + 1ULL] = (word == 623u) ? base : (base + word + 1u);
                set->host_index[(3ULL * lane) + 2ULL] =
                    (word < 227u) ? (base + word + TWISTER_REACH) : (base + word - (TWISTER_WORDS - TWISTER_REACH));
                lane += 1ULL;
            }
        }
        const unsigned int *const next_source = (part == 3u) ? set->fresh : set->state;
        const unsigned int *const far_source = (part == 0u) ? set->state : set->fresh;
        const unsigned int *const inputs[ENGINE_RECORD_MEMBERS_MAX] = {set->state, next_source, far_source};
        const unsigned long long bodies[ENGINE_RECORD_MEMBERS_MAX] = {words, words, words};
        ok = twister_run(results, set, TWISTER_TWIST, inputs, bodies, set->host_index, lane) &&
             sim_status_check(results,
                              cudaMemcpy2D(set->fresh + ((unsigned long long)first * TWISTER_RECORD_LIMBS),
                                           TWISTER_WORDS * TWISTER_RECORD_BYTES, set->out,
                                           (unsigned long long)width * TWISTER_RECORD_BYTES,
                                           (unsigned long long)width * TWISTER_RECORD_BYTES, count,
                                           cudaMemcpyDeviceToDevice),
                              "twist placed");
    }
    // the new state is the state now
    unsigned int *const swap = set->state;
    set->state = set->fresh;
    set->fresh = swap;
    if (ok && (set->rounds == set->capacity))
    {
        const unsigned long long grown = set->capacity * 2ULL;
        unsigned int *stream = NULL;
        ok = sim_status_check(results, cudaMalloc((void **)&stream, grown * words * TWISTER_RECORD_BYTES), "stream") &&
             sim_status_check(
                 results,
                 cudaMemcpy(stream, set->stream, set->rounds * words * TWISTER_RECORD_BYTES, cudaMemcpyDeviceToDevice),
                 "stream moved");
        if (ok)
        {
            cudaFree(set->stream);
            set->stream = stream;
            set->capacity = grown;
        }
    }
    for (unsigned long long word = 0ULL; ok && (word < words); word += 1ULL)
    {
        // a word of the state is below 2^32 records
        set->host_index[word] = (unsigned int)word;
    }
    const unsigned int *const inputs[ENGINE_RECORD_MEMBERS_MAX] = {set->state, NULL, NULL};
    const unsigned long long bodies[ENGINE_RECORD_MEMBERS_MAX] = {words, 0ULL, 0ULL};
    ok = ok && twister_run(results, set, TWISTER_TEMPER, inputs, bodies, set->host_index, words) &&
         sim_status_check(results,
                          cudaMemcpy(set->stream + (set->rounds * words * TWISTER_RECORD_LIMBS), set->out,
                                     words * TWISTER_RECORD_BYTES, cudaMemcpyDeviceToDevice),
                          "stream written");
    set->rounds += ok ? 1ULL : 0ULL;
    return ok;
}

/**
 * @brief Opens a set of generators on the device and seeds each from its own seed, as random.Random(seed) does.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[out]    set     The set [BORROWS].
 * @param[in]     seeds   The seed of each generator, each below 2^32 [BORROWS].
 * @param[in]     count   The generators.
 * @return                1 where every program loaded and every step ran.
 */
static inline int twister_open(SimResults *results, TwisterSet *set, const unsigned int *seeds,
                               unsigned long long count)
{
    memset(set, 0, sizeof(*set));
    EngineError error;
    memset(&error, 0, sizeof(error));
    set->count = count;
    const unsigned long long words = count * TWISTER_WORDS;
    set->room = words;
    set->capacity = TWISTER_ROUNDS_FIRST;
    int ok = 1;
    for (unsigned int which = 0u; ok && (which < TWISTER_PROGRAMS); which += 1u)
    {
        unsigned int value = 0u;
        unsigned int accept = 0u;
        ok = twister_make(which, &set->programs[which], &value, &accept, &error);
        if (which == TWISTER_DRAW)
        {
            set->draw_value = value;
            set->draw_accept = accept;
        }
    }
    sim_check(results, ok, "the six record programs of the twister imprint, arrange and load");
    set->place = (unsigned long long *)platform_cell_cleared((size_t)count, sizeof(unsigned long long),
                                                             platform_place_g_program_owner, 0);
    set->host_index =
        (unsigned int *)platform_cell_taken((size_t)(words * 3ULL * 4ULL), platform_place_g_program_owner, 0);
    set->host_out =
        (unsigned int *)platform_cell_taken((size_t)(count * 4ULL * 4ULL), platform_place_g_program_owner, 0);
    set->host_requests =
        (unsigned int *)platform_cell_taken((size_t)(count * 2ULL * 4ULL), platform_place_g_program_owner, 0);
    ok = ok && (set->place != NULL) && (set->host_index != NULL) && (set->host_out != NULL) &&
         (set->host_requests != NULL) &&
         sim_status_check(results, cudaMalloc((void **)&set->state, words * TWISTER_RECORD_BYTES), "state") &&
         sim_status_check(results, cudaMalloc((void **)&set->fresh, words * TWISTER_RECORD_BYTES), "fresh state") &&
         sim_status_check(results, cudaMalloc((void **)&set->stream, set->capacity * words * TWISTER_RECORD_BYTES),
                          "stream") &&
         sim_status_check(results, cudaMalloc((void **)&set->steps, TWISTER_WORDS * 4ULL), "steps") &&
         sim_status_check(results, cudaMalloc((void **)&set->seeds, count * 4ULL), "seeds") &&
         sim_status_check(results, cudaMalloc((void **)&set->requests, count * 2ULL * 4ULL), "requests") &&
         sim_status_check(results, cudaMalloc((void **)&set->index, words * 3ULL * 4ULL), "twister index") &&
         sim_status_check(results, cudaMalloc((void **)&set->out, words * TWISTER_RECORD_BYTES), "twister out") &&
         sim_status_check(results, cudaMemset(set->state, 0, words * TWISTER_RECORD_BYTES), "state cleared") &&
         sim_status_check(results, cudaMemcpy(set->seeds, seeds, count * 4ULL, cudaMemcpyHostToDevice), "seeds sent");
    unsigned int step_numbers[TWISTER_WORDS];
    for (unsigned int step = 0u; step < TWISTER_WORDS; step += 1u)
    {
        step_numbers[step] = step;
    }
    ok = ok &&
         sim_status_check(results, cudaMemcpy(set->steps, step_numbers, sizeof(step_numbers), cudaMemcpyHostToDevice),
                          "steps sent");
    // init_genrand(19650218): word 0 is the constant, and each word i from word i - 1. Each is the same for every
    // generator.
    unsigned int *const column = set->host_out;
    for (unsigned long long one = 0ULL; ok && (one < count); one += 1ULL)
    {
        column[(2ULL * one) + 0ULL] = 19650218u;
        column[(2ULL * one) + 1ULL] = 0u;
    }
    unsigned int *device_column = NULL;
    ok = ok && sim_status_check(results, cudaMalloc((void **)&device_column, count * TWISTER_RECORD_BYTES), "column") &&
         sim_status_check(results,
                          cudaMemcpy(device_column, column, count * TWISTER_RECORD_BYTES, cudaMemcpyHostToDevice),
                          "column sent") &&
         twister_place_column(results, set, device_column, 0u);
    for (unsigned int at = 1u; ok && (at < TWISTER_WORDS); at += 1u)
    {
        ok = twister_seed_step(results, set, TWISTER_GENRAND, at, set->steps + at, 0);
    }
    // the first loop of init_by_array, 624 steps from i = 1, with key[0] the seed and j always 0
    unsigned int at = 1u;
    for (unsigned int step = 0u; ok && (step < TWISTER_WORDS); step += 1u)
    {
        ok = twister_seed_step(results, set, TWISTER_ARRAY_ONE, at, set->seeds, 1);
        at += 1u;
        if (ok && (at >= TWISTER_WORDS))
        {
            ok = twister_wrap(results, set);
            at = 1u;
        }
    }
    // the second loop, 623 steps on from where the first stopped
    for (unsigned int step = 0u; ok && (step < (TWISTER_WORDS - 1u)); step += 1u)
    {
        ok = twister_seed_step(results, set, TWISTER_ARRAY_TWO, at, set->steps + at, 0);
        at += 1u;
        if (ok && (at >= TWISTER_WORDS))
        {
            ok = twister_wrap(results, set);
            at = 1u;
        }
    }
    // mt[0] = 0x80000000
    for (unsigned long long one = 0ULL; ok && (one < count); one += 1ULL)
    {
        column[(2ULL * one) + 0ULL] = 0x80000000u;
        column[(2ULL * one) + 1ULL] = 0u;
    }
    ok = ok &&
         sim_status_check(results,
                          cudaMemcpy(device_column, column, count * TWISTER_RECORD_BYTES, cudaMemcpyHostToDevice),
                          "column sent") &&
         twister_place_column(results, set, device_column, 0u);
    cudaFree(device_column);
    ok = ok && twister_refill(results, set);
    sim_check(results, ok, "the twister seeds every generator and fills its first round");
    return ok;
}

/**
 * @brief One draw below a bound for each named generator, as random._randbelow draws it.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in,out] set     The set [BORROWS].
 * @param[in]     which   The generators that draw, each once [BORROWS].
 * @param[in]     count   How many.
 * @param[in]     bounds  The bound of each, from 1 to 2^32 - 1 [BORROWS].
 * @param[out]    draws   The draw of each [BORROWS].
 * @return                1 where every sweep ran.
 * @note Each draw is getrandbits of the bit length of its bound, again while it is at the bound or past it. A sweep
 *       reads the next output of each generator still to draw. A generator past the end of its stream waits while the
 *       twister fills the next round for every generator.
 */
static inline int twister_below(SimResults *results, TwisterSet *set, const unsigned int *which,
                                unsigned long long count, const unsigned int *bounds, unsigned int *draws)
{
    unsigned int *const pending =
        (unsigned int *)platform_cell_taken((size_t)((count + 1ULL) * 4ULL), platform_place_g_program_owner, 0);
    int ok = pending != NULL;
    unsigned long long open = 0ULL;
    for (unsigned long long one = 0ULL; ok && (one < count); one += 1ULL)
    {
        // a draw below 1 is 0 at once by the same rule: getrandbits(1) until it is 0; it draws like any other
        pending[open] = (unsigned int)one;
        open += 1ULL;
    }
    const unsigned long long words = set->count * TWISTER_WORDS;
    const unsigned int value_offset = set->programs[TWISTER_DRAW].layout.step_table[set->draw_value].out_offset;
    const unsigned int accept_offset = set->programs[TWISTER_DRAW].layout.step_table[set->draw_accept].out_offset;
    const unsigned int out_limbs = set->programs[TWISTER_DRAW].layout.out_limbs;
    while (ok && (open > 0ULL))
    {
        // a round more where a generator still to draw reads past the end of its stream
        unsigned long long deepest = 0ULL;
        for (unsigned long long at = 0ULL; at < open; at += 1ULL)
        {
            const unsigned long long place = set->place[which[pending[at]]];
            deepest = (place > deepest) ? place : deepest;
        }
        while (ok && (deepest >= (set->rounds * TWISTER_WORDS)))
        {
            ok = twister_refill(results, set);
        }
        for (unsigned long long at = 0ULL; ok && (at < open); at += 1ULL)
        {
            const unsigned int one = pending[at];
            const unsigned long long generator = which[one];
            const unsigned long long place = set->place[generator];
            // the round, the generator and the word of the output, a record of the stream below 2^32
            set->host_index[(2ULL * at) + 0ULL] = (unsigned int)(((place / TWISTER_WORDS) * words) +
                                                                 (generator * TWISTER_WORDS) + (place % TWISTER_WORDS));
            set->host_index[(2ULL * at) + 1ULL] = (unsigned int)at;
            const unsigned int bits = exact_record_bits_of(bounds[one]);
            set->host_requests[(2ULL * at) + 0ULL] = 32u - bits;
            set->host_requests[(2ULL * at) + 1ULL] = bounds[one];
        }
        const unsigned int *const inputs[ENGINE_RECORD_MEMBERS_MAX] = {set->stream, set->requests, NULL};
        const unsigned long long bodies[ENGINE_RECORD_MEMBERS_MAX] = {set->rounds * words, open, 0ULL};
        ok = ok &&
             sim_status_check(results,
                              cudaMemcpy(set->requests, set->host_requests, open * 2ULL * 4ULL, cudaMemcpyHostToDevice),
                              "requests sent") &&
             twister_run(results, set, TWISTER_DRAW, inputs, bodies, set->host_index, open) &&
             sim_status_check(results,
                              cudaMemcpy(set->host_out, set->out, open * out_limbs * 4ULL, cudaMemcpyDeviceToHost),
                              "draws read");
        unsigned long long still = 0ULL;
        for (unsigned long long at = 0ULL; ok && (at < open); at += 1ULL)
        {
            const unsigned int one = pending[at];
            set->place[which[one]] += 1ULL;
            const unsigned int *const record = set->host_out + (at * out_limbs);
            const unsigned int accept = (record[accept_offset / 32u] >> (accept_offset % 32u)) & 1u;
            // the drawn value is in the first two limbs of its record, its 32 bits at offset 0
            const unsigned int drawn = record[value_offset / 32u];
            if (accept != 0u)
            {
                draws[one] = drawn;
            }
            else
            {
                pending[still] = one;
                still += 1ULL;
            }
        }
        open = still;
    }
    platform_cell_released(pending, platform_place_g_program_owner);
    return ok;
}

/**
 * @brief The order that random.Random(seed).shuffle gives a list, for every generator of the set, each list of its own
 *        length.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in,out] set     The set [BORROWS].
 * @param[in]     lengths The length of the list of each generator [BORROWS].
 * @param[in]     stride  The places that `orders` holds for each generator, at least its longest length.
 * @param[out]    orders  The order of each generator: the place that ends at each position of its list [BORROWS].
 * @return                1 where every draw ran.
 * @note The shuffle is that of Fisher and Yates from the top. For i from length - 1 down to 1, a draw j below i + 1
 *       swaps the places i and j. The device draws each j. The host swaps the places as it writes the order.
 * @note A generator whose list is shorter than i draws no j at i.
 */
static inline int twister_shuffle_each(SimResults *results, TwisterSet *set, const unsigned int *lengths,
                                       unsigned int stride, unsigned int *orders)
{
    const unsigned long long count = set->count;
    unsigned int *const which =
        (unsigned int *)platform_cell_taken((size_t)(count * 4ULL), platform_place_g_program_owner, 0);
    unsigned int *const bounds =
        (unsigned int *)platform_cell_taken((size_t)(count * 4ULL), platform_place_g_program_owner, 0);
    unsigned int *const draws =
        (unsigned int *)platform_cell_taken((size_t)(count * 4ULL), platform_place_g_program_owner, 0);
    int ok = (which != NULL) && (bounds != NULL) && (draws != NULL);
    unsigned int longest = 0u;
    for (unsigned long long one = 0ULL; ok && (one < count); one += 1ULL)
    {
        for (unsigned int place = 0u; place < lengths[one]; place += 1u)
        {
            orders[(one * stride) + place] = place;
        }
        longest = (lengths[one] > longest) ? lengths[one] : longest;
    }
    for (unsigned int at = (longest > 0u) ? (longest - 1u) : 0u; ok && (at > 0u); at -= 1u)
    {
        unsigned long long drawing = 0ULL;
        for (unsigned long long one = 0ULL; one < count; one += 1ULL)
        {
            if (lengths[one] > at)
            {
                // the generators are a small count
                which[drawing] = (unsigned int)one;
                bounds[drawing] = at + 1u;
                drawing += 1ULL;
            }
        }
        ok = twister_below(results, set, which, drawing, bounds, draws);
        for (unsigned long long one = 0ULL; ok && (one < drawing); one += 1ULL)
        {
            unsigned int *const order = orders + ((unsigned long long)which[one] * stride);
            const unsigned int held = order[at];
            order[at] = order[draws[one]];
            order[draws[one]] = held;
        }
    }
    platform_cell_released(which, platform_place_g_program_owner);
    platform_cell_released(bounds, platform_place_g_program_owner);
    platform_cell_released(draws, platform_place_g_program_owner);
    return ok;
}

/**
 * @brief The order that random.Random(seed).shuffle gives a list of `length`, for every generator of the set.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in,out] set     The set [BORROWS].
 * @param[in]     length  The length of the list.
 * @param[out]    orders  The order of each generator, `length` places each [BORROWS].
 * @return                1 where every draw ran.
 */
static inline int twister_shuffle(SimResults *results, TwisterSet *set, unsigned int length, unsigned int *orders)
{
    unsigned int *const lengths =
        (unsigned int *)platform_cell_taken((size_t)((set->count + 1ULL) * 4ULL), platform_place_g_program_owner, 0);
    int ok = lengths != NULL;
    for (unsigned long long one = 0ULL; ok && (one < set->count); one += 1ULL)
    {
        lengths[one] = length;
    }
    ok = ok && twister_shuffle_each(results, set, lengths, length, orders);
    platform_cell_released(lengths, platform_place_g_program_owner);
    return ok;
}

#endif
