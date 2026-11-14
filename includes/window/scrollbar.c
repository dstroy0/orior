// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scrollbar.c: the scrollbar every part that scrolls draws along its side or its foot
#include "scrollbar.h"

#include "frame.h"

// the shapes a layout makes: the arrow before and after, and the thumb's end before and after
#define SCROLLBAR_ARROW_BEFORE 0u
#define SCROLLBAR_ARROW_AFTER 1u
#define SCROLLBAR_END_BEFORE 2u
#define SCROLLBAR_END_AFTER 3u
// the samples across and down a pixel a shape's coverage is counted from
#define SCROLLBAR_SAMPLES 4

typedef struct
{
    const GuiScrollbar *bar_read;
    GuiScrollbar *bar;
    int pixels;
    int scale;
    unsigned int shape;
    GuiMask *mask;
    unsigned char *cover;
    int length;
    int thickness;
    double along;
    double cross;
} ScrollbarCtx;

// a size in unscaled pixels at `scale`, no more than a mask's side
EMBED_INLINE int scrollbar_px(const ScrollbarCtx *args)
{
    const int pixels =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels, .scale = args->scale);
    return (pixels > (int)GUI_SCROLLBAR_SIDE_MAX) ? (int)GUI_SCROLLBAR_SIDE_MAX : pixels;
}

// whether a point `along` and `cross` the shape's own way, in pixels, lies inside the shape: an arrow a triangle of
// three pixels' reach at the middle of its square, pointing to the scrollbar's start or end; a thumb's end half a disc
// of the thumb's half thickness, round to the start or the end
EMBED_INLINE int scrollbar_inside(const ScrollbarCtx *args)
{
    const double length = (double)args->length;
    const double thickness = (double)args->thickness;
    const int before = (args->shape == SCROLLBAR_ARROW_BEFORE) || (args->shape == SCROLLBAR_END_BEFORE);
    const double along = before ? args->along : (length - args->along);
    const double middle = thickness / 2.0;
    if ((args->shape == SCROLLBAR_ARROW_BEFORE) || (args->shape == SCROLLBAR_ARROW_AFTER))
    {
        const double reach = (double)EMBED_CALL(scrollbar_px, ScrollbarCtx, .pixels = 3, .scale = args->scale);
        const double point = (length - reach) / 2.0;
        const double down = along - point;
        const double off = (args->cross > middle) ? (args->cross - middle) : (middle - args->cross);
        return (down >= 0.0) && (down <= reach) && (off <= down);
    }
    const double off_along = along - length;
    const double off_cross = args->cross - middle;
    return ((off_along * off_along) + (off_cross * off_cross)) <= (length * length);
}

// a shape made as a mask, `length` along the scrollbar and `thickness` across it, each pixel's coverage counted from
// SCROLLBAR_SAMPLES by SCROLLBAR_SAMPLES points inside it
EMBED_INLINE void scrollbar_shape_make(const ScrollbarCtx *args)
{
    const int across = args->bar_read->across;
    GuiMask *const mask = args->mask;
    // a mask's sides are not negative and no more than GUI_SCROLLBAR_SIDE_MAX
    mask->width = (unsigned int)(across ? args->length : args->thickness);
    mask->height = (unsigned int)(across ? args->thickness : args->length);
    mask->left = 0;
    mask->top = 0;
    mask->cover = args->cover;
    for (int along = 0; along < args->length; along += 1)
    {
        for (int cross = 0; cross < args->thickness; cross += 1)
        {
            unsigned int inside = 0u;
            for (int sample = 0; sample < (SCROLLBAR_SAMPLES * SCROLLBAR_SAMPLES); sample += 1)
            {
                const double at_along = (double)along + (((double)(sample % SCROLLBAR_SAMPLES) + 0.5) / SCROLLBAR_SAMPLES);
                const double at_cross = (double)cross + (((double)(sample / SCROLLBAR_SAMPLES) + 0.5) / SCROLLBAR_SAMPLES);
                inside += (unsigned int)EMBED_CALL(scrollbar_inside, ScrollbarCtx, .scale = args->scale,
                                                   .shape = args->shape, .length = args->length,
                                                   .thickness = args->thickness, .along = at_along, .cross = at_cross);
            }
            const unsigned int at =
                across ? ((unsigned int)cross * mask->width) + (unsigned int)along
                       : ((unsigned int)along * mask->width) + (unsigned int)cross;
            // the samples inside of the samples, a share of 255
            args->cover[at] = (unsigned char)((inside * 255u) / (unsigned int)(SCROLLBAR_SAMPLES * SCROLLBAR_SAMPLES));
        }
    }
}

void gui_scrollbar_layout(const GuiScrollbarCfg *args)
{
    GuiScrollbar *const bar = args->bar;
    const int thickness = EMBED_CALL(scrollbar_px, ScrollbarCtx, .pixels = GUI_SCROLLBAR_WIDTH, .scale = args->scale);
    const int arrow = EMBED_CALL(scrollbar_px, ScrollbarCtx, .pixels = GUI_SCROLLBAR_ARROW, .scale = args->scale);
    const int made = (bar->arrow_before.cover != NULL) && (bar->across == args->across) && (bar->scale == args->scale);
    bar->across = args->across;
    bar->scale = args->scale;
    bar->rect = args->rect;
    bar->track = args->across
                     ? (GuiRect){args->rect.x + arrow, args->rect.y, args->rect.width - (2 * arrow), args->rect.height}
                     : (GuiRect){args->rect.x, args->rect.y + arrow, args->rect.width, args->rect.height - (2 * arrow)};
    bar->inset = EMBED_CALL(scrollbar_px, ScrollbarCtx, .pixels = 2, .scale = args->scale);
    bar->thickness = thickness - (2 * bar->inset);
    bar->least = EMBED_CALL(scrollbar_px, ScrollbarCtx, .pixels = GUI_SCROLLBAR_THUMB_MIN, .scale = args->scale);
    if (made)
    {
        return;
    }
    EMBED_CALL(scrollbar_shape_make, ScrollbarCtx, .bar_read = bar, .scale = args->scale,
               .shape = SCROLLBAR_ARROW_BEFORE, .mask = &bar->arrow_before, .cover = bar->arrow_cover[0],
               .length = arrow, .thickness = thickness);
    EMBED_CALL(scrollbar_shape_make, ScrollbarCtx, .bar_read = bar, .scale = args->scale,
               .shape = SCROLLBAR_ARROW_AFTER, .mask = &bar->arrow_after, .cover = bar->arrow_cover[1],
               .length = arrow, .thickness = thickness);
    EMBED_CALL(scrollbar_shape_make, ScrollbarCtx, .bar_read = bar, .scale = args->scale,
               .shape = SCROLLBAR_END_BEFORE, .mask = &bar->end_before, .cover = bar->end_cover[0],
               .length = bar->thickness / 2, .thickness = bar->thickness);
    EMBED_CALL(scrollbar_shape_make, ScrollbarCtx, .bar_read = bar, .scale = args->scale,
               .shape = SCROLLBAR_END_AFTER, .mask = &bar->end_after, .cover = bar->end_cover[1],
               .length = bar->thickness / 2, .thickness = bar->thickness);
}

int gui_scrollbar_needed(const GuiScrollbarCfg *args)
{
    return args->total > args->shown;
}

GuiRect gui_scrollbar_thumb(const GuiScrollbarCfg *args)
{
    const GuiScrollbar *const bar = args->bar_read;
    if (!gui_scrollbar_needed(args))
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const int track = bar->across ? bar->track.width : bar->track.height;
    // a track's length is far inside what an int represents, and the shares below never pass it
    const int share = (int)(((unsigned long long)track * args->shown) / args->total);
    const int length = (share < bar->least) ? bar->least : share;
    const unsigned long long most = args->total - args->shown;
    const unsigned long long first = (args->first < most) ? args->first : most;
    const int start = (int)(((unsigned long long)(track - length) * first) / most);
    return bar->across ? (GuiRect){bar->track.x + start, bar->track.y + bar->inset, length, bar->thickness}
                       : (GuiRect){bar->track.x + bar->inset, bar->track.y + start, bar->thickness, length};
}

void gui_scrollbar_draw(const GuiScrollbarCfg *args)
{
    const GuiScrollbar *const bar = args->bar_read;
    if (!gui_scrollbar_needed(args))
    {
        return;
    }
    const GuiRect rect = bar->rect;
    const GuiColor quiet = args->palette->text_3;
    const int after_x = bar->across ? (rect.x + rect.width - (int)bar->arrow_after.width) : rect.x;
    const int after_y = bar->across ? rect.y : (rect.y + rect.height - (int)bar->arrow_after.height);
    EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = args->raster, .x = rect.x, .y = rect.y,
               .mask_bits = &bar->arrow_before, .color = quiet, .clip = rect);
    EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = args->raster, .x = after_x, .y = after_y,
               .mask_bits = &bar->arrow_after, .color = quiet, .clip = rect);
    // the thumb: its end before, its straight middle, and its end after
    const GuiRect thumb = gui_scrollbar_thumb(args);
    const GuiColor color = args->pointed ? args->palette->text_2 : quiet;
    const int end = bar->thickness / 2;
    const GuiRect middle = bar->across ? (GuiRect){thumb.x + end, thumb.y, thumb.width - (2 * end), thumb.height}
                                       : (GuiRect){thumb.x, thumb.y + end, thumb.width, thumb.height - (2 * end)};
    EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = args->raster, .x = thumb.x, .y = thumb.y,
               .mask_bits = &bar->end_before, .color = color, .clip = rect);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = middle,
               .color = color);
    EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = args->raster,
               .x = bar->across ? (thumb.x + thumb.width - end) : thumb.x,
               .y = bar->across ? thumb.y : (thumb.y + thumb.height - end), .mask_bits = &bar->end_after,
               .color = color, .clip = rect);
}

unsigned int gui_scrollbar_hit(const GuiScrollbarCfg *args)
{
    const GuiScrollbar *const bar = args->bar_read;
    if (!gui_scrollbar_needed(args) ||
        !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = bar->rect, .x = args->x, .y = args->y))
    {
        return GUI_SCROLLBAR_NONE;
    }
    const GuiRect thumb = gui_scrollbar_thumb(args);
    const int at = bar->across ? args->x : args->y;
    const int track_start = bar->across ? bar->track.x : bar->track.y;
    const int track_end = track_start + (bar->across ? bar->track.width : bar->track.height);
    const int thumb_start = bar->across ? thumb.x : thumb.y;
    const int thumb_end = thumb_start + (bar->across ? thumb.width : thumb.height);
    if (at < track_start)
    {
        return GUI_SCROLLBAR_UP;
    }
    if (at >= track_end)
    {
        return GUI_SCROLLBAR_DOWN;
    }
    if (at < thumb_start)
    {
        return GUI_SCROLLBAR_ABOVE;
    }
    return (at < thumb_end) ? GUI_SCROLLBAR_THUMB : GUI_SCROLLBAR_BELOW;
}

void gui_scrollbar_first_at(const GuiScrollbarCfg *args)
{
    const GuiScrollbar *const bar = args->bar_read;
    if (!gui_scrollbar_needed(args))
    {
        *args->first_out = 0ull;
        return;
    }
    const GuiRect thumb = gui_scrollbar_thumb(args);
    const int room = bar->across ? (bar->track.width - thumb.width) : (bar->track.height - thumb.height);
    const int wanted = bar->across ? (args->x - bar->track.x) : (args->y - bar->track.y);
    const int start = (wanted < 0) ? 0 : ((wanted > room) ? room : wanted);
    const unsigned long long most = args->total - args->shown;
    // the place on the track is not negative past the tests, and rounds to the nearest line
    *args->first_out =
        (room > 0)
            ? ((((unsigned long long)start * most) + ((unsigned long long)room / 2ull)) / (unsigned long long)room)
            : 0ull;
}
