// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compression_job.h: the encode and the decode of compression as stages of a job of the device (tessera_job.h). The
// host crosses a stage in, and the kernel of the job runs it as a device function between its other stages. After the
// launch, the host agrees with the answer of the stage. The record of a stage holds the cells of the device that the
// host and the kernel share. The host reads none of their bytes, and gives them to the crossings and the kernel alone.
#ifndef COMPRESSION_JOB_H
#define COMPRESSION_JOB_H

#include "../../runtime/daemon/tessera_job.h"
#include "compression.h"

#include <cooperative_groups.h>

// A stage of compression. `job` is the cell of the record that the device function reads, and `answer` the cell of
// the answer of an encode. `coefficients` is the cell of the values. `count` is the values, and `chunks` their chunks
typedef struct
{
    const void *job;
    void *answer;
    int *coefficients;
    unsigned long long count;
    unsigned long long chunks;
} CompressionStage;

// The encode crossed in as a stage. Where `device_coefficients` is not NULL, the values are that cell, which an earlier
// stage writes. Else `request->coefficients` cross in. 1, or 0 with the error set
int compression_encode_stage_crossed(const CompressionEncodeJobRequest *request, int *device_coefficients,
                                     TesseraJobCells *cells, CompressionStage *stage);

// The host agrees with the answer of the encode. The offsets and the stream cross out into the host cells that
// compression keeps until its next encode, as compression_encode gives them. The cells that the kernel took go in
// `cells`, for their give back. 1, or 0 with the error set
int compression_encode_stage_agreed(const CompressionEncodeJobRequest *request, const CompressionStage *stage,
                                    TesseraJobCells *cells);

// The decode crossed in as a stage. The offsets and the stream cross in. Where `device_coefficients` is not NULL, the
// values go into that cell, for a later stage. Else they go into a room. 1, or 0 with the error set
int compression_decode_stage_crossed(const CompressionDecodeJobRequest *request, int *device_coefficients,
                                     TesseraJobCells *cells, CompressionStage *stage);

// The values of the decode crossed out into `request->coefficients`, where it is not NULL. 1, or 0 with the error set
int compression_decode_stage_agreed(const CompressionDecodeJobRequest *request, const CompressionStage *stage);

// The encode on the grid of the kernel, over the record `job` of its stage. 1 where the device gives each cell, after
// a sync of the grid
__device__ int compression_encode_staged(const void *job, const cooperative_groups::grid_group &grid);

// The decode on the grid of the kernel, over the record `job` of its stage. 1, after a sync of the grid. A later stage
// then reads the values
__device__ int compression_decode_staged(const void *job, const cooperative_groups::grid_group &grid);

#endif
