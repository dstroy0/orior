// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes.c: the Changes pane's rows as the git module lists the changed files, and the changed file's row every pane
// draws the same
#include "changes.h"

#include "../../code/git.h"
#include "../../command_line/cli_internal.h"
#include "../../text/text.h"
#include "explorer.h"

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

// the changed file's row, its name in the state's color
static void changes_row_draw(const GuiPaneCfg *args)
{
    CliChangedRow read;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->row, .row = &read))
    {
        return;
    }
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    (void)EMBED_CALL(
        gui_changes_file.file_draw, GuiChangesFileCfg, .pane = args, .x = args->rect.x + pad, .path = read.path,
        .name = read.name, .name_length = cellul.len(&(CatenaFinitaCfg){.src = read.name, .cap = CLI_PATH_MAX_LEN}),
        .folder_length = read.folder_length, .state = read.state,
        .color = EMBED_CALL(gui_changes_file.state_color, GuiChangesFileCfg, .pane = args, .state = read.state),
        .clip = args->clip);
}

// the row's file opened
static int changes_press(const GuiPaneCfg *args)
{
    CliChangedRow changed;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->row, .row = &changed))
    {
        return 0;
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

const GuiPaneKind gui_changes_kind = {
    .tool = "commit",
    .title = changes_title,
    .rows = changes_rows,
    .row_draw = changes_row_draw,
    .press = changes_press,
    .read = changes_read,
    .menu = changes_menu,
};
