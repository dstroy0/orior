// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// spinlock_device_test.cu: embedded_types' spinlock on the GPU and on the host side of one C++ unit. A lock in global
// memory every thread of a grid contends for, and a lock in shared memory each block's threads contend for, each
// guarding a plain counter; a counter short of the threads times the rounds is an increment two threads made at once.
// The host pass of the same unit takes and frees a lock of its own, so the host arm compiles as C++ beside the device
// arm
#include "embed_spinlock.h"

#include <cstdio>

// the grid the global case runs, the rounds each thread takes the lock for, and the threads of a block in the shared
// case
#define SPINLOCK_BLOCKS 64u
#define SPINLOCK_BLOCK_THREADS 128u
#define SPINLOCK_ROUNDS 16u

static unsigned int s_checks;
static unsigned int s_failed;

// one check made, printed as it passes or fails
static void spinlock_check(bool passed, const char *what)
{
    s_checks += 1u;
    s_failed += passed ? 0u : 1u;
    std::printf("  %s %s\n", passed ? "ok  " : "FAIL", what);
}

// every thread of the grid adds one to the counter SPINLOCK_ROUNDS times, each under the one lock in global memory;
// the read and the write are two volatile accesses, so two threads in them at once lose an increment
__global__ void spinlock_global_kernel(embed_spinlock *lock, unsigned int *count)
{
    volatile unsigned int *const counted = count;
    for (unsigned int step = 0u; step < SPINLOCK_ROUNDS; step += 1u)
    {
        EMBED_SPINLOCK_ACQUIRE(lock);
        const unsigned int before = *counted;
        *counted = before + 1u;
        EMBED_SPINLOCK_RELEASE(lock);
    }
}

// each block's threads add one to the block's counter SPINLOCK_ROUNDS times under the block's lock in shared memory,
// and the block's first thread writes the count out
__global__ void spinlock_shared_kernel(unsigned int *counts)
{
    __shared__ embed_spinlock lock;
    __shared__ unsigned int count;
    if (threadIdx.x == 0u)
    {
        lock.locked = 0u;
        count = 0u;
    }
    __syncthreads();
    volatile unsigned int *const counted = &count;
    for (unsigned int step = 0u; step < SPINLOCK_ROUNDS; step += 1u)
    {
        EMBED_SPINLOCK_ACQUIRE(&lock);
        const unsigned int before = *counted;
        *counted = before + 1u;
        EMBED_SPINLOCK_RELEASE(&lock);
    }
    __syncthreads();
    if (threadIdx.x == 0u)
    {
        counts[blockIdx.x] = count;
    }
}

// the global case: 1 where the grid ran
static bool spinlock_global(void)
{
    embed_spinlock *lock = nullptr;
    unsigned int *count = nullptr;
    bool ran = (cudaMalloc((void **)&lock, sizeof(*lock)) == cudaSuccess) &&
               (cudaMalloc((void **)&count, sizeof(*count)) == cudaSuccess) &&
               (cudaMemset(lock, 0, sizeof(*lock)) == cudaSuccess) &&
               (cudaMemset(count, 0, sizeof(*count)) == cudaSuccess);
    if (ran)
    {
        spinlock_global_kernel<<<SPINLOCK_BLOCKS, SPINLOCK_BLOCK_THREADS>>>(lock, count);
        ran = cudaDeviceSynchronize() == cudaSuccess;
    }
    unsigned int counted = 0u;
    embed_spinlock after = EMBED_SPINLOCK_INIT;
    ran = ran && (cudaMemcpy(&counted, count, sizeof(counted), cudaMemcpyDeviceToHost) == cudaSuccess) &&
          (cudaMemcpy(&after, lock, sizeof(after), cudaMemcpyDeviceToHost) == cudaSuccess);
    spinlock_check(ran, "the grid contending for one lock in global memory runs to its end");
    spinlock_check(counted == (SPINLOCK_BLOCKS * SPINLOCK_BLOCK_THREADS * SPINLOCK_ROUNDS),
                   "every increment the grid made under the lock is counted");
    spinlock_check(after.locked == 0u, "the lock is free once the grid ended");
    (void)cudaFree(lock);
    (void)cudaFree(count);
    return ran;
}

// the shared case: 1 where the grid ran
static bool spinlock_shared(void)
{
    unsigned int *counts = nullptr;
    bool ran = cudaMalloc((void **)&counts, SPINLOCK_BLOCKS * sizeof(*counts)) == cudaSuccess;
    if (ran)
    {
        spinlock_shared_kernel<<<SPINLOCK_BLOCKS, SPINLOCK_BLOCK_THREADS>>>(counts);
        ran = cudaDeviceSynchronize() == cudaSuccess;
    }
    unsigned int counted[SPINLOCK_BLOCKS] = {0u};
    ran = ran && (cudaMemcpy(counted, counts, sizeof(counted), cudaMemcpyDeviceToHost) == cudaSuccess);
    spinlock_check(ran, "each block contending for its lock in shared memory runs to its end");
    unsigned int whole = 0u;
    for (unsigned int block = 0u; block < SPINLOCK_BLOCKS; block += 1u)
    {
        whole += (counted[block] == (SPINLOCK_BLOCK_THREADS * SPINLOCK_ROUNDS)) ? 1u : 0u;
    }
    spinlock_check(whole == SPINLOCK_BLOCKS, "every block counts every increment its threads made under its lock");
    (void)cudaFree(counts);
    return ran;
}

int main(void)
{
    // the host arm of this C++ unit, taken and freed once
    embed_spinlock host = EMBED_SPINLOCK_INIT;
    EMBED_SPINLOCK_ACQUIRE(&host);
    const unsigned int owned = host.locked;
    EMBED_SPINLOCK_RELEASE(&host);
    spinlock_check((owned == 1u) && (host.locked == 0u), "the host arm takes and frees a lock in a C++ unit");
    spinlock_check(sizeof(embed_spinlock) == 4u, "the lock is one 32-bit word on the host");
    (void)spinlock_global();
    (void)spinlock_shared();
    std::printf("%u checks, %u failed\n", s_checks, s_failed);
    std::printf("test exit %d\n", (s_failed == 0u) ? 0 : 1);
    return (s_failed == 0u) ? 0 : 1;
}
