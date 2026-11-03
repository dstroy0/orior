// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// signature.c: the parameter info panel, as the controller publishes it. The label is drawn piece by piece, each
// parameter's span its own run, which lets the active and differing parameters take their colors and lines without a
// second layout
#include "signature.h"

#include "../../command_line/cli_internal.h"
#include "../../text/text.h"
#include "../frame.h"

void gui_signature_init(const GuiSignatureCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->signature, .val = 0, .bytes = sizeof(*args->signature)});
}

typedef struct
{
    const CliSignaturePanel *panel;
    unsigned int param;
    unsigned long long at;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    int x;
    int y;
    int pixel_width;
    int scale;
    GuiRect clip;
} SignatureCtx;

int gui_signature_holds(const GuiSignatureXCfg *args)
{
    return (args->signature_read->panel != NULL) &&
           EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->signature_read->rect, .x = args->x,
                      .y = args->y);
}

// whether the parameter `param` is among those the arguments differ from
EMBED_INLINE int signature_differs(const SignatureCtx *args)
{
    for (unsigned int at = 0u; at < args->panel->differing; at += 1u)
    {
        if (args->panel->differs[at] == args->param)
        {
            return 1;
        }
    }
    return 0;
}

// the parameter whose span starts at byte `at` of the label, or -1
EMBED_INLINE int signature_param_at(const SignatureCtx *args)
{
    for (unsigned int param = 0u; param < args->panel->parameters; param += 1u)
    {
        if ((args->panel->parameter[param].from == args->at) && (args->panel->parameter[param].to > args->at))
        {
            // the parameters are fewer than an int counts
            return (int)param;
        }
    }
    return -1;
}

// the label laid from x, y `pixel_width` pixels wide, wrapped by letters, each parameter's span in its color with its
// line, drawn where `raster` is given: the height taken
static int signature_label_layout(const SignatureCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    int x = args->x;
    int y = args->y;
    int scale = args->scale;
    GuiRect clip = args->clip;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_PANEL_CODE_SIZE, .scale = scale);
    const int line_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_CODE_LINE, .scale = scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_CODE, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_CODE, .size = size);
    const int baseline = (int)((((float)line_height - ascent - descent) / 2.0f) + ascent + 0.5f);
    const GuiColor type = EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = palette,
                                     .name_read = "type", .length = 4ULL);
    float pen = (float)x;
    int top = y;
    int param = -1;
    unsigned long long param_end = 0ULL;
    unsigned long long at = 0ULL;
    while (at < args->panel->length)
    {
        if (at >= param_end)
        {
            param = EMBED_CALL(signature_param_at, SignatureCtx, .panel = args->panel, .at = at);
            param_end = (param >= 0) ? args->panel->parameter[param].to : 0ULL;
        }
        unsigned long long consumed = 0ULL;
        (void)EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->panel->label, .size = args->panel->length,
                         .at = at, .length = &consumed);
        consumed = (consumed != 0ULL) ? consumed : 1ULL;
        const float letter_width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                       .size = size, .text = args->panel->label + at, .length = consumed);
        if ((pen + letter_width > (float)(x + args->pixel_width)) && (pen > (float)x))
        {
            top += line_height;
            pen = (float)x;
        }
        const int active = (param >= 0) && (param == args->panel->active);
        const int differs = (param >= 0) && EMBED_CALL(signature_differs, SignatureCtx, .panel = args->panel,
                                                       .param = (unsigned int)param);
        if (raster != NULL)
        {
            const GuiColor color = active ? type : palette->editor_text;
            const float after = EMBED_CALL(
                gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                .face = active ? GUI_FACE_CODE_BOLD : GUI_FACE_CODE, .size = size, .x_f32 = pen, .y = top + baseline,
                .text = args->panel->label + at, .length = consumed, .color = color, .clip = clip);
            if (active || differs)
            {
                const int under =
                    top + baseline + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 3, .scale = scale);
                EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                           .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                              .one = (GuiRect){(int)pen, under, (int)(after - pen + 0.5f),
                                                               EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                          .pixels = 1, .scale = scale)},
                                              .other = clip),
                           .color = differs ? palette->fail : type);
            }
            pen = after;
        }
        else
        {
            pen += letter_width;
        }
        at += consumed;
    }
    return (top - y) + line_height;
}

void gui_signature_draw(const GuiSignatureRasterCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    GuiRect anchor = args->anchor;
    GuiRect bounds = args->bounds;
    int scale = args->scale;
    const CliSignaturePanel *const panel = args->signature->panel;
    if (panel == NULL)
    {
        return;
    }
    const int pad_x =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_PAD_X, .scale = scale);
    const int pad_y =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_PAD_Y, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_GAP, .scale = scale);
    int pixel_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_WIDTH, .scale = scale);
    pixel_width = (pixel_width > bounds.width) ? bounds.width : pixel_width;
    const int inner = pixel_width - (2 * pad_x);
    const int label_height =
        EMBED_CALL(signature_label_layout, SignatureCtx, .panel = panel, .raster = NULL, .glyphs = glyphs,
                   .palette = palette, .x = 0, .y = 0, .pixel_width = inner, .scale = scale, .clip = bounds);
    const int doc_height =
        (panel->doc_length != 0ULL)
            ? gap + EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = NULL, .glyphs = glyphs,
                               .palette = palette, .text = panel->doc, .length = panel->doc_length, .x = 0, .y = 0,
                               .pixel_width = inner, .scale = scale, .clip = bounds)
            : 0;
    int pixel_height = label_height + doc_height + (2 * pad_y);
    const int most =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_HEIGHT, .scale = scale);
    pixel_height = (pixel_height > most) ? most : pixel_height;
    args->signature->rect =
        EMBED_CALL(gui_panel_width.place, GuiPanelWidthCfg, .anchor = anchor, .pixel_width = pixel_width,
                   .pixel_height = pixel_height, .back = 0, .bounds = bounds, .above = 1, .scale = scale);
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = raster, .palette = palette, .rect = args->signature->rect,
               .scale = scale);
    const GuiRect clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                    .one = (GuiRect){args->signature->rect.x + pad_x, args->signature->rect.y + pad_y,
                                                     inner, pixel_height - (2 * pad_y)},
                                    .other = bounds);
    const int laid =
        EMBED_CALL(signature_label_layout, SignatureCtx, .panel = panel, .raster = raster, .glyphs = glyphs,
                   .palette = palette, .x = args->signature->rect.x + pad_x, .y = args->signature->rect.y + pad_y,
                   .pixel_width = inner, .scale = scale, .clip = clip);
    if (panel->doc_length != 0ULL)
    {
        (void)EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = raster, .glyphs = glyphs,
                         .palette = palette, .text = panel->doc, .length = panel->doc_length,
                         .x = args->signature->rect.x + pad_x, .y = args->signature->rect.y + pad_y + laid + gap,
                         .pixel_width = inner, .scale = scale, .clip = clip);
    }
}
