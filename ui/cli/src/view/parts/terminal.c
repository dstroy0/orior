// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// terminal.c: the terminal's panel drawn as the window last read it of the terminal's tabs (terminal.h). Its places are
// whole pixels: each cell of a screen stands at its column times the code face's cell, rounded once, and its row times
// the row's height
#include "terminal.h"

#include "../../command_line/cli_internal.h"
#include "../../text/text.h"

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
    int x;
    int y;
    unsigned long long *row;
    unsigned long long *column;
    unsigned int letter;
    const CliTerminalStyle *style;
    GuiRect box;
    GuiRect place;
    int baseline;
    int width;
    int height;
    int one;
    int chosen;
    int cursor;
    int solid;
    const char *token;
    unsigned long long length;
    const char *word;
    CliTerminalStyle *style_out;
} TerminalCtx;

// a mark the xterm grammar's tokens set: the word past `keyword.` that puts it on, the same with `.off` after it
// taking it off
typedef struct
{
    const char *word;
    unsigned int mark;
} TerminalMark;

static const TerminalMark s_terminal_marks[] = {
    {"bold", CLI_TERMINAL_BOLD},           {"dim", CLI_TERMINAL_DIM},         {"italic", CLI_TERMINAL_ITALIC},
    {"underline", CLI_TERMINAL_UNDERLINE}, {"inverse", CLI_TERMINAL_INVERSE}, {"strike", CLI_TERMINAL_STRIKE},
};

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
        char label_text[CLI_PATH_MAX_LEN];
        VerbaLinea label_line = {label_text, sizeof(label_text), 0ULL};
        (void)cli_terminal_tabs_label(each, &label_line);
        (void)mmgr_line_finish(&label_line);
        const int label =
            EMBED_CALL(terminal_text_width, TerminalCtx, .glyphs = args->glyphs, .size = size, .text = label_text);
        const int width = pad_left + label + gap + close_width + pad_right;
        if (each == args->at)
        {
            *args->close = (GuiRect){x + width - pad_right - close_width, y, close_width, height};
            return (GuiRect){x, y, width, height};
        }
        x += width + between;
    }
}

int gui_terminal_size(const GuiTerminalCfg *args)
{
    const CliTerminalPanel *const panel = &args->terminal_read->panel;
    const GuiRect views = args->frame->terminal_views;
    if (!panel->open || (args->view >= panel->views) || (views.height <= 0))
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
    const int count = (int)args->terminal_read->panel.views;
    const int width = (views.width - ((count - 1) * line)) / count;
    return (args->view == 0u) ? (GuiRect){views.x, views.y, width, views.height}
                              : (GuiRect){views.x + width + line, views.y, views.width - width - line, views.height};
}

// the row of the pane `view` the pixel at x, y stands on and the column whose left edge it stands nearest, into `row`
// and `column`, each clamped to the pane's rows and columns
EMBED_INLINE void terminal_cell(const TerminalCtx *args)
{
    const CliTerminalView *const shown = &args->terminal_read->view[args->view];
    const GuiRect place = EMBED_CALL(terminal_view, TerminalCtx, .terminal_read = args->terminal_read,
                                     .frame = args->frame, .view = args->view);
    const int cell = EMBED_CALL(terminal_cell_width, TerminalCtx, .frame = args->frame, .glyphs = args->glyphs);
    const int row = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_ROW);
    const int left =
        place.x + EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_PAD_SIDE);
    const int top =
        place.y + EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_PAD_TOP);
    const int down = (args->y > top) ? ((args->y - top) / ((row > 0) ? row : 1)) : 0;
    const int across = (args->x > left) ? (((args->x - left) + (cell / 2)) / cell) : 0;
    // a pane's rows and columns are far inside what an int represents, and neither count is below one
    const int last_row = (shown->rows > 0u) ? ((int)shown->rows - 1) : 0;
    *args->row = (unsigned long long)((down < last_row) ? down : last_row);
    *args->column = (unsigned long long)((across < (int)shown->columns) ? across : (int)shown->columns);
}

int gui_terminal_cell_at(const GuiTerminalCfg *args)
{
    const CliTerminalPanel *const panel = &args->terminal_read->panel;
    if (!panel->open || (args->view >= panel->views))
    {
        return 0;
    }
    EMBED_CALL(terminal_cell, TerminalCtx, .terminal_read = args->terminal_read, .frame = args->frame,
               .glyphs = args->glyphs, .view = args->view, .x = args->x, .y = args->y, .row = args->row,
               .column = args->column);
    return 1;
}

unsigned int gui_terminal_hit(const GuiTerminalCfg *args)
{
    const CliTerminalPanel *const panel = &args->terminal_read->panel;
    if (!panel->open || !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->terminal,
                                    .x = args->x, .y = args->y))
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
            if ((args->row != NULL) && (args->column != NULL))
            {
                EMBED_CALL(terminal_cell, TerminalCtx, .terminal_read = args->terminal_read, .frame = args->frame,
                           .glyphs = args->glyphs, .view = view, .x = args->x, .y = args->y, .row = args->row,
                           .column = args->column);
            }
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
    return CLI_HIT_TERMINAL_PANEL;
}

// the strip drawn inside `place`: each tab, the one shown on the ground with the signal under it, and the two tools,
// each color at the share of its alpha the panel has reached
EMBED_INLINE void terminal_strip_draw(const TerminalCtx *args)
{
    const CliTerminalPanel *const panel = &args->terminal_read->panel;
    const GuiPalette *const palette = args->palette;
    const int share = args->frame->terminal_share;
    const GuiRect strip = EMBED_CALL(terminal_strip, TerminalCtx, .frame = args->frame);
    const unsigned int size = (unsigned int)EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame,
                                                       .pixels = (int)GUI_TERMINAL_TAB_SIZE);
    const int pad_left =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_TAB_PAD_LEFT);
    const int close_pad =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_CLOSE_PAD);
    const int shown_line =
        EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_SHOWN_LINE);
    const GuiRect tabs_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){strip.x, strip.y,
                                    EMBED_CALL(terminal_tool, TerminalCtx, .frame = args->frame, .at = 0u).x - strip.x,
                                    strip.height},
                   .other = args->place);
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
                       .color = GUI_FRAME_FADED(palette->background_fill, share));
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){tab.x, tab.y + tab.height - shown_line, tab.width, shown_line},
                                      .other = on),
                       .color = GUI_FRAME_FADED(palette->signal, share));
        }
        char label_text[CLI_PATH_MAX_LEN];
        VerbaLinea label_line = {label_text, sizeof(label_text), 0ULL};
        const int exited = cli_terminal_tabs_label(at, &label_line);
        const unsigned long long label_length = mmgr_line_finish(&label_line);
        const GuiColor text =
            GUI_FRAME_FADED(exited ? palette->text_3 : (shown ? palette->text : palette->text_2), share);
        const int baseline = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs,
                                        .face = GUI_FACE_CODE, .size = size, .top = tab.y, .pixel_height = tab.height);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                         .glyphs = args->glyphs, .face = GUI_FACE_CODE, .size = size,
                         .x_f32 = (float)(tab.x + pad_left), .y = baseline, .text = label_text, .length = label_length,
                         .color = text, .clip = on);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                         .glyphs = args->glyphs, .face = GUI_FACE_CODE, .size = size,
                         .x_f32 = (float)(close.x + close_pad), .y = baseline, .text = "\xc3\x97", .length = 2ULL,
                         .color = GUI_FRAME_FADED(palette->text_3, share), .clip = on);
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
        const GuiColor color =
            GUI_FRAME_FADED(((at == 1u) && (panel->beside >= 0)) ? palette->signal : palette->text_2, share);
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

// a terminal color `value`: one of the sixteen from the palette, the 6 by 6 by 6 cube with each part 0 or 55 and 40 a
// step, the 24 grays from 8 by tens, or red, green and blue; `color` where it is the default
EMBED_INLINE GuiColor terminal_color(const TerminalCtx *args)
{
    const unsigned int type = args->value & CLI_TERMINAL_COLOR_TYPE;
    const unsigned int index = args->value & 0xffu;
    if (type == CLI_TERMINAL_COLOR_DIRECT)
    {
        return GUI_RGB((args->value >> 16u) & 0xffu, (args->value >> 8u) & 0xffu, args->value & 0xffu);
    }
    if (type != CLI_TERMINAL_COLOR_INDEXED)
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

// the letter `letter` drawn in `box`, clipped to `place`, in the style `style` with `color` its foreground: its ground
// where the style has one of its own, the selection's where it is `chosen` and the signal's where the cursor stands on
// it `solid`; the letter in its color and face, dim at three fifths, inverse with its colors swapped, underlined and
// struck through; and the cursor's outline where it stands on it and is not solid; every color at the share of its
// alpha the panel has reached as it arrives
EMBED_INLINE void terminal_cell_draw(const TerminalCtx *args)
{
    const GuiPalette *const palette = args->palette;
    const CliTerminalStyle *const style = args->style;
    const GuiRect box = args->box;
    const int width = args->width;
    const int height = args->height;
    const int one = args->one;
    const int inverse = (style->marks & CLI_TERMINAL_INVERSE) != 0u;
    const GuiColor background = EMBED_CALL(terminal_color, TerminalCtx, .palette = palette, .value = style->background,
                                           .color = inverse ? palette->well : 0u);
    GuiColor ground = inverse ? args->color : background;
    GuiColor ink = inverse ? background : args->color;
    ground = args->chosen ? palette->selection : ground;
    ground = (args->cursor && args->solid) ? palette->signal : ground;
    ink = (args->cursor && args->solid) ? palette->well : ink;
    // a dim letter is drawn at three fifths of its ink
    ink = ((style->marks & CLI_TERMINAL_DIM) != 0u) ? ((ink & 0x00ffffffu) | (((GUI_ALPHA_OF(ink) * 3u) / 5u) << 24u))
                                                    : ink;
    // both at the share of their alpha the panel has reached as it arrives
    ground = GUI_FRAME_FADED(ground, args->frame->terminal_share);
    ink = GUI_FRAME_FADED(ink, args->frame->terminal_share);
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = box, .other = args->place);
    if (GUI_ALPHA_OF(ground) != 0u)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = on,
                   .color = ground);
    }
    if (args->letter > (unsigned int)' ')
    {
        const unsigned int bold = ((style->marks & CLI_TERMINAL_BOLD) != 0u) ? 1u : 0u;
        const unsigned int italic = ((style->marks & CLI_TERMINAL_ITALIC) != 0u) ? 2u : 0u;
        float advance = 0.0f;
        const GuiGlyphCell *const glyph =
            EMBED_CALL(gui_glyphs_face_letter.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = args->glyphs,
                       .face = GUI_FACE_CODE + bold + italic, .letter = args->letter, .size = args->size, .color = ink,
                       .advance = &advance);
        if (glyph != NULL)
        {
            EMBED_CALL(gui_glyphs_raster.glyph_cell_layout, GuiGlyphsRasterCfg, .raster = args->raster, .cell = glyph,
                       .x = box.x, .y = box.y + args->baseline, .clip = on);
        }
    }
    if ((style->marks & CLI_TERMINAL_UNDERLINE) != 0u)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){box.x, box.y + args->baseline + one, width, one}, .other = on),
                   .color = ink);
    }
    if ((style->marks & CLI_TERMINAL_STRIKE) != 0u)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){box.x, box.y + (height / 2), width, one}, .other = on),
                   .color = ink);
    }
    if (args->cursor && !args->solid)
    {
        const GuiRect edges[4] = {{box.x, box.y, width, one},
                                  {box.x, box.y + height - one, width, one},
                                  {box.x, box.y, one, height},
                                  {box.x + width - one, box.y, one, height}};
        for (unsigned int edge = 0u; edge < 4u; edge += 1u)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = edges[edge], .other = on),
                       .color = GUI_FRAME_FADED(palette->signal, args->frame->terminal_share));
        }
    }
}

// whether the `length` bytes at `token` are the word `word` whole
EMBED_INLINE int terminal_word_is(const TerminalCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = 32ULL});
    return (args->length == length) &&
           (memor.cmp(&(MemoriaCfg){.src = args->token, .other = args->word, .bytes = length}) == 0);
}

// the number the `size` bytes at `text` write, their first nine digits at the most
EMBED_INLINE unsigned int terminal_run_number(const TerminalCtx *args)
{
    unsigned int number = 0u;
    for (unsigned int at = 0u; (at < args->size) && (at < 9u); at += 1u)
    {
        const char letter = args->text[at];
        // a digit's value is far inside what an unsigned int counts
        number = ((letter >= '0') && (letter <= '9')) ? ((number * 10u) + (unsigned int)(letter - '0')) : number;
    }
    return number;
}

// `style_out` changed by the run of `size` bytes at `text` whose token is the `length` bytes at `token`: 1 where the
// token is keyword or keyword.<what>, an escape sequence among the letters, which takes no column; else 0, nothing
// changed. Past `keyword.`: reset is the plain style; a mark's word puts it on and the word with .off after it takes it
// off, normal taking bold and dim off; foreground.<n> and background.<n> are the color n of the sixteen, .default the
// default, .index the color of the 256 at the run's number, .direct red, green and blue at none, and .red, .green
// and .blue each that part at the run's number, 255 at the most
EMBED_INLINE int terminal_token_read(const TerminalCtx *args)
{
    static const char s_keyword[] = "keyword";
    static const char s_foreground[] = "foreground.";
    static const char s_background[] = "background.";
    static const char s_off[] = ".off";
    const unsigned long long head = sizeof(s_keyword) - 1u;
    const int keyword = (args->length >= head) &&
                        (memor.cmp(&(MemoriaCfg){.src = args->token, .other = s_keyword, .bytes = head}) == 0) &&
                        ((args->length == head) || (args->token[head] == '.'));
    if (!keyword || (args->length == head))
    {
        return keyword;
    }
    const char *const tail = args->token + head + 1u;
    const unsigned long long tail_length = args->length - head - 1u;
    CliTerminalStyle *const style = args->style_out;
    if (EMBED_CALL(terminal_word_is, TerminalCtx, .token = tail, .length = tail_length, .word = "reset"))
    {
        *style = (CliTerminalStyle){CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u};
        return 1;
    }
    if (EMBED_CALL(terminal_word_is, TerminalCtx, .token = tail, .length = tail_length, .word = "normal"))
    {
        style->marks &= ~(CLI_TERMINAL_BOLD | CLI_TERMINAL_DIM);
        return 1;
    }
    for (unsigned int at = 0u; at < (sizeof(s_terminal_marks) / sizeof(s_terminal_marks[0])); at += 1u)
    {
        const TerminalMark *const one = &s_terminal_marks[at];
        const unsigned long long word_length = cellul.len(&(CatenaFinitaCfg){.src = one->word, .cap = 32ULL});
        const int matched = (tail_length >= word_length) &&
                            (memor.cmp(&(MemoriaCfg){.src = tail, .other = one->word, .bytes = word_length}) == 0);
        if (matched && (tail_length == word_length))
        {
            style->marks |= one->mark;
            return 1;
        }
        if (matched && EMBED_CALL(terminal_word_is, TerminalCtx, .token = tail + word_length,
                                  .length = tail_length - word_length, .word = s_off))
        {
            style->marks &= ~one->mark;
            return 1;
        }
    }
    const unsigned long long side = sizeof(s_foreground) - 1u;
    const int foreground =
        (tail_length > side) && (memor.cmp(&(MemoriaCfg){.src = tail, .other = s_foreground, .bytes = side}) == 0);
    const int background =
        (tail_length > side) && (memor.cmp(&(MemoriaCfg){.src = tail, .other = s_background, .bytes = side}) == 0);
    if (!foreground && !background)
    {
        return 1;
    }
    unsigned int *const color = foreground ? &style->foreground : &style->background;
    const char *const part = tail + side;
    const unsigned long long part_length = tail_length - side;
    const unsigned int number = EMBED_CALL(terminal_run_number, TerminalCtx, .text = args->text, .size = args->size);
    const unsigned int sixteen = EMBED_CALL(terminal_run_number, TerminalCtx, .text = part,
                                            .size = (part_length < 3ULL) ? (unsigned int)part_length : 3u);
    const int digits = (part_length != 0ULL) && (part_length < 3ULL) && (part[0] >= '0') && (part[0] <= '9') &&
                       ((part_length == 1ULL) || ((part[1] >= '0') && (part[1] <= '9')));
    if (digits && (sixteen < GUI_TERMINAL_COLORS))
    {
        *color = CLI_TERMINAL_COLOR_INDEXED | sixteen;
    }
    else if (EMBED_CALL(terminal_word_is, TerminalCtx, .token = part, .length = part_length, .word = "default"))
    {
        *color = CLI_TERMINAL_COLOR_DEFAULT;
    }
    else if (EMBED_CALL(terminal_word_is, TerminalCtx, .token = part, .length = part_length, .word = "index"))
    {
        *color = CLI_TERMINAL_COLOR_INDEXED | (number & 0xffu);
    }
    else if (EMBED_CALL(terminal_word_is, TerminalCtx, .token = part, .length = part_length, .word = "direct"))
    {
        *color = CLI_TERMINAL_COLOR_DIRECT;
    }
    else
    {
        const int red = EMBED_CALL(terminal_word_is, TerminalCtx, .token = part, .length = part_length, .word = "red");
        const int green =
            EMBED_CALL(terminal_word_is, TerminalCtx, .token = part, .length = part_length, .word = "green");
        const int blue =
            EMBED_CALL(terminal_word_is, TerminalCtx, .token = part, .length = part_length, .word = "blue");
        const unsigned int shift = red ? 16u : (green ? 8u : 0u);
        const unsigned int parts = ((*color & CLI_TERMINAL_COLOR_TYPE) == CLI_TERMINAL_COLOR_DIRECT) ? *color : 0u;
        *color = (red || green || blue) ? (CLI_TERMINAL_COLOR_DIRECT | (parts & 0x00ffffffu & ~(0xffu << shift)) |
                                           (((number < 255u) ? number : 255u) << shift))
                                        : *color;
    }
    return 1;
}

// the pane `view` drawn: its view's rows of the text, the xterm's cells over the rows from the line they start on, each
// cell with the selection's ground where the view chose it, and the cursor
EMBED_INLINE void terminal_view_draw(const TerminalCtx *args)
{
    const CliTerminalPanel *const panel = &args->terminal_read->panel;
    const CliTerminalView *const shown = &args->terminal_read->view[args->view];
    const GuiPalette *const palette = args->palette;
    const GuiRect place = EMBED_CALL(terminal_view, TerminalCtx, .terminal_read = args->terminal_read,
                                     .frame = args->frame, .view = args->view);
    // the pane's pixels inside `place`, the panel's as the work's inside cuts it, and its colors at the share of their
    // alpha the panel has reached
    const GuiRect inside =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = place, .other = args->place);
    const int share = args->frame->terminal_share;
    if ((panel->views == 2u) && shown->focused)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){place.x, place.y, place.width,
                                                       EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame,
                                                                  .pixels = GUI_TERMINAL_CHOSEN_LINE)},
                                      .other = inside),
                   .color = GUI_FRAME_FADED(palette->signal, share));
    }
    const unsigned int size = (unsigned int)EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame,
                                                       .pixels = (int)GUI_TERMINAL_TEXT_SIZE);
    const int cell = EMBED_CALL(terminal_cell_width, TerminalCtx, .frame = args->frame, .glyphs = args->glyphs);
    const int row = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_ROW);
    const int one = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = 1);
    const int baseline = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs,
                                    .face = GUI_FACE_CODE, .size = size, .top = 0, .pixel_height = row);
    const int left =
        place.x + EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_PAD_SIDE);
    const int top =
        place.y + EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_PAD_TOP);
    const int solid = panel->keys && shown->focused;
    const CliViewFrame *const frame = args->terminal_read->frame[args->view];
    // the selection's ends in order, nothing chosen where they stand together
    const int anchored =
        (frame != NULL) && ((frame->anchor.line < frame->cursor.line) ||
                            ((frame->anchor.line == frame->cursor.line) && (frame->anchor.col < frame->cursor.col)));
    const CliPosition from = (frame == NULL) ? (CliPosition){0ULL, 0ULL} : (anchored ? frame->anchor : frame->cursor);
    const CliPosition to = (frame == NULL) ? (CliPosition){0ULL, 0ULL} : (anchored ? frame->cursor : frame->anchor);
    const int selected = (from.line != to.line) || (from.col != to.col);
    const CliDocumentNames *const names = args->terminal_read->names[args->view];
    const unsigned int count = (frame != NULL) ? frame->count : 0u;
    for (unsigned int y = 0u; (y < count) && (y < shown->rows); y += 1u)
    {
        const unsigned long long line = frame->row[y].number;
        const char *const text = frame->bytes + frame->row[y].text;
        const unsigned long long length = frame->row[y].length;
        // a line's runs stand in the frame's runs cell as its columns and then each run's token's place in the table,
        // none where that cell could not be met
        const CliDocumentLineRuns none = {0u, 0u};
        const CliDocumentLineRuns runs = (frame->runs != NULL) ? frame->runs->line[y] : none;
        const unsigned int *const column_of = (frame->runs != NULL) ? (frame->runs->word_text + runs.at) : NULL;
        const unsigned int *const name_of =
            (frame->runs != NULL) ? (frame->runs->word_text + runs.at + runs.runs) : NULL;
        const int on_chosen = selected && (line >= from.line) && (line <= to.line);
        const unsigned long long chosen_from = (line == from.line) ? from.col : 0ULL;
        const unsigned long long chosen_to = (line == to.line) ? to.col : ~0ULL;
        const int cursor_row = shown->cursor_shown && (line == (shown->xterm_line + shown->cursor_y));
        unsigned long long units = 0ULL;
        unsigned long long at = 0ULL;
        unsigned int x = 0u;
        unsigned int next = 0u;
        int sequence = 0;
        // the style of the letters, from the plain one at the line's start, changed by each keyword run's token
        CliTerminalStyle style = {CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u};
        while ((at < length) && (x < shown->columns))
        {
            while ((next < runs.runs) && (units >= column_of[next]))
            {
                const char *const token =
                    ((names != NULL) && (name_of[next] < names->count)) ? names->name[name_of[next]] : "";
                const unsigned long long run_end =
                    ((next + 1u) < runs.runs) ? column_of[next + 1u] : (units + (length - at));
                const unsigned long long run_size = (run_end > units) ? (run_end - units) : 0ULL;
                const unsigned long long within = (run_size < (length - at)) ? run_size : (length - at);
                // a keyword is an escape sequence among the letters, a byte to each of its units: it takes no column,
                // and its token changes the style of the letters after it; a run is far shorter than an unsigned int
                // counts
                sequence = EMBED_CALL(terminal_token_read, TerminalCtx, .token = token,
                                      .length = cellul.len(&(CatenaFinitaCfg){.src = token, .cap = 64ULL}),
                                      .text = text + at, .size = (unsigned int)within, .style_out = &style);
                next += 1u;
            }
            unsigned long long consumed = 0ULL;
            const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = length,
                                                   .at = at, .length = &consumed);
            at += (consumed != 0ULL) ? consumed : 1ULL;
            // a column is counted in the units of the text the view chooses in, a letter past the basic plane two
            const int chosen = on_chosen && (units >= chosen_from) && (units < chosen_to);
            units += (letter >= 0x10000u) ? 2ULL : 1ULL;
            if (sequence)
            {
                continue;
            }
            const GuiColor foreground = EMBED_CALL(terminal_color, TerminalCtx, .palette = palette,
                                                   .value = style.foreground, .color = palette->text);
            // a pane's columns and rows are far inside what an int represents
            EMBED_CALL(terminal_cell_draw, TerminalCtx, .raster = args->raster, .glyphs = args->glyphs,
                       .frame = args->frame, .palette = palette, .letter = letter, .color = foreground, .style = &style,
                       .box = (GuiRect){left + ((int)x * cell), top + ((int)y * row), cell, row}, .place = inside,
                       .size = size, .baseline = baseline, .width = cell, .height = row, .one = one, .chosen = chosen,
                       .cursor = cursor_row && (x == shown->cursor_x), .solid = solid);
            x += 1u;
        }
        // the cursor past the line's letters, on a blank of its own in the plain style
        if (cursor_row && (shown->cursor_x >= x) && (shown->cursor_x < shown->columns))
        {
            const CliTerminalStyle plain = {CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u};
            EMBED_CALL(terminal_cell_draw, TerminalCtx, .raster = args->raster, .glyphs = args->glyphs,
                       .frame = args->frame, .palette = palette, .letter = (unsigned int)' ', .color = palette->text,
                       .style = &plain,
                       .box = (GuiRect){left + ((int)shown->cursor_x * cell), top + ((int)y * row), cell, row},
                       .place = inside, .size = size, .baseline = baseline, .width = cell, .height = row, .one = one,
                       .chosen = 0, .cursor = 1, .solid = solid);
        }
    }
}

void gui_terminal_draw(const GuiTerminalCfg *args)
{
    const CliTerminalPanel *const panel = &args->terminal_read->panel;
    const GuiRect place = args->frame->terminal;
    if (!panel->open || EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = place))
    {
        return;
    }
    const int line = EMBED_CALL(terminal_scaled, TerminalCtx, .frame = args->frame, .pixels = GUI_TERMINAL_LINE);
    const GuiRect strip = EMBED_CALL(terminal_strip, TerminalCtx, .frame = args->frame);
    // the panel is drawn inside the work's inside, which cuts it off where it stands below its place as it arrives, in
    // its colors at the share of their alpha it has reached
    const GuiRect inside =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = place, .other = args->frame->inside);
    const GuiColor line_color = GUI_FRAME_FADED(args->palette->line, args->frame->terminal_share);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){place.x, place.y, place.width, line}, .other = inside),
               .color = line_color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect =
                   EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                              .one = (GuiRect){place.x, strip.y + strip.height, place.width, line}, .other = inside),
               .color = line_color);
    EMBED_CALL(terminal_strip_draw, TerminalCtx, .terminal_read = args->terminal_read, .frame = args->frame,
               .glyphs = args->glyphs, .raster = args->raster, .palette = args->palette, .place = inside);
    for (unsigned int view = 0u; view < panel->views; view += 1u)
    {
        if (view == 1u)
        {
            const GuiRect beside = EMBED_CALL(terminal_view, TerminalCtx, .terminal_read = args->terminal_read,
                                              .frame = args->frame, .view = 1u);
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                          .one = (GuiRect){beside.x - line, beside.y, line, beside.height},
                                          .other = inside),
                       .color = line_color);
        }
        EMBED_CALL(terminal_view_draw, TerminalCtx, .terminal_read = args->terminal_read, .frame = args->frame,
                   .glyphs = args->glyphs, .raster = args->raster, .palette = args->palette, .view = view,
                   .place = inside);
    }
}
