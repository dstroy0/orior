// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// statusbar_test.c: the status bar's check: the memory reading written in megabytes, the cursor as line and column
// from one, the selection as lines or letters, the indentation as tabs or spaces; and the bar drawn with the file's
// crumbs at its left and the editor's line at its right, the lock at the end, bare where no file is open
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

static int same(const char *text, const char *wanted)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = text, .other = wanted, .cap = 64ull});
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    GuiGlyphs glyphs;
    if (!gui_glyphs_made(&glyphs, &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_checked();
    }
    GuiIcons icons;
    gui_icons_made(&icons);
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    char text[64];
    gui_status_memory_text(12897484ull, text, sizeof(text));
    cli_check(same(text, "12.3 MB"), "a reading under a hundred megabytes is written to a tenth");
    gui_status_memory_text(157286400ull, text, sizeof(text));
    cli_check(same(text, "150 MB"), "a reading past a hundred is written whole");
    GuiStatus status;
    memor.set(&(MemoriaCfg){.dst = &status, .val = 0, .bytes = sizeof(status)});
    status.path = "ui/gui/src/link.c";
    status.has_cursor = 1;
    status.line = 11ull;
    status.col = 2ull;
    status.anchor_line = 11ull;
    status.anchor_col = 2ull;
    status.indent_tabs = -1;
    gui_status_cursor_text(&status, text, sizeof(text));
    cli_check(same(text, "12:3"), "the cursor is written as its line and column from one");
    gui_status_selection_text(&status, text, sizeof(text));
    cli_check(same(text, ""), "no selection is written as nothing");
    status.anchor_col = 7ull;
    gui_status_selection_text(&status, text, sizeof(text));
    cli_check(same(text, "5 selected"), "a selection within a line is written as its letters");
    status.anchor_line = 9ull;
    gui_status_selection_text(&status, text, sizeof(text));
    cli_check(same(text, "3 lines selected"), "a selection across lines is written as its lines");
    gui_status_indent_text(&status, text, sizeof(text));
    cli_check(same(text, "4 spaces"), "an indentation nothing set is four spaces");
    status.indent_tabs = 1;
    status.indent_size = 4u;
    gui_status_indent_text(&status, text, sizeof(text));
    cli_check(same(text, "Tab 4"), "an indentation of tabs is written as a tab and its step");
    status.indent_tabs = 0;
    status.indent_size = 2u;
    gui_status_indent_text(&status, text, sizeof(text));
    cli_check(same(text, "2 spaces"), "an indentation of spaces is written as its step");

    GuiFrame frame;
    gui_frame_laid(&frame, 900u, 300u, 1.0f);
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 900u, 300u), "a raster for the bar is made");
    gui_raster_filled(&raster, (GuiRect){0, 0, 900, 300}, palette->ground);
    status.memory = 157286400ull;
    status.anchor_line = 11ull;
    status.anchor_col = 2ull;
    gui_statusbar_drawn(&raster, &glyphs, &icons, palette, &frame, &status);
    const GuiColor tint = raster.pixels[((unsigned int)(frame.status.y + 2) * 900u) + 30u];
    cli_check(tint != palette->ground, "the bar is tinted over the ground");
    cli_check(painted(&raster, (GuiRect){frame.strip.width + 16, frame.status.y + 6, 120, 16}, tint), "the file's crumbs are drawn at the bar's left");
    cli_check(painted(&raster, (GuiRect){900 - 16 - 16 - 14, frame.status.y + 6, 14, 16}, tint), "the lock is drawn at the bar's right end");
    cli_check(painted(&raster, (GuiRect){900 - 16 - 16 - 14 - 18 - 60, frame.status.y + 6, 60, 16}, tint), "the indentation is drawn before the lock");
    cli_check(!painted(&raster, (GuiRect){400, frame.status.y + 3, 100, 20}, tint), "the bar's middle is bare");

    gui_raster_filled(&raster, (GuiRect){0, 0, 900, 300}, palette->ground);
    status.path = "";
    status.has_cursor = 0;
    gui_statusbar_drawn(&raster, &glyphs, &icons, palette, &frame, &status);
    cli_check(!painted(&raster, (GuiRect){frame.strip.width + 16, frame.status.y + 6, 120, 16}, tint), "with no file open the left is bare");
    cli_check(painted(&raster, (GuiRect){900 - 16 - 30, frame.status.y + 6, 30, 16}, tint), "the memory reading ends at the bar's right padding");
    cli_check(!painted(&raster, (GuiRect){900 - 16 - 16 - 14 - 18 - 200, frame.status.y + 6, 180, 16}, tint), "and the editor's line is away");
    gui_raster_given(&raster);
    gui_icons_given(&icons);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
