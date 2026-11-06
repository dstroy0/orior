// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// change_peek.c: the change peek laid under the change's line as the editor draws it, its head with the CLI's sentence
// and the controls from the right, its rows the change's lines as the CLI answers them; the box and the controls laid
// stand for the press that follows
#include "change_peek.h"

#include "../../editor/hunks.h"
#include "../frame.h"
#include "panel.h"

// The peek as last laid: its box, empty for none, and each control's square, revert first
typedef struct
{
    GuiRect box;
    GuiRect control[GUI_CHANGE_PEEK_CONTROLS];
} ChangePeekPlaces;

static ChangePeekPlaces s_places;

typedef struct
{
    const GuiChangePeekCfg *peek;
    int pixels;
    GuiRect rect;
    unsigned int which;
    GuiColor color;
} ChangePeekCtx;

// `pixels` of the page at the peek's scale
EMBED_INLINE int peek_scaled(const ChangePeekCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels, .scale = args->peek->scale);
}

// the control `which` drawn in its square `rect`: revert and close by their icons, the change before and after as a
// chevron up and down
EMBED_INLINE void peek_control_draw(const ChangePeekCtx *args)
{
    const GuiChangePeekCfg *const peek = args->peek;
    const GuiRect square = args->rect;
    if ((args->which == GUI_CHANGE_PEEK_PREVIOUS) || (args->which == GUI_CHANGE_PEEK_NEXT))
    {
        const int half = EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = peek, .pixels = 4);
        const int middle_x = square.x + (square.width / 2);
        const int middle_y = square.y + (square.height / 2);
        const int tip = (args->which == GUI_CHANGE_PEEK_PREVIOUS) ? middle_y - (half / 2) : middle_y + (half / 2);
        const int foot = (args->which == GUI_CHANGE_PEEK_PREVIOUS) ? middle_y + (half / 2) : middle_y - (half / 2);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = peek->raster, .x0 = middle_x - half,
                   .y0 = foot, .x1 = middle_x, .y1 = tip, .color = args->color, .clip = peek->bounds);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = peek->raster, .x0 = middle_x, .y0 = tip,
                   .x1 = middle_x + half, .y1 = foot, .color = args->color, .clip = peek->bounds);
        return;
    }
    if (peek->icons == NULL)
    {
        return;
    }
    // the icon's side is a few page pixels at the scale, far inside what an unsigned int and an int represent
    const unsigned int side =
        (unsigned int)EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = peek, .pixels = GUI_CHANGE_PEEK_ICON);
    const GuiMask *const mask_bits = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = peek->icons,
                                                .name = (args->which == GUI_CHANGE_PEEK_REVERT) ? "undo" : "close",
                                                .size = side, .stroke = GUI_ICON_STROKE * (float)side / GUI_ICON_UNITS);
    if (mask_bits != NULL)
    {
        EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = peek->raster, .mask_bits = mask_bits,
                   .x = square.x + ((square.width - (int)side) / 2), .y = square.y + ((square.height - (int)side) / 2),
                   .color = args->color, .clip = peek->bounds);
    }
}

void gui_change_peek_draw(const GuiChangePeekCfg *args)
{
    s_places = (ChangePeekPlaces){{0, 0, 0, 0}, {{0, 0, 0, 0}}};
    unsigned long long line = 0ULL;
    const unsigned int rows = EMBED_CALL(cli_hunks.row_count, CliHunksCfg, .line_out = &line);
    GuiRect anchor;
    if ((rows == 0u) || !EMBED_CALL(gui_editor_position_cell.position_rect, GuiEditorPositionCellCfg,
                                    .editor_read = args->editor_read, .place = args->place, .cell = args->cell,
                                    .scale = args->scale, .line = line, .col = 0ULL, .rect = &anchor))
    {
        return;
    }
    const GuiPalette *const palette = args->palette;
    const int pad = EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = GUI_CHANGE_PEEK_PAD);
    const int head = EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = GUI_CHANGE_PEEK_HEAD);
    const int row_height = EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = GUI_CHANGE_PEEK_ROW);
    const unsigned int shown = (rows < GUI_CHANGE_PEEK_ROWS) ? rows : GUI_CHANGE_PEEK_ROWS;
    // the rows shown are at most GUI_CHANGE_PEEK_ROWS, far inside an int
    const int pixel_height = head + ((int)shown * row_height);
    const int right = args->place.x + args->place.width - pad;
    // under the change's line where the bounds reach, else above it, inside the bounds
    int top = anchor.y + anchor.height;
    if (top + pixel_height > args->bounds.y + args->bounds.height)
    {
        top = anchor.y - pixel_height;
    }
    top = (top < args->bounds.y) ? args->bounds.y : top;
    const GuiRect box = {anchor.x, top, right - anchor.x, pixel_height};
    if ((box.width <= 0) || (box.height <= 0))
    {
        return;
    }
    s_places.box = box;
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = args->raster, .palette = palette, .rect = box,
               .scale = args->scale);
    // the head: the sentence from the left, the controls from the right, a line under them
    const unsigned int text_size =
        (unsigned int)EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = (int)GUI_CHANGE_PEEK_TEXT_SIZE);
    const GuiRect head_rect = {box.x, box.y, box.width, head};
    const GuiRect clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = box, .other = args->bounds);
    unsigned long long length = 0ULL;
    const char *const title = EMBED_CALL(cli_hunks.title, CliHunksCfg, .length_out = &length);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                     .glyphs = args->glyphs, .face = GUI_FACE_HEAD, .size = text_size, .x_f32 = (float)(box.x + pad),
                     .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs,
                                     .face = GUI_FACE_HEAD, .size = text_size, .top = head_rect.y,
                                     .pixel_height = head_rect.height),
                     .text = title, .length = length, .color = palette->text, .clip = clip);
    const int control = EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = GUI_CHANGE_PEEK_CONTROL);
    for (unsigned int which = GUI_CHANGE_PEEK_CLOSE; which >= GUI_CHANGE_PEEK_REVERT; which -= 1u)
    {
        // the controls are four, far inside an int
        const int x = box.x + box.width - pad - ((int)(GUI_CHANGE_PEEK_CLOSE - which + 1u) * control);
        const GuiRect square = {x, box.y + ((head - control) / 2), control, control};
        s_places.control[which - 1u] = square;
        EMBED_CALL(peek_control_draw, ChangePeekCtx, .peek = args, .rect = square, .which = which,
                   .color = palette->text_2);
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){box.x, box.y + head - 1, box.width, 1}, .other = clip),
               .color = palette->widget_line);
    // the change's lines, a sign then the text, as many as the box shows
    const unsigned int code_size =
        (unsigned int)EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = (int)GUI_CODE_SIZE);
    for (unsigned int at = 0u; at < shown; at += 1u)
    {
        CliHunkRow row;
        if (!EMBED_CALL(cli_hunks.row, CliHunksCfg, .at = at, .row = &row))
        {
            break;
        }
        // the row's place is below GUI_CHANGE_PEEK_ROWS, far inside an int
        const int row_y = box.y + head + ((int)at * row_height);
        const int baseline =
            EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs, .face = GUI_FACE_CODE,
                       .size = code_size, .top = row_y, .pixel_height = row_height);
        const GuiColor color = row.removed ? palette->mark[0] : palette->mark[1];
        const float after_sign = EMBED_CALL(
            gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster, .glyphs = args->glyphs,
            .face = GUI_FACE_CODE_BOLD, .size = code_size, .x_f32 = (float)(box.x + pad), .y = baseline,
            .text = row.removed ? "-" : "+", .length = 1ULL, .color = color, .clip = clip);
        char text[GUI_CHANGE_PEEK_LINE_BYTES];
        VerbaLinea out = {text, sizeof(text), 0ULL};
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .units = row.units,
                   .count = (row.count < GUI_CHANGE_PEEK_LINE_UNITS) ? row.count : GUI_CHANGE_PEEK_LINE_UNITS,
                   .out = &out);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                         .glyphs = args->glyphs, .face = GUI_FACE_CODE, .size = code_size,
                         .x_f32 = after_sign + args->cell, .y = baseline, .text = text, .length = out.at,
                         .color = color, .clip = clip);
    }
}

int gui_change_peek_hit(const GuiChangePeekCfg *args)
{
    // a change closed since the box was laid leaves no box to press
    if ((EMBED_CALL(cli_hunks.row_count, CliHunksCfg, .line_out = NULL) == 0u) ||
        !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = s_places.box, .x = args->x, .y = args->y))
    {
        return 0;
    }
    *args->action_out = GUI_CHANGE_PEEK_BOX;
    for (unsigned int at = 0u; at < GUI_CHANGE_PEEK_CONTROLS; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = s_places.control[at], .x = args->x,
                       .y = args->y))
        {
            *args->action_out = GUI_CHANGE_PEEK_REVERT + at;
        }
    }
    return 1;
}

GuiRect gui_change_peek_box(const GuiChangePeekCfg *args)
{
    (void)args;
    return s_places.box;
}
