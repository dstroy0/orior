// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes.c: the Changes pane's rows as the git module lists the changed files, each with its taken box, the commit's
// message box and Commit over them with git's refusal under, and the changed file's row every pane draws the same
#include "changes.h"

#include "../../code/git.h"
#include "../../command_line/cli_internal.h"
#include "explorer.h"

#define CHANGES_PLACEHOLDER "Commit message"
#define CHANGES_COMMIT "Commit"

typedef struct
{
    const GuiPaneCfg *pane;
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

// the message box's row, Commit's row and the refusal's row
static int changes_head(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_PANE_BOX + GUI_PANE_CHOICE + GUI_CHANGES_REPORT;
}

// the message box's rectangle, in the head's first row
EMBED_INLINE GuiRect changes_field_rect(const ChangesCtx *args)
{
    return EMBED_CALL(gui_pane_field.field_rect, GuiPaneFieldCfg, .pane = args->pane, .y = args->pane->rect.y);
}

// Commit's row, under the message box
EMBED_INLINE GuiRect changes_commit_rect(const ChangesCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const int box = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_BOX);
    return (GuiRect){pane->rect.x, pane->rect.y + box, pane->rect.width,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_CHOICE)};
}

// the refusal's row, under Commit
EMBED_INLINE GuiRect changes_report_rect(const ChangesCtx *args)
{
    const GuiRect commit = EMBED_CALL(changes_commit_rect, ChangesCtx, .pane = args->pane);
    return (GuiRect){args->pane->rect.x, commit.y + commit.height, args->pane->rect.width,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane, .x = GUI_CHANGES_REPORT)};
}

// the head drawn: the message box with the message typed, Commit under it, and git's refusal under that where it stands
static void changes_head_draw(const GuiPaneCfg *args)
{
    const GuiPalette *const palette = args->palette;
    const GuiRect field = EMBED_CALL(changes_field_rect, ChangesCtx, .pane = args);
    unsigned long long length = 0ULL;
    const char *const message = EMBED_CALL(cli_changes.message, CliGitChangesCfg, .length_out = &length);
    EMBED_CALL(gui_pane_field.field_draw, GuiPaneFieldCfg, .pane = args, .rect = field, .right = field.x + field.width,
               .text = message, .length = length, .placeholder = CHANGES_PLACEHOLDER, .keys = args->pane_read->keys);
    EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args,
               .rect = EMBED_CALL(changes_commit_rect, ChangesCtx, .pane = args), .text = CHANGES_COMMIT,
               .length = sizeof(CHANGES_COMMIT) - 1ULL, .pointed = args->pane_read->action == CLI_CHANGES_COMMIT);
    unsigned long long refused = 0ULL;
    const char *const refusal = EMBED_CALL(cli_changes.refusal, CliGitChangesCfg, .length_out = &refused);
    if (refused != 0ULL)
    {
        const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                         .size = GUI_CHANGES_REPORT_SIZE, .x_f32 = (float)(args->rect.x + pad), .text = refusal,
                         .length = refused, .color = palette->text_3,
                         .clip = EMBED_CALL(changes_report_rect, ChangesCtx, .pane = args));
    }
}

// the action at x, y: the message box where the pixel stands in it, else Commit where it stands in its row
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
    return 0u;
}

// an action taken: the message box takes the keys; Commit saves every view and commits the files taken
static void changes_action(const GuiPaneCfg *args)
{
    if (args->which == CLI_CHANGES_FIELD)
    {
        if (args->pane != NULL)
        {
            args->pane->keys = 1;
        }
        return;
    }
    if (args->save_all != NULL)
    {
        args->save_all(args->context);
    }
    (void)EMBED_CALL(cli_changes.commit, CliGitChangesCfg, .root = args->root);
}

// text typed goes into the message
static int changes_text(const GuiPaneCfg *args)
{
    EMBED_CALL(cli_changes.type, CliGitChangesCfg, .text = args->text, .size = args->length);
    return 1;
}

// Escape lets the keys go; Backspace goes to the message
static int changes_key(const GuiPaneCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        args->pane->keys = 0;
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

// the changed file's row: its taken box, lit in signal while the next commit takes it, then its name in the state's
// color
static void changes_row_draw(const GuiPaneCfg *args)
{
    CliChangedRow read;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->row, .row = &read))
    {
        return;
    }
    const GuiPalette *const palette = args->palette;
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    const int side = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_CHANGES_BOX);
    EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = args,
               .rect = (GuiRect){args->rect.x + pad, args->rect.y + ((args->rect.height - side) / 2), side, side},
               .radius = GUI_CHANGES_BOX_RADIUS, .color = read.taken ? palette->signal : palette->line,
               .inside = read.taken ? palette->signal : palette->editor_background);
    (void)EMBED_CALL(
        gui_changes_file.file_draw, GuiChangesFileCfg, .pane = args,
        .x = args->rect.x + EMBED_CALL(changes_file_x, ChangesCtx, .pane = args), .path = read.path, .name = read.name,
        .name_length = cellul.len(&(CatenaFinitaCfg){.src = read.name, .cap = CLI_PATH_MAX_LEN}),
        .folder_length = read.folder_length, .state = read.state,
        .color = EMBED_CALL(gui_changes_file.state_color, GuiChangesFileCfg, .pane = args, .state = read.state),
        .clip = args->clip);
}

// the row pressed: on its taken box, where the frame is known, the file is left out or taken again; elsewhere its file
// opens
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
        if (args->x < rect.x + EMBED_CALL(changes_file_x, ChangesCtx, .pane = args))
        {
            EMBED_CALL(cli_changes.toggle, CliGitChangesCfg, .at = args->row);
            return 0;
        }
    }
    *args->open_out = (GuiPaneOpen){changed.path, {0ULL, 0ULL}, 0};
    return 1;
}

// the changes read anew of the tree as the pane shows and as a file is saved
static void changes_read(const GuiPaneCfg *args)
{
    if ((args->event == GUI_PANE_SHOWN) || (args->event == GUI_PANE_SAVED))
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
