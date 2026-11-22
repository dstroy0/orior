// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dots.c: the state dots read from the overseer and drawn as round marks, the blinking ones shown for half of each
// blink (dots.h)
#include "dots.h"

#include "../../../includes/window/frame.h"
#include "../../cli/src/machine/overseer.h"

GuiColor gui_dots_color(const GuiDotsCfg *args)
{
    switch (args->state)
    {
    case CLI_OVERSEER_STARTING:
    case CLI_OVERSEER_UP:
        return args->palette->dot_up;
    case CLI_OVERSEER_STOPPING:
        return args->palette->dot_stopping;
    case CLI_OVERSEER_ERROR:
    case CLI_OVERSEER_CRITICAL:
        return args->palette->dot_failed;
    default:
        return GUI_RGBA(0u, 0u, 0u, 0u);
    }
}

// one dot of the state `state`, its right edge at `right` and its center on the middle of the row. Where the state
// blinks and the blink is in its dark half, the dot keeps its place and shows nothing
EMBED_INLINE void dots_one_draw(const GuiDotsCfg *args)
{
    const int side =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_DOTS_SIDE, .scale = args->scale);
    if (!EMBED_CALL(cli_overseer.lit, CliOverseerCfg, .state = args->state, .now_ms = args->now_ms))
    {
        return;
    }
    const GuiRect dot = {args->right - side, args->top + ((args->height - side) / 2), side, side};
    // the side is a few pixels, far below what an unsigned int counts
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = dot, .other = args->clip),
               .color = EMBED_CALL(gui_dots_color_of.color, GuiDotsCfg, .palette = args->palette, .state = args->state),
               .radius = (unsigned int)(side / 2));
}

int gui_dots_draw(const GuiDotsCfg *args)
{
    const int side =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_DOTS_SIDE, .scale = args->scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_DOTS_GAP, .scale = args->scale);
    GuiDotsCfg one = *args;
    if (args->worst)
    {
        one.state = EMBED_CALL(cli_overseer.worst, CliOverseerCfg, .role = args->role);
        if (one.state == CLI_OVERSEER_NONE)
        {
            return args->right;
        }
        EMBED_CALL(dots_one_draw, GuiDotsCfg, .raster = one.raster, .palette = one.palette, .clip = one.clip,
                   .right = one.right, .top = one.top, .height = one.height, .scale = one.scale, .state = one.state,
                   .now_ms = one.now_ms);
        return args->right - side;
    }
    CliOverseerSeen seen[GUI_DOTS_MAX];
    const unsigned int count =
        EMBED_CALL(cli_overseer.seen, CliOverseerCfg, .role = args->role, .seen_out = seen, .max = GUI_DOTS_MAX);
    // the dots go right to left, the last name rightmost
    int right = args->right;
    for (unsigned int at = count; at > 0u; at -= 1u)
    {
        EMBED_CALL(dots_one_draw, GuiDotsCfg, .raster = one.raster, .palette = one.palette, .clip = one.clip,
                   .right = right, .top = one.top, .height = one.height, .scale = one.scale,
                   .state = seen[at - 1u].state, .now_ms = one.now_ms);
        right -= side + gap;
    }
    return (count > 0u) ? right + gap : args->right;
}
