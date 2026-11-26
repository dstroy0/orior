// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// obsignatio_job.cu: the seals of the lanes and of a stream of bits as jobs of the device (obsignatio_lanes_job,
// obsignatio_bits_job). The host crosses the bytes in, and each job runs in one cooperative launch. The lanes job takes
// the chaining words of each level on the device, and the grid reduces them between syncs. The host crosses the flag
// and the signa out. A part with CUDA alone compiles this file.
#include "../../../../platform/platform_device.h"
#include "../daemon/tessera_job.h"
#include "memoria_operor/memoria_operor.h"
#include "obsignatio_internal.h"

#include <cooperative_groups.h>

// the threads of each block of a job
#define OBSIGNATIO_JOB_THREADS 256u

// a refused cell or crossing of a job, a resource the device lacks
#define OBSIGNATIO_JOB_CHECK(condition_, evacaddr_, error_)                                                            \
    engine_error_check((condition_), ENGINE_ERROR_RESOURCE, ENGINE_MODULE_OBSIGNATIO, (unsigned int)__LINE__,          \
                       (const void *)(evacaddr_), (error_))

// One level of the lanes, as obsignatio_many reads it: `messages` messages of `length` bytes, `stride` apart, sealed
// with `key` into `signa`
struct ObsignatioManyStage
{
    const unsigned char *bytes;
    unsigned long long messages;
    unsigned long long length;
    unsigned long long stride;
    ObsignatioKey key;
    unsigned char *signa;
};

// What the lanes job reads, which crosses in as one cell. It holds the four levels, and a flag of 1 where the device
// refuses a cell of the job
struct ObsignatioLanesJob
{
    ObsignatioManyStage level[OBSIGNATIO_EXTENT_AXES];
    unsigned int *refused;
};

// the chaining words of one level, which the first thread takes for the level and gives back after it
static __device__ unsigned int *s_obsignatio_wide;
static __device__ unsigned int *s_obsignatio_narrow;

// The first thread gives back the chaining words of the level, the newest cell first
static __device__ void obsignatio_job_chaining_given(void)
{
    platform_device_cell_released(s_obsignatio_narrow, platform_place_g_engine_owner);
    platform_device_cell_released(s_obsignatio_wide, platform_place_g_engine_owner);
    s_obsignatio_narrow = NULL;
    s_obsignatio_wide = NULL;
}

// One level, as obsignatio_many seals it. The first thread takes the chaining words where a message has more than one
// chunk. The grid seals each chunk, then reduces the chaining words by half on each pass to the root of each message.
// 1, or 0 where the device refuses a cell
static __device__ int obsignatio_job_many(const ObsignatioManyStage *stage, unsigned int *refused,
                                          const cooperative_groups::grid_group &grid)
{
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    const unsigned long long chunks =
        (stage->length == 0ULL) ? 1ULL : ((stage->length + OBSIGNATIO_CHUNK_BYTES - 1ULL) >> OBSIGNATIO_CHUNK_SHIFT);
    const unsigned long long halves = (chunks + 1ULL) / 2ULL;
    if ((first == 0ULL) && (chunks > 1ULL))
    {
        // Explicit casts take the addresses of the cells as their words
        s_obsignatio_wide = (unsigned int *)platform_device_cell_taken(
            stage->messages * chunks * OBSIGNATIO_CHAINING_WORDS * sizeof(unsigned int), platform_place_g_engine_owner,
            1);
        s_obsignatio_narrow = (unsigned int *)platform_device_cell_taken(
            stage->messages * halves * OBSIGNATIO_CHAINING_WORDS * sizeof(unsigned int), platform_place_g_engine_owner,
            1);
        *refused = ((s_obsignatio_wide == NULL) || (s_obsignatio_narrow == NULL)) ? 1u : 0u;
    }
    grid.sync();
    if (*refused != 0u)
    {
        if (first == 0ULL)
        {
            obsignatio_job_chaining_given();
        }
        return 0;
    }
    obsignatio_chunk_span(stage->bytes, stage->messages, stage->length, stage->stride, chunks, stage->key,
                          OBSIGNATIO_MODE_KEYED, s_obsignatio_wide, stage->signa, first, jump);
    grid.sync();
    unsigned int *from = s_obsignatio_wide;
    unsigned int *to = s_obsignatio_narrow;
    for (unsigned long long width = chunks; width > 1ULL; width = (width + 1ULL) / 2ULL)
    {
        obsignatio_level_span(from, stage->messages, width, stage->key, OBSIGNATIO_MODE_KEYED, to, stage->signa, first,
                              jump);
        grid.sync();
        unsigned int *const swapped = from;
        from = to;
        to = swapped;
    }
    if (first == 0ULL)
    {
        obsignatio_job_chaining_given();
    }
    return 1;
}

// The lanes job: the rows, then the planes, the volumes and the lanes, each level over the nodes of the level before it
__global__ void obsignatio_lanes_job_kernel(const ObsignatioLanesJob *job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    for (unsigned int level = 0u; level < OBSIGNATIO_EXTENT_AXES; level += 1u)
    {
        if (obsignatio_job_many(&job->level[level], job->refused, grid) == 0)
        {
            return;
        }
    }
}

extern "C" long obsignatio_lanes_job(const ObsignatioLanesJobRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return OBSIGNATIO_ERROR;
    }
    EngineError *const error = request->error;
    const unsigned long long nodes = obsignatio_lanes_nodes(request->extent);
    if (OBSIGNATIO_CHECK((request->lanes != NULL) && (request->nodes != NULL) && (nodes != 0ULL), request, error) == 0)
    {
        return OBSIGNATIO_ERROR;
    }
    const ObsignatioLevel levels[OBSIGNATIO_EXTENT_AXES] = {OBSIGNATIO_LEVEL_ROW, OBSIGNATIO_LEVEL_PLANE,
                                                            OBSIGNATIO_LEVEL_VOLUME, OBSIGNATIO_LEVEL_LANES};
    unsigned char keys[OBSIGNATIO_EXTENT_AXES][OBSIGNATIO_KEY_BYTES];
    for (unsigned int level = 0u; level < OBSIGNATIO_EXTENT_AXES; level += 1u)
    {
        if (obsignatio_level_key(levels[level], keys[level], error) != 0L)
        {
            return OBSIGNATIO_ERROR;
        }
    }
    const unsigned long long *const extent = request->extent;
    const unsigned long long volumes = extent[0];
    const unsigned long long planes = volumes * extent[1];
    const unsigned long long rows = planes * extent[2];
    const unsigned long long row_bytes = extent[3] * sizeof(unsigned short);
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, platform_place_g_engine_owner);
    void *cell = NULL;
    // the device is little-endian. The bytes of the lanes are their little-endian u16 form
    int ok = tessera_job_cells_in(&cells, request->lanes, rows * row_bytes, &cell);
    // Explicit cast reads the cell as the bytes of the lanes that crossed into it
    const unsigned char *const lane_bytes = (const unsigned char *)cell;
    ok = ok && tessera_job_cells_room(&cells, nodes * OBSIGNATIO_SIGNUM_BYTES, &cell);
    // Explicit cast reads the cell as the room of the nodes
    unsigned char *const row_nodes = (unsigned char *)cell;
    unsigned char *const plane_nodes = &row_nodes[rows * OBSIGNATIO_SIGNUM_BYTES];
    unsigned char *const volume_nodes = &plane_nodes[planes * OBSIGNATIO_SIGNUM_BYTES];
    unsigned char *const lanes_node = &volume_nodes[volumes * OBSIGNATIO_SIGNUM_BYTES];
    ObsignatioLanesJob job;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &job, .bytes = sizeof(job), .val = 0);
    const ObsignatioManyStage stages[OBSIGNATIO_EXTENT_AXES] = {
        {lane_bytes, rows, row_bytes, row_bytes, obsignatio_key_load(keys[0]), row_nodes},
        {row_nodes, planes, extent[2] * OBSIGNATIO_SIGNUM_BYTES, extent[2] * OBSIGNATIO_SIGNUM_BYTES,
         obsignatio_key_load(keys[1]), plane_nodes},
        {plane_nodes, volumes, extent[1] * OBSIGNATIO_SIGNUM_BYTES, extent[1] * OBSIGNATIO_SIGNUM_BYTES,
         obsignatio_key_load(keys[2]), volume_nodes},
        {volume_nodes, 1ULL, volumes * OBSIGNATIO_SIGNUM_BYTES, volumes * OBSIGNATIO_SIGNUM_BYTES,
         obsignatio_key_load(keys[3]), lanes_node}};
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = job.level, .src = stages, .bytes = sizeof(stages));
    ok = ok && tessera_job_cells_room(&cells, sizeof(unsigned int), &cell);
    // Explicit cast reads the cell as the flag of the job
    job.refused = (unsigned int *)cell;
    void *job_cell = NULL;
    ok = ok && tessera_job_cells_in(&cells, &job, sizeof(job), &job_cell);
    void *args[1] = {&job_cell};
    unsigned int refused = 1u;
    ok = OBSIGNATIO_JOB_CHECK(
             (ok != 0) && tessera_job_ran((const void *)obsignatio_lanes_job_kernel, OBSIGNATIO_JOB_THREADS, args),
             request, error) &&
         OBSIGNATIO_JOB_CHECK(tessera_job_crossed_out(&refused, job.refused, sizeof(refused)) && (refused == 0u),
                              request, error) &&
         OBSIGNATIO_JOB_CHECK(tessera_job_crossed_out(request->nodes, row_nodes, nodes * OBSIGNATIO_SIGNUM_BYTES),
                              request, error);
    tessera_job_cells_given(&cells);
    return (ok != 0) ? 0L : OBSIGNATIO_ERROR;
}

// What the bits job reads, which crosses in as one cell. It holds the stream, its offsets, its key and its mode. It
// also holds the room of the signa and the flag of a broken offset
struct ObsignatioBitsJob
{
    const unsigned int *limbs;
    const unsigned long long *offsets;
    unsigned long long messages;
    unsigned long long bits;
    ObsignatioKey key;
    unsigned int mode;
    unsigned char *signa;
    unsigned int *broken;
};

// The bits job. The grid checks that each offset lies within its message and the stream, then seals each message
__global__ void obsignatio_bits_job_kernel(const ObsignatioBitsJob *job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    obsignatio_bits_check_span(job->offsets, job->messages, job->bits, job->broken, first, jump);
    grid.sync();
    if (*job->broken != 0u)
    {
        return;
    }
    obsignatio_bits_span(job->limbs, job->offsets, job->messages, job->bits, job->key, job->mode, job->signa, first,
                         jump);
}

extern "C" long obsignatio_bits_job(const ObsignatioBitsJobRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return OBSIGNATIO_ERROR;
    }
    EngineError *const error = request->error;
    if (OBSIGNATIO_CHECK(((request->messages == 0ULL) || ((request->offsets != NULL) && (request->signa != NULL) &&
                                                          ((request->limbs != NULL) || (request->bits == 0ULL)))) &&
                             obsignatio_mode_valid(request->mode, request->key),
                         request, error) == 0)
    {
        return OBSIGNATIO_ERROR;
    }
    if (request->messages == 0ULL)
    {
        return 0L;
    }
    ObsignatioBitsJob job;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &job, .bytes = sizeof(job), .val = 0);
    job.messages = request->messages;
    job.bits = request->bits;
    job.key = obsignatio_key_load(request->key);
    job.mode = request->mode;
    const unsigned long long limbs = (request->bits + 31ULL) / 32ULL;
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, platform_place_g_engine_owner);
    void *cell = NULL;
    // the stream holds a word past its bits, which a read of the digest at its end reaches
    int ok = tessera_job_cells_room(&cells, (limbs + 1ULL) * sizeof(unsigned int), &cell) &&
             tessera_job_crossed_into(cell, request->limbs, limbs * sizeof(unsigned int));
    // Explicit cast reads the cell as the stream that crossed into it
    job.limbs = (const unsigned int *)cell;
    ok = ok && tessera_job_cells_in(&cells, request->offsets, job.messages * sizeof(unsigned long long), &cell);
    // Explicit cast reads the cell as the offsets that crossed into it
    job.offsets = (const unsigned long long *)cell;
    ok = ok && tessera_job_cells_room(&cells, job.messages * OBSIGNATIO_SIGNUM_BYTES, &cell);
    // Explicit cast reads the cell as the room of the signa
    job.signa = (unsigned char *)cell;
    ok = ok && tessera_job_cells_room(&cells, sizeof(unsigned int), &cell);
    // Explicit cast reads the cell as the flag of a broken offset
    job.broken = (unsigned int *)cell;
    void *job_cell = NULL;
    ok = ok && tessera_job_cells_in(&cells, &job, sizeof(job), &job_cell);
    void *args[1] = {&job_cell};
    unsigned int broken = 1u;
    ok =
        OBSIGNATIO_JOB_CHECK(
            (ok != 0) && tessera_job_ran((const void *)obsignatio_bits_job_kernel, OBSIGNATIO_JOB_THREADS, args) &&
                tessera_job_crossed_out(&broken, job.broken, sizeof(broken)),
            request, error) &&
        OBSIGNATIO_CHECK(broken == 0u, request->offsets, error) &&
        OBSIGNATIO_JOB_CHECK(tessera_job_crossed_out(request->signa, job.signa, job.messages * OBSIGNATIO_SIGNUM_BYTES),
                             request, error);
    tessera_job_cells_given(&cells);
    return (ok != 0) ? 0L : OBSIGNATIO_ERROR;
}
