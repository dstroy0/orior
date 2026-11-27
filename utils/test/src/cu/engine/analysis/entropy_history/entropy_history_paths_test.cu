// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// entropy_history_paths_test.cu: the path of the entropy history on the device, with the engine linked into the
// program. The test draws a seeded volume and projects its history. The history and the cloud must equal the history
// and the cloud that the host counts flip by flip. The signum of both must equal the signum of the device path
#include "../../../../../../../src/cu/engine/analysis/entropy_history/entropy_history.h"
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../includes/platform/platform.h"

#include "memoria_operor/memoria_operor.h"

// the extent of the volume: its frames, depth, height and width, and its voxels and values
#define ENTROPY_TEST_FRAMES 40ULL
#define ENTROPY_TEST_DEPTH 6ULL
#define ENTROPY_TEST_HEIGHT 16ULL
#define ENTROPY_TEST_WIDTH 20ULL
#define ENTROPY_TEST_VOXELS (ENTROPY_TEST_DEPTH * ENTROPY_TEST_HEIGHT * ENTROPY_TEST_WIDTH)
#define ENTROPY_TEST_VALUES (ENTROPY_TEST_FRAMES * ENTROPY_TEST_VOXELS)

// the windows of the volume: each holds ENGINE_HISTORY_WINDOW transitions, and the last holds the rest
#define ENTROPY_TEST_WINDOWS (((ENTROPY_TEST_FRAMES - 1ULL) + ENGINE_HISTORY_WINDOW - 1ULL) / ENGINE_HISTORY_WINDOW)

// the pool of the test, for its volume and its history
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 26u;

// the seeded words that the volume draws from
static unsigned int s_seed = 0x3C6EF372u;

static unsigned int entropy_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// the count of the checks, and one check, printed as it passes or fails
static unsigned int s_checks = 0u;

static unsigned int entropy_test_check(int held, const char *what)
{
    s_checks += 1u;
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// the volume, and the history and the cloud of the device and of the host
typedef struct
{
    unsigned short volume[ENTROPY_TEST_VALUES];
    unsigned long long history[2u][ENTROPY_TEST_WINDOWS * ENTROPY_TEST_VOXELS];
    unsigned long long cloud[2u][ENTROPY_TEST_WINDOWS * ENTROPY_TEST_WINDOWS];
} EntropyTestCase;

// Each voxel starts at a seeded level. Each frame flips a seeded set of its low bits, more of them in some voxels than
// in others. Now and then a frame also flips one of its high bits
static void entropy_test_drawn_volume(EntropyTestCase *one)
{
    for (unsigned long long voxel = 0ULL; voxel < ENTROPY_TEST_VOXELS; voxel++)
    {
        // Explicit cast takes a draw below 2^16 as an unsigned short
        unsigned short value = (unsigned short)(entropy_test_drawn() & 0xFFFFu);
        const unsigned int low_bits = 1u + (entropy_test_drawn() % 6u);
        for (unsigned long long frame = 0ULL; frame < ENTROPY_TEST_FRAMES; frame++)
        {
            one->volume[(frame * ENTROPY_TEST_VOXELS) + voxel] = value;
            const unsigned int flips = entropy_test_drawn() & ((1u << low_bits) - 1u);
            const unsigned int high =
                ((entropy_test_drawn() % 8u) == 0u) ? (1u << (8u + (entropy_test_drawn() % 8u))) : 0u;
            // Explicit cast takes the flips, below 2^16, as an unsigned short
            value = (unsigned short)(value ^ (unsigned short)(flips | high));
        }
    }
}

// The history and the cloud on the host. Each window counts the flips of each bit over its transitions, a nibble a
// bit. The density of a window is the sum of its counts, each shifted by its bit. The cloud sums the products of the
// densities of each pair of windows, and holds each pair twice
static void entropy_test_counted(EntropyTestCase *one)
{
    unsigned long long *const history = one->history[1];
    unsigned long long *const cloud = one->cloud[1];
    for (unsigned long long voxel = 0ULL; voxel < ENTROPY_TEST_VOXELS; voxel++)
    {
        unsigned long long density[ENTROPY_TEST_WINDOWS];
        for (unsigned long long window = 0ULL; window < ENTROPY_TEST_WINDOWS; window++)
        {
            const unsigned long long first = (window * ENGINE_HISTORY_WINDOW) + 1ULL;
            const unsigned long long end = ((first + ENGINE_HISTORY_WINDOW) < ENTROPY_TEST_FRAMES)
                                               ? (first + ENGINE_HISTORY_WINDOW)
                                               : ENTROPY_TEST_FRAMES;
            unsigned long long packed = 0ULL;
            density[window] = 0ULL;
            for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit++)
            {
                unsigned long long count = 0ULL;
                for (unsigned long long frame = first; frame < end; frame++)
                {
                    const unsigned int flipped =
                        (unsigned int)one->volume[(frame * ENTROPY_TEST_VOXELS) + voxel] ^
                        (unsigned int)one->volume[((frame - 1ULL) * ENTROPY_TEST_VOXELS) + voxel];
                    count += (flipped >> bit) & 1u;
                }
                packed |= count << (4u * bit);
                density[window] += count << bit;
            }
            history[(window * ENTROPY_TEST_VOXELS) + voxel] = packed;
        }
        for (unsigned long long left = 0ULL; left < ENTROPY_TEST_WINDOWS; left++)
        {
            for (unsigned long long right = left; right < ENTROPY_TEST_WINDOWS; right++)
            {
                cloud[(left * ENTROPY_TEST_WINDOWS) + right] += density[left] * density[right];
            }
        }
    }
    for (unsigned long long left = 0ULL; left < ENTROPY_TEST_WINDOWS; left++)
    {
        for (unsigned long long right = 0ULL; right < left; right++)
        {
            cloud[(left * ENTROPY_TEST_WINDOWS) + right] = cloud[(right * ENTROPY_TEST_WINDOWS) + left];
        }
    }
}

// the first 8 bytes of the signum of `count` bytes as a word, little end first, or 0 where the signum errors
static unsigned long long entropy_test_signum(const void *bytes, unsigned long long count)
{
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    // Explicit cast reads the history as the bytes that the signum reads
    const ObsignatioSignumRequest request = {(const unsigned char *)bytes, count, NULL, OBSIGNATIO_MODE_HASH, signum,
                                             OBSIGNATIO_SIGNUM_BYTES,      &error};
    if ((count == 0ULL) || (obsignatio_signum(&request) != 0L))
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

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the first word of the signum of the history and the cloud of the device, the answer of the device path
    static const unsigned long long SIGNUM = 0x7f631766f71e8cf0ULL;
    const unsigned int place = platform_place_g_program_owner;
    EntropyTestCase *const one = (EntropyTestCase *)platform_cell_taken(sizeof(EntropyTestCase), place, 1);
    if (one == NULL)
    {
        return 1;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = one, .bytes = sizeof(*one), .val = 0);
    entropy_test_drawn_volume(one);
    entropy_test_counted(one);
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    EngineHistory history;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &history, .bytes = sizeof(history), .val = 0);
    history.history = one->history[0];
    history.cloud = one->cloud[0];
    EntropyHistoryProjectRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    request.volume = one->volume;
    request.extent[0] = ENTROPY_TEST_FRAMES;
    request.extent[1] = ENTROPY_TEST_DEPTH;
    request.extent[2] = ENTROPY_TEST_HEIGHT;
    request.extent[3] = ENTROPY_TEST_WIDTH;
    request.history = &history;
    request.error = &error;
    unsigned long long broken = 1ULL;
    unsigned int failed =
        entropy_test_check(entropy_history_project(&request, &broken) == 0L, "the entropy history projects the volume");
    failed += entropy_test_check((broken == 0ULL) && (history.windows == ENTROPY_TEST_WINDOWS),
                                 "the parity of each voxel holds over its windows");
    failed += entropy_test_check(EMBED_CALL(memor.cmp, MemoriaCfg, .src = one->history[0], .other = one->history[1],
                                            .bytes = sizeof(one->history[0])) == 0,
                                 "the history of the device is the history of the host");
    failed += entropy_test_check(EMBED_CALL(memor.cmp, MemoriaCfg, .src = one->cloud[0], .other = one->cloud[1],
                                            .bytes = sizeof(one->cloud[0])) == 0,
                                 "the cloud of the device is the cloud of the host");
    const unsigned long long word = entropy_test_signum(one->history[0], sizeof(one->history) + sizeof(one->cloud));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the history and the cloud: signum 0x");
        mmgr_line_hex(output_line, word, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    failed += entropy_test_check((word != 0ULL) && (word == SIGNUM),
                                 "the history and the cloud are the answer of the device path");
    platform_cell_released(one, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  entropy history paths test: ");
    mmgr_line_decimal(output_line, s_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
