// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// statusbar_test.c: the status bar's check: the memory reading written in megabytes, the cursor as line and column
// from one, the selection as lines or letters, the indentation as tabs or spaces; and the bar drawn with the file's
// crumbs at its left and the editor's line at its right, the lock at the end, bare where no file is open
#include "../../../../../ui/gui/src/base.h"
#include "../../../../../ui/gui/src/parts/statusbar.h"
#include "../cli/cli_check.h"

// whether any pixel of `rect` is not `ground`
static int painted(const GuiRaster *raster, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground)
            {
                return 1;
            }
        }
    }
    return 0;
}

// whether every pixel of `rect` is the pane alone, as `base`, the frame's base drawn by itself, laid it
static int bare(const GuiRaster *raster, GuiRect rect, const GuiRaster *base)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != base->pixels[((unsigned int)y * base->width) + (unsigned int)x])
            {
                return 0;
            }
        }
    }
    return 1;
}

static int same(const char *text, const char *wanted)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = text, .other = wanted, .cap = 64ull});
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_on_check();
    }
    GuiIcons icons;
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &icons);
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    char text[64];
    EMBED_CALL(gui_statusbar_bytes.status_memory_text, GuiStatusbarBytesCfg, .bytes = 12897484ull, .text = text, .size = sizeof(text));
    cli_check(same(text, "12.3 MB"), "a reading under a hundred megabytes is written to a tenth");
    EMBED_CALL(gui_statusbar_bytes.status_memory_text, GuiStatusbarBytesCfg, .bytes = 157286400ull, .text = text, .size = sizeof(text));
    cli_check(same(text, "150 MB"), "a reading past a hundred is written whole");
    GuiStatus status;
    memor.set(&(MemoriaCfg){.dst = &status, .val = 0, .bytes = sizeof(status)});
    status.path = "ui/gui/src/interface.c";
    status.has_cursor = 1;
    status.line = 11ull;
    status.col = 2ull;
    status.anchor_line = 11ull;
    status.anchor_col = 2ull;
    status.indent_tabs = -1;
    EMBED_CALL(gui_statusbar.status_cursor_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "12:3"), "the cursor is written as its line and column from one");
    EMBED_CALL(gui_statusbar.status_selection_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, ""), "no selection is written as nothing");
    status.anchor_col = 7ull;
    EMBED_CALL(gui_statusbar.status_selection_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "5 selected"), "a selection within a line is written as its letters");
    status.anchor_line = 9ull;
    EMBED_CALL(gui_statusbar.status_selection_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "3 lines selected"), "a selection across lines is written as its lines");
    EMBED_CALL(gui_statusbar.status_indent_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "4 spaces"), "an indentation nothing set is four spaces");
    status.indent_tabs = 1;
    status.indent_size = 4u;
    EMBED_CALL(gui_statusbar.status_indent_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "Tab 4"), "an indentation of tabs is written as a tab and its step");
    status.indent_tabs = 0;
    status.indent_size = 2u;
    EMBED_CALL(gui_statusbar.status_indent_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "2 spaces"), "an indentation of spaces is written as its step");

    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 900u, .height = 300u, .scale = 100);
    GuiRaster raster;
    GuiRaster bare_base;
    GuiBase base;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 900u, .height = 300u) && EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &bare_base, .width = 900u, .height = 300u) && EMBED_CALL(gui_base_width.init, GuiBaseWidthCfg, .base = &base, .width = 900u, .height = 300u),
              "a raster for the bar, one for the base alone and the base are made");
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &bare_base, .ground = NULL, .color = palette->ground);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .ground = NULL, .color = palette->ground);
    status.memory = 157286400ull;
    status.anchor_line = 11ull;
    status.anchor_col = 2ull;
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &frame, .status = &status);
    const GuiColor tint = raster.pixels[((unsigned int)(frame.status.y + 2) * 900u) + 30u];
    cli_check(tint != palette->ground, "the bar is tinted over the ground");
    cli_check(painted(&raster, (GuiRect){frame.strip.width + 16, frame.status.y + 6, 120, 16}, tint), "the file's crumbs are drawn at the bar's left");
    cli_check(painted(&raster, (GuiRect){900 - 16 - 16 - 14, frame.status.y + 6, 14, 16}, tint), "the lock is drawn at the bar's right end");
    cli_check(painted(&raster, (GuiRect){900 - 16 - 16 - 14 - 18 - 60, frame.status.y + 6, 60, 16}, tint), "the indentation is drawn before the lock");
    cli_check(bare(&raster, (GuiRect){400, frame.status.y + 3, 100, 20}, &bare_base), "the bar's middle is bare");

    // the claim holding with the bar come back, the crumbs stand at the bar's left with no strip's room before them
    GuiClaimNumbers numbers = frame.claim;
    numbers.claim = GUI_WHOLE;
    numbers.reveal_bottom = GUI_WHOLE;
    numbers.room_bottom = GUI_WHOLE;
    GuiFrame claimed = frame;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &claimed, .numbers = &numbers);
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &claimed, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &bare_base, .ground = NULL, .color = palette->ground);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .ground = NULL, .color = palette->ground);
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &claimed, .status = &status);
    cli_check(!bare(&raster, (GuiRect){claimed.status.x + 8, claimed.status.y + 6, 160, 16}, &bare_base), "the crumbs stand at the bar's left while the claim holds");
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &bare_base, .ground = NULL, .color = palette->ground);

    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .ground = NULL, .color = palette->ground);
    status.path = "";
    status.has_cursor = 0;
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &frame, .status = &status);
    cli_check(bare(&raster, (GuiRect){frame.strip.width + 16, frame.status.y + 6, 120, 16}, &bare_base), "with no file open the left is bare");
    cli_check(painted(&raster, (GuiRect){900 - 16 - 30, frame.status.y + 6, 30, 16}, tint), "the memory reading ends at the bar's right padding");
    cli_check(bare(&raster, (GuiRect){900 - 16 - 16 - 14 - 18 - 200, frame.status.y + 6, 180, 16}, &bare_base), "and the editor's line is away");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &bare_base);
    EMBED_CALL(gui_base_width.free, GuiBaseWidthCfg, .base = &base);
    EMBED_CALL(cli_icons.free, GuiIconsCfg, .icons = &icons);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
