// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_device_test.cu: the pool of the device (platform_device.h) on the GPU. One thread places the pool and takes
// a temporary cell, a grid writes each byte of the cell through its address, and one thread reads each byte back and
// gives the cell back. One thread writes a movable cell through its handle and reads it back, takes and gives back a
// persistent cell, and asks for more bytes than the pool holds. Each answer goes into a record in the memory of the
// device, and the host reads the record once and agrees with it
#include "../../../../src/platform/platform.h"
#include "../../../../src/platform/platform_device.h"
#include "../../../../src/platform/platform_places.h"

#include <cuda_runtime.h>

// the bytes of the temporary cell that the grid writes, and the grid that writes it
#define DEVICE_TEST_CELL_BYTES (1u << 20u)
#define DEVICE_TEST_BLOCKS 256u
#define DEVICE_TEST_BLOCK_THREADS 256u

// the bytes that the movable case writes and reads back
#define DEVICE_TEST_MOVABLE_BYTES 64u

// the owner that the cells count to
#define DEVICE_TEST_PLACE 3u

// The pool of the device that the test declares, twice the default. The persistent tier must then offer more than the
// default holds
extern "C" const unsigned long long g_device_pool_bytes = 2ULL * PLATFORM_DEVICE_POOL_BYTES;

// What the kernels found, which the host reads after the last of them ends
typedef struct
{
    unsigned int opened;       // 1 where the first open placed the pool.
    unsigned int opened_again; // 1 where a second open answers 1 again.
    unsigned int taken;        // 1 where the temporary cell came.
    unsigned int bytes_agree;  // 1 where each byte of the cell is the byte the grid wrote.
    unsigned int given_back;   // 1 where the free bytes after the give are the free bytes before the take.

    unsigned int movable_taken;   // 1 where the movable cell came.
    unsigned int movable_agrees;  // 1 where the bytes read back out of the movable cell are the bytes written.
    unsigned int persistent_back; // 1 where a persistent take and give leave the free bytes as they were.
    unsigned int refused;         // 1 where a take past the bytes of the pool answers NULL.
} DeviceTestRecord;

static __device__ DeviceTestRecord s_record;

// the temporary cell, which the first kernel takes and the last gives back
static __device__ unsigned char *s_cell;

// the free bytes of the pool before the temporary take
static __device__ unsigned long long s_before;

// One thread: the pool placed, and the temporary cell taken
__global__ void device_test_opened(void)
{
    s_record.opened = (unsigned int)platform_device_open();
    s_record.opened_again = (unsigned int)platform_device_open();
    s_before = platform_device_available();
    // Explicit cast takes the address of the cell as its bytes
    s_cell = (unsigned char *)platform_device_cell_taken(DEVICE_TEST_CELL_BYTES, DEVICE_TEST_PLACE, 1);
    s_record.taken = (unsigned int)(s_cell != NULL);
}

// The grid: each thread writes the bytes at its index and at each index a grid's width past it
__global__ void device_test_written(void)
{
    if (s_cell == NULL)
    {
        return;
    }
    const unsigned int stride = gridDim.x * blockDim.x;
    for (unsigned int at = (blockIdx.x * blockDim.x) + threadIdx.x; at < DEVICE_TEST_CELL_BYTES; at += stride)
    {
        // Explicit cast keeps the low byte of the index, the byte that the read back expects
        s_cell[at] = (unsigned char)(at * 7u);
    }
}

// One thread: each byte read back, the cell given back, and the movable, persistent and refused cases
__global__ void device_test_read(void)
{
    unsigned int agree = (s_cell != NULL) ? 1u : 0u;
    for (unsigned int at = 0u; (agree != 0u) && (at < DEVICE_TEST_CELL_BYTES); at++)
    {
        // Explicit cast keeps the low byte of the index, the byte that the grid wrote
        agree = (s_cell[at] == (unsigned char)(at * 7u)) ? 1u : 0u;
    }
    s_record.bytes_agree = agree;
    platform_device_cell_released(s_cell, DEVICE_TEST_PLACE);
    s_record.given_back = (unsigned int)(platform_device_available() == s_before);

    unsigned char written[DEVICE_TEST_MOVABLE_BYTES];
    unsigned char read[DEVICE_TEST_MOVABLE_BYTES];
    for (unsigned int at = 0u; at < DEVICE_TEST_MOVABLE_BYTES; at++)
    {
        // Explicit cast keeps the low byte, a pattern no two neighbors share
        written[at] = (unsigned char)((at * 13u) + 1u);
        read[at] = 0u;
    }
    const void *const movable = platform_device_movable_taken(DEVICE_TEST_MOVABLE_BYTES, DEVICE_TEST_PLACE);
    s_record.movable_taken = (unsigned int)(movable != NULL);
    unsigned int same =
        (unsigned int)(platform_device_movable_written(movable, 0ULL, written, DEVICE_TEST_MOVABLE_BYTES) &&
                       platform_device_movable_read(movable, 0ULL, read, DEVICE_TEST_MOVABLE_BYTES));
    for (unsigned int at = 0u; (same != 0u) && (at < DEVICE_TEST_MOVABLE_BYTES); at++)
    {
        same = (read[at] == written[at]) ? 1u : 0u;
    }
    s_record.movable_agrees = same;
    platform_device_movable_released(movable, DEVICE_TEST_PLACE);

    const unsigned long long left = platform_device_available();
    void *const kept = platform_device_cell_taken(4096ULL, DEVICE_TEST_PLACE, 0);
    platform_device_cell_released(kept, DEVICE_TEST_PLACE);
    s_record.persistent_back = (unsigned int)((kept != NULL) && (platform_device_available() == left));
    s_record.refused =
        (unsigned int)(platform_device_cell_taken(PLATFORM_DEVICE_POOL_BYTES * 2ULL, DEVICE_TEST_PLACE, 0) == NULL);
}

// one check of the record, printed as it passes or fails
static unsigned int device_test_check(unsigned int held, const char *what)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0u) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0u) ? 0u : 1u;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the memory of the pool, taken from the system before the first kernel opens the pool
    const int stored = platform_device_storage_taken();
    device_test_opened<<<1, 1>>>();
    device_test_written<<<DEVICE_TEST_BLOCKS, DEVICE_TEST_BLOCK_THREADS>>>();
    device_test_read<<<1, 1>>>();
    DeviceTestRecord record = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    const int ran = (cudaDeviceSynchronize() == cudaSuccess) &&
                    (cudaMemcpyFromSymbol(&record, s_record, sizeof(record)) == cudaSuccess);
    unsigned long long before = 0ULL;
    const int read_before = cudaMemcpyFromSymbol(&before, s_before, sizeof(before)) == cudaSuccess;
    unsigned int failed = device_test_check((unsigned int)stored, "the memory of the pool comes from the system");
    failed += device_test_check((unsigned int)(read_before && (before > PLATFORM_DEVICE_POOL_BYTES)),
                                "the pool holds the bytes that the program declares, past the default");
    failed += device_test_check((unsigned int)ran, "the three kernels ran and the record came back");
    failed += device_test_check(record.opened, "the pool of the device is placed");
    failed += device_test_check(record.opened_again, "a second open answers that the pool is placed");
    failed += device_test_check(record.taken, "a temporary cell of 1 MiB comes from the pool");
    failed += device_test_check(record.bytes_agree, "each byte a grid wrote through the cell reads back");
    failed += device_test_check(record.given_back, "the give back leaves the pool the bytes it had");
    failed += device_test_check(record.movable_taken, "a movable cell comes from the pool");
    failed += device_test_check(record.movable_agrees, "the bytes written through the handle read back");
    failed += device_test_check(record.persistent_back, "a persistent take and give leave the pool as it was");
    failed += device_test_check(record.refused, "a take past the bytes of the pool is refused");
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  platform device test: 12 checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
