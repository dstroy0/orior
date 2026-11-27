// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// climb_machine_score.cu: scoring and deciding
#include "climb_machine_internal.h"

#include <cooperative_groups.h>

__device__ static void machine_score_entry(const MachineClimbJob *job, unsigned int entry)
{
    const unsigned int climber = job->entry_climber[entry];
    if (job->active[climber] == 0u)
    {
        return;
    }
    const MachineGeometry geometry = job->geometry;
    const unsigned int side = job->climber_side[climber];
    const unsigned long long own_word = (unsigned long long)job->side_residents[2u * side] * geometry.words;
    const unsigned long long other_word = (unsigned long long)job->side_residents[(2u * side) + 1u] * geometry.words;
    const int *const center = &job->centers[3u * climber];
    const unsigned int plane = geometry.height * geometry.width;
    unsigned int counts[CLIMB_MACHINE_CANDIDATES];
    for (unsigned int candidate = 0u; candidate < CLIMB_MACHINE_CANDIDATES; candidate += 1u)
    {
        counts[candidate] = 0u;
    }
    for (unsigned int run = job->entry_first[entry]; run < job->entry_end[entry]; run += 1u)
    {
        const unsigned int start = job->run_start[run];
        const long long length = (long long)job->run_length[run];
        const unsigned int rest = start % plane;
        const long long base_z = (long long)(start / plane) + (long long)center[0];
        const long long base_y = (long long)(rest / geometry.width) + (long long)center[1];
        const long long base_x = (long long)(rest % geometry.width) + (long long)center[2];
        unsigned int candidate = 0u;
        for (long long step_z = -1LL; step_z <= 1LL; step_z += 1LL)
        {
            const long long z = base_z + step_z;
            const int inside_z = (z >= 0LL) && (z < (long long)geometry.depth);
            for (long long step_y = -1LL; step_y <= 1LL; step_y += 1LL)
            {
                const long long y = base_y + step_y;
                const int inside_y = (y >= 0LL) && (y < (long long)geometry.height);
                for (long long step_x = -1LL; step_x <= 1LL; step_x += 1LL)
                {
                    const long long x = base_x + step_x;
                    const long long first = (x < 0LL) ? -x : 0LL;
                    const long long width_left = (long long)geometry.width - x;
                    const long long end = (width_left < length) ? width_left : length;
                    if ((inside_z != 0) && (inside_y != 0) && (first < end))
                    {
                        const unsigned long long row =
                            (unsigned long long)((z * (long long)geometry.height + y) * (long long)geometry.width);
                        counts[candidate] +=
                            machine_agreeing(job->positive, geometry.words, own_word,
                                             (unsigned long long)start + (unsigned long long)first, other_word,
                                             row + (unsigned long long)(x + first), (unsigned int)(end - first));
                    }
                    candidate += 1u;
                }
            }
        }
    }
    for (unsigned int candidate = 0u; candidate < CLIMB_MACHINE_CANDIDATES; candidate += 1u)
    {
        job->scores[((unsigned long long)entry * CLIMB_MACHINE_CANDIDATES) + candidate] = counts[candidate];
    }
}

__device__ static void machine_decide_climber(const MachineClimbJob *job, unsigned int climber)
{
    if (job->active[climber] == 0u)
    {
        return;
    }
    const MachineGeometry geometry = job->geometry;
    long long sums[CLIMB_MACHINE_CANDIDATES];
    for (unsigned int candidate = 0u; candidate < CLIMB_MACHINE_CANDIDATES; candidate += 1u)
    {
        sums[candidate] = 0LL;
    }
    for (unsigned int entry = job->climber_entry_start[climber]; entry < job->climber_entry_start[climber + 1u];
         entry += 1u)
    {
        for (unsigned int candidate = 0u; candidate < CLIMB_MACHINE_CANDIDATES; candidate += 1u)
        {
            sums[candidate] +=
                (long long)job->scores[((unsigned long long)entry * CLIMB_MACHINE_CANDIDATES) + candidate];
        }
    }
    int *const center = &job->centers[3u * climber];
    const long long here[3] = {(long long)center[0], (long long)center[1], (long long)center[2]};
    long long best[3] = {here[0], here[1], here[2]};
    long long best_score = sums[13];
    unsigned long long best_length = 0ULL;
    int stepped = 0;
    unsigned int candidate = 0u;
    for (long long step_z = -1LL; step_z <= 1LL; step_z += 1LL)
    {
        for (long long step_y = -1LL; step_y <= 1LL; step_y += 1LL)
        {
            for (long long step_x = -1LL; step_x <= 1LL; step_x += 1LL)
            {
                const long long lag[3] = {here[0] + step_z, here[1] + step_y, here[2] + step_x};
                const unsigned long long length =
                    (unsigned long long)geometry.weight_z * (unsigned long long)(lag[0] * lag[0]) +
                    (unsigned long long)(lag[1] * lag[1]) + (unsigned long long)(lag[2] * lag[2]);
                if ((candidate != 13u) &&
                    ((sums[candidate] > best_score) ||
                     ((stepped != 0) && (sums[candidate] == best_score) && (length < best_length))))
                {
                    best_score = sums[candidate];
                    best_length = length;
                    best[0] = lag[0];
                    best[1] = lag[1];
                    best[2] = lag[2];
                    stepped = 1;
                }
                candidate += 1u;
            }
        }
    }
    if (stepped != 0)
    {
        center[0] = (int)best[0];
        center[1] = (int)best[1];
        center[2] = (int)best[2];
        job->moved[0] = 1u;
    }
    else
    {
        job->active[climber] = 0u;
        job->final_score[climber] = (unsigned int)sums[13];
    }
}

// The climb as one job of the device. Each round clears the mark, then runs CLIMB_MACHINE_TICKS ticks. Each tick
// scores every entry, then lets each climber step to its best candidate. A sync of the grid follows each step. A
// climber that does not step stops, with its score at its center. The grid reads the mark after the round, and a
// round that moved no climber ends the loop
__global__ void machine_climb_job_kernel(MachineClimbJob job)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    const unsigned long long first = grid.thread_rank();
    const unsigned long long jump = grid.size();
    int settled = 0;
    while (settled == 0)
    {
        if (first == 0ULL)
        {
            job.moved[0] = 0u;
        }
        grid.sync();
        for (unsigned int tick = 0u; tick < CLIMB_MACHINE_TICKS; tick += 1u)
        {
            for (unsigned long long entry = first; entry < job.entries; entry += jump)
            {
                // Explicit cast takes an entry below the count, which an unsigned int holds
                machine_score_entry(&job, (unsigned int)entry);
            }
            grid.sync();
            for (unsigned long long climber = first; climber < job.climbers; climber += jump)
            {
                // Explicit cast takes a climber below the count, which an unsigned int holds
                machine_decide_climber(&job, (unsigned int)climber);
            }
            grid.sync();
        }
        settled = (job.moved[0] == 0u) ? 1 : 0;
        // each thread reads the mark before the first thread clears it for the next round
        grid.sync();
    }
}

int machine_climbed(const MachineClimbJob *job)
{
    MachineClimbJob arguments = *job;
    void *args[1] = {(void *)&arguments};
    // Explicit cast gives the address of the kernel as the job that tessera runs
    return tessera_job_ran((const void *)machine_climb_job_kernel, CLIMB_MACHINE_BLOCK, args);
}
