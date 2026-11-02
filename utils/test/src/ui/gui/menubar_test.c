// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// menubar_test.c: the menu bar's check: the catalog's menus with items that are neither the strip's nor popups are
// titles on the bar, each with a letter for Alt; a title pressed opens its menu, with its hidden and filled items left
// out and the ones that need an open file drawn but not chosen; the pointer on another title opens that one; the keys
// step the mark, open the items under an item and choose one; Alt with a letter opens a menu; a narrow window folds
// the titles into a last one that lists them; and the titles and an open menu are drawn
#include "../../../../../ui/gui/src/parts/menubar.h"
#include "../cli/cli_check.h"

static const char s_catalog[] =
    "{\"menus\":["
    "{\"title\":\"File\",\"items\":["
    "{\"label\":\"Open\xe2\x80\xa6\",\"command\":\"open-file\",\"keys\":\"Ctrl+O\"},"
    "\"-\","
    "{\"label\":\"Recent\",\"items\":[{\"label\":\"a.c\",\"command\":\"open-recent\"}]},"
    "{\"label\":\"Hidden\",\"command\":\"hidden\",\"hidden\":true},"
    "{\"label\":\"Macros\",\"fill\":\"macros\"},"
    "{\"label\":\"Save\",\"command\":\"save\",\"keys\":\"Ctrl+S\",\"needs\":\"editor\"}]},"
    "{\"title\":\"Edit\",\"items\":[{\"label\":\"Undo\",\"command\":\"undo\",\"keys\":\"Ctrl+Z\"}]},"
    "{\"title\":\"Protocol\",\"strip\":\"protocol\",\"items\":[{\"label\":\"Run\",\"command\":\"protocol-run\"}]},"
    "{\"title\":\"Files\",\"popup\":true,\"items\":[{\"label\":\"Rename\",\"command\":\"rename\"}]},"
    "{\"title\":\"Build\",\"groups\":[\"build\"]},"
    "{\"title\":\"Help\",\"items\":[{\"label\":\"About\",\"command\":\"about\"}]}]}";

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

// the middle of a rectangle
static int middle_x(GuiRect rect)
{
    return rect.x + (rect.width / 2);
}

static int middle_y(GuiRect rect)
{
    return rect.y + (rect.height / 2);
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
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_catalog, .size = sizeof(s_catalog) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(root != CLI_JSON_NONE, "the catalog is read");
    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 900u, .scale = 100);
    GuiMenubar menubar;
    EMBED_CALL(gui_menubar_glyphs.init, GuiMenubarGlyphsCfg, .menubar = &menubar);
    EMBED_CALL(gui_menubar_json.catalog, GuiMenubarJsonCfg, .menubar = &menubar, .json = &json, .menus = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = root, .name = "menus"));
    EMBED_CALL(gui_menubar_json.layout, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame);
    cli_check(menubar.titles == 3u, "the menus with items that are neither the strip's nor popups are the titles");
    cli_check((menubar.title[0].menu == 0u) && (menubar.title[1].menu == 1u) && (menubar.title[2].menu == 5u),
              "each title opens its own menu");
    cli_check((menubar.title[0].letter == 0u) && (menubar.title[1].letter == 0u) && (menubar.title[2].letter == 0u),
              "each title has a letter for Alt, the first no title before it took");
    cli_check(menubar.title[0].rect.x == frame.bar.x + 54, "the titles start past the eye's room, 52 and a title's margin of 2");
    cli_check((menubar.title[0].rect.x < menubar.title[1].rect.x) &&
                  (menubar.title[1].rect.x < menubar.title[2].rect.x) && (menubar.title[0].rect.y > frame.bar.y) &&
                  (menubar.title[0].rect.height < frame.bar.height),
              "the titles stand in a row inside the bar");
    cli_check(EMBED_CALL(gui_menubar_x.holds, GuiMenubarXCfg, .menubar_read = &menubar, .x = middle_x(menubar.title[0].rect), .y = middle_y(menubar.title[0].rect)) &&
                  !EMBED_CALL(gui_menubar_x.holds, GuiMenubarXCfg, .menubar_read = &menubar, .x = frame.bar.x + 2, .y = middle_y(menubar.title[0].rect)),
              "a title holds its pixels and the bar beside it does not");

    GuiChoice choice;
    cli_check(EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = middle_x(menubar.title[0].rect), .y = middle_y(menubar.title[0].rect), .choice = &choice) &&
                  (menubar.open == 0) && (menubar.levels == 1u) && (choice.command[0] == '\0'),
              "a title pressed opens its menu");
    cli_check(EMBED_CALL(gui_menubar_level.rows, GuiMenubarLevelCfg, .menubar_read = &menubar, .level = 0u) == 4u, "the menu's rows leave the hidden and the filled items out");
    GuiRow row;
    cli_check(EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = &menubar, .level = 0u, .index = 0u, .row = &row) && (row.kind == GUI_ROW_ITEM) && (row.label_length == 7ull) &&
                  (row.keys_length == 6ull) && !row.disabled,
              "the first row is the open item with its keys");
    cli_check(EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = &menubar, .level = 0u, .index = 1u, .row = &row) && (row.kind == GUI_ROW_LINE), "the second row is a line");
    cli_check(EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = &menubar, .level = 0u, .index = 2u, .row = &row) && (row.kind == GUI_ROW_MORE) && (row.items != CLI_JSON_NONE),
              "the third row has items of its own");
    cli_check(EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = &menubar, .level = 0u, .index = 3u, .row = &row) && (row.kind == GUI_ROW_ITEM) && row.disabled,
              "the item that needs an open file cannot be chosen while none is");
    cli_check(menubar.menu[0].pointed == 0, "the first row is marked as the menu opens from a press");
    cli_check((menubar.menu[0].rect.y > frame.bar.y + frame.bar.height) &&
                  (menubar.menu[0].rect.x == menubar.title[0].rect.x),
              "the menu opens under its title");
    g_test_clock_ms = 0ull;
    cli_check(EMBED_CALL(gui_menubar_glyphs_frame_x.move, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = middle_x(menubar.title[1].rect), .y = middle_y(menubar.title[1].rect)) &&
                  (menubar.open == 1) && (menubar.menu[0].pointed == -1),
              "the pointer on another title opens its menu, no row marked");
    cli_check(EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = middle_x(menubar.title[1].rect), .y = middle_y(menubar.title[1].rect), .choice = &choice) &&
                  (menubar.open == -1) && (menubar.levels == 0u),
              "the open title pressed closes its menu");

    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Alt+F", .choice = &choice) && (menubar.open == 0) &&
                  (menubar.menu[0].pointed == 0),
              "Alt with a title's letter opens its menu, the first row marked");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Down", .choice = &choice) && (menubar.menu[0].pointed == 2),
              "Down steps the mark over the line");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Down", .choice = &choice) && (menubar.menu[0].pointed == 0),
              "Down steps around the end over the item that cannot be chosen");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "End", .choice = &choice) && (menubar.menu[0].pointed == 2),
              "End marks the last row that can be chosen");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Right", .choice = &choice) && (menubar.levels == 2u) &&
                  (menubar.menu[0].opened == 2) && (menubar.menu[1].pointed == 0) &&
                  (menubar.menu[1].rect.x > menubar.menu[0].rect.x),
              "Right opens the items under the row beside it");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Left", .choice = &choice) && (menubar.levels == 1u) &&
                  (menubar.menu[0].opened == -1),
              "Left closes them again");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Enter", .choice = &choice) && (menubar.levels == 2u),
              "Enter on the row opens them too");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Enter", .choice = &choice) && (menubar.levels == 0u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "open-recent", .cap = 256ull}),
              "Enter on an item chooses it and closes the menus");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Alt+E", .choice = &choice) && (menubar.open == 1),
              "Alt+E opens the second menu");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Right", .choice = &choice) && (menubar.open == 2),
              "Right in a menu opens the next title's");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Right", .choice = &choice) && (menubar.open == 0),
              "Right at the last title comes around to the first");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "S", .choice = &choice) && (menubar.menu[0].pointed == 0),
              "a letter marks the next row it begins, none where only an item that cannot be chosen does");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "R", .choice = &choice) && (menubar.menu[0].pointed == 2),
              "R marks Recent");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Escape", .choice = &choice) && (menubar.levels == 0u) &&
                  (menubar.open == -1),
              "Escape closes the menu");
    cli_check(!EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Down", .choice = &choice),
              "a key with no menu open is not the bar's");

    menubar.has_editor = 1;
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Alt+F", .choice = &choice) &&
                  EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "S", .choice = &choice) && (menubar.menu[0].pointed == 3),
              "with a file open the item that needs one can be chosen");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Enter", .choice = &choice) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "save", .cap = 256ull}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.keys, .other = "Ctrl+S", .cap = 256ull}),
              "the choice carries the item's command and keys");

    // a key pressed with no menu open is looked up among every item's keys
    cli_check(EMBED_CALL(gui_menubar_choice.keys_command, GuiMenubarChoiceCfg, .menubar_read = &menubar, .name = "Mod+Z", .choice = &choice) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "undo", .cap = 256ull}),
              "a key an item names, Ctrl read as Mod, finds the item's command");
    cli_check(EMBED_CALL(gui_menubar_choice.keys_command, GuiMenubarChoiceCfg, .menubar_read = &menubar, .name = "Mod+S", .choice = &choice) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "save", .cap = 256ull}),
              "an item that needs a file is found while one is open");
    menubar.has_editor = 0;
    cli_check(!EMBED_CALL(gui_menubar_choice.keys_command, GuiMenubarChoiceCfg, .menubar_read = &menubar, .name = "Mod+S", .choice = &choice) && (choice.command[0] == '\0'),
              "and not while none is");
    menubar.has_editor = 1;
    cli_check(!EMBED_CALL(gui_menubar_choice.keys_command, GuiMenubarChoiceCfg, .menubar_read = &menubar, .name = "Mod+Q", .choice = &choice) &&
                  !EMBED_CALL(gui_menubar_choice.keys_command, GuiMenubarChoiceCfg, .menubar_read = &menubar, .name = "Z", .choice = &choice),
              "a key no item names finds nothing");

    // a press on a row with the pointer, and on an item of the menu beside it
    cli_check(EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = middle_x(menubar.title[0].rect), .y = middle_y(menubar.title[0].rect), .choice = &choice) &&
                  (menubar.levels == 1u),
              "the first menu opens again");
    const GuiRect more = {menubar.menu[0].rect.x + 10, menubar.menu[0].rect.y + 5 + 27 + 11 + 13, 10, 1};
    g_test_clock_ms = 1000ull;
    cli_check(EMBED_CALL(gui_menubar_glyphs_frame_x.move, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = more.x, .y = more.y) && (menubar.levels == 1u) &&
                  (menubar.menu[0].pointed == 2) && (EMBED_CALL(gui_menubar.next_deadline_ms, GuiMenubarCfg, .menubar_read = &menubar) == 1000ull + GUI_SUBMENU_DELAY_MS),
              "the pointer on a row with items marks it and begins the rest before they open");
    g_test_clock_ms = 1100ull;
    cli_check(!EMBED_CALL(gui_menubar_glyphs_frame_x.tick, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame) && (menubar.levels == 1u),
              "a tick before the rest ends opens nothing");
    g_test_clock_ms = 1000ull + GUI_SUBMENU_DELAY_MS;
    cli_check(EMBED_CALL(gui_menubar_glyphs_frame_x.tick, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame) && (menubar.levels == 2u) &&
                  (menubar.menu[0].opened == 2) && (EMBED_CALL(gui_menubar.next_deadline_ms, GuiMenubarCfg, .menubar_read = &menubar) == 0ull),
              "the rest's end opens the items beside the row");
    g_test_clock_ms = 2000ull;
    cli_check(EMBED_CALL(gui_menubar_glyphs_frame_x.move, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = more.x, .y = menubar.menu[0].rect.y + 5 + 13) &&
                  (menubar.levels == 2u) && (menubar.menu[0].pointed == 0) &&
                  (EMBED_CALL(gui_menubar.next_deadline_ms, GuiMenubarCfg, .menubar_read = &menubar) == 2000ull + GUI_SUBMENU_DELAY_MS),
              "the pointer on a sibling without items keeps them open and begins the rest before they close");
    g_test_clock_ms = 2050ull;
    cli_check(EMBED_CALL(gui_menubar_glyphs_frame_x.move, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = more.x, .y = more.y) &&
                  (EMBED_CALL(gui_menubar.next_deadline_ms, GuiMenubarCfg, .menubar_read = &menubar) == 0ull),
              "the pointer back on the opened row lets the rest go");
    g_test_clock_ms = 2100ull;
    cli_check(EMBED_CALL(gui_menubar_glyphs_frame_x.move, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = more.x, .y = menubar.menu[0].rect.y + 5 + 13) &&
                  EMBED_CALL(gui_menubar_glyphs_frame_x.tick, GuiMenubarGlyphsFrameXCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame) && (menubar.levels == 1u) &&
                  (menubar.menu[0].opened == -1),
              "the rest's end on the sibling closes the items");
    cli_check(EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = more.x, .y = more.y, .choice = &choice) && (menubar.levels == 2u),
              "a press on the row opens its items at once");
    cli_check(EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = menubar.menu[1].rect.x + 10, .y = menubar.menu[1].rect.y + 5 + 13, .choice = &choice) &&
                  (menubar.levels == 0u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "open-recent", .cap = 256ull}),
              "a press on an item beside chooses it");
    cli_check(EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = middle_x(menubar.title[2].rect), .y = middle_y(menubar.title[2].rect), .choice = &choice) &&
                  !EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = 400, .y = 400, .choice = &choice) && (menubar.levels == 0u),
              "a press outside the menus closes them and is not the bar's");

    // the titles and an open menu drawn
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 300u), "a raster for the bar is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1440, 300}, .color = palette->ground);
    (void)EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = middle_x(menubar.title[0].rect), .y = middle_y(menubar.title[0].rect), .choice = &choice);
    EMBED_CALL(gui_menubar_glyphs_frame.titles_draw, GuiMenubarGlyphsFrameCfg, .menubar_read = &menubar, .raster = &raster, .glyphs = &glyphs, .palette = palette, .frame = &frame);
    EMBED_CALL(gui_menubar_glyphs_frame.menus_draw, GuiMenubarGlyphsFrameCfg, .menubar_read = &menubar, .raster = &raster, .glyphs = &glyphs, .palette = palette, .frame = &frame);
    cli_check(painted(&raster, menubar.title[1].rect, palette->ground), "a title's text is drawn on the bar");
    cli_check(
        painted(&raster, (GuiRect){menubar.menu[0].rect.x + 2, menubar.menu[0].rect.y + 2, 4, 4}, palette->ground),
        "the open menu is drawn under the bar");
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);

    // a narrow window folds the titles into a last one that lists them
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 420u, .height = 300u, .scale = 100);
    EMBED_CALL(gui_menubar_json.layout, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame);
    cli_check((menubar.titles >= 1u) && menubar.title[menubar.titles - 1u].folded && (menubar.levels == 0u),
              "a narrow window folds the titles into a last one");
    cli_check(EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .x = middle_x(menubar.title[menubar.titles - 1u].rect), .y = middle_y(menubar.title[menubar.titles - 1u].rect), .choice = &choice) &&
                  (EMBED_CALL(gui_menubar_level.rows, GuiMenubarLevelCfg, .menubar_read = &menubar, .level = 0u) + menubar.titles == 4u) && EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = &menubar, .level = 0u, .index = 0u, .row = &row) &&
                  (row.kind == GUI_ROW_MORE),
              "the last title's menu lists the folded menus, each with its items under it");
    cli_check(EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &menubar, .glyphs = &glyphs, .frame = &frame, .given = "Right", .choice = &choice) && (menubar.levels == 2u) &&
                  EMBED_CALL(gui_menubar_level.row, GuiMenubarLevelCfg, .menubar_read = &menubar, .level = 1u, .index = 0u, .row = &row) && (row.kind == GUI_ROW_ITEM),
              "a folded menu opens beside its row");
    EMBED_CALL(gui_menubar_json.close, GuiMenubarJsonCfg, .menubar = &menubar);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
