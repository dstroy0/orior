// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes.c: the Changes pane's rows as the git module lists the changed files
#include "changes.h"

#include "../../code/git.h"
#include "../../command_line/cli_internal.h"
#include "../../text/text.h"
#include "explorer.h"

typedef struct
{
    const GuiPalette *palette;
    char state;
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

// the color of a state: a changed file in the third mark's, one added or not yet tracked in the second's, one gone in
// the first's, and any other in the text's quieter color
EMBED_INLINE GuiColor changes_color(const ChangesCtx *args)
{
    switch (args->state)
    {
    case 'M':
        return args->palette->mark[2];
    case 'A':
    case 'U':
        return args->palette->mark[1];
    case 'D':
        return args->palette->mark[0];
    default:
        return args->palette->text_2;
    }
}

// the file's mark centered in its box, the state letter at the right bold in the code face, and between them the name
// in the state's color with the folder quieter after it, both cut before the letter
static void changes_row_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiFrame *const frame = args->frame;
    const GuiRect rect = args->rect;
    const GuiRect on = args->clip;
    const int scale = frame->scale;
    CliChangedRow read;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->row, .row = &read))
    {
        return;
    }
    const unsigned int row_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                           .pixels = (int)GUI_PANE_ROW_SIZE, .scale = scale);
    const unsigned int where_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = (int)GUI_CHANGES_WHERE_SIZE, .scale = scale);
    const unsigned int state_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = (int)GUI_CHANGES_STATE_SIZE, .scale = scale);
    const unsigned int mark_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_EXPLORER_MARK_SIZE, .scale = scale);
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANE_PAD_LEFT, .scale = scale);
    const int icon = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CHANGES_ICON, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CHANGES_GAP, .scale = scale);
    const GuiColor state_color = GUI_FRAME_FADED(
        EMBED_CALL(changes_color, ChangesCtx, .palette = args->palette, .state = read.state), frame->side_share);
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = read.name, .cap = CLI_PATH_MAX_LEN});
    char mark_text[GUI_EXPLORER_MARK_MOST];
    const unsigned int mark_color = EMBED_CALL(gui_explorer_mark_table.mark, GuiExplorerMarkCfg, .name = read.name,
                                               .length = name_length, .text = mark_text);
    const unsigned long long mark_length =
        cellul.len(&(CatenaFinitaCfg){.src = mark_text, .cap = GUI_EXPLORER_MARK_MOST});
    const float mark_width =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE_BOLD,
                   .size = mark_size, .text = mark_text, .length = mark_length);
    const int x = rect.x + pad;
    (void)EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
        .face = GUI_FACE_CODE_BOLD, .size = mark_size, .x_f32 = (float)x + (((float)icon - mark_width) / 2.0f),
        .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_CODE_BOLD,
                        .size = mark_size, .top = rect.y, .pixel_height = rect.height),
        .text = mark_text, .length = mark_length,
        .color = GUI_FRAME_FADED((mark_color != 0u) ? args->palette->mark[mark_color - 1u] : args->palette->text_3,
                                 frame->side_share),
        .clip = on);
    const char state_text[2] = {read.state, '\0'};
    const float state_width =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE_BOLD,
                   .size = state_size, .text = state_text, .length = 1ULL);
    const float state_x = (float)(rect.x + rect.width - pad) - state_width;
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_CODE_BOLD, .size = state_size, .x_f32 = state_x,
                     .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_CODE_BOLD, .size = state_size, .top = rect.y,
                                     .pixel_height = rect.height),
                     .text = state_text, .length = 1ULL, .color = state_color, .clip = on);
    const GuiRect text_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){rect.x, rect.y, (int)state_x - gap - rect.x, rect.height}, .other = on);
    const int baseline =
        EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_HEAD,
                   .size = row_size, .top = rect.y, .pixel_height = rect.height);
    const float after =
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_HEAD, .size = row_size, .x_f32 = (float)(x + icon + gap), .y = baseline,
                   .text = read.name, .length = name_length, .color = state_color, .clip = text_clip);
    if (read.folder_length != 0ULL)
    {
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = where_size, .x_f32 = after + (float)gap, .y = baseline,
                         .text = read.path, .length = read.folder_length,
                         .color = GUI_FRAME_FADED(args->palette->text_3, frame->side_share), .clip = text_clip);
    }
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

const GuiPaneKind gui_changes_kind = {
    .tool = "commit",
    .title = changes_title,
    .rows = changes_rows,
    .row_draw = changes_row_draw,
    .press = changes_press,
    .read = changes_read,
};
