// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// main.c: the window's program. The command line is run on the words the program was started with; where it leaves a
// window to open, the window begins, the CLI's side with it, and opens on the system at the place kept for the tree,
// else in the middle of the main monitor; the loop runs until the window closes, its place kept as it does
#include "system.h"
#include "window.h"

// the window's size where none is kept, in the system's pixels
#define GUI_FIRST_WIDTH 1440u
#define GUI_FIRST_HEIGHT 920u
// the monitors read at the most
#define GUI_MONITORS 16u

static GuiWindow s_window;
static GuiSystem *s_system;

static void main_woken_elsewhere(void *context)
{
    (void)context;
    if (s_system != NULL)
    {
        gui_system_wake(s_system);
    }
}

static void main_sized(void *context, unsigned int width, unsigned int height, float scale)
{
    (void)gui_window_sized((GuiWindow *)context, width, height, scale);
}

static void main_pointer(void *context, unsigned int kind, int x, int y, unsigned int button, unsigned int mods)
{
    GuiWindow *const window = (GuiWindow *)context;
    (void)button;
    if (kind == GUI_POINTER_DOWN)
    {
        gui_window_pressed(window, x, y, mods);
    }
    else if (kind == GUI_POINTER_MOVED)
    {
        gui_window_moved(window, x, y);
    }
    else if (kind == GUI_POINTER_UP)
    {
        const unsigned int pressed = window->pressed;
        window->pressed = GUI_HIT_CLIENT;
        gui_window_released(window, x, y);
        if ((pressed != GUI_HIT_CLIENT) && (gui_window_hit(window, x, y) == pressed))
        {
            gui_system_act(s_system, (pressed == GUI_HIT_MINIMIZE)   ? GUI_ACT_MINIMIZE
                                     : (pressed == GUI_HIT_MAXIMIZE) ? GUI_ACT_MAXIMIZE
                                                                     : GUI_ACT_CLOSE);
        }
    }
}

static void main_wheel(void *context, int x, int y, int rows)
{
    gui_window_wheel((GuiWindow *)context, x, y, rows);
}

static void main_key(void *context, const char *name)
{
    gui_window_key((GuiWindow *)context, name);
}

static void main_text(void *context, const char *text, unsigned long long size)
{
    gui_window_text((GuiWindow *)context, text, size);
}

static void main_woken(void *context)
{
    gui_window_drained((GuiWindow *)context);
}

static void main_closing(void *context)
{
    (void)context;
    CliWindowPlace place;
    gui_system_placed(s_system, &place);
    cli_window_place_keep(&place);
    gui_system_closed(s_system);
    s_system = NULL;
}

static unsigned int main_hit(void *context, int x, int y)
{
    return gui_window_hit((const GuiWindow *)context, x, y);
}

static void main_idle(void *context)
{
    GuiWindow *const window = (GuiWindow *)context;
    if (s_system == NULL)
    {
        return;
    }
    if (window->owed)
    {
        gui_window_drawn(window);
        gui_system_present(s_system, &window->raster);
    }
    gui_system_tick(s_system, gui_window_due(window));
}

static void main_ticked(void *context)
{
    gui_window_ticked((GuiWindow *)context);
}

// a line of the errors, `orior: ` before what was written
static void main_err(const VerbaLinea *err_msg)
{
    char text[CLI_WINDOW_ERR_MSG_MAX_LEN + 8u];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, "orior: ");
    mmgr_line_put_n(&line, err_msg->out, mmgr_line_ok(err_msg) ? err_msg->at : 0ull);
    mmgr_line_ch(&line, '\n');
    query_terminal_written(1, text, mmgr_line_ok(&line) ? line.at : 0ull);
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
    if (!gui_window_begun(&s_window, main_woken_elsewhere, NULL, &why))
    {
        main_err(&why);
        return 1;
    }
    cli_window_begun(gui_link_emitted, &s_window.link);
    CliWindowArea areas[GUI_MONITORS];
    const unsigned int count = gui_system_areas(areas, GUI_MONITORS);
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
    static const GuiSystemEvents s_events = {main_sized, main_pointer, main_wheel, main_key,  main_text,
                                             main_woken, main_closing, main_hit,   main_idle, main_ticked};
    s_system = gui_system_opened(&place, "orior", &s_events, &s_window, &why);
    if (s_system == NULL)
    {
        main_err(&why);
        gui_window_ended(&s_window);
        cli_window_ended();
        return 1;
    }
    gui_window_started(&s_window);
    gui_system_run(s_system);
    gui_window_ended(&s_window);
    cli_window_ended();
    return 0;
}
