// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster_device_test.cu: the device arm of the renderer against the host arm. The test draws a seeded corpus with a
// period in it, a needle from the corpus and two probes. It renders each layout, channel and reduce rule as a sheet
// and as a volume, of two sizes each, on the device and on the host. The device must answer each case, its bytes must
// equal the bytes of the host, and the signum of all its bytes must equal the signum of the device path
#include "../../../../../../src/cu/engine/render/orior_raster.h"
#include "../../../../../../includes/platform/platform.h"

#include "memoria_operor/memoria_operor.h"

// the bytes of the corpus and of the needle, the cells of the largest sheet or volume, and the bytes that the test
// signs
#define RASTER_TEST_CORPUS 6000u
#define RASTER_TEST_NEEDLE 12u
#define RASTER_TEST_CELLS 4096u
#define RASTER_TEST_SIGNED (2u * 2u * ORIOR_RASTER_LAYOUTS * ORIOR_RASTER_CHANNELS * 2u * RASTER_TEST_CELLS)

// the pool of the test, for its corpus and its renders
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 26u;

// the seeded words that the corpus draws from
static unsigned int s_seed = 0x1F83D9ABu;

static unsigned int raster_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// the count of the checks, and of the checks that failed
static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void raster_test_check(int held, const char *what)
{
    s_checks += 1u;
    s_failed += (held != 0) ? 0u : 1u;
    if (held == 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  FAIL ");
        mmgr_line_put(output_line, what);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

// the first 8 bytes of a word of FNV-1a over `count` bytes, which the test prints and holds as its signum
static unsigned long long raster_test_signum(const unsigned char *bytes, unsigned long long count)
{
    unsigned long long word = 1469598103934665603ULL;
    for (unsigned long long at = 0ULL; at < count; at++)
    {
        word = (word ^ bytes[at]) * 1099511628211ULL;
    }
    return word;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the signum of every byte that the device renders, the answer of the device path
    static const unsigned long long SIGNUM = 0x1911c81159f171b6ULL;
    static const size_t SHEETS[2][2] = {{48u, 40u}, {64u, 64u}};
    static const size_t VOLUMES[2][3] = {{12u, 10u, 8u}, {16u, 16u, 16u}};
    const unsigned int place = platform_place_g_program_owner;
    unsigned char *const corpus = (unsigned char *)platform_cell_taken(RASTER_TEST_CORPUS, place, 1);
    unsigned char *const device_cells = (unsigned char *)platform_cell_taken(RASTER_TEST_CELLS, place, 1);
    unsigned char *const host_cells = (unsigned char *)platform_cell_taken(RASTER_TEST_CELLS, place, 1);
    unsigned char *const signed_bytes = (unsigned char *)platform_cell_taken(RASTER_TEST_SIGNED, place, 1);
    if ((corpus == NULL) || (device_cells == NULL) || (host_cells == NULL) || (signed_bytes == NULL))
    {
        return 1;
    }
    // a corpus of a period of 37 bytes, with about 1 byte of 20 drawn at random
    for (unsigned int at = 0u; at < RASTER_TEST_CORPUS; at++)
    {
        const unsigned char periodic = (unsigned char)('A' + ((at * 7u) % 37u) % 26u);
        corpus[at] = ((raster_test_drawn() % 20u) == 0u) ? (unsigned char)(raster_test_drawn() & 0xFFu) : periodic;
    }
    const unsigned char *const needle = &corpus[1110u];
    const OriorRasterProbe probes[2] = {{0u, 1u, 6u}, {3u, 2u, 4u}};
    raster_test_check(orior_raster_device_available() != 0, "a device answers");
    unsigned long long signed_count = 0ULL;
    unsigned int cases = 0u;
    for (unsigned int shape = 0u; shape < 2u; shape++)
    {
        for (unsigned int layout = 0u; layout < ORIOR_RASTER_LAYOUTS; layout++)
        {
            for (unsigned int channel = 0u; channel < ORIOR_RASTER_CHANNELS; channel++)
            {
                for (unsigned int reduce = 0u; reduce < 2u; reduce++)
                {
                    for (unsigned int volume = 0u; volume < 2u; volume++)
                    {
                        size_t cells = 0u;
                        int drawn = 0;
                        int hosted = 0;
                        EMBED_CALL(memor.set, MemoriaCfg, .dst = device_cells, .bytes = RASTER_TEST_CELLS, .val = 0xA5);
                        EMBED_CALL(memor.set, MemoriaCfg, .dst = host_cells, .bytes = RASTER_TEST_CELLS, .val = 0x5A);
                        if (volume == 0u)
                        {
                            OriorRasterConfig config;
                            EMBED_CALL(memor.set, MemoriaCfg, .dst = &config, .bytes = sizeof(config), .val = 0);
                            config.width = SHEETS[shape][0];
                            config.height = SHEETS[shape][1];
                            config.layout = (OriorRasterLayout)layout;
                            config.channel = (OriorRasterChannel)channel;
                            config.reduce = (OriorRasterReduce)reduce;
                            config.gain = (unsigned char)(1u + shape);
                            cells = config.width * config.height;
                            drawn = orior_raster_device(device_cells, &config, corpus, RASTER_TEST_CORPUS, needle,
                                                        RASTER_TEST_NEEDLE, probes, 2u);
                            hosted = orior_raster_host(host_cells, &config, corpus, RASTER_TEST_CORPUS, needle,
                                                       RASTER_TEST_NEEDLE, probes, 2u);
                        }
                        else
                        {
                            OriorVolumeConfig config;
                            EMBED_CALL(memor.set, MemoriaCfg, .dst = &config, .bytes = sizeof(config), .val = 0);
                            config.width = VOLUMES[shape][0];
                            config.height = VOLUMES[shape][1];
                            config.depth = VOLUMES[shape][2];
                            config.layout = (OriorVolumeLayout)layout;
                            config.channel = (OriorRasterChannel)channel;
                            config.reduce = (OriorRasterReduce)reduce;
                            config.gain = (unsigned char)(1u + shape);
                            cells = config.width * config.height * config.depth;
                            drawn = orior_volume_device(device_cells, &config, corpus, RASTER_TEST_CORPUS, needle,
                                                        RASTER_TEST_NEEDLE, probes, 2u, NULL);
                            hosted = orior_volume_render_host(host_cells, &config, corpus, RASTER_TEST_CORPUS, needle,
                                                              RASTER_TEST_NEEDLE, probes, 2u, NULL);
                        }
                        cases += 1u;
                        raster_test_check(drawn == hosted, "the device answers each case as the host answers it");
                        raster_test_check((hosted == 0) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device_cells,
                                                                       .other = host_cells, .bytes = cells) == 0),
                                          "the bytes of the device are the bytes of the host");
                        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &signed_bytes[signed_count], .src = device_cells,
                                   .bytes = cells);
                        signed_count += (hosted != 0) ? cells : 0u;
                    }
                }
            }
        }
    }
    const unsigned long long word = raster_test_signum(signed_bytes, signed_count);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal(output_line, cases, 1u);
        mmgr_line_put(output_line, " cases, ");
        mmgr_line_decimal(output_line, signed_count, 1u);
        mmgr_line_put(output_line, " bytes: signum 0x");
        mmgr_line_hex(output_line, word, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    raster_test_check((signed_count != 0ULL) && (word == SIGNUM),
                      "the bytes of the device are the answer of the device path");
    platform_cell_released(signed_bytes, place);
    platform_cell_released(host_cells, place);
    platform_cell_released(device_cells, place);
    platform_cell_released(corpus, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  raster device test: ");
    mmgr_line_decimal(output_line, s_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, s_failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (s_failed == 0u) ? 0 : 1;
}
