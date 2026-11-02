// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs.c: the tabs row. Each tab is as wide as its name, its padding and its close mark, and the tabs stand in a row
// from the row's left; tabs past the row's right edge are cut off at it
#include "tabs.h"

#include "../../../cli/src/text/text.h"

#include <math.h>

// the close mark's text
static const char s_close[] = "\xc3\x97";

const char *gui_tab_name(const GuiTabsCfg *args)
{
    *args->length = cellul.len(&(CatenaFinitaCfg){.src = args->tab->name, .cap = CLI_CELLS_POOL});
    return args->tab->name;
}

typedef struct
{
    GuiGlyphs *glyphs;
    const CliTabRow *tab;
    unsigned int size;
    int scale;
} TabsCtx;
// a tab's width: its padding each side, its name, the gap and the close mark with its padding
EMBED_INLINE int tabs_width(const TabsCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    const CliTabRow *tab = args->tab;
    unsigned int size = args->size;
    int scale = args->scale;
    unsigned long long length = 0ull;
    const char *const name = EMBED_CALL(gui_tabs.tab_name, GuiTabsCfg, .tab = tab, .length = &length);
    const float text = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                  .face = GUI_FACE_CODE, .size = size, .text = name, .length = length);
    const float close =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                   .size = size, .text = s_close, .length = sizeof(s_close) - 1u);
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_PAD_LEFT, .scale = scale) +
           (int)roundf(text) +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_GAP, .scale = scale) +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_CLOSE_PAD, .scale = scale) +
           (int)roundf(close) +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_CLOSE_PAD, .scale = scale) +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_PAD_RIGHT, .scale = scale);
}

GuiRect gui_tab_rect(const GuiTabsGlyphsCfg *args)
{
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_TAB_SIZE, .scale = args->frame->scale);
    int x = args->frame->tabs.x;
    for (unsigned int each = 0u; (each < args->at) && (each < args->count); each += 1u)
    {
        x += EMBED_CALL(tabs_width, TabsCtx, .glyphs = args->glyphs, .tab = &args->tab[each], .size = size,
                        .scale = args->frame->scale);
    }
    const int pixel_width = (args->at < args->count)
                                ? EMBED_CALL(tabs_width, TabsCtx, .glyphs = args->glyphs, .tab = &args->tab[args->at],
                                             .size = size, .scale = args->frame->scale)
                                : 0;
    return (GuiRect){x, args->frame->tabs.y, pixel_width,
                     args->frame->tabs.height - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1,
                                                           .scale = args->frame->scale)};
}

GuiRect gui_tab_close_rect(const GuiTabsGlyphsCfg *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    const GuiRect rect = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = glyphs, .frame = args->frame,
                                    .tab = args->tab, .count = args->count, .at = args->at);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_TAB_SIZE, .scale = args->frame->scale);
    const float close =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                   .size = size, .text = s_close, .length = sizeof(s_close) - 1u);
    const int pixel_width = (2 * EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_CLOSE_PAD,
                                            .scale = args->frame->scale)) +
                            (int)roundf(close);
    return (GuiRect){rect.x + rect.width -
                         EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_PAD_RIGHT,
                                    .scale = args->frame->scale) -
                         pixel_width,
                     rect.y, pixel_width, rect.height};
}

int gui_tabs_hit(const GuiTabsGlyphsFrameCountCfg *args)
{
    unsigned int *at = args->at_pointer;
    int *closing = args->closing;
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->tabs, .x = args->x,
                    .y = args->y))
    {
        return 0;
    }
    for (unsigned int each = 0u; each < args->count; each += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = args->glyphs,
                                          .frame = args->frame, .tab = args->tab, .count = args->count, .at = each),
                       .x = args->x, .y = args->y))
        {
            *at = each;
            *closing =
                EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                           .rect = EMBED_CALL(gui_tabs_glyphs.tab_close_rect, GuiTabsGlyphsCfg, .glyphs = args->glyphs,
                                              .frame = args->frame, .tab = args->tab, .count = args->count, .at = each),
                           .x = args->x, .y = args->y);
            return 1;
        }
    }
    return 0;
}

void gui_tabs_draw(const GuiTabsGlyphsFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const int one = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){args->frame->tabs.x, args->frame->tabs.y + args->frame->tabs.height - one,
                                 args->frame->tabs.width, one},
               .color = args->palette->line);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_TAB_SIZE, .scale = scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_CODE, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_CODE, .size = size);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const GuiRect rect = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = glyphs,
                                        .frame = args->frame, .tab = args->tab, .count = args->count, .at = at);
        const GuiRect met =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = args->frame->tabs);
        if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = met))
        {
            break;
        }
        // the tabs are fewer than an int counts
        const int displayed = (int)at == args->active;
        if (displayed)
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = met,
                       .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                          .pixels = GUI_TAB_RADIUS, .scale = scale),
                       .color = args->palette->tab_background);
            const int line =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_LINE, .scale = scale);
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){rect.x, rect.y, rect.width, line}, .other = args->frame->tabs),
                       .color = args->palette->tab_line);
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){rect.x, rect.y, line, rect.height}, .other = args->frame->tabs),
                       .color = args->palette->tab_line);
        }
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){rect.x + rect.width - one, rect.y, one, rect.height},
                                      .other = args->frame->tabs),
                   .color = args->palette->line);
        const int baseline = rect.y + (int)((((float)rect.height - ascent - descent) / 2.0f) + ascent + 0.5f);
        unsigned long long length = 0ull;
        const char *const name = EMBED_CALL(gui_tabs.tab_name, GuiTabsCfg, .tab = &args->tab[at], .length = &length);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = size,
                         .x_f32 = (float)(rect.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                              .pixels = GUI_TAB_PAD_LEFT, .scale = scale)),
                         .y = baseline, .text = name, .length = length,
                         .color = displayed ? args->palette->text : args->palette->text_2, .clip = met);
        const GuiRect close = EMBED_CALL(gui_tabs_glyphs.tab_close_rect, GuiTabsGlyphsCfg, .glyphs = glyphs,
                                         .frame = args->frame, .tab = args->tab, .count = args->count, .at = at);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = size,
                         .x_f32 = (float)(close.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                               .pixels = GUI_TAB_CLOSE_PAD, .scale = scale)),
                         .y = baseline, .text = s_close, .length = sizeof(s_close) - 1u, .color = args->palette->text_3,
                         .clip = met);
    }
}
