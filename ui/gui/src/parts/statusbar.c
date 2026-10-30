// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// statusbar.c: the status bar's items stand from its right end, the lock first, then the editor's line and the memory
// reading, each a gap apart, and the crumbs from its left, the folders in the bar's text, the file in its bright text
// and the marks between them in the path's
#include "statusbar.h"

// the crumbs' mark between a folder and what it holds
static const char s_crumb_mark[] = "\xe2\x80\xba";

void gui_status_memory_text(unsigned long long bytes, char *text, unsigned long long size)
{
    VerbaLinea line = {text, size, 0ull};
    const unsigned long long tenths = ((bytes * 10ull) + (1024ull * 512ull)) / (1024ull * 1024ull);
    if (tenths < 1000ull)
    {
        mmgr_line_decimal(&line, tenths / 10ull, 1u);
        mmgr_line_ch(&line, '.');
        mmgr_line_decimal(&line, tenths % 10ull, 1u);
    }
    else
    {
        mmgr_line_decimal(&line, (bytes + (1024ull * 512ull)) / (1024ull * 1024ull), 1u);
    }
    mmgr_line_put(&line, " MB");
    (void)mmgr_line_finish(&line);
}

void gui_status_cursor_text(const GuiStatus *status, char *text, unsigned long long size)
{
    VerbaLinea line = {text, size, 0ull};
    mmgr_line_decimal(&line, status->line + 1ull, 1u);
    mmgr_line_ch(&line, ':');
    mmgr_line_decimal(&line, status->col + 1ull, 1u);
    (void)mmgr_line_finish(&line);
}

void gui_status_selection_text(const GuiStatus *status, char *text, unsigned long long size)
{
    VerbaLinea line = {text, size, 0ull};
    const unsigned long long lines = (status->anchor_line > status->line) ? status->anchor_line - status->line
                                                                           : status->line - status->anchor_line;
    const unsigned long long letters = (status->anchor_col > status->col) ? status->anchor_col - status->col
                                                                           : status->col - status->anchor_col;
    if (lines != 0ull)
    {
        mmgr_line_decimal(&line, lines + 1ull, 1u);
        mmgr_line_put(&line, " lines selected");
    }
    else if (letters != 0ull)
    {
        mmgr_line_decimal(&line, letters, 1u);
        mmgr_line_put(&line, " selected");
    }
    (void)mmgr_line_finish(&line);
}

void gui_status_indent_text(const GuiStatus *status, char *text, unsigned long long size)
{
    VerbaLinea line = {text, size, 0ull};
    const unsigned int step = (status->indent_size != 0u) ? status->indent_size : 4u;
    if (status->indent_tabs == 1)
    {
        mmgr_line_put(&line, "Tab ");
        mmgr_line_decimal(&line, step, 1u);
    }
    else
    {
        mmgr_line_decimal(&line, step, 1u);
        mmgr_line_put(&line, " spaces");
    }
    (void)mmgr_line_finish(&line);
}

// an item's text drawn ending at `right`, on the baseline, in `color`: the x it starts at
static float statusbar_item_drawn(GuiRaster *raster, GuiGlyphs *glyphs, unsigned int size, float right, int baseline,
                                  const char *text, GuiColor color, GuiRect clip)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = GUI_STATUS_TEXT});
    const float wide = gui_text_width(glyphs, GUI_FACE_HEAD, size, text, length);
    (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, right - wide, baseline, text, length, color, clip);
    return right - wide;
}

void gui_statusbar_drawn(GuiRaster *raster, GuiGlyphs *glyphs, GuiIcons *icons, const GuiPalette *palette,
                         const GuiFrame *frame, const GuiStatus *status)
{
    const float scale = frame->scale;
    // the bar's pane is the slat its edge shows, whole outside the claim, laid by the frame's base; the items stand
    // in the slat
    const GuiRect bar = gui_rect_met(frame->status_slat, frame->status);
    if (gui_rect_empty(bar))
    {
        return;
    }
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_STATUS_SIZE, scale);
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_HEAD, size);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_HEAD, size);
    const int baseline = bar.y + (int)((((float)bar.height - ascent - descent) / 2.0f) + ascent + 0.5f);
    const int gap = gui_scaled(GUI_STATUS_GAP, scale);
    char text[GUI_STATUS_TEXT];
    // the items from the right: the lock, the indentation, the encoding, the line ends, the selection and the cursor,
    // while a file is shown, then the memory reading
    float right = (float)(bar.x + bar.width - gui_scaled(GUI_STATUS_PAD, scale));
    if (status->has_cursor)
    {
        const int lock = gui_scaled(GUI_STATUS_LOCK, scale);
        right -= (float)(gui_scaled(GUI_FRAME_GAP + GUI_STATUS_LOCK_END, scale) + lock);
        const GuiMask *const mask = gui_icon(icons, "unlock", (unsigned int)lock, GUI_ICON_STROKE * (float)lock / GUI_ICON_UNITS);
        if (mask != NULL)
        {
            gui_raster_masked(raster, mask, (int)right, bar.y + ((bar.height - lock) / 2), palette->status_text, bar);
        }
        right -= (float)gap;
        gui_status_indent_text(status, text, sizeof(text));
        right = statusbar_item_drawn(raster, glyphs, size, right, baseline, text, palette->status_text, bar) - (float)gap;
        right = statusbar_item_drawn(raster, glyphs, size, right, baseline, "UTF-8", palette->status_text, bar) - (float)gap;
        right = statusbar_item_drawn(raster, glyphs, size, right, baseline, status->crlf ? "CRLF" : "LF", palette->status_text, bar) - (float)gap;
        gui_status_selection_text(status, text, sizeof(text));
        if (text[0] != '\0')
        {
            right = statusbar_item_drawn(raster, glyphs, size, right, baseline, text, palette->status_text, bar) - (float)gap;
        }
        gui_status_cursor_text(status, text, sizeof(text));
        right = statusbar_item_drawn(raster, glyphs, size, right, baseline, text, palette->status_text, bar) - (float)gap;
    }
    if (status->memory != 0ull)
    {
        gui_status_memory_text(status->memory, text, sizeof(text));
        const float least = (float)GUI_STATUS_MEMORY_ZEROS * gui_text_width(glyphs, GUI_FACE_HEAD, size, "0", 1ull);
        const float wide = gui_text_width(glyphs, GUI_FACE_HEAD, size, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = GUI_STATUS_TEXT}));
        (void)statusbar_item_drawn(raster, glyphs, size, right, baseline, text, palette->status_text, bar);
        right -= ((wide > least) ? wide : least) + (float)gap;
    }
    // the crumbs from the left, as far as the items leave
    if (status->path[0] == '\0')
    {
        return;
    }
    const GuiRect room = gui_rect_met((GuiRect){bar.x, bar.y, (int)right - bar.x, bar.height}, bar);
    // the crumbs start past the strip's width, which the claim takes to none
    const int strip_room = (int)(((1.0f - frame->claim.claim) * (float)frame->strip.width) + 0.5f);
    float x = (float)(frame->status.x + strip_room + gui_scaled(GUI_STATUS_PAD, scale));
    const float crumb_gap = (float)gui_scaled(GUI_STATUS_CRUMB_GAP, scale);
    const char *at = status->path;
    while (*at != '\0')
    {
        unsigned long long length = 0ull;
        while ((at[length] != '\0') && (at[length] != '/') && (at[length] != '\\'))
        {
            length += 1ull;
        }
        const int last = at[length] == '\0';
        x = gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, x, baseline, at, length, last ? palette->bar_bright : palette->status_text, room);
        if (last)
        {
            break;
        }
        x += crumb_gap;
        x = gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, x, baseline, s_crumb_mark, sizeof(s_crumb_mark) - 1u, palette->bar_path, room) + crumb_gap;
        at += length + 1ull;
    }
}
