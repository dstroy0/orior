// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_bench.c: the frame bench, run by `harness.py window bench`: the window begun without a system, sized to a
// monitor at the scale of one with the explorer's side shown and a text of many lines in the editor, its frame drawn
// many times and timed, then drawn again with no text, the text's own cost the difference; each part of the frame
// drawn alone and timed, the base through the map and the map laid, the ground's copy and the editor's ground each alone, the editor's text, the explorer, the tabs, the map,
// the panes' contents and corners and the work's outline; and a blend of the whole raster at half alpha, timed alone, the one blend
// every drawing goes through
#include "../../../../../ui/gui/src/parts/chrome.h"
#include "../../../../../ui/gui/src/parts/glass.h"
#include "../../../../../ui/gui/src/window.h"

#include <stdio.h>

#define BENCH_WIDTH 1878u
#define BENCH_HEIGHT 1392u
#define BENCH_FRAMES 100u
#define BENCH_FILLS 50u
#define BENCH_LINES 120u

static void woken(void *context)
{
    (void)context;
}

// a part's drawing, run `BENCH_FRAMES` times on `window` and its milliseconds a run printed under `name`
typedef void (*BenchPart)(GuiWindow *window);

static void bench_timed(const char *name, BenchPart part, GuiWindow *window)
{
    const unsigned long long began = cli_clock_milliseconds();
    for (unsigned int run = 0u; run < BENCH_FRAMES; run += 1u)
    {
        part(window);
    }
    const unsigned long long took = cli_clock_milliseconds() - began;
    printf("  %s: %.2f ms\n", name, (double)took / (double)BENCH_FRAMES);
}

static void part_frame(GuiWindow *window)
{
    gui_window_drawn(window);
}

// the table the window's panes lie on
static GuiUnder bench_table(const GuiWindow *window)
{
    return gui_glass_table(&window->ground.raster, window->palette->ground, window->palette->glass);
}

static void part_base(GuiWindow *window)
{
    gui_base_drawn(&window->base, &window->raster, &window->ground.raster, window->palette->ground);
}

static void part_map_laid(GuiWindow *window)
{
    window->base.laid = 0;
    (void)gui_base_laid(&window->base, &window->frame, window->palette, 0);
}

static void part_ground_copied(GuiWindow *window)
{
    memor.cpy(&(MemoriaCfg){.dst = window->raster.pixels,
                            .src = window->ground.raster.pixels,
                            .bytes = (unsigned long long)window->raster.width * window->raster.height * sizeof(GuiColor)});
}

static void part_editor_ground(GuiWindow *window)
{
    gui_raster_filled(&window->raster, window->frame.editor, window->palette->editor_ground);
}

static void part_pane_corners(GuiWindow *window)
{
    const GuiUnder table = bench_table(window);
    const GuiColor pane = gui_glass_pane_tint(window->palette->glass);
    const GuiFrame *const frame = &window->frame;
    gui_raster_paned(&window->raster, table, gui_rect_met(frame->bar_slat, frame->bar), frame->pane_radius, pane, 1);
    gui_raster_paned(&window->raster, table, gui_rect_met(frame->strip_slat, frame->strip), frame->pane_radius, pane, 1);
    gui_raster_paned(&window->raster, table, gui_rect_met(frame->status_slat, frame->status), frame->pane_radius, pane, 1);
}

static void part_outline(GuiWindow *window)
{
    const GuiPalette *const palette = window->palette;
    const GuiFrame *const frame = &window->frame;
    const GuiColor right_line = GUI_RGB((GUI_RED_OF(palette->link) * 65u) / 100u, (GUI_GREEN_OF(palette->link) * 65u) / 100u,
                                        (GUI_BLUE_OF(palette->link) * 65u) / 100u);
    const GuiOutline lines = {palette->signal, right_line, right_line, (palette->signal & 0x00ffffffu) | (0x59u << 24u), 2, 2, 1, 1};
    const GuiUnder clear = {&window->ground.raster, 0u, NULL, 0u, palette->ground};
    GuiRect holes[4];
    const unsigned int count = gui_frame_holes(frame, holes);
    gui_raster_outlined(&window->raster, frame->work, frame->inside, frame->radius, &lines, holes, count, bench_table(window), clear);
}

static void part_editor(GuiWindow *window)
{
    gui_editor_drawn(&window->editor, &window->raster, &window->glyphs, window->palette, window->frame.editor, window->frame.scale);
}

static void part_explorer(GuiWindow *window)
{
    gui_explorer_drawn(&window->explorer, &window->raster, &window->glyphs, &window->icons, window->palette, &window->frame,
                       gui_strip_tool(0u)->label, 1, window->title, window->path);
}

static void part_tabs(GuiWindow *window)
{
    gui_tabs_drawn(&window->raster, &window->glyphs, window->palette, &window->frame, window->tab, window->tabs, window->active);
}

static void part_map(GuiWindow *window)
{
    gui_minimap_drawn(&window->minimap, &window->raster, window->palette, &window->editor, window->frame.map, window->frame.scale, 60.0f);
}

static void part_glass(GuiWindow *window)
{
    gui_strip_drawn(&window->raster, &window->icons, window->palette, &window->frame, &window->strip);
    gui_bar_drawn(&window->raster, &window->glyphs, window->palette, &window->frame, window->title, bench_table(window));
    const GuiStatus status = {"ui/gui/src/raster.c", 1, 3ull, 4ull, 3ull, 4ull, 0, 0, 4u, 100000000ull};
    gui_statusbar_drawn(&window->raster, &window->glyphs, &window->icons, window->palette, &window->frame, &status);
}

static void part_strip(GuiWindow *window)
{
    gui_strip_drawn(&window->raster, &window->icons, window->palette, &window->frame, &window->strip);
}

static void part_bar(GuiWindow *window)
{
    gui_bar_drawn(&window->raster, &window->glyphs, window->palette, &window->frame, window->title, bench_table(window));
}

static void part_status(GuiWindow *window)
{
    const GuiStatus status = {"ui/gui/src/raster.c", 1, 3ull, 4ull, 3ull, 4ull, 0, 0, 4u, 100000000ull};
    gui_statusbar_drawn(&window->raster, &window->glyphs, &window->icons, window->palette, &window->frame, &status);
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    static GuiWindow window;
    if (!gui_window_begun(&window, woken, NULL, &why))
    {
        mmgr_line_finish(&why);
        printf("the window did not begin: %s\n", why_text);
        return 1;
    }
    window.strip.shown = 0;
    if (!gui_window_sized(&window, BENCH_WIDTH, BENCH_HEIGHT, 1.0f))
    {
        printf("the window was not sized\n");
        return 1;
    }
    static char text[BENCH_LINES * 128u];
    VerbaLinea lines = {text, sizeof(text), 0ull};
    for (unsigned int line = 0u; line < BENCH_LINES; line += 1u)
    {
        mmgr_line_put(&lines, "    const unsigned int value = (color * share) + (ground * rest) + 127u; // a line of code\n");
    }
    (void)gui_window_text_put(&window, text, mmgr_line_ok(&lines) ? lines.at : 0ull);
    gui_window_drawn(&window);
    const unsigned long long frames_began = cli_clock_milliseconds();
    for (unsigned int frame = 0u; frame < BENCH_FRAMES; frame += 1u)
    {
        gui_window_drawn(&window);
    }
    const unsigned long long frames_took = cli_clock_milliseconds() - frames_began;
    printf("frame: %.2f ms over %u frames at %u by %u\n", (double)frames_took / (double)BENCH_FRAMES, BENCH_FRAMES, BENCH_WIDTH, BENCH_HEIGHT);
    printf("the frame's parts, each drawn alone:\n");
    bench_timed("the base through the map", part_base, &window);
    bench_timed("the map laid", part_map_laid, &window);
    bench_timed("  the ground copied alone", part_ground_copied, &window);
    bench_timed("  the editor's ground alone", part_editor_ground, &window);
    bench_timed("the editor's text", part_editor, &window);
    bench_timed("the explorer", part_explorer, &window);
    bench_timed("the tabs", part_tabs, &window);
    bench_timed("the map", part_map, &window);
    bench_timed("the panes' contents", part_glass, &window);
    bench_timed("  the strip", part_strip, &window);
    bench_timed("  the bar", part_bar, &window);
    bench_timed("  the status bar", part_status, &window);
    bench_timed("the panes' corners", part_pane_corners, &window);
    bench_timed("the work's outline", part_outline, &window);
    // the same frame with the editor holding no lines: the text's own cost is the difference
    gui_editor_given(&window.editor);
    gui_editor_made(&window.editor);
    const unsigned long long bare_began = cli_clock_milliseconds();
    for (unsigned int frame = 0u; frame < BENCH_FRAMES; frame += 1u)
    {
        part_frame(&window);
    }
    const unsigned long long bare_took = cli_clock_milliseconds() - bare_began;
    printf("frame with no text: %.2f ms, the text %.2f ms of the frame\n", (double)bare_took / (double)BENCH_FRAMES,
           ((double)frames_took - (double)bare_took) / (double)BENCH_FRAMES);
    const GuiRect whole = {0, 0, (int)BENCH_WIDTH, (int)BENCH_HEIGHT};
    const unsigned long long fills_began = cli_clock_milliseconds();
    for (unsigned int fill = 0u; fill < BENCH_FILLS; fill += 1u)
    {
        gui_raster_filled(&window.raster, whole, GUI_RGBA(0x80u, 0x40u, 0xc0u, 0x80u));
    }
    const unsigned long long fills_took = cli_clock_milliseconds() - fills_began;
    printf("blend of the whole raster: %.2f ms over %u fills\n", (double)fills_took / (double)BENCH_FILLS, BENCH_FILLS);
    gui_window_ended(&window);
    return 0;
}
