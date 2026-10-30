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

void gui_strip_made(GuiStrip *strip)
{
    strip->pointed = GUI_STRIP_NONE;
    strip->shown = GUI_STRIP_NONE;
}

unsigned int gui_strip_tools(void)
{
    return STRIP_TOOLS;
}

const GuiTool *gui_strip_tool(unsigned int at)
{
    return &s_tools[(at < STRIP_TOOLS) ? at : 0u];
}

GuiRect gui_strip_button(const GuiFrame *frame, unsigned int at)
{
    const float scale = frame->scale;
    const int side = gui_scaled(GUI_STRIP_BUTTON, scale);
    const int pitch = side + gui_scaled(GUI_STRIP_BUTTON_GAP, scale);
    const int pad = gui_scaled(GUI_STRIP_PAD, scale);
    const int x = frame->strip.x + ((frame->strip.width - side) / 2);
    const GuiTool *const tool = gui_strip_tool(at);
    if (tool->group == GUI_STRIP_TOP)
    {
        unsigned int before = 0u;
        for (unsigned int each = 0u; each < at; each += 1u)
        {
            before += (s_tools[each].group == GUI_STRIP_TOP) ? 1u : 0u;
        }
        // the tools before are far fewer than an int counts
        return (GuiRect){x, frame->strip.y + pad + ((int)before * pitch), side, side};
    }
    unsigned int after = 0u;
    for (unsigned int each = at + 1u; each < STRIP_TOOLS; each += 1u)
    {
        after += (s_tools[each].group == GUI_STRIP_FOOT) ? 1u : 0u;
    }
    // the tools after are far fewer than an int counts
    return (GuiRect){x, frame->strip.y + frame->strip.height - pad - side - ((int)after * pitch), side, side};
}

int gui_strip_hit(const GuiFrame *frame, int x, int y, unsigned int *at)
{
    if (!gui_rect_holds(frame->strip, x, y))
    {
        return 0;
    }
    for (unsigned int each = 0u; each < STRIP_TOOLS; each += 1u)
    {
        if (gui_rect_holds(gui_strip_button(frame, each), x, y))
        {
            *at = each;
            return 1;
        }
    }
    return 0;
}

int gui_strip_moved(GuiStrip *strip, const GuiFrame *frame, int x, int y)
{
    unsigned int at = 0u;
    // the tools are fewer than an int counts
    const int pointed = gui_strip_hit(frame, x, y, &at) ? (int)at : GUI_STRIP_NONE;
    if (pointed == strip->pointed)
    {
        return 0;
    }
    strip->pointed = pointed;
    return 1;
}

void gui_strip_toggled(GuiStrip *strip, unsigned int at)
{
    if ((at >= STRIP_TOOLS) || s_tools[at].menu)
    {
        return;
    }
    // the tools are fewer than an int counts
    strip->shown = (strip->shown == (int)at) ? GUI_STRIP_NONE : (int)at;
}

int gui_strip_pressed(GuiStrip *strip, const GuiFrame *frame, int x, int y)
{
    if (!gui_rect_holds(frame->strip, x, y))
    {
        return 0;
    }
    unsigned int at = 0u;
    if (gui_strip_hit(frame, x, y, &at))
    {
        gui_strip_toggled(strip, at);
    }
    return 1;
}

int gui_strip_keyed(GuiStrip *strip, const char *name)
{
    for (unsigned int at = 0u; at < STRIP_TOOLS; at += 1u)
    {
        const char *const keys = s_tools[at].keys;
        if ((keys[0] != '\0') && gui_menubar_keys_name(keys, cellul.len(&(CatenaFinitaCfg){.src = keys, .cap = 32ull}), name))
        {
            gui_strip_toggled(strip, at);
            return 1;
        }
    }
    return 0;
}

void gui_strip_drawn(GuiRaster *raster, GuiIcons *icons, const GuiPalette *palette, const GuiFrame *frame,
                     const GuiStrip *strip)
{
    // the strip's pane is the slat its edge shows, whole outside the claim, laid by the frame's base; the tools stand
    // in the slat
    const GuiRect pane = gui_rect_met(frame->strip_slat, frame->strip);
    const float scale = frame->scale;
    const unsigned int icon = (unsigned int)gui_scaled(GUI_STRIP_ICON, scale);
    const float stroke = GUI_STRIP_STROKE * (float)icon / GUI_ICON_UNITS;
    const unsigned int radius = (unsigned int)gui_scaled(GUI_STRIP_BUTTON_RADIUS, scale);
    for (unsigned int at = 0u; at < STRIP_TOOLS; at += 1u)
    {
        const GuiRect button = gui_strip_button(frame, at);
        const GuiRect on = gui_rect_met(button, pane);
        if (gui_rect_empty(on))
        {
            continue;
        }
        // the tools are fewer than an int counts
        const int shown = strip->shown == (int)at;
        const int pointed = strip->pointed == (int)at;
        if (pointed)
        {
            gui_raster_rounded(raster, on, radius, palette->bar_title_hover);
        }
        else if (shown)
        {
            gui_raster_rounded(raster, on, radius, palette->strip_on);
        }
        const GuiMask *const mask = gui_icon(icons, s_tools[at].glyph, icon, stroke);
        if (mask != NULL)
        {
            const GuiColor color = pointed ? palette->bar_bright : (shown ? palette->signal : palette->status_text);
            // the icon's side is far inside what an int represents
            gui_raster_masked(raster, mask, button.x + ((button.width - (int)icon) / 2), button.y + ((button.height - (int)icon) / 2),
                              color, on);
        }
        // the Problems tool's mark, a dot in the error's color at the button's top right corner
        if (strip->problems_marked && cellul.eq(&(CatenaFinitaCfg){.src = s_tools[at].name, .other = "problems", .cap = 32ull}))
        {
            const int mark = gui_scaled(GUI_STRIP_MARK, scale);
            const int in = gui_scaled(GUI_STRIP_MARK_IN, scale);
            const GuiRect dot = {button.x + button.width - in - mark, button.y + in, mark, mark};
            // the dot's corners round by half its side, the dot a circle
            gui_raster_rounded(raster, gui_rect_met(dot, on), (unsigned int)(mark / 2), palette->error);
        }
    }
}
