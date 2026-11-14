// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font_outline.c: a glyph's outline read from glyf, its curves cut into lines, and the lines handed to the one
// rasterizer (coverage.h) as the glyph's mask
#include "../arith/arith.h"
#include "font.h"

#include "../coverage/coverage.h"

// the steps a quadratic curve is cut into
#define FONT_CURVE_STEPS 8u

typedef struct
{
    FontContours *out;
    const float *matrix;
    float x;
    float y;
    const float *from;
    const float *control;
    const float *to;
    const Font *font;
    unsigned long long at;
    unsigned int depth;
    unsigned char *flags;
    unsigned int points;
    float *xs;
    float *ys;
    unsigned int start;
    unsigned int end;
} FontOutlineCtx;
// a point put after the outline's, placed by the matrix
EMBED_INLINE void outline_point(const FontOutlineCtx *args)
{
    if (args->out->points >= FONT_OUTLINE_POINTS)
    {
        return;
    }
    args->out->x[args->out->points] = (args->matrix[0] * args->x) + (args->matrix[2] * args->y) + args->matrix[4];
    args->out->y[args->out->points] = (args->matrix[1] * args->x) + (args->matrix[3] * args->y) + args->matrix[5];
    args->out->points += 1u;
}

// A quadratic curve cut into lines, each point placed by the matrix
EMBED_INLINE void outline_curve(const FontOutlineCtx *args)
{
    const float *from = args->from;
    const float *control = args->control;
    const float *to = args->to;
    for (unsigned int step = 1u; step <= FONT_CURVE_STEPS; step += 1u)
    {
        const float t = (float)step / (float)FONT_CURVE_STEPS;
        const float u = 1.0f - t;
        EMBED_CALL(outline_point, FontOutlineCtx, .out = args->out, .matrix = args->matrix,
                   .x = (u * u * from[0]) + (2.0f * u * t * control[0]) + (t * t * to[0]),
                   .y = (u * u * from[1]) + (2.0f * u * t * control[1]) + (t * t * to[1]));
    }
}

// A glyph made of other glyphs: each part outlined with its own matrix after the one given
EMBED_INLINE void outline_composite(const FontOutlineCtx *args)
{
    const Font *font = args->font;
    unsigned long long at = args->at;
    const float *matrix = args->matrix;
    unsigned int depth = args->depth;
    FontContours *out = args->out;
    if (depth > 6u)
    {
        return;
    }
    unsigned long long part = at + 10u;
    for (;;)
    {
        const unsigned int flags = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = part);
        const unsigned int child = EMBED_CALL(font_at.u16, FontAtCfg, .font = font, .at = part + 2u);
        part += 4u;
        float dx = 0.0f;
        float dy = 0.0f;
        if ((flags & 1u) != 0u)
        {
            dx = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = part);
            dy = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = part + 2u);
            part += 4u;
        }
        else
        {
            // each byte read as the signed byte it is
            dx = (float)((part < font->size) ? (signed char)font->data[part] : 0);
            dy = (float)(((part + 1u) < font->size) ? (signed char)font->data[part + 1u] : 0);
            part += 2u;
        }
        float a = 1.0f;
        float b = 0.0f;
        float c = 0.0f;
        float d = 1.0f;
        if ((flags & 0x8u) != 0u)
        {
            a = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = part) / 16384.0f;
            d = a;
            part += 2u;
        }
        else if ((flags & 0x40u) != 0u)
        {
            a = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = part) / 16384.0f;
            d = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = part + 2u) / 16384.0f;
            part += 4u;
        }
        else if ((flags & 0x80u) != 0u)
        {
            a = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = part) / 16384.0f;
            b = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = part + 2u) / 16384.0f;
            c = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = part + 4u) / 16384.0f;
            d = (float)EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = part + 6u) / 16384.0f;
            part += 8u;
        }
        const float child_matrix[6] = {(matrix[0] * a) + (matrix[2] * b),
                                       (matrix[1] * a) + (matrix[3] * b),
                                       (matrix[0] * c) + (matrix[2] * d),
                                       (matrix[1] * c) + (matrix[3] * d),
                                       (matrix[0] * dx) + (matrix[2] * dy) + matrix[4],
                                       (matrix[1] * dx) + (matrix[3] * dy) + matrix[5]};
        EMBED_CALL(font_glyph_matrix.outline, FontGlyphMatrixCfg, .font = font, .glyph = child,
                   .matrix = child_matrix, .depth = depth + 1u, .out = out);
        if ((flags & 0x20u) == 0u)
        {
            return;
        }
    }
}

// the flags of a simple glyph's `points` points read from `at` into `flags`, a flag's repeats with it: where its
// coordinates start
EMBED_INLINE unsigned long long outline_flags_read(const FontOutlineCtx *args)
{
    const Font *font = args->font;
    unsigned long long cursor = args->at;
    unsigned int flag_count = 0u;
    while (flag_count < args->points)
    {
        const unsigned char flag = (cursor < font->size) ? font->data[cursor] : 0u;
        cursor += 1ULL;
        args->flags[flag_count] = flag;
        flag_count += 1u;
        if ((flag & 8u) == 0u)
        {
            continue;
        }
        const unsigned int repeats = (cursor < font->size) ? font->data[cursor] : 0u;
        cursor += 1ULL;
        unsigned int each = 0u;
        while ((each < repeats) && (flag_count < args->points))
        {
            args->flags[flag_count] = flag;
            flag_count += 1u;
            each += 1u;
        }
    }
    return cursor;
}

// the x's of a simple glyph's points read from `at`, then the y's: a short one a byte, its sign by the flag, else a
// word, or the one before where the flag gives the same
EMBED_INLINE void outline_coords_read(const FontOutlineCtx *args)
{
    const Font *font = args->font;
    unsigned long long cursor = args->at;
    for (unsigned int axis = 0u; axis < 2u; axis += 1u)
    {
        const unsigned char short_flag = (axis == 0u) ? 0x02u : 0x04u;
        const unsigned char same_flag = (axis == 0u) ? 0x10u : 0x20u;
        float *const values = (axis == 0u) ? args->xs : args->ys;
        int value = 0;
        for (unsigned int index = 0u; index < args->points; index += 1u)
        {
            const unsigned char flag = args->flags[index];
            if ((flag & short_flag) != 0u)
            {
                const int delta = (cursor < font->size) ? font->data[cursor] : 0;
                cursor += 1ULL;
                value += ((flag & same_flag) != 0u) ? delta : -delta;
            }
            else if ((flag & same_flag) == 0u)
            {
                value += EMBED_CALL(font_at.i16, FontAtCfg, .font = font, .at = cursor);
                cursor += 2ULL;
            }
            values[index] = (float)value;
        }
    }
}

// the contour of the points from `start` to `end` put after the outline's, its curves cut into lines. It starts at a
// point on the curve: the first that is, or the middle of the first two
EMBED_INLINE void outline_contour(const FontOutlineCtx *args)
{
    const unsigned int start = args->start;
    const unsigned int ring = args->end - start + 1u;
    unsigned int first_on = 0u;
    while ((first_on < ring) && ((args->flags[start + first_on] & 1u) == 0u))
    {
        first_on += 1u;
    }
    const unsigned int second = start + ((ring > 1u) ? 1u : 0u);
    const float *const xs = args->xs;
    const float *const ys = args->ys;
    const float begin[2] = {(first_on < ring) ? xs[start + first_on] : ((xs[start] + xs[second]) / 2.0f),
                            (first_on < ring) ? ys[start + first_on] : ((ys[start] + ys[second]) / 2.0f)};
    const unsigned int shift = (first_on < ring) ? first_on : 0u;
    EMBED_CALL(outline_point, FontOutlineCtx, .out = args->out, .matrix = args->matrix, .x = begin[0], .y = begin[1]);
    int controlled = 0;
    float control[2] = {0.0f, 0.0f};
    float pen[2] = {begin[0], begin[1]};
    for (unsigned int step = 1u; step <= ring; step += 1u)
    {
        const unsigned int index = start + ((shift + step) % ring);
        const float point[2] = {xs[index], ys[index]};
        const int on_curve = (args->flags[index] & 1u) != 0u;
        if (on_curve && controlled)
        {
            EMBED_CALL(outline_curve, FontOutlineCtx, .out = args->out, .matrix = args->matrix, .from = pen,
                       .control = control, .to = point);
        }
        else if (on_curve)
        {
            EMBED_CALL(outline_point, FontOutlineCtx, .out = args->out, .matrix = args->matrix, .x = point[0],
                       .y = point[1]);
        }
        else if (controlled)
        {
            const float middle[2] = {(control[0] + point[0]) / 2.0f, (control[1] + point[1]) / 2.0f};
            EMBED_CALL(outline_curve, FontOutlineCtx, .out = args->out, .matrix = args->matrix, .from = pen,
                       .control = control, .to = middle);
            pen[0] = middle[0];
            pen[1] = middle[1];
        }
        pen[0] = on_curve ? point[0] : pen[0];
        pen[1] = on_curve ? point[1] : pen[1];
        control[0] = on_curve ? control[0] : point[0];
        control[1] = on_curve ? control[1] : point[1];
        controlled = !on_curve;
    }
    if (controlled)
    {
        EMBED_CALL(outline_curve, FontOutlineCtx, .out = args->out, .matrix = args->matrix, .from = pen, .control = control,
                   .to = begin);
    }
    if (args->out->contours < FONT_OUTLINE_CONTOURS)
    {
        args->out->end[args->out->contours] = args->out->points;
        args->out->contours += 1u;
    }
}

void font_outline(const FontGlyphMatrixCfg *args)
{
    static unsigned int s_ends[FONT_OUTLINE_CONTOURS];
    static unsigned char s_flags[FONT_OUTLINE_POINTS];
    static float s_xs[FONT_OUTLINE_POINTS];
    static float s_ys[FONT_OUTLINE_POINTS];
    unsigned long long at = 0ULL;
    unsigned long long to = 0ULL;
    if (!EMBED_CALL(font_glyph_table.glyph_range, FontGlyphCfg, .font = args->font, .glyph = args->glyph,
                    .from = &at, .to = &to))
    {
        return;
    }
    const int contours = EMBED_CALL(font_at.i16, FontAtCfg, .font = args->font, .at = at);
    if (contours < 0)
    {
        EMBED_CALL(outline_composite, FontOutlineCtx, .font = args->font, .at = at, .matrix = args->matrix,
                   .depth = args->depth, .out = args->out);
        return;
    }
    // the contours' count is not negative here
    const unsigned int count =
        ((unsigned int)contours < FONT_OUTLINE_CONTOURS) ? (unsigned int)contours : FONT_OUTLINE_CONTOURS;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        s_ends[index] = EMBED_CALL(font_at.u16, FontAtCfg, .font = args->font, .at = at + 10u + (index * 2ULL));
    }
    const unsigned int points = (count != 0u) ? (s_ends[count - 1u] + 1u) : 0u;
    if (points > FONT_OUTLINE_POINTS)
    {
        return;
    }
    const unsigned int instructions =
        EMBED_CALL(font_at.u16, FontAtCfg, .font = args->font, .at = at + 10u + (count * 2ULL));
    const unsigned long long coords =
        EMBED_CALL(outline_flags_read, FontOutlineCtx, .font = args->font, .at = at + 12u + (count * 2ULL) + instructions,
                   .flags = s_flags, .points = points);
    EMBED_CALL(outline_coords_read, FontOutlineCtx, .font = args->font, .at = coords, .flags = s_flags, .points = points,
               .xs = s_xs, .ys = s_ys);
    unsigned int start = 0u;
    for (unsigned int contour = 0u; contour < count; contour += 1u)
    {
        const unsigned int end = s_ends[contour];
        if ((end < start) || (end >= points))
        {
            break;
        }
        EMBED_CALL(outline_contour, FontOutlineCtx, .out = args->out, .matrix = args->matrix, .flags = s_flags, .xs = s_xs,
                   .ys = s_ys, .start = start, .end = end);
        start = end + 1u;
    }
}

unsigned long long font_rasterize_bytes(const FontContoursCfg *args)
{
    return EMBED_CALL(coverage.draw_bytes, CoverageCfg, .end = args->contours->end,
                      .contours = args->contours->contours);
}

void font_rasterize(const FontContoursCfg *args)
{
    float *cover = args->cover;
    float min_x = ARITH_F32_MAX;
    float min_y = ARITH_F32_MAX;
    float max_x = -ARITH_F32_MAX;
    float max_y = -ARITH_F32_MAX;
    for (unsigned int at = 0u; at < args->contours->points; at += 1u)
    {
        min_x = (args->contours->x[at] < min_x) ? args->contours->x[at] : min_x;
        max_x = (args->contours->x[at] > max_x) ? args->contours->x[at] : max_x;
        min_y = (args->contours->y[at] < min_y) ? args->contours->y[at] : min_y;
        max_y = (args->contours->y[at] > max_y) ? args->contours->y[at] : max_y;
    }
    args->mask_bits->width = 0ULL;
    args->mask_bits->height = 0ULL;
    args->mask_bits->left = 0;
    args->mask_bits->top = 0;
    args->mask_bits->cover = cover;
    if (min_x > max_x)
    {
        return;
    }
    // a glyph's place in pixels is far inside what an int represents
    const int left = (int)arith_floor_f32(min_x);
    const int top = (int)arith_ceil_f32(max_y);
    const int span = (int)arith_ceil_f32(max_x) - left;
    const int rows = top - (int)arith_floor_f32(min_y);
    const unsigned long long width = (unsigned long long)((span > 1) ? span : 1) + 1ULL;
    const unsigned long long height = (unsigned long long)((rows > 1) ? rows : 1);
    if (cover == NULL)
    {
        args->mask_bits->width = width;
        args->mask_bits->height = height;
        args->mask_bits->left = left;
        args->mask_bits->top = top;
        return;
    }
    if (((width * height) + 4ULL) > args->capacity)
    {
        return;
    }
    // the one rasterizer gives each pixel its share, the mask's top left corner at the outline's (left, top)
    EMBED_CALL(coverage.draw, CoverageCfg, .x = args->contours->x, .y = args->contours->y,
               .end = args->contours->end, .contours = args->contours->contours, .origin_x = (float)left,
               .origin_y = (float)top, .cover = cover, .width = width, .height = height, .scratch = args->scratch,
               .scratch_bytes = args->scratch_bytes);
    args->mask_bits->width = width;
    args->mask_bits->height = height;
    args->mask_bits->left = left;
    args->mask_bits->top = top;
}
