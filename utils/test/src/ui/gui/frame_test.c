// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_test.c: the frame's check: the parts laid out for a window's size stand where the page stands them, the bar
// across the top, the status bar across the foot, the strip down the left, the frame's gap down the right, the work
// between them with the tabs row at its head, the map's column at its right and the editor filling the rest, each
// scaled with the system's scale; and the preview taking the editor's right half beside the text or its whole
// rectangle alone
#include "../../../../../ui/cli/src/view/frame.h"
#include "../cli/cli_check.h"

static int same(GuiRect rect, int x, int y, int width, int height)
{
    return (rect.x == x) && (rect.y == y) && (rect.width == width) && (rect.height == height);
}

int main(void)
{
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
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
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.side) && (frame.side.x == 44 + 1), "no side stands in the work until one is shown");

    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);
    cli_check(same(frame.side, 44 + 1, 41 + 2, 304, 920 - 41 - 27 - 3),
              "the side shown stands at the inside's left, three hundred and four wide");
    cli_check(same(frame.tabs, 44 + 1 + 304, 41 + 2, 1440 - 44 - 12 - 3 - 304, 31), "the tabs row moves past the side");
    cli_check((frame.editor.x == 44 + 1 + 304) && (frame.editor.width == 1440 - 44 - 12 - 3 - 304 - 84) &&
                  (frame.editor.y == 41 + 2 + 31),
              "the editor moves past the side");
    cli_check((frame.map.x == 1440 - 12 - 2 - 84) && (frame.map.width == 84), "the map's column stays at the right");
    cli_check(same(frame.side_pane, frame.side.x, frame.side.y, 304, frame.side.height) &&
                  (frame.side_share == GUI_FIXED_ONE),
              "the pane standing is drawn on the side, whole");
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1, .away = GUI_FIXED_ONE / 2);
    cli_check(same(frame.side_pane, frame.side.x - 152, frame.side.y - ((frame.side.height + 1) / 2), 304, frame.side.height) &&
                  (frame.side_share == GUI_FIXED_ONE / 2) && (frame.side.width == 304) && (frame.editor.x == 44 + 1 + 304),
              "halfway gone, the pane stands up and to the left by half its size at half its opacity, its width still the side's");
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 0);
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.side) && same(frame.tabs, 44 + 1, 41 + 2, 1440 - 44 - 12 - 3, 31),
              "the side taken away gives the tabs row its capacity back");

    // the preview: none while off, the editor's right half beside the text, the editor's whole rectangle alone
    const GuiRect whole = frame.editor;
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.preview),
              "no preview stands while it is off");
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame, .displayed = GUI_FRAME_PREVIEW_BESIDE);
    cli_check(same(frame.editor, whole.x, whole.y, whole.width - (whole.width / 2), whole.height) &&
                  same(frame.preview, whole.x + whole.width - (whole.width / 2), whole.y, whole.width / 2, whole.height),
              "beside the text the preview takes the editor's right half and the editor keeps the left");
    cli_check(same(frame.map, 1440 - 12 - 2 - 84, 41 + 2 + 31, 84, whole.height), "the map's column stays at the right");
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);
    cli_check((frame.preview.x + frame.preview.width == whole.x + whole.width) && (frame.editor.x == 44 + 1 + 304) &&
                  (frame.preview.x == frame.editor.x + frame.editor.width),
              "a side shown lays the preview again beside the narrower editor");
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 0);
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame, .displayed = GUI_FRAME_PREVIEW_ALONE);
    cli_check(same(frame.preview, whole.x, whole.y, whole.width, whole.height) && (frame.editor.width == 0) &&
                  (frame.editor.height == whole.height),
              "alone the preview fills the editor's rectangle and the editor is left no width");
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame, .displayed = GUI_FRAME_PREVIEW_OFF);
    cli_check(same(frame.editor, whole.x, whole.y, whole.width, whole.height) &&
                  EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.preview),
              "off again, the editor has its rectangle back");

    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = 240);
    cli_check(same(frame.terminal_drawn, frame.terminal.x, frame.terminal.y, frame.terminal.width, frame.terminal.height) &&
                  same(frame.terminal_views_drawn, frame.terminal_views.x, frame.terminal_views.y, frame.terminal_views.width, frame.terminal_views.height) &&
                  (frame.terminal_share == GUI_FIXED_ONE) && (frame.terminal.height == 240),
              "the terminal's panel open is drawn at its place, whole");
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = 240, .away = GUI_FIXED_ONE / 2);
    cli_check(same(frame.terminal_drawn, frame.terminal.x, frame.terminal.y + 7, frame.terminal.width, frame.terminal.height) &&
                  (frame.terminal_views_drawn.y == frame.terminal_views.y + 7) && (frame.terminal_share == GUI_FIXED_ONE / 2) &&
                  (frame.editor.height == frame.inside.height - 31 - 240),
              "halfway through its arrival the panel is drawn seven pixels below its place at half its opacity, the editor laid for its place");
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = 0);
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.terminal_drawn),
              "the panel closed is drawn nowhere");

    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 2880u, .height = 1840u, .scale = 200);
    cli_check(same(frame.bar, 0, 0, 2880, 82), "at twice the scale the bar is twice as tall");
    cli_check(same(frame.status, 0, 1840 - 54, 2880, 54), "at twice the scale the status bar is twice as tall");
    cli_check(frame.strip.width == 88, "at twice the scale the strip is twice as wide");
    cli_check(frame.radius == 14u, "at twice the scale the corners are rounded by fourteen");
    cli_check(frame.controls.width == (3 * 68) + (2 * 4), "at twice the scale the controls are twice as wide");

    // a scale is a percent, and a scaled size rounds to the nearest pixel
    cli_check(EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 41, .scale = 100) == 41, "at 100 percent a size is unchanged");
    cli_check(EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 10, .scale = 110) == 11, "at 110 percent a size is a tenth larger");
    cli_check(EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 41, .scale = 125) == 51, "at 125 percent a size rounds down to the nearest pixel");
    cli_check(EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 41, .scale = 150) == 62, "at 150 percent a size rounds a half up");
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 2160u, .height = 1380u, .scale = 150);
    cli_check((frame.scale == 150) && (frame.bar.height == 62), "a frame at 150 percent lays its bar at 150 percent");

    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 100u, .height = 50u, .scale = 100);
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.bar) && !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.status), "a window too small keeps its bars");
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.editor), "a window too small has an editor of none");
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.map), "a window too small has a map of none");

    // the claim: the work takes the window less the gap, the bar, the strip and the status bar off it, the tabs row
    // and the map away
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    GuiAutohideNumbers numbers = frame.claim;
    cli_check(EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = &frame, .holes = NULL) == 0u, "no claim makes no holes in the work");
    cli_check(same(frame.bar_band, 0, 0, 1440, 41) && same(frame.strip_band, 0, 41, 44, 920 - 41 - 27) &&
                  same(frame.status_band, 0, 920 - 27, 1440, 27),
              "outside the claim each slat is its part whole");
    cli_check((frame.bar_edges.top == 0) && (frame.bar_edges.bottom == 41 * GUI_FIXED_ONE) && (frame.bar_edges.left == 0) &&
                  (frame.bar_edges.right == 1440 * GUI_FIXED_ONE) && (frame.status_edges.top == (920 - 27) * GUI_FIXED_ONE) &&
                  (frame.strip_edges.right == 44 * GUI_FIXED_ONE),
              "at rest each pane's edges are its rectangle's in the frame's words");
    cli_check(same(frame.tabs_row, frame.tabs.x, frame.tabs.y, frame.tabs.width, 31) &&
                  (frame.tabs_share == GUI_FIXED_ONE),
              "outside the claim the tabs row stands whole where it is shown, at its full opacity");
    numbers.claim = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(same(frame.work, 12, 12, 1440 - 24, 920 - 24),
              "the claim standing, the work is the window less the gap on every side");
    cli_check(frame.bar.y + frame.bar.height == 0, "the bar stands off the window's top");
    cli_check(frame.strip.x + frame.strip.width == 0, "the strip stands off the window's left");
    cli_check(frame.status.y == 920, "the status bar stands off the window's foot");
    cli_check((frame.tabs.height == 0) && (frame.map.width == 0), "the tabs row and the map are away");
    cli_check((frame.tabs_row.height == 31) && (frame.tabs_row.y + 31 == frame.tabs.y) && (frame.tabs_share == 0),
              "the tabs row stands whole, risen out of the work's head, at no opacity");
    cli_check(frame.controls.y + frame.controls.height == 0, "the controls stand off with the bar");
    cli_check(frame.pane_radius == 7u, "the panes are rounded as the work is");
    cli_check(EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = &frame, .holes = (GuiRect[4]){{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}) == 0u,
              "the parts away make no holes");
    numbers.claim = GUI_FIXED_ONE / 2;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.work.x == 28) && (frame.work.y == 26) && (frame.bar.y == -21) && (frame.status.y == 920 - 27 + 14),
              "halfway through the claim's slide every edge stands halfway");
    cli_check((frame.tabs_row.height == 31) && (frame.tabs_row.y + 31 == frame.tabs.y + frame.tabs.height) &&
                  (frame.tabs.height == 16) && (frame.tabs_share == GUI_FIXED_ONE / 2),
              "halfway, the tabs row stands whole, risen by half out of the work's head, at half its opacity");
    cli_check((frame.map.width == 42) && same(frame.map_whole, frame.map.x, frame.map.y, 84, frame.map.height),
              "halfway, the map's column is half closed and the map stands whole from its left, past the editor's edge");
    cli_check((frame.bar_edges.top == -(41 * GUI_FIXED_ONE) / 2) &&
                  (frame.bar_edges.bottom == frame.bar_edges.top + (41 * GUI_FIXED_ONE)),
              "the bar's edges carry the half pixel its rectangle rounds away");
    // three tenths through the claim: the tabs row's share at the base's sixteenth nearest it, the row's height at the
    // claim's own share
    numbers.claim = (GUI_FIXED_ONE * 3) / 10;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.tabs_share == (11 * GUI_FIXED_ONE) / 16) && (frame.tabs.height == 22),
              "the row's share stands at the sixteenth nearest the claim's, its height at the claim's own share");
    // the head revealed: the bar comes in as a pane the gap down from the top, past the wordmark's corner; the work
    // stays until the capacity is taken, then moves down under the bar
    numbers.claim = GUI_FIXED_ONE;
    numbers.reveal_top = GUI_FIXED_ONE;
    numbers.reveal_down = GUI_FIXED_ONE;
    numbers.grown_top = GUI_FIXED_ONE / 2;
    numbers.pulled_top = 700;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(same(frame.bar, 44 + 24, 12, 1440 - 44 - 24 - 12, 41),
              "the bar revealed stands the gap down and in, past the wordmark's corner");
    cli_check(frame.work.y == 12, "the work stands where it stood until the bar is given its capacity");
    cli_check((frame.bar_band.x == 700 - 344) && (frame.bar_band.x + frame.bar_band.width == 700 + 364) &&
                  (frame.bar_band.y == 12),
              "the bar's slat is grown half way from the point pulled, from the work's edge to the bar's end");
    cli_check(same(frame.controls_pane, frame.controls.x - 8, 12, frame.controls.width + 8, 41) &&
                  (frame.controls_pane_share == 0),
              "the controls stand in the bar, their own pane unseen");
    GuiRect holes[4];
    cli_check((EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = &frame, .holes = holes) == 1u) && (holes[0].y == 12) && (holes[0].x == 356 - 12) &&
                  (holes[0].y + holes[0].height == 12 + 41 + 12),
              "the bar's slat makes one hole in the work's head, the gap about it");
    numbers.inset_top = GUI_FIXED_ONE;
    numbers.grown_top = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.work.y == 12 + 41 + 12) && (frame.tabs.height == 31),
              "the capacity taken, the work stands under the bar and the tabs row is back");
    cli_check(same(frame.bar_band, 12, 12, 1440 - 24, 41), "the bar's slat grown whole spans from the work's edge");
    // the controls alone: their tab comes down the gap from the top with the bar away
    numbers.reveal_top = 0;
    numbers.inset_top = 0;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.controls.y == 12) && (frame.bar.y == -41) && (frame.controls_pane_share == GUI_FIXED_ONE),
              "the controls' tab stands the gap down while the bar is away");
    cli_check((EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = &frame, .holes = holes) == 1u) && (holes[0].x + holes[0].width == 1440 - 12) &&
                  (holes[0].y == 12),
              "the controls' tab makes a hole at the work's head, snapped to its right edge");
    // the left revealed: the strip comes in under the wordmark's corner
    numbers.reveal_down = 0;
    numbers.reveal_left = GUI_FIXED_ONE;
    numbers.inset_left = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.strip.x == 12) && (frame.strip.y == 12 + 41 + 12) &&
                  (frame.strip.y + frame.strip.height == 920 - 12),
              "the strip revealed stands the gap in, under the wordmark's corner, to the work's foot");
    cli_check(frame.work.x == 12 + 44 + 12, "the left's capacity taken, the work stands past the strip");
    // the foot revealed
    numbers.reveal_left = 0;
    numbers.inset_left = 0;
    numbers.reveal_bottom = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(same(frame.status, 12, 920 - 27 - 12, 1440 - 24, 27) && (frame.work.y + frame.work.height == 920 - 12),
              "the status bar revealed stands the gap up and in, the work staying until its capacity is taken");
    numbers.inset_bottom = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(frame.work.y + frame.work.height == 920 - 27 - 24,
              "the foot's capacity taken, the work ends above the status bar");
    // the right revealed brings the map back
    numbers.reveal_right = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(frame.map.width == 84, "the right revealed brings the map's column back");
    return cli_on_check();
}
