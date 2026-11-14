// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// coverage.c: the one rasterizer. All numbers are 16.16 fixed point. The edges are in a structure of arrays. The walk
// takes the cover tile by tile, in x and y order. Inside a tile it takes the 16 pixels in Morton order. Each pixel maps
// back into the outline's space, P' = M^-1 * P, as two lanes of one 64-bit word: v' in the high lane and u' in the low
// lane. A pixel's share is the sum over the edges of d * H(u'). Here d is the signed height of the edge in the pixel's
// row, and H is the mean of clamp(u' - x, 0, 1) over that part of the edge. This sum is the area each edge leaves to
// its right in the row, so it gives the nonzero rule. Each tile tests each edge once. An edge wholly left of the tile
// adds one number to each row. An edge wholly right of the tile adds nothing. Only an edge that crosses the tile goes
// to each pixel
#include "coverage.h"
#include "../arith/arith.h"

#include "memoria_operor/memoria_operor.h"

// the pixels a tile holds
#define COVERAGE_TILE_PIXELS (COVERAGE_TILE * COVERAGE_TILE)
// The low lane holds u' plus this offset. Then u' from -2^30 to 2^30 stays a positive 32-bit lane, and a step of the
// lane never carries into the high lane
#define COVERAGE_LANE_OFFSET 0x80000000u

_Static_assert((COVERAGE_TILE_PIXELS * sizeof(embed_i32)) == COVERAGE_TILE_BYTES,
               "a tile of 32-bit shares must fill one 64-byte cache line");
_Static_assert((sizeof(embed_i64) + (6u * sizeof(embed_i32))) == COVERAGE_EDGE_BYTES,
               "an edge's slope and its six 32-bit numbers fill COVERAGE_EDGE_BYTES");
_Static_assert((sizeof(long long) + (4u * sizeof(unsigned long long)) + (4u * sizeof(int)) + (5u * sizeof(unsigned int)) +
                16u) == COVERAGE_SCENE_BYTES_A_CAPACITY,
               "a scene's arrays take COVERAGE_SCENE_BYTES_A_CAPACITY for each unit of capacity");

// The edges as a structure of arrays, one entry each. For edge e: its lowest y and its highest y, its x at the lowest
// y, its least x and its max x, its way (+1 where it runs up, -1 where it runs down), and its slope, the x it moves
// for each unit of y. Then the bounds of all the edges. All are in 16.16, y up
typedef struct
{
    embed_i64 *slope;
    embed_i32 *low_y;
    embed_i32 *high_y;
    embed_i32 *x_low;
    embed_i32 *x_least;
    embed_i32 *x_max;
    embed_i32 *way;
    unsigned int count;
    embed_i32 bound_left;
    embed_i32 bound_right;
    embed_i32 bound_foot;
    embed_i32 bound_top;
} CoverageEdges;

typedef struct
{
    const CoverageCfg *cfg;
    CoverageEdges *edges;
    float value;
    embed_i32 number;
    embed_i32 least;
    embed_i32 max;
    embed_i64 span;
    unsigned int edge;
    embed_i32 y;
} CoverageCtx;

// `value` in pixels as a 16.16 number, to the nearest
EMBED_INLINE embed_i32 coverage_fixed(const CoverageCtx *args)
{
    // a value within 16384 pixels of the origin is inside what 16.16 holds
    return (embed_i32)arith_round_whole_f32(args->value * (float)COVERAGE_ONE);
}

// `number` held from `least` to `max`
EMBED_INLINE embed_i32 coverage_clamp(const CoverageCtx *args)
{
    const embed_i32 raised = (args->number < args->least) ? args->least : args->number;
    return (raised > args->max) ? args->max : raised;
}

// The integral of clamp(s, 0, 1) from 0 to `span`, `span` in 16.16: clamp(s, 0, 1) squared over 2, plus the part of s
// past 1. The result is in 16.32, so a narrow piece's mean retains all its bits
EMBED_INLINE embed_i64 coverage_ramp_area(const CoverageCtx *args)
{
    const embed_i64 clamped = (args->span < 0) ? 0 : ((args->span > COVERAGE_ONE) ? COVERAGE_ONE : args->span);
    const embed_i64 past = (args->span > COVERAGE_ONE) ? (args->span - COVERAGE_ONE) : 0;
    return ((clamped * clamped) >> 1) + (past << 16);
}

// the x of edge `edge` at `y`, which lies between its lowest y and its highest y
EMBED_INLINE embed_i32 coverage_x_at(const CoverageCtx *args)
{
    const CoverageEdges *const edges = args->edges;
    // the x lies between the edge's two ends, which 16.16 holds
    return edges->x_low[args->edge] +
           (embed_i32)((((embed_i64)args->y - edges->low_y[args->edge]) * edges->slope[args->edge]) >> 16);
}

// The edges of the outline put in `edges` with their bounds, and their count. An edge with no height leaves no area,
// so it is not put
EMBED_INLINE unsigned int coverage_edges_read(const CoverageCtx *args)
{
    const CoverageCfg *const cfg = args->cfg;
    CoverageEdges *const edges = args->edges;
    edges->bound_left = INT32_MAX;
    edges->bound_right = INT32_MIN;
    edges->bound_foot = INT32_MAX;
    edges->bound_top = INT32_MIN;
    unsigned int count = 0u;
    unsigned int first = 0u;
    for (unsigned int contour = 0u; contour < cfg->contours; contour += 1u)
    {
        const unsigned int end = cfg->end[contour];
        const unsigned int points = end - first;
        for (unsigned int pair = 0u; pair < points; pair += 1u)
        {
            const unsigned int from = first + pair;
            const unsigned int to = first + (((pair + 1u) == points) ? 0u : (pair + 1u));
            const embed_i32 from_x = EMBED_CALL(coverage_fixed, CoverageCtx, .value = cfg->x[from]);
            const embed_i32 from_y = EMBED_CALL(coverage_fixed, CoverageCtx, .value = cfg->y[from]);
            const embed_i32 to_x = EMBED_CALL(coverage_fixed, CoverageCtx, .value = cfg->x[to]);
            const embed_i32 to_y = EMBED_CALL(coverage_fixed, CoverageCtx, .value = cfg->y[to]);
            if (from_y == to_y)
            {
                continue;
            }
            edges->bound_left = (from_x < edges->bound_left) ? from_x : edges->bound_left;
            edges->bound_left = (to_x < edges->bound_left) ? to_x : edges->bound_left;
            edges->bound_right = (from_x > edges->bound_right) ? from_x : edges->bound_right;
            edges->bound_right = (to_x > edges->bound_right) ? to_x : edges->bound_right;
            edges->bound_foot = (from_y < edges->bound_foot) ? from_y : edges->bound_foot;
            edges->bound_foot = (to_y < edges->bound_foot) ? to_y : edges->bound_foot;
            edges->bound_top = (from_y > edges->bound_top) ? from_y : edges->bound_top;
            edges->bound_top = (to_y > edges->bound_top) ? to_y : edges->bound_top;
            const int up = to_y > from_y;
            const embed_i32 low_y = up ? from_y : to_y;
            const embed_i32 high_y = up ? to_y : from_y;
            const embed_i32 x_low = up ? from_x : to_x;
            const embed_i32 x_high = up ? to_x : from_x;
            edges->low_y[count] = low_y;
            edges->high_y[count] = high_y;
            edges->x_low[count] = x_low;
            edges->x_least[count] = (from_x < to_x) ? from_x : to_x;
            edges->x_max[count] = (from_x < to_x) ? to_x : from_x;
            edges->way[count] = up ? 1 : -1;
            edges->slope[count] = (((embed_i64)x_high - x_low) * COVERAGE_ONE) / ((embed_i64)high_y - low_y);
            count += 1u;
        }
        first = end;
    }
    return count;
}

// The tile at tile column `tile_x` and tile row `tile_y` of the cover. Its 16 shares go to the cover, each row's
// pixels that lie on the cover
static void coverage_tile_draw(const CoverageCfg *cfg, const CoverageEdges *edges, unsigned long long tile_x,
                               unsigned long long tile_y, embed_i32 origin_u, embed_i32 origin_v)
{
    EMBED_ALIGN(64) embed_i32 share[COVERAGE_TILE_PIXELS] = {0};
    embed_i32 left_rows[COVERAGE_TILE] = {0};
    // the tile's sides in the outline's space; a tile lies within 16384 pixels of the origin
    const embed_i32 tile_left = origin_u + (embed_i32)(tile_x * COVERAGE_TILE) * COVERAGE_ONE;
    const embed_i32 tile_right = tile_left + (embed_i32)COVERAGE_TILE * COVERAGE_ONE;
    const embed_i32 tile_top = origin_v - (embed_i32)(tile_y * COVERAGE_TILE) * COVERAGE_ONE;
    const embed_i32 tile_foot = tile_top - (embed_i32)COVERAGE_TILE * COVERAGE_ONE;
    // P' of the tile's first pixel's right side on its row's top, v' high and u' low
    const embed_u64 corner = ((embed_u64)(embed_u32)tile_top << 32u) |
                             (embed_u64)((embed_u32)(tile_left + COVERAGE_ONE) + COVERAGE_LANE_OFFSET);
    // Outside the outline's bounds every share is 0: the edges of a closed outline left of a pixel add to 0 in each
    // row. Such a tile tests no edge
    const int outside = (tile_right <= edges->bound_left) || (tile_left >= edges->bound_right) ||
                        (tile_top <= edges->bound_foot) || (tile_foot >= edges->bound_top);
    const unsigned int tested = outside ? 0u : edges->count;
    for (unsigned int edge = 0u; edge < tested; edge += 1u)
    {
        const embed_i32 low_y = edges->low_y[edge];
        const embed_i32 high_y = edges->high_y[edge];
        if ((high_y <= tile_foot) || (low_y >= tile_top) || (edges->x_least[edge] >= tile_right))
        {
            continue;
        }
        // the edge's signed height in each row of the tile
        embed_i32 height[COVERAGE_TILE];
        embed_i32 from_x[COVERAGE_TILE];
        embed_i32 to_x[COVERAGE_TILE];
        for (unsigned int row = 0u; row < COVERAGE_TILE; row += 1u)
        {
            const embed_i32 row_top = tile_top - ((embed_i32)row * COVERAGE_ONE);
            const embed_i32 piece_top =
                EMBED_CALL(coverage_clamp, CoverageCtx, .number = row_top, .least = low_y, .max = high_y);
            const embed_i32 piece_foot = EMBED_CALL(coverage_clamp, CoverageCtx, .number = row_top - COVERAGE_ONE,
                                                    .least = low_y, .max = high_y);
            height[row] = edges->way[edge] * (piece_top - piece_foot);
            from_x[row] =
                EMBED_CALL(coverage_x_at, CoverageCtx, .edges = (CoverageEdges *)edges, .edge = edge, .y = piece_foot);
            to_x[row] =
                EMBED_CALL(coverage_x_at, CoverageCtx, .edges = (CoverageEdges *)edges, .edge = edge, .y = piece_top);
        }
        if (edges->x_max[edge] <= tile_left)
        {
            // wholly left of the tile: H is 1 at every pixel's right side
            for (unsigned int row = 0u; row < COVERAGE_TILE; row += 1u)
            {
                left_rows[row] += height[row];
            }
            continue;
        }
        // the reciprocal of each row piece's width, and whether the piece is straight up
        embed_i64 reciprocal[COVERAGE_TILE];
        embed_i32 upright[COVERAGE_TILE];
        for (unsigned int row = 0u; row < COVERAGE_TILE; row += 1u)
        {
            const embed_i64 width = (embed_i64)to_x[row] - from_x[row];
            upright[row] = -(embed_i32)(width == 0);
            reciprocal[row] = ((embed_i64)1 << 32) / (width + (embed_i64)(width == 0));
        }
        for (unsigned int local = 0u; local < COVERAGE_TILE_PIXELS; local += 1u)
        {
            const unsigned int across = (local & 1u) | ((local >> 1u) & 2u);
            const unsigned int down = ((local >> 1u) & 1u) | ((local >> 2u) & 2u);
            // P' of this pixel: one pixel along u' for each pixel across, one pixel back along v' for each row down
            const embed_u64 point =
                corner + ((embed_u64)across * COVERAGE_ONE) - ((embed_u64)down * COVERAGE_ONE << 32u);
            // the low lane less its offset is u'; the 32-bit pattern reads as a signed number
            const embed_i64 u = (embed_i64)(embed_i32)((embed_u32)point - COVERAGE_LANE_OFFSET);
            const embed_i64 near = EMBED_CALL(coverage_ramp_area, CoverageCtx, .span = u - from_x[down]);
            const embed_i64 far = EMBED_CALL(coverage_ramp_area, CoverageCtx, .span = u - to_x[down]);
            // the areas in 16.32 over the width in 16.16 give the mean in 16.16
            const embed_i64 sloped = ((near - far) * reciprocal[down]) >> 32;
            const embed_i64 straight = ((u - from_x[down]) < 0)
                                           ? 0
                                           : (((u - from_x[down]) > COVERAGE_ONE) ? COVERAGE_ONE : (u - from_x[down]));
            const embed_i64 mean = (straight & upright[down]) | (sloped & ~(embed_i64)upright[down]);
            // a height and a mean of at max one pixel each give at max one pixel
            share[local] += (embed_i32)(((embed_i64)height[down] * mean) >> 16);
        }
    }
    const unsigned long long rows = ((cfg->height - (tile_y * COVERAGE_TILE)) < COVERAGE_TILE)
                                        ? (cfg->height - (tile_y * COVERAGE_TILE))
                                        : COVERAGE_TILE;
    const unsigned long long columns = ((cfg->width - (tile_x * COVERAGE_TILE)) < COVERAGE_TILE)
                                           ? (cfg->width - (tile_x * COVERAGE_TILE))
                                           : COVERAGE_TILE;
    for (unsigned long long row = 0ULL; row < rows; row += 1ULL)
    {
        float *const out =
            cfg->cover + (((tile_y * COVERAGE_TILE) + row) * cfg->width) + (tile_x * COVERAGE_TILE);
        for (unsigned long long column = 0ULL; column < columns; column += 1ULL)
        {
            // a row and a column of a tile are each below 4
            const unsigned int local =
                (unsigned int)((column & 1ULL) | ((row & 1ULL) << 1u) | ((column & 2ULL) << 1u) | ((row & 2ULL) << 2u));
            const embed_i32 sum = share[local] + left_rows[row];
            const embed_i32 magnitude = (sum < 0) ? -sum : sum;
            const float value =
                (float)((magnitude > COVERAGE_ONE) ? COVERAGE_ONE : magnitude) * (1.0f / (float)COVERAGE_ONE);
            out[column] = value;
        }
    }
}

unsigned long long coverage_draw_bytes(const CoverageCfg *args)
{
    const unsigned int points = (args->contours != 0u) ? args->end[args->contours - 1u] : 0u;
    return (unsigned long long)points * COVERAGE_EDGE_BYTES;
}

void coverage_draw(const CoverageCfg *args)
{
    const unsigned int points = (args->contours != 0u) ? args->end[args->contours - 1u] : 0u;
    const unsigned long long bytes = (unsigned long long)points * COVERAGE_EDGE_BYTES;
    // the edges lie in the caller's scratch for this draw alone
    unsigned char *const block =
        ((points != 0u) && (args->scratch_bytes >= bytes)) ? (unsigned char *)args->scratch : NULL;
    if (block == NULL)
    {
        // no outline, or no scratch for its edges: the cover shows nothing of it
        memor.set(
            &(MemoriaCfg){.dst = args->cover, .val = 0, .bytes = args->width * args->height * sizeof(args->cover[0])});
        return;
    }
    // the slopes first, on the block's own alignment, and the 32-bit arrays after them
    CoverageEdges edges = {.slope = (embed_i64 *)(void *)block};
    edges.low_y = (embed_i32 *)(void *)(block + ((unsigned long long)points * sizeof(embed_i64)));
    edges.high_y = edges.low_y + points;
    edges.x_low = edges.high_y + points;
    edges.x_least = edges.x_low + points;
    edges.x_max = edges.x_least + points;
    edges.way = edges.x_max + points;
    edges.count = EMBED_CALL(coverage_edges_read, CoverageCtx, .cfg = args, .edges = &edges);
    const embed_i32 origin_u = EMBED_CALL(coverage_fixed, CoverageCtx, .value = args->origin_x);
    const embed_i32 origin_v = EMBED_CALL(coverage_fixed, CoverageCtx, .value = args->origin_y);
    const unsigned long long tiles_across = (args->width + COVERAGE_TILE - 1u) / COVERAGE_TILE;
    const unsigned long long tiles_down = (args->height + COVERAGE_TILE - 1u) / COVERAGE_TILE;
    for (unsigned long long tile_y = 0ULL; tile_y < tiles_down; tile_y += 1ULL)
    {
        for (unsigned long long tile_x = 0ULL; tile_x < tiles_across; tile_x += 1ULL)
        {
            coverage_tile_draw(args, &edges, tile_x, tile_y, origin_u, origin_v);
        }
    }
}

// The scene's tiles: 4 pixels a side, 16 in a wide tile's row, and a share index that says a command fills whole
#define SCENE_TILE_PIXELS 4
#define SCENE_TILE_SIDE (SCENE_TILE_PIXELS * COVERAGE_ONE)
#define SCENE_WIDE_TILES (COVERAGE_WIDE / COVERAGE_TILE)
#define SCENE_ROWS 0xffffffffu

// The max wide tiles a scene holds: each key's wide tile is the index of a count the sort retains
#define SCENE_WIDES_MAX 65536u

// The Morton place of the pixel at `column`, `row` inside its tile, each from 0 to 3
EMBED_INLINE unsigned int scene_morton(unsigned int column, unsigned int row)
{
    return (column & 1u) | ((row & 1u) << 1u) | ((column & 2u) << 1u) | ((row & 2u) << 2u);
}

// The floor of a 16.16 number over `side`, `side` a positive 16.16 number. The division truncates toward 0, so a
// negative remainder takes one off the quotient, by a compare and no branch
EMBED_INLINE int scene_floor(long long value, long long side)
{
    // the quotient of a place on the target by a tile's side is far inside an int
    return (int)((value / side) - (long long)((value % side) < 0));
}

unsigned long long coverage_scene_bytes(const CoverageSceneCfg *args)
{
    return (unsigned long long)args->capacity * COVERAGE_SCENE_BYTES_A_CAPACITY;
}

int coverage_scene_init(const CoverageSceneCfg *args)
{
    CoverageScene *const scene = args->scene;
    memor.set(&(MemoriaCfg){.dst = scene, .val = 0, .bytes = sizeof(*scene)});
    // a target of more wide tiles than the sort counts is refused
    const unsigned long long wides = (((unsigned long long)args->width + COVERAGE_WIDE - 1u) / COVERAGE_WIDE) *
                                     (((unsigned long long)args->height + COVERAGE_TILE - 1u) / COVERAGE_TILE);
    if (wides > SCENE_WIDES_MAX)
    {
        return 0;
    }
    const unsigned long long capacity = args->capacity;
    // each capacity's arrays, the 64-bit ones first so each lies on its own alignment, in the caller's block
    unsigned char *const block =
        (args->block_bytes >= coverage_scene.bytes(args)) ? (unsigned char *)args->block : NULL;
    if (block == NULL)
    {
        return 0;
    }
    scene->width = args->width;
    scene->height = args->height;
    scene->capacity = args->capacity;
    scene->block = block;
    unsigned char *at = block;
    scene->slope = (long long *)(void *)at;
    at += capacity * sizeof(long long);
    scene->tile = (unsigned long long *)(void *)at;
    at += capacity * sizeof(unsigned long long);
    scene->tile_spare = (unsigned long long *)(void *)at;
    at += capacity * sizeof(unsigned long long);
    scene->key = (unsigned long long *)(void *)at;
    at += capacity * sizeof(unsigned long long);
    scene->key_spare = (unsigned long long *)(void *)at;
    at += capacity * sizeof(unsigned long long);
    scene->low_y = (int *)(void *)at;
    scene->high_y = scene->low_y + capacity;
    scene->x_low = scene->high_y + capacity;
    scene->way = scene->x_low + capacity;
    at += 4ULL * capacity * sizeof(int);
    scene->command_x = (unsigned int *)(void *)at;
    scene->command_tiles = scene->command_x + capacity;
    scene->command_share = scene->command_tiles + capacity;
    scene->command_color = scene->command_share + capacity;
    scene->command_rows = scene->command_color + capacity;
    at += 5ULL * capacity * sizeof(unsigned int);
    scene->share = at;
    return 1;
}

void coverage_scene_free(const CoverageSceneCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->scene, .val = 0, .bytes = sizeof(*args->scene)});
}

void coverage_scene_clear(const CoverageSceneCfg *args)
{
    args->scene->shares = 0u;
    args->scene->commands = 0u;
    args->scene->dropped = 0u;
}

// The keys sorted by their high 32 bits, from low to high, stable: two passes of 16 bits through `spare`
static void scene_sort(unsigned long long *keys, unsigned long long *spare, unsigned int count)
{
    unsigned long long *from = keys;
    unsigned long long *to = spare;
    for (unsigned int shift = 32u; shift < 64u; shift += 16u)
    {
        static unsigned int s_counts[65536];
        memor.set(&(MemoriaCfg){.dst = s_counts, .val = 0, .bytes = sizeof(s_counts)});
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            s_counts[(from[at] >> shift) & 0xffffu] += 1u;
        }
        unsigned int place = 0u;
        for (unsigned int digit = 0u; digit < 65536u; digit += 1u)
        {
            const unsigned int many = s_counts[digit];
            s_counts[digit] = place;
            place += many;
        }
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const unsigned int digit = (unsigned int)((from[at] >> shift) & 0xffffu);
            to[s_counts[digit]] = from[at];
            s_counts[digit] += 1u;
        }
        unsigned long long *const swap = from;
        from = to;
        to = swap;
    }
}

// The command keys copied into `sorted` by their high 32 bits, the wide tile, from low to high and stable: one
// counting pass over the `wides` wide tiles, each below SCENE_WIDES_MAX
static void scene_sort_wides(const unsigned long long *keys, unsigned long long *sorted, unsigned int count,
                             unsigned int wides)
{
    static unsigned int s_counts[SCENE_WIDES_MAX];
    memor.set(&(MemoriaCfg){.dst = s_counts, .val = 0, .bytes = wides * sizeof(s_counts[0])});
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_counts[keys[at] >> 32u] += 1u;
    }
    unsigned int place = 0u;
    for (unsigned int wide = 0u; wide < wides; wide += 1u)
    {
        const unsigned int many = s_counts[wide];
        s_counts[wide] = place;
        place += many;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int wide = (unsigned int)(keys[at] >> 32u);
        sorted[s_counts[wide]] = keys[at];
        s_counts[wide] += 1u;
    }
}

// The command that positions `tiles` tiles from tile column `column` of tile row `row`. Where `share` is SCENE_ROWS each
// pixel row of each tile takes its share from `rows`, a byte a row from the lowest; else each tile takes its shares
// from `share` on. The run splits at each wide tile's side: 1, or 0 where no command has room
static int scene_command(CoverageScene *scene, unsigned int row, unsigned int column, unsigned int tiles,
                         unsigned int share, unsigned int rows, unsigned int color)
{
    const unsigned int wides_across = (scene->width + COVERAGE_WIDE - 1u) / COVERAGE_WIDE;
    while (tiles > 0u)
    {
        const unsigned int wide = column / SCENE_WIDE_TILES;
        const unsigned int room = ((wide + 1u) * SCENE_WIDE_TILES) - column;
        const unsigned int taken = (tiles < room) ? tiles : room;
        const unsigned int last = scene->commands - 1u;
        // a run of shares that goes on from the last command in the same wide tile joins it
        if ((scene->commands > 0u) && (share != SCENE_ROWS) && (scene->command_share[last] != SCENE_ROWS) &&
            ((unsigned int)(scene->key[last] >> 32u) == ((row * wides_across) + wide)) &&
            (scene->command_x[last] + scene->command_tiles[last] == column) &&
            (scene->command_share[last] + (scene->command_tiles[last] * 16u) == share) &&
            (scene->command_color[last] == color))
        {
            scene->command_tiles[last] += taken;
        }
        else
        {
            if (scene->commands >= scene->capacity)
            {
                return 0;
            }
            const unsigned int at = scene->commands;
            scene->key[at] = ((unsigned long long)((row * wides_across) + wide) << 32u) | at;
            scene->command_x[at] = column;
            scene->command_tiles[at] = taken;
            scene->command_share[at] = share;
            scene->command_color[at] = color;
            scene->command_rows[at] = rows;
            scene->commands += 1u;
        }
        column += taken;
        tiles -= taken;
        share = (share == SCENE_ROWS) ? SCENE_ROWS : (share + (taken * 16u));
    }
    return 1;
}

// The edges of the path read into the scene, and the tiles each crosses, a key each: the tile row, the tile column
// plus 1, the edge and whether this is the edge's last tile in its row. A column left of the target is column -1, which
// only carries winding. 1, or 0 where the edges or the tiles pass the capacity
static int scene_tiles_read(const CoverageSceneCfg *args)
{
    CoverageScene *const scene = args->scene;
    const int tiles_across = (int)((scene->width + COVERAGE_TILE - 1u) / COVERAGE_TILE);
    const int tiles_down = (int)((scene->height + COVERAGE_TILE - 1u) / COVERAGE_TILE);
    scene->edges = 0u;
    scene->tiles = 0u;
    unsigned int first = 0u;
    for (unsigned int contour = 0u; contour < args->contours; contour += 1u)
    {
        const unsigned int end = args->end[contour];
        const unsigned int points = end - first;
        for (unsigned int pair = 0u; pair < points; pair += 1u)
        {
            const CoveragePoint from = args->point[first + pair];
            const CoveragePoint to = args->point[first + (((pair + 1u) == points) ? 0u : (pair + 1u))];
            if (from.y == to.y)
            {
                continue;
            }
            if (scene->edges >= scene->capacity)
            {
                return 0;
            }
            const unsigned int edge = scene->edges;
            const int down = to.y > from.y;
            const int low_y = down ? from.y : to.y;
            const int high_y = down ? to.y : from.y;
            const int x_low = down ? from.x : to.x;
            const int x_high = down ? to.x : from.x;
            scene->low_y[edge] = low_y;
            scene->high_y[edge] = high_y;
            scene->x_low[edge] = x_low;
            scene->way[edge] = down ? 1 : -1;
            scene->slope[edge] = (((long long)x_high - x_low) * COVERAGE_ONE) / ((long long)high_y - low_y);
            scene->edges += 1u;
            const int row_first = scene_floor(low_y, SCENE_TILE_SIDE);
            const int row_last = scene_floor((long long)high_y - 1, SCENE_TILE_SIDE);
            for (int row = (row_first < 0) ? 0 : row_first; (row <= row_last) && (row < tiles_down); row += 1)
            {
                const long long row_top = (long long)row * SCENE_TILE_SIDE;
                const long long piece_low = (low_y > row_top) ? low_y : row_top;
                const long long piece_high =
                    (high_y < (row_top + SCENE_TILE_SIDE)) ? high_y : (row_top + SCENE_TILE_SIDE);
                const long long x_from = x_low + (((piece_low - low_y) * scene->slope[edge]) >> 16);
                const long long x_to = x_low + (((piece_high - low_y) * scene->slope[edge]) >> 16);
                int column_from = scene_floor((x_from < x_to) ? x_from : x_to, SCENE_TILE_SIDE);
                int column_to = scene_floor((x_from < x_to) ? x_to : x_from, SCENE_TILE_SIDE);
                if (column_from >= tiles_across)
                {
                    continue;
                }
                column_from = (column_from < -1) ? -1 : column_from;
                column_to = (column_to < -1) ? -1 : ((column_to >= tiles_across) ? (tiles_across - 1) : column_to);
                for (int column = column_from; column <= column_to; column += 1)
                {
                    if (scene->tiles >= scene->capacity)
                    {
                        return 0;
                    }
                    // the row and the column plus 1 are not negative and below 2^16
                    scene->tile[scene->tiles] = ((unsigned long long)(unsigned int)row << 48u) |
                                                ((unsigned long long)(unsigned int)(column + 1) << 32u) |
                                                ((unsigned long long)edge << 1u) |
                                                (unsigned long long)(column == column_to);
                    scene->tiles += 1u;
                }
            }
        }
        first = end;
    }
    return 1;
}

// The 16 shares of the tile at tile row `row` and tile column `column` from the edges whose tiles run from `first` to
// before `last` in the sorted tiles, over `carry`, the winding each of the tile's 4 rows has from the edges left of
// the tile. Each share goes into `shares` at its Morton place, and each edge whose last tile in the row this is adds
// its height to the carry
static void scene_tile_shares(CoverageScene *scene, unsigned int first, unsigned int last, int row, int column,
                              int carry[COVERAGE_TILE], unsigned char *shares)
{
    int sum[COVERAGE_TILE * COVERAGE_TILE];
    for (unsigned int at = 0u; at < COVERAGE_TILE * COVERAGE_TILE; at += 1u)
    {
        sum[at] = carry[at / COVERAGE_TILE];
    }
    int carried[COVERAGE_TILE] = {0};
    for (unsigned int at = first; at < last; at += 1u)
    {
        const unsigned int edge = (unsigned int)((scene->tile[at] & 0xffffffffULL) >> 1u);
        const int ends_here = (int)(scene->tile[at] & 1ULL);
        const int low_y = scene->low_y[edge];
        const int high_y = scene->high_y[edge];
        for (int pixel_row = 0; pixel_row < SCENE_TILE_PIXELS; pixel_row += 1)
        {
            const int row_top = ((row * SCENE_TILE_PIXELS) + pixel_row) * COVERAGE_ONE;
            const int piece_low =
                EMBED_CALL(coverage_clamp, CoverageCtx, .number = row_top, .least = low_y, .max = high_y);
            const int piece_high = EMBED_CALL(coverage_clamp, CoverageCtx, .number = row_top + COVERAGE_ONE,
                                              .least = low_y, .max = high_y);
            const int height = scene->way[edge] * (piece_high - piece_low);
            if (height == 0)
            {
                continue;
            }
            const long long x_from = scene->x_low[edge] + (((long long)(piece_low - low_y) * scene->slope[edge]) >> 16);
            const long long x_to = scene->x_low[edge] + (((long long)(piece_high - low_y) * scene->slope[edge]) >> 16);
            const long long width = x_to - x_from;
            const long long reciprocal = ((long long)1 << 32) / (width + (long long)(width == 0));
            for (int pixel_column = 0; pixel_column < SCENE_TILE_PIXELS; pixel_column += 1)
            {
                // the pixel's right side, where H reads the mean of clamp(u - x, 0, 1) over the piece
                const long long u = ((long long)(column * SCENE_TILE_PIXELS) + pixel_column + 1) * COVERAGE_ONE;
                const long long near = EMBED_CALL(coverage_ramp_area, CoverageCtx, .span = u - x_from);
                const long long far = EMBED_CALL(coverage_ramp_area, CoverageCtx, .span = u - x_to);
                const long long sloped = ((near - far) * reciprocal) >> 32;
                const long long straight =
                    ((u - x_from) < 0) ? 0 : (((u - x_from) > COVERAGE_ONE) ? COVERAGE_ONE : (u - x_from));
                const long long mean = (width == 0) ? straight : sloped;
                // a height and a mean of at max one pixel each give at max one pixel
                sum[(pixel_row * SCENE_TILE_PIXELS) + pixel_column] += (int)(((long long)height * mean) >> 16);
            }
            carried[pixel_row] += ends_here ? height : 0;
        }
    }
    for (unsigned int at = 0u; at < COVERAGE_TILE * COVERAGE_TILE; at += 1u)
    {
        const int magnitude = (sum[at] < 0) ? -sum[at] : sum[at];
        const int whole = (magnitude > COVERAGE_ONE) ? COVERAGE_ONE : magnitude;
        // a share of one pixel in 16.16 by 255, rounded, is a byte
        shares[scene_morton(at % COVERAGE_TILE, at / COVERAGE_TILE)] =
            (unsigned char)(((whole * 255) + (COVERAGE_ONE / 2)) >> 16);
    }
    for (unsigned int pixel_row = 0u; pixel_row < COVERAGE_TILE; pixel_row += 1u)
    {
        carry[pixel_row] += carried[pixel_row];
    }
}

void coverage_scene_fill(const CoverageSceneCfg *args)
{
    CoverageScene *const scene = args->scene;
    const unsigned int shares_before = scene->shares;
    const unsigned int commands_before = scene->commands;
    int positioned = scene_tiles_read(args);
    if (positioned)
    {
        scene_sort(scene->tile, scene->tile_spare, scene->tiles);
    }
    unsigned int at = 0u;
    while (positioned && (at < scene->tiles))
    {
        // one tile row: the winding each pixel row carries from the left, and the last column positioned
        const unsigned int row = (unsigned int)(scene->tile[at] >> 48u);
        int carry[COVERAGE_TILE] = {0};
        int positioned_column = -1;
        while (positioned && (at < scene->tiles) && ((unsigned int)(scene->tile[at] >> 48u) == row))
        {
            const unsigned int key = (unsigned int)(scene->tile[at] >> 32u);
            unsigned int next = at;
            while ((next < scene->tiles) && ((unsigned int)(scene->tile[next] >> 32u) == key))
            {
                next += 1u;
            }
            const int column = (int)(key & 0xffffu) - 1;
            // The gap from the last column positioned to this one holds no edge, so each of its pixel rows retains the winding
            // it carries in. A horizontal edge makes no tile, so the 4 rows can differ: each row takes its own share
            unsigned int rows = 0u;
            for (unsigned int pixel_row = 0u; pixel_row < COVERAGE_TILE; pixel_row += 1u)
            {
                const int magnitude = (carry[pixel_row] < 0) ? -carry[pixel_row] : carry[pixel_row];
                const int whole = (magnitude > COVERAGE_ONE) ? COVERAGE_ONE : magnitude;
                // a share of one pixel in 16.16 by 255, rounded, is a byte
                rows |= (unsigned int)(((whole * 255) + (COVERAGE_ONE / 2)) >> 16) << (8u * pixel_row);
            }
            const int from = (positioned_column + 1 < 0) ? 0 : (positioned_column + 1);
            if ((rows != 0u) && (column > from))
            {
                positioned = scene_command(scene, row, (unsigned int)from, (unsigned int)(column - from), SCENE_ROWS, rows,
                                     args->color);
            }
            if (positioned && (column >= 0) &&
                ((unsigned long long)scene->shares + 16u > (unsigned long long)scene->capacity * 16u))
            {
                positioned = 0;
            }
            if (positioned)
            {
                unsigned char scratch[COVERAGE_TILE * COVERAGE_TILE];
                unsigned char *const shares = (column >= 0) ? (scene->share + scene->shares) : scratch;
                scene_tile_shares(scene, at, next, (int)row, column, carry, shares);
                if (column >= 0)
                {
                    positioned = scene_command(scene, row, (unsigned int)column, 1u, scene->shares, 0u, args->color);
                    scene->shares += 16u;
                }
            }
            positioned_column = column;
            at = next;
        }
    }
    if (!positioned)
    {
        // a fill past a capacity is dropped whole
        scene->shares = shares_before;
        scene->commands = commands_before;
        scene->dropped += 1u;
    }
}

// `color` positioned over `ground` by `share` of 255 of its own alpha, left opaque
EMBED_INLINE unsigned int scene_blend(unsigned int ground, unsigned int color, unsigned int share)
{
    // a share by an alpha, each of 255, over 255 is a share of 255
    const unsigned int alpha = ((share * (color >> 24u)) + 127u) / 255u;
    const unsigned int rest = 255u - alpha;
    const unsigned int red = ((((color >> 16u) & 0xffu) * alpha) + (((ground >> 16u) & 0xffu) * rest) + 127u) / 255u;
    const unsigned int green = ((((color >> 8u) & 0xffu) * alpha) + (((ground >> 8u) & 0xffu) * rest) + 127u) / 255u;
    const unsigned int blue = (((color & 0xffu) * alpha) + ((ground & 0xffu) * rest) + 127u) / 255u;
    return 0xff000000u | (red << 16u) | (green << 8u) | blue;
}

// The wide tile's pixels from `left`, `top` to before `right`, `foot` read from the target into `scratch` in Morton
// order, the wide tile's top left at `wide_x`, `wide_y`
static void scene_wide_load(const CoverageSceneCfg *args, unsigned int *scratch, int wide_x, int wide_y, int left,
                            int top, int right, int foot)
{
    for (int y = top; y < foot; y += 1)
    {
        // the pixels lie on the target and in this wide tile: each place is not negative
        const unsigned int row_place = scene_morton(0u, (unsigned int)(y - wide_y));
        const unsigned int *const from = &args->target[((unsigned int)y * args->pitch)];
        int x = left;
        while (x < right)
        {
            const unsigned int local = (unsigned int)(x - wide_x);
            unsigned int *const to = &scratch[((local / COVERAGE_TILE) * 16u) + row_place];
            // a whole tile's row: its 4 pixels go to Morton places 0, 1, 4 and 5 past the row's place
            if (((local % COVERAGE_TILE) == 0u) && ((x + (int)COVERAGE_TILE) <= right))
            {
                to[0] = from[x];
                to[1] = from[x + 1];
                to[4] = from[x + 2];
                to[5] = from[x + 3];
                x += (int)COVERAGE_TILE;
                continue;
            }
            to[scene_morton(local % COVERAGE_TILE, 0u)] = from[x];
            x += 1;
        }
    }
}

// The wide tile's pixels from `left`, `top` to before `right`, `foot` written from `scratch` to the target
static void scene_wide_store(const CoverageSceneCfg *args, const unsigned int *scratch, int wide_x, int wide_y,
                             int left, int top, int right, int foot)
{
    for (int y = top; y < foot; y += 1)
    {
        const unsigned int row_place = scene_morton(0u, (unsigned int)(y - wide_y));
        unsigned int *const to = &args->target[((unsigned int)y * args->pitch)];
        int x = left;
        while (x < right)
        {
            const unsigned int local = (unsigned int)(x - wide_x);
            const unsigned int *const from = &scratch[((local / COVERAGE_TILE) * 16u) + row_place];
            if (((local % COVERAGE_TILE) == 0u) && ((x + (int)COVERAGE_TILE) <= right))
            {
                to[x] = from[0];
                to[x + 1] = from[1];
                to[x + 2] = from[4];
                to[x + 3] = from[5];
                x += (int)COVERAGE_TILE;
                continue;
            }
            to[x] = from[scene_morton(local % COVERAGE_TILE, 0u)];
            x += 1;
        }
    }
}

// The commands from `first` to before `last` in `sorted` positioned in order on the wide tile's pixels in `scratch`
static void scene_wide_commands(const CoverageScene *scene, const unsigned long long *sorted, unsigned int first,
                                unsigned int last, unsigned int *scratch, int wide_x, int wide_y, int left, int top,
                                int right, int foot)
{
    for (unsigned int command_at = first; command_at < last; command_at += 1u)
    {
        const unsigned int command = (unsigned int)(sorted[command_at] & 0xffffffffULL);
        const unsigned int share = scene->command_share[command];
        const unsigned int color = scene->command_color[command];
        for (unsigned int tile = 0u; tile < scene->command_tiles[command]; tile += 1u)
        {
            const unsigned int local_tile = (scene->command_x[command] + tile) % SCENE_WIDE_TILES;
            unsigned int *const pixels = &scratch[local_tile * 16u];
            const unsigned char *const shares = (share == SCENE_ROWS) ? NULL : (scene->share + share + (tile * 16u));
            const unsigned int rows = scene->command_rows[command];
            for (unsigned int pixel = 0u; pixel < 16u; pixel += 1u)
            {
                // a pixel's row inside its tile, from its Morton place
                const unsigned int pixel_row = ((pixel >> 1u) & 1u) | ((pixel >> 2u) & 2u);
                const unsigned int pixel_share =
                    (shares == NULL) ? ((rows >> (8u * pixel_row)) & 0xffu) : shares[pixel];
                pixels[pixel] = scene_blend(pixels[pixel], color, pixel_share);
            }
        }
    }
}

void coverage_scene_render(const CoverageSceneCfg *args)
{
    CoverageScene *const scene = args->scene;
    const unsigned int wides_across = (scene->width + COVERAGE_WIDE - 1u) / COVERAGE_WIDE;
    const unsigned int wides_down = (scene->height + COVERAGE_TILE - 1u) / COVERAGE_TILE;
    // the commands' keys sorted into the spare keys by wide tile, the keys themselves retained for the next render
    unsigned long long *const sorted = scene->key_spare;
    scene_sort_wides(scene->key, sorted, scene->commands, wides_across * wides_down);
    // the clip cut to the target; the target is far inside what an int holds
    const int clip_left = (args->clip_x > 0) ? args->clip_x : 0;
    const int clip_top = (args->clip_y > 0) ? args->clip_y : 0;
    const int clip_right_end = args->clip_x + args->clip_width;
    const int clip_foot_end = args->clip_y + args->clip_height;
    const int clip_right = (clip_right_end < (int)scene->width) ? clip_right_end : (int)scene->width;
    const int clip_foot = (clip_foot_end < (int)scene->height) ? clip_foot_end : (int)scene->height;
    if ((clip_left >= clip_right) || (clip_top >= clip_foot))
    {
        return;
    }
    // each wide tile with a command starts from the target, and a wide tile without one is left as it is
    const unsigned int row_first = (unsigned int)clip_top / COVERAGE_TILE;
    const unsigned int row_last = (unsigned int)(clip_foot - 1) / COVERAGE_TILE;
    const unsigned int column_first = (unsigned int)clip_left / COVERAGE_WIDE;
    const unsigned int column_last = (unsigned int)(clip_right - 1) / COVERAGE_WIDE;
    unsigned int at = 0u;
    for (unsigned int wide_row = row_first; wide_row <= row_last; wide_row += 1u)
    {
        for (unsigned int wide_column = column_first; wide_column <= column_last; wide_column += 1u)
        {
            const unsigned int wide = (wide_row * wides_across) + wide_column;
            // the commands of wide tiles before this one, outside the clip, are passed over
            while ((at < scene->commands) && ((unsigned int)(sorted[at] >> 32u) < wide))
            {
                at += 1u;
            }
            unsigned int next = at;
            while ((next < scene->commands) && ((unsigned int)(sorted[next] >> 32u) == wide))
            {
                next += 1u;
            }
            if (next == at)
            {
                continue;
            }
            const int wide_x = (int)(wide_column * COVERAGE_WIDE);
            const int wide_y = (int)(wide_row * COVERAGE_TILE);
            const int left = (wide_x > clip_left) ? wide_x : clip_left;
            const int top = (wide_y > clip_top) ? wide_y : clip_top;
            const int right =
                ((wide_x + (int)COVERAGE_WIDE) < clip_right) ? (wide_x + (int)COVERAGE_WIDE) : clip_right;
            const int foot =
                ((wide_y + (int)COVERAGE_TILE) < clip_foot) ? (wide_y + (int)COVERAGE_TILE) : clip_foot;
            EMBED_ALIGN(64) unsigned int scratch[COVERAGE_WIDE * COVERAGE_TILE];
            scene_wide_load(args, scratch, wide_x, wide_y, left, top, right, foot);
            scene_wide_commands(scene, sorted, at, next, scratch, wide_x, wide_y, left, top, right, foot);
            scene_wide_store(args, scratch, wide_x, wide_y, left, top, right, foot);
            at = next;
        }
    }
}
