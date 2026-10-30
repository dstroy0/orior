// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.c: the pixels the window draws and all that is drawn into them. Every blend is of bytes in the colors' own
// space, a share from 0 to 255 of the color over what is there, as the page blends its colors, two channels at a
// time in one word; a pixel drawn is left opaque. A dot is put down as the page's shader puts the lattice's down: each pixel's share of it read in one step
// from how far the pixel's middle stands from the dot's edge, and the result dithered by a 4 by 4 ordered threshold
// below one level of 255, so a share between two levels is put down as both
#include "raster.h"

#include "../../cli/src/command_line/cli_internal.h"

#include <math.h>

static CellOwner s_owner = MMGR_CELL_OWNER("gui_raster");

int gui_rect_empty(GuiRect rect)
{
    return (rect.width <= 0) || (rect.height <= 0);
}

int gui_rect_holds(GuiRect rect, int x, int y)
{
    return (x >= rect.x) && (y >= rect.y) && (x < rect.x + rect.width) && (y < rect.y + rect.height);
}

GuiRect gui_rect_met(GuiRect one, GuiRect other)
{
    const int left = (one.x > other.x) ? one.x : other.x;
    const int top = (one.y > other.y) ? one.y : other.y;
    const int one_right = one.x + one.width;
    const int other_right = other.x + other.width;
    const int one_bottom = one.y + one.height;
    const int other_bottom = other.y + other.height;
    const int right = (one_right < other_right) ? one_right : other_right;
    const int bottom = (one_bottom < other_bottom) ? one_bottom : other_bottom;
    const GuiRect met = {left, top, right - left, bottom - top};
    return met;
}

// the raster's own rectangle
static GuiRect raster_rect(const GuiRaster *raster)
{
    // a raster's sides are far inside what an int represents
    const GuiRect rect = {0, 0, (int)raster->width, (int)raster->height};
    return rect;
}

unsigned int gui_raster_pitch(unsigned int width)
{
    return (width + (GUI_RASTER_PITCH - 1u)) & ~(GUI_RASTER_PITCH - 1u);
}

int gui_raster_made(GuiRaster *raster, unsigned int width, unsigned int height)
{
    const unsigned long long bytes = (unsigned long long)width * height * sizeof(GuiColor);
    raster->width = width;
    raster->height = height;
    raster->pages = NULL;
    raster->pages_bytes = 0ull;
    raster->placed = 0ull;
    raster->pixels = g_cli_cells->persistent_buf_alloc(bytes, &s_owner);
    if (raster->pixels == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = raster->pixels, .val = 0, .bytes = bytes});
    return 1;
}

void gui_raster_given(GuiRaster *raster)
{
    g_cli_cells->persistent_buf_release(raster->pixels, &s_owner);
    raster->pixels = NULL;
    raster->width = 0u;
    raster->height = 0u;
}

// `color` over `ground` by `share` of 255, the pixel left opaque: red and blue blended in one word, each in its own
// sixteen bits, and green in another, two multiplies a pixel; each lane's sum is the color's share plus the
// ground's rest plus half of 255, below 65536, and the lane divided by 255 exactly by adding its own high byte
// before the shift, which the raster's check holds over every ground, color and share
static GuiColor raster_blended(GuiColor ground, GuiColor color, unsigned int share)
{
    if (share >= 255u)
    {
        return color | 0xff000000u;
    }
    const unsigned int rest = 255u - share;
    const unsigned int red_blue = ((color & 0x00ff00ffu) * share) + ((ground & 0x00ff00ffu) * rest) + 0x00800080u;
    const unsigned int green = (((color >> 8u) & 0xffu) * share) + (((ground >> 8u) & 0xffu) * rest) + 0x80u;
    const unsigned int red_blue_out = ((red_blue + ((red_blue >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu;
    const unsigned int green_out = ((green + (green >> 8u)) >> 8u) & 0xffu;
    return 0xff000000u | red_blue_out | (green_out << 8u);
}

// the pixel at x, y of a rectangle known to lie on the raster blended with `color` by `share` of 255
static void raster_put(GuiRaster *raster, int x, int y, GuiColor color, unsigned int share)
{
    // the pixel lies on the raster: its coordinates are not negative
    GuiColor *const at = &raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
    *at = raster_blended(*at, color, share);
}

void gui_raster_filled(GuiRaster *raster, GuiRect rect, GuiColor color)
{
    const GuiRect on = gui_rect_met(rect, raster_rect(raster));
    const unsigned int alpha = GUI_ALPHA_OF(color);
    if (gui_rect_empty(on) || (alpha == 0u))
    {
        return;
    }
    // the rectangle lies on the raster: its coordinates are not negative
    GuiColor *row = &raster->pixels[((unsigned int)on.y * raster->width) + (unsigned int)on.x];
    if (alpha >= 255u)
    {
        const GuiColor whole = color | 0xff000000u;
        for (int y = 0; y < on.height; y += 1)
        {
            for (int x = 0; x < on.width; x += 1)
            {
                row[x] = whole;
            }
            row += raster->width;
        }
        return;
    }
    // the color's lanes by its share, with half of 255 in each, read once for the whole fill, as raster_blended lays
    // them; each pixel adds the ground's lanes by the rest
    const unsigned int rest = 255u - alpha;
    const unsigned int color_red_blue = ((color & 0x00ff00ffu) * alpha) + 0x00800080u;
    const unsigned int color_green = (((color >> 8u) & 0xffu) * alpha) + 0x80u;
    for (int y = 0; y < on.height; y += 1)
    {
        for (int x = 0; x < on.width; x += 1)
        {
            const GuiColor ground = row[x];
            const unsigned int red_blue = color_red_blue + ((ground & 0x00ff00ffu) * rest);
            const unsigned int green = color_green + (((ground >> 8u) & 0xffu) * rest);
            const unsigned int red_blue_out = ((red_blue + ((red_blue >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu;
            const unsigned int green_out = ((green + (green >> 8u)) >> 8u) & 0xffu;
            row[x] = 0xff000000u | red_blue_out | (green_out << 8u);
        }
        row += raster->width;
    }
}

// how much of the pixel at x, y of `rect` lies inside its corners' arcs of `radius`, 0 to 1: 1 away from the corners,
// read in whole pixels with no square root, and in a corner's square the distance left from the pixel's middle to
// the arc, clamped
static float raster_corner_share(GuiRect rect, unsigned int radius, int x, int y)
{
    // the corners' squares are radius by radius, which an int holds
    const int side = (int)radius;
    const int in_corner_column = (x < rect.x + side) || (x >= rect.x + rect.width - side);
    const int in_corner_row = (y < rect.y + side) || (y >= rect.y + rect.height - side);
    if (!in_corner_column || !in_corner_row)
    {
        return 1.0f;
    }
    const float reach = (float)radius;
    const float left = (float)rect.x + reach;
    const float right = (float)(rect.x + rect.width) - reach;
    const float top = (float)rect.y + reach;
    const float bottom = (float)(rect.y + rect.height) - reach;
    const float middle_x = (float)x + 0.5f;
    const float middle_y = (float)y + 0.5f;
    const float centre_x = (middle_x < left) ? left : ((middle_x > right) ? right : middle_x);
    const float centre_y = (middle_y < top) ? top : ((middle_y > bottom) ? bottom : middle_y);
    const float away = hypotf(middle_x - centre_x, middle_y - centre_y);
    const float share = reach + 0.5f - away;
    return (share < 0.0f) ? 0.0f : ((share > 1.0f) ? 1.0f : share);
}

void gui_raster_rounded(GuiRaster *raster, GuiRect rect, unsigned int radius, GuiColor color)
{
    const GuiRect on = gui_rect_met(rect, raster_rect(raster));
    const unsigned int alpha = GUI_ALPHA_OF(color);
    if (gui_rect_empty(on) || (alpha == 0u))
    {
        return;
    }
    if (radius == 0u)
    {
        gui_raster_filled(raster, rect, color);
        return;
    }
    // the rows between the corners' squares are filled whole through the fill's own loop; the rows through the
    // squares pixel by pixel, each by its share inside the arcs
    const int side = (int)radius;
    gui_raster_filled(raster, (GuiRect){rect.x, rect.y + side, rect.width, rect.height - (2 * side)}, color);
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        if ((y >= rect.y + side) && (y < rect.y + rect.height - side))
        {
            continue;
        }
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            const float share = raster_corner_share(rect, radius, x, y);
            if (share > 0.0f)
            {
                // a share of 255 by a factor from 0 to 1, rounded, is a byte
                raster_put(raster, x, y, color, (unsigned int)((float)alpha * share + 0.5f));
            }
        }
    }
}

GuiColor gui_color_over(GuiColor ground, GuiColor color)
{
    return raster_blended(ground, color, GUI_ALPHA_OF(color));
}

// the ground's own pixel at x, y: the ground raster's where one is given and holds the pixel, else the ground's color
static GuiColor raster_under_at(GuiUnder ground, int x, int y)
{
    if ((ground.raster == NULL) || (ground.raster->pixels == NULL) || !gui_rect_holds(raster_rect(ground.raster), x, y))
    {
        return ground.color;
    }
    // the pixel lies on the ground raster: its coordinates are not negative
    return ground.raster->pixels[((unsigned int)y * ground.raster->width) + (unsigned int)x];
}

// the alpha the ground's grain gives the pixel at x, y, 0 where it has none; the coordinates are not negative
static unsigned int raster_grain_at(GuiUnder ground, int x, int y)
{
    if ((ground.grain == NULL) || (ground.tile == 0u))
    {
        return 0u;
    }
    return ground.grain[(((unsigned int)y % ground.tile) * ground.tile) + ((unsigned int)x % ground.tile)];
}

// the ground's color at x, y, a pixel on the raster: the ground's own pixel with the color over it, and the grain's
// white over that where the ground has a grain
static GuiColor raster_ground_at(GuiUnder ground, int x, int y)
{
    const GuiColor tinted = raster_blended(raster_under_at(ground, x, y), ground.over, GUI_ALPHA_OF(ground.over));
    const unsigned int grain = raster_grain_at(ground, x, y);
    return (grain == 0u) ? tinted : raster_blended(tinted, GUI_RGB(0xffu, 0xffu, 0xffu), grain);
}

// a pixel of a pane on the table `ground` at x, y: the ground's own pixel with the ink at `alpha`, and the grain's
// white over that
static GuiColor raster_pane_at(GuiUnder ground, int x, int y, GuiColor ink, unsigned int alpha)
{
    const GuiColor tinted = raster_blended(raster_under_at(ground, x, y), ink, alpha);
    const unsigned int grain = raster_grain_at(ground, x, y);
    return (grain == 0u) ? tinted : raster_blended(tinted, GUI_RGB(0xffu, 0xffu, 0xffu), grain);
}

void gui_raster_paned(GuiRaster *raster, GuiUnder ground, GuiRect rect, unsigned int radius, GuiColor inner,
                      int corners_only)
{
    const GuiRect on = gui_rect_met(rect, raster_rect(raster));
    if (gui_rect_empty(on))
    {
        return;
    }
    const float outer_alpha = (float)GUI_ALPHA_OF(ground.over);
    const float inner_alpha = (float)GUI_ALPHA_OF(inner);
    // the corners' squares are radius by radius, which an int holds
    const int side = (int)radius;
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        const int through_corners = (y < rect.y + side) || (y >= rect.y + rect.height - side);
        if (corners_only && !through_corners)
        {
            continue;
        }
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            const int in_corner = through_corners && ((x < rect.x + side) || (x >= rect.x + rect.width - side));
            if (corners_only && !in_corner)
            {
                continue;
            }
            const float share = in_corner ? raster_corner_share(rect, radius, x, y) : 1.0f;
            // an alpha between two bytes by a factor from 0 to 1, rounded, is a byte
            const unsigned int alpha = (unsigned int)(outer_alpha + ((inner_alpha - outer_alpha) * share) + 0.5f);
            // the pixel lies on the raster: its coordinates are not negative
            raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] = raster_pane_at(ground, x, y, inner, alpha);
        }
    }
}

void gui_raster_corners_cut(GuiRaster *raster, GuiRect rect, unsigned int radius, GuiUnder ground)
{
    const GuiRect on = gui_rect_met(rect, raster_rect(raster));
    if (gui_rect_empty(on) || (radius == 0u))
    {
        return;
    }
    // the corners' squares, each radius by radius, are where the arcs run
    const int reach = (int)radius;
    const GuiRect squares[4] = {{rect.x, rect.y, reach, reach},
                                {rect.x + rect.width - reach, rect.y, reach, reach},
                                {rect.x, rect.y + rect.height - reach, reach, reach},
                                {rect.x + rect.width - reach, rect.y + rect.height - reach, reach, reach}};
    for (unsigned int corner = 0u; corner < 4u; corner += 1u)
    {
        const GuiRect square = gui_rect_met(squares[corner], on);
        for (int y = square.y; y < square.y + square.height; y += 1)
        {
            for (int x = square.x; x < square.x + square.width; x += 1)
            {
                const float outside = 1.0f - raster_corner_share(rect, radius, x, y);
                if (outside > 0.0f)
                {
                    // a share of 255 by a factor from 0 to 1, rounded, is a byte
                    raster_put(raster, x, y, raster_ground_at(ground, x, y), (unsigned int)((255.0f * outside) + 0.5f));
                }
            }
        }
    }
}

// the share of the pixel at `column`, `row` inside the arc of `reach` about `centre_x`, `centre_y`, 0 to 1 by the
// distance left from the pixel's middle to the arc
static float raster_arc_share(float centre_x, float centre_y, unsigned int reach, int column, int row)
{
    const float away = hypotf(((float)column + 0.5f) - centre_x, ((float)row + 0.5f) - centre_y);
    const float inside = (float)reach + 0.5f - away;
    return (inside < 0.0f) ? 0.0f : ((inside > 1.0f) ? 1.0f : inside);
}

// the turn of the work's outline out at x, y on the work's edge, rounded by `reach`: the square of that side from
// the point, reaching `toward_x` and `toward_y` (each 1 or -1) into the work, holds the arc about its far corner, and
// the pixels outside the arc take `ground` by their share outside it
static void raster_turned_out(GuiRaster *raster, int x, int y, unsigned int reach, int toward_x, int toward_y, GuiUnder ground)
{
    // the square's side is the radius, which an int holds
    const int side = (int)reach;
    const GuiRect square = {(toward_x < 0) ? x - side : x, (toward_y < 0) ? y - side : y, side, side};
    const GuiRect on = gui_rect_met(square, raster_rect(raster));
    const float centre_x = (float)(x + (toward_x * side));
    const float centre_y = (float)(y + (toward_y * side));
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            const float outside = 1.0f - raster_arc_share(centre_x, centre_y, reach, column, row);
            if (outside > 0.0f)
            {
                // a share of 255 by a factor from 0 to 1, rounded, is a byte
                raster_put(raster, column, row, raster_ground_at(ground, column, row), (unsigned int)((255.0f * outside) + 0.5f));
            }
        }
    }
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

// `notch` read as a hole in `work`, its turns rounded by `radius`: 1, or 0 where nothing of it lies on the work
static int raster_notch_read(GuiRect work, GuiRect notch, unsigned int radius, RasterNotch *read)
{
    const GuiRect hole = gui_rect_met(notch, work);
    if (gui_rect_empty(hole))
    {
        return 0;
    }
    // the radius is no more than half the hole's sides, where the turns would meet
    const unsigned int most = (unsigned int)(((hole.width < hole.height) ? hole.width : hole.height) / 2);
    read->hole = hole;
    read->reach = (radius > most) ? most : radius;
    const int left_edge = hole.x <= work.x;
    const int right_edge = hole.x + hole.width >= work.x + work.width;
    const int top_edge = hole.y <= work.y;
    const int bottom_edge = hole.y + hole.height >= work.y + work.height;
    const int corner_x[4] = {hole.x, hole.x + hole.width, hole.x, hole.x + hole.width};
    const int corner_y[4] = {hole.y, hole.y, hole.y + hole.height, hole.y + hole.height};
    const int on_x[4] = {left_edge, right_edge, left_edge, right_edge};
    const int on_y[4] = {top_edge, top_edge, bottom_edge, bottom_edge};
    const int into_x[4] = {1, -1, 1, -1};
    const int into_y[4] = {1, 1, -1, -1};
    for (unsigned int corner = 0u; corner < 4u; corner += 1u)
    {
        read->corner_x[corner] = corner_x[corner];
        read->corner_y[corner] = corner_y[corner];
        read->on_x[corner] = on_x[corner];
        read->on_y[corner] = on_y[corner];
        read->into_x[corner] = into_x[corner];
        read->into_y[corner] = into_y[corner];
    }
    return 1;
}

// the square of the fillet at the notch's `corner`, inside the hole at the corner, and the arc's center, the far
// corner of the square
static GuiRect raster_fillet_square(const RasterNotch *notch, unsigned int corner, float *centre_x, float *centre_y)
{
    // the square's side is the radius, which an int holds
    const int side = (int)notch->reach;
    const GuiRect square = {(notch->into_x[corner] < 0) ? notch->corner_x[corner] - side : notch->corner_x[corner],
                            (notch->into_y[corner] < 0) ? notch->corner_y[corner] - side : notch->corner_y[corner], side, side};
    *centre_x = (float)(notch->corner_x[corner] + (notch->into_x[corner] * side));
    *centre_y = (float)(notch->corner_y[corner] + (notch->into_y[corner] * side));
    return square;
}

// the square of the turn out at the notch's `corner`, on one of the work's edges: reaching from the corner into the
// work along the edge away from the hole and in from the edge, and the arc's center, the square's far corner
static GuiRect raster_turn_square(const RasterNotch *notch, unsigned int corner, float *centre_x, float *centre_y)
{
    const int toward_x = notch->on_y[corner] ? -notch->into_x[corner] : notch->into_x[corner];
    const int toward_y = notch->on_x[corner] ? -notch->into_y[corner] : notch->into_y[corner];
    // the square's side is the radius, which an int holds
    const int side = (int)notch->reach;
    const int x = notch->corner_x[corner];
    const int y = notch->corner_y[corner];
    const GuiRect square = {(toward_x < 0) ? x - side : x, (toward_y < 0) ? y - side : y, side, side};
    *centre_x = (float)(x + (toward_x * side));
    *centre_y = (float)(y + (toward_y * side));
    return square;
}

unsigned int gui_raster_notch_fillets(GuiRect work, GuiRect notch, unsigned int radius, GuiRect *squares)
{
    RasterNotch read;
    if (!raster_notch_read(work, notch, radius, &read) || (read.reach == 0u))
    {
        return 0u;
    }
    unsigned int count = 0u;
    for (unsigned int corner = 0u; corner < 4u; corner += 1u)
    {
        if (!read.on_x[corner] && !read.on_y[corner])
        {
            float centre_x = 0.0f;
            float centre_y = 0.0f;
            squares[count] = raster_fillet_square(&read, corner, &centre_x, &centre_y);
            count += 1u;
        }
    }
    return count;
}

void gui_raster_notched(GuiRaster *raster, GuiRect work, GuiRect notch, unsigned int radius, GuiUnder ground)
{
    RasterNotch read;
    if (!raster_notch_read(work, notch, radius, &read))
    {
        return;
    }
    // the fillets: at a corner inside the work the outline turns in round the hole, and the square of the radius
    // inside the hole at the corner takes the ground inside the arc alone, what was drawn standing outside it
    GuiRect fillet[4];
    float centre_x[4];
    float centre_y[4];
    unsigned int fillets = 0u;
    for (unsigned int corner = 0u; (read.reach > 0u) && (corner < 4u); corner += 1u)
    {
        if (!read.on_x[corner] && !read.on_y[corner])
        {
            fillet[fillets] = raster_fillet_square(&read, corner, &centre_x[fillets], &centre_y[fillets]);
            fillets += 1u;
        }
    }
    const GuiRect on = gui_rect_met(read.hole, raster_rect(raster));
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            float share = 1.0f;
            for (unsigned int at = 0u; at < fillets; at += 1u)
            {
                if (gui_rect_holds(fillet[at], column, row))
                {
                    share = raster_arc_share(centre_x[at], centre_y[at], read.reach, column, row);
                }
            }
            if (share > 0.0f)
            {
                // a share of 255 by a factor from 0 to 1, rounded, is a byte
                raster_put(raster, column, row, raster_ground_at(ground, column, row), (unsigned int)((255.0f * share) + 0.5f));
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
        float turn_x = 0.0f;
        float turn_y = 0.0f;
        const GuiRect square = raster_turn_square(&read, corner, &turn_x, &turn_y);
        const GuiRect turn_on = gui_rect_met(square, raster_rect(raster));
        for (int row = turn_on.y; row < turn_on.y + turn_on.height; row += 1)
        {
            for (int column = turn_on.x; column < turn_on.x + turn_on.width; column += 1)
            {
                const float outside = 1.0f - raster_arc_share(turn_x, turn_y, read.reach, column, row);
                if (outside > 0.0f)
                {
                    // a share of 255 by a factor from 0 to 1, rounded, is a byte
                    raster_put(raster, column, row, raster_ground_at(ground, column, row), (unsigned int)((255.0f * outside) + 0.5f));
                }
            }
        }
    }
}

// a line's `color` put at x, y by `cover`, 0 to 1, of the color's alpha
static void raster_line_put(GuiRaster *raster, int x, int y, GuiColor color, float cover)
{
    const float within = (cover < 0.0f) ? 0.0f : ((cover > 1.0f) ? 1.0f : cover);
    // an alpha by a factor from 0 to 1, rounded, is a byte
    const unsigned int share = (unsigned int)(((float)GUI_ALPHA_OF(color) * within) + 0.5f);
    if (share != 0u)
    {
        raster_put(raster, x, y, color, share);
    }
}

// the color and the width of the line of the edge `notch` reaches, the head's before the foot's before the left's
// before the right's; the right's where it reaches none
static GuiColor raster_notch_line(const RasterNotch *notch, const GuiOutline *lines, int *wide)
{
    if (notch->on_y[0])
    {
        *wide = lines->top_wide;
        return lines->top;
    }
    if (notch->on_y[2])
    {
        *wide = lines->bottom_wide;
        return lines->bottom;
    }
    if (notch->on_x[0])
    {
        *wide = lines->left_wide;
        return lines->left;
    }
    *wide = lines->right_wide;
    return lines->right;
}

// the work's own corner `corner`, top left, top right, bottom left or bottom right, laid: the square of the radius
// at it, each pixel cut to the `clear` ground outside the arc of `inside` rounded by a pixel less than the radius,
// the line of the nearer edge laid between that arc and the work's, and cut to the `table` outside the work's arc
static void raster_corner_outlined(GuiRaster *raster, GuiRect work, GuiRect inside, unsigned int radius, const GuiOutline *lines,
                                   unsigned int corner, GuiUnder table, GuiUnder clear)
{
    // the square's side is the radius, which an int holds
    const int side = (int)radius;
    const int at_right = (corner & 1u) != 0u;
    const int at_bottom = corner >= 2u;
    const GuiRect square = {at_right ? work.x + work.width - side : work.x, at_bottom ? work.y + work.height - side : work.y, side, side};
    const GuiRect on = gui_rect_met(square, raster_rect(raster));
    const unsigned int inner_radius = (radius > 0u) ? radius - 1u : 0u;
    const GuiColor across = at_bottom ? lines->bottom : lines->top;
    const GuiColor down = at_right ? lines->right : lines->left;
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            const float outer = raster_corner_share(work, radius, x, y);
            const float inner = gui_rect_holds(inside, x, y) ? raster_corner_share(inside, inner_radius, x, y) : 0.0f;
            // a share of 255 by a factor from 0 to 1, rounded, is a byte
            raster_put(raster, x, y, raster_ground_at(clear, x, y), (unsigned int)((255.0f * (1.0f - inner)) + 0.5f));
            // the nearer edge's line: how far the pixel's middle stands in from the edge across and the edge down
            const float from_across = at_bottom ? (float)(work.y + work.height) - ((float)y + 0.5f) : ((float)y + 0.5f) - (float)work.y;
            const float from_down = at_right ? (float)(work.x + work.width) - ((float)x + 0.5f) : ((float)x + 0.5f) - (float)work.x;
            raster_line_put(raster, x, y, (from_across <= from_down) ? across : down, outer - inner);
            raster_put(raster, x, y, raster_ground_at(table, x, y), (unsigned int)((255.0f * (1.0f - outer)) + 0.5f));
        }
    }
}

// the line of an edge laid along it between the work's corners, `from` to `to` along the edge, less the span of each
// of the `count` holes reaching the edge grown by the radius on either side, where the line turns out round them
static void raster_edge_lined(GuiRaster *raster, GuiRect work, unsigned int radius, int across, int at, int wide, GuiColor color,
                              int from, int to, const GuiRect *holes, unsigned int count)
{
    // the holes' spans along the edge, each grown by the radius, which an int holds
    const int side = (int)radius;
    int low[4];
    int high[4];
    unsigned int spans = 0u;
    for (unsigned int hole = 0u; (hole < count) && (spans < 4u); hole += 1u)
    {
        const int reaches = across ? ((at == work.y) ? holes[hole].y <= work.y : holes[hole].y + holes[hole].height >= work.y + work.height)
                                   : ((at == work.x) ? holes[hole].x <= work.x : holes[hole].x + holes[hole].width >= work.x + work.width);
        if (!reaches)
        {
            continue;
        }
        low[spans] = (across ? holes[hole].x : holes[hole].y) - side;
        high[spans] = (across ? holes[hole].x + holes[hole].width : holes[hole].y + holes[hole].height) + side;
        spans += 1u;
    }
    // the line laid piece by piece: from each point along the edge to the next hole's span, then from that span's end
    int begin = from;
    while (begin < to)
    {
        int end = to;
        int next = to;
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
            const GuiRect piece = across ? (GuiRect){begin, at, end - begin, wide} : (GuiRect){at, begin, wide, end - begin};
            gui_raster_filled(raster, gui_rect_met(piece, work), color);
        }
        begin = (next > begin) ? next : begin + 1;
    }
}

// the lines the hole of `notch` bends round laid: the table inside each fillet's arc and outside each turn's, and the
// line of the edge the hole reaches along the hole's sides inside the work, round the fillets and the turns
static void raster_notch_outlined(GuiRaster *raster, GuiRect work, const RasterNotch *notch, const GuiOutline *lines, GuiUnder table)
{
    int wide = 0;
    const GuiColor color = raster_notch_line(notch, lines, &wide);
    // the square's side is the radius, which an int holds
    const int side = (int)notch->reach;
    const GuiRect hole = notch->hole;
    for (unsigned int corner = 0u; (notch->reach > 0u) && (corner < 4u); corner += 1u)
    {
        float centre_x = 0.0f;
        float centre_y = 0.0f;
        if (!notch->on_x[corner] && !notch->on_y[corner])
        {
            // the fillet: the line lies outside the arc, the square grown by the line's width on its sides at the corner
            const GuiRect square = raster_fillet_square(notch, corner, &centre_x, &centre_y);
            const GuiRect grown = {(notch->into_x[corner] > 0) ? square.x - wide : square.x, (notch->into_y[corner] > 0) ? square.y - wide : square.y,
                                   square.width + wide, square.height + wide};
            const GuiRect on = gui_rect_met(gui_rect_met(grown, work), raster_rect(raster));
            for (int y = on.y; y < on.y + on.height; y += 1)
            {
                for (int x = on.x; x < on.x + on.width; x += 1)
                {
                    const float inside = raster_arc_share(centre_x, centre_y, notch->reach, x, y);
                    if (gui_rect_holds(square, x, y))
                    {
                        // a share of 255 by a factor from 0 to 1, rounded, is a byte
                        raster_put(raster, x, y, raster_ground_at(table, x, y), (unsigned int)((255.0f * inside) + 0.5f));
                    }
                    // the line's width is far inside what an unsigned int represents
                    raster_line_put(raster, x, y, color, raster_arc_share(centre_x, centre_y, notch->reach + (unsigned int)wide, x, y) - inside);
                }
            }
        }
        else if (notch->on_x[corner] != notch->on_y[corner])
        {
            // the turn out: the line lies inside the arc, within its width of it
            const GuiRect square = raster_turn_square(notch, corner, &centre_x, &centre_y);
            const GuiRect on = gui_rect_met(square, raster_rect(raster));
            const unsigned int inner = (notch->reach > (unsigned int)wide) ? notch->reach - (unsigned int)wide : 0u;
            for (int y = on.y; y < on.y + on.height; y += 1)
            {
                for (int x = on.x; x < on.x + on.width; x += 1)
                {
                    const float inside = raster_arc_share(centre_x, centre_y, notch->reach, x, y);
                    // a share of 255 by a factor from 0 to 1, rounded, is a byte
                    raster_put(raster, x, y, raster_ground_at(table, x, y), (unsigned int)((255.0f * (1.0f - inside)) + 0.5f));
                    raster_line_put(raster, x, y, color, inside - raster_arc_share(centre_x, centre_y, inner, x, y));
                }
            }
        }
    }
    // the hole's sides inside the work, each a stretch of the line just outside the hole between its corners' arcs
    if (!notch->on_x[0])
    {
        gui_raster_filled(raster, gui_rect_met((GuiRect){hole.x - wide, hole.y + side, wide, hole.height - (2 * side)}, work), color);
    }
    if (!notch->on_x[1])
    {
        gui_raster_filled(raster, gui_rect_met((GuiRect){hole.x + hole.width, hole.y + side, wide, hole.height - (2 * side)}, work), color);
    }
    if (!notch->on_y[0])
    {
        gui_raster_filled(raster, gui_rect_met((GuiRect){hole.x + side, hole.y - wide, hole.width - (2 * side), wide}, work), color);
    }
    if (!notch->on_y[2])
    {
        gui_raster_filled(raster, gui_rect_met((GuiRect){hole.x + side, hole.y + hole.height, hole.width - (2 * side), wide}, work), color);
    }
}

void gui_raster_outlined(GuiRaster *raster, GuiRect work, GuiRect inside, unsigned int radius, const GuiOutline *lines,
                         const GuiRect *holes, unsigned int count, GuiUnder table, GuiUnder clear)
{
    if (gui_rect_empty(gui_rect_met(work, raster_rect(raster))))
    {
        return;
    }
    // the work's corners, each unless a hole takes it whole
    const int corner_x[4] = {work.x, work.x + work.width - 1, work.x, work.x + work.width - 1};
    const int corner_y[4] = {work.y, work.y, work.y + work.height - 1, work.y + work.height - 1};
    for (unsigned int corner = 0u; (radius > 0u) && (corner < 4u); corner += 1u)
    {
        int taken = 0;
        for (unsigned int hole = 0u; hole < count; hole += 1u)
        {
            taken = taken || gui_rect_holds(holes[hole], corner_x[corner], corner_y[corner]);
        }
        if (!taken)
        {
            raster_corner_outlined(raster, work, inside, radius, lines, corner, table, clear);
        }
    }
    // the lines along the edges between the corners
    // the radius is far inside what an int represents
    const int side = (int)radius;
    raster_edge_lined(raster, work, radius, 1, work.y, lines->top_wide, lines->top, work.x + side, work.x + work.width - side, holes, count);
    raster_edge_lined(raster, work, radius, 1, work.y + work.height - lines->bottom_wide, lines->bottom_wide, lines->bottom, work.x + side,
                      work.x + work.width - side, holes, count);
    raster_edge_lined(raster, work, radius, 0, work.x, lines->left_wide, lines->left, work.y + side, work.y + work.height - side, holes, count);
    raster_edge_lined(raster, work, radius, 0, work.x + work.width - lines->right_wide, lines->right_wide, lines->right, work.y + side,
                      work.y + work.height - side, holes, count);
    // each hole's turns and the line round it
    for (unsigned int hole = 0u; hole < count; hole += 1u)
    {
        RasterNotch read;
        if (raster_notch_read(work, holes[hole], radius, &read))
        {
            raster_notch_outlined(raster, work, &read, lines, table);
        }
    }
}

void gui_raster_line(GuiRaster *raster, int x0, int y0, int x1, int y1, GuiColor color)
{
    const GuiRect on = raster_rect(raster);
    const unsigned int alpha = GUI_ALPHA_OF(color);
    const int step_x = (x1 > x0) ? 1 : -1;
    const int step_y = (y1 > y0) ? 1 : -1;
    const int span_x = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    const int span_y = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int error = span_x - span_y;
    int x = x0;
    int y = y0;
    for (;;)
    {
        if (gui_rect_holds(on, x, y))
        {
            raster_put(raster, x, y, color, alpha);
        }
        if ((x == x1) && (y == y1))
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

void gui_raster_masked(GuiRaster *raster, const GuiMask *mask, int x, int y, GuiColor color, GuiRect clip)
{
    // a mask's sides are far inside what an int represents
    const GuiRect laid = {x, y, (int)mask->width, (int)mask->height};
    const GuiRect on = gui_rect_met(gui_rect_met(laid, clip), raster_rect(raster));
    const unsigned int alpha = GUI_ALPHA_OF(color);
    if (gui_rect_empty(on) || (alpha == 0u))
    {
        return;
    }
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        // the row and column inside the mask are not negative, the rectangle drawn lying inside it
        const unsigned char *const cover = mask->cover + ((unsigned int)(row - y) * mask->width);
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            const unsigned int share = cover[(unsigned int)(column - x)];
            if (share != 0u)
            {
                // two shares of 255 multiplied are a share of 255 once divided by 255
                raster_put(raster, column, row, color, ((share * alpha) + 127u) / 255u);
            }
        }
    }
}

void gui_raster_lanes_laid(GuiRaster *raster, const unsigned int *red_blue, const unsigned int *green, const unsigned int *rest,
                           unsigned int width, unsigned int height, int x, int y, GuiRect clip)
{
    // a picture's sides are far inside what an int represents
    const GuiRect laid = {x, y, (int)width, (int)height};
    const GuiRect on = gui_rect_met(gui_rect_met(laid, clip), raster_rect(raster));
    if (gui_rect_empty(on))
    {
        return;
    }
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        // the row and column inside the picture are not negative, the rectangle laid lying inside it; the pixel lies
        // on the raster: its coordinates are not negative
        const unsigned int from = ((unsigned int)(row - y) * width) + (unsigned int)(on.x - x);
        const unsigned int *const ink_red_blue = red_blue + from;
        const unsigned int *const ink_green = green + from;
        const unsigned int *const kept = rest + from;
        GuiColor *const out = &raster->pixels[((unsigned int)row * raster->width) + (unsigned int)on.x];
        // the row's width is not negative
        for (unsigned int px = 0u; px < (unsigned int)on.width; px += 1u)
        {
            const GuiColor ground = out[px];
            const unsigned int lanes = ink_red_blue[px] + ((ground & 0x00ff00ffu) * kept[px]);
            const unsigned int lane = ink_green[px] + (((ground >> 8u) & 0xffu) * kept[px]);
            out[px] = 0xff000000u | (((lanes + ((lanes >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu) |
                      ((((lane + (lane >> 8u)) >> 8u) & 0xffu) << 8u);
        }
    }
}

void gui_raster_stenciled(GuiRaster *raster, const GuiMask *mask, GuiColor color)
{
    const unsigned long long pixels = (unsigned long long)mask->width * mask->height;
    const unsigned int most = (raster->width * raster->height < pixels) ? raster->width * raster->height : (unsigned int)pixels;
    for (unsigned int at = 0u; at < most; at += 1u)
    {
        raster->pixels[at] = ((GuiColor)mask->cover[at] << 24u) | (color & 0x00ffffffu);
    }
}

// the 4 by 4 ordered threshold at x, y, 0 to 15, as the page's shader orders it: the pixel's own parity across and
// down weighs most, its pair's parity the rest
static unsigned int raster_ordered(int x, int y)
{
    // a coordinate's low bits read the same whatever its sign
    const unsigned int across = (unsigned int)x;
    const unsigned int down = (unsigned int)y;
    return ((across & 1u) << 3u) | ((down & 1u) << 2u) | (((across >> 1u) & 1u) << 1u) | ((down >> 1u) & 1u);
}

// a channel of `color` put down over `ground` with the dot's `share` of it from 0 to 1 and the dither's `threshold`
// in levels of 255: the color's share and the threshold laid over the ground's rest, as the page's shader blends a
// premultiplied dot over what stands
static unsigned int raster_dot_channel(unsigned int ground, unsigned int color, float share, float threshold)
{
    const float laid = ((float)color * share) + threshold;
    const float within = (laid < 0.0f) ? 0.0f : ((laid > 255.0f) ? 255.0f : laid);
    const float alpha = (share * 255.0f) + threshold;
    const float rest = (alpha < 0.0f) ? 1.0f : ((alpha > 255.0f) ? 0.0f : 1.0f - (alpha / 255.0f));
    const float out = within + ((float)ground * rest);
    // a channel from 0 to 255, rounded, is a byte
    return (out >= 255.0f) ? 255u : (unsigned int)(out + 0.5f);
}

void gui_raster_grained(GuiRaster *raster, GuiRect rect, unsigned int radius, const unsigned char *alpha, unsigned int tile)
{
    const GuiRect on = gui_rect_met(rect, raster_rect(raster));
    if (gui_rect_empty(on) || (tile == 0u))
    {
        return;
    }
    const GuiColor white = GUI_RGB(0xffu, 0xffu, 0xffu);
    // the corners' squares are radius by radius, which an int holds; a row through them reads each pixel's share
    // inside the arcs, every other row lays the grain whole
    const int side = (int)radius;
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        // the pixel lies on the raster: its coordinates are not negative; the tile's row and the column it starts
        // at are read once a row and the column counted on, wrapping at the tile's side
        const unsigned char *const tile_row = alpha + (((unsigned int)y % tile) * tile);
        unsigned int column = (unsigned int)on.x % tile;
        const int through_corners = (side > 0) && ((y < rect.y + side) || (y >= rect.y + rect.height - side));
        GuiColor *pixel = &raster->pixels[((unsigned int)y * raster->width) + (unsigned int)on.x];
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            unsigned int grain = tile_row[column];
            column = (column + 1u == tile) ? 0u : column + 1u;
            if ((grain != 0u) && through_corners)
            {
                // a byte by a factor from 0 to 1, rounded, is a byte
                grain = (unsigned int)(((float)grain * raster_corner_share(rect, radius, x, y)) + 0.5f);
            }
            if (grain != 0u)
            {
                *pixel = raster_blended(*pixel, white, grain);
            }
            pixel += 1;
        }
    }
}

void gui_raster_dotted(GuiRaster *raster, float x, float y, float radius, GuiColor color)
{
    const float ink = (float)GUI_ALPHA_OF(color) / 255.0f;
    const float reach = radius + 1.0f;
    // the square about the dot, its sides whole pixels, is far inside what an int represents
    const int left = (int)floorf(x - reach);
    const int top = (int)floorf(y - reach);
    const GuiRect square = {left, top, (int)ceilf(x + reach) - left + 1, (int)ceilf(y + reach) - top + 1};
    const GuiRect on = gui_rect_met(square, raster_rect(raster));
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            const float inside = radius - hypotf(((float)column + 0.5f) - x, ((float)row + 0.5f) - y) + 0.5f;
            if (inside <= 0.0f)
            {
                continue;
            }
            const float share = ink * ((inside < 1.0f) ? inside : 1.0f);
            const float threshold = ((float)raster_ordered(column, row) - 7.5f) / 16.0f;
            // the pixel lies on the raster: its coordinates are not negative
            GuiColor *const at = &raster->pixels[((unsigned int)row * raster->width) + (unsigned int)column];
            *at = GUI_RGB(raster_dot_channel(GUI_RED_OF(*at), GUI_RED_OF(color), share, threshold),
                          raster_dot_channel(GUI_GREEN_OF(*at), GUI_GREEN_OF(color), share, threshold),
                          raster_dot_channel(GUI_BLUE_OF(*at), GUI_BLUE_OF(color), share, threshold));
        }
    }
}
