// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// quick.c: the quick open's box, its field and its rows as the quick open lists them, read where they stand
#include "quick.h"

#include "../../command_line/cli_internal.h"
#include "../../command_line/commands.h"
#include "../../editor/files.h"
#include "panel.h"

typedef struct
{
    const GuiQuickRasterCfg *drawing;
    int pixels;
    unsigned int face;
    unsigned int size;
    float x_f32;
    int y;
    const char *text;
    unsigned long long length;
    const unsigned int *hits;
    unsigned int hit_count;
    GuiColor color;
    GuiColor mark;
    GuiRect clip;
} QuickCtx;

void gui_quick_init(const GuiQuickCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->quick, .val = 0, .bytes = sizeof(*args->quick)});
}

// `pixels` of the page at the frame's scale
EMBED_INLINE int quick_scaled(const QuickCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels,
                      .scale = args->drawing->frame->scale);
}

// `length` bytes of `text` drawn from `x_f32` on the baseline `y` in `face` at `size`, the letters at `hits`,
// `hit_count` of them counted in letters, in the bold face in `mark` and the rest in `color`, each cut to `clip`: the x
// after it
EMBED_INLINE float quick_marked_draw(const QuickCtx *args)
{
    const GuiQuickRasterCfg *const drawing = args->drawing;
    float x = args->x_f32;
    unsigned int hit = 0u;
    unsigned int letter = 0u;
    unsigned long long run = 0ULL;
    int run_marked = 0;
    for (unsigned long long at = 0ULL; at <= args->length; at += 1ULL)
    {
        const int lead = (at == args->length) || ((((unsigned char)args->text[at]) & 0xC0u) != 0x80u);
        if (!lead)
        {
            continue;
        }
        int marked = 0;
        if (at < args->length)
        {
            // the hits stand in order, one letter after another
            while ((hit < args->hit_count) && (args->hits[hit] < letter))
            {
                hit += 1u;
            }
            marked = (hit < args->hit_count) && (args->hits[hit] == letter);
            letter += 1u;
        }
        if ((at == args->length) || ((at != 0ULL) && (marked != run_marked)))
        {
            x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = drawing->raster,
                           .glyphs = drawing->glyphs, .face = run_marked ? GUI_FACE_HEAD_BOLD : args->face,
                           .size = args->size, .x_f32 = x, .y = args->y, .text = args->text + run, .length = at - run,
                           .color = run_marked ? args->mark : args->color, .clip = args->clip);
            run = at;
        }
        run_marked = (at == 0ULL) ? marked : ((marked != run_marked) ? marked : run_marked);
    }
    return x;
}

// the box laid in the frame for the rows listed: its rectangle, its field and its list, the rows scrolled so the chosen
// one shows
EMBED_INLINE void quick_layout(const QuickCtx *args)
{
    const GuiQuickRasterCfg *const drawing = args->drawing;
    GuiQuick *const quick = drawing->quick;
    const GuiFrame *const frame = drawing->frame;
    // the frame's width and height are far inside what an int represents
    const int window_width = (int)frame->width;
    const int window_height = (int)frame->height;
    int width = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_WIDTH);
    const int narrow = (window_width * GUI_QUICK_WIDTH_PERCENT) / 100;
    width = (width > narrow) ? narrow : width;
    const int pad = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_PAD);
    const int top = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_BAR_HEIGHT);
    const int x = (window_width - width) / 2;
    quick->field = (GuiRect){x + pad, top + pad, width - (2 * pad),
                             EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_FIELD)};
    const unsigned int rows = EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u);
    const int row_height = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_ROW);
    int list_most = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_LIST_MAX);
    const int short_window = (window_height * GUI_QUICK_LIST_PERCENT) / 100;
    list_most = (list_most > short_window) ? short_window : list_most;
    // the rows shown fill the list's height, one at the least; the rows are few
    unsigned int shown = (row_height > 0) ? (unsigned int)(list_most / row_height) : 1u;
    shown = (shown == 0u) ? 1u : shown;
    shown = (shown > rows) ? rows : shown;
    quick->shown = shown;
    const unsigned int chosen = EMBED_CALL(cli_quick.chosen, CliQuickCfg, .at = 0u);
    quick->first = (quick->first + shown > rows) ? ((rows > shown) ? rows - shown : 0u) : quick->first;
    quick->first = (chosen < quick->first) ? chosen : quick->first;
    quick->first = (chosen >= quick->first + shown) ? chosen + 1u - shown : quick->first;
    const int gap =
        (rows != 0u) ? EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_LIST_GAP) : 0;
    quick->list = (GuiRect){quick->field.x, quick->field.y + quick->field.height + gap, quick->field.width,
                            (int)shown * row_height};
    quick->rect = (GuiRect){x, top, width, pad + quick->field.height + gap + quick->list.height + pad};
}

// the field drawn: a box in the signal's line on the well's ground, what is typed in it, the placeholder quieter while
// nothing is, and the caret after the text
EMBED_INLINE void quick_field_draw(const QuickCtx *args)
{
    const GuiQuickRasterCfg *const drawing = args->drawing;
    const GuiPalette *const palette = drawing->palette;
    const GuiRect field = drawing->quick->field;
    const int one = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = 1);
    const unsigned int radius =
        (unsigned int)EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_FIELD_RADIUS);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = drawing->raster, .rect = field,
               .radius = radius, .color = palette->signal);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = drawing->raster,
               .rect = (GuiRect){field.x + one, field.y + one, field.width - (2 * one), field.height - (2 * one)},
               .radius = (radius > (unsigned int)one) ? radius - (unsigned int)one : 0u, .color = palette->well);
    const int field_pad = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_FIELD_PAD);
    const unsigned int size =
        (unsigned int)EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = (int)GUI_QUICK_FIELD_SIZE);
    const GuiRect clip = {field.x + one, field.y, field.width - (2 * one), field.height};
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_quick.text, CliQuickCfg, .length_out = &length);
    const int empty = length == 0ULL;
    const float after = EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = drawing->raster, .glyphs = drawing->glyphs,
        .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(field.x + field_pad),
        .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = drawing->glyphs,
                        .face = GUI_FACE_HEAD, .size = size, .top = field.y, .pixel_height = field.height),
        .text = empty ? CLI_QUICK_PLACEHOLDER : text, .length = empty ? (sizeof(CLI_QUICK_PLACEHOLDER) - 1ULL) : length,
        .color = empty ? palette->text_3 : palette->text, .clip = clip);
    const int caret_pad = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_PAD);
    const int caret_x = empty ? field.x + field_pad : (int)(after + 0.5f);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = drawing->raster,
               .rect = EMBED_CALL(
                   gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){caret_x, field.y + caret_pad,
                                    EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_CARET),
                                    field.height - (2 * caret_pad)},
                   .other = clip),
               .color = palette->caret);
}

// the rows drawn: each shown in its row of the list, the chosen one on the selection's ground, its label with what
// matched marked, its detail quieter after it and its keys at the right
EMBED_INLINE void quick_rows_draw(const QuickCtx *args)
{
    const GuiQuickRasterCfg *const drawing = args->drawing;
    const GuiPalette *const palette = drawing->palette;
    const GuiQuick *const quick = drawing->quick;
    const GuiRect list = quick->list;
    const int row_height = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_ROW);
    const int pad = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_ROW_PAD);
    const int gap = EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_GAP);
    const unsigned int radius =
        (unsigned int)EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = GUI_QUICK_ROW_RADIUS);
    const unsigned int label_size =
        (unsigned int)EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = (int)GUI_QUICK_LABEL_SIZE);
    const unsigned int detail_size =
        (unsigned int)EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = (int)GUI_QUICK_DETAIL_SIZE);
    const unsigned int keys_size =
        (unsigned int)EMBED_CALL(quick_scaled, QuickCtx, .drawing = drawing, .pixels = (int)GUI_QUICK_KEYS_SIZE);
    const unsigned int chosen = EMBED_CALL(cli_quick.chosen, CliQuickCfg, .at = 0u);
    const int label_most = (list.width * GUI_QUICK_LABEL_PERCENT) / 100;
    for (unsigned int shown = 0u; shown < quick->shown; shown += 1u)
    {
        const unsigned int at = quick->first + shown;
        CliQuickRow row;
        if (!EMBED_CALL(cli_quick.row, CliQuickCfg, .at = at, .row = &row))
        {
            break;
        }
        // the rows shown are few: their height is far inside what an int represents
        const GuiRect rect = {list.x, list.y + ((int)shown * row_height), list.width, row_height};
        if (at == chosen)
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = drawing->raster, .rect = rect,
                       .radius = radius, .color = palette->selection);
        }
        const GuiColor color = row.goes ? palette->text : palette->text_2;
        const int right = rect.x + rect.width - pad;
        // the keys stand at the right, the detail cut before them
        int keys_x = right;
        if (row.keys[0] != '\0')
        {
            const unsigned long long keys_length =
                cellul.len(&(CatenaFinitaCfg){.src = row.keys, .cap = CLI_LABEL_MAX_LEN});
            const float keys_width =
                EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = drawing->glyphs,
                           .face = GUI_FACE_CODE, .size = keys_size, .text = row.keys, .length = keys_length);
            keys_x = right - (int)(keys_width + 0.5f);
            (void)EMBED_CALL(
                gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = drawing->raster,
                .glyphs = drawing->glyphs, .face = GUI_FACE_CODE, .size = keys_size, .x_f32 = (float)keys_x,
                .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = drawing->glyphs,
                                .face = GUI_FACE_CODE, .size = keys_size, .top = rect.y, .pixel_height = rect.height),
                .text = row.keys, .length = keys_length, .color = palette->text_2, .clip = rect);
        }
        const GuiRect label_clip = {rect.x + pad, rect.y, label_most, rect.height};
        const float after = EMBED_CALL(
            quick_marked_draw, QuickCtx, .drawing = drawing, .face = GUI_FACE_HEAD, .size = label_size,
            .x_f32 = (float)(rect.x + pad),
            .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = drawing->glyphs,
                            .face = GUI_FACE_HEAD, .size = label_size, .top = rect.y, .pixel_height = rect.height),
            .text = row.label, .length = row.label_length, .hits = row.hits, .hit_count = row.hit_count, .color = color,
            .mark = palette->signal, .clip = label_clip);
        if (row.detail_length == 0ULL)
        {
            continue;
        }
        const int label_end = (int)(after + 0.5f);
        const int detail_x = ((label_end > rect.x + pad + label_most) ? rect.x + pad + label_most : label_end) + gap;
        const GuiRect detail_clip = {detail_x, rect.y, keys_x - gap - detail_x, rect.height};
        (void)EMBED_CALL(
            quick_marked_draw, QuickCtx, .drawing = drawing, .face = GUI_FACE_HEAD, .size = detail_size,
            .x_f32 = (float)detail_x,
            .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = drawing->glyphs,
                            .face = GUI_FACE_HEAD, .size = detail_size, .top = rect.y, .pixel_height = rect.height),
            .text = row.detail, .length = row.detail_length, .hits = row.detail_hits, .hit_count = row.detail_hit_count,
            .color = palette->text_3, .mark = palette->signal, .clip = detail_clip);
    }
}

void gui_quick_draw(const GuiQuickRasterCfg *args)
{
    GuiQuick *const quick = args->quick;
    if (!EMBED_CALL(cli_quick.shown, CliQuickCfg, .at = 0u))
    {
        quick->rect = (GuiRect){0, 0, 0, 0};
        quick->field = (GuiRect){0, 0, 0, 0};
        quick->list = (GuiRect){0, 0, 0, 0};
        return;
    }
    EMBED_CALL(quick_layout, QuickCtx, .drawing = args);
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = args->raster, .palette = args->palette, .rect = quick->rect,
               .scale = args->frame->scale);
    EMBED_CALL(quick_field_draw, QuickCtx, .drawing = args);
    EMBED_CALL(quick_rows_draw, QuickCtx, .drawing = args);
}

int gui_quick_holds(const GuiQuickXCfg *args)
{
    return !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->quick_read->rect) &&
           EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->quick_read->rect, .x = args->x,
                      .y = args->y);
}

int gui_quick_row_at(const GuiQuickXCfg *args)
{
    const GuiQuick *const quick = args->quick_read;
    const GuiRect list = quick->list;
    if ((quick->shown == 0u) || (list.height <= 0) ||
        !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = list, .x = args->x, .y = args->y))
    {
        return 0;
    }
    // the list's height is a row for each row shown, and a pixel inside the list stands on a row that is not negative
    const int row_height = list.height / (int)quick->shown;
    const int row = (row_height > 0) ? (args->y - list.y) / row_height : 0;
    if ((unsigned int)row >= quick->shown)
    {
        return 0;
    }
    *args->at = quick->first + (unsigned int)row;
    return 1;
}

int gui_quick_wheel(const GuiQuickCfg *args)
{
    GuiQuick *const quick = args->quick;
    const unsigned int rows = EMBED_CALL(cli_quick.rows, CliQuickCfg, .at = 0u);
    const unsigned int most = (rows > quick->shown) ? rows - quick->shown : 0u;
    // the rows are few: their count is far inside what an int represents
    int first = (int)quick->first + args->rows;
    first = (first < 0) ? 0 : ((first > (int)most) ? (int)most : first);
    if ((unsigned int)first == quick->first)
    {
        return 0;
    }
    quick->first = (unsigned int)first;
    return 1;
}
