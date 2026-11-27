// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// climb_machine_core.cu: painting, reach and the core
#include "../../../../platform/platform.h"
#include "climb_machine_internal.h"
#include "memoria_operor/memoria_operor.h"

__global__ static void machine_paint_kernel(const unsigned int *run_start, const unsigned int *run_length,
                                            const unsigned int *run_leaf, unsigned int runs, unsigned int *painted)
{
    const unsigned int run = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (run >= runs)
    {
        return;
    }
    const unsigned int start = run_start[run];
    for (unsigned int step = 0u; step < run_length[run]; step += 1u)
    {
        painted[start + step] = run_leaf[run];
    }
}

__global__ static void machine_range_row_kernel(const unsigned int *painted, MachineGeometry geometry,
                                                unsigned int *range)
{
    const unsigned int row = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (row >= (geometry.depth * geometry.height))
    {
        return;
    }
    const unsigned int row_first = row * geometry.width;
    unsigned int edge_distance = 0u;
    for (unsigned int width_place = 0u; width_place < geometry.width; width_place += 1u)
    {
        const unsigned int voxel = row_first + width_place;
        const unsigned int label_boundary = ((width_place == 0u) || (painted[voxel] != painted[voxel - 1u])) ? 1u : 0u;
        edge_distance = (label_boundary != 0u) ? 1u : (edge_distance + 1u);
        range[voxel] = edge_distance;
    }
    edge_distance = 0u;
    for (unsigned int width_end = geometry.width; width_end > 0u; width_end -= 1u)
    {
        const unsigned int voxel = row_first + width_end - 1u;
        const unsigned int label_boundary =
            ((width_end == geometry.width) || (painted[voxel] != painted[voxel + 1u])) ? 1u : 0u;
        edge_distance = (label_boundary != 0u) ? 1u : (edge_distance + 1u);
        range[voxel] = (edge_distance < range[voxel]) ? edge_distance : range[voxel];
    }
}

__global__ static void machine_range_column_kernel(const unsigned int *painted, MachineGeometry geometry,
                                                   unsigned int axis, const unsigned int *range_in,
                                                   unsigned int *range_out)
{
    const unsigned int column = (blockIdx.x * blockDim.x) + threadIdx.x;
    const unsigned int plane = geometry.height * geometry.width;
    const unsigned int extent = (axis == 0u) ? geometry.depth : geometry.height;
    const unsigned int stride = (axis == 0u) ? plane : geometry.width;
    const unsigned int columns = (axis == 0u) ? plane : (geometry.depth * geometry.width);
    if (column >= columns)
    {
        return;
    }
    const unsigned int base = (axis == 0u) ? column : (((column / geometry.width) * plane) + (column % geometry.width));
    for (unsigned int place = 0u; place < extent; place += 1u)
    {
        const unsigned int voxel = base + (place * stride);
        const unsigned int label = painted[voxel];
        unsigned int least_range = range_in[voxel];
        for (unsigned int radius = 1u; (label != CLIMB_MACHINE_OUTSIDE) && (radius < least_range); radius += 1u)
        {
            for (unsigned int direction = 0u; direction < 2u; direction += 1u)
            {
                const unsigned int neighbor_inside =
                    (direction == 0u) ? ((radius <= place) ? 1u : 0u) : (((place + radius) < extent) ? 1u : 0u);
                const unsigned int neighbor_place = (direction == 0u) ? (place - radius) : (place + radius);
                const unsigned int neighbor = (neighbor_inside != 0u) ? (base + (neighbor_place * stride)) : 0u;
                const unsigned int neighbor_range =
                    ((neighbor_inside != 0u) && (painted[neighbor] == label)) ? range_in[neighbor] : 0u;
                const unsigned int candidate_range = (neighbor_range > radius) ? neighbor_range : radius;
                least_range = (candidate_range < least_range) ? candidate_range : least_range;
            }
        }
        range_out[voxel] = (label != CLIMB_MACHINE_OUTSIDE) ? least_range : 0u;
    }
}

__global__ static void machine_core_kernel(unsigned int items, const unsigned int *item_run_first,
                                           const unsigned int *item_run_end, const int *item_center,
                                           const unsigned int *item_match, const unsigned int *run_start,
                                           const unsigned int *run_length, const unsigned int *painted,
                                           const unsigned int *range, MachineGeometry geometry, unsigned int widest,
                                           unsigned long long *fields, unsigned int *error)
{
    const unsigned int item = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (item >= items)
    {
        return;
    }
    const unsigned int match = item_match[item];
    const long long shift_depth = item_center[3u * item];
    const long long shift_height = item_center[(3u * item) + 1u];
    const long long shift_width = item_center[(3u * item) + 2u];
    const unsigned int plane = geometry.height * geometry.width;
    // item widens from unsigned int to unsigned long long before the multiply. Item * widest * 10 cannot wrap in 32
    // bits
    unsigned long long *const item_bins = &fields[(unsigned long long)item * widest * CLIMB_MACHINE_CORE_FIELDS];
    for (unsigned int run = item_run_first[item]; run < item_run_end[item]; run += 1u)
    {
        const unsigned int start = run_start[run];
        const unsigned int rest = start % plane;
        const unsigned long long depth_place = start / plane;
        const unsigned long long height_place = rest / geometry.width;
        // depth_place is below geometry.depth, a 32-bit count. It re-signs to long long exactly
        const long long lagged_depth = (long long)depth_place + shift_depth;
        // height_place is below geometry.height, a 32-bit count. It re-signs to long long exactly
        const long long lagged_height = (long long)height_place + shift_height;
        if ((lagged_depth < 0LL) || (lagged_depth >= geometry.depth) || (lagged_height < 0LL) ||
            (lagged_height >= geometry.height))
        {
            continue;
        }
        const long long lagged_row = ((lagged_depth * geometry.height) + lagged_height) * geometry.width;
        for (unsigned int step = 0u; step < run_length[run]; step += 1u)
        {
            const unsigned long long width_place = (rest % geometry.width) + step;
            // width_place is below geometry.width, a 32-bit count. It re-signs to long long exactly
            const long long lagged_width = (long long)width_place + shift_width;
            if ((lagged_width < 0LL) || (lagged_width >= geometry.width) ||
                (painted[lagged_row + lagged_width] != match))
            {
                continue;
            }
            const unsigned int core_width = range[lagged_row + lagged_width] - 1u;
            if (core_width >= widest)
            {
                atomicAdd(error, 1u);
                continue;
            }
            // core_width widens from unsigned int to unsigned long long, to match the 64-bit offset that indexes
            // item_bins
            unsigned long long *const bin = &item_bins[(unsigned long long)core_width * CLIMB_MACHINE_CORE_FIELDS];
            bin[0] += 1ULL;
            bin[1] += depth_place;
            bin[2] += height_place;
            bin[3] += width_place;
            bin[4] += depth_place * depth_place;
            bin[5] += height_place * height_place;
            bin[6] += width_place * width_place;
            bin[7] += depth_place * height_place;
            bin[8] += depth_place * width_place;
            bin[9] += height_place * width_place;
        }
    }
}

void machine_core_release(ClimbMachine *machine)
{
    platform_cell_released(machine->core_earlier, platform_place_g_program_owner);
    platform_cell_released(machine->core_later, platform_place_g_program_owner);
    platform_cell_released(machine->core_side, platform_place_g_program_owner);
    platform_cell_released(machine->core_leaf, platform_place_g_program_owner);
    platform_cell_released(machine->core_match, platform_place_g_program_owner);
    platform_cell_released(machine->core_lag, platform_place_g_program_owner);
    platform_cell_released(machine->core_width_first, platform_place_g_program_owner);
    platform_cell_released(machine->core_fields, platform_place_g_program_owner);
    machine->core_earlier = NULL;
    machine->core_later = NULL;
    machine->core_side = NULL;
    machine->core_leaf = NULL;
    machine->core_match = NULL;
    machine->core_lag = NULL;
    machine->core_width_first = NULL;
    machine->core_fields = NULL;
}

extern "C" int climb_machine_core(ClimbMachine *machine, ClimbMachineCore *core)
{
    if ((machine == NULL) || (core == NULL))
    {
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = core, .bytes = sizeof(*core), .val = 0);
    machine_core_release(machine);
    const unsigned long long started = engine_clock_microseconds();
    const MachineGeometry geometry = machine->geometry;
    const unsigned int least_plane_side = (geometry.height < geometry.width) ? geometry.height : geometry.width;
    const unsigned int least_side = (geometry.depth < least_plane_side) ? geometry.depth : least_plane_side;
    const unsigned int widest = (least_side + 1u) / 2u;
    unsigned int listed_count = 0u;
    for (unsigned int pair = 0u; pair < machine->ran_count; pair += 1u)
    {
        const ClimbMachinePair *const pending = &machine->pending[pair];
        listed_count +=
            (pending->later == (pending->earlier + 1u)) ? (pending->earlier_leaves + pending->later_leaves) : 0u;
    }
    // listed_count widens from unsigned int to size_t. Listed_count + 1 cannot wrap in 32 bits
    const size_t capacity = (size_t)listed_count + 1u;
    const size_t residents = machine->capacity;
    machine->core_earlier =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->core_later =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->core_side =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->core_leaf =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->core_match =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->core_lag = (int *)platform_cell_taken(capacity * 3u * sizeof(int), platform_place_g_program_owner, 0);
    machine->core_width_first =
        (unsigned int *)platform_cell_taken((capacity + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const listed_resident = (unsigned int *)platform_cell_taken(
        capacity * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int *const listed_run_first = (unsigned int *)platform_cell_taken(
        capacity * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int *const listed_run_end = (unsigned int *)platform_cell_taken(
        capacity * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    int *const listed_center = (int *)platform_cell_taken(capacity * 3u * sizeof(int), platform_place_g_program_owner,
                                                          PLATFORM_TIER_TEMPORARY);
    unsigned int *const widths = (unsigned int *)platform_cell_cleared(
        capacity, sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    size_t *const staged_at = (size_t *)platform_cell_cleared(capacity, sizeof(size_t), platform_place_g_program_owner,
                                                              PLATFORM_TIER_TEMPORARY);
    unsigned int *const resident_first = (unsigned int *)platform_cell_cleared(
        residents + 1u, sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int *const resident_order = (unsigned int *)platform_cell_taken(
        capacity * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    int steps_succeeded =
        (widest != 0u) && (machine->core_earlier != NULL) && (machine->core_later != NULL) &&
        (machine->core_side != NULL) && (machine->core_leaf != NULL) && (machine->core_match != NULL) &&
        (machine->core_lag != NULL) && (machine->core_width_first != NULL) && (listed_resident != NULL) &&
        (listed_run_first != NULL) && (listed_run_end != NULL) && (listed_center != NULL) && (widths != NULL) &&
        (staged_at != NULL) && (resident_first != NULL) && (resident_order != NULL);
    unsigned int climber_first = 0u;
    unsigned int listed_written = 0u;
    for (unsigned int pair = 0u; steps_succeeded && (pair < machine->ran_count); pair += 1u)
    {
        const ClimbMachinePair *const pending = &machine->pending[pair];
        const unsigned int adjacent = (pending->later == (pending->earlier + 1u)) ? 1u : 0u;
        for (unsigned int side = 0u; side < 2u; side += 1u)
        {
            const unsigned int leaves = (side == 0u) ? pending->earlier_leaves : pending->later_leaves;
            for (unsigned int leaf = 0u; (adjacent != 0u) && (leaf < leaves); leaf += 1u)
            {
                const unsigned int climber = climber_first + leaf;
                const unsigned int index = machine->climber_side[climber];
                const unsigned int own_resident = machine->side_residents[2u * index];
                const unsigned int *const leaf_bundles = machine->resident_leaf_bundles[own_resident];
                const unsigned int bundle_first = leaf_bundles[leaf];
                const unsigned int bundle_end = leaf_bundles[leaf + 1u];
                // own_resident * run_room narrows from size_t to unsigned int exactly, as machine_split_runs bounds
                // capacity
                // * run_room within 32 bits
                const unsigned int run_base = (unsigned int)(own_resident * machine->run_capacity);
                const unsigned int empty = (bundle_end <= bundle_first) ? 1u : 0u;
                machine->core_earlier[listed_written] = pending->earlier;
                machine->core_later[listed_written] = pending->later;
                machine->core_side[listed_written] = side;
                machine->core_leaf[listed_written] = leaf;
                machine->core_match[listed_written] = machine->landed[climber];
                listed_resident[listed_written] = machine->side_residents[(2u * index) + 1u];
                listed_run_first[listed_written] =
                    (empty != 0u) ? 0u : (run_base + machine->resident_bundle_first[own_resident][bundle_first]);
                listed_run_end[listed_written] =
                    (empty != 0u) ? 0u : (run_base + machine->resident_bundle_end[own_resident][bundle_end - 1u]);
                listed_center[3u * listed_written] = machine->centers[3u * climber];
                listed_center[(3u * listed_written) + 1u] = machine->centers[(3u * climber) + 1u];
                listed_center[(3u * listed_written) + 2u] = machine->centers[(3u * climber) + 2u];
                machine->core_lag[3u * listed_written] = listed_center[3u * listed_written];
                machine->core_lag[(3u * listed_written) + 1u] = listed_center[(3u * listed_written) + 1u];
                machine->core_lag[(3u * listed_written) + 2u] = listed_center[(3u * listed_written) + 2u];
                resident_first[listed_resident[listed_written] + 1u] +=
                    (machine->landed[climber] != CLIMB_MACHINE_OUTSIDE) ? 1u : 0u;
                listed_written += 1u;
            }
            climber_first += leaves;
        }
    }
    for (size_t resident = 0u; steps_succeeded && (resident < residents); resident += 1u)
    {
        resident_first[resident + 1u] += resident_first[resident];
    }
    unsigned int *const resident_fill =
        steps_succeeded ? (unsigned int *)platform_cell_taken((residents + 1u) * sizeof(unsigned int),
                                                              platform_place_g_program_owner, 0)
                        : NULL;
    steps_succeeded = steps_succeeded && (resident_fill != NULL);
    if (steps_succeeded)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = resident_fill, .src = resident_first,
                   .bytes = (residents + 1u) * sizeof(unsigned int));
    }
    for (unsigned int listed_place = 0u; steps_succeeded && (listed_place < listed_count); listed_place += 1u)
    {
        if (machine->core_match[listed_place] == CLIMB_MACHINE_OUTSIDE)
        {
            continue;
        }
        resident_order[resident_fill[listed_resident[listed_place]]] = listed_place;
        resident_fill[listed_resident[listed_place]] += 1u;
    }
    unsigned int max_resident_items = 0u;
    for (size_t resident = 0u; steps_succeeded && (resident < residents); resident += 1u)
    {
        const unsigned int resident_items = resident_first[resident + 1u] - resident_first[resident];
        max_resident_items = (resident_items > max_resident_items) ? resident_items : max_resident_items;
    }
    // voxels narrows from unsigned long long to size_t exactly, as climb_machine_open already allocated this many
    // labels
    const size_t voxels = (size_t)geometry.voxels;
    // max_resident_items widens from unsigned int to size_t before the multiply. Items * widest * 10 cannot wrap in 32
    // bits
    const size_t bins = (size_t)max_resident_items * widest * CLIMB_MACHINE_CORE_FIELDS;
    // max_resident_items widens from unsigned int to size_t. Max_resident_items + 1 cannot wrap in 32 bits
    const size_t item_capacity = (size_t)max_resident_items + 1u;
    // The scratch of the core is temporary cells of the pool of the device, every byte 0 at the take. The call gives
    // them back last first before it returns
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, CLIMB_MACHINE_PLACE);
    void *room[9] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const unsigned long long item_words = (unsigned long long)item_capacity * sizeof(unsigned int);
    const unsigned long long bytes[9] = {(unsigned long long)voxels * sizeof(unsigned int),
                                         (unsigned long long)voxels * sizeof(unsigned int),
                                         (unsigned long long)voxels * sizeof(unsigned int),
                                         item_words,
                                         item_words,
                                         (unsigned long long)item_capacity * 3ULL * sizeof(int),
                                         item_words,
                                         ((unsigned long long)bins + 1ULL) * sizeof(unsigned long long),
                                         sizeof(unsigned int)};
    for (unsigned int which = 0u; steps_succeeded && (which < 9u); which += 1u)
    {
        steps_succeeded = tessera_job_cells_room(&cells, bytes[which], &room[which]);
    }
    // Explicit casts read each cell as the words that it holds
    unsigned int *const painted = (unsigned int *)room[0];
    unsigned int *const range_first = (unsigned int *)room[1];
    unsigned int *const range_second = (unsigned int *)room[2];
    unsigned int *const device_run_first = (unsigned int *)room[3];
    unsigned int *const device_run_end = (unsigned int *)room[4];
    int *const device_center = (int *)room[5];
    unsigned int *const device_match = (unsigned int *)room[6];
    unsigned long long *const device_bins = (unsigned long long *)room[7];
    unsigned int *const device_error = (unsigned int *)room[8];
    unsigned int *const item_run_first = steps_succeeded
                                             ? (unsigned int *)platform_cell_taken(item_capacity * sizeof(unsigned int),
                                                                                   platform_place_g_program_owner, 0)
                                             : NULL;
    unsigned int *const item_run_end = steps_succeeded
                                           ? (unsigned int *)platform_cell_taken(item_capacity * sizeof(unsigned int),
                                                                                 platform_place_g_program_owner, 0)
                                           : NULL;
    int *const item_center = steps_succeeded ? (int *)platform_cell_taken(item_capacity * 3u * sizeof(int),
                                                                          platform_place_g_program_owner, 0)
                                             : NULL;
    unsigned int *const item_match = steps_succeeded
                                         ? (unsigned int *)platform_cell_taken(item_capacity * sizeof(unsigned int),
                                                                               platform_place_g_program_owner, 0)
                                         : NULL;
    unsigned long long *const host_bins =
        steps_succeeded ? (unsigned long long *)platform_cell_taken((bins + 1u) * sizeof(unsigned long long),
                                                                    platform_place_g_program_owner, 0)
                        : NULL;
    steps_succeeded = steps_succeeded && (item_run_first != NULL) && (item_run_end != NULL) && (item_center != NULL) &&
                      (item_match != NULL) && (host_bins != NULL);
    unsigned long long *staged = NULL;
    size_t staged_capacity = 0u;
    size_t staged_count = 0u;
    const unsigned int rows = geometry.depth * geometry.height;
    const unsigned int plane = geometry.height * geometry.width;
    const unsigned int height_columns = geometry.depth * geometry.width;
    for (unsigned int resident = 0u; steps_succeeded && (resident < machine->capacity); resident += 1u)
    {
        const unsigned int resident_item_first = resident_first[resident];
        const unsigned int resident_items = resident_first[resident + 1u] - resident_item_first;
        if (resident_items == 0u)
        {
            continue;
        }
        for (unsigned int item = 0u; item < resident_items; item += 1u)
        {
            const unsigned int listed_place = resident_order[resident_item_first + item];
            item_run_first[item] = listed_run_first[listed_place];
            item_run_end[item] = listed_run_end[listed_place];
            item_center[3u * item] = listed_center[3u * listed_place];
            item_center[(3u * item) + 1u] = listed_center[(3u * listed_place) + 1u];
            item_center[(3u * item) + 2u] = listed_center[(3u * listed_place) + 2u];
            item_match[item] = machine->core_match[listed_place];
        }
        const size_t base = resident * machine->run_capacity;
        const unsigned int runs = machine->resident_runs[resident];
        // resident_items widens from unsigned int to size_t before the multiply. Items * widest * 10 cannot wrap in 32
        // bits
        const size_t resident_bins = (size_t)resident_items * widest * CLIMB_MACHINE_CORE_FIELDS;
        // every voxel of the paint starts outside each leaf, every byte 0xFF
        const unsigned long long resident_words = (unsigned long long)resident_items * sizeof(unsigned int);
        steps_succeeded =
            machine_filled(painted, voxels, CLIMB_MACHINE_OUTSIDE) &&
            tessera_job_cleared(device_bins, (unsigned long long)resident_bins * sizeof(unsigned long long)) &&
            tessera_job_crossed_into(device_run_first, item_run_first, resident_words) &&
            tessera_job_crossed_into(device_run_end, item_run_end, resident_words) &&
            tessera_job_crossed_into(device_center, item_center, resident_words * 3ULL) &&
            tessera_job_crossed_into(device_match, item_match, resident_words);
        if (steps_succeeded && (runs != 0u))
        {
            machine_paint_kernel<<<(runs + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK, CLIMB_MACHINE_BLOCK>>>(
                &machine->run_start[base], &machine->run_length[base], &machine->run_leaf[base], runs, painted);
            steps_succeeded = machine_launched();
        }
        if (steps_succeeded)
        {
            machine_range_row_kernel<<<(rows + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK, CLIMB_MACHINE_BLOCK>>>(
                painted, geometry, range_first);
            machine_range_column_kernel<<<(height_columns + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK,
                                          CLIMB_MACHINE_BLOCK>>>(painted, geometry, 1u, range_first, range_second);
            machine_range_column_kernel<<<(plane + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK,
                                          CLIMB_MACHINE_BLOCK>>>(painted, geometry, 0u, range_second, range_first);
            machine_core_kernel<<<(resident_items + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK,
                                  CLIMB_MACHINE_BLOCK>>>(
                resident_items, device_run_first, device_run_end, device_center, device_match, machine->run_start,
                machine->run_length, painted, range_first, geometry, widest, device_bins, device_error);
            steps_succeeded = machine_launched() && (cudaDeviceSynchronize() == cudaSuccess);
        }
        steps_succeeded =
            steps_succeeded && tessera_job_crossed_out(host_bins, device_bins,
                                                       (unsigned long long)resident_bins * sizeof(unsigned long long));
        for (unsigned int item = 0u; steps_succeeded && (item < resident_items); item += 1u)
        {
            const unsigned int listed_place = resident_order[resident_item_first + item];
            // item widens from unsigned int to size_t before the multiply. Item * widest * 10 cannot wrap in 32 bits
            const unsigned long long *const item_bins = &host_bins[(size_t)item * widest * CLIMB_MACHINE_CORE_FIELDS];
            unsigned int top_width = 0u;
            for (unsigned int width = 0u; width < widest; width += 1u)
            {
                // width widens from unsigned int to size_t, to match the size_t offset that indexes item_bins
                top_width = (item_bins[(size_t)width * CLIMB_MACHINE_CORE_FIELDS] != 0ULL) ? (width + 1u) : top_width;
            }
            if ((staged_count + top_width) > staged_capacity)
            {
                const size_t grown_capacity = ((staged_count + top_width) * 2u) + 1u;
                unsigned long long *const grown = (unsigned long long *)platform_cell_grown(
                    staged, staged_capacity * CLIMB_MACHINE_CORE_FIELDS * sizeof(unsigned long long),
                    grown_capacity * CLIMB_MACHINE_CORE_FIELDS * sizeof(unsigned long long),
                    platform_place_g_program_owner, 0);
                steps_succeeded = (grown != NULL);
                staged = steps_succeeded ? grown : staged;
                staged_capacity = steps_succeeded ? grown_capacity : staged_capacity;
            }
            for (unsigned int width = top_width; steps_succeeded && (width > 0u); width -= 1u)
            {
                // width - 1 widens from unsigned int to size_t, to match the size_t offset that indexes item_bins
                const size_t item_offset = (size_t)(width - 1u) * CLIMB_MACHINE_CORE_FIELDS;
                const size_t staged_offset = (staged_count + width - 1u) * CLIMB_MACHINE_CORE_FIELDS;
                for (unsigned int field = 0u; field < CLIMB_MACHINE_CORE_FIELDS; field += 1u)
                {
                    const unsigned long long wider_sum =
                        (width == top_width) ? 0ULL : staged[staged_offset + CLIMB_MACHINE_CORE_FIELDS + field];
                    staged[staged_offset + field] = item_bins[item_offset + field] + wider_sum;
                }
            }
            staged_at[listed_place] = staged_count;
            widths[listed_place] = top_width;
            staged_count += steps_succeeded ? top_width : 0u;
        }
    }
    unsigned int error = 0u;
    steps_succeeded = steps_succeeded && tessera_job_crossed_out(&error, device_error, sizeof(unsigned int)) &&
                      (error == 0u) && (staged_count <= 0xFFFFFFFFULL);
    machine->core_fields = steps_succeeded
                               ? (unsigned long long *)platform_cell_taken(
                                     (staged_count + 1u) * CLIMB_MACHINE_CORE_FIELDS * sizeof(unsigned long long),
                                     platform_place_g_program_owner, 0)
                               : NULL;
    steps_succeeded = steps_succeeded && (machine->core_fields != NULL);
    size_t widths_written = 0u;
    for (unsigned int listed_place = 0u; steps_succeeded && (listed_place < listed_count); listed_place += 1u)
    {
        // widths_written is at max staged_count, bounded within 32 bits above. It narrows from size_t to unsigned int
        // exactly
        machine->core_width_first[listed_place] = (unsigned int)widths_written;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &machine->core_fields[widths_written * CLIMB_MACHINE_CORE_FIELDS],
                   .src = &staged[staged_at[listed_place] * CLIMB_MACHINE_CORE_FIELDS],
                   .bytes = widths[listed_place] * sizeof(unsigned long long) * CLIMB_MACHINE_CORE_FIELDS);
        widths_written += widths[listed_place];
    }
    if (steps_succeeded)
    {
        // widths_written equals staged_count, bounded within 32 bits above. It narrows from size_t to unsigned int
        // exactly
        machine->core_width_first[listed_count] = (unsigned int)widths_written;
    }
    tessera_job_cells_given(&cells);
    platform_cell_released(item_run_first, platform_place_g_program_owner);
    platform_cell_released(item_run_end, platform_place_g_program_owner);
    platform_cell_released(item_center, platform_place_g_program_owner);
    platform_cell_released(item_match, platform_place_g_program_owner);
    platform_cell_released(host_bins, platform_place_g_program_owner);
    platform_cell_released(staged, platform_place_g_program_owner);
    platform_cell_released(resident_fill, platform_place_g_program_owner);
    platform_cell_released(resident_order, platform_place_g_program_owner);
    platform_cell_released(resident_first, platform_place_g_program_owner);
    platform_cell_released(staged_at, platform_place_g_program_owner);
    platform_cell_released(widths, platform_place_g_program_owner);
    platform_cell_released(listed_center, platform_place_g_program_owner);
    platform_cell_released(listed_run_end, platform_place_g_program_owner);
    platform_cell_released(listed_run_first, platform_place_g_program_owner);
    platform_cell_released(listed_resident, platform_place_g_program_owner);
    if (steps_succeeded == 0)
    {
        machine_core_release(machine);
        return 0;
    }
    core->climbers = listed_count;
    core->earlier = machine->core_earlier;
    core->later = machine->core_later;
    core->side = machine->core_side;
    core->leaf = machine->core_leaf;
    core->match = machine->core_match;
    core->lag = machine->core_lag;
    core->width_first = machine->core_width_first;
    core->fields = machine->core_fields;
    // widths_written equals staged_count, bounded within 32 bits above. It narrows from size_t to unsigned int exactly
    core->widths = (unsigned int)widths_written;
    core->microseconds = engine_clock_microseconds() - started;
    return 1;
}
