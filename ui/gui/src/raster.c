// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raster.c: the pixels the window draws and all that is drawn into them. Every blend is of bytes in the colors' own
// space, a share from 0 to 255 of the color over what is there, as the page blends its colors; a pixel drawn is left
// opaque
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

// `color` over `ground` by `share` of 255: each channel's bytes blended, the pixel left opaque
static GuiColor raster_blended(GuiColor ground, GuiColor color, unsigned int share)
{
    if (share >= 255u)
    {
        return color | 0xff000000u;
    }
    const unsigned int rest = 255u - share;
    // a channel's blend by shares of 255 is a byte once divided by 255
    const unsigned int red = ((GUI_RED_OF(color) * share) + (GUI_RED_OF(ground) * rest) + 127u) / 255u;
    const unsigned int green = ((GUI_GREEN_OF(color) * share) + (GUI_GREEN_OF(ground) * rest) + 127u) / 255u;
    const unsigned int blue = ((GUI_BLUE_OF(color) * share) + (GUI_BLUE_OF(ground) * rest) + 127u) / 255u;
    return GUI_RGB(red, green, blue);
}

// the pixel at x, y of a rectangle known to lie on the raster blended with `color` by `share` of 255
static void raster_put(GuiRaster *raster, int x, int y, GuiColor color, unsigned int share)
{
    // the pixel's place is on the raster, so its coordinates are not negative
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
    for (int y = on.y; y < on.y + on.height; y += 1)
    {
        for (int x = on.x; x < on.x + on.width; x += 1)
        {
            raster_put(raster, x, y, color, alpha);
        }
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

void gui_raster_corners_cut(GuiRaster *raster, GuiRect rect, unsigned int radius, GuiColor ground)
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
                    raster_put(raster, x, y, ground, (unsigned int)((255.0f * outside) + 0.5f));
                }
            }
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
