// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diffview.c: the compare drawn over the editor from the CLI's rows and sentences, the rows in the window of the scroll
// alone, each found by the row height; and its presses, wheel and keys handed to the CLI's steps and options
#include "diffview.h"

#include "../../code/diff_view.h"
#include "../../command_line/cli_internal.h"
#include "../../editor/document.h"

typedef struct
{
    const GuiWorkCfg *work;
    int pixels;
    GuiRect rect;
    GuiRect clip;
    unsigned int face;
    unsigned int size;
    float x_f32;
    const char *text;
    unsigned long long length;
    GuiColor color;
    unsigned int which;
    int rows;
    int direction;
    unsigned int row;
    int right;
    const CliDiffViewRow *row_read;
    int on;
    int enabled;
} DiffviewCtx;

// `pixels` of the page at the frame's scale
EMBED_INLINE int diffview_scaled(const DiffviewCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels,
                      .scale = args->work->frame->scale);
}

// the bar at the view's top
EMBED_INLINE GuiRect diffview_bar(const DiffviewCtx *args)
{
    const GuiRect rect = args->work->rect;
    return (GuiRect){rect.x, rect.y, rect.width,
                     EMBED_CALL(diffview_scaled, DiffviewCtx, .work = args->work, .pixels = GUI_DIFFVIEW_BAR)};
}

// the summary's row under the bar, of no height while the CLI answers none
EMBED_INLINE GuiRect diffview_summary(const DiffviewCtx *args)
{
    const GuiRect bar = EMBED_CALL(diffview_bar, DiffviewCtx, .work = args->work);
    unsigned long long length = 0ULL;
    (void)EMBED_CALL(cli_diff_view_at_row_length.summary, CliDiffViewAtRowLengthCfg, .length_out = &length);
    return (GuiRect){bar.x, bar.y + bar.height, bar.width,
                     (length != 0ULL)
                         ? EMBED_CALL(diffview_scaled, DiffviewCtx, .work = args->work, .pixels = GUI_DIFFVIEW_SUMMARY)
                         : 0};
}

// the body under the bar and the summary
EMBED_INLINE GuiRect diffview_body(const DiffviewCtx *args)
{
    const GuiRect rect = args->work->rect;
    const GuiRect summary = EMBED_CALL(diffview_summary, DiffviewCtx, .work = args->work);
    const int top = summary.y + summary.height;
    return (GuiRect){rect.x, top, rect.width, rect.y + rect.height - top};
}

// how many whole rows the body shows
EMBED_INLINE unsigned int diffview_shown(const DiffviewCtx *args)
{
    const GuiRect body = EMBED_CALL(diffview_body, DiffviewCtx, .work = args->work);
    const int row = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = args->work, .pixels = GUI_DIFFVIEW_ROW);
    // a height and a row's both stand positive where they are divided
    return ((row > 0) && (body.height > 0)) ? (unsigned int)(body.height / row) : 0u;
}

// the rectangle of the control `which`: the tools from the bar's right, close last, then the structure box and the
// white space box before them
EMBED_INLINE GuiRect diffview_control(const DiffviewCtx *args)
{
    const GuiRect bar = EMBED_CALL(diffview_bar, DiffviewCtx, .work = args->work);
    const int pad = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = args->work, .pixels = GUI_DIFFVIEW_PAD);
    const int tool = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = args->work, .pixels = GUI_DIFFVIEW_TOOL);
    const int box = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = args->work, .pixels = GUI_DIFFVIEW_BOX_WIDTH);
    const int y = bar.y + ((bar.height - tool) / 2);
    const int right = bar.x + bar.width - pad;
    switch (args->which)
    {
    case GUI_DIFFVIEW_CLOSE:
        return (GuiRect){right - tool, y, tool, tool};
    case GUI_DIFFVIEW_PREVIOUS:
        return (GuiRect){right - (2 * tool), y, tool, tool};
    case GUI_DIFFVIEW_NEXT:
        return (GuiRect){right - (3 * tool), y, tool, tool};
    case GUI_DIFFVIEW_STRUCTURE:
        return (GuiRect){right - (3 * tool) - pad - box, bar.y, box, bar.height};
    default:
        return (GuiRect){right - (3 * tool) - pad - (2 * box), bar.y, box, bar.height};
    }
}

// `length` bytes of `text` drawn from `x_f32` in `face` at `size` page pixels, its baseline centered in `rect`, cut to
// `clip`: the x after it
EMBED_INLINE float diffview_text(const DiffviewCtx *args)
{
    const GuiWorkCfg *const work = args->work;
    // a text size is far inside an int and stays positive at any scale
    const unsigned int size =
        (unsigned int)EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = (int)args->size);
    return EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = work->raster, .glyphs = work->glyphs,
        .face = args->face, .size = size, .x_f32 = args->x_f32,
        .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = work->glyphs, .face = args->face,
                        .size = size, .top = args->rect.y, .pixel_height = args->rect.height),
        .text = args->text, .length = args->length, .color = args->color,
        .clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->clip, .other = work->rect));
}

// `rect` filled with `color`, no further than the view
EMBED_INLINE void diffview_fill(const DiffviewCtx *args)
{
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->work->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->rect,
                                  .other = args->work->rect),
               .color = args->color);
}

// the tool `which` drawn as the icon `text`
EMBED_INLINE void diffview_tool_draw(const DiffviewCtx *args)
{
    const GuiWorkCfg *const work = args->work;
    const GuiRect tool = EMBED_CALL(diffview_control, DiffviewCtx, .work = work, .which = args->which);
    // an icon's side is far inside an int and stays positive at any scale
    const unsigned int icon =
        (unsigned int)EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = GUI_DIFFVIEW_TOOL_ICON);
    const GuiMask *const mask_bits =
        (work->icons != NULL) ? EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = work->icons, .name = args->text,
                                           .size = icon, .stroke = GUI_ICON_STROKE * (float)icon / GUI_ICON_UNITS)
                              : NULL;
    if (mask_bits != NULL)
    {
        // the icon's side is far inside what an int represents
        EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = work->raster, .mask_bits = mask_bits,
                   .x = tool.x + ((tool.width - (int)icon) / 2), .y = tool.y + ((tool.height - (int)icon) / 2),
                   .color = work->palette->text_2, .clip = work->rect);
    }
}

// the box `which` drawn with its words, `which` the bar's part the CLI answers them for: pressed where `on`, quieter
// where not `enabled`
EMBED_INLINE void diffview_box_draw(const DiffviewCtx *args)
{
    const GuiWorkCfg *const work = args->work;
    const GuiPalette *const palette = work->palette;
    const GuiRect area =
        EMBED_CALL(diffview_control, DiffviewCtx, .work = work,
                   .which = (args->which == CLI_DIFF_VIEW_BAR_SPACE) ? GUI_DIFFVIEW_SPACE : GUI_DIFFVIEW_STRUCTURE);
    const int side = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = GUI_DIFFVIEW_BOX);
    const int one = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = 1);
    const GuiRect box = {area.x, area.y + ((area.height - side) / 2), side, side};
    const GuiColor border = args->enabled ? palette->line : palette->text_3;
    EMBED_CALL(diffview_fill, DiffviewCtx, .work = work, .rect = box, .color = border);
    EMBED_CALL(diffview_fill, DiffviewCtx, .work = work,
               .rect = (GuiRect){box.x + one, box.y + one, box.width - (2 * one), box.height - (2 * one)},
               .color = args->on ? palette->signal : palette->editor_background);
    unsigned long long length = 0ULL;
    const char *const words = EMBED_CALL(cli_diff_view_at_row_length.bar, CliDiffViewAtRowLengthCfg,
                                         .part = args->which, .length_out = &length);
    (void)EMBED_CALL(
        diffview_text, DiffviewCtx, .work = work, .face = GUI_FACE_HEAD, .size = GUI_DIFFVIEW_BAR_SIZE,
        .x_f32 = (float)(box.x + side +
                         EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = GUI_DIFFVIEW_BOX_GAP)),
        .text = words, .length = length, .color = args->enabled ? palette->text_2 : palette->text_3, .rect = area,
        .clip = area);
}

// the bar drawn: the label, the sides sentence and the where sentence from its left, cut before the boxes; the boxes
// as the CLI answers their options; the tools; and the line under it
EMBED_INLINE void diffview_bar_draw(const DiffviewCtx *args)
{
    const GuiWorkCfg *const work = args->work;
    const GuiPalette *const palette = work->palette;
    const GuiRect bar = EMBED_CALL(diffview_bar, DiffviewCtx, .work = work);
    const int pad = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = GUI_DIFFVIEW_PAD);
    const GuiRect space = EMBED_CALL(diffview_control, DiffviewCtx, .work = work, .which = GUI_DIFFVIEW_SPACE);
    const GuiRect words = {bar.x, bar.y, space.x - pad - bar.x, bar.height};
    unsigned long long length = 0ULL;
    const char *text = EMBED_CALL(cli_diff_view_at_row_length.bar, CliDiffViewAtRowLengthCfg,
                                  .part = CLI_DIFF_VIEW_BAR_LABEL, .length_out = &length);
    float x = EMBED_CALL(diffview_text, DiffviewCtx, .work = work, .face = GUI_FACE_HEAD_BOLD,
                         .size = GUI_DIFFVIEW_BAR_SIZE, .x_f32 = (float)(bar.x + pad), .text = text, .length = length,
                         .color = palette->text, .rect = bar, .clip = words);
    text = EMBED_CALL(cli_diff_view_at_row_length.bar, CliDiffViewAtRowLengthCfg, .part = CLI_DIFF_VIEW_BAR_SIDES,
                      .length_out = &length);
    x = EMBED_CALL(diffview_text, DiffviewCtx, .work = work, .face = GUI_FACE_HEAD, .size = GUI_DIFFVIEW_BAR_SIZE,
                   .x_f32 = x + (float)pad, .text = text, .length = length, .color = palette->text_2, .rect = bar,
                   .clip = words);
    text = EMBED_CALL(cli_diff_view_at_row_length.where, CliDiffViewAtRowLengthCfg, .length_out = &length);
    (void)EMBED_CALL(diffview_text, DiffviewCtx, .work = work, .face = GUI_FACE_HEAD, .size = GUI_DIFFVIEW_BAR_SIZE,
                     .x_f32 = x + (float)pad, .text = text, .length = length, .color = palette->text_3, .rect = bar,
                     .clip = words);
    static const unsigned int s_options[2] = {CLI_DIFF_VIEW_SPACE, CLI_DIFF_VIEW_STRUCTURE};
    static const unsigned int s_parts[2] = {CLI_DIFF_VIEW_BAR_SPACE, CLI_DIFF_VIEW_BAR_STRUCTURE};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        int on = 0;
        int enabled = 0;
        EMBED_CALL(cli_diff_view_option_on.option_get, CliDiffViewOptionOnCfg, .option = s_options[at], .on_out = &on,
                   .enabled_out = &enabled);
        EMBED_CALL(diffview_box_draw, DiffviewCtx, .work = work, .which = s_parts[at], .on = on, .enabled = enabled);
    }
    EMBED_CALL(diffview_tool_draw, DiffviewCtx, .work = work, .which = GUI_DIFFVIEW_NEXT, .text = "pull");
    EMBED_CALL(diffview_tool_draw, DiffviewCtx, .work = work, .which = GUI_DIFFVIEW_PREVIOUS, .text = "push");
    EMBED_CALL(diffview_tool_draw, DiffviewCtx, .work = work, .which = GUI_DIFFVIEW_CLOSE, .text = "close");
    EMBED_CALL(diffview_fill, DiffviewCtx, .work = work, .rect = (GuiRect){bar.x, bar.y + bar.height - 1, bar.width, 1},
               .color = palette->line);
    const GuiRect summary = EMBED_CALL(diffview_summary, DiffviewCtx, .work = work);
    if (summary.height > 0)
    {
        text = EMBED_CALL(cli_diff_view_at_row_length.summary, CliDiffViewAtRowLengthCfg, .length_out = &length);
        (void)EMBED_CALL(diffview_text, DiffviewCtx, .work = work, .face = GUI_FACE_HEAD,
                         .size = GUI_DIFFVIEW_SUMMARY_SIZE, .x_f32 = (float)(summary.x + pad), .text = text,
                         .length = length, .color = palette->text_2, .rect = summary, .clip = summary);
    }
}

// the tint of a half, `right` the right one, by what the row `row_read` is: a changed row's in the third mark's, a line
// put in the second's on the right, a line taken out the first's on the left, each at the tint's alpha; none for a
// row both share
EMBED_INLINE GuiColor diffview_tint(const DiffviewCtx *args)
{
    const GuiPalette *const palette = args->work->palette;
    const unsigned int kind = args->row_read->kind;
    const GuiColor mark = (kind == CLI_CHANGED)                               ? palette->mark[2]
                          : ((kind == CLI_DIFF_VIEW_ADDED) && args->right)    ? palette->mark[1]
                          : ((kind == CLI_DIFF_VIEW_REMOVED) && !args->right) ? palette->mark[0]
                                                                              : 0u;
    return (mark != 0u) ? ((mark & 0x00ffffffu) | (GUI_DIFFVIEW_TINT << 24u)) : 0u;
}

// a half of the row `row_read` drawn in `rect`, `right` the right one: a gap where the side has no line, else its tint,
// its number counted from 1 and its line's units as UTF-8, as many as the half shows
EMBED_INLINE void diffview_half_draw(const DiffviewCtx *args)
{
    const GuiWorkCfg *const work = args->work;
    const GuiPalette *const palette = work->palette;
    const CliDiffViewRow *const row = args->row_read;
    const GuiRect half = args->rect;
    const int has = args->right ? row->has_right : row->has_left;
    if (!has)
    {
        EMBED_CALL(diffview_fill, DiffviewCtx, .work = work, .rect = half,
                   .color = (palette->line & 0x00ffffffu) | (GUI_DIFFVIEW_TINT << 24u));
        return;
    }
    const GuiColor tint = EMBED_CALL(diffview_tint, DiffviewCtx, .work = work, .row_read = row, .right = args->right);
    if (tint != 0u)
    {
        EMBED_CALL(diffview_fill, DiffviewCtx, .work = work, .rect = half, .color = tint);
    }
    const int pad = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = GUI_DIFFVIEW_PAD);
    const int number = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = GUI_DIFFVIEW_NUMBER);
    char digits[12];
    VerbaLinea line = {digits, sizeof(digits), 0ULL};
    mmgr_line_decimal(&line, (args->right ? row->right_line : row->left_line) + 1ULL, 1u);
    (void)EMBED_CALL(diffview_text, DiffviewCtx, .work = work, .face = GUI_FACE_CODE, .size = GUI_DIFFVIEW_NUMBER_SIZE,
                     .x_f32 = (float)(half.x + pad), .text = digits, .length = line.at, .color = palette->text_3,
                     .rect = half, .clip = (GuiRect){half.x, half.y, number, half.height});
    const unsigned long long count = args->right ? row->right_count : row->left_count;
    char text[GUI_DIFFVIEW_LINE_BYTES];
    VerbaLinea utf8 = {text, sizeof(text), 0ULL};
    EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg,
               .units = args->right ? row->right_units : row->left_units,
               .count = (count < GUI_DIFFVIEW_LINE_UNITS) ? count : GUI_DIFFVIEW_LINE_UNITS, .out = &utf8);
    (void)EMBED_CALL(diffview_text, DiffviewCtx, .work = work, .face = GUI_FACE_CODE, .size = GUI_DIFFVIEW_LINE_SIZE,
                     .x_f32 = (float)(half.x + number), .text = text, .length = utf8.at, .color = palette->text,
                     .rect = half, .clip = (GuiRect){half.x + number, half.y, half.width - number, half.height});
}

// the body drawn: the CLI's sentence in place of rows where it answers one, else the rows from the one scrolled to,
// as many as the body shows, each in two halves
EMBED_INLINE void diffview_body_draw(const DiffviewCtx *args)
{
    const GuiWorkCfg *const work = args->work;
    const GuiRect body = EMBED_CALL(diffview_body, DiffviewCtx, .work = work);
    const int pad = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = GUI_DIFFVIEW_PAD);
    const int height = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = work, .pixels = GUI_DIFFVIEW_ROW);
    unsigned long long length = 0ULL;
    const char *const empty =
        EMBED_CALL(cli_diff_view_at_row_length.empty, CliDiffViewAtRowLengthCfg, .length_out = &length);
    if (length != 0ULL)
    {
        const GuiRect band = {body.x, body.y + pad, body.width, height};
        (void)EMBED_CALL(diffview_text, DiffviewCtx, .work = work, .face = GUI_FACE_HEAD,
                         .size = GUI_DIFFVIEW_SUMMARY_SIZE, .x_f32 = (float)(body.x + pad), .text = empty,
                         .length = length, .color = work->palette->text_2, .rect = band, .clip = band);
        return;
    }
    const unsigned int rows = EMBED_CALL(cli_diff_view_at_row_length.rows, CliDiffViewAtRowLengthCfg, .at = 0u);
    const unsigned int shown = EMBED_CALL(diffview_shown, DiffviewCtx, .work = work) + 1u;
    const unsigned int first = work->work_read->scroll;
    const int left_width = body.width / 2;
    for (unsigned int at = first; (at < rows) && (at < first + shown); at += 1u)
    {
        CliDiffViewRow row;
        if (!EMBED_CALL(cli_diff_view_at_row_length.row, CliDiffViewAtRowLengthCfg, .at = at, .row = &row))
        {
            break;
        }
        // the rows drawn are fewer than the body shows, far inside an int
        const int y = body.y + ((int)(at - first) * height);
        EMBED_CALL(diffview_half_draw, DiffviewCtx, .work = work, .row_read = &row, .right = 0,
                   .rect = (GuiRect){body.x, y, left_width, height});
        EMBED_CALL(diffview_half_draw, DiffviewCtx, .work = work, .row_read = &row, .right = 1,
                   .rect = (GuiRect){body.x + left_width, y, body.width - left_width, height});
    }
    EMBED_CALL(diffview_fill, DiffviewCtx, .work = work, .rect = (GuiRect){body.x + left_width, body.y, 1, body.height},
               .color = work->palette->line);
}

static void diffview_draw(const GuiWorkCfg *args)
{
    EMBED_CALL(diffview_bar_draw, DiffviewCtx, .work = args);
    EMBED_CALL(diffview_body_draw, DiffviewCtx, .work = args);
}

static int diffview_hit(const GuiWorkCfg *args)
{
    static const unsigned int s_controls[5] = {GUI_DIFFVIEW_NEXT, GUI_DIFFVIEW_PREVIOUS, GUI_DIFFVIEW_CLOSE,
                                               GUI_DIFFVIEW_SPACE, GUI_DIFFVIEW_STRUCTURE};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(diffview_control, DiffviewCtx, .work = args, .which = s_controls[at]),
                       .x = args->x, .y = args->y))
        {
            *args->index_out = s_controls[at];
            return 1;
        }
    }
    const GuiRect body = EMBED_CALL(diffview_body, DiffviewCtx, .work = args);
    const int height = EMBED_CALL(diffview_scaled, DiffviewCtx, .work = args, .pixels = GUI_DIFFVIEW_ROW);
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = body, .x = args->x, .y = args->y) ||
        (height <= 0))
    {
        return 0;
    }
    // the pixel stands below the body's top, and the rows it is down are far fewer than an unsigned int counts
    const unsigned int row = args->work_read->scroll + (unsigned int)((args->y - body.y) / height);
    if (row >= EMBED_CALL(cli_diff_view_at_row_length.rows, CliDiffViewAtRowLengthCfg, .at = 0u))
    {
        return 0;
    }
    *args->index_out = GUI_DIFFVIEW_ROWS + row;
    return 1;
}

// the scroll moved by `rows` rows, no further than the last row fills the body, and the last change whose first row is
// in view or above it stepped to, where `on`, for the where sentence to follow the rows in view
EMBED_INLINE void diffview_scrolled(const DiffviewCtx *args)
{
    const GuiWorkCfg *const work = args->work;
    const unsigned int rows = EMBED_CALL(cli_diff_view_at_row_length.rows, CliDiffViewAtRowLengthCfg, .at = 0u);
    const unsigned int shown = EMBED_CALL(diffview_shown, DiffviewCtx, .work = work);
    const unsigned int most = (rows > shown) ? rows - shown : 0u;
    // the scroll and the rows are far fewer than an int counts
    const long long required = (long long)work->work->scroll + (long long)args->rows;
    // the clamped scroll lies between 0 and `most`, inside an unsigned int
    work->work->scroll = (required < 0LL) ? 0u : ((required > (long long)most) ? most : (unsigned int)required);
    if (args->on)
    {
        (void)EMBED_CALL(cli_diff_view_at_row_length.step, CliDiffViewAtRowLengthCfg, .direction = 0,
                         .at = work->work->scroll + ((shown != 0u) ? (shown - 1u) : 0u));
    }
}

// the scroll set to show the row `row` in the body's middle, no further than the last row fills the body
EMBED_INLINE void diffview_centered(const DiffviewCtx *args)
{
    const GuiWorkCfg *const work = args->work;
    const unsigned int shown = EMBED_CALL(diffview_shown, DiffviewCtx, .work = work);
    work->work->scroll = 0u;
    const unsigned int top = (args->row > (shown / 2u)) ? args->row - (shown / 2u) : 0u;
    // the top row is far fewer than an int counts
    EMBED_CALL(diffview_scrolled, DiffviewCtx, .work = work, .rows = (int)top, .on = 0);
}

// the change stepped to in `direction`, the scroll following
EMBED_INLINE void diffview_stepped(const DiffviewCtx *args)
{
    const unsigned int row = EMBED_CALL(cli_diff_view_at_row_length.step, CliDiffViewAtRowLengthCfg,
                                        .direction = args->direction, .at = args->work->work->scroll);
    EMBED_CALL(diffview_centered, DiffviewCtx, .work = args->work, .row = row);
}

static unsigned int diffview_press(const GuiWorkCfg *args)
{
    switch (args->index)
    {
    case GUI_DIFFVIEW_NEXT:
        EMBED_CALL(diffview_stepped, DiffviewCtx, .work = args, .direction = 1);
        return GUI_WORK_TAKEN;
    case GUI_DIFFVIEW_PREVIOUS:
        EMBED_CALL(diffview_stepped, DiffviewCtx, .work = args, .direction = -1);
        return GUI_WORK_TAKEN;
    case GUI_DIFFVIEW_CLOSE:
        return GUI_WORK_CLOSES;
    case GUI_DIFFVIEW_SPACE:
    case GUI_DIFFVIEW_STRUCTURE: {
        const unsigned int shown = EMBED_CALL(diffview_shown, DiffviewCtx, .work = args);
        const unsigned int row =
            EMBED_CALL(cli_diff_view_option_on.option_toggle, CliDiffViewOptionOnCfg,
                       .option = (args->index == GUI_DIFFVIEW_SPACE) ? CLI_DIFF_VIEW_SPACE : CLI_DIFF_VIEW_STRUCTURE,
                       .at = args->work->scroll + (shown / 2u));
        EMBED_CALL(diffview_centered, DiffviewCtx, .work = args, .row = row);
        return GUI_WORK_TAKEN;
    }
    default:
        return GUI_WORK_TAKEN;
    }
}

// a turn of rows scrolls them, the where sentence following; a turn of none brings the first change into sight
static unsigned int diffview_wheel(const GuiWorkCfg *args)
{
    if (args->rows == 0)
    {
        const unsigned int row =
            EMBED_CALL(cli_diff_view_at_row_length.step, CliDiffViewAtRowLengthCfg, .direction = 0, .at = 0u);
        EMBED_CALL(diffview_centered, DiffviewCtx, .work = args, .row = row);
        return GUI_WORK_TAKEN;
    }
    EMBED_CALL(diffview_scrolled, DiffviewCtx, .work = args, .rows = args->rows, .on = 1);
    return GUI_WORK_TAKEN;
}

// F7 steps to the next change and Shift+F7 to the one before; the arrows, the pages, Home and End scroll; the keys that
// edit a text are taken and do nothing, the editor under the view standing as it is; any other passes on
static unsigned int diffview_key(const GuiWorkCfg *args)
{
    static const char *const s_scrolls[6] = {"Up", "Down", "PageUp", "PageDown", "Home", "End"};
    static const char *const s_edits[5] = {"Backspace", "Delete", "Enter", "Tab", "Shift+Tab"};
    const int shown = (int)EMBED_CALL(diffview_shown, DiffviewCtx, .work = args);
    // the rows are far fewer than an int counts
    const int turns[6] = {-1, 1, -shown, shown, -(int)0x3fffffff, (int)0x3fffffff};
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "F7", .cap = 16ULL}) ||
        cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Shift+F7", .cap = 16ULL}))
    {
        EMBED_CALL(diffview_stepped, DiffviewCtx, .work = args, .direction = (args->name[0] == 'F') ? 1 : -1);
        return GUI_WORK_TAKEN;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_scrolls[at], .cap = 16ULL}))
        {
            EMBED_CALL(diffview_scrolled, DiffviewCtx, .work = args, .rows = turns[at], .on = 1);
            return GUI_WORK_TAKEN;
        }
    }
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_edits[at], .cap = 16ULL}))
        {
            return GUI_WORK_TAKEN;
        }
    }
    return GUI_WORK_PASSED;
}

// text typed is taken and does nothing, the editor under the view standing as it is
static unsigned int diffview_text_typed(const GuiWorkCfg *args)
{
    (void)args;
    return GUI_WORK_TAKEN;
}

static void diffview_close(const GuiWorkCfg *args)
{
    (void)args;
    EMBED_CALL(cli_diff_view_source_err_msg.close, CliDiffViewSourceErrMsgCfg, .root = NULL);
}

const GuiWorkTable gui_diffview_table = {
    .name = "diff",
    .draw = diffview_draw,
    .hit = diffview_hit,
    .press = diffview_press,
    .wheel = diffview_wheel,
    .key = diffview_key,
    .text = diffview_text_typed,
    .close = diffview_close,
};
