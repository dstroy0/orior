// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_test.c: the window's check without a system: sized, it lays its parts out and draws them into its raster,
// the bar and the status bar tinted with the ink, the three controls at the bar's right end, the work's corners
// rounded; the pointer over the bar's empty part is the caption, over an edge a border, over a control that control,
// inside the work the client; the wheel scrolls the editor; and a file's text put whole shows its lines
#include "../../../../../ui/gui/src/parts/glass.h"
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

// `color` over `under` by the color's alpha, as the raster blends
static GuiColor over(GuiColor under, GuiColor color)
{
    const unsigned int share = GUI_ALPHA_OF(color);
    const unsigned int rest = 255u - share;
    return GUI_RGB(((GUI_RED_OF(color) * share) + (GUI_RED_OF(under) * rest) + 127u) / 255u,
                   ((GUI_GREEN_OF(color) * share) + (GUI_GREEN_OF(under) * rest) + 127u) / 255u,
                   ((GUI_BLUE_OF(color) * share) + (GUI_BLUE_OF(under) * rest) + 127u) / 255u);
}

// the ground's pixel at x, y, the lattice as it stands under the window
static GuiColor ground_at(const GuiWindow *window, int x, int y)
{
    return window->ground.raster.pixels[((unsigned int)y * window->ground.raster.width) + (unsigned int)x];
}

// the glass over the ground at x, y: the tint over the lattice, grained as the window grains it
static GuiColor glass_at(const GuiWindow *window, int x, int y)
{
    const unsigned int grain = gui_grain()[(((unsigned int)y % GUI_GRAIN_TILE) * GUI_GRAIN_TILE) + ((unsigned int)x % GUI_GRAIN_TILE)];
    return over(over(ground_at(window, x, y), window->palette->glass), GUI_RGBA(0xffu, 0xffu, 0xffu, grain));
}

// whether any pixel of `rect` is other than the glass over the ground
static int painted_on_glass(const GuiWindow *window, GuiRect rect)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (at(window, x, y) != glass_at(window, x, y))
            {
                return 1;
            }
        }
    }
    return 0;
}

// the column from x along the row y whose ground pixel differs most from the ground color: a dot of the lattice
static int dot_column(const GuiWindow *window, int x, int y)
{
    int found = x;
    unsigned int most = 0u;
    for (int column = x; column < x + 40; column += 1)
    {
        const GuiColor pixel = ground_at(window, column, y);
        const unsigned int away = GUI_RED_OF(pixel) + GUI_GREEN_OF(pixel) + GUI_BLUE_OF(pixel);
        if (away > most)
        {
            most = away;
            found = column;
        }
    }
    return found;
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
    cli_check((window.ground.raster.width == 800u) && (window.ground.frames == 1u) &&
                  (window.ground.raster.pixels[(2u * 800u) + 2u] != ground) && (window.ground.raster.pixels[(499u * 800u) + 0u] == ground),
              "the ground is the lattice, a dot at the first point's origin over the ground color");
    cli_check((at(&window, 400, 20) == glass_at(&window, 400, 20)) && (at(&window, 400, 20) != ground_at(&window, 400, 20)),
              "the bar is the glass over the ground, tinted and grained");
    cli_check(at(&window, 20, 250) == glass_at(&window, 20, 250), "the strip is glass as the bar is");
    cli_check(at(&window, 400, 490) == glass_at(&window, 400, 490), "the status bar is glass as the bar is");
    cli_check(at(&window, 789, 250) == over(ground_at(&window, 789, 250), palette->glass), "the frame's gap at the right is tinted over the ground, with no grain");
    cli_check(painted_on_glass(&window, (GuiRect){800 - 5 - 34, 10, 34, 20}), "the close control is drawn");
    cli_check(painted_on_glass(&window, (GuiRect){800 - 5 - 106, 10, 34, 20}), "the minimize control is drawn");
    cli_check((at(&window, 44 + 1, 41 + 2) == over(ground_at(&window, 44 + 1, 41 + 2), palette->glass)) && (at(&window, 44 + 1, 41 + 2) != palette->surface),
              "the work's top left corner is rounded off to the frame's tint over the ground");
    cli_check(at(&window, 44 + 1 + 10, 41 + 2 + 10) == palette->surface, "the tabs row is the surface color");
    const int dot = dot_column(&window, 300, 300);
    cli_check((at(&window, dot, 300) == over(ground_at(&window, dot, 300), palette->editor_ground)) && (at(&window, dot, 300) != ground) &&
                  (at(&window, dot, 300) != ground_at(&window, dot, 300)),
              "the editor shows its ground at its opacity over the lattice, a dot dimmed through it");

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
    // the caret blinks: shown through the first half of the blink from the frame that began it, hidden through the
    // second, the window due at the flip
    window.editor.has_cursor = 1;
    window.editor.cursor = (GuiPlace){0ull, 0ull};
    window.editor.anchor = window.editor.cursor;
    window.caret_since_ms = cli_clock_milliseconds();
    const float cell = gui_glyphs_cell(&window.glyphs, GUI_CODE_SIZE);
    GuiRect first_cell;
    cli_check(gui_editor_place_rect(&window.editor, window.frame.editor, cell, 1.0f, 0ull, 0ull, &first_cell), "the first cell is among the rows drawn");
    gui_window_drawn(&window);
    cli_check(at(&window, first_cell.x, first_cell.y + 5) == palette->caret, "the caret is drawn as the blink begins");
    cli_check(gui_window_due(&window) <= GUI_CARET_BLINK / 2ull, "the window is due at the caret's flip");
    window.caret_since_ms -= (GUI_CARET_BLINK / 2ull) + 50ull;
    gui_window_ticked(&window);
    cli_check(window.owed, "the tick past the flip owes a frame");
    gui_window_drawn(&window);
    cli_check(at(&window, first_cell.x, first_cell.y + 5) != palette->caret, "the caret is hidden through the blink's second half");
    window.caret_since_ms -= GUI_CARET_BLINK / 2ull;
    gui_window_drawn(&window);
    cli_check(at(&window, first_cell.x, first_cell.y + 5) == palette->caret, "the caret is shown again through the next blink's first half");
    window.editor.has_cursor = 0;
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
