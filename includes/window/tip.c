// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tip.c: the tooltip under the button the pointer rests on: a panel box with the button's label in the interface face
// and its keys after it in the quieter text, shown as the rest on the button ends
#include "tip.h"

// the most bytes a tip holds, a label with its keys
#define GUI_TIP_LABEL_MAX 256u
#include "frame.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "top_menu.h"
#include "panel.h"

typedef struct
{
    const GuiRect *one;
    const GuiRect *other;
} TipCtx;

// whether two rectangles are the same
EMBED_INLINE int tip_rect_same(const TipCtx *args)
{
    return (args->one->x == args->other->x) && (args->one->y == args->other->y) &&
           (args->one->width == args->other->width) && (args->one->height == args->other->height);
}

void gui_tip_init(const GuiTipCfg *args)
{
    args->tip->label = NULL;
    args->tip->keys = NULL;
    args->tip->anchor = (GuiRect){0, 0, 0, 0};
    args->tip->due_ms = 0ULL;
    args->tip->displayed = 0;
    args->tip->rect = (GuiRect){0, 0, 0, 0};
}

int gui_tip_point(const GuiTipPointCfg *args)
{
    GuiTip *const tip = args->tip;
    const int same_button = (tip->label != NULL) && (args->label != NULL) &&
                            EMBED_CALL(tip_rect_same, TipCtx, .one = &tip->anchor, .other = &args->anchor);
    if (same_button)
    {
        return 0;
    }
    const int went = tip->displayed;
    tip->label = args->label;
    tip->keys = args->keys;
    tip->anchor = args->anchor;
    tip->beside = args->beside;
    tip->displayed = 0;
    tip->rect = (GuiRect){0, 0, 0, 0};
    tip->due_ms = (args->label != NULL) ? args->time_ms + GUI_TIP_REST_MS : 0ULL;
    return went;
}

int gui_tip_tick(const GuiTipTimeCfg *args)
{
    GuiTip *const tip = args->tip;
    if ((tip->label == NULL) || tip->displayed || (args->time_ms < tip->due_ms))
    {
        return 0;
    }
    tip->displayed = 1;
    return 1;
}

unsigned long long gui_tip_deadline_ms(const GuiTipReadCfg *args)
{
    const GuiTip *const tip = args->tip_read;
    return ((tip->label != NULL) && !tip->displayed) ? tip->due_ms : 0ULL;
}

void gui_tip_draw(const GuiTipRasterCfg *args)
{
    GuiTip *const tip = args->tip;
    tip->rect = (GuiRect){0, 0, 0, 0};
    if ((tip->label == NULL) || !tip->displayed)
    {
        return;
    }
    const int scale = args->scale;
    const unsigned int size =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HEAD_SIZE, .scale = scale);
    const int pad_x = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TIP_PAD_X, .scale = scale);
    const int pad_y = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TIP_PAD_Y, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TIP_GAP, .scale = scale);
    const int off = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TIP_OFF, .scale = scale);
    const unsigned long long label_length = cellul.len(&(CatenaFinitaCfg){.src = tip->label, .cap = GUI_TIP_LABEL_MAX});
    const unsigned long long keys_length =
        (tip->keys != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = tip->keys, .cap = GUI_TIP_LABEL_MAX}) : 0ULL;
    const float label_width =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs, .face = GUI_FACE_HEAD,
                   .size = size, .text = tip->label, .length = label_length);
    const float keys_width = (keys_length != 0ULL)
                                 ? EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg,
                                              .glyphs = args->glyphs, .face = GUI_FACE_HEAD, .size = size,
                                              .text = tip->keys, .length = keys_length)
                                 : 0.0f;
    // the widths are pixels, a few hundred at most, so the sum rounds up into an int
    const int text_width = (int)(label_width + ((keys_length != 0ULL) ? ((float)gap + keys_width) : 0.0f) + 0.5f);
    const int line = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_TEXT_LINE, .scale = scale);
    const int pixel_width = text_width + (2 * pad_x);
    const int pixel_height = line + (2 * pad_y);
    if (tip->beside)
    {
        // the box at the button's right, off it by its own distance, centred on the button's row, and held inside the
        // bounds: at the button's left where the right has no room
        int x = tip->anchor.x + tip->anchor.width + off;
        if (x + pixel_width > args->bounds.x + args->bounds.width)
        {
            x = tip->anchor.x - off - pixel_width;
        }
        int y = tip->anchor.y + ((tip->anchor.height - pixel_height) / 2);
        const int lowest = args->bounds.y + args->bounds.height - pixel_height;
        y = (y > lowest) ? lowest : y;
        y = (y < args->bounds.y) ? args->bounds.y : y;
        x = (x < args->bounds.x) ? args->bounds.x : x;
        tip->rect = (GuiRect){x, y, pixel_width, pixel_height};
    }
    else
    {
        // the box stands off the button by its own distance, so the anchor it is placed under grows by that less the
        // panel's own offset, and centres on the button by backing up half the width the box has over it
        const GuiRect anchor = {tip->anchor.x, tip->anchor.y - (off - GUI_PANEL_OFF), tip->anchor.width,
                                tip->anchor.height + (2 * (off - GUI_PANEL_OFF))};
        tip->rect = EMBED_CALL(gui_panel_width.place, GuiPanelWidthCfg, .anchor = anchor, .pixel_width = pixel_width,
                               .pixel_height = pixel_height, .back = (pixel_width - tip->anchor.width) / 2,
                               .bounds = args->bounds, .above = 0, .scale = scale);
    }
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = args->raster, .palette = args->palette, .rect = tip->rect,
               .scale = scale);
    const int baseline =
        EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs, .face = GUI_FACE_HEAD,
                   .size = size, .top = tip->rect.y + pad_y, .pixel_height = line);
    const GuiRect clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = tip->rect, .other = args->bounds);
    const float after = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                                   .glyphs = args->glyphs, .face = GUI_FACE_HEAD, .size = size,
                                   .x_f32 = (float)(tip->rect.x + pad_x), .y = baseline, .text = tip->label,
                                   .length = label_length, .color = args->palette->text, .clip = clip);
    if (keys_length != 0ULL)
    {
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                         .glyphs = args->glyphs, .face = GUI_FACE_HEAD, .size = size, .x_f32 = after + (float)gap,
                         .y = baseline, .text = tip->keys, .length = keys_length, .color = args->palette->text_2,
                         .clip = clip);
    }
}
