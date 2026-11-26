// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_crossing_test.cu: one job through the job interface of tessera (tessera_job.h). The host crosses in a lattice
// of values and an empty answer. One cooperative launch runs the job. Its first stage squares each value into a cell
// that the job takes on the device. Its second stage, after a sync of the grid, counts the odd squares and sums every
// square, and the job decides from the count whether it takes a second cell. The host crosses the answer out and agrees
// with it
#include "../../../../../../../src/cu/engine/runtime/daemon/tessera_job.h"
#include "../../../../../../../src/platform/platform.h"
#include "../../../../../../../src/platform/platform_device.h"

#include <cooperative_groups.h>
#include <cuda_runtime.h>

// the values of the lattice, and the threads of each block of the job
#define JOB_TEST_VALUES 100000u
#define JOB_TEST_THREADS 256u

// What the job answers, which the host crosses out and agrees with
typedef struct
{
    unsigned long long sum; // The sum of the squares of the values.
    unsigned long long odd; // The count of the odd squares.
    unsigned int decided;   // 1 where the count reached half the values and the job took its second cell.
    unsigned int taken;     // 1 where each cell that the job took came.
} JobTestAnswer;

// the cell of the squares, which the first thread of the job takes for the job
static __device__ unsigned long long *s_squares;

// The job: the squares, a sync of the grid, then the sum and the count in the first thread, which decides the rest
__global__ void job_test_kernel(const unsigned int *values, JobTestAnswer *answer)
{
    cooperative_groups::grid_group grid = cooperative_groups::this_grid();
    const unsigned long long first = grid.thread_rank();
    const unsigned long long stride = grid.size();
    if (first == 0ULL)
    {
        // Explicit cast takes the address of the cell as its words
        s_squares = (unsigned long long *)platform_device_cell_taken(
            (unsigned long long)JOB_TEST_VALUES * sizeof(unsigned long long), platform_place_g_engine_owner, 1);
        answer->taken = (unsigned int)(s_squares != NULL);
    }
    grid.sync();
    if (s_squares == NULL)
    {
        return;
    }
    for (unsigned long long at = first; at < JOB_TEST_VALUES; at += stride)
    {
        s_squares[at] = (unsigned long long)values[at] * (unsigned long long)values[at];
    }
    grid.sync();
    if (first == 0ULL)
    {
        unsigned long long sum = 0ULL;
        unsigned long long odd = 0ULL;
        for (unsigned int at = 0u; at < JOB_TEST_VALUES; at++)
        {
            sum += s_squares[at];
            odd += s_squares[at] & 1ULL;
        }
        answer->sum = sum;
        answer->odd = odd;
        // the decision of the job, made on the device: a second cell where half the squares or more are odd
        answer->decided = 0u;
        if ((odd * 2ULL) >= JOB_TEST_VALUES)
        {
            void *const second = platform_device_cell_taken(64ULL, platform_place_g_engine_owner, 1);
            answer->decided = 1u;
            answer->taken = answer->taken & (unsigned int)(second != NULL);
            platform_device_cell_released(second, platform_place_g_engine_owner);
        }
        platform_device_cell_released(s_squares, platform_place_g_engine_owner);
    }
}

// one check, printed as it passes or fails
static unsigned int job_test_check(int held, const char *what)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the values on the host, and the answer the host expects of them
    unsigned int *const values = (unsigned int *)platform_cell_taken(
        (unsigned long long)JOB_TEST_VALUES * sizeof(unsigned int), platform_place_g_program_owner, 1);
    if (values == NULL)
    {
        return 1;
    }
    unsigned long long sum = 0ULL;
    unsigned long long odd = 0ULL;
    for (unsigned int at = 0u; at < JOB_TEST_VALUES; at++)
    {
        values[at] = (at * 2654435761u) >> 16u;
        sum += (unsigned long long)values[at] * (unsigned long long)values[at];
        odd += ((unsigned long long)values[at] * (unsigned long long)values[at]) & 1ULL;
    }
    void *device_values = NULL;
    void *device_answer = NULL;
    const int crossed = tessera_job_crossed_in(values, (unsigned long long)JOB_TEST_VALUES * sizeof(unsigned int),
                                               platform_place_g_engine_owner, &device_values) &&
                        tessera_job_cell_taken(sizeof(JobTestAnswer), platform_place_g_engine_owner, &device_answer);
    unsigned int failed = job_test_check(crossed, "the values and the empty answer cross in");
    void *args[2] = {&device_values, &device_answer};
    const int ran = crossed && tessera_job_ran((const void *)job_test_kernel, JOB_TEST_THREADS, args);
    failed += job_test_check(ran, "the job runs as one cooperative launch");
    JobTestAnswer answer = {0ULL, 0ULL, 0u, 0u};
    failed += job_test_check(ran && tessera_job_crossed_out(&answer, device_answer, sizeof(answer)),
                             "the answer crosses out");
    tessera_job_given(device_answer, platform_place_g_engine_owner);
    tessera_job_given(device_values, platform_place_g_engine_owner);
    platform_cell_released(values, platform_place_g_program_owner);
    failed += job_test_check(answer.taken != 0u, "each cell the job took on the device came");
    failed += job_test_check(answer.sum == sum, "the sum of the squares is the host's");
    failed += job_test_check(answer.odd == odd, "the count of the odd squares is the host's");
    failed += job_test_check(answer.decided == (unsigned int)((odd * 2ULL) >= JOB_TEST_VALUES),
                             "the job decided on the device as the host decides");
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  tessera crossing test: 7 checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
