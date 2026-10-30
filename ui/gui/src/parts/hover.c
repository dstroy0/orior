// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hover.c: the hover panel. The timers run on the clock the window hands in, which lets the checks run them without
// waiting
#include "hover.h"

#include "../../../cli/src/command_line/cli_internal.h"
#include "../answer.h"
#include "../frame.h"

void gui_hover_made(GuiHover *hover)
{
    memor.set(&(MemoriaCfg){.dst = hover, .val = 0, .bytes = sizeof(*hover)});
}

void gui_hover_given(GuiHover *hover)
{
    g_cli_cells->persistent_buf_release(hover->text, &g_gui_cells);
    hover->text = NULL;
    hover->length = 0ull;
    hover->shown = 0;
}

void gui_hover_hidden(GuiHover *hover)
{
    gui_hover_given(hover);
    hover->left_ms = 0ull;
    hover->rect = (GuiRect){0, 0, 0, 0};
    hover->word = (GuiRect){0, 0, 0, 0};
}

// whether the pointer at x, y stands on the word, a cell either side, or in the box
static int hover_over(const GuiHover *hover, int x, int y)
{
    return hover->shown && (gui_rect_holds(hover->word, x, y) || gui_rect_holds(hover->rect, x, y));
}

void gui_hover_moved(GuiHover *hover, int in_text, unsigned long long row, unsigned long long column, int x, int y,
                     unsigned long long now)
{
    const int moved = !hover->in_text || (row != hover->row) || (column != hover->column);
    hover->x = x;
    hover->y = y;
    if (in_text)
    {
        if (moved || !hover->in_text)
        {
            hover->moved_ms = now;
            hover->asked = 0;
        }
        hover->row = row;
        hover->column = column;
    }
    else
    {
        hover->moved_ms = 0ull;
        hover->asked = 0;
    }
    hover->in_text = in_text;
    if (hover->shown)
    {
        if (hover_over(hover, x, y))
        {
            hover->left_ms = 0ull;
        }
        else if (hover->left_ms == 0ull)
        {
            hover->left_ms = now;
        }
    }
}

unsigned long long gui_hover_due(const GuiHover *hover, unsigned long long now)
{
    unsigned long long due = 0ull;
    if (hover->in_text && !hover->asked && (hover->moved_ms != 0ull))
    {
        const unsigned long long passed = now - hover->moved_ms;
        due = (passed < GUI_HOVER_RESTS) ? GUI_HOVER_RESTS - passed : 1ull;
    }
    if (hover->shown && (hover->left_ms != 0ull))
    {
        const unsigned long long passed = now - hover->left_ms;
        const unsigned long long linger = (passed < GUI_HOVER_LINGER) ? GUI_HOVER_LINGER - passed : 1ull;
        due = ((due == 0ull) || (linger < due)) ? linger : due;
    }
    return due;
}

int gui_hover_ticked(GuiHover *hover, unsigned long long now, int *ask)
{
    int owed = 0;
    *ask = 0;
    if (hover->shown && (hover->left_ms != 0ull) && (now - hover->left_ms >= GUI_HOVER_LINGER))
    {
        gui_hover_hidden(hover);
        owed = 1;
    }
    if (hover->in_text && !hover->asked && (hover->moved_ms != 0ull) && (now - hover->moved_ms >= GUI_HOVER_RESTS))
    {
        hover->asked = 1;
        hover->asked_row = hover->row;
        hover->asked_column = hover->column;
        // a pointer resting on the word shown does not ask again
        *ask = !hover->shown || !gui_rect_holds(hover->word, hover->x, hover->y);
    }
    return owed;
}

int gui_hover_put(GuiHover *hover, const CliJson *json, unsigned int root)
{
    if (!cli_json_is(json, root, CFG_JSON_OBJECT))
    {
        if (hover->shown && !hover_over(hover, hover->x, hover->y))
        {
            gui_hover_hidden(hover);
        }
        return 1;
    }
    const unsigned int parts = cli_json_member(json, root, "parts");
    const unsigned int count = cli_json_is(json, parts, CFG_JSON_ARRAY) ? json->node[parts].count : 0u;
    if (count == 0u)
    {
        gui_hover_hidden(hover);
        return 1;
    }
    VerbaLinea joined;
    if (!cli_growing_line_made(&joined, GUI_HOVER_FIRST, &g_gui_cells))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const part = cli_json_string(json, cli_json_item(json, parts, at), &length);
        if (part == NULL)
        {
            continue;
        }
        if (at > 0u)
        {
            mmgr_line_put(&joined, "\n---\n");
        }
        mmgr_line_put_n(&joined, part, length);
    }
    if (!mmgr_line_ok(&joined))
    {
        g_cli_cells->persistent_buf_release(joined.out, &g_gui_cells);
        return 0;
    }
    gui_hover_given(hover);
    hover->text = joined.out;
    hover->length = joined.at;
    const unsigned int from = cli_json_member(json, root, "from");
    const unsigned int to = cli_json_member(json, root, "to");
    hover->line = gui_answer_member_whole(json, from, "line", 0ull);
    hover->from = gui_answer_member_whole(json, from, "col", 0ull);
    hover->to = gui_answer_member_whole(json, to, "col", hover->from);
    hover->shown = 1;
    hover->left_ms = 0ull;
    hover->rect = (GuiRect){0, 0, 0, 0};
    hover->word = (GuiRect){0, 0, 0, 0};
    return 1;
}

int gui_hover_holds(const GuiHover *hover, int x, int y)
{
    return hover->shown && gui_rect_holds(hover->rect, x, y);
}

void gui_hover_drawn(GuiHover *hover, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette, GuiRect anchor,
                     GuiRect last, GuiRect bounds, float scale)
{
    if (!hover->shown)
    {
        return;
    }
    const int near = gui_scaled(GUI_HOVER_NEAR, scale) * anchor.width;
    hover->word = (GuiRect){anchor.x - near, anchor.y, (last.x + last.width) - anchor.x + (2 * near), anchor.height};
    const int pad_x = gui_scaled(GUI_HOVER_PAD_X, scale);
    const int pad_y = gui_scaled(GUI_HOVER_PAD_Y, scale);
    int wide = gui_scaled(GUI_HOVER_WIDE, scale);
    wide = (wide > bounds.width) ? bounds.width : wide;
    const int inner = wide - (2 * pad_x);
    const int measured =
        gui_panel_markdown(NULL, glyphs, palette, hover->text, hover->length, 0, 0, inner, scale, bounds);
    int tall = measured + (2 * pad_y);
    const int most = gui_scaled(GUI_HOVER_TALL, scale);
    tall = (tall > most) ? most : tall;
    hover->rect = gui_panel_placed(anchor, wide, tall, 0, bounds, 0, scale);
    gui_panel_box_drawn(raster, palette, hover->rect, scale);
    const GuiRect clip =
        gui_rect_met((GuiRect){hover->rect.x + pad_x, hover->rect.y + pad_y, inner, tall - (2 * pad_y)}, bounds);
    (void)gui_panel_markdown(raster, glyphs, palette, hover->text, hover->length, hover->rect.x + pad_x,
                             hover->rect.y + pad_y, inner, scale, clip);
}
