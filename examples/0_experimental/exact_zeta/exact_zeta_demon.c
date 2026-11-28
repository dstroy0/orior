// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-033
/**
 * @file exact_zeta_demon.c
 * @brief The arms of the demon over the zeros that the machine of Turing certifies, in exact integers.
 *
 * @note The arms are the permutation of identity against null, run on the curves that the zeros draw.
 *
 * @note Each cell runs once on the device with every point listed, with its theta / pi less and more its bound, the
 *       listing word 3. A zero lies between two certified points of opposite sign.
 * @note theta / pi rises. Its u = theta / pi + 1 lies between the theta / pi of the first point less its bound and
 *       that of the second more its bound, plus 1. That is an exact interval of integers at 2^-62.
 * @note A close pair with no point between shows where |Z| dips and keeps its sign. The code lists it again 64 times
 *       finer by pairs with the same brackets. The zeros placed must number the difference of N between two F. The
 *       method of Turing certifies N, or the caller gives it where a run of the machine closes over them.
 * @note Each pair of zeros within 8 has its difference as the interval that the two intervals of the zeros give. Each
 *       eighth of u counts its pairs whose interval lies in it, and those whose interval meets it and another.
 * @note Those verdicts steer. A zero in a pair that meets two eighths lists again 16 times finer between its two
 *       certified points. The code lists it by pairs with the brackets. It takes the interval about its one change of
 *       sign there, for up to three rounds. A zero whose two points are in different cells keeps its interval.
 * @note Run build_exact_zeta.sh demon <binary> <first> <last> [extra] [N at the F of first] [N at the F of last],
 *       beside this file.
 * @note Each zero is at the midpoint of its interval, an integer at 2^-64. The code reads every curve from those
 *       integers with no rounding. S between zeros is n - u at the midpoint of the n-th zero and the next.
 * @note The curves are the correlation of S over k zeros, and the pull of S back each zero. They are also the
 *       correlation of the gaps with their neighbors. They are also the number variance of windows of L on u, from
 *       windows that start every half unit. They are also the pair counts: every pair of zeros within 8 of each other,
 *       by eighths.
 * @note The arms: the permutation of identity against null, 31 draws an arm. Arm a shuffles the order of the gaps,
 *       with every gap kept. Arm b shuffles them within each run of 64 gaps, which keeps S at every 64th zero.
 * @note A point of a curve is above background where no draw reaches it. Then the real value is past the value of every
 *       draw on one side. Its reach is the fewer of the draws at or above it and at or below it. The verdict is whether
 *       the reach is 0.
 * @note The code marks a bin of a pair count where no draw reaches any count that the intervals allow. The code
 *       chooses no number. The permutation of identity gives back every real value, and every draw keeps the gaps of
 *       each run. The code checks both.
 * @note It sits in 0_experimental, and is an entry in the workbook of analytic number theory, on its rail. It proves
 *       nothing. It reads which curves of the zeros their gaps alone draw, and which only their order draws.
 */
#include "zeta_machine.h"
#include "../../../includes/platform/platform.h"
#include "zeta_shuffle.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The draws of each arm, the lags, and the bits by which a dip and a narrowing list finer. */
#define DRAWS 31
#define LAGS 48
#define FINER 6
#define DIPS 200
#define NARROW 4

/** @brief The lengths of the windows, in units of u. */
#define LENGTH_COUNT 9
static const int64_t LENGTHS[LENGTH_COUNT] = {1, 2, 4, 8, 16, 32, 64, 128, 256};

/** @brief The reach of the pairs, their bins a unit, the run of arm b, and the rounds of the narrowing. */
#define REACH 8
#define BINS 8
#define BLOCK 64
#define ROUNDS 3

/** @brief The seed of the draws. */
#define DEMON_SEED 20261004u

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *demon_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief num / den in decimal, cut toward zero at the places, for the page alone.
 *
 * @param[in]  num      The numerator [BORROWS].
 * @param[in]  den      The denominator [BORROWS].
 * @param[in]  places   The places.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void show(const OriorExactInteger *num, const OriorExactInteger *den, int places, char *text, size_t capacity)
{
    const int negative = ((num->sign < 0) != (den->sign < 0)) && (num->sign != 0);
    OriorExactInteger top;
    OriorExactInteger under;
    z_abs(num, &top);
    z_abs(den, &under);
    zeta_require(orior_exact_scale_by_ten(&top, (uint32_t)places), "scale");
    z_floor_div(&top, &under, &top);
    DecimalPair pair;
    pair.numerator = top;
    pair.places = places;
    text[0] = '-';
    const size_t skip = negative ? 1u : 0u;
    pair_text(&pair, places, text + skip, capacity - skip);
}

/**
 * @brief num / den for machine integers, shown.
 *
 * @param[in]  num      The numerator.
 * @param[in]  den      The denominator.
 * @param[in]  places   The places.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void show_small(int64_t num, int64_t den, int places, char *text, size_t capacity)
{
    OriorExactInteger top;
    OriorExactInteger under;
    z_set(&top, num);
    z_set(&under, den);
    show(&top, &under, places, text, capacity);
}

/** @brief A listed point: its cell, its lane, its certified sign, Z, and theta / pi less and more its bound. */
typedef struct
{
    int64_t nu;             /**< The cell. */
    int64_t lane;           /**< The lane. */
    int sign;               /**< The certified sign. */
    OriorExactInteger z;    /**< Z. */
    OriorExactInteger low;  /**< theta / pi less its bound. */
    OriorExactInteger high; /**< theta / pi more its bound. */
} BracketedPoint;

/** @brief A zero: its interval of u at 2^-62, and the cell, lattice and two lanes of the certified points about it. */
typedef struct
{
    OriorExactInteger low;  /**< The lower end. */
    OriorExactInteger high; /**< The upper end. */
    int64_t nu;             /**< The cell. */
    int64_t p;              /**< The lattice, or -1 where the two points are in different cells. */
    int64_t ja;             /**< The lane of the first point. */
    int64_t jb;             /**< The lane of the second point. */
} ZeroRecord;

/**
 * @brief Every listed point of the cells, in order.
 *
 * @param[in]  binary    The device program [BORROWS].
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  first     The first cell.
 * @param[in]  last      The last cell.
 * @param[in]  extra     The steps finer than four points a zero.
 * @param[out] cells     The cells, by nu [BORROWS].
 * @param[out] points    The points [BORROWS]. Allocated.
 * @return               How many.
 */
static size_t bracketed_points(const char *binary, const TuringConstants *constants, int64_t first, int64_t last,
                               int64_t extra, TuringCell *cells, BracketedPoint **points)
{
    size_t capacity = 4096u;
    size_t count = 0u;
    *points = (BracketedPoint *)demon_memory(
        platform_cell_taken(capacity * sizeof(BracketedPoint), platform_place_g_program_owner, 0));
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        const int64_t p = turing_lattice(nu, 4) + extra;
        OriorExactInteger bound;
        turing_run_cell(binary, constants, nu, p, TURING_TRANSFORM, 0, 3, &cells[nu], &bound);
        FILE *const handle = fopen(cells[nu].path, "r");
        if (handle == NULL)
        {
            zeta_stop("the listing of the cell is gone");
        }
        static char line[1 << 16];
        int64_t lane = 0;
        while (fgets(line, sizeof(line), handle) != NULL)
        {
            if (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "point", .ci = EMBED_FALSE))
            {
                continue;
            }
            char *words[16];
            size_t found = 0u;
            // each word of the line after its tag, a terminator written past each as the walk reaches it
            char *cursor = line + 5;
            while (found < 16u)
            {
                cursor += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = cursor, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                     .other = " \r\n");
                if (*cursor == '\0')
                {
                    break;
                }
                words[found] = cursor;
                found += 1u;
                cursor += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = cursor,
                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \r\n");
                if (*cursor != '\0')
                {
                    *cursor = '\0';
                    cursor += 1;
                }
            }
            if (found < 7u)
            {
                continue;
            }
            if (count >= capacity)
            {
                capacity *= 2u;
                *points = (BracketedPoint *)demon_memory(
                    platform_cell_grown(*points, (capacity / 2u) * sizeof(BracketedPoint),
                                        capacity * sizeof(BracketedPoint), platform_place_g_program_owner, 0));
            }
            BracketedPoint *const point = &(*points)[count];
            point->nu = nu;
            point->lane = lane;
            OriorExactInteger value;
            turing_parse_hex(words[0], &value);
            point->sign = (int)z_small(&value);
            turing_parse_hex(words[2], &point->z);
            turing_parse_hex(words[5], &point->low);
            turing_parse_hex(words[6], &point->high);
            count += 1u;
            lane += 1;
        }
        fclose(handle);
        (void)remove(cells[nu].path);
    }
    return count;
}

/** @brief A dip: the size of Z and its point. */
typedef struct
{
    OriorExactInteger size; /**< |Z|. */
    size_t at;              /**< The point. */
} ExactDip;

/**
 * @brief Orders two dips by |Z|, then by point.
 *
 * @param[in] left  First dip [BORROWS].
 * @param[in] right Second dip [BORROWS].
 * @return          -1, 0 or 1.
 */
static int dip_order(const void *left, const void *right)
{
    const ExactDip *const a = (const ExactDip *)left;
    const ExactDip *const b = (const ExactDip *)right;
    const int order = z_cmp(&a->size, &b->size);
    if (order != 0)
    {
        return order;
    }
    return (a->at < b->at) ? -1 : ((a->at > b->at) ? 1 : 0);
}

/**
 * @brief Orders two zeros by their lower end, then by their upper end.
 *
 * @param[in] left  First zero [BORROWS].
 * @param[in] right Second zero [BORROWS].
 * @return          -1, 0 or 1.
 */
static int record_order(const void *left, const void *right)
{
    const ZeroRecord *const a = (const ZeroRecord *)left;
    const ZeroRecord *const b = (const ZeroRecord *)right;
    const int order = z_cmp(&a->low, &b->low);
    return (order != 0) ? order : z_cmp(&a->high, &b->high);
}

/**
 * @brief Each zero between two points, until they number the count wanted.
 *
 * @note The zeros come at every change of certified sign. Then they come in the deepest dips that keep their sign,
 *       listed again finer.
 *
 * @param[in]  binary    The device program [BORROWS].
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  points    The points [BORROWS].
 * @param[in]  start     The first point.
 * @param[in]  end       The last point.
 * @param[in]  wanted    The count wanted.
 * @param[in]  extra     The steps finer than four points a zero.
 * @param[out] records   The zeros, in order [BORROWS]. Allocated.
 * @param[out] failed    The failed host checks [BORROWS].
 * @return               How many.
 */
static size_t zero_records(const char *binary, const TuringConstants *constants, const BracketedPoint *points,
                           size_t start, size_t end, int64_t wanted, int64_t extra, ZeroRecord **records,
                           int64_t *failed)
{
    OriorExactInteger one;
    z_two_to(TURING_SCALE_BITS, &one);
    size_t capacity = (end - start) + 64u;
    *records = (ZeroRecord *)demon_memory(
        platform_cell_taken(capacity * sizeof(ZeroRecord), platform_place_g_program_owner, 0));
    size_t count = 0u;
    int64_t previous = -1;
    for (size_t i = start; i <= end; i += 1u)
    {
        if (points[i].sign == 0)
        {
            continue;
        }
        if ((previous >= 0) && (points[previous].sign != points[i].sign))
        {
            const BracketedPoint *const a = &points[previous];
            const BracketedPoint *const b = &points[i];
            ZeroRecord *const record = &(*records)[count];
            z_add(&a->low, &one, &record->low);
            z_add(&b->high, &one, &record->high);
            record->nu = a->nu;
            record->p = (a->nu == b->nu) ? (turing_lattice(a->nu, 4) + extra) : -1;
            record->ja = a->lane;
            record->jb = b->lane;
            count += 1u;
        }
        // The index of a point fits a machine integer.
        previous = (int64_t)i;
    }
    const size_t crossings = count;
    ExactDip *const dips = (ExactDip *)demon_memory(
        platform_cell_taken(((end - start) + 1u) * sizeof(ExactDip), platform_place_g_program_owner, 0));
    size_t dip_count = 0u;
    for (size_t i = start + 1u; i < end; i += 1u)
    {
        const int before = points[i - 1u].z.sign > 0;
        const int here = points[i].z.sign > 0;
        const int after = points[i + 1u].z.sign > 0;
        OriorExactInteger size;
        OriorExactInteger left;
        OriorExactInteger right;
        z_abs(&points[i].z, &size);
        z_abs(&points[i - 1u].z, &left);
        z_abs(&points[i + 1u].z, &right);
        if ((before == here) && (here == after) && (z_cmp(&size, &left) < 0) && (z_cmp(&size, &right) < 0))
        {
            dips[dip_count].size = size;
            dips[dip_count].at = i;
            dip_count += 1u;
        }
    }
    qsort(dips, dip_count, sizeof(ExactDip), dip_order);
    size_t added = 0u;
    int64_t listed = 0;
    *failed = 0;
    for (size_t at = 0u; (at < dip_count) && (at < DIPS); at += 1u)
    {
        // The counts of zeros fit a machine integer.
        if ((int64_t)count >= wanted)
        {
            break;
        }
        const BracketedPoint *const point = &points[dips[at].at];
        const int64_t p = turing_lattice(point->nu, 4) + extra;
        if ((point->lane == 0) || ((point->lane + 1) >= ((int64_t)1 << p)))
        {
            continue;
        }
        const size_t total = (size_t)2 << FINER;
        int64_t *const js =
            (int64_t *)demon_memory(platform_cell_taken(total * sizeof(int64_t), platform_place_g_program_owner, 0));
        for (size_t k = 0u; k < total; k += 1u)
        {
            // k is below 128.
            js[k] = ((point->lane - 1) << FINER) + (int64_t)k;
        }
        TuringPoint *const rows = (TuringPoint *)demon_memory(
            platform_cell_cleared(total, sizeof(TuringPoint), platform_place_g_program_owner, 0));
        *failed += turing_run_points(binary, constants, point->nu, p + FINER, js, total, 1, rows);
        listed += 1;
        int64_t last = -1;
        for (size_t k = 0u; k < total; k += 1u)
        {
            if (rows[k].sign.sign == 0)
            {
                continue;
            }
            if ((last >= 0) && !orior_exact_equal(&rows[last].sign, &rows[k].sign))
            {
                if (count >= capacity)
                {
                    capacity *= 2u;
                    *records = (ZeroRecord *)demon_memory(
                        platform_cell_grown(*records, (capacity / 2u) * sizeof(ZeroRecord),
                                            capacity * sizeof(ZeroRecord), platform_place_g_program_owner, 0));
                }
                ZeroRecord *const record = &(*records)[count];
                z_add(&rows[last].extra[0], &one, &record->low);
                z_add(&rows[k].extra[1], &one, &record->high);
                record->nu = point->nu;
                record->p = p + FINER;
                record->ja = js[last];
                record->jb = js[k];
                count += 1u;
                added += 1u;
            }
            // k is below 128.
            last = (int64_t)k;
        }
        platform_cell_released(rows, platform_place_g_program_owner);
        platform_cell_released(js, platform_place_g_program_owner);
    }
    platform_cell_released(dips, platform_place_g_program_owner);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  zeros at a change of certified sign ");
        mmgr_line_decimal(output_line, (unsigned long long)crossings, 1u);
        mmgr_line_put(output_line, ", dips listed again ");
        mmgr_line_decimal_signed(output_line, (long long)listed);
        mmgr_line_put(output_line, ", zeros found in them ");
        mmgr_line_decimal(output_line, (unsigned long long)added, 1u);
        mmgr_line_put(output_line, ", host checks failed ");
        mmgr_line_decimal_signed(output_line, (long long)*failed);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    qsort(*records, count, sizeof(ZeroRecord), record_order);
    return count;
}

/**
 * @brief The zeros in the list again 2^4 times finer between their two certified points, by pairs with the brackets.
 *
 * @param[in]     binary    The device program [BORROWS].
 * @param[in]     constants The constants [BORROWS].
 * @param[in,out] records   The zeros [BORROWS].
 * @param[in]     which     The zeros to narrow, ascending [BORROWS].
 * @param[in]     count     How many.
 * @param[out]    failed    The failed host checks [BORROWS].
 * @return                  How many kept their interval.
 * @note Each takes the interval between the certified points about its one change of sign there. A zero whose two
 *       points are in different cells, or whose finer points change sign other than once, keeps its interval.
 */
static int64_t narrowed(const char *binary, const TuringConstants *constants, ZeroRecord *records, const size_t *which,
                        size_t count, int64_t *failed)
{
    OriorExactInteger one;
    z_two_to(TURING_SCALE_BITS, &one);
    *failed = 0;
    int64_t retained = 0;
    unsigned char *const grouped =
        (unsigned char *)demon_memory(platform_cell_cleared(count + 1u, 1u, platform_place_g_program_owner, 0));
    // The original records stay as the source of every group, as the list of Python keeps them.
    ZeroRecord *const before = (ZeroRecord *)demon_memory(
        platform_cell_taken((count + 1u) * sizeof(ZeroRecord), platform_place_g_program_owner, 0));
    for (size_t at = 0u; at < count; at += 1u)
    {
        before[at] = records[which[at]];
    }
    for (size_t lead = 0u; lead < count; lead += 1u)
    {
        if (grouped[lead] || (before[lead].p < 0))
        {
            continue;
        }
        const int64_t nu = before[lead].nu;
        const int64_t p = before[lead].p;
        size_t total = 0u;
        for (size_t at = lead; at < count; at += 1u)
        {
            if ((before[at].p == p) && (before[at].nu == nu))
            {
                total += (size_t)(((before[at].jb - before[at].ja) << NARROW) + 1);
            }
        }
        int64_t *const js = (int64_t *)demon_memory(
            platform_cell_taken((total + 1u) * sizeof(int64_t), platform_place_g_program_owner, 0));
        size_t *const members = (size_t *)demon_memory(
            platform_cell_taken((count + 1u) * sizeof(size_t), platform_place_g_program_owner, 0));
        size_t *const starts = (size_t *)demon_memory(
            platform_cell_taken((count + 1u) * sizeof(size_t), platform_place_g_program_owner, 0));
        size_t *const spans = (size_t *)demon_memory(
            platform_cell_taken((count + 1u) * sizeof(size_t), platform_place_g_program_owner, 0));
        size_t member_count = 0u;
        size_t filled = 0u;
        for (size_t at = lead; at < count; at += 1u)
        {
            if ((before[at].p != p) || (before[at].nu != nu))
            {
                continue;
            }
            grouped[at] = 1u;
            members[member_count] = at;
            starts[member_count] = filled;
            spans[member_count] = (size_t)(((before[at].jb - before[at].ja) << NARROW) + 1);
            for (int64_t j = before[at].ja << NARROW; j <= (before[at].jb << NARROW); j += 1)
            {
                js[filled] = j;
                filled += 1u;
            }
            member_count += 1u;
        }
        TuringPoint *const rows = (TuringPoint *)demon_memory(
            platform_cell_cleared(total + 1u, sizeof(TuringPoint), platform_place_g_program_owner, 0));
        *failed += turing_run_points(binary, constants, nu, p + NARROW, js, total, 1, rows);
        for (size_t m = 0u; m < member_count; m += 1u)
        {
            size_t changes = 0u;
            size_t change_a = 0u;
            size_t change_b = 0u;
            int64_t last = -1;
            for (size_t i = 0u; i < spans[m]; i += 1u)
            {
                const size_t at = starts[m] + i;
                if (rows[at].sign.sign == 0)
                {
                    continue;
                }
                if ((last >= 0) && !orior_exact_equal(&rows[last].sign, &rows[at].sign))
                {
                    changes += 1u;
                    // The place of a row fits a machine integer.
                    change_a = (size_t)last;
                    change_b = at;
                }
                last = (int64_t)at;
            }
            const int single = changes == 1u;
            retained += single ? 0 : 1;
            if (single)
            {
                ZeroRecord *const record = &records[which[members[m]]];
                z_add(&rows[change_a].extra[0], &one, &record->low);
                z_add(&rows[change_b].extra[1], &one, &record->high);
                record->nu = nu;
                record->p = p + NARROW;
                record->ja = js[change_a];
                record->jb = js[change_b];
            }
        }
        platform_cell_released(rows, platform_place_g_program_owner);
        platform_cell_released(js, platform_place_g_program_owner);
        platform_cell_released(members, platform_place_g_program_owner);
        platform_cell_released(starts, platform_place_g_program_owner);
        platform_cell_released(spans, platform_place_g_program_owner);
    }
    platform_cell_released(grouped, platform_place_g_program_owner);
    platform_cell_released(before, platform_place_g_program_owner);
    return retained;
}

/** @brief Every curve over the zeros at their positions, integers at 2^-64. */
typedef struct
{
    OriorExactInteger covariance[LAGS + 1];    /**< The covariance of S at each lag. */
    OriorExactInteger pull[2];                 /**< The pull, as a numerator and a denominator. */
    OriorExactInteger neighbors;               /**< The correlation of the gaps with their neighbors, its numerator. */
    OriorExactInteger variances[LENGTH_COUNT]; /**< The number variance at each length, its numerator. */
    int64_t bins[REACH * BINS];                /**< The pair counts by eighths. */
    int64_t windows;                           /**< The windows a length. */
    int64_t n;                                 /**< The values of S. */
} Curves;

/**
 * @brief The first place whose value reaches x in an ascending list, as bisect_left finds it.
 *
 * @param[in] values The list [BORROWS].
 * @param[in] count  How many.
 * @param[in] x      The value [BORROWS].
 * @return           The place.
 */
static size_t bisect_left(const OriorExactInteger *values, size_t count, const OriorExactInteger *x)
{
    size_t low = 0u;
    size_t high = count;
    while (low < high)
    {
        const size_t middle = (low + high) / 2u;
        if (z_cmp(&values[middle], x) < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

/**
 * @brief Every curve over the zeros at the positions, the first zero the N_start + 1-th of N.
 *
 * @param[in]  positions The positions [BORROWS].
 * @param[in]  count     How many.
 * @param[in]  n_start   N at the first F.
 * @param[out] curves    The curves [BORROWS].
 */
static void curves_of(const OriorExactInteger *positions, size_t count, int64_t n_start, Curves *curves)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = curves, .bytes = sizeof(*curves), .val = 0);
    const size_t n = count - 1u;
    // The count of zeros fits a machine integer.
    curves->n = (int64_t)n;
    OriorExactInteger wide;
    z_two_to(TURING_SCALE_BITS + 2, &wide);
    OriorExactInteger *const x = (OriorExactInteger *)demon_memory(
        platform_cell_taken((n + 1u) * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    OriorExactInteger total;
    orior_exact_zero(&total);
    for (size_t k = 0u; k < n; k += 1u)
    {
        // k fits a machine integer.
        z_mul_small(&wide, 2 * (n_start + (int64_t)k + 1), &x[k]);
        z_sub(&x[k], &positions[k], &x[k]);
        z_sub(&x[k], &positions[k + 1u], &x[k]);
        z_add(&total, &x[k], &total);
    }
    for (size_t k = 0u; k < n; k += 1u)
    {
        z_mul_small(&x[k], curves->n, &x[k]);
        z_sub(&x[k], &total, &x[k]);
    }
    for (size_t lag = 0u; lag <= LAGS; lag += 1u)
    {
        orior_exact_zero(&curves->covariance[lag]);
        for (size_t i = 0u; (i + lag) < n; i += 1u)
        {
            OriorExactInteger product;
            z_mul(&x[i], &x[i + lag], &product);
            z_add(&curves->covariance[lag], &product, &curves->covariance[lag]);
        }
    }
    OriorExactInteger last_square;
    z_mul(&x[n - 1u], &x[n - 1u], &last_square);
    OriorExactInteger adjacent;
    orior_exact_zero(&adjacent);
    for (size_t i = 0u; (i + 1u) < n; i += 1u)
    {
        OriorExactInteger product;
        z_mul(&x[i], &x[i + 1u], &product);
        z_add(&adjacent, &product, &adjacent);
    }
    z_sub(&adjacent, &curves->covariance[0], &curves->pull[0]);
    z_add(&curves->pull[0], &last_square, &curves->pull[0]);
    z_sub(&curves->covariance[0], &last_square, &curves->pull[1]);
    OriorExactInteger gap_total;
    z_sub(&positions[n], &positions[0], &gap_total);
    OriorExactInteger previous;
    orior_exact_zero(&curves->neighbors);
    for (size_t k = 0u; k < n; k += 1u)
    {
        OriorExactInteger y;
        z_sub(&positions[k + 1u], &positions[k], &y);
        z_mul_small(&y, curves->n, &y);
        z_sub(&y, &gap_total, &y);
        if (k > 0u)
        {
            OriorExactInteger product;
            z_mul(&previous, &y, &product);
            z_add(&curves->neighbors, &product, &curves->neighbors);
        }
        previous = y;
    }
    OriorExactInteger stop;
    z_mul_small(&wide, LENGTHS[LENGTH_COUNT - 1], &stop);
    z_sub(&positions[n], &stop, &stop);
    OriorExactInteger step;
    z_two_to(TURING_SCALE_BITS + 1, &step);
    size_t window_count = 0u;
    OriorExactInteger at = positions[0];
    size_t window_capacity = 1024u;
    OriorExactInteger *starts = (OriorExactInteger *)demon_memory(
        platform_cell_taken(window_capacity * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    while (z_cmp(&at, &stop) < 0)
    {
        if (window_count >= window_capacity)
        {
            window_capacity *= 2u;
            starts = (OriorExactInteger *)demon_memory(
                platform_cell_grown(starts, (window_capacity / 2u) * sizeof(OriorExactInteger),
                                    window_capacity * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
        }
        starts[window_count] = at;
        window_count += 1u;
        z_add(&at, &step, &at);
    }
    // The count of windows fits a machine integer.
    curves->windows = (int64_t)window_count;
    for (int length = 0; length < LENGTH_COUNT; length += 1)
    {
        OriorExactInteger reach;
        z_mul_small(&wide, LENGTHS[length], &reach);
        int64_t sum = 0;
        OriorExactInteger squares;
        orior_exact_zero(&squares);
        for (size_t w = 0u; w < window_count; w += 1u)
        {
            OriorExactInteger end;
            z_add(&starts[w], &reach, &end);
            // The counts of zeros fit a machine integer.
            const int64_t c =
                (int64_t)bisect_left(positions, count, &end) - (int64_t)bisect_left(positions, count, &starts[w]);
            sum += c;
            OriorExactInteger square;
            z_set(&square, c * c);
            z_add(&squares, &square, &squares);
        }
        z_mul_small(&squares, curves->windows, &curves->variances[length]);
        OriorExactInteger sum_square;
        z_set(&sum_square, sum);
        z_mul(&sum_square, &sum_square, &sum_square);
        z_sub(&curves->variances[length], &sum_square, &curves->variances[length]);
    }
    platform_cell_released(starts, platform_place_g_program_owner);
    OriorExactInteger span;
    z_mul_small(&wide, REACH, &span);
    for (size_t i = 0u; i < count; i += 1u)
    {
        OriorExactInteger end;
        z_add(&positions[i], &span, &end);
        const size_t top = bisect_left(positions, count, &end);
        for (size_t j = i + 1u; j < top; j += 1u)
        {
            OriorExactInteger difference;
            z_sub(&positions[j], &positions[i], &difference);
            z_mul_small(&difference, BINS, &difference);
            z_floor_div(&difference, &wide, &difference);
            curves->bins[z_small(&difference)] += 1;
        }
    }
    platform_cell_released(x, platform_place_g_program_owner);
}

/**
 * @brief Whether two sets of curves are equal.
 *
 * @param[in] a First [BORROWS].
 * @param[in] b Second [BORROWS].
 * @return      1 where they are.
 */
static int curves_equal(const Curves *a, const Curves *b)
{
    int equal = (a->n == b->n) && (a->windows == b->windows);
    for (int at = 0; at <= LAGS; at += 1)
    {
        equal = equal && orior_exact_equal(&a->covariance[at], &b->covariance[at]);
    }
    equal = equal && orior_exact_equal(&a->pull[0], &b->pull[0]) && orior_exact_equal(&a->pull[1], &b->pull[1]);
    equal = equal && orior_exact_equal(&a->neighbors, &b->neighbors);
    for (int at = 0; at < LENGTH_COUNT; at += 1)
    {
        equal = equal && orior_exact_equal(&a->variances[at], &b->variances[at]);
    }
    for (int at = 0; at < (REACH * BINS); at += 1)
    {
        equal = equal && (a->bins[at] == b->bins[at]);
    }
    return equal;
}

/**
 * @brief The zeros again with the order of their gaps shuffled within each run of a block of gaps, every gap kept.
 *
 * @param[in]     positions The positions [BORROWS].
 * @param[in]     count     How many.
 * @param[in,out] twister   The generator, or NULL for the identity [BORROWS].
 * @param[in]     block     The run.
 * @param[out]    out       The positions drawn [BORROWS].
 */
static void permuted(const OriorExactInteger *positions, size_t count, ZetaTwister *twister, size_t block,
                     OriorExactInteger *out)
{
    const size_t gap_count = count - 1u;
    OriorExactInteger *const gaps = (OriorExactInteger *)demon_memory(
        platform_cell_taken(gap_count * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    for (size_t k = 0u; k < gap_count; k += 1u)
    {
        z_sub(&positions[k + 1u], &positions[k], &gaps[k]);
    }
    if (twister != NULL)
    {
        for (size_t at = 0u; at < gap_count; at += block)
        {
            const size_t length = ((at + block) < gap_count) ? block : (gap_count - at);
            for (size_t i = length - 1u; (length > 1u) && (i > 0u); i -= 1u)
            {
                // The run is small, and the index fits a uint32_t.
                const size_t other = twister_below(twister, (uint32_t)(i + 1u));
                const OriorExactInteger held = gaps[at + i];
                gaps[at + i] = gaps[at + other];
                gaps[at + other] = held;
            }
        }
    }
    out[0] = positions[0];
    for (size_t k = 0u; k < gap_count; k += 1u)
    {
        z_add(&out[k], &gaps[k], &out[k + 1u]);
    }
    platform_cell_released(gaps, platform_place_g_program_owner);
}

/**
 * @brief Orders two exact integers.
 *
 * @param[in] left  First [BORROWS].
 * @param[in] right Second [BORROWS].
 * @return          -1, 0 or 1.
 */
static int exact_order(const void *left, const void *right)
{
    return z_cmp((const OriorExactInteger *)left, (const OriorExactInteger *)right);
}

/**
 * @brief Whether each run of the drawn positions holds the same gaps as the run of the real positions.
 *
 * @param[in] positions The real positions [BORROWS].
 * @param[in] drawn     The drawn positions [BORROWS].
 * @param[in] count     How many.
 * @param[in] block     The run.
 * @return              1 where every run keeps its gaps.
 */
static int runs_kept(const OriorExactInteger *positions, const OriorExactInteger *drawn, size_t count, size_t block)
{
    OriorExactInteger *const one = (OriorExactInteger *)demon_memory(
        platform_cell_taken((block + 1u) * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    OriorExactInteger *const two = (OriorExactInteger *)demon_memory(
        platform_cell_taken((block + 1u) * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    int kept = 1;
    for (size_t at = 0u; (at + 1u) < count; at += block)
    {
        const size_t end = ((at + block + 1u) < count) ? (at + block + 1u) : count;
        const size_t length = end - at - 1u;
        for (size_t k = 0u; k < length; k += 1u)
        {
            z_sub(&positions[at + k + 1u], &positions[at + k], &one[k]);
            z_sub(&drawn[at + k + 1u], &drawn[at + k], &two[k]);
        }
        qsort(one, length, sizeof(OriorExactInteger), exact_order);
        qsort(two, length, sizeof(OriorExactInteger), exact_order);
        for (size_t k = 0u; k < length; k += 1u)
        {
            kept = kept && orior_exact_equal(&one[k], &two[k]);
        }
    }
    platform_cell_released(one, platform_place_g_program_owner);
    platform_cell_released(two, platform_place_g_program_owner);
    return kept;
}

/**
 * @brief The pairs of zeros within the reach by the interval of their difference.
 *
 * @param[in]  intervals The intervals, two values a zero [BORROWS].
 * @param[in]  count     How many zeros.
 * @param[out] decided   By bin, the pairs whose whole interval lies in it [BORROWS].
 * @param[out] across    By bin, the pairs whose interval meets it and another bin besides [BORROWS].
 * @param[out] open      Which zeros belong to such a pair [BORROWS]. count of them.
 * @return               How many zeros belong to such a pair.
 */
static size_t pair_ranges(const ZeroRecord *intervals, size_t count, int64_t decided[REACH * BINS],
                          int64_t across[REACH * BINS], unsigned char *open)
{
    const int64_t last = (REACH * BINS) - 1;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = decided, .bytes = (size_t)(REACH * BINS) * sizeof(int64_t), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = across, .bytes = (size_t)(REACH * BINS) * sizeof(int64_t), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = open, .bytes = count, .val = 0);
    OriorExactInteger one;
    z_two_to(TURING_SCALE_BITS, &one);
    OriorExactInteger reach;
    z_mul_small(&one, REACH, &reach);
    OriorExactInteger *const lows = (OriorExactInteger *)demon_memory(
        platform_cell_taken((count + 1u) * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    for (size_t at = 0u; at < count; at += 1u)
    {
        lows[at] = intervals[at].low;
    }
    for (size_t i = 0u; i < count; i += 1u)
    {
        OriorExactInteger end;
        z_add(&intervals[i].high, &reach, &end);
        const size_t top = bisect_left(lows, count, &end);
        for (size_t j = i + 1u; j < top; j += 1u)
        {
            OriorExactInteger value;
            z_sub(&intervals[j].low, &intervals[i].high, &value);
            if (value.sign < 0)
            {
                orior_exact_zero(&value);
            }
            z_mul_small(&value, BINS, &value);
            z_floor_div(&value, &one, &value);
            const int64_t low = z_small(&value);
            z_sub(&intervals[j].high, &intervals[i].low, &value);
            z_mul_small(&value, BINS, &value);
            z_floor_div(&value, &one, &value);
            const int64_t high = z_small(&value);
            const int whole = low == high;
            for (int64_t at = low; at <= ((high < last) ? high : last); at += 1)
            {
                decided[at] += whole;
                across[at] += 1 - whole;
            }
            if (!whole && (low <= last))
            {
                open[i] = 1u;
                open[j] = 1u;
            }
        }
    }
    platform_cell_released(lows, platform_place_g_program_owner);
    size_t total = 0u;
    for (size_t at = 0u; at < count; at += 1u)
    {
        total += open[at];
    }
    return total;
}

/** @brief The arms of the demon: their names, and the curves of each draw. */
typedef struct
{
    const char *name; /**< The name. */
    Curves *draws;    /**< The curves of each draw [OWNS]. */
} Arm;

/**
 * @brief The reach of the real among the draws, by a comparison: the fewer of the draws at or past it on each side.
 *
 * @param[in]  real    The real curves [BORROWS].
 * @param[in]  draws   The draws [BORROWS].
 * @param[in]  above   1 where its first argument is above its second.
 * @param[in]  context The point of the curve.
 * @param[out] side    '+' where the real is above most of the draws, else '-' [BORROWS].
 * @return             The reach.
 */
static int reach_of(const Curves *real, const Curves *draws, int (*above)(const Curves *, const Curves *, int),
                    int context, char *side)
{
    int up = 0;
    int down = 0;
    for (int at = 0; at < DRAWS; at += 1)
    {
        up += 1 - above(real, &draws[at], context);
        down += 1 - above(&draws[at], real, context);
    }
    *side = (up < down) ? '+' : '-';
    return (up < down) ? up : down;
}

/** @brief The comparisons of the curves, each at a point of the curve. */
static int above_lag(const Curves *a, const Curves *b, int lag)
{
    OriorExactInteger left;
    OriorExactInteger right;
    z_mul(&a->covariance[lag], &b->covariance[0], &left);
    z_mul(&b->covariance[lag], &a->covariance[0], &right);
    return z_cmp(&left, &right) > 0;
}

static int above_pull(const Curves *a, const Curves *b, int unused)
{
    (void)unused;
    OriorExactInteger left;
    OriorExactInteger right;
    z_mul(&a->pull[0], &b->pull[1], &left);
    z_mul(&b->pull[0], &a->pull[1], &right);
    return z_cmp(&left, &right) > 0;
}

static int above_neighbors(const Curves *a, const Curves *b, int unused)
{
    (void)unused;
    return z_cmp(&a->neighbors, &b->neighbors) > 0;
}

static int above_variance(const Curves *a, const Curves *b, int at)
{
    return z_cmp(&a->variances[at], &b->variances[at]) > 0;
}

static int above_bin(const Curves *a, const Curves *b, int at)
{
    return a->bins[at] > b->bins[at];
}

/**
 * @brief The mean of rationals, as a numerator and a denominator: their reduced sum over the count.
 *
 * @param[in]  values The rationals [BORROWS].
 * @param[in]  count  How many.
 * @param[out] num    The numerator [BORROWS].
 * @param[out] den    The denominator [BORROWS].
 */
static void mean_of(const ZetaRational *values, int count, OriorExactInteger *num, OriorExactInteger *den)
{
    ZetaRational total;
    rational_small(0, 1, &total);
    for (int at = 0; at < count; at += 1)
    {
        rational_add(&total, &values[at], &total);
    }
    *num = total.top;
    z_mul_small(&total.under, count, den);
}

/**
 * @brief The real value of each curve beside the mean of the draws of each arm, with the reach and the side.
 *
 * @param[in] real      The real curves [BORROWS].
 * @param[in] arms      The arms [BORROWS].
 * @param[in] positions The positions [BORROWS].
 * @param[in] count     How many positions.
 * @param[in] decided   The pairs in one bin [BORROWS].
 * @param[in] across    The pairs across bins [BORROWS].
 */
static void report(const Curves *real, const Arm arms[2], const OriorExactInteger *positions, size_t count,
                   const int64_t *decided, const int64_t *across)
{
    const int64_t n = real->n;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the arms: a, ");
        mmgr_line_put(output_line, arms[0].name);
        mmgr_line_put(output_line, "; b, ");
        mmgr_line_put(output_line, arms[1].name);
        mmgr_line_put(output_line, ". Each column: the draws' mean, the reach, the side");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  S's correlation over k zeros: k, the real, then each arm:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    static char outside[2][LAGS * 8];
    outside[0][0] = '\0';
    outside[1][0] = '\0';
    for (int lag = 1; lag <= LAGS; lag += 1)
    {
        ZetaRational ratio;
        rational_small(n, n - lag, &ratio);
        ZetaRational value;
        rational_make(&real->covariance[lag], &real->covariance[0], &value);
        ZetaRational rho;
        rational_mul(&ratio, &value, &rho);
        char rho_text[64];
        show(&rho.top, &rho.under, 4, rho_text, sizeof(rho_text));
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    k ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)(lag);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 2u);
            }
            mmgr_line_put(output_line, ": ");
            mmgr_line_put_right(output_line, rho_text, 8u);
            mmgr_line_put(output_line, "   ");
            platform_line_out_ended();
        }
        for (int arm = 0; arm < 2; arm += 1)
        {
            ZetaRational values[DRAWS];
            for (int d = 0; d < DRAWS; d += 1)
            {
                rational_make(&arms[arm].draws[d].covariance[lag], &arms[arm].draws[d].covariance[0], &value);
                rational_mul(&ratio, &value, &values[d]);
            }
            OriorExactInteger num;
            OriorExactInteger den;
            mean_of(values, DRAWS, &num, &den);
            char side = '-';
            const int reach = reach_of(real, arms[arm].draws, above_lag, lag, &side);
            if (reach == 0)
            {
                char mark[16];
                snprintf(mark, sizeof(mark), "%s%c%d", (outside[arm][0] != '\0') ? " " : "", side, lag);
                // the mark appended where the text ends, cut at the room that the text has
                const size_t held =
                    EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = outside[arm], .cap = PLATFORM_TEXT_TO_TERMINATOR);
                (void)EMBED_CALL(cellul.copy, CatenaFinitaCfg, .src = mark, .cap = sizeof(outside[arm]) - held,
                                 .dst = &outside[arm][held]);
            }
            char mean_text[64];
            show(&num, &den, 4, mean_text, sizeof(mean_text));
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, (arm > 0) ? "   " : "");
                mmgr_line_put_right(output_line, mean_text, 8u);
                mmgr_line_put(output_line, " ");
                {
                    char linea_piece[24u];
                    VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                    const int64_t linea_value = (int64_t)(reach);
                    mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                    (void)mmgr_line_finish(&linea_piece_line);
                    mmgr_line_put_right(output_line, linea_piece, 2u);
                }
                mmgr_line_put(output_line, " ");
                mmgr_line_ch(output_line, (char)(side));
                platform_line_out_ended();
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    for (int arm = 0; arm < 2; arm += 1)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  lags no draw of ");
        mmgr_line_ch(output_line, (char)('a' + arm));
        mmgr_line_put(output_line, " reaches, with their side: ");
        mmgr_line_put(output_line, outside[arm]);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    char text[128];
    show(&real->pull[0], &real->pull[1], 4, text, sizeof(text));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  S's pull back each zero ");
        mmgr_line_put(output_line, text);
        mmgr_line_put(output_line, ";   ");
        platform_line_out_ended();
    }
    for (int arm = 0; arm < 2; arm += 1)
    {
        ZetaRational values[DRAWS];
        for (int d = 0; d < DRAWS; d += 1)
        {
            rational_make(&arms[arm].draws[d].pull[0], &arms[arm].draws[d].pull[1], &values[d]);
        }
        OriorExactInteger num;
        OriorExactInteger den;
        mean_of(values, DRAWS, &num, &den);
        char side = '-';
        const int reach = reach_of(real, arms[arm].draws, above_pull, 0, &side);
        show(&num, &den, 4, text, sizeof(text));
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, (arm > 0) ? "   " : "");
            mmgr_line_put(output_line, text);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_signed(output_line, reach);
            mmgr_line_put(output_line, " ");
            mmgr_line_ch(output_line, (char)(side));
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    OriorExactInteger square;
    orior_exact_zero(&square);
    OriorExactInteger span;
    z_sub(&positions[count - 1u], &positions[0], &span);
    for (size_t k = 0u; (k + 1u) < count; k += 1u)
    {
        OriorExactInteger gap;
        z_sub(&positions[k + 1u], &positions[k], &gap);
        z_mul_small(&gap, n, &gap);
        z_sub(&gap, &span, &gap);
        z_mul(&gap, &gap, &gap);
        z_add(&square, &gap, &square);
    }
    z_mul_small(&square, n - 1, &square);
    OriorExactInteger value;
    z_mul_small(&real->neighbors, n, &value);
    show(&value, &square, 4, text, sizeof(text));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the gaps' neighbors' correlation ");
        mmgr_line_put(output_line, text);
        mmgr_line_put(output_line, ";   ");
        platform_line_out_ended();
    }
    for (int arm = 0; arm < 2; arm += 1)
    {
        OriorExactInteger total;
        orior_exact_zero(&total);
        for (int d = 0; d < DRAWS; d += 1)
        {
            z_add(&total, &arms[arm].draws[d].neighbors, &total);
        }
        z_mul_small(&total, n, &total);
        OriorExactInteger under;
        z_mul_small(&square, DRAWS, &under);
        char side = '-';
        const int reach = reach_of(real, arms[arm].draws, above_neighbors, 0, &side);
        show(&total, &under, 4, text, sizeof(text));
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, (arm > 0) ? "   " : "");
            mmgr_line_put(output_line, text);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_signed(output_line, reach);
            mmgr_line_put(output_line, " ");
            mmgr_line_ch(output_line, (char)(side));
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int64_t windows = real->windows;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  number variance over windows of L on u, ");
        mmgr_line_decimal_signed(output_line, (long long)windows);
        mmgr_line_put(output_line, " windows a length: L, the real, then each arm:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (int at = 0; at < LENGTH_COUNT; at += 1)
    {
        OriorExactInteger under;
        z_set(&under, windows * windows);
        show(&real->variances[at], &under, 4, text, sizeof(text));
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    L ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)LENGTHS[at]);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 3u);
            }
            mmgr_line_put(output_line, ": ");
            mmgr_line_put_right(output_line, text, 9u);
            mmgr_line_put(output_line, "   ");
            platform_line_out_ended();
        }
        for (int arm = 0; arm < 2; arm += 1)
        {
            OriorExactInteger total;
            orior_exact_zero(&total);
            for (int d = 0; d < DRAWS; d += 1)
            {
                z_add(&total, &arms[arm].draws[d].variances[at], &total);
            }
            z_set(&under, windows * windows * DRAWS);
            char side = '-';
            const int reach = reach_of(real, arms[arm].draws, above_variance, at, &side);
            show(&total, &under, 4, text, sizeof(text));
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, (arm > 0) ? "   " : "");
                mmgr_line_put_right(output_line, text, 9u);
                mmgr_line_put(output_line, " ");
                {
                    char linea_piece[24u];
                    VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                    const int64_t linea_value = (int64_t)(reach);
                    mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                    (void)mmgr_line_finish(&linea_piece_line);
                    mmgr_line_put_right(output_line, linea_piece, 2u);
                }
                mmgr_line_put(output_line, " ");
                mmgr_line_ch(output_line, (char)(side));
                platform_line_out_ended();
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    int held = 1;
    for (int b = 0; b < (REACH * BINS); b += 1)
    {
        held = held && (decided[b] <= real->bins[b]) && (real->bins[b] <= (decided[b] + across[b]));
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  pair counts a zero over differences of u by eighths: the bin, the real, the "
                                   "pairs whose difference's interval lies in the bin and those meeting it and "
                                   "another, then each arm. Every real count between the two: ");
        mmgr_line_put(output_line, held ? "True" : "False");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    static char marks[2][REACH * BINS][4];
    for (int b = 0; b < (REACH * BINS); b += 1)
    {
        char columns[2][128];
        for (int arm = 0; arm < 2; arm += 1)
        {
            char side = '-';
            const int reach = reach_of(real, arms[arm].draws, above_bin, b, &side);
            int all_below = 1;
            int all_above = 1;
            int64_t total = 0;
            for (int d = 0; d < DRAWS; d += 1)
            {
                const int64_t count_here = arms[arm].draws[d].bins[b];
                all_below = all_below && (count_here < decided[b]);
                all_above = all_above && (count_here > (decided[b] + across[b]));
                total += count_here;
            }
            const int whole = all_below || all_above;
            snprintf(marks[arm][b], sizeof(marks[arm][b]), "%c%c", side, whole ? '#' : ((reach == 0) ? '*' : ' '));
            show_small(total, n * DRAWS, 4, text, sizeof(text));
            snprintf(columns[arm], sizeof(columns[arm]), "%.100s %2d %c", text, reach, side);
        }
        if ((b < (2 * BINS)) || ((b % BINS) == 0))
        {
            char texts[5][64];
            show_small(b, BINS, 3, texts[0], sizeof(texts[0]));
            show_small(b + 1, BINS, 3, texts[1], sizeof(texts[1]));
            show_small(real->bins[b], n, 4, texts[2], sizeof(texts[2]));
            show_small(decided[b], n, 4, texts[3], sizeof(texts[3]));
            show_small(across[b], n, 4, texts[4], sizeof(texts[4]));
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    [");
                mmgr_line_put(output_line, texts[0]);
                mmgr_line_put(output_line, ", ");
                mmgr_line_put(output_line, texts[1]);
                mmgr_line_put(output_line, "): ");
                mmgr_line_put(output_line, texts[2]);
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, texts[3]);
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, texts[4]);
                mmgr_line_put(output_line, "   ");
                mmgr_line_put(output_line, columns[0]);
                mmgr_line_put(output_line, "   ");
                mmgr_line_put(output_line, columns[1]);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }
    for (int arm = 0; arm < 2; arm += 1)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  every eighth against ");
            mmgr_line_ch(output_line, (char)('a' + arm));
            mmgr_line_put(output_line, ", its side, '*' where no draw reaches the real and '#' where none reaches any "
                                       "count the intervals allow:");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (int row = 0; row < REACH; row += 1)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    ");
                mmgr_line_decimal_signed(output_line, row);
                mmgr_line_put(output_line, " to ");
                mmgr_line_decimal_signed(output_line, row + 1);
                mmgr_line_put(output_line, ":");
                platform_line_out_ended();
            }
            for (int b = row * BINS; b < ((row + 1) * BINS); b += 1)
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                mmgr_line_put(output_line, marks[arm][b]);
                platform_line_out_ended();
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }
}

int program_main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argument_count < 4)
    {
        fprintf(stderr, "usage: exact_zeta_demon <binary> <first> <last> [extra] [N at first's F N at last's F]\n");
        return 2;
    }
    const char *const binary = arguments[1];
    const int64_t first = atoll(arguments[2]);
    const int64_t last = atoll(arguments[3]);
    const int64_t extra = (argument_count > 4) ? atoll(arguments[4]) : 2;
    const int has_given = argument_count > 6;
    TuringConstants constants;
    turing_constants(&constants);
    TuringCell *const cells = (TuringCell *)demon_memory(
        platform_cell_cleared((size_t)last + 2u, sizeof(TuringCell), platform_place_g_program_owner, 0));
    BracketedPoint *points = NULL;
    const size_t point_count = bracketed_points(binary, &constants, first, last, extra, cells, &points);
    int64_t start_nu = first;
    int64_t end_nu = last;
    int64_t n_start = 0;
    int64_t n_end = 0;
    if (has_given)
    {
        n_start = atoll(arguments[5]);
        n_end = atoll(arguments[6]);
    }
    else
    {
        int64_t tight[4096];
        int64_t lows[4096];
        size_t tight_count = 0u;
        for (int64_t nu = first + 1; nu < last; nu += 1)
        {
            ZetaRational low;
            ZetaRational high;
            turing_below(&cells[nu], &cells[nu - 1], &low);
            turing_above(&cells[nu], &cells[nu + 1], &high);
            OriorExactInteger low_value;
            OriorExactInteger high_value;
            z_neg(&low.top, &low_value);
            z_floor_div(&low_value, &low.under, &low_value);
            z_neg(&low_value, &low_value);
            z_floor_div(&high.top, &high.under, &high_value);
            if (orior_exact_equal(&low_value, &high_value) && (tight_count < 4096u))
            {
                tight[tight_count] = nu;
                lows[tight_count] = z_small(&low_value);
                tight_count += 1u;
            }
        }
        if (tight_count < 2u)
        {
            fprintf(stderr, "  N is not held to one value at two F's; give N at the first and last cells' F\n");
            return 1;
        }
        start_nu = tight[0];
        end_nu = tight[tight_count - 1u];
        n_start = lows[0];
        n_end = lows[tight_count - 1u];
    }
    size_t start = 0u;
    size_t end = 0u;
    for (size_t at = 0u; at < point_count; at += 1u)
    {
        if ((points[at].nu == start_nu) && (points[at].lane == cells[start_nu].first))
        {
            start = at;
            break;
        }
    }
    for (size_t at = 0u; at < point_count; at += 1u)
    {
        if ((points[at].nu == end_nu) && (points[at].lane == cells[end_nu].first))
        {
            end = at;
            break;
        }
    }
    ZeroRecord *records = NULL;
    int64_t failed = 0;
    const size_t count =
        zero_records(binary, &constants, points, start, end, n_end - n_start, extra, &records, &failed);
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        failed += cells[nu].failed;
    }
    // The count of zeros fits a machine integer.
    if ((int64_t)count != (n_end - n_start))
    {
        fprintf(stderr, "  the zeros placed, %llu, do not number N's difference, %lld\n", (unsigned long long)count,
                (long long)(n_end - n_start));
        return 1;
    }
    static int64_t decided[REACH * BINS];
    static int64_t across[REACH * BINS];
    unsigned char *const open =
        (unsigned char *)demon_memory(platform_cell_cleared(count + 1u, 1u, platform_place_g_program_owner, 0));
    size_t *const which =
        (size_t *)demon_memory(platform_cell_taken((count + 1u) * sizeof(size_t), platform_place_g_program_owner, 0));
    for (int turn = 0; turn <= ROUNDS; turn += 1)
    {
        const size_t open_count = pair_ranges(records, count, decided, across, open);
        int64_t decided_total = 0;
        for (int b = 0; b < (REACH * BINS); b += 1)
        {
            decided_total += decided[b];
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  round ");
            mmgr_line_decimal_signed(output_line, turn);
            mmgr_line_put(output_line, ": pairs within ");
            mmgr_line_decimal_signed(output_line, REACH);
            mmgr_line_put(output_line, " whose difference lies in one eighth ");
            mmgr_line_decimal_signed(output_line, (long long)decided_total);
            mmgr_line_put(output_line, "; zeros in a pair meeting two ");
            mmgr_line_decimal(output_line, (unsigned long long)open_count, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_line_flushed();
        if ((turn == ROUNDS) || (open_count == 0u))
        {
            break;
        }
        size_t which_count = 0u;
        for (size_t at = 0u; at < count; at += 1u)
        {
            if (open[at])
            {
                which[which_count] = at;
                which_count += 1u;
            }
        }
        int64_t bad = 0;
        const int64_t retained = narrowed(binary, &constants, records, which, which_count, &bad);
        failed += bad;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    listed again ");
            mmgr_line_decimal_signed(output_line, 1 << NARROW);
            mmgr_line_put(output_line, " times finer: ");
            mmgr_line_decimal(output_line, (unsigned long long)open_count, 1u);
            mmgr_line_put(output_line, " zeros, ");
            mmgr_line_decimal_signed(output_line, (long long)retained);
            mmgr_line_put(output_line, " retained their interval, host checks failed ");
            mmgr_line_decimal_signed(output_line, (long long)bad);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_line_flushed();
    }
    OriorExactInteger one;
    z_two_to(TURING_SCALE_BITS, &one);
    OriorExactInteger widest;
    orior_exact_zero(&widest);
    int ordered = 1;
    for (size_t at = 0u; at < count; at += 1u)
    {
        OriorExactInteger width;
        z_sub(&records[at].high, &records[at].low, &width);
        widest = ((at == 0u) || (z_cmp(&width, &widest) > 0)) ? width : widest;
        if ((at + 1u) < count)
        {
            ordered = ordered && (z_cmp(&records[at].low, &records[at + 1u].low) < 0) &&
                      (z_cmp(&records[at].high, &records[at + 1u].high) < 0);
        }
    }
    char widest_text[64];
    show(&widest, &one, 6, widest_text, sizeof(widest_text));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  from cell ");
        mmgr_line_decimal_signed(output_line, (long long)start_nu);
        mmgr_line_put(output_line, "'s F, N = ");
        mmgr_line_decimal_signed(output_line, (long long)n_start);
        mmgr_line_put(output_line, ", to cell ");
        mmgr_line_decimal_signed(output_line, (long long)end_nu);
        mmgr_line_put(output_line, "'s, N = ");
        mmgr_line_decimal_signed(output_line, (long long)n_end);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, (unsigned long long)count, 1u);
        mmgr_line_put(output_line, " zeros, each an interval of u, the widest ");
        mmgr_line_put(output_line, widest_text);
        mmgr_line_put(output_line, ", both ends rising from each to the next: ");
        mmgr_line_put(output_line, ordered ? "True" : "False");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    OriorExactInteger *const positions = (OriorExactInteger *)demon_memory(
        platform_cell_taken(count * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    for (size_t at = 0u; at < count; at += 1u)
    {
        z_add(&records[at].low, &records[at].high, &positions[at]);
        z_mul_small(&positions[at], 2, &positions[at]);
    }
    static Curves real;
    curves_of(positions, count, n_start, &real);
    OriorExactInteger *const drawn = (OriorExactInteger *)demon_memory(
        platform_cell_taken(count * sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
    permuted(positions, count, NULL, 1u, drawn);
    static Curves identity_curves;
    curves_of(drawn, count, n_start, &identity_curves);
    const int identity = curves_equal(&identity_curves, &real);
    ZetaTwister twister;
    twister_seed(&twister, DEMON_SEED);
    Arm arms[2];
    arms[0].name = "the gaps shuffled";
    arms[1].name = "the gaps shuffled within runs of 64";
    const size_t blocks[2] = {count, BLOCK};
    int retained = 1;
    for (int arm = 0; arm < 2; arm += 1)
    {
        arms[arm].draws =
            (Curves *)demon_memory(platform_cell_taken(DRAWS * sizeof(Curves), platform_place_g_program_owner, 0));
        for (int d = 0; d < DRAWS; d += 1)
        {
            permuted(positions, count, &twister, blocks[arm], drawn);
            retained = retained && runs_kept(positions, drawn, count, blocks[arm]);
            curves_of(drawn, count, n_start, &arms[arm].draws[d]);
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the identity permutation gives back every real value: ");
        mmgr_line_put(output_line, identity ? "True" : "False");
        mmgr_line_put(output_line, "; every draw retains each run's gaps: ");
        mmgr_line_put(output_line, retained ? "True" : "False");
        mmgr_line_put(output_line, "; ");
        mmgr_line_decimal_signed(output_line, DRAWS);
        mmgr_line_put(output_line, " draws an arm");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    report(&real, arms, positions, count, decided, across);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  host checks failed: ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(arms[0].draws, platform_place_g_program_owner);
    platform_cell_released(arms[1].draws, platform_place_g_program_owner);
    platform_cell_released(positions, platform_place_g_program_owner);
    platform_cell_released(drawn, platform_place_g_program_owner);
    platform_cell_released(records, platform_place_g_program_owner);
    platform_cell_released(points, platform_place_g_program_owner);
    platform_cell_released(cells, platform_place_g_program_owner);
    platform_cell_released(open, platform_place_g_program_owner);
    platform_cell_released(which, platform_place_g_program_owner);
    return ((failed != 0) || !identity || !retained) ? 1 : 0;
}
