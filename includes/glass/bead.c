// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bead.c: the bead of water baked and blitted. The bake runs once for a button. It positions a dome over the button,
// reads its normals, refracts a ray down through the water, and lights the dome from each of the caller's lights at
// the button's place: a glint where a light mirrors, a caustic crescent on the edge away from it, a dark rim where
// the dome turns away, and a shadow cast away from it on the glass. A pressed frame sinks and flattens the dome, so
// the glint slides, the caustic draws in and the shadow tightens, as a real button moves under a light. The blit
// does integer work only. It copies the patch under the cell, frosts the copy with box passes of four, reads each
// pixel through its offsets, mixes the sharp and the frosted reads by the plane's share, and positions the light over in
// sixteen-bit lanes of a sixty-four-bit word
#include "bead.h"

#include "../arith/arith.h"
#include "memoria_operor/memoria_operor.h"

// the lanes of a spread pixel: blue at bit 0, red at 16, green at 32 and alpha at 48, a byte each
#define BEAD_LANES 0x00ff00ff00ff00ffull
#define BEAD_LANE_CARRY 0x0001000100010001ull
// the rim's power, and the share of the rim that is all round, the rest on the side away from the light
#define BEAD_RIM_POWER 2.0
#define BEAD_RIM_ROUND 0.4
// the caustic's band center at rest and its shift with the press, each a share of the way in, and its width
#define BEAD_CAUSTIC_CENTER 0.18
#define BEAD_CAUSTIC_SHIFT 0.12
#define BEAD_CAUSTIC_WIDTH 0.12
// the shadow's spread as a share of the side and the share the press takes off it; the share of the dome's height
// above the glass the shadow is cast from; and the share the press takes off the shadow's density
#define BEAD_SHADOW_SPREAD 0.085
#define BEAD_SHADOW_SPREAD_PRESSED 0.35
#define BEAD_SHADOW_LIFT 0.65
#define BEAD_SHADOW_FADE_PRESSED 0.2
// The reflection of a light on the dome: broad and soft, as the sky reflects on a drop; its sharpness at rest, the
// share the press takes off it, its level, below the whole so no plateau saturates, the share the press takes off
// it, and the share of it that is at the dome's top, the rest growing toward the rim as water reflects more at a
// grazing angle
#define BEAD_GLINT_SHARP 28.0
#define BEAD_GLINT_SOFTEN 0.45
#define BEAD_GLINT_LEVEL 0.6
#define BEAD_GLINT_DIM 0.25
#define BEAD_GLINT_FLAT 0.35
// the max pixels of the shadow's kernel to each side, and a column of the cell for the vertical pass
#define BEAD_KERNEL_MAX 24u
#define BEAD_CELL_MAX 256u

typedef struct
{
    const GuiBeadAtlas *atlas_read;
    GuiBeadAtlas *atlas;
    unsigned int tool;
    unsigned int frame;
    unsigned int row;
    unsigned int patch;
    unsigned int plane;
    double value;
    double low;
    double high;
    double sigma;
    double across;
    double down;
    double share;
    GuiColor had;
    GuiColor color;
    const GuiColor *from;
    GuiColor *to;
    unsigned int lo;
    unsigned int hi;
    unsigned int step;
    unsigned int count;
    unsigned int lines;
    unsigned int line_step;
    unsigned int width;
    unsigned int height;
    unsigned int shape;
    unsigned int radius;
} BeadCtx;

// a way: three numbers
typedef struct
{
    double x;
    double y;
    double z;
} BeadWay;

// the number of `value` held between `low` and `high`
EMBED_INLINE double bead_held(const BeadCtx *args)
{
    return (args->value < args->low) ? args->low : ((args->value > args->high) ? args->high : args->value);
}

// `way` made a unit long; a way of no length stays
EMBED_INLINE BeadWay bead_unit(BeadWay way)
{
    const double length = arith_root((way.x * way.x) + (way.y * way.y) + (way.z * way.z));
    if (length <= 0.0)
    {
        return way;
    }
    return (BeadWay){way.x / length, way.y / length, way.z / length};
}

// the rows of a patch: the cell with the reach round it
EMBED_INLINE unsigned int bead_patch_rows(const BeadCtx *args)
{
    return args->atlas_read->cell_height + (2u * args->atlas_read->reach);
}

// the first row of plane A of `frame` of the button `tool`; plane B follows it a cell of rows on
EMBED_INLINE unsigned int bead_plane_row(const BeadCtx *args)
{
    return ((args->tool * GUI_BEAD_FRAMES) + args->frame) * 2u * args->atlas_read->cell_height;
}

// the first row of patch `patch`: the patches follow the frames
EMBED_INLINE unsigned int bead_patch_row(const BeadCtx *args)
{
    return (args->atlas_read->tools * GUI_BEAD_FRAMES * 2u * args->atlas_read->cell_height) +
           (args->patch * EMBED_CALL(bead_patch_rows, BeadCtx, .atlas_read = args->atlas_read));
}

// the first row of work plane `plane`: the work follows the patches
EMBED_INLINE unsigned int bead_work_row(const BeadCtx *args)
{
    return EMBED_CALL(bead_patch_row, BeadCtx, .atlas_read = args->atlas_read, .patch = 3u) +
           (args->plane * args->atlas_read->cell_height);
}

// the button `tool`'s own width and height: its cell's where it names them, else the atlas's largest
EMBED_INLINE unsigned int bead_tool_width(const BeadCtx *args)
{
    const int width = args->atlas_read->origin[args->tool].width;
    // a width named is positive
    return (width > 0) ? (unsigned int)width : args->atlas_read->width;
}

EMBED_INLINE unsigned int bead_tool_height(const BeadCtx *args)
{
    const int height = args->atlas_read->origin[args->tool].height;
    // a height named is positive
    return (height > 0) ? (unsigned int)height : args->atlas_read->height;
}

EMBED_INLINE GuiColor *bead_row(const BeadCtx *args)
{
    return args->atlas->cells.pixels + ((unsigned long long)args->row * args->atlas->stride);
}

EMBED_INLINE const GuiColor *bead_row_read(const BeadCtx *args)
{
    return args->atlas_read->cells.pixels + ((unsigned long long)args->row * args->atlas_read->stride);
}

// a pixel spread to its lanes, and the lanes packed to a pixel
EMBED_INLINE unsigned long long bead_spread(GuiColor pixel)
{
    return (unsigned long long)(pixel & 0x00ff00ffu) | ((unsigned long long)(pixel & 0xff00ff00u) << 24u);
}

EMBED_INLINE GuiColor bead_pack(unsigned long long lanes)
{
    // the lanes hold a byte each, which the pixel's bytes take whole
    return (GuiColor)(lanes & 0x00ff00ffull) | (GuiColor)((lanes >> 24u) & 0xff00ff00ull);
}

// a light's `color` at `share` of the color's alpha positioned over the premultiplied `had`, premultiplied
EMBED_INLINE GuiColor bead_light_over(const BeadCtx *args)
{
    const double alpha = args->share * (double)GUI_ALPHA_OF(args->color) / 255.0;
    if (alpha <= 0.0)
    {
        return args->had;
    }
    const GuiColor had = args->had;
    const double rest = 1.0 - alpha;
    // a premultiplied channel over another, rounded, is a byte
    const unsigned int red = (unsigned int)((alpha * (double)GUI_RED_OF(args->color)) + (rest * (double)GUI_RED_OF(had)) + 0.5);
    const unsigned int green = (unsigned int)((alpha * (double)GUI_GREEN_OF(args->color)) + (rest * (double)GUI_GREEN_OF(had)) + 0.5);
    const unsigned int blue = (unsigned int)((alpha * (double)GUI_BLUE_OF(args->color)) + (rest * (double)GUI_BLUE_OF(had)) + 0.5);
    const unsigned int whole = (unsigned int)((alpha * 255.0) + (rest * (double)GUI_ALPHA_OF(had)) + 0.5);
    return GUI_RGBA(red, green, blue, whole);
}

// The press share of `frame`, GUI_BEAD_LOW at the first frame and 1.0 at the last
EMBED_INLINE double bead_frame_share(const BeadCtx *args)
{
    return GUI_BEAD_LOW + ((1.0 - GUI_BEAD_LOW) * (double)args->frame / (double)(GUI_BEAD_FRAMES - 1u));
}

// The signed distance of the point `across`, `down` from the bead's center to the bead's edge, the bead `width` by
// `height` and `share` pressed: negative inside, in pixels. A dome's edge is a superellipse of GUI_BEAD_SHAPE_POWER
// on the button's half width and half height, round at every point, its distance in the smaller half side. A tab's
// edge is the tab's box, its top corners rounded by `radius`, no more than a half side, its foot and lower corners
// square. The press squashes the bead across and down
EMBED_INLINE double bead_distance(const BeadCtx *args)
{
    const double half_width = (double)args->width / 2.0;
    const double half_height = (double)args->height / 2.0;
    const double across = arith_abs(args->across / (1.0 + (GUI_BEAD_SQUISH * args->share)));
    const double down = args->down / (1.0 - (GUI_BEAD_SQUISH * args->share));
    if (args->shape == GUI_BEAD_SHAPE_TAB)
    {
        const double half = (half_width < half_height) ? half_width : half_height;
        const double radius = ((double)args->radius < half) ? (double)args->radius : half;
        if ((down < -(half_height - radius)) && (across > half_width - radius))
        {
            const double corner_x = across - (half_width - radius);
            const double corner_y = -down - (half_height - radius);
            return arith_root((corner_x * corner_x) + (corner_y * corner_y)) - radius;
        }
        const double edge_x = across - half_width;
        const double edge_y = arith_abs(down) - half_height;
        const double outside_x = arith_max(edge_x, 0.0);
        const double outside_y = arith_max(edge_y, 0.0);
        const double nearer = (edge_x > edge_y) ? edge_x : edge_y;
        return arith_root((outside_x * outside_x) + (outside_y * outside_y)) + ((nearer < 0.0) ? nearer : 0.0);
    }
    const double half = (half_width < half_height) ? half_width : half_height;
    const double reach = arith_pow(arith_pow(across / half_width, GUI_BEAD_SHAPE_POWER) +
                                       arith_pow(arith_abs(down) / half_height, GUI_BEAD_SHAPE_POWER),
                                   1.0 / GUI_BEAD_SHAPE_POWER);
    return (reach - 1.0) * half;
}

// The dome's height at `value` the signed distance of a bead `width` by `height`: a circular profile over the
// smaller side, nothing at the edge and `high`, the dome's top, at the center
EMBED_INLINE double bead_height(const BeadCtx *args)
{
    const double side = (double)((args->width < args->height) ? args->width : args->height);
    const double half = side / 2.0;
    const double in = EMBED_CALL(bead_held, BeadCtx, .value = -args->value / half, .low = 0.0, .high = 1.0);
    return args->high * arith_root(1.0 - ((1.0 - in) * (1.0 - in)));
}

// The work plane `plane` of the bake read as floats, one a pixel on the planes' stride
EMBED_INLINE float *bead_work(const BeadCtx *args)
{
    return (float *)(void *)EMBED_CALL(bead_row, BeadCtx, .atlas = args->atlas,
                                       .row = EMBED_CALL(bead_work_row, BeadCtx, .atlas_read = args->atlas,
                                                         .plane = args->plane));
}

// The shadow: the cover plane blurred by a kernel of `sigma` into the blur plane, across then down
EMBED_INLINE void bead_shadow_blur(const BeadCtx *args)
{
    GuiBeadAtlas *const atlas = args->atlas;
    const unsigned int cell_width = atlas->cell_width;
    const unsigned int cell_height = atlas->cell_height;
    const unsigned int stride = atlas->stride;
    const float *const cover = EMBED_CALL(bead_work, BeadCtx, .atlas = atlas, .plane = 1u);
    float *const blur = EMBED_CALL(bead_work, BeadCtx, .atlas = atlas, .plane = 2u);
    // the kernel reaches three sigmas each way, which an unsigned int holds
    unsigned int reach = (unsigned int)arith_ceil(3.0 * args->sigma);
    reach = (reach > BEAD_KERNEL_MAX) ? BEAD_KERNEL_MAX : reach;
    double weight[(2u * BEAD_KERNEL_MAX) + 1u];
    double summed = 0.0;
    for (unsigned int at = 0u; at <= 2u * reach; at += 1u)
    {
        const double offset = (double)at - (double)reach;
        weight[at] = arith_exp(-(offset * offset) / (2.0 * args->sigma * args->sigma));
        summed += weight[at];
    }
    for (unsigned int at = 0u; at <= 2u * reach; at += 1u)
    {
        weight[at] /= summed;
    }
    // across: each pixel the weighted sum of its row's cover, the row's ends located past them
    for (unsigned int y = 0u; y < cell_height; y += 1u)
    {
        for (unsigned int x = 0u; x < cell_width; x += 1u)
        {
            double total = 0.0;
            for (unsigned int at = 0u; at <= 2u * reach; at += 1u)
            {
                // the offset's sign is retained in an int; the cell's sides are far inside an int
                const int column = (int)x + (int)at - (int)reach;
                const int held = (column < 0) ? 0 : ((column >= (int)cell_width) ? (int)cell_width - 1 : column);
                total += weight[at] * (double)cover[(y * stride) + (unsigned int)held];
            }
            blur[(y * stride) + x] = (float)total;
        }
    }
    // down: each column taken out, summed and put back
    float column[BEAD_CELL_MAX];
    for (unsigned int x = 0u; x < cell_width; x += 1u)
    {
        for (unsigned int y = 0u; y < cell_height; y += 1u)
        {
            column[y] = blur[(y * stride) + x];
        }
        for (unsigned int y = 0u; y < cell_height; y += 1u)
        {
            double total = 0.0;
            for (unsigned int at = 0u; at <= 2u * reach; at += 1u)
            {
                const int row = (int)y + (int)at - (int)reach;
                const int held = (row < 0) ? 0 : ((row >= (int)cell_height) ? (int)cell_height - 1 : row);
                total += weight[at] * (double)column[(unsigned int)held];
            }
            blur[(y * stride) + x] = (float)total;
        }
    }
}

// The dome's height at the pixel `across`, `down` of the height plane, 0 past the cell
EMBED_INLINE double bead_height_at(const BeadCtx *args)
{
    GuiBeadAtlas *const atlas = args->atlas;
    const int x = (int)args->across;
    const int y = (int)args->down;
    if ((x < 0) || (y < 0) || (x >= (int)atlas->cell_width) || (y >= (int)atlas->cell_height))
    {
        return 0.0;
    }
    const float *const height = EMBED_CALL(bead_work, BeadCtx, .atlas = atlas, .plane = 0u);
    // the pixel lies in the cell: its coordinates are not negative
    return (double)height[((unsigned int)y * atlas->stride) + (unsigned int)x];
}

// The blur plane read at `across`, `down` whole pixels, 0 past the cell
EMBED_INLINE double bead_blur_at(const BeadCtx *args)
{
    GuiBeadAtlas *const atlas = args->atlas;
    const int x = (int)args->across;
    const int y = (int)args->down;
    if ((x < 0) || (y < 0) || (x >= (int)atlas->cell_width) || (y >= (int)atlas->cell_height))
    {
        return 0.0;
    }
    const float *const blur = EMBED_CALL(bead_work, BeadCtx, .atlas = atlas, .plane = 2u);
    // the pixel lies in the cell: its coordinates are not negative
    return (double)blur[((unsigned int)y * atlas->stride) + (unsigned int)x];
}

// One frame of one button baked: its heights and cover positioned, its shadow blurred, then every pixel's two planes. The
// bead is the button's own width and height, positioned from the cell's top left corner inside the margin, its dome and its
// press on the smaller of the two
EMBED_INLINE void bead_frame_bake(const BeadCtx *args)
{
    GuiBeadAtlas *const atlas = args->atlas;
    const unsigned int cell_width = atlas->cell_width;
    const unsigned int cell_height = atlas->cell_height;
    const unsigned int stride = atlas->stride;
    const double share = EMBED_CALL(bead_frame_share, BeadCtx, .frame = args->frame);
    const unsigned int width = EMBED_CALL(bead_tool_width, BeadCtx, .atlas_read = atlas, .tool = args->tool);
    const unsigned int height_of = EMBED_CALL(bead_tool_height, BeadCtx, .atlas_read = atlas, .tool = args->tool);
    const double center_x = (double)atlas->pad + ((double)width / 2.0);
    const double center_y = (double)atlas->pad + ((double)height_of / 2.0);
    const double side = (double)((width < height_of) ? width : height_of);
    const GuiRect origin = atlas->origin[args->tool];
    // the dome's top with the press's flattening, and how far the press sinks the bead into the glass
    const double top = side * GUI_BEAD_HEIGHT * (1.0 - (GUI_BEAD_FLATTEN * share));
    const double sink = side * GUI_BEAD_SINK * share;
    float *const height = EMBED_CALL(bead_work, BeadCtx, .atlas = atlas, .plane = 0u);
    float *const cover = EMBED_CALL(bead_work, BeadCtx, .atlas = atlas, .plane = 1u);
    for (unsigned int y = 0u; y < cell_height; y += 1u)
    {
        for (unsigned int x = 0u; x < cell_width; x += 1u)
        {
            const double across = ((double)x + 0.5) - center_x;
            const double down = ((double)y + 0.5) - center_y;
            const double distance =
                EMBED_CALL(bead_distance, BeadCtx, .width = width, .height = height_of, .shape = atlas->shape,
                           .radius = atlas->radius, .across = across, .down = down, .share = share);
            cover[(y * stride) + x] =
                (float)EMBED_CALL(bead_held, BeadCtx, .value = 0.5 - distance, .low = 0.0, .high = 1.0);
            height[(y * stride) + x] = (distance < 0.5)
                                           ? (float)EMBED_CALL(bead_height, BeadCtx, .width = width, .height = height_of,
                                                               .value = distance, .high = top)
                                           : 0.0f;
        }
    }
    const double sigma = side * BEAD_SHADOW_SPREAD * (1.0 - (BEAD_SHADOW_SPREAD_PRESSED * share));
    EMBED_CALL(bead_shadow_blur, BeadCtx, .atlas = atlas, .sigma = (sigma < 0.5) ? 0.5 : sigma);
    // each light's way from the bead's center, and where it casts the bead's shadow: away from it, as far as the
    // bead is above the glass lets it
    const double lift = arith_max(top - sink, 0.0) * BEAD_SHADOW_LIFT;
    double drop_x[GUI_LIGHTS_MAX];
    double drop_y[GUI_LIGHTS_MAX];
    double weight[GUI_LIGHTS_MAX];
    double strengths = 0.0;
    for (unsigned int at = 0u; at < atlas->lights; at += 1u)
    {
        strengths += atlas->light[at].strength;
    }
    for (unsigned int at = 0u; at < atlas->lights; at += 1u)
    {
        const GuiLight light = atlas->light[at];
        const BeadWay way = bead_unit((BeadWay){light.x - ((double)origin.x + center_x),
                                                light.y - ((double)origin.y + center_y), light.z});
        const double over = (way.z > 0.05) ? way.z : 0.05;
        drop_x[at] = -way.x / over * lift;
        drop_y[at] = -way.y / over * lift;
        weight[at] = (strengths > 0.0) ? light.strength / strengths : 0.0;
    }
    const double shadow_strength = 1.0 - (BEAD_SHADOW_FADE_PRESSED * share);
    const double eta = 1.0 / GUI_BEAD_INDEX;
    const double sharp = BEAD_GLINT_SHARP * (1.0 - (BEAD_GLINT_SOFTEN * share));
    const double glint_strength = 1.0 - (BEAD_GLINT_DIM * share);
    const double caustic_center = BEAD_CAUSTIC_CENTER + (BEAD_CAUSTIC_SHIFT * share);
    const double reach = (double)atlas->reach;
    const double pad = (double)atlas->pad;
    GuiColor *const plane_a = EMBED_CALL(
        bead_row, BeadCtx, .atlas = atlas,
        .row = EMBED_CALL(bead_plane_row, BeadCtx, .atlas_read = atlas, .tool = args->tool, .frame = args->frame));
    GuiColor *const plane_b = plane_a + ((unsigned long long)cell_height * stride);
    for (unsigned int y = 0u; y < cell_height; y += 1u)
    {
        for (unsigned int x = 0u; x < cell_width; x += 1u)
        {
            const double covered = (double)cover[(y * stride) + x];
            const double across = ((double)x + 0.5) - center_x;
            const double down = ((double)y + 0.5) - center_y;
            const double distance =
                EMBED_CALL(bead_distance, BeadCtx, .width = width, .height = height_of, .shape = atlas->shape,
                           .radius = atlas->radius, .across = across, .down = down, .share = share);
            // the normal from the heights either side, the height its length up
            const double slope_x = (EMBED_CALL(bead_height_at, BeadCtx, .atlas = atlas, .across = (double)x + 1.0, .down = (double)y) -
                                    EMBED_CALL(bead_height_at, BeadCtx, .atlas = atlas, .across = (double)x - 1.0, .down = (double)y)) / 2.0;
            const double slope_y = (EMBED_CALL(bead_height_at, BeadCtx, .atlas = atlas, .across = (double)x, .down = (double)y + 1.0) -
                                    EMBED_CALL(bead_height_at, BeadCtx, .atlas = atlas, .across = (double)x, .down = (double)y - 1.0)) / 2.0;
            const BeadWay normal = bead_unit((BeadWay){-slope_x, -slope_y, 1.0});
            // the ray down through the water: Snell's law, the ray bent toward the normal, read where it meets the
            // glass
            int offset_x = 0;
            int offset_y = 0;
            if (covered > 0.0)
            {
                const double cosine = normal.z;
                const double bent = 1.0 - (eta * eta * (1.0 - (cosine * cosine)));
                const double factor = (eta * cosine) - arith_root((bent > 0.0) ? bent : 0.0);
                const double ray_z = -eta + (factor * normal.z);
                const double travel = (double)height[(y * stride) + x] / ((ray_z < -0.01) ? -ray_z : 0.01);
                const double read_x = EMBED_CALL(bead_held, BeadCtx, .value = factor * normal.x * travel, .low = -reach, .high = reach);
                const double read_y = EMBED_CALL(bead_held, BeadCtx, .value = factor * normal.y * travel, .low = -reach, .high = reach);
                // an offset within the reach is a small int
                offset_x = (int)arith_round_whole(read_x);
                offset_y = (int)arith_round_whole(read_y);
            }
            // the light from each light: the glint where the dome mirrors it to the eye, the caustic on the edge
            // away from it, and the shadow it casts; the point lit is at the dome's height less the sink
            const double lit_z = (double)height[(y * stride) + x] - sink;
            const double in = EMBED_CALL(bead_held, BeadCtx, .value = -distance / (side / 2.0), .low = 0.0, .high = 1.0);
            const double band = (in - caustic_center) / BEAD_CAUSTIC_WIDTH;
            const double banded = arith_exp(-(band * band));
            // water reflects little at the dome's top and nearly all at a grazing angle
            const double grazing = (1.0 - normal.z) * (1.0 - normal.z);
            const double fresnel = BEAD_GLINT_FLAT + ((1.0 - BEAD_GLINT_FLAT) * grazing);
            double glint = 0.0;
            double caustic = 0.0;
            double facing_away = 0.0;
            double unshaded = 1.0;
            for (unsigned int at = 0u; at < atlas->lights; at += 1u)
            {
                const GuiLight light = atlas->light[at];
                const BeadWay toward = bead_unit((BeadWay){light.x - ((double)origin.x + (double)x + 0.5),
                                                           light.y - ((double)origin.y + (double)y + 0.5),
                                                           light.z - lit_z});
                const BeadWay half = bead_unit((BeadWay){toward.x, toward.y, toward.z + 1.0});
                const double mirrored =
                    arith_max((normal.x * half.x) + (normal.y * half.y) + (normal.z * half.z), 0.0);
                glint += light.strength * BEAD_GLINT_LEVEL * fresnel * arith_pow(mirrored, sharp);
                const BeadWay away = bead_unit((BeadWay){-toward.x, -toward.y, 0.0});
                const double facing = arith_max((normal.x * away.x) + (normal.y * away.y), 0.0);
                caustic += light.strength * facing * banded;
                facing_away = (light.strength * facing > facing_away) ? light.strength * facing : facing_away;
                const double cast = EMBED_CALL(bead_blur_at, BeadCtx, .atlas = atlas,
                                               .across = (double)x - arith_round(drop_x[at]),
                                               .down = (double)y - arith_round(drop_y[at]));
                unshaded *= 1.0 - (weight[at] * cast);
            }
            const double outside = arith_max(distance, 0.0);
            const double halo = EMBED_CALL(bead_held, BeadCtx, .value = 1.0 - (outside / pad), .low = 0.0, .high = 1.0);
            const double mix = (1.0 - covered) * halo * halo;
            const double shadow = (1.0 - unshaded) * shadow_strength * (1.0 - covered);
            // the rim darkens where the dome turns away from the eye, max on the side away from the light
            const double rim = arith_pow(1.0 - normal.z, BEAD_RIM_POWER) * covered *
                               (BEAD_RIM_ROUND + ((1.0 - BEAD_RIM_ROUND) * facing_away));
            GuiColor lit = 0u;
            lit = EMBED_CALL(bead_light_over, BeadCtx, .had = lit, .share = shadow, .color = atlas->color[GUI_BEAD_COLOR_SHADOW]);
            lit = EMBED_CALL(bead_light_over, BeadCtx, .had = lit, .share = rim, .color = atlas->color[GUI_BEAD_COLOR_RIM]);
            lit = EMBED_CALL(bead_light_over, BeadCtx, .had = lit,
                             .share = EMBED_CALL(bead_held, BeadCtx, .value = caustic * covered, .low = 0.0, .high = 1.0),
                             .color = atlas->color[GUI_BEAD_COLOR_CAUSTIC]);
            lit = EMBED_CALL(bead_light_over, BeadCtx, .had = lit,
                             .share = EMBED_CALL(bead_held, BeadCtx, .value = glint * glint_strength * covered, .low = 0.0, .high = 1.0),
                             .color = atlas->color[GUI_BEAD_COLOR_GLINT]);
            // shares of 255, rounded, are bytes
            const unsigned int mix_byte = (unsigned int)((mix * 255.0) + 0.5);
            const unsigned int cover_byte = (unsigned int)((covered * 255.0) + 0.5);
            unsigned int mask = (mix_byte > cover_byte) ? mix_byte : cover_byte;
            mask = (GUI_ALPHA_OF(lit) > mask) ? GUI_ALPHA_OF(lit) : mask;
            mask = ((mask == 0u) && ((offset_x != 0) || (offset_y != 0))) ? 1u : mask;
            // an offset's low byte is its two's complement, as the reader undoes it
            plane_a[(y * stride) + x] = (mask << 24u) | (mix_byte << 16u) | (((unsigned int)offset_y & 0xffu) << 8u) |
                                        ((unsigned int)offset_x & 0xffu);
            plane_b[(y * stride) + x] = lit;
        }
    }
}

void gui_bead_init(const GuiBeadBakeCfg *args)
{
    GuiBeadAtlas *const atlas = args->atlas;
    atlas->made = 0u;
    atlas->baked = 0u;
    atlas->tools = 0u;
    atlas->lights = 0u;
    atlas->shape = GUI_BEAD_SHAPE_DOME;
    atlas->radius = 0u;
    atlas->cells.pixels = NULL;
    atlas->cells.width = 0u;
    atlas->cells.height = 0u;
}

// whether the bake refuses the sizes: a size of 0, a cell taller than the column's work, a reach below 3, or no
// tools or too many
EMBED_INLINE int bead_sizes_refused(const GuiBeadBakeCfg *args)
{
    const unsigned int cell_height = args->height + (2u * args->pad);
    return (args->width == 0u) || (args->height == 0u) || (cell_height > BEAD_CELL_MAX) || (args->reach < 3u) ||
           (args->tools == 0u) || (args->tools > GUI_BEAD_TOOLS_MAX);
}

// the rows the atlas's raster holds for the sizes: each button's frames, two planes a frame; the three patches with
// the reach as a margin; and the three planes of float work
EMBED_INLINE unsigned int bead_rows(const GuiBeadBakeCfg *args)
{
    const unsigned int cell_height = args->height + (2u * args->pad);
    return (args->tools * GUI_BEAD_FRAMES * 2u * cell_height) + (3u * (cell_height + (2u * args->reach))) +
           (3u * cell_height);
}

unsigned long long gui_bead_bytes(const GuiBeadBakeCfg *args)
{
    if (bead_sizes_refused(args))
    {
        return 0ULL;
    }
    const unsigned long long stride = args->width + (2ULL * args->pad) + (2ULL * args->reach);
    return stride * bead_rows(args) * sizeof(GuiColor);
}

int gui_bead_bake(const GuiBeadBakeCfg *args)
{
    GuiBeadAtlas *const atlas = args->atlas;
    const unsigned int cell_width = args->width + (2u * args->pad);
    const unsigned int cell_height = args->height + (2u * args->pad);
    const unsigned int lights = (args->light_count > GUI_LIGHTS_MAX) ? GUI_LIGHTS_MAX : args->light_count;
    if (bead_sizes_refused(args) || (args->pixels == NULL))
    {
        atlas->made = 0u;
        atlas->baked = 0u;
        return 0;
    }
    // the raster arranged again for new sizes, every button's frames unbaked
    if (!atlas->made || (atlas->width != args->width) || (atlas->height != args->height) ||
        (atlas->pad != args->pad) || (atlas->reach != args->reach) || (atlas->tools != args->tools))
    {
        atlas->made = 0u;
        atlas->baked = 0u;
        atlas->width = args->width;
        atlas->height = args->height;
        atlas->pad = args->pad;
        atlas->reach = args->reach;
        atlas->tools = args->tools;
        atlas->cell_width = cell_width;
        atlas->cell_height = cell_height;
        atlas->stride = cell_width + (2u * args->reach);
        if (!EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &atlas->cells, .width = atlas->stride,
                        .height = bead_rows(args), .pixels = args->pixels))
        {
            return 0;
        }
        atlas->made = 1u;
    }
    // the lights, the colors or the shape changed unbake every button
    const GuiColor *const colors = args->colors;
    int relit = (atlas->lights != lights) || (atlas->shape != args->shape) || (atlas->radius != args->radius);
    atlas->shape = args->shape;
    atlas->radius = args->radius;
    for (unsigned int at = 0u; at < lights; at += 1u)
    {
        const GuiLight light = args->lights[at];
        relit = relit || (atlas->light[at].x != light.x) || (atlas->light[at].y != light.y) ||
                (atlas->light[at].z != light.z) || (atlas->light[at].strength != light.strength);
        atlas->light[at] = light;
    }
    atlas->lights = lights;
    for (unsigned int at = 0u; at < GUI_BEAD_COLORS; at += 1u)
    {
        relit = relit || (atlas->color[at] != colors[at]);
        atlas->color[at] = colors[at];
    }
    atlas->baked = relit ? 0u : atlas->baked;
    // a button moved or sized anew unbakes its own frames
    for (unsigned int tool = 0u; tool < args->tools; tool += 1u)
    {
        const GuiRect origin = args->cells[tool];
        if ((atlas->origin[tool].x != origin.x) || (atlas->origin[tool].y != origin.y) ||
            (atlas->origin[tool].width != origin.width) || (atlas->origin[tool].height != origin.height))
        {
            atlas->baked &= ~(1u << tool);
            atlas->origin[tool] = origin;
        }
    }
    return 1;
}

int gui_bead_tool_bake(const GuiBeadBlitCfg *args)
{
    GuiBeadAtlas *const atlas = args->atlas;
    if (!atlas->made || (args->tool >= atlas->tools))
    {
        return 0;
    }
    if ((atlas->baked & (1u << args->tool)) != 0u)
    {
        return 1;
    }
    for (unsigned int frame = 0u; frame < GUI_BEAD_FRAMES; frame += 1u)
    {
        EMBED_CALL(bead_frame_bake, BeadCtx, .atlas = atlas, .tool = args->tool, .frame = frame);
    }
    atlas->baked |= 1u << args->tool;
    return 1;
}

unsigned int gui_bead_frame(const GuiBeadCfg *args)
{
    // the press share in `one`, the low end's share the same, both inside a long long
    const long long low = (long long)(GUI_BEAD_LOW * (double)args->one);
    const long long span = args->one - low;
    if (span <= 0LL)
    {
        return 0u;
    }
    long long share = args->press - low;
    share = (share < 0LL) ? 0LL : ((share > span) ? span : share);
    const long long frame = ((share * (long long)(GUI_BEAD_FRAMES - 1u)) + (span / 2LL)) / span;
    // the frame is below the frames' count
    return (unsigned int)frame;
}

// A box pass of four over `lines` lines of a patch: each pixel of a line the mean of the four from offset `lo` to
// `hi` along `step`, the lines `line_step` apart, from `count` pixels of the line. The mean is a shift
EMBED_INLINE void bead_box_pass(const BeadCtx *args)
{
    for (unsigned int line = 0u; line < args->lines; line += 1u)
    {
        const GuiColor *const from = args->from + ((unsigned long long)line * args->line_step);
        GuiColor *const to = args->to + ((unsigned long long)line * args->line_step);
        for (unsigned int at = args->lo; at < args->count - args->hi; at += 1u)
        {
            const unsigned long long summed = bead_spread(from[(at - args->lo) * args->step]) +
                                              bead_spread(from[(at - args->lo + 1u) * args->step]) +
                                              bead_spread(from[(at - args->lo + 2u) * args->step]) +
                                              bead_spread(from[(at - args->lo + 3u) * args->step]);
            to[at * args->step] = bead_pack((summed >> 2u) & BEAD_LANES);
        }
    }
}

void gui_bead_blit(const GuiBeadBlitCfg *args)
{
    GuiBeadAtlas *const atlas = args->atlas;
    GuiRaster *const raster = args->raster;
    if ((args->opacity == 0u) || (args->frame >= GUI_BEAD_FRAMES) || !gui_bead_tool_bake(args))
    {
        return;
    }
    // the cell's sides are far inside an int
    const int reach = (int)atlas->reach;
    GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                            .one = (GuiRect){args->x, args->y, (int)atlas->cell_width, (int)atlas->cell_height},
                            .other = (GuiRect){0, 0, (int)raster->width, (int)raster->height});
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->clip))
    {
        on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = on, .other = args->clip);
    }
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = raster->clip))
    {
        on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = on, .other = raster->clip);
    }
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
    {
        return;
    }
    // the patch under the cell copied, the reach round it, the raster's edge pixels located past the raster
    const unsigned int stride = atlas->stride;
    const unsigned int patch_rows = EMBED_CALL(bead_patch_rows, BeadCtx, .atlas_read = atlas);
    GuiColor *const sharp =
        EMBED_CALL(bead_row, BeadCtx, .atlas = atlas, .row = EMBED_CALL(bead_patch_row, BeadCtx, .atlas_read = atlas, .patch = 0u));
    GuiColor *const ping =
        EMBED_CALL(bead_row, BeadCtx, .atlas = atlas, .row = EMBED_CALL(bead_patch_row, BeadCtx, .atlas_read = atlas, .patch = 1u));
    GuiColor *const frost =
        EMBED_CALL(bead_row, BeadCtx, .atlas = atlas, .row = EMBED_CALL(bead_patch_row, BeadCtx, .atlas_read = atlas, .patch = 2u));
    const int width = (int)raster->width;
    const int height = (int)raster->height;
    const int left = args->x - reach;
    const int top = args->y - reach;
    // the columns of the patch that lie on the raster are one copy a row; the columns past its sides take its edge
    // pixel; the sides are far inside an int
    const int first = (left < 0) ? -left : 0;
    const int past = ((left + (int)stride) > width) ? width - left : (int)stride;
    for (unsigned int row = 0u; row < patch_rows; row += 1u)
    {
        int source_row = top + (int)row;
        source_row = (source_row < 0) ? 0 : ((source_row >= height) ? height - 1 : source_row);
        // the row lies on the raster: its number is not negative
        const GuiColor *const from = raster->pixels + ((unsigned long long)(unsigned int)source_row * raster->width);
        GuiColor *const to = sharp + ((unsigned long long)row * stride);
        if (past > first)
        {
            memor.cpy(&(MemoriaCfg){.dst = to + first, .src = from + (left + first),
                                    .bytes = (unsigned long long)(past - first) * sizeof(GuiColor)});
        }
        for (int column = 0; column < first; column += 1)
        {
            to[column] = from[0];
        }
        for (int column = (past > first) ? past : first; column < (int)stride; column += 1)
        {
            to[column] = from[(unsigned int)(width - 1)];
        }
    }
    // the frost: two box passes of four across and two down, which together weigh seven pixels as a triangle
    EMBED_CALL(bead_box_pass, BeadCtx, .from = sharp, .to = ping, .lo = 2u, .hi = 1u, .step = 1u, .count = stride,
               .lines = patch_rows, .line_step = stride);
    EMBED_CALL(bead_box_pass, BeadCtx, .from = ping, .to = frost, .lo = 1u, .hi = 2u, .step = 1u, .count = stride,
               .lines = patch_rows, .line_step = stride);
    EMBED_CALL(bead_box_pass, BeadCtx, .from = frost, .to = ping, .lo = 2u, .hi = 1u, .step = stride, .count = patch_rows,
               .lines = stride, .line_step = 1u);
    EMBED_CALL(bead_box_pass, BeadCtx, .from = ping, .to = frost, .lo = 1u, .hi = 2u, .step = stride, .count = patch_rows,
               .lines = stride, .line_step = 1u);
    // each pixel of the cell on the raster: skipped where its mask is 0; else read through its offsets from the
    // sharp patch, mixed with the frosted patch by its share, and lit from plane B
    const unsigned int scale = args->opacity + (args->opacity >> 7u);
    const GuiColor *const plane_a = EMBED_CALL(
        bead_row_read, BeadCtx, .atlas_read = atlas,
        .row = EMBED_CALL(bead_plane_row, BeadCtx, .atlas_read = atlas, .tool = args->tool, .frame = args->frame));
    const GuiColor *const plane_b = plane_a + ((unsigned long long)atlas->cell_height * stride);
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        // the row and column inside the cell are not negative, the rectangle drawn lying inside it
        const unsigned int row = (unsigned int)(y - args->y);
        const GuiColor *const words = plane_a + ((unsigned long long)row * stride);
        const GuiColor *const lights = plane_b + ((unsigned long long)row * stride);
        const GuiColor *const frosted = frost + ((unsigned long long)(row + atlas->reach) * stride) + atlas->reach;
        const GuiColor *const center = sharp + ((unsigned long long)(row + atlas->reach) * stride) + atlas->reach;
        GuiColor *const out = raster->pixels + ((unsigned long long)(unsigned int)y * raster->width);
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            const unsigned int column = (unsigned int)(x - args->x);
            const GuiColor word = words[column];
            if (GUI_BEAD_MASK_OF(word) == 0u)
            {
                continue;
            }
            // the offsets at the opacity, toward 0 as it fades, each within the reach
            const int offset_x = (GUI_BEAD_DX_OF(word) * (int)scale) / 256;
            const int offset_y = (GUI_BEAD_DY_OF(word) * (int)scale) / 256;
            const GuiColor read = center[((long long)offset_y * (long long)stride) + (long long)column + offset_x];
            const unsigned int mix = (GUI_BEAD_MIX_OF(word) * scale) >> 8u;
            const unsigned long long ground =
                ((bead_spread(read) * (256u - mix)) + (bead_spread(frosted[column]) * mix)) >> 8u;
            const unsigned long long light = (bead_spread(lights[column]) * scale) >> 8u;
            const unsigned int alpha = (GUI_ALPHA_OF(lights[column]) * scale) >> 8u;
            unsigned long long lanes = (light & BEAD_LANES) + ((((ground & BEAD_LANES) * (256u - alpha)) >> 8u) & BEAD_LANES);
            // a lane past a byte is held at the byte's max
            const unsigned long long carried = (lanes >> 8u) & BEAD_LANE_CARRY;
            lanes = (lanes | ((carried << 8u) - carried)) & BEAD_LANES;
            out[(unsigned int)x] = bead_pack(lanes) | 0xff000000u;
        }
    }
}
