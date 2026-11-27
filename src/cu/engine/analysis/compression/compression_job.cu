// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compression_job.cu: the encode and the decode of compression as stages of a job of the device (compression_job.h),
// and each as a job of its own (compression_encode_job, compression_decode_job). The encode scans the bits of the
// chunks across the grid. It then takes the stream on the device at the size that the scan gives. The host crosses the
// answer out, and then the offsets and the stream at the size in the answer.
#include "compression_job.h"

#include "../../../../../includes/platform/platform_device.h"
#include "compression_internal.h"
#include "memoria_operor/memoria_operor.h"

// What the encode answers. It holds the bits of the stream, and the cells of the offsets and of the stream that it took
// on the device. Its flag is 1 where the device refuses a cell of the job
struct CompressionEncodeAnswer
{
    unsigned long long bits;
    unsigned long long *offsets;
    unsigned int *stream;
    unsigned int refused;
};

// what the encode reads, which crosses in as one cell: the values, their count and chunks, and the cell of its answer
struct CompressionEncodeJob
{
    const int *coefficients;
    unsigned long long count;
    unsigned long long chunks;
    CompressionEncodeAnswer *answer;
};

// The scratch of the encode, which its first thread takes on the device and gives back at the end. It holds the natural
// numbers of the values, the bits of each chunk, and the total of each run of the scan
static __device__ unsigned int *s_compression_naturals;
static __device__ unsigned long long *s_compression_bits;
static __device__ unsigned long long *s_compression_totals;

// The first thread gives back the scratch of the encode, the newest cell first
static __device__ void compression_job_scratch_given(void)
{
    platform_device_cell_released(s_compression_totals, platform_place_g_engine_owner);
    platform_device_cell_released(s_compression_bits, platform_place_g_engine_owner);
    platform_device_cell_released(s_compression_naturals, platform_place_g_engine_owner);
    s_compression_totals = NULL;
    s_compression_bits = NULL;
    s_compression_naturals = NULL;
}

// The encode. The first thread takes the cell of the offsets and the scratch. The grid zigzags the values and measures
// each chunk. Each thread then scans one run of the chunks, and the first thread scans the totals of the runs. It takes
// the stream at the bits of the scan. Each run adds its base, and the grid writes the stream.
//
// Each step reads what the step before it wrote, after a sync of the grid
__device__ int compression_encode_staged(const void *job_record, const cooperative_groups::grid_group &grid)
{
    // Explicit cast reads the record of the stage as the encode that crossed into it
    const CompressionEncodeJob *const job = (const CompressionEncodeJob *)job_record;
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    const unsigned long long chunks = job->chunks;
    CompressionEncodeAnswer *const answer = job->answer;
    // each run holds `per` chunks, and the runs before `runs` hold at least one
    const unsigned long long per = (chunks + jump - 1ULL) / jump;
    const unsigned long long runs = (chunks + per - 1ULL) / per;
    if (first == 0ULL)
    {
        // Explicit casts take the addresses of the cells as their words
        answer->offsets = (unsigned long long *)platform_device_cell_taken(chunks * sizeof(unsigned long long),
                                                                           platform_place_g_engine_owner, 1);
        s_compression_naturals = (unsigned int *)platform_device_cell_taken(job->count * sizeof(unsigned int),
                                                                            platform_place_g_engine_owner, 1);
        s_compression_bits = (unsigned long long *)platform_device_cell_taken(chunks * sizeof(unsigned long long),
                                                                              platform_place_g_engine_owner, 1);
        s_compression_totals = (unsigned long long *)platform_device_cell_taken(runs * sizeof(unsigned long long),
                                                                                platform_place_g_engine_owner, 1);
        answer->refused = ((answer->offsets == NULL) || (s_compression_naturals == NULL) ||
                           (s_compression_bits == NULL) || (s_compression_totals == NULL))
                              ? 1u
                              : 0u;
    }
    grid.sync();
    if (answer->refused != 0u)
    {
        if (first == 0ULL)
        {
            compression_job_scratch_given();
        }
        return 0;
    }
    compression_zigzag_span(job->coefficients, job->count, s_compression_naturals, first, jump);
    grid.sync();
    compression_measure_span(s_compression_naturals, job->count, chunks, s_compression_bits, first, jump);
    grid.sync();
    // the run of this thread: the chunks from `low` up to `high`, none past the last run
    const unsigned long long low = (first < runs) ? (first * per) : chunks;
    const unsigned long long high = ((low + per) < chunks) ? (low + per) : chunks;
    unsigned long long sum = 0ULL;
    for (unsigned long long chunk = low; chunk < high; chunk += 1ULL)
    {
        answer->offsets[chunk] = sum;
        sum += s_compression_bits[chunk];
    }
    if (first < runs)
    {
        s_compression_totals[first] = sum;
    }
    grid.sync();
    if (first == 0ULL)
    {
        unsigned long long base = 0ULL;
        for (unsigned long long run = 0ULL; run < runs; run += 1ULL)
        {
            const unsigned long long held = s_compression_totals[run];
            s_compression_totals[run] = base;
            base += held;
        }
        answer->bits = base;
        // the stream holds a word past its bits, which the window of a read at its end reaches
        const unsigned long long limbs = (base + 31ULL) / 32ULL;
        // Explicit cast takes the address of the cell as its words
        answer->stream = (unsigned int *)platform_device_cell_cleared((limbs + 1ULL) * sizeof(unsigned int),
                                                                      platform_place_g_engine_owner, 1);
        answer->refused = (answer->stream == NULL) ? 1u : 0u;
    }
    grid.sync();
    if (answer->refused != 0u)
    {
        if (first == 0ULL)
        {
            compression_job_scratch_given();
        }
        return 0;
    }
    for (unsigned long long chunk = low; chunk < high; chunk += 1ULL)
    {
        answer->offsets[chunk] += s_compression_totals[first];
    }
    grid.sync();
    compression_write_span(s_compression_naturals, job->count, chunks, answer->offsets, answer->stream, first, jump);
    grid.sync();
    if (first == 0ULL)
    {
        compression_job_scratch_given();
    }
    return 1;
}

__global__ void compression_encode_job_kernel(const void *job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    (void)compression_encode_staged(job, grid);
}

int compression_encode_stage_crossed(const CompressionEncodeJobRequest *request, int *device_coefficients,
                                     TesseraJobCells *cells, CompressionStage *stage)
{
    EngineError *const error = request->error;
    if (!COMPRESSION_CHECK(((request->coefficients != NULL) || (device_coefficients != NULL)) &&
                               (request->chunks != NULL) && (request->bits != NULL) && (request->offsets != NULL) &&
                               (request->stream != NULL),
                           request, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    CompressionEncodeJob job;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &job, .bytes = sizeof(job), .val = 0);
    job.count = request->count;
    job.chunks = compression_chunks(request->count);
    if (!COMPRESSION_CHECK(job.chunks != 0ULL, &request->count, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    void *cell = device_coefficients;
    int ok = (device_coefficients != NULL) ||
             tessera_job_cells_in(cells, request->coefficients, job.count * sizeof(int), &cell);
    // Explicit cast reads the cell as the values that crossed into it or that an earlier stage wrote
    job.coefficients = (const int *)cell;
    ok = ok && tessera_job_cells_room(cells, sizeof(CompressionEncodeAnswer), &cell);
    // Explicit cast reads the cell as the answer of the encode
    job.answer = (CompressionEncodeAnswer *)cell;
    void *job_cell = NULL;
    ok = ok && tessera_job_cells_in(cells, &job, sizeof(job), &job_cell);
    stage->job = job_cell;
    stage->answer = job.answer;
    // Explicit cast keeps the cell of the values, which the encode reads and does not write
    stage->coefficients = (int *)job.coefficients;
    stage->count = job.count;
    stage->chunks = job.chunks;
    return COMPRESSION_CHECK(ok != 0, request, error, ENGINE_ERROR_RESOURCE);
}

int compression_encode_stage_agreed(const CompressionEncodeJobRequest *request, const CompressionStage *stage,
                                    TesseraJobCells *cells)
{
    EngineError *const error = request->error;
    CompressionEncodeAnswer answer;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &answer, .bytes = sizeof(answer), .val = 0);
    answer.refused = 1u;
    int ok = COMPRESSION_CHECK(tessera_job_crossed_out(&answer, stage->answer, sizeof(answer)), request, error,
                               ENGINE_ERROR_RESOURCE);
    // the cells that the kernel took and left go back before the crossings, the stream first
    ok = ok && COMPRESSION_CHECK(tessera_job_cells_kept(cells, answer.offsets) &&
                                     tessera_job_cells_kept(cells, answer.stream),
                                 request, error, ENGINE_ERROR_RESOURCE);
    ok = ok && COMPRESSION_CHECK(answer.refused == 0u, request, error, ENGINE_ERROR_RESOURCE);
    const unsigned long long limbs = (answer.bits + 31ULL) / 32ULL;
    unsigned long long *host_offsets = NULL;
    unsigned int *host_stream = NULL;
    ok = ok && compression_host_room(stage->chunks, limbs, &host_offsets, &host_stream, error) &&
         COMPRESSION_CHECK(
             tessera_job_crossed_out(host_offsets, answer.offsets, stage->chunks * sizeof(unsigned long long)) &&
                 tessera_job_crossed_out(host_stream, answer.stream, limbs * sizeof(unsigned int)),
             request, error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        *request->chunks = stage->chunks;
        *request->bits = answer.bits;
        *request->offsets = host_offsets;
        *request->stream = host_stream;
    }
    return ok;
}

extern "C" long compression_encode_job(const CompressionEncodeJobRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return COMPRESSION_ERROR;
    }
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, platform_place_g_engine_owner);
    CompressionStage stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &stage, .bytes = sizeof(stage), .val = 0);
    int ok = compression_encode_stage_crossed(request, NULL, &cells, &stage);
    const void *job_cell = stage.job;
    // Explicit cast gives the address of the record of the stage as the one argument of the kernel
    void *args[1] = {(void *)&job_cell};
    ok = ok &&
         COMPRESSION_CHECK(tessera_job_ran((const void *)compression_encode_job_kernel, COMPRESSION_THREADS, args),
                           request, request->error, ENGINE_ERROR_RESOURCE) &&
         compression_encode_stage_agreed(request, &stage, &cells);
    tessera_job_cells_given(&cells);
    return (ok != 0) ? 0L : COMPRESSION_ERROR;
}

// what the decode reads, which crosses in as one cell: the stream, the offsets, the count and chunks, and the room of
// the values
struct CompressionDecodeJob
{
    const unsigned int *stream;
    const unsigned long long *offsets;
    unsigned long long count;
    unsigned long long chunks;
    int *coefficients;
};

// The decode: one step, each chunk read from its offset, then a sync of the grid
__device__ int compression_decode_staged(const void *job_record, const cooperative_groups::grid_group &grid)
{
    // Explicit cast reads the record of the stage as the decode that crossed into it
    const CompressionDecodeJob *const job = (const CompressionDecodeJob *)job_record;
    compression_read_span(job->stream, job->offsets, job->count, job->chunks, job->coefficients, grid.thread_rank(),
                          grid.size());
    grid.sync();
    return 1;
}

__global__ void compression_decode_job_kernel(const void *job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    (void)compression_decode_staged(job, grid);
}

int compression_decode_stage_crossed(const CompressionDecodeJobRequest *request, int *device_coefficients,
                                     TesseraJobCells *cells, CompressionStage *stage)
{
    EngineError *const error = request->error;
    if (!COMPRESSION_CHECK((request->offsets != NULL) && (request->stream != NULL), request, error,
                           ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    CompressionDecodeJob job;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &job, .bytes = sizeof(job), .val = 0);
    job.count = request->count;
    job.chunks = compression_chunks(request->count);
    const unsigned long long limbs = (request->bits + 31ULL) / 32ULL;
    if (!COMPRESSION_CHECK(job.chunks != 0ULL, &request->count, error, ENGINE_ERROR_REQUEST) ||
        !COMPRESSION_CHECK(request->chunks == job.chunks, &request->chunks, error, ENGINE_ERROR_LOGIC))
    {
        return 0;
    }
    for (unsigned long long chunk = 0ULL; chunk < job.chunks; chunk += 1ULL)
    {
        if (!COMPRESSION_CHECK((request->offsets[chunk] <= request->bits) &&
                                   ((chunk == 0ULL) || (request->offsets[chunk] >= request->offsets[chunk - 1ULL])),
                               &request->offsets[chunk], error, ENGINE_ERROR_LOGIC))
        {
            return 0;
        }
    }
    void *cell = NULL;
    // the stream holds a word past its bits, which the window of a read at its end reaches
    int ok = tessera_job_cells_room(cells, (limbs + 1ULL) * sizeof(unsigned int), &cell) &&
             tessera_job_crossed_into(cell, request->stream, limbs * sizeof(unsigned int));
    // Explicit cast reads the cell as the stream that crossed into it
    job.stream = (const unsigned int *)cell;
    ok = ok && tessera_job_cells_in(cells, request->offsets, job.chunks * sizeof(unsigned long long), &cell);
    // Explicit cast reads the cell as the offsets that crossed into it
    job.offsets = (const unsigned long long *)cell;
    cell = device_coefficients;
    ok = ok && ((device_coefficients != NULL) || tessera_job_cells_room(cells, job.count * sizeof(int), &cell));
    // Explicit cast reads the cell as the room of the values
    job.coefficients = (int *)cell;
    void *job_cell = NULL;
    ok = ok && tessera_job_cells_in(cells, &job, sizeof(job), &job_cell);
    stage->job = job_cell;
    stage->answer = NULL;
    stage->coefficients = job.coefficients;
    stage->count = job.count;
    stage->chunks = job.chunks;
    return COMPRESSION_CHECK(ok != 0, request, error, ENGINE_ERROR_RESOURCE);
}

int compression_decode_stage_agreed(const CompressionDecodeJobRequest *request, const CompressionStage *stage)
{
    return (request->coefficients == NULL) ||
           COMPRESSION_CHECK(
               tessera_job_crossed_out(request->coefficients, stage->coefficients, stage->count * sizeof(int)), request,
               request->error, ENGINE_ERROR_RESOURCE);
}

extern "C" long compression_decode_job(const CompressionDecodeJobRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return COMPRESSION_ERROR;
    }
    if (!COMPRESSION_CHECK(request->coefficients != NULL, request, request->error, ENGINE_ERROR_REQUEST))
    {
        return COMPRESSION_ERROR;
    }
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, platform_place_g_engine_owner);
    CompressionStage stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &stage, .bytes = sizeof(stage), .val = 0);
    int ok = compression_decode_stage_crossed(request, NULL, &cells, &stage);
    const void *job_cell = stage.job;
    // Explicit cast gives the address of the record of the stage as the one argument of the kernel
    void *args[1] = {(void *)&job_cell};
    ok = ok &&
         COMPRESSION_CHECK(tessera_job_ran((const void *)compression_decode_job_kernel, COMPRESSION_THREADS, args),
                           request, request->error, ENGINE_ERROR_RESOURCE) &&
         compression_decode_stage_agreed(request, &stage);
    tessera_job_cells_given(&cells);
    return (ok != 0) ? 0L : COMPRESSION_ERROR;
}
