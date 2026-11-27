// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// climb_machine_open.cu: the runs cut, the machine opened and a frame stored
#include "../../../../../includes/platform/platform.h"
#include "climb_machine_internal.h"
#include "memoria_operor/memoria_operor.h"

static int machine_split_runs(ClimbMachine *machine, unsigned int resident, const unsigned int *labels,
                              const ClimbMachineFrame *frame)
{
    const MachineGeometry geometry = machine->geometry;
    const unsigned int rows = geometry.depth * geometry.height;
    const unsigned int leaves = frame->leaf_count;
    const unsigned int row_blocks = (rows + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK;
    const unsigned int leaf_blocks = (leaves + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK;
    int ok = tessera_job_crossed_into(machine->device_peaks, frame->peaks,
                                      (unsigned long long)leaves * sizeof(unsigned int));
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
    const unsigned long long row_bytes = (unsigned long long)rows * sizeof(unsigned int);
    ok = ok && tessera_job_crossed_out(machine->row_counts, machine->device_row_counts, row_bytes);
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
        const unsigned long long capacity_bytes = (unsigned long long)capacity * sizeof(unsigned int);
        machine_cell_given((void **)&machine->device_unsorted_leaf);
        machine_cell_given((void **)&machine->device_unsorted_length);
        machine_cell_given((void **)&machine->device_unsorted_start);
        ok = ok && machine_cell_taken((void **)&machine->device_unsorted_start, capacity_bytes) &&
             machine_cell_taken((void **)&machine->device_unsorted_length, capacity_bytes) &&
             machine_cell_taken((void **)&machine->device_unsorted_leaf, capacity_bytes);
        ok = ok &&
             machine_grow((void **)&machine->order, (void **)&machine->device_order, capacity * sizeof(unsigned int));
        platform_cell_released(machine->unsorted_leaf, platform_place_g_program_owner);
        machine->unsorted_leaf =
            (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
        ok = ok && (machine->unsorted_leaf != NULL);
        machine->unsorted_capacity = (ok != 0) ? capacity : 0u;
    }
    ok = ok && tessera_job_crossed_into(machine->device_row_offsets, machine->row_counts, row_bytes);
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
    ok = ok && tessera_job_crossed_out(machine->unsorted_leaf, machine->device_unsorted_leaf,
                                       (unsigned long long)runs * sizeof(unsigned int));
    if (ok == 0)
    {
        return 0;
    }

    unsigned int *const counts = machine->leaf_counts;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = counts, .bytes = ((size_t)leaves + 1u) * sizeof(unsigned int), .val = 0);
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
        ok = ok && ((residents * capacity) <= 0xFFFFFFFFULL);
        for (unsigned int which = 0u; (ok != 0) && (which < 4u); which += 1u)
        {
            ok = machine_cell_taken((void **)&grown[which],
                                    (unsigned long long)residents * capacity * sizeof(unsigned int));
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
                ok = tessera_job_copied(&grown[which][(size_t)other_resident * capacity],
                                        &current[which][(size_t)other_resident * machine->run_capacity],
                                        (unsigned long long)count * sizeof(unsigned int));
            }
        }
        for (unsigned int which = 4u; which > 0u; which -= 1u)
        {
            void *old = current[which - 1u];
            machine_cell_given(&old);
        }
        machine->run_start = grown[0];
        machine->run_length = grown[1];
        machine->run_leaf = grown[2];
        machine->run_at_row = grown[3];
        machine->run_capacity = (ok != 0) ? capacity : 0u;
    }
    ok = ok && tessera_job_crossed_into(machine->device_order, machine->order,
                                        (unsigned long long)runs * sizeof(unsigned int));
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
        ok = tessera_job_crossed_into(&machine->row_first[offsets], machine->row_counts, row_bytes) &&
             tessera_job_crossed_into(&machine->row_first[offsets + rows], &total, sizeof(unsigned int));
    }

    platform_cell_released(machine->resident_leaf_bundles[resident], platform_place_g_program_owner);
    platform_cell_released(machine->resident_bundle_first[resident], platform_place_g_program_owner);
    platform_cell_released(machine->resident_bundle_end[resident], platform_place_g_program_owner);
    machine->resident_leaf_bundles[resident] = (unsigned int *)platform_cell_taken(
        ((size_t)leaves + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    const size_t bundle_capacity = (runs / CLIMB_MACHINE_BUNDLE) + (size_t)leaves + 1u;
    machine->resident_bundle_first[resident] =
        (unsigned int *)platform_cell_taken(bundle_capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->resident_bundle_end[resident] =
        (unsigned int *)platform_cell_taken(bundle_capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
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
    ClimbMachine *const machine =
        (ClimbMachine *)platform_cell_cleared(1u, sizeof(ClimbMachine), platform_place_g_program_owner, 0);
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
    geometry->words = (geometry->voxels + 63ULL) / 64ULL;
    machine->peak_capacity = extent->peak_capacity;
    machine->capacity = (extent->frames < 2u) ? 2u : extent->frames;
    const size_t residents = (size_t)machine->capacity;
    // The cells of the device that the machine holds from its open to its close are persistent cells of the pool of
    // the device. The cells that grow with the frames go back and come again at the new size
    int ok = 1;
    ok = ok && machine_cell_taken((void **)&machine->scratch_labels, 2ULL * geometry->voxels * sizeof(unsigned int));
    machine->scratch_frame[0] = CLIMB_MACHINE_EMPTY;
    machine->scratch_frame[1] = CLIMB_MACHINE_EMPTY;
    ok = ok && machine_cell_taken((void **)&machine->positive,
                                  (unsigned long long)residents * geometry->words * sizeof(unsigned long long));
    ok = ok &&
         machine_cell_taken((void **)&machine->row_first,
                            (unsigned long long)residents *
                                (((unsigned long long)extent->depth * extent->height) + 1ULL) * sizeof(unsigned int));
    machine->resident_frame =
        (unsigned int *)platform_cell_taken(residents * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->resident_leaves =
        (unsigned int *)platform_cell_cleared(residents, sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->resident_runs =
        (unsigned int *)platform_cell_cleared(residents, sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->resident_leaf_bundles =
        (unsigned int **)platform_cell_cleared(residents, sizeof(unsigned int *), platform_place_g_program_owner, 0);
    machine->resident_bundle_first =
        (unsigned int **)platform_cell_cleared(residents, sizeof(unsigned int *), platform_place_g_program_owner, 0);
    machine->resident_bundle_end =
        (unsigned int **)platform_cell_cleared(residents, sizeof(unsigned int *), platform_place_g_program_owner, 0);
    machine->resident_contact_start =
        (unsigned int **)platform_cell_cleared(residents, sizeof(unsigned int *), platform_place_g_program_owner, 0);
    machine->resident_contacts =
        (unsigned int **)platform_cell_cleared(residents, sizeof(unsigned int *), platform_place_g_program_owner, 0);
    ok = ok && (machine->resident_frame != NULL) && (machine->resident_leaves != NULL) &&
         (machine->resident_runs != NULL) && (machine->resident_leaf_bundles != NULL) &&
         (machine->resident_bundle_first != NULL) && (machine->resident_bundle_end != NULL) &&
         (machine->resident_contact_start != NULL) && (machine->resident_contacts != NULL);
    for (size_t resident = 0u; (ok != 0) && (resident < residents); resident += 1u)
    {
        machine->resident_frame[resident] = CLIMB_MACHINE_EMPTY;
    }
    const size_t rows = (size_t)extent->depth * extent->height;
    // every leaf of the map starts at -1, every byte 0xFF
    ok = ok && machine_cell_taken((void **)&machine->device_map, geometry->voxels * sizeof(int)) &&
         machine_filled(machine->device_map, geometry->voxels, 0xFFFFFFFFu);
    ok = ok && machine_cell_taken((void **)&machine->device_peaks,
                                  ((unsigned long long)extent->peak_capacity + 1ULL) * sizeof(unsigned int));
    ok = ok &&
         machine_grow((void **)&machine->row_counts, (void **)&machine->device_row_counts, rows * sizeof(unsigned int));
    ok = ok &&
         machine_cell_taken((void **)&machine->device_row_offsets, (unsigned long long)rows * sizeof(unsigned int));
    machine->leaf_counts = (unsigned int *)platform_cell_taken(
        ((size_t)extent->peak_capacity + 2u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = ok && (machine->leaf_counts != NULL);
    ok = ok && machine_cell_taken((void **)&machine->device_moved, 2ULL * sizeof(unsigned int));
    if (ok == 0)
    {
        climb_machine_close(machine);
        return NULL;
    }
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
    ok = ok && tessera_job_crossed_into(labels, frame->labels, geometry->voxels * sizeof(unsigned int));
    machine->scratch_frame[half] = (ok != 0) ? frame->frame : CLIMB_MACHINE_EMPTY;
    machine->scratch_next = 1u - half;
    ok = ok && tessera_job_crossed_into(&machine->positive[at * (size_t)geometry->words], frame->positive,
                                        geometry->words * sizeof(unsigned long long));
    platform_cell_released(machine->resident_contact_start[resident], platform_place_g_program_owner);
    platform_cell_released(machine->resident_contacts[resident], platform_place_g_program_owner);
    machine->resident_contact_start[resident] = NULL;
    machine->resident_contacts[resident] = NULL;
    machine->resident_leaves[resident] = frame->leaf_count;
    machine->resident_runs[resident] = 0u;
    platform_cell_released(machine->resident_leaf_bundles[resident], platform_place_g_program_owner);
    machine->resident_leaf_bundles[resident] = NULL;
    if ((ok != 0) && (frame->peaks != NULL))
    {
        ok = machine_split_runs(machine, resident, labels, frame);
    }
    if ((ok != 0) && (frame->contact_start != NULL) && (frame->contacts != NULL))
    {
        const size_t contacts = (size_t)frame->contact_start[frame->leaf_count];
        machine->resident_contact_start[resident] = (unsigned int *)platform_cell_taken(
            ((size_t)frame->leaf_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
        machine->resident_contacts[resident] = (unsigned int *)platform_cell_taken(
            (contacts + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
        ok = (machine->resident_contact_start[resident] != NULL) && (machine->resident_contacts[resident] != NULL);
        if (ok != 0)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = machine->resident_contact_start[resident],
                       .src = frame->contact_start, .bytes = ((size_t)frame->leaf_count + 1u) * sizeof(unsigned int));
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = machine->resident_contacts[resident], .src = frame->contacts,
                       .bytes = contacts * sizeof(unsigned int));
        }
    }
    machine->resident_frame[resident] = (ok != 0) ? frame->frame : CLIMB_MACHINE_EMPTY;
    machine->newest = (ok != 0) ? resident : machine->newest;
    return ok;
}
