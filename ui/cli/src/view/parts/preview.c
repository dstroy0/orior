// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preview.c: the preview's items drawn as the CLI lays them, each type in the raster's own strokes: a run of text in
// its face and size, struck through or underlined by its flags; a box filled in its role and bordered in its stroke; a
// rule; a line segment; and a block marked by the cursor's line outlined in the signal. Every color is a role the
// palette answers; every place a page pixel the frame's scale lays
#include "preview.h"

typedef struct
{
    const GuiPreviewCfg *preview;
    const CliPreviewItem *item;
    const char *text;
    GuiRect place;
    GuiRect clip;
    GuiColor color;
    unsigned int role;
    int pixels;
    int top;
} PreviewCtx;

// `pixels` of the page at the frame's scale
EMBED_INLINE int preview_scaled(const PreviewCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels,
                      .scale = args->preview->frame->scale);
}

// the palette's color of the role `role`
EMBED_INLINE GuiColor preview_role_color(const PreviewCtx *args)
{
    const GuiPalette *const palette = args->preview->palette;
    switch (args->role)
    {
    case CLI_PREVIEW_ROLE_TEXT:
        return palette->text;
    case CLI_PREVIEW_ROLE_QUIET:
        return palette->text_2;
    case CLI_PREVIEW_ROLE_FAINT:
        return palette->text_3;
    case CLI_PREVIEW_ROLE_SIGNAL:
    case CLI_PREVIEW_ROLE_HERE:
        return palette->signal;
    case CLI_PREVIEW_ROLE_LINE:
        return palette->line;
    case CLI_PREVIEW_ROLE_CODE_GROUND:
        return palette->well;
    case CLI_PREVIEW_ROLE_LINK:
        return palette->link;
    default:
        return (args->role >= CLI_PREVIEW_ROLE_SLICE) ? palette->mark[(args->role - CLI_PREVIEW_ROLE_SLICE) % GUI_MARKS]
                                                      : palette->text;
    }
}

// `place` filled in `color`, no further than `clip`
EMBED_INLINE void preview_fill(const PreviewCtx *args)
{
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->preview->raster,
               .rect =
                   EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place, .other = args->clip),
               .color = args->color);
}

// the border of `place`, a pixel at the scale wide, in `color`, no further than `clip`
EMBED_INLINE void preview_border(const PreviewCtx *args)
{
    const GuiRect place = args->place;
    const int one = EMBED_CALL(preview_scaled, PreviewCtx, .preview = args->preview, .pixels = 1);
    const GuiRect edges[4] = {{place.x, place.y, place.width, one},
                              {place.x, place.y + place.height - one, place.width, one},
                              {place.x, place.y, one, place.height},
                              {place.x + place.width - one, place.y, one, place.height}};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        EMBED_CALL(preview_fill, PreviewCtx, .preview = args->preview, .place = edges[at], .clip = args->clip,
                   .color = args->color);
    }
}

// a run of text drawn in its face and size on its line, its strike through the line's middle and its underline under
// the baseline
EMBED_INLINE void preview_text_draw(const PreviewCtx *args)
{
    const GuiPreviewCfg *const preview = args->preview;
    const CliPreviewItem *const item = args->item;
    const int bold = (item->flags & CLI_PREVIEW_BOLD) != 0u;
    const int italic = (item->flags & CLI_PREVIEW_ITALIC) != 0u;
    unsigned int face = GUI_FACE_HEAD;
    if (item->face == CLI_PREVIEW_FACE_CODE)
    {
        face = bold ? GUI_FACE_CODE_BOLD : (italic ? GUI_FACE_CODE_ITALIC : GUI_FACE_CODE);
    }
    else
    {
        face = bold ? GUI_FACE_HEAD_BOLD : (italic ? GUI_FACE_TEXT : GUI_FACE_HEAD);
    }
    // a text's size in pixels is a few dozen
    const unsigned int size =
        (unsigned int)EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview, .pixels = (int)item->size);
    const GuiRect place = args->place;
    const int baseline = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = preview->glyphs,
                                    .face = face, .size = size, .top = place.y, .pixel_height = place.height);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = preview->raster,
                     .glyphs = preview->glyphs, .face = face, .size = size, .x_f32 = (float)place.x, .y = baseline,
                     .text = args->text, .length = item->length, .color = args->color, .clip = args->clip);
    const int one = EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview, .pixels = 1);
    if ((item->flags & CLI_PREVIEW_STRIKE) != 0u)
    {
        EMBED_CALL(preview_fill, PreviewCtx, .preview = preview,
                   .place = (GuiRect){place.x, place.y + (place.height / 2), place.width, one}, .clip = args->clip,
                   .color = args->color);
    }
    if ((item->flags & CLI_PREVIEW_UNDERLINE) != 0u)
    {
        EMBED_CALL(preview_fill, PreviewCtx, .preview = preview,
                   .place = (GuiRect){place.x, baseline + (2 * one), place.width, one}, .clip = args->clip,
                   .color = args->color);
    }
}

// the item drawn at `place` as its type, no further than `clip`
EMBED_INLINE void preview_item_draw(const PreviewCtx *args)
{
    const GuiPreviewCfg *const preview = args->preview;
    const CliPreviewItem *const item = args->item;
    const GuiColor color = EMBED_CALL(preview_role_color, PreviewCtx, .preview = preview, .role = item->role);
    switch (item->type)
    {
    case CLI_PREVIEW_ITEM_TEXT:
        EMBED_CALL(preview_text_draw, PreviewCtx, .preview = preview, .item = item, .text = args->text,
                   .place = args->place, .clip = args->clip, .color = color);
        return;
    case CLI_PREVIEW_ITEM_BOX:
        if (item->role != CLI_PREVIEW_ROLE_NONE)
        {
            // a radius in pixels is a few
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = preview->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place,
                                          .other = args->clip),
                       .radius = (unsigned int)EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview,
                                                          .pixels = (int)item->radius),
                       .color = color);
        }
        if (item->stroke != CLI_PREVIEW_ROLE_NONE)
        {
            EMBED_CALL(preview_border, PreviewCtx, .preview = preview, .place = args->place, .clip = args->clip,
                       .color = EMBED_CALL(preview_role_color, PreviewCtx, .preview = preview, .role = item->stroke));
        }
        return;
    case CLI_PREVIEW_ITEM_RULE:
        EMBED_CALL(preview_fill, PreviewCtx, .preview = preview, .place = args->place, .clip = args->clip,
                   .color = color);
        return;
    case CLI_PREVIEW_ITEM_SEGMENT: {
        const GuiRect frame = preview->frame->preview;
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = preview->raster, .x0 = args->place.x,
                   .y0 = args->place.y,
                   .x1 = frame.x +
                         EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview, .pixels = item->x + item->width),
                   .y1 = frame.y + EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview,
                                              .pixels = item->y + item->height - args->top),
                   .color = color, .clip = args->clip);
        return;
    }
    default:
        if ((item->flags & CLI_PREVIEW_HERE) != 0u)
        {
            const int out = EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview, .pixels = GUI_PREVIEW_MARK_OUT);
            const GuiRect place = args->place;
            EMBED_CALL(preview_border, PreviewCtx, .preview = preview,
                       .place =
                           (GuiRect){place.x - out, place.y - out, place.width + (2 * out), place.height + (2 * out)},
                       .clip = args->clip, .color = color);
        }
        return;
    }
}

// the scroll thumb at the rectangle's right, its height the share of the items the rectangle shows and its top the
// share of the way the top has come, where the items pass the rectangle
EMBED_INLINE void preview_thumb_draw(const PreviewCtx *args)
{
    const GuiPreviewCfg *const preview = args->preview;
    const GuiRect rect = preview->frame->preview;
    const int extent = EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview,
                                  .pixels = EMBED_CALL(cli_preview_item.extent, CliPreviewItemCfg, .at = 0u));
    if (extent <= rect.height)
    {
        return;
    }
    const int least = EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview, .pixels = GUI_PREVIEW_THUMB_LEAST);
    // a share of the rectangle's height is no taller than it, an int
    const int shown = (int)(((long long)rect.height * (long long)rect.height) / (long long)extent);
    const int height = (shown > least) ? shown : least;
    const int top = EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview, .pixels = args->top);
    const int travel = rect.height - height;
    // the top has come no further than the items less the rectangle, so its share of the travel is within it
    const int y = rect.y + (int)(((long long)travel * (long long)top) / (long long)(extent - rect.height));
    const int width = EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview, .pixels = GUI_PREVIEW_THUMB);
    const int gap = EMBED_CALL(preview_scaled, PreviewCtx, .preview = preview, .pixels = GUI_PREVIEW_THUMB_GAP);
    EMBED_CALL(preview_fill, PreviewCtx, .preview = preview,
               .place = (GuiRect){rect.x + rect.width - width - gap, y, width, height}, .clip = rect,
               .color = preview->palette->map_slider);
}

void gui_preview_draw(const GuiPreviewCfg *args)
{
    const GuiRect rect = args->frame->preview;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = rect))
    {
        return;
    }
    EMBED_CALL(preview_fill, PreviewCtx, .preview = args, .place = rect, .clip = rect,
               .color = args->palette->editor_background);
    const int top = EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0);
    const unsigned int count = EMBED_CALL(cli_preview_item.count, CliPreviewItemCfg, .at = 0u);
    const int bottom = rect.y + rect.height;
    for (unsigned int at = EMBED_CALL(cli_preview_item.first, CliPreviewItemCfg, .top_px = top); at < count; at += 1u)
    {
        const char *text = NULL;
        const CliPreviewItem *const item =
            EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = at, .text_out = &text);
        const int x = rect.x + EMBED_CALL(preview_scaled, PreviewCtx, .preview = args, .pixels = item->x);
        const int y = rect.y + EMBED_CALL(preview_scaled, PreviewCtx, .preview = args, .pixels = item->y - top);
        const GuiRect place = {
            x, y,
            (rect.x + EMBED_CALL(preview_scaled, PreviewCtx, .preview = args, .pixels = item->x + item->width)) - x,
            (rect.y + EMBED_CALL(preview_scaled, PreviewCtx, .preview = args, .pixels = item->y + item->height - top)) -
                y};
        if ((item->type == CLI_PREVIEW_ITEM_BLOCK) && (place.y >= bottom))
        {
            break;
        }
        EMBED_CALL(preview_item_draw, PreviewCtx, .preview = args, .item = item, .text = text, .place = place,
                   .clip = rect, .top = top);
    }
    EMBED_CALL(preview_thumb_draw, PreviewCtx, .preview = args, .top = top);
}

int gui_preview_wheel(const GuiPreviewCfg *args)
{
    return EMBED_CALL(cli_preview_scroll.by, CliPreviewScrollCfg, .delta_px = args->rows * GUI_PREVIEW_ROW);
}

unsigned int gui_preview_press(const GuiPreviewCfg *args)
{
    const GuiRect rect = args->frame->preview;
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = rect, .x = args->x, .y = args->y))
    {
        return CLI_PREVIEW_OPEN_NONE;
    }
    const int scale = (args->frame->scale > 0) ? args->frame->scale : GUI_SCALE_ONE;
    return EMBED_CALL(cli_preview_mark.press, CliPreviewMarkCfg, .x = ((args->x - rect.x) * GUI_SCALE_ONE) / scale,
                      .y = ((args->y - rect.y) * GUI_SCALE_ONE) / scale, .open_out = args->open_out);
}
