// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system_x11.c: the window on X11. A window with its decorations taken off through the Motif hints, moved and resized
// by the window manager through _NET_WM_MOVERESIZE as the pointer presses on the caption or a border; the raster it is
// handed put into the window as a 32-bit image; the thread woken through a pipe the loop waits on beside the display's
// connection; the scale read from the screen's size in millimetres; the screen the one area. The records of the two
// windows that stand at once, and the words their icon goes to the system in, are this file's own
#include "../../cli/src/command_line/callback_cfg.h"
#include "system.h"

#include "../../cli/src/command_line/cli_internal.h"

// the tree names a structure Atom in its engine's headers, which the CLI's headers include; X's Atom is renamed
// X11Atom here, in X's own headers and in this file alike
#define Atom X11Atom // NOSONAR
#include "xsettings.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <poll.h>
#include <unistd.h>

struct GuiSystem
{
    int taken;
    Display *display;
    Window window;
    int screen;
    const GuiSystemEvents *events;
    void *context;
    GuiRaster displayed;
    int wake_pipe[2];
    int closed;
    unsigned int width;
    unsigned int height;
    Atom delete_window;
    Atom protocols;
    Atom moveresize;
    Atom state;
    Atom state_maximized_horizontal;
    Atom state_maximized_vertical;
    Window settings_owner;
    Atom settings;
    GuiSystem *owner;
    GuiSystem *owned;
};
// the rows one notch of the wheel scrolls; the Motif hints that take the decorations off, their flags word and the
// decorations word; and the window manager's move and resize directions, as the specification numbers them, and its
// request to move
#define GUI_WHEEL_ROWS 3
#define GUI_MOTIF_HINTS_DECORATIONS 2UL
#define GUI_MOVERESIZE_MOVE 8L
// the milliseconds of one refresh of the screen, taken as 60 a second: the core protocol carries no rate
#define GUI_FRAME_MS 17u
// the windows that stand at once: the main one and Preferences'
#define SYSTEM_WINDOWS 2u

static GuiSystem s_systems[SYSTEM_WINDOWS];
// the icon property's words: each icon's width and height and then its pixels
static unsigned long s_icon_words[4u + (GUI_ICON_SMALL * GUI_ICON_SMALL) + (GUI_ICON_LARGE * GUI_ICON_LARGE)];

typedef struct
{
    Display *display;
    int screen;
    GuiSystem *system;
    unsigned int from;
    unsigned int rows;
    int root_x;
    int root_y;
    long direction;
    unsigned int state;
    XKeyEvent *event;
    unsigned int hit;
    XEvent *event_x_event_pointer;
    const CliWindowPosition *place;
    const char *title;
    unsigned long *data;
    const GuiRaster *raster;
} SystemX11Ctx;
// the first record no window stands on, cleared and taken: NULL where both stand
static GuiSystem *system_record_take(void)
{
    for (unsigned int at = 0u; at < SYSTEM_WINDOWS; at += 1u)
    {
        if (!s_systems[at].taken)
        {
            memor.set(&(MemoriaCfg){.dst = &s_systems[at], .val = 0, .bytes = sizeof(s_systems[at])});
            s_systems[at].taken = 1;
            return &s_systems[at];
        }
    }
    return NULL;
}

// the scale of the screen: its pixels an inch over 96
EMBED_INLINE int system_scale_get(const SystemX11Ctx *args)
{
    const int pixels = DisplayWidth(args->display, args->screen);
    const int millimetres = DisplayWidthMM(args->display, args->screen);
    if ((pixels <= 0) || (millimetres <= 0))
    {
        return 100;
    }
    // pixels over millimetres times 25.4 millimetres an inch, over 96, as a percent
    const int scale = (int)(((long long)pixels * 2540LL) / ((long long)millimetres * 96LL));
    // a scale a sliver off one is one, as a screen reports its size roughly
    return ((scale > 90) && (scale < 110)) ? 100 : scale;
}

// the raster shown put into the window, its `rows` rows from the row `from`
EMBED_INLINE void system_append(const SystemX11Ctx *args)
{
    const GuiRaster *const raster = &args->system->displayed;
    if (raster->pixels == NULL)
    {
        return;
    }
    XImage image;
    memor.set(&(MemoriaCfg){.dst = &image, .val = 0, .bytes = sizeof(image)});
    // a raster's sides are far inside what an int represents
    image.width = (int)raster->width;
    image.height = (int)raster->height;
    image.format = ZPixmap;
    image.data = (char *)raster->pixels;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 32;
    image.bitmap_bit_order = LSBFirst;
    image.bitmap_pad = 32;
    image.depth = 24;
    image.bytes_per_line = (int)(raster->width * sizeof(GuiColor));
    image.bits_per_pixel = 32;
    image.red_mask = 0xff0000UL;
    image.green_mask = 0x00ff00UL;
    image.blue_mask = 0x0000ffUL;
    XInitImage(&image);
    GC context = XCreateGC(args->system->display, args->system->window, 0UL, NULL);
    // the rows are inside the raster, whose sides are far inside what an int represents
    XPutImage(args->system->display, args->system->window, context, &image, 0, (int)args->from, 0, (int)args->from,
              raster->width, args->rows);
    XFreeGC(args->system->display, context);
    XFlush(args->system->display);
}

// the window manager asked to move or resize the window from the pointer at x, y of the root, `direction` as the
// specification numbers the borders, or the move
EMBED_INLINE void system_moveresize(const SystemX11Ctx *args)
{
    XEvent event;
    memor.set(&(MemoriaCfg){.dst = &event, .val = 0, .bytes = sizeof(event)});
    event.xclient.type = ClientMessage;
    event.xclient.window = args->system->window;
    event.xclient.message_type = args->system->moveresize;
    event.xclient.format = 32;
    event.xclient.data.l[0] = args->root_x;
    event.xclient.data.l[1] = args->root_y;
    event.xclient.data.l[2] = args->direction;
    event.xclient.data.l[3] = Button1;
    event.xclient.data.l[4] = 1;
    XUngrabPointer(args->system->display, CurrentTime);
    XSendEvent(args->system->display, RootWindow(args->system->display, args->system->screen), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(args->system->display);
}

// the modifier keys down in an event's state, as system.h names them
EMBED_INLINE unsigned int system_mods(const SystemX11Ctx *args)
{
    return ((args->state & ShiftMask) ? GUI_MOD_SHIFT : 0u) | ((args->state & ControlMask) ? GUI_MOD_CONTROL : 0u) |
           ((args->state & Mod1Mask) ? GUI_MOD_ALT : 0u);
}

// a key press passed to the window: a key with a name of the page's editor's, the modifier keys down written before
// it, and a letter typed as text where the key maps to one and the control key is not down
EMBED_INLINE void system_key_press(const SystemX11Ctx *args)
{
    static const struct
    {
        KeySym key;
        const char *name;
    } s_named[] = {{XK_Left, "Left"},      {XK_Right, "Right"},        {XK_Up, "Up"},
                   {XK_Down, "Down"},      {XK_Home, "Home"},          {XK_End, "End"},
                   {XK_Page_Up, "PageUp"}, {XK_Page_Down, "PageDown"}, {XK_BackSpace, "Backspace"},
                   {XK_Delete, "Delete"},  {XK_Return, "Enter"},       {XK_KP_Enter, "Enter"},
                   {XK_Tab, "Tab"},        {XK_ISO_Left_Tab, "Tab"},   {XK_Escape, "Escape"},
                   {XK_space, "Space"},    {XK_Insert, "Insert"}};
    char text[32];
    KeySym key = NoSymbol;
    const int wrote = XLookupString(args->event, text, sizeof(text) - 1, &key, NULL);
    const unsigned int mods = EMBED_CALL(system_mods, SystemX11Ctx, .state = args->event->state);
    char own[32];
    VerbaLinea named = {own, sizeof(own), 0ULL};
    int has_name = 0;
    for (unsigned int at = 0u; at < sizeof(s_named) / sizeof(s_named[0]); at += 1u)
    {
        if (s_named[at].key == key)
        {
            mmgr_line_put(&named, s_named[at].name);
            has_name = 1;
        }
    }
    if (!has_name && (key >= XK_F1) && (key <= XK_F12))
    {
        mmgr_line_ch(&named, 'F');
        mmgr_line_decimal(&named, (unsigned long long)(key - XK_F1) + 1ULL, 1u);
        has_name = 1;
    }
    if (!has_name && (((key >= XK_a) && (key <= XK_z)) || ((key >= XK_A) && (key <= XK_Z))))
    {
        // a letter's key goes by its capital
        mmgr_line_ch(&named, (char)(((key >= XK_a) ? (key - XK_a) : (key - XK_A)) + 'A'));
        has_name = 1;
    }
    if (!has_name && (key >= XK_0) && (key <= XK_9))
    {
        mmgr_line_ch(&named, (char)((key - XK_0) + '0'));
        has_name = 1;
    }
    if (!has_name && (key < 0x80UL) && (key > XK_space))
    {
        // a punctuation key goes by its own character
        mmgr_line_ch(&named, (char)key);
        has_name = 1;
    }
    if (has_name && (mmgr_line_finish(&named) != 0ULL))
    {
        char whole[48];
        VerbaLinea line = {whole, sizeof(whole), 0ULL};
        mmgr_line_put(&line, (mods & GUI_MOD_CONTROL) ? "Mod+" : "");
        mmgr_line_put(&line, (mods & GUI_MOD_ALT) ? "Alt+" : "");
        mmgr_line_put(&line, (mods & GUI_MOD_SHIFT) ? "Shift+" : "");
        mmgr_line_put(&line, own);
        (void)mmgr_line_finish(&line);
        EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                   .input = GUI_KEY_INPUT(whole));
    }
    if ((wrote > 0) && !(mods & GUI_MOD_CONTROL) && ((unsigned char)text[0] >= 0x20u) &&
        ((unsigned char)text[0] != 0x7fu))
    {
        // the text as Latin-1, each byte a letter, written as UTF-8
        char utf8[128];
        VerbaLinea line = {utf8, sizeof(utf8), 0ULL};
        for (int at = 0; at < wrote; at += 1)
        {
            EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line,
                       .letter = (unsigned char)text[at]);
        }
        EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                   .input = GUI_TEXT_INPUT(utf8, line.at));
    }
}

// the move or resize direction a hit stands for, or -1 for none
EMBED_INLINE long system_direction(const SystemX11Ctx *args)
{
    static const long s_directions[13] = {-1L, GUI_MOVERESIZE_MOVE, 7L, 3L, 1L, 5L, 0L, 2L, 6L, 4L, -1L, -1L, -1L};
    return (args->hit < 13u) ? s_directions[args->hit] : -1L;
}

// a request of the settings' window's refused as that window goes: nothing is to be done about it. Its signature is
// the one XSetErrorHandler declares
static int system_settings_reject(Display *display, XErrorEvent *error) // NOSONAR
{
    (void)display;
    (void)error;
    return 0;
}

int gui_system_motion_reduce(const GuiSystemPositionCfg *args)
{
    if (args->system_read->settings_owner == None)
    {
        return 0;
    }
    Atom type = None;
    int format = 0;
    unsigned long count = 0UL;
    unsigned long after = 0UL;
    unsigned char *data = NULL;
    const XErrorHandler saved = XSetErrorHandler(system_settings_reject);
    // a settings list is far shorter than a million words
    const int requested =
        XGetWindowProperty(args->system_read->display, args->system_read->settings_owner, args->system_read->settings,
                           0L, 1L << 20, False, AnyPropertyType, &type, &format, &count, &after, &data);
    XSync(args->system_read->display, False);
    XSetErrorHandler(saved);
    int enabled = 1;
    const int read =
        (requested == Success) && (data != NULL) && (format == 8) &&
        EMBED_CALL(gui_xsettings.animations, GuiXsettingsCfg, .data = data, .size = count, .enabled = &enabled);
    if (data != NULL)
    {
        XFree(data);
    }
    return read && !enabled;
}

// the window holding the desktop's settings found and its changes watched, where the desktop runs one
EMBED_INLINE void system_settings_watch(const SystemX11Ctx *args)
{
    char name[32];
    VerbaLinea named = {name, sizeof(name), 0ULL};
    mmgr_line_put(&named, "_XSETTINGS_S");
    // a screen's number is not negative
    mmgr_line_decimal(&named, (unsigned long long)args->system->screen, 1u);
    (void)mmgr_line_finish(&named);
    args->system->settings = XInternAtom(args->system->display, "_XSETTINGS_SETTINGS", False);
    args->system->settings_owner =
        XGetSelectionOwner(args->system->display, XInternAtom(args->system->display, name, False));
    if (args->system->settings_owner != None)
    {
        XSelectInput(args->system->display, args->system->settings_owner, PropertyChangeMask | StructureNotifyMask);
    }
}

// one event of the display's taken
static void system_event(const SystemX11Ctx *args)
{
    // a window owned shares this one's connection and its events come through its loop
    if ((args->system->owned != NULL) && (args->event_x_event_pointer->xany.window == args->system->owned->window))
    {
        EMBED_CALL(system_event, SystemX11Ctx, .system = args->system->owned,
                   .event_x_event_pointer = args->event_x_event_pointer);
        return;
    }
    if ((args->system->settings_owner != None) &&
        (args->event_x_event_pointer->xany.window == args->system->settings_owner))
    {
        if (args->event_x_event_pointer->type == DestroyNotify)
        {
            args->system->settings_owner = None;
            args->system->events->motion(args->system->context);
        }
        else if ((args->event_x_event_pointer->type == PropertyNotify) &&
                 (args->event_x_event_pointer->xproperty.atom == args->system->settings))
        {
            args->system->events->motion(args->system->context);
        }
        return;
    }
    switch (args->event_x_event_pointer->type)
    {
    case Expose:
        if (args->event_x_event_pointer->xexpose.count == 0)
        {
            EMBED_CALL(system_append, SystemX11Ctx, .system = args->system, .from = 0u,
                       .rows = args->system->displayed.height);
        }
        break;
    case ConfigureNotify:
        // a window's sides are not negative
        if (((unsigned int)args->event_x_event_pointer->xconfigure.width != args->system->width) ||
            ((unsigned int)args->event_x_event_pointer->xconfigure.height != args->system->height))
        {
            args->system->width = (unsigned int)args->event_x_event_pointer->xconfigure.width;
            args->system->height = (unsigned int)args->event_x_event_pointer->xconfigure.height;
            EMBED_CALL(args->system->events->sized, GuiEachWidthHeightScaleCfg, .context = args->system->context,
                       .width = args->system->width, .height = args->system->height,
                       .scale = EMBED_CALL(system_scale_get, SystemX11Ctx, .display = args->system->display,
                                           .screen = args->system->screen),
                       .frame_ms = GUI_FRAME_MS);
        }
        break;
    case MapNotify:
        EMBED_CALL(args->system->events->displayed, GuiEachDisplayedCfg, .context = args->system->context,
                   .displayed = 1);
        break;
    case UnmapNotify:
        EMBED_CALL(args->system->events->displayed, GuiEachDisplayedCfg, .context = args->system->context,
                   .displayed = 0);
        break;
    case MotionNotify:
        EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                   .input = &(GuiInput){.type = GUI_INPUT_MOVED,
                                        .x = args->event_x_event_pointer->xmotion.x,
                                        .y = args->event_x_event_pointer->xmotion.y,
                                        .mods = EMBED_CALL(system_mods, SystemX11Ctx,
                                                           .state = args->event_x_event_pointer->xmotion.state)});
        break;
    case LeaveNotify:
        // a grab's crossing is no leave: the pointer has not gone
        if (args->event_x_event_pointer->xcrossing.mode == NotifyNormal)
        {
            EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                       .input = GUI_LEFT_INPUT());
        }
        break;
    case KeyPress:
        EMBED_CALL(system_key_press, SystemX11Ctx, .system = args->system, .event = &args->event_x_event_pointer->xkey);
        break;
    case ButtonPress:
        if (args->event_x_event_pointer->xbutton.button == Button1)
        {
            const long direction = EMBED_CALL(
                system_direction, SystemX11Ctx,
                .hit = EMBED_CALL(args->system->events->hit, GuiEachInputCfg, .context = args->system->context,
                                  .input = GUI_MOVED_INPUT(args->event_x_event_pointer->xbutton.x,
                                                           args->event_x_event_pointer->xbutton.y)));
            if (direction >= 0L)
            {
                EMBED_CALL(system_moveresize, SystemX11Ctx, .system = args->system,
                           .root_x = args->event_x_event_pointer->xbutton.x_root,
                           .root_y = args->event_x_event_pointer->xbutton.y_root, .direction = direction);
            }
            else
            {
                EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                           .input = GUI_DOWN_INPUT(args->event_x_event_pointer->xbutton.x,
                                                   args->event_x_event_pointer->xbutton.y,
                                                   EMBED_CALL(system_mods, SystemX11Ctx,
                                                              .state = args->event_x_event_pointer->xbutton.state)));
            }
        }
        else if (args->event_x_event_pointer->xbutton.button == Button2)
        {
            EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                       .input = &(GuiInput){.type = GUI_INPUT_DOWN,
                                            .x = args->event_x_event_pointer->xbutton.x,
                                            .y = args->event_x_event_pointer->xbutton.y,
                                            .button = GUI_BUTTON_MIDDLE,
                                            .mods = EMBED_CALL(system_mods, SystemX11Ctx,
                                                               .state = args->event_x_event_pointer->xbutton.state)});
        }
        else if (args->event_x_event_pointer->xbutton.button == Button3)
        {
            // the right button's press, and a browser on X11 opens a menu as it is pressed
            const unsigned int mods =
                EMBED_CALL(system_mods, SystemX11Ctx, .state = args->event_x_event_pointer->xbutton.state);
            EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                       .input = &(GuiInput){.type = GUI_INPUT_DOWN,
                                            .x = args->event_x_event_pointer->xbutton.x,
                                            .y = args->event_x_event_pointer->xbutton.y,
                                            .button = GUI_BUTTON_RIGHT,
                                            .mods = mods});
            EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                       .input = &(GuiInput){.type = GUI_INPUT_MENU,
                                            .x = args->event_x_event_pointer->xbutton.x,
                                            .y = args->event_x_event_pointer->xbutton.y,
                                            .button = GUI_BUTTON_RIGHT,
                                            .mods = mods});
        }
        else if ((args->event_x_event_pointer->xbutton.button == Button4) ||
                 (args->event_x_event_pointer->xbutton.button == Button5))
        {
            EMBED_CALL(
                args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                .input = GUI_WHEEL_INPUT(args->event_x_event_pointer->xbutton.x, args->event_x_event_pointer->xbutton.y,
                                         (args->event_x_event_pointer->xbutton.button == Button5) ? GUI_WHEEL_ROWS
                                                                                                  : -GUI_WHEEL_ROWS));
        }
        break;
    case ButtonRelease:
        if (args->event_x_event_pointer->xbutton.button == Button1)
        {
            EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                       .input = &(GuiInput){.type = GUI_INPUT_UP,
                                            .x = args->event_x_event_pointer->xbutton.x,
                                            .y = args->event_x_event_pointer->xbutton.y,
                                            .mods = EMBED_CALL(system_mods, SystemX11Ctx,
                                                               .state = args->event_x_event_pointer->xbutton.state)});
        }
        break;
    case ClientMessage:
        if ((args->event_x_event_pointer->xclient.message_type == args->system->protocols) &&
            ((Atom)args->event_x_event_pointer->xclient.data.l[0] == args->system->delete_window))
        {
            args->system->events->closing(args->system->context);
        }
        break;
    default:
        break;
    }
}

void gui_system_display_max(const GuiSystemWidthCfg *args)
{
    unsigned int *width = args->width;
    unsigned int *height = args->height;
    *width = 0u;
    *height = 0u;
    Display *const display = XOpenDisplay(NULL);
    if (display == NULL)
    {
        return;
    }
    const int screen = DefaultScreen(display);
    // a screen's sides are not negative
    *width = (unsigned int)DisplayWidth(display, screen);
    *height = (unsigned int)DisplayHeight(display, screen);
    XCloseDisplay(display);
}

unsigned int gui_system_areas(const GuiSystemAreasCfg *args)
{
    Display *const display = XOpenDisplay(NULL);
    if ((display == NULL) || (args->most == 0u))
    {
        return 0u;
    }
    const int screen = DefaultScreen(display);
    // a screen's sides are not negative
    args->areas[0] = (CliWindowArea){0, 0, (unsigned int)DisplayWidth(display, screen),
                                     (unsigned int)DisplayHeight(display, screen)};
    XCloseDisplay(display);
    return 1u;
}

// a window made at `place` on the record's connection, undecorated and with its protocols and least size, not yet
// mapped
EMBED_INLINE void system_window_init(const SystemX11Ctx *args)
{
    args->system->screen = DefaultScreen(args->system->display);
    const Window root = RootWindow(args->system->display, args->system->screen);
    XSetWindowAttributes attributes;
    attributes.background_pixel = 0x131331UL;
    attributes.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
                            PointerMotionMask | LeaveWindowMask | KeyPressMask | KeyReleaseMask;
    args->system->window = XCreateWindow(args->system->display, root, args->place->x, args->place->y,
                                         args->place->width, args->place->height, 0, CopyFromParent, InputOutput,
                                         CopyFromParent, CWBackPixel | CWEventMask, &attributes);
    // the decorations taken off: the hints' flags word says the decorations word counts, and it is zero
    const Atom motif_hints = XInternAtom(args->system->display, "_MOTIF_WM_HINTS", False);
    const unsigned long hints[5] = {GUI_MOTIF_HINTS_DECORATIONS, 0UL, 0UL, 0UL, 0UL};
    XChangeProperty(args->system->display, args->system->window, motif_hints, motif_hints, 32, PropModeReplace,
                    (const unsigned char *)hints, 5);
    args->system->protocols = XInternAtom(args->system->display, "WM_PROTOCOLS", False);
    args->system->delete_window = XInternAtom(args->system->display, "WM_DELETE_WINDOW", False);
    args->system->moveresize = XInternAtom(args->system->display, "_NET_WM_MOVERESIZE", False);
    args->system->state = XInternAtom(args->system->display, "_NET_WM_STATE", False);
    args->system->state_maximized_horizontal =
        XInternAtom(args->system->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    args->system->state_maximized_vertical = XInternAtom(args->system->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    XSetWMProtocols(args->system->display, args->system->window, &args->system->delete_window, 1);
    XStoreName(args->system->display, args->system->window, args->title);
    // the least size the window manager drags the window to; a size is far inside an int
    XSizeHints min;
    min.flags = PMinSize;
    min.min_width = (int)CLI_POSITION_MIN_WIDTH;
    min.min_height = (int)CLI_POSITION_MIN_HEIGHT;
    XSetWMNormalHints(args->system->display, args->system->window, &min);
}

GuiSystem *gui_system_open(const GuiSystemPositionTitleCfg *args)
{
    const GuiSystemEvents *events = args->events;
    void *context = args->context;
    GuiSystem *const system = system_record_take();
    if (system == NULL)
    {
        mmgr_line_put(args->err_msg, "the program's two windows already stand");
        return NULL;
    }
    system->events = events;
    system->context = context;
    system->display = XOpenDisplay(NULL);
    if (system->display == NULL)
    {
        system->taken = 0;
        mmgr_line_put(args->err_msg, "no X display is open to the window");
        return NULL;
    }
    if (pipe(system->wake_pipe) != 0)
    {
        XCloseDisplay(system->display);
        system->taken = 0;
        mmgr_line_put(args->err_msg, "the pipe the window's thread is woken through could not be made");
        return NULL;
    }
    EMBED_CALL(system_window_init, SystemX11Ctx, .system = system, .place = args->place, .title = args->title);
    EMBED_CALL(system_settings_watch, SystemX11Ctx, .system = system);
    XMapWindow(system->display, system->window);
    if (args->place->maximized)
    {
        EMBED_CALL(gui_system_table.act, GuiSystemCfg, .system = system, .act = GUI_ACT_MAXIMIZE);
    }
    XFlush(system->display);
    return system;
}

GuiSystem *gui_system_owner_open(const GuiSystemPositionTitleCfg *args)
{
    GuiSystem *owner = args->owner;
    const GuiSystemEvents *events = args->events;
    void *context = args->context;
    GuiSystem *const system = system_record_take();
    if (system == NULL)
    {
        mmgr_line_put(args->err_msg, "the program's two windows already stand");
        return NULL;
    }
    system->events = events;
    system->context = context;
    system->display = owner->display;
    system->owner = owner;
    EMBED_CALL(system_window_init, SystemX11Ctx, .system = system, .place = args->place, .title = args->title);
    // the window manager saves a transient window in front of the one it is for
    XSetTransientForHint(system->display, system->window, owner->window);
    owner->owned = system;
    XMapWindow(system->display, system->window);
    XFlush(system->display);
    return system;
}

void gui_system_run(const GuiSystemCfg *args)
{
    struct pollfd waited[2];
    waited[0].fd = ConnectionNumber(args->system->display);
    waited[0].events = POLLIN;
    waited[1].fd = args->system->wake_pipe[0];
    waited[1].events = POLLIN;
    while (!args->system->closed)
    {
        while (!args->system->closed && (XPending(args->system->display) > 0))
        {
            XEvent event;
            XNextEvent(args->system->display, &event);
            EMBED_CALL(system_event, SystemX11Ctx, .system = args->system, .event_x_event_pointer = &event);
        }
        if (args->system->closed)
        {
            break;
        }
        waited[0].revents = 0;
        waited[1].revents = 0;
        if (poll(waited, 2, -1) < 0)
        {
            continue;
        }
        if (waited[1].revents & POLLIN)
        {
            char drained[64];
            while (read(args->system->wake_pipe[0], drained, sizeof(drained)) == (ssize_t)sizeof(drained))
            {
                // each pass empties the pipe of as much as it reads
            }
            args->system->events->woken(args->system->context);
        }
    }
}

void gui_system_pumped(const GuiSystemCfg *args)
{
    while (!args->system->closed && (XPending(args->system->display) > 0))
    {
        XEvent event;
        XNextEvent(args->system->display, &event);
        EMBED_CALL(system_event, SystemX11Ctx, .system = args->system, .event_x_event_pointer = &event);
    }
}

void *gui_system_native(const GuiSystemPositionCfg *args)
{
    return (void *)(unsigned long)args->system_read->window;
}

void gui_system_close(const GuiSystemCfg *args)
{
    args->system->closed = 1;
    if (args->system->owner != NULL)
    {
        // the connection is the owner's and stays open
        args->system->owner->owned = NULL;
        XDestroyWindow(args->system->display, args->system->window);
        XFlush(args->system->display);
        args->system->display = NULL;
        return;
    }
    if (args->system->display != NULL)
    {
        XDestroyWindow(args->system->display, args->system->window);
        XCloseDisplay(args->system->display);
        args->system->display = NULL;
    }
}

void gui_system_free(const GuiSystemCfg *args)
{
    if (args->system->owner == NULL)
    {
        close(args->system->wake_pipe[0]);
        close(args->system->wake_pipe[1]);
    }
    args->system->taken = 0;
}

void gui_system_present(const GuiSystemRasterCfg *args)
{
    args->system->displayed = *args->raster;
    EMBED_CALL(system_append, SystemX11Ctx, .system = args->system, .from = 0, .rows = args->raster->height);
}

void gui_system_present_part(const GuiSystemRasterCfg *args)
{
    args->system->displayed = *args->raster;
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->part) && (args->part.y >= 0) &&
        (args->part.y < (int)args->raster->height))
    {
        // the part's rows are inside the raster, and far inside what an unsigned int counts
        const unsigned int from = (unsigned int)args->part.y;
        const unsigned int rows = ((unsigned int)args->part.height < args->raster->height - from)
                                      ? (unsigned int)args->part.height
                                      : args->raster->height - from;
        EMBED_CALL(system_append, SystemX11Ctx, .system = args->system, .from = from, .rows = rows);
    }
}

void gui_system_wake(const GuiSystemCfg *args)
{
    const char one = 1;
    (void)write(args->system->wake_pipe[1], &one, 1u);
}

// `raster`'s pixels put after `at` of `data` as the icon property lays them, the width, the height and then each
// pixel as a word: the words put
EMBED_INLINE unsigned long system_icon_append(const SystemX11Ctx *args)
{
    args->data[0] = args->raster->width;
    args->data[1] = args->raster->height;
    const unsigned long pixels = (unsigned long)args->raster->width * args->raster->height;
    for (unsigned long at = 0UL; at < pixels; at += 1UL)
    {
        args->data[2UL + at] = args->raster->pixels[at];
    }
    return 2UL + pixels;
}

void gui_system_icon(const GuiSystemTitleIconCfg *args)
{
    const unsigned long words = 4UL + ((unsigned long)args->title_icon->width * args->title_icon->height) +
                                ((unsigned long)args->taskbar_icon->width * args->taskbar_icon->height);
    if (words > (sizeof(s_icon_words) / sizeof(s_icon_words[0])))
    {
        return;
    }
    const unsigned long put =
        EMBED_CALL(system_icon_append, SystemX11Ctx, .data = s_icon_words, .raster = args->title_icon);
    (void)EMBED_CALL(system_icon_append, SystemX11Ctx, .data = s_icon_words + put, .raster = args->taskbar_icon);
    const Atom icon = XInternAtom(args->system->display, "_NET_WM_ICON", False);
    // the words are far fewer than an int counts
    XChangeProperty(args->system->display, args->system->window, icon, XA_CARDINAL, 32, PropModeReplace,
                    (const unsigned char *)s_icon_words, (int)words);
    XFlush(args->system->display);
}

void gui_system_act(const GuiSystemCfg *args)
{
    if (args->act == GUI_ACT_MINIMIZE)
    {
        XIconifyWindow(args->system->display, args->system->window, args->system->screen);
    }
    else if (args->act == GUI_ACT_MAXIMIZE)
    {
        // the maximized state toggled, the two atoms the window manager reads as both
        XEvent event;
        memor.set(&(MemoriaCfg){.dst = &event, .val = 0, .bytes = sizeof(event)});
        event.xclient.type = ClientMessage;
        event.xclient.window = args->system->window;
        event.xclient.message_type = args->system->state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 2L;
        event.xclient.data.l[1] = (long)args->system->state_maximized_horizontal;
        event.xclient.data.l[2] = (long)args->system->state_maximized_vertical;
        event.xclient.data.l[3] = 1L;
        XSendEvent(args->system->display, RootWindow(args->system->display, args->system->screen), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }
    else
    {
        args->system->events->closing(args->system->context);
    }
    if (args->system->display != NULL)
    {
        XFlush(args->system->display);
    }
}

void gui_system_on_position(const GuiSystemPositionCfg *args)
{
    XWindowAttributes attributes;
    XGetWindowAttributes(args->system_read->display, args->system_read->window, &attributes);
    Window child;
    int x = 0;
    int y = 0;
    XTranslateCoordinates(args->system_read->display, args->system_read->window,
                          RootWindow(args->system_read->display, args->system_read->screen), 0, 0, &x, &y, &child);
    args->position_out->x = x;
    args->position_out->y = y;
    // a window's sides are not negative
    args->position_out->width = (unsigned int)attributes.width;
    args->position_out->height = (unsigned int)attributes.height;
    args->position_out->maximized = 0;
    Atom kind;
    int format = 0;
    unsigned long count = 0UL;
    unsigned long left = 0UL;
    unsigned char *states = NULL;
    if ((XGetWindowProperty(args->system_read->display, args->system_read->window, args->system_read->state, 0L, 32L,
                            False, XA_ATOM, &kind, &format, &count, &left, &states) == Success) &&
        (states != NULL))
    {
        const Atom *const each = (const Atom *)states;
        for (unsigned long at = 0UL; at < count; at += 1UL)
        {
            args->position_out->maximized =
                args->position_out->maximized || (each[at] == args->system_read->state_maximized_vertical);
        }
        XFree(states);
    }
}

int gui_system_scale(const GuiSystemPositionCfg *args)
{
    return EMBED_CALL(system_scale_get, SystemX11Ctx, .display = args->system_read->display,
                      .screen = args->system_read->screen);
}
