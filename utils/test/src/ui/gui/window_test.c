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
#include "test_clock.h"


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
    return over(over(ground_at(window, x, y), EMBED_CALL(gui_glass.pane_tint, GuiGlassCfg, .glass = window->palette->glass)),
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
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    static GuiWindow window;
    const int begun = EMBED_CALL(gui_window_woken.begin, GuiWindowWokenCfg, .window = &window, .woken = woken, .context = NULL, .err_msg = &why);
    cli_check(begun, "the window begins");
    if (!begun)
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_on_check();
    }
    cli_check(EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = &window, .width = 800u, .height = 500u, .system_scale = 100), "the window sized lays its parts out and makes its raster");
    cli_check((window.raster->width == EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 800u)) && (window.raster->height == 500u) &&
                  (window.raster->pages != NULL),
              "the raster is the window's size at the framebuffers' pitch, in the system's pages");
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    const GuiPalette *const palette = window.palette;
    const GuiColor ground = palette->deep;
    cli_check((window.ground.raster.width == EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 800u)) && (window.ground.frames == 1u) &&
                  (window.ground.raster.pixels[(2u * EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 800u)) + 2u] != ground) &&
                  (window.ground.raster.pixels[(499u * EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 800u)) + 0u] == ground),
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
        EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &window);
        cli_check((window.shown == drawn) && (window.raster != drawn) && (window.raster->pixels != NULL) &&
                      (window.raster->width == EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = 800u)),
                  "the frame presented is the raster shown and the other, the window's size, is drawn into next");
        EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &window);
        cli_check(window.raster == drawn, "flipped again the first is drawn into again");
        window.shown = NULL;
    }
    const int dot = dot_column(&window, 300, 300);
    cli_check((at(&window, dot, 300) == over(ground_at(&window, dot, 300), palette->editor_ground)) &&
                  (at(&window, dot, 300) != ground) && (at(&window, dot, 300) != ground_at(&window, dot, 300)),
              "the editor shows its ground at its opacity over the lattice, a dot dimmed through it");

    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(400, 20)) == GUI_HIT_CAPTION, "the bar's empty part is the caption");
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(800 - 5 - 17, 20)) == GUI_HIT_CLOSE, "the rightmost control is close");
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(800 - 5 - 34 - 2 - 17, 20)) == GUI_HIT_MAXIMIZE, "the middle control is maximize");
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(800 - 5 - 72 - 17, 20)) == GUI_HIT_MINIMIZE, "the leftmost control is minimize");
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(400, 2)) == GUI_HIT_TOP, "the top edge is a border");
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(2, 2)) == GUI_HIT_TOP_LEFT, "the top left corner is a corner");
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(797, 497)) == GUI_HIT_BOTTOM_RIGHT, "the bottom right corner is a corner");
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(2, 250)) == GUI_HIT_LEFT, "the left edge is a border");
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(300, 300)) == GUI_HIT_CLIENT, "the work is the client");
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(400, 490)) == GUI_HIT_CLIENT, "the status bar is the client");

    cli_check(EMBED_CALL(gui_window_text_table.text_put, GuiWindowTextCfg, .window = &window, .text = "one\ntwo\r\nthree", .length = 14ull), "a text put whole is stored as lines");
    cli_check((window.editor.lines == 3ull) && (window.editor.count == 3u), "the text has three lines");
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(
        painted(&window, (GuiRect){window.frame.editor.x + 40, window.frame.editor.y, 100, 20}, palette->editor_ground),
        "the first line is drawn in the editor");
    // the caret breathes: at full ink through the first rest from the frame that began it, the window due at the
    // rest's end; sliding down through the next 300 ms, a frame owed at every tick; dim through the second rest; and
    // full again as the next breath begins
    window.editor.has_cursor = 1;
    window.editor.cursor = (GuiPlace){0ull, 0ull};
    window.editor.anchor = window.editor.cursor;
    const unsigned long long rest = (GUI_CARET_BLINK_PERIOD_MS - (2ull * GUI_CARET_FADE_MS)) / 2ull;
    window.caret_blink_start_ms = cli_clock_milliseconds();
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &window.glyphs, .size = GUI_CODE_SIZE);
    GuiRect first_cell;
    cli_check(EMBED_CALL(gui_editor_place_cell.place_rect, GuiEditorPlaceCellCfg, .editor_read = &window.editor, .place = window.frame.editor, .cell = cell, .scale = 100, .line = 0ull, .col = 0ull, .rect = &first_cell),
              "the first cell is among the rows drawn");
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(at(&window, first_cell.x, first_cell.y + 5) == palette->caret,
              "the caret is at full ink as the breath begins");
    cli_check(EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &window) <= rest, "the window is due at the first rest's end");
    window.caret_blink_start_ms = cli_clock_milliseconds() - rest - (GUI_CARET_FADE_MS / 2ull);
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(window.owed || window.caret_alone, "a tick through the slide owes the caret drawn again");
    cli_check(EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &window) == 1ull, "the window is due at once through the slide");
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    const GuiColor sliding = at(&window, first_cell.x, first_cell.y + 5);
    cli_check((sliding != palette->caret) && (sliding != palette->editor_ground),
              "the caret is between full and dim through the slide");
    window.caret_blink_start_ms = cli_clock_milliseconds() - rest - GUI_CARET_FADE_MS - 10ull;
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    const GuiColor dim = at(&window, first_cell.x, first_cell.y + 5);
    cli_check((dim != palette->caret) && (dim != sliding), "the caret is dimmer through the second rest");
    cli_check((EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &window) > 1ull) && (EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &window) <= rest),
              "the window is due at the second rest's end");
    window.caret_blink_start_ms = cli_clock_milliseconds() - GUI_CARET_BLINK_PERIOD_MS;
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(at(&window, first_cell.x, first_cell.y + 5) == palette->caret,
              "the caret is at full ink again as the next breath begins");
    // a tick that changes the caret's ink alone owes the caret drawn again by itself: its rows alone are to be
    // shown again, and the pixels beside it stand
    const GuiColor beside = at(&window, first_cell.x + 20, first_cell.y + 5);
    // the ground saved still, which saves its loop from owing a frame of its own at the tick
    EMBED_CALL(gui_ground.pause, GuiGroundCfg, .ground = &window.ground, .paused = 1);
    window.caret_blink_start_ms = cli_clock_milliseconds() - rest - (GUI_CARET_FADE_MS / 2ull);
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(!window.owed && window.caret_alone,
              "a tick through the slide with nothing else owed owes the caret alone, and no frame whole");
    const GuiRect redrawn = EMBED_CALL(gui_window_table.caret_draw, GuiWindowCfg, .window = &window);
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = redrawn) && (redrawn.y == first_cell.y) && (redrawn.height == first_cell.height) &&
                  !window.owed && !window.caret_alone,
              "the caret drawn again answers its rows and pays the frame owed");
    cli_check((at(&window, first_cell.x, first_cell.y + 5) != palette->caret) &&
                  (at(&window, first_cell.x + 20, first_cell.y + 5) == beside),
              "the caret stands dimmer and the pixels beside it as they were");
    window.owed = 1;
    window.caret_blink_start_ms = cli_clock_milliseconds() - rest - GUI_CARET_FADE_MS - 10ull;
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(window.owed && !window.caret_alone, "a tick with a frame owed already owes the frame whole");
    window.editor.has_cursor = 0;
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = &window, .input = GUI_WHEEL_INPUT(300, 300, 1));
    cli_check(window.editor.top == 1ull, "the wheel over the editor scrolls it a line");
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = &window, .input = GUI_WHEEL_INPUT(400, 20, 1));
    cli_check(window.editor.top == 1ull, "the wheel over the bar scrolls nothing");

    // the busy circle: an ask standing past the wait brings it in a frame whole; each stage after that is a blit of its rectangle
    // alone, the pixels beside it standing; an answer takes it away in a frame whole
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    const GuiRect circle = EMBED_CALL(gui_busy_frame.rect, GuiBusyFrameCfg, .frame = &window.frame);
    const GuiColor under_circle = at(&window, circle.x + (circle.width / 2), circle.y + 3);
    // the asks this check has left unanswered let go, one ask of its own the only one standing
    EMBED_CALL(cli_busy_still.init, GuiBusyStillCfg, .busy = &window.busy);
    const unsigned long long asked_ticket = window.cli.tickets + 1ull;
    EMBED_CALL(gui_busy_ticket.request, GuiBusyTicketCfg, .busy = &window.busy, .ticket = asked_ticket);
    cli_check((EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &window) > 1ull) && (EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &window) <= GUI_BUSY_SHOW_DELAY_MS), "an ask waiting, the window is due as the wait ends");
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(!window.busy.shown, "through the wait no circle stands");
    window.owed = 0;
    window.busy.request_start_ms[asked_ticket & (GUI_BUSY_ASKS - 1u)] = cli_clock_milliseconds() - GUI_BUSY_SHOW_DELAY_MS;
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(window.busy.shown && window.owed && !window.busy_alone, "the ask standing past the wait, the circle comes in a frame owed whole");
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(window.busy.under_saved && (at(&window, circle.x + (circle.width / 2), circle.y + 2) != under_circle), "the frame lays it over everything at the editor's foot");
    EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &window);
    // a stage's time on: its rectangle alone is owed, and no frame whole; the frame's other pixels stand
    window.busy.visible_start_ms -= GUI_BUSY_STAGE_MS * 5ull;
    const GuiColor beside_circle = shown_at(&window, circle.x - 2, circle.y + 2);
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(window.busy_alone && !window.owed, "a stage's step with nothing else owed owes the circle alone");
    cli_check((EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &window) >= 1ull) && (EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &window) <= GUI_BUSY_STAGE_MS), "the window is due at the next stage");
    const GuiRect stepped = EMBED_CALL(gui_window_table.busy_draw, GuiWindowCfg, .window = &window);
    cli_check((stepped.x == circle.x) && (stepped.y == circle.y) && (stepped.width == circle.width) && !window.busy_alone && !window.owed,
              "the step answers the circle's rectangle and pays what it owed");
    cli_check((window.busy.stage_drawn == EMBED_CALL(gui_busy.stage_at, GuiBusyCfg, .busy_read = &window.busy)) && (shown_at(&window, circle.x - 2, circle.y + 2) == beside_circle), "the stage stands on the frame shown and the pixels beside it as they were");
    // a frame owed before the tick is paid whole, whatever the step wanted
    window.busy.visible_start_ms -= GUI_BUSY_STAGE_MS;
    window.owed = 1;
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(window.owed && !window.busy_alone, "a tick with a frame owed already owes the frame whole");
    window.owed = 0;
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &window);
    window.busy.visible_start_ms -= GUI_BUSY_STAGE_MS;
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(window.busy_alone && !window.owed, "and a step alone again");
    // what comes between the tick and the drawing owes its frame beside the step, and the frame whole pays both
    window.owed = 1;
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(!window.owed && !window.busy_alone && !window.caret_alone, "the frame whole pays the step with it");
    EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &window);
    // a caret under the circle: both are laid in a frame whole, neither blit knowing what the other has laid
    window.busy.visible_start_ms -= GUI_BUSY_STAGE_MS;
    window.editor.caret_saved = 1;
    window.editor.caret_rect = (GuiRect){circle.x + 2, circle.y + 2, 2, 10};
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(window.owed && !window.busy_alone, "a caret under the circle owes the frame whole");
    window.editor.caret_saved = 0;
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &window);
    // less motion: the circle stands on its one stage and no stage's tick is wanted
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = &window, .reduced = 1);
    cli_check(window.busy.still && window.busy.shown && (window.busy.cell == NULL) && window.owed, "less motion asked, the circle lets its stages go and a frame is owed");
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(window.busy.stages == 1u, "the frame makes its one stage");
    EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &window);
    window.busy.visible_start_ms -= GUI_BUSY_STAGE_MS * 3ull;
    window.owed = 0;
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(!window.busy_alone && !window.owed, "standing still it owes no step");
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = &window, .reduced = 0);
    EMBED_CALL(gui_ground.pause, GuiGroundCfg, .ground = &window.ground, .paused = 1);
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &window);
    // the answer takes it away
    window.busy.visible_start_ms -= GUI_BUSY_MIN_VISIBLE_MS;
    EMBED_CALL(gui_busy_ticket.answer, GuiBusyTicketCfg, .busy = &window.busy, .ticket = asked_ticket);
    window.owed = 0;
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &window);
    cli_check(!window.busy.shown && window.owed && (window.busy.cell == NULL), "the ask answered, the circle goes in a frame owed whole and its cell is given back");
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(at(&window, circle.x + (circle.width / 2), circle.y + 3) == under_circle, "and the frame stands as it stood");
    EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &window);
    window.owed = 1;

    // the window's icon: the eye in the text's color with its own alpha, clear at a corner
    GuiRaster icon;
    cli_check(EMBED_CALL(cli_window_icon.icon, GuiWindowIconCfg, .window = &window, .icon = &icon, .size = 32u) && (icon.width == 32u), "the window's icon is made at its size");
    unsigned int inked = 0u;
    for (unsigned int at_pixel = 0u; at_pixel < 32u * 32u; at_pixel += 1u)
    {
        inked += (GUI_ALPHA_OF(icon.pixels[at_pixel]) != 0u) ? 1u : 0u;
    }
    cli_check((inked > 50u) && (GUI_ALPHA_OF(icon.pixels[0]) == 0u) &&
                  ((icon.pixels[(16u * 32u) + 16u] & 0x00ffffffu) == (palette->text & 0x00ffffffu)),
              "the icon's pixels are the text's color with the eye's cover as their alpha");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &icon);

    // the pointer gone out of the window: a rest waiting in an edge's band does not come
    GuiFrame claimed_frame = window.frame;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &window.claim, .on = 1);
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = &window, .input = GUI_MOVED_INPUT(400, 2));
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = &window, .input = GUI_MOVED_INPUT(400, 3));
    cli_check(window.claim.show_deadline_ms != 0ull, "a pointer in the head's band waits on its rest");
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = &window, .input = GUI_LEFT_INPUT());
    cli_check(window.claim.show_deadline_ms == 0ull, "the pointer gone, the rest does not come");
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &window.claim, .on = 0);
    window.frame = claimed_frame;

    // the controls' tab's shadow, 0 6px 18px --lifted under its pane at the pane's share, none where it stands in the bar
    GuiMask shadow = {0u, 0u, 0, 0, NULL};
    GuiRaster shaded;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &shaded, .width = 800u, .height = 500u), "a raster for the shadow is made");
    const GuiColor plain_gray = GUI_RGB(0x80u, 0x80u, 0x80u);
    const GuiUnder gray_table = {NULL, 0u, NULL, 0u, plain_gray};
    GuiFrame tabbed = window.frame;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &shaded, .rect = (GuiRect){0, 0, 800, 500}, .color = plain_gray);
    EMBED_CALL(gui_chrome_raster.controls_draw, GuiChromeRasterCfg, .raster = &shaded, .palette = palette, .frame = &tabbed, .table = gray_table, .shadow = &shadow);
    const GuiRect tab = tabbed.controls_pane;
    const int tab_middle = tab.x + (tab.width / 2);
    GuiClaimNumbers tab_numbers = tabbed.claim;
    const int foot_at = (tab.y + tab.height + 8);
    cli_check(shaded.pixels[((unsigned int)foot_at * 800u) + (unsigned int)tab_middle] == plain_gray,
              "the controls standing in the bar cast no shadow");
    tab_numbers.claim = GUI_WHOLE;
    tab_numbers.reveal_down = GUI_WHOLE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &tabbed, .numbers = &tab_numbers);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &shaded, .rect = (GuiRect){0, 0, 800, 500}, .color = plain_gray);
    EMBED_CALL(gui_chrome_raster.controls_draw, GuiChromeRasterCfg, .raster = &shaded, .palette = palette, .frame = &tabbed, .table = gray_table, .shadow = &shadow);
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
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &tabbed, .numbers = &tab_numbers);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &shaded, .rect = (GuiRect){0, 0, 800, 500}, .color = plain_gray);
    EMBED_CALL(gui_chrome_raster.controls_draw, GuiChromeRasterCfg, .raster = &shaded, .palette = palette, .frame = &tabbed, .table = gray_table, .shadow = &shadow);
    const GuiRect half_rect = tabbed.controls_pane;
    const unsigned int under_half = GUI_RED_OF(shaded.pixels[((unsigned int)(half_rect.y + half_rect.height + 8) * 800u) + (unsigned int)(half_rect.x + (half_rect.width / 2))]);
    cli_check((under_half > under_pane) && (under_half < 0x80u), "the shadow goes with the pane's share of the glass");
    EMBED_CALL(gui_chrome_raster.controls_shadow_free, GuiChromeRasterCfg, .shadow = &shadow);
    cli_check(shadow.cover == NULL, "the shadow's cell is given back");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &shaded);

    // the lattice lies on --deep, as the page's canvas does: most of its pixels are that color
    unsigned int on_deep = 0u;
    for (int x = 0; x < 200; x += 1)
    {
        on_deep += (ground_at(&window, x, 250) == window.palette->deep) ? 1u : 0u;
    }
    cli_check(on_deep > 100u, "the lattice's dots lie on --deep");

    // the name: the eye's place the caption's until it is home; while the pane stands the view is not drawn, the lattice
    // showing through the work with the name over it, and it comes back as the pane goes
    const GuiRect eye = EMBED_CALL(gui_wordmark_frame.eye_square, GuiWordmarkFrameCfg, .frame = &window.frame);
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(eye.x + 5, eye.y + 5)) == GUI_HIT_CAPTION, "the eye's place is the caption's until it is home");
    EMBED_CALL(cli_wordmark_still.land, GuiWordmarkStillCfg, .wordmark = &window.wordmark);
    cli_check(EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(eye.x + 5, eye.y + 5)) == GUI_HIT_CLIENT, "the eye home is the client's");
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(painted_on_pane(&window, eye), "the eye home is drawn over the bar");
    window.wordmark.home = 0;
    EMBED_CALL(gui_wordmark_frame.stand, GuiWordmarkFrameCfg, .wordmark = &window.wordmark);
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    const GuiRect editor = window.frame.editor;
    GuiWordmarkPlace place;
    EMBED_CALL(gui_wordmark_glyphs.place, GuiWordmarkGlyphsCfg, .glyphs = &window.glyphs, .frame = &window.frame, .place = &place);
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
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
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
        cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &direct, .width = window.raster->width, .height = window.raster->height), "a raster to hold a frame is made");
        const unsigned long long pixels = (unsigned long long)window.raster->width * window.raster->height;
        const unsigned long long claim_on = 500000ull;
        const unsigned long long bar_in = claim_on + 2000ull;
        const unsigned long long moments[8] = {claim_on + 40ull, claim_on + 150ull, claim_on + 290ull, bar_in + 40ull, bar_in + 150ull, bar_in + 280ull, bar_in + 400ull, bar_in + 560ull};
        g_test_clock_ms = claim_on;
        EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &window.claim, .on = 1);
        int same_frames = 1;
        int stage_read = 1;
        for (unsigned int at = 0u; at < 8u; at += 1u)
        {
            if (at == 3u)
            {
                g_test_clock_ms = bar_in - GUI_CLAIM_SHOW_DELAY_MS;
                EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &window.claim, .x = 400, .y = 4, .zone = GUI_CLAIM_TOP, .over_parts = 0);
                g_test_clock_ms = bar_in;
                (void)EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &window.claim);
            }
            GuiClaim numbers_from = window.claim;
            EMBED_CALL(gui_window_claim.claim_layout, GuiWindowClaimCfg, .window = &window, .time_ms = moments[at]);
            stage_read = stage_read && (window.stage != NULL);
            EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
            memor.cpy(&(MemoriaCfg){.dst = direct.pixels, .src = window.raster->pixels, .bytes = pixels * sizeof(GuiColor)});
            // the same moment laid by hand, no stage standing; the slats' shares as the stage has them
            const GuiStage *const was = window.stage;
            EMBED_CALL(gui_stages_table.stage_claim_take, GuiStagesCfg, .stage = was, .claim = &numbers_from);
            GuiClaimNumbers numbers;
            GuiFrame laid = window.frame;
            const unsigned long long stage_moment = window.stages.start_ms + (((moments[at] - window.stages.start_ms) / GUI_STAGE_MS) * GUI_STAGE_MS);
            EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &numbers_from, .time_ms = stage_moment, .numbers = &numbers);
            numbers.grown_top = was->frame.claim.grown_top;
            numbers.grown_left = was->frame.claim.grown_left;
            numbers.grown_bottom = was->frame.claim.grown_bottom;
            EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &laid, .numbers = &numbers);
            const GuiFrame saved_frame = window.frame;
            const GuiStage *const saved_stage = window.stage;
            window.stage = NULL;
            window.frame = laid;
            EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
            same_frames = same_frames && (memor.cmp(&(MemoriaCfg){.src = direct.pixels, .other = window.raster->pixels, .bytes = pixels * sizeof(GuiColor)}) == 0);
            window.frame = saved_frame;
            window.stage = saved_stage;
        }
        cli_check(stage_read, "each frame of the movement stands on a stage");
        cli_check(same_frames, "and draws the pixels the numbers lay by hand");
        EMBED_CALL(gui_window_claim.claim_layout, GuiWindowClaimCfg, .window = &window, .time_ms = bar_in + 900ull);
        cli_check((window.stage == NULL) && (window.stages.stage == NULL), "at rest no stage stands and the cell is given back");
        EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &direct);
        EMBED_CALL(cli_claim_still.init, GuiClaimStillCfg, .claim = &window.claim);
        EMBED_CALL(gui_window_claim.claim_layout, GuiWindowClaimCfg, .window = &window, .time_ms = bar_in + 1000ull);
    }

    // less motion asked for: the system's wish or View's Reduce Motion stills the lattice, the slides, the name and the caret
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = &window, .reduced = 1);
    cli_check(window.motion_still && (window.ground.paused_ms != 0ull) && window.wordmark.still && window.claim.still,
              "the system's wish stills the lattice, the claim and the name");
    const int had_cursor = window.editor.has_cursor;
    window.editor.has_cursor = 1;
    window.caret_blink_start_ms = cli_clock_milliseconds() - (GUI_CARET_BLINK_PERIOD_MS / 2ull);
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(window.caret_ink == 255u, "the caret stands at full ink");
    cli_check(EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &window) == 0ull, "and nothing wants the clock");
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = &window, .reduced = 0);
    window.caret_blink_start_ms = cli_clock_milliseconds() - (GUI_CARET_BLINK_PERIOD_MS / 2ull);
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &window);
    cli_check(!window.motion_still && (window.ground.paused_ms == 0ull) && (window.caret_ink < 255u), "no longer asked for, they move again");
    window.editor.has_cursor = had_cursor;
    EMBED_CALL(cli_window_shown.show, GuiWindowShownCfg, .window = &window, .shown = 0);
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = &window, .reduced = 1);
    EMBED_CALL(cli_window_shown.show, GuiWindowShownCfg, .window = &window, .shown = 1);
    cli_check(window.ground.paused_ms != 0ull, "a window back on the screen stays still while less motion is asked for");
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = &window, .reduced = 0);
    cli_check(window.ground.paused_ms == 0ull, "and moves as it no longer is");
    EMBED_CALL(cli_window_shown.show, GuiWindowShownCfg, .window = &window, .shown = 0);
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = &window, .reduced = 1);
    EMBED_CALL(gui_window_reduced.motion_tell, GuiWindowReducedCfg, .window = &window, .reduced = 0);
    cli_check(window.ground.paused_ms != 0ull, "a window off the screen stays still as the wish goes");
    EMBED_CALL(cli_window_shown.show, GuiWindowShownCfg, .window = &window, .shown = 1);
    const GuiChoice reduce = {"reduce-motion", ""};
    EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = &window, .choice = &reduce);
    cli_check(window.motion_saved && window.motion_still, "View's Reduce Motion asks for it");
    EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = &window, .choice = &reduce);
    cli_check(!window.motion_saved && !window.motion_still, "and chosen again no longer does");

    // File, Preferences: a window of its own, which the program opens and closes as `shown` says. The main window saves its keys, its
    // claim and its pointer: nothing of it waits on Preferences. Zoom lays both windows at its scale, the opacities and the colors
    // change the palette both draw in, Reduce Motion is the window's own, and Escape or the command again closes it
    const GuiChoice preferences_chosen = {"preferences", ""};
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &window.claim, .on = 1);
    EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = &window, .choice = &preferences_chosen);
    cli_check(window.preferences.shown && window.claim.claimed, "the command opens Preferences and the claim stands");
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &window.claim, .on = 0);
    EMBED_CALL(gui_window_width.preferences_resize, GuiWindowWidthCfg, .window = &window, .width = 856u, .height = 1008u, .system_scale = 100);
    cli_check((window.preferences_raster.width == 856u) && (window.preferences_raster.height == 1008u) && window.preferences_owed, "it has a raster of its own at its window's size");
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Down"));
    cli_check(window.preferences.marked == GUI_PREFERENCE_ZOOM, "a key in the main window is not Preferences'");
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = &window, .input = GUI_TEXT_INPUT("#", 1ull));
    cli_check(!window.preferences.entering && (window.preferences.entered_length == 0u), "and text typed in the main window is not");
    const GuiRect in_place = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &window.preferences, .row = GUI_PREFERENCE_ZOOM);
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = &window, .input = GUI_DOWN_INPUT(in_place.x + 4, in_place.y + 4, 0u));
    cli_check(window.preference_values.zoom_step == GUI_PREFERENCE_ZOOM_WHOLE, "and a press in the main window is not");
    cli_check(EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(400, 5)) == GUI_HIT_CAPTION, "its head drags its window");
    cli_check(EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(0, 500)) == GUI_HIT_LEFT, "its left edge sizes its window");
    cli_check(EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(855, 1007)) == GUI_HIT_BOTTOM_RIGHT, "and its corner");
    const GuiRect close_place = EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg, .preferences_read = &window.preferences);
    cli_check(EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(close_place.x + 4, close_place.y + (close_place.height / 2))) == GUI_HIT_CLIENT, "its close mark takes a press");
    cli_check(EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg, .window_read = &window, .input = GUI_MOVED_INPUT(in_place.x + 4, in_place.y + 4)) == GUI_HIT_CLIENT, "and its rows");
    const int scale_before = window.frame.scale;
    const unsigned int wide_before = window.frame.width;
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Right"));
    cli_check(window.preference_values.zoom_step == GUI_PREFERENCE_ZOOM_WHOLE + 1u, "Zoom moves on");
    cli_check(window.frame.scale == 110, "the frame is laid at its scale");
    cli_check(window.preferences.scale == 110, "and Preferences at its");
    cli_check(window.frame.width == wide_before, "the window's size standing");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Left"));
    cli_check((window.preference_values.zoom_step == GUI_PREFERENCE_ZOOM_WHOLE) && (window.frame.scale == scale_before) && (window.preferences.scale == 100), "and moved back the scale it stood at");
    cli_check(EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = &window, .width = wide_before, .height = window.frame.height, .system_scale = scale_before) && (window.frame.scale == scale_before), "the system's scale is the zoom's whole");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Down"));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Right"));
    cli_check((window.preference_values.bar_percent == 40u) && (GUI_ALPHA_OF(window.palette->glass) == 102u), "the bar's opacity moved on lays the ink's tint");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Left"));
    cli_check((window.preference_values.bar_percent == 30u) && (GUI_ALPHA_OF(window.palette->glass) == 77u), "and back");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Down"));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Left"));
    cli_check((window.preference_values.editor_percent == 70u) && (GUI_ALPHA_OF(window.palette->editor_ground) == 179u), "the editor's opacity moved lays its ground");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Right"));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Down"));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Right"));
    cli_check(window.motion_saved && window.motion_still, "Reduce Motion moved asks for less motion");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Right"));
    cli_check(!window.motion_saved && !window.motion_still, "and moved again no longer does");
    window.preferences_owed = 1;
    EMBED_CALL(gui_window_table.preferences_draw, GuiWindowCfg, .window = &window);
    cli_check(!window.preferences_owed, "its frame paid");
    const GuiRect row_drawn = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &window.preferences, .row = GUI_PREFERENCE_REDUCE);
    int row_painted = 0;
    for (int across_row = row_drawn.x; across_row < row_drawn.x + 160; across_row += 1)
    {
        for (int down_row = row_drawn.y; down_row < row_drawn.y + row_drawn.height; down_row += 1)
        {
            // the place lies on the raster
            row_painted = row_painted || (window.preferences_raster.pixels[((unsigned int)down_row * window.preferences_raster.width) + (unsigned int)across_row] != window.palette->surface);
        }
    }
    cli_check(row_painted, "its raster draws its rows");
    // the colors: one typed changes what both windows draw in
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("PageDown"));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Enter"));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_TEXT_INPUT("#010203", 7ull));
    window.preferences_owed = 0;
    window.owed = 0;
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Enter"));
    cli_check(window.palette->ground == GUI_RGB(1u, 2u, 3u), "a color typed changes the palette");
    cli_check(window.owed && window.preferences_owed, "and owes both windows a frame");
    cli_check(!window.preferences.entering, "and the typing ends");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Enter"));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_TEXT_INPUT("#131331", 7ull));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Enter"));
    cli_check(window.palette->ground == GUI_RGB(0x13u, 0x13u, 0x31u), "and typed back it stands as it stood");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("PageUp"));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Escape"));
    cli_check(!window.preferences.shown, "Escape closes it");
    EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = &window, .choice = &preferences_chosen);
    cli_check(window.preferences.shown, "the command opens it again");
    // a tab shown or a file opened in the main window leaves it open
    EMBED_CALL(gui_window_at.tab_show, GuiWindowAtCfg, .window = &window, .at = 0u);
    cli_check(window.preferences.shown, "a tab shown leaves it open");
    // a press on a row moves it; a press on the close mark closes it; the command in its own window closes it
    const GuiRect zoom_row = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &window.preferences, .row = GUI_PREFERENCE_ZOOM);
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_MOVED_INPUT(zoom_row.x + 4, zoom_row.y + 4));
    cli_check(window.preferences.marked == GUI_PREFERENCE_ZOOM, "the pointer marks a row");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_DOWN_INPUT(zoom_row.x + 4, zoom_row.y + 4, 0u));
    cli_check(window.preference_values.zoom_step == GUI_PREFERENCE_ZOOM_WHOLE + 1u, "a press on a row moves it on");
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_KEY_INPUT("Left"));
    EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = &window, .input = GUI_DOWN_INPUT(close_place.x + 4, close_place.y + 4, 0u));
    cli_check(!window.preferences.shown, "a press on the close mark closes it");
    EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = &window, .choice = &preferences_chosen);
    EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = &window, .choice = &preferences_chosen);
    cli_check(!window.preferences.shown, "and the command again closes it");

    cli_check(EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = &window, .width = 640u, .height = 400u, .system_scale = 100), "the window sized again remakes its raster");
    cli_check((window.raster->width == 640u) && (window.frame.bar.width == 640), "the parts follow the new size");

    EMBED_CALL(gui_window_table.end, GuiWindowCfg, .window = &window);
    cli_check(window.raster->pixels == NULL, "the window ended holds no raster");
    return cli_on_check();
}
