// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs.c: the tabs row. Each tab is as wide as its padding, the mark of changes not saved where it has them, its name
// and its close mark. The tabs are in one side_menu, and the row shows the part of the side_menu from the frame's
// `tabs_left`. The row's edges cut the tabs that cross them
#include "tabs.h"
#include "../../../includes/arith/arith.h"
#include "../../../includes/window/block.h"
#include "../../../includes/window/lights.h"

// the close mark's text, and its text while the close waits on a second
static const char s_close[] = "\xc3\x97";
static const char s_close_waiting[] = "\xc3\x97?";

const char *gui_tab_name(const GuiTabsCfg *args)
{
    // the tab's path copied into the tabs' own buffer, its name inside it, which the caller reads before the next tab
    static char s_tab_path[CLI_PATH_MAX_LEN];
    const char *const name = cli_view_name_at(&(CliViewAtCfg){.at = args->at, .path_out = s_tab_path});
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
    GuiRaster *raster;
    GuiRect box;
    GuiRect clip;
    unsigned int radius;
    unsigned int corners;
    GuiColor color;
} TabsCtx;

// the displayed tab's tint over its box, cut to `clip`: `color` at its own alpha along the box's edges, fading to
// nothing at the box's center, each pixel's alpha the greater of its shares across and down from the center; a pixel
// outside a rounded top corner is left
EMBED_INLINE void tabs_tint_draw(const TabsCtx *args)
{
    GuiRaster *const raster = args->raster;
    const GuiRect box = args->box;
    const GuiRect met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->clip,
                                   .other = (GuiRect){0, 0, (int)raster->width, (int)raster->height});
    if ((box.width <= 0) || (box.height <= 0) || EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = met))
    {
        return;
    }
    const float half_width = (float)box.width / 2.0f;
    const float half_height = (float)box.height / 2.0f;
    const float center_x = (float)box.x + half_width;
    const float center_y = (float)box.y + half_height;
    const float radius = (float)args->radius;
    const unsigned int edge_alpha = GUI_ALPHA_OF(args->color);
    for (int y = met.y; y < met.y + met.height; y += 1)
    {
        GuiColor *const row = raster->pixels + ((unsigned long long)(unsigned int)y * raster->width);
        const float down = ((float)y + 0.5f) - center_y;
        for (int x = met.x; x < met.x + met.width; x += 1)
        {
            const float across = ((float)x + 0.5f) - center_x;
            // a top corner rounded: the pixel is left where it lies past the corner's arc
            const int at_left = across < -(half_width - radius);
            const int at_right = across > half_width - radius;
            const int at_top = down < -(half_height - radius);
            if (at_top && ((at_left && ((args->corners & GUI_RASTER_CORNER_TOP_LEFT) != 0u)) ||
                           (at_right && ((args->corners & GUI_RASTER_CORNER_TOP_RIGHT) != 0u))))
            {
                const float corner_x = arith_abs_f32(across) - (half_width - radius);
                const float corner_y = -down - (half_height - radius);
                if ((corner_x * corner_x) + (corner_y * corner_y) > radius * radius)
                {
                    continue;
                }
            }
            const float share_x = arith_abs_f32(across) / half_width;
            const float share_y = arith_abs_f32(down) / half_height;
            const float share = (share_x > share_y) ? share_x : share_y;
            // the share is within 1, so the alpha is within the color's own, a byte
            const unsigned int alpha = (unsigned int)(((float)edge_alpha * share) + 0.5f);
            row[(unsigned int)x] =
                EMBED_CALL(gui_color.over, GuiRasterCfg, .color = (args->color & 0x00ffffffu) | (alpha << 24u),
                           .background_color = row[(unsigned int)x]);
        }
    }
}

// the close mark the tab `tab` shows, ×? while its close waits on a second, its length into `length`
EMBED_INLINE const char *tabs_close_text(const TabsCtx *args)
{
    const int closing = cli_view_closing(&(CliViewAtCfg){.at = args->tab});
    *args->length = closing ? (sizeof(s_close_waiting) - 1u) : (sizeof(s_close) - 1u);
    return closing ? s_close_waiting : s_close;
}

// whether the tab `tab`'s text has changes not saved
EMBED_INLINE int tabs_changed(const TabsCtx *args)
{
    char key[CLI_NAME_MAX_LEN];
    return cli_view_key(&(CliViewAtCfg){.at = args->tab, .key_out = key}) &&
           EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = key);
}

// the width the dot of changes not saved and the gap after it take before the tab `tab`'s name, 0 where it has none
EMBED_INLINE int tabs_changed_width(const TabsCtx *args)
{
    if (!tabs_changed(args))
    {
        return 0;
    }
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_DOT, .scale = args->scale) +
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
    // the tabs are clear of the work's lines: room above them, and room before the first
    const int clear =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_CLEAR, .scale = args->frame->scale);
    // the side_menu of tabs is slid left by the pixels the row holds left of its view
    int x = args->frame->tabs.x + clear - args->frame->tabs_left;
    for (unsigned int each = 0u; (each < args->at) && (each < count); each += 1u)
    {
        x += EMBED_CALL(tabs_width, TabsCtx, .glyphs = args->glyphs, .tab = each, .size = size,
                        .scale = args->frame->scale);
    }
    const int pixel_width = (args->at < count) ? EMBED_CALL(tabs_width, TabsCtx, .glyphs = args->glyphs,
                                                            .tab = args->at, .size = size, .scale = args->frame->scale)
                                               : 0;
    // a tab is whole in the row, which the claim lifts out of the work's head
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

// The beads over the first GUI_TABS_BEADS tabs of `count`, drawn as their presses are at `time_ms`: the atlas
// positioned out for the widest tab in the tab's shape, each tab's cell at its place in the row's whole side_menu with
// the row at rest, lit by the window's lights, over the second segment of the block's tab beads part. A cell at the
// row's rest keeps its bake while the claim slides the row, the bead drawn where the tab is: 0, as no tab waits to bake
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
    // a tab at the row's rest: the row a gap under the work's inside top, the tab clear of the row's top and its line
    const int clear = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_CLEAR, .scale = scale);
    const int rest_top = frame->inside.y +
                         EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TABS_GAP, .scale = scale) +
                         clear;
    const int tall = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TABS_HEIGHT, .scale = scale) -
                     clear - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    GuiRect cells[GUI_TABS_BEADS];
    GuiRect placed[GUI_TABS_BEADS];
    int widest = 0;
    for (unsigned int at = 0u; at < tools; at += 1u)
    {
        const GuiRect rect =
            EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = drawn->glyphs, .frame = frame, .at = at);
        cells[at] = (GuiRect){rect.x + frame->tabs_left - pad, rest_top - pad, rect.width - gap, tall};
        placed[at] = (GuiRect){rect.x - pad, rect.y - pad, rect.width - gap, tall};
        widest = (cells[at].width > widest) ? cells[at].width : widest;
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
    for (unsigned int at = 0u; at < tools; at += 1u)
    {
        // the beads fade with the row's share as the claim lifts it away; a share is within GUI_FIXED_ONE
        EMBED_CALL(gui_buttons_draw_table.draw, GuiButtonsDrawCfg, .buttons = drawn->buttons, .raster = drawn->raster,
                   .tool = at, .x = placed[at].x, .y = placed[at].y, .time_ms = drawn->time_ms, .clip = frame->tabs,
                   .fade = (unsigned int)(255 - ((255 * frame->tabs_share) / GUI_FIXED_ONE)));
    }
    return 0u;
}

unsigned int gui_tabs_draw(const GuiTabsGlyphsFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const int one = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    // the row's colors at the share of it displayed, the claim fading the row as it lifts it away; the row away has
    // nothing to draw, as its beads, cut to a row of no height, would draw whole
    const int share = args->frame->tabs_share;
    if (share <= 0)
    {
        return 0u;
    }
    const GuiColor line_color = GUI_FRAME_FADED(args->palette->line, share);
    const GuiColor displayed_line = GUI_FRAME_FADED(args->palette->tab_line, share);
    const GuiColor box_line = GUI_FRAME_FADED(args->palette->widget_line, share);
    const GuiColor ground = GUI_FRAME_FADED(args->palette->tab_background, share);
    const GuiColor ground_idle = GUI_FRAME_FADED(args->palette->tab_inactive, share);
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
    const int displayed = cli_view_displayed();
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
        const int shown = (int)at == displayed;
        // Each tab a box, its top corners rounded and its foot open on the row's line, a line one pixel wide up its
        // sides and over its top, and a gap before the next. Every tab is clear glass the lattice shows through under
        // its bead; the displayed tab carries its tint along its edges, fading to nothing at its center, in the tab's
        // line; the others in the widget's line
        const unsigned int radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = GUI_TAB_RADIUS, .scale = scale);
        const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_SPACE, .scale = scale);
        const GuiRect box = {met.x, met.y, met.width - gap, met.height};
        // a corner is round only where the row's edge does not cut the tab
        const unsigned int top = ((met.x == rect.x) ? GUI_RASTER_CORNER_TOP_LEFT : 0u) |
                                 ((met.x + met.width == rect.x + rect.width) ? GUI_RASTER_CORNER_TOP_RIGHT : 0u);
        if (shown)
        {
            EMBED_CALL(tabs_tint_draw, TabsCtx, .raster = raster, .box = box, .clip = met, .radius = radius,
                       .corners = top, .color = ground);
        }
        else
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = box,
                       .radius = radius, .color = ground_idle, .corners = top);
        }
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = box, .radius = radius,
                   .color = shown ? displayed_line : box_line, .corners = top, .stroke = one);
        const int baseline =
            EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_CODE,
                       .size = size, .top = rect.y, .pixel_height = rect.height);
        const int left =
            rect.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_PAD_LEFT, .scale = scale);
        // the dot of changes not saved before the name, in the hover's color, drawn as a disc at the tab's middle
        if (EMBED_CALL(tabs_changed, TabsCtx, .tab = at))
        {
            const int dot =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_DOT, .scale = scale);
            EMBED_CALL(gui_raster_color.dot, GuiRasterColorCfg, .raster = raster, .color = mark,
                       .x_f32 = (float)left + ((float)dot / 2.0f), .y_f32 = (float)rect.y + ((float)rect.height / 2.0f),
                       .radius_f32 = (float)dot / 2.0f, .clip = met);
        }
        unsigned long long length = 0ULL;
        const char *const name = EMBED_CALL(gui_tabs.tab_name, GuiTabsCfg, .at = at, .length = &length);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = size,
                         .x_f32 = (float)(left + EMBED_CALL(tabs_changed_width, TabsCtx, .glyphs = glyphs, .tab = at,
                                                            .size = size, .scale = scale)),
                         .y = baseline, .text = name, .length = length, .color = shown ? text : text_2, .clip = met);
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
