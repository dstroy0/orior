// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system_test.c: the system body's check, on Windows: a window opened and the messages a check posts to its queue
// handed on as the window's events, a key with the modifiers posted before it named as the page's editor names it, a
// character posted typed as text and a key pressed with control typing none, the pointer's press and release at their
// pixel, and the wheel; on another system the check has no window to open and checks nothing
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
    unsigned int pointer;
    int x;
    int y;
    unsigned int mods;
    int wheel_rows;
    unsigned int sized;
    unsigned int motions;
} Handed;

static void handed_sized(const GuiEachWidthHeightScaleCfg *args)
{
    ((Handed *)args->context)->sized += 1u;
}

static void handed_input(const GuiEachInputCfg *args)
{
    Handed *const handed = (Handed *)args->context;
    if (args->input->type == GUI_INPUT_WHEEL)
    {
        handed->wheel_rows += args->input->rows;
    }
    else if (args->input->type == GUI_INPUT_KEY)
    {
        handed->keys += 1u;
        VerbaLinea line = {handed->key, sizeof(handed->key), 0ull};
        mmgr_line_put(&line, args->input->name);
        (void)mmgr_line_finish(&line);
    }
    else if (args->input->type == GUI_INPUT_TEXT)
    {
        handed->texts += 1u;
        VerbaLinea line = {handed->text, sizeof(handed->text), 0ull};
        mmgr_line_put_n(&line, args->input->text, args->input->size);
        (void)mmgr_line_finish(&line);
    }
    else
    {
        handed->pointers += 1u;
        handed->pointer = args->input->type;
        handed->x = args->input->x;
        handed->y = args->input->y;
        handed->mods = args->input->mods;
    }
}

static void handed_nothing(void *context)
{
    (void)context;
}

static unsigned int handed_hit(const GuiEachInputCfg *args)
{
    (void)args;
    return 0u;
}

static void handed_shown(const GuiEachShownCfg *args)
{
    (void)args;
}

static void handed_motion(void *context)
{
    ((Handed *)context)->motions += 1u;
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
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    static Handed handed;
    static const GuiSystemEvents s_events = {handed_sized,   handed_input,   handed_nothing, handed_nothing, handed_hit,
                                             handed_nothing, handed_nothing, handed_shown,   handed_motion};
    const CliWindowPlace place = {20, 20, 320, 200, 0};
    GuiSystem *const system = EMBED_CALL(gui_system_place_title.open, GuiSystemPlaceTitleCfg, .place = &place, .title = "orior check", .events = &s_events, .context = &handed, .err_msg = &why);
    cli_check(system != NULL, "a window opens on the system");
    if (system == NULL)
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_on_check();
    }
    HWND window = (HWND)EMBED_CALL(gui_system_place.native, GuiSystemPlaceCfg, .system_read = system);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check(handed.sized > 0u, "the window is sized as it opens");
    SetWindowPos(window, NULL, 0, 0, 100, 100, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    RECT least;
    GetWindowRect(window, &least);
    cli_check(((least.right - least.left) >= (LONG)CLI_PLACE_LEAST_WIDTH) && ((least.bottom - least.top) >= (LONG)CLI_PLACE_LEAST_HEIGHT),
              "a window sized below its least size stands at the least");
    BOOL animates = TRUE;
    (void)SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0u, &animates, 0u);
    cli_check(EMBED_CALL(gui_system_place.motion_reduce, GuiSystemPlaceCfg, .system_read = system) == (animates ? 0 : 1), "the system's wish for less motion is read");
    SendMessageW(window, WM_SETTINGCHANGE, SPI_SETCLIENTAREAANIMATION, 0);
    cli_check(handed.motions == 1u, "the wish changed is handed on");
    SendMessageW(window, WM_SETTINGCHANGE, SPI_SETWORKAREA, 0);
    cli_check(handed.motions == 1u, "another setting changed is not");

    unsigned int most_width = 0u;
    unsigned int most_height = 0u;
    EMBED_CALL(gui_system_width.display_most, GuiSystemWidthCfg, .width = &most_width, .height = &most_height);
    cli_check((most_width != 0u) && (most_height != 0u), "the largest monitor has a size");
    CliWindowArea areas[16];
    const unsigned int area_count = EMBED_CALL(cli_system_areas.areas, GuiSystemAreasCfg, .areas = areas, .most = 16u);
    for (unsigned int at = 0u; at < area_count; at += 1u)
    {
        cli_check(((unsigned long long)areas[at].width * areas[at].height) <=
                      ((unsigned long long)most_width * most_height),
                  "and holds as many pixels as any monitor's area");
    }

    const WPARAM control[1] = {VK_CONTROL};
    posted(window, control, 1u, 'Z');
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((handed.keys == 1u) && same(handed.key, "Mod+Z"), "a key posted with control down before it is Mod+Z");
    cli_check_text(handed.text, cellul.len(&(CatenaFinitaCfg){.src = handed.text, .cap = sizeof(handed.text)}), "",
                   "a key pressed with control types no text");

    PostMessageW(window, WM_CHAR, 'x', 0);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((handed.texts == 1u) && same(handed.text, "x"), "a character posted is typed as text");

    const WPARAM shift[1] = {VK_SHIFT};
    posted(window, shift, 1u, VK_END);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((handed.keys == 2u) && same(handed.key, "Shift+End"),
              "a key posted with shift down before it is Shift+End");

    posted(window, NULL, 0u, VK_LEFT);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((handed.keys == 3u) && same(handed.key, "Left"), "a key posted alone goes by its own name");
    cli_check(handed.texts == 1u, "the keys named type no text");

    posted(window, NULL, 0u, 'Q');
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((handed.keys == 4u) && same(handed.key, "Q"), "a letter's key goes by its capital");

    PostMessageW(window, WM_LBUTTONDOWN, 0, (60 << 16) | 50);
    PostMessageW(window, WM_LBUTTONUP, 0, (60 << 16) | 50);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((handed.pointers == 2u) && (handed.pointer == GUI_INPUT_UP) && (handed.x == 50) && (handed.y == 60),
              "the pointer's press and release come at their pixel");

    // the pointer gone from the client is told only where it stands off the window altogether
    POINT cursor;
    RECT frame;
    const int cursor_read = GetCursorPos(&cursor) && GetWindowRect(window, &frame);
    const int off_window = cursor_read && !PtInRect(&frame, cursor);
    PostMessageW(window, WM_MOUSELEAVE, 0, 0);
    EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
    cli_check((handed.pointer == GUI_INPUT_LEFT) == (off_window != 0), "a leave off the window is told as the pointer gone");

    // a second window its owner's: it has its own events, and closing it leaves the owner running
    static Handed owned_handed;
    const CliWindowPlace owned_place = {60, 60, 200, 120, 0};
    GuiSystem *const owned = EMBED_CALL(gui_system_place_title.owned_open, GuiSystemPlaceTitleCfg, .owner = system, .place = &owned_place, .title = "orior owned", .events = &s_events, .context = &owned_handed, .err_msg = &why);
    cli_check(owned != NULL, "a window owned by it opens");
    if (owned != NULL)
    {
        HWND const owned_window = (HWND)EMBED_CALL(gui_system_place.native, GuiSystemPlaceCfg, .system_read = owned);
        cli_check(GetWindow(owned_window, GW_OWNER) == window, "the system knows its owner");
        const unsigned int owner_pointers = handed.pointers;
        PostMessageW(owned_window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(7, 9));
        PostMessageW(owned_window, WM_LBUTTONUP, 0, MAKELPARAM(7, 9));
        EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
        cli_check((owned_handed.pointers == 2u) && (owned_handed.x == 7) && (owned_handed.y == 9), "an event of its goes to its own context");
        cli_check(handed.pointers == owner_pointers, "and not to the owner's");
        const GuiColor pixels[4] = {GUI_RGB(1u, 2u, 3u), GUI_RGB(1u, 2u, 3u), GUI_RGB(1u, 2u, 3u), GUI_RGB(1u, 2u, 3u)};
        const GuiRaster small_raster = {2u, 2u, (GuiColor *)pixels, NULL, 0ull, 0ull};
        EMBED_CALL(gui_system_raster.present, GuiSystemRasterCfg, .system = owned, .raster = &small_raster);
        cli_check(EMBED_CALL(gui_system_place.scale, GuiSystemPlaceCfg, .system_read = owned) >= 100, "it reads its own monitor's scale");
        CliWindowPlace placed;
        EMBED_CALL(gui_system_place.place, GuiSystemPlaceCfg, .system_read = owned, .place_out = &placed);
        cli_check((placed.x == 60) && (placed.y == 60) && (placed.width == 200u) && (placed.height == 120u), "it stands where it was opened");
        EMBED_CALL(gui_system_table.close, GuiSystemCfg, .system = owned);
        EMBED_CALL(gui_system_table.free, GuiSystemCfg, .system = owned);
        EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
        PostMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(11, 13));
        PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(11, 13));
        EMBED_CALL(gui_system_table.pumped, GuiSystemCfg, .system = system);
        cli_check((handed.pointers == owner_pointers + 2u) && (handed.x == 11), "closing it leaves the owner running");
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
