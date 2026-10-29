// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_test.c: the frame's check: the parts laid out for a window's size stand where the page stands them, the bar
// across the top, the status bar across the foot, the strip down the left, the frame's gap down the right, the work
// between them with the tabs row at its head and the editor filling the rest, each scaled with the system's scale
#include "../../../../../ui/gui/src/frame.h"
#include "../cli/cli_check.h"

static int same(GuiRect rect, int x, int y, int width, int height)
{
    return (rect.x == x) && (rect.y == y) && (rect.width == width) && (rect.height == height);
}

int main(void)
{
    GuiFrame frame;
    gui_frame_laid(&frame, 1440u, 920u, 1.0f);
    cli_check(same(frame.bar, 0, 0, 1440, 41), "the bar runs across the top, forty-one pixels tall");
    cli_check(same(frame.status, 0, 920 - 27, 1440, 27), "the status bar runs across the foot, twenty-seven tall");
    cli_check(same(frame.strip, 0, 41, 44, 920 - 41 - 27), "the strip runs down the left between them, forty-four wide");
    cli_check(same(frame.gap, 1440 - 12, 41, 12, 920 - 41 - 27), "the frame's gap runs down the right, twelve wide");
    cli_check(same(frame.work, 44, 41, 1440 - 44 - 12, 920 - 41 - 27), "the work fills what the four leave");
    cli_check(same(frame.tabs, 44 + 1, 41 + 2, 1440 - 44 - 12 - 3, 31), "the tabs row stands at the work's head inside its padding");
    cli_check(same(frame.editor, 44 + 1, 41 + 2 + 31, 1440 - 44 - 12 - 3, 920 - 41 - 27 - 3 - 31),
              "the editor fills the work under the tabs row");
    cli_check(frame.controls.width == (3 * 34) + (2 * 2), "the three controls stand at the bar's right end");
    cli_check((frame.controls.x + frame.controls.width == 1440 - 5) && (frame.controls.y == 0) &&
                  (frame.controls.height == 41),
              "the controls end five pixels from the bar's right end, the bar's height tall");
    cli_check(frame.radius == 7u, "the work's corners are rounded by seven");

    gui_frame_laid(&frame, 2880u, 1840u, 2.0f);
    cli_check(same(frame.bar, 0, 0, 2880, 82), "at twice the scale the bar is twice as tall");
    cli_check(same(frame.status, 0, 1840 - 54, 2880, 54), "at twice the scale the status bar is twice as tall");
    cli_check(frame.strip.width == 88, "at twice the scale the strip is twice as wide");
    cli_check(frame.radius == 14u, "at twice the scale the corners are rounded by fourteen");
    cli_check(frame.controls.width == (3 * 68) + (2 * 4), "at twice the scale the controls are twice as wide");

    gui_frame_laid(&frame, 100u, 50u, 1.0f);
    cli_check(!gui_rect_empty(frame.bar) && !gui_rect_empty(frame.status), "a window too small keeps its bars");
    cli_check(gui_rect_empty(frame.editor), "a window too small has an editor of none");
    return cli_checked();
}
