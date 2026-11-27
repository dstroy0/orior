// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// max_tree_label.cu: labeling
#include "../../../../platform/platform.h"
#include "max_tree_device_internal.h"

__global__ static void max_tree_ranges_kernel(const unsigned int *residual, unsigned int voxels, unsigned int level,
                                              unsigned char *ranges)
{
    const unsigned int voxel = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (voxel >= voxels)
    {
        return;
    }
    ranges[voxel] = (unsigned char)(max_tree_selected(residual, voxel) & (max_tree_below(residual, voxel, level) ^ 1u));
}

__device__ static void max_tree_spread_voxel(const unsigned char *ranges, const unsigned char *bound,
                                             unsigned int every, unsigned int depth, unsigned int height,
                                             unsigned int width, unsigned int *label, unsigned int *moved,
                                             unsigned int voxel)
{
    const unsigned int plane = height * width;
    const unsigned int z = voxel / plane;
    const unsigned int y = (voxel % plane) / width;
    const unsigned int x = voxel % width;
    const unsigned int inside[3] = {(unsigned int)((z + 1u) < depth), (unsigned int)((y + 1u) < height),
                                    (unsigned int)((x + 1u) < width)};
    const unsigned int stride[3] = {plane, width, 1u};
    const unsigned int here = ranges[voxel];
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        const unsigned int beside = voxel + (stride[axis] * inside[axis]);
        const unsigned int crosses = inside[axis] & here & (unsigned int)ranges[beside] &
                                     (every | (unsigned int)bound[((size_t)voxel * 3u) + axis]);
        if (crosses == 0u)
        {
            continue;
        }
        const unsigned int mine = label[voxel];
        const unsigned int theirs = label[beside];
        if (mine == theirs)
        {
            continue;
        }
        const unsigned int was_mine = atomicMin(&label[voxel], theirs);
        const unsigned int was_theirs = atomicMin(&label[beside], mine);
        if ((theirs < was_mine) || (mine < was_theirs))
        {
            moved[0] = 1u;
        }
    }
}

__device__ static void max_tree_label_jump_voxel(unsigned int *label, unsigned int voxel)
{
    const unsigned int target = label[voxel];
    if (target != MAX_TREE_ABSENT)
    {
        label[voxel] = label[target];
    }
}

__global__ static void max_tree_differ_kernel(const unsigned int *across_every, const unsigned int *across_chosen,
                                              unsigned int voxels, unsigned int *differ)
{
    const unsigned int voxel = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (voxel >= voxels)
    {
        return;
    }
    if (across_every[voxel] != across_chosen[voxel])
    {
        atomicAdd(differ, 1u);
    }
}

// The arguments of a labeling, which the job kernel takes by value
struct MaxTreeLabelJob
{
    const unsigned char *ranges;
    const unsigned char *bound;
    unsigned int every;
    unsigned int depth;
    unsigned int height;
    unsigned int width;
    unsigned int *label;
    unsigned int *moved;
};

// The labels of the range as one job of the device. Each voxel in the range starts as its own label. Each round clears
// the mark, then runs MAX_TREE_TICKS ticks of a spread across the faces and MAX_TREE_JUMPS jumps. A sync of the grid
// follows each step. With `every` set, the spread crosses every face, and else only the bound faces. The grid reads the
// mark after the round, and a round that moved no label ends the loop
__global__ void max_tree_label_job_kernel(MaxTreeLabelJob job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    const unsigned int voxels = job.depth * job.height * job.width;
    for (unsigned long long voxel = first; voxel < voxels; voxel += jump)
    {
        // Explicit cast takes a voxel below the count, which an unsigned int holds
        job.label[voxel] = (job.ranges[voxel] != 0u) ? (unsigned int)voxel : MAX_TREE_ABSENT;
    }
    int settled = 0;
    while (settled == 0)
    {
        if (first == 0ULL)
        {
            job.moved[0] = 0u;
        }
        grid.sync();
        for (unsigned int tick = 0u; tick < MAX_TREE_TICKS; tick += 1u)
        {
            for (unsigned long long voxel = first; voxel < voxels; voxel += jump)
            {
                max_tree_spread_voxel(job.ranges, job.bound, job.every, job.depth, job.height, job.width, job.label,
                                      job.moved, (unsigned int)voxel);
            }
            grid.sync();
            for (unsigned int step = 0u; step < MAX_TREE_JUMPS; step += 1u)
            {
                for (unsigned long long voxel = first; voxel < voxels; voxel += jump)
                {
                    max_tree_label_jump_voxel(job.label, (unsigned int)voxel);
                }
                grid.sync();
            }
        }
        settled = (job.moved[0] == 0u) ? 1 : 0;
        // each thread reads the mark before the first thread clears it for the next round
        grid.sync();
    }
}

static int max_tree_label(const unsigned char *ranges, const unsigned char *bound, unsigned int every,
                          unsigned int depth, unsigned int height, unsigned int width, unsigned int *label,
                          unsigned int *moved, EngineError *error)
{
    MaxTreeLabelJob job = {ranges, bound, every, depth, height, width, label, moved};
    void *args[1] = {(void *)&job};
    // Explicit cast gives the address of the record of the job as the one argument of the kernel
    return MAX_TREE_CHECK(tessera_job_ran((const void *)max_tree_label_job_kernel, MAX_TREE_BLOCK, args), label, error,
                          ENGINE_ERROR_RESOURCE);
}

extern "C" long max_tree_bind(const MaxTreeBindRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return MAX_TREE_ERROR;
    }
    EngineError *const error = request->error;
    const size_t voxels = (size_t)request->depth * request->height * request->width;
    const int valid =
        MAX_TREE_CHECK(request->residual != NULL, &request->residual, error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK(request->bound != NULL, &request->bound, error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((request->level_count == 0u) || (request->levels != NULL), &request->levels, error,
                       ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((voxels != 0u) && ((voxels * 3u) < 0xFFFFFFFFULL), &request->depth, error, ENGINE_ERROR_REQUEST);
    if (valid == 0)
    {
        return MAX_TREE_ERROR;
    }
    const unsigned int depth = request->depth;
    const unsigned int height = request->height;
    const unsigned int width = request->width;
    const unsigned int proved_at = (request->level_count != 0u) ? request->level_count : 1u;
    const unsigned long long lanes = (unsigned long long)voxels;
    // The residual crosses in, and the scratch of the bind is temporary cells of the pool of the device, every byte 0
    // at the take. The call gives them back last first before it returns
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, MAX_TREE_PLACE);
    void *cell = NULL;
    int ok = MAX_TREE_CHECK(
        tessera_job_cells_in(&cells, request->residual, lanes * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int), &cell),
        &request->residual, error, ENGINE_ERROR_RESOURCE);
    // Explicit casts read each cell as the words that it holds
    const unsigned int *const device_residual = (const unsigned int *)cell;
    void *room[9] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const unsigned long long bytes[9] = {lanes,
                                         lanes * sizeof(unsigned int),
                                         lanes * MAX_TREE_KEY_WORDS * sizeof(unsigned long long),
                                         lanes * sizeof(unsigned int),
                                         lanes * 3ULL,
                                         3ULL * sizeof(unsigned int),
                                         (unsigned long long)proved_at * sizeof(unsigned int),
                                         (request->level_count != 0u) ? lanes : 0ULL,
                                         (request->level_count != 0u) ? (2ULL * lanes * sizeof(unsigned int)) : 0ULL};
    for (unsigned int at = 0u; (ok != 0) && (at < 9u); at += 1u)
    {
        ok =
            MAX_TREE_CHECK(tessera_job_cells_room(&cells, bytes[at], &room[at]), request, error, ENGINE_ERROR_RESOURCE);
    }
    unsigned char *const device_faces = (unsigned char *)room[0];
    unsigned int *const device_belongs = (unsigned int *)room[1];
    unsigned long long *const device_strongest = (unsigned long long *)room[2];
    unsigned int *const device_partner = (unsigned int *)room[3];
    unsigned char *const device_bound = (unsigned char *)room[4];
    unsigned int *const device_moved = (unsigned int *)room[5];
    unsigned int *const device_differ = (unsigned int *)room[6];
    unsigned char *const device_ranges = (unsigned char *)room[7];
    // the labels across every face, then the labels across the chosen faces, in one cell
    unsigned int *const device_across_every = (unsigned int *)room[8];
    unsigned int *const device_across_chosen = (device_across_every != NULL) ? &device_across_every[voxels] : NULL;
    const unsigned long long began = engine_clock_microseconds();
    const unsigned int count = (unsigned int)voxels;
    const unsigned int spread = (count + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK;
    ok = ok && max_tree_contract(device_residual, depth, height, width, device_faces, device_belongs, device_strongest,
                                 device_partner, device_bound, device_moved, error);
    const unsigned long long bound_at = engine_clock_microseconds();

    for (unsigned int taken = 0u; (ok != 0) && (taken < request->level_count); taken += 1u)
    {
        max_tree_ranges_kernel<<<spread, MAX_TREE_BLOCK>>>(device_residual, count, request->levels[taken],
                                                           device_ranges);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), device_ranges, error);
        ok = ok && max_tree_label(device_ranges, device_bound, 1u, depth, height, width, device_across_every,
                                  device_moved, error);
        ok = ok && max_tree_label(device_ranges, device_bound, 0u, depth, height, width, device_across_chosen,
                                  device_moved, error);
        if (ok != 0)
        {
            max_tree_differ_kernel<<<spread, MAX_TREE_BLOCK>>>(device_across_every, device_across_chosen, count,
                                                               &device_differ[taken]);
            ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), &device_differ[taken], error);
        }
    }
    ok = ok && MAX_TREE_STATUS_CHECK(cudaDeviceSynchronize(), device_differ, error);
    const unsigned long long proved_at_end = engine_clock_microseconds();

    unsigned int last = 0u;
    unsigned int *const differ = (unsigned int *)platform_cell_taken(
        (unsigned long long)(proved_at * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    ok = ok && MAX_TREE_CHECK(differ != NULL, &differ, error, ENGINE_ERROR_RESOURCE) &&
         MAX_TREE_CHECK(
             tessera_job_crossed_out(request->bound, device_bound, lanes * 3ULL) &&
                 tessera_job_crossed_out(&last, &device_moved[2], sizeof(unsigned int)) &&
                 tessera_job_crossed_out(differ, device_differ, (unsigned long long)proved_at * sizeof(unsigned int)),
             request->bound, error, ENGINE_ERROR_RESOURCE);
    long chosen = MAX_TREE_ERROR;
    unsigned int levels_equal = 0u;
    if (ok != 0)
    {
        chosen = 0L;
        for (size_t face = 0u; face < (voxels * 3u); face += 1u)
        {
            chosen += (long)(request->bound[face] != 0u);
        }
        for (unsigned int taken = 0u; taken < request->level_count; taken += 1u)
        {
            levels_equal += (unsigned int)(differ[taken] == 0u);
        }
        ok = MAX_TREE_CHECK(levels_equal == request->level_count, differ, error, ENGINE_ERROR_LOGIC);
        chosen = (ok != 0) ? chosen : MAX_TREE_ERROR;
    }
    if (request->rounds != NULL)
    {
        *request->rounds = last;
    }
    if (request->levels_equal != NULL)
    {
        *request->levels_equal = levels_equal;
    }
    if (request->bound_microseconds != NULL)
    {
        *request->bound_microseconds = bound_at - began;
    }
    if (request->proved_microseconds != NULL)
    {
        *request->proved_microseconds = proved_at_end - bound_at;
    }
    platform_cell_released(differ, (unsigned int)platform_place_g_engine_owner);
    tessera_job_cells_given(&cells);
    return chosen;
}
