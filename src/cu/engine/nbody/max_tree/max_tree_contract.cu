// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// max_tree_contract.cu: contraction (max_tree_contract) and its steps
#include "max_tree_device_internal.h"

__device__ static void max_tree_faces_voxel(const unsigned int *residual, unsigned int depth, unsigned int height,
                                            unsigned int width, unsigned char *faces, unsigned int voxel)
{
    const unsigned int plane = height * width;
    const unsigned int z = voxel / plane;
    const unsigned int y = (voxel % plane) / width;
    const unsigned int x = voxel % width;
    const unsigned int inside[3] = {(unsigned int)((z + 1u) < depth), (unsigned int)((y + 1u) < height),
                                    (unsigned int)((x + 1u) < width)};
    const unsigned int stride[3] = {plane, width, 1u};
    const unsigned int here = max_tree_selected(residual, voxel);
    unsigned int flags = 0u;
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        const unsigned int beside = voxel + (stride[axis] * inside[axis]);
        const unsigned int binds = inside[axis] & here & max_tree_selected(residual, beside);
        const unsigned int far_weaker = max_tree_below(residual, beside, voxel);
        flags |= (binds << axis) | ((binds & far_weaker) << (axis + 3u));
    }
    faces[voxel] = (unsigned char)flags;
}

__device__ static void max_tree_start_voxel(unsigned int *belongs, unsigned long long *strongest, unsigned char *bound,
                                            unsigned int voxel)
{
    belongs[voxel] = voxel;
    for (unsigned int word = 0u; word < MAX_TREE_KEY_WORDS; word += 1u)
    {
        strongest[((size_t)voxel * MAX_TREE_KEY_WORDS) + word] = 0ULL;
    }
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        bound[((size_t)voxel * 3u) + axis] = 0u;
    }
}

__device__ void max_tree_jump_voxel(unsigned int *belongs, unsigned int voxel)
{
    belongs[voxel] = belongs[belongs[voxel]];
}

__device__ void max_tree_flatten_voxel(unsigned int *belongs, unsigned int voxel)
{
    unsigned int at = belongs[voxel];
    while (belongs[at] != at)
    {
        at = belongs[at];
    }
    belongs[voxel] = at;
}

__device__ static void max_tree_choose_voxel(const unsigned int *residual, const unsigned char *faces,
                                             const unsigned int *belongs, unsigned int height, unsigned int width,
                                             unsigned int word, unsigned long long *strongest, unsigned int voxel)
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
        const unsigned long long mine = max_tree_key_word(residual, weaker, name, word);
        if (max_tree_leads(residual, weaker, name, word, &strongest[(size_t)one * MAX_TREE_KEY_WORDS]) != 0u)
        {
            atomicMax(&strongest[((size_t)one * MAX_TREE_KEY_WORDS) + word], mine);
        }
        if (max_tree_leads(residual, weaker, name, word, &strongest[(size_t)other * MAX_TREE_KEY_WORDS]) != 0u)
        {
            atomicMax(&strongest[((size_t)other * MAX_TREE_KEY_WORDS) + word], mine);
        }
    }
}

__device__ static void max_tree_partner_voxel(const unsigned long long *strongest, const unsigned int *belongs,
                                              unsigned int height, unsigned int width, unsigned int *partner,
                                              unsigned int voxel)
{
    const unsigned long long low = strongest[(size_t)voxel * MAX_TREE_KEY_WORDS];
    const unsigned int chose = (unsigned int)((belongs[voxel] == voxel) && (low != 0ULL));
    if (chose == 0u)
    {
        partner[voxel] = voxel;
        return;
    }
    const unsigned int name = ~(unsigned int)(low & 0xFFFFFFFFULL);
    const unsigned int lower = name / 3u;
    const unsigned int axis = name % 3u;
    const unsigned int plane = height * width;
    const unsigned int stride[3] = {plane, width, 1u};
    const unsigned int one = belongs[lower];
    const unsigned int other = belongs[lower + stride[axis]];
    partner[voxel] = (one == voxel) ? other : one;
}

__device__ static void max_tree_hook_voxel(unsigned long long *strongest, const unsigned int *partner,
                                           unsigned int tick, unsigned int *belongs, unsigned char *bound,
                                           unsigned int *moved, unsigned int *last, unsigned int voxel)
{
    const unsigned long long low = strongest[(size_t)voxel * MAX_TREE_KEY_WORDS];
    for (unsigned int word = 0u; word < MAX_TREE_KEY_WORDS; word += 1u)
    {
        strongest[((size_t)voxel * MAX_TREE_KEY_WORDS) + word] = 0ULL;
    }
    const unsigned int other = partner[voxel];
    const unsigned int mutual = (unsigned int)(partner[other] == voxel);
    const unsigned int joins = (unsigned int)(other != voxel) & ((mutual ^ 1u) | (unsigned int)(voxel > other));
    if (joins == 0u)
    {
        return;
    }
    belongs[voxel] = other;
    bound[~(unsigned int)(low & 0xFFFFFFFFULL)] = 1u;
    moved[0] = 1u;
    last[0] = tick + 1u;
}

// The arguments of a contraction, which the job kernel takes by value
struct MaxTreeContractJob
{
    const unsigned int *residual;
    unsigned int depth;
    unsigned int height;
    unsigned int width;
    unsigned char *faces;
    unsigned int *belongs;
    unsigned long long *strongest;
    unsigned int *partner;
    unsigned char *bound;
    unsigned int *moved;
};

// The contraction as one job of the device. The grid marks the faces and starts each voxel. Each round clears the mark,
// then runs MAX_TREE_TICKS ticks. A tick runs the jumps, the flatten, the choice of each key word, the partners and the
// hooks. A sync of the grid follows each step.
//
// The tick of a hook counts across the rounds. The grid reads the mark after the round, and a round that joined nothing
// ends the loop. moved[2] keeps the last tick that joined, plus 1
__global__ void max_tree_contract_job_kernel(MaxTreeContractJob job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    const unsigned int count = job.depth * job.height * job.width;
    for (unsigned long long voxel = first; voxel < count; voxel += jump)
    {
        // Explicit casts take a voxel below the count, which an unsigned int holds
        max_tree_faces_voxel(job.residual, job.depth, job.height, job.width, job.faces, (unsigned int)voxel);
        max_tree_start_voxel(job.belongs, job.strongest, job.bound, (unsigned int)voxel);
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
            for (unsigned int word = MAX_TREE_KEY_WORDS; word > 0u; word -= 1u)
            {
                for (unsigned long long voxel = first; voxel < count; voxel += jump)
                {
                    max_tree_choose_voxel(job.residual, job.faces, job.belongs, job.height, job.width, word - 1u,
                                          job.strongest, (unsigned int)voxel);
                }
                grid.sync();
            }
            for (unsigned long long voxel = first; voxel < count; voxel += jump)
            {
                max_tree_partner_voxel(job.strongest, job.belongs, job.height, job.width, job.partner,
                                       (unsigned int)voxel);
            }
            grid.sync();
            for (unsigned long long voxel = first; voxel < count; voxel += jump)
            {
                max_tree_hook_voxel(job.strongest, job.partner, (round * MAX_TREE_TICKS) + tick, job.belongs, job.bound,
                                    job.moved, &job.moved[2], (unsigned int)voxel);
            }
            grid.sync();
        }
        settled = (job.moved[0] == 0u) ? 1 : 0;
        // each thread reads the mark before the first thread clears it for the next round
        grid.sync();
    }
}

int max_tree_contract(const unsigned int *residual, unsigned int depth, unsigned int height, unsigned int width,
                      unsigned char *faces, unsigned int *belongs, unsigned long long *strongest, unsigned int *partner,
                      unsigned char *bound, unsigned int *moved, EngineError *error)
{
    MaxTreeContractJob job = {residual, depth, height, width, faces, belongs, strongest, partner, bound, moved};
    void *args[1] = {(void *)&job};
    // Explicit cast gives the address of the record of the job as the one argument of the kernel
    return MAX_TREE_CHECK(tessera_job_ran((const void *)max_tree_contract_job_kernel, MAX_TREE_BLOCK, args), bound,
                          error, ENGINE_ERROR_RESOURCE);
}
