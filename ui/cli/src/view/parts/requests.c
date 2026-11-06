// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests.c: the Requests view of the Comms pane: its head's choices and its rows as the CLI lays them, each drawn
// from the CLI's texts and numbers, and its presses handed to the CLI
#include "requests.h"

#include "../../command_line/cli_internal.h"
#include "../../requests/requests.h"

typedef struct
{
    const GuiPaneCfg *pane;
    unsigned int which;
    unsigned int tone;
    const CliRequestsRow *row;
} RequestsPartCtx;

// the head: Send and Copy as curl in one row, the environments in the next
static int requests_head(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_PANE_CHOICE + GUI_PANE_CHOICE;
}

static unsigned int requests_rows(const GuiPaneCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u);
}

// the rectangle of the head's choice `which`: Send and Copy as curl each half the first row, the environments sharing
// the second
EMBED_INLINE GuiRect requests_choice_rect(const RequestsPartCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiRect rect = pane->rect;
    const int row_height = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_CHOICE);
    if (args->which < CLI_REQUESTS_CHOICE_ENV)
    {
        const int half = rect.width / 2;
        return (args->which == CLI_REQUESTS_CHOICE_SEND)
                   ? (GuiRect){rect.x, rect.y, half, row_height}
                   : (GuiRect){rect.x + half, rect.y, rect.width - half, row_height};
    }
    const unsigned int choices = EMBED_CALL(cli_requests_pane.choices, CliRequestsPaneCfg, .at = 0u);
    // the environments are few, far inside an int
    const int envs = (int)(choices - CLI_REQUESTS_CHOICE_ENV);
    const int place = (int)(args->which - CLI_REQUESTS_CHOICE_ENV);
    const int part = (envs > 0) ? rect.width / envs : rect.width;
    return (GuiRect){rect.x + (place * part), rect.y + row_height,
                     (place + 1 == envs) ? rect.width - (place * part) : part, row_height};
}

// the head drawn: each choice the CLI answers, on the hover's ground where the pointer stands on it or it is lit
static void requests_head_draw(const GuiPaneCfg *args)
{
    const unsigned int choices = EMBED_CALL(cli_requests_pane.choices, CliRequestsPaneCfg, .at = 0u);
    for (unsigned int at = 0u; at < choices; at += 1u)
    {
        CliRequestsRow choice;
        if (!EMBED_CALL(cli_requests_pane.choice, CliRequestsPaneCfg, .at = at, .row = &choice))
        {
            continue;
        }
        EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args,
                   .rect = EMBED_CALL(requests_choice_rect, RequestsPartCtx, .pane = args, .which = at),
                   .text = choice.text, .length = choice.text_length,
                   .pointed = choice.open || (args->pane_read->action == at + 1u));
    }
}

// the action at x, y: the head's choice it stands on, its place and one, 0 for none
static unsigned int requests_head_hit(const GuiPaneCfg *args)
{
    const unsigned int choices = EMBED_CALL(cli_requests_pane.choices, CliRequestsPaneCfg, .at = 0u);
    for (unsigned int at = 0u; at < choices; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(requests_choice_rect, RequestsPartCtx, .pane = args, .which = at),
                       .x = args->x, .y = args->y))
        {
            return at + 1u;
        }
    }
    return 0u;
}

// a choice taken: Send sends the request the shown view's cursor stands in, Copy as curl puts its curl line on the
// clipboard, and an environment is chosen
static void requests_action(const GuiPaneCfg *args)
{
    if (args->which == 0u)
    {
        return;
    }
    const unsigned int choice = args->which - 1u;
    if (choice == CLI_REQUESTS_CHOICE_SEND)
    {
        (void)EMBED_CALL(cli_requests_pane.send, CliRequestsPaneCfg, .root = args->root);
        return;
    }
    if (choice == CLI_REQUESTS_CHOICE_CURL)
    {
        (void)EMBED_CALL(cli_requests_pane.copy, CliRequestsPaneCfg, .root = args->root);
        return;
    }
    EMBED_CALL(cli_requests_pane.env, CliRequestsPaneCfg, .at = choice);
}

// the color of a row's `tone`
EMBED_INLINE GuiColor requests_tone_color(const RequestsPartCtx *args)
{
    const GuiPalette *const palette = args->pane->palette;
    switch (args->tone)
    {
    case CLI_REQUESTS_TONE_GOOD:
        return palette->signal;
    case CLI_REQUESTS_TONE_MOVED:
        return palette->warning;
    case CLI_REQUESTS_TONE_FAILED:
        return palette->error;
    case CLI_REQUESTS_TONE_QUIET:
        return palette->text_3;
    default:
        return palette->text;
    }
}

// the x an answer's box ends at, from the row's left padding
EMBED_INLINE int requests_box_end(const RequestsPartCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    return EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT) +
           EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_REQUESTS_BOX) +
           EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_REQUESTS_BOX_GAP);
}

// the row drawn as its kind: a group with its arrow, a header or what was sent one guide in, the body and a
// comparison's lines in the code face, a comparison's with its sign first, an answer with its box, and any other in
// its tone's color
static void requests_row_draw(const GuiPaneCfg *args)
{
    CliRequestsRow row;
    if (!EMBED_CALL(cli_requests_pane.row, CliRequestsPaneCfg, .at = args->row, .row = &row))
    {
        return;
    }
    const GuiPalette *const palette = args->palette;
    const GuiRect rect = args->rect;
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    const int share = EMBED_CALL(gui_pane_part.share, GuiPanePartCfg, .pane = args);
    int x = rect.x + pad;
    unsigned int face = GUI_FACE_HEAD;
    GuiColor color = EMBED_CALL(requests_tone_color, RequestsPartCtx, .pane = args, .tone = row.tone);
    switch (row.kind)
    {
    case CLI_REQUESTS_ROW_GROUP: {
        const int arrow = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_ARROW);
        EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = args->raster, .x = x,
                   .y = rect.y + ((rect.height - arrow) / 2), .side = arrow, .open = row.open,
                   .color = GUI_FRAME_FADED(palette->text_2, share), .clip = args->clip);
        x += arrow + pad;
        color = palette->text_2;
        break;
    }
    case CLI_REQUESTS_ROW_HEADER:
    case CLI_REQUESTS_ROW_SENT:
        x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = args->raster, .palette = palette, .frame = args->frame,
                       .table = args->table, .rect = rect, .clip = args->clip, .depth = 1u);
        face = GUI_FACE_CODE;
        color = palette->text_2;
        break;
    case CLI_REQUESTS_ROW_BODY:
        face = GUI_FACE_CODE;
        break;
    case CLI_REQUESTS_ROW_SIGN: {
        face = GUI_FACE_CODE;
        const char sign[1] = {row.sign};
        // the x after the sign stands inside the row, far inside what an int represents
        x = (int)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .face = GUI_FACE_CODE_BOLD,
                            .size = GUI_REQUESTS_SIZE, .x_f32 = (float)x, .text = sign, .length = 1ULL,
                            .color = color) +
            pad;
        color = (row.sign == ' ') ? palette->text_2 : color;
        break;
    }
    case CLI_REQUESTS_ROW_ANSWER: {
        const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_REQUESTS_BOX);
        EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = args,
                   .rect = (GuiRect){x, rect.y + ((rect.height - side) / 2), side, side}, .radius = 2,
                   .color = row.checked ? palette->signal : palette->line,
                   .inside = row.checked ? palette->signal : palette->editor_background);
        x = rect.x + EMBED_CALL(requests_box_end, RequestsPartCtx, .pane = args);
        face = GUI_FACE_CODE;
        color = row.shown ? palette->text : palette->text_2;
        break;
    }
    case CLI_REQUESTS_ROW_COMPARE:
        color = row.inert ? palette->text_3 : palette->text;
        break;
    default:
        break;
    }
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .face = face, .size = GUI_REQUESTS_SIZE,
                     .x_f32 = (float)x, .text = row.text, .length = row.text_length, .color = color);
}

// the row pressed: a group opened or closed, an answer's box checked or the answer shown, and the two answers checked
// compared; nothing opens
static int requests_press(const GuiPaneCfg *args)
{
    CliRequestsRow row;
    if (!EMBED_CALL(cli_requests_pane.row, CliRequestsPaneCfg, .at = args->row, .row = &row))
    {
        return 0;
    }
    switch (row.kind)
    {
    case CLI_REQUESTS_ROW_GROUP:
        EMBED_CALL(cli_requests_pane.toggle, CliRequestsPaneCfg, .at = args->row);
        return 0;
    case CLI_REQUESTS_ROW_ANSWER: {
        const GuiRect rect = (args->frame != NULL)
                                 ? EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = args->table, .frame = args->frame,
                                              .pane_read = args->pane_read, .row = args->row)
                                 : (GuiRect){0, 0, 0, 0};
        const int on_box =
            (args->frame != NULL) && (args->x < rect.x + EMBED_CALL(requests_box_end, RequestsPartCtx, .pane = args));
        if (on_box)
        {
            EMBED_CALL(cli_requests_pane.check, CliRequestsPaneCfg, .at = args->row);
        }
        else
        {
            EMBED_CALL(cli_requests_pane.show, CliRequestsPaneCfg, .at = args->row);
        }
        return 0;
    }
    case CLI_REQUESTS_ROW_COMPARE:
        if (!row.inert)
        {
            (void)EMBED_CALL(cli_requests_pane.compare, CliRequestsPaneCfg, .at = args->row);
        }
        return 0;
    default:
        return 0;
    }
}

const GuiPaneTable gui_requests_view = {
    .tool = "comms",
    .foot = 1,
    .rows = requests_rows,
    .head = requests_head,
    .head_draw = requests_head_draw,
    .head_hit = requests_head_hit,
    .action = requests_action,
    .row_draw = requests_row_draw,
    .press = requests_press,
};
