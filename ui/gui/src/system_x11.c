// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system_x11.c: the window on X11. A window with its decorations taken off through the Motif hints, moved and resized
// by the window manager through _NET_WM_MOVERESIZE as the pointer presses on the caption or a border; the raster put
// into the window as a 32-bit image, a tick coming no sooner than the screen's next refresh after it; the thread
// woken through a pipe the loop waits on beside the display's connection; the scale read from the screen's size in
// millimetres; the screen the one area
#include "system.h"

#include "../../cli/src/command_line/cli_internal.h"

// the tree names a structure Atom in its engine's headers, which the CLI's headers include; X's Atom is renamed
// X11Atom here, in X's own headers and in this file alike
#define Atom X11Atom
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

// the rows one notch of the wheel scrolls; the Motif hints that take the decorations off, their flags word and the
// decorations word; and the window manager's move and resize directions, as the specification numbers them, and its
// request to move
#define GUI_WHEEL_ROWS 3
#define GUI_MOTIF_HINTS_DECORATIONS 2ul
#define GUI_MOVERESIZE_MOVE 8l
// the milliseconds of one refresh of the screen, taken as 60 a second: the core protocol carries no rate
#define GUI_FRAME_MS 17ull

struct GuiSystem
{
    Display *display;
    Window window;
    int screen;
    const GuiSystemEvents *events;
    void *context;
    const GuiRaster *shown;
    int wake_pipe[2];
    int closed;
    unsigned long long tick_due;
    unsigned long long presented_ms;
    unsigned int width;
    unsigned int height;
    Atom delete_window;
    Atom protocols;
    Atom moveresize;
    Atom state;
    Atom state_maximized_horizontal;
    Atom state_maximized_vertical;
};

// the scale of the screen: its pixels an inch over 96
static float system_scale_of(Display *display, int screen)
{
    const int pixels = DisplayWidth(display, screen);
    const int millimetres = DisplayWidthMM(display, screen);
    if ((pixels <= 0) || (millimetres <= 0))
    {
        return 1.0f;
    }
    const float scale = ((float)pixels * 25.4f) / ((float)millimetres * 96.0f);
    // a scale a sliver off one is one, as a screen reports its size roughly
    return ((scale > 0.9f) && (scale < 1.1f)) ? 1.0f : scale;
}

// the raster shown put into the window
// the raster shown put into the window, its `rows` rows from the row `from`
static void system_put(GuiSystem *system, unsigned int from, unsigned int rows)
{
    const GuiRaster *const raster = system->shown;
    if ((raster == NULL) || (raster->pixels == NULL))
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
    image.red_mask = 0xff0000ul;
    image.green_mask = 0x00ff00ul;
    image.blue_mask = 0x0000fful;
    XInitImage(&image);
    GC context = XCreateGC(system->display, system->window, 0ul, NULL);
    // the rows are inside the raster, whose sides are far inside what an int represents
    XPutImage(system->display, system->window, context, &image, 0, (int)from, 0, (int)from, raster->width, rows);
    XFreeGC(system->display, context);
    XFlush(system->display);
}

// the window manager asked to move or resize the window from the pointer at x, y of the root, `direction` as the
// specification numbers the borders, or the move
static void system_moveresize(GuiSystem *system, int root_x, int root_y, long direction)
{
    XEvent event;
    memor.set(&(MemoriaCfg){.dst = &event, .val = 0, .bytes = sizeof(event)});
    event.xclient.type = ClientMessage;
    event.xclient.window = system->window;
    event.xclient.message_type = system->moveresize;
    event.xclient.format = 32;
    event.xclient.data.l[0] = root_x;
    event.xclient.data.l[1] = root_y;
    event.xclient.data.l[2] = direction;
    event.xclient.data.l[3] = Button1;
    event.xclient.data.l[4] = 1;
    XUngrabPointer(system->display, CurrentTime);
    XSendEvent(system->display, RootWindow(system->display, system->screen), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(system->display);
}

// the modifier keys down in an event's state, as system.h names them
static unsigned int system_mods(unsigned int state)
{
    return ((state & ShiftMask) ? GUI_MOD_SHIFT : 0u) | ((state & ControlMask) ? GUI_MOD_CONTROL : 0u) |
           ((state & Mod1Mask) ? GUI_MOD_ALT : 0u);
}

// a key press passed to the window: a key with a name of the page's editor's, the modifier keys down written before
// it, and a letter typed as text where the key maps to one and the control key is not down
static void system_key_pressed(GuiSystem *system, XKeyEvent *event)
{
    static const struct
    {
        KeySym key;
        const char *name;
    } s_named[] = {{XK_Left, "Left"},       {XK_Right, "Right"},    {XK_Up, "Up"},           {XK_Down, "Down"},
                   {XK_Home, "Home"},       {XK_End, "End"},        {XK_Page_Up, "PageUp"},  {XK_Page_Down, "PageDown"},
                   {XK_BackSpace, "Backspace"}, {XK_Delete, "Delete"}, {XK_Return, "Enter"}, {XK_KP_Enter, "Enter"},
                   {XK_Tab, "Tab"},         {XK_ISO_Left_Tab, "Tab"}, {XK_Escape, "Escape"}, {XK_space, "Space"},
                   {XK_Insert, "Insert"}};
    char text[32];
    KeySym key = NoSymbol;
    const int wrote = XLookupString(event, text, sizeof(text) - 1, &key, NULL);
    const unsigned int mods = system_mods(event->state);
    char own[32];
    VerbaLinea named = {own, sizeof(own), 0ull};
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
        mmgr_line_decimal(&named, (unsigned long long)(key - XK_F1) + 1ull, 1u);
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
    if (!has_name && (key < 0x80ul) && (key > XK_space))
    {
        // a punctuation key goes by its own character
        mmgr_line_ch(&named, (char)key);
        has_name = 1;
    }
    if (has_name && (mmgr_line_finish(&named) != 0ull))
    {
        char whole[48];
        VerbaLinea line = {whole, sizeof(whole), 0ull};
        mmgr_line_put(&line, (mods & GUI_MOD_CONTROL) ? "Mod+" : "");
        mmgr_line_put(&line, (mods & GUI_MOD_ALT) ? "Alt+" : "");
        mmgr_line_put(&line, (mods & GUI_MOD_SHIFT) ? "Shift+" : "");
        mmgr_line_put(&line, own);
        (void)mmgr_line_finish(&line);
        system->events->key(system->context, whole);
    }
    if ((wrote > 0) && !(mods & GUI_MOD_CONTROL) && ((unsigned char)text[0] >= 0x20u) && ((unsigned char)text[0] != 0x7fu))
    {
        // the text as Latin-1, each byte a letter, written as UTF-8
        char utf8[128];
        VerbaLinea line = {utf8, sizeof(utf8), 0ull};
        for (int at = 0; at < wrote; at += 1)
        {
            cli_utf8_put(&line, (unsigned char)text[at]);
        }
        system->events->text(system->context, utf8, line.at);
    }
}

// the move or resize direction a hit stands for, or -1 for none
static long system_direction(unsigned int hit)
{
    static const long s_directions[13] = {-1l, GUI_MOVERESIZE_MOVE, 7l, 3l, 1l, 5l, 0l, 2l, 6l, 4l, -1l, -1l, -1l};
    return (hit < 13u) ? s_directions[hit] : -1l;
}

// one event of the display's taken
static void system_event(GuiSystem *system, XEvent *event)
{
    switch (event->type)
    {
    case Expose:
        if (event->xexpose.count == 0)
        {
            system_put(system, 0u, (system->shown != NULL) ? system->shown->height : 0u);
        }
        break;
    case ConfigureNotify:
        // a window's sides are not negative
        if (((unsigned int)event->xconfigure.width != system->width) ||
            ((unsigned int)event->xconfigure.height != system->height))
        {
            system->width = (unsigned int)event->xconfigure.width;
            system->height = (unsigned int)event->xconfigure.height;
            system->events->sized(system->context, system->width, system->height,
                                  system_scale_of(system->display, system->screen));
        }
        break;
    case MapNotify:
        system->events->shown(system->context, 1);
        break;
    case UnmapNotify:
        system->events->shown(system->context, 0);
        break;
    case MotionNotify:
        system->events->pointer(system->context, GUI_POINTER_MOVED, event->xmotion.x, event->xmotion.y, 0u,
                                system_mods(event->xmotion.state));
        break;
    case KeyPress:
        system_key_pressed(system, &event->xkey);
        break;
    case ButtonPress:
        if (event->xbutton.button == Button1)
        {
            const long direction = system_direction(system->events->hit(system->context, event->xbutton.x, event->xbutton.y));
            if (direction >= 0l)
            {
                system_moveresize(system, event->xbutton.x_root, event->xbutton.y_root, direction);
            }
            else
            {
                system->events->pointer(system->context, GUI_POINTER_DOWN, event->xbutton.x, event->xbutton.y, 0u,
                                        system_mods(event->xbutton.state));
            }
        }
        else if ((event->xbutton.button == Button4) || (event->xbutton.button == Button5))
        {
            system->events->wheel(system->context, event->xbutton.x, event->xbutton.y,
                                  (event->xbutton.button == Button5) ? GUI_WHEEL_ROWS : -GUI_WHEEL_ROWS);
        }
        break;
    case ButtonRelease:
        if (event->xbutton.button == Button1)
        {
            system->events->pointer(system->context, GUI_POINTER_UP, event->xbutton.x, event->xbutton.y, 0u,
                                    system_mods(event->xbutton.state));
        }
        break;
    case ClientMessage:
        if ((event->xclient.message_type == system->protocols) &&
            ((Atom)event->xclient.data.l[0] == system->delete_window))
        {
            system->events->closing(system->context);
        }
        break;
    default:
        break;
    }
}

void gui_system_display_most(unsigned int *width, unsigned int *height)
{
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

unsigned int gui_system_areas(CliWindowArea *areas, unsigned int most)
{
    Display *const display = XOpenDisplay(NULL);
    if ((display == NULL) || (most == 0u))
    {
        return 0u;
    }
    const int screen = DefaultScreen(display);
    // a screen's sides are not negative
    areas[0] = (CliWindowArea){0, 0, (unsigned int)DisplayWidth(display, screen), (unsigned int)DisplayHeight(display, screen)};
    XCloseDisplay(display);
    return 1u;
}

GuiSystem *gui_system_opened(const CliWindowPlace *place, const char *title, const GuiSystemEvents *events,
                             void *context, VerbaLinea *err_msg)
{
    GuiSystem *const system = g_cli_cells->persistent_buf_alloc(sizeof(*system), &g_gui_system_cells);
    if (system == NULL)
    {
        mmgr_line_put(err_msg, "the system window's cell could not be taken");
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = system, .val = 0, .bytes = sizeof(*system)});
    system->events = events;
    system->context = context;
    system->display = XOpenDisplay(NULL);
    if (system->display == NULL)
    {
        g_cli_cells->persistent_buf_release(system, &g_gui_system_cells);
        mmgr_line_put(err_msg, "no X display is open to the window");
        return NULL;
    }
    if (pipe(system->wake_pipe) != 0)
    {
        XCloseDisplay(system->display);
        g_cli_cells->persistent_buf_release(system, &g_gui_system_cells);
        mmgr_line_put(err_msg, "the pipe the window's thread is woken through could not be made");
        return NULL;
    }
    system->screen = DefaultScreen(system->display);
    const Window root = RootWindow(system->display, system->screen);
    XSetWindowAttributes attributes;
    attributes.background_pixel = 0x131331ul;
    attributes.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
                            PointerMotionMask | KeyPressMask | KeyReleaseMask;
    system->window = XCreateWindow(system->display, root, place->x, place->y, place->width, place->height, 0,
                                   CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &attributes);
    // the decorations taken off: the hints' flags word says the decorations word counts, and it is zero
    const Atom motif_hints = XInternAtom(system->display, "_MOTIF_WM_HINTS", False);
    const unsigned long hints[5] = {GUI_MOTIF_HINTS_DECORATIONS, 0ul, 0ul, 0ul, 0ul};
    XChangeProperty(system->display, system->window, motif_hints, motif_hints, 32, PropModeReplace,
                    (const unsigned char *)hints, 5);
    system->protocols = XInternAtom(system->display, "WM_PROTOCOLS", False);
    system->delete_window = XInternAtom(system->display, "WM_DELETE_WINDOW", False);
    system->moveresize = XInternAtom(system->display, "_NET_WM_MOVERESIZE", False);
    system->state = XInternAtom(system->display, "_NET_WM_STATE", False);
    system->state_maximized_horizontal = XInternAtom(system->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    system->state_maximized_vertical = XInternAtom(system->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    XSetWMProtocols(system->display, system->window, &system->delete_window, 1);
    XStoreName(system->display, system->window, title);
    XMapWindow(system->display, system->window);
    if (place->maximized)
    {
        gui_system_act(system, GUI_ACT_MAXIMIZE);
    }
    XFlush(system->display);
    return system;
}

void gui_system_run(GuiSystem *system)
{
    struct pollfd waited[2];
    waited[0].fd = ConnectionNumber(system->display);
    waited[0].events = POLLIN;
    waited[1].fd = system->wake_pipe[0];
    waited[1].events = POLLIN;
    while (!system->closed)
    {
        while (!system->closed && (XPending(system->display) > 0))
        {
            XEvent event;
            XNextEvent(system->display, &event);
            system_event(system, &event);
        }
        if (system->closed)
        {
            break;
        }
        system->events->idle(system->context);
        if (XPending(system->display) > 0)
        {
            continue;
        }
        waited[0].revents = 0;
        waited[1].revents = 0;
        // the wait ends at the tick asked for, where one is
        int wait = -1;
        if (system->tick_due != 0ull)
        {
            const unsigned long long now = cli_clock_milliseconds();
            // the milliseconds to a tick are far inside what an int represents
            wait = (system->tick_due > now) ? (int)(system->tick_due - now) : 0;
        }
        if (poll(waited, 2, wait) < 0)
        {
            continue;
        }
        if (waited[1].revents & POLLIN)
        {
            char drained[64];
            while (read(system->wake_pipe[0], drained, sizeof(drained)) == (ssize_t)sizeof(drained))
            {
            }
            system->events->woken(system->context);
        }
        if ((system->tick_due != 0ull) && (cli_clock_milliseconds() >= system->tick_due))
        {
            system->tick_due = 0ull;
            system->events->ticked(system->context);
        }
    }
}

void gui_system_tick(GuiSystem *system, unsigned long long after)
{
    if (after == 0ull)
    {
        system->tick_due = 0ull;
        return;
    }
    // no sooner than the screen's next refresh after the last frame presented
    const unsigned long long now = cli_clock_milliseconds();
    const unsigned long long since = now - system->presented_ms;
    const unsigned long long until_refresh = (since < GUI_FRAME_MS) ? GUI_FRAME_MS - since : 0ull;
    system->tick_due = now + ((after > until_refresh) ? after : until_refresh);
}

void gui_system_pumped(GuiSystem *system)
{
    while (!system->closed && (XPending(system->display) > 0))
    {
        XEvent event;
        XNextEvent(system->display, &event);
        system_event(system, &event);
    }
}

void *gui_system_native(const GuiSystem *system)
{
    return (void *)(unsigned long)system->window;
}

void gui_system_closed(GuiSystem *system)
{
    system->closed = 1;
    if (system->display != NULL)
    {
        XDestroyWindow(system->display, system->window);
        XCloseDisplay(system->display);
        system->display = NULL;
    }
}

void gui_system_given(GuiSystem *system)
{
    close(system->wake_pipe[0]);
    close(system->wake_pipe[1]);
    g_cli_cells->persistent_buf_release(system, &g_gui_system_cells);
}

void *gui_system_pages(unsigned long long bytes)
{
    // the pages are the C library's here, zero; no surface of the system's reads them where they lie
    return calloc(1u, (size_t)bytes);
}

void gui_system_pages_given(void *pages, unsigned long long bytes)
{
    (void)bytes;
    free(pages);
}

void gui_system_present(GuiSystem *system, const GuiRaster *raster)
{
    system->shown = raster;
    system_put(system, 0, raster->height);
    system->presented_ms = cli_clock_milliseconds();
}

void gui_system_present_part(GuiSystem *system, const GuiRaster *raster, GuiRect part)
{
    system->shown = raster;
    if (!gui_rect_empty(part) && (part.y >= 0) && (part.y < (int)raster->height))
    {
        // the part's rows are inside the raster, and far inside what an unsigned int counts
        const unsigned int from = (unsigned int)part.y;
        const unsigned int rows = ((unsigned int)part.height < raster->height - from) ? (unsigned int)part.height : raster->height - from;
        system_put(system, from, rows);
    }
    system->presented_ms = cli_clock_milliseconds();
}

void gui_system_wake(GuiSystem *system)
{
    const char one = 1;
    (void)write(system->wake_pipe[1], &one, 1u);
}

// `raster`'s pixels put after `at` of `data` as the icon property lays them, the width, the height and then each
// pixel as a word: the words put
static unsigned long system_icon_put(unsigned long *data, const GuiRaster *raster)
{
    data[0] = raster->width;
    data[1] = raster->height;
    const unsigned long pixels = (unsigned long)raster->width * raster->height;
    for (unsigned long at = 0ul; at < pixels; at += 1ul)
    {
        data[2ul + at] = raster->pixels[at];
    }
    return 2ul + pixels;
}

void gui_system_icon(GuiSystem *system, const GuiRaster *title_icon, const GuiRaster *taskbar_icon)
{
    const unsigned long words = 4ul + ((unsigned long)title_icon->width * title_icon->height) + ((unsigned long)taskbar_icon->width * taskbar_icon->height);
    unsigned long *const data = g_cli_cells->temporary_buf_alloc(words * sizeof(*data), &g_gui_system_cells);
    if (data == NULL)
    {
        return;
    }
    const unsigned long put = system_icon_put(data, title_icon);
    (void)system_icon_put(data + put, taskbar_icon);
    const Atom icon = XInternAtom(system->display, "_NET_WM_ICON", False);
    // the words are far fewer than an int counts
    XChangeProperty(system->display, system->window, icon, XA_CARDINAL, 32, PropModeReplace, (const unsigned char *)data, (int)words);
    XFlush(system->display);
    g_cli_cells->persistent_buf_release(data, &g_gui_system_cells);
}

void gui_system_act(GuiSystem *system, unsigned int act)
{
    if (act == GUI_ACT_MINIMIZE)
    {
        XIconifyWindow(system->display, system->window, system->screen);
    }
    else if (act == GUI_ACT_MAXIMIZE)
    {
        // the maximized state toggled, the two atoms the window manager reads as both
        XEvent event;
        memor.set(&(MemoriaCfg){.dst = &event, .val = 0, .bytes = sizeof(event)});
        event.xclient.type = ClientMessage;
        event.xclient.window = system->window;
        event.xclient.message_type = system->state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 2l;
        event.xclient.data.l[1] = (long)system->state_maximized_horizontal;
        event.xclient.data.l[2] = (long)system->state_maximized_vertical;
        event.xclient.data.l[3] = 1l;
        XSendEvent(system->display, RootWindow(system->display, system->screen), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }
    else
    {
        system->events->closing(system->context);
    }
    if (system->display != NULL)
    {
        XFlush(system->display);
    }
}

void gui_system_placed(const GuiSystem *system, CliWindowPlace *place)
{
    XWindowAttributes attributes;
    XGetWindowAttributes(system->display, system->window, &attributes);
    Window child;
    int x = 0;
    int y = 0;
    XTranslateCoordinates(system->display, system->window, RootWindow(system->display, system->screen), 0, 0, &x, &y,
                          &child);
    place->x = x;
    place->y = y;
    // a window's sides are not negative
    place->width = (unsigned int)attributes.width;
    place->height = (unsigned int)attributes.height;
    place->maximized = 0;
    Atom kind;
    int format = 0;
    unsigned long count = 0ul;
    unsigned long left = 0ul;
    unsigned char *states = NULL;
    if ((XGetWindowProperty(system->display, system->window, system->state, 0l, 32l, False, XA_ATOM, &kind, &format,
                            &count, &left, &states) == Success) &&
        (states != NULL))
    {
        const Atom *const each = (const Atom *)states;
        for (unsigned long at = 0ul; at < count; at += 1ul)
        {
            place->maximized = place->maximized || (each[at] == system->state_maximized_vertical);
        }
        XFree(states);
    }
}

float gui_system_scale(const GuiSystem *system)
{
    return system_scale_of(system->display, system->screen);
}
