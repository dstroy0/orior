// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tower_job.h: the lift and the lower of the tower as stages of a job of the device (tessera_job.h). The host crosses a
// stage in, and the kernel of the job runs it as a device function between its other stages. After the launch, the
// host agrees with the answer of the stage. The record of a stage holds the cells of the device that the host and the
// kernel share. The host reads none of their bytes, and gives them to the crossings and the kernel alone.
#ifndef TOWER_JOB_H
#define TOWER_JOB_H

#include "../../runtime/daemon/tessera_job.h"
#include "tower.h"

#include <cooperative_groups.h>

// A stage of the tower. `job` is the cell of the record that the device function reads. `coefficients` is the cell of
// the coefficients: the answer of a lift, and the crystal of a lower. `answer` is the cell of the flags, and `rebuilt`
// the cell of the samples of a lower. `count` is the lanes of the extent, and `floors` the floors of the tower
typedef struct
{
    const void *job;
    int *coefficients;
    void *answer;
    unsigned short *rebuilt;
    unsigned long long count;
    unsigned int floors;
} TowerStage;

// The lift crossed in as a stage. The lattice and the tables of the edges cross in, and the cells of the coefficients
// and the flag are rooms. Where `request->coefficients` is NULL, the coefficients stay on the device for a later stage.
// 1, or 0 with the error set
int tower_lift_stage_crossed(const TowerLiftJobRequest *request, TesseraJobCells *cells, TowerStage *stage);

// The host agrees with the flag of the lift, as tower_lift does. The coefficients cross out where
// `request->coefficients` is not NULL, and the floors go in `*request->floors`. 1, or 0 with the error set
int tower_lift_stage_agreed(const TowerLiftJobRequest *request, const TowerStage *stage);

// The lower crossed in as a stage. The tables of the edges and the lanes cross in. Where `request->coefficients` is
// NULL, the crystal is a room that an earlier stage writes. 1, or 0 with the error set
int tower_lower_stage_crossed(const TowerLowerJobRequest *request, TesseraJobCells *cells, TowerStage *stage);

// The host agrees with the flags of the lower, as tower_lower does. The rebuilt samples cross out into
// `request->rebuilt`, and the count of the samples that differ into `*request->mismatches`. 1, or 0 with the error set
int tower_lower_stage_agreed(const TowerLowerJobRequest *request, const TowerStage *stage);

// The lift on the grid of the kernel, over the record `job` of its stage. 1 where the lift leaves no flag, after a sync
// of the grid. A later stage then reads the coefficients
__device__ int tower_lift_staged(const void *job, const cooperative_groups::grid_group &grid);

// The lower on the grid of the kernel, over the record `job` of its stage. 1 where the lower leaves no flag, after a
// sync of the grid
__device__ int tower_lower_staged(const void *job, const cooperative_groups::grid_group &grid);

#endif
