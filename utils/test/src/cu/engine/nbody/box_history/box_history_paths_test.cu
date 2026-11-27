// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// box_history_paths_test.cu: the path of the box history on the device, with the engine linked into the program. The
// test draws a seeded history of windows and seeded boxes of bodies, one of them empty, and gathers the counts of each
// box. The counts must equal the counts that a walk of each box on the host gives, the device must find no
// disagreement, and the signum of the counts must equal the signum of the device path
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/nbody/box_history/box_history.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../includes/platform/platform.h"

#include "memoria_operor/memoria_operor.h"

// the extent of the history, its voxels, its windows and the bodies whose boxes the test gathers
#define BOX_TEST_DEPTH 10u
#define BOX_TEST_HEIGHT 20u
#define BOX_TEST_WIDTH 24u
#define BOX_TEST_VOXELS (BOX_TEST_DEPTH * BOX_TEST_HEIGHT * BOX_TEST_WIDTH)
#define BOX_TEST_WINDOWS 4u
#define BOX_TEST_BODIES 40u
#define BOX_TEST_COUNTS (BOX_TEST_BODIES * BOX_TEST_WINDOWS * ENGINE_HISTORY_BITS)

// the pool of the test, for its history and its counts
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 26u;

// the seeded words that the history and the boxes draw from
static unsigned int s_seed = 0xBB67AE85u;

static unsigned int box_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// the count of the checks, and one check, printed as it passes or fails
static unsigned int s_checks = 0u;

static unsigned int box_test_check(int held, const char *what)
{
    s_checks += 1u;
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// the history, the boxes, and the counts of the device and of the host
typedef struct
{
    unsigned long long history[BOX_TEST_WINDOWS * BOX_TEST_VOXELS];
    unsigned int extents[BOX_TEST_BODIES * BOX_HISTORY_EXTENT_FIELDS];
    unsigned int counts[BOX_TEST_COUNTS];
    unsigned int walked[BOX_TEST_COUNTS];
} BoxTestCase;

// a low and a high place along an axis of `extent` places, the low at most the high
static void box_test_span(unsigned int extent, unsigned int *low, unsigned int *high)
{
    const unsigned int one = box_test_drawn() % extent;
    const unsigned int other = box_test_drawn() % extent;
    *low = (one < other) ? one : other;
    *high = (one < other) ? other : one;
}

// Each voxel of each window holds a count of 0 to ENGINE_HISTORY_WINDOW in each nibble. Each box is a seeded span on
// each axis. The last box is empty: its least depth passes its most depth
static void box_test_drawn_case(BoxTestCase *one)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = one, .bytes = sizeof(*one), .val = 0);
    for (unsigned int word = 0u; word < (BOX_TEST_WINDOWS * BOX_TEST_VOXELS); word++)
    {
        for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit++)
        {
            const unsigned long long count = box_test_drawn() % (ENGINE_HISTORY_WINDOW + 1u);
            one->history[word] |= count << (4u * bit);
        }
    }
    const unsigned int extents[3] = {BOX_TEST_DEPTH, BOX_TEST_HEIGHT, BOX_TEST_WIDTH};
    for (unsigned int body = 0u; body < BOX_TEST_BODIES; body++)
    {
        unsigned int *const extent = &one->extents[body * BOX_HISTORY_EXTENT_FIELDS];
        for (unsigned int axis = 0u; axis < 3u; axis++)
        {
            box_test_span(extents[axis], &extent[axis], &extent[3u + axis]);
        }
    }
    unsigned int *const last = &one->extents[(BOX_TEST_BODIES - 1u) * BOX_HISTORY_EXTENT_FIELDS];
    last[0] = 6u;
    last[3] = 2u;
}

// the counts of each box walked on the host, voxel by voxel
static void box_test_walked(BoxTestCase *one)
{
    for (unsigned int body = 0u; body < BOX_TEST_BODIES; body++)
    {
        const unsigned int *const extent = &one->extents[body * BOX_HISTORY_EXTENT_FIELDS];
        for (unsigned int window = 0u; window < BOX_TEST_WINDOWS; window++)
        {
            unsigned int *const walked = &one->walked[((body * BOX_TEST_WINDOWS) + window) * ENGINE_HISTORY_BITS];
            for (unsigned int depth = extent[0]; depth <= extent[3]; depth++)
            {
                for (unsigned int height = extent[1]; height <= extent[4]; height++)
                {
                    for (unsigned int width = extent[2]; width <= extent[5]; width++)
                    {
                        const unsigned long long word =
                            one->history[(window * BOX_TEST_VOXELS) +
                                         (((depth * BOX_TEST_HEIGHT) + height) * BOX_TEST_WIDTH) + width];
                        for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit++)
                        {
                            // Explicit cast takes a nibble, below 16, as an unsigned int
                            walked[bit] += (unsigned int)((word >> (4u * bit)) & 0xFULL);
                        }
                    }
                }
            }
        }
    }
}

// the first 8 bytes of the signum of `count` bytes as a word, little end first, or 0 where the signum errors
static unsigned long long box_test_signum(const void *bytes, unsigned long long count)
{
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    // Explicit cast reads the counts as the bytes that the signum reads
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
    // the first word of the signum of the counts of the device, the answer of the device path
    static const unsigned long long SIGNUM = 0x9e04cc4926c4ba25ULL;
    const unsigned int place = platform_place_g_program_owner;
    BoxTestCase *const one = (BoxTestCase *)platform_cell_taken(sizeof(BoxTestCase), place, 1);
    if (one == NULL)
    {
        return 1;
    }
    box_test_drawn_case(one);
    box_test_walked(one);
    EngineHistory history;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &history, .bytes = sizeof(history), .val = 0);
    history.extent[0] = 1ULL;
    history.extent[1] = BOX_TEST_DEPTH;
    history.extent[2] = BOX_TEST_HEIGHT;
    history.extent[3] = BOX_TEST_WIDTH;
    history.windows = BOX_TEST_WINDOWS;
    history.history = one->history;
    unsigned long long disagreements = 1ULL;
    unsigned long long microseconds = 0ULL;
    const BoxHistoryRequest request = {&history,    BOX_TEST_BODIES, one->extents,
                                       one->counts, &disagreements,  &microseconds};
    unsigned int failed = box_test_check(box_history_gather(&request) == 0L, "the box history gathers each box");
    failed += box_test_check(disagreements == 0ULL, "the device finds no disagreement with its own walk");
    failed += box_test_check(
        EMBED_CALL(memor.cmp, MemoriaCfg, .src = one->counts, .other = one->walked, .bytes = sizeof(one->counts)) == 0,
        "the counts of the device are the counts of the walk on the host");
    const unsigned int *const empty = &one->counts[(BOX_TEST_BODIES - 1u) * BOX_TEST_WINDOWS * ENGINE_HISTORY_BITS];
    unsigned int empty_total = 0u;
    for (unsigned int entry = 0u; entry < (BOX_TEST_WINDOWS * ENGINE_HISTORY_BITS); entry++)
    {
        empty_total += empty[entry];
    }
    failed += box_test_check(empty_total == 0u, "the empty box counts 0");
    const unsigned long long word = box_test_signum(one->counts, sizeof(one->counts));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the counts: signum 0x");
        mmgr_line_hex(output_line, word, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    failed += box_test_check((word != 0ULL) && (word == SIGNUM),
                             "the counts of the device are the answer of the device path");
    platform_cell_released(one, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  box history paths test: ");
    mmgr_line_decimal(output_line, s_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
