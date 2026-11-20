// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.c: the parts arranged for the window's size and scale, and again for the claim's numbers, each part's place
// the page's own arithmetic over them: the bar's and the status bar's margins, the frame's padding and the side_menu's
// margins as style.css sets them from the claim, the reveals and the insets; a part a window too small leaves no inset
// for is a rectangle of none
#include "frame.h"

#include "memoria_operor/memoria_operor.h"
#include "scrollbar.h"


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
    int rest;
} FrameCtx;
// `share`, a number of GUI_FIXED_ONE, of `pixels`, rounded
EMBED_INLINE int frame_share(const FrameCtx *args)
{
    // a share of a size in pixels is within a long long and, rounded, far inside what an int represents
    const long long scaled = (long long)args->share * (long long)args->pixels;
    return (int)((scaled + ((scaled >= 0LL) ? (GUI_FIXED_ONE / 2) : -(GUI_FIXED_ONE / 2))) / GUI_FIXED_ONE);
}

// `share`, a number of GUI_FIXED_ONE from 0 to it, at the nearest of the parts where the share of a part that fades is
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

// the lesser and the greater of the place `first` of a part at its reveal, and `end` at the lagged number of the
// reveal. A pane spans between the two. Its far edge trails as it comes in, and its near edge trails as it goes
EMBED_INLINE int frame_least(const FrameCtx *args)
{
    return (args->first < args->end) ? args->first : args->end;
}

EMBED_INLINE int frame_greatest(const FrameCtx *args)
{
    return (args->first > args->end) ? args->first : args->end;
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

// a slat along an edge, into `from` and `to`. It is the span of the part from `first` to `end` along the edge, grown by
// `grown` from the point `pulled`, or the middle where there is none. Where `rounded`, the grown ends round to the
// nearest pixel, with the span in pixels. Else the code cuts the ends in the words of the frame
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

// the height of the panel of the terminal in the inside, which is `pixels` tall. It is the requested height at the
// scale, no less than its least. Short of that, it is no more than the share of the window. It is also no more than the
// height that leaves the views above it their least, and no taller than the inside. It is 0 while the panel does not show
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
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TERMINAL_MIN, .scale = scale);
    // the window's height is far inside what an int represents
    const int window_height = (int)args->frame->height;
    const int share = (window_height * GUI_TERMINAL_MAX_PERCENT) / 100;
    const int leaves = window_height - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                  .pixels = GUI_TERMINAL_SHELL_MIN, .scale = scale);
    const int max = (share < leaves) ? share : leaves;
    const int bounded = (asked < max) ? asked : max;
    const int height = (bounded > least) ? bounded : least;
    return (height < args->pixels) ? height : ((args->pixels > 0) ? args->pixels : 0);
}

// the inside of the work, arranged. The inside is the work less its padding. Where the panel of the terminal is open,
// it goes across the foot, drawn down from there by its arrival.
//
// Above it, the side takes `side_width` pixels at the left, and the tabs row goes across the rest. Under the tabs row
// is the editor, with the column of the map at its right. The numbers of the claim move the row and the column away
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
    // the tabs row is away while the claim holds and the claim does not take its own reveal. It rises out of the head
    // of the work whole, and fades by the share away. The map is away while the claim does not reveal the right
    const int tabs_share = GUI_FIXED_ONE - EMBED_CALL(frame_share, FrameCtx, .share = claim->claim,
                                                      .pixels = GUI_FIXED_ONE - claim->reveal_tabs);
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
    // the pane, drawn up and toward the left by the share of its way gone. It keeps its own size the whole way, at the
    // part of its opacity that stays
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
    // the panel, drawn down from its place by the share of its way left as it arrives, with its views. It is at the part
    // of its opacity that it reached
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
    // the tabs row sits a gap under the border line, the gap away with the row: the beads stay clear of the line
    const int tabs_gap = EMBED_CALL(
        frame_share, FrameCtx, .share = tabs_share,
        .pixels = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TABS_GAP, .scale = scale));
    args->frame->tabs =
        (GuiRect){args->frame->inside.x + side, args->frame->inside.y + tabs_gap, rest, tabs_height};
    args->frame->tabs_row =
        (GuiRect){args->frame->tabs.x, args->frame->tabs.y + tabs_height - tabs_whole, rest, tabs_whole};
    args->frame->tabs_share = EMBED_CALL(frame_part_share, FrameCtx, .share = tabs_share);
    args->frame->editor = (GuiRect){args->frame->tabs.x, args->frame->tabs.y + tabs_height, rest - map,
                                    inner_height - tabs_height - tabs_gap};
    args->frame->map = (GuiRect){args->frame->editor.x + args->frame->editor.width, args->frame->editor.y, map,
                                 args->frame->editor.height};
    // the map is whole from the left of its column. When the claim shuts the column, the map slides out past the edge of
    // the editor
    args->frame->map_whole =
        (GuiRect){args->frame->map.x, args->frame->map.y, (map_whole > map) ? map_whole : map, args->frame->map.height};
    // the preview beside the text takes the right half of the editor. Alone, it takes the rectangle of the editor, and
    // the editor keeps none of its width at its left edge. When the preview is off, it has no rectangle
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
    // The scrollbars of the editor take their room from its rectangle, as the scrollbars of a page do. One goes down its
    // right side, its standoff in from the line of the work. It starts its own thickness down from the head of the
    // text, and stops as far up from the foot of the text. The other goes along its foot. The text never runs under
    // either
    const int bar = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SCROLLBAR_WIDTH, .scale = scale);
    const int standoff =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SCROLLBAR_STANDOFF, .scale = scale);
    const GuiRect text = args->frame->editor;
    if ((text.width > bar + standoff) && (text.height > 2 * bar))
    {
        args->frame->editor = (GuiRect){text.x, text.y, text.width - bar - standoff, text.height - bar};
        args->frame->editor_bar =
            (GuiRect){text.x + text.width - bar - standoff, text.y + bar, bar, text.height - (2 * bar)};
        args->frame->editor_bar_across =
            (GuiRect){text.x, text.y + text.height - bar, text.width - bar - standoff, bar};
    }
    else
    {
        args->frame->editor_bar = (GuiRect){text.x + text.width, text.y, 0, 0};
        args->frame->editor_bar_across = (GuiRect){text.x, text.y + text.height, 0, 0};
    }
}

// each part arranged for the size, scale, side and claim of the frame. Where this is not the rest layout itself, the
// bar and its tools go again, with no claim, into the rest rectangles. The controls and the strip do the same.
//
// There their beads bake the places that the slides of the claim move them to. The tabs row also rests with no side
// beside it. The arrival of a side pane moves its beads without a bake
static void frame_layout(const FrameCtx *args);
static void frame_rest_positioned(GuiFrame *frame)
{
    GuiFrame rest = *frame;
    rest.claim.claim = 0;
    EMBED_CALL(frame_layout, FrameCtx, .frame = &rest, .rest = 1);
    frame->top_menu_rest = rest.top_menu;
    frame->controls_rest = rest.controls;
    frame->top_menu_tools_rest = rest.top_menu_tools;
    frame->side_menu_rest = rest.side_menu;
    frame->tabs_rest = (GuiRect){rest.inside.x, rest.tabs_row.y, rest.inside.width, rest.tabs_row.height};
}

static void frame_layout(const FrameCtx *args)
{
    const int scale = args->frame->scale;
    const GuiAutohideNumbers *const claim = &args->frame->claim;
    const int c = claim->claim;
    // the window's sides are far inside what an int represents
    const int pixel_width = (int)args->frame->width;
    const int pixel_height = (int)args->frame->height;
    const int top_menu_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TOP_MENU_HEIGHT, .scale = scale);
    const int status_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STATUS_HEIGHT, .scale = scale);
    const int side_menu_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDE_MENU_WIDTH, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FRAME_GAP, .scale = scale);
    const int gap_autohide = EMBED_CALL(frame_share, FrameCtx, .share = c, .pixels = gap);
    // the bar. Its top margin runs from none to the height of the bar above the window. As the claim reveals the head,
    // the margin is the gap below it. Its sides are in by the gap at the right, and by the corner of the wordmark and two gaps at the
    // left
    const int top_menu_y = EMBED_CALL(
        frame_share, FrameCtx, .share = c,
        .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->reveal_top, .pixels = gap) -
                  EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_top, .pixels = top_menu_height));
    // the bar's place at its reveal's lagged number: its pane spans between its two places
    const int top_menu_y_lag = EMBED_CALL(
        frame_share, FrameCtx, .share = c,
        .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->lag_top, .pixels = gap) -
                  EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->lag_top, .pixels = top_menu_height));
    const int top_menu_x = EMBED_CALL(frame_share, FrameCtx, .share = c, .pixels = side_menu_width + (2 * gap));
    args->frame->top_menu = (GuiRect){top_menu_x, top_menu_y, pixel_width - top_menu_x - gap_autohide, top_menu_height};
    // the row of the frame. It runs from the foot of the bar to the head of the status bar. The part of the head that
    // came in before its inset pulls the top up. The part of the foot pulls the bottom down in the same way.
    //
    // The work sits inside it. On the left, top and foot, it is in by the share of the gap that the claim gives. At the
    // right it is in by the gap
    const int frame_top =
        top_menu_y + top_menu_height -
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->reveal_top - claim->inset_top,
                                        .pixels = top_menu_height + gap));
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
    const int status_y_lag =
        pixel_height - status_height -
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->lag_bottom, .pixels = gap)) +
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->lag_bottom,
                                        .pixels = status_height));
    args->frame->status = (GuiRect){gap_autohide, status_y, pixel_width - (2 * gap_autohide), status_height};
    // the side_menu. It sits below the corner of the wordmark, however far the bar came in. It slides off to the left
    // until the claim reveals the left. The work follows it back as the claim takes the inset of the left
    const int side_menu_y =
        frame_top + gap_autohide +
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_top,
                                        .pixels = top_menu_height + gap));
    const int side_menu_x = gap_autohide - EMBED_CALL(frame_share, FrameCtx, .share = c,
                                                  .pixels = EMBED_CALL(frame_share, FrameCtx,
                                                                       .share = GUI_FIXED_ONE - claim->reveal_left,
                                                                       .pixels = side_menu_width + gap));
    const int side_menu_x_lag =
        gap_autohide - EMBED_CALL(frame_share, FrameCtx, .share = c,
                                  .pixels = EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->lag_left,
                                                       .pixels = side_menu_width + gap));
    args->frame->side_menu = (GuiRect){side_menu_x, side_menu_y, side_menu_width, frame_bottom - gap_autohide - side_menu_y};
    const int work_x =
        side_menu_width + (2 * gap_autohide) -
        EMBED_CALL(frame_share, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_share, FrameCtx, .share = GUI_FIXED_ONE - claim->inset_left,
                                        .pixels = side_menu_width + gap));
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
        top_menu_y + EMBED_CALL(frame_share, FrameCtx, .share = c,
                           .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->reveal_down - claim->reveal_top,
                                                .pixels = top_menu_height + gap));
    args->frame->controls = (GuiRect){
        args->frame->top_menu.x + args->frame->top_menu.width -
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROLS_END, .scale = scale) -
            controls_width,
        controls_y, controls_width, top_menu_height};
    const int bevel =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CONTROLS_BEVEL, .scale = scale);
    // the controls' pane spans from the controls' place to their place at the lagged numbers
    const int controls_y_lag =
        top_menu_y_lag + EMBED_CALL(frame_share, FrameCtx, .share = c,
                                    .pixels = EMBED_CALL(frame_share, FrameCtx, .share = claim->lag_down - claim->lag_top,
                                                         .pixels = top_menu_height + gap));
    const int controls_pane_y = EMBED_CALL(frame_least, FrameCtx, .first = controls_y, .end = controls_y_lag);
    args->frame->controls_pane =
        (GuiRect){args->frame->controls.x - bevel, controls_pane_y, controls_width + bevel,
                  EMBED_CALL(frame_greatest, FrameCtx, .first = controls_y, .end = controls_y_lag) - controls_pane_y +
                      top_menu_height};
    // the tools of the bar sit left of the controls, with their buttons centered on the row of the controls. They move
    // with the controls
    const int tool = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TOP_MENU_TOOL, .scale = scale);
    const int tool_gap =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TOP_MENU_TOOL_GAP, .scale = scale);
    const int tools_width = (GUI_TOP_MENU_TOOLS * tool) + ((GUI_TOP_MENU_TOOLS - 1) * tool_gap);
    args->frame->top_menu_tools =
        (GuiRect){args->frame->controls.x -
                      EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TOP_MENU_TOOLS_GAP, .scale = scale) -
                      tools_width,
                  controls_y + ((top_menu_height - tool) / 2), tools_width, tool};
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
    // The scrollbar of the editor is under the pane of the controls as the pane comes down. Its head keeps the standoff
    // from the line of the work that it keeps from the top of the work. That is below the line that bends round the pane
    if (args->frame->controls_pane_share > 0)
    {
        const int under = args->frame->controls_pane.y + args->frame->controls_pane.height + gap_autohide +
                          (args->frame->editor_bar.y - args->frame->work.y);
        if (under > args->frame->editor_bar.y)
        {
            const int moved = under - args->frame->editor_bar.y;
            args->frame->editor_bar.y = under;
            args->frame->editor_bar.height =
                (args->frame->editor_bar.height > moved) ? args->frame->editor_bar.height - moved : 0;
        }
    }
    // the slats. The slats of the head and the left span from the edge of the work, and the corner of the wordmark has
    // theirs. The slat of the foot spans from the ends of the status bar.
    //
    // Each slat spans from the place of its part to the place at the lagged number. The pane stretches as a bead does.
    // Its contents, in the rectangle of the part, follow the front edge
    int from = 0;
    int to = 0;
    EMBED_CALL(frame_band, FrameCtx,
               .first = (args->frame->work.x < args->frame->top_menu.x) ? args->frame->work.x : args->frame->top_menu.x,
               .end = args->frame->top_menu.x + args->frame->top_menu.width, .grown = claim->grown_top,
               .pulled = claim->pulled_top, .from = &from, .to = &to, .rounded = 1);
    const int bar_band_y = EMBED_CALL(frame_least, FrameCtx, .first = top_menu_y, .end = top_menu_y_lag);
    args->frame->top_menu_band =
        (GuiRect){from, bar_band_y, to - from,
                  EMBED_CALL(frame_greatest, FrameCtx, .first = top_menu_y, .end = top_menu_y_lag) - bar_band_y +
                      top_menu_height};
    args->frame->top_menu_pane = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->top_menu_band,
        .other = (GuiRect){args->frame->top_menu.x, bar_band_y, args->frame->top_menu.width, args->frame->top_menu_band.height});
    EMBED_CALL(frame_band, FrameCtx,
               .first = (args->frame->work.y < args->frame->side_menu.y) ? args->frame->work.y : args->frame->side_menu.y,
               .end = args->frame->side_menu.y + args->frame->side_menu.height, .grown = claim->grown_left,
               .pulled = claim->pulled_left, .from = &from, .to = &to, .rounded = 1);
    const int side_menu_band_x = EMBED_CALL(frame_least, FrameCtx, .first = side_menu_x, .end = side_menu_x_lag);
    args->frame->side_menu_band =
        (GuiRect){side_menu_band_x, from,
                  EMBED_CALL(frame_greatest, FrameCtx, .first = side_menu_x, .end = side_menu_x_lag) - side_menu_band_x +
                      side_menu_width,
                  to - from};
    args->frame->side_menu_pane =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->side_menu_band,
                   .other = (GuiRect){side_menu_band_x, args->frame->side_menu.y, args->frame->side_menu_band.width,
                                      args->frame->side_menu.height});
    EMBED_CALL(frame_band, FrameCtx, .first = args->frame->status.x,
               .end = args->frame->status.x + args->frame->status.width, .grown = claim->grown_bottom,
               .pulled = claim->pulled_bottom, .from = &from, .to = &to, .rounded = 1);
    const int status_band_y = EMBED_CALL(frame_least, FrameCtx, .first = status_y, .end = status_y_lag);
    args->frame->status_band =
        (GuiRect){from, status_band_y, to - from,
                  EMBED_CALL(frame_greatest, FrameCtx, .first = status_y, .end = status_y_lag) - status_band_y +
                      status_height};
    args->frame->status_pane = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->status_band,
        .other = (GuiRect){args->frame->status.x, status_band_y, args->frame->status.width, args->frame->status_band.height});
    // the share of the radius of the work that the claim gives rounds the corners of a pane. The radii are not negative
    args->frame->pane_radius = (unsigned int)EMBED_CALL(
        frame_share, FrameCtx, .share = c,
        .pixels = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_RADIUS, .scale = scale));
    args->frame->radius =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_RADIUS, .scale = scale);
    // the edges of the panes in the words of the frame, the same arithmetic without the round. That is the place of each
    // part along its slide and the span of its slat, met with the own span of the part
    const int bar_top = EMBED_CALL(
        frame_part, FrameCtx, .share = c,
        .pixels = EMBED_CALL(frame_part, FrameCtx, .share = claim->reveal_top, .pixels = gap * GUI_FIXED_ONE) -
                  EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_top,
                             .pixels = top_menu_height * GUI_FIXED_ONE));
    const int bar_top_lag = EMBED_CALL(
        frame_part, FrameCtx, .share = c,
        .pixels = EMBED_CALL(frame_part, FrameCtx, .share = claim->lag_top, .pixels = gap * GUI_FIXED_ONE) -
                  EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->lag_top,
                             .pixels = top_menu_height * GUI_FIXED_ONE));
    const int gap_autohidden = EMBED_CALL(frame_part, FrameCtx, .share = c, .pixels = gap * GUI_FIXED_ONE);
    const int head =
        bar_top + (top_menu_height * GUI_FIXED_ONE) -
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = claim->reveal_top - claim->inset_top,
                                        .pixels = (top_menu_height + gap) * GUI_FIXED_ONE));
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
    const int status_top_lag =
        ((pixel_height - status_height) * GUI_FIXED_ONE) -
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = claim->lag_bottom, .pixels = gap * GUI_FIXED_ONE)) +
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->lag_bottom,
                                        .pixels = status_height * GUI_FIXED_ONE));
    const int side_menu_top =
        head + gap_autohidden +
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_top,
                                        .pixels = (top_menu_height + gap) * GUI_FIXED_ONE));
    const int side_menu_left =
        gap_autohidden -
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->reveal_left,
                                        .pixels = (side_menu_width + gap) * GUI_FIXED_ONE));
    const int side_menu_left_lag =
        gap_autohidden -
        EMBED_CALL(frame_part, FrameCtx, .share = c,
                   .pixels = EMBED_CALL(frame_part, FrameCtx, .share = GUI_FIXED_ONE - claim->lag_left,
                                        .pixels = (side_menu_width + gap) * GUI_FIXED_ONE));
    int from_edge = 0;
    int to_edge = 0;
    const int head_first =
        ((args->frame->work.x < args->frame->top_menu.x) ? args->frame->work.x : args->frame->top_menu.x) * GUI_FIXED_ONE;
    EMBED_CALL(frame_band, FrameCtx, .first = head_first,
               .end = (args->frame->top_menu.x + args->frame->top_menu.width) * GUI_FIXED_ONE, .grown = claim->grown_top,
               .pulled = claim->pulled_top * GUI_FIXED_ONE, .from = &from_edge, .to = &to_edge);
    // each pane's edges span from the part's place to its place at the lagged number, as the slats do
    const int bar_edge_top = EMBED_CALL(frame_least, FrameCtx, .first = bar_top, .end = bar_top_lag);
    const int bar_edge_bottom =
        EMBED_CALL(frame_greatest, FrameCtx, .first = bar_top, .end = bar_top_lag) + (top_menu_height * GUI_FIXED_ONE);
    args->frame->bar_edges =
        EMBED_CALL(frame_edges_intersect, FrameCtx, .edges = (GuiEdges){from_edge, bar_edge_top, to_edge, bar_edge_bottom},
                   .left = args->frame->top_menu.x * GUI_FIXED_ONE, .top = bar_edge_top,
                   .right = (args->frame->top_menu.x + args->frame->top_menu.width) * GUI_FIXED_ONE,
                   .bottom = bar_edge_bottom);
    const int left_first =
        (args->frame->work.y * GUI_FIXED_ONE < side_menu_top) ? args->frame->work.y * GUI_FIXED_ONE : side_menu_top;
    EMBED_CALL(frame_band, FrameCtx, .first = left_first, .end = footer - gap_autohidden, .grown = claim->grown_left,
               .pulled = claim->pulled_left * GUI_FIXED_ONE, .from = &from_edge, .to = &to_edge);
    const int side_menu_edge_left = EMBED_CALL(frame_least, FrameCtx, .first = side_menu_left, .end = side_menu_left_lag);
    const int side_menu_edge_right = EMBED_CALL(frame_greatest, FrameCtx, .first = side_menu_left, .end = side_menu_left_lag) +
                                     (side_menu_width * GUI_FIXED_ONE);
    args->frame->side_menu_edges =
        EMBED_CALL(frame_edges_intersect, FrameCtx,
                   .edges = (GuiEdges){side_menu_edge_left, from_edge, side_menu_edge_right, to_edge},
                   .left = side_menu_edge_left, .top = side_menu_top, .right = side_menu_edge_right,
                   .bottom = footer - gap_autohidden);
    EMBED_CALL(frame_band, FrameCtx, .first = gap_autohidden, .end = (pixel_width * GUI_FIXED_ONE) - gap_autohidden,
               .grown = claim->grown_bottom, .pulled = claim->pulled_bottom * GUI_FIXED_ONE, .from = &from_edge,
               .to = &to_edge);
    const int status_edge_top = EMBED_CALL(frame_least, FrameCtx, .first = status_top, .end = status_top_lag);
    const int status_edge_bottom =
        EMBED_CALL(frame_greatest, FrameCtx, .first = status_top, .end = status_top_lag) + (status_height * GUI_FIXED_ONE);
    args->frame->status_edges =
        EMBED_CALL(frame_edges_intersect, FrameCtx,
                   .edges = (GuiEdges){from_edge, status_edge_top, to_edge, status_edge_bottom}, .left = gap_autohidden,
                   .top = status_edge_top, .right = (pixel_width * GUI_FIXED_ONE) - gap_autohidden,
                   .bottom = status_edge_bottom);
    const int controls_top =
        bar_top + EMBED_CALL(frame_part, FrameCtx, .share = c,
                             .pixels = EMBED_CALL(frame_part, FrameCtx, .share = claim->reveal_down - claim->reveal_top,
                                                  .pixels = (top_menu_height + gap) * GUI_FIXED_ONE));
    const int controls_top_lag =
        bar_top_lag + EMBED_CALL(frame_part, FrameCtx, .share = c,
                                 .pixels = EMBED_CALL(frame_part, FrameCtx, .share = claim->lag_down - claim->lag_top,
                                                      .pixels = (top_menu_height + gap) * GUI_FIXED_ONE));
    args->frame->controls_edges =
        (GuiEdges){args->frame->controls_pane.x * GUI_FIXED_ONE,
                   EMBED_CALL(frame_least, FrameCtx, .first = controls_top, .end = controls_top_lag),
                   (args->frame->controls_pane.x + args->frame->controls_pane.width) * GUI_FIXED_ONE,
                   EMBED_CALL(frame_greatest, FrameCtx, .first = controls_top, .end = controls_top_lag) +
                       (top_menu_height * GUI_FIXED_ONE)};
    if (!args->rest)
    {
        frame_rest_positioned(args->frame);
    }
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

// `box`, the slat of a part, into `hole`. It grows by `grown` on each side, cuts to `work`, and snaps to each edge of
// `work` within `snap` of it. The result is 1, or 0 where no part of it lies on the work
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
    count += EMBED_CALL(frame_hole, FrameCtx, .work = args->frame_read->work, .box = args->frame_read->top_menu_band,
                        .grown = grown, .snap = snap, .hole = &args->holes[count])
                 ? 1u
                 : 0u;
    count += EMBED_CALL(frame_hole, FrameCtx, .work = args->frame_read->work, .box = args->frame_read->side_menu_band,
                        .grown = grown, .snap = snap, .hole = &args->holes[count])
                 ? 1u
                 : 0u;
    count += EMBED_CALL(frame_hole, FrameCtx, .work = args->frame_read->work, .box = args->frame_read->status_band,
                        .grown = grown, .snap = snap, .hole = &args->holes[count])
                 ? 1u
                 : 0u;
    // the pane of the controls is a tab in the corner of the work. Its hole is the pane itself, and the border bends round
    // its edge with no gap. The pane and the bend read as one object
    if (args->frame_read->controls_pane_share > 0)
    {
        count +=
            EMBED_CALL(frame_hole, FrameCtx, .work = args->frame_read->work, .box = args->frame_read->controls_pane,
                       .grown = 0, .snap = snap, .hole = &args->holes[count])
                ? 1u
                : 0u;
    }
    return count;
}
