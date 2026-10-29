// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// minimap.c: the minimap part. The map's rows are the text's lines, two pixels each; the line at its first row follows
// the view's top through the text where the lines outnumber the rows, and stays put while the view does, so an edit
// that adds lines leaves the map where it stood
#include "minimap.h"

#include "../answer.h"
#include "../frame.h"

static CellOwner s_owner = MMGR_CELL_OWNER("gui_minimap");

// the share of a bar's color laid over the ground, 80%
#define MAP_BAR_ALPHA 0xccu

int gui_minimap_made(GuiMinimap *map)
{
    memor.set(&(MemoriaCfg){.dst = map, .val = 0, .bytes = sizeof(*map)});
    map->stale = 1;
    map->line_at = g_cli_cells->persistent_buf_alloc((GUI_MAP_LINES_MOST + 1u) * sizeof(*map->line_at), &s_owner);
    map->bar = g_cli_cells->persistent_buf_alloc(GUI_MAP_BARS_MOST * sizeof(*map->bar), &s_owner);
    if ((map->line_at == NULL) || (map->bar == NULL))
    {
        gui_minimap_given(map);
        return 0;
    }
    return 1;
}

void gui_minimap_given(GuiMinimap *map)
{
    g_cli_cells->persistent_buf_release(map->line_at, &s_owner);
    g_cli_cells->persistent_buf_release(map->bar, &s_owner);
    map->line_at = NULL;
    map->bar = NULL;
    map->count = 0u;
    map->bars = 0u;
}

void gui_minimap_cleared(GuiMinimap *map)
{
    map->from = 0ull;
    map->count = 0u;
    map->bars = 0u;
    map->stale = 1;
    map->held = 0;
    map->dragging = 0;
}

int gui_minimap_put(GuiMinimap *map, const CliJson *json, unsigned int root)
{
    const unsigned int lines = cli_json_member(json, root, "bars");
    if (!cli_json_is(json, root, CFG_JSON_OBJECT) || !cli_json_is(json, lines, CFG_JSON_ARRAY) || (map->line_at == NULL))
    {
        return 0;
    }
    map->from = gui_answer_member_whole(json, root, "from", 0ull);
    const unsigned int count = json->node[lines].count;
    map->count = (count > GUI_MAP_LINES_MOST) ? GUI_MAP_LINES_MOST : count;
    map->bars = 0u;
    for (unsigned int line = 0u; line < map->count; line += 1u)
    {
        map->line_at[line] = map->bars;
        const unsigned int bars = cli_json_item(json, lines, line);
        const unsigned int found = cli_json_is(json, bars, CFG_JSON_ARRAY) ? json->node[bars].count : 0u;
        for (unsigned int at = 0u; (at < found) && (map->bars < GUI_MAP_BARS_MOST); at += 1u)
        {
            const unsigned int bar = cli_json_item(json, bars, at);
            // a bar's start, width and name are each far inside what an unsigned int represents
            map->bar[map->bars].start = (unsigned int)gui_answer_whole(json, cli_json_item(json, bar, 0u), 0ull);
            map->bar[map->bars].width = (unsigned int)gui_answer_whole(json, cli_json_item(json, bar, 1u), 0ull);
            map->bar[map->bars].name = (unsigned int)gui_answer_whole(json, cli_json_item(json, bar, 2u), 0ull);
            map->bars += 1u;
        }
    }
    map->line_at[map->count] = map->bars;
    map->stale = 0;
    return 1;
}

// a row's height in pixels at `scale`, one at the least
static int map_row_tall(float scale)
{
    const int tall = gui_scaled(GUI_MAP_ROW, scale);
    return (tall > 0) ? tall : 1;
}

// `value` rounded to a line, no further than the text's first and last lines
static unsigned long long map_line_of(float value, unsigned long long lines)
{
    const unsigned long long last = (lines != 0ull) ? lines - 1ull : 0ull;
    if (value <= 0.0f)
    {
        return 0ull;
    }
    // a line count's float is not negative here and rounds to a whole number
    const unsigned long long line = (unsigned long long)(value + 0.5f);
    return (line > last) ? last : line;
}

void gui_minimap_placed(GuiMinimap *map, GuiRect place, float scale, const GuiEditor *editor, float on_screen)
{
    GuiMapPlace *const at = &map->place;
    const int row_tall = map_row_tall(scale);
    const int rows = place.height / row_tall;
    // the rows are counted where there is room for them
    at->fit = (rows > 0) ? (unsigned int)rows : 1u;
    at->lines = editor->lines;
    at->on_screen = on_screen;
    const float scrolled = (float)editor->lines - on_screen;
    at->range = (scrolled > 1.0f) ? scrolled : 1.0f;
    const unsigned long long top = editor->top;
    unsigned long long start = 0ull;
    if (editor->lines > (unsigned long long)at->fit)
    {
        const unsigned long long most = editor->lines - (unsigned long long)at->fit;
        if (map->held && (map->held_top == top))
        {
            // the view has not moved since the map last stood: the map stands where it did beside it
            const long long kept = (long long)top - map->held_offset;
            start = (kept > 0ll) ? (unsigned long long)kept : 0ull;
        }
        else
        {
            const float share = (float)top / at->range;
            start = (unsigned long long)((((share < 1.0f) ? share : 1.0f) * (float)most) + 0.5f);
        }
        start = (start > most) ? most : start;
    }
    at->start = start;
    map->held = 1;
    map->held_top = top;
    map->held_offset = (long long)top - (long long)start;
    // the slider's top is at or below the map's top, the view's top being at or below the map's first line
    at->slider_top = (int)(top - start) * row_tall;
    const int least = gui_scaled(GUI_MAP_SLIDER_LEAST, scale);
    const int tall = (int)((on_screen * (float)row_tall) + 0.5f);
    at->slider_tall = (tall > least) ? tall : least;
}

int gui_minimap_span(const GuiMinimap *map, unsigned long long *first, unsigned long long *last)
{
    const GuiMapPlace *const at = &map->place;
    if (at->lines == 0ull)
    {
        return 0;
    }
    const unsigned long long end = at->start + (unsigned long long)at->fit;
    *first = at->start;
    *last = ((end < at->lines) ? end : at->lines) - 1ull;
    return 1;
}

int gui_minimap_current(const GuiMinimap *map, unsigned long long first, unsigned long long last)
{
    return !map->stale && (map->from == first) && ((unsigned long long)map->count == last - first + 1ull);
}

// the map's pixel row of the text's `line` on the overview strip, which stands for the whole text
static int map_strip_y(GuiRect place, unsigned long long lines, unsigned long long line)
{
    const unsigned long long last = (lines != 0ull) ? lines - 1ull : 0ull;
    const unsigned long long shown = (line > last) ? last : line;
    const float size = (lines > 1ull) ? (float)lines : 1.0f;
    return place.y + (int)(((float)shown / size) * (float)place.height);
}

// the overview strip drawn down the map's right edge: the editor's ground, the widget line along its left, the matches
// and the caret's line
static void map_strip_drawn(GuiRaster *raster, const GuiPalette *palette, const GuiEditor *editor, GuiRect place,
                            float scale)
{
    const int strip_wide = gui_scaled(GUI_MAP_STRIP, scale);
    const int left = place.x + place.width - strip_wide;
    const int inset = gui_scaled(4, scale);
    const int mark = gui_scaled(GUI_MAP_MARK, scale);
    // a line's share of the strip, a mark's height at the least
    const int share = (int)((float)place.height / ((editor->lines > 1ull) ? (float)editor->lines : 1.0f));
    const int tall = (share > mark) ? share : mark;
    gui_raster_filled(raster, (GuiRect){left, place.y, strip_wide, place.height}, palette->editor_ground);
    gui_raster_filled(raster, (GuiRect){left, place.y, gui_scaled(1, scale), place.height}, palette->widget_line);
    if (editor->find.open)
    {
        for (unsigned long long at = 0ull; at < editor->matches; at += 1ull)
        {
            gui_raster_filled(raster, (GuiRect){left + inset, map_strip_y(place, editor->lines, editor->match[at][0]), strip_wide - inset, tall},
                              palette->map_match);
        }
    }
    if (editor->has_cursor)
    {
        gui_raster_filled(raster, (GuiRect){left + inset, map_strip_y(place, editor->lines, editor->cursor.line), strip_wide - inset, mark},
                          palette->caret);
    }
}

void gui_minimap_drawn(GuiMinimap *map, GuiRaster *raster, const GuiPalette *palette, const GuiEditor *editor,
                       GuiRect place, float scale, float on_screen)
{
    if (gui_rect_empty(place))
    {
        return;
    }
    gui_raster_filled(raster, place, palette->editor_ground);
    gui_raster_filled(raster, (GuiRect){place.x, place.y, gui_scaled(1, scale), place.height}, palette->line);
    if (editor->lines == 0ull)
    {
        return;
    }
    gui_minimap_placed(map, place, scale, editor, on_screen);
    const GuiMapPlace *const at = &map->place;
    const int row_tall = map_row_tall(scale);
    const int strip_wide = gui_scaled(GUI_MAP_STRIP, scale);
    const int margin = gui_scaled(GUI_MAP_MARGIN, scale);
    const int cell = gui_scaled(GUI_MAP_CHAR, scale);
    const int bar_tall = ((row_tall * 3) / 4 > 0) ? (row_tall * 3) / 4 : 1;
    const GuiRect rows = gui_rect_met(place, (GuiRect){place.x, place.y, place.width - strip_wide, place.height});
    const unsigned long long first = at->start;
    const unsigned long long reach = at->start + (unsigned long long)at->fit;
    const unsigned long long end = (reach < editor->lines) ? reach : editor->lines;
    // the selection's rows, its ends in order
    const int anchored = (editor->anchor.line < editor->cursor.line) ||
                         ((editor->anchor.line == editor->cursor.line) && (editor->anchor.col < editor->cursor.col));
    const GuiPlace selected_from = anchored ? editor->anchor : editor->cursor;
    const GuiPlace selected_to = anchored ? editor->cursor : editor->anchor;
    const int selected = editor->has_cursor && ((selected_from.line != selected_to.line) || (selected_from.col != selected_to.col));
    if (selected && (selected_to.line >= first) && (selected_from.line < end))
    {
        const unsigned long long top_row = (selected_from.line > first) ? selected_from.line : first;
        const unsigned long long bottom_row = (selected_to.line < end - 1ull) ? selected_to.line : end - 1ull;
        // the rows are within the map's, which an int counts
        gui_raster_filled(raster, (GuiRect){rows.x, place.y + ((int)(top_row - first) * row_tall), rows.width, (int)(bottom_row - top_row + 1ull) * row_tall},
                          palette->map_selection);
    }
    if (editor->find.open)
    {
        for (unsigned long long each = 0ull; each < editor->matches; each += 1ull)
        {
            const unsigned long long line = editor->match[each][0];
            if ((line >= first) && (line < end))
            {
                gui_raster_filled(raster, (GuiRect){rows.x, place.y + ((int)(line - first) * row_tall), rows.width, row_tall},
                                  palette->map_match);
            }
        }
    }
    // each line's bars, a line the map does not hold left as ground
    for (unsigned long long line = first; line < end; line += 1ull)
    {
        if ((line < map->from) || (line >= map->from + (unsigned long long)map->count))
        {
            continue;
        }
        const unsigned int held = (unsigned int)(line - map->from);
        const int y = place.y + ((int)(line - first) * row_tall);
        for (unsigned int bar = map->line_at[held]; bar < map->line_at[held + 1u]; bar += 1u)
        {
            const GuiBar *const one = &map->bar[bar];
            if (one->start >= GUI_MAP_WIDEST)
            {
                continue;
            }
            const unsigned int width = (one->start + one->width > GUI_MAP_WIDEST) ? GUI_MAP_WIDEST - one->start : one->width;
            const GuiColor color = gui_editor_color(editor, palette, one->name);
            // the bars' cells are within the map's width, which an int counts
            gui_raster_filled(raster,
                              gui_rect_met((GuiRect){place.x + margin + ((int)one->start * cell), y, (int)width * cell, bar_tall}, rows),
                              (color & 0x00ffffffu) | (MAP_BAR_ALPHA << 24u));
        }
    }
    gui_raster_filled(raster, gui_rect_met((GuiRect){place.x, place.y + at->slider_top, place.width, at->slider_tall}, place),
                      map->dragging ? palette->map_slider_on : palette->map_slider);
    map_strip_drawn(raster, palette, editor, place, scale);
}

int gui_minimap_pressed(GuiMinimap *map, GuiRect place, float scale, const GuiEditor *editor, float on_screen, int x,
                        int y, unsigned long long *top)
{
    if (!gui_rect_holds(place, x, y) || (editor->lines == 0ull))
    {
        return 0;
    }
    gui_minimap_placed(map, place, scale, editor, on_screen);
    const GuiMapPlace *const at = &map->place;
    const int local_y = y - place.y;
    const int strip_wide = gui_scaled(GUI_MAP_STRIP, scale);
    if (x >= place.x + place.width - strip_wide)
    {
        // the strip stands for the whole text: the part pressed is centered in the view
        const float share = (float)local_y / (float)place.height;
        *top = map_line_of((share * (float)editor->lines) - (on_screen / 2.0f), editor->lines);
        return 1;
    }
    unsigned long long now = editor->top;
    if ((local_y < at->slider_top) || (local_y > at->slider_top + at->slider_tall))
    {
        const unsigned long long row = at->start + (unsigned long long)(local_y / map_row_tall(scale));
        now = map_line_of((float)row - (on_screen / 2.0f), editor->lines);
    }
    *top = now;
    map->dragging = 1;
    map->drag_y = y;
    map->drag_top = now;
    return 1;
}

int gui_minimap_dragged(GuiMinimap *map, int y, unsigned long long *top)
{
    if (!map->dragging)
    {
        return 0;
    }
    const GuiMapPlace *const at = &map->place;
    // the slider's run down the map: the rows the map shows less the view's, in pixels
    const float shown = ((unsigned long long)at->fit < at->lines) ? (float)at->fit : (float)at->lines;
    const float run = (shown - at->on_screen) * (float)GUI_MAP_ROW;
    const float span = (run > 1.0f) ? run : 1.0f;
    const float moved = ((float)(y - map->drag_y) * at->range) / span;
    *top = map_line_of((float)map->drag_top + moved, at->lines);
    return 1;
}

void gui_minimap_released(GuiMinimap *map)
{
    map->dragging = 0;
}
