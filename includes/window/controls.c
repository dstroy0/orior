// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controls.c: the three window controls at the top menu's right end, each its glyph in a box of GUI_CONTROL_GLYPH
// pixels centred in its button under its bead: minimize a line across the middle, maximize a square, close two
// diagonals, as the page's icons draw them
#include "controls.h"
#include "lights.h"

#include "block.h"

typedef struct
{
    const GuiFrame *frame;
    const GuiRect *controls;
    unsigned int which;
} ControlsCtx;

// the rectangle of the control `which` among the frame's controls
EMBED_INLINE GuiRect controls_button(const ControlsCtx *args)
{
    const int pixel_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROL_WIDTH, .scale = args->frame->scale);
    const int gap =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROL_GAP, .scale = args->frame->scale);
    // the row the control is laid in: the one handed in, else the frame's; a control's index is one of three
    const GuiRect controls = (args->controls != NULL) ? *args->controls : args->frame->controls;
    const int at = controls.x + ((int)args->which * (pixel_width + gap));
    const GuiRect button = {at, controls.y, pixel_width, controls.height};
    return button;
}

// The controls' beads laid out, one a control at its rest place on the top menu's row, where the row stands with no
// claim, each on the top menu tools' side centred on the control, lit by the window's lights. A control's frames
// bake the first time it is drawn
EMBED_INLINE void controls_beads_laid(const GuiControlsRasterCfg *args)
{
    const GuiFrame *const frame = args->frame;
    const int scale = frame->scale;
    const int side = frame->top_menu_tools.height;
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_PAD, .scale = scale);
    const int reach = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_REACH, .scale = scale);
    const int rest_y = ((frame->top_menu.height - side) / 2) - pad;
    GuiRect cells[GUI_CONTROLS];
    for (unsigned int which = GUI_CONTROL_MINIMIZE; which <= GUI_CONTROL_CLOSE; which += 1u)
    {
        const GuiRect button =
            EMBED_CALL(controls_button, ControlsCtx, .frame = frame, .controls = &frame->controls_rest, .which = which);
        cells[which] = (GuiRect){button.x + ((button.width - side) / 2) - pad, rest_y, 0, 0};
    }
    GuiLight lights[GUI_LIGHTS_MAX];
    const unsigned int light_count = EMBED_CALL(gui_lights_table.lights, GuiLightsCfg, .lights = lights, .frame = frame);
    // the sides are a few pixels, which an unsigned int holds; the atlas's pixels over the third segment of the
    // block's bead part
    const GuiButtonsBakeCfg sizes = {.width = (unsigned int)side,
                                     .height = (unsigned int)side,
                                     .pad = (unsigned int)pad,
                                     .reach = (reach < 3) ? 3u : (unsigned int)reach,
                                     .tools = GUI_CONTROLS};
    unsigned char *const span = EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_BEAD, .segment = 2u,
                                           .bytes = gui_buttons_bake_table.bytes(&sizes));
    (void)EMBED_CALL(gui_buttons_bake_table.bake, GuiButtonsBakeCfg, .buttons = args->buttons, .palette = args->palette,
                     .width = sizes.width, .height = sizes.height, .pad = sizes.pad, .reach = sizes.reach, .cells = cells,
                     .tools = sizes.tools, .lights = lights, .light_count = light_count,
                     .pixels = (GuiColor *)(void *)span);
}

void gui_controls_draw(const GuiControlsRasterCfg *args)
{
    EMBED_CALL(controls_beads_laid, GuiControlsRasterCfg, .buttons = args->buttons, .palette = args->palette,
               .frame = args->frame);
    const int box =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROL_GLYPH, .scale = args->frame->scale);
    // the controls' pane lays no glass and casts no shadow behind the beads: the beads stand on the table alone
    const GuiColor text_color = GUI_FRAME_FADED(args->palette->status_text, args->frame->controls_alpha);
    // each control's bead stands on the top menu tools' side, centred on the control, as faded as its glyph
    const int side = args->frame->top_menu_tools.height;
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_PAD, .scale = args->frame->scale);
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
        // the control's bead over its glyph, faded as the controls are: GUI_FIXED_ONE is 255
        EMBED_CALL(gui_buttons_draw_table.draw, GuiButtonsDrawCfg, .buttons = args->buttons, .raster = args->raster,
                   .tool = which, .x = button.x + ((button.width - side) / 2) - pad,
                   .y = button.y + ((button.height - side) / 2) - pad, .time_ms = args->time_ms,
                   .fade = (unsigned int)(((GUI_FIXED_ONE - args->frame->controls_alpha) * 255) / GUI_FIXED_ONE));
    }
}

GuiRect gui_control_rect(const GuiControlsCfg *args)
{
    return EMBED_CALL(controls_button, ControlsCtx, .frame = args->frame, .which = args->which);
}

unsigned int gui_controls_hit(const GuiControlsCfg *args)
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
