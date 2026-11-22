// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "drift.h"
#include "../../../../src/platform/platform.h"
#include "../../../../includes/exact_types/integers/exact_integer.h"
#include "hessian.h"
#include "peaks.h"
#include "scan.h"
#include "../../../../src/cu/engine/analysis/shift_agreement/shift_agreement.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCAN_MEASUREMENT_MAX 65535u

#define SCAN_SEED_SCALE 100ull

#define SCAN_SEED_LOW 1ull

#define SCAN_SEED_HIGH 99ull

// .points and .shape each open with frames, depth, height, width, the residual's limbs, and a sixth word: the set's
// contrast bits in .points, the shape's limbs (the residual's and one more) in .shape. A .shape frame is its count,
// each point's faces, then each point's six differences, all in the order of the frame's .points
#define SCAN_HEADER_WORDS 6u

// .drift opens with frames, depth, height, width and the three weights; each frame then contains its positive voxels,
// and every frame after the first the lag (z, y, x) carrying the previous frame's positive set onto it and the
// agreement at that lag
#define SCAN_DRIFT_HEADER_WORDS 7u

typedef struct
{
    unsigned long long count;
    unsigned long long sum;
} ScanClass;

typedef struct
{
    unsigned long long found;
    unsigned long long lo;
    unsigned long long hi;
    unsigned long long lo_steps;
    unsigned long long hi_steps;
} ScanFooting;

typedef struct
{
    unsigned long long frames;
    unsigned long long points;
    unsigned long long max;
    unsigned long long least;
    unsigned long long on_face;
    unsigned long long pairs;
    unsigned long long moved;
    unsigned long long residual_microseconds;
    unsigned long long peaks_microseconds;
    unsigned long long hessian_microseconds;
    unsigned long long drift_microseconds;
} ScanResults;

typedef struct
{
    unsigned int capacity;
    unsigned int *voxels;
    unsigned int *levels;
    unsigned int hessian_capacity;
    unsigned int *faces;
    unsigned int *differences;
    size_t positive_capacity;
    unsigned long long *before;
    unsigned long long *after;
} ScanPoints;

typedef struct
{
    unsigned long long extent[4];
    unsigned long long voxels;
    unsigned short *lanes;
} ScanVolume;

typedef struct
{
    const unsigned long long *cumulative;
    unsigned long long readings;
    unsigned int bits;
    unsigned int input_limbs;
    unsigned int residual_limbs;
    unsigned int weights[ENGINE_AXES];
    unsigned int *planes;
    size_t plane_capacity;
} ScanContrast;

static void scan_exact_unsigned(OriorExactInteger *value, unsigned long long number)
{
    orior_exact_zero(value);
    // the low half of a 64-bit word fits one 32-bit limb
    value->limb[0] = (uint32_t)(number & 0xFFFFFFFFull);
    // the high half, shifted down, is below 2^32
    value->limb[1] = (uint32_t)(number >> 32u);
    value->sign = (number == 0ull) ? 0 : 1;
}

static void scan_classes(const unsigned long long *counts, unsigned int first, unsigned int split, unsigned int last,
                         ScanClass *lower, ScanClass *upper)
{
    memset(lower, 0, sizeof(*lower));
    memset(upper, 0, sizeof(*upper));
    for (unsigned int value = first; value <= last; value += 1u)
    {
        ScanClass *const side = (value <= split) ? lower : upper;
        side->count += counts[value];
        side->sum += counts[value] * value;
    }
}

static int scan_midpoint(const ScanClass *lower, const ScanClass *upper, unsigned int *split)
{
    OriorExactInteger lower_count;
    OriorExactInteger upper_count;
    OriorExactInteger lower_sum;
    OriorExactInteger upper_sum;
    OriorExactInteger two;
    scan_exact_unsigned(&lower_count, lower->count);
    scan_exact_unsigned(&upper_count, upper->count);
    scan_exact_unsigned(&lower_sum, lower->sum);
    scan_exact_unsigned(&upper_sum, upper->sum);
    scan_exact_unsigned(&two, 2ull);
    OriorExactInteger across;
    OriorExactInteger numerator;
    OriorExactInteger denominator;
    OriorExactInteger remainder;
    const int divided = (orior_exact_multiply(&lower_sum, &upper_count, &numerator) == ORIOR_EXACT_OK) &&
                        (orior_exact_multiply(&upper_sum, &lower_count, &across) == ORIOR_EXACT_OK) &&
                        (orior_exact_add(&numerator, &across, &numerator) == ORIOR_EXACT_OK) &&
                        (orior_exact_multiply(&lower_count, &upper_count, &denominator) == ORIOR_EXACT_OK) &&
                        (orior_exact_multiply(&denominator, &two, &denominator) == ORIOR_EXACT_OK) &&
                        (orior_exact_divide(&numerator, &denominator, &numerator, &remainder) == ORIOR_EXACT_OK);
    int fits = divided;
    for (unsigned int limb = 1u; fits && (limb < ORIOR_EXACT_LIMBS); limb += 1u)
    {
        fits = (numerator.limb[limb] == 0u);
    }
    fits = fits && (numerator.limb[0] <= SCAN_MEASUREMENT_MAX);
    *split = fits ? numerator.limb[0] : *split;
    return fits;
}

static int scan_two_means(const unsigned long long *counts, unsigned int first, unsigned int seed, unsigned int last,
                          unsigned int *split, unsigned long long *steps)
{
    unsigned int at = seed;
    unsigned int next = seed;
    unsigned long long taken = 0ull;
    int ok = 1;
    do
    {
        at = next;
        ScanClass lower;
        ScanClass upper;
        scan_classes(counts, first, at, last, &lower, &upper);
        ok = (lower.count != 0ull) && (upper.count != 0ull) && (scan_midpoint(&lower, &upper, &next) != 0);
        taken += 1ull;
    } while (ok && (next != at) && (taken <= (last - first + 1u)));
    *split = at;
    *steps = taken;
    return ok && (next == at);
}

static void scan_footing(const unsigned long long *counts, unsigned long long total, ScanFooting *footing)
{
    memset(footing, 0, sizeof(*footing));
    unsigned int least = SCAN_READINGS;
    unsigned int max = 0u;
    unsigned int median = SCAN_READINGS;
    unsigned int low_seed = SCAN_READINGS;
    unsigned int high_seed = SCAN_READINGS;
    unsigned long long running = 0ull;
    for (unsigned int value = 0u; value < SCAN_READINGS; value += 1u)
    {
        running += counts[value];
        least = ((least == SCAN_READINGS) && (counts[value] != 0ull)) ? value : least;
        max = (counts[value] != 0ull) ? value : max;
        median = ((median == SCAN_READINGS) && ((2ull * running) >= total)) ? value : median;
        low_seed = ((low_seed == SCAN_READINGS) && ((SCAN_SEED_SCALE * running) >= (SCAN_SEED_LOW * total))) ? value
                                                                                                             : low_seed;
        high_seed = ((high_seed == SCAN_READINGS) && ((SCAN_SEED_SCALE * running) >= (SCAN_SEED_HIGH * total)))
                        ? value
                        : high_seed;
    }
    unsigned int above = SCAN_READINGS;
    for (unsigned int value = median + 1u;
         (median < SCAN_MEASUREMENT_MAX) && (above == SCAN_READINGS) && (value < SCAN_READINGS); value += 1u)
    {
        above = (counts[value] != 0ull) ? value : above;
    }
    const int lower_half = (total != 0ull) && (median < SCAN_READINGS) && (least < median);
    const int upper_half = lower_half && (above < max);
    unsigned int lo = 0u;
    unsigned int hi = 0u;
    const unsigned int lower_seed = (low_seed < median) ? low_seed : (median - 1u);
    const unsigned int upper_seed = (high_seed < above) ? above : ((high_seed < max) ? high_seed : (max - 1u));
    const int lower_found =
        lower_half && (scan_two_means(counts, least, lower_seed, median, &lo, &footing->lo_steps) != 0);
    const int upper_found =
        upper_half && (scan_two_means(counts, above, upper_seed, max, &hi, &footing->hi_steps) != 0);
    footing->found = (lower_found && upper_found) ? 1ull : 0ull;
    footing->lo = lo;
    footing->hi = hi;
}

static int scan_load(const ScanRequest *request, const char *sample, ScanVolume *volume)
{
    memset(volume, 0, sizeof(*volume));
    EngineSignum root;
    const int loaded =
        (engine_iapx_load(request->set, sample, volume->extent, &volume->lanes, &root, NULL, request->error) == 0L) &&
        (volume->extent[0] != 0ull) && (volume->extent[0] <= 0xFFFFFFFFull) && (volume->extent[1] != 0ull) &&
        (volume->extent[2] != 0ull) && (volume->extent[2] <= 0xFFFFFFFFull) && (volume->extent[3] != 0ull) &&
        (volume->extent[3] <= 0xFFFFFFFFull);
    const unsigned long long plane = loaded ? (volume->extent[2] * volume->extent[3]) : 0ull;
    const int sized = loaded && (plane <= (0x7FFFFFFFull / volume->extent[1]));
    volume->voxels = sized ? (plane * volume->extent[1]) : 0ull;
    const int summed =
        sized && (volume->extent[0] <= ((0xFFFFFFFFFFFFFFFFull / SCAN_MEASUREMENT_MAX) / volume->voxels));
    if (summed == 0)
    {
        fprintf(stderr, "  scan: %s: its .iapx in %s did not load, or its frames are too wide to scan\n", sample,
                request->set);
        platform_cell_released(volume->lanes, platform_place_g_program_owner);
        volume->lanes = NULL;
    }
    return summed;
}

static int scan_points_reserve(ScanPoints *points, unsigned int capacity, unsigned int limbs)
{
    unsigned int *const voxels = (unsigned int *)platform_cell_grown(
        points->voxels, ((size_t)points->capacity + 1u) * sizeof(unsigned int),
        ((size_t)capacity + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    points->voxels = (voxels != NULL) ? voxels : points->voxels;
    unsigned int *const levels =
        (voxels != NULL)
            ? (unsigned int *)platform_cell_grown(
                  points->levels, (((size_t)points->capacity * limbs) + 1u) * sizeof(unsigned int),
                  (((size_t)capacity * limbs) + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0)
            : NULL;
    points->levels = (levels != NULL) ? levels : points->levels;
    points->capacity = (levels != NULL) ? capacity : points->capacity;
    return levels != NULL;
}

static int scan_hessian_reserve(ScanPoints *points, unsigned int count, unsigned int limbs)
{
    if (count <= points->hessian_capacity)
    {
        return 1;
    }
    unsigned int *const faces = (unsigned int *)platform_cell_grown(
        points->faces, ((size_t)points->hessian_capacity + 1u) * sizeof(unsigned int),
        ((size_t)count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    points->faces = (faces != NULL) ? faces : points->faces;
    const size_t words = ((size_t)count * HESSIAN_ENTRIES * ((size_t)limbs + 1u)) + 1u;
    unsigned int *const differences =
        (faces != NULL) ? (unsigned int *)platform_cell_grown(
                              points->differences,
                              (((size_t)points->hessian_capacity * HESSIAN_ENTRIES * ((size_t)limbs + 1u)) + 1u) *
                                  sizeof(unsigned int),
                              words * sizeof(unsigned int), platform_place_g_program_owner, 0)
                        : NULL;
    points->differences = (differences != NULL) ? differences : points->differences;
    points->hessian_capacity = (differences != NULL) ? count : points->hessian_capacity;
    return differences != NULL;
}

static int scan_positive_reserve(ScanPoints *points, size_t words)
{
    if (words <= points->positive_capacity)
    {
        return 1;
    }
    platform_cell_released(points->before, platform_place_g_program_owner);
    platform_cell_released(points->after, platform_place_g_program_owner);
    points->before = (unsigned long long *)platform_cell_taken(words * sizeof(unsigned long long),
                                                               platform_place_g_program_owner, 0);
    points->after = (unsigned long long *)platform_cell_taken(words * sizeof(unsigned long long),
                                                              platform_place_g_program_owner, 0);
    const int ok = (points->before != NULL) && (points->after != NULL);
    points->positive_capacity = ok ? words : 0u;
    return ok;
}

static unsigned long long scan_on_face(const unsigned int *voxels, unsigned int count, unsigned int depth,
                                       unsigned int height, unsigned int width)
{
    const unsigned int plane = height * width;
    unsigned long long on_face = 0ull;
    for (unsigned int point = 0u; point < count; point += 1u)
    {
        const unsigned int z = voxels[point] / plane;
        const unsigned int y = (voxels[point] % plane) / width;
        const unsigned int x = voxels[point] % width;
        on_face += ((z == 0u) || (z + 1u == depth) || (y == 0u) || (y + 1u == height) || (x == 0u) || (x + 1u == width))
                       ? 1ull
                       : 0ull;
    }
    return on_face;
}

static int scan_write(FILE *file, const void *words, size_t bytes)
{
    return (bytes == 0u) || (fwrite(words, 1u, bytes, file) == bytes);
}

static int scan_readings_write(const char *set, const char *sample, const unsigned long long *counts,
                               const ScanFooting *footing)
{
    char path[ENGINE_PATH_CAPACITY];
    FILE *const file = engine_sample_path(path, sizeof(path), set, sample, ".readings") ? fopen(path, "wb") : NULL;
    const int written = (file != NULL) && scan_write(file, counts, SCAN_READINGS * sizeof(unsigned long long)) &&
                        scan_write(file, footing, sizeof(*footing));
    const int closed = (file != NULL) && (fclose(file) == 0);
    if ((written == 0) || (closed == 0))
    {
        fprintf(stderr, "  scan: %s: the readings could not be written\n", sample);
    }
    if ((file != NULL) && ((written == 0) || (closed == 0)))
    {
        (void)remove(path);
    }
    return written && closed;
}

static int scan_set_write(const ScanRequest *request, const ScanContrast *contrast)
{
    char path[ENGINE_PATH_CAPACITY];
    const int written = snprintf(path, sizeof(path), "%s/scan.set", request->set);
    // a non-negative length is compared whole against the capacity
    FILE *const file = ((written > 0) && ((size_t)written < sizeof(path))) ? fopen(path, "wb") : NULL;
    int ok = (file != NULL) && (fprintf(file, "readings %llu\nbits %u\nsamples %u\n", contrast->readings,
                                        contrast->bits, request->count) > 0);
    for (unsigned int at = 0u; ok && (at < request->count); at += 1u)
    {
        ok = (fprintf(file, "%s\n", request->samples[at]) > 0);
    }
    const int closed = (file != NULL) && (fclose(file) == 0);
    if ((ok == 0) || (closed == 0))
    {
        fprintf(stderr, "  scan: the set's contrast could not be written to %s/scan.set\n", request->set);
    }
    if ((file != NULL) && ((ok == 0) || (closed == 0)))
    {
        (void)remove(path);
    }
    return ok && closed;
}

static int scan_ingest(const ScanRequest *request, const char *sample, unsigned long long *set_counts,
                       unsigned long long *sample_counts, unsigned long long *readings)
{
    ScanVolume volume;
    if (scan_load(request, sample, &volume) == 0)
    {
        return 0;
    }
    memset(sample_counts, 0, SCAN_READINGS * sizeof(unsigned long long));
    const unsigned long long total = volume.extent[0] * volume.voxels;
    for (unsigned long long measurement = 0ull; measurement < total; measurement += 1ull)
    {
        sample_counts[volume.lanes[measurement]] += 1ull;
    }
    platform_cell_released(volume.lanes, platform_place_g_program_owner);
    ScanFooting footing;
    scan_footing(sample_counts, total, &footing);
    const int counted = (total <= (0xFFFFFFFFFFFFFFFFull - *readings)) &&
                        scan_readings_write(request->set, sample, sample_counts, &footing);
    for (unsigned int value = 0u; counted && (value < SCAN_READINGS); value += 1u)
    {
        set_counts[value] += sample_counts[value];
    }
    *readings += counted ? total : 0ull;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ingest ");
        mmgr_line_put_columns(output_line, sample, 24u);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal_columns(output_line, total, 12u);
        mmgr_line_put(output_line, " readings; footing ");
        platform_line_out_ended();
    }
    if (footing.found != 0ull)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_decimal(output_line, footing.lo, 1u);
        mmgr_line_put(output_line, "..");
        mmgr_line_decimal(output_line, footing.hi, 1u);
        mmgr_line_put(output_line, " (");
        mmgr_line_decimal(output_line, footing.lo_steps, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, footing.hi_steps, 1u);
        mmgr_line_put(output_line, " steps)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "not found (a half contains one value)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return counted;
}

static void scan_contrast_map(const ScanContrast *contrast, const unsigned short *frame, size_t voxels)
{
    for (size_t voxel = 0u; voxel < voxels; voxel += 1u)
    {
        const unsigned long long value = contrast->cumulative[frame[voxel]];
        for (unsigned int limb = 0u; limb < contrast->input_limbs; limb += 1u)
        {
            // one 32-bit limb of the count, taken from the bottom after the shift
            contrast->planes[((size_t)limb * voxels) + voxel] = (unsigned int)(value >> (32u * limb));
        }
    }
}

static int scan_frames(const ScanRequest *request, const char *sample, const ScanVolume *volume, ScanContrast *contrast,
                       ScanPoints *points, ScanResults *results)
{
    // every extent is bounded at or below 2^32 - 1 by scan_load. Each narrows to unsigned int exactly
    const unsigned int header[SCAN_HEADER_WORDS] = {(unsigned int)volume->extent[0], (unsigned int)volume->extent[1],
                                                    (unsigned int)volume->extent[2], (unsigned int)volume->extent[3],
                                                    contrast->residual_limbs,        contrast->bits};
    // the residual's limbs are below 2^30 for any orders of 32 bits. One more limb does not wrap
    const unsigned int hessian_header[SCAN_HEADER_WORDS] = {
        header[0], header[1], header[2], header[3], contrast->residual_limbs, contrast->residual_limbs + 1u};
    const unsigned int drift_header[SCAN_DRIFT_HEADER_WORDS] = {
        header[0], header[1], header[2], header[3], contrast->weights[0], contrast->weights[1], contrast->weights[2]};
    const size_t voxels = (size_t)volume->voxels;
    const size_t plane_words = voxels * contrast->input_limbs;
    unsigned int *const planes =
        (plane_words > contrast->plane_capacity)
            ? (unsigned int *)platform_cell_grown(contrast->planes, contrast->plane_capacity * sizeof(unsigned int),
                                                  plane_words * sizeof(unsigned int), platform_place_g_program_owner, 0)
            : contrast->planes;
    contrast->planes = (planes != NULL) ? planes : contrast->planes;
    contrast->plane_capacity =
        ((planes != NULL) && (plane_words > contrast->plane_capacity)) ? plane_words : contrast->plane_capacity;
    char path[ENGINE_PATH_CAPACITY];
    FILE *const file = ((planes != NULL) && engine_sample_path(path, sizeof(path), request->set, sample, ".points"))
                           ? fopen(path, "wb")
                           : NULL;
    char hessian_path[ENGINE_PATH_CAPACITY];
    FILE *const hessian_file =
        ((file != NULL) && engine_sample_path(hessian_path, sizeof(hessian_path), request->set, sample, ".shape"))
            ? fopen(hessian_path, "wb")
            : NULL;
    char drift_path[ENGINE_PATH_CAPACITY];
    FILE *const drift_file =
        ((hessian_file != NULL) && engine_sample_path(drift_path, sizeof(drift_path), request->set, sample, ".drift"))
            ? fopen(drift_path, "wb")
            : NULL;
    int ok = (file != NULL) && (hessian_file != NULL) && (drift_file != NULL) &&
             scan_positive_reserve(points, (size_t)DRIFT_WORDS(voxels)) && scan_write(file, header, sizeof(header)) &&
             scan_write(file, &contrast->readings, sizeof(contrast->readings)) &&
             scan_write(file, contrast->cumulative, SCAN_READINGS * sizeof(unsigned long long)) &&
             scan_write(hessian_file, hessian_header, sizeof(hessian_header)) &&
             scan_write(drift_file, drift_header, sizeof(drift_header));
    for (unsigned int frame = 0u; ok && (frame < header[0]); frame += 1u)
    {
        scan_contrast_map(contrast, &volume->lanes[(size_t)frame * voxels], voxels);
        EngineResidualPlanesRequest residual;
        memset(&residual, 0, sizeof(residual));
        residual.planes = contrast->planes;
        residual.input_bits = contrast->bits;
        residual.depth = header[1];
        residual.height = header[2];
        residual.width = header[3];
        memcpy(residual.smooth_orders, request->smooth_orders, sizeof(residual.smooth_orders));
        memcpy(residual.background_orders, request->background_orders, sizeof(residual.background_orders));
        residual.error = request->error;
        const unsigned int *device_residual = NULL;
        unsigned int limbs = 0u;
        unsigned long long mark = engine_clock_microseconds();
        ok = (engine_residual_planes(&residual, &device_residual, &limbs) == 0L) && (limbs == contrast->residual_limbs);
        results->residual_microseconds += engine_clock_microseconds() - mark;
        mark = engine_clock_microseconds();
        PeaksRequest peaks = {device_residual, header[1],        header[2],      header[3],
                              limbs,           points->capacity, points->voxels, points->levels};
        long found = ok ? peaks_find(&peaks) : PEAKS_ERROR;
        // a count past the capacity is not negative. It re-signs to unsigned long long exactly
        if ((found != PEAKS_ERROR) && ((unsigned long long)found > points->capacity))
        {
            // the count is at max the frame's voxels, below 2^31. It narrows to unsigned int exactly
            ok = scan_points_reserve(points, (unsigned int)found, limbs);
            peaks.capacity = points->capacity;
            peaks.voxels = points->voxels;
            peaks.levels = points->levels;
            found = ok ? peaks_find(&peaks) : PEAKS_ERROR;
        }
        results->peaks_microseconds += engine_clock_microseconds() - mark;
        // a count within the capacity is not negative and at max the capacity. It narrows to unsigned int exactly
        const unsigned int count =
            ((found != PEAKS_ERROR) && ((unsigned long long)found <= points->capacity)) ? (unsigned int)found : 0u;
        ok = ok && (found != PEAKS_ERROR) && ((unsigned long long)found <= points->capacity) &&
             scan_write(file, &count, sizeof(count)) &&
             scan_write(file, points->voxels, (size_t)count * sizeof(unsigned int)) &&
             scan_write(file, points->levels, (size_t)count * limbs * sizeof(unsigned int));
        // S1b and O4 at each point, read from this frame's residual before the next frame's overwrites it
        mark = engine_clock_microseconds();
        ok = ok && scan_hessian_reserve(points, count, limbs);
        const HessianRequest hessian = {device_residual, header[1],      header[2],     header[3],          limbs,
                                        count,           points->voxels, points->faces, points->differences};
        const long shaped = ok ? hessian_find(&hessian) : HESSIAN_ERROR;
        // a count within the capacity is below 2^31. It widens to long exactly
        ok = ok && (shaped == (long)count) && scan_write(hessian_file, &count, sizeof(count)) &&
             scan_write(hessian_file, points->faces, (size_t)count * sizeof(unsigned int)) &&
             scan_write(hessian_file, points->differences,
                        (size_t)count * HESSIAN_ENTRIES * ((size_t)limbs + 1u) * sizeof(unsigned int));
        results->hessian_microseconds += engine_clock_microseconds() - mark;
        // the faces the device recorded name a face for exactly the points the host finds on one
        const unsigned long long on_face =
            ok ? scan_on_face(points->voxels, count, header[1], header[2], header[3]) : 0ull;
        unsigned long long faced = 0ull;
        for (unsigned int point = 0u; ok && (point < count); point += 1u)
        {
            faced += (points->faces[point] != 0u) ? 1ull : 0ull;
        }
        if (ok && (faced != on_face))
        {
            fprintf(stderr, "  scan: %s: frame %u: %llu points have faces from the device, %llu are on a face\n",
                    sample, frame, faced, on_face);
            ok = 0;
        }
        // S2: the frame's positive set, and the lag that best carries the previous frame's onto it
        mark = engine_clock_microseconds();
        const DriftPositiveRequest positive = {device_residual, header[1], header[2], header[3], limbs, points->after};
        const long positives = ok ? drift_positive(&positive) : DRIFT_ERROR;
        // a count that is not an error is not negative and is below 2^31. It narrows to unsigned int exactly
        const unsigned int positive_count = (positives >= 0L) ? (unsigned int)positives : 0u;
        ok = ok && (positives >= 0L) && scan_write(drift_file, &positive_count, sizeof(positive_count));
        if (ok && (frame != 0u))
        {
            ShiftAgreementRequest agreement;
            memset(&agreement, 0, sizeof(agreement));
            agreement.axes = ENGINE_AXES;
            agreement.extents[0] = header[1];
            agreement.extents[1] = header[2];
            agreement.extents[2] = header[3];
            memcpy(agreement.weights, contrast->weights, sizeof(contrast->weights));
            agreement.before = points->before;
            agreement.after = points->after;
            agreement.counts = NULL;
            const long agreed = shift_agreement_run(&agreement);
            if (agreed != 0L)
            {
                fprintf(stderr, "  scan: %s: the drift errored frames %u and %u\n", sample, frame - 1u, frame);
            }
            ok = (agreed == 0L) && scan_write(drift_file, agreement.lag, ENGINE_AXES * sizeof(int)) &&
                 scan_write(drift_file, &agreement.agreement, sizeof(agreement.agreement));
            results->pairs += ok ? 1ull : 0ull;
            results->moved +=
                (ok && ((agreement.lag[0] != 0) || (agreement.lag[1] != 0) || (agreement.lag[2] != 0))) ? 1ull : 0ull;
        }
        unsigned long long *const carried = points->before;
        points->before = points->after;
        points->after = carried;
        results->drift_microseconds += engine_clock_microseconds() - mark;
        results->frames += ok ? 1ull : 0ull;
        results->points += ok ? count : 0ull;
        results->max = (ok && (count > results->max)) ? count : results->max;
        results->least = (ok && (count < results->least)) ? count : results->least;
        results->on_face += ok ? on_face : 0ull;
    }
    const int closed = (file != NULL) && (fclose(file) == 0);
    const int hessian_closed = (hessian_file != NULL) && (fclose(hessian_file) == 0);
    const int drift_closed = (drift_file != NULL) && (fclose(drift_file) == 0);
    const int complete = ok && closed && hessian_closed && drift_closed;
    if (complete == 0)
    {
        fprintf(stderr, "  scan: %s: the points stopped at frame %llu of %u\n", sample, results->frames, header[0]);
    }
    if ((file != NULL) && (complete == 0))
    {
        (void)remove(path);
    }
    if ((hessian_file != NULL) && (complete == 0))
    {
        (void)remove(hessian_path);
    }
    if ((drift_file != NULL) && (complete == 0))
    {
        (void)remove(drift_path);
    }
    return complete;
}

static int scan_sample(const ScanRequest *request, const char *sample, ScanContrast *contrast, ScanPoints *points,
                       ScanResults *results)
{
    memset(results, 0, sizeof(*results));
    results->least = ~0ull;
    ScanVolume volume;
    const int ok = (scan_load(request, sample, &volume) != 0) &&
                   (scan_frames(request, sample, &volume, contrast, points, results) != 0);
    platform_cell_released(volume.lanes, platform_place_g_program_owner);
    return ok;
}

static unsigned int scan_residual_limbs(const ScanRequest *request, unsigned int bits)
{
    unsigned long long residual_bits = (unsigned long long)bits + 1ull;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        residual_bits += (unsigned long long)request->smooth_orders[axis] + request->background_orders[axis];
    }
    const unsigned long long limbs = (residual_bits + 31ull) / 32ull;
    // a width past 2^32 limbs errors as no width at all; below it the limbs narrow to unsigned int exactly
    return (limbs <= 0xFFFFFFFFull) ? (unsigned int)limbs : 0u;
}

extern "C" long scan_set(const ScanRequest *request)
{
    if ((request == NULL) || (request->set == NULL) || (request->samples == NULL) || (request->count == 0u) ||
        (request->error == NULL))
    {
        return SCAN_ERROR;
    }
    ScanContrast contrast;
    memset(&contrast, 0, sizeof(contrast));
    if (drift_weights(request->voxel_pm, contrast.weights) == 0)
    {
        fprintf(stderr,
                "  scan: voxel_pm %llu, %llu, %llu gives the drift no weights: a size is zero, or a size over"
                " their greatest common divisor squares past 32 bits\n",
                request->voxel_pm[0], request->voxel_pm[1], request->voxel_pm[2]);
        return SCAN_ERROR;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the drift's weights: ");
        mmgr_line_decimal(output_line, contrast.weights[0], 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, contrast.weights[1], 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, contrast.weights[2], 1u);
        mmgr_line_put(output_line, ", each the square of voxel_pm ");
        mmgr_line_decimal(output_line, request->voxel_pm[0], 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, request->voxel_pm[1], 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, request->voxel_pm[2], 1u);
        mmgr_line_put(output_line, " over their greatest common divisor");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned long long *const cumulative = (unsigned long long *)platform_cell_cleared(
        SCAN_READINGS, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    unsigned long long *const sample_counts = (unsigned long long *)platform_cell_taken(
        SCAN_READINGS * sizeof(unsigned long long), platform_place_g_program_owner, 0);
    int ok = (cumulative != NULL) && (sample_counts != NULL);
    for (unsigned int at = 0u; ok && (at < request->count); at += 1u)
    {
        ok = scan_ingest(request, request->samples[at], cumulative, sample_counts, &contrast.readings);
    }
    unsigned long long running = 0ull;
    for (unsigned int value = 0u; ok && (value < SCAN_READINGS); value += 1u)
    {
        running += cumulative[value];
        cumulative[value] = running;
    }
    while (ok && (contrast.bits < 64u) && ((contrast.readings >> contrast.bits) != 0ull))
    {
        contrast.bits += 1u;
    }
    contrast.cumulative = cumulative;
    contrast.input_limbs = (contrast.bits + 31u) / 32u;
    contrast.residual_limbs = scan_residual_limbs(request, contrast.bits);
    ok = ok && (contrast.bits != 0u) && (contrast.residual_limbs != 0u) && scan_set_write(request, &contrast);
    if (ok == 0)
    {
        fprintf(stderr, "  scan: the set was not ingested whole, so no frame is scanned\n");
        platform_cell_released(cumulative, platform_place_g_program_owner);
        platform_cell_released(sample_counts, platform_place_g_program_owner);
        return SCAN_ERROR;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the set: ");
        mmgr_line_decimal(output_line, contrast.readings, 1u);
        mmgr_line_put(output_line, " readings; the contrast is their cumulative count, ");
        mmgr_line_decimal(output_line, contrast.bits, 1u);
        mmgr_line_put(output_line, " bits in ");
        mmgr_line_decimal(output_line, contrast.input_limbs, 1u);
        mmgr_line_put(output_line, " limbs; the residual ");
        mmgr_line_decimal(output_line, contrast.residual_limbs, 1u);
        mmgr_line_put(output_line, " limbs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "sample", 24u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "frames", 6u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "points", 10u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, "a frame least..max, on a face; pairs moved of"
                                   " pairs; residual us, peaks us, hessian us, drift us");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    ScanPoints points;
    memset(&points, 0, sizeof(points));
    ScanResults totals;
    memset(&totals, 0, sizeof(totals));
    totals.least = ~0ull;
    unsigned int failed = 0u;
    for (unsigned int at = 0u; at < request->count; at += 1u)
    {
        ScanResults results;
        const char *const sample = request->samples[at];
        if (scan_sample(request, sample, &contrast, &points, &results) == 0)
        {
            fprintf(stderr, "  scan: %s failed\n", sample);
            failed += 1u;
            continue;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, sample, 24u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, results.frames, 6u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, results.points, 10u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, results.least, 1u);
            mmgr_line_put(output_line, "..");
            mmgr_line_decimal(output_line, results.max, 1u);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, results.on_face, 1u);
            mmgr_line_put(output_line, "; ");
            mmgr_line_decimal(output_line, results.moved, 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal(output_line, results.pairs, 1u);
            mmgr_line_put(output_line, "; ");
            mmgr_line_decimal(output_line, results.residual_microseconds, 1u);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, results.peaks_microseconds, 1u);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, results.hessian_microseconds, 1u);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, results.drift_microseconds, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        totals.frames += results.frames;
        totals.points += results.points;
        totals.max = (results.max > totals.max) ? results.max : totals.max;
        totals.least = (results.least < totals.least) ? results.least : totals.least;
        totals.on_face += results.on_face;
        totals.pairs += results.pairs;
        totals.moved += results.moved;
        totals.residual_microseconds += results.residual_microseconds;
        totals.peaks_microseconds += results.peaks_microseconds;
        totals.hessian_microseconds += results.hessian_microseconds;
        totals.drift_microseconds += results.drift_microseconds;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  scanned ");
        mmgr_line_decimal(output_line, request->count - failed, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, request->count, 1u);
        mmgr_line_put(output_line, " samples: ");
        mmgr_line_decimal(output_line, totals.frames, 1u);
        mmgr_line_put(output_line, " frames, ");
        mmgr_line_decimal(output_line, totals.points, 1u);
        mmgr_line_put(output_line, " points, ");
        mmgr_line_decimal(output_line, (totals.frames != 0ull) ? totals.least : 0ull, 1u);
        mmgr_line_put(output_line, "..");
        mmgr_line_decimal(output_line, totals.max, 1u);
        mmgr_line_put(output_line, " a frame, ");
        mmgr_line_decimal(output_line, totals.on_face, 1u);
        mmgr_line_put(output_line, " on a face; ");
        mmgr_line_decimal(output_line, totals.moved, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, totals.pairs, 1u);
        mmgr_line_put(output_line, " frame pairs moved; residual ");
        mmgr_line_decimal(output_line, totals.residual_microseconds, 1u);
        mmgr_line_put(output_line, " us, peaks ");
        mmgr_line_decimal(output_line, totals.peaks_microseconds, 1u);
        mmgr_line_put(output_line, " us, hessian ");
        mmgr_line_decimal(output_line, totals.hessian_microseconds, 1u);
        mmgr_line_put(output_line, " us, drift ");
        mmgr_line_decimal(output_line, totals.drift_microseconds, 1u);
        mmgr_line_put(output_line, " us");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(cumulative, platform_place_g_program_owner);
    platform_cell_released(sample_counts, platform_place_g_program_owner);
    platform_cell_released(contrast.planes, platform_place_g_program_owner);
    platform_cell_released(points.voxels, platform_place_g_program_owner);
    platform_cell_released(points.levels, platform_place_g_program_owner);
    platform_cell_released(points.faces, platform_place_g_program_owner);
    platform_cell_released(points.differences, platform_place_g_program_owner);
    platform_cell_released(points.before, platform_place_g_program_owner);
    platform_cell_released(points.after, platform_place_g_program_owner);
    return (failed == 0u) ? 0L : SCAN_ERROR;
}
