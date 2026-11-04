// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes.c: the Changes pane. Each changed file is a row as the git module lists them; the pane's scroll and the row
// the pointer marks are the window's own
#include "changes.h"

#include "../../command_line/cli_internal.h"
#include "../../text/text.h"
#include "explorer.h"
#include "pane.h"

void gui_changes_init(const GuiChangesCfg *args)
{
    args->changes_out->pointed = GUI_PANE_NONE;
    args->changes_out->scroll = 0u;
}

typedef struct
{
    const GuiPalette *palette;
    char state;
} ChangesCtx;

unsigned int gui_changes_rows(const GuiChangesRowFrameCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_changes.rows, CliGitChangesCfg, .at = 0u);
}

int gui_changes_hit(const GuiChangesRowFrameCfg *args)
{
    return EMBED_CALL(gui_pane.hit, GuiPaneCfg, .frame = args->frame, .x = args->x, .y = args->y,
                      .scroll = args->changes->scroll, .count = gui_changes_row_frame.rows(args),
                      .row_pointer = args->row_pointer);
}

int gui_changes_move(const GuiChangesFrameCfg *args)
{
    unsigned int row = 0u;
    // the rows are fewer than an int counts
    const int pointed = EMBED_CALL(gui_changes_row_frame.hit, GuiChangesRowFrameCfg, .changes = args->changes_out,
                                   .frame = args->frame, .x = args->x, .y = args->y, .row_pointer = &row)
                            ? (int)row
                            : GUI_PANE_NONE;
    const int changed = pointed != args->changes_out->pointed;
    args->changes_out->pointed = pointed;
    return changed;
}

int gui_changes_wheel(const GuiChangesRowFrameCfg *args)
{
    return EMBED_CALL(gui_pane.scrolled, GuiPaneCfg, .frame = args->frame, .scroll_pointer = &args->changes_out->scroll,
                      .rows = args->rows, .count = gui_changes_row_frame.rows(args));
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

void gui_changes_draw(const GuiChangesRowFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const GuiRect body = EMBED_CALL(gui_pane.body, GuiPaneCfg, .raster = raster, .glyphs = glyphs,
                                    .palette = args->palette, .frame = args->frame, .title = "Changes");
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = body))
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
    const unsigned int rows = gui_changes_row_frame.rows(args);
    for (unsigned int row = args->changes->scroll; row < rows; row += 1u)
    {
        const GuiRect rect = EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .frame = args->frame, .row = row,
                                        .scroll = args->changes->scroll);
        if (rect.y >= body.y + body.height)
        {
            break;
        }
        CliChangedRow read;
        if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = row, .row = &read))
        {
            break;
        }
        const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = body);
        // the rows are fewer than an int counts
        if (args->changes->pointed == (int)row)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = on,
                       .color = GUI_FRAME_FADED(args->palette->line, args->frame->side_share));
        }
        const GuiColor state_color =
            GUI_FRAME_FADED(EMBED_CALL(changes_color, ChangesCtx, .palette = args->palette, .state = read.state),
                            args->frame->side_share);
        // the file's mark centered in its box
        const unsigned long long name_length =
            cellul.len(&(CatenaFinitaCfg){.src = read.name, .cap = CLI_PATH_MAX_LEN});
        char mark_text[GUI_EXPLORER_MARK_MOST];
        const unsigned int mark_color = EMBED_CALL(gui_explorer_mark_table.mark, GuiExplorerMarkCfg, .name = read.name,
                                                   .length = name_length, .text = mark_text);
        const unsigned long long mark_length =
            cellul.len(&(CatenaFinitaCfg){.src = mark_text, .cap = GUI_EXPLORER_MARK_MOST});
        const float mark_width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                       .face = GUI_FACE_CODE_BOLD, .size = mark_size, .text = mark_text, .length = mark_length);
        const int x = rect.x + pad;
        (void)EMBED_CALL(
            gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
            .face = GUI_FACE_CODE_BOLD, .size = mark_size, .x_f32 = (float)x + (((float)icon - mark_width) / 2.0f),
            .y = EMBED_CALL(gui_pane.baseline, GuiPaneCfg, .glyphs = glyphs, .face = GUI_FACE_CODE_BOLD,
                            .size = mark_size, .top = rect.y, .pixel_height = rect.height),
            .text = mark_text, .length = mark_length,
            .color = GUI_FRAME_FADED((mark_color != 0u) ? args->palette->mark[mark_color - 1u] : args->palette->text_3,
                                     args->frame->side_share),
            .clip = on);
        // the state letter at the right, bold in the code face
        const char state_text[2] = {read.state, '\0'};
        const float state_width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                       .face = GUI_FACE_CODE_BOLD, .size = state_size, .text = state_text, .length = 1ULL);
        const float state_x = (float)(rect.x + rect.width - pad) - state_width;
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE_BOLD, .size = state_size, .x_f32 = state_x,
                         .y = EMBED_CALL(gui_pane.baseline, GuiPaneCfg, .glyphs = glyphs, .face = GUI_FACE_CODE_BOLD,
                                         .size = state_size, .top = rect.y, .pixel_height = rect.height),
                         .text = state_text, .length = 1ULL, .color = state_color, .clip = on);
        // the name in the state's color, the folder quieter after it, both cut before the letter
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, (int)state_x - gap - rect.x, rect.height}, .other = on);
        const int baseline = EMBED_CALL(gui_pane.baseline, GuiPaneCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD,
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
                             .color = GUI_FRAME_FADED(args->palette->text_3, args->frame->side_share),
                             .clip = text_clip);
        }
    }
}
