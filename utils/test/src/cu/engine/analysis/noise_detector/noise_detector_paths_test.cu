// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// noise_detector_paths_test.cu: the paths of the noise detector on the device, with the engine linked into the program.
// The test draws two seeded volumes of frames. Each has a fixed pattern, an offset on each row, noise that changes from
// frame to frame, pixels stuck at 0, at the top and at one level, clips and spikes. It reads each volume through the
// flicker, the lines, the neighbors, the clips, the crosstalk, the structure and the halves, and reads the two volumes
// as a bias and a dark through the charge. The signum of the answers of each path must equal the signum of its case,
// the answer that the device path gives
#include "../../../../../../../src/cu/engine/analysis/noise_detector/noise_detector.h"
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../includes/platform/platform.h"

#include "memoria_operor/memoria_operor.h"

// the extent of each volume: its frames, depth, height and width, and its values
#define NOISE_TEST_FRAMES 24ULL
#define NOISE_TEST_DEPTH 8ULL
#define NOISE_TEST_HEIGHT 32ULL
#define NOISE_TEST_WIDTH 32ULL
#define NOISE_TEST_VOXELS (NOISE_TEST_DEPTH * NOISE_TEST_HEIGHT * NOISE_TEST_WIDTH)
#define NOISE_TEST_VALUES (NOISE_TEST_FRAMES * NOISE_TEST_VOXELS)

// the volumes of the test, and the paths whose answers it signs
#define NOISE_TEST_VOLUMES 2u
#define NOISE_TEST_PATHS 8u

// the most bytes of the answers of one path, over both volumes
#define NOISE_TEST_ANSWER_BYTES 65536u

// the pool of the test, for its volumes and its answers
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 28u;

// the seeded words that the volumes draw from
static unsigned int s_seed = 0x5BD1E995u;

static unsigned int noise_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// the count of the checks, and one check, printed as it passes or fails
static unsigned int s_checks = 0u;

static unsigned int noise_test_check(int held, const char *what)
{
    s_checks += 1u;
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// One volume, frame after frame. Each voxel holds a level of `base` plus its fixed pattern and the offset of its row.
// Noise of `spread` adds to it each frame, and so does a drift that each voxel of a frame shares. Pixel 0 stays at 0,
// pixel 1 at the top and pixel 2 at one level. About 1 value in 3000 clips to 0, and about 1 in 3000 is a spike.
//
// No value other than pixel 1 clips to the top. A top clip beside a voxel would swamp the squares of each neighbor
static void noise_test_volume(unsigned int base, unsigned int spread, unsigned short *volume)
{
    static unsigned int pattern[NOISE_TEST_VOXELS];
    static unsigned int rows[NOISE_TEST_DEPTH * NOISE_TEST_HEIGHT];
    for (unsigned long long voxel = 0ULL; voxel < NOISE_TEST_VOXELS; voxel++)
    {
        pattern[voxel] = noise_test_drawn() % 40u;
    }
    for (unsigned long long row = 0ULL; row < (NOISE_TEST_DEPTH * NOISE_TEST_HEIGHT); row++)
    {
        rows[row] = noise_test_drawn() % 24u;
    }
    for (unsigned long long frame = 0ULL; frame < NOISE_TEST_FRAMES; frame++)
    {
        const unsigned int drift = noise_test_drawn() % 16u;
        for (unsigned long long voxel = 0ULL; voxel < NOISE_TEST_VOXELS; voxel++)
        {
            const unsigned int noise = (noise_test_drawn() % spread) + (noise_test_drawn() % spread);
            unsigned int value = base + pattern[voxel] + rows[voxel / NOISE_TEST_WIDTH] + drift + noise;
            const unsigned int draw = noise_test_drawn() % 3000u;
            value = (draw == 0u) ? 0u : value;
            value = (draw == 1u) ? (value + 200u + (noise_test_drawn() % 100u)) : value;
            value = (voxel == 0ULL) ? 0u : ((voxel == 1ULL) ? 65535u : ((voxel == 2ULL) ? 1000u : value));
            // Explicit cast takes a value at most 65535, which an unsigned short holds
            volume[(frame * NOISE_TEST_VOXELS) + voxel] = (unsigned short)((value > 65535u) ? 65535u : value);
        }
    }
}

// The answers of one path over both volumes, as bytes that the test signs. Each path puts its return and its answers
typedef struct
{
    unsigned char bytes[NOISE_TEST_ANSWER_BYTES];
    unsigned long long count;
    int fits;
    int read;
    int marked;
} NoiseTestAnswer;

static void noise_test_put(NoiseTestAnswer *answer, long returned, const void *bytes, unsigned long long count)
{
    answer->read = answer->read && (returned == 0L);
    // Explicit cast reads the answers as the bytes that they hold
    const unsigned char *const answer_bytes = (const unsigned char *)bytes;
    for (unsigned long long at = 0ULL; at < count; at++)
    {
        answer->marked |= (answer_bytes[at] != 0u);
    }
    answer->fits = answer->fits && ((answer->count + sizeof(returned) + count) <= NOISE_TEST_ANSWER_BYTES);
    if (answer->fits != 0)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &answer->bytes[answer->count], .src = &returned,
                   .bytes = sizeof(returned));
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &answer->bytes[answer->count + sizeof(returned)], .src = bytes,
                   .bytes = (size_t)count);
        answer->count += sizeof(returned) + count;
    }
}

// the first 8 bytes of the signum of the answers as a word, little end first, or 0 where the signum errors
static unsigned long long noise_test_signum(const NoiseTestAnswer *answer)
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

// the answers of each path for one volume, with every byte 0 before the path writes them
typedef struct
{
    unsigned long long flicker[2u * NOISE_FLICKER_LAGS];
    long long lines[NOISE_PLANE_READINGS];
    long long neighbors[NOISE_NEIGHBOR_RADII];
    unsigned long long clips_counts[NOISE_CLIPS_COUNTS];
    unsigned int clips_box[NOISE_CLIPS_BOX];
    unsigned long long clips_spikes[NOISE_SPIKE_SUMS];
    NoiseCrosstalkMeasurement crosstalk;
    NoiseChargeMeasurement charge;
    NoiseStructureMeasurement structure;
    NoiseHalvesMeasurement halves;
} NoiseTestReadings;

// Each path of one volume, its answers put into `answers`. The charge reads `volume` as the bias and `other` as the
// dark
static void noise_test_paths(const unsigned short *volume, const unsigned short *other, NoiseTestReadings *readings,
                             NoiseTestAnswer *answers)
{
    static const unsigned long long EXTENT[4] = {NOISE_TEST_FRAMES, NOISE_TEST_DEPTH, NOISE_TEST_HEIGHT,
                                                 NOISE_TEST_WIDTH};
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = readings, .bytes = sizeof(*readings), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    long returned =
        noise_flicker_volume(volume, EXTENT, readings->flicker, &readings->flicker[NOISE_FLICKER_LAGS], &error);
    noise_test_put(&answers[0], returned, readings->flicker, sizeof(readings->flicker));
    returned = noise_lines_volume(volume, EXTENT, readings->lines, &error);
    noise_test_put(&answers[1], returned, readings->lines, sizeof(readings->lines));
    returned = noise_neighbors_volume(volume, EXTENT, readings->neighbors, &error);
    noise_test_put(&answers[2], returned, readings->neighbors, sizeof(readings->neighbors));
    returned =
        noise_clips_volume(volume, EXTENT, readings->clips_counts, readings->clips_box, readings->clips_spikes, &error);
    noise_test_put(&answers[3], returned, readings->clips_counts,
                   sizeof(readings->clips_counts) + sizeof(readings->clips_box) + sizeof(readings->clips_spikes));
    returned = noise_crosstalk_volume(volume, EXTENT, &readings->crosstalk, &error);
    noise_test_put(&answers[4], returned, &readings->crosstalk, sizeof(readings->crosstalk));
    returned = noise_charge_series(volume, other, EXTENT, &readings->charge, &error);
    noise_test_put(&answers[5], returned, &readings->charge, sizeof(readings->charge));
    returned = noise_structure_volume(volume, EXTENT, &readings->structure, &error);
    noise_test_put(&answers[6], returned, &readings->structure, sizeof(readings->structure));
    returned = noise_halves_volume(volume, EXTENT, &readings->halves, &error);
    noise_test_put(&answers[7], returned, &readings->halves, sizeof(readings->halves));
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the first word of the signum of each path, the answer of the device path
    static const unsigned long long SIGNA[NOISE_TEST_PATHS] = {
        0x930471a126933cf2ULL, 0x14a726cae7a22c3eULL, 0xae265deae617bb21ULL, 0xcc315527951d0f38ULL,
        0x37cd1757d7bf446aULL, 0xd2c63e74c968dbdfULL, 0xe4342e7bccd85ef8ULL, 0xcd35bc67a1bfc8a3ULL};
    static const char *const NAMES[NOISE_TEST_PATHS] = {"the flicker",   "the lines",  "the neighbors", "the clips",
                                                        "the crosstalk", "the charge", "the structure", "the halves"};
    const unsigned int place = platform_place_g_program_owner;
    unsigned short *const volumes = (unsigned short *)platform_cell_taken(
        NOISE_TEST_VOLUMES * NOISE_TEST_VALUES * sizeof(unsigned short), place, 1);
    NoiseTestAnswer *const answers =
        (NoiseTestAnswer *)platform_cell_taken(NOISE_TEST_PATHS * sizeof(NoiseTestAnswer), place, 1);
    NoiseTestReadings *const readings = (NoiseTestReadings *)platform_cell_taken(sizeof(NoiseTestReadings), place, 1);
    if ((volumes == NULL) || (answers == NULL) || (readings == NULL))
    {
        return 1;
    }
    for (unsigned int path = 0u; path < NOISE_TEST_PATHS; path++)
    {
        answers[path].count = 0ULL;
        answers[path].fits = 1;
        answers[path].read = 1;
        answers[path].marked = 0;
    }
    noise_test_volume(60u, 10u, volumes);
    noise_test_volume(70u, 14u, &volumes[NOISE_TEST_VALUES]);
    noise_test_paths(volumes, &volumes[NOISE_TEST_VALUES], readings, answers);
    noise_test_paths(&volumes[NOISE_TEST_VALUES], volumes, readings, answers);
    unsigned int failed = 0u;
    for (unsigned int path = 0u; path < NOISE_TEST_PATHS; path++)
    {
        const unsigned long long word = noise_test_signum(&answers[path]);
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, NAMES[path]);
        mmgr_line_put(output_line, ": signum 0x");
        mmgr_line_hex(output_line, word, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        failed += noise_test_check(answers[path].read, "the path reads both volumes");
        failed += noise_test_check(answers[path].marked, "the path writes an answer that is not 0");
        failed += noise_test_check((word != 0ULL) && (word == SIGNA[path]),
                                   "the answers of the path are the answer of the device path");
    }
    platform_cell_released(readings, place);
    platform_cell_released(answers, place);
    platform_cell_released(volumes, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  noise detector paths test: ");
    mmgr_line_decimal(output_line, s_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
