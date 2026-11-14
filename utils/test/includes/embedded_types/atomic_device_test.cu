// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// atomic_device_test.cu: embedded_types' atomics on the GPU and on the host side of one C++ unit. Every thread of a
// grid adds to one counter in global memory, every thread of a block adds to its block's counter in shared memory, and
// the grid's threads claim slots by compare-and-swap, each slot taken once; a count short of the threads times the
// rounds is an add two threads made at once, and a slot claimed twice is a swap that did not compare. The host pass of
// the same unit runs the same operations once, so the host arm compiles as C++ beside the device arm
#include "embed_atomic.h"

#include <cstdio>

// the grid the global case runs, the rounds each thread adds for, the threads of a block in the shared case, and the
// places the claiming case contends for
#define ATOMIC_BLOCKS 64u
#define ATOMIC_BLOCK_THREADS 128u
#define ATOMIC_ROUNDS 16u
#define ATOMIC_SLOTS 4096u

static unsigned int s_checks;
static unsigned int s_failed;

// one check made, printed as it passes or fails
static void atomic_check(bool passed, const char *what)
{
    s_checks += 1u;
    s_failed += passed ? 0u : 1u;
    std::printf("  %s %s\n", passed ? "ok  " : "FAIL", what);
}

// every thread of the grid adds one to the 32-bit counter and three to the 64-bit counter ATOMIC_ROUNDS times
__global__ void atomic_global_kernel(embed_atomic_u32 *count, embed_atomic_u64 *wide)
{
    for (unsigned int step = 0u; step < ATOMIC_ROUNDS; step += 1u)
    {
        (void)embed_atomic_u32_fetch_add(count, 1u);
        (void)embed_atomic_u64_fetch_add(wide, 3u);
    }
}

// each block's threads add one to the block's counter in shared memory ATOMIC_ROUNDS times, and the block's first
// thread writes the count out
__global__ void atomic_shared_kernel(unsigned int *counts)
{
    __shared__ embed_atomic_u32 count;
    if (threadIdx.x == 0u)
    {
        embed_atomic_u32_store(&count, 0u);
    }
    __syncthreads();
    for (unsigned int step = 0u; step < ATOMIC_ROUNDS; step += 1u)
    {
        (void)embed_atomic_u32_fetch_add(&count, 1u);
    }
    __syncthreads();
    if (threadIdx.x == 0u)
    {
        counts[blockIdx.x] = embed_atomic_u32_load(&count);
    }
}

// every thread of the grid tries every place, taking it by a swap from 0 to its own number plus one, and counts the
// places it won into its own entry
__global__ void atomic_claim_kernel(embed_atomic_u32 *slots, unsigned int *won)
{
    const unsigned int thread = (blockIdx.x * blockDim.x) + threadIdx.x;
    unsigned int taken = 0u;
    for (unsigned int at = 0u; at < ATOMIC_SLOTS; at += 1u)
    {
        embed_u32 free_slot = 0u;
        if (embed_atomic_u32_swap_if(&slots[at], &free_slot, thread + 1u))
        {
            taken += 1u;
        }
    }
    won[thread] = taken;
}

// the global case: true where the grid ran
static bool atomic_global(void)
{
    embed_atomic_u32 *count = nullptr;
    embed_atomic_u64 *wide = nullptr;
    bool ran = (cudaMalloc((void **)&count, sizeof(*count)) == cudaSuccess) &&
               (cudaMalloc((void **)&wide, sizeof(*wide)) == cudaSuccess) &&
               (cudaMemset(count, 0, sizeof(*count)) == cudaSuccess) && (cudaMemset(wide, 0, sizeof(*wide)) == cudaSuccess);
    if (ran)
    {
        atomic_global_kernel<<<ATOMIC_BLOCKS, ATOMIC_BLOCK_THREADS>>>(count, wide);
        ran = cudaDeviceSynchronize() == cudaSuccess;
    }
    embed_atomic_u32 counted = EMBED_ATOMIC_INIT(0u);
    embed_atomic_u64 counted_wide = EMBED_ATOMIC_INIT(0u);
    ran = ran && (cudaMemcpy(&counted, count, sizeof(counted), cudaMemcpyDeviceToHost) == cudaSuccess) &&
          (cudaMemcpy(&counted_wide, wide, sizeof(counted_wide), cudaMemcpyDeviceToHost) == cudaSuccess);
    atomic_check(ran, "the grid adding to two counters in global memory runs to its end");
    atomic_check(embed_atomic_u32_load(&counted) == (ATOMIC_BLOCKS * ATOMIC_BLOCK_THREADS * ATOMIC_ROUNDS),
                 "every add the grid made to the 32-bit counter is counted");
    atomic_check(embed_atomic_u64_load(&counted_wide) == ((embed_u64)ATOMIC_BLOCKS * ATOMIC_BLOCK_THREADS * ATOMIC_ROUNDS * 3u),
                 "every add the grid made to the 64-bit counter is counted");
    (void)cudaFree(count);
    (void)cudaFree(wide);
    return ran;
}

// the shared case: true where the grid ran
static bool atomic_shared(void)
{
    unsigned int *counts = nullptr;
    bool ran = cudaMalloc((void **)&counts, ATOMIC_BLOCKS * sizeof(*counts)) == cudaSuccess;
    if (ran)
    {
        atomic_shared_kernel<<<ATOMIC_BLOCKS, ATOMIC_BLOCK_THREADS>>>(counts);
        ran = cudaDeviceSynchronize() == cudaSuccess;
    }
    unsigned int counted[ATOMIC_BLOCKS] = {0u};
    ran = ran && (cudaMemcpy(counted, counts, sizeof(counted), cudaMemcpyDeviceToHost) == cudaSuccess);
    atomic_check(ran, "each block adding to its counter in shared memory runs to its end");
    unsigned int whole = 0u;
    for (unsigned int block = 0u; block < ATOMIC_BLOCKS; block += 1u)
    {
        whole += (counted[block] == (ATOMIC_BLOCK_THREADS * ATOMIC_ROUNDS)) ? 1u : 0u;
    }
    atomic_check(whole == ATOMIC_BLOCKS, "every block counts every add its threads made");
    (void)cudaFree(counts);
    return ran;
}

// the claiming case: true where the grid ran
static bool atomic_claim(void)
{
    const unsigned int threads = ATOMIC_BLOCKS * ATOMIC_BLOCK_THREADS;
    embed_atomic_u32 *slots = nullptr;
    unsigned int *won = nullptr;
    bool ran = (cudaMalloc((void **)&slots, ATOMIC_SLOTS * sizeof(*slots)) == cudaSuccess) &&
               (cudaMalloc((void **)&won, threads * sizeof(*won)) == cudaSuccess) &&
               (cudaMemset(slots, 0, ATOMIC_SLOTS * sizeof(*slots)) == cudaSuccess);
    if (ran)
    {
        atomic_claim_kernel<<<ATOMIC_BLOCKS, ATOMIC_BLOCK_THREADS>>>(slots, won);
        ran = cudaDeviceSynchronize() == cudaSuccess;
    }
    static unsigned int taken[ATOMIC_BLOCKS * ATOMIC_BLOCK_THREADS];
    static embed_atomic_u32 holders[ATOMIC_SLOTS];
    ran = ran && (cudaMemcpy(taken, won, sizeof(taken), cudaMemcpyDeviceToHost) == cudaSuccess) &&
          (cudaMemcpy(holders, slots, sizeof(holders), cudaMemcpyDeviceToHost) == cudaSuccess);
    atomic_check(ran, "the grid claiming slots by compare-and-swap runs to its end");
    unsigned int whole = 0u;
    for (unsigned int thread = 0u; thread < threads; thread += 1u)
    {
        whole += taken[thread];
    }
    atomic_check(whole == ATOMIC_SLOTS, "the slots won add up to the slots, each claimed once");
    unsigned int held = 0u;
    for (unsigned int at = 0u; at < ATOMIC_SLOTS; at += 1u)
    {
        const embed_u32 holder = embed_atomic_u32_load(&holders[at]);
        held += ((holder >= 1u) && (holder <= threads)) ? 1u : 0u;
    }
    atomic_check(held == ATOMIC_SLOTS, "every slot holds one thread's number");
    (void)cudaFree(slots);
    (void)cudaFree(won);
    return ran;
}

int main(void)
{
    // the host arm of this C++ unit: the same operations once
    embed_atomic_u32 host = EMBED_ATOMIC_INIT(5u);
    embed_u32 expected = 5u;
    const bool swapped = embed_atomic_u32_swap_if(&host, &expected, 7u);
    const embed_u32 before = embed_atomic_u32_fetch_add(&host, 1u);
    atomic_check(swapped && (before == 7u) && (embed_atomic_u32_load(&host) == 8u),
                 "the host arm swaps, adds and reads a word in a C++ unit");
    atomic_check((sizeof(embed_atomic_u32) == 4u) && (sizeof(embed_atomic_u64) == 8u),
                 "the words are 32 and 64 bits on the host");
    (void)atomic_global();
    (void)atomic_shared();
    (void)atomic_claim();
    std::printf("%u checks, %u failed\n", s_checks, s_failed);
    std::printf("test exit %d\n", (s_failed == 0u) ? 0 : 1);
    return (s_failed == 0u) ? 0 : 1;
}
