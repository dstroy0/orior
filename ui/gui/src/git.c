// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git.c: the Git pane's repository choice, field and rows as the Git pane lists them
#include "git.h"

#include "../../cli/src/code/git.h"
#include "../../cli/src/command_line/cli_internal.h"
#include "changes.h"
#include "explorer.h"

// what the field reads while the query is empty
#define GIT_PLACEHOLDER "Search commits"

// the palette's mark a line of the graph takes, by the line's number: the page's tones t4, t2, t5, t3, t6 and t1 in
// turn
static const unsigned int s_tones[6] = {3u, 1u, 4u, 2u, 5u, 0u};

typedef struct
{
    const GuiPaneCfg *pane;
    int x;
    const unsigned int *stroke;
    GuiColor color;
    float x0_f32;
    float y0_f32;
    float x1_f32;
    float y1_f32;
} GitCtx;

static const char *git_title(const GuiPaneCfg *args)
{
    (void)args;
    return "Git";
}

static unsigned int git_rows(const GuiPaneCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_git_log.rows, CliGitLogCfg, .at = 0u);
}

// whether the tree has more than one repository, which puts the choice of repository over the field
EMBED_INLINE int git_repos_listed(const GitCtx *args)
{
    (void)args;
    unsigned int count = 0u;
    (void)EMBED_CALL(cli_git_log.repo, CliGitLogCfg, .count_out = &count);
    return count >= 2u;
}

// the choice's row where the tree has more than one repository, and the field's row
static int git_head(const GuiPaneCfg *args)
{
    return (EMBED_CALL(git_repos_listed, GitCtx, .pane = args) ? GUI_PANE_CHOICE : 0) + GUI_PANE_BOX;
}

// the choice's row at the head's top, empty where the tree has one repository; and the field's rectangle under it
EMBED_INLINE GuiRect git_choice_rect(const GitCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    return (GuiRect){pane->rect.x, pane->rect.y, pane->rect.width,
                     EMBED_CALL(git_repos_listed, GitCtx, .pane = pane)
                         ? EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_CHOICE)
                         : 0};
}

EMBED_INLINE GuiRect git_field_rect(const GitCtx *args)
{
    const GuiRect choice = EMBED_CALL(git_choice_rect, GitCtx, .pane = args->pane);
    return EMBED_CALL(gui_pane_field.field_rect, GuiPaneFieldCfg, .pane = args->pane, .y = choice.y + choice.height);
}

// the head drawn: the repository chosen in its box where the tree has more than one, and the field with the query
static void git_head_draw(const GuiPaneCfg *args)
{
    const GuiRect choice = EMBED_CALL(git_choice_rect, GitCtx, .pane = args);
    if (choice.height != 0)
    {
        unsigned int count = 0u;
        const char *const label = EMBED_CALL(cli_git_log.repo, CliGitLogCfg, .count_out = &count);
        EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args, .rect = choice, .text = label,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = label, .cap = CLI_NAME_MAX_LEN * 2ULL}),
                   .pointed = args->pane_read->action == CLI_GIT_LOG_REPO);
    }
    const GuiRect field = EMBED_CALL(git_field_rect, GitCtx, .pane = args);
    char query[CLI_GIT_LOG_QUERY_MAX];
    const unsigned long long query_length = EMBED_CALL(cli_git_log.query, CliGitLogCfg, .text_out = query);
    EMBED_CALL(gui_pane_field.field_draw, GuiPaneFieldCfg, .pane = args, .rect = field, .right = field.x + field.width,
               .text = query, .length = query_length, .placeholder = GIT_PLACEHOLDER, .keys = args->pane_read->keys);
}

// the action at x, y: the field where the pixel stands in it, else the choice
static unsigned int git_head_hit(const GuiPaneCfg *args)
{
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(git_field_rect, GitCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return CLI_GIT_LOG_FIELD;
    }
    const GuiRect choice = EMBED_CALL(git_choice_rect, GitCtx, .pane = args);
    if ((choice.height != 0) &&
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = choice, .x = args->x, .y = args->y))
    {
        return CLI_GIT_LOG_REPO;
    }
    return 0u;
}

// an action taken: the field takes the keys; the choice moves on to the next repository
static void git_action(const GuiPaneCfg *args)
{
    if (args->which == CLI_GIT_LOG_FIELD)
    {
        if (args->pane != NULL)
        {
            args->pane->keys = 1;
        }
        return;
    }
    EMBED_CALL(cli_git_log.option, CliGitLogCfg, .which = args->which, .root = args->root,
               .now_ms = cli_clock_milliseconds());
}

// text typed goes into the query
static int git_text(const GuiPaneCfg *args)
{
    EMBED_CALL(cli_git_log.type, CliGitLogCfg, .text = args->text, .size = args->length,
               .now_ms = cli_clock_milliseconds());
    return 1;
}

// Escape lets the keys go; Backspace and Enter go to the query
static int git_key(const GuiPaneCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        args->pane->keys = 0;
        return 1;
    }
    return EMBED_CALL(cli_git_log.key, CliGitLogCfg, .name = args->name, .now_ms = cli_clock_milliseconds());
}

// the branch's row is taller than the others
static int git_row_height(const GuiPaneCfg *args)
{
    CliGitLogRow row;
    const int branch =
        EMBED_CALL(cli_git_log.row, CliGitLogCfg, .at = args->row, .row = &row) && (row.type == CLI_GIT_LOG_ROW_BRANCH);
    return branch ? GUI_GIT_BRANCH_ROW : GUI_PANE_ROW;
}

// a segment of a stroke from x0, y0 to x1, y1 in `color`, the stroke's width in page pixels: the line, and a second
// beside it at the width's remainder in the color's share where the width passes a pixel
EMBED_INLINE void git_segment_draw(const GitCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const float width = GUI_GIT_STROKE_F32 * (float)pane->frame->scale / 100.0f;
    // the segment's ends are pixels of the raster, far inside what an int represents
    const int x0 = (int)(args->x0_f32 + 0.5f);
    const int y0 = (int)(args->y0_f32 + 0.5f);
    const int x1 = (int)(args->x1_f32 + 0.5f);
    const int y1 = (int)(args->y1_f32 + 0.5f);
    EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = pane->raster, .color = args->color, .x0 = x0,
               .y0 = y0, .x1 = x1, .y1 = y1, .clip = pane->clip);
    // the width past the first pixel is drawn one pixel over at its share
    const int whole = (int)width;
    const float rest = width - (float)whole;
    const GuiColor beside =
        (rest > 0.0f) ? GUI_FRAME_FADED(args->color, (unsigned int)(rest * (float)GUI_FIXED_ONE)) : args->color;
    for (int over = 1; (over < whole) || ((over == whole) && (rest > 0.0f)); over += 1)
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = pane->raster,
                   .color = (over == whole) ? beside : args->color, .x0 = x0 + over, .y0 = y0, .x1 = x1 + over,
                   .y1 = y1, .clip = pane->clip);
    }
}

// a stroke of the row drawn from the columns' left `x`: from its column at the row's top to its column at the middle,
// or from the middle to the bottom where its half is set, straight where the columns are one and as a curve between
// them where not
EMBED_INLINE void git_stroke_draw(const GitCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiRect rect = pane->rect;
    const float lane = (float)EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_GIT_LANE);
    const float middle = (float)rect.height / 2.0f;
    const float top = (args->stroke[2] != 0u) ? middle : 0.0f;
    const float bottom = (args->stroke[2] != 0u) ? (float)rect.height : middle;
    const float x0 = (float)args->x + (lane / 2.0f) + ((float)args->stroke[0] * lane);
    const float x1 = (float)args->x + (lane / 2.0f) + ((float)args->stroke[1] * lane);
    const float y0 = (float)rect.y + top;
    const float y1 = (float)rect.y + bottom;
    if (args->stroke[0] == args->stroke[1])
    {
        EMBED_CALL(git_segment_draw, GitCtx, .pane = pane, .color = args->color, .x0_f32 = x0, .y0_f32 = y0,
                   .x1_f32 = x0, .y1_f32 = y1);
        return;
    }
    // the curve bends halfway down from each end, as the page's path does, drawn in straight segments
    const float bend = (bottom - top) / 2.0f;
    float from_x = x0;
    float from_y = y0;
    for (unsigned int step = 1u; step <= GUI_GIT_CURVE_STEPS; step += 1u)
    {
        const float at = (float)step / (float)GUI_GIT_CURVE_STEPS;
        const float left = 1.0f - at;
        const float weight_0 = left * left * left;
        const float weight_1 = 3.0f * left * left * at;
        const float weight_2 = 3.0f * left * at * at;
        const float weight_3 = at * at * at;
        const float to_x = ((weight_0 + weight_1) * x0) + ((weight_2 + weight_3) * x1);
        const float to_y = (weight_0 * y0) + (weight_1 * (y0 + bend)) + (weight_2 * (y1 - bend)) + (weight_3 * y1);
        EMBED_CALL(git_segment_draw, GitCtx, .pane = pane, .color = args->color, .x0_f32 = from_x, .y0_f32 = from_y,
                   .x1_f32 = to_x, .y1_f32 = to_y);
        from_x = to_x;
        from_y = to_y;
    }
}

// a row drawn: the branch's name bold with the commits counted after it; a commit's strokes and dot, the branches and
// tags at it in pills, its subject cut before its author and its day at the right; a file's guide, mark, name, folder
// and state letter; or the notice
static void git_row_draw(const GuiPaneCfg *args)
{
    const GuiPalette *const palette = args->palette;
    const GuiFrame *const frame = args->frame;
    const GuiRect rect = args->rect;
    CliGitLogRow row;
    if (!EMBED_CALL(cli_git_log.row, CliGitLogCfg, .at = args->row, .row = &row))
    {
        return;
    }
    // the rows are fewer than an int counts
    const int pointed = args->pane_read->pointed == (int)args->row;
    const GuiColor text_color = pointed ? palette->text : palette->text_2;
    if (row.type == CLI_GIT_LOG_ROW_BRANCH)
    {
        const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_BRANCH_PAD);
        const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_BRANCH_GAP);
        const float after =
            EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .face = GUI_FACE_HEAD_BOLD,
                       .size = GUI_PANE_ROW_SIZE, .x_f32 = (float)(rect.x + pad), .text = row.text,
                       .length = row.text_length, .color = palette->text);
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .face = GUI_FACE_HEAD,
                         .size = GUI_GIT_SMALL_SIZE, .x_f32 = after + (float)gap, .text = row.second,
                         .length = row.second_length, .color = palette->text_3);
        return;
    }
    if (row.type == CLI_GIT_LOG_ROW_NONE)
    {
        (void)EMBED_CALL(
            gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .face = GUI_FACE_HEAD, .size = GUI_GIT_NONE_SIZE,
            .x_f32 =
                (float)(rect.x + EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_NONE_PAD)),
            .text = row.text, .length = row.text_length, .color = palette->text_3);
        return;
    }
    if (row.type == CLI_GIT_LOG_ROW_FILE)
    {
        const int x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = args->raster, .palette = palette,
                                 .frame = frame, .rect = rect, .clip = args->clip, .depth = 1u);
        (void)EMBED_CALL(gui_changes_file.file_draw, GuiChangesFileCfg, .pane = args, .x = x, .path = row.path,
                         .name = row.text, .name_length = row.text_length, .folder_length = row.second_length,
                         .state = row.state, .color = text_color, .clip = args->clip);
        return;
    }
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_PAD);
    const int lane = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_LANE);
    int x = rect.x + gap;
    // the strokes, then the dot over them, hollow where the commit merges
    for (unsigned int at = 0u; at < row.strokes; at += 1u)
    {
        EMBED_CALL(git_stroke_draw, GitCtx, .pane = args, .x = x, .stroke = row.stroke[at],
                   .color = GUI_FRAME_FADED(palette->mark[s_tones[row.stroke[at][3] % 6u]], frame->side_share));
    }
    const float scale = (float)frame->scale / 100.0f;
    const float dot_x = (float)x + ((float)lane / 2.0f) + ((float)row.lane_at * (float)lane);
    const float dot_y = (float)rect.y + ((float)rect.height / 2.0f);
    const GuiColor tone = GUI_FRAME_FADED(palette->mark[s_tones[row.color % 6u]], frame->side_share);
    EMBED_CALL(gui_raster_color.dot, GuiRasterColorCfg, .raster = args->raster, .color = tone, .x_f32 = dot_x,
               .y_f32 = dot_y, .radius_f32 = (row.merge ? GUI_GIT_MERGE_DOT_F32 : GUI_GIT_DOT_F32) * scale,
               .clip = args->clip);
    if (row.merge)
    {
        EMBED_CALL(gui_raster_color.dot, GuiRasterColorCfg, .raster = args->raster,
                   .color = GUI_FRAME_FADED(palette->surface, frame->side_share), .x_f32 = dot_x, .y_f32 = dot_y,
                   .radius_f32 = (GUI_GIT_MERGE_DOT_F32 - GUI_GIT_STROKE_F32) * scale, .clip = args->clip);
    }
    // the row's parts are far fewer pixels than an int counts
    x += ((int)row.across * lane) + gap;
    // the day at the right, the author before it where authors show, and what stands before them cut before the author
    int limit =
        rect.x + rect.width - EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_RIGHT_PAD);
    const float day_width = EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = args, .face = GUI_FACE_HEAD,
                                       .size = GUI_GIT_SMALL_SIZE, .text = row.day, .length = CLI_GIT_LOG_DAY - 1ULL);
    limit -= (int)(day_width + 0.5f);
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .face = GUI_FACE_HEAD,
                     .size = GUI_GIT_SMALL_SIZE, .x_f32 = (float)limit, .text = row.day,
                     .length = CLI_GIT_LOG_DAY - 1ULL, .color = palette->text_3);
    limit -= gap;
    if (row.second_length != 0ULL)
    {
        const float measured = EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = args, .face = GUI_FACE_HEAD,
                                          .size = GUI_GIT_SMALL_SIZE, .text = row.second, .length = row.second_length);
        const int most = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_AUTHOR_MAX);
        const int author_width = (measured < (float)most) ? (int)(measured + 0.5f) : most;
        limit -= author_width;
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                         .size = GUI_GIT_SMALL_SIZE, .x_f32 = (float)limit, .text = row.second,
                         .length = row.second_length, .color = palette->text_2,
                         .clip = (GuiRect){limit, rect.y, author_width, rect.height});
        limit -= gap;
    }
    // the branches and tags at the commit, each in a pill between its least and most widths, HEAD's in signal and a
    // tag's in the third mark
    const int ref_pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_REF_PAD);
    const int ref_min = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_REF_MIN);
    const int ref_max = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_REF_MAX);
    const int ref_height = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_GIT_REF_HEIGHT);
    CliGitLogRef ref;
    for (unsigned int which = 0u;
         (x < limit) && EMBED_CALL(cli_git_log.ref, CliGitLogCfg, .at = args->row, .which = which, .ref = &ref);
         which += 1u)
    {
        const float measured = EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = args, .face = GUI_FACE_CODE,
                                          .size = GUI_GIT_REF_SIZE, .text = ref.name, .length = ref.length);
        int width = (int)(measured + 0.5f) + (2 * ref_pad);
        width = (width < ref_min) ? ref_min : ((width > ref_max) ? ref_max : width);
        const GuiRect pill = {x, rect.y + ((rect.height - ref_height) / 2), width, ref_height};
        EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = args, .rect = pill, .radius = GUI_GIT_REF_RADIUS,
                   .color = ref.head ? palette->signal : palette->line,
                   .inside = pointed ? palette->line : palette->editor_background);
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                         .size = GUI_GIT_REF_SIZE, .x_f32 = (float)(pill.x + ref_pad), .text = ref.name,
                         .length = ref.length,
                         .color = ref.head ? palette->signal : (ref.tag ? palette->mark[2] : palette->text_2),
                         .clip = (GuiRect){pill.x + ref_pad, pill.y, pill.width - (2 * ref_pad), pill.height});
        x += width + gap;
    }
    if (x < limit)
    {
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                         .size = GUI_PANE_ROW_SIZE, .x_f32 = (float)x, .text = row.text, .length = row.text_length,
                         .color = text_color, .clip = (GuiRect){x, rect.y, limit - x, rect.height});
    }
}

// a commit's row pressed opens or closes its files; a file's row opens the file
static int git_press(const GuiPaneCfg *args)
{
    CliGitLogRow row;
    if (!EMBED_CALL(cli_git_log.row, CliGitLogCfg, .at = args->row, .row = &row))
    {
        return 0;
    }
    if (row.type == CLI_GIT_LOG_ROW_COMMIT)
    {
        EMBED_CALL(cli_git_log.toggle, CliGitLogCfg, .at = args->row, .root = args->root);
        return 0;
    }
    if (row.type != CLI_GIT_LOG_ROW_FILE)
    {
        return 0;
    }
    *args->open_out = (GuiPaneOpen){row.path, {0ULL, 0ULL}, 0};
    return 1;
}

// the repositories and the graph read anew as the pane shows
static void git_read(const GuiPaneCfg *args)
{
    if (args->event == GUI_PANE_SHOWN)
    {
        EMBED_CALL(cli_git_log.relist, CliGitLogCfg, .root = args->root, .now_ms = cli_clock_milliseconds());
    }
}

// the menu of a commit's row or a file's
static int git_menu(const GuiPaneCfg *args)
{
    return EMBED_CALL(cli_git_log.menu, CliGitLogCfg, .at = args->row);
}

const GuiPaneTable gui_git_table = {
    .tool = "git",
    .title = git_title,
    .rows = git_rows,
    .row_height = git_row_height,
    .head = git_head,
    .head_draw = git_head_draw,
    .head_hit = git_head_hit,
    .action = git_action,
    .row_draw = git_row_draw,
    .press = git_press,
    .read = git_read,
    .text = git_text,
    .key = git_key,
    .menu = git_menu,
};
