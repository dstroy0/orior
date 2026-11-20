// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_gram_walk.h
 * @brief The search of the Gram index: the sweep, the walk up from a start, and the cut of a bracket.
 *
 * @note The search reads theta through its phase, at a point in the units of the walk. The walk of
 *       exact_zeta_gram.c is in t. The walk of exact_zeta_riemann_siegel.c is in u, with t = 2pi u^4.
 */
#ifndef ZETA_GRAM_WALK_H
#define ZETA_GRAM_WALK_H

#include "zeta_exact.h"

/** @brief The table of the smallest Gram points, g_0 to g_15, in t, from the article on the theta function. */
extern const char *const ZETA_GRAM_PUBLISHED[16];

/** @brief The same article: Gram's law first fails at index 126. */
#define ZETA_GRAM_FIRST_FAILURE 126

/** @brief What the search knows of one point. */
typedef struct
{
    DecimalPair t; /**< The point. */
    char *key;     /**< Its canonical text [OWNS]. */
    int has_index; /**< Nonzero where the point has its Gram index. */
    int64_t index; /**< The Gram index. */
} GramPoint;

/**
 * @brief The two routes of theta at a point, at the places and the cut N.
 *
 * @param[in]  point  The point [BORROWS].
 * @param[in]  places The places.
 * @param[in]  n_sum  The cut N.
 * @param[out] one    The first route [BORROWS].
 * @param[out] two    The second route [BORROWS].
 */
typedef void (*GramPhase)(const DecimalPair *point, int32_t places, int64_t n_sum, OriorExactInteger *one,
                          OriorExactInteger *two);

/** @brief The Gram index decided at each point, and the cost of each decision. */
typedef struct
{
    GramPhase phase;     /**< The two routes of theta at a point. */
    DecimalPair start;   /**< The first point of the walk. */
    DecimalPair step;    /**< The first step of the walk. */
    GramPoint *points;   /**< The points [OWNS]. */
    size_t count;        /**< How many. */
    size_t capacity;     /**< How many the array holds. */
    size_t *buckets;     /**< The hash table, each an index + 1, or 0 for an empty bucket [OWNS]. */
    size_t bucket_count; /**< How many buckets, a power of two. */
    uint64_t asked;      /**< How many theta values the search asked for. */
    int32_t deepest;     /**< The most places at a decided point. */
    int64_t widest;      /**< The largest cut N at a decided point. */
} GramSearch;

/** @brief A bracket of one Gram point: [low, high] and the index n. */
typedef struct
{
    DecimalPair low;  /**< The lower end. */
    DecimalPair high; /**< The upper end. */
    int64_t index;    /**< The index n. */
} GramBracket;

/**
 * @brief The index of a point in the search, with a new record where the point is new.
 *
 * @param[in,out] search The search [BORROWS].
 * @param[in]     t      The point [BORROWS].
 * @return               The index.
 */
size_t gram_intern(GramSearch *search, const DecimalPair *t);

/**
 * @brief Decides the Gram index of each point, pass by pass, each pass reading the verdicts of the last.
 *
 * @param[in,out] search The search [BORROWS].
 * @param[in]     points The points, as indices, repeats allowed [BORROWS].
 * @param[in]     count  How many.
 */
void gram_sweep(GramSearch *search, const size_t *points, size_t count);

/**
 * @brief Up from the start. Each pass reads the indices at the ends of the open cells, and gates the next cells.
 *
 * @param[in,out] search The search [BORROWS].
 * @param[in]     height The height [BORROWS].
 * @param[out]    found  The brackets, in ascending order of index [BORROWS]. Allocated.
 * @return               How many brackets.
 */
size_t gram_walk(GramSearch *search, const DecimalPair *height, GramBracket **found);

/**
 * @brief Cuts each bracket in half by the Gram index at its midpoint.
 *
 * @param[in,out] search   The search [BORROWS].
 * @param[in,out] brackets The brackets [BORROWS].
 * @param[in]     count    How many.
 */
void gram_cut(GramSearch *search, GramBracket *brackets, size_t count);

#endif
