// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_test.c: the frame's check: the parts arranged for a window's size are where the page is them, the bar
// across the top, the status bar across the foot, the side_menu down the left, the frame's gap down the right, the work
// between them with the tabs row at its head, the map's column at its right and the editor filling the rest, each
// scaled with the system's scale; and the preview taking the editor's right half beside the text or its whole
// rectangle alone
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/frame.h"
#include "../../../../../includes/window/scrollbar.h"
#include "../cli/cli_check.h"

static int same(GuiRect rect, int x, int y, int width, int height)
{
    return (rect.x == x) && (rect.y == y) && (rect.width == width) && (rect.height == height);
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    cli_check(same(frame.top_menu, 0, 0, 1440, GUI_TOP_MENU_HEIGHT), "the bar runs across the top, its height");
    cli_check(same(frame.status, 0, 920 - GUI_STATUS_HEIGHT, 1440, GUI_STATUS_HEIGHT),
              "the status bar runs across the foot, twenty-two tall");
    cli_check(same(frame.side_menu, 0, GUI_TOP_MENU_HEIGHT, GUI_SIDE_MENU_WIDTH, 920 - GUI_TOP_MENU_HEIGHT - GUI_STATUS_HEIGHT),
              "the side_menu runs down the left between them, thirty-six wide");
    cli_check(
        same(frame.gap, 1440 - GUI_FRAME_GAP, GUI_TOP_MENU_HEIGHT, GUI_FRAME_GAP, 920 - GUI_TOP_MENU_HEIGHT - GUI_STATUS_HEIGHT),
        "the frame's gap runs down the right, eight wide");
    cli_check(same(frame.work, GUI_SIDE_MENU_WIDTH, GUI_TOP_MENU_HEIGHT, 1440 - GUI_SIDE_MENU_WIDTH - GUI_FRAME_GAP,
                   920 - GUI_TOP_MENU_HEIGHT - GUI_STATUS_HEIGHT),
              "the work fills what the four leave");
    cli_check(same(frame.tabs, GUI_SIDE_MENU_WIDTH + 1, GUI_TOP_MENU_HEIGHT + 2 + GUI_TABS_GAP, 1440 - GUI_SIDE_MENU_WIDTH - GUI_FRAME_GAP - 3, 31),
              "the tabs row is at the work's head inside its padding, a gap under the border line");
    cli_check(same(frame.editor, GUI_SIDE_MENU_WIDTH + 1, GUI_TOP_MENU_HEIGHT + 2 + GUI_TABS_GAP + 31,
                   1440 - GUI_SIDE_MENU_WIDTH - GUI_FRAME_GAP - 3 - 84 - 10 - GUI_SCROLLBAR_STANDOFF,
                   920 - GUI_TOP_MENU_HEIGHT - GUI_STATUS_HEIGHT - 3 - GUI_TABS_GAP - 31 - 10),
              "the editor fills the work under the tabs row, less the map's column, its two scrollbars and the standoff");
    cli_check(same(frame.editor_bar, frame.editor.x + frame.editor.width, frame.editor.y + 10, 10, frame.editor.height - 10),
              "the editor's scrollbar down is at its right, ten wide, its thickness down from the text's head and as far short of its foot");
    cli_check(
        same(frame.editor_bar_across, frame.editor.x, frame.editor.y + frame.editor.height, frame.editor.width, 10),
        "the editor's scrollbar across is at its foot, ten tall, the text's width");
    cli_check(same(frame.map, 1440 - GUI_FRAME_GAP - 2 - 84, GUI_TOP_MENU_HEIGHT + 2 + GUI_TABS_GAP + 31, 84,
                   920 - GUI_TOP_MENU_HEIGHT - GUI_STATUS_HEIGHT - 3 - GUI_TABS_GAP - 31),
              "the map's column is at the editor's right, eighty-four wide, the editor's height");
    cli_check(frame.controls.width == (3 * 30) + (2 * 2), "the three controls are at the bar's right end");
    cli_check((frame.controls.x + frame.controls.width == 1440 - GUI_CONTROLS_END) && (frame.controls.y == 0) &&
                  (frame.controls.height == GUI_TOP_MENU_HEIGHT) && (GUI_CONTROLS_END == GUI_FRAME_GAP),
              "the controls end the frame's gap from the bar's right end, on the work's right edge, the bar's height tall");
    cli_check(same(frame.top_menu_tools, frame.controls.x - 11 - ((4 * 30) + (3 * 2)), (GUI_TOP_MENU_HEIGHT - 30) / 2,
                   (4 * 30) + (3 * 2), 30),
              "the bar's four tools are before the controls, a gap off them, centred on the bar's row");
    cli_check(frame.radius == 7u, "the work's corners are rounded by seven");
    cli_check(same(frame.inside, GUI_SIDE_MENU_WIDTH + 1, GUI_TOP_MENU_HEIGHT + 2, 1440 - GUI_SIDE_MENU_WIDTH - GUI_FRAME_GAP - 3,
                   920 - GUI_TOP_MENU_HEIGHT - GUI_STATUS_HEIGHT - 3),
              "the work's inside is the work less its padding");
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.side) &&
                  (frame.side.x == GUI_SIDE_MENU_WIDTH + 1),
              "no side is in the work until one is displayed");

    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);
    cli_check(
        same(frame.side, GUI_SIDE_MENU_WIDTH + 1, GUI_TOP_MENU_HEIGHT + 2, 304, 920 - GUI_TOP_MENU_HEIGHT - GUI_STATUS_HEIGHT - 3),
        "the side displayed is at the inside's left, three hundred and four wide");
    cli_check(same(frame.tabs, GUI_SIDE_MENU_WIDTH + 1 + 304, GUI_TOP_MENU_HEIGHT + 2 + GUI_TABS_GAP,
                   1440 - GUI_SIDE_MENU_WIDTH - GUI_FRAME_GAP - 3 - 304, 31),
              "the tabs row moves past the side");
    cli_check((frame.editor.x == GUI_SIDE_MENU_WIDTH + 1 + 304) &&
                  (frame.editor.width == 1440 - GUI_SIDE_MENU_WIDTH - GUI_FRAME_GAP - 3 - 304 - 84 - 10 - GUI_SCROLLBAR_STANDOFF) &&
                  (frame.editor.y == GUI_TOP_MENU_HEIGHT + 2 + GUI_TABS_GAP + 31),
              "the editor moves past the side");
    cli_check((frame.map.x == 1440 - GUI_FRAME_GAP - 2 - 84) && (frame.map.width == 84),
              "the map's column stays at the right");
    cli_check(same(frame.side_pane, frame.side.x, frame.side.y, 304, frame.side.height) &&
                  (frame.side_share == GUI_FIXED_ONE),
              "the pane located is drawn on the side, whole");
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1,
               .away = GUI_FIXED_ONE / 2);
    cli_check(same(frame.side_pane, frame.side.x - 152, frame.side.y - ((frame.side.height + 1) / 2), 304,
                   frame.side.height) &&
                  (frame.side_share == GUI_FIXED_ONE / 2) && (frame.side.width == 304) &&
                  (frame.editor.x == GUI_SIDE_MENU_WIDTH + 1 + 304),
              "halfway gone, the pane is up and to the left by half its size at half its opacity, its width still "
              "the side's");
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 0);
    cli_check(
        EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.side) &&
            same(frame.tabs, GUI_SIDE_MENU_WIDTH + 1, GUI_TOP_MENU_HEIGHT + 2 + GUI_TABS_GAP, 1440 - GUI_SIDE_MENU_WIDTH - GUI_FRAME_GAP - 3, 31),
        "the side taken away gives the tabs row its capacity back");

    // The preview: none while off, the right half of the editor's area beside the text, the whole area alone. The area
    // is the editor's text, its two scrollbars and the scrollbar's standoff together
    const GuiRect whole = {frame.editor.x, frame.editor.y,
                           frame.editor.width + frame.editor_bar.width + GUI_SCROLLBAR_STANDOFF,
                           frame.editor.height + frame.editor_bar_across.height};
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.preview),
              "no preview is while it is off");
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame,
               .displayed = GUI_FRAME_PREVIEW_BESIDE);
    cli_check(
        same(frame.editor, whole.x, whole.y, whole.width - (whole.width / 2) - 10 - GUI_SCROLLBAR_STANDOFF,
             whole.height - 10) &&
            same(frame.preview, whole.x + whole.width - (whole.width / 2), whole.y, whole.width / 2, whole.height),
        "beside the text the preview takes the area's right half and the editor retains the left with its scrollbars");
    cli_check(same(frame.map, 1440 - GUI_FRAME_GAP - 2 - 84, GUI_TOP_MENU_HEIGHT + 2 + GUI_TABS_GAP + 31, 84, whole.height),
              "the map's column stays at the right");
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);
    cli_check((frame.preview.x + frame.preview.width == whole.x + whole.width) &&
                  (frame.editor.x == GUI_SIDE_MENU_WIDTH + 1 + 304) &&
                  (frame.preview.x ==
                   frame.editor.x + frame.editor.width + frame.editor_bar.width + GUI_SCROLLBAR_STANDOFF),
              "a side displayed positions the preview again beside the narrower editor and its scrollbar");
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 0);
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame,
               .displayed = GUI_FRAME_PREVIEW_ALONE);
    cli_check(same(frame.preview, whole.x, whole.y, whole.width, whole.height) && (frame.editor.width == 0) &&
                  (frame.editor.height == whole.height) &&
                  EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.editor_bar),
              "alone the preview fills the editor's area and the editor is left no width and no scrollbars");
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame, .displayed = GUI_FRAME_PREVIEW_OFF);
    cli_check(same(frame.editor, whole.x, whole.y, whole.width - 10 - GUI_SCROLLBAR_STANDOFF, whole.height - 10) &&
                  EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.preview),
              "off again, the editor has its rectangle back");

    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = 240);
    cli_check(
        same(frame.terminal_drawn, frame.terminal.x, frame.terminal.y, frame.terminal.width, frame.terminal.height) &&
            same(frame.terminal_views_drawn, frame.terminal_views.x, frame.terminal_views.y, frame.terminal_views.width,
                 frame.terminal_views.height) &&
            (frame.terminal_share == GUI_FIXED_ONE) && (frame.terminal.height == 240),
        "the terminal's panel open is drawn at its place, whole");
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = 240,
               .away = GUI_FIXED_ONE / 2);
    cli_check(same(frame.terminal_drawn, frame.terminal.x, frame.terminal.y + 7, frame.terminal.width,
                   frame.terminal.height) &&
                  (frame.terminal_views_drawn.y == frame.terminal_views.y + 7) &&
                  (frame.terminal_share == GUI_FIXED_ONE / 2) &&
                  (frame.editor.height == frame.inside.height - GUI_TABS_GAP - 31 - 240 - 10),
              "halfway through its arrival the panel is drawn seven pixels below its place at half its opacity, the "
              "editor positioned for its place");
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = 0);
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.terminal_drawn),
              "the panel closed is drawn nowhere");

    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 2880u, .height = 1840u,
               .scale = 200);
    cli_check(same(frame.top_menu, 0, 0, 2880, 2 * GUI_TOP_MENU_HEIGHT), "at twice the scale the bar is twice as tall");
    cli_check(same(frame.status, 0, 1840 - (2 * GUI_STATUS_HEIGHT), 2880, 2 * GUI_STATUS_HEIGHT),
              "at twice the scale the status bar is twice as tall");
    cli_check(frame.side_menu.width == 2 * GUI_SIDE_MENU_WIDTH, "at twice the scale the side_menu is twice as wide");
    cli_check(frame.radius == 14u, "at twice the scale the corners are rounded by fourteen");
    cli_check(frame.controls.width == (3 * 60) + (2 * 4), "at twice the scale the controls are twice as wide");

    // a scale is a percent, and a scaled size rounds to the nearest pixel
    cli_check(EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 41, .scale = 100) == 41,
              "at 100 percent a size is unchanged");
    cli_check(EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 10, .scale = 110) == 11,
              "at 110 percent a size is a tenth larger");
    cli_check(EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 41, .scale = 125) == 51,
              "at 125 percent a size rounds down to the nearest pixel");
    cli_check(EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 41, .scale = 150) == 62,
              "at 150 percent a size rounds a half up");
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 2160u, .height = 1380u,
               .scale = 150);
    cli_check((frame.scale == 150) && (frame.top_menu.height == (GUI_TOP_MENU_HEIGHT * 3) / 2),
              "a frame at 150 percent positions its bar at 150 percent");

    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 100u, .height = 50u, .scale = 100);
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.top_menu) &&
                  !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.status),
              "a window too small retains its bars");
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.editor),
              "a window too small has an editor of none");
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.map),
              "a window too small has a map of none");

    // the claim: the work takes the window less the gap, the bar, the side_menu and the status bar off it, the tabs row
    // and the map away
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    GuiAutohideNumbers numbers = frame.claim;
    cli_check(EMBED_CALL(gui_frame_holes_table.holes, GuiFrameHolesCfg, .frame_read = &frame, .holes = NULL) == 0u,
              "no claim makes no holes in the work");
    cli_check(
        same(frame.top_menu_band, 0, 0, 1440, GUI_TOP_MENU_HEIGHT) &&
            same(frame.side_menu_band, 0, GUI_TOP_MENU_HEIGHT, GUI_SIDE_MENU_WIDTH, 920 - GUI_TOP_MENU_HEIGHT - GUI_STATUS_HEIGHT) &&
            same(frame.status_band, 0, 920 - GUI_STATUS_HEIGHT, 1440, GUI_STATUS_HEIGHT),
        "outside the claim each slat is its part whole");
    cli_check((frame.bar_edges.top == 0) && (frame.bar_edges.bottom == GUI_TOP_MENU_HEIGHT * GUI_FIXED_ONE) &&
                  (frame.bar_edges.left == 0) && (frame.bar_edges.right == 1440 * GUI_FIXED_ONE) &&
                  (frame.status_edges.top == (920 - GUI_STATUS_HEIGHT) * GUI_FIXED_ONE) &&
                  (frame.side_menu_edges.right == GUI_SIDE_MENU_WIDTH * GUI_FIXED_ONE),
              "at rest each pane's edges are its rectangle's in the frame's words");
    cli_check(same(frame.tabs_row, frame.tabs.x, frame.tabs.y, frame.tabs.width, 31) &&
                  (frame.tabs_share == GUI_FIXED_ONE),
              "outside the claim the tabs row is whole where it is displayed, at its full opacity");
    numbers.claim = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(same(frame.work, GUI_FRAME_GAP, GUI_FRAME_GAP, 1440 - (2 * GUI_FRAME_GAP), 920 - (2 * GUI_FRAME_GAP)),
              "the claim located, the work is the window less the gap on every side");
    cli_check(frame.top_menu.y + frame.top_menu.height == 0, "the bar is off the window's top");
    cli_check(frame.side_menu.x + frame.side_menu.width == 0, "the side_menu is off the window's left");
    cli_check(frame.status.y == 920, "the status bar is off the window's foot");
    cli_check((frame.tabs.height == 0) && (frame.map.width == 0), "the tabs row and the map are away");
    cli_check((frame.tabs_row.height == 31) && (frame.tabs_row.y + 31 == frame.tabs.y) && (frame.tabs_share == 0),
              "the tabs row is whole, risen out of the work's head, at no opacity");
    cli_check(frame.controls.y + frame.controls.height == 0, "the controls are off with the bar");
    cli_check(frame.pane_radius == 7u, "the panes are rounded as the work is");
    cli_check(EMBED_CALL(gui_frame_holes_table.holes, GuiFrameHolesCfg, .frame_read = &frame,
                         .holes = (GuiRect[4]){{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}) == 0u,
              "the parts away make no holes");
    numbers.claim = GUI_FIXED_ONE / 2;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    // a half of an odd count of pixels rounds up, as the frame's shares round
    cli_check((frame.work.x == GUI_SIDE_MENU_WIDTH + GUI_FRAME_GAP - ((GUI_SIDE_MENU_WIDTH + GUI_FRAME_GAP + 1) / 2)) &&
                  (frame.work.y == (GUI_TOP_MENU_HEIGHT + GUI_FRAME_GAP + 1) / 2) &&
                  (frame.top_menu.y == -((GUI_TOP_MENU_HEIGHT + 1) / 2)) &&
                  (frame.status.y == 920 - GUI_STATUS_HEIGHT + ((GUI_STATUS_HEIGHT + 1) / 2)),
              "halfway through the claim's slide every edge is halfway");
    cli_check((frame.tabs_row.height == 31) && (frame.tabs_row.y + 31 == frame.tabs.y + frame.tabs.height) &&
                  (frame.tabs.height == 16) && (frame.tabs_share == GUI_FIXED_ONE / 2),
              "halfway, the tabs row is whole, risen by half out of the work's head, at half its opacity");
    cli_check(
        (frame.map.width == 42) && same(frame.map_whole, frame.map.x, frame.map.y, 84, frame.map.height),
        "halfway, the map's column is half closed and the map is whole from its left, past the editor's edge");
    cli_check((frame.bar_edges.top == -(GUI_TOP_MENU_HEIGHT * GUI_FIXED_ONE) / 2) &&
                  (frame.bar_edges.bottom == frame.bar_edges.top + (GUI_TOP_MENU_HEIGHT * GUI_FIXED_ONE)),
              "the bar's edges carry the half pixel its rectangle rounds away");
    // three tenths through the claim: the tabs row's share at the base's sixteenth nearest it, the row's height at the
    // claim's own share
    numbers.claim = (GUI_FIXED_ONE * 3) / 10;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.tabs_share == (11 * GUI_FIXED_ONE) / 16) && (frame.tabs.height == 22),
              "the row's share is at the sixteenth nearest the claim's, its height at the claim's own share");
    // the head revealed: the bar comes in as a pane the gap down from the top, past the wordmark's corner; the work
    // stays until the capacity is taken, then moves down under the bar
    numbers.claim = GUI_FIXED_ONE;
    numbers.reveal_top = GUI_FIXED_ONE;
    numbers.reveal_down = GUI_FIXED_ONE;
    numbers.grown_top = GUI_FIXED_ONE / 2;
    numbers.pulled_top = 700;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(same(frame.top_menu, GUI_SIDE_MENU_WIDTH + (2 * GUI_FRAME_GAP), GUI_FRAME_GAP,
                   1440 - GUI_SIDE_MENU_WIDTH - (2 * GUI_FRAME_GAP) - GUI_FRAME_GAP, GUI_TOP_MENU_HEIGHT),
              "the bar revealed is the gap down and in, past the wordmark's corner");
    cli_check(frame.work.y == GUI_FRAME_GAP, "the work is where it was until the bar is given its capacity");
    cli_check((frame.top_menu_band.x == 700 - ((700 - GUI_FRAME_GAP) / 2)) &&
                  (frame.top_menu_band.x + frame.top_menu_band.width == 700 + ((1440 - GUI_FRAME_GAP - 700) / 2)) &&
                  (frame.top_menu_band.y == GUI_FRAME_GAP),
              "the bar's slat is grown half way from the point pulled, from the work's edge to the bar's end");
    cli_check(
        same(frame.controls_pane, frame.controls.x - 8, GUI_FRAME_GAP, frame.controls.width + 8, GUI_TOP_MENU_HEIGHT) &&
            (frame.controls_pane_share == 0),
        "the controls are in the bar, their own pane unseen");
    GuiRect holes[4];
    cli_check((EMBED_CALL(gui_frame_holes_table.holes, GuiFrameHolesCfg, .frame_read = &frame, .holes = holes) == 1u) &&
                  (holes[0].y == GUI_FRAME_GAP) && (holes[0].x == frame.top_menu_band.x - GUI_FRAME_GAP) &&
                  (holes[0].y + holes[0].height == GUI_FRAME_GAP + GUI_TOP_MENU_HEIGHT + GUI_FRAME_GAP),
              "the bar's slat makes one hole in the work's head, the gap about it");
    numbers.inset_top = GUI_FIXED_ONE;
    numbers.grown_top = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.work.y == GUI_FRAME_GAP + GUI_TOP_MENU_HEIGHT + GUI_FRAME_GAP) && (frame.tabs.height == 0),
              "the capacity taken, the work is under the bar and the tabs row, a claim of its own, stays away");
    numbers.reveal_tabs = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.tabs.height == 31) && (frame.tabs.y == frame.inside.y + GUI_TABS_GAP),
              "the tabs row's own reveal brings it back, a gap under the border line");
    cli_check(same(frame.top_menu_band, GUI_FRAME_GAP, GUI_FRAME_GAP, 1440 - (2 * GUI_FRAME_GAP), GUI_TOP_MENU_HEIGHT),
              "the bar's slat grown whole spans from the work's edge");
    // the controls alone: their tab comes down the gap from the top with the bar away
    numbers.reveal_top = 0;
    numbers.inset_top = 0;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.controls.y == GUI_FRAME_GAP) && (frame.top_menu.y == -GUI_TOP_MENU_HEIGHT) &&
                  (frame.controls_pane_share == GUI_FIXED_ONE),
              "the controls' tab is the gap down while the bar is away");
    cli_check((EMBED_CALL(gui_frame_holes_table.holes, GuiFrameHolesCfg, .frame_read = &frame, .holes = holes) == 1u) &&
                  (holes[0].x + holes[0].width == 1440 - GUI_FRAME_GAP) && (holes[0].y == GUI_FRAME_GAP),
              "the controls' tab makes a hole at the work's head, snapped to its right edge");
    // the left revealed: the side_menu comes in under the wordmark's corner
    numbers.reveal_down = 0;
    numbers.reveal_left = GUI_FIXED_ONE;
    numbers.inset_left = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check((frame.side_menu.x == GUI_FRAME_GAP) && (frame.side_menu.y == GUI_FRAME_GAP + GUI_TOP_MENU_HEIGHT + GUI_FRAME_GAP) &&
                  (frame.side_menu.y + frame.side_menu.height == 920 - GUI_FRAME_GAP),
              "the side_menu revealed is the gap in, under the wordmark's corner, to the work's foot");
    cli_check(frame.work.x == GUI_FRAME_GAP + GUI_SIDE_MENU_WIDTH + GUI_FRAME_GAP,
              "the left's capacity taken, the work is past the side_menu");
    cli_check((frame.side_menu_rest.x == 0) && (frame.side_menu_rest.y == GUI_TOP_MENU_HEIGHT) &&
                  (frame.top_menu_rest.y == 0) && (frame.top_menu_rest.x == 0) &&
                  (frame.controls_rest.y == 0) && (frame.top_menu_tools_rest.x < frame.controls_rest.x),
              "the side_menu, the bar, its tools and the controls at rest are where no claim positions them");
    // the foot revealed
    numbers.reveal_left = 0;
    numbers.inset_left = 0;
    numbers.reveal_bottom = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(same(frame.status, GUI_FRAME_GAP, 920 - GUI_STATUS_HEIGHT - GUI_FRAME_GAP, 1440 - (2 * GUI_FRAME_GAP),
                   GUI_STATUS_HEIGHT) &&
                  (frame.work.y + frame.work.height == 920 - GUI_FRAME_GAP),
              "the status bar revealed is the gap up and in, the work staying until its capacity is taken");
    numbers.inset_bottom = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(frame.work.y + frame.work.height == 920 - GUI_STATUS_HEIGHT - (2 * GUI_FRAME_GAP),
              "the foot's capacity taken, the work ends above the status bar");
    // the right revealed brings the map back
    numbers.reveal_right = GUI_FIXED_ONE;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    cli_check(frame.map.width == 84, "the right revealed brings the map's column back");
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
