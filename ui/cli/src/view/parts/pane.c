// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pane.c: the side's panes' shared arithmetic and drawing, each in the frame's words at the frame's scale
#include "pane.h"

#include "explorer.h"

int gui_pane_rows_top(const GuiPaneCfg *args)
{
    return args->frame->side.y +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                      .pixels = GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE + GUI_EXPLORER_PANE_HEAD,
                      .scale = args->frame->scale);
}

GuiRect gui_pane_row_rect(const GuiPaneCfg *args)
{
    const int pixel_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANE_ROW, .scale = args->frame->scale);
    // the rows scrolled off are no more than the row, and the rows are far fewer than an int counts
    const int from_top = (int)args->row - (int)args->scroll;
    return (GuiRect){args->frame->side.x, gui_pane.rows_top(args) + (from_top * pixel_height),
                     args->frame->side.width - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1,
                                                          .scale = args->frame->scale),
                     pixel_height};
}

int gui_pane_hit(const GuiPaneCfg *args)
{
    const int top = gui_pane.rows_top(args);
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->side, .x = args->x,
                    .y = args->y) ||
        (args->y < top))
    {
        return 0;
    }
    const int pixel_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANE_ROW, .scale = args->frame->scale);
    // the pointer stands below the top, and the rows it is down are far fewer than an unsigned int counts
    const unsigned int row = args->scroll + (unsigned int)((args->y - top) / pixel_height);
    if (row >= args->count)
    {
        return 0;
    }
    *args->row_pointer = row;
    return 1;
}

int gui_pane_scrolled(const GuiPaneCfg *args)
{
    const int pixel_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANE_ROW, .scale = args->frame->scale);
    const int capacity = args->frame->side.y + args->frame->side.height - gui_pane.rows_top(args);
    const int displayed = (pixel_height > 0) ? capacity / pixel_height : 0;
    // the rows are far fewer than an int counts
    const int count = (int)args->count;
    const int most = (count > displayed) ? count - displayed : 0;
    const int required = (int)*args->scroll_pointer + args->rows;
    int clamped;
    if (required < 0)
    {
        clamped = 0;
    }
    else if (required > most)
    {
        clamped = most;
    }
    else
    {
        clamped = required;
    }
    // the scroll clamped is not negative
    if ((unsigned int)clamped == *args->scroll_pointer)
    {
        return 0;
    }
    *args->scroll_pointer = (unsigned int)clamped;
    return 1;
}

int gui_pane_baseline(const GuiPaneCfg *args)
{
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs,
                                    .face = args->face, .size = args->size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs,
                                     .face = args->face, .size = args->size);
    return args->top + (int)((((float)args->pixel_height - ascent - descent) / 2.0f) + ascent + 0.5f);
}

void gui_pane_arrow_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiColor color = args->color;
    const int half = args->side / 2;
    const GuiRect box = {args->x, args->y, args->side, args->side};
    const GuiRect met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = box, .other = args->clip);
    if ((met.width != box.width) || (met.height != box.height))
    {
        return;
    }
    if (args->open)
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x, .y0 = args->y + 1,
                   .x1 = args->x + half, .y1 = args->y + half + 1, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half,
                   .y0 = args->y + half + 1, .x1 = args->x + args->side - 1, .y1 = args->y + 1, .color = color);
    }
    else
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + 1, .y0 = args->y,
                   .x1 = args->x + half + 1, .y1 = args->y + half, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half + 1,
                   .y0 = args->y + half, .x1 = args->x + 1, .y1 = args->y + args->side - 1, .color = color);
    }
}

GuiRect gui_pane_body(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const GuiRect side = args->frame->side;
    const GuiRect none = {0, 0, 0, 0};
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = side))
    {
        return none;
    }
    const int one = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    // the pane no further than the work's inside, as its pane slides out of it
    const GuiRect inside =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){side.x, side.y, side.width - one, side.height}, .other = args->frame->inside);
    const int line_y =
        side.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_HEAD, .scale = scale);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){side.x, line_y, inside.width, one}, .other = inside),
               .color = GUI_FRAME_FADED(args->palette->line, args->frame->side_share));
    const int pane_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_PANE_HEAD, .scale = scale);
    const int pane_y = line_y + one;
    const int arrow = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANE_ARROW, .scale = scale);
    const unsigned int head_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_PANE_HEAD_SIZE, .scale = scale);
    const GuiColor head_color = GUI_FRAME_FADED(args->palette->text, args->frame->side_share);
    EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = raster,
               .x = side.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 7, .scale = scale),
               .y = pane_y + ((pane_height - arrow) / 2), .side = arrow, .open = 1, .color = head_color,
               .clip = inside);
    (void)EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs, .face = GUI_FACE_HEAD,
        .size = head_size,
        .x_f32 = (float)(side.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 19, .scale = scale)),
        .y = EMBED_CALL(gui_pane.baseline, GuiPaneCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD, .size = head_size,
                        .top = pane_y, .pixel_height = pane_height),
        .text = args->title, .length = cellul.len(&(CatenaFinitaCfg){.src = args->title, .cap = 64ULL}),
        .color = head_color, .clip = inside);
    const int rows_top = gui_pane.rows_top(args);
    const GuiRect body =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){side.x, rows_top, inside.width, side.y + side.height - rows_top}, .other = inside);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = body))
    {
        return none;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = body,
               .color = GUI_FRAME_FADED(args->palette->editor_background, args->frame->side_share));
    return body;
}

int gui_pane_guides(const GuiPaneCfg *args)
{
    const int scale = args->frame->scale;
    const int one = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    const int guide = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANE_GUIDE, .scale = scale);
    const int in = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 7, .scale = scale);
    int x = args->rect.x +
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANE_PAD_LEFT, .scale = scale);
    for (unsigned int level = 0u; level < args->depth; level += 1u)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                   .rect =
                       EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){x + in, args->rect.y, one, args->rect.height}, .other = args->clip),
                   .color = GUI_FRAME_FADED(args->palette->line, args->frame->side_share));
        x += guide;
    }
    return x;
}
