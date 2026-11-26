// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef TESSERA_JOB_H
#define TESSERA_JOB_H

// The job interface of tessera on the device. The bytes of a job cross the host and the device here and nowhere else.
// A crossing in takes a temporary cell of the pool of the device (platform_device.h) and copies the bytes into it. A
// crossing out copies the bytes of a cell back to the host, which then agrees with them. The host does not read or
// write through the address of a cell. It gives the address to the job kernel and to the crossings alone.
//
// A job runs as one cooperative launch of its kernel, as many blocks as the device holds at once. The kernel syncs the
// grid between the stages of the job, and its device code decides each stage. The cells of the job are on the temporary
// tier. The job thus gives them back last first: the cells of its kernel, then its crossings.

#ifdef __cplusplus
extern "C"
{
#endif

    // `count` bytes of `bytes` copied into a temporary cell of the pool of the device, counted to the owner at
    // `place`. The address of the cell goes in `*cell`. 1, or 0 where the pool cannot meet the bytes or a copy errors
    int tessera_job_crossed_in(const void *bytes, unsigned long long count, unsigned int place, void **cell);

    // An empty temporary cell of `count` bytes in the pool of the device, every byte 0, for the job to write its
    // answer into. 1, or 0 where the pool cannot meet the bytes
    int tessera_job_cell_taken(unsigned long long count, unsigned int place, void **cell);

    // `count` bytes of `bytes` copied into the start of a cell that the job holds, such as a room of
    // tessera_job_cell_taken with more bytes than the copy. 1, or 0 where the copy errors
    int tessera_job_crossed_into(void *cell, const void *bytes, unsigned long long count);

    // `count` bytes of the cell copied back into `bytes` on the host: 1, or 0 where the copy errors
    int tessera_job_crossed_out(void *bytes, const void *cell, unsigned long long count);

    // the cell given back to the pool of the device by the owner at `place`, the newest cell of the job first
    void tessera_job_given(void *cell, unsigned int place);

    // The job kernel `kernel` launched once over as many blocks of `threads` threads as the device holds at once, with
    // its arguments `args`, and waited for. The call sets the stack of each thread first, as MMgr's CUDA guide asks. 1,
    // or 0 where the launch or the kernel errors
    int tessera_job_ran(const void *kernel, unsigned int threads, void **args);

// the cells that one call of a job holds at max
#define TESSERA_JOB_CELLS_MAX 96u

    // The cells that one call of a job holds, counted to the owner at `place`, in the order that it took them. The
    // call gives them back at its end, the newest first
    typedef struct
    {
        void *cell[TESSERA_JOB_CELLS_MAX];
        unsigned int count;
        unsigned int place;
    } TesseraJobCells;

    // the list empty, for the owner at `place`
    void tessera_job_cells_begun(TesseraJobCells *cells, unsigned int place);

    // `count` bytes of `bytes` crossed in as tessera_job_crossed_in crosses them, the cell kept in the list. 1, or 0
    // where the list is full or the crossing errors
    int tessera_job_cells_in(TesseraJobCells *cells, const void *bytes, unsigned long long count, void **cell);

    // an empty cell of `count` bytes taken as tessera_job_cell_taken takes it, the cell kept in the list. 1, or 0
    // where the list is full or the take errors
    int tessera_job_cells_room(TesseraJobCells *cells, unsigned long long count, void **cell);

    // A cell that the job kernel took and left for the host, kept in the list for its give back. 1, or 0 where the
    // list is full. A NULL cell is not kept, and the call gives 1
    int tessera_job_cells_kept(TesseraJobCells *cells, void *cell);

    // every cell of the list given back, the newest first, and the list empty
    void tessera_job_cells_given(TesseraJobCells *cells);

#ifdef __cplusplus
}
#endif

#endif
