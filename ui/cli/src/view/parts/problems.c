// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// problems.c: the problems pane. Each file and diagnostic is a row as the servers' reports stand; the pane's scroll and
// the row the pointer marks are the window's own
#include "problems.h"

#include "../../command_line/cli_internal.h"
#include "../../editor/view.h"
#include "../../text/text.h"
#include "explorer.h"

void gui_problems_init(const GuiProblemsCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->problems_out, .val = 0, .bytes = sizeof(*args->problems_out)});
    args->problems_out->pointed = GUI_PROBLEMS_NONE;
}

typedef struct
{
    const GuiProblems *problems_read;
    const GuiFrame *frame;
    unsigned int row;
    GuiGlyphs *glyphs;
    unsigned int face;
    unsigned int size;
    int top;
    int pixel_height;
    GuiRaster *raster;
    int x;
    int y;
    int side;
    int open;
    GuiColor color;
    const GuiPalette *palette;
    unsigned int severity;
    GuiRect inside;
} ProblemsCtx;

unsigned int gui_problems_rows(const GuiProblemsRowFrameCfg *args)
{
    (void)args;
    return cli_servers_problems_counts().rows;
}

int gui_problems_rows_top(const GuiProblemsRowFrameCfg *args)
{
    return args->frame->side.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                            .pixels = GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE + GUI_PROBLEMS_HEAD,
                                            .scale = args->frame->scale);
}

// the height of the row `row`: a file's or a diagnostic's
EMBED_INLINE int problems_row_height(const ProblemsCtx *args)
{
    CliProblemRow row;
    const int file = cli_servers_problems_row(args->row, &row) && row.is_file;
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                      .pixels = file ? GUI_PROBLEMS_FILE_ROW : GUI_PROBLEMS_ROW, .scale = args->frame->scale);
}

GuiRect gui_problems_row_rect(const GuiProblemsRowFrameCfg *args)
{
    int y = gui_problems_row_frame.rows_top(args);
    for (unsigned int each = args->problems->scroll; each < args->row; each += 1u)
    {
        y += EMBED_CALL(problems_row_height, ProblemsCtx, .problems_read = args->problems, .frame = args->frame,
                        .row = each);
    }
    return (GuiRect){args->frame->side.x, y,
                     args->frame->side.width - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1,
                                                          .scale = args->frame->scale),
                     EMBED_CALL(problems_row_height, ProblemsCtx, .problems_read = args->problems, .frame = args->frame,
                                .row = args->row)};
}

int gui_problems_hit(const GuiProblemsRowFrameCfg *args)
{
    unsigned int *row = args->row_pointer;
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->side, .x = args->x,
                    .y = args->y) ||
        (args->y < gui_problems_row_frame.rows_top(args)))
    {
        return 0;
    }
    const unsigned int rows = gui_problems_row_frame.rows(args);
    int top = gui_problems_row_frame.rows_top(args);
    for (unsigned int at = args->problems->scroll; at < rows; at += 1u)
    {
        const int pixel_height = EMBED_CALL(problems_row_height, ProblemsCtx, .problems_read = args->problems,
                                            .frame = args->frame, .row = at);
        if (args->y < top + pixel_height)
        {
            *row = at;
            return 1;
        }
        top += pixel_height;
    }
    return 0;
}

int gui_problems_move(const GuiProblemsFrameCfg *args)
{
    unsigned int row = 0u;
    // the rows are fewer than an int counts
    const int pointed = EMBED_CALL(gui_problems_row_frame.hit, GuiProblemsRowFrameCfg, .problems = args->problems_out,
                                   .frame = args->frame, .x = args->x, .y = args->y, .row_pointer = &row)
                            ? (int)row
                            : GUI_PROBLEMS_NONE;
    const int changed = pointed != args->problems_out->pointed;
    args->problems_out->pointed = pointed;
    return changed;
}

// the pane's head rectangle in the frame's side
EMBED_INLINE GuiRect problems_head_rect(const ProblemsCtx *args)
{
    const int top = args->frame->side.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                     .pixels = GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE,
                                                     .scale = args->frame->scale);
    return (GuiRect){args->frame->side.x, top,
                     args->frame->side.width - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1,
                                                          .scale = args->frame->scale),
                     EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_HEAD,
                                .scale = args->frame->scale)};
}

int gui_problems_head_holds(const GuiProblemsFrameCfg *args)
{
    return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                      .rect = EMBED_CALL(problems_head_rect, ProblemsCtx, .frame = args->frame), .x = args->x,
                      .y = args->y);
}

int gui_problems_wheel(const GuiProblemsRowFrameCfg *args)
{
    const int pixel_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PROBLEMS_ROW, .scale = args->frame->scale);
    const int capacity = args->frame->side.y + args->frame->side.height - gui_problems_row_frame.rows_top(args);
    const int displayed = (pixel_height > 0) ? capacity / pixel_height : 0;
    // the rows are far fewer than an int counts
    const int count =
        (int)EMBED_CALL(gui_problems_row_frame.rows, GuiProblemsRowFrameCfg, .problems = args->problems_out);
    const int most = (count > displayed) ? count - displayed : 0;
    const int required = (int)args->problems_out->scroll + args->rows;
    int clamped;
    if (required < 0)
    {
        clamped = 0;
    }
    else if (required > most)
    {
        clamped = most;
    }
    else
    {
        clamped = required;
    }
    // the scroll clamped is not negative
    if ((unsigned int)clamped == args->problems_out->scroll)
    {
        return 0;
    }
    args->problems_out->scroll = (unsigned int)clamped;
    return 1;
}

// the baseline of text in `face` at `size` centered in a row of `pixel_height` from `top`
EMBED_INLINE int problems_baseline(const ProblemsCtx *args)
{
    const GuiGlyphs *glyphs = args->glyphs;
    unsigned int face = args->face;
    unsigned int size = args->size;
    const float ascent =
        EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face, .size = size);
    const float descent =
        EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face, .size = size);
    return args->top + (int)((((float)args->pixel_height - ascent - descent) / 2.0f) + ascent + 0.5f);
}

// a file row's arrow, pointing down where its rows are open and right where closed, `side` pixels across at x, y
EMBED_INLINE void problems_arrow_draw(const ProblemsCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiColor color = args->color;
    const int half = args->side / 2;
    if (args->open)
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x, .y0 = args->y + 1,
                   .x1 = args->x + half, .y1 = args->y + half + 1, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half,
                   .y0 = args->y + half + 1, .x1 = args->x + args->side - 1, .y1 = args->y + 1, .color = color);
    }
    else
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + 1, .y0 = args->y,
                   .x1 = args->x + half + 1, .y1 = args->y + half, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half + 1,
                   .y0 = args->y + half, .x1 = args->x + 1, .y1 = args->y + args->side - 1, .color = color);
    }
}

// the color of a severity's mark
EMBED_INLINE GuiColor problems_color(const ProblemsCtx *args)
{
    if (args->severity == GUI_PROBLEMS_ERROR)
    {
        return args->palette->error;
    }
    return (args->severity == GUI_PROBLEMS_WARNING) ? args->palette->warning : args->palette->hover;
}

// the pane's head drawn: the check's progress and Stop while it runs, else the problems and files counted and the
// check offered
EMBED_INLINE void problems_head_draw(const ProblemsCtx *args)
{
    const CliProblemsCounts counted = cli_servers_problems_counts();
    const CliProblemsCounts *const problems = &counted;
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const GuiRect head = problems_head_rect(args);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_PROBLEMS_HEAD_SIZE, .scale = scale);
    const int baseline = EMBED_CALL(problems_baseline, ProblemsCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                                    .size = size, .top = head.y, .pixel_height = head.height);
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
                     .length = line.at, .color = args->palette->text_2, .clip = args->inside);
    const char *const action = running ? "Stop" : "Check the tree";
    const unsigned long long action_length = cellul.len(&(CatenaFinitaCfg){.src = action, .cap = 32ULL});
    const float action_width = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                          .face = GUI_FACE_HEAD, .size = size, .text = action, .length = action_length);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(head.x + head.width - pad) - action_width,
                     .y = baseline, .text = action, .length = action_length, .color = args->palette->link,
                     .clip = args->inside);
}

void gui_problems_draw(const GuiProblemsRowFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const GuiRect side = args->frame->side;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = side))
    {
        return;
    }
    const int one = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    const GuiRect inside = {side.x, side.y, side.width - one, side.height};
    const int line_y =
        side.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_HEAD, .scale = scale);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){side.x, line_y, inside.width, one}, .other = inside),
               .color = args->palette->line);
    EMBED_CALL(problems_head_draw, ProblemsCtx, .problems_read = args->problems, .raster = raster, .glyphs = glyphs,
               .palette = args->palette, .frame = args->frame, .inside = inside);
    const int rows_top = gui_problems_row_frame.rows_top(args);
    const GuiRect body =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){side.x, rows_top, inside.width, side.y + side.height - rows_top}, .other = inside);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = body))
    {
        return;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = body,
               .color = args->palette->editor_background);
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
    const int arrow =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ARROW, .scale = scale);
    const unsigned int rows = gui_problems_row_frame.rows(args);
    for (unsigned int row = args->problems->scroll; row < rows; row += 1u)
    {
        const GuiRect rect = EMBED_CALL(gui_problems_row_frame.row_rect, GuiProblemsRowFrameCfg,
                                        .problems = args->problems, .frame = args->frame, .row = row);
        if (rect.y >= body.y + body.height)
        {
            break;
        }
        const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = body);
        // the rows are fewer than an int counts
        if (args->problems->pointed == (int)row)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = on,
                       .color = args->palette->bar_title_hover);
        }
        CliProblemRow read;
        if (!cli_servers_problems_row(row, &read))
        {
            break;
        }
        const CliProblemRow *const file = &read;
        if (file->is_file)
        {
            // a file's row: the arrow, the name, the folder after it quieter, and the count at the right
            EMBED_CALL(problems_arrow_draw, ProblemsCtx, .raster = raster, .x = rect.x + pad,
                       .y = rect.y + ((rect.height - arrow) / 2), .side = arrow, .open = !file->closed,
                       .color = args->palette->text);
            const int baseline = EMBED_CALL(problems_baseline, ProblemsCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
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
            const int count_height = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                .pixels = GUI_PROBLEMS_FILE_ROW - 8, .scale = scale);
            const GuiRect badge = {rect.x + rect.width - pad - count_width, rect.y + ((rect.height - count_height) / 2),
                                   count_width, count_height};
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = badge, .other = on),
                       .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                          .pixels = GUI_PROBLEMS_COUNT_RADIUS, .scale = scale),
                       .color = args->palette->line);
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_CODE, .size = count_size, .x_f32 = (float)(badge.x + count_pad),
                             .y = EMBED_CALL(problems_baseline, ProblemsCtx, .glyphs = glyphs, .face = GUI_FACE_CODE,
                                             .size = count_size, .top = badge.y, .pixel_height = badge.height),
                             .text = digits, .length = count.at, .color = args->palette->text_2, .clip = on);
            const GuiRect text_clip =
                EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                           .one = (GuiRect){rect.x, rect.y, badge.x - gap - rect.x, rect.height}, .other = on);
            float x = (float)(rect.x + pad + arrow + gap);
            x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                           .face = GUI_FACE_HEAD, .size = name_size, .x_f32 = x, .y = baseline, .text = file->name,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = file->name, .cap = CLI_PATH_MAX_LEN}),
                           .color = args->palette->text, .clip = text_clip);
            // the folder is the front of the file's path, before its name
            if (file->folder_length != 0ULL)
            {
                (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster,
                                 .glyphs = glyphs, .face = GUI_FACE_HEAD, .size = where_size, .x_f32 = x + (float)gap,
                                 .y = baseline, .text = file->path, .length = file->folder_length,
                                 .color = args->palette->text_3, .clip = text_clip);
            }
            continue;
        }
        // a diagnostic's row: the guide, the mark in its severity's color, the message and its place at the right
        const CliProblemRow *const inner_one = file;
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){rect.x + pad + (arrow / 2), rect.y,
                                                       EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                  .pixels = 1, .scale = scale),
                                                       rect.height},
                                      .other = on),
                   .color = args->palette->line);
        const int mark_x = rect.x + pad + guide;
        EMBED_CALL(
            gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
            .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                               .one = (GuiRect){mark_x, rect.y + ((rect.height - mark) / 2), mark, mark}, .other = on),
            .radius = (unsigned int)(mark / 2),
            .color =
                EMBED_CALL(problems_color, ProblemsCtx, .palette = args->palette, .severity = inner_one->severity));
        char where_text[48];
        VerbaLinea where = {where_text, sizeof(where_text), 0ULL};
        mmgr_line_decimal(&where, inner_one->line + 1ULL, 1u);
        mmgr_line_ch(&where, ':');
        mmgr_line_decimal(&where, inner_one->col + 1ULL, 1u);
        const float where_width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                       .size = where_size, .text = where_text, .length = where.at);
        const int baseline = EMBED_CALL(problems_baseline, ProblemsCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                                        .size = message_size, .top = rect.y, .pixel_height = rect.height);
        const float where_x = (float)(rect.x + rect.width - pad) - where_width;
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = where_size, .x_f32 = where_x, .y = baseline, .text = where_text,
                         .length = where.at, .color = args->palette->text_3, .clip = on);
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, (int)where_x - gap - rect.x, rect.height}, .other = on);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = message_size, .x_f32 = (float)(mark_x + mark + gap),
                         .y = baseline, .text = inner_one->message, .length = inner_one->message_length,
                         .color = args->palette->text, .clip = text_clip);
    }
}

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
