// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// explorer_test.c: the explorer's check: a folder's listing answered with the ticket it was asked under puts its
// entries as rows under the folder, a press on a folder opens it and asks for its listing once, a press on the folder
// open closes it with its entries kept, a press on a file picks it to open, the rows scroll with the wheel, the
// pointer marks the row it stands on, and the side drawn shows its ground, its rows and the row pointed
#include "../../../../../ui/gui/src/parts/explorer.h"
#include "../cli/cli_check.h"

static const char s_root[] = "[{\"name\":\"ui\",\"path\":\"ui\",\"dir\":true,\"ignored\":false},"
                             "{\"name\":\"README.md\",\"path\":\"README.md\",\"dir\":false,\"ignored\":false}]";
static const char s_ui[] = "[{\"name\":\"gui\",\"path\":\"ui/gui\",\"dir\":true,\"ignored\":false},"
                           "{\"name\":\"a.c\",\"path\":\"ui/a.c\",\"dir\":false,\"ignored\":true}]";

// a listing's text taken as the answer to the call `ticket`
static int listed(GuiExplorer *explorer, unsigned long long ticket, const char *text, unsigned long long size)
{
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    const unsigned int root = cli_json_taken(&json, text, size, 0u, 0ull, &err_msg);
    const int taken = (root != CLI_JSON_NONE) && gui_explorer_listed(explorer, ticket, &json, root);
    if (root != CLI_JSON_NONE)
    {
        cli_json_given(&json);
    }
    return taken;
}

// whether the row `row` stands for the path `path`
static int row_is(const GuiExplorer *explorer, unsigned int row, const char *path)
{
    unsigned int node = 0u;
    if (!gui_explorer_row(explorer, row, &node))
    {
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const held = gui_explorer_path(explorer, node, &length);
    return (length == cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN})) &&
           (memor.cmp(&(MemoriaCfg){.src = held, .other = path, .bytes = length}) == 0);
}

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

static GuiColor pixel(const GuiRaster *raster, int x, int y)
{
    return raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
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
    GuiFrame frame;
    gui_frame_laid(&frame, 800u, 600u, 1.0f);
    gui_frame_sided(&frame, 1);
    GuiExplorer explorer;
    cli_check(gui_explorer_made(&explorer) && (gui_explorer_rows(&explorer) == 0u) && (explorer.pointed == GUI_EXPLORER_NONE),
              "an explorer is made holding no rows");
    cli_check(!listed(&explorer, 7ull, s_root, sizeof(s_root) - 1u), "a listing under a ticket never asked is not taken");
    gui_explorer_listing(&explorer, "", 7ull);
    cli_check(listed(&explorer, 7ull, s_root, sizeof(s_root) - 1u) && (gui_explorer_rows(&explorer) == 2u),
              "the top folder's listing puts its entries as rows");
    cli_check(row_is(&explorer, 0u, "ui") && row_is(&explorer, 1u, "README.md"), "the rows stand in the listing's order");
    unsigned int node = 0u;
    cli_check(gui_explorer_row(&explorer, 0u, &node) && explorer.node[node].dir && (explorer.node[node].depth == 0u) &&
                  !explorer.node[node].open,
              "a folder's row is a folder at the top's depth, closed");

    const GuiRect first = gui_explorer_row_rect(&frame, &explorer, 0u);
    const GuiRect second = gui_explorer_row_rect(&frame, &explorer, 1u);
    cli_check((first.x == frame.side.x) && (first.width == frame.side.width - 1) && (first.height == 22) &&
                  (second.y == first.y + 22) && (first.y == frame.side.y + 53),
              "the rows stand under the explorer's head and the pane's, twenty-two tall");
    GuiPick pick;
    cli_check(gui_explorer_pressed(&explorer, &frame, first.x + 10, first.y + 10, &pick) && (pick.kind == GUI_PICK_LIST) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = pick.path, .other = "ui", .cap = sizeof(pick.path)}) && explorer.node[0].open,
              "a press on a folder opens it and asks for its listing");
    gui_explorer_listing(&explorer, "ui", 8ull);
    cli_check(listed(&explorer, 8ull, s_ui, sizeof(s_ui) - 1u) && (gui_explorer_rows(&explorer) == 4u),
              "the folder's listing puts its entries under it");
    cli_check(row_is(&explorer, 1u, "ui/gui") && row_is(&explorer, 2u, "ui/a.c") && row_is(&explorer, 3u, "README.md"),
              "the entries stand under their folder, before the rows after it");
    cli_check(gui_explorer_row(&explorer, 2u, &node) && !explorer.node[node].dir && (explorer.node[node].depth == 1u) &&
                  explorer.node[node].ignored,
              "a file under the folder is a level deeper, ignored where the listing says so");
    const GuiRect third = gui_explorer_row_rect(&frame, &explorer, 2u);
    cli_check(gui_explorer_pressed(&explorer, &frame, third.x + 40, third.y + 10, &pick) && (pick.kind == GUI_PICK_OPEN) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = pick.path, .other = "ui/a.c", .cap = sizeof(pick.path)}),
              "a press on a file picks it to open");
    cli_check(gui_explorer_pressed(&explorer, &frame, first.x + 10, first.y + 10, &pick) && (pick.kind == GUI_PICK_NONE) &&
                  (gui_explorer_rows(&explorer) == 2u) && row_is(&explorer, 1u, "README.md"),
              "a press on the open folder closes it, its entries kept out of the rows");
    cli_check(gui_explorer_pressed(&explorer, &frame, first.x + 10, first.y + 10, &pick) && (pick.kind == GUI_PICK_NONE) &&
                  (gui_explorer_rows(&explorer) == 4u),
              "a press on it again opens it from the entries kept, asking for nothing");
    cli_check(!gui_explorer_pressed(&explorer, &frame, frame.editor.x + 10, first.y + 10, &pick), "a press in the editor is not the side's");
    cli_check(gui_explorer_pressed(&explorer, &frame, first.x + 10, frame.side.y + frame.side.height - 5, &pick) &&
                  (pick.kind == GUI_PICK_NONE),
              "a press on the side off its rows is the side's and picks nothing");

    cli_check(gui_explorer_moved(&explorer, &frame, first.x + 10, first.y + 10) && (explorer.pointed == 0),
              "the pointer moving onto a row marks it");
    cli_check(!gui_explorer_moved(&explorer, &frame, first.x + 12, first.y + 12), "moving within the row changes nothing");
    cli_check(gui_explorer_moved(&explorer, &frame, frame.editor.x + 10, first.y + 10) && (explorer.pointed == GUI_EXPLORER_NONE),
              "the pointer leaving the side unmarks it");

    // a side that holds two rows scrolls the four by the wheel
    gui_frame_laid(&frame, 800u, 41u + 27u + 3u + 53u + 44u, 1.0f);
    gui_frame_sided(&frame, 1);
    cli_check(gui_explorer_wheeled(&explorer, &frame, 1) && (explorer.scroll == 1u) &&
                  (gui_explorer_row_rect(&frame, &explorer, 1u).y == frame.side.y + 53),
              "the wheel scrolls the rows a row down");
    cli_check(gui_explorer_wheeled(&explorer, &frame, 5) && (explorer.scroll == 2u), "the scroll stops at the last rows");
    cli_check(gui_explorer_wheeled(&explorer, &frame, -5) && (explorer.scroll == 0u) && !gui_explorer_wheeled(&explorer, &frame, -1),
              "the wheel scrolls back to the top and no further");

    gui_frame_laid(&frame, 800u, 600u, 1.0f);
    gui_frame_sided(&frame, 1);
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 800u, 600u), "a raster for the side is made");
    gui_raster_filled(&raster, (GuiRect){0, 0, 800, 600}, palette->ground);
    explorer.pointed = 1;
    gui_explorer_drawn(&explorer, &raster, &glyphs, &icons, palette, &frame, "Explorer", 1, "anchor_sift", "ui/a.c");
    const GuiRect row = gui_explorer_row_rect(&frame, &explorer, 0u);
    cli_check(pixel(&raster, frame.side.x + 3, frame.side.y + 3) != palette->ground, "the side is tinted over the ground");
    cli_check(pixel(&raster, frame.side.x + frame.side.width - 1, frame.side.y + 100) == pixel(&raster, frame.side.x + frame.side.width - 1, frame.side.y + 300),
              "the side's edge runs down its right");
    cli_check(painted(&raster, (GuiRect){frame.side.x + 10, frame.side.y + 6, 80, 18}, pixel(&raster, frame.side.x + 3, frame.side.y + 3)),
              "the group's title is drawn at the side's head");
    cli_check(painted(&raster, (GuiRect){row.x + 30, row.y + 4, 60, 14}, pixel(&raster, row.x + 2, row.y + 2)), "a row's name is drawn");
    const GuiRect pointed = gui_explorer_row_rect(&frame, &explorer, 1u);
    cli_check(pixel(&raster, pointed.x + 2, pointed.y + 2) != pixel(&raster, row.x + 2, row.y + 2), "the row pointed stands on its own ground");
    const GuiRect current = gui_explorer_row_rect(&frame, &explorer, 2u);
    cli_check((pixel(&raster, current.x + 2, current.y + 2) != pixel(&raster, row.x + 2, row.y + 2)) &&
                  (pixel(&raster, current.x + 2, current.y + 2) != pixel(&raster, pointed.x + 2, pointed.y + 2)),
              "the row of the file shown stands on the selection's ground");
    gui_raster_given(&raster);
    gui_explorer_given(&explorer);
    gui_icons_given(&icons);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
