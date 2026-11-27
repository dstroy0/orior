// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// max_tree_select.cu: admission, gather, count and scatter
#include "../../../../platform/platform.h"
#include "max_tree_device_internal.h"

__global__ static void max_tree_select_kernel(const unsigned int *residual, unsigned int voxels, unsigned int *admits)
{
    const unsigned int voxel = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (voxel >= voxels)
    {
        return;
    }
    const unsigned int *const limbs = &residual[(size_t)voxel * ENGINE_RESIDUAL_LIMBS];
    const unsigned int negative = (limbs[ENGINE_RESIDUAL_LIMBS - 1u] >> 31u) & 1u;
    unsigned int any = 0u;
    for (unsigned int limb = 0u; limb < ENGINE_RESIDUAL_LIMBS; limb += 1u)
    {
        any |= limbs[limb];
    }
    admits[voxel] = (unsigned int)((negative == 0u) && (any != 0u));
}

__global__ static void max_tree_gather_kernel(const unsigned int *admits, const unsigned int *offsets,
                                              unsigned int voxels, unsigned int *order)
{
    const unsigned int voxel = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (voxel >= voxels)
    {
        return;
    }
    if (admits[voxel] != 0u)
    {
        order[offsets[voxel]] = voxel;
    }
}

__global__ static void max_tree_count_kernel(const unsigned int *residual, const unsigned int *order,
                                             unsigned int selected_count, unsigned int at, unsigned int *counts)
{
    __shared__ unsigned int mine[MAX_TREE_BUCKETS];
    for (unsigned int bucket = threadIdx.x; bucket < MAX_TREE_BUCKETS; bucket += blockDim.x)
    {
        mine[bucket] = 0u;
    }
    __syncthreads();
    const unsigned int first = blockIdx.x * blockDim.x;
    const unsigned int index = first + threadIdx.x;
    if (index < selected_count)
    {
        const unsigned int voxel = order[index];
        const unsigned int limb = residual[((size_t)voxel * ENGINE_RESIDUAL_LIMBS) + (at / 4u)];
        const unsigned int byte = (~(limb >> ((at % 4u) * 8u))) & 0xFFu;
        atomicAdd(&mine[byte], 1u);
    }
    __syncthreads();
    for (unsigned int bucket = threadIdx.x; bucket < MAX_TREE_BUCKETS; bucket += blockDim.x)
    {
        counts[((size_t)blockIdx.x * MAX_TREE_BUCKETS) + bucket] = mine[bucket];
    }
}

__global__ static void max_tree_scatter_kernel(const unsigned int *residual, const unsigned int *order,
                                               unsigned int selected_count, unsigned int at, const unsigned int *places,
                                               unsigned int *sorted)
{
    const unsigned int block = (blockIdx.x * blockDim.x) + threadIdx.x;
    const unsigned int first = block * MAX_TREE_BLOCK;
    if (first >= selected_count)
    {
        return;
    }
    unsigned int running[MAX_TREE_BUCKETS];
    for (unsigned int bucket = 0u; bucket < MAX_TREE_BUCKETS; bucket += 1u)
    {
        running[bucket] = places[((size_t)block * MAX_TREE_BUCKETS) + bucket];
    }
    const unsigned int end = ((first + MAX_TREE_BLOCK) < selected_count) ? (first + MAX_TREE_BLOCK) : selected_count;
    for (unsigned int index = first; index < end; index += 1u)
    {
        const unsigned int voxel = order[index];
        const unsigned int limb = residual[((size_t)voxel * ENGINE_RESIDUAL_LIMBS) + (at / 4u)];
        const unsigned int byte = (~(limb >> ((at % 4u) * 8u))) & 0xFFu;
        sorted[running[byte]] = voxel;
        running[byte] += 1u;
    }
}

extern "C" int max_tree_order_agrees(const unsigned int *residual, unsigned int depth, unsigned int height,
                                     unsigned int width, EngineError *error)
{
    if (error == NULL)
    {
        return 0;
    }
    MaxTree host;
    const long admitted = max_tree_build(residual, depth, height, width, &host);
    if (MAX_TREE_CHECK(admitted >= 0L, residual, error, ENGINE_ERROR_REQUEST) == 0)
    {
        return 0;
    }
    const size_t voxels = (size_t)depth * height * width;
    const unsigned int selected_count = host.admitted;
    const unsigned int blocks = (unsigned int)((selected_count + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK);
    const unsigned int spread = (unsigned int)((voxels + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK);
    const unsigned long long lanes = (unsigned long long)voxels;
    const unsigned long long bucket_bytes = (unsigned long long)blocks * MAX_TREE_BUCKETS * sizeof(unsigned int);
    // The residual crosses in, and the scratch of the check is temporary cells of the pool of the device, every byte 0
    // at the take. The call gives them back last first before it returns
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, MAX_TREE_PLACE);
    void *cell = NULL;
    int ok = MAX_TREE_CHECK(
        tessera_job_cells_in(&cells, residual, lanes * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int), &cell), residual,
        error, ENGINE_ERROR_RESOURCE);
    // Explicit casts read each cell as the words that it holds
    const unsigned int *const device_residual = (const unsigned int *)cell;
    void *room[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
    const unsigned long long bytes[6] = {lanes * sizeof(unsigned int),
                                         lanes * sizeof(unsigned int),
                                         ((unsigned long long)selected_count + 1ULL) * sizeof(unsigned int),
                                         ((unsigned long long)selected_count + 1ULL) * sizeof(unsigned int),
                                         bucket_bytes,
                                         bucket_bytes};
    for (unsigned int at = 0u; (ok != 0) && (at < 6u); at += 1u)
    {
        ok = MAX_TREE_CHECK(tessera_job_cells_room(&cells, bytes[at], &room[at]), residual, error,
                            ENGINE_ERROR_RESOURCE);
    }
    unsigned int *const device_admits = (unsigned int *)room[0];
    unsigned int *const device_offsets = (unsigned int *)room[1];
    unsigned int *device_order = (unsigned int *)room[2];
    unsigned int *device_sorted = (unsigned int *)room[3];
    unsigned int *const device_counts = (unsigned int *)room[4];
    unsigned int *const device_places = (unsigned int *)room[5];
    if (ok != 0)
    {
        max_tree_select_kernel<<<spread, MAX_TREE_BLOCK>>>(device_residual, (unsigned int)voxels, device_admits);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), device_admits, error);
    }
    unsigned int *const admits = (unsigned int *)platform_cell_taken(
        (unsigned long long)(voxels * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const offsets = (unsigned int *)platform_cell_taken(
        (unsigned long long)(voxels * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    ok = ok && MAX_TREE_CHECK(admits != NULL, &admits, error, ENGINE_ERROR_RESOURCE) &&
         MAX_TREE_CHECK(offsets != NULL, &offsets, error, ENGINE_ERROR_RESOURCE);
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_out(admits, device_admits, lanes * sizeof(unsigned int)), admits,
                              error, ENGINE_ERROR_RESOURCE);
    unsigned int running = 0u;
    for (size_t voxel = 0u; (ok != 0) && (voxel < voxels); voxel += 1u)
    {
        offsets[voxel] = running;
        running += admits[voxel];
    }
    ok = ok && MAX_TREE_CHECK(running == selected_count, admits, error, ENGINE_ERROR_LOGIC);
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_into(device_offsets, offsets, lanes * sizeof(unsigned int)),
                              device_offsets, error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        max_tree_gather_kernel<<<spread, MAX_TREE_BLOCK>>>(device_admits, device_offsets, (unsigned int)voxels,
                                                           device_order);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), device_order, error);
    }
    unsigned int *const counts = (unsigned int *)platform_cell_taken(
        (unsigned long long)((size_t)blocks * MAX_TREE_BUCKETS * sizeof(unsigned int)),
        (unsigned int)platform_place_g_engine_owner, 0);
    ok = ok && MAX_TREE_CHECK(counts != NULL, &counts, error, ENGINE_ERROR_RESOURCE);
    for (unsigned int at = 0u; (ok != 0) && (at < MAX_TREE_KEY_BYTES); at += 1u)
    {
        max_tree_count_kernel<<<blocks, MAX_TREE_BLOCK>>>(device_residual, device_order, selected_count, at,
                                                          device_counts);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), device_counts, error);
        ok = ok && MAX_TREE_CHECK(tessera_job_crossed_out(counts, device_counts, bucket_bytes), counts, error,
                                  ENGINE_ERROR_RESOURCE);
        unsigned int place = 0u;
        for (unsigned int bucket = 0u; (ok != 0) && (bucket < MAX_TREE_BUCKETS); bucket += 1u)
        {
            for (unsigned int block = 0u; block < blocks; block += 1u)
            {
                const size_t index = ((size_t)block * MAX_TREE_BUCKETS) + bucket;
                const unsigned int many = counts[index];
                counts[index] = place;
                place += many;
            }
        }
        ok = ok && MAX_TREE_CHECK(place == selected_count, counts, error, ENGINE_ERROR_LOGIC);
        ok = ok && MAX_TREE_CHECK(tessera_job_crossed_into(device_places, counts, bucket_bytes), device_places, error,
                                  ENGINE_ERROR_RESOURCE);
        if (ok != 0)
        {
            const unsigned int walkers = (unsigned int)((blocks + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK);
            max_tree_scatter_kernel<<<walkers, MAX_TREE_BLOCK>>>(device_residual, device_order, selected_count, at,
                                                                 device_places, device_sorted);
            ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), device_sorted, error);
        }
        unsigned int *const swap = device_order;
        device_order = device_sorted;
        device_sorted = swap;
    }
    unsigned int *const theirs =
        (unsigned int *)platform_cell_taken((unsigned long long)(((size_t)selected_count + 1u) * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, 0);
    ok = ok && MAX_TREE_CHECK(theirs != NULL, &theirs, error, ENGINE_ERROR_RESOURCE);
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_out(theirs, device_order,
                                                      (unsigned long long)selected_count * sizeof(unsigned int)),
                              theirs, error, ENGINE_ERROR_RESOURCE);
    int agrees = ok;
    for (unsigned int at = 0u; (agrees != 0) && (at < selected_count); at += 1u)
    {
        agrees = MAX_TREE_CHECK(theirs[at] == host.order[at], &theirs[at], error, ENGINE_ERROR_LOGIC);
    }
    platform_cell_released(admits, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(offsets, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(counts, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(theirs, (unsigned int)platform_place_g_engine_owner);
    tessera_job_cells_given(&cells);
    max_tree_release(&host);
    return agrees;
}
