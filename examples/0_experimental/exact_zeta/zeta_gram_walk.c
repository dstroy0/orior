// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_gram_walk.c
 * @brief The search of the Gram index, as zeta_gram_walk.h states it.
 */
#include "zeta_gram_walk.h"
#include "zeta_theta.h"

#include <stdlib.h>
#include <string.h>

const char *const ZETA_GRAM_PUBLISHED[16] = {"17.8455995405", "23.1702827012", "27.6701822178", "31.7179799547",
                                             "35.4671842971", "38.9992099640", "42.3635503920", "45.5930289815",
                                             "48.7107766217", "51.7338428133", "54.6752374468", "57.5451651795",
                                             "60.3518119691", "63.1018679824", "65.8008876380", "68.4535449175"};

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *walk_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief The hash of a text, by FNV-1a.
 *
 * @param[in] text The text [BORROWS].
 * @return         The hash.
 */
static uint64_t walk_text_hash(const char *text)
{
    uint64_t hash = 1469598103934665603u;
    for (const char *at = text; *at != '\0'; at += 1)
    {
        hash = (hash ^ (uint64_t)(unsigned char)*at) * 1099511628211u;
    }
    return hash;
}

/* ---------------------------------------------------------------- the search of the Gram index */

/**
 * @brief The index of a point in the search, with a new record where the point is new.
 *
 * @param[in,out] search The search [BORROWS].
 * @param[in]     t      The point [BORROWS].
 * @return               The index.
 */
size_t gram_intern(GramSearch *search, const DecimalPair *t)
{
    if ((search->count * 2u) >= search->bucket_count)
    {
        const size_t grown = (search->bucket_count == 0u) ? 1024u : (search->bucket_count * 2u);
        size_t *const buckets = (size_t *)walk_memory(calloc(grown, sizeof(size_t)));
        for (size_t at = 0u; at < search->count; at += 1u)
        {
            size_t place = (size_t)(walk_text_hash(search->points[at].key) & (grown - 1u));
            while (buckets[place] != 0u)
            {
                place = (place + 1u) & (grown - 1u);
            }
            buckets[place] = at + 1u;
        }
        free(search->buckets);
        search->buckets = buckets;
        search->bucket_count = grown;
    }
    static char digits[8192];
    z_text(&t->numerator, digits, sizeof(digits));
    char key[8256];
    snprintf(key, sizeof(key), "%s/%d", digits, (int)t->places);
    size_t place = (size_t)(walk_text_hash(key) & (search->bucket_count - 1u));
    while (search->buckets[place] != 0u)
    {
        const size_t index = search->buckets[place] - 1u;
        if (strcmp(search->points[index].key, key) == 0)
        {
            return index;
        }
        place = (place + 1u) & (search->bucket_count - 1u);
    }
    if (search->count >= search->capacity)
    {
        search->capacity = (search->capacity == 0u) ? 256u : (search->capacity * 2u);
        search->points = (GramPoint *)walk_memory(realloc(search->points, search->capacity * sizeof(GramPoint)));
    }
    GramPoint *const point = &search->points[search->count];
    memset(point, 0, sizeof(*point));
    point->t = *t;
    point->key = (char *)walk_memory(malloc(strlen(key) + 1u));
    memcpy(point->key, key, strlen(key) + 1u);
    search->buckets[place] = search->count + 1u;
    search->count += 1u;
    return search->count - 1u;
}

/** @brief A point still open in a sweep. */
typedef struct
{
    size_t index;   /**< The point. */
    int32_t places; /**< The places to ask at. */
    int64_t n_sum;  /**< The cut N to ask at. */
} GramOpen;

/**
 * @brief Decides the Gram index of each point, pass by pass, each pass reading the verdicts of the last.
 *
 * @param[in,out] search The search [BORROWS].
 * @param[in]     points The points, as indices, repeats allowed [BORROWS].
 * @param[in]     count  How many.
 */
void gram_sweep(GramSearch *search, const size_t *points, size_t count)
{
    unsigned char *const seen = (unsigned char *)walk_memory(calloc(search->count + 1u, 1u));
    GramOpen *const open = (GramOpen *)walk_memory(malloc((count + 1u) * sizeof(GramOpen)));
    size_t open_count = 0u;
    for (size_t at = 0u; at < count; at += 1u)
    {
        if (seen[points[at]])
        {
            continue;
        }
        seen[points[at]] = 1u;
        if (!search->points[points[at]].has_index)
        {
            open[open_count].index = points[at];
            open[open_count].places = 1;
            open[open_count].n_sum = 1;
            open_count += 1u;
        }
    }
    free(seen);
    while (open_count > 0u)
    {
        size_t next_count = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            const GramOpen asked = open[at];
            search->asked += 1u;
            OriorExactInteger one;
            OriorExactInteger two;
            search->phase(&search->points[asked.index].t, asked.places, asked.n_sum, &one, &two);
            const int64_t agree = orior_exact_equal(&one, &two);
            OriorExactInteger big_pi;
            zeta_pi(asked.places + ZETA_GUARD, &big_pi);
            OriorExactInteger scaled = one;
            zeta_require(orior_exact_scale_by_ten(&scaled, ZETA_GUARD), "scale");
            OriorExactInteger ratio;
            z_floor_div(&scaled, &big_pi, &ratio);
            const int64_t i = z_small(&ratio);
            OriorExactInteger bound;
            theta_pi_times(i, &big_pi, &bound);
            const int64_t low = z_cmp(&one, &bound);
            theta_pi_times(i + 1, &big_pi, &bound);
            const int64_t high = z_cmp(&bound, &one);
            const int64_t decided = agree * ((1 + low) / 2) * ((1 + high) / 2);
            if (decided)
            {
                GramPoint *const point = &search->points[asked.index];
                point->has_index = 1;
                point->index = i;
                search->deepest = (asked.places > search->deepest) ? asked.places : search->deepest;
                search->widest = (asked.n_sum > search->widest) ? asked.n_sum : search->widest;
                continue;
            }
            open[next_count].index = asked.index;
            open[next_count].places = asked.places * (int32_t)(1 + agree);
            open[next_count].n_sum = asked.n_sum * (2 - agree);
            next_count += 1u;
        }
        open_count = next_count;
    }
    free(open);
}

/**
 * @brief Up from the start. Each pass reads the indices at the ends of the open cells, and gates the next cells.
 *
 * @param[in,out] search The search [BORROWS].
 * @param[in]     height The height [BORROWS].
 * @param[out]    found  The brackets, in ascending order of index [BORROWS]. Allocated.
 * @return               How many brackets.
 */
size_t gram_walk(GramSearch *search, const DecimalPair *height, GramBracket **found)
{
    size_t found_count = 0u;
    size_t found_capacity = 16u;
    *found = (GramBracket *)walk_memory(malloc(found_capacity * sizeof(GramBracket)));
    DecimalPair *cells = NULL;
    size_t cell_count = 0u;
    DecimalPair low;
    DecimalPair step;
    low = search->start;
    step = search->step;
    while (pair_below(&low, height) || (cell_count > 0u))
    {
        const int top = pair_below(&low, height);
        DecimalPair reach;
        pair_plus(&low, &step, &reach);
        const DecimalPair high = pair_below(&reach, height) ? reach : *height;
        const size_t asked_count = cell_count + (top ? 1u : 0u);
        DecimalPair *const asked = (DecimalPair *)walk_memory(malloc((2u * asked_count + 2u) * sizeof(DecimalPair)));
        memcpy(asked, cells, 2u * cell_count * sizeof(DecimalPair));
        if (top)
        {
            asked[2u * cell_count] = low;
            asked[(2u * cell_count) + 1u] = high;
        }
        free(cells);
        cells = NULL;
        size_t *const indices = (size_t *)walk_memory(malloc((2u * asked_count + 1u) * sizeof(size_t)));
        for (size_t at = 0u; at < (2u * asked_count); at += 1u)
        {
            indices[at] = gram_intern(search, &asked[at]);
        }
        gram_sweep(search, indices, 2u * asked_count);
        size_t next_cells = 0u;
        cells = (DecimalPair *)walk_memory(malloc((4u * asked_count + 2u) * sizeof(DecimalPair)));
        int64_t last = 0;
        for (size_t cell = 0u; cell < asked_count; cell += 1u)
        {
            const int64_t start = search->points[indices[2u * cell]].index;
            const int64_t end = search->points[indices[(2u * cell) + 1u]].index;
            const int64_t points_inside = end - start;
            last = points_inside;
            if (points_inside == 1)
            {
                if (found_count >= found_capacity)
                {
                    found_capacity *= 2u;
                    *found = (GramBracket *)walk_memory(realloc(*found, found_capacity * sizeof(GramBracket)));
                }
                (*found)[found_count].low = asked[2u * cell];
                (*found)[found_count].high = asked[(2u * cell) + 1u];
                (*found)[found_count].index = end;
                found_count += 1u;
            }
            else if (points_inside > 1)
            {
                DecimalPair centre;
                pair_middle(&asked[2u * cell], &asked[(2u * cell) + 1u], &centre);
                cells[4u * next_cells / 2u] = asked[2u * cell];
                cells[(4u * next_cells / 2u) + 1u] = centre;
                cells[(4u * next_cells / 2u) + 2u] = centre;
                cells[(4u * next_cells / 2u) + 3u] = asked[(2u * cell) + 1u];
                next_cells += 2u;
            }
        }
        cell_count = next_cells;
        free(indices);
        free(asked);
        last *= top;
        DecimalPair next_step;
        if (last == 0)
        {
            pair_twice(&step, &next_step);
        }
        else if (last > 1)
        {
            pair_half(&step, &next_step);
        }
        else
        {
            next_step = step;
        }
        step = next_step;
        if (top)
        {
            low = high;
        }
    }
    free(cells);
    // A stable sort by index.
    for (size_t at = 1u; at < found_count; at += 1u)
    {
        const GramBracket moving = (*found)[at];
        size_t place = at;
        while ((place > 0u) && (moving.index < (*found)[place - 1u].index))
        {
            (*found)[place] = (*found)[place - 1u];
            place -= 1u;
        }
        (*found)[place] = moving;
    }
    return found_count;
}

/**
 * @brief Cuts each bracket in half by the Gram index at its midpoint.
 *
 * @param[in,out] search   The search [BORROWS].
 * @param[in,out] brackets The brackets [BORROWS].
 * @param[in]     count    How many.
 */
void gram_cut(GramSearch *search, GramBracket *brackets, size_t count)
{
    size_t *const mids = (size_t *)walk_memory(calloc(count + 1u, sizeof(size_t)));
    for (size_t at = 0u; at < count; at += 1u)
    {
        DecimalPair centre;
        pair_middle(&brackets[at].low, &brackets[at].high, &centre);
        mids[at] = gram_intern(search, &centre);
    }
    gram_sweep(search, mids, count);
    for (size_t at = 0u; at < count; at += 1u)
    {
        const GramPoint *const middle = &search->points[mids[at]];
        if (middle->index >= brackets[at].index)
        {
            brackets[at].high = middle->t;
        }
        else
        {
            brackets[at].low = middle->t;
        }
    }
    free(mids);
}
