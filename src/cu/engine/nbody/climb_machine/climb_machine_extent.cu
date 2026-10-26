// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// climb_machine_extent.cu: the extent
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
    unsigned int *const initial_extents = (unsigned int *)malloc((fields + 1u) * sizeof(unsigned int));
    for (unsigned int leaf = 0u; (initial_extents != NULL) && (leaf < leaves); leaf += 1u)
    {
        for (unsigned int axis = 0u; axis < axes; axis += 1u)
        {
            initial_extents[(CLIMB_MACHINE_EXTENT_FIELDS * leaf) + axis] = 0xFFFFFFFFu;
            initial_extents[(CLIMB_MACHINE_EXTENT_FIELDS * leaf) + axes + axis] = 0u;
        }
    }
    unsigned int *device_extents = NULL;
    int steps_succeeded = (initial_extents != NULL) &&
                          (cudaMalloc((void **)&device_extents, (fields + 1u) * sizeof(unsigned int)) == cudaSuccess) &&
                          (cudaMemcpy(device_extents, initial_extents, fields * sizeof(unsigned int),
                                      cudaMemcpyHostToDevice) == cudaSuccess);
    if (steps_succeeded && (runs != 0u))
    {
        const size_t base = resident * machine->run_capacity;
        machine_extent_kernel<<<(runs + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK, CLIMB_MACHINE_BLOCK>>>(
            &machine->run_start[base], &machine->run_length[base], &machine->run_leaf[base], runs, machine->geometry,
            device_extents);
        steps_succeeded = machine_launched();
    }
    steps_succeeded = steps_succeeded && (cudaMemcpy(request->extents, device_extents, fields * sizeof(unsigned int),
                                                     cudaMemcpyDeviceToHost) == cudaSuccess);
    cudaFree(device_extents);
    free(initial_extents);
    return steps_succeeded ? 1 : 0;
}

extern "C" void climb_machine_close(ClimbMachine *machine)
{
    if (machine == NULL)
    {
        return;
    }
    cudaFree(machine->scratch_labels);
    cudaFree(machine->positive);
    cudaFree(machine->row_first);
    for (unsigned int resident = 0u; (machine->resident_leaf_bundles != NULL) && (resident < machine->capacity);
         resident += 1u)
    {
        free(machine->resident_leaf_bundles[resident]);
        free(machine->resident_bundle_first[resident]);
        free(machine->resident_bundle_end[resident]);
        free(machine->resident_contact_start[resident]);
        free(machine->resident_contacts[resident]);
    }
    free(machine->resident_frame);
    free(machine->resident_leaves);
    free(machine->resident_runs);
    free(machine->resident_leaf_bundles);
    free(machine->resident_bundle_first);
    free(machine->resident_bundle_end);
    free(machine->resident_contact_start);
    free(machine->resident_contacts);
    cudaFree(machine->run_start);
    cudaFree(machine->run_length);
    cudaFree(machine->run_leaf);
    cudaFree(machine->run_at_row);
    cudaFree(machine->device_map);
    cudaFree(machine->device_peaks);
    free(machine->row_counts);
    cudaFree(machine->device_row_counts);
    cudaFree(machine->device_row_offsets);
    cudaFree(machine->device_unsorted_start);
    cudaFree(machine->device_unsorted_length);
    cudaFree(machine->device_unsorted_leaf);
    free(machine->unsorted_leaf);
    free(machine->order);
    cudaFree(machine->device_order);
    free(machine->leaf_counts);
    free(machine->pending);
    free(machine->side_residents);
    cudaFree(machine->device_side_residents);
    free(machine->climber_side);
    free(machine->climber_peak);
    free(machine->climber_entry_start);
    free(machine->centers);
    free(machine->active);
    free(machine->landed);
    free(machine->final_score);
    cudaFree(machine->device_final_score);
    cudaFree(machine->device_climber_side);
    cudaFree(machine->device_climber_peak);
    cudaFree(machine->device_climber_entry_start);
    cudaFree(machine->device_centers);
    cudaFree(machine->device_active);
    cudaFree(machine->device_landed);
    free(machine->entry_first);
    free(machine->entry_end);
    free(machine->entry_climber);
    cudaFree(machine->device_entry_first);
    cudaFree(machine->device_entry_end);
    cudaFree(machine->device_entry_climber);
    cudaFree(machine->device_scores);
    cudaFree(machine->device_moved);
    cudaFree(machine->device_spiral);
    cudaFreeHost(machine->pinned_moved);
    cudaFreeHost(machine->pinned_zero);
    if (machine->blocks_done[0] != NULL)
    {
        cudaEventDestroy(machine->blocks_done[0]);
    }
    if (machine->blocks_done[1] != NULL)
    {
        cudaEventDestroy(machine->blocks_done[1]);
    }
    machine_box_release(machine);
    machine_core_release(machine);
    free(machine);
}
