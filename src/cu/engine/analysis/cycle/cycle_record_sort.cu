// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cycle_record_sort.cu: the order of a sweep's records by an output, run by run, on the device
#include "cycle_record_internal.h"

// a pass of the radix sort reads four bits of the key, and a thread's counts are sixteen
#define CYCLE_SORT_DIGIT_BITS 4u

#define CYCLE_SORT_DIGITS 16u

// the radix sort's threads: each walks one stretch of the order, and their counts, sixteen a thread, are scanned by one
// block of CYCLE_SORT_SCAN_THREADS
#define CYCLE_SORT_THREADS_MAX 65536ull

#define CYCLE_SORT_SCAN_THREADS 1024u

// the bitonic network's threads in one block, each taking the pairs of its stride of the run
#define CYCLE_SORT_NETWORK_THREADS 1024u

// `width` bits of a record's field at `offset`, `bits` wide, from the field's bit `from`, as an unsigned word: zero
// past the field. `width` is at most 32, and the second limb is read only where the bits run into it
__device__ static unsigned int cycle_sort_bits(const unsigned int *record, unsigned int offset, unsigned int bits,
                                               unsigned int from, unsigned int width)
{
    if (from >= bits)
    {
        return 0u;
    }
    const unsigned int left = bits - from;
    const unsigned int taken = (width < left) ? width : left;
    const unsigned int start = offset + from;
    const unsigned int shift = start % 32u;
    unsigned long long window = record[start / 32u];
    if ((shift + taken) > 32u)
    {
        window |= (unsigned long long)record[(start / 32u) + 1u] << 32u;
    }
    // the window's bits from `shift` on, taken of them: below 2^32
    const unsigned int value = (unsigned int)(window >> shift);
    return (taken < 32u) ? (value & ((1u << taken) - 1u)) : value;
}

// 1 where the network's entry `one` stands after entry `other`: its key is greater, limb by limb from the top, or the
// keys are equal and its place in the run is later. No two entries share a place, and no two stand level
__device__ static int cycle_sort_after(const unsigned int *keys, const unsigned int *places, unsigned int limbs,
                                       unsigned int one, unsigned int other)
{
    for (unsigned int limb = limbs; limb > 0u; limb -= 1u)
    {
        const unsigned int left = keys[(one * limbs) + limb - 1u];
        const unsigned int right = keys[(other * limbs) + limb - 1u];
        if (left != right)
        {
            return (left > right) ? 1 : 0;
        }
    }
    return (places[one] > places[other]) ? 1 : 0;
}

// a run sorted in one thread block's shared memory: its keys and its places in the run are laid out, the places past
// the run's end padded to the network's width with keys of all ones, and the bitonic network orders (key, place) over
// that width. A padded entry's place is past every record's: the padding stands after the run whatever its keys.
// Every thread of the block reaches each barrier: the runs are the same for all of them
__global__ static void cycle_sort_network_kernel(const unsigned int *records, unsigned long long runs,
                                                 unsigned long long group, unsigned int out_limbs, unsigned int offset,
                                                 unsigned int bits, unsigned int limbs, unsigned int width,
                                                 unsigned int *order)
{
    extern __shared__ unsigned int cycle_sort_shared[];
    unsigned int *const keys = cycle_sort_shared;
    unsigned int *const places = &cycle_sort_shared[(unsigned long long)width * limbs];
    for (unsigned long long run = blockIdx.x; run < runs; run += gridDim.x)
    {
        const unsigned long long first = run * group;
        for (unsigned int entry = threadIdx.x; entry < width; entry += blockDim.x)
        {
            places[entry] = entry;
            const unsigned int *const record = &records[(first + entry) * out_limbs];
            for (unsigned int limb = 0u; limb < limbs; limb += 1u)
            {
                keys[(entry * limbs) + limb] =
                    (entry < group) ? cycle_sort_bits(record, offset, bits, 32u * limb, 32u) : 0xFFFFFFFFu;
            }
        }
        __syncthreads();
        for (unsigned int size = 2u; size <= width; size *= 2u)
        {
            for (unsigned int stride = size / 2u; stride > 0u; stride /= 2u)
            {
                for (unsigned int entry = threadIdx.x; entry < width; entry += blockDim.x)
                {
                    const unsigned int partner = entry ^ stride;
                    if (partner > entry)
                    {
                        const int rising = (entry & size) == 0u;
                        if (cycle_sort_after(keys, places, limbs, entry, partner) == rising)
                        {
                            for (unsigned int limb = 0u; limb < limbs; limb += 1u)
                            {
                                const unsigned int held = keys[(entry * limbs) + limb];
                                keys[(entry * limbs) + limb] = keys[(partner * limbs) + limb];
                                keys[(partner * limbs) + limb] = held;
                            }
                            const unsigned int place = places[entry];
                            places[entry] = places[partner];
                            places[partner] = place;
                        }
                    }
                }
                __syncthreads();
            }
        }
        for (unsigned int entry = threadIdx.x; entry < group; entry += blockDim.x)
        {
            // the lanes are below 2^32: the request holds at most 2^32 records
            order[first + entry] = (unsigned int)(first + places[entry]);
        }
        __syncthreads();
    }
}

__global__ static void cycle_sort_identity_kernel(unsigned int *order, unsigned long long count)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long lane = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; lane < count;
         lane += jump)
    {
        // the lanes are below 2^32
        order[lane] = (unsigned int)lane;
    }
}

// pass `pass` of the radix sort's digit for a lane: the passes below `field_passes` read the field's bits four at a
// time from its lowest, and the rest read the lane's run number the same way
__device__ static unsigned int cycle_sort_digit(const unsigned int *records, unsigned int lane, unsigned long long group,
                                                unsigned int out_limbs, unsigned int offset, unsigned int bits,
                                                unsigned int field_passes, unsigned int pass)
{
    if (pass < field_passes)
    {
        return cycle_sort_bits(&records[(unsigned long long)lane * out_limbs], offset, bits,
                               CYCLE_SORT_DIGIT_BITS * pass, CYCLE_SORT_DIGIT_BITS);
    }
    const unsigned long long run = lane / group;
    // a run number's four bits, below 16
    return (unsigned int)((run >> (CYCLE_SORT_DIGIT_BITS * (pass - field_passes))) & (CYCLE_SORT_DIGITS - 1u));
}

// each thread counts the digits of its stretch of the order, into counts[digit * threads + thread]
__global__ static void cycle_sort_count_kernel(const unsigned int *records, const unsigned int *from,
                                               unsigned long long count, unsigned long long group,
                                               unsigned int out_limbs, unsigned int offset, unsigned int bits,
                                               unsigned int field_passes, unsigned int pass, unsigned long long stretch,
                                               unsigned long long threads, unsigned int *counts)
{
    const unsigned long long thread = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (thread >= threads)
    {
        return;
    }
    unsigned int tally[CYCLE_SORT_DIGITS];
    for (unsigned int digit = 0u; digit < CYCLE_SORT_DIGITS; digit += 1u)
    {
        tally[digit] = 0u;
    }
    const unsigned long long begin = thread * stretch;
    const unsigned long long end = ((begin + stretch) < count) ? (begin + stretch) : count;
    for (unsigned long long at = begin; at < end; at += 1ull)
    {
        tally[cycle_sort_digit(records, from[at], group, out_limbs, offset, bits, field_passes, pass)] += 1u;
    }
    for (unsigned int digit = 0u; digit < CYCLE_SORT_DIGITS; digit += 1u)
    {
        counts[(digit * threads) + thread] = tally[digit];
    }
}

// the counts' exclusive prefix, in place, in the order digit by digit and thread by thread within a digit: one block,
// each thread summing a stretch, the stretches' totals scanned in shared memory, and each stretch written from its
// start. The totals are at most `count`, below 2^32 + 1, and a 64-bit running sum holds them
__global__ static void cycle_sort_scan_kernel(unsigned int *counts, unsigned long long entries)
{
    __shared__ unsigned long long totals[CYCLE_SORT_SCAN_THREADS];
    const unsigned long long stretch = (entries + CYCLE_SORT_SCAN_THREADS - 1ull) / CYCLE_SORT_SCAN_THREADS;
    const unsigned long long begin = threadIdx.x * stretch;
    const unsigned long long end = ((begin + stretch) < entries) ? (begin + stretch) : entries;
    unsigned long long total = 0ull;
    for (unsigned long long at = begin; at < end; at += 1ull)
    {
        total += counts[at];
    }
    totals[threadIdx.x] = total;
    __syncthreads();
    if (threadIdx.x == 0u)
    {
        unsigned long long running = 0ull;
        for (unsigned int thread = 0u; thread < CYCLE_SORT_SCAN_THREADS; thread += 1u)
        {
            const unsigned long long here = totals[thread];
            totals[thread] = running;
            running += here;
        }
    }
    __syncthreads();
    unsigned long long running = totals[threadIdx.x];
    for (unsigned long long at = begin; at < end; at += 1ull)
    {
        const unsigned long long here = counts[at];
        // a place in the order is below the count, at most 2^32 records, and the last prefix is below 2^32
        counts[at] = (unsigned int)running;
        running += here;
    }
}

// each thread walks its stretch again in order and lays each lane at its digit's next place: lanes of one digit keep
// the order they came in, thread by thread and within a thread
__global__ static void cycle_sort_scatter_kernel(const unsigned int *records, const unsigned int *from,
                                                 unsigned int *to, unsigned long long count, unsigned long long group,
                                                 unsigned int out_limbs, unsigned int offset, unsigned int bits,
                                                 unsigned int field_passes, unsigned int pass,
                                                 unsigned long long stretch, unsigned long long threads,
                                                 const unsigned int *places)
{
    const unsigned long long thread = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (thread >= threads)
    {
        return;
    }
    unsigned int next[CYCLE_SORT_DIGITS];
    for (unsigned int digit = 0u; digit < CYCLE_SORT_DIGITS; digit += 1u)
    {
        next[digit] = places[(digit * threads) + thread];
    }
    const unsigned long long begin = thread * stretch;
    const unsigned long long end = ((begin + stretch) < count) ? (begin + stretch) : count;
    for (unsigned long long at = begin; at < end; at += 1ull)
    {
        const unsigned int lane = from[at];
        const unsigned int digit = cycle_sort_digit(records, lane, group, out_limbs, offset, bits, field_passes, pass);
        to[next[digit]] = lane;
        next[digit] += 1u;
    }
}

// the bits a count needs: 0 for 0
static unsigned int cycle_sort_bit_length(unsigned long long value)
{
    unsigned int length = 0u;
    while (value != 0ull)
    {
        length += 1u;
        value >>= 1u;
    }
    return length;
}

// the radix sort: from the lanes in order, a stable pass a digit, the field's digits from its lowest and then the run
// number's, so that the last pass groups the runs and each run keeps the field's order and, under it, the lanes'
static int cycle_sort_radix(const CycleRecordSortRequest *request, EngineError *error)
{
    const unsigned long long count = request->count;
    const unsigned long long runs = count / request->group;
    const unsigned int field_passes = (request->bits + CYCLE_SORT_DIGIT_BITS - 1u) / CYCLE_SORT_DIGIT_BITS;
    const unsigned int run_passes =
        (cycle_sort_bit_length(runs - 1ull) + CYCLE_SORT_DIGIT_BITS - 1u) / CYCLE_SORT_DIGIT_BITS;
    const unsigned int passes = field_passes + run_passes;
    const unsigned long long threads = (count < CYCLE_SORT_THREADS_MAX) ? count : CYCLE_SORT_THREADS_MAX;
    const unsigned long long stretch = (count + threads - 1ull) / threads;
    const unsigned long long entries = CYCLE_SORT_DIGITS * threads;
    // the thread blocks are at most CYCLE_SORT_THREADS_MAX / CYCLE_BLOCK, far under 2^31
    const unsigned int blocks = (unsigned int)((threads + CYCLE_BLOCK - 1ull) / CYCLE_BLOCK);
    const unsigned long long fill_needed = (count + CYCLE_BLOCK - 1ull) / CYCLE_BLOCK;
    const unsigned int fill_blocks =
        (unsigned int)((fill_needed < CYCLE_RECORD_BLOCKS_MAX) ? fill_needed : CYCLE_RECORD_BLOCKS_MAX);
    unsigned int *spare = NULL;
    unsigned int *counts = NULL;
    int ok = CYCLE_STATUS_CHECK(cudaMalloc((void **)&spare, (size_t)count * sizeof(unsigned int)), &spare, error) &&
             CYCLE_STATUS_CHECK(cudaMalloc((void **)&counts, (size_t)entries * sizeof(unsigned int)), &counts, error);
    // the passes alternate between the order and the spare, starting where the last pass ends in the order
    unsigned int *from = ((passes % 2u) == 0u) ? request->order : spare;
    unsigned int *to = ((passes % 2u) == 0u) ? spare : request->order;
    if (ok != 0)
    {
        cycle_sort_identity_kernel<<<fill_blocks, CYCLE_BLOCK>>>(from, count);
        ok = CYCLE_STATUS_CHECK(cudaGetLastError(), from, error);
    }
    for (unsigned int pass = 0u; (ok != 0) && (pass < passes); pass += 1u)
    {
        cycle_sort_count_kernel<<<blocks, CYCLE_BLOCK>>>(request->records, from, count, request->group,
                                                        request->out_limbs, request->offset, request->bits,
                                                        field_passes, pass, stretch, threads, counts);
        ok = CYCLE_STATUS_CHECK(cudaGetLastError(), counts, error);
        if (ok != 0)
        {
            cycle_sort_scan_kernel<<<1u, CYCLE_SORT_SCAN_THREADS>>>(counts, entries);
            ok = CYCLE_STATUS_CHECK(cudaGetLastError(), counts, error);
        }
        if (ok != 0)
        {
            cycle_sort_scatter_kernel<<<blocks, CYCLE_BLOCK>>>(request->records, from, to, count, request->group,
                                                              request->out_limbs, request->offset, request->bits,
                                                              field_passes, pass, stretch, threads, counts);
            ok = CYCLE_STATUS_CHECK(cudaGetLastError(), to, error);
        }
        unsigned int *const swapped = from;
        from = to;
        to = swapped;
    }
    ok = ok && CYCLE_STATUS_CHECK(cudaDeviceSynchronize(), request->order, error);
    cudaFree(spare);
    cudaFree(counts);
    return ok;
}

extern "C" long cycle_record_sort(const CycleRecordSortRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return CYCLE_ERROR;
    }
    EngineError *const error = request->error;
    if (!CYCLE_CHECK(cycle_record_sort_valid(request) != 0, request, error, ENGINE_ERROR_REQUEST))
    {
        return CYCLE_ERROR;
    }
    const unsigned long long runs = request->count / request->group;
    const unsigned int limbs = (request->bits + 31u) / 32u;
    unsigned long long width = 1ull;
    while (width < request->group)
    {
        width *= 2ull;
    }
    int device = 0;
    int shared_most = 0;
    int ok = CYCLE_STATUS_CHECK(cudaGetDevice(&device), &device, error) &&
             CYCLE_STATUS_CHECK(cudaDeviceGetAttribute(&shared_most, cudaDevAttrMaxSharedMemoryPerBlockOptin, device),
                                &shared_most, error);
    if (ok == 0)
    {
        return CYCLE_ERROR;
    }
    // the network's keys and places for a run padded to a power of two
    const unsigned long long shared_bytes = width * ((unsigned long long)limbs + 1ull) * sizeof(unsigned int);
    // the attribute is a positive byte count the runtime reports, and it re-signs to unsigned exactly
    if (shared_bytes <= (unsigned long long)shared_most)
    {
        const unsigned int threads =
            (unsigned int)((width < CYCLE_SORT_NETWORK_THREADS) ? ((width < 32ull) ? 32ull : width)
                                                                : CYCLE_SORT_NETWORK_THREADS);
        const unsigned int blocks = (unsigned int)((runs < CYCLE_RECORD_BLOCKS_MAX) ? runs : CYCLE_RECORD_BLOCKS_MAX);
        // the shared bytes are at most the device's most, an int
        ok = CYCLE_STATUS_CHECK(cudaFuncSetAttribute(cycle_sort_network_kernel,
                                                     cudaFuncAttributeMaxDynamicSharedMemorySize, (int)shared_bytes),
                                request, error);
        if (ok != 0)
        {
            // the width is at most the shared memory's words, an unsigned int
            cycle_sort_network_kernel<<<blocks, threads, (size_t)shared_bytes>>>(
                request->records, runs, request->group, request->out_limbs, request->offset, request->bits, limbs,
                (unsigned int)width, request->order);
            ok = CYCLE_STATUS_CHECK(cudaGetLastError(), request->order, error) &&
                 CYCLE_STATUS_CHECK(cudaDeviceSynchronize(), request->order, error);
        }
    }
    else
    {
        ok = cycle_sort_radix(request, error);
    }
    return (ok != 0) ? (long)request->count : CYCLE_ERROR;
}
