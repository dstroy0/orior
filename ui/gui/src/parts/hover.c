// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hover.c: the hover panel. The timers run on the clock the window hands in, which lets the checks run them without
// waiting
#include "hover.h"

#include "../../../cli/src/command_line/cli_internal.h"
#include "../answer.h"
#include "../frame.h"

void gui_hover_init(const GuiHoverStateCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->hover, .val = 0, .bytes = sizeof(*args->hover)});
}

void gui_hover_free(const GuiHoverStateCfg *args)
{
    g_cli_cells->persistent_buf_release(args->hover->text, &g_gui_cells);
    args->hover->text = NULL;
    args->hover->length = 0ull;
    args->hover->shown = 0;
    args->hover->hide_deadline_ms = 0ull;
}

void gui_hover_hide(const GuiHoverStateCfg *args)
{
    gui_hover_state.free(args);
    args->hover->rect = (GuiRect){0, 0, 0, 0};
    args->hover->word = (GuiRect){0, 0, 0, 0};
}

typedef struct
{
    const GuiHover *hover;
    int x;
    int y;
} HoverCtx;
// whether the pointer at x, y stands on the word, a cell either side, or in the box
EMBED_INLINE int hover_over(const HoverCtx *args)
{
    int x = args->x;
    int y = args->y;
    return args->hover->shown &&
           (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->hover->word, .x = x, .y = y) ||
            EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->hover->rect, .x = x, .y = y));
}

void gui_hover_move(const GuiHoverInCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    int in_text = args->in_text;
    unsigned long long row = args->row;
    unsigned long long column = args->column;
    int x = args->x;
    int y = args->y;
    const int moved = !args->hover->in_text || (row != args->hover->row) || (column != args->hover->column);
    args->hover->x = x;
    args->hover->y = y;
    if (in_text)
    {
        if (moved || !args->hover->in_text)
        {
            args->hover->show_deadline_ms = now_ms + GUI_HOVER_SHOW_DELAY_MS;
            args->hover->asked = 0;
        }
        args->hover->row = row;
        args->hover->column = column;
    }
    else
    {
        args->hover->show_deadline_ms = 0ull;
        args->hover->asked = 0;
    }
    args->hover->in_text = in_text;
    if (args->hover->shown)
    {
        if (EMBED_CALL(hover_over, HoverCtx, .hover = args->hover, .x = x, .y = y))
        {
            args->hover->hide_deadline_ms = 0ull;
        }
        else if (args->hover->hide_deadline_ms == 0ull)
        {
            args->hover->hide_deadline_ms = now_ms + GUI_HOVER_HIDE_DELAY_MS;
        }
    }
}

unsigned long long gui_hover_next_deadline_ms(const GuiHoverCfg *args)
{
    const unsigned long long show = args->hover_read->show_deadline_ms;
    const unsigned long long hide = args->hover_read->hide_deadline_ms;
    if (show == 0ull)
    {
        return hide;
    }
    return ((hide != 0ull) && (hide < show)) ? hide : show;
}

int gui_hover_tick(const GuiHoverStateCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    int *ask = args->ask;
    int owed = 0;
    *ask = 0;
    if ((args->hover->hide_deadline_ms != 0ull) && (now_ms >= args->hover->hide_deadline_ms))
    {
        gui_hover_state.hide(args);
        owed = 1;
    }
    if ((args->hover->show_deadline_ms != 0ull) && (now_ms >= args->hover->show_deadline_ms))
    {
        args->hover->show_deadline_ms = 0ull;
        args->hover->asked = 1;
        args->hover->asked_row = args->hover->row;
        args->hover->asked_column = args->hover->column;
        // a pointer resting on the word shown does not ask again
        *ask = !args->hover->shown || !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                                                  .rect = args->hover->word, .x = args->hover->x, .y = args->hover->y);
    }
    return owed;
}

int gui_hover_put(const GuiHoverStateCfg *args)
{
    const CliJson *json = args->json;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->root,
                    .kind = CFG_JSON_OBJECT))
    {
        if (args->hover->shown &&
            !EMBED_CALL(hover_over, HoverCtx, .hover = args->hover, .x = args->hover->x, .y = args->hover->y))
        {
            gui_hover_state.hide(args);
        }
        return 1;
    }
    const unsigned int parts = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->root, .name = "parts");
    const unsigned int count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = parts, .kind = CFG_JSON_ARRAY)
                                   ? json->node[parts].count
                                   : 0u;
    if (count == 0u)
    {
        gui_hover_state.hide(args);
        return 1;
    }
    const unsigned long long joined_room =
        EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = json);
    char *const joined_cell = g_cli_cells->persistent_buf_alloc(joined_room, &g_gui_cells);
    VerbaLinea joined = mmgr_line_over(joined_cell, joined_room);
    if (joined.out == NULL)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const part =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = parts, .index = at),
                       .length_pointer = &length);
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
    gui_hover_state.free(args);
    args->hover->text = joined.out;
    args->hover->length = joined.at;
    const unsigned int from = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->root, .name = "from");
    const unsigned int to = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                       .object = args->root, .name = "to");
    args->hover->line = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = from,
                                   .name = "line", .otherwise = 0ull);
    args->hover->from = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = from,
                                   .name = "col", .otherwise = 0ull);
    args->hover->to = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = to,
                                 .name = "col", .otherwise = args->hover->from);
    args->hover->shown = 1;
    args->hover->hide_deadline_ms = 0ull;
    args->hover->rect = (GuiRect){0, 0, 0, 0};
    args->hover->word = (GuiRect){0, 0, 0, 0};
    return 1;
}

int gui_hover_holds(const GuiHoverXCfg *args)
{
    return args->hover_read->shown && EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                                                 .rect = args->hover_read->rect, .x = args->x, .y = args->y);
}

void gui_hover_draw(const GuiHoverRasterCfg *args)
{
    int scale = args->scale;
    if (!args->hover->shown)
    {
        return;
    }
    const int near = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HOVER_NEAR, .scale = scale) *
                     args->anchor.width;
    args->hover->word = (GuiRect){args->anchor.x - near, args->anchor.y,
                                  (args->last.x + args->last.width) - args->anchor.x + (2 * near), args->anchor.height};
    const int pad_x = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HOVER_PAD_X, .scale = scale);
    const int pad_y = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HOVER_PAD_Y, .scale = scale);
    int wide = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HOVER_WIDE, .scale = scale);
    wide = (wide > args->bounds.width) ? args->bounds.width : wide;
    const int inner = wide - (2 * pad_x);
    const int measured =
        EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = NULL, .glyphs = args->glyphs,
                   .palette = args->palette, .text = args->hover->text, .length = args->hover->length, .x = 0, .y = 0,
                   .wide = inner, .scale = scale, .clip = args->bounds);
    int tall = measured + (2 * pad_y);
    const int most = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_HOVER_TALL, .scale = scale);
    tall = (tall > most) ? most : tall;
    args->hover->rect = EMBED_CALL(gui_panel_wide.place, GuiPanelWideCfg, .anchor = args->anchor, .wide = wide,
                                   .tall = tall, .back = 0, .bounds = args->bounds, .above = 0, .scale = scale);
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = args->raster, .palette = args->palette,
               .rect = args->hover->rect, .scale = scale);
    const GuiRect clip = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg,
        .one = (GuiRect){args->hover->rect.x + pad_x, args->hover->rect.y + pad_y, inner, tall - (2 * pad_y)},
        .other = args->bounds);
    (void)EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = args->raster, .glyphs = args->glyphs,
                     .palette = args->palette, .text = args->hover->text, .length = args->hover->length,
                     .x = args->hover->rect.x + pad_x, .y = args->hover->rect.y + pad_y, .wide = inner, .scale = scale,
                     .clip = clip);
}
