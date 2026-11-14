// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_dialog.c: the dialog's system window on the view's thread. It holds the dialog's raster and lattice, passes
// each input to the dialog part (parts/dialog.c), and sends the answer to the interface's held command or retains it for the
// caller. The view opens the window modal over its owner while the dialog shows, and closes it after (thread.c)
#include "../../cli/src/machine/reach.h"
#include "window.h"

typedef struct
{
    GuiWindow *window;
    const GuiDialogChange *change;
} WindowDialogCtx;

int gui_window_dialog_ask(const GuiWindowDialogAskCfg *args)
{
    GuiWindow *const window = args->window;
    if (window->dialog.displayed || (args->owner >= CLI_VIEW_SURFACES) || (args->owner == CLI_VIEW_DIALOG))
    {
        return 0;
    }
    if (!EMBED_CALL(gui_dialog_asking.ask, GuiDialogAskCfg, .dialog = &window->dialog, .title = args->title,
                    .body = args->body, .labels = args->labels, .count = args->count, .preferred = args->preferred,
                    .error = args->error))
    {
        return 0;
    }
    window->dialog_owner = args->owner;
    window->dialog_held = args->held;
    window->dialog_answer = (GuiDialogChange){GUI_DIALOG_NOTHING, GUI_DIALOG_BUTTONS_MAX};
    window->dialog_pending = 1;
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &window->dialog_ground, .paused = 0,
               .now_ms = cli_clock_milliseconds());
    return 1;
}

void gui_window_dialog_want(const GuiWindowDialogWantCfg *args)
{
    EMBED_CALL(gui_dialog_wanting.want, GuiDialogWantCfg, .dialog_read = &args->window->dialog,
               .glyphs = &args->window->glyphs, .scale = args->window->system_scale,
               .width_pointer = args->width_pointer, .height_pointer = args->height_pointer);
}

// The dialog's answer taken: the held command answered with the pressed button's label, or refused for a close. Else
// the answer stays for the caller. The lattice stops while no dialog shows
EMBED_INLINE void window_dialog_answered(const WindowDialogCtx *args)
{
    GuiWindow *const window = args->window;
    const GuiDialogChange *const change = args->change;
    window->dialog_answer = *change;
    if (window->dialog_held)
    {
        window->dialog_held = 0;
        if (change->type == GUI_DIALOG_ANSWERED)
        {
            cli_reach_held_answer(1, window->dialog.label[change->button]);
        }
        else
        {
            cli_reach_held_answer(0, "the dialog closed with no answer");
        }
    }
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &window->dialog_ground, .paused = 1,
               .now_ms = cli_clock_milliseconds());
}

// a change the dialog answered: a new frame owed, and the answer taken where the dialog ended
EMBED_INLINE void window_dialog_changed(const WindowDialogCtx *args)
{
    const unsigned int type = args->change->type;
    if (type == GUI_DIALOG_NOTHING)
    {
        return;
    }
    args->window->dialog_pending = 1;
    if ((type == GUI_DIALOG_ANSWERED) || (type == GUI_DIALOG_CLOSED))
    {
        EMBED_CALL(window_dialog_answered, WindowDialogCtx, .window = args->window, .change = args->change);
    }
}

void gui_window_dialog_resize(const GuiWindowWidthCfg *args)
{
    GuiWindow *const window = args->window;
    const unsigned int width = args->width;
    const unsigned int height = args->height;
    window->dialog_system_scale = args->system_scale;
    EMBED_CALL(gui_dialog_width.resize, GuiDialogWidthCfg, .dialog = &window->dialog, .glyphs = &window->glyphs,
               .width = width, .height = height, .scale = args->system_scale);
    window->dialog_pending = 1;
    // The raster's pixels are for as long as the view does, as the system shows them again each time it paints.
    // The window makes them once, with room for the largest monitor
    if (window->dialog_raster.pixels == NULL)
    {
        const unsigned int max_width = (width > window->display_width) ? width : window->display_width;
        const unsigned int max_height = (height > window->display_height) ? height : window->display_height;
        if (!EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &window->dialog_raster, .width = max_width,
                        .height = max_height, .part = GUI_BLOCK_DIALOG) ||
            !EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &window->dialog_ground_raster,
                        .width = max_width, .height = max_height, .part = GUI_BLOCK_DIALOG_GROUND))
        {
            return;
        }
        window->dialog_capacity = (unsigned long long)max_width * max_height;
    }
    if (((unsigned long long)width * height) > window->dialog_capacity)
    {
        return;
    }
    window->dialog_raster.width = width;
    window->dialog_raster.height = height;
    window->dialog_ground_raster.width = width;
    window->dialog_ground_raster.height = height;
    (void)EMBED_CALL(gui_background_raster.layout, GuiBackgroundRasterCfg, .now_ms = cli_clock_milliseconds(),
                     .background_fill = &window->dialog_ground, .raster = &window->dialog_ground_raster, .width = width,
                     .height = height, .scale = args->system_scale, .palette = window->palette);
}

void gui_window_dialog_hits(const GuiWindowHitsCfg *args)
{
    const GuiDialog *const dialog = &args->window->dialog;
    CliViewHits *const hits = args->hits;
    hits->width = dialog->width;
    hits->height = dialog->height;
    // the dialog retains the size it opened at, so it has no border to drag
    hits->border = 0;
    hits->minimize = (GuiRect){0, 0, 0, 0};
    hits->maximize = (GuiRect){0, 0, 0, 0};
    hits->close = (GuiRect){0, 0, 0, 0};
    // The close mark and each button take their presses. The rest of the head band moves the window
    hits->client[0] = EMBED_CALL(gui_dialog_read.close_rect, GuiDialogReadCfg, .dialog_read = dialog);
    hits->clients = 1u;
    for (unsigned int at = 0u; (at < dialog->buttons) && (hits->clients < GUI_WINDOW_HIT_CLIENTS); at += 1u)
    {
        hits->client[hits->clients] =
            EMBED_CALL(gui_dialog_read.button_rect, GuiDialogReadCfg, .dialog_read = dialog, .at = at);
        hits->clients += 1u;
    }
    hits->caption = EMBED_CALL(gui_dialog_read.head_rect, GuiDialogReadCfg, .dialog_read = dialog);
}

unsigned int gui_window_dialog_hit(const GuiWindowInputCfg *args)
{
    GuiRect client[GUI_WINDOW_HIT_CLIENTS];
    CliViewHits hits = {.client = client};
    EMBED_CALL(gui_window_dialog_hits_table.hits, GuiWindowHitsCfg, .window = args->window_read, .hits = &hits);
    return cli_view_hit_of(&hits, args->input->x, args->input->y);
}

void gui_window_dialog_draw(const GuiWindowCfg *args)
{
    GuiWindow *const window = args->window;
    window->dialog_pending = 0;
    if (window->dialog_raster.pixels == NULL)
    {
        return;
    }
    // the lattice under the panes where the window positioned it at the dialog's size, else the deep color
    const GuiRaster *const ground = &window->dialog_ground_raster;
    const int positioned = window->dialog_ground.positioned && (ground->pixels != NULL) &&
                     (ground->width == window->dialog_raster.width) && (ground->height == window->dialog_raster.height);
    EMBED_CALL(gui_dialog_drawing.draw, GuiDialogDrawCfg, .dialog_read = &window->dialog,
               .raster = &window->dialog_raster, .glyphs = &window->glyphs, .palette = window->palette,
               .ground = positioned ? ground : NULL);
}

void gui_window_dialog_close(const GuiWindowCfg *args)
{
    GuiWindow *const window = args->window;
    if (!window->dialog.displayed)
    {
        return;
    }
    window->dialog.displayed = 0;
    const GuiDialogChange closed = {GUI_DIALOG_CLOSED, GUI_DIALOG_BUTTONS_MAX};
    EMBED_CALL(window_dialog_changed, WindowDialogCtx, .window = window, .change = &closed);
}

static void window_dialog_move(const GuiInputHandlerCfg *args)
{
    GuiDialogChange change;
    EMBED_CALL(gui_dialog_point.move, GuiDialogPointCfg, .dialog = &args->window->dialog, .x = args->input->x,
               .y = args->input->y, .change = &change);
    EMBED_CALL(window_dialog_changed, WindowDialogCtx, .window = args->window, .change = &change);
}

static void window_dialog_press(const GuiInputHandlerCfg *args)
{
    if (args->input->button != GUI_BUTTON_LEFT)
    {
        return;
    }
    GuiDialogChange change;
    EMBED_CALL(gui_dialog_point.press, GuiDialogPointCfg, .dialog = &args->window->dialog, .x = args->input->x,
               .y = args->input->y, .change = &change);
    EMBED_CALL(window_dialog_changed, WindowDialogCtx, .window = args->window, .change = &change);
}

static void window_dialog_release(const GuiInputHandlerCfg *args)
{
    if (args->input->button != GUI_BUTTON_LEFT)
    {
        return;
    }
    GuiDialogChange change;
    EMBED_CALL(gui_dialog_point.release, GuiDialogPointCfg, .dialog = &args->window->dialog, .x = args->input->x,
               .y = args->input->y, .change = &change);
    EMBED_CALL(window_dialog_changed, WindowDialogCtx, .window = args->window, .change = &change);
}

static void window_dialog_leave(const GuiInputHandlerCfg *args)
{
    GuiDialogChange change;
    EMBED_CALL(gui_dialog_point.move, GuiDialogPointCfg, .dialog = &args->window->dialog, .x = -1, .y = -1,
               .change = &change);
    EMBED_CALL(window_dialog_changed, WindowDialogCtx, .window = args->window, .change = &change);
}

static void window_dialog_key(const GuiInputHandlerCfg *args)
{
    GuiDialogChange change;
    EMBED_CALL(gui_dialog_keys.key, GuiDialogKeyCfg, .dialog = &args->window->dialog, .name = args->input->name,
               .change = &change);
    EMBED_CALL(window_dialog_changed, WindowDialogCtx, .window = args->window, .change = &change);
}

// the focus come to the dialog or gone from it: the signal on its border and on the button with the keys follows it
static void window_dialog_focus(const GuiInputHandlerCfg *args)
{
    args->window->dialog.focused = 1;
    args->window->dialog_pending = 1;
}

static void window_dialog_blur(const GuiInputHandlerCfg *args)
{
    args->window->dialog.focused = 0;
    args->window->dialog_pending = 1;
}

static const GuiInputHandlers s_dialog_handlers = {{[GUI_INPUT_MOVED] = window_dialog_move,
                                                    [GUI_INPUT_DOWN] = window_dialog_press,
                                                    [GUI_INPUT_UP] = window_dialog_release,
                                                    [GUI_INPUT_LEFT] = window_dialog_leave,
                                                    [GUI_INPUT_KEY] = window_dialog_key,
                                                    [GUI_INPUT_FOCUS] = window_dialog_focus,
                                                    [GUI_INPUT_BLUR] = window_dialog_blur}};

void gui_window_dialog_input(const GuiWindowInputTypeCfg *args)
{
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &s_dialog_handlers, .window = args->window,
               .input = args->input);
}
