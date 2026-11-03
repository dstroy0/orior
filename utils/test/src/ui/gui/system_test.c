// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system_test.c: the system body's check, on Windows: a window opened and the messages a check posts to its queue
// handed on as the window's events, a key with the modifiers posted before it named as the page's editor names it, a
// character posted typed as text and a key pressed with control typing none, Alt+F4 the system's close and no key, the
// pointer's press and release at their pixel, and the wheel; on another system the check has no window to open and
// checks nothing
#include "../../../../../ui/gui/src/system.h"
#include "../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../cli/cli_check.h"

#if defined(_WIN32)
#define UNICODE
#include <windows.h>

// what the window was handed: the keys, the texts and the pointer's events, each the last and how many
typedef struct
{
    unsigned int keys;
    char key[64];
    unsigned int texts;
    char text[32];
    unsigned int pointers;
    unsigned int lefts;
    unsigned int blurs;
    unsigned int pointer;
    unsigned int button;
    int x;
    int y;
    unsigned int mods;
    int wheel_rows;
    unsigned int sized;
    unsigned int motions;
    unsigned int closings;
} Transferred;

static void transferred_sized(const GuiEachWidthHeightScaleCfg *args)
{
    ((Transferred *)args->context)->sized += 1u;
}

static void transferred_input(const GuiEachInputCfg *args)
{
    Transferred *const transferred = (Transferred *)args->context;
    if (args->input->type == GUI_INPUT_WHEEL)
    {
        transferred->wheel_rows += args->input->rows;
    }
    else if (args->input->type == GUI_INPUT_KEY)
    {
        transferred->keys += 1u;
        VerbaLinea line = {transferred->key, sizeof(transferred->key), 0ull};
        mmgr_line_put(&line, args->input->name);
        (void)mmgr_line_finish(&line);
    }
    else if (args->input->type == GUI_INPUT_TEXT)
    {
        transferred->texts += 1u;
        VerbaLinea line = {transferred->text, sizeof(transferred->text), 0ull};
        mmgr_line_put_n(&line, args->input->text, args->input->size);
        (void)mmgr_line_finish(&line);
    }
    else if (args->input->type == GUI_INPUT_LEFT)
    {
        transferred->lefts += 1u;
    }
    else if (args->input->type == GUI_INPUT_BLUR)
    {
        transferred->blurs += 1u;
    }
    else if (args->input->type != GUI_INPUT_MOVED)
    {
        transferred->pointers += 1u;
        transferred->pointer = args->input->type;
        transferred->button = args->input->button;
        transferred->x = args->input->x;
        transferred->y = args->input->y;
        transferred->mods = args->input->mods;
    }
}

static void transferred_nothing(void *context)
{
    (void)context;
}

static unsigned int transferred_hit(const GuiEachInputCfg *args)
{
    (void)args;
    return 0u;
}

static void transferred_displayed(const GuiEachDisplayedCfg *args)
{
    (void)args;
}

static void transferred_motion(void *context)
{
    ((Transferred *)context)->motions += 1u;
}

static void transferred_closing(void *context)
{
    ((Transferred *)context)->closings += 1u;
}

static int same(const char *one, const char *other)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = one, .other = other, .cap = 64ull});
}

// a key posted down and up with the modifiers `modifiers` posted down before and up after, as the harness posts them
static void posted(HWND window, const WPARAM *modifiers, unsigned int count, WPARAM key)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        PostMessageW(window, WM_KEYDOWN, modifiers[at], 0);
    }
    PostMessageW(window, WM_KEYDOWN, key, 0);
    PostMessageW(window, WM_KEYUP, key, 0);
    for (unsigned int at = count; at > 0u; at -= 1u)
    {
        PostMessageW(window, WM_KEYUP, modifiers[at - 1u], 0);
    }
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    static Transferred transferred;
    static const GuiSystemEvents s_events = {transferred_sized, transferred_input,     transferred_nothing, transferred_closing,
                                             transferred_hit,   transferred_displayed, transferred_motion};
    const CliWindowPosition place = {20, 20, 320, 200, 0};
    GuiSystem *const system = EMBED_CALL(gui_system_position_title.open, GuiSystemPositionTitleCfg, .place = &place, .title = "orior check", .events = &s_events, .context = &transferred, .err_msg = &why);
    cli_check(system != NULL, "a window opens on the system");
    if (system == NULL)
    {
        mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    HWND window = (HWND)EMBED_CALL(gui_system_position.native, GuiSystemPositionCfg, .system_read = system);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check(transferred.sized > 0u, "the window is sized as it opens");
    SetWindowPos(window, NULL, 0, 0, 100, 100, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    RECT min;
    GetWindowRect(window, &min);
    cli_check(((min.right - min.left) >= (LONG)CLI_POSITION_MIN_WIDTH) && ((min.bottom - min.top) >= (LONG)CLI_POSITION_MIN_HEIGHT),
              "a window sized below its least size stands at the least");
    BOOL animates = TRUE;
    (void)SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0u, &animates, 0u);
    cli_check(EMBED_CALL(gui_system_position.motion_reduce, GuiSystemPositionCfg, .system_read = system) == (animates ? 0 : 1), "the system's wish for less motion is read");
    SendMessageW(window, WM_SETTINGCHANGE, SPI_SETCLIENTAREAANIMATION, 0);
    cli_check(transferred.motions == 1u, "the wish changed is handed on");
    SendMessageW(window, WM_SETTINGCHANGE, SPI_SETWORKAREA, 0);
    cli_check(transferred.motions == 1u, "another setting changed is not");

    unsigned int max_width = 0u;
    unsigned int max_height = 0u;
    EMBED_CALL(gui_system_width.display_max, GuiSystemWidthCfg, .width = &max_width, .height = &max_height);
    cli_check((max_width != 0u) && (max_height != 0u), "the largest monitor has a size");
    CliWindowArea areas[16];
    const unsigned int area_count = EMBED_CALL(cli_system_areas.areas, GuiSystemAreasCfg, .areas = areas, .most = 16u);
    for (unsigned int at = 0u; at < area_count; at += 1u)
    {
        cli_check(((unsigned long long)areas[at].width * areas[at].height) <=
                      ((unsigned long long)max_width * max_height),
                  "and holds as many pixels as any monitor's area");
    }

    const WPARAM control[1] = {VK_CONTROL};
    posted(window, control, 1u, 'Z');
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.keys == 1u) && same(transferred.key, "Mod+Z"), "a key posted with control down before it is Mod+Z");
    cli_check_text(transferred.text, cellul.len(&(CatenaFinitaCfg){.src = transferred.text, .cap = sizeof(transferred.text)}), "",
                   "a key pressed with control types no text");

    PostMessageW(window, WM_CHAR, 'x', 0);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.texts == 1u) && same(transferred.text, "x"), "a character posted is typed as text");

    const WPARAM shift[1] = {VK_SHIFT};
    posted(window, shift, 1u, VK_END);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.keys == 2u) && same(transferred.key, "Shift+End"),
              "a key posted with shift down before it is Shift+End");

    posted(window, NULL, 0u, VK_LEFT);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.keys == 3u) && same(transferred.key, "Left"), "a key posted alone goes by its own name");
    cli_check(transferred.texts == 1u, "the keys named type no text");

    posted(window, NULL, 0u, 'Q');
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.keys == 4u) && same(transferred.key, "Q"), "a letter's key goes by its capital");

    posted(window, NULL, 0u, VK_APPS);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.keys == 5u) && same(transferred.key, "ContextMenu"), "the menu key goes by the name a browser gives it");

    // Alt+F4 is the system's, as a browser leaves it to the system: it asks the window to close and is handed on as no
    // key
    PostMessageW(window, WM_SYSKEYDOWN, VK_F4, (LPARAM)KF_ALTDOWN << 16);
    PostMessageW(window, WM_SYSKEYUP, VK_F4, (LPARAM)KF_ALTDOWN << 16);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check(transferred.closings == 1u, "Alt+F4 asks the window to close");
    cli_check(transferred.keys == 5u, "and is handed on as no key");

    PostMessageW(window, WM_LBUTTONDOWN, 0, (60 << 16) | 50);
    PostMessageW(window, WM_LBUTTONUP, 0, (60 << 16) | 50);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.pointers == 2u) && (transferred.pointer == GUI_INPUT_UP) && (transferred.x == 50) && (transferred.y == 60),
              "the pointer's press and release come at their pixel");

    // the right button's press comes as its press, and its release as the system's gesture for a menu at its pixel, a
    // browser on this system opening one as the button is let go
    PostMessageW(window, WM_RBUTTONDOWN, 0, (70 << 16) | 40);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.pointers == 3u) && (transferred.pointer == GUI_INPUT_DOWN) && (transferred.button == GUI_BUTTON_RIGHT) && (transferred.x == 40) &&
                  (transferred.y == 70),
              "a right press comes as the right button's press at its pixel");
    PostMessageW(window, WM_RBUTTONUP, 0, (72 << 16) | 44);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.pointers == 4u) && (transferred.pointer == GUI_INPUT_MENU) && (transferred.x == 44) && (transferred.y == 72),
              "its release asks for a menu at its pixel");

    // the right button on the bar the system moves the window by comes as it does on the client, at the client's pixel,
    // and opens no window menu of the system's: the page's bar answers it with no menu
    POINT on_bar = {30, 20};
    ClientToScreen(window, &on_bar);
    PostMessageW(window, WM_NCRBUTTONDOWN, HTCAPTION, MAKELPARAM(on_bar.x, on_bar.y));
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.pointers == 5u) && (transferred.pointer == GUI_INPUT_DOWN) && (transferred.button == GUI_BUTTON_RIGHT) && (transferred.x == 30) &&
                  (transferred.y == 20),
              "a right press on the bar comes as the right button's press at its client pixel");
    PostMessageW(window, WM_NCRBUTTONUP, HTCAPTION, MAKELPARAM(on_bar.x, on_bar.y));
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.pointers == 6u) && (transferred.pointer == GUI_INPUT_MENU) && (transferred.x == 30) && (transferred.y == 20),
              "its release asks for a menu at its client pixel");

    // the keys gone to another window are told, as a page's window is blurred
    const unsigned int blurs_before = transferred.blurs;
    SendMessageW(window, WM_KILLFOCUS, 0, 0);
    cli_check(transferred.blurs == (blurs_before + 1u), "the keys gone to another window are told as the window blurred");

    // the pointer gone from the client is told only where it stands off the window altogether
    POINT cursor;
    RECT frame;
    const int cursor_read = GetCursorPos(&cursor) && GetWindowRect(window, &frame);
    const int off_window = cursor_read && !PtInRect(&frame, cursor);
    PostMessageW(window, WM_MOUSELEAVE, 0, 0);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((transferred.lefts != 0u) == (off_window != 0), "a leave off the window is told as the pointer gone");

    // a second window its owner's: it has its own events, and closing it leaves the owner running
    static Transferred owner_transferred;
    const CliWindowPosition owner_position = {60, 60, 200, 120, 0};
    GuiSystem *const owner = EMBED_CALL(gui_system_position_title.owner_open, GuiSystemPositionTitleCfg, .owner = system, .place = &owner_position, .title = "orior owned", .events = &s_events, .context = &owner_transferred, .err_msg = &why);
    cli_check(owner != NULL, "a window owned by it opens");
    if (owner != NULL)
    {
        HWND const owner_window = (HWND)EMBED_CALL(gui_system_position.native, GuiSystemPositionCfg, .system_read = owner);
        cli_check(GetWindow(owner_window, GW_OWNER) == window, "the system knows its owner");
        const unsigned int owner_pointers = transferred.pointers;
        PostMessageW(owner_window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(7, 9));
        PostMessageW(owner_window, WM_LBUTTONUP, 0, MAKELPARAM(7, 9));
        EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
        cli_check((owner_transferred.pointers == 2u) && (owner_transferred.x == 7) && (owner_transferred.y == 9), "an event of its goes to its own context");
        cli_check(transferred.pointers == owner_pointers, "and not to the owner's");
        const GuiColor pixels[4] = {GUI_RGB(1u, 2u, 3u), GUI_RGB(1u, 2u, 3u), GUI_RGB(1u, 2u, 3u), GUI_RGB(1u, 2u, 3u)};
        const GuiRaster small_raster = {2u, 2u, (GuiColor *)pixels, NULL, 0ull, 0ull};
        EMBED_CALL(gui_system_raster.present, GuiSystemRasterCfg, .system = owner, .raster = &small_raster);
        cli_check(EMBED_CALL(gui_system_position.scale, GuiSystemPositionCfg, .system_read = owner) >= 100, "it reads its own monitor's scale");
        CliWindowPosition positioned;
        EMBED_CALL(gui_system_position.place, GuiSystemPositionCfg, .system_read = owner, .position_out = &positioned);
        cli_check((positioned.x == 60) && (positioned.y == 60) && (positioned.width == 200u) && (positioned.height == 120u), "it stands where it was opened");
        EMBED_CALL(gui_system_table.close, GuiSystemCfg, .system = owner);
        EMBED_CALL(gui_system_table.free, GuiSystemCfg, .system = owner);
        EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
        PostMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(11, 13));
        PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(11, 13));
        EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
        cli_check((transferred.pointers == owner_pointers + 2u) && (transferred.x == 11), "closing it leaves the owner running");
    }

    EMBED_CALL(gui_system_table.close, GuiSystemCfg, .system = system);
    EMBED_CALL(gui_system_table.free, GuiSystemCfg, .system = system);
    return cli_on_check();
}
#else
int main(void)
{
    return cli_on_check();
}
#endif
