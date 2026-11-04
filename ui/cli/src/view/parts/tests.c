// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tests.c: the Tests pane's bar, summary and rows as the tests pane lists them
#include "tests.h"

#include "../../command_line/cli_internal.h"
#include "../../tests/testing.h"
#include "../../text/text.h"
#include "explorer.h"

// the share of a disabled action's colors
#define TESTS_DISABLED_SHARE ((GUI_FIXED_ONE * 45) / 100)

// An action of the bar: its number and its label
typedef struct
{
    unsigned int which;
    const char *label;
} TestsAction;

// the bar's actions, left to right, the Parallel choice apart at the right
static const TestsAction s_actions[] = {
    {CLI_TESTS_RUN, "Run All"},
    {CLI_TESTS_RUN_FAILED, "Run Failed"},
    {CLI_TESTS_COVER, "Coverage"},
    {CLI_TESTS_STOP, "Stop"},
};

typedef struct
{
    const GuiPaneCfg *pane;
    unsigned int which;
    unsigned int outcome;
    int x;
    int y;
    GuiColor ground;
    const char *text;
    unsigned long long length;
    GuiColor color;
    unsigned int face;
    unsigned int size;
    float x_f32;
} TestsCtx;

static const char *tests_title(const GuiPaneCfg *args)
{
    (void)args;
    return "Tests";
}

static unsigned int tests_rows(const GuiPaneCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_tests_pane.rows, CliTestsPaneCfg, .at = 0u);
}

// the bar, and the summary's row where a run goes or went
static int tests_head(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_TESTS_BAR + (EMBED_CALL(cli_tests_pane.said, CliTestsPaneCfg, .at = 0u) ? GUI_TESTS_SAID : 0);
}

// `pixels` of the page at the frame's scale
EMBED_INLINE int tests_scaled(const TestsCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->x, .scale = args->pane->frame->scale);
}

// the text size of the bar's actions and summary in pixels
EMBED_INLINE unsigned int tests_action_size(const TestsCtx *args)
{
    return (unsigned int)EMBED_CALL(tests_scaled, TestsCtx, .pane = args->pane, .x = (int)GUI_TESTS_ACTION_SIZE);
}

// the rectangle of the action `which` in the bar, the head's `rect`: the buttons from the left, each its label's width
// with its padding, the Parallel choice its box and label at the right
EMBED_INLINE GuiRect tests_action_rect(const TestsCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiRect bar = {pane->rect.x, pane->rect.y, pane->rect.width,
                         EMBED_CALL(tests_scaled, TestsCtx, .pane = pane, .x = GUI_TESTS_BAR)};
    const unsigned int size = EMBED_CALL(tests_action_size, TestsCtx, .pane = pane);
    const int pad = EMBED_CALL(tests_scaled, TestsCtx, .pane = pane, .x = GUI_TESTS_ACTION_PAD);
    const int gap = EMBED_CALL(tests_scaled, TestsCtx, .pane = pane, .x = GUI_TESTS_ACTION_GAP);
    const int height = EMBED_CALL(tests_scaled, TestsCtx, .pane = pane, .x = GUI_TESTS_BAR - 8);
    const int y = bar.y + ((bar.height - height) / 2);
    if (args->which == CLI_TESTS_PARALLEL)
    {
        const int box = EMBED_CALL(tests_scaled, TestsCtx, .pane = pane, .x = GUI_TESTS_BOX);
        const int width =
            box + gap +
            (int)(EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = pane->glyphs,
                             .face = GUI_FACE_HEAD, .size = size, .text = "Parallel", .length = 8ULL) +
                  0.5f);
        return (GuiRect){bar.x + bar.width - pad - width, y, width, height};
    }
    int x = bar.x + pad;
    for (unsigned int at = 0u; at < sizeof(s_actions) / sizeof(s_actions[0]); at += 1u)
    {
        const int width =
            (int)(EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = pane->glyphs,
                             .face = GUI_FACE_HEAD, .size = size, .text = s_actions[at].label,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = s_actions[at].label, .cap = 16ULL})) +
                  0.5f) +
            (2 * pad);
        if (s_actions[at].which == args->which)
        {
            return (GuiRect){x, y, width, height};
        }
        x += width + gap;
    }
    return (GuiRect){0, 0, 0, 0};
}

// whether the action `which` can be taken: Run Failed where a test failed, Stop while a run goes, the rest always
EMBED_INLINE int tests_action_enabled(const TestsCtx *args)
{
    if (args->which == CLI_TESTS_RUN_FAILED)
    {
        return EMBED_CALL(cli_tests_pane.any_failed, CliTestsPaneCfg, .at = 0u);
    }
    if (args->which == CLI_TESTS_STOP)
    {
        return EMBED_CALL(cli_tests_pane.running, CliTestsPaneCfg, .at = 0u);
    }
    return 1;
}

// `text` drawn at x in the head's row from `y`, `height` pixels, in `color` cut to the pane's clip
EMBED_INLINE float tests_text_draw(const TestsCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    return EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = pane->raster,
                      .glyphs = pane->glyphs, .face = args->face, .size = args->size, .x_f32 = args->x_f32,
                      .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = pane->glyphs,
                                      .face = args->face, .size = args->size, .top = args->y, .pixel_height = args->x),
                      .text = args->text, .length = args->length, .color = args->color, .clip = pane->clip);
}

// the bar drawn: each action a bordered button on the editor's ground, its label centered, lit where the pointer stands
// on it and dim where it cannot be taken; the Parallel choice its box, filled where on, and its label; and under the
// bar the summary, in the error's color where the run's end counted a failure
static void tests_head_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    const GuiFrame *const frame = args->frame;
    const unsigned int size = EMBED_CALL(tests_action_size, TestsCtx, .pane = args);
    const int one = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = 1);
    const unsigned int radius = (unsigned int)EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = 6);
    const GuiColor ground = GUI_FRAME_FADED(args->palette->editor_background, frame->side_share);
    for (unsigned int at = 0u; at < sizeof(s_actions) / sizeof(s_actions[0]); at += 1u)
    {
        const unsigned int which = s_actions[at].which;
        const GuiRect button = EMBED_CALL(tests_action_rect, TestsCtx, .pane = args, .which = which);
        const int enabled = EMBED_CALL(tests_action_enabled, TestsCtx, .which = which);
        const int lit = enabled && (args->pane_read->action == which);
        const int share = enabled ? GUI_FIXED_ONE : TESTS_DISABLED_SHARE;
        const GuiColor border = GUI_FRAME_FADED(
            GUI_FRAME_FADED(lit ? args->palette->hover : args->palette->line, share), frame->side_share);
        const GuiColor text = GUI_FRAME_FADED(GUI_FRAME_FADED(lit ? args->palette->text : args->palette->text_2, share),
                                              frame->side_share);
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                   .rect =
                       EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = button, .other = args->clip),
                   .radius = radius, .color = border);
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){button.x + one, button.y + one, button.width - (2 * one),
                                                       button.height - (2 * one)},
                                      .other = args->clip),
                   .radius = (radius > (unsigned int)one) ? radius - (unsigned int)one : 0u, .color = ground);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_actions[at].label, .cap = 16ULL});
        const float width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs,
                       .face = GUI_FACE_HEAD, .size = size, .text = s_actions[at].label, .length = length);
        (void)EMBED_CALL(tests_text_draw, TestsCtx, .pane = args, .face = GUI_FACE_HEAD, .size = size,
                         .x_f32 = (float)button.x + (((float)button.width - width) / 2.0f), .y = button.y,
                         .x = button.height, .text = s_actions[at].label, .length = length, .color = text);
    }
    const GuiRect parallel = EMBED_CALL(tests_action_rect, TestsCtx, .pane = args, .which = CLI_TESTS_PARALLEL);
    const int box = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_BOX);
    const int on = EMBED_CALL(cli_tests_pane.parallel, CliTestsPaneCfg, .at = 0u);
    const GuiRect square = {parallel.x, parallel.y + ((parallel.height - box) / 2), box, box};
    const GuiColor box_color =
        GUI_FRAME_FADED((args->pane_read->action == CLI_TESTS_PARALLEL) ? args->palette->hover : args->palette->line,
                        frame->side_share);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = square, .other = args->clip),
               .radius = (unsigned int)(one * 2), .color = box_color);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){square.x + one, square.y + one, box - (2 * one), box - (2 * one)},
                                  .other = args->clip),
               .radius = (unsigned int)one,
               .color = on ? GUI_FRAME_FADED(args->palette->link, frame->side_share) : ground);
    (void)EMBED_CALL(
        tests_text_draw, TestsCtx, .pane = args, .face = GUI_FACE_HEAD, .size = size,
        .x_f32 = (float)(square.x + box + EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_ACTION_GAP)),
        .y = parallel.y, .x = parallel.height, .text = "Parallel", .length = 8ULL,
        .color = GUI_FRAME_FADED(args->palette->text_3, frame->side_share));
    char summary[CLI_TESTS_SUMMARY];
    int failed = 0;
    const unsigned long long said =
        EMBED_CALL(cli_tests_pane.summary, CliTestsPaneCfg, .text = summary, .failed_out = &failed);
    if (said != 0ULL)
    {
        const int bar = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_BAR);
        (void)EMBED_CALL(
            tests_text_draw, TestsCtx, .pane = args, .face = GUI_FACE_HEAD, .size = size,
            .x_f32 =
                (float)(args->rect.x + EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_PANE_PAD_LEFT + 2)),
            .y = args->rect.y + bar, .x = args->rect.height - bar, .text = summary, .length = said,
            .color = GUI_FRAME_FADED(failed ? args->palette->error : args->palette->text_2, frame->side_share));
    }
}

// the action whose button or box the pixel at x, y stands in, where it can be taken
static unsigned int tests_head_hit(const GuiPaneCfg *args)
{
    for (unsigned int which = CLI_TESTS_RUN; which <= CLI_TESTS_PARALLEL; which += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(tests_action_rect, TestsCtx, .pane = args, .which = which), .x = args->x,
                       .y = args->y) &&
            EMBED_CALL(tests_action_enabled, TestsCtx, .which = which))
        {
            return which;
        }
    }
    return 0u;
}

// an action taken: a run saves every view first, as the page saves them
static void tests_action(const GuiPaneCfg *args)
{
    switch (args->which)
    {
    case CLI_TESTS_RUN:
    case CLI_TESTS_RUN_FAILED:
    case CLI_TESTS_COVER:
        if (args->save_all != NULL)
        {
            args->save_all(args->context);
        }
        (void)EMBED_CALL(cli_tests_pane.run, CliTestsPaneCfg, .root = args->root,
                         .failed = args->which == CLI_TESTS_RUN_FAILED, .cover = args->which == CLI_TESTS_COVER);
        break;
    case CLI_TESTS_STOP:
        EMBED_CALL(cli_tests_pane.stop, CliTestsPaneCfg, .at = 0u);
        break;
    case CLI_TESTS_PARALLEL:
        EMBED_CALL(cli_tests_pane.parallel_set, CliTestsPaneCfg,
                   .on = !EMBED_CALL(cli_tests_pane.parallel, CliTestsPaneCfg, .at = 0u));
        break;
    default:
        break;
    }
}

// an outcome's mark, a dot of the mark's side about x, y: filled where passed, failed or skipped, a ring on `ground`
// where running or not run, in the outcome's color
EMBED_INLINE void tests_mark_draw(const TestsCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiPalette *const palette = pane->palette;
    const int mark = EMBED_CALL(tests_scaled, TestsCtx, .pane = pane, .x = GUI_TESTS_MARK);
    GuiColor color;
    int ring = 0;
    switch (args->outcome)
    {
    case CLI_TESTS_PASSED:
        color = palette->mark[1];
        break;
    case CLI_TESTS_FAILED:
        color = palette->error;
        break;
    case CLI_TESTS_SKIPPED:
        color = palette->text_3;
        break;
    case CLI_TESTS_RUNNING:
        color = palette->signal;
        ring = 1;
        break;
    default:
        color = palette->text_3;
        ring = 1;
        break;
    }
    const GuiRect dot = {args->x, args->y - (mark / 2), mark, mark};
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = pane->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = dot, .other = pane->clip),
               .radius = (unsigned int)(mark / 2), .color = GUI_FRAME_FADED(color, pane->frame->side_share));
    if (ring)
    {
        const int in = EMBED_CALL(tests_scaled, TestsCtx, .pane = pane, .x = 2);
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = pane->raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){dot.x + in, dot.y + in, mark - (2 * in), mark - (2 * in)},
                                      .other = pane->clip),
                   .radius = (unsigned int)((mark / 2) - in), .color = args->ground);
    }
}

// the color a count or a mark takes for an outcome
EMBED_INLINE GuiColor tests_outcome_color(const TestsCtx *args)
{
    const GuiPalette *const palette = args->pane->palette;
    if (args->outcome == CLI_TESTS_FAILED)
    {
        return palette->error;
    }
    return (args->outcome == CLI_TESTS_PASSED) ? palette->mark[1] : palette->text_3;
}

// a time written: under a second in milliseconds, else in seconds to a tenth
EMBED_INLINE void tests_time_write(const TestsCtx *args)
{
    VerbaLinea *const line = (VerbaLinea *)args->text;
    const unsigned int milliseconds = args->which;
    if (milliseconds < 1000u)
    {
        mmgr_line_decimal(line, milliseconds, 1u);
        mmgr_line_put(line, " ms");
        return;
    }
    const unsigned int tenths = (milliseconds + 50u) / 100u;
    mmgr_line_decimal(line, tenths / 10u, 1u);
    mmgr_line_ch(line, '.');
    mmgr_line_decimal(line, tenths % 10u, 1u);
    mmgr_line_put(line, " s");
}

// a row drawn: a file's arrow, mark, icon, name, folder and counts; a test's guide, mark, label and time; a message's
// first line; the coverage's head; a covered file's path, bar and share
static void tests_row_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiFrame *const frame = args->frame;
    const GuiPalette *const palette = args->palette;
    const GuiRect rect = args->rect;
    const GuiRect on = args->clip;
    CliTestsRow row;
    if (!EMBED_CALL(cli_tests_pane.row, CliTestsPaneCfg, .at = args->row, .row = &row))
    {
        return;
    }
    const unsigned int row_size =
        (unsigned int)EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = (int)GUI_PANE_ROW_SIZE);
    const unsigned int where_size =
        (unsigned int)EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = (int)GUI_TESTS_WHERE_SIZE);
    const unsigned int count_size =
        (unsigned int)EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = (int)GUI_TESTS_COUNT_SIZE);
    const int pad = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_PANE_PAD_LEFT);
    const int gap = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_GAP);
    const int mark = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_MARK);
    const int icon = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_ICON);
    const int arrow = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_PANE_ARROW);
    const int twisty = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_EXPLORER_TWISTY);
    // the rows are fewer than an int counts
    const GuiColor ground = GUI_FRAME_FADED(
        (args->pane_read->pointed == (int)args->row) ? palette->line : palette->editor_background, frame->side_share);
    const int middle = rect.y + (rect.height / 2);
    const int baseline =
        EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_HEAD,
                   .size = row_size, .top = rect.y, .pixel_height = rect.height);
    const int right = rect.x + rect.width - pad;
    switch (row.kind)
    {
    case CLI_TESTS_ROW_FILE: {
        int x = rect.x + pad;
        EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = raster, .x = x + ((twisty - arrow) / 2),
                   .y = middle - (arrow / 2), .side = arrow, .open = row.open,
                   .color = GUI_FRAME_FADED(palette->text_2, frame->side_share), .clip = on);
        x += twisty;
        EMBED_CALL(tests_mark_draw, TestsCtx, .pane = args, .outcome = row.outcome, .x = x, .y = middle,
                   .ground = ground);
        x += mark + gap;
        char mark_text[GUI_EXPLORER_MARK_MOST];
        const unsigned int mark_color = EMBED_CALL(gui_explorer_mark_table.mark, GuiExplorerMarkCfg, .name = row.name,
                                                   .length = row.name_length, .text = mark_text);
        const unsigned long long mark_length =
            cellul.len(&(CatenaFinitaCfg){.src = mark_text, .cap = GUI_EXPLORER_MARK_MOST});
        const unsigned int mark_size =
            (unsigned int)EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = (int)GUI_EXPLORER_MARK_SIZE);
        const float mark_width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                       .face = GUI_FACE_CODE_BOLD, .size = mark_size, .text = mark_text, .length = mark_length);
        (void)EMBED_CALL(
            gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
            .face = GUI_FACE_CODE_BOLD, .size = mark_size, .x_f32 = (float)x + (((float)icon - mark_width) / 2.0f),
            .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                            .face = GUI_FACE_CODE_BOLD, .size = mark_size, .top = rect.y, .pixel_height = rect.height),
            .text = mark_text, .length = mark_length,
            .color = GUI_FRAME_FADED((mark_color != 0u) ? palette->mark[mark_color - 1u] : palette->text_3,
                                     frame->side_share),
            .clip = on);
        x += icon + gap;
        // the counts at the right, failed, passed and skipped, each where it is not 0, right to left
        const unsigned int outcomes[3] = {CLI_TESTS_SKIPPED, CLI_TESTS_PASSED, CLI_TESTS_FAILED};
        const unsigned int counts[3] = {row.skipped, row.passed, row.failed};
        const int between = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = 6);
        float count_x = (float)right;
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            if (counts[at] == 0u)
            {
                continue;
            }
            char digits[16];
            VerbaLinea count = {digits, sizeof(digits), 0ULL};
            mmgr_line_decimal(&count, counts[at], 1u);
            const float width =
                EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                           .face = GUI_FACE_CODE, .size = count_size, .text = digits, .length = count.at);
            count_x -= width;
            (void)EMBED_CALL(
                gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                .face = GUI_FACE_CODE, .size = count_size, .x_f32 = count_x,
                .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                .face = GUI_FACE_CODE, .size = count_size, .top = rect.y, .pixel_height = rect.height),
                .text = digits, .length = count.at,
                .color =
                    GUI_FRAME_FADED(EMBED_CALL(tests_outcome_color, TestsCtx, .pane = args, .outcome = outcomes[at]),
                                    frame->side_share),
                .clip = on);
            count_x -= (float)between;
        }
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, (int)count_x - gap - rect.x, rect.height}, .other = on);
        const float after = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster,
                                       .glyphs = glyphs, .face = GUI_FACE_HEAD, .size = row_size, .x_f32 = (float)x,
                                       .y = baseline, .text = row.name, .length = row.name_length,
                                       .color = GUI_FRAME_FADED(palette->text, frame->side_share), .clip = text_clip);
        if (row.folder_length != 0ULL)
        {
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_HEAD, .size = where_size, .x_f32 = after + (float)gap, .y = baseline,
                             .text = row.path, .length = row.folder_length,
                             .color = GUI_FRAME_FADED(palette->text_3, frame->side_share), .clip = text_clip);
        }
        break;
    }
    case CLI_TESTS_ROW_TEST: {
        int x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = raster, .palette = palette, .frame = frame,
                           .rect = rect, .clip = on, .depth = 1u);
        EMBED_CALL(tests_mark_draw, TestsCtx, .pane = args, .outcome = row.outcome, .x = x, .y = middle,
                   .ground = ground);
        x += mark + gap;
        float time_x = (float)right;
        if (row.milliseconds != 0u)
        {
            char digits[24];
            VerbaLinea time = {digits, sizeof(digits), 0ULL};
            EMBED_CALL(tests_time_write, TestsCtx, .which = row.milliseconds, .text = (const char *)&time);
            const float width =
                EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                           .face = GUI_FACE_HEAD, .size = where_size, .text = digits, .length = time.at);
            time_x -= width;
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_HEAD, .size = where_size, .x_f32 = time_x, .y = baseline, .text = digits,
                             .length = time.at, .color = GUI_FRAME_FADED(palette->text_3, frame->side_share),
                             .clip = on);
        }
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, (int)time_x - gap - rect.x, rect.height}, .other = on);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = row_size, .x_f32 = (float)x, .y = baseline, .text = row.name,
                         .length = row.name_length, .color = GUI_FRAME_FADED(palette->text_2, frame->side_share),
                         .clip = text_clip);
        break;
    }
    case CLI_TESTS_ROW_MESSAGE: {
        const unsigned int message_size =
            (unsigned int)EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = (int)GUI_TESTS_MESSAGE_SIZE);
        (void)EMBED_CALL(
            gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
            .face = GUI_FACE_CODE, .size = message_size,
            .x_f32 = (float)(rect.x + EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_MESSAGE_PAD)),
            .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_CODE,
                            .size = message_size, .top = rect.y, .pixel_height = rect.height),
            .text = row.name, .length = row.name_length, .color = GUI_FRAME_FADED(palette->error, frame->side_share),
            .clip = on);
        break;
    }
    case CLI_TESTS_ROW_COVER_HEAD: {
        char text[96];
        VerbaLinea line = {text, sizeof(text), 0ULL};
        const unsigned int could = row.passed + row.failed;
        mmgr_line_put(&line, "Coverage: ");
        mmgr_line_decimal(&line, (could != 0u) ? ((100u * row.passed) + (could / 2u)) / could : 0u, 1u);
        mmgr_line_put(&line, "% of ");
        mmgr_line_decimal(&line, could, 1u);
        mmgr_line_put(&line, " lines");
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD_BOLD, .size = where_size + 1u, .x_f32 = (float)(rect.x + pad),
                         .y = baseline, .text = text, .length = line.at,
                         .color = GUI_FRAME_FADED(palette->text_2, frame->side_share), .clip = on);
        break;
    }
    case CLI_TESTS_ROW_COVER: {
        const unsigned int total = row.passed + row.failed;
        const unsigned int share = (total != 0u) ? ((100u * row.passed) + (total / 2u)) / total : 0u;
        char digits[8];
        VerbaLinea percent = {digits, sizeof(digits), 0ULL};
        mmgr_line_decimal(&percent, share, 1u);
        mmgr_line_ch(&percent, '%');
        const int percent_width = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = 36);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = where_size, .x_f32 = (float)(right - percent_width),
                         .y = baseline, .text = digits, .length = percent.at,
                         .color = GUI_FRAME_FADED(palette->text_3, frame->side_share), .clip = on);
        const int bar_width = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_COVER_WIDTH);
        const int bar_height = EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_COVER_HEIGHT);
        const unsigned int bar_radius =
            (unsigned int)EMBED_CALL(tests_scaled, TestsCtx, .pane = args, .x = GUI_TESTS_COVER_RADIUS);
        const GuiRect bar = {right - percent_width - gap - bar_width, middle - (bar_height / 2), bar_width, bar_height};
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = bar, .other = on),
                   .radius = bar_radius,
                   .color =
                       GUI_FRAME_FADED(GUI_FRAME_FADED(palette->error, (GUI_FIXED_ONE * 55) / 100), frame->side_share));
        // the bar's ran share is fewer pixels than an int counts
        const int ran_width = (total != 0u) ? (int)(((unsigned long long)bar_width * row.passed) / total) : 0;
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){bar.x, bar.y, ran_width, bar.height}, .other = on),
                   .radius = bar_radius, .color = GUI_FRAME_FADED(palette->mark[1], frame->side_share));
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, bar.x - gap - rect.x, rect.height}, .other = on);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = row_size, .x_f32 = (float)(rect.x + pad), .y = baseline,
                         .text = row.name, .length = row.name_length,
                         .color = GUI_FRAME_FADED(palette->text_2, frame->side_share), .clip = text_clip);
        break;
    }
    default:
        break;
    }
}

// a file's row pressed closes or opens its tests; a test's opens the file where it failed or where it is written; a
// covered file's opens it at its first line missed
static int tests_press(const GuiPaneCfg *args)
{
    CliTestsRow row;
    if (!EMBED_CALL(cli_tests_pane.row, CliTestsPaneCfg, .at = args->row, .row = &row))
    {
        return 0;
    }
    if (row.kind == CLI_TESTS_ROW_FILE)
    {
        EMBED_CALL(cli_tests_pane.toggle, CliTestsPaneCfg, .at = args->row);
        return 0;
    }
    if ((row.kind != CLI_TESTS_ROW_TEST) && (row.kind != CLI_TESTS_ROW_COVER))
    {
        return 0;
    }
    *args->open_out = (GuiPaneOpen){row.path, {row.line, 0ULL}, 1};
    return 1;
}

// the tree's tests read as the pane first shows
static void tests_read(const GuiPaneCfg *args)
{
    if ((args->event == GUI_PANE_SHOWN) && !EMBED_CALL(cli_tests_pane.loaded, CliTestsPaneCfg, .at = 0u))
    {
        EMBED_CALL(cli_tests_pane.read, CliTestsPaneCfg, .root = args->root);
    }
}

const GuiPaneKind gui_tests_kind = {
    .tool = "tests",
    .title = tests_title,
    .rows = tests_rows,
    .head = tests_head,
    .head_draw = tests_head_draw,
    .head_hit = tests_head_hit,
    .action = tests_action,
    .row_draw = tests_row_draw,
    .press = tests_press,
    .read = tests_read,
};
