// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs.c: the tabs row. Each tab is as wide as its name, its padding and its close mark, and the tabs stand in a row
// from the row's left; tabs past the row's right edge are cut off at it
#include "tabs.h"

#include <math.h>

// the close mark's text
static const char s_close[] = "\xc3\x97";

const char *gui_tab_name(const GuiTab *tab, unsigned long long *length)
{
    unsigned long long start = 0ull;
    unsigned long long at = 0ull;
    while (tab->path[at] != '\0')
    {
        if (((tab->path[at] == '/') || (tab->path[at] == '\\')) && (tab->path[at + 1ull] != '\0'))
        {
            start = at + 1ull;
        }
        at += 1ull;
    }
    *length = at - start;
    return tab->path + start;
}

// a tab's width: its padding each side, its name, the gap and the close mark with its padding
static int tabs_wide(GuiGlyphs *glyphs, const GuiTab *tab, unsigned int size, float scale)
{
    unsigned long long length = 0ull;
    const char *const name = gui_tab_name(tab, &length);
    const float text = gui_text_width(glyphs, GUI_FACE_CODE, size, name, length);
    const float close = gui_text_width(glyphs, GUI_FACE_CODE, size, s_close, sizeof(s_close) - 1u);
    return gui_scaled(GUI_TAB_PAD_LEFT, scale) + (int)roundf(text) + gui_scaled(GUI_TAB_GAP, scale) +
           gui_scaled(GUI_TAB_CLOSE_PAD, scale) + (int)roundf(close) + gui_scaled(GUI_TAB_CLOSE_PAD, scale) +
           gui_scaled(GUI_TAB_PAD_RIGHT, scale);
}

GuiRect gui_tab_rect(GuiGlyphs *glyphs, const GuiFrame *frame, const GuiTab *tab, unsigned int count, unsigned int at)
{
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_TAB_SIZE, frame->scale);
    int x = frame->tabs.x;
    for (unsigned int each = 0u; (each < at) && (each < count); each += 1u)
    {
        x += tabs_wide(glyphs, &tab[each], size, frame->scale);
    }
    const int wide = (at < count) ? tabs_wide(glyphs, &tab[at], size, frame->scale) : 0;
    return (GuiRect){x, frame->tabs.y, wide, frame->tabs.height - gui_scaled(1, frame->scale)};
}

GuiRect gui_tab_close_rect(GuiGlyphs *glyphs, const GuiFrame *frame, const GuiTab *tab, unsigned int count,
                           unsigned int at)
{
    const GuiRect rect = gui_tab_rect(glyphs, frame, tab, count, at);
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_TAB_SIZE, frame->scale);
    const float close = gui_text_width(glyphs, GUI_FACE_CODE, size, s_close, sizeof(s_close) - 1u);
    const int wide = (2 * gui_scaled(GUI_TAB_CLOSE_PAD, frame->scale)) + (int)roundf(close);
    return (GuiRect){rect.x + rect.width - gui_scaled(GUI_TAB_PAD_RIGHT, frame->scale) - wide, rect.y, wide, rect.height};
}

int gui_tabs_hit(GuiGlyphs *glyphs, const GuiFrame *frame, const GuiTab *tab, unsigned int count, int x, int y,
                 unsigned int *at, int *closing)
{
    if (!gui_rect_holds(frame->tabs, x, y))
    {
        return 0;
    }
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (gui_rect_holds(gui_tab_rect(glyphs, frame, tab, count, each), x, y))
        {
            *at = each;
            *closing = gui_rect_holds(gui_tab_close_rect(glyphs, frame, tab, count, each), x, y);
            return 1;
        }
    }
    return 0;
}

void gui_tabs_drawn(GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette, const GuiFrame *frame,
                    const GuiTab *tab, unsigned int count, int active)
{
    const float scale = frame->scale;
    const int one = gui_scaled(1, scale);
    gui_raster_filled(raster, frame->tabs, palette->surface);
    gui_raster_filled(raster, (GuiRect){frame->tabs.x, frame->tabs.y + frame->tabs.height - one, frame->tabs.width, one},
                      palette->line);
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_TAB_SIZE, scale);
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_CODE, size);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_CODE, size);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const GuiRect rect = gui_tab_rect(glyphs, frame, tab, count, at);
        const GuiRect met = gui_rect_met(rect, frame->tabs);
        if (gui_rect_empty(met))
        {
            break;
        }
        // the tabs are fewer than an int counts
        const int shown = (int)at == active;
        if (shown)
        {
            gui_raster_rounded(raster, met, (unsigned int)gui_scaled(GUI_TAB_RADIUS, scale), palette->tab_ground);
            const int line = gui_scaled(GUI_TAB_LINE, scale);
            gui_raster_filled(raster, gui_rect_met((GuiRect){rect.x, rect.y, rect.width, line}, frame->tabs), palette->tab_line);
            gui_raster_filled(raster, gui_rect_met((GuiRect){rect.x, rect.y, line, rect.height}, frame->tabs), palette->tab_line);
        }
        gui_raster_filled(raster, gui_rect_met((GuiRect){rect.x + rect.width - one, rect.y, one, rect.height}, frame->tabs),
                          palette->line);
        const int baseline = rect.y + (int)((((float)rect.height - ascent - descent) / 2.0f) + ascent + 0.5f);
        unsigned long long length = 0ull;
        const char *const name = gui_tab_name(&tab[at], &length);
        (void)gui_text_drawn(raster, glyphs, GUI_FACE_CODE, size, (float)(rect.x + gui_scaled(GUI_TAB_PAD_LEFT, scale)), baseline,
                             name, length, shown ? palette->text : palette->text_2, met);
        const GuiRect close = gui_tab_close_rect(glyphs, frame, tab, count, at);
        (void)gui_text_drawn(raster, glyphs, GUI_FACE_CODE, size, (float)(close.x + gui_scaled(GUI_TAB_CLOSE_PAD, scale)), baseline,
                             s_close, sizeof(s_close) - 1u, palette->text_3, met);
    }
}
