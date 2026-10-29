// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_bench.c: the frame bench, run by `harness.py window bench`: the window begun without a system, sized to a
// monitor at the scale of one with the explorer's side shown and a text of many lines in the editor, its frame drawn
// many times and timed; and a blend of the whole raster at half alpha, timed alone, the one blend every drawing
// goes through
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
    static char text[BENCH_LINES * 80u];
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
    const GuiRect whole = {0, 0, (int)BENCH_WIDTH, (int)BENCH_HEIGHT};
    const unsigned long long fills_began = cli_clock_milliseconds();
    for (unsigned int fill = 0u; fill < BENCH_FILLS; fill += 1u)
    {
        gui_raster_filled(&window.raster, whole, GUI_RGBA(0x80u, 0x40u, 0xc0u, 0x80u));
    }
    const unsigned long long fills_took = cli_clock_milliseconds() - fills_began;
    printf("frame: %.2f ms over %u frames at %u by %u\n", (double)frames_took / (double)BENCH_FRAMES, BENCH_FRAMES, BENCH_WIDTH, BENCH_HEIGHT);
    printf("blend of the whole raster: %.2f ms over %u fills\n", (double)fills_took / (double)BENCH_FILLS, BENCH_FILLS);
    gui_window_ended(&window);
    return 0;
}
