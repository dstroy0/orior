// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-025
/**
 * @file zeta_zero_certificate.c
 * @brief A counted zero of the Riemann zeta function, as a certificate that a second reader checks in one pass.
 *
 * @note The second reader does not repeat the search. The theorem of Proth gives a prime the same shape.
 * @note Run build_exact_zeta.sh certificate [low] [high], beside this file.
 * @note This reads no corpus. It sits in 0_experimental, and is an entry in the workbook of analytic number
 *       theory, on its rail: it claims nothing about the Riemann hypothesis. It certifies the zeros that it reaches
 *       inside the boxes that it asks for, and stops there.
 * @note The two costs: exact_zeta_zeros.c counts a zero by a walk around a box, and sums the eighth turns of zeta.
 *       It splits an edge wherever a chord or a step verdict is not yet positive. That walk is a search: it decides
 *       for itself how deep to read each point and how far to subdivide. The search is the expensive half. The check
 *       of its answer is the cheap half, and this file keeps the two apart.
 * @note A certificate is the settled path, and for each of its points, the places and the N at which the routes
 *       agreed. It records where to read and how deep, never the value found. The checker reads each point once at
 *       that depth, and derives the eighth of a turn from the exact value again.
 * @note The checker confirms three things. The two routes agree. The turns close to a whole number of circles.
 *       Every chord and step verdict is positive. It reads no point twice, and subdivides nothing. The count follows
 *       from the turns.
 * @note Why the check is a proof: this is the division that the theorem of Proth draws for a prime N = k 2^n + 1.
 *       A witness a is hard to find, and proves nothing until a^((N-1)/2) == -1 settles it in one power.
 * @note The eighth turns are the eighth roots of unity. The turn from point to point is subtraction in that group,
 *       the same group as the twiddle table of a number theoretic transform. The article
 *       theory/theory/twiddle_constants_article.tex covers it, in its sections on the theorem of Proth and on the
 *       group law.
 * @note The sum of the turns is the winding. On a box symmetric about Re(s) = 1/2, a winding of one circle holds a
 *       zero on the line. The symmetry s -> 1 - conj(s) brings the mirror of a zero off the line into the same box
 *       (zeta_zero_symmetry.py, EXP-x-015).
 * @note The group law is not enough. The winding alone is the trap of the twiddle table in another form. A root of
 *       half the required order satisfies every relation among the entries of the table. Only the order test, two
 *       powers, catches it. Here, a path too coarse to resolve the turns can sum to a whole circle by luck, with no
 *       chord proved.
 * @note The chord and step verdicts are the order test. A chord holds the angle across an edge under a sixth of a
 *       turn. A step holds the motion under the size. Together they lift the sum from the group Z/8 to the
 *       integers. Prove the steps, then trust the count. The reverse is a fast wrong answer.
 * @note Positive control: the unit tiles from t = 14 to t = 26, edge to edge. Three of them hold one zero each, by
 *       certificate, against the published table of Odlyzko, and the rest hold none. The null: the four corners
 *       alone. Their winding is already one circle, and the checker refuses their chords.
 */
#include "zeta_search.h"
#include "../../../src/platform/platform.h"

#include "memoria_operor/memoria_operor.h"
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief One entry of a certificate: a point, and the places and the cut at which to read it. */
typedef struct
{
    size_t point;   /**< The point, as an index into the store of points. */
    int32_t places; /**< The places. */
    int64_t n_sum;  /**< The cut N. */
} CertificateEntry;

/** @brief A certificate: entries in path order, with the midpoints in place. */
typedef struct
{
    CertificateEntry *entries; /**< The entries [OWNS]. */
    size_t count;              /**< How many. */
} Certificate;

/** @brief The outcome of a check. */
typedef struct
{
    int agree;     /**< Whether the two routes agree at every point. */
    int64_t turns; /**< The sum of the eighth turns. */
    size_t edges;  /**< How many edges. */
    size_t chords; /**< How many chord verdicts are positive. */
    size_t steps;  /**< How many step verdicts are positive. */
    int whole;     /**< Whether the turns close to whole circles. */
    int64_t count; /**< The count, where the turns close. */
    int verified;  /**< Whether every verdict holds. */
} CheckRecord;

/**
 * @brief The text of a truth value, as Python prints it.
 *
 * @param[in] value The value.
 * @return          "True" or "False".
 */
static const char *truth(int value)
{
    return value ? "True" : "False";
}

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *certificate_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief The eighth of a turn that zeta sits in, from three signs, derived again from the exact value.
 *
 * @param[in] re The real part [BORROWS].
 * @param[in] im The imaginary part [BORROWS].
 * @return       The eighth. A zero sign gives a reading that the checker then refuses at the chord.
 */
static int64_t octant(const OriorExactInteger *re, const OriorExactInteger *im)
{
    OriorExactInteger size_re;
    OriorExactInteger size_im;
    z_abs(re, &size_re);
    z_abs(im, &size_im);
    const int64_t sign_re = re->sign;
    const int64_t sign_im = im->sign;
    const int64_t sign_size = z_cmp(&size_re, &size_im);
    const int64_t quadrant = (1 - sign_im) + ((1 - (sign_re * sign_im)) / 2);
    return (2 * quadrant) + ((((1 - sign_size) / 2) + quadrant) % 2);
}

/**
 * @brief A path with a midpoint in every edge.
 *
 * @param[in,out] store The store of points [BORROWS].
 * @param[in]     path  The path [BORROWS].
 * @param[out]    out   The refined path [BORROWS].
 */
static void refined(Steering *store, const PointList *path, PointList *out)
{
    out->at = NULL;
    out->count = 0u;
    out->capacity = 0u;
    for (size_t at = 0u; at < path->count; at += 1u)
    {
        const size_t from = path->at[at];
        const size_t to = path->at[(at + 1u) % path->count];
        point_list_push(out, from);
        point_list_push(out, steering_midpoint(store, from, to));
    }
}

/**
 * @brief The search of exact_zeta_zeros.c, run once over a box, then the settled path and the depth of each point.
 *
 * @param[in]  box         The box [BORROWS].
 * @param[out] store       The steering, which also stores the points of the certificate [BORROWS].
 * @param[out] certificate The certificate, in path order with its midpoints [BORROWS].
 * @param[out] searched    How many values the search asked for [BORROWS].
 * @return                 The count.
 */
static int64_t certify(const ZetaBox *box, Steering *store, Certificate *certificate, uint64_t *searched)
{
    steering_begin(store);
    BoxList boxes = {NULL, 0u, 0u};
    box_list_push(&boxes, box);
    int64_t count = 0;
    steering_count(store, &boxes, &count);
    box_list_free(&boxes);
    PointList path;
    refined(store, &store->last_paths[0], &path);
    certificate->entries = (CertificateEntry *)certificate_memory(
        platform_cell_taken((path.count + 1u) * sizeof(CertificateEntry), platform_place_g_program_owner, 0));
    certificate->count = path.count;
    for (size_t at = 0u; at < path.count; at += 1u)
    {
        certificate->entries[at].point = path.at[at];
        certificate->entries[at].places = store->records[path.at[at]].places;
        certificate->entries[at].n_sum = store->records[path.at[at]].n_sum;
    }
    point_list_free(&path);
    *searched = store->asked;
    return count;
}

/** @brief One reading of a point in a check. */
typedef struct
{
    size_t point;               /**< The point. */
    int32_t places;             /**< The places. */
    OriorExactInteger value[4]; /**< Re, Im, Re', Im'. */
} CheckReading;

/**
 * @brief One pass over the certificate, with no search.
 *
 * @param[in] store       The store of points [BORROWS].
 * @param[in] certificate The certificate [BORROWS].
 * @return                The record of the check.
 * @note The check reads each point once at its recorded depth, derives its octant again, and counts the three
 *       verdicts.
 */
static CheckRecord check(const Steering *store, const Certificate *certificate)
{
    CheckRecord record;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &record, .bytes = sizeof(record), .val = 0);
    record.agree = 1;
    const size_t edges = certificate->count;
    CheckReading *const reads = (CheckReading *)certificate_memory(
        platform_cell_taken((edges + 1u) * sizeof(CheckReading), platform_place_g_program_owner, 0));
    for (size_t at = 0u; at < edges; at += 1u)
    {
        const CertificateEntry *const entry = &certificate->entries[at];
        OriorExactInteger values[8];
        zeta_routes(&store->records[entry->point].point, entry->places, entry->n_sum, 1, values);
        record.agree =
            record.agree && orior_exact_equal(&values[0], &values[4]) && orior_exact_equal(&values[1], &values[5]);
        reads[at].point = entry->point;
        reads[at].places = entry->places;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = reads[at].value, .src = values, .bytes = sizeof(reads[at].value));
    }
    record.edges = edges;
    for (size_t at = 0u; at < edges; at += 1u)
    {
        const CheckReading *const near = &reads[at];
        const CheckReading *const far = &reads[(at + 1u) % edges];
        const int64_t difference =
            octant(&far->value[0], &far->value[1]) - octant(&near->value[0], &near->value[1]) + 4;
        record.turns += (((difference % 8) + 8) % 8) - 4;
        OriorExactInteger near_size;
        OriorExactInteger far_size;
        OriorExactInteger product;
        z_mul(&near->value[0], &near->value[0], &near_size);
        z_mul(&near->value[1], &near->value[1], &product);
        z_add(&near_size, &product, &near_size);
        z_mul(&far->value[0], &far->value[0], &far_size);
        z_mul(&far->value[1], &far->value[1], &product);
        z_add(&far_size, &product, &far_size);
        const OriorExactInteger *const smaller = (z_cmp(&near_size, &far_size) <= 0) ? &near_size : &far_size;
        OriorExactInteger across;
        OriorExactInteger part;
        z_sub(&far->value[0], &near->value[0], &part);
        z_mul(&part, &part, &across);
        z_sub(&far->value[1], &near->value[1], &part);
        z_mul(&part, &part, &part);
        z_add(&across, &part, &across);
        record.chords += ((z_cmp(smaller, &across) > 0) && (far->places == near->places)) ? 1u : 0u;
        DecimalPair run_sigma;
        DecimalPair run_t;
        pair_minus(&store->records[far->point].point.sigma, &store->records[near->point].point.sigma, &run_sigma);
        pair_minus(&store->records[far->point].point.t, &store->records[near->point].point.t, &run_t);
        OriorExactInteger run_re;
        OriorExactInteger run_im;
        const int32_t run_places = pair_common(&run_sigma, &run_t, &run_re, &run_im);
        OriorExactInteger span;
        z_mul(&run_re, &run_re, &span);
        z_mul(&run_im, &run_im, &product);
        z_add(&span, &product, &span);
        OriorExactInteger unit;
        z_ten_to(2 * run_places, &unit);
        OriorExactInteger left;
        OriorExactInteger right;
        z_mul(&near_size, &unit, &left);
        z_mul(&near->value[2], &near->value[2], &right);
        z_mul(&near->value[3], &near->value[3], &product);
        z_add(&right, &product, &right);
        z_mul(&right, &span, &right);
        const int step_near = z_cmp(&left, &right);
        z_mul(&far_size, &unit, &left);
        z_mul(&far->value[2], &far->value[2], &right);
        z_mul(&far->value[3], &far->value[3], &product);
        z_add(&right, &product, &right);
        z_mul(&right, &span, &right);
        const int step_far = z_cmp(&left, &right);
        record.steps += ((step_near > 0) && (step_far > 0)) ? 1u : 0u;
    }
    platform_cell_released(reads, platform_place_g_program_owner);
    record.whole = (((record.turns % 8) + 8) % 8) == 0;
    record.count = (record.turns >= 0) ? (record.turns / 8) : -((-record.turns + 7) / 8);
    record.verified = record.agree && record.whole && (record.chords == edges) && (record.steps == edges);
    return record;
}

/**
 * @brief A path around a box, refined a fixed number of times, every point at the same depth. No search.
 *
 * @param[in,out] store       The store of points [BORROWS].
 * @param[in]     box         The box [BORROWS].
 * @param[in]     refinements How many times to halve each edge.
 * @param[in]     places      The places of every point.
 * @param[in]     n_sum       The cut of every point.
 * @param[out]    certificate The certificate [BORROWS].
 */
static void fixed_depth(Steering *store, const ZetaBox *box, int refinements, int32_t places, int64_t n_sum,
                        Certificate *certificate)
{
    ZetaPoint corners[4];
    box_corners(box, corners);
    PointList path = {NULL, 0u, 0u};
    for (unsigned corner = 0u; corner < 4u; corner += 1u)
    {
        point_list_push(&path, steering_intern(store, &corners[corner]));
    }
    for (int pass = 0; pass < refinements; pass += 1)
    {
        PointList finer;
        refined(store, &path, &finer);
        point_list_free(&path);
        path = finer;
    }
    certificate->entries = (CertificateEntry *)certificate_memory(
        platform_cell_taken((path.count + 1u) * sizeof(CertificateEntry), platform_place_g_program_owner, 0));
    certificate->count = path.count;
    for (size_t at = 0u; at < path.count; at += 1u)
    {
        certificate->entries[at].point = path.at[at];
        certificate->entries[at].places = places;
        certificate->entries[at].n_sum = n_sum;
    }
    point_list_free(&path);
}

/**
 * @brief A unit box from t = floor to t = floor + 1, symmetric about Re(s) = 1/2.
 *
 * @param[out] box   The box [BORROWS].
 * @param[in]  floor The lower t, a whole number.
 */
static void unit_box(ZetaBox *box, int64_t floor)
{
    pair_small(0, 0, &box->left);
    pair_small(1, 0, &box->right);
    pair_small(floor, 0, &box->low);
    pair_small(floor + 1, 0, &box->high);
}

/**
 * @brief Section one: every unit tile from t = low to t = high, searched once and checked in one pass.
 *
 * @param[in] out  The stream [BORROWS].
 * @param[in] low  The lower t.
 * @param[in] high The upper t.
 * @return         1 where every tile passes, and the count matches the table where the table covers the span.
 */
static int report_line(FILE *out, int64_t low, int64_t high)
{
    fprintf(out, "  the line: every unit tile from t = %lld to t = %lld searched once and checked in one pass\n",
            (long long)low, (long long)high);
    fprintf(out, "  %-20s %-6s %-7s %-9s %-8s %s\n", "tile", "count", "turns", "searched", "checked", "verdict");
    int all_pass = 1;
    int64_t total = 0;
    BoxList pending = {NULL, 0u, 0u};
    for (int64_t floor = low; floor < high; floor += 1)
    {
        ZetaBox box;
        unit_box(&box, floor);
        box_list_push(&pending, &box);
    }
    size_t next = 0u;
    while (next < pending.count)
    {
        const ZetaBox box = pending.boxes[next];
        next += 1u;
        Steering store;
        Certificate certificate;
        uint64_t searched = 0u;
        const int64_t count = certify(&box, &store, &certificate, &searched);
        if (count > 1)
        {
            // The halves go to the front of the queue, before every box not yet read.
            DecimalPair centre;
            pair_middle(&box.low, &box.high, &centre);
            ZetaBox halves[2] = {box, box};
            halves[0].high = centre;
            halves[1].low = centre;
            BoxList reordered = {NULL, 0u, 0u};
            box_list_push(&reordered, &halves[0]);
            box_list_push(&reordered, &halves[1]);
            for (size_t at = next; at < pending.count; at += 1u)
            {
                box_list_push(&reordered, &pending.boxes[at]);
            }
            box_list_free(&pending);
            pending = reordered;
            next = 0u;
            platform_cell_released(certificate.entries, platform_place_g_program_owner);
            steering_free(&store);
            continue;
        }
        const CheckRecord record = check(&store, &certificate);
        const int single = record.verified && (record.count == count);
        all_pass = all_pass && single;
        total += count;
        const char *const verdict = single ? ((count == 0) ? "no zero" : "one zero on the line") : "FAIL";
        char low_text[64];
        char high_text[64];
        pair_text(&box.low, 3, low_text, sizeof(low_text));
        pair_text(&box.high, 3, high_text, sizeof(high_text));
        fprintf(out, "  [%s, %s]   %-6lld %-7lld %-9llu %-8llu %s\n", low_text, high_text, (long long)count,
                (long long)record.turns, (unsigned long long)searched, (unsigned long long)certificate.count, verdict);
        platform_cell_released(certificate.entries, platform_place_g_program_owner);
        steering_free(&store);
    }
    box_list_free(&pending);
    DecimalPair lower;
    DecimalPair upper;
    pair_small(low, 0, &lower);
    pair_small(high, 0, &upper);
    int64_t published = 0;
    for (size_t at = 0u; at < 40u; at += 1u)
    {
        DecimalPair ordinate;
        pair_units(ZETA_PUBLISHED_ZEROS[at], &ordinate);
        published += (pair_below(&lower, &ordinate) && pair_below(&ordinate, &upper)) ? 1 : 0;
    }
    DecimalPair last;
    pair_units(ZETA_PUBLISHED_ZEROS[39], &last);
    const int covered = pair_below(&upper, &last);
    const int matches = !covered || (total == published);
    fprintf(out, "\n  %lld zeros certified in [%lld, %lld]; the published table holds ", (long long)total,
            (long long)low, (long long)high);
    if (covered)
    {
        fprintf(out, "%lld there.\n", (long long)published);
    }
    else
    {
        fputs("too few ordinates to say there.\n", out);
    }
    fputs("  each tile is symmetric about Re(s) = 1/2, where a winding of one circle holds a zero on the\n", out);
    fputs("  line (zeta_zero_symmetry.py) and a winding of none holds no zero. the tiles meet edge to\n", out);
    fputs("  edge and every one is checked; no part of the span is read by trust.\n\n", out);
    return all_pass && matches;
}

/**
 * @brief Section two: the checker refuses an understated depth, and the certificate carries nothing to fiddle.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where the searched certificate passes and the shallow one fails.
 */
static int report_rejection(FILE *out)
{
    fputs("  rejection: a certificate that understates its depth cannot pass\n", out);
    DecimalPair ordinate;
    pair_units(ZETA_PUBLISHED_ZEROS[2], &ordinate);
    OriorExactInteger whole;
    OriorExactInteger under;
    z_ten_to(ordinate.places, &under);
    z_floor_div(&ordinate.numerator, &under, &whole);
    ZetaBox box;
    unit_box(&box, z_small(&whole));
    Steering store;
    Certificate certificate;
    uint64_t searched = 0u;
    (void)certify(&box, &store, &certificate, &searched);
    const CheckRecord record = check(&store, &certificate);
    Certificate shallow;
    shallow.count = certificate.count;
    shallow.entries = (CertificateEntry *)certificate_memory(
        platform_cell_taken((shallow.count + 1u) * sizeof(CertificateEntry), platform_place_g_program_owner, 0));
    for (size_t at = 0u; at < shallow.count; at += 1u)
    {
        shallow.entries[at].point = certificate.entries[at].point;
        shallow.entries[at].places = 1;
        shallow.entries[at].n_sum = 1;
    }
    const CheckRecord shallow_record = check(&store, &shallow);
    char ordinate_text[64];
    pair_text(&ordinate, 3, ordinate_text, sizeof(ordinate_text));
    fprintf(out, "  box around %s at its searched depth: routes agree %s, verified %s (count ", ordinate_text,
            truth(record.agree), truth(record.verified));
    if (record.whole)
    {
        fprintf(out, "%lld)\n", (long long)record.count);
    }
    else
    {
        fputs("None)\n", out);
    }
    fprintf(out, "  the same points forced to 1 place and N = 1: routes agree %s, verified %s\n",
            truth(shallow_record.agree), truth(shallow_record.verified));
    fputs("\n  a check that cannot fail is not a check. this one fails on a depth that cannot sign the\n", out);
    fputs("  value, caught by the two routes drawing apart. the certificate records only where to read\n", out);
    fputs("  and how deep, never the value found. a fiddled reading has nothing to fiddle: the checker\n", out);
    fputs("  re-derives every octant from zeta itself.\n\n", out);
    platform_cell_released(shallow.entries, platform_place_g_program_owner);
    platform_cell_released(certificate.entries, platform_place_g_program_owner);
    steering_free(&store);
    return !shallow_record.verified && record.verified;
}

/**
 * @brief Section three: the winding closes on a path too coarse to prove, and the chords catch it.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where the corners fail, and the searched path passes with a count of one.
 */
static int report_floor(FILE *out)
{
    fputs("  floor: the winding alone closes to one circle before any chord is proved\n", out);
    fprintf(out, "  %-24s %-7s %-10s %-10s %s\n", "path around [14, 15]", "turns", "chords+", "steps+", "verdict");
    ZetaBox box;
    unit_box(&box, 14);
    static const char *const names[3] = {"4 corners", "8 points", "16 points"};
    int four_corner_verified = 0;
    for (int refinements = 0; refinements < 3; refinements += 1)
    {
        Steering store;
        steering_begin(&store);
        Certificate certificate;
        fixed_depth(&store, &box, refinements, 6, 4, &certificate);
        const CheckRecord record = check(&store, &certificate);
        if (refinements == 0)
        {
            four_corner_verified = record.verified;
        }
        char chords[32];
        char steps[32];
        snprintf(chords, sizeof(chords), "%llu/%llu", (unsigned long long)record.chords,
                 (unsigned long long)record.edges);
        snprintf(steps, sizeof(steps), "%llu/%llu", (unsigned long long)record.steps, (unsigned long long)record.edges);
        fprintf(out, "  %-24s %-7lld %-10s %-10s %s\n", names[refinements], (long long)record.turns, chords, steps,
                "winding only, unproved");
        platform_cell_released(certificate.entries, platform_place_g_program_owner);
        steering_free(&store);
    }
    Steering store;
    Certificate certificate;
    uint64_t searched = 0u;
    (void)certify(&box, &store, &certificate, &searched);
    const CheckRecord record = check(&store, &certificate);
    char chords[32];
    char steps[32];
    snprintf(chords, sizeof(chords), "%llu/%llu", (unsigned long long)record.chords, (unsigned long long)record.edges);
    snprintf(steps, sizeof(steps), "%llu/%llu", (unsigned long long)record.steps, (unsigned long long)record.edges);
    fprintf(out, "  %-24s %-7lld %-10s %-10s %s\n", "searched and settled", (long long)record.turns, chords, steps,
            record.verified ? "certificate" : "FAIL");
    fputs("\n  the four corners already wind one circle, the group law satisfied, and no chord holds: a\n", out);
    fputs("  half turn at a corner hides its direction from the two ends. only the chord and step\n", out);
    fputs("  verdicts, the order test of the twiddle table, separate a proved count from a lucky one.\n", out);
    platform_cell_released(certificate.entries, platform_place_g_program_owner);
    steering_free(&store);
    return !four_corner_verified && record.verified && record.whole && (record.count == 1);
}

int program_main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *const out = stdout;
    const int64_t low = (argument_count > 1) ? atoll(arguments[1]) : 14;
    const int64_t high = (argument_count > 2) ? atoll(arguments[2]) : 26;
    fputs("  the zero certificate: a counted zero checked in one pass, the shape Proth gives a prime\n", out);
    fputs("  it claims nothing about the Riemann hypothesis; it certifies the zeros it reaches\n\n", out);
    const int line = report_line(out, low, high);
    const int rejection = report_rejection(out);
    const int floor_test = report_floor(out);
    fflush(out);
    return (line && rejection && floor_test) ? 0 : 1;
}
