// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gutter.c: a row's change mark, as the store's diff answers it for the rows in view, positioned as a bar in the gutter's
// column before the text
#include "gutter.h"

#include "../../../includes/window/frame.h"
#include "../../cli/src/editor/document_changes.h"

typedef struct
{
    int text_x;
    int scale;
} GutterCtx;

// the marks' column: its left and its width in pixels, the bar located GUI_GUTTER_MARK_RIGHT before the text
EMBED_INLINE GuiRect gutter_column(const GutterCtx *args)
{
    const int right = args->text_x - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                .pixels = GUI_GUTTER_MARK_RIGHT, .scale = args->scale);
    const int width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_GUTTER_MARK_WIDTH, .scale = args->scale);
    return (GuiRect){right - width, 0, width, 0};
}

void gui_gutter_mark_draw(const GuiGutterCfg *args)
{
    const unsigned int mark = args->mark;
    if (mark == CLI_CHANGE_MARK_NONE)
    {
        return;
    }
    const GuiRect column = EMBED_CALL(gutter_column, GutterCtx, .text_x = args->text_x, .scale = args->scale);
    const GuiPalette *const palette = args->palette;
    GuiRect bar = {column.x, args->rect.y, column.width, args->rect.height};
    GuiColor color = (mark == CLI_CHANGE_MARK_ADDED) ? palette->mark[1] : palette->mark[2];
    if (mark == CLI_CHANGE_MARK_REMOVED)
    {
        // lines taken out after the row: a short bar across the row's foot, twice the column wide
        const int foot =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_GUTTER_MARK_FOOT, .scale = args->scale);
        bar = (GuiRect){column.x - column.width, args->rect.y + args->rect.height - (foot / 2), 2 * column.width, foot};
        color = palette->mark[0];
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = bar, .other = args->clip),
               .color = color);
}

int gui_gutter_mark_hit(const GuiGutterCfg *args)
{
    const GuiRect column = EMBED_CALL(gutter_column, GutterCtx, .text_x = args->text_x, .scale = args->scale);
    const int reach =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_GUTTER_MARK_REACH, .scale = args->scale);
    return (args->x >= column.x - reach) && (args->x < column.x + column.width + reach);
}
