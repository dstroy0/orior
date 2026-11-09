// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.c: the parts laid out for the window's size and scale, and again for the claim's numbers, each part's place
// the page's own arithmetic over them: the bar's and the status bar's margins, the frame's padding and the strip's
// margins as style.css sets them from the claim, the reveals and the insets; a part a window too small leaves no inset
// for is a rectangle of none
#include "frame.h"
#include "parts/scrollbar.h"

#include "../command_line/cli_internal.h"

int gui_scale(const GuiFramePixelsCfg *args)
{
    // a size in pixels times a percent is far inside what an int represents
    return ((args->pixels * args->scale) + (GUI_SCALE_ONE / 2)) / GUI_SCALE_ONE;
}

typedef struct
{
    int share;
    int pixels;
    int first;
    int end;
    int grown;
    int pulled;
    int *from;
    int *to;
    int rounded;
    GuiEdges edges;
    int left;
    int top;
    int right;
    int bottom;
    GuiFrame *frame;
    int side_width;
    GuiRect work;
    GuiRect box;
    int snap;
    GuiRect *hole;
} FrameCtx;
// `share`, a number of GUI_FIXED_ONE, of `pixels`, rounded
EMBED_INLINE int frame_share(const FrameCtx *args)
{
    // a share of a size in pixels is within a long long and, rounded, far inside what an int represents
    const long long scaled = (long long)args->share * (long long)args->pixels;
    return (int)((scaled + ((scaled >= 0LL) ? (GUI_FIXED_ONE / 2) : -(GUI_FIXED_ONE / 2))) / GUI_FIXED_ONE);
}

// `share`, a number of GUI_FIXED_ONE from 0 to it, at the nearest of the parts a fading part's share stands at
EMBED_INLINE int frame_part_share(const FrameCtx *args)
{
    // a share in parts, rounded, is a small count
    const int part = ((args->share * GUI_FRAME_PARTS) + (GUI_FIXED_ONE / 2)) / GUI_FIXED_ONE;
    return (part * GUI_FIXED_ONE) / GUI_FRAME_PARTS;
}

// `share`, a number of GUI_FIXED_ONE, of `pixels`, a count of pixels in the frame's words, saved in those words
EMBED_INLINE int frame_part(const FrameCtx *args)
{
    // a share of a size in the frame's words is within a long long and comes back to the words
    return (int)(((long long)args->share * (long long)args->pixels) / GUI_FIXED_ONE);
}

// the pane's edges met with the span `left` to `right` across and `top` to `bottom` down, each in the frame's words
EMBED_INLINE GuiEdges frame_edges_intersect(const FrameCtx *args)
{
    const GuiEdges met = {(args->edges.left > args->left) ? args->edges.left : args->left,
                          (args->edges.top > args->top) ? args->edges.top : args->top,
                          (args->edges.right < args->right) ? args->edges.right : args->right,
                          (args->edges.bottom < args->bottom) ? args->edges.bottom : args->bottom};
    return met;
}

// a slat along an edge: the part's span from `first` to `end` along the edge, grown by `grown` from the point
// `pulled`, the middle where none has been, into `from` and `to`; the grown ends rounded to the nearest pixel where
// `rounded`, the span in pixels, else cut in the frame's words
EMBED_INLINE void frame_band(const FrameCtx *args)
{
    int *from = args->from;
    int *to = args->to;
    const int middle = (args->first + args->end) / 2;
    const int required = (args->pulled < 0) ? middle : args->pulled;
    int at;
    if (required < args->first)
    {
        at = args->first;
    }
    else if (required > args->end)
    {
        at = args->end;
    }
    else
    {
        at = required;
    }
    const int before = at - args->first;
    const int after = args->end - at;
    *from = at - (args->rounded ? EMBED_CALL(frame_share, FrameCtx, .share = args->grown, .pixels = before)
                                : EMBED_CALL(frame_part, FrameCtx, .share = args->grown, .pixels = before));
    *to = at + (args->rounded ? EMBED_CALL(frame_share, FrameCtx, .share = args->grown, .pixels = after)
                              : EMBED_CALL(frame_part, FrameCtx, .share = args->grown, .pixels = after));
}

// the terminal's panel's height in the inside `pixels` tall: the height asked at the scale, no less than its least and,
// short of that, no more than the window's share or what leaves the views above it their least, and no taller than the
// inside; 0 while the panel is closed
EMBED_INLINE int frame_terminal_height(const FrameCtx *args)
{
    if (args->frame->terminal_height <= 0)
    {
        return 0;
    }
    const int scale = args->frame->scale;
    const int asked =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->frame->terminal_height, .scale = scale);
    const int least =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TERMINAL_LEAST, .scale = scale);
    // the window's height is far inside what an int represents
    const int window_height = (int)args->frame->height;
    const int share = (window_height * GUI_TERMINAL_MOST_PERCENT) / 100;
    const int leaves = window_height - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                  .pixels = GUI_TERMINAL_SHELL_LEAST, .scale = scale);
    const int most = (share < leaves) ? share : leaves;
    const int bounded = (asked < most) ? asked : most;
    const int height = (bounded > least) ? bounded : least;
    return (height < args->pixels) ? height : ((args->pixels > 0) ? args->pixels : 0);
}

// the work's inside laid out: the inside the work less its padding, the terminal's panel across its foot where it is
// open, drawn down from there by its arrival, above it the side `side_width` pixels of it at the left, the tabs row
// across the rest, and under it the editor with the map's column at its right, the row and the column away by the
// claim's numbers
EMBED_INLINE void frame_inside_layout(const FrameCtx *args)
{
    const int scale = args->frame->scale;
    const GuiAutohideNumbers *const claim = &args->frame->claim;
    const int pad_top =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_TOP, .scale = scale);
    const int pad_right =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_RIGHT, .scale = scale);
    const int pad_bottom =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_BOTTOM, .scale = scale);
    const int pad_left =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_LEFT, .scale = scale);
    // the tabs row is away while the claim holds and the head's inset is not taken, risen out of the work's head whole
    // and faded by the share away; the map is away while the right is not revealed
    const int tabs_share = GUI_FIXED_ONE - EMBED_CALL(frame_share, FrameCtx, .share = claim->claim,
                                                      .pixels = GUI_FIXED_ONE - claim->inset_top);
    const int tabs_whole =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TABS_HEIGHT, .scale = scale);
    const int tabs_height = EMBED_CALL(frame_share, FrameCtx, .share = tabs_share, .pixels = tabs_whole);
    const int inner_width = args->frame->work.width - pad_left - pad_right;
    const int inside_height = args->frame->work.height - pad_top - pad_bottom;
    const int terminal = EMBED_CALL(frame_terminal_height, FrameCtx, .frame = args->frame, .pixels = inside_height);
    const int inner_height = inside_height - terminal;
    int side;
    if (args->side_width > inner_width)
    {
        if (inner_width > 0)
        {
            side = inner_width;
        }
        else
        {
            side = 0;
        }
    }
    else
    {
        side = args->side_width;
    }
    const int rest = inner_width - side;
    const int map_whole =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MAP_WIDTH, .scale = scale);
    const int map_width = EMBED_CALL(frame_share, FrameCtx,
                                     .share = GUI_FIXED_ONE - EMBED_CALL(frame_share, FrameCtx, .share = claim->claim,
                                                                         .pixels = GUI_FIXED_ONE - claim->reveal_right),
                                     .pixels = map_whole);
    int map;
    if (map_width > rest)
    {
        if (rest > 0)
        {
            map = rest;
        }
        else
        {
            map = 0;
        }
    }
    else
    {
        map = map_width;
    }
    args->frame->inside =
        (GuiRect){args->frame->work.x + pad_left, args->frame->work.y + pad_top, inner_width, inside_height};
    args->frame->side = (GuiRect){args->frame->inside.x, args->frame->inside.y, side, inner_height};
    // the pane drawn moved up and toward the left by the share of its way gone, its own size the whole way, at the
    // part of its opacity left
    const int away = args->frame->side_away;
    args->frame->side_pane =
        (GuiRect){args->frame->side.x - EMBED_CALL(frame_share, FrameCtx, .share = away, .pixels = side),
                  args->frame->side.y - EMBED_CALL(frame_share, FrameCtx, .share = away, .pixels = inner_height), side,
                  inner_height};
    args->frame->side_share = EMBED_CALL(frame_part_share, FrameCtx, .share = GUI_FIXED_ONE - away);
    args->frame->terminal =
        (GuiRect){args->frame->inside.x, args->frame->inside.y + inner_height, inner_width, terminal};
    const int head = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                .pixels = (2 * GUI_TERMINAL_LINE) + GUI_TERMINAL_STRIP, .scale = scale);
    const int views = (terminal > head) ? (terminal - head) : 0;
    args->frame->terminal_views = (GuiRect){args->frame->terminal.x, args->frame->terminal.y + terminal - views,
                                            (views > 0) ? inner_width : 0, views};
    // the panel drawn down from its place by the share of its way left as it arrives, its views with it, at the part of
    // its opacity reached
    const int arriving = args->frame->terminal_away;
    const int down = EMBED_CALL(frame_share, FrameCtx, .share = arriving,
                                .pixels = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                     .pixels = GUI_TERMINAL_ARRIVES, .scale = scale));
    args->frame->terminal_drawn = (GuiRect){args->frame->terminal.x, args->frame->terminal.y + down,
                                            args->frame->terminal.width, args->frame->terminal.height};
    args->frame->terminal_views_drawn =
        (GuiRect){args->frame->terminal_views.x, args->frame->terminal_views.y + down,
                  args->frame->terminal_views.width, args->frame->terminal_views.height};
    args->frame->terminal_share = EMBED_CALL(frame_part_share, FrameCtx, .share = GUI_FIXED_ONE - arriving);
    args->frame->tabs = (GuiRect){args->frame->inside.x + side, args->frame->inside.y, rest, tabs_height};
    args->frame->tabs_row =
        (GuiRect){args->frame->tabs.x, args->frame->inside.y + tabs_height - tabs_whole, rest, tabs_whole};
    args->frame->tabs_share = EMBED_CALL(frame_part_share, FrameCtx, .share = tabs_share);
    args->frame->editor =
        (GuiRect){args->frame->tabs.x, args->frame->tabs.y + tabs_height, rest - map, inner_height - tabs_height};
    args->frame->map = (GuiRect){args->frame->editor.x + args->frame->editor.width, args->frame->editor.y, map,
                                 args->frame->editor.height};
    // the map stands whole from its column's left, the claim closing the column sliding it out past the editor's edge
    args->frame->map_whole =
        (GuiRect){args->frame->map.x, args->frame->map.y, (map_whole > map) ? map_whole : map, args->frame->map.height};
    // the preview beside the text takes the editor's right half, and alone the editor's rectangle, the editor left none
    // of its width at its left edge; off, it has no rectangle
    const GuiRect editor = args->frame->editor;
    const int half = editor.width / 2;
    if (args->frame->preview_mode == GUI_FRAME_PREVIEW_BESIDE)
    {
        args->frame->preview = (GuiRect){editor.x + editor.width - half, editor.y, half, editor.height};
        args->frame->editor.width = editor.width - half;
    }
    else if (args->frame->preview_mode == GUI_FRAME_PREVIEW_ALONE)
    {
        args->frame->preview = editor;
        args->frame->editor.width = 0;
    }
    else
    {
        args->frame->preview = (GuiRect){editor.x + editor.width, editor.y, 0, 0};
    }
    // The editor's scrollbars take their room from its rectangle, as a scrolling page's do: one down its right side
    // and one along its foot. The text never runs under either
    const int bar = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SCROLLBAR_WIDTH, .scale = scale);
    const GuiRect text = args->frame->editor;
    if ((text.width > bar) && (text.height > bar))
    {
        args->frame->editor = (GuiRect){text.x, text.y, text.width - bar, text.height - bar};
        args->frame->editor_bar = (GuiRect){text.x + text.width - bar, text.y, bar, text.height - bar};
        args->frame->editor_bar_across = (GuiRect){text.x, text.y + text.height - bar, text.width - bar, bar};
    }
    else
    {
        args->frame->editor_bar = (GuiRect){text.x + text.width, text.y, 0, 0};
        args->frame->editor_bar_across = (GuiRect){text.x, text.y + text.height, 0, 0};
    }
}

// every part laid out for the frame's size, scale, side and claim
static void frame_layout(const FrameCtx *args)
{
    const int scale = args->frame->scale;
    const GuiAutohideNumbers *const claim = &args->frame->claim;
    const int c = claim->claim;
    // the window's sides are far inside what an int represents
    const int pixel_width = (int)args->frame->width;
    const int pixel_height = (int)args->frame->height;
    const int bar_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BAR_HEIGHT, .scale = scale);
    const int status_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STATUS_HEIGHT, .scale = scale);
    const int strip_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_WIDTH, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FRAME_GAP, .scale = scale);
    const int gap_autohide = EMBED_CALL(frame_share, FrameCtx, .share = c, .pixels = gap);
    // the bar: its top margin runs from none to the bar's height above the window, or the gap below it as the head is
    // revealed; its sides stand in by the gap at the right and by the wordmark's corner and two gaps at the left
    const int bar_y = EMBED_CALL(
        frame_share, FrameCtx, .share = c,
        .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->reveal_top, .pixels = gap) -
                  EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_top, .pixels = bar_height));
    const int bar_x = EMBED_CALL(frame_share, FrameCtx, .share = c, .pixels = strip_width + (2 * gap));
    args->frame->bar = (GuiRect){bar_x, bar_y, pixel_width - bar_x - gap_autohide, bar_height};
    // the frame's row: from the bar's foot, pulled up by the head's part come in but not yet given its inset, to the
    // status bar's head, pulled down by the foot's likewise; the work inside it by the claim's share of the gap on the
    // left, top and foot and the gap at the right
    const int frame_top =
        bar_y + bar_height -
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->reveal_top - claim->inset_top,
                                        .pixels = bar_height + gap));
    const int status_y =
        pixel_height - status_height -
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->reveal_bottom, .pixels = gap)) +
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_bottom,
                                        .pixels = status_height));
    const int frame_bottom =
        status_y +
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->reveal_bottom - claim->inset_bottom,
                                        .pixels = status_height + gap));
    args->frame->status = (GuiRect){gap_autohide, status_y, pixel_width - (2 * gap_autohide), status_height};
    // the strip: below the wordmark's corner however far the bar has come in, slid off to the left until the left is
    // revealed, the work following it back as the left's inset is taken
    const int strip_y =
        frame_top + gap_autohide +
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_top,
                                        .pixels = bar_height + gap));
    const int strip_x = gap_autohide - EMBED_CALL(frame_share, FrameCtx, .share = c,
                                                  .pixels = EMBED_CALL(frame_share, FrameCtx,
                                                                       .share = GUI_FIXED_ONE - claim->reveal_left,
                                                                       .pixels = strip_width + gap));
    args->frame->strip = (GuiRect){strip_x, strip_y, strip_width, frame_bottom - gap_autohide - strip_y};
    const int work_x =
        strip_width + (2 * gap_autohide) -
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->inset_left,
                                        .pixels = strip_width + gap));
    args->frame->gap = (GuiRect){pixel_width - gap, frame_top, gap, frame_bottom - frame_top};
    args->frame->work = (GuiRect){work_x, frame_top + gap_autohide, pixel_width - gap - work_x,
                                  frame_bottom - gap_autohide - frame_top - gap_autohide};
    EMBED_CALL(frame_inside_layout, FrameCtx, .frame = args->frame,
               .side_width = args->frame->side_displayed ? EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                      .pixels = GUI_SIDE_WIDTH, .scale = scale)
                                                         : 0);
    // the controls at the bar's right end, moved down from the bar's place by their way down less the bar's own
    const int control_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROL_WIDTH, .scale = scale);
    const int control_gap =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROL_GAP, .scale = scale);
    const int controls_width = (GUI_CONTROLS * control_width) + ((GUI_CONTROLS - 1) * control_gap);
    const int controls_y =
        bar_y + EMBED_CALL(frame_share, FrameCtx, .share = c,
                           .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->reveal_down - claim->reveal_top,
                                                .pixels = bar_height + gap));
    args->frame->controls = (GuiRect){
        args->frame->bar.x + args->frame->bar.width -
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROLS_END, .scale = scale) -
            controls_width,
        controls_y, controls_width, bar_height};
    const int bevel =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROLS_BEVEL, .scale = scale);
    args->frame->controls_pane =
        (GuiRect){args->frame->controls.x - bevel, controls_y, controls_width + bevel, bar_height};
    const int pane_share =
        EMBED_CALL(frame_share, FrameCtx, .share = c, .pixels = claim->reveal_down - claim->reveal_top);
    if (pane_share < 0)
    {
        args->frame->controls_pane_share = 0;
    }
    else if (pane_share > GUI_FIXED_ONE)
    {
        args->frame->controls_pane_share = GUI_FIXED_ONE;
    }
    else
    {
        args->frame->controls_pane_share = EMBED_CALL(frame_part_share, FrameCtx, .share = pane_share);
    }
    args->frame->controls_alpha = EMBED_CALL(frame_part_share, FrameCtx, .share = claim->controls_alpha);
    // the slats: the head's and the left's span from the work's own edge, the wordmark's corner theirs; the foot's
    // from the status bar's own ends
    int from = 0;
    int to = 0;
    EMBED_CALL(frame_band, FrameCtx,
               .first = (args->frame->work.x < args->frame->bar.x) ? args->frame->work.x : args->frame->bar.x,
               .end = args->frame->bar.x + args->frame->bar.width, .grown = claim->grown_top,
               .pulled = claim->pulled_top, .from = &from, .to = &to, .rounded = 1);
    args->frame->bar_band = (GuiRect){from, args->frame->bar.y, to - from, args->frame->bar.height};
    EMBED_CALL(frame_band, FrameCtx,
               .first = (args->frame->work.y < args->frame->strip.y) ? args->frame->work.y : args->frame->strip.y,
               .end = args->frame->strip.y + args->frame->strip.height, .grown = claim->grown_left,
               .pulled = claim->pulled_left, .from = &from, .to = &to, .rounded = 1);
    args->frame->strip_band = (GuiRect){args->frame->strip.x, from, args->frame->strip.width, to - from};
    EMBED_CALL(frame_band, FrameCtx, .first = args->frame->status.x,
               .end = args->frame->status.x + args->frame->status.width, .grown = claim->grown_bottom,
               .pulled = claim->pulled_bottom, .from = &from, .to = &to, .rounded = 1);
    args->frame->status_band = (GuiRect){from, args->frame->status.y, to - from, args->frame->status.height};
    // a pane's corners are rounded by the claim's share of the work's radius; the radii are not negative
    args->frame->pane_radius = (unsigned int)EMBED_CALL(
        frame_share, FrameCtx, .share = c,
        .pixels = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_RADIUS, .scale = scale));
    args->frame->radius =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_RADIUS, .scale = scale);
    // the panes' edges in the frame's words, the same arithmetic unrounded: each part's place along its slide and
    // its slat's span, met with the part's own
    const int bar_top = EMBED_CALL(
        frame_part, FrameCtx, .share = c,
        .pixels = EMBED_CALL(frame_part, FrameCtx, .share = claim->reveal_top, .pixels = gap * GUI_FIXED_ONE) -
                  EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_top,
                             .pixels = bar_height * GUI_FIXED_ONE));
    const int gap_autohidden = EMBED_CALL(frame_part, FrameCtx, .share = c, .pixels = gap * GUI_FIXED_ONE);
    const int head =
        bar_top + (bar_height * GUI_FIXED_ONE) -
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = claim->reveal_top - claim->inset_top,
                                        .pixels = (bar_height + gap) * GUI_FIXED_ONE));
    const int status_top =
        ((pixel_height - status_height) * GUI_FIXED_ONE) -
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels =
                       EMBED_CALL(frame_part, FrameCtx, .share = claim->reveal_bottom, .pixels = gap * GUI_FIXED_ONE)) +
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_bottom,
                                        .pixels = status_height * GUI_FIXED_ONE));
    const int footer = status_top + EMBED_CALL(frame_part, FrameCtx, .share = c,
                                               .pixels = EMBED_CALL(frame_part, FrameCtx,
                                                                    .share = claim->reveal_bottom - claim->inset_bottom,
                                                                    .pixels = (status_height + gap) * GUI_FIXED_ONE));
    const int strip_top =
        head + gap_autohidden +
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_top,
                                        .pixels = (bar_height + gap) * GUI_FIXED_ONE));
    const int strip_left =
        gap_autohidden -
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_left,
                                        .pixels = (strip_width + gap) * GUI_FIXED_ONE));
    int from_edge = 0;
    int to_edge = 0;
    const int head_first =
        ((args->frame->work.x < args->frame->bar.x) ? args->frame->work.x : args->frame->bar.x) * GUI_FIXED_ONE;
    EMBED_CALL(frame_band, FrameCtx, .first = head_first,
               .end = (args->frame->bar.x + args->frame->bar.width) * GUI_FIXED_ONE, .grown = claim->grown_top,
               .pulled = claim->pulled_top * GUI_FIXED_ONE, .from = &from_edge, .to = &to_edge);
    args->frame->bar_edges =
        EMBED_CALL(frame_edges_intersect, FrameCtx,
                   .edges = (GuiEdges){from_edge, bar_top, to_edge, bar_top + (bar_height * GUI_FIXED_ONE)},
                   .left = args->frame->bar.x * GUI_FIXED_ONE, .top = bar_top,
                   .right = (args->frame->bar.x + args->frame->bar.width) * GUI_FIXED_ONE,
                   .bottom = bar_top + (bar_height * GUI_FIXED_ONE));
    const int left_first =
        (args->frame->work.y * GUI_FIXED_ONE < strip_top) ? args->frame->work.y * GUI_FIXED_ONE : strip_top;
    EMBED_CALL(frame_band, FrameCtx, .first = left_first, .end = footer - gap_autohidden, .grown = claim->grown_left,
               .pulled = claim->pulled_left * GUI_FIXED_ONE, .from = &from_edge, .to = &to_edge);
    args->frame->strip_edges =
        EMBED_CALL(frame_edges_intersect, FrameCtx,
                   .edges = (GuiEdges){strip_left, from_edge, strip_left + (strip_width * GUI_FIXED_ONE), to_edge},
                   .left = strip_left, .top = strip_top, .right = strip_left + (strip_width * GUI_FIXED_ONE),
                   .bottom = footer - gap_autohidden);
    EMBED_CALL(frame_band, FrameCtx, .first = gap_autohidden, .end = (pixel_width * GUI_FIXED_ONE) - gap_autohidden,
               .grown = claim->grown_bottom, .pulled = claim->pulled_bottom * GUI_FIXED_ONE, .from = &from_edge,
               .to = &to_edge);
    args->frame->status_edges =
        EMBED_CALL(frame_edges_intersect, FrameCtx,
                   .edges = (GuiEdges){from_edge, status_top, to_edge, status_top + (status_height * GUI_FIXED_ONE)},
                   .left = gap_autohidden, .top = status_top, .right = (pixel_width * GUI_FIXED_ONE) - gap_autohidden,
                   .bottom = status_top + (status_height * GUI_FIXED_ONE));
    const int controls_top =
        bar_top + EMBED_CALL(frame_part, FrameCtx, .share = c,
                             .pixels = EMBED_CALL(frame_part, FrameCtx, .share = claim->reveal_down - claim->reveal_top,
                                                  .pixels = (bar_height + gap) * GUI_FIXED_ONE));
    args->frame->controls_edges =
        (GuiEdges){args->frame->controls_pane.x * GUI_FIXED_ONE, controls_top,
                   (args->frame->controls_pane.x + args->frame->controls_pane.width) * GUI_FIXED_ONE,
                   controls_top + (bar_height * GUI_FIXED_ONE)};
}

void gui_frame_side(const GuiFrameDisplayedCfg *args)
{
    args->frame->side_displayed = args->displayed;
    args->frame->side_away = args->away;
    EMBED_CALL(frame_layout, FrameCtx, .frame = args->frame);
}

void gui_frame_terminal(const GuiFrameDisplayedCfg *args)
{
    args->frame->terminal_height = args->height;
    args->frame->terminal_away = args->away;
    EMBED_CALL(frame_layout, FrameCtx, .frame = args->frame);
}

void gui_frame_preview(const GuiFrameDisplayedCfg *args)
{
    args->frame->preview_mode = args->displayed;
    EMBED_CALL(frame_layout, FrameCtx, .frame = args->frame);
}

void gui_frame_autohide(const GuiFrameCfg *args)
{
    args->frame->claim = *args->numbers;
    EMBED_CALL(frame_layout, FrameCtx, .frame = args->frame);
}

void gui_frame_layout(const GuiFrameScaleCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    int scale = args->scale;
    args->frame->width = width;
    args->frame->height = height;
    args->frame->scale = scale;
    args->frame->side_displayed = 0;
    args->frame->side_away = 0;
    args->frame->terminal_away = 0;
    memor.set(&(MemoriaCfg){.dst = &args->frame->claim, .val = 0, .bytes = sizeof(args->frame->claim)});
    args->frame->claim.grown_top = GUI_FIXED_ONE;
    args->frame->claim.grown_left = GUI_FIXED_ONE;
    args->frame->claim.grown_bottom = GUI_FIXED_ONE;
    args->frame->claim.controls_alpha = GUI_FIXED_ONE;
    args->frame->claim.pulled_top = -1;
    args->frame->claim.pulled_left = -1;
    args->frame->claim.pulled_bottom = -1;
    EMBED_CALL(frame_layout, FrameCtx, .frame = args->frame);
}

// `box`, a part's slat, grown by `grown` on every side, cut to `work` and snapped to the edges of `work` it comes
// within `snap` of, into `hole`: 1, or 0 where nothing of it lies on the work
EMBED_INLINE int frame_hole(const FrameCtx *args)
{
    GuiRect *hole = args->hole;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->box))
    {
        return 0;
    }
    const GuiRect grown_box = {args->box.x - args->grown, args->box.y - args->grown,
                               args->box.width + (2 * args->grown), args->box.height + (2 * args->grown)};
    const GuiRect on =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = grown_box, .other = args->work);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
    {
        return 0;
    }
    const int left = (on.x - args->work.x < args->snap) ? args->work.x : on.x;
    const int top = (on.y - args->work.y < args->snap) ? args->work.y : on.y;
    const int right = ((args->work.x + args->work.width) - (on.x + on.width) < args->snap)
                          ? args->work.x + args->work.width
                          : on.x + on.width;
    const int bottom = ((args->work.y + args->work.height) - (on.y + on.height) < args->snap)
                           ? args->work.y + args->work.height
                           : on.y + on.height;
    *hole = (GuiRect){left, top, right - left, bottom - top};
    return 1;
}

unsigned int gui_frame_holes(const GuiFrameHolesCfg *args)
{
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FRAME_GAP,
                               .scale = args->frame_read->scale);
    const int grown = EMBED_CALL(frame_share, FrameCtx, .share = args->frame_read->claim.claim, .pixels = gap);
    const int snap = GUI_HOLE_SNAP * gap;
    unsigned int count = 0u;
    if (args->frame_read->claim.claim <= 0)
    {
        return 0u;
    }
    count += EMBED_CALL(frame_hole, FrameCtx, .work = args->frame_read->work, .box = args->frame_read->bar_band,
                        .grown = grown, .snap = snap, .hole = &args->holes[count])
                 ? 1u
                 : 0u;
    count += EMBED_CALL(frame_hole, FrameCtx, .work = args->frame_read->work, .box = args->frame_read->strip_band,
                        .grown = grown, .snap = snap, .hole = &args->holes[count])
                 ? 1u
                 : 0u;
    count += EMBED_CALL(frame_hole, FrameCtx, .work = args->frame_read->work, .box = args->frame_read->status_band,
                        .grown = grown, .snap = snap, .hole = &args->holes[count])
                 ? 1u
                 : 0u;
    if (args->frame_read->controls_pane_share > 0)
    {
        count +=
            EMBED_CALL(frame_hole, FrameCtx, .work = args->frame_read->work, .box = args->frame_read->controls_pane,
                       .grown = grown, .snap = snap, .hole = &args->holes[count])
                ? 1u
                : 0u;
    }
    return count;
}
