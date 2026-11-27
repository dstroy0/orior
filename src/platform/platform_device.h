// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_device.h: the memory of a job on the device. One cellblock of MMgr lies over a pool in the memory of the
// device, and each job kernel places it before its first cell. The calls take and give back the cells of the job, as
// the calls of platform.h do on the host. A kernel calls them, and the host does not. The bytes of the job cross the
// host and the device at tessera alone.
//
// A cell that the job takes and gives back before it ends goes on the temporary tier. That tier gives an address that
// the threads of a kernel index. A cell that grows, or that the job keeps past a stage, is a movable cell, read and
// written through its handle. The owners are the places of platform_places.h, as on the host.
#ifndef PLATFORM_DEVICE_H
#define PLATFORM_DEVICE_H

#include "embed_compiler_directives.h"

// The bytes of the pool of the device where the program declares none. A program that needs more defines
// g_device_pool_bytes, as it defines g_program_pool_bytes for the pool of the host
#ifndef PLATFORM_DEVICE_POOL_BYTES
#define PLATFORM_DEVICE_POOL_BYTES (256ULL << 20u)
#endif

#if defined(__CUDACC__)

#ifdef __cplusplus
extern "C"
{
#endif

    // The bytes of the pool of the device that the program declares, or 0 for PLATFORM_DEVICE_POOL_BYTES. The platform
    // gives a default of 0, and a definition in the program replaces it
    extern const unsigned long long g_device_pool_bytes;

    // The memory of the pool of the device, which the platform takes from the system one time for the life of the
    // program. The pool of the host comes the same way. The host calls it before the first kernel that opens the pool,
    // and each later call does nothing. 1, or 0 where the system refuses the bytes
    int platform_device_storage_taken(void);

    // The pool placed on the memory of the device, one time for the life of the program. The first thread of the job
    // kernel calls it before any other thread takes a cell. 1, or 0 where the cellblock refuses the pool
    __device__ int platform_device_open(void);

    // A cell of `bytes` from the pool of the device, counted to the owner at `place`. An ephemeral cell is on the
    // temporary tier and goes back before the job that took it ends. Any other cell is on the persistent tier. NULL
    // where the pool cannot meet the bytes [RETURNS OWNERSHIP]
    __device__ void *platform_device_cell_taken(unsigned long long bytes, unsigned int place, int ephemeral);

    // a cell of `bytes` from the pool of the device, every byte 0, as platform_device_cell_taken takes it
    __device__ void *platform_device_cell_cleared(unsigned long long bytes, unsigned int place, int ephemeral);

    // a cell given back to the pool of the device by the owner that took it. Nothing goes back where the cell is NULL
    // [TAKES OWNERSHIP]
    __device__ void platform_device_cell_released(void *cell, unsigned int place);

    // A movable cell of `bytes` from the pool of the device, counted to the owner at `place`. A move of the pool can
    // move it, and the job reads and writes it through the calls below by its handle alone. NULL where the pool cannot
    // meet the bytes [RETURNS OWNERSHIP]
    __device__ const void *platform_device_movable_taken(unsigned long long bytes, unsigned int place);

    // `count` bytes of `from` written into the movable cell at `offset`: 1, or 0 where the span passes the cell
    __device__ int platform_device_movable_written(const void *cell, unsigned long long offset, const void *from,
                                                   unsigned long long count);

    // `count` bytes of the movable cell at `offset` read into `into`: 1, or 0 where the span passes the cell
    __device__ int platform_device_movable_read(const void *cell, unsigned long long offset, void *into,
                                                unsigned long long count);

    // a movable cell given back by the owner that took it. Nothing goes back where the cell is NULL [TAKES OWNERSHIP]
    __device__ void platform_device_movable_released(const void *cell, unsigned int place);

    // the bytes that the persistent tier of the pool of the device can still give a cell
    __device__ unsigned long long platform_device_available(void);

#ifdef __cplusplus
}
#endif

#endif

#endif
