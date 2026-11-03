// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// terminal.c: the terminal's panel drawn from what the controller publishes (terminal.h). Its places are whole pixels:
// each cell of a screen stands at its column times the code face's cell, rounded once, and its row times the row's
// height
#include "terminal.h"

#include "../../command_line/cli_internal.h"
#include "../../controller/controller.h"

typedef struct
{
    const GuiTerminal *terminal_read;
    const GuiFrame *frame;
    GuiGlyphs *glyphs;
    GuiRaster *raster;
    const GuiPalette *palette;
    unsigned int view;
    unsigned int at;
    int pixels;
    unsigned int value;
    GuiColor color;
    GuiRect *close;
    const char *text;
    unsigned int size;
} TerminalCtx;

// `pixels` in the page's pixels at the frame's scale
EMBED_INLINE int terminal_scaled(const TerminalCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels, .scale = args->frame->scale);
}

// the width of the C string `text` in the code face at `size`, in whole pixels
EMBED_INLINE int terminal_text_width(const TerminalCtx *args)
{
    const float width = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs,
                                   .face = GUI_FACE_CODE, .size = args->size, .text = args->text,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = 256ULL}));
    // a text's width is far inside what an int represents
    return (int)(width + 0.5f);
}

// the code face's cell at the terminal's text size, in whole pixels, one at the least
EMBED_INLINE int terminal_cell_width(const TerminalCtx *args)
{
    const unsigned int size = (unsigned int)EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame,
                                                       .pixels = (int)GUI_TERMINAL_TEXT_SIZE);
    const float cell =
        EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = args->glyphs, .size = size);
    // a cell is far inside what an int represents
    const int width = (int)(cell + 0.5f);
    return (width > 0) ? width : 1;
}

// the strip's rectangle, under the panel's top line
EMBED_INLINE GuiRect terminal_strip(const TerminalCtx *args)
{
    const GuiRect panel = args->frame->terminal;
    const int line = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_LINE);
    return (GuiRect){panel.x, panel.y + line, panel.width,
                     EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_STRIP)};
}

// the tool `at`, 0 New Terminal and 1 Split, at the strip's right end
EMBED_INLINE GuiRect terminal_tool(const TerminalCtx *args)
{
    const GuiRect strip = EMBED_CALL(terminal_strip, TerminalCtx, .frame = args->frame);
    const int width = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_TOOL_WIDTH);
    const int height =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_TOOL_HEIGHT);
    const int pad =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_STRIP_PAD_LEFT);
    const int gap = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_STRIP_GAP);
    // the tools are two, which an int counts
    const int from_right = (2 - (int)args->at) * width + ((1 - (int)args->at) * gap);
    return (GuiRect){strip.x + strip.width - pad - from_right, strip.y + ((strip.height - height) / 2), width, height};
}

// the tab `at`'s rectangle in the strip, and its ×'s into `close`, the tabs laid one after another from the strip's
// left
EMBED_INLINE GuiRect terminal_tab(const TerminalCtx *args)
{
    const CliTerminalPanel *const panel = args->terminal_read->panel;
    const GuiRect strip = EMBED_CALL(terminal_strip, TerminalCtx, .frame = args->frame);
    const unsigned int size = (unsigned int)EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame,
                                                       .pixels = (int)GUI_TERMINAL_TAB_SIZE);
    const int height =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_TAB_HEIGHT);
    const int pad_left =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_TAB_PAD_LEFT);
    const int pad_right =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_TAB_PAD_RIGHT);
    const int gap = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_TAB_GAP);
    const int close_pad =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_CLOSE_PAD);
    const int between =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_STRIP_GAP);
    const int close_width = (2 * close_pad) + EMBED_CALL(terminal_text_width, TerminalCtx, .glyphs = args->glyphs,
                                                         .size = size, .text = "\xc3\x97");
    int x =
        strip.x + EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_STRIP_PAD_LEFT);
    const int y = strip.y + ((strip.height - height) / 2);
    for (unsigned int each = 0u;; each += 1u)
    {
        const int label = EMBED_CALL(terminal_text_width, TerminalCtx, .glyphs = args->glyphs, .size = size,
                                     .text = panel->tab[each].label);
        const int width = pad_left + label + gap + close_width + pad_right;
        if (each == args->at)
        {
            *args->close = (GuiRect){x + width - pad_right - close_width, y, close_width, height};
            return (GuiRect){x, y, width, height};
        }
        x += width + between;
    }
}

// a cell's color `value`: one of the sixteen from the palette, the 6 by 6 by 6 cube with each part 0 or 55 and 40 a
// step, the 24 grays from 8 by tens, or red, green and blue; `color` where it is the default
EMBED_INLINE GuiColor terminal_color(const TerminalCtx *args)
{
    const unsigned int kind = args->value & CLI_TERMINAL_COLOR_KIND;
    const unsigned int index = args->value & 0xffu;
    if (kind == CLI_TERMINAL_COLOR_DIRECT)
    {
        return GUI_RGB((args->value >> 16u) & 0xffu, (args->value >> 8u) & 0xffu, args->value & 0xffu);
    }
    if (kind != CLI_TERMINAL_COLOR_INDEXED)
    {
        return args->color;
    }
    if (index < GUI_TERMINAL_COLORS)
    {
        return args->palette->terminal[index];
    }
    if (index < 232u)
    {
        const unsigned int cube = index - 16u;
        const unsigned int red = cube / 36u;
        const unsigned int green = (cube / 6u) % 6u;
        const unsigned int blue = cube % 6u;
        return GUI_RGB((red != 0u) ? (55u + (40u * red)) : 0u, (green != 0u) ? (55u + (40u * green)) : 0u,
                       (blue != 0u) ? (55u + (40u * blue)) : 0u);
    }
    const unsigned int gray = 8u + (10u * (index - 232u));
    return GUI_RGB(gray, gray, gray);
}

int gui_terminal_size(const GuiTerminalCfg *args)
{
    const CliTerminalPanel *const panel = args->terminal_read->panel;
    const GuiRect views = args->frame->terminal_views;
    if ((panel == NULL) || !panel->open || (args->view >= panel->views) || (views.height <= 0))
    {
        return 0;
    }
    const int line = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_LINE);
    // the views are one or two, which an int counts
    const int count = (int)panel->views;
    const int width = (views.width - ((count - 1) * line)) / count;
    const int across =
        width - (2 * EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_PAD_SIDE));
    const int down = views.height -
                     EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_PAD_TOP) -
                     EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_PAD_BOTTOM);
    const int cell = EMBED_CALL(terminal_cell_width, TerminalCtx, .frame = args->frame, .glyphs = args->glyphs);
    const int row = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_ROW);
    const int columns = (across > 0) ? (across / cell) : 0;
    const int rows = ((down > 0) && (row > 0)) ? (down / row) : 0;
    // the counts are not negative, and two at the least
    *args->columns = (columns > 2) ? (unsigned int)columns : 2u;
    *args->rows = (rows > 2) ? (unsigned int)rows : 2u;
    return 1;
}

// the screen `view`'s rectangle among the views
EMBED_INLINE GuiRect terminal_view(const TerminalCtx *args)
{
    const GuiRect views = args->frame->terminal_views;
    const int line = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_LINE);
    // the views are one or two, which an int counts
    const int count = (int)args->terminal_read->panel->views;
    const int width = (views.width - ((count - 1) * line)) / count;
    return (args->view == 0u) ? (GuiRect){views.x, views.y, width, views.height}
                              : (GuiRect){views.x + width + line, views.y, views.width - width - line, views.height};
}

unsigned int gui_terminal_hit(const GuiTerminalCfg *args)
{
    const CliTerminalPanel *const panel = args->terminal_read->panel;
    if ((panel == NULL) || !panel->open ||
        !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->terminal, .x = args->x,
                    .y = args->y))
    {
        return CLI_HIT_NONE;
    }
    for (unsigned int view = 0u; view < panel->views; view += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(terminal_view, TerminalCtx, .terminal_read = args->terminal_read,
                                          .frame = args->frame, .view = view),
                       .x = args->x, .y = args->y))
        {
            *args->index = view;
            return CLI_HIT_TERMINAL;
        }
    }
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(terminal_tool, TerminalCtx, .frame = args->frame, .at = at), .x = args->x,
                       .y = args->y))
        {
            return (at == 0u) ? CLI_HIT_TERMINAL_NEW : CLI_HIT_TERMINAL_SPLIT;
        }
    }
    for (unsigned int at = 0u; at < panel->tabs; at += 1u)
    {
        GuiRect close;
        const GuiRect tab = EMBED_CALL(terminal_tab, TerminalCtx, .terminal_read = args->terminal_read,
                                       .frame = args->frame, .glyphs = args->glyphs, .at = at, .close = &close);
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = tab, .x = args->x, .y = args->y))
        {
            *args->index = at;
            return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = close, .x = args->x, .y = args->y)
                       ? CLI_HIT_TERMINAL_TAB_CLOSE
                       : CLI_HIT_TERMINAL_TAB;
        }
    }
    return CLI_HIT_NONE;
}

// the baseline of text in `face` at `size`, from the top of a row `pixels` tall
EMBED_INLINE int terminal_baseline(const TerminalCtx *args)
{
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs,
                                    .face = GUI_FACE_CODE, .size = args->size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs,
                                     .face = GUI_FACE_CODE, .size = args->size);
    // a row's baseline is far inside what an int represents
    return (int)((((float)args->pixels - ascent - descent) / 2.0f) + ascent + 0.5f);
}

// the strip drawn: each tab, the one shown on the ground with the signal under it, and the two tools
EMBED_INLINE void terminal_strip_draw(const TerminalCtx *args)
{
    const CliTerminalPanel *const panel = args->terminal_read->panel;
    const GuiPalette *const palette = args->palette;
    const GuiRect strip = EMBED_CALL(terminal_strip, TerminalCtx, .frame = args->frame);
    const unsigned int size = (unsigned int)EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame,
                                                       .pixels = (int)GUI_TERMINAL_TAB_SIZE);
    const int pad_left =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_TAB_PAD_LEFT);
    const int close_pad =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_CLOSE_PAD);
    const int shown_line =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_SHOWN_LINE);
    const GuiRect tabs_clip = {strip.x, strip.y,
                               EMBED_CALL(terminal_tool, TerminalCtx, .frame = args->frame, .at = 0u).x - strip.x,
                               strip.height};
    for (unsigned int at = 0u; at < panel->tabs; at += 1u)
    {
        GuiRect close;
        const GuiRect tab = EMBED_CALL(terminal_tab, TerminalCtx, .terminal_read = args->terminal_read,
                                       .frame = args->frame, .glyphs = args->glyphs, .at = at, .close = &close);
        const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = tab, .other = tabs_clip);
        if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
        {
            break;
        }
        // the tabs are fewer than an int counts
        const int shown = ((int)at == panel->shown) || ((int)at == panel->beside);
        if (shown)
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = args->raster, .rect = on,
                       .radius = (unsigned int)EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame,
                                                          .pixels = GUI_TERMINAL_RADIUS),
                       .color = palette->background_fill);
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){tab.x, tab.y + tab.height - shown_line, tab.width, shown_line},
                                      .other = on),
                       .color = palette->signal);
        }
        const GuiColor text = panel->tab[at].exited ? palette->text_3 : (shown ? palette->text : palette->text_2);
        const int baseline = tab.y + EMBED_CALL(terminal_baseline, TerminalCtx, .glyphs = args->glyphs, .size = size,
                                                .pixels = tab.height);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                         .glyphs = args->glyphs, .face = GUI_FACE_CODE, .size = size,
                         .x_f32 = (float)(tab.x + pad_left), .y = baseline, .text = panel->tab[at].label,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = panel->tab[at].label, .cap = 256ULL}),
                         .color = text, .clip = on);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                         .glyphs = args->glyphs, .face = GUI_FACE_CODE, .size = size,
                         .x_f32 = (float)(close.x + close_pad), .y = baseline, .text = "\xc3\x97", .length = 2ULL,
                         .color = palette->text_3, .clip = on);
    }
    const int icon = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_ICON);
    // each icon's lines on the 16 a side its paths are drawn on, from 3.5 to 12.5 across a box `icon` wide
    const int near = (icon * 7) / 32;
    const int far = (icon * 25) / 32;
    const int middle = icon / 2;
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const GuiRect tool = EMBED_CALL(terminal_tool, TerminalCtx, .frame = args->frame, .at = at);
        const int x = tool.x + ((tool.width - icon) / 2);
        const int y = tool.y + ((tool.height - icon) / 2);
        const GuiColor color = ((at == 1u) && (panel->beside >= 0)) ? palette->signal : palette->text_2;
        const int lines[2][4][4] = {
            {{x + middle, y + near, x + middle, y + far}, {x + near, y + middle, x + far, y + middle}, {0}, {0}},
            {{x + (icon * 5 / 32), y + near, x + (icon * 27 / 32), y + near},
             {x + (icon * 5 / 32), y + far, x + (icon * 27 / 32), y + far},
             {x + (icon * 5 / 32), y + near, x + (icon * 5 / 32), y + far},
             {x + (icon * 27 / 32), y + near, x + (icon * 27 / 32), y + far}}};
        for (unsigned int each = 0u; each < 4u; each += 1u)
        {
            const int *const one = lines[at][each];
            if ((one[0] != 0) || (one[2] != 0))
            {
                EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = one[0],
                           .y0 = one[1], .x1 = one[2], .y1 = one[3], .color = color);
            }
        }
        if (at == 1u)
        {
            EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = x + middle,
                       .y0 = y + near, .x1 = x + middle, .y1 = y + far, .color = color);
        }
    }
}

// the screen `view` drawn: each cell's ground where it has one of its own, its letter in its color, face and marks, and
// the cursor
EMBED_INLINE void terminal_view_draw(const TerminalCtx *args)
{
    const CliTerminalPanel *const panel = args->terminal_read->panel;
    const CliTerminalView *const shown = &panel->view[args->view];
    const GuiPalette *const palette = args->palette;
    const GuiRect place = EMBED_CALL(terminal_view, TerminalCtx, .terminal_read = args->terminal_read,
                                     .frame = args->frame, .view = args->view);
    if ((panel->views == 2u) && shown->focused)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                   .rect = (GuiRect){place.x, place.y, place.width,
                                     EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame,
                                                .pixels = GUI_TERMINAL_CHOSEN_LINE)},
                   .color = palette->signal);
    }
    const unsigned int size = (unsigned int)EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame,
                                                       .pixels = (int)GUI_TERMINAL_TEXT_SIZE);
    const int cell = EMBED_CALL(terminal_cell_width, TerminalCtx, .frame = args->frame, .glyphs = args->glyphs);
    const int row = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_ROW);
    const int one = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = 1);
    const int baseline =
        EMBED_CALL(terminal_baseline, TerminalCtx, .glyphs = args->glyphs, .size = size, .pixels = row);
    const int left =
        place.x + EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_PAD_SIDE);
    const int top =
        place.y + EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_PAD_TOP);
    const int solid = panel->keys && shown->focused;
    for (unsigned int y = 0u; y < shown->rows; y += 1u)
    {
        for (unsigned int x = 0u; x < shown->columns; x += 1u)
        {
            const CliTerminalCell *const at = &shown->cell[((unsigned long long)y * shown->columns) + x];
            // a screen's columns and rows are far inside what an int represents
            const GuiRect box = {left + ((int)x * cell), top + ((int)y * row), cell, row};
            const int cursor = shown->cursor_shown && (x == shown->cursor_x) && (y == shown->cursor_y);
            const int inverse = (at->marks & CLI_TERMINAL_INVERSE) != 0u;
            const GuiColor foreground = EMBED_CALL(terminal_color, TerminalCtx, .palette = palette,
                                                   .value = at->foreground, .color = palette->text);
            const GuiColor background = EMBED_CALL(terminal_color, TerminalCtx, .palette = palette,
                                                   .value = at->background, .color = inverse ? palette->well : 0u);
            GuiColor ground =
                inverse ? (at->foreground != CLI_TERMINAL_COLOR_DEFAULT ? foreground : palette->text) : background;
            GuiColor ink = inverse ? background : foreground;
            ground = (cursor && solid) ? palette->signal : ground;
            ink = (cursor && solid) ? palette->well : ink;
            // a dim letter is drawn at three fifths of its ink
            ink = ((at->marks & CLI_TERMINAL_DIM) != 0u)
                      ? ((ink & 0x00ffffffu) | (((GUI_ALPHA_OF(ink) * 3u) / 5u) << 24u))
                      : ink;
            const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = box, .other = place);
            if (GUI_ALPHA_OF(ground) != 0u)
            {
                EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = on,
                           .color = ground);
            }
            if (at->letter > (unsigned int)' ')
            {
                const unsigned int bold = ((at->marks & CLI_TERMINAL_BOLD) != 0u) ? 1u : 0u;
                const unsigned int italic = ((at->marks & CLI_TERMINAL_ITALIC) != 0u) ? 2u : 0u;
                float advance = 0.0f;
                const GuiGlyphCell *const glyph =
                    EMBED_CALL(gui_glyphs_face_letter.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = args->glyphs,
                               .face = GUI_FACE_CODE + bold + italic, .letter = at->letter, .size = size, .color = ink,
                               .advance = &advance);
                if (glyph != NULL)
                {
                    EMBED_CALL(gui_glyphs_raster.glyph_cell_layout, GuiGlyphsRasterCfg, .raster = args->raster,
                               .cell = glyph, .x = box.x, .y = box.y + baseline, .clip = on);
                }
            }
            if ((at->marks & CLI_TERMINAL_UNDERLINE) != 0u)
            {
                EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                           .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                              .one = (GuiRect){box.x, box.y + baseline + one, cell, one}, .other = on),
                           .color = ink);
            }
            if ((at->marks & CLI_TERMINAL_STRIKE) != 0u)
            {
                EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                           .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                              .one = (GuiRect){box.x, box.y + (row / 2), cell, one}, .other = on),
                           .color = ink);
            }
            if (cursor && !solid)
            {
                const GuiRect edges[4] = {{box.x, box.y, cell, one},
                                          {box.x, box.y + row - one, cell, one},
                                          {box.x, box.y, one, row},
                                          {box.x + cell - one, box.y, one, row}};
                for (unsigned int edge = 0u; edge < 4u; edge += 1u)
                {
                    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = edges[edge],
                                                  .other = on),
                               .color = palette->signal);
                }
            }
        }
    }
}

void gui_terminal_draw(const GuiTerminalCfg *args)
{
    const CliTerminalPanel *const panel = args->terminal_read->panel;
    const GuiRect place = args->frame->terminal;
    if ((panel == NULL) || !panel->open || EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = place))
    {
        return;
    }
    const int line = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_LINE);
    const GuiRect strip = EMBED_CALL(terminal_strip, TerminalCtx, .frame = args->frame);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = (GuiRect){place.x, place.y, place.width, line}, .color = args->palette->line);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){place.x, strip.y + strip.height, place.width, line}, .other = place),
               .color = args->palette->line);
    EMBED_CALL(terminal_strip_draw, TerminalCtx, .terminal_read = args->terminal_read, .frame = args->frame,
               .glyphs = args->glyphs, .raster = args->raster, .palette = args->palette);
    for (unsigned int view = 0u; view < panel->views; view += 1u)
    {
        if (view == 1u)
        {
            const GuiRect beside = EMBED_CALL(terminal_view, TerminalCtx, .terminal_read = args->terminal_read,
                                              .frame = args->frame, .view = 1u);
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = (GuiRect){beside.x - line, beside.y, line, beside.height}, .color = args->palette->line);
        }
        EMBED_CALL(terminal_view_draw, TerminalCtx, .terminal_read = args->terminal_read, .frame = args->frame,
                   .glyphs = args->glyphs, .raster = args->raster, .palette = args->palette, .view = view);
    }
}
