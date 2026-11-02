// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controls.c: the three window controls at the bar's right end, each its glyph in a box of GUI_CONTROL_GLYPH pixels
// centred in its button: minimize a line across the middle, maximize a square, close two diagonals, as the page's
// icons draw them
#include "chrome.h"

#include "../../../cli/src/command_line/cli_internal.h"
#include "glass.h"

// the rectangle of the control `which` among the frame's controls
static GuiRect controls_button(const GuiFrame *frame, unsigned int which)
{
    const int wide = gui_scaled(GUI_CONTROL_WIDE, frame->scale);
    const int gap = gui_scaled(GUI_CONTROL_GAP, frame->scale);
    // a control's index is one of three
    const int at = frame->controls.x + ((int)which * (wide + gap));
    const GuiRect button = {at, frame->controls.y, wide, frame->controls.height};
    return button;
}

// the tab's box-shadow, 0 6px 18px, in the page's pixels: how far it falls, its blur's width, three box blurs of 18
// standing in for the blur's gaussian of nine, and how far the blur reaches past the tab, three of the gaussian's
#define CONTROLS_SHADOW_DOWN 6
#define CONTROLS_SHADOW_BOX 18
#define CONTROLS_SHADOW_BOXES 3u
#define CONTROLS_SHADOW_REACH 27

// `count` bytes, `step` apart, blurred by a box `box` wide into `other`, then back: each byte the mean of the box about
// it, a run's ends taken as what lies past them
static void controls_box_blurred(unsigned char *line, unsigned char *other, unsigned int count, unsigned int step, unsigned int box)
{
    const unsigned int half = box / 2u;
    unsigned int sum = 0u;
    for (unsigned int at = 0u; at < box; at += 1u)
    {
        sum += line[((at > half) ? ((at - half < count) ? at - half : count - 1u) : 0u) * step];
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        // the mean of a box of bytes, rounded, is a byte
        other[at] = (unsigned char)((sum + (box / 2u)) / box);
        const unsigned int out = (at >= half) ? at - half : 0u;
        const unsigned int in = (at + box - half < count) ? at + box - half : count - 1u;
        sum = sum + line[in * step] - line[out * step];
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        line[at * step] = other[at];
    }
}

// whether the pixel at x, y of a box `wide` by `tall` with its corners rounded by `radius` lies inside it
static int controls_in_rounded(int x, int y, int wide, int tall, int radius)
{
    if ((x < 0) || (y < 0) || (x >= wide) || (y >= tall))
    {
        return 0;
    }
    const int across = (x < radius) ? radius - x : ((x >= wide - radius) ? x - (wide - radius) + 1 : 0);
    const int down = (y < radius) ? radius - y : ((y >= tall - radius) ? y - (tall - radius) + 1 : 0);
    return (across == 0) || (down == 0) || (((across * across) + (down * down)) <= (radius * radius));
}

// the shadow's mask made for the frame's pane: the pane's rounded box blurred by the blur, less the box where the pane
// itself stands, as a box's shadow is not drawn under the box; a mask of its size kept is the one wanted
static void controls_shadow_made(GuiMask *shadow, const GuiFrame *frame)
{
    const int reach = gui_scaled(CONTROLS_SHADOW_REACH, frame->scale);
    const int down = gui_scaled(CONTROLS_SHADOW_DOWN, frame->scale);
    const GuiRect pane = frame->controls_pane;
    // the pane's sides and the reach are not negative
    const unsigned int width = (unsigned int)(pane.width + (2 * reach));
    const unsigned int height = (unsigned int)(pane.height + (2 * reach));
    if ((shadow->cover != NULL) && (shadow->width == width) && (shadow->height == height))
    {
        return;
    }
    gui_controls_shadow_given(shadow);
    const unsigned long long pixels = (unsigned long long)width * height;
    const unsigned int longer = (width > height) ? width : height;
    unsigned char *const cover = g_cli_cells->persistent_buf_alloc(pixels, &g_gui_cells);
    unsigned char *const other = g_cli_cells->temporary_buf_alloc(longer, &g_gui_cells);
    if ((cover == NULL) || (other == NULL))
    {
        g_cli_cells->persistent_buf_release(cover, &g_gui_cells);
        g_cli_cells->persistent_buf_release(other, &g_gui_cells);
        return;
    }
    const int radius = (int)frame->radius;
    for (unsigned int y = 0u; y < height; y += 1u)
    {
        for (unsigned int x = 0u; x < width; x += 1u)
        {
            // the mask's sides are far inside what an int represents
            cover[(y * width) + x] = controls_in_rounded((int)x - reach, (int)y - reach, pane.width, pane.height, radius) ? 255u : 0u;
        }
    }
    // the box's width at the scale is positive, and an even width blurs a half pixel off, the next pass taking it back
    const unsigned int box = (unsigned int)gui_scaled(CONTROLS_SHADOW_BOX, frame->scale);
    for (unsigned int pass = 0u; pass < CONTROLS_SHADOW_BOXES; pass += 1u)
    {
        for (unsigned int y = 0u; y < height; y += 1u)
        {
            controls_box_blurred(&cover[y * width], other, width, 1u, box);
        }
        for (unsigned int x = 0u; x < width; x += 1u)
        {
            controls_box_blurred(&cover[x], other, height, width, box);
        }
    }
    // the pane stands `down` above where the shadow is laid: none of the shadow under it
    for (unsigned int y = 0u; y < height; y += 1u)
    {
        for (unsigned int x = 0u; x < width; x += 1u)
        {
            if (controls_in_rounded((int)x - reach, (int)y - reach + down, pane.width, pane.height, radius))
            {
                cover[(y * width) + x] = 0u;
            }
        }
    }
    g_cli_cells->persistent_buf_release(other, &g_gui_cells);
    *shadow = (GuiMask){width, height, 0, 0, cover};
}

void gui_controls_shadow_given(GuiMask *shadow)
{
    g_cli_cells->persistent_buf_release(shadow->cover, &g_gui_cells);
    shadow->cover = NULL;
    shadow->width = 0u;
    shadow->height = 0u;
}

// `color` at `share`, a number of GUI_WHOLE, of its alpha
static GuiColor controls_faded(GuiColor color, int share)
{
    const int within = (share < 0) ? 0 : ((share > GUI_WHOLE) ? GUI_WHOLE : share);
    // an alpha by a share of GUI_WHOLE, rounded, is a byte
    return (color & 0x00ffffffu) | ((GuiColor)((((int)GUI_ALPHA_OF(color) * within) + (GUI_WHOLE / 2)) / GUI_WHOLE) << 24u);
}

void gui_controls_drawn(GuiRaster *raster, const GuiPalette *palette, const GuiFrame *frame, GuiUnder table, GuiMask *shadow)
{
    const int box = gui_scaled(GUI_CONTROL_GLYPH, frame->scale);
    if (frame->controls_pane_share > 0)
    {
        controls_shadow_made(shadow, frame);
        if (shadow->cover != NULL)
        {
            const int reach = gui_scaled(CONTROLS_SHADOW_REACH, frame->scale);
            // the raster's sides are far inside what an int represents
            gui_raster_masked(raster, shadow, frame->controls_pane.x - reach, frame->controls_pane.y - reach + gui_scaled(CONTROLS_SHADOW_DOWN, frame->scale),
                              controls_faded(palette->lifted, frame->controls_pane_share), (GuiRect){0, 0, (int)raster->width, (int)raster->height});
        }
    }
    // the controls' own pane, with its lip, while they stand off the bar as their tab or its ghost: a pane on the
    // table whose tint thickens from the sheet's to a pane's by the share, whole while it comes down, and its
    // corners alone once it stands, the base laying the rest of it
    const GuiColor pane = gui_glass_pane_tint(palette->glass);
    if (frame->controls_pane_share >= GUI_WHOLE)
    {
        gui_raster_paned(raster, table, frame->controls_pane, frame->radius, pane, 1);
    }
    else if (frame->controls_pane_share > 0)
    {
        const int sheet = (int)GUI_ALPHA_OF(table.over);
        const int share = frame->controls_pane_share;
        // an alpha between two bytes by a share of GUI_WHOLE, rounded, is a byte
        const GuiColor alpha = (GuiColor)(sheet + (((((int)GUI_ALPHA_OF(pane) - sheet) * share) + (GUI_WHOLE / 2)) / GUI_WHOLE));
        gui_raster_paned(raster, table, frame->controls_pane, frame->radius, (pane & 0x00ffffffu) | (alpha << 24u), 0);
    }
    const GuiColor ink = controls_faded(palette->status_text, frame->controls_alpha);
    for (unsigned int which = GUI_CONTROL_MINIMIZE; which <= GUI_CONTROL_CLOSE; which += 1u)
    {
        const GuiRect button = controls_button(frame, which);
        const int left = button.x + ((button.width - box) / 2);
        const int top = button.y + ((button.height - box) / 2);
        // the glyphs' strokes in the box, as the icons stand in theirs of sixteen: the line at 3.5..12.5 by 8, the
        // square at 4..12, the diagonals from 4.2 to 11.8
        const int low = left + ((box * 4) / 16);
        const int high = left + ((box * 12) / 16) - 1;
        const int middle = top + (box / 2);
        const int above = top + ((box * 4) / 16);
        const int below = top + ((box * 12) / 16) - 1;
        if (which == GUI_CONTROL_MINIMIZE)
        {
            gui_raster_line(raster, low, middle, high, middle, ink);
        }
        else if (which == GUI_CONTROL_MAXIMIZE)
        {
            gui_raster_line(raster, low, above, high, above, ink);
            gui_raster_line(raster, low, below, high, below, ink);
            gui_raster_line(raster, low, above, low, below, ink);
            gui_raster_line(raster, high, above, high, below, ink);
        }
        else
        {
            gui_raster_line(raster, low, above, high, below, ink);
            gui_raster_line(raster, high, above, low, below, ink);
        }
    }
}

unsigned int gui_controls_hit(const GuiFrame *frame, int x, int y)
{
    for (unsigned int which = GUI_CONTROL_MINIMIZE; which <= GUI_CONTROL_CLOSE; which += 1u)
    {
        if (gui_rect_holds(controls_button(frame, which), x, y))
        {
            return which;
        }
    }
    return GUI_CONTROL_NONE;
}
