// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file period_detector.h
 * @brief The period detector: the period whose own multiples agree most, scored against the lags outside its family.
 *
 * @note The mark of a lag is n P / (N^2 sqrt(D_l D_r)), from five sums over the pairs of the lag. A record program
 *       takes the terms of each pair on the device, and cycle_record_sum sums them for each series and lag.
 * @note The host takes each mark as an interval at 2^-B by an exact integer square root. It doubles B until each
 *       decision is clear. A decision that stays unclear at the widest B carries a flag in its reading.
 * @note The score of a period p is the mean mark of its family, p, 2p and on, to the window. The score takes away the
 *       mean mark of every other lag. The highest score wins. Of equal scores the larger period wins, as the reverse
 *       sort of the pairs of score and period orders them.
 */
#ifndef PERIOD_DETECTOR_H
#define PERIOD_DETECTOR_H

#include "game_record.h"

/** @brief The sums that the lag program writes for each pair: v v', v, v', v^2 and v'^2. */
#define DETECTOR_SUMS 5u

/** @brief The last lag that a reading can ask for. */
#define DETECTOR_LAGS_MAX 128u

/** @brief The first precision of a mark, in bits, and the widest that a decision can ask for. */
#define DETECTOR_BITS_FIRST 64u
#define DETECTOR_BITS_LAST 1024u

/** @brief The lanes of one sweep of the lag program, at most. */
#define DETECTOR_SWEEP_LANES 6400000ULL

/** @brief The sums of one series at one lag, as the device returned them. */
typedef struct
{
    long long term[DETECTOR_SUMS]; /**< v v', v, v', v^2 and v'^2, each summed over the pairs of the lag. */
} DetectorLagSums;

/**
 * @brief The five terms of a pair a lag apart: v v', v, v', v^2 and v'^2.
 *
 * @param[in]  width   The bits of a value. A value record holds it in its low width + 1 bits, one limb a record.
 * @param[out] program The program [BORROWS].
 * @param[out] steps   The step of each term [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 */
static inline int detector_make_lag(unsigned int width, GameProgram *program, unsigned int steps[DETECTOR_SUMS],
                                    EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int field = exact_record_member_field(&built, width + 1u, 0u);
    const unsigned int here = exact_record_read_unsigned(&built, field, 0u);
    const unsigned int there = exact_record_read_unsigned(&built, field, 1u);
    steps[0] = exact_record_product(&built, here, there);
    steps[1] = here;
    steps[2] = there;
    steps[3] = exact_record_product(&built, here, here);
    steps[4] = exact_record_product(&built, there, there);
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {1u, 1u, 0u};
    const int ok = game_load(&built, 2u, steps, DETECTOR_SUMS, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/** @brief The buffers of the lag sweep. */
typedef struct
{
    const GameProgram *program;   /**< The lag program [BORROWS]. */
    const unsigned int *steps;    /**< The step of each term [BORROWS]. */
    const unsigned int *values;   /**< The values of every series on the device, then one zero record [BORROWS]. */
    unsigned long long series;    /**< The series. */
    unsigned long long positions; /**< The values of a series. */
    unsigned int lags;            /**< The last lag. */
    unsigned int *host_index;     /**< The index on the host, DETECTOR_SWEEP_LANES * 2 entries [BORROWS]. */
    unsigned int *device_index;   /**< The index on the device, as large [BORROWS]. */
    unsigned int *device_out;     /**< The records of a sweep, DETECTOR_SWEEP_LANES of the program's [BORROWS]. */
    unsigned int *sums;           /**< One sum a run, two limbs each, for a sweep [BORROWS]. */
} DetectorSweep;

/**
 * @brief The lag sums of every series at lags 0 to `lags`, on the device.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in]     sweep   The buffers [BORROWS].
 * @param[out]    out     The sums, lags + 1 a series [BORROWS].
 * @return                1 where every sweep and sum ran.
 * @note A pair past the end of a series reads the zero record after the last series. Each run of the sum is one series
 *       at one lag.
 */
static inline int detector_sweep(SimResults *results, const DetectorSweep *sweep, DetectorLagSums *out)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    const unsigned long long per_series = (sweep->lags + 1ULL) * sweep->positions;
    const unsigned long long batch_max =
        (DETECTOR_SWEEP_LANES / per_series > 0ULL) ? (DETECTOR_SWEEP_LANES / per_series) : 1ULL;
    const unsigned long long records = (sweep->series * sweep->positions) + 1ULL;
    // the records of the values are below 2^32, and every place fits a limb
    const unsigned int zero_record = (unsigned int)(sweep->series * sweep->positions);
    int ok = 1;
    for (unsigned long long first = 0ULL; ok && (first < sweep->series); first += batch_max)
    {
        const unsigned long long batch = ((sweep->series - first) < batch_max) ? (sweep->series - first) : batch_max;
        unsigned long long lane = 0ULL;
        for (unsigned long long one = 0ULL; one < batch; one += 1ULL)
        {
            const unsigned long long base = (first + one) * sweep->positions;
            for (unsigned long long shift = 0ULL; shift <= sweep->lags; shift += 1ULL)
            {
                for (unsigned long long place = 0ULL; place < sweep->positions; place += 1ULL)
                {
                    const int inside = (place + shift) < sweep->positions;
                    // each place is below the zero record, a limb
                    sweep->host_index[2ULL * lane] = inside ? (unsigned int)(base + place) : zero_record;
                    sweep->host_index[(2ULL * lane) + 1ULL] =
                        inside ? (unsigned int)(base + place + shift) : zero_record;
                    lane += 1ULL;
                }
            }
        }
        ok = sim_status_check(
            results, cudaMemcpy(sweep->device_index, sweep->host_index, lane * 2ULL * 4ULL, cudaMemcpyHostToDevice),
            "lag index sent");
        const CycleRecordRunRequest run = {sweep->program->record,
                                           {sweep->values, sweep->values, NULL},
                                           {records, records, 0ULL},
                                           sweep->device_index,
                                           lane,
                                           sweep->device_out,
                                           &error};
        ok = ok && (cycle_record_run(&run) == (long)lane);
        for (unsigned int term = 0u; ok && (term < DETECTOR_SUMS); term += 1u)
        {
            ok = game_sum(sweep->program, sweep->steps[term], sweep->device_out, lane, sweep->positions, sweep->sums,
                          &error);
            for (unsigned long long group = 0ULL; ok && (group < (lane / sweep->positions)); group += 1ULL)
            {
                out[(first * (sweep->lags + 1ULL)) + group].term[term] = game_sum_value(sweep->sums, group);
            }
        }
    }
    return ok;
}

/**
 * @brief The mark of one lag at 2^-bits: n P / (N^2 sqrt(D_l D_r)), between a floor and a ceiling.
 *
 * @param[in]  sums      The sums of the series at lags 0 and on [BORROWS].
 * @param[in]  positions N, the values of the series.
 * @param[in]  lag       The lag.
 * @param[in]  bits      The precision.
 * @param[out] mark      The mark [BORROWS].
 * @return               1 where it is clean.
 * @note P = N^2 sum v v' - N S (sum v + sum v') + n S^2, with S the sum of the series and n = N - lag. D is
 *       n sum v^2 - (sum v)^2 on each side, without the scale of the center. A side that does not vary gives 0.
 */
static inline int detector_mark(const DetectorLagSums *sums, unsigned long long positions, unsigned int lag,
                                unsigned int bits, GameInterval *mark)
{
    // the positions are a small count
    const long long whole = (long long)positions;
    const long long pairs = whole - (long long)lag;
    OriorExactInteger total;
    OriorExactInteger product;
    OriorExactInteger left;
    OriorExactInteger right;
    OriorExactInteger left_square;
    OriorExactInteger right_square;
    game_exact(&total, sums[0].term[1]);
    game_exact(&product, sums[lag].term[0]);
    game_exact(&left, sums[lag].term[1]);
    game_exact(&right, sums[lag].term[2]);
    game_exact(&left_square, sums[lag].term[3]);
    game_exact(&right_square, sums[lag].term[4]);
    OriorExactInteger length;
    OriorExactInteger count;
    game_exact(&length, whole);
    game_exact(&count, pairs);
    OriorExactInteger scale;
    int ok = game_multiply(&length, &length, &scale);
    OriorExactInteger p_value;
    OriorExactInteger work;
    OriorExactInteger side;
    ok = ok && game_multiply(&scale, &product, &p_value) && game_add(&left, &right, &side) &&
         game_multiply(&side, &total, &side) && game_multiply(&side, &length, &side) &&
         game_subtract(&p_value, &side, &p_value) && game_multiply(&total, &total, &work) &&
         game_multiply(&work, &count, &work) && game_add(&p_value, &work, &p_value);
    OriorExactInteger left_spread;
    OriorExactInteger right_spread;
    ok = ok && game_multiply(&count, &left_square, &left_spread) && game_multiply(&left, &left, &work) &&
         game_subtract(&left_spread, &work, &left_spread) && game_multiply(&count, &right_square, &right_spread) &&
         game_multiply(&right, &right, &work) && game_subtract(&right_spread, &work, &right_spread);
    if (!ok)
    {
        return 0;
    }
    if ((left_spread.sign <= 0) || (right_spread.sign <= 0) || (p_value.sign == 0))
    {
        orior_exact_zero(&mark->low);
        orior_exact_zero(&mark->high);
        return 1;
    }
    OriorExactInteger spread;
    OriorExactInteger power;
    OriorExactInteger root;
    ok = game_multiply(&left_spread, &right_spread, &spread) && game_power(2, 2u * bits, &power) &&
         game_multiply(&spread, &power, &spread) && game_root(&spread, &root);
    // sqrt(D_l D_r) 2^bits lies in [root, root + 1), and it is root where the square is exact
    OriorExactInteger square;
    ok = ok && game_multiply(&root, &root, &square);
    const int exact = ok && (orior_exact_compare(&square, &spread) == 0);
    OriorExactInteger numerator;
    OriorExactInteger shift;
    ok = ok && game_power(2, bits, &shift) && game_multiply(&count, &p_value, &numerator) &&
         game_multiply(&numerator, &shift, &numerator) && game_multiply(&numerator, &shift, &numerator);
    OriorExactInteger one;
    game_exact(&one, 1);
    OriorExactInteger small_root;
    OriorExactInteger large_root;
    ok = ok && game_multiply(&scale, &root, &small_root);
    if (exact)
    {
        large_root = small_root;
    }
    else
    {
        OriorExactInteger root_up;
        ok = ok && game_add(&root, &one, &root_up) && game_multiply(&scale, &root_up, &large_root);
    }
    // the magnitude over the larger root is the smaller bound, and over the smaller root the larger one
    const int negative = numerator.sign < 0;
    numerator.sign = 1;
    OriorExactInteger lower;
    OriorExactInteger upper;
    ok = ok && game_divide(&numerator, &large_root, 0, &lower) && game_divide(&numerator, &small_root, 1, &upper);
    if (ok && negative)
    {
        mark->low = upper;
        mark->low.sign = (upper.sign == 0) ? 0 : -1;
        mark->high = lower;
        mark->high.sign = (lower.sign == 0) ? 0 : -1;
    }
    else if (ok)
    {
        mark->low = lower;
        mark->high = upper;
    }
    return ok;
}

/** @brief The reading of the detector on one series at one window. */
typedef struct
{
    unsigned int period; /**< The period it reads, or 0 for none. */
    int decided;         /**< 1 where the order of the scores is clear at the precision that it reached. */
    GameInterval margin; /**< The margin of the period over the lags outside its family, at 2^-64. */
} DetectorReading;

/**
 * @brief The period detector on one series at one window.
 *
 * @param[in]  sums      The sums of the series at lags 0 and on [BORROWS].
 * @param[in]  positions N, the values of the series.
 * @param[in]  longest   The window, the last lag read.
 * @param[out] reading   The reading [BORROWS].
 * @return               1 where every exact step is clean.
 * @note A series that does not vary reads no period. So does a series shorter than four windows.
 */
static inline int detector_detect(const DetectorLagSums *sums, unsigned long long positions, unsigned int longest,
                                  DetectorReading *reading)
{
    memset(reading, 0, sizeof(*reading));
    // the positions are a small count
    const long long whole = (long long)positions;
    if (((whole * sums[0].term[3]) == (sums[0].term[1] * sums[0].term[1])) || (positions < (4ULL * longest)))
    {
        reading->decided = 1;
        return 1;
    }
    static GameInterval marks[DETECTOR_LAGS_MAX + 1u];
    static GameInterval scores[DETECTOR_LAGS_MAX + 1u];
    static long long weights[DETECTOR_LAGS_MAX + 1u];
    for (unsigned int bits = DETECTOR_BITS_FIRST; bits <= DETECTOR_BITS_LAST; bits *= 2u)
    {
        int ok = 1;
        for (unsigned int lag = 1u; ok && (lag <= longest); lag += 1u)
        {
            ok = detector_mark(sums, positions, lag, bits, &marks[lag]);
        }
        for (unsigned int period = 2u; ok && (period <= (longest / 2u)); period += 1u)
        {
            OriorExactInteger family_low;
            OriorExactInteger family_high;
            OriorExactInteger outside_low;
            OriorExactInteger outside_high;
            orior_exact_zero(&family_low);
            orior_exact_zero(&family_high);
            orior_exact_zero(&outside_low);
            orior_exact_zero(&outside_high);
            long long family = 0;
            for (unsigned int lag = 1u; ok && (lag <= longest); lag += 1u)
            {
                const int inside = (lag % period) == 0u;
                OriorExactInteger *const low = inside ? &family_low : &outside_low;
                OriorExactInteger *const high = inside ? &family_high : &outside_high;
                family += inside ? 1 : 0;
                ok = game_add(low, &marks[lag].low, low) && game_add(high, &marks[lag].high, high);
            }
            const long long outside = (long long)longest - family;
            OriorExactInteger family_count;
            OriorExactInteger outside_count;
            game_exact(&family_count, family);
            game_exact(&outside_count, outside);
            OriorExactInteger first;
            OriorExactInteger second;
            // the score times family x outside: outside sum(family) - family sum(outside), low and high
            ok = ok && game_multiply(&outside_count, &family_low, &first) &&
                 game_multiply(&family_count, &outside_high, &second) &&
                 game_subtract(&first, &second, &scores[period].low) &&
                 game_multiply(&outside_count, &family_high, &first) &&
                 game_multiply(&family_count, &outside_low, &second) &&
                 game_subtract(&first, &second, &scores[period].high);
            weights[period] = family * outside;
        }
        if (!ok)
        {
            return 0;
        }
        // the leader by its high bound, the larger period on a tie, then a check that it beats every other clearly
        unsigned int best = 2u;
        for (unsigned int period = 3u; ok && (period <= (longest / 2u)); period += 1u)
        {
            OriorExactInteger one;
            OriorExactInteger other;
            OriorExactInteger weight;
            game_exact(&weight, weights[best]);
            ok = game_multiply(&scores[period].high, &weight, &one);
            game_exact(&weight, weights[period]);
            ok = ok && game_multiply(&scores[best].high, &weight, &other);
            if (ok && (orior_exact_compare(&one, &other) >= 0))
            {
                best = period;
            }
        }
        int clear = ok;
        for (unsigned int period = 2u; clear && (period <= (longest / 2u)); period += 1u)
        {
            if (period == best)
            {
                continue;
            }
            OriorExactInteger winner;
            OriorExactInteger rival;
            OriorExactInteger weight;
            game_exact(&weight, weights[period]);
            ok = game_multiply(&scores[best].low, &weight, &winner);
            game_exact(&weight, weights[best]);
            ok = ok && game_multiply(&scores[period].high, &weight, &rival);
            const int order = orior_exact_compare(&winner, &rival);
            clear = ok && ((order > 0) || ((order == 0) && (period < best)));
        }
        if (!ok)
        {
            return 0;
        }
        if (clear || ((bits * 2u) > DETECTOR_BITS_LAST))
        {
            reading->period = best;
            reading->decided = clear;
            // the margin at 2^-64: the score over its weight, from 2^-bits
            OriorExactInteger weight;
            OriorExactInteger shift;
            game_exact(&weight, weights[best]);
            return game_power(2, bits - 64u, &shift) && game_multiply(&weight, &shift, &weight) &&
                   game_divide(&scores[best].low, &weight, 0, &reading->margin.low) &&
                   game_divide(&scores[best].high, &weight, 1, &reading->margin.high);
        }
    }
    return 0;
}

/** @brief The order of two intervals by their floors, for qsort. */
static inline int detector_interval_order(const void *left, const void *right)
{
    return orior_exact_compare(&((const GameInterval *)left)->low, &((const GameInterval *)right)->low);
}

/**
 * @brief The min, the median and the max of intervals.
 *
 * @param[in,out] values The intervals [BORROWS]. The call sorts them.
 * @param[in]     count  How many.
 * @param[out]    found  The min, the median and the max [BORROWS].
 * @return               1 where it is clean.
 * @note The median is the middle one, or the mean of the middle two, as statistics.median takes it.
 */
static inline int detector_spread(GameInterval *values, unsigned long long count, GameInterval found[3])
{
    qsort(values, (size_t)count, sizeof(GameInterval), detector_interval_order);
    found[0] = values[0];
    found[2] = values[count - 1ULL];
    if ((count % 2ULL) == 1ULL)
    {
        found[1] = values[count / 2ULL];
        return 1;
    }
    OriorExactInteger two;
    game_exact(&two, 2);
    const GameInterval *const low = &values[(count / 2ULL) - 1ULL];
    const GameInterval *const high = &values[count / 2ULL];
    return game_add(&low->low, &high->low, &found[1].low) && game_divide(&found[1].low, &two, 0, &found[1].low) &&
           game_add(&low->high, &high->high, &found[1].high) && game_divide(&found[1].high, &two, 1, &found[1].high);
}

#endif
