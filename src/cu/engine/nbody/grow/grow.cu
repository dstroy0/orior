// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../../../../includes/platform/platform.h"
#include "grow.h"
#include "memoria_operor/memoria_operor.h"

#define GROW_CHECK(condition_, evacaddr_, error_, type_)                                                               \
    engine_error_check((condition_), (type_), ENGINE_MODULE_GROW, (unsigned int)__LINE__, (const void *)(evacaddr_),   \
                       (error_))

extern "C" long grow_leaves(const EngineBody *bodies, unsigned int count, EngineLeaves *leaves, EngineError *error)
{
    if (error == NULL)
    {
        return GROW_ERROR;
    }
    if (!GROW_CHECK((leaves != NULL) && ((bodies != NULL) || (count == 0u)), bodies, error, ENGINE_ERROR_REQUEST))
    {
        return GROW_ERROR;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = leaves, .bytes = sizeof(*leaves), .val = 0);
    const size_t capacity = (size_t)count + 1u;
    leaves->peaks =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    leaves->sizes =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    leaves->sums = (unsigned long long *)platform_cell_taken(capacity * 3u * sizeof(unsigned long long),
                                                             platform_place_g_program_owner, 0);
    leaves->moments = (unsigned long long *)platform_cell_taken(capacity * 6u * sizeof(unsigned long long),
                                                                platform_place_g_program_owner, 0);
    leaves->touches =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    leaves->joined = (unsigned int *)platform_cell_taken(2u * sizeof(unsigned int), platform_place_g_program_owner, 0);
    if (!GROW_CHECK((leaves->peaks != NULL) && (leaves->sizes != NULL) && (leaves->sums != NULL) &&
                        (leaves->moments != NULL) && (leaves->touches != NULL) && (leaves->joined != NULL),
                    leaves, error, ENGINE_ERROR_RESOURCE))
    {
        platform_cell_released(leaves->peaks, platform_place_g_program_owner);
        platform_cell_released(leaves->sizes, platform_place_g_program_owner);
        platform_cell_released(leaves->sums, platform_place_g_program_owner);
        platform_cell_released(leaves->moments, platform_place_g_program_owner);
        platform_cell_released(leaves->touches, platform_place_g_program_owner);
        platform_cell_released(leaves->joined, platform_place_g_program_owner);
        EMBED_CALL(memor.set, MemoriaCfg, .dst = leaves, .bytes = sizeof(*leaves), .val = 0);
        return GROW_ERROR;
    }
    for (unsigned int leaf = 0u; leaf < count; leaf += 1u)
    {
        const EngineBody *const body = &bodies[leaf];
        leaves->peaks[leaf] = body->peak;
        leaves->sizes[leaf] = body->mass;
        leaves->touches[leaf] = body->touches;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &leaves->sums[(size_t)leaf * 3u], .src = body->sums,
                   .bytes = 3u * sizeof(unsigned long long));
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &leaves->moments[(size_t)leaf * 6u], .src = body->moments,
                   .bytes = 6u * sizeof(unsigned long long));
    }
    leaves->leaf_count = count;
    leaves->joined_count = 0u;
    return (long)count;
}

extern "C" void grow_group_voxels(const EngineGroupRequest *request)
{
    const unsigned int voxels = request->voxels;
    const unsigned int *const labels = request->labels;
    int *const leaf_at_peak = request->leaf_at_peak;
    unsigned int *const start = request->start;
    for (unsigned int leaf = 0u; leaf < request->leaf_count; leaf += 1u)
    {
        leaf_at_peak[request->peaks[leaf]] = (int)leaf;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = start, .bytes = ((size_t)request->leaf_count + 2u) * sizeof(unsigned int),
               .val = 0);
    // each pass reads its first run at voxel 0, and an empty volume reads no label
    unsigned int run_label = 0u;
    int run_leaf = -1;
    for (unsigned int voxel = 0u; voxel < voxels; voxel += 1u)
    {
        if ((voxel == 0u) || (labels[voxel] != run_label))
        {
            run_label = labels[voxel];
            run_leaf = leaf_at_peak[run_label];
        }
        if (run_leaf >= 0)
        {
            start[(unsigned int)run_leaf + 1u] += 1u;
        }
    }
    for (unsigned int leaf = 0u; leaf < request->leaf_count; leaf += 1u)
    {
        start[leaf + 1u] += start[leaf];
    }
    for (unsigned int voxel = 0u; voxel < voxels; voxel += 1u)
    {
        if ((voxel == 0u) || (labels[voxel] != run_label))
        {
            run_label = labels[voxel];
            run_leaf = leaf_at_peak[run_label];
        }
        if (run_leaf >= 0)
        {
            request->grouped[start[(unsigned int)run_leaf]] = voxel;
            start[(unsigned int)run_leaf] += 1u;
        }
    }
    for (unsigned int leaf = request->leaf_count; leaf > 0u; leaf -= 1u)
    {
        start[leaf] = start[leaf - 1u];
    }
    start[0] = 0u;
    for (unsigned int leaf = 0u; leaf < request->leaf_count; leaf += 1u)
    {
        leaf_at_peak[request->peaks[leaf]] = -1;
    }
}
