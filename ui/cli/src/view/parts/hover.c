// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hover.c: the hover panel, its box laid under its word as the panels over the editor carry it
#include "hover.h"

#include "../../command_line/cli_internal.h"
#include "../frame.h"

void gui_hover_init(const GuiHoverCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->hover, .val = 0, .bytes = sizeof(*args->hover)});
}

int gui_hover_holds(const GuiHoverXCfg *args)
{
    return (args->hover_read->panel != NULL) && EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                                                           .rect = args->hover_read->rect, .x = args->x, .y = args->y);
}

void gui_hover_draw(const GuiHoverRasterCfg *args)
{
    int scale = args->scale;
    const CliHoverPanel *const panel = args->hover->panel;
    if (panel == NULL)
    {
        return;
    }
    const int pad_x = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HOVER_PAD_X, .scale = scale);
    const int pad_y = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HOVER_PAD_Y, .scale = scale);
    int pixel_width = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HOVER_WIDTH, .scale = scale);
    pixel_width = (pixel_width > args->bounds.width) ? args->bounds.width : pixel_width;
    const int inner = pixel_width - (2 * pad_x);
    const int measured =
        EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = NULL, .glyphs = args->glyphs,
                   .palette = args->palette, .text = panel->text, .length = panel->length, .x = 0, .y = 0,
                   .pixel_width = inner, .scale = scale, .clip = args->bounds);
    int pixel_height = measured + (2 * pad_y);
    const int most = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HOVER_HEIGHT, .scale = scale);
    pixel_height = (pixel_height > most) ? most : pixel_height;
    args->hover->rect =
        EMBED_CALL(gui_panel_width.place, GuiPanelWidthCfg, .anchor = args->anchor, .pixel_width = pixel_width,
                   .pixel_height = pixel_height, .back = 0, .bounds = args->bounds, .above = 0, .scale = scale);
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = args->raster, .palette = args->palette,
               .rect = args->hover->rect, .scale = scale);
    const GuiRect clip = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg,
        .one = (GuiRect){args->hover->rect.x + pad_x, args->hover->rect.y + pad_y, inner, pixel_height - (2 * pad_y)},
        .other = args->bounds);
    (void)EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = args->raster, .glyphs = args->glyphs,
                     .palette = args->palette, .text = panel->text, .length = panel->length,
                     .x = args->hover->rect.x + pad_x, .y = args->hover->rect.y + pad_y, .pixel_width = inner,
                     .scale = scale, .clip = clip);
}
