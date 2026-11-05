// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runs.c: the Runs pane's rows as the runs listing lists them
#include "runs.h"

#include "../../command_line/cli_internal.h"
#include "../../runs/qry.h"

// what the pane reads where the tree has no run
#define RUNS_EMPTY_TEXT                                                                                                \
    "No query runs in this tree. A run's .qry shows here from the moment its writer creates it, wherever its script "  \
    "was started."

typedef struct
{
    const GuiPaneCfg *pane;
    int x;
    unsigned int face;
    unsigned int size;
    float x_f32;
    const char *text;
    unsigned long long length;
    GuiColor color;
    unsigned long long bytes;
    GuiRect clip;
    int clipped;
} RunsCtx;

static const char *runs_title(const GuiPaneCfg *args)
{
    (void)args;
    return "Runs";
}

static unsigned int runs_rows(const GuiPaneCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_runs_pane.rows, CliRunsPaneCfg, .at = 0u);
}

// `x` pixels of the page at the frame's scale
EMBED_INLINE int runs_scaled(const RunsCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->x, .scale = args->pane->frame->scale);
}

// the width of `length` bytes of `text` in `face` at `size` page pixels
EMBED_INLINE float runs_text_width(const RunsCtx *args)
{
    return EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->pane->glyphs,
                      .face = args->face,
                      .size = (unsigned int)EMBED_CALL(runs_scaled, RunsCtx, .pane = args->pane, .x = (int)args->size),
                      .text = args->text, .length = args->length);
}

// `length` bytes of `text` drawn from x in the row, `face` at `size` page pixels, in `color` faded with the side, cut
// to `clip` where `clipped` and to the row's clip where not: the x after them
EMBED_INLINE float runs_text_draw(const RunsCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const unsigned int size = (unsigned int)EMBED_CALL(runs_scaled, RunsCtx, .pane = pane, .x = (int)args->size);
    return EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = pane->raster, .glyphs = pane->glyphs,
        .face = args->face, .size = size, .x_f32 = args->x_f32,
        .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = pane->glyphs, .face = args->face,
                        .size = size, .top = pane->rect.y, .pixel_height = pane->rect.height),
        .text = args->text, .length = args->length, .color = GUI_FRAME_FADED(args->color, pane->frame->side_share),
        .clip = args->clipped ? args->clip : pane->clip);
}

// `bytes` written into the line at `text`: under 1024 in bytes, under a mebibyte in kibibytes to a tenth, else in
// mebibytes to a tenth
EMBED_INLINE void runs_size_write(const RunsCtx *args)
{
    VerbaLinea *const line = (VerbaLinea *)args->text;
    if (args->bytes < 1024ULL)
    {
        mmgr_line_decimal(line, args->bytes, 1u);
        mmgr_line_put(line, " B");
        return;
    }
    const unsigned long long unit = (args->bytes < (1ULL << 20u)) ? 1024ULL : (1ULL << 20u);
    const unsigned long long tenths = ((args->bytes * 10ULL) + (unit / 2ULL)) / unit;
    mmgr_line_decimal(line, tenths / 10ULL, 1u);
    mmgr_line_ch(line, '.');
    mmgr_line_decimal(line, tenths % 10ULL, 1u);
    mmgr_line_put(line, (unit == 1024ULL) ? " KB" : " MB");
}

// a bar down the row from x, GUI_RUNS_BAR wide, in `color` faded with the side
EMBED_INLINE void runs_bar_draw(const RunsCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = pane->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){args->x, pane->rect.y,
                                                   EMBED_CALL(runs_scaled, RunsCtx, .pane = pane, .x = GUI_RUNS_BAR),
                                                   pane->rect.height},
                                  .other = pane->clip),
               .color = GUI_FRAME_FADED(args->color, pane->frame->side_share));
}

// `length` bytes of `text` drawn ending at the row's right padding, `face` at `size`, in `color`: the x they start at
EMBED_INLINE float runs_text_draw_right(const RunsCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const int pad = EMBED_CALL(runs_scaled, RunsCtx, .pane = pane, .x = GUI_PANE_PAD_LEFT);
    const float width = EMBED_CALL(runs_text_width, RunsCtx, .pane = pane, .face = args->face, .size = args->size,
                                   .text = args->text, .length = args->length);
    const float x = (float)(pane->rect.x + pane->rect.width - pad) - width;
    (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = pane, .face = args->face, .size = args->size, .x_f32 = x,
                     .text = args->text, .length = args->length, .color = args->color);
    return x;
}

// a row drawn: the empty row's text; a folder's path; a run's name with `live`, `panicked` or its bytes at the right;
// a panic's head and lines behind a bar in the error's color; a conference's question and accounts behind a quiet bar;
// the count; a record's sequence, name and bytes; and the lessons' head and each lesson's cause, carrier and stand-in
static void runs_row_draw(const GuiPaneCfg *args)
{
    const GuiPalette *const palette = args->palette;
    const GuiRect rect = args->rect;
    CliRunsRow row;
    if (!EMBED_CALL(cli_runs_pane.row, CliRunsPaneCfg, .at = args->row, .row = &row))
    {
        return;
    }
    const int pad = EMBED_CALL(runs_scaled, RunsCtx, .pane = args, .x = GUI_PANE_PAD_LEFT);
    const int gap = EMBED_CALL(runs_scaled, RunsCtx, .pane = args, .x = GUI_RUNS_GAP);
    // a folder's, a run's and the empty row's left; the detail's left, where its bars, counts, records and lessons
    // start; and a text's left past a bar
    const int left = rect.x + pad;
    const int detail = left + EMBED_CALL(runs_scaled, RunsCtx, .pane = args, .x = GUI_RUNS_DETAIL_PAD);
    const int past_bar = detail + EMBED_CALL(runs_scaled, RunsCtx, .pane = args, .x = GUI_RUNS_BAR) +
                         EMBED_CALL(runs_scaled, RunsCtx, .pane = args, .x = GUI_RUNS_BAR_PAD);
    // the rows are fewer than an int counts
    const int pointed = args->pane_read->pointed == (int)args->row;
    char digits[64];
    VerbaLinea line = {digits, sizeof(digits), 0ULL};
    switch (row.kind)
    {
    case CLI_RUNS_ROW_EMPTY:
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_HEAD, .size = GUI_RUNS_TEXT_SIZE,
                         .x_f32 = (float)left, .text = RUNS_EMPTY_TEXT,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = RUNS_EMPTY_TEXT, .cap = 256ULL}),
                         .color = palette->text_3);
        break;
    case CLI_RUNS_ROW_FOLDER:
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_SMALL_SIZE,
                         .x_f32 = (float)left, .text = row.text, .length = row.text_length, .color = palette->text_3);
        break;
    case CLI_RUNS_ROW_RUN: {
        if (row.open && !pointed)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = args->clip,
                       .color = GUI_FRAME_FADED(palette->line, args->frame->side_share));
        }
        float right_x;
        if (row.live)
        {
            const char *const label = (row.panicked != 0ULL) ? "panicked" : "live";
            right_x = EMBED_CALL(runs_text_draw_right, RunsCtx, .pane = args, .face = GUI_FACE_HEAD,
                                 .size = GUI_RUNS_SMALL_SIZE, .text = label,
                                 .length = cellul.len(&(CatenaFinitaCfg){.src = label, .cap = 16ULL}),
                                 .color = (row.panicked != 0ULL) ? palette->error : palette->signal);
        }
        else
        {
            EMBED_CALL(runs_size_write, RunsCtx, .text = (const char *)&line, .bytes = row.size);
            right_x =
                EMBED_CALL(runs_text_draw_right, RunsCtx, .pane = args, .face = GUI_FACE_CODE,
                           .size = GUI_RUNS_SMALL_SIZE, .text = digits, .length = line.at, .color = palette->text_3);
        }
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, (int)right_x - gap - rect.x, rect.height}, .other = args->clip);
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_TEXT_SIZE,
                         .x_f32 = (float)left, .text = row.text, .length = row.text_length,
                         .color = (pointed || row.open) ? palette->text : palette->text_2, .clip = text_clip,
                         .clipped = 1);
        break;
    }
    case CLI_RUNS_ROW_PANIC:
        EMBED_CALL(runs_bar_draw, RunsCtx, .pane = args, .x = detail, .color = palette->error);
        mmgr_line_put(&line, "panic #");
        mmgr_line_decimal(&line, row.number, 1u);
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_SMALL_SIZE,
                         .x_f32 = (float)past_bar, .text = digits, .length = line.at, .color = palette->error);
        break;
    case CLI_RUNS_ROW_PANIC_LINE:
        EMBED_CALL(runs_bar_draw, RunsCtx, .pane = args, .x = detail, .color = palette->error);
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_TEXT_SIZE,
                         .x_f32 = (float)past_bar, .text = row.text, .length = row.text_length, .color = palette->text);
        break;
    case CLI_RUNS_ROW_QUESTION:
        EMBED_CALL(runs_bar_draw, RunsCtx, .pane = args, .x = detail, .color = palette->text_3);
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_HEAD, .size = GUI_RUNS_TEXT_SIZE,
                         .x_f32 = (float)past_bar, .text = row.text, .length = row.text_length,
                         .color = palette->text_2);
        break;
    case CLI_RUNS_ROW_ACCOUNT: {
        EMBED_CALL(runs_bar_draw, RunsCtx, .pane = args, .x = detail, .color = palette->text_3);
        float x = (float)past_bar;
        if (row.second_length != 0ULL)
        {
            x = EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_TEXT_SIZE,
                           .x_f32 = x, .text = row.second, .length = row.second_length,
                           .color = row.ruling ? palette->signal : palette->text_2) +
                (float)gap;
        }
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_TEXT_SIZE,
                         .x_f32 = x, .text = row.text, .length = row.text_length,
                         .color = row.ruling ? palette->signal : palette->text);
        break;
    }
    case CLI_RUNS_ROW_COUNT:
        mmgr_line_decimal(&line, row.number, 1u);
        mmgr_line_put(&line, (row.number == 1ULL) ? " blob" : " blobs");
        mmgr_line_put(&line, row.live ? " so far" : "");
        if (row.size != 0ULL)
        {
            mmgr_line_put(&line, ", ");
            mmgr_line_decimal(&line, row.size, 1u);
            mmgr_line_put(&line, " bytes after the last whole record");
        }
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_HEAD, .size = GUI_RUNS_SMALL_SIZE,
                         .x_f32 = (float)detail, .text = digits, .length = line.at, .color = palette->text_3);
        break;
    case CLI_RUNS_ROW_BLOB: {
        EMBED_CALL(runs_size_write, RunsCtx, .text = (const char *)&line, .bytes = row.size);
        const float right_x =
            EMBED_CALL(runs_text_draw_right, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_SMALL_SIZE,
                       .text = digits, .length = line.at, .color = palette->text_3);
        line.at = 0ULL;
        mmgr_line_ch(&line, '#');
        mmgr_line_decimal(&line, row.number, 1u);
        const float after =
            EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_SMALL_SIZE,
                       .x_f32 = (float)detail, .text = digits, .length = line.at, .color = palette->text_3);
        GuiColor name_color = palette->text_2;
        if (row.priority == QRY_PANIC)
        {
            name_color = palette->error;
        }
        else if (row.priority == QRY_CRITICAL)
        {
            name_color = palette->text;
        }
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, (int)right_x - gap - rect.x, rect.height}, .other = args->clip);
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_SMALL_SIZE,
                         .x_f32 = after + (float)gap, .text = row.text, .length = row.text_length, .color = name_color,
                         .clip = text_clip, .clipped = 1);
        break;
    }
    case CLI_RUNS_ROW_LESSONS:
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_HEAD, .size = GUI_RUNS_SMALL_SIZE,
                         .x_f32 = (float)detail, .text = "Lessons", .length = 7ULL, .color = palette->text_3);
        break;
    case CLI_RUNS_ROW_LESSON: {
        float x =
            EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_TEXT_SIZE,
                       .x_f32 = (float)detail, .text = row.text, .length = row.text_length, .color = palette->text) +
            (float)gap;
        x = EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_TEXT_SIZE,
                       .x_f32 = x, .text = row.second, .length = row.second_length, .color = palette->text_3) +
            (float)gap;
        (void)EMBED_CALL(runs_text_draw, RunsCtx, .pane = args, .face = GUI_FACE_CODE, .size = GUI_RUNS_TEXT_SIZE,
                         .x_f32 = x, .text = row.third, .length = row.third_length, .color = palette->text_2);
        break;
    }
    default:
        break;
    }
}

// a run's row pressed opens the run, read whole, or closes it where it is opened; no row opens a file
static int runs_press(const GuiPaneCfg *args)
{
    EMBED_CALL(cli_runs_pane.toggle, CliRunsPaneCfg, .at = args->row, .root = args->root);
    return 0;
}

// the runs listed anew as the pane shows
static void runs_read(const GuiPaneCfg *args)
{
    if (args->event == GUI_PANE_SHOWN)
    {
        EMBED_CALL(cli_runs_pane.relist, CliRunsPaneCfg, .root = args->root, .now_ms = cli_clock_milliseconds());
    }
}

const GuiPaneKind gui_runs_kind = {
    .tool = "runs",
    .title = runs_title,
    .rows = runs_rows,
    .row_draw = runs_row_draw,
    .press = runs_press,
    .read = runs_read,
};
