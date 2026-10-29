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
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    const unsigned int root = cli_json_taken(&json, s_catalog, sizeof(s_catalog) - 1u, 0u, 0ull, &err_msg);
    cli_check(root != CLI_JSON_NONE, "the catalog is read");
    GuiFrame frame;
    gui_frame_laid(&frame, 1440u, 900u, 1.0f);
    GuiMenubar menubar;
    gui_menubar_made(&menubar);
    gui_menubar_cataloged(&menubar, &json, cli_json_member(&json, root, "menus"));
    gui_menubar_laid(&menubar, &glyphs, &frame);
    cli_check(menubar.titles == 3u, "the menus with items that are neither the strip's nor popups are the titles");
    cli_check((menubar.title[0].menu == 0u) && (menubar.title[1].menu == 1u) && (menubar.title[2].menu == 5u),
              "each title opens its own menu");
    cli_check((menubar.title[0].letter == 0u) && (menubar.title[1].letter == 0u) && (menubar.title[2].letter == 0u),
              "each title has a letter for Alt, the first no title before it took");
    cli_check((menubar.title[0].rect.x < menubar.title[1].rect.x) && (menubar.title[1].rect.x < menubar.title[2].rect.x) &&
                  (menubar.title[0].rect.y > frame.bar.y) && (menubar.title[0].rect.height < frame.bar.height),
              "the titles stand in a row inside the bar");
    cli_check(gui_menubar_holds(&menubar, middle_x(menubar.title[0].rect), middle_y(menubar.title[0].rect)) &&
                  !gui_menubar_holds(&menubar, frame.bar.x + 2, middle_y(menubar.title[0].rect)),
              "a title holds its pixels and the bar beside it does not");

    GuiChoice choice;
    cli_check(gui_menubar_pressed(&menubar, &glyphs, &frame, middle_x(menubar.title[0].rect), middle_y(menubar.title[0].rect), &choice) &&
                  (menubar.open == 0) && (menubar.levels == 1u) && (choice.command[0] == '\0'),
              "a title pressed opens its menu");
    cli_check(gui_menubar_rows(&menubar, 0u) == 4u, "the menu's rows leave the hidden and the filled items out");
    GuiRow row;
    cli_check(gui_menubar_row(&menubar, 0u, 0u, &row) && (row.kind == GUI_ROW_ITEM) && (row.label_length == 7ull) &&
                  (row.keys_length == 6ull) && !row.disabled,
              "the first row is the open item with its keys");
    cli_check(gui_menubar_row(&menubar, 0u, 1u, &row) && (row.kind == GUI_ROW_LINE), "the second row is a line");
    cli_check(gui_menubar_row(&menubar, 0u, 2u, &row) && (row.kind == GUI_ROW_MORE) && (row.items != CLI_JSON_NONE),
              "the third row has items of its own");
    cli_check(gui_menubar_row(&menubar, 0u, 3u, &row) && (row.kind == GUI_ROW_ITEM) && row.disabled,
              "the item that needs an open file cannot be chosen while none is");
    cli_check(menubar.menu[0].pointed == 0, "the first row is marked as the menu opens from a press");
    cli_check((menubar.menu[0].rect.y > frame.bar.y + frame.bar.height) && (menubar.menu[0].rect.x == menubar.title[0].rect.x),
              "the menu opens under its title");
    cli_check(gui_menubar_moved(&menubar, &glyphs, &frame, middle_x(menubar.title[1].rect), middle_y(menubar.title[1].rect), 0ull) &&
                  (menubar.open == 1) && (menubar.menu[0].pointed == -1),
              "the pointer on another title opens its menu, no row marked");
    cli_check(gui_menubar_pressed(&menubar, &glyphs, &frame, middle_x(menubar.title[1].rect), middle_y(menubar.title[1].rect), &choice) &&
                  (menubar.open == -1) && (menubar.levels == 0u),
              "the open title pressed closes its menu");

    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Alt+F", &choice) && (menubar.open == 0) && (menubar.menu[0].pointed == 0),
              "Alt with a title's letter opens its menu, the first row marked");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Down", &choice) && (menubar.menu[0].pointed == 2),
              "Down steps the mark over the line");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Down", &choice) && (menubar.menu[0].pointed == 0),
              "Down steps around the end over the item that cannot be chosen");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "End", &choice) && (menubar.menu[0].pointed == 2),
              "End marks the last row that can be chosen");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Right", &choice) && (menubar.levels == 2u) &&
                  (menubar.menu[0].opened == 2) && (menubar.menu[1].pointed == 0) &&
                  (menubar.menu[1].rect.x > menubar.menu[0].rect.x),
              "Right opens the items under the row beside it");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Left", &choice) && (menubar.levels == 1u) && (menubar.menu[0].opened == -1),
              "Left closes them again");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Enter", &choice) && (menubar.levels == 2u),
              "Enter on the row opens them too");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Enter", &choice) && (menubar.levels == 0u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "open-recent", .cap = 256ull}),
              "Enter on an item chooses it and closes the menus");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Alt+E", &choice) && (menubar.open == 1), "Alt+E opens the second menu");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Right", &choice) && (menubar.open == 2), "Right in a menu opens the next title's");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Right", &choice) && (menubar.open == 0), "Right at the last title comes around to the first");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "S", &choice) && (menubar.menu[0].pointed == 0),
              "a letter marks the next row it begins, none where only an item that cannot be chosen does");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "R", &choice) && (menubar.menu[0].pointed == 2), "R marks Recent");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Escape", &choice) && (menubar.levels == 0u) && (menubar.open == -1),
              "Escape closes the menu");
    cli_check(!gui_menubar_keyed(&menubar, &glyphs, &frame, "Down", &choice), "a key with no menu open is not the bar's");

    menubar.has_editor = 1;
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Alt+F", &choice) && gui_menubar_keyed(&menubar, &glyphs, &frame, "S", &choice) &&
                  (menubar.menu[0].pointed == 3),
              "with a file open the item that needs one can be chosen");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Enter", &choice) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "save", .cap = 256ull}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.keys, .other = "Ctrl+S", .cap = 256ull}),
              "the choice carries the item's command and keys");

    // a key pressed with no menu open is looked up among every item's keys
    cli_check(gui_menubar_keys_command(&menubar, "Mod+Z", &choice) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "undo", .cap = 256ull}),
              "a key an item names, Ctrl read as Mod, finds the item's command");
    cli_check(gui_menubar_keys_command(&menubar, "Mod+S", &choice) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "save", .cap = 256ull}),
              "an item that needs a file is found while one is open");
    menubar.has_editor = 0;
    cli_check(!gui_menubar_keys_command(&menubar, "Mod+S", &choice) && (choice.command[0] == '\0'),
              "and not while none is");
    menubar.has_editor = 1;
    cli_check(!gui_menubar_keys_command(&menubar, "Mod+Q", &choice) && !gui_menubar_keys_command(&menubar, "Z", &choice),
              "a key no item names finds nothing");

    // a press on a row with the pointer, and on an item of the menu beside it
    cli_check(gui_menubar_pressed(&menubar, &glyphs, &frame, middle_x(menubar.title[0].rect), middle_y(menubar.title[0].rect), &choice) &&
                  (menubar.levels == 1u),
              "the first menu opens again");
    const GuiRect more = {menubar.menu[0].rect.x + 10, menubar.menu[0].rect.y + 5 + 27 + 11 + 13, 10, 1};
    cli_check(gui_menubar_moved(&menubar, &glyphs, &frame, more.x, more.y, 1000ull) && (menubar.levels == 1u) &&
                  (menubar.menu[0].pointed == 2) && (gui_menubar_due(&menubar, 1000ull) == GUI_MENU_REST),
              "the pointer on a row with items marks it and begins the rest before they open");
    cli_check(!gui_menubar_ticked(&menubar, &glyphs, &frame, 1100ull) && (menubar.levels == 1u), "a tick before the rest ends opens nothing");
    cli_check(gui_menubar_ticked(&menubar, &glyphs, &frame, 1000ull + GUI_MENU_REST) && (menubar.levels == 2u) &&
                  (menubar.menu[0].opened == 2) && (gui_menubar_due(&menubar, 1200ull) == 0ull),
              "the rest's end opens the items beside the row");
    cli_check(gui_menubar_moved(&menubar, &glyphs, &frame, more.x, menubar.menu[0].rect.y + 5 + 13, 2000ull) && (menubar.levels == 2u) &&
                  (menubar.menu[0].pointed == 0) && (gui_menubar_due(&menubar, 2000ull) == GUI_MENU_REST),
              "the pointer on a sibling without items keeps them open and begins the rest before they close");
    cli_check(gui_menubar_moved(&menubar, &glyphs, &frame, more.x, more.y, 2050ull) && (gui_menubar_due(&menubar, 2050ull) == 0ull),
              "the pointer back on the opened row lets the rest go");
    cli_check(gui_menubar_moved(&menubar, &glyphs, &frame, more.x, menubar.menu[0].rect.y + 5 + 13, 2100ull) &&
                  gui_menubar_ticked(&menubar, &glyphs, &frame, 2100ull + GUI_MENU_REST) && (menubar.levels == 1u) && (menubar.menu[0].opened == -1),
              "the rest's end on the sibling closes the items");
    cli_check(gui_menubar_pressed(&menubar, &glyphs, &frame, more.x, more.y, &choice) && (menubar.levels == 2u),
              "a press on the row opens its items at once");
    cli_check(gui_menubar_pressed(&menubar, &glyphs, &frame, menubar.menu[1].rect.x + 10, menubar.menu[1].rect.y + 5 + 13, &choice) &&
                  (menubar.levels == 0u) && cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "open-recent", .cap = 256ull}),
              "a press on an item beside chooses it");
    cli_check(gui_menubar_pressed(&menubar, &glyphs, &frame, middle_x(menubar.title[2].rect), middle_y(menubar.title[2].rect), &choice) &&
                  !gui_menubar_pressed(&menubar, &glyphs, &frame, 400, 400, &choice) && (menubar.levels == 0u),
              "a press outside the menus closes them and is not the bar's");

    // the titles and an open menu drawn
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 1440u, 300u), "a raster for the bar is made");
    gui_raster_filled(&raster, (GuiRect){0, 0, 1440, 300}, palette->ground);
    (void)gui_menubar_pressed(&menubar, &glyphs, &frame, middle_x(menubar.title[0].rect), middle_y(menubar.title[0].rect), &choice);
    gui_menubar_titles_drawn(&menubar, &raster, &glyphs, palette, &frame);
    gui_menubar_menus_drawn(&menubar, &raster, &glyphs, palette, &frame);
    cli_check(painted(&raster, menubar.title[1].rect, palette->ground), "a title's text is drawn on the bar");
    cli_check(painted(&raster, (GuiRect){menubar.menu[0].rect.x + 2, menubar.menu[0].rect.y + 2, 4, 4}, palette->ground),
              "the open menu is drawn under the bar");
    gui_raster_given(&raster);

    // a narrow window folds the titles into a last one that lists them
    gui_frame_laid(&frame, 420u, 300u, 1.0f);
    gui_menubar_laid(&menubar, &glyphs, &frame);
    cli_check((menubar.titles >= 1u) && menubar.title[menubar.titles - 1u].folded && (menubar.levels == 0u),
              "a narrow window folds the titles into a last one");
    cli_check(gui_menubar_pressed(&menubar, &glyphs, &frame, middle_x(menubar.title[menubar.titles - 1u].rect),
                                  middle_y(menubar.title[menubar.titles - 1u].rect), &choice) &&
                  (gui_menubar_rows(&menubar, 0u) + menubar.titles == 4u) && gui_menubar_row(&menubar, 0u, 0u, &row) &&
                  (row.kind == GUI_ROW_MORE),
              "the last title's menu lists the folded menus, each with its items under it");
    cli_check(gui_menubar_keyed(&menubar, &glyphs, &frame, "Right", &choice) && (menubar.levels == 2u) && gui_menubar_row(&menubar, 1u, 0u, &row) &&
                  (row.kind == GUI_ROW_ITEM),
              "a folded menu opens beside its row");
    gui_menubar_closed(&menubar);
    cli_json_given(&json);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
