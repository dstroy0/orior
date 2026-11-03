// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// statusbar.c: the status bar's items stand from its right end, the lock first, then the editor's line and the memory
// reading, each a gap apart, and the crumbs from its left, the folders in the bar's text, the file in its bright text
// and the marks between them in the path's
#include "statusbar.h"
#include "../../text/text.h"

// the crumbs' mark between a folder and what it holds
static const char s_crumb_mark[] = "\xe2\x80\xba";

void gui_status_memory_text(const GuiStatusbarBytesCfg *args)
{
    VerbaLinea line = {args->text, args->size, 0ULL};
    const unsigned long long tenths = ((args->bytes * 10ULL) + (1024ULL * 512ULL)) / (1024ULL * 1024ULL);
    if (tenths < 1000ULL)
    {
        mmgr_line_decimal(&line, tenths / 10ULL, 1u);
        mmgr_line_ch(&line, '.');
        mmgr_line_decimal(&line, tenths % 10ULL, 1u);
    }
    else
    {
        mmgr_line_decimal(&line, (args->bytes + (1024ULL * 512ULL)) / (1024ULL * 1024ULL), 1u);
    }
    mmgr_line_put(&line, " MB");
    (void)mmgr_line_finish(&line);
}

void gui_status_cursor_text(const GuiStatusbarCfg *args)
{
    VerbaLinea line = {args->text, args->size, 0ULL};
    mmgr_line_decimal(&line, args->status->line + 1ULL, 1u);
    mmgr_line_ch(&line, ':');
    mmgr_line_decimal(&line, args->status->col + 1ULL, 1u);
    (void)mmgr_line_finish(&line);
}

void gui_status_selection_text(const GuiStatusbarCfg *args)
{
    VerbaLinea line = {args->text, args->size, 0ULL};
    const unsigned long long lines = (args->status->anchor_line > args->status->line)
                                         ? args->status->anchor_line - args->status->line
                                         : args->status->line - args->status->anchor_line;
    const unsigned long long letters = (args->status->anchor_col > args->status->col)
                                           ? args->status->anchor_col - args->status->col
                                           : args->status->col - args->status->anchor_col;
    if (lines != 0ULL)
    {
        mmgr_line_decimal(&line, lines + 1ULL, 1u);
        mmgr_line_put(&line, " lines selected");
    }
    else if (letters != 0ULL)
    {
        mmgr_line_decimal(&line, letters, 1u);
        mmgr_line_put(&line, " selected");
    }
    (void)mmgr_line_finish(&line);
}

void gui_status_indent_text(const GuiStatusbarCfg *args)
{
    VerbaLinea line = {args->text, args->size, 0ULL};
    const unsigned int step = (args->status->indent_size != 0u) ? args->status->indent_size : 4u;
    if (args->status->indent_tabs == 1)
    {
        mmgr_line_put(&line, "Tab ");
        mmgr_line_decimal(&line, step, 1u);
    }
    else
    {
        mmgr_line_decimal(&line, step, 1u);
        mmgr_line_put(&line, " spaces");
    }
    (void)mmgr_line_finish(&line);
}

typedef struct
{
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    unsigned int size;
    float right;
    int baseline;
    const char *text;
    GuiColor color;
    GuiRect clip;
} StatusbarCtx;
// an item's text drawn ending at `right`, on the baseline, in `color`: the x it starts at
EMBED_INLINE float statusbar_item_draw(const StatusbarCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    unsigned int size = args->size;
    const char *text = args->text;
    GuiColor color = args->color;
    GuiRect clip = args->clip;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = GUI_STATUS_TEXT});
    const float pixel_width = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                         .face = GUI_FACE_HEAD, .size = size, .text = text, .length = length);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = args->right - pixel_width, .y = args->baseline,
                     .text = text, .length = length, .color = color, .clip = clip);
    return args->right - pixel_width;
}

void gui_statusbar_draw(const GuiStatusbarStatusCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    GuiIcons *icons = args->icons;
    const int scale = args->frame->scale;
    // the bar's pane is the slat its edge shows, whole outside the claim, laid by the frame's base; the items stand
    // in the slat
    const GuiRect bar = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->status_band,
                                   .other = args->frame->status);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = bar))
    {
        return;
    }
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_STATUS_SIZE, .scale = scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size);
    const int baseline = bar.y + (int)((((float)bar.height - ascent - descent) / 2.0f) + ascent + 0.5f);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STATUS_GAP, .scale = scale);
    char text[GUI_STATUS_TEXT];
    // the items from the right: the lock, the indentation, the encoding, the line ends, the selection and the cursor,
    // while a file is shown, then the memory reading
    float right =
        (float)(bar.x + bar.width -
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STATUS_PAD, .scale = scale));
    if (args->status->has_cursor)
    {
        const int lock =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STATUS_LOCK, .scale = scale);
        right -= (float)(EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                    .pixels = GUI_FRAME_GAP + GUI_STATUS_LOCK_END, .scale = scale) +
                         lock);
        const GuiMask *const mask =
            EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = icons, .name = "unlock", .size = (unsigned int)lock,
                       .stroke = GUI_ICON_STROKE * (float)lock / GUI_ICON_UNITS);
        if (mask != NULL)
        {
            EMBED_CALL(gui_raster_xy_color.mask, GuiRasterXYColorCfg, .raster = raster, .mask = mask, .x = (int)right,
                       .y = bar.y + ((bar.height - lock) / 2), .color = args->palette->status_text, .clip = bar);
        }
        right -= (float)gap;
        EMBED_CALL(gui_statusbar.status_indent_text, GuiStatusbarCfg, .status = args->status, .text = text,
                   .size = sizeof(text));
        right = EMBED_CALL(statusbar_item_draw, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size,
                           .right = right, .baseline = baseline, .text = text, .color = args->palette->status_text,
                           .clip = bar) -
                (float)gap;
        right = EMBED_CALL(statusbar_item_draw, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size,
                           .right = right, .baseline = baseline, .text = "UTF-8", .color = args->palette->status_text,
                           .clip = bar) -
                (float)gap;
        right = EMBED_CALL(statusbar_item_draw, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size,
                           .right = right, .baseline = baseline, .text = args->status->crlf ? "CRLF" : "LF",
                           .color = args->palette->status_text, .clip = bar) -
                (float)gap;
        EMBED_CALL(gui_statusbar.status_selection_text, GuiStatusbarCfg, .status = args->status, .text = text,
                   .size = sizeof(text));
        if (text[0] != '\0')
        {
            right = EMBED_CALL(statusbar_item_draw, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size,
                               .right = right, .baseline = baseline, .text = text, .color = args->palette->status_text,
                               .clip = bar) -
                    (float)gap;
        }
        EMBED_CALL(gui_statusbar.status_cursor_text, GuiStatusbarCfg, .status = args->status, .text = text,
                   .size = sizeof(text));
        right = EMBED_CALL(statusbar_item_draw, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size,
                           .right = right, .baseline = baseline, .text = text, .color = args->palette->status_text,
                           .clip = bar) -
                (float)gap;
    }
    if (args->status->memory != 0ULL)
    {
        EMBED_CALL(gui_statusbar_bytes.status_memory_text, GuiStatusbarBytesCfg, .bytes = args->status->memory,
                   .text = text, .size = sizeof(text));
        const float min = (float)GUI_STATUS_MEMORY_ZEROS *
                          EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size, .text = "0", .length = 1ULL);
        const float pixel_width = EMBED_CALL(
            gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD,
            .size = size, .text = text, .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = GUI_STATUS_TEXT}));
        (void)EMBED_CALL(statusbar_item_draw, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size,
                         .right = right, .baseline = baseline, .text = text, .color = args->palette->status_text,
                         .clip = bar);
        right -= ((pixel_width > min) ? pixel_width : min) + (float)gap;
    }
    // the crumbs from the left, as far as the items leave
    if (args->status->path[0] == '\0')
    {
        return;
    }
    const GuiRect capacity = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                        .one = (GuiRect){bar.x, bar.y, (int)right - bar.x, bar.height}, .other = bar);
    // the crumbs start past the strip's width, which the claim takes to none
    // a share of a width in pixels, rounded, is far inside what an int represents
    const int strip_capacity =
        (int)((((long long)(GUI_FIXED_ONE - args->frame->claim.claim) * (long long)args->frame->strip.width) +
               (GUI_FIXED_ONE / 2)) /
              GUI_FIXED_ONE);
    float x = (float)(args->frame->status.x + strip_capacity +
                      EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STATUS_PAD, .scale = scale));
    const float crumb_gap =
        (float)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STATUS_CRUMB_GAP, .scale = scale);
    const char *at = args->status->path;
    while (*at != '\0')
    {
        unsigned long long length = 0ULL;
        length = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = at, .at = length,
                            .size = length + cellul.len(&(CatenaFinitaCfg){.src = at + length, .cap = CLI_CELLS_POOL}),
                            .set = "/\\");
        const int last = at[length] == '\0';
        x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                       .face = GUI_FACE_HEAD, .size = size, .x_f32 = x, .y = baseline, .text = at, .length = length,
                       .color = last ? args->palette->bar_bright : args->palette->status_text, .clip = capacity);
        if (last)
        {
            break;
        }
        x += crumb_gap;
        x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                       .face = GUI_FACE_HEAD, .size = size, .x_f32 = x, .y = baseline, .text = s_crumb_mark,
                       .length = sizeof(s_crumb_mark) - 1u, .color = args->palette->bar_path, .clip = capacity) +
            crumb_gap;
        at += length + 1ULL;
    }
}
