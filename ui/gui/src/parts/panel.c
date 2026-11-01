// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// panel.c: the panels' box and their Markdown. The layout runs once over the text, drawing as it measures, and runs
// again with no raster to measure alone, which keeps one set of rules for both
#include "panel.h"

#include "../../../cli/src/text/text.h"
#include "../frame.h"

void gui_panel_box_drawn(GuiRaster *raster, const GuiPalette *palette, GuiRect rect, float scale)
{
    const unsigned int radius = (unsigned int)gui_scaled(GUI_PANEL_RADIUS, scale);
    const int line = gui_scaled(1, scale);
    gui_raster_rounded(raster, rect, radius, palette->widget_line);
    const GuiRect inside = {rect.x + line, rect.y + line, rect.width - (2 * line), rect.height - (2 * line)};
    // the inner corners round by the radius less the line, no less than nothing
    const unsigned int inner = ((unsigned int)line < radius) ? radius - (unsigned int)line : 0u;
    gui_raster_rounded(raster, inside, inner, palette->surface);
}

// What a layout holds as it runs: where it draws, the width, the scale, the pen's x on the line being laid, the y of
// the line's top and the line's height, and the faces' sizes
typedef struct
{
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    GuiRect clip;
    int left;
    int wide;
    float x;
    int y;
    unsigned int text_size;
    unsigned int code_size;
    int text_line;
    int code_line;
    float scale;
} GuiLayout;

// the baseline of a line `tall` pixels high whose top is `y`, the face centered in it
static int layout_baseline(const GuiLayout *layout, unsigned int face, unsigned int size, int y, int tall)
{
    const float ascent = gui_glyphs_ascent(layout->glyphs, face, size);
    const float descent = gui_glyphs_descent(layout->glyphs, face, size);
    return y + (int)((((float)tall - ascent - descent) / 2.0f) + ascent + 0.5f);
}

// `length` bytes of `text` drawn in `face` at `size` from the pen on the line whose top is `y`, where there is a
// raster: the pen moved past them
static void layout_run(GuiLayout *layout, unsigned int face, unsigned int size, int tall, const char *text,
                       unsigned long long length, GuiColor color)
{
    if (layout->raster != NULL)
    {
        const int baseline = layout_baseline(layout, face, size, layout->y, tall);
        layout->x = gui_text_drawn(layout->raster, layout->glyphs, face, size, layout->x, baseline, text, length, color,
                                   layout->clip);
    }
    else
    {
        layout->x += gui_text_width(layout->glyphs, face, size, text, length);
    }
}

// a line of code laid from `text`, `length` bytes, broken by letters where it is wider than the width: each piece on
// a line of its own
static void layout_code_line(GuiLayout *layout, const char *text, unsigned long long length, GuiColor color)
{
    unsigned long long at = 0ull;
    layout->x = (float)layout->left;
    while (at < length)
    {
        unsigned long long took = 0ull;
        (void)cli_text_letter(text, length, at, &took);
        took = (took != 0ull) ? took : 1ull;
        const float letter_wide = gui_text_width(layout->glyphs, GUI_FACE_CODE, layout->code_size, text + at, took);
        if ((layout->x + letter_wide > (float)(layout->left + layout->wide)) && (layout->x > (float)layout->left))
        {
            layout->y += layout->code_line;
            layout->x = (float)layout->left;
        }
        layout_run(layout, GUI_FACE_CODE, layout->code_size, layout->code_line, text + at, took, color);
        at += took;
    }
    layout->y += layout->code_line;
}

// a word of a paragraph laid from `text`, `length` bytes, in `face`, wrapped to the next line where it does not fit
// after the pen, and broken by letters where it is wider than the whole width
static void layout_word(GuiLayout *layout, unsigned int face, unsigned int size, const char *text,
                        unsigned long long length, GuiColor color)
{
    const float wide = gui_text_width(layout->glyphs, face, size, text, length);
    const float right = (float)(layout->left + layout->wide);
    if ((layout->x + wide > right) && (layout->x > (float)layout->left))
    {
        layout->y += layout->text_line;
        layout->x = (float)layout->left;
    }
    if (layout->x + wide <= right)
    {
        layout_run(layout, face, size, layout->text_line, text, length, color);
        return;
    }
    unsigned long long at = 0ull;
    while (at < length)
    {
        unsigned long long took = 0ull;
        (void)cli_text_letter(text, length, at, &took);
        took = (took != 0ull) ? took : 1ull;
        const float letter_wide = gui_text_width(layout->glyphs, face, size, text + at, took);
        if ((layout->x + letter_wide > right) && (layout->x > (float)layout->left))
        {
            layout->y += layout->text_line;
            layout->x = (float)layout->left;
        }
        layout_run(layout, face, size, layout->text_line, text + at, took, color);
        at += took;
    }
}

// a paragraph's line laid from `text`, `length` bytes, word by word, the text between single backticks in the code
// face in the type's color; the pen left after its last word, a blank's width on, for the next line's words
static void layout_paragraph_line(GuiLayout *layout, const char *text, unsigned long long length)
{
    const GuiColor type = gui_palette_token(layout->palette, "type", 4ull);
    int code = 0;
    unsigned long long at = 0ull;
    while (at < length)
    {
        if (text[at] == '`')
        {
            code = !code;
            at += 1ull;
            continue;
        }
        if (text[at] == ' ')
        {
            layout->x += gui_text_width(layout->glyphs, GUI_FACE_TEXT, layout->text_size, " ", 1ull);
            at += 1ull;
            continue;
        }
        unsigned long long end = at;
        end = cli_text_to_any(text, end, length, " `");
        if (code)
        {
            layout_word(layout, GUI_FACE_CODE, layout->code_size, text + at, end - at, type);
        }
        else
        {
            layout_word(layout, GUI_FACE_TEXT, layout->text_size, text + at, end - at, layout->palette->text);
        }
        at = end;
    }
    layout->x += gui_text_width(layout->glyphs, GUI_FACE_TEXT, layout->text_size, " ", 1ull);
}

// whether `length` bytes of a line are `fence`, the three backticks, or three dashes, with blanks about them
static int layout_line_is(const char *text, unsigned long long length, char mark)
{
    unsigned long long from = 0ull;
    unsigned long long to = length;
    from = cli_text_past_any(text, from, to, " \t\r");
    to = (from + cli_text_back_past(text + from, to - from, " \t\r"));
    if (to - from < 3ull)
    {
        return 0;
    }
    for (unsigned long long at = from; at < from + 3ull; at += 1ull)
    {
        if (text[at] != mark)
        {
            return 0;
        }
    }
    // a fence may name its language after the backticks; a rule is its dashes alone
    return (mark == '`') || (to - from == 3ull);
}

int gui_panel_markdown(GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette, const char *text,
                       unsigned long long length, int x, int y, int wide, float scale, GuiRect clip)
{
    GuiLayout layout = {raster,
                        glyphs,
                        palette,
                        clip,
                        x,
                        wide,
                        (float)x,
                        y,
                        (unsigned int)(((float)GUI_PANEL_TEXT_SIZE * scale) + 0.5f),
                        (unsigned int)(((float)GUI_PANEL_CODE_SIZE * scale) + 0.5f),
                        gui_scaled(GUI_PANEL_TEXT_LINE, scale),
                        gui_scaled(GUI_PANEL_CODE_LINE, scale),
                        scale};
    int fenced = 0;
    int in_paragraph = 0;
    unsigned long long at = 0ull;
    while (at < length)
    {
        unsigned long long end = at;
        end = cli_text_to_any(text, end, length, "\n");
        unsigned long long line_length = end - at;
        line_length =
            ((line_length > 0ull) && (text[at + line_length - 1ull] == '\r')) ? line_length - 1ull : line_length;
        const char *const line = text + at;
        at = (end < length) ? end + 1ull : end;
        if (layout_line_is(line, line_length, '`'))
        {
            if (in_paragraph)
            {
                layout.y += layout.text_line + gui_scaled(GUI_PANEL_PARAGRAPH, scale);
                in_paragraph = 0;
            }
            fenced = !fenced;
            continue;
        }
        if (fenced)
        {
            layout_code_line(&layout, line, line_length, palette->editor_text);
            continue;
        }
        if (layout_line_is(line, line_length, '-'))
        {
            if (in_paragraph)
            {
                layout.y += layout.text_line + gui_scaled(GUI_PANEL_PARAGRAPH, scale);
                in_paragraph = 0;
            }
            const int margin = gui_scaled(GUI_PANEL_RULE, scale);
            if (raster != NULL)
            {
                gui_raster_filled(raster,
                                  gui_rect_met((GuiRect){x, layout.y + margin, wide, gui_scaled(1, scale)}, clip),
                                  palette->widget_line);
            }
            layout.y += (2 * margin) + gui_scaled(1, scale);
            continue;
        }
        if (line_length == 0ull)
        {
            if (in_paragraph)
            {
                layout.y += layout.text_line + gui_scaled(GUI_PANEL_PARAGRAPH, scale);
                in_paragraph = 0;
            }
            continue;
        }
        if (!in_paragraph)
        {
            layout.x = (float)x;
            in_paragraph = 1;
        }
        layout_paragraph_line(&layout, line, line_length);
    }
    if (in_paragraph)
    {
        layout.y += layout.text_line;
    }
    return layout.y - y;
}

GuiRect gui_panel_placed(GuiRect anchor, int wide, int tall, int back, GuiRect bounds, int above, float scale)
{
    const int off = gui_scaled(GUI_PANEL_OFF, scale);
    const int below_y = anchor.y + anchor.height + off;
    const int above_y = anchor.y - off - tall;
    const int fits_below = below_y + tall <= bounds.y + bounds.height;
    const int fits_above = above_y >= bounds.y;
    int y = below_y;
    if ((above && fits_above) || (!fits_below && fits_above))
    {
        y = above_y;
    }
    else if (!fits_below)
    {
        // neither side has the room: the box stands as low as the bounds allow
        y = bounds.y + bounds.height - tall;
        y = (y < bounds.y) ? bounds.y : y;
    }
    int x = anchor.x - back;
    const int right = bounds.x + bounds.width - wide;
    x = (x > right) ? right : x;
    x = (x < bounds.x) ? bounds.x : x;
    return (GuiRect){x, y, wide, tall};
}
