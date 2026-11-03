// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// icons.c: the icons as the page's icons module draws them, each path in the path language of SVG, the moves, lines,
// cubic curves, arcs and closes, absolute and relative, and its dots as circles. A curve is flattened to short lines,
// and each line is laid as a capsule, the pixels within half the line's width of it covered by the distance left to
// its edge, the cover of a pixel the most any capsule gives it, which draws the line with round ends and joins; a dot
// is a capsule of no length. The cog is drawn from its teeth
#include "icons.h"

#include "../command_line/cli_internal.h"
#include "../text/text.h"

#include <math.h>

#define ICONS_PATHS 3u
// An icon's shape: its name, its paths, its dots as `fills` triples of center x, center y and radius, whether it is
// the cog, drawn from its teeth; its own box, left, top, width and height in its units, where it is not drawn on the
// square of sixteen, the box fitted to the square and centered; and a closed path filled whole, where it has one
typedef struct
{
    const char *name;
    const char *path[ICONS_PATHS];
    const float *fill;
    unsigned int fills;
    int cog;
    const float *box;
    const char *filled;
} GuiIconShape;
// The pen a path is drawn with: the cover it draws into, floats of 0 to 1 a pixel of the square, the square's side,
// the pixels a unit takes, the units the shape's box starts at and the pixels the box stands in from the square's
// top left, and the line's half width in pixels; where the pen stands and where its subpath began, in units; the
// second control point of the last curve, for a smooth curve to mirror, where the last part was one; and the
// corners gathered while a shape is filled, in pixels, in place of lines drawn, NULL while it draws
typedef struct
{
    float *cover;
    unsigned int side;
    float scale;
    float left;
    float top;
    float pad_x;
    float pad_y;
    float radius;
    float x;
    float y;
    float start_x;
    float start_y;
    float control_x;
    float control_y;
    int has_control;
    float *corner;
    unsigned int corners;
} GuiPen;
// the pixels between the points a curve is flattened to, and the most parts an icon's path has
#define ICONS_STEP 0.5f

// the eye of the wordmark, the page's line art: each stroke the center line of one line of the drawing, and the
// iris its one filled shape, in the drawing's own units within its box
static const char s_logo_strokes[] =
    "M164.8 30.5L164.2 32.8L164.5 65M136.5 42.2L140.5 68M191.5 42.8L188.5 63L188 67L188.2 68.2"
    "M113.2 55.2L113.8 58.2L119.8 74.8M215.5 55.8L208.5 75M231.5 71.2L225.7 82.2L224.5 83.5"
    "M96.8 71.5L103.1 82.8L103.2 84.2M83.8 87.2L89 93.8M244.8 87.2L239.2 93.8M256.2 101.2L252.8 105.2"
    "M72.8 102L75.8 105.5M164.8 106L164.5 107.5"
    "M164.5 107.5L162.4 109.2L141 146.5L140.6 148.2L141.3 149.3L147.8 149.5L149.8 150"
    "M164.5 107.5L166.6 109.2L187.1 144.8L188.5 148.8M164.8 127L164.5 127.8"
    "M164.2 128L162.4 129.8L153.9 144.5L152.9 146.5L152.5 148.8"
    "M164.8 128L166.4 129.5L173.7 142L176.2 146.8L176.8 149M77.8 136.5L76.2 138.1L64.8 145.8L61 147.2"
    "M250.2 136.5L251.5 137.9L262 145L268.5 148.8"
    "M60.8 147L61.9 143.9L65.5 139.5L71 133.5L83.2 121.8L97.2 110.5L104.2 105.7L113 100.6L121 96.4L128 93.4L141.5 89.1"
    "L147.5 87.7L160 86L172 86L184 87.6L196.2 90.9L203.8 93.6L210 96.5L217.8 100.6L226 105.7L240.8 116.9L253.4 128.8"
    "L267.6 144.5L268.7 147L268.8 148.8"
    "M60.5 147.2L59.2 150.8M152.2 149L150 150M152.5 149L155 149.5L176.5 149.2"
    "M188.2 149L187 149.5L181.2 149.5L179.2 150M188.8 149L189.8 149.2M268.8 149L269 150.8"
    "M75.5 149.2L64.8 150.8L59.5 151M177 149.2L178.5 149.5L179 150M252.5 149.2L263.2 150.8L268.8 151"
    "M149.8 150.2L148.9 153.2L141.2 166.5L140.6 168.5L141.1 169.6L187.9 169.6L188.4 168.5L187.8 166.5L180.2 "
    "153.5L179.2 150.2"
    "M59.2 151.2L59.2 152.5L59.7 154L61.8 156.2M269 151.2L269.2 151.8M272.2 151.8L269.5 152"
    "M269.2 152.2L268.8 154M268.5 154.2L263 155.4L251.5 159.8M268.8 154.5L267.9 157.5L265.8 160.2"
    "M62.2 156.2L63.8 155.5L65.2 155.7L75.5 159.4L76.8 159.5M62 156.5L63 159.2L65 161.2"
    "M265.2 160.2L262.5 161.1L253.2 167.7L241 175.2L230 180.8L215.8 186.6L199 191.4L197.5 192.2"
    "M265.5 160.5L264.9 162.8L263.8 164.5L251.8 177.1L240.5 187.1L227.5 196.6L214.5 204.2L202.5 209.7L189 214L182.5 "
    "215.1L181 214.8"
    "M65.5 161.2L68.8 162.3L84.8 172.1L96.2 178.1L113.2 185.1L125.2 188.8L128.8 190.2"
    "M65.2 161.5L65.7 163.2L66.9 165.2L78.8 177.2L88.5 185.8L103 196.6L115.5 204L127.8 209.6L140.5 213.7L142.8 "
    "214.1L144.5 213.8"
    "M38.8 168.5L43.8 174L52.5 182.6L53.8 185M290 169L277.1 183.2L276 185.5M53.5 185.2L50 189.5"
    "M54 185.2L56.8 186.5L69.2 197.2L70.5 199.2M275.8 185.8L273 187.2L261.8 196.9L260.4 198.2L259.8 199.8"
    "M276 185.8L279.8 189.5M129 190.5L131 190.4L141.5 192.6L144.2 192"
    "M128.8 190.8L129.5 194.5L132.8 200.5L137.2 206L143.2 211L144.8 213.5"
    "M197 192.2L195.2 192.2L184.2 193.8L181 193.2"
    "M197.2 192.5L196.5 196L191.9 203.2L187.5 207.8L181.8 211.9L180 214.5M70.2 199.5L69.2 201.2L64 207.8"
    "M70.8 199.5L72.8 200.1L74.2 201.1L87 210L88.5 211.8"
    "M259.5 200L258 200.3L256.2 201.3L243 210.5L241.2 212.2M259.8 200L265.8 207.8M146 200.8L150 204.8"
    "M180.2 201L176.5 204.8M88.8 212L91.8 213L109 222.5M88.5 212.2L87.5 214.8L80.2 227.2"
    "M241 212.5L238.8 213.2L229 218.6L222.5 221.7L220.8 223.2M241.2 212.5L249.2 227.2"
    "M145 213.8L153.5 215.9L159.2 216.8L170.5 216.5L173.5 216.2L180 214.5"
    "M181.2 214.2L181.5 214L181.5 214.5M180 214.5L181 214.8"
    "M109.2 222.8L112 223.4L126 228.7L133.8 230.8L135.2 231.8M109 223L108.3 225.8L100.2 248.2"
    "M220.5 223.5L217.5 224L205 228.6L197 230.7L194.5 232M220.8 223.8L227.7 243.5L228.9 246.5L229.8 247.5"
    "M135.2 231.8L139 231.9L142 232.6L151.5 234L162.5 234.6L164.2 235.2M135.2 231.8L130.5 263.2"
    "M194.5 232L192.8 231.8L180.8 233.8L166.8 234.8L164.5 235.2M194.5 232L199 263"
    "M164.5 235.5L164.2 276.5L164.8 278.8";
static const char s_logo_iris[] = "M163 209.4L158.8 208.9L155.5 207.9L151.2 205.4L145.4 199.5L142.6 194.8L145.2 "
                                  "192.1L159 193.6L176.2 193.6L180.2 193.1"
                                  "L183.1 196L180.9 199.8L175.2 205.4L173 206.9L167.5 208.9L163 209.4Z";
static const float s_logo_box[4] = {36.8f, 28.5f, 255.2f, 252.2f};
// the eye's line in its units, as the page strokes it
#define ICONS_EYE_STROKE 2.2f

static const float s_more_dots[] = {3.5f, 8.0f, 1.2f, 8.0f, 8.0f, 1.2f, 12.5f, 8.0f, 1.2f};
static const float s_problems_dots[] = {8.0f, 11.1f, 0.9f};
static const float s_git_dots[] = {5.0f, 3.0f, 1.5f, 5.0f, 13.0f, 1.5f, 11.0f, 4.6f, 1.5f};
static const float s_render_dots[] = {10.6f, 6.1f, 1.1f};
static const float s_view_dots[] = {8.0f, 8.0f, 1.9f};

#define ICONS_BOX                                                                                                      \
    "M3.4 3h9.2A1.4 1.4 0 0 1 14 4.4v7.2a1.4 1.4 0 0 1-1.4 1.4H3.4A1.4 1.4 0 0 1 2 11.6V4.4A1.4 1.4 0 0 1 3.4 3z"
#define ICONS_LOCK_BODY "M4.6 7.4h6.8a.9.9 0 0 1 .9.9v4.6a.9.9 0 0 1-.9.9H4.6a.9.9 0 0 1-.9-.9V8.3a.9.9 0 0 1 .9-.9z"

static const GuiIconShape s_shapes[] = {
    {"folder",
     {"M2 4.6A1.6 1.6 0 0 1 3.6 3h2.8l1.6 1.6h4.4A1.6 1.6 0 0 1 14 6.2v5.2a1.6 1.6 0 0 1-1.6 1.6H3.6A1.6 1.6 0 0 1 2 "
      "11.4z",
      NULL, NULL},
     NULL,
     0u,
     0},
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
    {"tests",
     {"M6.2 2h3.6", "M6.8 2v4.3L3.1 12.5A1 1 0 0 0 4 14h8a1 1 0 0 0 .9-1.5L9.2 6.3V2", "M4.7 10.4h6.6"},
     NULL,
     0u,
     0},
    {"gear", {NULL, "M8 5.9a2.1 2.1 0 1 1 0 4.2a2.1 2.1 0 0 1 0-4.2z", NULL}, NULL, 0u, 1},
    {"bell", {"M4.2 11.2V7.6a3.8 3.8 0 0 1 7.6 0v3.6l1.2 1.4H3z", "M6.6 13.9a1.5 1.5 0 0 0 2.8 0", NULL}, NULL, 0u, 0},
    {"database",
     {"M3 4.2C3 3 5.2 2 8 2s5 1 5 2.2-2.2 2.2-5 2.2-5-1-5-2.2z", "M3 4.2v7.6C3 13 5.2 14 8 14s5-1 5-2.2V4.2",
      "M3 8c0 1.2 2.2 2.2 5 2.2s5-1 5-2.2"},
     NULL,
     0u,
     0},
    {"comms",
     {"M8 6.3a1.7 1.7 0 1 1 0 3.4a1.7 1.7 0 0 1 0-3.4z", "M8 6.3V3.6M6.5 8.9l-2.3 1.4M9.5 8.9l2.3 1.4",
      "M8 1.6a1 1 0 1 1 0 2a1 1 0 0 1 0-2zM3.3 10a1 1 0 1 1 0 2a1 1 0 0 1 0-2zM12.7 10a1 1 0 1 1 0 2a1 1 0 0 1 0-2z"},
     NULL,
     0u,
     0},
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
    {"protocol",
     {"M3.5 2.5v11M12.5 2.5v11", "M3.5 5.5h7.4M9.4 4l1.5 1.5-1.5 1.5", "M12.5 10.5H5.1M6.6 9l-1.5 1.5 1.5 1.5"},
     NULL,
     0u,
     0},
    {"ingest",
     {"M8 2.2v7.3M5 6.6l3 3 3-3", "M2.5 10.2v1.9a1.4 1.4 0 0 0 1.4 1.4h8.2a1.4 1.4 0 0 0 1.4-1.4v-1.9", NULL},
     NULL,
     0u,
     0},
    {"render", {ICONS_BOX, "M2.4 11.4 6 7.8l2.6 2.6 1.6-1.6 3.4 3.4", NULL}, s_render_dots, 1u, 0},
    {"sim", {"M1.8 8.6C3.3 3.4 4.9 3.4 6.4 8.2s3.2 4.8 4.7.4c.9-2.6 2-3.4 3.1-2.6", NULL, NULL}, NULL, 0u, 0},
    {"pipeline", {"M1.5 6h3.2v4H1.5zM6.4 6h3.2v4H6.4zM11.3 6h3.2v4h-3.2z", "M4.7 8h1.7M9.6 8h1.7", NULL}, NULL, 0u, 0},
    {"stage", {"M8 2.3 14 5.3 8 8.3 2 5.3z", "M2 8.1l6 3 6-3", "M2 10.9l6 3 6-3"}, NULL, 0u, 0},
    {"runs", {"M1.5 8.5h2.8l1.6-4.2 2.7 7.4 1.6-3.2h4.3", NULL, NULL}, NULL, 0u, 0},
    {"search", {"M6.8 2.4a4.4 4.4 0 1 1 0 8.8a4.4 4.4 0 0 1 0-8.8z", "M10 10l3.9 3.9", NULL}, NULL, 0u, 0},
    {"program",
     {"M4.6 4.6h6.8v6.8H4.6z",
      "M6.6 2.2v2.4M9.4 2.2v2.4M6.6 11.4v2.4M9.4 11.4v2.4M2.2 6.6h2.4M2.2 9.4h2.4M11.4 6.6h2.4M11.4 9.4h2.4", NULL},
     NULL,
     0u,
     0},
    {"launch", {"M2.6 3.6 7 8l-4.4 4.4", "M8.4 3.6 12.8 8l-4.4 4.4", NULL}, NULL, 0u, 0},
    {"view",
     {"M1.5 8C3.2 4.8 5.4 3.5 8 3.5s4.8 1.3 6.5 4.5c-1.7 3.2-3.9 4.5-6.5 4.5S3.2 11.2 1.5 8z", NULL, NULL},
     s_view_dots,
     1u,
     0},
    {"check",
     {"M3.6 2.6h8.8a1 1 0 0 1 1 1v8.8a1 1 0 0 1-1 1H3.6a1 1 0 0 1-1-1V3.6a1 1 0 0 1 1-1z", "M5.4 8.3l1.9 1.9 3.5-4",
      NULL},
     NULL,
     0u,
     0},
    {"hammer", {"M6.4 4.6 9.6 1.4l4.9 4.9-3.2 3.2z", "M8 6.2 2.2 12a1.1 1.1 0 0 0 1.6 1.6L9.6 7.8", NULL}, NULL, 0u, 0},
    {"wrench",
     {"M10.8 2.2a3.2 3.2 0 0 0-3.9 4.3L2.4 11a1.5 1.5 0 0 0 2.1 2.1l4.5-4.5a3.2 3.2 0 0 0 4.3-3.9l-2 2-1.9-.6-.6-1.9z",
      NULL, NULL},
     NULL,
     0u,
     0},
    {"eye", {s_logo_strokes, NULL, NULL}, NULL, 0u, 0, s_logo_box, s_logo_iris},
};
#define ICONS_COUNT (sizeof(s_shapes) / sizeof(s_shapes[0]))

// the most corners a filled shape has, and the samples across and down a pixel its cover is read from
#define ICONS_CORNERS 256u
#define ICONS_SAMPLES 4u

typedef struct
{
    const GuiPen *pen_read;
    float x;
    float y;
    GuiPen *pen;
    float x0;
    float y0;
    float x1;
    float y1;
    float radius;
    float x2;
    float y2;
    float ux;
    float uy;
    float vx;
    float vy;
    float rx;
    float ry;
    float turn;
    int large;
    int sweep;
    const char **at;
    float *value;
    float *values;
    unsigned int count;
    char byte;
    char command;
    const char *d;
    const char *name;
    unsigned int *index;
    const GuiIconShape *shape;
    unsigned int size;
    float stroke;
    GuiMask *mask_bits;
} IconsCtx;
// a place in units as pixels of the square across and down
EMBED_INLINE float pen_px(const IconsCtx *args)
{
    return ((args->x - args->pen_read->left) * args->pen_read->scale) + args->pen_read->pad_x;
}

EMBED_INLINE float pen_py(const IconsCtx *args)
{
    return ((args->y - args->pen_read->top) * args->pen_read->scale) + args->pen_read->pad_y;
}

// a capsule of `radius` pixels about the line from x0, y0 to x1, y1 in pixels laid on the cover: each pixel's share
// the distance left from its middle to the capsule's edge, clamped, where that is more than the pixel has; a capsule
// of no length is a disc
EMBED_INLINE void pen_capsule(const IconsCtx *args)
{
    // the square's side is far inside what an int represents
    const int side = (int)args->pen->side;
    const int left = (int)floorf(((args->x0 < args->x1) ? args->x0 : args->x1) - args->radius - 1.0f);
    const int right = (int)ceilf(((args->x0 > args->x1) ? args->x0 : args->x1) + args->radius + 1.0f);
    const int top = (int)floorf(((args->y0 < args->y1) ? args->y0 : args->y1) - args->radius - 1.0f);
    const int bottom = (int)ceilf(((args->y0 > args->y1) ? args->y0 : args->y1) + args->radius + 1.0f);
    const float run_x = args->x1 - args->x0;
    const float run_y = args->y1 - args->y0;
    const float length_sq = (run_x * run_x) + (run_y * run_y);
    for (int y = (top < 0) ? 0 : top; (y <= bottom) && (y < side); y += 1)
    {
        for (int x = (left < 0) ? 0 : left; (x <= right) && (x < side); x += 1)
        {
            const float from_x = ((float)x + 0.5f) - args->x0;
            const float from_y = ((float)y + 0.5f) - args->y0;
            const float along = (length_sq > 0.0f) ? ((from_x * run_x) + (from_y * run_y)) / length_sq : 0.0f;
            float clamped;
            if (along < 0.0f)
            {
                clamped = 0.0f;
            }
            else if (along > 1.0f)
            {
                clamped = 1.0f;
            }
            else
            {
                clamped = along;
            }
            const float away = hypotf(from_x - (clamped * run_x), from_y - (clamped * run_y));
            const float share = args->radius + 0.5f - away;
            // the pixel lies on the square: its place is not negative
            float *const had = &args->pen->cover[((unsigned int)y * args->pen->side) + (unsigned int)x];
            if (share > *had)
            {
                *had = (share > 1.0f) ? 1.0f : share;
            }
        }
    }
}

// a corner gathered at x, y in units while a shape is filled
EMBED_INLINE void pen_corner(const IconsCtx *args)
{
    if (args->pen->corners < ICONS_CORNERS)
    {
        args->pen->corner[args->pen->corners * 2u] = EMBED_CALL(pen_px, IconsCtx, .pen_read = args->pen, .x = args->x);
        args->pen->corner[(args->pen->corners * 2u) + 1u] =
            EMBED_CALL(pen_py, IconsCtx, .pen_read = args->pen, .y = args->y);
        args->pen->corners += 1u;
    }
}

// the pen moved to x, y in units, a subpath begun there
EMBED_INLINE void pen_move(const IconsCtx *args)
{
    float x = args->x;
    float y = args->y;
    args->pen->x = x;
    args->pen->y = y;
    args->pen->start_x = x;
    args->pen->start_y = y;
    if (args->pen->corner != NULL)
    {
        EMBED_CALL(pen_corner, IconsCtx, .pen = args->pen, .x = x, .y = y);
    }
}

// a line drawn from the pen to x, y in units, the pen left at its end; a corner gathered in its place while a shape
// is filled
EMBED_INLINE void pen_draw(const IconsCtx *args)
{
    float x = args->x;
    float y = args->y;
    if (args->pen->corner != NULL)
    {
        EMBED_CALL(pen_corner, IconsCtx, .pen = args->pen, .x = x, .y = y);
    }
    else
    {
        EMBED_CALL(pen_capsule, IconsCtx, .pen = args->pen,
                   .x0 = EMBED_CALL(pen_px, IconsCtx, .pen_read = args->pen, .x = args->pen->x),
                   .y0 = EMBED_CALL(pen_py, IconsCtx, .pen_read = args->pen, .y = args->pen->y),
                   .x1 = EMBED_CALL(pen_px, IconsCtx, .pen_read = args->pen, .x = x),
                   .y1 = EMBED_CALL(pen_py, IconsCtx, .pen_read = args->pen, .y = y), .radius = args->pen->radius);
    }
    args->pen->x = x;
    args->pen->y = y;
}

void gui_icons_polygon_fill(const GuiIconsCoverCfg *args)
{
    if (args->corners < 3u)
    {
        return;
    }
    float min_y = args->corner[1];
    float max_y = args->corner[1];
    for (unsigned int at = 1u; at < args->corners; at += 1u)
    {
        const float y = args->corner[(at * 2u) + 1u];
        min_y = (y < min_y) ? y : min_y;
        max_y = (y > max_y) ? y : max_y;
    }
    unsigned char *const counts = g_cli_cells->temporary_buf_alloc(args->width, &g_gui_icons_cells);
    if (counts == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = counts, .val = 0, .bytes = args->width});
    // the cover's sides are far inside what an int represents, and a sample's column past them is clipped
    const int pixel_height = (int)args->height;
    const int samples_width = (int)args->width * (int)ICONS_SAMPLES;
    const int top = (int)floorf(min_y);
    const int bottom = (int)ceilf(max_y);
    const float step = 1.0f / (float)ICONS_SAMPLES;
    const unsigned int saved = (args->corners < ICONS_CORNERS) ? args->corners : ICONS_CORNERS;
    for (int y = (top < 0) ? 0 : top; (y <= bottom) && (y < pixel_height); y += 1)
    {
        int seen_from = samples_width;
        int seen_to = 0;
        for (unsigned int row = 0u; row < ICONS_SAMPLES; row += 1u)
        {
            const float sample_y = (float)y + (((float)row + 0.5f) * step);
            // the edges this row of samples crosses, in order across, each with the way it winds
            float cross_x[ICONS_CORNERS];
            int cross_way[ICONS_CORNERS];
            unsigned int crossings = 0u;
            for (unsigned int at = 0u; at < saved; at += 1u)
            {
                const unsigned int next = (at + 1u) % saved;
                const float x0 = args->corner[at * 2u];
                const float y0 = args->corner[(at * 2u) + 1u];
                const float x1 = args->corner[next * 2u];
                const float y1 = args->corner[(next * 2u) + 1u];
                if ((y0 > sample_y) != (y1 > sample_y))
                {
                    const float x = x0 + ((sample_y - y0) * (x1 - x0) / (y1 - y0));
                    unsigned int place = crossings;
                    while ((place > 0u) && (cross_x[place - 1u] > x))
                    {
                        cross_x[place] = cross_x[place - 1u];
                        cross_way[place] = cross_way[place - 1u];
                        place -= 1u;
                    }
                    cross_x[place] = x;
                    cross_way[place] = (y1 > y0) ? 1 : -1;
                    crossings += 1u;
                }
            }
            // each run the winding holds off zero counted across the samples it covers, a whole pixel at a time inside
            // it
            int winding = 0;
            float run_from = 0.0f;
            for (unsigned int at = 0u; at < crossings; at += 1u)
            {
                const int before = winding;
                winding += cross_way[at];
                if ((before == 0) && (winding != 0))
                {
                    run_from = cross_x[at];
                }
                else if ((before != 0) && (winding == 0))
                {
                    // a sample stands at the middle of its fraction of a pixel: the first at or past each end
                    const int first = (int)ceilf((run_from * (float)ICONS_SAMPLES) - 0.5f);
                    const int end = (int)ceilf((cross_x[at] * (float)ICONS_SAMPLES) - 0.5f);
                    int sample = (first < 0) ? 0 : first;
                    const int stop = (end > samples_width) ? samples_width : end;
                    seen_from = ((sample < stop) && (sample < seen_from)) ? sample : seen_from;
                    seen_to = ((sample < stop) && (stop > seen_to)) ? stop : seen_to;
                    while (sample < stop)
                    {
                        const int whole =
                            (((sample % (int)ICONS_SAMPLES) == 0) && (sample + (int)ICONS_SAMPLES <= stop))
                                ? (int)ICONS_SAMPLES
                                : 1;
                        // a pixel's count of its samples is a small count, and the sample lies on the row
                        counts[(unsigned int)sample / ICONS_SAMPLES] =
                            (unsigned char)(counts[(unsigned int)sample / ICONS_SAMPLES] + whole);
                        sample += whole;
                    }
                }
            }
        }
        // the pixels this row touched take their shares, where those are more than they have
        const int pixels_to = (seen_to + (int)ICONS_SAMPLES - 1) / (int)ICONS_SAMPLES;
        for (int x = seen_from / (int)ICONS_SAMPLES; x < pixels_to; x += 1)
        {
            // the pixel lies on the cover: its place is not negative
            const unsigned int count = counts[(unsigned int)x];
            counts[(unsigned int)x] = 0u;
            float *const had = &args->cover[((unsigned int)y * args->width) + (unsigned int)x];
            const float share = (float)count / (float)(ICONS_SAMPLES * ICONS_SAMPLES);
            if (share > *had)
            {
                *had = share;
            }
        }
    }
    g_cli_cells->persistent_buf_release(counts, &g_gui_icons_cells);
}

// a cubic curve drawn from the pen through its control points x1, y1 and x2, y2 to x, y, as lines between points
// along it
EMBED_INLINE void pen_curve(const IconsCtx *args)
{
    const float x0 = args->pen->x;
    const float y0 = args->pen->y;
    const float length = (hypotf(args->x1 - x0, args->y1 - y0) + hypotf(args->x2 - args->x1, args->y2 - args->y1) +
                          hypotf(args->x - args->x2, args->y - args->y2)) *
                         args->pen->scale;
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
        EMBED_CALL(pen_draw, IconsCtx, .pen = args->pen,
                   .x = (weight_0 * x0) + (weight_1 * args->x1) + (weight_2 * args->x2) + (weight_3 * args->x),
                   .y = (weight_0 * y0) + (weight_1 * args->y1) + (weight_2 * args->y2) + (weight_3 * args->y));
    }
    args->pen->control_x = args->x2;
    args->pen->control_y = args->y2;
    args->pen->has_control = 1;
}

// the angle from the vector ux, uy to vx, vy, signed
EMBED_INLINE float pen_angle(const IconsCtx *args)
{
    const float dot = (args->ux * args->vx) + (args->uy * args->vy);
    const float lengths = hypotf(args->ux, args->uy) * hypotf(args->vx, args->vy);
    const float ratio = (lengths > 0.0f) ? dot / lengths : 1.0f;
    float clamped;
    if (ratio < -1.0f)
    {
        clamped = -1.0f;
    }
    else if (ratio > 1.0f)
    {
        clamped = 1.0f;
    }
    else
    {
        clamped = ratio;
    }
    const float angle = acosf(clamped);
    return (((args->ux * args->vy) - (args->uy * args->vx)) < 0.0f) ? -angle : angle;
}

// an elliptical arc drawn from the pen to x, y with radii rx, ry turned by `turn` degrees, the larger arc where
// `large` is set and the one that sweeps clockwise where `sweep` is, as the path language's arc, its center found
// from its ends and the arc walked in steps
EMBED_INLINE void pen_arc(const IconsCtx *args)
{
    float x = args->x;
    float y = args->y;
    const float x1 = args->pen->x;
    const float y1 = args->pen->y;
    if ((args->rx == 0.0f) || (args->ry == 0.0f) || ((x1 == x) && (y1 == y)))
    {
        EMBED_CALL(pen_draw, IconsCtx, .pen = args->pen, .x = x, .y = y);
        return;
    }
    const float radians = args->turn * 3.14159265f / 180.0f;
    const float cos_turn = cosf(radians);
    const float sin_turn = sinf(radians);
    const float half_x = (x1 - x) / 2.0f;
    const float half_y = (y1 - y) / 2.0f;
    const float x1_turned = (cos_turn * half_x) + (sin_turn * half_y);
    const float y1_turned = (-sin_turn * half_x) + (cos_turn * half_y);
    float pixel_width = fabsf(args->rx);
    float pixel_height = fabsf(args->ry);
    const float lambda = ((x1_turned * x1_turned) / (pixel_width * pixel_width)) +
                         ((y1_turned * y1_turned) / (pixel_height * pixel_height));
    if (lambda > 1.0f)
    {
        pixel_width *= sqrtf(lambda);
        pixel_height *= sqrtf(lambda);
    }
    const float width_sq = pixel_width * pixel_width;
    const float height_sq = pixel_height * pixel_height;
    const float numerator =
        (width_sq * height_sq) - (width_sq * y1_turned * y1_turned) - (height_sq * x1_turned * x1_turned);
    const float denominator = (width_sq * y1_turned * y1_turned) + (height_sq * x1_turned * x1_turned);
    const float ratio = (denominator > 0.0f) ? numerator / denominator : 0.0f;
    const float root = sqrtf((ratio < 0.0f) ? 0.0f : ratio);
    const float coefficient = (args->large != args->sweep) ? root : -root;
    const float centre_x_turned = coefficient * pixel_width * y1_turned / pixel_height;
    const float centre_y_turned = -coefficient * pixel_height * x1_turned / pixel_width;
    const float centre_x = (cos_turn * centre_x_turned) - (sin_turn * centre_y_turned) + ((x1 + x) / 2.0f);
    const float centre_y = (sin_turn * centre_x_turned) + (cos_turn * centre_y_turned) + ((y1 + y) / 2.0f);
    const float from_x = (x1_turned - centre_x_turned) / pixel_width;
    const float from_y = (y1_turned - centre_y_turned) / pixel_height;
    const float to_x = (-x1_turned - centre_x_turned) / pixel_width;
    const float to_y = (-y1_turned - centre_y_turned) / pixel_height;
    const float start = EMBED_CALL(pen_angle, IconsCtx, .ux = 1.0f, .uy = 0.0f, .vx = from_x, .vy = from_y);
    float sweep_angle = EMBED_CALL(pen_angle, IconsCtx, .ux = from_x, .uy = from_y, .vx = to_x, .vy = to_y);
    if (!args->sweep && (sweep_angle > 0.0f))
    {
        sweep_angle -= 2.0f * 3.14159265f;
    }
    else if (args->sweep && (sweep_angle < 0.0f))
    {
        sweep_angle += 2.0f * 3.14159265f;
    }
    const float longer = (pixel_width > pixel_height) ? pixel_width : pixel_height;
    // the steps around an arc of an icon are far fewer than an unsigned int counts
    unsigned int steps = (unsigned int)ceilf(fabsf(sweep_angle) * longer * args->pen->scale / ICONS_STEP);
    steps = (steps < 4u) ? 4u : steps;
    for (unsigned int step = 1u; step <= steps; step += 1u)
    {
        const float angle = start + (sweep_angle * (float)step / (float)steps);
        const float ellipse_x = pixel_width * cosf(angle);
        const float ellipse_y = pixel_height * sinf(angle);
        EMBED_CALL(pen_draw, IconsCtx, .pen = args->pen,
                   .x = (cos_turn * ellipse_x) - (sin_turn * ellipse_y) + centre_x,
                   .y = (sin_turn * ellipse_x) + (cos_turn * ellipse_y) + centre_y);
    }
    args->pen->x = x;
    args->pen->y = y;
}

// a number read from `*at` as the path language writes one, a sign, digits and a fraction, the separators before it
// passed over: 1 with the number into `value` and `*at` past it, or 0 where none stands there
EMBED_INLINE int icons_number(const IconsCtx *args)
{
    const char **at = args->at;
    float *value = args->value;
    const char *text = *at;
    text += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), .set = " ,");
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
EMBED_INLINE int icons_numbers(const IconsCtx *args)
{
    for (unsigned int each = 0u; each < args->count; each += 1u)
    {
        if (!EMBED_CALL(icons_number, IconsCtx, .at = args->at, .value = &args->values[each]))
        {
            return 0;
        }
    }
    return 1;
}

EMBED_INLINE int icons_letter(const IconsCtx *args)
{
    return ((args->byte >= 'A') && (args->byte <= 'Z')) || ((args->byte >= 'a') && (args->byte <= 'z'));
}

// one part of a path drawn by its command with the numbers that follow it: 1, or 0 where the numbers run out
EMBED_INLINE int pen_command(const IconsCtx *args)
{
    const int relative = (args->command >= 'a') && (args->command <= 'z');
    const float base_x = relative ? args->pen->x : 0.0f;
    const float base_y = relative ? args->pen->y : 0.0f;
    const char upper = relative ? (char)(args->command - 'a' + 'A') : args->command;
    float values[7];
    if (upper == 'M')
    {
        if (!EMBED_CALL(icons_numbers, IconsCtx, .at = args->at, .values = values, .count = 2u))
        {
            return 0;
        }
        EMBED_CALL(pen_move, IconsCtx, .pen = args->pen, .x = base_x + values[0], .y = base_y + values[1]);
    }
    else if (upper == 'L')
    {
        if (!EMBED_CALL(icons_numbers, IconsCtx, .at = args->at, .values = values, .count = 2u))
        {
            return 0;
        }
        EMBED_CALL(pen_draw, IconsCtx, .pen = args->pen, .x = base_x + values[0], .y = base_y + values[1]);
    }
    else if (upper == 'H')
    {
        if (!EMBED_CALL(icons_numbers, IconsCtx, .at = args->at, .values = values, .count = 1u))
        {
            return 0;
        }
        EMBED_CALL(pen_draw, IconsCtx, .pen = args->pen, .x = base_x + values[0], .y = args->pen->y);
    }
    else if (upper == 'V')
    {
        if (!EMBED_CALL(icons_numbers, IconsCtx, .at = args->at, .values = values, .count = 1u))
        {
            return 0;
        }
        EMBED_CALL(pen_draw, IconsCtx, .pen = args->pen, .x = args->pen->x, .y = base_y + values[0]);
    }
    else if (upper == 'C')
    {
        if (!EMBED_CALL(icons_numbers, IconsCtx, .at = args->at, .values = values, .count = 6u))
        {
            return 0;
        }
        EMBED_CALL(pen_curve, IconsCtx, .pen = args->pen, .x1 = base_x + values[0], .y1 = base_y + values[1],
                   .x2 = base_x + values[2], .y2 = base_y + values[3], .x = base_x + values[4],
                   .y = base_y + values[5]);
        return 1;
    }
    else if (upper == 'S')
    {
        if (!EMBED_CALL(icons_numbers, IconsCtx, .at = args->at, .values = values, .count = 4u))
        {
            return 0;
        }
        const float x1 = args->pen->has_control ? (2.0f * args->pen->x) - args->pen->control_x : args->pen->x;
        const float y1 = args->pen->has_control ? (2.0f * args->pen->y) - args->pen->control_y : args->pen->y;
        EMBED_CALL(pen_curve, IconsCtx, .pen = args->pen, .x1 = x1, .y1 = y1, .x2 = base_x + values[0],
                   .y2 = base_y + values[1], .x = base_x + values[2], .y = base_y + values[3]);
        return 1;
    }
    else if (upper == 'A')
    {
        if (!EMBED_CALL(icons_numbers, IconsCtx, .at = args->at, .values = values, .count = 7u))
        {
            return 0;
        }
        EMBED_CALL(pen_arc, IconsCtx, .pen = args->pen, .rx = values[0], .ry = values[1], .turn = values[2],
                   .large = values[3] != 0.0f, .sweep = values[4] != 0.0f, .x = base_x + values[5],
                   .y = base_y + values[6]);
    }
    else
    {
        return 0;
    }
    args->pen->has_control = 0;
    return 1;
}

// the path `d` drawn: each command with the numbers that follow it, a command's numbers repeating it, the pairs after
// a move drawn as lines, and a close a line back to where the subpath began
EMBED_INLINE void pen_path(const IconsCtx *args)
{
    const char *at = args->d;
    char command = '\0';
    for (;;)
    {
        at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                         .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}), .set = " ,");
        if (*at == '\0')
        {
            return;
        }
        if (EMBED_CALL(icons_letter, IconsCtx, .byte = *at))
        {
            command = *at;
            at += 1;
            if ((command == 'Z') || (command == 'z'))
            {
                EMBED_CALL(pen_draw, IconsCtx, .pen = args->pen, .x = args->pen->start_x, .y = args->pen->start_y);
                args->pen->has_control = 0;
                command = '\0';
                continue;
            }
        }
        if ((command == '\0') || !EMBED_CALL(pen_command, IconsCtx, .pen = args->pen, .command = command, .at = &at))
        {
            return;
        }
        // the pairs after a move are lines, relative where the move was
        if (command == 'M')
        {
            command = 'L';
        }
        else if (command == 'm')
        {
            command = 'l';
        }
        else
        {
            command = command;
        }
    }
}

// the cog drawn: eight teeth around a wheel, each tooth's top a little narrower than its root
EMBED_INLINE void pen_cog(const IconsCtx *args)
{
    const float outer = 6.6f;
    const float inner = 4.9f;
    const float step = (2.0f * 3.14159265f) / 8.0f;
    for (unsigned int tooth = 0u; tooth < 8u; tooth += 1u)
    {
        const float middle = (float)tooth * step;
        const float turns[4] = {middle - (step * 0.32f), middle - (step * 0.18f), middle + (step * 0.18f),
                                middle + (step * 0.32f)};
        const float radii[4] = {inner, outer, outer, inner};
        for (unsigned int corner = 0u; corner < 4u; corner += 1u)
        {
            const float x = 8.0f + (radii[corner] * cosf(turns[corner]));
            const float y = 8.0f + (radii[corner] * sinf(turns[corner]));
            if ((tooth == 0u) && (corner == 0u))
            {
                EMBED_CALL(pen_move, IconsCtx, .pen = args->pen, .x = x, .y = y);
            }
            else
            {
                EMBED_CALL(pen_draw, IconsCtx, .pen = args->pen, .x = x, .y = y);
            }
        }
    }
    EMBED_CALL(pen_draw, IconsCtx, .pen = args->pen, .x = args->pen->start_x, .y = args->pen->start_y);
}

// the shape named `name`, or NULL
EMBED_INLINE const GuiIconShape *icons_shape(const IconsCtx *args)
{
    unsigned int *index = args->index;
    for (unsigned int at = 0u; at < ICONS_COUNT; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_shapes[at].name, .cap = 32ULL}))
        {
            *index = at;
            return &s_shapes[at];
        }
    }
    return NULL;
}

int gui_icon_exists(const GuiIconsCfg *args)
{
    unsigned int index = 0u;
    return EMBED_CALL(icons_shape, IconsCtx, .name = args->name, .index = &index) != NULL;
}

void gui_icons_init(const GuiIconsCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->icons, .val = 0, .bytes = sizeof(*args->icons)});
}

void gui_icons_free(const GuiIconsCfg *args)
{
    for (unsigned int at = 0u; at < GUI_ICON_CACHE_ENTRIES; at += 1u)
    {
        if (args->icons->entry[at].used)
        {
            g_cli_cells->persistent_buf_release(args->icons->entry[at].mask_bits.cover, &g_gui_icons_cells);
            args->icons->entry[at].mask_bits.cover = NULL;
            args->icons->entry[at].used = 0;
        }
    }
    args->icons->count = 0u;
}

// the shape `index` drawn at `size` with its line `stroke` wide into `mask`, the cover a cell of its own: 1, or 0
// where a cell could not be taken
EMBED_INLINE int icons_draw(const IconsCtx *args)
{
    const unsigned long long pixels = (unsigned long long)args->size * args->size;
    float *const floats = g_cli_cells->persistent_buf_alloc(pixels * sizeof(*floats), &g_gui_icons_cells);
    unsigned char *const cover = g_cli_cells->persistent_buf_alloc(pixels, &g_gui_icons_cells);
    if ((floats == NULL) || (cover == NULL))
    {
        g_cli_cells->persistent_buf_release(floats, &g_gui_icons_cells);
        g_cli_cells->persistent_buf_release(cover, &g_gui_icons_cells);
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = floats, .val = 0, .bytes = pixels * sizeof(*floats)});
    GuiPen pen;
    memor.set(&(MemoriaCfg){.dst = &pen, .val = 0, .bytes = sizeof(pen)});
    pen.cover = floats;
    pen.side = args->size;
    // a shape on its own box is fitted to the square by the box's longer side and centered along its shorter
    // a shape on no box of its own stands on the icon's square
    static const float s_square[4] = {0.0f, 0.0f, GUI_ICON_UNITS, GUI_ICON_UNITS};
    const float *const box = (args->shape->box != NULL) ? args->shape->box : s_square;
    const float box_width = box[2];
    const float box_height = box[3];
    const float longer = (box_width > box_height) ? box_width : box_height;
    pen.scale = (float)args->size / longer;
    pen.left = box[0];
    pen.top = box[1];
    pen.pad_x = (longer - box_width) * pen.scale / 2.0f;
    pen.pad_y = (longer - box_height) * pen.scale / 2.0f;
    pen.radius = args->stroke / 2.0f;
    if (args->shape->cog)
    {
        EMBED_CALL(pen_cog, IconsCtx, .pen = &pen);
    }
    for (unsigned int part = 0u; part < ICONS_PATHS; part += 1u)
    {
        if (args->shape->path[part] != NULL)
        {
            EMBED_CALL(pen_path, IconsCtx, .pen = &pen, .d = args->shape->path[part]);
        }
    }
    for (unsigned int dot = 0u; dot < args->shape->fills; dot += 1u)
    {
        const float *const fill = &args->shape->fill[dot * 3u];
        EMBED_CALL(pen_capsule, IconsCtx, .pen = &pen,
                   .x0 = EMBED_CALL(pen_px, IconsCtx, .pen_read = &pen, .x = fill[0]),
                   .y0 = EMBED_CALL(pen_py, IconsCtx, .pen_read = &pen, .y = fill[1]),
                   .x1 = EMBED_CALL(pen_px, IconsCtx, .pen_read = &pen, .x = fill[0]),
                   .y1 = EMBED_CALL(pen_py, IconsCtx, .pen_read = &pen, .y = fill[1]), .radius = fill[2] * pen.scale);
    }
    if (args->shape->filled != NULL)
    {
        float corners[ICONS_CORNERS * 2u];
        pen.corner = corners;
        pen.corners = 0u;
        EMBED_CALL(pen_path, IconsCtx, .pen = &pen, .d = args->shape->filled);
        EMBED_CALL(gui_icons_cover.polygon_fill, GuiIconsCoverCfg, .cover = pen.cover, .width = pen.side,
                   .height = pen.side, .corner = pen.corner, .corners = pen.corners);
        pen.corner = NULL;
    }
    for (unsigned long long at = 0ULL; at < pixels; at += 1ULL)
    {
        // a coverage from 0 to 1 by 255, rounded, is a byte
        cover[at] = (unsigned char)((floats[at] * 255.0f) + 0.5f);
    }
    g_cli_cells->persistent_buf_release(floats, &g_gui_icons_cells);
    args->mask_bits->width = args->size;
    args->mask_bits->height = args->size;
    args->mask_bits->left = 0;
    args->mask_bits->top = 0;
    args->mask_bits->cover = cover;
    return 1;
}

const GuiMask *gui_icon_get(const GuiIconsCfg *args)
{
    unsigned int size = args->size;
    float stroke = args->stroke;
    unsigned int index = 0u;
    const GuiIconShape *const shape = EMBED_CALL(icons_shape, IconsCtx, .name = args->name, .index = &index);
    if ((shape == NULL) || (size == 0u))
    {
        return NULL;
    }
    for (unsigned int at = 0u; at < GUI_ICON_CACHE_ENTRIES; at += 1u)
    {
        const GuiIconCacheEntry *const entry = &args->icons->entry[at];
        if (entry->used && (entry->icon == index) && (entry->size == size) && (entry->stroke == stroke))
        {
            return &entry->mask_bits;
        }
    }
    if (args->icons->count >= GUI_ICON_CACHE_ENTRIES)
    {
        cli_icons.free(args);
    }
    GuiIconCacheEntry *const entry = &args->icons->entry[args->icons->count];
    if (!EMBED_CALL(icons_draw, IconsCtx, .shape = shape, .size = size, .stroke = stroke,
                    .mask_bits = &entry->mask_bits))
    {
        return NULL;
    }
    entry->used = 1;
    entry->icon = index;
    entry->size = size;
    entry->stroke = stroke;
    args->icons->count += 1u;
    return &entry->mask_bits;
}
