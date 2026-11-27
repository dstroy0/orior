// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// max_tree_paths_test.cu: the paths of the max tree on the device past its objects, with the engine linked into the
// program. The test retains frames and runs the objects of two seeded residuals. After the first it runs the slide and
// the pack, and after the second the overlap of the two frames. On the first residual it runs the bind, the proof of
// the contraction and the check of the order. The signum of the answers of each path must equal the signum of its
// case, the answer that the device path gives
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/nbody/max_tree/max_tree.h"
#include "../../../../../../../src/cu/engine/nbody/max_tree/max_tree_device_internal.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../src/platform/platform.h"

#include <cuda_runtime.h>

#include "memoria_operor/memoria_operor.h"

// the extent of each residual, its voxels, and the most bodies, probes and steps of a case
#define PATHS_TEST_DEPTH 16u
#define PATHS_TEST_HEIGHT 40u
#define PATHS_TEST_WIDTH 48u
#define PATHS_TEST_VOXELS (PATHS_TEST_DEPTH * PATHS_TEST_HEIGHT * PATHS_TEST_WIDTH)
#define PATHS_TEST_BODIES_MAX 4096u
#define PATHS_TEST_PROBES 8u
#define PATHS_TEST_STEPS 512u

// The levels of each frame that the overlap steps through, below the top of each frame. Each step writes the probes of
// both frames
#define PATHS_TEST_LEVELS 3u
static const unsigned int PATHS_TEST_OVERLAP_LEVELS[PATHS_TEST_LEVELS] = {2u, 8u, 16u};

// the most limbs of the record of one body that the pack writes
#define PATHS_TEST_LIMBS_MAX 32u

// the paths whose answers the test signs: the slide, the pack, the overlap, the bind and the proof
#define PATHS_TEST_PATHS 5u

// the pool of the test, for its residuals, its bodies and its answers
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 30u;

// the seeded words that the residuals draw their blobs from
static unsigned int s_seed = 0x165667B1u;

static unsigned int paths_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// one check, printed as it passes or fails
static unsigned int paths_test_check(int held, const char *what)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// A residual of `blobs` positive blobs over a negative background, ENGINE_RESIDUAL_LIMBS limbs a lane, as the objects
// test draws it
static void paths_test_residual(unsigned int blobs, unsigned int *residual)
{
    for (unsigned int voxel = 0u; voxel < PATHS_TEST_VOXELS; voxel++)
    {
        for (unsigned int limb = 0u; limb < ENGINE_RESIDUAL_LIMBS; limb++)
        {
            residual[(voxel * ENGINE_RESIDUAL_LIMBS) + limb] = 0xFFFFFFFFu;
        }
    }
    for (unsigned int blob = 0u; blob < blobs; blob++)
    {
        const int center_z = (int)(paths_test_drawn() % PATHS_TEST_DEPTH);
        const int center_y = (int)(paths_test_drawn() % PATHS_TEST_HEIGHT);
        const int center_x = (int)(paths_test_drawn() % PATHS_TEST_WIDTH);
        const int radius = 2 + (int)(paths_test_drawn() % 4u);
        const unsigned int crest = 400u + (paths_test_drawn() % 600u);
        for (unsigned int voxel = 0u; voxel < PATHS_TEST_VOXELS; voxel++)
        {
            const int z = (int)(voxel / (PATHS_TEST_HEIGHT * PATHS_TEST_WIDTH));
            const int y = (int)((voxel / PATHS_TEST_WIDTH) % PATHS_TEST_HEIGHT);
            const int x = (int)(voxel % PATHS_TEST_WIDTH);
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

// the first 8 bytes of the signum of `bytes` as a word, little end first, or 0 where the signum errors
static unsigned long long paths_test_signum(const void *bytes, unsigned long long count)
{
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    // Explicit cast reads the answers as the bytes that the signum reads
    const ObsignatioSignumRequest request = {(const unsigned char *)bytes, count, NULL, OBSIGNATIO_MODE_HASH, signum,
                                             OBSIGNATIO_SIGNUM_BYTES,      &error};
    if (obsignatio_signum(&request) != 0L)
    {
        return 0ULL;
    }
    unsigned long long word = 0ULL;
    for (unsigned int at = 0u; at < 8u; at++)
    {
        word |= (unsigned long long)signum[at] << (8u * at);
    }
    return word;
}

// The objects of a residual on the device, its bodies in `bodies`. The count of the bodies, or -1 where the run errors
static long paths_test_objects(const unsigned int *residual, EngineBody *bodies)
{
    const size_t bytes = (size_t)PATHS_TEST_VOXELS * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int);
    void *device = NULL;
    long found = -1L;
    if ((cudaMalloc(&device, bytes) == cudaSuccess) &&
        (cudaMemcpy(device, residual, bytes, cudaMemcpyHostToDevice) == cudaSuccess))
    {
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        MaxTreeObjectsRequest request;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
        // Explicit cast reads the memory of the device as the residual that crossed into it
        request.device_residual = (const unsigned int *)device;
        request.depth = PATHS_TEST_DEPTH;
        request.height = PATHS_TEST_HEIGHT;
        request.width = PATHS_TEST_WIDTH;
        request.capacity = PATHS_TEST_BODIES_MAX;
        request.bodies = bodies;
        request.error = &error;
        found = max_tree_objects(&request);
    }
    cudaFree(device);
    return found;
}

// The answers of every path of a run, which the test signs path by path
struct PathsTestAnswers
{
    MaxTreeSlideStep slide_steps[PATHS_TEST_STEPS];
    unsigned int slide_partitions[PATHS_TEST_STEPS * PATHS_TEST_PROBES];
    unsigned int slide_counts[4];
    unsigned int pack_words[PATHS_TEST_BODIES_MAX * PATHS_TEST_LIMBS_MAX];
    unsigned long long pack_mismatches;
    MaxTreeOverlapStep overlap_steps[PATHS_TEST_STEPS];
    MaxTreeOverlapProbe overlap_probes[2u * PATHS_TEST_LEVELS * 2u * PATHS_TEST_PROBES];
    unsigned int overlap_count;
    unsigned char bind_bound[PATHS_TEST_VOXELS * 3u];
    unsigned int bind_counts[2];
    unsigned char proof_bound[PATHS_TEST_VOXELS * 3u];
    unsigned int proof_rounds;
};

// the slide over the levels of the first frame, probed at the peaks of its first bodies, into `answers`
static long paths_test_slide(const unsigned int *probes, PathsTestAnswers *answers)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    MaxTreeSlideRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    request.probe_voxels = probes;
    request.probe_count = PATHS_TEST_PROBES;
    request.steps = answers->slide_steps;
    request.partitions = answers->slide_partitions;
    request.step_capacity = PATHS_TEST_STEPS;
    request.step_count = &answers->slide_counts[0];
    request.top_level = &answers->slide_counts[1];
    request.threshold = &answers->slide_counts[2];
    request.labelings = &answers->slide_counts[3];
    request.error = &error;
    return max_tree_slide(&request);
}

// the pack of the bodies of the first frame into words of the device, copied back into `answers`
static long paths_test_pack(long bodies, PathsTestAnswers *answers)
{
    MaxTreeLayout layout;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &layout, .bytes = sizeof(layout), .val = 0);
    max_tree_layout(PATHS_TEST_DEPTH, PATHS_TEST_HEIGHT, PATHS_TEST_WIDTH, 2u, 1u, &layout);
    const size_t bytes = (size_t)bodies * layout.limbs * sizeof(unsigned int);
    if ((layout.limbs > PATHS_TEST_LIMBS_MAX) || (bytes == 0u))
    {
        return -1L;
    }
    void *device = NULL;
    long packed = -1L;
    if ((cudaMalloc(&device, bytes) == cudaSuccess) && (cudaMemset(device, 0, bytes) == cudaSuccess))
    {
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        MaxTreePackRequest request;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
        request.layout = &layout;
        // Explicit cast reads the memory of the device as the words of the pack
        request.device_magnitudes = (unsigned int *)device;
        request.mismatches = &answers->pack_mismatches;
        request.error = &error;
        packed = max_tree_pack(&request);
        if ((packed == MAX_TREE_ERROR) ||
            (cudaMemcpy(answers->pack_words, device, bytes, cudaMemcpyDeviceToHost) != cudaSuccess))
        {
            packed = -1L;
        }
    }
    cudaFree(device);
    return packed;
}

// the overlap of the two retained frames, probed at the peaks of the first bodies of each, into `answers`
static long paths_test_overlap(const unsigned int *earlier, const unsigned int *later, PathsTestAnswers *answers)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    MaxTreeOverlapRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    request.probe_voxels[0] = earlier;
    request.probe_voxels[1] = later;
    request.probe_counts[0] = PATHS_TEST_PROBES;
    request.probe_counts[1] = PATHS_TEST_PROBES;
    request.earlier_levels = PATHS_TEST_OVERLAP_LEVELS;
    request.earlier_level_count = PATHS_TEST_LEVELS;
    request.later_levels = PATHS_TEST_OVERLAP_LEVELS;
    request.later_level_count = PATHS_TEST_LEVELS;
    request.probes = answers->overlap_probes;
    request.steps = answers->overlap_steps;
    request.step_capacity = PATHS_TEST_STEPS;
    request.step_count = &answers->overlap_count;
    request.error = &error;
    return max_tree_overlap(&request);
}

// The bind of the first residual on the host, its faces in `answers`. It proves the bound faces at the levels of the
// values of the first PATHS_TEST_LEVELS probes
static long paths_test_bind(const unsigned int *residual, const unsigned int *levels, PathsTestAnswers *answers)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    unsigned long long bound_microseconds = 0ULL;
    unsigned long long proved_microseconds = 0ULL;
    MaxTreeBindRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    request.residual = residual;
    request.depth = PATHS_TEST_DEPTH;
    request.height = PATHS_TEST_HEIGHT;
    request.width = PATHS_TEST_WIDTH;
    request.levels = levels;
    request.level_count = PATHS_TEST_LEVELS;
    request.bound = answers->bind_bound;
    request.rounds = &answers->bind_counts[0];
    request.levels_equal = &answers->bind_counts[1];
    request.bound_microseconds = &bound_microseconds;
    request.proved_microseconds = &proved_microseconds;
    request.error = &error;
    return max_tree_bind(&request);
}

// The frame that the last objects retained, checked against the codes that they built and against the residual. The
// codes of the frame must equal the codes of the max tree word for word. Its top must equal the top of the tree. The
// values of the frame must hold, at the code of each selected voxel, the limbs of the residual of that voxel. 1 where
// all agree
static int paths_test_frame(const unsigned int *residual)
{
    const MaxTreeResident *const resident = &g_max_tree_resident;
    const unsigned int place = resident->frame_current;
    const unsigned int top = resident->frame_top[place];
    const unsigned long long value_words = ((unsigned long long)top + 2ULL) * ENGINE_RESIDUAL_LIMBS;
    unsigned int *const codes = (unsigned int *)platform_cell_taken(PATHS_TEST_VOXELS * sizeof(unsigned int),
                                                                    platform_place_g_program_owner, 1);
    unsigned int *const frame_codes = (unsigned int *)platform_cell_taken(PATHS_TEST_VOXELS * sizeof(unsigned int),
                                                                          platform_place_g_program_owner, 1);
    unsigned int *const values =
        (unsigned int *)platform_cell_taken(value_words * sizeof(unsigned int), platform_place_g_program_owner, 1);
    int agree = (codes != NULL) && (frame_codes != NULL) && (values != NULL) && (resident->frame_ready[place] != 0u) &&
                (top == resident->top) &&
                (cudaMemcpy(codes, resident->code, PATHS_TEST_VOXELS * sizeof(unsigned int), cudaMemcpyDeviceToHost) ==
                 cudaSuccess) &&
                (cudaMemcpy(frame_codes, resident->frame_code[place], PATHS_TEST_VOXELS * sizeof(unsigned int),
                            cudaMemcpyDeviceToHost) == cudaSuccess) &&
                (cudaMemcpy(values, resident->frame_values[place], (size_t)value_words * sizeof(unsigned int),
                            cudaMemcpyDeviceToHost) == cudaSuccess) &&
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = codes, .other = frame_codes,
                            .bytes = PATHS_TEST_VOXELS * sizeof(unsigned int)) == 0);
    for (unsigned int voxel = 0u; (agree != 0) && (voxel < PATHS_TEST_VOXELS); voxel++)
    {
        const unsigned int *const lane = &residual[voxel * ENGINE_RESIDUAL_LIMBS];
        if ((lane[ENGINE_RESIDUAL_LIMBS - 1u] != 0u) || (lane[0] == 0u))
        {
            continue;
        }
        agree = (codes[voxel] <= top) &&
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &values[(size_t)codes[voxel] * ENGINE_RESIDUAL_LIMBS],
                            .other = lane, .bytes = ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int)) == 0);
    }
    platform_cell_released(values, platform_place_g_program_owner);
    platform_cell_released(frame_codes, platform_place_g_program_owner);
    platform_cell_released(codes, platform_place_g_program_owner);
    return agree;
}

// the peaks of the first PATHS_TEST_PROBES bodies, which the slide and the overlap probe
static void paths_test_probes(const EngineBody *bodies, long found, unsigned int *probes)
{
    for (unsigned int probe = 0u; probe < PATHS_TEST_PROBES; probe++)
    {
        // Explicit cast takes a count of bodies above 0, which an unsigned long counts
        probes[probe] = bodies[probe % (unsigned long)found].peak;
    }
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the first word of the signum of each path, the answer of the device path
    static const unsigned long long SIGNA[PATHS_TEST_PATHS] = {0xa993fcb66217cbc7ULL, 0xc4198cd0b3156a48ULL,
                                                               0xecf4adf0941a03c5ULL, 0xc951033a573443dfULL,
                                                               0x8b958cc4b2bef60fULL};
    static const char *const NAMES[PATHS_TEST_PATHS] = {"the slide", "the pack", "the overlap", "the bind",
                                                        "the proof of the contraction"};
    const unsigned int place = platform_place_g_program_owner;
    const unsigned long long residual_bytes =
        (unsigned long long)PATHS_TEST_VOXELS * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int);
    unsigned int *const first = (unsigned int *)platform_cell_taken(residual_bytes, place, 1);
    unsigned int *const second = (unsigned int *)platform_cell_taken(residual_bytes, place, 1);
    EngineBody *const bodies = (EngineBody *)platform_cell_taken(PATHS_TEST_BODIES_MAX * sizeof(EngineBody), place, 1);
    PathsTestAnswers *const answers = (PathsTestAnswers *)platform_cell_taken(sizeof(PathsTestAnswers), place, 1);
    if ((first == NULL) || (second == NULL) || (bodies == NULL) || (answers == NULL))
    {
        return 1;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = answers, .bytes = sizeof(*answers), .val = 0);
    paths_test_residual(14u, first);
    paths_test_residual(14u, second);
    unsigned int earlier[PATHS_TEST_PROBES];
    unsigned int later[PATHS_TEST_PROBES];
    unsigned long long words[PATHS_TEST_PATHS] = {0ULL, 0ULL, 0ULL, 0ULL, 0ULL};

    unsigned int failed = paths_test_check(max_tree_retain_frames() == 1, "the max tree retains its frames");
    const long first_found = paths_test_objects(first, bodies);
    failed += paths_test_check(first_found > 0L, "the objects of the first frame are found");
    failed += paths_test_check(paths_test_frame(first),
                               "the first frame retains the codes of the tree and the values of the residual");
    if (first_found > 0L)
    {
        paths_test_probes(bodies, first_found, earlier);
        const int slid = paths_test_slide(earlier, answers) == 0L;
        words[0] = slid ? paths_test_signum(answers->slide_steps, sizeof(answers->slide_steps) +
                                                                      sizeof(answers->slide_partitions) +
                                                                      sizeof(answers->slide_counts))
                        : 0ULL;
        const int packed = paths_test_pack(first_found, answers) != -1L;
        words[1] = packed ? paths_test_signum(answers->pack_words,
                                              sizeof(answers->pack_words) + sizeof(answers->pack_mismatches))
                          : 0ULL;
    }
    const long second_found = paths_test_objects(second, bodies);
    failed += paths_test_check(second_found > 0L, "the objects of the second frame are found");
    failed += paths_test_check(paths_test_frame(second),
                               "the second frame retains the codes of the tree and the values of the residual");
    if ((first_found > 0L) && (second_found > 0L))
    {
        paths_test_probes(bodies, second_found, later);
        const int overlapped = paths_test_overlap(earlier, later, answers) == 0L;
        words[2] = overlapped ? paths_test_signum(answers->overlap_steps, sizeof(answers->overlap_steps) +
                                                                              sizeof(answers->overlap_probes) +
                                                                              sizeof(answers->overlap_count))
                              : 0ULL;
    }
    const int bound = (first_found > 0L) && (paths_test_bind(first, earlier, answers) != MAX_TREE_ERROR);
    words[3] = bound
                   ? paths_test_signum(answers->bind_bound, sizeof(answers->bind_bound) + sizeof(answers->bind_counts))
                   : 0ULL;
    const int proved = max_tree_poc(first, PATHS_TEST_DEPTH, PATHS_TEST_HEIGHT, PATHS_TEST_WIDTH, answers->proof_bound,
                                    &answers->proof_rounds) != 0;
    words[4] =
        proved ? paths_test_signum(answers->proof_bound, sizeof(answers->proof_bound) + sizeof(answers->proof_rounds))
               : 0ULL;
    EngineError order_error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &order_error, .bytes = sizeof(order_error), .val = 0);
    failed += paths_test_check(
        max_tree_order_agrees(first, PATHS_TEST_DEPTH, PATHS_TEST_HEIGHT, PATHS_TEST_WIDTH, &order_error) != 0,
        "the order of the device agrees with the order of the host");
    for (unsigned int path = 0u; path < PATHS_TEST_PATHS; path++)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, NAMES[path]);
        mmgr_line_put(output_line, ": signum 0x");
        mmgr_line_hex(output_line, words[path], 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        failed += paths_test_check((words[path] != 0ULL) && (words[path] == SIGNA[path]),
                                   "the answers of the path are the answer of the device path");
    }
    platform_cell_released(answers, place);
    platform_cell_released(bodies, place);
    platform_cell_released(second, place);
    platform_cell_released(first, place);
    max_tree_resident_release();
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  max tree paths test: 11 checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
