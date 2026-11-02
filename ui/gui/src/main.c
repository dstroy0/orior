// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// main.c: the window's program. The command line is run on the words the program was started with; where it leaves a
// window to open, the window begins, the CLI's side with it, and opens on the system at the place saved for the tree,
// else in the middle of the main monitor; the loop runs until the window closes, its place saved as it does
#include "../../cli/src/command_line/callback_cfg.h"
#include "system.h"
#include "window.h"

// the window's size where none is saved, in the system's pixels
#define GUI_FIRST_WIDTH 1440u
#define GUI_FIRST_HEIGHT 920u
// the monitors read at the most; and the icon's sides in pixels, the small one the title's and the large one the
// taskbar's and the switcher's
#define GUI_MONITORS 16u
#define GUI_ICON_SMALL 32u
#define GUI_ICON_LARGE 128u

static GuiWindow s_window;
static GuiSystem *s_system;
static GuiSystem *s_preferences;

static void main_woken_elsewhere(void *context)
{
    (void)context;
    if (s_system != NULL)
    {
        EMBED_CALL(gui_system_table.wake, GuiSystemCfg, .system = s_system);
    }
}

static void main_resize(const GuiEachWidthHeightScaleCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    (void)EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = (GuiWindow *)args->context, .width = width,
                     .height = height, .system_scale = args->scale);
}

// an input of the main window: a window control pressed acts as the button is let go over it
static void main_input(const GuiEachInputCfg *args)
{
    const GuiInput *input = args->input;
    GuiWindow *const window = (GuiWindow *)args->context;
    const unsigned int pressed = window->pressed;
    const int released = input->type == GUI_INPUT_UP;
    if (released)
    {
        window->pressed = GUI_HIT_CLIENT;
    }
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = window, .input = input);
    if (released && (pressed != GUI_HIT_CLIENT) &&
        (EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = window, .input = input) == pressed))
    {
        EMBED_CALL(gui_system_table.act, GuiSystemCfg, .system = s_system,
                   .act = (pressed == GUI_HIT_MINIMIZE)   ? GUI_ACT_MINIMIZE
                          : (pressed == GUI_HIT_MAXIMIZE) ? GUI_ACT_MAXIMIZE
                                                          : GUI_ACT_CLOSE);
    }
}

static void main_wake(void *context)
{
    EMBED_CALL(gui_window_table.drain, GuiWindowCfg, .window = (GuiWindow *)context);
}

// Preferences' window closed where it stands open, a window owned going before its owner does
static void main_preferences_close(void)
{
    if (s_preferences != NULL)
    {
        EMBED_CALL(gui_system_table.close, GuiSystemCfg, .system = s_preferences);
        EMBED_CALL(gui_system_table.free, GuiSystemCfg, .system = s_preferences);
        s_preferences = NULL;
    }
}

static void main_closing(void *context)
{
    (void)context;
    main_preferences_close();
    CliWindowPlace place;
    EMBED_CALL(gui_system_place.place, GuiSystemPlaceCfg, .system_read = s_system, .place_out = &place);
    cli_window_place_keep(&place);
    EMBED_CALL(gui_system_table.close, GuiSystemCfg, .system = s_system);
    s_system = NULL;
}

typedef struct
{
    GuiWindow *window;
    const VerbaLinea *err_msg;
} MainCtx;
// Preferences' window's frame drawn and shown where one is owed
EMBED_INLINE void main_preferences_present(const MainCtx *args)
{
    GuiWindow *window = args->window;
    if ((s_preferences != NULL) && window->preferences_owed)
    {
        EMBED_CALL(gui_window_table.preferences_draw, GuiWindowCfg, .window = window);
        if (window->preferences_raster.pixels != NULL)
        {
            EMBED_CALL(gui_system_raster.present, GuiSystemRasterCfg, .system = s_preferences,
                       .raster = &window->preferences_raster);
        }
    }
}

static void main_preferences_resize(const GuiEachWidthHeightScaleCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    EMBED_CALL(gui_window_width.preferences_resize, GuiWindowWidthCfg, .window = (GuiWindow *)args->context,
               .width = width, .height = height, .system_scale = args->scale);
}

static void main_preferences_input(const GuiEachInputCfg *args)
{
    const GuiInput *input = args->input;
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = (GuiWindow *)args->context,
               .input = input);
}

static void main_preferences_nothing(void *context)
{
    (void)context;
}

// the system asking to close it: the next idle closes it, inside its own message here
static void main_preferences_closing(void *context)
{
    GuiWindow *const window = (GuiWindow *)context;
    const GuiChoice chosen = {"preferences", ""};
    if (window->preferences.shown)
    {
        EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = window, .choice = &chosen);
    }
}

static unsigned int main_preferences_hit(const GuiEachInputCfg *args)
{
    const GuiInput *input = args->input;
    return EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg,
                      .window_read = (const GuiWindow *)args->context, .input = input);
}

// its frame is drawn here too, which saves it answering while the system drags its edge in a loop of the system's own
static void main_preferences_idle(void *context)
{
    EMBED_CALL(main_preferences_present, MainCtx, .window = (GuiWindow *)context);
}

static void main_preferences_show(const GuiEachShownCfg *args)
{
    (void)args;
}

// Preferences' window opened, in the middle of the main window at the size that shows every color, no taller than the
// main window
EMBED_INLINE void main_preferences_open(const MainCtx *args)
{
    GuiWindow *window = args->window;
    static const GuiSystemEvents s_events = {
        main_preferences_resize,  main_preferences_input, main_preferences_nothing,
        main_preferences_closing, main_preferences_hit,   main_preferences_idle,
        main_preferences_nothing, main_preferences_show,  main_preferences_nothing};
    CliWindowPlace over;
    EMBED_CALL(gui_system_place.place, GuiSystemPlaceCfg, .system_read = s_system, .place_out = &over);
    const int scale = EMBED_CALL(gui_system_place.scale, GuiSystemPlaceCfg, .system_read = s_system);
    const int zoomed = (scale * (int)EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                                .step = window->preference_values.zoom_step)) /
                       GUI_SCALE_WHOLE;
    unsigned int width = 0u;
    unsigned int height = 0u;
    EMBED_CALL(gui_preferences_scale.want, GuiPreferencesScaleCfg, .palette = window->palette, .scale = zoomed,
               .width_pointer = &width, .height_pointer = &height);
    height = (height < over.height) ? height : over.height;
    // a window's sides are far inside what an int represents
    const CliWindowPlace place = {over.x + (((int)over.width - (int)width) / 2),
                                  over.y + (((int)over.height - (int)height) / 2), width, height, 0};
    char why_text[CLI_WINDOW_ERR_MSG_MAX_LEN];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    s_preferences =
        EMBED_CALL(gui_system_place_title.owned_open, GuiSystemPlaceTitleCfg, .owner = s_system, .place = &place,
                   .title = "Preferences", .events = &s_events, .context = window, .err_msg = &why);
    if (s_preferences == NULL)
    {
        window->preferences.shown = 0;
        return;
    }
    EMBED_CALL(gui_window_width.preferences_resize, GuiWindowWidthCfg, .window = window, .width = width,
               .height = height,
               .system_scale = EMBED_CALL(gui_system_place.scale, GuiSystemPlaceCfg, .system_read = s_preferences));
}

// Preferences' window opened or closed as the command last left it
EMBED_INLINE void main_preferences_follow(const MainCtx *args)
{
    if (args->window->preferences.shown && (s_preferences == NULL))
    {
        main_preferences_open(args);
    }
    else if (!args->window->preferences.shown)
    {
        main_preferences_close();
    }
    main_preferences_present(args);
}

static unsigned int main_hit(const GuiEachInputCfg *args)
{
    const GuiInput *input = args->input;
    return EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = (const GuiWindow *)args->context,
                      .input = input);
}

static void main_idle(void *context)
{
    GuiWindow *const window = (GuiWindow *)context;
    if (s_system == NULL)
    {
        return;
    }
    EMBED_CALL(main_preferences_follow, MainCtx, .window = window);
    if (window->owed)
    {
        EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = window);
        EMBED_CALL(gui_system_raster.present, GuiSystemRasterCfg, .system = s_system, .raster = window->raster);
        EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = window);
    }
    // no frame whole owed: the caret's ink or the circle's stage alone has changed, each drawn again over the frame and
    // its rows shown again
    if (window->caret_alone)
    {
        const GuiRect caret = EMBED_CALL(gui_window_table.caret_draw, GuiWindowCfg, .window = window);
        if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = caret))
        {
            EMBED_CALL(gui_system_raster.present_part, GuiSystemRasterCfg, .system = s_system,
                       .raster = (window->shown != NULL) ? window->shown : window->raster, .part = caret);
        }
    }
    if (window->busy_alone)
    {
        const GuiRect circle = EMBED_CALL(gui_window_table.busy_draw, GuiWindowCfg, .window = window);
        if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = circle))
        {
            EMBED_CALL(gui_system_raster.present_part, GuiSystemRasterCfg, .system = s_system,
                       .raster = (window->shown != NULL) ? window->shown : window->raster, .part = circle);
        }
    }
    EMBED_CALL(gui_system_after.tick, GuiSystemAfterCfg, .system = s_system,
               .after = EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = window));
}

static void main_tick(void *context)
{
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = (GuiWindow *)context);
}

static void main_motion(void *context)
{
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = (GuiWindow *)context,
               .reduced = EMBED_CALL(gui_system_place.motion_reduce, GuiSystemPlaceCfg, .system_read = s_system));
}

static void main_show(const GuiEachShownCfg *args)
{
    int shown = args->shown;
    EMBED_CALL(cli_window_shown.show, GuiWindowShownCfg, .window = (GuiWindow *)args->context, .shown = shown);
}

// a line of the errors, `orior: ` before what was written
EMBED_INLINE void main_err(const MainCtx *args)
{
    char text[CLI_WINDOW_ERR_MSG_MAX_LEN + 8u];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, "orior: ");
    mmgr_line_put_n(&line, args->err_msg->out, mmgr_line_ok(args->err_msg) ? args->err_msg->at : 0ull);
    mmgr_line_ch(&line, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = text,
               .length = mmgr_line_ok(&line) ? line.at : 0ull);
}

int main(void)
{
    int window = 0;
    const int code = cli_window_command_line(&window);
    if (!window)
    {
        return code;
    }
    char why_text[CLI_WINDOW_ERR_MSG_MAX_LEN];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    if (!EMBED_CALL(gui_window_woken.begin, GuiWindowWokenCfg, .window = &s_window, .woken = main_woken_elsewhere,
                    .context = NULL, .err_msg = &why))
    {
        EMBED_CALL(main_err, MainCtx, .err_msg = &why);
        return 1;
    }
    cli_window_begun(gui_link_emit, &s_window.link);
    CliWindowArea areas[GUI_MONITORS];
    const unsigned int count =
        EMBED_CALL(cli_system_areas.areas, GuiSystemAreasCfg, .areas = areas, .most = GUI_MONITORS);
    CliWindowPlace place;
    if (!cli_window_place_kept(areas, count, &place))
    {
        // the first size, in the middle of the main monitor where there is one
        place = (CliWindowPlace){0, 0, GUI_FIRST_WIDTH, GUI_FIRST_HEIGHT, 0};
        if (count > 0u)
        {
            // the window's size is far inside what an int represents
            place.x = areas[0].x + (((int)areas[0].width - (int)GUI_FIRST_WIDTH) / 2);
            place.y = areas[0].y + (((int)areas[0].height - (int)GUI_FIRST_HEIGHT) / 2);
        }
    }
    static const GuiSystemEvents s_events = {main_resize, main_input, main_wake, main_closing, main_hit,
                                             main_idle,   main_tick,  main_show, main_motion};
    s_system = EMBED_CALL(gui_system_place_title.open, GuiSystemPlaceTitleCfg, .place = &place, .title = "orior",
                          .events = &s_events, .context = &s_window, .err_msg = &why);
    if (s_system == NULL)
    {
        EMBED_CALL(main_err, MainCtx, .err_msg = &why);
        EMBED_CALL(gui_window_table.end, GuiWindowCfg, .window = &s_window);
        cli_window_ended();
        return 1;
    }
    // the window's icon, the eye at the title's size and the taskbar's, the system taking its own copies
    GuiRaster small;
    GuiRaster large;
    if (EMBED_CALL(cli_window_icon.icon, GuiWindowIconCfg, .window = &s_window, .icon = &small,
                   .size = GUI_ICON_SMALL) &&
        EMBED_CALL(cli_window_icon.icon, GuiWindowIconCfg, .window = &s_window, .icon = &large, .size = GUI_ICON_LARGE))
    {
        EMBED_CALL(gui_system_title_icon.icon, GuiSystemTitleIconCfg, .system = s_system, .title_icon = &small,
                   .taskbar_icon = &large);
        EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &large);
        EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &small);
    }
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = &s_window,
               .reduced = EMBED_CALL(gui_system_place.motion_reduce, GuiSystemPlaceCfg, .system_read = s_system));
    EMBED_CALL(gui_window_table.start, GuiWindowCfg, .window = &s_window);
    // the record saved here for the giving after the loop, since closing the window inside the loop lets go of
    // s_system and leaves the record for the loop to read until it returns
    GuiSystem *const system = s_system;
    EMBED_CALL(gui_system_table.run, GuiSystemCfg, .system = system);
    EMBED_CALL(gui_system_table.free, GuiSystemCfg, .system = system);
    EMBED_CALL(gui_window_table.end, GuiWindowCfg, .window = &s_window);
    cli_window_ended();
    return 0;
}
