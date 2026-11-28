// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// points_check: S0's and S1's files, read apart from the scan by a reader of their own. From the set's scan.set it
// takes the samples the contrast was counted over and sums their .readings; from that sum it rebuilds the set's
// readings, the contrast's bits (the least b with readings >> b == 0) and the contrast C (the running count). scan.set
// must name the same readings and bits. Then each named sample's .points: its head (frames, depth, height, width,
// limbs, bits), the set's readings and C, which must agree across the samples and equal the rebuilt ones; and each
// frame, whose count must fit the view, whose voxels must rise strictly and lie inside the view, and whose levels must
// each be positive (the top limb's top bit clear, and not every limb 0). A sample's readings must be its frames times
// its view, and its .points must end at its last frame. Reads only; every word is little-endian, as the scan writes it.
//   points_check <set> <sample> [<sample> ...]
//
// The formats, restated here from what the scan writes instead of read from its code: a change to them must be made
// here too.
//   scan.set   "readings R", "bits B", "samples S", then the S names, a line each
//   .readings  65536 64-bit counts, one a reading's value, then the footing: found, lo, hi, lo's steps, hi's steps
//   .points    six 32-bit words (frames, depth, height, width, limbs, bits), the readings (64 bits), C (65536 64-bit
//              words), then a frame after another: its count, each point's voxel, each point's levels (limbs words,
//              the lowest first)
#include "../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define POINTS_CHECK_READINGS 65536u

#define POINTS_CHECK_FOOTING_WORDS 5u

#define POINTS_CHECK_HEAD_WORDS 6u

#define POINTS_CHECK_PATH 4096u

#define POINTS_CHECK_LINE 4096u

#define POINTS_CHECK_TOP_BIT 0x80000000u

// the faults, each counted and named in the last line
enum
{
    POINTS_CHECK_SET_UNREAD = 0,
    POINTS_CHECK_READINGS_UNREAD,
    POINTS_CHECK_SUM_PAST_64,
    POINTS_CHECK_SET_NOT_REBUILT,
    POINTS_CHECK_NOT_LISTED,
    POINTS_CHECK_POINTS_UNREAD,
    POINTS_CHECK_HEAD_DIFFERS,
    POINTS_CHECK_HEAD_NOT_REBUILT,
    POINTS_CHECK_EMPTY_VIEW,
    POINTS_CHECK_COUNT_PAST_VIEW,
    POINTS_CHECK_FRAME_SHORT,
    POINTS_CHECK_OUT_OF_ORDER,
    POINTS_CHECK_OUTSIDE,
    POINTS_CHECK_NOT_POSITIVE,
    POINTS_CHECK_PAST_LAST_FRAME,
    POINTS_CHECK_READINGS_NOT_VIEW,
    POINTS_CHECK_FAULTS
};

static const char *const POINTS_CHECK_FAULT_NAMES[POINTS_CHECK_FAULTS] = {
    "scan.set does not read",
    "a .readings does not read whole",
    "a sum passes 64 bits",
    "scan.set is not the rebuilt readings and bits",
    "a sample not in scan.set",
    "a .points does not open or its head does not read",
    "a head differs from the first sample's",
    "a head is not the rebuilt readings, bits and C",
    "a view or frame count of 0",
    "a frame's count past its view",
    "a frame stops short",
    "voxels out of order",
    "voxels outside the view",
    "levels not positive",
    "a .points goes on past its last frame",
    "a sample's readings are not its frames times its view"};

typedef struct
{
    char **names;
    unsigned int count;
    unsigned long long readings;
    unsigned int bits;
    unsigned long long *totals;
    unsigned long long *counts;
    unsigned long long *contrast;
    unsigned long long rebuilt_readings;
    unsigned int rebuilt_bits;
} PointsCheckSet;

typedef struct
{
    unsigned int *voxels;
    unsigned int *levels;
    size_t voxel_capacity;
    size_t level_capacity;
} PointsCheckWorkspace;

typedef struct
{
    int recorded;
    unsigned int head[POINTS_CHECK_HEAD_WORDS];
    unsigned long long readings;
    unsigned long long *contrast;
} PointsCheckFirst;

static int points_check_path(char *path, size_t capacity, const char *set, const char *sample, const char *suffix)
{
    const int written = snprintf(path, capacity, "%s/%s/%s%s", set, sample, sample, suffix);
    // a non-negative length is compared whole against the capacity
    return (written > 0) && ((size_t)written < capacity);
}

static int points_check_read(FILE *file, void *words, size_t bytes)
{
    return (bytes == 0u) || (fread(words, 1u, bytes, file) == bytes);
}

// the file's place is its end: nothing past what was read
static int points_check_ended(FILE *file)
{
    return (fgetc(file) == EOF) && (feof(file) != 0);
}

// a line of scan.set with its line feed taken off, or 0 when none reads or it does not fit
static int points_check_line(FILE *file, char *line, size_t capacity)
{
    if (fgets(line, (int)capacity, file) == NULL)
    {
        return 0;
    }
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if ((length == 0u) || (line[length - 1u] != '\n'))
    {
        return 0;
    }
    line[length - 1u] = '\0';
    return 1;
}

static void points_check_set_free(PointsCheckSet *set)
{
    for (unsigned int at = 0u; (set->names != NULL) && (at < set->count); at += 1u)
    {
        platform_cell_released(set->names[at], platform_place_g_program_owner);
    }
    platform_cell_released(set->names, platform_place_g_program_owner);
    platform_cell_released(set->totals, platform_place_g_program_owner);
    platform_cell_released(set->counts, platform_place_g_program_owner);
    platform_cell_released(set->contrast, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = set, .bytes = sizeof(*set), .val = 0);
}

// scan.set's readings, bits and names
static int points_check_set_read(const char *root, PointsCheckSet *set)
{
    char path[POINTS_CHECK_PATH];
    const int written = snprintf(path, sizeof(path), "%s/scan.set", root);
    FILE *const file = ((written > 0) && ((size_t)written < sizeof(path))) ? fopen(path, "rb") : NULL;
    char line[POINTS_CHECK_LINE];
    char rest = '\0';
    int ok = (file != NULL) && points_check_line(file, line, sizeof(line)) &&
             (sscanf(line, "readings %llu%c", &set->readings, &rest) == 1) &&
             points_check_line(file, line, sizeof(line)) && (sscanf(line, "bits %u%c", &set->bits, &rest) == 1) &&
             points_check_line(file, line, sizeof(line)) && (sscanf(line, "samples %u%c", &set->count, &rest) == 1) &&
             (set->count != 0u);
    set->names =
        ok ? (char **)platform_cell_cleared(set->count, sizeof(char *), platform_place_g_program_owner, 0) : NULL;
    set->totals = ok ? (unsigned long long *)platform_cell_cleared(set->count, sizeof(unsigned long long),
                                                                   platform_place_g_program_owner, 0)
                     : NULL;
    ok = ok && (set->names != NULL) && (set->totals != NULL);
    for (unsigned int at = 0u; ok && (at < set->count); at += 1u)
    {
        ok = points_check_line(file, line, sizeof(line)) && (line[0] != '\0');
        set->names[at] =
            ok ? (char *)platform_cell_taken(
                     EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
                     platform_place_g_program_owner, 0)
               : NULL;
        ok = ok && (set->names[at] != NULL);
        if (ok)
        {
            EMBED_CALL(
                memor.cpy, MemoriaCfg, .dst = set->names[at], .src = line,
                .bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
        }
    }
    ok = ok && points_check_ended(file);
    if (file != NULL)
    {
        fclose(file);
    }
    if (ok == 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, path);
        mmgr_line_put(output_line,
                      " does not read: readings, bits, samples and one name a line, and nothing after them");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return ok;
}

// every sample's .readings summed; the set's readings, bits and C rebuilt from the sum; the footings' ranges printed
static int points_check_rebuild(const char *root, PointsCheckSet *set, unsigned long long *faults)
{
    set->counts = (unsigned long long *)platform_cell_cleared(POINTS_CHECK_READINGS, sizeof(unsigned long long),
                                                              platform_place_g_program_owner, 0);
    set->contrast = (unsigned long long *)platform_cell_cleared(POINTS_CHECK_READINGS, sizeof(unsigned long long),
                                                                platform_place_g_program_owner, 0);
    unsigned long long *const one = (unsigned long long *)platform_cell_taken(
        POINTS_CHECK_READINGS * sizeof(unsigned long long), platform_place_g_program_owner, 0);
    if ((set->counts == NULL) || (set->contrast == NULL) || (one == NULL))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  the counts' memory could not be reserved");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_cell_released(one, platform_place_g_program_owner);
        return 0;
    }
    unsigned long long found_count = 0ull;
    unsigned long long least[POINTS_CHECK_FOOTING_WORDS];
    unsigned long long max[POINTS_CHECK_FOOTING_WORDS];
    for (unsigned int word = 0u; word < POINTS_CHECK_FOOTING_WORDS; word += 1u)
    {
        least[word] = ~0ull;
        max[word] = 0ull;
    }
    int complete = 1;
    for (unsigned int at = 0u; at < set->count; at += 1u)
    {
        char path[POINTS_CHECK_PATH];
        unsigned long long footing[POINTS_CHECK_FOOTING_WORDS];
        FILE *const file =
            points_check_path(path, sizeof(path), root, set->names[at], ".readings") ? fopen(path, "rb") : NULL;
        const int read = (file != NULL) && points_check_read(file, one, POINTS_CHECK_READINGS * sizeof(*one)) &&
                         points_check_read(file, footing, sizeof(footing)) && points_check_ended(file);
        if (file != NULL)
        {
            fclose(file);
        }
        if (read == 0)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, set->names[at]);
                mmgr_line_put(
                    output_line,
                    ": its .readings does not read whole: 65536 counts and a footing of 5 words, nothing after");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            faults[POINTS_CHECK_READINGS_UNREAD] += 1ull;
            complete = 0;
            continue;
        }
        unsigned long long total = 0ull;
        for (unsigned int value = 0u; value < POINTS_CHECK_READINGS; value += 1u)
        {
            const int fits = (one[value] <= (~0ull - total)) && (one[value] <= (~0ull - set->counts[value]));
            faults[POINTS_CHECK_SUM_PAST_64] += fits ? 0ull : 1ull;
            complete = complete && fits;
            total += fits ? one[value] : 0ull;
            set->counts[value] += fits ? one[value] : 0ull;
        }
        set->totals[at] = total;
        found_count += (footing[0] == 1ull) ? 1ull : 0ull;
        for (unsigned int word = 1u; (footing[0] == 1ull) && (word < POINTS_CHECK_FOOTING_WORDS); word += 1u)
        {
            least[word] = (footing[word] < least[word]) ? footing[word] : least[word];
            max[word] = (footing[word] > max[word]) ? footing[word] : max[word];
        }
    }
    platform_cell_released(one, platform_place_g_program_owner);
    unsigned long long running = 0ull;
    for (unsigned int value = 0u; value < POINTS_CHECK_READINGS; value += 1u)
    {
        const int fits = set->counts[value] <= (~0ull - running);
        faults[POINTS_CHECK_SUM_PAST_64] += fits ? 0ull : 1ull;
        complete = complete && fits;
        running += fits ? set->counts[value] : 0ull;
        set->contrast[value] = running;
    }
    set->rebuilt_readings = running;
    set->rebuilt_bits = 0u;
    while ((set->rebuilt_bits < 64u) && ((running >> set->rebuilt_bits) != 0ull))
    {
        set->rebuilt_bits += 1u;
    }
    const int agrees = complete && (set->rebuilt_readings == set->readings) && (set->rebuilt_bits == set->bits);
    faults[POINTS_CHECK_SET_NOT_REBUILT] += agrees ? 0ull : 1ull;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  rebuilt from the ");
        mmgr_line_decimal(output_line, set->count, 1u);
        mmgr_line_put(output_line, " samples' .readings: ");
        mmgr_line_decimal(output_line, set->rebuilt_readings, 1u);
        mmgr_line_put(output_line, " readings, so the contrast is ");
        mmgr_line_decimal(output_line, set->rebuilt_bits, 1u);
        mmgr_line_put(output_line, " bits; scan.set reads ");
        mmgr_line_decimal(output_line, set->readings, 1u);
        mmgr_line_put(output_line, " readings and ");
        mmgr_line_decimal(output_line, set->bits, 1u);
        mmgr_line_put(output_line, " bits: ");
        mmgr_line_put(output_line, agrees ? "the same" : "NOT the same");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the footings as stored: found on ");
        mmgr_line_decimal(output_line, found_count, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, set->count, 1u);
        mmgr_line_put(output_line, "; lo ");
        mmgr_line_decimal(output_line, least[1], 1u);
        mmgr_line_put(output_line, " to ");
        mmgr_line_decimal(output_line, max[1], 1u);
        mmgr_line_put(output_line, ", hi ");
        mmgr_line_decimal(output_line, least[2], 1u);
        mmgr_line_put(output_line, " to ");
        mmgr_line_decimal(output_line, max[2], 1u);
        mmgr_line_put(output_line, ", lo's steps ");
        mmgr_line_decimal(output_line, least[3], 1u);
        mmgr_line_put(output_line, " to ");
        mmgr_line_decimal(output_line, max[3], 1u);
        mmgr_line_put(output_line, ", hi's steps ");
        mmgr_line_decimal(output_line, least[4], 1u);
        mmgr_line_put(output_line, " to ");
        mmgr_line_decimal(output_line, max[4], 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return complete;
}

static int points_check_reserve(PointsCheckWorkspace *capacity, size_t voxels, size_t levels)
{
    if (voxels > capacity->voxel_capacity)
    {
        unsigned int *const grown = (unsigned int *)platform_cell_grown(
            capacity->voxels, (capacity->voxel_capacity + 1u) * sizeof(unsigned int),
            (voxels + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
        capacity->voxels = (grown != NULL) ? grown : capacity->voxels;
        capacity->voxel_capacity = (grown != NULL) ? voxels : capacity->voxel_capacity;
    }
    if (levels > capacity->level_capacity)
    {
        unsigned int *const grown = (unsigned int *)platform_cell_grown(
            capacity->levels, (capacity->level_capacity + 1u) * sizeof(unsigned int),
            (levels + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
        capacity->levels = (grown != NULL) ? grown : capacity->levels;
        capacity->level_capacity = (grown != NULL) ? levels : capacity->level_capacity;
    }
    return (voxels <= capacity->voxel_capacity) && (levels <= capacity->level_capacity);
}

// one sample's .points against the set, its faults added to `faults`; 1 when it contains none
static int points_check_sample(const char *root, const char *sample, const PointsCheckSet *set, PointsCheckFirst *first,
                               PointsCheckWorkspace *capacity, unsigned long long *faults,
                               unsigned long long *points_seen)
{
    unsigned long long mine[POINTS_CHECK_FAULTS];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = mine, .bytes = sizeof(mine), .val = 0);
    unsigned int listed = set->count;
    for (unsigned int at = 0u; at < set->count; at += 1u)
    {
        listed = ((listed == set->count) &&
                  (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = set->names[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                              .other = sample, .ci = EMBED_FALSE)))
                     ? at
                     : listed;
    }
    mine[POINTS_CHECK_NOT_LISTED] += (listed == set->count) ? 1ull : 0ull;
    char path[POINTS_CHECK_PATH];
    FILE *const file = points_check_path(path, sizeof(path), root, sample, ".points") ? fopen(path, "rb") : NULL;
    unsigned int head[POINTS_CHECK_HEAD_WORDS];
    unsigned long long readings = 0ull;
    unsigned long long *const contrast = (unsigned long long *)platform_cell_taken(
        POINTS_CHECK_READINGS * sizeof(unsigned long long), platform_place_g_program_owner, 0);
    int ok = (file != NULL) && (contrast != NULL) && points_check_read(file, head, sizeof(head)) &&
             points_check_read(file, &readings, sizeof(readings)) &&
             points_check_read(file, contrast, POINTS_CHECK_READINGS * sizeof(unsigned long long));
    if (ok == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, sample);
            mmgr_line_put(output_line, ": its .points does not open, or its head does not read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        faults[POINTS_CHECK_POINTS_UNREAD] += 1ull;
        faults[POINTS_CHECK_NOT_LISTED] += mine[POINTS_CHECK_NOT_LISTED];
        if (file != NULL)
        {
            fclose(file);
        }
        platform_cell_released(contrast, platform_place_g_program_owner);
        return 0;
    }
    unsigned int contrast_differs = POINTS_CHECK_READINGS;
    for (unsigned int value = 0u; value < POINTS_CHECK_READINGS; value += 1u)
    {
        contrast_differs = ((contrast_differs == POINTS_CHECK_READINGS) && (contrast[value] != set->contrast[value]))
                               ? value
                               : contrast_differs;
    }
    const int rebuilt = (head[5] == set->rebuilt_bits) && (readings == set->rebuilt_readings) &&
                        (contrast_differs == POINTS_CHECK_READINGS);
    mine[POINTS_CHECK_HEAD_NOT_REBUILT] += rebuilt ? 0ull : 1ull;
    if (first->recorded == 0)
    {
        first->recorded = 1;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = first->head, .src = head, .bytes = sizeof(head));
        first->readings = readings;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = first->contrast, .src = contrast,
                   .bytes = POINTS_CHECK_READINGS * sizeof(unsigned long long));
    }
    const int agrees = (head[4] == first->head[4]) && (head[5] == first->head[5]) && (readings == first->readings) &&
                       (EMBED_CALL(memor.cmp, MemoriaCfg, .src = contrast, .other = first->contrast,
                                   .bytes = POINTS_CHECK_READINGS * sizeof(unsigned long long)) == 0);
    mine[POINTS_CHECK_HEAD_DIFFERS] += agrees ? 0ull : 1ull;
    platform_cell_released(contrast, platform_place_g_program_owner);
    const unsigned long long plane = (unsigned long long)head[2] * head[3];
    const unsigned long long view = plane * head[1];
    mine[POINTS_CHECK_EMPTY_VIEW] += ((head[0] == 0u) || (view == 0ull)) ? 1ull : 0ull;
    const unsigned int limbs = head[4];
    unsigned long long points = 0ull;
    unsigned int least = 0xFFFFFFFFu;
    unsigned int max = 0u;
    unsigned int frame = 0u;
    for (frame = 0u; ok && (frame < head[0]); frame += 1u)
    {
        unsigned int count = 0u;
        ok = points_check_read(file, &count, sizeof(count));
        if (ok && ((unsigned long long)count > view))
        {
            mine[POINTS_CHECK_COUNT_PAST_VIEW] += 1ull;
            ok = 0;
        }
        // a count within the view is below 2^32, and its levels are stored only when their words fit a size_t
        const unsigned long long words = (unsigned long long)count * limbs;
        ok = ok && ((limbs == 0u) || ((words / limbs) == count)) && (words <= (SIZE_MAX / sizeof(unsigned int))) &&
             points_check_reserve(capacity, (size_t)count, (size_t)words) &&
             points_check_read(file, capacity->voxels, (size_t)count * sizeof(unsigned int)) &&
             points_check_read(file, capacity->levels, (size_t)words * sizeof(unsigned int));
        if (ok == 0)
        {
            mine[POINTS_CHECK_FRAME_SHORT] += (mine[POINTS_CHECK_COUNT_PAST_VIEW] == 0ull) ? 1ull : 0ull;
            break;
        }
        for (unsigned int point = 0u; point < count; point += 1u)
        {
            mine[POINTS_CHECK_OUT_OF_ORDER] +=
                ((point != 0u) && (capacity->voxels[point] <= capacity->voxels[point - 1u])) ? 1ull : 0ull;
            mine[POINTS_CHECK_OUTSIDE] += ((unsigned long long)capacity->voxels[point] >= view) ? 1ull : 0ull;
            const unsigned int *const level = &capacity->levels[(size_t)point * limbs];
            int nonzero = 0;
            for (unsigned int limb = 0u; limb < limbs; limb += 1u)
            {
                nonzero = nonzero || (level[limb] != 0u);
            }
            const int positive = (limbs != 0u) && nonzero && ((level[limbs - 1u] & POINTS_CHECK_TOP_BIT) == 0u);
            mine[POINTS_CHECK_NOT_POSITIVE] += positive ? 0ull : 1ull;
        }
        points += count;
        least = (count < least) ? count : least;
        max = (count > max) ? count : max;
    }
    const int ended = ok && points_check_ended(file);
    mine[POINTS_CHECK_PAST_LAST_FRAME] += (ok && (ended == 0)) ? 1ull : 0ull;
    fclose(file);
    // a sample's readings are every voxel of every frame, read once; a sample not in scan.set has no readings to
    // contain
    const unsigned long long total = (listed != set->count) ? set->totals[listed] : 0ull;
    const int counted =
        (listed != set->count) && (head[0] != 0u) && (view <= (~0ull / head[0])) && (total == (view * head[0]));
    mine[POINTS_CHECK_READINGS_NOT_VIEW] += ((listed != set->count) && (counted == 0)) ? 1ull : 0ull;
    unsigned long long fault_count = 0ull;
    for (unsigned int fault = 0u; fault < POINTS_CHECK_FAULTS; fault += 1u)
    {
        faults[fault] += mine[fault];
        fault_count += mine[fault];
    }
    *points_seen += points;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, sample, 16u);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal_columns(output_line, frame, 4u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal_columns(output_line, head[0], 4u);
        mmgr_line_put(output_line, " frames of ");
        mmgr_line_decimal(output_line, head[1], 1u);
        mmgr_line_put(output_line, " x ");
        mmgr_line_decimal(output_line, head[2], 1u);
        mmgr_line_put(output_line, " x ");
        mmgr_line_decimal(output_line, head[3], 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal_columns(output_line, points, 9u);
        mmgr_line_put(output_line, " points, ");
        mmgr_line_decimal(output_line, (points != 0ull) ? least : 0u, 1u);
        mmgr_line_put(output_line, " to ");
        mmgr_line_decimal(output_line, max, 1u);
        mmgr_line_put(output_line, " a frame; limbs ");
        mmgr_line_decimal(output_line, limbs, 1u);
        mmgr_line_put(output_line, ", bits ");
        mmgr_line_decimal(output_line, head[5], 1u);
        mmgr_line_put(output_line, "; not positive ");
        mmgr_line_decimal(output_line, mine[POINTS_CHECK_NOT_POSITIVE], 1u);
        mmgr_line_put(output_line, ", out of order ");
        mmgr_line_decimal(output_line, mine[POINTS_CHECK_OUT_OF_ORDER], 1u);
        mmgr_line_put(output_line, ", outside ");
        mmgr_line_decimal(output_line, mine[POINTS_CHECK_OUTSIDE], 1u);
        mmgr_line_put(output_line, "; C ");
        mmgr_line_put(output_line,
                      (contrast_differs == POINTS_CHECK_READINGS) ? "is the rebuilt" : "differs from the rebuilt");
        mmgr_line_put(output_line, "; readings ");
        mmgr_line_decimal(output_line, total, 1u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, counted ? "=" : "!=");
        mmgr_line_put(output_line, " frames x view; ");
        mmgr_line_decimal(output_line, fault_count, 1u);
        mmgr_line_put(output_line, " faults");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (contrast_differs != POINTS_CHECK_READINGS)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, sample, 16u);
        mmgr_line_put(output_line, " C first differs at value ");
        mmgr_line_decimal(output_line, contrast_differs, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return fault_count == 0ull;
}

int program_main(int count, char **arguments)
{
    const uint32_t probe = 1u;
    if (*(const unsigned char *)&probe != 1u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  this host is not little-endian, and the files are");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    if (count < 3)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  usage: points_check <set> <sample> [<sample> ...]");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    const char *const root = arguments[1];
    unsigned long long faults[POINTS_CHECK_FAULTS];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = faults, .bytes = sizeof(faults), .val = 0);
    PointsCheckSet set;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &set, .bytes = sizeof(set), .val = 0);
    if (points_check_set_read(root, &set) == 0)
    {
        faults[POINTS_CHECK_SET_UNREAD] += 1ull;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  faults: ");
            mmgr_line_put(output_line, POINTS_CHECK_FAULT_NAMES[POINTS_CHECK_SET_UNREAD]);
            mmgr_line_put(output_line, " 1");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        points_check_set_free(&set);
        return 1;
    }
    (void)points_check_rebuild(root, &set, faults);
    PointsCheckFirst first;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &first, .bytes = sizeof(first), .val = 0);
    first.contrast = (unsigned long long *)platform_cell_taken(POINTS_CHECK_READINGS * sizeof(unsigned long long),
                                                               platform_place_g_program_owner, 0);
    PointsCheckWorkspace workspace;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &workspace, .bytes = sizeof(workspace), .val = 0);
    if (first.contrast == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  the first head's memory could not be reserved");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        points_check_set_free(&set);
        return 1;
    }
    unsigned int clean = 0u;
    unsigned long long points = 0ull;
    for (int at = 2; at < count; at += 1)
    {
        clean += points_check_sample(root, arguments[at], &set, &first, &workspace, faults, &points) ? 1u : 0u;
    }
    unsigned long long total = 0ull;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  checked ");
        mmgr_line_decimal_signed(output_line, count - 2);
        mmgr_line_put(output_line, " samples, ");
        mmgr_line_decimal(output_line, clean, 1u);
        mmgr_line_put(output_line, " with no fault, ");
        mmgr_line_decimal(output_line, points, 1u);
        mmgr_line_put(output_line, " points; faults:");
        platform_line_out_ended();
    }
    for (unsigned int fault = 0u; fault < POINTS_CHECK_FAULTS; fault += 1u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, (fault == 0u) ? "" : ",");
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, POINTS_CHECK_FAULT_NAMES[fault]);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, faults[fault], 1u);
            platform_line_out_ended();
        }
        total += faults[fault];
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n  ");
        mmgr_line_decimal(output_line, total, 1u);
        mmgr_line_put(output_line, " faults");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(first.contrast, platform_place_g_program_owner);
    platform_cell_released(workspace.voxels, platform_place_g_program_owner);
    platform_cell_released(workspace.levels, platform_place_g_program_owner);
    points_check_set_free(&set);
    return (total == 0ull) ? 0 : 1;
}
