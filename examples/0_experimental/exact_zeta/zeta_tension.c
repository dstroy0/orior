// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_tension.c
 * @brief The certified zeros, each placed, as zeta_tension.h states them.
 */
#include "zeta_tension.h"
#include "../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"

#include "memoria_operor/memoria_operor.h"
#include <math.h>
#include <stdlib.h>

/** @brief The bits by which a dip lists finer. */
#define FINER 6

/** @brief The most dips listed again, the deepest first. */
#define DIPS 200

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *tension_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

double tension_theta_pi(double s)
{
    return (s * log(s)) - s - 0.125 + (1.0 / (96.0 * 3.141592653589793 * 3.141592653589793 * s));
}

size_t tension_prime_powers(int64_t limit, int64_t (**powers)[2])
{
    unsigned char *const sieve =
        (unsigned char *)tension_memory(platform_cell_taken((size_t)limit + 1u, platform_place_g_program_owner, 0));
    EMBED_CALL(memor.set, MemoriaCfg, .dst = sieve, .bytes = (size_t)limit + 1u, .val = 1);
    size_t capacity = 256u;
    size_t count = 0u;
    *powers = (int64_t (*)[2])tension_memory(
        platform_cell_taken(capacity * sizeof(int64_t[2]), platform_place_g_program_owner, 0));
    for (int64_t p = 2; p <= limit; p += 1)
    {
        if (!sieve[p])
        {
            continue;
        }
        for (int64_t multiple = p * p; multiple <= limit; multiple += p)
        {
            sieve[multiple] = 0u;
        }
        int64_t power = p;
        for (int64_t r = 1; power <= limit; r += 1)
        {
            if (count >= capacity)
            {
                capacity *= 2u;
                *powers = (int64_t (*)[2])tension_memory(
                    platform_cell_grown(*powers, (capacity / 2u) * sizeof(int64_t[2]), capacity * sizeof(int64_t[2]),
                                        platform_place_g_program_owner, 0));
            }
            (*powers)[count][0] = p;
            (*powers)[count][1] = r;
            count += 1u;
            power *= p;
        }
    }
    platform_cell_released(sieve, platform_place_g_program_owner);
    return count;
}

/** @brief A listed point: its cell, its lane, s, the certified sign, and Z. */
typedef struct
{
    int64_t nu;   /**< The cell. */
    int64_t lane; /**< The lane. */
    double s;     /**< s = t / 2 pi. */
    int sign;     /**< The certified sign. */
    double z;     /**< Z. */
} ListedPoint;

/** @brief A dip: the size of Z and its point. */
typedef struct
{
    double size; /**< |Z|. */
    size_t at;   /**< The point. */
} Dip;

/**
 * @brief Orders two dips by |Z|, then by point.
 *
 * @param[in] left  First dip [BORROWS].
 * @param[in] right Second dip [BORROWS].
 * @return          -1, 0 or 1.
 */
static int dip_order(const void *left, const void *right)
{
    const Dip *const a = (const Dip *)left;
    const Dip *const b = (const Dip *)right;
    if (a->size != b->size)
    {
        return (a->size < b->size) ? -1 : 1;
    }
    return (a->at < b->at) ? -1 : ((a->at > b->at) ? 1 : 0);
}

/**
 * @brief Orders two doubles.
 *
 * @param[in] left  First [BORROWS].
 * @param[in] right Second [BORROWS].
 * @return          -1, 0 or 1.
 */
static int double_order(const void *left, const void *right)
{
    const double a = *(const double *)left;
    const double b = *(const double *)right;
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

/**
 * @brief Every listed point of the cells, in order, with the cells.
 *
 * @param[in]  binary    The device program [BORROWS].
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  first     The first cell.
 * @param[in]  last      The last cell.
 * @param[in]  extra     The steps finer than four points a zero.
 * @param[out] cells     The cells, by nu [BORROWS].
 * @param[out] points    The points [BORROWS]. Allocated.
 * @return               How many points.
 */
static size_t listed_points(const char *binary, const TuringConstants *constants, int64_t first, int64_t last,
                            int64_t extra, TuringCell *cells, ListedPoint **points)
{
    size_t capacity = 4096u;
    size_t count = 0u;
    *points = (ListedPoint *)tension_memory(
        platform_cell_taken(capacity * sizeof(ListedPoint), platform_place_g_program_owner, 0));
    OriorExactInteger unit;
    z_two_to(TURING_SCALE_BITS, &unit);
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        const int64_t p = turing_lattice(nu, 4) + extra;
        OriorExactInteger bound;
        turing_run_cell(binary, constants, nu, p, TURING_TRANSFORM, 0, 1, &cells[nu], &bound);
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
            if (found < 3u)
            {
                continue;
            }
            if (count >= capacity)
            {
                capacity *= 2u;
                *points = (ListedPoint *)tension_memory(
                    platform_cell_grown(*points, (capacity / 2u) * sizeof(ListedPoint), capacity * sizeof(ListedPoint),
                                        platform_place_g_program_owner, 0));
            }
            ListedPoint *const point = &(*points)[count];
            point->nu = nu;
            point->lane = lane;
            OriorExactInteger value;
            turing_parse_hex(words[1], &value);
            // S at a point is below 2^53, where a double holds it.
            point->s = (double)z_small(&value) / (double)((int64_t)1 << p);
            turing_parse_hex(words[0], &value);
            point->sign = (int)z_small(&value);
            turing_parse_hex(words[2], &value);
            ZetaRational ratio;
            rational_make(&value, &unit, &ratio);
            point->z = rational_to_double(&ratio);
            count += 1u;
            lane += 1;
        }
        fclose(handle);
        (void)remove(cells[nu].path);
    }
    return count;
}

/**
 * @brief The zeros between two points, until they number the count wanted.
 *
 * @note The zeros come at every change of sign of Z. Then they come in the deepest dips without one, listed again
 *       finer.
 *
 * @param[in]  binary    The device program [BORROWS].
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  points    The points [BORROWS].
 * @param[in]  start     The first point.
 * @param[in]  end       The last point.
 * @param[in]  wanted    The count wanted.
 * @param[in]  extra     The steps finer than four points a zero.
 * @param[out] zeros     The zeros, ascending [BORROWS]. Allocated.
 * @return               How many.
 */
static size_t place_zeros(const char *binary, const TuringConstants *constants, const ListedPoint *points, size_t start,
                          size_t end, int64_t wanted, int64_t extra, double **zeros)
{
    size_t capacity = (end - start) + 64u;
    *zeros =
        (double *)tension_memory(platform_cell_taken(capacity * sizeof(double), platform_place_g_program_owner, 0));
    size_t count = 0u;
    for (size_t i = start; i < end; i += 1u)
    {
        const double s0 = points[i].s;
        const double z0 = points[i].z;
        const double s1 = points[i + 1u].s;
        const double z1 = points[i + 1u].z;
        if ((z0 > 0.0) != (z1 > 0.0))
        {
            (*zeros)[count] = s0 + (((s1 - s0) * z0) / (z0 - z1));
            count += 1u;
        }
    }
    const size_t crossings = count;
    Dip *const dips = (Dip *)tension_memory(
        platform_cell_taken(((end - start) + 1u) * sizeof(Dip), platform_place_g_program_owner, 0));
    size_t dip_count = 0u;
    for (size_t i = start + 1u; i < end; i += 1u)
    {
        const int before = points[i - 1u].z > 0.0;
        const int here = points[i].z > 0.0;
        const int after = points[i + 1u].z > 0.0;
        const double size = fabs(points[i].z);
        if ((before == here) && (here == after) && (size < fabs(points[i - 1u].z)) && (size < fabs(points[i + 1u].z)))
        {
            dips[dip_count].size = size;
            dips[dip_count].at = i;
            dip_count += 1u;
        }
    }
    qsort(dips, dip_count, sizeof(Dip), dip_order);
    size_t added = 0u;
    int64_t listed = 0;
    for (size_t at = 0u; (at < dip_count) && (at < DIPS); at += 1u)
    {
        // The counts of zeros fit a machine integer.
        if ((int64_t)(count) >= wanted)
        {
            break;
        }
        const ListedPoint *const point = &points[dips[at].at];
        const int64_t p = turing_lattice(point->nu, 4) + extra;
        if ((point->lane == 0) || ((point->lane + 1) >= ((int64_t)1 << p)))
        {
            continue;
        }
        const size_t total = (size_t)2 << FINER;
        int64_t *const js =
            (int64_t *)tension_memory(platform_cell_taken(total * sizeof(int64_t), platform_place_g_program_owner, 0));
        for (size_t k = 0u; k < total; k += 1u)
        {
            // k is below 128.
            js[k] = ((point->lane - 1) << FINER) + (int64_t)k;
        }
        TuringPoint *const rows = (TuringPoint *)tension_memory(
            platform_cell_cleared(total, sizeof(TuringPoint), platform_place_g_program_owner, 0));
        const int64_t failed = turing_run_points(binary, constants, point->nu, p + FINER, js, total, 0, rows);
        if (failed != 0)
        {
            fprintf(stderr, "  a listed run's host check failed\n");
            exit(1);
        }
        listed += 1;
        int64_t previous_sign = 0;
        int64_t previous_s = 0;
        int have = 0;
        for (size_t k = 0u; k < total; k += 1u)
        {
            const int64_t sign = z_small(&rows[k].sign);
            if (sign == 0)
            {
                continue;
            }
            const int64_t s = z_small(&rows[k].s);
            if (have && (sign != previous_sign))
            {
                if (count >= capacity)
                {
                    capacity *= 2u;
                    *zeros = (double *)tension_memory(platform_cell_grown(*zeros, (capacity / 2u) * sizeof(double),
                                                                          capacity * sizeof(double),
                                                                          platform_place_g_program_owner, 0));
                }
                (*zeros)[count] = (0.5 * (double)(previous_s + s)) / (double)((int64_t)1 << (p + FINER));
                count += 1u;
                added += 1u;
            }
            previous_sign = sign;
            previous_s = s;
            have = 1;
        }
        platform_cell_released(rows, platform_place_g_program_owner);
        platform_cell_released(js, platform_place_g_program_owner);
    }
    platform_cell_released(dips, platform_place_g_program_owner);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  zeros at a change of sign ");
        mmgr_line_decimal(output_line, (unsigned long long)crossings, 1u);
        mmgr_line_put(output_line, ", dips listed again ");
        mmgr_line_decimal_signed(output_line, (long long)listed);
        mmgr_line_put(output_line, ", zeros found in them ");
        mmgr_line_decimal(output_line, (unsigned long long)added, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    qsort(*zeros, count, sizeof(double), double_order);
    return count;
}

void tension_certified_zeros(const char *binary, int64_t first, int64_t last, int64_t extra, const int64_t *given,
                             TensionZeros *zeros)
{
    TuringConstants constants;
    turing_constants(&constants);
    TuringCell *const cells = (TuringCell *)tension_memory(
        platform_cell_cleared((size_t)last + 2u, sizeof(TuringCell), platform_place_g_program_owner, 0));
    ListedPoint *points = NULL;
    const size_t count = listed_points(binary, &constants, first, last, extra, cells, &points);
    int64_t start_nu = first;
    int64_t end_nu = last;
    int64_t n_start = 0;
    int64_t n_end = 0;
    if (given != NULL)
    {
        n_start = given[0];
        n_end = given[1];
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cells ");
            mmgr_line_decimal_signed(output_line, (long long)first);
            mmgr_line_put(output_line, " to ");
            mmgr_line_decimal_signed(output_line, (long long)last);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, (unsigned long long)count, 1u);
            mmgr_line_put(output_line, " points; N at the two F's as given");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    else
    {
        int64_t tight[4096];
        size_t tight_count = 0u;
        int64_t lows[4096];
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
            exit(1);
        }
        start_nu = tight[0];
        end_nu = tight[tight_count - 1u];
        n_start = lows[0];
        n_end = lows[tight_count - 1u];
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cells ");
            mmgr_line_decimal_signed(output_line, (long long)first);
            mmgr_line_put(output_line, " to ");
            mmgr_line_decimal_signed(output_line, (long long)last);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, (unsigned long long)count, 1u);
            mmgr_line_put(output_line, " points; N certified to one value at ");
            mmgr_line_decimal(output_line, (unsigned long long)tight_count, 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal_signed(output_line, (long long)(last - first - 1));
            mmgr_line_put(output_line, " F's");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    size_t start = 0u;
    size_t end = 0u;
    for (size_t at = 0u; at < count; at += 1u)
    {
        if ((points[at].nu == start_nu) && (points[at].lane == cells[start_nu].first))
        {
            start = at;
            break;
        }
    }
    for (size_t at = 0u; at < count; at += 1u)
    {
        if ((points[at].nu == end_nu) && (points[at].lane == cells[end_nu].first))
        {
            end = at;
            break;
        }
    }
    zeros->count = place_zeros(binary, &constants, points, start, end, n_end - n_start, extra, &zeros->zeros);
    // The count of zeros fits a machine integer.
    if ((int64_t)zeros->count != (n_end - n_start))
    {
        fprintf(stderr, "  the zeros placed, %llu, do not number N's difference, %lld\n",
                (unsigned long long)zeros->count, (long long)(n_end - n_start));
        exit(1);
    }
    zeros->n_start = n_start;
    zeros->start_nu = start_nu;
    zeros->end_nu = end_nu;
    platform_cell_released(points, platform_place_g_program_owner);
    platform_cell_released(cells, platform_place_g_program_owner);
}
