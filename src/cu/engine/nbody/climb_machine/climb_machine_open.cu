// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// climb_machine_open.cu: the runs cut, the machine opened and a frame stored
#include "climb_machine_internal.h"

static int machine_split_runs(ClimbMachine *machine, unsigned int resident, const unsigned int *labels,
                              const ClimbMachineFrame *frame)
{
    const MachineGeometry geometry = machine->geometry;
    const unsigned int rows = geometry.depth * geometry.height;
    const unsigned int leaves = frame->leaf_count;
    const unsigned int row_blocks = (rows + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK;
    const unsigned int leaf_blocks = (leaves + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK;
    int ok = (leaves == 0u) || (cudaMemcpy(machine->device_peaks, frame->peaks, (size_t)leaves * sizeof(unsigned int),
                                           cudaMemcpyHostToDevice) == cudaSuccess);
    if ((ok != 0) && (leaves != 0u))
    {
        machine_map_kernel<<<leaf_blocks, CLIMB_MACHINE_BLOCK>>>(machine->device_peaks, leaves, 0u,
                                                                 machine->device_map);
        ok = machine_launched();
    }
    if (ok != 0)
    {
        machine_row_kernel<<<row_blocks, CLIMB_MACHINE_BLOCK>>>(labels, machine->device_map, rows, geometry, NULL,
                                                                machine->device_row_counts, NULL, NULL, NULL);
        ok = machine_launched();
    }
    ok = ok && (cudaMemcpy(machine->row_counts, machine->device_row_counts, (size_t)rows * sizeof(unsigned int),
                           cudaMemcpyDeviceToHost) == cudaSuccess);
    size_t runs = 0u;
    for (unsigned int row = 0u; (ok != 0) && (row < rows); row += 1u)
    {
        const unsigned int count = machine->row_counts[row];
        machine->row_counts[row] = (unsigned int)runs;
        runs += (size_t)count;
    }
    if ((ok != 0) && (runs + 1u > machine->unsorted_capacity))
    {
        const size_t capacity = runs + (runs / 2u) + 1u;
        cudaFree(machine->device_unsorted_start);
        cudaFree(machine->device_unsorted_length);
        cudaFree(machine->device_unsorted_leaf);
        machine->device_unsorted_start = NULL;
        machine->device_unsorted_length = NULL;
        machine->device_unsorted_leaf = NULL;
        ok = ok &&
             (cudaMalloc((void **)&machine->device_unsorted_start, capacity * sizeof(unsigned int)) == cudaSuccess);
        ok = ok &&
             (cudaMalloc((void **)&machine->device_unsorted_length, capacity * sizeof(unsigned int)) == cudaSuccess);
        ok =
            ok && (cudaMalloc((void **)&machine->device_unsorted_leaf, capacity * sizeof(unsigned int)) == cudaSuccess);
        ok = ok &&
             machine_grow((void **)&machine->order, (void **)&machine->device_order, capacity * sizeof(unsigned int));
        free(machine->unsorted_leaf);
        machine->unsorted_leaf = (unsigned int *)malloc(capacity * sizeof(unsigned int));
        ok = ok && (machine->unsorted_leaf != NULL);
        machine->unsorted_capacity = (ok != 0) ? capacity : 0u;
    }
    ok = ok && (cudaMemcpy(machine->device_row_offsets, machine->row_counts, (size_t)rows * sizeof(unsigned int),
                           cudaMemcpyHostToDevice) == cudaSuccess);
    if (ok != 0)
    {
        machine_row_kernel<<<row_blocks, CLIMB_MACHINE_BLOCK>>>(
            labels, machine->device_map, rows, geometry, machine->device_row_offsets, machine->device_row_counts,
            machine->device_unsorted_start, machine->device_unsorted_length, machine->device_unsorted_leaf);
        ok = machine_launched();
    }
    if ((ok != 0) && (leaves != 0u))
    {
        machine_map_kernel<<<leaf_blocks, CLIMB_MACHINE_BLOCK>>>(machine->device_peaks, leaves, 1u,
                                                                 machine->device_map);
        ok = machine_launched();
    }
    ok = ok && ((runs == 0u) || (cudaMemcpy(machine->unsorted_leaf, machine->device_unsorted_leaf,
                                            runs * sizeof(unsigned int), cudaMemcpyDeviceToHost) == cudaSuccess));
    if (ok == 0)
    {
        return 0;
    }

    unsigned int *const counts = machine->leaf_counts;
    memset(counts, 0, ((size_t)leaves + 1u) * sizeof(unsigned int));
    for (size_t run = 0u; run < runs; run += 1u)
    {
        counts[machine->unsorted_leaf[run] + 1u] += 1u;
    }
    for (unsigned int leaf = 0u; leaf < leaves; leaf += 1u)
    {
        counts[leaf + 1u] += counts[leaf];
    }
    for (size_t run = 0u; run < runs; run += 1u)
    {
        const unsigned int leaf = machine->unsorted_leaf[run];
        machine->order[counts[leaf]] = (unsigned int)run;
        counts[leaf] += 1u;
    }

    if (runs > machine->run_capacity)
    {
        const size_t capacity = runs + (runs / 4u) + 1u;
        const size_t residents = (size_t)machine->capacity;
        unsigned int *grown[4] = {NULL, NULL, NULL, NULL};
        unsigned int *const current[4] = {machine->run_start, machine->run_length, machine->run_leaf,
                                          machine->run_at_row};
        ok = ok && ((residents * capacity) <= 0xFFFFFFFFull);
        for (unsigned int which = 0u; (ok != 0) && (which < 4u); which += 1u)
        {
            ok = (cudaMalloc((void **)&grown[which], residents * capacity * sizeof(unsigned int)) == cudaSuccess) ? 1
                                                                                                                  : 0;
        }
        for (unsigned int other_resident = 0u; (ok != 0) && (other_resident < machine->capacity); other_resident += 1u)
        {
            const size_t count = (size_t)machine->resident_runs[other_resident];
            if ((other_resident == resident) || (machine->resident_frame[other_resident] == CLIMB_MACHINE_EMPTY) ||
                (count == 0u))
            {
                continue;
            }
            for (unsigned int which = 0u; (ok != 0) && (which < 4u); which += 1u)
            {
                ok = (cudaMemcpy(&grown[which][(size_t)other_resident * capacity],
                                 &current[which][(size_t)other_resident * machine->run_capacity],
                                 count * sizeof(unsigned int), cudaMemcpyDeviceToDevice) == cudaSuccess)
                         ? 1
                         : 0;
            }
        }
        for (unsigned int which = 0u; which < 4u; which += 1u)
        {
            cudaFree(current[which]);
        }
        machine->run_start = grown[0];
        machine->run_length = grown[1];
        machine->run_leaf = grown[2];
        machine->run_at_row = grown[3];
        machine->run_capacity = (ok != 0) ? capacity : 0u;
    }
    ok = ok && ((runs == 0u) || (cudaMemcpy(machine->device_order, machine->order, runs * sizeof(unsigned int),
                                            cudaMemcpyHostToDevice) == cudaSuccess));
    if ((ok != 0) && (runs != 0u))
    {
        const size_t base = (size_t)resident * machine->run_capacity;
        const unsigned int run_blocks = (unsigned int)((runs + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK);
        machine_gather_kernel<<<run_blocks, CLIMB_MACHINE_BLOCK>>>(
            machine->device_order, (unsigned int)runs, machine->device_unsorted_start, machine->device_unsorted_length,
            machine->device_unsorted_leaf, &machine->run_start[base], &machine->run_length[base],
            &machine->run_leaf[base], &machine->run_at_row[base]);
        ok = machine_launched();
    }
    if (ok != 0)
    {
        const size_t offsets = (size_t)resident * ((size_t)rows + 1u);
        const unsigned int total = (unsigned int)runs;
        ok = (cudaMemcpy(&machine->row_first[offsets], machine->row_counts, (size_t)rows * sizeof(unsigned int),
                         cudaMemcpyHostToDevice) == cudaSuccess)
                 ? 1
                 : 0;
        ok = ok && (cudaMemcpy(&machine->row_first[offsets + rows], &total, sizeof(unsigned int),
                               cudaMemcpyHostToDevice) == cudaSuccess);
    }

    free(machine->resident_leaf_bundles[resident]);
    free(machine->resident_bundle_first[resident]);
    free(machine->resident_bundle_end[resident]);
    machine->resident_leaf_bundles[resident] = (unsigned int *)malloc(((size_t)leaves + 1u) * sizeof(unsigned int));
    const size_t bundle_capacity = (runs / CLIMB_MACHINE_BUNDLE) + (size_t)leaves + 1u;
    machine->resident_bundle_first[resident] = (unsigned int *)malloc(bundle_capacity * sizeof(unsigned int));
    machine->resident_bundle_end[resident] = (unsigned int *)malloc(bundle_capacity * sizeof(unsigned int));
    ok = ok && (machine->resident_leaf_bundles[resident] != NULL) &&
         (machine->resident_bundle_first[resident] != NULL) && (machine->resident_bundle_end[resident] != NULL);
    unsigned int bundles = 0u;
    unsigned int leaf_first = 0u;
    for (unsigned int leaf = 0u; (ok != 0) && (leaf < leaves); leaf += 1u)
    {
        const unsigned int leaf_end = counts[leaf];
        machine->resident_leaf_bundles[resident][leaf] = bundles;
        for (unsigned int first = leaf_first; first < leaf_end; first += CLIMB_MACHINE_BUNDLE)
        {
            machine->resident_bundle_first[resident][bundles] = first;
            machine->resident_bundle_end[resident][bundles] =
                ((leaf_end - first) < CLIMB_MACHINE_BUNDLE) ? leaf_end : (first + CLIMB_MACHINE_BUNDLE);
            bundles += 1u;
        }
        leaf_first = leaf_end;
    }
    if (ok != 0)
    {
        machine->resident_leaf_bundles[resident][leaves] = bundles;
        machine->resident_runs[resident] = (unsigned int)runs;
    }
    return ok;
}

extern "C" ClimbMachine *climb_machine_open(const ClimbMachineExtent *extent)
{
    if ((extent == NULL) || (extent->depth == 0u) || (extent->height == 0u) || (extent->width == 0u))
    {
        return NULL;
    }
    ClimbMachine *const machine = (ClimbMachine *)calloc(1u, sizeof(ClimbMachine));
    if (machine == NULL)
    {
        return NULL;
    }
    MachineGeometry *const geometry = &machine->geometry;
    geometry->depth = extent->depth;
    geometry->height = extent->height;
    geometry->width = extent->width;
    geometry->weight_z = extent->weight_z;
    geometry->voxels = (unsigned long long)extent->depth * extent->height * extent->width;
    geometry->words = (geometry->voxels + 63ull) / 64ull;
    machine->peak_capacity = extent->peak_capacity;
    machine->capacity = (extent->frames < 2u) ? 2u : extent->frames;
    const size_t residents = (size_t)machine->capacity;
    int ok = 1;
    ok = ok && (cudaMalloc((void **)&machine->scratch_labels, 2u * (size_t)geometry->voxels * sizeof(unsigned int)) ==
                cudaSuccess);
    machine->scratch_frame[0] = CLIMB_MACHINE_EMPTY;
    machine->scratch_frame[1] = CLIMB_MACHINE_EMPTY;
    ok = ok && (cudaMalloc((void **)&machine->positive,
                           residents * (size_t)geometry->words * sizeof(unsigned long long)) == cudaSuccess);
    ok = ok && (cudaMalloc((void **)&machine->row_first, residents * (((size_t)extent->depth * extent->height) + 1u) *
                                                             sizeof(unsigned int)) == cudaSuccess);
    machine->resident_frame = (unsigned int *)malloc(residents * sizeof(unsigned int));
    machine->resident_leaves = (unsigned int *)calloc(residents, sizeof(unsigned int));
    machine->resident_runs = (unsigned int *)calloc(residents, sizeof(unsigned int));
    machine->resident_leaf_bundles = (unsigned int **)calloc(residents, sizeof(unsigned int *));
    machine->resident_bundle_first = (unsigned int **)calloc(residents, sizeof(unsigned int *));
    machine->resident_bundle_end = (unsigned int **)calloc(residents, sizeof(unsigned int *));
    machine->resident_contact_start = (unsigned int **)calloc(residents, sizeof(unsigned int *));
    machine->resident_contacts = (unsigned int **)calloc(residents, sizeof(unsigned int *));
    ok = ok && (machine->resident_frame != NULL) && (machine->resident_leaves != NULL) &&
         (machine->resident_runs != NULL) && (machine->resident_leaf_bundles != NULL) &&
         (machine->resident_bundle_first != NULL) && (machine->resident_bundle_end != NULL) &&
         (machine->resident_contact_start != NULL) && (machine->resident_contacts != NULL);
    for (size_t resident = 0u; (ok != 0) && (resident < residents); resident += 1u)
    {
        machine->resident_frame[resident] = CLIMB_MACHINE_EMPTY;
    }
    const size_t rows = (size_t)extent->depth * extent->height;
    ok = ok && (cudaMalloc((void **)&machine->device_map, (size_t)geometry->voxels * sizeof(int)) == cudaSuccess);
    ok = ok && (cudaMemset(machine->device_map, 0xFF, (size_t)geometry->voxels * sizeof(int)) == cudaSuccess);
    ok = ok && (cudaMalloc((void **)&machine->device_peaks,
                           ((size_t)extent->peak_capacity + 1u) * sizeof(unsigned int)) == cudaSuccess);
    ok = ok &&
         machine_grow((void **)&machine->row_counts, (void **)&machine->device_row_counts, rows * sizeof(unsigned int));
    ok = ok && (cudaMalloc((void **)&machine->device_row_offsets, rows * sizeof(unsigned int)) == cudaSuccess);
    machine->leaf_counts = (unsigned int *)malloc(((size_t)extent->peak_capacity + 2u) * sizeof(unsigned int));
    ok = ok && (machine->leaf_counts != NULL);
    ok = ok && (cudaMalloc((void **)&machine->device_moved, 2u * sizeof(unsigned int)) == cudaSuccess);
    ok = ok && (cudaMallocHost((void **)&machine->pinned_moved, 2u * sizeof(unsigned int)) == cudaSuccess);
    ok = ok && (cudaMallocHost((void **)&machine->pinned_zero, sizeof(unsigned int)) == cudaSuccess);
    ok = ok && (cudaEventCreate(&machine->blocks_done[0]) == cudaSuccess);
    ok = ok && (cudaEventCreate(&machine->blocks_done[1]) == cudaSuccess);
    if (ok == 0)
    {
        climb_machine_close(machine);
        return NULL;
    }
    machine->pinned_zero[0] = 0u;
    machine->newest = CLIMB_MACHINE_EMPTY;
    return machine;
}

extern "C" int climb_machine_store(ClimbMachine *machine, const ClimbMachineFrame *frame)
{
    if ((machine == NULL) || (frame == NULL) || (frame->labels == NULL) || (frame->positive == NULL) ||
        (frame->leaf_count > machine->peak_capacity))
    {
        return 0;
    }
    unsigned int resident = machine_resident_of(machine, CLIMB_MACHINE_EMPTY);
    int ok = 1;
    if (resident == CLIMB_MACHINE_EMPTY)
    {
        ok = climb_machine_run(machine);
        for (unsigned int other_resident = 0u; (ok != 0) && (other_resident < machine->capacity); other_resident += 1u)
        {
            if (other_resident != machine->newest)
            {
                machine->resident_frame[other_resident] = CLIMB_MACHINE_EMPTY;
            }
        }
        resident = machine_resident_of(machine, CLIMB_MACHINE_EMPTY);
        ok = ok && (resident != CLIMB_MACHINE_EMPTY);
    }
    if (ok == 0)
    {
        return 0;
    }
    const MachineGeometry *const geometry = &machine->geometry;
    const size_t at = (size_t)resident;
    const unsigned int half = machine->scratch_next;
    unsigned int *const labels = &machine->scratch_labels[(size_t)half * (size_t)geometry->voxels];
    machine->scratch_frame[half] = CLIMB_MACHINE_EMPTY;
    ok = ok && (cudaMemcpy(labels, frame->labels, (size_t)geometry->voxels * sizeof(unsigned int),
                           cudaMemcpyHostToDevice) == cudaSuccess);
    machine->scratch_frame[half] = (ok != 0) ? frame->frame : CLIMB_MACHINE_EMPTY;
    machine->scratch_next = 1u - half;
    ok =
        ok && (cudaMemcpy(&machine->positive[at * (size_t)geometry->words], frame->positive,
                          (size_t)geometry->words * sizeof(unsigned long long), cudaMemcpyHostToDevice) == cudaSuccess);
    free(machine->resident_contact_start[resident]);
    free(machine->resident_contacts[resident]);
    machine->resident_contact_start[resident] = NULL;
    machine->resident_contacts[resident] = NULL;
    machine->resident_leaves[resident] = frame->leaf_count;
    machine->resident_runs[resident] = 0u;
    free(machine->resident_leaf_bundles[resident]);
    machine->resident_leaf_bundles[resident] = NULL;
    if ((ok != 0) && (frame->peaks != NULL))
    {
        ok = machine_split_runs(machine, resident, labels, frame);
    }
    if ((ok != 0) && (frame->contact_start != NULL) && (frame->contacts != NULL))
    {
        const size_t contacts = (size_t)frame->contact_start[frame->leaf_count];
        machine->resident_contact_start[resident] =
            (unsigned int *)malloc(((size_t)frame->leaf_count + 1u) * sizeof(unsigned int));
        machine->resident_contacts[resident] = (unsigned int *)malloc((contacts + 1u) * sizeof(unsigned int));
        ok = (machine->resident_contact_start[resident] != NULL) && (machine->resident_contacts[resident] != NULL);
        if (ok != 0)
        {
            memcpy(machine->resident_contact_start[resident], frame->contact_start,
                   ((size_t)frame->leaf_count + 1u) * sizeof(unsigned int));
            memcpy(machine->resident_contacts[resident], frame->contacts, contacts * sizeof(unsigned int));
        }
    }
    machine->resident_frame[resident] = (ok != 0) ? frame->frame : CLIMB_MACHINE_EMPTY;
    machine->newest = (ok != 0) ? resident : machine->newest;
    return ok;
}
