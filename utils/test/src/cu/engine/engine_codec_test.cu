// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine_codec_test.cu: the codec of the engine from end to end, with the engine linked into the program. The test
// writes one seeded sample as a .npy source. The ingest encodes it as one job, seals it, writes the crystal, reads it
// back and verifies it. The proof of the set verifies the crystal again from the disk. The load must give the samples
// back, and the bits of a lattice of ints must equal the bits of tower_lift and compression_encode
#include "../../../../../src/cu/engine/analysis/compression/compression.h"
#include "../../../../../src/cu/engine/analysis/tower/tower.h"
#include "../../../../../src/cu/engine/engine.h"
#include "../../../../../src/platform/platform.h"

#include <cuda_runtime.h>

#include <cstring>

// the extent of the sample, t, z, y and x. Each row of 700 samples is more than one chunk of a seal
#define CODEC_TEST_T 2ULL
#define CODEC_TEST_Z 6ULL
#define CODEC_TEST_Y 20ULL
#define CODEC_TEST_X 700ULL
#define CODEC_TEST_LANES (CODEC_TEST_T * CODEC_TEST_Z * CODEC_TEST_Y * CODEC_TEST_X)

// the room of one path of the test
#define CODEC_TEST_PATH 1024u

// the seeded words that the sample draws its noise from
static unsigned int s_seed = 0xC2B2AE35u;

static unsigned int codec_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// one check, printed as it passes or fails
static unsigned int codec_test_check(int held, const char *what)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// `top`, then `name`, into `out`: 1, or 0 where the path passes the room
static int codec_test_path(char *out, const char *top, const char *name)
{
    const size_t top_length = strlen(top);
    const size_t name_length = strlen(name);
    if ((top_length + name_length + 1u) > CODEC_TEST_PATH)
    {
        return 0;
    }
    memcpy(out, top, top_length);
    memcpy(out + top_length, name, name_length + 1u);
    return 1;
}

// The samples written as a .npy file of version 1.0: its magic, its header of 118 bytes, then the samples as u16 in
// little-endian order. The header pads with spaces to a newline. The samples thus start at byte 128
static int codec_test_npy_written(const char *path, const unsigned short *lanes)
{
    char header[118];
    memset(header, ' ', sizeof(header));
    const char *const dictionary = "{'descr': '<u2', 'fortran_order': False, 'shape': (2, 6, 20, 700), }";
    memcpy(header, dictionary, strlen(dictionary));
    header[sizeof(header) - 1u] = '\n';
    const unsigned char magic[10] = {0x93u, 'N', 'U', 'M', 'P', 'Y', 1u, 0u, (unsigned char)sizeof(header), 0u};
    PlatformFile *const file = platform_file_open(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    const int ok = (platform_file_write_items(magic, 1u, sizeof(magic), file) == sizeof(magic)) &&
                   (platform_file_write_items(header, 1u, sizeof(header), file) == sizeof(header)) &&
                   (platform_file_write_items(lanes, sizeof(unsigned short), (size_t)CODEC_TEST_LANES, file) ==
                    (size_t)CODEC_TEST_LANES);
    return (platform_file_close(file) == 0) && ok;
}

// the bits of the lattice of ints by tower_lift and compression_encode, from the memory of the device
static long codec_test_bits_kernels(const int *values, const unsigned long long extent[4], unsigned long long *bits)
{
    void *device = NULL;
    long answer = -1L;
    if ((cudaMalloc(&device, (size_t)CODEC_TEST_LANES * sizeof(int)) == cudaSuccess) &&
        (cudaMemcpy(device, values, (size_t)CODEC_TEST_LANES * sizeof(int), cudaMemcpyHostToDevice) == cudaSuccess))
    {
        EngineError error;
        memset(&error, 0, sizeof(error));
        const int *coefficients = NULL;
        unsigned int *scratch = NULL;
        unsigned int floors = 0u;
        TowerLiftRequest lift;
        memset(&lift, 0, sizeof(lift));
        // Explicit cast reads the memory of the device as the lattice that crossed into it
        lift.device_values = (const int *)device;
        memcpy(lift.extent, extent, sizeof(lift.extent));
        lift.coefficients = &coefficients;
        lift.scratch = &scratch;
        lift.floors = &floors;
        lift.error = &error;
        unsigned long long chunks = 0ULL;
        const unsigned long long *offsets = NULL;
        const unsigned int *stream = NULL;
        CompressionEncodeRequest code;
        memset(&code, 0, sizeof(code));
        code.count = CODEC_TEST_LANES;
        code.chunks = &chunks;
        code.bits = bits;
        code.offsets = &offsets;
        code.stream = &stream;
        code.error = &error;
        if (tower_lift(&lift) == 0L)
        {
            code.device_coefficients = coefficients;
            code.device_scratch = scratch;
            answer = compression_encode(&code);
        }
    }
    cudaFree(device);
    return answer;
}

int program_main(int argc, char **argv)
{
    if (argc < 2)
    {
        return 1;
    }
    const char *const top = argv[1];
    char source[CODEC_TEST_PATH];
    char set[CODEC_TEST_PATH];
    char npy[CODEC_TEST_PATH];
    if (!codec_test_path(source, top, "/source") || !codec_test_path(set, top, "/set") ||
        !codec_test_path(npy, top, "/source/sample_a.npy") || !engine_directories_make(source, 1) ||
        !engine_directories_make(set, 1))
    {
        return 1;
    }
    const unsigned int place = platform_place_g_program_owner;
    unsigned short *const lanes =
        (unsigned short *)platform_cell_taken((size_t)CODEC_TEST_LANES * sizeof(unsigned short), place, 1);
    int *const values = (int *)platform_cell_taken((size_t)CODEC_TEST_LANES * sizeof(int), place, 1);
    if ((lanes == NULL) || (values == NULL))
    {
        return 1;
    }
    for (unsigned long long at = 0ULL; at < CODEC_TEST_LANES; at++)
    {
        const unsigned long long x = at % CODEC_TEST_X;
        const unsigned long long y = (at / CODEC_TEST_X) % CODEC_TEST_Y;
        // Explicit casts keep a smooth ramp and a little noise below 12 bits as a sample, and its value about 0
        lanes[at] = (unsigned short)(((x * 3ULL) + (y * 5ULL) + (codec_test_drawn() % 40u)) & 0x0FFFULL);
        values[at] = (int)lanes[at] - 2048;
    }
    unsigned int failed = codec_test_check(codec_test_npy_written(npy, lanes), "the sample is written as a .npy");

    char sample_name[] = "sample_a";
    char *const samples[1] = {sample_name};
    EngineError error;
    memset(&error, 0, sizeof(error));
    EngineSampleRecord record;
    memset(&record, 0, sizeof(record));
    EngineSetReport report;
    memset(&report, 0, sizeof(report));
    report.samples = &record;
    const EngineIngestRequest ingest = {source, set, samples, 1u, "tzyx", 0u, &error, &report};
    failed += codec_test_check((engine_ingest_set(&ingest) == 0L) && (record.crystal_written == 1ULL) &&
                                   (record.crystal_read == 1ULL) && (record.voxels_differ == 0ULL) &&
                                   (record.pixels_differ == 0ULL) && (record.rows_differ == 0ULL) &&
                                   (record.chunks_differ == 0ULL) && (record.roots_differ == 0ULL),
                               "the ingest encodes, seals, writes, reads back and verifies the crystal");

    memset(&error, 0, sizeof(error));
    memset(&record, 0, sizeof(record));
    memset(&report, 0, sizeof(report));
    report.samples = &record;
    const EngineSetRequest prove = {set, samples, 1u, &error, &report};
    failed += codec_test_check((engine_iapx_prove_set(&prove) == 0L) && (record.voxels_differ == 0ULL) &&
                                   (record.rows_differ == 0ULL) && (record.roots_differ == 0ULL),
                               "the proof of the set verifies the crystal from the disk");

    memset(&error, 0, sizeof(error));
    unsigned long long extent[4] = {0ULL, 0ULL, 0ULL, 0ULL};
    unsigned short *volume = NULL;
    EngineSignum root;
    const int loaded = engine_iapx_load(set, sample_name, extent, &volume, &root, NULL, &error) == 0L;
    const unsigned long long wanted[4] = {CODEC_TEST_T, CODEC_TEST_Z, CODEC_TEST_Y, CODEC_TEST_X};
    failed += codec_test_check(loaded && (memcmp(extent, wanted, sizeof(extent)) == 0) && (volume != NULL) &&
                                   (memcmp(volume, lanes, (size_t)CODEC_TEST_LANES * sizeof(unsigned short)) == 0),
                               "the load gives back the samples that went in");
    platform_cell_released(volume, place);

    memset(&error, 0, sizeof(error));
    unsigned long long bits = 0ULL;
    unsigned long long bits_kernels = 1ULL;
    failed += codec_test_check((engine_lattice_bits(values, wanted, &bits, &error) == 0L) &&
                                   (codec_test_bits_kernels(values, wanted, &bits_kernels) == 0L) &&
                                   (bits == bits_kernels) && (bits != 0ULL),
                               "the bits of a lattice of ints equal the bits of tower_lift and compression_encode");

    platform_cell_released(values, place);
    platform_cell_released(lanes, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  engine codec test: 5 checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
