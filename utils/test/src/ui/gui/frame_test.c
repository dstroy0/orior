// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_test.c: the frame's check: the parts laid out for a window's size stand where the page stands them, the bar
// across the top, the status bar across the foot, the strip down the left, the frame's gap down the right, the work
// between them with the tabs row at its head, the map's column at its right and the editor filling the rest, each
// scaled with the system's scale
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
    cli_check(same(frame.strip, 0, 41, 44, 920 - 41 - 27),
              "the strip runs down the left between them, forty-four wide");
    cli_check(same(frame.gap, 1440 - 12, 41, 12, 920 - 41 - 27), "the frame's gap runs down the right, twelve wide");
    cli_check(same(frame.work, 44, 41, 1440 - 44 - 12, 920 - 41 - 27), "the work fills what the four leave");
    cli_check(same(frame.tabs, 44 + 1, 41 + 2, 1440 - 44 - 12 - 3, 31),
              "the tabs row stands at the work's head inside its padding");
    cli_check(same(frame.editor, 44 + 1, 41 + 2 + 31, 1440 - 44 - 12 - 3 - 84, 920 - 41 - 27 - 3 - 31),
              "the editor fills the work under the tabs row, less the map's column");
    cli_check(same(frame.map, 1440 - 12 - 2 - 84, 41 + 2 + 31, 84, 920 - 41 - 27 - 3 - 31),
              "the map's column stands at the editor's right, eighty-four wide, the editor's height");
    cli_check(frame.controls.width == (3 * 34) + (2 * 2), "the three controls stand at the bar's right end");
    cli_check((frame.controls.x + frame.controls.width == 1440 - 5) && (frame.controls.y == 0) &&
                  (frame.controls.height == 41),
              "the controls end five pixels from the bar's right end, the bar's height tall");
    cli_check(frame.radius == 7u, "the work's corners are rounded by seven");
    cli_check(same(frame.inside, 44 + 1, 41 + 2, 1440 - 44 - 12 - 3, 920 - 41 - 27 - 3),
              "the work's inside is the work less its padding");
    cli_check(gui_rect_empty(frame.side) && (frame.side.x == 44 + 1), "no side stands in the work until one is shown");

    gui_frame_sided(&frame, 1);
    cli_check(same(frame.side, 44 + 1, 41 + 2, 304, 920 - 41 - 27 - 3),
              "the side shown stands at the inside's left, three hundred and four wide");
    cli_check(same(frame.tabs, 44 + 1 + 304, 41 + 2, 1440 - 44 - 12 - 3 - 304, 31), "the tabs row moves past the side");
    cli_check((frame.editor.x == 44 + 1 + 304) && (frame.editor.width == 1440 - 44 - 12 - 3 - 304 - 84) &&
                  (frame.editor.y == 41 + 2 + 31),
              "the editor moves past the side");
    cli_check((frame.map.x == 1440 - 12 - 2 - 84) && (frame.map.width == 84), "the map's column stays at the right");
    gui_frame_sided(&frame, 0);
    cli_check(gui_rect_empty(frame.side) && same(frame.tabs, 44 + 1, 41 + 2, 1440 - 44 - 12 - 3, 31),
              "the side taken away gives the tabs row its room back");

    gui_frame_laid(&frame, 2880u, 1840u, 2.0f);
    cli_check(same(frame.bar, 0, 0, 2880, 82), "at twice the scale the bar is twice as tall");
    cli_check(same(frame.status, 0, 1840 - 54, 2880, 54), "at twice the scale the status bar is twice as tall");
    cli_check(frame.strip.width == 88, "at twice the scale the strip is twice as wide");
    cli_check(frame.radius == 14u, "at twice the scale the corners are rounded by fourteen");
    cli_check(frame.controls.width == (3 * 68) + (2 * 4), "at twice the scale the controls are twice as wide");

    gui_frame_laid(&frame, 100u, 50u, 1.0f);
    cli_check(!gui_rect_empty(frame.bar) && !gui_rect_empty(frame.status), "a window too small keeps its bars");
    cli_check(gui_rect_empty(frame.editor), "a window too small has an editor of none");
    cli_check(gui_rect_empty(frame.map), "a window too small has a map of none");

    // the claim: the work takes the window less the gap, the bar, the strip and the status bar off it, the tabs row
    // and the map away
    gui_frame_laid(&frame, 1440u, 920u, 1.0f);
    GuiClaimNumbers numbers = frame.claim;
    cli_check(gui_frame_holes(&frame, NULL) == 0u, "no claim makes no holes in the work");
    cli_check(same(frame.bar_slat, 0, 0, 1440, 41) && same(frame.strip_slat, 0, 41, 44, 920 - 41 - 27) &&
                  same(frame.status_slat, 0, 920 - 27, 1440, 27),
              "outside the claim each slat is its part whole");
    cli_check((frame.bar_edges.top == 0) && (frame.bar_edges.bottom == 41 * GUI_WHOLE) && (frame.bar_edges.left == 0) &&
                  (frame.bar_edges.right == 1440 * GUI_WHOLE) && (frame.status_edges.top == (920 - 27) * GUI_WHOLE) &&
                  (frame.strip_edges.right == 44 * GUI_WHOLE),
              "at rest each pane's edges are its rectangle's in the frame's words");
    numbers.claim = GUI_WHOLE;
    gui_frame_claimed(&frame, &numbers);
    cli_check(same(frame.work, 12, 12, 1440 - 24, 920 - 24),
              "the claim standing, the work is the window less the gap on every side");
    cli_check(frame.bar.y + frame.bar.height == 0, "the bar stands off the window's top");
    cli_check(frame.strip.x + frame.strip.width == 0, "the strip stands off the window's left");
    cli_check(frame.status.y == 920, "the status bar stands off the window's foot");
    cli_check((frame.tabs.height == 0) && (frame.map.width == 0), "the tabs row and the map are away");
    cli_check(frame.controls.y + frame.controls.height == 0, "the controls stand off with the bar");
    cli_check(frame.pane_radius == 7u, "the panes are rounded as the work is");
    cli_check(gui_frame_holes(&frame, (GuiRect[4]){{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}) == 0u,
              "the parts away make no holes");
    numbers.claim = GUI_WHOLE / 2;
    gui_frame_claimed(&frame, &numbers);
    cli_check((frame.work.x == 28) && (frame.work.y == 26) && (frame.bar.y == -21) && (frame.status.y == 920 - 27 + 14),
              "halfway through the claim's slide every edge stands halfway");
    cli_check((frame.bar_edges.top == -(41 * GUI_WHOLE) / 2) &&
                  (frame.bar_edges.bottom == frame.bar_edges.top + (41 * GUI_WHOLE)),
              "the bar's edges carry the half pixel its rectangle rounds away");
    // the head revealed: the bar comes in as a pane the gap down from the top, past the wordmark's corner; the work
    // stays until the room is taken, then moves down under the bar
    numbers.claim = GUI_WHOLE;
    numbers.reveal_top = GUI_WHOLE;
    numbers.reveal_down = GUI_WHOLE;
    numbers.grown_top = GUI_WHOLE / 2;
    numbers.pulled_top = 700;
    gui_frame_claimed(&frame, &numbers);
    cli_check(same(frame.bar, 44 + 24, 12, 1440 - 44 - 24 - 12, 41),
              "the bar revealed stands the gap down and in, past the wordmark's corner");
    cli_check(frame.work.y == 12, "the work stands where it stood until the bar is given its room");
    cli_check((frame.bar_slat.x == 700 - 344) && (frame.bar_slat.x + frame.bar_slat.width == 700 + 364) &&
                  (frame.bar_slat.y == 12),
              "the bar's slat is grown half way from the point pulled, from the work's edge to the bar's end");
    cli_check(same(frame.controls_pane, frame.controls.x - 8, 12, frame.controls.width + 8, 41) &&
                  (frame.controls_pane_share == 0),
              "the controls stand in the bar, their own pane unseen");
    GuiRect holes[4];
    cli_check((gui_frame_holes(&frame, holes) == 1u) && (holes[0].y == 12) && (holes[0].x == 356 - 12) &&
                  (holes[0].y + holes[0].height == 12 + 41 + 12),
              "the bar's slat makes one hole in the work's head, the gap about it");
    numbers.room_top = GUI_WHOLE;
    numbers.grown_top = GUI_WHOLE;
    gui_frame_claimed(&frame, &numbers);
    cli_check((frame.work.y == 12 + 41 + 12) && (frame.tabs.height == 31),
              "the room taken, the work stands under the bar and the tabs row is back");
    cli_check(same(frame.bar_slat, 12, 12, 1440 - 24, 41), "the bar's slat grown whole spans from the work's edge");
    // the controls alone: their tab comes down the gap from the top with the bar away
    numbers.reveal_top = 0;
    numbers.room_top = 0;
    gui_frame_claimed(&frame, &numbers);
    cli_check((frame.controls.y == 12) && (frame.bar.y == -41) && (frame.controls_pane_share == GUI_WHOLE),
              "the controls' tab stands the gap down while the bar is away");
    cli_check((gui_frame_holes(&frame, holes) == 1u) && (holes[0].x + holes[0].width == 1440 - 12) &&
                  (holes[0].y == 12),
              "the controls' tab makes a hole at the work's head, snapped to its right edge");
    // the left revealed: the strip comes in under the wordmark's corner
    numbers.reveal_down = 0;
    numbers.reveal_left = GUI_WHOLE;
    numbers.room_left = GUI_WHOLE;
    gui_frame_claimed(&frame, &numbers);
    cli_check((frame.strip.x == 12) && (frame.strip.y == 12 + 41 + 12) &&
                  (frame.strip.y + frame.strip.height == 920 - 12),
              "the strip revealed stands the gap in, under the wordmark's corner, to the work's foot");
    cli_check(frame.work.x == 12 + 44 + 12, "the left's room taken, the work stands past the strip");
    // the foot revealed
    numbers.reveal_left = 0;
    numbers.room_left = 0;
    numbers.reveal_bottom = GUI_WHOLE;
    gui_frame_claimed(&frame, &numbers);
    cli_check(same(frame.status, 12, 920 - 27 - 12, 1440 - 24, 27) && (frame.work.y + frame.work.height == 920 - 12),
              "the status bar revealed stands the gap up and in, the work staying until its room is taken");
    numbers.room_bottom = GUI_WHOLE;
    gui_frame_claimed(&frame, &numbers);
    cli_check(frame.work.y + frame.work.height == 920 - 27 - 24,
              "the foot's room taken, the work ends above the status bar");
    // the right revealed brings the map back
    numbers.reveal_right = GUI_WHOLE;
    gui_frame_claimed(&frame, &numbers);
    cli_check(frame.map.width == 84, "the right revealed brings the map's column back");
    return cli_checked();
}
