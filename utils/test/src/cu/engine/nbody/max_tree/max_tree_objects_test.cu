// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// max_tree_objects_test.cu: the objects of the max tree on the device (max_tree_objects), with the engine linked into
// the program. Each case builds a seeded residual of positive blobs over a negative background, and runs the objects
// with its grade off and on. The mass of each body must equal the count of the voxels that carry its peak as their
// label. The signum of the bodies, the labels, the positive words and the level code must equal the signum of its
// case, the answer that the device path gives
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/nbody/max_tree/max_tree.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../src/platform/platform.h"

#include <cuda_runtime.h>

#include <cstring>

// the most voxels of a case, and the most bodies that a case holds
#define OBJECTS_TEST_VOXELS_MAX (16u * 40u * 48u)
#define OBJECTS_TEST_BODIES_MAX 4096u

// the cases: their extents, their blobs, and their grades
#define OBJECTS_TEST_CASES 4u

// the pool of the test, for its residuals, its bodies and its labels
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 30u;

// the seeded words that each case draws its blobs from
static unsigned int s_seed = 0x27D4EB2Fu;

static unsigned int objects_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// one check, printed as it passes or fails
static unsigned int objects_test_check(int held, const char *what)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// One case: its extent, its count of blobs and its grade
struct ObjectsTestCase
{
    unsigned int depth;
    unsigned int height;
    unsigned int width;
    unsigned int blobs;
    unsigned int grade;
};

// The residual of a case in `residual`, ENGINE_RESIDUAL_LIMBS limbs a lane. A lane inside a blob holds a positive
// value that falls with the distance from the center of the blob. Every other lane holds -1, all limbs of all ones
static void objects_test_residual(const ObjectsTestCase *test_case, unsigned int *residual)
{
    const unsigned int voxels = test_case->depth * test_case->height * test_case->width;
    for (unsigned int voxel = 0u; voxel < voxels; voxel++)
    {
        for (unsigned int limb = 0u; limb < ENGINE_RESIDUAL_LIMBS; limb++)
        {
            residual[(voxel * ENGINE_RESIDUAL_LIMBS) + limb] = 0xFFFFFFFFu;
        }
    }
    for (unsigned int blob = 0u; blob < test_case->blobs; blob++)
    {
        const int center_z = (int)(objects_test_drawn() % test_case->depth);
        const int center_y = (int)(objects_test_drawn() % test_case->height);
        const int center_x = (int)(objects_test_drawn() % test_case->width);
        const int radius = 2 + (int)(objects_test_drawn() % 4u);
        const unsigned int crest = 400u + (objects_test_drawn() % 600u);
        for (unsigned int voxel = 0u; voxel < voxels; voxel++)
        {
            const int z = (int)(voxel / (test_case->height * test_case->width));
            const int y = (int)((voxel / test_case->width) % test_case->height);
            const int x = (int)(voxel % test_case->width);
            const int distance =
                (z - center_z) * (z - center_z) + (y - center_y) * (y - center_y) + (x - center_x) * (x - center_x);
            if (distance > radius * radius)
            {
                continue;
            }
            // Explicit cast takes a distance below the square of the radius, which the crest stays above
            const unsigned int value = crest - (unsigned int)(distance * 16);
            unsigned int *const lane = &residual[voxel * ENGINE_RESIDUAL_LIMBS];
            const unsigned int held = (lane[ENGINE_RESIDUAL_LIMBS - 1u] == 0u) ? lane[0] : 0u;
            for (unsigned int limb = 0u; limb < ENGINE_RESIDUAL_LIMBS; limb++)
            {
                lane[limb] = 0u;
            }
            lane[0] = (value > held) ? value : held;
        }
    }
}

// The outputs of one run: the bodies, the labels, the positive words and the level code
struct ObjectsTestRun
{
    EngineBody bodies[OBJECTS_TEST_BODIES_MAX];
    unsigned int labels[OBJECTS_TEST_VOXELS_MAX];
    unsigned long long words[(OBJECTS_TEST_VOXELS_MAX + 63u) / 64u];
    unsigned int level_code;
    long found;
};

// the objects of the residual on the device, into `run`
static long objects_test_run(const ObjectsTestCase *test_case, const unsigned int *residual, ObjectsTestRun *run)
{
    const unsigned int voxels = test_case->depth * test_case->height * test_case->width;
    const size_t bytes = (size_t)voxels * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int);
    void *device = NULL;
    long found = -1L;
    if ((cudaMalloc(&device, bytes) == cudaSuccess) &&
        (cudaMemcpy(device, residual, bytes, cudaMemcpyHostToDevice) == cudaSuccess))
    {
        EngineError error;
        memset(&error, 0, sizeof(error));
        unsigned int faces_differ = 0u;
        unsigned int proof_count = 0u;
        MaxTreeObjectsRequest request;
        memset(&request, 0, sizeof(request));
        // Explicit cast reads the memory of the device as the residual that crossed into it
        request.device_residual = (const unsigned int *)device;
        request.depth = test_case->depth;
        request.height = test_case->height;
        request.width = test_case->width;
        request.capacity = OBJECTS_TEST_BODIES_MAX;
        request.bodies = run->bodies;
        request.labels = run->labels;
        request.positive_words = run->words;
        request.grade = test_case->grade;
        request.faces_differ = &faces_differ;
        request.level_code = &run->level_code;
        request.proof_count = &proof_count;
        request.error = &error;
        found = max_tree_objects(&request);
    }
    cudaFree(device);
    return found;
}

// 1 where the mass of each body is the count of the voxels that carry its peak as their label
static int objects_test_masses(const ObjectsTestCase *test_case, const unsigned int *residual,
                               const ObjectsTestRun *run)
{
    const unsigned int voxels = test_case->depth * test_case->height * test_case->width;
    int agree = run->found > 0L;
    for (long body = 0L; (agree != 0) && (body < run->found); body++)
    {
        unsigned int counted = 0u;
        for (unsigned int voxel = 0u; voxel < voxels; voxel++)
        {
            const unsigned int *const lane = &residual[voxel * ENGINE_RESIDUAL_LIMBS];
            const int selected = (lane[ENGINE_RESIDUAL_LIMBS - 1u] == 0u) && (lane[0] != 0u);
            counted += (selected && (run->labels[voxel] == run->bodies[body].peak)) ? 1u : 0u;
        }
        agree = counted == run->bodies[body].mass;
    }
    return agree;
}

// the signum of the outputs of a run: its bodies, its labels, its positive words and its level code
static int objects_test_signum(const ObjectsTestCase *test_case, const ObjectsTestRun *run,
                               unsigned char signum[OBSIGNATIO_SIGNUM_BYTES])
{
    const unsigned int voxels = test_case->depth * test_case->height * test_case->width;
    const unsigned long long body_bytes = (unsigned long long)run->found * sizeof(EngineBody);
    const unsigned long long label_bytes = (unsigned long long)voxels * sizeof(unsigned int);
    const unsigned long long word_bytes = (unsigned long long)((voxels + 63u) / 64u) * sizeof(unsigned long long);
    const unsigned long long bytes = body_bytes + label_bytes + word_bytes + sizeof(run->level_code);
    unsigned char *const message = (unsigned char *)platform_cell_taken(bytes, platform_place_g_program_owner, 1);
    if (message == NULL)
    {
        return 0;
    }
    memcpy(message, run->bodies, (size_t)body_bytes);
    memcpy(message + body_bytes, run->labels, (size_t)label_bytes);
    memcpy(message + body_bytes + label_bytes, run->words, (size_t)word_bytes);
    memcpy(message + body_bytes + label_bytes + word_bytes, &run->level_code, sizeof(run->level_code));
    EngineError error;
    memset(&error, 0, sizeof(error));
    const ObsignatioSignumRequest request = {
        message, bytes, NULL, OBSIGNATIO_MODE_HASH, signum, OBSIGNATIO_SIGNUM_BYTES, &error};
    const int signed_run = obsignatio_signum(&request) == 0L;
    platform_cell_released(message, platform_place_g_program_owner);
    return signed_run;
}

// the first 8 bytes of a signum as a word, little end first, which the test prints and compares
static unsigned long long objects_test_word(const unsigned char signum[OBSIGNATIO_SIGNUM_BYTES])
{
    unsigned long long word = 0ULL;
    for (unsigned int at = 0u; at < 8u; at++)
    {
        word |= (unsigned long long)signum[at] << (8u * at);
    }
    return word;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    static const ObjectsTestCase CASES[OBJECTS_TEST_CASES] = {
        {8u, 24u, 32u, 6u, 0u}, {8u, 24u, 32u, 6u, 1u}, {16u, 40u, 48u, 14u, 0u}, {16u, 40u, 48u, 14u, 1u}};
    // the first word of the signum of each case, the answer of the device path
    static const unsigned long long SIGNA[OBJECTS_TEST_CASES] = {0xa8963461d7a1b436ULL, 0xa8963461d7a1b436ULL,
                                                                 0x3514b3eb4b933759ULL, 0x3514b3eb4b933759ULL};
    const unsigned int place = platform_place_g_program_owner;
    unsigned int *const residual = (unsigned int *)platform_cell_taken(
        (unsigned long long)OBJECTS_TEST_VOXELS_MAX * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int), place, 1);
    ObjectsTestRun *const run = (ObjectsTestRun *)platform_cell_taken(sizeof(ObjectsTestRun), place, 1);
    if ((residual == NULL) || (run == NULL))
    {
        return 1;
    }
    unsigned int failed = 0u;
    for (unsigned int at = 0u; at < OBJECTS_TEST_CASES; at++)
    {
        const ObjectsTestCase *const test_case = &CASES[at];
        // The residual of a graded case is the residual of the case before it. The seed thus draws it once
        if (test_case->grade == 0u)
        {
            objects_test_residual(test_case, residual);
        }
        memset(run, 0, sizeof(*run));
        run->found = objects_test_run(test_case, residual, run);
        unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
        const int signed_run = (run->found > 0L) && objects_test_signum(test_case, run, signum);
        const unsigned long long word = signed_run ? objects_test_word(signum) : 0ULL;
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  case ");
        mmgr_line_decimal(output_line, at, 1u);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, (unsigned long long)((run->found > 0L) ? run->found : 0L), 1u);
        mmgr_line_put(output_line, " bodies, level code ");
        mmgr_line_decimal(output_line, run->level_code, 1u);
        mmgr_line_put(output_line, ", signum 0x");
        mmgr_line_hex(output_line, word, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        failed += objects_test_check(objects_test_masses(test_case, residual, run),
                                     "the mass of each body is the count of the voxels of its label");
        failed += objects_test_check(signed_run && (word == SIGNA[at]),
                                     "the bodies, labels, words and level code are the answer of the device path");
    }
    platform_cell_released(run, place);
    platform_cell_released(residual, place);
    max_tree_resident_release();
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  max tree objects test: 8 checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
