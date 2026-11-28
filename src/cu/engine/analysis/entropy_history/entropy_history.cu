// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../../../../includes/platform/platform.h"
#include "../../runtime/daemon/tessera_job.h"
#include "entropy_history.h"
#include "memoria_operor/memoria_operor.h"

#include "crc.h"

#include <cuda_runtime.h>

static_assert(cudaSuccess == 0, "the engine reads a CUDA status of 0 as success");

// cudaError_t enumerates non-negative codes below INT_MAX: the status converts to int exactly
#define ENTROPY_HISTORY_STATUS_CHECK(call_, evacaddr_, error_)                                                         \
    engine_status_check((int)(call_), ENGINE_MODULE_ENTROPY_HISTORY, (unsigned int)__LINE__,                           \
                        (const void *)(evacaddr_), (error_))

#define ENTROPY_HISTORY_CHECK(condition_, evacaddr_, error_, type_)                                                    \
    engine_error_check((condition_), (type_), ENGINE_MODULE_ENTROPY_HISTORY, (unsigned int)__LINE__,                   \
                       (const void *)(evacaddr_), (error_))

#define ENTROPY_HISTORY_THREADS 256u

static_assert(ENGINE_HISTORY_WINDOW <= 15u, "entropy_history: a window's count must fit a nibble");
static_assert((ENGINE_HISTORY_BITS * 4u) == 64u, "entropy_history: a window's counts must fill one 64 bit word");

#define ENTROPY_HISTORY_DENSITY_MAX ((unsigned long long)ENGINE_HISTORY_WINDOW * 65535ULL)

__global__ static void entropy_history_kernel(const unsigned short *lanes, unsigned long long frames,
                                              unsigned long long voxels, unsigned int windows,
                                              unsigned long long *history, unsigned long long *broken,
                                              unsigned long long *cloud)
{
    __shared__ unsigned long long overlap[ENGINE_HISTORY_WINDOWS_MAX * ENGINE_HISTORY_WINDOWS_MAX];
    for (unsigned int entry = threadIdx.x; entry < (windows * windows); entry += blockDim.x)
    {
        overlap[entry] = 0ULL;
    }
    __syncthreads();
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    unsigned long long breaks = 0ULL;
    for (unsigned long long voxel = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; voxel < voxels;
         voxel += jump)
    {
        unsigned int density[ENGINE_HISTORY_WINDOWS_MAX];
        unsigned int counts[ENGINE_HISTORY_BITS];
        unsigned int totals[ENGINE_HISTORY_BITS];
#pragma unroll
        for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
        {
            counts[bit] = 0u;
            totals[bit] = 0u;
        }
        const unsigned int first = lanes[voxel];
        unsigned int before = first;
        unsigned long long window = 0ULL;
        unsigned int in_window = 0u;
        for (unsigned long long frame = 1ULL; frame < frames; frame += 1ULL)
        {
            const unsigned int now = lanes[(frame * voxels) + voxel];
            const unsigned int flipped = now ^ before;
            before = now;
#pragma unroll
            for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
            {
                counts[bit] += (flipped >> bit) & 1u;
                totals[bit] += (flipped >> bit) & 1u;
            }
            in_window += 1u;
            if ((in_window == ENGINE_HISTORY_WINDOW) || (frame == (frames - 1ULL)))
            {
                unsigned long long packed = 0ULL;
                unsigned int changed = 0u;
#pragma unroll
                for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
                {
                    packed |= (unsigned long long)counts[bit] << (4u * bit);
                    changed += counts[bit] << bit;
                    counts[bit] = 0u;
                }
                history[(window * voxels) + voxel] = packed;
                density[window] = changed;
                window += 1ULL;
                in_window = 0u;
            }
        }
        const unsigned int net = first ^ before;
        unsigned int parity_ok = 1u;
#pragma unroll
        for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
        {
            parity_ok &= (unsigned int)((totals[bit] & 1u) == ((net >> bit) & 1u));
        }
        breaks += (parity_ok != 0u) ? 0ULL : 1ULL;
        for (unsigned int one = 0u; one < windows; one += 1u)
        {
            for (unsigned int other = one; other < windows; other += 1u)
            {
                atomicAdd(&overlap[(one * windows) + other], (unsigned long long)density[one] * density[other]);
            }
        }
    }
    if (breaks != 0ULL)
    {
        atomicAdd(broken, breaks);
    }
    __syncthreads();
    for (unsigned int entry = threadIdx.x; entry < (windows * windows); entry += blockDim.x)
    {
        if (overlap[entry] != 0ULL)
        {
            atomicAdd(&cloud[entry], overlap[entry]);
        }
    }
}

extern "C" long entropy_history_windows(const unsigned long long extent[4], EngineError *error)
{
    if (error == NULL)
    {
        return ENTROPY_HISTORY_ERROR;
    }
    if (!ENTROPY_HISTORY_CHECK(extent != NULL, &extent, error, ENGINE_ERROR_REQUEST))
    {
        return ENTROPY_HISTORY_ERROR;
    }
    const unsigned long long frames = extent[0];
    const unsigned long long voxels = extent[1] * extent[2] * extent[3];
    if (!ENTROPY_HISTORY_CHECK(frames >= 2ULL, &extent[0], error, ENGINE_ERROR_REQUEST))
    {
        return ENTROPY_HISTORY_ERROR;
    }
    const unsigned long long windows = ((frames - 1ULL) + ENGINE_HISTORY_WINDOW - 1ULL) / ENGINE_HISTORY_WINDOW;
    const int bounded =
        (windows <= ENGINE_HISTORY_WINDOWS_MAX) &&
        (voxels <= (0xFFFFFFFFFFFFFFFFULL / (ENTROPY_HISTORY_DENSITY_MAX * ENTROPY_HISTORY_DENSITY_MAX)));
    return ENTROPY_HISTORY_CHECK(bounded, extent, error, ENGINE_ERROR_REQUEST) ? (long)windows : ENTROPY_HISTORY_ERROR;
}

extern "C" long entropy_history_project(const EntropyHistoryProjectRequest *request, unsigned long long *broken)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return ENTROPY_HISTORY_ERROR;
    }
    EngineError *const error = request->error;
    if (!ENTROPY_HISTORY_CHECK((request->history != NULL) && (request->volume != NULL) && (broken != NULL), request,
                               error, ENGINE_ERROR_REQUEST))
    {
        return ENTROPY_HISTORY_ERROR;
    }
    const long counted = entropy_history_windows(request->extent, error);
    if (counted == ENTROPY_HISTORY_ERROR)
    {
        return ENTROPY_HISTORY_ERROR;
    }
    EngineHistory *const history = request->history;
    const unsigned long long windows = (unsigned long long)counted;
    const unsigned long long frames = request->extent[0];
    const unsigned long long voxels = request->extent[1] * request->extent[2] * request->extent[3];
    const size_t words = (size_t)(windows * voxels);
    const size_t entries = (size_t)(windows * windows);

    const unsigned long long history_bytes = (unsigned long long)words * sizeof(unsigned long long);
    const unsigned long long cloud_bytes = (unsigned long long)entries * sizeof(unsigned long long);
    // The lanes cross in. The history, the count of the breaks and the cloud are temporary cells of the pool of the
    // device. Every byte is 0 at the take. The call gives them back last first before it returns
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, (unsigned int)platform_place_g_engine_owner);
    void *cell[4] = {NULL, NULL, NULL, NULL};
    *broken = 1ULL;
    int ok = ENTROPY_HISTORY_CHECK(
        tessera_job_cells_in(&cells, request->volume, frames * voxels * sizeof(unsigned short), &cell[0]) &&
            tessera_job_cells_room(&cells, history_bytes, &cell[1]) &&
            tessera_job_cells_room(&cells, sizeof(unsigned long long), &cell[2]) &&
            tessera_job_cells_room(&cells, cloud_bytes, &cell[3]),
        request->volume, error, ENGINE_ERROR_RESOURCE);
    // Explicit casts read each cell as the words that it holds
    const unsigned short *const device_lanes = (const unsigned short *)cell[0];
    unsigned long long *const device_history = (unsigned long long *)cell[1];
    unsigned long long *const device_broken = (unsigned long long *)cell[2];
    unsigned long long *const device_cloud = (unsigned long long *)cell[3];
    if (ok != 0)
    {
        const unsigned long long needed = (voxels + ENTROPY_HISTORY_THREADS - 1ULL) / ENTROPY_HISTORY_THREADS;
        const unsigned int blocks = (unsigned int)((needed < 65536ULL) ? needed : 65536ULL);
        entropy_history_kernel<<<blocks, ENTROPY_HISTORY_THREADS>>>(device_lanes, frames, voxels, (unsigned int)windows,
                                                                    device_history, device_broken, device_cloud);
        ok = ENTROPY_HISTORY_STATUS_CHECK(cudaGetLastError(), device_history, error);
    }
    ok = ok && ENTROPY_HISTORY_CHECK(tessera_job_crossed_out(history->history, device_history, history_bytes) &&
                                         tessera_job_crossed_out(history->cloud, device_cloud, cloud_bytes) &&
                                         tessera_job_crossed_out(broken, device_broken, sizeof(unsigned long long)),
                                     history->history, error, ENGINE_ERROR_RESOURCE);
    tessera_job_cells_given(&cells);
    if ((ok == 0) || !ENTROPY_HISTORY_CHECK(*broken == 0ULL, broken, error, ENGINE_ERROR_LOGIC))
    {
        return ENTROPY_HISTORY_ERROR;
    }
    for (unsigned long long one = 0ULL; one < windows; one += 1ULL)
    {
        for (unsigned long long other = 0ULL; other < one; other += 1ULL)
        {
            history->cloud[(size_t)((one * windows) + other)] = history->cloud[(size_t)((other * windows) + one)];
        }
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = history->extent, .src = request->extent, .bytes = sizeof(history->extent));
    history->windows = windows;
    history->sample = request->sample;
    history->payload_crc = crc_words(CRC_TABLE, history->history, words);
    history->cloud_crc = crc_words(CRC_TABLE, history->cloud, entries);
    return 0L;
}

static void entropy_history_report_cloud(const EngineHistory *history)
{
    const unsigned long long windows = history->windows;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(
            output_line,
            "    the cloud: each floor's quietest partner, the first floor its nulls are drawn from:\n      floor");
        platform_line_out_ended();
    }
    for (unsigned long long floor = 0ULL; floor < windows; floor += 1ULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal_columns(output_line, floor, 2u);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n      quiet");
        platform_line_out_ended();
    }
    for (unsigned long long floor = 0ULL; floor < windows; floor += 1ULL)
    {
        unsigned long long quietest = (floor == 0ULL) ? 1ULL : 0ULL;
        for (unsigned long long other = 0ULL; other < windows; other += 1ULL)
        {
            const int better = (other != floor) && (history->cloud[(size_t)((floor * windows) + other)] <
                                                    history->cloud[(size_t)((floor * windows) + quietest)]);
            quietest = better ? other : quietest;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, quietest, 2u);
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

extern "C" void entropy_history_report(const EngineHistory *history)
{
    const unsigned long long frames = history->extent[0];
    const unsigned long long voxels = history->extent[1] * history->extent[2] * history->extent[3];
    const unsigned long long windows = history->windows;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    flips per thousand transitions, bit by window:\n      bit");
        platform_line_out_ended();
    }
    for (unsigned long long window = 0ULL; window < windows; window += 1ULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal_columns(output_line, window, 5u);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int bit = ENGINE_HISTORY_BITS; bit > 0u; bit -= 1u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "      ");
            mmgr_line_decimal_columns(output_line, bit - 1u, 3u);
            platform_line_out_ended();
        }
        for (unsigned long long window = 0ULL; window < windows; window += 1ULL)
        {
            const unsigned long long first = (window * ENGINE_HISTORY_WINDOW) + 1ULL;
            const unsigned long long last = ((first + ENGINE_HISTORY_WINDOW - 1ULL) < frames)
                                                ? (first + ENGINE_HISTORY_WINDOW - 1ULL)
                                                : (frames - 1ULL);
            const unsigned long long taken = last - first + 1ULL;
            unsigned long long flips = 0ULL;
            const unsigned long long *const row = &history->history[(size_t)(window * voxels)];
            for (unsigned long long voxel = 0ULL; voxel < voxels; voxel += 1ULL)
            {
                flips += (row[voxel] >> (4u * (bit - 1u))) & 0xFULL;
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                mmgr_line_decimal_columns(output_line, (1000ULL * flips) / (taken * voxels), 5u);
                platform_line_out_ended();
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    entropy_history_report_cloud(history);
}
