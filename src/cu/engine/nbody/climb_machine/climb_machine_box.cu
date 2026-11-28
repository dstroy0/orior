// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// climb_machine_box.cu: boxes released, ordered and merged
#include "../../../../../includes/platform/platform.h"
#include "climb_machine_internal.h"
#include "memoria_operor/memoria_operor.h"

void machine_box_release(ClimbMachine *machine)
{
    platform_cell_released(machine->box_earlier, platform_place_g_program_owner);
    platform_cell_released(machine->box_later, platform_place_g_program_owner);
    platform_cell_released(machine->box_leaf, platform_place_g_program_owner);
    platform_cell_released(machine->box_cell_first, platform_place_g_program_owner);
    platform_cell_released(machine->box_cell_shift, platform_place_g_program_owner);
    platform_cell_released(machine->box_entry_first, platform_place_g_program_owner);
    platform_cell_released(machine->box_target, platform_place_g_program_owner);
    platform_cell_released(machine->box_count, platform_place_g_program_owner);
    machine->box_earlier = NULL;
    machine->box_later = NULL;
    machine->box_leaf = NULL;
    machine->box_cell_first = NULL;
    machine->box_cell_shift = NULL;
    machine->box_entry_first = NULL;
    machine->box_target = NULL;
    machine->box_count = NULL;
}

static int machine_box_piece_order(const void *left, const void *right)
{
    const unsigned int one = ((const MachineBoxPiece *)left)->target;
    const unsigned int other = ((const MachineBoxPiece *)right)->target;
    return (one < other) ? -1 : ((one > other) ? 1 : 0);
}

static int machine_box_merge(ClimbMachine *machine, const unsigned int *cell_raw, size_t cell_count)
{
    size_t merged = 0u;
    size_t widest = 0u;
    for (size_t cell = 0u; cell < cell_count; cell += 1u)
    {
        const size_t size = (size_t)(machine->box_entry_first[cell + 1u] - machine->box_entry_first[cell]);
        widest = (size > widest) ? size : widest;
    }
    MachineBoxPiece *const pieces = (MachineBoxPiece *)platform_cell_taken(
        (widest + 1u) * sizeof(MachineBoxPiece), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (pieces == NULL)
    {
        return 0;
    }
    unsigned int from = 0u;
    for (size_t cell = 0u; cell < cell_count; cell += 1u)
    {
        const unsigned int end = machine->box_entry_first[cell + 1u];
        machine->box_entry_first[cell] = (unsigned int)merged;
        if (cell_raw[cell] == 0u)
        {
            for (unsigned int entry = from; entry < end; entry += 1u)
            {
                machine->box_target[merged] = machine->box_target[entry];
                machine->box_count[merged] = machine->box_count[entry];
                merged += 1u;
            }
            from = end;
            continue;
        }
        size_t piece_count = 0u;
        unsigned int outside = 0u;
        for (unsigned int entry = from; entry < end; entry += 1u)
        {
            const unsigned int none = (unsigned int)(machine->box_target[entry] == CLIMB_MACHINE_BOX_NONE);
            outside = (none != 0u) ? machine->box_count[entry] : outside;
            pieces[piece_count].target = machine->box_target[entry];
            pieces[piece_count].count = machine->box_count[entry];
            piece_count += (none != 0u) ? 0u : 1u;
        }
        {
            // the room of the sort, on the temporary tier, which goes back as the sort ends
            void *const sort_room =
                platform_cell_taken((unsigned long long)(piece_count) * (unsigned long long)(sizeof(MachineBoxPiece)),
                                    platform_place_g_program_owner, 1);
            (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = pieces, .bytes = sizeof(MachineBoxPiece),
                             .scratch = sort_room, .count = piece_count, .order = machine_box_piece_order);
            platform_cell_released(sort_room, platform_place_g_program_owner);
        }
        for (size_t piece = 0u; piece < piece_count; piece += 1u)
        {
            const int same = (piece != 0u) && (pieces[piece].target == pieces[piece - 1u].target);
            machine->box_target[merged - (same ? 1u : 0u)] = pieces[piece].target;
            machine->box_count[merged - (same ? 1u : 0u)] =
                same ? (machine->box_count[merged - 1u] + pieces[piece].count) : pieces[piece].count;
            merged += same ? 0u : 1u;
        }
        if (outside != 0u)
        {
            machine->box_target[merged] = CLIMB_MACHINE_BOX_NONE;
            machine->box_count[merged] = outside;
            merged += 1u;
        }
        from = end;
    }
    machine->box_entry_first[cell_count] = (unsigned int)merged;
    platform_cell_released(pieces, platform_place_g_program_owner);
    return 1;
}

extern "C" int climb_machine_box(ClimbMachine *machine, ClimbMachineBox *box)
{
    if ((machine == NULL) || (box == NULL))
    {
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = box, .bytes = sizeof(*box), .val = 0);
    machine_box_release(machine);
    const unsigned long long started = engine_clock_microseconds();
    unsigned int boxed = 0u;
    for (unsigned int pair = 0u; pair < machine->ran_count; pair += 1u)
    {
        boxed += machine->pending[pair].earlier_leaves;
    }
    const size_t capacity = (size_t)boxed + 1u;
    const unsigned long long cells = (unsigned long long)boxed * CLIMB_MACHINE_BOX_CELLS;
    const size_t cell_count = (cells <= 0x7FFFFFFFULL) ? (size_t)cells : 0u;
    machine->box_earlier =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->box_later =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->box_leaf =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->box_cell_first =
        (unsigned int *)platform_cell_taken((capacity + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    machine->box_cell_shift =
        (int *)platform_cell_taken((cell_count + 1u) * 3u * sizeof(int), platform_place_g_program_owner, 0);
    machine->box_entry_first = (unsigned int *)platform_cell_taken((cell_count + 1u) * sizeof(unsigned int),
                                                                   platform_place_g_program_owner, 0);
    unsigned int *const climber_of_box =
        (unsigned int *)platform_cell_taken(capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const cell_climber = (unsigned int *)platform_cell_taken((cell_count + 1u) * sizeof(unsigned int),
                                                                           platform_place_g_program_owner, 0);
    unsigned int *const cell_total = (unsigned int *)platform_cell_taken((cell_count + 1u) * sizeof(unsigned int),
                                                                         platform_place_g_program_owner, 0);
    unsigned int *const cell_raw = (unsigned int *)platform_cell_taken((cell_count + 1u) * sizeof(unsigned int),
                                                                       platform_place_g_program_owner, 0);
    int ok = (cells <= 0x7FFFFFFFULL) && (machine->box_earlier != NULL) && (machine->box_later != NULL) &&
             (machine->box_leaf != NULL) && (machine->box_cell_first != NULL) && (machine->box_cell_shift != NULL) &&
             (machine->box_entry_first != NULL) && (climber_of_box != NULL) && (cell_climber != NULL) &&
             (cell_total != NULL) && (cell_raw != NULL);
    unsigned int climber = 0u;
    unsigned int at = 0u;
    for (unsigned int pair = 0u; ok && (pair < machine->ran_count); pair += 1u)
    {
        const ClimbMachinePair *const pending = &machine->pending[pair];
        for (unsigned int leaf = 0u; leaf < pending->earlier_leaves; leaf += 1u)
        {
            const int *const center = &machine->centers[3u * (climber + leaf)];
            machine->box_earlier[at] = pending->earlier;
            machine->box_later[at] = pending->later;
            machine->box_leaf[at] = leaf;
            climber_of_box[at] = climber + leaf;
            machine->box_cell_first[at] = at * CLIMB_MACHINE_BOX_CELLS;
            unsigned int cell = at * CLIMB_MACHINE_BOX_CELLS;
            for (int step_z = -1; step_z <= 1; step_z += 1)
            {
                for (int step_y = -1; step_y <= 1; step_y += 1)
                {
                    for (int step_x = -1; step_x <= 1; step_x += 1)
                    {
                        machine->box_cell_shift[3u * cell] = center[0] + step_z;
                        machine->box_cell_shift[(3u * cell) + 1u] = center[1] + step_y;
                        machine->box_cell_shift[(3u * cell) + 2u] = center[2] + step_x;
                        cell_climber[cell] = at;
                        cell += 1u;
                    }
                }
            }
            machine->box_cell_shift[3u * cell] = pending->lag[0];
            machine->box_cell_shift[(3u * cell) + 1u] = pending->lag[1];
            machine->box_cell_shift[(3u * cell) + 2u] = pending->lag[2];
            cell_climber[cell] = at;
            at += 1u;
        }
        climber += pending->earlier_leaves + pending->later_leaves;
    }
    if (ok)
    {
        machine->box_cell_first[boxed] = (unsigned int)cell_count;
    }
    MachineBoxCells device;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &device, .bytes = sizeof(device), .val = 0);
    // The scratch of the box is temporary cells of the pool of the device, every byte 0 at the take. The call gives
    // them back last first before it returns
    TesseraJobCells job_cells;
    tessera_job_cells_begun(&job_cells, CLIMB_MACHINE_PLACE);
    const unsigned long long cell_words = ((unsigned long long)cell_count + 1ULL) * sizeof(unsigned int);
    void *room[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const unsigned long long bytes[8] = {cell_words,
                                         (unsigned long long)capacity * sizeof(unsigned int),
                                         ((unsigned long long)cell_count + 1ULL) * 3ULL * sizeof(int),
                                         cell_words,
                                         cell_words,
                                         cell_words,
                                         cell_words,
                                         2ULL * sizeof(unsigned int)};
    for (unsigned int which = 0u; ok && (which < 8u); which += 1u)
    {
        ok = tessera_job_cells_room(&job_cells, bytes[which], &room[which]);
    }
    // Explicit casts read each cell as the words that it holds
    unsigned int *const device_cell_climber = (unsigned int *)room[0];
    unsigned int *const device_climber_of_box = (unsigned int *)room[1];
    int *const device_cell_shift = (int *)room[2];
    unsigned int *const device_entry_first = (unsigned int *)room[3];
    unsigned int *const device_cell_entries = (unsigned int *)room[4];
    unsigned int *const device_cell_total = (unsigned int *)room[5];
    unsigned int *const device_cell_raw = (unsigned int *)room[6];
    unsigned int *const device_error = (unsigned int *)room[7];
    unsigned int *device_target = NULL;
    unsigned int *device_count = NULL;
    ok = ok &&
         tessera_job_crossed_into(device_cell_climber, cell_climber,
                                  (unsigned long long)cell_count * sizeof(unsigned int)) &&
         tessera_job_crossed_into(device_climber_of_box, climber_of_box,
                                  (unsigned long long)boxed * sizeof(unsigned int)) &&
         tessera_job_crossed_into(device_cell_shift, machine->box_cell_shift,
                                  (unsigned long long)cell_count * 3ULL * sizeof(int));
    device.cell_climber = device_cell_climber;
    device.climber_of_box = device_climber_of_box;
    device.cell_shift = device_cell_shift;
    device.entry_first = device_entry_first;
    device.cell_entries = device_cell_entries;
    device.cell_total = device_cell_total;
    device.cell_raw = device_cell_raw;
    device.error = device_error;
    const unsigned int cell_blocks = (unsigned int)((cell_count + CLIMB_MACHINE_BLOCK - 1u) / CLIMB_MACHINE_BLOCK);
    if (ok && (cell_count != 0u))
    {
        machine_box_kernel<<<cell_blocks, CLIMB_MACHINE_BLOCK>>>(
            (unsigned int)cell_count, 0u, device, machine->device_climber_side, machine->device_side_residents,
            machine->device_climber_entry_start, machine->device_entry_first, machine->device_entry_end,
            machine->row_first, machine->run_at_row, machine->run_start, machine->run_length, machine->run_leaf,
            (unsigned int)machine->run_capacity, machine->positive, machine->geometry);
        ok = machine_launched() && (cudaDeviceSynchronize() == cudaSuccess);
    }
    const unsigned long long counted_bytes = (unsigned long long)cell_count * sizeof(unsigned int);
    ok = ok && tessera_job_crossed_out(machine->box_entry_first, device_cell_entries, counted_bytes) &&
         tessera_job_crossed_out(cell_total, device_cell_total, counted_bytes) &&
         tessera_job_crossed_out(cell_raw, device_cell_raw, counted_bytes);
    unsigned long long entries = 0ULL;
    for (size_t cell = 0u; ok && (cell < cell_count); cell += 1u)
    {
        const unsigned int here = machine->box_entry_first[cell];
        machine->box_entry_first[cell] = (unsigned int)entries;
        entries += (unsigned long long)here;
    }
    ok = ok && (entries <= 0x7FFFFFFFULL);
    if (ok)
    {
        machine->box_entry_first[cell_count] = (unsigned int)entries;
    }
    const size_t entry_count = ok ? (size_t)entries : 0u;
    machine->box_target = ok ? (unsigned int *)platform_cell_taken((entry_count + 1u) * sizeof(unsigned int),
                                                                   platform_place_g_program_owner, 0)
                             : NULL;
    machine->box_count = ok ? (unsigned int *)platform_cell_taken((entry_count + 1u) * sizeof(unsigned int),
                                                                  platform_place_g_program_owner, 0)
                            : NULL;
    const unsigned long long entry_words = ((unsigned long long)entry_count + 1ULL) * sizeof(unsigned int);
    void *target_cell = NULL;
    void *count_cell = NULL;
    ok = ok && (machine->box_target != NULL) && (machine->box_count != NULL) &&
         tessera_job_cells_room(&job_cells, entry_words, &target_cell) &&
         tessera_job_cells_room(&job_cells, entry_words, &count_cell) &&
         tessera_job_crossed_into(device_entry_first, machine->box_entry_first, cell_words);
    // Explicit casts read the two cells as the words that they hold
    device_target = (unsigned int *)target_cell;
    device_count = (unsigned int *)count_cell;
    device.target = device_target;
    device.count = device_count;
    if (ok && (cell_count != 0u))
    {
        machine_box_kernel<<<cell_blocks, CLIMB_MACHINE_BLOCK>>>(
            (unsigned int)cell_count, 1u, device, machine->device_climber_side, machine->device_side_residents,
            machine->device_climber_entry_start, machine->device_entry_first, machine->device_entry_end,
            machine->row_first, machine->run_at_row, machine->run_start, machine->run_length, machine->run_leaf,
            (unsigned int)machine->run_capacity, machine->positive, machine->geometry);
        ok = machine_launched() && (cudaDeviceSynchronize() == cudaSuccess);
    }
    unsigned int error[2] = {0u, 0u};
    const unsigned long long target_bytes = (unsigned long long)entry_count * sizeof(unsigned int);
    ok = ok && tessera_job_crossed_out(machine->box_target, device_target, target_bytes) &&
         tessera_job_crossed_out(machine->box_count, device_count, target_bytes);
    const int counted =
        (device_error != NULL) && tessera_job_crossed_out(error, device_error, 2ULL * sizeof(unsigned int));
    box->climbers = boxed;
    box->cells = (unsigned int)((cells <= 0xFFFFFFFFULL) ? cells : 0xFFFFFFFFULL);
    box->crowded = counted ? error[0] : 0u;
    box->broken = counted ? error[1] : 0u;
    ok = ok && counted && (error[1] == 0u) && machine_box_merge(machine, cell_raw, cell_count);
    unsigned int final_score_differ = 0u;
    unsigned int not_highest = 0u;
    for (unsigned int one = 0u; ok && (one < boxed); one += 1u)
    {
        const unsigned int first = machine->box_cell_first[one];
        const unsigned int top = cell_total[first + CLIMB_MACHINE_BOX_CENTER];
        final_score_differ += (top != machine->final_score[climber_of_box[one]]) ? 1u : 0u;
        unsigned int higher = 0u;
        for (unsigned int near = 0u; near < CLIMB_MACHINE_BOX_NEIGHBORS; near += 1u)
        {
            higher += (cell_total[first + near] > top) ? 1u : 0u;
        }
        not_highest += (higher != 0u) ? 1u : 0u;
    }
    tessera_job_cells_given(&job_cells);
    platform_cell_released(cell_climber, platform_place_g_program_owner);
    platform_cell_released(cell_total, platform_place_g_program_owner);
    platform_cell_released(cell_raw, platform_place_g_program_owner);
    platform_cell_released(climber_of_box, platform_place_g_program_owner);
    if (ok == 0)
    {
        machine_box_release(machine);
        return 0;
    }
    box->earlier = machine->box_earlier;
    box->later = machine->box_later;
    box->leaf = machine->box_leaf;
    box->cell_first = machine->box_cell_first;
    box->cell_shift = machine->box_cell_shift;
    box->entry_first = machine->box_entry_first;
    box->target = machine->box_target;
    box->count = machine->box_count;
    box->cells = (unsigned int)cell_count;
    box->entries = machine->box_entry_first[cell_count];
    box->final_score_differ = final_score_differ;
    box->not_highest = not_highest;
    box->microseconds = engine_clock_microseconds() - started;
    return 1;
}
