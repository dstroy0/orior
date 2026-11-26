// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tower_job_test.cu: the lift as one job of the device (tower_lift_job) held to the lift of tower_lift, coefficient
// for coefficient. Each case lifts one seeded lattice both ways. The 16-bit samples go with no edge, with one edge and
// with edges on two floors, and a lattice of ints goes through the take. A value at the limit must error both ways.
// The lower as one job (tower_lower_job) gives the lanes back after the lift job, and lowers a changed crystal as
// tower_lower lowers it
#include "../../../../../../../src/cu/engine/analysis/tower/tower.h"
#include "../../../../../../../src/platform/platform.h"

#include <cuda_runtime.h>

#include <cstring>

// the room of the largest lattice of the cases, and the bits of the edges
#define JOB_TEST_LANES_MAX (4u * 32u * 48u)
#define JOB_TEST_EDGE_BITS 8u
#define JOB_TEST_EDGE_SIZE (1u << JOB_TEST_EDGE_BITS)

// the seeded words that each case draws its lattice and its tables from
static unsigned int s_seed = 0x2545F491u;

static unsigned int job_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// a permutation of the JOB_TEST_EDGE_SIZE indexes into `table`, a shuffle of the seeded words
static void job_test_permutation(unsigned int *table)
{
    for (unsigned int at = 0u; at < JOB_TEST_EDGE_SIZE; at++)
    {
        table[at] = at;
    }
    for (unsigned int at = JOB_TEST_EDGE_SIZE - 1u; at > 0u; at--)
    {
        const unsigned int other = job_test_drawn() % (at + 1u);
        const unsigned int held = table[at];
        table[at] = table[other];
        table[other] = held;
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

// the lattice lifted by tower_lift from the memory of the device, the coefficients copied back into `out`. The answer
// of tower_lift
static long job_test_lifted(const unsigned short *lanes, const int *values, const unsigned long long extent[4],
                            const TowerEdge *edges, unsigned int edge_count, int *out, unsigned int *floors)
{
    unsigned long long count = extent[0] * extent[1] * extent[2] * extent[3];
    const unsigned long long bytes = count * ((values != NULL) ? sizeof(int) : sizeof(unsigned short));
    void *device = NULL;
    if ((cudaMalloc(&device, (size_t)bytes) != cudaSuccess) ||
        (cudaMemcpy(device, (values != NULL) ? (const void *)values : (const void *)lanes, (size_t)bytes,
                    cudaMemcpyHostToDevice) != cudaSuccess))
    {
        cudaFree(device);
        return -1L;
    }
    EngineError error;
    memset(&error, 0, sizeof(error));
    const int *coefficients = NULL;
    unsigned int *scratch = NULL;
    TowerLiftRequest lift;
    memset(&lift, 0, sizeof(lift));
    // Explicit casts read the memory of the device as the lattice that crossed into it
    lift.device_lanes = (values != NULL) ? NULL : (const unsigned short *)device;
    lift.device_values = (values != NULL) ? (const int *)device : NULL;
    memcpy(lift.extent, extent, sizeof(lift.extent));
    lift.coefficients = &coefficients;
    lift.scratch = &scratch;
    lift.floors = floors;
    lift.edges = edges;
    lift.edge_count = edge_count;
    lift.error = &error;
    const long lifted = tower_lift(&lift);
    if (lifted == 0L)
    {
        cudaMemcpy(out, coefficients, (size_t)count * sizeof(int), cudaMemcpyDeviceToHost);
    }
    cudaFree(device);
    return lifted;
}

// the lattice lifted by tower_lift_job, the coefficients crossed out into `out`. The answer of tower_lift_job
static long job_test_job(const unsigned short *lanes, const int *values, const unsigned long long extent[4],
                         const TowerEdge *edges, unsigned int edge_count, int *out, unsigned int *floors)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    TowerLiftJobRequest job;
    memset(&job, 0, sizeof(job));
    job.lanes = lanes;
    job.values = values;
    memcpy(job.extent, extent, sizeof(job.extent));
    job.edges = edges;
    job.edge_count = edge_count;
    job.coefficients = out;
    job.floors = floors;
    job.error = &error;
    return tower_lift_job(&job);
}

// One case: the lattice lifted both ways, then each coefficient and the count of floors compared
static unsigned int job_test_case(const char *what, const unsigned short *lanes, const int *values,
                                  const unsigned long long extent[4], const TowerEdge *edges, unsigned int edge_count,
                                  int *lifted, int *job)
{
    const unsigned long long count = extent[0] * extent[1] * extent[2] * extent[3];
    unsigned int lifted_floors = 0u;
    unsigned int job_floors = 0u;
    const long lift_answer = job_test_lifted(lanes, values, extent, edges, edge_count, lifted, &lifted_floors);
    const long job_answer = job_test_job(lanes, values, extent, edges, edge_count, job, &job_floors);
    int agree = (lift_answer == 0L) && (job_answer == 0L) && (lifted_floors == job_floors);
    for (unsigned long long at = 0ULL; (agree != 0) && (at < count); at++)
    {
        agree = lifted[at] == job[at];
    }
    return job_test_check(agree, what);
}

// The crystal lowered by tower_lower against the lanes. First a lift by tower_lift of the lanes makes the pool of the
// tower ready. Then the crystal copies over the coefficients that the lift left. The rebuilt samples go into
// `rebuilt`. The answer of tower_lower
static long job_test_lowered(const unsigned short *lanes, const unsigned long long extent[4], const TowerEdge *edges,
                             unsigned int edge_count, const int *crystal, unsigned short *rebuilt,
                             unsigned long long *mismatches, int *room)
{
    unsigned int floors = 0u;
    if (job_test_lifted(lanes, NULL, extent, edges, edge_count, room, &floors) != 0L)
    {
        return -1L;
    }
    const unsigned long long count = extent[0] * extent[1] * extent[2] * extent[3];
    int *coefficients = NULL;
    void *device = NULL;
    EngineError error;
    memset(&error, 0, sizeof(error));
    long answer = -1L;
    if ((tower_capacity(extent, &coefficients, &error) == 0L) &&
        (cudaMemcpy(coefficients, crystal, (size_t)count * sizeof(int), cudaMemcpyHostToDevice) == cudaSuccess) &&
        (cudaMalloc(&device, (size_t)count * sizeof(unsigned short)) == cudaSuccess) &&
        (cudaMemcpy(device, lanes, (size_t)count * sizeof(unsigned short), cudaMemcpyHostToDevice) == cudaSuccess))
    {
        const unsigned short *device_rebuilt = NULL;
        TowerLowerRequest lower;
        memset(&lower, 0, sizeof(lower));
        // Explicit cast reads the memory of the device as the lanes that crossed into it
        lower.device_lanes = (const unsigned short *)device;
        memcpy(lower.extent, extent, sizeof(lower.extent));
        lower.mismatches = mismatches;
        lower.device_rebuilt = &device_rebuilt;
        lower.rebuilt = rebuilt;
        lower.edges = edges;
        lower.edge_count = edge_count;
        lower.error = &error;
        answer = tower_lower(&lower);
    }
    cudaFree(device);
    return answer;
}

// the crystal lowered by tower_lower_job against the lanes, the rebuilt samples crossed out into `rebuilt`
static long job_test_lowered_job(const unsigned short *lanes, const unsigned long long extent[4],
                                 const TowerEdge *edges, unsigned int edge_count, const int *crystal,
                                 unsigned short *rebuilt, unsigned long long *mismatches)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    TowerLowerJobRequest lower;
    memset(&lower, 0, sizeof(lower));
    lower.coefficients = crystal;
    lower.lanes = lanes;
    memcpy(lower.extent, extent, sizeof(lower.extent));
    lower.edges = edges;
    lower.edge_count = edge_count;
    lower.mismatches = mismatches;
    lower.rebuilt = rebuilt;
    lower.error = &error;
    return tower_lower_job(&lower);
}

// One round trip: the lift job and then the lower job give the lanes back with no mismatch. Then a crystal with one
// coefficient changed lowers by the job as tower_lower lowers it, sample for sample and mismatch for mismatch
static unsigned int job_test_trip(const char *what, const unsigned short *lanes, const unsigned long long extent[4],
                                  const TowerEdge *edges, unsigned int edge_count, int *crystal, int *room,
                                  unsigned short *rebuilt, unsigned short *rebuilt_job)
{
    const unsigned long long count = extent[0] * extent[1] * extent[2] * extent[3];
    unsigned int floors = 0u;
    unsigned long long miss = 1ULL;
    unsigned long long miss_job = 1ULL;
    int agree = (job_test_job(lanes, NULL, extent, edges, edge_count, crystal, &floors) == 0L) &&
                (job_test_lowered_job(lanes, extent, edges, edge_count, crystal, rebuilt_job, &miss_job) == 0L) &&
                (miss_job == 0ULL);
    for (unsigned long long at = 0ULL; (agree != 0) && (at < count); at++)
    {
        agree = rebuilt_job[at] == lanes[at];
    }
    unsigned int failed = job_test_check(agree, what);
    crystal[count / 3ULL] += 1;
    agree = (job_test_lowered(lanes, extent, edges, edge_count, crystal, rebuilt, &miss, room) == 0L) &&
            (job_test_lowered_job(lanes, extent, edges, edge_count, crystal, rebuilt_job, &miss_job) == 0L) &&
            (miss == miss_job) && (miss != 0ULL);
    for (unsigned long long at = 0ULL; (agree != 0) && (at < count); at++)
    {
        agree = rebuilt_job[at] == rebuilt[at];
    }
    failed += job_test_check(agree, "a changed crystal lowers by the job as tower_lower lowers it");
    return failed;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    unsigned short *const lanes = (unsigned short *)platform_cell_taken(JOB_TEST_LANES_MAX * sizeof(unsigned short),
                                                                        platform_place_g_program_owner, 1);
    int *const values = (int *)platform_cell_taken(JOB_TEST_LANES_MAX * sizeof(int), platform_place_g_program_owner, 1);
    int *const lifted = (int *)platform_cell_taken(JOB_TEST_LANES_MAX * sizeof(int), platform_place_g_program_owner, 1);
    int *const job = (int *)platform_cell_taken(JOB_TEST_LANES_MAX * sizeof(int), platform_place_g_program_owner, 1);
    unsigned int *const tables = (unsigned int *)platform_cell_taken(2u * JOB_TEST_EDGE_SIZE * sizeof(unsigned int),
                                                                     platform_place_g_program_owner, 1);
    unsigned short *const rebuilt = (unsigned short *)platform_cell_taken(JOB_TEST_LANES_MAX * sizeof(unsigned short),
                                                                          platform_place_g_program_owner, 1);
    unsigned short *const rebuilt_job = (unsigned short *)platform_cell_taken(
        JOB_TEST_LANES_MAX * sizeof(unsigned short), platform_place_g_program_owner, 1);
    if ((lanes == NULL) || (values == NULL) || (lifted == NULL) || (job == NULL) || (tables == NULL) ||
        (rebuilt == NULL) || (rebuilt_job == NULL))
    {
        return 1;
    }
    for (unsigned int at = 0u; at < JOB_TEST_LANES_MAX; at++)
    {
        // Explicit casts keep the low bits of the drawn words: a sample, and an int of either sign below the limit
        lanes[at] = (unsigned short)(job_test_drawn() & 0x0FFFu);
        values[at] = (int)(job_test_drawn() % 8192u) - 4096;
    }
    job_test_permutation(tables);
    job_test_permutation(tables + JOB_TEST_EDGE_SIZE);

    const unsigned long long cube[4] = {1ULL, 8ULL, 8ULL, 8ULL};
    const unsigned long long wide[4] = {1ULL, 4ULL, 32ULL, 48ULL};
    TowerEdge one[1];
    one[0].floor = 1u;
    one[0].index_bits = JOB_TEST_EDGE_BITS;
    one[0].forward = tables;
    TowerEdge two[2];
    two[0].floor = 0u;
    two[0].index_bits = JOB_TEST_EDGE_BITS;
    two[0].forward = tables;
    two[1].floor = 2u;
    two[1].index_bits = JOB_TEST_EDGE_BITS;
    two[1].forward = tables + JOB_TEST_EDGE_SIZE;

    unsigned int failed = 0u;
    failed += job_test_case("16-bit samples with no edge lift as tower_lift lifts them", lanes, NULL, cube, NULL, 0u,
                            lifted, job);
    failed += job_test_case("16-bit samples with an edge on floor 1 lift as tower_lift lifts them", lanes, NULL, cube,
                            one, 1u, lifted, job);
    failed += job_test_case("a wider lattice with edges on floors 0 and 2 lifts as tower_lift lifts it", lanes, NULL,
                            wide, two, 2u, lifted, job);
    failed += job_test_case("a lattice of ints of either sign lifts as tower_lift lifts it", NULL, values, wide, NULL,
                            0u, lifted, job);
    values[17] = ENGINE_COEFFICIENT_LIMIT;
    unsigned int floors = 0u;
    failed += job_test_check((job_test_lifted(NULL, values, wide, NULL, 0u, lifted, &floors) != 0L) &&
                                 (job_test_job(NULL, values, wide, NULL, 0u, job, &floors) != 0L),
                             "a value at the limit errors both ways");
    failed += job_test_trip("the lift job and the lower job give back the samples of the cube with one edge", lanes,
                            cube, one, 1u, job, lifted, rebuilt, rebuilt_job);
    failed += job_test_trip("the lift job and the lower job give back the samples of the wide lattice with two edges",
                            lanes, wide, two, 2u, job, lifted, rebuilt, rebuilt_job);

    platform_cell_released(rebuilt_job, platform_place_g_program_owner);
    platform_cell_released(rebuilt, platform_place_g_program_owner);
    platform_cell_released(tables, platform_place_g_program_owner);
    platform_cell_released(job, platform_place_g_program_owner);
    platform_cell_released(lifted, platform_place_g_program_owner);
    platform_cell_released(values, platform_place_g_program_owner);
    platform_cell_released(lanes, platform_place_g_program_owner);
    tower_resident_release();
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  tower job test: 9 checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
