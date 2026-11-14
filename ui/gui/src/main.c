// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// main.c: the window's program, a pump and a blit. The command line is run on the words the program was started with;
// where it leaves a window to open, the view begins on a thread of the command line's and the window opens on the
// system at the place saved for the tree, else in the middle of the main monitor. Every event the system gives goes to
// the view's ring, each order the view writes back is done, and a raster an order passes over is displayed; the loop runs
// until the window closes, its place saved as it does
#include "../../cli/src/command_line/callback_cfg.h"
#include "../../cli/src/command_line/command_line.h"
#include "system.h"

// the window's size where none is saved, in the system's pixels; and the monitors read at max
#define GUI_FIRST_WIDTH 1440u
#define GUI_FIRST_HEIGHT 920u
#define GUI_MONITORS 16u

static GuiSystem *s_system;
static GuiSystem *s_preferences;
static GuiSystem *s_dialog;

// each window's events come with the surface they are of
static const unsigned int s_main_surface = CLI_VIEW_MAIN;
static const unsigned int s_preferences_surface = CLI_VIEW_PREFERENCES;
static const unsigned int s_dialog_surface = CLI_VIEW_DIALOG;

static void main_woken_elsewhere(void *context)
{
    (void)context;
    if (s_system != NULL)
    {
        EMBED_CALL(gui_system_table.wake, GuiSystemCfg, .system = s_system);
    }
}

static void main_sized(const GuiEachWidthHeightScaleCfg *args)
{
    const CliViewEvent event = {.type = CLI_VIEW_EVENT_SIZED,
                                .surface = *(const unsigned int *)args->context,
                                .width = args->width,
                                .height = args->height,
                                .scale = args->scale,
                                .frame_ms = args->frame_ms};
    cli_view_event(&event);
}

static void main_input(const GuiEachInputCfg *args)
{
    const CliViewEvent event = {
        .type = CLI_VIEW_EVENT_INPUT, .surface = *(const unsigned int *)args->context, .input = *args->input};
    cli_view_event(&event);
}

static unsigned int main_hit(const GuiEachInputCfg *args)
{
    return cli_view_hit(*(const unsigned int *)args->context, args->input->x, args->input->y);
}

// Preferences' window closed where it is open, a window owned going before its owner does
static void main_preferences_close(void)
{
    if (s_preferences != NULL)
    {
        EMBED_CALL(gui_system_table.close, GuiSystemCfg, .system = s_preferences);
        EMBED_CALL(gui_system_table.free, GuiSystemCfg, .system = s_preferences);
        s_preferences = NULL;
    }
}

// the dialog's window closed where it is open, its owner taking input again as it goes
static void main_dialog_close(void)
{
    if (s_dialog != NULL)
    {
        EMBED_CALL(gui_system_table.close, GuiSystemCfg, .system = s_dialog);
        EMBED_CALL(gui_system_table.free, GuiSystemCfg, .system = s_dialog);
        s_dialog = NULL;
    }
}

static void main_closing(void *context)
{
    (void)context;
    main_dialog_close();
    main_preferences_close();
    CliWindowPosition place;
    EMBED_CALL(gui_system_position.place, GuiSystemPositionCfg, .system_read = s_system, .position_out = &place);
    cli_view_place_store(cli_command_line_root(), &place);
    EMBED_CALL(gui_system_table.close, GuiSystemCfg, .system = s_system);
    s_system = NULL;
}

static void main_preferences_nothing(void *context)
{
    (void)context;
}

// the system asking to close Preferences' window: the view told, and an order after closes it
static void main_preferences_closing(void *context)
{
    (void)context;
    const CliViewEvent event = {.type = CLI_VIEW_EVENT_CLOSING, .surface = CLI_VIEW_PREFERENCES};
    cli_view_event(&event);
}

static void main_preferences_displayed(const GuiEachDisplayedCfg *args)
{
    (void)args;
}

// the system asking to close the dialog's window: the view told, and an order after closes it
static void main_dialog_closing(void *context)
{
    (void)context;
    const CliViewEvent event = {.type = CLI_VIEW_EVENT_CLOSING, .surface = CLI_VIEW_DIALOG};
    cli_view_event(&event);
}

static void main_displayed(const GuiEachDisplayedCfg *args)
{
    const CliViewEvent event = {.type = CLI_VIEW_EVENT_DISPLAYED, .flag = args->displayed};
    cli_view_event(&event);
}

static void main_motion(void *context)
{
    (void)context;
    const CliViewEvent event = {
        .type = CLI_VIEW_EVENT_MOTION,
        .flag = EMBED_CALL(gui_system_position.motion_reduce, GuiSystemPositionCfg, .system_read = s_system)};
    cli_view_event(&event);
}

// Preferences' window opened in the middle of the main window at the size asked, no taller than the main window; its
// size passed to the view with the scale it is at, or the view told it was refused
static void main_preferences_open(const CliViewOrder *order)
{
    static const GuiSystemEvents s_events = {main_sized,
                                             main_input,
                                             main_preferences_nothing,
                                             main_preferences_closing,
                                             main_hit,
                                             main_preferences_displayed,
                                             main_preferences_nothing};
    CliWindowPosition over;
    EMBED_CALL(gui_system_position.place, GuiSystemPositionCfg, .system_read = s_system, .position_out = &over);
    const unsigned int height = (order->height < over.height) ? order->height : over.height;
    // a window's sides are far inside what an int represents
    const CliWindowPosition place = {over.x + (((int)over.width - (int)order->width) / 2),
                                     over.y + (((int)over.height - (int)height) / 2), order->width, height, 0};
    char error_text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    s_preferences = EMBED_CALL(gui_system_position_title.owner_open, GuiSystemPositionTitleCfg, .owner = s_system,
                               .place = &place, .title = "Preferences", .events = &s_events,
                               .context = (void *)&s_preferences_surface, .err_msg = &why);
    if (s_preferences == NULL)
    {
        const CliViewEvent refused = {.type = CLI_VIEW_EVENT_REFUSED, .surface = CLI_VIEW_PREFERENCES};
        cli_view_event(&refused);
        return;
    }
    const CliViewEvent sized = {
        .type = CLI_VIEW_EVENT_SIZED,
        .surface = CLI_VIEW_PREFERENCES,
        .width = order->width,
        .height = height,
        .scale = EMBED_CALL(gui_system_position.scale, GuiSystemPositionCfg, .system_read = s_preferences)};
    cli_view_event(&sized);
}

// The dialog's window opened modal in the middle of the window that owns it, at the size asked and no larger than the
// owner. Its size goes to the view with the scale it is at, or the view hears that the system refused it
static void main_dialog_open(const CliViewOrder *order)
{
    static const GuiSystemEvents s_events = {main_sized,
                                             main_input,
                                             main_preferences_nothing,
                                             main_dialog_closing,
                                             main_hit,
                                             main_preferences_displayed,
                                             main_preferences_nothing};
    GuiSystem *const owner =
        ((order->owner == CLI_VIEW_PREFERENCES) && (s_preferences != NULL)) ? s_preferences : s_system;
    CliWindowPosition over;
    EMBED_CALL(gui_system_position.place, GuiSystemPositionCfg, .system_read = owner, .position_out = &over);
    const unsigned int width = (order->width < over.width) ? order->width : over.width;
    const unsigned int height = (order->height < over.height) ? order->height : over.height;
    // a window's sides are far inside what an int represents
    const CliWindowPosition place = {over.x + (((int)over.width - (int)width) / 2),
                                     over.y + (((int)over.height - (int)height) / 2), width, height, 0};
    char error_text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    s_dialog = EMBED_CALL(gui_system_position_title.owner_open, GuiSystemPositionTitleCfg, .owner = owner,
                          .place = &place, .title = "orior", .events = &s_events, .context = (void *)&s_dialog_surface,
                          .err_msg = &why, .modal = 1);
    if (s_dialog == NULL)
    {
        const CliViewEvent refused = {.type = CLI_VIEW_EVENT_REFUSED, .surface = CLI_VIEW_DIALOG};
        cli_view_event(&refused);
        return;
    }
    const CliViewEvent sized = {
        .type = CLI_VIEW_EVENT_SIZED,
        .surface = CLI_VIEW_DIALOG,
        .width = width,
        .height = height,
        .scale = EMBED_CALL(gui_system_position.scale, GuiSystemPositionCfg, .system_read = s_dialog)};
    cli_view_event(&sized);
}

// the system that shows `surface`, NULL where its window no longer is
static GuiSystem *main_system_of(unsigned int surface)
{
    if (surface == CLI_VIEW_PREFERENCES)
    {
        return s_preferences;
    }
    return (surface == CLI_VIEW_DIALOG) ? s_dialog : s_system;
}

// a raster displayed on its surface, the view told once it has been; a surface no longer located has nothing to show it on
static void main_present(const CliViewOrder *order)
{
    GuiSystem *const system = main_system_of(order->surface);
    if ((system != NULL) && (order->type == CLI_VIEW_ORDER_PRESENT))
    {
        EMBED_CALL(gui_system_raster.present, GuiSystemRasterCfg, .system = system, .raster = &order->raster);
    }
    else if (system != NULL)
    {
        EMBED_CALL(gui_system_raster.present_part, GuiSystemRasterCfg, .system = system, .raster = &order->raster,
                   .part = order->part);
    }
    const CliViewEvent displayed = {.type = CLI_VIEW_EVENT_PRESENTED, .surface = order->surface};
    cli_view_event(&displayed);
}

// every order waiting done, until the window closes under one
static void main_woken(void *context)
{
    (void)context;
    CliViewOrder order;
    while ((s_system != NULL) && cli_view_order(&order))
    {
        switch (order.type)
        {
        case CLI_VIEW_ORDER_PRESENT:
        case CLI_VIEW_ORDER_PRESENT_PART:
            main_present(&order);
            break;
        case CLI_VIEW_ORDER_ACT:
            EMBED_CALL(gui_system_table.act, GuiSystemCfg, .system = s_system, .act = order.act);
            break;
        case CLI_VIEW_ORDER_CURSOR:
            EMBED_CALL(gui_system_cursor.show, GuiSystemCursorCfg, .system = s_system, .cursor = order.act);
            break;
        case CLI_VIEW_ORDER_PREFERENCES_OPEN:
            main_preferences_open(&order);
            break;
        case CLI_VIEW_ORDER_PREFERENCES_CLOSE:
            main_preferences_close();
            break;
        case CLI_VIEW_ORDER_DIALOG_OPEN:
            main_dialog_open(&order);
            break;
        case CLI_VIEW_ORDER_DIALOG_CLOSE:
            main_dialog_close();
            break;
        case CLI_VIEW_ORDER_ICON:
            EMBED_CALL(gui_system_title_icon.icon, GuiSystemTitleIconCfg, .system = s_system,
                       .title_icon = &order.raster, .taskbar_icon = &order.raster_large);
            break;
        default:
            break;
        }
    }
}

// a line of the errors, `orior: ` before what was written
static void main_err(const VerbaLinea *err_msg)
{
    char text[CLI_ERR_MSG_MAX_LEN + 8u];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&line, "orior: ");
    mmgr_line_put_n(&line, err_msg->out, mmgr_line_ok(err_msg) ? err_msg->at : 0ULL);
    mmgr_line_ch(&line, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = text,
               .length = mmgr_line_ok(&line) ? line.at : 0ULL);
}

int main(void)
{
    int window = 0;
    const int code = cli_command_line_window(&window);
    if (!window)
    {
        return code;
    }
    char error_text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    unsigned int display_width = 0u;
    unsigned int display_height = 0u;
    EMBED_CALL(gui_system_width.display_max, GuiSystemWidthCfg, .width = &display_width, .height = &display_height);
    const CliViewBeginCfg begin = {main_woken_elsewhere, NULL, display_width, display_height, &why};
    if (!cli_view_begin(&begin))
    {
        main_err(&why);
        return 1;
    }
    CliWindowArea areas[GUI_MONITORS];
    const unsigned int count =
        EMBED_CALL(cli_system_areas.areas, GuiSystemAreasCfg, .areas = areas, .max = GUI_MONITORS);
    CliWindowPosition place;
    if (!cli_view_place_saved(cli_command_line_root(), areas, count, &place))
    {
        // the first size, in the middle of the main monitor where there is one
        place = (CliWindowPosition){0, 0, GUI_FIRST_WIDTH, GUI_FIRST_HEIGHT, 0};
        if (count > 0u)
        {
            // the window's size is far inside what an int represents
            place.x = areas[0].x + (((int)areas[0].width - (int)GUI_FIRST_WIDTH) / 2);
            place.y = areas[0].y + (((int)areas[0].height - (int)GUI_FIRST_HEIGHT) / 2);
        }
    }
    static const GuiSystemEvents s_events = {main_sized, main_input,     main_woken, main_closing,
                                             main_hit,   main_displayed, main_motion};
    s_system = EMBED_CALL(gui_system_position_title.open, GuiSystemPositionTitleCfg, .place = &place, .title = "orior",
                          .events = &s_events, .context = (void *)&s_main_surface, .err_msg = &why);
    if (s_system == NULL)
    {
        main_err(&why);
        cli_view_end();
        return 1;
    }
    main_motion(NULL);
    // the orders written before the window was, whose wakes went nowhere
    main_woken(NULL);
    // the record saved here for the giving after the loop, since closing the window inside the loop lets go of
    // s_system and leaves the record for the loop to read until it returns
    GuiSystem *const system = s_system;
    EMBED_CALL(gui_system_table.run, GuiSystemCfg, .system = system);
    EMBED_CALL(gui_system_table.free, GuiSystemCfg, .system = system);
    cli_view_end();
    return 0;
}
