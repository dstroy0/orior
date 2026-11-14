// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs.c: the tabs row. Each tab is as wide as its padding, the mark of changes not saved where it has them, its name
// and its close mark. The tabs stand in one side_menu, and the row shows the part of the side_menu from the frame's
// `tabs_left`. The row's edges cut the tabs that cross them
#include "tabs.h"
#include "../../../includes/arith/arith.h"
#include "../../../includes/window/block.h"
#include "../../../includes/window/lights.h"

// the close mark's text, its text while the close waits on a second, and the mark of changes not saved
static const char s_close[] = "\xc3\x97";
static const char s_close_waiting[] = "\xc3\x97?";
static const char s_changed[] = "\xe2\x97\x8f";

const char *gui_tab_name(const GuiTabsCfg *args)
{
    // the tab's path copied into the tabs' own buffer, its name inside it, which the caller reads before the next tab
    static char s_tab_path[CLI_PATH_MAX_LEN];
    const char *const name = cli_view_name_at(args->at, s_tab_path);
    *args->length = (name != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_PATH_MAX_LEN}) : 0ULL;
    return (name != NULL) ? name : "";
}

typedef struct
{
    GuiGlyphs *glyphs;
    unsigned int tab;
    unsigned int size;
    int scale;
    unsigned long long *length;
    const GuiTabsGlyphsFrameCfg *drawn;
    unsigned int count;
} TabsCtx;

// the close mark the tab `tab` shows, ×? while its close waits on a second, its length into `length`
EMBED_INLINE const char *tabs_close_text(const TabsCtx *args)
{
    const int closing = cli_view_closing(args->tab);
    *args->length = closing ? (sizeof(s_close_waiting) - 1u) : (sizeof(s_close) - 1u);
    return closing ? s_close_waiting : s_close;
}

// whether the tab `tab`'s text has changes not saved
EMBED_INLINE int tabs_changed(const TabsCtx *args)
{
    char key[CLI_NAME_MAX_LEN];
    return cli_view_key(args->tab, key) && EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = key);
}

// the width the mark of changes not saved and the gap after it take before the tab `tab`'s name, 0 where it has none
EMBED_INLINE int tabs_changed_width(const TabsCtx *args)
{
    if (!tabs_changed(args))
    {
        return 0;
    }
    const float mark =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs, .face = GUI_FACE_CODE,
                   .size = args->size, .text = s_changed, .length = sizeof(s_changed) - 1u);
    return (int)arith_round_f32(mark) +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_GAP, .scale = args->scale);
}

// a tab's width: its padding each side, the mark of changes not saved, its name, the gap and the close mark with its
// padding
EMBED_INLINE int tabs_width(const TabsCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    const unsigned int tab = args->tab;
    unsigned int size = args->size;
    int scale = args->scale;
    unsigned long long length = 0ULL;
    const char *const name = EMBED_CALL(gui_tabs.tab_name, GuiTabsCfg, .at = tab, .length = &length);
    const float text = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                  .face = GUI_FACE_CODE, .size = size, .text = name, .length = length);
    unsigned long long close_length = 0ULL;
    const char *const close_text = EMBED_CALL(tabs_close_text, TabsCtx, .tab = tab, .length = &close_length);
    const float close = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                   .face = GUI_FACE_CODE, .size = size, .text = close_text, .length = close_length);
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_PAD_LEFT, .scale = scale) +
           EMBED_CALL(tabs_changed_width, TabsCtx, .glyphs = glyphs, .tab = tab, .size = size, .scale = scale) +
           (int)arith_round_f32(text) +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_GAP, .scale = scale) +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_CLOSE_PAD, .scale = scale) +
           (int)arith_round_f32(close) +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_CLOSE_PAD, .scale = scale) +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_PAD_RIGHT, .scale = scale);
}

GuiRect gui_tab_rect(const GuiTabsGlyphsCfg *args)
{
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_TAB_SIZE, .scale = args->frame->scale);
    const unsigned int count = cli_view_count();
    // the tabs stand clear of the work's lines: room above them, and room before the first
    const int clear =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_CLEAR, .scale = args->frame->scale);
    // the side_menu of tabs stands slid left by the pixels the row holds left of its view
    int x = args->frame->tabs.x + clear - args->frame->tabs_left;
    for (unsigned int each = 0u; (each < args->at) && (each < count); each += 1u)
    {
        x += EMBED_CALL(tabs_width, TabsCtx, .glyphs = args->glyphs, .tab = each, .size = size,
                        .scale = args->frame->scale);
    }
    const int pixel_width = (args->at < count) ? EMBED_CALL(tabs_width, TabsCtx, .glyphs = args->glyphs,
                                                            .tab = args->at, .size = size, .scale = args->frame->scale)
                                               : 0;
    // a tab stands whole in the row, which the claim lifts out of the work's head
    return (GuiRect){
        x, args->frame->tabs_row.y + clear, pixel_width,
        args->frame->tabs_row.height - clear -
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = args->frame->scale)};
}

int gui_tabs_whole(const GuiTabsGlyphsCfg *args)
{
    // the place after the last tab, put back by the slide, less the row's left edge
    const GuiRect after = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = args->glyphs,
                                     .frame = args->frame, .at = cli_view_count());
    return after.x + args->frame->tabs_left - args->frame->tabs.x;
}

GuiRect gui_tab_close_rect(const GuiTabsGlyphsCfg *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    const GuiRect rect =
        EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = glyphs, .frame = args->frame, .at = args->at);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_TAB_SIZE, .scale = args->frame->scale);
    unsigned long long close_length = 0ULL;
    const char *const close_text = EMBED_CALL(tabs_close_text, TabsCtx, .tab = args->at, .length = &close_length);
    const float close = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                   .face = GUI_FACE_CODE, .size = size, .text = close_text, .length = close_length);
    const int pixel_width = (2 * EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_CLOSE_PAD,
                                            .scale = args->frame->scale)) +
                            (int)arith_round_f32(close);
    return (GuiRect){rect.x + rect.width -
                         EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_PAD_RIGHT,
                                    .scale = args->frame->scale) -
                         pixel_width,
                     rect.y, pixel_width, rect.height};
}

int gui_tabs_hit(const GuiTabsGlyphsFrameCountCfg *args)
{
    unsigned int *at = args->at_pointer;
    int *closing = args->closing;
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->tabs, .x = args->x,
                    .y = args->y))
    {
        return 0;
    }
    const unsigned int count = cli_view_count();
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = args->glyphs,
                                          .frame = args->frame, .at = each),
                       .x = args->x, .y = args->y))
        {
            *at = each;
            *closing = EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                                  .rect = EMBED_CALL(gui_tabs_glyphs.tab_close_rect, GuiTabsGlyphsCfg,
                                                     .glyphs = args->glyphs, .frame = args->frame, .at = each),
                                  .x = args->x, .y = args->y);
            return 1;
        }
    }
    return 0;
}

// The beads over the first GUI_TABS_BEADS tabs of `count`, drawn as their presses stand at `time_ms`: the atlas laid
// out for the widest tab in the tab's shape, each tab's cell at its place in the row's whole side_menu, lit by the
// window's lights, over the second segment of the block's tab beads part; one unbaked tab baked a draw, the rest
// left for the draws after: how many still wait
EMBED_INLINE unsigned int tabs_beads_draw(const TabsCtx *args)
{
    const GuiTabsGlyphsFrameCfg *const drawn = args->drawn;
    const GuiFrame *const frame = drawn->frame;
    const int scale = frame->scale;
    const unsigned int tools = (args->count < GUI_TABS_BEADS) ? args->count : GUI_TABS_BEADS;
    if ((drawn->buttons == NULL) || (tools == 0u))
    {
        return 0u;
    }
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_PAD, .scale = scale);
    const int reach = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_REACH, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_SPACE, .scale = scale);
    const int radius = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_RADIUS, .scale = scale);
    GuiRect cells[GUI_TABS_BEADS];
    GuiRect placed[GUI_TABS_BEADS];
    int widest = 0;
    int tall = 0;
    for (unsigned int at = 0u; at < tools; at += 1u)
    {
        const GuiRect rect =
            EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = drawn->glyphs, .frame = frame, .at = at);
        cells[at] = (GuiRect){rect.x + frame->tabs_left - pad, rect.y - pad, rect.width - gap, rect.height};
        placed[at] = (GuiRect){rect.x - pad, rect.y - pad, rect.width - gap, rect.height};
        widest = (cells[at].width > widest) ? cells[at].width : widest;
        tall = rect.height;
    }
    GuiLight lights[GUI_LIGHTS_MAX];
    const unsigned int light_count =
        EMBED_CALL(gui_lights_table.lights, GuiLightsCfg, .lights = lights, .frame = frame);
    // the sides are a few pixels, which an unsigned int holds
    const GuiButtonsBakeCfg sizes = {.width = (unsigned int)widest,
                                     .height = (unsigned int)tall,
                                     .pad = (unsigned int)pad,
                                     .reach = (reach < 3) ? 3u : (unsigned int)reach,
                                     .tools = tools};
    unsigned char *const span = EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_TAB_BEADS,
                                           .segment = 1u, .bytes = gui_buttons_bake_table.bytes(&sizes));
    if (!EMBED_CALL(gui_buttons_bake_table.bake, GuiButtonsBakeCfg, .buttons = drawn->buttons,
                    .palette = drawn->palette, .width = sizes.width, .height = sizes.height, .pad = sizes.pad,
                    .reach = sizes.reach, .shape = GUI_BEAD_SHAPE_TAB, .radius = (unsigned int)radius, .cells = cells,
                    .tools = tools, .lights = lights, .light_count = light_count, .pixels = (GuiColor *)(void *)span))
    {
        return 0u;
    }
    unsigned int waiting = 0u;
    int baked_now = 0;
    for (unsigned int at = 0u; at < tools; at += 1u)
    {
        const int baked = (drawn->buttons->atlas.baked & (1u << at)) != 0u;
        if (!baked && baked_now)
        {
            waiting += 1u;
            continue;
        }
        baked_now = baked_now || !baked;
        EMBED_CALL(gui_buttons_draw_table.draw, GuiButtonsDrawCfg, .buttons = drawn->buttons, .raster = drawn->raster,
                   .tool = at, .x = placed[at].x, .y = placed[at].y, .time_ms = drawn->time_ms, .clip = frame->tabs);
    }
    return waiting;
}

unsigned int gui_tabs_draw(const GuiTabsGlyphsFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const int one = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    // the row's colors at the share of it shown, the claim fading the row as it lifts it away
    const int share = args->frame->tabs_share;
    const GuiColor line_color = GUI_FRAME_FADED(args->palette->line, share);
    const GuiColor shown_line = GUI_FRAME_FADED(args->palette->tab_line, share);
    const GuiColor box_line = GUI_FRAME_FADED(args->palette->widget_line, share);
    const GuiColor ground = GUI_FRAME_FADED(args->palette->tab_background, share);
    const GuiColor mark = GUI_FRAME_FADED(args->palette->hover, share);
    const GuiColor text = GUI_FRAME_FADED(args->palette->text, share);
    const GuiColor text_2 = GUI_FRAME_FADED(args->palette->text_2, share);
    const GuiColor text_3 = GUI_FRAME_FADED(args->palette->text_3, share);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){args->frame->tabs.x, args->frame->tabs.y + args->frame->tabs.height - one,
                                 args->frame->tabs.width, one},
               .color = line_color);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_TAB_SIZE, .scale = scale);
    const unsigned int count = cli_view_count();
    const int shown = cli_view_shown();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const GuiRect rect =
            EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = glyphs, .frame = args->frame, .at = at);
        // a tab left of the row's view is not drawn, and the tabs after one right of it are not drawn
        if (rect.x >= args->frame->tabs.x + args->frame->tabs.width)
        {
            break;
        }
        const GuiRect met =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = args->frame->tabs);
        if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = met))
        {
            continue;
        }
        // the tabs are fewer than an int counts
        const int displayed = (int)at == shown;
        // Each tab a box, its top corners rounded and its foot open on the row's line, a line one pixel wide up its
        // sides and over its top, and a gap before the next. The shown tab stands on the tab's ground in the tab's
        // line; the others have no ground of their own, the lattice through each under its bead, in the widget's line
        const unsigned int radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = GUI_TAB_RADIUS, .scale = scale);
        const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_SPACE, .scale = scale);
        const GuiRect box = {met.x, met.y, met.width - gap, met.height};
        // a corner is round only where the row's edge does not cut the tab
        const unsigned int top = ((met.x == rect.x) ? GUI_RASTER_CORNER_TOP_LEFT : 0u) |
                                 ((met.x + met.width == rect.x + rect.width) ? GUI_RASTER_CORNER_TOP_RIGHT : 0u);
        if (displayed)
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = box,
                       .radius = radius, .color = ground, .corners = top);
        }
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = box, .radius = radius,
                   .color = displayed ? shown_line : box_line, .corners = top, .stroke = one);
        const int baseline =
            EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_CODE,
                       .size = size, .top = rect.y, .pixel_height = rect.height);
        const int left =
            rect.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_PAD_LEFT, .scale = scale);
        // the mark of changes not saved before the name, in the hover's color, as tabs.js draws its dot
        if (EMBED_CALL(tabs_changed, TabsCtx, .tab = at))
        {
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_CODE, .size = size, .x_f32 = (float)left, .y = baseline,
                             .text = s_changed, .length = sizeof(s_changed) - 1u, .color = mark, .clip = met);
        }
        unsigned long long length = 0ULL;
        const char *const name = EMBED_CALL(gui_tabs.tab_name, GuiTabsCfg, .at = at, .length = &length);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = size,
                         .x_f32 = (float)(left + EMBED_CALL(tabs_changed_width, TabsCtx, .glyphs = glyphs, .tab = at,
                                                            .size = size, .scale = scale)),
                         .y = baseline, .text = name, .length = length, .color = displayed ? text : text_2,
                         .clip = met);
        const GuiRect close = EMBED_CALL(gui_tabs_glyphs.tab_close_rect, GuiTabsGlyphsCfg, .glyphs = glyphs,
                                         .frame = args->frame, .at = at);
        unsigned long long close_length = 0ULL;
        const char *const close_text = EMBED_CALL(tabs_close_text, TabsCtx, .tab = at, .length = &close_length);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = size,
                         .x_f32 = (float)(close.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                               .pixels = GUI_TAB_CLOSE_PAD, .scale = scale)),
                         .y = baseline, .text = close_text, .length = close_length, .color = text_3, .clip = met);
    }
    return EMBED_CALL(tabs_beads_draw, TabsCtx, .drawn = args, .count = count);
}
