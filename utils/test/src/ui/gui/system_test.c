// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system_test.c: the system body's check, on Windows: a window opened and the messages a check posts to its queue
// handed on as the window's events, a key with the modifiers posted before it named as the page's editor names it, a
// character posted typed as text and a key held with control typing none, the pointer's press and release at their
// pixel, and the wheel; on another system the check has no window to open and checks nothing
#include "../../../../../ui/gui/src/system.h"
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
} Handed;

static void handed_sized(void *context, unsigned int width, unsigned int height, float scale)
{
    (void)width;
    (void)height;
    (void)scale;
    ((Handed *)context)->sized += 1u;
}

static void handed_pointer(void *context, unsigned int kind, int x, int y, unsigned int button, unsigned int mods)
{
    Handed *const handed = (Handed *)context;
    (void)button;
    handed->pointers += 1u;
    handed->pointer = kind;
    handed->x = x;
    handed->y = y;
    handed->mods = mods;
}

static void handed_wheel(void *context, int x, int y, int rows)
{
    (void)x;
    (void)y;
    ((Handed *)context)->wheel_rows += rows;
}

static void handed_key(void *context, const char *name)
{
    Handed *const handed = (Handed *)context;
    handed->keys += 1u;
    VerbaLinea line = {handed->key, sizeof(handed->key), 0ull};
    mmgr_line_put(&line, name);
    (void)mmgr_line_finish(&line);
}

static void handed_text(void *context, const char *text, unsigned long long size)
{
    Handed *const handed = (Handed *)context;
    handed->texts += 1u;
    VerbaLinea line = {handed->text, sizeof(handed->text), 0ull};
    mmgr_line_put_n(&line, text, size);
    (void)mmgr_line_finish(&line);
}

static void handed_nothing(void *context)
{
    (void)context;
}

static unsigned int handed_hit(void *context, int x, int y)
{
    (void)context;
    (void)x;
    (void)y;
    return 0u;
}

static void handed_shown(void *context, int shown)
{
    (void)context;
    (void)shown;
}

static int same(const char *one, const char *other)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = one, .other = other, .cap = 64ull});
}

// a key posted down and up with the modifiers `held` posted down before and up after, as the harness posts them
static void posted(HWND window, const WPARAM *held, unsigned int count, WPARAM key)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        PostMessageW(window, WM_KEYDOWN, held[at], 0);
    }
    PostMessageW(window, WM_KEYDOWN, key, 0);
    PostMessageW(window, WM_KEYUP, key, 0);
    for (unsigned int at = count; at > 0u; at -= 1u)
    {
        PostMessageW(window, WM_KEYUP, held[at - 1u], 0);
    }
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    static Handed handed;
    static const GuiSystemEvents s_events = {handed_sized,   handed_pointer, handed_wheel, handed_key,     handed_text,  handed_nothing,
                                             handed_nothing, handed_hit,     handed_nothing, handed_nothing, handed_shown};
    const CliWindowPlace place = {20, 20, 320, 200, 0};
    GuiSystem *const system = gui_system_opened(&place, "orior check", &s_events, &handed, &why);
    cli_check(system != NULL, "a window opens on the system");
    if (system == NULL)
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_checked();
    }
    HWND window = (HWND)gui_system_native(system);
    gui_system_pumped(system);
    cli_check(handed.sized > 0u, "the window is sized as it opens");

    const WPARAM control[1] = {VK_CONTROL};
    posted(window, control, 1u, 'Z');
    gui_system_pumped(system);
    cli_check((handed.keys == 1u) && same(handed.key, "Mod+Z"), "a key posted with control down before it is Mod+Z");
    cli_check_text(handed.text, cellul.len(&(CatenaFinitaCfg){.src = handed.text, .cap = sizeof(handed.text)}), "",
                   "a key held with control types no text");

    PostMessageW(window, WM_CHAR, 'x', 0);
    gui_system_pumped(system);
    cli_check((handed.texts == 1u) && same(handed.text, "x"), "a character posted is typed as text");

    const WPARAM shift[1] = {VK_SHIFT};
    posted(window, shift, 1u, VK_END);
    gui_system_pumped(system);
    cli_check((handed.keys == 2u) && same(handed.key, "Shift+End"), "a key posted with shift down before it is Shift+End");

    posted(window, NULL, 0u, VK_LEFT);
    gui_system_pumped(system);
    cli_check((handed.keys == 3u) && same(handed.key, "Left"), "a key posted alone goes by its own name");
    cli_check(handed.texts == 1u, "the keys named type no text");

    posted(window, NULL, 0u, 'Q');
    gui_system_pumped(system);
    cli_check((handed.keys == 4u) && same(handed.key, "Q"), "a letter's key goes by its capital");

    PostMessageW(window, WM_LBUTTONDOWN, 0, (60 << 16) | 50);
    PostMessageW(window, WM_LBUTTONUP, 0, (60 << 16) | 50);
    gui_system_pumped(system);
    cli_check((handed.pointers == 2u) && (handed.pointer == GUI_POINTER_UP) && (handed.x == 50) && (handed.y == 60),
              "the pointer's press and release come at their pixel");

    gui_system_closed(system);
    gui_system_given(system);
    return cli_checked();
}
#else
int main(void)
{
    return cli_checked();
}
#endif
