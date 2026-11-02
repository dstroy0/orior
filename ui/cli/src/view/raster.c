// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.c: the pixels the window draws and all that is drawn into them. Every blend is of bytes in the colors' own
// space, a share from 0 to 255 of the color over what is there, as the page blends its colors, two channels at a
// time in one word; a pixel drawn is left opaque. A dot is put down as the page's shader puts the lattice's down: each
// pixel's share of it read in one step from how far the pixel's middle stands from the dot's edge, and the result
// dithered by a 4 by 4 ordered threshold below one level of 255, so a share between two levels is put down as both
#include "raster.h"

#include "../command_line/cli_internal.h"

#include <math.h>

int gui_rect_empty(const GuiRasterRectCfg *args)
{
    return (args->rect.width <= 0) || (args->rect.height <= 0);
}

int gui_rect_holds(const GuiRasterRectCfg *args)
{
    return (args->x >= args->rect.x) && (args->y >= args->rect.y) && (args->x < args->rect.x + args->rect.width) &&
           (args->y < args->rect.y + args->rect.height);
}

GuiRect gui_rect_intersect(const GuiRasterOneCfg *args)
{
    const int left = (args->one.x > args->other.x) ? args->one.x : args->other.x;
    const int top = (args->one.y > args->other.y) ? args->one.y : args->other.y;
    const int one_right = args->one.x + args->one.width;
    const int other_right = args->other.x + args->other.width;
    const int one_bottom = args->one.y + args->one.height;
    const int other_bottom = args->other.y + args->other.height;
    const int right = (one_right < other_right) ? one_right : other_right;
    const int bottom = (one_bottom < other_bottom) ? one_bottom : other_bottom;
    const GuiRect met = {left, top, right - left, bottom - top};
    return met;
}

// A notch read: the hole it cuts in the work, the radius its turns are rounded by, no more than half its shorter
// side, and each of its four corners, top left, top right, bottom left and bottom right: its point, whether it
// stands on the work's left or right edge and on its top or bottom edge, and the way into the hole from it
typedef struct
{
    GuiRect hole;
    unsigned int reach;
    int corner_x[4];
    int corner_y[4];
    int on_x[4];
    int on_y[4];
    int into_x[4];
    int into_y[4];
} RasterNotch;

typedef struct
{
    const GuiRaster *raster_read;
    GuiColor background_color;
    GuiColor color;
    unsigned int share;
    GuiRaster *raster;
    int x;
    int y;
    unsigned int reach;
    unsigned int across;
    unsigned int down;
    int at;
    int centre;
    int centre_x_value;
    int centre_y_value;
    int column;
    int row;
    GuiRect rect;
    unsigned int radius;
    GuiUnder background_fill;
    GuiColor text_color;
    unsigned int alpha;
    int toward_x;
    int toward_y;
    GuiRect work;
    GuiRect notch_rect;
    RasterNotch *read;
    const RasterNotch *notch;
    unsigned int corner;
    int *centre_x;
    int *centre_y;
    int cover;
    const GuiOutline *lines;
    int *pixel_width;
    GuiRect inside;
    GuiUnder table;
    GuiUnder clear;
    int across_i32;
    int width_value;
    int from;
    int to;
    const GuiRect *holes;
    unsigned int count;
    unsigned int background_u32;
    unsigned int color_u32;
    float share_f32;
    float threshold;
} GuiRasterCtx;
// the raster's own rectangle
EMBED_INLINE GuiRect raster_rect(const GuiRasterCtx *args)
{
    // a raster's sides are far inside what an int represents
    const GuiRect rect = {0, 0, (int)args->raster_read->width, (int)args->raster_read->height};
    return rect;
}

unsigned int gui_raster_pitch(const GuiRasterWidthCfg *args)
{
    return (args->width + (GUI_RASTER_PITCH - 1u)) & ~(GUI_RASTER_PITCH - 1u);
}

int gui_raster_init(const GuiRasterWidthXCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    const unsigned long long bytes = (unsigned long long)width * height * sizeof(GuiColor);
    args->raster->width = width;
    args->raster->height = height;
    args->raster->pages = NULL;
    args->raster->pages_bytes = 0ull;
    args->raster->positioned = 0ull;
    args->raster->pixels = g_cli_cells->persistent_buf_alloc(bytes, &g_gui_raster_cells);
    if (args->raster->pixels == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = args->raster->pixels, .val = 0, .bytes = bytes});
    return 1;
}

void gui_raster_free(const GuiRasterRectColorCfg *args)
{
    g_cli_cells->persistent_buf_release(args->raster->pixels, &g_gui_raster_cells);
    args->raster->pixels = NULL;
    args->raster->width = 0u;
    args->raster->height = 0u;
}

// `color` over `background_fill` by `share` of 255, the pixel left opaque: red and blue blended in one word, each in
// its own sixteen bits, and green in another, two multiplies a pixel; each lane's sum is the color's share plus the
// ground's rest plus half of 255, below 65536, and the lane divided by 255 exactly by adding its own high byte
// before the shift, which the raster's check holds over every ground, color and share
EMBED_INLINE GuiColor raster_blend(const GuiRasterCtx *args)
{
    if (args->share >= 255u)
    {
        return args->color | 0xff000000u;
    }
    const unsigned int rest = 255u - args->share;
    const unsigned int red_blue =
        ((args->color & 0x00ff00ffu) * args->share) + ((args->background_color & 0x00ff00ffu) * rest) + 0x00800080u;
    const unsigned int green =
        (((args->color >> 8u) & 0xffu) * args->share) + (((args->background_color >> 8u) & 0xffu) * rest) + 0x80u;
    const unsigned int red_blue_out = ((red_blue + ((red_blue >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu;
    const unsigned int green_out = ((green + (green >> 8u)) >> 8u) & 0xffu;
    return 0xff000000u | red_blue_out | (green_out << 8u);
}

// the pixel at x, y of a rectangle known to lie on the raster blended with `color` by `share` of 255
EMBED_INLINE void raster_append(const GuiRasterCtx *args)
{
    // the pixel lies on the raster: its coordinates are not negative
    GuiColor *const at = &args->raster->pixels[((unsigned int)args->y * args->raster->width) + (unsigned int)args->x];
    *at = EMBED_CALL(raster_blend, GuiRasterCtx, .background_color = *at, .color = args->color, .share = args->share);
}

void gui_raster_fill(const GuiRasterRectColorCfg *args)
{
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->rect,
                                  .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    const unsigned int alpha = GUI_ALPHA_OF(args->color);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on) || (alpha == 0u))
    {
        return;
    }
    // the rectangle lies on the raster: its coordinates are not negative
    GuiColor *row = &args->raster->pixels[((unsigned int)on.y * args->raster->width) + (unsigned int)on.x];
    if (alpha >= 255u)
    {
        const GuiColor whole = args->color | 0xff000000u;
        for (int y = 0; y < on.height; y += 1)
        {
            for (int x = 0; x < on.width; x += 1)
            {
                row[x] = whole;
            }
            row += args->raster->width;
        }
        return;
    }
    // the color's lanes by its share, with half of 255 in each, read once for the whole fill, as raster_blend lays
    // them; each pixel adds the ground's lanes by the rest
    const unsigned int rest = 255u - alpha;
    const unsigned int color_red_blue = ((args->color & 0x00ff00ffu) * alpha) + 0x00800080u;
    const unsigned int color_green = (((args->color >> 8u) & 0xffu) * alpha) + 0x80u;
    for (int y = 0; y < on.height; y += 1)
    {
        for (int x = 0; x < on.width; x += 1)
        {
            const GuiColor background_fill = row[x];
            const unsigned int red_blue = color_red_blue + ((background_fill & 0x00ff00ffu) * rest);
            const unsigned int green = color_green + (((background_fill >> 8u) & 0xffu) * rest);
            const unsigned int red_blue_out = ((red_blue + ((red_blue >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu;
            const unsigned int green_out = ((green + (green >> 8u)) >> 8u) & 0xffu;
            row[x] = 0xff000000u | red_blue_out | (green_out << 8u);
        }
        row += args->raster->width;
    }
}

// the reaches an arc's table is saved for, past every corner a frame rounds at any scale, and the tables, each made
// once: a table's rows and columns the whole pixels from the arc's center, each entry the pixel's share of 255 inside
// the arc
#define RASTER_ARC_MOST 47u
static unsigned char s_arc[RASTER_ARC_MOST + 1u][RASTER_ARC_MOST + 1u][RASTER_ARC_MOST + 1u];
static unsigned char s_arc_created[RASTER_ARC_MOST + 1u];

// the share of 255 inside an arc of `reach` of the pixel `across` and `down` whole pixels from its center: the distance
// left from the pixel's middle to the arc, clamped, by 255, rounded
EMBED_INLINE unsigned int raster_arc_work(const GuiRasterCtx *args)
{
    const float inside = (float)args->reach + 0.5f - hypotf((float)args->across + 0.5f, (float)args->down + 0.5f);
    const float saved = (inside < 0.0f) ? 0.0f : ((inside > 1.0f) ? 1.0f : inside);
    // a share of 255 by a factor from 0 to 1, rounded, is a byte
    return (unsigned int)((255.0f * saved) + 0.5f);
}

unsigned int gui_raster_arc_cover(const GuiRasterReachCfg *args)
{
    if (args->reach > RASTER_ARC_MOST)
    {
        return EMBED_CALL(raster_arc_work, GuiRasterCtx, .reach = args->reach, .across = args->across,
                          .down = args->down);
    }
    if ((args->across > args->reach) || (args->down > args->reach))
    {
        return 0u;
    }
    if (!s_arc_created[args->reach])
    {
        for (unsigned int row = 0u; row <= args->reach; row += 1u)
        {
            for (unsigned int column = 0u; column <= args->reach; column += 1u)
            {
                // a share of 255 is a byte
                s_arc[args->reach][row][column] = (unsigned char)EMBED_CALL(
                    raster_arc_work, GuiRasterCtx, .reach = args->reach, .across = column, .down = row);
            }
        }
        s_arc_created[args->reach] = 1u;
    }
    return s_arc[args->reach][args->down][args->across];
}

// the whole pixels from `centre` to the pixel `at` along one side, on either side of it
EMBED_INLINE unsigned int raster_arc_off(const GuiRasterCtx *args)
{
    // the difference is not negative on the side it is taken for
    return (args->at >= args->centre) ? (unsigned int)(args->at - args->centre)
                                      : (unsigned int)(args->centre - args->at - 1);
}

// the share of 255 of the pixel at `column`, `row` inside the arc of `reach` about the pixel corner `centre_x`,
// `centre_y`
EMBED_INLINE unsigned int raster_arc_at(const GuiRasterCtx *args)
{
    unsigned int reach = args->reach;
    return EMBED_CALL(
        gui_raster_reach.arc_cover, GuiRasterReachCfg, .reach = reach,
        .across = EMBED_CALL(raster_arc_off, GuiRasterCtx, .at = args->column, .centre = args->centre_x_value),
        .down = EMBED_CALL(raster_arc_off, GuiRasterCtx, .at = args->row, .centre = args->centre_y_value));
}

// how much of the pixel at x, y of `rect` lies inside its corners' arcs of `radius`, a share of 255: whole away from
// the corners, and in a corner's square as the arc's table has it
EMBED_INLINE unsigned int raster_corner_cover(const GuiRasterCtx *args)
{
    // the corners' squares are radius by radius, which an int holds
    const int side = (int)args->radius;
    const int left = args->rect.x + side;
    const int right = args->rect.x + args->rect.width - side;
    const int top = args->rect.y + side;
    const int bottom = args->rect.y + args->rect.height - side;
    const int in_left = args->x < left;
    const int in_top = args->y < top;
    if ((!in_left && (args->x < right)) || (!in_top && (args->y < bottom)))
    {
        return 255u;
    }
    return EMBED_CALL(raster_arc_at, GuiRasterCtx, .centre_x_value = in_left ? left : right,
                      .centre_y_value = in_top ? top : bottom, .reach = args->radius, .column = args->x,
                      .row = args->y);
}

void gui_raster_round(const GuiRasterRectColorCfg *args)
{
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->rect,
                                  .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    const unsigned int alpha = GUI_ALPHA_OF(args->color);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on) || (alpha == 0u))
    {
        return;
    }
    if (args->radius == 0u)
    {
        gui_raster_rect_color.fill(args);
        return;
    }
    // the rows between the corners' squares are filled whole through the fill's own loop; the rows through the
    // squares pixel by pixel, each by its share inside the arcs
    const int side = (int)args->radius;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = (GuiRect){args->rect.x, args->rect.y + side, args->rect.width, args->rect.height - (2 * side)},
               .color = args->color);
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        if ((y >= args->rect.y + side) && (y < args->rect.y + args->rect.height - side))
        {
            continue;
        }
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            const unsigned int cover = EMBED_CALL(raster_corner_cover, GuiRasterCtx, .rect = args->rect,
                                                  .radius = args->radius, .x = x, .y = y);
            if (cover != 0u)
            {
                // two shares of 255 multiplied are a share of 255 once divided by 255
                EMBED_CALL(raster_append, GuiRasterCtx, .raster = args->raster, .x = x, .y = y, .color = args->color,
                           .share = ((alpha * cover) + 127u) / 255u);
            }
        }
    }
}

GuiColor gui_color_over(const GuiRasterCfg *args)
{
    return EMBED_CALL(raster_blend, GuiRasterCtx, .background_color = args->background_color, .color = args->color,
                      .share = GUI_ALPHA_OF(args->color));
}

// the ground's own pixel at x, y: the ground raster's where one is given and holds the pixel, else the ground's color
EMBED_INLINE GuiColor raster_under_at(const GuiRasterCtx *args)
{
    int x = args->x;
    int y = args->y;
    if ((args->background_fill.raster == NULL) || (args->background_fill.raster->pixels == NULL) ||
        !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                    .rect = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->background_fill.raster), .x = x,
                    .y = y))
    {
        return args->background_fill.color;
    }
    // the pixel lies on the ground raster: its coordinates are not negative
    return args->background_fill.raster
        ->pixels[((unsigned int)y * args->background_fill.raster->width) + (unsigned int)x];
}

// the alpha the ground's grain gives the pixel at x, y, 0 where it has none; the coordinates are not negative
EMBED_INLINE unsigned int raster_grain_at(const GuiRasterCtx *args)
{
    if ((args->background_fill.grain == NULL) || (args->background_fill.tile == 0u))
    {
        return 0u;
    }
    return args->background_fill
        .grain[(((unsigned int)args->y % args->background_fill.tile) * args->background_fill.tile) +
               ((unsigned int)args->x % args->background_fill.tile)];
}

// the ground's color at x, y, a pixel on the raster: the ground's own pixel with the color over it, and the grain's
// white over that where the ground has a grain
EMBED_INLINE GuiColor raster_background_at(const GuiRasterCtx *args)
{
    const GuiColor tinted =
        EMBED_CALL(raster_blend, GuiRasterCtx, .background_color = raster_under_at(args),
                   .color = args->background_fill.over, .share = GUI_ALPHA_OF(args->background_fill.over));
    const unsigned int grain = raster_grain_at(args);
    return (grain == 0u) ? tinted
                         : EMBED_CALL(raster_blend, GuiRasterCtx, .background_color = tinted,
                                      .color = GUI_RGB(0xffu, 0xffu, 0xffu), .share = grain);
}

// a pixel of a pane on the table `background_fill` at x, y: the ground's own pixel with the ink at `alpha`, and the
// grain's white over that
EMBED_INLINE GuiColor raster_pane_at(const GuiRasterCtx *args)
{
    const GuiColor tinted = EMBED_CALL(raster_blend, GuiRasterCtx, .background_color = raster_under_at(args),
                                       .color = args->text_color, .share = args->alpha);
    const unsigned int grain = raster_grain_at(args);
    return (grain == 0u) ? tinted
                         : EMBED_CALL(raster_blend, GuiRasterCtx, .background_color = tinted,
                                      .color = GUI_RGB(0xffu, 0xffu, 0xffu), .share = grain);
}

void gui_raster_pane(const GuiRasterRectRadiusBackgroundCfg *args)
{
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->rect,
                                  .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
    {
        return;
    }
    const int outer_alpha = (int)GUI_ALPHA_OF(args->background_fill.over);
    const int alpha_span = (int)GUI_ALPHA_OF(args->inner) - outer_alpha;
    // the corners' squares are radius by radius, which an int holds
    const int side = (int)args->radius;
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        const int through_corners = (y < args->rect.y + side) || (y >= args->rect.y + args->rect.height - side);
        if (args->corners_only && !through_corners)
        {
            continue;
        }
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            const int in_corner =
                through_corners && ((x < args->rect.x + side) || (x >= args->rect.x + args->rect.width - side));
            if (args->corners_only && !in_corner)
            {
                continue;
            }
            // the cover is a share of 255 and a byte's span by it, rounded, comes back between the two bytes
            const int cover = in_corner ? (int)EMBED_CALL(raster_corner_cover, GuiRasterCtx, .rect = args->rect,
                                                          .radius = args->radius, .x = x, .y = y)
                                        : 255;
            const unsigned int alpha =
                (unsigned int)(outer_alpha + (((alpha_span * cover) + ((alpha_span < 0) ? -127 : 127)) / 255));
            // the pixel lies on the raster: its coordinates are not negative
            args->raster->pixels[((unsigned int)y * args->raster->width) + (unsigned int)x] =
                EMBED_CALL(raster_pane_at, GuiRasterCtx, .background_fill = args->background_fill, .x = x, .y = y,
                           .text_color = args->inner, .alpha = alpha);
        }
    }
}

void gui_raster_corners_cut(const GuiRasterRectRadiusBackgroundCfg *args)
{
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->rect,
                                  .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on) || (args->radius == 0u))
    {
        return;
    }
    // the corners' squares, each radius by radius, are where the arcs run
    const int reach = (int)args->radius;
    const GuiRect squares[4] = {
        {args->rect.x, args->rect.y, reach, reach},
        {args->rect.x + args->rect.width - reach, args->rect.y, reach, reach},
        {args->rect.x, args->rect.y + args->rect.height - reach, reach, reach},
        {args->rect.x + args->rect.width - reach, args->rect.y + args->rect.height - reach, reach, reach}};
    for (unsigned int corner = 0u; corner < 4u; corner += 1u)
    {
        const GuiRect square =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = squares[corner], .other = on);
        for (int y = square.y; y < square.y + square.height; y += 1)
        {
            for (int x = square.x; x < square.x + square.width; x += 1)
            {
                const unsigned int outside = 255u - EMBED_CALL(raster_corner_cover, GuiRasterCtx, .rect = args->rect,
                                                               .radius = args->radius, .x = x, .y = y);
                if (outside != 0u)
                {
                    EMBED_CALL(raster_append, GuiRasterCtx, .raster = args->raster, .x = x, .y = y,
                               .color = EMBED_CALL(raster_background_at, GuiRasterCtx,
                                                   .background_fill = args->background_fill, .x = x, .y = y),
                               .share = outside);
                }
            }
        }
    }
}

// `notch` read as a hole in `work`, its turns rounded by `radius`: 1, or 0 where nothing of it lies on the work
EMBED_INLINE int raster_notch_read(const GuiRasterCtx *args)
{
    const GuiRect hole =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->notch_rect, .other = args->work);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = hole))
    {
        return 0;
    }
    // the radius is no more than half the hole's sides, where the turns would meet
    const unsigned int most = (unsigned int)(((hole.width < hole.height) ? hole.width : hole.height) / 2);
    args->read->hole = hole;
    args->read->reach = (args->radius > most) ? most : args->radius;
    const int left_edge = hole.x <= args->work.x;
    const int right_edge = hole.x + hole.width >= args->work.x + args->work.width;
    const int top_edge = hole.y <= args->work.y;
    const int bottom_edge = hole.y + hole.height >= args->work.y + args->work.height;
    const int corner_x[4] = {hole.x, hole.x + hole.width, hole.x, hole.x + hole.width};
    const int corner_y[4] = {hole.y, hole.y, hole.y + hole.height, hole.y + hole.height};
    const int on_x[4] = {left_edge, right_edge, left_edge, right_edge};
    const int on_y[4] = {top_edge, top_edge, bottom_edge, bottom_edge};
    const int into_x[4] = {1, -1, 1, -1};
    const int into_y[4] = {1, 1, -1, -1};
    for (unsigned int corner = 0u; corner < 4u; corner += 1u)
    {
        args->read->corner_x[corner] = corner_x[corner];
        args->read->corner_y[corner] = corner_y[corner];
        args->read->on_x[corner] = on_x[corner];
        args->read->on_y[corner] = on_y[corner];
        args->read->into_x[corner] = into_x[corner];
        args->read->into_y[corner] = into_y[corner];
    }
    return 1;
}

// the square of the fillet at the notch's `corner`, inside the hole at the corner, and the arc's center, the far
// corner of the square
EMBED_INLINE GuiRect raster_fillet_square(const GuiRasterCtx *args)
{
    int *centre_x = args->centre_x;
    int *centre_y = args->centre_y;
    // the square's side is the radius, which an int holds
    const int side = (int)args->notch->reach;
    const GuiRect square = {(args->notch->into_x[args->corner] < 0) ? args->notch->corner_x[args->corner] - side
                                                                    : args->notch->corner_x[args->corner],
                            (args->notch->into_y[args->corner] < 0) ? args->notch->corner_y[args->corner] - side
                                                                    : args->notch->corner_y[args->corner],
                            side, side};
    *centre_x = args->notch->corner_x[args->corner] + (args->notch->into_x[args->corner] * side);
    *centre_y = args->notch->corner_y[args->corner] + (args->notch->into_y[args->corner] * side);
    return square;
}

// the square of the turn out at the notch's `corner`, on one of the work's edges: reaching from the corner into the
// work along the edge away from the hole and in from the edge, and the arc's center, the square's far corner
EMBED_INLINE GuiRect raster_turn_square(const GuiRasterCtx *args)
{
    int *centre_x = args->centre_x;
    int *centre_y = args->centre_y;
    const int toward_x =
        args->notch->on_y[args->corner] ? -args->notch->into_x[args->corner] : args->notch->into_x[args->corner];
    const int toward_y =
        args->notch->on_x[args->corner] ? -args->notch->into_y[args->corner] : args->notch->into_y[args->corner];
    // the square's side is the radius, which an int holds
    const int side = (int)args->notch->reach;
    const int x = args->notch->corner_x[args->corner];
    const int y = args->notch->corner_y[args->corner];
    const GuiRect square = {(toward_x < 0) ? x - side : x, (toward_y < 0) ? y - side : y, side, side};
    *centre_x = x + (toward_x * side);
    *centre_y = y + (toward_y * side);
    return square;
}

unsigned int gui_raster_notch_fillets(const GuiRasterRadiusCfg *args)
{
    RasterNotch read;
    if (!EMBED_CALL(raster_notch_read, GuiRasterCtx, .work = args->work, .notch_rect = args->notch,
                    .radius = args->radius, .read = &read) ||
        (read.reach == 0u))
    {
        return 0u;
    }
    unsigned int count = 0u;
    for (unsigned int corner = 0u; corner < 4u; corner += 1u)
    {
        if (!read.on_x[corner] && !read.on_y[corner])
        {
            int centre_x = 0;
            int centre_y = 0;
            args->squares[count] = EMBED_CALL(raster_fillet_square, GuiRasterCtx, .notch = &read, .corner = corner,
                                              .centre_x = &centre_x, .centre_y = &centre_y);
            count += 1u;
        }
    }
    return count;
}

void gui_raster_notch(const GuiRasterRadiusBackgroundCfg *args)
{
    RasterNotch read;
    if (!EMBED_CALL(raster_notch_read, GuiRasterCtx, .work = args->work, .notch_rect = args->notch,
                    .radius = args->radius, .read = &read))
    {
        return;
    }
    // the fillets: at a corner inside the work the outline turns in round the hole, and the square of the radius
    // inside the hole at the corner takes the ground inside the arc alone, what was drawn standing outside it
    GuiRect fillet[4];
    int centre_x[4];
    int centre_y[4];
    unsigned int fillets = 0u;
    for (unsigned int corner = 0u; (read.reach > 0u) && (corner < 4u); corner += 1u)
    {
        if (!read.on_x[corner] && !read.on_y[corner])
        {
            fillet[fillets] = EMBED_CALL(raster_fillet_square, GuiRasterCtx, .notch = &read, .corner = corner,
                                         .centre_x = &centre_x[fillets], .centre_y = &centre_y[fillets]);
            fillets += 1u;
        }
    }
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = read.hole,
                                  .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            unsigned int share = 255u;
            for (unsigned int at = 0u; at < fillets; at += 1u)
            {
                if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = fillet[at], .x = column, .y = row))
                {
                    share =
                        EMBED_CALL(raster_arc_at, GuiRasterCtx, .centre_x_value = centre_x[at],
                                   .centre_y_value = centre_y[at], .reach = read.reach, .column = column, .row = row);
                }
            }
            if (share != 0u)
            {
                EMBED_CALL(raster_append, GuiRasterCtx, .raster = args->raster, .x = column, .y = row,
                           .color = EMBED_CALL(raster_background_at, GuiRasterCtx,
                                               .background_fill = args->background_fill, .x = column, .y = row),
                           .share = share);
            }
        }
    }
    // at a corner on one of the work's edges the outline turns out, the square reaching into the work along the edge
    // away from the hole and in from the edge; a corner on two edges is the work's own, which the hole takes whole
    for (unsigned int corner = 0u; (read.reach > 0u) && (corner < 4u); corner += 1u)
    {
        if (read.on_x[corner] == read.on_y[corner])
        {
            continue;
        }
        int turn_x = 0;
        int turn_y = 0;
        const GuiRect square = EMBED_CALL(raster_turn_square, GuiRasterCtx, .notch = &read, .corner = corner,
                                          .centre_x = &turn_x, .centre_y = &turn_y);
        const GuiRect turn_on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = square,
                                           .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
        for (int row = turn_on.y; row < turn_on.y + turn_on.height; row += 1)
        {
            for (int column = turn_on.x; column < turn_on.x + turn_on.width; column += 1)
            {
                const unsigned int outside =
                    255u - EMBED_CALL(raster_arc_at, GuiRasterCtx, .centre_x_value = turn_x, .centre_y_value = turn_y,
                                      .reach = read.reach, .column = column, .row = row);
                if (outside != 0u)
                {
                    EMBED_CALL(raster_append, GuiRasterCtx, .raster = args->raster, .x = column, .y = row,
                               .color = EMBED_CALL(raster_background_at, GuiRasterCtx,
                                                   .background_fill = args->background_fill, .x = column, .y = row),
                               .share = outside);
                }
            }
        }
    }
}

// a line's `color` put at x, y by `cover`, a share of 255, of the color's alpha; a cover below none puts nothing
EMBED_INLINE void raster_line_append(const GuiRasterCtx *args)
{
    // the cover is not negative past the test, and two shares of 255 multiplied are a share of 255 once divided by 255
    const unsigned int share =
        (args->cover <= 0) ? 0u : ((GUI_ALPHA_OF(args->color) * (unsigned int)args->cover) + 127u) / 255u;
    if (share != 0u)
    {
        EMBED_CALL(raster_append, GuiRasterCtx, .raster = args->raster, .x = args->x, .y = args->y,
                   .color = args->color, .share = share);
    }
}

// the color and the width of the line of the edge `notch` reaches, the head's before the foot's before the left's
// before the right's; the right's where it reaches none
EMBED_INLINE GuiColor raster_notch_line(const GuiRasterCtx *args)
{
    int *pixel_width = args->pixel_width;
    if (args->notch->on_y[0])
    {
        *pixel_width = args->lines->top_width;
        return args->lines->top;
    }
    if (args->notch->on_y[2])
    {
        *pixel_width = args->lines->bottom_width;
        return args->lines->bottom;
    }
    if (args->notch->on_x[0])
    {
        *pixel_width = args->lines->left_width;
        return args->lines->left;
    }
    *pixel_width = args->lines->right_width;
    return args->lines->right;
}

// the work's own corner `corner`, top left, top right, bottom left or bottom right, laid: the square of the radius
// at it, each pixel cut to the `clear` ground outside the arc of `inside` rounded by a pixel less than the radius,
// the line of the nearer edge laid between that arc and the work's, and cut to the `table` outside the work's arc
EMBED_INLINE void raster_corner_outline(const GuiRasterCtx *args)
{
    // the square's side is the radius, which an int holds
    const int side = (int)args->radius;
    const int at_right = (args->corner & 1u) != 0u;
    const int at_bottom = args->corner >= 2u;
    const GuiRect square = {at_right ? args->work.x + args->work.width - side : args->work.x,
                            at_bottom ? args->work.y + args->work.height - side : args->work.y, side, side};
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = square,
                                  .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    const unsigned int inner_radius = (args->radius > 0u) ? args->radius - 1u : 0u;
    const GuiColor across = at_bottom ? args->lines->bottom : args->lines->top;
    const GuiColor down = at_right ? args->lines->right : args->lines->left;
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            const unsigned int outer = EMBED_CALL(raster_corner_cover, GuiRasterCtx, .rect = args->work,
                                                  .radius = args->radius, .x = x, .y = y);
            const unsigned int inner =
                EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->inside, .x = x, .y = y)
                    ? EMBED_CALL(raster_corner_cover, GuiRasterCtx, .rect = args->inside, .radius = inner_radius,
                                 .x = x, .y = y)
                    : 0u;
            EMBED_CALL(
                raster_append, GuiRasterCtx, .raster = args->raster, .x = x, .y = y,
                .color = EMBED_CALL(raster_background_at, GuiRasterCtx, .background_fill = args->clear, .x = x, .y = y),
                .share = 255u - inner);
            // the nearer edge's line: how far the pixel's middle stands in from the edge across and the edge down, in
            // half pixels
            const int from_across =
                at_bottom ? (2 * (args->work.y + args->work.height - y)) - 1 : (2 * (y - args->work.y)) + 1;
            const int from_down =
                at_right ? (2 * (args->work.x + args->work.width - x)) - 1 : (2 * (x - args->work.x)) + 1;
            // the shares are bytes
            EMBED_CALL(raster_line_append, GuiRasterCtx, .raster = args->raster, .x = x, .y = y,
                       .color = (from_across <= from_down) ? across : down, .cover = (int)outer - (int)inner);
            EMBED_CALL(
                raster_append, GuiRasterCtx, .raster = args->raster, .x = x, .y = y,
                .color = EMBED_CALL(raster_background_at, GuiRasterCtx, .background_fill = args->table, .x = x, .y = y),
                .share = 255u - outer);
        }
    }
}

// the line of an edge laid along it between the work's corners, `from` to `to` along the edge, less the span of each
// of the `count` holes reaching the edge grown by the radius on either side, where the line turns out round them
static void raster_edge_line(const GuiRasterCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiColor color = args->color;
    // the holes' spans along the edge, each grown by the radius, which an int holds
    const int side = (int)args->radius;
    int low[4];
    int high[4];
    unsigned int spans = 0u;
    for (unsigned int hole = 0u; (hole < args->count) && (spans < 4u); hole += 1u)
    {
        const int reaches =
            args->across_i32 ? ((args->at == args->work.y) ? args->holes[hole].y <= args->work.y
                                                           : args->holes[hole].y + args->holes[hole].height >=
                                                                 args->work.y + args->work.height)
                             : ((args->at == args->work.x)
                                    ? args->holes[hole].x <= args->work.x
                                    : args->holes[hole].x + args->holes[hole].width >= args->work.x + args->work.width);
        if (!reaches)
        {
            continue;
        }
        low[spans] = (args->across_i32 ? args->holes[hole].x : args->holes[hole].y) - side;
        high[spans] = (args->across_i32 ? args->holes[hole].x + args->holes[hole].width
                                        : args->holes[hole].y + args->holes[hole].height) +
                      side;
        spans += 1u;
    }
    // the line laid piece by piece: from each point along the edge to the next hole's span, then from that span's end
    int begin = args->from;
    while (begin < args->to)
    {
        int end = args->to;
        int next = args->to;
        for (unsigned int span = 0u; span < spans; span += 1u)
        {
            if ((high[span] > begin) && (low[span] < end))
            {
                end = (low[span] > begin) ? low[span] : begin;
                next = (high[span] < next) ? high[span] : next;
            }
        }
        if (end > begin)
        {
            const GuiRect piece = args->across_i32 ? (GuiRect){begin, args->at, end - begin, args->width_value}
                                                   : (GuiRect){args->at, begin, args->width_value, end - begin};
            EMBED_CALL(
                gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = piece, .other = args->work),
                .color = color);
        }
        begin = (next > begin) ? next : begin + 1;
    }
}

// the lines the hole of `notch` bends round laid: the table inside each fillet's arc and outside each turn's, and the
// line of the edge the hole reaches along the hole's sides inside the work, round the fillets and the turns
EMBED_INLINE void raster_notch_outline(const GuiRasterCtx *args)
{
    GuiRaster *raster = args->raster;
    int pixel_width = 0;
    const GuiColor color = EMBED_CALL(raster_notch_line, GuiRasterCtx, .notch = args->notch, .lines = args->lines,
                                      .pixel_width = &pixel_width);
    // the square's side is the radius, which an int holds
    const int side = (int)args->notch->reach;
    const GuiRect hole = args->notch->hole;
    for (unsigned int corner = 0u; (args->notch->reach > 0u) && (corner < 4u); corner += 1u)
    {
        int centre_x = 0;
        int centre_y = 0;
        if (!args->notch->on_x[corner] && !args->notch->on_y[corner])
        {
            // the fillet: the line lies outside the arc, the square grown by the line's width on its sides at the
            // corner
            const GuiRect square = EMBED_CALL(raster_fillet_square, GuiRasterCtx, .notch = args->notch,
                                              .corner = corner, .centre_x = &centre_x, .centre_y = &centre_y);
            const GuiRect grown = {(args->notch->into_x[corner] > 0) ? square.x - pixel_width : square.x,
                                   (args->notch->into_y[corner] > 0) ? square.y - pixel_width : square.y,
                                   square.width + pixel_width, square.height + pixel_width};
            const GuiRect on = EMBED_CALL(
                gui_raster_one.rect_intersect, GuiRasterOneCfg,
                .one = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = grown, .other = args->work),
                .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = raster));
            for (int y = on.y; y < on.y + on.height; y += 1)
            {
                for (int x = on.x; x < on.x + on.width; x += 1)
                {
                    const unsigned int inside =
                        EMBED_CALL(raster_arc_at, GuiRasterCtx, .centre_x_value = centre_x, .centre_y_value = centre_y,
                                   .reach = args->notch->reach, .column = x, .row = y);
                    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = square, .x = x, .y = y))
                    {
                        EMBED_CALL(raster_append, GuiRasterCtx, .raster = raster, .x = x, .y = y,
                                   .color = EMBED_CALL(raster_background_at, GuiRasterCtx,
                                                       .background_fill = args->table, .x = x, .y = y),
                                   .share = inside);
                    }
                    // the line's width is far inside what an unsigned int represents, and the shares are bytes
                    EMBED_CALL(raster_line_append, GuiRasterCtx, .raster = raster, .x = x, .y = y, .color = color,
                               .cover = (int)EMBED_CALL(raster_arc_at, GuiRasterCtx, .centre_x_value = centre_x,
                                                        .centre_y_value = centre_y,
                                                        .reach = args->notch->reach + (unsigned int)pixel_width,
                                                        .column = x, .row = y) -
                                        (int)inside);
                }
            }
        }
        else if (args->notch->on_x[corner] != args->notch->on_y[corner])
        {
            // the turn out: the line lies inside the arc, within its width of it
            const GuiRect square = EMBED_CALL(raster_turn_square, GuiRasterCtx, .notch = args->notch, .corner = corner,
                                              .centre_x = &centre_x, .centre_y = &centre_y);
            const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = square,
                                          .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = raster));
            const unsigned int inner =
                (args->notch->reach > (unsigned int)pixel_width) ? args->notch->reach - (unsigned int)pixel_width : 0u;
            for (int y = on.y; y < on.y + on.height; y += 1)
            {
                for (int x = on.x; x < on.x + on.width; x += 1)
                {
                    const unsigned int inside =
                        EMBED_CALL(raster_arc_at, GuiRasterCtx, .centre_x_value = centre_x, .centre_y_value = centre_y,
                                   .reach = args->notch->reach, .column = x, .row = y);
                    EMBED_CALL(raster_append, GuiRasterCtx, .raster = raster, .x = x, .y = y,
                               .color = EMBED_CALL(raster_background_at, GuiRasterCtx, .background_fill = args->table,
                                                   .x = x, .y = y),
                               .share = 255u - inside);
                    // the shares are bytes
                    EMBED_CALL(raster_line_append, GuiRasterCtx, .raster = raster, .x = x, .y = y, .color = color,
                               .cover =
                                   (int)inside - (int)EMBED_CALL(raster_arc_at, GuiRasterCtx,
                                                                 .centre_x_value = centre_x, .centre_y_value = centre_y,
                                                                 .reach = inner, .column = x, .row = y));
                }
            }
        }
    }
    // the hole's sides inside the work, each a stretch of the line just outside the hole between its corners' arcs
    if (!args->notch->on_x[0])
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(
                       gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){hole.x - pixel_width, hole.y + side, pixel_width, hole.height - (2 * side)},
                       .other = args->work),
                   .color = color);
    }
    if (!args->notch->on_x[1])
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(
                       gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){hole.x + hole.width, hole.y + side, pixel_width, hole.height - (2 * side)},
                       .other = args->work),
                   .color = color);
    }
    if (!args->notch->on_y[0])
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(
                       gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){hole.x + side, hole.y - pixel_width, hole.width - (2 * side), pixel_width},
                       .other = args->work),
                   .color = color);
    }
    if (!args->notch->on_y[2])
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(
                       gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){hole.x + side, hole.y + hole.height, hole.width - (2 * side), pixel_width},
                       .other = args->work),
                   .color = color);
    }
}

void gui_raster_outline(const GuiRasterRadiusWorkCfg *args)
{
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->work,
                                      .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster))))
    {
        return;
    }
    // the work's corners, each unless a hole takes it whole
    const int corner_x[4] = {args->work.x, args->work.x + args->work.width - 1, args->work.x,
                             args->work.x + args->work.width - 1};
    const int corner_y[4] = {args->work.y, args->work.y, args->work.y + args->work.height - 1,
                             args->work.y + args->work.height - 1};
    for (unsigned int corner = 0u; (args->radius > 0u) && (corner < 4u); corner += 1u)
    {
        int reserved = 0;
        for (unsigned int hole = 0u; hole < args->count; hole += 1u)
        {
            reserved = reserved || EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->holes[hole],
                                              .x = corner_x[corner], .y = corner_y[corner]);
        }
        if (!reserved)
        {
            EMBED_CALL(raster_corner_outline, GuiRasterCtx, .raster = args->raster, .work = args->work,
                       .inside = args->inside, .radius = args->radius, .lines = args->lines, .corner = corner,
                       .table = args->table, .clear = args->clear);
        }
    }
    // the lines along the edges between the corners
    // the radius is far inside what an int represents
    const int side = (int)args->radius;
    EMBED_CALL(raster_edge_line, GuiRasterCtx, .raster = args->raster, .work = args->work, .radius = args->radius,
               .across_i32 = 1, .at = args->work.y, .width_value = args->lines->top_width, .color = args->lines->top,
               .from = args->work.x + side, .to = args->work.x + args->work.width - side, .holes = args->holes,
               .count = args->count);
    EMBED_CALL(raster_edge_line, GuiRasterCtx, .raster = args->raster, .work = args->work, .radius = args->radius,
               .across_i32 = 1, .at = args->work.y + args->work.height - args->lines->bottom_width,
               .width_value = args->lines->bottom_width, .color = args->lines->bottom, .from = args->work.x + side,
               .to = args->work.x + args->work.width - side, .holes = args->holes, .count = args->count);
    EMBED_CALL(raster_edge_line, GuiRasterCtx, .raster = args->raster, .work = args->work, .radius = args->radius,
               .across_i32 = 0, .at = args->work.x, .width_value = args->lines->left_width, .color = args->lines->left,
               .from = args->work.y + side, .to = args->work.y + args->work.height - side, .holes = args->holes,
               .count = args->count);
    EMBED_CALL(raster_edge_line, GuiRasterCtx, .raster = args->raster, .work = args->work, .radius = args->radius,
               .across_i32 = 0, .at = args->work.x + args->work.width - args->lines->right_width,
               .width_value = args->lines->right_width, .color = args->lines->right, .from = args->work.y + side,
               .to = args->work.y + args->work.height - side, .holes = args->holes, .count = args->count);
    // each hole's turns and the line round it
    for (unsigned int hole = 0u; hole < args->count; hole += 1u)
    {
        RasterNotch read;
        if (EMBED_CALL(raster_notch_read, GuiRasterCtx, .work = args->work, .notch_rect = args->holes[hole],
                       .radius = args->radius, .read = &read))
        {
            EMBED_CALL(raster_notch_outline, GuiRasterCtx, .raster = args->raster, .work = args->work, .notch = &read,
                       .lines = args->lines, .table = args->table);
        }
    }
}

void gui_raster_line(const GuiRasterColorX0Cfg *args)
{
    const GuiRect on = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster);
    const unsigned int alpha = GUI_ALPHA_OF(args->color);
    const int step_x = (args->x1 > args->x0) ? 1 : -1;
    const int step_y = (args->y1 > args->y0) ? 1 : -1;
    const int span_x = (args->x1 > args->x0) ? (args->x1 - args->x0) : (args->x0 - args->x1);
    const int span_y = (args->y1 > args->y0) ? (args->y1 - args->y0) : (args->y0 - args->y1);
    int error = span_x - span_y;
    int x = args->x0;
    int y = args->y0;
    for (;;)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = on, .x = x, .y = y))
        {
            EMBED_CALL(raster_append, GuiRasterCtx, .raster = args->raster, .x = x, .y = y, .color = args->color,
                       .share = alpha);
        }
        if ((x == args->x1) && (y == args->y1))
        {
            return;
        }
        const int twice = 2 * error;
        if (twice > -span_y)
        {
            error -= span_y;
            x += step_x;
        }
        if (twice < span_x)
        {
            error += span_x;
            y += step_y;
        }
    }
}

void gui_raster_mask(const GuiRasterXYColorCfg *args)
{
    // a mask's sides are far inside what an int represents
    const GuiRect laid = {args->x, args->y, (int)args->mask->width, (int)args->mask->height};
    const GuiRect on =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = laid, .other = args->clip),
                   .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    const unsigned int alpha = GUI_ALPHA_OF(args->color);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on) || (alpha == 0u))
    {
        return;
    }
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        // the row and column inside the mask are not negative, the rectangle drawn lying inside it
        const unsigned char *const cover = args->mask->cover + ((unsigned int)(row - args->y) * args->mask->width);
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            const unsigned int share = cover[(unsigned int)(column - args->x)];
            if (share != 0u)
            {
                // two shares of 255 multiplied are a share of 255 once divided by 255
                EMBED_CALL(raster_append, GuiRasterCtx, .raster = args->raster, .x = column, .y = row,
                           .color = args->color, .share = ((share * alpha) + 127u) / 255u);
            }
        }
    }
}

void gui_raster_lanes_layout(const GuiRasterWidthXCfg *args)
{
    // a picture's sides are far inside what an int represents
    const GuiRect laid = {args->x, args->y, (int)args->width, (int)args->height};
    const GuiRect on =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = laid, .other = args->clip),
                   .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
    {
        return;
    }
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        // the row and column inside the picture are not negative, the rectangle laid lying inside it; the pixel lies
        // on the raster: its coordinates are not negative
        const unsigned int from = ((unsigned int)(row - args->y) * args->width) + (unsigned int)(on.x - args->x);
        const unsigned int *const text_color_red_blue = args->red_blue + from;
        const unsigned int *const text_color_green = args->green + from;
        const unsigned int *const saved = args->rest + from;
        GuiColor *const out = &args->raster->pixels[((unsigned int)row * args->raster->width) + (unsigned int)on.x];
        // the row's width is not negative
        for (unsigned int px = 0u; px < (unsigned int)on.width; px += 1u)
        {
            const GuiColor background_fill = out[px];
            const unsigned int lanes = text_color_red_blue[px] + ((background_fill & 0x00ff00ffu) * saved[px]);
            const unsigned int lane = text_color_green[px] + (((background_fill >> 8u) & 0xffu) * saved[px]);
            out[px] = 0xff000000u | (((lanes + ((lanes >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu) |
                      ((((lane + (lane >> 8u)) >> 8u) & 0xffu) << 8u);
        }
    }
}

void gui_raster_stencil(const GuiRasterXYColorCfg *args)
{
    const unsigned long long pixels = (unsigned long long)args->mask->width * args->mask->height;
    const unsigned int most = (args->raster->width * args->raster->height < pixels)
                                  ? args->raster->width * args->raster->height
                                  : (unsigned int)pixels;
    for (unsigned int at = 0u; at < most; at += 1u)
    {
        args->raster->pixels[at] = ((GuiColor)args->mask->cover[at] << 24u) | (args->color & 0x00ffffffu);
    }
}

unsigned int gui_raster_row_cover(const GuiRasterWidthCfg *args)
{
    int *x = args->x_pointer;
    *x = 0;
    if ((args->right <= args->left) || (args->width == 0u))
    {
        return 0u;
    }
    const long long span = (long long)args->right - (long long)args->left;
    const int first = (args->left < 0) ? 0 : (args->left >> 16);
    const int ends = (int)((((long long)args->right + 65535ll) >> 16));
    // the raster's width is far inside what an int represents
    const int end = (ends > (int)args->raster_width) ? (int)args->raster_width : ends;
    if (end <= first)
    {
        return 0u;
    }
    *x = first;
    // the pixels are not negative in count, past being past first
    const unsigned int count = (unsigned int)(end - first);
    if (args->out == NULL)
    {
        return count;
    }
    // how much of the row a whole pixel takes, in the row's bytes with sixteen bits of fraction, and one over it with
    // forty, each worked once for the row: a pixel's share is the row's cover summed along what of it lands on the
    // pixel, by that one over: no division a pixel
    const long long whole = (((long long)args->width << 16) << 16) / span;
    const long long over = (1ll << 40) / whole;
    const long long stop_position = (long long)args->width << 16;
    long long place =
        (first == (args->left >> 16)) ? 0ll : ((((long long)first << 16) - (long long)args->left) * whole) >> 16;
    unsigned int at = (unsigned int)(place >> 16);
    for (unsigned int pixel = 0u; pixel < count; pixel += 1u)
    {
        const long long end_word = (((long long)first + (long long)pixel + 1ll) << 16);
        const long long worked =
            (((end_word > (long long)args->right) ? (long long)args->right : end_word) - (long long)args->left) *
                whole >>
            16;
        const long long stop = (worked > stop_position) ? stop_position : worked;
        long long area = 0ll;
        while (place < stop)
        {
            const long long next = ((long long)at + 1ll) << 16;
            const long long upto = (next < stop) ? next : stop;
            area += (long long)args->cover[at] * (upto - place);
            place = upto;
            at += (upto == next) ? 1u : 0u;
        }
        // a mean of bytes, rounded, is a share of 255
        args->out[pixel] = (unsigned char)(((area * over) + (1ll << 39)) >> 40);
    }
    return count;
}

void gui_raster_span_layout(const GuiRasterXYColorCountCfg *args)
{
    const unsigned int alpha = GUI_ALPHA_OF(args->color);
    // the raster's sides are far inside what an int represents, and a span made for the raster lies inside its width
    if ((args->y < 0) || (args->y >= (int)args->raster->height) || (alpha == 0u))
    {
        return;
    }
    GuiColor *const out = &args->raster->pixels[((unsigned int)args->y * args->raster->width) + (unsigned int)args->x];
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (args->cover[at] != 0u)
        {
            out[at] = EMBED_CALL(raster_blend, GuiRasterCtx, .background_color = out[at], .color = args->color,
                                 .share = ((args->cover[at] * alpha) + 127u) / 255u);
        }
    }
}

void gui_raster_stenciled_over(const GuiRasterXYCfg *args)
{
    // the mask's sides are far inside what an int represents
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){args->x, args->y, (int)args->mask->width, (int)args->mask->height},
                                  .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->picture));
    const unsigned int alpha = GUI_ALPHA_OF(args->color);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on) || (alpha == 0u))
    {
        return;
    }
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        // the row and column inside the mask are not negative, the rectangle drawn lying inside it
        const unsigned char *const cover = args->mask->cover + ((unsigned int)(row - args->y) * args->mask->width);
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            const unsigned int laid = ((cover[(unsigned int)(column - args->x)] * alpha) + 127u) / 255u;
            if (laid != 0u)
            {
                GuiColor *const at =
                    &args->picture->pixels[((unsigned int)row * args->picture->width) + (unsigned int)column];
                const GuiColor had = *at;
                const unsigned int saved = ((GUI_ALPHA_OF(had) * (255u - laid)) + 127u) / 255u;
                const unsigned int total = laid + saved;
                // the two shares of a channel over their sum, rounded, are a byte
                *at = GUI_RGBA(
                    ((GUI_RED_OF(args->color) * laid) + (GUI_RED_OF(had) * saved) + (total / 2u)) / total,
                    ((GUI_GREEN_OF(args->color) * laid) + (GUI_GREEN_OF(had) * saved) + (total / 2u)) / total,
                    ((GUI_BLUE_OF(args->color) * laid) + (GUI_BLUE_OF(had) * saved) + (total / 2u)) / total, total);
            }
        }
    }
}

void gui_raster_picture_layout(const GuiRasterXCfg *args)
{
    // the picture's sides are far inside what an int represents
    const GuiRect on =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){args->x, args->y, (int)args->picture_read->width, (int)args->picture_read->height},
                   .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on) || (args->opacity == 0u))
    {
        return;
    }
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        // the row and column inside the picture are not negative, the rectangle laid lying inside it
        const GuiColor *const from =
            args->picture_read->pixels + ((unsigned int)(row - args->y) * args->picture_read->width);
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            const GuiColor pixel = from[(unsigned int)(column - args->x)];
            const unsigned int share = ((GUI_ALPHA_OF(pixel) * args->opacity) + 127u) / 255u;
            if (share != 0u)
            {
                EMBED_CALL(raster_append, GuiRasterCtx, .raster = args->raster, .x = column, .y = row, .color = pixel,
                           .share = share);
            }
        }
    }
}

unsigned int gui_raster_order(const GuiRasterRectCfg *args)
{
    // a coordinate's low bits read the same whatever its sign
    const unsigned int across = (unsigned int)args->x;
    const unsigned int down = (unsigned int)args->y;
    return ((across & 1u) << 3u) | ((down & 1u) << 2u) | (((across >> 1u) & 1u) << 1u) | ((down >> 1u) & 1u);
}

// a channel of `color` put down over `background_fill` with the dot's `share` of it from 0 to 1 and the dither's
// `threshold` in levels of 255: the color's share and the threshold laid over the ground's rest, as the page's shader
// blends a premultiplied dot over what stands
EMBED_INLINE unsigned int raster_dot_channel(const GuiRasterCtx *args)
{
    const float laid = ((float)args->color_u32 * args->share_f32) + args->threshold;
    const float within = (laid < 0.0f) ? 0.0f : ((laid > 255.0f) ? 255.0f : laid);
    const float alpha = (args->share_f32 * 255.0f) + args->threshold;
    const float rest = (alpha < 0.0f) ? 1.0f : ((alpha > 255.0f) ? 0.0f : 1.0f - (alpha / 255.0f));
    const float out = within + ((float)args->background_u32 * rest);
    // a channel from 0 to 255, rounded, is a byte
    return (out >= 255.0f) ? 255u : (unsigned int)(out + 0.5f);
}

void gui_raster_grain(const GuiRasterRectRadiusCfg *args)
{
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->rect,
                                  .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on) || (args->tile == 0u))
    {
        return;
    }
    const GuiColor white = GUI_RGB(0xffu, 0xffu, 0xffu);
    // the corners' squares are radius by radius, which an int holds; a row through them reads each pixel's share
    // inside the arcs, every other row lays the grain whole
    const int side = (int)args->radius;
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        // the pixel lies on the raster: its coordinates are not negative; the tile's row and the column it starts
        // at are read once a row and the column counted on, wrapping at the tile's side
        const unsigned char *const tile_row = args->alpha + (((unsigned int)y % args->tile) * args->tile);
        unsigned int column = (unsigned int)on.x % args->tile;
        const int through_corners =
            (side > 0) && ((y < args->rect.y + side) || (y >= args->rect.y + args->rect.height - side));
        GuiColor *pixel = &args->raster->pixels[((unsigned int)y * args->raster->width) + (unsigned int)on.x];
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            unsigned int grain = tile_row[column];
            column = (column + 1u == args->tile) ? 0u : column + 1u;
            if ((grain != 0u) && through_corners)
            {
                // two shares of 255 multiplied are a share of 255 once divided by 255
                grain = ((grain * EMBED_CALL(raster_corner_cover, GuiRasterCtx, .rect = args->rect,
                                             .radius = args->radius, .x = x, .y = y)) +
                         127u) /
                        255u;
            }
            if (grain != 0u)
            {
                *pixel =
                    EMBED_CALL(raster_blend, GuiRasterCtx, .background_color = *pixel, .color = white, .share = grain);
            }
            pixel += 1;
        }
    }
}

void gui_raster_dot(const GuiRasterColorCfg *args)
{
    const float text_color = (float)GUI_ALPHA_OF(args->color) / 255.0f;
    const float reach = args->radius_f32 + 1.0f;
    // the square about the dot, its sides whole pixels, is far inside what an int represents
    const int left = (int)floorf(args->x_f32 - reach);
    const int top = (int)floorf(args->y_f32 - reach);
    const GuiRect square = {left, top, (int)ceilf(args->x_f32 + reach) - left + 1,
                            (int)ceilf(args->y_f32 + reach) - top + 1};
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = square,
                                  .other = EMBED_CALL(raster_rect, GuiRasterCtx, .raster_read = args->raster));
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            const float inside = args->radius_f32 -
                                 hypotf(((float)column + 0.5f) - args->x_f32, ((float)row + 0.5f) - args->y_f32) + 0.5f;
            if (inside <= 0.0f)
            {
                continue;
            }
            const float share = text_color * ((inside < 1.0f) ? inside : 1.0f);
            const float threshold =
                ((float)EMBED_CALL(gui_raster_rect.order, GuiRasterRectCfg, .x = column, .y = row) - 7.5f) / 16.0f;
            // the pixel lies on the raster: its coordinates are not negative
            GuiColor *const at =
                &args->raster->pixels[((unsigned int)row * args->raster->width) + (unsigned int)column];
            *at =
                GUI_RGB(EMBED_CALL(raster_dot_channel, GuiRasterCtx, .background_u32 = GUI_RED_OF(*at),
                                   .color_u32 = GUI_RED_OF(args->color), .share_f32 = share, .threshold = threshold),
                        EMBED_CALL(raster_dot_channel, GuiRasterCtx, .background_u32 = GUI_GREEN_OF(*at),
                                   .color_u32 = GUI_GREEN_OF(args->color), .share_f32 = share, .threshold = threshold),
                        EMBED_CALL(raster_dot_channel, GuiRasterCtx, .background_u32 = GUI_BLUE_OF(*at),
                                   .color_u32 = GUI_BLUE_OF(args->color), .share_f32 = share, .threshold = threshold));
        }
    }
}
