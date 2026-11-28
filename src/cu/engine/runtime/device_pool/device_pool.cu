// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "device_pool.h"
#include "memoria_operor/memoria_operor.h"

#include <cuda_runtime.h>

static_assert(cudaSuccess == 0, "the engine reads a CUDA status of 0 as success");

static_assert((DEVICE_POOL_SLICE_ALIGN & (DEVICE_POOL_SLICE_ALIGN - 1ULL)) == 0ULL,
              "a slice's alignment is a power of two, so rounding to it is a mask");

static_assert((DEVICE_POOL_PAGE_BYTES & (DEVICE_POOL_PAGE_BYTES - 1ULL)) == 0ULL,
              "the page is a power of two, so rounding to it is a mask");

static_assert(sizeof(size_t) >= sizeof(unsigned long long),
              "cudaMalloc's size_t represents a pool's 64-bit byte count");

// cudaError_t enumerates non-negative codes below INT_MAX: the status converts to int exactly
#define DEVICE_POOL_STATUS_CHECK(call_, evacaddr_, error_)                                                             \
    engine_status_check((int)(call_), ENGINE_MODULE_DEVICE_POOL, (unsigned int)__LINE__, (const void *)(evacaddr_),    \
                        (error_))

#define DEVICE_POOL_CHECK(condition_, evacaddr_, error_, type_)                                                        \
    engine_error_check((condition_), (type_), ENGINE_MODULE_DEVICE_POOL, (unsigned int)__LINE__,                       \
                       (const void *)(evacaddr_), (error_))

// the bound on the sum of a plan, a whole number of slices and of pages. Its rounding to either never wraps, and never
// passes it
#define DEVICE_POOL_BYTES_MAX (1ULL << 62u)

static_assert(((DEVICE_POOL_BYTES_MAX % DEVICE_POOL_SLICE_ALIGN) == 0ULL) &&
                  ((DEVICE_POOL_BYTES_MAX % DEVICE_POOL_PAGE_BYTES) == 0ULL),
              "the max a plan contains rounds to itself on the slice and on the page");

static unsigned long long device_pool_round(unsigned long long bytes, unsigned long long step)
{
    return (bytes + (step - 1ULL)) & ~(step - 1ULL);
}

extern "C" unsigned long long device_pool_plan_slice(DevicePoolPlan *plan, unsigned long long bytes)
{
    // an unspoiled plan's sum is at max 2^62. Its offset is too, and the capacity left never wraps
    const unsigned long long offset = device_pool_round(plan->bytes, DEVICE_POOL_SLICE_ALIGN);
    if ((plan->spoiled != 0) || (bytes > (DEVICE_POOL_BYTES_MAX - offset)))
    {
        plan->spoiled = 1;
        return offset;
    }
    plan->bytes = offset + bytes;
    plan->slices += 1ULL;
    return offset;
}

extern "C" unsigned long long device_pool_plan_bytes(const DevicePoolPlan *plan)
{
    return (plan->spoiled != 0) ? 0ULL : device_pool_round(plan->bytes, DEVICE_POOL_PAGE_BYTES);
}

extern "C" long device_pool_reserve(const DevicePoolReserveRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return DEVICE_POOL_ERROR;
    }
    EngineError *const error = request->error;
    if (!DEVICE_POOL_CHECK((request->plan != NULL) && (request->pool != NULL) && (request->plan->spoiled == 0), request,
                           error, ENGINE_ERROR_REQUEST))
    {
        return DEVICE_POOL_ERROR;
    }
    DevicePool *const pool = request->pool;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = pool, .bytes = sizeof(*pool), .val = 0);
    const unsigned long long bytes = device_pool_plan_bytes(request->plan);
    if (bytes == 0ULL)
    {
        return 0L;
    }
    unsigned char *base = NULL;
    // a plan's bytes are at max 2^62, and size_t is at least 64 bits (asserted above)
    if (!DEVICE_POOL_STATUS_CHECK(cudaMalloc((void **)&base, (size_t)bytes), pool, error))
    {
        return DEVICE_POOL_ERROR;
    }
    pool->base = base;
    pool->bytes = bytes;
    return 0L;
}

extern "C" long device_pool_take(const DevicePoolTakeRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return DEVICE_POOL_ERROR;
    }
    EngineError *const error = request->error;
    if (!DEVICE_POOL_CHECK((request->pool != NULL) && (request->slice != NULL), request, error, ENGINE_ERROR_REQUEST))
    {
        return DEVICE_POOL_ERROR;
    }
    DevicePool *const pool = request->pool;
    // what the calls take never passes the pool, which is at max `2^62` bytes, thus the rounding never wraps
    const unsigned long long offset = device_pool_round(pool->used, DEVICE_POOL_SLICE_ALIGN);
    if (!DEVICE_POOL_CHECK((offset <= pool->bytes) && (request->bytes <= (pool->bytes - offset)), pool, error,
                           ENGINE_ERROR_REQUEST))
    {
        return DEVICE_POOL_ERROR;
    }
    *request->slice = (void *)(pool->base + offset);
    pool->used = offset + request->bytes;
    return 0L;
}

extern "C" void device_pool_return(DevicePool *pool)
{
    pool->used = 0ULL;
}

extern "C" void device_pool_release(DevicePool *pool)
{
    cudaFree(pool->base);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = pool, .bytes = sizeof(*pool), .val = 0);
}
