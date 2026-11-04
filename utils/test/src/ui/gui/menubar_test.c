// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// menubar_test.c: the menu bar's check: a menu asked for at a point stands there, turned in from the window's edges; a
// menu too long for the window stands no taller than the window less 3.5rem and scrolls: the wheel over it moves its
// rows a hundred pixels a notch, no further than its last, a row marked anew is scrolled into view, and a menu opened
// anew stands at its first row
#include "../../../../../ui/cli/src/view/parts/menubar.h"
#include "../cli/cli_check.h"

#define MENUBAR_TEST_ROWS 60u

static CliItem s_rows[MENUBAR_TEST_ROWS];

// the row of the open menu the pixel at x, y stands on, the rows' count where it stands on none
static unsigned int row_at(const GuiMenubar *menubar, int x, int y)
{
    unsigned int level = 0u;
    unsigned int index = MENUBAR_TEST_ROWS;
    const unsigned int hit = EMBED_CALL(gui_menubar_x.hit, GuiMenubarXCfg, .menubar_read = menubar, .scale = 100, .x = x, .y = y, .level = &level, .index = &index);
    return (hit == CLI_HIT_MENU_ROW) ? index : MENUBAR_TEST_ROWS + 1u;
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    for (unsigned int at = 0u; at < MENUBAR_TEST_ROWS; at += 1u)
    {
        s_rows[at] = (CliItem){"Row", "row", "", "", "", "", 0, 0, 0, 0, 0u};
    }
    cli_menus_own_open(s_rows, MENUBAR_TEST_ROWS, CLI_MENU_PLACE_POINT, 300, 100);
    static GuiMenubar menubar;
    EMBED_CALL(gui_menubar.init, GuiMenubarCfg, .menubar = &menubar);
    EMBED_CALL(gui_menubar_bar.menus_layout, GuiMenubarBarCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame);
    const GuiRect menu = menubar.menu_rect[0];
    cli_check(menu.height == (920 - 56), "a menu too long for the window stands no taller than the window less 3.5rem");
    cli_check((menu.x == 300) && (menu.y == 4), "turned up from the window's foot, it stands at the window's top edge less its margin");
    const int first_y = menu.y + GUI_MENU_PAD + (GUI_ITEM_HEIGHT / 2);
    cli_check(row_at(&menubar, 310, first_y) == 0u, "unscrolled, its first row stands at its top");

    cli_check(EMBED_CALL(gui_menubar_glyphs_frame_x.wheel, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = 310, .y = first_y, .rows = 3),
              "the wheel over a menu is the menu's");
    cli_check(row_at(&menubar, 310, first_y) == (unsigned int)((100 + (GUI_ITEM_HEIGHT / 2)) / GUI_ITEM_HEIGHT), "a notch scrolls its rows a hundred pixels");
    cli_check(!EMBED_CALL(gui_menubar_glyphs_frame_x.wheel, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = 100, .y = 400, .rows = 3),
              "the wheel away from the menus is not theirs");
    (void)EMBED_CALL(gui_menubar_glyphs_frame_x.wheel, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = 310, .y = first_y, .rows = 300);
    const int last_y = menu.y + menu.height - GUI_MENU_PAD - (GUI_ITEM_HEIGHT / 2);
    cli_check(row_at(&menubar, 310, last_y) == (MENUBAR_TEST_ROWS - 1u), "scrolled to its end, its last row stands at its foot and no further");

    g_cli_menus_open.level[0].pointed = 2;
    EMBED_CALL(gui_menubar_bar.menus_layout, GuiMenubarBarCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame);
    cli_check(row_at(&menubar, 310, menu.y + 1 + (GUI_ITEM_HEIGHT / 2)) == 2u, "a row marked anew above the rows shown is scrolled to the top");
    (void)EMBED_CALL(gui_menubar_glyphs_frame_x.wheel, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = 310, .y = first_y, .rows = 3);
    EMBED_CALL(gui_menubar_bar.menus_layout, GuiMenubarBarCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame);
    cli_check(row_at(&menubar, 310, first_y) != 0u, "the same row still marked leaves the rows where the wheel moved them");

    cli_menus_own_open(s_rows, MENUBAR_TEST_ROWS, CLI_MENU_PLACE_POINT, 300, 100);
    EMBED_CALL(gui_menubar_bar.menus_layout, GuiMenubarBarCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame);
    cli_check(row_at(&menubar, 310, first_y) == 0u, "a menu opened anew stands at its first row");

    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
