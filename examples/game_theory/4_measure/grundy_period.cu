// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: GAM-4-002
/**
 * @file grundy_period.cu
 * @brief The period detector on the Grundy sequence of each subtraction game, against the exact period of each row.
 *
 * @note The run is on the record machine, in exact integers.
 * @note Run bash examples/game_theory/build_game_theory.sh grundy_period [largest move] [max moves].
 * @note Every claim of reach in this work is empirical, over a set that nobody listed. A list of instances becomes a
 *       statement about a domain only through a controlled series inside it, with an outside answer on each row.
 * @note Impartial games remove the cost of the rows. A subtraction game with a finite move set has a Grundy sequence
 *       that is periodic past a start, and a proof shows it. An exact comparison computes the period. A row costs no
 *       data, and the answer of no row comes from the detector under test.
 * @note A record program steps each game one position a sweep: the mex of the values that its moves reach. A second
 *       program marks each pair a shift apart that differs past the bound. The sum of the marks of a game is 0 at its
 *       exact period. The detector reads the lags 1 to w of a window w, and scores each period to w / 2 against its own
 *       multiples. A period past half the window has one multiple inside it, and it is out of range.
 * @note The mark of a lag is n P / (N^2 sqrt(D_l D_r)), from five sums over the pairs of the lag. The device takes the
 *       sums. The host takes each mark as an interval at 2^-B by an integer square root. It doubles B until each
 *       decision and each printed digit is clear.
 * @note Each shuffle keeps every value of a row and moves the place that carries it, by the twister of CPython at the
 *       seeds 0 to 11. The tallest margin of the twelve is the floor of the row.
 */
#include "../game_record.h"

#include "../../0_experimental/exact_zeta/zeta_shuffle.h"

/** @brief The positions of each game. The longest period of the sweep still repeats many times past its start. */
#define GRUNDY_POSITIONS 2048u

/** @brief The windows that the run reads each row at, and the last lag of the widest. */
#define GRUNDY_WINDOW_COUNT 5u
static const unsigned int GRUNDY_WINDOWS[GRUNDY_WINDOW_COUNT] = {16u, 24u, 32u, 48u, 64u};
#define GRUNDY_LAGS 64u

/** @brief The shuffles of each row. */
#define GRUNDY_DRAWS 12u

/** @brief The series of one sweep of the lag program. */
#define GRUNDY_BATCH 48u

/** @brief The sums that the lag program writes for each pair: v v', v, v', v^2 and v'^2. */
#define GRUNDY_SUMS 5u

/** @brief The first precision of a mark, in bits, and the widest that a decision can ask for. */
#define GRUNDY_BITS_FIRST 64u
#define GRUNDY_BITS_LAST 1024u

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
static int grundy_make_step(unsigned int largest, unsigned int most, unsigned int width, GameProgram *program,
                            EngineError *error)
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
 * @brief The mark of a pair a shift apart: 1 where its two values differ.
 *
 * @param[in]  width      The bits of a value.
 * @param[in]  in_limbs_0 The limbs of a record of the sequence.
 * @param[out] program    The program [BORROWS].
 * @param[out] output     The step of the mark [BORROWS].
 * @param[out] error      The error [BORROWS].
 * @return                1 where it loaded.
 */
static int grundy_make_differ(unsigned int width, unsigned int in_limbs_0, GameProgram *program, unsigned int *output,
                              EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int field = exact_record_member_field(&built, width + 1u, 0u);
    const unsigned int here = exact_record_read_unsigned(&built, field, 0u);
    const unsigned int there = exact_record_read_unsigned(&built, field, 1u);
    const unsigned int differ =
        exact_record_difference(&built, exact_record_constant(&built, 1u), exact_record_equal(&built, here, there));
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {in_limbs_0, in_limbs_0, 0u};
    *output = differ;
    const int ok = game_load(&built, 2u, &differ, 1u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief The gather of a value: the value of a record of the sequence, written as a record of its own.
 *
 * @param[in]  width      The bits of a value.
 * @param[in]  in_limbs_0 The limbs of a record of the sequence.
 * @param[out] program    The program [BORROWS].
 * @param[out] error      The error [BORROWS].
 * @return                1 where it loaded.
 */
static int grundy_make_gather(unsigned int width, unsigned int in_limbs_0, GameProgram *program, EngineError *error)
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

/**
 * @brief The five terms of a pair a lag apart: v v', v, v', v^2 and v'^2.
 *
 * @param[in]  width   The bits of a value.
 * @param[out] program The program [BORROWS].
 * @param[out] steps   The step of each term [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 */
static int grundy_make_lag(unsigned int width, GameProgram *program, unsigned int steps[GRUNDY_SUMS],
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
    const int ok = game_load(&built, 2u, steps, GRUNDY_SUMS, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/* ---------------------------------------------------------------- the exact detector on the host */

/** @brief The sums of one series at one lag, as the device returned them. */
typedef struct
{
    long long term[GRUNDY_SUMS]; /**< v v', v, v', v^2 and v'^2, each summed over the pairs of the lag. */
} GrundyLagSums;

/**
 * @brief The mark of one lag at 2^-bits: n P / (N^2 sqrt(D_l D_r)), between a floor and a ceiling.
 *
 * @param[in]  sums The sums of the series at lags 0 to GRUNDY_LAGS [BORROWS].
 * @param[in]  lag  The lag.
 * @param[in]  bits The precision.
 * @param[out] mark The mark [BORROWS].
 * @return          1 where it is clean.
 * @note P = N^2 sum v v' - N S (sum v + sum v') + n S^2, with S the sum of the series. D = n sum v^2 - (sum v)^2 on
 *       each side, without the scale of the center. A side that does not vary gives a mark of 0.
 */
static int grundy_mark(const GrundyLagSums *sums, unsigned int lag, unsigned int bits, GameInterval *mark)
{
    const long long whole = GRUNDY_POSITIONS;
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
} GrundyReading;

/**
 * @brief The period detector on one series at one window.
 *
 * @param[in]  sums    The sums of the series at lags 0 to GRUNDY_LAGS [BORROWS].
 * @param[in]  longest The window, the last lag read.
 * @param[out] reading The reading [BORROWS].
 * @return             1 where every exact step is clean.
 * @note It reads the period whose own multiples agree most, and the margin of that period over the lags outside its
 *       family.
 * @note The score of a period p is the mean mark of its family, p, 2p and on. The score takes away the mean mark of
 *       every other lag. The highest score wins. Of equal scores the larger period wins, as the reverse sort of the
 * pairs orders them.
 * @note A series that does not vary reads no period.
 */
static int grundy_detect(const GrundyLagSums *sums, unsigned int longest, GrundyReading *reading)
{
    memset(reading, 0, sizeof(*reading));
    const long long whole = GRUNDY_POSITIONS;
    if ((whole * sums[0].term[3]) == (sums[0].term[1] * sums[0].term[1]))
    {
        reading->decided = 1;
        return 1;
    }
    static GameInterval marks[GRUNDY_LAGS + 1u];
    static GameInterval scores[GRUNDY_LAGS + 1u];
    static long long weights[GRUNDY_LAGS + 1u];
    for (unsigned int bits = GRUNDY_BITS_FIRST; bits <= GRUNDY_BITS_LAST; bits *= 2u)
    {
        int ok = 1;
        for (unsigned int lag = 1u; ok && (lag <= longest); lag += 1u)
        {
            ok = grundy_mark(sums, lag, bits, &marks[lag]);
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
        if (clear || ((bits * 2u) > GRUNDY_BITS_LAST))
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
static int grundy_interval_order(const void *left, const void *right)
{
    return orior_exact_compare(&((const GameInterval *)left)->low, &((const GameInterval *)right)->low);
}

/**
 * @brief The min, the median and the max of intervals.
 *
 * @note The median is the middle one, or the mean of the middle two, as statistics.median takes it.
 *
 * @param[in,out] values The intervals [BORROWS]. The call sorts them.
 * @param[in]     count  How many.
 * @param[out]    found  The min, the median and the max [BORROWS].
 * @return               1 where it is clean.
 */
static int grundy_spread(GameInterval *values, unsigned long long count, GameInterval found[3])
{
    qsort(values, (size_t)count, sizeof(GameInterval), grundy_interval_order);
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

/* ---------------------------------------------------------------- the sweep */

/** @brief The next combination of size k of 1 to n, least first, in place. 0 past the last. */
static int grundy_next(unsigned int *chosen, unsigned int size, unsigned int largest)
{
    for (unsigned int slot = size; slot > 0u; slot -= 1u)
    {
        if (chosen[slot - 1u] < (largest - (size - slot)))
        {
            chosen[slot - 1u] += 1u;
            for (unsigned int after = slot; after < size; after += 1u)
            {
                chosen[after] = chosen[after - 1u] + 1u;
            }
            return 1;
        }
    }
    return 0;
}

/** @brief The device state of the run. */
typedef struct
{
    unsigned int *games;   /**< The moves of each game, one limb a record. */
    unsigned int *places;  /**< The positions 0 to N - 1, one limb a record. */
    unsigned int *zero;    /**< One state of zeros. */
    unsigned int *history; /**< The state of each game after each position, position by position. */
    unsigned int *index;   /**< The index of a sweep. */
    unsigned int *out;     /**< The records of a sweep. */
    unsigned int *values;  /**< The value of each place of each series, one limb a record, a zero record last. */
} GrundyDevice;

/**
 * @brief Sends the index of a sweep to the device.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in]     device  The device state [BORROWS].
 * @param[in]     index   The index on the host [BORROWS].
 * @param[in]     entries How many entries.
 * @return                1 where it went.
 */
static int grundy_send_index(SimResults *results, const GrundyDevice *device, const unsigned int *index,
                             unsigned long long entries)
{
    return sim_status_check(results, cudaMemcpy(device->index, index, entries * 4ULL, cudaMemcpyHostToDevice),
                            "index sent");
}

/**
 * @brief Runs the sweep.
 *
 * @param[in] argc The count of arguments.
 * @param[in] argv The largest move and the max moves, both optional [BORROWS].
 * @return         0 where every check holds, else 1.
 */
int main(int argc, char **argv)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    VerbaLinea *const line = &results.line;
    // The arguments are small whole numbers.
    const unsigned int largest = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 10) : 10u;
    const unsigned int most = (argc > 2) ? (unsigned int)strtoul(argv[2], NULL, 10) : 4u;
    sim_check(&results, (largest >= 1u) && (largest <= 32u) && (most >= 1u) && (most <= largest),
              "the largest move and the max moves name a sweep");
    if (results.failures != 0ULL)
    {
        return sim_close(&results, "grundy period");
    }
    const unsigned int width = exact_record_bits_of(most);
    // the move sets, in the order of itertools.combinations, each as a mask of its moves
    unsigned int games = 0u;
    unsigned int *masks = NULL;
    for (unsigned int size = 1u; size <= most; size += 1u)
    {
        unsigned int chosen[32];
        for (unsigned int slot = 0u; slot < size; slot += 1u)
        {
            chosen[slot] = slot + 1u;
        }
        do
        {
            unsigned int *const grown = (unsigned int *)realloc(masks, (games + 1u) * sizeof(unsigned int));
            if (grown == NULL)
            {
                free(masks);
                return sim_close(&results, "grundy period");
            }
            masks = grown;
            masks[games] = 0u;
            for (unsigned int slot = 0u; slot < size; slot += 1u)
            {
                masks[games] |= 1u << (chosen[slot] - 1u);
            }
            games += 1u;
        } while (grundy_next(chosen, size, largest));
    }
    EngineError error;
    memset(&error, 0, sizeof(error));
    GameProgram step;
    GameProgram differ;
    GameProgram gather;
    GameProgram lag;
    unsigned int lag_steps[GRUNDY_SUMS];
    unsigned int differ_step = 0u;
    int ok = grundy_make_step(largest, most, width, &step, &error);
    const unsigned int state_limbs = step.layout.out_limbs;
    ok = ok && grundy_make_differ(width, state_limbs, &differ, &differ_step, &error) &&
         grundy_make_gather(width, state_limbs, &gather, &error) && grundy_make_lag(width, &lag, lag_steps, &error);
    sim_check(&results, ok, "the four record programs imprint, arrange and load");
    const unsigned long long whole = GRUNDY_POSITIONS;
    const unsigned long long history_records = (whole * games) + 1ULL;
    const unsigned long long series_max = games + ((unsigned long long)GRUNDY_DRAWS * games);
    const unsigned long long lag_lanes = GRUNDY_BATCH * (GRUNDY_LAGS + 1ULL) * whole;
    const unsigned long long sweep_lanes = (lag_lanes > (whole * games)) ? lag_lanes : (whole * games);
    const unsigned long long out_limbs_max = (lag.layout.out_limbs > 1u) ? lag.layout.out_limbs : 1u;
    const unsigned long long declared = (history_records * state_limbs * 4ULL) + (sweep_lanes * 3ULL * 4ULL) +
                                        (sweep_lanes * out_limbs_max * 4ULL) + (((series_max * whole) + 1ULL) * 4ULL) +
                                        (whole * 4ULL) + (games * 4ULL) + (state_limbs * 4ULL);
    ok = ok && sim_job_submit(&results, "grundy_period", argc, argv, declared);
    GrundyDevice device;
    memset(&device, 0, sizeof(device));
    ok =
        ok && sim_status_check(&results, cudaMalloc((void **)&device.games, games * 4ULL), "games") &&
        sim_status_check(&results, cudaMalloc((void **)&device.places, whole * 4ULL), "places") &&
        sim_status_check(&results, cudaMalloc((void **)&device.zero, state_limbs * 4ULL), "zero state") &&
        sim_status_check(&results, cudaMalloc((void **)&device.history, history_records * state_limbs * 4ULL),
                         "history") &&
        sim_status_check(&results, cudaMalloc((void **)&device.index, sweep_lanes * 3ULL * 4ULL), "index") &&
        sim_status_check(&results, cudaMalloc((void **)&device.out, sweep_lanes * out_limbs_max * 4ULL), "out") &&
        sim_status_check(&results, cudaMalloc((void **)&device.values, ((series_max * whole) + 1ULL) * 4ULL), "values");
    unsigned int *const host_index = (unsigned int *)malloc((size_t)(sweep_lanes * 3ULL * 4ULL));
    unsigned int *const places = (unsigned int *)malloc((size_t)(whole * 4ULL));
    ok = ok && (host_index != NULL) && (places != NULL);
    for (unsigned int place = 0u; ok && (place < GRUNDY_POSITIONS); place += 1u)
    {
        places[place] = place;
    }
    ok = ok &&
         sim_status_check(&results, cudaMemcpy(device.games, masks, games * 4ULL, cudaMemcpyHostToDevice),
                          "games sent") &&
         sim_status_check(&results, cudaMemcpy(device.places, places, whole * 4ULL, cudaMemcpyHostToDevice),
                          "places sent") &&
         sim_status_check(&results, cudaMemset(device.zero, 0, state_limbs * 4ULL), "zero state set") &&
         sim_status_check(&results, cudaMemset(device.history, 0, history_records * state_limbs * 4ULL),
                          "history cleared") &&
         sim_status_check(&results, cudaMemset(device.values, 0, ((series_max * whole) + 1ULL) * 4ULL),
                          "values cleared");
    // the Grundy step, one sweep a position: lane g reads its state, its game and the position. The first position
    // reads one state of zeros for every game.
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        host_index[(3u * game) + 0u] = 0u;
        host_index[(3u * game) + 1u] = game;
        host_index[(3u * game) + 2u] = 0u;
    }
    ok = ok && grundy_send_index(&results, &device, host_index, games * 3ULL);
    for (unsigned int place = 0u; ok && (place < GRUNDY_POSITIONS); place += 1u)
    {
        if (place == 1u)
        {
            for (unsigned int game = 0u; game < games; game += 1u)
            {
                host_index[3u * game] = game;
            }
            ok = grundy_send_index(&results, &device, host_index, games * 3ULL);
        }
        const unsigned int *const before =
            (place == 0u) ? device.zero : (device.history + ((unsigned long long)(place - 1u) * games * state_limbs));
        const CycleRecordRunRequest run = {step.record,
                                           {before, device.games, device.places + place},
                                           {(place == 0u) ? 1ULL : games, games, 1ULL},
                                           device.index,
                                           games,
                                           device.history + ((unsigned long long)place * games * state_limbs),
                                           &error};
        ok = ok && (cycle_record_run(&run) == (long)games);
    }
    sim_check(&results, ok, "the Grundy step sweeps every position of every game");
    // the exact period: the first shift where the pairs that differ past the bound sum to 0
    unsigned int *const period = (unsigned int *)calloc(games + 1u, sizeof(unsigned int));
    unsigned int *const open = (unsigned int *)malloc((games + 1u) * sizeof(unsigned int));
    unsigned int *const sums =
        (unsigned int *)malloc((size_t)((series_max + 1ULL) * (GRUNDY_LAGS + 1ULL) * 2ULL * 4ULL));
    ok = ok && (period != NULL) && (open != NULL) && (sums != NULL);
    unsigned int settled = 0u;
    for (unsigned int shift = 1u; ok && (settled < games) && (shift <= (GRUNDY_POSITIONS / 4u)); shift += 1u)
    {
        const unsigned int cap = GRUNDY_POSITIONS / 2u;
        const unsigned int room = GRUNDY_POSITIONS - (3u * shift);
        const unsigned int bound = (cap < room) ? cap : room;
        const unsigned int from = bound - 1u;
        const unsigned int to = GRUNDY_POSITIONS - shift;
        const unsigned int run_length = (to > from) ? (to - from) : 0u;
        unsigned int open_count = 0u;
        for (unsigned int game = 0u; game < games; game += 1u)
        {
            if (period[game] == 0u)
            {
                open[open_count] = game;
                open_count += 1u;
            }
        }
        if (run_length == 0u)
        {
            for (unsigned int at = 0u; at < open_count; at += 1u)
            {
                period[open[at]] = shift;
            }
            settled += open_count;
            continue;
        }
        unsigned long long lane = 0ULL;
        for (unsigned int at = 0u; at < open_count; at += 1u)
        {
            for (unsigned int place = from; place < to; place += 1u)
            {
                host_index[2ULL * lane] = (place * games) + open[at];
                host_index[(2ULL * lane) + 1ULL] = ((place + shift) * games) + open[at];
                lane += 1ULL;
            }
        }
        ok = grundy_send_index(&results, &device, host_index, lane * 2ULL);
        const CycleRecordRunRequest run = {differ.record,
                                           {device.history, device.history, NULL},
                                           {whole * games, whole * games, 0ULL},
                                           device.index,
                                           lane,
                                           device.out,
                                           &error};
        ok = ok && (cycle_record_run(&run) == (long)lane) &&
             game_sum(&differ, differ_step, device.out, lane, run_length, sums, &error);
        for (unsigned int at = 0u; ok && (at < open_count); at += 1u)
        {
            if ((sums[2u * at] == 0u) && (sums[(2u * at) + 1u] == 0u))
            {
                period[open[at]] = shift;
                settled += 1u;
            }
        }
    }
    sim_check(&results, ok, "the shifts sweep and sum on the device");
    unsigned char present[(GRUNDY_POSITIONS / 4u) + 1u];
    memset(present, 0, sizeof(present));
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        present[period[game]] = 1u;
    }
    if (ok)
    {
        mmgr_line_decimal(line, games, 1u);
        mmgr_line_put(line, " move sets, exact period found for ");
        mmgr_line_decimal(line, settled, 1u);
        mmgr_line_put(line, " of them\nperiods present: [");
        int first = 1;
        for (unsigned int shift = 1u; shift <= (GRUNDY_POSITIONS / 4u); shift += 1u)
        {
            if (present[shift] != 0u)
            {
                mmgr_line_put(line, first ? "" : ", ");
                mmgr_line_decimal(line, shift, 1u);
                first = 0;
            }
        }
        mmgr_line_put(line, "]\n\n");
        sim_flush(&results);
    }
    // the rows that the widest window reads, and their shuffles at the seeds 0 to 11 by the twister of CPython
    const unsigned int widest = GRUNDY_WINDOWS[GRUNDY_WINDOW_COUNT - 1u];
    unsigned int rows = 0u;
    unsigned int *const row_game = (unsigned int *)malloc((games + 1u) * sizeof(unsigned int));
    ok = ok && (row_game != NULL);
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        if ((period[game] != 0u) && (period[game] <= (widest / 2u)))
        {
            row_game[rows] = game;
            rows += 1u;
        }
    }
    // series 0 to games - 1 are the games themselves, then the 12 shuffles of each row, row by row
    const unsigned long long series = games + ((unsigned long long)rows * GRUNDY_DRAWS);
    unsigned int *const order = (unsigned int *)malloc(whole * sizeof(unsigned int));
    ok = ok && (order != NULL);
    unsigned long long written = 0ULL;
    for (unsigned long long one = 0ULL; ok && (one < series); one += 1ULL)
    {
        // the game of a series and the seed of a shuffle are small counts
        unsigned int game = (unsigned int)one;
        for (unsigned int place = 0u; place < GRUNDY_POSITIONS; place += 1u)
        {
            order[place] = place;
        }
        if (one >= games)
        {
            const unsigned long long rank = one - games;
            game = row_game[rank / GRUNDY_DRAWS];
            ZetaTwister twister;
            twister_seed(&twister, (uint32_t)(rank % GRUNDY_DRAWS));
            for (unsigned int place = GRUNDY_POSITIONS - 1u; place > 0u; place -= 1u)
            {
                const unsigned int other = twister_below(&twister, place + 1u);
                const unsigned int held_place = order[place];
                order[place] = order[other];
                order[other] = held_place;
            }
        }
        for (unsigned int place = 0u; place < GRUNDY_POSITIONS; place += 1u)
        {
            host_index[written] = (order[place] * games) + game;
            written += 1ULL;
        }
        // the gather writes a batch of series at a time
        if ((written >= lag_lanes) || ((one + 1ULL) == series))
        {
            const unsigned long long first_series = (one + 1ULL) - (written / whole);
            ok = grundy_send_index(&results, &device, host_index, written);
            const CycleRecordRunRequest run = {gather.record,
                                               {device.history, NULL, NULL},
                                               {whole * games, 0ULL, 0ULL},
                                               device.index,
                                               written,
                                               device.values + (first_series * whole),
                                               &error};
            ok = ok && (cycle_record_run(&run) == (long)written);
            written = 0ULL;
        }
    }
    sim_check(&results, ok, "the gather writes every series and shuffle");
    // the lag sums of every series at lags 0 to 64. A pair past the end reads the zero record at the end of the values
    GrundyLagSums *const lag_sums =
        (GrundyLagSums *)calloc((size_t)(series * (GRUNDY_LAGS + 1ULL)), sizeof(GrundyLagSums));
    ok = ok && (lag_sums != NULL);
    // the series are below 2^20 records apart, and the zero record fits a limb
    const unsigned int zero_record = (unsigned int)(series * whole);
    for (unsigned long long first_series = 0ULL; ok && (first_series < series); first_series += GRUNDY_BATCH)
    {
        const unsigned long long batch =
            ((series - first_series) < GRUNDY_BATCH) ? (series - first_series) : GRUNDY_BATCH;
        unsigned long long lane = 0ULL;
        for (unsigned long long one = 0ULL; one < batch; one += 1ULL)
        {
            // a series begins at a multiple of the positions, below the zero record
            const unsigned int base = (unsigned int)((first_series + one) * whole);
            for (unsigned int shift = 0u; shift <= GRUNDY_LAGS; shift += 1u)
            {
                for (unsigned int place = 0u; place < GRUNDY_POSITIONS; place += 1u)
                {
                    const int inside = (place + shift) < GRUNDY_POSITIONS;
                    host_index[2ULL * lane] = inside ? (base + place) : zero_record;
                    host_index[(2ULL * lane) + 1ULL] = inside ? (base + place + shift) : zero_record;
                    lane += 1ULL;
                }
            }
        }
        ok = grundy_send_index(&results, &device, host_index, lane * 2ULL);
        const CycleRecordRunRequest run = {lag.record,
                                           {device.values, device.values, NULL},
                                           {(series * whole) + 1ULL, (series * whole) + 1ULL, 0ULL},
                                           device.index,
                                           lane,
                                           device.out,
                                           &error};
        ok = ok && (cycle_record_run(&run) == (long)lane);
        for (unsigned int term = 0u; ok && (term < GRUNDY_SUMS); term += 1u)
        {
            ok = game_sum(&lag, lag_steps[term], device.out, lane, whole, sums, &error);
            for (unsigned long long group = 0ULL; ok && (group < (lane / whole)); group += 1ULL)
            {
                // each sum is below 2^63: its two limbs are a whole number that is not negative
                const unsigned long long low = sums[2ULL * group];
                const unsigned long long high = sums[(2ULL * group) + 1ULL];
                lag_sums[(first_series * (GRUNDY_LAGS + 1ULL)) + group].term[term] = (long long)((high << 32u) | low);
            }
        }
    }
    sim_check(&results, ok, "the lag program sweeps and sums every series on the device");
    // the detector at every window on every row in range, and its floor from the shuffles at the widest
    GrundyReading *const readings = (GrundyReading *)calloc((size_t)games * GRUNDY_WINDOW_COUNT, sizeof(GrundyReading));
    ok = ok && (readings != NULL);
    unsigned long long undecided = 0ULL;
    if (ok)
    {
        mmgr_line_put(line, "window   in range    exact out of range\n");
    }
    for (unsigned int window = 0u; ok && (window < GRUNDY_WINDOW_COUNT); window += 1u)
    {
        const unsigned int reach = GRUNDY_WINDOWS[window];
        unsigned int inside = 0u;
        unsigned int hit = 0u;
        for (unsigned int game = 0u; ok && (game < games); game += 1u)
        {
            if ((period[game] == 0u) || (period[game] > (reach / 2u)))
            {
                continue;
            }
            inside += 1u;
            GrundyReading *const reading = &readings[(game * GRUNDY_WINDOW_COUNT) + window];
            ok = grundy_detect(&lag_sums[(unsigned long long)game * (GRUNDY_LAGS + 1ULL)], reach, reading);
            undecided += (ok && !reading->decided) ? 1ULL : 0ULL;
            hit += (ok && (reading->period == period[game])) ? 1u : 0u;
        }
        char text[96];
        snprintf(text, sizeof(text), "%-8u %8u %8u %8u\n", reach, inside, hit, settled - inside);
        mmgr_line_put(line, text);
    }
    GameInterval *const live = (GameInterval *)calloc(rows + 1u, sizeof(GameInterval));
    GameInterval *const floors = (GameInterval *)calloc(rows + 1u, sizeof(GameInterval));
    GameInterval *const ratios = (GameInterval *)calloc(rows + 1u, sizeof(GameInterval));
    ok = ok && (live != NULL) && (floors != NULL) && (ratios != NULL);
    unsigned long long ratio_count = 0ULL;
    unsigned long long uncleared = 0ULL;
    unsigned long long unclear_orders = 0ULL;
    OriorExactInteger unit;
    ok = ok && game_power(2, 64u, &unit);
    for (unsigned int row = 0u; ok && (row < rows); row += 1u)
    {
        live[row] = readings[(row_game[row] * GRUNDY_WINDOW_COUNT) + (GRUNDY_WINDOW_COUNT - 1u)].margin;
        orior_exact_zero(&floors[row].low);
        orior_exact_zero(&floors[row].high);
        for (unsigned int draw = 0u; ok && (draw < GRUNDY_DRAWS); draw += 1u)
        {
            GrundyReading shuffled;
            const unsigned long long one = games + ((unsigned long long)row * GRUNDY_DRAWS) + draw;
            ok = grundy_detect(&lag_sums[one * (GRUNDY_LAGS + 1ULL)], widest, &shuffled);
            undecided += (ok && !shuffled.decided) ? 1ULL : 0ULL;
            if (ok && (shuffled.period != 0u))
            {
                if (orior_exact_compare(&shuffled.margin.low, &floors[row].low) > 0)
                {
                    floors[row].low = shuffled.margin.low;
                }
                if (orior_exact_compare(&shuffled.margin.high, &floors[row].high) > 0)
                {
                    floors[row].high = shuffled.margin.high;
                }
            }
        }
        // the floor is above 0 where its low end is, and an interval across 0 leaves the comparison unclear
        if (ok && (floors[row].low.sign > 0))
        {
            OriorExactInteger work;
            ok = game_multiply(&live[row].low, &unit, &work) &&
                 game_divide(&work, &floors[row].high, 0, &ratios[ratio_count].low) &&
                 game_multiply(&live[row].high, &unit, &work) &&
                 game_divide(&work, &floors[row].low, 1, &ratios[ratio_count].high);
            ratio_count += 1ULL;
        }
        else if (ok && (floors[row].high.sign > 0))
        {
            unclear_orders += 1ULL;
        }
        // the margin did not clear its floor where its ceiling is at most the low end of the floor. Where they overlap
        // the order is unclear
        if (ok && (orior_exact_compare(&live[row].high, &floors[row].low) <= 0))
        {
            uncleared += 1ULL;
        }
        else if (ok && (orior_exact_compare(&live[row].low, &floors[row].high) <= 0))
        {
            unclear_orders += 1ULL;
        }
    }
    unsigned long long unclear_digits = 0ULL;
    if (ok && (rows > 0u))
    {
        char text[96];
        snprintf(text, sizeof(text), "\nat window %u, %u rows\n", widest, rows);
        mmgr_line_put(line, text);
        GameInterval *const sets[3] = {live, floors, ratios};
        const unsigned long long counts[3] = {rows, rows, ratio_count};
        static const char *const LABELS[3] = {"  live margin   min ", "  shuffle floor min ", "  ratio         min "};
        static const char *const PARTS[3] = {"", "  median ", "  max "};
        for (int set = 0; ok && (set < 3); set += 1)
        {
            GameInterval found[3];
            ok = (counts[set] > 0ULL) && grundy_spread(sets[set], counts[set], found);
            const unsigned int decimals = (set == 2) ? 2u : 3u;
            mmgr_line_put(line, LABELS[set]);
            for (int part = 0; ok && (part < ((set == 2) ? 2 : 3)); part += 1)
            {
                long long scaled = 0;
                unclear_digits += game_round(&found[part], 64u, decimals, &scaled) ? 0ULL : 1ULL;
                mmgr_line_put(line, PARTS[part]);
                game_put_decimal(line, scaled, decimals);
            }
            mmgr_line_ch(line, '\n');
        }
        snprintf(text, sizeof(text), "  rows where the live margin did not clear its own floor: %llu\n", uncleared);
        mmgr_line_put(line, text);
    }
    // window invariance: a true period is one number at every window
    unsigned int short_rows = 0u;
    unsigned int stable = 0u;
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        if ((period[game] == 0u) || (period[game] > (GRUNDY_WINDOWS[0] / 2u)))
        {
            continue;
        }
        short_rows += 1u;
        int same = 1;
        for (unsigned int window = 0u; window < GRUNDY_WINDOW_COUNT; window += 1u)
        {
            same = same && (readings[(game * GRUNDY_WINDOW_COUNT) + window].period == period[game]);
        }
        stable += same ? 1u : 0u;
    }
    if (ok)
    {
        char text[192];
        snprintf(text, sizeof(text),
                 "\nrows with a period readable at every window: %u, of which the answer was the same number at all %u "
                 "windows: %u\n",
                 short_rows, GRUNDY_WINDOW_COUNT, stable);
        mmgr_line_put(line, text);
    }
    sim_check(&results, ok, "every row was computed and read");
    sim_check(&results, settled == games, "every move set has an exact period inside its sequence");
    sim_check(&results, undecided == 0ULL, "every reading is decided at the precision that it asked for");
    sim_check(&results, unclear_orders == 0ULL, "every margin and floor are in a clear order");
    sim_check(&results, unclear_digits == 0ULL, "every printed value rounds alike at both ends of its interval");
    sim_check(&results, stable == short_rows, "a true period is one number at every window");
    sim_flush(&results);
    cudaFree(device.games);
    cudaFree(device.places);
    cudaFree(device.zero);
    cudaFree(device.history);
    cudaFree(device.index);
    cudaFree(device.out);
    cudaFree(device.values);
    game_release(&step);
    game_release(&differ);
    game_release(&gather);
    game_release(&lag);
    cycle_record_sum_release();
    free(masks);
    free(host_index);
    free(places);
    free(period);
    free(open);
    free(sums);
    free(row_game);
    free(order);
    free(lag_sums);
    free(readings);
    free(live);
    free(floors);
    free(ratios);
    return sim_close(&results, "grundy period");
}
