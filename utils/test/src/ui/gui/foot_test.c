// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// foot_test.c: the check of a foot table's pane: a table marked `foot` stands in the frame's terminal panel and a side
// table in the side; its rows are drawn inside the panel and nowhere in the side; a pixel in the panel hits its row and
// one in the side does not; the wheel scrolls its rows no further than the last fills the panel; and its text is faded
// by the panel's share
#include "../../../../../ui/cli/src/view/parts/explorer.h"
#include "../../../../../ui/cli/src/view/parts/pane.h"
#include "../cli/cli_check.h"

#define FOOT_ROWS 40u
#define FOOT_HEIGHT 220

static const char *foot_title(const GuiPaneCfg *args)
{
    (void)args;
    return "Foot";
}

static unsigned int foot_rows(const GuiPaneCfg *args)
{
    (void)args;
    return FOOT_ROWS;
}

// each row drawn as its number in the text color: the panel carries the pane's pixels
static void foot_row_draw(const GuiPaneCfg *args)
{
    char digits[12];
    VerbaLinea line = {digits, sizeof(digits), 0ULL};
    mmgr_line_decimal(&line, args->row, 1u);
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .face = GUI_FACE_HEAD,
                     .size = GUI_PANE_ROW_SIZE,
                     .x_f32 = (float)(args->rect.x +
                                      EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT)),
                     .text = digits, .length = line.at, .color = args->palette->text);
}

static const GuiPaneTable s_foot_table = {
    .tool = "foot_check",
    .foot = 1,
    .title = foot_title,
    .rows = foot_rows,
    .row_draw = foot_row_draw,
};

// whether any pixel of `rect` in the raster differs from `ground`
static int rect_touched(const GuiRaster *raster, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            // the rectangle stands inside the raster
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground)
            {
                return 1;
            }
        }
    }
    return 0;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = FOOT_HEIGHT, .away = 0);
    GuiIcons icons;
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &icons);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");

    const GuiRect foot = EMBED_CALL(gui_pane.region, GuiPaneCfg, .table = &s_foot_table, .frame = &frame);
    const GuiRect side = EMBED_CALL(gui_pane.region, GuiPaneCfg, .table = &gui_explorer_table, .frame = &frame);
    cli_check((foot.x == frame.terminal.x) && (foot.y == frame.terminal.y) && (foot.width == frame.terminal.width) &&
                  (foot.height == frame.terminal.height) && (foot.height > 0),
              "a foot table's region is the frame's terminal panel");
    cli_check((side.x == frame.side.x) && (side.width == frame.side.width) && (side.width > 0),
              "a side table's region is the frame's side");

    // an opaque ground no part of the pane is drawn in: a pixel the pane touches differs from it
    const GuiColor ground = 0xFF102030u;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster,
               .rect = (GuiRect){0, 0, 1440, 920}, .color = ground);
    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = &s_foot_table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .icons = &icons, .palette = palette, .title = "Foot", .tree = "");
    cli_check(rect_touched(&raster, frame.terminal, ground), "the foot table draws inside the panel");
    cli_check(!rect_touched(&raster, frame.side, ground), "the foot table draws nothing in the side");
    cli_check(!rect_touched(&raster, frame.editor, ground), "the foot table draws nothing in the editor");

    const int rows_top = EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = &s_foot_table, .frame = &frame);
    cli_check(rows_top > frame.terminal.y, "the rows start under the panel's head");
    unsigned int row = 99u;
    cli_check(EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = &s_foot_table, .pane_read = &pane, .frame = &frame,
                         .x = frame.terminal.x + 20, .y = rows_top + 3, .row_pointer = &row) &&
                  (row == 0u),
              "a pixel under the panel's head hits the first row");
    cli_check(!EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = &s_foot_table, .pane_read = &pane, .frame = &frame,
                          .x = frame.side.x + 20, .y = frame.side.y + 100, .row_pointer = &row),
              "a pixel in the side hits no row of the foot table");

    const int row_height = EMBED_CALL(gui_pane.row_height, GuiPaneCfg, .table = &s_foot_table, .frame = &frame);
    const int displayed = (frame.terminal.y + frame.terminal.height - rows_top) / row_height;
    cli_check(EMBED_CALL(gui_pane.wheel, GuiPaneCfg, .table = &s_foot_table, .pane = &pane, .frame = &frame,
                         .rows = 1000) &&
                  (pane.scroll == FOOT_ROWS - (unsigned int)displayed),
              "the wheel scrolls no further than the last row fills the panel");
    cli_check(!EMBED_CALL(gui_pane.wheel, GuiPaneCfg, .table = &s_foot_table, .pane = &pane, .frame = &frame,
                          .rows = 1),
              "a turn past the end moves nothing");

    frame.terminal_share = 50;
    const GuiPaneCfg faded = {.table = &s_foot_table, .frame = &frame};
    cli_check(EMBED_CALL(gui_pane_part.share, GuiPanePartCfg, .pane = &faded) == 50,
              "a foot table's text fades with the panel's share");
    const GuiPaneCfg sided = {.table = &gui_explorer_table, .frame = &frame};
    cli_check(EMBED_CALL(gui_pane_part.share, GuiPanePartCfg, .pane = &sided) == frame.side_share,
              "a side table's text fades with the side's share");

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
