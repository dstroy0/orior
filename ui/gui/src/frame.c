// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.c: the parts laid out for the window's size and scale, and again for the claim's numbers, each part's place
// the page's own arithmetic over them: the bar's and the status bar's margins, the frame's padding and the strip's
// margins as style.css sets them from the claim, the reveals and the rooms; a part a window too small leaves no room
// for is a rectangle of none
#include "frame.h"

#include "../../cli/src/command_line/cli_internal.h"

int gui_scaled(int pixels, float scale)
{
    // a size in pixels, scaled and rounded, is far inside what an int represents
    return (int)(((float)pixels * scale) + 0.5f);
}

// `share`, a number of GUI_WHOLE, of `pixels`, rounded
static int frame_share(int share, int pixels)
{
    // a share of a size in pixels is within a long long and, rounded, far inside what an int represents
    const long long scaled = (long long)share * (long long)pixels;
    return (int)((scaled + ((scaled >= 0ll) ? (GUI_WHOLE / 2) : -(GUI_WHOLE / 2))) / GUI_WHOLE);
}

// `share`, a number of GUI_WHOLE, of `pixels`, a count of pixels in the frame's words, kept in those words
static int frame_part(int share, int pixels)
{
    // a share of a size in the frame's words is within a long long and comes back to the words
    return (int)(((long long)share * (long long)pixels) / GUI_WHOLE);
}

// a slat along an edge in the frame's words: the part's span from `first` to `end` along the edge, grown by `grown`
// from the point `pulled`, the middle where none has been, into `from` and `to`
static void frame_slat_edges(int first, int end, int grown, int pulled, int *from, int *to)
{
    const int middle = (first + end) / 2;
    const int wanted = (pulled < 0) ? middle : pulled;
    const int at = (wanted < first) ? first : ((wanted > end) ? end : wanted);
    *from = at - frame_part(grown, at - first);
    *to = at + frame_part(grown, end - at);
}

// the pane's edges met with the span `left` to `right` across and `top` to `bottom` down, each in the frame's words
static GuiEdges frame_edges_met(GuiEdges edges, int left, int top, int right, int bottom)
{
    const GuiEdges met = {(edges.left > left) ? edges.left : left, (edges.top > top) ? edges.top : top,
                          (edges.right < right) ? edges.right : right, (edges.bottom < bottom) ? edges.bottom : bottom};
    return met;
}

// a slat along an edge: the part's span from `first` to `end` along the edge, grown by `grown` from the point
// `pulled`, the middle where none has been, into `from` and `to`
static void frame_slat(int first, int end, int grown, int pulled, int *from, int *to)
{
    const int middle = (first + end) / 2;
    const int wanted = (pulled < 0) ? middle : pulled;
    const int at = (wanted < first) ? first : ((wanted > end) ? end : wanted);
    *from = at - frame_share(grown, at - first);
    *to = at + frame_share(grown, end - at);
}

// the work's inside laid out: the inside the work less its padding, the side `side_wide` pixels of it at the left,
// the tabs row across the rest, and under it the editor with the map's column at its right, the row and the column
// away by the claim's numbers
static void frame_inside_laid(GuiFrame *frame, int side_wide)
{
    const float scale = frame->scale;
    const GuiClaimNumbers *const claim = &frame->claim;
    const int pad_top = gui_scaled(GUI_WORK_PAD_TOP, scale);
    const int pad_right = gui_scaled(GUI_WORK_PAD_RIGHT, scale);
    const int pad_bottom = gui_scaled(GUI_WORK_PAD_BOTTOM, scale);
    const int pad_left = gui_scaled(GUI_WORK_PAD_LEFT, scale);
    // the tabs row is away while the claim holds and the head's room is not taken; the map while the right is not
    // revealed
    const int tabs_tall = frame_share(GUI_WHOLE - frame_share(claim->claim, GUI_WHOLE - claim->room_top), gui_scaled(GUI_TABS_TALL, scale));
    const int inner_wide = frame->work.width - pad_left - pad_right;
    const int inner_tall = frame->work.height - pad_top - pad_bottom;
    const int side = (side_wide > inner_wide) ? ((inner_wide > 0) ? inner_wide : 0) : side_wide;
    const int rest = inner_wide - side;
    const int map_wide = frame_share(GUI_WHOLE - frame_share(claim->claim, GUI_WHOLE - claim->reveal_right), gui_scaled(GUI_MAP_WIDE, scale));
    const int map = (map_wide > rest) ? ((rest > 0) ? rest : 0) : map_wide;
    frame->inside = (GuiRect){frame->work.x + pad_left, frame->work.y + pad_top, inner_wide, inner_tall};
    frame->side = (GuiRect){frame->inside.x, frame->inside.y, side, inner_tall};
    frame->tabs = (GuiRect){frame->inside.x + side, frame->inside.y, rest, tabs_tall};
    frame->editor = (GuiRect){frame->tabs.x, frame->tabs.y + tabs_tall, rest - map, inner_tall - tabs_tall};
    frame->map = (GuiRect){frame->editor.x + frame->editor.width, frame->editor.y, map, frame->editor.height};
}

// every part laid out for the frame's size, scale, side and claim
static void frame_laid(GuiFrame *frame)
{
    const float scale = frame->scale;
    const GuiClaimNumbers *const claim = &frame->claim;
    const int c = claim->claim;
    // the window's sides are far inside what an int represents
    const int wide = (int)frame->width;
    const int tall = (int)frame->height;
    const int bar_tall = gui_scaled(GUI_BAR_TALL, scale);
    const int status_tall = gui_scaled(GUI_STATUS_TALL, scale);
    const int strip_wide = gui_scaled(GUI_STRIP_WIDE, scale);
    const int gap = gui_scaled(GUI_FRAME_GAP, scale);
    const int gap_claim = frame_share(c, gap);
    // the bar: its top margin runs from none to the bar's height above the window, or the gap below it as the head is
    // revealed; its sides stand in by the gap at the right and by the wordmark's corner and two gaps at the left
    const int bar_y = frame_share(c, frame_share(claim->reveal_top, gap) - frame_share(GUI_WHOLE - claim->reveal_top, bar_tall));
    const int bar_x = frame_share(c, strip_wide + (2 * gap));
    frame->bar = (GuiRect){bar_x, bar_y, wide - bar_x - gap_claim, bar_tall};
    // the frame's row: from the bar's foot, pulled up by the head's part come in but not yet given its room, to the
    // status bar's head, pulled down by the foot's likewise; the work inside it by the claim's share of the gap on the
    // left, top and foot and the gap at the right
    const int frame_top = bar_y + bar_tall - frame_share(c, frame_share(claim->reveal_top - claim->room_top, bar_tall + gap));
    const int status_y = tall - status_tall - frame_share(c, frame_share(claim->reveal_bottom, gap)) +
                         frame_share(c, frame_share(GUI_WHOLE - claim->reveal_bottom, status_tall));
    const int frame_bottom = status_y + frame_share(c, frame_share(claim->reveal_bottom - claim->room_bottom, status_tall + gap));
    frame->status = (GuiRect){gap_claim, status_y, wide - (2 * gap_claim), status_tall};
    // the strip: below the wordmark's corner however far the bar has come in, slid off to the left until the left is
    // revealed, the work following it back as the left's room is taken
    const int strip_y = frame_top + gap_claim + frame_share(c, frame_share(GUI_WHOLE - claim->reveal_top, bar_tall + gap));
    const int strip_x = gap_claim - frame_share(c, frame_share(GUI_WHOLE - claim->reveal_left, strip_wide + gap));
    frame->strip = (GuiRect){strip_x, strip_y, strip_wide, frame_bottom - gap_claim - strip_y};
    const int work_x = strip_wide + (2 * gap_claim) - frame_share(c, frame_share(GUI_WHOLE - claim->room_left, strip_wide + gap));
    frame->gap = (GuiRect){wide - gap, frame_top, gap, frame_bottom - frame_top};
    frame->work = (GuiRect){work_x, frame_top + gap_claim, wide - gap - work_x, frame_bottom - gap_claim - frame_top - gap_claim};
    frame_inside_laid(frame, frame->side_shown ? gui_scaled(GUI_SIDE_WIDE, scale) : 0);
    // the controls at the bar's right end, moved down from the bar's place by their way down less the bar's own
    const int control_wide = gui_scaled(GUI_CONTROL_WIDE, scale);
    const int control_gap = gui_scaled(GUI_CONTROL_GAP, scale);
    const int controls_wide = (GUI_CONTROLS * control_wide) + ((GUI_CONTROLS - 1) * control_gap);
    const int controls_y = bar_y + frame_share(c, frame_share(claim->reveal_down - claim->reveal_top, bar_tall + gap));
    frame->controls = (GuiRect){frame->bar.x + frame->bar.width - gui_scaled(GUI_CONTROLS_END, scale) - controls_wide, controls_y,
                                controls_wide, bar_tall};
    const int lip = gui_scaled(GUI_CONTROLS_LIP, scale);
    frame->controls_pane = (GuiRect){frame->controls.x - lip, controls_y, controls_wide + lip, bar_tall};
    const int pane_share = frame_share(c, claim->reveal_down - claim->reveal_top);
    frame->controls_pane_share = (pane_share < 0) ? 0 : ((pane_share > GUI_WHOLE) ? GUI_WHOLE : pane_share);
    frame->controls_alpha = claim->controls_alpha;
    // the slats: the head's and the left's span from the work's own edge, the wordmark's corner theirs; the foot's
    // from the status bar's own ends
    int from = 0;
    int to = 0;
    frame_slat((frame->work.x < frame->bar.x) ? frame->work.x : frame->bar.x, frame->bar.x + frame->bar.width, claim->grown_top,
               claim->pulled_top, &from, &to);
    frame->bar_slat = (GuiRect){from, frame->bar.y, to - from, frame->bar.height};
    frame_slat((frame->work.y < frame->strip.y) ? frame->work.y : frame->strip.y, frame->strip.y + frame->strip.height,
               claim->grown_left, claim->pulled_left, &from, &to);
    frame->strip_slat = (GuiRect){frame->strip.x, from, frame->strip.width, to - from};
    frame_slat(frame->status.x, frame->status.x + frame->status.width, claim->grown_bottom, claim->pulled_bottom, &from, &to);
    frame->status_slat = (GuiRect){from, frame->status.y, to - from, frame->status.height};
    // a pane's corners are rounded by the claim's share of the work's radius; the radii are not negative
    frame->pane_radius = (unsigned int)frame_share(c, gui_scaled(GUI_WORK_RADIUS, scale));
    frame->radius = (unsigned int)gui_scaled(GUI_WORK_RADIUS, scale);
    // the panes' edges in the frame's words, the same arithmetic unrounded: each part's place along its slide and
    // its slat's span, met with the part's own
    const int bar_top = frame_part(c, frame_part(claim->reveal_top, gap * GUI_WHOLE) - frame_part(GUI_WHOLE - claim->reveal_top, bar_tall * GUI_WHOLE));
    const int gap_claimed = frame_part(c, gap * GUI_WHOLE);
    const int head = bar_top + (bar_tall * GUI_WHOLE) - frame_part(c, frame_part(claim->reveal_top - claim->room_top, (bar_tall + gap) * GUI_WHOLE));
    const int status_top = ((tall - status_tall) * GUI_WHOLE) - frame_part(c, frame_part(claim->reveal_bottom, gap * GUI_WHOLE)) +
                           frame_part(c, frame_part(GUI_WHOLE - claim->reveal_bottom, status_tall * GUI_WHOLE));
    const int foot = status_top + frame_part(c, frame_part(claim->reveal_bottom - claim->room_bottom, (status_tall + gap) * GUI_WHOLE));
    const int strip_top = head + gap_claimed + frame_part(c, frame_part(GUI_WHOLE - claim->reveal_top, (bar_tall + gap) * GUI_WHOLE));
    const int strip_left = gap_claimed - frame_part(c, frame_part(GUI_WHOLE - claim->reveal_left, (strip_wide + gap) * GUI_WHOLE));
    int from_edge = 0;
    int to_edge = 0;
    const int head_first = ((frame->work.x < frame->bar.x) ? frame->work.x : frame->bar.x) * GUI_WHOLE;
    frame_slat_edges(head_first, (frame->bar.x + frame->bar.width) * GUI_WHOLE, claim->grown_top, claim->pulled_top * GUI_WHOLE, &from_edge,
                     &to_edge);
    frame->bar_edges = frame_edges_met((GuiEdges){from_edge, bar_top, to_edge, bar_top + (bar_tall * GUI_WHOLE)}, frame->bar.x * GUI_WHOLE,
                                       bar_top, (frame->bar.x + frame->bar.width) * GUI_WHOLE, bar_top + (bar_tall * GUI_WHOLE));
    const int left_first = (frame->work.y * GUI_WHOLE < strip_top) ? frame->work.y * GUI_WHOLE : strip_top;
    frame_slat_edges(left_first, foot - gap_claimed, claim->grown_left, claim->pulled_left * GUI_WHOLE, &from_edge, &to_edge);
    frame->strip_edges = frame_edges_met((GuiEdges){strip_left, from_edge, strip_left + (strip_wide * GUI_WHOLE), to_edge}, strip_left,
                                         strip_top, strip_left + (strip_wide * GUI_WHOLE), foot - gap_claimed);
    frame_slat_edges(gap_claimed, (wide * GUI_WHOLE) - gap_claimed, claim->grown_bottom, claim->pulled_bottom * GUI_WHOLE, &from_edge,
                     &to_edge);
    frame->status_edges = frame_edges_met((GuiEdges){from_edge, status_top, to_edge, status_top + (status_tall * GUI_WHOLE)}, gap_claimed,
                                          status_top, (wide * GUI_WHOLE) - gap_claimed, status_top + (status_tall * GUI_WHOLE));
    const int controls_top = bar_top + frame_part(c, frame_part(claim->reveal_down - claim->reveal_top, (bar_tall + gap) * GUI_WHOLE));
    frame->controls_edges = (GuiEdges){frame->controls_pane.x * GUI_WHOLE, controls_top, (frame->controls_pane.x + frame->controls_pane.width) * GUI_WHOLE,
                                       controls_top + (bar_tall * GUI_WHOLE)};
}

void gui_frame_sided(GuiFrame *frame, int shown)
{
    frame->side_shown = shown;
    frame_laid(frame);
}

void gui_frame_claimed(GuiFrame *frame, const GuiClaimNumbers *numbers)
{
    frame->claim = *numbers;
    frame_laid(frame);
}

void gui_frame_laid(GuiFrame *frame, unsigned int width, unsigned int height, float scale)
{
    frame->width = width;
    frame->height = height;
    frame->scale = scale;
    frame->side_shown = 0;
    memor.set(&(MemoriaCfg){.dst = &frame->claim, .val = 0, .bytes = sizeof(frame->claim)});
    frame->claim.grown_top = GUI_WHOLE;
    frame->claim.grown_left = GUI_WHOLE;
    frame->claim.grown_bottom = GUI_WHOLE;
    frame->claim.controls_alpha = GUI_WHOLE;
    frame->claim.pulled_top = -1;
    frame->claim.pulled_left = -1;
    frame->claim.pulled_bottom = -1;
    frame_laid(frame);
}

// `box`, a part's slat, grown by `grown` on every side, cut to `work` and snapped to the edges of `work` it comes
// within `snap` of, into `hole`: 1, or 0 where nothing of it lies on the work
static int frame_hole(GuiRect work, GuiRect box, int grown, int snap, GuiRect *hole)
{
    if (gui_rect_empty(box))
    {
        return 0;
    }
    const GuiRect grown_box = {box.x - grown, box.y - grown, box.width + (2 * grown), box.height + (2 * grown)};
    const GuiRect on = gui_rect_met(grown_box, work);
    if (gui_rect_empty(on))
    {
        return 0;
    }
    const int left = (on.x - work.x < snap) ? work.x : on.x;
    const int top = (on.y - work.y < snap) ? work.y : on.y;
    const int right = ((work.x + work.width) - (on.x + on.width) < snap) ? work.x + work.width : on.x + on.width;
    const int bottom = ((work.y + work.height) - (on.y + on.height) < snap) ? work.y + work.height : on.y + on.height;
    *hole = (GuiRect){left, top, right - left, bottom - top};
    return 1;
}

unsigned int gui_frame_holes(const GuiFrame *frame, GuiRect *holes)
{
    const int gap = gui_scaled(GUI_FRAME_GAP, frame->scale);
    const int grown = frame_share(frame->claim.claim, gap);
    const int snap = GUI_HOLE_SNAP * gap;
    unsigned int count = 0u;
    if (frame->claim.claim <= 0)
    {
        return 0u;
    }
    count += frame_hole(frame->work, frame->bar_slat, grown, snap, &holes[count]) ? 1u : 0u;
    count += frame_hole(frame->work, frame->strip_slat, grown, snap, &holes[count]) ? 1u : 0u;
    count += frame_hole(frame->work, frame->status_slat, grown, snap, &holes[count]) ? 1u : 0u;
    if (frame->controls_pane_share > 0)
    {
        count += frame_hole(frame->work, frame->controls_pane, grown, snap, &holes[count]) ? 1u : 0u;
    }
    return count;
}
