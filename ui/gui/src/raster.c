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

int gui_raster_made(GuiRaster *raster, unsigned int width, unsigned int height)
{
    const unsigned long long bytes = (unsigned long long)width * height * sizeof(GuiColor);
    raster->width = width;
    raster->height = height;
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
// and in a corner's square the distance left from the pixel's middle to the arc, clamped
static float raster_corner_share(GuiRect rect, unsigned int radius, int x, int y)
{
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
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
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

// the ground's color at x, y, a pixel on the raster: the ground raster's pixel there with the color over it, or the
// color alone where no raster is the ground
static GuiColor raster_ground_at(GuiUnder ground, int x, int y)
{
    if ((ground.raster == NULL) || (ground.raster->pixels == NULL) || !gui_rect_holds(raster_rect(ground.raster), x, y))
    {
        return ground.over;
    }
    // the pixel lies on the ground raster: its coordinates are not negative
    const GuiColor under = ground.raster->pixels[((unsigned int)y * ground.raster->width) + (unsigned int)x];
    return raster_blended(under, ground.over, GUI_ALPHA_OF(ground.over));
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

void gui_raster_notched(GuiRaster *raster, GuiRect work, GuiRect notch, unsigned int radius, GuiUnder ground)
{
    const GuiRect hole = gui_rect_met(notch, work);
    if (gui_rect_empty(hole))
    {
        return;
    }
    // the radius is no more than half the hole's sides, where the turns would meet
    const unsigned int most = (unsigned int)(((hole.width < hole.height) ? hole.width : hole.height) / 2);
    const unsigned int reach = (radius > most) ? most : radius;
    // the square's side is the radius, which an int holds
    const int side = (int)reach;
    // the hole's corners, each on the work's left or right edge, its top or bottom edge, both or neither
    const int left_edge = hole.x <= work.x;
    const int right_edge = hole.x + hole.width >= work.x + work.width;
    const int top_edge = hole.y <= work.y;
    const int bottom_edge = hole.y + hole.height >= work.y + work.height;
    const int corner_x[4] = {hole.x, hole.x + hole.width, hole.x, hole.x + hole.width};
    const int corner_y[4] = {hole.y, hole.y, hole.y + hole.height, hole.y + hole.height};
    const int on_x[4] = {left_edge, right_edge, left_edge, right_edge};
    const int on_y[4] = {top_edge, top_edge, bottom_edge, bottom_edge};
    // the way into the hole from each corner
    const int into_x[4] = {1, -1, 1, -1};
    const int into_y[4] = {1, 1, -1, -1};
    // the fillets: at a corner inside the work the outline turns in round the hole, and the square of the radius
    // inside the hole at the corner takes the ground inside the arc alone, what was drawn standing outside it
    GuiRect fillet[4];
    float centre_x[4];
    float centre_y[4];
    unsigned int fillets = 0u;
    for (unsigned int corner = 0u; (side > 0) && (corner < 4u); corner += 1u)
    {
        if (!on_x[corner] && !on_y[corner])
        {
            fillet[fillets] = (GuiRect){(into_x[corner] < 0) ? corner_x[corner] - side : corner_x[corner],
                                        (into_y[corner] < 0) ? corner_y[corner] - side : corner_y[corner], side, side};
            centre_x[fillets] = (float)(corner_x[corner] + (into_x[corner] * side));
            centre_y[fillets] = (float)(corner_y[corner] + (into_y[corner] * side));
            fillets += 1u;
        }
    }
    const GuiRect on = gui_rect_met(hole, raster_rect(raster));
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            float share = 1.0f;
            for (unsigned int at = 0u; at < fillets; at += 1u)
            {
                if (gui_rect_holds(fillet[at], column, row))
                {
                    share = raster_arc_share(centre_x[at], centre_y[at], reach, column, row);
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
    for (unsigned int corner = 0u; (side > 0) && (corner < 4u); corner += 1u)
    {
        if (on_x[corner] == on_y[corner])
        {
            continue;
        }
        const int toward_x = on_y[corner] ? -into_x[corner] : into_x[corner];
        const int toward_y = on_x[corner] ? -into_y[corner] : into_y[corner];
        raster_turned_out(raster, corner_x[corner], corner_y[corner], reach, toward_x, toward_y, ground);
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
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        // the pixel lies on the raster: its coordinates are not negative
        const unsigned char *const row = alpha + (((unsigned int)y % tile) * tile);
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            const float share = (radius == 0u) ? 1.0f : raster_corner_share(rect, radius, x, y);
            const unsigned int grain = row[(unsigned int)x % tile];
            if ((share > 0.0f) && (grain != 0u))
            {
                // a byte by a factor from 0 to 1, rounded, is a byte
                raster_put(raster, x, y, white, (unsigned int)(((float)grain * share) + 0.5f));
            }
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
