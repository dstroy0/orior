// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The period on the record machine against the period's kernels and against the host.
// - period_record_read, given a top, writes the agreement and the measurement period_read writes, word for word, on
//   lattices of three axes with a period planted along one, of odd and even extents, of an axis of extent 1, and of a
//   line long enough that the sort's radix route orders it;
// - each null draw of period_record_read and period_record_draw is the host's: each line keyed by period's line hash
//   of the content and the draw, the keys sorted with equal keys in step order, the line gathered, its agreement
//   counted and its strongest peak read, height for height;
// - a lattice of more than 2^31 voxels is refused before a lane is read.
// On a lattice of 30 x 640 x 640 it prints the microseconds of the reading and of one draw on each route. The test is
// one job on the device's tessera daemon.
#include "../../../../../../../src/cu/engine/analysis/period/period_internal.h"
#include "../../../../../../../src/cu/engine/analysis/period/period.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORD_TEST_SHAPES 6u

#define RECORD_TEST_DRAWS 3ull

#define RECORD_TEST_KEY 0x5045524Full

// the timed lattice
#define RECORD_TEST_TIMED_Z 30ull

#define RECORD_TEST_TIMED_YX 640ull

// the most the test puts on the device at once: the timed lattice's lanes, both routes' pools, and the record
// programs' compiled modules beside them
#define RECORD_TEST_DECLARED(voxels_, entries_)                                                                        \
    (((voxels_) * sizeof(unsigned short)) + period_reserve_bytes((voxels_), (entries_)) +                             \
     period_record_reserve_bytes(voxels_) + (192ull << 20u))

typedef struct
{
    unsigned long long extent[3];
    unsigned int planted_axis;
    unsigned long long planted_period;
} RecordShape;

static unsigned int ref_round(unsigned int half, unsigned int key)
{
    unsigned int mixed = (half ^ key) * 0x9E3779B1u;
    mixed ^= mixed >> 15u;
    mixed *= 0x85EBCA77u;
    mixed ^= mixed >> 13u;
    return mixed;
}

static unsigned int ref_line_random(const PeriodShuffle *shuffle, unsigned long long line, unsigned int step)
{
    // the line's two 32-bit halves, each mixed in alone
    unsigned int hash = ref_round((unsigned int)line ^ shuffle->keys[0], shuffle->keys[1]);
    hash = ref_round(hash ^ (unsigned int)(line >> 32u), shuffle->keys[2]);
    return ref_round(hash ^ step, shuffle->keys[3]);
}

// the agreement along an axis of `data` laid with `stride`, as the kernel counts it: each usable place and the place a
// lag on
static void ref_agreement(const unsigned short *data, unsigned long long voxels, unsigned long long stride,
                          unsigned long long extent, unsigned long long *same)
{
    const unsigned long long lags = extent / 2ull;
    const unsigned long long usable = extent - lags;
    const unsigned long long outers = voxels / (extent * stride);
    for (unsigned long long lag = 1ull; lag <= lags; lag += 1ull)
    {
        unsigned long long count = 0ull;
        for (unsigned long long outer = 0ull; outer < outers; outer += 1ull)
        {
            for (unsigned long long along = 0ull; along < usable; along += 1ull)
            {
                for (unsigned long long inner = 0ull; inner < stride; inner += 1ull)
                {
                    const unsigned long long voxel = (((outer * extent) + along) * stride) + inner;
                    count += (data[voxel] == data[voxel + (lag * stride)]) ? 1ull : 0ull;
                }
            }
        }
        same[lag - 1ull] = count;
    }
}

typedef struct
{
    unsigned int key;
    unsigned int step;
} RefKeyed;

static int ref_keyed_order(const void *one, const void *other)
{
    const RefKeyed *const left = (const RefKeyed *)one;
    const RefKeyed *const right = (const RefKeyed *)other;
    if (left->key != right->key)
    {
        return (left->key > right->key) ? 1 : -1;
    }
    return (left->step > right->step) ? 1 : ((left->step < right->step) ? -1 : 0);
}

// the host's null draw on one axis: every line keyed, sorted by key and then by step, gathered line-major, and the
// strongest peak of its agreement
static int ref_null(const unsigned short *lanes, const PeriodLattice *lattice, const EngineSignum *content,
                    unsigned long long counter, unsigned int axis, PeriodMargin *height)
{
    const unsigned long long voxels = lattice->voxels;
    const unsigned long long extent = lattice->extent[axis];
    const unsigned long long stride = lattice->stride[axis];
    const unsigned long long lines = voxels / extent;
    unsigned short *const shuffled = (unsigned short *)malloc((size_t)voxels * sizeof(unsigned short));
    RefKeyed *const keyed = (RefKeyed *)malloc((size_t)extent * sizeof(RefKeyed));
    unsigned long long *const same = (unsigned long long *)calloc((size_t)(extent / 2ull) + 1u, sizeof(unsigned long long));
    if ((shuffled == NULL) || (keyed == NULL) || (same == NULL))
    {
        free(shuffled);
        free(keyed);
        free(same);
        return 0;
    }
    PeriodShuffle shuffle;
    period_shuffle_fill(&shuffle, content, counter);
    for (unsigned long long line = 0ull; line < lines; line += 1ull)
    {
        const unsigned long long outer = line / stride;
        const unsigned long long inner = line % stride;
        for (unsigned long long step = 0ull; step < extent; step += 1ull)
        {
            // a step is below the extent, a 32-bit count
            keyed[step].key = ref_line_random(&shuffle, line, (unsigned int)step);
            keyed[step].step = (unsigned int)step;
        }
        qsort(keyed, (size_t)extent, sizeof(RefKeyed), ref_keyed_order);
        for (unsigned long long at = 0ull; at < extent; at += 1ull)
        {
            shuffled[(line * extent) + at] = lanes[(((outer * extent) + keyed[at].step) * stride) + inner];
        }
    }
    ref_agreement(shuffled, voxels, 1ull, extent, same);
    PeriodAxis drawn;
    period_axis_open(lattice, axis, &drawn);
    period_strongest(same, &drawn);
    height->numerator = (drawn.candidate != 0ull) ? drawn.margin.numerator : 0ull;
    height->denominator = drawn.pairs_per_lag;
    free(shuffled);
    free(keyed);
    free(same);
    return 1;
}

// a lattice with a period planted along one axis, each voxel the place along that axis modulo the period, or with
// none planted, each voxel one of three values drawn from its place
static void record_fill(const RecordShape *shape, unsigned short *lanes)
{
    const unsigned long long voxels = shape->extent[0] * shape->extent[1] * shape->extent[2];
    for (unsigned long long voxel = 0ull; voxel < voxels; voxel += 1ull)
    {
        const unsigned long long place[3] = {voxel / (shape->extent[1] * shape->extent[2]),
                                             (voxel / shape->extent[2]) % shape->extent[1], voxel % shape->extent[2]};
        const unsigned long long noise = (shape->planted_period == 0ull) ? (sim_draw(RECORD_TEST_KEY, voxel) % 3ull)
                                                                         : 0ull;
        const unsigned long long phase =
            (shape->planted_period != 0ull) ? (place[shape->planted_axis] % shape->planted_period) : 0ull;
        // a phase and a noise below 256 make a 16-bit lane
        lanes[voxel] = (unsigned short)((phase << 8u) | noise);
    }
}

static void record_request(PeriodRequest *request, const unsigned short *device_lanes, const RecordShape *shape,
                           const EngineSignum *content, EngineError *error)
{
    memset(request, 0, sizeof(*request));
    request->device_lanes = device_lanes;
    request->rank = 3u;
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        request->extent[axis] = shape->extent[axis];
    }
    request->content = *content;
    request->error = error;
}

static unsigned long long record_now(void)
{
    // a steady clock's count is never negative
    return (unsigned long long)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// one shape on both routes: the given-top reading word for word, then the draws against the host
static void record_shape(SimResults *results, const RecordShape *shape)
{
    const unsigned long long voxels = shape->extent[0] * shape->extent[1] * shape->extent[2];
    unsigned short *const lanes = (unsigned short *)malloc((size_t)voxels * sizeof(unsigned short));
    unsigned short *device_lanes = NULL;
    int ok = (lanes != NULL) && sim_status_check(results,
                                                  cudaMalloc((void **)&device_lanes, voxels * sizeof(unsigned short)),
                                                  "the shape's lanes are held on the device");
    if (ok != 0)
    {
        record_fill(shape, lanes);
        ok = sim_status_check(results,
                              cudaMemcpy(device_lanes, lanes, voxels * sizeof(unsigned short), cudaMemcpyHostToDevice),
                              "the shape's lanes are copied to the device");
    }
    const EngineSignum content = sim_content(lanes, voxels, RECORD_TEST_KEY);
    const unsigned long long entries = period_agreement_entries(3u, shape->extent);
    unsigned long long *const kernel_agreement = (unsigned long long *)calloc((size_t)entries + 1u, 8u);
    unsigned long long *const record_agreement = (unsigned long long *)calloc((size_t)entries + 1u, 8u);
    PeriodMargin record_band[RECORD_TEST_DRAWS * 3ull];
    ok = ok && (kernel_agreement != NULL) && (record_agreement != NULL);
    char what[256];
    const PeriodMargin top[3] = {{0ull, 1ull}, {0ull, 1ull}, {0ull, 1ull}};
    if (ok != 0)
    {
        EngineError kernel_error;
        EngineError record_error;
        memset(&kernel_error, 0, sizeof(kernel_error));
        memset(&record_error, 0, sizeof(record_error));
        PeriodMeasurement kernel_measurement;
        PeriodMeasurement record_measurement;
        PeriodRequest request;
        record_request(&request, device_lanes, shape, &content, &kernel_error);
        request.null_top = top;
        request.agreement = kernel_agreement;
        request.agreement_capacity = entries;
        request.measurement = &kernel_measurement;
        const int kernel_read = period_read(&request) == 0L;
        record_request(&request, device_lanes, shape, &content, &record_error);
        request.null_top = top;
        request.agreement = record_agreement;
        request.agreement_capacity = entries;
        request.measurement = &record_measurement;
        const int record_read = period_record_read(&request) == 0L;
        snprintf(what, sizeof(what), "%llu x %llu x %llu: both routes read the lattice with a top given",
                 shape->extent[0], shape->extent[1], shape->extent[2]);
        sim_check(results, kernel_read && record_read, what);
        snprintf(what, sizeof(what), "%llu x %llu x %llu: the record machine's agreement is the kernels' word for word",
                 shape->extent[0], shape->extent[1], shape->extent[2]);
        sim_check(results,
                  kernel_read && record_read &&
                      (memcmp(kernel_agreement, record_agreement, (size_t)entries * sizeof(unsigned long long)) == 0),
                  what);
        snprintf(what, sizeof(what), "%llu x %llu x %llu: the record machine's measurement is the kernels'",
                 shape->extent[0], shape->extent[1], shape->extent[2]);
        sim_check(results,
                  kernel_read && record_read &&
                      (memcmp(&kernel_measurement, &record_measurement, sizeof(PeriodMeasurement)) == 0),
                  what);
        if ((kernel_read != 0) && (record_read != 0) && (shape->planted_period != 0ull))
        {
            snprintf(what, sizeof(what), "%llu x %llu x %llu: the planted period %llu is read along axis %u",
                     shape->extent[0], shape->extent[1], shape->extent[2], shape->planted_period,
                     shape->planted_axis);
            sim_check(results, record_measurement.axis[shape->planted_axis].period == shape->planted_period, what);
        }
    }
    // the draws: the reading's band and period_record_draw's heights against the host's
    if (ok != 0)
    {
        EngineError error;
        memset(&error, 0, sizeof(error));
        PeriodMeasurement measurement;
        PeriodRequest request;
        record_request(&request, device_lanes, shape, &content, &error);
        request.draws = RECORD_TEST_DRAWS;
        request.band = record_band;
        request.band_capacity = RECORD_TEST_DRAWS * 3ull;
        request.measurement = &measurement;
        const int read = period_record_read(&request) == 0L;
        snprintf(what, sizeof(what), "%llu x %llu x %llu: the record machine reads with %llu draws of its own",
                 shape->extent[0], shape->extent[1], shape->extent[2], RECORD_TEST_DRAWS);
        sim_check(results, read, what);
        PeriodLattice lattice;
        unsigned long long counted = 0ull;
        const int filled = period_lattice_fill(&request, &lattice, &counted);
        int drawn_same = read && filled;
        // each axis's heights that are not zero, in the order the draws make them
        PeriodMargin drawn_band[RECORD_TEST_DRAWS * 3ull];
        unsigned long long drawn_counts[3] = {0ull, 0ull, 0ull};
        for (unsigned long long draw = 0ull; (drawn_same != 0) && (draw < RECORD_TEST_DRAWS); draw += 1ull)
        {
            PeriodMargin heights[3];
            record_request(&request, device_lanes, shape, &content, &error);
            drawn_same = period_record_draw(&request, draw, heights) == 0L;
            for (unsigned int axis = 0u; (drawn_same != 0) && (axis < 3u); axis += 1u)
            {
                PeriodMargin host;
                drawn_same = ref_null(lanes, &lattice, &content, (draw * 3ull) + axis, axis, &host) &&
                             (host.numerator == heights[axis].numerator) &&
                             (host.denominator == heights[axis].denominator);
                if ((drawn_same != 0) && (heights[axis].numerator != 0ull))
                {
                    drawn_band[(axis * RECORD_TEST_DRAWS) + drawn_counts[axis]] = heights[axis];
                    drawn_counts[axis] += 1ull;
                }
            }
        }
        // the reading's band holds, axis by axis, the draws' heights that are not zero, sorted by period_select
        int band_same = drawn_same;
        for (unsigned int axis = 0u; (band_same != 0) && (axis < 3u); axis += 1u)
        {
            PeriodMargin *const drawn_axis = &drawn_band[axis * RECORD_TEST_DRAWS];
            qsort(drawn_axis, (size_t)drawn_counts[axis], sizeof(PeriodMargin), period_margin_compare);
            band_same = (measurement.axis[axis].band_count == drawn_counts[axis]) &&
                        (memcmp(drawn_axis, &record_band[axis * RECORD_TEST_DRAWS],
                                (size_t)drawn_counts[axis] * sizeof(PeriodMargin)) == 0);
        }
        snprintf(what, sizeof(what), "%llu x %llu x %llu: every draw's heights are the host's line by line null",
                 shape->extent[0], shape->extent[1], shape->extent[2]);
        sim_check(results, drawn_same, what);
        snprintf(what, sizeof(what), "%llu x %llu x %llu: the reading's band holds the draws' heights",
                 shape->extent[0], shape->extent[1], shape->extent[2]);
        sim_check(results, band_same, what);
    }
    cudaFree(device_lanes);
    free(lanes);
    free(kernel_agreement);
    free(record_agreement);
}

// the reading and one draw on each route over a lattice of 30 x 640 x 640, in microseconds
static void record_timed(SimResults *results)
{
    const RecordShape shape = {{RECORD_TEST_TIMED_Z, RECORD_TEST_TIMED_YX, RECORD_TEST_TIMED_YX}, 2u, 4ull};
    const unsigned long long voxels = shape.extent[0] * shape.extent[1] * shape.extent[2];
    unsigned short *const lanes = (unsigned short *)malloc((size_t)voxels * sizeof(unsigned short));
    unsigned short *device_lanes = NULL;
    int ok = (lanes != NULL) && sim_status_check(results,
                                                  cudaMalloc((void **)&device_lanes, voxels * sizeof(unsigned short)),
                                                  "the timed lattice is held on the device");
    if (ok != 0)
    {
        record_fill(&shape, lanes);
        ok = sim_status_check(results,
                              cudaMemcpy(device_lanes, lanes, voxels * sizeof(unsigned short), cudaMemcpyHostToDevice),
                              "the timed lattice is copied to the device");
    }
    const EngineSignum content = sim_content(lanes, voxels, RECORD_TEST_KEY);
    const PeriodMargin top[3] = {{0ull, 1ull}, {0ull, 1ull}, {0ull, 1ull}};
    unsigned long long spent[4] = {0ull, 0ull, 0ull, 0ull};
    int done[4] = {0, 0, 0, 0};
    for (unsigned int route = 0u; (ok != 0) && (route < 4u); route += 1u)
    {
        EngineError error;
        memset(&error, 0, sizeof(error));
        PeriodMeasurement measurement;
        PeriodMargin heights[3];
        PeriodRequest request;
        record_request(&request, device_lanes, &shape, &content, &error);
        request.measurement = &measurement;
        if (route < 2u)
        {
            request.null_top = top;
        }
        const unsigned long long began = record_now();
        done[route] = (route == 0u)   ? (period_read(&request) == 0L)
                      : (route == 1u) ? (period_record_read(&request) == 0L)
                      : (route == 2u) ? (period_draw(&request, 0ull, heights) == 0L)
                                      : (period_record_draw(&request, 0ull, heights) == 0L);
        spent[route] = record_now() - began;
    }
    sim_check(results, done[0] && done[1] && done[2] && done[3], "30 x 640 x 640: every route runs");
    ScripturaLine *const line = &results->line;
    const char *const names[4] = {"period_read", "period_record_read", "period_draw", "period_record_draw"};
    for (unsigned int route = 0u; route < 4u; route += 1u)
    {
        scriptura_text(line, "  30 x 640 x 640: ");
        scriptura_text(line, names[route]);
        scriptura_text(line, " ");
        scriptura_decimal(line, spent[route], 1u);
        scriptura_text(line, " us\n");
    }
    cudaFree(device_lanes);
    free(lanes);
}

int main(int count, char **arguments)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    const unsigned long long timed = RECORD_TEST_TIMED_Z * RECORD_TEST_TIMED_YX * RECORD_TEST_TIMED_YX;
    const unsigned long long timed_extent[3] = {RECORD_TEST_TIMED_Z, RECORD_TEST_TIMED_YX, RECORD_TEST_TIMED_YX};
    const unsigned long long timed_entries = period_agreement_entries(3u, timed_extent);
    int ok = sim_job_submit(&results, "period_record_test", count, arguments,
                            RECORD_TEST_DECLARED(timed, timed_entries));
    const RecordShape shapes[RECORD_TEST_SHAPES] = {{{24ull, 64ull, 80ull}, 2u, 5ull},
                                                    {{7ull, 33ull, 129ull}, 1u, 3ull},
                                                    {{1ull, 50ull, 50ull}, 2u, 0ull},
                                                    {{12ull, 9ull, 31ull}, 0u, 2ull},
                                                    {{9ull, 40ull, 41ull}, 0u, 0ull},
                                                    {{1ull, 2ull, 20000ull}, 2u, 7ull}};
    for (unsigned int shape = 0u; (ok != 0) && (shape < RECORD_TEST_SHAPES); shape += 1u)
    {
        record_shape(&results, &shapes[shape]);
        sim_flush(&results);
    }
    // a lattice past 2^31 voxels is refused before its lanes are read
    if (ok != 0)
    {
        EngineError error;
        memset(&error, 0, sizeof(error));
        PeriodMeasurement measurement;
        PeriodRequest request;
        memset(&request, 0, sizeof(request));
        request.device_lanes = (const unsigned short *)&error;
        request.rank = 2u;
        request.extent[0] = 2ull;
        request.extent[1] = (1ull << 30u) + 1ull;
        request.measurement = &measurement;
        request.error = &error;
        sim_check(&results, period_record_read(&request) == PERIOD_ERROR,
                  "a lattice of more than 2^31 voxels is refused");
    }
    if (ok != 0)
    {
        record_timed(&results);
    }
    period_record_release();
    return sim_close(&results, "period record test");
}
