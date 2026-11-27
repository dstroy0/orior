// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// body_overlap_paths_test.cu: the paths of the body overlap on the device, with the engine linked into the program.
// The test draws seeded pairs of frames, and each pair runs through body_overlap_run, through
// body_overlap_run_on_device from cells that cross into the device, and through body_overlap_host. The answers of each
// device path must equal the answers of the host. The signum of the answers of the device paths must equal the signum
// of the device path
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/nbody/body_overlap/body_overlap.h"
#include "../../../../../../../src/cu/engine/runtime/daemon/tessera_job.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../src/platform/platform.h"

#include "memoria_operor/memoria_operor.h"

// the most voxels of a case, the most pairs that a case writes, and the blobs of a frame of three axes
#define OVERLAP_TEST_VOXELS_MAX 40960u
#define OVERLAP_TEST_CAPACITY 4096u
#define OVERLAP_TEST_BLOBS 12u

// the cases of the test, and the most bytes of the answers of the device paths over every case
#define OVERLAP_TEST_CASES 5u
#define OVERLAP_TEST_ANSWER_BYTES (1u << 20u)

// the pool of the test, for its frames and its answers
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 28u;

// the seeded words that the frames draw from
static unsigned int s_seed = 0x3C6EF372u;

static unsigned int overlap_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// the count of the checks, and one check, printed as it passes or fails
static unsigned int s_checks = 0u;

static unsigned int overlap_test_check(int held, const char *what)
{
    s_checks += 1u;
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// One case: a pair of frames, their extent, the lag of the pair and the lags of some labels
typedef struct
{
    unsigned int axes;
    unsigned int extents[BODY_OVERLAP_AXES];
    int lag[BODY_OVERLAP_AXES];
    unsigned int voxels;
    unsigned int capacity;
    unsigned int labels_before[OVERLAP_TEST_VOXELS_MAX];
    unsigned int labels_after[OVERLAP_TEST_VOXELS_MAX];
    unsigned long long positive_before[OVERLAP_TEST_VOXELS_MAX / 64u];
    unsigned long long positive_after[OVERLAP_TEST_VOXELS_MAX / 64u];
    unsigned int lag_peaks[OVERLAP_TEST_BLOBS];
    int lag_steps[OVERLAP_TEST_BLOBS * BODY_OVERLAP_AXES];
    unsigned int lag_count;
} OverlapTestCase;

// The answers of one path of one case: its return, its count of runs and the pairs that it writes
typedef struct
{
    long returned;
    unsigned long long runs;
    unsigned int peaks_before[OVERLAP_TEST_CAPACITY];
    unsigned int peaks_after[OVERLAP_TEST_CAPACITY];
    unsigned int counts[OVERLAP_TEST_CAPACITY];
} OverlapTestAnswer;

// The labels and the positive bits of one frame of three axes, of blobs at `centers`. A voxel inside a blob holds
// the voxel of the center of the blob, plus 1, as its label, and else 0. About 1 voxel of 9 of a blob is not positive
static void overlap_test_frame(const OverlapTestCase *one, const int (*centers)[3], const int *radius,
                               unsigned int *labels, unsigned long long *positive)
{
    const unsigned int height = one->extents[1];
    const unsigned int width = one->extents[2];
    for (unsigned int voxel = 0u; voxel < one->voxels; voxel++)
    {
        // Explicit casts take each coordinate below the extent of the frame, which an int holds
        const int place[3] = {(int)(voxel / (height * width)), (int)((voxel / width) % height), (int)(voxel % width)};
        unsigned int nearest = OVERLAP_TEST_BLOBS;
        int nearest_length = 0x7FFFFFFF;
        for (unsigned int blob = 0u; blob < OVERLAP_TEST_BLOBS; blob++)
        {
            const int step[3] = {place[0] - centers[blob][0], place[1] - centers[blob][1], place[2] - centers[blob][2]};
            const int length = (step[0] * step[0]) + (step[1] * step[1]) + (step[2] * step[2]);
            const int inside = (length <= (radius[blob] * radius[blob])) && (length < nearest_length);
            nearest = (inside != 0) ? blob : nearest;
            nearest_length = (inside != 0) ? length : nearest_length;
        }
        // Explicit casts take each center, inside the frame and so not negative, as an unsigned int
        labels[voxel] =
            (nearest == OVERLAP_TEST_BLOBS)
                ? 0u
                : (((((unsigned int)centers[nearest][0] * height) + (unsigned int)centers[nearest][1]) * width) +
                   (unsigned int)centers[nearest][2] + 1u);
        const int lit = (nearest != OVERLAP_TEST_BLOBS) && ((overlap_test_drawn() % 9u) != 0u);
        positive[voxel / 64u] |= (lit != 0) ? (1ULL << (voxel % 64u)) : 0ULL;
    }
}

// A case of three axes: blobs that step `drift` from the earlier frame to the later, with a jitter of at most 1 an
// axis. Every third blob carries its own lag, its drift and its jitter, and the rest take the lag of the pair
static void overlap_test_blobs(unsigned int depth, unsigned int height, unsigned int width, const int *drift,
                               unsigned int capacity, OverlapTestCase *one)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = one, .bytes = sizeof(*one), .val = 0);
    one->axes = 3u;
    one->extents[0] = depth;
    one->extents[1] = height;
    one->extents[2] = width;
    one->voxels = depth * height * width;
    one->capacity = capacity;
    int before[OVERLAP_TEST_BLOBS][3];
    int after[OVERLAP_TEST_BLOBS][3];
    int radius[OVERLAP_TEST_BLOBS];
    for (unsigned int blob = 0u; blob < OVERLAP_TEST_BLOBS; blob++)
    {
        radius[blob] = 2 + (int)(overlap_test_drawn() % 3u);
        for (unsigned int axis = 0u; axis < 3u; axis++)
        {
            // Explicit casts take each extent and each draw below 2^31, which an int holds
            const int extent = (int)one->extents[axis];
            const int jitter = (int)(overlap_test_drawn() % 3u) - 1;
            before[blob][axis] = 3 + (int)(overlap_test_drawn() % (unsigned int)(extent - 6));
            const int moved = before[blob][axis] + drift[axis] + jitter;
            after[blob][axis] = (moved < 0) ? 0 : ((moved >= extent) ? (extent - 1) : moved);
            one->lag_steps[(one->lag_count * 3u) + axis] = after[blob][axis] - before[blob][axis];
        }
        one->lag_count += ((blob % 3u) == 0u) ? 1u : 0u;
    }
    for (unsigned int axis = 0u; axis < 3u; axis++)
    {
        one->lag[axis] = drift[axis];
    }
    overlap_test_frame(one, before, radius, one->labels_before, one->positive_before);
    overlap_test_frame(one, after, radius, one->labels_after, one->positive_after);
    // the lags of every third blob, in the order of their peaks, as the overlap searches them
    unsigned int at = 0u;
    for (unsigned int blob = 0u; blob < OVERLAP_TEST_BLOBS; blob += 3u)
    {
        // Explicit casts take each center, inside the frame and so not negative, as an unsigned int
        one->lag_peaks[at] = ((((unsigned int)before[blob][0] * height) + (unsigned int)before[blob][1]) * width) +
                             (unsigned int)before[blob][2] + 1u;
        at += 1u;
    }
    for (unsigned int one_at = 1u; one_at < one->lag_count; one_at++)
    {
        for (unsigned int back = one_at; (back > 0u) && (one->lag_peaks[back - 1u] > one->lag_peaks[back]); back--)
        {
            const unsigned int peak = one->lag_peaks[back];
            one->lag_peaks[back] = one->lag_peaks[back - 1u];
            one->lag_peaks[back - 1u] = peak;
            for (unsigned int axis = 0u; axis < 3u; axis++)
            {
                const int step = one->lag_steps[(back * 3u) + axis];
                one->lag_steps[(back * 3u) + axis] = one->lag_steps[((back - 1u) * 3u) + axis];
                one->lag_steps[((back - 1u) * 3u) + axis] = step;
            }
        }
    }
}

// A case of four axes, each label one of 7, each voxel positive in about 2 of 3. Labels 1, 3, 5 and 7 carry lags of
// their own. Their 16 steps pass the 12 steps of the four lags of a case of three axes before it
static void overlap_test_four(OverlapTestCase *one)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = one, .bytes = sizeof(*one), .val = 0);
    static const unsigned int EXTENTS[4] = {3u, 6u, 8u, 10u};
    static const int LAG[4] = {0, 1, -1, 2};
    one->axes = 4u;
    one->voxels = 1u;
    for (unsigned int axis = 0u; axis < 4u; axis++)
    {
        one->extents[axis] = EXTENTS[axis];
        one->lag[axis] = LAG[axis];
        one->voxels *= EXTENTS[axis];
    }
    one->capacity = OVERLAP_TEST_CAPACITY;
    for (unsigned int voxel = 0u; voxel < one->voxels; voxel++)
    {
        one->labels_before[voxel] = 1u + (overlap_test_drawn() % 7u);
        one->labels_after[voxel] = 1u + (overlap_test_drawn() % 7u);
        one->positive_before[voxel / 64u] |= ((overlap_test_drawn() % 3u) != 0u) ? (1ULL << (voxel % 64u)) : 0ULL;
        one->positive_after[voxel / 64u] |= ((overlap_test_drawn() % 3u) != 0u) ? (1ULL << (voxel % 64u)) : 0ULL;
    }
    one->lag_count = 4u;
    for (unsigned int at = 0u; at < one->lag_count; at++)
    {
        one->lag_peaks[at] = 1u + (2u * at);
        for (unsigned int axis = 0u; axis < 4u; axis++)
        {
            // Explicit cast takes a draw of 3 values as an int
            one->lag_steps[(at * 4u) + axis] = LAG[axis] + (int)(overlap_test_drawn() % 3u) - 1;
        }
    }
}

// the request of a case, its labels and positive bits at the addresses given, its answers in `answer`
static void overlap_test_request(const OverlapTestCase *one, const unsigned int *labels_before,
                                 const unsigned long long *positive_before, const unsigned int *labels_after,
                                 const unsigned long long *positive_after, OverlapTestAnswer *answer,
                                 BodyOverlapRequest *request)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = answer, .bytes = sizeof(*answer), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request, .bytes = sizeof(*request), .val = 0);
    request->labels_before = labels_before;
    request->positive_before = positive_before;
    request->labels_after = labels_after;
    request->positive_after = positive_after;
    request->axes = one->axes;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request->extents, .src = one->extents, .bytes = sizeof(request->extents));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request->lag, .src = one->lag, .bytes = sizeof(request->lag));
    request->voxels = one->voxels;
    request->capacity = one->capacity;
    request->peaks_before = answer->peaks_before;
    request->peaks_after = answer->peaks_after;
    request->counts = answer->counts;
    request->lag_peaks = one->lag_peaks;
    request->lag_steps = one->lag_steps;
    request->lag_count = one->lag_count;
    request->runs = &answer->runs;
}

// 1 where the answers of a device path equal the answers of the host. The host counts no runs
static int overlap_test_agree(const OverlapTestAnswer *device, const OverlapTestAnswer *host)
{
    const size_t written = ((host->returned > 0L) && ((unsigned long)host->returned <= OVERLAP_TEST_CAPACITY))
                               ? (size_t)host->returned
                               : 0u;
    return (device->returned == host->returned) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device->peaks_before, .other = host->peaks_before,
                       .bytes = written * sizeof(unsigned int)) == 0) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device->peaks_after, .other = host->peaks_after,
                       .bytes = written * sizeof(unsigned int)) == 0) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device->counts, .other = host->counts,
                       .bytes = written * sizeof(unsigned int)) == 0);
}

// the first 8 bytes of the signum of `count` bytes as a word, little end first, or 0 where the signum errors
static unsigned long long overlap_test_signum(const unsigned char *bytes, unsigned long long count)
{
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const ObsignatioSignumRequest request = {bytes, count, NULL, OBSIGNATIO_MODE_HASH, signum, OBSIGNATIO_SIGNUM_BYTES,
                                             &error};
    if ((count == 0ULL) || (obsignatio_signum(&request) != 0L))
    {
        return 0ULL;
    }
    unsigned long long word = 0ULL;
    for (unsigned int at = 0u; at < 8u; at++)
    {
        word |= (unsigned long long)signum[at] << (8u * at);
    }
    return word;
}

// The frames of a case crossed into temporary cells of the pool of the device, for body_overlap_run_on_device. The
// cells go in `cells`, the newest last. 1 where each crossing succeeds
static int overlap_test_crossed(const OverlapTestCase *one, void **cells)
{
    const unsigned long long label_bytes = (unsigned long long)one->voxels * sizeof(unsigned int);
    const unsigned long long word_bytes = ((one->voxels + 63ULL) / 64ULL) * sizeof(unsigned long long);
    const unsigned int place = platform_place_g_program_owner;
    return tessera_job_crossed_in(one->labels_before, label_bytes, place, &cells[0]) &&
           tessera_job_crossed_in(one->positive_before, word_bytes, place, &cells[1]) &&
           tessera_job_crossed_in(one->labels_after, label_bytes, place, &cells[2]) &&
           tessera_job_crossed_in(one->positive_after, word_bytes, place, &cells[3]);
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the first word of the signum of the answers of the device paths over every case
    static const unsigned long long SIGNUM = 0xa1453d9746ca062bULL;
    static const char *const NAMES[OVERLAP_TEST_CASES] = {"three axes, a small frame", "three axes, a larger frame",
                                                          "three axes, the capacity below the pairs", "four axes",
                                                          "three axes, the small frame again"};
    const unsigned int place = platform_place_g_program_owner;
    OverlapTestCase *const one = (OverlapTestCase *)platform_cell_taken(sizeof(OverlapTestCase), place, 1);
    OverlapTestAnswer *const answers =
        (OverlapTestAnswer *)platform_cell_taken(3u * sizeof(OverlapTestAnswer), place, 1);
    unsigned char *const signed_bytes = (unsigned char *)platform_cell_taken(OVERLAP_TEST_ANSWER_BYTES, place, 1);
    if ((one == NULL) || (answers == NULL) || (signed_bytes == NULL))
    {
        return 1;
    }
    unsigned long long signed_count = 0ULL;
    unsigned int failed = 0u;
    static const int DRIFT_SMALL[3] = {1, -2, 2};
    static const int DRIFT_LARGE[3] = {-1, 3, -2};
    for (unsigned int at = 0u; at < OVERLAP_TEST_CASES; at++)
    {
        if (at == 3u)
        {
            overlap_test_four(one);
        }
        else
        {
            const int larger = (at == 1u);
            overlap_test_blobs(larger ? 20u : 12u, larger ? 32u : 24u, larger ? 64u : 32u,
                               larger ? DRIFT_LARGE : DRIFT_SMALL, (at == 2u) ? 4u : OVERLAP_TEST_CAPACITY, one);
        }
        BodyOverlapRequest request;
        overlap_test_request(one, one->labels_before, one->positive_before, one->labels_after, one->positive_after,
                             &answers[0], &request);
        answers[0].returned = body_overlap_host(&request);
        overlap_test_request(one, one->labels_before, one->positive_before, one->labels_after, one->positive_after,
                             &answers[1], &request);
        answers[1].returned = body_overlap_run(&request);
        void *cells[4] = {NULL, NULL, NULL, NULL};
        const int crossed = overlap_test_crossed(one, cells);
        // Explicit casts read the cells as the labels and the positive words that they hold
        overlap_test_request(one, (const unsigned int *)cells[0], (const unsigned long long *)cells[1],
                             (const unsigned int *)cells[2], (const unsigned long long *)cells[3], &answers[2],
                             &request);
        answers[2].returned = (crossed != 0) ? body_overlap_run_on_device(&request) : BODY_OVERLAP_ERROR;
        for (unsigned int cell = 4u; cell > 0u; cell--)
        {
            tessera_job_given(cells[cell - 1u], place);
        }
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, NAMES[at]);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, (unsigned long long)((answers[0].returned > 0L) ? answers[0].returned : 0L), 1u);
        mmgr_line_put(output_line, " pairs, ");
        mmgr_line_decimal(output_line, answers[1].runs, 1u);
        mmgr_line_put(output_line, " runs\n");
        platform_line_out_ended();
        failed += overlap_test_check(answers[0].returned > 0L, "the host finds pairs");
        failed += overlap_test_check(overlap_test_agree(&answers[1], &answers[0]),
                                     "body_overlap_run gives the answers of the host");
        failed +=
            overlap_test_check(overlap_test_agree(&answers[2], &answers[0]) && (answers[2].runs == answers[1].runs),
                               "body_overlap_run_on_device gives the answers of the host");
        for (unsigned int path = 1u; path < 3u; path++)
        {
            if ((signed_count + sizeof(OverlapTestAnswer)) <= OVERLAP_TEST_ANSWER_BYTES)
            {
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &signed_bytes[signed_count], .src = &answers[path],
                           .bytes = sizeof(OverlapTestAnswer));
                signed_count += sizeof(OverlapTestAnswer);
            }
        }
    }
    body_overlap_release();
    const unsigned long long word = overlap_test_signum(signed_bytes, signed_count);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the device paths: signum 0x");
        mmgr_line_hex(output_line, word, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    failed += overlap_test_check((word != 0ULL) && (word == SIGNUM),
                                 "the answers of the device paths are the answer of the device path");
    platform_cell_released(signed_bytes, place);
    platform_cell_released(answers, place);
    platform_cell_released(one, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  body overlap paths test: ");
    mmgr_line_decimal(output_line, s_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
