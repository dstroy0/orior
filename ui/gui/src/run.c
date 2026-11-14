// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run.c: the Run pane's rows as the CLI's job runs answer them, each drawn by its type from the row's texts and
// flags, a line in its pieces as its spans cut it; its head, the filter's field, Start and Stop with the refusal under
// them; and every press, key and text handed to the CLI, which decides what each does
#include "run.h"

#include "../../cli/src/runs/job_runs.h"

#define RUN_PLACEHOLDER "Filter jobs"
#define RUN_START "Start"
#define RUN_STOP "Stop"

typedef struct
{
    const GuiPaneCfg *pane;
    const CliJobRunRow *row;
    float x_f32;
    const char *text;
    unsigned long long length;
    GuiColor color;
    unsigned int face;
} RunCtx;

static const char *run_title(const GuiPaneCfg *args)
{
    (void)args;
    return "Run";
}

static unsigned int run_rows(const GuiPaneCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u);
}

// the field's row, the buttons' row and the refusal's row
static int run_head(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_PANE_BOX + GUI_PANE_CHOICE + GUI_RUN_REFUSAL;
}

// the filter's field, in the head's first row
EMBED_INLINE GuiRect run_field_rect(const RunCtx *args)
{
    return EMBED_CALL(gui_pane_field.field_rect, GuiPaneFieldCfg, .pane = args->pane, .y = args->pane->rect.y);
}

// the buttons' row, under the field's: Start in its left half, Stop in its right
EMBED_INLINE GuiRect run_buttons_rect(const RunCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    return (GuiRect){
        pane->rect.x, pane->rect.y + EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_BOX),
        pane->rect.width, EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_CHOICE)};
}

EMBED_INLINE GuiRect run_start_rect(const RunCtx *args)
{
    const GuiRect buttons = EMBED_CALL(run_buttons_rect, RunCtx, .pane = args->pane);
    return (GuiRect){buttons.x, buttons.y, buttons.width / 2, buttons.height};
}

EMBED_INLINE GuiRect run_stop_rect(const RunCtx *args)
{
    const GuiRect buttons = EMBED_CALL(run_buttons_rect, RunCtx, .pane = args->pane);
    return (GuiRect){buttons.x + (buttons.width / 2), buttons.y, buttons.width - (buttons.width / 2), buttons.height};
}

// the refusal's row, under the buttons
EMBED_INLINE GuiRect run_refusal_rect(const RunCtx *args)
{
    const GuiRect buttons = EMBED_CALL(run_buttons_rect, RunCtx, .pane = args->pane);
    return (GuiRect){buttons.x, buttons.y + buttons.height, buttons.width,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane, .x = GUI_RUN_REFUSAL)};
}

// the head drawn: the filter's field with the filter typed, the caret while it takes the keys; Start; Stop, quieter
// while no run shown goes; and the refusal of the last start under them where it stands
static void run_head_draw(const GuiPaneCfg *args)
{
    const GuiPalette *const palette = args->palette;
    const GuiRect field = EMBED_CALL(run_field_rect, RunCtx, .pane = args);
    // the filter copied out of the store's cell for the draw
    char filter[CLI_JOB_RUNS_FIELD_MAX];
    const unsigned long long length = EMBED_CALL(cli_job_runs_field.filter, CliJobRunsFieldCfg, .text_out = filter);
    EMBED_CALL(gui_pane_field.field_draw, GuiPaneFieldCfg, .pane = args, .rect = field, .right = field.x + field.width,
               .text = filter, .length = length, .placeholder = RUN_PLACEHOLDER,
               .keys = args->pane_read->keys &&
                       EMBED_CALL(cli_job_runs_field.focused, CliJobRunsFieldCfg, .length_out = NULL));
    EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args,
               .rect = EMBED_CALL(run_start_rect, RunCtx, .pane = args), .text = RUN_START,
               .length = sizeof(RUN_START) - 1ULL, .pointed = args->pane_read->action == GUI_RUN_START);
    const GuiRect stop = EMBED_CALL(run_stop_rect, RunCtx, .pane = args);
    if (EMBED_CALL(cli_job_runs_run.live, CliJobRunsRunCfg, .root = args->root))
    {
        EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args, .rect = stop, .text = RUN_STOP,
                   .length = sizeof(RUN_STOP) - 1ULL, .pointed = args->pane_read->action == GUI_RUN_STOP);
    }
    else
    {
        // the box a choice stands in, its text in the quietest color
        const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
        const int one = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = 1);
        const GuiRect box = {stop.x + pad, stop.y + one, stop.width - (2 * pad), stop.height - (2 * one)};
        EMBED_CALL(gui_pane_field.button_draw, GuiPaneFieldCfg, .pane = args, .rect = box, .radius = GUI_PANE_RADIUS,
                   .color = palette->line);
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                         .size = GUI_PANE_CHOICE_SIZE,
                         .x_f32 = (float)(box.x + EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args,
                                                             .x = GUI_PANE_FIELD_PAD)),
                         .text = RUN_STOP, .length = sizeof(RUN_STOP) - 1ULL, .color = palette->text_3, .clip = box);
    }
    unsigned long long refused = 0ULL;
    const char *const refusal = EMBED_CALL(cli_job_runs_run.refusal, CliJobRunsRunCfg, .length_out = &refused);
    if (refused != 0ULL)
    {
        const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                         .size = GUI_RUN_REFUSAL_SIZE, .x_f32 = (float)(args->rect.x + pad), .text = refusal,
                         .length = refused, .color = palette->error,
                         .clip = EMBED_CALL(run_refusal_rect, RunCtx, .pane = args));
    }
}

// the action at x, y: the field, Start or Stop where the pixel stands in it
static unsigned int run_head_hit(const GuiPaneCfg *args)
{
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(run_field_rect, RunCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return GUI_RUN_FIELD;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(run_start_rect, RunCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return GUI_RUN_START;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(run_stop_rect, RunCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return GUI_RUN_STOP;
    }
    return 0u;
}

// an action taken: the field takes the keys for the filter; Start starts the job chosen, every view saved first; Stop
// stops the run shown
static void run_action(const GuiPaneCfg *args)
{
    switch (args->which)
    {
    case GUI_RUN_FIELD:
        EMBED_CALL(cli_job_runs_field.filter_focus, CliJobRunsFieldCfg, .length_out = NULL);
        if (args->pane != NULL)
        {
            args->pane->keys = 1;
        }
        return;
    case GUI_RUN_START:
        if (args->save_all != NULL)
        {
            args->save_all(args->context);
        }
        (void)EMBED_CALL(cli_job_runs_run.start, CliJobRunsRunCfg, .root = args->root);
        return;
    case GUI_RUN_STOP:
        (void)EMBED_CALL(cli_job_runs_run.stop, CliJobRunsRunCfg, .root = args->root);
        return;
    default:
        return;
    }
}

// `length` bytes of `text` drawn from `x_f32` in `color` and `face`, centered in the row: the x after them
EMBED_INLINE float run_text_draw(const RunCtx *args)
{
    return EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args->pane, .face = args->face,
                      .size = (args->face == GUI_FACE_HEAD) ? GUI_PANE_ROW_SIZE : GUI_RUN_LINE_SIZE,
                      .x_f32 = args->x_f32, .text = args->text, .length = args->length, .color = args->color);
}

// the color of a run's state: signal while it goes, the second mark's where it ended with 0, the error's otherwise
EMBED_INLINE GuiColor run_state_color(const RunCtx *args)
{
    const GuiPalette *const palette = args->pane->palette;
    const unsigned int flags = args->row->flags;
    return ((flags & CLI_JOB_RUN_LIVE) != 0u)     ? palette->signal
           : ((flags & CLI_JOB_RUN_OK) != 0u)     ? palette->mark[1]
           : ((flags & CLI_JOB_RUN_FAILED) != 0u) ? palette->error
                                                  : palette->text_2;
}

// a line of the run shown drawn from `x_f32` in the code face: each piece its spans cut in the span's color of the
// sixteen and bold where the span is, the rest in the stream's color, the errors' in the error color and the commands'
// quieter
EMBED_INLINE void run_line_draw(const RunCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiPalette *const palette = pane->palette;
    CliJobRunLine line;
    if (!EMBED_CALL(cli_job_runs_row.line, CliJobRunsRowCfg, .row_at = pane->row, .line_out = &line))
    {
        return;
    }
    const GuiColor base = (line.stream == CLI_JOB_RUN_STDERR)    ? palette->error
                          : (line.stream == CLI_JOB_RUN_COMMAND) ? palette->text_3
                                                                 : palette->editor_text;
    float x = args->x_f32;
    unsigned long long at = 0ULL;
    for (unsigned int each = 0u; each < line.spans; each += 1u)
    {
        const CliTerminalXtermSpan *const span = &line.span[each];
        if (span->offset > at)
        {
            x = EMBED_CALL(run_text_draw, RunCtx, .pane = pane, .x_f32 = x, .text = line.text + at,
                           .length = span->offset - at, .color = base, .face = GUI_FACE_CODE);
        }
        x = EMBED_CALL(run_text_draw, RunCtx, .pane = pane, .x_f32 = x, .text = line.text + span->offset,
                       .length = span->length,
                       .color = (span->color < GUI_TERMINAL_COLORS) ? palette->terminal[span->color] : base,
                       .face = span->bold ? GUI_FACE_CODE_BOLD : GUI_FACE_CODE);
        at = (unsigned long long)span->offset + span->length;
    }
    if (at < line.length)
    {
        (void)EMBED_CALL(run_text_draw, RunCtx, .pane = pane, .x_f32 = x, .text = line.text + at,
                         .length = line.length - at, .color = base, .face = GUI_FACE_CODE);
    }
}

// a box `GUI_RUN_BOX` across at `x_f32`, lit in signal where the row's choice is taken
EMBED_INLINE void run_box_draw(const RunCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiPalette *const palette = pane->palette;
    const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_RUN_BOX);
    const int lit = (args->row->flags & CLI_JOB_RUN_CHOSEN) != 0u;
    // the x is a pixel's place in the pane, inside an int
    EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = pane,
               .rect = (GuiRect){(int)args->x_f32, pane->rect.y + ((pane->rect.height - side) / 2), side, side},
               .radius = GUI_RUN_BOX_RADIUS, .color = lit ? palette->signal : palette->line,
               .inside = lit ? palette->signal : palette->editor_background);
}

// the row drawn as its type, from its texts and flags: the chosen job and the run shown on the idle selection's ground
static void run_row_draw(const GuiPaneCfg *args)
{
    CliJobRunRow row;
    if (!EMBED_CALL(cli_job_runs_row.row, CliJobRunsRowCfg, .row_at = args->row, .row_out = &row))
    {
        return;
    }
    const GuiPalette *const palette = args->palette;
    const int share = EMBED_CALL(gui_pane_part.share, GuiPanePartCfg, .pane = args);
    // the rows are fewer than an int counts
    if (((row.flags & (CLI_JOB_RUN_CHOSEN | CLI_JOB_RUN_SHOWN)) != 0u) && (row.type != CLI_JOB_RUN_CHOICE) &&
        (args->pane_read->pointed != (int)args->row))
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = args->clip,
                   .color = GUI_FRAME_FADED(palette->selection_idle, share));
    }
    const int x =
        EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = args->raster, .palette = palette, .frame = args->frame,
                   .table = args->table, .rect = args->rect, .clip = args->clip, .depth = row.depth);
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_RUN_GAP);
    float after = (float)x;
    switch (row.type)
    {
    case CLI_JOB_RUN_GROUP: {
        const int arrow = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_ARROW);
        EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = args->raster, .x = x,
                   .y = args->rect.y + ((args->rect.height - arrow) / 2), .side = arrow,
                   .open = (row.flags & CLI_JOB_RUN_OPEN) != 0u, .color = GUI_FRAME_FADED(palette->text_2, share),
                   .clip = args->clip);
        after = EMBED_CALL(run_text_draw, RunCtx, .pane = args, .x_f32 = (float)(x + arrow + gap), .text = row.text,
                           .length = row.text_length, .color = palette->text_2, .face = GUI_FACE_HEAD);
        (void)EMBED_CALL(run_text_draw, RunCtx, .pane = args, .x_f32 = after + (float)gap, .text = row.second,
                         .length = row.second_length, .color = palette->text_3, .face = GUI_FACE_HEAD);
        return;
    }
    case CLI_JOB_RUN_JOB: {
        const int dot = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_RUN_DOT);
        if ((row.flags & CLI_JOB_RUN_LIVE) != 0u)
        {
            EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = args,
                       .rect = (GuiRect){x, args->rect.y + ((args->rect.height - dot) / 2), dot, dot},
                       .radius = GUI_RUN_DOT / 2, .color = palette->signal, .inside = palette->signal);
        }
        (void)EMBED_CALL(run_text_draw, RunCtx, .pane = args, .x_f32 = (float)(x + dot + gap), .text = row.text,
                         .length = row.text_length, .color = palette->text, .face = GUI_FACE_HEAD);
        return;
    }
    case CLI_JOB_RUN_ABOUT:
        after = EMBED_CALL(run_text_draw, RunCtx, .pane = args, .x_f32 = after, .text = row.text,
                           .length = row.text_length, .color = palette->link, .face = GUI_FACE_HEAD);
        (void)EMBED_CALL(run_text_draw, RunCtx, .pane = args, .x_f32 = after + (float)gap, .text = row.second,
                         .length = row.second_length, .color = palette->text_3, .face = GUI_FACE_HEAD);
        return;
    case CLI_JOB_RUN_PARAM: {
        after =
            EMBED_CALL(run_text_draw, RunCtx, .pane = args, .x_f32 = after, .text = row.text, .length = row.text_length,
                       .color = ((row.flags & CLI_JOB_RUN_REQUIRED) != 0u) ? palette->signal : palette->text_2,
                       .face = GUI_FACE_HEAD);
        after = EMBED_CALL(run_text_draw, RunCtx, .pane = args, .x_f32 = after + (float)gap, .text = row.second,
                           .length = row.second_length,
                           .color = ((row.flags & CLI_JOB_RUN_DEFAULTED) != 0u) ? palette->text_3 : palette->text,
                           .face = GUI_FACE_CODE);
        if (((row.flags & CLI_JOB_RUN_FOCUSED) != 0u) && args->pane_read->keys)
        {
            // the caret after the value, at the row's middle half
            const int caret = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_CARET);
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                          .one = (GuiRect){(int)(after + 0.5f), args->rect.y + (args->rect.height / 4),
                                                           caret, args->rect.height / 2},
                                          .other = args->clip),
                       .color = GUI_FRAME_FADED(palette->caret, share));
        }
        return;
    }
    case CLI_JOB_RUN_CHOICE: {
        EMBED_CALL(run_box_draw, RunCtx, .pane = args, .row = &row, .x_f32 = after);
        const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_RUN_BOX);
        (void)EMBED_CALL(run_text_draw, RunCtx, .pane = args, .x_f32 = after + (float)(side + gap), .text = row.text,
                         .length = row.text_length, .color = palette->text_2, .face = GUI_FACE_HEAD);
        return;
    }
    case CLI_JOB_RUN_LINE:
        EMBED_CALL(run_line_draw, RunCtx, .pane = args, .x_f32 = after);
        return;
    default:
        (void)EMBED_CALL(run_text_draw, RunCtx, .pane = args, .x_f32 = after, .text = row.text,
                         .length = row.text_length,
                         .color = EMBED_CALL(run_state_color, RunCtx, .pane = args, .row = &row),
                         .face = (row.type == CLI_JOB_RUN_RUN) ? GUI_FACE_HEAD : GUI_FACE_CODE);
        return;
    }
}

// the row pressed handed to the CLI, the pane taking the keys where the row does; a press opens no file
static int run_press(const GuiPaneCfg *args)
{
    const int keys = EMBED_CALL(cli_job_runs_row.press, CliJobRunsRowCfg, .row_at = args->row);
    if (keys && (args->pane != NULL))
    {
        args->pane->keys = 1;
    }
    return 0;
}

// text typed goes to the field that takes the keys
static int run_text(const GuiPaneCfg *args)
{
    EMBED_CALL(cli_job_runs_field.type, CliJobRunsFieldCfg, .bytes = args->text, .length = args->length);
    return 1;
}

// Escape lets the keys go; every other key goes to the CLI by its name
static int run_key(const GuiPaneCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        args->pane->keys = 0;
        return 1;
    }
    return EMBED_CALL(cli_job_runs_field.key, CliJobRunsFieldCfg, .key_name = args->name);
}

// the jobs read anew of the tree as the pane shows
static void run_read(const GuiPaneCfg *args)
{
    if (args->event == GUI_PANE_SHOWN)
    {
        EMBED_CALL(cli_job_runs_root.read, CliJobRunsRootCfg, .root = args->root, .catalog = NULL);
    }
}

void gui_run_follow(const GuiPaneCfg *args)
{
    const GuiRect region = EMBED_CALL(gui_pane.region, GuiPaneCfg, .table = args->table, .frame = args->frame);
    const int top = EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = args->table, .frame = args->frame);
    const int height = EMBED_CALL(gui_pane.row_height, GuiPaneCfg, .table = args->table, .frame = args->frame);
    const int capacity = (height > 0) ? ((region.y + region.height - top) / height) : 0;
    if (capacity <= 0)
    {
        return;
    }
    // the capacity is positive, as checked above
    args->pane->scroll = EMBED_CALL(cli_job_runs_scroll.follow, CliJobRunsScrollCfg, .scroll_top = args->pane->scroll,
                                    .capacity = (unsigned int)capacity);
}

const GuiPaneTable gui_run_table = {
    .tool = "run",
    .foot = 1,
    .title = run_title,
    .rows = run_rows,
    .head = run_head,
    .head_draw = run_head_draw,
    .head_hit = run_head_hit,
    .action = run_action,
    .row_draw = run_row_draw,
    .press = run_press,
    .text = run_text,
    .key = run_key,
    .read = run_read,
};
