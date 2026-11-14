// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug.c: the Debug pane at the work's foot: its head, the bar's steps with the status line, the views' choices and
// the field, and its rows, each positioned from what the debug module answers and nothing composed here
#include "debug.h"

#include "../../../includes/window/icons.h"
#include "../../cli/src/debug/debug_pane.h"

typedef struct
{
    const GuiPaneCfg *pane;
    GuiRect rect;
    unsigned int at;
    int x;
    const CliDebugPaneRow *row;
    GuiColor color;
} DebugCtx;

// the bar's row at the head's top, the views' row under it, and the field's row under that
EMBED_INLINE GuiRect debug_bar_rect(const DebugCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    return (GuiRect){pane->rect.x, pane->rect.y, pane->rect.width,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_CHOICE)};
}

EMBED_INLINE GuiRect debug_views_rect(const DebugCtx *args)
{
    const GuiRect bar = EMBED_CALL(debug_bar_rect, DebugCtx, .pane = args->pane);
    return (GuiRect){bar.x, bar.y + bar.height, bar.width, bar.height};
}

EMBED_INLINE GuiRect debug_field_rect(const DebugCtx *args)
{
    const GuiRect views = EMBED_CALL(debug_views_rect, DebugCtx, .pane = args->pane);
    return EMBED_CALL(gui_pane_field.field_rect, GuiPaneFieldCfg, .pane = args->pane, .y = views.y + views.height);
}

// the box of the bar's step `at`, after the boxes of the steps displayed before it; an empty box for a step not displayed
EMBED_INLINE GuiRect debug_step_rect(const DebugCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiRect bar = EMBED_CALL(debug_bar_rect, DebugCtx, .pane = pane);
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT);
    const int step = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_DEBUG_STEP);
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_DEBUG_STEP_GAP);
    const int one = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = 1);
    int before = 0;
    for (unsigned int each = 0u; each <= args->at; each += 1u)
    {
        CliDebugBarItem item;
        if (!EMBED_CALL(cli_debug_pane_bar.item, CliDebugPaneBarCfg, .at = each, .item = &item) || !item.displayed)
        {
            if (each == args->at)
            {
                return (GuiRect){bar.x, bar.y, 0, 0};
            }
            continue;
        }
        before += (each < args->at) ? 1 : 0;
    }
    return (GuiRect){bar.x + pad + (before * (step + gap)), bar.y + one, step, bar.height - (2 * one)};
}

// the choice of the view `at` in the views' row
EMBED_INLINE GuiRect debug_view_rect(const DebugCtx *args)
{
    const GuiRect views = EMBED_CALL(debug_views_rect, DebugCtx, .pane = args->pane);
    const int width = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane, .x = GUI_DEBUG_VIEW);
    // the views are three, far inside an int
    return (GuiRect){views.x + ((int)args->at * width), views.y, width, views.height};
}

// the head's rows: the bar, the views' choices and the field
static int debug_head(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_PANE_CHOICE + GUI_PANE_CHOICE + GUI_PANE_BOX;
}

// the bar's step `at` drawn in its box: on the hover's ground where the pointer is on it, its icon where the icons
// carry one and else its label, quieter where it cannot act
EMBED_INLINE void debug_step_draw(const DebugCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiPalette *const palette = pane->palette;
    CliDebugBarItem item;
    if (!EMBED_CALL(cli_debug_pane_bar.item, CliDebugPaneBarCfg, .at = args->at, .item = &item) || !item.displayed)
    {
        return;
    }
    const GuiRect box = EMBED_CALL(debug_step_rect, DebugCtx, .pane = pane, .at = args->at);
    const int pointed = pane->pane_read->action == (GUI_DEBUG_STEP_FIRST + args->at);
    EMBED_CALL(gui_pane_field.button_draw, GuiPaneFieldCfg, .pane = pane, .rect = box, .radius = GUI_PANE_RADIUS,
               .color = pointed ? palette->hover : palette->line);
    const GuiColor color = item.enabled ? palette->text_2 : palette->text_3;
    // the icon's side scaled is a positive count of pixels
    const unsigned int icon =
        (unsigned int)EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_DEBUG_STEP_ICON);
    const GuiMask *const mask_bits =
        (pane->icons != NULL) ? EMBED_CALL(gui_icons.icon, GuiIconsCfg, .icons = pane->icons, .name = item.glyph,
                                           .size = icon, .stroke = GUI_ICON_STROKE * (float)icon / GUI_ICON_UNITS)
                              : NULL;
    if (mask_bits != NULL)
    {
        // the icon's side is far inside what an int represents
        EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = pane->raster, .mask_bits = mask_bits,
                   .x = box.x + ((box.width - (int)icon) / 2), .y = box.y + ((box.height - (int)icon) / 2),
                   .color = GUI_FRAME_FADED(color, EMBED_CALL(gui_pane_part.share, GuiPanePartCfg, .pane = pane)),
                   .clip = pane->clip);
        return;
    }
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = pane, .clipped = 1, .face = GUI_FACE_HEAD,
                     .size = GUI_PANE_CHOICE_SIZE,
                     .x_f32 = (float)(box.x + EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane,
                                                         .x = GUI_PANE_FIELD_PAD)),
                     .text = item.label, .length = item.label_length, .color = color, .clip = box);
}

// the head drawn: the bar's steps, the status line after the last displayed, the views' choices with the one displayed lit,
// and the field of the view displayed with its text or its placeholder
static void debug_head_draw(const GuiPaneCfg *args)
{
    const GuiPalette *const palette = args->palette;
    GuiRect last = {0, 0, 0, 0};
    for (unsigned int at = 0u; at < CLI_DEBUG_STEPS; at += 1u)
    {
        EMBED_CALL(debug_step_draw, DebugCtx, .pane = args, .at = at);
        const GuiRect box = EMBED_CALL(debug_step_rect, DebugCtx, .pane = args, .at = at);
        last = (box.width > 0) ? box : last;
    }
    CliDebugPaneStatus status;
    EMBED_CALL(cli_debug_pane_bar.status, CliDebugPaneBarCfg, .status_out = &status);
    const GuiRect bar = EMBED_CALL(debug_bar_rect, DebugCtx, .pane = args);
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    const int status_x = last.x + last.width + pad;
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                     .size = GUI_DEBUG_STATUS_SIZE, .x_f32 = (float)status_x, .text = status.text,
                     .length = status.length, .color = status.failed ? palette->mark[0] : palette->text_3,
                     .clip = (GuiRect){status_x, bar.y, bar.x + bar.width - status_x, bar.height});
    const unsigned int displayed = EMBED_CALL(cli_debug_pane_view.view, CliDebugPaneViewCfg, .at = 0u);
    for (unsigned int view = 0u; view < CLI_DEBUG_VIEWS; view += 1u)
    {
        unsigned long long length = 0ULL;
        const char *const label =
            EMBED_CALL(cli_debug_pane_view.label, CliDebugPaneViewCfg, .view = view, .length_out = &length);
        EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args,
                   .rect = EMBED_CALL(debug_view_rect, DebugCtx, .pane = args, .at = view), .text = label,
                   .length = length,
                   .pointed = (view == displayed) || (args->pane_read->action == (GUI_DEBUG_VIEW_FIRST + view)));
    }
    CliDebugPaneField field;
    EMBED_CALL(cli_debug_pane_input.field, CliDebugPaneInputCfg, .field_out = &field);
    if (field.displayed)
    {
        const GuiRect rect = EMBED_CALL(debug_field_rect, DebugCtx, .pane = args);
        EMBED_CALL(gui_pane_field.field_draw, GuiPaneFieldCfg, .pane = args, .rect = rect, .right = rect.x + rect.width,
                   .text = field.text, .length = field.length, .placeholder = field.placeholder,
                   .keys = args->pane_read->keys);
    }
}

// the action at x, y: the field where the view displayed has one and the pixel is in it, a view's choice, or a step
// displayed
static unsigned int debug_head_hit(const GuiPaneCfg *args)
{
    CliDebugPaneField field;
    EMBED_CALL(cli_debug_pane_input.field, CliDebugPaneInputCfg, .field_out = &field);
    if (field.displayed &&
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(debug_field_rect, DebugCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return GUI_DEBUG_FIELD;
    }
    for (unsigned int view = 0u; view < CLI_DEBUG_VIEWS; view += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(debug_view_rect, DebugCtx, .pane = args, .at = view), .x = args->x,
                       .y = args->y))
        {
            return GUI_DEBUG_VIEW_FIRST + view;
        }
    }
    for (unsigned int at = 0u; at < CLI_DEBUG_STEPS; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(debug_step_rect, DebugCtx, .pane = args, .at = at), .x = args->x,
                       .y = args->y))
        {
            return GUI_DEBUG_STEP_FIRST + at;
        }
    }
    return 0u;
}

// an action taken: the field takes the keys, a choice shows its view, and a step runs
static void debug_action(const GuiPaneCfg *args)
{
    if (args->which == GUI_DEBUG_FIELD)
    {
        if (args->pane != NULL)
        {
            args->pane->keys = 1;
        }
        return;
    }
    if ((args->which >= GUI_DEBUG_VIEW_FIRST) && (args->which < GUI_DEBUG_VIEW_FIRST + CLI_DEBUG_VIEWS))
    {
        EMBED_CALL(cli_debug_pane_view.view_set, CliDebugPaneViewCfg, .view = args->which - GUI_DEBUG_VIEW_FIRST);
        return;
    }
    if ((args->which >= GUI_DEBUG_STEP_FIRST) && (args->which < GUI_DEBUG_STEP_FIRST + CLI_DEBUG_STEPS))
    {
        (void)EMBED_CALL(cli_debug_pane_bar.run, CliDebugPaneBarCfg, .at = args->which - GUI_DEBUG_STEP_FIRST,
                         .root = args->root);
    }
}

static unsigned int debug_rows(const GuiPaneCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_debug_pane_view.rows, CliDebugPaneViewCfg,
                      .view = EMBED_CALL(cli_debug_pane_view.view, CliDebugPaneViewCfg, .at = 0u));
}

// `length` bytes of `text` drawn from x in `color`, cut to the row: the x after them
EMBED_INLINE float debug_text_draw(const DebugCtx *args)
{
    const CliDebugPaneRow *const row = args->row;
    return EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args->pane, .clipped = 1, .face = GUI_FACE_CODE,
                      .size = GUI_DEBUG_ROW_SIZE, .x_f32 = (float)args->x,
                      .text = (args->at == 0u) ? row->text : ((args->at == 1u) ? row->second : row->third),
                      .length = (args->at == 0u) ? row->text_length
                                                 : ((args->at == 1u) ? row->second_length : row->third_length),
                      .color = args->color, .clip = args->rect);
}

// the third text of the row, or its second where `at` is 1, drawn quieter against the row's right padding
EMBED_INLINE void debug_right_draw(const DebugCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const CliDebugPaneRow *const row = args->row;
    const unsigned long long length = (args->at == 1u) ? row->second_length : row->third_length;
    if (length == 0ULL)
    {
        return;
    }
    const float width =
        EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = pane, .face = GUI_FACE_CODE,
                   .size = GUI_DEBUG_ROW_SIZE, .text = (args->at == 1u) ? row->second : row->third, .length = length);
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT);
    // the text's width is far inside what an int represents
    const int x = pane->rect.x + pane->rect.width - pad - (int)width;
    (void)EMBED_CALL(debug_text_draw, DebugCtx, .pane = pane, .row = row, .at = args->at, .x = x,
                     .color = pane->palette->text_3, .rect = pane->clip);
}

// the arrow of a row that opens, at x, down where its children are open
EMBED_INLINE void debug_arrow_draw(const DebugCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const int twisty = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_GUIDE);
    const int arrow = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_ARROW);
    EMBED_CALL(
        gui_pane.arrow_draw, GuiPaneCfg, .raster = pane->raster, .x = args->x + ((twisty - arrow) / 2),
        .y = pane->rect.y + ((pane->rect.height - arrow) / 2), .side = arrow, .open = args->row->open,
        .color = GUI_FRAME_FADED(pane->palette->text_2, EMBED_CALL(gui_pane_part.share, GuiPanePartCfg, .pane = pane)),
        .clip = pane->clip);
}

// the row drawn as its type
static void debug_row_draw(const GuiPaneCfg *args)
{
    const GuiPalette *const palette = args->palette;
    CliDebugPaneRow row;
    if (!EMBED_CALL(cli_debug_pane_view.row, CliDebugPaneViewCfg,
                    .view = EMBED_CALL(cli_debug_pane_view.view, CliDebugPaneViewCfg, .at = 0u), .at = args->row,
                    .row = &row))
    {
        return;
    }
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    const int twisty = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_GUIDE);
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_DEBUG_GAP);
    const int left = args->rect.x + pad;
    switch (row.type)
    {
    case CLI_DEBUG_ROW_NOTICE:
        (void)EMBED_CALL(debug_text_draw, DebugCtx, .pane = args, .row = &row, .at = 0u, .x = left,
                         .color = palette->text_3, .rect = args->clip);
        return;
    case CLI_DEBUG_ROW_FRAME: {
        // the chosen frame marked by the arrow before its function
        if (row.current)
        {
            row.open = 0;
            EMBED_CALL(debug_arrow_draw, DebugCtx, .pane = args, .row = &row, .x = left);
        }
        (void)EMBED_CALL(debug_text_draw, DebugCtx, .pane = args, .row = &row, .at = 0u, .x = left + twisty,
                         .color = palette->text, .rect = args->clip);
        EMBED_CALL(debug_right_draw, DebugCtx, .pane = args, .row = &row, .at = 1u);
        return;
    }
    case CLI_DEBUG_ROW_SCOPE:
    case CLI_DEBUG_ROW_VARIABLE: {
        const int x =
            EMBED_CALL(gui_pane.guides, GuiPaneCfg, .table = args->table, .raster = args->raster, .palette = palette,
                       .frame = args->frame, .rect = args->rect, .clip = args->clip, .depth = row.depth);
        if (row.opens)
        {
            EMBED_CALL(debug_arrow_draw, DebugCtx, .pane = args, .row = &row, .x = x);
        }
        const float after = EMBED_CALL(debug_text_draw, DebugCtx, .pane = args, .row = &row, .at = 0u, .x = x + twisty,
                                       .color = (row.type == CLI_DEBUG_ROW_SCOPE) ? palette->text_2 : palette->text,
                                       .rect = args->clip);
        // the value after the name, its pixel the next whole one
        (void)EMBED_CALL(debug_text_draw, DebugCtx, .pane = args, .row = &row, .at = 1u, .x = (int)after + gap,
                         .color = palette->text_2, .rect = args->clip);
        EMBED_CALL(debug_right_draw, DebugCtx, .pane = args, .row = &row, .at = 2u);
        return;
    }
    case CLI_DEBUG_ROW_WATCH: {
        const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_DEBUG_BOX);
        EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = args,
                   .rect = (GuiRect){left, args->rect.y + ((args->rect.height - side) / 2), side, side},
                   .radius = GUI_PANE_RADIUS / 2, .color = palette->line, .inside = palette->editor_background);
        const float after = EMBED_CALL(debug_text_draw, DebugCtx, .pane = args, .row = &row, .at = 0u,
                                       .x = left + twisty, .color = palette->text, .rect = args->clip);
        // the value after the expression, its pixel the next whole one
        (void)EMBED_CALL(debug_text_draw, DebugCtx, .pane = args, .row = &row, .at = 1u, .x = (int)after + gap,
                         .color = row.failed ? palette->mark[0] : palette->text_2, .rect = args->clip);
        EMBED_CALL(debug_right_draw, DebugCtx, .pane = args, .row = &row, .at = 2u);
        return;
    }
    default: {
        GuiColor color = palette->text;
        switch (row.line_type)
        {
        case CLI_DEBUG_LINE_STDERR:
            color = palette->mark[0];
            break;
        case CLI_DEBUG_LINE_CONSOLE:
        case CLI_DEBUG_LINE_INPUT:
            color = palette->text_2;
            break;
        case CLI_DEBUG_LINE_RESULT:
            color = palette->mark[1];
            break;
        default:
            color = palette->text;
            break;
        }
        (void)EMBED_CALL(debug_text_draw, DebugCtx, .pane = args, .row = &row, .at = 0u, .x = left, .color = color,
                         .rect = args->clip);
        return;
    }
    }
}

// the row pressed: a frame chosen, its file opened where its path is in the tree; where the frame is known, a scope's
// or a variable's arrow opens or closes it and a watch's box takes the watch out
static int debug_press(const GuiPaneCfg *args)
{
    const unsigned int view = EMBED_CALL(cli_debug_pane_view.view, CliDebugPaneViewCfg, .at = 0u);
    CliDebugPaneRow row;
    if (!EMBED_CALL(cli_debug_pane_view.row, CliDebugPaneViewCfg, .view = view, .at = args->row, .row = &row))
    {
        return 0;
    }
    if (view == CLI_DEBUG_VIEW_FRAMES)
    {
        CliDebugPaneOpen open = {NULL, 0u, 0};
        if (!EMBED_CALL(cli_debug_pane_view.press, CliDebugPaneViewCfg, .view = view, .at = args->row,
                        .open_out = &open, .root = args->root))
        {
            return 0;
        }
        *args->open_out = (GuiPaneOpen){open.path, {open.line, 0ULL}, open.placed};
        return 1;
    }
    if ((view != CLI_DEBUG_VIEW_VARIABLES) || (args->frame == NULL))
    {
        return 0;
    }
    const GuiRect rect = EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = args->table, .frame = args->frame,
                                    .pane_read = args->pane_read, .row = args->row);
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    const int guide = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_GUIDE);
    // the depth is at max CLI_DEBUG_DEPTH guides, far inside an int; the arrow and a watch's box are in the guide
    // after the row's own
    const int reach = rect.x + pad + (((int)row.depth + 1) * guide);
    if ((args->x < reach) && (row.opens || (row.type == CLI_DEBUG_ROW_WATCH)))
    {
        (void)EMBED_CALL(cli_debug_pane_view.toggle, CliDebugPaneViewCfg, .view = view, .at = args->row,
                         .root = args->root);
    }
    return 0;
}

// text typed goes into the field of the view displayed
static int debug_text(const GuiPaneCfg *args)
{
    EMBED_CALL(cli_debug_pane_input.type, CliDebugPaneInputCfg, .text = args->text, .size = args->length);
    return 1;
}

// Escape lets the keys go; Enter and Backspace go to the field
static int debug_key(const GuiPaneCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        args->pane->keys = 0;
        return 1;
    }
    return EMBED_CALL(cli_debug_pane_input.key, CliDebugPaneInputCfg, .name = args->name, .root = args->root);
}

const GuiPaneTable gui_debug_table = {
    .tool = "debug",
    .foot = 1,
    .rows = debug_rows,
    .head = debug_head,
    .head_draw = debug_head_draw,
    .head_hit = debug_head_hit,
    .action = debug_action,
    .row_draw = debug_row_draw,
    .press = debug_press,
    .text = debug_text,
    .key = debug_key,
};
