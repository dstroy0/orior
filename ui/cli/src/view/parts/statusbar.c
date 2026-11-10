// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// statusbar.c: the status bar's items stand from its right end, the lock first, then the editor's line, Focus and the
// memory reading, each a gap apart, and the crumbs from its left as the crumbs part lays them, the notice a gap after
// them with Silence beside it. One pass lays the bar out and draws it, or with no raster lays it out alone, so a press
// finds what the drawing showed
#include "statusbar.h"
#include "../../text/text.h"
#include "../view.h"
#include "crumbs.h"

// the ellipsis a notice cut short ends in, Silence and Focus
static const char s_ellipsis[] = "\xe2\x80\xa6";
static const char s_silence[] = "Silence";
static const char s_focus[] = "Focus";

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
    float x;
    float most;
    int baseline;
    const char *text;
    unsigned long long length;
    unsigned long long *length_out;
    GuiColor color;
    GuiRect clip;
} StatusbarCtx;

// `length` bytes of `text` drawn from x on the baseline, in `color`, where there is a raster to draw on: the x after it
EMBED_INLINE float statusbar_text(const StatusbarCtx *args)
{
    if (args->raster == NULL)
    {
        return args->x + EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs,
                                    .face = GUI_FACE_HEAD, .size = args->size, .text = args->text,
                                    .length = args->length);
    }
    return EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                      .glyphs = args->glyphs, .face = GUI_FACE_HEAD, .size = args->size, .x_f32 = args->x,
                      .y = args->baseline, .text = args->text, .length = args->length, .color = args->color,
                      .clip = args->clip);
}

// an item's text drawn ending at `right`, on the baseline, in `color`: the x it starts at
EMBED_INLINE float statusbar_item_draw(const StatusbarCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = GUI_STATUS_TEXT});
    const float pixel_width =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs, .face = GUI_FACE_HEAD,
                   .size = args->size, .text = args->text, .length = length);
    (void)EMBED_CALL(statusbar_text, StatusbarCtx, .raster = args->raster, .glyphs = args->glyphs, .size = args->size,
                     .x = args->right - pixel_width, .baseline = args->baseline, .text = args->text, .length = length,
                     .color = args->color, .clip = args->clip);
    return args->right - pixel_width;
}

// how many bytes of `text` show within `most` pixels, its code points whole: all of it where it fits, else as many as
// fit with the ellipsis after them, which `length_out` then counts and 1 returns; 0 where the whole fits
EMBED_INLINE int statusbar_fitted(const StatusbarCtx *args)
{
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    *args->length_out = whole;
    if (EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs, .face = GUI_FACE_HEAD,
                   .size = args->size, .text = args->text, .length = whole) <= args->most)
    {
        return 0;
    }
    const float dots =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs, .face = GUI_FACE_HEAD,
                   .size = args->size, .text = s_ellipsis, .length = sizeof(s_ellipsis) - 1u);
    // the longest start of the text at a code point's first byte whose width and the ellipsis's fit
    unsigned long long low = 0ULL;
    unsigned long long high = whole;
    while (low < high)
    {
        unsigned long long middle = low + ((high - low + 1ULL) / 2ULL);
        while ((middle > low) && ((((unsigned char)args->text[middle]) & 0xc0u) == 0x80u))
        {
            middle -= 1ULL;
        }
        // no code point starts between: the one after `low` is the next to try
        if (middle == low)
        {
            middle = low + 1ULL;
            while ((middle < whole) && ((((unsigned char)args->text[middle]) & 0xc0u) == 0x80u))
            {
                middle += 1ULL;
            }
            if (middle > high)
            {
                break;
            }
        }
        const float width = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs,
                                       .face = GUI_FACE_HEAD, .size = args->size, .text = args->text, .length = middle);
        if (width + dots <= args->most)
        {
            low = middle;
        }
        else
        {
            high = middle - 1ULL;
        }
    }
    *args->length_out = low;
    return 1;
}

void gui_statusbar_draw(const GuiStatusbarStatusCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    GuiIcons *icons = args->icons;
    const int scale = args->frame->scale;
    GuiStatusPlaces none;
    memor.set(&(MemoriaCfg){.dst = &none, .val = 0, .bytes = sizeof(none)});
    GuiStatusPlaces *const places = (args->places != NULL) ? args->places : &none;
    *places = none;
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
    const int baseline = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size, .top = bar.y, .pixel_height = bar.height);
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
        const GuiMask *const mask_bits =
            EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = icons, .name = "unlock", .size = (unsigned int)lock,
                       .stroke = GUI_ICON_STROKE * (float)lock / GUI_ICON_UNITS);
        if ((mask_bits != NULL) && (raster != NULL))
        {
            EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = raster, .mask_bits = mask_bits,
                       .x = (int)right, .y = bar.y + ((bar.height - lock) / 2), .color = args->palette->status_text,
                       .clip = bar);
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
    // Focus, while it holds the notices, in the signal, with how many wait
    if (args->status->focus)
    {
        VerbaLinea focused = {text, sizeof(text), 0ULL};
        mmgr_line_put(&focused, s_focus);
        if (args->status->focus_waiting != 0u)
        {
            mmgr_line_put(&focused, ", ");
            mmgr_line_decimal(&focused, args->status->focus_waiting, 1u);
            mmgr_line_put(&focused, " waiting");
        }
        (void)mmgr_line_finish(&focused);
        const float end = right;
        right =
            EMBED_CALL(statusbar_item_draw, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size,
                       .right = right, .baseline = baseline, .text = text, .color = args->palette->signal, .clip = bar);
        places->focus = (GuiRect){(int)right, bar.y, (int)(end - right + 0.5f), bar.height};
        right -= (float)gap;
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
    const GuiRect capacity = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                        .one = (GuiRect){bar.x, bar.y, (int)right - bar.x, bar.height}, .other = bar);
    // the crumbs start past the strip's width, which the claim takes to none
    // a share of a width in pixels, rounded, is far inside what an int represents
    const int strip_capacity =
        (int)((((long long)(GUI_FIXED_ONE - args->frame->claim.claim) * (long long)args->frame->strip.width) +
               (GUI_FIXED_ONE / 2)) /
              GUI_FIXED_ONE);
    const float crumbs_x =
        (float)(args->frame->status.x + strip_capacity +
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STATUS_PAD, .scale = scale));
    // the crumbs as the crumbs part lays them, the notice a gap after them where any stands
    float x = EMBED_CALL(gui_crumbs.draw, GuiCrumbsCfg, .raster = raster, .glyphs = glyphs, .palette = args->palette,
                         .size = size, .x_f32 = crumbs_x, .baseline = baseline, .clip = capacity, .scale = scale);
    x += (EMBED_CALL(gui_crumbs.count, GuiCrumbsCfg, .index = 0u) != 0u) ? (float)gap : 0.0f;
    // the notice's first line, as much as half the window's width and the width before the items, the ellipsis ending
    // what does not fit, a failure's in the fail color, and Silence beside it, quieter
    if ((args->status->notice == NULL) || (args->status->notice[0] == '\0'))
    {
        return;
    }
    const float notice_gap =
        (float)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STATUS_NOTICE_GAP, .scale = scale);
    const float silence =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                   .size = size, .text = s_silence, .length = sizeof(s_silence) - 1u);
    const float share = ((float)args->frame->width * (float)GUI_STATUS_NOTICE_SHARE) / 100.0f;
    const float space = (float)capacity.x + (float)capacity.width - x - notice_gap - silence;
    const float most = (share < space) ? share : space;
    if (most <= 0.0f)
    {
        return;
    }
    unsigned long long shown = 0ULL;
    const int cut = EMBED_CALL(statusbar_fitted, StatusbarCtx, .glyphs = glyphs, .size = size,
                               .text = args->status->notice, .most = most, .length_out = &shown);
    const GuiColor notice_color = args->status->notice_failed ? args->palette->fail : args->palette->status_text;
    const float start = x;
    x = EMBED_CALL(statusbar_text, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size, .x = x,
                   .baseline = baseline, .text = args->status->notice, .length = shown, .color = notice_color,
                   .clip = capacity);
    if (cut)
    {
        x = EMBED_CALL(statusbar_text, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size, .x = x,
                       .baseline = baseline, .text = s_ellipsis, .length = sizeof(s_ellipsis) - 1u,
                       .color = notice_color, .clip = capacity);
    }
    places->notice = (GuiRect){(int)start, bar.y, (int)(x - start + 0.5f), bar.height};
    x += notice_gap;
    // Silence in the bar's quiet text
    const GuiColor quiet = args->palette->status_quiet;
    const float silence_start = x;
    x = EMBED_CALL(statusbar_text, StatusbarCtx, .raster = raster, .glyphs = glyphs, .size = size, .x = x,
                   .baseline = baseline, .text = s_silence, .length = sizeof(s_silence) - 1u, .color = quiet,
                   .clip = capacity);
    places->silence = (GuiRect){(int)silence_start, bar.y, (int)(x - silence_start + 0.5f), bar.height};
}

unsigned int gui_statusbar_hit(const GuiStatusbarXCfg *args)
{
    GuiStatusPlaces places;
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .status = args->status, .raster = NULL,
               .glyphs = args->glyphs, .icons = args->icons,
               .palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK),
               .frame = args->frame, .places = &places);
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = places.notice, .x = args->x, .y = args->y))
    {
        return CLI_HIT_STATUS_NOTICE;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = places.silence, .x = args->x, .y = args->y))
    {
        return CLI_HIT_STATUS_SILENCE;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = places.focus, .x = args->x, .y = args->y))
    {
        return CLI_HIT_STATUS_FOCUS;
    }
    return CLI_HIT_NONE;
}
