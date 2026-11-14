// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bar.c: the bar across the top, on the pane the frame's base positions: the tree's name in the interface face at its right
// before the bar's tools, as the page's bar ends with it; the tools, Preferences, Notifications, Toolchains and
// Definitions, each its icon in a button, before the controls; and the controls at its right end; the menu bar's
// titles are drawn over it by the menu bar part
#include "../arith/arith.h"
#include "block.h"
#include "top_menu.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "lights.h"

// the gap in the page's pixels between the tree's name and the bar's tools, 0.7rem, and the name's max width, 16rem
#define GUI_TOP_MENU_GAP 11
#define GUI_TOP_MENU_NAME_WIDTH 256

const GuiTopMenuTool g_gui_top_menu_tools[GUI_TOP_MENU_TOOLS] = {
    {"preferences", "gear", "Preferences", "Ctrl+,"},
    {"notifications", "bell", "Notifications", ""},
    {"toolchains", "wrench", "Toolchains", ""},
    {"definitions", "m", "Definitions", ""},
};

typedef struct
{
    const GuiTopMenuRasterCfg *drawn;
    unsigned int which;
    GuiRect button;
    GuiRect clip;
    GuiColor color;
} TopMenuCtx;

GuiRect gui_top_menu_tool_rect(const GuiTopMenuCfg *args)
{
    const int side = args->frame->top_menu_tools.height;
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TOP_MENU_TOOL_GAP,
                               .scale = args->frame->scale);
    // the tools' row the button is positioned in: the one passed in, else the frame's; a tool's place is one of four
    const GuiRect tools = (args->tools != NULL) ? *args->tools : args->frame->top_menu_tools;
    const int at = tools.x + ((int)args->which * (side + gap));
    const GuiRect button = {at, tools.y, side, side};
    return button;
}

unsigned int gui_top_menu_tools_hit(const GuiTopMenuCfg *args)
{
    for (unsigned int which = 0u; which < GUI_TOP_MENU_TOOLS; which += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(gui_top_menu.tool_rect, GuiTopMenuCfg, .frame = args->frame, .which = which),
                       .x = args->x, .y = args->y))
        {
            return which;
        }
    }
    return GUI_TOP_MENU_TOOL_NONE;
}

// a tool's character icon, the one character of its glyph's name, in the bold interface face centred in its button
EMBED_INLINE void top_menu_character_draw(const TopMenuCtx *args)
{
    const GuiTopMenuRasterCfg *const drawn = args->drawn;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_TOP_MENU_TOOL_CHARACTER, .scale = drawn->frame->scale);
    const char *const character = g_gui_top_menu_tools[args->which].glyph;
    const float measured =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = drawn->glyphs,
                   .face = GUI_FACE_HEAD_BOLD, .size = size, .text = character, .length = 1ULL);
    const int baseline =
        EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = drawn->glyphs, .face = GUI_FACE_HEAD_BOLD,
                   .size = size, .top = args->button.y, .pixel_height = args->button.height);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = drawn->raster,
                     .glyphs = drawn->glyphs, .face = GUI_FACE_HEAD_BOLD, .size = size,
                     .x_f32 = (float)args->button.x + (((float)args->button.width - measured) / 2.0f), .y = baseline,
                     .text = character, .length = 1ULL, .color = args->color, .clip = args->clip);
}

// a tool's icon by its glyph's name, centred in its button
EMBED_INLINE void top_menu_icon_draw(const TopMenuCtx *args)
{
    const GuiTopMenuRasterCfg *const drawn = args->drawn;
    const unsigned int icon = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_TOP_MENU_TOOL_ICON, .scale = drawn->frame->scale);
    const GuiMask *const mask_bits =
        EMBED_CALL(gui_icons.icon, GuiIconsCfg, .icons = drawn->icons, .name = g_gui_top_menu_tools[args->which].glyph,
                   .size = icon, .stroke = GUI_TOP_MENU_TOOL_STROKE * (float)icon / GUI_ICON_UNITS);
    if (mask_bits == NULL)
    {
        return;
    }
    // the icon's side is far inside what an int represents
    EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = drawn->raster, .mask_bits = mask_bits,
               .x = args->button.x + ((args->button.width - (int)icon) / 2),
               .y = args->button.y + ((args->button.height - (int)icon) / 2), .color = args->color, .clip = args->clip);
}

// The tools' beads arranged, one a tool at its rest place on the top menu's row, where the row is with no claim,
// lit by the window's lights. A tool's frames bake the first time it is drawn
EMBED_INLINE void top_menu_beads_positioned(const TopMenuCtx *args)
{
    const GuiTopMenuRasterCfg *const drawn = args->drawn;
    const GuiFrame *const frame = drawn->frame;
    const int scale = frame->scale;
    const int side = frame->top_menu_tools.height;
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_PAD, .scale = scale);
    const int reach = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_REACH, .scale = scale);
    const int rest_y = ((frame->top_menu.height - side) / 2) - pad;
    GuiRect cells[GUI_TOP_MENU_TOOLS];
    for (unsigned int which = 0u; which < GUI_TOP_MENU_TOOLS; which += 1u)
    {
        const GuiRect button = EMBED_CALL(gui_top_menu.tool_rect, GuiTopMenuCfg, .frame = frame,
                                          .tools = &frame->top_menu_tools_rest, .which = which);
        cells[which] = (GuiRect){button.x - pad, rest_y, 0, 0};
    }
    GuiLight lights[GUI_LIGHTS_MAX];
    const unsigned int light_count = EMBED_CALL(gui_lights_table.lights, GuiLightsCfg, .lights = lights, .frame = frame);
    // the sides are a few pixels, which an unsigned int holds; the atlas's pixels over the second segment of the
    // block's bead part
    const GuiButtonsBakeCfg sizes = {.width = (unsigned int)side,
                                     .height = (unsigned int)side,
                                     .pad = (unsigned int)pad,
                                     .reach = (reach < 3) ? 3u : (unsigned int)reach,
                                     .tools = GUI_TOP_MENU_TOOLS};
    unsigned char *const span = EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_BEAD, .segment = 1u,
                                           .bytes = gui_buttons_bake_table.bytes(&sizes));
    (void)EMBED_CALL(gui_buttons_bake_table.bake, GuiButtonsBakeCfg, .buttons = drawn->buttons, .palette = drawn->palette,
                     .width = sizes.width, .height = sizes.height, .pad = sizes.pad, .reach = sizes.reach, .cells = cells,
                     .tools = sizes.tools, .lights = lights, .light_count = light_count,
                     .pixels = (GuiColor *)(void *)span);
}

// the bar's tools drawn: each its icon on the glass, the pointed one in the bright color, the notifications tool
// with a dot at its top right corner while notices wait, and each tool's bead of water over its icon
EMBED_INLINE void top_menu_tools_draw(const TopMenuCtx *args)
{
    const GuiTopMenuRasterCfg *const drawn = args->drawn;
    const int scale = drawn->frame->scale;
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_PAD, .scale = scale);
    const int mark = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TOP_MENU_TOOL_MARK, .scale = scale);
    const int mark_in =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TOP_MENU_TOOL_MARK_IN, .scale = scale);
    for (unsigned int which = 0u; which < GUI_TOP_MENU_TOOLS; which += 1u)
    {
        const GuiRect button =
            EMBED_CALL(gui_top_menu.tool_rect, GuiTopMenuCfg, .frame = drawn->frame, .which = which);
        const GuiRect on =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = button, .other = args->clip);
        if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
        {
            continue;
        }
        const int pointed = drawn->pointed == which;
        const GuiColor color = pointed ? drawn->palette->top_menu_bright : drawn->palette->status_text;
        if (g_gui_top_menu_tools[which].glyph[1] == '\0')
        {
            EMBED_CALL(top_menu_character_draw, TopMenuCtx, .drawn = drawn, .which = which, .button = button, .clip = on,
                       .color = color);
        }
        else
        {
            EMBED_CALL(top_menu_icon_draw, TopMenuCtx, .drawn = drawn, .which = which, .button = button, .clip = on,
                       .color = color);
        }
        if ((which == GUI_TOP_MENU_TOOL_NOTIFICATIONS) && drawn->unread)
        {
            const GuiRect dot = {button.x + button.width - mark_in - mark, button.y + mark_in, mark, mark};
            // the mark's side is a few pixels, so its half is its radius
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = drawn->raster, .rect = dot,
                       .radius = (unsigned int)mark / 2u, .color = drawn->palette->signal);
        }
        EMBED_CALL(gui_buttons_draw_table.draw, GuiButtonsDrawCfg, .buttons = drawn->buttons, .raster = drawn->raster, .tool = which,
                   .x = button.x - pad, .y = button.y - pad, .time_ms = drawn->time_ms, .clip = args->clip);
    }
}

void gui_top_menu_draw(const GuiTopMenuRasterCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    // the bar's pane is the slat its edge shows, whole outside the claim, positioned by the frame's base
    const GuiRect pane = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->top_menu_band,
                                    .other = args->frame->top_menu);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = args->frame->scale);
    const int baseline =
        EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_HEAD,
                   .size = size, .top = args->frame->top_menu.y, .pixel_height = args->frame->top_menu.height);
    unsigned long long length = 0ULL;
    length = cellul.len(&(CatenaFinitaCfg){.src = args->title, .cap = GUI_LABEL_MAX});
    const int max = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TOP_MENU_NAME_WIDTH,
                                .scale = args->frame->scale);
    const float measured = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                      .face = GUI_FACE_HEAD, .size = size, .text = args->title, .length = length);
    const int pixel_width = (measured > (float)max) ? max : (int)arith_round_f32(measured);
    const int right = args->frame->top_menu_tools.x - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = GUI_TOP_MENU_GAP, .scale = args->frame->scale);
    const GuiRect clip = {right - pixel_width, args->frame->top_menu.y, pixel_width, args->frame->top_menu.height};
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(right - pixel_width), .y = baseline,
                     .text = args->title, .length = length, .color = args->palette->top_menu_text,
                     .clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = clip, .other = pane));
    EMBED_CALL(top_menu_beads_positioned, TopMenuCtx, .drawn = args);
    EMBED_CALL(top_menu_tools_draw, TopMenuCtx, .drawn = args, .clip = pane);
}
