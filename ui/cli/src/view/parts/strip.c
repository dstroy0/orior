// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// strip.c: the tool strip and the frame's gap down the right, each tinted with the ink; the top group's buttons stand
// down from the strip's head and the foot group's up from its foot, each as the page's strip lays them out
#include "strip.h"

#include "../../command_line/cli_internal.h"
#include "pane.h"

// the tools, the top group then the foot group
const CliTool g_cli_tools[] = {
    {"explorer", "folder", "Explorer", "Ctrl+Shift+E", CLI_TOOL_TOP, 0, 1},
    {"search", "search", "Search", "Ctrl+Shift+F", CLI_TOOL_TOP, 0, 1},
    {"structure", "structure", "Structure", "", CLI_TOOL_TOP, 0, 1},
    {"commit", "commit", "Commit", "", CLI_TOOL_TOP, 0, 1},
    {"more", "more", "More Tool Windows", "", CLI_TOOL_TOP, 1, 0},
    {"run", "run", "Run", "Ctrl+Shift+D", CLI_TOOL_FOOTER, 0, 0},
    {"debug", "debug", "Debug", "Alt+5", CLI_TOOL_FOOTER, 0, 0},
    {"terminal", "terminal", "Terminal", "Ctrl+`", CLI_TOOL_FOOTER, 0, 0},
    {"profile", "profile", "Profile", "", CLI_TOOL_FOOTER, 0, 0},
    {"containers", "containers", "Containers", "", CLI_TOOL_FOOTER, 0, 0},
    {"database", "database", "Database", "", CLI_TOOL_FOOTER, 0, 0},
    {"comms", "comms", "Comms", "", CLI_TOOL_FOOTER, 0, 0},
    {"problems", "problems", "Problems", "", CLI_TOOL_FOOTER, 0, 1},
    {"tests", "tests", "Tests", "", CLI_TOOL_FOOTER, 0, 1},
    {"git", "git", "Git", "", CLI_TOOL_FOOTER, 0, 1},
    {"runs", "runs", "Runs", "", CLI_TOOL_FOOTER, 0, 1},
};
const unsigned int g_cli_tools_count = sizeof(g_cli_tools) / sizeof(g_cli_tools[0]);
_Static_assert(sizeof(g_cli_tools) / sizeof(g_cli_tools[0]) <= GUI_PANE_TOOLS, "the side has a pane state a tool");

void gui_strip_init(const GuiStripInitCfg *args)
{
    args->strip->pointed = GUI_STRIP_NONE;
    args->strip->displayed = GUI_STRIP_NONE;
    args->strip->foot = GUI_STRIP_NONE;
    args->strip->collapsed = 0;
    args->strip->problems_marked = 0;
    args->strip->runs_live = 0u;
    args->strip->runs_panicked = 0u;
}

GuiRect gui_strip_button(const GuiStripCfg *args)
{
    const int scale = args->frame->scale;
    const int side = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_BUTTON, .scale = scale);
    const int pitch =
        side + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_BUTTON_GAP, .scale = scale);
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_PAD, .scale = scale);
    const int x = args->frame->strip.x + ((args->frame->strip.width - side) / 2);
    const CliTool *const tool = &g_cli_tools[(args->at < g_cli_tools_count) ? args->at : 0u];
    if (tool->group == CLI_TOOL_TOP)
    {
        unsigned int before = 0u;
        for (unsigned int each = 0u; each < args->at; each += 1u)
        {
            before += (g_cli_tools[each].group == CLI_TOOL_TOP) ? 1u : 0u;
        }
        // the tools before are far fewer than an int counts
        return (GuiRect){x, args->frame->strip.y + pad + ((int)before * pitch), side, side};
    }
    unsigned int after = 0u;
    for (unsigned int each = args->at + 1u; each < g_cli_tools_count; each += 1u)
    {
        after += (g_cli_tools[each].group == CLI_TOOL_FOOTER) ? 1u : 0u;
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
    for (unsigned int each = 0u; each < g_cli_tools_count; each += 1u)
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

void gui_strip_draw(const GuiStripFrameRasterCfg *args)
{
    GuiIcons *icons = args->icons;
    // the strip's pane is the slat its edge shows, whole outside the claim, laid by the frame's base; the tools stand
    // in the slat
    const GuiRect pane = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->strip_band,
                                    .other = args->frame->strip);
    const int scale = args->frame->scale;
    const unsigned int icon =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_ICON, .scale = scale);
    const float stroke = GUI_STRIP_STROKE * (float)icon / GUI_ICON_UNITS;
    const unsigned int radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                         .pixels = GUI_STRIP_BUTTON_RADIUS, .scale = scale);
    for (unsigned int at = 0u; at < g_cli_tools_count; at += 1u)
    {
        const GuiRect button = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = args->frame, .at = at);
        const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = button, .other = pane);
        if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
        {
            continue;
        }
        // the tools are fewer than an int counts
        const int displayed = ((args->strip_read->displayed == (int)at) && !args->strip_read->collapsed) ||
                              (args->strip_read->foot == (int)at) ||
                              (args->strip_read->terminal_open && (at == args->strip_read->terminal_at));
        const int pointed = args->strip_read->pointed == (int)at;
        if (pointed)
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster, .rect = on,
                       .radius = radius, .color = args->palette->bar_title_hover);
        }
        else if (displayed)
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster, .rect = on,
                       .radius = radius, .color = args->palette->strip_on);
        }
        const GuiMask *const mask_bits = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = icons,
                                                    .name = g_cli_tools[at].glyph, .size = icon, .stroke = stroke);
        if (mask_bits != NULL)
        {
            GuiColor color;
            if (pointed)
            {
                color = args->palette->bar_bright;
            }
            else if (displayed)
            {
                color = args->palette->signal;
            }
            else
            {
                color = args->palette->status_text;
            }
            // the icon's side is far inside what an int represents
            EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = args->raster,
                       .mask_bits = mask_bits, .x = button.x + ((button.width - (int)icon) / 2),
                       .y = button.y + ((button.height - (int)icon) / 2), .color = color, .clip = on);
        }
        // a tool's mark, a dot at the button's top right corner: the Problems tool's in the error's color while it is
        // marked, the Runs tool's in signal while a run is live and in the error's color while a live run has panicked
        int marked = 0;
        GuiColor mark_color = args->palette->error;
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[at].name, .other = "problems", .cap = 32ULL}))
        {
            marked = args->strip_read->problems_marked;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[at].name, .other = "runs", .cap = 32ULL}))
        {
            marked = args->strip_read->runs_live != 0u;
            mark_color = (args->strip_read->runs_panicked != 0u) ? args->palette->error : args->palette->signal;
        }
        if (marked)
        {
            const int mark =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_MARK, .scale = scale);
            const int in =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRIP_MARK_IN, .scale = scale);
            const GuiRect dot = {button.x + button.width - in - mark, button.y + in, mark, mark};
            // the dot's corners round by half its side, the dot a circle
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = dot, .other = on),
                       .radius = (unsigned int)(mark / 2), .color = mark_color);
        }
    }
}
