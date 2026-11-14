// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// change_peek.c: the change peek positioned under the change's line as the editor draws it, its head with the sentence over
// the change and the controls from the right, its rows the change's lines as the store answers them; the box and the
// controls positioned are for the press that follows. The peek retains the key and the place of the change displayed and when it
// was displayed; every read of the change itself is one pass of the store's diff
#include "change_peek.h"

#include "../../../includes/window/frame.h"
#include "../../../includes/window/panel.h"
#include "../../cli/src/command_line/cli_internal.h"
#include "../../cli/src/editor/document_changes.h"

// The peek as last positioned: its box, empty for none, and each control's square, revert first
typedef struct
{
    GuiRect box;
    GuiRect control[GUI_CHANGE_PEEK_CONTROLS];
} ChangePeekPlaces;

static ChangePeekPlaces s_places;

// The change displayed: the key of the text it is in, its place among the text's changes, CLI_CHANGE_NONE for none, and
// the millisecond it was displayed at
typedef struct
{
    char key[CLI_NAME_MAX_LEN];
    unsigned int at;
    unsigned long long displayed_ms;
} ChangePeekShown;

static ChangePeekShown s_displayed = {{0}, CLI_CHANGE_NONE, 0ULL};

typedef struct
{
    const GuiChangePeekCfg *peek;
    int pixels;
    GuiRect rect;
    unsigned int which;
    GuiColor color;
    unsigned int at;
    const char *key;
} ChangePeekCtx;

// `pixels` of the page at the peek's scale
EMBED_INLINE int peek_scaled(const ChangePeekCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels, .scale = args->peek->scale);
}

// the change `at` of the text under `key` displayed from now
EMBED_INLINE void peek_displayed(const ChangePeekCtx *args)
{
    VerbaLinea named = {s_displayed.key, sizeof(s_displayed.key), 0ULL};
    mmgr_line_put(&named, args->key);
    (void)mmgr_line_finish(&named);
    s_displayed.at = args->at;
    s_displayed.displayed_ms = cli_clock_milliseconds();
}

// the change displayed read anew off the store, into `change_out` with how many changes are into `count_out`: 1, or 0
// where none is displayed or it is gone, which closes the peek
EMBED_INLINE int peek_change(CliChange *change_out, unsigned int *count_out)
{
    if (s_displayed.at == CLI_CHANGE_NONE)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = s_displayed.key, .at = s_displayed.at,
                    .change_out = change_out, .count_out = count_out))
    {
        s_displayed.at = CLI_CHANGE_NONE;
        return 0;
    }
    return 1;
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
    const GuiMask *const mask_bits = EMBED_CALL(gui_icons.icon, GuiIconsCfg, .icons = peek->icons,
                                                .name = (args->which == GUI_CHANGE_PEEK_REVERT) ? "undo" : "close",
                                                .size = side, .stroke = GUI_ICON_STROKE * (float)side / GUI_ICON_UNITS);
    if (mask_bits != NULL)
    {
        EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = peek->raster, .mask_bits = mask_bits,
                   .x = square.x + ((square.width - (int)side) / 2), .y = square.y + ((square.height - (int)side) / 2),
                   .color = args->color, .clip = peek->bounds);
    }
}

// the line the peek is under: the change's last newer line, or the line above lines taken out
EMBED_INLINE unsigned long long peek_under(const CliChange *change)
{
    return (change->now_to > change->now_from)
               ? (unsigned long long)change->now_to - 1ULL
               : (unsigned long long)EMBED_CALL(cli_document_change.marked_from, CliChangeCfg, .change = change);
}

void gui_change_peek_draw(const GuiChangePeekCfg *args)
{
    s_places = (ChangePeekPlaces){{0, 0, 0, 0}, {{0, 0, 0, 0}}};
    CliChange change;
    unsigned int count = 0u;
    if (!peek_change(&change, &count))
    {
        return;
    }
    const unsigned int removed = change.then_to - change.then_from;
    const unsigned int rows = removed + (change.now_to - change.now_from);
    GuiRect anchor;
    if ((rows == 0u) || !EMBED_CALL(gui_editor_position_cell.position_rect, GuiEditorPositionCellCfg,
                                    .editor_read = args->editor_read, .place = args->place, .cell = args->cell,
                                    .scale = args->scale, .line = peek_under(&change), .col = 0ULL, .rect = &anchor))
    {
        return;
    }
    const GuiPalette *const palette = args->palette;
    const int pad = EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = GUI_CHANGE_PEEK_PAD);
    const int head = EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = GUI_CHANGE_PEEK_HEAD);
    const int row_height = EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = GUI_CHANGE_PEEK_ROW);
    const unsigned int displayed = (rows < GUI_CHANGE_PEEK_ROWS) ? rows : GUI_CHANGE_PEEK_ROWS;
    // the rows displayed are at max GUI_CHANGE_PEEK_ROWS, far inside an int
    const int pixel_height = head + ((int)displayed * row_height);
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
    char title[64];
    VerbaLinea title_line = {title, sizeof(title), 0ULL};
    mmgr_line_put(&title_line, "Change ");
    mmgr_line_decimal(&title_line, s_displayed.at + 1u, 1u);
    mmgr_line_put(&title_line, " of ");
    mmgr_line_decimal(&title_line, count, 1u);
    (void)mmgr_line_finish(&title_line);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                     .glyphs = args->glyphs, .face = GUI_FACE_HEAD, .size = text_size, .x_f32 = (float)(box.x + pad),
                     .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs,
                                     .face = GUI_FACE_HEAD, .size = text_size, .top = head_rect.y,
                                     .pixel_height = head_rect.height),
                     .text = title, .length = title_line.at, .color = palette->text, .clip = clip);
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
    // the change's lines, a sign then the text, as many as the box shows: the older lines first, read off the file as
    // the last commit has it, then the newer, read off the text
    const unsigned int code_size =
        (unsigned int)EMBED_CALL(peek_scaled, ChangePeekCtx, .peek = args, .pixels = (int)GUI_CODE_SIZE);
    for (unsigned int at = 0u; at < displayed; at += 1u)
    {
        const int is_removed = at < removed;
        unsigned short units[GUI_CHANGE_PEEK_LINE_UNITS];
        unsigned long long units_count = 0ULL;
        if (is_removed)
        {
            (void)EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg, .key = s_displayed.key,
                             .line = change.then_from + at, .count = &units_count, .dst = units,
                             .capacity = GUI_CHANGE_PEEK_LINE_UNITS);
        }
        else
        {
            (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = s_displayed.key,
                             .line = change.now_from + (at - removed), .count = &units_count, .dst = units,
                             .capacity = GUI_CHANGE_PEEK_LINE_UNITS);
        }
        // the row's place is below GUI_CHANGE_PEEK_ROWS, far inside an int
        const int row_y = box.y + head + ((int)at * row_height);
        const int baseline =
            EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs, .face = GUI_FACE_CODE,
                       .size = code_size, .top = row_y, .pixel_height = row_height);
        const GuiColor color = is_removed ? palette->mark[0] : palette->mark[1];
        const float after_sign = EMBED_CALL(
            gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster, .glyphs = args->glyphs,
            .face = GUI_FACE_CODE_BOLD, .size = code_size, .x_f32 = (float)(box.x + pad), .y = baseline,
            .text = is_removed ? "-" : "+", .length = 1ULL, .color = color, .clip = clip);
        char text[GUI_CHANGE_PEEK_LINE_BYTES];
        VerbaLinea out = {text, sizeof(text), 0ULL};
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .units = units,
                   .count = (units_count < GUI_CHANGE_PEEK_LINE_UNITS) ? units_count : GUI_CHANGE_PEEK_LINE_UNITS,
                   .out = &out);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster,
                         .glyphs = args->glyphs, .face = GUI_FACE_CODE, .size = code_size,
                         .x_f32 = after_sign + args->cell, .y = baseline, .text = text, .length = out.at,
                         .color = color, .clip = clip);
    }
}

int gui_change_peek_hit(const GuiChangePeekCfg *args)
{
    // a change closed since the box was positioned leaves no box to press
    if ((s_displayed.at == CLI_CHANGE_NONE) ||
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

int gui_change_peek_show(const GuiChangePeekCfg *args)
{
    unsigned int at = CLI_CHANGE_NONE;
    if ((args->key == NULL) || !EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = args->key,
                                           .at = CLI_CHANGE_NONE, .line = args->line, .at_out = &at))
    {
        return 0;
    }
    EMBED_CALL(peek_displayed, ChangePeekCtx, .key = args->key, .at = at);
    return 1;
}

int gui_change_peek_step(const GuiChangePeekCfg *args)
{
    if (args->key == NULL)
    {
        return 0;
    }
    // from the change displayed where one is displayed on this key, else from the line
    const int same = (s_displayed.at != CLI_CHANGE_NONE) &&
                     cellul.eq(&(CatenaFinitaCfg){.src = s_displayed.key, .other = args->key, .cap = CLI_NAME_MAX_LEN});
    CliChange change;
    unsigned int at = CLI_CHANGE_NONE;
    if (!EMBED_CALL(cli_document_change.step, CliChangeCfg, .key = args->key, .at = same ? s_displayed.at : CLI_CHANGE_NONE,
                    .line = args->line, .by = args->by, .change_out = &change, .at_out = &at))
    {
        return 0;
    }
    EMBED_CALL(peek_displayed, ChangePeekCtx, .key = args->key, .at = at);
    if (args->line_out != NULL)
    {
        *args->line_out = EMBED_CALL(cli_document_change.marked_from, CliChangeCfg, .change = &change);
    }
    return 1;
}

void gui_change_peek_close(const GuiChangePeekCfg *args)
{
    (void)args;
    s_displayed.at = CLI_CHANGE_NONE;
}

int gui_change_peek_key(const GuiChangePeekCfg *args)
{
    if ((s_displayed.at == CLI_CHANGE_NONE) ||
        !cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        return 0;
    }
    s_displayed.at = CLI_CHANGE_NONE;
    return 1;
}

int gui_change_peek_settled(const GuiChangePeekCfg *args)
{
    (void)args;
    return (s_displayed.at == CLI_CHANGE_NONE) ||
           (cli_clock_milliseconds() >= s_displayed.displayed_ms + GUI_CHANGE_PEEK_SETTLE_MS);
}

int gui_change_peek_revert(const GuiChangePeekCfg *args)
{
    CliChange change;
    unsigned int count = 0u;
    if (!peek_change(&change, &count))
    {
        mmgr_line_put(args->err_msg, "no change is displayed");
        return 0;
    }
    return EMBED_CALL(cli_document_change.revert, CliChangeCfg, .key = s_displayed.key, .change = &change,
                      .edit_out = args->edit_out, .err_msg = args->err_msg);
}

void gui_change_peek_revert_end(const GuiChangePeekCfg *args)
{
    EMBED_CALL(cli_document_change.revert_end, CliChangeCfg, .edit_out = args->edit_out);
}
