// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system_windows.c: the window on Windows. A popup window with a sizing frame whose non-client area is nothing, so the
// client is the whole window and the system still drags, snaps and resizes it by what the window answers to the
// hit test, the compositor rounding its corners and drawing its one-pixel border; the raster blitted into the client
// as a top-down 32-bit bitmap, a frame drawn at each size the system drags it to and at each tick, inside the
// system's own drag loop too, and a tick coming no sooner than the monitor's next refresh after the last frame; the
// thread woken by a message posted to the window; the scale and the refresh rate read from the window's monitor
#define _WIN32_WINNT 0x0A00
#define UNICODE
#define _UNICODE
#include "system.h"

#include "../../cli/src/command_line/cli_internal.h"

#include <windows.h>

#include <dwmapi.h>
#include <windowsx.h>

static CellOwner s_owner = MMGR_CELL_OWNER("gui_system");

// the message another thread posts to wake the window's thread
#define GUI_WAKE_MESSAGE (WM_APP + 1u)
// the rows one notch of the wheel scrolls
#define GUI_WHEEL_ROWS 3
// the timer the window's tick comes on
#define GUI_TICK_TIMER 1u

struct GuiSystem
{
    HWND window;
    const GuiSystemEvents *events;
    void *context;
    const GuiRaster *shown;
    int closed;
    unsigned short high_surrogate;
    unsigned int mods;
    unsigned long long presented_ms;
    unsigned int frame_ms;
};

// the milliseconds of one refresh of `window`'s monitor, whole, at least one; 1000 / 60 where the monitor does not
// say its rate
static unsigned int system_frame_ms_of(HWND window)
{
    MONITORINFOEXW info;
    memor.set(&(MemoriaCfg){.dst = &info, .val = 0, .bytes = sizeof(info)});
    info.cbSize = sizeof(info);
    DEVMODEW mode;
    memor.set(&(MemoriaCfg){.dst = &mode, .val = 0, .bytes = sizeof(mode)});
    mode.dmSize = sizeof(mode);
    unsigned int hertz = 60u;
    if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), (MONITORINFO *)&info) &&
        EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) && (mode.dmDisplayFrequency > 1u))
    {
        hertz = mode.dmDisplayFrequency;
    }
    return (1000u + hertz - 1u) / hertz;
}

// the modifier keys down, as system.h names them: read from the keys' own messages as they come, which gives a key
// message posted to the window the modifiers posted before it, and from the keyboard's state as the window takes
// the focus
static unsigned int system_mods(const GuiSystem *system)
{
    return system->mods;
}

static unsigned int system_mods_of_keyboard(void)
{
    return ((GetKeyState(VK_SHIFT) < 0) ? GUI_MOD_SHIFT : 0u) | ((GetKeyState(VK_CONTROL) < 0) ? GUI_MOD_CONTROL : 0u) |
           ((GetKeyState(VK_MENU) < 0) ? GUI_MOD_ALT : 0u);
}

// a modifier key's message applied to the modifier keys down: 1 where the key is a modifier, else 0
static int system_modifier_keyed(GuiSystem *system, WPARAM key, int down)
{
    const unsigned int bit = ((key == VK_SHIFT) || (key == VK_LSHIFT) || (key == VK_RSHIFT))       ? GUI_MOD_SHIFT
                             : ((key == VK_CONTROL) || (key == VK_LCONTROL) || (key == VK_RCONTROL)) ? GUI_MOD_CONTROL
                             : ((key == VK_MENU) || (key == VK_LMENU) || (key == VK_RMENU))          ? GUI_MOD_ALT
                                                                                                   : 0u;
    if (bit == 0u)
    {
        return 0;
    }
    system->mods = down ? (system->mods | bit) : (system->mods & ~bit);
    return 1;
}

// a virtual key's name as the page's editor names a key, into `name`, which is sized for `room`: 1, or 0 for a key
// that has no name, a modifier among them
static int system_key_named(WPARAM key, char *name, unsigned long long room)
{
    static const struct
    {
        WPARAM key;
        const char *name;
    } s_named[] = {{VK_LEFT, "Left"},         {VK_RIGHT, "Right"},     {VK_UP, "Up"},       {VK_DOWN, "Down"},
                   {VK_HOME, "Home"},         {VK_END, "End"},         {VK_PRIOR, "PageUp"}, {VK_NEXT, "PageDown"},
                   {VK_BACK, "Backspace"},    {VK_DELETE, "Delete"},   {VK_RETURN, "Enter"}, {VK_TAB, "Tab"},
                   {VK_ESCAPE, "Escape"},     {VK_SPACE, "Space"},     {VK_INSERT, "Insert"}, {VK_OEM_4, "["},
                   {VK_OEM_6, "]"},           {VK_OEM_2, "/"},         {VK_OEM_5, "\\"},     {VK_OEM_PERIOD, "."},
                   {VK_OEM_COMMA, ","},       {VK_OEM_1, ";"},         {VK_OEM_7, "'"},      {VK_OEM_3, "`"},
                   {VK_OEM_MINUS, "-"},       {VK_OEM_PLUS, "="}};
    VerbaLinea line = {name, room, 0ull};
    for (unsigned int at = 0u; at < sizeof(s_named) / sizeof(s_named[0]); at += 1u)
    {
        if (s_named[at].key == key)
        {
            mmgr_line_put(&line, s_named[at].name);
            return mmgr_line_finish(&line) != 0ull;
        }
    }
    if (((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')))
    {
        // a letter's or a digit's virtual key is its own character
        mmgr_line_ch(&line, (char)key);
        return mmgr_line_finish(&line) != 0ull;
    }
    if ((key >= VK_F1) && (key <= VK_F24))
    {
        mmgr_line_ch(&line, 'F');
        mmgr_line_decimal(&line, (unsigned long long)(key - VK_F1) + 1ull, 1u);
        return mmgr_line_finish(&line) != 0ull;
    }
    return 0;
}

// a key press passed to the window, the modifier keys down written before its name
static void system_key_pressed(GuiSystem *system, WPARAM key)
{
    char own[32];
    if (!system_key_named(key, own, sizeof(own)))
    {
        return;
    }
    const unsigned int mods = system_mods(system);
    char named[48];
    VerbaLinea line = {named, sizeof(named), 0ull};
    mmgr_line_put(&line, (mods & GUI_MOD_CONTROL) ? "Mod+" : "");
    mmgr_line_put(&line, (mods & GUI_MOD_ALT) ? "Alt+" : "");
    mmgr_line_put(&line, (mods & GUI_MOD_SHIFT) ? "Shift+" : "");
    mmgr_line_put(&line, own);
    (void)mmgr_line_finish(&line);
    system->events->key(system->context, named);
}

// a character typed handed to the window as UTF-8, a surrogate pair joined across its two messages; a control
// character and one typed with the control key down are keys, not text
static void system_char_typed(GuiSystem *system, WPARAM unit)
{
    if ((unit < 0x20u) || (unit == 0x7fu) || ((system->mods & (GUI_MOD_CONTROL | GUI_MOD_ALT)) != 0u))
    {
        return;
    }
    unsigned int letter = (unsigned int)unit;
    if ((unit >= 0xd800u) && (unit <= 0xdbffu))
    {
        // the high surrogate waits for its low one
        system->high_surrogate = (unsigned short)unit;
        return;
    }
    if ((unit >= 0xdc00u) && (unit <= 0xdfffu))
    {
        if (system->high_surrogate == 0u)
        {
            return;
        }
        letter = 0x10000u + (((unsigned int)system->high_surrogate - 0xd800u) << 10u) + ((unsigned int)unit - 0xdc00u);
        system->high_surrogate = 0u;
    }
    char text[8];
    VerbaLinea line = {text, sizeof(text), 0ull};
    cli_utf8_put(&line, letter);
    system->events->text(system->context, text, line.at);
}

static const wchar_t s_class[] = L"orior_window";

// the scale of `window`'s monitor
static float system_scale_of(HWND window)
{
    return (float)GetDpiForWindow(window) / 96.0f;
}

// the raster shown blitted into `context`, its `rows` rows from the row `from`, as a bitmap of those rows alone
static void system_blitted(GuiSystem *system, HDC context, unsigned int from, unsigned int rows)
{
    const GuiRaster *const raster = system->shown;
    if ((raster == NULL) || (raster->pixels == NULL) || (from >= raster->height) || (rows == 0u))
    {
        return;
    }
    const unsigned int count = (rows < raster->height - from) ? rows : raster->height - from;
    BITMAPINFO info;
    memor.set(&(MemoriaCfg){.dst = &info, .val = 0, .bytes = sizeof(info)});
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    // a raster's sides are far inside what a LONG represents; a negative height lays the rows top down
    info.bmiHeader.biWidth = (LONG)raster->width;
    info.bmiHeader.biHeight = -(LONG)count;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    // the row's place is far inside what an int represents
    SetDIBitsToDevice(context, 0, (int)from, raster->width, count, 0, 0, 0u, count, &raster->pixels[from * raster->width], &info,
                      DIB_RGB_COLORS);
}

// the hit test's answer as the system names the places
static LRESULT system_hit_code(unsigned int hit)
{
    static const LRESULT s_codes[13] = {HTCLIENT,    HTCAPTION,     HTLEFT,        HTRIGHT,  HTTOP,
                                        HTBOTTOM,    HTTOPLEFT,     HTTOPRIGHT,    HTBOTTOMLEFT, HTBOTTOMRIGHT,
                                        HTCLIENT,    HTCLIENT,      HTCLIENT};
    return (hit < 13u) ? s_codes[hit] : HTCLIENT;
}

// the window's one-pixel border colored for `active`: the system's accent while the window is active and the user
// shows the accent on borders, as the system's own windows do, else the compositor's default for the state
static void system_border_colored(HWND window, int active)
{
    COLORREF color = DWMWA_COLOR_DEFAULT;
    DWORD prevalence = 0u;
    DWORD bytes = sizeof(prevalence);
    if (active && (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"ColorPrevalence",
                                RRF_RT_REG_DWORD, NULL, &prevalence, &bytes) == ERROR_SUCCESS) &&
        (prevalence != 0u))
    {
        DWORD accent = 0u;
        BOOL opaque = FALSE;
        if (DwmGetColorizationColor(&accent, &opaque) == S_OK)
        {
            // the accent comes as alpha, red, green and blue bytes from the top; a COLORREF holds red in its low byte
            color = RGB((accent >> 16u) & 0xffu, (accent >> 8u) & 0xffu, accent & 0xffu);
        }
    }
    DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &color, sizeof(color));
}

static LRESULT CALLBACK system_message(HWND window, UINT message, WPARAM wide, LPARAM low)
{
    GuiSystem *const system = (GuiSystem *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (system == NULL)
    {
        return DefWindowProcW(window, message, wide, low);
    }
    switch (message)
    {
    case WM_NCCALCSIZE:
        // the client is the whole window
        return (wide != 0u) ? 0 : DefWindowProcW(window, message, wide, low);
    case WM_NCACTIVATE:
        // the window taking or losing the focus: its border colored for the state, and the system told not to paint
        // its own frame over the client, which -1 in place of the update region does
        system_border_colored(window, wide != 0u);
        return DefWindowProcW(window, message, wide, (LPARAM)-1);
    case WM_NCPAINT:
        // there is no non-client area to paint
        return 0;
    case WM_NCHITTEST:
    {
        POINT at = {GET_X_LPARAM(low), GET_Y_LPARAM(low)};
        ScreenToClient(window, &at);
        return system_hit_code(system->events->hit(system->context, at.x, at.y));
    }
    case WM_SIZE:
        if (wide == SIZE_MINIMIZED)
        {
            system->events->shown(system->context, 0);
        }
        else
        {
            system->events->shown(system->context, 1);
            // the client's sides, as the message carries them, are not negative
            system->events->sized(system->context, (unsigned int)LOWORD(low), (unsigned int)HIWORD(low),
                                  system_scale_of(window));
            // the frame at the new size is drawn here: while the system drags the window's edge it runs a loop of
            // its own, and the run loop's idle does not come until the drag ends
            system->events->idle(system->context);
        }
        return 0;
    case WM_DPICHANGED:
    {
        const RECT *const suggested = (const RECT *)low;
        SetWindowPos(window, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        system->frame_ms = system_frame_ms_of(window);
        return 0;
    }
    case WM_DISPLAYCHANGE:
        system->frame_ms = system_frame_ms_of(window);
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC context = BeginPaint(window, &paint);
        system_blitted(system, context, 0u, (system->shown != NULL) ? system->shown->height : 0u);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        system->events->pointer(system->context, GUI_POINTER_MOVED, GET_X_LPARAM(low), GET_Y_LPARAM(low), 0u,
                                system_mods(system));
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(window);
        system->events->pointer(system->context, GUI_POINTER_DOWN, GET_X_LPARAM(low), GET_Y_LPARAM(low), 0u,
                                system_mods(system));
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        system->events->pointer(system->context, GUI_POINTER_UP, GET_X_LPARAM(low), GET_Y_LPARAM(low), 0u,
                                system_mods(system));
        return 0;
    case WM_SETFOCUS:
        system->mods = system_mods_of_keyboard();
        return 0;
    case WM_KILLFOCUS:
        system->mods = 0u;
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (!system_modifier_keyed(system, wide, 1))
        {
            system_key_pressed(system, wide);
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        (void)system_modifier_keyed(system, wide, 0);
        return 0;
    case WM_CHAR:
        system_char_typed(system, wide);
        return 0;
    case WM_MOUSEWHEEL:
    {
        POINT at = {GET_X_LPARAM(low), GET_Y_LPARAM(low)};
        ScreenToClient(window, &at);
        // a notch is WHEEL_DELTA, positive away from the user, which scrolls up
        const int notches = -GET_WHEEL_DELTA_WPARAM(wide) / WHEEL_DELTA;
        system->events->wheel(system->context, at.x, at.y, notches * GUI_WHEEL_ROWS);
        return 0;
    }
    case GUI_WAKE_MESSAGE:
        system->events->woken(system->context);
        return 0;
    case WM_TIMER:
        if (wide == GUI_TICK_TIMER)
        {
            KillTimer(window, GUI_TICK_TIMER);
            system->events->ticked(system->context);
            // the frame the tick owes is drawn here, which keeps the ground moving while the system drags the window
            system->events->idle(system->context);
        }
        return 0;
    case WM_CLOSE:
        system->events->closing(system->context);
        return 0;
    case WM_DESTROY:
        system->closed = 1;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wide, low);
    }
}

// each monitor's area put after those seen, the main one first
static BOOL CALLBACK system_monitor_seen(HMONITOR monitor, HDC context, LPRECT rect, LPARAM data)
{
    (void)context;
    (void)rect;
    unsigned int *const count = (unsigned int *)data;
    CliWindowArea *const areas = (CliWindowArea *)(count + 2);
    const unsigned int most = count[1];
    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info) || (count[0] >= most))
    {
        return TRUE;
    }
    // a monitor's area is the part the taskbar leaves: a window fitted to it has its foot in view; its sides are
    // not negative
    const CliWindowArea area = {info.rcWork.left, info.rcWork.top, (unsigned int)(info.rcWork.right - info.rcWork.left),
                                (unsigned int)(info.rcWork.bottom - info.rcWork.top)};
    if ((info.dwFlags & MONITORINFOF_PRIMARY) && (count[0] > 0u))
    {
        areas[count[0]] = areas[0];
        areas[0] = area;
    }
    else
    {
        areas[count[0]] = area;
    }
    count[0] += 1u;
    return TRUE;
}

unsigned int gui_system_areas(CliWindowArea *areas, unsigned int most)
{
    // the count, the most and the areas handed to the enumeration together, the areas after two words
    unsigned int *const words = g_cli_cells->persistent_buf_alloc((2u * sizeof(unsigned int)) + ((most != 0u ? most : 1u) * sizeof(*areas)), &s_owner);
    if (words == NULL)
    {
        return 0u;
    }
    words[0] = 0u;
    words[1] = most;
    EnumDisplayMonitors(NULL, NULL, system_monitor_seen, (LPARAM)words);
    const unsigned int count = words[0];
    memor.cpy(&(MemoriaCfg){.dst = areas, .src = words + 2, .bytes = count * sizeof(*areas)});
    g_cli_cells->persistent_buf_release(words, &s_owner);
    return count;
}

GuiSystem *gui_system_opened(const CliWindowPlace *place, const char *title, const GuiSystemEvents *events,
                             void *context, VerbaLinea *err_msg)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    GuiSystem *const system = g_cli_cells->persistent_buf_alloc(sizeof(*system), &s_owner);
    if (system == NULL)
    {
        mmgr_line_put(err_msg, "the system window's cell could not be taken");
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = system, .val = 0, .bytes = sizeof(*system)});
    system->events = events;
    system->context = context;
    WNDCLASSEXW class_of;
    memor.set(&(MemoriaCfg){.dst = &class_of, .val = 0, .bytes = sizeof(class_of)});
    class_of.cbSize = sizeof(class_of);
    class_of.style = CS_HREDRAW | CS_VREDRAW;
    class_of.lpfnWndProc = system_message;
    class_of.hInstance = GetModuleHandleW(NULL);
    class_of.hCursor = LoadCursorW(NULL, IDC_ARROW);
    class_of.lpszClassName = s_class;
    if ((RegisterClassExW(&class_of) == 0) && (GetLastError() != ERROR_CLASS_ALREADY_EXISTS))
    {
        g_cli_cells->persistent_buf_release(system, &s_owner);
        mmgr_line_put(err_msg, "the window's class could not be registered");
        return NULL;
    }
    wchar_t wide_title[CLI_NAME_MAX_LEN];
    const int wrote = MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, CLI_NAME_MAX_LEN);
    if (wrote <= 0)
    {
        wide_title[0] = L'\0';
    }
    // the place's sides are far inside what an int represents
    system->window = CreateWindowExW(0, s_class, wide_title, WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
                                     place->x, place->y, (int)place->width, (int)place->height, NULL, NULL,
                                     class_of.hInstance, NULL);
    if (system->window == NULL)
    {
        g_cli_cells->persistent_buf_release(system, &s_owner);
        mmgr_line_put(err_msg, "the window could not be made");
        return NULL;
    }
    SetWindowLongPtrW(system->window, GWLP_USERDATA, (LONG_PTR)system);
    system->frame_ms = system_frame_ms_of(system->window);
    // the frame's change is taken up now the client is the whole window
    SetWindowPos(system->window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    // the compositor rounds the window's corners and draws its one-pixel border in the system's color
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(system->window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    ShowWindow(system->window, place->maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    return system;
}

// one message dispatched: a key pressed with control or alt down is a key, not text, and is not translated to a
// character; the character a key makes is queued behind the messages posted already, which would put it past the
// modifiers' release
static void system_message_taken(GuiSystem *system, MSG *message)
{
    const int keyed = (message->message == WM_KEYDOWN) || (message->message == WM_KEYUP) ||
                      (message->message == WM_SYSKEYDOWN) || (message->message == WM_SYSKEYUP);
    if (!keyed || ((system->mods & (GUI_MOD_CONTROL | GUI_MOD_ALT)) == 0u))
    {
        TranslateMessage(message);
    }
    DispatchMessageW(message);
}

void gui_system_run(GuiSystem *system)
{
    MSG message;
    while (!system->closed && (GetMessageW(&message, NULL, 0u, 0u) > 0))
    {
        system_message_taken(system, &message);
        if (!system->closed && !PeekMessageW(&message, NULL, 0u, 0u, PM_NOREMOVE))
        {
            system->events->idle(system->context);
        }
    }
}

void gui_system_pumped(GuiSystem *system)
{
    MSG message;
    while (!system->closed && PeekMessageW(&message, NULL, 0u, 0u, PM_REMOVE))
    {
        system_message_taken(system, &message);
    }
}

void *gui_system_native(const GuiSystem *system)
{
    return (void *)system->window;
}

void gui_system_closed(GuiSystem *system)
{
    if (system->window != NULL)
    {
        DestroyWindow(system->window);
        system->window = NULL;
    }
    g_cli_cells->persistent_buf_release(system, &s_owner);
}

void gui_system_present(GuiSystem *system, const GuiRaster *raster)
{
    system->shown = raster;
    HDC context = GetDC(system->window);
    system_blitted(system, context, 0u, raster->height);
    ReleaseDC(system->window, context);
    system->presented_ms = cli_clock_milliseconds();
}

void gui_system_present_part(GuiSystem *system, const GuiRaster *raster, GuiRect part)
{
    system->shown = raster;
    if (!gui_rect_empty(part) && (part.y >= 0))
    {
        // the part's rows are not negative where it is not empty
        HDC context = GetDC(system->window);
        system_blitted(system, context, (unsigned int)part.y, (unsigned int)part.height);
        ReleaseDC(system->window, context);
    }
    system->presented_ms = cli_clock_milliseconds();
}

void gui_system_wake(GuiSystem *system)
{
    if (system->window != NULL)
    {
        PostMessageW(system->window, GUI_WAKE_MESSAGE, 0u, 0);
    }
}

void gui_system_tick(GuiSystem *system, unsigned long long after)
{
    if (system->window == NULL)
    {
        return;
    }
    if (after == 0ull)
    {
        KillTimer(system->window, GUI_TICK_TIMER);
        return;
    }
    // no sooner than the monitor's next refresh after the last frame presented
    const unsigned long long since = cli_clock_milliseconds() - system->presented_ms;
    const unsigned long long until_refresh = (since < system->frame_ms) ? system->frame_ms - since : 0ull;
    const unsigned long long wait = (after > until_refresh) ? after : until_refresh;
    // a wait is far inside what the timer's milliseconds hold
    SetTimer(system->window, GUI_TICK_TIMER, (wait > USER_TIMER_MAXIMUM) ? USER_TIMER_MAXIMUM : (UINT)wait, NULL);
}

// an icon made from `raster`'s pixels with their alpha: a 32-bit section the pixels are copied into and an empty mask
static HICON system_icon_made(const GuiRaster *raster)
{
    BITMAPV5HEADER header;
    memor.set(&(MemoriaCfg){.dst = &header, .val = 0, .bytes = sizeof(header)});
    header.bV5Size = sizeof(header);
    // a raster's sides are far inside what a LONG represents; a negative height lays the rows top down
    header.bV5Width = (LONG)raster->width;
    header.bV5Height = -(LONG)raster->height;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00ff0000u;
    header.bV5GreenMask = 0x0000ff00u;
    header.bV5BlueMask = 0x000000ffu;
    header.bV5AlphaMask = 0xff000000u;
    void *bits = NULL;
    HBITMAP color = CreateDIBSection(NULL, (const BITMAPINFO *)&header, DIB_RGB_COLORS, &bits, NULL, 0u);
    if ((color == NULL) || (bits == NULL))
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = bits, .src = raster->pixels, .bytes = (unsigned long long)raster->width * raster->height * sizeof(GuiColor)});
    HBITMAP mask = CreateBitmap((int)raster->width, (int)raster->height, 1u, 1u, NULL);
    ICONINFO info;
    info.fIcon = TRUE;
    info.xHotspot = 0u;
    info.yHotspot = 0u;
    info.hbmMask = mask;
    info.hbmColor = color;
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(color);
    if (mask != NULL)
    {
        DeleteObject(mask);
    }
    return icon;
}

void gui_system_icon(GuiSystem *system, const GuiRaster *title_icon, const GuiRaster *taskbar_icon)
{
    if (system->window == NULL)
    {
        return;
    }
    HICON title = system_icon_made(title_icon);
    HICON taskbar = system_icon_made(taskbar_icon);
    if (title != NULL)
    {
        SendMessageW(system->window, WM_SETICON, ICON_SMALL, (LPARAM)title);
    }
    if (taskbar != NULL)
    {
        SendMessageW(system->window, WM_SETICON, ICON_BIG, (LPARAM)taskbar);
    }
}

void gui_system_act(GuiSystem *system, unsigned int act)
{
    if (act == GUI_ACT_MINIMIZE)
    {
        ShowWindow(system->window, SW_MINIMIZE);
    }
    else if (act == GUI_ACT_MAXIMIZE)
    {
        ShowWindow(system->window, IsZoomed(system->window) ? SW_RESTORE : SW_MAXIMIZE);
    }
    else
    {
        PostMessageW(system->window, WM_CLOSE, 0u, 0);
    }
}

void gui_system_placed(const GuiSystem *system, CliWindowPlace *place)
{
    WINDOWPLACEMENT placement;
    placement.length = sizeof(placement);
    GetWindowPlacement(system->window, &placement);
    const RECT *const rect = &placement.rcNormalPosition;
    place->x = rect->left;
    place->y = rect->top;
    // a window's sides are not negative
    place->width = (unsigned int)(rect->right - rect->left);
    place->height = (unsigned int)(rect->bottom - rect->top);
    place->maximized = (placement.showCmd == SW_SHOWMAXIMIZED) ? 1 : 0;
}

float gui_system_scale(const GuiSystem *system)
{
    return system_scale_of(system->window);
}
