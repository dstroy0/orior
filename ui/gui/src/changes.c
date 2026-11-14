// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes.c: the Changes pane's rows as the git module lists the changed files, each with its taken box; its head, the
// bar of tools, the all box with the taken count, the commit's message box, Commit and Commit and Push, with git's
// refusal under; and the changed file's row every pane draws the same
#include "changes.h"

#include "../../../includes/window/icons.h"
#include "../../cli/src/code/git.h"
#include "../../cli/src/command_line/cli_internal.h"
#include "../../cli/src/command_line/command_line.h"
#include "../../cli/src/editor/document.h"
#include "explorer.h"

#define CHANGES_PLACEHOLDER "Commit message"
#define CHANGES_COMMIT "Commit"
#define CHANGES_COMMIT_PUSH "Commit and Push"
#define CHANGES_NONE "No file differs from the last commit."
#define CHANGES_ARMED "A second press rolls the files taken back to the last commit."

typedef struct
{
    const GuiPaneCfg *pane;
    const CliChangedRow *row;
    unsigned int which;
    const char *text;
    int failed;
    int x;
} ChangesCtx;

static const char *changes_title(const GuiPaneCfg *args)
{
    (void)args;
    return "Changes";
}

static unsigned int changes_rows(const GuiPaneCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u);
}

// the bar's row, the taken row, the message box's row, the buttons' row and the refusal's row
static int changes_head(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_PANE_CHOICE + GUI_PANE_CHOICE + GUI_PANE_BOX + GUI_PANE_CHOICE + GUI_CHANGES_REPORT;
}

// the bar's row at the head's top
EMBED_INLINE GuiRect changes_bar_rect(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    return (GuiRect){pane->rect.x, pane->rect.y, pane->rect.width,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_CHOICE)};
}

// the taken row, under the bar
EMBED_INLINE GuiRect changes_taken_rect(const ChangesCtx *args)
{
    const GuiRect bar = EMBED_CALL(changes_bar_rect, ChangesCtx, .pane = args->pane);
    return (GuiRect){bar.x, bar.y + bar.height, bar.width, bar.height};
}

// the message box's rectangle, in the row under the taken row
EMBED_INLINE GuiRect changes_field_rect(const ChangesCtx *args)
{
    const GuiRect taken = EMBED_CALL(changes_taken_rect, ChangesCtx, .pane = args->pane);
    return EMBED_CALL(gui_pane_field.field_rect, GuiPaneFieldCfg, .pane = args->pane, .y = taken.y + taken.height);
}

// the buttons' row, under the message box: Commit in its left half, Commit and Push in its right
EMBED_INLINE GuiRect changes_buttons_rect(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiRect taken = EMBED_CALL(changes_taken_rect, ChangesCtx, .pane = pane);
    const int box = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_BOX);
    return (GuiRect){pane->rect.x, taken.y + taken.height + box, pane->rect.width,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_CHOICE)};
}

EMBED_INLINE GuiRect changes_commit_rect(const ChangesCtx *args)
{
    const GuiRect buttons = EMBED_CALL(changes_buttons_rect, ChangesCtx, .pane = args->pane);
    return (GuiRect){buttons.x, buttons.y, buttons.width / 2, buttons.height};
}

EMBED_INLINE GuiRect changes_commit_push_rect(const ChangesCtx *args)
{
    const GuiRect buttons = EMBED_CALL(changes_buttons_rect, ChangesCtx, .pane = args->pane);
    return (GuiRect){buttons.x + (buttons.width / 2), buttons.y, buttons.width - (buttons.width / 2), buttons.height};
}

// the refusal's row, under the buttons
EMBED_INLINE GuiRect changes_report_rect(const ChangesCtx *args)
{
    const GuiRect buttons = EMBED_CALL(changes_buttons_rect, ChangesCtx, .pane = args->pane);
    return (GuiRect){buttons.x, buttons.y + buttons.height, buttons.width,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane, .x = GUI_CHANGES_REPORT)};
}

// the square of the bar's tool `which`: the tree read again and the roll back from the left, the pull and the push
// from the right, each of the latter with the width of its count after it
EMBED_INLINE GuiRect changes_tool_rect(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiRect bar = EMBED_CALL(changes_bar_rect, ChangesCtx, .pane = pane);
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT);
    const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_TOOL);
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_TOOL_GAP);
    const int count = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_COUNT);
    const int y = bar.y + ((bar.height - side) / 2);
    const int right = bar.x + bar.width - pad;
    switch (args->which)
    {
    case CLI_CHANGES_READ:
        return (GuiRect){bar.x + pad, y, side, side};
    case CLI_CHANGES_ROLLBACK:
        return (GuiRect){bar.x + pad + side + gap, y, side, side};
    case CLI_CHANGES_PUSH:
        return (GuiRect){right - count - side, y, side, side};
    default:
        return (GuiRect){right - count - side - gap - count - side, y, side, side};
    }
}

// the bar's tool `which` drawn: its ground where the pointer is on it, and its icon `text`, quieter where it
// `failed`, which is where it can do nothing
EMBED_INLINE void changes_tool_draw(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiPalette *const palette = pane->palette;
    const GuiRect tool = EMBED_CALL(changes_tool_rect, ChangesCtx, .pane = pane, .which = args->which);
    // each tool a button of glass, tinted with the hover's color under the pointer
    EMBED_CALL(gui_pane_field.button_draw, GuiPaneFieldCfg, .pane = pane, .rect = tool, .radius = GUI_PANE_RADIUS,
               .color = (pane->pane_read->action == args->which) ? palette->hover : palette->line,
               .inside = (pane->pane_read->action == args->which) ? palette->hover : 0u);
    const unsigned int icon =
        (unsigned int)EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_TOOL_ICON);
    const GuiMask *const mask_bits =
        (pane->icons != NULL) ? EMBED_CALL(gui_icons.icon, GuiIconsCfg, .icons = pane->icons, .name = args->text,
                                           .size = icon, .stroke = GUI_ICON_STROKE * (float)icon / GUI_ICON_UNITS)
                              : NULL;
    if (mask_bits != NULL)
    {
        // the icon's side is far inside what an int represents
        EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = pane->raster, .mask_bits = mask_bits,
                   .x = tool.x + ((tool.width - (int)icon) / 2), .y = tool.y + ((tool.height - (int)icon) / 2),
                   .color = GUI_FRAME_FADED(args->failed ? palette->text_3 : palette->text_2, pane->frame->side_share),
                   .clip = pane->clip);
    }
}

// the count `pane->rows` drawn after the tool `which`, where it is not 0
EMBED_INLINE void changes_count_draw(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    if (pane->rows <= 0)
    {
        return;
    }
    const GuiRect tool = EMBED_CALL(changes_tool_rect, ChangesCtx, .pane = pane, .which = args->which);
    const int count = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_COUNT);
    char digits[12];
    VerbaLinea line = {digits, sizeof(digits), 0ULL};
    // the count is not negative, as checked above
    mmgr_line_decimal(&line, (unsigned long long)pane->rows, 1u);
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = pane, .clipped = 1, .face = GUI_FACE_HEAD,
                     .size = GUI_CHANGES_COUNT_SIZE, .x_f32 = (float)(tool.x + tool.width + 1), .text = digits,
                     .length = line.at, .color = pane->palette->text_2,
                     .clip = (GuiRect){tool.x + tool.width, tool.y, count, tool.height});
}

// the head drawn: the bar, its tools quieter where they can do nothing, the pull and the push with how far the branch
// is from its remote; the all box lit while every file is taken, its inside the line's while some are, beside how
// many are taken of how many; the message box with the message typed; Commit and Commit and Push; and git's refusal
// under them where it is
static void changes_head_draw(const GuiPaneCfg *args)
{
    const GuiPalette *const palette = args->palette;
    const unsigned int count = EMBED_CALL(cli_changes.files, CliGitChangesCfg, .at = 0u);
    const unsigned int taken = EMBED_CALL(cli_changes.taken, CliGitChangesCfg, .at = 0u);
    unsigned int ahead = 0u;
    unsigned int behind = 0u;
    const int followed = EMBED_CALL(cli_changes.sync, CliGitChangesCfg, .ahead = &ahead, .behind = &behind);
    EMBED_CALL(changes_tool_draw, ChangesCtx, .pane = args, .which = CLI_CHANGES_READ, .text = "refresh");
    EMBED_CALL(changes_tool_draw, ChangesCtx, .pane = args, .which = CLI_CHANGES_ROLLBACK, .text = "undo",
               .failed = taken == 0u);
    EMBED_CALL(changes_tool_draw, ChangesCtx, .pane = args, .which = CLI_CHANGES_PULL, .text = "pull",
               .failed = !followed);
    EMBED_CALL(changes_tool_draw, ChangesCtx, .pane = args, .which = CLI_CHANGES_PUSH, .text = "push",
               .failed = !followed);
    // the counts are below an int's range, as git counts commits
    GuiPaneCfg counted = *args;
    counted.rows = (int)behind;
    EMBED_CALL(changes_count_draw, ChangesCtx, .pane = &counted, .which = CLI_CHANGES_PULL);
    counted.rows = (int)ahead;
    EMBED_CALL(changes_count_draw, ChangesCtx, .pane = &counted, .which = CLI_CHANGES_PUSH);

    const GuiRect taken_row = EMBED_CALL(changes_taken_rect, ChangesCtx, .pane = args);
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_CHANGES_BOX);
    const int all = (count != 0u) && (taken == count);
    EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = args,
               .rect = (GuiRect){taken_row.x + pad, taken_row.y + ((taken_row.height - side) / 2), side, side},
               .radius = GUI_CHANGES_BOX_RADIUS, .color = all ? palette->signal : palette->line,
               .inside = all ? palette->signal : ((taken != 0u) ? palette->line : palette->editor_background));
    char words[64];
    VerbaLinea line = {words, sizeof(words), 0ULL};
    if (count == 0u)
    {
        mmgr_line_put(&line, CHANGES_NONE);
    }
    else
    {
        mmgr_line_decimal(&line, taken, 1u);
        mmgr_line_put(&line, " of ");
        mmgr_line_decimal(&line, count, 1u);
        mmgr_line_put(&line, (count == 1u) ? " file taken" : " files taken");
    }
    (void)EMBED_CALL(
        gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
        .size = GUI_CHANGES_TAKEN_SIZE,
        .x_f32 = (float)(taken_row.x + pad + side +
                         EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_CHANGES_BOX_GAP)),
        .text = words, .length = line.at, .color = palette->text_2, .clip = taken_row);

    const GuiRect field = EMBED_CALL(changes_field_rect, ChangesCtx, .pane = args);
    char message[CLI_CHANGES_MESSAGE_MAX];
    const unsigned long long length = EMBED_CALL(cli_changes.message, CliGitChangesCfg, .text_out = message);
    EMBED_CALL(gui_pane_field.field_draw, GuiPaneFieldCfg, .pane = args, .rect = field, .right = field.x + field.width,
               .text = message, .length = length, .placeholder = CHANGES_PLACEHOLDER, .keys = args->pane_read->keys);
    EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args,
               .rect = EMBED_CALL(changes_commit_rect, ChangesCtx, .pane = args), .text = CHANGES_COMMIT,
               .length = sizeof(CHANGES_COMMIT) - 1ULL, .pointed = args->pane_read->action == CLI_CHANGES_COMMIT);
    EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args,
               .rect = EMBED_CALL(changes_commit_push_rect, ChangesCtx, .pane = args), .text = CHANGES_COMMIT_PUSH,
               .length = sizeof(CHANGES_COMMIT_PUSH) - 1ULL,
               .pointed = args->pane_read->action == CLI_CHANGES_COMMIT_PUSH);
    char refusal[CLI_CHANGES_REFUSAL_MAX];
    const unsigned long long refused = EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .text_out = refusal);
    if (refused != 0ULL)
    {
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                         .size = GUI_CHANGES_REPORT_SIZE, .x_f32 = (float)(args->rect.x + pad), .text = refusal,
                         .length = refused, .color = palette->text_3,
                         .clip = EMBED_CALL(changes_report_rect, ChangesCtx, .pane = args));
    }
}

// whether the pixel at x, y is in the bar's tool `which`
EMBED_INLINE int changes_tool_holds(const ChangesCtx *args)
{
    return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                      .rect = EMBED_CALL(changes_tool_rect, ChangesCtx, .pane = args->pane, .which = args->which),
                      .x = args->pane->x, .y = args->pane->y);
}

// the action at x, y: the message box where the pixel is in it, Commit or Commit and Push where it is in
// their halves, the bar's tool it is on, or the all box's row where files are listed
static unsigned int changes_head_hit(const GuiPaneCfg *args)
{
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(changes_field_rect, ChangesCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return CLI_CHANGES_FIELD;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(changes_commit_rect, ChangesCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return CLI_CHANGES_COMMIT;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(changes_commit_push_rect, ChangesCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return CLI_CHANGES_COMMIT_PUSH;
    }
    static const unsigned int s_tools[4] = {CLI_CHANGES_READ, CLI_CHANGES_ROLLBACK, CLI_CHANGES_PULL, CLI_CHANGES_PUSH};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (EMBED_CALL(changes_tool_holds, ChangesCtx, .pane = args, .which = s_tools[at]))
        {
            return s_tools[at];
        }
    }
    if ((EMBED_CALL(cli_changes.files, CliGitChangesCfg, .at = 0u) != 0u) &&
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(changes_taken_rect, ChangesCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return CLI_CHANGES_ALL;
    }
    return 0u;
}

// `text` posted as a notice, a failure's where `failed`; where `text` is NULL, the refusal's first line as a failure's
EMBED_INLINE void changes_noticed(const ChangesCtx *args)
{
    if (args->text != NULL)
    {
        cli_notice_post(args->text, args->failed);
        return;
    }
    char refusal[CLI_CHANGES_REFUSAL_MAX];
    const unsigned long long length = EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .text_out = refusal);
    unsigned long long first = 0ULL;
    while ((first < length) && (refusal[first] != '\n'))
    {
        first += 1ULL;
    }
    char line_text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea line = {line_text, sizeof(line_text), 0ULL};
    mmgr_line_put_n(&line, refusal, first);
    (void)mmgr_line_finish(&line);
    cli_notice_post(line_text, 1);
}

// the files taken committed, pushed as well where `which` is Commit and Push, every view saved first
EMBED_INLINE void changes_committed(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    if (pane->save_all != NULL)
    {
        pane->save_all(pane->context);
    }
    const int push = args->which == CLI_CHANGES_COMMIT_PUSH;
    const int committed = EMBED_CALL(cli_changes.commit, CliGitChangesCfg, .root = pane->root, .push = push);
    EMBED_CALL(changes_noticed, ChangesCtx, .text = committed ? (push ? "Committed and pushed." : "Committed.") : NULL,
               .failed = !committed);
}

// the files taken rolled back on the second press: each open view on one read again from the disk, the changes read
// again, and the refusal posted where a file was refused
EMBED_INLINE void changes_rolled(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const unsigned int taken = EMBED_CALL(cli_changes.taken, CliGitChangesCfg, .at = 0u);
    if (!EMBED_CALL(cli_changes.rollback, CliGitChangesCfg, .root = pane->root, .now_ms = cli_clock_milliseconds()))
    {
        if (taken != 0u)
        {
            cli_notice_post(CHANGES_ARMED, 0);
        }
        return;
    }
    const unsigned int count = EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u);
    for (unsigned int at = 0u; (pane->file_reread != NULL) && (at < count); at += 1u)
    {
        CliChangedRow row;
        if (EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = at, .row = &row) &&
            (row.type == CLI_CHANGES_ROW_FILE) && row.taken)
        {
            pane->file_reread(pane->context, row.path);
        }
    }
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = pane->root);
    const unsigned long long refused = EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .text_out = NULL);
    char words[64];
    VerbaLinea line = {words, sizeof(words), 0ULL};
    mmgr_line_put(&line, "Rolled ");
    mmgr_line_decimal(&line, taken, 1u);
    mmgr_line_put(&line, (taken == 1u) ? " file back to the last commit." : " files back to the last commit.");
    (void)mmgr_line_finish(&line);
    EMBED_CALL(changes_noticed, ChangesCtx, .text = (refused == 0ULL) ? words : NULL, .failed = refused != 0ULL);
}

// an action taken: the message box takes the keys; Commit and Commit and Push commit the files taken; the tree is
// read again; the roll back arms, then rolls; the branch is pulled or pushed; and every file is taken where some is
// left out, else every file is left out
static void changes_action(const GuiPaneCfg *args)
{
    switch (args->which)
    {
    case CLI_CHANGES_FIELD:
        if (args->pane != NULL)
        {
            args->pane->keys = 1;
        }
        return;
    case CLI_CHANGES_COMMIT:
    case CLI_CHANGES_COMMIT_PUSH:
        EMBED_CALL(changes_committed, ChangesCtx, .pane = args, .which = args->which);
        return;
    case CLI_CHANGES_READ:
        EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
        return;
    case CLI_CHANGES_ROLLBACK:
        EMBED_CALL(changes_rolled, ChangesCtx, .pane = args);
        return;
    case CLI_CHANGES_PULL: {
        const int pulled = EMBED_CALL(cli_changes.pull, CliGitChangesCfg, .root = args->root);
        EMBED_CALL(changes_noticed, ChangesCtx, .text = pulled ? "Pulled." : NULL, .failed = !pulled);
        return;
    }
    case CLI_CHANGES_PUSH: {
        const int pushed = EMBED_CALL(cli_changes.push, CliGitChangesCfg, .root = args->root);
        EMBED_CALL(changes_noticed, ChangesCtx, .text = pushed ? "Pushed." : NULL, .failed = !pushed);
        return;
    }
    case CLI_CHANGES_ALL:
        EMBED_CALL(cli_changes.take_all, CliGitChangesCfg,
                   .on = EMBED_CALL(cli_changes.taken, CliGitChangesCfg, .at = 0u) <
                         EMBED_CALL(cli_changes.files, CliGitChangesCfg, .at = 0u));
        return;
    default:
        return;
    }
}

// text typed goes into the message
static int changes_text(const GuiPaneCfg *args)
{
    EMBED_CALL(cli_changes.type, CliGitChangesCfg, .text = args->text, .size = args->length);
    return 1;
}

// Escape lets the keys go; Ctrl+Enter commits; Backspace goes to the message
static int changes_key(const GuiPaneCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        args->pane->keys = 0;
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Ctrl+Enter", .cap = 16ULL}))
    {
        EMBED_CALL(changes_committed, ChangesCtx, .pane = args, .which = CLI_CHANGES_COMMIT);
        return 1;
    }
    return EMBED_CALL(cli_changes.key, CliGitChangesCfg, .name = args->name);
}

GuiColor gui_changes_state_color(const GuiChangesFileCfg *args)
{
    const GuiPalette *const palette = args->pane->palette;
    switch (args->state)
    {
    case 'M':
        return palette->mark[2];
    case 'A':
    case 'U':
        return palette->mark[1];
    case 'D':
        return palette->mark[0];
    default:
        return palette->text_2;
    }
}

float gui_changes_file_draw(const GuiChangesFileCfg *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiRect rect = pane->rect;
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT);
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_GAP);
    const char state_text[2] = {args->state, '\0'};
    const float state_width =
        EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = pane, .face = GUI_FACE_CODE_BOLD,
                   .size = GUI_CHANGES_STATE_SIZE, .text = state_text, .length = 1ULL);
    const float state_x = (float)(rect.x + rect.width - pad) - state_width;
    (void)EMBED_CALL(
        gui_pane_part.text_draw, GuiPanePartCfg, .pane = pane, .clipped = 1, .face = GUI_FACE_CODE_BOLD,
        .size = GUI_CHANGES_STATE_SIZE, .x_f32 = state_x, .text = state_text, .length = 1ULL,
        .color = EMBED_CALL(gui_changes_file.state_color, GuiChangesFileCfg, .pane = pane, .state = args->state),
        .clip = args->clip);
    const GuiRect text_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){rect.x, rect.y, (int)state_x - gap - rect.x, rect.height}, .other = args->clip);
    return EMBED_CALL(gui_explorer_file.file_draw, GuiExplorerFileCfg, .pane = pane, .x = args->x, .name = args->name,
                      .length = args->name_length, .folder = args->path, .folder_length = args->folder_length,
                      .color = args->color, .clip = text_clip);
}

// the x where a row's file starts, after its taken box: the padding, the box's side and the gap after it
EMBED_INLINE int changes_file_x(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    return EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT) +
           EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_BOX) +
           EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_BOX_GAP);
}

// a row's taken box drawn from `x`: lit in signal while the next commit takes all of the row, its inside the line's
// while it takes some lines of it, else clear
EMBED_INLINE void changes_box_draw(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiPalette *const palette = pane->palette;
    const CliChangedRow *const read = args->row;
    const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_BOX);
    const int whole = read->taken && !read->part;
    EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = pane,
               .rect = (GuiRect){args->x, pane->rect.y + ((pane->rect.height - side) / 2), side, side},
               .radius = GUI_CHANGES_BOX_RADIUS, .color = whole ? palette->signal : palette->line,
               .inside = whole ? palette->signal : (read->part ? palette->line : palette->editor_background));
}

// a changed file's row: its taken box, then where its changes can open the arrow, down while they are, then its name
// in the state's color
EMBED_INLINE void changes_file_row_draw(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const CliChangedRow *const read = args->row;
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT);
    EMBED_CALL(changes_box_draw, ChangesCtx, .pane = pane, .row = read, .x = pane->rect.x + pad);
    int x = pane->rect.x + EMBED_CALL(changes_file_x, ChangesCtx, .pane = pane);
    if (read->opens)
    {
        const int twisty = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_EXPLORER_TWISTY);
        const int arrow = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_ARROW);
        EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = pane->raster, .x = x + ((twisty - arrow) / 2),
                   .y = pane->rect.y + ((pane->rect.height - arrow) / 2), .side = arrow, .open = read->open,
                   .color = GUI_FRAME_FADED(pane->palette->text_2, pane->frame->side_share), .clip = pane->clip);
        x += twisty;
    }
    (void)EMBED_CALL(
        gui_changes_file.file_draw, GuiChangesFileCfg, .pane = pane, .x = x, .path = read->path, .name = read->name,
        .name_length = cellul.len(&(CatenaFinitaCfg){.src = read->name, .cap = CLI_PATH_MAX_LEN}),
        .folder_length = read->folder_length, .state = read->state,
        .color = EMBED_CALL(gui_changes_file.state_color, GuiChangesFileCfg, .pane = pane, .state = read->state),
        .clip = pane->clip);
}

// a change's row, one guide in: its taken box, then the lines of the newer text it covers, counted from 1
EMBED_INLINE void changes_hunk_row_draw(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const CliChangedRow *const read = args->row;
    const int x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = pane->raster, .palette = pane->palette,
                             .frame = pane->frame, .rect = pane->rect, .clip = pane->clip, .depth = 1u);
    EMBED_CALL(changes_box_draw, ChangesCtx, .pane = pane, .row = read, .x = x);
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_BOX_GAP);
    const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_BOX);
    char words[48];
    VerbaLinea line = {words, sizeof(words), 0ULL};
    mmgr_line_put(&line, (read->to > read->from + 1u) ? "Lines " : "Line ");
    mmgr_line_decimal(&line, read->from + 1u, 1u);
    if (read->to > read->from + 1u)
    {
        mmgr_line_put(&line, " to ");
        mmgr_line_decimal(&line, read->to, 1u);
    }
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = pane, .clipped = 1, .face = GUI_FACE_CODE,
                     .size = GUI_CHANGES_LINE_SIZE, .x_f32 = (float)(x + side + gap), .text = words, .length = line.at,
                     .color = pane->palette->text_2, .clip = pane->clip);
}

// a line's row, two guides in: its taken box, its sign, a minus for a line taken out in the first mark's color and a
// plus for one put in in the second's, then the line's text in the code face
EMBED_INLINE void changes_line_row_draw(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiPalette *const palette = pane->palette;
    const CliChangedRow *const read = args->row;
    const int x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = pane->raster, .palette = palette,
                             .frame = pane->frame, .rect = pane->rect, .clip = pane->clip, .depth = 2u);
    EMBED_CALL(changes_box_draw, ChangesCtx, .pane = pane, .row = read, .x = x);
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_BOX_GAP);
    const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_BOX);
    const float after_sign =
        EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = pane, .clipped = 1, .face = GUI_FACE_CODE_BOLD,
                   .size = GUI_CHANGES_LINE_SIZE, .x_f32 = (float)(x + side + gap), .text = read->gone ? "-" : "+",
                   .length = 1ULL, .color = read->gone ? palette->mark[0] : palette->mark[1], .clip = pane->clip);
    // the line's units as UTF-8, as many as the row can show
    char text[GUI_CHANGES_LINE_BYTES];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .units = read->units,
               .count = (read->count < GUI_CHANGES_LINE_UNITS) ? read->count : GUI_CHANGES_LINE_UNITS, .out = &line);
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = pane, .clipped = 1, .face = GUI_FACE_CODE,
                     .size = GUI_CHANGES_LINE_SIZE, .x_f32 = after_sign + (float)gap, .text = text, .length = line.at,
                     .color = palette->text_2, .clip = pane->clip);
}

// the row drawn as its type: a file, a change under one, or a line of a change
static void changes_row_draw(const GuiPaneCfg *args)
{
    CliChangedRow read;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->row, .row = &read))
    {
        return;
    }
    switch (read.type)
    {
    case CLI_CHANGES_ROW_FILE:
        EMBED_CALL(changes_file_row_draw, ChangesCtx, .pane = args, .row = &read);
        return;
    case CLI_CHANGES_ROW_HUNK:
        EMBED_CALL(changes_hunk_row_draw, ChangesCtx, .pane = args, .row = &read);
        return;
    default:
        EMBED_CALL(changes_line_row_draw, ChangesCtx, .pane = args, .row = &read);
        return;
    }
}

// the x where a row's box ends, past the guides a change's or a line's row is in
EMBED_INLINE int changes_box_end(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const unsigned int depth = (args->row->type == CLI_CHANGES_ROW_FILE)   ? 0u
                               : (args->row->type == CLI_CHANGES_ROW_HUNK) ? 1u
                                                                           : 2u;
    // the depth is at max two guides, far inside an int
    return EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT) +
           ((int)depth * EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_GUIDE)) +
           EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_BOX) +
           EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_CHANGES_BOX_GAP);
}

// the row pressed, where the frame is known: on its taken box, the row is left out or taken again; on a file's arrow,
// its changes open or close; elsewhere its file opens, at the change's first line from a change's or a line's row
static int changes_press(const GuiPaneCfg *args)
{
    CliChangedRow changed;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->row, .row = &changed))
    {
        return 0;
    }
    if (args->frame != NULL)
    {
        const GuiRect rect = EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = args->table, .frame = args->frame,
                                        .pane_read = args->pane_read, .row = args->row);
        const int box_end = rect.x + EMBED_CALL(changes_box_end, ChangesCtx, .pane = args, .row = &changed);
        if (args->x < box_end)
        {
            EMBED_CALL(cli_changes.toggle, CliGitChangesCfg, .at = args->row);
            return 0;
        }
        if ((changed.type == CLI_CHANGES_ROW_FILE) && changed.opens &&
            (args->x <
             box_end + EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_EXPLORER_TWISTY)))
        {
            (void)EMBED_CALL(cli_changes.open, CliGitChangesCfg, .root = args->root, .at = args->row);
            return 0;
        }
    }
    const int placed = changed.type != CLI_CHANGES_ROW_FILE;
    // the path the press opens, retained past the row for the window to open
    static char s_pressed_path[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = s_pressed_path, .src = changed.path, .bytes = sizeof(s_pressed_path)});
    *args->open_out = (GuiPaneOpen){s_pressed_path, {placed ? changed.from : 0ULL, 0ULL}, placed};
    return 1;
}

// the changes read anew of the tree as the pane shows and as a file is saved
static void changes_read(const GuiPaneCfg *args)
{
    if ((args->event == GUI_PANE_DISPLAYED) || (args->event == GUI_PANE_SAVED))
    {
        EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    }
}

// the menu of a changed file's row
static int changes_menu(const GuiPaneCfg *args)
{
    return EMBED_CALL(cli_changes.menu, CliGitChangesCfg, .at = args->row);
}

const GuiPaneTable gui_changes_table = {
    .tool = "commit",
    .title = changes_title,
    .rows = changes_rows,
    .head = changes_head,
    .head_draw = changes_head_draw,
    .head_hit = changes_head_hit,
    .action = changes_action,
    .row_draw = changes_row_draw,
    .press = changes_press,
    .text = changes_text,
    .key = changes_key,
    .read = changes_read,
    .menu = changes_menu,
};
