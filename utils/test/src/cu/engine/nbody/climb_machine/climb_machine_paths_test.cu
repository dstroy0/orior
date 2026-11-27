// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// climb_machine_paths_test.cu: the paths of the climb machine, with the engine linked into the program. Seeded frames
// of blobs drift between frames. The machine stores three frames, climbs three pairs of them, then boxes, cores and
// measures them. It then stores two more frames over the first two and does the same again. The machine runs once with
// the land at the peak and once with the land by mass along a spiral. The signum of the answers of each path must equal
// the signum of its case, the answer that the device path gives
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/nbody/climb_machine/climb_machine.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../includes/platform/platform.h"

#include <cuda_runtime.h>

#include "memoria_operor/memoria_operor.h"

// the extent of each frame, its voxels and the words of its positive bits
#define CLIMB_TEST_DEPTH 16u
#define CLIMB_TEST_HEIGHT 48u
#define CLIMB_TEST_WIDTH 56u
#define CLIMB_TEST_VOXELS (CLIMB_TEST_DEPTH * CLIMB_TEST_HEIGHT * CLIMB_TEST_WIDTH)
#define CLIMB_TEST_WORDS ((CLIMB_TEST_VOXELS + 63u) / 64u)

// the frames of the test, the frames that the machine holds at once, and the most leaves of a frame
#define CLIMB_TEST_FRAMES 5u
#define CLIMB_TEST_HELD 3u
#define CLIMB_TEST_LEAVES_MAX 32u

// the pairs that each round climbs, and the rounds
#define CLIMB_TEST_PAIRS 3u
#define CLIMB_TEST_ROUNDS 2u

// the blobs of each frame. Each frame has more runs than the frame before it. The store thus grows the runs
static const unsigned int CLIMB_TEST_BLOBS[CLIMB_TEST_FRAMES] = {8u, 10u, 12u, 16u, 20u};

// the step of each blob from one frame to the next, before its jitter
static const int CLIMB_TEST_DRIFT[3] = {1, 2, -2};

// the paths whose answers the test signs, for each of the two modes of the land
#define CLIMB_TEST_PATHS 4u
#define CLIMB_TEST_MODES 2u

// the most bytes of the answers of one path, over both rounds
#define CLIMB_TEST_ANSWER_BYTES (1ULL << 20u)

// the pool of the test, for its frames and its answers
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 28u;

// the seeded words that the frames draw their blobs from
static unsigned int s_seed = 0x2F6B1C3Du;

static unsigned int climb_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// a mix of four words into one, for the holes of the blobs
static unsigned int climb_test_mixed(unsigned int one, unsigned int two, unsigned int three, unsigned int four)
{
    unsigned int word = (one * 0x9E3779B1u) ^ (two * 0x85EBCA77u) ^ (three * 0xC2B2AE3Du) ^ (four * 0x27D4EB2Fu);
    word ^= word >> 15u;
    word *= 0x2C1B3C6Du;
    word ^= word >> 12u;
    return word;
}

// the count of the checks, and one check, printed as it passes or fails
static unsigned int s_checks = 0u;

static unsigned int climb_test_check(int held, const char *what)
{
    s_checks += 1u;
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// the blobs that the frames draw from: the place and the radius of each in the first frame
typedef struct
{
    int center[CLIMB_TEST_LEAVES_MAX][3];
    int radius[CLIMB_TEST_LEAVES_MAX];
} ClimbTestBlobs;

// One frame as the machine stores it. Each voxel of a blob holds the peak of its blob as its label, and else 0. The
// positive bit of a voxel is 1 where a blob labels it, as the box asks. The contacts of a leaf are the blobs that
// touch it
typedef struct
{
    unsigned int leaves;
    int center[CLIMB_TEST_LEAVES_MAX][3];
    unsigned int peaks[CLIMB_TEST_LEAVES_MAX];
    unsigned int contact_start[CLIMB_TEST_LEAVES_MAX + 1u];
    unsigned int contacts[CLIMB_TEST_LEAVES_MAX * CLIMB_TEST_LEAVES_MAX];
    unsigned int labels[CLIMB_TEST_VOXELS];
    unsigned long long positive[CLIMB_TEST_WORDS];
} ClimbTestFrame;

static int climb_test_clamped(int value, unsigned int extent)
{
    // Explicit cast takes an extent of the frame below 2^31, which an int holds
    const int last = (int)extent - 2;
    return (value < 1) ? 1 : ((value > last) ? last : value);
}

static void climb_test_blobs_drawn(ClimbTestBlobs *blobs)
{
    for (unsigned int blob = 0u; blob < CLIMB_TEST_LEAVES_MAX; blob++)
    {
        // Explicit casts take each draw below the extent of the frame, which an int holds
        blobs->center[blob][0] = 1 + (int)(climb_test_drawn() % (CLIMB_TEST_DEPTH - 2u));
        blobs->center[blob][1] = 1 + (int)(climb_test_drawn() % (CLIMB_TEST_HEIGHT - 10u));
        blobs->center[blob][2] = 9 + (int)(climb_test_drawn() % (CLIMB_TEST_WIDTH - 10u));
        blobs->radius[blob] = 2 + (int)(climb_test_drawn() % 2u);
    }
}

// the voxel of a place in the frame
static unsigned int climb_test_voxel(const int *place)
{
    // Explicit casts take each coordinate, inside the frame and so not negative, as an unsigned int
    return (((unsigned int)place[0] * CLIMB_TEST_HEIGHT) + (unsigned int)place[1]) * CLIMB_TEST_WIDTH +
           (unsigned int)place[2];
}

// The frame `at` of the blobs. Each blob steps CLIMB_TEST_DRIFT a frame, with a jitter of at most 1 an axis. A blob
// whose peak meets the peak of an earlier blob steps along the width until its peak is free
static void climb_test_frame_drawn(const ClimbTestBlobs *blobs, unsigned int at, ClimbTestFrame *frame)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = frame, .bytes = sizeof(*frame), .val = 0);
    frame->leaves = CLIMB_TEST_BLOBS[at];
    const unsigned int extents[3] = {CLIMB_TEST_DEPTH, CLIMB_TEST_HEIGHT, CLIMB_TEST_WIDTH};
    for (unsigned int blob = 0u; blob < frame->leaves; blob++)
    {
        for (unsigned int axis = 0u; axis < 3u; axis++)
        {
            // Explicit casts take a frame number and a draw of 3 values below 2^31, which an int holds
            const int jitter = (int)(climb_test_mixed(blob, at, axis, 7u) % 3u) - 1;
            const int place =
                blobs->center[blob][axis] + ((int)at * CLIMB_TEST_DRIFT[axis]) + ((at != 0u) ? jitter : 0);
            frame->center[blob][axis] = climb_test_clamped(place, extents[axis]);
        }
        // At the last place along the width, the step goes back to the first place
        int taken = 1;
        while (taken != 0)
        {
            taken = 0;
            for (unsigned int earlier = 0u; earlier < blob; earlier++)
            {
                taken |= (EMBED_CALL(memor.cmp, MemoriaCfg, .src = frame->center[earlier], .other = frame->center[blob],
                                     .bytes = sizeof(frame->center[blob])) == 0);
            }
            const int last = (frame->center[blob][2] >= ((int)CLIMB_TEST_WIDTH - 2));
            frame->center[blob][2] =
                (taken == 0) ? frame->center[blob][2] : ((last != 0) ? 1 : (frame->center[blob][2] + 1));
        }
        frame->peaks[blob] = climb_test_voxel(frame->center[blob]);
    }
    for (unsigned int voxel = 0u; voxel < CLIMB_TEST_VOXELS; voxel++)
    {
        // Explicit casts take each coordinate below the extent of the frame, which an int holds
        const int place[3] = {(int)(voxel / (CLIMB_TEST_HEIGHT * CLIMB_TEST_WIDTH)),
                              (int)((voxel / CLIMB_TEST_WIDTH) % CLIMB_TEST_HEIGHT), (int)(voxel % CLIMB_TEST_WIDTH)};
        unsigned int nearest = CLIMB_TEST_LEAVES_MAX;
        int nearest_length = 0x7FFFFFFF;
        for (unsigned int blob = 0u; blob < frame->leaves; blob++)
        {
            const int step[3] = {place[0] - frame->center[blob][0], place[1] - frame->center[blob][1],
                                 place[2] - frame->center[blob][2]};
            const int length = (step[0] * step[0]) + (step[1] * step[1]) + (step[2] * step[2]);
            const int inside = (length <= (blobs->radius[blob] * blobs->radius[blob])) && (length < nearest_length);
            nearest = (inside != 0) ? blob : nearest;
            nearest_length = (inside != 0) ? length : nearest_length;
        }
        // a hole in about 1 voxel of 7 of a blob, never at its peak. Each hole is the same in each frame of its blob
        const int hole = (nearest != CLIMB_TEST_LEAVES_MAX) && (nearest_length != 0) &&
                         ((climb_test_mixed(nearest, (unsigned int)(place[0] - frame->center[nearest][0] + 8),
                                            (unsigned int)(place[1] - frame->center[nearest][1] + 8),
                                            (unsigned int)(place[2] - frame->center[nearest][2] + 8)) %
                           7u) == 0u);
        const int labeled = (nearest != CLIMB_TEST_LEAVES_MAX) && (hole == 0);
        frame->labels[voxel] = (labeled != 0) ? frame->peaks[nearest] : 0u;
        frame->positive[voxel / 64u] |= (labeled != 0) ? (1ULL << (voxel % 64u)) : 0ULL;
    }
    unsigned int contact = 0u;
    for (unsigned int leaf = 0u; leaf < frame->leaves; leaf++)
    {
        frame->contact_start[leaf] = contact;
        for (unsigned int other = 0u; other < frame->leaves; other++)
        {
            const int step[3] = {frame->center[leaf][0] - frame->center[other][0],
                                 frame->center[leaf][1] - frame->center[other][1],
                                 frame->center[leaf][2] - frame->center[other][2]};
            const int reach = blobs->radius[leaf] + blobs->radius[other] + 1;
            const int touches =
                (other != leaf) && (((step[0] * step[0]) + (step[1] * step[1]) + (step[2] * step[2])) <= reach * reach);
            frame->contacts[contact] = other;
            contact += (touches != 0) ? 1u : 0u;
        }
    }
    frame->contact_start[frame->leaves] = contact;
}

// The answers of one path over both rounds, as bytes that the test signs. 1 where the bytes fit, and else 0
typedef struct
{
    unsigned char *bytes;
    unsigned long long count;
    int fits;
} ClimbTestAnswer;

static void climb_test_put(ClimbTestAnswer *answer, const void *bytes, unsigned long long count)
{
    answer->fits = answer->fits && ((answer->count + count) <= CLIMB_TEST_ANSWER_BYTES);
    if ((answer->fits != 0) && (count != 0ULL))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &answer->bytes[answer->count], .src = bytes, .bytes = (size_t)count);
        answer->count += count;
    }
}

// the first 8 bytes of the signum of `bytes` as a word, little end first, or 0 where the signum errors
static unsigned long long climb_test_signum(const ClimbTestAnswer *answer)
{
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const ObsignatioSignumRequest request = {answer->bytes, answer->count,           NULL,  OBSIGNATIO_MODE_HASH,
                                             signum,        OBSIGNATIO_SIGNUM_BYTES, &error};
    if ((answer->fits == 0) || (answer->count == 0ULL) || (obsignatio_signum(&request) != 0L))
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

// the answers of the climb of one pair, both sides
typedef struct
{
    int forward_lags[3u * CLIMB_TEST_LEAVES_MAX];
    int forward[CLIMB_TEST_LEAVES_MAX];
    int backward_lags[3u * CLIMB_TEST_LEAVES_MAX];
    int backward[CLIMB_TEST_LEAVES_MAX];
    unsigned int forward_final_score[CLIMB_TEST_LEAVES_MAX];
    unsigned int backward_final_score[CLIMB_TEST_LEAVES_MAX];
} ClimbTestClimb;

// The bounds of each leaf of a frame on the host, as the extents of the machine give them. The bounds are the least
// depth, height and width of its voxels, then the most. 1 where the extents of the machine equal them
static int climb_test_extents_agree(const ClimbTestFrame *frame, const unsigned int *extents)
{
    unsigned int bounds[CLIMB_TEST_LEAVES_MAX * CLIMB_MACHINE_EXTENT_FIELDS];
    for (unsigned int leaf = 0u; leaf < frame->leaves; leaf++)
    {
        for (unsigned int axis = 0u; axis < 3u; axis++)
        {
            bounds[(leaf * CLIMB_MACHINE_EXTENT_FIELDS) + axis] = 0xFFFFFFFFu;
            bounds[(leaf * CLIMB_MACHINE_EXTENT_FIELDS) + 3u + axis] = 0u;
        }
    }
    for (unsigned int voxel = 0u; voxel < CLIMB_TEST_VOXELS; voxel++)
    {
        unsigned int leaf = CLIMB_TEST_LEAVES_MAX;
        for (unsigned int one = 0u; (frame->labels[voxel] != 0u) && (one < frame->leaves); one++)
        {
            leaf = (frame->peaks[one] == frame->labels[voxel]) ? one : leaf;
        }
        if (leaf == CLIMB_TEST_LEAVES_MAX)
        {
            continue;
        }
        const unsigned int place[3] = {voxel / (CLIMB_TEST_HEIGHT * CLIMB_TEST_WIDTH),
                                       (voxel / CLIMB_TEST_WIDTH) % CLIMB_TEST_HEIGHT, voxel % CLIMB_TEST_WIDTH};
        for (unsigned int axis = 0u; axis < 3u; axis++)
        {
            unsigned int *const least = &bounds[(leaf * CLIMB_MACHINE_EXTENT_FIELDS) + axis];
            unsigned int *const most = &bounds[(leaf * CLIMB_MACHINE_EXTENT_FIELDS) + 3u + axis];
            *least = (place[axis] < *least) ? place[axis] : *least;
            *most = (place[axis] > *most) ? place[axis] : *most;
        }
    }
    return EMBED_CALL(memor.cmp, MemoriaCfg, .src = bounds, .other = extents,
                      .bytes = (size_t)frame->leaves * CLIMB_MACHINE_EXTENT_FIELDS * sizeof(unsigned int)) == 0;
}

// The labels and the positive bits that the machine holds for a frame, read back from the device. 1 where they equal
// the frame. The labels of a frame stay in the machine until two newer frames replace them
static int climb_test_held_agree(const ClimbMachine *machine, unsigned int at, const ClimbTestFrame *frame,
                                 int labels_held, unsigned int *labels, unsigned long long *positive)
{
    const unsigned int *const device_labels = climb_machine_labels(machine, at);
    const unsigned long long *const device_positive = climb_machine_positive(machine, at);
    int agree =
        (device_positive != NULL) &&
        (cudaMemcpy(positive, device_positive, sizeof(frame->positive), cudaMemcpyDeviceToHost) == cudaSuccess) &&
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = positive, .other = frame->positive,
                    .bytes = sizeof(frame->positive)) == 0);
    if (labels_held != 0)
    {
        agree = agree && (device_labels != NULL) &&
                (cudaMemcpy(labels, device_labels, sizeof(frame->labels), cudaMemcpyDeviceToHost) == cudaSuccess) &&
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = labels, .other = frame->labels,
                            .bytes = sizeof(frame->labels)) == 0);
    }
    return agree;
}

// The frames of one round climbed in three pairs: the first and the second, the second and the third, and the first
// and the third. Each climb starts one step from the drift. Each climber must thus move
static int climb_test_round(ClimbMachine *machine, const ClimbTestFrame *frames, const unsigned int *at,
                            ClimbTestClimb *climbs, ClimbTestAnswer *answers, unsigned int *landed)
{
    const unsigned int earlier[CLIMB_TEST_PAIRS] = {at[0], at[1], at[0]};
    const unsigned int later[CLIMB_TEST_PAIRS] = {at[1], at[2], at[2]};
    int ok = 1;
    for (unsigned int pair = 0u; pair < CLIMB_TEST_PAIRS; pair++)
    {
        // Explicit cast takes the frames apart, 1 or 2, as an int
        const int apart = (int)(later[pair] - earlier[pair]);
        ClimbMachinePair request;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &climbs[pair], .bytes = sizeof(climbs[pair]), .val = 0);
        request.earlier = earlier[pair];
        request.later = later[pair];
        request.lag[0] = (CLIMB_TEST_DRIFT[0] * apart) - 1;
        request.lag[1] = (CLIMB_TEST_DRIFT[1] * apart) + 1;
        request.lag[2] = (CLIMB_TEST_DRIFT[2] * apart) + 1;
        request.earlier_leaves = frames[earlier[pair]].leaves;
        request.earlier_peaks = frames[earlier[pair]].peaks;
        request.later_leaves = frames[later[pair]].leaves;
        request.later_peaks = frames[later[pair]].peaks;
        request.forward_lags = climbs[pair].forward_lags;
        request.forward = climbs[pair].forward;
        request.backward_lags = climbs[pair].backward_lags;
        request.backward = climbs[pair].backward;
        request.forward_final_score = climbs[pair].forward_final_score;
        request.backward_final_score = climbs[pair].backward_final_score;
        ok = ok && (climb_machine_pend(machine, &request) != 0);
    }
    ok = ok && (climb_machine_run(machine) != 0);
    for (unsigned int pair = 0u; (ok != 0) && (pair < CLIMB_TEST_PAIRS); pair++)
    {
        climb_test_put(&answers[0], &climbs[pair], sizeof(climbs[pair]));
        for (unsigned int leaf = 0u; leaf < frames[earlier[pair]].leaves; leaf++)
        {
            landed[0] += (climbs[pair].forward[leaf] == (int)leaf) ? 1u : 0u;
            landed[1] += 1u;
        }
    }
    ClimbMachineBox box;
    const int boxed = ok && (climb_machine_box(machine, &box) != 0);
    if (boxed != 0)
    {
        const unsigned int counts[8] = {box.climbers,    box.cells,   box.entries, box.final_score_differ,
                                        box.not_highest, box.crowded, box.broken,  0u};
        climb_test_put(&answers[1], counts, sizeof(counts));
        climb_test_put(&answers[1], box.earlier, (unsigned long long)box.climbers * sizeof(unsigned int));
        climb_test_put(&answers[1], box.later, (unsigned long long)box.climbers * sizeof(unsigned int));
        climb_test_put(&answers[1], box.leaf, (unsigned long long)box.climbers * sizeof(unsigned int));
        climb_test_put(&answers[1], box.cell_first, ((unsigned long long)box.climbers + 1ULL) * sizeof(unsigned int));
        climb_test_put(&answers[1], box.cell_shift, (unsigned long long)box.cells * 3ULL * sizeof(int));
        climb_test_put(&answers[1], box.entry_first, ((unsigned long long)box.cells + 1ULL) * sizeof(unsigned int));
        climb_test_put(&answers[1], box.target, (unsigned long long)box.entries * sizeof(unsigned int));
        climb_test_put(&answers[1], box.count, (unsigned long long)box.entries * sizeof(unsigned int));
    }
    ClimbMachineCore core;
    const int cored = boxed && (climb_machine_core(machine, &core) != 0);
    if (cored != 0)
    {
        const unsigned int counts[2] = {core.climbers, core.widths};
        const unsigned long long climbers = (unsigned long long)core.climbers;
        climb_test_put(&answers[2], counts, sizeof(counts));
        climb_test_put(&answers[2], core.earlier, climbers * sizeof(unsigned int));
        climb_test_put(&answers[2], core.later, climbers * sizeof(unsigned int));
        climb_test_put(&answers[2], core.side, climbers * sizeof(unsigned int));
        climb_test_put(&answers[2], core.leaf, climbers * sizeof(unsigned int));
        climb_test_put(&answers[2], core.match, climbers * sizeof(unsigned int));
        climb_test_put(&answers[2], core.lag, climbers * 3ULL * sizeof(int));
        climb_test_put(&answers[2], core.width_first, (climbers + 1ULL) * sizeof(unsigned int));
        climb_test_put(&answers[2], core.fields,
                       (unsigned long long)core.widths * CLIMB_MACHINE_CORE_FIELDS * sizeof(unsigned long long));
    }
    int measured = cored;
    for (unsigned int frame = 0u; (measured != 0) && (frame < 3u); frame++)
    {
        unsigned int extents[CLIMB_TEST_LEAVES_MAX * CLIMB_MACHINE_EXTENT_FIELDS];
        const ClimbMachineExtentsRequest request = {at[frame], extents};
        measured =
            (climb_machine_extents(machine, &request) != 0) && climb_test_extents_agree(&frames[at[frame]], extents);
        climb_test_put(&answers[3], extents,
                       (unsigned long long)frames[at[frame]].leaves * CLIMB_MACHINE_EXTENT_FIELDS *
                           sizeof(unsigned int));
    }
    return measured;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the first word of the signum of each path in each mode, the answer of the device path
    static const unsigned long long SIGNA[CLIMB_TEST_MODES][CLIMB_TEST_PATHS] = {
        {0x3843cf5e1ef9a4d4ULL, 0x358db8721289ef60ULL, 0x0d6fd54259757dd5ULL, 0x151729b98ec84eccULL},
        {0x295536786794f0e5ULL, 0x358db8721289ef60ULL, 0xa269763e34da9c4fULL, 0x151729b98ec84eccULL}};
    static const char *const MODES[CLIMB_TEST_MODES] = {"at the peak", "by mass"};
    static const char *const NAMES[CLIMB_TEST_PATHS] = {"the climb", "the box", "the core", "the extents"};
    const unsigned int place = platform_place_g_program_owner;
    ClimbTestFrame *const frames =
        (ClimbTestFrame *)platform_cell_taken(CLIMB_TEST_FRAMES * sizeof(ClimbTestFrame), place, 1);
    ClimbTestBlobs *const blobs = (ClimbTestBlobs *)platform_cell_taken(sizeof(ClimbTestBlobs), place, 1);
    ClimbTestClimb *const climbs =
        (ClimbTestClimb *)platform_cell_taken(CLIMB_TEST_PAIRS * sizeof(ClimbTestClimb), place, 1);
    unsigned int *const labels =
        (unsigned int *)platform_cell_taken(CLIMB_TEST_VOXELS * sizeof(unsigned int), place, 1);
    unsigned long long *const positive =
        (unsigned long long *)platform_cell_taken(CLIMB_TEST_WORDS * sizeof(unsigned long long), place, 1);
    unsigned char *const bytes =
        (unsigned char *)platform_cell_taken(CLIMB_TEST_PATHS * CLIMB_TEST_ANSWER_BYTES, place, 1);
    if ((frames == NULL) || (blobs == NULL) || (climbs == NULL) || (labels == NULL) || (positive == NULL) ||
        (bytes == NULL))
    {
        return 1;
    }
    climb_test_blobs_drawn(blobs);
    for (unsigned int at = 0u; at < CLIMB_TEST_FRAMES; at++)
    {
        climb_test_frame_drawn(blobs, at, &frames[at]);
    }
    // the frames that each round climbs. The second round keeps the last frame of the first
    static const unsigned int ROUNDS[CLIMB_TEST_ROUNDS][3] = {{0u, 1u, 2u}, {2u, 3u, 4u}};
    unsigned int failed = 0u;
    for (unsigned int mode = 0u; mode < CLIMB_TEST_MODES; mode++)
    {
        ClimbTestAnswer answers[CLIMB_TEST_PATHS];
        for (unsigned int path = 0u; path < CLIMB_TEST_PATHS; path++)
        {
            answers[path].bytes = &bytes[path * CLIMB_TEST_ANSWER_BYTES];
            answers[path].count = 0ULL;
            answers[path].fits = 1;
        }
        const ClimbMachineExtent extent = {
            CLIMB_TEST_DEPTH, CLIMB_TEST_HEIGHT, CLIMB_TEST_WIDTH, CLIMB_TEST_LEAVES_MAX, CLIMB_TEST_HELD, 1u, 0u};
        ClimbMachine *const machine = climb_machine_open(&extent);
        int ok = (machine != NULL);
        if (ok != 0)
        {
            climb_machine_land_by_mass(machine, mode);
            ok = climb_machine_spiral(machine, (mode != 0u) ? 6u : 0u) != 0;
        }
        failed += climb_test_check(ok, (mode == 0u) ? "the machine opens to land at the peak"
                                                    : "the machine opens to land by mass along a spiral");
        unsigned int landed[2] = {0u, 0u};
        int held = ok;
        int rounds = ok;
        for (unsigned int round = 0u; (rounds != 0) && (round < CLIMB_TEST_ROUNDS); round++)
        {
            for (unsigned int frame = (round == 0u) ? 0u : 1u; (rounds != 0) && (frame < 3u); frame++)
            {
                const unsigned int at = ROUNDS[round][frame];
                const ClimbMachineFrame stored = {at,
                                                  frames[at].leaves,
                                                  frames[at].labels,
                                                  frames[at].positive,
                                                  frames[at].peaks,
                                                  frames[at].contact_start,
                                                  frames[at].contacts};
                rounds = climb_machine_store(machine, &stored) != 0;
            }
            for (unsigned int frame = 0u; (rounds != 0) && (frame < 3u); frame++)
            {
                held = held && climb_test_held_agree(machine, ROUNDS[round][frame], &frames[ROUNDS[round][frame]],
                                                     frame != 0u, labels, positive);
            }
            rounds = rounds && climb_test_round(machine, frames, ROUNDS[round], climbs, answers, landed);
        }
        failed += climb_test_check(rounds, "each round climbs, boxes, cores and measures its frames");
        failed += climb_test_check(held, "the machine holds the labels and the positive bits of its frames");
        failed +=
            climb_test_check((4u * landed[0]) >= (3u * landed[1]), "most climbers land on the leaf of their own blob");
        climb_machine_close(machine);
        for (unsigned int path = 0u; path < CLIMB_TEST_PATHS; path++)
        {
            const unsigned long long word = rounds ? climb_test_signum(&answers[path]) : 0ULL;
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, NAMES[path]);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, MODES[mode]);
            mmgr_line_put(output_line, ": signum 0x");
            mmgr_line_hex(output_line, word, 16u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
            failed += climb_test_check((word != 0ULL) && (word == SIGNA[mode][path]),
                                       "the answers of the path are the answer of the device path");
        }
    }
    platform_cell_released(bytes, place);
    platform_cell_released(positive, place);
    platform_cell_released(labels, place);
    platform_cell_released(climbs, place);
    platform_cell_released(blobs, place);
    platform_cell_released(frames, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  climb machine paths test: ");
    mmgr_line_decimal(output_line, s_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
