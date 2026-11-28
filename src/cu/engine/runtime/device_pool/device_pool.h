// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef DEVICE_POOL_H
#define DEVICE_POOL_H

#include "../../engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DEVICE_POOL_ERROR (-1L)

// every slice starts on 256 bytes, the alignment cudaMalloc gives its own allocations. Any kernel or cub workspace
// reads a slice as it would read an allocation of its own
#define DEVICE_POOL_SLICE_ALIGN 256ULL

// the allocation page of the device. The driver maps allocations in whole pages, and small ones share a page. A pool
// is one allocation, rounded to the page once
#define DEVICE_POOL_PAGE_BYTES (2ULL << 20u)

    // The device buffers of a job as one allocation. A plan arranges each buffer as a slice whose offset is the total
    // so far, rounded up to DEVICE_POOL_SLICE_ALIGN. The plan gives the size of the pool before any device work. It is
    // the sum of the slices, rounded up to the page once, the size that the job declares. A plan whose sum would pass
    // `2^62` bytes gets `spoiled`, and the call reserves no pool from it.
    typedef struct
    {
        unsigned long long bytes;
        unsigned long long slices;
        int spoiled;
    } DevicePoolPlan;

    // The allocated pool: `bytes` from `base`, of which `used` are in use. The calls take slices in the order of the
    // plan, each at the offset that the plan gave it. A take past the pool gives an error.
    typedef struct
    {
        unsigned char *base;
        unsigned long long bytes;
        unsigned long long used;
    } DevicePool;

    // arranges a slice of `bytes` in the plan and returns its offset
    unsigned long long device_pool_plan_slice(DevicePoolPlan *plan, unsigned long long bytes);

    // the bytes a pool reserved from the plan takes: the slices' sum rounded up to the page; 0 for a spoiled plan
    unsigned long long device_pool_plan_bytes(const DevicePoolPlan *plan);

    typedef struct
    {
        const DevicePoolPlan *plan;
        DevicePool *pool;
        EngineError *error;
    } DevicePoolReserveRequest;

    // allocates the pool of the plan as one allocation of device_pool_plan_bytes, with nothing taken yet. A plan of no
    // bytes makes an empty pool, and allocates nothing. The call writes the pool, and never reads it. It releases a
    // pool that is already allocated first.
    long device_pool_reserve(const DevicePoolReserveRequest *request);

    typedef struct
    {
        DevicePool *pool;
        unsigned long long bytes;
        void **slice;
        EngineError *error;
    } DevicePoolTakeRequest;

    // takes the next slice of `bytes` at the next 256-byte offset. A slice past the pool gives an error, and the call
    // takes nothing
    long device_pool_take(const DevicePoolTakeRequest *request);

    // every slice given back at once, the pool still allocated
    void device_pool_return(DevicePool *pool);

    void device_pool_release(DevicePool *pool);

#ifdef __cplusplus
}
#endif

#endif
