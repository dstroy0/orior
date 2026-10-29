// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_test.c: the window's check without a system: sized, it lays its parts out and draws them into its raster,
// the bar and the status bar tinted with the ink, the three controls at the bar's right end, the work's corners
// rounded; the pointer over the bar's empty part is the caption, over an edge a border, over a control that control,
// inside the work the client; the wheel scrolls the editor; and a file's text put whole shows its lines
#include "../../../../../ui/gui/src/window.h"
#include "../cli/cli_check.h"

static void woken(void *context)
{
    (void)context;
}

static GuiColor at(const GuiWindow *window, int x, int y)
{
    return window->raster.pixels[((unsigned int)y * window->raster.width) + (unsigned int)x];
}

// whether any pixel of `rect` is not `ground`
static int painted(const GuiWindow *window, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (at(window, x, y) != ground)
            {
                return 1;
            }
        }
    }
    return 0;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    static GuiWindow window;
    const int begun = gui_window_begun(&window, woken, NULL, &why);
    cli_check(begun, "the window begins");
    if (!begun)
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_checked();
    }
    cli_check(gui_window_sized(&window, 800u, 500u, 1.0f), "the window sized lays its parts out and makes its raster");
    cli_check((window.raster.width == 800u) && (window.raster.height == 500u), "the raster is the window's size");
    gui_window_drawn(&window);
    const GuiPalette *const palette = window.palette;
    const GuiColor ground = palette->ground;
    cli_check(at(&window, 400, 20) != ground, "the bar is tinted over the ground");
    cli_check(at(&window, 400, 20) == at(&window, 20, 250), "the strip is tinted as the bar is");
    cli_check(at(&window, 400, 490) == at(&window, 400, 20), "the status bar is tinted as the bar is");
    cli_check(at(&window, 795, 250) == at(&window, 400, 20), "the frame's gap at the right is tinted as the bar is");
    cli_check(painted(&window, (GuiRect){800 - 5 - 34, 10, 34, 20}, at(&window, 400, 20)), "the close control is drawn");
    cli_check(painted(&window, (GuiRect){800 - 5 - 106, 10, 34, 20}, at(&window, 400, 20)), "the minimize control is drawn");
    cli_check((at(&window, 44 + 1, 41 + 2) == palette->ground) && (at(&window, 44 + 1, 41 + 2) != palette->surface),
              "the work's top left corner is rounded off to the ground");
    cli_check(at(&window, 44 + 1 + 10, 41 + 2 + 10) == palette->surface, "the tabs row is the surface color");
    cli_check(at(&window, 300, 300) == palette->editor_ground, "the editor shows its ground");

    cli_check(gui_window_hit(&window, 400, 20) == GUI_HIT_CAPTION, "the bar's empty part is the caption");
    cli_check(gui_window_hit(&window, 800 - 5 - 17, 20) == GUI_HIT_CLOSE, "the rightmost control is close");
    cli_check(gui_window_hit(&window, 800 - 5 - 34 - 2 - 17, 20) == GUI_HIT_MAXIMIZE, "the middle control is maximize");
    cli_check(gui_window_hit(&window, 800 - 5 - 72 - 17, 20) == GUI_HIT_MINIMIZE, "the leftmost control is minimize");
    cli_check(gui_window_hit(&window, 400, 2) == GUI_HIT_TOP, "the top edge is a border");
    cli_check(gui_window_hit(&window, 2, 2) == GUI_HIT_TOP_LEFT, "the top left corner is a corner");
    cli_check(gui_window_hit(&window, 797, 497) == GUI_HIT_BOTTOM_RIGHT, "the bottom right corner is a corner");
    cli_check(gui_window_hit(&window, 2, 250) == GUI_HIT_LEFT, "the left edge is a border");
    cli_check(gui_window_hit(&window, 300, 300) == GUI_HIT_CLIENT, "the work is the client");
    cli_check(gui_window_hit(&window, 400, 490) == GUI_HIT_CLIENT, "the status bar is the client");

    cli_check(gui_window_text_put(&window, "one\ntwo\r\nthree", 14ull), "a text put whole is held as lines");
    cli_check((window.editor.lines == 3ull) && (window.editor.count == 3u), "the text has three lines");
    gui_window_drawn(&window);
    cli_check(painted(&window, (GuiRect){window.frame.editor.x + 40, window.frame.editor.y, 100, 20}, palette->editor_ground),
              "the first line is drawn in the editor");
    gui_window_wheel(&window, 300, 300, 1);
    cli_check(window.editor.top == 1ull, "the wheel over the editor scrolls it a line");
    gui_window_wheel(&window, 400, 20, 1);
    cli_check(window.editor.top == 1ull, "the wheel over the bar scrolls nothing");

    cli_check(gui_window_sized(&window, 640u, 400u, 1.0f), "the window sized again remakes its raster");
    cli_check((window.raster.width == 640u) && (window.frame.bar.width == 640), "the parts follow the new size");

    gui_window_ended(&window);
    cli_check(window.raster.pixels == NULL, "the window ended holds no raster");
    return cli_checked();
}
