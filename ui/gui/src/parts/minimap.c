// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// minimap.c: the minimap part. The map's rows are the text's lines, two pixels each; the line at its first row follows
// the view's top through the text where the lines outnumber the rows, and stays put while the view does, so an edit
// that adds lines leaves the map where it stood
#include "minimap.h"

#include "../answer.h"
#include "../frame.h"

// the share of a bar's color laid over the ground, 80%
#define MAP_BAR_ALPHA 0xccu

int gui_minimap_init(const GuiMinimapJsonCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->map, .val = 0, .bytes = sizeof(*args->map)});
    args->map->stale = 1;
    args->map->line_at = g_cli_cells->persistent_buf_alloc((GUI_MAP_LINES_MOST + 1u) * sizeof(*args->map->line_at),
                                                           &g_gui_minimap_cells);
    args->map->bar =
        g_cli_cells->persistent_buf_alloc(GUI_MAP_BARS_MOST * sizeof(*args->map->bar), &g_gui_minimap_cells);
    if ((args->map->line_at == NULL) || (args->map->bar == NULL))
    {
        gui_minimap_json.free(args);
        return 0;
    }
    return 1;
}

void gui_minimap_free(const GuiMinimapJsonCfg *args)
{
    g_cli_cells->persistent_buf_release(args->map->line_at, &g_gui_minimap_cells);
    g_cli_cells->persistent_buf_release(args->map->bar, &g_gui_minimap_cells);
    args->map->line_at = NULL;
    args->map->bar = NULL;
    args->map->count = 0u;
    args->map->bars = 0u;
}

void gui_minimap_clear(const GuiMinimapJsonCfg *args)
{
    args->map->from = 0ull;
    args->map->count = 0u;
    args->map->bars = 0u;
    args->map->stale = 1;
    args->map->anchored = 0;
    args->map->dragging = 0;
}

int gui_minimap_put(const GuiMinimapJsonCfg *args)
{
    const CliJson *json = args->json;
    const unsigned int lines = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->root, .name = "bars");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->root,
                    .kind = CFG_JSON_OBJECT) ||
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = lines,
                    .kind = CFG_JSON_ARRAY) ||
        (args->map->line_at == NULL))
    {
        return 0;
    }
    args->map->from = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = args->root,
                                 .name = "from", .otherwise = 0ull);
    const unsigned int count = json->node[lines].count;
    args->map->count = (count > GUI_MAP_LINES_MOST) ? GUI_MAP_LINES_MOST : count;
    args->map->bars = 0u;
    for (unsigned int line = 0u; line < args->map->count; line += 1u)
    {
        args->map->line_at[line] = args->map->bars;
        const unsigned int bars = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = lines, .index = line);
        const unsigned int found = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = json, .node = bars, .kind = CFG_JSON_ARRAY)
                                       ? json->node[bars].count
                                       : 0u;
        for (unsigned int at = 0u; (at < found) && (args->map->bars < GUI_MAP_BARS_MOST); at += 1u)
        {
            const unsigned int bar = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                .json_read = json, .array = bars, .index = at);
            // a bar's start, width and name are each far inside what an unsigned int represents
            args->map->bar[args->map->bars].start = (unsigned int)EMBED_CALL(
                gui_answer_table.whole, GuiAnswerCfg, .json = json,
                .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                   .array = bar, .index = 0u),
                .otherwise = 0ull);
            args->map->bar[args->map->bars].width = (unsigned int)EMBED_CALL(
                gui_answer_table.whole, GuiAnswerCfg, .json = json,
                .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                   .array = bar, .index = 1u),
                .otherwise = 0ull);
            args->map->bar[args->map->bars].name = (unsigned int)EMBED_CALL(
                gui_answer_table.whole, GuiAnswerCfg, .json = json,
                .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                   .array = bar, .index = 2u),
                .otherwise = 0ull);
            args->map->bars += 1u;
        }
    }
    args->map->line_at[args->map->count] = args->map->bars;
    args->map->stale = 0;
    return 1;
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
    const GuiEditor *editor;
} MinimapCtx;
// a row's height in pixels at `scale`, one at the least
EMBED_INLINE int map_row_tall(const MinimapCtx *args)
{
    int scale = args->scale;
    const int tall = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_ROW, .scale = scale);
    return (tall > 0) ? tall : 1;
}

// `value` rounded to a line, no further than the text's first and last lines
EMBED_INLINE unsigned long long map_line_get(const MinimapCtx *args)
{
    const unsigned long long last = (args->lines != 0ull) ? args->lines - 1ull : 0ull;
    if (args->value <= 0.0f)
    {
        return 0ull;
    }
    // a line count's float is not negative here and rounds to a whole number
    const unsigned long long line = (unsigned long long)(args->value + 0.5f);
    return (line > last) ? last : line;
}

void gui_minimap_on_place(const GuiMinimapPlaceCfg *args)
{
    int scale = args->scale;
    float on_screen = args->on_screen;
    GuiMapPlace *const at = &args->map->place;
    const int row_tall = EMBED_CALL(map_row_tall, MinimapCtx, .scale = scale);
    const int rows = args->place.height / row_tall;
    // the rows are counted where there is room for them
    at->fit = (rows > 0) ? (unsigned int)rows : 1u;
    at->lines = args->editor->lines;
    at->on_screen = on_screen;
    const float scrolled = (float)args->editor->lines - on_screen;
    at->range = (scrolled > 1.0f) ? scrolled : 1.0f;
    const unsigned long long top = args->editor->top;
    unsigned long long start = 0ull;
    if (args->editor->lines > (unsigned long long)at->fit)
    {
        const unsigned long long most = args->editor->lines - (unsigned long long)at->fit;
        if (args->map->anchored && (args->map->anchor_top == top))
        {
            // the view has not moved since the map last stood: the map stands where it did beside it
            const long long saved = (long long)top - args->map->anchor_offset;
            start = (saved > 0ll) ? (unsigned long long)saved : 0ull;
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
    at->slider_top = (int)(top - start) * row_tall;
    const int least =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_SLIDER_LEAST, .scale = scale);
    const int tall = (int)((on_screen * (float)row_tall) + 0.5f);
    at->slider_tall = (tall > least) ? tall : least;
}

int gui_minimap_span(const GuiMinimapFirstCfg *args)
{
    unsigned long long *first = args->first;
    unsigned long long *last = args->last;
    const GuiMapPlace *const at = &args->map_read->place;
    if (at->lines == 0ull)
    {
        return 0;
    }
    const unsigned long long end = at->start + (unsigned long long)at->fit;
    *first = at->start;
    *last = ((end < at->lines) ? end : at->lines) - 1ull;
    return 1;
}

int gui_minimap_current(const GuiMinimapCfg *args)
{
    return !args->map_read->stale && (args->map_read->from == args->first_value) &&
           ((unsigned long long)args->map_read->count == args->last_value - args->first_value + 1ull);
}

// the map's pixel row of the text's `line` on the overview strip, which stands for the whole text
EMBED_INLINE int map_strip_y(const MinimapCtx *args)
{
    const unsigned long long last = (args->lines != 0ull) ? args->lines - 1ull : 0ull;
    const unsigned long long shown = (args->line > last) ? last : args->line;
    const float size = (args->lines > 1ull) ? (float)args->lines : 1.0f;
    return args->place.y + (int)(((float)shown / size) * (float)args->place.height);
}

// the overview strip drawn down the map's right edge: the editor's ground, the widget line along its left, the matches
// and the caret's line
EMBED_INLINE void map_strip_draw(const MinimapCtx *args)
{
    GuiRaster *raster = args->raster;
    int scale = args->scale;
    const int strip_wide =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_STRIP, .scale = scale);
    const int left = args->place.x + args->place.width - strip_wide;
    const int inset = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale);
    const int mark = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_MARK, .scale = scale);
    // a line's share of the strip, a mark's height at the least
    const int share =
        (int)((float)args->place.height / ((args->editor->lines > 1ull) ? (float)args->editor->lines : 1.0f));
    const int tall = (share > mark) ? share : mark;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){left, args->place.y, strip_wide, args->place.height},
               .color = args->palette->editor_ground);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){left, args->place.y,
                                 EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                                 args->place.height},
               .color = args->palette->widget_line);
    if (args->editor->find.open)
    {
        for (unsigned long long at = 0ull; at < args->editor->matches; at += 1ull)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect = (GuiRect){left + inset,
                                         EMBED_CALL(map_strip_y, MinimapCtx, .place = args->place,
                                                    .lines = args->editor->lines, .line = args->editor->match[at][0]),
                                         strip_wide - inset, tall},
                       .color = args->palette->map_match);
        }
    }
    if (args->editor->has_cursor)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = (GuiRect){left + inset,
                                     EMBED_CALL(map_strip_y, MinimapCtx, .place = args->place,
                                                .lines = args->editor->lines, .line = args->editor->cursor.line),
                                     strip_wide - inset, mark},
                   .color = args->palette->caret);
    }
}

void gui_minimap_draw(const GuiMinimapPlaceCfg *args)
{
    const GuiPalette *palette = args->palette;
    const GuiEditor *editor = args->editor;
    int scale = args->scale;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->place))
    {
        return;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = args->place,
               .color = palette->editor_ground);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = (GuiRect){args->place.x, args->place.y,
                                 EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                                 args->place.height},
               .color = palette->line);
    if (editor->lines == 0ull)
    {
        return;
    }
    EMBED_CALL(gui_minimap_place.place, GuiMinimapPlaceCfg, .map = args->map, .place = args->place, .scale = scale,
               .editor = editor, .on_screen = args->on_screen);
    const GuiMapPlace *const at = &args->map->place;
    const int row_tall = EMBED_CALL(map_row_tall, MinimapCtx, .scale = scale);
    const int strip_wide =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_STRIP, .scale = scale);
    const int margin = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_MARGIN, .scale = scale);
    const int cell = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_CHAR, .scale = scale);
    const int bar_tall = ((row_tall * 3) / 4 > 0) ? (row_tall * 3) / 4 : 1;
    const GuiRect rows = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place,
        .other = (GuiRect){args->place.x, args->place.y, args->place.width - strip_wide, args->place.height});
    const unsigned long long first = at->start;
    const unsigned long long reach = at->start + (unsigned long long)at->fit;
    const unsigned long long end = (reach < editor->lines) ? reach : editor->lines;
    // the selection's rows, its ends in order
    const int anchored = (editor->anchor.line < editor->cursor.line) ||
                         ((editor->anchor.line == editor->cursor.line) && (editor->anchor.col < editor->cursor.col));
    const GuiPlace selected_from = anchored ? editor->anchor : editor->cursor;
    const GuiPlace selected_to = anchored ? editor->cursor : editor->anchor;
    const int selected =
        editor->has_cursor && ((selected_from.line != selected_to.line) || (selected_from.col != selected_to.col));
    if (selected && (selected_to.line >= first) && (selected_from.line < end))
    {
        const unsigned long long top_row = (selected_from.line > first) ? selected_from.line : first;
        const unsigned long long bottom_row = (selected_to.line < end - 1ull) ? selected_to.line : end - 1ull;
        // the rows are within the map's, which an int counts
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                   .rect = (GuiRect){rows.x, args->place.y + ((int)(top_row - first) * row_tall), rows.width,
                                     (int)(bottom_row - top_row + 1ull) * row_tall},
                   .color = palette->map_selection);
    }
    if (editor->find.open)
    {
        for (unsigned long long each = 0ull; each < editor->matches; each += 1ull)
        {
            const unsigned long long line = editor->match[each][0];
            if ((line >= first) && (line < end))
            {
                EMBED_CALL(
                    gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                    .rect = (GuiRect){rows.x, args->place.y + ((int)(line - first) * row_tall), rows.width, row_tall},
                    .color = palette->map_match);
            }
        }
    }
    // each line's bars, a line the map does not hold left as ground
    for (unsigned long long line = first; line < end; line += 1ull)
    {
        if ((line < args->map->from) || (line >= args->map->from + (unsigned long long)args->map->count))
        {
            continue;
        }
        const unsigned int line_index = (unsigned int)(line - args->map->from);
        const int y = args->place.y + ((int)(line - first) * row_tall);
        for (unsigned int bar = args->map->line_at[line_index]; bar < args->map->line_at[line_index + 1u]; bar += 1u)
        {
            const GuiBar *const one = &args->map->bar[bar];
            if (one->start >= GUI_MAP_WIDEST)
            {
                continue;
            }
            const unsigned int width =
                (one->start + one->width > GUI_MAP_WIDEST) ? GUI_MAP_WIDEST - one->start : one->width;
            const GuiColor color = EMBED_CALL(gui_editor_palette.color, GuiEditorPaletteCfg, .editor_read = editor,
                                              .palette = palette, .name = one->name);
            // the bars' cells are within the map's width, which an int counts
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                          .one = (GuiRect){args->place.x + margin + ((int)one->start * cell), y,
                                                           (int)width * cell, bar_tall},
                                          .other = rows),
                       .color = (color & 0x00ffffffu) | (MAP_BAR_ALPHA << 24u));
        }
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = EMBED_CALL(
                   gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){args->place.x, args->place.y + at->slider_top, args->place.width, at->slider_tall},
                   .other = args->place),
               .color = args->map->dragging ? palette->map_slider_on : palette->map_slider);
    EMBED_CALL(map_strip_draw, MinimapCtx, .raster = args->raster, .palette = palette, .editor = editor,
               .place = args->place, .scale = scale);
}

int gui_minimap_press(const GuiMinimapPlaceScaleCfg *args)
{
    int scale = args->scale;
    unsigned long long *top = args->top;
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->place, .x = args->x, .y = args->y) ||
        (args->editor->lines == 0ull))
    {
        return 0;
    }
    EMBED_CALL(gui_minimap_place.place, GuiMinimapPlaceCfg, .map = args->map, .place = args->place, .scale = scale,
               .editor = args->editor, .on_screen = args->on_screen);
    const GuiMapPlace *const at = &args->map->place;
    const int local_y = args->y - args->place.y;
    const int strip_wide =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_STRIP, .scale = scale);
    if (args->x >= args->place.x + args->place.width - strip_wide)
    {
        // the strip stands for the whole text: the part pressed is centered in the view
        const float share = (float)local_y / (float)args->place.height;
        *top = EMBED_CALL(map_line_get, MinimapCtx,
                          .value = (share * (float)args->editor->lines) - (args->on_screen / 2.0f),
                          .lines = args->editor->lines);
        return 1;
    }
    unsigned long long top_before = args->editor->top;
    if ((local_y < at->slider_top) || (local_y > at->slider_top + at->slider_tall))
    {
        const unsigned long long row =
            at->start + (unsigned long long)(local_y / EMBED_CALL(map_row_tall, MinimapCtx, .scale = scale));
        now_ms = EMBED_CALL(map_line_get, MinimapCtx, .value = (float)row - (args->on_screen / 2.0f),
                         .lines = args->editor->lines);
    }
    *top = now_ms;
    args->map->dragging = 1;
    args->map->drag_y = args->y;
    args->map->drag_top = now_ms;
    return 1;
}

int gui_minimap_drag(const GuiMinimapPlaceScaleCfg *args)
{
    unsigned long long *top = args->top;
    if (!args->map->dragging)
    {
        return 0;
    }
    const GuiMapPlace *const at = &args->map->place;
    // the slider's run down the map: the rows the map shows less the view's, in pixels
    const float shown = ((unsigned long long)at->fit < at->lines) ? (float)at->fit : (float)at->lines;
    const float run = (shown - at->on_screen) * (float)GUI_MAP_ROW;
    const float span = (run > 1.0f) ? run : 1.0f;
    const float moved = ((float)(args->y - args->map->drag_y) * at->range) / span;
    *top = EMBED_CALL(map_line_get, MinimapCtx, .value = (float)args->map->drag_top + moved, .lines = at->lines);
    return 1;
}

void gui_minimap_release(const GuiMinimapJsonCfg *args)
{
    args->map->dragging = 0;
}
