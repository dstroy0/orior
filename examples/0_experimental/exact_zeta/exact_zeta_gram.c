// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-022
/**
 * @file exact_zeta_gram.c
 * @brief The theta function of Riemann and Siegel, the Gram points, and the sign of Z at each, by verdicts.
 *
 * @note Every verdict is a field, zero false and nonzero true. A pass writes its verdicts for the next pass to
 *       read, as in exact_zeta_zeros.c, whose values this reads.
 * @note Run build_exact_zeta.sh gram [height] [bits], beside this file.
 * @note This reads no corpus. It sits in 0_experimental, and is an entry in the workbook of analytic number
 *       theory, on its rail: it claims nothing about the Riemann hypothesis.
 * @note The phase: theta(t) = Im ln Gamma(1/4 + it/2) - (t/2) ln pi. With z = 1/4 + it/2 and w = z + M,
 *       ln Gamma(z) = ln Gamma(w) - sum over k < M of ln(z + k). The series of Stirling with K Bernoulli terms
 *       gives ln Gamma(w).
 * @note Its imaginary part has three parts. The first is (M - 1/4) arg w + (t/4) ln |w|^2 - t/2. The second is
 *       the imaginary parts of B_2j / (2j (2j - 1)) w^(1 - 2j). The third is minus every arg(z + k).
 * @note Each arg is an arctangent of a rational, by two series that must agree. One is the series of Euler, with
 *       its own pi from the identity of Euler. The other is the Taylor series about 1/2, with its own pi from the
 *       identity of Machin.
 * @note ln comes from the two series of zeta_exact.c, and ln pi from the logarithm of the floor of pi. Two routes
 *       run, at M = K = N and at 2N, and where they disagree, N doubles.
 * @note The Gram index: g_n is the point where theta(g_n) = n pi. At a point, the index i is theta, read at its
 *       places, over pi. The search decides the point where theta reads strictly above i pi and strictly below
 *       (i + 1) pi. Each reading is toward zero at the same places. A reading of equal doubles the places.
 * @note The walk: up from t = 10 in cells. theta increases past t = 6.29. The article on the theta function of
 *       Riemann and Siegel on Wikipedia reports that. A cell holds as many Gram points as the indices of its ends
 * differ by. An empty cell doubles the step, a cell that holds one is a bracket, and a cell that holds more splits.
 * @note Each bracket gets one bit per pass: the index at the midpoint gates which half the next pass reads.
 * @note The sign of Z: zeta(1/2 + it) = e^(-i theta) Z(t), with Z real. Just below g_n, Im zeta has the sign of
 *       Re zeta, and just above, the opposite sign. This phase verdict ties theta, from Stirling, to the phase of
 *       zeta, from Euler-Maclaurin. A bracket is final where three things hold at both ends: the phase verdict,
 *       one nonzero sign of Re zeta, and the step verdict. Else the index at its midpoint cuts it in half.
 * @note On a settled bracket, the sign of Re zeta is the sign of (-1)^n Z(g_n). Gram's law is that sign positive.
 * @note Gram's law as an agreement: the signs of Z(g_n) alternate where Gram's law holds. That is agreement at
 *       lag 2 and none at lag 1. The null is the same signs in drawn orders. It counts the draws that reach the
 *       live agreement at lag 2. zeta_shuffle.h states the draws.
 * @note Gram's law is a reading here, and never a steer. It fails for about a quarter of the Gram intervals in the
 *       long run, as the same article reports. The zero walk of exact_zeta_zeros.c steers only by its own verdicts.
 * @note Positive control: the Gram points g_0 to g_15 against the table of that article. Also the first failure
 *       of Gram's law at n = 126, as the same article reports it. The null: the Bernoulli terms left out of both
 *       routes of theta. Then the routes do not meet at the doublings shown, and the search decides no Gram index.
 */
#include "zeta_search.h"
#include "zeta_theta.h"
#include "zeta_shuffle.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The table of the smallest Gram points, g_0 to g_15, from the article on the theta function. */
static const char *const PUBLISHED[16] = {"17.8455995405", "23.1702827012", "27.6701822178", "31.7179799547",
                                          "35.4671842971", "38.9992099640", "42.3635503920", "45.5930289815",
                                          "48.7107766217", "51.7338428133", "54.6752374468", "57.5451651795",
                                          "60.3518119691", "63.1018679824", "65.8008876380", "68.4535449175"};

/** @brief The same article: Gram's law first fails at index 126. */
#define FIRST_FAILURE 126

/** @brief The seed of the drawn orders, as the shuffles of the tree name it. */
#define SHUFFLE_SEED 0x51F7u

/** @brief How many drawn orders. */
#define DRAWS 1000

/**
 * @brief The text of a truth value, as Python prints it.
 *
 * @param[in] value The value.
 * @return          "True" or "False".
 */
static const char *truth(int64_t value)
{
    return value ? "True" : "False";
}

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *gram_memory(void *block)
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
static uint64_t text_hash(const char *text)
{
    uint64_t hash = 1469598103934665603u;
    for (const char *at = text; *at != '\0'; at += 1)
    {
        hash = (hash ^ (uint64_t)(unsigned char)*at) * 1099511628211u;
    }
    return hash;
}

/* ---------------------------------------------------------------- the search of the Gram index */

/** @brief What the search knows of one point t. */
typedef struct
{
    DecimalPair t; /**< The point. */
    char *key;     /**< Its canonical text [OWNS]. */
    int has_index; /**< Nonzero where the point has its Gram index. */
    int64_t index; /**< The Gram index. */
} GramPoint;

/** @brief The Gram index decided at each point, and the cost of each decision. */
typedef struct
{
    GramPoint *points;   /**< The points [OWNS]. */
    size_t count;        /**< How many. */
    size_t capacity;     /**< How many the array holds. */
    size_t *buckets;     /**< The hash table, each an index + 1, or 0 for an empty bucket [OWNS]. */
    size_t bucket_count; /**< How many buckets, a power of two. */
    uint64_t asked;      /**< How many theta values the search asked for. */
    int32_t deepest;     /**< The most places at a decided point. */
    int64_t widest;      /**< The largest cut N at a decided point. */
} GramSearch;

/**
 * @brief The index of a point in the search, with a new record where the point is new.
 *
 * @param[in,out] search The search [BORROWS].
 * @param[in]     t      The point [BORROWS].
 * @return               The index.
 */
static size_t gram_intern(GramSearch *search, const DecimalPair *t)
{
    if ((search->count * 2u) >= search->bucket_count)
    {
        const size_t grown = (search->bucket_count == 0u) ? 1024u : (search->bucket_count * 2u);
        size_t *const buckets = (size_t *)gram_memory(calloc(grown, sizeof(size_t)));
        for (size_t at = 0u; at < search->count; at += 1u)
        {
            size_t place = (size_t)(text_hash(search->points[at].key) & (grown - 1u));
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
    size_t place = (size_t)(text_hash(key) & (search->bucket_count - 1u));
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
        search->points = (GramPoint *)gram_memory(realloc(search->points, search->capacity * sizeof(GramPoint)));
    }
    GramPoint *const point = &search->points[search->count];
    memset(point, 0, sizeof(*point));
    point->t = *t;
    point->key = (char *)gram_memory(malloc(strlen(key) + 1u));
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
static void gram_sweep(GramSearch *search, const size_t *points, size_t count)
{
    unsigned char *const seen = (unsigned char *)gram_memory(calloc(search->count + 1u, 1u));
    GramOpen *const open = (GramOpen *)gram_memory(malloc((count + 1u) * sizeof(GramOpen)));
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
            theta_routes(&search->points[asked.index].t, asked.places, asked.n_sum, 1, &one, &two);
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

/** @brief A bracket of one Gram point: [low, high] and the index n. */
typedef struct
{
    DecimalPair low;  /**< The lower end. */
    DecimalPair high; /**< The upper end. */
    int64_t index;    /**< The index n. */
} GramBracket;

/**
 * @brief Up from t = 10. Each pass reads the indices at the ends of the open cells, and gates the next cells.
 *
 * @param[in,out] search The search [BORROWS].
 * @param[in]     height The height [BORROWS].
 * @param[out]    found  The brackets, in ascending order of index [BORROWS]. Allocated.
 * @return               How many brackets.
 */
static size_t gram_walk(GramSearch *search, const DecimalPair *height, GramBracket **found)
{
    size_t found_count = 0u;
    size_t found_capacity = 16u;
    *found = (GramBracket *)gram_memory(malloc(found_capacity * sizeof(GramBracket)));
    DecimalPair *cells = NULL;
    size_t cell_count = 0u;
    DecimalPair low;
    DecimalPair step;
    pair_small(10, 0, &low);
    pair_small(1, 0, &step);
    while (pair_below(&low, height) || (cell_count > 0u))
    {
        const int top = pair_below(&low, height);
        DecimalPair reach;
        pair_plus(&low, &step, &reach);
        const DecimalPair high = pair_below(&reach, height) ? reach : *height;
        const size_t asked_count = cell_count + (top ? 1u : 0u);
        DecimalPair *const asked = (DecimalPair *)gram_memory(malloc((2u * asked_count + 2u) * sizeof(DecimalPair)));
        memcpy(asked, cells, 2u * cell_count * sizeof(DecimalPair));
        if (top)
        {
            asked[2u * cell_count] = low;
            asked[(2u * cell_count) + 1u] = high;
        }
        free(cells);
        cells = NULL;
        size_t *const indices = (size_t *)gram_memory(malloc((2u * asked_count + 1u) * sizeof(size_t)));
        for (size_t at = 0u; at < (2u * asked_count); at += 1u)
        {
            indices[at] = gram_intern(search, &asked[at]);
        }
        gram_sweep(search, indices, 2u * asked_count);
        size_t next_cells = 0u;
        cells = (DecimalPair *)gram_memory(malloc((4u * asked_count + 2u) * sizeof(DecimalPair)));
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
                    *found = (GramBracket *)gram_memory(realloc(*found, found_capacity * sizeof(GramBracket)));
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
static void gram_cut(GramSearch *search, GramBracket *brackets, size_t count)
{
    size_t *const mids = (size_t *)gram_memory(calloc(count + 1u, sizeof(size_t)));
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

/**
 * @brief Cuts each bracket until zeta settles the sign of Re zeta(1/2 + it) on it.
 *
 * @param[in,out] search   The search of the Gram index [BORROWS].
 * @param[in]     brackets The placed brackets [BORROWS].
 * @param[in]     count    How many.
 * @param[in,out] zeta     The search of zeta [BORROWS].
 * @param[out]    signs    The sign at each index, by index [BORROWS]. Large enough for every index.
 * @param[out]    settled  The settled bracket at each index, by index [BORROWS].
 * @return                 How many further cuts.
 */
static size_t gram_settle(GramSearch *search, const GramBracket *brackets, size_t count, Steering *zeta, int64_t *signs,
                          GramBracket *settled)
{
    GramBracket *open = (GramBracket *)gram_memory(malloc((count + 1u) * sizeof(GramBracket)));
    memcpy(open, brackets, count * sizeof(GramBracket));
    size_t open_count = count;
    size_t cuts = 0u;
    DecimalPair line;
    pair_small(5, 1, &line);
    while (open_count > 0u)
    {
        PointList asked = {NULL, 0u, 0u};
        size_t *const ends = (size_t *)gram_memory(malloc((2u * open_count + 1u) * sizeof(size_t)));
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            ZetaPoint point;
            point.sigma = line;
            point.t = open[at].low;
            ends[2u * at] = steering_intern(zeta, &point);
            point.t = open[at].high;
            ends[(2u * at) + 1u] = steering_intern(zeta, &point);
            point_list_push(&asked, ends[2u * at]);
            point_list_push(&asked, ends[(2u * at) + 1u]);
        }
        steering_sweep(zeta, &asked);
        point_list_free(&asked);
        size_t next_count = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            const size_t a = ends[2u * at];
            const size_t c = ends[(2u * at) + 1u];
            const int64_t sign_a = zeta->records[a].value[0].sign;
            const int64_t sign_c = zeta->records[c].value[0].sign;
            const int64_t phase =
                (zeta->records[a].value[1].sign == sign_a) && (zeta->records[c].value[1].sign == -sign_c);
            const int64_t same = sign_a == sign_c;
            const int64_t from_a = steering_step(zeta, a, c, a);
            const int64_t from_c = steering_step(zeta, a, c, c);
            const int64_t decided = phase * same * ((1 + from_a) / 2) * ((1 + from_c) / 2);
            if (decided)
            {
                signs[open[at].index] = sign_a;
                settled[open[at].index] = open[at];
                continue;
            }
            open[next_count] = open[at];
            next_count += 1u;
        }
        free(ends);
        gram_cut(search, open, next_count);
        open_count = next_count;
        cuts += next_count;
    }
    free(open);
    return cuts;
}

/**
 * @brief The gap of the two routes of theta at a cut.
 *
 * @param[in]  t         The point [BORROWS].
 * @param[in]  places    The places.
 * @param[in]  n_sum     The cut N.
 * @param[in]  bernoulli 1 to keep the Bernoulli terms, 0 to drop them.
 * @param[out] gap       The gap [BORROWS].
 */
static void theta_gap(const DecimalPair *t, int32_t places, int64_t n_sum, int bernoulli, OriorExactInteger *gap)
{
    OriorExactInteger one;
    OriorExactInteger two;
    theta_routes(t, places, n_sum, bernoulli, &one, &two);
    z_sub(&one, &two, gap);
    z_abs(gap, gap);
}

/**
 * @brief Writes a list of integers as Python prints a list.
 *
 * @param[in] values The values [BORROWS].
 * @param[in] count  How many.
 * @param[in] out    The stream [BORROWS].
 */
static void print_list(const OriorExactInteger *values, size_t count, FILE *out)
{
    fputs("[", out);
    for (size_t at = 0u; at < count; at += 1u)
    {
        if (at > 0u)
        {
            fputs(", ", out);
        }
        z_print(&values[at], out);
    }
    fputs("]", out);
}

int main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *const out = stdout;
    DecimalPair height;
    pair_units((argument_count > 1) ? arguments[1] : "285", &height);
    const int bits = (argument_count > 2) ? atoi(arguments[2]) : 16;
    fputs("  the Riemann-Siegel theta function and the Gram points by truthy and falsy verdicts, in exact integers\n",
          out);
    fputs(
        "  this reads the sign of Z at the Gram points it reaches and claims nothing about the Riemann hypothesis\n\n",
        out);

    GramSearch search;
    memset(&search, 0, sizeof(search));
    GramBracket *found = NULL;
    const size_t found_count = gram_walk(&search, &height, &found);
    if (found_count == 0u)
    {
        zeta_stop("no Gram point below the height");
    }
    const int64_t last_index = found[found_count - 1u].index;
    const int64_t consecutive = ((int64_t)found_count == (last_index + 1)) && (found[0].index == 0);
    fprintf(out, "  %llu Gram points with 10 < t < ", (unsigned long long)found_count);
    pair_print(&height, height.places, out);
    fprintf(out, ", indices 0 to %lld, each alone in its bracket: %s\n", (long long)last_index, truth(consecutive));
    fflush(out);

    GramBracket *const placed = (GramBracket *)gram_memory(malloc(found_count * sizeof(GramBracket)));
    memcpy(placed, found, found_count * sizeof(GramBracket));
    for (int bit = 0; bit < bits; bit += 1)
    {
        gram_cut(&search, placed, found_count);
    }
    char label[64];
    snprintf(label, sizeof(label), "placed by %d bits", bits);
    fprintf(out, "  %-5s %-34s %-15s %s\n", "n", label, "published", "holds");
    int64_t every = 1;
    for (size_t at = 0u; (at < found_count) && (at < 16u); at += 1u)
    {
        const int held = pair_holds(&placed[at].low, &placed[at].high, PUBLISHED[at]);
        every *= held;
        fprintf(out, "  %-5lld [", (long long)placed[at].index);
        pair_print(&placed[at].low, 10, out);
        fputs(", ", out);
        pair_print(&placed[at].high, 10, out);
        fprintf(out, "]  %-15s %s\n", PUBLISHED[at], truth(held));
    }
    fprintf(out, "  %llu theta values asked, the deepest at %d places, the widest at N = %lld\n\n",
            (unsigned long long)search.asked, (int)search.deepest, (long long)search.widest);
    fflush(out);

    Steering zeta;
    steering_begin(&zeta);
    // The indices run 0 to last_index, one bracket each, as the consecutive verdict checks.
    int64_t *const signs = (int64_t *)gram_memory(calloc((size_t)last_index + 1u, sizeof(int64_t)));
    GramBracket *const settled = (GramBracket *)gram_memory(calloc((size_t)last_index + 1u, sizeof(GramBracket)));
    const size_t cuts = gram_settle(&search, placed, found_count, &zeta, signs, settled);
    int64_t *const ordered = (int64_t *)gram_memory(malloc(found_count * sizeof(int64_t)));
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        ordered[at] = signs[found[at].index];
    }
    fputs("  the sign of (-1)^n Z(g_n), from Re zeta(1/2 + i g_n) on each settled bracket:\n", out);
    for (size_t row = 0u; row < found_count; row += 32u)
    {
        fprintf(out, "    %4lld  ", (long long)found[row].index);
        for (size_t at = row; (at < found_count) && (at < (row + 32u)); at += 1u)
        {
            fputc((ordered[at] > 0) ? '+' : '-', out);
        }
        fputs("\n", out);
    }
    fputs("  Gram's law fails at n = ", out);
    size_t bad_count = 0u;
    int64_t first_bad = -1;
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        if (ordered[at] == -1)
        {
            fprintf(out, "%s%lld", (bad_count > 0u) ? ", " : "", (long long)found[at].index);
            first_bad = (bad_count == 0u) ? found[at].index : first_bad;
            bad_count += 1u;
        }
    }
    fputs((bad_count == 0u) ? "none\n" : "\n", out);
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        if (ordered[at] == -1)
        {
            const GramBracket *const bracket = &settled[found[at].index];
            fprintf(out, "    g_%lld in [", (long long)found[at].index);
            pair_print(&bracket->low, 10, out);
            fputs(", ", out);
            pair_print(&bracket->high, 10, out);
            fputs("]\n", out);
        }
    }
    fprintf(out, "  %llu zeta values asked, %llu further cuts, the deepest at %d places, the widest at N = %lld\n",
            (unsigned long long)zeta.asked, (unsigned long long)cuts, (int)zeta.deepest, (long long)zeta.widest);
    const int64_t reached = last_index >= FIRST_FAILURE;
    const int64_t first = (reached * (first_bad == FIRST_FAILURE)) + ((1 - reached) * (bad_count == 0u));
    fprintf(out, "  the first failure against the reported index %d: %s\n\n", FIRST_FAILURE, truth(first));
    fflush(out);

    unsigned char *const seats = (unsigned char *)gram_memory(malloc(found_count + 1u));
    size_t plus_count = 0u;
    for (size_t at = 0u; at < found_count; at += 1u)
    {
        const int64_t value = ordered[at] * (((found[at].index % 2) == 0) ? 1 : -1);
        // The value is -1 or 1, and (1 + value) / 2 is 0 or 1.
        seats[at] = (unsigned char)((1 + value) / 2);
        plus_count += seats[at];
    }
    const size_t live_one = zeta_agreement(seats, found_count, 1u);
    const size_t live_two = zeta_agreement(seats, found_count, 2u);
    unsigned char *const drawn = (unsigned char *)gram_memory(malloc(found_count + 1u));
    size_t reach = 0u;
    size_t most = 0u;
    for (unsigned draw = 0u; draw < DRAWS; draw += 1u)
    {
        memcpy(drawn, seats, found_count);
        zeta_permuted(drawn, found_count, SHUFFLE_SEED + draw);
        const size_t agreement = zeta_agreement(drawn, found_count, 2u);
        reach += (agreement >= live_two) ? 1u : 0u;
        most = (agreement > most) ? agreement : most;
    }
    fprintf(out, "  the signs of Z(g_n), %llu positive and %llu negative, as a sequence\n",
            (unsigned long long)plus_count, (unsigned long long)(found_count - plus_count));
    fprintf(out, "    agreement at lag 1: %llu of %llu, at lag 2: %llu of %llu\n", (unsigned long long)live_one,
            (unsigned long long)(found_count - 1u), (unsigned long long)live_two,
            (unsigned long long)(found_count - 2u));
    fprintf(out, "    %d drawn orders of the same signs, %llu reaching %llu at lag 2, the max %llu\n", DRAWS,
            (unsigned long long)reach, (unsigned long long)live_two, (unsigned long long)most);
    const size_t fewer = (plus_count < (found_count - plus_count)) ? plus_count : (found_count - plus_count);
    const int64_t alternates = (live_two > most) && (fewer >= 32u);
    fprintf(out, "  agreement at lag 2 above every drawn order, with 32 or more of each sign: %s\n\n",
            truth(alternates));
    fflush(out);

    DecimalPair point;
    pair_small(20, 0, &point);
    const int32_t places = 8;
    OriorExactInteger true_gaps[6];
    OriorExactInteger null_gaps[6];
    for (unsigned j = 0u; j < 6u; j += 1u)
    {
        theta_gap(&point, places, (int64_t)1 << j, 1, &true_gaps[j]);
        theta_gap(&point, places, (int64_t)1 << j, 0, &null_gaps[j]);
    }
    fprintf(out, "  drawn null at t = 20, %d places: the gap between the two routes of theta as N doubles from 1\n",
            (int)places);
    fputs("    with the Bernoulli terms:     ", out);
    print_list(true_gaps, 6u, out);
    fputs("\n    without them:                 ", out);
    print_list(null_gaps, 6u, out);
    fputs("\n", out);
    const int64_t apart = (true_gaps[5].sign == 0) && (null_gaps[5].sign > 0);
    fprintf(out, "  without the Bernoulli terms the routes do not meet at the doublings displayed: %s\n", truth(apart));
    fflush(out);
    return (int)(1 - (consecutive * every * first * alternates * apart));
}
