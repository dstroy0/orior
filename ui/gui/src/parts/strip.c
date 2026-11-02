// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// strip.c: the tool strip and the frame's gap down the right, each tinted with the ink; the top group's buttons stand
// down from the strip's head and the foot group's up from its foot, each as the page's strip lays them out
#include "strip.h"

#include "menubar.h"

// the tools, the top group then the foot group, as the page's strip lists them
static const GuiTool s_tools[] = {
    {"explorer", "folder", "Explorer", "Ctrl+Shift+E", GUI_STRIP_TOP, 0, 1},
    {"search", "search", "Search", "Ctrl+Shift+F", GUI_STRIP_TOP, 0, 1},
    {"structure", "structure", "Structure", "", GUI_STRIP_TOP, 0, 1},
    {"commit", "commit", "Commit", "", GUI_STRIP_TOP, 0, 1},
    {"more", "more", "More Tool Windows", "", GUI_STRIP_TOP, 1, 0},
    {"run", "run", "Run", "Ctrl+Shift+D", GUI_STRIP_FOOT, 0, 0},
    {"debug", "debug", "Debug", "Alt+5", GUI_STRIP_FOOT, 0, 0},
    {"terminal", "terminal", "Terminal", "Ctrl+`", GUI_STRIP_FOOT, 0, 0},
    {"profile", "profile", "Profile", "", GUI_STRIP_FOOT, 0, 0},
    {"containers", "containers", "Containers", "", GUI_STRIP_FOOT, 0, 0},
    {"database", "database", "Database", "", GUI_STRIP_FOOT, 0, 0},
    {"comms", "comms", "Comms", "", GUI_STRIP_FOOT, 0, 0},
    {"problems", "problems", "Problems", "", GUI_STRIP_FOOT, 0, 1},
    {"tests", "tests", "Tests", "", GUI_STRIP_FOOT, 0, 1},
    {"git", "git", "Git", "", GUI_STRIP_FOOT, 0, 1},
    {"runs", "runs", "Runs", "", GUI_STRIP_FOOT, 0, 1},
};
#define STRIP_TOOLS (sizeof(s_tools) / sizeof(s_tools[0]))

void gui_strip_init(const GuiStripNameCfg *args)
{
    args->strip->pointed = GUI_STRIP_NONE;
    args->strip->shown = GUI_STRIP_NONE;
}

unsigned int gui_strip_tools(void)
{
    return STRIP_TOOLS;
}

const GuiTool *gui_strip_tool(const GuiStripCfg *args)
{
    return &s_tools[(args->at < STRIP_TOOLS) ? args->at : 0u];
}

GuiRect gui_strip_button(const GuiStripCfg *args)
{
    const int scale = args->frame->scale;
    const int side = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_BUTTON, .scale = scale);
    const int pitch =
        side + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_BUTTON_GAP, .scale = scale);
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_PAD, .scale = scale);
    const int x = args->frame->strip.x + ((args->frame->strip.width - side) / 2);
    const GuiTool *const tool = gui_strip.tool(args);
    if (tool->group == GUI_STRIP_TOP)
    {
        unsigned int before = 0u;
        for (unsigned int each = 0u; each < args->at; each += 1u)
        {
            before += (s_tools[each].group == GUI_STRIP_TOP) ? 1u : 0u;
        }
        // the tools before are far fewer than an int counts
        return (GuiRect){x, args->frame->strip.y + pad + ((int)before * pitch), side, side};
    }
    unsigned int after = 0u;
    for (unsigned int each = args->at + 1u; each < STRIP_TOOLS; each += 1u)
    {
        after += (s_tools[each].group == GUI_STRIP_FOOT) ? 1u : 0u;
    }
    // the tools after are far fewer than an int counts
    return (GuiRect){x, args->frame->strip.y + args->frame->strip.height - pad - side - ((int)after * pitch), side,
                     side};
}

int gui_strip_hit(const GuiStripFrameCfg *args)
{
    unsigned int *at = args->at_pointer;
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->strip, .x = args->x,
                    .y = args->y))
    {
        return 0;
    }
    for (unsigned int each = 0u; each < STRIP_TOOLS; each += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = args->frame, .at = each),
                       .x = args->x, .y = args->y))
        {
            *at = each;
            return 1;
        }
    }
    return 0;
}

int gui_strip_move(const GuiStripFrameXCfg *args)
{
    unsigned int at = 0u;
    // the tools are fewer than an int counts
    const int pointed = EMBED_CALL(gui_strip_frame.hit, GuiStripFrameCfg, .frame = args->frame, .x = args->x,
                                   .y = args->y, .at_pointer = &at)
                            ? (int)at
                            : GUI_STRIP_NONE;
    if (pointed == args->strip->pointed)
    {
        return 0;
    }
    args->strip->pointed = pointed;
    return 1;
}

void gui_strip_toggle(const GuiStripAtCfg *args)
{
    if ((args->at >= STRIP_TOOLS) || s_tools[args->at].menu)
    {
        return;
    }
    // the tools are fewer than an int counts
    args->strip->shown = (args->strip->shown == (int)args->at) ? GUI_STRIP_NONE : (int)args->at;
}

int gui_strip_press(const GuiStripFrameXCfg *args)
{
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->strip, .x = args->x,
                    .y = args->y))
    {
        return 0;
    }
    unsigned int at = 0u;
    if (EMBED_CALL(gui_strip_frame.hit, GuiStripFrameCfg, .frame = args->frame, .x = args->x, .y = args->y,
                   .at_pointer = &at))
    {
        EMBED_CALL(gui_strip_at.toggle, GuiStripAtCfg, .strip = args->strip, .at = at);
    }
    return 1;
}

int gui_strip_key(const GuiStripNameCfg *args)
{
    const char *name = args->name;
    for (unsigned int at = 0u; at < STRIP_TOOLS; at += 1u)
    {
        const char *const keys = s_tools[at].keys;
        if ((keys[0] != '\0') &&
            EMBED_CALL(gui_menubar_name.keys_name, GuiMenubarNameCfg, .keys = keys,
                       .keys_length = cellul.len(&(CatenaFinitaCfg){.src = keys, .cap = 32ull}), .name = name))
        {
            EMBED_CALL(gui_strip_at.toggle, GuiStripAtCfg, .strip = args->strip, .at = at);
            return 1;
        }
    }
    return 0;
}

void gui_strip_draw(const GuiStripFrameRasterCfg *args)
{
    GuiIcons *icons = args->icons;
    // the strip's pane is the slat its edge shows, whole outside the claim, laid by the frame's base; the tools stand
    // in the slat
    const GuiRect pane = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->strip_slat,
                                    .other = args->frame->strip);
    const int scale = args->frame->scale;
    const unsigned int icon =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_ICON, .scale = scale);
    const float stroke = GUI_STRIP_STROKE * (float)icon / GUI_ICON_UNITS;
    const unsigned int radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                         .pixels = GUI_STRIP_BUTTON_RADIUS, .scale = scale);
    for (unsigned int at = 0u; at < STRIP_TOOLS; at += 1u)
    {
        const GuiRect button = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = args->frame, .at = at);
        const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = button, .other = pane);
        if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
        {
            continue;
        }
        // the tools are fewer than an int counts
        const int shown = args->strip_read->shown == (int)at;
        const int pointed = args->strip_read->pointed == (int)at;
        if (pointed)
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster, .rect = on,
                       .radius = radius, .color = args->palette->bar_title_hover);
        }
        else if (shown)
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster, .rect = on,
                       .radius = radius, .color = args->palette->strip_on);
        }
        const GuiMask *const mask = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = icons, .name = s_tools[at].glyph,
                                               .size = icon, .stroke = stroke);
        if (mask != NULL)
        {
            const GuiColor color =
                pointed ? args->palette->bar_bright : (shown ? args->palette->signal : args->palette->status_text);
            // the icon's side is far inside what an int represents
            EMBED_CALL(gui_raster_xy_color.mask, GuiRasterXYColorCfg, .raster = args->raster, .mask = mask,
                       .x = button.x + ((button.width - (int)icon) / 2),
                       .y = button.y + ((button.height - (int)icon) / 2), .color = color, .clip = on);
        }
        // the Problems tool's mark, a dot in the error's color at the button's top right corner
        if (args->strip_read->problems_marked &&
            cellul.eq(&(CatenaFinitaCfg){.src = s_tools[at].name, .other = "problems", .cap = 32ull}))
        {
            const int mark =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_MARK, .scale = scale);
            const int in =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_MARK_IN, .scale = scale);
            const GuiRect dot = {button.x + button.width - in - mark, button.y + in, mark, mark};
            // the dot's corners round by half its side, the dot a circle
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = dot, .other = on),
                       .radius = (unsigned int)(mark / 2), .color = args->palette->error);
        }
    }
}
