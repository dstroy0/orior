// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms.c: the Comms pane in the panel at the work's foot: its head's four choices, the Channels view's count and
// Read Again, and its rows as the CLI lists them, each drawn from the CLI's texts and numbers; the Requests view's
// head, rows and presses passed to its table
#include "comms.h"

#include "../../cli/src/command_line/cli_internal.h"
#include "../../cli/src/comms/comms.h"
#include "requests.h"

typedef struct
{
    const GuiPaneCfg *pane;
    unsigned int which;
} CommsPartCtx;

// the view displayed, as the CLI answers it
EMBED_INLINE unsigned int comms_view(const CommsPartCtx *args)
{
    (void)args;
    return EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u);
}

static unsigned int comms_rows(const GuiPaneCfg *args)
{
    const unsigned int view = EMBED_CALL(comms_view, CommsPartCtx, .pane = args);
    if (view == CLI_COMMS_VIEW_REQUESTS)
    {
        return gui_requests_view.rows(args);
    }
    return (view == CLI_COMMS_VIEW_CHANNELS) ? EMBED_CALL(cli_comms_pane.rows, CliCommsPaneCfg, .at = 0u) : 0u;
}

// the head: the choices' row, and under it the Channels view's count and Read Again or the Requests view's head
static int comms_head(const GuiPaneCfg *args)
{
    const unsigned int view = EMBED_CALL(comms_view, CommsPartCtx, .pane = args);
    const int under = (view == CLI_COMMS_VIEW_REQUESTS)   ? gui_requests_view.head(args)
                      : (view == CLI_COMMS_VIEW_CHANNELS) ? GUI_PANE_CHOICE
                                                          : 0;
    return GUI_PANE_CHOICE + under;
}

// the rectangle of the choice of the view `which`, a quarter of the head's first row
EMBED_INLINE GuiRect comms_choice_rect(const CommsPartCtx *args)
{
    const GuiRect rect = args->pane->rect;
    const int quarter = rect.width / (int)CLI_COMMS_VIEWS;
    // the views are four, far inside an int
    const int place = (int)args->which;
    return (GuiRect){rect.x + (place * quarter), rect.y,
                     (args->which + 1u == CLI_COMMS_VIEWS) ? rect.width - (place * quarter) : quarter,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane, .x = GUI_PANE_CHOICE)};
}

// the head under the choices' row, where the view displayed positions its own
EMBED_INLINE GuiRect comms_under_rect(const CommsPartCtx *args)
{
    const GuiRect rect = args->pane->rect;
    const int choice = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane, .x = GUI_PANE_CHOICE);
    return (GuiRect){rect.x, rect.y + choice, rect.width, rect.height - choice};
}

// Read Again's rectangle, at the right of the row under the choices
EMBED_INLINE GuiRect comms_again_rect(const CommsPartCtx *args)
{
    const GuiRect under = EMBED_CALL(comms_under_rect, CommsPartCtx, .pane = args->pane);
    const int again = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane, .x = GUI_COMMS_AGAIN);
    return (GuiRect){under.x + under.width - again, under.y, again,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane, .x = GUI_PANE_CHOICE)};
}

// the pane as the view displayed reads it: its head under the choices, and the action the pointer is on as its own
EMBED_INLINE GuiPaneCfg comms_forwarded(const CommsPartCtx *args)
{
    GuiPaneCfg forwarded = *args->pane;
    forwarded.rect = EMBED_CALL(comms_under_rect, CommsPartCtx, .pane = args->pane);
    return forwarded;
}

// the head drawn: the four choices, the view displayed lit; under them the Channels view's count and Read Again, or the
// Requests view's head
static void comms_head_draw(const GuiPaneCfg *args)
{
    const unsigned int view = EMBED_CALL(comms_view, CommsPartCtx, .pane = args);
    const unsigned int action = args->pane_read->action;
    for (unsigned int at = 0u; at < CLI_COMMS_VIEWS; at += 1u)
    {
        unsigned long long length = 0ULL;
        const char *const label =
            EMBED_CALL(cli_comms_pane.label, CliCommsPaneCfg, .at = CLI_COMMS_CHOOSE + at, .length_out = &length);
        EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args,
                   .rect = EMBED_CALL(comms_choice_rect, CommsPartCtx, .pane = args, .which = at), .text = label,
                   .length = length, .pointed = (view == at) || (action == CLI_COMMS_CHOOSE + at));
    }
    if (view == CLI_COMMS_VIEW_REQUESTS)
    {
        GuiPane own = *args->pane_read;
        own.action = (action >= CLI_COMMS_FORWARD) ? action - CLI_COMMS_FORWARD : 0u;
        GuiPaneCfg forwarded = EMBED_CALL(comms_forwarded, CommsPartCtx, .pane = args);
        forwarded.pane_read = &own;
        gui_requests_view.head_draw(&forwarded);
        return;
    }
    if (view != CLI_COMMS_VIEW_CHANNELS)
    {
        return;
    }
    const GuiRect again = EMBED_CALL(comms_again_rect, CommsPartCtx, .pane = args);
    const GuiRect under = EMBED_CALL(comms_under_rect, CommsPartCtx, .pane = args);
    unsigned long long length = 0ULL;
    const char *const summary = EMBED_CALL(cli_comms_pane.summary, CliCommsPaneCfg, .length_out = &length);
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                     .size = GUI_PANE_HEAD_SIZE,
                     .x_f32 = (float)(under.x + EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args,
                                                           .x = GUI_PANE_PAD_LEFT)),
                     .text = summary, .length = length, .color = args->palette->text_2,
                     .clip = (GuiRect){under.x, again.y, again.x - under.x, again.height});
    const char *const read_again =
        EMBED_CALL(cli_comms_pane.label, CliCommsPaneCfg, .at = CLI_COMMS_READ, .length_out = &length);
    EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args, .rect = again, .text = read_again,
               .length = length, .pointed = action == CLI_COMMS_READ);
}

// the action at x, y: a view's choice, Read Again, or the Requests view's own past CLI_COMMS_FORWARD, 0 for none
static unsigned int comms_head_hit(const GuiPaneCfg *args)
{
    for (unsigned int at = 0u; at < CLI_COMMS_VIEWS; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(comms_choice_rect, CommsPartCtx, .pane = args, .which = at), .x = args->x,
                       .y = args->y))
        {
            return CLI_COMMS_CHOOSE + at;
        }
    }
    const unsigned int view = EMBED_CALL(comms_view, CommsPartCtx, .pane = args);
    if (view == CLI_COMMS_VIEW_REQUESTS)
    {
        const GuiPaneCfg forwarded = EMBED_CALL(comms_forwarded, CommsPartCtx, .pane = args);
        const unsigned int own = gui_requests_view.head_hit(&forwarded);
        return (own != 0u) ? CLI_COMMS_FORWARD + own : 0u;
    }
    const int on_again =
        (view == CLI_COMMS_VIEW_CHANNELS) &&
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(comms_again_rect, CommsPartCtx, .pane = args), .x = args->x, .y = args->y);
    return on_again ? CLI_COMMS_READ : 0u;
}

// an action taken: a view chosen, the layer listed again, or the Requests view's own action
static void comms_action(const GuiPaneCfg *args)
{
    if ((args->which >= CLI_COMMS_CHOOSE) && (args->which < CLI_COMMS_CHOOSE + CLI_COMMS_VIEWS))
    {
        EMBED_CALL(cli_comms_pane.choose, CliCommsPaneCfg, .view = args->which - CLI_COMMS_CHOOSE);
        (void)EMBED_CALL(cli_comms_pane.step, CliCommsPaneCfg, .root = args->root, .displayed = 1, .force = 1);
        return;
    }
    if (args->which == CLI_COMMS_READ)
    {
        (void)EMBED_CALL(cli_comms_pane.step, CliCommsPaneCfg, .root = args->root, .displayed = 1, .force = 1);
        return;
    }
    if ((args->which > CLI_COMMS_FORWARD) &&
        (EMBED_CALL(comms_view, CommsPartCtx, .pane = args) == CLI_COMMS_VIEW_REQUESTS))
    {
        GuiPaneCfg forwarded = *args;
        forwarded.which = args->which - CLI_COMMS_FORWARD;
        gui_requests_view.action(&forwarded);
    }
}

// a part's or a channel's row: its dot, in the error's color where it refused a message and in signal where it did
// not, its name, its status quieter and its detail quieter still; a group's, its name and its count quieter; and a
// message's, one guide in, its sender quieter and its body
static void comms_row_draw(const GuiPaneCfg *args)
{
    if (EMBED_CALL(comms_view, CommsPartCtx, .pane = args) == CLI_COMMS_VIEW_REQUESTS)
    {
        gui_requests_view.row_draw(args);
        return;
    }
    CliCommsRow row;
    if (!EMBED_CALL(cli_comms_pane.row, CliCommsPaneCfg, .at = args->row, .row = &row))
    {
        return;
    }
    const GuiPalette *const palette = args->palette;
    const GuiRect rect = args->rect;
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_COMMS_GAP);
    int x = rect.x + pad;
    GuiColor name_color = palette->text;
    GuiColor status_color = palette->text_2;
    if (row.type == CLI_COMMS_ROW_GROUP)
    {
        name_color = palette->text_2;
        status_color = palette->text_3;
    }
    else if (row.type == CLI_COMMS_ROW_MESSAGE)
    {
        x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = args->raster, .palette = palette, .frame = args->frame,
                       .table = args->table, .rect = rect, .clip = args->clip, .depth = 1u);
        name_color = palette->text_2;
        status_color = palette->text;
    }
    else
    {
        const int dot = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_COMMS_DOT);
        const GuiColor dot_color = row.refused ? palette->error : palette->signal;
        EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = args,
                   .rect = (GuiRect){x, rect.y + ((rect.height - dot) / 2), dot, dot}, .radius = GUI_COMMS_DOT / 2,
                   .color = dot_color, .inside = dot_color);
        x += dot + EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_COMMS_DOT_GAP);
    }
    const float after_name = EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .face = GUI_FACE_HEAD,
                                        .size = GUI_PANE_ROW_SIZE, .x_f32 = (float)x, .text = row.name,
                                        .length = row.name_length, .color = name_color);
    const float after_status = EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args,
                                          .face = (row.type == CLI_COMMS_ROW_MESSAGE) ? GUI_FACE_CODE : GUI_FACE_HEAD,
                                          .size = GUI_PANE_ROW_SIZE, .x_f32 = after_name + (float)gap,
                                          .text = row.status, .length = row.status_length, .color = status_color);
    if (row.detail_length != 0ULL)
    {
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .face = GUI_FACE_HEAD,
                         .size = GUI_PANE_ROW_SIZE, .x_f32 = after_status + (float)gap, .text = row.detail,
                         .length = row.detail_length, .color = palette->text_3);
    }
}

// the channel last opened or closed, on the idle selection's ground
static int comms_current(const GuiPaneCfg *args)
{
    return (EMBED_CALL(comms_view, CommsPartCtx, .pane = args) == CLI_COMMS_VIEW_CHANNELS)
               ? EMBED_CALL(cli_comms_pane.current, CliCommsPaneCfg, .at = 0u)
               : GUI_PANE_NONE;
}

// a channel's row pressed opens its stored messages under it or closes them; a part's does nothing; the Requests
// view's rows are its own; nothing opens a file
static int comms_press(const GuiPaneCfg *args)
{
    if (EMBED_CALL(comms_view, CommsPartCtx, .pane = args) == CLI_COMMS_VIEW_REQUESTS)
    {
        return gui_requests_view.press(args);
    }
    EMBED_CALL(cli_comms_pane.toggle, CliCommsPaneCfg, .at = args->row);
    return 0;
}

// the layer listed again as the pane shows
static void comms_read(const GuiPaneCfg *args)
{
    if (args->event == GUI_PANE_DISPLAYED)
    {
        (void)EMBED_CALL(cli_comms_pane.step, CliCommsPaneCfg, .root = args->root, .displayed = 1, .force = 1);
    }
}

const GuiPaneTable gui_comms_table = {
    .tool = "comms",
    .foot = 1,
    .rows = comms_rows,
    .head = comms_head,
    .head_draw = comms_head_draw,
    .head_hit = comms_head_hit,
    .action = comms_action,
    .row_draw = comms_row_draw,
    .current = comms_current,
    .press = comms_press,
    .read = comms_read,
};
