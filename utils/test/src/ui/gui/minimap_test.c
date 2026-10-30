// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// minimap_test.c: the minimap part's check: the lines an answer holds are kept with their bars, the map placed beside
// the view with its first line following the view's top through a long text and standing still while the view does,
// the lines it stands for, each bar drawn in its token's color with the slider over the view's rows and the strip at
// the right, a press going to the row's line and a drag moving the view by the slider's run
#include "../../../../../ui/gui/src/parts/minimap.h"
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

static const char s_names[] = "[\"\",\"keyword\",\"string\"]";
static const char s_map[] = "{\"from\":3,\"bars\":[[[0,3,1],[4,4,0]],[],[[2,1,2]]]}";

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    GuiMinimap map;
    cli_check(gui_minimap_made(&map) && (map.count == 0u) && map.stale, "a map made holds nothing and is stale");
    GuiEditor editor;
    gui_editor_made(&editor);
    CliJson json;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    unsigned int root = cli_json_taken(&json, s_names, sizeof(s_names) - 1u, 0u, 0ull, &err_msg, &g_checks_cells);
    cli_check(gui_editor_named(&editor, &json, root), "the grammar's names are kept");
    cli_json_given(&json);
    root = cli_json_taken(&json, s_map, sizeof(s_map) - 1u, 0u, 0ull, &err_msg, &g_checks_cells);
    cli_check(gui_minimap_put(&map, &json, root), "the answer's lines are kept");
    cli_json_given(&json);
    cli_check((map.from == 3ull) && (map.count == 3u) && (map.bars == 3u),
              "three lines from the fourth, three bars among them");
    cli_check((map.line_at[0] == 0u) && (map.line_at[1] == 2u) && (map.line_at[2] == 2u) && (map.line_at[3] == 3u),
              "the first line has two bars, the second none, the third one");
    cli_check((map.bar[1].start == 4u) && (map.bar[1].width == 4u) && (map.bar[1].name == 0u),
              "a bar is its start, width and name");
    cli_check(!map.stale, "the map is stale no more");

    // a short text: every line fits, the map begins at the first
    const GuiRect place = {100, 10, 84, 200};
    gui_editor_shaped(&editor, 10ull);
    editor.top = 2ull;
    gui_minimap_placed(&map, place, 1.0f, &editor, 20.0f);
    cli_check((map.place.fit == 100u) && (map.place.start == 0ull), "a short text's map begins at the first line");
    cli_check((map.place.slider_top == 4) && (map.place.slider_tall == 40),
              "the slider stands over the view's rows, two pixels a row");
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    cli_check(gui_minimap_span(&map, &first, &last) && (first == 0ull) && (last == 9ull),
              "the map stands for the whole short text");
    cli_check(!gui_minimap_current(&map, 0ull, 9ull), "the lines held are not the whole text");
    cli_check(gui_minimap_current(&map, 3ull, 5ull), "the lines held are the fourth to the sixth");

    // a long text: the map's first line follows the view's top through it
    gui_editor_shaped(&editor, 1000ull);
    editor.top = 490ull;
    gui_minimap_placed(&map, place, 1.0f, &editor, 20.0f);
    cli_check(map.place.start == 450ull, "halfway through the text the map begins halfway through its lines");
    cli_check(map.place.slider_top == 80, "the slider stands where the view's top stands among the map's rows");
    cli_check(gui_minimap_span(&map, &first, &last) && (first == 450ull) && (last == 549ull),
              "the map stands for a hundred lines from its first");
    const long long offset = map.held_offset;
    gui_editor_shaped(&editor, 1200ull);
    gui_minimap_placed(&map, place, 1.0f, &editor, 20.0f);
    cli_check((map.place.start == 450ull) && (map.held_offset == offset),
              "lines added while the view stands still leave the map where it stood");
    editor.top = 1199ull;
    gui_minimap_placed(&map, place, 1.0f, &editor, 20.0f);
    cli_check(map.place.start == 1100ull,
              "the view moved places the map by its top again, no further than its last lines");

    // drawn: the bars in their colors, the slider, the strip
    gui_editor_shaped(&editor, 10ull);
    editor.top = 0ull;
    editor.has_cursor = 1;
    editor.cursor.line = 4ull;
    editor.cursor.col = 0ull;
    editor.anchor = editor.cursor;
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 200u, 220u), "a raster for the map is made");
    gui_raster_filled(&raster, (GuiRect){0, 0, 200, 220}, palette->ground);
    gui_minimap_drawn(&map, &raster, palette, &editor, place, 1.0f, 20.0f);
    // the ground below the slider is the editor's; the slider lies over the first twenty rows
    const GuiColor ground = raster.pixels[(100u * 200u) + 150u];
    const GuiColor under_slider = raster.pixels[(30u * 200u) + 150u];
    // the editor's ground at its opacity over the page's ground, which in the dark scheme is the same color
    const unsigned int share = GUI_ALPHA_OF(palette->editor_ground);
    const GuiColor laid = GUI_RGB(
        ((GUI_RED_OF(palette->editor_ground) * share) + (GUI_RED_OF(palette->ground) * (255u - share)) + 127u) / 255u,
        ((GUI_GREEN_OF(palette->editor_ground) * share) + (GUI_GREEN_OF(palette->ground) * (255u - share)) + 127u) /
            255u,
        ((GUI_BLUE_OF(palette->editor_ground) * share) + (GUI_BLUE_OF(palette->ground) * (255u - share)) + 127u) /
            255u);
    cli_check(ground == laid, "the map stands on the editor's ground at its opacity");
    cli_check((under_slider != ground) && (raster.pixels[(60u * 200u) + 150u] == ground),
              "the slider lies over the view's rows and no further");
    cli_check(raster.pixels[(100u * 200u) + 100u] != ground, "a line runs along the map's left edge");
    cli_check(painted(&raster, (GuiRect){104, 10 + (3 * 2), 3, 1}, under_slider),
              "the fourth line's first bar is drawn at its cell");
    cli_check(!painted(&raster, (GuiRect){104, 10 + (4 * 2), 60, 1}, under_slider),
              "the fifth line, empty, is left as ground");
    cli_check(painted(&raster, (GuiRect){106, 10 + (5 * 2), 1, 1}, under_slider),
              "the sixth line's bar is drawn at its cell");
    cli_check(!painted(&raster, (GuiRect){104, 10 + (7 * 2), 60, 1}, under_slider),
              "a line the map does not hold is left as ground");
    const GuiColor strip_ground = raster.pixels[(150u * 200u) + 182u];
    cli_check((strip_ground == ground) && (raster.pixels[(150u * 200u) + 176u] != ground),
              "the strip's line runs down the map's right");
    cli_check(raster.pixels[((10u + 80u) * 200u) + 182u] != strip_ground, "the caret's line is marked on the strip");
    const GuiColor fourth = raster.pixels[(16u * 200u) + 104u];
    cli_check(GUI_BLUE_OF(fourth) > GUI_RED_OF(fourth) + 0x20u, "the fourth line's first bar is the keyword's color");

    // pressed and dragged
    gui_editor_shaped(&editor, 1000ull);
    editor.top = 0ull;
    unsigned long long top = 0ull;
    cli_check(!gui_minimap_pressed(&map, place, 1.0f, &editor, 20.0f, 50, 50, &top),
              "a press outside the map is not the map's");
    cli_check(gui_minimap_pressed(&map, place, 1.0f, &editor, 20.0f, 120, 10 + 100, &top) && (top == 40ull),
              "a press below the slider centers the row's line in the view");
    cli_check(map.dragging, "the press begins a drag");
    cli_check(gui_minimap_dragged(&map, 10 + 100 + 80, &top) && (top > 40ull),
              "the pointer moved down moves the view down");
    gui_minimap_released(&map);
    cli_check(!map.dragging && !gui_minimap_dragged(&map, 0, &top), "the drag ends as the button is let go");
    cli_check(gui_minimap_pressed(&map, place, 1.0f, &editor, 20.0f, 100 + 84 - 2, 10 + 100, &top) && (top == 490ull) &&
                  !map.dragging,
              "a press on the strip goes to that part of the text, no drag begun");
    gui_minimap_cleared(&map);
    cli_check((map.count == 0u) && map.stale && !map.held, "the map cleared holds nothing, stale, its hold let go");
    gui_raster_given(&raster);
    gui_minimap_given(&map);
    gui_editor_given(&editor);
    return cli_checked();
}
