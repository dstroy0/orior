// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// Calls period_read and period_draw (src/cu/engine/analysis/period/) on the requests a file contains and prints
// each measurement in full, every count and every ratio as its exact numerator and denominator. It is the engine's side
// of archive/utils/test/src/python/engine/analysis/period/period_test.py, which writes the file and reads these lines
// against measure/period.py.
//
//   period_probe <requests file>
//
// The file contains records one after another, every field little endian:
//   u32 magic 0x31445250, u32 mode (0 period_read, 1 period_draw), u32 rank, u32 has_null_top,
//   u64 extent[8], u64 draws (period_draw's draw number in mode 1), u8 content[32],
//   u64 null_top[8][2] (numerator, denominator), u64 lane count, u16 lanes[lane count].
// A lane count of 0 still passes the engine one device lane, since a request in error reads none.
//
// The probe is a job on the device's tessera daemon (sims/cu/sim_job.cu). It reads the file once to size its
// declaration, submits the job, and then runs the records.
//
// Each record prints "case <n> mode <read|draw> status <s>", the measurement's lines when the status is 0, and "end".

#include "../../../../../../../src/cu/engine/runtime/device_pool/device_pool.h"
#include "../../../../../../../includes/platform/platform.h"
#include "../../../../../../../src/cu/engine/analysis/period/period.h"
#include "sim.h"

#include <cuda_runtime.h>

#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_MAGIC 0x31445250u
#define PROBE_BAND_MAX (1ull << 24u)

typedef struct
{
    unsigned int magic;
    unsigned int mode;
    unsigned int rank;
    unsigned int has_null_top;
    unsigned long long extent[ENGINE_ARRAY_RANK];
    unsigned long long draws;
    unsigned char content[ENGINE_SIGNUM_BYTES];
    PeriodMargin null_top[ENGINE_ARRAY_RANK];
    unsigned long long lane_count;
} ProbeRecord;

static int probe_take(FILE *file, void *into, size_t bytes)
{
    return fread(into, 1u, bytes, file) == bytes;
}

// the record's fixed fields in the file's order; 0 at the end of the file
static int probe_record(FILE *file, ProbeRecord *record)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = record, .bytes = sizeof(*record), .val = 0);
    if (!probe_take(file, &record->magic, sizeof(record->magic)))
    {
        return 0;
    }
    int ok = (record->magic == PROBE_MAGIC) && probe_take(file, &record->mode, sizeof(record->mode)) &&
             probe_take(file, &record->rank, sizeof(record->rank)) &&
             probe_take(file, &record->has_null_top, sizeof(record->has_null_top));
    for (unsigned int axis = 0u; ok && (axis < ENGINE_ARRAY_RANK); axis += 1u)
    {
        ok = probe_take(file, &record->extent[axis], sizeof(record->extent[axis]));
    }
    ok = ok && probe_take(file, &record->draws, sizeof(record->draws)) &&
         probe_take(file, record->content, sizeof(record->content));
    for (unsigned int axis = 0u; ok && (axis < ENGINE_ARRAY_RANK); axis += 1u)
    {
        ok = probe_take(file, &record->null_top[axis].numerator, sizeof(unsigned long long)) &&
             probe_take(file, &record->null_top[axis].denominator, sizeof(unsigned long long));
    }
    ok = ok && probe_take(file, &record->lane_count, sizeof(record->lane_count));
    return ok ? 1 : -1;
}

static int probe_skip(FILE *file, unsigned long long bytes)
{
    unsigned char discard[4096];
    while (bytes != 0ull)
    {
        const size_t step = (bytes < sizeof(discard)) ? (size_t)bytes : sizeof(discard);
        if (!probe_take(file, discard, step))
        {
            return 0;
        }
        bytes -= step;
    }
    return 1;
}

// The bytes the job declares: the device lanes the largest record copies, rounded up to pages, beside
// period_reserve_bytes for the max voxels and the max agreement entries of the records it can contain (a job over
// several extents declares the pool for the max of each). 0 when a record is malformed.
static unsigned long long probe_declared(FILE *file)
{
    unsigned long long lane_bytes_max = sizeof(unsigned short);
    unsigned long long voxels_max = 0ull;
    unsigned long long entries_max = 0ull;
    for (;;)
    {
        ProbeRecord record;
        const int read = probe_record(file, &record);
        if (read == 0)
        {
            break;
        }
        const unsigned long long lane_bytes = record.lane_count * sizeof(unsigned short);
        if ((read < 0) || !probe_skip(file, lane_bytes))
        {
            return 0ull;
        }
        lane_bytes_max = (lane_bytes > lane_bytes_max) ? lane_bytes : lane_bytes_max;
        const unsigned int bounded_rank = (record.rank < ENGINE_ARRAY_RANK) ? record.rank : ENGINE_ARRAY_RANK;
        unsigned long long voxels = (bounded_rank != 0u) ? 1ull : 0ull;
        for (unsigned int axis = 0u; axis < bounded_rank; axis += 1u)
        {
            const unsigned long long extent = record.extent[axis];
            // saturates at 2^64 - 1, which period_reserve_bytes takes as more voxels than the calls cover
            voxels = ((extent != 0ull) && (voxels > (~0ull / extent))) ? ~0ull : (voxels * extent);
        }
        const unsigned long long entries = period_agreement_entries(bounded_rank, record.extent);
        if (period_reserve_bytes(voxels, entries) != 0ull)
        {
            voxels_max = (voxels > voxels_max) ? voxels : voxels_max;
            entries_max = (entries > entries_max) ? entries : entries_max;
        }
    }
    const unsigned long long lane_pages = (lane_bytes_max + DEVICE_POOL_PAGE_BYTES - 1ull) / DEVICE_POOL_PAGE_BYTES;
    return (lane_pages * DEVICE_POOL_PAGE_BYTES) + period_reserve_bytes(voxels_max, entries_max);
}

static void probe_margin(const char *name, const PeriodMargin *margin)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, " ");
    mmgr_line_put(output_line, name);
    mmgr_line_put(output_line, " ");
    mmgr_line_decimal(output_line, margin->numerator, 1u);
    mmgr_line_put(output_line, "/");
    mmgr_line_decimal(output_line, margin->denominator, 1u);
    platform_line_out_ended();
}

static void probe_measurement(const PeriodMeasurement *measurement, const unsigned long long *agreement,
                              unsigned long long entries, const PeriodMargin *band, unsigned long long band_entries)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "measurement rank ");
        mmgr_line_decimal(output_line, measurement->rank, 1u);
        mmgr_line_put(output_line, " voxels ");
        mmgr_line_decimal(output_line, measurement->voxels, 1u);
        mmgr_line_put(output_line, " collisions ");
        mmgr_line_decimal(output_line, measurement->collisions, 1u);
        mmgr_line_put(output_line, " draws ");
        mmgr_line_decimal(output_line, measurement->draws, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int axis = 0u; axis < measurement->rank; axis += 1u)
    {
        const PeriodAxis *const measured = &measurement->axis[axis];
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "axis ");
            mmgr_line_decimal(output_line, axis, 1u);
            mmgr_line_put(output_line, " extent ");
            mmgr_line_decimal(output_line, measured->extent, 1u);
            mmgr_line_put(output_line, " period ");
            mmgr_line_decimal(output_line, measured->period, 1u);
            mmgr_line_put(output_line, " candidate ");
            mmgr_line_decimal(output_line, measured->candidate, 1u);
            mmgr_line_put(output_line, " lags ");
            mmgr_line_decimal(output_line, measured->lags, 1u);
            mmgr_line_put(output_line, " pairs ");
            mmgr_line_decimal(output_line, measured->pairs_per_lag, 1u);
            mmgr_line_put(output_line, " at ");
            mmgr_line_decimal(output_line, measured->agreement_at_candidate, 1u);
            mmgr_line_put(output_line, " beside ");
            mmgr_line_decimal(output_line, measured->agreement_beside_candidate, 1u);
            mmgr_line_put(output_line, " doubled ");
            mmgr_line_decimal(output_line, measured->agreement_at_double, 1u);
            mmgr_line_put(output_line, " beside_double ");
            mmgr_line_decimal(output_line, measured->agreement_beside_double, 1u);
            platform_line_out_ended();
        }
        probe_margin("margin", &measured->margin);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " band_count ");
            mmgr_line_decimal(output_line, measured->band_count, 1u);
            platform_line_out_ended();
        }
        probe_margin("bottom", &measured->band_bottom);
        probe_margin("top", &measured->band_top);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "agreement ");
        mmgr_line_decimal(output_line, entries, 1u);
        platform_line_out_ended();
    }
    for (unsigned long long entry = 0ull; entry < entries; entry += 1ull)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, agreement[entry], 1u);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\nband ");
        mmgr_line_decimal(output_line, band_entries, 1u);
        platform_line_out_ended();
    }
    for (unsigned long long entry = 0ull; entry < band_entries; entry += 1ull)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, band[entry].numerator, 1u);
        mmgr_line_put(output_line, "/");
        mmgr_line_decimal(output_line, band[entry].denominator, 1u);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

static int probe_run(const ProbeRecord *record, const unsigned short *lanes, unsigned long long number)
{
    const unsigned long long device_lane_count = (record->lane_count != 0ull) ? record->lane_count : 1ull;
    unsigned short *device_lanes = NULL;
    if (cudaMalloc((void **)&device_lanes, (size_t)device_lane_count * sizeof(unsigned short)) != cudaSuccess)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "case ");
            mmgr_line_decimal(output_line, number, 1u);
            mmgr_line_put(output_line, " device lanes not allocated");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    int ok = cudaMemset(device_lanes, 0, (size_t)device_lane_count * sizeof(unsigned short)) == cudaSuccess;
    if (ok && (record->lane_count != 0ull))
    {
        ok = cudaMemcpy(device_lanes, lanes, (size_t)record->lane_count * sizeof(unsigned short),
                        cudaMemcpyHostToDevice) == cudaSuccess;
    }
    const unsigned int bounded_rank = (record->rank < ENGINE_ARRAY_RANK) ? record->rank : ENGINE_ARRAY_RANK;
    const unsigned long long entries = period_agreement_entries(bounded_rank, record->extent);
    const unsigned long long band_entries = record->draws * bounded_rank;
    unsigned long long *const agreement = (unsigned long long *)platform_cell_cleared(
        (size_t)(entries + 1ull), sizeof(unsigned long long), platform_place_g_program_owner, 0);
    // a band past the max this probe supports is passed as none, which only a request in error requests
    PeriodMargin *const band =
        (band_entries <= PROBE_BAND_MAX)
            ? (PeriodMargin *)platform_cell_cleared((size_t)(band_entries + 1ull), sizeof(PeriodMargin),
                                                    platform_place_g_program_owner, 0)
            : NULL;
    PeriodMargin heights[ENGINE_ARRAY_RANK];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = heights, .bytes = sizeof(heights), .val = 0);
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    PeriodMeasurement measurement;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &measurement, .bytes = sizeof(measurement), .val = 0);
    PeriodRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    request.device_lanes = device_lanes;
    request.rank = record->rank;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.extent, .src = record->extent, .bytes = sizeof(request.extent));
    request.draws = (record->mode == 0u) ? record->draws : 0ull;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.content.bytes, .src = record->content,
               .bytes = sizeof(request.content.bytes));
    request.null_top = (record->has_null_top != 0u) ? record->null_top : NULL;
    request.agreement = agreement;
    request.agreement_capacity = entries;
    request.band = band;
    request.band_capacity = (band != NULL) ? band_entries : 0ull;
    request.measurement = &measurement;
    request.error = &error;
    long status = -2L;
    if (ok && (agreement != NULL))
    {
        status = (record->mode == 0u) ? period_read(&request) : period_draw(&request, record->draws, heights);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "case ");
        mmgr_line_decimal(output_line, number, 1u);
        mmgr_line_put(output_line, " mode ");
        mmgr_line_put(output_line, (record->mode == 0u) ? "read" : "draw");
        mmgr_line_put(output_line, " status ");
        mmgr_line_decimal_signed(output_line, status);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if ((status == 0L) && (record->mode == 0u))
    {
        probe_measurement(&measurement, agreement, entries, band, (band != NULL) ? band_entries : 0ull);
    }
    else if (status == 0L)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "heights ");
            mmgr_line_decimal(output_line, bounded_rank, 1u);
            platform_line_out_ended();
        }
        for (unsigned int axis = 0u; axis < bounded_rank; axis += 1u)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, heights[axis].numerator, 1u);
            mmgr_line_put(output_line, "/");
            mmgr_line_decimal(output_line, heights[axis].denominator, 1u);
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "error type ");
        mmgr_line_decimal_signed(output_line, (int)error.type);
        mmgr_line_put(output_line, " module ");
        mmgr_line_decimal_signed(output_line, (int)error.module);
        mmgr_line_put(output_line, " site ");
        mmgr_line_decimal(output_line, error.site, 1u);
        mmgr_line_put(output_line, " status ");
        mmgr_line_decimal_signed(output_line, (int)error.status);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "end");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(agreement, platform_place_g_program_owner);
    platform_cell_released(band, platform_place_g_program_owner);
    cudaFree(device_lanes);
    return 1;
}

int program_main(int count, char **arguments)
{
    if (count != 2)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "usage: period_probe <requests file>");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    FILE *const file = fopen(arguments[1], "rb");
    if (file == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "period_probe: ");
            mmgr_line_put(output_line, arguments[1]);
            mmgr_line_put(output_line, " not opened");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    const unsigned long long declared = probe_declared(file);
    rewind(file);
    if (declared == 0ull)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "period_probe: a record in ");
            mmgr_line_put(output_line, arguments[1]);
            mmgr_line_put(output_line, " is malformed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        fclose(file);
        return 2;
    }
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    if (sim_job_submit(&results, "period_probe", count, arguments, declared) == 0)
    {
        fclose(file);
        return sim_close(&results, "period probe");
    }
    int ok = 1;
    unsigned long long number = 0ull;
    for (;;)
    {
        ProbeRecord record;
        const int read = probe_record(file, &record);
        if (read == 0)
        {
            break;
        }
        unsigned short *const lanes = (unsigned short *)platform_cell_taken(
            (size_t)(record.lane_count + 1ull) * sizeof(unsigned short), platform_place_g_program_owner, 0);
        if ((read < 0) || (lanes == NULL) ||
            !probe_take(file, lanes, (size_t)record.lane_count * sizeof(unsigned short)))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "period_probe: record ");
                mmgr_line_decimal(output_line, number, 1u);
                mmgr_line_put(output_line, " is malformed");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            platform_cell_released(lanes, platform_place_g_program_owner);
            ok = 0;
            break;
        }
        ok = probe_run(&record, lanes, number) && ok;
        platform_cell_released(lanes, platform_place_g_program_owner);
        number += 1ull;
        platform_line_flushed();
    }
    fclose(file);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "records ");
        mmgr_line_decimal(output_line, number, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int closed = sim_close(&results, "period probe");
    return (ok && (closed == 0)) ? 0 : 1;
}
