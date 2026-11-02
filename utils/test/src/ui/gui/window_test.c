// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_test.c: the window's check without a system: sized, it lays its parts out and draws them into its raster,
// the bar and the status bar tinted with the ink, the three controls at the bar's right end, the work's corners
// rounded; the pointer over the bar's empty part is the caption, over an edge a border, over a control that control,
// inside the work the client; the wheel scrolls the editor; and a file's text put whole shows its lines
#include "../../../../../ui/gui/src/parts/chrome.h"
#include "../../../../../ui/gui/src/parts/glass.h"
#include "../../../../../ui/gui/src/window.h"
#include "../cli/cli_check.h"

static void woken(void *context)
{
    (void)context;
}

static GuiColor at(const GuiWindow *window, int x, int y)
{
    return window->raster->pixels[((unsigned int)y * window->raster->width) + (unsigned int)x];
}

// the pixel at x, y of the raster shown
static GuiColor shown_at(const GuiWindow *window, int x, int y)
{
    return window->shown->pixels[((unsigned int)y * window->shown->width) + (unsigned int)x];
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

// whether two colors stand within a level of each other on every channel, as the base's one blend stands from two
static int near(GuiColor one, GuiColor other)
{
    const int red = (int)GUI_RED_OF(one) - (int)GUI_RED_OF(other);
    const int green = (int)GUI_GREEN_OF(one) - (int)GUI_GREEN_OF(other);
    const int blue = (int)GUI_BLUE_OF(one) - (int)GUI_BLUE_OF(other);
    return (red >= -1) && (red <= 1) && (green >= -1) && (green <= 1) && (blue >= -1) && (blue <= 1);
}

// the glass sheet over the ground at x, y: the tint over the lattice, grained as the window grains it; and a pane's
// thinner tint over it with the grain
static GuiColor glass_at(const GuiWindow *window, int x, int y)
{
    const unsigned int grain =
        gui_grain()[(((unsigned int)y % GUI_GRAIN_TILE) * GUI_GRAIN_TILE) + ((unsigned int)x % GUI_GRAIN_TILE)];
    return over(over(ground_at(window, x, y), window->palette->glass), GUI_RGBA(0xffu, 0xffu, 0xffu, grain));
}

static GuiColor pane_at(const GuiWindow *window, int x, int y)
{
    const unsigned int grain =
        gui_grain()[(((unsigned int)y % GUI_GRAIN_TILE) * GUI_GRAIN_TILE) + ((unsigned int)x % GUI_GRAIN_TILE)];
    return over(over(ground_at(window, x, y), gui_glass_pane_tint(window->palette->glass)),
                GUI_RGBA(0xffu, 0xffu, 0xffu, grain));
}

// whether any pixel of `rect` is other than the pane over the ground
static int painted_on_pane(const GuiWindow *window, GuiRect rect)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (!near(at(window, x, y), pane_at(window, x, y)))
            {
                return 1;
            }
        }
    }
    return 0;
}

// the column from x along the row y whose ground pixel differs most from the deep color: a dot of the lattice
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
    cli_check((window.raster->width == gui_raster_pitch(800u)) && (window.raster->height == 500u) &&
                  (window.raster->pages != NULL),
              "the raster is the window's size at the framebuffers' pitch, in the system's pages");
    gui_window_drawn(&window);
    const GuiPalette *const palette = window.palette;
    const GuiColor ground = palette->deep;
    cli_check((window.ground.raster.width == gui_raster_pitch(800u)) && (window.ground.frames == 1u) &&
                  (window.ground.raster.pixels[(2u * gui_raster_pitch(800u)) + 2u] != ground) &&
                  (window.ground.raster.pixels[(499u * gui_raster_pitch(800u)) + 0u] == ground),
              "the ground is the lattice, a dot at the first point's origin over the deep color");
    cli_check(near(at(&window, 400, 20), pane_at(&window, 400, 20)) &&
                  (at(&window, 400, 20) != ground_at(&window, 400, 20)),
              "the bar is a pane over the ground, the thinner tint grained");
    cli_check(near(at(&window, 20, 250), pane_at(&window, 20, 250)), "the strip is a pane as the bar is");
    cli_check(near(at(&window, 400, 490), pane_at(&window, 400, 490)), "the status bar is a pane as the bar is");
    cli_check(near(at(&window, 789, 250), glass_at(&window, 789, 250)),
              "the frame's gap at the right is the sheet over the ground, grained");
    cli_check(painted_on_pane(&window, (GuiRect){800 - 5 - 34, 10, 34, 20}), "the close control is drawn");
    cli_check(painted_on_pane(&window, (GuiRect){800 - 5 - 106, 10, 34, 20}), "the minimize control is drawn");
    cli_check(near(at(&window, 44, 41), glass_at(&window, 44, 41)) && (at(&window, 44, 41) != palette->surface),
              "the work's top left corner is rounded off to the sheet");
    cli_check((at(&window, 200, 41) == palette->signal) && (at(&window, 200, 42) == palette->signal),
              "the work's head is the signal line, two pixels deep");
    cli_check(at(&window, 44 + 1 + 10, 41 + 1) != palette->surface, "the lines stand outside the tabs row");
    cli_check(at(&window, 44 + 1 + 10, 41 + 2 + 10) == palette->surface, "the tabs row is the surface color");
    {
        const GuiRaster *const drawn = window.raster;
        gui_window_flipped(&window);
        cli_check((window.shown == drawn) && (window.raster != drawn) && (window.raster->pixels != NULL) &&
                      (window.raster->width == gui_raster_pitch(800u)),
                  "the frame presented is the raster shown and the other, the window's size, is drawn into next");
        gui_window_flipped(&window);
        cli_check(window.raster == drawn, "flipped again the first is drawn into again");
        window.shown = NULL;
    }
    const int dot = dot_column(&window, 300, 300);
    cli_check((at(&window, dot, 300) == over(ground_at(&window, dot, 300), palette->editor_ground)) &&
                  (at(&window, dot, 300) != ground) && (at(&window, dot, 300) != ground_at(&window, dot, 300)),
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

    cli_check(gui_window_text_put(&window, "one\ntwo\r\nthree", 14ull), "a text put whole is stored as lines");
    cli_check((window.editor.lines == 3ull) && (window.editor.count == 3u), "the text has three lines");
    gui_window_drawn(&window);
    cli_check(
        painted(&window, (GuiRect){window.frame.editor.x + 40, window.frame.editor.y, 100, 20}, palette->editor_ground),
        "the first line is drawn in the editor");
    // the caret breathes: at full ink through the first rest from the frame that began it, the window due at the
    // rest's end; sliding down through the next 300 ms, a frame owed at every tick; dim through the second rest; and
    // full again as the next breath begins
    window.editor.has_cursor = 1;
    window.editor.cursor = (GuiPlace){0ull, 0ull};
    window.editor.anchor = window.editor.cursor;
    const unsigned long long rest = (GUI_CARET_BREATH - (2ull * GUI_CARET_SLIDE)) / 2ull;
    window.caret_since_ms = cli_clock_milliseconds();
    const float cell = gui_glyphs_cell(&window.glyphs, GUI_CODE_SIZE);
    GuiRect first_cell;
    cli_check(gui_editor_place_rect(&window.editor, window.frame.editor, cell, 1.0f, 0ull, 0ull, &first_cell),
              "the first cell is among the rows drawn");
    gui_window_drawn(&window);
    cli_check(at(&window, first_cell.x, first_cell.y + 5) == palette->caret,
              "the caret is at full ink as the breath begins");
    cli_check(gui_window_due(&window) <= rest, "the window is due at the first rest's end");
    window.caret_since_ms = cli_clock_milliseconds() - rest - (GUI_CARET_SLIDE / 2ull);
    gui_window_ticked(&window);
    cli_check(window.owed || window.caret_alone, "a tick through the slide owes the caret drawn again");
    cli_check(gui_window_due(&window) == 1ull, "the window is due at once through the slide");
    gui_window_drawn(&window);
    const GuiColor sliding = at(&window, first_cell.x, first_cell.y + 5);
    cli_check((sliding != palette->caret) && (sliding != palette->editor_ground),
              "the caret is between full and dim through the slide");
    window.caret_since_ms = cli_clock_milliseconds() - rest - GUI_CARET_SLIDE - 10ull;
    gui_window_drawn(&window);
    const GuiColor dim = at(&window, first_cell.x, first_cell.y + 5);
    cli_check((dim != palette->caret) && (dim != sliding), "the caret is dimmer through the second rest");
    cli_check((gui_window_due(&window) > 1ull) && (gui_window_due(&window) <= rest),
              "the window is due at the second rest's end");
    window.caret_since_ms = cli_clock_milliseconds() - GUI_CARET_BREATH;
    gui_window_drawn(&window);
    cli_check(at(&window, first_cell.x, first_cell.y + 5) == palette->caret,
              "the caret is at full ink again as the next breath begins");
    // a tick that changes the caret's ink alone owes the caret drawn again by itself: its rows alone are to be
    // shown again, and the pixels beside it stand
    const GuiColor beside = at(&window, first_cell.x + 20, first_cell.y + 5);
    // the ground kept still, which keeps its loop from owing a frame of its own at the tick
    gui_ground_paused(&window.ground, 1, cli_clock_milliseconds());
    window.caret_since_ms = cli_clock_milliseconds() - rest - (GUI_CARET_SLIDE / 2ull);
    gui_window_ticked(&window);
    cli_check(!window.owed && window.caret_alone,
              "a tick through the slide with nothing else owed owes the caret alone, and no frame whole");
    const GuiRect redrawn = gui_window_caret_drawn(&window);
    cli_check(!gui_rect_empty(redrawn) && (redrawn.y == first_cell.y) && (redrawn.height == first_cell.height) &&
                  !window.owed && !window.caret_alone,
              "the caret drawn again answers its rows and pays the frame owed");
    cli_check((at(&window, first_cell.x, first_cell.y + 5) != palette->caret) &&
                  (at(&window, first_cell.x + 20, first_cell.y + 5) == beside),
              "the caret stands dimmer and the pixels beside it as they were");
    window.owed = 1;
    window.caret_since_ms = cli_clock_milliseconds() - rest - GUI_CARET_SLIDE - 10ull;
    gui_window_ticked(&window);
    cli_check(window.owed && !window.caret_alone, "a tick with a frame owed already owes the frame whole");
    window.editor.has_cursor = 0;
    gui_window_wheel(&window, 300, 300, 1);
    cli_check(window.editor.top == 1ull, "the wheel over the editor scrolls it a line");
    gui_window_wheel(&window, 400, 20, 1);
    cli_check(window.editor.top == 1ull, "the wheel over the bar scrolls nothing");

    // the busy circle: an ask standing past the wait brings it in a frame whole; each stage after that is a blit of its rectangle
    // alone, the pixels beside it standing; an answer takes it away in a frame whole
    gui_window_drawn(&window);
    const GuiRect circle = gui_busy_rect(&window.frame);
    const GuiColor under_circle = at(&window, circle.x + (circle.width / 2), circle.y + 3);
    // the asks this check has left unanswered let go, one ask of its own the only one standing
    gui_busy_made(&window.busy);
    const unsigned long long asked_ticket = window.link.tickets + 1ull;
    gui_busy_asked(&window.busy, asked_ticket, cli_clock_milliseconds());
    cli_check((gui_window_due(&window) > 1ull) && (gui_window_due(&window) <= GUI_BUSY_WAITS), "an ask waiting, the window is due as the wait ends");
    gui_window_ticked(&window);
    cli_check(!window.busy.shown, "through the wait no circle stands");
    window.owed = 0;
    window.busy.asked_ms[asked_ticket & (GUI_BUSY_ASKS - 1u)] = cli_clock_milliseconds() - GUI_BUSY_WAITS;
    gui_window_ticked(&window);
    cli_check(window.busy.shown && window.owed && !window.busy_alone, "the ask standing past the wait, the circle comes in a frame owed whole");
    gui_window_drawn(&window);
    cli_check(window.busy.under_kept && (at(&window, circle.x + (circle.width / 2), circle.y + 2) != under_circle), "the frame lays it over everything at the editor's foot");
    gui_window_flipped(&window);
    // a stage's time on: its rectangle alone is owed, and no frame whole; the frame's other pixels stand
    window.busy.shown_ms -= GUI_BUSY_STAGE_MS * 5ull;
    const GuiColor beside_circle = shown_at(&window, circle.x - 2, circle.y + 2);
    gui_window_ticked(&window);
    cli_check(window.busy_alone && !window.owed, "a stage's step with nothing else owed owes the circle alone");
    cli_check((gui_window_due(&window) >= 1ull) && (gui_window_due(&window) <= GUI_BUSY_STAGE_MS), "the window is due at the next stage");
    const GuiRect stepped = gui_window_busy_drawn(&window);
    cli_check((stepped.x == circle.x) && (stepped.y == circle.y) && (stepped.width == circle.width) && !window.busy_alone && !window.owed,
              "the step answers the circle's rectangle and pays what it owed");
    cli_check((window.busy.stage_drawn == gui_busy_stage_at(&window.busy, cli_clock_milliseconds())) && (shown_at(&window, circle.x - 2, circle.y + 2) == beside_circle), "the stage stands on the frame shown and the pixels beside it as they were");
    // a frame owed before the tick is paid whole, whatever the step wanted
    window.busy.shown_ms -= GUI_BUSY_STAGE_MS;
    window.owed = 1;
    gui_window_ticked(&window);
    cli_check(window.owed && !window.busy_alone, "a tick with a frame owed already owes the frame whole");
    window.owed = 0;
    gui_window_drawn(&window);
    gui_window_flipped(&window);
    window.busy.shown_ms -= GUI_BUSY_STAGE_MS;
    gui_window_ticked(&window);
    cli_check(window.busy_alone && !window.owed, "and a step alone again");
    // what comes between the tick and the drawing owes its frame beside the step, and the frame whole pays both
    window.owed = 1;
    gui_window_drawn(&window);
    cli_check(!window.owed && !window.busy_alone && !window.caret_alone, "the frame whole pays the step with it");
    gui_window_flipped(&window);
    // a caret under the circle: both are laid in a frame whole, neither blit knowing what the other has laid
    window.busy.shown_ms -= GUI_BUSY_STAGE_MS;
    window.editor.caret_kept = 1;
    window.editor.caret_rect = (GuiRect){circle.x + 2, circle.y + 2, 2, 10};
    gui_window_ticked(&window);
    cli_check(window.owed && !window.busy_alone, "a caret under the circle owes the frame whole");
    window.editor.caret_kept = 0;
    gui_window_drawn(&window);
    gui_window_flipped(&window);
    // less motion: the circle stands on its one stage and no stage's tick is wanted
    gui_window_motion_told(&window, 1);
    cli_check(window.busy.still && window.busy.shown && (window.busy.cell == NULL) && window.owed, "less motion asked, the circle lets its stages go and a frame is owed");
    gui_window_drawn(&window);
    cli_check(window.busy.stages == 1u, "the frame makes its one stage");
    gui_window_flipped(&window);
    window.busy.shown_ms -= GUI_BUSY_STAGE_MS * 3ull;
    window.owed = 0;
    gui_window_ticked(&window);
    cli_check(!window.busy_alone && !window.owed, "standing still it owes no step");
    gui_window_motion_told(&window, 0);
    gui_ground_paused(&window.ground, 1, cli_clock_milliseconds());
    gui_window_drawn(&window);
    gui_window_flipped(&window);
    // the answer takes it away
    window.busy.shown_ms -= GUI_BUSY_STANDS;
    gui_busy_answered(&window.busy, asked_ticket);
    window.owed = 0;
    gui_window_ticked(&window);
    cli_check(!window.busy.shown && window.owed && (window.busy.cell == NULL), "the ask answered, the circle goes in a frame owed whole and its cell is given back");
    gui_window_drawn(&window);
    cli_check(at(&window, circle.x + (circle.width / 2), circle.y + 3) == under_circle, "and the frame stands as it stood");
    gui_window_flipped(&window);
    window.owed = 1;

    // the window's icon: the eye in the text's color with its own alpha, clear at a corner
    GuiRaster icon;
    cli_check(gui_window_icon(&window, &icon, 32u) && (icon.width == 32u), "the window's icon is made at its size");
    unsigned int inked = 0u;
    for (unsigned int at_pixel = 0u; at_pixel < 32u * 32u; at_pixel += 1u)
    {
        inked += (GUI_ALPHA_OF(icon.pixels[at_pixel]) != 0u) ? 1u : 0u;
    }
    cli_check((inked > 50u) && (GUI_ALPHA_OF(icon.pixels[0]) == 0u) &&
                  ((icon.pixels[(16u * 32u) + 16u] & 0x00ffffffu) == (palette->text & 0x00ffffffu)),
              "the icon's pixels are the text's color with the eye's cover as their alpha");
    gui_raster_given(&icon);

    // the pointer gone out of the window: a rest waiting in an edge's band does not come
    GuiFrame claimed_frame = window.frame;
    gui_claim_set(&window.claim, 1, cli_clock_milliseconds());
    gui_window_moved(&window, 400, 2);
    gui_window_moved(&window, 400, 3);
    cli_check(window.claim.rest_due != 0ull, "a pointer in the head's band waits on its rest");
    gui_window_left(&window);
    cli_check(window.claim.rest_due == 0ull, "the pointer gone, the rest does not come");
    gui_claim_set(&window.claim, 0, cli_clock_milliseconds());
    window.frame = claimed_frame;

    // the controls' tab's shadow, 0 6px 18px --lifted under its pane at the pane's share, none where it stands in the bar
    GuiMask shadow = {0u, 0u, 0, 0, NULL};
    GuiRaster shaded;
    cli_check(gui_raster_made(&shaded, 800u, 500u), "a raster for the shadow is made");
    const GuiColor plain_gray = GUI_RGB(0x80u, 0x80u, 0x80u);
    const GuiUnder gray_table = {NULL, 0u, NULL, 0u, plain_gray};
    GuiFrame tabbed = window.frame;
    gui_raster_filled(&shaded, (GuiRect){0, 0, 800, 500}, plain_gray);
    gui_controls_drawn(&shaded, palette, &tabbed, gray_table, &shadow);
    const GuiRect tab = tabbed.controls_pane;
    const int tab_middle = tab.x + (tab.width / 2);
    GuiClaimNumbers tab_numbers = tabbed.claim;
    const int foot_at = (tab.y + tab.height + 8);
    cli_check(shaded.pixels[((unsigned int)foot_at * 800u) + (unsigned int)tab_middle] == plain_gray,
              "the controls standing in the bar cast no shadow");
    tab_numbers.claim = GUI_WHOLE;
    tab_numbers.reveal_down = GUI_WHOLE;
    gui_frame_claimed(&tabbed, &tab_numbers);
    gui_raster_filled(&shaded, (GuiRect){0, 0, 800, 500}, plain_gray);
    gui_controls_drawn(&shaded, palette, &tabbed, gray_table, &shadow);
    const GuiRect pane_rect = tabbed.controls_pane;
    const int pane_middle = pane_rect.x + (pane_rect.width / 2);
    const int foot = pane_rect.y + pane_rect.height;
    const unsigned int under_pane = GUI_RED_OF(shaded.pixels[((unsigned int)(foot + 8) * 800u) + (unsigned int)pane_middle]);
    const unsigned int far_under = GUI_RED_OF(shaded.pixels[((unsigned int)(foot + 40) * 800u) + (unsigned int)pane_middle]);
    const unsigned int above_pane = GUI_RED_OF(shaded.pixels[((unsigned int)(pane_rect.y - 8) * 800u) + (unsigned int)pane_middle]);
    cli_check(under_pane < 0x70u, "the tab casts its shadow below it");
    cli_check(far_under == 0x80u, "the shadow ends past its blur");
    cli_check(above_pane > under_pane, "the shadow falls six pixels down, less of it above the tab");
    tab_numbers.reveal_down = GUI_WHOLE / 2;
    gui_frame_claimed(&tabbed, &tab_numbers);
    gui_raster_filled(&shaded, (GuiRect){0, 0, 800, 500}, plain_gray);
    gui_controls_drawn(&shaded, palette, &tabbed, gray_table, &shadow);
    const GuiRect half_rect = tabbed.controls_pane;
    const unsigned int under_half = GUI_RED_OF(shaded.pixels[((unsigned int)(half_rect.y + half_rect.height + 8) * 800u) + (unsigned int)(half_rect.x + (half_rect.width / 2))]);
    cli_check((under_half > under_pane) && (under_half < 0x80u), "the shadow goes with the pane's share of the glass");
    gui_controls_shadow_given(&shadow);
    cli_check(shadow.cover == NULL, "the shadow's cell is given back");
    gui_raster_given(&shaded);

    // the lattice lies on --deep, as the page's canvas does: most of its pixels are that color
    unsigned int on_deep = 0u;
    for (int x = 0; x < 200; x += 1)
    {
        on_deep += (ground_at(&window, x, 250) == window.palette->deep) ? 1u : 0u;
    }
    cli_check(on_deep > 100u, "the lattice's dots lie on --deep");

    // the name: the eye's place the caption's until it is home; while the pane stands the view is not drawn, the lattice
    // showing through the work with the name over it, and it comes back as the pane goes
    const GuiRect eye = gui_wordmark_eye_square(&window.frame);
    cli_check(gui_window_hit(&window, eye.x + 5, eye.y + 5) == GUI_HIT_CAPTION, "the eye's place is the caption's until it is home");
    gui_wordmark_landed(&window.wordmark);
    cli_check(gui_window_hit(&window, eye.x + 5, eye.y + 5) == GUI_HIT_CLIENT, "the eye home is the client's");
    gui_window_drawn(&window);
    cli_check(painted_on_pane(&window, eye), "the eye home is drawn over the bar");
    window.wordmark.home = 0;
    gui_wordmark_stood(&window.wordmark, cli_clock_milliseconds());
    gui_window_drawn(&window);
    const GuiRect editor = window.frame.editor;
    GuiWordmarkPlace place;
    gui_wordmark_placed(&window.glyphs, &window.frame, &place);
    // how many pixels of the editor's rows above the name stand off the lattice
    const int rows_above = place.name.y - editor.y;
    int off_lattice = 0;
    for (int y = editor.y; y < editor.y + rows_above; y += 1)
    {
        for (int x = editor.x; x < editor.x + editor.width; x += 1)
        {
            off_lattice += (at(&window, x, y) != ground_at(&window, x, y)) ? 1 : 0;
        }
    }
    cli_check(off_lattice == 0, "while the name's pane stands the lattice shows through the work");
    int name_inked = 0;
    for (int x = place.name.x; x < place.name.x + place.name.width; x += 1)
    {
        for (int y = place.name.y; y < place.name.y + place.name.height; y += 1)
        {
            name_inked += (at(&window, x, y) == window.palette->open_text) ? 1 : 0;
        }
    }
    cli_check(name_inked > 200, "and the name stands in its color");
    window.wordmark.stands = 0;
    gui_window_drawn(&window);
    for (int y = editor.y; y < editor.y + rows_above; y += 1)
    {
        for (int x = editor.x; x < editor.x + editor.width; x += 1)
        {
            off_lattice += (at(&window, x, y) != ground_at(&window, x, y)) ? 1 : 0;
        }
    }
    cli_check(off_lattice > 0, "the pane gone, the view comes back");

    // a frame of a movement read from its stage is the frame the claim's numbers lay at its moment, pixel for pixel: the
    // claim coming on, the bar coming in with its notch in the work, and its room coming after it
    {
        GuiRaster direct;
        cli_check(gui_raster_made(&direct, window.raster->width, window.raster->height), "a raster to hold a frame is made");
        const unsigned long long pixels = (unsigned long long)window.raster->width * window.raster->height;
        const unsigned long long claim_on = 500000ull;
        const unsigned long long bar_in = claim_on + 2000ull;
        const unsigned long long moments[8] = {claim_on + 40ull, claim_on + 150ull, claim_on + 290ull, bar_in + 40ull, bar_in + 150ull, bar_in + 280ull, bar_in + 400ull, bar_in + 560ull};
        gui_claim_set(&window.claim, 1, claim_on);
        int same_frames = 1;
        int stage_read = 1;
        for (unsigned int at = 0u; at < 8u; at += 1u)
        {
            if (at == 3u)
            {
                gui_claim_moved(&window.claim, 400, 4, GUI_CLAIM_TOP, 0, bar_in - GUI_CLAIM_RESTS);
                (void)gui_claim_stepped(&window.claim, bar_in);
            }
            GuiClaim numbers_from = window.claim;
            gui_window_claim_laid(&window, moments[at]);
            stage_read = stage_read && (window.stage != NULL);
            gui_window_drawn(&window);
            memor.cpy(&(MemoriaCfg){.dst = direct.pixels, .src = window.raster->pixels, .bytes = pixels * sizeof(GuiColor)});
            // the same moment laid by hand, no stage standing; the slats' shares as the stage has them
            const GuiStage *const was = window.stage;
            gui_stage_claim_taken(was, &numbers_from);
            GuiClaimNumbers numbers;
            GuiFrame laid = window.frame;
            const unsigned long long stage_moment = window.stages.began + (((moments[at] - window.stages.began) / GUI_STAGE_MS) * GUI_STAGE_MS);
            gui_claim_numbers(&numbers_from, stage_moment, &numbers);
            numbers.grown_top = was->frame.claim.grown_top;
            numbers.grown_left = was->frame.claim.grown_left;
            numbers.grown_bottom = was->frame.claim.grown_bottom;
            gui_frame_claimed(&laid, &numbers);
            const GuiFrame kept_frame = window.frame;
            const GuiStage *const kept_stage = window.stage;
            window.stage = NULL;
            window.frame = laid;
            gui_window_drawn(&window);
            same_frames = same_frames && (memor.cmp(&(MemoriaCfg){.src = direct.pixels, .other = window.raster->pixels, .bytes = pixels * sizeof(GuiColor)}) == 0);
            window.frame = kept_frame;
            window.stage = kept_stage;
        }
        cli_check(stage_read, "each frame of the movement stands on a stage");
        cli_check(same_frames, "and draws the pixels the numbers lay by hand");
        gui_window_claim_laid(&window, bar_in + 900ull);
        cli_check((window.stage == NULL) && (window.stages.stage == NULL), "at rest no stage stands and the cell is given back");
        gui_raster_given(&direct);
        gui_claim_made(&window.claim);
        gui_window_claim_laid(&window, bar_in + 1000ull);
    }

    // less motion asked for: the system's wish or View's Reduce Motion stills the lattice, the slides, the name and the caret
    gui_window_motion_told(&window, 1);
    cli_check(window.motion_still && (window.ground.paused_since_ms != 0ull) && window.wordmark.still && window.claim.still,
              "the system's wish stills the lattice, the claim and the name");
    const int had_cursor = window.editor.has_cursor;
    window.editor.has_cursor = 1;
    window.caret_since_ms = cli_clock_milliseconds() - (GUI_CARET_BREATH / 2ull);
    gui_window_drawn(&window);
    cli_check(window.caret_ink == 255u, "the caret stands at full ink");
    cli_check(gui_window_due(&window) == 0ull, "and nothing wants the clock");
    gui_window_motion_told(&window, 0);
    window.caret_since_ms = cli_clock_milliseconds() - (GUI_CARET_BREATH / 2ull);
    gui_window_drawn(&window);
    cli_check(!window.motion_still && (window.ground.paused_since_ms == 0ull) && (window.caret_ink < 255u), "no longer asked for, they move again");
    window.editor.has_cursor = had_cursor;
    gui_window_shown(&window, 0);
    gui_window_motion_told(&window, 1);
    gui_window_shown(&window, 1);
    cli_check(window.ground.paused_since_ms != 0ull, "a window back on the screen stays still while less motion is asked for");
    gui_window_motion_told(&window, 0);
    cli_check(window.ground.paused_since_ms == 0ull, "and moves as it no longer is");
    gui_window_shown(&window, 0);
    gui_window_motion_told(&window, 1);
    gui_window_motion_told(&window, 0);
    cli_check(window.ground.paused_since_ms != 0ull, "a window off the screen stays still as the wish goes");
    gui_window_shown(&window, 1);
    const GuiChoice reduce = {"reduce-motion", ""};
    gui_window_chosen(&window, &reduce);
    cli_check(window.motion_kept && window.motion_still, "View's Reduce Motion asks for it");
    gui_window_chosen(&window, &reduce);
    cli_check(!window.motion_kept && !window.motion_still, "and chosen again no longer does");

    cli_check(gui_window_sized(&window, 640u, 400u, 1.0f), "the window sized again remakes its raster");
    cli_check((window.raster->width == 640u) && (window.frame.bar.width == 640), "the parts follow the new size");

    gui_window_ended(&window);
    cli_check(window.raster->pixels == NULL, "the window ended holds no raster");
    return cli_checked();
}
