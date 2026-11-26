// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_job.cu: the crossings of a job and its one cooperative launch (tessera_job.h). A kernel of one thread takes
// and gives back each crossing cell on the device, and the host reads back the address alone. The copies of the bytes
// of the job are the only copies between the host and the device in the engine.
#include "tessera_job.h"

#include "../../../../platform/platform_device.h"

#include <cuda_runtime.h>

// The bytes of the stack of each device thread of a job. The frame of cellul.find alone takes 1344 on sm_86, and a job
// calls MMgr through its cellblock
#define TESSERA_JOB_STACK_BYTES (64u * 1024u)

// the cell that the last take on the device gave, which the host reads back after the kernel of the take ends
static __device__ void *s_job_cell;

// One thread: the pool placed, and a temporary cell of `count` bytes taken, cleared where `cleared` is not 0
__global__ void tessera_job_taken_kernel(unsigned long long count, unsigned int place, int cleared)
{
    if (platform_device_open() == 0)
    {
        s_job_cell = NULL;
        return;
    }
    s_job_cell =
        (cleared != 0) ? platform_device_cell_cleared(count, place, 1) : platform_device_cell_taken(count, place, 1);
}

// One thread: the cell given back
__global__ void tessera_job_given_kernel(void *cell, unsigned int place)
{
    platform_device_cell_released(cell, place);
}

// A temporary cell of `count` bytes taken on the device, its address read back into `*cell`. 1, or 0 where the pool
// refuses the bytes or the kernel errors
static int tessera_job_taken(unsigned long long count, unsigned int place, int cleared, void **cell)
{
    *cell = NULL;
    tessera_job_taken_kernel<<<1, 1>>>(count, place, cleared);
    if ((cudaGetLastError() != cudaSuccess) || (cudaDeviceSynchronize() != cudaSuccess) ||
        (cudaMemcpyFromSymbol(cell, s_job_cell, sizeof(*cell)) != cudaSuccess))
    {
        *cell = NULL;
        return 0;
    }
    return (*cell != NULL) || (count == 0ULL);
}

extern "C" int tessera_job_crossed_in(const void *bytes, unsigned long long count, unsigned int place, void **cell)
{
    if (!tessera_job_taken(count, place, 0, cell))
    {
        return 0;
    }
    // Explicit cast takes the count of bytes in memory, which a size_t counts
    return (count == 0ULL) || (cudaMemcpy(*cell, bytes, (size_t)count, cudaMemcpyHostToDevice) == cudaSuccess);
}

extern "C" int tessera_job_cell_taken(unsigned long long count, unsigned int place, void **cell)
{
    return tessera_job_taken(count, place, 1, cell);
}

extern "C" int tessera_job_crossed_into(void *cell, const void *bytes, unsigned long long count)
{
    // Explicit cast takes the count of bytes in memory, which a size_t counts
    return (count == 0ULL) ||
           ((cell != NULL) && (cudaMemcpy(cell, bytes, (size_t)count, cudaMemcpyHostToDevice) == cudaSuccess));
}

extern "C" int tessera_job_crossed_out(void *bytes, const void *cell, unsigned long long count)
{
    // Explicit cast takes the count of bytes in memory, which a size_t counts
    return (count == 0ULL) ||
           ((cell != NULL) && (cudaMemcpy(bytes, cell, (size_t)count, cudaMemcpyDeviceToHost) == cudaSuccess));
}

extern "C" void tessera_job_given(void *cell, unsigned int place)
{
    if (cell == NULL)
    {
        return;
    }
    tessera_job_given_kernel<<<1, 1>>>(cell, place);
    (void)cudaDeviceSynchronize();
}

extern "C" int tessera_job_ran(const void *kernel, unsigned int threads, void **args)
{
    int device = 0;
    int cooperative = 0;
    int processors = 0;
    int blocks_each = 0;
    if ((cudaGetDevice(&device) != cudaSuccess) ||
        (cudaDeviceGetAttribute(&cooperative, cudaDevAttrCooperativeLaunch, device) != cudaSuccess) ||
        (cooperative == 0) ||
        (cudaDeviceGetAttribute(&processors, cudaDevAttrMultiProcessorCount, device) != cudaSuccess) ||
        (cudaDeviceSetLimit(cudaLimitStackSize, TESSERA_JOB_STACK_BYTES) != cudaSuccess) ||
        (cudaOccupancyMaxActiveBlocksPerMultiprocessor(&blocks_each, kernel, (int)threads, 0) != cudaSuccess) ||
        (blocks_each <= 0))
    {
        return 0;
    }
    // Explicit cast takes the count of blocks that the device holds at once, a positive int, as the grid's width
    const dim3 grid((unsigned int)(blocks_each * processors));
    return (cudaLaunchCooperativeKernel(kernel, grid, dim3(threads), args, 0, NULL) == cudaSuccess) &&
           (cudaDeviceSynchronize() == cudaSuccess);
}

extern "C" void tessera_job_cells_begun(TesseraJobCells *cells, unsigned int place)
{
    cells->count = 0u;
    cells->place = place;
}

extern "C" int tessera_job_cells_kept(TesseraJobCells *cells, void *cell)
{
    if (cell == NULL)
    {
        return 1;
    }
    if (cells->count >= TESSERA_JOB_CELLS_MAX)
    {
        return 0;
    }
    cells->cell[cells->count] = cell;
    cells->count += 1u;
    return 1;
}

extern "C" int tessera_job_cells_in(TesseraJobCells *cells, const void *bytes, unsigned long long count, void **cell)
{
    *cell = NULL;
    return (cells->count < TESSERA_JOB_CELLS_MAX) && tessera_job_crossed_in(bytes, count, cells->place, cell) &&
           tessera_job_cells_kept(cells, *cell);
}

extern "C" int tessera_job_cells_room(TesseraJobCells *cells, unsigned long long count, void **cell)
{
    *cell = NULL;
    return (cells->count < TESSERA_JOB_CELLS_MAX) && tessera_job_cell_taken(count, cells->place, cell) &&
           tessera_job_cells_kept(cells, *cell);
}

extern "C" void tessera_job_cells_given(TesseraJobCells *cells)
{
    while (cells->count > 0u)
    {
        cells->count -= 1u;
        tessera_job_given(cells->cell[cells->count], cells->place);
    }
}
