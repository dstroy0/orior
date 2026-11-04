// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// problems.c: the problems pane's rows as the servers' reports stand, and the editor's underlines of the shown file's
// spans
#include "problems.h"

#include "../../command_line/cli_internal.h"
#include "../../editor/view.h"
#include "../../text/text.h"

typedef struct
{
    const GuiPalette *palette;
    unsigned int severity;
} ProblemsCtx;

// the color of a severity's mark
EMBED_INLINE GuiColor problems_color(const ProblemsCtx *args)
{
    if (args->severity == GUI_PROBLEMS_ERROR)
    {
        return args->palette->error;
    }
    return (args->severity == GUI_PROBLEMS_WARNING) ? args->palette->warning : args->palette->hover;
}

static unsigned int problems_rows(const GuiPaneCfg *args)
{
    (void)args;
    return cli_servers_problems_counts().rows;
}

// the height of the row `row`: a file's or a diagnostic's
static int problems_row_height(const GuiPaneCfg *args)
{
    CliProblemRow row;
    const int file = cli_servers_problems_row(args->row, &row) && row.is_file;
    return file ? GUI_PROBLEMS_FILE_ROW : GUI_PROBLEMS_ROW;
}

static int problems_head(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_PROBLEMS_HEAD;
}

// the pane's head drawn: the check's progress and Stop while it runs, else the problems and files counted and the
// check offered
static void problems_head_draw(const GuiPaneCfg *args)
{
    const CliProblemsCounts counted = cli_servers_problems_counts();
    const CliProblemsCounts *const problems = &counted;
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const GuiRect head = args->rect;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_PROBLEMS_HEAD_SIZE, .scale = scale);
    const int baseline = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size, .top = head.y, .pixel_height = head.height);
    const int pad =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_PAD_LEFT, .scale = scale);
    char text[160];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    const int running = (problems->total != 0ULL) && (problems->done < problems->total);
    if (running)
    {
        mmgr_line_put(&line, "Checking the tree: ");
        mmgr_line_decimal(&line, problems->done, 1u);
        mmgr_line_put(&line, " of ");
        mmgr_line_decimal(&line, problems->total, 1u);
        mmgr_line_put(&line, " files");
    }
    else if (problems->items > 0u)
    {
        mmgr_line_decimal(&line, problems->items, 1u);
        mmgr_line_put(&line, (problems->items == 1u) ? " problem in " : " problems in ");
        mmgr_line_decimal(&line, problems->files, 1u);
        mmgr_line_put(&line, (problems->files == 1u) ? " file" : " files");
    }
    else
    {
        mmgr_line_put(&line, "No problems in the tree");
    }
    (void)mmgr_line_finish(&line);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(head.x + pad), .y = baseline, .text = text,
                     .length = line.at, .color = GUI_FRAME_FADED(args->palette->text_2, args->frame->side_share),
                     .clip = args->clip);
    const char *const action = running ? "Stop" : "Check the tree";
    const unsigned long long action_length = cellul.len(&(CatenaFinitaCfg){.src = action, .cap = 32ULL});
    const float action_width = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                          .face = GUI_FACE_HEAD, .size = size, .text = action, .length = action_length);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(head.x + head.width - pad) - action_width,
                     .y = baseline, .text = action, .length = action_length,
                     .color = GUI_FRAME_FADED(args->palette->link, args->frame->side_share), .clip = args->clip);
}

// the whole head offers the check
static unsigned int problems_head_hit(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_PROBLEMS_CHECK;
}

static void problems_action(const GuiPaneCfg *args)
{
    cli_servers_problems_check(args->root);
}

// a file's row: the arrow, the name, the folder after it quieter, and the count at the right; a diagnostic's row: the
// guide, the mark in its severity's color, the message and its place at the right
static void problems_row_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiFrame *const frame = args->frame;
    const GuiRect rect = args->rect;
    const GuiRect on = args->clip;
    const int scale = frame->scale;
    CliProblemRow read;
    if (!cli_servers_problems_row(args->row, &read))
    {
        return;
    }
    const unsigned int name_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_PROBLEMS_NAME_SIZE, .scale = scale);
    const unsigned int where_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = (int)GUI_PROBLEMS_WHERE_SIZE, .scale = scale);
    const unsigned int message_size = (unsigned int)EMBED_CALL(
        gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = (int)GUI_PROBLEMS_MESSAGE_SIZE, .scale = scale);
    const unsigned int count_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = (int)GUI_PROBLEMS_COUNT_SIZE, .scale = scale);
    const int pad =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_PAD_LEFT, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_GAP, .scale = scale);
    const int guide =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_GUIDE, .scale = scale);
    const int mark = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_MARK, .scale = scale);
    const int arrow = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANE_ARROW, .scale = scale);
    const CliProblemRow *const file = &read;
    if (file->is_file)
    {
        EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = raster, .x = rect.x + pad,
                   .y = rect.y + ((rect.height - arrow) / 2), .side = arrow, .open = !file->closed,
                   .color = GUI_FRAME_FADED(args->palette->text, frame->side_share), .clip = on);
        const int baseline =
            EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_HEAD,
                       .size = name_size, .top = rect.y, .pixel_height = rect.height);
        char digits[24];
        VerbaLinea count = {digits, sizeof(digits), 0ULL};
        mmgr_line_decimal(&count, file->count, 1u);
        const int count_pad =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_COUNT_PAD, .scale = scale);
        const int count_width =
            (int)(EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                             .face = GUI_FACE_CODE, .size = count_size, .text = digits, .length = count.at) +
                  0.5f) +
            (2 * count_pad);
        const int count_height =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_FILE_ROW - 8, .scale = scale);
        const GuiRect badge = {rect.x + rect.width - pad - count_width, rect.y + ((rect.height - count_height) / 2),
                               count_width, count_height};
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = badge, .other = on),
                   .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                      .pixels = GUI_PROBLEMS_COUNT_RADIUS, .scale = scale),
                   .color = GUI_FRAME_FADED(args->palette->line, frame->side_share));
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = count_size, .x_f32 = (float)(badge.x + count_pad),
                         .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                         .face = GUI_FACE_CODE, .size = count_size, .top = badge.y,
                                         .pixel_height = badge.height),
                         .text = digits, .length = count.at,
                         .color = GUI_FRAME_FADED(args->palette->text_2, frame->side_share), .clip = on);
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, badge.x - gap - rect.x, rect.height}, .other = on);
        float x = (float)(rect.x + pad + arrow + gap);
        x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                       .face = GUI_FACE_HEAD, .size = name_size, .x_f32 = x, .y = baseline, .text = file->name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = file->name, .cap = CLI_PATH_MAX_LEN}),
                       .color = GUI_FRAME_FADED(args->palette->text, frame->side_share), .clip = text_clip);
        // the folder is the front of the file's path, before its name
        if (file->folder_length != 0ULL)
        {
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_HEAD, .size = where_size, .x_f32 = x + (float)gap, .y = baseline,
                             .text = file->path, .length = file->folder_length,
                             .color = GUI_FRAME_FADED(args->palette->text_3, frame->side_share), .clip = text_clip);
        }
        return;
    }
    const CliProblemRow *const inner_one = file;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(
                   gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){rect.x + pad + (arrow / 2), rect.y,
                                    EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                                    rect.height},
                   .other = on),
               .color = GUI_FRAME_FADED(args->palette->line, frame->side_share));
    const int mark_x = rect.x + pad + guide;
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
               .rect =
                   EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                              .one = (GuiRect){mark_x, rect.y + ((rect.height - mark) / 2), mark, mark}, .other = on),
               .radius = (unsigned int)(mark / 2),
               .color = GUI_FRAME_FADED(
                   EMBED_CALL(problems_color, ProblemsCtx, .palette = args->palette, .severity = inner_one->severity),
                   frame->side_share));
    char where_text[48];
    VerbaLinea where = {where_text, sizeof(where_text), 0ULL};
    mmgr_line_decimal(&where, inner_one->line + 1ULL, 1u);
    mmgr_line_ch(&where, ':');
    mmgr_line_decimal(&where, inner_one->col + 1ULL, 1u);
    const float where_width =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                   .size = where_size, .text = where_text, .length = where.at);
    const int baseline =
        EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_HEAD,
                   .size = message_size, .top = rect.y, .pixel_height = rect.height);
    const float where_x = (float)(rect.x + rect.width - pad) - where_width;
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_CODE, .size = where_size, .x_f32 = where_x, .y = baseline, .text = where_text,
                     .length = where.at, .color = GUI_FRAME_FADED(args->palette->text_3, frame->side_share),
                     .clip = on);
    const GuiRect text_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){rect.x, rect.y, (int)where_x - gap - rect.x, rect.height}, .other = on);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = message_size, .x_f32 = (float)(mark_x + mark + gap), .y = baseline,
                     .text = inner_one->message, .length = inner_one->message_length,
                     .color = GUI_FRAME_FADED(args->palette->text, frame->side_share), .clip = text_clip);
}

// a file's row pressed closes or opens its diagnostics; a diagnostic's opens the file at its place
static int problems_press(const GuiPaneCfg *args)
{
    CliProblemRow row;
    if (!cli_servers_problems_press(args->row, &row))
    {
        return 0;
    }
    *args->open_out = (GuiPaneOpen){row.path, {row.line, row.col}, 1};
    return 1;
}

const GuiPaneKind gui_problems_kind = {
    .tool = "problems",
    .rows = problems_rows,
    .row_height = problems_row_height,
    .head = problems_head,
    .head_draw = problems_head_draw,
    .head_hit = problems_head_hit,
    .action = problems_action,
    .row_draw = problems_row_draw,
    .press = problems_press,
};

void gui_problems_underlined(const GuiProblemsPathRasterCfg *args)
{
    const GuiEditor *editor = args->editor;
    GuiRect place = args->place;
    float cell = args->cell;
    int scale = args->scale;
    // the spans of the file the editor shows, its path the shown view's; a view's place is far inside an int
    const int shown = cli_view_shown();
    const char *const path = (shown >= 0) ? cli_view_path_at((unsigned int)shown) : NULL;
    const int thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    const int up = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 2, .scale = scale);
    CliProblemSpan span;
    for (unsigned int at = 0u; cli_servers_problems_span(path, at, &span); at += 1u)
    {
        const CliProblemSpan *const one = &span;
        const GuiColor color =
            EMBED_CALL(problems_color, ProblemsCtx, .palette = args->palette, .severity = one->severity);
        for (unsigned long long line = one->from_line; line <= one->to_line; line += 1ULL)
        {
            const unsigned long long from = (line == one->from_line) ? one->from_col : 0ULL;
            const unsigned long long to = (line == one->to_line) ? one->to_col : ~0ULL;
            GuiRect start;
            GuiRect end;
            if (!EMBED_CALL(gui_editor_position_cell.position_rect, GuiEditorPositionCellCfg, .editor_read = editor,
                            .place = place, .cell = cell, .scale = scale, .line = line, .col = from, .rect = &start) ||
                !EMBED_CALL(gui_editor_position_cell.position_rect, GuiEditorPositionCellCfg, .editor_read = editor,
                            .place = place, .cell = cell, .scale = scale, .line = line, .col = to, .rect = &end))
            {
                continue;
            }
            // a span of no width underlines its one cell
            const int right = (end.x > start.x) ? end.x : start.x + start.width;
            const GuiRect under = {start.x, start.y + start.height - up, right - start.x, thick};
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = under, .other = place),
                       .color = color);
        }
    }
}
