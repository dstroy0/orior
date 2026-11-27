// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../../../platform/platform.h"
#include "../../runtime/daemon/tessera_job.h"
#include "box_history.h"

#include <cuda_runtime.h>

#define BOX_HISTORY_THREADS 256u

static_assert((ENGINE_HISTORY_BITS * 4u) == 64u, "box_history: a window's counts fill one 64 bit word, a nibble a bit");
static_assert(ENGINE_HISTORY_WINDOW <= 15u, "box_history: a window's count must fit a nibble");

struct BoxHistoryExtent
{
    unsigned int depth;
    unsigned int height;
    unsigned int width;
};

__device__ static unsigned long long box_history_key_cell(const BoxHistoryExtent *box_extent, unsigned int depth_place,
                                                          unsigned int height_place, unsigned int width_place)
{
    return ((((unsigned long long)depth_place * (box_extent->height + 1u)) + height_place) *
            (unsigned long long)(box_extent->width + 1u)) +
           width_place;
}

__global__ static void box_history_row_kernel(const unsigned long long *window_counts, BoxHistoryExtent box_extent,
                                              unsigned int bit, unsigned int *key)
{
    const unsigned int line = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (line >= (box_extent.depth * box_extent.height))
    {
        return;
    }
    const unsigned int depth_place = line / box_extent.height;
    const unsigned int height_place = line % box_extent.height;
    const unsigned long long first = (unsigned long long)line * box_extent.width;
    unsigned int running = 0u;
    for (unsigned int width_place = 0u; width_place < box_extent.width; width_place += 1u)
    {
        running += (unsigned int)((window_counts[first + width_place] >> (4u * bit)) & 0xFULL);
        key[box_history_key_cell(&box_extent, depth_place + 1u, height_place + 1u, width_place + 1u)] = running;
    }
}

__global__ static void box_history_column_kernel(BoxHistoryExtent box_extent, unsigned int axis, unsigned int *key)
{
    const unsigned int line = (blockIdx.x * blockDim.x) + threadIdx.x;
    const unsigned int across = (axis == 0u) ? box_extent.height : box_extent.depth;
    const unsigned int extent = (axis == 0u) ? box_extent.depth : box_extent.height;
    if (line >= (across * box_extent.width))
    {
        return;
    }
    const unsigned int across_place = (line / box_extent.width) + 1u;
    const unsigned int width_place = (line % box_extent.width) + 1u;
    unsigned int running = 0u;
    for (unsigned int place = 1u; place <= extent; place += 1u)
    {
        const unsigned long long cell = (axis == 0u)
                                            ? box_history_key_cell(&box_extent, place, across_place, width_place)
                                            : box_history_key_cell(&box_extent, across_place, place, width_place);
        running += key[cell];
        key[cell] = running;
    }
}

__global__ static void box_history_gather_kernel(const unsigned int *key, BoxHistoryExtent box_extent,
                                                 const unsigned int *extents, unsigned int bodies, unsigned int windows,
                                                 unsigned int window, unsigned int bit, unsigned int *counts)
{
    const unsigned int body = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (body >= bodies)
    {
        return;
    }
    const unsigned int *const extent = &extents[BOX_HISTORY_EXTENT_FIELDS * body];
    const unsigned int empty_box =
        (unsigned int)((extent[0] > extent[3]) || (extent[1] > extent[4]) || (extent[2] > extent[5]));
    const unsigned int low_depth = (empty_box != 0u) ? 0u : extent[0];
    const unsigned int low_height = (empty_box != 0u) ? 0u : extent[1];
    const unsigned int low_width = (empty_box != 0u) ? 0u : extent[2];
    const unsigned int high_depth = (empty_box != 0u) ? 0u : (extent[3] + 1u);
    const unsigned int high_height = (empty_box != 0u) ? 0u : (extent[4] + 1u);
    const unsigned int high_width = (empty_box != 0u) ? 0u : (extent[5] + 1u);
    const unsigned int box_count = key[box_history_key_cell(&box_extent, high_depth, high_height, high_width)] -
                                   key[box_history_key_cell(&box_extent, low_depth, high_height, high_width)] -
                                   key[box_history_key_cell(&box_extent, high_depth, low_height, high_width)] -
                                   key[box_history_key_cell(&box_extent, high_depth, high_height, low_width)] +
                                   key[box_history_key_cell(&box_extent, low_depth, low_height, high_width)] +
                                   key[box_history_key_cell(&box_extent, low_depth, high_height, low_width)] +
                                   key[box_history_key_cell(&box_extent, high_depth, low_height, low_width)] -
                                   key[box_history_key_cell(&box_extent, low_depth, low_height, low_width)];
    counts[((((unsigned long long)body * windows) + window) * ENGINE_HISTORY_BITS) + bit] = box_count;
}

__global__ static void box_history_walk_kernel(const unsigned long long *window_counts, BoxHistoryExtent box_extent,
                                               const unsigned int *extents, unsigned int bodies, unsigned int windows,
                                               unsigned int window, const unsigned int *counts,
                                               unsigned long long *disagreements)
{
    const unsigned int body = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (body >= bodies)
    {
        return;
    }
    const unsigned int *const extent = &extents[BOX_HISTORY_EXTENT_FIELDS * body];
    unsigned int walked_counts[ENGINE_HISTORY_BITS];
    for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
    {
        walked_counts[bit] = 0u;
    }
    for (unsigned int depth_place = extent[0]; depth_place <= extent[3]; depth_place += 1u)
    {
        for (unsigned int height_place = extent[1]; height_place <= extent[4]; height_place += 1u)
        {
            const unsigned long long first =
                (((unsigned long long)depth_place * box_extent.height) + height_place) * box_extent.width;
            for (unsigned int width_place = extent[2]; width_place <= extent[5]; width_place += 1u)
            {
                const unsigned long long word = window_counts[first + width_place];
                for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
                {
                    walked_counts[bit] += (unsigned int)((word >> (4u * bit)) & 0xFULL);
                }
            }
        }
    }
    const unsigned int *const box_counts =
        &counts[(((unsigned long long)body * windows) + window) * ENGINE_HISTORY_BITS];
    unsigned int disagreeing_bits = 0u;
    for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
    {
        disagreeing_bits += (walked_counts[bit] != box_counts[bit]) ? 1u : 0u;
    }
    if (disagreeing_bits != 0u)
    {
        atomicAdd(disagreements, (unsigned long long)disagreeing_bits);
    }
}

extern "C" long box_history_gather(const BoxHistoryRequest *request)
{
    if ((request == NULL) || (request->history == NULL) || (request->history->history == NULL) ||
        (request->extents == NULL) || (request->counts == NULL) || (request->disagreements == NULL) ||
        (request->microseconds == NULL) || (request->history->extent[1] > 0xFFFFFULL) ||
        (request->history->extent[2] > 0xFFFFFULL) || (request->history->extent[3] > 0xFFFFFULL))
    {
        return BOX_HISTORY_ERROR;
    }
    const unsigned long long start_microseconds = engine_clock_microseconds();
    const unsigned long long voxels =
        request->history->extent[1] * request->history->extent[2] * request->history->extent[3];
    if (voxels > (0xFFFFFFFFULL / 15ULL))
    {
        return BOX_HISTORY_ERROR;
    }
    const BoxHistoryExtent box_extent = {(unsigned int)request->history->extent[1],
                                         (unsigned int)request->history->extent[2],
                                         (unsigned int)request->history->extent[3]};
    const unsigned int windows = (unsigned int)request->history->windows;
    const size_t key_cells = (size_t)(box_extent.depth + 1u) * (box_extent.height + 1u) * (box_extent.width + 1u);
    const size_t count_entries = (size_t)request->bodies * windows * ENGINE_HISTORY_BITS;
    const unsigned long long window_bytes = voxels * sizeof(unsigned long long);
    const unsigned long long key_bytes = (unsigned long long)key_cells * sizeof(unsigned int);
    // The extents cross in. The window, the key, the counts and the count of the disagreements are temporary cells of
    // the pool of the device. Every byte is 0 at the take. The call gives them back last first before it returns
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, (unsigned int)platform_place_g_engine_owner);
    void *cell[5] = {NULL, NULL, NULL, NULL, NULL};
    int steps_succeeded =
        tessera_job_cells_in(&cells, request->extents,
                             (unsigned long long)request->bodies * BOX_HISTORY_EXTENT_FIELDS * sizeof(unsigned int),
                             &cell[0]) &&
        tessera_job_cells_room(&cells, window_bytes, &cell[1]) && tessera_job_cells_room(&cells, key_bytes, &cell[2]) &&
        tessera_job_cells_room(&cells, ((unsigned long long)count_entries + 1ULL) * sizeof(unsigned int), &cell[3]) &&
        tessera_job_cells_room(&cells, sizeof(unsigned long long), &cell[4]);
    // Explicit casts read each cell as the words that it holds
    const unsigned int *const device_extents = (const unsigned int *)cell[0];
    unsigned long long *const device_window_counts = (unsigned long long *)cell[1];
    unsigned int *const device_key = (unsigned int *)cell[2];
    unsigned int *const device_counts = (unsigned int *)cell[3];
    unsigned long long *const device_disagreements = (unsigned long long *)cell[4];
    const unsigned int rows = box_extent.depth * box_extent.height;
    const unsigned int height_columns = box_extent.depth * box_extent.width;
    const unsigned int depth_columns = box_extent.height * box_extent.width;
    const unsigned int body_blocks = (request->bodies + BOX_HISTORY_THREADS - 1u) / BOX_HISTORY_THREADS;
    for (unsigned int window = 0u; steps_succeeded && (window < windows); window += 1u)
    {
        steps_succeeded = tessera_job_crossed_into(device_window_counts,
                                                   &request->history->history[(size_t)window * voxels], window_bytes);
        for (unsigned int bit = 0u; steps_succeeded && (bit < ENGINE_HISTORY_BITS); bit += 1u)
        {
            steps_succeeded = tessera_job_cleared(device_key, key_bytes);
            if (steps_succeeded)
            {
                box_history_row_kernel<<<(rows + BOX_HISTORY_THREADS - 1u) / BOX_HISTORY_THREADS,
                                         BOX_HISTORY_THREADS>>>(device_window_counts, box_extent, bit, device_key);
                box_history_column_kernel<<<(height_columns + BOX_HISTORY_THREADS - 1u) / BOX_HISTORY_THREADS,
                                            BOX_HISTORY_THREADS>>>(box_extent, 1u, device_key);
                box_history_column_kernel<<<(depth_columns + BOX_HISTORY_THREADS - 1u) / BOX_HISTORY_THREADS,
                                            BOX_HISTORY_THREADS>>>(box_extent, 0u, device_key);
            }
            if (steps_succeeded && (request->bodies != 0u))
            {
                box_history_gather_kernel<<<body_blocks, BOX_HISTORY_THREADS>>>(
                    device_key, box_extent, device_extents, request->bodies, windows, window, bit, device_counts);
            }
            steps_succeeded = steps_succeeded && (cudaGetLastError() == cudaSuccess);
        }
        if (steps_succeeded && (request->bodies != 0u))
        {
            box_history_walk_kernel<<<body_blocks, BOX_HISTORY_THREADS>>>(device_window_counts, box_extent,
                                                                          device_extents, request->bodies, windows,
                                                                          window, device_counts, device_disagreements);
            steps_succeeded = (cudaGetLastError() == cudaSuccess);
        }
    }
    unsigned long long host_disagreements = 0ULL;
    steps_succeeded = steps_succeeded && (cudaDeviceSynchronize() == cudaSuccess) &&
                      tessera_job_crossed_out(&host_disagreements, device_disagreements, sizeof(unsigned long long)) &&
                      tessera_job_crossed_out(request->counts, device_counts,
                                              (unsigned long long)count_entries * sizeof(unsigned int));
    tessera_job_cells_given(&cells);
    if (steps_succeeded)
    {
        *request->disagreements = host_disagreements;
        *request->microseconds = engine_clock_microseconds() - start_microseconds;
    }
    return steps_succeeded ? 0L : BOX_HISTORY_ERROR;
}
