// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// minimap.c: the minimap part. The map's rows are the text's lines, two pixels each; the line at its first row follows
// the view's top through the text where the lines outnumber the rows, and stays put while the view does, so an edit
// that adds lines leaves the map where it stood
#include "minimap.h"

#include "../../../includes/window/frame.h"

// the share of a bar's color laid over the ground, 80%
#define MAP_BAR_ALPHA 0xccu

void gui_minimap_clear(const GuiMinimapCfg *args)
{
    args->map->anchored = 0;
    args->map->dragging = 0;
}

typedef struct
{
    int scale;
    float value;
    unsigned long long lines;
    GuiRect place;
    unsigned long long line;
    GuiRaster *raster;
    const GuiPalette *palette;
    const CliViewFrame *editor;
    GuiRect clip;
    GuiColor color;
} MinimapCtx;

// `place` filled with `color` no further than `clip`
EMBED_INLINE void map_fill(const MinimapCtx *args)
{
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect =
                   EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place, .other = args->clip),
               .color = args->color);
}
// a row's height in pixels at `scale`, one at the least
EMBED_INLINE int map_row_height(const MinimapCtx *args)
{
    int scale = args->scale;
    const int pixel_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_ROW, .scale = scale);
    return (pixel_height > 0) ? pixel_height : 1;
}

// `value` rounded to a line, no further than the text's first and last lines
EMBED_INLINE unsigned long long map_line_get(const MinimapCtx *args)
{
    const unsigned long long last = (args->lines != 0ULL) ? args->lines - 1ULL : 0ULL;
    if (args->value <= 0.0f)
    {
        return 0ULL;
    }
    // a line count's float is not negative here and rounds to a whole number
    const unsigned long long line = (unsigned long long)(args->value + 0.5f);
    return (line > last) ? last : line;
}

void gui_minimap_on_position(const GuiMinimapPositionCfg *args)
{
    int scale = args->scale;
    float on_screen = args->on_screen;
    GuiMapPosition *const at = &args->map->place;
    const int row_height = EMBED_CALL(map_row_height, MinimapCtx, .scale = scale);
    const int rows = args->place.height / row_height;
    const CliViewFrame *const frame = args->editor->frame;
    const unsigned long long lines = (frame != NULL) ? frame->lines : 0ULL;
    // the rows are counted where there is capacity for them
    at->fit = (rows > 0) ? (unsigned int)rows : 1u;
    at->lines = lines;
    at->on_screen = on_screen;
    const float scrolled = (float)lines - on_screen;
    at->range = (scrolled > 1.0f) ? scrolled : 1.0f;
    const unsigned long long top = (frame != NULL) ? frame->top : 0ULL;
    unsigned long long start = 0ULL;
    if (lines > (unsigned long long)at->fit)
    {
        const unsigned long long most = lines - (unsigned long long)at->fit;
        if (args->map->anchored && (args->map->anchor_top == top))
        {
            // the view has not moved since the map last stood: the map stands where it did beside it
            const long long saved = (long long)top - args->map->anchor_offset;
            start = (saved > 0LL) ? (unsigned long long)saved : 0ULL;
        }
        else
        {
            const float share = (float)top / at->range;
            start = (unsigned long long)((((share < 1.0f) ? share : 1.0f) * (float)most) + 0.5f);
        }
        start = (start > most) ? most : start;
    }
    at->start = start;
    args->map->anchored = 1;
    args->map->anchor_top = top;
    args->map->anchor_offset = (long long)top - (long long)start;
    // the slider's top is at or below the map's top, the view's top being at or below the map's first line
    at->slider_top = (int)(top - start) * row_height;
    const int min = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_SLIDER_MIN, .scale = scale);
    const int pixel_height = (int)((on_screen * (float)row_height) + 0.5f);
    at->slider_height = (pixel_height > min) ? pixel_height : min;
}

int gui_minimap_span(const GuiMinimapFirstCfg *args)
{
    unsigned long long *first = args->first;
    unsigned long long *last = args->last;
    const GuiMapPosition *const at = &args->map_read->place;
    if (at->lines == 0ULL)
    {
        return 0;
    }
    const unsigned long long end = at->start + (unsigned long long)at->fit;
    *first = at->start;
    *last = ((end < at->lines) ? end : at->lines) - 1ULL;
    return 1;
}

// the map's pixel row of the text's `line` on the overview side_menu, which stands for the whole text
EMBED_INLINE int map_strip_y(const MinimapCtx *args)
{
    const unsigned long long last = (args->lines != 0ULL) ? args->lines - 1ULL : 0ULL;
    const unsigned long long displayed = (args->line > last) ? last : args->line;
    const float size = (args->lines > 1ULL) ? (float)args->lines : 1.0f;
    return args->place.y + (int)(((float)displayed / size) * (float)args->place.height);
}

// the overview side_menu drawn down the map's right edge: the editor's ground, the widget line along its left, the
// matches and the caret's line
EMBED_INLINE void map_strip_draw(const MinimapCtx *args)
{
    GuiRaster *raster = args->raster;
    int scale = args->scale;
    const int side_menu_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_STRIP, .scale = scale);
    const int left = args->place.x + args->place.width - side_menu_width;
    const int inset = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale);
    const int mark = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_MARK, .scale = scale);
    // a line's share of the side_menu, a mark's height at the least
    const int share =
        (int)((float)args->place.height / ((args->editor->lines > 1ULL) ? (float)args->editor->lines : 1.0f));
    const int pixel_height = (share > mark) ? share : mark;
    EMBED_CALL(map_fill, MinimapCtx, .raster = raster, .clip = args->clip,
               .place = (GuiRect){left, args->place.y, side_menu_width, args->place.height},
               .color = args->palette->editor_background);
    EMBED_CALL(map_fill, MinimapCtx, .raster = raster, .clip = args->clip,
               .place = (GuiRect){left, args->place.y,
                                  EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                                  args->place.height},
               .color = args->palette->widget_line);
    if (args->editor->find.open)
    {
        for (unsigned long long at = 0ULL; at < args->editor->matches; at += 1ULL)
        {
            EMBED_CALL(map_fill, MinimapCtx, .raster = raster, .clip = args->clip,
                       .place = (GuiRect){left + inset,
                                          EMBED_CALL(map_strip_y, MinimapCtx, .place = args->place,
                                                     .lines = args->editor->lines, .line = args->editor->match[at][0]),
                                          side_menu_width - inset, pixel_height},
                       .color = args->palette->map_match);
        }
    }
    EMBED_CALL(map_fill, MinimapCtx, .raster = raster, .clip = args->clip,
               .place = (GuiRect){left + inset,
                                  EMBED_CALL(map_strip_y, MinimapCtx, .place = args->place,
                                             .lines = args->editor->lines, .line = args->editor->cursor.line),
                                  side_menu_width - inset, mark},
               .color = args->palette->caret);
}

void gui_minimap_draw(const GuiMinimapPositionCfg *args)
{
    const GuiPalette *palette = args->palette;
    const GuiEditor *const painter = args->editor;
    const CliViewFrame *const editor = painter->frame;
    int scale = args->scale;
    const GuiRect shown =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place, .other = args->clip);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = shown))
    {
        return;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = shown,
               .color = palette->editor_background);
    EMBED_CALL(map_fill, MinimapCtx, .raster = args->raster, .clip = shown,
               .place = (GuiRect){args->place.x, args->place.y,
                                  EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                                  args->place.height},
               .color = palette->line);
    if ((editor == NULL) || (editor->lines == 0ULL))
    {
        return;
    }
    EMBED_CALL(gui_minimap_position.place, GuiMinimapPositionCfg, .map = args->map, .place = args->place,
               .scale = scale, .editor = painter, .on_screen = args->on_screen);
    const GuiMapPosition *const at = &args->map->place;
    const int row_height = EMBED_CALL(map_row_height, MinimapCtx, .scale = scale);
    const int side_menu_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_STRIP, .scale = scale);
    const int margin = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_MARGIN, .scale = scale);
    const int cell = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_CHAR, .scale = scale);
    const int top_menu_height = ((row_height * 3) / 4 > 0) ? (row_height * 3) / 4 : 1;
    const GuiRect rows = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = shown,
        .other = (GuiRect){args->place.x, args->place.y, args->place.width - side_menu_width, args->place.height});
    const unsigned long long first = at->start;
    const unsigned long long reach = at->start + (unsigned long long)at->fit;
    const unsigned long long end = (reach < editor->lines) ? reach : editor->lines;
    // the selection's rows, its ends in order
    const int anchored = (editor->anchor.line < editor->cursor.line) ||
                         ((editor->anchor.line == editor->cursor.line) && (editor->anchor.col < editor->cursor.col));
    const CliPosition selected_from = anchored ? editor->anchor : editor->cursor;
    const CliPosition selected_to = anchored ? editor->cursor : editor->anchor;
    const int selected = (selected_from.line != selected_to.line) || (selected_from.col != selected_to.col);
    if (selected && (selected_to.line >= first) && (selected_from.line < end))
    {
        const unsigned long long top_row = (selected_from.line > first) ? selected_from.line : first;
        const unsigned long long bottom_row = (selected_to.line < end - 1ULL) ? selected_to.line : end - 1ULL;
        // the rows are within the map's, which an int counts
        EMBED_CALL(map_fill, MinimapCtx, .raster = args->raster, .clip = rows,
                   .place = (GuiRect){rows.x, args->place.y + ((int)(top_row - first) * row_height), rows.width,
                                      (int)(bottom_row - top_row + 1ULL) * row_height},
                   .color = palette->map_selection);
    }
    if (editor->find.open)
    {
        for (unsigned long long each = 0ULL; each < editor->matches; each += 1ULL)
        {
            const unsigned long long line = editor->match[each][0];
            if ((line >= first) && (line < end))
            {
                EMBED_CALL(map_fill, MinimapCtx, .raster = args->raster, .clip = rows,
                           .place = (GuiRect){rows.x, args->place.y + ((int)(line - first) * row_height), rows.width,
                                              row_height},
                           .color = palette->map_match);
            }
        }
    }
    // each line's bars, a line the map does not hold left as ground
    const CliDocumentMap *const held = args->map->lines;
    for (unsigned long long line = first; (held != NULL) && (line < end); line += 1ULL)
    {
        if ((line < held->from) || (line >= held->from + (unsigned long long)held->count))
        {
            continue;
        }
        const unsigned int line_index = (unsigned int)(line - held->from);
        const int y = args->place.y + ((int)(line - first) * row_height);
        for (unsigned int bar = held->line_at[line_index]; bar < held->line_at[line_index + 1u]; bar += 1u)
        {
            const CliDocumentMapBar *const one = &held->bar[bar];
            if (one->start >= GUI_MAP_WIDEST)
            {
                continue;
            }
            const unsigned int width =
                (one->start + one->width > GUI_MAP_WIDEST) ? GUI_MAP_WIDEST - one->start : one->width;
            const GuiColor color = EMBED_CALL(gui_editor_palette.color, GuiEditorPaletteCfg, .editor_read = painter,
                                              .palette = palette, .name = one->name);
            // the bars' cells are within the map's width, which an int counts
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                          .one = (GuiRect){args->place.x + margin + ((int)one->start * cell), y,
                                                           (int)width * cell, top_menu_height},
                                          .other = rows),
                       .color = (color & 0x00ffffffu) | (MAP_BAR_ALPHA << 24u));
        }
    }
    EMBED_CALL(map_fill, MinimapCtx, .raster = args->raster, .clip = shown,
               .place = (GuiRect){args->place.x, args->place.y + at->slider_top, args->place.width, at->slider_height},
               .color = args->map->dragging ? palette->map_slider_on : palette->map_slider);
    EMBED_CALL(map_strip_draw, MinimapCtx, .raster = args->raster, .palette = palette, .editor = editor,
               .place = args->place, .clip = shown, .scale = scale);
}

int gui_minimap_press(const GuiMinimapPositionScaleCfg *args)
{
    int scale = args->scale;
    unsigned long long *top = args->top;
    const GuiRect shown =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place, .other = args->clip);
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = shown, .x = args->x, .y = args->y) ||
        (args->editor->frame == NULL) || (args->editor->frame->lines == 0ULL))
    {
        return 0;
    }
    EMBED_CALL(gui_minimap_position.place, GuiMinimapPositionCfg, .map = args->map, .place = args->place,
               .scale = scale, .editor = args->editor, .on_screen = args->on_screen);
    const GuiMapPosition *const at = &args->map->place;
    const int local_y = args->y - args->place.y;
    const int side_menu_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_STRIP, .scale = scale);
    if (args->x >= args->place.x + args->place.width - side_menu_width)
    {
        // the side_menu stands for the whole text: the part pressed is centered in the view
        const float share = (float)local_y / (float)args->place.height;
        *top = EMBED_CALL(map_line_get, MinimapCtx,
                          .value = (share * (float)args->editor->frame->lines) - (args->on_screen / 2.0f),
                          .lines = args->editor->frame->lines);
        return 1;
    }
    unsigned long long top_line = args->editor->frame->top;
    if ((local_y < at->slider_top) || (local_y > at->slider_top + at->slider_height))
    {
        const unsigned long long row =
            at->start + (unsigned long long)(local_y / EMBED_CALL(map_row_height, MinimapCtx, .scale = scale));
        top_line = EMBED_CALL(map_line_get, MinimapCtx, .value = (float)row - (args->on_screen / 2.0f),
                              .lines = args->editor->frame->lines);
    }
    *top = top_line;
    args->map->dragging = 1;
    args->map->drag_y = args->y;
    args->map->drag_top = top_line;
    return 1;
}

int gui_minimap_drag(const GuiMinimapPositionScaleCfg *args)
{
    unsigned long long *top = args->top;
    if (!args->map->dragging)
    {
        return 0;
    }
    const GuiMapPosition *const at = &args->map->place;
    // the slider's run down the map: the rows the map shows less the view's, in pixels
    const float displayed = ((unsigned long long)at->fit < at->lines) ? (float)at->fit : (float)at->lines;
    const float run = (displayed - at->on_screen) * (float)GUI_MAP_ROW;
    const float span = (run > 1.0f) ? run : 1.0f;
    const float moved = ((float)(args->y - args->map->drag_y) * at->range) / span;
    *top = EMBED_CALL(map_line_get, MinimapCtx, .value = (float)args->map->drag_top + moved, .lines = at->lines);
    return 1;
}
