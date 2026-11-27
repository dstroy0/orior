// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "compression_internal.h"

#include "../../runtime/daemon/tessera_job.h"
#include "../../runtime/device_pool/device_pool.h"

#include <cub/cub.cuh>

__device__ static unsigned int compression_block_k(const unsigned int *values, unsigned long long take,
                                                   unsigned long long *bits)
{
    unsigned long long sum = 0ULL;
    for (unsigned long long at = 0ULL; at < take; at += 1ULL)
    {
        sum += values[at];
    }
    unsigned int guess = 0u;
    while ((guess < 32u) && (((sum / take) >> guess) != 0ULL))
    {
        guess += 1u;
    }
    const unsigned int low = (guess > 2u) ? (guess - 2u) : 0u;
    const unsigned int high = ((guess + 1u) < 31u) ? (guess + 1u) : 31u;
    unsigned long long best = ~0ULL;
    unsigned int chosen = low;
    for (unsigned int k = low; k <= high; k += 1u)
    {
        unsigned long long cost = COMPRESSION_K_BITS;
        for (unsigned long long at = 0ULL; at < take; at += 1ULL)
        {
            const unsigned int quotient = values[at] >> k;
            cost += (quotient < COMPRESSION_ESCAPE) ? ((unsigned long long)quotient + 1ULL + k)
                                                    : (COMPRESSION_ESCAPE + 32ULL);
        }
        if (cost < best)
        {
            best = cost;
            chosen = k;
        }
    }
    *bits = best;
    return chosen;
}

// the index of the thread in its grid, where a kernel walk starts
__device__ static unsigned long long compression_thread_first(void)
{
    return ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
}

// the threads of the grid, the jump of a kernel walk
__device__ static unsigned long long compression_thread_jump(void)
{
    return (unsigned long long)gridDim.x * blockDim.x;
}

__device__ void compression_zigzag_span(const int *from, unsigned long long count, unsigned int *to,
                                        unsigned long long first, unsigned long long jump)
{
    for (unsigned long long index = first; index < count; index += jump)
    {
        const int value = from[index];
        to[index] = (value >= 0) ? (2u * (unsigned int)value) : ((2u * (unsigned int)(-value)) - 1u);
    }
}

__global__ static void compression_zigzag_kernel(const int *from, unsigned long long count, unsigned int *to)
{
    compression_zigzag_span(from, count, to, compression_thread_first(), compression_thread_jump());
}

__device__ void compression_measure_span(const unsigned int *values, unsigned long long count,
                                         unsigned long long chunks, unsigned long long *chunk_bits,
                                         unsigned long long first, unsigned long long jump)
{
    for (unsigned long long chunk = first; chunk < chunks; chunk += jump)
    {
        unsigned long long total = 0ULL;
        for (unsigned long long start = chunk * COMPRESSION_CHUNK;
             (start < count) && (start < ((chunk + 1ULL) * COMPRESSION_CHUNK)); start += COMPRESSION_BLOCK)
        {
            const unsigned long long take = ((count - start) < COMPRESSION_BLOCK) ? (count - start) : COMPRESSION_BLOCK;
            unsigned long long bits = 0ULL;
            (void)compression_block_k(&values[start], take, &bits);
            total += bits;
        }
        chunk_bits[chunk] = total;
    }
}

__global__ static void compression_measure_kernel(const unsigned int *values, unsigned long long count,
                                                  unsigned long long chunks, unsigned long long *chunk_bits)
{
    compression_measure_span(values, count, chunks, chunk_bits, compression_thread_first(), compression_thread_jump());
}

__device__ static void compression_put(unsigned int *stream, unsigned long long at, unsigned int field,
                                       unsigned int width)
{
    if (width == 0u)
    {
        return;
    }
    const unsigned int shift = (unsigned int)(at % 32ULL);
    atomicOr(&stream[at / 32ULL], field << shift);
    if ((shift + width) > 32u)
    {
        atomicOr(&stream[(at / 32ULL) + 1ULL], field >> (32u - shift));
    }
}

__device__ void compression_write_span(const unsigned int *values, unsigned long long count, unsigned long long chunks,
                                       const unsigned long long *offsets, unsigned int *stream,
                                       unsigned long long first, unsigned long long jump)
{
    for (unsigned long long chunk = first; chunk < chunks; chunk += jump)
    {
        unsigned long long at = offsets[chunk];
        for (unsigned long long start = chunk * COMPRESSION_CHUNK;
             (start < count) && (start < ((chunk + 1ULL) * COMPRESSION_CHUNK)); start += COMPRESSION_BLOCK)
        {
            const unsigned long long take = ((count - start) < COMPRESSION_BLOCK) ? (count - start) : COMPRESSION_BLOCK;
            unsigned long long bits = 0ULL;
            const unsigned int k = compression_block_k(&values[start], take, &bits);
            compression_put(stream, at, k, COMPRESSION_K_BITS);
            at += COMPRESSION_K_BITS;
            for (unsigned long long index = 0ULL; index < take; index += 1ULL)
            {
                const unsigned int value = values[start + index];
                const unsigned int quotient = value >> k;
                if (quotient < COMPRESSION_ESCAPE)
                {
                    compression_put(stream, at, (1u << quotient) - 1u, quotient);
                    at += (unsigned long long)quotient + 1ULL;
                    compression_put(stream, at, value & ((k == 0u) ? 0u : (0xFFFFFFFFu >> (32u - k))), k);
                    at += k;
                }
                else
                {
                    compression_put(stream, at, (1u << COMPRESSION_ESCAPE) - 1u, COMPRESSION_ESCAPE);
                    at += COMPRESSION_ESCAPE;
                    compression_put(stream, at, value, 32u);
                    at += 32ULL;
                }
            }
        }
    }
}

__global__ static void compression_write_kernel(const unsigned int *values, unsigned long long count,
                                                unsigned long long chunks, const unsigned long long *offsets,
                                                unsigned int *stream)
{
    compression_write_span(values, count, chunks, offsets, stream, compression_thread_first(),
                           compression_thread_jump());
}

__device__ static unsigned int compression_get(const unsigned int *stream, unsigned long long at, unsigned int width)
{
    if (width == 0u)
    {
        return 0u;
    }
    const unsigned int shift = (unsigned int)(at % 32ULL);
    const unsigned long long pair =
        (unsigned long long)stream[at / 32ULL] | ((unsigned long long)stream[(at / 32ULL) + 1ULL] << 32u);
    return (unsigned int)((pair >> shift) & ((width == 32u) ? 0xFFFFFFFFULL : ((1ULL << width) - 1ULL)));
}

__device__ void compression_read_span(const unsigned int *stream, const unsigned long long *offsets,
                                      unsigned long long count, unsigned long long chunks, int *to,
                                      unsigned long long first, unsigned long long jump)
{
    for (unsigned long long chunk = first; chunk < chunks; chunk += jump)
    {
        unsigned long long at = offsets[chunk];
        for (unsigned long long start = chunk * COMPRESSION_CHUNK;
             (start < count) && (start < ((chunk + 1ULL) * COMPRESSION_CHUNK)); start += COMPRESSION_BLOCK)
        {
            const unsigned long long take = ((count - start) < COMPRESSION_BLOCK) ? (count - start) : COMPRESSION_BLOCK;
            const unsigned int k = compression_get(stream, at, COMPRESSION_K_BITS);
            at += COMPRESSION_K_BITS;
            for (unsigned long long index = 0ULL; index < take; index += 1ULL)
            {
                const unsigned int window = compression_get(stream, at, COMPRESSION_ESCAPE);
                const unsigned int quotient = (window == ((1u << COMPRESSION_ESCAPE) - 1u))
                                                  ? COMPRESSION_ESCAPE
                                                  : (unsigned int)(__ffs(~window) - 1);
                unsigned int value = 0u;
                if (quotient < COMPRESSION_ESCAPE)
                {
                    at += (unsigned long long)quotient + 1ULL;
                    value = (quotient << k) | compression_get(stream, at, k);
                    at += k;
                }
                else
                {
                    at += COMPRESSION_ESCAPE;
                    value = compression_get(stream, at, 32u);
                    at += 32ULL;
                }
                to[start + index] = ((value & 1u) == 0u) ? (int)(value >> 1u) : -(int)((value + 1u) >> 1u);
            }
        }
    }
}

__global__ static void compression_read_kernel(const unsigned int *stream, const unsigned long long *offsets,
                                               unsigned long long count, unsigned long long chunks, int *to)
{
    compression_read_span(stream, offsets, count, chunks, to, compression_thread_first(), compression_thread_jump());
}

static unsigned int compression_blocks(unsigned long long count)
{
    const unsigned long long needed = (count + COMPRESSION_THREADS - 1ULL) / COMPRESSION_THREADS;
    return (unsigned int)((needed < 65536ULL) ? ((needed == 0ULL) ? 1ULL : needed) : 65536ULL);
}

// The bits of each chunk, the offset of each chunk and the scratch of the scan are slices of one pool. The pool holds
// the most chunks that a call asked for. The values set the size of the stream, which the call knows only after it
// measures them. The stream is a pool of its own
struct CompressionResident
{
    unsigned long long *chunk_bits;
    unsigned long long *chunk_offsets;
    size_t chunk_capacity;
    unsigned int *stream;
    size_t stream_capacity;
    void *scan;
    size_t scan_bytes;
    PlatformArray host_offsets;
    PlatformArray host_stream;
};

static CompressionResident s_compression_resident;

// the owner that the cells of compression in the pool of the device count to
#define COMPRESSION_PLACE ((unsigned int)platform_place_g_engine_owner)

#define COMPRESSION_CHUNK_SLICES 3u

// the addresses of the cells of the chunks: the bits of each chunk, the offset of each chunk, and the scratch of the
// scan
static void compression_chunk_cells(CompressionResident *resident, void **cells[COMPRESSION_CHUNK_SLICES])
{
    cells[0] = (void **)&resident->chunk_bits;
    cells[1] = (void **)&resident->chunk_offsets;
    cells[2] = &resident->scan;
}

// each cell of the chunks given back to the pool of the device, the newest first
static void compression_chunks_given(CompressionResident *resident)
{
    void **cells[COMPRESSION_CHUNK_SLICES];
    compression_chunk_cells(resident, cells);
    for (unsigned int at = COMPRESSION_CHUNK_SLICES; at > 0u; at -= 1u)
    {
        tessera_job_given(*cells[at - 1u], COMPRESSION_PLACE);
        *cells[at - 1u] = NULL;
    }
    resident->chunk_capacity = 0u;
    resident->scan_bytes = 0u;
}

extern "C" void compression_resident_release(void)
{
    CompressionResident *const resident = &s_compression_resident;
    tessera_job_given(resident->stream, COMPRESSION_PLACE);
    resident->stream = NULL;
    resident->stream_capacity = 0u;
    compression_chunks_given(resident);
    platform_array_ended(&resident->host_offsets);
    platform_array_ended(&resident->host_stream);
}

// the bytes of the cells of the chunks for `chunks`, in the order of compression_chunk_cells
static void compression_chunk_slices(size_t chunks, size_t scan_bytes,
                                     unsigned long long bytes[COMPRESSION_CHUNK_SLICES])
{
    bytes[0] = (unsigned long long)chunks * sizeof(unsigned long long);
    bytes[1] = (unsigned long long)chunks * sizeof(unsigned long long);
    bytes[2] = (unsigned long long)scan_bytes;
}

// The cells of the chunks grown to `chunks`, persistent cells of the pool of the device that the resident holds until
// compression_resident_release
static int compression_reserve_chunks(size_t chunks, EngineError *error)
{
    CompressionResident *const resident = &s_compression_resident;
    if (chunks <= resident->chunk_capacity)
    {
        return 1;
    }
    compression_chunks_given(resident);
    size_t scan_bytes = 0u;
    // cub sizes the scan's scratch when called with none, and allocates nothing; compression_chunks contains the count
    // below 2^31 - 1: it narrows to cub's int exactly
    int ok = COMPRESSION_STATUS_CHECK(cub::DeviceScan::ExclusiveSum(NULL, scan_bytes, (const unsigned long long *)NULL,
                                                                    (unsigned long long *)NULL, (int)chunks),
                                      &scan_bytes, error);
    unsigned long long bytes[COMPRESSION_CHUNK_SLICES];
    compression_chunk_slices(chunks, scan_bytes, bytes);
    void **cells[COMPRESSION_CHUNK_SLICES];
    compression_chunk_cells(resident, cells);
    for (unsigned int at = 0u; (ok != 0) && (at < COMPRESSION_CHUNK_SLICES); at += 1u)
    {
        ok = COMPRESSION_CHECK(tessera_job_persistent_taken(bytes[at], COMPRESSION_PLACE, cells[at]), cells[at], error,
                               ENGINE_ERROR_RESOURCE);
    }
    resident->chunk_capacity = (ok != 0) ? chunks : 0u;
    resident->scan_bytes = (ok != 0) ? scan_bytes : 0u;
    return ok;
}

// the stream grown to `limbs` limbs and one more, a persistent cell of the pool of the device
static int compression_reserve_stream(size_t limbs, EngineError *error)
{
    CompressionResident *const resident = &s_compression_resident;
    if ((limbs + 1u) <= resident->stream_capacity)
    {
        return 1;
    }
    tessera_job_given(resident->stream, COMPRESSION_PLACE);
    resident->stream = NULL;
    resident->stream_capacity = 0u;
    void *cell = NULL;
    const int ok =
        COMPRESSION_CHECK(tessera_job_persistent_taken(((unsigned long long)limbs + 1ULL) * sizeof(unsigned int),
                                                       COMPRESSION_PLACE, &cell),
                          &resident->stream, error, ENGINE_ERROR_RESOURCE);
    // Explicit cast reads the cell as the limbs that it holds
    resident->stream = (unsigned int *)cell;
    resident->stream_capacity = (ok != 0) ? (limbs + 1u) : 0u;
    return ok;
}

extern "C" unsigned long long compression_chunks(unsigned long long count)
{
    const unsigned long long chunks = (count + COMPRESSION_CHUNK - 1ULL) / COMPRESSION_CHUNK;
    return ((count != 0ULL) && (chunks < 0x7FFFFFFFULL)) ? chunks : 0ULL;
}

extern "C" unsigned long long compression_reserve_bytes(unsigned long long count)
{
    const unsigned long long chunks = compression_chunks(count);
    size_t scan_bytes = 0u;
    // cub sizes the scan's scratch when called with none, and allocates nothing; compression_chunks contains the count
    // below 2^31 - 1: it narrows to cub's int exactly
    if ((chunks == 0ULL) || (cub::DeviceScan::ExclusiveSum(NULL, scan_bytes, (const unsigned long long *)NULL,
                                                           (unsigned long long *)NULL, (int)chunks) != cudaSuccess))
    {
        return 0ULL;
    }
    unsigned long long bytes[COMPRESSION_CHUNK_SLICES];
    // a size_t represents 64 bits, which device_pool asserts: the chunk count converts exactly
    compression_chunk_slices((size_t)chunks, scan_bytes, bytes);
    DevicePoolPlan plan = {0ULL, 0ULL, 0};
    for (unsigned int at = 0u; at < COMPRESSION_CHUNK_SLICES; at += 1u)
    {
        device_pool_plan_slice(&plan, bytes[at]);
    }
    return device_pool_plan_bytes(&plan);
}

extern "C" long compression_encode(const CompressionEncodeRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return COMPRESSION_ERROR;
    }
    EngineError *const error = request->error;
    if (!COMPRESSION_CHECK((request->device_coefficients != NULL) && (request->device_scratch != NULL) &&
                               (request->chunks != NULL) && (request->bits != NULL) && (request->offsets != NULL) &&
                               (request->stream != NULL),
                           request, error, ENGINE_ERROR_REQUEST))
    {
        return COMPRESSION_ERROR;
    }
    CompressionResident *const resident = &s_compression_resident;
    const unsigned long long count = request->count;
    const unsigned long long chunks = compression_chunks(count);
    if (!COMPRESSION_CHECK(chunks != 0ULL, &request->count, error, ENGINE_ERROR_REQUEST) ||
        (compression_reserve_chunks((size_t)chunks, error) == 0))
    {
        return COMPRESSION_ERROR;
    }
    unsigned int *const naturals = request->device_scratch;
    compression_zigzag_kernel<<<compression_blocks(count), COMPRESSION_THREADS>>>(request->device_coefficients, count,
                                                                                  naturals);
    const unsigned int chunk_blocks = (unsigned int)((chunks + COMPRESSION_THREADS - 1ULL) / COMPRESSION_THREADS);
    compression_measure_kernel<<<chunk_blocks, COMPRESSION_THREADS>>>(naturals, count, chunks, resident->chunk_bits);
    size_t bytes = resident->scan_bytes;
    int ok = COMPRESSION_STATUS_CHECK(cudaGetLastError(), naturals, error) &&
             COMPRESSION_STATUS_CHECK(cub::DeviceScan::ExclusiveSum(resident->scan, bytes, resident->chunk_bits,
                                                                    resident->chunk_offsets, (int)chunks),
                                      resident->chunk_offsets, error);
    unsigned long long last_offset = 0ULL;
    unsigned long long last_bits = 0ULL;
    ok = ok && COMPRESSION_CHECK(tessera_job_crossed_out(&last_offset, &resident->chunk_offsets[chunks - 1ULL],
                                                         sizeof(unsigned long long)) &&
                                     tessera_job_crossed_out(&last_bits, &resident->chunk_bits[chunks - 1ULL],
                                                             sizeof(unsigned long long)),
                                 &last_offset, error, ENGINE_ERROR_RESOURCE);
    const unsigned long long bits = last_offset + last_bits;
    const size_t limbs = (size_t)((bits + 31ULL) / 32ULL);
    const unsigned long long stream_bytes = (unsigned long long)limbs * sizeof(unsigned int);
    ok = ok && compression_reserve_stream(limbs, error) &&
         COMPRESSION_CHECK(tessera_job_cleared(resident->stream, stream_bytes + sizeof(unsigned int)), resident->stream,
                           error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        compression_write_kernel<<<chunk_blocks, COMPRESSION_THREADS>>>(naturals, count, chunks,
                                                                        resident->chunk_offsets, resident->stream);
        ok = COMPRESSION_STATUS_CHECK(cudaGetLastError(), resident->stream, error);
    }
    unsigned long long *host_offsets = NULL;
    unsigned int *host_stream = NULL;
    ok = ok && compression_host_room(chunks, limbs, &host_offsets, &host_stream, error) &&
         COMPRESSION_CHECK(
             tessera_job_crossed_out(host_offsets, resident->chunk_offsets, chunks * sizeof(unsigned long long)) &&
                 tessera_job_crossed_out(host_stream, resident->stream, stream_bytes),
             host_offsets, error, ENGINE_ERROR_RESOURCE);
    if (ok == 0)
    {
        return COMPRESSION_ERROR;
    }
    *request->chunks = chunks;
    *request->bits = bits;
    *request->offsets = host_offsets;
    *request->stream = host_stream;
    return 0L;
}

int compression_host_room(unsigned long long chunks, unsigned long long limbs, unsigned long long **offsets,
                          unsigned int **stream, EngineError *error)
{
    CompressionResident *const resident = &s_compression_resident;
    // an array with no element size has not begun: it begins at its first copy
    if (resident->host_offsets.element == 0ULL)
    {
        platform_array_begun(&resident->host_offsets, sizeof(unsigned long long), platform_place_g_program_owner, 0);
        platform_array_begun(&resident->host_stream, sizeof(unsigned int), platform_place_g_program_owner, 0);
    }
    if (!COMPRESSION_STATUS_CHECK(platform_array_counted(&resident->host_offsets, chunks) &&
                                          platform_array_counted(&resident->host_stream, limbs + 1ULL)
                                      ? cudaSuccess
                                      : cudaErrorMemoryAllocation,
                                  &resident->host_offsets, error))
    {
        return 0;
    }
    // Explicit casts read the cells of the arrays as the offsets and the words that they hold
    *offsets = (unsigned long long *)resident->host_offsets.cell;
    *stream = (unsigned int *)resident->host_stream.cell;
    return 1;
}

extern "C" long compression_decode(const CompressionDecodeRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return COMPRESSION_ERROR;
    }
    EngineError *const error = request->error;
    if (!COMPRESSION_CHECK((request->offsets != NULL) && (request->stream != NULL) &&
                               (request->device_coefficients != NULL),
                           request, error, ENGINE_ERROR_REQUEST))
    {
        return COMPRESSION_ERROR;
    }
    CompressionResident *const resident = &s_compression_resident;
    const unsigned long long chunks = compression_chunks(request->count);
    const size_t limbs = (size_t)((request->bits + 31ULL) / 32ULL);
    if (!COMPRESSION_CHECK(chunks != 0ULL, &request->count, error, ENGINE_ERROR_REQUEST) ||
        !COMPRESSION_CHECK(request->chunks == chunks, &request->chunks, error, ENGINE_ERROR_LOGIC) ||
        (compression_reserve_chunks((size_t)chunks, error) == 0) || (compression_reserve_stream(limbs, error) == 0))
    {
        return COMPRESSION_ERROR;
    }
    for (unsigned long long chunk = 0ULL; chunk < chunks; chunk += 1ULL)
    {
        if (!COMPRESSION_CHECK((request->offsets[chunk] <= request->bits) &&
                                   ((chunk == 0ULL) || (request->offsets[chunk] >= request->offsets[chunk - 1ULL])),
                               &request->offsets[chunk], error, ENGINE_ERROR_LOGIC))
        {
            return COMPRESSION_ERROR;
        }
    }
    const unsigned long long stream_bytes = (unsigned long long)limbs * sizeof(unsigned int);
    int ok = COMPRESSION_CHECK(
        tessera_job_cleared(resident->stream, stream_bytes + sizeof(unsigned int)) &&
            tessera_job_crossed_into(resident->stream, request->stream, stream_bytes) &&
            tessera_job_crossed_into(resident->chunk_offsets, request->offsets, chunks * sizeof(unsigned long long)),
        resident->stream, error, ENGINE_ERROR_RESOURCE);
    if (ok != 0)
    {
        const unsigned int chunk_blocks = (unsigned int)((chunks + COMPRESSION_THREADS - 1ULL) / COMPRESSION_THREADS);
        compression_read_kernel<<<chunk_blocks, COMPRESSION_THREADS>>>(
            resident->stream, resident->chunk_offsets, request->count, chunks, request->device_coefficients);
        ok = COMPRESSION_STATUS_CHECK(cudaGetLastError(), request->device_coefficients, error);
    }
    return (ok != 0) ? 0L : COMPRESSION_ERROR;
}
