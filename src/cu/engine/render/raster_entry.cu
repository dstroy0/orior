// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster_entry.cu: the device entry points
#include "../../../../includes/platform/platform.h"
#include "../runtime/daemon/tessera_job.h"
#include "raster_cuda_internal.h"

// the owner that the cells of the renderer in the pool of the device count to
#define RASTER_PLACE ((unsigned int)platform_place_g_engine_owner)

// The cells of one render on the device. The staging holds a word a cell, and the error a mark of a layout that
// errored. The corpus, the needle, the probes and the counts of the bytes cross in
struct RasterCells
{
    unsigned int *staging;
    int *error;
    const unsigned char *corpus;
    const unsigned char *needle;
    const DeviceProbe *probes;
    const unsigned long long *occurrences;
};

// each word of the staging set to the identity of the reduction
__global__ static void raster_identity_kernel(unsigned int *staging, unsigned long long cells, unsigned int identity)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long cell = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; cell < cells;
         cell += jump)
    {
        staging[cell] = identity;
    }
}

// The cells of a render taken into `list` as temporary cells of the pool of the device, which the caller gives back
// before it returns. The staging starts at `identity`, and the error at 0. 1, or 0 where a take, a crossing or the
// kernel errors
static int raster_cells_taken(TesseraJobCells *list, size_t cells, unsigned int identity, const uint8_t *corpus,
                              size_t corpus_len, const uint8_t *needle, size_t needle_len, const DeviceProbe *staged,
                              size_t probe_count, const unsigned long long *occurrences, RasterCells *taken)
{
    void *cell[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
    int ok = tessera_job_cells_room(list, (unsigned long long)cells * sizeof(unsigned int), &cell[0]) &&
             tessera_job_cells_room(list, sizeof(int), &cell[1]) &&
             tessera_job_cells_in(list, corpus, corpus_len, &cell[2]) &&
             tessera_job_cells_in(list, needle, needle_len, &cell[3]) &&
             tessera_job_cells_in(list, staged, (unsigned long long)probe_count * sizeof(DeviceProbe), &cell[4]) &&
             tessera_job_cells_in(list, occurrences, 256ULL * sizeof(unsigned long long), &cell[5]);
    // Explicit casts read each cell as the words, the bytes and the records that it holds
    taken->staging = (unsigned int *)cell[0];
    taken->error = (int *)cell[1];
    taken->corpus = (const unsigned char *)cell[2];
    taken->needle = (const unsigned char *)cell[3];
    taken->probes = (const DeviceProbe *)cell[4];
    taken->occurrences = (const unsigned long long *)cell[5];
    if (ok != 0)
    {
        const unsigned long long needed = ((unsigned long long)cells + 255ULL) / 256ULL;
        // Explicit cast takes a count of blocks below 4096
        raster_identity_kernel<<<(unsigned int)((needed < 4096ULL) ? needed : 4096ULL), 256u>>>(
            taken->staging, (unsigned long long)cells, identity);
        ok = cudaGetLastError() == cudaSuccess;
    }
    return ok;
}

// The staging crossed out and narrowed into `out`, a byte a cell. A cell still at `identity` reads as empty. 1, or 0
// where the host cell or the crossing errors
static int raster_cells_narrowed(const unsigned int *staging, size_t cells, unsigned int identity, uint8_t *out)
{
    unsigned int *const result =
        (unsigned int *)platform_cell_taken((unsigned long long)(cells * sizeof(unsigned int)), RASTER_PLACE, 0);
    const int ok =
        (result != NULL) && tessera_job_crossed_out(result, staging, (unsigned long long)cells * sizeof(unsigned int));
    for (size_t cell = 0u; (ok != 0) && (cell < cells); cell += 1u)
    {
        out[cell] = (result[cell] == identity) ? (uint8_t)ORIOR_RASTER_EMPTY : (uint8_t)result[cell];
    }
    platform_cell_released(result, RASTER_PLACE);
    return ok;
}

extern "C" int orior_volume_device_available(void)
{
    int devices = 0;
    if ((cudaGetDeviceCount(&devices) != cudaSuccess) || (devices < 1))
    {
        return 0;
    }
    return 1;
}

extern "C" int orior_volume_device(uint8_t *voxels, const OriorVolumeConfig *config, const uint8_t *corpus,
                                   size_t corpus_len, const uint8_t *needle, size_t needle_len,
                                   const OriorRasterProbe *probes, size_t probe_count, const void *census)
{
    // RESERVED. Built from the corpus below exactly as the host arm does. The two
    // agree on rarity, and a caller supplied census is discarded on both.
    (void)census;

    if ((voxels == NULL) || (config == NULL) || (corpus == NULL) || (needle == NULL) || (config->width == 0u) ||
        (config->height == 0u) || (config->depth == 0u) || (needle_len == 0u) || (needle_len > corpus_len))
    {
        return 0;
    }
    if ((probes == NULL) && (probe_count != 0u))
    {
        return 0;
    }
    if (orior_volume_device_available() == 0)
    {
        return 0;
    }

    const size_t cells = config->width * config->height * config->depth;
    const size_t alignments = (corpus_len - needle_len) + 1u;

    unsigned long long occurrences[256];
    for (size_t symbol = 0u; symbol < 256u; symbol += 1u)
    {
        occurrences[symbol] = 0u;
    }
    for (size_t at = 0u; at < corpus_len; at += 1u)
    {
        occurrences[corpus[at]] += 1u;
    }

    DeviceVolumeConfig plan;
    plan.width = config->width;
    plan.height = config->height;
    plan.depth = config->depth;
    plan.layout = (int)config->layout;
    plan.channel = (int)config->channel;
    plan.reduce = (int)config->reduce;
    plan.gain = config->gain;

    DeviceProbe staged[16];
    if (probe_count > 16u)
    {
        return 0;
    }
    for (size_t index = 0u; index < probe_count; index += 1u)
    {
        staged[index].origin = probes[index].origin;
        staged[index].step = probes[index].step;
        staged[index].length = probes[index].length;
    }

    // The staging starts at the reduction's identity. The narrowing pass turns any untouched cell back into empty, the
    // same bytes the host's first-arrival fill writes
    const unsigned int identity = (config->reduce == ORIOR_REDUCE_MAX) ? 0u : 0xFFFFFFFFu;
    TesseraJobCells list;
    tessera_job_cells_begun(&list, RASTER_PLACE);
    RasterCells taken;
    int ok = raster_cells_taken(&list, cells, identity, corpus, corpus_len, needle, needle_len, staged, probe_count,
                                occurrences, &taken);
    int error = 0;
    if (ok != 0)
    {
        const unsigned int threads = 256u;
        const unsigned int blocks = (unsigned int)((alignments + threads - 1u) / threads);
        render_volume<<<blocks, threads>>>(taken.staging, taken.error, plan, taken.corpus,
                                           (unsigned long long)corpus_len, taken.needle, (unsigned long long)needle_len,
                                           taken.probes, (unsigned long long)probe_count, taken.occurrences,
                                           (unsigned long long)corpus_len);
        ok = (cudaDeviceSynchronize() == cudaSuccess) && tessera_job_crossed_out(&error, taken.error, sizeof(int));
    }
    // The layout errored on this configuration, exactly as the host returns 0 without writing a volume
    ok = ok && (error == 0) && raster_cells_narrowed(taken.staging, cells, identity, voxels);
    tessera_job_cells_given(&list);
    return ok;
}

extern "C" int orior_raster_device(uint8_t *pixels, const OriorRasterConfig *config, const uint8_t *corpus,
                                   size_t corpus_len, const uint8_t *needle, size_t needle_len,
                                   const OriorRasterProbe *probes, size_t probe_count)
{
    if ((pixels == NULL) || (config == NULL) || (corpus == NULL) || (needle == NULL) || (config->width == 0u) ||
        (config->height == 0u) || (needle_len == 0u) || (needle_len > corpus_len))
    {
        return 0;
    }
    if ((probes == NULL) && (probe_count != 0u))
    {
        return 0;
    }
    if (orior_raster_device_available() == 0)
    {
        return 0;
    }

    const size_t cells = config->width * config->height;
    const size_t alignments = (corpus_len - needle_len) + 1u;

    /* The census is built on the host and uploaded. It is one pass over the corpus either way, and
     * building it here would add a second reduction to grade against the host's. */
    unsigned long long occurrences[256];
    for (size_t symbol = 0u; symbol < 256u; symbol += 1u)
    {
        occurrences[symbol] = 0u;
    }
    for (size_t at = 0u; at < corpus_len; at += 1u)
    {
        occurrences[corpus[at]] += 1u;
    }

    DeviceConfig plan;
    plan.width = config->width;
    plan.height = config->height;
    plan.layout = (int)config->layout;
    plan.channel = (int)config->channel;
    plan.reduce = (int)config->reduce;
    plan.gain = config->gain;

    DeviceProbe staged[16];
    if (probe_count > 16u)
    {
        return 0;
    }
    for (size_t index = 0u; index < probe_count; index += 1u)
    {
        staged[index].origin = probes[index].origin;
        staged[index].step = probes[index].step;
        staged[index].length = probes[index].length;
    }

    // An empty cell is the identity for whichever reduction runs. The host marks which cells an arrival reached. Here
    // the staging starts at the identity and the narrowing pass turns any untouched cell back into empty
    const unsigned int identity = (config->reduce == ORIOR_REDUCE_MAX) ? 0u : 0xFFFFFFFFu;
    TesseraJobCells list;
    tessera_job_cells_begun(&list, RASTER_PLACE);
    RasterCells taken;
    int ok = raster_cells_taken(&list, cells, identity, corpus, corpus_len, needle, needle_len, staged, probe_count,
                                occurrences, &taken);
    if (ok != 0)
    {
        const unsigned int threads = 256u;
        const unsigned int blocks = (unsigned int)((alignments + threads - 1u) / threads);
        render_alignments<<<blocks, threads>>>(taken.staging, plan, taken.corpus, (unsigned long long)corpus_len,
                                               taken.needle, (unsigned long long)needle_len, taken.probes,
                                               (unsigned long long)probe_count, taken.occurrences,
                                               (unsigned long long)corpus_len);
        ok = cudaDeviceSynchronize() == cudaSuccess;
    }
    ok = ok && raster_cells_narrowed(taken.staging, cells, identity, pixels);
    tessera_job_cells_given(&list);
    return ok;
}
