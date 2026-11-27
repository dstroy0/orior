// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// flatten_paths_test.cu: the flatten of a set on the device, with the engine linked into the program. The test writes
// two seeded samples as .npy sources, each frame with more blobs than the frame before it, and ingests them as a set.
// The flatten packs the bodies of each frame on the device, and the read gives them back. The count of the bodies and
// the signum of their magnitudes must equal those of the device path
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/nbody/flatten/flatten.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../src/platform/platform.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// the samples, and the frames, depth, height and width of each
#define FLATTEN_TEST_SAMPLES 2u
static const unsigned long long FLATTEN_TEST_EXTENTS[FLATTEN_TEST_SAMPLES][4] = {{3ULL, 6ULL, 24ULL, 32ULL},
                                                                                 {2ULL, 6ULL, 20ULL, 28ULL}};

// the most values of a sample, and the room of a path
#define FLATTEN_TEST_VALUES_MAX (3ULL * 6ULL * 24ULL * 32ULL)
#define FLATTEN_TEST_PATH 512u

// the pool of the test, for its samples and the flatten
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 28u;

// the seeded words that the samples draw from
static unsigned int s_seed = 0x510E527Fu;

static unsigned int flatten_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// the count of the checks, and one check, printed as it passes or fails
static unsigned int s_checks = 0u;

static unsigned int flatten_test_check(int held, const char *what)
{
    s_checks += 1u;
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// `top`, then `name`, into `out`: 1, or 0 where the path passes the room
static int flatten_test_path(char *out, const char *top, const char *name)
{
    const size_t top_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = top, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t name_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if ((top_length + name_length + 1u) > FLATTEN_TEST_PATH)
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = out, .src = top, .bytes = top_length);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = out + top_length, .src = name, .bytes = name_length + 1u);
    return 1;
}

// One sample, frame after frame. A background near 200 holds blobs near 1200. Frame f holds 4 + 4 f blobs, each of
// radius 2 or 3. Each frame thus has more bodies than the frame before it
static void flatten_test_sample(const unsigned long long *extent, unsigned short *lanes)
{
    const unsigned long long depth = extent[1];
    const unsigned long long height = extent[2];
    const unsigned long long width = extent[3];
    const unsigned long long voxels = depth * height * width;
    for (unsigned long long frame = 0ULL; frame < extent[0]; frame++)
    {
        unsigned short *const plane = &lanes[frame * voxels];
        for (unsigned long long voxel = 0ULL; voxel < voxels; voxel++)
        {
            // Explicit cast takes a value below 2^16 as an unsigned short
            plane[voxel] = (unsigned short)(200u + (flatten_test_drawn() % 24u));
        }
        const unsigned long long blobs = 4ULL + (4ULL * frame);
        for (unsigned long long blob = 0ULL; blob < blobs; blob++)
        {
            // Explicit casts take each draw below the extent of the sample, which an int holds
            const int center[3] = {(int)(flatten_test_drawn() % depth), (int)(flatten_test_drawn() % height),
                                   (int)(flatten_test_drawn() % width)};
            const int radius = 2 + (int)(flatten_test_drawn() % 2u);
            const unsigned int crest = 1000u + (flatten_test_drawn() % 400u);
            for (unsigned long long voxel = 0ULL; voxel < voxels; voxel++)
            {
                // Explicit casts take each coordinate below the extent of the sample, which an int holds
                const int step[3] = {(int)(voxel / (height * width)) - center[0],
                                     (int)((voxel / width) % height) - center[1], (int)(voxel % width) - center[2]};
                const int length = (step[0] * step[0]) + (step[1] * step[1]) + (step[2] * step[2]);
                if (length <= (radius * radius))
                {
                    // Explicit cast takes a crest less a fall below 2^16 as an unsigned short
                    const unsigned short value = (unsigned short)(crest - (40u * (unsigned int)length));
                    plane[voxel] = (value > plane[voxel]) ? value : plane[voxel];
                }
            }
        }
    }
}

// The values of a sample written as a .npy file of version 1.0. The file holds its magic and a header of 118 bytes,
// then the values as u16 in little-endian order. The header pads with spaces to a newline
static int flatten_test_npy_written(const char *path, const unsigned long long *extent, const unsigned short *lanes)
{
    char header[118];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = header, .bytes = sizeof(header), .val = ' ');
    char dictionary[118];
    VerbaLinea line = mmgr_line_over(dictionary, sizeof(dictionary));
    mmgr_line_put(&line, "{'descr': '<u2', 'fortran_order': False, 'shape': (");
    for (unsigned int axis = 0u; axis < 4u; axis++)
    {
        mmgr_line_decimal(&line, extent[axis], 1u);
        mmgr_line_put(&line, (axis < 3u) ? ", " : "), }");
    }
    (void)mmgr_line_finish(&line);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = header, .src = dictionary,
               .bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = dictionary, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    header[sizeof(header) - 1u] = '\n';
    const unsigned char magic[10] = {0x93u, 'N', 'U', 'M', 'P', 'Y', 1u, 0u, (unsigned char)sizeof(header), 0u};
    const size_t values = (size_t)(extent[0] * extent[1] * extent[2] * extent[3]);
    PlatformFile *const file = platform_file_open(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    const int ok = (platform_file_write_items(magic, 1u, sizeof(magic), file) == sizeof(magic)) &&
                   (platform_file_write_items(header, 1u, sizeof(header), file) == sizeof(header)) &&
                   (platform_file_write_items(lanes, sizeof(unsigned short), values, file) == values);
    return (platform_file_close(file) == 0) && ok;
}

// the first 8 bytes of the signum of `count` bytes as a word, little end first, or 0 where the signum errors
static unsigned long long flatten_test_signum(const void *bytes, unsigned long long count)
{
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    // Explicit cast reads the magnitudes as the bytes that the signum reads
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
    if (argc < 2)
    {
        return 1;
    }
    // the count of the bodies and the first word of the signum of their magnitudes, the answer of the device path
    static const unsigned long long BODIES = 262ULL;
    static const unsigned long long SIGNUM = 0xdec5cff39d0f4952ULL;
    const char *const top = argv[1];
    char source[FLATTEN_TEST_PATH];
    char set[FLATTEN_TEST_PATH];
    char npy[FLATTEN_TEST_SAMPLES][FLATTEN_TEST_PATH];
    if (!flatten_test_path(source, top, "/source") || !flatten_test_path(set, top, "/set") ||
        !flatten_test_path(npy[0], top, "/source/sample_a.npy") ||
        !flatten_test_path(npy[1], top, "/source/sample_b.npy") || !engine_directories_make(source, 1) ||
        !engine_directories_make(set, 1))
    {
        return 1;
    }
    const unsigned int place = platform_place_g_program_owner;
    unsigned short *const lanes =
        (unsigned short *)platform_cell_taken(FLATTEN_TEST_VALUES_MAX * sizeof(unsigned short), place, 1);
    if (lanes == NULL)
    {
        return 1;
    }
    int written = 1;
    for (unsigned int sample = 0u; sample < FLATTEN_TEST_SAMPLES; sample++)
    {
        flatten_test_sample(FLATTEN_TEST_EXTENTS[sample], lanes);
        written = written && flatten_test_npy_written(npy[sample], FLATTEN_TEST_EXTENTS[sample], lanes);
    }
    platform_cell_released(lanes, place);
    unsigned int failed = flatten_test_check(written, "the samples are written as .npy sources");

    char name_a[] = "sample_a";
    char name_b[] = "sample_b";
    char *const samples[FLATTEN_TEST_SAMPLES] = {name_a, name_b};
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    EngineSampleRecord records[FLATTEN_TEST_SAMPLES];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = records, .bytes = sizeof(records), .val = 0);
    EngineSetReport report;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &report, .bytes = sizeof(report), .val = 0);
    report.samples = records;
    const EngineIngestRequest ingest = {source, set, samples, FLATTEN_TEST_SAMPLES, "tzyx", 0u, &error, &report};
    failed += flatten_test_check(engine_ingest_set(&ingest) == 0L, "the ingest seals the samples as a set");

    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    FlattenSetRequest flatten;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &flatten, .bytes = sizeof(flatten), .val = 0);
    flatten.set = set;
    flatten.names = samples;
    flatten.count = FLATTEN_TEST_SAMPLES;
    static const unsigned int SMOOTH[ENGINE_AXES] = {1u, 3u, 3u};
    static const unsigned int BACKGROUND[ENGINE_AXES] = {2u, 8u, 8u};
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = flatten.smooth_orders, .src = SMOOTH,
               .bytes = sizeof(flatten.smooth_orders));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = flatten.background_orders, .src = BACKGROUND,
               .bytes = sizeof(flatten.background_orders));
    flatten.error = &error;
    failed += flatten_test_check(flatten_set(&flatten) != 0, "the flatten packs the bodies of each frame");

    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    FlattenResident resident;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &resident, .bytes = sizeof(resident), .val = 0);
    const int read = flatten_read(set, &resident, &error) != 0;
    failed += flatten_test_check(read && (resident.bodies != 0ULL), "the read gives the bodies back");
    const unsigned long long word =
        read ? flatten_test_signum(resident.magnitudes, resident.bodies * resident.layout.limbs * sizeof(unsigned int))
             : 0ULL;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal(output_line, resident.bodies, 1u);
        mmgr_line_put(output_line, " bodies of ");
        mmgr_line_decimal(output_line, resident.layout.limbs, 1u);
        mmgr_line_put(output_line, " limbs: signum 0x");
        mmgr_line_hex(output_line, word, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    failed += flatten_test_check((resident.bodies == BODIES) && (word != 0ULL) && (word == SIGNUM),
                                 "the bodies and their magnitudes are the answer of the device path");
    if (read)
    {
        flatten_release(&resident);
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  flatten paths test: ");
    mmgr_line_decimal(output_line, s_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
