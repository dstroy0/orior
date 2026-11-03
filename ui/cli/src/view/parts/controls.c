// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controls.c: the three window controls at the bar's right end, each its glyph in a box of GUI_CONTROL_GLYPH pixels
// centred in its button: minimize a line across the middle, maximize a square, close two diagonals, as the page's
// icons draw them
#include "chrome.h"

#include "../../command_line/cli_internal.h"
#include "glass.h"

typedef struct
{
    const GuiFrame *frame;
    unsigned int which;
    unsigned char *line;
    unsigned char *other;
    unsigned int count;
    unsigned int step;
    unsigned int box;
    int x;
    int y;
    int pixel_width;
    int pixel_height;
    int radius;
    GuiMask *shadow;
    GuiColor color;
    int share;
} ControlsCtx;

// the rectangle of the control `which` among the frame's controls
EMBED_INLINE GuiRect controls_button(const ControlsCtx *args)
{
    const int pixel_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROL_WIDTH, .scale = args->frame->scale);
    const int gap =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROL_GAP, .scale = args->frame->scale);
    // a control's index is one of three
    const int at = args->frame->controls.x + ((int)args->which * (pixel_width + gap));
    const GuiRect button = {at, args->frame->controls.y, pixel_width, args->frame->controls.height};
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
EMBED_INLINE void controls_box_blur(const ControlsCtx *args)
{
    if ((args->box == 0u) || (args->count == 0u))
    {
        return;
    }
    const unsigned int half = args->box / 2u;
    unsigned int sum = 0u;
    for (unsigned int at = 0u; at < args->box; at += 1u)
    {
        sum += args->line[((at > half) ? ((at - half < args->count) ? at - half : args->count - 1u) : 0u) * args->step];
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        // the mean of a box of bytes, rounded, is a byte
        args->other[at] = (unsigned char)((sum + (args->box / 2u)) / args->box);
        const unsigned int out = (at >= half) ? at - half : 0u;
        const unsigned int in = (at + args->box - half < args->count) ? at + args->box - half : args->count - 1u;
        sum = sum + args->line[in * args->step] - args->line[out * args->step];
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        args->line[at * args->step] = args->other[at];
    }
}

// whether the pixel at x, y of a box `pixel_width` by `pixel_height` with its corners rounded by `radius` lies inside
// it
EMBED_INLINE int controls_in_round(const ControlsCtx *args)
{
    if ((args->x < 0) || (args->y < 0) || (args->x >= args->pixel_width) || (args->y >= args->pixel_height))
    {
        return 0;
    }
    const int across =
        (args->x < args->radius)
            ? args->radius - args->x
            : ((args->x >= args->pixel_width - args->radius) ? args->x - (args->pixel_width - args->radius) + 1 : 0);
    const int down =
        (args->y < args->radius)
            ? args->radius - args->y
            : ((args->y >= args->pixel_height - args->radius) ? args->y - (args->pixel_height - args->radius) + 1 : 0);
    return (across == 0) || (down == 0) || (((across * across) + (down * down)) <= (args->radius * args->radius));
}

// the shadow's mask made for the frame's pane: the pane's rounded box blurred by the blur, less the box where the pane
// itself stands, as a box's shadow is not drawn under the box; a mask of its size saved is the one wanted
EMBED_INLINE void controls_shadow_init(const ControlsCtx *args)
{
    GuiMask *shadow = args->shadow;
    const int reach = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = CONTROLS_SHADOW_REACH,
                                 .scale = args->frame->scale);
    const int down = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = CONTROLS_SHADOW_DOWN,
                                .scale = args->frame->scale);
    const GuiRect pane = args->frame->controls_pane;
    // the pane's sides and the reach are not negative
    const unsigned int width = (unsigned int)(pane.width + (2 * reach));
    const unsigned int height = (unsigned int)(pane.height + (2 * reach));
    if ((shadow->cover != NULL) && (shadow->width == width) && (shadow->height == height))
    {
        return;
    }
    EMBED_CALL(gui_chrome_raster.controls_shadow_free, GuiChromeRasterCfg, .shadow = shadow);
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
    const int radius = (int)args->frame->radius;
    for (unsigned int y = 0u; y < height; y += 1u)
    {
        for (unsigned int x = 0u; x < width; x += 1u)
        {
            // the mask's sides are far inside what an int represents
            cover[(y * width) + x] =
                EMBED_CALL(controls_in_round, ControlsCtx, .x = (int)x - reach, .y = (int)y - reach,
                           .pixel_width = pane.width, .pixel_height = pane.height, .radius = radius)
                    ? 255u
                    : 0u;
        }
    }
    // the box's width at the scale is positive, and an even width blurs a half pixel off, the next pass taking it back
    const unsigned int box = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                      .pixels = CONTROLS_SHADOW_BOX, .scale = args->frame->scale);
    for (unsigned int pass = 0u; pass < CONTROLS_SHADOW_BOXES; pass += 1u)
    {
        for (unsigned int y = 0u; y < height; y += 1u)
        {
            EMBED_CALL(controls_box_blur, ControlsCtx, .line = &cover[y * width], .other = other, .count = width,
                       .step = 1u, .box = box);
        }
        for (unsigned int x = 0u; x < width; x += 1u)
        {
            EMBED_CALL(controls_box_blur, ControlsCtx, .line = &cover[x], .other = other, .count = height,
                       .step = width, .box = box);
        }
    }
    // the pane stands `down` above where the shadow is laid: none of the shadow under it
    for (unsigned int y = 0u; y < height; y += 1u)
    {
        for (unsigned int x = 0u; x < width; x += 1u)
        {
            if (EMBED_CALL(controls_in_round, ControlsCtx, .x = (int)x - reach, .y = (int)y - reach + down,
                           .pixel_width = pane.width, .pixel_height = pane.height, .radius = radius))
            {
                cover[(y * width) + x] = 0u;
            }
        }
    }
    g_cli_cells->persistent_buf_release(other, &g_gui_cells);
    *shadow = (GuiMask){width, height, 0, 0, cover};
}

void gui_controls_shadow_free(const GuiChromeRasterCfg *args)
{
    g_cli_cells->persistent_buf_release(args->shadow->cover, &g_gui_cells);
    args->shadow->cover = NULL;
    args->shadow->width = 0u;
    args->shadow->height = 0u;
}

// `color` at `share`, a number of GUI_FIXED_ONE, of its alpha
EMBED_INLINE GuiColor controls_fade(const ControlsCtx *args)
{
    const int within = (args->share < 0) ? 0 : ((args->share > GUI_FIXED_ONE) ? GUI_FIXED_ONE : args->share);
    // an alpha by a share of GUI_FIXED_ONE, rounded, is a byte
    return (args->color & 0x00ffffffu) |
           ((GuiColor)((((int)GUI_ALPHA_OF(args->color) * within) + (GUI_FIXED_ONE / 2)) / GUI_FIXED_ONE) << 24u);
}

void gui_controls_draw(const GuiChromeRasterCfg *args)
{
    const int box =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROL_GLYPH, .scale = args->frame->scale);
    if (args->frame->controls_pane_share > 0)
    {
        EMBED_CALL(controls_shadow_init, ControlsCtx, .shadow = args->shadow, .frame = args->frame);
        if (args->shadow->cover != NULL)
        {
            const int reach = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = CONTROLS_SHADOW_REACH,
                                         .scale = args->frame->scale);
            // the raster's sides are far inside what an int represents
            EMBED_CALL(gui_raster_xy_color.mask, GuiRasterXYColorCfg, .raster = args->raster, .mask = args->shadow,
                       .x = args->frame->controls_pane.x - reach,
                       .y = args->frame->controls_pane.y - reach +
                            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = CONTROLS_SHADOW_DOWN,
                                       .scale = args->frame->scale),
                       .color = EMBED_CALL(controls_fade, ControlsCtx, .color = args->palette->lifted,
                                           .share = args->frame->controls_pane_share),
                       .clip = (GuiRect){0, 0, (int)args->raster->width, (int)args->raster->height});
        }
    }
    // the controls' own pane, with its lip, while they stand off the bar as their tab or its ghost: a pane on the
    // table whose tint thickens from the sheet's to a pane's by the share, whole while it comes down, and its
    // corners alone once it stands, the base laying the rest of it
    const GuiColor pane = EMBED_CALL(gui_glass.pane_tint, GuiGlassCfg, .glass = args->palette->glass);
    if (args->frame->controls_pane_share >= GUI_FIXED_ONE)
    {
        EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = args->raster,
                   .background_fill = args->table, .rect = args->frame->controls_pane, .radius = args->frame->radius,
                   .inner = pane, .corners_only = 1);
    }
    else if (args->frame->controls_pane_share > 0)
    {
        const int sheet = (int)GUI_ALPHA_OF(args->table.over);
        const int share = args->frame->controls_pane_share;
        // an alpha between two bytes by a share of GUI_FIXED_ONE, rounded, is a byte
        const GuiColor alpha =
            (GuiColor)(sheet + (((((int)GUI_ALPHA_OF(pane) - sheet) * share) + (GUI_FIXED_ONE / 2)) / GUI_FIXED_ONE));
        EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = args->raster,
                   .background_fill = args->table, .rect = args->frame->controls_pane, .radius = args->frame->radius,
                   .inner = (pane & 0x00ffffffu) | (alpha << 24u), .corners_only = 0);
    }
    const GuiColor text_color = EMBED_CALL(controls_fade, ControlsCtx, .color = args->palette->status_text,
                                           .share = args->frame->controls_alpha);
    for (unsigned int which = GUI_CONTROL_MINIMIZE; which <= GUI_CONTROL_CLOSE; which += 1u)
    {
        const GuiRect button = EMBED_CALL(controls_button, ControlsCtx, .frame = args->frame, .which = which);
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
            EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = low, .y0 = middle,
                       .x1 = high, .y1 = middle, .color = text_color);
        }
        else if (which == GUI_CONTROL_MAXIMIZE)
        {
            EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = low, .y0 = above,
                       .x1 = high, .y1 = above, .color = text_color);
            EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = low, .y0 = below,
                       .x1 = high, .y1 = below, .color = text_color);
            EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = low, .y0 = above,
                       .x1 = low, .y1 = below, .color = text_color);
            EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = high, .y0 = above,
                       .x1 = high, .y1 = below, .color = text_color);
        }
        else
        {
            EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = low, .y0 = above,
                       .x1 = high, .y1 = below, .color = text_color);
            EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = high, .y0 = above,
                       .x1 = low, .y1 = below, .color = text_color);
        }
    }
}

GuiRect gui_control_rect(const GuiChromeCfg *args)
{
    return EMBED_CALL(controls_button, ControlsCtx, .frame = args->frame, .which = args->which);
}

unsigned int gui_controls_hit(const GuiChromeCfg *args)
{
    for (unsigned int which = GUI_CONTROL_MINIMIZE; which <= GUI_CONTROL_CLOSE; which += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(controls_button, ControlsCtx, .frame = args->frame, .which = which),
                       .x = args->x, .y = args->y))
        {
            return which;
        }
    }
    return GUI_CONTROL_NONE;
}
