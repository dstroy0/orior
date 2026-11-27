// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// max_tree_grade.cu: grading the codes
#include "../../../../platform/platform.h"
#include "max_tree_device_internal.h"

static int max_tree_grade_codes(const unsigned int *residual, unsigned int depth, unsigned int height,
                                unsigned int width, const unsigned char *coded, unsigned int *differ,
                                EngineError *error)
{
    const unsigned long long voxels = (unsigned long long)depth * height * width;
    // The scratch of the grade, temporary cells of the pool of the device, every byte 0 at the take. The call gives
    // them back last first before it returns
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, MAX_TREE_PLACE);
    void *room[7] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const unsigned long long bytes[7] = {voxels,
                                         voxels * sizeof(unsigned int),
                                         voxels * MAX_TREE_KEY_WORDS * sizeof(unsigned long long),
                                         voxels * sizeof(unsigned int),
                                         voxels * 3ULL,
                                         3ULL * sizeof(unsigned int),
                                         sizeof(unsigned int)};
    int ok = 1;
    for (unsigned int at = 0u; (ok != 0) && (at < 7u); at += 1u)
    {
        ok = MAX_TREE_CHECK(tessera_job_cells_room(&cells, bytes[at], &room[at]), coded, error, ENGINE_ERROR_RESOURCE);
    }
    // Explicit casts read each cell as the words that it holds
    unsigned char *const faces = (unsigned char *)room[0];
    unsigned int *const belongs = (unsigned int *)room[1];
    unsigned long long *const strongest = (unsigned long long *)room[2];
    unsigned int *const partner = (unsigned int *)room[3];
    unsigned char *const bound = (unsigned char *)room[4];
    unsigned int *const moved = (unsigned int *)room[5];
    unsigned int *const device_differ = (unsigned int *)room[6];
    ok = ok &&
         max_tree_contract(residual, depth, height, width, faces, belongs, strongest, partner, bound, moved, error);
    const unsigned int face_count = (unsigned int)(voxels * 3u);
    if (ok != 0)
    {
        max_tree_faces_differ_kernel<<<(face_count + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK, MAX_TREE_BLOCK>>>(
            bound, coded, face_count, device_differ);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), device_differ, error);
    }
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_out(differ, device_differ, sizeof(unsigned int)), differ, error,
                              ENGINE_ERROR_RESOURCE);
    ok = ok && MAX_TREE_CHECK(*differ == 0u, coded, error, ENGINE_ERROR_LOGIC);
    tessera_job_cells_given(&cells);
    return ok;
}

extern "C" long max_tree_objects(const MaxTreeObjectsRequest *request)
{
    if (g_max_tree_profile < 0)
    {
        char profile_text[PLATFORM_ENVIRONMENT_BYTES];
        g_max_tree_profile =
            (platform_environment("MAX_TREE_PROFILE", profile_text, sizeof(profile_text)) != 0ULL) ? 1 : 0;
    }
    unsigned long long stage_mark = engine_clock_microseconds();
    if ((request == NULL) || (request->error == NULL))
    {
        return MAX_TREE_ERROR;
    }
    EngineError *const error = request->error;
    const unsigned int depth = request->depth;
    const unsigned int height = request->height;
    const unsigned int width = request->width;
    const size_t voxels = (size_t)depth * height * width;
    const unsigned int limb_count = (request->limbs != 0u) ? request->limbs : ENGINE_RESIDUAL_LIMBS;
    const int whole = (limb_count == ENGINE_RESIDUAL_LIMBS) ? 1 : 0;
    const int valid =
        MAX_TREE_CHECK(request->device_residual != NULL, &request->device_residual, error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((limb_count <= ENGINE_RESIDUAL_LIMBS) &&
                           ((whole != 0) || ((request->grade == 0u) && (g_max_tree_resident.retaining_frames == 0))),
                       &request->limbs, error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((request->capacity == 0u) || (request->bodies != NULL), &request->bodies, error,
                       ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((voxels != 0u) && ((voxels * 3u) < 0xFFFFFFFFULL), &request->depth, error, ENGINE_ERROR_REQUEST);
    if (valid == 0)
    {
        return MAX_TREE_ERROR;
    }
    MaxTreeResident *const resident = &g_max_tree_resident;
    int ok = max_tree_reserve(voxels, error);
    const unsigned int count = (unsigned int)voxels;
    const int items = (int)count;
    const unsigned int spread = (count + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK;
    const unsigned int chunks = (count + MAX_TREE_CHUNK - 1u) / MAX_TREE_CHUNK;
    const unsigned int chunk_spread = (chunks + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK;
    const unsigned int words = (count + 63u) / 64u;
    const unsigned int *const residual = request->device_residual;

    cub::DoubleBuffer<unsigned int> keys(resident->keys[0], resident->keys[1]);
    cub::DoubleBuffer<unsigned int> order(resident->order[0], resident->order[1]);
    if (ok != 0)
    {
        max_tree_iota_kernel<<<spread, MAX_TREE_BLOCK>>>(count, order.Current());
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), order.Current(), error);
    }
    for (unsigned int limb = 0u; (ok != 0) && (limb < limb_count); limb += 1u)
    {
        max_tree_code_gather_kernel<<<spread, MAX_TREE_BLOCK>>>(residual, limb_count, order.Current(), count, limb,
                                                                keys.Current());
        size_t bytes = resident->scratch_bytes;
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), keys.Current(), error) &&
             MAX_TREE_STATUS_CHECK(cub::DeviceRadixSort::SortPairs(resident->scratch, bytes, keys, order, items),
                                   resident->scratch, error);
    }
    unsigned int *const flags = keys.Current();
    unsigned int *const ranks = keys.Alternate();
    if (ok != 0)
    {
        max_tree_code_flags_kernel<<<spread, MAX_TREE_BLOCK>>>(residual, limb_count, order.Current(), count, flags);
        size_t bytes = resident->scratch_bytes;
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), flags, error) &&
             MAX_TREE_STATUS_CHECK(cub::DeviceScan::InclusiveSum(resident->scratch, bytes, flags, ranks, items), ranks,
                                   error);
    }
    if (ok != 0)
    {
        max_tree_code_scatter_kernel<<<spread, MAX_TREE_BLOCK>>>(residual, limb_count, order.Current(), ranks, count,
                                                                 resident->code);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->code, error);
    }
    unsigned int top = 0u;
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_out(&top, &ranks[count - 1u], sizeof(unsigned int)),
                              &ranks[count - 1u], error, ENGINE_ERROR_RESOURCE);
    top += 1u;
    max_tree_stage(0u, &stage_mark);

    ok = ok && MAX_TREE_CHECK(tessera_job_cleared(resident->moved, 3ULL * sizeof(unsigned int)), resident->moved, error,
                              ENGINE_ERROR_RESOURCE);
    ok = ok && max_tree_code_contract(resident->code, depth, height, width, resident->faces, resident->belongs,
                                      resident->best, resident->partner, resident->bound, resident->moved, error);
    max_tree_stage(1u, &stage_mark);
    unsigned int faces_differ = 0u;
    ok = ok && ((request->grade == 0u) ||
                max_tree_grade_codes(residual, depth, height, width, resident->bound, &faces_differ, error));
    max_tree_stage(8u, &stage_mark);

    ok = ok && MAX_TREE_CHECK(tessera_job_cleared(resident->change, ((unsigned long long)top + 1ULL) * sizeof(int)),
                              resident->change, error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        max_tree_code_levels_kernel<<<spread, MAX_TREE_BLOCK>>>(resident->code, resident->bound, depth, height, width,
                                                                resident->change);
        max_tree_reverse_kernel<<<(top + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK, MAX_TREE_BLOCK>>>(resident->change, top,
                                                                                                  resident->counts);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->counts, error);
    }
    if (ok != 0)
    {
        size_t bytes = resident->scratch_bytes;
        ok = MAX_TREE_STATUS_CHECK(
            cub::DeviceScan::InclusiveSum(resident->scratch, bytes, resident->counts, resident->counts, (int)top),
            resident->counts, error);
        bytes = resident->scratch_bytes;
        ok = ok && MAX_TREE_STATUS_CHECK(
                       cub::DeviceReduce::Max(resident->scratch, bytes, resident->counts, resident->max, (int)top),
                       resident->max, error);
    }
    // the level starts at all ones, the mark of no level that the pick finds
    const unsigned int no_level = 0xFFFFFFFFu;
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_into(resident->level, &no_level, sizeof(no_level)), resident->level,
                              error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        max_tree_level_pick_kernel<<<(top + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK, MAX_TREE_BLOCK>>>(
            resident->counts, top, resident->max, resident->level);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->level, error);
    }
    int expected = 0;
    unsigned int level_code = 0u;
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_out(&expected, resident->max, sizeof(int)) &&
                                  tessera_job_crossed_out(&level_code, resident->level, sizeof(unsigned int)),
                              resident->level, error, ENGINE_ERROR_RESOURCE);
    level_code = ((top > 1u) && (level_code != 0xFFFFFFFFu)) ? level_code : 1u;
    max_tree_stage(2u, &stage_mark);

    if (ok != 0)
    {
        max_tree_cc_start_kernel<<<spread, MAX_TREE_BLOCK>>>(resident->code, count, level_code, resident->ranges,
                                                             resident->label);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->label, error);
    }
    ok = ok && max_tree_cc(resident->ranges, depth, height, width, resident->label, resident->moved, error);
    unsigned int found = 0u;
    ok = ok && MAX_TREE_CHECK(tessera_job_cleared(resident->roots, sizeof(unsigned int)), resident->roots, error,
                              ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        max_tree_roots_kernel<<<spread, MAX_TREE_BLOCK>>>(resident->ranges, resident->label, count, resident->roots);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->roots, error);
    }
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_out(&found, resident->roots, sizeof(unsigned int)), resident->roots,
                              error, ENGINE_ERROR_RESOURCE);
    max_tree_stage(3u, &stage_mark);

    ok = ok && MAX_TREE_CHECK(tessera_job_cleared(resident->best, voxels * sizeof(unsigned long long)), resident->best,
                              error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        max_tree_peak_kernel<<<spread, MAX_TREE_BLOCK>>>(resident->code, resident->ranges, resident->label, count,
                                                         resident->best);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->best, error);
    }
    max_tree_stage(4u, &stage_mark);

    if (ok != 0)
    {
        max_tree_mark_kernel<<<chunk_spread, MAX_TREE_BLOCK>>>(residual, limb_count, resident->code, resident->ranges,
                                                               resident->label, resident->best, count, 0u,
                                                               resident->at_chunk, NULL, NULL);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->at_chunk, error);
    }
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_out(resident->host_chunks, resident->at_chunk,
                                                      (unsigned long long)chunks * sizeof(unsigned int)),
                              resident->host_chunks, error, ENGINE_ERROR_RESOURCE);
    unsigned long long bodies = 0ULL;
    for (unsigned int chunk = 0u; (ok != 0) && (chunk < chunks); chunk += 1u)
    {
        const unsigned int here = resident->host_chunks[chunk];
        resident->host_chunks[chunk] = (unsigned int)bodies;
        bodies += (unsigned long long)here;
    }
    ok = ok && max_tree_grow_bodies((size_t)bodies, error);
    ok = ok && MAX_TREE_CHECK(tessera_job_crossed_into(resident->at_chunk, resident->host_chunks,
                                                       (unsigned long long)chunks * sizeof(unsigned int)) &&
                                  tessera_job_cleared(resident->bodies, (bodies + 1ULL) * sizeof(EngineBody)),
                              resident->bodies, error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        max_tree_mark_kernel<<<chunk_spread, MAX_TREE_BLOCK>>>(residual, limb_count, resident->code, resident->ranges,
                                                               resident->label, resident->best, count, 1u,
                                                               resident->at_chunk, resident->partner, resident->bodies);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->bodies, error);
    }
    max_tree_stage(5u, &stage_mark);
    if (ok != 0)
    {
        max_tree_object_census_kernel<<<spread, MAX_TREE_BLOCK>>>(resident->ranges, resident->label, resident->best,
                                                                  resident->partner, depth, height, width,
                                                                  resident->bodies, resident->belongs);
        if (request->positive_words != NULL)
        {
            max_tree_pack_kernel<<<(words + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK, MAX_TREE_BLOCK>>>(
                residual, limb_count, count, words, resident->packed);
        }
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->bodies, error);
    }
    max_tree_stage(6u, &stage_mark);
    ok = ok && MAX_TREE_CHECK(((long long)found == (long long)expected) && ((unsigned long long)found == bodies),
                              resident->roots, error, ENGINE_ERROR_LOGIC);
    ok = ok && MAX_TREE_CHECK((request->bodies == NULL) || (bodies <= (unsigned long long)request->capacity),
                              &request->capacity, error, ENGINE_ERROR_REQUEST);
    ok = ok && ((request->labels == NULL) || MAX_TREE_CHECK(tessera_job_crossed_out(request->labels, resident->belongs,
                                                                                    voxels * sizeof(unsigned int)),
                                                            request->labels, error, ENGINE_ERROR_RESOURCE));
    ok = ok && ((request->positive_words == NULL) ||
                MAX_TREE_CHECK(tessera_job_crossed_out(request->positive_words, resident->packed,
                                                       (unsigned long long)words * sizeof(unsigned long long)),
                               request->positive_words, error, ENGINE_ERROR_RESOURCE));
    ok = ok && ((request->bodies == NULL) || (bodies == 0ULL) ||
                MAX_TREE_CHECK(tessera_job_crossed_out(request->bodies, resident->bodies, bodies * sizeof(EngineBody)),
                               request->bodies, error, ENGINE_ERROR_RESOURCE));
    max_tree_stage(7u, &stage_mark);
    const unsigned int frame_place = resident->frame_current ^ 1u;
    if ((ok != 0) && (resident->retaining_frames != 0))
    {
        resident->frame_ready[frame_place] = 0u;
        ok = MAX_TREE_CHECK(
            tessera_job_copied(resident->frame_code[frame_place], resident->code, voxels * sizeof(unsigned int)) &&
                tessera_job_cleared(resident->frame_values[frame_place],
                                    ((unsigned long long)top + 1ULL) * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int)),
            resident->frame_code[frame_place], error, ENGINE_ERROR_RESOURCE);
    }
    if ((ok != 0) && (resident->retaining_frames != 0))
    {
        max_tree_code_values_kernel<<<spread, MAX_TREE_BLOCK>>>(residual, order.Current(), flags, resident->code, count,
                                                                resident->frame_values[frame_place]);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), resident->frame_values[frame_place], error);
        resident->frame_top[frame_place] = top;
        resident->frame_ready[frame_place] = (unsigned int)ok;
        resident->frame_current = frame_place;
    }
    if (request->level_code != NULL)
    {
        *request->level_code = level_code;
    }
    if (request->faces_differ != NULL)
    {
        *request->faces_differ = faces_differ;
    }
    if (request->proof_count != NULL)
    {
        *request->proof_count = (unsigned int)(ok != 0);
    }
    if (ok == 0)
    {
        max_tree_release_resident(resident);
        return MAX_TREE_ERROR;
    }
    resident->last_bodies = (size_t)bodies;
    resident->top = top;
    resident->threshold = level_code;
    resident->last_depth = depth;
    resident->last_height = height;
    resident->last_width = width;
    return (long)bodies;
}
