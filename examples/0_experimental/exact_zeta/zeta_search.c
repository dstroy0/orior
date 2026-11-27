// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_search.c
 * @brief The steering of the zeta zeros. zeta_search.h states the rules.
 *
 * @note The count of a box reads each edge twice: end to end, and through its midpoint. Each half carries two types
 *       of verdict. The first is COMPARE of the smaller |zeta|^2 at its ends against its chord. The second, at each
 *       end, is COMPARE of |zeta|^2 against |zeta'|^2 times the step squared.
 * @note A zero verdict doubles the places that it reads. A negative verdict, or two readings that disagree, puts
 *       the midpoint into the path. An edge is final where the readings agree and every verdict is positive.
 */
#include "zeta_search.h"
#include "../../../src/platform/platform.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdlib.h>

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *steering_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief 1 - |COMPARE(value, 0)|: 1 where the value is zero, else 0.
 *
 * @param[in] value The value.
 * @return          The verdict.
 */
static int64_t verdict_not(int64_t value)
{
    return (value == 0) ? 1 : 0;
}

/**
 * @brief The canonical text of a point.
 *
 * @param[in] point The point [BORROWS].
 * @return          The text [OWNS].
 */
static char *point_key(const ZetaPoint *point)
{
    static char sigma[8192];
    static char t[8192];
    z_text(&point->sigma.numerator, sigma, sizeof(sigma));
    z_text(&point->t.numerator, t, sizeof(t));
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = sigma, .cap = PLATFORM_TEXT_TO_TERMINATOR) +
                          EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = t, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 64u;
    char *const key = (char *)steering_memory(platform_cell_taken(length, platform_place_g_program_owner, 0));
    snprintf(key, length, "%s/%d,%s/%d", sigma, (int)point->sigma.places, t, (int)point->t.places);
    return key;
}

/**
 * @brief The hash of a text, by FNV-1a.
 *
 * @param[in] text The text [BORROWS].
 * @return         The hash.
 */
static uint64_t key_hash(const char *text)
{
    uint64_t hash = 1469598103934665603u;
    for (const char *at = text; *at != '\0'; at += 1)
    {
        hash ^= (uint64_t)(unsigned char)*at;
        hash *= 1099511628211u;
    }
    return hash;
}

void steering_begin(Steering *steering)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = steering, .bytes = sizeof(*steering), .val = 0);
}

void steering_free(Steering *steering)
{
    for (size_t at = 0u; at < steering->count; at += 1u)
    {
        platform_cell_released(steering->records[at].key, platform_place_g_program_owner);
    }
    platform_cell_released(steering->records, platform_place_g_program_owner);
    platform_cell_released(steering->buckets, platform_place_g_program_owner);
    for (size_t at = 0u; at < steering->last_count; at += 1u)
    {
        point_list_free(&steering->last_paths[at]);
    }
    platform_cell_released(steering->last_paths, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = steering, .bytes = sizeof(*steering), .val = 0);
}

size_t steering_intern(Steering *steering, const ZetaPoint *point)
{
    if ((steering->count * 2u) >= steering->bucket_capacity)
    {
        const size_t grown = (steering->bucket_capacity == 0u) ? 4096u : (steering->bucket_capacity * 2u);
        size_t *const buckets =
            (size_t *)steering_memory(platform_cell_cleared(grown, sizeof(size_t), platform_place_g_program_owner, 0));
        for (size_t at = 0u; at < steering->count; at += 1u)
        {
            size_t place = (size_t)(key_hash(steering->records[at].key) & (grown - 1u));
            while (buckets[place] != 0u)
            {
                place = (place + 1u) & (grown - 1u);
            }
            buckets[place] = at + 1u;
        }
        platform_cell_released(steering->buckets, platform_place_g_program_owner);
        steering->buckets = buckets;
        steering->bucket_capacity = grown;
    }
    char *const key = point_key(point);
    size_t place = (size_t)(key_hash(key) & (steering->bucket_capacity - 1u));
    while (steering->buckets[place] != 0u)
    {
        const size_t index = steering->buckets[place] - 1u;
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = steering->records[index].key,
                       .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = key, .ci = EMBED_FALSE))
        {
            platform_cell_released(key, platform_place_g_program_owner);
            return index;
        }
        place = (place + 1u) & (steering->bucket_capacity - 1u);
    }
    if (steering->count >= steering->capacity)
    {
        steering->capacity = (steering->capacity == 0u) ? 1024u : (steering->capacity * 2u);
        steering->records = (PointRecord *)steering_memory(
            platform_cell_grown(steering->records, steering->count * sizeof(PointRecord),
                                steering->capacity * sizeof(PointRecord), platform_place_g_program_owner, 0));
    }
    PointRecord *const record = &steering->records[steering->count];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = record, .bytes = sizeof(*record), .val = 0);
    record->point = *point;
    record->key = key;
    record->n_sum = 1;
    steering->buckets[place] = steering->count + 1u;
    steering->count += 1u;
    return steering->count - 1u;
}

void point_list_push(PointList *list, size_t index)
{
    if (list->count >= list->capacity)
    {
        list->capacity = (list->capacity == 0u) ? 16u : (list->capacity * 2u);
        list->at = (size_t *)steering_memory(platform_cell_grown(list->at, list->count * sizeof(size_t),
                                                                 list->capacity * sizeof(size_t),
                                                                 platform_place_g_program_owner, 0));
    }
    list->at[list->count] = index;
    list->count += 1u;
}

void point_list_free(PointList *list)
{
    platform_cell_released(list->at, platform_place_g_program_owner);
    list->at = NULL;
    list->count = 0u;
    list->capacity = 0u;
}

void box_list_push(BoxList *list, const ZetaBox *box)
{
    if (list->count >= list->capacity)
    {
        list->capacity = (list->capacity == 0u) ? 8u : (list->capacity * 2u);
        list->boxes = (ZetaBox *)steering_memory(platform_cell_grown(list->boxes, list->count * sizeof(ZetaBox),
                                                                     list->capacity * sizeof(ZetaBox),
                                                                     platform_place_g_program_owner, 0));
    }
    list->boxes[list->count] = *box;
    list->count += 1u;
}

void box_list_free(BoxList *list)
{
    platform_cell_released(list->boxes, platform_place_g_program_owner);
    list->boxes = NULL;
    list->count = 0u;
    list->capacity = 0u;
}

/** @brief A point still open in a sweep, with the places and the cut to ask at. */
typedef struct
{
    size_t index;   /**< The point. */
    int32_t places; /**< The places to ask at. */
    int64_t n_sum;  /**< The cut N to ask at. */
} OpenPoint;

void steering_sweep(Steering *steering, const PointList *points)
{
    // The points in their first order, each once.
    unsigned char *const seen = (unsigned char *)steering_memory(
        platform_cell_cleared(steering->count + 1u, 1u, platform_place_g_program_owner, 0));
    OpenPoint *open = (OpenPoint *)steering_memory(
        platform_cell_taken((points->count + 1u) * sizeof(OpenPoint), platform_place_g_program_owner, 0));
    size_t open_count = 0u;
    for (size_t at = 0u; at < points->count; at += 1u)
    {
        const size_t index = points->at[at];
        if (seen[index])
        {
            continue;
        }
        seen[index] = 1u;
        const PointRecord *const record = &steering->records[index];
        const int32_t held = record->has_values ? record->places : 0;
        const int32_t wanted = record->has_wanted ? record->wanted : 1;
        if (held < wanted)
        {
            open[open_count].index = index;
            open[open_count].places = wanted;
            open[open_count].n_sum = record->has_values ? record->n_sum : 1;
            open_count += 1u;
        }
    }
    platform_cell_released(seen, platform_place_g_program_owner);
    while (open_count > 0u)
    {
        size_t next_count = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            const OpenPoint asked = open[at];
            steering->asked += 1u;
            OriorExactInteger values[8];
            zeta_routes(&steering->records[asked.index].point, asked.places, asked.n_sum, 1, values);
            if (asked.places > ZETA_SCALE_DIGITS)
            {
                zeta_stop("a point asks for more places than a value holds");
            }
            const int64_t agree =
                verdict_not(z_cmp(&values[0], &values[4]) != 0) * verdict_not(z_cmp(&values[1], &values[5]) != 0);
            const int64_t sign_re = values[0].sign;
            const int64_t sign_im = values[1].sign;
            OriorExactInteger size_re;
            OriorExactInteger size_im;
            z_abs(&values[0], &size_re);
            z_abs(&values[1], &size_im);
            const int64_t sign_size = z_cmp(&size_re, &size_im);
            const int64_t decided =
                agree * ((sign_re != 0) ? 1 : 0) * ((sign_im != 0) ? 1 : 0) * ((sign_size != 0) ? 1 : 0);
            if (decided)
            {
                PointRecord *const record = &steering->records[asked.index];
                const int64_t quadrant = (1 - sign_im) + ((1 - (sign_re * sign_im)) / 2);
                record->decided = (int)((2 * quadrant) + ((((1 - sign_size) / 2) + quadrant) % 2));
                record->has_decided = 1;
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = record->value, .src = values, .bytes = sizeof(record->value));
                record->places = asked.places;
                record->n_sum = asked.n_sum;
                record->has_values = 1;
                steering->deepest = (asked.places > steering->deepest) ? asked.places : steering->deepest;
                steering->widest = (asked.n_sum > steering->widest) ? asked.n_sum : steering->widest;
                continue;
            }
            // The kept list writes over entries already read, and never over one still unread.
            open[next_count].index = asked.index;
            open[next_count].places = asked.places * (int32_t)(1 + agree);
            open[next_count].n_sum = asked.n_sum * (2 - agree);
            next_count += 1u;
        }
        open_count = next_count;
    }
    platform_cell_released(open, platform_place_g_program_owner);
}

/**
 * @brief The eighth turns of zeta from one point to another, read as the shorter way round.
 *
 * @param[in] steering The steering [BORROWS].
 * @param[in] from     The first point.
 * @param[in] to       The second point.
 * @return             The turns, -4 to 3.
 */
static int64_t steering_turn(const Steering *steering, size_t from, size_t to)
{
    const int64_t difference = (int64_t)steering->records[to].decided - steering->records[from].decided + 4;
    // The difference is at least -3, thus the remainder of C equals the floor remainder after one turn.
    return (((difference % 8) + 8) % 8) - 4;
}

/**
 * @brief Two verdicts on a chord: its ends at the same places, and COMPARE(smaller |zeta|^2, chord^2).
 *
 * @param[in]  steering The steering [BORROWS].
 * @param[in]  from     The first end.
 * @param[in]  to       The second end.
 * @param[out] chord    COMPARE(smaller |zeta|^2, chord^2) [BORROWS].
 * @return              1 where the ends hold the same places.
 * @note The second is positive where the chord |zeta(c) - zeta(a)| is under the smaller of |zeta(a)| and
 *       |zeta(c)|. That keeps the angle between them under a sixth of a turn. It is negative where it is not, and
 *       zero where the places cannot tell.
 */
static int64_t steering_short(const Steering *steering, size_t from, size_t to, int64_t *chord)
{
    const PointRecord *const first = &steering->records[from];
    const PointRecord *const second = &steering->records[to];
    OriorExactInteger first_size;
    OriorExactInteger second_size;
    OriorExactInteger product;
    z_mul(&first->value[0], &first->value[0], &first_size);
    z_mul(&first->value[1], &first->value[1], &product);
    z_add(&first_size, &product, &first_size);
    z_mul(&second->value[0], &second->value[0], &second_size);
    z_mul(&second->value[1], &second->value[1], &product);
    z_add(&second_size, &product, &second_size);
    const OriorExactInteger *const smaller = (z_cmp(&first_size, &second_size) <= 0) ? &first_size : &second_size;
    OriorExactInteger across;
    OriorExactInteger part;
    z_sub(&second->value[0], &first->value[0], &part);
    z_mul(&part, &part, &across);
    z_sub(&second->value[1], &first->value[1], &part);
    z_mul(&part, &part, &part);
    z_add(&across, &part, &across);
    *chord = z_cmp(smaller, &across);
    return verdict_not(first->places - second->places);
}

/**
 * @brief COMPARE(|zeta|^2, |zeta'|^2 |c - a|^2) at one end of the step from a to c.
 *
 * @param[in] steering The steering [BORROWS].
 * @param[in] from     The point a.
 * @param[in] to       The point c.
 * @param[in] end      The end that gives zeta and zeta'.
 * @return             The verdict.
 * @note Positive where the step moves zeta less than its own size, read from the derivative at that end. Two
 *       values alone cannot see zeta wind once between them and land where it began. The derivative sees how fast
 *       it turns.
 */
int64_t steering_step(const Steering *steering, size_t from, size_t to, size_t end)
{
    const PointRecord *const record = &steering->records[end];
    DecimalPair across_sigma;
    DecimalPair across_t;
    pair_minus(&steering->records[to].point.sigma, &steering->records[from].point.sigma, &across_sigma);
    pair_minus(&steering->records[to].point.t, &steering->records[from].point.t, &across_t);
    OriorExactInteger x;
    OriorExactInteger y;
    const int32_t places = pair_common(&across_sigma, &across_t, &x, &y);
    OriorExactInteger size;
    OriorExactInteger product;
    z_mul(&record->value[0], &record->value[0], &size);
    z_mul(&record->value[1], &record->value[1], &product);
    z_add(&size, &product, &size);
    zeta_require(orior_exact_scale_by_ten(&size, (uint32_t)(2 * places)), "scale");
    OriorExactInteger speed;
    z_mul(&record->value[2], &record->value[2], &speed);
    z_mul(&record->value[3], &record->value[3], &product);
    z_add(&speed, &product, &speed);
    OriorExactInteger length;
    z_mul(&x, &x, &length);
    z_mul(&y, &y, &product);
    z_add(&length, &product, &length);
    z_mul(&speed, &length, &speed);
    return z_cmp(&size, &speed);
}

/**
 * @brief Asks a point again at the places given.
 *
 * @param[in,out] steering The steering [BORROWS].
 * @param[in]     index    The point.
 * @param[in]     places   The places.
 */
static void steering_deeper(Steering *steering, size_t index, int32_t places)
{
    steering->records[index].wanted = places;
    steering->records[index].has_wanted = 1;
}

/**
 * @brief The index of the midpoint of two points.
 *
 * @param[in,out] steering The steering [BORROWS].
 * @param[in]     from     The first point.
 * @param[in]     to       The second point.
 * @return                 The index of the midpoint.
 */
size_t steering_midpoint(Steering *steering, size_t from, size_t to)
{
    ZetaPoint middle;
    pair_middle(&steering->records[from].point.sigma, &steering->records[to].point.sigma, &middle.sigma);
    pair_middle(&steering->records[from].point.t, &steering->records[to].point.t, &middle.t);
    return steering_intern(steering, &middle);
}

void box_corners(const ZetaBox *box, ZetaPoint corners[4])
{
    corners[0].sigma = box->left;
    corners[0].t = box->low;
    corners[1].sigma = box->right;
    corners[1].t = box->low;
    corners[2].sigma = box->right;
    corners[2].t = box->high;
    corners[3].sigma = box->left;
    corners[3].t = box->high;
}

/** @brief One edge of a path: its two ends and its midpoint. */
typedef struct
{
    size_t from;   /**< The first end. */
    size_t middle; /**< The midpoint. */
    size_t to;     /**< The second end. */
} PathEdge;

void steering_count(Steering *steering, const BoxList *boxes, int64_t *counts)
{
    const size_t total = boxes->count;
    PointList *const paths = (PointList *)steering_memory(
        platform_cell_cleared(total + 1u, sizeof(PointList), platform_place_g_program_owner, 0));
    for (size_t at = 0u; at < total; at += 1u)
    {
        ZetaPoint corners[4];
        box_corners(&boxes->boxes[at], corners);
        for (unsigned corner = 0u; corner < 4u; corner += 1u)
        {
            point_list_push(&paths[at], steering_intern(steering, &corners[corner]));
        }
        counts[at] = 0;
    }
    size_t *active = (size_t *)steering_memory(
        platform_cell_taken((total + 1u) * sizeof(size_t), platform_place_g_program_owner, 0));
    size_t active_count = total;
    for (size_t at = 0u; at < total; at += 1u)
    {
        active[at] = at;
    }
    while (active_count > 0u)
    {
        PathEdge **const edges = (PathEdge **)steering_memory(
            platform_cell_cleared(active_count, sizeof(PathEdge *), platform_place_g_program_owner, 0));
        PointList asked = {NULL, 0u, 0u};
        for (size_t which = 0u; which < active_count; which += 1u)
        {
            const PointList *const path = &paths[active[which]];
            edges[which] = (PathEdge *)steering_memory(
                platform_cell_taken(path->count * sizeof(PathEdge), platform_place_g_program_owner, 0));
            for (size_t at = 0u; at < path->count; at += 1u)
            {
                const size_t from = path->at[at];
                const size_t to = path->at[(at + 1u) % path->count];
                const size_t middle = steering_midpoint(steering, from, to);
                edges[which][at].from = from;
                edges[which][at].middle = middle;
                edges[which][at].to = to;
                point_list_push(&asked, from);
                point_list_push(&asked, middle);
                point_list_push(&asked, to);
            }
        }
        steering_sweep(steering, &asked);
        point_list_free(&asked);

        size_t next_count = 0u;
        for (size_t which = 0u; which < active_count; which += 1u)
        {
            const size_t box = active[which];
            PointList *const path = &paths[box];
            const size_t edge_count = path->count;
            int64_t turns = 0;
            int64_t least_settled = 1;
            PointList next_path = {NULL, 0u, 0u};
            for (size_t at = 0u; at < edge_count; at += 1u)
            {
                const PathEdge edge = edges[which][at];
                const int64_t first_half = steering_turn(steering, edge.from, edge.middle);
                const int64_t second_half = steering_turn(steering, edge.middle, edge.to);
                turns += first_half + second_half;
                const int64_t agree =
                    verdict_not(steering_turn(steering, edge.from, edge.to) - first_half - second_half);
                int64_t positive = agree;
                int64_t retained = agree;
                const size_t halves[2][2] = {{edge.from, edge.middle}, {edge.middle, edge.to}};
                for (unsigned half = 0u; half < 2u; half += 1u)
                {
                    const size_t x = halves[half][0];
                    const size_t y = halves[half][1];
                    int64_t chord = 0;
                    const int64_t same = steering_short(steering, x, y, &chord);
                    const int32_t places = (steering->records[x].places > steering->records[y].places)
                                               ? steering->records[x].places
                                               : steering->records[y].places;
                    if (verdict_not(same))
                    {
                        steering_deeper(steering, x, places);
                        steering_deeper(steering, y, places);
                    }
                    if ((same * verdict_not(chord)) != 0)
                    {
                        steering_deeper(steering, x, 2 * places);
                        steering_deeper(steering, y, 2 * places);
                    }
                    const int64_t from_x = steering_step(steering, x, y, x);
                    const int64_t from_y = steering_step(steering, x, y, y);
                    if (verdict_not(from_x))
                    {
                        steering_deeper(steering, x, 2 * steering->records[x].places);
                    }
                    if (verdict_not(from_y))
                    {
                        steering_deeper(steering, y, 2 * steering->records[y].places);
                    }
                    positive *= same * ((1 + chord) / 2) * ((1 + from_x) / 2) * ((1 + from_y) / 2);
                    retained *= (1 - (same * ((1 - chord) / 2))) * (1 - ((1 - from_x) / 2)) * (1 - ((1 - from_y) / 2));
                }
                least_settled = (positive < least_settled) ? positive : least_settled;
                point_list_push(&next_path, edge.from);
                if (1 - retained)
                {
                    point_list_push(&next_path, edge.middle);
                }
            }
            // The winding is a whole count of turns, at least zero. Floor and truncation agree on it.
            counts[box] = (turns >= 0) ? (turns / 8) : -((-turns + 7) / 8);
            point_list_free(path);
            *path = next_path;
            if (verdict_not(least_settled))
            {
                active[next_count] = box;
                next_count += 1u;
            }
            platform_cell_released(edges[which], platform_place_g_program_owner);
        }
        platform_cell_released(edges, platform_place_g_program_owner);
        active_count = next_count;
    }
    platform_cell_released(active, platform_place_g_program_owner);
    for (size_t at = 0u; at < steering->last_count; at += 1u)
    {
        point_list_free(&steering->last_paths[at]);
    }
    platform_cell_released(steering->last_paths, platform_place_g_program_owner);
    steering->last_paths = paths;
    steering->last_count = total;
}

/**
 * @brief The two halves of a box, split at the midpoint of t.
 *
 * @param[in]  box    The box [BORROWS].
 * @param[out] halves The lower half, then the upper half [BORROWS].
 */
static void box_halves(const ZetaBox *box, ZetaBox halves[2])
{
    DecimalPair centre;
    pair_middle(&box->low, &box->high, &centre);
    halves[0] = *box;
    halves[0].high = centre;
    halves[1] = *box;
    halves[1].low = centre;
}

void steering_walk(Steering *steering, const DecimalPair *height, BoxList *found)
{
    BoxList boxes = {NULL, 0u, 0u};
    DecimalPair low;
    DecimalPair step;
    pair_small(1, 0, &low);
    pair_small(1, 0, &step);
    found->boxes = NULL;
    found->count = 0u;
    found->capacity = 0u;
    while (pair_below(&low, height) || (boxes.count > 0u))
    {
        const int top = pair_below(&low, height);
        DecimalPair reach;
        pair_plus(&low, &step, &reach);
        const DecimalPair high = pair_below(&reach, height) ? reach : *height;
        BoxList asked = {NULL, 0u, 0u};
        for (size_t at = 0u; at < boxes.count; at += 1u)
        {
            box_list_push(&asked, &boxes.boxes[at]);
        }
        if (top)
        {
            ZetaBox fresh;
            pair_small(0, 0, &fresh.left);
            pair_small(1, 0, &fresh.right);
            fresh.low = low;
            fresh.high = high;
            box_list_push(&asked, &fresh);
        }
        int64_t *const counts = (int64_t *)steering_memory(
            platform_cell_taken((asked.count + 1u) * sizeof(int64_t), platform_place_g_program_owner, 0));
        steering_count(steering, &asked, counts);
        box_list_free(&boxes);
        for (size_t at = 0u; at < asked.count; at += 1u)
        {
            if (counts[at] == 1)
            {
                box_list_push(found, &asked.boxes[at]);
            }
            else if (counts[at] > 1)
            {
                ZetaBox halves[2];
                box_halves(&asked.boxes[at], halves);
                box_list_push(&boxes, &halves[0]);
                box_list_push(&boxes, &halves[1]);
            }
        }
        const int64_t last = counts[asked.count - 1u] * top;
        platform_cell_released(counts, platform_place_g_program_owner);
        box_list_free(&asked);
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
    box_list_free(&boxes);
    // A stable sort by the lower end of t.
    for (size_t at = 1u; at < found->count; at += 1u)
    {
        const ZetaBox moving = found->boxes[at];
        size_t place = at;
        while ((place > 0u) && pair_below(&moving.low, &found->boxes[place - 1u].low))
        {
            found->boxes[place] = found->boxes[place - 1u];
            place -= 1u;
        }
        found->boxes[place] = moving;
    }
}

void steering_place(Steering *steering, const BoxList *found, int bits, DecimalPair *lows, DecimalPair *highs,
                    char **paths)
{
    const size_t count = found->count;
    for (size_t at = 0u; at < count; at += 1u)
    {
        lows[at] = found->boxes[at].low;
        highs[at] = found->boxes[at].high;
        paths[at] =
            (char *)steering_memory(platform_cell_cleared((size_t)bits + 1u, 1u, platform_place_g_program_owner, 0));
    }
    DecimalPair line;
    pair_small(5, 1, &line);
    int64_t *const counts = (int64_t *)steering_memory(
        platform_cell_taken((count + 1u) * sizeof(int64_t), platform_place_g_program_owner, 0));
    for (int bit = 0; bit < bits; bit += 1)
    {
        BoxList squares = {NULL, 0u, 0u};
        for (size_t at = 0u; at < count; at += 1u)
        {
            DecimalPair centre;
            pair_middle(&lows[at], &highs[at], &centre);
            DecimalPair span;
            pair_minus(&centre, &lows[at], &span);
            DecimalPair reach;
            pair_half(&span, &reach);
            ZetaBox square;
            pair_minus(&line, &reach, &square.left);
            pair_plus(&line, &reach, &square.right);
            square.low = lows[at];
            square.high = centre;
            box_list_push(&squares, &square);
        }
        steering_count(steering, &squares, counts);
        box_list_free(&squares);
        for (size_t at = 0u; at < count; at += 1u)
        {
            const int lower = counts[at] == 1;
            DecimalPair centre;
            pair_middle(&lows[at], &highs[at], &centre);
            if (lower)
            {
                highs[at] = centre;
            }
            else
            {
                lows[at] = centre;
            }
            paths[at][bit] = lower ? '0' : '1';
        }
    }
    platform_cell_released(counts, platform_place_g_program_owner);
}
