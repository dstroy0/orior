// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_raster_output.c: the PGM, the volume and the device entry points
#include "orior_raster_internal.h"
#include "../../../../includes/platform/platform.h"

int orior_raster_write_pgm(const char *path, const uint8_t *pixels, size_t width, size_t height)
{
    if ((path == NULL) || (pixels == NULL) || (width == 0u) || (height == 0u))
    {
        return 0;
    }

    PlatformFile *handle = platform_file_open(path, "wb");
    if (handle == NULL)
    {
        return 0;
    }

    unsigned long long formed_mark_1;
    if ((formed_mark_1 = platform_file_mark(handle), mmgr_line_put(platform_file_line(handle), "P5\n"),
         mmgr_line_decimal(platform_file_line(handle), width, 1u), mmgr_line_put(platform_file_line(handle), " "),
         mmgr_line_decimal(platform_file_line(handle), height, 1u),
         mmgr_line_put(platform_file_line(handle), "\n255\n"), platform_file_written_since(handle, formed_mark_1)) < 0)
    {
        platform_file_close(handle);
        return 0;
    }

    const size_t cells = width * height;
    const size_t written = platform_file_write_items(pixels, 1u, cells, handle);
    platform_file_close(handle);
    return (written == cells) ? 1 : 0;
}

const char *orior_volume_layout_name(OriorVolumeLayout layout)
{
    if (layout == ORIOR_VOLUME_SLABS)
    {
        return "slabs";
    }
    if (layout == ORIOR_VOLUME_BOUSTRO)
    {
        return "boustrophedon";
    }
    if (layout == ORIOR_VOLUME_MORTON)
    {
        return "morton";
    }
    if (layout == ORIOR_VOLUME_HELIX)
    {
        return "helix";
    }
    return "unknown";
}

/**
 * @brief Whether a value is a power of two and not zero.
 *
 * @param[in] value The extent to test.
 * @return          1 where the value is a power of two, 0 otherwise.
 */
static int volume_is_power_of_two(size_t value)
{
    return ((value != 0u) && ((value & (value - 1u)) == 0u)) ? 1 : 0;
}

size_t orior_volume_cell_for(const OriorVolumeConfig *config, size_t alignment)
{
    if (config == NULL)
    {
        return 0u;
    }

    const size_t width = config->width;
    const size_t height = config->height;
    const size_t depth = config->depth;
    const size_t cells = width * height * depth;

    if ((width == 0u) || (height == 0u) || (depth == 0u))
    {
        return 0u;
    }

    // Out of range folds back into the block. Every layout below is a bijection on [0, cells), and
    // an alignment count above the block size has to land somewhere; wrapping preserves the map total
    // and is stated and not left to an out of bounds write.
    const size_t at = alignment % cells;
    const size_t sheet = width * height;

    switch (config->layout)
    {
    case ORIOR_VOLUME_SLABS: {
        return at;
    }
    case ORIOR_VOLUME_BOUSTRO: {
        const size_t slab = at / sheet;
        const size_t within = at % sheet;
        size_t row = within / width;
        size_t column = within % width;

        // Reverse every other row, then reverse the row order of every other slab. Consecutive
        // alignments stay adjacent across a row boundary and across a slab boundary both.
        if ((row % 2u) == 1u)
        {
            column = (width - 1u) - column;
        }
        if ((slab % 2u) == 1u)
        {
            row = (height - 1u) - row;
        }
        return (slab * sheet) + (row * width) + column;
    }
    case ORIOR_VOLUME_MORTON: {
        // Errored and not remapped where the extents are not powers of two, because the
        // interleave is a bijection only then and a silent fallback would make two
        // configurations render identically while reporting different layouts.
        if ((volume_is_power_of_two(width) == 0) || (volume_is_power_of_two(height) == 0) ||
            (volume_is_power_of_two(depth) == 0))
        {
            return cells;
        }

        size_t x = 0u;
        size_t y = 0u;
        size_t z = 0u;
        for (size_t bit = 0u; bit < (sizeof(size_t) * 8u) / 3u; bit += 1u)
        {
            x |= ((at >> ((3u * bit) + 0u)) & 1u) << bit;
            y |= ((at >> ((3u * bit) + 1u)) & 1u) << bit;
            z |= ((at >> ((3u * bit) + 2u)) & 1u) << bit;
        }
        x %= width;
        y %= height;
        z %= depth;
        return (z * sheet) + (y * width) + x;
    }
    case ORIOR_VOLUME_HELIX: {
        const size_t slab = at / sheet;
        const size_t within = at % sheet;
        const size_t row = within / width;
        const size_t column = (within + slab) % width;

        // A shear by the depth index. Adding the slab to the column is a bijection on each row
        // because it is addition modulo the width, and a feature at a fixed corpus offset
        // therefore advances one column per slab and winds through the block.
        return (slab * sheet) + (row * width) + column;
    }
    default: {
        return cells;
    }
    }
}

int orior_volume_render_host(uint8_t *voxels, const OriorVolumeConfig *config, const uint8_t *corpus, size_t corpus_len,
                             const uint8_t *needle, size_t needle_len, const OriorRasterProbe *probes,
                             size_t probe_count, const void *census_in)
{
    // RESERVED, NOT READ, AND NOT DELETED. The census below is built from `corpus`. A caller
    // supplied one is discarded here. The parameter stays because a tunable with no reader is an
    // integration point and not dead weight, and the header states so at the declaration. It is not
    // the rarity source.
    (void)census_in;

    if ((voxels == NULL) || (config == NULL) || (corpus == NULL) || (needle == NULL) || (config->width == 0u) ||
        (config->height == 0u) || (config->depth == 0u) || (needle_len == 0u) || (needle_len > corpus_len))
    {
        return 0;
    }
    if ((probes == NULL) && (probe_count != 0u))
    {
        return 0;
    }

    const size_t cells = config->width * config->height * config->depth;
    // which cells an alignment reached; a cell nothing reaches writes ORIOR_RASTER_EMPTY
    uint8_t *const filled = (uint8_t *)platform_cell_cleared((unsigned long long)(cells), (unsigned long long)(1u),
                                                             (unsigned int)platform_place_g_engine_owner, 0);
    if (filled == NULL)
    {
        return 0;
    }
    for (size_t cell = 0u; cell < cells; cell += 1u)
    {
        voxels[cell] = (uint8_t)ORIOR_RASTER_EMPTY;
    }

    // The channel, the gain and the reduce rule are the raster's and are read through a raster
    // configuration built here. Re-implementing them for three dimensions would be a second copy of
    // a decision that has one place, and the two copies would answer differently the first time a
    // channel is added to one of them.
    const OriorRasterConfig flat = {config->width,   config->height, ORIOR_LAYOUT_ROWS,
                                    config->channel, config->reduce, config->gain};

    OriorRasterCensus census;
    raster_census(&census, corpus, corpus_len);

    const size_t alignments = (corpus_len - needle_len) + 1u;
    for (size_t at = 0u; at < alignments; at += 1u)
    {
        const size_t cell = orior_volume_cell_for(config, at);
        if (cell >= cells)
        {
            // The layout errored on this configuration. Erroring on every alignment identically
            // makes the error visible as an empty volume and not as a partial one.
            platform_cell_released(filled, (unsigned int)platform_place_g_engine_owner);
            return 0;
        }

        const uint8_t value = orior_raster_sample(&flat, corpus, needle, needle_len, probes, probe_count, at,
                                                  census.occurrences, census.total);
        raster_reduce(voxels, filled, cell, value, config->reduce);
    }
    platform_cell_released(filled, (unsigned int)platform_place_g_engine_owner);
    return 1;
}

int orior_volume_render(uint8_t *voxels, const OriorVolumeConfig *config, const uint8_t *corpus, size_t corpus_len,
                        const uint8_t *needle, size_t needle_len, const OriorRasterProbe *probes, size_t probe_count,
                        const void *census)
{
    // DEVICE FIRST WHERE THERE IS ONE, exactly as orior_raster_render does for a sheet. Both arms
    // produce the same bytes. This is a performance choice and never a correctness one.
    if (orior_volume_device_available() != 0)
    {
        if (orior_volume_device(voxels, config, corpus, corpus_len, needle, needle_len, probes, probe_count, census) !=
            0)
        {
            return 1;
        }
    }
    return orior_volume_render_host(voxels, config, corpus, corpus_len, needle, needle_len, probes, probe_count,
                                    census);
}

int orior_volume_write_raw(const char *path, const uint8_t *voxels, const OriorVolumeConfig *config)
{
    if ((path == NULL) || (voxels == NULL) || (config == NULL) || (config->width == 0u) || (config->height == 0u) ||
        (config->depth == 0u))
    {
        return 0;
    }

    PlatformFile *const handle = platform_file_open(path, "wb");
    if (handle == NULL)
    {
        return 0;
    }

    const size_t cells = config->width * config->height * config->depth;
    const size_t written = platform_file_write_items(voxels, 1u, cells, handle);
    platform_file_close(handle);

    if (written != cells)
    {
        return 0;
    }

    // The sidecar, because Netpbm has no volume container and inventing one would make this tree
    // the only reader of its own output. A generated file states it is generated and names what made
    // it, and somebody meeting the .raw alone can work out what to do with it.
    char sidecar[512];
    VerbaLinea formed_line_1 = mmgr_line_over(sidecar, sizeof(sidecar));
    mmgr_line_put(&formed_line_1, path);
    mmgr_line_put(&formed_line_1, ".txt");
    (void)mmgr_line_finish(&formed_line_1);
    const int used = (int)formed_line_1.at;
    if ((used <= 0) || ((size_t)used >= sizeof(sidecar)))
    {
        return 0;
    }

    PlatformFile *const notes = platform_file_open(sidecar, "wb");
    if (notes == NULL)
    {
        return 0;
    }
    {
        mmgr_line_put(platform_file_line(notes),
                      "generated by orior_volume_write_raw in the engine's render/orior_raster_*.c\n");
    }
    {
        mmgr_line_put(platform_file_line(notes),
                      "format raw unsigned 8 bit, x fastest then y then z, no header, no padding\n");
    }
    {
        mmgr_line_put(platform_file_line(notes), "width ");
        mmgr_line_decimal(platform_file_line(notes), config->width, 1u);
        mmgr_line_put(platform_file_line(notes), "\nheight ");
        mmgr_line_decimal(platform_file_line(notes), config->height, 1u);
        mmgr_line_put(platform_file_line(notes), "\ndepth ");
        mmgr_line_decimal(platform_file_line(notes), config->depth, 1u);
        mmgr_line_put(platform_file_line(notes), "\nbytes ");
        mmgr_line_decimal(platform_file_line(notes), cells, 1u);
        mmgr_line_put(platform_file_line(notes), "\n");
    }
    {
        mmgr_line_put(platform_file_line(notes), "layout ");
        mmgr_line_put(platform_file_line(notes), orior_volume_layout_name(config->layout));
        mmgr_line_put(platform_file_line(notes), "\nchannel ");
        mmgr_line_put(platform_file_line(notes), orior_raster_channel_name(config->channel));
        mmgr_line_put(platform_file_line(notes), "\n");
    }
    {
        mmgr_line_put(platform_file_line(notes), "reduce ");
        mmgr_line_put(platform_file_line(notes), (config->reduce == ORIOR_REDUCE_MAX) ? "max" : "min");
        mmgr_line_put(platform_file_line(notes), "\ngain ");
        mmgr_line_decimal(platform_file_line(notes), (unsigned)config->gain, 1u);
        mmgr_line_put(platform_file_line(notes), "\n");
    }
    platform_file_close(notes);
    return 1;
}

#if !defined(ORIOR_RASTER_HAVE_CUDA) || !ORIOR_RASTER_HAVE_CUDA

/* BOTH ARMS DEFINED. A build without the device renderer still carries these symbols. A driver
 * written against both arms links and runs against either. The available test returning zero is what
 * a caller checks before calling the other, and the other errors instead of pretending. */

int orior_raster_device_available(void)
{
    return 0;
}

int orior_raster_device(uint8_t *pixels, const OriorRasterConfig *config, const uint8_t *corpus, size_t corpus_len,
                        const uint8_t *needle, size_t needle_len, const OriorRasterProbe *probes, size_t probe_count)
{
    (void)pixels;
    (void)config;
    (void)corpus;
    (void)corpus_len;
    (void)needle;
    (void)needle_len;
    (void)probes;
    (void)probe_count;
    return 0;
}

int orior_volume_device_available(void)
{
    return 0;
}

int orior_volume_device(uint8_t *voxels, const OriorVolumeConfig *config, const uint8_t *corpus, size_t corpus_len,
                        const uint8_t *needle, size_t needle_len, const OriorRasterProbe *probes, size_t probe_count,
                        const void *census)
{
    (void)voxels;
    (void)config;
    (void)corpus;
    (void)corpus_len;
    (void)needle;
    (void)needle_len;
    (void)probes;
    (void)probe_count;
    (void)census;
    return 0;
}

#endif
