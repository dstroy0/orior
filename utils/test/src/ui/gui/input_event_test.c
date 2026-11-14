// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// input_event_test.c: the input's check: each input carries what its type says and leaves the rest zero, and the table passes an input
// to the function named for its type and to nothing else
#include "../../../../../ui/gui/src/input_event.h"
#include "../cli/cli_check.h"

// the input each function last took, by the function
static const GuiInput *s_pressed;
static const GuiInput *s_moved;
static struct GuiWindow *s_window_transferred;

static void transferred_pressed(const GuiInputHandlerCfg *args)
{
    s_window_transferred = args->window;
    s_pressed = args->input;
}

static void transferred_moved(const GuiInputHandlerCfg *args)
{
    s_window_transferred = args->window;
    s_moved = args->input;
}

int main(void)
{
    const GuiInput *const down = GUI_DOWN_INPUT(7, 9, GUI_MOD_SHIFT);
    cli_check((down->type == GUI_INPUT_DOWN) && (down->x == 7) && (down->y == 9) && (down->mods == GUI_MOD_SHIFT), "a press carries its place and its modifiers");
    cli_check((down->button == 0u) && (down->name == NULL) && (down->text == NULL) && (down->size == 0ull), "and the rest of it is zero");
    const GuiInput *const moved = GUI_MOVED_INPUT(3, 4);
    cli_check((moved->type == GUI_INPUT_MOVED) && (moved->x == 3) && (moved->y == 4), "the pointer moved carries its place");
    const GuiInput *const up = GUI_UP_INPUT(5, 6);
    cli_check((up->type == GUI_INPUT_UP) && (up->x == 5) && (up->y == 6), "a release carries its place");
    cli_check(GUI_LEFT_INPUT()->type == GUI_INPUT_LEFT, "the pointer gone carries its type");
    const GuiInput *const wheel = GUI_WHEEL_INPUT(1, 2, -3, -40, 0);
    cli_check((wheel->type == GUI_INPUT_WHEEL) && (wheel->x == 1) && (wheel->y == 2) && (wheel->rows == -3), "the wheel carries its place and its rows");
    cli_check((wheel->wheel_down == -40) && (wheel->wheel_across == 0), "the wheel carries its turn in units, a part of a notch retained");
    const GuiInput *const key = GUI_KEY_INPUT("Mod+Left");
    cli_check((key->type == GUI_INPUT_KEY) && (key->name != NULL) && (key->name[0] == 'M'), "a key carries its name");
    const GuiInput *const text = GUI_TEXT_INPUT("ab", 2ull);
    cli_check((text->type == GUI_INPUT_TEXT) && (text->size == 2ull) && (text->text[1] == 'b'), "text typed carries its bytes");

    // the table passes each type to the function it names, and a type it names none for goes nowhere
    GuiInputHandlers handlers = {{NULL}};
    handlers.by_type[GUI_INPUT_DOWN] = transferred_pressed;
    handlers.by_type[GUI_INPUT_MOVED] = transferred_moved;
    struct GuiWindow *const window = (struct GuiWindow *)&handlers;
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &handlers, .window = window, .input = down);
    cli_check((s_pressed == down) && (s_moved == NULL) && (s_window_transferred == window), "a press reaches the function its table names, with the window");
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &handlers, .window = window, .input = moved);
    cli_check((s_moved == moved) && (s_pressed == down), "the pointer moved reaches its own");
    s_pressed = NULL;
    s_moved = NULL;
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &handlers, .window = window, .input = key);
    cli_check((s_pressed == NULL) && (s_moved == NULL), "a type the table does not answer goes nowhere");
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &handlers, .window = window, .input = &(GuiInput){.type = GUI_INPUT_TYPES});
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &handlers, .window = window, .input = &(GuiInput){.type = 0xffffffffu});
    cli_check((s_pressed == NULL) && (s_moved == NULL), "and a type no table knows goes nowhere");
    return cli_on_check();
}
