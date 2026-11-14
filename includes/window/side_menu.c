// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// side_menu.c: the tool side_menu and the frame's gap down the right, each tinted with the ink; the top group's buttons stand
// down from the side_menu's head and the foot group's up from its foot, each as the page's side_menu lays them out
#include "side_menu.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "lights.h"

#include "block.h"

typedef struct
{
    GuiSideMenu *side_menu;
    const GuiFrame *frame;
    const GuiRect *strip;
    unsigned long long time_ms;
    unsigned int at;
} SideMenuCtx;

// the tools, the top group then the foot group
const GuiTool g_gui_tools[] = {
    {"explorer", "folder", "Explorer", "Ctrl+Shift+E", GUI_TOOL_TOP, 0, 1},
    {"search", "search", "Search", "Ctrl+Shift+F", GUI_TOOL_TOP, 0, 1},
    {"structure", "structure", "Structure", "", GUI_TOOL_TOP, 0, 1},
    {"commit", "commit", "Commit", "", GUI_TOOL_TOP, 0, 1},
    {"more", "more", "More Tool Windows", "", GUI_TOOL_TOP, 1, 0},
    {"run", "run", "Run", "Ctrl+Shift+D", GUI_TOOL_FOOTER, 0, 0},
    {"debug", "debug", "Debug", "Alt+5", GUI_TOOL_FOOTER, 0, 0},
    {"terminal", "terminal", "Terminal", "Ctrl+`", GUI_TOOL_FOOTER, 0, 0},
    {"profile", "profile", "Profile", "", GUI_TOOL_FOOTER, 0, 0},
    {"containers", "containers", "Containers", "", GUI_TOOL_FOOTER, 0, 0},
    {"database", "database", "Database", "", GUI_TOOL_FOOTER, 0, 0},
    {"comms", "comms", "Comms", "", GUI_TOOL_FOOTER, 0, 0},
    {"problems", "problems", "Problems", "", GUI_TOOL_FOOTER, 0, 1},
    {"tests", "tests", "Tests", "", GUI_TOOL_FOOTER, 0, 1},
    {"git", "git", "Git", "", GUI_TOOL_FOOTER, 0, 1},
    {"runs", "runs", "Runs", "", GUI_TOOL_FOOTER, 0, 1},
};
const unsigned int g_gui_tools_count = sizeof(g_gui_tools) / sizeof(g_gui_tools[0]);
_Static_assert(sizeof(g_gui_tools) / sizeof(g_gui_tools[0]) <= GUI_BEAD_TOOLS_MAX, "the bead's atlas holds frames a tool");
_Static_assert(sizeof(g_gui_tools) / sizeof(g_gui_tools[0]) <= GUI_SIDE_MENU_TOOLS_MAX, "the side menu holds its tools");

void gui_side_menu_init(const GuiSideMenuInitCfg *args)
{
    args->side_menu->pointed = GUI_SIDE_MENU_NONE;
    args->side_menu->displayed = GUI_SIDE_MENU_NONE;
    args->side_menu->foot = GUI_SIDE_MENU_NONE;
    args->side_menu->collapsed = 0;
    args->side_menu->problems_marked = 0;
    args->side_menu->runs_live = 0u;
    args->side_menu->runs_panicked = 0u;
    EMBED_CALL(gui_buttons.init, GuiButtonsCfg, .buttons = &args->side_menu->buttons);
}

// the bead's cell top left for the tool `at`: the button's, the margin out
EMBED_INLINE GuiRect side_menu_bead_cell(const SideMenuCtx *args)
{
    const GuiRect button =
        EMBED_CALL(gui_side_menu.button, GuiSideMenuCfg, .frame = args->frame, .strip = args->strip, .at = args->at);
    const int pad =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_PAD, .scale = args->frame->scale);
    return (GuiRect){button.x - pad, button.y - pad, button.width, button.height};
}

GuiRect gui_side_menu_button(const GuiSideMenuCfg *args)
{
    const int scale = args->frame->scale;
    const int side = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDE_MENU_BUTTON, .scale = scale);
    const int pitch =
        side + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDE_MENU_BUTTON_GAP, .scale = scale);
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDE_MENU_PAD, .scale = scale);
    // the strip the button is laid in: the one handed in, else the frame's
    const GuiRect strip = (args->strip != NULL) ? *args->strip : args->frame->side_menu;
    const int x = strip.x + ((strip.width - side) / 2);
    const GuiTool *const tool = &g_gui_tools[(args->at < g_gui_tools_count) ? args->at : 0u];
    if (tool->group == GUI_TOOL_TOP)
    {
        unsigned int before = 0u;
        for (unsigned int each = 0u; each < args->at; each += 1u)
        {
            before += (g_gui_tools[each].group == GUI_TOOL_TOP) ? 1u : 0u;
        }
        // the tools before are far fewer than an int counts
        return (GuiRect){x, strip.y + pad + ((int)before * pitch), side, side};
    }
    unsigned int after = 0u;
    for (unsigned int each = args->at + 1u; each < g_gui_tools_count; each += 1u)
    {
        after += (g_gui_tools[each].group == GUI_TOOL_FOOTER) ? 1u : 0u;
    }
    // the tools after are far fewer than an int counts
    return (GuiRect){x, strip.y + strip.height - pad - side - ((int)after * pitch), side, side};
}

int gui_side_menu_hit(const GuiSideMenuFrameCfg *args)
{
    unsigned int *at = args->at_pointer;
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->side_menu, .x = args->x,
                    .y = args->y))
    {
        return 0;
    }
    for (unsigned int each = 0u; each < g_gui_tools_count; each += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(gui_side_menu.button, GuiSideMenuCfg, .frame = args->frame, .at = each),
                       .x = args->x, .y = args->y))
        {
            *at = each;
            return 1;
        }
    }
    return 0;
}

int gui_side_menu_move(const GuiSideMenuFrameXCfg *args)
{
    unsigned int at = 0u;
    // the tools are fewer than an int counts
    const int pointed = EMBED_CALL(gui_side_menu_frame.hit, GuiSideMenuFrameCfg, .frame = args->frame, .x = args->x,
                                   .y = args->y, .at_pointer = &at)
                            ? (int)at
                            : GUI_SIDE_MENU_NONE;
    if (pointed == args->side_menu->pointed)
    {
        return 0;
    }
    args->side_menu->pointed = pointed;
    return 1;
}

void gui_side_menu_leave(const GuiSideMenuFrameXCfg *args)
{
    args->side_menu->pointed = GUI_SIDE_MENU_NONE;
}

void gui_side_menu_press(const GuiSideMenuFrameXCfg *args)
{
    if ((args->at < 0) || ((unsigned int)args->at >= g_gui_tools_count))
    {
        return;
    }
    // the tool's place is not negative
    EMBED_CALL(gui_buttons_move.press, GuiButtonsMoveCfg, .buttons = &args->side_menu->buttons, .tool = (unsigned int)args->at,
               .time_ms = args->time_ms);
}

void gui_side_menu_release(const GuiSideMenuFrameXCfg *args)
{
    EMBED_CALL(gui_buttons_move.release, GuiButtonsMoveCfg, .buttons = &args->side_menu->buttons, .time_ms = args->time_ms);
}

void gui_side_menu_draw(const GuiSideMenuFrameRasterCfg *args)
{
    GuiIcons *icons = args->icons;
    // the side_menu's pane is the slat its edge shows, whole outside the claim, laid by the frame's base; the tools stand
    // in the slat
    const GuiRect pane = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->side_menu_band,
                                    .other = args->frame->side_menu);
    const int scale = args->frame->scale;
    const unsigned int icon =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDE_MENU_ICON, .scale = scale);
    const float stroke = GUI_SIDE_MENU_STROKE * (float)icon / GUI_ICON_UNITS;
    // the beads' atlas laid out for the buttons' size, each button's cell at its resting place and the window's lights,
    // so the strip's slide moves the beads without baking them again: a button's frames bake the first time it is drawn
    const unsigned int side =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDE_MENU_BUTTON, .scale = scale);
    const unsigned int reach =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_REACH, .scale = scale);
    GuiRect cells[GUI_BEAD_TOOLS_MAX];
    GuiRect rest[GUI_BEAD_TOOLS_MAX];
    for (unsigned int at = 0u; at < g_gui_tools_count; at += 1u)
    {
        cells[at] = EMBED_CALL(side_menu_bead_cell, SideMenuCtx, .frame = args->frame, .at = at);
        rest[at] = EMBED_CALL(side_menu_bead_cell, SideMenuCtx, .frame = args->frame, .strip = &args->frame->side_menu_rest,
                              .at = at);
    }
    GuiLight lights[GUI_LIGHTS_MAX];
    const unsigned int light_count = EMBED_CALL(gui_lights_table.lights, GuiLightsCfg, .lights = lights, .frame = args->frame);
    // the atlas's pixels over the first segment of the block's bead part
    const GuiButtonsBakeCfg sizes = {
        .width = side,
        .height = side,
        .pad = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_PAD, .scale = scale),
        .reach = (reach < 3u) ? 3u : reach,
        .tools = g_gui_tools_count};
    unsigned char *const span = EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_BEAD, .segment = 0u,
                                           .bytes = gui_buttons_bake_table.bytes(&sizes));
    (void)EMBED_CALL(gui_buttons_bake_table.bake, GuiButtonsBakeCfg, .buttons = args->buttons, .palette = args->palette,
                     .width = sizes.width, .height = sizes.height, .pad = sizes.pad, .reach = sizes.reach, .cells = rest,
                     .tools = sizes.tools, .lights = lights, .light_count = light_count,
                     .pixels = (GuiColor *)(void *)span);
    for (unsigned int at = 0u; at < g_gui_tools_count; at += 1u)
    {
        const GuiRect button = EMBED_CALL(gui_side_menu.button, GuiSideMenuCfg, .frame = args->frame, .at = at);
        const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = button, .other = pane);
        if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
        {
            continue;
        }
        // the tools are fewer than an int counts
        const int displayed = ((args->side_menu_read->displayed == (int)at) && !args->side_menu_read->collapsed) ||
                              (args->side_menu_read->foot == (int)at) ||
                              (args->side_menu_read->terminal_open && (at == args->side_menu_read->terminal_at));
        const int pointed = args->side_menu_read->pointed == (int)at;
        // each tool's icon on the glass, then its bead of water over the icon
        const GuiMask *const mask_bits = EMBED_CALL(gui_icons.icon, GuiIconsCfg, .icons = icons,
                                                    .name = g_gui_tools[at].glyph, .size = icon, .stroke = stroke);
        if (mask_bits != NULL)
        {
            GuiColor color;
            if (pointed)
            {
                color = args->palette->top_menu_bright;
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
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_gui_tools[at].name, .other = "problems", .cap = 32ULL}))
        {
            marked = args->side_menu_read->problems_marked;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = g_gui_tools[at].name, .other = "runs", .cap = 32ULL}))
        {
            marked = args->side_menu_read->runs_live != 0u;
            mark_color = (args->side_menu_read->runs_panicked != 0u) ? args->palette->error : args->palette->signal;
        }
        if (marked)
        {
            const int mark =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDE_MENU_MARK, .scale = scale);
            const int in =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIDE_MENU_MARK_IN, .scale = scale);
            const GuiRect dot = {button.x + button.width - in - mark, button.y + in, mark, mark};
            // the dot's corners round by half its side, the dot a circle
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = dot, .other = on),
                       .radius = (unsigned int)(mark / 2), .color = mark_color);
        }
        EMBED_CALL(gui_buttons_draw_table.draw, GuiButtonsDrawCfg, .buttons = args->buttons, .raster = args->raster, .tool = at,
                   .x = cells[at].x, .y = cells[at].y, .time_ms = args->time_ms, .clip = pane);
    }
}
