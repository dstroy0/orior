// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file game_record.h
 * @brief What the game theory programs share.
 *
 * @note They share a program on the record machine and its sums on the device. They share the exact integers of the
 *       host, with a square root and an interval that rounds to its digits.
 *
 * @note Each value on the device is a register of the record machine, and each value on the host is an exact integer
 *       of orior. An irrational value is an interval of exact integers at 2^-bits. A caller asks for more bits where
 *       a decision or a digit needs them.
 */
#ifndef GAME_RECORD_H
#define GAME_RECORD_H

#include "sim.h"

#include "../../includes/exact_types/integers/exact_record/exact_record.h"
#include "cycle.h"
#include "key_schedule.h"
#include "keymath.h"

/** @brief A program on the record machine: its key, its layout and its load on the device. */
typedef struct
{
    EngineRecordKey key;       /**< The imprint. */
    EngineRecordLayout layout; /**< The layout. */
    CycleRecord *record;       /**< The load on the device. */
} GameProgram;

/**
 * @brief Imprints, arranges and loads a built program.
 *
 * @param[in]  built    The program [BORROWS].
 * @param[in]  members  The members it reads.
 * @param[in]  outputs  The steps it writes [BORROWS].
 * @param[in]  count    How many.
 * @param[in]  in_limbs The limbs of a record of each member [BORROWS].
 * @param[out] program  The program [BORROWS].
 * @param[out] error    The error [BORROWS].
 * @return              1 where it loaded.
 */
static inline int game_load(const ExactRecordProgram *built, unsigned int members, const unsigned int *outputs,
                            unsigned int count, const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX],
                            GameProgram *program, EngineError *error)
{
    memset(program, 0, sizeof(*program));
    const KeymathRecordRequest encode = {
        built->steps, built->count,  built->field_bits,  built->fields, members, outputs,
        count,        built->tables, built->table_count, &program->key, error};
    const KeyScheduleRecordRequest layout = {
        &program->key, built->field_offset, built->fields, in_limbs, 1, &program->layout, error};
    return (built->failed == 0) && (keymath_record_encode(&encode) != KEYMATH_ERROR) &&
           (key_schedule_record_layout(&layout) != KEY_SCHEDULE_ERROR) &&
           (cycle_record_load(&program->layout, &program->record, error) != CYCLE_ERROR);
}

/** @brief Gives a program back. */
static inline void game_release(GameProgram *program)
{
    if (program->record != NULL)
    {
        cycle_record_release(program->record);
    }
    key_schedule_record_release(&program->layout);
    keymath_record_release(&program->key);
    memset(program, 0, sizeof(*program));
}

/**
 * @brief The sums of one output field over each run of records on the device.
 *
 * @param[in]  program The program that wrote the records [BORROWS].
 * @param[in]  step    The step of the field.
 * @param[in]  records The records on the device [BORROWS].
 * @param[in]  count   How many.
 * @param[in]  group   The records of a run.
 * @param[out] sums    One sum a run, two limbs each, a whole number of either sign [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where the device summed them.
 */
static inline int game_sum(const GameProgram *program, unsigned int step, const unsigned int *records,
                           unsigned long long count, unsigned long long group, unsigned int *sums, EngineError *error)
{
    const CycleRecordSumRequest request = {records,
                                           count,
                                           group,
                                           program->layout.out_limbs,
                                           program->layout.step_table[step].out_offset,
                                           program->layout.step_table[step].out_bits,
                                           2u,
                                           sums,
                                           error};
    return cycle_record_sum(&request) != CYCLE_ERROR;
}

/** @brief A sum of two limbs, as game_sum writes it, as a whole number of either sign. */
static inline long long game_sum_value(const unsigned int *sums, unsigned long long group)
{
    const unsigned long long low = sums[2ULL * group];
    const unsigned long long high = sums[(2ULL * group) + 1ULL];
    // the two limbs are the two's complement of a 64-bit whole number
    return (long long)((high << 32u) | low);
}

/** @brief An exact integer from a whole number of either sign. */
static inline void game_exact(OriorExactInteger *value, long long number)
{
    sim_exact_signed(value, number);
}

/** @brief The sum of two exact integers into a result; 0 where the width cannot hold it. */
static inline int game_add(const OriorExactInteger *left, const OriorExactInteger *right, OriorExactInteger *result)
{
    return orior_exact_add(left, right, result) == ORIOR_EXACT_OK;
}

/** @brief The difference of two exact integers into a result; 0 where the width cannot hold it. */
static inline int game_subtract(const OriorExactInteger *left, const OriorExactInteger *right,
                                OriorExactInteger *result)
{
    return orior_exact_subtract(left, right, result) == ORIOR_EXACT_OK;
}

/** @brief The product of two exact integers into a result; 0 where the width cannot hold it. */
static inline int game_multiply(const OriorExactInteger *left, const OriorExactInteger *right,
                                OriorExactInteger *result)
{
    return orior_exact_multiply(left, right, result) == ORIOR_EXACT_OK;
}

/**
 * @brief The floor or the ceiling of left / right, for a positive divisor.
 *
 * @param[in]  left    The dividend [BORROWS].
 * @param[in]  right   The divisor, above 0 [BORROWS].
 * @param[in]  ceiling 1 for the ceiling, 0 for the floor.
 * @param[out] result  The quotient [BORROWS].
 * @return             1 where the division is clean.
 */
static inline int game_divide(const OriorExactInteger *left, const OriorExactInteger *right, int ceiling,
                              OriorExactInteger *result)
{
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    if (orior_exact_divide(left, right, &quotient, &remainder) != ORIOR_EXACT_OK)
    {
        return 0;
    }
    OriorExactInteger one;
    game_exact(&one, 1);
    // The quotient rounds toward zero. A remainder below zero moves the floor down, and one above zero the ceiling up.
    if ((remainder.sign < 0) && !ceiling)
    {
        return game_subtract(&quotient, &one, result);
    }
    if ((remainder.sign > 0) && ceiling)
    {
        return game_add(&quotient, &one, result);
    }
    *result = quotient;
    return 1;
}

/** @brief A whole power of a whole number, as an exact integer. */
static inline int game_power(long long base, unsigned int exponent, OriorExactInteger *result)
{
    OriorExactInteger factor;
    game_exact(result, 1);
    game_exact(&factor, base);
    int ok = 1;
    for (unsigned int at = 0u; ok && (at < exponent); at += 1u)
    {
        ok = game_multiply(result, &factor, result);
    }
    return ok;
}

/** @brief The bits of a value that is not negative, 0 for 0. */
static inline unsigned int game_bits(const OriorExactInteger *value)
{
    unsigned int top = ORIOR_EXACT_LIMBS;
    while ((top > 0u) && (value->limb[top - 1u] == 0u))
    {
        top -= 1u;
    }
    unsigned int bits = 32u * top;
    while ((bits > 0u) && (((value->limb[(bits - 1u) / 32u] >> ((bits - 1u) % 32u)) & 1u) == 0u))
    {
        bits -= 1u;
    }
    return bits;
}

/**
 * @brief The whole square root of a value that is not negative, by the method of Newton on exact integers.
 *
 * @param[in]  value  The value [BORROWS].
 * @param[out] result The root: the largest r with r^2 at most the value [BORROWS].
 * @return            1 where it is clean.
 */
static inline int game_root(const OriorExactInteger *value, OriorExactInteger *result)
{
    if (value->sign <= 0)
    {
        orior_exact_zero(result);
        return value->sign == 0;
    }
    // The first guess is 2^ceil(bits / 2), at least the root. Each step of Newton from above stays at the root or
    // above.
    OriorExactInteger guess;
    int ok = game_power(2, (game_bits(value) + 1u) / 2u, &guess);
    OriorExactInteger two;
    game_exact(&two, 2);
    while (ok)
    {
        OriorExactInteger next;
        ok = game_divide(value, &guess, 0, &next) && game_add(&next, &guess, &next) &&
             game_divide(&next, &two, 0, &next);
        if (!ok || (orior_exact_compare(&next, &guess) >= 0))
        {
            break;
        }
        guess = next;
    }
    *result = guess;
    return ok;
}

/** @brief An interval of a real value at 2^-bits: its floor and its ceiling there. */
typedef struct
{
    OriorExactInteger low;  /**< At most the value, at 2^-bits. */
    OriorExactInteger high; /**< At least the value. */
} GameInterval;

/**
 * @brief The value times 10^places, rounded to the nearest whole number, from an interval at 2^-bits.
 *
 * @param[in]  value  The value [BORROWS].
 * @param[in]  bits   The scale of the interval.
 * @param[in]  places The decimals.
 * @param[out] scaled The rounded value [BORROWS].
 * @return            1 where both ends of the interval round alike.
 */
static inline int game_round(const GameInterval *value, unsigned int bits, unsigned int places, long long *scaled)
{
    OriorExactInteger ten_power;
    OriorExactInteger unit;
    OriorExactInteger half;
    int ok = game_power(10, places, &ten_power) && game_power(2, bits, &unit) && game_power(2, bits - 1u, &half);
    long long ends[2] = {0, 0};
    const OriorExactInteger *const bounds[2] = {&value->low, &value->high};
    for (int end = 0; ok && (end < 2); end += 1)
    {
        OriorExactInteger work;
        ok = game_multiply(bounds[end], &ten_power, &work) && game_add(&work, &half, &work) &&
             game_divide(&work, &unit, 0, &work);
        // the rounded value is a small whole number, in the low two limbs
        const unsigned long long magnitude = ((unsigned long long)work.limb[1] << 32u) | work.limb[0];
        ends[end] = (work.sign < 0) ? -(long long)magnitude : (long long)magnitude;
    }
    *scaled = ends[0];
    return ok && (ends[0] == ends[1]);
}

/** @brief Writes a rounded value, `scaled` over 10^places, with its decimals. */
static inline void game_put_decimal(VerbaLinea *line, long long scaled, unsigned int places)
{
    long long unit = 1;
    for (unsigned int place = 0u; place < places; place += 1u)
    {
        unit *= 10;
    }
    if (scaled < 0)
    {
        mmgr_line_ch(line, '-');
        scaled = -scaled;
    }
    // the rounded value is not negative past the sign
    mmgr_line_decimal(line, (unsigned long long)(scaled / unit), 1u);
    if (places > 0u)
    {
        mmgr_line_ch(line, '.');
        mmgr_line_decimal(line, (unsigned long long)(scaled % unit), places);
    }
}

#endif
