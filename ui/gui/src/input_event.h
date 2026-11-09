#include "embed_dispatch_layout.h"
// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// input_event.h: one input: its type, the pointer's place, the button, the modifier keys, the wheel's rows, a key's
// name or the text typed. The system fills it in and each window has one table saying which of its functions answers
// which type (input_event.c)
#ifndef GUI_INPUT_EVENT_H
#define GUI_INPUT_EVENT_H
typedef struct GuiInputEventCfg GuiInputEventCfg;

// the types of input: the pointer moved, a button pressed, a button let go, the pointer gone off the window, the wheel
// turned, a key pressed, text typed, the system's gesture for a menu at the pointer, the right button let go on
// Windows and pressed on X11 as a browser on each opens one, and the keys gone to another window; and the modifier keys
// down, as bits
#define GUI_INPUT_MOVED 0u
#define GUI_INPUT_DOWN 1u
#define GUI_INPUT_UP 2u
#define GUI_INPUT_LEFT 3u
#define GUI_INPUT_WHEEL 4u
#define GUI_INPUT_KEY 5u
#define GUI_INPUT_TEXT 6u
#define GUI_INPUT_MENU 7u
#define GUI_INPUT_BLUR 8u
#define GUI_INPUT_FOCUS 9u
#define GUI_INPUT_TYPES 10u
#define GUI_MOD_SHIFT 1u
#define GUI_MOD_CONTROL 2u
#define GUI_MOD_ALT 4u
// the buttons a press names: the left, the middle and the right
#define GUI_BUTTON_LEFT 0u
#define GUI_BUTTON_MIDDLE 1u
#define GUI_BUTTON_RIGHT 2u

// An input: the type; the pointer at x, y of the client; the button, 0 for the left; the modifier keys down; the
// wheel's rows, down where positive; the key's name as the editor names a key, `Mod+Shift+Left`, Mod the control key,
// Alt and Shift before the key's own name; and `size` bytes of text typed as UTF-8. Last, the wheel's turn as the
// system gave it, in GUI_WHEEL_UNITS a notch, down and to the right where positive. A touchpad and a free wheel turn by
// parts of a notch, and `rows` counts only the whole notches the parts add up to
typedef struct
{
    unsigned int type;
    int x;
    int y;
    unsigned int button;
    unsigned int mods;
    int rows;
    const char *name;
    const char *text;
    unsigned long long size;
    int wheel_down;
    int wheel_across;
} GuiInput;

// the wheel's units in one notch, as Win32 counts them; X11 turns a whole notch at a press of a wheel button
#define GUI_WHEEL_UNITS 120

// an input of each type as a compound literal at the call: any member not named is zero
#define GUI_MOVED_INPUT(x_, y_) (&(GuiInput){.type = GUI_INPUT_MOVED, .x = (x_), .y = (y_)})
#define GUI_DOWN_INPUT(x_, y_, mods_) (&(GuiInput){.type = GUI_INPUT_DOWN, .x = (x_), .y = (y_), .mods = (mods_)})
#define GUI_UP_INPUT(x_, y_) (&(GuiInput){.type = GUI_INPUT_UP, .x = (x_), .y = (y_)})
#define GUI_LEFT_INPUT() (&(GuiInput){.type = GUI_INPUT_LEFT})
#define GUI_WHEEL_INPUT(x_, y_, rows_, down_, across_)                                                                  \
    (&(GuiInput){.type = GUI_INPUT_WHEEL,                                                                              \
                 .x = (x_),                                                                                            \
                 .y = (y_),                                                                                            \
                 .rows = (rows_),                                                                                      \
                 .wheel_down = (down_),                                                                                \
                 .wheel_across = (across_)})
#define GUI_KEY_INPUT(name_) (&(GuiInput){.type = GUI_INPUT_KEY, .name = (name_)})
#define GUI_TEXT_INPUT(text_, size_) (&(GuiInput){.type = GUI_INPUT_TEXT, .text = (text_), .size = (size_)})

// What answers an input: the window's own record and the input
struct GuiWindow;
typedef struct
{
    struct GuiWindow *window;
    const GuiInput *input;
} GuiInputHandlerCfg;

typedef void (*GuiInputHandler)(const GuiInputHandlerCfg *args);

// the function answering each type, none where a type goes unanswered
typedef struct
{
    GuiInputHandler by_type[GUI_INPUT_TYPES];
} GuiInputHandlers;

// `input` handle to the function `handlers` names for its type, a type unknown or unanswered going nowhere
void gui_input_event_handle(const GuiInputEventCfg *args);
struct GuiInputEventCfg
{
    const GuiInputHandlers *handlers;
    struct GuiWindow *window;
    const GuiInput *input;
};

typedef struct
{
    void (*handle)(const GuiInputEventCfg *args);
} GuiInputEventNs;
EMBED_TABLE_LAYOUT(GuiInputEventNs, handle);
EMBED_TABLE_STORAGE GuiInputEventNs gui_input_event EMBED_UNUSED = {
    .handle = gui_input_event_handle,
};

#endif
