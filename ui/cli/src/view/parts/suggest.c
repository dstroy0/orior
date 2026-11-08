// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// suggest.c: the completion panel, its rows as the panels over the editor rank them, read where they stand
#include "suggest.h"

#include "../../command_line/cli_internal.h"
#include "../../text/text.h"
#include "../frame.h"

void gui_suggest_init(const GuiSuggestCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->suggest, .val = 0, .bytes = sizeof(*args->suggest)});
}

typedef struct
{
    unsigned long long length;
    const GuiPalette *palette;
    const char *type;
} SuggestCtx;

int gui_suggest_holds(const GuiSuggestXCfg *args)
{
    return (args->suggest_read->panel != NULL) &&
           EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->suggest_read->rect, .x = args->x,
                      .y = args->y);
}

int gui_suggest_row_at(const GuiSuggestXCfg *args)
{
    const CliCompletionPanel *const panel = args->suggest_read->panel;
    const GuiRect list = args->suggest_read->list;
    if ((panel == NULL) || (panel->rows == 0u) || (list.height <= 0) ||
        !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = list, .x = args->x, .y = args->y))
    {
        return 0;
    }
    // the list's height is a row for each row shown, and a pixel inside the list stands on a row that is not negative
    const int row_height = list.height / (int)panel->rows;
    const int row = (row_height > 0) ? (args->y - list.y) / row_height : 0;
    if ((unsigned int)row >= panel->rows)
    {
        return 0;
    }
    *args->at = panel->first + (unsigned int)row;
    return 1;
}

// the color of a type's mark: the token colors the page gives each type
EMBED_INLINE GuiColor suggest_type_color(const SuggestCtx *args)
{
    const GuiPalette *palette = args->palette;
    const char *type = args->type;
    unsigned long long length = args->length;
    static const struct
    {
        const char *type;
        const char *token;
    } s_marks[] = {{"type", "type"},           {"class", "type"},         {"state", "type"},
                   {"function", "predefined"}, {"method", "predefined"},  {"operator", "predefined"},
                   {"field", "attribute"},     {"property", "attribute"}, {"variable", "variable"},
                   {"parameter", "variable"},  {"local", "variable"},     {"constant", "number"},
                   {"snippet", "string"},      {"module", "tag"}};
    for (unsigned int at = 0u; at < sizeof(s_marks) / sizeof(s_marks[0]); at += 1u)
    {
        const unsigned long long known = cellul.len(&(CatenaFinitaCfg){.src = s_marks[at].type, .cap = 32ULL});
        if ((known == length) &&
            (memor.cmp(&(MemoriaCfg){.src = s_marks[at].type, .other = type, .bytes = length}) == 0))
        {
            const char *const token = s_marks[at].token;
            return EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = palette, .name_read = token,
                              .length = cellul.len(&(CatenaFinitaCfg){.src = token, .cap = 32ULL}));
        }
    }
    return EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = palette, .name_read = "keyword",
                      .length = 7ULL);
}

void gui_suggest_draw(const GuiSuggestRasterCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    GuiRect anchor = args->anchor;
    GuiRect bounds = args->bounds;
    int scale = args->scale;
    const CliCompletionPanel *const panel = args->suggest->panel;
    if ((panel == NULL) || (panel->rows == 0u))
    {
        return;
    }
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_PANEL_CODE_SIZE, .scale = scale);
    const int row_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_ROW, .scale = scale);
    const int pad_x =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_PAD_X, .scale = scale);
    const int pad_y =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_PAD_Y, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_GAP, .scale = scale);
    const int mark = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_TYPE, .scale = scale);
    const unsigned int rows = panel->rows;
    // the list's width is its widest row's, between the least and the most
    float widest = 0.0f;
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        CliCompletionRow read = {"", 0ULL, "", "", 0ULL, 0u};
        (void)cli_servers_panels_completion_row(row, &read);
        const CliCompletionRow *const item = &read;
        float pixel_width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                       .size = size, .text = item->label, .length = item->label_length);
        if (item->detail_length > 0ULL)
        {
            pixel_width += (float)gap + EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg,
                                                   .glyphs = glyphs, .face = GUI_FACE_CODE, .size = size,
                                                   .text = item->detail, .length = item->detail_length);
        }
        widest = (pixel_width > widest) ? pixel_width : widest;
    }
    int list_width = (int)(widest + 0.5f) + (2 * pad_x) + mark + gap;
    const int min = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_MIN, .scale = scale);
    const int most = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_MAX, .scale = scale);
    if (list_width < min)
    {
        list_width = min;
    }
    else if (list_width > most)
    {
        list_width = most;
    }
    // the rows are few: their height is far inside what an int represents
    const int list_height = ((int)rows * row_height) + (2 * pad_y);
    // the row chosen is always among the rows shown
    CliCompletionRow chosen_row = {"", 0ULL, "", "", 0ULL, 0u};
    (void)cli_servers_panels_completion_row(panel->chosen - panel->first, &chosen_row);
    const CliCompletionRow *const chosen = &chosen_row;
    const int detailed = chosen->detail_length > 0ULL;
    const int detail_width =
        detailed ? EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_DETAIL, .scale = scale)
                 : 0;
    int pixel_width = list_width + detail_width;
    pixel_width = (pixel_width > bounds.width) ? bounds.width : pixel_width;
    args->suggest->rect = EMBED_CALL(
        gui_panel_width.place, GuiPanelWidthCfg, .anchor = anchor, .pixel_width = pixel_width,
        .pixel_height = list_height,
        .back = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_BACK, .scale = scale),
        .bounds = bounds, .above = 0, .scale = scale);
    args->suggest->list =
        (GuiRect){args->suggest->rect.x, args->suggest->rect.y + pad_y, list_width, (int)rows * row_height};
    args->suggest->detail = detailed ? (GuiRect){args->suggest->rect.x + list_width, args->suggest->rect.y,
                                                 pixel_width - list_width, list_height}
                                     : (GuiRect){0, 0, 0, 0};
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = raster, .palette = palette, .rect = args->suggest->rect,
               .scale = scale);
    const GuiRect clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->suggest->rect, .other = bounds);
    const int baseline = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_CODE, .size = size, .top = 0, .pixel_height = row_height);
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        const unsigned int at = panel->first + row;
        CliCompletionRow read = {"", 0ULL, "", "", 0ULL, 0u};
        (void)cli_servers_panels_completion_row(row, &read);
        const CliCompletionRow *const item = &read;
        const GuiRect row_rect = {args->suggest->list.x, args->suggest->list.y + ((int)row * row_height), list_width,
                                  row_height};
        if (at == panel->chosen)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = row_rect, .other = clip),
                       .color = palette->selection);
        }
        const GuiRect mark_rect = {row_rect.x + pad_x, row_rect.y + ((row_height - mark) / 2), mark, mark};
        EMBED_CALL(
            gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
            .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = mark_rect, .other = clip),
            .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 2, .scale = scale),
            .color = EMBED_CALL(suggest_type_color, SuggestCtx, .palette = palette, .type = item->type,
                                .length = cellul.len(&(CatenaFinitaCfg){.src = item->type, .cap = CLI_POOL_BYTES})));
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){row_rect.x, row_rect.y, list_width - pad_x, row_height}, .other = clip);
        float x = (float)(row_rect.x + pad_x + mark + gap);
        const char *const label = item->label;
        const unsigned long long typed = item->marked;
        if (typed != 0ULL)
        {
            x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                           .face = GUI_FACE_CODE_BOLD, .size = size, .x_f32 = x, .y = row_rect.y + baseline,
                           .text = label, .length = typed, .color = palette->signal, .clip = text_clip);
            x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                           .face = GUI_FACE_CODE, .size = size, .x_f32 = x, .y = row_rect.y + baseline,
                           .text = label + typed, .length = item->label_length - typed, .color = palette->text,
                           .clip = text_clip);
        }
        else
        {
            x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                           .face = GUI_FACE_CODE, .size = size, .x_f32 = x, .y = row_rect.y + baseline, .text = label,
                           .length = item->label_length, .color = palette->text, .clip = text_clip);
        }
        if (item->detail_length > 0ULL)
        {
            const float detail_width_text =
                EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                           .face = GUI_FACE_CODE, .size = size, .text = item->detail, .length = item->detail_length);
            float detail_x = (float)(row_rect.x + list_width - pad_x) - detail_width_text;
            detail_x = (detail_x < x + (float)gap) ? x + (float)gap : detail_x;
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_CODE, .size = size, .x_f32 = detail_x, .y = row_rect.y + baseline,
                             .text = item->detail, .length = item->detail_length, .color = palette->text_3,
                             .clip = text_clip);
        }
    }
    if (detailed)
    {
        const int line = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){args->suggest->detail.x, args->suggest->detail.y + line, line,
                                                       args->suggest->detail.height - (2 * line)},
                                      .other = clip),
                   .color = palette->widget_line);
        const int detail_pad_x =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_DETAIL_PAD_X, .scale = scale);
        const int detail_pad_y =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_DETAIL_PAD_Y, .scale = scale);
        // the detail is laid as code, the panel's inline code, inside one pair of backticks
        char fenced[GUI_PANEL_RUN + 2u];
        VerbaLinea text = {fenced, sizeof(fenced), 0ULL};
        mmgr_line_ch(&text, '`');
        mmgr_line_put_n(&text, chosen->detail,
                        (chosen->detail_length < GUI_PANEL_RUN) ? chosen->detail_length : GUI_PANEL_RUN);
        mmgr_line_ch(&text, '`');
        const GuiRect detail_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){args->suggest->detail.x + detail_pad_x, args->suggest->detail.y + detail_pad_y,
                                        args->suggest->detail.width - (2 * detail_pad_x),
                                        args->suggest->detail.height - (2 * detail_pad_y)},
                       .other = clip);
        (void)EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = raster, .glyphs = glyphs,
                         .palette = palette, .text = fenced, .length = mmgr_line_ok(&text) ? text.at : 0ULL,
                         .x = args->suggest->detail.x + detail_pad_x, .y = args->suggest->detail.y + detail_pad_y,
                         .pixel_width = args->suggest->detail.width - (2 * detail_pad_x), .scale = scale,
                         .clip = detail_clip);
    }
}
