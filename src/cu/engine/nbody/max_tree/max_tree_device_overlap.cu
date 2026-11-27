// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// max_tree_device_overlap.cu: the overlap step
#include "max_tree_device_internal.h"
#include "memoria_operor/memoria_operor.h"

static int max_tree_overlap_step(MaxTreeOverlap *overlap, unsigned int origin, unsigned int level,
                                 MaxTreeOverlapStep *step, MaxTreeOverlapProbe *probes, unsigned char *links_present)
{
    const MaxTreeResident *const resident = &g_max_tree_resident;
    EngineError *const error = overlap->error;
    const unsigned int count = overlap->depth * overlap->height * overlap->width;
    const unsigned int spread = (count + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK;
    const unsigned int origin_place = overlap->frame_place[origin];
    if (!MAX_TREE_CHECK((level != 0u) && (level <= resident->frame_top[origin_place]), step, error,
                        ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    const unsigned int *const value = &resident->frame_values[origin_place][(size_t)level * ENGINE_RESIDUAL_LIMBS];
    unsigned int thresholds[2] = {0u, 0u};
    int ok = 1;
    for (unsigned int frame = 0u; (ok != 0) && (frame < 2u); frame += 1u)
    {
        const unsigned int place = overlap->frame_place[frame];
        const unsigned int top = resident->frame_top[place];
        thresholds[frame] = top + 1u;
        ok = MAX_TREE_CHECK(
            tessera_job_crossed_into(&overlap->thresholds[frame], &thresholds[frame], sizeof(unsigned int)),
            &overlap->thresholds[frame], error, ENGINE_ERROR_RESOURCE);
        if (ok != 0)
        {
            max_tree_overlap_threshold_kernel<<<(top + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK, MAX_TREE_BLOCK>>>(
                resident->frame_values[place], top, value, &overlap->thresholds[frame]);
            ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), &overlap->thresholds[frame], error);
        }
    }
    ok =
        ok && MAX_TREE_CHECK(tessera_job_crossed_out(thresholds, overlap->thresholds, 2ULL * sizeof(unsigned int)) &&
                                 tessera_job_cleared(overlap->tallies, MAX_TREE_OVERLAP_TALLIES * sizeof(unsigned int)),
                             overlap->tallies, error, ENGINE_ERROR_RESOURCE);
    for (unsigned int frame = 0u; (ok != 0) && (frame < 2u); frame += 1u)
    {
        max_tree_cc_start_kernel<<<spread, MAX_TREE_BLOCK>>>(resident->frame_code[overlap->frame_place[frame]], count,
                                                             thresholds[frame], overlap->ranges[frame],
                                                             overlap->label[frame]);
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), overlap->label[frame], error) &&
             max_tree_cc(overlap->ranges[frame], overlap->depth, overlap->height, overlap->width, overlap->label[frame],
                         overlap->moved, error) &&
             MAX_TREE_CHECK(
                 tessera_job_cleared(overlap->partners[frame], (unsigned long long)count * sizeof(unsigned int)) &&
                     tessera_job_cleared(overlap->backs[frame], (unsigned long long)count * sizeof(unsigned int)),
                 overlap->partners[frame], error, ENGINE_ERROR_RESOURCE);
        if (ok != 0)
        {
            max_tree_roots_kernel<<<spread, MAX_TREE_BLOCK>>>(overlap->ranges[frame], overlap->label[frame], count,
                                                              &overlap->tallies[frame]);
            ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), &overlap->tallies[frame], error);
        }
    }
    if (ok != 0)
    {
        max_tree_overlap_pairs_kernel<<<spread, MAX_TREE_BLOCK>>>(
            overlap->ranges[0], overlap->label[0], overlap->ranges[1], overlap->label[1], overlap->depth,
            overlap->height, overlap->width, overlap->lag[0], overlap->lag[1], overlap->lag[2], overlap->pairs[0]);
        size_t bytes = overlap->scratch_bytes;
        // The cut of the frame holds the voxel count below 2^32, and the sort takes it as int
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), overlap->pairs[0], error) &&
             MAX_TREE_STATUS_CHECK(cub::DeviceRadixSort::SortKeys(overlap->scratch, bytes, overlap->pairs[0],
                                                                  overlap->pairs[1], (int)count),
                                   overlap->pairs[1], error);
    }
    if (ok != 0)
    {
        max_tree_overlap_partners_kernel<<<spread, MAX_TREE_BLOCK>>>(overlap->pairs[1], count, overlap->partners[0],
                                                                     overlap->partners[1]);
        for (unsigned int frame = 0u; frame < 2u; frame += 1u)
        {
            max_tree_overlap_one_kernel<<<spread, MAX_TREE_BLOCK>>>(overlap->ranges[frame], overlap->label[frame],
                                                                    overlap->partners[frame], count,
                                                                    &overlap->tallies[2u + frame]);
        }
        max_tree_overlap_backs_kernel<<<spread, MAX_TREE_BLOCK>>>(
            overlap->pairs[1], count, overlap->partners[0], overlap->partners[1], overlap->backs[0], overlap->backs[1]);
        for (unsigned int frame = 0u; frame < 2u; frame += 1u)
        {
            max_tree_overlap_census_kernel<<<spread, MAX_TREE_BLOCK>>>(overlap->ranges[frame], overlap->label[frame],
                                                                       overlap->partners[frame], overlap->backs[frame],
                                                                       count, &overlap->tallies[4u + (3u * frame)]);
        }
        for (unsigned int frame = 0u; frame < 2u; frame += 1u)
        {
            if (overlap->probe_counts[frame] != 0u)
            {
                max_tree_overlap_probe_kernel<<<(overlap->probe_counts[frame] + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK,
                                                MAX_TREE_BLOCK>>>(
                    overlap->probe_voxels[frame], overlap->probe_counts[frame], overlap->ranges[frame],
                    overlap->label[frame], overlap->partners[frame], overlap->backs[frame], overlap->probes[frame]);
            }
        }
        if (overlap->link_count != 0u)
        {
            max_tree_overlap_link_kernel<<<(overlap->link_count + MAX_TREE_BLOCK - 1u) / MAX_TREE_BLOCK,
                                           MAX_TREE_BLOCK>>>(overlap->probe_links, overlap->link_count,
                                                             overlap->probes[0], overlap->probes[1], overlap->pairs[1],
                                                             count, overlap->links_present);
        }
        ok = MAX_TREE_STATUS_CHECK(cudaGetLastError(), overlap->tallies, error);
    }
    unsigned int tallies[MAX_TREE_OVERLAP_TALLIES] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    ok = ok &&
         MAX_TREE_CHECK(
             tessera_job_crossed_out(probes, overlap->probes[0],
                                     (unsigned long long)overlap->probe_counts[0] * sizeof(MaxTreeOverlapProbe)) &&
                 tessera_job_crossed_out(&probes[overlap->probe_counts[0]], overlap->probes[1],
                                         (unsigned long long)overlap->probe_counts[1] * sizeof(MaxTreeOverlapProbe)) &&
                 tessera_job_crossed_out(links_present, overlap->links_present, overlap->link_count) &&
                 tessera_job_crossed_out(tallies, overlap->tallies, MAX_TREE_OVERLAP_TALLIES * sizeof(unsigned int)),
             overlap->tallies, error, ENGINE_ERROR_RESOURCE);
    step->origin = origin;
    step->level = level;
    step->earlier_code = thresholds[0];
    step->later_code = thresholds[1];
    step->earlier_components = tallies[0];
    step->later_components = tallies[1];
    step->earlier_one = tallies[2];
    step->later_one = tallies[3];
    step->earlier_unpaired = tallies[4];
    step->earlier_mutual = tallies[5];
    step->earlier_forked = tallies[6];
    step->later_unpaired = tallies[7];
    step->later_mutual = tallies[8];
    step->later_forked = tallies[9];
    return ok;
}

extern "C" long max_tree_overlap(const MaxTreeOverlapRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return MAX_TREE_ERROR;
    }
    EngineError *const error = request->error;
    const MaxTreeResident *const resident = &g_max_tree_resident;
    int valid =
        MAX_TREE_CHECK((request->step_count != NULL) && (request->steps != NULL), request, error,
                       ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((resident->retaining_frames != 0) && (resident->frame_ready[0] != 0u) &&
                           (resident->frame_ready[1] != 0u) && (resident->last_depth != 0u),
                       resident, error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((request->earlier_level_count == 0u) || (request->earlier_levels != NULL),
                       &request->earlier_levels, error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((request->later_level_count == 0u) || (request->later_levels != NULL), &request->later_levels,
                       error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((request->probe_counts[0] == 0u) || (request->probe_voxels[0] != NULL),
                       &request->probe_voxels[0], error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((request->probe_counts[1] == 0u) || (request->probe_voxels[1] != NULL),
                       &request->probe_voxels[1], error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK(((request->probe_counts[0] + request->probe_counts[1]) == 0u) || (request->probes != NULL),
                       &request->probes, error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((request->link_count == 0u) ||
                           ((request->probe_links != NULL) && (request->links_present != NULL)),
                       &request->probe_links, error, ENGINE_ERROR_REQUEST) &&
        MAX_TREE_CHECK((request->earlier_level_count + request->later_level_count) <= request->step_capacity,
                       &request->step_capacity, error, ENGINE_ERROR_REQUEST);
    const size_t voxels = resident->voxels;
    for (unsigned int frame = 0u; (valid != 0) && (frame < 2u); frame += 1u)
    {
        for (unsigned int probe = 0u; (valid != 0) && (probe < request->probe_counts[frame]); probe += 1u)
        {
            valid = MAX_TREE_CHECK(request->probe_voxels[frame][probe] < voxels, &request->probe_voxels[frame][probe],
                                   error, ENGINE_ERROR_REQUEST);
        }
    }
    for (unsigned int link = 0u; (valid != 0) && (link < request->link_count); link += 1u)
    {
        valid = MAX_TREE_CHECK((request->probe_links[2u * link] < request->probe_counts[0]) &&
                                   (request->probe_links[(2u * link) + 1u] < request->probe_counts[1]),
                               &request->probe_links[2u * link], error, ENGINE_ERROR_REQUEST);
    }
    if (valid == 0)
    {
        return MAX_TREE_ERROR;
    }
    MaxTreeOverlap overlap;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &overlap, .bytes = sizeof(overlap), .val = 0);
    overlap.depth = resident->last_depth;
    overlap.height = resident->last_height;
    overlap.width = resident->last_width;
    overlap.frame_place[0] = resident->frame_current ^ 1u;
    overlap.frame_place[1] = resident->frame_current;
    overlap.link_count = request->link_count;
    overlap.error = error;
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        overlap.lag[axis] = request->lag[axis];
    }
    // Every buffer of the overlap is a temporary cell of the pool of the device, every byte 0 at the take. The call
    // gives them back last first before it returns
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, MAX_TREE_PLACE);
    void *cell = NULL;
    int ok = 1;
    for (unsigned int frame = 0u; frame < 2u; frame += 1u)
    {
        const unsigned long long probes = request->probe_counts[frame];
        overlap.probe_counts[frame] = request->probe_counts[frame];
        ok = ok && MAX_TREE_CHECK(
                       tessera_job_cells_in(&cells, request->probe_voxels[frame], probes * sizeof(unsigned int), &cell),
                       &request->probe_voxels[frame], error, ENGINE_ERROR_RESOURCE);
        // Explicit casts read each cell as the words that it holds
        overlap.probe_voxels[frame] = (const unsigned int *)cell;
        ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, probes * sizeof(MaxTreeOverlapProbe), &cell),
                                  &request->probes, error, ENGINE_ERROR_RESOURCE);
        overlap.probes[frame] = (MaxTreeOverlapProbe *)cell;
    }
    const unsigned long long link_words = (unsigned long long)request->link_count * 2ULL;
    ok = ok &&
         MAX_TREE_CHECK(tessera_job_cells_in(&cells, request->probe_links, link_words * sizeof(unsigned int), &cell),
                        &request->probe_links, error, ENGINE_ERROR_RESOURCE);
    overlap.probe_links = (const unsigned int *)cell;
    ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, request->link_count, &cell), &request->links_present,
                              error, ENGINE_ERROR_RESOURCE);
    overlap.links_present = (unsigned char *)cell;
    // The cut of the frame holds the voxel count below 2^32, and the sort takes it as int
    ok = ok && MAX_TREE_STATUS_CHECK(cub::DeviceRadixSort::SortKeys(NULL, overlap.scratch_bytes,
                                                                    (const unsigned long long *)NULL,
                                                                    (unsigned long long *)NULL, (int)voxels),
                                     &overlap.scratch_bytes, error);
    ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, overlap.scratch_bytes, &overlap.scratch), request, error,
                              ENGINE_ERROR_RESOURCE);
    ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, 2ULL * sizeof(unsigned int), &cell), request, error,
                              ENGINE_ERROR_RESOURCE);
    overlap.thresholds = (unsigned int *)cell;
    ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, MAX_TREE_OVERLAP_TALLIES * sizeof(unsigned int), &cell),
                              request, error, ENGINE_ERROR_RESOURCE);
    overlap.tallies = (unsigned int *)cell;
    ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, 3ULL * sizeof(unsigned int), &cell), request, error,
                              ENGINE_ERROR_RESOURCE);
    overlap.moved = (unsigned int *)cell;
    const unsigned long long lanes = (unsigned long long)voxels;
    for (unsigned int frame = 0u; frame < 2u; frame += 1u)
    {
        ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, lanes, &cell), request, error, ENGINE_ERROR_RESOURCE);
        overlap.ranges[frame] = (unsigned char *)cell;
        ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, lanes * sizeof(unsigned int), &cell), request, error,
                                  ENGINE_ERROR_RESOURCE);
        overlap.label[frame] = (unsigned int *)cell;
        ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, lanes * sizeof(unsigned int), &cell), request, error,
                                  ENGINE_ERROR_RESOURCE);
        overlap.partners[frame] = (unsigned int *)cell;
        ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, lanes * sizeof(unsigned int), &cell), request, error,
                                  ENGINE_ERROR_RESOURCE);
        overlap.backs[frame] = (unsigned int *)cell;
        ok = ok && MAX_TREE_CHECK(tessera_job_cells_room(&cells, lanes * sizeof(unsigned long long), &cell), request,
                                  error, ENGINE_ERROR_RESOURCE);
        overlap.pairs[frame] = (unsigned long long *)cell;
    }
    const size_t probes_a_step = (size_t)request->probe_counts[0] + request->probe_counts[1];
    unsigned int written = 0u;
    for (unsigned int taken = 0u; (ok != 0) && (taken < request->earlier_level_count); taken += 1u)
    {
        ok = max_tree_overlap_step(&overlap, 0u, request->earlier_levels[taken], &request->steps[written],
                                   &request->probes[written * probes_a_step],
                                   &request->links_present[(size_t)written * request->link_count]);
        written += 1u;
    }
    for (unsigned int taken = 0u; (ok != 0) && (taken < request->later_level_count); taken += 1u)
    {
        ok = max_tree_overlap_step(&overlap, 1u, request->later_levels[taken], &request->steps[written],
                                   &request->probes[written * probes_a_step],
                                   &request->links_present[(size_t)written * request->link_count]);
        written += 1u;
    }
    tessera_job_cells_given(&cells);
    if (ok == 0)
    {
        return MAX_TREE_ERROR;
    }
    *request->step_count = written;
    return 0L;
}
