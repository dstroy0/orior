// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// obsignatio_job_test.cu: the seal of the lanes as one job of the device (obsignatio_lanes_job) held to
// obsignatio_lanes node for node. The rows go below one chunk and across several chunks. The seal of a stream of bits
// as one job (obsignatio_bits_job) is held to obsignatio_bits signum for signum. An offset past the stream must error
// both ways
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../src/platform/platform.h"

#include <cuda_runtime.h>

#include "memoria_operor/memoria_operor.h"

// the most lanes of a case, the most nodes of its seal, and the words and messages of the stream
#define JOB_TEST_LANES_MAX (2u * 4u * 9u * 1500u)
#define JOB_TEST_NODES_MAX (2u * 4u * 9u + 2u * 4u + 2u + 1u)
#define JOB_TEST_LIMBS 4096u
#define JOB_TEST_MESSAGES 37u

// the seeded words that each case draws its bytes from
static unsigned int s_seed = 0x85EBCA6Bu;

static unsigned int job_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
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

// the nodes of the lanes sealed by obsignatio_lanes from the memory of the device, copied back into `nodes`
static long job_test_lanes(const unsigned short *lanes, const unsigned long long extent[4], unsigned char *nodes)
{
    const unsigned long long count = extent[0] * extent[1] * extent[2] * extent[3];
    const unsigned long long bytes = obsignatio_lanes_nodes(extent) * OBSIGNATIO_SIGNUM_BYTES;
    void *device_lanes = NULL;
    void *device_nodes = NULL;
    long answer = -1L;
    if ((cudaMalloc(&device_lanes, (size_t)count * sizeof(unsigned short)) == cudaSuccess) &&
        (cudaMalloc(&device_nodes, (size_t)bytes) == cudaSuccess) &&
        (cudaMemcpy(device_lanes, lanes, (size_t)count * sizeof(unsigned short), cudaMemcpyHostToDevice) ==
         cudaSuccess))
    {
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        // Explicit casts read the memory of the device as the lanes and the room of the nodes
        const ObsignatioLanesRequest request = {(const unsigned short *)device_lanes, extent,
                                                (unsigned char *)device_nodes, &error};
        answer = obsignatio_lanes(&request);
        if ((answer == 0L) && (cudaMemcpy(nodes, device_nodes, (size_t)bytes, cudaMemcpyDeviceToHost) != cudaSuccess))
        {
            answer = -1L;
        }
    }
    cudaFree(device_nodes);
    cudaFree(device_lanes);
    return answer;
}

// the nodes of the lanes sealed by obsignatio_lanes_job into `nodes`
static long job_test_lanes_job(const unsigned short *lanes, const unsigned long long extent[4], unsigned char *nodes)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const ObsignatioLanesJobRequest request = {lanes, extent, nodes, &error};
    return obsignatio_lanes_job(&request);
}

// One case of the lanes: the nodes sealed both ways and compared byte for byte
static unsigned int job_test_lanes_case(const char *what, const unsigned short *lanes,
                                        const unsigned long long extent[4], unsigned char *nodes,
                                        unsigned char *nodes_job)
{
    const unsigned long long bytes = obsignatio_lanes_nodes(extent) * OBSIGNATIO_SIGNUM_BYTES;
    const int agree =
        (job_test_lanes(lanes, extent, nodes) == 0L) && (job_test_lanes_job(lanes, extent, nodes_job) == 0L) &&
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = nodes, .other = nodes_job, .bytes = (size_t)bytes) == 0);
    return job_test_check(agree, what);
}

// the signa of the stream sealed by obsignatio_bits from the memory of the device, copied back into `signa`
static long job_test_bits(const unsigned int *limbs, const unsigned long long *offsets, unsigned long long bits,
                          const unsigned char *key, unsigned char *signa)
{
    void *device_limbs = NULL;
    void *device_offsets = NULL;
    void *device_signa = NULL;
    long answer = -1L;
    if ((cudaMalloc(&device_limbs, (JOB_TEST_LIMBS + 1u) * sizeof(unsigned int)) == cudaSuccess) &&
        (cudaMalloc(&device_offsets, JOB_TEST_MESSAGES * sizeof(unsigned long long)) == cudaSuccess) &&
        (cudaMalloc(&device_signa, JOB_TEST_MESSAGES * OBSIGNATIO_SIGNUM_BYTES) == cudaSuccess) &&
        (cudaMemset(device_limbs, 0, (JOB_TEST_LIMBS + 1u) * sizeof(unsigned int)) == cudaSuccess) &&
        (cudaMemcpy(device_limbs, limbs, JOB_TEST_LIMBS * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
         cudaSuccess) &&
        (cudaMemcpy(device_offsets, offsets, JOB_TEST_MESSAGES * sizeof(unsigned long long), cudaMemcpyHostToDevice) ==
         cudaSuccess))
    {
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        // Explicit casts read the memory of the device as the stream, the offsets and the room of the signa
        const ObsignatioBitsRequest request = {(const unsigned int *)device_limbs,
                                               (const unsigned long long *)device_offsets,
                                               JOB_TEST_MESSAGES,
                                               bits,
                                               key,
                                               OBSIGNATIO_MODE_KEYED,
                                               (unsigned char *)device_signa,
                                               &error};
        answer = obsignatio_bits(&request);
        if ((answer == 0L) && (cudaMemcpy(signa, device_signa, JOB_TEST_MESSAGES * OBSIGNATIO_SIGNUM_BYTES,
                                          cudaMemcpyDeviceToHost) != cudaSuccess))
        {
            answer = -1L;
        }
    }
    cudaFree(device_signa);
    cudaFree(device_offsets);
    cudaFree(device_limbs);
    return answer;
}

// the signa of the stream sealed by obsignatio_bits_job into `signa`
static long job_test_bits_job(const unsigned int *limbs, const unsigned long long *offsets, unsigned long long bits,
                              const unsigned char *key, unsigned char *signa)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const ObsignatioBitsJobRequest request = {limbs, offsets, JOB_TEST_MESSAGES, bits, key, OBSIGNATIO_MODE_KEYED,
                                              signa, &error};
    return obsignatio_bits_job(&request);
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const unsigned int place = platform_place_g_program_owner;
    unsigned short *const lanes =
        (unsigned short *)platform_cell_taken(JOB_TEST_LANES_MAX * sizeof(unsigned short), place, 1);
    unsigned char *const nodes =
        (unsigned char *)platform_cell_taken(JOB_TEST_NODES_MAX * OBSIGNATIO_SIGNUM_BYTES, place, 1);
    unsigned char *const nodes_job =
        (unsigned char *)platform_cell_taken(JOB_TEST_NODES_MAX * OBSIGNATIO_SIGNUM_BYTES, place, 1);
    unsigned int *const limbs = (unsigned int *)platform_cell_taken(JOB_TEST_LIMBS * sizeof(unsigned int), place, 1);
    unsigned char *const signa =
        (unsigned char *)platform_cell_taken(JOB_TEST_MESSAGES * OBSIGNATIO_SIGNUM_BYTES, place, 1);
    unsigned char *const signa_job =
        (unsigned char *)platform_cell_taken(JOB_TEST_MESSAGES * OBSIGNATIO_SIGNUM_BYTES, place, 1);
    if ((lanes == NULL) || (nodes == NULL) || (nodes_job == NULL) || (limbs == NULL) || (signa == NULL) ||
        (signa_job == NULL))
    {
        return 1;
    }
    for (unsigned int at = 0u; at < JOB_TEST_LANES_MAX; at++)
    {
        // Explicit cast keeps the low 16 bits of the drawn word as a sample
        lanes[at] = (unsigned short)(job_test_drawn() & 0xFFFFu);
    }
    for (unsigned int at = 0u; at < JOB_TEST_LIMBS; at++)
    {
        limbs[at] = job_test_drawn();
    }
    // The offsets rise by a drawn step of below 3000 bits. 37 messages end below the 131072 bits of the stream
    unsigned long long offsets[JOB_TEST_MESSAGES];
    unsigned long long at_bit = 0ULL;
    for (unsigned int message = 0u; message < JOB_TEST_MESSAGES; message++)
    {
        offsets[message] = at_bit;
        at_bit += job_test_drawn() % 3000u;
    }
    const unsigned long long bits = at_bit;
    unsigned char key[OBSIGNATIO_KEY_BYTES];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    if (obsignatio_level_key(OBSIGNATIO_LEVEL_CHUNK, key, &error) != 0L)
    {
        return 1;
    }

    const unsigned long long short_rows[4] = {1ULL, 3ULL, 5ULL, 300ULL};
    const unsigned long long long_rows[4] = {2ULL, 4ULL, 9ULL, 1500ULL};
    unsigned int failed = 0u;
    failed += job_test_lanes_case("rows below one chunk seal as obsignatio_lanes seals them", lanes, short_rows, nodes,
                                  nodes_job);
    failed += job_test_lanes_case("rows of three chunks seal as obsignatio_lanes seals them", lanes, long_rows, nodes,
                                  nodes_job);
    failed += job_test_check((job_test_bits(limbs, offsets, bits, key, signa) == 0L) &&
                                 (job_test_bits_job(limbs, offsets, bits, key, signa_job) == 0L) &&
                                 (EMBED_CALL(memor.cmp, MemoriaCfg, .src = signa, .other = signa_job,
                                             .bytes = JOB_TEST_MESSAGES * OBSIGNATIO_SIGNUM_BYTES) == 0),
                             "the messages of a stream seal as obsignatio_bits seals them");
    offsets[20] = bits + 1ULL;
    failed += job_test_check((job_test_bits(limbs, offsets, bits, key, signa) != 0L) &&
                                 (job_test_bits_job(limbs, offsets, bits, key, signa_job) != 0L),
                             "an offset past the stream errors both ways");

    platform_cell_released(signa_job, place);
    platform_cell_released(signa, place);
    platform_cell_released(limbs, place);
    platform_cell_released(nodes_job, place);
    platform_cell_released(nodes, place);
    platform_cell_released(lanes, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  obsignatio job test: 4 checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
