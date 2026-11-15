// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// references.c: the References pane's report and rows as the CLI lists the references found
#include "references.h"

#include "../../cli/src/code/references.h"
#include "../../cli/src/command_line/cli_internal.h"
#include "explorer.h"

static const char *references_title(const GuiPaneCfg *args)
{
    (void)args;
    return "References";
}

static unsigned int references_rows(const GuiPaneCfg *args)
{
    (void)args;
    return cli_references_rows();
}

// the report's row
static int references_head(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_REFERENCES_REPORT;
}

// the report drawn quiet in the head's row
static void references_head_draw(const GuiPaneCfg *args)
{
    unsigned long long reported = 0ULL;
    const char *const report = cli_references_report(&(CliReferencesLengthCfg){.length_out = &reported});
    if (reported == 0ULL)
    {
        return;
    }
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                     .size = GUI_REFERENCES_REPORT_SIZE, .x_f32 = (float)(args->rect.x + pad), .text = report,
                     .length = reported, .color = args->palette->text_3, .clip = args->rect);
}

// a row drawn: a file's arrow, name, folder and its count in a pill at the right; a reference's guide, its place at
// the right and its line's text from a little before the name, the name in signal
static void references_row_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    const GuiFrame *const frame = args->frame;
    const GuiPalette *const palette = args->palette;
    const GuiRect rect = args->rect;
    CliReferenceRow row;
    if (!cli_references_row(&(CliReferencesAtRowCfg){.at = args->row, .row = &row}))
    {
        return;
    }
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    // the rows are fewer than an int counts
    const int pointed = args->pane_read->pointed == (int)args->row;
    const GuiColor text_color = pointed ? palette->text : palette->text_2;
    if (row.type == CLI_REFERENCES_ROW_FILE)
    {
        int x = rect.x + pad;
        const int twisty = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_EXPLORER_TWISTY);
        const int arrow = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_ARROW);
        EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = raster, .x = x + ((twisty - arrow) / 2),
                   .y = rect.y + ((rect.height - arrow) / 2), .side = arrow, .open = row.open,
                   .color = GUI_FRAME_FADED(palette->text_2, frame->side_share), .clip = args->clip);
        x += twisty;
        char digits[16];
        VerbaLinea count = {digits, sizeof(digits), 0ULL};
        mmgr_line_decimal(&count, row.count, 1u);
        const int count_pad =
            EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_REFERENCES_COUNT_PAD);
        const float width = EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = args, .face = GUI_FACE_CODE,
                                       .size = GUI_REFERENCES_PLACE_SIZE, .text = digits, .length = count.at);
        const int pill_width = (int)(width + 0.5f) + (2 * count_pad);
        const int pill_height =
            rect.height - (2 * EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = 4));
        const GuiRect pill = {rect.x + rect.width - pad - pill_width, rect.y + ((rect.height - pill_height) / 2),
                              pill_width, pill_height};
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = pill, .other = args->clip),
                   .radius = (unsigned int)(pill_height / 2),
                   .color = GUI_FRAME_FADED(pointed ? palette->hover : palette->line, frame->side_share));
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                         .size = GUI_REFERENCES_PLACE_SIZE, .x_f32 = (float)(pill.x + count_pad), .text = digits,
                         .length = count.at, .color = palette->text_2, .clip = rect);
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, pill.x - pad - rect.x, rect.height}, .other = args->clip);
        (void)EMBED_CALL(gui_explorer_file.file_draw, GuiExplorerFileCfg, .pane = args, .x = x, .name = row.name,
                         .length = row.name_length, .folder = row.path, .folder_length = row.folder_length,
                         .color = text_color, .clip = text_clip);
        return;
    }
    const int x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = raster, .palette = palette, .frame = frame,
                             .rect = rect, .clip = args->clip, .depth = 1u);
    // the place, line and column counted from 1, at the right
    char place_text[48];
    VerbaLinea place = {place_text, sizeof(place_text), 0ULL};
    mmgr_line_decimal(&place, row.line + 1ULL, 1u);
    mmgr_line_ch(&place, ':');
    mmgr_line_decimal(&place, row.col + 1ULL, 1u);
    const float place_width = EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = args, .face = GUI_FACE_CODE,
                                         .size = GUI_REFERENCES_PLACE_SIZE, .text = place_text, .length = place.at);
    const float place_x = (float)(rect.x + rect.width - pad) - place_width;
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                     .size = GUI_REFERENCES_PLACE_SIZE, .x_f32 = place_x, .text = place_text, .length = place.at,
                     .color = palette->text_3, .clip = rect);
    const GuiRect text_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){rect.x, rect.y, (int)place_x - pad - rect.x, rect.height}, .other = args->clip);
    // the line from a little before the name where the name is past the lead, its blanks before the lead passed
    // over, the name in signal, and what follows the name
    unsigned long long from = (row.match_at > GUI_REFERENCES_LEAD_PAST) ? row.match_at - GUI_REFERENCES_LEAD : 0ULL;
    while ((from < row.match_at) && ((row.text[from] == ' ') || (row.text[from] == '\t')))
    {
        from += 1ULL;
    }
    const unsigned long long match_end =
        (row.match_at + row.match_length < row.text_length) ? row.match_at + row.match_length : row.text_length;
    float at = (float)x;
    if (from > 0ULL)
    {
        at = EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                        .size = GUI_REFERENCES_TEXT_SIZE, .x_f32 = at, .text = "\xE2\x80\xA6", .length = 3ULL,
                        .color = palette->text_3, .clip = text_clip);
    }
    at = EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                    .size = GUI_REFERENCES_TEXT_SIZE, .x_f32 = at, .text = row.text + from,
                    .length = (row.match_at > from) ? row.match_at - from : 0ULL, .color = text_color,
                    .clip = text_clip);
    at = EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE_BOLD,
                    .size = GUI_REFERENCES_TEXT_SIZE, .x_f32 = at, .text = row.text + row.match_at,
                    .length = match_end - row.match_at, .color = palette->signal, .clip = text_clip);
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                     .size = GUI_REFERENCES_TEXT_SIZE, .x_f32 = at, .text = row.text + match_end,
                     .length = row.text_length - match_end, .color = text_color, .clip = text_clip);
}

// a file's row pressed closes or opens its references; a reference's opens the file at the name
static int references_press(const GuiPaneCfg *args)
{
    CliReferenceRow row;
    if (!cli_references_press(&(CliReferencesAtRowCfg){.at = args->row, .row = &row}))
    {
        return 0;
    }
    *args->open_out = (GuiPaneOpen){row.path, {row.line, row.col}, 1};
    return 1;
}

const GuiPaneTable gui_references_table = {
    .tool = "references",
    .title = references_title,
    .rows = references_rows,
    .head = references_head,
    .head_draw = references_head_draw,
    .row_draw = references_row_draw,
    .press = references_press,
};
