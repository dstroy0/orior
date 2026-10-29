// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// icons.c: the icons as the page's icons module draws them, each path in the path language of SVG, the moves, lines,
// cubic curves, arcs and closes, absolute and relative, and its dots as circles. A curve is flattened to short lines,
// and each line is laid as a capsule, the pixels within half the line's width of it covered by the distance left to
// its edge, the cover of a pixel the most any capsule gives it, which draws the line with round ends and joins; a dot
// is a capsule of no length. The cog is drawn from its teeth
#include "icons.h"

#include "../../cli/src/command_line/cli_internal.h"

#include <math.h>

static CellOwner s_owner = MMGR_CELL_OWNER("gui_icons");

// the pixels between the points a curve is flattened to, and the most parts an icon's path has
#define ICONS_STEP 0.5f
#define ICONS_PATHS 3u

// An icon's shape: its name, its paths, its dots as `fills` triples of center x, center y and radius, and whether it
// is the cog, drawn from its teeth
typedef struct
{
    const char *name;
    const char *path[ICONS_PATHS];
    const float *fill;
    unsigned int fills;
    int cog;
} GuiIconShape;

static const float s_more_dots[] = {3.5f, 8.0f, 1.2f, 8.0f, 8.0f, 1.2f, 12.5f, 8.0f, 1.2f};
static const float s_problems_dots[] = {8.0f, 11.1f, 0.9f};
static const float s_git_dots[] = {5.0f, 3.0f, 1.5f, 5.0f, 13.0f, 1.5f, 11.0f, 4.6f, 1.5f};
static const float s_render_dots[] = {10.6f, 6.1f, 1.1f};
static const float s_view_dots[] = {8.0f, 8.0f, 1.9f};

#define ICONS_BOX "M3.4 3h9.2A1.4 1.4 0 0 1 14 4.4v7.2a1.4 1.4 0 0 1-1.4 1.4H3.4A1.4 1.4 0 0 1 2 11.6V4.4A1.4 1.4 0 0 1 3.4 3z"
#define ICONS_LOCK_BODY "M4.6 7.4h6.8a.9.9 0 0 1 .9.9v4.6a.9.9 0 0 1-.9.9H4.6a.9.9 0 0 1-.9-.9V8.3a.9.9 0 0 1 .9-.9z"

static const GuiIconShape s_shapes[] = {
    {"folder", {"M2 4.6A1.6 1.6 0 0 1 3.6 3h2.8l1.6 1.6h4.4A1.6 1.6 0 0 1 14 6.2v5.2a1.6 1.6 0 0 1-1.6 1.6H3.6A1.6 1.6 0 0 1 2 11.4z", NULL, NULL}, NULL, 0u, 0},
    {"structure", {"M2.5 2.5h4v3h-4zM9.5 6.5h4v3h-4zM9.5 11h4v3h-4z", "M4.5 5.5v7h5M4.5 8h5", NULL}, NULL, 0u, 0},
    {"commit", {"M1.5 8h4M10.5 8h4", "M8 5.4a2.6 2.6 0 1 1 0 5.2a2.6 2.6 0 0 1 0-5.2z", NULL}, NULL, 0u, 0},
    {"more", {NULL, NULL, NULL}, s_more_dots, 3u, 0},
    {"run", {"M2 13.5h12", "M3.2 10.8 6.4 7.3l2.7 2.2 3.7-4.8", NULL}, NULL, 0u, 0},
    {"debug", {"M8 1.8l5.4 3.1v6.2L8 14.2l-5.4-3.1V4.9z", "M6.7 5.9v4.2L10.1 8z", NULL}, NULL, 0u, 0},
    {"terminal", {ICONS_BOX, "M5 6.4 7 8l-2 1.6M8.6 10h2.6", NULL}, NULL, 0u, 0},
    {"problems", {"M8 2a6 6 0 1 1 0 12A6 6 0 0 1 8 2z", "M8 4.9v3.8", NULL}, s_problems_dots, 1u, 0},
    {"git", {"M5 3.6v8.8", "M5 10c0-2.6 6-2.2 6-5", NULL}, s_git_dots, 3u, 0},
    {"profile", {"M2 13.5h12M3 10.5h8.5M4 7.5h5M5 4.5h2", NULL, NULL}, NULL, 0u, 0},
    {"containers", {"M2.5 5L8 2.5 13.5 5v6L8 13.5 2.5 11z", "M2.5 5L8 7.5 13.5 5M8 7.5v6", NULL}, NULL, 0u, 0},
    {"tests", {"M6.2 2h3.6", "M6.8 2v4.3L3.1 12.5A1 1 0 0 0 4 14h8a1 1 0 0 0 .9-1.5L9.2 6.3V2", "M4.7 10.4h6.6"}, NULL, 0u, 0},
    {"gear", {NULL, "M8 5.9a2.1 2.1 0 1 1 0 4.2a2.1 2.1 0 0 1 0-4.2z", NULL}, NULL, 0u, 1},
    {"bell", {"M4.2 11.2V7.6a3.8 3.8 0 0 1 7.6 0v3.6l1.2 1.4H3z", "M6.6 13.9a1.5 1.5 0 0 0 2.8 0", NULL}, NULL, 0u, 0},
    {"database", {"M3 4.2C3 3 5.2 2 8 2s5 1 5 2.2-2.2 2.2-5 2.2-5-1-5-2.2z", "M3 4.2v7.6C3 13 5.2 14 8 14s5-1 5-2.2V4.2", "M3 8c0 1.2 2.2 2.2 5 2.2s5-1 5-2.2"}, NULL, 0u, 0},
    {"comms", {"M8 6.3a1.7 1.7 0 1 1 0 3.4a1.7 1.7 0 0 1 0-3.4z", "M8 6.3V3.6M6.5 8.9l-2.3 1.4M9.5 8.9l2.3 1.4", "M8 1.6a1 1 0 1 1 0 2a1 1 0 0 1 0-2zM3.3 10a1 1 0 1 1 0 2a1 1 0 0 1 0-2zM12.7 10a1 1 0 1 1 0 2a1 1 0 0 1 0-2z"}, NULL, 0u, 0},
    {"lock", {ICONS_LOCK_BODY, "M5.9 7.4V5.6a2.1 2.1 0 0 1 4.2 0v1.8", NULL}, NULL, 0u, 0},
    {"unlock", {ICONS_LOCK_BODY, "M5.9 7.4V5.6a2.1 2.1 0 0 1 4-.9", NULL}, NULL, 0u, 0},
    {"chevron", {"M6 4l4 4-4 4", NULL, NULL}, NULL, 0u, 0},
    {"refresh", {"M13 8a5 5 0 1 1-1.5-3.6", "M13 2.4v3.2H9.8", NULL}, NULL, 0u, 0},
    {"undo", {"M5.5 4 2.5 7l3 3", "M2.8 7H10a3.5 3.5 0 0 1 0 7H7", NULL}, NULL, 0u, 0},
    {"pull", {"M8 2.5v8M4.8 7.6 8 10.8l3.2-3.2", "M3 13.5h10", NULL}, NULL, 0u, 0},
    {"push", {"M8 13.5v-8M4.8 8.4 8 5.2l3.2 3.2", "M3 2.5h10", NULL}, NULL, 0u, 0},
    {"minimize", {"M3.5 8h9", NULL, NULL}, NULL, 0u, 0},
    {"maximize", {"M4 4h8v8H4z", NULL, NULL}, NULL, 0u, 0},
    {"restore", {"M4 6h6v6H4z", "M6 6V4h6v6h-2", NULL}, NULL, 0u, 0},
    {"close", {"M4.2 4.2l7.6 7.6M11.8 4.2l-7.6 7.6", NULL, NULL}, NULL, 0u, 0},
    {"protocol", {"M3.5 2.5v11M12.5 2.5v11", "M3.5 5.5h7.4M9.4 4l1.5 1.5-1.5 1.5", "M12.5 10.5H5.1M6.6 9l-1.5 1.5 1.5 1.5"}, NULL, 0u, 0},
    {"ingest", {"M8 2.2v7.3M5 6.6l3 3 3-3", "M2.5 10.2v1.9a1.4 1.4 0 0 0 1.4 1.4h8.2a1.4 1.4 0 0 0 1.4-1.4v-1.9", NULL}, NULL, 0u, 0},
    {"render", {ICONS_BOX, "M2.4 11.4 6 7.8l2.6 2.6 1.6-1.6 3.4 3.4", NULL}, s_render_dots, 1u, 0},
    {"sim", {"M1.8 8.6C3.3 3.4 4.9 3.4 6.4 8.2s3.2 4.8 4.7.4c.9-2.6 2-3.4 3.1-2.6", NULL, NULL}, NULL, 0u, 0},
    {"pipeline", {"M1.5 6h3.2v4H1.5zM6.4 6h3.2v4H6.4zM11.3 6h3.2v4h-3.2z", "M4.7 8h1.7M9.6 8h1.7", NULL}, NULL, 0u, 0},
    {"stage", {"M8 2.3 14 5.3 8 8.3 2 5.3z", "M2 8.1l6 3 6-3", "M2 10.9l6 3 6-3"}, NULL, 0u, 0},
    {"runs", {"M1.5 8.5h2.8l1.6-4.2 2.7 7.4 1.6-3.2h4.3", NULL, NULL}, NULL, 0u, 0},
    {"search", {"M6.8 2.4a4.4 4.4 0 1 1 0 8.8a4.4 4.4 0 0 1 0-8.8z", "M10 10l3.9 3.9", NULL}, NULL, 0u, 0},
    {"program", {"M4.6 4.6h6.8v6.8H4.6z", "M6.6 2.2v2.4M9.4 2.2v2.4M6.6 11.4v2.4M9.4 11.4v2.4M2.2 6.6h2.4M2.2 9.4h2.4M11.4 6.6h2.4M11.4 9.4h2.4", NULL}, NULL, 0u, 0},
    {"launch", {"M2.6 3.6 7 8l-4.4 4.4", "M8.4 3.6 12.8 8l-4.4 4.4", NULL}, NULL, 0u, 0},
    {"view", {"M1.5 8C3.2 4.8 5.4 3.5 8 3.5s4.8 1.3 6.5 4.5c-1.7 3.2-3.9 4.5-6.5 4.5S3.2 11.2 1.5 8z", NULL, NULL}, s_view_dots, 1u, 0},
    {"check", {"M3.6 2.6h8.8a1 1 0 0 1 1 1v8.8a1 1 0 0 1-1 1H3.6a1 1 0 0 1-1-1V3.6a1 1 0 0 1 1-1z", "M5.4 8.3l1.9 1.9 3.5-4", NULL}, NULL, 0u, 0},
    {"hammer", {"M6.4 4.6 9.6 1.4l4.9 4.9-3.2 3.2z", "M8 6.2 2.2 12a1.1 1.1 0 0 0 1.6 1.6L9.6 7.8", NULL}, NULL, 0u, 0},
    {"wrench", {"M10.8 2.2a3.2 3.2 0 0 0-3.9 4.3L2.4 11a1.5 1.5 0 0 0 2.1 2.1l4.5-4.5a3.2 3.2 0 0 0 4.3-3.9l-2 2-1.9-.6-.6-1.9z", NULL, NULL}, NULL, 0u, 0},
};
#define ICONS_COUNT (sizeof(s_shapes) / sizeof(s_shapes[0]))

// The pen a path is drawn with: the cover it draws into, floats of 0 to 1 a pixel of the square, the square's side,
// the pixels a unit takes and the line's half width in pixels; where the pen stands and where its subpath began, in
// units; and the second control point of the last curve, for a smooth curve to mirror, where the last part was one
typedef struct
{
    float *cover;
    unsigned int side;
    float scale;
    float radius;
    float x;
    float y;
    float start_x;
    float start_y;
    float control_x;
    float control_y;
    int has_control;
} GuiPen;

// a capsule of `radius` pixels about the line from x0, y0 to x1, y1 in pixels laid on the cover: each pixel's share
// the distance left from its middle to the capsule's edge, clamped, where that is more than the pixel has; a capsule
// of no length is a disc
static void pen_capsule(GuiPen *pen, float x0, float y0, float x1, float y1, float radius)
{
    // the square's side is far inside what an int represents
    const int side = (int)pen->side;
    const int left = (int)floorf(((x0 < x1) ? x0 : x1) - radius - 1.0f);
    const int right = (int)ceilf(((x0 > x1) ? x0 : x1) + radius + 1.0f);
    const int top = (int)floorf(((y0 < y1) ? y0 : y1) - radius - 1.0f);
    const int bottom = (int)ceilf(((y0 > y1) ? y0 : y1) + radius + 1.0f);
    const float run_x = x1 - x0;
    const float run_y = y1 - y0;
    const float length_sq = (run_x * run_x) + (run_y * run_y);
    for (int y = (top < 0) ? 0 : top; (y <= bottom) && (y < side); y += 1)
    {
        for (int x = (left < 0) ? 0 : left; (x <= right) && (x < side); x += 1)
        {
            const float from_x = ((float)x + 0.5f) - x0;
            const float from_y = ((float)y + 0.5f) - y0;
            const float along = (length_sq > 0.0f) ? ((from_x * run_x) + (from_y * run_y)) / length_sq : 0.0f;
            const float clamped = (along < 0.0f) ? 0.0f : ((along > 1.0f) ? 1.0f : along);
            const float away = hypotf(from_x - (clamped * run_x), from_y - (clamped * run_y));
            const float share = radius + 0.5f - away;
            // the pixel lies on the square: its place is not negative
            float *const had = &pen->cover[((unsigned int)y * pen->side) + (unsigned int)x];
            if (share > *had)
            {
                *had = (share > 1.0f) ? 1.0f : share;
            }
        }
    }
}

// the pen moved to x, y in units, a subpath begun there
static void pen_moved(GuiPen *pen, float x, float y)
{
    pen->x = x;
    pen->y = y;
    pen->start_x = x;
    pen->start_y = y;
}

// a line drawn from the pen to x, y in units, the pen left at its end
static void pen_drawn(GuiPen *pen, float x, float y)
{
    pen_capsule(pen, pen->x * pen->scale, pen->y * pen->scale, x * pen->scale, y * pen->scale, pen->radius);
    pen->x = x;
    pen->y = y;
}

// a cubic curve drawn from the pen through its control points x1, y1 and x2, y2 to x, y, as lines between points
// along it
static void pen_curved(GuiPen *pen, float x1, float y1, float x2, float y2, float x, float y)
{
    const float x0 = pen->x;
    const float y0 = pen->y;
    const float length = (hypotf(x1 - x0, y1 - y0) + hypotf(x2 - x1, y2 - y1) + hypotf(x - x2, y - y2)) * pen->scale;
    // the points along a curve of an icon are far fewer than an unsigned int counts
    unsigned int steps = (unsigned int)ceilf(length / ICONS_STEP);
    steps = (steps < 4u) ? 4u : steps;
    for (unsigned int step = 1u; step <= steps; step += 1u)
    {
        const float along = (float)step / (float)steps;
        const float rest = 1.0f - along;
        const float weight_0 = rest * rest * rest;
        const float weight_1 = 3.0f * rest * rest * along;
        const float weight_2 = 3.0f * rest * along * along;
        const float weight_3 = along * along * along;
        pen_drawn(pen, (weight_0 * x0) + (weight_1 * x1) + (weight_2 * x2) + (weight_3 * x),
                  (weight_0 * y0) + (weight_1 * y1) + (weight_2 * y2) + (weight_3 * y));
    }
    pen->control_x = x2;
    pen->control_y = y2;
    pen->has_control = 1;
}

// the angle from the vector ux, uy to vx, vy, signed
static float pen_angle(float ux, float uy, float vx, float vy)
{
    const float dot = (ux * vx) + (uy * vy);
    const float lengths = hypotf(ux, uy) * hypotf(vx, vy);
    const float ratio = (lengths > 0.0f) ? dot / lengths : 1.0f;
    const float clamped = (ratio < -1.0f) ? -1.0f : ((ratio > 1.0f) ? 1.0f : ratio);
    const float angle = acosf(clamped);
    return (((ux * vy) - (uy * vx)) < 0.0f) ? -angle : angle;
}

// an elliptical arc drawn from the pen to x, y with radii rx, ry turned by `turn` degrees, the larger arc where
// `large` is set and the one that sweeps clockwise where `sweep` is, as the path language's arc, its center found
// from its ends and the arc walked in steps
static void pen_arced(GuiPen *pen, float rx, float ry, float turn, int large, int sweep, float x, float y)
{
    const float x1 = pen->x;
    const float y1 = pen->y;
    if ((rx == 0.0f) || (ry == 0.0f) || ((x1 == x) && (y1 == y)))
    {
        pen_drawn(pen, x, y);
        return;
    }
    const float radians = turn * 3.14159265f / 180.0f;
    const float cos_turn = cosf(radians);
    const float sin_turn = sinf(radians);
    const float half_x = (x1 - x) / 2.0f;
    const float half_y = (y1 - y) / 2.0f;
    const float x1_turned = (cos_turn * half_x) + (sin_turn * half_y);
    const float y1_turned = (-sin_turn * half_x) + (cos_turn * half_y);
    float wide = fabsf(rx);
    float tall = fabsf(ry);
    const float lambda = ((x1_turned * x1_turned) / (wide * wide)) + ((y1_turned * y1_turned) / (tall * tall));
    if (lambda > 1.0f)
    {
        wide *= sqrtf(lambda);
        tall *= sqrtf(lambda);
    }
    const float wide_sq = wide * wide;
    const float tall_sq = tall * tall;
    const float numerator = (wide_sq * tall_sq) - (wide_sq * y1_turned * y1_turned) - (tall_sq * x1_turned * x1_turned);
    const float denominator = (wide_sq * y1_turned * y1_turned) + (tall_sq * x1_turned * x1_turned);
    const float ratio = (denominator > 0.0f) ? numerator / denominator : 0.0f;
    const float root = sqrtf((ratio < 0.0f) ? 0.0f : ratio);
    const float coefficient = (large != sweep) ? root : -root;
    const float centre_x_turned = coefficient * wide * y1_turned / tall;
    const float centre_y_turned = -coefficient * tall * x1_turned / wide;
    const float centre_x = (cos_turn * centre_x_turned) - (sin_turn * centre_y_turned) + ((x1 + x) / 2.0f);
    const float centre_y = (sin_turn * centre_x_turned) + (cos_turn * centre_y_turned) + ((y1 + y) / 2.0f);
    const float from_x = (x1_turned - centre_x_turned) / wide;
    const float from_y = (y1_turned - centre_y_turned) / tall;
    const float to_x = (-x1_turned - centre_x_turned) / wide;
    const float to_y = (-y1_turned - centre_y_turned) / tall;
    const float start = pen_angle(1.0f, 0.0f, from_x, from_y);
    float sweep_angle = pen_angle(from_x, from_y, to_x, to_y);
    if (!sweep && (sweep_angle > 0.0f))
    {
        sweep_angle -= 2.0f * 3.14159265f;
    }
    else if (sweep && (sweep_angle < 0.0f))
    {
        sweep_angle += 2.0f * 3.14159265f;
    }
    const float longer = (wide > tall) ? wide : tall;
    // the steps around an arc of an icon are far fewer than an unsigned int counts
    unsigned int steps = (unsigned int)ceilf(fabsf(sweep_angle) * longer * pen->scale / ICONS_STEP);
    steps = (steps < 4u) ? 4u : steps;
    for (unsigned int step = 1u; step <= steps; step += 1u)
    {
        const float angle = start + (sweep_angle * (float)step / (float)steps);
        const float ellipse_x = wide * cosf(angle);
        const float ellipse_y = tall * sinf(angle);
        pen_drawn(pen, (cos_turn * ellipse_x) - (sin_turn * ellipse_y) + centre_x,
                  (sin_turn * ellipse_x) + (cos_turn * ellipse_y) + centre_y);
    }
    pen->x = x;
    pen->y = y;
}

// a number read from `*at` as the path language writes one, a sign, digits and a fraction, the separators before it
// passed over: 1 with the number into `value` and `*at` past it, or 0 where none stands there
static int icons_number(const char **at, float *value)
{
    const char *text = *at;
    while ((*text == ' ') || (*text == ','))
    {
        text += 1;
    }
    const int negative = *text == '-';
    if ((*text == '-') || (*text == '+'))
    {
        text += 1;
    }
    float whole = 0.0f;
    unsigned int digits = 0u;
    while ((*text >= '0') && (*text <= '9'))
    {
        whole = (whole * 10.0f) + (float)(*text - '0');
        text += 1;
        digits += 1u;
    }
    float fraction = 0.0f;
    if (*text == '.')
    {
        text += 1;
        float place = 0.1f;
        while ((*text >= '0') && (*text <= '9'))
        {
            fraction += (float)(*text - '0') * place;
            place /= 10.0f;
            text += 1;
            digits += 1u;
        }
    }
    if (digits == 0u)
    {
        return 0;
    }
    *value = negative ? -(whole + fraction) : (whole + fraction);
    *at = text;
    return 1;
}

// `count` numbers read from `*at` into `values`: 1 where all stood there
static int icons_numbers(const char **at, float *values, unsigned int count)
{
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (!icons_number(at, &values[each]))
        {
            return 0;
        }
    }
    return 1;
}

static int icons_letter(char byte)
{
    return ((byte >= 'A') && (byte <= 'Z')) || ((byte >= 'a') && (byte <= 'z'));
}

// one part of a path drawn by its command with the numbers that follow it: 1, or 0 where the numbers run out
static int pen_commanded(GuiPen *pen, char command, const char **at)
{
    const int relative = (command >= 'a') && (command <= 'z');
    const float base_x = relative ? pen->x : 0.0f;
    const float base_y = relative ? pen->y : 0.0f;
    const char upper = relative ? (char)(command - 'a' + 'A') : command;
    float values[7];
    if (upper == 'M')
    {
        if (!icons_numbers(at, values, 2u))
        {
            return 0;
        }
        pen_moved(pen, base_x + values[0], base_y + values[1]);
    }
    else if (upper == 'L')
    {
        if (!icons_numbers(at, values, 2u))
        {
            return 0;
        }
        pen_drawn(pen, base_x + values[0], base_y + values[1]);
    }
    else if (upper == 'H')
    {
        if (!icons_numbers(at, values, 1u))
        {
            return 0;
        }
        pen_drawn(pen, base_x + values[0], pen->y);
    }
    else if (upper == 'V')
    {
        if (!icons_numbers(at, values, 1u))
        {
            return 0;
        }
        pen_drawn(pen, pen->x, base_y + values[0]);
    }
    else if (upper == 'C')
    {
        if (!icons_numbers(at, values, 6u))
        {
            return 0;
        }
        pen_curved(pen, base_x + values[0], base_y + values[1], base_x + values[2], base_y + values[3], base_x + values[4],
                   base_y + values[5]);
        return 1;
    }
    else if (upper == 'S')
    {
        if (!icons_numbers(at, values, 4u))
        {
            return 0;
        }
        const float x1 = pen->has_control ? (2.0f * pen->x) - pen->control_x : pen->x;
        const float y1 = pen->has_control ? (2.0f * pen->y) - pen->control_y : pen->y;
        pen_curved(pen, x1, y1, base_x + values[0], base_y + values[1], base_x + values[2], base_y + values[3]);
        return 1;
    }
    else if (upper == 'A')
    {
        if (!icons_numbers(at, values, 7u))
        {
            return 0;
        }
        pen_arced(pen, values[0], values[1], values[2], values[3] != 0.0f, values[4] != 0.0f, base_x + values[5],
                  base_y + values[6]);
    }
    else
    {
        return 0;
    }
    pen->has_control = 0;
    return 1;
}

// the path `d` drawn: each command with the numbers that follow it, a command's numbers repeating it, the pairs after
// a move drawn as lines, and a close a line back to where the subpath began
static void pen_path(GuiPen *pen, const char *d)
{
    const char *at = d;
    char command = '\0';
    for (;;)
    {
        while ((*at == ' ') || (*at == ','))
        {
            at += 1;
        }
        if (*at == '\0')
        {
            return;
        }
        if (icons_letter(*at))
        {
            command = *at;
            at += 1;
            if ((command == 'Z') || (command == 'z'))
            {
                pen_drawn(pen, pen->start_x, pen->start_y);
                pen->has_control = 0;
                command = '\0';
                continue;
            }
        }
        if ((command == '\0') || !pen_commanded(pen, command, &at))
        {
            return;
        }
        // the pairs after a move are lines, relative where the move was
        command = (command == 'M') ? 'L' : ((command == 'm') ? 'l' : command);
    }
}

// the cog drawn: eight teeth around a wheel, each tooth's top a little narrower than its root
static void pen_cog(GuiPen *pen)
{
    const float outer = 6.6f;
    const float inner = 4.9f;
    const float step = (2.0f * 3.14159265f) / 8.0f;
    for (unsigned int tooth = 0u; tooth < 8u; tooth += 1u)
    {
        const float middle = (float)tooth * step;
        const float turns[4] = {middle - (step * 0.32f), middle - (step * 0.18f), middle + (step * 0.18f), middle + (step * 0.32f)};
        const float radii[4] = {inner, outer, outer, inner};
        for (unsigned int corner = 0u; corner < 4u; corner += 1u)
        {
            const float x = 8.0f + (radii[corner] * cosf(turns[corner]));
            const float y = 8.0f + (radii[corner] * sinf(turns[corner]));
            if ((tooth == 0u) && (corner == 0u))
            {
                pen_moved(pen, x, y);
            }
            else
            {
                pen_drawn(pen, x, y);
            }
        }
    }
    pen_drawn(pen, pen->start_x, pen->start_y);
}

// the shape named `name`, or NULL
static const GuiIconShape *icons_shape(const char *name, unsigned int *index)
{
    for (unsigned int at = 0u; at < ICONS_COUNT; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_shapes[at].name, .cap = 32ull}))
        {
            *index = at;
            return &s_shapes[at];
        }
    }
    return NULL;
}

int gui_icon_named(const char *name)
{
    unsigned int index = 0u;
    return icons_shape(name, &index) != NULL;
}

void gui_icons_made(GuiIcons *icons)
{
    memor.set(&(MemoriaCfg){.dst = icons, .val = 0, .bytes = sizeof(*icons)});
}

void gui_icons_given(GuiIcons *icons)
{
    for (unsigned int at = 0u; at < GUI_ICON_SLOTS; at += 1u)
    {
        if (icons->slot[at].used)
        {
            g_cli_cells->persistent_buf_release(icons->slot[at].mask.cover, &s_owner);
            icons->slot[at].mask.cover = NULL;
            icons->slot[at].used = 0;
        }
    }
    icons->kept = 0u;
}

// the shape `index` drawn at `size` with its line `stroke` wide into `mask`, the cover a cell of its own: 1, or 0
// where a cell could not be taken
static int icons_drawn(const GuiIconShape *shape, unsigned int size, float stroke, GuiMask *mask)
{
    const unsigned long long pixels = (unsigned long long)size * size;
    float *const floats = g_cli_cells->persistent_buf_alloc(pixels * sizeof(*floats), &s_owner);
    unsigned char *const cover = g_cli_cells->persistent_buf_alloc(pixels, &s_owner);
    if ((floats == NULL) || (cover == NULL))
    {
        g_cli_cells->persistent_buf_release(floats, &s_owner);
        g_cli_cells->persistent_buf_release(cover, &s_owner);
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = floats, .val = 0, .bytes = pixels * sizeof(*floats)});
    GuiPen pen;
    memor.set(&(MemoriaCfg){.dst = &pen, .val = 0, .bytes = sizeof(pen)});
    pen.cover = floats;
    pen.side = size;
    pen.scale = (float)size / GUI_ICON_UNITS;
    pen.radius = stroke / 2.0f;
    if (shape->cog)
    {
        pen_cog(&pen);
    }
    for (unsigned int part = 0u; part < ICONS_PATHS; part += 1u)
    {
        if (shape->path[part] != NULL)
        {
            pen_path(&pen, shape->path[part]);
        }
    }
    for (unsigned int dot = 0u; dot < shape->fills; dot += 1u)
    {
        const float *const fill = &shape->fill[dot * 3u];
        pen_capsule(&pen, fill[0] * pen.scale, fill[1] * pen.scale, fill[0] * pen.scale, fill[1] * pen.scale, fill[2] * pen.scale);
    }
    for (unsigned long long at = 0ull; at < pixels; at += 1ull)
    {
        // a coverage from 0 to 1 by 255, rounded, is a byte
        cover[at] = (unsigned char)((floats[at] * 255.0f) + 0.5f);
    }
    g_cli_cells->persistent_buf_release(floats, &s_owner);
    mask->width = size;
    mask->height = size;
    mask->left = 0;
    mask->top = 0;
    mask->cover = cover;
    return 1;
}

const GuiMask *gui_icon(GuiIcons *icons, const char *name, unsigned int size, float stroke)
{
    unsigned int index = 0u;
    const GuiIconShape *const shape = icons_shape(name, &index);
    if ((shape == NULL) || (size == 0u))
    {
        return NULL;
    }
    for (unsigned int at = 0u; at < GUI_ICON_SLOTS; at += 1u)
    {
        const GuiKeptIcon *const kept = &icons->slot[at];
        if (kept->used && (kept->icon == index) && (kept->size == size) && (kept->stroke == stroke))
        {
            return &kept->mask;
        }
    }
    if (icons->kept >= GUI_ICON_SLOTS)
    {
        gui_icons_given(icons);
    }
    GuiKeptIcon *const slot = &icons->slot[icons->kept];
    if (!icons_drawn(shape, size, stroke, &slot->mask))
    {
        return NULL;
    }
    slot->used = 1;
    slot->icon = index;
    slot->size = size;
    slot->stroke = stroke;
    icons->kept += 1u;
    return &slot->mask;
}
