// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: GAM-6-006
/**
 * @file sturmian_convergents.cu
 * @brief What the period detector returns on a sequence that has no period, and why the number it returns is not
 *        arbitrary.
 *
 * @note Run bash examples/game_theory/build_game_theory.sh sturmian_convergents.
 * @note The gaps between successive terms of a Beatty sequence of irrational slope form a Sturmian word. The word has
 *       no period: that follows from the slope, and no measure is in it. The positions of Wythoff that lose are the
 * case phi.
 * @note On that word the detector returns 8, then 13, then 21, then 34, then 55, as the window grows. These are the
 *       denominators of the convergents of the continued fraction of phi. A rotation by an irrational agrees with
 * itself at the denominator of a good rational approximation of that irrational. The detector reads that agreement
 *       right, and only the name period on its output is false.
 * @note The run tests the mechanism on nine slopes at seven windows each. A reported number is a hit where it is a
 *       denominator of a convergent of the slope, or of a fraction between two convergents.
 * @note A true period is one number at every window. No slope here gives one answer across the seven windows, and
 *       115 of 115 subtraction games with a real period do.
 * @note Each slope is an interval of exact integers at 2^-61. The square roots come from the exact integer root, e and
 *       ln 2 from their series, and pi from the formula of Machin. The cube root of 2 comes from an exact integer
 *       cube root. A record program writes each word on the device from both ends of the interval. The run checks
 *       that the ends agree at every term.
 */
#include "../../../includes/platform/platform.h"
#include "../period_detector.h"
#include "../twister_record.h"
#include "memoria_operor/memoria_operor.h"

#include "../../0_experimental/exact_zeta/zeta_shuffle.h"

/** @brief The terms of each word. The widest window still has four full candidate periods. */
#define STURMIAN_TERMS 4000u

/** @brief The windows read. The answer moves as the window grows, and a single window would hide it. */
#define STURMIAN_WINDOW_COUNT 7u
static const unsigned int STURMIAN_WINDOWS[STURMIAN_WINDOW_COUNT] = {16u, 24u, 32u, 48u, 64u, 96u, 128u};

/** @brief The shuffles of each word. */
#define STURMIAN_DRAWS 12u

/** @brief The slopes. */
#define STURMIAN_SLOPES 9u

/** @brief The scale of a slope, 2^-61, and the guard bits of its series past that scale. */
#define STURMIAN_BITS 61u
#define STURMIAN_GUARD 32u

/** @brief The bits of a gap. Every slope here is below 4, and a gap is at most 4. */
#define STURMIAN_WIDTH 3u

/** @brief The depth of the continued fraction that names the denominators. */
#define STURMIAN_DEPTH 10u

/** @brief The names of the slopes, in the order of the report. */
static const char *const STURMIAN_NAMES[STURMIAN_SLOPES] = {"phi",   "sqrt2", "e",     "pi",     "sqrt3",
                                                            "sqrt5", "sqrt7", "ln2+1", "cbrt2+1"};

/**
 * @brief An interval [low, high] at 2^-bits from a value at 2^-(bits + guard) and its bound on the error there.
 *
 * @param[in]  sum   The value at the finer scale [BORROWS].
 * @param[in]  error The bound on its error there.
 * @param[out] slope The interval [BORROWS].
 * @return           1 where it is clean.
 */
static int sturmian_from_series(const OriorExactInteger *sum, long long error, GameInterval *slope)
{
    OriorExactInteger bound;
    OriorExactInteger guard;
    OriorExactInteger low;
    OriorExactInteger high;
    game_exact(&bound, error);
    return game_power(2, STURMIAN_GUARD, &guard) && game_subtract(sum, &bound, &low) && game_add(sum, &bound, &high) &&
           game_divide(&low, &guard, 0, &slope->low) && game_divide(&high, &guard, 1, &slope->high);
}

/** @brief sqrt(n) at 2^-bits: the root of n 4^bits and one more, since n is not a square here. */
static int sturmian_root(long long number, GameInterval *slope)
{
    OriorExactInteger value;
    OriorExactInteger scale;
    OriorExactInteger one;
    game_exact(&value, number);
    game_exact(&one, 1);
    return game_power(4, STURMIAN_BITS, &scale) && game_multiply(&value, &scale, &value) &&
           game_root(&value, &slope->low) && game_add(&slope->low, &one, &slope->high);
}

/** @brief e = the sum of 1 / k! at 2^-bits. Each term is a floor, and the tail past k = 40 is below one unit. */
static int sturmian_e(GameInterval *slope)
{
    OriorExactInteger unit;
    OriorExactInteger sum;
    OriorExactInteger factorial;
    orior_exact_zero(&sum);
    game_exact(&factorial, 1);
    int ok = game_power(2, STURMIAN_BITS + STURMIAN_GUARD, &unit);
    const long long terms = 40;
    for (long long term = 0; ok && (term <= terms); term += 1)
    {
        OriorExactInteger count;
        OriorExactInteger part;
        game_exact(&count, (term == 0) ? 1 : term);
        ok = game_multiply(&factorial, &count, &factorial) && game_divide(&unit, &factorial, 0, &part) &&
             game_add(&sum, &part, &sum);
    }
    return ok && sturmian_from_series(&sum, terms + 2, slope);
}

/**
 * @brief atan(1 / x) at the finer scale, by its series of terms of alternate sign, each term a floor.
 *
 * @param[in]  base  x.
 * @param[out] sum   The sum [BORROWS].
 * @param[out] terms The count of terms that it took, each wrong by less than one unit [BORROWS].
 * @return           1 where it is clean.
 */
static int sturmian_arctan(long long base, OriorExactInteger *sum, long long *terms)
{
    OriorExactInteger unit;
    OriorExactInteger power;
    OriorExactInteger square;
    orior_exact_zero(sum);
    game_exact(&power, base);
    game_exact(&square, base * base);
    int ok = game_power(2, STURMIAN_BITS + STURMIAN_GUARD, &unit);
    *terms = 0;
    for (long long term = 0; ok; term += 1)
    {
        OriorExactInteger divisor;
        OriorExactInteger part;
        OriorExactInteger odd;
        game_exact(&odd, (2 * term) + 1);
        ok = game_multiply(&power, &odd, &divisor) && game_divide(&unit, &divisor, 0, &part);
        if (!ok || (part.sign == 0))
        {
            break;
        }
        ok = ((term % 2) == 0) ? game_add(sum, &part, sum) : game_subtract(sum, &part, sum);
        ok = ok && game_multiply(&power, &square, &power);
        *terms += 1;
    }
    return ok;
}

/** @brief pi = 16 atan(1/5) - 4 atan(1/239), at 2^-bits. */
static int sturmian_pi(GameInterval *slope)
{
    OriorExactInteger fifth;
    OriorExactInteger large;
    long long fifth_terms = 0;
    long long large_terms = 0;
    OriorExactInteger sixteen;
    OriorExactInteger four;
    game_exact(&sixteen, 16);
    game_exact(&four, 4);
    OriorExactInteger sum;
    // each series is wrong by less than its count of terms and the first term omitted, one unit each
    return sturmian_arctan(5, &fifth, &fifth_terms) && sturmian_arctan(239, &large, &large_terms) &&
           game_multiply(&fifth, &sixteen, &fifth) && game_multiply(&large, &four, &large) &&
           game_subtract(&fifth, &large, &sum) &&
           sturmian_from_series(&sum, (16 * (fifth_terms + 1)) + (4 * (large_terms + 1)), slope);
}

/** @brief ln 2 + 1, from ln 2 = the sum of 1 / (k 2^k), at 2^-bits. Each term is a floor, and the tail is below 2. */
static int sturmian_log_two(GameInterval *slope)
{
    OriorExactInteger unit;
    OriorExactInteger sum;
    orior_exact_zero(&sum);
    int ok = game_power(2, STURMIAN_BITS + STURMIAN_GUARD, &unit);
    long long terms = 0;
    for (long long term = 1; ok; term += 1)
    {
        OriorExactInteger divisor;
        OriorExactInteger part;
        OriorExactInteger count;
        game_exact(&count, term);
        ok = game_power(2, (unsigned int)term, &divisor) && game_multiply(&divisor, &count, &divisor) &&
             game_divide(&unit, &divisor, 0, &part);
        if (!ok || (part.sign == 0))
        {
            break;
        }
        ok = game_add(&sum, &part, &sum);
        terms += 1;
    }
    return ok && game_add(&sum, &unit, &sum) && sturmian_from_series(&sum, terms + 2, slope);
}

/** @brief The cube root of 2, plus 1, at 2^-bits: the largest r with r^3 at most 2 8^bits, by bisection. */
static int sturmian_cube_root(GameInterval *slope)
{
    OriorExactInteger target;
    OriorExactInteger two;
    game_exact(&two, 2);
    int ok = game_power(8, STURMIAN_BITS, &target) && game_multiply(&target, &two, &target);
    OriorExactInteger low;
    OriorExactInteger high;
    OriorExactInteger one;
    game_exact(&one, 1);
    // the root lies in [2^bits, 2^(bits + 1))
    ok = ok && game_power(2, STURMIAN_BITS, &low) && game_power(2, STURMIAN_BITS + 1u, &high);
    while (ok)
    {
        OriorExactInteger gap;
        ok = game_subtract(&high, &low, &gap);
        if (!ok || (orior_exact_compare(&gap, &one) <= 0))
        {
            break;
        }
        OriorExactInteger middle;
        OriorExactInteger cube;
        ok = game_add(&low, &high, &middle) && game_divide(&middle, &two, 0, &middle) &&
             game_multiply(&middle, &middle, &cube) && game_multiply(&cube, &middle, &cube);
        if (ok && (orior_exact_compare(&cube, &target) <= 0))
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    OriorExactInteger unit;
    ok = ok && game_power(2, STURMIAN_BITS, &unit) && game_add(&low, &unit, &slope->low) &&
         game_add(&slope->low, &one, &slope->high);
    return ok;
}

/** @brief Each slope at 2^-bits, in the order of STURMIAN_NAMES. */
static int sturmian_slopes(GameInterval slopes[STURMIAN_SLOPES])
{
    GameInterval root_five;
    OriorExactInteger unit;
    OriorExactInteger two;
    OriorExactInteger one;
    game_exact(&two, 2);
    game_exact(&one, 1);
    int ok = sturmian_root(5, &root_five) && game_power(2, STURMIAN_BITS, &unit);
    // phi = (1 + sqrt 5) / 2
    OriorExactInteger work;
    ok = ok && game_add(&unit, &root_five.low, &work) && game_divide(&work, &two, 0, &slopes[0].low) &&
         game_add(&unit, &root_five.high, &work) && game_divide(&work, &two, 1, &slopes[0].high);
    ok = ok && sturmian_root(2, &slopes[1]) && sturmian_e(&slopes[2]) && sturmian_pi(&slopes[3]) &&
         sturmian_root(3, &slopes[4]) && sturmian_root(5, &slopes[5]) && sturmian_root(7, &slopes[6]) &&
         sturmian_log_two(&slopes[7]) && sturmian_cube_root(&slopes[8]);
    return ok;
}

/** @brief A value below 2^64, from the low two limbs of an exact integer that is not negative. */
static unsigned long long sturmian_word(const OriorExactInteger *value)
{
    return ((unsigned long long)value->limb[1] << 32u) | value->limb[0];
}

/** @brief A set of whole numbers, as a list of its members. */
typedef struct
{
    long long values[512]; /**< The members. */
    unsigned int count;    /**< How many. */
    int complete;          /**< 0 where the interval of the slope could not decide a term of the fraction. */
} SturmianSet;

/** @brief Adds a member to the set. */
static void sturmian_add(SturmianSet *set, long long value)
{
    for (unsigned int at = 0u; at < set->count; at += 1u)
    {
        if (set->values[at] == value)
        {
            return;
        }
    }
    if (set->count < 512u)
    {
        set->values[set->count] = value;
        set->count += 1u;
    }
}

/** @brief Whether the set holds a value. */
static int sturmian_has(const SturmianSet *set, long long value)
{
    for (unsigned int at = 0u; at < set->count; at += 1u)
    {
        if (set->values[at] == value)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief The denominators of the convergents of a slope, and of the fractions between each two of them.
 *
 * @param[in]  slope The slope at 2^-bits [BORROWS].
 * @param[out] set   The denominators [BORROWS].
 * @return           1 where it is clean.
 * @note The fractions between two convergents are best approximations from one side. A detector that reads agreement
 *       has no reason to prefer the convergents on both sides.
 * @note The rest after each term is an interval of fractions. A term is the floor of the rest, and the call takes it
 *       only where both ends of the interval give it.
 */
static int sturmian_denominators(const GameInterval *slope, SturmianSet *set)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = set, .bytes = sizeof(*set), .val = 0);
    set->complete = 1;
    OriorExactInteger low_top = slope->low;
    OriorExactInteger high_top = slope->high;
    OriorExactInteger low_bottom;
    OriorExactInteger high_bottom;
    int ok = game_power(2, STURMIAN_BITS, &low_bottom);
    high_bottom = low_bottom;
    long long lower = 0;
    long long previous = 1;
    for (unsigned int depth = 0u; ok && (depth < STURMIAN_DEPTH); depth += 1u)
    {
        OriorExactInteger low_whole;
        OriorExactInteger high_whole;
        ok = game_divide(&low_top, &low_bottom, 0, &low_whole) && game_divide(&high_top, &high_bottom, 0, &high_whole);
        if (!ok || (orior_exact_compare(&low_whole, &high_whole) != 0))
        {
            set->complete = 0;
            break;
        }
        // a term of the fraction is a small whole number
        const long long whole = (long long)sturmian_word(&low_whole);
        for (long long step = 1; step <= whole; step += 1)
        {
            sturmian_add(set, (step * lower) + previous);
        }
        const long long next = (whole * lower) + previous;
        previous = lower;
        lower = next;
        sturmian_add(set, lower);
        // the rest less its term, then its reciprocal: [low, high] - w becomes [1 / (high - w), 1 / (low - w)]
        OriorExactInteger low_part;
        OriorExactInteger high_part;
        OriorExactInteger shift;
        ok = game_multiply(&low_whole, &low_bottom, &shift) && game_subtract(&low_top, &shift, &low_part) &&
             game_multiply(&high_whole, &high_bottom, &shift) && game_subtract(&high_top, &shift, &high_part);
        if (!ok || (low_part.sign <= 0))
        {
            set->complete = (low_part.sign > 0);
            break;
        }
        OriorExactInteger swap_bottom = low_bottom;
        low_top = high_bottom;
        low_bottom = high_part;
        high_top = swap_bottom;
        high_bottom = low_part;
    }
    return ok;
}

/**
 * @brief The word of each slope: lane i of a slope gives floor((i + 1) x) - floor(i x) at both ends of its interval.
 *
 * @param[out] program The program [BORROWS].
 * @param[out] steps   The steps of the gap at the low end and at the high end [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 * @note Member 0 is the slope of the lane, the low end in bits 0 to 63 and the high end in bits 64 to 127, at 2^-61.
 *       The term of a lane is its own number modulo the terms.
 */
static int sturmian_make_word(GameProgram *program, unsigned int steps[2], EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int ends[2] = {exact_record_read_unsigned(&built, exact_record_member_field(&built, 64u, 0u), 0u),
                                  exact_record_read_unsigned(&built, exact_record_member_field(&built, 64u, 64u), 0u)};
    const unsigned int lane = exact_record_lane_below(&built, 32u);
    const unsigned int term = exact_record_remainder(&built, lane, exact_record_constant(&built, STURMIAN_TERMS));
    const unsigned int next = exact_record_sum(&built, term, exact_record_constant(&built, 1u));
    const unsigned int scale = exact_record_power_two(&built, STURMIAN_BITS);
    for (unsigned int end = 0u; end < 2u; end += 1u)
    {
        const unsigned int above = exact_record_quotient(&built, exact_record_product(&built, next, ends[end]), scale);
        const unsigned int below = exact_record_quotient(&built, exact_record_product(&built, term, ends[end]), scale);
        steps[end] = exact_record_narrow(&built, exact_record_difference(&built, above, below), STURMIAN_WIDTH);
    }
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {4u, 0u, 0u};
    const int ok = game_load(&built, 1u, steps, 2u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief The gather of a gap: the gap at the low end of a record of a word, written as a record of its own.
 *
 * @param[out] program The program [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 */
static int sturmian_make_gather(GameProgram *program, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int read =
        exact_record_read_unsigned(&built, exact_record_member_field(&built, STURMIAN_WIDTH + 1u, 0u), 0u);
    const unsigned int value = exact_record_narrow(&built, read, STURMIAN_WIDTH);
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {1u, 0u, 0u};
    const int ok = game_load(&built, 1u, &value, 1u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/** @brief One row of the report: a slope at a window. */
typedef struct
{
    GameInterval margin; /**< The margin of the reported period. */
    GameInterval floor;  /**< The tallest margin of the shuffles, at least 0. */
    unsigned int slope;  /**< The slope. */
    unsigned int window; /**< The window. */
    unsigned int period; /**< The reported period. */
    int known;           /**< 1 where the period is a denominator of the slope. */
    unsigned int order;  /**< Its place in the order of the run, for a stable sort by margin. */
} SturmianRow;

/** @brief The order of two rows by margin, the order of the run on a tie, as the stable sort of Python keeps them. */
static int sturmian_row_order(const void *left, const void *right)
{
    const SturmianRow *const first = (const SturmianRow *)left;
    const SturmianRow *const second = (const SturmianRow *)right;
    const int order = orior_exact_compare(&first->margin.low, &second->margin.low);
    if (order != 0)
    {
        return order;
    }
    return (first->order < second->order) ? -1 : ((first->order > second->order) ? 1 : 0);
}

/**
 * @brief Runs the nine slopes.
 *
 * @param[in] argc The count of arguments.
 * @param[in] argv The arguments, none read [BORROWS].
 * @return         0 where every check holds, else 1.
 */
int program_main(int argc, char **argv)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    VerbaLinea *const line = &results.line;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    GameInterval slopes[STURMIAN_SLOPES];
    SturmianSet known[STURMIAN_SLOPES];
    int ok = sturmian_slopes(slopes);
    for (unsigned int slope = 0u; ok && (slope < STURMIAN_SLOPES); slope += 1u)
    {
        ok = sturmian_denominators(&slopes[slope], &known[slope]);
        sim_check(&results, ok && known[slope].complete, "the interval of the slope decides each term of its fraction");
    }
    sim_check(&results, ok, "the nine slopes are intervals of exact integers");
    GameProgram word;
    GameProgram gather;
    GameProgram lag;
    unsigned int word_steps[2];
    unsigned int lag_steps[DETECTOR_SUMS];
    ok = ok && sturmian_make_word(&word, word_steps, &error) && sturmian_make_gather(&gather, &error) &&
         detector_make_lag(STURMIAN_WIDTH, &lag, lag_steps, &error);
    sim_check(&results, ok, "the three record programs imprint, arrange and load");
    const unsigned long long terms = STURMIAN_TERMS;
    const unsigned long long words = STURMIAN_SLOPES * terms;
    const unsigned long long series = STURMIAN_SLOPES * (1ULL + STURMIAN_DRAWS);
    const unsigned long long values = (series * terms) + 1ULL;
    const unsigned long long lanes = (DETECTOR_SWEEP_LANES > values) ? DETECTOR_SWEEP_LANES : values;
    const unsigned long long out_limbs = (lag.layout.out_limbs > 1u) ? lag.layout.out_limbs : 1u;
    const unsigned long long declared = (STURMIAN_SLOPES * 16ULL) + (words * 4ULL) + (values * 4ULL) +
                                        (lanes * 2ULL * 4ULL) + (lanes * out_limbs * 4ULL) +
                                        twister_bytes(STURMIAN_DRAWS);
    ok = ok && sim_job_submit(&results, "sturmian_convergents", argc, argv, declared);
    unsigned int *device_slopes = NULL;
    unsigned int *device_words = NULL;
    unsigned int *device_values = NULL;
    unsigned int *device_index = NULL;
    unsigned int *device_out = NULL;
    ok = ok && sim_status_check(&results, cudaMalloc((void **)&device_slopes, STURMIAN_SLOPES * 16ULL), "slopes") &&
         sim_status_check(&results, cudaMalloc((void **)&device_words, words * 4ULL), "words") &&
         sim_status_check(&results, cudaMalloc((void **)&device_values, values * 4ULL), "values") &&
         sim_status_check(&results, cudaMalloc((void **)&device_index, lanes * 2ULL * 4ULL), "index") &&
         sim_status_check(&results, cudaMalloc((void **)&device_out, lanes * out_limbs * 4ULL), "out") &&
         sim_status_check(&results, cudaMemset(device_values, 0, values * 4ULL), "values cleared");
    unsigned int *const host_index =
        (unsigned int *)platform_cell_taken((size_t)(lanes * 2ULL * 4ULL), platform_place_g_program_owner, 0);
    unsigned int *const host_words =
        (unsigned int *)platform_cell_taken((size_t)(words * 4ULL), platform_place_g_program_owner, 0);
    unsigned int *const sums = (unsigned int *)platform_cell_taken(
        (size_t)(series * (DETECTOR_LAGS_MAX + 1ULL) * 2ULL * 4ULL), platform_place_g_program_owner, 0);
    ok = ok && (host_index != NULL) && (host_words != NULL) && (sums != NULL);
    // the slopes, the low and the high end each in two limbs; each is below 2^63
    unsigned int records[STURMIAN_SLOPES * 4u];
    for (unsigned int slope = 0u; ok && (slope < STURMIAN_SLOPES); slope += 1u)
    {
        records[(4u * slope) + 0u] = slopes[slope].low.limb[0];
        records[(4u * slope) + 1u] = slopes[slope].low.limb[1];
        records[(4u * slope) + 2u] = slopes[slope].high.limb[0];
        records[(4u * slope) + 3u] = slopes[slope].high.limb[1];
    }
    for (unsigned long long lane = 0ULL; ok && (lane < words); lane += 1ULL)
    {
        // the slope of a lane is a small count
        host_index[lane] = (unsigned int)(lane / terms);
    }
    ok = ok &&
         sim_status_check(&results, cudaMemcpy(device_slopes, records, sizeof(records), cudaMemcpyHostToDevice),
                          "slopes sent") &&
         sim_status_check(&results, cudaMemcpy(device_index, host_index, words * 4ULL, cudaMemcpyHostToDevice),
                          "word index sent");
    const CycleRecordRunRequest run_word = {
        word.record, {device_slopes, NULL, NULL}, {STURMIAN_SLOPES, 0ULL, 0ULL}, device_index, words, device_words,
        &error};
    ok = ok && (cycle_record_run(&run_word) == (long)words) &&
         sim_status_check(&results, cudaMemcpy(host_words, device_words, words * 4ULL, cudaMemcpyDeviceToHost),
                          "words read");
    // both ends of the interval give the same gap at every term
    const unsigned int low_offset = word.layout.step_table[word_steps[0]].out_offset;
    const unsigned int high_offset = word.layout.step_table[word_steps[1]].out_offset;
    const unsigned int mask = (1u << (STURMIAN_WIDTH + 1u)) - 1u;
    unsigned long long split = 0ULL;
    for (unsigned long long lane = 0ULL; ok && (lane < words); lane += 1ULL)
    {
        split +=
            (((host_words[lane] >> low_offset) & mask) != ((host_words[lane] >> high_offset) & mask)) ? 1ULL : 0ULL;
    }
    sim_check(&results, ok && (low_offset == 0u) && (split == 0ULL),
              "both ends of each slope give the same gap at every term");
    // the series: for each slope its word, then its 12 shuffles at the seeds 0 to 11. The twister on the device draws
    // each order. The order depends on the seed and the length alone, and one generator a seed serves every slope.
    unsigned int seeds[STURMIAN_DRAWS];
    for (unsigned int draw = 0u; draw < STURMIAN_DRAWS; draw += 1u)
    {
        seeds[draw] = draw;
    }
    unsigned int *const orders = (unsigned int *)platform_cell_taken(
        (size_t)(STURMIAN_DRAWS * terms * sizeof(unsigned int)), platform_place_g_program_owner, 0);
    TwisterSet twister;
    ok = ok && (orders != NULL) && twister_open(&results, &twister, seeds, STURMIAN_DRAWS) &&
         twister_shuffle(&results, &twister, STURMIAN_TERMS, orders);
    // the twister of the host, as a second route: every order of the device equals its order
    unsigned int *const order =
        (unsigned int *)platform_cell_taken((size_t)(terms * sizeof(unsigned int)), platform_place_g_program_owner, 0);
    unsigned long long mismatches = 0ULL;
    ok = ok && (order != NULL);
    for (unsigned int draw = 0u; ok && (draw < STURMIAN_DRAWS); draw += 1u)
    {
        for (unsigned int place = 0u; place < STURMIAN_TERMS; place += 1u)
        {
            order[place] = place;
        }
        ZetaTwister host;
        twister_seed(&host, draw);
        for (unsigned int place = STURMIAN_TERMS - 1u; place > 0u; place -= 1u)
        {
            const unsigned int other = twister_below(&host, place + 1u);
            const unsigned int held_place = order[place];
            order[place] = order[other];
            order[other] = held_place;
        }
        for (unsigned int place = 0u; place < STURMIAN_TERMS; place += 1u)
        {
            mismatches += (order[place] != orders[(draw * terms) + place]) ? 1ULL : 0ULL;
        }
    }
    sim_check(&results, ok && (mismatches == 0ULL),
              "the twister of the device draws every order that the twister of the host draws");
    unsigned long long written = 0ULL;
    for (unsigned long long one = 0ULL; ok && (one < series); one += 1ULL)
    {
        // the slope and the seed are small counts
        const unsigned int slope = (unsigned int)(one / (1ULL + STURMIAN_DRAWS));
        const unsigned int draw = (unsigned int)(one % (1ULL + STURMIAN_DRAWS));
        for (unsigned int place = 0u; place < STURMIAN_TERMS; place += 1u)
        {
            const unsigned int from = (draw == 0u) ? place : orders[((draw - 1u) * terms) + place];
            host_index[written + place] = (slope * STURMIAN_TERMS) + from;
        }
        written += terms;
    }
    twister_close(&twister);
    platform_cell_released(orders, platform_place_g_program_owner);
    ok = ok && sim_status_check(&results, cudaMemcpy(device_index, host_index, written * 4ULL, cudaMemcpyHostToDevice),
                                "gather index sent");
    const CycleRecordRunRequest run_gather = {
        gather.record, {device_words, NULL, NULL}, {words, 0ULL, 0ULL}, device_index, written, device_values, &error};
    ok = ok && (cycle_record_run(&run_gather) == (long)written);
    sim_check(&results, ok, "the words and their shuffles are written on the device");
    const unsigned int widest = STURMIAN_WINDOWS[STURMIAN_WINDOW_COUNT - 1u];
    DetectorLagSums *const lag_sums = (DetectorLagSums *)platform_cell_cleared(
        (size_t)(series * (widest + 1ULL)), sizeof(DetectorLagSums), platform_place_g_program_owner, 0);
    ok = ok && (lag_sums != NULL);
    const DetectorSweep sweep = {&lag,   lag_steps,  device_values, series,     terms,
                                 widest, host_index, device_index,  device_out, sums};
    ok = ok && detector_sweep(&results, &sweep, lag_sums);
    sim_check(&results, ok, "the lag program sweeps and sums every series on the device");
    // the readings: every slope at every window, against the tallest margin of its shuffles there
    SturmianRow rows[STURMIAN_SLOPES * STURMIAN_WINDOW_COUNT];
    unsigned long long undecided = 0ULL;
    unsigned long long unclear = 0ULL;
    unsigned int spread = 0u;
    mmgr_line_put(line, "slope    reported period by window      all clear their floor\n");
    for (unsigned int slope = 0u; ok && (slope < STURMIAN_SLOPES); slope += 1u)
    {
        char reported[64];
        size_t used = 0u;
        int cleared = 1;
        int one_answer = 1;
        reported[0] = '\0';
        for (unsigned int window = 0u; ok && (window < STURMIAN_WINDOW_COUNT); window += 1u)
        {
            const unsigned int longest = STURMIAN_WINDOWS[window];
            const unsigned long long base = (unsigned long long)slope * (1ULL + STURMIAN_DRAWS);
            DetectorReading reading;
            ok = detector_detect(&lag_sums[base * (widest + 1ULL)], terms, longest, &reading);
            undecided += (ok && !reading.decided) ? 1ULL : 0ULL;
            SturmianRow *const row = &rows[(slope * STURMIAN_WINDOW_COUNT) + window];
            row->margin = reading.margin;
            orior_exact_zero(&row->floor.low);
            orior_exact_zero(&row->floor.high);
            for (unsigned int draw = 1u; ok && (draw <= STURMIAN_DRAWS); draw += 1u)
            {
                DetectorReading shuffled;
                ok = detector_detect(&lag_sums[(base + draw) * (widest + 1ULL)], terms, longest, &shuffled);
                undecided += (ok && !shuffled.decided) ? 1ULL : 0ULL;
                if (ok && (shuffled.period != 0u))
                {
                    if (orior_exact_compare(&shuffled.margin.low, &row->floor.low) > 0)
                    {
                        row->floor.low = shuffled.margin.low;
                    }
                    if (orior_exact_compare(&shuffled.margin.high, &row->floor.high) > 0)
                    {
                        row->floor.high = shuffled.margin.high;
                    }
                }
            }
            row->slope = slope;
            row->window = longest;
            row->period = reading.period;
            row->known = sturmian_has(&known[slope], reading.period);
            row->order = (slope * STURMIAN_WINDOW_COUNT) + window;
            // the margin clears its floor where its low end is above the high end of the floor
            const int above = orior_exact_compare(&row->margin.low, &row->floor.high) > 0;
            const int below = orior_exact_compare(&row->margin.high, &row->floor.low) <= 0;
            unclear += (!above && !below) ? 1ULL : 0ULL;
            cleared = cleared && (reading.period != 0u) && above;
            one_answer = one_answer && (reading.period == rows[slope * STURMIAN_WINDOW_COUNT].period);
            used += (size_t)snprintf(reported + used, sizeof(reported) - used, "%s%u", (window == 0u) ? "[" : ", ",
                                     reading.period);
        }
        snprintf(reported + used, sizeof(reported) - used, "]");
        spread += one_answer ? 1u : 0u;
        char text[160];
        snprintf(text, sizeof(text), "%-8s %-30s %s\n", STURMIAN_NAMES[slope], reported, cleared ? "True" : "False");
        mmgr_line_put(line, text);
    }
    const unsigned int row_count = STURMIAN_SLOPES * STURMIAN_WINDOW_COUNT;
    unsigned int inside = 0u;
    for (unsigned int at = 0u; at < row_count; at += 1u)
    {
        inside += rows[at].known ? 1u : 0u;
    }
    if (ok)
    {
        char text[200];
        snprintf(text, sizeof(text),
                 "\nreported period is a convergent or intermediate denominator of the slope: %u of %u rows\n", inside,
                 row_count);
        mmgr_line_put(line, text);
        qsort(rows, row_count, sizeof(SturmianRow), sturmian_row_order);
        for (unsigned int at = 0u; at < row_count; at += 1u)
        {
            if (rows[at].known)
            {
                continue;
            }
            long long scaled = 0;
            unclear += game_round(&rows[at].margin, 64u, 3u, &scaled) ? 0ULL : 1ULL;
            mmgr_line_put(line, "  the exception at margin ");
            game_put_decimal(line, scaled, 3u);
            snprintf(text, sizeof(text), " (%s, window %u, reported %u) is rank %u of %u by margin\n",
                     STURMIAN_NAMES[rows[at].slope], rows[at].window, rows[at].period, at, row_count);
            mmgr_line_put(line, text);
        }
        // the margin over its floor, on the rows whose floor is above 0, at 2^-64
        GameInterval ratios[STURMIAN_SLOPES * STURMIAN_WINDOW_COUNT];
        unsigned int ratio_count = 0u;
        OriorExactInteger unit;
        ok = game_power(2, 64u, &unit);
        for (unsigned int at = 0u; ok && (at < row_count); at += 1u)
        {
            if (rows[at].floor.low.sign > 0)
            {
                OriorExactInteger work;
                ok = game_multiply(&rows[at].margin.low, &unit, &work) &&
                     game_divide(&work, &rows[at].floor.high, 0, &ratios[ratio_count].low) &&
                     game_multiply(&rows[at].margin.high, &unit, &work) &&
                     game_divide(&work, &rows[at].floor.low, 1, &ratios[ratio_count].high);
                ratio_count += 1u;
            }
            else if (rows[at].floor.high.sign > 0)
            {
                unclear += 1ULL;
            }
        }
        if (ok && (ratio_count > 0u))
        {
            qsort(ratios, ratio_count, sizeof(GameInterval), detector_interval_order);
            long long low_ratio = 0;
            long long middle_ratio = 0;
            unclear += game_round(&ratios[0], 64u, 1u, &low_ratio) ? 0ULL : 1ULL;
            unclear += game_round(&ratios[ratio_count / 2u], 64u, 1u, &middle_ratio) ? 0ULL : 1ULL;
            mmgr_line_put(line, "margin over its own shuffle floor: min ");
            game_put_decimal(line, low_ratio, 1u);
            mmgr_line_put(line, ", median ");
            game_put_decimal(line, middle_ratio, 1u);
            mmgr_line_ch(line, '\n');
        }
        snprintf(text, sizeof(text),
                 "slopes giving one answer at all %u windows: %u of %u. For a real period it is all of them.\n",
                 STURMIAN_WINDOW_COUNT, spread, STURMIAN_SLOPES);
        mmgr_line_put(line, text);
    }
    sim_check(&results, ok, "every slope was computed and read");
    sim_check(&results, undecided == 0ULL, "every reading is decided at the precision that it asked for");
    sim_check(&results, unclear == 0ULL, "every comparison and printed value is clear at both ends of its interval");
    sim_check(&results, spread == 0u, "no slope with no period gives one answer at every window");
    sim_flush(&results);
    cudaFree(device_slopes);
    cudaFree(device_words);
    cudaFree(device_values);
    cudaFree(device_index);
    cudaFree(device_out);
    game_release(&word);
    game_release(&gather);
    game_release(&lag);
    cycle_record_sum_release();
    platform_cell_released(host_index, platform_place_g_program_owner);
    platform_cell_released(host_words, platform_place_g_program_owner);
    platform_cell_released(sums, platform_place_g_program_owner);
    platform_cell_released(order, platform_place_g_program_owner);
    platform_cell_released(lag_sums, platform_place_g_program_owner);
    return sim_close(&results, "sturmian convergents");
}
