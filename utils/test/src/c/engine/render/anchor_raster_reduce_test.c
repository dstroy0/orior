// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// anchor_raster_reduce_test.c: the host raster and volume reduce a 0 byte the way the device's identity seed does
#include "anchor_raster.h"

#include <stdio.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void reduce_check(int held, const char *what, unsigned int first, unsigned int second)
{
    s_checks += 1u;
    if (held != 0)
    {
        printf("  held: %s (%u %u)\n", what, first, second);
    }
    else
    {
        printf("  FAILED: %s (%u %u)\n", what, first, second);
        s_failed += 1u;
    }
}

int main(void)
{
    // Eight alignments of a 2-byte needle over nine bytes. Laid in rows over two cells, cell 0 takes the bytes 0, 9,
    // 7, 5 and cell 1 takes 0, 3, 4, 6, a 0 arriving first in each. Laid in slabs over two voxels, voxel 0 takes 0, 7,
    // 0, 4 and voxel 1 takes 9, 5, 3, 6
    const uint8_t corpus[9] = {0u, 9u, 7u, 5u, 0u, 3u, 4u, 6u, 1u};
    const uint8_t needle[2] = {1u, 1u};
    AnchorRasterConfig config = {2u, 1u, ANCHOR_LAYOUT_ROWS, ANCHOR_CHANNEL_BYTE, ANCHOR_REDUCE_MIN, 1u};
    uint8_t pixels[2] = {0u, 0u};
    int rendered = anchor_raster_host(pixels, &config, corpus, 9u, needle, 2u, NULL, 0u);
    reduce_check((rendered != 0) && (pixels[0] == 0u) && (pixels[1] == 0u),
                 "the raster's minimum keeps a 0 byte that arrives first", pixels[0], pixels[1]);
    config.reduce = ANCHOR_REDUCE_MAX;
    rendered = anchor_raster_host(pixels, &config, corpus, 9u, needle, 2u, NULL, 0u);
    reduce_check((rendered != 0) && (pixels[0] == 9u) && (pixels[1] == 6u),
                 "the raster's maximum over the same bytes is 9 and 6", pixels[0], pixels[1]);
    AnchorVolumeConfig volume = {2u, 1u, 1u, ANCHOR_VOLUME_SLABS, ANCHOR_CHANNEL_BYTE, ANCHOR_REDUCE_MIN, 1u};
    uint8_t voxels[2] = {0u, 0u};
    rendered = anchor_volume_render_host(voxels, &volume, corpus, 9u, needle, 2u, NULL, 0u, NULL);
    reduce_check((rendered != 0) && (voxels[0] == 0u) && (voxels[1] == 3u),
                 "the volume's minimum keeps a 0 byte that arrives first and again later", voxels[0], voxels[1]);
    // sixteen cells and eight alignments: the cells nothing reaches write ANCHOR_RASTER_EMPTY
    uint8_t wide[16];
    const AnchorRasterConfig sparse = {16u, 1u, ANCHOR_LAYOUT_ROWS, ANCHOR_CHANNEL_BYTE, ANCHOR_REDUCE_MIN, 1u};
    rendered = anchor_raster_host(wide, &sparse, corpus, 9u, needle, 2u, NULL, 0u);
    unsigned int empty = 0u;
    for (unsigned int cell = 0u; cell < 16u; cell += 1u)
    {
        empty += (wide[cell] == (uint8_t)ANCHOR_RASTER_EMPTY) ? 1u : 0u;
    }
    // the eight alignments land on cells 0, 2, ..., 14; of those, two hold the byte 0
    reduce_check((rendered != 0) && (empty == 10u) && (wide[2] == 9u), "a raster wider than its alignments", empty,
                 wide[2]);
    printf("  anchor_raster reduce test: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
