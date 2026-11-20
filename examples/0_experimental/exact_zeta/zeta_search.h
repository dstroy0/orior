// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_search.h
 * @brief The steering of the zeta zeros: points decided pass by pass, boxes counted by winding, zeros placed.
 *
 * @note Every verdict is a field, zero false and nonzero true. AND is a product. NOT is 1 - |COMPARE(v, 0)|. A
 *       pass writes its verdicts, and the next pass reads them. Nothing sets a precision, region, step or term
 *       count in advance.
 * @note A point lives once in the table of the steering. A list of points holds indices into that table.
 */
#ifndef ZETA_SEARCH_H
#define ZETA_SEARCH_H

#include "zeta_exact.h"

/** @brief What the steering knows of one point. */
typedef struct
{
    ZetaPoint point;            /**< The point. */
    char *key;                  /**< The canonical text of the point [OWNS]. */
    int has_decided;            /**< Nonzero where the point has its eighth of a turn. */
    int decided;                /**< The eighth of a turn, 0 to 7. */
    int has_values;             /**< Nonzero where the point has values. */
    OriorExactInteger value[4]; /**< Re, Im, Re', Im' of zeta at the point. */
    int32_t places;             /**< The places of the values. */
    int64_t n_sum;              /**< The cut N of the values. */
    int has_wanted;             /**< Nonzero where a chord asks the point for more places. */
    int32_t wanted;             /**< The places asked for. */
} PointRecord;

/** @brief A list of points, as indices into the table of the steering. */
typedef struct
{
    size_t *at;      /**< The indices [OWNS]. */
    size_t count;    /**< How many. */
    size_t capacity; /**< How many the array holds. */
} PointList;

/** @brief A box [left, right] x [low, high] in the plane of s. */
typedef struct
{
    DecimalPair left;  /**< The least sigma. */
    DecimalPair right; /**< The greatest sigma. */
    DecimalPair low;   /**< The least t. */
    DecimalPair high;  /**< The greatest t. */
} ZetaBox;

/** @brief A list of boxes. */
typedef struct
{
    ZetaBox *boxes;  /**< The boxes [OWNS]. */
    size_t count;    /**< How many. */
    size_t capacity; /**< How many the array holds. */
} BoxList;

/** @brief The points already decided, and what their decision cost. */
typedef struct
{
    PointRecord *records;   /**< The points [OWNS]. */
    size_t count;           /**< How many points. */
    size_t capacity;        /**< How many the array holds. */
    size_t *buckets;        /**< The hash table of keys, each an index + 1, or 0 for an empty bucket [OWNS]. */
    size_t bucket_capacity; /**< How many buckets, a power of two. */
    uint64_t asked;         /**< How many values the steering asked for. */
    int32_t deepest;        /**< The most places at a decided point. */
    int64_t widest;         /**< The largest cut N at a decided point. */
    PointList *last_paths;  /**< The paths of the last count, one per box [OWNS]. */
    size_t last_count;      /**< How many paths. */
} Steering;

/**
 * @brief An empty steering.
 *
 * @param[out] steering The steering [BORROWS].
 */
void steering_begin(Steering *steering);

/**
 * @brief Frees a steering and leaves it empty.
 *
 * @param[in,out] steering The steering [BORROWS].
 */
void steering_free(Steering *steering);

/**
 * @brief The index of a point in the table, with a new record where the point is new.
 *
 * @param[in,out] steering The steering [BORROWS].
 * @param[in]     point    The point [BORROWS].
 * @return                 The index.
 */
size_t steering_intern(Steering *steering, const ZetaPoint *point);

/**
 * @brief Appends an index to a list.
 *
 * @param[in,out] list  The list [BORROWS].
 * @param[in]     index The index.
 */
void point_list_push(PointList *list, size_t index);

/**
 * @brief Frees a list.
 *
 * @param[in,out] list The list [BORROWS].
 */
void point_list_free(PointList *list);

/**
 * @brief Appends a box to a list.
 *
 * @param[in,out] list The list [BORROWS].
 * @param[in]     box  The box [BORROWS].
 */
void box_list_push(BoxList *list, const ZetaBox *box);

/**
 * @brief Frees a list of boxes.
 *
 * @param[in,out] list The list [BORROWS].
 */
void box_list_free(BoxList *list);

/**
 * @brief Decides the eighth of a turn of each point, pass by pass, each pass reading the verdicts of the last.
 *
 * @param[in,out] steering The steering [BORROWS].
 * @param[in]     points   The points, repeats allowed [BORROWS].
 * @note The sweep asks a point again where a chord asks it for more places than it holds.
 */
void steering_sweep(Steering *steering, const PointList *points);

/**
 * @brief The zeros in each box, by the winding of zeta around its edge.
 *
 * @param[in,out] steering The steering [BORROWS].
 * @param[in]     boxes    The boxes [BORROWS].
 * @param[out]    counts   One count per box [BORROWS].
 */
void steering_count(Steering *steering, const BoxList *boxes, int64_t *counts);

/**
 * @brief Up the strip. Each pass counts the open boxes, and its verdicts gate the boxes of the next pass.
 *
 * @param[in,out] steering The steering [BORROWS].
 * @param[in]     height   The height to walk to [BORROWS].
 * @param[out]    found    The boxes that hold one zero each, in ascending order of t [BORROWS].
 */
void steering_walk(Steering *steering, const DecimalPair *height, BoxList *found);

/**
 * @brief One bit per pass for every zero at once: does the lower square centred on the line count one?
 *
 * @param[in,out] steering The steering [BORROWS].
 * @param[in]     found    The boxes of the zeros [BORROWS].
 * @param[in]     bits     How many passes.
 * @param[out]    lows     The lower end of each bracket [BORROWS].
 * @param[out]    highs    The upper end of each bracket [BORROWS].
 * @param[out]    paths    The bits of each zero, as text of bits + 1 bytes [BORROWS].
 */
void steering_place(Steering *steering, const BoxList *found, int bits, DecimalPair *lows, DecimalPair *highs,
                    char **paths);

/**
 * @brief COMPARE(|zeta|^2, |zeta'|^2 |c - a|^2) at one end of the step from a to c.
 *
 * @param[in] steering The steering, with values at the end [BORROWS].
 * @param[in] from     The point a.
 * @param[in] to       The point c.
 * @param[in] end      The end that gives zeta and zeta'.
 * @return             The verdict: positive where the step moves zeta less than its own size.
 */
int64_t steering_step(const Steering *steering, size_t from, size_t to, size_t end);

/**
 * @brief The index of the midpoint of two points, with a new record where it is new.
 *
 * @param[in,out] steering The steering [BORROWS].
 * @param[in]     from     The first point.
 * @param[in]     to       The second point.
 * @return                 The index of the midpoint.
 */
size_t steering_midpoint(Steering *steering, size_t from, size_t to);

/**
 * @brief The four corners of a box, counterclockwise from (left, low).
 *
 * @param[in]  box     The box [BORROWS].
 * @param[out] corners The corners [BORROWS].
 */
void box_corners(const ZetaBox *box, ZetaPoint corners[4]);

#endif
