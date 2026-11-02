// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// panel.c: the panels' box and their Markdown. The layout runs once over the text, drawing as it measures, and runs
// again with no raster to measure alone, which saves one set of rules for both
#include "panel.h"

#include "../../../cli/src/text/text.h"
#include "../frame.h"

// What a layout holds as it runs: where it draws, the width, the scale, the pen's x on the line being laid, the y of
// the line's top and the line's height, and the faces' sizes
typedef struct
{
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    GuiRect clip;
    int left;
    int pixel_width;
    float x;
    int y;
    unsigned int text_size;
    unsigned int code_size;
    int text_line;
    int code_line;
    int scale;
} GuiLayout;
void gui_panel_box_draw(const GuiPanelCfg *args)
{
    int scale = args->scale;
    const unsigned int radius =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_RADIUS, .scale = scale);
    const int line = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster, .rect = args->rect,
               .radius = radius, .color = args->palette->widget_line);
    const GuiRect inside = {args->rect.x + line, args->rect.y + line, args->rect.width - (2 * line),
                            args->rect.height - (2 * line)};
    // the inner corners round by the radius less the line, no less than nothing
    const unsigned int inner = ((unsigned int)line < radius) ? radius - (unsigned int)line : 0u;
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster, .rect = inside,
               .radius = inner, .color = args->palette->surface);
}

typedef struct
{
    const GuiLayout *layout_read;
    unsigned int face;
    unsigned int size;
    int y;
    int pixel_height;
    GuiLayout *layout;
    const char *text;
    unsigned long long length;
    GuiColor color;
    char mark;
} PanelCtx;
// the baseline of a line `pixel_height` pixels high whose top is `y`, the face centered in it
EMBED_INLINE int layout_baseline(const PanelCtx *args)
{
    unsigned int face = args->face;
    unsigned int size = args->size;
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = args->layout_read->glyphs,
                                    .face = face, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg,
                                     .glyphs_read = args->layout_read->glyphs, .face = face, .size = size);
    return args->y + (int)((((float)args->pixel_height - ascent - descent) / 2.0f) + ascent + 0.5f);
}

// `length` bytes of `text` drawn in `face` at `size` from the pen on the line whose top is `y`, where there is a
// raster: the pen moved past them
EMBED_INLINE void layout_run(const PanelCtx *args)
{
    unsigned int face = args->face;
    unsigned int size = args->size;
    const char *text = args->text;
    unsigned long long length = args->length;
    GuiColor color = args->color;
    if (args->layout->raster != NULL)
    {
        const int baseline = EMBED_CALL(layout_baseline, PanelCtx, .layout_read = args->layout, .face = face,
                                        .size = size, .y = args->layout->y, .pixel_height = args->pixel_height);
        args->layout->x =
            EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->layout->raster,
                       .glyphs = args->layout->glyphs, .face = face, .size = size, .x_f32 = args->layout->x,
                       .y = baseline, .text = text, .length = length, .color = color, .clip = args->layout->clip);
    }
    else
    {
        args->layout->x +=
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->layout->glyphs,
                       .face = face, .size = size, .text = text, .length = length);
    }
}

// a line of code laid from `text`, `length` bytes, broken by letters where it is wider than the width: each piece on
// a line of its own
EMBED_INLINE void layout_code_line(const PanelCtx *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    unsigned long long at = 0ull;
    args->layout->x = (float)args->layout->left;
    while (at < length)
    {
        unsigned long long consumed = 0ull;
        (void)EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = length, .at = at, .length = &consumed);
        consumed = (consumed != 0ull) ? consumed : 1ull;
        const float letter_width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->layout->glyphs,
                       .face = GUI_FACE_CODE, .size = args->layout->code_size, .text = text + at, .length = consumed);
        if ((args->layout->x + letter_width > (float)(args->layout->left + args->layout->pixel_width)) &&
            (args->layout->x > (float)args->layout->left))
        {
            args->layout->y += args->layout->code_line;
            args->layout->x = (float)args->layout->left;
        }
        EMBED_CALL(layout_run, PanelCtx, .layout = args->layout, .face = GUI_FACE_CODE, .size = args->layout->code_size,
                   .pixel_height = args->layout->code_line, .text = text + at, .length = consumed,
                   .color = args->color);
        at += consumed;
    }
    args->layout->y += args->layout->code_line;
}

// a word of a paragraph laid from `text`, `length` bytes, in `face`, wrapped to the next line where it does not fit
// after the pen, and broken by letters where it is wider than the whole width
EMBED_INLINE void layout_word(const PanelCtx *args)
{
    unsigned int face = args->face;
    unsigned int size = args->size;
    const char *text = args->text;
    unsigned long long length = args->length;
    const float pixel_width =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->layout->glyphs, .face = face,
                   .size = size, .text = text, .length = length);
    const float right = (float)(args->layout->left + args->layout->pixel_width);
    if ((args->layout->x + pixel_width > right) && (args->layout->x > (float)args->layout->left))
    {
        args->layout->y += args->layout->text_line;
        args->layout->x = (float)args->layout->left;
    }
    if (args->layout->x + pixel_width <= right)
    {
        EMBED_CALL(layout_run, PanelCtx, .layout = args->layout, .face = face, .size = size,
                   .pixel_height = args->layout->text_line, .text = text, .length = length, .color = args->color);
        return;
    }
    unsigned long long at = 0ull;
    while (at < length)
    {
        unsigned long long consumed = 0ull;
        (void)EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = length, .at = at, .length = &consumed);
        consumed = (consumed != 0ull) ? consumed : 1ull;
        const float letter_width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->layout->glyphs,
                       .face = face, .size = size, .text = text + at, .length = consumed);
        if ((args->layout->x + letter_width > right) && (args->layout->x > (float)args->layout->left))
        {
            args->layout->y += args->layout->text_line;
            args->layout->x = (float)args->layout->left;
        }
        EMBED_CALL(layout_run, PanelCtx, .layout = args->layout, .face = face, .size = size,
                   .pixel_height = args->layout->text_line, .text = text + at, .length = consumed,
                   .color = args->color);
        at += consumed;
    }
}

// a paragraph's line laid from `text`, `length` bytes, word by word, the text between single backticks in the code
// face in the type's color; the pen left after its last word, a blank's width on, for the next line's words
EMBED_INLINE void layout_paragraph_line(const PanelCtx *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    const GuiColor type = EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = args->layout->palette,
                                     .name_read = "type", .length = 4ull);
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
            args->layout->x +=
                EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->layout->glyphs,
                           .face = GUI_FACE_TEXT, .size = args->layout->text_size, .text = " ", .length = 1ull);
            at += 1ull;
            continue;
        }
        unsigned long long end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = end,
                         .size = length, .set = " `");
        if (code)
        {
            EMBED_CALL(layout_word, PanelCtx, .layout = args->layout, .face = GUI_FACE_CODE,
                       .size = args->layout->code_size, .text = text + at, .length = end - at, .color = type);
        }
        else
        {
            EMBED_CALL(layout_word, PanelCtx, .layout = args->layout, .face = GUI_FACE_TEXT,
                       .size = args->layout->text_size, .text = text + at, .length = end - at,
                       .color = args->layout->palette->text);
        }
        at = end;
    }
    args->layout->x += EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->layout->glyphs,
                                  .face = GUI_FACE_TEXT, .size = args->layout->text_size, .text = " ", .length = 1ull);
}

// whether `length` bytes of a line are `fence`, the three backticks, or three dashes, with blanks about them
EMBED_INLINE int layout_line_is(const PanelCtx *args)
{
    const char *text = args->text;
    unsigned long long from = 0ull;
    unsigned long long to = args->length;
    from = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = from, .size = to,
                      .set = " \t\r");
    to = (from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = text + from,
                            .at = to - from, .set = " \t\r"));
    if (to - from < 3ull)
    {
        return 0;
    }
    for (unsigned long long at = from; at < from + 3ull; at += 1ull)
    {
        if (text[at] != args->mark)
        {
            return 0;
        }
    }
    // a fence may name its language after the backticks; a rule is its dashes alone
    return (args->mark == '`') || (to - from == 3ull);
}

int gui_panel_markdown(const GuiPanelRasterCfg *args)
{
    int x = args->x;
    int y = args->y;
    int scale = args->scale;
    GuiLayout layout = {
        args->raster,
        args->glyphs,
        args->palette,
        args->clip,
        x,
        args->pixel_width,
        (float)x,
        y,
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_TEXT_SIZE,
                                 .scale = scale),
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_CODE_SIZE,
                                 .scale = scale),
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_TEXT_LINE, .scale = scale),
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_CODE_LINE, .scale = scale),
        scale};
    int fenced = 0;
    int in_paragraph = 0;
    unsigned long long at = 0ull;
    while (at < args->length)
    {
        unsigned long long end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = end,
                         .size = args->length, .set = "\n");
        unsigned long long line_length = end - at;
        line_length =
            ((line_length > 0ull) && (args->text[at + line_length - 1ull] == '\r')) ? line_length - 1ull : line_length;
        const char *const line = args->text + at;
        at = (end < args->length) ? end + 1ull : end;
        if (EMBED_CALL(layout_line_is, PanelCtx, .text = line, .length = line_length, .mark = '`'))
        {
            if (in_paragraph)
            {
                layout.y += layout.text_line + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                          .pixels = GUI_PANEL_PARAGRAPH, .scale = scale);
                in_paragraph = 0;
            }
            fenced = !fenced;
            continue;
        }
        if (fenced)
        {
            EMBED_CALL(layout_code_line, PanelCtx, .layout = &layout, .text = line, .length = line_length,
                       .color = args->palette->editor_text);
            continue;
        }
        if (EMBED_CALL(layout_line_is, PanelCtx, .text = line, .length = line_length, .mark = '-'))
        {
            if (in_paragraph)
            {
                layout.y += layout.text_line + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                          .pixels = GUI_PANEL_PARAGRAPH, .scale = scale);
                in_paragraph = 0;
            }
            const int margin =
                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_RULE, .scale = scale);
            if (args->raster != NULL)
            {
                EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                           .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                              .one = (GuiRect){x, layout.y + margin, args->pixel_width,
                                                               EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                          .pixels = 1, .scale = scale)},
                                              .other = args->clip),
                           .color = args->palette->widget_line);
            }
            layout.y +=
                (2 * margin) + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
            continue;
        }
        if (line_length == 0ull)
        {
            if (in_paragraph)
            {
                layout.y += layout.text_line + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                          .pixels = GUI_PANEL_PARAGRAPH, .scale = scale);
                in_paragraph = 0;
            }
            continue;
        }
        if (!in_paragraph)
        {
            layout.x = (float)x;
            in_paragraph = 1;
        }
        EMBED_CALL(layout_paragraph_line, PanelCtx, .layout = &layout, .text = line, .length = line_length);
    }
    if (in_paragraph)
    {
        layout.y += layout.text_line;
    }
    return layout.y - y;
}

GuiRect gui_panel_position(const GuiPanelWidthCfg *args)
{
    int scale = args->scale;
    const int off = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_OFF, .scale = scale);
    const int below_y = args->anchor.y + args->anchor.height + off;
    const int above_y = args->anchor.y - off - args->pixel_height;
    const int fits_below = below_y + args->pixel_height <= args->bounds.y + args->bounds.height;
    const int fits_above = above_y >= args->bounds.y;
    int y = below_y;
    if ((args->above && fits_above) || (!fits_below && fits_above))
    {
        y = above_y;
    }
    else if (!fits_below)
    {
        // neither side has the capacity: the box stands as low as the bounds allow
        y = args->bounds.y + args->bounds.height - args->pixel_height;
        y = (y < args->bounds.y) ? args->bounds.y : y;
    }
    int x = args->anchor.x - args->back;
    const int right = args->bounds.x + args->bounds.width - args->pixel_width;
    x = (x > right) ? right : x;
    x = (x < args->bounds.x) ? args->bounds.x : x;
    return (GuiRect){x, y, args->pixel_width, args->pixel_height};
}
