// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// max_tree_code.cu: the code kernels, levels and connected components
#include "max_tree_device_internal.h"

__global__ void max_tree_iota_kernel(unsigned int voxels, unsigned int *order)
{
    const unsigned int place = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (place >= voxels)
    {
        return;
    }
    order[place] = place;
}

__global__ void max_tree_code_gather_kernel(const unsigned int *residual, unsigned int limb_count,
                                            const unsigned int *order, unsigned int voxels, unsigned int limb,
                                            unsigned int *keys)
{
    const unsigned int place = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (place >= voxels)
    {
        return;
    }
    const unsigned int voxel = order[place];
    keys[place] = residual[((size_t)voxel * limb_count) + limb] * max_tree_selected(residual, voxel, limb_count);
}

__global__ void max_tree_code_flags_kernel(const unsigned int *residual, unsigned int limb_count,
                                           const unsigned int *order, unsigned int voxels, unsigned int *flags)
{
    const unsigned int place = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (place >= voxels)
    {
        return;
    }
    const unsigned int voxel = order[place];
    const unsigned int before = order[place - (unsigned int)(place > 0u)];
    const unsigned int one = max_tree_selected(residual, voxel, limb_count);
    const unsigned int other = max_tree_selected(residual, before, limb_count);
    unsigned int differs = one ^ other;
    for (unsigned int limb = 0u; limb < limb_count; limb += 1u)
    {
        differs |= one & other &
                   (unsigned int)(residual[((size_t)voxel * limb_count) + limb] !=
                                  residual[((size_t)before * limb_count) + limb]);
    }
    flags[place] = differs;
}

__global__ void max_tree_code_scatter_kernel(const unsigned int *residual, unsigned int limb_count,
                                             const unsigned int *order, const unsigned int *ranks, unsigned int voxels,
                                             unsigned int *code)
{
    const unsigned int place = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (place >= voxels)
    {
        return;
    }
    const unsigned int voxel = order[place];
    code[voxel] = (ranks[place] + 1u) * max_tree_selected(residual, voxel, limb_count);
}

__device__ static void max_tree_code_faces_voxel(const unsigned int *code, unsigned int depth, unsigned int height,
                                                 unsigned int width, unsigned char *faces, unsigned int voxel)
{
    const unsigned int plane = height * width;
    const unsigned int z = voxel / plane;
    const unsigned int y = (voxel % plane) / width;
    const unsigned int x = voxel % width;
    const unsigned int inside[3] = {(unsigned int)((z + 1u) < depth), (unsigned int)((y + 1u) < height),
                                    (unsigned int)((x + 1u) < width)};
    const unsigned int stride[3] = {plane, width, 1u};
    const unsigned int mine = code[voxel];
    unsigned int flags = 0u;
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        const unsigned int beside = voxel + (stride[axis] * inside[axis]);
        const unsigned int theirs = code[beside];
        const unsigned int binds = inside[axis] & (unsigned int)(mine != 0u) & (unsigned int)(theirs != 0u);
        flags |= (binds << axis) | ((binds & (unsigned int)(theirs < mine)) << (axis + 3u));
    }
    faces[voxel] = (unsigned char)flags;
}

__device__ static void max_tree_code_start_voxel(unsigned int *belongs, unsigned long long *best, unsigned char *bound,
                                                 unsigned int voxel)
{
    belongs[voxel] = voxel;
    best[voxel] = 0ULL;
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        bound[((size_t)voxel * 3u) + axis] = 0u;
    }
}

__device__ static void max_tree_code_choose_voxel(const unsigned int *code, const unsigned char *faces,
                                                  const unsigned int *belongs, unsigned int height, unsigned int width,
                                                  unsigned long long *best, unsigned int voxel)
{
    const unsigned int flags = faces[voxel];
    const unsigned int plane = height * width;
    const unsigned int stride[3] = {plane, width, 1u};
    const unsigned int one = belongs[voxel];
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        const unsigned int binds = (flags >> axis) & 1u;
        const unsigned int beside = voxel + (stride[axis] * binds);
        const unsigned int other = belongs[beside];
        if (other == one)
        {
            continue;
        }
        const unsigned int weaker = (((flags >> (axis + 3u)) & 1u) != 0u) ? beside : voxel;
        const unsigned int name = (voxel * 3u) + axis;
        const unsigned long long key = ((unsigned long long)code[weaker] << 32u) | (unsigned long long)(~name);
        if (key > best[one])
        {
            atomicMax(&best[one], key);
        }
        if (key > best[other])
        {
            atomicMax(&best[other], key);
        }
    }
}

__device__ static void max_tree_code_partner_voxel(const unsigned long long *best, const unsigned int *belongs,
                                                   unsigned int height, unsigned int width, unsigned int *partner,
                                                   unsigned int voxel)
{
    const unsigned long long key = best[voxel];
    if ((belongs[voxel] != voxel) || (key == 0ULL))
    {
        partner[voxel] = voxel;
        return;
    }
    const unsigned int name = ~(unsigned int)(key & 0xFFFFFFFFULL);
    const unsigned int lower = name / 3u;
    const unsigned int plane = height * width;
    const unsigned int stride[3] = {plane, width, 1u};
    const unsigned int one = belongs[lower];
    const unsigned int other = belongs[lower + stride[name % 3u]];
    partner[voxel] = (one == voxel) ? other : one;
}

__device__ static void max_tree_code_hook_voxel(unsigned long long *best, const unsigned int *partner,
                                                unsigned int tick, unsigned int *belongs, unsigned char *bound,
                                                unsigned int *moved, unsigned int *last, unsigned int voxel)
{
    const unsigned long long key = best[voxel];
    best[voxel] = 0ULL;
    const unsigned int other = partner[voxel];
    const unsigned int mutual = (unsigned int)(partner[other] == voxel);
    const unsigned int joins = (unsigned int)(other != voxel) & ((mutual ^ 1u) | (unsigned int)(voxel > other));
    if (joins == 0u)
    {
        return;
    }
    belongs[voxel] = other;
    bound[~(unsigned int)(key & 0xFFFFFFFFULL)] = 1u;
    moved[0] = 1u;
    last[0] = tick + 1u;
}

// The arguments of a contraction over the codes, which the job kernel takes by value
struct MaxTreeCodeContractJob
{
    const unsigned int *code;
    unsigned int depth;
    unsigned int height;
    unsigned int width;
    unsigned char *faces;
    unsigned int *belongs;
    unsigned long long *best;
    unsigned int *partner;
    unsigned char *bound;
    unsigned int *moved;
};

// The contraction over the codes as one job of the device, as max_tree_contract_job_kernel runs it over the residual.
// The choice reads one key of the code of a voxel in place of the words of the residual
__global__ void max_tree_code_contract_job_kernel(MaxTreeCodeContractJob job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    const unsigned int count = job.depth * job.height * job.width;
    for (unsigned long long voxel = first; voxel < count; voxel += jump)
    {
        // Explicit casts take a voxel below the count, which an unsigned int holds
        max_tree_code_faces_voxel(job.code, job.depth, job.height, job.width, job.faces, (unsigned int)voxel);
        max_tree_code_start_voxel(job.belongs, job.best, job.bound, (unsigned int)voxel);
    }
    int settled = 0;
    for (unsigned int round = 0u; settled == 0; round += 1u)
    {
        if (first == 0ULL)
        {
            job.moved[0] = 0u;
        }
        grid.sync();
        for (unsigned int tick = 0u; tick < MAX_TREE_TICKS; tick += 1u)
        {
            for (unsigned int step = 0u; step < MAX_TREE_JUMPS; step += 1u)
            {
                for (unsigned long long voxel = first; voxel < count; voxel += jump)
                {
                    max_tree_jump_voxel(job.belongs, (unsigned int)voxel);
                }
                grid.sync();
            }
            for (unsigned long long voxel = first; voxel < count; voxel += jump)
            {
                max_tree_flatten_voxel(job.belongs, (unsigned int)voxel);
            }
            grid.sync();
            for (unsigned long long voxel = first; voxel < count; voxel += jump)
            {
                max_tree_code_choose_voxel(job.code, job.faces, job.belongs, job.height, job.width, job.best,
                                           (unsigned int)voxel);
            }
            grid.sync();
            for (unsigned long long voxel = first; voxel < count; voxel += jump)
            {
                max_tree_code_partner_voxel(job.best, job.belongs, job.height, job.width, job.partner,
                                            (unsigned int)voxel);
            }
            grid.sync();
            for (unsigned long long voxel = first; voxel < count; voxel += jump)
            {
                max_tree_code_hook_voxel(job.best, job.partner, (round * MAX_TREE_TICKS) + tick, job.belongs, job.bound,
                                         job.moved, &job.moved[2], (unsigned int)voxel);
            }
            grid.sync();
        }
        settled = (job.moved[0] == 0u) ? 1 : 0;
        // each thread reads the mark before the first thread clears it for the next round
        grid.sync();
    }
}

int max_tree_code_contract(const unsigned int *code, unsigned int depth, unsigned int height, unsigned int width,
                           unsigned char *faces, unsigned int *belongs, unsigned long long *best, unsigned int *partner,
                           unsigned char *bound, unsigned int *moved, EngineError *error)
{
    MaxTreeCodeContractJob job = {code, depth, height, width, faces, belongs, best, partner, bound, moved};
    void *args[1] = {(void *)&job};
    // Explicit cast gives the address of the record of the job as the one argument of the kernel
    return MAX_TREE_CHECK(tessera_job_ran((const void *)max_tree_code_contract_job_kernel, MAX_TREE_BLOCK, args), bound,
                          error, ENGINE_ERROR_RESOURCE);
}

__global__ void max_tree_faces_differ_kernel(const unsigned char *one, const unsigned char *other, unsigned int faces,
                                             unsigned int *differ)
{
    const unsigned int face = (blockIdx.x * blockDim.x) + threadIdx.x;
    if ((face < faces) && (one[face] != other[face]))
    {
        atomicAdd(differ, 1u);
    }
}

__global__ void max_tree_code_levels_kernel(const unsigned int *code, const unsigned char *bound, unsigned int depth,
                                            unsigned int height, unsigned int width, int *change)
{
    const unsigned int voxels = depth * height * width;
    const unsigned int voxel = (blockIdx.x * blockDim.x) + threadIdx.x;
    if ((voxel >= voxels) || (code[voxel] == 0u))
    {
        return;
    }
    const unsigned int mine = code[voxel];
    atomicAdd(&change[mine], 1);
    const unsigned int plane = height * width;
    const unsigned int stride[3] = {plane, width, 1u};
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        if (bound[((size_t)voxel * 3u) + axis] != 0u)
        {
            const unsigned int theirs = code[voxel + stride[axis]];
            atomicSub(&change[(theirs < mine) ? theirs : mine], 1);
        }
    }
}

__global__ void max_tree_reverse_kernel(const int *change, unsigned int top, int *reversed)
{
    const unsigned int place = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (place >= top)
    {
        return;
    }
    reversed[place] = change[top - place];
}

__global__ void max_tree_level_pick_kernel(const int *counts, unsigned int top, const int *max, unsigned int *level)
{
    const unsigned int place = (blockIdx.x * blockDim.x) + threadIdx.x;
    if ((place < top) && (counts[place] == max[0]))
    {
        atomicMin(level, top - place);
    }
}

__global__ void max_tree_cc_start_kernel(const unsigned int *code, unsigned int voxels, unsigned int level,
                                         unsigned char *ranges, unsigned int *parent)
{
    const unsigned int voxel = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (voxel >= voxels)
    {
        return;
    }
    const unsigned int reached = (unsigned int)(code[voxel] >= level);
    ranges[voxel] = (unsigned char)reached;
    parent[voxel] = (reached != 0u) ? voxel : MAX_TREE_ABSENT;
}

__device__ static unsigned int max_tree_cc_root(const unsigned int *parent, unsigned int voxel)
{
    unsigned int at = voxel;
    while (parent[at] != at)
    {
        at = parent[at];
    }
    return at;
}

// One voxel of the hook. Each neighbor past it on an axis that lies in the range hooks the higher of their two roots
// under the lower. The hook marks `moved` where it does
__device__ static void max_tree_cc_hooked(const unsigned char *ranges, unsigned int depth, unsigned int height,
                                          unsigned int width, unsigned int *parent, unsigned int *moved,
                                          unsigned int voxel)
{
    const unsigned int plane = height * width;
    const unsigned int z = voxel / plane;
    const unsigned int y = (voxel % plane) / width;
    const unsigned int x = voxel % width;
    const unsigned int inside[3] = {(unsigned int)((z + 1u) < depth), (unsigned int)((y + 1u) < height),
                                    (unsigned int)((x + 1u) < width)};
    const unsigned int stride[3] = {plane, width, 1u};
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        const unsigned int beside = voxel + (stride[axis] * inside[axis]);
        if ((inside[axis] & (unsigned int)ranges[beside]) == 0u)
        {
            continue;
        }
        const unsigned int mine = max_tree_cc_root(parent, voxel);
        const unsigned int theirs = max_tree_cc_root(parent, beside);
        if (mine == theirs)
        {
            continue;
        }
        const unsigned int high = (mine > theirs) ? mine : theirs;
        const unsigned int low = (mine > theirs) ? theirs : mine;
        atomicMin(&parent[high], low);
        moved[0] = 1u;
    }
}

// The steps of the components as walks of their own: the voxels from `first` on, `jump` apart. The hook hooks each
// voxel in the range, and the jump halves the path of each voxel to its root. The flatten points each voxel at its root
__device__ static void max_tree_cc_hook_span(const unsigned char *ranges, unsigned int depth, unsigned int height,
                                             unsigned int width, unsigned int *parent, unsigned int *moved,
                                             unsigned long long first, unsigned long long jump)
{
    const unsigned long long voxels = (unsigned long long)depth * height * width;
    for (unsigned long long voxel = first; voxel < voxels; voxel += jump)
    {
        if (ranges[voxel] != 0u)
        {
            // Explicit cast takes a voxel below the count, which max_tree_objects holds below 2^32 / 3
            max_tree_cc_hooked(ranges, depth, height, width, parent, moved, (unsigned int)voxel);
        }
    }
}

__device__ static void max_tree_cc_jump_span(const unsigned char *ranges, unsigned int voxels, unsigned int *parent,
                                             unsigned long long first, unsigned long long jump)
{
    for (unsigned long long voxel = first; voxel < voxels; voxel += jump)
    {
        if (ranges[voxel] != 0u)
        {
            parent[voxel] = parent[parent[voxel]];
        }
    }
}

__device__ static void max_tree_cc_flatten_span(const unsigned char *ranges, unsigned int voxels, unsigned int *parent,
                                                unsigned long long first, unsigned long long jump)
{
    for (unsigned long long voxel = first; voxel < voxels; voxel += jump)
    {
        if (ranges[voxel] != 0u)
        {
            // Explicit cast takes a voxel below the count, which an unsigned int holds
            parent[voxel] = max_tree_cc_root(parent, (unsigned int)voxel);
        }
    }
}

// The components of the range as one job of the device. Each round clears `moved`, then runs MAX_TREE_TICKS ticks of
// a hook and MAX_TREE_JUMPS jumps, with a sync of the grid after each step. The grid reads `moved` after the round, and
// a round that hooked nothing ends the loop. The flatten then points each voxel at its root. The root of a component
// is its least voxel, whatever the order of the hooks
__global__ void max_tree_cc_job_kernel(const unsigned char *ranges, unsigned int depth, unsigned int height,
                                       unsigned int width, unsigned int *parent, unsigned int *moved)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    const unsigned int voxels = depth * height * width;
    int settled = 0;
    while (settled == 0)
    {
        if (first == 0ULL)
        {
            moved[0] = 0u;
        }
        grid.sync();
        for (unsigned int tick = 0u; tick < MAX_TREE_TICKS; tick += 1u)
        {
            max_tree_cc_hook_span(ranges, depth, height, width, parent, moved, first, jump);
            grid.sync();
            for (unsigned int step = 0u; step < MAX_TREE_JUMPS; step += 1u)
            {
                max_tree_cc_jump_span(ranges, voxels, parent, first, jump);
                grid.sync();
            }
        }
        settled = (moved[0] == 0u) ? 1 : 0;
        // each thread reads the mark before the first thread clears it for the next round
        grid.sync();
    }
    max_tree_cc_flatten_span(ranges, voxels, parent, first, jump);
}
