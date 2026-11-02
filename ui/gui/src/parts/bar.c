// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bar.c: the bar across the top, on the pane the frame's base lays: the tree's name in the interface face at its right
// before the controls, as the page's bar ends with it, and the controls at its right end; the menu bar's titles are
// drawn over it by the menu bar part
#include "chrome.h"

#include <math.h>

// the gap in the page's pixels between the tree's name and the controls, 0.7rem, and the name's most width, 16rem
#define GUI_BAR_GAP 11
#define GUI_BAR_NAME_WIDE 256

void gui_bar_draw(const GuiChromeRasterCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    // the bar's pane is the slat its edge shows, whole outside the claim, laid by the frame's base
    const GuiRect pane = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->bar_slat,
                                    .other = args->frame->bar);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = args->frame->scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size);
    const int baseline =
        args->frame->bar.y + (int)((((float)args->frame->bar.height - ascent - descent) / 2.0f) + ascent + 0.5f);
    unsigned long long length = 0ull;
    length = cellul.len(&(CatenaFinitaCfg){.src = args->title, .cap = CLI_CELLS_POOL});
    const int most =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BAR_NAME_WIDE, .scale = args->frame->scale);
    const float measured = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                      .face = GUI_FACE_HEAD, .size = size, .text = args->title, .length = length);
    const int wide = (measured > (float)most) ? most : (int)roundf(measured);
    const int right = args->frame->controls.x - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                           .pixels = GUI_BAR_GAP, .scale = args->frame->scale);
    const GuiRect clip = {right - wide, args->frame->bar.y, wide, args->frame->bar.height};
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(right - wide), .y = baseline,
                     .text = args->title, .length = length, .color = args->palette->bar_text,
                     .clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = clip, .other = pane));
    EMBED_CALL(gui_chrome_raster.controls_draw, GuiChromeRasterCfg, .raster = raster, .palette = args->palette,
               .frame = args->frame, .table = args->table, .shadow = args->shadow);
}
