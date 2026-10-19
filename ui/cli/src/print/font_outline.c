// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font_outline.c: a glyph's outline read from glyf, its curves cut into lines, and the lines drawn as coverage
#include "font.h"

#include <float.h>
#include <math.h>

// the steps a quadratic curve is cut into
#define CLI_CURVE_STEPS 8u

// a point put after the outline's, placed by the matrix
static void cli_point(CliContours *out, const float *matrix, float x, float y)
{
    if (out->points >= CLI_OUTLINE_POINTS)
    {
        return;
    }
    out->x[out->points] = (matrix[0] * x) + (matrix[2] * y) + matrix[4];
    out->y[out->points] = (matrix[1] * x) + (matrix[3] * y) + matrix[5];
    out->points += 1u;
}

// A quadratic curve cut into lines, each point placed by the matrix
static void cli_curve(CliContours *out, const float *matrix, const float *from, const float *control, const float *to)
{
    for (unsigned int step = 1u; step <= CLI_CURVE_STEPS; step += 1u)
    {
        const float t = (float)step / (float)CLI_CURVE_STEPS;
        const float u = 1.0f - t;
        cli_point(out, matrix, (u * u * from[0]) + (2.0f * u * t * control[0]) + (t * t * to[0]),
                  (u * u * from[1]) + (2.0f * u * t * control[1]) + (t * t * to[1]));
    }
}

// A glyph made of other glyphs: each part outlined with its own matrix after the one given
static void cli_composite(const CliFont *font, unsigned long long at, const float *matrix, unsigned int depth,
                          CliContours *out)
{
    if (depth > 6u)
    {
        return;
    }
    unsigned long long part = at + 10u;
    for (;;)
    {
        const unsigned int flags = cli_font_u16(font, part);
        const unsigned int child = cli_font_u16(font, part + 2u);
        part += 4u;
        float dx = 0.0f;
        float dy = 0.0f;
        if ((flags & 1u) != 0u)
        {
            dx = (float)cli_font_i16(font, part);
            dy = (float)cli_font_i16(font, part + 2u);
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
            a = (float)cli_font_i16(font, part) / 16384.0f;
            d = a;
            part += 2u;
        }
        else if ((flags & 0x40u) != 0u)
        {
            a = (float)cli_font_i16(font, part) / 16384.0f;
            d = (float)cli_font_i16(font, part + 2u) / 16384.0f;
            part += 4u;
        }
        else if ((flags & 0x80u) != 0u)
        {
            a = (float)cli_font_i16(font, part) / 16384.0f;
            b = (float)cli_font_i16(font, part + 2u) / 16384.0f;
            c = (float)cli_font_i16(font, part + 4u) / 16384.0f;
            d = (float)cli_font_i16(font, part + 6u) / 16384.0f;
            part += 8u;
        }
        const float child_matrix[6] = {(matrix[0] * a) + (matrix[2] * b),
                                       (matrix[1] * a) + (matrix[3] * b),
                                       (matrix[0] * c) + (matrix[2] * d),
                                       (matrix[1] * c) + (matrix[3] * d),
                                       (matrix[0] * dx) + (matrix[2] * dy) + matrix[4],
                                       (matrix[1] * dx) + (matrix[3] * dy) + matrix[5]};
        cli_font_outline(font, child, child_matrix, depth + 1u, out);
        if ((flags & 0x20u) == 0u)
        {
            return;
        }
    }
}

void cli_font_outline(const CliFont *font, unsigned int glyph, const float *matrix, unsigned int depth,
                      CliContours *out)
{
    static unsigned int s_ends[CLI_OUTLINE_CONTOURS];
    static unsigned char s_flags[CLI_OUTLINE_POINTS];
    static float s_xs[CLI_OUTLINE_POINTS];
    static float s_ys[CLI_OUTLINE_POINTS];
    unsigned long long at = 0ull;
    unsigned long long to = 0ull;
    if (!cli_font_glyph_range(font, glyph, &at, &to))
    {
        return;
    }
    const int contours = cli_font_i16(font, at);
    if (contours < 0)
    {
        cli_composite(font, at, matrix, depth, out);
        return;
    }
    // the contours' count is not negative here
    const unsigned int count =
        ((unsigned int)contours < CLI_OUTLINE_CONTOURS) ? (unsigned int)contours : CLI_OUTLINE_CONTOURS;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        s_ends[index] = cli_font_u16(font, at + 10u + (index * 2ull));
    }
    const unsigned int points = (count != 0u) ? (s_ends[count - 1u] + 1u) : 0u;
    if (points > CLI_OUTLINE_POINTS)
    {
        return;
    }
    const unsigned int instructions = cli_font_u16(font, at + 10u + (count * 2ull));
    unsigned long long cursor = at + 12u + (count * 2ull) + instructions;
    unsigned int flag_count = 0u;
    while (flag_count < points)
    {
        const unsigned char flag = (cursor < font->size) ? font->data[cursor] : 0u;
        cursor += 1ull;
        s_flags[flag_count] = flag;
        flag_count += 1u;
        if ((flag & 8u) != 0u)
        {
            const unsigned int repeats = (cursor < font->size) ? font->data[cursor] : 0u;
            cursor += 1ull;
            for (unsigned int each = 0u; (each < repeats) && (flag_count < points); each += 1u)
            {
                s_flags[flag_count] = flag;
                flag_count += 1u;
            }
        }
    }
    // the x's, then the y's: a short one a byte, its sign by the flag, else a word, or the one before where the flag
    // says the same
    for (unsigned int axis = 0u; axis < 2u; axis += 1u)
    {
        const unsigned char short_flag = (axis == 0u) ? 0x02u : 0x04u;
        const unsigned char same_flag = (axis == 0u) ? 0x10u : 0x20u;
        float *const values = (axis == 0u) ? s_xs : s_ys;
        int value = 0;
        for (unsigned int index = 0u; index < points; index += 1u)
        {
            const unsigned char flag = s_flags[index];
            if ((flag & short_flag) != 0u)
            {
                const int delta = (cursor < font->size) ? font->data[cursor] : 0;
                cursor += 1ull;
                value += ((flag & same_flag) != 0u) ? delta : -delta;
            }
            else if ((flag & same_flag) == 0u)
            {
                value += cli_font_i16(font, cursor);
                cursor += 2ull;
            }
            values[index] = (float)value;
        }
    }
    unsigned int start = 0u;
    for (unsigned int contour = 0u; contour < count; contour += 1u)
    {
        const unsigned int end = s_ends[contour];
        if ((end < start) || (end >= points))
        {
            break;
        }
        const unsigned int ring = end - start + 1u;
        // The contour starts at a point on the curve: the first that is, or the middle of the first two
        unsigned int first_on = ring;
        for (unsigned int index = 0u; (first_on == ring) && (index < ring); index += 1u)
        {
            first_on = ((s_flags[start + index] & 1u) != 0u) ? index : ring;
        }
        const unsigned int second = start + ((ring > 1u) ? 1u : 0u);
        const float begin[2] = {(first_on < ring) ? s_xs[start + first_on] : ((s_xs[start] + s_xs[second]) / 2.0f),
                                (first_on < ring) ? s_ys[start + first_on] : ((s_ys[start] + s_ys[second]) / 2.0f)};
        const unsigned int shift = (first_on < ring) ? first_on : 0u;
        cli_point(out, matrix, begin[0], begin[1]);
        int controlled = 0;
        float control[2] = {0.0f, 0.0f};
        float pen[2] = {begin[0], begin[1]};
        for (unsigned int step = 1u; step <= ring; step += 1u)
        {
            const unsigned int index = start + ((shift + step) % ring);
            const float point[2] = {s_xs[index], s_ys[index]};
            if ((s_flags[index] & 1u) != 0u)
            {
                if (controlled)
                {
                    cli_curve(out, matrix, pen, control, point);
                    controlled = 0;
                }
                else
                {
                    cli_point(out, matrix, point[0], point[1]);
                }
                pen[0] = point[0];
                pen[1] = point[1];
            }
            else if (controlled)
            {
                const float middle[2] = {(control[0] + point[0]) / 2.0f, (control[1] + point[1]) / 2.0f};
                cli_curve(out, matrix, pen, control, middle);
                pen[0] = middle[0];
                pen[1] = middle[1];
                control[0] = point[0];
                control[1] = point[1];
            }
            else
            {
                control[0] = point[0];
                control[1] = point[1];
                controlled = 1;
            }
        }
        if (controlled)
        {
            cli_curve(out, matrix, pen, control, begin);
        }
        if (out->contours < CLI_OUTLINE_CONTOURS)
        {
            out->end[out->contours] = out->points;
            out->contours += 1u;
        }
        start = end + 1u;
    }
}

// Adds the signed area a line from `p0` to `p1` leaves in each cell it crosses, as the sum along a row of the cells'
// areas gives each pixel's coverage
static void cli_line(float *area, unsigned long long width, unsigned long long height, float x0, float y0, float x1,
                     float y1)
{
    if (fabsf(y0 - y1) <= FLT_EPSILON)
    {
        return;
    }
    const float dir = (y0 < y1) ? 1.0f : -1.0f;
    const float from[2] = {(y0 < y1) ? x0 : x1, (y0 < y1) ? y0 : y1};
    const float to[2] = {(y0 < y1) ? x1 : x0, (y0 < y1) ? y1 : y0};
    const float dxdy = (to[0] - from[0]) / (to[1] - from[1]);
    float x = from[0];
    if (from[1] < 0.0f)
    {
        x -= from[1] * dxdy;
    }
    // the rows the line crosses, from the first whole row to the last, inside the mask
    const unsigned long long first = (unsigned long long)((from[1] > 0.0f) ? from[1] : 0.0f);
    const unsigned long long ceiling = (unsigned long long)ceilf(to[1]);
    const unsigned long long last = (ceiling < height) ? ceiling : height;
    for (unsigned long long y = first; y < last; y += 1ull)
    {
        const long long start = (long long)(y * width);
        const float top = ((float)(y + 1ull) < to[1]) ? (float)(y + 1ull) : to[1];
        const float bottom = ((float)y > from[1]) ? (float)y : from[1];
        const float dy = top - bottom;
        const float next = x + (dxdy * dy);
        const float d = dy * dir;
        const float x0_cell = (x < next) ? x : next;
        const float x1_cell = (x < next) ? next : x;
        const float x0_floor = floorf(x0_cell);
        const long long x0i = (long long)x0_floor;
        const float x1_ceil = ceilf(x1_cell);
        const long long x1i = (long long)x1_ceil;
#define CLI_AT(column_) ((unsigned long long)(((start + (column_)) > 0ll) ? (start + (column_)) : 0ll))
        if (x1i <= (x0i + 1ll))
        {
            const float middle = (0.5f * (x + next)) - x0_floor;
            area[CLI_AT(x0i)] += d - (d * middle);
            area[CLI_AT(x0i + 1ll)] += d * middle;
        }
        else
        {
            const float s = 1.0f / (x1_cell - x0_cell);
            const float x0f = x0_cell - x0_floor;
            const float a0 = 0.5f * s * (1.0f - x0f) * (1.0f - x0f);
            const float x1f = (x1_cell - x1_ceil) + 1.0f;
            const float am = 0.5f * s * x1f * x1f;
            area[CLI_AT(x0i)] += d * a0;
            if (x1i == (x0i + 2ll))
            {
                area[CLI_AT(x0i + 1ll)] += d * (1.0f - a0 - am);
            }
            else
            {
                const float a1 = s * (1.5f - x0f);
                area[CLI_AT(x0i + 1ll)] += d * (a1 - a0);
                for (long long column = x0i + 2ll; column < (x1i - 1ll); column += 1ll)
                {
                    area[CLI_AT(column)] += d * s;
                }
                const float a2 = a1 + ((float)(x1i - x0i - 3ll) * s);
                area[CLI_AT(x1i - 1ll)] += d * (1.0f - a2 - am);
            }
            area[CLI_AT(x1i)] += d * am;
        }
#undef CLI_AT
        x = next;
    }
}

void cli_font_rasterize(const CliContours *contours, CliMask *mask, float *cover, unsigned long long room)
{
    float min_x = FLT_MAX;
    float min_y = FLT_MAX;
    float max_x = -FLT_MAX;
    float max_y = -FLT_MAX;
    for (unsigned int at = 0u; at < contours->points; at += 1u)
    {
        min_x = (contours->x[at] < min_x) ? contours->x[at] : min_x;
        max_x = (contours->x[at] > max_x) ? contours->x[at] : max_x;
        min_y = (contours->y[at] < min_y) ? contours->y[at] : min_y;
        max_y = (contours->y[at] > max_y) ? contours->y[at] : max_y;
    }
    mask->width = 0ull;
    mask->height = 0ull;
    mask->left = 0;
    mask->top = 0;
    mask->cover = cover;
    if (min_x > max_x)
    {
        return;
    }
    // a glyph's place in pixels is far inside what an int holds
    const int left = (int)floorf(min_x);
    const int top = (int)ceilf(max_y);
    const int span = (int)ceilf(max_x) - left;
    const int rows = top - (int)floorf(min_y);
    const unsigned long long width = (unsigned long long)((span > 1) ? span : 1) + 1ull;
    const unsigned long long height = (unsigned long long)((rows > 1) ? rows : 1);
    if (((width * height) + 4ull) > room)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = cover, .val = 0, .bytes = ((width * height) + 4ull) * sizeof(cover[0])});
    unsigned int first = 0u;
    for (unsigned int contour = 0u; contour < contours->contours; contour += 1u)
    {
        const unsigned int end = contours->end[contour];
        const unsigned int count = end - first;
        for (unsigned int pair = 0u; pair < count; pair += 1u)
        {
            const unsigned int a = first + pair;
            const unsigned int b = first + ((pair + 1u) % count);
            cli_line(cover, width, height, contours->x[a] - (float)left, (float)top - contours->y[a],
                     contours->x[b] - (float)left, (float)top - contours->y[b]);
        }
        first = end;
    }
    // the areas summed along each row in place, each pixel's coverage at most 1
    for (unsigned long long row = 0ull; row < height; row += 1ull)
    {
        float sum = 0.0f;
        for (unsigned long long column = 0ull; column < width; column += 1ull)
        {
            sum += cover[(row * width) + column];
            const float magnitude = fabsf(sum);
            cover[(row * width) + column] = (magnitude < 1.0f) ? magnitude : 1.0f;
        }
    }
    mask->width = width;
    mask->height = height;
    mask->left = left;
    mask->top = top;
}
