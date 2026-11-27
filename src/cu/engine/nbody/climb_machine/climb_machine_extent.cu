// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// climb_machine_extent.cu: the extent
#include "../../../../platform/platform.h"
#include "climb_machine_internal.h"

__global__ static void machine_extent_kernel(const unsigned int *run_start, const unsigned int *run_length,
                                             const unsigned int *run_leaf, unsigned int runs, MachineGeometry geometry,
                                             unsigned int *extents)
{
    const unsigned int run = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (run >= runs)
    {
        return;
    }
    const unsigned int plane = geometry.height * geometry.width;
    const unsigned int start = run_start[run];
    const unsigned int rest = start % plane;
    const unsigned int depth_place = start / plane;
    const unsigned int height_place = rest / geometry.width;
    const unsigned int width_place = rest % geometry.width;
    unsigned int *const extent = &extents[CLIMB_MACHINE_EXTENT_FIELDS * run_leaf[run]];
    atomicMin(&extent[0], depth_place);
    atomicMin(&extent[1], height_place);
    atomicMin(&extent[2], width_place);
    atomicMax(&extent[3], depth_place);
    atomicMax(&extent[4], height_place);
    atomicMax(&extent[5], width_place + run_length[run] - 1u);
}

extern "C" int climb_machine_extents(ClimbMachine *machine, const ClimbMachineExtentsRequest *request)
{
    const unsigned int resident = ((machine != NULL) && (request != NULL) && (request->extents != NULL))
                                      ? machine_resident_of(machine, request->frame)
                                      : CLIMB_MACHINE_EMPTY;
    if (resident == CLIMB_MACHINE_EMPTY)
    {
        return 0;
    }
    const unsigned int leaves = machine->resident_leaves[resident];
    const unsigned int runs = machine->resident_runs[resident];
    const unsigned int axes = CLIMB_MACHINE_EXTENT_FIELDS / 2u;
    // leaves widens from unsigned int to size_t before the multiply. Leaves * 6 cannot wrap in 32 bits
    const size_t fields = (size_t)leaves * CLIMB_MACHINE_EXTENT_FIELDS;
    unsigned int *const initial_extents = (unsigned int *)platform_cell_taken(
        (fields + 1u) * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned int leaf = 0u; (initial_extents != NULL) && (leaf < leaves); leaf += 1u)
    {
        for (unsigned int axis = 0u; axis < axes; axis += 1u)
        {
            initial_extents[(CLIMB_MACHINE_EXTENT_FIELDS * leaf) + axis] = 0xFFFFFFFFu;
            initial_extents[(CLIMB_MACHINE_EXTENT_FIELDS * leaf) + axes + axis] = 0u;
        }
    }
    // the extents of the leaves, a temporary cell of the pool of the device that the call gives back before it returns
    void *cell = NULL;
    int steps_succeeded =
        (initial_extents != NULL) &&
        tessera_job_cell_taken((fields + 1u) * sizeof(unsigned int), CLIMB_MACHINE_PLACE, &cell) &&
        tessera_job_crossed_into(cell, initial_extents, (unsigned long long)fields * sizeof(unsigned int));
    // Explicit cast reads the cell as the words that it holds
    unsigned int *const device_extents = (unsigned int *)cell;
    if (steps_succeeded && (runs != 0u))
    {
        const size_t base = resident * machine->run_capacity;
        machine_extent_kernel<<<(runs + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK, CLIMB_MACHINE_BLOCK>>>(
            &machine->run_start[base], &machine->run_length[base], &machine->run_leaf[base], runs, machine->geometry,
            device_extents);
        steps_succeeded = machine_launched();
    }
    steps_succeeded = steps_succeeded && tessera_job_crossed_out(request->extents, device_extents,
                                                                 (unsigned long long)fields * sizeof(unsigned int));
    tessera_job_given(cell, CLIMB_MACHINE_PLACE);
    platform_cell_released(initial_extents, platform_place_g_program_owner);
    return steps_succeeded ? 1 : 0;
}

extern "C" void climb_machine_close(ClimbMachine *machine)
{
    if (machine == NULL)
    {
        return;
    }
    machine_cell_given((void **)&machine->scratch_labels);
    machine_cell_given((void **)&machine->positive);
    machine_cell_given((void **)&machine->row_first);
    for (unsigned int resident = 0u; (machine->resident_leaf_bundles != NULL) && (resident < machine->capacity);
         resident += 1u)
    {
        platform_cell_released(machine->resident_leaf_bundles[resident], platform_place_g_program_owner);
        platform_cell_released(machine->resident_bundle_first[resident], platform_place_g_program_owner);
        platform_cell_released(machine->resident_bundle_end[resident], platform_place_g_program_owner);
        platform_cell_released(machine->resident_contact_start[resident], platform_place_g_program_owner);
        platform_cell_released(machine->resident_contacts[resident], platform_place_g_program_owner);
    }
    platform_cell_released(machine->resident_frame, platform_place_g_program_owner);
    platform_cell_released(machine->resident_leaves, platform_place_g_program_owner);
    platform_cell_released(machine->resident_runs, platform_place_g_program_owner);
    platform_cell_released(machine->resident_leaf_bundles, platform_place_g_program_owner);
    platform_cell_released(machine->resident_bundle_first, platform_place_g_program_owner);
    platform_cell_released(machine->resident_bundle_end, platform_place_g_program_owner);
    platform_cell_released(machine->resident_contact_start, platform_place_g_program_owner);
    platform_cell_released(machine->resident_contacts, platform_place_g_program_owner);
    machine_cell_given((void **)&machine->run_start);
    machine_cell_given((void **)&machine->run_length);
    machine_cell_given((void **)&machine->run_leaf);
    machine_cell_given((void **)&machine->run_at_row);
    machine_cell_given((void **)&machine->device_map);
    machine_cell_given((void **)&machine->device_peaks);
    platform_cell_released(machine->row_counts, platform_place_g_program_owner);
    machine_cell_given((void **)&machine->device_row_counts);
    machine_cell_given((void **)&machine->device_row_offsets);
    machine_cell_given((void **)&machine->device_unsorted_start);
    machine_cell_given((void **)&machine->device_unsorted_length);
    machine_cell_given((void **)&machine->device_unsorted_leaf);
    platform_cell_released(machine->unsorted_leaf, platform_place_g_program_owner);
    platform_cell_released(machine->order, platform_place_g_program_owner);
    machine_cell_given((void **)&machine->device_order);
    platform_cell_released(machine->leaf_counts, platform_place_g_program_owner);
    platform_cell_released(machine->pending, platform_place_g_program_owner);
    platform_cell_released(machine->side_residents, platform_place_g_program_owner);
    machine_cell_given((void **)&machine->device_side_residents);
    platform_cell_released(machine->climber_side, platform_place_g_program_owner);
    platform_cell_released(machine->climber_peak, platform_place_g_program_owner);
    platform_cell_released(machine->climber_entry_start, platform_place_g_program_owner);
    platform_cell_released(machine->centers, platform_place_g_program_owner);
    platform_cell_released(machine->active, platform_place_g_program_owner);
    platform_cell_released(machine->landed, platform_place_g_program_owner);
    platform_cell_released(machine->final_score, platform_place_g_program_owner);
    machine_cell_given((void **)&machine->device_final_score);
    machine_cell_given((void **)&machine->device_climber_side);
    machine_cell_given((void **)&machine->device_climber_peak);
    machine_cell_given((void **)&machine->device_climber_entry_start);
    machine_cell_given((void **)&machine->device_centers);
    machine_cell_given((void **)&machine->device_active);
    machine_cell_given((void **)&machine->device_landed);
    platform_cell_released(machine->entry_first, platform_place_g_program_owner);
    platform_cell_released(machine->entry_end, platform_place_g_program_owner);
    platform_cell_released(machine->entry_climber, platform_place_g_program_owner);
    machine_cell_given((void **)&machine->device_entry_first);
    machine_cell_given((void **)&machine->device_entry_end);
    machine_cell_given((void **)&machine->device_entry_climber);
    machine_cell_given((void **)&machine->device_scores);
    machine_cell_given((void **)&machine->device_moved);
    machine_cell_given((void **)&machine->device_spiral);
    machine_box_release(machine);
    machine_core_release(machine);
    platform_cell_released(machine, platform_place_g_program_owner);
}
