// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// coverage.h: the one rasterizer. A scene takes the frame's paths in 16.16 in paint order. Each path becomes sparse
// strips of 4 by 4 tiles as it is filled, its edge pixels' shares stored in Morton order and its inside left as runs.
// One render then composites every run over the target, a 256 by 4 wide tile at a time, reading and writing each pixel
// once (coverage.c)
#ifndef CLI_COVERAGE_H
#define CLI_COVERAGE_H
#include "embed_dispatch_layout.h"

typedef struct CliCoverageCfg CliCoverageCfg;

// The pixels across and down a tile. A tile of 16 shares of 4 bytes fills one cache line of 64 bytes
#define CLI_COVERAGE_TILE 4u
#define CLI_COVERAGE_TILE_BYTES 64u

// The outline's points, `x` and `y` in pixels with y up, and the contours they make: contour k ends before
// point `end[k]`. Each point and the next in its contour make an edge, and the last point closes to the first.
// `origin_x` and `origin_y` are the outline's point at the top left corner of the cover: the cover's pixel (c, r) spans
// x from origin_x + c to origin_x + c + 1 and y from origin_y - r - 1 to origin_y - r. `cover` takes `width` by
// `height` shares, row after row, each from 0 to 1. The shares follow the nonzero rule on the area each edge leaves.
// Every point lies within 16384 pixels of the origin, which the 16.16 numbers hold
void cli_coverage_draw(const CliCoverageCfg *args);
struct CliCoverageCfg
{
    const float *x;
    const float *y;
    const unsigned int *end;
    unsigned int contours;
    float origin_x;
    float origin_y;
    float *cover;
    unsigned long long width;
    unsigned long long height;
};

typedef struct
{
    void (*draw)(const CliCoverageCfg *args);
} CliCoverageNs;
EMBED_TABLE_LAYOUT(CliCoverageNs, draw);
EMBED_TABLE_STORAGE CliCoverageNs cli_coverage EMBED_UNUSED = {
    .draw = cli_coverage_draw,
};

typedef struct CliCoverageSceneCfg CliCoverageSceneCfg;

// One pixel in 16.16 fixed point, and the pixels across a wide tile, the stretch of 4 rows the fine pass composites
// at once
#define CLI_COVERAGE_ONE 65536
#define CLI_COVERAGE_WIDE 256u

// A point in 16.16 pixels of the target, y down
typedef struct
{
    int x;
    int y;
} CliCoveragePoint;

// The scene: what the frame draws, in the order it draws it, as commands for the fine pass. Each fill's path becomes
// strips of 4 by 4 tiles at the moment it is filled: `share` holds 16 coverage bytes for each tile in a strip, in
// Morton order. Each command lays a run of whole tiles of one wide tile's row in one color: a strip by its tiles'
// shares from `command_share`, or a gap between strips by a share for each of its 4 pixel rows packed in
// `command_rows`. The edges and tiles hold one path at a time; `key` holds the commands' sort keys. Each cell comes
// from one block the scene takes at init, sized by its capacity: `capacity` tiles a path, `capacity` commands and
// 16 shares a tile. A fill past a capacity is dropped whole and counted in `dropped`
typedef struct
{
    unsigned int width;
    unsigned int height;
    unsigned int capacity;
    void *block;
    long long *slope;
    int *low_y;
    int *high_y;
    int *x_low;
    int *way;
    unsigned int edges;
    unsigned long long *tile;
    unsigned long long *tile_spare;
    unsigned int tiles;
    unsigned char *share;
    unsigned int shares;
    unsigned long long *key;
    unsigned long long *key_spare;
    unsigned int *command_x;
    unsigned int *command_tiles;
    unsigned int *command_share;
    unsigned int *command_color;
    unsigned int *command_rows;
    unsigned int commands;
    unsigned int dropped;
} CliCoverageScene;

// The scene made for a target `width` by `height` pixels with `capacity` for each of its parts: 1, or 0 where no
// cell holds it. Given back, its cell with it. Cleared, it draws nothing. `fill` puts the path of `contours` contours
// of 16.16 `point`s, contour k ending before point end[k], in `color` over what the scene draws before it, by the
// nonzero rule. `render` composites the scene over the target's pixels, `pitch` pixels from row to row, inside the
// clip `clip_x`, `clip_y`, `clip_width` by `clip_height` and no further: each pixel is read once and written once,
// and a stretch with nothing drawn on it is not touched.
int cli_coverage_scene_init(const CliCoverageSceneCfg *args);
void cli_coverage_scene_free(const CliCoverageSceneCfg *args);
void cli_coverage_scene_clear(const CliCoverageSceneCfg *args);
void cli_coverage_scene_fill(const CliCoverageSceneCfg *args);
void cli_coverage_scene_render(const CliCoverageSceneCfg *args);
struct CliCoverageSceneCfg
{
    CliCoverageScene *scene;
    unsigned int width;
    unsigned int height;
    unsigned int capacity;
    const CliCoveragePoint *point;
    const unsigned int *end;
    unsigned int contours;
    unsigned int color;
    unsigned int *target;
    unsigned int pitch;
    int clip_x;
    int clip_y;
    int clip_width;
    int clip_height;
};

typedef struct
{
    int (*init)(const CliCoverageSceneCfg *args);
    void (*free)(const CliCoverageSceneCfg *args);
    void (*clear)(const CliCoverageSceneCfg *args);
    void (*fill)(const CliCoverageSceneCfg *args);
    void (*render)(const CliCoverageSceneCfg *args);
} CliCoverageSceneNs;
EMBED_TABLE_LAYOUT(CliCoverageSceneNs, init, free, clear, fill, render);
EMBED_TABLE_STORAGE CliCoverageSceneNs cli_coverage_scene EMBED_UNUSED = {
    .init = cli_coverage_scene_init,
    .free = cli_coverage_scene_free,
    .clear = cli_coverage_scene_clear,
    .fill = cli_coverage_scene_fill,
    .render = cli_coverage_scene_render,
};

#endif
