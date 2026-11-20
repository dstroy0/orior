// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-034
/**
 * @file exact_zeta_carrier.c
 * @brief The walker w = exp(i theta) F read against known carriers on theta / pi, and the arms of the demon against
 *        each.
 *
 * @note Each cell runs once on the device by the multiple evaluation, on a lattice of 16 points a zero or more. The
 *       device lists every point with w and theta / pi less and more its bound, the listing word 3.
 * @note Run build_exact_zeta.sh carrier <binary> <first> <last> [rate], beside this file.
 * @note A carrier is a run of marks on theta / pi from the first point of the cell, rate marks a unit on average.
 *       The carriers are the uniform comb, the 1, 1, 2 comb, and the golden and silver words.
 * @note Those words are the Sturmian words of slope 1 / beta with beta = (n + sqrt(n^2 + 4)) / 2, n 1 and 2, two
 *       gaps in the ratio beta. Character k of the word is floor((k + 1) / beta) - floor(k / beta), and
 *       floor(k / beta) = (isqrt(k^2 (n^2 + 4)) - k n) // 2, exactly.
 * @note The code reads a mark at the first listed point whose theta / pi less its bound reaches it. For the uniform
 *       comb and 1, 1, 2, that is a comparison of integers. For the metallic words, it is A >= 0 and A^2 >= (n^2 + 4)
 *       B^2, with A and B the integers of the place of the mark. The code never writes sqrt(n^2 + 4) down.
 * @note The moments of w come at the points that a carrier reads. Each is an exact sum of the integers of the device
 *       at 2^-62, over the count. They are the inertia |w|^2, w itself, and w against its conjugate, w^2.
 * @note From each read point to the next, they are the coherence and the angular momentum. The coherence is
 *       Re(conj(w_k) w_(k+1)), and the angular momentum is Im(conj(w_k) w_(k+1)). The beat is the difference of the
 *       moments of two carriers.
 * @note The arms of the demon, 31 draws each: arm a is the steps of the carrier shuffled, every step kept. Arm b is
 *       the carrier shifted along theta / pi by d / 32 of its mean step, d from 1 to 31.
 * @note A moment is above background where no draw reaches it. Its reach is the fewer of the draws at or above it and
 *       at or below it. The steps of the uniform comb are all alike, and its arm a gives it back. The code checks
 *       that. The code chooses no number.
 * @note It sits in 0_experimental, and is an entry in the workbook of analytic number theory, on its rail. It proves
 *       nothing. It reads whether w holds to a carrier set on the own clock of theta.
 */
#include "zeta_machine.h"
#include "zeta_shuffle.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The draws of each arm. */
#define DRAWS 31

/** @brief The seed of the shuffles. */
#define CARRIER_SEED 20261004u

/** @brief The carriers and the moments. */
#define CARRIER_COUNT 4
#define MOMENT_COUNT 7

static const char *const CARRIERS[CARRIER_COUNT] = {"uniform", "comb 1,1,2", "golden", "silver"};
static const char *const MOMENTS[MOMENT_COUNT] = {"inertia |w|^2",   "Re w", "Im w", "Re w^2", "Im w^2", "coherence",
                                                  "angular momentum"};

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *carrier_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief An exact rational in decimal, cut toward zero, for the page alone.
 *
 * @param[in]  value    The rational [BORROWS].
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void show(const ZetaRational *value, char *text, size_t capacity)
{
    OriorExactInteger size;
    z_abs(&value->top, &size);
    zeta_require(orior_exact_scale_by_ten(&size, 6u), "scale");
    z_floor_div(&size, &value->under, &size);
    DecimalPair pair;
    pair.numerator = size;
    pair.places = 6;
    text[0] = '-';
    const size_t skip = (value->top.sign < 0) ? 1u : 0u;
    pair_text(&pair, 6, text + skip, capacity - skip);
}

/**
 * @brief The text of a double as Python prints it: the shortest digits that read back to it.
 *
 * @param[in]  value    The double.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 * @note A value with no point and no exponent gains ".0". The exponent form starts at 1e16 and below 1e-4.
 */
static void python_repr(double value, char *text, size_t capacity)
{
    char digits[64];
    int precision = 1;
    for (; precision <= 17; precision += 1)
    {
        snprintf(digits, sizeof(digits), "%.*e", precision - 1, value);
        if (strtod(digits, NULL) == value)
        {
            break;
        }
    }
    const double size = fabs(value);
    if ((size != 0.0) && ((size >= 1e16) || (size < 1e-4)))
    {
        snprintf(text, capacity, "%s", digits);
        return;
    }
    const char *const mark = strchr(digits, 'e');
    const int exponent = (mark != NULL) ? atoi(mark + 1) : 0;
    const int decimals = ((precision - 1 - exponent) > 0) ? (precision - 1 - exponent) : 0;
    snprintf(digits, sizeof(digits), "%.*f", decimals, value);
    if (strchr(digits, '.') == NULL)
    {
        snprintf(text, capacity, "%.60s.0", digits);
    }
    else
    {
        snprintf(text, capacity, "%s", digits);
    }
}

/**
 * @brief A double cut toward zero, as an exact integer, as int() of Python cuts it.
 *
 * @param[in]  value  The double.
 * @param[out] result The integer [BORROWS].
 */
static void double_to_exact(double value, OriorExactInteger *result)
{
    const double whole = trunc(value);
    int exponent = 0;
    const double mantissa = frexp(fabs(whole), &exponent);
    // The mantissa times 2^53 is an integer below 2^53.
    const int64_t bits = (int64_t)ldexp(mantissa, 53);
    z_set(result, bits);
    if (exponent >= 53)
    {
        OriorExactInteger power;
        z_two_to(exponent - 53, &power);
        z_mul(result, &power, result);
    }
    else
    {
        OriorExactInteger power;
        z_two_to(53 - exponent, &power);
        z_floor_div(result, &power, result);
    }
    if (whole < 0.0)
    {
        z_neg(result, result);
    }
}

/** @brief One listed cell: the w of each point and theta / pi less its bound. */
typedef struct
{
    OriorExactInteger (*ws)[2]; /**< The w of each point [OWNS]. */
    OriorExactInteger *lows;    /**< theta / pi less its bound [OWNS]. */
    size_t count;               /**< How many points. */
    int64_t failed;             /**< The failed host checks. */
} Listing;

/**
 * @brief Cell nu at 16 points a zero or more: the w of each point, two integers at 2^-62, and theta / pi less its
 *        bound.
 *
 * @param[in]  binary    The device program [BORROWS].
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  nu        The cell.
 * @param[out] listing   The listing [BORROWS].
 */
static void listed(const char *binary, const TuringConstants *constants, int64_t nu, Listing *listing)
{
    // The bits of a lattice are small.
    const int64_t p = (int64_t)ceil(log2(16.0 * turing_rises(nu)));
    TuringCell *const cell = (TuringCell *)carrier_memory(malloc(sizeof(TuringCell)));
    OriorExactInteger bound;
    turing_run_cell(binary, constants, nu, p, TURING_TRANSFORM, 0, 3, cell, &bound);
    FILE *const handle = fopen(cell->path, "r");
    if (handle == NULL)
    {
        zeta_stop("the listing of the cell is gone");
    }
    size_t capacity = 1024u;
    listing->ws = (OriorExactInteger(*)[2])carrier_memory(malloc(capacity * sizeof(OriorExactInteger[2])));
    listing->lows = (OriorExactInteger *)carrier_memory(malloc(capacity * sizeof(OriorExactInteger)));
    listing->count = 0u;
    static char line[1 << 16];
    while (fgets(line, sizeof(line), handle) != NULL)
    {
        if (strncmp(line, "point", 5u) != 0)
        {
            continue;
        }
        char *words[16];
        size_t count = 0u;
        char *cursor = strtok(line + 5, " \r\n");
        while ((cursor != NULL) && (count < 16u))
        {
            words[count] = cursor;
            count += 1u;
            cursor = strtok(NULL, " \r\n");
        }
        if (count < 6u)
        {
            continue;
        }
        if (listing->count >= capacity)
        {
            capacity *= 2u;
            listing->ws =
                (OriorExactInteger(*)[2])carrier_memory(realloc(listing->ws, capacity * sizeof(OriorExactInteger[2])));
            listing->lows =
                (OriorExactInteger *)carrier_memory(realloc(listing->lows, capacity * sizeof(OriorExactInteger)));
        }
        turing_parse_hex(words[3], &listing->ws[listing->count][0]);
        turing_parse_hex(words[4], &listing->ws[listing->count][1]);
        turing_parse_hex(words[5], &listing->lows[listing->count]);
        listing->count += 1u;
    }
    fclose(handle);
    (void)remove(cell->path);
    listing->failed = cell->failed;
    free(cell);
}

/** @brief A carrier: its marks as a test of reach. */
typedef struct
{
    int n;                     /**< 1 for golden, 2 for silver, 0 for the combs. */
    OriorExactInteger num;     /**< The numerator of the rate. */
    OriorExactInteger den;     /**< The denominator of the rate. */
    unsigned char *characters; /**< The characters, 1 for the long step [OWNS]. */
    size_t count;              /**< How many. */
} Carrier;

/**
 * @brief The carrier of a name at a rate, with characters for twice the span and a margin.
 *
 * @param[out] carrier The carrier [BORROWS].
 * @param[in]  which   The index of the name.
 * @param[in]  rate    The rate.
 * @param[in]  exact   The rate as an exact rational [BORROWS].
 * @param[in]  span    The span of theta / pi over the cell, in units.
 */
static void carrier_begin(Carrier *carrier, int which, double rate, const ZetaRational *exact, int64_t span)
{
    carrier->n = (which == 2) ? 1 : ((which == 3) ? 2 : 0);
    carrier->num = exact->top;
    carrier->den = exact->under;
    // The count of marks is small, and a double holds it.
    carrier->count = ((size_t)((double)span * rate) * 2u) + 8u;
    carrier->characters = (unsigned char *)carrier_memory(calloc(carrier->count + 1u, 1u));
    for (size_t k = 0u; k < carrier->count; k += 1u)
    {
        if (carrier->n != 0)
        {
            // The words: floor(k / beta) = (isqrt(k^2 d) - k n) // 2.
            const int64_t d = (carrier->n * carrier->n) + 4;
            int64_t floors[2];
            for (int step = 0; step < 2; step += 1)
            {
                // The count of marks fits a machine integer.
                const int64_t at = (int64_t)k + step;
                OriorExactInteger value;
                z_set(&value, at * at * d);
                z_isqrt(&value, &value);
                floors[step] = (z_small(&value) - (at * carrier->n)) / 2;
            }
            // The step is 0 or 1.
            carrier->characters[k] = (unsigned char)(floors[1] - floors[0]);
        }
        else if (which == 1)
        {
            carrier->characters[k] = ((k % 3u) == 2u) ? 1u : 0u;
        }
    }
}

/**
 * @brief 1 where x reaches the mark after the long and short steps, with the whole carrier moved on by the shift.
 *
 * @param[in] carrier The carrier [BORROWS].
 * @param[in] x       The place [BORROWS].
 * @param[in] longs   The long steps.
 * @param[in] shorts  The short steps.
 * @param[in] shift   The shift [BORROWS].
 * @return            1 where x reaches the mark.
 * @note x is theta / pi past the origin, as an integer at 2^-62. The shift is a fraction of the mean step.
 * @note The combs: a long step is two short ones, and the mean step is 1 / rate.
 * @note The words: the steps are short and beta short, the mean step 1 / rate with short = 1 / (rate (2 - omega)).
 *       Then q (4 + n - sqrt d) / 2 >= longs (n + sqrt d) / 2 + shorts is A >= sqrt(d) B, each side times ONE den sd.
 */
static int carrier_reached(const Carrier *carrier, const OriorExactInteger *x, int64_t longs, int64_t shorts,
                           const ZetaRational *shift)
{
    OriorExactInteger one;
    z_two_to(TURING_SCALE_BITS, &one);
    const OriorExactInteger *const sn = &shift->top;
    const OriorExactInteger *const sd = &shift->under;
    if (carrier->n == 0)
    {
        int64_t longs_in = 0;
        for (size_t at = 0u; at < carrier->count; at += 1u)
        {
            longs_in += carrier->characters[at];
        }
        // The count of characters fits a machine integer.
        const int64_t size = (int64_t)carrier->count;
        const int64_t total = size + longs_in;
        OriorExactInteger left;
        z_mul(x, &carrier->num, &left);
        z_mul_small(&left, total, &left);
        z_mul(&left, sd, &left);
        OriorExactInteger right;
        z_mul_small(sd, ((2 * longs) + shorts) * size, &right);
        OriorExactInteger part;
        z_mul_small(sn, total, &part);
        z_add(&right, &part, &right);
        z_mul(&right, &carrier->den, &right);
        z_mul(&right, &one, &right);
        return z_cmp(&left, &right) >= 0;
    }
    const int64_t n = carrier->n;
    const int64_t d = (n * n) + 4;
    OriorExactInteger scale;
    z_mul(&one, &carrier->den, &scale);
    z_mul(&scale, sd, &scale);
    OriorExactInteger q;
    z_mul(x, &carrier->num, &q);
    z_mul(&q, sd, &q);
    OriorExactInteger part;
    z_mul(sn, &one, &part);
    z_mul(&part, &carrier->den, &part);
    z_sub(&q, &part, &q);
    OriorExactInteger a;
    z_mul_small(&q, 4 + n, &a);
    z_mul_small(&scale, (2 * shorts) + (longs * n), &part);
    z_sub(&a, &part, &a);
    OriorExactInteger b;
    z_mul_small(&scale, longs, &part);
    z_add(&q, &part, &b);
    if (a.sign < 0)
    {
        return 0;
    }
    OriorExactInteger left;
    OriorExactInteger right;
    z_mul(&a, &a, &left);
    z_mul(&b, &b, &right);
    z_mul_small(&right, d, &right);
    return z_cmp(&left, &right) >= 0;
}

/**
 * @brief Where the mark is, in units of theta / pi, near enough to start the search. The verdicts place it.
 *
 * @param[in] carrier The carrier [BORROWS].
 * @param[in] longs   The long steps.
 * @param[in] shorts  The short steps.
 * @param[in] shift   The shift.
 * @return            The guess.
 */
static double carrier_guess(const Carrier *carrier, int64_t longs, int64_t shorts, double shift)
{
    const double den = (double)z_small(&carrier->den);
    const double num = (double)z_small(&carrier->num);
    if (carrier->n == 0)
    {
        int64_t longs_in = 0;
        for (size_t at = 0u; at < carrier->count; at += 1u)
        {
            longs_in += carrier->characters[at];
        }
        // The count of characters fits a machine integer.
        const int64_t size = (int64_t)carrier->count;
        const int64_t total = size + longs_in;
        return (((double)(((2 * longs) + shorts) * size) / (double)total) + shift) * den / num;
    }
    const double n = (double)carrier->n;
    const double beta = (n + sqrt((n * n) + 4.0)) / 2.0;
    return (((((double)longs * beta) + (double)shorts) / (2.0 - (1.0 / beta))) + shift) * den / num;
}

/**
 * @brief The listed points that the carrier reads, one for each mark in turn.
 *
 * @param[in]  carrier    The carrier [BORROWS].
 * @param[in]  listing    The listing [BORROWS].
 * @param[in]  characters The characters [BORROWS].
 * @param[in]  shift      The shift [BORROWS].
 * @param[out] points     The points [BORROWS]. At most the count of characters + 1.
 * @return                How many.
 * @note Each is the first point whose theta / pi less its bound reaches the mark. The search starts from a guess, and
 *       the verdicts alone place the point.
 */
static size_t carrier_points(const Carrier *carrier, const Listing *listing, const unsigned char *characters,
                             const ZetaRational *shift, size_t *points)
{
    const OriorExactInteger *const origin = &listing->lows[0];
    OriorExactInteger one;
    z_two_to(TURING_SCALE_BITS, &one);
    const double shift_value = rational_to_double(shift);
    size_t found = 0u;
    int64_t longs = 0;
    int64_t shorts = 0;
    for (size_t k = 0u; k <= carrier->count; k += 1u)
    {
        if (k > 0u)
        {
            const int character = characters[k - 1u];
            longs += character;
            shorts += 1 - character;
        }
        OriorExactInteger target;
        double_to_exact(carrier_guess(carrier, longs, shorts, shift_value) * 4611686018427387904.0, &target);
        z_add(origin, &target, &target);
        size_t low = 0u;
        size_t high = listing->count;
        while (low < high)
        {
            const size_t middle = (low + high) / 2u;
            if (z_cmp(&listing->lows[middle], &target) < 0)
            {
                low = middle + 1u;
            }
            else
            {
                high = middle;
            }
        }
        size_t j = low;
        OriorExactInteger x;
        while (j > 0u)
        {
            z_sub(&listing->lows[j - 1u], origin, &x);
            if (!carrier_reached(carrier, &x, longs, shorts, shift))
            {
                break;
            }
            j -= 1u;
        }
        while (j < listing->count)
        {
            z_sub(&listing->lows[j], origin, &x);
            if (carrier_reached(carrier, &x, longs, shorts, shift))
            {
                break;
            }
            j += 1u;
        }
        if (j == listing->count)
        {
            break;
        }
        points[found] = j;
        found += 1u;
    }
    return found;
}

/**
 * @brief Adds the exact sums of the moments over the points to the totals.
 *
 * @param[in]     listing The listing [BORROWS].
 * @param[in]     points  The points [BORROWS].
 * @param[in]     count   How many.
 * @param[in,out] sums    The totals [BORROWS].
 */
static void add_moments(const Listing *listing, const size_t *points, size_t count,
                        OriorExactInteger sums[MOMENT_COUNT])
{
    for (size_t at = 0u; at < count; at += 1u)
    {
        const OriorExactInteger *const re = &listing->ws[points[at]][0];
        const OriorExactInteger *const im = &listing->ws[points[at]][1];
        OriorExactInteger first;
        OriorExactInteger second;
        z_mul(re, re, &first);
        z_mul(im, im, &second);
        OriorExactInteger value;
        z_add(&first, &second, &value);
        z_add(&sums[0], &value, &sums[0]);
        z_add(&sums[1], re, &sums[1]);
        z_add(&sums[2], im, &sums[2]);
        z_sub(&first, &second, &value);
        z_add(&sums[3], &value, &sums[3]);
        z_mul(re, im, &value);
        z_mul_small(&value, 2, &value);
        z_add(&sums[4], &value, &sums[4]);
        if ((at + 1u) < count)
        {
            const OriorExactInteger *const re2 = &listing->ws[points[at + 1u]][0];
            const OriorExactInteger *const im2 = &listing->ws[points[at + 1u]][1];
            z_mul(re, re2, &first);
            z_mul(im, im2, &second);
            z_add(&first, &second, &value);
            z_add(&sums[5], &value, &sums[5]);
            z_mul(re, im2, &first);
            z_mul(im, re2, &second);
            z_sub(&first, &second, &value);
            z_add(&sums[6], &value, &sums[6]);
        }
    }
}

/**
 * @brief Each moment over its count in units of w: the squares over 2^124, the first powers over 2^62.
 *
 * @param[in]  sums   The sums [BORROWS].
 * @param[in]  count  The count.
 * @param[out] values The moments [BORROWS].
 */
static void scaled(const OriorExactInteger sums[MOMENT_COUNT], int64_t count, ZetaRational values[MOMENT_COUNT])
{
    static const int POWERS[MOMENT_COUNT] = {2, 1, 1, 2, 2, 2, 2};
    for (int at = 0; at < MOMENT_COUNT; at += 1)
    {
        OriorExactInteger under;
        z_two_to(TURING_SCALE_BITS * POWERS[at], &under);
        z_mul_small(&under, count, &under);
        rational_make(&sums[at], &under, &values[at]);
    }
}

/**
 * @brief The reach of a value among the draws: the fewer of those at or above it and at or below it, and the side.
 *
 * @param[in]  real  The value [BORROWS].
 * @param[in]  draws The draws [BORROWS].
 * @param[out] side  '+' where fewer draws lie above, else '-' [BORROWS].
 * @return           The reach.
 */
static int reach_of(const ZetaRational *real, const ZetaRational draws[DRAWS], char *side)
{
    int up = 0;
    int down = 0;
    for (int at = 0; at < DRAWS; at += 1)
    {
        const int order = rational_cmp(&draws[at], real);
        up += order >= 0;
        down += order <= 0;
    }
    *side = (up < down) ? '+' : '-';
    return (up < down) ? up : down;
}

/**
 * @brief The mean of the draws, shown.
 *
 * @param[in]  draws    The draws [BORROWS].
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void show_mean(const ZetaRational draws[DRAWS], char *text, size_t capacity)
{
    ZetaRational total;
    rational_small(0, 1, &total);
    for (int at = 0; at < DRAWS; at += 1)
    {
        rational_add(&total, &draws[at], &total);
    }
    ZetaRational count;
    rational_small(DRAWS, 1, &count);
    rational_div(&total, &count, &total);
    show(&total, text, capacity);
}

int main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argument_count < 4)
    {
        fprintf(stderr, "usage: exact_zeta_carrier <binary> <first> <last> [rate]\n");
        return 2;
    }
    const char *const binary = arguments[1];
    const int64_t first = atoll(arguments[2]);
    const int64_t last = atoll(arguments[3]);
    const double rate = (argument_count > 4) ? strtod(arguments[4], NULL) : 1.0;
    char rate_text[64];
    python_repr(rate, rate_text, sizeof(rate_text));
    DecimalPair rate_pair;
    pair_units(rate_text, &rate_pair);
    OriorExactInteger rate_under;
    z_ten_to(rate_pair.places, &rate_under);
    ZetaRational exact_rate;
    rational_make(&rate_pair.numerator, &rate_under, &exact_rate);
    TuringConstants constants;
    turing_constants(&constants);
    static OriorExactInteger real[CARRIER_COUNT][MOMENT_COUNT];
    int64_t counts[CARRIER_COUNT] = {0};
    static OriorExactInteger arms[2][CARRIER_COUNT][DRAWS][MOMENT_COUNT];
    static int64_t arm_counts[2][CARRIER_COUNT][DRAWS];
    ZetaTwister twister;
    twister_seed(&twister, CARRIER_SEED);
    int64_t failed = 0;
    int given_back = 1;
    OriorExactInteger one;
    z_two_to(TURING_SCALE_BITS, &one);
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        Listing listing;
        listed(binary, &constants, nu, &listing);
        failed += listing.failed;
        OriorExactInteger span_value;
        z_sub(&listing.lows[listing.count - 1u], &listing.lows[0], &span_value);
        z_floor_div(&span_value, &one, &span_value);
        const int64_t span = z_small(&span_value) + 1;
        for (int which = 0; which < CARRIER_COUNT; which += 1)
        {
            Carrier carrier;
            carrier_begin(&carrier, which, rate, &exact_rate, span);
            size_t *const idx = (size_t *)carrier_memory(malloc((carrier.count + 2u) * sizeof(size_t)));
            size_t *const drawn = (size_t *)carrier_memory(malloc((carrier.count + 2u) * sizeof(size_t)));
            unsigned char *const characters = (unsigned char *)carrier_memory(malloc(carrier.count + 1u));
            ZetaRational zero;
            rational_small(0, 1, &zero);
            const size_t idx_count = carrier_points(&carrier, &listing, carrier.characters, &zero, idx);
            add_moments(&listing, idx, idx_count, real[which]);
            // The count of points fits a machine integer.
            counts[which] += (int64_t)idx_count;
            for (int d = 0; d < DRAWS; d += 1)
            {
                memcpy(characters, carrier.characters, carrier.count);
                twister_shuffle(&twister, characters, carrier.count);
                const size_t drawn_count = carrier_points(&carrier, &listing, characters, &zero, drawn);
                if (which == 0)
                {
                    given_back = given_back && (drawn_count == idx_count) &&
                                 (memcmp(drawn, idx, idx_count * sizeof(size_t)) == 0);
                }
                add_moments(&listing, drawn, drawn_count, arms[0][which][d]);
                arm_counts[0][which][d] += (int64_t)drawn_count;
                ZetaRational shift;
                rational_small(d + 1, DRAWS + 1, &shift);
                const size_t moved_count = carrier_points(&carrier, &listing, carrier.characters, &shift, drawn);
                add_moments(&listing, drawn, moved_count, arms[1][which][d]);
                arm_counts[1][which][d] += (int64_t)moved_count;
            }
            free(idx);
            free(drawn);
            free(characters);
            free(carrier.characters);
        }
        printf("  cell %lld: %llu points listed, ", (long long)nu, (unsigned long long)listing.count);
        for (int which = 0; which < CARRIER_COUNT; which += 1)
        {
            printf("%s%s %lld", (which > 0) ? ", " : "", CARRIERS[which], (long long)counts[which]);
        }
        printf(" read\n");
        fflush(stdout);
        free(listing.ws);
        free(listing.lows);
    }
    printf(
        "  rate %s marks a unit of theta / pi; the uniform comb's arm a gives it back: %s; host checks failed %lld\n",
        rate_text, given_back ? "True" : "False", (long long)failed);
    static ZetaRational values[CARRIER_COUNT][MOMENT_COUNT];
    static ZetaRational drawn_values[2][CARRIER_COUNT][DRAWS][MOMENT_COUNT];
    for (int which = 0; which < CARRIER_COUNT; which += 1)
    {
        scaled(real[which], counts[which], values[which]);
        for (int arm = 0; arm < 2; arm += 1)
        {
            for (int d = 0; d < DRAWS; d += 1)
            {
                scaled(arms[arm][which][d], arm_counts[arm][which][d], drawn_values[arm][which][d]);
            }
        }
    }
    printf("  each moment over its count, for each carrier: the real; arm a's mean, reach, side; arm b's mean, reach, "
           "side\n");
    for (int at = 0; at < MOMENT_COUNT; at += 1)
    {
        printf("  %s\n", MOMENTS[at]);
        for (int which = 0; which < CARRIER_COUNT; which += 1)
        {
            char real_text[128];
            show(&values[which][at], real_text, sizeof(real_text));
            printf("    %-11s %12s   ", CARRIERS[which], real_text);
            for (int arm = 0; arm < 2; arm += 1)
            {
                ZetaRational draws[DRAWS];
                for (int d = 0; d < DRAWS; d += 1)
                {
                    draws[d] = drawn_values[arm][which][d][at];
                }
                char side = '-';
                const int reach = reach_of(&values[which][at], draws, &side);
                char mean_text[128];
                show_mean(draws, mean_text, sizeof(mean_text));
                printf("%s%12s %2d %c", (arm > 0) ? "   " : "", mean_text, reach, side);
            }
            printf("\n");
        }
    }
    printf("  the beats, golden less each other carrier, against the same draws' beats:\n");
    static const int OTHERS[3] = {0, 1, 3};
    for (int at = 0; at < MOMENT_COUNT; at += 1)
    {
        printf("    %-17s ", MOMENTS[at]);
        for (int which = 0; which < 3; which += 1)
        {
            const int other = OTHERS[which];
            ZetaRational beat;
            rational_sub(&values[2][at], &values[other][at], &beat);
            char beat_text[128];
            show(&beat, beat_text, sizeof(beat_text));
            printf("%s%s %12s", (which > 0) ? "   " : "", CARRIERS[other], beat_text);
            for (int arm = 0; arm < 2; arm += 1)
            {
                ZetaRational draws[DRAWS];
                for (int d = 0; d < DRAWS; d += 1)
                {
                    rational_sub(&drawn_values[arm][2][d][at], &drawn_values[arm][other][d][at], &draws[d]);
                }
                char side = '-';
                const int reach = reach_of(&beat, draws, &side);
                printf(" %c%d%c", (arm == 0) ? 'a' : 'b', reach, side);
            }
        }
        printf("\n");
    }
    return ((failed != 0) || !given_back) ? 1 : 0;
}
