// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// noise_detector_flicker.cu: flicker
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "noise_detector_internal.h"

__device__ static void noise_flicker_flush(unsigned long long *cells, unsigned int lag, unsigned int level_bin,
                                           unsigned long long run[NOISE_FLICKER_SUMS])
{
    unsigned long long *const cell = &cells[((lag * NOISE_LEVEL_BINS) + level_bin) * NOISE_FLICKER_SUMS];
    for (unsigned int sum = 0u; sum < NOISE_FLICKER_SUMS; sum += 1u)
    {
        if (run[sum] != 0ULL)
        {
            atomicAdd(&cell[sum], run[sum]);
        }
        run[sum] = 0ULL;
    }
}

// One thread a voxel. Consecutive pairs of a voxel mostly fall in one level bin. A pair adds to the thread's run
// and the run goes to the block's sums only when the bin changes.
__global__ static void noise_flicker_kernel(const unsigned short *lanes, unsigned long long frames,
                                            unsigned long long voxels, unsigned long long width,
                                            unsigned long long *sums)
{
    __shared__ unsigned long long cells[NOISE_FLICKER_CELLS];
    for (unsigned int entry = threadIdx.x; entry < NOISE_FLICKER_CELLS; entry += blockDim.x)
    {
        cells[entry] = 0ULL;
    }
    __syncthreads();
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long voxel = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; voxel < voxels;
         voxel += jump)
    {
        const int beside = ((voxel % width) + 1ULL) < width;
        for (unsigned int lag = 0u; lag < NOISE_FLICKER_LAGS; lag += 1u)
        {
            const unsigned long long apart = 1ULL << lag;
            unsigned long long run[NOISE_FLICKER_SUMS] = {0ULL, 0ULL, 0ULL, 0ULL};
            unsigned int run_bin = 0u;
            for (unsigned long long frame = 0ULL; (frame + apart) < frames; frame += 1ULL)
            {
                const unsigned long long early = (frame * voxels) + voxel;
                const unsigned long long late = ((frame + apart) * voxels) + voxel;
                const unsigned int before = lanes[early];
                const unsigned int after = lanes[late];
                const unsigned int summed_bin = (before + after) >> NOISE_LEVEL_BIN_SHIFT;
                const unsigned int level_bin = (summed_bin < NOISE_LEVEL_BINS) ? summed_bin : (NOISE_LEVEL_BINS - 1u);
                if ((level_bin != run_bin) && (run[NOISE_FLICKER_PAIRS] != 0ULL))
                {
                    noise_flicker_flush(cells, lag, run_bin, run);
                }
                run_bin = level_bin;
                const long long moved = (long long)after - (long long)before;
                run[NOISE_FLICKER_PAIRS] += 1ULL;
                // a square is never negative. It re-signs to unsigned long long exactly
                run[NOISE_FLICKER_SQUARES] += (unsigned long long)(moved * moved);
                if (beside)
                {
                    const long long neighbor_moved = (long long)lanes[late + 1ULL] - (long long)lanes[early + 1ULL];
                    const long long against_neighbor = moved - neighbor_moved;
                    run[NOISE_FLICKER_NEIGHBOR_PAIRS] += 1ULL;
                    run[NOISE_FLICKER_NEIGHBOR_SQUARES] += (unsigned long long)(against_neighbor * against_neighbor);
                }
            }
            if (run[NOISE_FLICKER_PAIRS] != 0ULL)
            {
                noise_flicker_flush(cells, lag, run_bin, run);
            }
        }
    }
    __syncthreads();
    for (unsigned int entry = threadIdx.x; entry < NOISE_FLICKER_CELLS; entry += blockDim.x)
    {
        if (cells[entry] != 0ULL)
        {
            atomicAdd(&sums[entry], cells[entry]);
        }
    }
}

long noise_flicker_sample(const unsigned short *volume, const unsigned long long extent[4],
                          unsigned long long sums[NOISE_FLICKER_CELLS], EngineError *error)
{
    const unsigned long long frames = extent[0];
    const unsigned long long voxels = extent[1] * extent[2] * extent[3];
    const unsigned long long width = extent[3];
    // every sum is at max one sample's pairs times the widest square, and so is every total the summary forms
    const int bounded = (frames >= 2ULL) && (voxels != 0ULL) && (width != 0ULL) && (frames <= (~0ULL / voxels)) &&
                        ((frames * voxels) <= (~0ULL / NOISE_WIDEST_SQUARE));
    if (!NOISE_DETECTOR_CHECK(bounded, extent, error, ENGINE_ERROR_REQUEST))
    {
        return NOISE_DETECTOR_ERROR;
    }
    const unsigned long long lane_bytes = frames * voxels * sizeof(unsigned short);
    const unsigned long long sum_bytes = (unsigned long long)NOISE_FLICKER_CELLS * sizeof(unsigned long long);
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, NOISE_DETECTOR_PLACE);
    unsigned short *device_lanes = NULL;
    unsigned long long *device_sums = NULL;
    int ok = noise_lanes_crossed(&cells, volume, lane_bytes, sum_bytes, &device_lanes, &device_sums, error);
    if (ok != 0)
    {
        const unsigned long long needed = (voxels + NOISE_DETECTOR_THREADS - 1ULL) / NOISE_DETECTOR_THREADS;
        const unsigned int blocks = (unsigned int)((needed < 65536ULL) ? needed : 65536ULL);
        noise_flicker_kernel<<<blocks, NOISE_DETECTOR_THREADS>>>(device_lanes, frames, voxels, width, device_sums);
        ok = NOISE_DETECTOR_STATUS_CHECK(cudaGetLastError(), device_sums, error) &&
             NOISE_DETECTOR_CHECK(tessera_job_crossed_out(sums, device_sums, sum_bytes), sums, error,
                                  ENGINE_ERROR_RESOURCE);
    }
    tessera_job_cells_given(&cells);
    return (ok != 0) ? 0L : NOISE_DETECTOR_ERROR;
}

int noise_lanes_crossed(TesseraJobCells *cells, const unsigned short *volume, unsigned long long lane_bytes,
                        unsigned long long sum_bytes, unsigned short **lanes, unsigned long long **sums,
                        EngineError *error)
{
    void *lane_cell = NULL;
    void *sum_cell = NULL;
    const int ok = NOISE_DETECTOR_CHECK(tessera_job_cells_in(cells, volume, lane_bytes, &lane_cell) &&
                                            tessera_job_cells_room(cells, sum_bytes, &sum_cell),
                                        volume, error, ENGINE_ERROR_RESOURCE);
    // Explicit casts read the cells as the lanes and the sums that they hold
    *lanes = (unsigned short *)lane_cell;
    *sums = (unsigned long long *)sum_cell;
    return ok;
}

void noise_exact_word(OriorExactInteger *value, unsigned long long word)
{
    orior_exact_zero(value);
    value->limb[0] = (uint32_t)(word & 0xFFFFFFFFULL);
    value->limb[1] = (uint32_t)(word >> 32u);
    value->sign = (word != 0ULL) ? 1 : 0;
}

// (squares / pairs) against (first_squares / first_pairs), in thousandths rounded down: the four words multiplied
// across. No mean is ever rounded. 0 where either mean is undefined or the quotient passes 2^64.
int noise_per_mille(unsigned long long squares, unsigned long long pairs, unsigned long long first_squares,
                    unsigned long long first_pairs, unsigned long long *per_mille)
{
    if ((pairs == 0ULL) || (first_pairs == 0ULL) || (first_squares == 0ULL))
    {
        return 0;
    }
    OriorExactInteger numerator;
    OriorExactInteger denominator;
    OriorExactInteger factor;
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    noise_exact_word(&numerator, squares);
    noise_exact_word(&factor, first_pairs);
    int ok = orior_exact_multiply(&numerator, &factor, &numerator) == ORIOR_EXACT_OK;
    noise_exact_word(&factor, 1000ULL);
    ok = ok && (orior_exact_multiply(&numerator, &factor, &numerator) == ORIOR_EXACT_OK);
    noise_exact_word(&denominator, first_squares);
    noise_exact_word(&factor, pairs);
    ok = ok && (orior_exact_multiply(&denominator, &factor, &denominator) == ORIOR_EXACT_OK) &&
         (orior_exact_divide(&numerator, &denominator, &quotient, &remainder) == ORIOR_EXACT_OK);
    for (unsigned int limb = 2u; ok && (limb < ORIOR_EXACT_LIMBS); limb += 1u)
    {
        ok = quotient.limb[limb] == 0u;
    }
    *per_mille = ok ? (((unsigned long long)quotient.limb[1] << 32u) | quotient.limb[0]) : 0ULL;
    return ok;
}

// every lag's sums pooled over the summary's level bins, means 40 to 199
void noise_flicker_pooled(const unsigned long long sums[NOISE_FLICKER_CELLS],
                          unsigned long long pooled[NOISE_FLICKER_LAGS][NOISE_FLICKER_SUMS])
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = pooled,
               .bytes = (size_t)NOISE_FLICKER_LAGS * NOISE_FLICKER_SUMS * sizeof(unsigned long long), .val = 0);
    for (unsigned int lag = 0u; lag < NOISE_FLICKER_LAGS; lag += 1u)
    {
        for (unsigned int level_bin = NOISE_SUMMARY_FIRST_BIN; level_bin <= NOISE_SUMMARY_LAST_BIN; level_bin += 1u)
        {
            for (unsigned int sum = 0u; sum < NOISE_FLICKER_SUMS; sum += 1u)
            {
                pooled[lag][sum] += sums[(((lag * NOISE_LEVEL_BINS) + level_bin) * NOISE_FLICKER_SUMS) + sum];
            }
        }
    }
}

static void noise_flicker_summary(const char *name, const unsigned long long extent[4],
                                  const unsigned long long sums[NOISE_FLICKER_CELLS])
{
    unsigned long long pooled[NOISE_FLICKER_LAGS][NOISE_FLICKER_SUMS];
    noise_flicker_pooled(sums, pooled);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, name, 24u);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, extent[0], 1u);
        mmgr_line_put(output_line, " frames of ");
        mmgr_line_decimal(output_line, extent[1], 1u);
        mmgr_line_put(output_line, "x");
        mmgr_line_decimal(output_line, extent[2], 1u);
        mmgr_line_put(output_line, "x");
        mmgr_line_decimal(output_line, extent[3], 1u);
        mmgr_line_put(output_line, "; means 40 to 199, ");
        mmgr_line_decimal(output_line, pooled[0][NOISE_FLICKER_PAIRS], 1u);
        mmgr_line_put(output_line, " pairs at lag 1");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned int types[2][2] = {{NOISE_FLICKER_SQUARES, NOISE_FLICKER_PAIRS},
                                      {NOISE_FLICKER_NEIGHBOR_SQUARES, NOISE_FLICKER_NEIGHBOR_PAIRS}};
    const char *const labels[2] = {"the frame difference         ", "less its x neighbor's       "};
    for (unsigned int type = 0u; type < 2u; type += 1u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    ");
            mmgr_line_put(output_line, labels[type]);
            platform_line_out_ended();
        }
        for (unsigned int lag = 0u; lag < NOISE_FLICKER_LAGS; lag += 1u)
        {
            unsigned long long per_mille = 0ULL;
            if (noise_per_mille(pooled[lag][types[type][0]], pooled[lag][types[type][1]], pooled[0][types[type][0]],
                                pooled[0][types[type][1]], &per_mille))
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                mmgr_line_decimal_columns(output_line, per_mille, 6u);
                platform_line_out_ended();
            }
            else
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                mmgr_line_put_right(output_line, "-", 6u);
                platform_line_out_ended();
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    platform_line_flushed();
}

static int noise_flicker_rows(PlatformFile *table, const char *name, const unsigned long long sums[NOISE_FLICKER_CELLS])
{
    int ok = 1;
    for (unsigned int lag = 0u; ok && (lag < NOISE_FLICKER_LAGS); lag += 1u)
    {
        for (unsigned int level_bin = 0u; ok && (level_bin < NOISE_LEVEL_BINS); level_bin += 1u)
        {
            const unsigned long long *const cell = &sums[((lag * NOISE_LEVEL_BINS) + level_bin) * NOISE_FLICKER_SUMS];
            if ((cell[NOISE_FLICKER_PAIRS] == 0ULL) && (cell[NOISE_FLICKER_NEIGHBOR_PAIRS] == 0ULL))
            {
                continue;
            }
            const unsigned int lowest = level_bin << (NOISE_LEVEL_BIN_SHIFT - 1u);
            unsigned long long formed_mark_1;
            ok =
                (formed_mark_1 = platform_file_mark(table), mmgr_line_put(platform_file_line(table), name),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), 1u << lag, 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), lowest, 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), cell[NOISE_FLICKER_PAIRS], 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), cell[NOISE_FLICKER_SQUARES], 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), cell[NOISE_FLICKER_NEIGHBOR_PAIRS], 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), cell[NOISE_FLICKER_NEIGHBOR_SQUARES], 1u),
                 mmgr_line_put(platform_file_line(table), "\n"), platform_file_written_since(table, formed_mark_1)) > 0;
        }
    }
    return ok;
}

extern "C" long noise_flicker_set(const NoiseSetRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return NOISE_DETECTOR_ERROR;
    }
    EngineError *const error = request->error;
    char path[ENGINE_PATH_CAPACITY];
    VerbaLinea formed_line_1;
    const int written = (request->set != NULL) ? (formed_line_1 = mmgr_line_over(path, sizeof(path)),
                                                  mmgr_line_put(&formed_line_1, request->set),
                                                  mmgr_line_put(&formed_line_1, "/noise_flicker.tsv"),
                                                  (void)mmgr_line_finish(&formed_line_1), (int)formed_line_1.at)
                                               : -1;
    const int named = (written > 0) && ((size_t)written < sizeof(path));
    PlatformFile *const table = named ? platform_file_open(path, "wb") : NULL;
    unsigned long long formed_mark_2;
    int ok = NOISE_DETECTOR_CHECK(named && (request->samples != NULL) && (request->load != NULL), request, error,
                                  ENGINE_ERROR_REQUEST) &&
             NOISE_DETECTOR_IO(table != NULL, path, error) &&
             NOISE_DETECTOR_IO(
                 (formed_mark_2 = platform_file_mark(table),
                  mmgr_line_put(platform_file_line(table),
                                "sample\tlag\tlowest_mean\tpairs\tsquares\tneighbor_pairs\tneighbor_squares\n"),
                  platform_file_written_since(table, formed_mark_2)) > 0,
                 table, error);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  flicker: the structure function D(k), the mean square of I(t + k) - I(t), at "
                                   "lags 1 to 64, in bins of 8 by the pair's mean");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  each lag's mean square against lag 1's, per mille (white noise gives 1000 at every lag):");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    ");
        mmgr_line_put_columns(output_line, "lag", 29u);
        platform_line_out_ended();
    }
    for (unsigned int lag = 0u; lag < NOISE_FLICKER_LAGS; lag += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal_columns(output_line, 1u << lag, 6u);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned long long *const sums = (unsigned long long *)platform_cell_taken(
        (size_t)NOISE_FLICKER_CELLS * sizeof(unsigned long long), platform_place_g_program_owner, 0);
    ok = ok && NOISE_DETECTOR_CHECK(sums != NULL, &sums, error, ENGINE_ERROR_RESOURCE);
    const unsigned long long began = engine_clock_microseconds();
    for (unsigned int sample = 0u; ok && (sample < request->count); sample += 1u)
    {
        const char *const name = request->samples[sample];
        unsigned long long extent[4] = {0ULL, 0ULL, 0ULL, 0ULL};
        unsigned short *volume = NULL;
        ok = NOISE_DETECTOR_CHECK(request->load(request->set, name, extent, &volume, error) == 0L, name, error,
                                  ENGINE_ERROR_REQUEST) &&
             (noise_flicker_sample(volume, extent, sums, error) == 0L);
        platform_cell_released(volume, platform_place_g_program_owner);
        ok = ok && NOISE_DETECTOR_IO(noise_flicker_rows(table, name, sums) != 0, table, error);
        if (ok != 0)
        {
            noise_flicker_summary(name, extent, sums);
        }
    }
    platform_cell_released(sums, platform_place_g_program_owner);
    if (table != NULL)
    {
        ok = NOISE_DETECTOR_IO(platform_file_close(table) == 0, path, error) && ok;
    }
    if (ok != 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  flicker: ");
        mmgr_line_decimal(output_line, request->count, 1u);
        mmgr_line_put(output_line, " samples in ");
        mmgr_line_decimal(output_line, (engine_clock_microseconds() - began) / 1000ULL, 1u);
        mmgr_line_put(output_line, " ms; every sum is in ");
        mmgr_line_put(output_line, path);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (ok != 0) ? 0L : NOISE_DETECTOR_ERROR;
}
